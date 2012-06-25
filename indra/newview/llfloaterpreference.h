/** 
 * @file llfloaterpreference.h
 * @brief LLPreferenceCore class definition
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

#ifndef LL_LLFLOATERPREFERENCE_H
#define LL_LLFLOATERPREFERENCE_H

#include "llfloater.h"
#include "llavatarpropertiesprocessor.h"

class LLPanelPreference;
class LLPanelLCD;
class LLPanelDebug;
class LLMessageSystem;
class LLScrollListCtrl;
class LLSliderCtrl;
class LLSD;
class LLTextBox;
class LLComboBox;

typedef enum
	{
		GS_LOW_GRAPHICS,
		GS_MID_GRAPHICS,
		GS_HIGH_GRAPHICS,
		GS_ULTRA_GRAPHICS
		
	} EGraphicsSettings;


// Floater to control preferences (display, audio, bandwidth, general.
class LLFloaterPreference : public LLFloater, public LLAvatarPropertiesObserver
{
public: 
	LLFloaterPreference(const LLSD& key);
	~LLFloaterPreference();

	void apply();
	void cancel();

	/*virtual*/ BOOL postBuild();
	/*virtual*/ void onOpen(const LLSD& key);
	/*virtual*/	void onClose(bool app_quitting);

	// static data update, called from message handler
	static void updateUserInfo(const std::string& visibility, bool im_via_email, const std::string& email);

	// refresh all the graphics preferences menus
	static void refreshEnabledGraphics();
	
	// translate user's busy response message according to current locale if message is default, otherwise do nothing
	static void initBusyResponse();

	void processProperties( void* pData, EAvatarProcessorType type );
	void processProfileProperties(const LLAvatarData* pAvatarData );
	void storeAvatarProperties( const LLAvatarData* pAvatarData );
	void saveAvatarProperties( void );

protected:	
	void		onBtnOK();
	void		onBtnCancel();
	void		onBtnApply();

	void		onClickBrowserClearCache();		// Clear web history and caches as well as viewer caches above
	void		onLanguageChange();
	void		onNameTagOpacityChange(const LLSD& newvalue);



	// ## Zi: Pie menu
	// make sure controls get greyed out or enabled when pie color override is toggled
	void onPieColorsOverrideChanged();
	// ## Zi: Pie menu

	// set value of "BusyResponseChanged" in account settings depending on whether busy response
	// string differs from default after user changes.
	void onBusyResponseChanged();
	// if the custom settings box is clicked
	void onChangeCustom();
	void updateMeterText(LLUICtrl* ctrl);
	void onOpenHardwareSettings();
	// callback for defaults
	void setHardwareDefaults();
	// callback for when client turns on shaders
	void onVertexShaderEnable();
	// AO: callback for local lights toggle
	void onLocalLightsEnable();

	// callback for commit in the "Single click on land" and "Double click on land" comboboxes.
	void onClickActionChange();
	// updates click/double-click action settings depending on controls values
	void updateClickActionSettings();
	// updates click/double-click action controls depending on values from settings.xml
	void updateClickActionControls();
	
	// ## Zi: Optional Edit Appearance Lighting
	// make sure controls get greyed out or enabled when appearance camera movement is toggled
	void onAppearanceCameraChanged();
	// ## Zi: Optional Edit Appearance Lighting

	// This function squirrels away the current values of the controls so that
	// cancel() can restore them.	
	void saveSettings();
		

public:

	void setCacheLocation(const LLStringExplicit& location);

	void onClickSetCache();
	void onClickBrowseCache();
	void onClickBrowseCrashLogs();
	void onClickBrowseChatLogDir();
	void onClickResetCache();
	void onClickClearCache(); // AO: was protected, moved to public
	void onClickCookies();
	void onClickJavascript();
	void onClickBrowseSettingsDir();
	void onClickSkin(LLUICtrl* ctrl,const LLSD& userdata);
	void onSelectSkin();
	void onClickSetKey();
	void setKey(KEY key);
	void onClickSetMiddleMouse();
	void onClickSetSounds();
	void setPreprocInclude();
//	void onClickSkipDialogs();
//	void onClickResetDialogs();
	void onClickEnablePopup();
	void onClickDisablePopup();	
	void resetAllIgnored();
	void setAllIgnored();
	void onClickLogPath();
	//[FIX FIRE-2765 : SJ] Making sure Reset button resets works
	void onClickResetLogPath();
	void enableHistory();
	void setPersonalInfo(const std::string& visibility, bool im_via_email, const std::string& email);
	void refreshEnabledState();
	void disableUnavailableSettings();
	void onCommitWindowedMode();
	void refresh();	// Refresh enable/disable
	// if the quality radio buttons are changed
	void onChangeQuality(const LLSD& data);
	void onClickClearSettings();
	void onClickChatOnlineNotices();
	void onClickClearSpamList();
	//void callback_clear_settings(const LLSD& notification, const LLSD& response);
	
	void updateSliderText(LLSliderCtrl* ctrl, LLTextBox* text_box);
	void onUpdateSliderText(LLUICtrl* ctrl, const LLSD& name);
//	void fractionFromDecimal(F32 decimal_val, S32& numerator, S32& denominator);

	void onCommitParcelMediaAutoPlayEnable();
	void onCommitMediaEnabled();
	void onCommitMusicEnabled();
	void applyResolution();
	void onChangeMaturity();
	void onClickBlockList();
	void onClickSortContacts();
	void onClickProxySettings();
	void onClickTranslationSettings();
	void applyUIColor(LLUICtrl* ctrl, const LLSD& param);
	void getUIColor(LLUICtrl* ctrl, const LLSD& param);
	
	void buildPopupLists();
	static void refreshSkin(void* data);
private:
	static std::string sSkin;
	bool mClickActionDirty; ///< Set to true when the click/double-click options get changed by user.
	bool mGotPersonalInfo;
	bool mOriginalIMViaEmail;
	bool mLanguageChanged;
	bool mAvatarDataInitialized;
	
	bool mOriginalHideOnlineStatus;
	std::string mDirectoryVisibility;
	
	LLAvatarData mAvatarProperties;
};

class LLPanelPreference : public LLPanel
{
public:
	LLPanelPreference();
	/*virtual*/ BOOL postBuild();
	
	virtual ~LLPanelPreference();

	virtual void apply();
	virtual void cancel();
	void setControlFalse(const LLSD& user_data);
	virtual void setHardwareDefaults(){};

	// Disables "Allow Media to auto play" check box only when both
	// "Streaming Music" and "Media" are unchecked. Otherwise enables it.
	void updateMediaAutoPlayCheckbox(LLUICtrl* ctrl);

	// This function squirrels away the current values of the controls so that
	// cancel() can restore them.
	virtual void saveSettings();
	
	class Updater;

protected:
	typedef std::map<LLControlVariable*, LLSD> control_values_map_t;
	control_values_map_t mSavedValues;

private:
	//for "Only friends and groups can call or IM me"
	static void showFriendsOnlyWarning(LLUICtrl*, const LLSD&);

	static void showCustomPortWarning(LLUICtrl*, const LLSD&); // -WoLf

 	//for "Show my Favorite Landmarks at Login"
	static void showFavoritesOnLoginWarning(LLUICtrl* checkbox, const LLSD& value);

	typedef std::map<std::string, LLColor4> string_color_map_t;
	string_color_map_t mSavedColors;

	Updater* mBandWidthUpdater;
};

class LLPanelPreferenceGraphics : public LLPanelPreference
{
public:
	BOOL postBuild();
	void draw();
	void apply();
	void cancel();
	void saveSettings();
	void setHardwareDefaults();
protected:
	bool hasDirtyChilds();
	void resetDirtyChilds();
	

	LLButton*	mButtonApply;
};

// [SL:KB] - Catznip Viewer-Skins
class LLPanelPreferenceSkins : public LLPanelPreference
{
public:
	LLPanelPreferenceSkins();
	
	/*virtual*/ BOOL postBuild();
	/*virtual*/ void apply();
	/*virtual*/ void cancel();
protected:
	void onSkinChanged();
	void onSkinThemeChanged();
	void refreshSkinList();
	void refreshSkinThemeList();
	
protected:
	std::string m_Skin;
	LLComboBox* m_pSkinCombo;
	std::string m_SkinTheme;
	LLComboBox* m_pSkinThemeCombo;
	LLSD        m_SkinsInfo;
};
// [/SL:KB]

// [SL:KB] - Patch: Viewer-CrashReporting | Checked: 2010-10-21 (Catznip-2.6.0a) | Added: Catznip-2.2.0c
class LLPanelPreferenceCrashReports : public LLPanelPreference
{
public:
	LLPanelPreferenceCrashReports();

	/*virtual*/ BOOL postBuild();
	/*virtual*/ void apply();
	/*virtual*/ void cancel();

	void refresh();
};
// [/SL:KB]

#ifdef HAS_OPENSIM_SUPPORT// <FS:AW optional opensim support>
// <FS:AW  opensim preferences>
class LLPanelPreferenceOpensim : public LLPanelPreference
{
public:
	LLPanelPreferenceOpensim();
// <FS:AW  grid management>
	/*virtual*/ BOOL postBuild();
	/*virtual*/ void apply();
	/*virtual*/ void cancel();

protected:

	void onClickAddGrid();
	void addedGrid(bool success);
	void onClickClearGrid();
	void onClickRefreshGrid();
	void onClickRemoveGrid();
	bool removeGridCB(const LLSD& notification, const LLSD& response);
// </FS:AW  grid management>
// <FS:AW  opensim search support>
	void onClickClearDebugSearchURL();
	void onClickPickDebugSearchURL();
// </FS:AW  opensim search support>

	void refreshGridList(bool success = true);
	LLScrollListCtrl* mGridListControl;
};
// </FS:AW  opensim preferences>
#endif // HAS_OPENSIM_SUPPORT // <FS:AW optional opensim support>

class LLFloaterPreferenceProxy : public LLFloater
{
public: 
	LLFloaterPreferenceProxy(const LLSD& key);
	~LLFloaterPreferenceProxy();

	/// show off our menu
	static void show();
	void cancel();
	
protected:
	BOOL postBuild();
	void onOpen(const LLSD& key);
	void onClose(bool app_quitting);
	void saveSettings();
	void onBtnOk();
	void onBtnCancel();

	void onChangeSocksSettings();

private:
	
	bool mSocksSettingsDirty;
	typedef std::map<LLControlVariable*, LLSD> control_values_map_t;
	control_values_map_t mSavedValues;

};


#endif  // LL_LLPREFERENCEFLOATER_H
