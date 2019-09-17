/** 
* @file   llfloatertwitter.h
* @brief  Header file for fsfloaterdiscord
* @author liny@pinkfox.xyz
*
* $LicenseInfo:firstyear=2013&license=viewerlgpl$
* Phoenix Firestorm Viewer Source Code
* Copyright (C) 2019 Liny Odell @ Second Life
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
*/
#ifndef FS_FSFLOATERDISCORD_H
#define FS_FSFLOATERDISCORD_H

#include "llfloater.h"

class LLCheckBoxCtrl;
class LLComboBox;
class LLScrollListCtrl;
class LLLineEditor;
class LLTextBox;

class FSFloaterDiscord : public LLFloater
{
public:
	FSFloaterDiscord(const LLSD& key);
	BOOL postBuild();
	void draw();
	void onClose(bool app_quitting);

private:
	void onVisibilityChange(BOOL visible);
	bool onDiscordConnectStateChange(const LLSD& data);
	bool onDiscordConnectInfoChange();
	void onConnect();
	void onDisconnect();
	void onAdd();
	void onRemove();

	void showConnectButton();
	void hideConnectButton();
	void showDisconnectedLayout();
	void showConnectedLayout();

	LLTextBox*			mAccountCaptionLabel;
	LLTextBox*			mAccountNameLabel;
	LLButton*			mConnectButton;
	LLButton*			mDisconnectButton;
	LLScrollListCtrl*	mBlacklistedNames;
	LLLineEditor*		mBlacklistEntry;
	LLTextBox*			mStatusText;
};

#endif // FS_FSFLOATERDISCORD_H
