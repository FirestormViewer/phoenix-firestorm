#ifndef LL_LLNEARBYCHATBARBASE_H
#define LL_LLNEARBYCHATBARBASE_H

#include "llchat.h"
#include "lluictrl.h"

class LLNearbyChatBarBase
{
public:
	static void sendChatFromViewer(const std::string &utf8text, EChatType type, BOOL animate);
	static void sendChatFromViewer(const LLWString &wtext, EChatType type, BOOL animate);

protected:
	static void onChatBoxFocusLost(LLUICtrl* pChatBoxCtrl);
	static void onChatBoxFocusReceived(LLUICtrl* pChatBoxCtrl);

	void sendChat(LLUICtrl* pChatBoxCtrl, EChatType type);
	void stopChat(LLUICtrl* pChatBoxCtrl);

	static BOOL      matchChatTypeTrigger(const std::string& in_str, std::string* out_str);
	static EChatType processChatTypeTriggers(EChatType type, std::string &str);
	static LLWString stripChannelNumber(const LLWString &mesg, S32* channel);

protected:
	virtual LLWString getChatBoxText() = 0;
	virtual void      setChatBoxText(LLStringExplicit& text) = 0;

	// Which non-zero channel did we last chat on?
	static S32 sLastSpecialChatChannel;
};

#endif // LL_LLNEARBYCHATBARBASE_H
