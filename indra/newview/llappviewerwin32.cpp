/**
 * @file llappviewerwin32.cpp
 * @brief The LLAppViewerWin32 class definitions
 *
 * $LicenseInfo:firstyear=2007&license=viewerlgpl$
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

#ifdef INCLUDE_VLD
#define VLD_FORCE_ENABLE 1
#include "vld.h"
#endif
#include "llwin32headers.h"

#include "llwindowwin32.h" // *FIX: for setting gIconResource.

#include "llappviewerwin32.h"

#include "llgl.h"
#include "res/resource.h" // *FIX: for setting gIconResource.

#include <fcntl.h>      //_O_APPEND
#include <io.h>         //_open_osfhandle()
#include <WERAPI.H>     // for WerAddExcludedApplication()
#include <process.h>    // _spawnl()
#include <tchar.h>      // For TCHAR support

#include "llviewercontrol.h"
#include "lldxhardware.h"

#include "nvapi/nvapi.h"
#include "nvapi/NvApiDriverSettings.h"

#include <stdlib.h>

#include "llweb.h"

#include "llviewernetwork.h"
#include "llmd5.h"
#include "llfindlocale.h"

#include "llcommandlineparser.h"
#include "lltrans.h"

#ifndef LL_RELEASE_FOR_DOWNLOAD
#include "llwindebug.h"
#endif

#include "stringize.h"
#include "lldir.h"
#include "llerrorcontrol.h"

#include <fstream>
#include <exception>

// Bugsplat (http://bugsplat.com) crash reporting tool
#ifdef LL_BUGSPLAT
#include "bugsplatattributes.h"
#include "BugSplat.h"
#include "boost/json.hpp"                 // Boost.Json
#include "llagent.h"                // for agent location
#include "llstartup.h"
#include "llviewerregion.h"
#include "llvoavatarself.h"         // for agent name
#pragma optimize( "", off )

namespace FS
{
    std::wstring LogfileIn;
    std::wstring LogfileOut;
    std::wstring DumpFile;
}

namespace
{
    // MiniDmpSender's constructor is defined to accept __wchar_t* instead of
    // plain wchar_t*. That said, wunder() returns std::basic_string<__wchar_t>,
    // NOT plain __wchar_t*, despite the apparent convenience. Calling
    // wunder(something).c_str() as an argument expression is fine: that
    // std::basic_string instance will survive until the function returns.
    // Calling c_str() on a std::basic_string local to wunder() would be
    // Undefined Behavior: we'd be left with a pointer into a destroyed
    // std::basic_string instance. But we can do that with a macro...
    #define WCSTR(string) wunder(string).c_str()

    // It would be nice if, when wchar_t is the same as __wchar_t, this whole
    // function would optimize away. However, we use it only for the arguments
    // to the BugSplat API -- a handful of calls.
    inline std::basic_string<__wchar_t> wunder(const std::wstring& str)
    {
        return { str.begin(), str.end() };
    }

    // when what we have in hand is a std::string, convert from UTF-8 using
    // specific wstringize() overload
    inline std::basic_string<__wchar_t> wunder(const std::string& str)
    {
        return wunder(wstringize(str));
    }

    // Irritatingly, MiniDmpSender::setCallback() is defined to accept a
    // classic-C function pointer instead of an arbitrary C++ callable. If it
    // did accept a modern callable, we could pass a lambda that binds our
    // MiniDmpSender pointer. As things stand, though, we must define an
    // actual function and store the pointer statically.
    static MiniDmpSender *sBugSplatSender = nullptr;

    bool bugsplatSendLog(UINT nCode, LPVOID lpVal1, LPVOID lpVal2)
    {
        if (nCode == MDSCB_EXCEPTIONCODE)
        {
            // <FS:ND> Save dump and log into unique crash dymp folder
            __wchar_t aBuffer[1024] = {};
            sBugSplatSender->getMinidumpPath(aBuffer, _countof(aBuffer));
            std::wstring strPath{ (wchar_t*)aBuffer };
            ::CopyFileW(strPath.c_str(), FS::DumpFile.c_str(), FALSE);
            ::CopyFileW(FS::LogfileIn.c_str(), FS::LogfileOut.c_str(), FALSE);
            // </FS:ND>

            // second instance does not have own log files
            if (!LLAppViewer::instance()->isSecondInstance())
            {
                // <FS:ND> We don't send log files
                // sBugSplatSender->sendAdditionalFile(
                //     WCSTR(LLError::logFileName()));
                // </FS:ND>

                sBugSplatSender->sendAdditionalFile(
                    WCSTR(*LLAppViewer::instance()->getStaticDebugFile()));
                }

            // sBugSplatSender->sendAdditionalFile(
            //   WCSTR(gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, "settings.xml")));
            if (gCrashSettings.getBOOL("CrashSubmitSettings"))
                sBugSplatSender->sendAdditionalFile(  WCSTR(gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, "settings.xml")));

            // We don't have an email address for any user. Hijack this
            // metadata field for the platform identifier.
            // sBugSplatSender->setDefaultUserEmail(
            //     WCSTR(LLOSInfo::instance().getOSStringSimple()));

            // <FS:ND> Add which flavor of FS generated an error
            std::string flavor = "hvk";
#ifdef OPENSIM
            flavor = "oss";
#endif
            sBugSplatSender->setDefaultUserEmail( WCSTR(STRINGIZE(LLOSInfo::instance().getOSStringSimple() << " ("  << ADDRESS_SIZE << "-bit, flavor " << flavor <<")")));
            BugSplatAttributes::instance().setAttribute("Flavor", flavor);
            // </FS:ND>

            //<FS:ND/> Clear out username first, as we get some crashes that has the OS set as username, let's see if this fixes it. Use Crash.Linden as a usr can never have a "Linden"
            // name and on the other hand a Linden will not likely ever crash on Firestom.
            sBugSplatSender->setDefaultUserName( WCSTR("Crash.Linden") );

            if (gAgentAvatarp)
            {
                // <FS:ND> Only send avatar name if enabled via prefs
                if (gCrashSettings.getBOOL("CrashSubmitName"))
                // </FS:ND>
                {
                    // user name, when we have it
                    sBugSplatSender->setDefaultUserName(WCSTR(gAgentAvatarp->getFullname()));
                // <FS:ND> Only send avatar name if enabled via prefs
                }
                // </FS:ND>

                //<FS:Ansariel> Only include if sending settings file
                //sBugSplatSender->sendAdditionalFile(
                //    WCSTR(gDirUtilp->getExpandedFilename(LL_PATH_PER_SL_ACCOUNT, "settings_per_account.xml")));
                if (gCrashSettings.getBOOL("CrashSubmitSettings"))
                {
                    sBugSplatSender->sendAdditionalFile(
                        WCSTR(gDirUtilp->getExpandedFilename(LL_PATH_PER_SL_ACCOUNT, "settings_per_account.xml")));
                }
                // <FS:Ansariel>
            }

            // LL_ERRS message, when there is one
            // <FS:Beq> Improve bugsplpat reporting with attributes
           // sBugSplatSender->setDefaultUserDescription(WCSTR(LLError::getFatalMessage()));
            // sBugSplatSender->setAttribute(WCSTR(L"OS"), WCSTR(LLOSInfo::instance().getOSStringSimple())); // In case we ever stop using email for this
            // sBugSplatSender->setAttribute(WCSTR(L"AppState"), WCSTR(LLStartUp::getStartupStateString()));
            // sBugSplatSender->setAttribute(WCSTR(L"GL Vendor"), WCSTR(gGLManager.mGLVendor));
            // sBugSplatSender->setAttribute(WCSTR(L"GL Version"), WCSTR(gGLManager.mGLVersionString));
            // sBugSplatSender->setAttribute(WCSTR(L"GPU Version"), WCSTR(gGLManager.mDriverVersionVendorString));
            // sBugSplatSender->setAttribute(WCSTR(L"GL Renderer"), WCSTR(gGLManager.mGLRenderer));
            // sBugSplatSender->setAttribute(WCSTR(L"VRAM"), WCSTR(STRINGIZE(gGLManager.mVRAM)));
            // sBugSplatSender->setAttribute(WCSTR(L"RAM"), WCSTR(STRINGIZE(gSysMemory.getPhysicalMemoryKB().value())));

            auto fatal_message = LLError::getFatalMessage();
            sBugSplatSender->setDefaultUserDescription(WCSTR(fatal_message));
            BugSplatAttributes::instance().setAttribute("FatalMessage", fatal_message); // <FS:Beq/> Store this additionally as an attribute in case user overwrites.
            // App state
            BugSplatAttributes::instance().setAttribute("AppState", LLStartUp::getStartupStateString());
            // Location
            // </FS:Beq>
            if (gAgent.getRegion())
            {
                // region location, when we have it
                // <FS:Beq> Improve bugsplat reporting with attributes
                // LLVector3 loc = gAgent.getPositionAgent();
                // sBugSplatSender->resetAppIdentifier(
                //     WCSTR(STRINGIZE(gAgent.getRegion()->getName()
                //                     << '/' << loc.mV[0]
                //                     << '/' << loc.mV[1]
                //                     << '/' << loc.mV[2])));
                const LLVector3 loc = gAgent.getPositionAgent();
                const auto & fullLocation = STRINGIZE(gAgent.getRegion()->getName()
                                    << '/' << loc.mV[0]
                                    << '/' << loc.mV[1]
                                    << '/' << loc.mV[2]);
                sBugSplatSender->resetAppIdentifier(WCSTR(fullLocation));
                BugSplatAttributes::instance().setAttribute("Location", std::string(fullLocation));
                // </FS:Beq>
            }
            // <FS:Beq> Improve bugsplat reporting with attributes
            LLAppViewer::instance()->writeDebugInfo();            
            sBugSplatSender->sendAdditionalFile(WCSTR(BugSplatAttributes::getCrashContextFileName())); // <FS:Beq/> Add the new attributes file
            // </FS:Beq>
        } // MDSCB_EXCEPTIONCODE

        return false;
    }
}
#endif // LL_BUGSPLAT

namespace
{
    void (*gOldTerminateHandler)() = NULL;
}

static void exceptionTerminateHandler()
{
    // reinstall default terminate() handler in case we re-terminate.
    if (gOldTerminateHandler) std::set_terminate(gOldTerminateHandler);
    // treat this like a regular viewer crash, with nice stacktrace etc.
    long *null_ptr;
    null_ptr = 0;
    *null_ptr = 0xDEADBEEF; //Force an exception that will trigger breakpad.

    // we've probably been killed-off before now, but...
    gOldTerminateHandler(); // call old terminate() handler
}

LONG WINAPI catchallCrashHandler(EXCEPTION_POINTERS * /*ExceptionInfo*/)
{
    LL_WARNS() << "Hit last ditch-effort attempt to catch crash." << LL_ENDL;
    exceptionTerminateHandler();
    return 0;
}

const std::string LLAppViewerWin32::sWindowClass = "Second Life";

/*
    This function is used to print to the command line a text message
    describing the nvapi error and quits
*/
void nvapi_error(NvAPI_Status status)
{
    NvAPI_ShortString szDesc = {0};
    NvAPI_GetErrorMessage(status, szDesc);
    LL_WARNS() << szDesc << LL_ENDL;

    //should always trigger when asserts are enabled
    //llassert(status == NVAPI_OK);
}

// Create app mutex creates a unique global windows object.
// If the object can be created it returns true, otherwise
// it returns false. The false result can be used to determine
// if another instance of a second life app (this vers. or later)
// is running.
// *NOTE: Do not use this method to run a single instance of the app.
// This is intended to help debug problems with the cross-platform
// locked file method used for that purpose.
bool create_app_mutex()
{
    bool result = true;
    LPCWSTR unique_mutex_name = L"SecondLifeAppMutex";
    HANDLE hMutex;
    hMutex = CreateMutex(NULL, TRUE, unique_mutex_name);
    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        result = false;
    }
    return result;
}

void ll_nvapi_init(NvDRSSessionHandle hSession)
{
    // (2) load all the system settings into the session
    NvAPI_Status status = NvAPI_DRS_LoadSettings(hSession);
    if (status != NVAPI_OK)
    {
        nvapi_error(status);
        return;
    }

    NvAPI_UnicodeString profile_name;
    std::string app_name = LLTrans::getString("APP_NAME");
    llutf16string w_app_name = utf8str_to_utf16str(app_name);
    wsprintf(profile_name, L"%s", w_app_name.c_str());
    NvDRSProfileHandle hProfile = 0;
    // (3) Check if we already have an application profile for the viewer
    status = NvAPI_DRS_FindProfileByName(hSession, profile_name, &hProfile);
    if (status != NVAPI_OK && status != NVAPI_PROFILE_NOT_FOUND)
    {
        nvapi_error(status);
        return;
    }
    else if (status == NVAPI_PROFILE_NOT_FOUND)
    {
        // Don't have an application profile yet - create one
        LL_INFOS() << "Creating NVIDIA application profile" << LL_ENDL;

        NVDRS_PROFILE profileInfo;
        profileInfo.version = NVDRS_PROFILE_VER;
        profileInfo.isPredefined = 0;
        wsprintf(profileInfo.profileName, L"%s", w_app_name.c_str());

        status = NvAPI_DRS_CreateProfile(hSession, &profileInfo, &hProfile);
        if (status != NVAPI_OK)
        {
            nvapi_error(status);
            return;
        }
    }

    // (4) Check if current exe is part of the profile
    std::string exe_name = gDirUtilp->getExecutableFilename();
    NVDRS_APPLICATION profile_application;
    profile_application.version = NVDRS_APPLICATION_VER;

    llutf16string w_exe_name = utf8str_to_utf16str(exe_name);
    NvAPI_UnicodeString profile_app_name;
    wsprintf(profile_app_name, L"%s", w_exe_name.c_str());

    status = NvAPI_DRS_GetApplicationInfo(hSession, hProfile, profile_app_name, &profile_application);
    if (status != NVAPI_OK && status != NVAPI_EXECUTABLE_NOT_FOUND)
    {
        nvapi_error(status);
        return;
    }
    else if (status == NVAPI_EXECUTABLE_NOT_FOUND)
    {
        LL_INFOS() << "Creating application for " << exe_name << " for NVIDIA application profile" << LL_ENDL;

        // Add this exe to the profile
        NVDRS_APPLICATION application;
        application.version = NVDRS_APPLICATION_VER;
        application.isPredefined = 0;
        wsprintf(application.appName, L"%s", w_exe_name.c_str());
        wsprintf(application.userFriendlyName, L"%s", w_exe_name.c_str());
        wsprintf(application.launcher, L"%s", w_exe_name.c_str());
        wsprintf(application.fileInFolder, L"%s", "");

        status = NvAPI_DRS_CreateApplication(hSession, hProfile, &application);
        if (status != NVAPI_OK)
        {
            nvapi_error(status);
            return;
        }

        // Save application in case we added one
        status = NvAPI_DRS_SaveSettings(hSession);
        if (status != NVAPI_OK)
        {
            nvapi_error(status);
            return;
        }
    }

    // load settings for querying
    status = NvAPI_DRS_LoadSettings(hSession);
    if (status != NVAPI_OK)
    {
        nvapi_error(status);
        return;
    }

    //get the preferred power management mode for Second Life
    NVDRS_SETTING drsSetting = {0};
    drsSetting.version = NVDRS_SETTING_VER;
    status = NvAPI_DRS_GetSetting(hSession, hProfile, PREFERRED_PSTATE_ID, &drsSetting);
    if (status == NVAPI_SETTING_NOT_FOUND)
    { //only override if the user hasn't specifically set this setting
        // (5) Specify that we want to enable maximum performance setting
        // first we fill the NVDRS_SETTING struct, then we call the function
        drsSetting.version = NVDRS_SETTING_VER;
        drsSetting.settingId = PREFERRED_PSTATE_ID;
        drsSetting.settingType = NVDRS_DWORD_TYPE;
        drsSetting.u32CurrentValue = PREFERRED_PSTATE_PREFER_MAX;
        status = NvAPI_DRS_SetSetting(hSession, hProfile, &drsSetting);
        if (status != NVAPI_OK)
        {
            nvapi_error(status);
            return;
        }

        // (6) Now we apply (or save) our changes to the system
        status = NvAPI_DRS_SaveSettings(hSession);
        if (status != NVAPI_OK)
        {
            nvapi_error(status);
            return;
        }
    }
    else if (status != NVAPI_OK)
    {
        nvapi_error(status);
        return;
    }

    // enable Threaded Optimization instead of letting the driver decide
    status = NvAPI_DRS_GetSetting(hSession, hProfile, OGL_THREAD_CONTROL_ID, &drsSetting);
    if (status == NVAPI_SETTING_NOT_FOUND || (status == NVAPI_OK && drsSetting.u32CurrentValue != OGL_THREAD_CONTROL_ENABLE))
    {
        drsSetting.version = NVDRS_SETTING_VER;
        drsSetting.settingId = OGL_THREAD_CONTROL_ID;
        drsSetting.settingType = NVDRS_DWORD_TYPE;
        drsSetting.u32CurrentValue = OGL_THREAD_CONTROL_ENABLE;
        status = NvAPI_DRS_SetSetting(hSession, hProfile, &drsSetting);
        if (status != NVAPI_OK)
        {
            nvapi_error(status);
            return;
        }

        // Now we apply (or save) our changes to the system
        status = NvAPI_DRS_SaveSettings(hSession);
        if (status != NVAPI_OK)
        {
            nvapi_error(status);
            return;
        }
    }
    else if (status != NVAPI_OK)
    {
        nvapi_error(status);
        return;
    }
}

//#define DEBUGGING_SEH_FILTER 1
#if DEBUGGING_SEH_FILTER
#   define WINMAIN DebuggingWinMain
#else
#   define WINMAIN wWinMain
#endif

int APIENTRY WINMAIN(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     PWSTR     pCmdLine,
                     int       nCmdShow)
{
    // Call Tracy first thing to have it allocate memory
    // https://github.com/wolfpld/tracy/issues/196
    LL_PROFILER_FRAME_END;
    LL_PROFILER_SET_THREAD_NAME("App");

    const S32 MAX_HEAPS = 255;
    DWORD heap_enable_lfh_error[MAX_HEAPS];
    S32 num_heaps = 0;

    // <FS:Ansariel> Set via manifest
    //LLWindowWin32::setDPIAwareness();

#if WINDOWS_CRT_MEM_CHECKS && !INCLUDE_VLD
    _CrtSetDbgFlag ( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF ); // dump memory leaks on exit
#elif 0
    // Experimental - enable the low fragmentation heap
    // This results in a 2-3x improvement in opening a new Inventory window (which uses a large numebr of allocations)
    // Note: This won't work when running from the debugger unless the _NO_DEBUG_HEAP environment variable is set to 1

    // Enable to get mem debugging within visual studio.
#if LL_DEBUG
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#else
    _CrtSetDbgFlag(0); // default, just making explicit

    ULONG ulEnableLFH = 2;
    HANDLE* hHeaps = new HANDLE[MAX_HEAPS];
    num_heaps = GetProcessHeaps(MAX_HEAPS, hHeaps);
    for(S32 i = 0; i < num_heaps; i++)
    {
        bool success = HeapSetInformation(hHeaps[i], HeapCompatibilityInformation, &ulEnableLFH, sizeof(ulEnableLFH));
        if (success)
            heap_enable_lfh_error[i] = 0;
        else
            heap_enable_lfh_error[i] = GetLastError();
    }
#endif
#endif

    // *FIX: global
    gIconResource = MAKEINTRESOURCE(IDI_LL_ICON);

    LLAppViewerWin32* viewer_app_ptr = new LLAppViewerWin32(ll_convert_wide_to_string(pCmdLine).c_str());

    gOldTerminateHandler = std::set_terminate(exceptionTerminateHandler);

    // Set a debug info flag to indicate if multiple instances are running.
    bool found_other_instance = !create_app_mutex();
    gDebugInfo["FoundOtherInstanceAtStartup"] = LLSD::Boolean(found_other_instance);

    bool ok = viewer_app_ptr->init();
    if (!ok)
    {
        LL_WARNS() << "Application init failed." << LL_ENDL;
        return -1;
    }

    NvDRSSessionHandle hSession = 0;
    static LLCachedControl<bool> use_nv_api(gSavedSettings, "NvAPICreateApplicationProfile", true);
    if (use_nv_api)
    {
        NvAPI_Status status;

        // Initialize NVAPI
        status = NvAPI_Initialize();

        if (status == NVAPI_OK)
        {
            // Create the session handle to access driver settings
            status = NvAPI_DRS_CreateSession(&hSession);
            if (status != NVAPI_OK)
            {
                nvapi_error(status);
            }
            else
            {
                //override driver setting as needed
                ll_nvapi_init(hSession);
            }
        }
    }

    // Have to wait until after logging is initialized to display LFH info
    if (num_heaps > 0)
    {
        LL_INFOS() << "Attempted to enable LFH for " << num_heaps << " heaps." << LL_ENDL;
        for(S32 i = 0; i < num_heaps; i++)
        {
            if (heap_enable_lfh_error[i])
            {
                LL_INFOS() << "  Failed to enable LFH for heap: " << i << " Error: " << heap_enable_lfh_error[i] << LL_ENDL;
            }
        }
    }

    // Run the application main loop
    while (! viewer_app_ptr->frame())
    {}

    if (!LLApp::isError())
    {
        //
        // We don't want to do cleanup here if the error handler got called -
        // the assumption is that the error handler is responsible for doing
        // app cleanup if there was a problem.
        //
#if WINDOWS_CRT_MEM_CHECKS
        LL_INFOS() << "CRT Checking memory:" << LL_ENDL;
        if (!_CrtCheckMemory())
        {
            LL_WARNS() << "_CrtCheckMemory() failed at prior to cleanup!" << LL_ENDL;
        }
        else
        {
            LL_INFOS() << " No corruption detected." << LL_ENDL;
        }
#endif

        gGLActive = true;

        viewer_app_ptr->cleanup();

#if WINDOWS_CRT_MEM_CHECKS
        LL_INFOS() << "CRT Checking memory:" << LL_ENDL;
        if (!_CrtCheckMemory())
        {
            LL_WARNS() << "_CrtCheckMemory() failed after cleanup!" << LL_ENDL;
        }
        else
        {
            LL_INFOS() << " No corruption detected." << LL_ENDL;
        }
#endif

    }
    delete viewer_app_ptr;
    viewer_app_ptr = NULL;

    // (NVAPI) (6) We clean up. This is analogous to doing a free()
    if (hSession)
    {
        NvAPI_DRS_DestroySession(hSession);
        hSession = 0;
    }

    return 0;
}

#if DEBUGGING_SEH_FILTER
// The compiler doesn't like it when you use __try/__except blocks
// in a method that uses object destructors. Go figure.
// This winmain just calls the real winmain inside __try.
// The __except calls our exception filter function. For debugging purposes.
int APIENTRY wWinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     PWSTR     lpCmdLine,
                     int       nCmdShow)
{
    __try
    {
        WINMAIN(hInstance, hPrevInstance, lpCmdLine, nCmdShow);
    }
    __except( viewer_windows_exception_handler( GetExceptionInformation() ) )
    {
        _tprintf( _T("Exception handled.\n") );
    }
}
#endif
// <FS:Beq> Use the Attributes API on Windows to enhance crash metadata
void LLAppViewerWin32::bugsplatAddStaticAttributes(const LLSD& info)
{
#ifdef LL_BUGSPLAT
    auto& bugSplatMap = BugSplatAttributes::instance();
    static bool write_once_after_startup = false;
    if (!write_once_after_startup )
    {
        // Only write the attributes that are fixed once after we've started.
        // note we might update them more than once and some/many may be empty during startup as we want to catch early crashes
        // once we're started we can assume they don't change for this run.
        if( LLStartUp::getStartupState() == STATE_STARTED)
        {
            write_once_after_startup = true;
        }

        auto multipleInstances = gDebugInfo["FoundOtherInstanceAtStartup"].asBoolean();
        bugSplatMap.setAttribute("MultipleInstance", multipleInstances);

        bugSplatMap.setAttribute("GPU", info["GRAPHICS_CARD"].asString());
        bugSplatMap.setAttribute("GPU VRAM Detected (MB)", info["GRAPHICS_CARD_MEMORY_DETECTED"].asInteger());
        bugSplatMap.setAttribute("GPU VRAM (Budget)", info["VRAM_BUDGET_ENGLISH"].asInteger());

        bugSplatMap.setAttribute("CPU", info["CPU"].asString());
        bugSplatMap.setAttribute("Graphics Driver", info["GRAPHICS_DRIVER_VERSION"].asString());
        bugSplatMap.setAttribute("CPU MHz", (S32)gSysCPU.getMHz()); // 
#ifdef USE_AVX2_OPTIMIZATION
        bugSplatMap.setAttribute("SIMD", "AVX2");
#elif USE_AVX_OPTIMIZATION
        bugSplatMap.setAttribute("SIMD", "AVX");
#else
        bugSplatMap.setAttribute("SIMD", "SSE2");
#endif
    // set physical ram integer as a string attribute
        bugSplatMap.setAttribute("Physical RAM (KB)", LLMemory::getMaxMemKB().value());
        bugSplatMap.setAttribute("OpenGL Version", info["OPENGL_VERSION"].asString());
        bugSplatMap.setAttribute("libcurl Version", info["LIBCURL_VERSION"].asString());
        bugSplatMap.setAttribute("J2C Decoder Version", info["J2C_VERSION"].asString());
        bugSplatMap.setAttribute("Audio Driver Version", info["AUDIO_DRIVER_VERSION"].asString());
    // bugSplatMap.setAttribute("CEF Info", info["LIBCEF_VERSION"].asString());
        bugSplatMap.setAttribute("LibVLC Version", info["LIBVLC_VERSION"].asString());
        bugSplatMap.setAttribute("Vivox Version", info["VOICE_VERSION"].asString());
        bugSplatMap.setAttribute("RLVa", info["RLV_VERSION"].asString());
        bugSplatMap.setAttribute("Mode", info["MODE"].asString());
        bugSplatMap.setAttribute("Skin", llformat("%s (%s)", info["SKIN"].asString().c_str(), info["THEME"].asString().c_str()));
    #if LL_DARWIN
        bugSplatMap.setAttribute("HiDPI", info["HIDPI"].asBoolean() ? "Enabled" : "Disabled");
    #endif
        bugSplatMap.setAttribute("Max Texture Size", gSavedSettings.getString("RenderMaxTextureResolution"));
    }

    // These attributes are potentially dynamic
    bugSplatMap.setAttribute("Packets Lost", llformat("%.0f/%.0f (%.1f%%)", info["PACKETS_LOST"].asReal(), info["PACKETS_IN"].asReal(), info["PACKETS_PCT"].asReal()));
    bugSplatMap.setAttribute("Window Size", llformat("%sx%s px", info["WINDOW_WIDTH"].asString().c_str(), info["WINDOW_HEIGHT"].asString().c_str()));
    bugSplatMap.setAttribute("Draw Distance (m)", info["DRAW_DISTANCE"].asInteger());
    bugSplatMap.setAttribute("Bandwidth (kbit/s)", info["BANDWIDTH"].asInteger());
    bugSplatMap.setAttribute("LOD Factor", info["LOD"].asReal());
    bugSplatMap.setAttribute("Render quality", info["RENDERQUALITY_FSDATA_ENGLISH"].asString());
    bugSplatMap.setAttribute("Disk Cache", info["DISK_CACHE_INFO"].asString());

    bugSplatMap.setAttribute("GridName", gDebugInfo["GridName"].asString());
    LLMemory::updateMemoryInfo();
    bugSplatMap.setAttribute("Available RAM (KB)", LLMemory::getAvailableMemKB().value());
    bugSplatMap.setAttribute("Allocated RAM (KB)", LLMemory::getAllocatedMemKB().value());
    bugSplatMap.setAttribute("GPU VRAM (MB)", info["GRAPHICS_CARD_MEMORY"].asInteger());


    if (bugSplatMap.writeToFile(BugSplatAttributes::getCrashContextFileName()))
    {
        LL_INFOS() << "Crash context saved to " << BugSplatAttributes::getCrashContextFileName() << LL_ENDL;
    }
#endif
}
// </FS:Beq>

void LLAppViewerWin32::disableWinErrorReporting()
{
    std::string executable_name = gDirUtilp->getExecutableFilename();

    if( S_OK == WerAddExcludedApplication( utf8str_to_utf16str(executable_name).c_str(), FALSE ) )
    {
        LL_INFOS() << "WerAddExcludedApplication() succeeded for " << executable_name << LL_ENDL;
    }
    else
    {
        LL_INFOS() << "WerAddExcludedApplication() failed for " << executable_name << LL_ENDL;
    }
}

const S32 MAX_CONSOLE_LINES = 7500;
// Only defined in newer SDKs than we currently use
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 4
#endif

namespace {

void set_stream(const char* desc, FILE* fp, DWORD handle_id, const char* name, const char* mode="w");

bool create_console()
{
    // allocate a console for this app
    const bool isConsoleAllocated = AllocConsole();

    if (isConsoleAllocated)
    {
        // set the screen buffer to be big enough to let us scroll text
        CONSOLE_SCREEN_BUFFER_INFO coninfo;
        GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &coninfo);
        coninfo.dwSize.Y = MAX_CONSOLE_LINES;
        SetConsoleScreenBufferSize(GetStdHandle(STD_OUTPUT_HANDLE), coninfo.dwSize);

        // redirect unbuffered STDOUT to the console
        set_stream("stdout", stdout, STD_OUTPUT_HANDLE, "CONOUT$");
        // redirect unbuffered STDERR to the console
        set_stream("stderr", stderr, STD_ERROR_HANDLE, "CONOUT$");
        // redirect unbuffered STDIN to the console
        // Don't bother: our console is solely for log output. We never read stdin.
//      set_stream("stdin", stdin, STD_INPUT_HANDLE, "CONIN$", "r");
    }

    return isConsoleAllocated;
}

void set_stream(const char* desc, FILE* fp, DWORD handle_id, const char* name, const char* mode)
{
    // SL-13528: This code used to be based on
    // http://dslweb.nwnexus.com/~ast/dload/guicon.htm
    // (referenced in https://stackoverflow.com/a/191880).
    // But one of the comments on that StackOverflow answer points out that
    // assigning to *stdout or *stderr "probably doesn't even work with the
    // Universal CRT that was introduced in 2015," suggesting freopen_s()
    // instead. Code below is based on https://stackoverflow.com/a/55875595.
    auto std_handle = GetStdHandle(handle_id);
    if (std_handle == INVALID_HANDLE_VALUE)
    {
        LL_WARNS() << "create_console() failed to get " << desc << " handle" << LL_ENDL;
    }
    else
    {
        if (mode == std::string("w"))
        {
            // Enable color processing on Windows 10 console windows.
            DWORD dwMode = 0;
            GetConsoleMode(std_handle, &dwMode);
            dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(std_handle, dwMode);
        }
        // Redirect the passed fp to the console.
        FILE* ignore;
        if (freopen_s(&ignore, name, mode, fp) == 0)
        {
            // use unbuffered I/O
            setvbuf( fp, NULL, _IONBF, 0 );
        }
    }
}

} // anonymous namespace

LLAppViewerWin32::LLAppViewerWin32(const char* cmd_line) :
    mCmdLine(cmd_line),
    mIsConsoleAllocated(false)
{
}

LLAppViewerWin32::~LLAppViewerWin32()
{
}

// <FS:ND> Check if %TEMP% is defined and accessible (see FIRE-29623, sometimes BugSplat has problems to access TEMP, try to find out why)
static void checkTemp()
{
    char *pTemp{ getenv("TEMP") };
    if (!pTemp)
    {
        LL_WARNS() << "%TEMP% is not set" << LL_ENDL;
    }
    else
    {
        LL_INFOS() << "%TEMP%: " << pTemp << LL_ENDL;
        DWORD dwAttr = ::GetFileAttributesA(pTemp);
        DWORD dwLE = ::GetLastError();
        if (dwAttr == INVALID_FILE_ATTRIBUTES)
        {
            LL_WARNS() << "%TEMP%: " << pTemp << " GetFileAttributesA failed, last error: " << dwLE << LL_ENDL;
        }
        else if (0 == (dwAttr & FILE_ATTRIBUTE_DIRECTORY))
        {
            LL_WARNS() << "%TEMP%: " << pTemp << " is not a directory" << LL_ENDL;
        }
        else
        {
            LLUUID id = LLUUID::generateNewID();
            std::string strFile{ pTemp };
            if (strFile[strFile.size() - 1] != '/' && strFile[strFile.size() - 1] != '\\')
                strFile += "\\";

            strFile += id.asString();
            FILE *fp = fopen(strFile.c_str(), "w");
            if (!fp)
            {
                LL_WARNS() << "%TEMP%: " << pTemp << " cannot create file " << strFile << LL_ENDL;
            }
            else
            {
                fclose(fp);
                remove(strFile.c_str());
                LL_INFOS() << "%TEMP%: " << pTemp << " successfully created file " << strFile << LL_ENDL;
            }
        }
    }
}
// </FS:ND>

bool LLAppViewerWin32::init()
{
    bool success{ false }; // <FS:ND/> For BugSplat we need to call base::init() early on or there's no access to settings.
    // Platform specific initialization.

    // Turn off Windows Error Reporting
    // (Don't send our data to Microsoft--at least until we are Logo approved and have a way
    // of getting the data back from them.)
    //
    LL_INFOS() << "Turning off Windows error reporting." << LL_ENDL;
    disableWinErrorReporting();

#ifndef LL_RELEASE_FOR_DOWNLOAD
    // Merely requesting the LLSingleton instance initializes it.
    LLWinDebug::instance();
#endif

#if LL_SEND_CRASH_REPORTS
#if ! defined(LL_BUGSPLAT)
#pragma message("Building without BugSplat")

#else // LL_BUGSPLAT
#pragma message("Building with BugSplat")
    // <FS:ND> Pre BugSplat dance, make sure settings are valid, query crash behavior and then set up Bugsplat accordingly"
    //if (!isSecondInstance())
    //{
    //    // Cleanup previous session
    //    std::string log_file = gDirUtilp->getExpandedFilename(LL_PATH_LOGS, "bugsplat.log");
    //    LLFile::remove(log_file, ENOENT);
    //}

    // Win7 is no longer supported
    bool is_win_7_or_below = LLOSInfo::getInstance()->mMajorVer <= 6 && LLOSInfo::getInstance()->mMajorVer <= 1;

    if (!is_win_7_or_below)
    {
        success = LLAppViewer::init();
        if (!success)
            return false;

        checkTemp(); // Always do and log this, no matter if using Bugsplat or not

        // Save those early so we don't have to deal with the dynamic memory during in process crash handling.
        FS::LogfileIn = ll_convert_string_to_wide(gDirUtilp->getExpandedFilename(LL_PATH_LOGS, "Firestorm.log"));
        FS::LogfileOut = ll_convert_string_to_wide(gDirUtilp->getExpandedFilename(LL_PATH_DUMP, "Firestorm.log"));
        FS::DumpFile = ll_convert_string_to_wide(gDirUtilp->getExpandedFilename(LL_PATH_DUMP, "Firestorm.dmp"));

        S32 nCrashSubmitBehavior = gCrashSettings.getS32("CrashSubmitBehavior");
        // Don't ever send? bail out!
        if (nCrashSubmitBehavior == 2 /*CRASH_BEHAVIOR_NEVER_SEND*/)
            return success;

        DWORD dwAsk{ MDSF_NONINTERACTIVE };
        if (nCrashSubmitBehavior == 0 /*CRASH_BEHAVIOR_ASK*/)
            dwAsk = 0;
        // </FS:ND>

        std::string build_data_fname(
            gDirUtilp->getExpandedFilename(LL_PATH_EXECUTABLE, "build_data.json"));
        // Use llifstream instead of std::ifstream because LL_PATH_EXECUTABLE
        // could contain non-ASCII characters, which std::ifstream doesn't handle.
        llifstream inf(build_data_fname.c_str());
        if (! inf.is_open())
        {
            LL_WARNS("BUGSPLAT") << "Can't initialize BugSplat, can't read '" << build_data_fname
                       << "'" << LL_ENDL;
        }
        else
        {
            boost::system::error_code ec;
            boost::json::value build_data = boost::json::parse(inf, ec);
            if(ec.failed())
            {
                // gah, the typo is baked into Json::Reader API
                LL_WARNS("BUGSPLAT") << "Can't initialize BugSplat, can't parse '" << build_data_fname
                    << "': " << ec.what() << LL_ENDL;
            }
            else
            {
                if (!build_data.is_object() || !build_data.as_object().contains("BugSplat DB"))
                {
                    LL_WARNS("BUGSPLAT") << "Can't initialize BugSplat, no 'BugSplat DB' entry in '"
                               << build_data_fname << "'" << LL_ENDL;
                }
                else
                {
                    boost::json::value BugSplat_DB = build_data.at("BugSplat DB");

                    // Got BugSplat_DB, onward!
                    std::wstring version_string(WSTRINGIZE(LL_VIEWER_VERSION_MAJOR << '.' <<
                                                           LL_VIEWER_VERSION_MINOR << '.' <<
                                                           LL_VIEWER_VERSION_PATCH << '.' <<
                                                           LL_VIEWER_VERSION_BUILD));

                    // <FS:ND> Set up Bugsplat to ask or always send
                    //DWORD dwFlags = MDSF_NONINTERACTIVE | // automatically submit report without prompting
                    //                MDSF_PREVENTHIJACKING; // disallow swiping Exception filter
                    DWORD dwFlags = dwAsk |
                                    MDSF_PREVENTHIJACKING; // disallow swiping Exception filter
                    // </FS:ND>

                    //bool needs_log_file = !isSecondInstance() && debugLoggingEnabled("BUGSPLAT");
                    //LL_DEBUGS("BUGSPLAT");
                    //if (needs_log_file)
                    //{
                    //    // Startup only!
                    //    LL_INFOS("BUGSPLAT") << "Engaged BugSplat logging to bugsplat.log" << LL_ENDL;
                    //    dwFlags |= MDSF_LOGFILE | MDSF_LOG_VERBOSE;
                    //}
                    //LL_ENDL;

                    // have to convert normal wide strings to strings of __wchar_t
                    sBugSplatSender = new MiniDmpSender(
                        WCSTR(boost::json::value_to<std::string>(BugSplat_DB)),
                        WCSTR(LL_TO_WSTRING(LL_VIEWER_CHANNEL)),
                        WCSTR(version_string),
                        nullptr,              // szAppIdentifier -- set later
                        dwFlags);

                    sBugSplatSender->setCallback(bugsplatSendLog);

                    //LL_DEBUGS("BUGSPLAT");
                    //if (needs_log_file)
                    //{
                    //    // Log file will be created in %TEMP%, but it will be moved into logs folder in case of crash
                    //    std::string log_file = gDirUtilp->getExpandedFilename(LL_PATH_LOGS, "bugsplat.log");
                    //    sBugSplatSender->setLogFilePath(WCSTR(log_file));
                    //}
                    //LL_ENDL;

                    // engage stringize() overload that converts from wstring
                    LL_INFOS("BUGSPLAT") << "Engaged BugSplat(" << LL_TO_STRING(LL_VIEWER_CHANNEL)
                               << ' ' << stringize(version_string) << ')' << LL_ENDL;
                } // got BugSplat_DB
            } // parsed build_data.json
        } // opened build_data.json
    } // !is_win_7_or_below

#endif // LL_BUGSPLAT
#endif // LL_SEND_CRASH_REPORTS

    // <FS:ND> base::init() was potentially called earlier.
    // bool success = LLAppViewer::init();
    // </FS:ND>

    if( !success )
        success = LLAppViewer::init();

    return success;
}

bool LLAppViewerWin32::cleanup()
{
    bool result = LLAppViewer::cleanup();

    gDXHardware.cleanup();

    if (mIsConsoleAllocated)
    {
        FreeConsole();
        mIsConsoleAllocated = false;
    }

    return result;
}

void LLAppViewerWin32::reportCrashToBugsplat(void* pExcepInfo)
{
#if defined(LL_BUGSPLAT)
    if (sBugSplatSender)
    {
        sBugSplatSender->createReport((EXCEPTION_POINTERS*)pExcepInfo);
    }
#endif // LL_BUGSPLAT
}

void LLAppViewerWin32::initLoggingAndGetLastDuration()
{
    LLAppViewer::initLoggingAndGetLastDuration();
}

void LLAppViewerWin32::initConsole()
{
    // pop up debug console
    mIsConsoleAllocated = create_console();
    return LLAppViewer::initConsole();
}

void write_debug_dx(const char* str)
{
    std::string value = gDebugInfo["DXInfo"].asString();
    value += str;
    gDebugInfo["DXInfo"] = value;
}

void write_debug_dx(const std::string& str)
{
    write_debug_dx(str.c_str());
}

bool LLAppViewerWin32::initHardwareTest()
{
    //
    // Do driver verification and initialization based on DirectX
    // hardware polling and driver versions
    //
    if (/*true == gSavedSettings.getBOOL("ProbeHardwareOnStartup") &&*/ false == gSavedSettings.getBOOL("NoHardwareProbe")) // <FS:Ansariel> FIRE-20378 / FIRE-20382: Breaks memory detection an 4K monitor workaround
    {
        // per DEV-11631 - disable hardware probing for everything
        // but vram.
        bool vram_only = true;

        LLSplashScreen::update(LLTrans::getString("StartupDetectingHardware"));

        LL_DEBUGS("AppInit") << "Attempting to poll DirectX for hardware info" << LL_ENDL;
        gDXHardware.setWriteDebugFunc(write_debug_dx);
        // <FS:Ansariel> FIRE-15891: Add option to disable WMI check in case of problems
        //bool probe_ok = gDXHardware.getInfo(vram_only);
        bool probe_ok = gDXHardware.getInfo(vram_only, gSavedSettings.getBOOL("FSDisableWMIProbing"));
        // </FS:Ansariel>

        if (!probe_ok
            && gWarningSettings.getBOOL("AboutDirectX9"))
        {
            LL_WARNS("AppInit") << "DirectX probe failed, alerting user." << LL_ENDL;

            // Warn them that runnin without DirectX 9 will
            // not allow us to tell them about driver issues
            std::ostringstream msg;
            msg << LLTrans::getString ("MBNoDirectX");
            S32 button = OSMessageBox(
                msg.str(),
                LLTrans::getString("MBWarning"),
                OSMB_YESNO);
            if (OSBTN_NO== button)
            {
                LL_INFOS("AppInit") << "User quitting after failed DirectX 9 detection" << LL_ENDL;
                LLWeb::loadURLExternal("http://www.firestormviewer.org/support", false);
                return false;
            }
            gWarningSettings.setBOOL("AboutDirectX9", false);
        }
        LL_DEBUGS("AppInit") << "Done polling DirectX for hardware info" << LL_ENDL;

        // Only probe once after installation
        gSavedSettings.setBOOL("ProbeHardwareOnStartup", false);

        // Disable so debugger can work
        std::string splash_msg;
        LLStringUtil::format_map_t args;
        args["[APP_NAME]"] = LLAppViewer::instance()->getSecondLifeTitle();
        args["[CURRENT_GRID]"] = LLGridManager::getInstance()->getGridLabel();
        splash_msg = LLTrans::getString("StartupLoading", args);

        LLSplashScreen::update(splash_msg);
    }

    if (!restoreErrorTrap())
    {
        LL_WARNS("AppInit") << " Someone took over my exception handler (post hardware probe)!" << LL_ENDL;
    }

    if (gGLManager.mVRAM == 0)
    {
        gGLManager.mVRAM = gDXHardware.getVRAM();
    }

    // LL_INFOS("AppInit") << "Detected VRAM: " << gGLManager.mVRAM << LL_ENDL; // <FS:Beq/> move this into common code

    return true;
}

bool LLAppViewerWin32::initParseCommandLine(LLCommandLineParser& clp)
{
    if (!clp.parseCommandLineString(mCmdLine))
    {
        return false;
    }

    // Find the system language.
    FL_Locale *locale = NULL;
    FL_Success success = FL_FindLocale(&locale, FL_MESSAGES);
    if (success != 0)
    {
        if (success >= 2 && locale->lang) // confident!
        {
            LL_INFOS("AppInit") << "Language: " << ll_safe_string(locale->lang) << LL_ENDL;
            LL_INFOS("AppInit") << "Location: " << ll_safe_string(locale->country) << LL_ENDL;
            LL_INFOS("AppInit") << "Variant: " << ll_safe_string(locale->variant) << LL_ENDL;
            LLControlVariable* c = gSavedSettings.getControl("SystemLanguage");
            if(c)
            {
                c->setValue(std::string(locale->lang), false);
            }
        }
    }
    FL_FreeLocale(&locale);

    return true;
}

bool LLAppViewerWin32::beingDebugged()
{
    return IsDebuggerPresent();
}

bool LLAppViewerWin32::restoreErrorTrap()
{
    return true; // we don't check for handler collisions on windows, so just say they're ok
}

//virtual
bool LLAppViewerWin32::sendURLToOtherInstance(const std::string& url)
{
    wchar_t window_class[256]; /* Flawfinder: ignore */   // Assume max length < 255 chars.
    mbstowcs(window_class, sWindowClass.c_str(), 255);
    window_class[255] = 0;
    // Use the class instead of the window name.
    HWND other_window = FindWindow(window_class, NULL);

    if (other_window != NULL)
    {
        LL_DEBUGS() << "Found other window with the name '" << getWindowTitle() << "'" << LL_ENDL;
        COPYDATASTRUCT cds;
        const S32 SLURL_MESSAGE_TYPE = 0;
        cds.dwData = SLURL_MESSAGE_TYPE;
        cds.cbData = static_cast<DWORD>(url.length()) + 1;
        cds.lpData = (void*)url.c_str();

        LRESULT msg_result = SendMessage(other_window, WM_COPYDATA, NULL, (LPARAM)&cds);
        LL_DEBUGS() << "SendMessage(WM_COPYDATA) to other window '"
                 << getWindowTitle() << "' returned " << msg_result << LL_ENDL;
        return true;
    }
    return false;
}


std::string LLAppViewerWin32::generateSerialNumber()
{
    char serial_md5[MD5HEX_STR_SIZE];       // Flawfinder: ignore
    serial_md5[0] = 0;

    DWORD serial = 0;
    DWORD flags = 0;
    BOOL success = GetVolumeInformation(
            L"C:\\",
            NULL,       // volume name buffer
            0,          // volume name buffer size
            &serial,    // volume serial
            NULL,       // max component length
            &flags,     // file system flags
            NULL,       // file system name buffer
            0);         // file system name buffer size
    if (success)
    {
        LLMD5 md5;
        md5.update( (unsigned char*)&serial, sizeof(DWORD));
        md5.finalize();
        md5.hex_digest(serial_md5);
    }
    else
    {
        LL_WARNS() << "GetVolumeInformation failed" << LL_ENDL;
    }
    return serial_md5;
}

// <FS:ND> Thread to purge old texture cache in the background.
// The cache dir will be search for directories named *.old_texturecache, then each of this directories
// will be deleted.
// The thread will be started each time the viewer starts, just in case there is directories so huge,
// the user quit the viewer before the old cache was fully cleared.
void deleteFilesInDirectory( std::wstring aDir )
{
    if( aDir == L"." || aDir == L".." || aDir.empty() )
        return;

    if( aDir[ aDir.size() -1 ] != '\\' || aDir[ aDir.size() -1 ] != '/' )
        aDir += L"\\";

    WIN32_FIND_DATA oFindData;
    HANDLE hFindHandle = ::FindFirstFile( (aDir + L"*.*").c_str(), &oFindData );

    if( INVALID_HANDLE_VALUE == hFindHandle )
        return;

    do
    {
        if( ! (oFindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) )
        {
            std::wstring strFile = aDir + oFindData.cFileName;
            if( oFindData.dwFileAttributes & FILE_ATTRIBUTE_READONLY )
                ::SetFileAttributes( strFile.c_str(), FILE_ATTRIBUTE_NORMAL );

            ::DeleteFile( ( aDir + oFindData.cFileName ).c_str() );
        }
    } while( ::FindNextFile( hFindHandle, &oFindData ) );

    ::FindClose( hFindHandle );
}

void deleteCacheDirectory( std::wstring aDir )
{
    if( aDir == L"." || aDir == L".." || aDir.empty() )
        return;

    if( aDir[ aDir.size() -1 ] != '\\' || aDir[ aDir.size() -1 ] != '/' )
        aDir += L"\\";

    wchar_t aCacheDirs[] = L"0123456789abcdef";

    for( int i = 0; i < _countof( aCacheDirs ); ++i )
    {
        deleteFilesInDirectory( aDir + aCacheDirs[i] );
        ::RemoveDirectory( (aDir + aCacheDirs[i]).c_str() );
    }

    deleteFilesInDirectory( aDir );
    ::RemoveDirectory( aDir.c_str() );
}

DWORD WINAPI purgeThread( LPVOID lpParameter )
{
    wchar_t *pDir = reinterpret_cast< wchar_t* >( lpParameter );
    if( !pDir )
        return 0;

    std::wstring strPath = pDir;
    free( pDir );

    if( strPath.empty() )
        return 0;

    if( strPath[ strPath.size() -1 ] != '\\' || strPath[ strPath.size() -1 ] != '/' )
        strPath += L"\\";

    WIN32_FIND_DATA oFindData;
    HANDLE hFindHandle = ::FindFirstFile( ( strPath + L"*.old_texturecache" ).c_str(), &oFindData );

    std::vector< std::wstring > vctDirs;

    if( INVALID_HANDLE_VALUE == hFindHandle )
        return 0;

    do
    {
        if( oFindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
            vctDirs.push_back( strPath + oFindData.cFileName );
    } while( ::FindNextFile( hFindHandle, &oFindData ) );

    ::FindClose( hFindHandle );

    for( auto dir : vctDirs )
    {
        LL_INFOS("LLDiskCache") << "Removing an old cache" << LL_ENDL; // <FS:Beq/> consistent tagging to help searching log files
        deleteCacheDirectory( dir );
    }

    return 0;
}

void LLAppViewerWin32::startCachePurge()
{
    if( isSecondInstance() )
        return;

    std::wstring strCacheDir = wstringize( gDirUtilp->getExpandedFilename( LL_PATH_CACHE, "" ) );

    HANDLE hThread = CreateThread( nullptr, 0, purgeThread, _wcsdup( strCacheDir.c_str() ), 0, nullptr );

    if( !hThread )
    {
        LL_WARNS("LLDiskCache") << "CreateThread failed: "  << GetLastError() << LL_ENDL; // <FS:Beq/> consistent tagging to help searching log files
    }
    else
        SetThreadPriority( hThread, THREAD_MODE_BACKGROUND_BEGIN );
}
// </FS:ND>
