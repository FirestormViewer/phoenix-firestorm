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
	void onChatBoxCommit();
	void onChatBoxKeystroke(LLTextEditor* pTextCtrl);

	void sendChat(EChatType type) { LLNearbyChatBarBase::sendChat(m_pChatEditor, type); }

	/*virtual*/ BOOL		handleKeyHere(KEY key, MASK mask);

	/*virtual*/ LLWString	getChatBoxText()						 { return m_pChatEditor->getWText(); }
	/*virtual*/ void		setChatBoxText(LLStringExplicit& text) { m_pChatEditor->setText(text); }

	LLTextEditor* m_pChatEditor;
};

#endif // LL_LLNEARBYCHATBARMULTI_H
