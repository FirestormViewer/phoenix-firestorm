/**
 * @file fsfloaterassetblacklist.cpp
 * @brief Floater for Asset Blacklist and Derender
 *
 * $LicenseInfo:firstyear=2012&license=fsviewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (C) 2012, Wolfspirit Magic
 * Copyright (C) 2016, Ansariel Hiller
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

#include "fsfloaterassetblacklist.h"

#include "fsscrolllistctrl.h"
#include "fsassetblacklist.h"
#include "llfiltereditor.h"
#include "llfloaterreg.h"
#include "llviewercontrol.h"


FSFloaterAssetBlacklist::FSFloaterAssetBlacklist(const LLSD& key)
 : LLFloater(key),
   mResultList(NULL),
   mFilterSubString(LLStringUtil::null),
   mFilterSubStringOrig(LLStringUtil::null)
{
}

FSFloaterAssetBlacklist::~FSFloaterAssetBlacklist()
{
}

BOOL FSFloaterAssetBlacklist::postBuild()
{
	mResultList = getChild<FSScrollListCtrl>("result_list");
	mResultList->setContextMenu(&FSFloaterAssetBlacklistMenu::gFSAssetBlacklistMenu);
	mResultList->setFilterColumn(0);

	childSetAction("remove_btn", boost::bind(&FSFloaterAssetBlacklist::onRemoveBtn, this));
	childSetAction("close_btn", boost::bind(&FSFloaterAssetBlacklist::onCloseBtn, this));

	getChild<LLFilterEditor>("filter_input")->setCommitCallback(boost::bind(&FSFloaterAssetBlacklist::onFilterEdit, this, _2));

	return TRUE;
}

void FSFloaterAssetBlacklist::onOpen(const LLSD& key)
{
	mResultList->clearRows();
	buildBlacklist();
}

std::string FSFloaterAssetBlacklist::getTypeString(S32 type)
{
	switch (type)
	{
		case 0:
			return getString("asset_texture");
		case 1:
			return getString("asset_sound");
		case 6:
			return getString("asset_object");
		case 45:
			return getString("asset_resident");
		default:
			return getString("asset_unknown");
	}
}

void FSFloaterAssetBlacklist::buildBlacklist()
{
	bool needs_sort = mResultList->isSorted();
	mResultList->setNeedsSort(false);

	blacklist_data_t data = FSAssetBlacklist::instance().getBlacklistData();
	
	for (blacklist_data_t::const_iterator iterator = data.begin(); iterator != data.end(); ++iterator)
	{
		addElementToList(iterator->first, iterator->second);
	}

	mResultList->setNeedsSort(needs_sort);
	mResultList->updateSort();
}

void FSFloaterAssetBlacklist::addElementToList(const LLUUID& id, const LLSD& data)
{
	// Undo the persisted date in legacy format...
	std::string asset_date = data["asset_date"].asString() + "Z";
	asset_date.replace(asset_date.find(" "), 1, "T");
	LLDate date(asset_date);
	
	std::string date_str = getString("DateFormatString");
	LLSD substitution;
	substitution["datetime"] = date.secondsSinceEpoch();
	LLStringUtil::format(date_str, substitution);

	LLSD element;
	element["id"] = id;
	element["columns"][0]["column"] = "name";
	element["columns"][0]["type"] = "text";
	element["columns"][0]["value"] = !data["asset_name"].asString().empty() ? data["asset_name"].asString() : getString("unknown_object");
	element["columns"][1]["column"] = "region";
	element["columns"][1]["type"] = "text";
	element["columns"][1]["value"] = !data["asset_region"].asString().empty() ? data["asset_region"].asString() : getString("unknown_region");
	element["columns"][2]["column"] = "type";
	element["columns"][2]["type"] = "text";
	element["columns"][2]["value"] = getTypeString(data["asset_type"].asInteger());
	element["columns"][3]["column"] = "date";
	element["columns"][3]["type"] = "text";
	element["columns"][3]["value"] = date_str;
	element["columns"][4]["column"] = "permanent";
	element["columns"][4]["type"] = "text";
	element["columns"][4]["halign"] = "center";
	element["columns"][4]["value"] = data["asset_permanent"].asBoolean() ? getString("asset_permanent") : LLStringUtil::null;
	element["columns"][5]["column"] = "date_sort";
	element["columns"][5]["type"] = "text";
	element["columns"][5]["value"] = llformat("%u", (U64)date.secondsSinceEpoch());

	mResultList->addElement(element, ADD_BOTTOM);
}

void FSFloaterAssetBlacklist::removeElements()
{
	std::vector<LLScrollListItem*> list = mResultList->getAllSelected();

	FSAssetBlacklist& instance = FSAssetBlacklist::instance();
	for (std::vector<LLScrollListItem*>::const_iterator it = list.begin(); it != list.end(); ++it)
	{
		instance.removeItemFromBlacklist((*it)->getUUID());
	}
	
	mResultList->deleteSelectedItems();
}

void FSFloaterAssetBlacklist::onRemoveBtn()
{
	removeElements();
}

void FSFloaterAssetBlacklist::onCloseBtn()
{
	closeFloater();
}

void FSFloaterAssetBlacklist::onFilterEdit(const std::string& search_string)
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
	mResultList->setFilterString(mFilterSubStringOrig);
}

//---------------------------------------------------------------------------
// Context menu
//---------------------------------------------------------------------------
namespace FSFloaterAssetBlacklistMenu
{
	LLContextMenu* FSAssetBlacklistMenu::createMenu()
	{
		// set up the callbacks for all of the avatar menu items
		LLUICtrl::CommitCallbackRegistry::ScopedRegistrar registrar;

		registrar.add("Blacklist.Remove", boost::bind(&FSAssetBlacklistMenu::onContextMenuItemClick, this, _2));

		// create the context menu from the XUI
		return createFromFile("menu_fs_asset_blacklist.xml");
	}

	void FSAssetBlacklistMenu::onContextMenuItemClick(const LLSD& param)
	{
		std::string command = param.asString();

		if (command == "remove")
		{
			FSFloaterAssetBlacklist* floater = LLFloaterReg::findTypedInstance<FSFloaterAssetBlacklist>("fs_asset_blacklist");
			if (floater)
			{
				floater->removeElements();
			}
		}
	}

	FSAssetBlacklistMenu gFSAssetBlacklistMenu;
}
