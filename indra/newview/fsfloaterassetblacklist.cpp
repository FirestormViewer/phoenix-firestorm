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

#include "fscommon.h"
#include "fsscrolllistctrl.h"
#include "llagent.h"
#include "llaudioengine.h"
#include "llfiltereditor.h"
#include "llfloaterreg.h"
#include "llviewercontrol.h"
#include "llviewerobjectlist.h"


FSFloaterAssetBlacklist::FSFloaterAssetBlacklist(const LLSD& key)
 : LLFloater(key), LLEventTimer(0.25f),
   mResultList(nullptr),
   mFilterSubString(LLStringUtil::null),
   mFilterSubStringOrig(LLStringUtil::null),
   mAudioSourceID(LLUUID::null),
   mBlacklistCallbackConnection()
{
}

FSFloaterAssetBlacklist::~FSFloaterAssetBlacklist()
{
    if (mBlacklistCallbackConnection.connected())
    {
        mBlacklistCallbackConnection.disconnect();
    }
}

bool FSFloaterAssetBlacklist::postBuild()
{
    mResultList = getChild<FSScrollListCtrl>("result_list");
    mResultList->setContextMenu(&FSFloaterAssetBlacklistMenu::gFSAssetBlacklistMenu);
    mResultList->setFilterColumn(0);
    mResultList->setCommitCallback(boost::bind(&FSFloaterAssetBlacklist::onSelectionChanged, this));
    mResultList->setCommitOnSelectionChange(true);

    childSetAction("remove_btn", boost::bind(&FSFloaterAssetBlacklist::onRemoveBtn, this));
    childSetAction("remove_temp_btn", boost::bind(&FSFloaterAssetBlacklist::onRemoveAllTemporaryBtn, this));
    childSetAction("play_btn", boost::bind(&FSFloaterAssetBlacklist::onPlayBtn, this));
    childSetAction("stop_btn", boost::bind(&FSFloaterAssetBlacklist::onStopBtn, this));
    childSetAction("close_btn", boost::bind(&FSFloaterAssetBlacklist::onCloseBtn, this));

    getChild<LLFilterEditor>("filter_input")->setCommitCallback(boost::bind(&FSFloaterAssetBlacklist::onFilterEdit, this, _2));

    mBlacklistCallbackConnection = FSAssetBlacklist::getInstance()->setBlacklistChangedCallback(boost::bind(&FSFloaterAssetBlacklist::onBlacklistChanged, this, _1, _2));

    childSetEnabled("play_btn", false);
    childSetEnabled("stop_btn", true);
    childSetVisible("play_btn", true);
    childSetVisible("stop_btn", false);

    return true;
}

void FSFloaterAssetBlacklist::onOpen(const LLSD& key)
{
    mResultList->clearRows();
    buildBlacklist();
}

FSAssetBlacklist::eBlacklistFlag FSFloaterAssetBlacklist::getFlagFromLLSD(const LLSD& data)
{
    if (data.has("asset_blacklist_flag"))
    {
        return static_cast<FSAssetBlacklist::eBlacklistFlag>(data["asset_blacklist_flag"].asInteger());
    }
    return FSAssetBlacklist::eBlacklistFlag::NONE;
}

std::string FSFloaterAssetBlacklist::getTypeString(S32 type) const
{
    switch (type)
    {
        case LLAssetType::AT_TEXTURE:
            return getString("asset_texture");
        case LLAssetType::AT_OBJECT:
            return getString("asset_object");
        case LLAssetType::AT_ANIMATION:
            return getString("asset_animation");
        case LLAssetType::AT_PERSON:
            return getString("asset_resident");
        case LLAssetType::AT_SOUND:
            return getString("asset_sound");
        default:
            return getString("asset_unknown");
    }
}

std::string FSFloaterAssetBlacklist::getFlagString(FSAssetBlacklist::eBlacklistFlag flag) const
{
    switch (flag)
    {
        case FSAssetBlacklist::eBlacklistFlag::NONE:
            return getString("blacklist_flag_none");
        case FSAssetBlacklist::eBlacklistFlag::WORN:
            return getString("blacklist_flag_mute_avatar_worn_objects_sounds");
        case FSAssetBlacklist::eBlacklistFlag::REZZED:
            return getString("blacklist_flag_mute_avatar_rezzed_objects_sounds");
        case FSAssetBlacklist::eBlacklistFlag::GESTURE:
            return getString("blacklist_flag_mute_avatar_gestures_sounds");
        default:
            return getString("blacklist_flag_unknown");
    }
}

void FSFloaterAssetBlacklist::buildBlacklist()
{
    bool needs_sort = mResultList->isSorted();
    mResultList->setNeedsSort(false);

    for (const auto& [id, data] : FSAssetBlacklist::instance().getBlacklistData())
    {
        addElementToList(id, data);
    }

    mResultList->setNeedsSort(needs_sort);
    mResultList->updateSort();
}

void FSFloaterAssetBlacklist::addElementToList(const LLUUID& id, const FSAssetBlacklistData& data)
{
    std::string date_str = getString("DateFormatString");
    LLSD substitution;
    substitution["datetime"] = data.date.secondsSinceEpoch();
    LLStringUtil::format(date_str, substitution);

    const S32 last_flag_value = static_cast<S32>(FSAssetBlacklist::eBlacklistFlag::LAST_FLAG);

    for (S32 flag_value = 1; flag_value <= last_flag_value; flag_value <<= 1)
    {
        if ((data.flags & flag_value) || data.flags == FSAssetBlacklist::eBlacklistFlag::NONE)
        {
            FSAssetBlacklist::eBlacklistFlag flag = static_cast<FSAssetBlacklist::eBlacklistFlag>(flag_value);

            if (data.flags == FSAssetBlacklist::eBlacklistFlag::NONE)
                flag = FSAssetBlacklist::eBlacklistFlag::NONE;

            LLSD element;
            element["id"] = id;
            element["columns"][0]["column"] = "name";
            element["columns"][0]["type"] = "text";
            element["columns"][0]["value"] = !data.name.empty() ? data.name : getString("unknown_object");

            element["columns"][1]["column"] = "region";
            element["columns"][1]["type"] = "text";
            element["columns"][1]["value"] = !data.region.empty() ? data.region : getString("unknown_region");

            element["columns"][2]["column"] = "type";
            element["columns"][2]["type"] = "text";
            element["columns"][2]["value"]  = getTypeString(data.type);

            element["columns"][3]["column"] = "flags";
            element["columns"][3]["type"]   = "text";
            element["columns"][3]["value"]  = getFlagString(flag);

            element["columns"][4]["column"] = "date";
            element["columns"][4]["type"] = "text";
            element["columns"][4]["value"] = date_str;

            element["columns"][5]["column"] = "permanent";
            element["columns"][5]["type"] = "text";
            element["columns"][5]["halign"] = "center";
            element["columns"][5]["value"] = data.permanent ? getString("asset_permanent") : LLStringUtil::null;

            element["columns"][6]["column"] = "date_sort";
            element["columns"][6]["type"] = "text";
            element["columns"][6]["value"] = llformat("%u", (U64)data.date.secondsSinceEpoch());

            element["columns"][7]["column"] = "asset_type";
            element["columns"][7]["type"] = "integer";
            element["columns"][7]["value"] = (S32)data.type;

            LLSD value;
            value["flag"] = static_cast<S32>(flag);
            element["alt_value"] = value;

            mResultList->addElement(element, ADD_BOTTOM);

            if (data.flags == FSAssetBlacklist::eBlacklistFlag::NONE)
                break;
        }
    }
}

void FSFloaterAssetBlacklist::removeElements()
{
    std::map<LLUUID, S32> flags_to_remove_by_id;

    for (auto listitem : mResultList->getAllSelected())
    {
        LLUUID id = listitem->getUUID();
        LLSD value = listitem->getAltValue();

        if (value.has("flag"))
        {
            S32 flag = value["flag"].asInteger();
            flags_to_remove_by_id[id] |= flag;
        }
        else
        {
            // fallback: remove full item
            FSAssetBlacklist::instance().removeItemsFromBlacklist({ id });
        }
    }

    for (const auto& [id, flags] : flags_to_remove_by_id)
    {
        FSAssetBlacklist::instance().removeFlagsFromItem(id, flags);
    }
}

void FSFloaterAssetBlacklist::onBlacklistChanged(const FSAssetBlacklist::changed_signal_data_t& data, FSAssetBlacklist::eBlacklistOperation op)
{
    if (op == FSAssetBlacklist::eBlacklistOperation::BLACKLIST_ADD)
    {
        bool need_sort = mResultList->isSorted();
        mResultList->setNeedsSort(false);

        for (const auto& [id, data] : data)
        {
            mResultList->deleteItems(id);
            if (data.has_value())
            {
                addElementToList(id, data.value());
            }
        }

        mResultList->setNeedsSort(need_sort);
        mResultList->updateSort();
    }
    else
    {
        for (const auto& [id, data] : data)
        {
            mResultList->deleteItems(id);
        }
        mResultList->updateLayout();
    }
}

void FSFloaterAssetBlacklist::onRemoveBtn()
{
    removeElements();
}

void FSFloaterAssetBlacklist::onRemoveAllTemporaryBtn()
{
    gObjectList.resetDerenderList(true);
}

void FSFloaterAssetBlacklist::onSelectionChanged()
{
    bool enabled = false;
    if (size_t num_selected = mResultList->getAllSelected().size(); num_selected == 1)
    {
        const LLScrollListItem* item = mResultList->getFirstSelected();
        S32 name_column = mResultList->getColumn("asset_type")->mIndex;

        if (item && item->getColumn(name_column)->getValue().asInteger() == LLAssetType::AT_SOUND)
        {
            enabled = true;
        }
    }

    childSetEnabled("play_btn", enabled);
}

void FSFloaterAssetBlacklist::onPlayBtn()
{
    const LLScrollListItem* item = mResultList->getFirstSelected();
    S32 name_column = mResultList->getColumn("asset_type")->mIndex;

    if (!item || item->getUUID().isNull() || item->getColumn(name_column)->getValue().asInteger() != LLAssetType::AT_SOUND)
    {
        return;
    }

    onStopBtn();

    mAudioSourceID = LLUUID::generateNewID();
    gAudiop->triggerSound(item->getUUID(), gAgentID, 1.0f, LLAudioEngine::AUDIO_TYPE_UI, LLVector3d::zero, LLUUID::null, mAudioSourceID);

    childSetVisible("stop_btn", true);
    childSetVisible("play_btn", false);
}

void FSFloaterAssetBlacklist::onStopBtn()
{
    if (mAudioSourceID.isNull())
    {
        return;
    }

    if (LLAudioSource* audio_source = gAudiop->findAudioSource(mAudioSourceID); audio_source && !audio_source->isDone())
    {
        audio_source->play(LLUUID::null);
    }
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
    onSelectionChanged();
}

bool FSFloaterAssetBlacklist::handleKeyHere(KEY key, MASK mask)
{
    if (FSCommon::isFilterEditorKeyCombo(key, mask))
    {
        getChild<LLFilterEditor>("filter_input")->setFocus(true);
        return true;
    }

    return LLFloater::handleKeyHere(key, mask);
}

bool FSFloaterAssetBlacklist::tick()
{
    if (mAudioSourceID.isNull())
    {
        return false;
    }

    if (LLAudioSource* audio_source = gAudiop->findAudioSource(mAudioSourceID); !audio_source || audio_source->isDone())
    {
        childSetVisible("play_btn", true);
        childSetVisible("stop_btn", false);

        mAudioSourceID.setNull();
        onSelectionChanged();
    }

    return false;
}

void FSFloaterAssetBlacklist::closeFloater(bool /* app_quitting */)
{
    onStopBtn();
    LLFloater::closeFloater();
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
            if (FSFloaterAssetBlacklist* floater = LLFloaterReg::findTypedInstance<FSFloaterAssetBlacklist>("fs_asset_blacklist"); floater)
            {
                floater->removeElements();
            }
        }
    }

    FSAssetBlacklistMenu gFSAssetBlacklistMenu;
}
