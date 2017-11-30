
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
#include "llviewerwindow.h"
#include "llviewermessage.h"
#include "llviewershadermgr.h"
#include "llviewerthrottle.h"
#include "llvoavatarself.h"
#include "llvotree.h"
#include "llvosky.h"
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
#include "llvoavatar.h"
#include "llvovolume.h"
#include "llwindow.h"
#include "llworld.h"
#include "pipeline.h"
#include "lluictrlfactory.h"
#include "llviewermedia.h"
#include "llpluginclassmedia.h"
#include "llteleporthistorystorage.h"
#include "llproxy.h"
#include "llweb.h"
// [RLVa:KB] - Checked: 2010-03-18 (RLVa-1.2.0a)
#include "rlvactions.h"
#include "rlvhandler.h"
// [/RLVa:KB]

#include "lllogininstance.h"        // to check if logged in yet
#include "llsdserialize.h"
#include "llpresetsmanager.h"
#include "llviewercontrol.h"
#include "llpresetsmanager.h"
#include "llfeaturemanager.h"
#include "llviewertexturelist.h"

// Firestorm Includes
#include "exogroupmutelist.h"
#include "fsavatarrenderpersistence.h"
#include "fsdroptarget.h"
#include "fsfloaterimcontainer.h"
#include "fssearchableui.h"
#include "growlmanager.h"
#include "lfsimfeaturehandler.h"
#include "llavatarname.h"	// <FS:CR> Deeper name cache stuffs
#include "lleventtimer.h"
#include "lldiriterator.h"	// <Kadah> for populating the fonts combo
#include "llline.h"
#include "llpanelblockedlist.h"
#include "llpanelmaininventory.h"
#include "llscrolllistctrl.h"
#include "llspellcheck.h"
#include "llsdserialize.h" // KB: SkinsSelector
#include "lltoolbarview.h"
#include "llviewernetwork.h" // <FS:AW  opensim search support>
#include "llwaterparammanager.h"
#include "llwldaycycle.h"
#include "llwlparammanager.h"
#include "NACLantispam.h"
#include "../llcrashlogger/llcrashlogger.h"
#if LL_WINDOWS
#include <VersionHelpers.h>
#endif

//<FS:HG> FIRE-6340, FIRE-6567 - Setting Bandwidth issues
//const F32 BANDWIDTH_UPDATER_TIMEOUT = 0.5f;
char const* const VISIBILITY_DEFAULT = "default";
char const* const VISIBILITY_HIDDEN = "hidden";

//control value for middle mouse as talk2push button
const static std::string MIDDLE_MOUSE_CV = "MiddleMouse";

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

class LLVoiceSetKeyDialog : public LLModalDialog
{
public:
	LLVoiceSetKeyDialog(const LLSD& key);
	~LLVoiceSetKeyDialog();
	
	/*virtual*/ BOOL postBuild();
	
	void setParent(LLFloaterPreference* parent) { mParent = parent; }
	
	BOOL handleKeyHere(KEY key, MASK mask);
	static void onCancel(void* user_data);
		
private:
	LLFloaterPreference* mParent;
};

LLVoiceSetKeyDialog::LLVoiceSetKeyDialog(const LLSD& key)
  : LLModalDialog(key),
	mParent(NULL)
{
}

//virtual
BOOL LLVoiceSetKeyDialog::postBuild()
{
	childSetAction("Cancel", onCancel, this);
	getChild<LLUICtrl>("Cancel")->setFocus(TRUE);
	
	gFocusMgr.setKeystrokesOnly(TRUE);
	
	return TRUE;
}

LLVoiceSetKeyDialog::~LLVoiceSetKeyDialog()
{
}

BOOL LLVoiceSetKeyDialog::handleKeyHere(KEY key, MASK mask)
{
	BOOL result = TRUE;
	
	if (key == 'Q' && mask == MASK_CONTROL)
	{
		result = FALSE;
	}
	else if (mParent)
	{
		mParent->setKey(key);
	}
	closeFloater();
	return result;
}

//static
void LLVoiceSetKeyDialog::onCancel(void* user_data)
{
	LLVoiceSetKeyDialog* self = (LLVoiceSetKeyDialog*)user_data;
	self->closeFloater();
}


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
		gSavedSettings.setString("FSPurgeInventoryCacheOnStartup", gAgentID.asString());
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
		gSavedSettings.setBOOL("FSStartupClearBrowserCache", TRUE);
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
		gSavedSettings.setBOOL("PurgeCacheOnNextStartup", TRUE);
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
		LLViewerMedia::clearAllCaches();
		LLViewerMedia::clearAllCookies();
		
		// clean nav bar history
		LLNavigationBar::getInstance()->clearHistoryCache();
		
		// flag client texture cache for clearing next time the client runs
		// <FS:AO> Don't clear main texture cache on browser cache clear - it's too expensive to be done except explicitly
		//gSavedSettings.setBOOL("PurgeCacheOnNextStartup", TRUE);
		//LLNotificationsUtil::add("CacheWillClear");

		LLSearchHistory::getInstance()->clearHistory();
		LLSearchHistory::getInstance()->save();
		// <FS:Zi> Make navigation bar part of the UI
		// LLSearchComboBox* search_ctrl = LLNavigationBar::getInstance()->getChild<LLSearchComboBox>("search_combo_box");
		// search_ctrl->clearHistory();
		LLNavigationBar::instance().clearHistory();
		// </FS:Zi>

		LLTeleportHistoryStorage::getInstance()->purgeItems();
		LLTeleportHistoryStorage::getInstance()->save();
	}
	
	return false;
}

void handleNameTagOptionChanged(const LLSD& newvalue)
{
	LLAvatarNameCache::setUseUsernames(gSavedSettings.getBOOL("NameTagShowUsernames"));
	LLVOAvatar::invalidateNameTags();
}

void handleDisplayNamesOptionChanged(const LLSD& newvalue)
{
	LLAvatarNameCache::setUseDisplayNames(newvalue.asBoolean());
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

/*bool callback_skip_dialogs(const LLSD& notification, const LLSD& response, LLFloaterPreference* floater)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	if (0 == option && floater )
	{
		if ( floater )
		{
			floater->setAllIgnored();
		//	LLFirstUse::disableFirstUse();
			floater->buildPopupLists();
		}
	}
	return false;
}

bool callback_reset_dialogs(const LLSD& notification, const LLSD& response, LLFloaterPreference* floater)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	if ( 0 == option && floater )
	{
		if ( floater )
		{
			floater->resetAllIgnored();
			//LLFirstUse::resetFirstUse();
			floater->buildPopupLists();
		}
	}
	return false;
}
*/

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
// static
std::string LLFloaterPreference::sSkin = "";
//////////////////////////////////////////////
// LLFloaterPreference

LLFloaterPreference::LLFloaterPreference(const LLSD& key)
	: LLFloater(key),
	mGotPersonalInfo(false),
	mOriginalIMViaEmail(false),
	mLanguageChanged(false),
	mAvatarDataInitialized(false),
	mClickActionDirty(false),
	mSearchData(NULL) // <FS:ND> Hook up and init for filtering
{
	LLConversationLog::instance().addObserver(this);

	//Build Floater is now Called from 	LLFloaterReg::add("preferences", "floater_preferences.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLFloaterPreference>);
	
	static bool registered_dialog = false;
	if (!registered_dialog)
	{
		LLFloaterReg::add("voice_set_key", "floater_select_key.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLVoiceSetKeyDialog>);
		registered_dialog = true;
	}
	
	mCommitCallbackRegistrar.add("Pref.Cancel",				boost::bind(&LLFloaterPreference::onBtnCancel, this, _2));
	mCommitCallbackRegistrar.add("Pref.OK",					boost::bind(&LLFloaterPreference::onBtnOK, this, _2));
	
	mCommitCallbackRegistrar.add("Pref.ClearCache",				boost::bind(&LLFloaterPreference::onClickClearCache, this));
	mCommitCallbackRegistrar.add("Pref.WebClearCache",			boost::bind(&LLFloaterPreference::onClickBrowserClearCache, this));
	// <FS:Ansariel> Clear inventory cache button
	mCommitCallbackRegistrar.add("Pref.InvClearCache",			boost::bind(&LLFloaterPreference::onClickInventoryClearCache, this));
	// </FS:Ansariel>
	// <FS:Ansariel> Clear web browser cache button
	mCommitCallbackRegistrar.add("Pref.WebBrowserClearCache",		boost::bind(&LLFloaterPreference::onClickWebBrowserClearCache, this));
	// </FS:Ansariel>
	mCommitCallbackRegistrar.add("Pref.SetCache",				boost::bind(&LLFloaterPreference::onClickSetCache, this));
	mCommitCallbackRegistrar.add("Pref.ResetCache",				boost::bind(&LLFloaterPreference::onClickResetCache, this));
//	mCommitCallbackRegistrar.add("Pref.ClickSkin",				boost::bind(&LLFloaterPreference::onClickSkin, this,_1, _2));
//	mCommitCallbackRegistrar.add("Pref.SelectSkin",				boost::bind(&LLFloaterPreference::onSelectSkin, this));
	mCommitCallbackRegistrar.add("Pref.VoiceSetKey",			boost::bind(&LLFloaterPreference::onClickSetKey, this));
	mCommitCallbackRegistrar.add("Pref.VoiceSetClearKey",		boost::bind(&LLFloaterPreference::onClickClearKey, this)); // <FS:Ansariel> FIRE-3803: Clear voice toggle button
	mCommitCallbackRegistrar.add("Pref.VoiceSetMiddleMouse",	boost::bind(&LLFloaterPreference::onClickSetMiddleMouse, this));
	//<FS:KC> Handled centrally now
//	mCommitCallbackRegistrar.add("Pref.SetSounds",				boost::bind(&LLFloaterPreference::onClickSetSounds, this));
	mCommitCallbackRegistrar.add("Pref.ClickEnablePopup",		boost::bind(&LLFloaterPreference::onClickEnablePopup, this));
	mCommitCallbackRegistrar.add("Pref.ClickDisablePopup",		boost::bind(&LLFloaterPreference::onClickDisablePopup, this));	
	mCommitCallbackRegistrar.add("Pref.LogPath",				boost::bind(&LLFloaterPreference::onClickLogPath, this));
	mCommitCallbackRegistrar.add("Pref.RenderExceptions",       boost::bind(&LLFloaterPreference::onClickRenderExceptions, this));
	mCommitCallbackRegistrar.add("Pref.HardwareDefaults",		boost::bind(&LLFloaterPreference::setHardwareDefaults, this));
	mCommitCallbackRegistrar.add("Pref.AvatarImpostorsEnable",	boost::bind(&LLFloaterPreference::onAvatarImpostorsEnable, this));
	mCommitCallbackRegistrar.add("Pref.UpdateIndirectMaxComplexity",	boost::bind(&LLFloaterPreference::updateMaxComplexity, this));
	mCommitCallbackRegistrar.add("Pref.VertexShaderEnable",		boost::bind(&LLFloaterPreference::onVertexShaderEnable, this));
	mCommitCallbackRegistrar.add("Pref.LocalLightsEnable",		boost::bind(&LLFloaterPreference::onLocalLightsEnable, this));
	mCommitCallbackRegistrar.add("Pref.WindowedMod",			boost::bind(&LLFloaterPreference::onCommitWindowedMode, this));
	mCommitCallbackRegistrar.add("Pref.UpdateSliderText",		boost::bind(&LLFloaterPreference::refreshUI,this));
	mCommitCallbackRegistrar.add("Pref.QualityPerformance",		boost::bind(&LLFloaterPreference::onChangeQuality, this, _2));
	mCommitCallbackRegistrar.add("Pref.applyUIColor",			boost::bind(&LLFloaterPreference::applyUIColor, this ,_1, _2));
	mCommitCallbackRegistrar.add("Pref.getUIColor",				boost::bind(&LLFloaterPreference::getUIColor, this ,_1, _2));
	mCommitCallbackRegistrar.add("Pref.MaturitySettings",		boost::bind(&LLFloaterPreference::onChangeMaturity, this));
	mCommitCallbackRegistrar.add("Pref.BlockList",				boost::bind(&LLFloaterPreference::onClickBlockList, this));
	mCommitCallbackRegistrar.add("Pref.Proxy",					boost::bind(&LLFloaterPreference::onClickProxySettings, this));
	mCommitCallbackRegistrar.add("Pref.TranslationSettings",	boost::bind(&LLFloaterPreference::onClickTranslationSettings, this));
	mCommitCallbackRegistrar.add("Pref.AutoReplace",            boost::bind(&LLFloaterPreference::onClickAutoReplace, this));
	mCommitCallbackRegistrar.add("Pref.PermsDefault",           boost::bind(&LLFloaterPreference::onClickPermsDefault, this));
	mCommitCallbackRegistrar.add("Pref.SpellChecker",           boost::bind(&LLFloaterPreference::onClickSpellChecker, this));
	mCommitCallbackRegistrar.add("Pref.Advanced",				boost::bind(&LLFloaterPreference::onClickAdvanced, this));

	// <FS:Ansariel> Improved graphics preferences
	mCommitCallbackRegistrar.add("Pref.UpdateIndirectMaxNonImpostors", boost::bind(&LLFloaterPreference::updateMaxNonImpostors, this));
	// </FS:Ansariel>

	sSkin = gSavedSettings.getString("SkinCurrent");

	mCommitCallbackRegistrar.add("Pref.ClickActionChange",		boost::bind(&LLFloaterPreference::onClickActionChange, this));

	gSavedSettings.getControl("NameTagShowUsernames")->getCommitSignal()->connect(boost::bind(&handleNameTagOptionChanged,  _2));
	gSavedSettings.getControl("NameTagShowFriends")->getCommitSignal()->connect(boost::bind(&handleNameTagOptionChanged,  _2));
	gSavedSettings.getControl("UseDisplayNames")->getCommitSignal()->connect(boost::bind(&handleDisplayNamesOptionChanged,  _2));

	gSavedSettings.getControl("AppearanceCameraMovement")->getCommitSignal()->connect(boost::bind(&handleAppearanceCameraMovementChanged,  _2));

	LLAvatarPropertiesProcessor::getInstance()->addObserver( gAgent.getID(), this );

	mCommitCallbackRegistrar.add("Pref.ClearLog",				boost::bind(&LLConversationLog::onClearLog, &LLConversationLog::instance()));
	mCommitCallbackRegistrar.add("Pref.DeleteTranscripts",      boost::bind(&LLFloaterPreference::onDeleteTranscripts, this));

	// <Firestorm Callbacks>
	mCommitCallbackRegistrar.add("NACL.AntiSpamUnblock",		boost::bind(&LLFloaterPreference::onClickClearSpamList, this));
	mCommitCallbackRegistrar.add("NACL.SetPreprocInclude",		boost::bind(&LLFloaterPreference::setPreprocInclude, this));
	//[ADD - Clear Settings : SJ]
	mCommitCallbackRegistrar.add("Pref.ClearSettings",			boost::bind(&LLFloaterPreference::onClickClearSettings, this));
	mCommitCallbackRegistrar.add("Pref.Online_Notices",			boost::bind(&LLFloaterPreference::onClickChatOnlineNotices, this));	
	// <FS:PP> FIRE-8190: Preview function for "UI Sounds" Panel
	mCommitCallbackRegistrar.add("PreviewUISound",				boost::bind(&LLFloaterPreference::onClickPreviewUISound, this, _2));
	mCommitCallbackRegistrar.add("Pref.BrowseCache",			boost::bind(&LLFloaterPreference::onClickBrowseCache, this));
	mCommitCallbackRegistrar.add("Pref.BrowseCrashLogs",		boost::bind(&LLFloaterPreference::onClickBrowseCrashLogs, this));
	mCommitCallbackRegistrar.add("Pref.BrowseSettingsDir",		boost::bind(&LLFloaterPreference::onClickBrowseSettingsDir, this));
	mCommitCallbackRegistrar.add("Pref.BrowseLogPath",			boost::bind(&LLFloaterPreference::onClickBrowseChatLogDir, this));
	mCommitCallbackRegistrar.add("Pref.Cookies",	    		boost::bind(&LLFloaterPreference::onClickCookies, this));
	mCommitCallbackRegistrar.add("Pref.Javascript",	        	boost::bind(&LLFloaterPreference::onClickJavascript, this));
	//[FIX FIRE-2765 : SJ] Making sure Reset button resets works
	mCommitCallbackRegistrar.add("Pref.ResetLogPath",			boost::bind(&LLFloaterPreference::onClickResetLogPath, this));
	// <FS:CR>
	gSavedSettings.getControl("FSColorUsername")->getCommitSignal()->connect(boost::bind(&handleNameTagOptionChanged, _2));
	gSavedSettings.getControl("FSUseLegacyClienttags")->getCommitSignal()->connect(boost::bind(&handleNameTagOptionChanged, _2));
	gSavedSettings.getControl("FSClientTagsVisibility")->getCommitSignal()->connect(boost::bind(&handleNameTagOptionChanged, _2));
	gSavedSettings.getControl("FSColorClienttags")->getCommitSignal()->connect(boost::bind(&handleNameTagOptionChanged, _2));
	// </FS:CR>

	// <FS:Ansariel> Sound cache
	mCommitCallbackRegistrar.add("Pref.BrowseSoundCache",				boost::bind(&LLFloaterPreference::onClickBrowseSoundCache, this));
	mCommitCallbackRegistrar.add("Pref.SetSoundCache",					boost::bind(&LLFloaterPreference::onClickSetSoundCache, this));
	mCommitCallbackRegistrar.add("Pref.ResetSoundCache",				boost::bind(&LLFloaterPreference::onClickResetSoundCache, this));
	// </FS:Ansariel>

	// <FS:Ansariel> FIRE-2912: Reset voice button
	mCommitCallbackRegistrar.add("Pref.ResetVoice",						boost::bind(&LLFloaterPreference::onClickResetVoice, this));

	mCommitCallbackRegistrar.add("UpdateFilter", boost::bind(&LLFloaterPreference::onUpdateFilterTerm, this, false)); // <FS:ND/> Hook up for filtering
}

void LLFloaterPreference::processProperties( void* pData, EAvatarProcessorType type )
{
	if ( APT_PROPERTIES == type )
	{
		const LLAvatarData* pAvatarData = static_cast<const LLAvatarData*>( pData );
		if (pAvatarData && (gAgent.getID() == pAvatarData->avatar_id) && (pAvatarData->avatar_id != LLUUID::null))
		{
			storeAvatarProperties( pAvatarData );
			processProfileProperties( pAvatarData );
		}
	}	
}

void LLFloaterPreference::storeAvatarProperties( const LLAvatarData* pAvatarData )
{
	if (gAgent.isInitialized() && (gAgent.getID() != LLUUID::null) && (LLStartUp::getStartupState() == STATE_STARTED))
	{
		mAvatarProperties.avatar_id		= pAvatarData->avatar_id;
		mAvatarProperties.image_id		= pAvatarData->image_id;
		mAvatarProperties.fl_image_id   = pAvatarData->fl_image_id;
		mAvatarProperties.about_text	= pAvatarData->about_text;
		mAvatarProperties.fl_about_text = pAvatarData->fl_about_text;
		mAvatarProperties.profile_url   = pAvatarData->profile_url;
		mAvatarProperties.flags		    = pAvatarData->flags;
		mAvatarProperties.allow_publish	= pAvatarData->flags & AVATAR_ALLOW_PUBLISH;

		mAvatarDataInitialized = true;
	}
}

void LLFloaterPreference::processProfileProperties(const LLAvatarData* pAvatarData )
{
	getChild<LLUICtrl>("online_searchresults")->setValue( (bool)(pAvatarData->flags & AVATAR_ALLOW_PUBLISH) );	
}

void LLFloaterPreference::saveAvatarProperties( void )
{
	const BOOL allowPublish = getChild<LLUICtrl>("online_searchresults")->getValue();

	if (allowPublish)
	{
		mAvatarProperties.flags |= AVATAR_ALLOW_PUBLISH;
	}

	//
	// NOTE: We really don't want to send the avatar properties unless we absolutely
	//       need to so we can avoid the accidental profile reset bug, so, if we're
	//       logged in, the avatar data has been initialized and we have a state change
	//       for the "allow publish" flag, then set the flag to its new value and send
	//       the properties update.
	//
	// NOTE: The only reason we can not remove this update altogether is because of the
	//       "allow publish" flag, the last remaining profile setting in the viewer
	//       that doesn't exist in the web profile.
	//
	if ((LLStartUp::getStartupState() == STATE_STARTED) && mAvatarDataInitialized && (allowPublish != mAvatarProperties.allow_publish))
	{
		mAvatarProperties.allow_publish = allowPublish;

		LLAvatarPropertiesProcessor::getInstance()->sendAvatarPropertiesUpdate( &mAvatarProperties );
	}
}

BOOL LLFloaterPreference::postBuild()
{
	// <FS:Ansariel> [FS communication UI]
	//gSavedSettings.getControl("ChatFontSize")->getSignal()->connect(boost::bind(&LLFloaterIMSessionTab::processChatHistoryStyleUpdate, false));

	//gSavedSettings.getControl("ChatFontSize")->getSignal()->connect(boost::bind(&LLViewerChat::signalChatFontChanged));
	// </FS:Ansariel> [FS communication UI]

	gSavedSettings.getControl("ChatBubbleOpacity")->getSignal()->connect(boost::bind(&LLFloaterPreference::onNameTagOpacityChange, this, _2));
	gSavedSettings.getControl("ConsoleBackgroundOpacity")->getSignal()->connect(boost::bind(&LLFloaterPreference::onConsoleOpacityChange, this, _2));	// <FS:CR> FIRE-1332 - Sepeate opacity settings for nametag and console chat

	gSavedSettings.getControl("PreferredMaturity")->getSignal()->connect(boost::bind(&LLFloaterPreference::onChangeMaturity, this));

	gSavedPerAccountSettings.getControl("ModelUploadFolder")->getSignal()->connect(boost::bind(&LLFloaterPreference::onChangeModelFolder, this));
	gSavedPerAccountSettings.getControl("TextureUploadFolder")->getSignal()->connect(boost::bind(&LLFloaterPreference::onChangeTextureFolder, this));
	gSavedPerAccountSettings.getControl("SoundUploadFolder")->getSignal()->connect(boost::bind(&LLFloaterPreference::onChangeSoundFolder, this));
	gSavedPerAccountSettings.getControl("AnimationUploadFolder")->getSignal()->connect(boost::bind(&LLFloaterPreference::onChangeAnimationFolder, this));

	LLTabContainer* tabcontainer = getChild<LLTabContainer>("pref core");
	if (!tabcontainer->selectTab(gSavedSettings.getS32("LastPrefTab")))
		tabcontainer->selectFirstTab();
	
	getChild<LLUICtrl>("cache_location")->setEnabled(FALSE); // make it read-only but selectable (STORM-227)
	// getChildView("log_path_string")->setEnabled(FALSE);// do the same for chat logs path - <FS:PP> Field removed from Privacy tab, we have it already in Network & Files tab along with few fancy buttons (03 Mar 2015)
	getChildView("log_path_string-panelsetup")->setEnabled(FALSE);// and the redundant instance -WoLf
	std::string cache_location = gDirUtilp->getExpandedFilename(LL_PATH_CACHE, "");
	setCacheLocation(cache_location);
	// <FS:Ansariel> Sound cache
	setSoundCacheLocation(gSavedSettings.getString("FSSoundCacheLocation"));
	getChild<LLUICtrl>("FSSoundCacheLocation")->setEnabled(FALSE);
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
	
	// <FS:Zi> Optional Edit Appearance Lighting
	gSavedSettings.getControl("AppearanceCameraMovement")->getCommitSignal()->connect(boost::bind(&LLFloaterPreference::onAppearanceCameraChanged, this));
	onAppearanceCameraChanged();
	// </FS:Zi> Optional Edit Appearance Lighting


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

	LLLogChat::setSaveHistorySignal(boost::bind(&LLFloaterPreference::onLogChatHistorySaved, this));

	LLSliderCtrl* fov_slider = getChild<LLSliderCtrl>("camera_fov");
	fov_slider->setMinValue(LLViewerCamera::getInstance()->getMinView());
	fov_slider->setMaxValue(LLViewerCamera::getInstance()->getMaxView());

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
#ifndef OPENSIM// <FS:AW optional opensim support/>
	// Hide the opensim tab if opensim isn't enabled
	LLTabContainer* tab_container = getChild<LLTabContainer>("pref core");
	if (tab_container)
	{
		LLPanel* opensim_panel = tab_container->getPanelByName("opensim");
		if (opensim_panel)
			tab_container->removeTabPanel(opensim_panel);
	}
// </FS:AW  opensim preferences>
#endif  // OPENSIM // <FS:AW optional opensim support/>


	// <FS:Zi> Pie menu
	gSavedSettings.getControl("OverridePieColors")->getSignal()->connect(boost::bind(&LLFloaterPreference::onPieColorsOverrideChanged, this));
	// make sure pie color controls are enabled or greyed out properly
	onPieColorsOverrideChanged();
	// </FS:Zi> Pie menu

	// <FS:Ansariel> Show email address in preferences (FIRE-1071)
	getChild<LLCheckBoxCtrl>("send_im_to_email")->setLabelArg("[EMAIL]", getString("LoginToChange"));

	// <FS:Kadah> Load the list of font settings
	populateFontSelectionCombo();
	// </FS:Kadah>

	// <FS:ND> Hook up and init for filtering
	mFilterEdit = getChild<LLSearchEditor>("search_prefs_edit");
	mFilterEdit->setKeystrokeCallback(boost::bind(&LLFloaterPreference::onUpdateFilterTerm, this, false));
	// </FS:ND>

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
	childSetEnabled("FSEnableAutomaticUIScaling", FALSE);
	childSetEnabled("FSDisableWMIProbing", FALSE);
#endif
	// </FS:Ansariel>

	// <FS:Ansariel> Disable options only available on Linux and not on other platforms
#ifndef LL_LINUX
	childSetEnabled("FSRemapLinuxShortcuts", FALSE);
#endif
	// </FS:Ansariel>

	return TRUE;
}

// <FS:Zi> Pie menu
void LLFloaterPreference::onPieColorsOverrideChanged()
{
	BOOL enable = gSavedSettings.getBOOL("OverridePieColors");

	getChild<LLColorSwatchCtrl>("pie_bg_color_override")->setEnabled(enable);
	getChild<LLColorSwatchCtrl>("pie_selected_color_override")->setEnabled(enable);
	getChild<LLSliderCtrl>("pie_menu_opacity")->setEnabled(enable);
	getChild<LLSliderCtrl>("pie_menu_fade_out")->setEnabled(enable);
}
// </FS:Zi> Pie menu

void LLFloaterPreference::updateDeleteTranscriptsButton()
{
	// <FS:ND> LLLogChat::getListOfTranscriptFiles will go through the whole chatlog dir, reach a bit of each file,
	// then append this file to the return-list if it seems to be valid.
	// All this only to see if there is at least one item.
	// There's two ways to make this faster:
	//   1. Make a new function which returns just true/false and exist with true as soon as one valid file is found.
	//   2. Always enable this button.
	// There seems to be little reason why this button should ever be disabled, so 2. it is, unless someone knows 
	// a good reason why 1. is the better way to handle this.
	
	// std::vector<std::string> list_of_transcriptions_file_names;
	// LLLogChat::getListOfTranscriptFiles(list_of_transcriptions_file_names);
	// getChild<LLButton>("delete_transcripts")->setEnabled(list_of_transcriptions_file_names.size() > 0);

	getChild<LLButton>("delete_transcripts")->setEnabled( true );

	// </FS:ND>
}

void LLFloaterPreference::onDoNotDisturbResponseChanged()
{
	// set "DoNotDisturbResponseChanged" TRUE if user edited message differs from default, FALSE otherwise
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

// <FS:Zi> Optional Edit Appearance Lighting
void LLFloaterPreference::onAppearanceCameraChanged()
{
	BOOL enable = gSavedSettings.getBOOL("AppearanceCameraMovement");
	getChild<LLCheckBoxCtrl>("EditAppearanceLighting")->setEnabled(enable);
}
// </FS:Zi> Optional Edit Appearance Lighting

LLFloaterPreference::~LLFloaterPreference()
{
	/* Dead code - "windowsize combo" is not in any of the skin files, except for the
	 * dutch translation, which hints at a removed control. Apart from that, I don't
	 * even understand what this code does O.o -Zi
	// clean up user data
	LLComboBox* ctrl_window_size = getChild<LLComboBox>("windowsize combo");
	for (S32 i = 0; i < ctrl_window_size->getItemCount(); i++)
	{
		ctrl_window_size->setCurrentByIndex(i);
	}*/

	LLConversationLog::instance().removeObserver(this);

	delete mSearchData;
}

void LLFloaterPreference::draw()
{
	// <FS:Ansariel> Performance improvement
	//BOOL has_first_selected = (getChildRef<LLScrollListCtrl>("disabled_popups").getFirstSelected()!=NULL);
	//gSavedSettings.setBOOL("FirstSelectedDisabledPopups", has_first_selected);
	//
	//has_first_selected = (getChildRef<LLScrollListCtrl>("enabled_popups").getFirstSelected()!=NULL);
	//gSavedSettings.setBOOL("FirstSelectedEnabledPopups", has_first_selected);

	static LLScrollListCtrl* disabled_popups = getChild<LLScrollListCtrl>("disabled_popups");
	static LLScrollListCtrl* enabled_popups = getChild<LLScrollListCtrl>("enabled_popups");
	BOOL has_first_selected = disabled_popups->getFirstSelected() != NULL;
	gSavedSettings.setBOOL("FirstSelectedDisabledPopups", has_first_selected);
	has_first_selected = enabled_popups->getFirstSelected() != NULL;
	gSavedSettings.setBOOL("FirstSelectedEnabledPopups", has_first_selected);
	// </FS:Ansariel>

	LLFloater::draw();
}

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
	
	LLViewerMedia::setCookiesEnabled(getChild<LLUICtrl>("cookies_enabled")->getValue());
	
	if (hasChild("web_proxy_enabled", TRUE) &&hasChild("web_proxy_editor", TRUE) && hasChild("web_proxy_port", TRUE))
	{
		bool proxy_enable = getChild<LLUICtrl>("web_proxy_enabled")->getValue();
		std::string proxy_address = getChild<LLUICtrl>("web_proxy_editor")->getValue();
		int proxy_port = getChild<LLUICtrl>("web_proxy_port")->getValue();
		LLViewerMedia::setProxyConfig(proxy_enable, proxy_address, proxy_port);
	}
	
	if (mGotPersonalInfo)
	{ 
		bool new_im_via_email = getChild<LLUICtrl>("send_im_to_email")->getValue().asBoolean();
		bool new_hide_online = getChild<LLUICtrl>("online_visibility")->getValue().asBoolean();		
	
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
			gAgent.sendAgentUpdateUserInfo(new_im_via_email,mDirectoryVisibility);
		}
	}

	saveAvatarProperties();

	if (mClickActionDirty)
	{
		updateClickActionSettings();
		mClickActionDirty = false;
	}

	// <FS:Ansariel> Fix resetting graphics preset on cancel; Save preset here because cancel() gets called in either way!
	saveGraphicsPreset(gSavedSettings.getString("PresetGraphicActive"));
}

void LLFloaterPreference::cancel()
{
	LLTabContainer* tabcontainer = getChild<LLTabContainer>("pref core");
	// Call cancel() on all panels that derive from LLPanelPreference
	for (child_list_t::const_iterator iter = tabcontainer->getChildList()->begin();
		iter != tabcontainer->getChildList()->end(); ++iter)
	{
		LLView* view = *iter;
		LLPanelPreference* panel = dynamic_cast<LLPanelPreference*>(view);
		if (panel)
			panel->cancel();
	}
	// hide joystick pref floater
	LLFloaterReg::hideInstance("pref_joystick");

	// hide translation settings floater
	LLFloaterReg::hideInstance("prefs_translation");
	
	// hide autoreplace settings floater
	LLFloaterReg::hideInstance("prefs_autoreplace");
	
	// hide spellchecker settings folder
	LLFloaterReg::hideInstance("prefs_spellchecker");

	// hide advancede floater
	LLFloaterReg::hideInstance("prefs_graphics_advanced");
	
	// reverts any changes to current skin
	//gSavedSettings.setString("SkinCurrent", sSkin);

	if (mClickActionDirty)
	{
		updateClickActionControls();
		mClickActionDirty = false;
	}

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
}

void LLFloaterPreference::onOpen(const LLSD& key)
{

	// this variable and if that follows it are used to properly handle do not disturb mode response message
	static bool initialized = FALSE;
	// if user is logged in and we haven't initialized do not disturb mode response yet, do it
	if (!initialized && LLStartUp::getStartupState() == STATE_STARTED)
	{
		// Special approach is used for do not disturb response localization, because "DoNotDisturbModeResponse" is
		// in non-localizable xml, and also because it may be changed by user and in this case it shouldn't be localized.
		// To keep track of whether do not disturb response is default or changed by user additional setting DoNotDisturbResponseChanged
		// was added into per account settings.

		// initialization should happen once,so setting variable to TRUE
		initialized = TRUE;
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
		getChildView("restore_per_account_disable_cover")->setVisible(FALSE);

		// <FS:Ansariel> Keyword settings are per-account; enable after logging in
		LLPanel* keyword_panel = getChild<LLPanel>("ChatKeywordAlerts");
		for (child_list_t::const_iterator iter = keyword_panel->getChildList()->begin();
			 iter != keyword_panel->getChildList()->end(); ++iter)
		{
			LLUICtrl* child = static_cast<LLUICtrl*>(*iter);
			LLControlVariable* enabled_control = child->getEnabledControlVariable();
			BOOL enabled = !enabled_control || enabled_control->getValue().asBoolean();
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
	LLAvatarPropertiesProcessor::getInstance()->sendAvatarPropertiesRequest( gAgent.getID() );
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
	onChangeTextureFolder();
	onChangeSoundFolder();
	onChangeAnimationFolder();

	// Load (double-)click to walk/teleport settings.
	updateClickActionControls();
	
	// <FS:PP> Load UI Sounds tabs settings.
	updateUISoundsControls();
	
	// Enabled/disabled popups, might have been changed by user actions
	// while preferences floater was closed.
	buildPopupLists();


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

	
	getChildView("plain_text_chat_history")->setEnabled(TRUE);
	getChild<LLUICtrl>("plain_text_chat_history")->setValue(gSavedSettings.getBOOL("PlainTextChatHistory"));
	
// <FS:CR> Show/hide Client Tag panel
	bool in_opensim = false;
#ifdef OPENSIM
	in_opensim = LLGridManager::getInstance()->isInOpenSim();
#endif // OPENSIM
	getChild<LLPanel>("client_tags_panel")->setVisible(in_opensim);
// </FS:CR>

	// <FS:Ansariel> Force HTTP features on SL
	getChild<LLCheckBoxCtrl>("TexturesHTTP")->setEnabled(in_opensim);

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
	LLPresetsManager::getInstance()->createMissingDefault();

	// <FS:Ansariel> Fix resetting graphics preset on cancel
	saveGraphicsPreset(gSavedSettings.getString("PresetGraphicActive"));

	// <FS:Ansariel> FIRE-19810: Make presets global since PresetGraphicActive setting is global as well
	//bool started = (LLStartUp::getStartupState() == STATE_STARTED);

	//LLButton* load_btn = findChild<LLButton>("PrefLoadButton");
	//LLButton* save_btn = findChild<LLButton>("PrefSaveButton");
	//LLButton* delete_btn = findChild<LLButton>("PrefDeleteButton");
	//LLButton* exceptions_btn = findChild<LLButton>("RenderExceptionsButton");

	//load_btn->setEnabled(started);
	//save_btn->setEnabled(started);
	//delete_btn->setEnabled(started);
	//exceptions_btn->setEnabled(started);
	// </FS:Ansariel>

	// <FS:ND> Hook up and init for filtering
	collectSearchableItems();
	if (!mFilterEdit->getText().empty())
	{
		mFilterEdit->setText(LLStringExplicit(""));
		onUpdateFilterTerm(true);

		if (!tabcontainer->selectTab(gSavedSettings.getS32("LastPrefTab")))
			tabcontainer->selectFirstTab();
	}
	// </FS:ND>
}

void LLFloaterPreference::onVertexShaderEnable()
{
	refreshEnabledGraphics();
}

void LLFloaterPreferenceGraphicsAdvanced::onVertexShaderEnable()
{
	LLFloaterPreference* instance = LLFloaterReg::findTypedInstance<LLFloaterPreference>("preferences");
	if (instance)
	{
		instance->refresh();
	}

	refreshEnabledGraphics();
}

void LLFloaterPreferenceGraphicsAdvanced::refreshEnabledGraphics()
{
	refreshEnabledState();
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
	//	saveGraphicsPreset(preset_graphic_active);
	//	saveSettings(); // save here to be able to return to the previous preset by Cancel
	//}
	// </FS:Ansariel>

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
		gSavedSettings.saveToFile(gSavedSettings.getString("ClientSettingsFile"), TRUE);
// [SL:KB] - Patch: Viewer-CrashReporting | Checked: 2011-10-02 (Catznip-2.8.0e) | Added: Catznip-2.8.0e
		// We need to save all crash settings, even if they're defaults [see LLCrashLogger::loadCrashBehaviorSetting()]
		gCrashSettings.saveToFile(gSavedSettings.getString("CrashSettingsFile"), FALSE);
// [/SL:KB]
		
		//Only save once logged in and loaded per account settings
		if(mGotPersonalInfo)
		{
			gSavedPerAccountSettings.saveToFile(gSavedSettings.getString("PerAccountSettingsFile"), TRUE);
		}
	}
	else
	{
		// Show beep, pop up dialog, etc.
		LL_INFOS() << "Can't close preferences!" << LL_ENDL;
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
	cancel();
	if (userdata.asString() == "closeadvanced")
	{
		LLFloaterReg::hideInstance("prefs_graphics_advanced");
	}
	else
	{
		closeFloater();
	}
}

// static 
// <FS:Ansariel> Show email address in preferences (FIRE-1071)
//void LLFloaterPreference::updateUserInfo(const std::string& visibility, bool im_via_email, bool is_verified_email)
void LLFloaterPreference::updateUserInfo(const std::string& visibility, bool im_via_email, bool is_verified_email, const std::string& email)
{
	LLFloaterPreference* instance = LLFloaterReg::findTypedInstance<LLFloaterPreference>("preferences");
	if (instance)
	{
		// <FS:Ansariel> Show email address in preferences (FIRE-1071)
        //instance->setPersonalInfo(visibility, im_via_email, is_verified_email);
		instance->setPersonalInfo(visibility, im_via_email, is_verified_email, email);
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
		color_swatch->set( new_color.setAlpha(newvalue.asReal()) );
	}
}

// <FS:CR> FIRE-1332 - Sepeate opacity settings for nametag and console chat
void LLFloaterPreference::onConsoleOpacityChange(const LLSD& newvalue)
{
	LLColorSwatchCtrl* color_swatch = findChild<LLColorSwatchCtrl>("console_background");
	if (color_swatch)
	{
		LLColor4 new_color = color_swatch->get();
		color_swatch->set( new_color.setAlpha(newvalue.asReal()) );
	}
}
// </FS:CR>

void LLFloaterPreference::onClickSetCache()
{
	std::string cur_name(gSavedSettings.getString("CacheLocation"));
//	std::string cur_top_folder(gDirUtilp->getBaseFileName(cur_name));
	
	std::string proposed_name(cur_name);

	LLDirPicker& picker = LLDirPicker::instance();
	if (! picker.getDir(&proposed_name ) )
	{
		return; //Canceled!
	}

	std::string dir_name = picker.getDirName();
	if (!dir_name.empty() && dir_name != cur_name)
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

	LLDirPicker& picker = LLDirPicker::instance();
	if (! picker.getDir(&proposed_name ) )
	{
		return; //Canceled!
	}

	std::string dir_name = picker.getDirName();
	if (!dir_name.empty() && dir_name != cur_name)
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

	BOOL tick()
	{
		gSavedSettings.setBOOL("EnableVoiceChat", TRUE);
		LLFloaterPreference* floater = LLFloaterReg::findTypedInstance<LLFloaterPreference>("preferences");
		if (floater)
		{
			floater->childSetEnabled("enable_voice_check", true);
			floater->childSetEnabled("enable_voice_check_volume", true);
		}
		return TRUE;
	}
};

void LLFloaterPreference::onClickResetVoice()
{
	if (gSavedSettings.getBOOL("EnableVoiceChat") && !gSavedSettings.getBOOL("CmdLineDisableVoice"))
	{
		gSavedSettings.setBOOL("EnableVoiceChat", FALSE);
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

	LLDirPicker& picker = LLDirPicker::instance();
	if (! picker.getDir(&proposed_name ) )
	{
		return; //Canceled!
	}

	std::string dir_name = picker.getDirName();
	if (!dir_name.empty() && dir_name != cur_name)
	{
		std::string new_top_folder(gDirUtilp->getBaseFileName(dir_name));	
		gSavedSettings.setString("_NACL_PreProcHDDIncludeLocation", dir_name);
	}
}

//[FIX JIRA-1971 : SJ] Show an notify when Cookies setting change
void LLFloaterPreference::onClickCookies()
{
	LLNotificationsUtil::add("DisableCookiesBreaksSearch");
}

//[FIX JIRA-1971 : SJ] Show an notify when Javascript setting change
void LLFloaterPreference::onClickJavascript()
{
	LLNotificationsUtil::add("DisableJavascriptBreaksSearch");
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
void LLFloaterPreference::buildPopupLists()
{
	LLScrollListCtrl& disabled_popups =
		getChildRef<LLScrollListCtrl>("disabled_popups");
	LLScrollListCtrl& enabled_popups =
		getChildRef<LLScrollListCtrl>("enabled_popups");
	
	disabled_popups.deleteAllItems();
	enabled_popups.deleteAllItems();
	
	for (LLNotifications::TemplateMap::const_iterator iter = LLNotifications::instance().templatesBegin();
		 iter != LLNotifications::instance().templatesEnd();
		 ++iter)
	{
		LLNotificationTemplatePtr templatep = iter->second;
		LLNotificationFormPtr formp = templatep->mForm;
		
		LLNotificationForm::EIgnoreType ignore = formp->getIgnoreType();
		if (ignore == LLNotificationForm::IGNORE_NO)
			continue;
		
		LLSD row;
		row["columns"][0]["value"] = formp->getIgnoreMessage();
		row["columns"][0]["font"] = "SANSSERIF_SMALL";
		row["columns"][0]["width"] = 400;
		
		LLScrollListItem* item = NULL;
		
		bool show_popup = !formp->getIgnored();
		if (!show_popup)
		{
// <FS:Ansariel> Don't show chosen option for ignored dialogs in the list. There is only one
//               notification that makes use of it ("ReplaceAttachment") and it would make the
//               list appear truncated.
#if 0
			if (ignore == LLNotificationForm::IGNORE_WITH_LAST_RESPONSE)
			{
				// <FS:Ansariel> Default responses are declared in "ignores" settings group, see llnotifications.cpp
				//LLSD last_response = LLUI::sSettingGroups["config"]->getLLSD("Default" + templatep->mName);
				LLSD last_response = LLUI::sSettingGroups["ignores"]->getLLSD("Default" + templatep->mName);
				// </FS:Ansariel>
				if (!last_response.isUndefined())
				{
					for (LLSD::map_const_iterator it = last_response.beginMap();
						 it != last_response.endMap();
						 ++it)
					{
						if (it->second.asBoolean())
						{
							row["columns"][1]["value"] = formp->getElement(it->first)["ignore"].asString();
							break;
						}
					}
				}
				row["columns"][1]["font"] = "SANSSERIF_SMALL";
				row["columns"][1]["width"] = 360;
			}
#endif
			item = disabled_popups.addElement(row);
		}
		else
		{
			item = enabled_popups.addElement(row);
		}
		
		if (item)
		{
			item->setUserdata((void*)&iter->first);
		}
	}

	// <FS:Ansariel> Let's sort it so we can find stuff!
	enabled_popups.sortByColumnIndex(0, TRUE);
	disabled_popups.sortByColumnIndex(0, TRUE);
	// </FS:Ansariel>
}

void LLFloaterPreference::refreshEnabledState()
{
	// <FS:Ansariel> Improved graphics preferences
	//LLCheckBoxCtrl* ctrl_wind_light = getChild<LLCheckBoxCtrl>("WindLightUseAtmosShaders");
	//LLCheckBoxCtrl* ctrl_deferred = getChild<LLCheckBoxCtrl>("UseLightShaders");

	//// if vertex shaders off, disable all shader related products
	//if (!LLFeatureManager::getInstance()->isFeatureAvailable("VertexShaderEnable") ||
	//	!LLFeatureManager::getInstance()->isFeatureAvailable("WindLightUseAtmosShaders"))
	//{
	//	ctrl_wind_light->setEnabled(FALSE);
	//	ctrl_wind_light->setValue(FALSE);
	//}
	//else
	//{
	//	ctrl_wind_light->setEnabled(gSavedSettings.getBOOL("VertexShaderEnable"));
	//}

	////Deferred/SSAO/Shadows
	//BOOL bumpshiny = gGLManager.mHasCubeMap && LLCubeMap::sUseCubeMaps && LLFeatureManager::getInstance()->isFeatureAvailable("RenderObjectBump") && gSavedSettings.getBOOL("RenderObjectBump");
	//BOOL shaders = gSavedSettings.getBOOL("WindLightUseAtmosShaders") && gSavedSettings.getBOOL("VertexShaderEnable");
	//BOOL enabled = LLFeatureManager::getInstance()->isFeatureAvailable("RenderDeferred") &&
	//					bumpshiny &&
	//					shaders && 
	//					gGLManager.mHasFramebufferObject &&
	//					gSavedSettings.getBOOL("RenderAvatarVP") &&
	//					(ctrl_wind_light->get()) ? TRUE : FALSE;

	//ctrl_deferred->setEnabled(enabled);

	F32 mem_multiplier = gSavedSettings.getF32("RenderTextureMemoryMultiple");
	
	S32Megabytes min_tex_mem = LLViewerTextureList::getMinVideoRamSetting();
	S32Megabytes max_tex_mem = LLViewerTextureList::getMaxVideoRamSetting(false, mem_multiplier);
	getChild<LLSliderCtrl>("GraphicsCardTextureMemory")->setMinValue(min_tex_mem.value());
	getChild<LLSliderCtrl>("GraphicsCardTextureMemory")->setMaxValue(max_tex_mem.value());

	if (!LLFeatureManager::getInstance()->isFeatureAvailable("RenderVBOEnable") ||
		!gGLManager.mHasVertexBufferObject)
	{
		getChildView("vbo")->setEnabled(FALSE);
		getChildView("vbo_stream")->setEnabled(FALSE);
	}
	else
#if LL_DARWIN
		getChildView("vbo_stream")->setEnabled(FALSE);  //Hardcoded disable on mac
		getChild<LLUICtrl>("vbo_stream")->setValue((LLSD::Boolean) FALSE);
#else
		getChildView("vbo_stream")->setEnabled(LLVertexBuffer::sEnableVBOs);
#endif

	if (!LLFeatureManager::getInstance()->isFeatureAvailable("RenderCompressTextures") ||
		!gGLManager.mHasVertexBufferObject)
	{
		getChildView("texture compression")->setEnabled(FALSE);
	}

	// if no windlight shaders, turn off nighttime brightness, gamma, and fog distance
	LLSpinCtrl* gamma_ctrl = getChild<LLSpinCtrl>("gamma");
	gamma_ctrl->setEnabled(!gPipeline.canUseWindLightShaders());
	getChildView("fog")->setEnabled(!gPipeline.canUseWindLightShaders());

	// anti-aliasing
	{
		LLUICtrl* fsaa_ctrl = getChild<LLUICtrl>("fsaa");
		
		// Enable or disable the control, the "Antialiasing:" label and the restart warning
		// based on code support for the feature on the current hardware.

		if (gPipeline.canUseAntiAliasing())
		{
			fsaa_ctrl->setEnabled(TRUE);
		}
		else
		{
			fsaa_ctrl->setEnabled(FALSE);
			fsaa_ctrl->setValue((LLSD::Integer) 0);
		}
	}

	LLComboBox* ctrl_reflections = getChild<LLComboBox>("Reflections");

// [RLVa:KB] - Checked: 2013-05-11 (RLVa-1.4.9)
	if (rlv_handler_t::isEnabled())
	{
		getChild<LLUICtrl>("do_not_disturb_response")->setEnabled(!RlvActions::hasBehaviour(RLV_BHVR_SENDIM));
	}
// [/RLVa:KB]

	// Reflections
	BOOL reflections = gSavedSettings.getBOOL("VertexShaderEnable")
		&& gGLManager.mHasCubeMap
		&& LLCubeMap::sUseCubeMaps;
	ctrl_reflections->setEnabled(reflections);
	
	// Bump & Shiny
	LLCheckBoxCtrl* bumpshiny_ctrl = getChild<LLCheckBoxCtrl>("BumpShiny");
	bool bumpshiny = gGLManager.mHasCubeMap && LLCubeMap::sUseCubeMaps && LLFeatureManager::getInstance()->isFeatureAvailable("RenderObjectBump");
	bumpshiny_ctrl->setEnabled(bumpshiny ? TRUE : FALSE);
	
	// <FS:Ansariel> Does not exist
    //LLCheckBoxCtrl* ctrl_enhanced_skel = getChild<LLCheckBoxCtrl>("AvatarEnhancedSkeleton");
    //bool enhanced_skel_enabled = gSavedSettings.getBOOL("IncludeEnhancedSkeleton");
    //ctrl_enhanced_skel->setValue(enhanced_skel_enabled);
	// </FS:Ansariel>
    
	// Avatar Mode
	// Enable Avatar Shaders
	LLCheckBoxCtrl* ctrl_avatar_vp = getChild<LLCheckBoxCtrl>("AvatarVertexProgram");
	// Avatar Render Mode
	LLCheckBoxCtrl* ctrl_avatar_cloth = getChild<LLCheckBoxCtrl>("AvatarCloth");
	
	bool avatar_vp_enabled = LLFeatureManager::getInstance()->isFeatureAvailable("RenderAvatarVP");
	if (LLViewerShaderMgr::sInitialized)
	{
		S32 max_avatar_shader = LLViewerShaderMgr::instance()->mMaxAvatarShaderLevel;
		avatar_vp_enabled = (max_avatar_shader > 0) ? TRUE : FALSE;
	}

	ctrl_avatar_vp->setEnabled(avatar_vp_enabled);
	
	if (gSavedSettings.getBOOL("VertexShaderEnable") == FALSE || 
		gSavedSettings.getBOOL("RenderAvatarVP") == FALSE)
	{
		ctrl_avatar_cloth->setEnabled(false);
	} 
	else
	{
		ctrl_avatar_cloth->setEnabled(true);
	}
	
	// Vertex Shaders
	// Global Shader Enable
	LLCheckBoxCtrl* ctrl_shader_enable = getChild<LLCheckBoxCtrl>("BasicShaders");
	// radio set for terrain detail mode
	LLRadioGroup* terrain_detail = getChild<LLRadioGroup>("TerrainDetailRadio");   // can be linked with control var

//	ctrl_shader_enable->setEnabled(LLFeatureManager::getInstance()->isFeatureAvailable("VertexShaderEnable"));
// [RLVa:KB] - Checked: 2010-03-18 (RLVa-1.2.0a) | Modified: RLVa-0.2.0a
	// "Basic Shaders" can't be disabled - but can be enabled - under @setenv=n
	bool fCtrlShaderEnable = LLFeatureManager::getInstance()->isFeatureAvailable("VertexShaderEnable");
	ctrl_shader_enable->setEnabled(
		fCtrlShaderEnable && ((!gRlvHandler.hasBehaviour(RLV_BHVR_SETENV)) || (!gSavedSettings.getBOOL("VertexShaderEnable"))) );
// [/RLVa:KB]

	BOOL shaders = ctrl_shader_enable->get();
	if (shaders)
	{
		terrain_detail->setEnabled(FALSE);
	}
	else
	{
		terrain_detail->setEnabled(TRUE);
	}
	
	// WindLight
	LLCheckBoxCtrl* ctrl_wind_light = getChild<LLCheckBoxCtrl>("WindLightUseAtmosShaders");
	LLSliderCtrl* sky = getChild<LLSliderCtrl>("SkyMeshDetail");
	
	// *HACK just checks to see if we can use shaders... 
	// maybe some cards that use shaders, but don't support windlight
//	ctrl_wind_light->setEnabled(ctrl_shader_enable->getEnabled() && shaders);
// [RLVa:KB] - Checked: 2010-03-18 (RLVa-1.2.0a) | Modified: RLVa-0.2.0a
	// "Atmospheric Shaders" can't be disabled - but can be enabled - under @setenv=n
	bool fCtrlWindLightEnable = fCtrlShaderEnable && shaders;
	ctrl_wind_light->setEnabled(
		fCtrlWindLightEnable && ((!gRlvHandler.hasBehaviour(RLV_BHVR_SETENV)) || (!gSavedSettings.getBOOL("WindLightUseAtmosShaders"))) );
// [/RLVa:KB]

	sky->setEnabled(ctrl_wind_light->get() && shaders);

	//Deferred/SSAO/Shadows
	LLCheckBoxCtrl* ctrl_deferred = getChild<LLCheckBoxCtrl>("UseLightShaders");

	BOOL enabled = LLFeatureManager::getInstance()->isFeatureAvailable("RenderDeferred") &&
						((bumpshiny_ctrl && bumpshiny_ctrl->get()) ? TRUE : FALSE) &&
						shaders && 
						gGLManager.mHasFramebufferObject &&
						gSavedSettings.getBOOL("RenderAvatarVP") &&
						(ctrl_wind_light->get()) ? TRUE : FALSE;

	ctrl_deferred->setEnabled(enabled);

	LLCheckBoxCtrl* ctrl_ssao = getChild<LLCheckBoxCtrl>("UseSSAO");
	LLCheckBoxCtrl* ctrl_dof = getChild<LLCheckBoxCtrl>("UseDoF");
	LLComboBox* ctrl_shadow = getChild<LLComboBox>("ShadowDetail");

	// note, okay here to get from ctrl_deferred as it's twin, ctrl_deferred2 will alway match it
	enabled = enabled && LLFeatureManager::getInstance()->isFeatureAvailable("RenderDeferredSSAO") && (ctrl_deferred->get() ? TRUE : FALSE);
	
	ctrl_deferred->set(gSavedSettings.getBOOL("RenderDeferred"));

	ctrl_ssao->setEnabled(enabled);
	ctrl_dof->setEnabled(enabled);

	enabled = enabled && LLFeatureManager::getInstance()->isFeatureAvailable("RenderShadowDetail");

	ctrl_shadow->setEnabled(enabled);
	
	LLComboBox* ctrl_avatar_shadow = getChild<LLComboBox>("AvatarShadowDetail");
	ctrl_avatar_shadow->setEnabled(enabled && ctrl_shadow->getValue().asInteger() > 0);

	// now turn off any features that are unavailable
	disableUnavailableSettings();

	getChildView("block_list")->setEnabled(LLLoginInstance::getInstance()->authSuccess());

	// Cannot have floater active until caps have been received
	//getChild<LLButton>("default_creation_permissions")->setEnabled(LLStartUp::getStartupState() < STATE_STARTED ? false : true);
	getChild<LLButton>("fs_default_creation_permissions")->setEnabled(LLStartUp::getStartupState() < STATE_STARTED ? false : true);
}

void LLFloaterPreferenceGraphicsAdvanced::refreshEnabledState()
{
	LLComboBox* ctrl_reflections = getChild<LLComboBox>("Reflections");
	LLTextBox* reflections_text = getChild<LLTextBox>("ReflectionsText");

	// Reflections
	BOOL reflections = gSavedSettings.getBOOL("VertexShaderEnable") 
		&& gGLManager.mHasCubeMap
		&& LLCubeMap::sUseCubeMaps;
	ctrl_reflections->setEnabled(reflections);
	reflections_text->setEnabled(reflections);
	
	// Bump & Shiny	
	LLCheckBoxCtrl* bumpshiny_ctrl = getChild<LLCheckBoxCtrl>("BumpShiny");
	bool bumpshiny = gGLManager.mHasCubeMap && LLCubeMap::sUseCubeMaps && LLFeatureManager::getInstance()->isFeatureAvailable("RenderObjectBump");
	bumpshiny_ctrl->setEnabled(bumpshiny ? TRUE : FALSE);
    
	// Avatar Mode
	// Enable Avatar Shaders
	LLCheckBoxCtrl* ctrl_avatar_vp = getChild<LLCheckBoxCtrl>("AvatarVertexProgram");
	// Avatar Render Mode
	LLCheckBoxCtrl* ctrl_avatar_cloth = getChild<LLCheckBoxCtrl>("AvatarCloth");
	
	bool avatar_vp_enabled = LLFeatureManager::getInstance()->isFeatureAvailable("RenderAvatarVP");
	if (LLViewerShaderMgr::sInitialized)
	{
		S32 max_avatar_shader = LLViewerShaderMgr::instance()->mMaxAvatarShaderLevel;
		avatar_vp_enabled = (max_avatar_shader > 0) ? TRUE : FALSE;
	}

	ctrl_avatar_vp->setEnabled(avatar_vp_enabled);
	
	if (gSavedSettings.getBOOL("VertexShaderEnable") == FALSE || 
		gSavedSettings.getBOOL("RenderAvatarVP") == FALSE)
	{
		ctrl_avatar_cloth->setEnabled(FALSE);
	} 
	else
	{
		ctrl_avatar_cloth->setEnabled(TRUE);
	}
	
	// Vertex Shaders
	// Global Shader Enable
	LLCheckBoxCtrl* ctrl_shader_enable   = getChild<LLCheckBoxCtrl>("BasicShaders");
	LLSliderCtrl* terrain_detail = getChild<LLSliderCtrl>("TerrainDetail");   // can be linked with control var
	LLTextBox* terrain_text = getChild<LLTextBox>("TerrainDetailText");

//	ctrl_shader_enable->setEnabled(LLFeatureManager::getInstance()->isFeatureAvailable("VertexShaderEnable"));
// [RLVa:KB] - Checked: 2010-03-18 (RLVa-1.2.0a) | Modified: RLVa-0.2.0a
	// "Basic Shaders" can't be disabled - but can be enabled - under @setenv=n
	bool fCtrlShaderEnable = LLFeatureManager::getInstance()->isFeatureAvailable("VertexShaderEnable");
	ctrl_shader_enable->setEnabled(
		fCtrlShaderEnable && ((!gRlvHandler.hasBehaviour(RLV_BHVR_SETENV)) || (!gSavedSettings.getBOOL("VertexShaderEnable"))) );
// [/RLVa:KB]

	BOOL shaders = ctrl_shader_enable->get();
	if (shaders)
	{
		terrain_detail->setValue(1);
		terrain_detail->setEnabled(FALSE);
		terrain_text->setEnabled(FALSE);
	}
	else
	{
		terrain_detail->setEnabled(TRUE);
		terrain_text->setEnabled(TRUE);
	}
	
	// WindLight
	LLCheckBoxCtrl* ctrl_wind_light = getChild<LLCheckBoxCtrl>("WindLightUseAtmosShaders");
	LLSliderCtrl* sky = getChild<LLSliderCtrl>("SkyMeshDetail");
	LLTextBox* sky_text = getChild<LLTextBox>("SkyMeshDetailText");

	// *HACK just checks to see if we can use shaders... 
	// maybe some cards that use shaders, but don't support windlight
//	ctrl_wind_light->setEnabled(ctrl_shader_enable->getEnabled() && shaders);
// [RLVa:KB] - Checked: 2010-03-18 (RLVa-1.2.0a) | Modified: RLVa-0.2.0a
	// "Atmospheric Shaders" can't be disabled - but can be enabled - under @setenv=n
	bool fCtrlWindLightEnable = fCtrlShaderEnable && shaders;
	ctrl_wind_light->setEnabled(
		fCtrlWindLightEnable && ((!gRlvHandler.hasBehaviour(RLV_BHVR_SETENV)) || (!gSavedSettings.getBOOL("WindLightUseAtmosShaders"))) );
// [/RLVa:KB]

	sky->setEnabled(ctrl_wind_light->get() && shaders);
	sky_text->setEnabled(ctrl_wind_light->get() && shaders);

	//Deferred/SSAO/Shadows
	LLCheckBoxCtrl* ctrl_deferred = getChild<LLCheckBoxCtrl>("UseLightShaders");
	
	BOOL enabled = LLFeatureManager::getInstance()->isFeatureAvailable("RenderDeferred") &&
						((bumpshiny_ctrl && bumpshiny_ctrl->get()) ? TRUE : FALSE) &&
						shaders && 
						gGLManager.mHasFramebufferObject &&
						gSavedSettings.getBOOL("RenderAvatarVP") &&
						(ctrl_wind_light->get()) ? TRUE : FALSE;

	ctrl_deferred->setEnabled(enabled);

	LLCheckBoxCtrl* ctrl_ssao = getChild<LLCheckBoxCtrl>("UseSSAO");
	LLCheckBoxCtrl* ctrl_dof = getChild<LLCheckBoxCtrl>("UseDoF");
	LLComboBox* ctrl_shadow = getChild<LLComboBox>("ShadowDetail");
	LLTextBox* shadow_text = getChild<LLTextBox>("RenderShadowDetailText");

	// note, okay here to get from ctrl_deferred as it's twin, ctrl_deferred2 will alway match it
	enabled = enabled && LLFeatureManager::getInstance()->isFeatureAvailable("RenderDeferredSSAO") && (ctrl_deferred->get() ? TRUE : FALSE);
	
	ctrl_deferred->set(gSavedSettings.getBOOL("RenderDeferred"));

	ctrl_ssao->setEnabled(enabled);
	ctrl_dof->setEnabled(enabled);

	enabled = enabled && LLFeatureManager::getInstance()->isFeatureAvailable("RenderShadowDetail");

	ctrl_shadow->setEnabled(enabled);
	shadow_text->setEnabled(enabled);

	// Hardware settings
	F32 mem_multiplier = gSavedSettings.getF32("RenderTextureMemoryMultiple");
	S32Megabytes min_tex_mem = LLViewerTextureList::getMinVideoRamSetting();
	S32Megabytes max_tex_mem = LLViewerTextureList::getMaxVideoRamSetting(false, mem_multiplier);
	getChild<LLSliderCtrl>("GraphicsCardTextureMemory")->setMinValue(min_tex_mem.value());
	getChild<LLSliderCtrl>("GraphicsCardTextureMemory")->setMaxValue(max_tex_mem.value());

	if (!LLFeatureManager::getInstance()->isFeatureAvailable("RenderVBOEnable") ||
		!gGLManager.mHasVertexBufferObject)
	{
		getChildView("vbo")->setEnabled(FALSE);
	}

	if (!LLFeatureManager::getInstance()->isFeatureAvailable("RenderCompressTextures") ||
		!gGLManager.mHasVertexBufferObject)
	{
		getChildView("texture compression")->setEnabled(FALSE);
	}

	// if no windlight shaders, turn off nighttime brightness, gamma, and fog distance
	LLUICtrl* gamma_ctrl = getChild<LLUICtrl>("gamma");
	gamma_ctrl->setEnabled(!gPipeline.canUseWindLightShaders());
	getChildView("(brightness, lower is brighter)")->setEnabled(!gPipeline.canUseWindLightShaders());
	getChildView("fog")->setEnabled(!gPipeline.canUseWindLightShaders());
	getChildView("antialiasing restart")->setVisible(!LLFeatureManager::getInstance()->isFeatureAvailable("RenderDeferred"));

	// now turn off any features that are unavailable
	disableUnavailableSettings();

	getChildView("block_list")->setEnabled(LLLoginInstance::getInstance()->authSuccess());
}


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
	U32 indirect_max_non_impostors = (0 == max_non_impostors) ? LLVOAvatar::IMPOSTORS_OFF : max_non_impostors;
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
	LLComboBox* ctrl_reflections   = getChild<LLComboBox>("Reflections");
	LLCheckBoxCtrl* ctrl_avatar_vp     = getChild<LLCheckBoxCtrl>("AvatarVertexProgram");
	LLCheckBoxCtrl* ctrl_avatar_cloth  = getChild<LLCheckBoxCtrl>("AvatarCloth");
	LLCheckBoxCtrl* ctrl_shader_enable = getChild<LLCheckBoxCtrl>("BasicShaders");
	LLCheckBoxCtrl* ctrl_wind_light    = getChild<LLCheckBoxCtrl>("WindLightUseAtmosShaders");
	LLCheckBoxCtrl* ctrl_deferred = getChild<LLCheckBoxCtrl>("UseLightShaders");
	LLComboBox* ctrl_shadows = getChild<LLComboBox>("ShadowDetail");
	LLCheckBoxCtrl* ctrl_ssao = getChild<LLCheckBoxCtrl>("UseSSAO");
	LLCheckBoxCtrl* ctrl_dof = getChild<LLCheckBoxCtrl>("UseDoF");
	LLComboBox* ctrl_avatar_shadow = getChild<LLComboBox>("AvatarShadowDetail");
	LLSliderCtrl* sky = getChild<LLSliderCtrl>("SkyMeshDetail");

	// if vertex shaders off, disable all shader related products
	if (!LLFeatureManager::getInstance()->isFeatureAvailable("VertexShaderEnable"))
	{
		ctrl_shader_enable->setEnabled(FALSE);
		ctrl_shader_enable->setValue(FALSE);
		
		ctrl_wind_light->setEnabled(FALSE);
		ctrl_wind_light->setValue(FALSE);

		sky->setEnabled(FALSE);

		ctrl_reflections->setEnabled(FALSE);
		ctrl_reflections->setValue(0);
		
		ctrl_avatar_vp->setEnabled(FALSE);
		ctrl_avatar_vp->setValue(FALSE);
		
		ctrl_avatar_cloth->setEnabled(FALSE);
		ctrl_avatar_cloth->setValue(FALSE);

		ctrl_shadows->setEnabled(FALSE);
		ctrl_shadows->setValue(0);
		
		ctrl_avatar_shadow->setEnabled(FALSE);
		ctrl_avatar_shadow->setValue(0);

		ctrl_ssao->setEnabled(FALSE);
		ctrl_ssao->setValue(FALSE);

		ctrl_dof->setEnabled(FALSE);
		ctrl_dof->setValue(FALSE);

		ctrl_deferred->setEnabled(FALSE);
		ctrl_deferred->setValue(FALSE);
	}
	
	// disabled windlight
	if (!LLFeatureManager::getInstance()->isFeatureAvailable("WindLightUseAtmosShaders"))
	{
		ctrl_wind_light->setEnabled(FALSE);
		ctrl_wind_light->setValue(FALSE);

		sky->setEnabled(FALSE);

		//deferred needs windlight, disable deferred
		ctrl_shadows->setEnabled(FALSE);
		ctrl_shadows->setValue(0);

		ctrl_avatar_shadow->setEnabled(FALSE);
		ctrl_avatar_shadow->setValue(0);
		
		ctrl_ssao->setEnabled(FALSE);
		ctrl_ssao->setValue(FALSE);

		ctrl_dof->setEnabled(FALSE);
		ctrl_dof->setValue(FALSE);

		ctrl_deferred->setEnabled(FALSE);
		ctrl_deferred->setValue(FALSE);
	}

	// disabled deferred
	if (!LLFeatureManager::getInstance()->isFeatureAvailable("RenderDeferred") ||
		!gGLManager.mHasFramebufferObject)
	{
		ctrl_shadows->setEnabled(FALSE);
		ctrl_shadows->setValue(0);
		
		ctrl_avatar_shadow->setEnabled(FALSE);
		ctrl_avatar_shadow->setValue(0);

		ctrl_ssao->setEnabled(FALSE);
		ctrl_ssao->setValue(FALSE);

		ctrl_dof->setEnabled(FALSE);
		ctrl_dof->setValue(FALSE);

		ctrl_deferred->setEnabled(FALSE);
		ctrl_deferred->setValue(FALSE);
	}
	
	// disabled deferred SSAO
	if (!LLFeatureManager::getInstance()->isFeatureAvailable("RenderDeferredSSAO"))
	{
		ctrl_ssao->setEnabled(FALSE);
		ctrl_ssao->setValue(FALSE);
	}
	
	// disabled deferred shadows
	if (!LLFeatureManager::getInstance()->isFeatureAvailable("RenderShadowDetail"))
	{
		ctrl_shadows->setEnabled(FALSE);
		ctrl_shadows->setValue(0);

		ctrl_avatar_shadow->setEnabled(FALSE);
		ctrl_avatar_shadow->setValue(0);
	}

	// disabled reflections
	if (!LLFeatureManager::getInstance()->isFeatureAvailable("RenderReflectionDetail"))
	{
		ctrl_reflections->setEnabled(FALSE);
		ctrl_reflections->setValue(FALSE);
	}
	
	// disabled av
	if (!LLFeatureManager::getInstance()->isFeatureAvailable("RenderAvatarVP"))
	{
		ctrl_avatar_vp->setEnabled(FALSE);
		ctrl_avatar_vp->setValue(FALSE);
		
		ctrl_avatar_cloth->setEnabled(FALSE);
		ctrl_avatar_cloth->setValue(FALSE);

		//deferred needs AvatarVP, disable deferred
		ctrl_shadows->setEnabled(FALSE);
		ctrl_shadows->setValue(0);
		
		ctrl_avatar_shadow->setEnabled(FALSE);
		ctrl_avatar_shadow->setValue(0);

		ctrl_ssao->setEnabled(FALSE);
		ctrl_ssao->setValue(FALSE);

		ctrl_dof->setEnabled(FALSE);
		ctrl_dof->setValue(FALSE);

		ctrl_deferred->setEnabled(FALSE);
		ctrl_deferred->setValue(FALSE);
	}

	// disabled cloth
	if (!LLFeatureManager::getInstance()->isFeatureAvailable("RenderAvatarCloth"))
	{
		ctrl_avatar_cloth->setEnabled(FALSE);
		ctrl_avatar_cloth->setValue(FALSE);
	}
}

void LLFloaterPreferenceGraphicsAdvanced::disableUnavailableSettings()
{	
	LLComboBox* ctrl_reflections   = getChild<LLComboBox>("Reflections");
	LLTextBox* reflections_text = getChild<LLTextBox>("ReflectionsText");
	LLCheckBoxCtrl* ctrl_avatar_vp     = getChild<LLCheckBoxCtrl>("AvatarVertexProgram");
	LLCheckBoxCtrl* ctrl_avatar_cloth  = getChild<LLCheckBoxCtrl>("AvatarCloth");
	LLCheckBoxCtrl* ctrl_shader_enable = getChild<LLCheckBoxCtrl>("BasicShaders");
	LLCheckBoxCtrl* ctrl_wind_light    = getChild<LLCheckBoxCtrl>("WindLightUseAtmosShaders");
	LLCheckBoxCtrl* ctrl_deferred = getChild<LLCheckBoxCtrl>("UseLightShaders");
	LLComboBox* ctrl_shadows = getChild<LLComboBox>("ShadowDetail");
	LLTextBox* shadows_text = getChild<LLTextBox>("RenderShadowDetailText");
	LLCheckBoxCtrl* ctrl_ssao = getChild<LLCheckBoxCtrl>("UseSSAO");
	LLCheckBoxCtrl* ctrl_dof = getChild<LLCheckBoxCtrl>("UseDoF");
	LLSliderCtrl* sky = getChild<LLSliderCtrl>("SkyMeshDetail");
	LLTextBox* sky_text = getChild<LLTextBox>("SkyMeshDetailText");

	// if vertex shaders off, disable all shader related products
	if (!LLFeatureManager::getInstance()->isFeatureAvailable("VertexShaderEnable"))
	{
		ctrl_shader_enable->setEnabled(FALSE);
		ctrl_shader_enable->setValue(FALSE);
		
		ctrl_wind_light->setEnabled(FALSE);
		ctrl_wind_light->setValue(FALSE);

		sky->setEnabled(FALSE);
		sky_text->setEnabled(FALSE);

		ctrl_reflections->setEnabled(FALSE);
		ctrl_reflections->setValue(0);
		reflections_text->setEnabled(FALSE);
		
		ctrl_avatar_vp->setEnabled(FALSE);
		ctrl_avatar_vp->setValue(FALSE);
		
		ctrl_avatar_cloth->setEnabled(FALSE);
		ctrl_avatar_cloth->setValue(FALSE);

		ctrl_shadows->setEnabled(FALSE);
		ctrl_shadows->setValue(0);
		shadows_text->setEnabled(FALSE);
		
		ctrl_ssao->setEnabled(FALSE);
		ctrl_ssao->setValue(FALSE);

		ctrl_dof->setEnabled(FALSE);
		ctrl_dof->setValue(FALSE);

		ctrl_deferred->setEnabled(FALSE);
		ctrl_deferred->setValue(FALSE);
	}
	
	// disabled windlight
	if (!LLFeatureManager::getInstance()->isFeatureAvailable("WindLightUseAtmosShaders"))
	{
		ctrl_wind_light->setEnabled(FALSE);
		ctrl_wind_light->setValue(FALSE);

		sky->setEnabled(FALSE);
		sky_text->setEnabled(FALSE);

		//deferred needs windlight, disable deferred
		ctrl_shadows->setEnabled(FALSE);
		ctrl_shadows->setValue(0);
		shadows_text->setEnabled(FALSE);
		
		ctrl_ssao->setEnabled(FALSE);
		ctrl_ssao->setValue(FALSE);

		ctrl_dof->setEnabled(FALSE);
		ctrl_dof->setValue(FALSE);

		ctrl_deferred->setEnabled(FALSE);
		ctrl_deferred->setValue(FALSE);
	}

	// disabled deferred
	if (!LLFeatureManager::getInstance()->isFeatureAvailable("RenderDeferred") ||
		!gGLManager.mHasFramebufferObject)
	{
		ctrl_shadows->setEnabled(FALSE);
		ctrl_shadows->setValue(0);
		shadows_text->setEnabled(FALSE);
		
		ctrl_ssao->setEnabled(FALSE);
		ctrl_ssao->setValue(FALSE);

		ctrl_dof->setEnabled(FALSE);
		ctrl_dof->setValue(FALSE);

		ctrl_deferred->setEnabled(FALSE);
		ctrl_deferred->setValue(FALSE);
	}
	
	// disabled deferred SSAO
	if (!LLFeatureManager::getInstance()->isFeatureAvailable("RenderDeferredSSAO"))
	{
		ctrl_ssao->setEnabled(FALSE);
		ctrl_ssao->setValue(FALSE);
	}
	
	// disabled deferred shadows
	if (!LLFeatureManager::getInstance()->isFeatureAvailable("RenderShadowDetail"))
	{
		ctrl_shadows->setEnabled(FALSE);
		ctrl_shadows->setValue(0);
		shadows_text->setEnabled(FALSE);
	}

	// disabled reflections
	if (!LLFeatureManager::getInstance()->isFeatureAvailable("RenderReflectionDetail"))
	{
		ctrl_reflections->setEnabled(FALSE);
		ctrl_reflections->setValue(FALSE);
		reflections_text->setEnabled(FALSE);
	}
	
	// disabled av
	if (!LLFeatureManager::getInstance()->isFeatureAvailable("RenderAvatarVP"))
	{
		ctrl_avatar_vp->setEnabled(FALSE);
		ctrl_avatar_vp->setValue(FALSE);
		
		ctrl_avatar_cloth->setEnabled(FALSE);
		ctrl_avatar_cloth->setValue(FALSE);

		//deferred needs AvatarVP, disable deferred
		ctrl_shadows->setEnabled(FALSE);
		ctrl_shadows->setValue(0);
		shadows_text->setEnabled(FALSE);
		
		ctrl_ssao->setEnabled(FALSE);
		ctrl_ssao->setValue(FALSE);

		ctrl_dof->setEnabled(FALSE);
		ctrl_dof->setValue(FALSE);

		ctrl_deferred->setEnabled(FALSE);
		ctrl_deferred->setValue(FALSE);
	}

	// disabled cloth
	if (!LLFeatureManager::getInstance()->isFeatureAvailable("RenderAvatarCloth"))
	{
		ctrl_avatar_cloth->setEnabled(FALSE);
		ctrl_avatar_cloth->setValue(FALSE);
	}
}

void LLFloaterPreference::refresh()
{
	LLPanel::refresh();

	// <FS:Ansariel> Improved graphics preferences
	getChild<LLUICtrl>("fsaa")->setValue((LLSD::Integer)  gSavedSettings.getU32("RenderFSAASamples"));
	updateSliderText(getChild<LLSliderCtrl>("RenderPostProcess",	true), getChild<LLTextBox>("PostProcessText",			true));
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
}

void LLFloaterPreferenceGraphicsAdvanced::refresh()
{
	getChild<LLUICtrl>("fsaa")->setValue((LLSD::Integer)  gSavedSettings.getU32("RenderFSAASamples"));

	// sliders and their text boxes
	//	mPostProcess = gSavedSettings.getS32("RenderGlowResolutionPow");
	// slider text boxes
	updateSliderText(getChild<LLSliderCtrl>("ObjectMeshDetail",		true), getChild<LLTextBox>("ObjectMeshDetailText",		true));
	updateSliderText(getChild<LLSliderCtrl>("FlexibleMeshDetail",	true), getChild<LLTextBox>("FlexibleMeshDetailText",	true));
	updateSliderText(getChild<LLSliderCtrl>("TreeMeshDetail",		true), getChild<LLTextBox>("TreeMeshDetailText",		true));
	updateSliderText(getChild<LLSliderCtrl>("AvatarMeshDetail",		true), getChild<LLTextBox>("AvatarMeshDetailText",		true));
	updateSliderText(getChild<LLSliderCtrl>("AvatarPhysicsDetail",	true), getChild<LLTextBox>("AvatarPhysicsDetailText",		true));
	updateSliderText(getChild<LLSliderCtrl>("TerrainMeshDetail",	true), getChild<LLTextBox>("TerrainMeshDetailText",		true));
	updateSliderText(getChild<LLSliderCtrl>("RenderPostProcess",	true), getChild<LLTextBox>("PostProcessText",			true));
	updateSliderText(getChild<LLSliderCtrl>("SkyMeshDetail",		true), getChild<LLTextBox>("SkyMeshDetailText",			true));
	updateSliderText(getChild<LLSliderCtrl>("TerrainDetail",		true), getChild<LLTextBox>("TerrainDetailText",			true));	
    LLAvatarComplexityControls::setIndirectControls();
	setMaxNonImpostorsText(
        gSavedSettings.getU32("RenderAvatarMaxNonImpostors"),
        getChild<LLTextBox>("IndirectMaxNonImpostorsText", true));
    LLAvatarComplexityControls::setText(
        gSavedSettings.getU32("RenderAvatarMaxComplexity"),
        getChild<LLTextBox>("IndirectMaxComplexityText", true));
	refreshEnabledState();
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

void LLFloaterPreference::onClickSetKey()
{
	LLVoiceSetKeyDialog* dialog = LLFloaterReg::showTypedInstance<LLVoiceSetKeyDialog>("voice_set_key", LLSD(), TRUE);
	if (dialog)
	{
		dialog->setParent(this);
	}
}

// <FS:Ansariel> FIRE-3803: Clear voice toggle button
void LLFloaterPreference::onClickClearKey()
{
	gSavedSettings.setString("PushToTalkButton", "");
}
// </FS:Ansariel>

void LLFloaterPreference::setKey(KEY key)
{
	getChild<LLUICtrl>("modifier_combo")->setValue(LLKeyboard::stringFromKey(key));
	// update the control right away since we no longer wait for apply
	getChild<LLUICtrl>("modifier_combo")->onCommit();
}

void LLFloaterPreference::onClickSetMiddleMouse()
{
	LLUICtrl* p2t_line_editor = getChild<LLUICtrl>("modifier_combo");

	// update the control right away since we no longer wait for apply
	p2t_line_editor->setControlValue(MIDDLE_MOUSE_CV);

	//push2talk button "middle mouse" control value is in English, need to localize it for presentation
	LLPanel* audioPanel=getChild<LLPanel>("audio");
	p2t_line_editor->setValue(audioPanel->getString("middle_mouse"));
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

/*
void LLFloaterPreference::onClickSkipDialogs()
{
	LLNotificationsUtil::add("SkipShowNextTimeDialogs", LLSD(), LLSD(), boost::bind(&callback_skip_dialogs, _1, _2, this));
}

void LLFloaterPreference::onClickResetDialogs()
{
	LLNotificationsUtil::add("ResetShowNextTimeDialogs", LLSD(), LLSD(), boost::bind(&callback_reset_dialogs, _1, _2, this));
}
 */

void LLFloaterPreference::onClickEnablePopup()
{	
	LLScrollListCtrl& disabled_popups = getChildRef<LLScrollListCtrl>("disabled_popups");
	
	std::vector<LLScrollListItem*> items = disabled_popups.getAllSelected();
	std::vector<LLScrollListItem*>::iterator itor;
	for (itor = items.begin(); itor != items.end(); ++itor)
	{
		LLNotificationTemplatePtr templatep = LLNotifications::instance().getTemplate(*(std::string*)((*itor)->getUserdata()));
		//gSavedSettings.setWarning(templatep->mName, TRUE);
		std::string notification_name = templatep->mName;
		LLUI::sSettingGroups["ignores"]->setBOOL(notification_name, TRUE);
	}
	
	buildPopupLists();
}

void LLFloaterPreference::onClickDisablePopup()
{	
	LLScrollListCtrl& enabled_popups = getChildRef<LLScrollListCtrl>("enabled_popups");
	
	std::vector<LLScrollListItem*> items = enabled_popups.getAllSelected();
	std::vector<LLScrollListItem*>::iterator itor;
	for (itor = items.begin(); itor != items.end(); ++itor)
	{
		LLNotificationTemplatePtr templatep = LLNotifications::instance().getTemplate(*(std::string*)((*itor)->getUserdata()));
		templatep->mForm->setIgnored(true);
	}
	
	buildPopupLists();
}

void LLFloaterPreference::resetAllIgnored()
{
	for (LLNotifications::TemplateMap::const_iterator iter = LLNotifications::instance().templatesBegin();
		 iter != LLNotifications::instance().templatesEnd();
		 ++iter)
	{
		if (iter->second->mForm->getIgnoreType() != LLNotificationForm::IGNORE_NO)
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
		if (iter->second->mForm->getIgnoreType() != LLNotificationForm::IGNORE_NO)
		{
			iter->second->mForm->setIgnored(true);
		}
	}
}

void LLFloaterPreference::onClickLogPath()
{
	std::string proposed_name(gSavedPerAccountSettings.getString("InstantMessageLogPath"));	 
	mPriorInstantMessageLogPath.clear();
	
	LLDirPicker& picker = LLDirPicker::instance();
	//Launches a directory picker and waits for feedback
	if (!picker.getDir(&proposed_name ) )
	{
		return; //Canceled!
	}

	//Gets the path from the directory picker
	std::string dir_name = picker.getDirName();

	//Path changed
	if(proposed_name != dir_name)
	{
	gSavedPerAccountSettings.setString("InstantMessageLogPath", dir_name);
		mPriorInstantMessageLogPath = proposed_name;
	
	// enable/disable 'Delete transcripts button
	updateDeleteTranscriptsButton();
}
	//[FIX FIRE-2765 : SJ] Enable Reset button when own Chatlogdirectory is set
	getChildView("reset_logpath")->setEnabled(TRUE);
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

	getChildView("reset_logpath")->setEnabled(FALSE);
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

// <FS:Ansariel> Show email address in preferences (FIRE-1071)
//void LLFloaterPreference::setPersonalInfo(const std::string& visibility, bool im_via_email, bool is_verified_email)
void LLFloaterPreference::setPersonalInfo(const std::string& visibility, bool im_via_email, bool is_verified_email, const std::string& email)
// </FS:Ansariel> Show email address in preferences (FIRE-1071)
{
	mGotPersonalInfo = true;
	mOriginalIMViaEmail = im_via_email;
	mDirectoryVisibility = visibility;
	
	if (visibility == VISIBILITY_DEFAULT)
	{
		mOriginalHideOnlineStatus = false;
		getChildView("online_visibility")->setEnabled(TRUE); 	 
	}
	else if (visibility == VISIBILITY_HIDDEN)
	{
		mOriginalHideOnlineStatus = true;
		getChildView("online_visibility")->setEnabled(TRUE); 	 
	}
	else
	{
		mOriginalHideOnlineStatus = true;
	}
	
	getChild<LLUICtrl>("online_searchresults")->setEnabled(TRUE);
	getChildView("friends_online_notify_checkbox")->setEnabled(TRUE);
	getChild<LLUICtrl>("online_visibility")->setValue(mOriginalHideOnlineStatus); 	 
	getChild<LLUICtrl>("online_visibility")->setLabelArg("[DIR_VIS]", mDirectoryVisibility);
	getChildView("send_im_to_email")->setEnabled(is_verified_email);

    std::string tooltip;
    if (!is_verified_email)
        tooltip = getString("email_unverified_tooltip");

    getChildView("send_im_to_email")->setToolTip(tooltip);

    // *TODO: Show or hide verify email text here based on is_verified_email
    getChild<LLUICtrl>("send_im_to_email")->setValue(im_via_email);
	getChildView("favorites_on_login_check")->setEnabled(TRUE);
	//getChildView("log_path_button")->setEnabled(TRUE); // <FS:Ansariel> Does not exist as of 12-09-2014
	getChildView("chat_font_size")->setEnabled(TRUE);
	//getChildView("open_log_path_button")->setEnabled(TRUE); // <FS:Ansariel> Does not exist as of 12-09-2014
	getChildView("log_path_button-panelsetup")->setEnabled(TRUE);// second set of controls for panel_preferences_setup  -WoLf
	getChildView("open_log_path_button-panelsetup")->setEnabled(TRUE);
	std::string Chatlogsdir = gDirUtilp->getOSUserAppDir();
	
	getChildView("conversation_log_combo")->setEnabled(TRUE);	// <FS:CR>
	getChildView("LogNearbyChat")->setEnabled(TRUE);	// <FS:CR>
	//getChildView("log_nearby_chat")->setEnabled(TRUE); // <FS:Ansariel> Does not exist as of 12-09-2014
	//[FIX FIRE-2765 : SJ] Set Chatlog Reset Button on enabled when Chatlogpath isn't the default folder
	if (gSavedPerAccountSettings.getString("InstantMessageLogPath") != gDirUtilp->getOSUserAppDir())
	{
		getChildView("reset_logpath")->setEnabled(TRUE);
	}
	// <FS:Ansariel> Show email address in preferences (FIRE-1071)
	std::string display_email(email);
	if(display_email.size() > 30)
	{
		display_email.resize(30);
		display_email += "...";
	}
	getChild<LLCheckBoxCtrl>("send_im_to_email")->setLabelArg("[EMAIL]", display_email);
	// </FS:Ansariel> Show email address in preferences (FIRE-1071)

	// <FS:Ansariel> FIRE-420: Show end of last conversation in history
	getChildView("LogShowHistory")->setEnabled(TRUE);

	// <FS:Ansariel> Clear inventory cache button
	getChildView("ClearInventoryCache")->setEnabled(TRUE);

	// <FS:Ansariel> FIRE-18250: Option to disable default eye movement
	getChildView("FSStaticEyes")->setEnabled(TRUE);
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

	if (0 == value || LLVOAvatar::IMPOSTORS_OFF <= value)
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

	if (0 == value || LLVOAvatar::IMPOSTORS_OFF <= value)
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

void LLFloaterPreferenceGraphicsAdvanced::updateSliderText(LLSliderCtrl* ctrl, LLTextBox* text_box)
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

void LLFloaterPreferenceGraphicsAdvanced::updateMaxNonImpostors()
{
	// Called when the IndirectMaxNonImpostors control changes
	// Responsible for fixing the slider label (IndirectMaxNonImpostorsText) and setting RenderAvatarMaxNonImpostors
	LLSliderCtrl* ctrl = getChild<LLSliderCtrl>("IndirectMaxNonImpostors",true);
	U32 value = ctrl->getValue().asInteger();

	if (0 == value || LLVOAvatar::IMPOSTORS_OFF <= value)
	{
		value=0;
	}
	gSavedSettings.setU32("RenderAvatarMaxNonImpostors", value);
	LLVOAvatar::updateImpostorRendering(value); // make it effective immediately
	setMaxNonImpostorsText(value, getChild<LLTextBox>("IndirectMaxNonImpostorsText"));
}

void LLFloaterPreferenceGraphicsAdvanced::setMaxNonImpostorsText(U32 value, LLTextBox* text_box)
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

void LLAvatarComplexityControls::updateMax(LLSliderCtrl* slider, LLTextBox* value_label)
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
	setText(max_arc, value_label);
}

void LLAvatarComplexityControls::setText(U32 value, LLTextBox* text_box)
{
	if (0 == value)
	{
		text_box->setText(LLTrans::getString("no_limit"));
	}
	else
	{
		// <FS:Ansariel> Proper number formatting with delimiter
		//text_box->setText(llformat("%d", value));
		std::string output_string;
		LLLocale locale("");
		LLResMgr::getInstance()->getIntegerString(output_string, value);
		text_box->setText(output_string);
	}
}

void LLFloaterPreference::updateMaxComplexity()
{
	// Called when the IndirectMaxComplexity control changes
    LLAvatarComplexityControls::updateMax(
        getChild<LLSliderCtrl>("IndirectMaxComplexity"),
        getChild<LLTextBox>("IndirectMaxComplexityText"));
}

void LLFloaterPreferenceGraphicsAdvanced::updateMaxComplexity()
{
	// Called when the IndirectMaxComplexity control changes
    LLAvatarComplexityControls::updateMax(
        getChild<LLSliderCtrl>("IndirectMaxComplexity"),
        getChild<LLTextBox>("IndirectMaxComplexityText"));
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

std::string get_category_path(LLUUID cat_id)
{
    LLViewerInventoryCategory* cat = gInventory.getCategory(cat_id);
    std::string localized_cat_name;
    if (!LLTrans::findString(localized_cat_name, "InvFolder " + cat->getName()))
    {
        localized_cat_name = cat->getName();
    }

    if (cat->getParentUUID().notNull())
    {
        return get_category_path(cat->getParentUUID()) + " > " + localized_cat_name;
    }
    else
    {
        return localized_cat_name;
    }
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
	//	LLSD().with("people_panel_tab_name", "blocked_panel"));
	BOOL saved_setting = gSavedSettings.getBOOL("FSDisableBlockListAutoOpen");
	gSavedSettings.setBOOL("FSDisableBlockListAutoOpen", FALSE);
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
	mClickActionDirty = true;
}

void LLFloaterPreference::onClickPermsDefault()
{
	LLFloaterReg::showInstance("perms_default");
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
	LLButton * delete_transcripts_buttonp = getChild<LLButton>("delete_transcripts");

	if (!delete_transcripts_buttonp->getEnabled())
	{
		delete_transcripts_buttonp->setEnabled(true);
	}
}

void LLFloaterPreference::updateClickActionSettings()
{
	const int single_clk_action = getChild<LLComboBox>("single_click_action_combo")->getValue().asInteger();
	const int double_clk_action = getChild<LLComboBox>("double_click_action_combo")->getValue().asInteger();

	gSavedSettings.setBOOL("ClickToWalk",			single_clk_action == 1);
	gSavedSettings.setBOOL("DoubleClickAutoPilot",	double_clk_action == 1);
	gSavedSettings.setBOOL("DoubleClickTeleport",	double_clk_action == 2);
}

void LLFloaterPreference::updateClickActionControls()
{
	const bool click_to_walk = gSavedSettings.getBOOL("ClickToWalk");
	const bool dbl_click_to_walk = gSavedSettings.getBOOL("DoubleClickAutoPilot");
	const bool dbl_click_to_teleport = gSavedSettings.getBOOL("DoubleClickTeleport");

	getChild<LLComboBox>("single_click_action_combo")->setValue((int)click_to_walk);
	getChild<LLComboBox>("double_click_action_combo")->setValue(dbl_click_to_teleport ? 2 : (int)dbl_click_to_walk);
}

// <FS:PP> Load UI Sounds tabs settings
void LLFloaterPreference::updateUISoundsControls()
{
	getChild<LLComboBox>("PlayModeUISndNewIncomingIMSession")->setValue((int)gSavedSettings.getU32("PlayModeUISndNewIncomingIMSession")); // 0, 1, 2, 3. Shared with Chat > Notifications > "When receiving Instant Messages"
	getChild<LLComboBox>("PlayModeUISndNewIncomingGroupIMSession")->setValue((int)gSavedSettings.getU32("PlayModeUISndNewIncomingGroupIMSession")); // 0, 1, 2, 3. Shared with Chat > Notifications > "When receiving Group Instant Messages"
	getChild<LLComboBox>("PlayModeUISndNewIncomingConfIMSession")->setValue((int)gSavedSettings.getU32("PlayModeUISndNewIncomingConfIMSession")); // 0, 1, 2, 3. Shared with Chat > Notifications > "When receiving AdHoc Instant Messages"
#ifdef OPENSIM
	getChild<LLTextBox>("textFSRestartOpenSim")->setVisible(TRUE);
	getChild<LLLineEditor>("UISndRestartOpenSim")->setVisible(TRUE);
	getChild<LLButton>("Prev_UISndRestartOpenSim")->setVisible(TRUE);
	getChild<LLButton>("Def_UISndRestartOpenSim")->setVisible(TRUE);
	getChild<LLCheckBoxCtrl>("PlayModeUISndRestartOpenSim")->setVisible(TRUE);
#endif
	getChild<LLComboBox>("UseLSLFlightAssist")->setValue((int)gSavedPerAccountSettings.getF32("UseLSLFlightAssist")); // Flight Assist combo box; Not sound-related, but better to place it here instead of creating whole new void

	// FIRE-9856: Mute sound effects disable plays sound from collisions and plays sound from gestures checkbox not disable after restart/relog
	bool mute_sound_effects = gSavedSettings.getBOOL("MuteSounds");
	bool mute_all_sounds = gSavedSettings.getBOOL("MuteAudio");
	getChild<LLCheckBoxCtrl>("gesture_audio_play_btn")->setEnabled(!(mute_sound_effects || mute_all_sounds));
	getChild<LLCheckBoxCtrl>("collisions_audio_play_btn")->setEnabled(!(mute_sound_effects || mute_all_sounds));
}
// </FS:PP>

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
	LLPanel * panel = tab_containerp->getPanelByName(name);
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
//	gViewerThrottle.setMaxBandwidth((F32) newvalue.asReal());
//	return true;
//}

//class LLPanelPreference::Updater : public LLEventTimer
//{

//public:

//	typedef boost::function<bool(const LLSD&)> callback_t;

//	Updater(callback_t cb, F32 period)
//	:LLEventTimer(period),
//	 mCallback(cb)
//	{
//		mEventTimer.stop();
//	}

//	virtual ~Updater(){}

//	void update(const LLSD& new_value)
//	{
//		mNewValue = new_value;
//		mEventTimer.start();
//	}

//protected:

//	BOOL tick()
//	{
//		mCallback(mNewValue);
//		mEventTimer.stop();

//		return FALSE;
//	}

//private:

//	LLSD mNewValue;
//	callback_t mCallback;
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
	// mCommitCallbackRegistrar.add("Pref.setControlFalse",	boost::bind(&LLPanelPreference::setControlFalse,this, _2));
	mCommitCallbackRegistrar.add("Pref.updateMediaAutoPlayCheckbox",	boost::bind(&LLPanelPreference::updateMediaAutoPlayCheckbox, this, _1));
	mCommitCallbackRegistrar.add("Pref.PrefDelete",	boost::bind(&LLPanelPreference::deletePreset, this, _2));
	mCommitCallbackRegistrar.add("Pref.PrefSave",	boost::bind(&LLPanelPreference::savePreset, this, _2));
	mCommitCallbackRegistrar.add("Pref.PrefLoad",	boost::bind(&LLPanelPreference::loadPreset, this, _2));

	// <FS:Ansariel> Customizable contact list columns
	mCommitCallbackRegistrar.add("FS.CheckContactListColumnMode", boost::bind(&LLPanelPreference::onCheckContactListColumnMode, this));
}

//virtual
BOOL LLPanelPreference::postBuild()
{
	////////////////////// PanelGeneral ///////////////////
	if (hasChild("display_names_check", TRUE))
	{
		BOOL use_people_api = gSavedSettings.getBOOL("UsePeopleAPI");
		LLCheckBoxCtrl* ctrl_display_name = getChild<LLCheckBoxCtrl>("display_names_check");
		ctrl_display_name->setEnabled(use_people_api);
		if (!use_people_api)
		{
			ctrl_display_name->setValue(FALSE);
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
	if (hasChild("FSNotifyIMFlash", TRUE))
	{
		gSavedSettings.getControl("FSChatWindow")->getSignal()->connect(boost::bind(&LLPanelPreference::onChatWindowChanged, this));
		onChatWindowChanged();
	}
	// </FS:Ansariel>

	// <FS:Ansariel> Exodus' mouselook combat feature
	if (hasChild("FSMouselookCombatFeatures", TRUE))
	{
		gSavedSettings.getControl("EnableMouselook")->getSignal()->connect(boost::bind(&LLPanelPreference::updateMouselookCombatFeatures, this));
		gSavedSettings.getControl("FSMouselookCombatFeatures")->getSignal()->connect(boost::bind(&LLPanelPreference::updateMouselookCombatFeatures, this));
		updateMouselookCombatFeatures();
	}
	// </FS:Ansariel>

	////////////////////// PanelVoice ///////////////////
	// <FS:Ansariel> Doesn't exist as of 25-07-2014
	//if (hasChild("voice_unavailable", TRUE))
	//{
	//	BOOL voice_disabled = gSavedSettings.getBOOL("CmdLineDisableVoice");
	//	getChildView("voice_unavailable")->setVisible( voice_disabled);
	//	getChildView("enable_voice_check")->setVisible( !voice_disabled);
	//}
	// </FS:Ansariel>
	
	//////////////////////PanelSkins ///////////////////
	
	/* <FS:CR> Handled below
	if (hasChild("skin_selection", TRUE))
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
	if (hasChild("media_enabled", TRUE))
	{
		bool media_enabled = gSavedSettings.getBOOL("AudioStreamingMedia");
		
		getChild<LLCheckBoxCtrl>("media_enabled")->set(media_enabled);
		getChild<LLCheckBoxCtrl>("autoplay_enabled")->setEnabled(media_enabled);
	}
	if (hasChild("music_enabled", TRUE))
	{
		getChild<LLCheckBoxCtrl>("music_enabled")->set(gSavedSettings.getBOOL("AudioStreamingMusic"));
	}
	if (hasChild("media_filter"))
	{
		getChild<LLCheckBoxCtrl>("media_filter")->set(gSavedSettings.getBOOL("MediaEnableFilter"));
	}
	if (hasChild("voice_call_friends_only_check", TRUE))
	{
		getChild<LLCheckBoxCtrl>("voice_call_friends_only_check")->setCommitCallback(boost::bind(&showFriendsOnlyWarning, _1, _2));
	}
	if (hasChild("favorites_on_login_check", TRUE))
	{
		getChild<LLCheckBoxCtrl>("favorites_on_login_check")->setCommitCallback(boost::bind(&handleFavoritesOnLoginChanged, _1, _2));
		// <FS:Ansariel> [FS Login Panel]
		//bool show_favorites_at_login = LLPanelLogin::getShowFavorites();
		bool show_favorites_at_login = FSPanelLogin::getShowFavorites();
		// </FS:Ansariel> [FS Login Panel]
		getChild<LLCheckBoxCtrl>("favorites_on_login_check")->setValue(show_favorites_at_login);
	}

	//////////////////////PanelAdvanced ///////////////////
	if (hasChild("modifier_combo", TRUE))
	{
		//localizing if push2talk button is set to middle mouse
		if (MIDDLE_MOUSE_CV == getChild<LLUICtrl>("modifier_combo")->getValue().asString())
		{
			getChild<LLUICtrl>("modifier_combo")->setValue(getString("middle_mouse"));
		}
	}
	// Panel Setup (Network) -WoLf
	if (hasChild("connection_port_enabled"))
	{
		getChild<LLCheckBoxCtrl>("connection_port_enabled")->setCommitCallback(boost::bind(&showCustomPortWarning, _1, _2));
	} 
	// [/WoLf]

	//////////////////////PanelSetup ///////////////////
	//<FS:HG> FIRE-6340, FIRE-6567 - Setting Bandwidth issues
	//if (hasChild("max_bandwidth"), TRUE)
	//{
	//	mBandWidthUpdater = new LLPanelPreference::Updater(boost::bind(&handleBandwidthChanged, _1), BANDWIDTH_UPDATER_TIMEOUT);
	//	gSavedSettings.getControl("ThrottleBandwidthKBPS")->getSignal()->connect(boost::bind(&LLPanelPreference::Updater::update, mBandWidthUpdater, _2));
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
	if (hasChild("OnlineOfflinetoNearbyChat", TRUE))
	{
		getChildView("OnlineOfflinetoNearbyChatHistory")->setEnabled(getChild<LLUICtrl>("OnlineOfflinetoNearbyChat")->getValue().asBoolean());
	}

	// <FS:Ansariel> Only enable Growl checkboxes if Growl is usable
	if (hasChild("notify_growl_checkbox", TRUE))
	{
		BOOL growl_enabled = gSavedSettings.getBOOL("FSEnableGrowl") && GrowlManager::isUsable();
		getChild<LLCheckBoxCtrl>("notify_growl_checkbox")->setCommitCallback(boost::bind(&LLPanelPreference::onEnableGrowlChanged, this));
		getChild<LLCheckBoxCtrl>("notify_growl_checkbox")->setEnabled(GrowlManager::isUsable());
		getChild<LLCheckBoxCtrl>("notify_growl_always_checkbox")->setEnabled(growl_enabled);
		getChild<LLCheckBoxCtrl>("FSFilterGrowlKeywordDuplicateIMs")->setEnabled(growl_enabled);
	}
	// </FS:Ansariel>

	////////////////////// PanelUI ///////////////////
	// <FS:Ansariel> Customizable contact list columns
	if (hasChild("textFriendlistColumns", TRUE))
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
	//	delete mBandWidthUpdater;
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
}

void LLPanelPreference::showFriendsOnlyWarning(LLUICtrl* checkbox, const LLSD& value)
{
	if (checkbox && checkbox->getValue())
	{
		LLNotificationsUtil::add("FriendsAndGroupsOnly");
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

// <FS:Ansariel> Only enable Growl checkboxes if Growl is usable
void LLPanelPreference::onEnableGrowlChanged()
{
	BOOL growl_enabled = gSavedSettings.getBOOL("FSEnableGrowl") && GrowlManager::isUsable();
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
	color.mV[VW] = value.asReal();
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

void LLPanelPreference::cancel()
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
		control->set(LLSD(FALSE));
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

		getChild<LLCheckBoxCtrl>("media_auto_play_btn")->setEnabled(music_enabled || media_enabled);
	}
}

void LLPanelPreference::deletePreset(const LLSD& user_data)
{
	std::string subdirectory = user_data.asString();
	LLFloaterReg::showInstance("delete_pref_preset", subdirectory);
}

void LLPanelPreference::savePreset(const LLSD& user_data)
{
	std::string subdirectory = user_data.asString();
	LLFloaterReg::showInstance("save_pref_preset", subdirectory);
}

void LLPanelPreference::loadPreset(const LLSD& user_data)
{
	std::string subdirectory = user_data.asString();
	LLFloaterReg::showInstance("load_pref_preset", subdirectory);
}

void LLPanelPreference::setHardwareDefaults()
{
}

class LLPanelPreferencePrivacy : public LLPanelPreference
{
public:
	LLPanelPreferencePrivacy()
	{
		mAccountIndependentSettings.push_back("VoiceCallsFriendsOnly");
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
	/*virtual*/ BOOL postBuild()
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
		clear_item_btn->setEnabled(FALSE);
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
				clear_item_btn->setEnabled(TRUE);
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
	FSCopyTransInventoryDropTarget*	mInvDropTarget;
	std::string						mAutoresponseItem;

	// <FS:Ansariel> DebugLookAt checkbox status not working properly
	void onChangeDebugLookAt()
	{
		getChild<LLCheckBoxCtrl>("showlookat")->set(gSavedPerAccountSettings.getS32("DebugLookAt") == 0 ? FALSE : TRUE);
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

BOOL LLPanelPreferenceGraphics::postBuild()
{
	// <FS:Ansariel> Improved graphics preferences
	//LLFloaterReg::showInstance("prefs_graphics_advanced");
	//LLFloaterReg::hideInstance("prefs_graphics_advanced");
	// </FS:Ansariel>

// Don't do this on Mac as their braindead GL versioning
// sets this when 8x and 16x are indeed available
//
#if !LL_DARWIN
	if (gGLManager.mIsIntel || gGLManager.mGLVersion < 3.f)
	{ //remove FSAA settings above "4x"
		LLComboBox* combo = getChild<LLComboBox>("fsaa");
		combo->remove("8x");
		combo->remove("16x");
	}
#endif

	resetDirtyChilds();
	setPresetText();

	LLPresetsManager* presetsMgr = LLPresetsManager::getInstance();
    presetsMgr->setPresetListChangeCallback(boost::bind(&LLPanelPreferenceGraphics::onPresetsListChange, this));
    presetsMgr->createMissingDefault(); // a no-op after the first time, but that's ok
    

// <FS:CR> Hide this until we have fullscreen mode functional on OSX again
#ifdef LL_DARWIN
	getChild<LLCheckBoxCtrl>("Fullscreen Mode")->setVisible(FALSE);
#endif // LL_DARWIN
// </FS:CR>

	return LLPanelPreference::postBuild();
}
void LLPanelPreferenceGraphics::draw()
{
	// <FS:Ansariel> Graphic preset controls independent from XUI
	//setPresetText();
	LLPanelPreference::draw();
}

void LLPanelPreferenceGraphics::onPresetsListChange()
{
	resetDirtyChilds();
	setPresetText();

	//LLFloaterPreference* instance = LLFloaterReg::findTypedInstance<LLFloaterPreference>("preferences");
	//if (instance && !gSavedSettings.getString("PresetGraphicActive").empty())
	//{
	//	instance->saveSettings(); //make cancel work correctly after changing the preset
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
	//	LLFloaterPreference* instance = LLFloaterReg::findTypedInstance<LLFloaterPreference>("preferences");
	//	if (instance)
	//	{
	//		instance->saveGraphicsPreset(preset_graphic_active);
	//	}
	//}
	// </FS:Ansariel>

	// <FS:Ansariel> Graphic preset controls independent from XUI
	//if (hasDirtyChilds() && !preset_graphic_active.empty())
	//{
	//	gSavedSettings.setString("PresetGraphicActive", "");
	//	preset_graphic_active.clear();
	//	// This doesn't seem to cause an infinite recursion.  This trigger is needed to cause the pulldown
	//	// panel to update.
	//	LLPresetsManager::getInstance()->triggerChangeSignal();
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

void LLPanelPreferenceGraphics::cancel()
{
	// <FS:Ansariel> Improved graphics preferences
	resetDirtyChilds();
	LLPanelPreference::cancel();
}
void LLPanelPreferenceGraphics::saveSettings()
{
	resetDirtyChilds();
	LLPanelPreference::saveSettings();
}
void LLPanelPreferenceGraphics::setHardwareDefaults()
{
	resetDirtyChilds();
	// <FS:Ansariel> Improved graphics preferences
	LLPanelPreference::setHardwareDefaults();
}

LLFloaterPreferenceGraphicsAdvanced::LLFloaterPreferenceGraphicsAdvanced(const LLSD& key)
	: LLFloater(key)
{
	mCommitCallbackRegistrar.add("Pref.VertexShaderEnable",		boost::bind(&LLFloaterPreferenceGraphicsAdvanced::onVertexShaderEnable, this));
	mCommitCallbackRegistrar.add("Pref.UpdateIndirectMaxNonImpostors", boost::bind(&LLFloaterPreferenceGraphicsAdvanced::updateMaxNonImpostors,this));
	mCommitCallbackRegistrar.add("Pref.UpdateIndirectMaxComplexity",   boost::bind(&LLFloaterPreferenceGraphicsAdvanced::updateMaxComplexity,this));
}

LLFloaterPreferenceGraphicsAdvanced::~LLFloaterPreferenceGraphicsAdvanced()
{
}

LLFloaterPreferenceProxy::LLFloaterPreferenceProxy(const LLSD& key)
	: LLFloater(key),
	  mSocksSettingsDirty(false)
{
	mCommitCallbackRegistrar.add("Proxy.OK",                boost::bind(&LLFloaterPreferenceProxy::onBtnOk, this));
	mCommitCallbackRegistrar.add("Proxy.Cancel",            boost::bind(&LLFloaterPreferenceProxy::onBtnCancel, this));
	mCommitCallbackRegistrar.add("Proxy.Change",            boost::bind(&LLFloaterPreferenceProxy::onChangeSocksSettings, this));
}

void LLFloaterPreferenceGraphicsAdvanced::onOpen(const LLSD& key)
{
    refresh();
}

void LLFloaterPreferenceGraphicsAdvanced::onClickCloseBtn(bool app_quitting)
{
	LLFloaterPreference* instance = LLFloaterReg::findTypedInstance<LLFloaterPreference>("preferences");
	if (instance)
	{
		instance->cancel();
	}
}

LLFloaterPreferenceProxy::~LLFloaterPreferenceProxy()
{
}

BOOL LLFloaterPreferenceProxy::postBuild()
{
	LLRadioGroup* socksAuth = getChild<LLRadioGroup>("socks5_auth_type");
	if (!socksAuth)
	{
		return FALSE;
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

	return TRUE;
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
			getChild<LLCheckBoxCtrl>("socks_proxy_enabled")->get() == FALSE )||(
					otherHttpProxy->getSelectedValue().asString() == "Web" &&
					getChild<LLCheckBoxCtrl>("web_proxy_enabled")->get() == FALSE ) )
	{
		otherHttpProxy->selectFirstItem();
	}

}

// [SL:KB] - Patch: Viewer-CrashReporting | Checked: 2010-11-16 (Catznip-2.6.0a) | Added: Catznip-2.4.0b
static LLPanelInjector<LLPanelPreferenceCrashReports> t_pref_crashreports("panel_preference_crashreports");

LLPanelPreferenceCrashReports::LLPanelPreferenceCrashReports()
	: LLPanelPreference()
{
}

BOOL LLPanelPreferenceCrashReports::postBuild()
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

	refresh();

	return LLPanelPreference::postBuild();
}

void LLPanelPreferenceCrashReports::refresh()
{
	LLCheckBoxCtrl* pSendCrashReports = getChild<LLCheckBoxCtrl>("checkSendCrashReports");
	pSendCrashReports->setEnabled(TRUE);

	bool fEnable = pSendCrashReports->get();
	getChild<LLUICtrl>("comboSaveMiniDumpType")->setEnabled(fEnable);
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

void LLPanelPreferenceCrashReports::cancel()
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

BOOL LLPanelPreferenceSkins::postBuild()
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
			gSavedSettings.setBOOL("ResetToolbarSettings", TRUE);
		}

		if (m_Skin == "starlight" || m_Skin == "starlightcui")
		{
			std::string noteMessage;

			if (gSavedSettings.getBOOL("ShowMenuBarLocation"))
			{
				noteMessage = LLTrans::getString("skin_defaults_starlight_location");
				gSavedSettings.setBOOL("ShowMenuBarLocation", FALSE);
			}

			if (!gSavedSettings.getBOOL("ShowNavbarNavigationPanel"))
			{
				if (!noteMessage.empty())
				{
					noteMessage += "\n";
				}
				noteMessage += LLTrans::getString("skin_defaults_starlight_navbar");
				gSavedSettings.setBOOL("ShowNavbarNavigationPanel", TRUE);
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

void LLPanelPreferenceSkins::cancel()
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
	
	BOOL fFound = m_pSkinCombo->setSelectedByValue(m_Skin, TRUE);
	if (!fFound)
	{
		m_pSkinCombo->setSelectedByValue("default", TRUE);
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

	BOOL fFound = m_pSkinThemeCombo->setSelectedByValue(m_SkinTheme, TRUE);
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
// copied from llxfer_file.cpp - Hopefully this will be part of LLFile some day -Zi
// added a safeguard so the destination file is only created when the source file exists -Zi
S32 copy_prefs_file(const std::string& from, const std::string& to)
{
	LL_WARNS() << "copying " << from << " to " << to << LL_ENDL;
	S32 rv = 0;
	LLFILE* in = LLFile::fopen(from, "rb");	/*Flawfinder: ignore*/
	if(!in)
	{
		LL_WARNS() << "couldn't open source file " << from << " - copy aborted." << LL_ENDL;
		return -1;
	}

	LLFILE* out = LLFile::fopen(to, "wb");	/*Flawfinder: ignore*/
	if(!out)
	{
		fclose(in);
		LL_WARNS() << "couldn't open destination file " << to << " - copy aborted." << LL_ENDL;
		return -1;
	}

	S32 read = 0;
	const S32 COPY_BUFFER_SIZE = 16384;
	U8 buffer[COPY_BUFFER_SIZE];
	while(((read = fread(buffer, 1, sizeof(buffer), in)) > 0)
		  && (fwrite(buffer, 1, read, out) == (U32)read));		/* Flawfinder : ignore */
	if(ferror(in) || ferror(out)) rv = -2;
	
	if(in) fclose(in);
	if(out) fclose(out);
	
	return rv;
}

static LLPanelInjector<FSPanelPreferenceBackup> t_pref_backup("panel_preference_backup");

FSPanelPreferenceBackup::FSPanelPreferenceBackup() : LLPanelPreference()
{
	mCommitCallbackRegistrar.add("Pref.SetBackupSettingsPath",	boost::bind(&FSPanelPreferenceBackup::onClickSetBackupSettingsPath, this));
	mCommitCallbackRegistrar.add("Pref.BackupSettings",			boost::bind(&FSPanelPreferenceBackup::onClickBackupSettings, this));
	mCommitCallbackRegistrar.add("Pref.RestoreSettings",		boost::bind(&FSPanelPreferenceBackup::onClickRestoreSettings, this));
	mCommitCallbackRegistrar.add("Pref.BackupSelectAll",		boost::bind(&FSPanelPreferenceBackup::onClickSelectAll, this));
	mCommitCallbackRegistrar.add("Pref.BackupDeselectAll",		boost::bind(&FSPanelPreferenceBackup::onClickDeselectAll, this));
}

BOOL FSPanelPreferenceBackup::postBuild()
{
	// <FS:Zi> Backup Settings
	// Apparently, line editors don't update with their settings controls, so do that manually here
	std::string dir_name = gSavedSettings.getString("SettingsBackupPath");
	getChild<LLLineEditor>("settings_backup_path")->setValue(dir_name);
	// </FS:Zi>
	
	return LLPanelPreference::postBuild();
}

void FSPanelPreferenceBackup::onClickSetBackupSettingsPath()
{
	std::string dir_name = gSavedSettings.getString("SettingsBackupPath");
	LLDirPicker& picker = LLDirPicker::instance();
	if (!picker.getDir(&dir_name))
	{
		// canceled
		return;
	}

	dir_name = picker.getDirName();
	gSavedSettings.setString("SettingsBackupPath", dir_name);
	getChild<LLLineEditor>("settings_backup_path")->setValue(dir_name);
}

void FSPanelPreferenceBackup::onClickBackupSettings()
{
	LL_INFOS("SettingsBackup") << "entered" << LL_ENDL;
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
		LLControlGroup* group;	// our control group that will hold the backup controls
		f(LLControlGroup* g) : group(g) {}	// constructor, initializing group variable
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
					LLControlVariable::PERSIST_NONDFT);	// need to set persisitent flag, or it won't be saved
			}
		}
	} func_global(&backup_global_controls), func_per_account(&backup_per_account_controls);

	// run backup on global controls
	LL_INFOS("SettingsBackup") << "running functor on global settings" << LL_ENDL;
	gSavedSettings.applyToAll(&func_global);

	// make sure to write color preferences before copying them
	LL_INFOS("SettingsBackup") << "saving UI color table" << LL_ENDL;
	LLUIColorTable::instance().saveUserSettings();

	// set it to save defaults, too (FALSE), because our declaration automatically
	// makes the value default
	std::string backup_global_name = gDirUtilp->getExpandedFilename(LL_PATH_NONE, dir_name,
				LLAppViewer::instance()->getSettingsFilename("Default","Global"));
	LL_INFOS("SettingsBackup") << "saving backup global settings" << LL_ENDL;
	backup_global_controls.saveToFile(backup_global_name, FALSE);

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
			// save defaults here as well (FALSE)
			LL_INFOS("SettingsBackup") << "saving backup per account settings" << LL_ENDL;
			backup_per_account_controls.saveToFile(backup_per_account_name, FALSE);

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

		// run restore on global controls
		LL_INFOS("SettingsBackup") << "restoring global settings from backup" << LL_ENDL;
		gSavedSettings.loadFromFile(backup_global_name);
		LL_INFOS("SettingsBackup") << "saving global settings" << LL_ENDL;
		gSavedSettings.saveToFile(global_name, TRUE);
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
			gSavedPerAccountSettings.saveToFile(per_account_name, TRUE);
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
		gToolBarView->loadToolbars(FALSE);
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
	gSavedSettings.setBOOL("FSFirstRunAfterSettingsRestore", TRUE);
	
	// Tell the user we have finished restoring settings and the viewer must shut down
	LLNotificationsUtil::add("RestoreFinished", LLSD(), LLSD(), boost::bind(&FSPanelPreferenceBackup::onQuitConfirmed, this, _1, _2));
}

// User confirmed the shutdown and we proceed
void FSPanelPreferenceBackup::onQuitConfirmed(const LLSD& notification,const LLSD& response)
{
	// Make sure the viewer will not save any settings on exit, so our copied files will survive
	LLAppViewer::instance()->setSaveSettingsOnExit(FALSE);
	// Quit the viewer so all gets saved immediately
	LL_INFOS("SettingsBackup") << "setting to quit" << LL_ENDL;
	LLAppViewer::instance()->requestQuit();
}

void FSPanelPreferenceBackup::onClickSelectAll()
{
	doSelect(TRUE);
}

void FSPanelPreferenceBackup::onClickDeselectAll()
{
	doSelect(FALSE);
}

void FSPanelPreferenceBackup::doSelect(BOOL all)
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

void FSPanelPreferenceBackup::applySelection(LLScrollListCtrl* control, BOOL all)
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
#ifdef OPENSIM
static LLPanelInjector<LLPanelPreferenceOpensim> t_pref_opensim("panel_preference_opensim");

LLPanelPreferenceOpensim::LLPanelPreferenceOpensim() : LLPanelPreference(),
	mGridListControl(NULL)
{
	mCommitCallbackRegistrar.add("Pref.ClearDebugSearchURL", boost::bind(&LLPanelPreferenceOpensim::onClickClearDebugSearchURL, this));
	mCommitCallbackRegistrar.add("Pref.PickDebugSearchURL", boost::bind(&LLPanelPreferenceOpensim::onClickPickDebugSearchURL, this));
	mCommitCallbackRegistrar.add("Pref.AddGrid", boost::bind(&LLPanelPreferenceOpensim::onClickAddGrid, this));
	mCommitCallbackRegistrar.add("Pref.ClearGrid", boost::bind(&LLPanelPreferenceOpensim::onClickClearGrid, this));
	mCommitCallbackRegistrar.add("Pref.RefreshGrid", boost::bind( &LLPanelPreferenceOpensim::onClickRefreshGrid, this));
	mCommitCallbackRegistrar.add("Pref.RemoveGrid", boost::bind( &LLPanelPreferenceOpensim::onClickRemoveGrid, this));
	mCommitCallbackRegistrar.add("Pref.SaveGrid", boost::bind(&LLPanelPreferenceOpensim::onClickSaveGrid, this));
}

BOOL LLPanelPreferenceOpensim::postBuild()
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
	refreshGridList();

	return LLPanelPreference::postBuild();
}

void LLPanelPreferenceOpensim::onSelectGrid()
{
	LLSD  grid_info;
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

void LLPanelPreferenceOpensim::cancel()
{
	LLGridManager::getInstance()->resetGrids();
	FSPanelLogin::updateServer();
}

void LLPanelPreferenceOpensim::onClickAddGrid()
{

	std::string new_grid = gSavedSettings.getString("OpensimPrefsAddGrid");

	if (!new_grid.empty())
	{
		getChild<LLUICtrl>("grid_management_panel")->setEnabled(FALSE);
		LLGridManager::getInstance()->addGridListChangedCallback(boost::bind(&LLPanelPreferenceOpensim::addedGrid, this, _1));
		LLGridManager::getInstance()->addGrid(new_grid);
	}
}

void LLPanelPreferenceOpensim::addedGrid(bool success)
{
	if (success)
	{
		onClickClearGrid();
	}
	refreshGridList(success);
}

// TODO: Save changes to grid entries
void LLPanelPreferenceOpensim::onClickSaveGrid()
{
	LLSD  grid_info;
	grid_info[GRID_VALUE] = mGridListControl->getSelectedValue();
	grid_info[GRID_LABEL_VALUE] = mEditorGridName->getValue();
	grid_info[GRID_LOGIN_URI_VALUE][0] = mEditorGridURI->getValue();
	grid_info[GRID_LOGIN_PAGE_VALUE] = mEditorLoginPage->getValue();
	grid_info[GRID_HELPER_URI_VALUE] = mEditorHelperURI->getValue();
	grid_info["about"] = mEditorWebsite->getValue();
	grid_info["help"] = mEditorSupport->getValue();
	grid_info[GRID_REGISTER_NEW_ACCOUNT] = mEditorRegister->getValue();
	grid_info[GRID_FORGOT_PASSWORD] = mEditorPassword->getValue();
	grid_info["search"] = mEditorSearch->getValue();
	grid_info["message"] = mEditorGridMessage->getValue();

	// GridEntry* grid_entry = new GridEntry;
	// grid_entry->grid = grid_info;
	// grid_entry->set_current = false;
	
	//getChild<LLUICtrl>("grid_management_panel")->setEnabled(FALSE);
	//LLGridManager::getInstance()->addGridListChangedCallback(boost::bind(&LLPanelPreferenceOpensim::addedGrid, this, _1));
	//LLGridManager::getInstance()->addGrid(grid_entry, LLGridManager::MANUAL);
}

void LLPanelPreferenceOpensim::onClickClearGrid()
{
	gSavedSettings.setString("OpensimPrefsAddGrid", std::string());
}

void LLPanelPreferenceOpensim::onClickRefreshGrid()
{
	std::string grid = mGridListControl->getSelectedValue();
	getChild<LLUICtrl>("grid_management_panel")->setEnabled(FALSE);
	LLGridManager::getInstance()->addGridListChangedCallback(boost::bind(&LLPanelPreferenceOpensim::refreshGridList, this, _1));
	LLGridManager::getInstance()->reFetchGrid(grid);
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
		getChild<LLUICtrl>("grid_management_panel")->setEnabled(FALSE);
		/*mGridListChanged =*/ LLGridManager::getInstance()->addGridListChangedCallback(boost::bind(&LLPanelPreferenceOpensim::refreshGridList, this, _1));
		LLGridManager::getInstance()->removeGrid(grid);
	}
	return false;
}

void LLPanelPreferenceOpensim::refreshGridList(bool success)
{
	FSPanelLogin::updateServer();

	getChild<LLUICtrl>("grid_management_panel")->setEnabled(TRUE);

	if (!mGridListControl)
	{
		LL_WARNS() << "No GridListControl - bug or out of memory" << LL_ENDL;
		return;
	}

	mGridListControl->operateOnAll(LLCtrlListInterface::OP_DELETE);
	mGridListControl->sortByColumnIndex(0, TRUE);

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

#endif // OPENSIM
// <FS:AW optional opensim support>

// <FS:ND> Code to filter/search in the prefs panel
void LLFloaterPreference::onUpdateFilterTerm(bool force)
{
	LLWString seachValue = utf8str_to_wstring( mFilterEdit->getValue() );
	LLWStringUtil::toLower( seachValue );

	if( !mSearchData || (mSearchData->mLastFilter == seachValue && !force))
		return;

	mSearchData->mLastFilter = seachValue;

	if( !mSearchData->mRootTab )
		return;

	mSearchData->mRootTab->hightlightAndHide( seachValue );
	LLTabContainer *pRoot = getChild< LLTabContainer >( "pref core" );
	if( pRoot )
		pRoot->selectFirstTab();
}

void collectChildren( LLView const *aView, nd::prefs::PanelDataPtr aParentPanel, nd::prefs::TabContainerDataPtr aParentTabContainer )
{
	if( !aView )
		return;

	llassert_always( aParentPanel || aParentTabContainer );

	LLView::child_list_const_iter_t itr = aView->beginChild();
	LLView::child_list_const_iter_t itrEnd = aView->endChild();

	while( itr != itrEnd )
	{
		LLView *pView = *itr;
		nd::prefs::PanelDataPtr pCurPanelData = aParentPanel;
		nd::prefs::TabContainerDataPtr pCurTabContainer = aParentTabContainer;
		if( !pView )
			continue;
		LLPanel const *pPanel = dynamic_cast< LLPanel const *>( pView );
		LLTabContainer const *pTabContainer = dynamic_cast< LLTabContainer const *>( pView );
		nd::ui::SearchableControl const *pSCtrl = dynamic_cast< nd::ui::SearchableControl const *>( pView );

		if( pTabContainer )
		{
			pCurPanelData.reset();

			pCurTabContainer = nd::prefs::TabContainerDataPtr( new nd::prefs::TabContainerData );
			pCurTabContainer->mTabContainer = const_cast< LLTabContainer *>( pTabContainer );
			pCurTabContainer->mLabel = pTabContainer->getLabel();
			pCurTabContainer->mPanel = 0;

			if( aParentPanel )
				aParentPanel->mChildPanel.push_back( pCurTabContainer );
			if( aParentTabContainer )
				aParentTabContainer->mChildPanel.push_back( pCurTabContainer );
		}
		else if( pPanel )
		{
			pCurTabContainer.reset();

			pCurPanelData = nd::prefs::PanelDataPtr( new nd::prefs::PanelData );
			pCurPanelData->mPanel = pPanel;
			pCurPanelData->mLabel = pPanel->getLabel();

			llassert_always( aParentPanel || aParentTabContainer );

			if( aParentTabContainer )
				aParentTabContainer->mChildPanel.push_back( pCurPanelData );
			else if( aParentPanel )
				aParentPanel->mChildPanel.push_back( pCurPanelData );
		}
		else if( pSCtrl && pSCtrl->getSearchText().size() )
		{
			nd::prefs::SearchableItemPtr item = nd::prefs::SearchableItemPtr( new nd::prefs::SearchableItem() );
			item->mView = pView;
			item->mCtrl = pSCtrl;

			item->mLabel = utf8str_to_wstring( pSCtrl->getSearchText() );
			LLWStringUtil::toLower( item->mLabel );

			llassert_always( aParentPanel || aParentTabContainer );

			if( aParentPanel )
				aParentPanel->mChildren.push_back( item );
			if( aParentTabContainer )
				aParentTabContainer->mChildren.push_back( item );
		}
		collectChildren( pView, pCurPanelData, pCurTabContainer );
		++itr;
	}
}

void LLFloaterPreference::collectSearchableItems()
{
	delete mSearchData;
	mSearchData = NULL;
	LLTabContainer *pRoot = getChild< LLTabContainer >( "pref core" );
	if( mFilterEdit && pRoot )
	{
		mSearchData = new nd::prefs::SearchData();

		nd::prefs::TabContainerDataPtr pRootTabcontainer = nd::prefs::TabContainerDataPtr( new nd::prefs::TabContainerData );
		pRootTabcontainer->mTabContainer = pRoot;
		pRootTabcontainer->mLabel = pRoot->getLabel();
		mSearchData->mRootTab = pRootTabcontainer;

		collectChildren( this, nd::prefs::PanelDataPtr(), pRootTabcontainer );
	}
}
// </FS:ND>
