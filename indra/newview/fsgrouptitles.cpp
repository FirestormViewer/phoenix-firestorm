/**
 * @file fsgrouptitles.cpp
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

#include "llviewerprecompiledheaders.h"

#include "fsgrouptitles.h"
#include "llgroupactions.h"
#include "llscrolllistitem.h"
#include "llviewermessage.h"
#include "lltrans.h"


/////////////////////////////////////////////////////
// FSGroupTitlesObserver class
//
FSGroupTitlesObserver::FSGroupTitlesObserver(const LLGroupData& group_data, FSGroupTitles* parent) :
	LLGroupMgrObserver(group_data.mID),
	mGroupData(group_data),
	mParent(parent)
{
	LLGroupMgr::instance().addObserver(this);
}

FSGroupTitlesObserver::~FSGroupTitlesObserver()
{
	LLGroupMgr::instance().removeObserver(this);
}

void FSGroupTitlesObserver::changed(LLGroupChange gc)
{
	if (mParent)
	{
		mParent->processGroupTitleResults(mGroupData);
	}
}


/////////////////////////////////////////////////////
// FSGroupTitles class
//
FSGroupTitles::FSGroupTitles(const LLSD& key) :  
	LLFloater(key),
	mIsUpdated(false),
	mLastScrollPosition(0)
{
	// Register observer and event listener
	LLGroupMgr::getInstance()->addObserver(this);

	// Don't use "new group" listener. The "new group" event
	// will be fired n times with n = number of groups!
	gAgent.addListener(this, "update grouptitle list");
}

FSGroupTitles::~FSGroupTitles()
{
	gAgent.removeListener(this);
	LLGroupMgr::getInstance()->removeObserver(this);

	// Clean up still registered observers
	clearObservers();
}

BOOL FSGroupTitles::postBuild()
{
	mActivateButton = getChild<LLButton>("btnActivate");
	mRefreshButton = getChild<LLButton>("btnRefresh");
	mInfoButton = getChild<LLButton>("btnInfo");
	mTitleList = getChild<LLScrollListCtrl>("title_list");

	mActivateButton->setCommitCallback(boost::bind(&FSGroupTitles::activateGroupTitle, this));
	mRefreshButton->setCommitCallback(boost::bind(&FSGroupTitles::refreshGroupTitles, this));
	mInfoButton->setCommitCallback(boost::bind(&FSGroupTitles::openGroupInfo, this));
	mTitleList->setDoubleClickCallback(boost::bind(&FSGroupTitles::activateGroupTitle, this));
	mTitleList->setCommitCallback(boost::bind(&FSGroupTitles::selectedTitleChanged, this));

	mTitleList->sortByColumn("grouptitle", TRUE);

	refreshGroupTitles();

	return TRUE;
}

void FSGroupTitles::draw()
{
	if (mIsUpdated && mGroupTitleObserverMap.size() == 0)
	{
		mIsUpdated = false;
		mTitleList->updateSort();

		// Add "no group"
		bool is_active_group = (LLUUID::null == gAgent.getGroupID());
		addListItem(LLUUID::null, LLUUID::null, getString("NoGroupTitle"), LLTrans::getString("GroupsNone"), is_active_group, ADD_TOP);

		mTitleList->setScrollPos(mLastScrollPosition);
	}

	LLFloater::draw();
}

void FSGroupTitles::changed(LLGroupChange gc)
{
	switch (gc)
	{
		case GC_MEMBER_DATA:
		case GC_ROLE_MEMBER_DATA:
		case GC_TITLES:
			refreshGroupTitles();
			break;
		default:
			;
	}
}

bool FSGroupTitles::handleEvent(LLPointer<LLOldEvents::LLEvent> event, const LLSD& userdata)
{
	if (event->desc() == "update grouptitle list")
	{
		refreshGroupTitles();
		return true;
	}
	return false;
}


void FSGroupTitles::clearObservers()
{
	std::map<LLUUID, void*>::iterator it;
	while (!mGroupTitleObserverMap.empty())
	{
		it = mGroupTitleObserverMap.begin();
		FSGroupTitlesObserver* observer = (FSGroupTitlesObserver*)it->second;
		delete observer;
		observer = NULL;
		mGroupTitleObserverMap.erase(it);
	}
	mGroupTitleObserverMap.clear();
}

void FSGroupTitles::addListItem(const LLUUID& group_id, const LLUUID& role_id, const std::string& title,
	const std::string& group_name, bool is_active, EAddPosition position)
{
	std::string font_style = (is_active ? "BOLD" : "NORMAL");

	LLSD item;
	item["id"] = role_id;
	item["columns"][0]["column"] = "grouptitle";
	item["columns"][0]["type"] = "text";
	item["columns"][0]["font"]["style"] = font_style;
	item["columns"][0]["value"] = title;
	item["columns"][1]["column"] = "groupname";
	item["columns"][1]["type"] = "text";
	item["columns"][1]["font"]["style"] = font_style;
	item["columns"][1]["value"] = group_name;
	item["columns"][2]["column"] = "role_id";
	item["columns"][2]["type"] = "text";
	item["columns"][2]["value"] = role_id;
	item["columns"][3]["column"] = "group_id";
	item["columns"][3]["type"] = "text";
	item["columns"][3]["value"] = group_id;

	LLScrollListItem* list_item = mTitleList->addElement(item, position);

	if (is_active)
	{
		list_item->setSelected(TRUE);
	}
}

void FSGroupTitles::processGroupTitleResults(const LLGroupData& group_data)
{
	// Save the group name
	std::string group_name(group_data.mName);

	LLGroupMgrGroupData* gmgr_data = LLGroupMgr::getInstance()->getGroupData(group_data.mID);
	const std::vector<LLGroupTitle> group_titles = gmgr_data->mTitles;

	// Add group titles
	for (std::vector<LLGroupTitle>::const_iterator it = group_titles.begin();
		it != group_titles.end(); it++)
	{
		bool is_active_title = ((*it).mSelected && group_data.mID == gAgent.getGroupID());
		addListItem(group_data.mID, (*it).mRoleID, (*it).mTitle, group_name, is_active_title);
	}

	// Remove observer
	std::map<LLUUID, void*>::iterator it = mGroupTitleObserverMap.find(group_data.mID);
	if (it != mGroupTitleObserverMap.end() && it->second != NULL)
	{
		FSGroupTitlesObserver* observer = (FSGroupTitlesObserver*)it->second;
		delete observer;
		observer = NULL;
		mGroupTitleObserverMap.erase(it);
	}
}

void FSGroupTitles::activateGroupTitle()
{
	LLScrollListItem* selected_item = mTitleList->getFirstSelected();
	if (selected_item)
	{
		LLUUID group_id = selected_item->getColumn(mTitleList->getColumn("group_id")->mIndex)->getValue().asUUID();
		LLUUID role_id = selected_item->getColumn(mTitleList->getColumn("role_id")->mIndex)->getValue().asUUID();

		// Don't update group title for role if "no group" is going to be activated
		if (group_id.notNull())
		{
			LLGroupMgr::getInstance()->sendGroupTitleUpdate(group_id, role_id);
		}

		// Send activate group request only if group is different from current group
		if (gAgent.getGroupID() != group_id)
		{
			// Copied from LLGroupActions::activate()
			LLMessageSystem* msg = gMessageSystem;
			msg->newMessageFast(_PREHASH_ActivateGroup);
			msg->nextBlockFast(_PREHASH_AgentData);
			msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
			msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
			msg->addUUIDFast(_PREHASH_GroupID, group_id);
			gAgent.sendReliableMessage();
		}
	}
}

void FSGroupTitles::refreshGroupTitles()
{
	mIsUpdated = false;
	clearObservers();
	mLastScrollPosition = mTitleList->getScrollPos();
	mTitleList->clearRows();

	for (S32 i = 0; i < gAgent.mGroups.count(); i++)
	{
		LLGroupData group_data = gAgent.mGroups.get(i);
		FSGroupTitlesObserver* roleObserver = new FSGroupTitlesObserver(group_data, this);
		mGroupTitleObserverMap[group_data.mID] = roleObserver;
		LLGroupMgr::getInstance()->sendGroupTitlesRequest(group_data.mID);
	}
	mIsUpdated = true;
}

void FSGroupTitles::selectedTitleChanged()
{
	LLScrollListItem* selected_item = mTitleList->getFirstSelected();
	if (selected_item)
	{
		LLUUID group_id = selected_item->getColumn(mTitleList->getColumn("group_id")->mIndex)->getValue().asUUID();
		mInfoButton->setEnabled(group_id.notNull());
	}
}

void FSGroupTitles::openGroupInfo()
{
	LLScrollListItem* selected_item = mTitleList->getFirstSelected();
	if (selected_item)
	{
		LLUUID group_id = selected_item->getColumn(mTitleList->getColumn("group_id")->mIndex)->getValue().asUUID();
		LLGroupActions::show(group_id);
	}
}
