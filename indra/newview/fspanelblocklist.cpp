/** 
 * @file fspanelblocklist.cpp
 * @brief Container for blocked Residents & Objects list
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 * Copyright (c) 2014 Ansariel Hiller
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
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

// Original file: llpanelblockedlist.cpp

#include "llviewerprecompiledheaders.h"

#include "fspanelblocklist.h"

#include "fscommon.h"
#include "fsblocklistmenu.h"
#include "fsscrolllistctrl.h"
#include "llavataractions.h"
#include "llavatarname.h"
#include "llfiltereditor.h"
#include "llfloater.h"
#include "llfloateravatarpicker.h"
#include "llfloaterreg.h"
#include "llfloatersidepanelcontainer.h"
#include "llnotificationsutil.h"
#include "llpanelblockedlist.h"
#include "llviewercontrol.h"


static LLPanelInjector<FSPanelBlockList> t_panel_blocked_list("fs_panel_block_list_sidetray");

//
// Constants
//
const std::string BLOCKED_PARAM_NAME = "blocked_to_select";

//-----------------------------------------------------------------------------
// FSPanelBlockedList()
//-----------------------------------------------------------------------------

FSPanelBlockList::FSPanelBlockList()
:	LLPanel(),
	mFilterSubString(LLStringUtil::null),
	mFilterSubStringOrig(LLStringUtil::null)
{
	mCommitCallbackRegistrar.add("Block.Action",	boost::bind(&FSPanelBlockList::onCustomAction,  this, _2));
	mEnableCallbackRegistrar.add("Block.Check",		boost::bind(&FSPanelBlockList::isActionChecked, this, _2));
	mEnableCallbackRegistrar.add("Block.Enable",	boost::bind(&FSPanelBlockList::isActionEnabled, this, _2));
}

void FSPanelBlockList::removePicker()
{
	if (mAvatarPicker.get())
	{
		mAvatarPicker.get()->closeFloater();
	}
	if (mObjectPicker.get())
	{
		mObjectPicker.get()->closeFloater();
	}
}

FSPanelBlockList::~FSPanelBlockList()
{
	LLMuteList::getInstance()->removeObserver(this);
}

BOOL FSPanelBlockList::postBuild()
{
	mBlockedList = getChild<FSScrollListCtrl>("block_list");
	mBlockedList->setCommitOnSelectionChange(TRUE);
	mBlockedList->setCommitCallback(boost::bind(&FSPanelBlockList::onSelectionChanged, this));
	mBlockedList->setDoubleClickCallback(boost::bind(&FSPanelBlockList::showProfile, this));
	mBlockedList->setSearchColumn(mBlockedList->getColumn("item_name")->mIndex);
	mBlockedList->setContextMenu(&gFSBlockListMenu);
	mBlockedList->setFilterColumn(0);
	mBlockedList->setSortChangedCallback(boost::bind(&FSPanelBlockList::onSortChanged, this));

	getChild<LLButton>("unblock_btn")->setCommitCallback(boost::bind(&FSPanelBlockList::removeMutes, this));
	getChild<LLFilterEditor>("blocked_filter_input")->setCommitCallback(boost::bind(&FSPanelBlockList::onFilterEdit, this, _2));

	LLMuteList::getInstance()->addObserver(this);
	
	refreshBlockedList();

	setVisibleCallback(boost::bind(&FSPanelBlockList::removePicker, this));
	
	updateButtons();

	return LLPanel::postBuild();
}

void FSPanelBlockList::onOpen(const LLSD& key)
{
	if (key.has(BLOCKED_PARAM_NAME) && key[BLOCKED_PARAM_NAME].asUUID().notNull())
	{
		selectBlocked(key[BLOCKED_PARAM_NAME].asUUID());
	}
}

BOOL FSPanelBlockList::handleKeyHere(KEY key, MASK mask)
{
	if (FSCommon::isFilterEditorKeyCombo(key, mask))
	{
		getChild<LLFilterEditor>("blocked_filter_input")->setFocus(TRUE);
		return TRUE;
	}

	return LLPanel::handleKeyHere(key, mask);
}

void FSPanelBlockList::selectBlocked(const LLUUID& mute_id)
{
	mBlockedList->deselectAllItems();
	std::vector<LLScrollListItem*> items = mBlockedList->getAllData();
	for (std::vector<LLScrollListItem*>::iterator it = items.begin(); it != items.end(); it++)
	{
		if ((*it)->getColumn(3)->getValue().asUUID() == mute_id)
		{
			(*it)->setSelected(TRUE);
			break;
		}
	}

	mBlockedList->scrollToShowSelected();
}

void FSPanelBlockList::showPanelAndSelect(const LLUUID& idToSelect)
{
	if (gSavedSettings.getBOOL("FSDisableBlockListAutoOpen"))
	{
		return;
	}

	if (gSavedSettings.getBOOL("FSUseStandaloneBlocklistFloater"))
	{
		LLFloaterReg::showInstance("fs_blocklist", LLSD().with(BLOCKED_PARAM_NAME, idToSelect));
	}
	else
	{
		LLFloaterSidePanelContainer::showPanel("people", "panel_people",
			LLSD().with("people_panel_tab_name", "blocked_panel").with(BLOCKED_PARAM_NAME, idToSelect));
	}
}


//////////////////////////////////////////////////////////////////////////
// Private Section
//////////////////////////////////////////////////////////////////////////
void FSPanelBlockList::refreshBlockedList()
{
	mBlockedList->deleteAllItems();

	std::vector<LLMute> mutes = LLMuteList::getInstance()->getMutes();
	std::vector<LLMute>::iterator it;
	for (it = mutes.begin(); it != mutes.end(); ++it)
	{
		LLScrollListItem::Params item_p;
		item_p.enabled(TRUE);
		item_p.value(LLUUID::generateNewID()); // can't link UUID of blocked item directly because of mutes by name
		item_p.columns.add().column("item_name").value(it->mName);
		item_p.columns.add().column("item_type").value(it->getDisplayType());
		item_p.columns.add().column("item_mute_type").value(it->mType);
		item_p.columns.add().column("item_mute_uuid").value(it->mID);

		mBlockedList->addRow(item_p, ADD_BOTTOM);
	}
	mBlockedList->refreshLineHeight();

	LLUICtrl* block_limit = getChild<LLUICtrl>("block_limit");
	block_limit->setTextArg("[COUNT]", llformat("%d", LLMuteList::getInstance()->getMutes().size()));
	block_limit->setTextArg("[LIMIT]", llformat("%d", gSavedSettings.getS32("MuteListLimit")));
}

void FSPanelBlockList::updateButtons()
{
	bool has_selection = mBlockedList->getNumSelected() > 0;
	getChildView("blocked_gear_btn")->setEnabled(has_selection);
	getChildView("unblock_btn")->setEnabled(has_selection);
}

void FSPanelBlockList::removeMutes()
{
	S32 scroll_pos = mBlockedList->getScrollPos();
	S32 last_selected = mBlockedList->getFirstSelectedIndex();

	// Remove observer before bulk operation or it would refresh the
	// list after each removal, sending us straight into a crash!
	LLMuteList::getInstance()->removeObserver(this);

	std::vector<LLScrollListItem*> selected_items = mBlockedList->getAllSelected();
	for (std::vector<LLScrollListItem*>::iterator it = selected_items.begin(); it != selected_items.end(); it++)
	{
		std::string name = (*it)->getColumn(0)->getValue().asString();
		LLUUID id = (*it)->getColumn(3)->getValue().asUUID();
		LLMute mute(id, name);
		LLMuteList::getInstance()->remove(mute);
	}

	LLMuteList::getInstance()->addObserver(this);
	refreshBlockedList();

	if (last_selected == mBlockedList->getItemCount())
	{
		// we were on the last item, so select the last item again
		mBlockedList->selectNthItem(last_selected - 1);
	}
	else
	{
		// else select the item after the last item previously selected
		mBlockedList->selectNthItem(last_selected);
	}
	onSelectionChanged();
	mBlockedList->setScrollPos(scroll_pos);
}

void FSPanelBlockList::onCustomAction(const LLSD& userdata)
{
	const std::string command_name = userdata.asString();

	if ("block_obj_by_name" == command_name)
	{
		blockObjectByName();
	}
	else if ("block_res_by_name" == command_name)
	{
		blockResidentByName();
	}
	else if ("sort_by_name" == command_name)
	{
		mBlockedList->sortByColumn("item_name", TRUE);
		gSavedSettings.setU32("BlockPeopleSortOrder", E_SORT_BY_NAME_ASC);
	}
	else if ("sort_by_type" == command_name)
	{
		mBlockedList->sortByColumn("item_type", TRUE);
		gSavedSettings.setU32("BlockPeopleSortOrder", E_SORT_BY_TYPE_ASC);
	}
	else if ("sort_by_name_desc" == command_name)
	{
		mBlockedList->sortByColumn("item_name", FALSE);
		gSavedSettings.setU32("BlockPeopleSortOrder", E_SORT_BY_NAME_DESC);
	}
	else if ("sort_by_type_desc" == command_name)
	{
		mBlockedList->sortByColumn("item_type", FALSE);
		gSavedSettings.setU32("BlockPeopleSortOrder", E_SORT_BY_TYPE_DESC);
	}
	else if ("unblock_item" == command_name)
	{
		removeMutes();
	}
	else if ("profile_item" == command_name)
	{
		showProfile();
	}
}

bool FSPanelBlockList::isActionChecked(const LLSD& userdata)
{
	std::string item = userdata.asString();
	U32 sort_order = gSavedSettings.getU32("BlockPeopleSortOrder");

	if ("sort_by_name" == item)
	{
		return E_SORT_BY_NAME_ASC == sort_order;
	}
	else if ("sort_by_type" == item)
	{
		return E_SORT_BY_TYPE_ASC == sort_order;
	}
	else if ("sort_by_name_desc" == item)
	{
		return E_SORT_BY_NAME_DESC == sort_order;
	}
	else if ("sort_by_type_desc" == item)
	{
		return E_SORT_BY_TYPE_DESC == sort_order;
	}

	return false;
}

bool FSPanelBlockList::isActionEnabled(const LLSD& userdata)
{
	std::string item = userdata.asString();
	if ("unblock_item" == item)
	{
		return (mBlockedList->getNumSelected() > 0);
	}
	else if ("profile_item" == item)
	{
		return (mBlockedList->getNumSelected() == 1 &&
				(LLMute::EType)mBlockedList->getFirstSelected()->getColumn(2)->getValue().asInteger() == LLMute::AGENT);
	}

	return false;
}


void FSPanelBlockList::blockResidentByName()
{
	const BOOL allow_multiple = FALSE;
	const BOOL close_on_select = TRUE;

	LLFloaterAvatarPicker* picker = LLFloaterAvatarPicker::show(boost::bind(&FSPanelBlockList::callbackBlockPicked, this, _1, _2), allow_multiple, close_on_select);
	LLFloater* parent = dynamic_cast<LLFloater*>(getParent());
	if (parent)
	{
		parent->addDependentFloater(picker);
	}
	mAvatarPicker = picker->getHandle();
}

void FSPanelBlockList::blockObjectByName()
{
	LLFloaterGetBlockedObjectName* picker = LLFloaterGetBlockedObjectName::show(boost::bind(&FSPanelBlockList::callbackBlockByName, this, _1));
	LLFloater* parent = dynamic_cast<LLFloater*>(getParent());
	if (parent)
	{
		parent->addDependentFloater(picker);
	}
	mObjectPicker = picker->getHandle();
}

void FSPanelBlockList::onSelectionChanged()
{
	updateButtons();
}

void FSPanelBlockList::showProfile()
{
	if (mBlockedList->getNumSelected() == 1 &&
		(LLMute::EType)mBlockedList->getFirstSelected()->getColumn(2)->getValue().asInteger() == LLMute::AGENT)
	{
		LLAvatarActions::showProfile(mBlockedList->getFirstSelected()->getColumn(3)->getValue().asUUID());
	}
}

void FSPanelBlockList::callbackBlockPicked(const uuid_vec_t& ids, const std::vector<LLAvatarName> names)
{
	if (names.empty() || ids.empty()) return;
	LLMute mute(ids[0], names[0].getLegacyName(), LLMute::AGENT);
	LLMuteList::getInstance()->add(mute);
	showPanelAndSelect(mute.mID);
}

void FSPanelBlockList::callbackBlockByName(const std::string& text)
{
	if (text.empty()) return;

	LLMute mute(LLUUID::null, text, LLMute::BY_NAME);
	BOOL success = LLMuteList::getInstance()->add(mute);
	if (!success)
	{
		LLNotificationsUtil::add("MuteByNameFailed");
	}
	else
	{
		mBlockedList->selectItemByLabel(text);
		mBlockedList->scrollToShowSelected();
	}
}

void FSPanelBlockList::onFilterEdit(std::string search_string)
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
	mBlockedList->setFilterString(mFilterSubStringOrig);
}

void FSPanelBlockList::onSortChanged()
{
	BOOL ascending = mBlockedList->getSortAscending();
	std::string column = mBlockedList->getSortColumnName();

	if (column == "item_name")
	{
		if (ascending)
		{
			gSavedSettings.setU32("BlockPeopleSortOrder", E_SORT_BY_NAME_ASC);
		}
		else
		{
			gSavedSettings.setU32("BlockPeopleSortOrder", E_SORT_BY_NAME_DESC);
		}
	}
	else if (column == "item_type")
	{
		if (ascending)
		{
			gSavedSettings.setU32("BlockPeopleSortOrder", E_SORT_BY_TYPE_ASC);
		}
		else
		{
			gSavedSettings.setU32("BlockPeopleSortOrder", E_SORT_BY_TYPE_DESC);
		}
	}
}

//EOF
