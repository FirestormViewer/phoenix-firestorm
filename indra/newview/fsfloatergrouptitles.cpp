/**
 * @file fsfloatergrouptitles.cpp
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

#include "fsfloatergrouptitles.h"
#include "llgroupactions.h"
#include "llscrolllistctrl.h"
#include "lltrans.h"

/////////////////////////////////////////////////////
// FSGroupTitlesObserver class
//
FSGroupTitlesObserver::FSGroupTitlesObserver(const LLGroupData& group_data, FSFloaterGroupTitles* parent) :
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
FSFloaterGroupTitles::FSFloaterGroupTitles(const LLSD& key) :
	LLFloater(key)
{
	// Register observer and event listener
	LLGroupMgr::getInstance()->addObserver(this);

	// Don't use "new group" listener. The "new group" event
	// will be fired n times with n = number of groups!
	gAgent.addListener(this, "update grouptitle list");
}

FSFloaterGroupTitles::~FSFloaterGroupTitles()
{
	gAgent.removeListener(this);
	LLGroupMgr::getInstance()->removeObserver(this);

	// Clean up still registered observers
	clearObservers();
}

BOOL FSFloaterGroupTitles::postBuild()
{
	mActivateButton = getChild<LLButton>("btnActivate");
	mRefreshButton = getChild<LLButton>("btnRefresh");
	mInfoButton = getChild<LLButton>("btnInfo");
	mTitleList = getChild<LLScrollListCtrl>("title_list");

	mActivateButton->setCommitCallback(boost::bind(&FSFloaterGroupTitles::activateGroupTitle, this));
	mRefreshButton->setCommitCallback(boost::bind(&FSFloaterGroupTitles::refreshGroupTitles, this));
	mInfoButton->setCommitCallback(boost::bind(&FSFloaterGroupTitles::openGroupInfo, this));
	mTitleList->setDoubleClickCallback(boost::bind(&FSFloaterGroupTitles::activateGroupTitle, this));
	mTitleList->setCommitCallback(boost::bind(&FSFloaterGroupTitles::selectedTitleChanged, this));

	mTitleList->sortByColumn("title_sort_column", TRUE);

	refreshGroupTitles();

	return TRUE;
}

void FSFloaterGroupTitles::changed(LLGroupChange gc)
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

bool FSFloaterGroupTitles::handleEvent(LLPointer<LLOldEvents::LLEvent> event, const LLSD& userdata)
{
	if (event->desc() == "update grouptitle list")
	{
		refreshGroupTitles();
		return true;
	}
	return false;
}


void FSFloaterGroupTitles::clearObservers()
{
	for (observer_map_t::iterator it = mGroupTitleObserverMap.begin(); it != mGroupTitleObserverMap.end(); ++it)
	{
		delete it->second;
	}
	mGroupTitleObserverMap.clear();
}

void FSFloaterGroupTitles::addListItem(const LLUUID& group_id, const LLUUID& role_id, const std::string& title,
	const std::string& group_name, bool is_active, bool is_group)
{
	std::string font_style = (is_active ? "BOLD" : "NORMAL");

	LLSD item;
	item["id"] = group_id.asString() + role_id.asString(); // Only combination of group id and role id is unique!
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
	item["columns"][4]["column"] = "title_sort_column";
	item["columns"][4]["type"] = "text";
	item["columns"][4]["value"] = (is_group ? ("1_" + title) : "0");
	item["columns"][5]["column"] = "name_sort_column";
	item["columns"][5]["type"] = "text";
	item["columns"][5]["value"] = (is_group ? ("1_" + group_name) : "0");

	mTitleList->addElement(item);

	// Need to do use the selectByValue method or there would be multiple
	// selections on login.
	if (is_active)
	{
		mTitleList->selectByValue(group_id.asString() + role_id.asString());
	}
}

void FSFloaterGroupTitles::processGroupTitleResults(const LLGroupData& group_data)
{
	// Save the group name
	std::string group_name(group_data.mName);

	LLGroupMgrGroupData* gmgr_data = LLGroupMgr::getInstance()->getGroupData(group_data.mID);
	const std::vector<LLGroupTitle> group_titles = gmgr_data->mTitles;

	// Add group titles
	for (std::vector<LLGroupTitle>::const_iterator it = group_titles.begin(); it != group_titles.end(); ++it)
	{
		LLGroupTitle group_title = *it;
		bool is_active_title = (group_title.mSelected && group_data.mID == gAgent.getGroupID());
		addListItem(group_data.mID, group_title.mRoleID, group_title.mTitle, group_name, is_active_title);
	}

	mTitleList->scrollToShowSelected();

	// Remove observer
	observer_map_t::iterator found_it = mGroupTitleObserverMap.find(group_data.mID);
	if (found_it != mGroupTitleObserverMap.end())
	{
		delete found_it->second;
		mGroupTitleObserverMap.erase(found_it);
	}
}

void FSFloaterGroupTitles::activateGroupTitle()
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
			LLGroupActions::activate(group_id);
		}
	}
}

void FSFloaterGroupTitles::refreshGroupTitles()
{
	clearObservers();
	mTitleList->clearRows();

	// Add "no group"
	addListItem(LLUUID::null, LLUUID::null, getString("NoGroupTitle"), LLTrans::getString("GroupsNone"), gAgent.getGroupID().isNull(), false);

	for (std::vector<LLGroupData>::iterator it = gAgent.mGroups.begin(); it != gAgent.mGroups.end(); ++it)
	{
		LLGroupData& group_data = *it;
		FSGroupTitlesObserver* roleObserver = new FSGroupTitlesObserver(group_data, this);
		mGroupTitleObserverMap[group_data.mID] = roleObserver;
		LLGroupMgr::getInstance()->sendGroupTitlesRequest(group_data.mID);
	}
}

void FSFloaterGroupTitles::selectedTitleChanged()
{
	LLScrollListItem* selected_item = mTitleList->getFirstSelected();
	if (selected_item)
	{
		LLUUID group_id = selected_item->getColumn(mTitleList->getColumn("group_id")->mIndex)->getValue().asUUID();
		mInfoButton->setEnabled(group_id.notNull());
	}
}

void FSFloaterGroupTitles::openGroupInfo()
{
	LLScrollListItem* selected_item = mTitleList->getFirstSelected();
	if (selected_item)
	{
		LLUUID group_id = selected_item->getColumn(mTitleList->getColumn("group_id")->mIndex)->getValue().asUUID();
		LLGroupActions::show(group_id);
	}
}
