/**
 * @file fsfloaterwhitelisthelper.cpp
 * @brief Helper tool implementation to display paths to whitelist in antivirus tools
 *
 * $LicenseInfo:firstyear=2024&license=fsviewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (C) 2024, The Phoenix Firestorm Project, Inc.
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
 * The Phoenix Firestorm Project, Inc., 1831 Oakwood Drive, Fairmont, Minnesota 56031-3225 USA
 * http://www.firestormviewer.org
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "fsfloaterwhitelisthelper.h"
#include "lltexteditor.h"
#include "llviewercontrol.h"
#include "lldir.h"


FSFloaterWhiteListHelper::FSFloaterWhiteListHelper(const LLSD& key) : LLFloater(key)
{
}

bool FSFloaterWhiteListHelper::postBuild()
{
    populateWhitelistInfo();
    return true;
}

void FSFloaterWhiteListHelper::populateWhitelistInfo()
{
// Hopefully we can trash this bit soon in favor of webRTC
#if LL_WINDOWS
    // On windows use exe (not work or RO) directory
    std::string voiceexe_path = gDirUtilp->getExecutableDir();
    gDirUtilp->append(voiceexe_path, "SLVoice.exe");
    std::string dullahan_path = gDirUtilp->getLLPluginDir();
    std::string dullahan_exe =  "dullahan_host.exe";
#elif LL_DARWIN
    // On MAC use resource directory
    std::string voiceexe_path = gDirUtilp->getAppRODataDir();
    gDirUtilp->append(voiceexe_path, "SLVoice");
    std::string dullahan_path = ""; // ignore dullahan on mac until we can identify it accurately
    std::string dullahan_exe = "";
#else
    std::string voiceexe_path = gDirUtilp->getExecutableDir();
    bool usingWine = gSavedSettings.getBOOL("FSLinuxEnableWin64VoiceProxy");
    if (!usingWine)
    {
        gDirUtilp->append(voiceexe_path, "SLVoice"); // native version
    }
    else
    {
        gDirUtilp->append(voiceexe_path, "win64/SLVoice.exe"); // use bundled win64 version
    }
    std::string dullahan_path = gDirUtilp->getExecutableDir(); // linux keeps dullahan in the bin folder
    std::string dullahan_exe = "dullahan_host";
#endif

    gDirUtilp->append(dullahan_path, dullahan_exe);

    const std::string slpluginexe_path = gDirUtilp->getLLPluginLauncher();

    std::string whitelist_folder_info = 
        gDirUtilp->getExecutableDir() + "\n" // Executable Dir
        + gDirUtilp->getOSUserAppDir() + "\n" // Top-level User Data Dir
        + gDirUtilp->getCacheDir(); // "Cache Dir

    std::string whitelist_exe_info = 
        gDirUtilp->getExecutableFilename() + "\n" // Viewer Binary
        + gDirUtilp->getExecutablePathAndName() + "\n" // Viewer Binary full path
        + gDirUtilp->getBaseFileName(voiceexe_path, false) + "\n" // " Voice Binary"
        + voiceexe_path + "\n" // slvoice full path
        + gDirUtilp->getBaseFileName(slpluginexe_path, false) + "\n" // SLPlugin Launcher Binary
        + slpluginexe_path + "\n" // SLPlugin Launcher full path
        + gDirUtilp->getBaseFileName(dullahan_path, false) + "\n" // SLPlugin Launcher Binary
        + dullahan_path + "\n"
        ;  

    getChild<LLTextEditor>("whitelist_folders_editor")->setText(whitelist_folder_info);
    getChild<LLTextEditor>("whitelist_exes_editor")->setText(whitelist_exe_info);
}
