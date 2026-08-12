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
#include "fscommon.h"
#include "fsgrouptitleregionmgr.h"
#include "llcheckboxctrl.h"
#include "llfiltereditor.h"
#include "llgroupactions.h"
#include "llmenugl.h"
#include "llscrolllistctrl.h"
#include "lltrans.h"
#include "llviewermenu.h"

/////////////////////////////////////////////////////
// FSGroupTitlesObserver class
//
FSGroupTitlesObserver::FSGroupTitlesObserver(const LLGroupData& group_data, LLHandle<FSFloaterGroupTitles> parent) :
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
    if (gc == GC_TITLES && !mParent.isDead())
    {
        mParent.get()->processGroupTitleResults(mGroupData);
    }
}


/////////////////////////////////////////////////////
// FSGroupTitles class
//
FSFloaterGroupTitles::FSFloaterGroupTitles(const LLSD& key) :
    LLFloater(key),
    mFilterSubString(LLStringUtil::null),
    mFilterSubStringOrig(LLStringUtil::null)
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

    if (mAssignmentsChangedConnection.connected())
    {
        mAssignmentsChangedConnection.disconnect();
    }

    if (LLContextMenu* menu = mClearRegionMenuHandle.get())
    {
        gMenuHolder->removeChild(menu);
        delete menu;
    }

    // Clean up still registered observers
    clearObservers();
}

bool FSFloaterGroupTitles::postBuild()
{
    mActivateButton = getChild<LLButton>("btnActivate");
    mRefreshButton = getChild<LLButton>("btnRefresh");
    mInfoButton = getChild<LLButton>("btnInfo");
    mSetRegionButton = getChild<LLButton>("btnSetRegion");
    mSetRegionManualButton = getChild<LLButton>("btnSetRegionManual");
    mClearRegionButton = getChild<LLButton>("btnClearRegion");
    mSetDefaultButton = getChild<LLButton>("btnSetDefault");
    mUseDefaultCheck = getChild<LLCheckBoxCtrl>("use_default_title");
    mTitleList = getChild<LLScrollListCtrl>("title_list");
    mFilterEditor = getChild<LLFilterEditor>("filter_input");

    mActivateButton->setCommitCallback(boost::bind(&FSFloaterGroupTitles::activateGroupTitle, this));
    mRefreshButton->setCommitCallback(boost::bind(&FSFloaterGroupTitles::refreshGroupTitles, this));
    mInfoButton->setCommitCallback(boost::bind(&FSFloaterGroupTitles::openGroupInfo, this));
    mSetRegionButton->setCommitCallback(boost::bind(&FSFloaterGroupTitles::onSetRegion, this));
    mSetRegionManualButton->setCommitCallback(boost::bind(&FSFloaterGroupTitles::onSetRegionManual, this));
    mClearRegionButton->setCommitCallback(boost::bind(&FSFloaterGroupTitles::onClearRegion, this));
    mSetDefaultButton->setCommitCallback(boost::bind(&FSFloaterGroupTitles::onSetAsDefault, this));
    mUseDefaultCheck->setCommitCallback(boost::bind(&FSFloaterGroupTitles::onUseDefaultToggle, this));
    mTitleList->setDoubleClickCallback(boost::bind(&FSFloaterGroupTitles::activateGroupTitle, this));
    mTitleList->setCommitCallback(boost::bind(&FSFloaterGroupTitles::selectedTitleChanged, this));
    mFilterEditor->setCommitCallback(boost::bind(&FSFloaterGroupTitles::onFilterEdit, this, _2));

    mSetRegionButton->setEnabled(false);
    mSetRegionManualButton->setEnabled(false);
    mClearRegionButton->setEnabled(false);
    mSetDefaultButton->setEnabled(false);
    mUseDefaultCheck->set(FSGroupTitleRegionMgr::getInstance()->getEnabled());
    mAssignmentsChangedConnection = FSGroupTitleRegionMgr::getInstance()->setAssignmentsChangedCallback([this]() { updateRegionColumn(); });

    mTitleList->sortByColumn("title_sort_column", true);
    mTitleList->setFilterColumn(mTitleList->getColumn("filter_text")->mIndex);

    refreshGroupTitles();

    return true;
}

void FSFloaterGroupTitles::onOpen(const LLSD& key)
{
    LLFloater::onOpen(key);

    mTitleList->setFocus(true);
}

bool FSFloaterGroupTitles::handleKeyHere(KEY key, MASK mask)
{
    if (FSCommon::isFilterEditorKeyCombo(key, mask))
    {
        mFilterEditor->setFocus(true);
        return true;
    }

    return LLFloater::handleKeyHere(key, mask);
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
    std::string region_name = FSGroupTitleRegionMgr::getInstance()->getRegionForTitle(group_id, role_id);

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
    item["columns"][2]["column"] = "regionname";
    item["columns"][2]["type"] = "text";
    item["columns"][2]["font"]["style"] = font_style;
    item["columns"][2]["value"] = region_name;
    item["columns"][3]["column"] = "role_id";
    item["columns"][3]["type"] = "text";
    item["columns"][3]["value"] = role_id;
    item["columns"][4]["column"] = "group_id";
    item["columns"][4]["type"] = "text";
    item["columns"][4]["value"] = group_id;
    item["columns"][5]["column"] = "title_sort_column";
    item["columns"][5]["type"] = "text";
    item["columns"][5]["value"] = (is_group ? ("1_" + title) : "0");
    item["columns"][6]["column"] = "name_sort_column";
    item["columns"][6]["type"] = "text";
    item["columns"][6]["value"] = (is_group ? ("1_" + group_name) : "0");
    item["columns"][7]["column"] = "filter_text";
    item["columns"][7]["type"] = "text";
    item["columns"][7]["value"] = title + " " + group_name + " " + region_name;
    item["columns"][8]["column"] = "default_marker";
    item["columns"][8]["type"] = "icon";
    item["columns"][8]["halign"] = "center";
    item["columns"][8]["value"] = LLStringUtil::null;

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
    LLGroupMgrGroupData* gmgr_data = LLGroupMgr::getInstance()->getGroupData(group_data.mID);
    if (!gmgr_data || gmgr_data->mTitles.empty())
    {
        return;
    }

    std::string group_name(group_data.mName);
    const std::vector<LLGroupTitle> group_titles = gmgr_data->mTitles;

    // Add group titles
    for (std::vector<LLGroupTitle>::const_iterator it = group_titles.begin(); it != group_titles.end(); ++it)
    {
        LLGroupTitle group_title = *it;
        bool is_active_title = (group_title.mSelected && group_data.mID == gAgent.getGroupID());
        addListItem(group_data.mID, group_title.mRoleID, group_title.mTitle, group_name, is_active_title);
    }

    mTitleList->scrollToShowSelected();
    updateDefaultColumn();

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
        FSGroupTitlesObserver* roleObserver = new FSGroupTitlesObserver(group_data, getDerivedHandle<FSFloaterGroupTitles>());
        mGroupTitleObserverMap[group_data.mID] = roleObserver;
        LLGroupMgr::getInstance()->sendGroupTitlesRequest(group_data.mID);
    }

    updateDefaultColumn();
}

void FSFloaterGroupTitles::selectedTitleChanged()
{
    LLScrollListItem* selected_item = mTitleList->getFirstSelected();
    if (selected_item)
    {
        LLUUID group_id = selected_item->getColumn(mTitleList->getColumn("group_id")->mIndex)->getValue().asUUID();
        LLUUID role_id  = selected_item->getColumn(mTitleList->getColumn("role_id")->mIndex)->getValue().asUUID();
        mInfoButton->setEnabled(group_id.notNull());
        const bool has_region = !FSGroupTitleRegionMgr::getInstance()->getRegionForTitle(group_id, role_id).empty();
        mSetRegionButton->setEnabled(true);
        mSetRegionManualButton->setEnabled(true);
        mClearRegionButton->setEnabled(has_region);
        mSetDefaultButton->setEnabled(true);
    }
    else
    {
        mSetRegionButton->setEnabled(false);
        mSetRegionManualButton->setEnabled(false);
        mClearRegionButton->setEnabled(false);
        mSetDefaultButton->setEnabled(false);
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

void FSFloaterGroupTitles::onFilterEdit(const std::string& search_string)
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
    mTitleList->setFilterString(mFilterSubStringOrig);
}

void FSFloaterGroupTitles::onSetRegion()
{
    auto* selected_item = mTitleList->getFirstSelected();
    if (!selected_item)
    {
        return;
    }
    const auto group_id = selected_item->getColumn(mTitleList->getColumn("group_id")->mIndex)->getValue().asUUID();
    const auto role_id  = selected_item->getColumn(mTitleList->getColumn("role_id")->mIndex)->getValue().asUUID();
    FSGroupTitleRegionMgr::getInstance()->setAssignmentForCurrentRegion(group_id, role_id);
    selectedTitleChanged();
}

void FSFloaterGroupTitles::onSetRegionManual()
{
    auto* selected_item = mTitleList->getFirstSelected();
    if (!selected_item)
    {
        return;
    }
    const auto group_id = selected_item->getColumn(mTitleList->getColumn("group_id")->mIndex)->getValue().asUUID();
    const auto role_id  = selected_item->getColumn(mTitleList->getColumn("role_id")->mIndex)->getValue().asUUID();
    FSGroupTitleRegionMgr::getInstance()->showRegionInputDialog(group_id, role_id);
}

void FSFloaterGroupTitles::onClearRegion()
{
    auto* selected_item = mTitleList->getFirstSelected();
    if (!selected_item)
    {
        return;
    }
    const auto group_id = selected_item->getColumn(mTitleList->getColumn("group_id")->mIndex)->getValue().asUUID();
    const auto role_id  = selected_item->getColumn(mTitleList->getColumn("role_id")->mIndex)->getValue().asUUID();
    auto* mgr = FSGroupTitleRegionMgr::getInstance();
    std::vector<std::string> regions = mgr->getRegionDisplayNamesForTitle(group_id, role_id);

    if (regions.empty())
    {
        return;
    }

    if (regions.size() == 1)
    {
        mgr->clearAssignmentByRegion(regions[0]);
        selectedTitleChanged();
        return;
    }

    // Multiple regions, build a popup menu
    if (LLContextMenu* old_menu = mClearRegionMenuHandle.get())
    {
        gMenuHolder->removeChild(old_menu);
        delete old_menu;
    }

    LLContextMenu::Params menu_params;
    menu_params.name("clear_region_menu");
    menu_params.visible(false);
    LLContextMenu* menu = LLUICtrlFactory::create<LLContextMenu>(menu_params);

    for (const auto& region : regions)
    {
        LLMenuItemCallGL::Params item_params;
        item_params.label(region);
        item_params.name(region);
        item_params.on_click.function(
            [mgr, region](LLUICtrl*, const LLSD&)
            {
                mgr->clearAssignmentByRegion(region);
            });
        menu->addChild(LLUICtrlFactory::create<LLMenuItemCallGL>(item_params));
    }

    menu->addSeparator();

    LLMenuItemCallGL::Params clear_all_params;
    clear_all_params.label(getString("ClearAllRegions"));
    clear_all_params.name("clear_all_regions");
    clear_all_params.on_click.function(
        [mgr, group_id, role_id](LLUICtrl*, const LLSD&)
        {
            mgr->clearAssignment(group_id, role_id);
        });
    menu->addChild(LLUICtrlFactory::create<LLMenuItemCallGL>(clear_all_params));

    mClearRegionMenuHandle = menu->getHandle();
    gMenuHolder->addChild(menu);
    S32 screen_x, screen_y;
    mClearRegionButton->localPointToScreen(0, mClearRegionButton->getRect().getHeight(), &screen_x, &screen_y);
    menu->show(screen_x, screen_y, mClearRegionButton);
}

void FSFloaterGroupTitles::onUseDefaultToggle()
{
    FSGroupTitleRegionMgr::getInstance()->setEnabled(mUseDefaultCheck->get());
    updateDefaultColumn();
}

void FSFloaterGroupTitles::onSetAsDefault()
{
    LLScrollListItem* selected_item = mTitleList->getFirstSelected();
    if (!selected_item)
    {
        return;
    }
    const LLUUID group_id = selected_item->getColumn(mTitleList->getColumn("group_id")->mIndex)->getValue().asUUID();
    const LLUUID role_id  = selected_item->getColumn(mTitleList->getColumn("role_id")->mIndex)->getValue().asUUID();
    auto* mgr = FSGroupTitleRegionMgr::getInstance();
    mgr->setDefaultTitle(group_id, role_id);
    mgr->setEnabled(true);
    mUseDefaultCheck->set(true);
    updateDefaultColumn();
}

void FSFloaterGroupTitles::updateDefaultColumn()
{
    LLScrollListColumn* default_col = mTitleList->getColumn("default_marker");
    if (!default_col)
    {
        return;
    }

    const auto default_col_idx  = default_col->mIndex;
    const auto group_id_col_idx = mTitleList->getColumn("group_id")->mIndex;
    const auto role_id_col_idx  = mTitleList->getColumn("role_id")->mIndex;

    auto* mgr = FSGroupTitleRegionMgr::getInstance();
    const bool enabled = mgr->getEnabled();
    const LLUUID default_group = mgr->getDefaultGroupID();
    const LLUUID default_role = mgr->getDefaultRoleID();

    for (auto* item : mTitleList->getAllData())
    {
        auto* cell = item->getColumn(default_col_idx);
        if (!cell)
        {
            continue;
        }
        const LLUUID group_id = item->getColumn(group_id_col_idx)->getValue().asUUID();
        const LLUUID role_id = item->getColumn(role_id_col_idx)->getValue().asUUID();
        const bool is_default = enabled && group_id == default_group && role_id == default_role;
        cell->setValue(is_default ? LLSD("Check_Mark") : LLSD(LLStringUtil::null));
    }
}

void FSFloaterGroupTitles::updateRegionColumn()
{
    const auto region_col_idx   = mTitleList->getColumn("regionname")->mIndex;
    const auto title_col_idx    = mTitleList->getColumn("grouptitle")->mIndex;
    const auto group_col_idx    = mTitleList->getColumn("groupname")->mIndex;
    const auto filter_col_idx   = mTitleList->getColumn("filter_text")->mIndex;
    const auto group_id_col_idx = mTitleList->getColumn("group_id")->mIndex;
    const auto role_id_col_idx  = mTitleList->getColumn("role_id")->mIndex;
    auto* mgr = FSGroupTitleRegionMgr::getInstance();
    for (auto* item : mTitleList->getAllData())
    {
        const auto group_id = item->getColumn(group_id_col_idx)->getValue().asUUID();
        const auto role_id  = item->getColumn(role_id_col_idx)->getValue().asUUID();
        const std::string region_name = mgr->getRegionForTitle(group_id, role_id);

        if (auto* cell = item->getColumn(region_col_idx))
        {
            cell->setValue(region_name);
        }

        if (auto* filter_cell = item->getColumn(filter_col_idx))
        {
            const std::string title = item->getColumn(title_col_idx)->getValue().asString();
            const std::string group_name = item->getColumn(group_col_idx)->getValue().asString();
            filter_cell->setValue(title + " " + group_name + " " + region_name);
        }
    }
}
