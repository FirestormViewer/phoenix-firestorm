/**
 * @file llagentwearables.cpp
 * @brief LLAgentWearables class implementation
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
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

#include "llviewerprecompiledheaders.h"
#include "llagentwearables.h"

#include "llattachmentsmgr.h"
#include "llaccordionctrltab.h"
#include "llagent.h"
#include "llagentcamera.h"
#include "llappearancemgr.h"
#include "llcallbacklist.h"
#include "llfloatersidepanelcontainer.h"
#include "llgesturemgr.h"
#include "llinventorybridge.h"
#include "llinventoryfunctions.h"
#include "llinventorymodelbackgroundfetch.h"
#include "llinventoryobserver.h"
#include "llinventorypanel.h"
#include "lllocaltextureobject.h"
#include "llmd5.h" // <FS:Ansariel> [Legacy Bake]
#include "llnotificationsutil.h"
#include "lloutfitobserver.h"
#include "llsidepanelappearance.h"
#include "lltexlayer.h"
#include "lltooldraganddrop.h"
#include "llviewerregion.h"
#include "llvoavatarself.h"
#include "llviewerwearable.h"
#include "llwearablelist.h"
#include "llfloaterperms.h"
// [RLVa:KB] - Checked: 2011-05-22 (RLVa-1.3.1)
#include "rlvhandler.h"
#include "rlvlocks.h"
// [/RLVa:KB]

// <FS:Ansariel> [Legacy Bake]
#include "llagentwearablesfetch.h"
#ifdef OPENSIM
#include "llviewernetwork.h"
#endif
// </FS:Ansariel> [Legacy Bake]

LLAgentWearables gAgentWearables;

bool LLAgentWearables::mInitialWearablesUpdateReceived = false;
// [SL:KB] - Patch: Appearance-InitialWearablesLoadedCallback | Checked: 2010-08-14 (Catznip-2.1)
bool LLAgentWearables::mInitialWearablesLoaded = false;
// [/SL:KB]
// [RLVa:KB] - Checked: 2011-05-22 (RLVa-1.3.1)
bool LLAgentWearables::mInitialAttachmentsRequested = false;
// [/RLVa:KB]

using namespace LLAvatarAppearanceDefines;

///////////////////////////////////////////////////////////////////////////////

void set_default_permissions(LLViewerInventoryItem* item)
{
    llassert(item);
    LLPermissions perm = item->getPermissions();
    if (perm.getMaskNextOwner() != LLFloaterPerms::getNextOwnerPerms("Wearables")
        || perm.getMaskEveryone() != LLFloaterPerms::getEveryonePerms("Wearables")
        || perm.getMaskGroup() != LLFloaterPerms::getGroupPerms("Wearables"))
    {
        perm.setMaskNext(LLFloaterPerms::getNextOwnerPerms("Wearables"));
        perm.setMaskEveryone(LLFloaterPerms::getEveryonePerms("Wearables"));
        perm.setMaskGroup(LLFloaterPerms::getGroupPerms("Wearables"));

        item->setPermissions(perm);

        item->updateServer(false);
    }
}

// Callback to wear and start editing an item that has just been created.
void wear_and_edit_cb(const LLUUID& inv_item)
{
    if (inv_item.isNull()) return;

    LLViewerInventoryItem* item = gInventory.getItem(inv_item);
    if (!item) return;

    set_default_permissions(item);

    // item was just created, update even if permissions did not changed
    gInventory.updateItem(item);
    gInventory.notifyObservers();

    // Request editing the item after it gets worn.
    gAgentWearables.requestEditingWearable(inv_item);

    // Wear it.
    LLAppearanceMgr::instance().wearItemOnAvatar(inv_item,true);
}

void wear_cb(const LLUUID& inv_item)
{
    if (!inv_item.isNull())
    {
        LLViewerInventoryItem* item = gInventory.getItem(inv_item);
        if (item)
        {
            set_default_permissions(item);

            gInventory.updateItem(item);
            gInventory.notifyObservers();
        }
    }
}

///////////////////////////////////////////////////////////////////////////////

// HACK: For EXT-3923: Pants item shows in inventory with skin icon and messes with "current look"
// Some db items are corrupted, have inventory flags = 0, implying wearable type = shape, even though
// wearable type stored in asset is some other value.
// Calling this function whenever a wearable is added to increase visibility if this problem
// turns up in other inventories.
void checkWearableAgainstInventory(LLViewerWearable *wearable)
{
    if (wearable->getItemID().isNull())
        return;

    // Check for wearable type consistent with inventory item wearable type.
    LLViewerInventoryItem *item = gInventory.getItem(wearable->getItemID());
    if (item)
    {
        if (!item->isWearableType())
        {
            LL_WARNS() << "wearable associated with non-wearable item" << LL_ENDL;
        }
        if (item->getWearableType() != wearable->getType())
        {
            LL_WARNS() << "type mismatch: wearable " << wearable->getName()
                    << " has type " << wearable->getType()
                    << " but inventory item " << item->getName()
                    << " has type "  << item->getWearableType() << LL_ENDL;
        }
    }
    else
    {
        LL_WARNS() << "wearable inventory item not found" << wearable->getName()
                << " itemID " << wearable->getItemID().asString() << LL_ENDL;
    }
}

void LLAgentWearables::dump()
{
    LL_INFOS() << "LLAgentWearablesDump" << LL_ENDL;
    for (S32 i = 0; i < LLWearableType::WT_COUNT; i++)
    {
        U32 count = getWearableCount((LLWearableType::EType)i);
        LL_INFOS() << "Type: " << i << " count " << count << LL_ENDL;
        for (U32 j=0; j<count; j++)
        {
            LLViewerWearable* wearable = getViewerWearable((LLWearableType::EType)i,j);
            if (wearable == NULL)
            {
                LL_INFOS() << "    " << j << " NULL wearable" << LL_ENDL;
            }
            LL_INFOS() << "    " << j << " Name " << wearable->getName()
                    << " description " << wearable->getDescription() << LL_ENDL;

        }
    }
}

struct LLAgentDumper
{
    LLAgentDumper(std::string name):
        mName(name)
    {
        LL_INFOS() << LL_ENDL;
        LL_INFOS() << "LLAgentDumper " << mName << LL_ENDL;
        gAgentWearables.dump();
    }

    ~LLAgentDumper()
    {
        LL_INFOS() << LL_ENDL;
        LL_INFOS() << "~LLAgentDumper " << mName << LL_ENDL;
        gAgentWearables.dump();
    }

    std::string mName;
};

LLAgentWearables::LLAgentWearables() :
    LLWearableData(),
    mWearablesLoaded(false)
,   mCOFChangeInProgress(false)
{
}

LLAgentWearables::~LLAgentWearables()
{
    cleanup();
}

void LLAgentWearables::cleanup()
{
}

// static
void LLAgentWearables::initClass()
{
    // this can not be called from constructor because its instance is global and is created too early.
    // Subscribe to "COF is Saved" signal to notify observers about this (Loading indicator for ex.).
    LLOutfitObserver::instance().addCOFSavedCallback(boost::bind(&LLAgentWearables::notifyLoadingFinished, &gAgentWearables));
}

void LLAgentWearables::setAvatarObject(LLVOAvatarSelf *avatar)
{
    llassert(avatar);
    // <FS:Ansariel> [Legacy Bake]
#ifdef OPENSIM
    if (!LLGridManager::getInstance()->isInSecondLife())
    {
    avatar->outputRezTiming("Sending wearables request");
    sendAgentWearablesRequest();
    }
#endif
    // </FS:Ansariel> [Legacy Bake]
    setAvatarAppearance(avatar);
}

/**
 * @brief Construct a callback for dealing with the wearables.
 *
 * Would like to pass the agent in here, but we can't safely
 * count on it being around later.  Just use gAgent directly.
 * @param cb callback to execute on completion (? unused ?)
 * @param type Type for the wearable in the agent
 * @param wearable The wearable data.
 * @param todo Bitmask of actions to take on completion.
 */
LLAgentWearables::AddWearableToAgentInventoryCallback::AddWearableToAgentInventoryCallback(
    LLPointer<LLRefCount> cb, LLWearableType::EType type, U32 index, LLViewerWearable* wearable, U32 todo, const std::string description) :
    mType(type),
    mIndex(index),
    mWearable(wearable),
    mTodo(todo),
    mCB(cb),
    mDescription(description)
{
    LL_INFOS() << "constructor" << LL_ENDL;
}

void LLAgentWearables::AddWearableToAgentInventoryCallback::fire(const LLUUID& inv_item)
{
    if (inv_item.isNull())
        return;

    gAgentWearables.addWearabletoAgentInventoryDone(mType, mIndex, inv_item, mWearable);

    // <FS:Ansariel> [Legacy Bake]
    if (mTodo & CALL_UPDATE)
    {
        gAgentWearables.sendAgentWearablesUpdate();
    }
    if (mTodo & CALL_RECOVERDONE)
    {
        LLAppearanceMgr::instance().addCOFItemLink(inv_item);
        gAgentWearables.recoverMissingWearableDone();
    }
    // </FS:Ansariel> [Legacy Bake]

    /*
     * Do this for every one in the loop
     */
    // <FS:Ansariel> [Legacy Bake]
    if (mTodo & CALL_CREATESTANDARDDONE)
    {
        LLAppearanceMgr::instance().addCOFItemLink(inv_item);
        gAgentWearables.createStandardWearablesDone(mType, mIndex);
    }
    // </FS:Ansariel> [Legacy Bake]
    if (mTodo & CALL_MAKENEWOUTFITDONE)
    {
        gAgentWearables.makeNewOutfitDone(mType, mIndex);
    }
    if (mTodo & CALL_WEARITEM)
    {
        LLAppearanceMgr::instance().addCOFItemLink(inv_item,
            new LLUpdateAppearanceAndEditWearableOnDestroy(inv_item), mDescription);
        editWearable(inv_item);
    }
}

void LLAgentWearables::addWearabletoAgentInventoryDone(const LLWearableType::EType type,
                                                       const U32 index,
                                                       const LLUUID& item_id,
                                                       LLViewerWearable* wearable)
{
    LL_INFOS() << "type " << type << " index " << index << " item " << item_id.asString() << LL_ENDL;

    if (item_id.isNull())
        return;

    LLUUID old_item_id = getWearableItemID(type,index);

    if (wearable)
    {
        wearable->setItemID(item_id);

        if (old_item_id.notNull())
        {
            gInventory.addChangedMask(LLInventoryObserver::LABEL, old_item_id);
            setWearable(type,index,wearable);
        }
        else
        {
            pushWearable(type,wearable);
        }
    }

    gInventory.addChangedMask(LLInventoryObserver::LABEL, item_id);

    LLViewerInventoryItem* item = gInventory.getItem(item_id);
    if (item && wearable)
    {
        // We're changing the asset id, so we both need to set it
        // locally via setAssetUUID() and via setTransactionID() which
        // will be decoded on the server. JC
        item->setAssetUUID(wearable->getAssetID());
        item->setTransactionID(wearable->getTransactionID());
        gInventory.addChangedMask(LLInventoryObserver::INTERNAL, item_id);
        item->updateServer(false);
    }
    gInventory.notifyObservers();
}

// <FS:Ansariel> [Legacy Bake]
//void LLAgentWearables::saveWearable(const LLWearableType::EType type, const U32 index,
void LLAgentWearables::saveWearable(const LLWearableType::EType type, const U32 index, bool send_update,
// </FS:Ansariel> [Legacy Bake]
                                    const std::string new_name)
{
    LLViewerWearable* old_wearable = getViewerWearable(type, index);
    if(!old_wearable) return;
    bool name_changed = !new_name.empty() && (new_name != old_wearable->getName());
    if (name_changed || old_wearable->isDirty() || old_wearable->isOldVersion())
    {
        LLUUID old_item_id = old_wearable->getItemID();
        LLViewerWearable* new_wearable = LLWearableList::instance().createCopy(old_wearable);
        new_wearable->setItemID(old_item_id); // should this be in LLViewerWearable::copyDataFrom()?
        setWearable(type,index,new_wearable);

        // old_wearable may still be referred to by other inventory items. Revert
        // unsaved changes so other inventory items aren't affected by the changes
        // that were just saved.
        old_wearable->revertValues();

        LLInventoryItem* item = gInventory.getItem(old_item_id);
        if (item)
        {
            std::string item_name = item->getName();
            if (name_changed)
            {
                LL_INFOS() << "saveWearable changing name from "  << item->getName() << " to " << new_name << LL_ENDL;
                item_name = new_name;
            }
            // Update existing inventory item
            LLPointer<LLViewerInventoryItem> template_item =
                new LLViewerInventoryItem(item->getUUID(),
                                          item->getParentUUID(),
                                          item->getPermissions(),
                                          new_wearable->getAssetID(),
                                          new_wearable->getAssetType(),
                                          item->getInventoryType(),
                                          item_name,
                                          item->getDescription(),
                                          item->getSaleInfo(),
                                          item->getFlags(),
                                          item->getCreationDate());
            template_item->setTransactionID(new_wearable->getTransactionID());
            update_inventory_item(template_item, gAgentAvatarp->mEndCustomizeCallback);
        }
        else
        {
            // Add a new inventory item (shouldn't ever happen here)
            U32 todo = AddWearableToAgentInventoryCallback::CALL_NONE;
            // <FS:Ansariel> [Legacy Bake]
            if (send_update)
            {
                todo |= AddWearableToAgentInventoryCallback::CALL_UPDATE;
            }
            // </FS:Ansariel> [Legacy Bake]
            LLPointer<LLInventoryCallback> cb =
                new AddWearableToAgentInventoryCallback(
                    LLPointer<LLRefCount>(NULL),
                    type,
                    index,
                    new_wearable,
                    todo);
            addWearableToAgentInventory(cb, new_wearable);
            return;
        }

        // <FS:Ansariel> [Legacy Bake]
        //gAgentAvatarp->wearableUpdated(type);
        gAgentAvatarp->wearableUpdated(type, true);

        if (send_update)
        {
            sendAgentWearablesUpdate();
        }
        // </FS:Ansariel> [Legacy Bake]
    }
}

void LLAgentWearables::saveWearableAs(const LLWearableType::EType type,
                                      const U32 index,
                                      const std::string& new_name,
                                      const std::string& description,
                                      bool save_in_lost_and_found)
{
    if (!isWearableCopyable(type, index))
    {
        LL_WARNS() << "LLAgent::saveWearableAs() not copyable." << LL_ENDL;
        return;
    }
    LLViewerWearable* old_wearable = getViewerWearable(type, index);
    if (!old_wearable)
    {
        LL_WARNS() << "LLAgent::saveWearableAs() no old wearable." << LL_ENDL;
        return;
    }

    LLInventoryItem* item = gInventory.getItem(getWearableItemID(type,index));
    if (!item)
    {
        LL_WARNS() << "LLAgent::saveWearableAs() no inventory item." << LL_ENDL;
        return;
    }
    std::string trunc_name(new_name);
    LLStringUtil::truncate(trunc_name, DB_INV_ITEM_NAME_STR_LEN);
    LLViewerWearable* new_wearable = LLWearableList::instance().createCopy(
        old_wearable,
        trunc_name);

    LLPointer<LLInventoryCallback> cb =
        new AddWearableToAgentInventoryCallback(
            LLPointer<LLRefCount>(NULL),
            type,
            index,
            new_wearable,
            AddWearableToAgentInventoryCallback::CALL_WEARITEM,
            description
            );
    LLUUID category_id;
    if (save_in_lost_and_found)
    {
        category_id = gInventory.findCategoryUUIDForType(
            LLFolderType::FT_LOST_AND_FOUND);
    }
    else
    {
        // put in same folder as original
        category_id = item->getParentUUID();
    }

    copy_inventory_item(
        gAgent.getID(),
        item->getPermissions().getOwner(),
        item->getUUID(),
        category_id,
        new_name,
        cb);

    // old_wearable may still be referred to by other inventory items. Revert
    // unsaved changes so other inventory items aren't affected by the changes
    // that were just saved.
    old_wearable->revertValuesWithoutUpdate();
}

void LLAgentWearables::revertWearable(const LLWearableType::EType type, const U32 index)
{
    LLViewerWearable* wearable = getViewerWearable(type, index);
    llassert(wearable);
    if (wearable)
    {
        wearable->revertValues();
    }

    // <FS:Ansariel> [Legacy Bake]
    gAgent.sendAgentSetAppearance();
}

void LLAgentWearables::saveAllWearables()
{
    //if (!gInventory.isLoaded())
    //{
    //  return;
    //}

    for (S32 i=0; i < LLWearableType::WT_COUNT; i++)
    {
        for (U32 j=0; j < getWearableCount((LLWearableType::EType)i); j++)
            // <FS:Ansariel> [Legacy Bake]
            //saveWearable((LLWearableType::EType)i, j);
            saveWearable((LLWearableType::EType)i, j, false);
    }

    // <FS:Ansariel> [Legacy Bake]
    gAgent.sendAgentSetAppearance();
}

// Called when the user changes the name of a wearable inventory item that is currently being worn.
void LLAgentWearables::setWearableName(const LLUUID& item_id, const std::string& new_name)
{
    for (S32 i=0; i < LLWearableType::WT_COUNT; i++)
    {
        for (U32 j=0; j < getWearableCount((LLWearableType::EType)i); j++)
        {
            LLUUID curr_item_id = getWearableItemID((LLWearableType::EType)i,j);
            if (curr_item_id == item_id)
            {
                LLViewerWearable* old_wearable = getViewerWearable((LLWearableType::EType)i,j);
                llassert(old_wearable);
                if (!old_wearable) continue;

                std::string old_name = old_wearable->getName();
                old_wearable->setName(new_name);
                LLViewerWearable* new_wearable = LLWearableList::instance().createCopy(old_wearable);
                new_wearable->setItemID(item_id);
                LLInventoryItem* item = gInventory.getItem(item_id);
                if (item)
                {
                    new_wearable->setPermissions(item->getPermissions());
                }
                old_wearable->setName(old_name);

                setWearable((LLWearableType::EType)i,j,new_wearable);
                // <FS:Ansariel> [Legacy Bake]
                sendAgentWearablesUpdate();
                break;
            }
        }
    }
}


bool LLAgentWearables::isWearableModifiable(LLWearableType::EType type, U32 index) const
{
    LLUUID item_id = getWearableItemID(type, index);
    return item_id.notNull() ? isWearableModifiable(item_id) : false;
}

bool LLAgentWearables::isWearableModifiable(const LLUUID& item_id) const
{
    const LLUUID& linked_id = gInventory.getLinkedItemID(item_id);
    if (linked_id.notNull())
    {
        LLInventoryItem* item = gInventory.getItem(linked_id);
        if (item && item->getPermissions().allowModifyBy(gAgent.getID(),
                                                         gAgent.getGroupID()))
        {
            return true;
        }
    }
    return false;
}

bool LLAgentWearables::isWearableCopyable(LLWearableType::EType type, U32 index) const
{
    LLUUID item_id = getWearableItemID(type, index);
    if (!item_id.isNull())
    {
        LLInventoryItem* item = gInventory.getItem(item_id);
        if (item && item->getPermissions().allowCopyBy(gAgent.getID(),
                                                       gAgent.getGroupID()))
        {
            return true;
        }
    }
    return false;
}

LLInventoryItem* LLAgentWearables::getWearableInventoryItem(LLWearableType::EType type, U32 index)
{
    LLUUID item_id = getWearableItemID(type,index);
    LLInventoryItem* item = NULL;
    if (item_id.notNull())
    {
        item = gInventory.getItem(item_id);
    }
    return item;
}

const LLViewerWearable* LLAgentWearables::getWearableFromItemID(const LLUUID& item_id) const
{
    const LLUUID& base_item_id = gInventory.getLinkedItemID(item_id);
    for (S32 i=0; i < LLWearableType::WT_COUNT; i++)
    {
        for (U32 j=0; j < getWearableCount((LLWearableType::EType)i); j++)
        {
            const LLViewerWearable * curr_wearable = getViewerWearable((LLWearableType::EType)i, j);
            if (curr_wearable && (curr_wearable->getItemID() == base_item_id))
            {
                return curr_wearable;
            }
        }
    }
    return NULL;
}

LLViewerWearable* LLAgentWearables::getWearableFromItemID(const LLUUID& item_id)
{
    const LLUUID& base_item_id = gInventory.getLinkedItemID(item_id);
    for (S32 i=0; i < LLWearableType::WT_COUNT; i++)
    {
        for (U32 j=0; j < getWearableCount((LLWearableType::EType)i); j++)
        {
            LLViewerWearable * curr_wearable = getViewerWearable((LLWearableType::EType)i, j);
            if (curr_wearable && (curr_wearable->getItemID() == base_item_id))
            {
                return curr_wearable;
            }
        }
    }
    return NULL;
}

LLViewerWearable*   LLAgentWearables::getWearableFromAssetID(const LLUUID& asset_id)
{
    for (S32 i=0; i < LLWearableType::WT_COUNT; i++)
    {
        for (U32 j=0; j < getWearableCount((LLWearableType::EType)i); j++)
        {
            LLViewerWearable * curr_wearable = getViewerWearable((LLWearableType::EType)i, j);
            if (curr_wearable && (curr_wearable->getAssetID() == asset_id))
            {
                return curr_wearable;
            }
        }
    }
    return NULL;
}

LLViewerWearable* LLAgentWearables::getViewerWearable(const LLWearableType::EType type, U32 index /*= 0*/)
{
    return dynamic_cast<LLViewerWearable*> (getWearable(type, index));
}

const LLViewerWearable* LLAgentWearables::getViewerWearable(const LLWearableType::EType type, U32 index /*= 0*/) const
{
    return dynamic_cast<const LLViewerWearable*> (getWearable(type, index));
}

// static
bool LLAgentWearables::selfHasWearable(LLWearableType::EType type)
{
    return (gAgentWearables.getWearableCount(type) > 0);
}

// virtual
void LLAgentWearables::wearableUpdated(LLWearable *wearable, bool removed)
{
    if (isAgentAvatarValid())
    {
        // <FS:Ansariel> [Legacy Bake]
        //gAgentAvatarp->wearableUpdated(wearable->getType());
        const bool upload_result = removed;
        gAgentAvatarp->wearableUpdated(wearable->getType(), upload_result);
        // </FS:Ansariel> [Legacy Bake]
    }

    LLWearableData::wearableUpdated(wearable, removed);

    if (!removed)
    {
        LLViewerWearable* viewer_wearable = dynamic_cast<LLViewerWearable*>(wearable);
        viewer_wearable->refreshName();

        // Hack pt 2. If the wearable we just loaded has definition version 24,
        // then force a re-save of this wearable after slamming the version number to 22.
        // This number was incorrectly incremented for internal builds before release, and
        // this fix will ensure that the affected wearables are re-saved with the right version number.
        // the versions themselves are compatible. This code can be removed before release.
        if( wearable->getDefinitionVersion() == 24 )
        {
            U32 index;
            if (getWearableIndex(wearable,index))
            {
                LL_INFOS() << "forcing wearable type " << wearable->getType() << " to version 22 from 24" << LL_ENDL;
                wearable->setDefinitionVersion(22);
                // <FS:Ansariel> [Legacy Bake]
                //saveWearable(wearable->getType(),index);
                saveWearable(wearable->getType(),index, true);
            }
        }

        checkWearableAgainstInventory(viewer_wearable);
    }
}

const LLUUID LLAgentWearables::getWearableItemID(LLWearableType::EType type, U32 index) const
{
    const LLViewerWearable *wearable = getViewerWearable(type,index);
    if (wearable)
        return wearable->getItemID();
    else
        return LLUUID();
}

// [RLVa:KB] - Checked: 2011-03-31 (RLVa-1.3.0)
void LLAgentWearables::getWearableItemIDs(uuid_vec_t& idItems) const
{
    for (wearableentry_map_t::const_iterator itWearableType = mWearableDatas.begin();
            itWearableType != mWearableDatas.end(); ++itWearableType)
    {
        getWearableItemIDs(itWearableType->first, idItems);
    }
}

void LLAgentWearables::getWearableItemIDs(LLWearableType::EType eType, uuid_vec_t& idItems) const
{
    wearableentry_map_t::const_iterator itWearableType = mWearableDatas.find(eType);
    if (mWearableDatas.end() != itWearableType)
    {
        for (wearableentry_vec_t::const_iterator itWearable = itWearableType->second.begin();
                itWearable != itWearableType->second.end(); ++itWearable)
        {
            const LLViewerWearable* pWearable = dynamic_cast<LLViewerWearable*>(*itWearable);
            if (pWearable)
            {
                idItems.push_back(pWearable->getItemID());
            }
        }
    }
}
// [/RLVa:KB]

const LLUUID LLAgentWearables::getWearableAssetID(LLWearableType::EType type, U32 index) const
{
    const LLViewerWearable *wearable = getViewerWearable(type,index);
    if (wearable)
        return wearable->getAssetID();
    else
        return LLUUID();
}

bool LLAgentWearables::isWearingItem(const LLUUID& item_id) const
{
    return getWearableFromItemID(item_id) != nullptr;
}

void LLAgentWearables::addLocalTextureObject(const LLWearableType::EType wearable_type, const LLAvatarAppearanceDefines::ETextureIndex texture_type, U32 wearable_index)
{
    LLViewerWearable* wearable = getViewerWearable((LLWearableType::EType)wearable_type, wearable_index);
    if (!wearable)
    {
        LL_ERRS() << "Tried to add local texture object to invalid wearable with type " << wearable_type << " and index " << wearable_index << LL_ENDL;
        return;
    }
    LLLocalTextureObject lto;
    wearable->setLocalTextureObject(texture_type, lto);
}

class OnWearableItemCreatedCB: public LLInventoryCallback
{
public:
    OnWearableItemCreatedCB():
        mWearablesAwaitingItems(LLWearableType::WT_COUNT,NULL)
    {
        LL_INFOS() << "created callback" << LL_ENDL;
    }
    /* virtual */ void fire(const LLUUID& inv_item)
    {
        LL_INFOS() << "One item created " << inv_item.asString() << LL_ENDL;
        LLConstPointer<LLInventoryObject> item = gInventory.getItem(inv_item);
        mItemsToLink.push_back(item);
        updatePendingWearable(inv_item);
    }
    ~OnWearableItemCreatedCB()
    {
        LL_INFOS() << "All items created" << LL_ENDL;
        LLPointer<LLInventoryCallback> link_waiter = new LLUpdateAppearanceOnDestroy;
        link_inventory_array(LLAppearanceMgr::instance().getCOF(),
                             mItemsToLink,
                             link_waiter);
    }
    void addPendingWearable(LLViewerWearable *wearable)
    {
        if (!wearable)
        {
            LL_WARNS() << "no wearable" << LL_ENDL;
            return;
        }
        LLWearableType::EType type = wearable->getType();
        if (type<LLWearableType::WT_COUNT)
        {
            mWearablesAwaitingItems[type] = wearable;
        }
        else
        {
            LL_WARNS() << "invalid type " << type << LL_ENDL;
        }
    }
    void updatePendingWearable(const LLUUID& inv_item)
    {
        LLViewerInventoryItem *item = gInventory.getItem(inv_item);
        if (!item)
        {
            LL_WARNS() << "no item found" << LL_ENDL;
            return;
        }
        if (!item->isWearableType())
        {
            LL_WARNS() << "non-wearable item found" << LL_ENDL;
            return;
        }
        if (item && item->isWearableType())
        {
            LLWearableType::EType type = item->getWearableType();
            if (type < LLWearableType::WT_COUNT)
            {
                LLViewerWearable *wearable = mWearablesAwaitingItems[type];
                if (wearable)
                    wearable->setItemID(inv_item);
            }
            else
            {
                LL_WARNS() << "invalid wearable type " << type << LL_ENDL;
            }
        }
    }

private:
    LLInventoryObject::const_object_list_t mItemsToLink;
    std::vector<LLViewerWearable*> mWearablesAwaitingItems;
};

void LLAgentWearables::createStandardWearables()
{
    LL_WARNS() << "Creating standard wearables" << LL_ENDL;

    if (!isAgentAvatarValid()) return;

    constexpr bool create[LLWearableType::WT_COUNT] =
        {
            true,  //LLWearableType::WT_SHAPE
            true,  //LLWearableType::WT_SKIN
            true,  //LLWearableType::WT_HAIR
            true,  //LLWearableType::WT_EYES
            true,  //LLWearableType::WT_SHIRT
            true,  //LLWearableType::WT_PANTS
            true,  //LLWearableType::WT_SHOES
            true,  //LLWearableType::WT_SOCKS
            false, //LLWearableType::WT_JACKET
            false, //LLWearableType::WT_GLOVES
            true,  //LLWearableType::WT_UNDERSHIRT
            true,  //LLWearableType::WT_UNDERPANTS
            false  //LLWearableType::WT_SKIRT
        };

    LLPointer<LLInventoryCallback> cb = new OnWearableItemCreatedCB;
    for (S32 i=0; i < LLWearableType::WT_COUNT; i++)
    {
        if (create[i])
        {
            llassert(getWearableCount((LLWearableType::EType)i) == 0);
            LLViewerWearable* wearable = LLWearableList::instance().createNewWearable((LLWearableType::EType)i, gAgentAvatarp);
            ((OnWearableItemCreatedCB*)(&(*cb)))->addPendingWearable(wearable);
            // no need to update here...
            LLUUID category_id = LLUUID::null;
            create_inventory_wearable(gAgent.getID(),
                                  gAgent.getSessionID(),
                                  category_id,
                                  wearable->getTransactionID(),
                                  wearable->getName(),
                                  wearable->getDescription(),
                                  wearable->getAssetType(),
                                  wearable->getType(),
                                  wearable->getPermissions().getMaskNextOwner(),
                                  cb);
        }
    }
}

// We no longer need this message in the current viewer, but send
// it for now to maintain compatibility with release viewers. Can
// remove this function once the SH-3455 changesets are universally deployed.
void LLAgentWearables::sendDummyAgentWearablesUpdate()
{
    LL_DEBUGS("Avatar") << "sendAgentWearablesUpdate()" << LL_ENDL;

    // Send the AgentIsNowWearing
    gMessageSystem->newMessageFast(_PREHASH_AgentIsNowWearing);

    gMessageSystem->nextBlockFast(_PREHASH_AgentData);
    gMessageSystem->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
    gMessageSystem->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());

    // Send 4 standardized nonsense item ids (same as returned by the modified sim, not that it especially matters).
    gMessageSystem->nextBlockFast(_PREHASH_WearableData);
    gMessageSystem->addU8Fast(_PREHASH_WearableType, U8(1));
    gMessageSystem->addUUIDFast(_PREHASH_ItemID, LLUUID("db5a4e5f-9da3-44c8-992d-1181c5795498"));

    gMessageSystem->nextBlockFast(_PREHASH_WearableData);
    gMessageSystem->addU8Fast(_PREHASH_WearableType, U8(2));
    gMessageSystem->addUUIDFast(_PREHASH_ItemID, LLUUID("6969c7cc-f72f-4a76-a19b-c293cce8ce4f"));

    gMessageSystem->nextBlockFast(_PREHASH_WearableData);
    gMessageSystem->addU8Fast(_PREHASH_WearableType, U8(3));
    gMessageSystem->addUUIDFast(_PREHASH_ItemID, LLUUID("7999702b-b291-48f9-8903-c91dfb828408"));

    gMessageSystem->nextBlockFast(_PREHASH_WearableData);
    gMessageSystem->addU8Fast(_PREHASH_WearableType, U8(4));
    gMessageSystem->addUUIDFast(_PREHASH_ItemID, LLUUID("566cb59e-ef60-41d7-bfa6-e0f293fbea40"));

    gAgent.sendReliableMessage();
}

void LLAgentWearables::makeNewOutfitDone(S32 type, U32 index)
{
    LLUUID first_item_id = getWearableItemID((LLWearableType::EType)type, index);
    // Open the inventory and select the first item we added.
    if (first_item_id.notNull())
    {
        LLInventoryPanel *active_panel = LLInventoryPanel::getActiveInventoryPanel();
        if (active_panel)
        {
            active_panel->setSelection(first_item_id, TAKE_FOCUS_NO);
        }
    }
}


void LLAgentWearables::addWearableToAgentInventory(LLPointer<LLInventoryCallback> cb,
                                                   LLViewerWearable* wearable,
                                                   const LLUUID& category_id,
                                                   bool notify)
{
    create_inventory_wearable(gAgent.getID(),
                          gAgent.getSessionID(),
                          category_id,
                          wearable->getTransactionID(),
                          wearable->getName(),
                          wearable->getDescription(),
                          wearable->getAssetType(),
                          wearable->getType(),
                          wearable->getPermissions().getMaskNextOwner(),
                          cb);
}

void LLAgentWearables::removeWearable(const LLWearableType::EType type, bool do_remove_all, U32 index)
{
    if (getWearableCount(type) == 0)
    {
        // no wearables to remove
        return;
    }

    if (do_remove_all)
    {
        removeWearableFinal(type, do_remove_all, index);
    }
    else
    {
        LLViewerWearable* old_wearable = getViewerWearable(type,index);

        if (old_wearable)
        {
            if (old_wearable->isDirty())
            {
                LLSD payload;
                payload["wearable_type"] = (S32)type;
                payload["wearable_index"] = (S32)index;
                // Bring up view-modal dialog: Save changes? Yes, No, Cancel
                LLNotificationsUtil::add("WearableSave", LLSD(), payload, &LLAgentWearables::onRemoveWearableDialog);
                return;
            }
            else
            {
                removeWearableFinal(type, do_remove_all, index);
            }
        }
    }
}


// static
bool LLAgentWearables::onRemoveWearableDialog(const LLSD& notification, const LLSD& response)
{
    S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
    LLWearableType::EType type = (LLWearableType::EType)notification["payload"]["wearable_type"].asInteger();
    S32 index = (S32)notification["payload"]["wearable_index"].asInteger();
    switch(option)
    {
        case 0:  // "Save"
            gAgentWearables.saveWearable(type, index);
            gAgentWearables.removeWearableFinal(type, false, index);
            break;

        case 1:  // "Don't Save"
            gAgentWearables.removeWearableFinal(type, false, index);
            break;

        case 2: // "Cancel"
            break;

        default:
            llassert(0);
            break;
    }
    return false;
}

// Called by removeWearable() and onRemoveWearableDialog() to actually do the removal.
void LLAgentWearables::removeWearableFinal(const LLWearableType::EType type, bool do_remove_all, U32 index)
{
    //LLAgentDumper dumper("removeWearable");
    if (do_remove_all)
    {
        S32 max_entry = getWearableCount(type)-1;
        for (S32 i=max_entry; i>=0; i--)
        {
            LLViewerWearable* old_wearable = getViewerWearable(type,i);
            if (old_wearable)
            {
                eraseWearable(old_wearable);
                // <FS:Ansariel> [Legacy Bake]
                //old_wearable->removeFromAvatar();
                old_wearable->removeFromAvatar(true);
            }
        }
//      clearWearableType(type);
// [RLVa:KB] - Checked: 2010-05-14 (RLVa-1.2.0)
        // The line above shouldn't be needed
        RLV_VERIFY(0 == getWearableCount(type));
// [/RLVa:KB]
    }
    else
    {
        LLViewerWearable* old_wearable = getViewerWearable(type, index);

        if (old_wearable)
        {
            eraseWearable(old_wearable);
            // <FS:Ansariel> [Legacy Bake]
            //old_wearable->removeFromAvatar();
            old_wearable->removeFromAvatar(true);
        }
    }

    // <FS:Ansariel> [Legacy Bake]
    queryWearableCache();
    updateServer();
    // </FS:Ansariel> [Legacy Bake]

    gInventory.notifyObservers();
}

// Assumes existing wearables are not dirty.
void LLAgentWearables::setWearableOutfit(const LLInventoryItem::item_array_t& items,
                                         const std::vector< LLViewerWearable* >& wearables)
{
    LL_INFOS() << "setWearableOutfit() start" << LL_ENDL;

    auto count = wearables.size();
    llassert(items.size() == count);

    // Check for whether outfit already matches the one requested
    S32 matched = 0, mismatched = 0;
    const S32 arr_size = LLWearableType::WT_COUNT;
    S32 type_counts[arr_size];
    bool update_inventory{ false };
    std::fill(type_counts,type_counts+arr_size,0);
    for (size_t i = 0; i < count; i++)
    {
        LLViewerWearable* new_wearable = wearables[i];
        LLPointer<LLInventoryItem> new_item = items[i];

        const LLWearableType::EType type = new_wearable->getType();
        //<FS:Beq> BOM fallback legacy opensim
        #ifdef OPENSIM
        if (!LLGridManager::getInstance()->isInSecondLife())
        {
            if(!gAgent.getRegion()->bakesOnMeshEnabled())
            {
                if(type == LLWearableType::WT_UNIVERSAL)
                {
                    LL_DEBUGS("Avatar") << "Universal wearable not supported on this region - ignoring." << LL_ENDL;
                    mismatched++;
                    continue;
                }
            }
        }
        #endif
        //</FS:Beq>
        if (type < 0 || type>=LLWearableType::WT_COUNT)
        {
            LL_WARNS() << "invalid type " << type << LL_ENDL;
            mismatched++;
            continue;
        }
        S32 index = type_counts[type];
        type_counts[type]++;

        LLViewerWearable *curr_wearable = dynamic_cast<LLViewerWearable*>(getWearable(type,index));
        if (!new_wearable || !curr_wearable ||
            new_wearable->getAssetID() != curr_wearable->getAssetID())
        {
            LL_DEBUGS("Avatar") << "mismatch, type " << type << " index " << index
                                << " names " << (curr_wearable ? curr_wearable->getName() : "NONE")  << ","
                                << " names " << (new_wearable ? new_wearable->getName() : "NONE")  << LL_ENDL;
            mismatched++;
            continue;
        }

        // Update only inventory in this case - ordering of wearables with the same asset id has no effect.
        // Updating wearables in this case causes the two-alphas error in MAINT-4158.
        // We should actually disallow wearing two wearables with the same asset id.
        if (curr_wearable->getName() != new_item->getName() ||
            curr_wearable->getItemID() != new_item->getUUID())
        {
            LL_DEBUGS("Avatar") << "mismatch on name or inventory id, names "
                                << curr_wearable->getName() << " vs " << new_item->getName()
                                << " item ids " << curr_wearable->getItemID() << " vs " << new_item->getUUID()
                                << LL_ENDL;
            update_inventory = true;
            continue;
        }
        // If we got here, everything matches.
        matched++;
    }
    LL_DEBUGS("Avatar") << "matched " << matched << " mismatched " << mismatched << LL_ENDL;
    for (S32 j=0; j<LLWearableType::WT_COUNT; j++)
    {
        LLWearableType::EType type = (LLWearableType::EType) j;
        if (getWearableCount(type) != type_counts[j])
        {
            LL_DEBUGS("Avatar") << "count mismatch for type " << j << " current " << getWearableCount(j) << " requested " << type_counts[j] << LL_ENDL;
            mismatched++;
        }
    }
    if (mismatched == 0 && !update_inventory)
    {
        LL_DEBUGS("Avatar") << "no changes, bailing out" << LL_ENDL;
        notifyLoadingFinished();
        return;
    }

    // updating inventory
    LLWearableType* wearable_type_inst = LLWearableType::getInstance();

    // TODO: Removed check for ensuring that teens don't remove undershirt and underwear. Handle later
    // note: shirt is the first non-body part wearable item. Update if wearable order changes.
    // This loop should remove all clothing, but not any body parts
    for (S32 j = 0; j < (S32)LLWearableType::WT_COUNT; j++)
    {
        if (wearable_type_inst->getAssetType((LLWearableType::EType)j) == LLAssetType::AT_CLOTHING)
        {
            removeWearable((LLWearableType::EType)j, true, 0);
        }
    }

    for (S32 i = 0; i < count; i++)
    {
        LLViewerWearable* new_wearable = wearables[i];
        LLPointer<LLInventoryItem> new_item = items[i];

        llassert(new_wearable);
        if (new_wearable)
        {
            const LLWearableType::EType type = new_wearable->getType();

            LLUUID old_wearable_id = new_wearable->getItemID();
            new_wearable->setName(new_item->getName());
            new_wearable->setItemID(new_item->getUUID());

            if (wearable_type_inst->getAssetType(type) == LLAssetType::AT_BODYPART)
            {
                // exactly one wearable per body part
                setWearable(type,0,new_wearable);
                if (old_wearable_id.notNull())
                {
                    // we changed id before setting wearable, update old item manually
                    // to complete the swap.
                    gInventory.addChangedMask(LLInventoryObserver::LABEL, old_wearable_id);
                }
            }
            else
            {
                pushWearable(type,new_wearable);
            }

            constexpr bool removed = false;
            wearableUpdated(new_wearable, removed);
        }
    }

    gInventory.notifyObservers();

    if (mismatched == 0)
    {
        LL_DEBUGS("Avatar") << "inventory updated, wearable assets not changed, bailing out" << LL_ENDL;
        notifyLoadingFinished();
        return;
    }

    // updating agent avatar

    if (isAgentAvatarValid())
    {
        gAgentAvatarp->setCompositeUpdatesEnabled(true);

        // If we have not yet loaded core parts, we may want to use
        // baked texture UUIDs sent from the first objectUpdate message
        // don't overwrite these. If we have parts already, we've saved
        // these texture ids as the last known good textures and can
        // invalidate without having to recloud avatar.
        if (!gAgentAvatarp->getHasMissingParts())
        {
            gAgentAvatarp->invalidateAll();
        }
    }

    // Start rendering & update the server
    mWearablesLoaded = true;

// [SL:KB] - Patch: Appearance-InitialWearablesLoadedCallback | Checked: 2010-09-22 (Catznip-2.2)
    if (!mInitialWearablesLoaded)
    {
        mInitialWearablesLoaded = true;
        mInitialWearablesLoadedSignal();
    }
// [/SL:KB]
    notifyLoadingFinished();

    // Copy wearable params to avatar.
    gAgentAvatarp->writeWearablesToAvatar();

    // Then update the avatar based on the copied params.
    gAgentAvatarp->updateVisualParams();

    // <FS:Ansariel> [Legacy Bake]
    queryWearableCache();
    updateServer();
    // </FS:Ansariel> [Legacy Bake]

    gAgentAvatarp->dumpAvatarTEs("setWearableOutfit");

    LL_DEBUGS("Avatar") << "setWearableOutfit() end" << LL_ENDL;
}


// User has picked "wear on avatar" from a menu.
//void LLAgentWearables::setWearableItem(LLInventoryItem* new_item, LLViewerWearable* new_wearable, bool do_append)
//{
//  //LLAgentDumper dumper("setWearableItem");
//  if (isWearingItem(new_item->getUUID()))
//  {
//      LL_WARNS() << "wearable " << new_item->getUUID() << " is already worn" << LL_ENDL;
//      return;
//  }
//
//  const LLWearableType::EType type = new_wearable->getType();
//
//  if (!do_append)
//  {
//      // Remove old wearable, if any
//      // MULTI_WEARABLE: hardwired to 0
//      LLViewerWearable* old_wearable = getViewerWearable(type,0);
//      if (old_wearable)
//      {
//          const LLUUID& old_item_id = old_wearable->getItemID();
//          if ((old_wearable->getAssetID() == new_wearable->getAssetID()) &&
//              (old_item_id == new_item->getUUID()))
//          {
//              LL_DEBUGS() << "No change to wearable asset and item: " << LLWearableType::getInstance()->getTypeName(type) << LL_ENDL;
//              return;
//          }
//
//          if (old_wearable->isDirty())
//          {
//              // Bring up modal dialog: Save changes? Yes, No, Cancel
//              LLSD payload;
//              payload["item_id"] = new_item->getUUID();
//              LLNotificationsUtil::add("WearableSave", LLSD(), payload, boost::bind(onSetWearableDialog, _1, _2, new_wearable));
//              return;
//          }
//      }
//  }
//
//  setWearableFinal(new_item, new_wearable, do_append);
//}

// static
//bool LLAgentWearables::onSetWearableDialog(const LLSD& notification, const LLSD& response, LLViewerWearable* wearable)
//{
//  S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
//  LLInventoryItem* new_item = gInventory.getItem(notification["payload"]["item_id"].asUUID());
//  U32 index;
//  if (!gAgentWearables.getWearableIndex(wearable,index))
//  {
//      LL_WARNS() << "Wearable not found" << LL_ENDL;
//      delete wearable;
//      return false;
//  }
//  switch(option)
//  {
//      case 0:  // "Save"
//          gAgentWearables.saveWearable(wearable->getType(),index);
//          gAgentWearables.setWearableFinal(new_item, wearable);
//          break;
//
//      case 1:  // "Don't Save"
//          gAgentWearables.setWearableFinal(new_item, wearable);
//          break;
//
//      case 2: // "Cancel"
//          break;
//
//      default:
//          llassert(0);
//          break;
//  }
//
//  delete wearable;
//  return false;
//}

// Called from setWearableItem() and onSetWearableDialog() to actually set the wearable.
// MULTI_WEARABLE: unify code after null objects are gone.
//void LLAgentWearables::setWearableFinal(LLInventoryItem* new_item, LLViewerWearable* new_wearable, bool do_append)
//{
//  const LLWearableType::EType type = new_wearable->getType();
//
//  if (do_append && getWearableItemID(type,0).notNull())
//  {
//      new_wearable->setItemID(new_item->getUUID());
//      const bool trigger_updated = false;
//      pushWearable(type, new_wearable, trigger_updated);
//      LL_INFOS() << "Added additional wearable for type " << type
//              << " size is now " << getWearableCount(type) << LL_ENDL;
//      checkWearableAgainstInventory(new_wearable);
//  }
//  else
//  {
//      // Replace the old wearable with a new one.
//      llassert(new_item->getAssetUUID() == new_wearable->getAssetID());
//
//      LLViewerWearable *old_wearable = getViewerWearable(type,0);
//      LLUUID old_item_id;
//      if (old_wearable)
//      {
//          old_item_id = old_wearable->getItemID();
//      }
//      new_wearable->setItemID(new_item->getUUID());
//      setWearable(type,0,new_wearable);
//
//      if (old_item_id.notNull())
//      {
//          gInventory.addChangedMask(LLInventoryObserver::LABEL, old_item_id);
//          gInventory.notifyObservers();
//      }
//      LL_INFOS() << "Replaced current element 0 for type " << type
//              << " size is now " << getWearableCount(type) << LL_ENDL;
//  }
//}

// User has picked "remove from avatar" from a menu.
// static
//void LLAgentWearables::userRemoveWearable(const LLWearableType::EType &type, const U32 &index)
//{
//  if (!(type==LLWearableType::WT_SHAPE || type==LLWearableType::WT_SKIN || type==LLWearableType::WT_HAIR || type==LLWearableType::WT_EYES)) //&&
//      //!((!gAgent.isTeen()) && (type==LLWearableType::WT_UNDERPANTS || type==LLWearableType::WT_UNDERSHIRT)))
//  {
//      gAgentWearables.removeWearable(type,false,index);
//  }
//}

//static
//void LLAgentWearables::userRemoveWearablesOfType(const LLWearableType::EType &type)
//{
//  if (!(type==LLWearableType::WT_SHAPE || type==LLWearableType::WT_SKIN || type==LLWearableType::WT_HAIR || type==LLWearableType::WT_EYES)) //&&
//      //!((!gAgent.isTeen()) && (type==LLWearableType::WT_UNDERPANTS || type==LLWearableType::WT_UNDERSHIRT)))
//  {
//      gAgentWearables.removeWearable(type,true,0);
//  }
//}

// Given a desired set of attachments, find what objects need to be
// removed, and what additional inventory items need to be added.
void LLAgentWearables::findAttachmentsAddRemoveInfo(LLInventoryModel::item_array_t& obj_item_array,
                                                    llvo_vec_t& objects_to_remove,
                                                    llvo_vec_t& objects_to_retain,
                                                    LLInventoryModel::item_array_t& items_to_add)
{
    // Possible cases:
    // already wearing but not in request set -> take off.
    // already wearing and in request set -> leave alone.
    // not wearing and in request set -> put on.

    if (!isAgentAvatarValid()) return;

    std::set<LLUUID> requested_item_ids;
    std::set<LLUUID> current_item_ids;
    for (S32 i=0; i<obj_item_array.size(); i++)
    {
        const LLUUID & requested_id = obj_item_array[i].get()->getLinkedUUID();
        //LL_INFOS() << "Requested attachment id " << requested_id << LL_ENDL;
        requested_item_ids.insert(requested_id);
    }

    // Build up list of objects to be removed and items currently attached.
    LLVOAvatar::attachment_map_t::iterator iter = gAgentAvatarp->mAttachmentPoints.begin();
    LLVOAvatar::attachment_map_t::iterator end = gAgentAvatarp->mAttachmentPoints.end();
    while (iter != end)
    {
        LLVOAvatar::attachment_map_t::iterator curiter = iter++;
        LLViewerJointAttachment* attachment = curiter->second;
        for (LLViewerJointAttachment::attachedobjs_vec_t::iterator attachment_iter = attachment->mAttachedObjects.begin();
             attachment_iter != attachment->mAttachedObjects.end();
             ++attachment_iter)
        {
            LLViewerObject *objectp = attachment_iter->get();
            if (objectp)
            {
                LLUUID object_item_id = objectp->getAttachmentItemID();

                bool remove_attachment = true;
                if (requested_item_ids.find(object_item_id) != requested_item_ids.end())
                {   // Object currently worn, was requested to keep it
                    // Flag as currently worn so we won't have to add it again.
                    remove_attachment = false;
                }
                else if (objectp->isTempAttachment())
                {   // Check if we should keep this temp attachment
                    remove_attachment = LLAppearanceMgr::instance().shouldRemoveTempAttachment(objectp->getID());
                }

                if (remove_attachment)
                {
                    // LL_INFOS() << "found object to remove, id " << objectp->getID() << ", item " << objectp->getAttachmentItemID() << LL_ENDL;
                    objects_to_remove.push_back(objectp);
                }
                else
                {
                    // LL_INFOS() << "found object to keep, id " << objectp->getID() << ", item " << objectp->getAttachmentItemID() << LL_ENDL;
                    current_item_ids.insert(object_item_id);
                    objects_to_retain.push_back(objectp);
                }
            }
        }
    }

    for (LLInventoryModel::item_array_t::iterator it = obj_item_array.begin();
         it != obj_item_array.end();
         ++it)
    {
        LLUUID linked_id = (*it).get()->getLinkedUUID();
        if (current_item_ids.find(linked_id) != current_item_ids.end())
        {
            // Requested attachment is already worn.
        }
        else
        {
            // Requested attachment is not worn yet.
            items_to_add.push_back(*it);
        }
    }
    // S32 remove_count = objects_to_remove.size();
    // S32 add_count = items_to_add.size();
    // LL_INFOS() << "remove " << remove_count << " add " << add_count << LL_ENDL;
}

std::vector<LLViewerObject*> LLAgentWearables::getTempAttachments()
{
    llvo_vec_t temp_attachs;
    if (isAgentAvatarValid())
    {
        for (LLVOAvatar::attachment_map_t::iterator iter = gAgentAvatarp->mAttachmentPoints.begin(); iter != gAgentAvatarp->mAttachmentPoints.end();)
        {
            LLVOAvatar::attachment_map_t::iterator curiter = iter++;
            LLViewerJointAttachment* attachment = curiter->second;
            for (LLViewerJointAttachment::attachedobjs_vec_t::iterator attachment_iter = attachment->mAttachedObjects.begin();
                attachment_iter != attachment->mAttachedObjects.end();
                ++attachment_iter)
            {
                LLViewerObject *objectp = attachment_iter->get();
                if (objectp && objectp->isTempAttachment())
                {
                    temp_attachs.push_back(objectp);
                }
            }
        }
    }
    return temp_attachs;
}

void LLAgentWearables::userRemoveMultipleAttachments(llvo_vec_t& objects_to_remove)
{
    if (!isAgentAvatarValid()) return;

// [RLVa:KB] - Checked: 2010-03-04 (RLVa-1.2.0)
    // RELEASE-RLVa: [SL-3.4] Check our callers and verify that erasing elements from the passed vector won't break random things
    if ( (rlv_handler_t::isEnabled()) && (gRlvAttachmentLocks.hasLockedAttachmentPoint(RLV_LOCK_REMOVE)) )
    {
        llvo_vec_t::iterator itObj = objects_to_remove.begin();
        while (objects_to_remove.end() != itObj)
        {
            const LLViewerObject* pAttachObj = *itObj;
            if (gRlvAttachmentLocks.isLockedAttachment(pAttachObj))
            {
                itObj = objects_to_remove.erase(itObj);

                // Fall-back code: re-add the attachment if it got removed from COF somehow (compensates for possible bugs elsewhere)
                bool fInCOF = LLAppearanceMgr::instance().isLinkedInCOF(pAttachObj->getAttachmentItemID());
                RLV_ASSERT(fInCOF);
                if (!fInCOF)
                {
                    LLAppearanceMgr::instance().registerAttachment(pAttachObj->getAttachmentItemID());
                }
            }
            else
            {
                ++itObj;
            }
        }
    }
// [/RLVa:KB]

    if (objects_to_remove.empty())
        return;

    LL_DEBUGS("Avatar") << "ATT [ObjectDetach] removing " << objects_to_remove.size() << " objects" << LL_ENDL;
    gMessageSystem->newMessage("ObjectDetach");
    gMessageSystem->nextBlockFast(_PREHASH_AgentData);
    gMessageSystem->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
    gMessageSystem->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());

    for (llvo_vec_t::iterator it = objects_to_remove.begin();
         it != objects_to_remove.end();
         ++it)
    {
        LLViewerObject *objectp = *it;
        //gAgentAvatarp->resetJointPositionsOnDetach(objectp);
        gMessageSystem->nextBlockFast(_PREHASH_ObjectData);
        gMessageSystem->addU32Fast(_PREHASH_ObjectLocalID, objectp->getLocalID());
        const LLUUID& item_id = objectp->getAttachmentItemID();
        LLViewerInventoryItem *item = gInventory.getItem(item_id);
        LL_DEBUGS("Avatar") << "ATT removing object, item is " << (item ? item->getName() : "UNKNOWN") << " " << item_id << LL_ENDL;
        LLAttachmentsMgr::instance().onDetachRequested(item_id);
    }
    gMessageSystem->sendReliable(gAgent.getRegionHost());
}

void LLAgentWearables::userAttachMultipleAttachments(LLInventoryModel::item_array_t& obj_item_array)
{
    // Build a compound message to send all the objects that need to be rezzed.
    auto obj_count = obj_item_array.size();
    if (obj_count > 0)
    {
        LL_DEBUGS("Avatar") << "ATT attaching multiple, total obj_count " << obj_count << LL_ENDL;
    }

    for(LLInventoryModel::item_array_t::const_iterator it = obj_item_array.begin();
        it != obj_item_array.end();
        ++it)
    {
        const LLInventoryItem* item = *it;
        LLAttachmentsMgr::instance().addAttachmentRequest(item->getLinkedUUID(), 0, true);
    }

// [RLVa:KB] - Checked: 2011-05-22 (RLVa-1.3.1)
    mInitialAttachmentsRequested = true;
// [/RLVa:KB]
}

// Returns false if the given wearable is already topmost/bottommost
// (depending on closer_to_body parameter).
bool LLAgentWearables::canMoveWearable(const LLUUID& item_id, bool closer_to_body) const
{
    const LLWearable* wearable = getWearableFromItemID(item_id);
    if (!wearable) return false;

    LLWearableType::EType wtype = wearable->getType();
    const LLWearable* marginal_wearable = closer_to_body ? getBottomWearable(wtype) : getTopWearable(wtype);
    if (!marginal_wearable) return false;

    return wearable != marginal_wearable;
}

bool LLAgentWearables::areWearablesLoaded() const
{
    return mWearablesLoaded;
}

bool LLAgentWearables::canWearableBeRemoved(const LLViewerWearable* wearable) const
{
    if (!wearable) return false;

    LLWearableType::EType type = wearable->getType();
    // Make sure the user always has at least one shape, skin, eyes, and hair type currently worn.
    return !(((type == LLWearableType::WT_SHAPE) || (type == LLWearableType::WT_SKIN) || (type == LLWearableType::WT_HAIR) || (type == LLWearableType::WT_EYES))
             && (getWearableCount(type) <= 1) );
}

// <FS:Ansariel> [Legacy Bake]
//void LLAgentWearables::animateAllWearableParams(F32 delta)
void LLAgentWearables::animateAllWearableParams(F32 delta, bool upload_bake)
{
    for( S32 type = 0; type < LLWearableType::WT_COUNT; ++type )
    {
        for (S32 count = 0; count < (S32)getWearableCount((LLWearableType::EType)type); ++count)
        {
            LLViewerWearable *wearable = getViewerWearable((LLWearableType::EType)type,count);
            llassert(wearable);
            if (wearable)
            {
                // <FS:Ansariel> [Legacy Bake]
                //wearable->animateParams(delta);
                wearable->animateParams(delta, upload_bake);
            }
        }
    }
}

bool LLAgentWearables::moveWearable(const LLViewerInventoryItem* item, bool closer_to_body)
{
    if (!item) return false;
    if (!item->isWearableType()) return false;

    LLWearableType::EType type = item->getWearableType();
    U32 wearable_count = getWearableCount(type);
    if (0 == wearable_count) return false;

    const LLUUID& asset_id = item->getAssetUUID();

    //nowhere to move if the wearable is already on any boundary (closest to the body/furthest from the body)
    if (closer_to_body)
    {
        LLViewerWearable* bottom_wearable = dynamic_cast<LLViewerWearable*>( getBottomWearable(type) );
        if (bottom_wearable->getAssetID() == asset_id)
        {
            return false;
        }
    }
    else // !closer_to_body
    {
        LLViewerWearable* top_wearable = dynamic_cast<LLViewerWearable*>( getTopWearable(type) );
        if (top_wearable->getAssetID() == asset_id)
        {
            return false;
        }
    }

    for (U32 i = 0; i < wearable_count; ++i)
    {
        LLViewerWearable* wearable = getViewerWearable(type, i);
        if (!wearable) continue;
        if (wearable->getAssetID() != asset_id) continue;

        //swapping wearables
        U32 swap_i = closer_to_body ? i-1 : i+1;
        swapWearables(type, i, swap_i);
        return true;
    }

    return false;
}

// static
void LLAgentWearables::createWearable(LLWearableType::EType type, bool wear, const LLUUID& parent_id, std::function<void(const LLUUID&)> created_cb)
{
    if (type == LLWearableType::WT_INVALID || type == LLWearableType::WT_NONE) return;

    if (type == LLWearableType::WT_UNIVERSAL && !gAgent.getRegion()->bakesOnMeshEnabled())
    {
        LL_WARNS("Inventory") << "Can't create WT_UNIVERSAL type " << LL_ENDL;
        return;
    }

    LLViewerWearable* wearable = LLWearableList::instance().createNewWearable(type, gAgentAvatarp);
    LLAssetType::EType asset_type = wearable->getAssetType();
    LLPointer<LLBoostFuncInventoryCallback> cb;
    if(wear)
    {
        cb = new LLBoostFuncInventoryCallback(wear_and_edit_cb);
    }
    else
    {
        cb = new LLBoostFuncInventoryCallback(wear_cb);
    }
    if (created_cb != NULL)
    {
        cb->addOnFireFunc(created_cb);
    }

    LLUUID folder_id;

    if (parent_id.notNull())
    {
        folder_id = parent_id;
    }
    else
    {
        LLFolderType::EType folder_type = LLFolderType::assetTypeToFolderType(asset_type);
        folder_id = gInventory.findCategoryUUIDForType(folder_type);
    }

    create_inventory_wearable(gAgent.getID(),
                          gAgent.getSessionID(),
                          folder_id,
                          wearable->getTransactionID(),
                          wearable->getName(),
                          wearable->getDescription(),
                          asset_type,
                          wearable->getType(),
                          LLFloaterPerms::getNextOwnerPerms("Wearables"),
                          cb);
}

// static
void LLAgentWearables::editWearable(const LLUUID& item_id)
{
    LLViewerInventoryItem* item = gInventory.getLinkedItem(item_id);
    if (!item)
    {
        LL_WARNS() << "Failed to get linked item" << LL_ENDL;
        return;
    }

    if (!item->isFinished())
    {
        LL_WARNS() << "Tried to edit wearable that isn't loaded" << LL_ENDL;
        // Restart fetch or put item to the front
        LLInventoryModelBackgroundFetch::instance().start(item->getUUID(), false);
        return;
    }

    LLViewerWearable* wearable = gAgentWearables.getWearableFromItemID(item_id);
    if (!wearable)
    {
        LL_WARNS() << "Cannot get wearable" << LL_ENDL;
        return;
    }

    if (!gAgentWearables.isWearableModifiable(item->getUUID()))
    {
        LL_WARNS() << "Cannot modify wearable" << LL_ENDL;
        return;
    }

    S32 shape_count = gAgentWearables.getWearableCount(LLWearableType::WT_SHAPE);
    S32 hair_count = gAgentWearables.getWearableCount(LLWearableType::WT_HAIR);
    S32 eye_count = gAgentWearables.getWearableCount(LLWearableType::WT_EYES);
    S32 skin_count = gAgentWearables.getWearableCount(LLWearableType::WT_SKIN);
    if (!shape_count || !hair_count || !eye_count || !skin_count)
    {
        // Don't let user edit wearables if avatar is cloud due to missing parts.
        // Let user edit wearables if avatar is cloud due to missing textures.
        LL_WARNS() << "Cannot modify wearable. Avatar is cloud and missing parts." << LL_ENDL;
        return;
    }

    const bool disable_camera_switch = LLWearableType::getInstance()->getDisableCameraSwitch(wearable->getType());
    LLPanel* panel = LLFloaterSidePanelContainer::getPanel("appearance");
    LLSidepanelAppearance::editWearable(wearable, panel, disable_camera_switch);
}

// Request editing the item after it gets worn.
void LLAgentWearables::requestEditingWearable(const LLUUID& item_id)
{
    mItemToEdit = gInventory.getLinkedItemID(item_id);
}

// Start editing the item if previously requested.
void LLAgentWearables::editWearableIfRequested(const LLUUID& item_id)
{
    if (mItemToEdit.notNull() &&
        mItemToEdit == gInventory.getLinkedItemID(item_id))
    {
        LLAgentWearables::editWearable(item_id);
        mItemToEdit.setNull();
    }
}

boost::signals2::connection LLAgentWearables::addLoadingStartedCallback(loading_started_callback_t cb)
{
    return mLoadingStartedSignal.connect(cb);
}

boost::signals2::connection LLAgentWearables::addLoadedCallback(loaded_callback_t cb)
{
    return mLoadedSignal.connect(cb);
}

bool LLAgentWearables::changeInProgress() const
{
    return mCOFChangeInProgress;
}

// [SL:KB] - Patch: Appearance-InitialWearablesLoadedCallback | Checked: 2010-08-14 (Catznip-2.1)
boost::signals2::connection LLAgentWearables::addInitialWearablesLoadedCallback(const loaded_callback_t& cb)
{
    return mInitialWearablesLoadedSignal.connect(cb);
}
// [/SL:KB]

void LLAgentWearables::notifyLoadingStarted()
{
    mCOFChangeInProgress = true;
    mCOFChangeTimer.reset();
    mLoadingStartedSignal();
}

void LLAgentWearables::notifyLoadingFinished()
{
    mCOFChangeInProgress = false;
    mLoadedSignal();
}

// <FS:Ansariel> [Legacy Bake]
//-----------------------------------------------------------------------------
// Legacy baking
//-----------------------------------------------------------------------------
// wearables
LLAgentWearables::createStandardWearablesAllDoneCallback::~createStandardWearablesAllDoneCallback()
{
    LL_INFOS() << "destructor - all done?" << LL_ENDL;
    gAgentWearables.createStandardWearablesAllDone();
}

LLAgentWearables::sendAgentWearablesUpdateCallback::~sendAgentWearablesUpdateCallback()
{
    gAgentWearables.sendAgentWearablesUpdate();
}

void LLAgentWearables::sendAgentWearablesUpdate()
{
    // First make sure that we have inventory items for each wearable
    for (S32 type=0; type < LLWearableType::WT_COUNT; ++type)
    {
        for (U32 index=0; index < getWearableCount((LLWearableType::EType)type); ++index)
        {
            LLViewerWearable* wearable = getViewerWearable((LLWearableType::EType)type,index);
            if (wearable)
            {
                if (wearable->getItemID().isNull())
                {
                    LLPointer<LLInventoryCallback> cb =
                        new AddWearableToAgentInventoryCallback(
                            LLPointer<LLRefCount>(NULL),
                            (LLWearableType::EType)type,
                            index,
                            wearable,
                            AddWearableToAgentInventoryCallback::CALL_NONE);
                    addWearableToAgentInventory(cb, wearable);
                }
                else
                {
                    gInventory.addChangedMask(LLInventoryObserver::LABEL,
                                              wearable->getItemID());
                }
            }
        }
    }

    // Then make sure the inventory is in sync with the avatar.
    gInventory.notifyObservers();

    // Send the AgentIsNowWearing
    gMessageSystem->newMessageFast(_PREHASH_AgentIsNowWearing);

    gMessageSystem->nextBlockFast(_PREHASH_AgentData);
    gMessageSystem->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
    gMessageSystem->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());

    LL_DEBUGS() << "sendAgentWearablesUpdate()" << LL_ENDL;
    // MULTI-WEARABLE: DEPRECATED: HACK: index to 0- server database tables don't support concept of multiwearables.
    for (S32 type=0; type < LLWearableType::WT_COUNT; ++type)
    {
        gMessageSystem->nextBlockFast(_PREHASH_WearableData);

        U8 type_u8 = (U8)type;
        gMessageSystem->addU8Fast(_PREHASH_WearableType, type_u8);

        LLViewerWearable* wearable = getViewerWearable((LLWearableType::EType)type, 0);
        if (wearable)
        {
            //LL_INFOS() << "Sending wearable " << wearable->getName() << LL_ENDL;
            LLUUID item_id = wearable->getItemID();
            const LLViewerInventoryItem *item = gInventory.getItem(item_id);
            if (item && item->getIsLinkType())
            {
                // Get the itemID that this item points to.  i.e. make sure
                // we are storing baseitems, not their links, in the database.
                item_id = item->getLinkedUUID();
            }
            gMessageSystem->addUUIDFast(_PREHASH_ItemID, item_id);
        }
        else
        {
            //LL_INFOS() << "Not wearing wearable type " << LLWearableType::getInstance()->getTypeName((LLWearableType::EType)i) << LL_ENDL;
            gMessageSystem->addUUIDFast(_PREHASH_ItemID, LLUUID::null);
        }

        LL_DEBUGS() << "       " << LLWearableType::getInstance()->getTypeLabel((LLWearableType::EType)type) << ": " << (wearable ? wearable->getAssetID() : LLUUID::null) << LL_ENDL;
    }
    gAgent.sendReliableMessage();
}

void LLAgentWearables::sendAgentWearablesRequest()
{
    gMessageSystem->newMessageFast(_PREHASH_AgentWearablesRequest);
    gMessageSystem->nextBlockFast(_PREHASH_AgentData);
    gMessageSystem->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
    gMessageSystem->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
    gAgent.sendReliableMessage();
}

bool LLAgentWearables::itemUpdatePending(const LLUUID& item_id) const
{
    return mItemsAwaitingWearableUpdate.find(item_id) != mItemsAwaitingWearableUpdate.end();
}

U32 LLAgentWearables::itemUpdatePendingCount() const
{
    return static_cast<U32>(mItemsAwaitingWearableUpdate.size());
}

// MULTI-WEARABLE: DEPRECATED (see backwards compatibility)
// static
// ! BACKWARDS COMPATIBILITY ! When we stop supporting viewer1.23, we can assume
// that viewers have a Current Outfit Folder and won't need this message, and thus
// we can remove/ignore this whole function. EXCEPT gAgentWearables.notifyLoadingStarted
void LLAgentWearables::processAgentInitialWearablesUpdate(LLMessageSystem* mesgsys, void** user_data)
{
    // We should only receive this message a single time.  Ignore subsequent AgentWearablesUpdates
    // that may result from AgentWearablesRequest having been sent more than once.
    if (mInitialWearablesUpdateReceived)
        return;

    if (isAgentAvatarValid())
    {
        gAgentAvatarp->startPhase("process_initial_wearables_update");
        gAgentAvatarp->outputRezTiming("Received initial wearables update");
    }

    // notify subscribers that wearables started loading. See EXT-7777
    // *TODO: find more proper place to not be called from deprecated method.
    // Seems such place is found: LLInitialWearablesFetch::processContents()
    gAgentWearables.notifyLoadingStarted();

    mInitialWearablesUpdateReceived = true;

    LLUUID agent_id;
    gMessageSystem->getUUIDFast(_PREHASH_AgentData, _PREHASH_AgentID, agent_id);

    if (isAgentAvatarValid() && (agent_id == gAgentAvatarp->getID()))
    {
        gMessageSystem->getU32Fast(_PREHASH_AgentData, _PREHASH_SerialNum, gAgentQueryManager.mUpdateSerialNum);

        constexpr S32 NUM_BODY_PARTS = 4;
        S32 num_wearables = gMessageSystem->getNumberOfBlocksFast(_PREHASH_WearableData);
        if (num_wearables < NUM_BODY_PARTS)
        {
            // Transitional state.  Avatars should always have at least their body parts (hair, eyes, shape and skin).
            // The fact that they don't have any here (only a dummy is sent) implies that either:
            // 1. This account existed before we had wearables
            // 2. The database has gotten messed up
            // 3. This is the account's first login (i.e. the wearables haven't been generated yet).
            return;
        }

        // Get the UUID of the current outfit folder (will be created if it doesn't exist)
        const LLUUID current_outfit_id = gInventory.findCategoryUUIDForType(LLFolderType::FT_CURRENT_OUTFIT);
        LLInitialWearablesFetch* outfit = new LLInitialWearablesFetch(current_outfit_id);

        //LL_DEBUGS() << "processAgentInitialWearablesUpdate()" << LL_ENDL;
        // Add wearables
        // MULTI-WEARABLE: DEPRECATED: Message only supports one wearable per type, will be ignored in future.
        gAgentWearables.mItemsAwaitingWearableUpdate.clear();
        for (S32 i=0; i < num_wearables; i++)
        {
            // Parse initial wearables data from message system
            U8 type_u8 = 0;
            gMessageSystem->getU8Fast(_PREHASH_WearableData, _PREHASH_WearableType, type_u8, i);
            if (type_u8 >= LLWearableType::WT_COUNT)
            {
                continue;
            }
            const LLWearableType::EType type = (LLWearableType::EType) type_u8;

            LLUUID item_id;
            gMessageSystem->getUUIDFast(_PREHASH_WearableData, _PREHASH_ItemID, item_id, i);

            LLUUID asset_id;
            gMessageSystem->getUUIDFast(_PREHASH_WearableData, _PREHASH_AssetID, asset_id, i);
            if (asset_id.isNull())
            {
                LLViewerWearable::removeFromAvatar(type, false);
            }
            else
            {
                LLAssetType::EType asset_type = LLWearableType::getInstance()->getAssetType(type);
                if (asset_type == LLAssetType::AT_NONE)
                {
                    continue;
                }

                // MULTI-WEARABLE: DEPRECATED: this message only supports one wearable per type. Should be ignored in future versions

                // Store initial wearables data until we know whether we have the current outfit folder or need to use the data.
                LLInitialWearablesFetch::InitialWearableData wearable_data(type, item_id, asset_id);
                outfit->add(wearable_data);
            }

            LL_DEBUGS() << "       " << LLWearableType::getInstance()->getTypeLabel(type) << LL_ENDL;
        }

        // Get the complete information on the items in the inventory and set up an observer
        // that will trigger when the complete information is fetched.
        outfit->startFetch();
        if(outfit->isFinished())
        {
            // everything is already here - call done.
            outfit->done();
        }
        else
        {
            // it's all on it's way - add an observer, and the inventory
            // will call done for us when everything is here.
            gInventory.addObserver(outfit);
        }

    }
}

// Normally, all wearables referred to "AgentWearablesUpdate" will correspond to actual assets in the
// database.  If for some reason, we can't load one of those assets, we can try to reconstruct it so that
// the user isn't left without a shape, for example.  (We can do that only after the inventory has loaded.)
void LLAgentWearables::recoverMissingWearable(const LLWearableType::EType type, U32 index)
{
    // Try to recover by replacing missing wearable with a new one.
    LLNotificationsUtil::add("ReplacedMissingWearable");
    LL_DEBUGS() << "Wearable " << LLWearableType::getInstance()->getTypeLabel(type) << " could not be downloaded.  Replaced inventory item with default wearable." << LL_ENDL;
    LLViewerWearable* new_wearable = LLWearableList::instance().createNewWearable(type, gAgentAvatarp);

    setWearable(type,index,new_wearable);
    //new_wearable->writeToAvatar(true);

    // Add a new one in the lost and found folder.
    // (We used to overwrite the "not found" one, but that could potentially
    // destory content.) JC
    const LLUUID lost_and_found_id = gInventory.findCategoryUUIDForType(LLFolderType::FT_LOST_AND_FOUND);
    LLPointer<LLInventoryCallback> cb =
        new AddWearableToAgentInventoryCallback(
            LLPointer<LLRefCount>(NULL),
            type,
            index,
            new_wearable,
            AddWearableToAgentInventoryCallback::CALL_RECOVERDONE);
    addWearableToAgentInventory(cb, new_wearable, lost_and_found_id, true);
}

void LLAgentWearables::recoverMissingWearableDone()
{
    // Have all the wearables that the avatar was wearing at log-in arrived or been fabricated?
    updateWearablesLoaded();
    if (areWearablesLoaded())
    {
        // Make sure that the server's idea of the avatar's wearables actually match the wearables.
        gAgent.sendAgentSetAppearance();
    }
    else
    {
        gInventory.addChangedMask(LLInventoryObserver::LABEL, LLUUID::null);
        gInventory.notifyObservers();
    }
}

void LLAgentWearables::createStandardWearablesDone(S32 type, U32 index)
{
    LL_INFOS() << "type " << type << " index " << index << LL_ENDL;

    if (!isAgentAvatarValid()) return;
    gAgentAvatarp->updateVisualParams();
}

void LLAgentWearables::createStandardWearablesAllDone()
{
    // ... because sendAgentWearablesUpdate will notify inventory
    // observers.
    LL_INFOS() << "all done?" << LL_ENDL;

    mWearablesLoaded = true;
    notifyLoadingFinished();

    updateServer();

    // Treat this as the first texture entry message, if none received yet
    gAgentAvatarp->onFirstTEMessageReceived();
}

void LLAgentWearables::queryWearableCache()
{
    if (!areWearablesLoaded() || (gAgent.getRegion() && gAgent.getRegion()->getCentralBakeVersion()))
    {
        return;
    }

    // Look up affected baked textures.
    // If they exist:
    //      disallow updates for affected layersets (until dataserver responds with cache request.)
    //      If cache miss, turn updates back on and invalidate composite.
    //      If cache hit, modify baked texture entries.
    //
    // Cache requests contain list of hashes for each baked texture entry.
    // Response is list of valid baked texture assets. (same message)

    gMessageSystem->newMessageFast(_PREHASH_AgentCachedTexture);
    gMessageSystem->nextBlockFast(_PREHASH_AgentData);
    gMessageSystem->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
    gMessageSystem->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
    gMessageSystem->addS32Fast(_PREHASH_SerialNum, gAgentQueryManager.mWearablesCacheQueryID);

    S32 num_queries = 0;
    // <FS:Beq> BOM fallback for legacy opensim
    // for (U8 baked_index = 0; baked_index < BAKED_NUM_INDICES; baked_index++)
    for (U8 baked_index = 0; baked_index < gAgentAvatarp->getNumBakes(); baked_index++)
    // </FS:Beq>
    {
        LLUUID hash_id = computeBakedTextureHash((EBakedTextureIndex) baked_index);
        if (hash_id.notNull())
        {
            num_queries++;
            // *NOTE: make sure at least one request gets packed

            ETextureIndex te_index = LLAvatarAppearance::getDictionary()->bakedToLocalTextureIndex((EBakedTextureIndex)baked_index);

            //LL_INFOS() << "Requesting texture for hash " << hash << " in baked texture slot " << baked_index << LL_ENDL;
            gMessageSystem->nextBlockFast(_PREHASH_WearableData);
            gMessageSystem->addUUIDFast(_PREHASH_ID, hash_id);
            gMessageSystem->addU8Fast(_PREHASH_TextureIndex, (U8)te_index);
        }

        gAgentQueryManager.mActiveCacheQueries[baked_index] = gAgentQueryManager.mWearablesCacheQueryID;
    }
    //VWR-22113: gAgent.getRegion() can return null if invalid, seen here on logout
    if(gAgent.getRegion())
    {
        if (isAgentAvatarValid())
        {
            selfStartPhase("fetch_texture_cache_entries");
            gAgentAvatarp->outputRezTiming("Fetching textures from cache");
        }

        LL_DEBUGS("Avatar") << gAgentAvatarp->avString() << "Requesting texture cache entry for " << num_queries << " baked textures" << LL_ENDL;
        gMessageSystem->sendReliable(gAgent.getRegion()->getHost());
        gAgentQueryManager.mNumPendingQueries++;
        gAgentQueryManager.mWearablesCacheQueryID++;
    }
}

// virtual
void LLAgentWearables::invalidateBakedTextureHash(LLMD5& hash) const
{
    // Add some garbage into the hash so that it becomes invalid.
    if (isAgentAvatarValid())
    {
        hash.update((const unsigned char*)gAgentAvatarp->getID().mData, UUID_BYTES);
    }
}

// MULTI-WEARABLE: DEPRECATED: item pending count relies on old messages that don't support multi-wearables. do not trust to be accurate
void LLAgentWearables::updateWearablesLoaded()
{
    mWearablesLoaded = (itemUpdatePendingCount()==0);
    if (mWearablesLoaded)
    {
        notifyLoadingFinished();
    }
}

void LLAgentWearables::updateServer()
{
    sendAgentWearablesUpdate();
    gAgent.sendAgentSetAppearance();
}
// </FS:Ansariel> [Legacy Bake]


// EOF
