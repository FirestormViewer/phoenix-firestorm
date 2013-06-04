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
#include "llfloaterreg.h"
#include "lllayoutstack.h"
#include "llmenugl.h"
#include "llnotificationsutil.h"
#include "lluictrlfactory.h"
#include "llmenubutton.h"
#include "lltoggleablemenu.h"

// newview
#include "fsradarmenu.h"
#include "llavataractions.h"
#include "llfloatersidepanelcontainer.h"
#include "llnetmap.h"
#include "llviewercontrol.h"		// for gSavedSettings
#include "llviewermenu.h"			// for gMenuHolder
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

static LLRegisterPanelClassWrapper<FSPanelRadar> t_fs_panel_radar("fs_panel_radar");

FSPanelRadar::FSPanelRadar()
	:	LLPanel(),
 		mRadarGearButton(NULL),
		mMiniMap(NULL),
		mRadarList(NULL),
		mVisibleCheckFunction(NULL)
{
	mButtonsUpdater = new FSButtonsUpdater(boost::bind(&FSPanelRadar::updateButtons, this));
	mCommitCallbackRegistrar.add("People.addFriend", boost::bind(&FSPanelRadar::onAddFriendButtonClicked, this));
}

FSPanelRadar::~FSPanelRadar()
{
	mUpdateSignalConnection.disconnect();
	delete mButtonsUpdater;

	if (mRadarGearMenuHandle.get()) mRadarGearMenuHandle.get()->die();
}

BOOL FSPanelRadar::postBuild()
{
	// AO: radarlist takes over for nearbylist for presentation.
	mRadarList = getChild<FSRadarListCtrl>("radar_list");
	mRadarList->sortByColumn("range", TRUE); // sort by range
	mRadarList->setFilterColumn(0);
	mRadarList->setContextMenu(&FSFloaterRadarMenu::gFSRadarMenu);
	mRadarList->setDoubleClickCallback(boost::bind(&FSPanelRadar::onRadarListDoubleClicked, this));
	mRadarList->setCommitCallback(boost::bind(&FSPanelRadar::onRadarListCommitted, this));
	
	mMiniMap = getChild<LLNetMap>("Net Map");
	mAddFriendButton = getChild<LLButton>("add_friend_btn_nearby");

	// Create menus.
	LLUICtrl::CommitCallbackRegistry::ScopedRegistrar registrar;
	LLUICtrl::EnableCallbackRegistry::ScopedRegistrar enable_registrar;
	
	registrar.add("Radar.Gear.Action",  boost::bind(&FSPanelRadar::onGearMenuItemClicked, this, _2));
	registrar.add("Radar.NameFmt",		boost::bind(&FSRadar::onRadarNameFmtClicked, _2));
	registrar.add("Radar.ReportTo",		boost::bind(&FSRadar::onRadarReportToClicked, _2));

	enable_registrar.add("Radar.NameFmtCheck",	boost::bind(&FSRadar::radarNameFmtCheck, _2));
	enable_registrar.add("Radar.ReportToCheck",	boost::bind(&FSRadar::radarReportToCheck, _2));

	mRadarGearButton = getChild<LLMenuButton>("nearby_view_sort_btn");

	LLToggleableMenu* radar_gear  = LLUICtrlFactory::getInstance()->createFromFile<LLToggleableMenu>("menu_fs_radar_gear.xml",  gMenuHolder, LLViewerMenuHolderGL::child_registry_t::instance());
	if (radar_gear)
	{
		mRadarGearMenuHandle = radar_gear->getHandle();
		mRadarGearButton->setMenu(radar_gear);
	}

	// Register for radar updates
	mUpdateSignalConnection = FSRadar::getInstance()->setUpdateCallback(boost::bind(&FSPanelRadar::updateList, this, _1, _2));
	
	// call this method in case some list is empty and buttons can be in inconsistent state
	updateButtons();

	return TRUE;
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
}

LLUUID FSPanelRadar::getCurrentItemID() const
{
	static S32 uuid_column_index = mRadarList->getColumn("uuid")->mIndex;

	LLScrollListItem* item = mRadarList->getFirstSelected();
	if (item)
	{
		return item->getColumn(uuid_column_index)->getValue().asUUID();
	}

	return LLUUID::null;
}

void FSPanelRadar::getCurrentItemIDs(uuid_vec_t& selected_uuids) const
{
	static S32 uuid_column_index = mRadarList->getColumn("uuid")->mIndex;

	for (size_t i = 0; i < mRadarList->getAllSelected().size(); ++i)
	{
		selected_uuids.push_back(mRadarList->getAllSelected().at(i)->getColumn(uuid_column_index)->getValue().asUUID());
	}
}

void FSPanelRadar::setFilter(const std::string& search_string)
{
	// Apply new filter.
	mRadarList->setFilterString(search_string);
}

void FSPanelRadar::onRadarListDoubleClicked()
{
	LLScrollListItem* item = mRadarList->getFirstSelected();
	if (!item)
	{
		return;
	}

	LLUUID clicked_id = item->getColumn(mRadarList->getColumn("uuid")->mIndex)->getValue().asUUID();
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

void FSPanelRadar::onGearMenuItemClicked(const LLSD& userdata)
{
	std::string chosen_item = userdata.asString();

	if (chosen_item == "panel_block_list_sidetray")
	{
		if (gSavedSettings.getBOOL("FSUseStandaloneBlocklistFloater"))
		{
			LLFloaterReg::showInstance("fs_blocklist", LLSD());
		}
		else
		{
			LLFloaterSidePanelContainer::showPanel("people", "panel_block_list_sidetray", LLSD());
		}
	}
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

	// Store current selection and scroll position
	static S32 uuidColumnIndex = mRadarList->getColumn("uuid")->mIndex;
	std::vector<LLScrollListItem*> selected_items = mRadarList->getAllSelected();
	uuid_vec_t selected_ids;
	for (size_t i = 0; i < selected_items.size(); i++)
	{
		selected_ids.push_back(selected_items.at(i)->getColumn(uuidColumnIndex)->getValue().asUUID());
	}
	S32 lastScroll = mRadarList->getScrollPos();

	// Update list
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
		row_data["columns"][2]["value"] = (entry["in_region"].asBoolean() ? "avatar_in_region" : "");
		row_data["columns"][3]["column"] = "flags";
		row_data["columns"][3]["type"] = flagsColumnType;
		row_data["columns"][4]["column"] = "age";
		row_data["columns"][4]["value"] = entry["age"];
		row_data["columns"][5]["column"] = "seen";
		row_data["columns"][5]["value"] = entry["seen"];
		row_data["columns"][6]["column"] = "range";
		row_data["columns"][6]["value"] = entry["range"];
		row_data["columns"][7]["column"] = "uuid"; // invisible column for referencing av-key the row belongs to
		row_data["columns"][7]["value"] = entry["id"];

		LLScrollListItem* row = mRadarList->addElement(row_data);

		static S32 rangeColumnIndex = mRadarList->getColumn("range")->mIndex;
		static S32 nameColumnIndex = mRadarList->getColumn("name")->mIndex;
		static S32 voiceLevelColumnIndex = mRadarList->getColumn("voice_level")->mIndex;
		static S32 flagsColumnIndex = mRadarList->getColumn("flags")->mIndex;

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
	}

	LLStringUtil::format_map_t name_count_args;
	name_count_args["[TOTAL]"] = stats["total"].asString();
	name_count_args["[IN_REGION]"] = stats["region"].asString();
	name_count_args["[IN_CHAT_RANGE]"] = stats["chatrange"].asString();
	LLScrollListColumn* column = mRadarList->getColumn("name");
	column->mHeader->setLabel(getString("avatar_name_count", name_count_args));
	column->mHeader->setToolTipArgs(name_count_args);

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
