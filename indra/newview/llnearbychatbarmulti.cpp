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
#include "llviewerprecompiledheaders.h"

#include "llagent.h"
#include "llfirstuse.h"
#include "llgesturemgr.h"
#include "llkeyboard.h"
#include "llnearbychatbarmulti.h"

LLNearbyChatBarMulti::LLNearbyChatBarMulti()
{
	// Initialize current history line iterator
	mCurrentHistoryLine = mLineHistory.begin();
}

BOOL LLNearbyChatBarMulti::postBuild()
{
	m_pChatEditor = findChild<LLTextEditor>("chat_editor");

	m_pChatEditor->setCommitCallback(boost::bind(&LLNearbyChatBarMulti::onChatBoxCommit, this, CHAT_TYPE_NORMAL));
	m_pChatEditor->setKeystrokeCallback(boost::bind(&LLNearbyChatBarMulti::onChatBoxKeystroke, this));
	m_pChatEditor->setFocusLostCallback(boost::bind(&LLNearbyChatBarMulti::onChatBoxFocusLost, this));
	m_pChatEditor->setFocusReceivedCallback(boost::bind(&LLNearbyChatBarMulti::onChatBoxFocusReceived, this));

	m_pChatEditor->setCommitOnFocusLost(FALSE);

	return TRUE;
}

// virtual
BOOL LLNearbyChatBarMulti::handleKeyHere(KEY key, MASK mask)
{
	BOOL handled = FALSE;
	if (KEY_RETURN == key)
	{
		if (MASK_CONTROL == mask)
		{
			onChatBoxCommit(CHAT_TYPE_SHOUT);
			handled = TRUE;
		}
		else if (MASK_SHIFT == mask)
		{
			onChatBoxCommit(CHAT_TYPE_WHISPER);
			handled = TRUE;
		}
	}
	else if ( (KEY_UP == key) && (MASK_CONTROL == mask) )
	{
		if (mCurrentHistoryLine > mLineHistory.begin())
		{
			m_pChatEditor->setText(*(--mCurrentHistoryLine));
			m_pChatEditor->endOfDoc();
		}
		else
		{
			LLUI::reportBadKeystroke();
		}
		handled = TRUE;
	}
	else if ( (KEY_DOWN == key) && (MASK_CONTROL == mask) )
	{
		if ( (!mLineHistory.empty()) && (mCurrentHistoryLine < mLineHistory.end() - 1) )
		{
			m_pChatEditor->setText(*(++mCurrentHistoryLine));
			m_pChatEditor->endOfDoc();
		}
		else
		{
			LLUI::reportBadKeystroke();
		}
		handled = TRUE;
	}
	return handled;
}

void LLNearbyChatBarMulti::onChatBoxCommit(EChatType eChatType)
{
	if (m_pChatEditor->getLength() > 0)
	{
		if (!mLineHistory.empty())
		{
			// When not empty, last line of history should always be blank.
			if (mLineHistory.back().empty())
			{
				// discard the empty line
				mLineHistory.pop_back();
			}
			else
			{
				LL_WARNS("") << "Last line of history was not blank." << LL_ENDL;
			}
		}

		// Add text to history, ignoring duplicates
		if ( (mLineHistory.empty()) || (m_pChatEditor->getText() != mLineHistory.back()) )
		{
			mLineHistory.push_back(m_pChatEditor->getText());
		}

		// Restore the blank line and set mCurrentHistoryLine to point at it
		mLineHistory.push_back("");
		mCurrentHistoryLine = mLineHistory.end() - 1;

		// Send the chat
		LLNearbyChatBarBase::sendChat(eChatType);
	}

	gAgent.stopTyping();
}

LLWString LLNearbyChatBarMulti::getChatBoxText()
{
	return m_pChatEditor->getWText();
}

void LLNearbyChatBarMulti::setChatBoxText(LLStringExplicit& text)
{
	m_pChatEditor->setText(text);

	// Set current history line to end of history.
	if (mLineHistory.empty())
	{
		mCurrentHistoryLine = mLineHistory.end();
	}
	else
	{
		mCurrentHistoryLine = mLineHistory.end() - 1;
	}
}
