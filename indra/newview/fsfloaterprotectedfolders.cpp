/**
 * @file fsfloaterprotectedfolders.cpp
 * @brief Class for the protected folders floater
 *
 * $LicenseInfo:firstyear=2020&license=viewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (c) 2020 Ansariel Hiller @ Second Life
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

#include "fsfloaterprotectedfolders.h"
#include "fscommon.h"
#include "llbutton.h"
#include "llfiltereditor.h"
#include "llinventoryfunctions.h"
#include "llinventorymodel.h"
#include "llscrolllistctrl.h"
#include "llviewercontrol.h"		// for gSavedSettings
#include "rlvactions.h"


FSFloaterProtectedFolders::FSFloaterProtectedFolders(const LLSD& key)
	: LLFloater(key),
	mFolderList(NULL),
	mFilterSubString(LLStringUtil::null),
	mFilterSubStringOrig(LLStringUtil::null),
	mProtectedCategoriesChangedCallbackConnection(),
	mInitialized(false)
{
}

FSFloaterProtectedFolders::~FSFloaterProtectedFolders()
{
	if (mProtectedCategoriesChangedCallbackConnection.connected())
	{
		mProtectedCategoriesChangedCallbackConnection.disconnect();
	}
}

//virtual
BOOL FSFloaterProtectedFolders::postBuild()
{
	mFolderList = getChild<LLScrollListCtrl>("folder_list");
	mFolderList->setFilterColumn(0);
	mFolderList->setDoubleClickCallback(boost::bind(&FSFloaterProtectedFolders::onDoubleClick, this));

	mRemoveFolderBtn = getChild<LLButton>("remove_btn");
	mRemoveFolderBtn->setCommitCallback(boost::bind(&FSFloaterProtectedFolders::handleRemove, this));

	mFilterEditor = getChild<LLFilterEditor>("filter_input");
	mFilterEditor->setCommitCallback(boost::bind(&FSFloaterProtectedFolders::onFilterEdit, this, _2));

	return TRUE;
}

//virtual
void FSFloaterProtectedFolders::onOpen(const LLSD& /*info*/)
{
	if (!mInitialized)
	{
		if (!gInventory.isInventoryUsable())
		{
			return;
		}

		mProtectedCategoriesChangedCallbackConnection = gSavedPerAccountSettings.getControl("FSProtectedFolders")->getCommitSignal()->connect(boost::bind(&FSFloaterProtectedFolders::updateList, this));

		updateList();

		mInitialized = true;
	}
}

//virtual
void FSFloaterProtectedFolders::draw()
{
	LLFloater::draw();

	mRemoveFolderBtn->setEnabled(mFolderList->getNumSelected() > 0);
}

//virtual
BOOL FSFloaterProtectedFolders::handleKeyHere(KEY key, MASK mask)
{
	if (FSCommon::isFilterEditorKeyCombo(key, mask))
	{
		mFilterEditor->setFocus(TRUE);
		return TRUE;
	}

	return LLFloater::handleKeyHere(key, mask);
}

void FSFloaterProtectedFolders::updateList()
{
	bool needs_sort = mFolderList->isSorted();
	mFolderList->setNeedsSort(false);
	mFolderList->clearRows();

	LLSD protected_folders = gSavedPerAccountSettings.getLLSD("FSProtectedFolders");
	for (LLSD::array_const_iterator it = protected_folders.beginArray(); it != protected_folders.endArray(); ++it)
	{
		LLUUID id = (*it).asUUID();
		LLViewerInventoryCategory* cat = gInventory.getCategory(id);

		LLSD row_data;
		row_data["value"] = id;
		row_data["columns"][0]["column"] = "name";
		row_data["columns"][0]["value"] = cat ? cat->getName() : getString("UnknownFolder");

		LLScrollListItem* row = mFolderList->addElement(row_data);
		if (!cat)
		{
			LLScrollListText* name_column = (LLScrollListText*)row->getColumn(0);
			name_column->setFontStyle(LLFontGL::NORMAL | LLFontGL::ITALIC);
		}
	}

	mFolderList->setNeedsSort(needs_sort);
	mFolderList->updateSort();
}

void FSFloaterProtectedFolders::handleRemove()
{
	uuid_set_t selected_ids;
	std::vector<LLScrollListItem*> selected_items = mFolderList->getAllSelected();

	for (std::vector<LLScrollListItem*>::iterator it = selected_items.begin(); it != selected_items.end(); ++it)
	{
		selected_ids.insert((*it)->getUUID());
	}

	LLSD protected_folders = gSavedPerAccountSettings.getLLSD("FSProtectedFolders");
	LLSD new_protected_folders;
	for (LLSD::array_const_iterator it = protected_folders.beginArray(); it != protected_folders.endArray(); ++it)
	{
		if (selected_ids.find((*it).asUUID()) == selected_ids.end())
		{
			new_protected_folders.append(*it);
		}
	}
	gSavedPerAccountSettings.setLLSD("FSProtectedFolders", new_protected_folders);
}

void FSFloaterProtectedFolders::onFilterEdit(const std::string& search_string)
{
	mFilterSubStringOrig = search_string;
	LLStringUtil::trimHead(mFilterSubStringOrig);
	// Searches are case-insensitive
	std::string search_upper = mFilterSubStringOrig;
	LLStringUtil::toUpper(search_upper);

	if (mFilterSubString == search_upper)
	{
		return;
	}

	mFilterSubString = search_upper;

	// Apply new filter.
	mFolderList->setFilterString(mFilterSubStringOrig);
}

void FSFloaterProtectedFolders::onDoubleClick()
{
	LLUUID selected_item_id = mFolderList->getStringUUIDSelectedItem();
	if (selected_item_id.notNull() && (!RlvActions::isRlvEnabled() || !RlvActions::hasBehaviour(RLV_BHVR_SHOWINV)))
	{
		show_item_original(selected_item_id);
	}
}
