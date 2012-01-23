/** 
 *
 * Copyright (c) 2011, Kitty Barnett
 * 
 * The source code in this file is provided to you under the terms of the 
 * GNU Lesser General Public License, version 2.1, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A 
 * PARTICULAR PURPOSE. Terms of the LGPL can be found in doc/LGPL-licence.txt 
 * in this distribution, or online at http://www.gnu.org/licenses/lgpl-2.1.txt
 * 
 * By copying, modifying or distributing this software, you acknowledge that
 * you have read and understood your obligations described above, and agree to 
 * abide by those obligations.
 * 
 */
#ifndef LL_LLNEARBYCHATBARBASE_H
#define LL_LLNEARBYCHATBARBASE_H

#include "llchat.h"
#include "lluictrl.h"

class LLNearbyChatBarBase
{
public:
	LLNearbyChatBarBase();

	static LLNearbyChatBarBase* findInstance();

	static void sendChatFromViewer(const std::string &utf8text, EChatType type, BOOL animate);
	static void sendChatFromViewer(const LLWString &wtext, EChatType type, BOOL animate);

public:
	void sendChat(EChatType type);

	virtual LLUICtrl* getChatBoxCtrl() = 0;
	virtual LLWString getChatBoxText() = 0;
	virtual void	  setChatBoxText(const LLStringExplicit& text) = 0;
	virtual void	  setChatBoxCursorToEnd() = 0;

	// <FS:ND> moved from nearbychatbar
	BOOL playChatAnimations() const
	{	return FSPlayChatAnimation; }
	// <FS:ND>

	//protected: // <FS:ND> need this for nearbychatbar
	void onChatBoxFocusLost();
	void onChatBoxFocusReceived();

protected:
	void onChatBoxKeystroke();

	static BOOL      matchChatTypeTrigger(const std::string& in_str, std::string* out_str);
	static EChatType processChatTypeTriggers(EChatType type, std::string &str);
	static LLWString stripChannelNumber(const LLWString &mesg, S32* channel);

protected:
	// Which non-zero channel did we last chat on?
	static S32 sLastSpecialChatChannel;

private:
	// <FS:ND> moved from nearbychatbar
	void updateFSPlayChatAnimation(const LLSD &data);
	BOOL			FSPlayChatAnimation;
	// </FS:ND>
};

#endif // LL_LLNEARBYCHATBARBASE_H
