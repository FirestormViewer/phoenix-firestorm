/**
 * @file linux_crash_logger.cpp
 * @brief Standalone SDL3 Linux crash report consent and submission
 *
 * $LicenseInfo:firstyear=2003&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 * Copyright (C) 2026, The Phoenix Firestorm Project, Inc.
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

// <FS:Beq> Only the separate process displays UI or uploads a saved crash report.
#include "linux_crash_report.h"
#include <SDL3/SDL_messagebox.h>
#include <SDL3/SDL_error.h>
#include <curl/curl.h>
#include <iostream>
#include <sstream>
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>

namespace
{
enum Choice { LATER = 0, SEND = 1, DECLINE = 2, DETAILS = 3 };

int message(const std::string& text, const SDL_MessageBoxButtonData* buttons, int count)
{
    const SDL_MessageBoxData box = {SDL_MESSAGEBOX_WARNING, nullptr,
        "Firestorm crash report — BugSplat", text.c_str(), count, buttons, nullptr};
    int choice = -1;
    if (!SDL_ShowMessageBox(&box, &choice))
    {
        std::cerr << "Cannot display crash report: " << SDL_GetError() << '\n';
        return -1;
    }
    // A window-manager close is never permission to send.
    return choice < 0 ? LATER : choice;
}

std::string details(const FSCrashReport::Report& report)
{
    std::ostringstream text;
    text << report.product << " " << report.version << "\nRecorded: " << report.timestamp;
    text << "\nName: " << (report.name.empty() ? "Not included" : report.name);
    text << "\n\nFiles to send:\n";
    for (const auto& attachment : report.attachments)
        if (attachment.included || attachment.required)
            text << "  " << attachment.file << " — " << attachment.label << '\n';
    text << "\nThe viewer log is not sent. Settings are included only when enabled in your crash-reporting preferences."
            "\nCrash dumps may contain personal information from memory."
            "\n\nReport files and submission log (bugsplat.log):\n" << report.directory.string()
         << "\n\nPrivacy policy: https://www.firestormviewer.org/privacy-policy";
    return text.str();
}

int review(FSCrashReport::Report& report)
{
    // Prevent a restart from opening a second dialog for a report already shown.
    const int lock = open((report.directory / "dialog.lock").c_str(), O_CREAT | O_RDWR | O_CLOEXEC, 0600);
    if (lock < 0 || flock(lock, LOCK_EX | LOCK_NB) != 0)
    {
        if (lock >= 0) close(lock);
        return 0;
    }
    struct Unlock { int fd; ~Unlock() { flock(fd, LOCK_UN); close(fd); } } unlock{lock};
    constexpr SDL_MessageBoxButtonData choices[] = {
        {SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT | SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, LATER, "Send later"},
        {0, DECLINE, "Don't send"}, {0, DETAILS, "Report details"}, {0, SEND, "Send report"}
    };
    constexpr SDL_MessageBoxButtonData back[] = {
        {SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT | SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, LATER, "Back"}
    };
    std::string prompt = "Firestorm closed unexpectedly. Send a crash report to Firestorm via BugSplat?\n\n"
                         "The report includes a crash dump and diagnostic information.\n"
                         "Your crash-reporting preferences control inclusion of your name and settings.\n"
                         "Choose Report details to see the files before sending.";
    for (;;)
    {
        switch (message(prompt, choices, 4))
        {
        case SEND:
        {
            std::string error;
            bool sent = false;
            try { sent = FSCrashReport::upload(report, error); }
            catch (const std::exception& failure)
            {
                error = failure.what();
                FSCrashReport::log(report, error + "; report retained for retry.");
            }
            if (sent)
            {
                message("Thank you. BugSplat accepted your crash report.", back, 1);
                return 0;
            }
            prompt = error + "\n\nYour report has been kept. Choose Send report to retry, or Send later.";
            break;
        }
        case DECLINE:
            report.status = "discarded";
            FSCrashReport::save(report);
            FSCrashReport::log(report, "User chose not to send this report.");
            return 0;
        case DETAILS:
            if (message(details(report), back, 1) < 0) return 2;
            break;
        case LATER:
            FSCrashReport::log(report, "Report retained for later; no submission requested.");
            return 0;
        default:
            FSCrashReport::log(report, "No graphical dialog available; report retained without submission.");
            return 2;
        }
    }
}
}

int main(int argc, char** argv)
{
    try
    {
        FSCrashReport::Report report;
        bool ask = true;
        if (argc == 3 && std::string(argv[1]) == "--report")
            report = FSCrashReport::load(argv[2]);
        else if (argc == 3 && std::string(argv[1]) == "--pending")
        {
            const auto reports = FSCrashReport::pending(argv[2]);
            if (reports.empty()) return 0;
            report = FSCrashReport::load(reports.front());
        }
        else if (argc == 5 && (std::string(argv[4]) == "ask" || std::string(argv[4]) == "send"))
        {
            report = FSCrashReport::capture(argv[1], argv[2], argv[3]);
            ask = std::string(argv[4]) == "ask";
        }
        else
        {
            std::cerr << "Usage: linux-crash-logger dump version database ask|send\n"
                         "       linux-crash-logger --report report.json\n"
                         "       linux-crash-logger --pending viewer-log-directory\n";
            return 1;
        }
        if (report.status != "pending") return 0;
        if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK)
            throw std::runtime_error("Cannot initialize crash uploader");
        struct CurlCleanup { ~CurlCleanup() { curl_global_cleanup(); } } cleanup;
        if (ask) return review(report);
        std::string error;
        const bool sent = FSCrashReport::upload(report, error);
        if (!sent) std::cerr << error << '\n';
        return sent ? 0 : 1;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Crash reporter: " << error.what() << '\n';
        return 1;
    }
}
// </FS:Beq>
