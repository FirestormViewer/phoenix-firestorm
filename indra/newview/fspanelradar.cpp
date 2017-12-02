/**
 * @file fspanelradar.cpp
 * @brief Firestorm radar panel implementation
 *
 * $LicenseInfo:firstyear=2013&license=viewerlgpl$
 * Copyright (c) 2013 Ansariel Hiller @ Second Life
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

#include "fspanelradar.h"

// libs
#include "llfiltereditor.h"
#include "llmenugl.h"
#include "lluictrlfactory.h"
#include "llmenubutton.h"
#include "lltoggleablemenu.h"

// newview
#include "fscommon.h"
#include "fsradarmenu.h"
#include "llavataractions.h"
#include "llnetmap.h"
#include "llpanelblockedlist.h"
#include "llviewercontrol.h"		// for gSavedSettings
#include "llviewermenu.h"			// for gMenuHolder
#include "rlvactions.h"
#include "rlvhandler.h"


/**
 * Update buttons on changes in our friend relations (STORM-557).
 */
class FSButtonsUpdater : public FSRadar::Updater, public LLFriendObserver
{
public:
	FSButtonsUpdater(callback_t cb)
	:	FSRadar::Updater(cb)
	{
		LLAvatarTracker::instance().addObserver(this);
	}

	~FSButtonsUpdater()
	{
		LLAvatarTracker::instance().removeObserver(this);
	}

	/*virtual*/ void changed(U32 mask)
	{
		(void) mask;
		update();
	}
};


//=============================================================================

static LLPanelInjector<FSPanelRadar> t_fs_panel_radar("fs_panel_radar");

FSPanelRadar::FSPanelRadar()
	:	LLPanel(),
		mRadarGearButton(NULL),
		mOptionsButton(NULL),
		mMiniMap(NULL),
		mFilterSubString(LLStringUtil::null),
		mFilterSubStringOrig(LLStringUtil::null),
		mRadarList(NULL),
		mVisibleCheckFunction(NULL),
		mUpdateSignalConnection(),
		mFSRadarColumnConfigConnection(),
		mLastResizeDelta(0)
{
	mButtonsUpdater = new FSButtonsUpdater(boost::bind(&FSPanelRadar::updateButtons, this));
	mCommitCallbackRegistrar.add("Radar.AddFriend",	boost::bind(&FSPanelRadar::onAddFriendButtonClicked,	this));
	mCommitCallbackRegistrar.add("Radar.Gear",		boost::bind(&FSPanelRadar::onGearButtonClicked,			this, _1));

	mColumnBits["name"] = 1;
	mColumnBits["voice_level"] = 2;
	mColumnBits["in_region"] = 4;
	mColumnBits["typing_status"] = 8;
	mColumnBits["sitting_status"] = 16;
	mColumnBits["flags"] = 32;
	mColumnBits["age"] = 64;
	mColumnBits["seen"] = 128;
	mColumnBits["range"] = 256;
	mColumnBits["has_notes"] = 512;
}

FSPanelRadar::~FSPanelRadar()
{
	if (mUpdateSignalConnection.connected())
	{
		mUpdateSignalConnection.disconnect();
	}

	if (mFSRadarColumnConfigConnection.connected())
	{
		mFSRadarColumnConfigConnection.disconnect();
	}

	delete mButtonsUpdater;

	if (mOptionsMenuHandle.get()) mOptionsMenuHandle.get()->die();
}

BOOL FSPanelRadar::postBuild()
{
	mRadarList = getChild<FSRadarListCtrl>("radar_list");
	mRadarList->setFilterColumn(0);
	mRadarList->setContextMenu(&FSFloaterRadarMenu::gFSRadarMenu);
	mRadarList->setDoubleClickCallback(boost::bind(&FSPanelRadar::onRadarListDoubleClicked, this));
	mRadarList->setCommitCallback(boost::bind(&FSPanelRadar::onRadarListCommitted, this));
	
	mMiniMap = getChild<LLNetMap>("Net Map");
	mAddFriendButton = getChild<LLButton>("add_friend_btn");

	mFilterEditor = getChild<LLFilterEditor>("nearby_filter_input");
	mFilterEditor->setCommitCallback(boost::bind(&FSPanelRadar::onFilterEdit, this, _2));

	// Create menus.
	LLUICtrl::CommitCallbackRegistry::ScopedRegistrar registrar;
	LLUICtrl::EnableCallbackRegistry::ScopedRegistrar enable_registrar;
	
	registrar.add("Radar.Option.Action",	boost::bind(&FSPanelRadar::onOptionsMenuItemClicked, this, _2));
	registrar.add("Radar.NameFmt",			boost::bind(&FSRadar::onRadarNameFmtClicked, _2));
	registrar.add("Radar.ReportTo",			boost::bind(&FSRadar::onRadarReportToClicked, _2));
	registrar.add("Radar.ToggleColumn",		boost::bind(&FSPanelRadar::onColumnVisibilityChecked, this, _2));

	enable_registrar.add("Radar.NameFmtCheck",	boost::bind(&FSRadar::radarNameFmtCheck, _2));
	enable_registrar.add("Radar.ReportToCheck",	boost::bind(&FSRadar::radarReportToCheck, _2));
	enable_registrar.add("Radar.EnableColumn",	boost::bind(&FSPanelRadar::onEnableColumnVisibilityChecked, this, _2));

	mRadarGearButton = getChild<LLButton>("gear_btn");
	mOptionsButton = getChild<LLMenuButton>("options_btn");

	LLToggleableMenu* options_menu  = LLUICtrlFactory::getInstance()->createFromFile<LLToggleableMenu>("menu_fs_radar_options.xml", gMenuHolder, LLViewerMenuHolderGL::child_registry_t::instance());
	if (options_menu)
	{
		mOptionsMenuHandle = options_menu->getHandle();
		mOptionsButton->setMenu(options_menu, LLMenuButton::MP_BOTTOM_LEFT);
	}

	mFSRadarColumnConfigConnection = gSavedSettings.getControl("FSRadarColumnConfig")->getSignal()->connect(boost::bind(&FSPanelRadar::onColumnDisplayModeChanged, this));
	onColumnDisplayModeChanged();

	// Register for radar updates
	mUpdateSignalConnection = FSRadar::getInstance()->setUpdateCallback(boost::bind(&FSPanelRadar::updateList, this, _1, _2));
	
	// call this method in case some list is empty and buttons can be in inconsistent state
	updateButtons();

	return TRUE;
}

BOOL FSPanelRadar::handleKeyHere(KEY key, MASK mask)
{
	if (FSCommon::isFilterEditorKeyCombo(key, mask))
	{
		mFilterEditor->setFocus(TRUE);
		return TRUE;
	}

	return LLPanel::handleKeyHere(key, mask);
}

void FSPanelRadar::updateButtons()
{
	LLUUID selected_id;

	uuid_vec_t selected_uuids;
	getCurrentItemIDs(selected_uuids);
	
	bool item_selected = (selected_uuids.size() == 1);

	// Check whether selected avatar is our friend.
	bool is_friend = true;
	if (item_selected)
	{
		selected_id = selected_uuids.front();
		is_friend = LLAvatarTracker::instance().getBuddyInfo(selected_id) != NULL;
	}
	mAddFriendButton->setEnabled(!is_friend && !gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES));
	mRadarGearButton->setEnabled(selected_uuids.size() > 0 && !gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES));
}

LLUUID FSPanelRadar::getCurrentItemID() const
{
	LLScrollListItem* item = mRadarList->getFirstSelected();
	if (item)
	{
		return item->getUUID();
	}

	return LLUUID::null;
}

void FSPanelRadar::getCurrentItemIDs(uuid_vec_t& selected_uuids) const
{
	std::vector<LLScrollListItem*> selected_items = mRadarList->getAllSelected();
	for (std::vector<LLScrollListItem*>::iterator it = selected_items.begin(); it != selected_items.end(); ++it)
	{
		selected_uuids.push_back((*it)->getUUID());
	}
}

void FSPanelRadar::onFilterEdit(const std::string& search_string)
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
	mRadarList->setFilterString(mFilterSubStringOrig);
}

void FSPanelRadar::onRadarListDoubleClicked()
{
	LLScrollListItem* item = mRadarList->getFirstSelected();
	if (!item)
	{
		return;
	}

	LLUUID clicked_id = item->getUUID();
	std::string name = item->getColumn(mRadarList->getColumn("name")->mIndex)->getValue().asString();

	FSRadar* radar = FSRadar::getInstance();
	if (radar)
	{
		radar->zoomAvatar(clicked_id, name);
	}
}

void FSPanelRadar::onRadarListCommitted()
{
	uuid_vec_t selected_uuids;
	LLUUID sVal = mRadarList->getSelectedValue().asUUID();
	if (sVal.notNull())
	{
		selected_uuids.push_back(sVal);
		mMiniMap->setSelected(selected_uuids);
	}

	updateButtons();
	mChangeSignal();
}

void FSPanelRadar::onAddFriendButtonClicked()
{
	LLUUID id = getCurrentItemID();
	if (id.notNull())
	{
		LLAvatarActions::requestFriendshipDialog(id);
	}
}

void FSPanelRadar::onOptionsMenuItemClicked(const LLSD& userdata)
{
	std::string chosen_item = userdata.asString();

	if (chosen_item == "panel_block_list_sidetray")
	{
		LLPanelBlockedList::showPanelAndSelect();
	}
}

void FSPanelRadar::onGearButtonClicked(LLUICtrl* btn)
{
	uuid_vec_t selected_uuids;
	getCurrentItemIDs(selected_uuids);
	// Spawn at bottom left corner of the button.
	FSFloaterRadarMenu::gFSRadarMenu.show(btn, selected_uuids, 0, 0);
}

void FSPanelRadar::requestUpdate()
{
	FSRadar* radar = FSRadar::getInstance();
	if (radar)
	{
		std::vector<LLSD> entries;
		LLSD stats;
		radar->getCurrentData(entries, stats);
		updateList(entries, stats);
	}
}

void FSPanelRadar::updateList(const std::vector<LLSD>& entries, const LLSD& stats)
{
	if (mVisibleCheckFunction && !mVisibleCheckFunction())
	{
		return;
	}

	static const std::string flagsColumnType = getString("FlagsColumnType");
	static const std::string flagsColumnValues [3] = { getString("FlagsColumnValue_0"), getString("FlagsColumnValue_1"), getString("FlagsColumnValue_2") };
	static const std::string notesColumnIcon = getString("NotesColumnIcon");
	static const std::string sittingColumnIcon = getString("SittingColumnIcon");
	static const std::string typingColumnIcon = getString("TypingColumnIcon");

	// Store current selection and scroll position
	std::vector<LLScrollListItem*> selected_items = mRadarList->getAllSelected();
	uuid_vec_t selected_ids;
	for (std::vector<LLScrollListItem*>::iterator it = selected_items.begin(); it != selected_items.end(); ++it)
	{
		selected_ids.push_back((*it)->getUUID());
	}
	S32 lastScroll = mRadarList->getScrollPos();

	// Update list
	mRadarList->setCommentText(RlvActions::canShowNearbyAgents() ? LLStringUtil::null : RlvStrings::getString("blocked_nearby"));

	bool needs_sort = mRadarList->isSorted();
	mRadarList->setNeedsSort(false);

	mRadarList->clearRows();
	const std::vector<LLSD>::const_iterator it_end = entries.end();
	for (std::vector<LLSD>::const_iterator it = entries.begin(); it != it_end; ++it)
	{
		LLSD entry = (*it)["entry"];
		LLSD options = (*it)["options"];

		LLSD row_data;
		row_data["value"] = entry["id"];
		row_data["columns"][0]["column"] = "name";
		row_data["columns"][0]["value"] = entry["name"];

		row_data["columns"][1]["column"] = "voice_level";
		row_data["columns"][1]["type"] = "icon";
		row_data["columns"][1]["value"] = ""; // Need to set it after the row has been created because it's to big for the row

		row_data["columns"][2]["column"] = "in_region";
		row_data["columns"][2]["type"] = "icon";
		if (entry["on_parcel"].asBoolean())
		{
			row_data["columns"][2]["value"] = "avatar_on_parcel";
		}
		else if (entry["in_region"].asBoolean())
		{
			row_data["columns"][2]["value"] = "avatar_in_region";
		}
		else
		{
			"";
		}

		row_data["columns"][3]["column"] = "typing_status";
		row_data["columns"][3]["type"] = "icon";
		row_data["columns"][3]["value"] = (entry["typing"].asBoolean() ? typingColumnIcon : "");

		row_data["columns"][4]["column"] = "sitting_status";
		row_data["columns"][4]["type"] = "icon";
		row_data["columns"][4]["value"] = (entry["sitting"].asBoolean() ? sittingColumnIcon : "");

		row_data["columns"][5]["column"] = "flags";
		row_data["columns"][5]["type"] = flagsColumnType;

		row_data["columns"][6]["column"] = "has_notes";
		row_data["columns"][6]["type"] = "icon";
		row_data["columns"][6]["value"] = (entry["has_notes"].asBoolean() ? notesColumnIcon : "");

		row_data["columns"][7]["column"] = "age";
		row_data["columns"][7]["value"] = entry["age"];
		row_data["columns"][7]["halign"] = "right";

		row_data["columns"][8]["column"] = "seen";
		row_data["columns"][8]["value"] = entry["seen"];
		row_data["columns"][8]["halign"] = "right";

		row_data["columns"][9]["column"] = "range";
		row_data["columns"][9]["value"] = entry["range"];

		row_data["columns"][10]["column"] = "seen_sort";
		row_data["columns"][10]["value"] = entry["seen"].asString() + "_" + entry["name"].asString();

		LLScrollListItem* row = mRadarList->addElement(row_data);

		static S32 rangeColumnIndex = mRadarList->getColumn("range")->mIndex;
		static S32 nameColumnIndex = mRadarList->getColumn("name")->mIndex;
		static S32 voiceLevelColumnIndex = mRadarList->getColumn("voice_level")->mIndex;
		static S32 flagsColumnIndex = mRadarList->getColumn("flags")->mIndex;
		static S32 ageColumnIndex = mRadarList->getColumn("age")->mIndex;

		LLScrollListText* radarRangeCell = (LLScrollListText*)row->getColumn(rangeColumnIndex);
		radarRangeCell->setColor(LLColor4(options["range_color"]));
		radarRangeCell->setFontStyle(options["range_style"].asInteger());

		LLScrollListText* radarNameCell = (LLScrollListText*)row->getColumn(nameColumnIndex);
		radarNameCell->setFontStyle(options["name_style"].asInteger());
		if (options.has("name_color"))
		{
			radarNameCell->setColor(LLColor4(options["name_color"]));
		}

		LLScrollListText* voiceLevelCell = (LLScrollListText*)row->getColumn(voiceLevelColumnIndex);
		if (entry.has("voice_level_icon"))
		{
			voiceLevelCell->setValue(entry["voice_level_icon"].asString());
		}

		LLScrollListText* flagsCell = (LLScrollListText*)row->getColumn(flagsColumnIndex);
		if (entry.has("flags"))
		{
			flagsCell->setValue(flagsColumnValues[entry["flags"].asInteger()]);
		}

		LLScrollListText* ageCell = (LLScrollListText*)row->getColumn(ageColumnIndex);
		if (options.has("age_color"))
		{
			ageCell->setColor(LLColor4(options["age_color"]));
		}
	}

	mRadarList->setNeedsSort(needs_sort);
	mRadarList->updateSort();

	LLStringUtil::format_map_t name_count_args;
	name_count_args["[TOTAL]"] = stats["total"].asString();
	name_count_args["[IN_REGION]"] = stats["region"].asString();
	name_count_args["[IN_CHAT_RANGE]"] = stats["chatrange"].asString();
	LLScrollListColumn* column = mRadarList->getColumn("name");
	column->mHeader->setLabel(getString("avatar_name_count", name_count_args));
	column->mHeader->setToolTipArgs(name_count_args);

	mRadarList->refreshLineHeight();

	// Restore scroll position
	mRadarList->setScrollPos(lastScroll);

	// Restore selection list
	if (!selected_ids.empty())
	{
		mRadarList->selectMultiple(selected_ids);
	}

	updateButtons();
	mChangeSignal();
}

void FSPanelRadar::onColumnDisplayModeChanged()
{
	U32 column_config = gSavedSettings.getU32("FSRadarColumnConfig");
	std::vector<LLScrollListColumn::Params> column_params = mRadarList->getColumnInitParams();
	S32 column_padding = mRadarList->getColumnPadding();

	LLFloater* parent_floater = NULL;
	LLView* parent = getParent();
	while (parent)
	{
		parent_floater = dynamic_cast<LLFloater*>(parent);
		if (parent_floater)
		{
			break;
		}
		parent = parent->getParent();
	}
	if (!parent_floater)
	{
		return;
	}

	S32 default_width = 0;
	S32 new_width = 0;
	S32 min_width, min_height;
	parent_floater->getResizeLimits(&min_width, &min_height);

	std::string current_sort_col = mRadarList->getSortColumnName();
	BOOL current_sort_asc = mRadarList->getSortAscending();
	
	mRadarList->clearRows();
	mRadarList->clearColumns();
	mRadarList->updateLayout();

	std::vector<LLScrollListColumn::Params>::iterator param_it;
	for (param_it = column_params.begin(); param_it != column_params.end(); ++param_it)
	{
		LLScrollListColumn::Params p = *param_it;
		default_width += (p.width.pixel_width.getValue() + column_padding);
		
		LLScrollListColumn::Params params;
		params.header = p.header;
		params.name = p.name;
		params.halign = p.halign;
		params.sort_direction = p.sort_direction;
		params.sort_column = p.sort_column;
		params.tool_tip = p.tool_tip;

		if (column_config & mColumnBits[p.name.getValue()])
		{
			params.width = p.width;
			new_width += (params.width.pixel_width.getValue() + column_padding);
		}
		else
		{
			params.width.pixel_width.set(-1, true);
		}

		mRadarList->addColumn(params);
	}

	min_width -= (default_width - new_width - mLastResizeDelta);
	mLastResizeDelta = default_width - new_width;
	parent_floater->setResizeLimits(min_width, min_height);

	if (parent_floater->getRect().getWidth() < min_width)
	{
		parent_floater->reshape(min_width, parent_floater->getRect().getHeight());
	}

	if (current_sort_col.empty() ||
		(current_sort_col != "seen_sort" && mRadarList->getColumn(current_sort_col)->getWidth() == -1) ||
		(current_sort_col == "seen_sort" && mRadarList->getColumn("seen")->getWidth() == -1))
	{
		current_sort_col = "range";
		current_sort_asc = TRUE;
	}
	mRadarList->sortByColumn(current_sort_col, current_sort_asc);
	mRadarList->setFilterColumn(0);

	mRadarList->dirtyColumns();
}

void FSPanelRadar::onColumnVisibilityChecked(const LLSD& userdata)
{
	std::string column = userdata.asString();
	U32 column_config = gSavedSettings.getU32("FSRadarColumnConfig");

	U32 new_value;
	U32 enabled = (mColumnBits[column] & column_config);
	if (enabled)
	{
		new_value = (column_config & ~mColumnBits[column]);
	}
	else
	{
		new_value = (column_config | mColumnBits[column]);
	}

	gSavedSettings.setU32("FSRadarColumnConfig", new_value);
}

bool FSPanelRadar::onEnableColumnVisibilityChecked(const LLSD& userdata)
{
	std::string column = userdata.asString();
	U32 column_config = gSavedSettings.getU32("FSRadarColumnConfig");

	return (mColumnBits[column] & column_config);
}
