/**
 * @file lldirpicker.cpp
 * @brief OS-specific file picker
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "lldirpicker.h"
#include "llworld.h"
#include "llviewerwindow.h"
#include "llkeyboard.h"
#include "lldir.h"
#include "llframetimer.h"
#include "lltrans.h"
#include "llwindow.h"   // beforeDialog()
#include "llviewercontrol.h"
#include "llwin32headers.h"

#if LL_LINUX || LL_DARWIN
# include "llfilepicker.h"
#endif

#ifdef LL_FLTK
  #include "FL/Fl.H"
  #include "FL/Fl_Native_File_Chooser.H"
#endif

#if LL_LINUX
# include <unistd.h>
# include <sys/wait.h>
#endif

#if LL_WINDOWS
#include <shlobj.h>
#endif

//
// Implementation
//

// utility function to check if access to local file system via file browser
// is enabled and if not, tidy up and indicate we're not allowed to do this.
bool LLDirPicker::check_local_file_access_enabled()
{
    // if local file browsing is turned off, return without opening dialog
    bool local_file_system_browsing_enabled = gSavedSettings.getBOOL("LocalFileSystemBrowsingEnabled");
    if ( ! local_file_system_browsing_enabled )
    {
        mDir.clear();   // Windows
        mFileName = NULL; // Mac/Linux
        return false;
    }

    return true;
}

#if LL_WINDOWS

LLDirPicker::LLDirPicker() :
    mFileName(NULL),
    mLocked(false),
    pDialog(NULL)
{
}

LLDirPicker::~LLDirPicker()
{
    mEventListener.disconnect();
}

void LLDirPicker::reset()
{
    if (pDialog)
    {
        IFileDialog* p_file_dialog = (IFileDialog*)pDialog;
        p_file_dialog->Close(S_FALSE);
        pDialog = NULL;
    }
}

bool LLDirPicker::getDir(std::string* filename, bool blocking)
{
    if (mLocked)
    {
        return false;
    }

    // if local file browsing is turned off, return without opening dialog
    if (!check_local_file_access_enabled())
    {
        return false;
    }

    bool success = false;

    if (blocking)
    {
        // Modal, so pause agent
        send_agent_pause();
    }
    else if (!mEventListener.connected())
    {
        mEventListener = LLEventPumps::instance().obtain("LLApp").listen(
            "DirPicker",
            [this](const LLSD& stat)
            {
                std::string status(stat["status"]);
                if (status != "running")
                {
                    reset();
                }
                return false;
            });
    }

    ::OleInitialize(NULL);

    IFileDialog* p_file_dialog;
    if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&p_file_dialog))))
    {
        DWORD dwOptions;
        if (SUCCEEDED(p_file_dialog->GetOptions(&dwOptions)))
        {
            p_file_dialog->SetOptions(dwOptions | FOS_PICKFOLDERS);
        }
        HWND owner = (HWND)gViewerWindow->getPlatformWindow();
        pDialog = p_file_dialog;
        if (SUCCEEDED(p_file_dialog->Show(owner)))
        {
            IShellItem* psi;
            if (SUCCEEDED(p_file_dialog->GetResult(&psi)))
            {
                wchar_t* pwstr = NULL;
                if (SUCCEEDED(psi->GetDisplayName(SIGDN_FILESYSPATH, &pwstr)))
                {
                    mDir = ll_convert_wide_to_string(pwstr);
                    CoTaskMemFree(pwstr);
                    success = true;
                }
                psi->Release();
            }
        }
        pDialog = NULL;
        p_file_dialog->Release();
    }

    ::OleUninitialize();

    if (blocking)
    {
        send_agent_resume();
    }

    // Account for the fact that the app has been stalled.
    LLFrameTimer::updateFrameTime();
    return success;
}

std::string LLDirPicker::getDirName()
{
    return mDir;
}

/////////////////////////////////////////////DARWIN
#elif LL_DARWIN

LLDirPicker::LLDirPicker() :
mFileName(NULL),
mLocked(false)
{
    mFilePicker = new LLFilePicker();
    reset();
}

LLDirPicker::~LLDirPicker()
{
    delete mFilePicker;
}

void LLDirPicker::reset()
{
    if (mFilePicker)
        mFilePicker->reset();
}


//static
bool LLDirPicker::getDir(std::string* filename, bool blocking)
{
    LLFilePicker::ELoadFilter filter=LLFilePicker::FFLOAD_DIRECTORY;

    return mFilePicker->getOpenFile(filter, true);
}

std::string LLDirPicker::getDirName()
{
    return mFilePicker->getFirstFile();
}

#elif LL_LINUX

LLDirPicker::LLDirPicker() :
    mFileName(NULL),
    mLocked(false)
{
#ifndef LL_FLTK
    mFilePicker = new LLFilePicker();
#endif

    reset();
}

LLDirPicker::~LLDirPicker()
{
#ifndef LL_FLTK
    delete mFilePicker;
#endif
}


void LLDirPicker::reset()
{
#ifndef LL_FLTK
    if (mFilePicker)
        mFilePicker->reset();
#else
    mDir = "";
#endif
}

bool LLDirPicker::getDir(std::string* filename, bool blocking)
{
    reset();
    // if local file browsing is turned off, return without opening dialog
    if (!check_local_file_access_enabled())
    {
        return false;
    }

#ifndef LL_FLTK
#if !LL_MESA_HEADLESS

    if (mFilePicker)
    {
        GtkWindow* picker = mFilePicker->buildFilePicker(false, true,
                                 "dirpicker");

        if (picker)
        {
           gtk_window_set_title(GTK_WINDOW(picker), LLTrans::getString("choose_the_directory").c_str());
           gtk_widget_show_all(GTK_WIDGET(picker));
           gtk_main();
           return (!mFilePicker->getFirstFile().empty());
        }
    }
#endif // !LL_MESA_HEADLESS

    return false;
#else
    gViewerWindow->getWindow()->beforeDialog();

    // Use kdialog (KDE) or zenity (GNOME/other) as a subprocess instead of
    // FLTK's GTK2 file chooser, which crashes on modern Wayland/Fedora desktops
    // due to GTK2 incompatibility with at-spi2 and other system libraries.
    std::string start_dir = (filename && !filename->empty())
        ? *filename : gDirUtilp->getLindenUserDir();

    auto run_dir_picker = [](const char* prog, std::vector<const char*> argv) -> std::string
    {
        argv.push_back(nullptr);
        int pipefd[2];
        if (::pipe(pipefd) != 0) return "";
        pid_t pid = ::fork();
        if (pid == 0)
        {
            ::close(pipefd[0]);
            ::dup2(pipefd[1], STDOUT_FILENO);
            ::close(pipefd[1]);
            ::execvp(prog, const_cast<char* const*>(argv.data()));
            ::_exit(1);
        }
        else if (pid < 0)
        {
            ::close(pipefd[0]);
            ::close(pipefd[1]);
            return "";
        }
        ::close(pipefd[1]);
        char buf[4096] = {};
        ssize_t n = ::read(pipefd[0], buf, sizeof(buf) - 1);
        ::close(pipefd[0]);
        int status = 0;
        ::waitpid(pid, &status, 0);
        if (n > 0 && WIFEXITED(status) && WEXITSTATUS(status) == 0)
        {
            std::string result(buf, (size_t)n);
            while (!result.empty() && (result.back() == '\n' || result.back() == '\r'))
                result.pop_back();
            return result;
        }
        return "";
    };

    std::string chosen;
    if (::access("/usr/bin/kdialog", X_OK) == 0)
    {
        chosen = run_dir_picker("kdialog",
            {"kdialog", "--getexistingdirectory", start_dir.c_str()});
    }
    else if (::access("/usr/bin/zenity", X_OK) == 0)
    {
        std::string dir_arg = "--filename=" + start_dir + "/";
        chosen = run_dir_picker("zenity",
            {"zenity", "--file-selection", "--directory", dir_arg.c_str()});
    }
    else
    {
        LL_WARNS() << "No kdialog or zenity found; cannot open directory picker." << LL_ENDL;
    }

    if (!chosen.empty())
        mDir = chosen;

    gViewerWindow->getWindow()->afterDialog();
    return !mDir.empty();
#endif
}

std::string LLDirPicker::getDirName()
{
#ifndef LL_FLTK
    if (mFilePicker)
    {
        return mFilePicker->getFirstFile();
    }
    return "";
#else
    return mDir;
#endif
}

#else // not implemented

LLDirPicker::LLDirPicker()
{
    reset();
}

LLDirPicker::~LLDirPicker()
{
}


void LLDirPicker::reset()
{
}

bool LLDirPicker::getDir(std::string* filename, bool blocking)
{
    return false;
}

std::string LLDirPicker::getDirName()
{
    return "";
}

#endif


LLMutex* LLDirPickerThread::sMutex = NULL;
std::queue<LLDirPickerThread*> LLDirPickerThread::sDeadQ;

void LLDirPickerThread::getFile()
{
#if LL_WINDOWS
    start();
#else
    run();
#endif
}

//virtual
void LLDirPickerThread::run()
{
#if LL_WINDOWS
    bool blocking = false;
#else
    bool blocking = true; // modal
#endif

    LLDirPicker picker;

    if (picker.getDir(&mProposedName, blocking))
    {
        mResponses.push_back(picker.getDirName());
    }

    {
        LLMutexLock lock(sMutex);
        sDeadQ.push(this);
    }

}

//static
void LLDirPickerThread::initClass()
{
    sMutex = new LLMutex();
}

//static
void LLDirPickerThread::cleanupClass()
{
    clearDead();

    delete sMutex;
    sMutex = NULL;
}

//static
void LLDirPickerThread::clearDead()
{
    if (!sDeadQ.empty())
    {
        LLMutexLock lock(sMutex);
        while (!sDeadQ.empty())
        {
            LLDirPickerThread* thread = sDeadQ.front();
            thread->notify(thread->mResponses);
            delete thread;
            sDeadQ.pop();
        }
    }
}

LLDirPickerThread::LLDirPickerThread(const dir_picked_signal_t::slot_type& cb, const std::string &proposed_name)
    : LLThread("dir picker"),
    mFilePickedSignal(NULL)
{
    mFilePickedSignal = new dir_picked_signal_t();
    mFilePickedSignal->connect(cb);
}

LLDirPickerThread::~LLDirPickerThread()
{
    delete mFilePickedSignal;
}

void LLDirPickerThread::notify(const std::vector<std::string>& filenames)
{
    if (!filenames.empty())
    {
        if (mFilePickedSignal)
        {
            (*mFilePickedSignal)(filenames, mProposedName);
        }
    }
}
