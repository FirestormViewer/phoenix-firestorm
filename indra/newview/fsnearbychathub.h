 /** 
 * @file llnearbychat.h
 * @brief Nearby chat central class for handling multiple chat input controls
 *
 * $LicenseInfo:firstyear=2004&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 * Copyright (C) 2012, Zi Ree @ Second Life
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

#ifndef FS_NEARBYCHAT_H
#define FS_NEARBYCHAT_H

#include "llsingleton.h"
#include "llviewerchat.h"

class FSNearbyChatControl;

class FSNearbyChat : public LLSingleton<FSNearbyChat>
{
	friend class LLSingleton<FSNearbyChat>;

private:
	FSNearbyChat();
	~FSNearbyChat();

	void sendMsg();

	static S32 sLastSpecialChatChannel;
	FSNearbyChatControl* mDefaultChatBar;

	void onDefaultChatBarButtonClicked();

public:
	void registerChatBar(FSNearbyChatControl* chatBar);

	// set the contents of the chat bar to "text" if it was empty, otherwise just show it
	void showDefaultChatBar(BOOL visible,const char* text=0) const;

	void sendChat(LLWString text,EChatType type);
	LLWString stripChannelNumber(const LLWString &mesg, S32* channel);
	EChatType processChatTypeTriggers(EChatType type, std::string &str);
	void sendChatFromViewer(const std::string& utf8text, EChatType type, BOOL animate);
	void sendChatFromViewer(const LLWString& wtext, EChatType type, BOOL animate);

	void setFocusedInputEditor(FSNearbyChatControl* inputEditor,BOOL focus);

	BOOL defaultChatBarIsIdle() const;
	BOOL defaultChatBarHasFocus() const;

	FSNearbyChatControl* mFocusedInputEditor;
};

#endif // FS_NEARBYCHAT_H
