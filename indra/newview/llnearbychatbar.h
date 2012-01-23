/** 
 * @file llnearbychatbar.h
 * @brief LLNearbyChatBar class definition
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

#ifndef LL_LLNEARBYCHATBAR_H
#define LL_LLNEARBYCHATBAR_H

#include "llfloater.h"
#include "llcombobox.h"
#include "llgesturemgr.h"
#include "llchat.h"
#include "llvoiceclient.h"
#include "lloutputmonitorctrl.h"
#include "llspeakers.h"
// [SL:KB] - Patch: Chat-NearbyChatBar | Checked: 2011-08-20 (Catznip-3.2.0a)
#include "llnearbychatbarbase.h"

class LLNearbyChat;
// [/SL:KB]

// [SL:KB] - Patch: Chat-NearbyChatBar | Checked: 2011-10-26 (Catznip-3.2.0a) | Added: Catznip-3.2.0a
class LLNearbyChatBarSingle 
	: public LLPanel
	, public LLNearbyChatBarBase
{
public:
	LLNearbyChatBarSingle();
	/*virtual*/ ~LLNearbyChatBarSingle() {}

public:
	/*virtual*/ void draw();
	/*virtual*/ BOOL postBuild();
protected:
	void displaySpeakingIndicator();
	void onChatBoxCommit();
	void onChatFontChange(LLFontGL* fontp);

	// LLNearbyChatBarBase overrides
public:
	/*virtual*/ LLUICtrl* getChatBoxCtrl()								{ return mChatBox; }
	/*virtual*/ LLWString getChatBoxText()								{ return mChatBox->getConvertedText(); }
	/*virtual*/ void      setChatBoxText(const LLStringExplicit& text)	{ mChatBox->setText(text); }
	/*virtual*/ void	  setChatBoxCursorToEnd()						{ mChatBox->setCursorToEnd(); }

protected:
	LLLineEditor*		 mChatBox;
	LLOutputMonitorCtrl* mOutputMonitor;
	LLLocalSpeakerMgr*	 mSpeakerMgr;
};
// [/SL:KB]

class LLNearbyChatBar :	public LLFloater
{
public:
	// constructor for inline chat-bars (e.g. hosted in chat history window)
	LLNearbyChatBar(const LLSD& key);
	~LLNearbyChatBar() {}

	virtual BOOL postBuild();

	static LLNearbyChatBar* getInstance();

//	LLLineEditor* getChatBox() { return mChatBox; }
// [SL:KB] - Patch: Chat-NearbyChatBar | Checked: 2011-10-26 (Catznip-3.2.0a) | Added: Catznip-3.2.0a
	LLNearbyChatBarBase* getChatBarImpl() const { return mChatBarImpl; }
// [/SL:KB]

//	virtual void draw();

	std::string getCurrentChat();
	virtual BOOL handleKeyHere( KEY key, MASK mask );

	static void startChat(const char* line);
	static void stopChat();

//	static void sendChatFromViewer(const std::string &utf8text, EChatType type, BOOL animate);
//	static void sendChatFromViewer(const LLWString &wtext, EChatType type, BOOL animate);
	
	// <AO>, moved to public so we can relay from other textentries.
	void onChatBoxFocusLost()
	{ mChatBarImpl->onChatBoxFocusLost(); }
    void onChatBoxFocusReceived()
	{ mChatBarImpl->onChatBoxFocusReceived(); }

    void onChatBoxCommit();
	// </AO>
	void setText(const LLStringExplicit &new_text);

	void showHistory();
	void enableTranslationCheckbox(BOOL enable);
//	/*virtual*/void setMinimized(BOOL b);
// [SL:KB] - Patch: Chat-NearbyChatBar | Checked: 2011-11-17 (Catznip-3.2.0a) | Added: Catznip-3.2.0a
	/*virtual*/ BOOL canClose();
	/*virtual*/ void onOpen(const LLSD& sdKey);
// [/SL:KB]

	void sendChat( EChatType type )
	{ mChatBarImpl->sendChat( type ); }

protected:
//	static BOOL matchChatTypeTrigger(const std::string& in_str, std::string* out_str);
//	static void onChatBoxKeystroke(LLLineEditor* caller, void* userdata);
//	static void onChatBoxFocusLost(LLFocusableElement* caller, void* userdata);
//	void onChatBoxFocusReceived();
// [SL:KB] - Patch: Chat-NearbyChatBar | Checked: 2011-11-12 (Catznip-3.2.0a) | Added: Catznip-3.2.0a
	bool onNewNearbyChatMsg(const LLSD& sdEvent);
	void onTearOff(const LLSD& sdData);
// [/SL:KB]

	//void onChatBoxCommit(); // moved to public
//	void onChatFontChange(LLFontGL* fontp);

	/* virtual */ bool applyRectControl();

	void onToggleNearbyChatPanel();

//	static LLWString stripChannelNumber(const LLWString &mesg, S32* channel);
//	EChatType processChatTypeTriggers(EChatType type, std::string &str);

//	void displaySpeakingIndicator();

// [SL:KB] - Patch: Chat-NearbyChatBar | Checked: 2011-10-26 (Catznip-3.2.0a) | Added: Catznip-3.2.0a
public:
	static const std::string&	getFloaterXMLFile();
	static bool					isTabbedNearbyChat();
	static void					processFloaterTypeChanged();
protected:
	static void* createChatBarSingle(void*);
	static void* createChatBarMulti(void*);
// [/SL:KB]

//	// Which non-zero channel did we last chat on?
//	static S32 sLastSpecialChatChannel;
//
//	LLLineEditor*		mChatBox;
//	LLOutputMonitorCtrl* mOutputMonitor;
//	LLLocalSpeakerMgr*  mSpeakerMgr;
// [SL:KB] - Patch: Chat-NearbyChatBar | Checked: 2011-10-26 (Catznip-3.2.0a) | Added: Catznip-3.2.0a
	LLPanel*			 mNearbyChatContainer;		// "panel_nearby_chat" is the parent panel containing "nearby_chat"
	LLNearbyChat*		 mNearbyChat;				// "nearby_chat"
	LLNearbyChatBarBase* mChatBarImpl;
// [/SL:KB]

	S32 mExpandedHeight;
};

#endif
