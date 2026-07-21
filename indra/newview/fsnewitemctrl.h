/**
 * @file fsnewitemctrl.h
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

#ifndef FSNEWITEMCTRL_H
#define FSNEWITEMCTRL_H

// <FS:mjr> [FIRE-36685] - Toolbox Window - Add new notecard button to Content tab

#include "llsingleton.h"
#include "llpanelobjectinventory.h"

class LLPanelContents;

class FSNewItemCtrl : public LLSingleton<FSNewItemCtrl>
{
    LLSINGLETON(FSNewItemCtrl);
    ~FSNewItemCtrl() = default;

public:
    enum class EAddNewItemState
    {
        IDLE = 0,
        DND_INV_TO_OBJECT_START,
        DND_INV_TO_OBJECT,
        DND_INV_TO_OBJECT_DONE,
        MOVE_INT_TO_TRASH_START,
        MOVE_INT_TO_TRASH,
        MOVE_INT_TO_TRASH_DONE
    };

    using void_callback_t = std::function<void()>;
    using panel_contents_callback_t = std::function<void(LLPanelContents*, const LLUUID& new_id)>;

    void processStates(LLPanelObjectInventory* panel_object_inv);
    void onRemoveItemDone(); // Callback
    void callFinishNewItem();
    // Used by the LLPanelContents to let this class to enable rename, sticky sort location
    // and open the supported window if need be.
    void onCreateInvDone(LLHandle<LLPanel> handle, const LLUUID& new_id, const void_callback_t& cb = nullptr);
    void startDNDInvToObject(const LLUUID& original_UUID, const void_callback_t& cb = nullptr); 
    void saveAndClearInventoryFlags(); // Saves the Show Inventory settings.xml flags and clears therm
    void restoreInventoryFlags();// Restores the Show Inventory settings.xml flags.

    EAddNewItemState getState() const { return mState; }
    void setState(const EAddNewItemState new_state) { mState = new_state; }
    bool checkState(const EAddNewItemState check_state) const { return mState == check_state; }
    bool isStateIdle() const { return mState == EAddNewItemState::IDLE; }

    void resetLatestCreationTime() { mLatestCreationTime = 0; }
    bool checkAgainstLatestObject(const LLPointer<LLInventoryObject>& obj);

    void setFinishNewItemCallback(void_callback_t cb) { mFinishNewItemCallback = cb; }

protected:

    EAddNewItemState   mState;              // Flags if there is a item that needs an action performed on it.
    LLAssetType::EType mItemAssetType;      // Stores the action type of the new item, used for idenifying the idem.
    LLUUID             mOriginalItemUUID;   // Stores the UUID of the item that was first passed to be transfered. UUID is changed when added to object's inventory
    LLUUID             mNewItemUUID;        // The UUID will change when moved so that cannot be used for tracking which item needs to be acted upon.
    time_t             mLatestCreationTime; // Stores the latest creation time, used for finding the object with the latest version.
    LLInventoryItem*   mLatestCreatedItem;  // Stores the pointer to the latest item found.
    void_callback_t    mFinishNewItemCallback; // Function pointer used as a callback for when the new item is finished.
    S32                mCurrentInventoryFlags;
};
// </FS:mjr> [FIRE-36685]
#endif
