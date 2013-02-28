/**
 * @file fsmoneytracker.h
 * @brief Tip Tracker Window
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
 * Copyright (c) 2011 Arrehn Oberlander
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
 */

#ifndef FS_MONEYTRACKER_H
#define FS_MONEYTRACKER_H

#include "llfloater.h"
#include "llsingleton.h"
// <FS:Ansariel> [FS communication UI]
//#include "llchathistory.h"
#include "fschathistory.h"
// </FS:Ansariel> [FS communication UI]
#include "lllineeditor.h"
#include "llchat.h"
#include <string.h>

class LLTextBox;
class LLViewerRegion;
	
class FSMoneyTracker: public LLFloater
{
public:
	FSMoneyTracker(const LLSD& seed);
	virtual ~FSMoneyTracker();

	BOOL postBuild();
	void addMessage(const LLChat& chat,bool archive,const LLSD &args);

private:
	void clear();
	std::string appendTime();
	// <FS:Ansariel> [FS communication UI]
	//LLChatHistory* mTransactionHistory;
	FSChatHistory* mTransactionHistory;
	// </FS:Ansariel> [FS communication UI]
};

#endif // FS_MONEYTRACKER_H
