
/**
 * @file llfloaterpreference.cpp
 * @brief Global preferences with and without persistence.
 *
 * $LicenseInfo:firstyear=2002&license=viewerlgpl$
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

/*

 * App-wide preferences.  Note that these are not per-user,
 * because we need to load many preferences before we have
 * a login name.
 */

#include "llviewerprecompiledheaders.h"

#include "llfloaterpreference.h"

#include "message.h"
#include "llfloaterautoreplacesettings.h"
#include "llviewertexturelist.h"
#include "llagent.h"
#include "llagentcamera.h"
#include "llcheckboxctrl.h"
#include "llcolorswatch.h"
#include "llcombobox.h"
#include "llcommandhandler.h"
#include "lldirpicker.h"
#include "lleventtimer.h"
#include "llfeaturemanager.h"
#include "llfocusmgr.h"
//#include "llfirstuse.h"
#include "llfloaterreg.h"
#include "llfloaterabout.h"
#include "llfavoritesbar.h"
#include "llfloaterpreferencesgraphicsadvanced.h"
#include "llfloaterperformance.h"
#include "llfloatersidepanelcontainer.h"
// <FS:Ansariel> [FS communication UI]
//#include "llfloaterimsession.h"
#include "fsfloaterim.h"
#include "fsfloaternearbychat.h"
// </FS:Ansariel> [FS communication UI]
#include "llkeyboard.h"
#include "llmodaldialog.h"
#include "llnavigationbar.h"
#include "llfloaterimnearbychat.h"
#include "llnotifications.h"
#include "llnotificationsutil.h"
#include "llnotificationtemplate.h"
// <FS:Ansariel> [FS Login Panel]
//#include "llpanellogin.h"
#include "fspanellogin.h"
// </FS:Ansariel> [FS Login Panel]
#include "llpanelvoicedevicesettings.h"
#include "llradiogroup.h"
#include "llsearchcombobox.h"
#include "llsky.h"
#include "llscrolllistctrl.h"
#include "llscrolllistitem.h"
#include "llsliderctrl.h"
#include "lltabcontainer.h"
#include "lltrans.h"
#include "llviewercontrol.h"
#include "llviewercamera.h"
#include "llviewereventrecorder.h"
#include "llviewermessage.h"
#include "llviewerwindow.h"
#include "llviewerthrottle.h"
#include "llvoavatarself.h"
#include "llvotree.h"
#include "llfloaterpathfindingconsole.h"
// linden library includes
#include "llavatarnamecache.h"
#include "llerror.h"
#include "llfontgl.h"
#include "llrect.h"
#include "llstring.h"

// project includes

#include "llbutton.h"
#include "llflexibleobject.h"
#include "lllineeditor.h"
#include "llresmgr.h"
#include "llspinctrl.h"
#include "llstartup.h"
#include "lltextbox.h"
#include "llui.h"
#include "llviewerobjectlist.h"
#include "llvovolume.h"
#include "llwindow.h"
#include "llworld.h"
#include "lluictrlfactory.h"
#include "llviewermedia.h"
#include "llpluginclassmedia.h"
#include "llteleporthistorystorage.h"
#include "llproxy.h"
#include "llweb.h"
// [RLVa:KB] - Checked: 2010-03-18 (RLVa-1.2.0a)
#include "rlvactions.h"
// [/RLVa:KB]

#include "lllogininstance.h"        // to check if logged in yet
#include "llsdserialize.h"
#include "llpresetsmanager.h"
#include "llviewercontrol.h"
#include "llpresetsmanager.h"
#include "llinventoryfunctions.h"

#include "llsearchableui.h"
#include "llperfstats.h"

// Firestorm Includes
#include "exogroupmutelist.h"
#include "fsavatarrenderpersistence.h"
#include "fsdroptarget.h"
#include "fsfloaterimcontainer.h"
#include "growlmanager.h"
#include "lfsimfeaturehandler.h"
#include "llaudioengine.h" // <FS:Ansariel> Output device selection
#include "llavatarname.h"   // <FS:CR> Deeper name cache stuffs
#include "llclipboard.h"    // <FS:Zi> Support preferences search SLURLs
#include "lldiriterator.h"  // <Kadah> for populating the fonts combo
#include "lleventtimer.h"
#include "llline.h"
#include "lllocationhistory.h"
#include "llpanelblockedlist.h"
#include "llpanelmaininventory.h"
#include "llscrolllistctrl.h"
#include "llsdserialize.h" // KB: SkinsSelector
#include "llspellcheck.h"
#include "lltoolbarview.h"
#include "llviewermenufile.h" // <FS:LO> FIRE-23606 Reveal path to external script editor in prefernces
#include "llviewernetwork.h" // <FS:AW  opensim search support>
#include "llviewershadermgr.h"
#include "NACLantispam.h"
#include "../llcrashlogger/llcrashlogger.h"
#include <filesystem>
#if LL_WINDOWS
#include <VersionHelpers.h>
#endif

// <FS:LO> FIRE-23606 Reveal path to external script editor in prefernces
#if LL_DARWIN
#include <CoreFoundation/CFURL.h>
#include <CoreFoundation/CFBundle.h>    // [FS:CR]
#endif
// </FS:LO>

// <FS:Zi> FIRE-19539 - Include the alert messages in Prefs>Notifications>Alerts in preference Search.
#include "llfiltereditor.h"
#include "llviewershadermgr.h"
//<FS:HG> FIRE-6340, FIRE-6567 - Setting Bandwidth issues
//const F32 BANDWIDTH_UPDATER_TIMEOUT = 0.5f;
char const* const VISIBILITY_DEFAULT = "default";
char const* const VISIBILITY_HIDDEN = "hidden";

//control value for middle mouse as talk2push button
//const static std::string MIDDLE_MOUSE_CV = "MiddleMouse"; // for voice client and redability
//const static std::string MOUSE_BUTTON_4_CV = "MouseButton4";
//const static std::string MOUSE_BUTTON_5_CV = "MouseButton5";

/// This must equal the maximum value set for the IndirectMaxComplexity slider in panel_preferences_graphics1.xml
static const U32 INDIRECT_MAX_ARC_OFF = 101; // all the way to the right == disabled
static const U32 MIN_INDIRECT_ARC_LIMIT = 1; // must match minimum of IndirectMaxComplexity in panel_preferences_graphics1.xml
static const U32 MAX_INDIRECT_ARC_LIMIT = INDIRECT_MAX_ARC_OFF-1; // one short of all the way to the right...

/// These are the effective range of values for RenderAvatarMaxComplexity
static const F32 MIN_ARC_LIMIT =  20000.0f;
static const F32 MAX_ARC_LIMIT = 350000.0f;
static const F32 MIN_ARC_LOG = log(MIN_ARC_LIMIT);
static const F32 MAX_ARC_LOG = log(MAX_ARC_LIMIT);
static const F32 ARC_LIMIT_MAP_SCALE = (MAX_ARC_LOG - MIN_ARC_LOG) / (MAX_INDIRECT_ARC_LIMIT - MIN_INDIRECT_ARC_LIMIT);

// <FS:Zi> FIRE-19539 - Include the alert messages in Prefs>Notifications>Alerts in preference Search.
// define these constants so any future changes will be easier and less error prone
static const S32 COLUMN_POPUP_SPACER = 0;
static const S32 COLUMN_POPUP_CHECKBOX = 1;
static const S32 COLUMN_POPUP_LABEL = 2;
// </FS:Zi>

struct LabelDef : public LLInitParam::Block<LabelDef>
{
    Mandatory<std::string> name;
    Mandatory<std::string> value;

    LabelDef()
        : name("name"),
        value("value")
    {}
};

struct LabelTable : public LLInitParam::Block<LabelTable>
{
    Multiple<LabelDef> labels;
    LabelTable()
        : labels("label")
    {}
};


// global functions

// helper functions for getting/freeing the web browser media
// if creating/destroying these is too slow, we'll need to create
// a static member and update all our static callbacks

void handleNameTagOptionChanged(const LLSD& newvalue);
void handleDisplayNamesOptionChanged(const LLSD& newvalue);
bool callback_clear_browser_cache(const LLSD& notification, const LLSD& response);
bool callback_clear_cache(const LLSD& notification, const LLSD& response);

// <Firestorm>
bool callback_clear_inventory_cache(const LLSD& notification, const LLSD& response);
void handleFlightAssistOptionChanged(const LLSD& newvalue);
void handleMovelockOptionChanged(const LLSD& newvalue);
void handleMovelockAfterMoveOptionChanged(const LLSD& newvalue);
bool callback_clear_settings(const LLSD& notification, const LLSD& response);
// <FS:AW  opensim search support>
bool callback_clear_debug_search(const LLSD& notification, const LLSD& response);
bool callback_pick_debug_search(const LLSD& notification, const LLSD& response);
// </FS:AW  opensim search support>

// <FS:LO> FIRE-7050 - Add a warning to the Growl preference option because of FIRE-6868
#ifdef LL_WINDOWS
bool callback_growl_not_installed(const LLSD& notification, const LLSD& response);
#endif
// </FS:LO>
// </Firestorm>

//bool callback_skip_dialogs(const LLSD& notification, const LLSD& response, LLFloaterPreference* floater);
//bool callback_reset_dialogs(const LLSD& notification, const LLSD& response, LLFloaterPreference* floater);

void fractionFromDecimal(F32 decimal_val, S32& numerator, S32& denominator);

// <FS:Ansariel> Clear inventory cache button
bool callback_clear_inventory_cache(const LLSD& notification, const LLSD& response)
{
    S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
    if ( option == 0 ) // YES
    {
        // flag client texture cache for clearing next time the client runs

        // use a marker file instead of a settings variable to prevent logout crashes and
        // dual log ins from messing with the flag. -Zi
        std::string delete_cache_marker = gDirUtilp->getExpandedFilename(LL_PATH_CACHE, gAgentID.asString() + "_DELETE_INV_GZ");
        FILE* fd = LLFile::fopen(delete_cache_marker, "w");
        LLFile::close(fd);
        LLNotificationsUtil::add("CacheWillClear");
    }

    return false;
}
// </FS:Ansariel>

// <FS:Ansariel> Clear inventory cache button
bool callback_clear_web_browser_cache(const LLSD& notification, const LLSD& response)
{
    S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
    if ( option == 0 ) // YES
    {
        gSavedSettings.setBOOL("FSStartupClearBrowserCache", true);
    }

    return false;
}
// </FS:Ansariel>

bool callback_clear_cache(const LLSD& notification, const LLSD& response)
{
    S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
    if ( option == 0 ) // YES
    {
        // flag client texture cache for clearing next time the client runs
        gSavedSettings.setBOOL("PurgeCacheOnNextStartup", true);
        LLNotificationsUtil::add("CacheWillClear");
    }

    return false;
}

bool callback_clear_browser_cache(const LLSD& notification, const LLSD& response)
{
    S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
    if ( option == 0 ) // YES
    {
        // clean web
        LLViewerMedia::getInstance()->clearAllCaches();
        LLViewerMedia::getInstance()->clearAllCookies();

        // clean nav bar history
        LLNavigationBar::getInstance()->clearHistoryCache();

        // flag client texture cache for clearing next time the client runs
        // <FS:AO> Don't clear main texture cache on browser cache clear - it's too expensive to be done except explicitly
        //gSavedSettings.setBOOL("PurgeCacheOnNextStartup", true);
        //LLNotificationsUtil::add("CacheWillClear");

        LLSearchHistory::getInstance()->clearHistory();
        LLSearchHistory::getInstance()->save();
        // <FS:Zi> Make navigation bar part of the UI
        // LLSearchComboBox* search_ctrl = LLNavigationBar::getInstance()->getChild<LLSearchComboBox>("search_combo_box");
        // search_ctrl->clearHistory();
        LLNavigationBar::instance().clearHistory();
        // </FS:Zi>

        // <FS:Ansariel> FIRE-29761: Clear Location History does not clear Typed Locations history
        LLLocationHistory::getInstance()->removeItems();
        LLLocationHistory::getInstance()->save();
        // </FS:Ansariel>

        LLTeleportHistoryStorage::getInstance()->purgeItems();
        LLTeleportHistoryStorage::getInstance()->save();
    }

    return false;
}

void handleNameTagOptionChanged(const LLSD& newvalue)
{
    LLAvatarNameCache::getInstance()->setUseUsernames(gSavedSettings.getBOOL("NameTagShowUsernames"));
    LLVOAvatar::invalidateNameTags();
}

void handleDisplayNamesOptionChanged(const LLSD& newvalue)
{
    LLAvatarNameCache::getInstance()->setUseDisplayNames(newvalue.asBoolean());
    LLVOAvatar::invalidateNameTags();
}

void handleAppearanceCameraMovementChanged(const LLSD& newvalue)
{
    if(!newvalue.asBoolean() && gAgentCamera.getCameraMode() == CAMERA_MODE_CUSTOMIZE_AVATAR)
    {
        gAgentCamera.changeCameraToDefault();
        gAgentCamera.resetView();
    }
}

// <FS:AW  opensim search support>
bool callback_clear_debug_search(const LLSD& notification, const LLSD& response)
{
    S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
    if ( option == 0 ) // YES
    {
        gSavedSettings.setString("SearchURLDebug","");
    }

    return false;
}

bool callback_pick_debug_search(const LLSD& notification, const LLSD& response)
{
    S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
    if ( option == 0 ) // YES
    {
        std::string url;

        if (LFSimFeatureHandler::instanceExists())
        {
            url = LFSimFeatureHandler::instance().searchURL();
        }
        else
        {
#ifdef OPENSIM // <FS:AW optional opensim support>
            if (LLGridManager::getInstance()->isInOpenSim())
            {
                url = LLLoginInstance::getInstance()->hasResponse("search")
                    ? LLLoginInstance::getInstance()->getResponse("search").asString()
                    : gSavedSettings.getString("SearchURLOpenSim");
            }
            else // we are in SL or SL beta
#endif // OPENSIM // <FS:AW optional opensim support>
            {
                //not in OpenSim means we are in SL or SL beta
                url = gSavedSettings.getString("SearchURL");
            }
        }

        gSavedSettings.setString("SearchURLDebug", url);
    }

    return false;
}
// </FS:AW  opensim search support>

void fractionFromDecimal(F32 decimal_val, S32& numerator, S32& denominator)
{
    numerator = 0;
    denominator = 0;
    for (F32 test_denominator = 1.f; test_denominator < 30.f; test_denominator += 1.f)
    {
        if (fmodf((decimal_val * test_denominator) + 0.01f, 1.f) < 0.02f)
        {
            numerator = ll_round(decimal_val * test_denominator);
            denominator = ll_round(test_denominator);
            break;
        }
    }
}

// handle secondlife:///app/worldmap/{NAME}/{COORDS} URLs
// Also see LLUrlEntryKeybinding, the value of this command type
// is ability to show up to date value in chat
class LLKeybindingHandler: public LLCommandHandler
{
public:
    // requires trusted browser to trigger
    LLKeybindingHandler(): LLCommandHandler("keybinding", UNTRUSTED_CLICK_ONLY)
    {
    }

    bool handle(const LLSD& params, const LLSD& query_map,
                const std::string& grid, LLMediaCtrl* web)
    {
        if (params.size() < 1) return false;

        LLFloaterPreference* prefsfloater = dynamic_cast<LLFloaterPreference*>
            (LLFloaterReg::showInstance("preferences"));

        if (prefsfloater)
        {
            // find 'controls' panel and bring it the front
            LLTabContainer* tabcontainer = prefsfloater->getChild<LLTabContainer>("pref core");
            LLPanel* panel = prefsfloater->getChild<LLPanel>("controls");
            if (tabcontainer && panel)
            {
                tabcontainer->selectTabPanel(panel);
            }
        }

        return true;
    }
};
LLKeybindingHandler gKeybindHandler;


//////////////////////////////////////////////
// LLFloaterPreference

// static
std::string LLFloaterPreference::sSkin = "";

LLFloaterPreference::LLFloaterPreference(const LLSD& key)
    : LLFloater(key),
    mGotPersonalInfo(false),
    // <FS:Ansariel> Keep this for OpenSim
    mOriginalIMViaEmail(false),
    mLanguageChanged(false),
    mAvatarDataInitialized(false),
    mSearchDataDirty(true)
{
    LLConversationLog::instance().addObserver(this);

    //Build Floater is now Called from  LLFloaterReg::add("preferences", "floater_preferences.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLFloaterPreference>);

    static bool registered_dialog = false;
    if (!registered_dialog)
    {
        LLFloaterReg::add("keybind_dialog", "floater_select_key.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLSetKeyBindDialog>);
        registered_dialog = true;
    }

    mCommitCallbackRegistrar.add("Pref.Cancel",             boost::bind(&LLFloaterPreference::onBtnCancel, this, _2));
    mCommitCallbackRegistrar.add("Pref.OK",                 boost::bind(&LLFloaterPreference::onBtnOK, this, _2));

    mCommitCallbackRegistrar.add("Pref.ClearCache",             boost::bind(&LLFloaterPreference::onClickClearCache, this));
    mCommitCallbackRegistrar.add("Pref.WebClearCache",          boost::bind(&LLFloaterPreference::onClickBrowserClearCache, this));
    // <FS:Ansariel> Clear inventory cache button
    mCommitCallbackRegistrar.add("Pref.InvClearCache",          boost::bind(&LLFloaterPreference::onClickInventoryClearCache, this));
    // </FS:Ansariel>
    // <FS:Ansariel> Clear web browser cache button
    mCommitCallbackRegistrar.add("Pref.WebBrowserClearCache",       boost::bind(&LLFloaterPreference::onClickWebBrowserClearCache, this));
    // </FS:Ansariel>
    mCommitCallbackRegistrar.add("Pref.SetCache",               boost::bind(&LLFloaterPreference::onClickSetCache, this));
    mCommitCallbackRegistrar.add("Pref.ResetCache",             boost::bind(&LLFloaterPreference::onClickResetCache, this));
//  mCommitCallbackRegistrar.add("Pref.ClickSkin",              boost::bind(&LLFloaterPreference::onClickSkin, this,_1, _2));
//  mCommitCallbackRegistrar.add("Pref.SelectSkin",             boost::bind(&LLFloaterPreference::onSelectSkin, this));
    //<FS:KC> Handled centrally now
//  mCommitCallbackRegistrar.add("Pref.SetSounds",              boost::bind(&LLFloaterPreference::onClickSetSounds, this));
    // <FS:Zi> FIRE-19539 - Include the alert messages in Prefs>Notifications>Alerts in preference Search.
    // mCommitCallbackRegistrar.add("Pref.ClickEnablePopup",        boost::bind(&LLFloaterPreference::onClickEnablePopup, this));
    // mCommitCallbackRegistrar.add("Pref.ClickDisablePopup",       boost::bind(&LLFloaterPreference::onClickDisablePopup, this));
    mCommitCallbackRegistrar.add("Pref.SelectPopup",            boost::bind(&LLFloaterPreference::onSelectPopup, this));
    mCommitCallbackRegistrar.add("Pref.UpdatePopupFilter",      boost::bind(&LLFloaterPreference::onUpdatePopupFilter, this));
    // </FS:Zi>
    mCommitCallbackRegistrar.add("Pref.LogPath",                boost::bind(&LLFloaterPreference::onClickLogPath, this));
    mCommitCallbackRegistrar.add("Pref.RenderExceptions",       boost::bind(&LLFloaterPreference::onClickRenderExceptions, this));
    // mCommitCallbackRegistrar.add("Pref.AutoAdjustments",         boost::bind(&LLFloaterPreference::onClickAutoAdjustments, this)); // <FS:Beq/> Not required in FS at present
    mCommitCallbackRegistrar.add("Pref.HardwareDefaults",       boost::bind(&LLFloaterPreference::setHardwareDefaults, this));
    mCommitCallbackRegistrar.add("Pref.AvatarImpostorsEnable",  boost::bind(&LLFloaterPreference::onAvatarImpostorsEnable, this));
    mCommitCallbackRegistrar.add("Pref.UpdateIndirectMaxComplexity",    boost::bind(&LLFloaterPreference::updateMaxComplexity, this));
    mCommitCallbackRegistrar.add("Pref.RenderOptionUpdate",     boost::bind(&LLFloaterPreference::onRenderOptionEnable, this));
    mCommitCallbackRegistrar.add("Pref.LocalLightsEnable",      boost::bind(&LLFloaterPreference::onLocalLightsEnable, this));
    mCommitCallbackRegistrar.add("Pref.WindowedMod",            boost::bind(&LLFloaterPreference::onCommitWindowedMode, this));
    mCommitCallbackRegistrar.add("Pref.UpdateSliderText",       boost::bind(&LLFloaterPreference::refreshUI,this));
    mCommitCallbackRegistrar.add("Pref.QualityPerformance",     boost::bind(&LLFloaterPreference::onChangeQuality, this, _2));
    mCommitCallbackRegistrar.add("Pref.applyUIColor",           boost::bind(&LLFloaterPreference::applyUIColor, this ,_1, _2));
    mCommitCallbackRegistrar.add("Pref.getUIColor",             boost::bind(&LLFloaterPreference::getUIColor, this ,_1, _2));
    mCommitCallbackRegistrar.add("Pref.MaturitySettings",       boost::bind(&LLFloaterPreference::onChangeMaturity, this));
    mCommitCallbackRegistrar.add("Pref.BlockList",              boost::bind(&LLFloaterPreference::onClickBlockList, this));
    mCommitCallbackRegistrar.add("Pref.Proxy",                  boost::bind(&LLFloaterPreference::onClickProxySettings, this));
    mCommitCallbackRegistrar.add("Pref.TranslationSettings",    boost::bind(&LLFloaterPreference::onClickTranslationSettings, this));
    mCommitCallbackRegistrar.add("Pref.AutoReplace",            boost::bind(&LLFloaterPreference::onClickAutoReplace, this));
    mCommitCallbackRegistrar.add("Pref.PermsDefault",           boost::bind(&LLFloaterPreference::onClickPermsDefault, this));
    mCommitCallbackRegistrar.add("Pref.RememberedUsernames",    boost::bind(&LLFloaterPreference::onClickRememberedUsernames, this));
    mCommitCallbackRegistrar.add("Pref.SpellChecker",           boost::bind(&LLFloaterPreference::onClickSpellChecker, this));
    mCommitCallbackRegistrar.add("Pref.Advanced",               boost::bind(&LLFloaterPreference::onClickAdvanced, this));

    // <FS:Ansariel> Improved graphics preferences
    mCommitCallbackRegistrar.add("Pref.UpdateIndirectMaxNonImpostors", boost::bind(&LLFloaterPreference::updateMaxNonImpostors, this));
    // </FS:Ansariel>

    // <FS:Zi> Support preferences search SLURLs
    mCommitCallbackRegistrar.add("Pref.CopySearchAsSLURL",      boost::bind(&LLFloaterPreference::onCopySearch, this));
    // </FS_ZI>

    sSkin = gSavedSettings.getString("SkinCurrent");

    mCommitCallbackRegistrar.add("Pref.ClickActionChange",      boost::bind(&LLFloaterPreference::onClickActionChange, this));

    gSavedSettings.getControl("NameTagShowUsernames")->getCommitSignal()->connect(boost::bind(&handleNameTagOptionChanged,  _2));
    gSavedSettings.getControl("NameTagShowFriends")->getCommitSignal()->connect(boost::bind(&handleNameTagOptionChanged,  _2));
    gSavedSettings.getControl("UseDisplayNames")->getCommitSignal()->connect(boost::bind(&handleDisplayNamesOptionChanged,  _2));

    gSavedSettings.getControl("AppearanceCameraMovement")->getCommitSignal()->connect(boost::bind(&handleAppearanceCameraMovementChanged,  _2));
    gSavedSettings.getControl("WindLightUseAtmosShaders")->getCommitSignal()->connect(boost::bind(&LLFloaterPreference::onAtmosShaderChange, this));

    LLAvatarPropertiesProcessor::getInstance()->addObserver( gAgent.getID(), this );

    mComplexityChangedSignal = gSavedSettings.getControl("RenderAvatarMaxComplexity")->getCommitSignal()->connect(boost::bind(&LLFloaterPreference::updateComplexityText, this));

    mCommitCallbackRegistrar.add("Pref.ClearLog",               boost::bind(&LLConversationLog::onClearLog, &LLConversationLog::instance()));
    mCommitCallbackRegistrar.add("Pref.DeleteTranscripts",      boost::bind(&LLFloaterPreference::onDeleteTranscripts, this));
    mCommitCallbackRegistrar.add("UpdateFilter", boost::bind(&LLFloaterPreference::onUpdateFilterTerm, this, false)); // <FS:ND/> Hook up for filtering

    // <Firestorm Callbacks>
    mCommitCallbackRegistrar.add("NACL.AntiSpamUnblock",        boost::bind(&LLFloaterPreference::onClickClearSpamList, this));
    mCommitCallbackRegistrar.add("NACL.SetPreprocInclude",      boost::bind(&LLFloaterPreference::setPreprocInclude, this));
    // <FS:LO> FIRE-23606 Reveal path to external script editor in prefernces
    mCommitCallbackRegistrar.add("Pref.SetExternalEditor",      boost::bind(&LLFloaterPreference::setExternalEditor, this));
    //[ADD - Clear Settings : SJ]
    mCommitCallbackRegistrar.add("Pref.ClearSettings",          boost::bind(&LLFloaterPreference::onClickClearSettings, this));
    mCommitCallbackRegistrar.add("Pref.Online_Notices",         boost::bind(&LLFloaterPreference::onClickChatOnlineNotices, this));
    // <FS:PP> FIRE-8190: Preview function for "UI Sounds" Panel
    mCommitCallbackRegistrar.add("PreviewUISound",              boost::bind(&LLFloaterPreference::onClickPreviewUISound, this, _2));
    mCommitCallbackRegistrar.add("Pref.BrowseCache",            boost::bind(&LLFloaterPreference::onClickBrowseCache, this));
    mCommitCallbackRegistrar.add("Pref.BrowseCrashLogs",        boost::bind(&LLFloaterPreference::onClickBrowseCrashLogs, this));
    mCommitCallbackRegistrar.add("Pref.BrowseSettingsDir",      boost::bind(&LLFloaterPreference::onClickBrowseSettingsDir, this));
    mCommitCallbackRegistrar.add("Pref.BrowseLogPath",          boost::bind(&LLFloaterPreference::onClickBrowseChatLogDir, this));
    mCommitCallbackRegistrar.add("Pref.Javascript",             boost::bind(&LLFloaterPreference::onClickJavascript, this));
    //[FIX FIRE-2765 : SJ] Making sure Reset button resets works
    mCommitCallbackRegistrar.add("Pref.ResetLogPath",           boost::bind(&LLFloaterPreference::onClickResetLogPath, this));
    // <FS:CR>
    gSavedSettings.getControl("FSColorUsername")->getCommitSignal()->connect(boost::bind(&handleNameTagOptionChanged, _2));
    gSavedSettings.getControl("FSUseLegacyClienttags")->getCommitSignal()->connect(boost::bind(&handleNameTagOptionChanged, _2));
    gSavedSettings.getControl("FSClientTagsVisibility")->getCommitSignal()->connect(boost::bind(&handleNameTagOptionChanged, _2));
    gSavedSettings.getControl("FSColorClienttags")->getCommitSignal()->connect(boost::bind(&handleNameTagOptionChanged, _2));
    // </FS:CR>

    // <FS:Ansariel> Sound cache
    mCommitCallbackRegistrar.add("Pref.BrowseSoundCache",               boost::bind(&LLFloaterPreference::onClickBrowseSoundCache, this));
    mCommitCallbackRegistrar.add("Pref.SetSoundCache",                  boost::bind(&LLFloaterPreference::onClickSetSoundCache, this));
    mCommitCallbackRegistrar.add("Pref.ResetSoundCache",                boost::bind(&LLFloaterPreference::onClickResetSoundCache, this));
    // </FS:Ansariel>

    // <FS:Ansariel> FIRE-2912: Reset voice button
    mCommitCallbackRegistrar.add("Pref.ResetVoice",                     boost::bind(&LLFloaterPreference::onClickResetVoice, this));
}

void LLFloaterPreference::processProperties( void* pData, EAvatarProcessorType type )
{
    if ( APT_PROPERTIES_LEGACY == type )
    {
        const LLAvatarLegacyData* pAvatarData = static_cast<const LLAvatarLegacyData*>( pData );
        if (pAvatarData && (gAgent.getID() == pAvatarData->avatar_id) && (pAvatarData->avatar_id != LLUUID::null))
        {
            mAllowPublish = (bool)(pAvatarData->flags & AVATAR_ALLOW_PUBLISH);
            mAvatarDataInitialized = true;
            getChild<LLUICtrl>("online_searchresults")->setValue(mAllowPublish);
        }
    }
}

void LLFloaterPreference::saveAvatarProperties( void )
{
    const bool allowPublish = getChild<LLUICtrl>("online_searchresults")->getValue();

    if ((LLStartUp::getStartupState() == STATE_STARTED)
        && mAvatarDataInitialized
        && (allowPublish != mAllowPublish))
    {
        std::string cap_url = gAgent.getRegionCapability("AgentProfile");
        if (!cap_url.empty())
        {
            mAllowPublish = allowPublish;

            LLCoros::instance().launch("requestAgentUserInfoCoro",
                boost::bind(saveAvatarPropertiesCoro, cap_url, allowPublish));
        }
    }
}

void LLFloaterPreference::saveAvatarPropertiesCoro(const std::string cap_url, bool allow_publish)
{
    LLCore::HttpRequest::policy_t httpPolicy(LLCore::HttpRequest::DEFAULT_POLICY_ID);
    LLCoreHttpUtil::HttpCoroutineAdapter::ptr_t
        httpAdapter(new LLCoreHttpUtil::HttpCoroutineAdapter("put_avatar_properties_coro", httpPolicy));
    LLCore::HttpRequest::ptr_t httpRequest(new LLCore::HttpRequest);
    LLCore::HttpHeaders::ptr_t httpHeaders;

    LLCore::HttpOptions::ptr_t httpOpts(new LLCore::HttpOptions);
    httpOpts->setFollowRedirects(true);

    std::string finalUrl = cap_url + "/" + gAgentID.asString();
    LLSD data;
    data["allow_publish"] = allow_publish;

    LLSD result = httpAdapter->putAndSuspend(httpRequest, finalUrl, data, httpOpts, httpHeaders);

    LLSD httpResults = result[LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS];
    LLCore::HttpStatus status = LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(httpResults);

    if (!status)
    {
        LL_WARNS("Preferences") << "Failed to put agent information " << data << " for id " << gAgentID << LL_ENDL;
        return;
    }

    LL_DEBUGS("Preferences") << "Agent id: " << gAgentID << " Data: " << data << " Result: " << httpResults << LL_ENDL;
}

bool LLFloaterPreference::postBuild()
{
    mDeleteTranscriptsBtn = getChild<LLButton>("delete_transcripts");

    // <FS:Ansariel> We don't have these buttons
    //mEnabledPopups  = getChild<LLScrollListCtrl>("enabled_popups");
    //mDisabledPopups = getChild<LLScrollListCtrl>("disabled_popups");
    //mEnablePopupBtn = getChild<LLButton>("enable_this_popup");
    //mDisablePopupBtn = getChild<LLButton>("disable_this_popup");

    // <FS:Ansariel> [FS communication UI]
    //gSavedSettings.getControl("ChatFontSize")->getSignal()->connect(boost::bind(&LLFloaterIMSessionTab::processChatHistoryStyleUpdate, false));

    //gSavedSettings.getControl("ChatFontSize")->getSignal()->connect(boost::bind(&LLViewerChat::signalChatFontChanged));
    // </FS:Ansariel> [FS communication UI]

    gSavedSettings.getControl("ChatBubbleOpacity")->getSignal()->connect(boost::bind(&LLFloaterPreference::onNameTagOpacityChange, this, _2));
    gSavedSettings.getControl("ConsoleBackgroundOpacity")->getSignal()->connect(boost::bind(&LLFloaterPreference::onConsoleOpacityChange, this, _2));   // <FS:CR> FIRE-1332 - Sepeate opacity settings for nametag and console chat

    gSavedSettings.getControl("PreferredMaturity")->getSignal()->connect(boost::bind(&LLFloaterPreference::onChangeMaturity, this));

    gSavedSettings.getControl("RenderAvatarComplexityMode")->getSignal()->connect(
        [this](LLControlVariable* control, const LLSD& new_val, const LLSD& old_val)
        {
            onChangeComplexityMode(new_val);
        });

    gSavedPerAccountSettings.getControl("ModelUploadFolder")->getSignal()->connect(boost::bind(&LLFloaterPreference::onChangeModelFolder, this));
    gSavedPerAccountSettings.getControl("PBRUploadFolder")->getSignal()->connect(boost::bind(&LLFloaterPreference::onChangePBRFolder, this));
    gSavedPerAccountSettings.getControl("TextureUploadFolder")->getSignal()->connect(boost::bind(&LLFloaterPreference::onChangeTextureFolder, this));
    gSavedPerAccountSettings.getControl("SoundUploadFolder")->getSignal()->connect(boost::bind(&LLFloaterPreference::onChangeSoundFolder, this));
    gSavedPerAccountSettings.getControl("AnimationUploadFolder")->getSignal()->connect(boost::bind(&LLFloaterPreference::onChangeAnimationFolder, this));

    LLTabContainer* tabcontainer = getChild<LLTabContainer>("pref core");
    if (!tabcontainer->selectTab(gSavedSettings.getS32("LastPrefTab")))
        tabcontainer->selectFirstTab();

    getChild<LLUICtrl>("cache_location")->setEnabled(false); // make it read-only but selectable (STORM-227)
    // getChildView("log_path_string")->setEnabled(false);// do the same for chat logs path - <FS:PP> Field removed from Privacy tab, we have it already in Network & Files tab along with few fancy buttons (03 Mar 2015)
    getChildView("log_path_string-panelsetup")->setEnabled(false);// and the redundant instance -WoLf
    std::string cache_location = gDirUtilp->getExpandedFilename(LL_PATH_CACHE, "");
    setCacheLocation(cache_location);
    // <FS:Ansariel> Sound cache
    setSoundCacheLocation(gSavedSettings.getString("FSSoundCacheLocation"));
    getChild<LLUICtrl>("FSSoundCacheLocation")->setEnabled(false);
    // </FS:Ansariel>

    getChild<LLComboBox>("language_combobox")->setCommitCallback(boost::bind(&LLFloaterPreference::onLanguageChange, this));

    // <FS:CR> [CHUI MERGE]
    // We don't use these in FS Communications UI, should we in the future? Disabling for now.
    //getChild<LLComboBox>("FriendIMOptions")->setCommitCallback(boost::bind(&LLFloaterPreference::onNotificationsChange, this,"FriendIMOptions"));
    //getChild<LLComboBox>("NonFriendIMOptions")->setCommitCallback(boost::bind(&LLFloaterPreference::onNotificationsChange, this,"NonFriendIMOptions"));
    //getChild<LLComboBox>("ConferenceIMOptions")->setCommitCallback(boost::bind(&LLFloaterPreference::onNotificationsChange, this,"ConferenceIMOptions"));
    //getChild<LLComboBox>("GroupChatOptions")->setCommitCallback(boost::bind(&LLFloaterPreference::onNotificationsChange, this,"GroupChatOptions"));
    //getChild<LLComboBox>("NearbyChatOptions")->setCommitCallback(boost::bind(&LLFloaterPreference::onNotificationsChange, this,"NearbyChatOptions"));
    //getChild<LLComboBox>("ObjectIMOptions")->setCommitCallback(boost::bind(&LLFloaterPreference::onNotificationsChange, this,"ObjectIMOptions"));
    // </FS:CR>

    // if floater is opened before login set default localized do not disturb message
    if (LLStartUp::getStartupState() < STATE_STARTED)
    {
        gSavedPerAccountSettings.setString("DoNotDisturbModeResponse", LLTrans::getString("DoNotDisturbModeResponseDefault"));
        // <FS:Ansariel> FIRE-5436: Unlocalizable auto-response messages
        gSavedPerAccountSettings.setString("FSAutorespondModeResponse", LLTrans::getString("AutoResponseModeDefault"));
        gSavedPerAccountSettings.setString("FSAutorespondNonFriendsResponse", LLTrans::getString("AutoResponseModeNonFriendsDefault"));
        gSavedPerAccountSettings.setString("FSRejectTeleportOffersResponse", LLTrans::getString("RejectTeleportOffersResponseDefault"));
        gSavedPerAccountSettings.setString("FSRejectFriendshipRequestsResponse", LLTrans::getString("RejectFriendshipRequestsResponseDefault"));
        gSavedPerAccountSettings.setString("FSMutedAvatarResponse", LLTrans::getString("MutedAvatarsResponseDefault"));
        gSavedPerAccountSettings.setString("FSAwayAvatarResponse", LLTrans::getString("AwayAvatarResponseDefault"));
        // </FS:Ansariel>
    }

    // set 'enable' property for 'Clear log...' button
    changed();

    LLLogChat::getInstance()->setSaveHistorySignal(boost::bind(&LLFloaterPreference::onLogChatHistorySaved, this));

    LLSliderCtrl* fov_slider = getChild<LLSliderCtrl>("camera_fov");
    fov_slider->setMinValue(LLViewerCamera::getInstance()->getMinView());
    fov_slider->setMaxValue(LLViewerCamera::getInstance()->getMaxView());

    bool enable_complexity = gSavedSettings.getS32("RenderAvatarComplexityMode") != LLVOAvatar::AV_RENDER_ONLY_SHOW_FRIENDS;
    getChild<LLSliderCtrl>("IndirectMaxComplexity")->setEnabled(enable_complexity);

    // Hook up and init for filtering
    mFilterEdit = getChild<LLSearchEditor>("search_prefs_edit");
    mFilterEdit->setKeystrokeCallback(boost::bind(&LLFloaterPreference::onUpdateFilterTerm, this, false));

    // Load and assign label for 'default language'
    std::string user_filename = gDirUtilp->getExpandedFilename(LL_PATH_DEFAULT_SKIN, "default_languages.xml");
    std::map<std::string, std::string> labels;
    if (loadFromFilename(user_filename, labels))
    {
        std::string system_lang = gSavedSettings.getString("SystemLanguage");
        std::map<std::string, std::string>::iterator iter = labels.find(system_lang);
        if (iter != labels.end())
        {
            getChild<LLComboBox>("language_combobox")->add(iter->second, LLSD("default"), ADD_TOP, true);
        }
        else
        {
            LL_WARNS() << "Language \"" << system_lang << "\" is not in default_languages.xml" << LL_ENDL;
            getChild<LLComboBox>("language_combobox")->add("System default", LLSD("default"), ADD_TOP, true);
        }
    }
    else
    {
        LL_WARNS() << "Failed to load labels from " << user_filename << ". Using default." << LL_ENDL;
        getChild<LLComboBox>("language_combobox")->add("System default", LLSD("default"), ADD_TOP, true);
    }

// [SL:KB] - Patch: Viewer-CrashReporting | Checked: 2011-06-11 (Catznip-2.6.c) | Added: Catznip-2.6.0c
#ifndef LL_SEND_CRASH_REPORTS
    // Hide the crash report tab if crash reporting isn't enabled
    LLTabContainer* pTabContainer = getChild<LLTabContainer>("pref core");
    if (pTabContainer)
    {
        LLPanel* pCrashReportPanel = pTabContainer->getPanelByName("crashreports");
        if (pCrashReportPanel)
            pTabContainer->removeTabPanel(pCrashReportPanel);
    }
#endif // LL_SEND_CRASH_REPORTS
// [/SL:KB]

// <FS:AW  opensim preferences>
#if !defined(OPENSIM) || defined(SINGLEGRID)
    // Hide the opensim tab if opensim isn't enabled
    LLTabContainer* tab_container = getChild<LLTabContainer>("pref core");
    if (tab_container)
    {
        LLPanel* opensim_panel = tab_container->getPanelByName("opensim");
        if (opensim_panel)
            tab_container->removeTabPanel(opensim_panel);
    }
#endif
#if defined(OPENSIM) && !defined(SINGLEGRID)
    childSetEnabled("show_grid_selection_check", !gSavedSettings.getBOOL("FSOpenSimAlwaysForceShowGrid"));
#endif
// </FS:AW  opensim preferences>

    // <FS:Zi> Pie menu
    gSavedSettings.getControl("OverridePieColors")->getSignal()->connect(boost::bind(&LLFloaterPreference::onPieColorsOverrideChanged, this));
    // make sure pie color controls are enabled or greyed out properly
    onPieColorsOverrideChanged();
    // </FS:Zi> Pie menu

    // <FS:Zi> Group Notices and chiclets location setting conversion bool => S32
    gSavedSettings.getControl("ShowGroupNoticesTopRight")->getSignal()->connect(boost::bind(&LLFloaterPreference::onShowGroupNoticesTopRightChanged, this));
    onShowGroupNoticesTopRightChanged();
    // </FS:Zi> Group Notices and chiclets location setting conversion bool => S32

    // <FS:Ansariel> Show email address in preferences (FIRE-1071)
    getChild<LLCheckBoxCtrl>("send_im_to_email")->setEnabled(false);
    getChild<LLCheckBoxCtrl>("send_im_to_email")->setVisible(false);
    childSetVisible("email_settings", false);
    childSetEnabled("email_settings", false);
    childSetVisible("email_settings_login_to_change", true);

    // <FS:Kadah> Load the list of font settings
    populateFontSelectionCombo();
    // </FS:Kadah>

    // <FS:Ansariel> Update label for max. non imposters and max complexity
    gSavedSettings.getControl("IndirectMaxNonImpostors")->getCommitSignal()->connect(boost::bind(&LLFloaterPreference::updateMaxNonImpostorsLabel, this, _2));
    gSavedSettings.getControl("RenderAvatarMaxComplexity")->getCommitSignal()->connect(boost::bind(&LLFloaterPreference::updateMaxComplexityLabel, this, _2));

    // <FS:Ansariel> Properly disable avatar tag setting
    gSavedSettings.getControl("NameTagShowUsernames")->getCommitSignal()->connect(boost::bind(&LLFloaterPreference::onAvatarTagSettingsChanged, this));
    gSavedSettings.getControl("FSNameTagShowLegacyUsernames")->getCommitSignal()->connect(boost::bind(&LLFloaterPreference::onAvatarTagSettingsChanged, this));
    gSavedSettings.getControl("AvatarNameTagMode")->getCommitSignal()->connect(boost::bind(&LLFloaterPreference::onAvatarTagSettingsChanged, this));
    gSavedSettings.getControl("FSTagShowARW")->getCommitSignal()->connect(boost::bind(&LLFloaterPreference::onAvatarTagSettingsChanged, this));
    onAvatarTagSettingsChanged();
    // </FS:Ansariel>

    // <FS:Ansariel> Correct enabled state of Animated Script Dialogs option
    gSavedSettings.getControl("ScriptDialogsPosition")->getCommitSignal()->connect(boost::bind(&LLFloaterPreference::updateAnimatedScriptDialogs, this));
    updateAnimatedScriptDialogs();

    // <FS:Ansariel> Set max. UI scaling factor depending on max. supported OS scaling factor
#if LL_WINDOWS
    if (IsWindowsVersionOrGreater(10, 0, 0))
    {
        getChild<LLSliderCtrl>("ui_scale_slider")->setMaxValue(4.5f);
    }
    else if (IsWindows8Point1OrGreater())
    {
        getChild<LLSliderCtrl>("ui_scale_slider")->setMaxValue(2.5f);
    }
#endif
    // </FS:Ansariel>

    // <FS:Ansariel> Disable options only available on Windows and not on other platforms
#ifndef LL_WINDOWS
    childSetEnabled("FSDisableWMIProbing", false);
#endif
    // </FS:Ansariel>

    // <FS:Ansariel> Disable options only available on Linux and not on other platforms
#ifndef LL_LINUX
    childSetEnabled("FSRemapLinuxShortcuts", false);
#endif
    // </FS:Ansariel>

    // <FS:Zi> FIRE-19539 - Include the alert messages in Prefs>Notifications>Alerts in preference Search.
    mPopupList = getChild<LLScrollListCtrl>("all_popups");
    mPopupList->setFilterColumn(COLUMN_POPUP_LABEL);
    mPopupFilter = getChild<LLFilterEditor>("popup_filter");
    // </FS:Zi>

    // <FS:Zi> SDL2 IME support
#if LL_SDL2
    childSetVisible("use_ime", true);
#endif
    // </FS:Zi>

    return true;
}

// <FS:Zi> Pie menu
void LLFloaterPreference::onPieColorsOverrideChanged()
{
    bool enable = gSavedSettings.getBOOL("OverridePieColors");

    getChild<LLColorSwatchCtrl>("pie_bg_color_override")->setEnabled(enable);
    getChild<LLColorSwatchCtrl>("pie_selected_color_override")->setEnabled(enable);
    getChild<LLSliderCtrl>("pie_menu_opacity")->setEnabled(enable);
    getChild<LLSliderCtrl>("pie_menu_fade_out")->setEnabled(enable);
}
// </FS:Zi> Pie menu

// <FS:Zi> Group Notices and chiclets location setting conversion bool => S32
void LLFloaterPreference::onShowGroupNoticesTopRightChanged()
{
    getChild<LLRadioGroup>("ShowGroupNoticesTopRight")->setValue(gSavedSettings.getBOOL("ShowGroupNoticesTopRight"));
}
// </FS:Zi> Group Notices and chiclets location setting conversion bool => S32

void LLFloaterPreference::updateDeleteTranscriptsButton()
{
    mDeleteTranscriptsBtn->setEnabled(LLLogChat::transcriptFilesExist());
}

void LLFloaterPreference::onDoNotDisturbResponseChanged()
{
    // set "DoNotDisturbResponseChanged" true if user edited message differs from default, false otherwise
    bool response_changed_flag =
            LLTrans::getString("DoNotDisturbModeResponseDefault")
                    != getChild<LLUICtrl>("do_not_disturb_response")->getValue().asString();

    gSavedPerAccountSettings.setBOOL("DoNotDisturbResponseChanged", response_changed_flag );

    // <FS:Ansariel> FIRE-5436: Unlocalizable auto-response messages
    bool auto_response_changed_flag =
            LLTrans::getString("AutoResponseModeDefault")
                    != getChild<LLUICtrl>("autorespond_response")->getValue().asString();

    gSavedPerAccountSettings.setBOOL("FSAutoResponseChanged", auto_response_changed_flag );

    bool auto_response_non_friends_changed_flag =
            LLTrans::getString("AutoResponseModeNonFriendsDefault")
                    != getChild<LLUICtrl>("autorespond_nf_response")->getValue().asString();

    gSavedPerAccountSettings.setBOOL("FSAutoResponseNonFriendsChanged", auto_response_non_friends_changed_flag );

    bool reject_teleport_offers_response_changed_flag =
            LLTrans::getString("RejectTeleportOffersResponseDefault")
                    != getChild<LLUICtrl>("autorespond_rto_response")->getValue().asString();

    gSavedPerAccountSettings.setBOOL("FSRejectTeleportOffersResponseChanged", reject_teleport_offers_response_changed_flag );

    bool reject_friendship_requests_response_changed_flag =
            LLTrans::getString("RejectFriendshipRequestsResponseDefault")
                    != getChild<LLUICtrl>("autorespond_rfr_response")->getValue().asString();

    gSavedPerAccountSettings.setBOOL("FSRejectFriendshipRequestsResponseChanged", reject_friendship_requests_response_changed_flag );

    bool muted_avatar_response_changed_flag =
            LLTrans::getString("MutedAvatarsResponseDefault")
                    != getChild<LLUICtrl>("muted_avatar_response")->getValue().asString();

    gSavedPerAccountSettings.setBOOL("FSMutedAvatarResponseChanged", muted_avatar_response_changed_flag );

    bool away_avatar_response_changed_flag =
            LLTrans::getString("AwayAvatarResponseDefault")
                    != getChild<LLUICtrl>("away_avatar_response")->getValue().asString();

    gSavedPerAccountSettings.setBOOL("FSAwayAvatarResponseChanged", away_avatar_response_changed_flag );
    // </FS:Ansariel>
}

LLFloaterPreference::~LLFloaterPreference()
{
    LLConversationLog::instance().removeObserver(this);
    mComplexityChangedSignal.disconnect();
}

// <FS:Zi> FIRE-19539 - Include the alert messages in Prefs>Notifications>Alerts in preference Search.
// void LLFloaterPreference::draw()
// {
//  bool has_first_selected = (mDisabledPopups->getFirstSelected()!=NULL);
//  mEnablePopupBtn->setEnabled(has_first_selected);
//
//  has_first_selected = (mEnabledPopups.getFirstSelected()!=NULL);
//  mDisablePopupBtn->setEnabled(has_first_selected);
//
//  LLFloater::draw();
//}
// </FS:Zi>

void LLFloaterPreference::saveSettings()
{
    LLTabContainer* tabcontainer = getChild<LLTabContainer>("pref core");
    child_list_t::const_iterator iter = tabcontainer->getChildList()->begin();
    child_list_t::const_iterator end = tabcontainer->getChildList()->end();
    for ( ; iter != end; ++iter)
    {
        LLView* view = *iter;
        LLPanelPreference* panel = dynamic_cast<LLPanelPreference*>(view);
        if (panel)
            panel->saveSettings();
    }
    saveIgnoredNotifications();
}

void LLFloaterPreference::apply()
{
    LLAvatarPropertiesProcessor::getInstance()->addObserver( gAgent.getID(), this );

    LLTabContainer* tabcontainer = getChild<LLTabContainer>("pref core");
/*
 if (sSkin != gSavedSettings.getString("SkinCurrent"))
    {
        LLNotificationsUtil::add("ChangeSkin");
        refreshSkin(this);
    }
*/
    // Call apply() on all panels that derive from LLPanelPreference
    for (child_list_t::const_iterator iter = tabcontainer->getChildList()->begin();
         iter != tabcontainer->getChildList()->end(); ++iter)
    {
        LLView* view = *iter;
        LLPanelPreference* panel = dynamic_cast<LLPanelPreference*>(view);
        if (panel)
            panel->apply();
    }

    gViewerWindow->requestResolutionUpdate(); // for UIScaleFactor

    LLSliderCtrl* fov_slider = getChild<LLSliderCtrl>("camera_fov");
    fov_slider->setMinValue(LLViewerCamera::getInstance()->getMinView());
    fov_slider->setMaxValue(LLViewerCamera::getInstance()->getMaxView());

    std::string cache_location = gDirUtilp->getExpandedFilename(LL_PATH_CACHE, "");
    setCacheLocation(cache_location);
    // <FS:Ansariel> Sound cache
    setSoundCacheLocation(gSavedSettings.getString("FSSoundCacheLocation"));

    //LLViewerMedia::getInstance()->setCookiesEnabled(getChild<LLUICtrl>("cookies_enabled")->getValue());

    if (hasChild("web_proxy_enabled", true) &&hasChild("web_proxy_editor", true) && hasChild("web_proxy_port", true))
    {
        bool proxy_enable = getChild<LLUICtrl>("web_proxy_enabled")->getValue();
        std::string proxy_address = getChild<LLUICtrl>("web_proxy_editor")->getValue();
        int proxy_port = getChild<LLUICtrl>("web_proxy_port")->getValue();
        LLViewerMedia::getInstance()->setProxyConfig(proxy_enable, proxy_address, proxy_port);
    }

    if (mGotPersonalInfo)
    {
        // <FS:Ansariel> Keep this for OpenSim
        bool new_im_via_email = getChild<LLUICtrl>("send_im_to_email")->getValue().asBoolean();
        bool new_hide_online = getChild<LLUICtrl>("online_visibility")->getValue().asBoolean();

        // <FS:Ansariel> Keep this for OpenSim
        //if (new_hide_online != mOriginalHideOnlineStatus)
        if ((new_im_via_email != mOriginalIMViaEmail)
            ||(new_hide_online != mOriginalHideOnlineStatus))
        {
            // This hack is because we are representing several different
            // possible strings with a single checkbox. Since most users
            // can only select between 2 values, we represent it as a
            // checkbox. This breaks down a little bit for liaisons, but
            // works out in the end.
            if (new_hide_online != mOriginalHideOnlineStatus)
            {
                if (new_hide_online) mDirectoryVisibility = VISIBILITY_HIDDEN;
                else mDirectoryVisibility = VISIBILITY_DEFAULT;
             //Update showonline value, otherwise multiple applys won't work
                mOriginalHideOnlineStatus = new_hide_online;
            }
            // <FS:Ansariel> Keep this for OpenSim
            //gAgent.sendAgentUpdateUserInfo(mDirectoryVisibility);
            gAgent.sendAgentUpdateUserInfo(new_im_via_email,mDirectoryVisibility);
        }
    }

    saveAvatarProperties();

    // <FS:Ansariel> Fix resetting graphics preset on cancel; Save preset here because cancel() gets called in either way!
    saveGraphicsPreset(gSavedSettings.getString("PresetGraphicActive"));
}

void LLFloaterPreference::cancel(const std::vector<std::string> settings_to_skip)
{
    LLTabContainer* tabcontainer = getChild<LLTabContainer>("pref core");
    // Call cancel() on all panels that derive from LLPanelPreference
    for (child_list_t::const_iterator iter = tabcontainer->getChildList()->begin();
        iter != tabcontainer->getChildList()->end(); ++iter)
    {
        LLView* view = *iter;
        LLPanelPreference* panel = dynamic_cast<LLPanelPreference*>(view);
        if (panel)
            panel->cancel(settings_to_skip);
    }
    // hide joystick pref floater
    LLFloaterReg::hideInstance("pref_joystick");

    // hide translation settings floater
    LLFloaterReg::hideInstance("prefs_translation");

    // hide autoreplace settings floater
    LLFloaterReg::hideInstance("prefs_autoreplace");

    // hide spellchecker settings folder
    LLFloaterReg::hideInstance("prefs_spellchecker");

    // hide advanced graphics floater
    LLFloaterReg::hideInstance("prefs_graphics_advanced");

    // reverts any changes to current skin
    //gSavedSettings.setString("SkinCurrent", sSkin);

    updateClickActionViews();

    LLFloaterPreferenceProxy * advanced_proxy_settings = LLFloaterReg::findTypedInstance<LLFloaterPreferenceProxy>("prefs_proxy");
    if (advanced_proxy_settings)
    {
        advanced_proxy_settings->cancel();
    }
    //Need to reload the navmesh if the pathing console is up
    LLHandle<LLFloaterPathfindingConsole> pathfindingConsoleHandle = LLFloaterPathfindingConsole::getInstanceHandle();
    if ( !pathfindingConsoleHandle.isDead() )
    {
        LLFloaterPathfindingConsole* pPathfindingConsole = pathfindingConsoleHandle.get();
        pPathfindingConsole->onRegionBoundaryCross();
    }

    // <FS:Ansariel> Fix resetting graphics preset on cancel
    //if (!mSavedGraphicsPreset.empty())
    if (mSavedGraphicsPreset != gSavedSettings.getString("PresetGraphicActive"))
    // </FS:Ansariel>
    {
        gSavedSettings.setString("PresetGraphicActive", mSavedGraphicsPreset);
        LLPresetsManager::getInstance()->triggerChangeSignal();
    }

    restoreIgnoredNotifications();
}

void LLFloaterPreference::onOpen(const LLSD& key)
{
    // this variable and if that follows it are used to properly handle do not disturb mode response message
    static bool initialized = false;
    // if user is logged in and we haven't initialized do not disturb mode response yet, do it
    if (!initialized && LLStartUp::getStartupState() == STATE_STARTED)
    {
        // Special approach is used for do not disturb response localization, because "DoNotDisturbModeResponse" is
        // in non-localizable xml, and also because it may be changed by user and in this case it shouldn't be localized.
        // To keep track of whether do not disturb response is default or changed by user additional setting DoNotDisturbResponseChanged
        // was added into per account settings.

        // initialization should happen once,so setting variable to true
        initialized = true;
        // this connection is needed to properly set "DoNotDisturbResponseChanged" setting when user makes changes in
        // do not disturb response message.
        gSavedPerAccountSettings.getControl("DoNotDisturbModeResponse")->getSignal()->connect(boost::bind(&LLFloaterPreference::onDoNotDisturbResponseChanged, this));
        // <FS:Ansariel> FIRE-5436: Unlocalizable auto-response messages
        gSavedPerAccountSettings.getControl("FSAutorespondModeResponse")->getSignal()->connect(boost::bind(&LLFloaterPreference::onDoNotDisturbResponseChanged, this));
        gSavedPerAccountSettings.getControl("FSAutorespondNonFriendsResponse")->getSignal()->connect(boost::bind(&LLFloaterPreference::onDoNotDisturbResponseChanged, this));
        gSavedPerAccountSettings.getControl("FSRejectTeleportOffersResponse")->getSignal()->connect(boost::bind(&LLFloaterPreference::onDoNotDisturbResponseChanged, this));
        gSavedPerAccountSettings.getControl("FSRejectFriendshipRequestsResponse")->getSignal()->connect(boost::bind(&LLFloaterPreference::onDoNotDisturbResponseChanged, this));
        gSavedPerAccountSettings.getControl("FSMutedAvatarResponse")->getSignal()->connect(boost::bind(&LLFloaterPreference::onDoNotDisturbResponseChanged, this));
        gSavedPerAccountSettings.getControl("FSAwayAvatarResponse")->getSignal()->connect(boost::bind(&LLFloaterPreference::onDoNotDisturbResponseChanged, this));
        // </FS:Ansariel>

        // <FS:Ansariel> FIRE-17630: Properly disable per-account settings backup list
        getChildView("restore_per_account_disable_cover")->setVisible(false);

        // <FS:Ansariel> Keyword settings are per-account; enable after logging in
        LLPanel* keyword_panel = getChild<LLPanel>("ChatKeywordAlerts");
        for (child_list_t::const_iterator iter = keyword_panel->getChildList()->begin();
             iter != keyword_panel->getChildList()->end(); ++iter)
        {
            LLUICtrl* child = static_cast<LLUICtrl*>(*iter);
            LLControlVariable* enabled_control = child->getEnabledControlVariable();
            bool enabled = !enabled_control || enabled_control->getValue().asBoolean();
            child->setEnabled(enabled);
        }
        // </FS:Ansariel>
    }
    gAgent.sendAgentUserInfoRequest();

    /////////////////////////// From LLPanelGeneral //////////////////////////
    // if we have no agent, we can't let them choose anything
    // if we have an agent, then we only let them choose if they have a choice
    bool can_choose_maturity =
        gAgent.getID().notNull() &&
        (gAgent.isMature() || gAgent.isGodlike());

    LLComboBox* maturity_combo = getChild<LLComboBox>("maturity_desired_combobox");
    LLAvatarPropertiesProcessor::getInstance()->sendAvatarLegacyPropertiesRequest( gAgent.getID() );
    if (can_choose_maturity)
    {
        // if they're not adult or a god, they shouldn't see the adult selection, so delete it
        if (!gAgent.isAdult() && !gAgent.isGodlikeWithoutAdminMenuFakery())
        {
            // we're going to remove the adult entry from the combo
            LLScrollListCtrl* maturity_list = maturity_combo->findChild<LLScrollListCtrl>("ComboBox");
            if (maturity_list)
            {
                maturity_list->deleteItems(LLSD(SIM_ACCESS_ADULT));
            }
        }
        getChildView("maturity_desired_combobox")->setEnabled( true);
        getChildView("maturity_desired_textbox")->setVisible( false);
    }
    else
    {
        getChild<LLUICtrl>("maturity_desired_textbox")->setValue(maturity_combo->getSelectedItemLabel());
        getChildView("maturity_desired_combobox")->setEnabled( false);
    }

    // Forget previous language changes.
    mLanguageChanged = false;

    // Display selected maturity icons.
    onChangeMaturity();

    onChangeModelFolder();
    onChangePBRFolder();
    onChangeTextureFolder();
    onChangeSoundFolder();
    onChangeAnimationFolder();

    // Load (double-)click to walk/teleport settings.
    updateClickActionViews();

    // <FS:PP> Load UI Sounds tabs settings.
    updateUISoundsControls();

    // Enabled/disabled popups, might have been changed by user actions
    // while preferences floater was closed.

    // <FS:Zi> FIRE-19539 - Include the alert messages in Prefs>Notifications>Alerts in preference Search.
    // buildPopupLists();
    mPopupFilter->setText(LLStringExplicit(""));
    mPopupList->setFilterString(LLStringExplicit(""));

    buildPopupList();
    // </FS:Zi>

    //get the options that were checked
    // <FS:CR> [CHUI MERGE]
    // We don't use these in FS Communications UI, should we in the future? Disabling for now.
    //onNotificationsChange("FriendIMOptions");
    //onNotificationsChange("NonFriendIMOptions");
    //onNotificationsChange("ConferenceIMOptions");
    //onNotificationsChange("GroupChatOptions");
    //onNotificationsChange("NearbyChatOptions");
    //onNotificationsChange("ObjectIMOptions");
    // </FS:CR>

    // <FS:Ansariel> [FS Login Panel]
    //LLPanelLogin::setAlwaysRefresh(true);
    FSPanelLogin::setAlwaysRefresh(true);
    // </FS:Ansariel> [FS Login Panel]
    refresh();


    getChildView("plain_text_chat_history")->setEnabled(true);
    getChild<LLUICtrl>("plain_text_chat_history")->setValue(gSavedSettings.getBOOL("PlainTextChatHistory"));

// <FS:CR> Show/hide Client Tag panel
    bool in_opensim = false;
#ifdef OPENSIM
    in_opensim = LLGridManager::getInstance()->isInOpenSim();
#endif // OPENSIM
    getChild<LLPanel>("client_tags_panel")->setVisible(in_opensim);
// </FS:CR>

    // <FS:Ansariel> Group mutes backup
    LLScrollListItem* groupmute_item = getChild<LLScrollListCtrl>("restore_per_account_files_list")->getItem(LLSD("groupmutes"));
    groupmute_item->setEnabled(in_opensim);
    groupmute_item->getColumn(0)->setEnabled(in_opensim);
    // </FS:Ansariel>

    // <FS:Ansariel> Call onOpen on all panels for additional initialization on open
    // Call onOpen() on all panels that derive from LLPanelPreference
    LLTabContainer* tabcontainer = getChild<LLTabContainer>("pref core");
    for (child_list_t::const_iterator iter = tabcontainer->getChildList()->begin();
        iter != tabcontainer->getChildList()->end(); ++iter)
    {
        LLView* view = *iter;
        LLPanelPreference* panel = dynamic_cast<LLPanelPreference*>(view);
        if (panel)
            panel->onOpen(key);
    }
    // </FS:Ansariel>

    // Make sure the current state of prefs are saved away when
    // when the floater is opened.  That will make cancel do its
    // job
    saveSettings();

    // Make sure there is a default preference file
    LLPresetsManager::getInstance()->createMissingDefault(PRESETS_CAMERA);
    LLPresetsManager::getInstance()->createMissingDefault(PRESETS_GRAPHIC);

    // <FS:Ansariel> Fix resetting graphics preset on cancel
    saveGraphicsPreset(gSavedSettings.getString("PresetGraphicActive"));

    // <FS:Ansariel> FIRE-19810: Make presets global since PresetGraphicActive setting is global as well
    //bool started = (LLStartUp::getStartupState() == STATE_STARTED);
    //LLButton* load_btn = findChild<LLButton>("PrefLoadButton");
    //LLButton* save_btn = findChild<LLButton>("PrefSaveButton");
    //LLButton* delete_btn = findChild<LLButton>("PrefDeleteButton");
    //LLButton* exceptions_btn = findChild<LLButton>("RenderExceptionsButton");
    // LLButton* auto_adjustments_btn = findChild<LLButton>("AutoAdjustmentsButton");
    //if (load_btn && save_btn && delete_btn && exceptions_btn && auto_adjustments_btn)
    //{
    //  load_btn->setEnabled(started);
    //  save_btn->setEnabled(started);
    //  delete_btn->setEnabled(started);
    //  exceptions_btn->setEnabled(started);
    //  auto_adjustments_btn->setEnabled(started);
    //}
    // </FS:Ansariel>
    collectSearchableItems();
    if (!mFilterEdit->getText().empty())
    {
        mFilterEdit->setText(LLStringExplicit(""));
        onUpdateFilterTerm(true);

        // <FS:ND> Hook up and init for filtering
        if (!tabcontainer->selectTab(gSavedSettings.getS32("LastPrefTab")))
            tabcontainer->selectFirstTab();
        // </FS:ND>

    }
    // <FS:Zi> Support for tab/subtab links like:
    //         secondlife:///app/openfloater/preferences?tab=backup
    //         secondlife:///app/openfloater/preferences?tab=colors&subtab=tab-minimap
    if (key.has("tab"))
    {
        selectPanel(key["tab"]);

        if (key.has("subtab"))
        {
            LLTabContainer* tab_containerp = findChild<LLTabContainer>("pref core");
            if (tab_containerp)
            {
                LLTabContainer* tabs = tab_containerp->getCurrentPanel()->findChild<LLTabContainer>("tabs");
                if (tabs)
                {
                    LLPanel* panel = tabs->getPanelByName(key["subtab"].asString());
                    if (panel)
                    {
                        tabs->selectTabPanel(panel);
                    }
                }
            }
        }
    }
    // Support preferences search SLURLs:
    // secondlife:///app/openfloater/preferences?search=%23%20of%20lines
    else if (key.has("search"))
    {
        mFilterEdit->setText(key["search"].asString());
        onUpdateFilterTerm(true);
    }
    // </FS:Zi>
}

void LLFloaterPreference::onRenderOptionEnable()
{
    refreshEnabledGraphics();
}

void LLFloaterPreference::onAvatarImpostorsEnable()
{
    refreshEnabledGraphics();
}

// <FS:AO> toggle lighting detail availability in response to local light rendering, to avoid confusion
void LLFloaterPreference::onLocalLightsEnable()
{
    LLFloaterPreference* instance = LLFloaterReg::findTypedInstance<LLFloaterPreference>("preferences");
    if (instance)
    {
        getChildView("LocalLightsDetail")->setEnabled(gSavedSettings.getBOOL("RenderLocalLights"));
    }
}
// </FS:AO>

//static
void LLFloaterPreference::initDoNotDisturbResponse()
    {
        if (!gSavedPerAccountSettings.getBOOL("DoNotDisturbResponseChanged"))
        {
            //LLTrans::getString("DoNotDisturbModeResponseDefault") is used here for localization (EXT-5885)
            gSavedPerAccountSettings.setString("DoNotDisturbModeResponse", LLTrans::getString("DoNotDisturbModeResponseDefault"));
        }

        // <FS:Ansariel> FIRE-5436: Unlocalizable auto-response messages
        if (!gSavedPerAccountSettings.getBOOL("FSAutoResponseChanged"))
        {
            gSavedPerAccountSettings.setString("FSAutorespondModeResponse", LLTrans::getString("AutoResponseModeDefault"));
        }

        if (!gSavedPerAccountSettings.getBOOL("FSAutoResponseNonFriendsChanged"))
        {
            gSavedPerAccountSettings.setString("FSAutorespondNonFriendsResponse", LLTrans::getString("AutoResponseModeNonFriendsDefault"));
        }

        if (!gSavedPerAccountSettings.getBOOL("FSRejectTeleportOffersResponseChanged"))
        {
            gSavedPerAccountSettings.setString("FSRejectTeleportOffersResponse", LLTrans::getString("RejectTeleportOffersResponseDefault"));
        }

        if (!gSavedPerAccountSettings.getBOOL("FSRejectFriendshipRequestsResponseChanged"))
        {
            gSavedPerAccountSettings.setString("FSRejectFriendshipRequestsResponse", LLTrans::getString("RejectFriendshipRequestsResponseDefault"));
        }

        if (!gSavedPerAccountSettings.getBOOL("FSMutedAvatarResponseChanged"))
        {
            gSavedPerAccountSettings.setString("FSMutedAvatarResponse", LLTrans::getString("MutedAvatarsResponseDefault"));
        }

        if (!gSavedPerAccountSettings.getBOOL("FSAwayAvatarResponseChanged"))
        {
            gSavedPerAccountSettings.setString("FSAwayAvatarResponse", LLTrans::getString("AwayAvatarResponseDefault"));
        }
        // </FS:Ansariel>
    }

//static
void LLFloaterPreference::updateShowFavoritesCheckbox(bool val)
{
    LLFloaterPreference* instance = LLFloaterReg::findTypedInstance<LLFloaterPreference>("preferences");
    if (instance)
    {
        instance->getChild<LLUICtrl>("favorites_on_login_check")->setValue(val);
    }
}

void LLFloaterPreference::setHardwareDefaults()
{
    // <FS:Ansariel> Fix resetting graphics preset on cancel
    //std::string preset_graphic_active = gSavedSettings.getString("PresetGraphicActive");
    //if (!preset_graphic_active.empty())
    //{
    //  saveGraphicsPreset(preset_graphic_active);
    //  saveSettings(); // save here to be able to return to the previous preset by Cancel
    //}
    // </FS:Ansariel>
    setRecommendedSettings();
}

void LLFloaterPreference::setRecommendedSettings()
{
    resetAutotuneSettings();
    gSavedSettings.getControl("RenderVSyncEnable")->resetToDefault(true);

    LLFeatureManager::getInstance()->applyRecommendedSettings();

    // reset indirects before refresh because we may have changed what they control
    LLAvatarComplexityControls::setIndirectControls();

    refreshEnabledGraphics();
    gSavedSettings.setString("PresetGraphicActive", "");
    LLPresetsManager::getInstance()->triggerChangeSignal();

    LLTabContainer* tabcontainer = getChild<LLTabContainer>("pref core");
    child_list_t::const_iterator iter = tabcontainer->getChildList()->begin();
    child_list_t::const_iterator end = tabcontainer->getChildList()->end();
    for ( ; iter != end; ++iter)
    {
        LLView* view = *iter;
        LLPanelPreference* panel = dynamic_cast<LLPanelPreference*>(view);
        if (panel)
        {
            panel->setHardwareDefaults();
        }
    }
}

void LLFloaterPreference::resetAutotuneSettings()
{
    gSavedSettings.setBOOL("AutoTuneFPS", false);

    const std::string autotune_settings[] = {
        "AutoTuneLock",
        "KeepAutoTuneLock",
        "TargetFPS",
        "TuningFPSStrategy",
        "AutoTuneImpostorByDistEnabled",
        "AutoTuneImpostorFarAwayDistance" ,
        "AutoTuneRenderFarClipMin",
        "AutoTuneRenderFarClipTarget",
        "RenderAvatarMaxART"
    };

    for (auto it : autotune_settings)
    {
        gSavedSettings.getControl(it)->resetToDefault(true);
    }
}

void LLFloaterPreference::getControlNames(std::vector<std::string>& names)
{
    LLView* view = findChild<LLView>("display");
    LLFloater* advanced = LLFloaterReg::findTypedInstance<LLFloater>("prefs_graphics_advanced");
    // <FS:Ansariel> Improved graphics preferences
    //if (view && advanced)
    if (view)
    // </FS:Ansariel>
    {
        std::list<LLView*> stack;
        stack.push_back(view);
        // <FS:Ansariel> Improved graphics preferences
        //stack.push_back(advanced);
        if (advanced)
        {
            stack.push_back(advanced);
        }
        // </FS:Ansariel>
        while(!stack.empty())
        {
            // Process view on top of the stack
            LLView* curview = stack.front();
            stack.pop_front();

            LLUICtrl* ctrl = dynamic_cast<LLUICtrl*>(curview);
            if (ctrl)
            {
                LLControlVariable* control = ctrl->getControlVariable();
                if (control)
                {
                    std::string control_name = control->getName();
                    if (std::find(names.begin(), names.end(), control_name) == names.end())
                    {
                        names.push_back(control_name);
                    }
                }
            }

            for (child_list_t::const_iterator iter = curview->getChildList()->begin();
                iter != curview->getChildList()->end(); ++iter)
            {
                stack.push_back(*iter);
            }
        }
    }
}

//virtual
void LLFloaterPreference::onClose(bool app_quitting)
{
    // <FS:Ansariel> Preferences search
    //gSavedSettings.setS32("LastPrefTab", getChild<LLTabContainer>("pref core")->getCurrentPanelIndex());
    if (mFilterEdit->getText().empty())
    {
        gSavedSettings.setS32("LastPrefTab", getChild<LLTabContainer>("pref core")->getCurrentPanelIndex());
    }
    // </FS:Ansariel>
    // <FS:Ansariel> [FS Login Panel]
    //LLPanelLogin::setAlwaysRefresh(false);
    FSPanelLogin::setAlwaysRefresh(false);
    // </FS:Ansariel> [FS Login Panel]
    if (!app_quitting)
    {
        cancel();
    }
}

// static
void LLFloaterPreference::onBtnOK(const LLSD& userdata)
{
    // commit any outstanding text entry
    if (hasFocus())
    {
        LLUICtrl* cur_focus = dynamic_cast<LLUICtrl*>(gFocusMgr.getKeyboardFocus());
        if (cur_focus && cur_focus->acceptsTextInput())
        {
            cur_focus->onCommit();
        }
    }

    if (canClose())
    {
        saveSettings();
        apply();

        if (userdata.asString() == "closeadvanced")
        {
            LLFloaterReg::hideInstance("prefs_graphics_advanced");
        }
        else
        {
            closeFloater(false);
        }

        //Conversation transcript and log path changed so reload conversations based on new location
        if(mPriorInstantMessageLogPath.length())
        {
            if(moveTranscriptsAndLog())
            {
                //When floaters are empty but have a chat history files, reload chat history into them
                // <FS:Ansariel> [FS communication UI]
                //LLFloaterIMSessionTab::reloadEmptyFloaters();
                FSFloaterIMContainer::reloadEmptyFloaters();
                // </FS:Ansariel> [FS communication UI]
            }
            //Couldn't move files so restore the old path and show a notification
            else
            {
                gSavedPerAccountSettings.setString("InstantMessageLogPath", mPriorInstantMessageLogPath);
                LLNotificationsUtil::add("PreferenceChatPathChanged");
            }
            mPriorInstantMessageLogPath.clear();
        }

        LLUIColorTable::instance().saveUserSettings();
        gSavedSettings.saveToFile(gSavedSettings.getString("ClientSettingsFile"), true);
// [SL:KB] - Patch: Viewer-CrashReporting | Checked: 2011-10-02 (Catznip-2.8.0e) | Added: Catznip-2.8.0e
        // We need to save all crash settings, even if they're defaults [see LLCrashLogger::loadCrashBehaviorSetting()]
        gCrashSettings.saveToFile(gSavedSettings.getString("CrashSettingsFile"), false);
// [/SL:KB]

        //Only save once logged in and loaded per account settings
        if(mGotPersonalInfo)
        {
            gSavedPerAccountSettings.saveToFile(gSavedSettings.getString("PerAccountSettingsFile"), true);
        }
    }
    else
    {
        // Show beep, pop up dialog, etc.
        LL_INFOS("Preferences") << "Can't close preferences!" << LL_ENDL;
    }

    // <FS:Ansariel> [FS Login Panel]
    //LLPanelLogin::updateLocationSelectorsVisibility();
    FSPanelLogin::updateLocationSelectorsVisibility();
    // </FS:Ansariel> [FS Login Panel]
    //Need to reload the navmesh if the pathing console is up
    LLHandle<LLFloaterPathfindingConsole> pathfindingConsoleHandle = LLFloaterPathfindingConsole::getInstanceHandle();
    if ( !pathfindingConsoleHandle.isDead() )
    {
        LLFloaterPathfindingConsole* pPathfindingConsole = pathfindingConsoleHandle.get();
        pPathfindingConsole->onRegionBoundaryCross();
    }

}

// static
void LLFloaterPreference::onBtnCancel(const LLSD& userdata)
{
    if (hasFocus())
    {
        LLUICtrl* cur_focus = dynamic_cast<LLUICtrl*>(gFocusMgr.getKeyboardFocus());
        if (cur_focus && cur_focus->acceptsTextInput())
        {
            cur_focus->onCommit();
        }
        refresh();
    }

    if (userdata.asString() == "closeadvanced")
    {
        cancel({"RenderQualityPerformance"});
        LLFloaterReg::hideInstance("prefs_graphics_advanced");
    }
    else
    {
        cancel();
        closeFloater();
    }
}

// static
// <FS:Ansariel> Show email address in preferences (FIRE-1071) and keep it for OpenSim
//void LLFloaterPreference::updateUserInfo(const std::string& visibility)
void LLFloaterPreference::updateUserInfo(const std::string& visibility, bool im_via_email, const std::string& email)
{
    LLFloaterPreference* instance = LLFloaterReg::findTypedInstance<LLFloaterPreference>("preferences");
    if (instance)
    {
        // <FS:Ansariel> Show email address in preferences (FIRE-1071) and keep it for OpenSim
        //instance->setPersonalInfo(visibility);
        instance->setPersonalInfo(visibility, im_via_email, email);
    }
}


void LLFloaterPreference::refreshEnabledGraphics()
{
    LLFloaterPreference* instance = LLFloaterReg::findTypedInstance<LLFloaterPreference>("preferences");
    if (instance)
    {
        instance->refresh();
    }

    LLFloater* advanced = LLFloaterReg::findTypedInstance<LLFloater>("prefs_graphics_advanced");
    if (advanced)
    {
        advanced->refresh();
    }
}

void LLFloaterPreference::onClickClearCache()
{
    LLNotificationsUtil::add("ConfirmClearCache", LLSD(), LLSD(), callback_clear_cache);
}

void LLFloaterPreference::onClickBrowserClearCache()
{
    LLNotificationsUtil::add("ConfirmClearBrowserCache", LLSD(), LLSD(), callback_clear_browser_cache);
}

// <FS:Ansariel> Clear inventory cache button
void LLFloaterPreference::onClickInventoryClearCache()
{
    LLNotificationsUtil::add("ConfirmClearInventoryCache", LLSD(), LLSD(), callback_clear_inventory_cache);
}
// </FS:Ansariel>

// <FS:Ansariel> Clear web browser cache button
void LLFloaterPreference::onClickWebBrowserClearCache()
{
    LLNotificationsUtil::add("ConfirmClearWebBrowserCache", LLSD(), LLSD(), callback_clear_web_browser_cache);
}
// </FS:Ansariel>

// Called when user changes language via the combobox.
void LLFloaterPreference::onLanguageChange()
{
    // Let the user know that the change will only take effect after restart.
    // Do it only once so that we're not too irritating.
    if (!mLanguageChanged)
    {
        LLNotificationsUtil::add("ChangeLanguage");
        mLanguageChanged = true;
    }
}

void LLFloaterPreference::onNotificationsChange(const std::string& OptionName)
{
    mNotificationOptions[OptionName] = getChild<LLComboBox>(OptionName)->getSelectedItemLabel();

    bool show_notifications_alert = true;
    for (notifications_map::iterator it_notification = mNotificationOptions.begin(); it_notification != mNotificationOptions.end(); it_notification++)
    {
        if(it_notification->second != "No action")
        {
            show_notifications_alert = false;
            break;
        }
    }

    getChild<LLTextBox>("notifications_alert")->setVisible(show_notifications_alert);
}

void LLFloaterPreference::onNameTagOpacityChange(const LLSD& newvalue)
{
    LLColorSwatchCtrl* color_swatch = findChild<LLColorSwatchCtrl>("background");
    if (color_swatch)
    {
        LLColor4 new_color = color_swatch->get();
        color_swatch->set(new_color.setAlpha((F32)newvalue.asReal()));
    }
}

// <FS:CR> FIRE-1332 - Sepeate opacity settings for nametag and console chat
void LLFloaterPreference::onConsoleOpacityChange(const LLSD& newvalue)
{
    LLColorSwatchCtrl* color_swatch = findChild<LLColorSwatchCtrl>("console_background");
    if (color_swatch)
    {
        LLColor4 new_color = color_swatch->get();
        color_swatch->set(new_color.setAlpha((F32)newvalue.asReal()));
    }
}
// </FS:CR>

void LLFloaterPreference::onClickSetCache()
{
    std::string cur_name(gSavedSettings.getString("CacheLocation"));
//  std::string cur_top_folder(gDirUtilp->getBaseFileName(cur_name));

    std::string proposed_name(cur_name);

    (new LLDirPickerThread(boost::bind(&LLFloaterPreference::changeCachePath, this, _1, _2), proposed_name))->getFile();
}

void LLFloaterPreference::changeCachePath(const std::vector<std::string>& filenames, std::string proposed_name)
{
    std::string dir_name = filenames[0];
    if (!dir_name.empty() && dir_name != proposed_name)
    {
        std::string new_top_folder(gDirUtilp->getBaseFileName(dir_name));
        LLNotificationsUtil::add("CacheWillBeMoved");
        gSavedSettings.setString("NewCacheLocation", dir_name);
        gSavedSettings.setString("NewCacheLocationTopFolder", new_top_folder);
    }
    else
    {
        std::string cache_location = gDirUtilp->getCacheDir();
        gSavedSettings.setString("CacheLocation", cache_location);
        std::string top_folder(gDirUtilp->getBaseFileName(cache_location));
        gSavedSettings.setString("CacheLocationTopFolder", top_folder);
    }
}

void LLFloaterPreference::onClickBrowseCache()
{
    gViewerWindow->getWindow()->openFile(gDirUtilp->getExpandedFilename(LL_PATH_CACHE,""));
}
void LLFloaterPreference::onClickBrowseCrashLogs()
{
    gViewerWindow->getWindow()->openFile(gDirUtilp->getExpandedFilename(LL_PATH_LOGS,""));
}
void LLFloaterPreference::onClickBrowseSettingsDir()
{
    gViewerWindow->getWindow()->openFile(gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS,""));
}
void LLFloaterPreference::onClickBrowseChatLogDir()
{
    gViewerWindow->getWindow()->openFile(gDirUtilp->getExpandedFilename(LL_PATH_CHAT_LOGS,""));
}
void LLFloaterPreference::onClickResetCache()
{
    if (gDirUtilp->getCacheDir(false) == gDirUtilp->getCacheDir(true))
    {
        // The cache location was already the default.
        return;
    }
    gSavedSettings.setString("NewCacheLocation", "");
    gSavedSettings.setString("NewCacheLocationTopFolder", "");
    LLNotificationsUtil::add("CacheWillBeMoved");
    std::string cache_location = gDirUtilp->getCacheDir(false);
    gSavedSettings.setString("CacheLocation", cache_location);
    std::string top_folder(gDirUtilp->getBaseFileName(cache_location));
    gSavedSettings.setString("CacheLocationTopFolder", top_folder);
}

// <FS:Ansariel> Sound cache
void LLFloaterPreference::onClickSetSoundCache()
{
    std::string cur_name(gSavedSettings.getString("FSSoundCacheLocation"));
    std::string proposed_name(cur_name);

    (new LLDirPickerThread(boost::bind(&LLFloaterPreference::changeSoundCachePath, this, _1, _2), proposed_name))->getFile();
}

void LLFloaterPreference::changeSoundCachePath(const std::vector<std::string>& filenames, std::string proposed_name)
{
    std::string dir_name = filenames[0];
    if (!dir_name.empty() && dir_name != proposed_name)
    {
        gSavedSettings.setString("FSSoundCacheLocation", dir_name);
        setSoundCacheLocation(dir_name);
        LLNotificationsUtil::add("SoundCacheWillBeMoved");
    }
}

void LLFloaterPreference::onClickBrowseSoundCache()
{
    gViewerWindow->getWindow()->openFile(gDirUtilp->getExpandedFilename(LL_PATH_FS_SOUND_CACHE, ""));
}

void LLFloaterPreference::onClickResetSoundCache()
{
    gSavedSettings.setString("FSSoundCacheLocation", std::string());
    setSoundCacheLocation(std::string());
    LLNotificationsUtil::add("SoundCacheWillBeMoved");
}
// </FS:Ansariel>

// <FS:Ansariel> FIRE-2912: Reset voice button
class FSResetVoiceTimer : public LLEventTimer
{
public:
    FSResetVoiceTimer() : LLEventTimer(5.f) { }
    ~FSResetVoiceTimer() { }

    bool tick()
    {
        gSavedSettings.setBOOL("EnableVoiceChat", true);
        LLFloaterPreference* floater = LLFloaterReg::findTypedInstance<LLFloaterPreference>("preferences");
        if (floater)
        {
            floater->childSetEnabled("enable_voice_check", true);
            floater->childSetEnabled("enable_voice_check_volume", true);
        }
        return true;
    }
};

void LLFloaterPreference::onClickResetVoice()
{
    if (gSavedSettings.getBOOL("EnableVoiceChat") && !gSavedSettings.getBOOL("CmdLineDisableVoice"))
    {
        gSavedSettings.setBOOL("EnableVoiceChat", false);
        childSetEnabled("enable_voice_check", false);
        childSetEnabled("enable_voice_check_volume", false);
        new FSResetVoiceTimer();
    }
}
// </FS:Ansariel>

// Performs a wipe of the local settings dir on next restart
bool callback_clear_settings(const LLSD& notification, const LLSD& response)
{
    S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
    if ( option == 0 ) // YES
    {

        // Create a filesystem marker instructing a full settings wipe
        std::string clear_file_name;
        clear_file_name = gDirUtilp->getExpandedFilename(LL_PATH_LOGS,"CLEAR");
        LL_INFOS() << "Creating clear settings marker file " << clear_file_name << LL_ENDL;

        LLAPRFile clear_file ;
        clear_file.open(clear_file_name, LL_APR_W);
        if (clear_file.getFileHandle())
        {
            LL_INFOS("MarkerFile") << "Created clear settings marker file " << clear_file_name << LL_ENDL;
            clear_file.close();
            LLNotificationsUtil::add("SettingsWillClear");
        }
        else
        {
            LL_WARNS("MarkerFile") << "Cannot clear settings marker file " << clear_file_name << LL_ENDL;
        }

        return true;
    }
    return false;
}

//[ADD - Clear Usersettings : SJ] - When button Reset Defaults is clicked show a warning
void LLFloaterPreference::onClickClearSettings()
{
    LLNotificationsUtil::add("FirestormClearSettingsPrompt",LLSD(), LLSD(), callback_clear_settings);
}

void LLFloaterPreference::onClickChatOnlineNotices()
{
    getChildView("OnlineOfflinetoNearbyChatHistory")->setEnabled(getChild<LLUICtrl>("OnlineOfflinetoNearbyChat")->getValue().asBoolean());
}

void LLFloaterPreference::onClickClearSpamList()
{
    NACLAntiSpamRegistry::instance().purgeAllQueues();
}

void LLFloaterPreference::setPreprocInclude()
{
    std::string cur_name(gSavedSettings.getString("_NACL_PreProcHDDIncludeLocation"));
    std::string proposed_name(cur_name);

    (new LLDirPickerThread(boost::bind(&LLFloaterPreference::changePreprocIncludePath, this, _1, _2), proposed_name))->getFile();
}

void LLFloaterPreference::changePreprocIncludePath(const std::vector<std::string>& filenames, std::string proposed_name)
{
    std::string dir_name = filenames[0];
    if (!dir_name.empty() && dir_name != proposed_name)
    {
        std::string new_top_folder(gDirUtilp->getBaseFileName(dir_name));
        gSavedSettings.setString("_NACL_PreProcHDDIncludeLocation", dir_name);
    }
}

// <FS:LO> FIRE-23606 Reveal path to external script editor in prefernces
void LLFloaterPreference::setExternalEditor()
{
    std::string cur_name(gSavedSettings.getString("ExternalEditor"));
    std::string proposed_name(cur_name);

    LLFilePickerReplyThread::startPicker(boost::bind(&LLFloaterPreference::changeExternalEditorPath, this, _1), LLFilePicker::FFLOAD_EXE, false);
}

void LLFloaterPreference::changeExternalEditorPath(const std::vector<std::string>& filenames)
{
    const std::string chosen_path = filenames[0];
    std::string executable_path = chosen_path;
#if LL_DARWIN
    // on Mac, if it's an application bundle, figure out the actual path from the Info.plist file
    CFStringRef path_cfstr = CFStringCreateWithCString(kCFAllocatorDefault, chosen_path.c_str(), kCFStringEncodingMacRoman);        // get path as a CFStringRef
    CFURLRef path_url = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, path_cfstr, kCFURLPOSIXPathStyle, true);         // turn it into a CFURLRef
    CFBundleRef chosen_bundle = CFBundleCreate(kCFAllocatorDefault, path_url);                                              // get a handle for the bundle
    CFRelease(path_url);    // [FS:CR] Don't leave a mess clean up our objects after we use them
    LLSD args;
    if (NULL != chosen_bundle)
    {
        CFDictionaryRef bundleInfoDict = CFBundleGetInfoDictionary(chosen_bundle);                                              // get the bundle's dictionary
        CFRelease(chosen_bundle);   // [FS:CR] Don't leave a mess clean up our objects after we use them
        if (NULL != bundleInfoDict)
        {
            CFStringRef executable_cfstr = (CFStringRef)CFDictionaryGetValue(bundleInfoDict, CFSTR("CFBundleExecutable"));  // get the name of the actual executable (e.g. TextEdit or firefox-bin)
            int max_file_length = 256;                                                                                      // (max file name length is 255 in OSX)
            char executable_buf[max_file_length];
            if (CFStringGetCString(executable_cfstr, executable_buf, max_file_length, kCFStringEncodingMacRoman))           // convert CFStringRef to char*
            {
                executable_path += std::string("/Contents/MacOS/") + std::string(executable_buf);                           // append path to executable directory and then executable name to exec path
            }
            else
            {
                std::string warning = "Unable to get CString from CFString for executable path";
                LL_WARNS() << warning << LL_ENDL;
                args["MESSAGE"] = warning;
                LLNotificationsUtil::add("GenericAlert", args);
            }
        }
        else
        {
            std::string warning = "Unable to get bundle info dictionary from application bundle";
            LL_WARNS() << warning << LL_ENDL;
            args["MESSAGE"] = warning;
            LLNotificationsUtil::add("GenericAlert", args);
        }
    }
    else
    {
        if (-1 != executable_path.find(".app")) // only warn if this path actually had ".app" in it, i.e. it probably just wasn'nt an app bundle and that's okay
        {
            std::string warning = std::string("Unable to get bundle from path \"") + chosen_path + std::string("\"");
            LL_WARNS() << warning << LL_ENDL;
            args["MESSAGE"] = warning;
            LLNotificationsUtil::add("GenericAlert", args);
        }
    }

#endif
    {
        std::string bin = executable_path;
        if (!bin.empty())
        {
            // surround command with double quotes for the case if the path contains spaces
            if (bin.find("\"") == std::string::npos)
            {
                bin = "\"" + bin + "\"";
            }
            executable_path = bin;
        }
    }
    gSavedSettings.setString("ExternalEditor", executable_path);
}
// </FS:LO>

//[FIX JIRA-1971 : SJ] Show an notify when Javascript setting change
void LLFloaterPreference::onClickJavascript()
{
    if (!gSavedSettings.getBOOL("BrowserJavascriptEnabled"))
    {
        LLNotificationsUtil::add("DisableJavascriptBreaksSearch");
    }
}

/*
void LLFloaterPreference::onClickSkin(LLUICtrl* ctrl, const LLSD& userdata)
{
    gSavedSettings.setString("SkinCurrent", userdata.asString());
    ctrl->setValue(userdata.asString());
}

void LLFloaterPreference::onSelectSkin()
{
    std::string skin_selection = getChild<LLRadioGroup>("skin_selection")->getValue().asString();
    gSavedSettings.setString("SkinCurrent", skin_selection);
}

void LLFloaterPreference::refreshSkin(void* data)
{
    LLPanel*self = (LLPanel*)data;
    sSkin = gSavedSettings.getString("SkinCurrent");
    self->getChild<LLRadioGroup>("skin_selection", true)->setValue(sSkin);
}
*/

// <FS:Zi> FIRE-19539 - Include the alert messages in Prefs>Notifications>Alerts in preference Search.
// void LLFloaterPreference::buildPopupLists()
// {
//  mDisabledPopups.deleteAllItems();
//  mEnabledPopups.deleteAllItems();
//
//  for (LLNotifications::TemplateMap::const_iterator iter = LLNotifications::instance().templatesBegin();
//       iter != LLNotifications::instance().templatesEnd();
//       ++iter)
//  {
//      LLNotificationTemplatePtr templatep = iter->second;
//      LLNotificationFormPtr formp = templatep->mForm;
//
//      LLNotificationForm::EIgnoreType ignore = formp->getIgnoreType();
//      if (ignore <= LLNotificationForm::IGNORE_NO)
//          continue;
//
//      LLSD row;
//      row["columns"][0]["value"] = formp->getIgnoreMessage();
//      row["columns"][0]["font"] = "SANSSERIF_SMALL";
//      row["columns"][0]["width"] = 400;
//
//      LLScrollListItem* item = NULL;
//
//      bool show_popup = !formp->getIgnored();
//      if (!show_popup)
//      {
//          if (ignore == LLNotificationForm::IGNORE_WITH_LAST_RESPONSE)
//          {
//              // <FS:Ansariel> Default responses are declared in "ignores" settings group, see llnotifications.cpp
//              //LLSD last_response = LLUI::getInstance()->mSettingGroups["config"]->getLLSD("Default" + templatep->mName);
//              LLSD last_response = LLUI::getInstance()->mSettingGroups["ignores"]->getLLSD("Default" + templatep->mName);
//              // </FS:Ansariel>
//              if (!last_response.isUndefined())
//              {
//                  for (LLSD::map_const_iterator it = last_response.beginMap();
//                       it != last_response.endMap();
//                       ++it)
//                  {
//                      if (it->second.asBoolean())
//                      {
//                          row["columns"][1]["value"] = formp->getElement(it->first)["ignore"].asString();
//                          row["columns"][1]["font"] = "SANSSERIF_SMALL";
//                          row["columns"][1]["width"] = 360;
//                          break;
//                      }
//                  }
//              }
//          }
//          item = mDisabledPopups.addElement(row);
//      }
//      else
//      {
//          item = mEnabledPopups.addElement(row);
//      }
//
//      if (item)
//      {
//          item->setUserdata((void*)&iter->first);
//      }
//  }
// }

void LLFloaterPreference::buildPopupList()
{
    mPopupList->deleteAllItems();

    for (LLNotifications::TemplateMap::const_iterator iter = LLNotifications::instance().templatesBegin();
         iter != LLNotifications::instance().templatesEnd();
         ++iter)
    {
        LLNotificationTemplatePtr templatep = iter->second;
        LLNotificationFormPtr formp = templatep->mForm;

        LLNotificationForm::EIgnoreType ignore = formp->getIgnoreType();
        if (ignore <= LLNotificationForm::IGNORE_NO)
            continue;

        bool show_popup = !formp->getIgnored();

        LLSD row;

        // column COLUMN_POPUP_SPACER makes things look good, since "halign" and "center" or LLFontGL::HCENTER don't work -Zi
        row["columns"][COLUMN_POPUP_CHECKBOX]["type"] = "checkbox";
        row["columns"][COLUMN_POPUP_CHECKBOX]["column"] = "alert_enabled_check";
        row["columns"][COLUMN_POPUP_CHECKBOX]["value"] = show_popup;
        row["columns"][COLUMN_POPUP_LABEL]["column"] = "alert_label";
        row["columns"][COLUMN_POPUP_LABEL]["value"] = formp->getIgnoreMessage();

        LLScrollListItem* item = mPopupList->addElement(row);

        if (item)
        {
            item->setUserdata((void*)&iter->first);
        }
    }
}

void LLFloaterPreference::onSelectPopup()
{
    LLScrollListItem* last = mPopupList->getLastSelectedItem();
    for (auto popup : mPopupList->getAllSelected())
    {
        LLNotificationTemplatePtr templatep = LLNotifications::instance().getTemplate(*(std::string*)(popup->getUserdata()));
        std::string notification_name = templatep->mName;
        LLUI::getInstance()->mSettingGroups["ignores"]->setBOOL(notification_name, last->getColumn(COLUMN_POPUP_CHECKBOX)->getValue().asBoolean());
    }
}

void LLFloaterPreference::onUpdatePopupFilter()
{
    mPopupList->setFilterString(mPopupFilter->getValue().asString());
}
// <FS:Zi>

void LLFloaterPreference::refreshEnabledState()
{
#if ADDRESS_SIZE == 32
    childSetEnabled("FSRestrictMaxTextureSize", false);
#endif

    if (!LLFeatureManager::getInstance()->isFeatureAvailable("RenderCompressTextures"))
    {
        getChildView("texture compression")->setEnabled(false);
    }

    // anti-aliasing
    LLUICtrl* fsaa_ctrl = getChild<LLUICtrl>("fsaa");

    // Enable or disable the control, the "Antialiasing:" label and the restart warning
    // based on code support for the feature on the current hardware.

    if (gPipeline.canUseAntiAliasing())
    {
        fsaa_ctrl->setEnabled(true);
    }
    else
    {
        fsaa_ctrl->setEnabled(false);
        fsaa_ctrl->setValue((LLSD::Integer) 0);
    }

    if (!LLFeatureManager::instance().isFeatureAvailable("RenderFSAASamples"))
    {
        fsaa_ctrl->setEnabled(false);
    }

    // WindLight
    LLSliderCtrl* sky = getChild<LLSliderCtrl>("SkyMeshDetail");
    sky->setEnabled(true);

    LLCheckBoxCtrl* ctrl_ssao = getChild<LLCheckBoxCtrl>("UseSSAO");
    LLCheckBoxCtrl* ctrl_dof = getChild<LLCheckBoxCtrl>("UseDoF");
    LLComboBox* ctrl_shadow = getChild<LLComboBox>("ShadowDetail");

    bool enabled = LLFeatureManager::getInstance()->isFeatureAvailable("RenderDeferredSSAO");

    ctrl_ssao->setEnabled(enabled);
    ctrl_dof->setEnabled(enabled);

    enabled = enabled && LLFeatureManager::getInstance()->isFeatureAvailable("RenderShadowDetail");

    ctrl_shadow->setEnabled(enabled);

    enabled = false;
    if (!LLFeatureManager::instance().isFeatureAvailable("RenderReflectionsEnabled"))
    {
        // getChildView("ReflectionsEnabled")->setEnabled(false);
    }
    else
    {
        enabled = gSavedSettings.getBOOL("RenderReflectionsEnabled");
    }
    getChildView("ReflectionDetail")->setEnabled(enabled);
    getChildView("ReflectionLevel")->setEnabled(enabled);
    //getChildView("ReflectionDetailText")->setEnabled(enabled);
    getChildView("ScreenSpaceReflections")->setEnabled(enabled);

    // now turn off any features that are unavailable
    disableUnavailableSettings();

    // Cannot have floater active until caps have been received
    //getChild<LLButton>("default_creation_permissions")->setEnabled(LLStartUp::getStartupState() >= STATE_STARTED);
    getChild<LLButton>("fs_default_creation_permissions")->setEnabled(LLStartUp::getStartupState() >= STATE_STARTED);

    getChildView("block_list")->setEnabled(LLLoginInstance::getInstance()->authSuccess());

    // [RLVa:KB] - Checked: 2013-05-11 (RLVa-1.4.9)
    if (RlvActions::isRlvEnabled())
    {
        getChild<LLUICtrl>("do_not_disturb_response")->setEnabled(!RlvActions::hasBehaviour(RLV_BHVR_SENDIM));
    }
    // [/RLVa:KB]

    // <FS:Ansariel> Expose max texture VRAM setting
    auto max_texmem = getChild<LLSliderCtrl>("RenderMaxVRAMBudget");
    max_texmem->setMinValue(MIN_VRAM_BUDGET);
    max_texmem->setMaxValue((F32)gGLManager.mVRAM);
    // </FS:Ansariel>
    // <FS:minerjr> [FIRE-35198] Limit VRAM texture usage UI control reverts to default value
    static LLCachedControl<U32> max_vram_budget(gSavedSettings, "RenderMaxVRAMBudget", 0); // Get the same VRAM value that is used for the Bias calcuation
    // Set the value of the UI element on after loggin in. (The value was correct and applied correctly, just the Graphics Settings slider defaulted backe to 4096
    // instead of the last set value by the user.
    max_texmem->setValue((F32)max_vram_budget, false);
    // </FS:minerjr> [FIRE-35198]
}

// <FS:Zi> Support preferences search SLURLs
void LLFloaterPreference::onCopySearch()
{
    std::string searchQuery = "secondlife:///app/openfloater/preferences?search=" + LLURI::escape(mFilterEdit->getText());
    LLClipboard::instance().copyToClipboard(utf8str_to_wstring(searchQuery), 0, static_cast<S32>(searchQuery.size()));
}
// </FS:Zi>


// static
void LLAvatarComplexityControls::setIndirectControls()
{
    /*
     * We have controls that have an indirect relationship between the control
     * values and adjacent text and the underlying setting they influence.
     * In each case, the control and its associated setting are named Indirect<something>
     * This method interrogates the controlled setting and establishes the
     * appropriate value for the indirect control. It must be called whenever the
     * underlying setting may have changed other than through the indirect control,
     * such as when the 'Reset all to recommended settings' button is used...
     */
    setIndirectMaxNonImpostors();
    setIndirectMaxArc();
}

// static
void LLAvatarComplexityControls::setIndirectMaxNonImpostors()
{
    U32 max_non_impostors = gSavedSettings.getU32("RenderAvatarMaxNonImpostors");
    // for this one, we just need to make zero, which means off, the max value of the slider
    U32 indirect_max_non_impostors = (0 == max_non_impostors) ? LLVOAvatar::NON_IMPOSTORS_MAX_SLIDER : max_non_impostors;
    gSavedSettings.setU32("IndirectMaxNonImpostors", indirect_max_non_impostors);
}

void LLAvatarComplexityControls::setIndirectMaxArc()
{
    U32 max_arc = gSavedSettings.getU32("RenderAvatarMaxComplexity");
    U32 indirect_max_arc;
    if (0 == max_arc)
    {
        // the off position is all the way to the right, so set to control max
        indirect_max_arc = INDIRECT_MAX_ARC_OFF;
    }
    else
    {
        // This is the inverse of the calculation in updateMaxComplexity
        indirect_max_arc = (U32)ll_round(((log(F32(max_arc)) - MIN_ARC_LOG) / ARC_LIMIT_MAP_SCALE)) + MIN_INDIRECT_ARC_LIMIT;
    }
    gSavedSettings.setU32("IndirectMaxComplexity", indirect_max_arc);
}

void LLFloaterPreference::disableUnavailableSettings()
{
    LLComboBox* ctrl_shadows = getChild<LLComboBox>("ShadowDetail");
    LLCheckBoxCtrl* ctrl_ssao = getChild<LLCheckBoxCtrl>("UseSSAO");
    LLSliderCtrl* cas_slider = getChild<LLSliderCtrl>("RenderSharpness");

    // disabled deferred SSAO
    if (!LLFeatureManager::getInstance()->isFeatureAvailable("RenderDeferredSSAO"))
    {
        ctrl_ssao->setEnabled(false);
        ctrl_ssao->setValue(false);
    }

    // disabled deferred shadows
    if (!LLFeatureManager::getInstance()->isFeatureAvailable("RenderShadowDetail"))
    {
        ctrl_shadows->setEnabled(false);
        ctrl_shadows->setValue(0);
    }

    // Vintage mode
    static LLCachedControl<bool> is_not_vintage(gSavedSettings, "RenderDisableVintageMode");
    LLSliderCtrl* tonemapMix = getChild<LLSliderCtrl>("TonemapMix");
    LLComboBox* tonemapSelect = getChild<LLComboBox>("TonemapType");
    LLTextBox* tonemapLabel = getChild<LLTextBox>("TonemapTypeText");
    LLSliderCtrl* exposureSlider = getChild<LLSliderCtrl>("RenderExposure");

    tonemapSelect->setEnabled(is_not_vintage);
    tonemapLabel->setEnabled(is_not_vintage);
    tonemapMix->setEnabled(is_not_vintage);
    exposureSlider->setEnabled(is_not_vintage);
    cas_slider->setEnabled(is_not_vintage);

}

void LLFloaterPreference::refresh()
{
    LLPanel::refresh();

    // <FS:Ansariel> Improved graphics preferences
    getChild<LLUICtrl>("fsaa")->setValue((LLSD::Integer)  gSavedSettings.getU32("RenderFSAASamples"));
    updateSliderText(getChild<LLSliderCtrl>("RenderPostProcess",    true), getChild<LLTextBox>("PostProcessText",           true));
    LLAvatarComplexityControls::setIndirectControls();
    setMaxNonImpostorsText(gSavedSettings.getU32("RenderAvatarMaxNonImpostors"),getChild<LLTextBox>("IndirectMaxNonImpostorsText", true));
    // </FS:Ansariel>

    LLAvatarComplexityControls::setText(
        gSavedSettings.getU32("RenderAvatarMaxComplexity"),
        getChild<LLTextBox>("IndirectMaxComplexityText", true));

    refreshEnabledState();
    LLFloater* advanced = LLFloaterReg::findTypedInstance<LLFloater>("prefs_graphics_advanced");
    if (advanced)
    {
        advanced->refresh();
    }
    updateClickActionViews();
}

void LLFloaterPreference::onCommitWindowedMode()
{
    refresh();
}

void LLFloaterPreference::onChangeQuality(const LLSD& data)
{
    U32 level = (U32)(data.asReal());
    LLFeatureManager::getInstance()->setGraphicsLevel(level, true);
    refreshEnabledGraphics();
    refresh();
}

//<FS:KC> Handled centrally now
/*
void LLFloaterPreference::onClickSetSounds()
{
    // Disable Enable gesture/collisions sounds checkbox if the master sound is disabled
    // or if sound effects are disabled.
    getChild<LLCheckBoxCtrl>("gesture_audio_play_btn")->setEnabled(!gSavedSettings.getBOOL("MuteSounds"));
    getChild<LLCheckBoxCtrl>("collisions_audio_play_btn")->setEnabled(!gSavedSettings.getBOOL("MuteSounds"));
}
*/

// <FS:PP> FIRE-8190: Preview function for "UI Sounds" Panel
void LLFloaterPreference::onClickPreviewUISound(const LLSD& ui_sound_id)
{
    std::string uisndid = ui_sound_id.asString();
    make_ui_sound(uisndid.c_str(), true);
}
// </FS:PP> FIRE-8190: Preview function for "UI Sounds" Panel

// <FS:Zi> FIRE-19539 - Include the alert messages in Prefs>Notifications>Alerts in preference Search.
// void LLFloaterPreference::onClickEnablePopup()
// {
//  std::vector<LLScrollListItem*> items = mDisabledPopups.getAllSelected();
//  std::vector<LLScrollListItem*>::iterator itor;
//  for (itor = items.begin(); itor != items.end(); ++itor)
//  {
//      LLNotificationTemplatePtr templatep = LLNotifications::instance().getTemplate(*(std::string*)((*itor)->getUserdata()));
//      //gSavedSettings.setWarning(templatep->mName, true);
//      std::string notification_name = templatep->mName;
//      LLUI::getInstance()->mSettingGroups["ignores"]->setBOOL(notification_name, true);
//  }
//
//  buildPopupLists();
//    if (!mFilterEdit->getText().empty())
//    {
//        filterIgnorableNotifications();
//    }
// }

// void LLFloaterPreference::onClickDisablePopup()
// {
//  std::vector<LLScrollListItem*> items = mEnabledPopups.getAllSelected();
//  std::vector<LLScrollListItem*>::iterator itor;
//  for (itor = items.begin(); itor != items.end(); ++itor)
//  {
//      LLNotificationTemplatePtr templatep = LLNotifications::instance().getTemplate(*(std::string*)((*itor)->getUserdata()));
//      templatep->mForm->setIgnored(true);
//  }
//
//  buildPopupLists();
//    if (!mFilterEdit->getText().empty())
//    {
//        filterIgnorableNotifications();
//    }
// }
// </FS:Zi>

void LLFloaterPreference::resetAllIgnored()
{
    for (LLNotifications::TemplateMap::const_iterator iter = LLNotifications::instance().templatesBegin();
         iter != LLNotifications::instance().templatesEnd();
         ++iter)
    {
        if (iter->second->mForm->getIgnoreType() > LLNotificationForm::IGNORE_NO)
        {
            iter->second->mForm->setIgnored(false);
        }
    }
}

void LLFloaterPreference::setAllIgnored()
{
    for (LLNotifications::TemplateMap::const_iterator iter = LLNotifications::instance().templatesBegin();
         iter != LLNotifications::instance().templatesEnd();
         ++iter)
    {
        if (iter->second->mForm->getIgnoreType() > LLNotificationForm::IGNORE_NO)
        {
            iter->second->mForm->setIgnored(true);
        }
    }
}

void LLFloaterPreference::onClickLogPath()
{
    std::string proposed_name(gSavedPerAccountSettings.getString("InstantMessageLogPath"));
    mPriorInstantMessageLogPath.clear();


    (new LLDirPickerThread(boost::bind(&LLFloaterPreference::changeLogPath, this, _1, _2), proposed_name))->getFile();
}

void LLFloaterPreference::changeLogPath(const std::vector<std::string>& filenames, std::string proposed_name)
{
    //Path changed
    if (proposed_name != filenames[0])
    {
        gSavedPerAccountSettings.setString("InstantMessageLogPath", filenames[0]);
        mPriorInstantMessageLogPath = proposed_name;

        // enable/disable 'Delete transcripts button
        updateDeleteTranscriptsButton();
    }
    //[FIX FIRE-2765 : SJ] Enable Reset button when own Chatlogdirectory is set
    getChildView("reset_logpath")->setEnabled(true);
}

//[FIX FIRE-2765 : SJ] Making sure Reset button resets the chatlogdirectory to the default setting
void LLFloaterPreference::onClickResetLogPath()
{
    // <FS:Ansariel> FIRE-12955: Logs don't get moved when clicking reset log path button
    //gDirUtilp->setChatLogsDir(gDirUtilp->getOSUserAppDir());
    //gSavedPerAccountSettings.setString("InstantMessageLogPath", gDirUtilp->getChatLogsDir());

    mPriorInstantMessageLogPath = gDirUtilp->getChatLogsDir();
    gSavedPerAccountSettings.setString("InstantMessageLogPath", gDirUtilp->getOSUserAppDir());

    // enable/disable 'Delete transcripts button
    updateDeleteTranscriptsButton();

    getChildView("reset_logpath")->setEnabled(false);
    // </FS:Ansariel>
}

bool LLFloaterPreference::moveTranscriptsAndLog()
{
    std::string instantMessageLogPath(gSavedPerAccountSettings.getString("InstantMessageLogPath"));
    std::string chatLogPath = gDirUtilp->add(instantMessageLogPath, gDirUtilp->getUserName());

    bool madeDirectory = false;

    //Does the directory really exist, if not then make it
    if(!LLFile::isdir(chatLogPath))
    {
        //mkdir success is defined as zero
        if(LLFile::mkdir(chatLogPath) != 0)
        {
            return false;
        }
        madeDirectory = true;
    }

    std::string originalConversationLogDir = LLConversationLog::instance().getFileName();
    std::string targetConversationLogDir = gDirUtilp->add(chatLogPath, "conversation.log");
    //Try to move the conversation log
    if(!LLConversationLog::instance().moveLog(originalConversationLogDir, targetConversationLogDir))
    {
        //Couldn't move the log and created a new directory so remove the new directory
        if(madeDirectory)
        {
            LLFile::rmdir(chatLogPath);
        }
        return false;
    }

    //Attempt to move transcripts
    std::vector<std::string> listOfTranscripts;
    std::vector<std::string> listOfFilesMoved;

    LLLogChat::getListOfTranscriptFiles(listOfTranscripts);

    if(!LLLogChat::moveTranscripts(gDirUtilp->getChatLogsDir(),
                                    instantMessageLogPath,
                                    listOfTranscripts,
                                    listOfFilesMoved))
    {
        //Couldn't move all the transcripts so restore those that moved back to their old location
        LLLogChat::moveTranscripts(instantMessageLogPath,
            gDirUtilp->getChatLogsDir(),
            listOfFilesMoved);

        //Move the conversation log back
        LLConversationLog::instance().moveLog(targetConversationLogDir, originalConversationLogDir);

        if(madeDirectory)
        {
            LLFile::rmdir(chatLogPath);
        }

        return false;
    }

    gDirUtilp->setChatLogsDir(instantMessageLogPath);
    gDirUtilp->updatePerAccountChatLogsDir();

    return true;
}

// <FS:Ansariel> Show email address in preferences (FIRE-1071) and keep it for OpenSim
//void LLFloaterPreference::setPersonalInfo(const std::string& visibility)
void LLFloaterPreference::setPersonalInfo(const std::string& visibility, bool im_via_email, const std::string& email)
// </FS:Ansariel> Show email address in preferences (FIRE-1071)
{
    mGotPersonalInfo = true;
    // <FS:Ansariel> Keep this for OpenSim
    mOriginalIMViaEmail = im_via_email;
    mDirectoryVisibility = visibility;

    if (visibility == VISIBILITY_DEFAULT)
    {
        mOriginalHideOnlineStatus = false;
        getChildView("online_visibility")->setEnabled(true);
    }
    else if (visibility == VISIBILITY_HIDDEN)
    {
        mOriginalHideOnlineStatus = true;
        getChildView("online_visibility")->setEnabled(true);
    }
    else
    {
        mOriginalHideOnlineStatus = true;
    }

    getChild<LLUICtrl>("online_searchresults")->setEnabled(true);
    getChildView("friends_online_notify_checkbox")->setEnabled(true);
    getChild<LLUICtrl>("online_visibility")->setValue(mOriginalHideOnlineStatus);
    getChild<LLUICtrl>("online_visibility")->setLabelArg("[DIR_VIS]", mDirectoryVisibility);
    getChildView("favorites_on_login_check")->setEnabled(true);
    //getChildView("log_path_button")->setEnabled(true); // <FS:Ansariel> Does not exist as of 12-09-2014
    getChildView("chat_font_size")->setEnabled(true);
    //getChildView("open_log_path_button")->setEnabled(true); // <FS:Ansariel> Does not exist as of 12-09-2014
    getChildView("log_path_button-panelsetup")->setEnabled(true);// second set of controls for panel_preferences_setup  -WoLf
    getChildView("open_log_path_button-panelsetup")->setEnabled(true);
    std::string Chatlogsdir = gDirUtilp->getOSUserAppDir();

    getChildView("conversation_log_combo")->setEnabled(true);   // <FS:CR>
    getChildView("LogNearbyChat")->setEnabled(true);    // <FS:CR>
    //getChildView("log_nearby_chat")->setEnabled(true); // <FS:Ansariel> Does not exist as of 12-09-2014
    //[FIX FIRE-2765 : SJ] Set Chatlog Reset Button on enabled when Chatlogpath isn't the default folder
    if (gSavedPerAccountSettings.getString("InstantMessageLogPath") != gDirUtilp->getOSUserAppDir())
    {
        getChildView("reset_logpath")->setEnabled(true);
    }
    // <FS:Ansariel> Keep this for OpenSim
    if (LLGridManager::instance().isInSecondLife())
    {
        childSetEnabled("email_settings", true);
        childSetVisible("email_settings", true);
    }
    else
    {
        std::string display_email(email);
        if (display_email.size() > 30)
        {
            display_email.resize(30);
            display_email += "...";
        }

        LLCheckBoxCtrl* send_im_to_email = getChild<LLCheckBoxCtrl>("send_im_to_email");
        send_im_to_email->setVisible(true);
        send_im_to_email->setEnabled(true);
        send_im_to_email->setValue(im_via_email);
        send_im_to_email->setLabelArg("[EMAIL]", display_email);
    }
    childSetVisible("email_settings_login_to_change", false);
    // </FS:Ansariel>

    // <FS:Ansariel> FIRE-420: Show end of last conversation in history
    getChildView("LogShowHistory")->setEnabled(true);

    // <FS:Ansariel> Clear inventory cache button
    getChildView("ClearInventoryCache")->setEnabled(true);

    // <FS:Ansariel> FIRE-18250: Option to disable default eye movement
    getChildView("FSStaticEyes")->setEnabled(true);

    // <FS:Ansariel> FIRE-22564: Route llOwnerSay to scipt debug window
    getChildView("FSllOwnerSayToScriptDebugWindow_checkbox")->setEnabled(true);

    // <FS:Ansariel> Clear Cache button actually clears per-account cache items
    getChildView("clear_webcache")->setEnabled(true);

    getChild<LLUICtrl>("voice_call_friends_only_check")->setEnabled(true);
    getChild<LLUICtrl>("voice_call_friends_only_check")->setValue(gSavedPerAccountSettings.getBOOL("VoiceCallsFriendsOnly"));
}


void LLFloaterPreference::refreshUI()
{
    refresh();
}

// <FS:Ansariel> Improved graphics preferences
void LLFloaterPreference::updateSliderText(LLSliderCtrl* ctrl, LLTextBox* text_box)
{
    if (text_box == NULL || ctrl== NULL)
        return;

    // get range and points when text should change
    F32 value = (F32)ctrl->getValue().asReal();
    F32 min = ctrl->getMinValue();
    F32 max = ctrl->getMaxValue();
    F32 range = max - min;
    llassert(range > 0);
    F32 midPoint = min + range / 3.0f;
    F32 highPoint = min + (2.0f * range / 3.0f);

    // choose the right text
    if (value < midPoint)
    {
        text_box->setText(LLTrans::getString("GraphicsQualityLow"));
    }
    else if (value < highPoint)
    {
        text_box->setText(LLTrans::getString("GraphicsQualityMid"));
    }
    else
    {
        text_box->setText(LLTrans::getString("GraphicsQualityHigh"));
    }
}

void LLFloaterPreference::updateMaxNonImpostors()
{
    // Called when the IndirectMaxNonImpostors control changes
    // Responsible for fixing the slider label (IndirectMaxNonImpostorsText) and setting RenderAvatarMaxNonImpostors
    LLSliderCtrl* ctrl = getChild<LLSliderCtrl>("IndirectMaxNonImpostors",true);
    U32 value = ctrl->getValue().asInteger();

    if (0 == value || LLVOAvatar::NON_IMPOSTORS_MAX_SLIDER <= value)
    {
        value=0;
    }
    gSavedSettings.setU32("RenderAvatarMaxNonImpostors", value);
    LLVOAvatar::updateImpostorRendering(value); // make it effective immediately
    setMaxNonImpostorsText(value, getChild<LLTextBox>("IndirectMaxNonImpostorsText"));
}

void LLFloaterPreference::setMaxNonImpostorsText(U32 value, LLTextBox* text_box)
{
    if (0 == value)
    {
        text_box->setText(LLTrans::getString("no_limit"));
    }
    else
    {
        text_box->setText(llformat("%d", value));
    }
}

void LLFloaterPreference::updateMaxNonImpostorsLabel(const LLSD& newvalue)
{
    U32 value = newvalue.asInteger();

    if (0 == value || LLVOAvatar::NON_IMPOSTORS_MAX_SLIDER <= value)
    {
        value=0;
    }
    setMaxNonImpostorsText(value, getChild<LLTextBox>("IndirectMaxNonImpostorsText"));
}

void LLFloaterPreference::updateMaxComplexityLabel(const LLSD& newvalue)
{
    U32 value = newvalue.asInteger();

    LLAvatarComplexityControls::setText(value, getChild<LLTextBox>("IndirectMaxComplexityText"));
}
// </FS:Ansariel>

void LLAvatarComplexityControls::updateMax(LLSliderCtrl* slider, LLTextBox* value_label, bool short_val)
{
    // Called when the IndirectMaxComplexity control changes
    // Responsible for fixing the slider label (IndirectMaxComplexityText) and setting RenderAvatarMaxComplexity
    U32 indirect_value = slider->getValue().asInteger();
    U32 max_arc;

    if (INDIRECT_MAX_ARC_OFF == indirect_value)
    {
        // The 'off' position is when the slider is all the way to the right,
        // which is a value of INDIRECT_MAX_ARC_OFF,
        // so it is necessary to set max_arc to 0 disable muted avatars.
        max_arc = 0;
    }
    else
    {
        // if this is changed, the inverse calculation in setIndirectMaxArc
        // must be changed to match
        max_arc = (U32)ll_round(exp(MIN_ARC_LOG + (ARC_LIMIT_MAP_SCALE * (indirect_value - MIN_INDIRECT_ARC_LIMIT))));
    }

    gSavedSettings.setU32("RenderAvatarMaxComplexity", (U32)max_arc);
    setText(max_arc, value_label, short_val);
}

void LLAvatarComplexityControls::setText(U32 value, LLTextBox* text_box, bool short_val)
{
    if (0 == value)
    {
        text_box->setText(LLTrans::getString("no_limit"));
    }
    else
    {
        // <FS:Ansariel> Proper number formatting with delimiter
        //std::string text_value = short_val ? llformat("%d", value / 1000) : llformat("%d", value);
        //text_box->setText(text_value);
        std::string output_string;
        LLLocale locale("");
        LLResMgr::getInstance()->getIntegerString(output_string, (short_val ? value / 1000 : value));
        text_box->setText(output_string);
    }
}

void LLAvatarComplexityControls::updateMaxRenderTime(LLSliderCtrl* slider, LLTextBox* value_label, bool short_val)
{
    setRenderTimeText((F32)(LLPerfStats::renderAvatarMaxART_ns/1000), value_label, short_val);
}

void LLAvatarComplexityControls::setRenderTimeText(F32 value, LLTextBox* text_box, bool short_val)
{
    if (0 == value)
    {
        text_box->setText(LLTrans::getString("no_limit"));
    }
    else
    {
        text_box->setText(llformat("%.0f", value));
    }
}

void LLFloaterPreference::updateMaxComplexity()
{
    // Called when the IndirectMaxComplexity control changes
    LLAvatarComplexityControls::updateMax(
        getChild<LLSliderCtrl>("IndirectMaxComplexity"),
        getChild<LLTextBox>("IndirectMaxComplexityText"));
}

void LLFloaterPreference::updateComplexityText()
{
    LLAvatarComplexityControls::setText(gSavedSettings.getU32("RenderAvatarMaxComplexity"),
        getChild<LLTextBox>("IndirectMaxComplexityText", true));
}

bool LLFloaterPreference::loadFromFilename(const std::string& filename, std::map<std::string, std::string> &label_map)
{
    LLXMLNodePtr root;

    if (!LLXMLNode::parseFile(filename, root, NULL))
    {
        LL_WARNS("Preferences") << "Unable to parse file " << filename << LL_ENDL;
        return false;
    }

    if (!root->hasName("labels"))
    {
        LL_WARNS("Preferences") << filename << " is not a valid definition file" << LL_ENDL;
        return false;
    }

    LabelTable params;
    LLXUIParser parser;
    parser.readXUI(root, params, filename);

    if (params.validateBlock())
    {
        for (LLInitParam::ParamIterator<LabelDef>::const_iterator it = params.labels.begin();
            it != params.labels.end();
            ++it)
        {
            LabelDef label_entry = *it;
            label_map[label_entry.name] = label_entry.value;
        }
    }
    else
    {
        LL_WARNS("Preferences") << filename << " failed to load" << LL_ENDL;
        return false;
    }

    return true;
}

void LLFloaterPreference::onChangeMaturity()
{
    U8 sim_access = gSavedSettings.getU32("PreferredMaturity");

    getChild<LLIconCtrl>("rating_icon_general")->setVisible(sim_access == SIM_ACCESS_PG
                                                            || sim_access == SIM_ACCESS_MATURE
                                                            || sim_access == SIM_ACCESS_ADULT);

    getChild<LLIconCtrl>("rating_icon_moderate")->setVisible(sim_access == SIM_ACCESS_MATURE
                                                            || sim_access == SIM_ACCESS_ADULT);

    getChild<LLIconCtrl>("rating_icon_adult")->setVisible(sim_access == SIM_ACCESS_ADULT);
}

void LLFloaterPreference::onChangeComplexityMode(const LLSD& newvalue)
{
    bool enable_complexity = newvalue.asInteger() != LLVOAvatar::AV_RENDER_ONLY_SHOW_FRIENDS;
    getChild<LLSliderCtrl>("IndirectMaxComplexity")->setEnabled(enable_complexity);
}

std::string get_category_path(LLFolderType::EType cat_type)
{
    LLUUID cat_id = gInventory.findUserDefinedCategoryUUIDForType(cat_type);
    return get_category_path(cat_id);
}

void LLFloaterPreference::onChangeModelFolder()
{
    if (gInventory.isInventoryUsable())
    {
        getChild<LLTextBox>("upload_models")->setText(get_category_path(LLFolderType::FT_OBJECT));
    }
}

void LLFloaterPreference::onChangePBRFolder()
{
    if (gInventory.isInventoryUsable())
    {
        getChild<LLTextBox>("upload_pbr")->setText(get_category_path(LLFolderType::FT_MATERIAL));
    }
}

void LLFloaterPreference::onChangeTextureFolder()
{
    if (gInventory.isInventoryUsable())
    {
        getChild<LLTextBox>("upload_textures")->setText(get_category_path(LLFolderType::FT_TEXTURE));
    }
}

void LLFloaterPreference::onChangeSoundFolder()
{
    if (gInventory.isInventoryUsable())
    {
        getChild<LLTextBox>("upload_sounds")->setText(get_category_path(LLFolderType::FT_SOUND));
    }
}

void LLFloaterPreference::onChangeAnimationFolder()
{
    if (gInventory.isInventoryUsable())
    {
        getChild<LLTextBox>("upload_animation")->setText(get_category_path(LLFolderType::FT_ANIMATION));
    }
}

// FIXME: this will stop you from spawning the sidetray from preferences dialog on login screen
// but the UI for this will still be enabled
void LLFloaterPreference::onClickBlockList()
{
    // </FS:Ansariel> Optional standalone blocklist floater
    //LLFloaterSidePanelContainer::showPanel("people", "panel_people",
    //  LLSD().with("people_panel_tab_name", "blocked_panel"));
    bool saved_setting = gSavedSettings.getBOOL("FSDisableBlockListAutoOpen");
    gSavedSettings.setBOOL("FSDisableBlockListAutoOpen", false);
    LLPanelBlockedList::showPanelAndSelect();
    gSavedSettings.setBOOL("FSDisableBlockListAutoOpen", saved_setting);
    // </FS:Ansariel>
}

void LLFloaterPreference::onClickProxySettings()
{
    LLFloaterReg::showInstance("prefs_proxy");
}

void LLFloaterPreference::onClickTranslationSettings()
{
    LLFloaterReg::showInstance("prefs_translation");
}

void LLFloaterPreference::onClickAutoReplace()
{
    LLFloaterReg::showInstance("prefs_autoreplace");
}

void LLFloaterPreference::onClickSpellChecker()
{
    LLFloaterReg::showInstance("prefs_spellchecker");
}

void LLFloaterPreference::onClickRenderExceptions()
{
    LLFloaterReg::showInstance("avatar_render_settings");
}

// <FS:Beq> Not currently used in FS
// void LLFloaterPreference::onClickAutoAdjustments()
// {
//     LLFloaterPerformance* performance_floater = LLFloaterReg::showTypedInstance<LLFloaterPerformance>("performance");
//     if (performance_floater)
//     {
//         performance_floater->showAutoadjustmentsPanel();
//     }
// }
// </FS:Beq>

void LLFloaterPreference::onClickAdvanced()
{
    LLFloaterReg::showInstance("prefs_graphics_advanced");

    LLTabContainer* tabcontainer = getChild<LLTabContainer>("pref core");
    for (child_list_t::const_iterator iter = tabcontainer->getChildList()->begin();
         iter != tabcontainer->getChildList()->end(); ++iter)
    {
        LLView* view = *iter;
        LLPanelPreferenceGraphics* panel = dynamic_cast<LLPanelPreferenceGraphics*>(view);
        if (panel)
        {
            panel->resetDirtyChilds();
        }
    }
}

void LLFloaterPreference::onClickActionChange()
{
    updateClickActionControls();
}

void LLFloaterPreference::onAtmosShaderChange()
{
    LLCheckBoxCtrl* ctrl_alm = getChild<LLCheckBoxCtrl>("UseLightShaders");
    if(ctrl_alm)
    {
        //Deferred/SSAO/Shadows
        bool bumpshiny = LLCubeMap::sUseCubeMaps && LLFeatureManager::getInstance()->isFeatureAvailable("RenderObjectBump") && gSavedSettings.getBOOL("RenderObjectBump");
        bool shaders = gSavedSettings.getBOOL("WindLightUseAtmosShaders");
        bool enabled = LLFeatureManager::getInstance()->isFeatureAvailable("RenderDeferred") &&
                        bumpshiny &&
                        shaders;

        ctrl_alm->setEnabled(enabled);
    }
}

void LLFloaterPreference::onClickPermsDefault()
{
    LLFloaterReg::showInstance("perms_default");
}

void LLFloaterPreference::onClickRememberedUsernames()
{
    LLFloaterReg::showInstance("forget_username");
}

void LLFloaterPreference::onDeleteTranscripts()
{
    LLSD args;
    args["FOLDER"] = gDirUtilp->getUserName();

    LLNotificationsUtil::add("PreferenceChatDeleteTranscripts", args, LLSD(), boost::bind(&LLFloaterPreference::onDeleteTranscriptsResponse, this, _1, _2));
}

void LLFloaterPreference::onDeleteTranscriptsResponse(const LLSD& notification, const LLSD& response)
{
    if (0 == LLNotificationsUtil::getSelectedOption(notification, response))
    {
        LLLogChat::deleteTranscripts();
        updateDeleteTranscriptsButton();
    }
}

void LLFloaterPreference::onLogChatHistorySaved()
{
    if (!mDeleteTranscriptsBtn->getEnabled())
    {
        mDeleteTranscriptsBtn->setEnabled(true);
    }
}

// <FS:PP> Load UI Sounds tabs settings
void LLFloaterPreference::updateUISoundsControls()
{
    getChild<LLComboBox>("PlayModeUISndNewIncomingIMSession")->setValue((int)gSavedSettings.getU32("PlayModeUISndNewIncomingIMSession")); // 0, 1, 2, 3. Shared with Chat > Notifications > "When receiving Instant Messages"
    getChild<LLComboBox>("PlayModeUISndNewIncomingGroupIMSession")->setValue((int)gSavedSettings.getU32("PlayModeUISndNewIncomingGroupIMSession")); // 0, 1, 2, 3. Shared with Chat > Notifications > "When receiving Group Instant Messages"
    getChild<LLComboBox>("PlayModeUISndNewIncomingConfIMSession")->setValue((int)gSavedSettings.getU32("PlayModeUISndNewIncomingConfIMSession")); // 0, 1, 2, 3. Shared with Chat > Notifications > "When receiving AdHoc Instant Messages"
#ifdef OPENSIM
    // <FS:Beq> OpenSim has option to not attenuate nearby local voice by distance
    auto earPosGroup = findChild<LLRadioGroup>("ear_location");
    if (earPosGroup)
    {
        // It seems there is no better way to do this than with magic numbers short of importing the enums in vivoxvoice (which aren't necessarily the same thing).
        // Index 2 here is the opensim only option to hear nearby chat without attenuation.
        constexpr int hearNearbyVoiceFullVolume{2};
        earPosGroup->setIndexEnabled(hearNearbyVoiceFullVolume, LLGridManager::instance().isInOpenSim());
    }
    // <FS:Beq>
    getChild<LLTextBox>("textFSRestartOpenSim")->setVisible(true);
    getChild<LLLineEditor>("UISndRestartOpenSim")->setVisible(true);
    getChild<LLButton>("Prev_UISndRestartOpenSim")->setVisible(true);
    getChild<LLButton>("Def_UISndRestartOpenSim")->setVisible(true);
    getChild<LLCheckBoxCtrl>("PlayModeUISndRestartOpenSim")->setVisible(true);
#endif
    getChild<LLComboBox>("UseLSLFlightAssist")->setValue((int)gSavedPerAccountSettings.getF32("UseLSLFlightAssist")); // Flight Assist combo box; Not sound-related, but better to place it here instead of creating whole new void

    // FIRE-9856: Mute sound effects disable plays sound from collisions and plays sound from gestures checkbox not disable after restart/relog
    bool mute_sound_effects = gSavedSettings.getBOOL("MuteSounds");
    bool mute_all_sounds = gSavedSettings.getBOOL("MuteAudio");
    getChild<LLCheckBoxCtrl>("gesture_audio_play_btn")->setEnabled(!(mute_sound_effects || mute_all_sounds));
    getChild<LLCheckBoxCtrl>("collisions_audio_play_btn")->setEnabled(!(mute_sound_effects || mute_all_sounds));
}
// </FS:PP>

void LLFloaterPreference::updateClickActionControls()
{
    const int single_clk_action = getChild<LLComboBox>("single_click_action_combo")->getValue().asInteger();
    const int double_clk_action = getChild<LLComboBox>("double_click_action_combo")->getValue().asInteger();

    // Todo: This is a very ugly way to get access to keybindings.
    // Reconsider possible options.
    // Potential option: make constructor of LLKeyConflictHandler private
    // but add a getter that will return shared pointer for specific
    // mode, pointer should only exist so long as there are external users.
    // In such case we won't need to do this 'dynamic_cast' nightmare.
    // updateTable() can also be avoided
    LLTabContainer* tabcontainer = getChild<LLTabContainer>("pref core");
    for (child_list_t::const_iterator iter = tabcontainer->getChildList()->begin();
        iter != tabcontainer->getChildList()->end(); ++iter)
    {
        LLView* view = *iter;
        LLPanelPreferenceControls* panel = dynamic_cast<LLPanelPreferenceControls*>(view);
        if (panel)
        {
            panel->setKeyBind("walk_to",
                              EMouseClickType::CLICK_LEFT,
                              KEY_NONE,
                              MASK_NONE,
                              single_clk_action == 1);

            panel->setKeyBind("walk_to",
                              EMouseClickType::CLICK_DOUBLELEFT,
                              KEY_NONE,
                              MASK_NONE,
                              double_clk_action == 1);

            panel->setKeyBind("teleport_to",
                              EMouseClickType::CLICK_DOUBLELEFT,
                              KEY_NONE,
                              MASK_NONE,
                              double_clk_action == 2);

            panel->updateAndApply();
        }
    }
}

void LLFloaterPreference::updateClickActionViews()
{
    bool click_to_walk = false;
    bool dbl_click_to_walk = false;
    bool dbl_click_to_teleport = false;

    // Todo: This is a very ugly way to get access to keybindings.
    // Reconsider possible options.
    LLTabContainer* tabcontainer = getChild<LLTabContainer>("pref core");
    for (child_list_t::const_iterator iter = tabcontainer->getChildList()->begin();
        iter != tabcontainer->getChildList()->end(); ++iter)
    {
        LLView* view = *iter;
        LLPanelPreferenceControls* panel = dynamic_cast<LLPanelPreferenceControls*>(view);
        if (panel)
        {
            click_to_walk = panel->canKeyBindHandle("walk_to",
                EMouseClickType::CLICK_LEFT,
                KEY_NONE,
                MASK_NONE);

            dbl_click_to_walk = panel->canKeyBindHandle("walk_to",
                EMouseClickType::CLICK_DOUBLELEFT,
                KEY_NONE,
                MASK_NONE);

            dbl_click_to_teleport = panel->canKeyBindHandle("teleport_to",
                EMouseClickType::CLICK_DOUBLELEFT,
                KEY_NONE,
                MASK_NONE);
        }
    }

    getChild<LLComboBox>("single_click_action_combo")->setValue((int)click_to_walk);
    getChild<LLComboBox>("double_click_action_combo")->setValue(dbl_click_to_teleport ? 2 : (int)dbl_click_to_walk);
}

void LLFloaterPreference::updateSearchableItems()
{
    mSearchDataDirty = true;
}

void LLFloaterPreference::applyUIColor(LLUICtrl* ctrl, const LLSD& param)
{
    LLUIColorTable::instance().setColor(param.asString(), LLColor4(ctrl->getValue()));
}

void LLFloaterPreference::getUIColor(LLUICtrl* ctrl, const LLSD& param)
{
    LLColorSwatchCtrl* color_swatch = (LLColorSwatchCtrl*) ctrl;
    color_swatch->setOriginal(LLUIColorTable::instance().getColor(param.asString()));
}

void LLFloaterPreference::setCacheLocation(const LLStringExplicit& location)
{
    LLUICtrl* cache_location_editor = getChild<LLUICtrl>("cache_location");
    cache_location_editor->setValue(location);
    cache_location_editor->setToolTip(location);
}

// <FS:Ansariel> Sound cache
void LLFloaterPreference::setSoundCacheLocation(const LLStringExplicit& location)
{
    LLUICtrl* cache_location_editor = getChild<LLUICtrl>("FSSoundCacheLocation");
    cache_location_editor->setValue(location);
    cache_location_editor->setToolTip(location);
}
// </FS:Ansariel>

void LLFloaterPreference::selectPanel(const LLSD& name)
{
    LLTabContainer * tab_containerp = getChild<LLTabContainer>("pref core");
    LLPanel * panel = tab_containerp->getPanelByName(name.asStringRef());
    if (NULL != panel)
    {
        tab_containerp->selectTabPanel(panel);
    }
}

void LLFloaterPreference::selectPrivacyPanel()
{
    selectPanel("im");
}

void LLFloaterPreference::selectChatPanel()
{
    selectPanel("chat");
}

void LLFloaterPreference::changed()
{
    getChild<LLButton>("clear_log")->setEnabled(LLConversationLog::instance().getConversations().size() > 0);

    // set 'enable' property for 'Delete transcripts...' button
    updateDeleteTranscriptsButton();

}

// <FS:Ansariel> Build fix
//void LLFloaterPreference::saveGraphicsPreset(std::string& preset)
void LLFloaterPreference::saveGraphicsPreset(const std::string& preset)
// </FS:Ansariel>
{
    mSavedGraphicsPreset = preset;
}


// <FS:Ansariel> Properly disable avatar tag setting
void LLFloaterPreference::onAvatarTagSettingsChanged()
{
    bool usernames_enabled = gSavedSettings.getBOOL("NameTagShowUsernames");
    bool legacy_enabled = gSavedSettings.getBOOL("FSNameTagShowLegacyUsernames");

    childSetEnabled("FSshow_legacyun", usernames_enabled);
    childSetEnabled("legacy_trim_check", usernames_enabled && legacy_enabled);

    bool arw_options_enabled = gSavedSettings.getBOOL("FSTagShowARW") && gSavedSettings.getS32("AvatarNameTagMode") > 0;
    childSetEnabled("FSTagShowTooComplexOnlyARW", arw_options_enabled);
    childSetEnabled("FSTagShowOwnARW", arw_options_enabled);
}
// </FS:Ansariel>

// <FS:Ansariel> Correct enabled state of Animated Script Dialogs option
void LLFloaterPreference::updateAnimatedScriptDialogs()
{
    S32 position = gSavedSettings.getS32("ScriptDialogsPosition");
    childSetEnabled("FSAnimatedScriptDialogs", position == 2 || position == 3);
}
// </FS:Ansariel>

//------------------------------Updater---------------------------------------

//<FS:HG> FIRE-6340, FIRE-6567 - Setting Bandwidth issues
//static bool handleBandwidthChanged(const LLSD& newvalue)
//{
//  gViewerThrottle.setMaxBandwidth((F32) newvalue.asReal());
//  return true;
//}

//class LLPanelPreference::Updater : public LLEventTimer
//{

//public:

//  typedef boost::function<bool(const LLSD&)> callback_t;

//  Updater(callback_t cb, F32 period)
//  :LLEventTimer(period),
//   mCallback(cb)
//  {
//      mEventTimer.stop();
//  }

//  virtual ~Updater(){}

//  void update(const LLSD& new_value)
//  {
//      mNewValue = new_value;
//      mEventTimer.start();
//  }

//protected:

//  bool tick()
//  {
//      mCallback(mNewValue);
//      mEventTimer.stop();

//      return false;
//  }

//private:

//  LLSD mNewValue;
//  callback_t mCallback;
//};
//---------------------------------------------------------------------------- */
//</FS:HG> FIRE-6340, FIRE-6567 - Setting Bandwidth issues

static LLPanelInjector<LLPanelPreference> t_places("panel_preference");
LLPanelPreference::LLPanelPreference()
: LLPanel()
  //<FS:HG> FIRE-6340, FIRE-6567 - Setting Bandwidth issues
  //mBandWidthUpdater(NULL)
{
    //<FS:KC> Handled centrally now
    // mCommitCallbackRegistrar.add("Pref.setControlFalse", boost::bind(&LLPanelPreference::setControlFalse,this, _2));
    mCommitCallbackRegistrar.add("Pref.updateMediaAutoPlayCheckbox",    boost::bind(&LLPanelPreference::updateMediaAutoPlayCheckbox, this, _1));
    mCommitCallbackRegistrar.add("Pref.PrefDelete", boost::bind(&LLPanelPreference::deletePreset, this, _2));
    mCommitCallbackRegistrar.add("Pref.PrefSave",   boost::bind(&LLPanelPreference::savePreset, this, _2));
    mCommitCallbackRegistrar.add("Pref.PrefLoad",   boost::bind(&LLPanelPreference::loadPreset, this, _2));

    // <FS:Ansariel> Customizable contact list columns
    mCommitCallbackRegistrar.add("FS.CheckContactListColumnMode", boost::bind(&LLPanelPreference::onCheckContactListColumnMode, this));
}

//virtual
bool LLPanelPreference::postBuild()
{
    ////////////////////// PanelGeneral ///////////////////
    if (hasChild("display_names_check", true))
    {
        bool use_people_api = gSavedSettings.getBOOL("UsePeopleAPI");
        LLCheckBoxCtrl* ctrl_display_name = getChild<LLCheckBoxCtrl>("display_names_check");
        ctrl_display_name->setEnabled(use_people_api);
        if (!use_people_api)
        {
            ctrl_display_name->setValue(false);
        }
    }

    // <FS:Ansariel> Minimap pick radius transparency
    LLSliderCtrl* map_pickradius_transparency = findChild<LLSliderCtrl>("MapPickRadiusTransparency");
    if (map_pickradius_transparency)
    {
        mOriginalMapPickRadiusTransparency = LLUIColorTable::instance().getColor("MapPickRadiusColor").get().mV[VW];
        map_pickradius_transparency->setValue(mOriginalMapPickRadiusTransparency);
        map_pickradius_transparency->setCommitCallback(boost::bind(&LLPanelPreference::updateMapPickRadiusTransparency, this, _2));
    }
    // </FS:Ansariel>

    // <FS:Ansariel> Flash chat toolbar button notification
    if (hasChild("FSNotifyIMFlash", true))
    {
        gSavedSettings.getControl("FSChatWindow")->getSignal()->connect(boost::bind(&LLPanelPreference::onChatWindowChanged, this));
        onChatWindowChanged();
    }
    // </FS:Ansariel>

    // <FS:Ansariel> Exodus' mouselook combat feature
    if (hasChild("FSMouselookCombatFeatures", true))
    {
        gSavedSettings.getControl("EnableMouselook")->getSignal()->connect(boost::bind(&LLPanelPreference::updateMouselookCombatFeatures, this));
        gSavedSettings.getControl("FSMouselookCombatFeatures")->getSignal()->connect(boost::bind(&LLPanelPreference::updateMouselookCombatFeatures, this));
        updateMouselookCombatFeatures();
    }
    // </FS:Ansariel>

    ////////////////////// PanelVoice ///////////////////
    // <FS:Ansariel> Doesn't exist as of 25-07-2014
    //if (hasChild("voice_unavailable", true))
    //{
    //  bool voice_disabled = gSavedSettings.getBOOL("CmdLineDisableVoice");
    //  getChildView("voice_unavailable")->setVisible( voice_disabled);
    //  getChildView("enable_voice_check")->setVisible( !voice_disabled);
    //}
    // </FS:Ansariel>

    //////////////////////PanelSkins ///////////////////

    /* <FS:CR> Handled below
    if (hasChild("skin_selection", true))
    {
        LLFloaterPreference::refreshSkin(this);

        // if skin is set to a skin that no longer exists (silver) set back to default
        if (getChild<LLRadioGroup>("skin_selection")->getSelectedIndex() < 0)
        {
            gSavedSettings.setString("SkinCurrent", "default");
            LLFloaterPreference::refreshSkin(this);
        }

    }
     */

    //////////////////////PanelPrivacy ///////////////////
    if (hasChild("media_enabled", true))
    {
        bool media_enabled = gSavedSettings.getBOOL("AudioStreamingMedia");

        getChild<LLCheckBoxCtrl>("media_enabled")->set(media_enabled);
        getChild<LLCheckBoxCtrl>("autoplay_enabled")->setEnabled(media_enabled);
    }
    if (hasChild("music_enabled", true))
    {
        getChild<LLCheckBoxCtrl>("music_enabled")->set(gSavedSettings.getBOOL("AudioStreamingMusic"));
    }
    if (hasChild("media_filter"))
    {
        getChild<LLCheckBoxCtrl>("media_filter")->set(gSavedSettings.getBOOL("MediaEnableFilter"));
    }
    if (hasChild("voice_call_friends_only_check", true))
    {
        getChild<LLCheckBoxCtrl>("voice_call_friends_only_check")->setCommitCallback(boost::bind(&showFriendsOnlyWarning, _1, _2));
    }
    // <FS:Ansariel> Disable running multiple viewers warning
    //if (hasChild("allow_multiple_viewer_check", true))
    //{
    //  getChild<LLCheckBoxCtrl>("allow_multiple_viewer_check")->setCommitCallback(boost::bind(&showMultipleViewersWarning, _1, _2));
    //}
    // </FS:Ansariel>
    if (hasChild("favorites_on_login_check", true))
    {
        getChild<LLCheckBoxCtrl>("favorites_on_login_check")->setCommitCallback(boost::bind(&handleFavoritesOnLoginChanged, _1, _2));
        // <FS:Ansariel> [FS Login Panel]
        //bool show_favorites_at_login = LLPanelLogin::getShowFavorites();
        bool show_favorites_at_login = FSPanelLogin::getShowFavorites();
        // </FS:Ansariel> [FS Login Panel]
        getChild<LLCheckBoxCtrl>("favorites_on_login_check")->setValue(show_favorites_at_login);
    }
    if (hasChild("mute_chb_label", true))
    {
        getChild<LLTextBox>("mute_chb_label")->setShowCursorHand(false);
        getChild<LLTextBox>("mute_chb_label")->setSoundFlags(LLView::MOUSE_UP);
        getChild<LLTextBox>("mute_chb_label")->setClickedCallback(boost::bind(&toggleMuteWhenMinimized));
    }

    // Panel Setup (Network) -WoLf
    if (hasChild("connection_port_enabled"))
    {
        getChild<LLCheckBoxCtrl>("connection_port_enabled")->setCommitCallback(boost::bind(&showCustomPortWarning, _1, _2));
    }
    // [/WoLf]

    //////////////////////PanelSetup ///////////////////
    //<FS:HG> FIRE-6340, FIRE-6567 - Setting Bandwidth issues
    //if (hasChild("max_bandwidth", true))
    //{
    //  mBandWidthUpdater = new LLPanelPreference::Updater(boost::bind(&handleBandwidthChanged, _1), BANDWIDTH_UPDATER_TIMEOUT);
    //  gSavedSettings.getControl("ThrottleBandwidthKBPS")->getSignal()->connect(boost::bind(&LLPanelPreference::Updater::update, mBandWidthUpdater, _2));
    //}
    //</FS:HG> FIRE-6340, FIRE-6567 - Setting Bandwidth issues

#ifdef EXTERNAL_TOS
    LLRadioGroup* ext_browser_settings = getChild<LLRadioGroup>("preferred_browser_behavior");
    if (ext_browser_settings)
    {
        // turn off ability to set external/internal browser
        ext_browser_settings->setSelectedByValue(LLWeb::BROWSER_EXTERNAL_ONLY, true);
        ext_browser_settings->setEnabled(false);
    }
#endif

    ////////////////////// PanelAlerts ///////////////////
    if (hasChild("OnlineOfflinetoNearbyChat", true))
    {
        getChildView("OnlineOfflinetoNearbyChatHistory")->setEnabled(getChild<LLUICtrl>("OnlineOfflinetoNearbyChat")->getValue().asBoolean());
    }

    // <FS:Ansariel> Only enable Growl checkboxes if Growl is usable
    if (hasChild("notify_growl_checkbox", true))
    {
        bool growl_enabled = gSavedSettings.getBOOL("FSEnableGrowl") && GrowlManager::isUsable();
        getChild<LLCheckBoxCtrl>("notify_growl_checkbox")->setCommitCallback(boost::bind(&LLPanelPreference::onEnableGrowlChanged, this));
        getChild<LLCheckBoxCtrl>("notify_growl_checkbox")->setEnabled(GrowlManager::isUsable());
        getChild<LLCheckBoxCtrl>("notify_growl_always_checkbox")->setEnabled(growl_enabled);
        getChild<LLCheckBoxCtrl>("FSFilterGrowlKeywordDuplicateIMs")->setEnabled(growl_enabled);
    }
    // </FS:Ansariel>

    ////////////////////// PanelUI ///////////////////
    // <FS:Ansariel> Customizable contact list columns
    if (hasChild("textFriendlistColumns", true))
    {
        onCheckContactListColumnMode();
    }
    // </FS:Ansariel>

    apply();
    return true;
}

LLPanelPreference::~LLPanelPreference()
{
    //<FS:HG> FIRE-6340, FIRE-6567 - Setting Bandwidth issues
    //if (mBandWidthUpdater)
    //{
    //  delete mBandWidthUpdater;
    //}
    //</FS:HG> FIRE-6340, FIRE-6567 - Setting Bandwidth issues
}
void LLPanelPreference::apply()
{
    // no-op
}

void LLPanelPreference::saveSettings()
{
    LLFloater* advanced = LLFloaterReg::findTypedInstance<LLFloater>("prefs_graphics_advanced");

    // Save the value of all controls in the hierarchy
    mSavedValues.clear();
    std::list<LLView*> view_stack;
    view_stack.push_back(this);
    if (advanced)
    {
        view_stack.push_back(advanced);
    }
    while(!view_stack.empty())
    {
        // Process view on top of the stack
        LLView* curview = view_stack.front();
        view_stack.pop_front();

        LLColorSwatchCtrl* color_swatch = dynamic_cast<LLColorSwatchCtrl *>(curview);
        if (color_swatch)
        {
            mSavedColors[color_swatch->getName()] = color_swatch->get();
        }
        else
        {
            LLUICtrl* ctrl = dynamic_cast<LLUICtrl*>(curview);
            if (ctrl)
            {
                LLControlVariable* control = ctrl->getControlVariable();
                if (control)
                {
                    mSavedValues[control] = control->getValue();
                }
            }
        }

        // Push children onto the end of the work stack
        for (child_list_t::const_iterator iter = curview->getChildList()->begin();
             iter != curview->getChildList()->end(); ++iter)
        {
            view_stack.push_back(*iter);
        }
    }

    if (LLStartUp::getStartupState() == STATE_STARTED)
    {
        LLControlVariable* control = gSavedPerAccountSettings.getControl("VoiceCallsFriendsOnly");
        if (control)
        {
            mSavedValues[control] = control->getValue();
        }
    }
}

void LLPanelPreference::showMultipleViewersWarning(LLUICtrl* checkbox, const LLSD& value)
{
    if (checkbox && checkbox->getValue())
    {
        LLNotificationsUtil::add("AllowMultipleViewers");
    }
}

void LLPanelPreference::showFriendsOnlyWarning(LLUICtrl* checkbox, const LLSD& value)
{
    if (checkbox)
    {
        gSavedPerAccountSettings.setBOOL("VoiceCallsFriendsOnly", checkbox->getValue().asBoolean());
        if (checkbox->getValue())
        {
            LLNotificationsUtil::add("FriendsAndGroupsOnly");
        }
    }
}
// Manage the custom port alert, fixes Cant Close bug. -WoLf
void LLPanelPreference::showCustomPortWarning(LLUICtrl* checkbox, const LLSD& value)
{
        LLNotificationsUtil::add("ChangeConnectionPort");
}
// [/WoLf]

void LLPanelPreference::handleFavoritesOnLoginChanged(LLUICtrl* checkbox, const LLSD& value)
{
    if (checkbox)
    {
        LLFavoritesOrderStorage::instance().showFavoritesOnLoginChanged(checkbox->getValue().asBoolean());
        if(checkbox->getValue())
        {
            LLNotificationsUtil::add("FavoritesOnLogin");
        }
    }
}

void LLPanelPreference::toggleMuteWhenMinimized()
{
    std::string mute("MuteWhenMinimized");
    gSavedSettings.setBOOL(mute, !gSavedSettings.getBOOL(mute));
    LLFloaterPreference* instance = LLFloaterReg::findTypedInstance<LLFloaterPreference>("preferences");
    if (instance)
    {
        instance->getChild<LLCheckBoxCtrl>("mute_when_minimized")->setBtnFocus();
    }
}

// <FS:Ansariel> Only enable Growl checkboxes if Growl is usable
void LLPanelPreference::onEnableGrowlChanged()
{
    bool growl_enabled = gSavedSettings.getBOOL("FSEnableGrowl") && GrowlManager::isUsable();
    getChild<LLCheckBoxCtrl>("notify_growl_always_checkbox")->setEnabled(growl_enabled);
    getChild<LLCheckBoxCtrl>("FSFilterGrowlKeywordDuplicateIMs")->setEnabled(growl_enabled);
}
// </FS:Ansariel>

// <FS:Ansariel> Flash chat toolbar button notification
void  LLPanelPreference::onChatWindowChanged()
{
    getChild<LLCheckBoxCtrl>("FSNotifyIMFlash")->setEnabled(gSavedSettings.getS32("FSChatWindow") == 1);
}
// </FS:Ansariel>

// <FS:Ansariel> Exodus' mouselook combat feature
void LLPanelPreference::updateMouselookCombatFeatures()
{
    bool enabled = gSavedSettings.getBOOL("EnableMouselook") && gSavedSettings.getBOOL("FSMouselookCombatFeatures");
    getChild<LLCheckBoxCtrl>("ExodusMouselookIFF")->setEnabled(enabled);
    getChild<LLSliderCtrl>("ExodusMouselookIFFRange")->setEnabled(enabled);
}
// </FS:Ansariel>

// <FS:Ansariel> Minimap pick radius transparency
void LLPanelPreference::updateMapPickRadiusTransparency(const LLSD& value)
{
    static LLColorSwatchCtrl* color_swatch = getChild<LLColorSwatchCtrl>("MapPickRadiusColor");

    LLUIColorTable& color_table = LLUIColorTable::instance();
    LLColor4 color = color_table.getColor("MapPickRadiusColor").get();
    color.mV[VW] = (F32)value.asReal();
    color_table.setColor("MapPickRadiusColor", color);
    color_swatch->set(color);
}
// </FS:Ansariel>

// <FS:Ansariel> Customizable contact list columns
void LLPanelPreference::onCheckContactListColumnMode()
{
    childSetEnabled("FSFriendListColumnShowUserName", gSavedSettings.getBOOL("FSFriendListColumnShowDisplayName") || gSavedSettings.getBOOL("FSFriendListColumnShowFullName"));
    childSetEnabled("FSFriendListColumnShowDisplayName", gSavedSettings.getBOOL("FSFriendListColumnShowUserName") || gSavedSettings.getBOOL("FSFriendListColumnShowFullName"));
    childSetEnabled("FSFriendListColumnShowFullName", gSavedSettings.getBOOL("FSFriendListColumnShowUserName") || gSavedSettings.getBOOL("FSFriendListColumnShowDisplayName"));
}
// </FS:Ansariel>

void LLPanelPreference::cancel(const std::vector<std::string> settings_to_skip)
{
    LLPresetsManager::instance().setIsLoadingPreset(true); // <FS:Ansariel> Graphic preset controls independent from XUI

    for (control_values_map_t::iterator iter =  mSavedValues.begin();
         iter !=  mSavedValues.end(); ++iter)
    {
        LLControlVariable* control = iter->first;
        LLSD ctrl_value = iter->second;

        if((control->getName() == "InstantMessageLogPath") && (ctrl_value.asString() == ""))
        {
            continue;
        }

        auto found = std::find(settings_to_skip.begin(), settings_to_skip.end(), control->getName());
        if (found != settings_to_skip.end())
        {
            continue;
        }

        control->set(ctrl_value);
    }

    for (string_color_map_t::iterator iter = mSavedColors.begin();
         iter != mSavedColors.end(); ++iter)
    {
        LLColorSwatchCtrl* color_swatch = findChild<LLColorSwatchCtrl>(iter->first);
        if (color_swatch)
        {
            color_swatch->set(iter->second);
            color_swatch->onCommit();
        }
    }

    // <FS:Ansariel> Minimap pick radius transparency
    LLSliderCtrl* map_pickradius_transparency = findChild<LLSliderCtrl>("MapPickRadiusTransparency");
    if (map_pickradius_transparency)
    {
        map_pickradius_transparency->setValue(mOriginalMapPickRadiusTransparency);
    }
    // </FS:Ansariel>

    LLPresetsManager::instance().setIsLoadingPreset(false); // <FS:Ansariel> Graphic preset controls indepentent from XUI
}

//<FS:KC> Handled centrally now
/*
void LLPanelPreference::setControlFalse(const LLSD& user_data)
{
    std::string control_name = user_data.asString();
    LLControlVariable* control = findControl(control_name);

    if (control)
        control->set(LLSD(false));
}
*/

void LLPanelPreference::updateMediaAutoPlayCheckbox(LLUICtrl* ctrl)
{
    std::string name = ctrl->getName();

    // Disable "Allow Media to auto play" only when both
    // "Streaming Music" and "Media" are unchecked. STORM-513.
    if ((name == "enable_music") || (name == "enable_media"))
    {
        bool music_enabled = getChild<LLCheckBoxCtrl>("enable_music")->get();
        bool media_enabled = getChild<LLCheckBoxCtrl>("enable_media")->get();

        getChild<LLCheckBoxCtrl>("media_auto_play_combo")->setEnabled(music_enabled || media_enabled);
    }
}

void LLPanelPreference::deletePreset(const LLSD& user_data)
{
    LLFloaterReg::showInstance("delete_pref_preset", user_data.asString());
}

void LLPanelPreference::savePreset(const LLSD& user_data)
{
    LLFloaterReg::showInstance("save_pref_preset", user_data.asString());
}

void LLPanelPreference::loadPreset(const LLSD& user_data)
{
    LLFloaterReg::showInstance("load_pref_preset", user_data.asString());
}

void LLPanelPreference::setHardwareDefaults()
{
}

class LLPanelPreferencePrivacy : public LLPanelPreference
{
public:
    LLPanelPreferencePrivacy()
    {
        mAccountIndependentSettings.push_back("AutoDisengageMic");

        mAutoresponseItem = gSavedPerAccountSettings.getString("FSAutoresponseItemUUID");
    }

    /*virtual*/ void saveSettings()
    {
        LLPanelPreference::saveSettings();

        // Don't save (=erase from the saved values map) per-account privacy settings
        // if we're not logged in, otherwise they will be reset to defaults on log off.
        if (LLStartUp::getStartupState() != STATE_STARTED)
        {
            // Erase only common settings, assuming there are no color settings on Privacy page.
            for (control_values_map_t::iterator it = mSavedValues.begin(); it != mSavedValues.end(); )
            {
                const std::string setting = it->first->getName();
                if (find(mAccountIndependentSettings.begin(),
                    mAccountIndependentSettings.end(), setting) == mAccountIndependentSettings.end())
                {
                    mSavedValues.erase(it++);
                }
                else
                {
                    ++it;
                }
            }
        }
    }

    // <FS:Ansariel> Send inventory item on autoresponse
    /*virtual*/ void apply()
    {
        LLPanelPreference::apply();
        if (LLStartUp::getStartupState() == STATE_STARTED)
        {
            gSavedPerAccountSettings.setString("FSAutoresponseItemUUID", mAutoresponseItem);
        }
    }
    // </FS:Ansariel>

    // <FS:Ansariel> DebugLookAt checkbox status not working properly
    /*virtual*/ bool postBuild()
    {
        getChild<LLUICtrl>("showlookat")->setCommitCallback(boost::bind(&LLPanelPreferencePrivacy::onClickDebugLookAt, this, _2));
        gSavedPerAccountSettings.getControl("DebugLookAt")->getSignal()->connect(boost::bind(&LLPanelPreferencePrivacy::onChangeDebugLookAt, this));
        onChangeDebugLookAt();

        mInvDropTarget = getChild<FSCopyTransInventoryDropTarget>("autoresponse_item");
        mInvDropTarget->setDADCallback(boost::bind(&LLPanelPreferencePrivacy::onDADAutoresponseItem, this, _1));
        getChild<LLButton>("clear_autoresponse_item")->setCommitCallback(boost::bind(&LLPanelPreferencePrivacy::onClearAutoresponseItem, this));

        return LLPanelPreference::postBuild();
    }
    // </FS:Ansariel>

    // <FS:Ansariel> Send inventory item on autoresponse
    /* virtual */ void onOpen(const LLSD& key)
    {
        LLButton* clear_item_btn = getChild<LLButton>("clear_autoresponse_item");
        clear_item_btn->setEnabled(false);
        if (LLStartUp::getStartupState() == STATE_STARTED)
        {
            mAutoresponseItem = gSavedPerAccountSettings.getString("FSAutoresponseItemUUID");
            LLUUID item_id(mAutoresponseItem);
            if (item_id.isNull())
            {
                mInvDropTarget->setText(getString("AutoresponseItemNotSet"));
            }
            else
            {
                clear_item_btn->setEnabled(true);
                LLInventoryObject* item = gInventory.getObject(item_id);
                if (item)
                {
                    mInvDropTarget->setText(item->getName());
                }
                else
                {
                    mInvDropTarget->setText(getString("AutoresponseItemNotAvailable"));
                }
            }
        }
        else
        {
            mInvDropTarget->setText(getString("AutoresponseItemNotLoggedIn"));
        }
    }
    // </FS:Ansariel>

private:
    std::list<std::string> mAccountIndependentSettings;

    // <FS:Ansariel> Send inventory item on autoresponse
    FSCopyTransInventoryDropTarget* mInvDropTarget;
    std::string                     mAutoresponseItem;

    // <FS:Ansariel> DebugLookAt checkbox status not working properly
    void onChangeDebugLookAt()
    {
        getChild<LLCheckBoxCtrl>("showlookat")->set(gSavedPerAccountSettings.getS32("DebugLookAt") == 0 ? false : true);
    }

    void onClickDebugLookAt(const LLSD& value)
    {
        gSavedPerAccountSettings.setS32("DebugLookAt", value.asBoolean());
    }
    // </FS:Ansariel>

    // <FS:Ansariel> Send inventory item on autoresponse
    void onDADAutoresponseItem(const LLUUID& item_id)
    {
        LLInventoryObject* item = gInventory.getObject(item_id);
        if (item)
        {
            mInvDropTarget->setText(item->getName());
            mAutoresponseItem = item_id.asString();
            childSetEnabled("clear_autoresponse_item", true);
        }
    }

    void onClearAutoresponseItem()
    {
        mAutoresponseItem = "";
        mInvDropTarget->setText(getString("AutoresponseItemNotSet"));
        childSetEnabled("clear_autoresponse_item", false);
    }
    // </FS:Ansariel>
};

static LLPanelInjector<LLPanelPreferenceGraphics> t_pref_graph("panel_preference_graphics");
static LLPanelInjector<LLPanelPreferencePrivacy> t_pref_privacy("panel_preference_privacy");

bool LLPanelPreferenceGraphics::postBuild()
{
    // <FS:Ansariel> Improved graphics preferences
    //LLFloaterReg::showInstance("prefs_graphics_advanced");
    //LLFloaterReg::hideInstance("prefs_graphics_advanced");
    // </FS:Ansariel>

    // <FS:Ansariel> Advanced graphics preferences
    // Disable FSAA combo when shaders are not loaded
    //
    {
        LLComboBox* combo = getChild<LLComboBox>("fsaa");
        if (!gFXAAProgram[0].isComplete())
            combo->remove("FXAA");

        if (!gSMAAEdgeDetectProgram[0].isComplete())
            combo->remove("SMAA");

        if (!gFXAAProgram[0].isComplete() && !gSMAAEdgeDetectProgram[0].isComplete())
        {
            combo->setEnabled(false);
            getChild<LLComboBox>("fsaa quality")->setEnabled(false);
        }
    }

#if !LL_DARWIN
    LLCheckBoxCtrl *use_HiDPI = getChild<LLCheckBoxCtrl>("use HiDPI");
    use_HiDPI->setEnabled(false);
#endif
    // </FS:Ansariel>

    resetDirtyChilds();
    setPresetText();

    LLPresetsManager* presetsMgr = LLPresetsManager::getInstance();
    presetsMgr->setPresetListChangeCallback(boost::bind(&LLPanelPreferenceGraphics::onPresetsListChange, this));
    presetsMgr->createMissingDefault(PRESETS_GRAPHIC); // a no-op after the first time, but that's ok


// <FS:CR> Hide this until we have fullscreen mode functional on OSX again
#ifdef LL_DARWIN
    getChild<LLCheckBoxCtrl>("Fullscreen Mode")->setVisible(false);
#endif // LL_DARWIN
// </FS:CR>

    return LLPanelPreference::postBuild();
}
void LLPanelPreferenceGraphics::draw()
{
    LLPanelPreference::draw();
}

void LLPanelPreferenceGraphics::onPresetsListChange()
{
    resetDirtyChilds();
    setPresetText();

    //LLFloaterPreference* instance = LLFloaterReg::findTypedInstance<LLFloaterPreference>("preferences");
    //if (instance && !gSavedSettings.getString("PresetGraphicActive").empty())
    //{
    //  instance->saveSettings(); //make cancel work correctly after changing the preset
    //}
    //else
    //{
    //  std::string dummy;
    //  instance->saveGraphicsPreset(dummy);
    //}
}

void LLPanelPreferenceGraphics::setPresetText()
{
    // <FS:Ansariel> Performance improvement
    //LLTextBox* preset_text = getChild<LLTextBox>("preset_text");
    static LLTextBox* preset_text = getChild<LLTextBox>("preset_text");
    // </FS:Ansariel>

    std::string preset_graphic_active = gSavedSettings.getString("PresetGraphicActive");

    // <FS:Ansariel> Fix resetting graphics preset on cancel
    //if (!preset_graphic_active.empty() && preset_graphic_active != preset_text->getText())
    //{
    //  LLFloaterPreference* instance = LLFloaterReg::findTypedInstance<LLFloaterPreference>("preferences");
    //  if (instance)
    //  {
    //      instance->saveGraphicsPreset(preset_graphic_active);
    //  }
    //}
    // </FS:Ansariel>

    // <FS:Ansariel> Graphic preset controls independent from XUI
    //if (hasDirtyChilds() && !preset_graphic_active.empty())
    //{
    //  gSavedSettings.setString("PresetGraphicActive", "");
    //  preset_graphic_active.clear();
    //  // This doesn't seem to cause an infinite recursion.  This trigger is needed to cause the pulldown
    //  // panel to update.
    //  LLPresetsManager::getInstance()->triggerChangeSignal();
    //}
    // </FS:Ansariel>

    if (!preset_graphic_active.empty())
    {
        if (preset_graphic_active == PRESETS_DEFAULT)
        {
            preset_graphic_active = LLTrans::getString(PRESETS_DEFAULT);
        }
        preset_text->setText(preset_graphic_active);
    }
    else
    {
        preset_text->setText(LLTrans::getString("none_paren_cap"));
    }

    preset_text->resetDirty();
}

bool LLPanelPreferenceGraphics::hasDirtyChilds()
{
    LLFloater* advanced = LLFloaterReg::findTypedInstance<LLFloater>("prefs_graphics_advanced");
    std::list<LLView*> view_stack;
    view_stack.push_back(this);
    if (advanced)
    {
        view_stack.push_back(advanced);
    }
    while(!view_stack.empty())
    {
        // Process view on top of the stack
        LLView* curview = view_stack.front();
        view_stack.pop_front();

        LLUICtrl* ctrl = dynamic_cast<LLUICtrl*>(curview);
        if (ctrl)
        {
            if (ctrl->isDirty())
            {
                LLControlVariable* control = ctrl->getControlVariable();
                if (control)
                {
                    std::string control_name = control->getName();
                    if (!control_name.empty())
                    {
                        return true;
                    }
                }
            }
        }
        // Push children onto the end of the work stack
        for (child_list_t::const_iterator iter = curview->getChildList()->begin();
             iter != curview->getChildList()->end(); ++iter)
        {
            view_stack.push_back(*iter);
        }
    }
    return false;
}

void LLPanelPreferenceGraphics::resetDirtyChilds()
{
    LLFloater* advanced = LLFloaterReg::findTypedInstance<LLFloater>("prefs_graphics_advanced");
    std::list<LLView*> view_stack;
    view_stack.push_back(this);
    if (advanced)
    {
        view_stack.push_back(advanced);
    }
    while(!view_stack.empty())
    {
        // Process view on top of the stack
        LLView* curview = view_stack.front();
        view_stack.pop_front();

        LLUICtrl* ctrl = dynamic_cast<LLUICtrl*>(curview);
        if (ctrl)
        {
            ctrl->resetDirty();
        }
        // Push children onto the end of the work stack
        for (child_list_t::const_iterator iter = curview->getChildList()->begin();
             iter != curview->getChildList()->end(); ++iter)
        {
            view_stack.push_back(*iter);
        }
    }
}

void LLPanelPreferenceGraphics::cancel(const std::vector<std::string> settings_to_skip)
{
    // <FS:Ansariel> Improved graphics preferences
    resetDirtyChilds();
    LLPanelPreference::cancel(settings_to_skip);
}
void LLPanelPreferenceGraphics::saveSettings()
{
    resetDirtyChilds();
    // <FS:Ansariel> Improved graphics preferences; We don't need this
    //std::string preset_graphic_active = gSavedSettings.getString("PresetGraphicActive");
    //if (preset_graphic_active.empty())
    //{
    //  LLFloaterPreference* instance = LLFloaterReg::findTypedInstance<LLFloaterPreference>("preferences");
    //  if (instance)
    //  {
    //      //don't restore previous preset after closing Preferences
    //      instance->saveGraphicsPreset(preset_graphic_active);
    //  }
    //}
    // </FS:Ansariel>
    LLPanelPreference::saveSettings();
}
void LLPanelPreferenceGraphics::setHardwareDefaults()
{
    resetDirtyChilds();
    // <FS:Ansariel> Improved graphics preferences
    LLPanelPreference::setHardwareDefaults();
}

//------------------------LLPanelPreferenceControls--------------------------------
static LLPanelInjector<LLPanelPreferenceControls> t_pref_contrls("panel_preference_controls");

LLPanelPreferenceControls::LLPanelPreferenceControls()
    :LLPanelPreference(),
    mEditingColumn(-1),
    mEditingMode(0)
{
    // MODE_COUNT - 1 because there are currently no settings assigned to 'saved settings'.
    for (U32 i = 0; i < LLKeyConflictHandler::MODE_COUNT - 1; ++i)
    {
        mConflictHandler[i].setLoadMode((LLKeyConflictHandler::ESourceMode)i);
    }
}

LLPanelPreferenceControls::~LLPanelPreferenceControls()
{
}

bool LLPanelPreferenceControls::postBuild()
{
    // populate list of controls
    pControlsTable = getChild<LLScrollListCtrl>("controls_list");
    pKeyModeBox = getChild<LLComboBox>("key_mode");

    pControlsTable->setCommitCallback(boost::bind(&LLPanelPreferenceControls::onListCommit, this));
    pKeyModeBox->setCommitCallback(boost::bind(&LLPanelPreferenceControls::onModeCommit, this));
    getChild<LLButton>("restore_defaults")->setCommitCallback(boost::bind(&LLPanelPreferenceControls::onRestoreDefaultsBtn, this));

    return true;
}

void LLPanelPreferenceControls::regenerateControls()
{
    mEditingMode = pKeyModeBox->getValue().asInteger();
    mConflictHandler[mEditingMode].loadFromSettings((LLKeyConflictHandler::ESourceMode)mEditingMode);
    populateControlTable();
}

bool LLPanelPreferenceControls::addControlTableColumns(const std::string &filename)
{
    LLXMLNodePtr xmlNode;
    LLScrollListCtrl::Contents contents;
    if (!LLUICtrlFactory::getLayeredXMLNode(filename, xmlNode))
    {
        LL_WARNS("Preferences") << "Failed to load " << filename << LL_ENDL;
        return false;
    }
    LLXUIParser parser;
    parser.readXUI(xmlNode, contents, filename);

    if (!contents.validateBlock())
    {
        return false;
    }

    for (LLInitParam::ParamIterator<LLScrollListColumn::Params>::const_iterator col_it = contents.columns.begin();
        col_it != contents.columns.end();
        ++col_it)
    {
        pControlsTable->addColumn(*col_it);
    }

    return true;
}

bool LLPanelPreferenceControls::addControlTableRows(const std::string &filename)
{
    LLXMLNodePtr xmlNode;
    LLScrollListCtrl::Contents contents;
    if (!LLUICtrlFactory::getLayeredXMLNode(filename, xmlNode))
    {
        LL_WARNS("Preferences") << "Failed to load " << filename << LL_ENDL;
        return false;
    }
    LLXUIParser parser;
    parser.readXUI(xmlNode, contents, filename);

    if (!contents.validateBlock())
    {
        return false;
    }

    LLScrollListCell::Params cell_params;
    // init basic cell params
    cell_params.font = LLFontGL::getFontSansSerif();
    cell_params.font_halign = LLFontGL::LEFT;
    cell_params.column = "";
    cell_params.value = "";


    for (LLInitParam::ParamIterator<LLScrollListItem::Params>::const_iterator row_it = contents.rows.begin();
        row_it != contents.rows.end();
        ++row_it)
    {
        std::string control = row_it->value.getValue().asString();
        if (!control.empty() && control != "menu_separator")
        {
            bool show = true;
            bool enabled = mConflictHandler[mEditingMode].canAssignControl(control);
            if (!enabled)
            {
                // If empty: this is a placeholder to make sure user won't assign
                // value by accident, don't show it
                // If not empty: predefined control combination user should see
                // to know that combination is reserved
                show = !mConflictHandler[mEditingMode].isControlEmpty(control);
                // example: teleport_to and walk_to in first person view, and
                // sitting related functions, see generatePlaceholders()
            }

            if (show)
            {
                // At the moment viewer is hardcoded to assume that columns are named as lst_ctrl%d
                LLScrollListItem::Params item_params(*row_it);
                item_params.enabled.setValue(enabled);

                S32 num_columns = pControlsTable->getNumColumns();
                for (S32 col = 1; col < num_columns; col++)
                {
                    cell_params.column = llformat("lst_ctrl%d", col);
                    cell_params.value = mConflictHandler[mEditingMode].getControlString(control, col - 1);
                    item_params.columns.add(cell_params);
                }
                pControlsTable->addRow(item_params, EAddPosition::ADD_BOTTOM);
            }
        }
        else
        {
            // Separator example:
            // <rows
            //  enabled = "false">
            //  <columns
            //   type = "icon"
            //   color = "0 0 0 0.7"
            //   halign = "center"
            //   value = "menu_separator"
            //   column = "lst_action" / >
            //</rows>
            pControlsTable->addRow(*row_it, EAddPosition::ADD_BOTTOM);
        }
    }
    return true;
}

void LLPanelPreferenceControls::addControlTableSeparator()
{
    LLScrollListItem::Params separator_params;
    separator_params.enabled(false);
    LLScrollListCell::Params column_params;
    column_params.type = "icon";
    column_params.value = "menu_separator";
    column_params.column = "lst_action";
    column_params.color = LLColor4(0.f, 0.f, 0.f, 0.7f);
    column_params.font_halign = LLFontGL::HCENTER;
    separator_params.columns.add(column_params);
    pControlsTable->addRow(separator_params, EAddPosition::ADD_BOTTOM);
}

void LLPanelPreferenceControls::populateControlTable()
{
    pControlsTable->clearRows();
    pControlsTable->clearColumns();

    // Add columns
    std::string filename;
    switch ((LLKeyConflictHandler::ESourceMode)mEditingMode)
    {
    case LLKeyConflictHandler::MODE_THIRD_PERSON:
    case LLKeyConflictHandler::MODE_FIRST_PERSON:
    case LLKeyConflictHandler::MODE_EDIT_AVATAR:
    case LLKeyConflictHandler::MODE_SITTING:
        filename = "control_table_contents_columns_basic.xml";
        break;
    default:
        {
            // Either unknown mode or MODE_SAVED_SETTINGS
            // It doesn't have UI or actual settings yet
            LL_WARNS("Preferences") << "Unimplemented mode" << LL_ENDL;

            // Searchable columns were removed, mark searchables for an update
            LLFloaterPreference* instance = LLFloaterReg::findTypedInstance<LLFloaterPreference>("preferences");
            if (instance)
            {
                instance->updateSearchableItems();
            }
            return;
        }
    }
    addControlTableColumns(filename);

    // Add rows.
    // Each file represents individual visual group (movement/camera/media...)
    if (mEditingMode == LLKeyConflictHandler::MODE_FIRST_PERSON)
    {
        // Don't display whole camera and editing groups
        addControlTableRows("control_table_contents_movement.xml");
        addControlTableSeparator();
        addControlTableRows("control_table_contents_media.xml");
    }
    // MODE_THIRD_PERSON; MODE_EDIT_AVATAR; MODE_SITTING
    else if (mEditingMode < LLKeyConflictHandler::MODE_SAVED_SETTINGS)
    {
        // In case of 'sitting' mode, movements still apply due to vehicles
        // but walk_to is not supported and will be hidden by addControlTableRows
        addControlTableRows("control_table_contents_movement.xml");
        addControlTableSeparator();

        addControlTableRows("control_table_contents_camera.xml");
        addControlTableSeparator();

        addControlTableRows("control_table_contents_editing.xml");
        addControlTableSeparator();

        addControlTableRows("control_table_contents_media.xml");
    }
    else
    {
        LL_WARNS("Preferences") << "Unimplemented mode" << LL_ENDL;
    }

    // explicit update to make sure table is ready for llsearchableui
    pControlsTable->updateColumns();

    // Searchable columns were removed and readded, mark searchables for an update
    // Note: at the moment tables/lists lack proper llsearchableui support
    LLFloaterPreference* instance = LLFloaterReg::findTypedInstance<LLFloaterPreference>("preferences");
    if (instance)
    {
        instance->updateSearchableItems();
    }
}

void LLPanelPreferenceControls::updateTable()
{
    mEditingControl.clear();
    std::vector<LLScrollListItem*> list = pControlsTable->getAllData();
    for (S32 i = 0; i < list.size(); ++i)
    {
        std::string control = list[i]->getValue();
        if (!control.empty())
        {
            LLScrollListCell* cell = NULL;

            S32 num_columns = pControlsTable->getNumColumns();
            for (S32 col = 1; col < num_columns; col++)
            {
                cell = list[i]->getColumn(col);
                cell->setValue(mConflictHandler[mEditingMode].getControlString(control, col - 1));
            }
        }
    }
    pControlsTable->deselectAllItems();
}

void LLPanelPreferenceControls::apply()
{
    for (U32 i = 0; i < LLKeyConflictHandler::MODE_COUNT - 1; ++i)
    {
        if (mConflictHandler[i].hasUnsavedChanges())
        {
            mConflictHandler[i].saveToSettings();
        }
    }
}

void LLPanelPreferenceControls::cancel(const std::vector<std::string> settings_to_skip)
{
    for (U32 i = 0; i < LLKeyConflictHandler::MODE_COUNT - 1; ++i)
    {
        if (mConflictHandler[i].hasUnsavedChanges())
        {
            mConflictHandler[i].clear();
            if (mEditingMode == i)
            {
                // cancel() can be called either when preferences floater closes
                // or when child floater closes (like advanced graphical settings)
                // in which case we need to clear and repopulate table
                regenerateControls();
            }
        }
    }
}

void LLPanelPreferenceControls::saveSettings()
{
    for (U32 i = 0; i < LLKeyConflictHandler::MODE_COUNT - 1; ++i)
    {
        if (mConflictHandler[i].hasUnsavedChanges())
        {
            mConflictHandler[i].saveToSettings();
            mConflictHandler[i].clear();
        }
    }

    S32 mode = pKeyModeBox->getValue().asInteger();
    if (mConflictHandler[mode].empty() || pControlsTable->isEmpty())
    {
        regenerateControls();
    }
}

void LLPanelPreferenceControls::resetDirtyChilds()
{
    regenerateControls();
}

void LLPanelPreferenceControls::onListCommit()
{
    LLScrollListItem* item = pControlsTable->getFirstSelected();
    if (item == NULL)
    {
        return;
    }

    std::string control = item->getValue();

    if (control.empty())
    {
        pControlsTable->deselectAllItems();
        return;
    }

    if (!mConflictHandler[mEditingMode].canAssignControl(control))
    {
        pControlsTable->deselectAllItems();
        return;
    }

    S32 cell_ind = item->getSelectedCell();
    if (cell_ind <= 0)
    {
        pControlsTable->deselectAllItems();
        return;
    }

    // List does not tell us what cell was clicked, so we have to figure it out manually, but
    // fresh mouse coordinates are not yet accessible during onCommit() and there are other issues,
    // so we cheat: remember item user clicked at, trigger 'key dialog' on hover that comes next,
    // use coordinates from hover to calculate cell

    LLScrollListCell* cell = item->getColumn(cell_ind);
    if (cell)
    {
        LLSetKeyBindDialog* dialog = LLFloaterReg::getTypedInstance<LLSetKeyBindDialog>("keybind_dialog", LLSD());
        if (dialog)
        {
            mEditingControl = control;
            mEditingColumn = cell_ind;
            dialog->setParent(this, pControlsTable, DEFAULT_KEY_FILTER);

            LLFloater* root_floater = gFloaterView->getParentFloater(this);
            if (root_floater)
                root_floater->addDependentFloater(dialog);
            dialog->openFloater();
            dialog->setFocus(true);
        }
    }
    else
    {
        pControlsTable->deselectAllItems();
    }
}

void LLPanelPreferenceControls::onModeCommit()
{
    mEditingMode = pKeyModeBox->getValue().asInteger();
    if (mConflictHandler[mEditingMode].empty())
    {
        // opening for first time
        mConflictHandler[mEditingMode].loadFromSettings((LLKeyConflictHandler::ESourceMode)mEditingMode);
    }
    populateControlTable();
}

void LLPanelPreferenceControls::onRestoreDefaultsBtn()
{
    LLNotificationsUtil::add("PreferenceControlsDefaults", LLSD(), LLSD(), boost::bind(&LLPanelPreferenceControls::onRestoreDefaultsResponse, this, _1, _2));
}

void LLPanelPreferenceControls::onRestoreDefaultsResponse(const LLSD& notification, const LLSD& response)
{
    S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
    switch(option)
    {
    case 0: // All
        for (U32 i = 0; i < LLKeyConflictHandler::MODE_COUNT - 1; ++i)
        {
            mConflictHandler[i].resetToDefaults();
            // Apply changes to viewer as 'temporary'
            mConflictHandler[i].saveToSettings(true);

            // notify comboboxes in move&view about potential change
            LLFloaterPreference* instance = LLFloaterReg::findTypedInstance<LLFloaterPreference>("preferences");
            if (instance)
            {
                instance->updateClickActionViews();
            }
        }

        updateTable();
        break;
    case 1: // Current
        mConflictHandler[mEditingMode].resetToDefaults();
        // Apply changes to viewer as 'temporary'
        mConflictHandler[mEditingMode].saveToSettings(true);

        if (mEditingMode == LLKeyConflictHandler::MODE_THIRD_PERSON)
        {
            // notify comboboxes in move&view about potential change
            LLFloaterPreference* instance = LLFloaterReg::findTypedInstance<LLFloaterPreference>("preferences");
            if (instance)
            {
                instance->updateClickActionViews();
            }
        }

        updateTable();
        break;
    case 2: // Cancel
    default:
        //exit;
        break;
    }
}

// Bypass to let Move & view read values without need to create own key binding handler
// Assumes third person view
// Might be better idea to just move whole mConflictHandler into LLFloaterPreference
bool LLPanelPreferenceControls::canKeyBindHandle(const std::string &control, EMouseClickType click, KEY key, MASK mask)
{
    S32 mode = LLKeyConflictHandler::MODE_THIRD_PERSON;
    if (mConflictHandler[mode].empty())
    {
        // opening for first time
        mConflictHandler[mode].loadFromSettings(LLKeyConflictHandler::MODE_THIRD_PERSON);
    }

    return mConflictHandler[mode].canHandleControl(control, click, key, mask);
}

// Bypass to let Move & view modify values without need to create own key binding handler
// Assumes third person view
// Might be better idea to just move whole mConflictHandler into LLFloaterPreference
void LLPanelPreferenceControls::setKeyBind(const std::string &control, EMouseClickType click, KEY key, MASK mask, bool set)
{
    S32 mode = LLKeyConflictHandler::MODE_THIRD_PERSON;
    if (mConflictHandler[mode].empty())
    {
        // opening for first time
        mConflictHandler[mode].loadFromSettings(LLKeyConflictHandler::MODE_THIRD_PERSON);
    }

    if (!mConflictHandler[mode].canAssignControl(mEditingControl))
    {
        return;
    }

    bool already_recorded = mConflictHandler[mode].canHandleControl(control, click, key, mask);
    if (set)
    {
        if (already_recorded)
        {
            // nothing to do
            return;
        }

        // find free spot to add data, if no free spot, assign to first
        S32 index = 0;
        for (S32 i = 0; i < 3; i++)
        {
            if (mConflictHandler[mode].getControl(control, i).isEmpty())
            {
                index = i;
                break;
            }
        }
        // At the moment 'ignore_mask' mask is mostly ignored, a placeholder
        // Todo: implement it since it's preferable for things like teleport to match
        // mask exactly but for things like running to ignore additional masks
        // Ideally this needs representation in keybindings UI
        bool ignore_mask = true;
        mConflictHandler[mode].registerControl(control, index, click, key, mask, ignore_mask);
    }
    else if (!set)
    {
        if (!already_recorded)
        {
            // nothing to do
            return;
        }

        // find specific control and reset it
        for (S32 i = 0; i < 3; i++)
        {
            LLKeyData data = mConflictHandler[mode].getControl(control, i);
            if (data.mMouse == click && data.mKey == key && data.mMask == mask)
            {
                mConflictHandler[mode].clearControl(control, i);
            }
        }
    }
}

void LLPanelPreferenceControls::updateAndApply()
{
    S32 mode = LLKeyConflictHandler::MODE_THIRD_PERSON;
    mConflictHandler[mode].saveToSettings(true);
    updateTable();
}

// from LLSetKeybindDialog's interface
bool LLPanelPreferenceControls::onSetKeyBind(EMouseClickType click, KEY key, MASK mask, bool all_modes)
{
    if (!mConflictHandler[mEditingMode].canAssignControl(mEditingControl))
    {
        return true;
    }

    if ( mEditingColumn > 0)
    {
        if (all_modes)
        {
            for (U32 i = 0; i < LLKeyConflictHandler::MODE_COUNT - 1; ++i)
            {
                if (mConflictHandler[i].empty())
                {
                    mConflictHandler[i].loadFromSettings((LLKeyConflictHandler::ESourceMode)i);
                }
                mConflictHandler[i].registerControl(mEditingControl, mEditingColumn - 1, click, key, mask, true);
                // Apply changes to viewer as 'temporary'
                mConflictHandler[i].saveToSettings(true);
            }
        }
        else
        {
            mConflictHandler[mEditingMode].registerControl(mEditingControl, mEditingColumn - 1, click, key, mask, true);
            // Apply changes to viewer as 'temporary'
            mConflictHandler[mEditingMode].saveToSettings(true);
        }
    }

    updateTable();

    if ((mEditingMode == LLKeyConflictHandler::MODE_THIRD_PERSON || all_modes)
        && (mEditingControl == "walk_to"
            || mEditingControl == "teleport_to"
            || click == CLICK_LEFT
            || click == CLICK_DOUBLELEFT))
    {
        // notify comboboxes in move&view about potential change
        LLFloaterPreference* instance = LLFloaterReg::findTypedInstance<LLFloaterPreference>("preferences");
        if (instance)
        {
            instance->updateClickActionViews();
        }
    }

    return true;
}

void LLPanelPreferenceControls::onDefaultKeyBind(bool all_modes)
{
    if (!mConflictHandler[mEditingMode].canAssignControl(mEditingControl))
    {
        return;
    }

    if (mEditingColumn > 0)
    {
        if (all_modes)
        {
            for (U32 i = 0; i < LLKeyConflictHandler::MODE_COUNT - 1; ++i)
            {
                if (mConflictHandler[i].empty())
                {
                    mConflictHandler[i].loadFromSettings((LLKeyConflictHandler::ESourceMode)i);
                }
                mConflictHandler[i].resetToDefault(mEditingControl, mEditingColumn - 1);
                // Apply changes to viewer as 'temporary'
                mConflictHandler[i].saveToSettings(true);
            }
        }
        else
        {
            mConflictHandler[mEditingMode].resetToDefault(mEditingControl, mEditingColumn - 1);
            // Apply changes to viewer as 'temporary'
            mConflictHandler[mEditingMode].saveToSettings(true);
        }
    }
    updateTable();

    if (mEditingMode == LLKeyConflictHandler::MODE_THIRD_PERSON || all_modes)
    {
        // notify comboboxes in move&view about potential change
        LLFloaterPreference* instance = LLFloaterReg::findTypedInstance<LLFloaterPreference>("preferences");
        if (instance)
        {
            instance->updateClickActionViews();
        }
    }
}

void LLPanelPreferenceControls::onCancelKeyBind()
{
    pControlsTable->deselectAllItems();
}

LLFloaterPreferenceProxy::LLFloaterPreferenceProxy(const LLSD& key)
    : LLFloater(key),
      mSocksSettingsDirty(false)
{
    mCommitCallbackRegistrar.add("Proxy.OK",                boost::bind(&LLFloaterPreferenceProxy::onBtnOk, this));
    mCommitCallbackRegistrar.add("Proxy.Cancel",            boost::bind(&LLFloaterPreferenceProxy::onBtnCancel, this));
    mCommitCallbackRegistrar.add("Proxy.Change",            boost::bind(&LLFloaterPreferenceProxy::onChangeSocksSettings, this));
}

LLFloaterPreferenceProxy::~LLFloaterPreferenceProxy()
{
}

bool LLFloaterPreferenceProxy::postBuild()
{
    LLRadioGroup* socksAuth = getChild<LLRadioGroup>("socks5_auth_type");
    if (!socksAuth)
    {
        return false;
    }
    if (socksAuth->getSelectedValue().asString() == "None")
    {
        getChild<LLLineEditor>("socks5_username")->setEnabled(false);
        getChild<LLLineEditor>("socks5_password")->setEnabled(false);
    }
    else
    {
        // Populate the SOCKS 5 credential fields with protected values.
        LLPointer<LLCredential> socks_cred = gSecAPIHandler->loadCredential("SOCKS5");
        getChild<LLLineEditor>("socks5_username")->setValue(socks_cred->getIdentifier()["username"].asString());
        getChild<LLLineEditor>("socks5_password")->setValue(socks_cred->getAuthenticator()["creds"].asString());
    }

    return true;
}

void LLFloaterPreferenceProxy::onOpen(const LLSD& key)
{
    saveSettings();
}

void LLFloaterPreferenceProxy::onClose(bool app_quitting)
{
    if(app_quitting)
    {
        cancel();
    }

    if (mSocksSettingsDirty)
    {

        // If the user plays with the Socks proxy settings after login, it's only fair we let them know
        // it will not be updated until next restart.
        if (LLStartUp::getStartupState()>STATE_LOGIN_WAIT)
        {
            LLNotifications::instance().add("ChangeProxySettings", LLSD(), LLSD());
            mSocksSettingsDirty = false; // we have notified the user now be quiet again
        }
    }
}

void LLFloaterPreferenceProxy::saveSettings()
{
    // Save the value of all controls in the hierarchy
    mSavedValues.clear();
    std::list<LLView*> view_stack;
    view_stack.push_back(this);
    while(!view_stack.empty())
    {
        // Process view on top of the stack
        LLView* curview = view_stack.front();
        view_stack.pop_front();

        LLUICtrl* ctrl = dynamic_cast<LLUICtrl*>(curview);
        if (ctrl)
        {
            LLControlVariable* control = ctrl->getControlVariable();
            if (control)
            {
                mSavedValues[control] = control->getValue();
            }
        }

        // Push children onto the end of the work stack
        for (child_list_t::const_iterator iter = curview->getChildList()->begin();
                iter != curview->getChildList()->end(); ++iter)
        {
            view_stack.push_back(*iter);
        }
    }
}

void LLFloaterPreferenceProxy::onBtnOk()
{
    // commit any outstanding text entry
    if (hasFocus())
    {
        LLUICtrl* cur_focus = dynamic_cast<LLUICtrl*>(gFocusMgr.getKeyboardFocus());
        if (cur_focus && cur_focus->acceptsTextInput())
        {
            cur_focus->onCommit();
        }
    }

    // Save SOCKS proxy credentials securely if password auth is enabled
    LLRadioGroup* socksAuth = getChild<LLRadioGroup>("socks5_auth_type");
    if (socksAuth->getSelectedValue().asString() == "UserPass")
    {
        LLSD socks_id = LLSD::emptyMap();
        socks_id["type"] = "SOCKS5";
        socks_id["username"] = getChild<LLLineEditor>("socks5_username")->getValue().asString();

        LLSD socks_authenticator = LLSD::emptyMap();
        socks_authenticator["type"] = "SOCKS5";
        socks_authenticator["creds"] = getChild<LLLineEditor>("socks5_password")->getValue().asString();

        // Using "SOCKS5" as the "grid" argument since the same proxy
        // settings will be used for all grids and because there is no
        // way to specify the type of credential.
        LLPointer<LLCredential> socks_cred = gSecAPIHandler->createCredential("SOCKS5", socks_id, socks_authenticator);
        gSecAPIHandler->saveCredential(socks_cred, true);
    }
    else
    {
        // Clear SOCKS5 credentials since they are no longer needed.
        LLPointer<LLCredential> socks_cred = new LLCredential("SOCKS5");
        gSecAPIHandler->deleteCredential(socks_cred);
    }

    closeFloater(false);
}

void LLFloaterPreferenceProxy::onBtnCancel()
{
    if (hasFocus())
    {
        LLUICtrl* cur_focus = dynamic_cast<LLUICtrl*>(gFocusMgr.getKeyboardFocus());
        if (cur_focus && cur_focus->acceptsTextInput())
        {
            cur_focus->onCommit();
        }
        refresh();
    }

    cancel();
}

void LLFloaterPreferenceProxy::onClickCloseBtn(bool app_quitting)
{
    cancel();
}

void LLFloaterPreferenceProxy::cancel()
{

    for (control_values_map_t::iterator iter =  mSavedValues.begin();
            iter !=  mSavedValues.end(); ++iter)
    {
        LLControlVariable* control = iter->first;
        LLSD ctrl_value = iter->second;
        control->set(ctrl_value);
    }
    mSocksSettingsDirty = false;
    closeFloater();
}

void LLFloaterPreferenceProxy::onChangeSocksSettings()
{
    mSocksSettingsDirty = true;

    LLRadioGroup* socksAuth = getChild<LLRadioGroup>("socks5_auth_type");
    if (socksAuth->getSelectedValue().asString() == "None")
    {
        getChild<LLLineEditor>("socks5_username")->setEnabled(false);
        getChild<LLLineEditor>("socks5_password")->setEnabled(false);
    }
    else
    {
        getChild<LLLineEditor>("socks5_username")->setEnabled(true);
        getChild<LLLineEditor>("socks5_password")->setEnabled(true);
    }

    // Check for invalid states for the other HTTP proxy radio
    LLRadioGroup* otherHttpProxy = getChild<LLRadioGroup>("other_http_proxy_type");
    if ((otherHttpProxy->getSelectedValue().asString() == "Socks" &&
            !getChild<LLCheckBoxCtrl>("socks_proxy_enabled")->get())||(
                    otherHttpProxy->getSelectedValue().asString() == "Web" &&
                    !getChild<LLCheckBoxCtrl>("web_proxy_enabled")->get()))
    {
        otherHttpProxy->selectFirstItem();
    }

}

void LLFloaterPreference::onUpdateFilterTerm(bool force)
{
    LLWString seachValue = utf8str_to_wstring(mFilterEdit->getValue());
    LLWStringUtil::toLower(seachValue);

    if (!mSearchData || (mSearchData->mLastFilter == seachValue && !force))
        return;

    if (mSearchDataDirty)
    {
        // Data exists, but is obsolete, regenerate
        collectSearchableItems();
    }

    mSearchData->mLastFilter = seachValue;

    if (!mSearchData->mRootTab)
        return;

    mSearchData->mRootTab->hightlightAndHide( seachValue );
    //filterIgnorableNotifications(); // <FS:Ansariel> Using different solution
    if (LLTabContainer* pRoot = getChild<LLTabContainer>("pref core"))
        pRoot->selectFirstTab();
}

// <FS:Ansariel> Using different solution
//void LLFloaterPreference::filterIgnorableNotifications()
//{
//    bool visible = mEnabledPopups->highlightMatchingItems(mFilterEdit->getValue());
//    visible |= mDisabledPopups->highlightMatchingItems(mFilterEdit->getValue());
//
//    if (visible)
//    {
//        getChildRef<LLTabContainer>("pref core").setTabVisibility(getChild<LLPanel>("msgs"), true);
//    }
//}
// </FS:Ansariel>

void collectChildren( LLView const *aView, ll::prefs::PanelDataPtr aParentPanel, ll::prefs::TabContainerDataPtr aParentTabContainer )
{
    if (!aView)
        return;

    llassert_always(aParentPanel || aParentTabContainer);

    for (LLView* pView : *aView->getChildList())
    {
        if (!pView)
            continue;

        ll::prefs::PanelDataPtr pCurPanelData = aParentPanel;
        ll::prefs::TabContainerDataPtr pCurTabContainer = aParentTabContainer;

        LLPanel const *pPanel = dynamic_cast<LLPanel const*>(pView);
        LLTabContainer const *pTabContainer = dynamic_cast<LLTabContainer const*>(pView);
        ll::ui::SearchableControl const *pSCtrl = dynamic_cast<ll::ui::SearchableControl const*>( pView );

        if (pTabContainer)
        {
            pCurPanelData.reset();

            pCurTabContainer = ll::prefs::TabContainerDataPtr(new ll::prefs::TabContainerData);
            pCurTabContainer->mTabContainer = const_cast< LLTabContainer *>(pTabContainer);
            pCurTabContainer->mLabel = pTabContainer->getLabel();
            pCurTabContainer->mPanel = 0;

            if (aParentPanel)
                aParentPanel->mChildPanel.push_back(pCurTabContainer);
            if (aParentTabContainer)
                aParentTabContainer->mChildPanel.push_back(pCurTabContainer);
        }
        else if (pPanel)
        {
            pCurTabContainer.reset();

            pCurPanelData = ll::prefs::PanelDataPtr(new ll::prefs::PanelData);
            pCurPanelData->mPanel = pPanel;
            pCurPanelData->mLabel = pPanel->getLabel();

            llassert_always( aParentPanel || aParentTabContainer );

            if (aParentTabContainer)
                aParentTabContainer->mChildPanel.push_back(pCurPanelData);
            else if (aParentPanel)
                aParentPanel->mChildPanel.push_back(pCurPanelData);
        }
        else if (pSCtrl && pSCtrl->getSearchText().size())
        {
            ll::prefs::SearchableItemPtr item = ll::prefs::SearchableItemPtr(new ll::prefs::SearchableItem());
            item->mView = pView;
            item->mCtrl = pSCtrl;

            item->mLabel = utf8str_to_wstring(pSCtrl->getSearchText());
            LLWStringUtil::toLower(item->mLabel);

            llassert_always(aParentPanel || aParentTabContainer);

            if (aParentPanel)
                aParentPanel->mChildren.push_back(item);
            if (aParentTabContainer)
                aParentTabContainer->mChildren.push_back(item);
        }
        collectChildren(pView, pCurPanelData, pCurTabContainer);
    }
}

void LLFloaterPreference::collectSearchableItems()
{
    mSearchData.reset( nullptr );
    LLTabContainer *pRoot = getChild< LLTabContainer >( "pref core" );
    if( mFilterEdit && pRoot )
    {
        mSearchData.reset(new ll::prefs::SearchData() );

        ll::prefs::TabContainerDataPtr pRootTabcontainer = ll::prefs::TabContainerDataPtr( new ll::prefs::TabContainerData );
        pRootTabcontainer->mTabContainer = pRoot;
        pRootTabcontainer->mLabel = pRoot->getLabel();
        mSearchData->mRootTab = pRootTabcontainer;

        collectChildren( this, ll::prefs::PanelDataPtr(), pRootTabcontainer );
    }
    mSearchDataDirty = false;
}

void LLFloaterPreference::saveIgnoredNotifications()
{
    for (LLNotifications::TemplateMap::const_iterator iter = LLNotifications::instance().templatesBegin();
            iter != LLNotifications::instance().templatesEnd();
            ++iter)
    {
        LLNotificationTemplatePtr templatep = iter->second;
        LLNotificationFormPtr formp = templatep->mForm;

        LLNotificationForm::EIgnoreType ignore = formp->getIgnoreType();
        if (ignore <= LLNotificationForm::IGNORE_NO)
            continue;

        mIgnorableNotifs[templatep->mName] = !formp->getIgnored();
    }
}

void LLFloaterPreference::restoreIgnoredNotifications()
{
    for (std::map<std::string, bool>::iterator it = mIgnorableNotifs.begin(); it != mIgnorableNotifs.end(); ++it)
    {
        LLUI::getInstance()->mSettingGroups["ignores"]->setBOOL(it->first, it->second);
    }
}

// [SL:KB] - Patch: Viewer-CrashReporting | Checked: 2010-11-16 (Catznip-2.6.0a) | Added: Catznip-2.4.0b
static LLPanelInjector<LLPanelPreferenceCrashReports> t_pref_crashreports("panel_preference_crashreports");

LLPanelPreferenceCrashReports::LLPanelPreferenceCrashReports()
    : LLPanelPreference()
{
}

bool LLPanelPreferenceCrashReports::postBuild()
{
    S32 nCrashSubmitBehavior = gCrashSettings.getS32("CrashSubmitBehavior");

    LLCheckBoxCtrl* pSendCrashReports = getChild<LLCheckBoxCtrl>("checkSendCrashReports");
    pSendCrashReports->set(CRASH_BEHAVIOR_NEVER_SEND != nCrashSubmitBehavior);
    pSendCrashReports->setCommitCallback(boost::bind(&LLPanelPreferenceCrashReports::refresh, this));

    LLCheckBoxCtrl* pSendAlwaysAsk = getChild<LLCheckBoxCtrl>("checkSendCrashReportsAlwaysAsk");
    pSendAlwaysAsk->set(CRASH_BEHAVIOR_ASK == nCrashSubmitBehavior);

    LLCheckBoxCtrl* pSendSettings = getChild<LLCheckBoxCtrl>("checkSendSettings");
    pSendSettings->set(gCrashSettings.getBOOL("CrashSubmitSettings"));

    LLCheckBoxCtrl* pSendName = getChild<LLCheckBoxCtrl>("checkSendName");
    pSendName->set(gCrashSettings.getBOOL("CrashSubmitName"));

    getChild<LLTextBox>("textInformation4")->setTextArg("[URL]", getString("PrivacyPolicyUrl"));

#if LL_SEND_CRASH_REPORTS && defined(LL_BUGSPLAT)
    childSetVisible("textRestartRequired", true);
#endif

    refresh();

    return LLPanelPreference::postBuild();
}

void LLPanelPreferenceCrashReports::refresh()
{
    LLCheckBoxCtrl* pSendCrashReports = getChild<LLCheckBoxCtrl>("checkSendCrashReports");
    pSendCrashReports->setEnabled(true);

    bool fEnable = pSendCrashReports->get();
    getChild<LLUICtrl>("checkSendCrashReportsAlwaysAsk")->setEnabled(fEnable);
    getChild<LLUICtrl>("checkSendSettings")->setEnabled(fEnable);
    getChild<LLUICtrl>("checkSendName")->setEnabled(fEnable);
}

void LLPanelPreferenceCrashReports::apply()
{
    LLCheckBoxCtrl* pSendCrashReports = getChild<LLCheckBoxCtrl>("checkSendCrashReports");
    LLCheckBoxCtrl* pSendAlwaysAsk = getChild<LLCheckBoxCtrl>("checkSendCrashReportsAlwaysAsk");
    if (pSendCrashReports->get())
        gCrashSettings.setS32("CrashSubmitBehavior", (pSendAlwaysAsk->get()) ? CRASH_BEHAVIOR_ASK : CRASH_BEHAVIOR_ALWAYS_SEND);
    else
        gCrashSettings.setS32("CrashSubmitBehavior", CRASH_BEHAVIOR_NEVER_SEND);

    LLCheckBoxCtrl* pSendSettings = getChild<LLCheckBoxCtrl>("checkSendSettings");
    gCrashSettings.setBOOL("CrashSubmitSettings", pSendSettings->get());

    LLCheckBoxCtrl* pSendName = getChild<LLCheckBoxCtrl>("checkSendName");
    gCrashSettings.setBOOL("CrashSubmitName", pSendName->get());
}

void LLPanelPreferenceCrashReports::cancel(const std::vector<std::string> settings_to_skip)
{
}
// [/SL:KB]

// [SL:KB] - Patch: Viewer-Skins | Checked: 2010-10-21 (Catznip-2.2)
static LLPanelInjector<LLPanelPreferenceSkins> t_pref_skins("panel_preference_skins");

LLPanelPreferenceSkins::LLPanelPreferenceSkins()
    : LLPanelPreference()
    , m_pSkinCombo(NULL)
    , m_pSkinThemeCombo(NULL)
    , m_pSkinPreview(NULL) // <FS:PP> FIRE-1689: Skins preview image
{
    m_Skin = gSavedSettings.getString("SkinCurrent");
    m_SkinTheme = gSavedSettings.getString("SkinCurrentTheme");
    m_SkinName = gSavedSettings.getString("FSSkinCurrentReadableName");
    m_SkinThemeName = gSavedSettings.getString("FSSkinCurrentThemeReadableName");

    const std::string strSkinsPath = gDirUtilp->getSkinBaseDir() + gDirUtilp->getDirDelimiter() + "skins.xml";
    llifstream fileSkins(strSkinsPath.c_str(), std::ios::binary);
    if (fileSkins.is_open())
    {
        LLSDSerialize::fromXMLDocument(m_SkinsInfo, fileSkins);
    }
}

bool LLPanelPreferenceSkins::postBuild()
{
    m_pSkinCombo = getChild<LLComboBox>("skin_combobox");
    if (m_pSkinCombo)
        m_pSkinCombo->setCommitCallback(boost::bind(&LLPanelPreferenceSkins::onSkinChanged, this));

    m_pSkinThemeCombo = getChild<LLComboBox>("theme_combobox");
    if (m_pSkinThemeCombo)
        m_pSkinThemeCombo->setCommitCallback(boost::bind(&LLPanelPreferenceSkins::onSkinThemeChanged, this));

    refreshSkinList();

    // <FS:PP> FIRE-1689: Skins preview image
    m_pSkinPreview = getChild<LLButton>("skin_preview");
    refreshPreviewImage();
    // </FS:PP>

    return LLPanelPreference::postBuild();
}

void LLPanelPreferenceSkins::apply()
{
    if ( (m_Skin != gSavedSettings.getString("SkinCurrent")) || (m_SkinTheme != gSavedSettings.getString("SkinCurrentTheme")) )
    {
        gSavedSettings.setString("SkinCurrent", m_Skin);
        gSavedSettings.setString("SkinCurrentTheme", m_SkinTheme);

        gSavedSettings.setString("FSSkinCurrentReadableName", m_SkinName);
        gSavedSettings.setString("FSSkinCurrentThemeReadableName", m_SkinThemeName);

        // <FS:AO> Some crude hardcoded preferences per skin. Without this, some defaults from the
        // current skin would be carried over, leading to confusion and a first experience with
        // the skin that the designer didn't intend.
        if (gSavedSettings.getBOOL("FSSkinClobbersToolbarPrefs"))
        {
            LL_INFOS() << "Clearing toolbar settings." << LL_ENDL;
            gSavedSettings.setBOOL("ResetToolbarSettings", true);
        }

        if (m_Skin == "starlight" || m_Skin == "starlightcui")
        {
            std::string noteMessage;

            if (gSavedSettings.getBOOL("ShowMenuBarLocation"))
            {
                noteMessage = LLTrans::getString("skin_defaults_starlight_location");
                gSavedSettings.setBOOL("ShowMenuBarLocation", false);
            }

            if (!gSavedSettings.getBOOL("ShowNavbarNavigationPanel"))
            {
                if (!noteMessage.empty())
                {
                    noteMessage += "\n";
                }
                noteMessage += LLTrans::getString("skin_defaults_starlight_navbar");
                gSavedSettings.setBOOL("ShowNavbarNavigationPanel", true);
            }

            if (!noteMessage.empty())
            {
                LLSD args;
                args["MESSAGE"] = noteMessage;
                LLNotificationsUtil::add("SkinDefaultsChangeSettings", args, LLSD(), boost::bind(&LLPanelPreferenceSkins::showSkinChangeNotification, this));
                return;
            }
        }
        // </FS:AO>

        showSkinChangeNotification();
    }
}

void LLPanelPreferenceSkins::showSkinChangeNotification()
{
    LLSD args, payload;
    LLNotificationsUtil::add("ChangeSkin",
                                args,
                                payload,
                                boost::bind(&LLPanelPreferenceSkins::callbackRestart, this, _1, _2));
}

void LLPanelPreferenceSkins::callbackRestart(const LLSD& notification, const LLSD& response)
{
    S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
    if (2 == option) // Ok button
    {
        return;
    }
    if (0 == option) // Restart
    {
        LL_INFOS() << "User requested quit" << LL_ENDL;
        LLAppViewer::instance()->requestQuit();
    }
}

void LLPanelPreferenceSkins::cancel(const std::vector<std::string> settings_to_skip)
{
    m_Skin = gSavedSettings.getString("SkinCurrent");
    m_SkinTheme = gSavedSettings.getString("SkinCurrentTheme");
    m_SkinName = gSavedSettings.getString("FSSkinCurrentReadableName");
    m_SkinThemeName = gSavedSettings.getString("FSSkinCurrentThemeReadableName");
    refreshSkinList();
    refreshPreviewImage(); // <FS:PP> FIRE-1689: Skins preview image
}

void LLPanelPreferenceSkins::onSkinChanged()
{
    m_Skin = (m_pSkinCombo) ? m_pSkinCombo->getSelectedValue().asString() : "default";
    refreshSkinThemeList();
    m_SkinTheme = (m_pSkinThemeCombo) ? m_pSkinThemeCombo->getSelectedValue().asString() : "";

    m_SkinName = m_pSkinCombo->getSelectedItemLabel();
    m_SkinThemeName = m_pSkinThemeCombo->getSelectedItemLabel();
    refreshPreviewImage(); // <FS:PP> FIRE-1689: Skins preview image
}

void LLPanelPreferenceSkins::onSkinThemeChanged()
{
    m_SkinTheme = (m_pSkinThemeCombo) ? m_pSkinThemeCombo->getSelectedValue().asString() : "";
    m_SkinThemeName = m_pSkinThemeCombo->getSelectedItemLabel();
    refreshPreviewImage(); // <FS:PP> FIRE-1689: Skins preview image
}

void LLPanelPreferenceSkins::refreshSkinList()
{
    if (!m_pSkinCombo)
        return;

    m_pSkinCombo->clearRows();
    for (LLSD::array_const_iterator itSkinInfo = m_SkinsInfo.beginArray(), endSkinInfo = m_SkinsInfo.endArray();
            itSkinInfo != endSkinInfo; ++itSkinInfo)
    {
        const LLSD& sdSkin = *itSkinInfo;
        std::string strPath = gDirUtilp->getSkinBaseDir();
        gDirUtilp->append(strPath, sdSkin["folder"].asString());
        if (gDirUtilp->fileExists(strPath))
        {
            m_pSkinCombo->add(sdSkin["name"].asString(), sdSkin["folder"]);
        }
    }

    bool fFound = m_pSkinCombo->setSelectedByValue(m_Skin, true);
    if (!fFound)
    {
        m_pSkinCombo->setSelectedByValue("default", true);
    }

    refreshSkinThemeList();
}

void LLPanelPreferenceSkins::refreshSkinThemeList()
{
    if (!m_pSkinThemeCombo)
        return;

    m_pSkinThemeCombo->clearRows();
    for (LLSD::array_const_iterator itSkinInfo = m_SkinsInfo.beginArray(), endSkinInfo = m_SkinsInfo.endArray();
            itSkinInfo != endSkinInfo; ++itSkinInfo)
    {
        const LLSD& sdSkin = *itSkinInfo;
        if (sdSkin["folder"].asString() == m_Skin)
        {
            const LLSD& sdThemes = sdSkin["themes"];
            for (LLSD::array_const_iterator itTheme = sdThemes.beginArray(), endTheme = sdThemes.endArray(); itTheme != endTheme; ++itTheme)
            {
                const LLSD& sdTheme = *itTheme;
                std::string strPath = gDirUtilp->getSkinBaseDir();
                gDirUtilp->append(strPath, sdSkin["folder"].asString());
                gDirUtilp->append(strPath, "themes");
                gDirUtilp->append(strPath, sdTheme["folder"].asString());
                if ( (gDirUtilp->fileExists(strPath)) || (sdTheme["folder"].asString().empty()) )
                {
                    m_pSkinThemeCombo->add(sdTheme["name"].asString(), sdTheme["folder"]);
                }
            }
            break;
        }
    }

    bool fFound = m_pSkinThemeCombo->setSelectedByValue(m_SkinTheme, true);
    if (!fFound)
    {
        m_pSkinThemeCombo->selectFirstItem();
    }
}
// [/SL:KB]

// <FS:PP> FIRE-1689: Skins preview image
void LLPanelPreferenceSkins::refreshPreviewImage()
{
    std::string previewImageName = "skin " + m_SkinName + " " + m_SkinThemeName;
    LLStringUtil::toLower(previewImageName);
    m_pSkinPreview->setImages(previewImageName, previewImageName);
}
// </FS:PP>

// <FS:Zi> Backup Settings
static void copy_prefs_file(const std::string& from, const std::string& to)
{
    LL_INFOS() << "Copying " << from << " to " << to << LL_ENDL;

    std::error_code ec;
    if (!std::filesystem::copy_file(from, to, std::filesystem::copy_options::overwrite_existing, ec) || ec)
        LL_WARNS() << "Couldn't copy file: " << ec.message() << LL_ENDL;
}

static LLPanelInjector<FSPanelPreferenceBackup> t_pref_backup("panel_preference_backup");

FSPanelPreferenceBackup::FSPanelPreferenceBackup() : LLPanelPreference()
{
    mCommitCallbackRegistrar.add("Pref.SetBackupSettingsPath",  boost::bind(&FSPanelPreferenceBackup::onClickSetBackupSettingsPath, this));
    mCommitCallbackRegistrar.add("Pref.BackupSettings",         boost::bind(&FSPanelPreferenceBackup::onClickBackupSettings, this));
    mCommitCallbackRegistrar.add("Pref.RestoreSettings",        boost::bind(&FSPanelPreferenceBackup::onClickRestoreSettings, this));
    mCommitCallbackRegistrar.add("Pref.BackupSelectAll",        boost::bind(&FSPanelPreferenceBackup::onClickSelectAll, this));
    mCommitCallbackRegistrar.add("Pref.BackupDeselectAll",      boost::bind(&FSPanelPreferenceBackup::onClickDeselectAll, this));
}

bool FSPanelPreferenceBackup::postBuild()
{
    // <FS:Zi> Backup Settings
    // Apparently, line editors don't update with their settings controls, so do that manually here
    std::string dir_name = gSavedSettings.getString("SettingsBackupPath");
    getChild<LLLineEditor>("settings_backup_path")->setValue(dir_name);
    // </FS:Zi>

    // <FS:Beq>
    #if !defined OPENSIM
    // Note: Windlight setting restore is enabled in OPENSIM bulds irrespective of grid (or pre-login)
    // windlights settings folders are not grid specific and thus neither is the restore.
    // if windlight folders existsed they will be backed up on all builds but for SL only builds they will not be restored.

    LLScrollListCtrl* globalFoldersScrollList = getChild<LLScrollListCtrl>("restore_global_folders_list");
    std::vector<LLScrollListItem*> globalFoldersList = globalFoldersScrollList->getAllData();
    for (const auto item : globalFoldersList)
    {
        // if it is windlight related remove it.
        if (item->getValue().asString().rfind("windlight",0) == 0)
        {
            LL_INFOS() << "removing windlight folder (no longer used in SL) : " << item->getValue().asString() << " index: " << globalFoldersScrollList->getItemIndex(item) << LL_ENDL;
            globalFoldersScrollList->deleteSingleItem(globalFoldersScrollList->getItemIndex(item));
        }
    }
    #endif
    // </FS:Beq>
    return LLPanelPreference::postBuild();
}

void FSPanelPreferenceBackup::onClickSetBackupSettingsPath()
{
    std::string dir_name = gSavedSettings.getString("SettingsBackupPath");
    (new LLDirPickerThread(boost::bind(&FSPanelPreferenceBackup::changeBackupSettingsPath, this, _1, _2), dir_name))->getFile();
}

void FSPanelPreferenceBackup::changeBackupSettingsPath(const std::vector<std::string>& filenames, std::string proposed_name)
{
    std::string dir_name = filenames[0];
    if (!dir_name.empty() && dir_name != proposed_name)
    {
        gSavedSettings.setString("SettingsBackupPath", dir_name);
        getChild<LLLineEditor>("settings_backup_path")->setValue(dir_name);
    }
}

void FSPanelPreferenceBackup::onClickBackupSettings()
{

    LLSD args;
    args["DIRECTORY"] = gSavedSettings.getString("SettingsBackupPath");
    LLNotificationsUtil::add("SettingsConfirmBackup", args, LLSD(),
        boost::bind(&FSPanelPreferenceBackup::doBackupSettings, this, _1, _2));
}

void FSPanelPreferenceBackup::doBackupSettings(const LLSD& notification, const LLSD& response)
{
    LL_INFOS("SettingsBackup") << "entered" << LL_ENDL;

    S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
    if ( option == 1 ) // CANCEL
    {
        LL_INFOS("SettingsBackup") << "backup cancelled" << LL_ENDL;
        return;
    }

    // Get settings backup path
    std::string dir_name = gSavedSettings.getString("SettingsBackupPath");

    // If we don't have a path yet, ask the user
    if (dir_name.empty())
    {
        LL_INFOS("SettingsBackup") << "ask user for backup path" << LL_ENDL;
        onClickSetBackupSettingsPath();
    }

    // Remember the backup path
    dir_name = gSavedSettings.getString("SettingsBackupPath");

    // If the backup path is still empty, complain to the user and do nothing else
    if (dir_name.empty())
    {
        LL_INFOS("SettingsBackup") << "backup path empty" << LL_ENDL;
        LLNotificationsUtil::add("BackupPathEmpty");
        return;
    }

    // Try to make sure the folder exists
    LLFile::mkdir(dir_name.c_str());
    // If the folder is still not there, give up
    if (!LLFile::isdir(dir_name.c_str()))
    {
        LL_WARNS("SettingsBackup") << "backup path does not exist or could not be created" << LL_ENDL;
        LLNotificationsUtil::add("BackupPathDoesNotExistOrCreateFailed");
        return;
    }

    // define a couple of control groups to store the settings to back up
    LLControlGroup backup_global_controls("BackupGlobal");
    LLControlGroup backup_per_account_controls("BackupPerAccount");

    // functor that will go over all settings in a control group and copy the ones that are
    // meant to be backed up
    struct f : public LLControlGroup::ApplyFunctor
    {
        LLControlGroup* group;  // our control group that will hold the backup controls
        f(LLControlGroup* g) : group(g) {}  // constructor, initializing group variable
        virtual void apply(const std::string& name, LLControlVariable* control)
        {
            if (!control->isPersisted() && !control->isBackupable())
            {
                LL_INFOS("SettingsBackup") << "Settings control " << control->getName() << ": non persistant controls don't need to be set not backupable." << LL_ENDL;
                return;
            }

            // only backup settings that are not default, are persistent an are marked as "safe" to back up
            if (!control->isDefault() && control->isPersisted() && control->isBackupable())
            {
                LL_WARNS() << control->getName() << LL_ENDL;
                // copy the control to our backup group
                (*group).declareControl(
                    control->getName(),
                    control->type(),
                    control->getValue(),
                    control->getComment(),
                    SANITY_TYPE_NONE,
                    LLSD(),
                    std::string(),
                    LLControlVariable::PERSIST_NONDFT); // need to set persisitent flag, or it won't be saved
            }
        }
    } func_global(&backup_global_controls), func_per_account(&backup_per_account_controls);

    // run backup on global controls
    LL_INFOS("SettingsBackup") << "running functor on global settings" << LL_ENDL;
    gSavedSettings.applyToAll(&func_global);

    // make sure to write color preferences before copying them
    LL_INFOS("SettingsBackup") << "saving UI color table" << LL_ENDL;
    LLUIColorTable::instance().saveUserSettings();

    // set it to save defaults, too (false), because our declaration automatically
    // makes the value default
    std::string backup_global_name = gDirUtilp->getExpandedFilename(LL_PATH_NONE, dir_name,
                LLAppViewer::instance()->getSettingsFilename("Default","Global"));
    LL_INFOS("SettingsBackup") << "saving backup global settings" << LL_ENDL;
    backup_global_controls.saveToFile(backup_global_name, false);

    // Get scroll list control that holds the list of global files
    LLScrollListCtrl* globalScrollList = getChild<LLScrollListCtrl>("restore_global_files_list");
    // Pull out all data
    std::vector<LLScrollListItem*> globalFileList = globalScrollList->getAllData();
    // Go over each entry
    for (size_t index = 0; index < globalFileList.size(); ++index)
    {
        // Get the next item in the list
        LLScrollListItem* item = globalFileList[index];
        // Don't bother with the checkbox and get the path, since we back up all files
        // and only restore selectively
        std::string file = item->getColumn(2)->getValue().asString();
        LL_INFOS("SettingsBackup") << "copying global file " << file << LL_ENDL;
        copy_prefs_file(
            gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, file),
            gDirUtilp->getExpandedFilename(LL_PATH_NONE, dir_name, file));
    }

    // Only back up per-account settings when the path is available, meaning, the user
    // has logged in
    std::string per_account_name = gDirUtilp->getExpandedFilename(LL_PATH_PER_SL_ACCOUNT,
                LLAppViewer::instance()->getSettingsFilename("Default", "PerAccount"));
    if (!per_account_name.empty())
    {
        // get path and file names to the relevant settings files
        std::string userlower = gDirUtilp->getBaseFileName(gDirUtilp->getLindenUserDir(), false);
        std::string backup_per_account_folder = dir_name+gDirUtilp->getDirDelimiter() + userlower;
        std::string backup_per_account_name = gDirUtilp->getExpandedFilename(LL_PATH_NONE, backup_per_account_folder,
                    LLAppViewer::instance()->getSettingsFilename("Default", "PerAccount"));

        // Make sure to persist settings to file before we copy them
        FSAvatarRenderPersistence::instance().saveAvatarRenderSettings();

        LL_INFOS("SettingsBackup") << "copying per account settings" << LL_ENDL;
        // create per-user folder if it doesn't exist yet
        LLFile::mkdir(backup_per_account_folder.c_str());

        // check if the path is actually a folder
        if (LLFile::isdir(backup_per_account_folder.c_str()))
        {
            // run backup on per-account controls
            LL_INFOS("SettingsBackup") << "running functor on per account settings" << LL_ENDL;
            gSavedPerAccountSettings.applyToAll(&func_per_account);
            // save defaults here as well (false)
            LL_INFOS("SettingsBackup") << "saving backup per account settings" << LL_ENDL;
            backup_per_account_controls.saveToFile(backup_per_account_name, false);

            // Get scroll list control that holds the list of per account files
            LLScrollListCtrl* perAccountScrollList = getChild<LLScrollListCtrl>("restore_per_account_files_list");
            // Pull out all data
            std::vector<LLScrollListItem*> perAccountFileList = perAccountScrollList->getAllData();
            // Go over each entry
            for (size_t index = 0; index < perAccountFileList.size(); ++index)
            {

                // Get the next item in the list
                LLScrollListItem* item = perAccountFileList[index];
                // Don't bother with the checkbox and get the path, since we back up all files
                // and only restore selectively

                std::string file = item->getColumn(2)->getValue().asString();
                LL_INFOS("SettingsBackup") << "copying per account file " << file << LL_ENDL;
                copy_prefs_file(
                    gDirUtilp->getExpandedFilename(LL_PATH_PER_SL_ACCOUNT, file),
                    gDirUtilp->getExpandedFilename(LL_PATH_NONE, backup_per_account_folder, file));
            }
        }
        else
        {
            LL_WARNS("SettingsBackup") << backup_per_account_folder << " is not a folder. Per account settings save aborted." << LL_ENDL;
        }
    }

    // Get scroll list control that holds the list of global folders
    LLScrollListCtrl* globalFoldersScrollList = getChild<LLScrollListCtrl>("restore_global_folders_list");
    // Pull out all data
    std::vector<LLScrollListItem*> globalFoldersList = globalFoldersScrollList->getAllData();
    // Go over each entry
    for (size_t index = 0; index < globalFoldersList.size(); ++index)
    {
        // Get the next item in the list
        LLScrollListItem* item = globalFoldersList[index];
        // Don't bother with the checkbox and get the path, since we back up all folders
        // and only restore selectively
        if (item->getValue().asString() != "presets")
        {
            std::string folder = item->getColumn(2)->getValue().asString();

            std::string folder_name = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, folder) + gDirUtilp->getDirDelimiter();
            std::string backup_folder_name = gDirUtilp->getExpandedFilename(LL_PATH_NONE, dir_name, folder) + gDirUtilp->getDirDelimiter();

            LL_INFOS("SettingsBackup") << "backing up global folder: " << folder_name << LL_ENDL;

            // create folder if it's not there already
            LLFile::mkdir(backup_folder_name.c_str());

            std::string file_name;
            while (gDirUtilp->getNextFileInDir(folder_name, "*", file_name))
            {
                LL_INFOS("SettingsBackup") << "found entry: " << folder_name + file_name << LL_ENDL;
                // only copy files, not subfolders
                if (LLFile::isfile(folder_name + file_name.c_str()))
                {
                    copy_prefs_file(folder_name + file_name, backup_folder_name + file_name);
                }
                else
                {
                    LL_INFOS("SettingsBackup") << "skipping subfolder " << folder_name + file_name << LL_ENDL;
                }
            }
        }
        else
        {
            LLFile::mkdir(gDirUtilp->getExpandedFilename(LL_PATH_NONE, dir_name, PRESETS_DIR));

            std::string presets_folder = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, PRESETS_DIR) + gDirUtilp->getDirDelimiter();
            std::string graphics_presets_folder = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, PRESETS_DIR, PRESETS_GRAPHIC) + gDirUtilp->getDirDelimiter();
            std::string camera_presets_folder =  gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, PRESETS_DIR, PRESETS_CAMERA) + gDirUtilp->getDirDelimiter();

            if (LLFile::isdir(graphics_presets_folder))
            {
                LLFile::mkdir(gDirUtilp->getExpandedFilename(LL_PATH_NONE, dir_name, PRESETS_DIR, PRESETS_GRAPHIC));

                std::string file_name;
                while (gDirUtilp->getNextFileInDir(graphics_presets_folder, "*", file_name))
                {
                    std::string source = gDirUtilp->getExpandedFilename(LL_PATH_NONE, graphics_presets_folder, file_name);

                    if (LLFile::isfile(source.c_str()))
                    {
                        std::string target = gDirUtilp->add(gDirUtilp->add(gDirUtilp->add(dir_name, PRESETS_DIR), PRESETS_GRAPHIC), file_name);
                        copy_prefs_file(source, target);
                    }
                }
            }

            if (LLFile::isdir(camera_presets_folder))
            {
                LLFile::mkdir(gDirUtilp->getExpandedFilename(LL_PATH_NONE, dir_name, PRESETS_DIR, PRESETS_CAMERA));

                std::string file_name;
                while (gDirUtilp->getNextFileInDir(camera_presets_folder, "*", file_name))
                {
                    std::string source = gDirUtilp->getExpandedFilename(LL_PATH_NONE, camera_presets_folder, file_name);

                    if (LLFile::isfile(source.c_str()))
                    {
                        std::string target = gDirUtilp->add(gDirUtilp->add(gDirUtilp->add(dir_name, PRESETS_DIR), PRESETS_CAMERA), file_name);
                        copy_prefs_file(source, target);
                    }
                }
            }
        }
    }

    LLNotificationsUtil::add("BackupFinished");
}

void FSPanelPreferenceBackup::onClickRestoreSettings()
{
    // ask the user if they really want to restore and restart
    LLNotificationsUtil::add("SettingsRestoreNeedsLogout", LLSD(), LLSD(), boost::bind(&FSPanelPreferenceBackup::doRestoreSettings, this, _1, _2));
}

void FSPanelPreferenceBackup:: doRestoreSettings(const LLSD& notification, const LLSD& response)
{
    LL_INFOS("SettingsBackup") << "entered" << LL_ENDL;
    // Check the user's answer about restore and restart
    S32 option = LLNotificationsUtil::getSelectedOption(notification, response);

    // If canceled, do nothing
    if (option == 1)
    {
        LL_INFOS("SettingsBackup") << "restore canceled" << LL_ENDL;
        return;
    }

    // Get settings backup path
    std::string dir_name = gSavedSettings.getString("SettingsBackupPath");

    // Backup path is empty, ask the user where to find the backup
    if (dir_name.empty())
    {
        LL_INFOS("SettingsBackup") << "ask user for path to restore from" << LL_ENDL;
        onClickSetBackupSettingsPath();
    }

    // Remember the backup path
    dir_name = gSavedSettings.getString("SettingsBackupPath");

    // If the backup path is still empty, complain to the user and do nothing else
    if (dir_name.empty())
    {
        LL_INFOS("SettingsBackup") << "restore path empty" << LL_ENDL;
        LLNotificationsUtil::add("BackupPathEmpty");
        return;
    }

    // If the path does not exist, give up
    if (!LLFile::isdir(dir_name.c_str()))
    {
        LL_INFOS("SettingsBackup") << "backup path does not exist" << LL_ENDL;
        LLNotificationsUtil::add("BackupPathDoesNotExist");
        return;
    }

    // Close the window so the restored settings can't be destroyed by the user
    LLFloaterPreference* instance = LLFloaterReg::findTypedInstance<LLFloaterPreference>("preferences");
    if (instance)
    {
        instance->onBtnOK(LLSD());
    }

    if (gSavedSettings.getBOOL("RestoreGlobalSettings"))
    {
        // Get path and file names to backup and restore settings path
        std::string global_name = gSavedSettings.getString("ClientSettingsFile");
        std::string backup_global_name = gDirUtilp->getExpandedFilename(LL_PATH_NONE, dir_name,
                    LLAppViewer::instance()->getSettingsFilename("Default", "Global"));

        // start clean
        LL_INFOS("SettingsBackup") << "clearing global settings" << LL_ENDL;
        gSavedSettings.resetToDefaults();
        LLFeatureManager::getInstance()->applyRecommendedSettings();  

        // run restore on global controls
        LL_INFOS("SettingsBackup") << "restoring global settings from backup" << LL_ENDL;
        gSavedSettings.loadFromFile(backup_global_name);
        LL_INFOS("SettingsBackup") << "saving global settings" << LL_ENDL;
        gSavedSettings.saveToFile(global_name, true);
    }

    // Get scroll list control that holds the list of global files
    LLScrollListCtrl* globalScrollList = getChild<LLScrollListCtrl>("restore_global_files_list");
    // Pull out all data
    std::vector<LLScrollListItem*> globalFileList = globalScrollList->getAllData();
    // Go over each entry
    for (size_t index = 0; index < globalFileList.size(); ++index)
    {
        // Get the next item in the list
        LLScrollListItem* item = globalFileList[index];
        // Look at the first column and make sure it's a checkbox control
        LLScrollListCheck* checkbox = dynamic_cast<LLScrollListCheck*>(item->getColumn(0));
        if (!checkbox)
            continue;
        // Only restore if this item is checked on
        if (checkbox->getCheckBox()->getValue().asBoolean())
        {
            // Get the path to restore for this item
            std::string file = item->getColumn(2)->getValue().asString();
            LL_INFOS("SettingsBackup") << "copying global file " << file << LL_ENDL;
            copy_prefs_file(
                gDirUtilp->getExpandedFilename(LL_PATH_NONE, dir_name, file),
                gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, file));
        }
    }

    // Only restore per-account settings when the path is available
    std::string per_account_name = gDirUtilp->getExpandedFilename(LL_PATH_PER_SL_ACCOUNT,
                LLAppViewer::instance()->getSettingsFilename("Default", "PerAccount"));
    if (!per_account_name.empty())
    {
        // Get path and file names to the relevant settings files
        std::string userlower = gDirUtilp->getBaseFileName(gDirUtilp->getLindenUserDir(), false);
        std::string backup_per_account_folder = dir_name + gDirUtilp->getDirDelimiter() + userlower;
        std::string backup_per_account_name = gDirUtilp->getExpandedFilename(LL_PATH_NONE, backup_per_account_folder,
                    LLAppViewer::instance()->getSettingsFilename("Default", "PerAccount"));

        if (gSavedSettings.getBOOL("RestorePerAccountSettings"))
        {
            // run restore on per-account controls
            LL_INFOS("SettingsBackup") << "restoring per account settings" << LL_ENDL;
            gSavedPerAccountSettings.loadFromFile(backup_per_account_name);
            LL_INFOS("SettingsBackup") << "saving per account settings" << LL_ENDL;
            gSavedPerAccountSettings.saveToFile(per_account_name, true);
        }

        // Get scroll list control that holds the list of per account files
        LLScrollListCtrl* perAccountScrollList = getChild<LLScrollListCtrl>("restore_per_account_files_list");
        // Pull out all data
        std::vector<LLScrollListItem*> perAccountFileList = perAccountScrollList->getAllData();
        // Go over each entry
        for (size_t index = 0; index < perAccountFileList.size(); ++index)
        {
            // Get the next item in the list
            LLScrollListItem* item = perAccountFileList[index];
            // Look at the first column and make sure it's a checkbox control
            LLScrollListCheck* checkbox = dynamic_cast<LLScrollListCheck*>(item->getColumn(0));
            if (!checkbox)
                continue;
            // Only restore if this item is checked on
            if (checkbox->getCheckBox()->getValue().asBoolean())
            {
                // Get the path to restore for this item
                std::string file = item->getColumn(2)->getValue().asString();
                LL_INFOS("SettingsBackup") << "copying per account file " << file << LL_ENDL;
                copy_prefs_file(
                    gDirUtilp->getExpandedFilename(LL_PATH_NONE, backup_per_account_folder, file),
                    gDirUtilp->getExpandedFilename(LL_PATH_PER_SL_ACCOUNT, file));
            }
        }

        // toolbars get overwritten when LLToolbarView is destroyed, so make sure
        // the toolbars are updated here already
        LL_INFOS("SettingsBackup") << "clearing toolbars" << LL_ENDL;
        gToolBarView->clearToolbars();
        LL_INFOS("SettingsBackup") << "reloading toolbars" << LL_ENDL;
        gToolBarView->loadToolbars(false);
#ifdef OPENSIM
        if (LLGridManager::instance().isInOpenSim())
        {
            LL_INFOS("SettingsBackup") << "reloading group mute list" << LL_ENDL;
            exoGroupMuteList::instance().loadMuteList();
        }
#endif
        FSAvatarRenderPersistence::instance().loadAvatarRenderSettings();

        LLPanelMainInventory::sSaveFilters = false;
        LLFavoritesOrderStorage::mSaveOnExit = false;
    }

    // Get scroll list control that holds the list of global folders
    LLScrollListCtrl* globalFoldersScrollList = getChild<LLScrollListCtrl>("restore_global_folders_list");
    // Pull out all data
    std::vector<LLScrollListItem*> globalFoldersList = globalFoldersScrollList->getAllData();
    // Go over each entry
    for (size_t index = 0; index < globalFoldersList.size(); ++index)
    {
        // Get the next item in the list
        LLScrollListItem* item = globalFoldersList[index];
        // Look at the first column and make sure it's a checkbox control
        LLScrollListCheck* checkbox = dynamic_cast<LLScrollListCheck*>(item->getColumn(0));
        if (!checkbox)
            continue;
        // Only restore if this item is checked on
        if (checkbox->getCheckBox()->getValue().asBoolean())
        {
            if (item->getValue().asString() != "presets")
            {
                // Get the path to restore for this item
                std::string folder = item->getColumn(2)->getValue().asString();

                std::string folder_name = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, folder) + gDirUtilp->getDirDelimiter();
                std::string backup_folder_name = gDirUtilp->getExpandedFilename(LL_PATH_NONE, dir_name, folder) + gDirUtilp->getDirDelimiter();

                LL_INFOS("SettingsBackup") << "restoring global folder: " << folder_name << LL_ENDL;

                // create folder if it's not there already
                LLFile::mkdir(folder_name.c_str());

                std::string file_name;
                while (gDirUtilp->getNextFileInDir(backup_folder_name, "*", file_name))
                {
                    LL_INFOS("SettingsBackup") << "found entry: " << backup_folder_name + file_name << LL_ENDL;
                    // only restore files, not subfolders
                    if (LLFile::isfile(backup_folder_name + file_name.c_str()))
                    {
                        copy_prefs_file(backup_folder_name + file_name, folder_name + file_name);
                    }
                    else
                    {
                        LL_INFOS("SettingsBackup") << "skipping subfolder " << backup_folder_name + file_name << LL_ENDL;
                    }
                }
            }
            else
            {
                LLFile::mkdir(gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, PRESETS_DIR));

                std::string presets_folder = gDirUtilp->getExpandedFilename(LL_PATH_NONE, dir_name, PRESETS_DIR) + gDirUtilp->getDirDelimiter();
                std::string graphics_presets_folder = gDirUtilp->getExpandedFilename(LL_PATH_NONE, dir_name, PRESETS_DIR, PRESETS_GRAPHIC) + gDirUtilp->getDirDelimiter();
                std::string camera_presets_folder =  gDirUtilp->getExpandedFilename(LL_PATH_NONE, dir_name, PRESETS_DIR, PRESETS_CAMERA) + gDirUtilp->getDirDelimiter();

                if (LLFile::isdir(graphics_presets_folder))
                {
                    LLFile::mkdir(gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, PRESETS_DIR, PRESETS_GRAPHIC));

                    std::string file_name;
                    while (gDirUtilp->getNextFileInDir(graphics_presets_folder, "*", file_name))
                    {
                        std::string source = gDirUtilp->getExpandedFilename(LL_PATH_NONE, graphics_presets_folder, file_name);

                        if (LLFile::isfile(source.c_str()))
                        {
                            std::string target = gDirUtilp->add(gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, PRESETS_DIR, PRESETS_GRAPHIC), file_name);
                            copy_prefs_file(source, target);
                        }
                    }
                }

                if (LLFile::isdir(camera_presets_folder))
                {
                    LLFile::mkdir(gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, PRESETS_DIR, PRESETS_CAMERA));

                    std::string file_name;
                    while (gDirUtilp->getNextFileInDir(camera_presets_folder, "*", file_name))
                    {
                        std::string source = gDirUtilp->getExpandedFilename(LL_PATH_NONE, camera_presets_folder, file_name);

                        if (LLFile::isfile(source.c_str()))
                        {
                            std::string target = gDirUtilp->add(gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, PRESETS_DIR, PRESETS_CAMERA), file_name);
                            copy_prefs_file(source, target);
                        }
                    }
                }
            }
        }
    }
    // <FS:CR> Set this true so we can update newer settings with their deprecated counterparts on next launch
    gSavedSettings.setBOOL("FSFirstRunAfterSettingsRestore", true);

    // Tell the user we have finished restoring settings and the viewer must shut down
    LLNotificationsUtil::add("RestoreFinished", LLSD(), LLSD(), boost::bind(&FSPanelPreferenceBackup::onQuitConfirmed, this, _1, _2));
}

// User confirmed the shutdown and we proceed
void FSPanelPreferenceBackup::onQuitConfirmed(const LLSD& notification,const LLSD& response)
{
    // Make sure the viewer will not save any settings on exit, so our copied files will survive
    LLAppViewer::instance()->setSaveSettingsOnExit(false);
    // Quit the viewer so all gets saved immediately
    LL_INFOS("SettingsBackup") << "setting to quit" << LL_ENDL;
    LLAppViewer::instance()->requestQuit();
}

void FSPanelPreferenceBackup::onClickSelectAll()
{
    doSelect(true);
}

void FSPanelPreferenceBackup::onClickDeselectAll()
{
    doSelect(false);
}

void FSPanelPreferenceBackup::doSelect(bool all)
{
    // Get scroll list control that holds the list of global files
    LLScrollListCtrl* globalScrollList = getChild<LLScrollListCtrl>("restore_global_files_list");
    // Get scroll list control that holds the list of per account files
    LLScrollListCtrl* perAccountScrollList = getChild<LLScrollListCtrl>("restore_per_account_files_list");
    // Get scroll list control that holds the list of global folders
    LLScrollListCtrl* globalFoldersScrollList = getChild<LLScrollListCtrl>("restore_global_folders_list");

    applySelection(globalScrollList, all);
    applySelection(perAccountScrollList, all);
    applySelection(globalFoldersScrollList, all);
}

void FSPanelPreferenceBackup::applySelection(LLScrollListCtrl* control, bool all)
{
    // Pull out all data
    std::vector<LLScrollListItem*> itemList = control->getAllData();
    // Go over each entry
    for (size_t index = 0; index < itemList.size(); ++index)
    {
        // Get the next item in the list
        LLScrollListItem* item = itemList[index];
        // Check/uncheck the box only when the item is enabled
        if (item->getEnabled())
        {
            // Look at the first column and make sure it's a checkbox control
            LLScrollListCheck* checkbox = dynamic_cast<LLScrollListCheck*>(item->getColumn(0));
            if (checkbox)
            {
                checkbox->getCheckBox()->setValue(all);
            }
        }
    }
}
// </FS:Zi>

// <FS:Kadah>
void LLFloaterPreference::loadFontPresetsFromDir(const std::string& dir, LLComboBox* font_selection_combo)
{
    LLDirIterator dir_iter(dir, "*.xml");
    std::string file;
    while (dir_iter.next(file))
    {
        //hack to deal with "fonts.xml"
        if (file == "fonts.xml")
        {
            font_selection_combo->add("Deja Vu", file);
        }
        //hack to get "fonts_[name].xml" to "Name"
        else
        {
            std::string fontpresetname = file.substr(6, file.length() - 10);
            LLStringUtil::replaceChar(fontpresetname, '_', ' ');
            fontpresetname[0] = LLStringOps::toUpper(fontpresetname[0]);
            font_selection_combo->add(fontpresetname, file);
        }
    }
}

void LLFloaterPreference::populateFontSelectionCombo()
{
    LLComboBox* font_selection_combo = getChild<LLComboBox>("Fontsettingsfile");
    if (font_selection_combo)
    {
        const std::string fontDir(gDirUtilp->getExpandedFilename(LL_PATH_FONTS, "", ""));
        const std::string userfontDir(gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS , "fonts", ""));

        // Load fonts.xmls from the install dir first then user_settings
        loadFontPresetsFromDir(fontDir, font_selection_combo);
        loadFontPresetsFromDir(userfontDir, font_selection_combo);

        font_selection_combo->setValue(gSavedSettings.getString("FSFontSettingsFile"));
    }
}
// </FS:Kadah>

// <FS:AW optional opensim support>
static LLPanelInjector<LLPanelPreferenceOpensim> t_pref_opensim("panel_preference_opensim");

#ifdef OPENSIM
LLPanelPreferenceOpensim::LLPanelPreferenceOpensim() : LLPanelPreference(),
    mGridListControl(NULL),
    mGridListChangedCallbackConnection(),
    mGridAddedCallbackConnection()
{
    mCommitCallbackRegistrar.add("Pref.ClearDebugSearchURL", boost::bind(&LLPanelPreferenceOpensim::onClickClearDebugSearchURL, this));
    mCommitCallbackRegistrar.add("Pref.PickDebugSearchURL", boost::bind(&LLPanelPreferenceOpensim::onClickPickDebugSearchURL, this));
    mCommitCallbackRegistrar.add("Pref.AddGrid", boost::bind(&LLPanelPreferenceOpensim::onClickAddGrid, this));
    mCommitCallbackRegistrar.add("Pref.ClearGrid", boost::bind(&LLPanelPreferenceOpensim::onClickClearGrid, this));
    mCommitCallbackRegistrar.add("Pref.RefreshGrid", boost::bind( &LLPanelPreferenceOpensim::onClickRefreshGrid, this));
    mCommitCallbackRegistrar.add("Pref.RemoveGrid", boost::bind( &LLPanelPreferenceOpensim::onClickRemoveGrid, this));
}

LLPanelPreferenceOpensim::~LLPanelPreferenceOpensim()
{
    if (mGridListChangedCallbackConnection.connected())
    {
        mGridListChangedCallbackConnection.disconnect();
    }

    if (mGridAddedCallbackConnection.connected())
    {
        mGridAddedCallbackConnection.disconnect();
    }
}

bool LLPanelPreferenceOpensim::postBuild()
{
    mEditorGridName = findChild<LLLineEditor>("name_edit");
    mEditorGridURI = findChild<LLLineEditor>("grid_uri_edit");
    mEditorLoginPage = findChild<LLLineEditor>("login_page_edit");
    mEditorHelperURI = findChild<LLLineEditor>("helper_uri_edit");
    mEditorWebsite = findChild<LLLineEditor>("website_edit");
    mEditorSupport = findChild<LLLineEditor>("support_edit");
    mEditorRegister = findChild<LLLineEditor>("register_edit");
    mEditorPassword = findChild<LLLineEditor>("password_edit");
    mEditorSearch = findChild<LLLineEditor>("search_edit");
    mEditorGridMessage = findChild<LLLineEditor>("message_edit");
    mGridListControl = getChild<LLScrollListCtrl>("grid_list");
    mGridListControl->setCommitCallback(boost::bind(&LLPanelPreferenceOpensim::onSelectGrid, this));
    mGridListChangedCallbackConnection = LLGridManager::getInstance()->addGridListChangedCallback(boost::bind(&LLPanelPreferenceOpensim::refreshGridList, this, _1));
    refreshGridList();

    return LLPanelPreference::postBuild();
}

void LLPanelPreferenceOpensim::onOpen(const LLSD& key)
{
    mCurrentGrid = LLGridManager::getInstance()->getGrid();

    mEditorGridName->clear();
    mEditorGridURI->clear();
    mEditorLoginPage->clear();
    mEditorHelperURI->clear();
    mEditorWebsite->clear();
    mEditorSupport->clear();
    mEditorRegister->clear();
    mEditorPassword->clear();
    mEditorSearch->clear();
    mEditorGridMessage->clear();
}

void LLPanelPreferenceOpensim::onSelectGrid()
{
    LLSD grid_info;
    std::string grid = mGridListControl->getSelectedValue();
    LLGridManager::getInstance()->getGridData(grid, grid_info);

    mEditorGridName->setText(grid_info[GRID_LABEL_VALUE].asString());
    mEditorGridURI->setText(grid_info[GRID_LOGIN_URI_VALUE][0].asString());
    mEditorLoginPage->setText(grid_info[GRID_LOGIN_PAGE_VALUE].asString());
    mEditorHelperURI->setText(grid_info[GRID_HELPER_URI_VALUE].asString());
    mEditorWebsite->setText(grid_info["about"].asString());
    mEditorSupport->setText(grid_info["help"].asString());
    mEditorRegister->setText(grid_info[GRID_REGISTER_NEW_ACCOUNT].asString());
    mEditorPassword->setText(grid_info[GRID_FORGOT_PASSWORD].asString());
    mEditorSearch->setText(grid_info["search"].asString());
    mEditorGridMessage->setText(grid_info["message"].asString());
}

void LLPanelPreferenceOpensim::apply()
{
    LLGridManager::getInstance()->saveGridList();
    FSPanelLogin::updateServer();
}

void LLPanelPreferenceOpensim::cancel(const std::vector<std::string> settings_to_skip)
{
    LLGridManager::getInstance()->resetGrids();
    LLGridManager::getInstance()->setGridChoice(mCurrentGrid);
    FSPanelLogin::updateServer();
}

void LLPanelPreferenceOpensim::onClickAddGrid()
{
    std::string new_grid = getChild<LLLineEditor>("add_grid")->getText();

    if (!new_grid.empty())
    {
        getChild<LLUICtrl>("grid_management_panel")->setEnabled(false);
        if (mGridAddedCallbackConnection.connected())
        {
            mGridAddedCallbackConnection.disconnect();
        }
        LLGridManager::getInstance()->addGridListChangedCallback(boost::bind(&LLPanelPreferenceOpensim::addedGrid, this, _1));
        LLGridManager::getInstance()->addGrid(new_grid);
    }
}

void LLPanelPreferenceOpensim::addedGrid(bool success)
{
    if (mGridAddedCallbackConnection.connected())
    {
        mGridAddedCallbackConnection.disconnect();
    }

    if (success)
    {
        const std::string& new_grid = getChild<LLLineEditor>("add_grid")->getText();

        for (auto row : mGridListControl->getAllData())
        {
            if (new_grid.find(row->getColumn(1)->getValue().asString()) != std::string::npos)
            {
                row->setSelected(true);
                mGridListControl->scrollToShowSelected();
                onSelectGrid();
                break;
            }
        }

        onClickClearGrid();
    }
}

void LLPanelPreferenceOpensim::onClickClearGrid()
{
    getChild<LLLineEditor>("add_grid")->clear();
}

void LLPanelPreferenceOpensim::onClickRefreshGrid()
{
    std::string grid = mGridListControl->getSelectedValue();
    getChild<LLUICtrl>("grid_management_panel")->setEnabled(false);
    LLGridManager::getInstance()->reFetchGrid(grid, (grid == LLGridManager::getInstance()->getGrid()) );
}

void LLPanelPreferenceOpensim::onClickRemoveGrid()
{
    std::string grid = mGridListControl->getSelectedValue();
    LLSD args;

    if (grid != LLGridManager::getInstance()->getGrid())
    {
        args["REMOVE_GRID"] = grid;
        LLSD payload = grid;
        LLNotificationsUtil::add("ConfirmRemoveGrid", args, payload, boost::bind(&LLPanelPreferenceOpensim::removeGridCB, this,  _1, _2));
    }
    else
    {
        args["REMOVE_GRID"] = LLGridManager::getInstance()->getGridLabel();
        LLNotificationsUtil::add("CanNotRemoveConnectedGrid", args);
    }
}

bool LLPanelPreferenceOpensim::removeGridCB(const LLSD& notification, const LLSD& response)
{
    const S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
    if (0 == option)
    {
        std::string grid = notification["payload"].asString();
        getChild<LLUICtrl>("grid_management_panel")->setEnabled(false);
        mEditorGridName->clear();
        mEditorGridURI->clear();
        mEditorLoginPage->clear();
        mEditorHelperURI->clear();
        mEditorWebsite->clear();
        mEditorSupport->clear();
        mEditorRegister->clear();
        mEditorPassword->clear();
        mEditorSearch->clear();
        mEditorGridMessage->clear();
        LLGridManager::getInstance()->removeGrid(grid);
        FSPanelLogin::updateServer();
    }
    return false;
}

void LLPanelPreferenceOpensim::refreshGridList(bool success)
{
    FSPanelLogin::updateServer();

    getChild<LLUICtrl>("grid_management_panel")->setEnabled(true);

    if (!mGridListControl)
    {
        LL_WARNS() << "No GridListControl - bug or out of memory" << LL_ENDL;
        return;
    }

    mGridListControl->operateOnAll(LLCtrlListInterface::OP_DELETE);
    mGridListControl->sortByColumnIndex(0, true);

    std::map<std::string, std::string> known_grids = LLGridManager::getInstance()->getKnownGrids();
        std::map<std::string, std::string>::iterator grid_iter = known_grids.begin();
    for(; grid_iter != known_grids.end(); grid_iter++)
    {
        if (!grid_iter->first.empty() && !grid_iter->second.empty())
        {
            LLURI login_uri = LLURI(LLGridManager::getInstance()->getLoginURI(grid_iter->first));
            LLSD element;
            const std::string connected_grid = LLGridManager::getInstance()->getGrid();

            std::string style = "NORMAL";
            if (connected_grid == grid_iter->first)
            {
                style = "BOLD";
            }

            int col = 0;
            element["id"] = grid_iter->first;
            element["columns"][col]["column"] = "grid_label";
            element["columns"][col]["value"] = grid_iter->second;
            element["columns"][col]["font"]["name"] = "SANSSERIF";
            element["columns"][col]["font"]["style"] = style;
            col++;
            element["columns"][col]["column"] = "login_uri";
            element["columns"][col]["value"] = login_uri.authority();
            element["columns"][col]["font"]["name"] = "SANSSERIF";
            element["columns"][col]["font"]["style"] = style;

            mGridListControl->addElement(element);
        }
    }
}

void LLPanelPreferenceOpensim::onClickClearDebugSearchURL()
{
    LLNotificationsUtil::add("ConfirmClearDebugSearchURL", LLSD(), LLSD(), callback_clear_debug_search);
}

void LLPanelPreferenceOpensim::onClickPickDebugSearchURL()
{

    LLNotificationsUtil::add("ConfirmPickDebugSearchURL", LLSD(), LLSD(),callback_pick_debug_search );
}
#else
void no_cb()
{ }

LLPanelPreferenceOpensim::LLPanelPreferenceOpensim() : LLPanelPreference()
{
    mCommitCallbackRegistrar.add("Pref.ClearDebugSearchURL", boost::bind(&no_cb));
    mCommitCallbackRegistrar.add("Pref.PickDebugSearchURL", boost::bind(&no_cb));
    mCommitCallbackRegistrar.add("Pref.AddGrid", boost::bind(&no_cb));
    mCommitCallbackRegistrar.add("Pref.ClearGrid", boost::bind(&no_cb));
    mCommitCallbackRegistrar.add("Pref.RefreshGrid", boost::bind(&no_cb));
    mCommitCallbackRegistrar.add("Pref.RemoveGrid", boost::bind(&no_cb));
}
LLPanelPreferenceOpensim::~LLPanelPreferenceOpensim()
{
}

#endif
// <FS:AW optional opensim support>

// <FS:Ansariel> Output device selection
static LLPanelInjector<FSPanelPreferenceSounds> t_pref_sounds("panel_preference_sounds");

FSPanelPreferenceSounds::FSPanelPreferenceSounds() :
    LLPanelPreference(),
    mOutputDevicePanel(nullptr),
    mOutputDeviceComboBox(nullptr),
    mOutputDeviceListChangedConnection()
{ }

FSPanelPreferenceSounds::~FSPanelPreferenceSounds()
{
    if (mOutputDeviceListChangedConnection.connected())
    {
        mOutputDeviceListChangedConnection.disconnect();
    }
}

bool FSPanelPreferenceSounds::postBuild()
{
    mOutputDevicePanel = findChild<LLPanel>("output_device_settings_panel");
    mOutputDeviceComboBox = findChild<LLComboBox>("sound_output_device");

#if LL_FMODSTUDIO
    if (gAudiop && mOutputDevicePanel && mOutputDeviceComboBox)
    {
        gSavedSettings.getControl("FSOutputDeviceUUID")->getSignal()->connect(boost::bind(&FSPanelPreferenceSounds::onOutputDeviceChanged, this, _2));

        mOutputDeviceListChangedConnection = gAudiop->setOutputDeviceListChangedCallback(boost::bind(&FSPanelPreferenceSounds::onOutputDeviceListChanged, this, _1));
        onOutputDeviceListChanged(gAudiop->getDevices());

        mOutputDeviceComboBox->setCommitCallback(boost::bind(&FSPanelPreferenceSounds::onOutputDeviceSelectionChanged, this, _2));
    }
#else
    if (mOutputDevicePanel)
    {
        mOutputDevicePanel->setVisible(false);
    }
#endif

    return LLPanelPreference::postBuild();
}

void FSPanelPreferenceSounds::onOutputDeviceChanged(const LLSD& new_value)
{
    mOutputDeviceComboBox->setSelectedByValue(new_value.asUUID(), true);
}

void FSPanelPreferenceSounds::onOutputDeviceSelectionChanged(const LLSD& new_value)
{
    gSavedSettings.setString("FSOutputDeviceUUID", mOutputDeviceComboBox->getSelectedValue().asString());
}

void FSPanelPreferenceSounds::onOutputDeviceListChanged(LLAudioEngine::output_device_map_t output_devices)
{
    LLUUID selected_device(gSavedSettings.getString("FSOutputDeviceUUID"));
    mOutputDeviceComboBox->removeall();

    if (output_devices.empty())
    {
        LL_INFOS() << "No output devices available" << LL_ENDL;
        mOutputDeviceComboBox->add(mOutputDevicePanel->getString("output_no_device"), LLUUID::null);

        if (selected_device != LLUUID::null)
        {
            LL_INFOS() << "Non-default device selected - adding unavailable for " << selected_device << LL_ENDL;
            mOutputDeviceComboBox->add(mOutputDevicePanel->getString("output_device_unavailable"), selected_device);
        }
    }
    else
    {
        bool selected_device_found = false;

        mOutputDeviceComboBox->add(mOutputDevicePanel->getString("output_default_text"), LLUUID::null);
        selected_device_found = selected_device == LLUUID::null;

        for (auto device : output_devices)
        {
            mOutputDeviceComboBox->add(device.second.empty() ? mOutputDevicePanel->getString("output_name_no_device") : device.second, device.first);

            if (!selected_device_found && device.first == selected_device)
            {
                LL_INFOS() << "Found selected device \"" << device.second << "\" (" << device.first << ")" << LL_ENDL;
                selected_device_found = true;
            }
        }

        if (!selected_device_found)
        {
            LL_INFOS() << "Selected device " << selected_device << " NOT found - adding unavailable" << LL_ENDL;
            mOutputDeviceComboBox->add(mOutputDevicePanel->getString("output_device_unavailable"), selected_device);
        }
    }

    mOutputDeviceComboBox->setSelectedByValue(selected_device, true);
}
// </FS:Ansariel>
