/** 
 * @file fspanellogin.h
 * @brief Login username entry fields.
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

// Original file: llpanellogin.h

#ifndef FS_PANELLOGIN_H
#define FS_PANELLOGIN_H

#include "llpanel.h"
#include "llpointer.h"			// LLPointer<>
#include "llmediactrl.h"	// LLMediaCtrlObserver

class LLLineEditor;
class LLSLURL;
class LLCredential;

class FSPanelLogin:	
	public LLPanel,
	public LLViewerMediaObserver
{
	LOG_CLASS(FSPanelLogin);
public:
	FSPanelLogin(const LLRect &rect,
				void (*callback)(S32 option, void* user_data),
				void *callback_data);
	~FSPanelLogin();

	virtual void setFocus( BOOL b );

	static void show(const LLRect &rect,
		void (*callback)(S32 option, void* user_data), 
		void* callback_data);

	static void setFields(LLPointer<LLCredential> credential, bool from_startup = false);

	static void getFields(LLPointer<LLCredential>& credential, BOOL& remember);

	static BOOL areCredentialFieldsDirty();
	static void setLocation(const LLSLURL& slurl);
	static void autologinToLocation(const LLSLURL& slurl);
	
	/// Call when preferences that control visibility may have changed
	static void updateLocationSelectorsVisibility();

	static void closePanel();

	void setSiteIsAlive( bool alive );

	void showLoginWidgets();

	static void loadLoginPage();	
	static void giveFocus();
	static void setAlwaysRefresh(bool refresh); 
	
	// inherited from LLViewerMediaObserver
	/*virtual*/ void handleMediaEvent(LLPluginClassMedia* self, EMediaEvent event);
	static void updateServer();  // update the combo box, change the login page to the new server, clear the combo

	/// to be called from LLStartUp::setStartSLURL
	static void onUpdateStartSLURL(const LLSLURL& new_start_slurl);

	// called from prefs when initializing panel
	static bool getShowFavorites();

	static void clearPassword() { sPassword.clear(); }

private:
	void addFavoritesToStartLocation();
	void addUsersToCombo(BOOL show_server);
	void onSelectUser();
	void onModeChange(const LLSD& original_value, const LLSD& new_value);
	void onModeChangeConfirm(const LLSD& original_value, const LLSD& new_value, const LLSD& notification, const LLSD& response);
	void onSelectServer();
	void onLocationSLURL();
	void onUsernameTextChanged();

	static void onClickConnect(void*);
	static void onClickNewAccount(void*);
	static void onClickVersion(void*);
	static void onClickForgotPassword(void*);
	static void onClickHelp(void*);
	static void onPassKey(LLLineEditor* caller, void* user_data);
	static void updateServerCombo();
	static void onClickRemove(void*);
	static void onRemoveCallback(const LLSD& notification, const LLSD& response);
	static void onClickGridMgrHelp(void*);
	static void gridListChanged(bool success);
	static std::string credentialName();

private:
	void updateLoginButtons();

	void			(*mCallback)(S32 option, void *userdata);
	void*			mCallbackData;

	BOOL            mPasswordModified;
	bool			mShowFavorites;

	static FSPanelLogin* sInstance;
	static BOOL		sCapslockDidNotification;

	unsigned int mUsernameLength;
	unsigned int mPasswordLength;
	unsigned int mLocationLength;

	std::string		mPreviousUsername;
	static std::string	sPassword;
};

#endif //FS_PANELLOGIN_H
