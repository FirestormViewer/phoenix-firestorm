 /** 
 * @file fsfloaternearbychat.h
 * @brief Nearby chat history scrolling panel implementation
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

// Original file: llfloaternearbychat.h

#ifndef FS_FLOATERNEARBYCHAT_H
#define FS_FLOATERNEARBYCHAT_H

#include "lldockablefloater.h"
#include "llscrollbar.h"
#include "llviewerchat.h"

class LLResizeBar;
class FSChatHistory;
#include "llchatentry.h"
//#include "llsingleton.h"
#include "lllayoutstack.h"

class FSFloaterNearbyChat: public LLDockableFloater
{
public:
	FSFloaterNearbyChat(const LLSD& key);
	~FSFloaterNearbyChat();

	BOOL	postBuild			();

	/** @param archive true - to save a message to the chat history log */
	void	addMessage			(const LLChat& message,bool archive = true, const LLSD &args = LLSD());	
	void	onNearbyChatContextMenuItemClicked(const LLSD& userdata);
	bool	onNearbyChatCheckContextMenuItem(const LLSD& userdata);

	void	onChatBarVisibilityChanged();
	void	onChatChannelVisibilityChanged();

	/*virtual*/ void	onOpen	(const LLSD& key);

	/*virtual*/ void	setVisible(BOOL visible);
	void	openFloater(const LLSD& key);

	virtual void setRect		(const LLRect &rect);

	void clearChatHistory();
	virtual void updateChatHistoryStyle();

	static void processChatHistoryStyleUpdate(const LLSD& newvalue);

	void loadHistory();
	void reloadMessages(bool clean_messages = false);

	static FSFloaterNearbyChat* getInstance();
	
	void removeScreenChat();
	
	static bool isChatMultiTab();
	
	BOOL getVisible();

	static void onHistoryButtonClicked(LLUICtrl* ctrl, void* userdata);

	// overridden to fix the multitab focus bug -Zi
	BOOL focusFirstItem(BOOL prefer_text_fields = FALSE, BOOL focus_flash = TRUE );

	void updateFSUseNearbyChatConsole(const LLSD &data);
	static bool isWordsName(const std::string& name);

	void enableTranslationButton(bool enabled);
	LLChatEntry* getChatBox() { return mInputEditor; }
	
	virtual BOOL handleKeyHere( KEY key, MASK mask );
	
	static void startChat(const char* line);
	static void stopChat();
	
	static void sendChatFromViewer(const std::string &utf8text, EChatType type, BOOL animate);
	static void sendChatFromViewer(const LLWString &wtext, EChatType type, BOOL animate);
	
protected:
	static BOOL matchChatTypeTrigger(const std::string& in_str, std::string* out_str);
	void onChatBoxKeystroke();
	void onChatBoxFocusLost();
	void onChatBoxFocusReceived();
	
	void sendChat( EChatType type );
	void onChatBoxCommit();
	
	static LLWString stripChannelNumber(const LLWString &mesg, S32* channel);
	EChatType processChatTypeTriggers(EChatType type, std::string &str);
	void reshapeFloater(bool collapse);
	void reshapeChatLayoutPanel();
	
	static S32 sLastSpecialChatChannel;

private:
	void	getAllowedRect		(LLRect& rect);

	void	onNearbySpeakers	();

private:
	LLHandle<LLView>	mPopupMenuHandle;
	FSChatHistory*		mChatHistory;
	// <FS:Ansariel> Optional muted chat history
	FSChatHistory*		mChatHistoryMuted;
	LLChatEntry*		mInputEditor;
	LLLayoutPanel*		mChatLayoutPanel;
	LLLayoutStack*		mInputPanels;
	
	S32 mInputEditorPad;
	S32 mChatLayoutPanelHeight;
	S32 mFloaterHeight;

	std::vector<LLChat> mMessageArchive;

	BOOL FSUseNearbyChatConsole;
};

#endif // FS_FLOATERNEARBYCHAT_H
