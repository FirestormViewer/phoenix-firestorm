/**
 * @file fsgrouptitles.h
 * @brief Group title overview and changer
 *
 * $LicenseInfo:firstyear=2012&license=viewerlgpl$
 * Copyright (c) 2012 Ansariel Hiller @ Second Life
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
 * The Phoenix Firestorm Project, Inc., 1831 Oakwood Drive, Fairmont, Minnesota 56031-3225 USA
 * http://www.firestormviewer.org
 * $/LicenseInfo$
 */

#ifndef FS_GROUPTITLES_H
#define FS_GROUPTITLES_H

#include "llsingleton.h"

#include "llfloater.h"
#include "llscrolllistctrl.h"

#include "llagent.h"
#include "llgroupmgr.h"

class FSGroupTitles : public LLSingleton<FSGroupTitles>, public LLFloater, public LLGroupMgrObserver, public LLOldEvents::LLSimpleListener
{
public:
	FSGroupTitles(const LLSD &);
	virtual ~FSGroupTitles();

	/*virtual*/ BOOL postBuild();
	/*virtual*/ void draw();

	virtual void changed(LLGroupChange gc);
	bool handleEvent(LLPointer<LLOldEvents::LLEvent> event, const LLSD& userdata); // called on agent group list changes

	void processGroupTitleResults(const LLGroupData& group_data);

private:
	void clearObservers();

	void addListItem(const LLUUID& group_id, const LLUUID& role_id, const std::string& title,
		const std::string& group_name, bool is_active, EAddPosition position = ADD_BOTTOM);

	void refreshGroupTitles();
	void activateGroupTitle();
	void selectedTitleChanged();
	void openGroupInfo();

	LLButton*			mActivateButton;
	LLButton*			mRefreshButton;
	LLButton*			mInfoButton;
	LLScrollListCtrl*	mTitleList;

	bool	mIsUpdated;
	S32		mLastScrollPosition;

	std::map<LLUUID, void*> mGroupTitleObserverMap;
};


class FSGroupTitlesObserver : LLGroupMgrObserver
{
public:
	FSGroupTitlesObserver(const LLGroupData& group_data, FSGroupTitles* parent);
	virtual ~FSGroupTitlesObserver();

	virtual void changed(LLGroupChange gc);

protected:
	FSGroupTitles*	mParent;
	LLGroupData		mGroupData;
};

#endif // FS_GROUPTITLES_H
