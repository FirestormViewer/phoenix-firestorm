#ifndef LL_LLNEARBYCHATBARMULTI_H
#define LL_LLNEARBYCHATBARMULTI_H

#include "lllayoutstack.h"
#include "llnearbychatbarbase.h"
#include "lltexteditor.h"

class LLNearbyChatBarMulti
:	public LLPanel
,	public LLNearbyChatBarBase
{
public:
	// constructor for inline chat-bars (e.g. hosted in chat history window)
	LLNearbyChatBarMulti();
	~LLNearbyChatBarMulti() {}

	virtual BOOL postBuild();

protected:
	void onChatBoxCommit(EChatType eChatType);
	void onChatBoxKeystroke(LLTextEditor* pTextCtrl);

	/*virtual*/ BOOL		handleKeyHere(KEY key, MASK mask);

	/*virtual*/ LLWString	getChatBoxText();
	/*virtual*/ void		setChatBoxText(LLStringExplicit& text);

	/*
	 * Member variables
	 */
protected:
	// Controls
	LLTextEditor*						m_pChatEditor;

	// Line history support
	typedef std::vector<std::string>	line_history_t;
	line_history_t						mLineHistory;
	line_history_t::iterator			mCurrentHistoryLine;
};

#endif // LL_LLNEARBYCHATBARMULTI_H
