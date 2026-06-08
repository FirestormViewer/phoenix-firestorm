/**
 * @file fsnewitemctrl.cpp
 * @brief A singleton controller for handling adding new items to Objects using Build Window's Content tab.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (c) 2026 minerjr @ The Phoenix Firestorm Project, Inc.
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
 * $/LicenseInfo$
 */

// <FS:minerjr> [FIRE-36685] - Toolbox Window - Add dropdown of new items to Content tab

#include "fsnewitemctrl.h"
#include "llinventory.h"
#include "llviewerinventory.h"
#include "llselectmgr.h"
#include "llcontrol.h" // gSavedSettings
#include "llinventorymodel.h" // gInventory
#include "llagentdata.h" // gAgentID

extern LLControlGroup gSavedSettings;
extern LLInventoryModel gInventory;
extern LLUUID gAgentID;

FSNewItemCtrl::FSNewItemCtrl() : LLSingleton<FSNewItemCtrl>(),
    mState(EAddNewItemState::IDLE),
    mItemAssetType(LLAssetType::AT_NONE),
    mOriginalItemUUID(),
    mNewItemUUID()
{

}

FSNewItemCtrl::~FSNewItemCtrl()
{
    // delete Xxx;
}

void FSNewItemCtrl::init()
{
    mState = EAddNewItemState::IDLE;
    mItemAssetType = LLAssetType::AT_NONE;
    mOriginalItemUUID.setNull();
    mNewItemUUID.setNull();
}

void FSNewItemCtrl::processStates(LLPanelObjectInventory* panel_object_inv)
{
    // Check if the passed in panel object inventory is valid, and if not, return
    if (panel_object_inv == nullptr)
    {
        return;
    }

    LLFolderView* folders = panel_object_inv->getRootFolder();

    // Idle state, just return as there is nothing to process
    if (mState == EAddNewItemState::IDLE)
    {
        return;
    }
    // Handle the start of the drag and drop the item from avatar inventory to the object's inventory
    // wait until the inventory movement has finished before handling the item appearing in the inventory
    else if (mState == EAddNewItemState::DND_INV_TO_OBJECT)
    {
        if (LLViewerObject* objectp = gObjectList.findObject(panel_object_inv->getTaskUUID()))
        {
            // If the inveotory is not pending and not dirty, then we want to move to the next step.
            if (objectp && !objectp->isInventoryPending() && !objectp->isInventoryDirty())
            {
                mState = EAddNewItemState::DND_INV_TO_OBJECT_DONE;
            }
        }
    }
    // Handles when the item appears, will select the new item and set it to rename.
    // If the item is able to be opened, it will be opened.
    else if (mState == EAddNewItemState::DND_INV_TO_OBJECT_DONE)
    {
        bool inventory_has_focus = panel_object_inv->hasInventory() && folders && gFocusMgr.childHasKeyboardFocus(folders);

        // Restore the show new inventory current settings
        restoreInventoryFlags();

        panel_object_inv->setFocusRoot(true);

        if (!mNewItemUUID.isNull())
        {
            // Flag that item needs to be renamed, needs to be done once added to the actual folder.
            LLFolderViewItem* current_item = panel_object_inv->getItemByID(mNewItemUUID);
            if (current_item && folders)
            {
                // Set the found
                folders->setSelection(current_item, true, inventory_has_focus);
                // mFolders->requestArrange();
                folders->startRenamingSelectedItem();
                // If the item is a gesture, notecard, text or script, we need to re-open the item floater to allow floater to
                // modify the object content's item and not the one generated in the avatar's inventory.
                if (mItemAssetType == LLAssetType::AT_LSL_TEXT || mItemAssetType == LLAssetType::AT_NOTECARD ||
                    mItemAssetType == LLAssetType::AT_GESTURE || mItemAssetType == LLAssetType::AT_MATERIAL)
                {
                    current_item->openItem();
                }
            }
        }
        // Turn off the do action
        mState = EAddNewItemState::MOVE_INT_TO_TRASH_START;
    }
    else if (mState == EAddNewItemState::MOVE_INT_TO_TRASH_START)
    {
        // Once the object's inventory is done being processed, can switch back to the idle state
        if (LLViewerObject* objectp = gObjectList.findObject(panel_object_inv->getTaskUUID()))
        {
            // If there is no original item (LSL scripts), can go back to normal
            if (mOriginalItemUUID.isNull())
            {
                onRemoveItemDone();
            }
            // Else if the inveotory is not pending and not dirty, then we want to move to the next step.
            else if (objectp && !objectp->isInventoryPending() && !objectp->isInventoryDirty())
            {
                // Remove the original inventory item and do a callback to set the
                // state to the next one once it finishes.
                LLPointer<LLInventoryCallback> cb = new LLBoostFuncInventoryCallback(boost::bind(FSNewItemCtrl::onRemoveItemDone));
                remove_inventory_item(mOriginalItemUUID, cb);
                // Set the state to the move iventory to trash, which will spin until the callback is triggered
                mState = EAddNewItemState::MOVE_INT_TO_TRASH;
            }
        }
    }
    // No need to MOVE_INT_TO_TRASH as it's handled by the onRemoveItemDone callback method.
}

// <FS:minerjr> [FIRE-36685] - Toolbox Window - Add dropdown of new items to Content tab
// static
// Callback method for when an item is removed from the avatar's inventory to reset the internal state
void FSNewItemCtrl::onRemoveItemDone()
{
    // Store the pointer to the FSNewItemCtrl as a static value as it is a singleton and
    // saves from having to be called all the time.
    static FSNewItemCtrl* self = FSNewItemCtrl::getInstance();
    if (self)
    {
        // Set the new item state back to idle
        self->setState(FSNewItemCtrl::EAddNewItemState::IDLE);
        self->callFinishNewItem();
    }
}

// Call the Finish New Item callback method if it is valid.
void FSNewItemCtrl::callFinishNewItem()
{
    if (mFinishNewItemCallback)
        mFinishNewItemCallback();
}

void FSNewItemCtrl::onCreateInvDone(LLHandle<LLPanel> handle, const LLUUID& new_id, const void_callback_t& cb)
{
    gInventory.notifyObservers();
    LLPanelContents* panel = (LLPanelContents*)handle.get();

    if (panel && new_id.notNull())
    {
        // The the selected object's root object
        const bool children_ok = true;
        LLPointer<LLViewerObject> object = LLSelectMgr::getInstance()->getSelection()->getFirstRootObject(children_ok);

        if (object && new_id.notNull())
        {
            // Get the Inventory Item from the new ID passed in
            LLViewerInventoryItem* item = gInventory.getItem(new_id);
            if (item)
            {
                // If the item can move to the target object
                if (LLToolDragAndDrop::isInventoryDropAcceptable(object, item))
                {
                    // Set the panel to treat the newest item as a new item and needs to perform an action on it.
                    startDNDInvToObject(item->getUUID(), cb);
                    // Drag and Drop the new item into the inventory of the object.
                    LLToolDragAndDrop::dropInventory(object, item, LLToolDragAndDrop::SOURCE_AGENT, gAgentID);
                }
            }
            else
            {
                LL_WARNS() << "Could not transfer new item with ID: " << new_id << LL_ENDL;
            }
        }
    }
}

void FSNewItemCtrl::startDNDInvToObject(const LLUUID& original_UUID, const void_callback_t& cb)
{
    // Store the UUID of the original item drag and dropped by the script
    // so once it is finished with, can then next move to the avatar's trash.
    mOriginalItemUUID = original_UUID;
    // Flag that an action needs to take place on the newest item
    mState = EAddNewItemState::DND_INV_TO_OBJECT_START;

    // Set the finish new item callback to the one passed in, defaults to null.
    mFinishNewItemCallback = cb;
}

bool FSNewItemCtrl::checkAgainstLatestObject(const LLPointer<LLInventoryObject> &obj)
{
    // If there is a need to do an action on the item, then
    if (mState == EAddNewItemState::DND_INV_TO_OBJECT_START)
    {
        // This is needed to be able to convert the LLPointer<LLInventoryObject> to an LLInventoryItem*.
        // Issue is the existing methods ends up returning a const pointer, which you then cannnot
        // act upon or return with a static method call.
        LLInventoryObject* current_object = obj;
        LLInventoryItem*   current_item   = dynamic_cast<LLInventoryItem*>(current_object);

        // Need to search for the newest item as the new item recieved a new UUID so the one
        // from the LLPanelContents.cpp no longer exists.
        // Use the CreationDate of the item to determine which is the newer item.
        if (current_item && current_item->getCreationDate() > mLatestCreationTime)
        {
            // Store the latest creation date
            mLatestCreationTime = current_item->getCreationDate();
            // Store the current items new UUID and action type (for use to determine if the item needs to be opened)
            mNewItemUUID   = current_item->getUUID();
            mItemAssetType = current_item->getActualType();
            mLatestCreatedItem = current_item;

            // Return true as there was a newer item was found
            return true;
        }
    }
    // Return false as there was no newer item found
    return false;
}

// Store the show invengory flags and disables them.
// Used to hide the item floaters attached to the new avatar items
// so the user does not edit the wrong items.
void FSNewItemCtrl::saveAndClearInventoryFlags()
{
    // Save the show new, in and gesture inventory settings
    S32 backup_inventory_flags = 0;
    backup_inventory_flags |= S32(gSavedSettings.getBOOL("ShowNewInventory")) * SHOW_NEW_INVENTORY;
    backup_inventory_flags |= S32(gSavedSettings.getBOOL("ShowInInventory")) * SHOW_IN_INVENTORY;
    gSavedSettings.setS32("BackupInventoryFlags", backup_inventory_flags);

    // do not pop up preview floaters when creating new and in inventory items.
    gSavedSettings.setBOOL("ShowNewInventory", false);
    gSavedSettings.setBOOL("ShowInInventory", false);
}

// Restores the Show Inventory settings.xml flags.
void FSNewItemCtrl::restoreInventoryFlags()
{
    // Restore the show new inventory current settings
    S32 backup_inventory_flags = gSavedSettings.getS32("BackupInventoryFlags");
    gSavedSettings.setBOOL("ShowNewInventory", bool(backup_inventory_flags & SHOW_NEW_INVENTORY));
    gSavedSettings.setBOOL("ShowInInventory", bool(backup_inventory_flags & SHOW_IN_INVENTORY));
}
// </FS:minerjr> [FIRE-36685]
