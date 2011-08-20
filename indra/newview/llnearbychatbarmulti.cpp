#include "llviewerprecompiledheaders.h"

#include "llagent.h"
#include "llfirstuse.h"
#include "llgesturemgr.h"
#include "llkeyboard.h"
#include "llnearbychatbarmulti.h"

LLNearbyChatBarMulti::LLNearbyChatBarMulti()
{
}

BOOL LLNearbyChatBarMulti::postBuild()
{
	m_pChatEditor = findChild<LLTextEditor>("chat_editor");

	m_pChatEditor->setCommitCallback(boost::bind(&LLNearbyChatBarMulti::onChatBoxCommit, this));
	m_pChatEditor->setKeystrokeCallback(boost::bind(&LLNearbyChatBarMulti::onChatBoxKeystroke, this, _1));
	m_pChatEditor->setFocusLostCallback(boost::bind(&onChatBoxFocusLost, m_pChatEditor));
	m_pChatEditor->setFocusReceivedCallback(boost::bind(&onChatBoxFocusReceived, m_pChatEditor));

	m_pChatEditor->setCommitOnFocusLost(FALSE);

/*
	mChatBox->setIgnoreArrowKeys( FALSE ); 
	mChatBox->setRevertOnEsc( FALSE );
	mChatBox->setIgnoreTab(TRUE);
	mChatBox->setPassDelete(TRUE);
	mChatBox->setReplaceNewlinesWithSpaces(FALSE);
	mChatBox->setEnableLineHistory(TRUE);
	mChatBox->setFont(LLViewerChat::getChatFont());
*/

	return TRUE;
}

// virtual
BOOL LLNearbyChatBarMulti::handleKeyHere(KEY key, MASK mask)
{
	BOOL handled = FALSE;

	if ( (KEY_RETURN == key) && (MASK_CONTROL == mask) )
	{
		// shout
		sendChat(CHAT_TYPE_SHOUT);
		handled = TRUE;
	}

	return handled;
}

void LLNearbyChatBarMulti::onChatBoxCommit()
{
	if (m_pChatEditor->getLength() > 0)
	{
		sendChat(CHAT_TYPE_NORMAL);
	}

	gAgent.stopTyping();
}

void LLNearbyChatBarMulti::onChatBoxKeystroke(LLTextEditor* pTextCtrl)
{
	LLFirstUse::otherAvatarChatFirst(false);

	LLWString raw_text = pTextCtrl->getWText();

	// Can't trim the end, because that will cause autocompletion
	// to eat trailing spaces that might be part of a gesture.
	LLWStringUtil::trimHead(raw_text);

	S32 length = raw_text.length();

	if( (length > 0) && (raw_text[0] != '/') )  // forward slash is used for escape (eg. emote) sequences
	{
		gAgent.startTyping();
	}
	else
	{
		gAgent.stopTyping();
	}

	/* Doesn't work -- can't tell the difference between a backspace
	   that killed the selection vs. backspace at the end of line.
	if (length > 1 
		&& text[0] == '/'
		&& key == KEY_BACKSPACE)
	{
		// the selection will already be deleted, but we need to trim
		// off the character before
		std::string new_text = raw_text.substr(0, length-1);
		self->mInputEditor->setText( new_text );
		self->mInputEditor->setCursorToEnd();
		length = length - 1;
	}
	*/

	KEY key = gKeyboard->currentKey();

	// Ignore "special" keys, like backspace, arrows, etc.
	if (length > 1 
		&& raw_text[0] == '/'
		&& key < KEY_SPECIAL)
	{
		// we're starting a gesture, attempt to autocomplete

		std::string utf8_trigger = wstring_to_utf8str(raw_text);
		std::string utf8_out_str(utf8_trigger);

		if (LLGestureMgr::instance().matchPrefix(utf8_trigger, &utf8_out_str))
		{
			std::string rest_of_match = utf8_out_str.substr(utf8_trigger.size());
			pTextCtrl->setText(utf8_trigger + rest_of_match); // keep original capitalization for user-entered part
			S32 outlength = pTextCtrl->getLength(); // in characters

			// Select to end of line, starting from the character
			// after the last one the user typed.
			pTextCtrl->setSelection(length, outlength);
		}
		else if (matchChatTypeTrigger(utf8_trigger, &utf8_out_str))
		{
			std::string rest_of_match = utf8_out_str.substr(utf8_trigger.size());
			pTextCtrl->setText(utf8_trigger + rest_of_match + " "); // keep original capitalization for user-entered part
			pTextCtrl->endOfDoc();
		}

		//llinfos << "GESTUREDEBUG " << trigger 
		//	<< " len " << length
		//	<< " outlen " << out_str.getLength()
		//	<< llendl;
	}
}
