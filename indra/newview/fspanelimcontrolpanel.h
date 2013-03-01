/** 
 * @file fspanelimcontrolpanel.h
 * @brief LLPanelIMControlPanel and related class definitions
 *
 * $LicenseInfo:firstyear=2004&license=viewerlgpl$
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

// Original file: // Original file: llpanelimcontrolpanel.h

#ifndef FS_PANELIMCONTROLPANEL_H
#define FS_PANELIMCONTROLPANEL_H

#include "llpanel.h"
#include "llvoicechannel.h"
#include "llcallingcard.h"

class LLParticipantList;

class FSPanelChatControlPanel 
	: public LLPanel
	, public LLVoiceClientStatusObserver
{
public:
	FSPanelChatControlPanel() :
		mSessionId(LLUUID()) {};
	~FSPanelChatControlPanel();

	virtual BOOL postBuild();

	void onCallButtonClicked();
	void onEndCallButtonClicked();
	void onOpenVoiceControlsClicked();

	// Implements LLVoiceClientStatusObserver::onChange() to enable the call
	// button when voice is available
	/*virtual*/ void onChange(EStatusType status, const std::string &channelURI, bool proximal);

	virtual void onVoiceChannelStateChanged(const LLVoiceChannel::EState& old_state, const LLVoiceChannel::EState& new_state);

	void updateButtons(LLVoiceChannel::EState state);
	
	// Enables/disables call button depending on voice availability
	void updateCallButton();

	virtual void setSessionId(const LLUUID& session_id);
	const LLUUID& getSessionId() { return mSessionId; }

private:
	LLUUID mSessionId;

	// connection to voice channel state change signal
	boost::signals2::connection mVoiceChannelStateChangeConnection;
};


class FSPanelIMControlPanel : public FSPanelChatControlPanel, LLFriendObserver
{
public:
	FSPanelIMControlPanel();
	~FSPanelIMControlPanel();

	BOOL postBuild();

	void setSessionId(const LLUUID& session_id);

	// LLFriendObserver trigger
	virtual void changed(U32 mask);

protected:
	void onNameCache(const LLUUID& id, const std::string& full_name, bool is_group);

private:
	void onViewProfileButtonClicked();
	void onAddFriendButtonClicked();
	void onShareButtonClicked();
	void onTeleportButtonClicked();
	void onPayButtonClicked();
	void onFocusReceived();

	void onClickMuteVolume();
	void onClickBlock();
	void onClickUnblock();
	/*virtual*/ void draw();
	void onVolumeChange(const LLSD& data);

	LLUUID mAvatarID;
};


class FSPanelGroupControlPanel : public FSPanelChatControlPanel
{
public:
	FSPanelGroupControlPanel(const LLUUID& session_id);
	~FSPanelGroupControlPanel();

	BOOL postBuild();

	void setSessionId(const LLUUID& session_id);
	/*virtual*/ void draw();

protected:
	LLUUID mGroupID;

	LLParticipantList* mParticipantList;

private:
	void onGroupInfoButtonClicked();
	void onSortMenuItemClicked(const LLSD& userdata);
	/*virtual*/ void onVoiceChannelStateChanged(const LLVoiceChannel::EState& old_state, const LLVoiceChannel::EState& new_state);
};

class FSPanelAdHocControlPanel : public FSPanelGroupControlPanel
{
public:
	FSPanelAdHocControlPanel(const LLUUID& session_id);

	BOOL postBuild();

};

#endif // FS_PANELIMCONTROLPANEL_H
