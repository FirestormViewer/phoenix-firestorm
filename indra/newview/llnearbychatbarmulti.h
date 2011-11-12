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
#ifndef LL_LLNEARBYCHATBARMULTI_H
#define LL_LLNEARBYCHATBARMULTI_H

#include "lllayoutstack.h"
#include "llnearbychatbarbase.h"
#include "lltexteditor.h"

class LLNearbyChatBarMulti
	: public LLPanel
	, public LLNearbyChatBarBase
{
public:
	LLNearbyChatBarMulti();
	/*virtual*/ ~LLNearbyChatBarMulti() {}

	virtual BOOL postBuild();

	/*virtual*/ LLUICtrl* getChatBoxCtrl()				{ return m_pChatEditor; }
	/*virtual*/ LLWString getChatBoxText();
	/*virtual*/ void	  setChatBoxText(LLStringExplicit& text);
	/*virtual*/ void	  setChatBoxCursorToEnd()		{ m_pChatEditor->endOfDoc(); }
protected:
	void onChatBoxCommit(EChatType eChatType);

	/*virtual*/ BOOL		handleKeyHere(KEY key, MASK mask);

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
