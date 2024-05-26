#include "fsfloaterwhitelisthelper.h"
#include "lltextbox.h"
#include "lltexteditor.h"
#include "lluictrlfactory.h"
#include "llviewercontrol.h"
#include "lldir.h"


FSFloaterWhiteListHelper::FSFloaterWhiteListHelper(const LLSD& key) : LLFloater(key)
{
}

FSFloaterWhiteListHelper::~FSFloaterWhiteListHelper()=default;

BOOL FSFloaterWhiteListHelper::postBuild()
{
    LLTextEditor* text_editor = getChild<LLTextEditor>("whitelist_text");
    if (text_editor)
    {
        populateWhitelistInfo();
    }
    return TRUE;
}

void FSFloaterWhiteListHelper::populateWhitelistInfo()
{
// Hopefully we can trash this bit soon in favor of webRTC
#if LL_WINDOWS
    // On windows use exe (not work or RO) directory
    std::string voiceexe_path = gDirUtilp->getExecutableDir();
    gDirUtilp->append(voiceexe_path, "SLVoice.exe");
#elif LL_DARWIN
    // On MAC use resource directory
    std::string voiceexe_path = gDirUtilp->getAppRODataDir();
    gDirUtilp->append(voiceexe_path, "SLVoice");
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
#endif

    const std::string& slpluginexe_path = gDirUtilp->getLLPluginLauncher();

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
        + slpluginexe_path + "\n"; // SLPlugin Launcher full path

    LLTextEditor* text_editor = getChild<LLTextEditor>("whitelist_folders_editor");
    if (text_editor)
    {
        text_editor->setText(whitelist_folder_info);
    }

    text_editor = getChild<LLTextEditor>("whitelist_exes_editor");
    if (text_editor)
    {
        text_editor->setText(whitelist_exe_info);
    }
}
