/**
 * @file llpanelcontents.cpp
 * @brief Object contents panel in the tools floater.
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

// file include
#include "llpanelcontents.h"

// linden library includes
#include "llerror.h"
#include "llfiltereditor.h"
#include "llfloaterreg.h"
#include "llfontgl.h"
#include "llinventorydefines.h"
#include "llmaterialtable.h"
#include "llpermissionsflags.h"
#include "llrect.h"
#include "llstring.h"
#include "llui.h"
#include "m3math.h"
#include "material_codes.h"

// project includes
#include "llagent.h"
#include "llpanelobjectinventory.h"
#include "llpreviewscript.h"
#include "llresmgr.h"
#include "llselectmgr.h"
#include "lltool.h"
#include "lltoolcomp.h"
#include "lltoolmgr.h"
#include "lltooldraganddrop.h" // <FS:FIRE-36059> For custom script template
#include "lltrans.h"
#include "llviewerassettype.h"
#include "llviewercontrol.h" // <FS:FIRE-36059> For custom script template
#include "llviewerinventory.h"
#include "llviewermenu.h" // <FS> Script reset in edit floater
#include "llviewerobject.h"
#include "llviewerregion.h"
#include "llviewerwindow.h"
#include "llworld.h"
#include "llfloaterperms.h"
// [RLVa:KB] - Checked: 2011-05-22 (RLVa-1.3.1a)
#include "rlvhandler.h"
#include "rlvlocks.h"
// [/RLVa:KB]
#include "fsnewitemctrl.h" // <FS:mjr> [FIRE-36685] - Toolbox Window - Add new notecard button to Content tab

//
// Imported globals
//


//
// Globals
//
const char* LLPanelContents::TENTATIVE_SUFFIX = "_tentative";
const char* LLPanelContents::PERMS_OWNER_INTERACT_KEY = "perms_owner_interact";
const char* LLPanelContents::PERMS_OWNER_CONTROL_KEY = "perms_owner_control";
const char* LLPanelContents::PERMS_GROUP_INTERACT_KEY = "perms_group_interact";
const char* LLPanelContents::PERMS_GROUP_CONTROL_KEY = "perms_group_control";
const char* LLPanelContents::PERMS_ANYONE_INTERACT_KEY = "perms_anyone_interact";
const char* LLPanelContents::PERMS_ANYONE_CONTROL_KEY = "perms_anyone_control";

bool LLPanelContents::postBuild()
{
    setMouseOpaque(false);

    childSetAction("button new script",&LLPanelContents::onClickNewScript, this);
    childSetAction("button new notecard", &LLPanelContents::onClickNewNotecard, this); // <FS:mjr> [FIRE-36685] - Toolbox Window - Add new notecard button to Content tab
    childSetAction("button permissions",&LLPanelContents::onClickPermissions, this);
    childSetAction("btn_reset_scripts", &LLPanelContents::onClickResetScripts, this); // <FS> Script reset in edit floater
    childSetAction("button refresh",&LLPanelContents::onClickRefresh, this);

    mFilterEditor = getChild<LLFilterEditor>("contents_filter");
    mFilterEditor->setCommitCallback([&](LLUICtrl*, const LLSD&) { onFilterEdit(); });

    mPanelInventoryObject = getChild<LLPanelObjectInventory>("contents_inventory");

    // update permission filter once UI is fully initialized
    mSavedFolderState.setApply(false);

    return true;
}

LLPanelContents::LLPanelContents()
    :   LLPanel(),
        mPanelInventoryObject(NULL)
{
}


LLPanelContents::~LLPanelContents()
{
    // Children all cleaned up by default view destructor.
}


void LLPanelContents::getState(LLViewerObject *objectp )
{
    if( !objectp )
    {
        getChildView("button new script")->setEnabled(false);
        getChildView("btn_reset_scripts")->setEnabled(false); // <FS> Script reset in edit floater
        // <FS:mjr> [FIRE-36685] - Toolbox Window - Add new notecard button to Content tab
        getChildView("button new notecard")->setEnabled(false);
        // </FS:mjr> [FIRE-36685]
        return;
    }

    LLUUID group_id;            // used for SL-23488
    LLSelectMgr::getInstance()->selectGetGroup(group_id);  // sets group_id as a side effect SL-23488

    // BUG? Check for all objects being editable?
    bool editable = gAgent.isGodlike()
                    || (objectp->permModify() && !objectp->isPermanentEnforced()
                           && ( objectp->permYouOwner() || ( !group_id.isNull() && gAgent.isInGroup(group_id) )));  // solves SL-23488
    bool all_volume = LLSelectMgr::getInstance()->selectionAllPCode( LL_PCODE_VOLUME );

// [RLVa:KB] - Checked: 2010-04-01 (RLVa-1.2.0c) | Modified: RLVa-1.0.5a
    if ( (rlv_handler_t::isEnabled()) && (editable) )
    {
        // Don't allow creation of new scripts if it's non-detachable
        if (objectp->isAttachment())
            editable = !gRlvAttachmentLocks.isLockedAttachment(objectp->getRootEdit());

        // Don't allow creation of new scripts if we're @unsit=n or @sittp=n restricted and we're sitting on the selection
        if ( (editable) && ((gRlvHandler.hasBehaviour(RLV_BHVR_UNSIT)) || (gRlvHandler.hasBehaviour(RLV_BHVR_SITTP))) )
        {
            // Only check the first (non-)root object because nothing else would result in enabling the button (see below)
            LLViewerObject* pObj = LLSelectMgr::getInstance()->getSelection()->getFirstRootObject(true);

            editable =
                (pObj) && (isAgentAvatarValid()) && ((!gAgentAvatarp->isSitting()) || (gAgentAvatarp->getRoot() != pObj->getRootEdit()));
        }
    }
// [/RLVa:KB]

    // Edit script buttons - ok if object is editable and there's an unambiguous destination for the object.
    // <FS:PP> FIRE-3219: Reset Scripts button in Build floater
    //  getChildView("button new script")->setEnabled(
    //      editable &&
    //      all_volume &&
    //      ((LLSelectMgr::getInstance()->getSelection()->getRootObjectCount() == 1)
    //          || (LLSelectMgr::getInstance()->getSelection()->getObjectCount() == 1)));

    bool objectIsOK = false;
    if( editable && all_volume && ( (LLSelectMgr::getInstance()->getSelection()->getRootObjectCount() == 1) || (LLSelectMgr::getInstance()->getSelection()->getObjectCount() == 1) ) )
    {
        objectIsOK = true;
    }

    getChildView("button new script")->setEnabled(objectIsOK);
    getChildView("button new notecard")->setEnabled(objectIsOK); // <FS:mjr> [FIRE-36685] - Toolbox Window - Add new notecard button to Content tab
    getChildView("btn_reset_scripts")->setEnabled(objectIsOK);
    // </FS:PP>

    getChildView("button permissions")->setEnabled(!objectp->isPermanentEnforced());
    mPanelInventoryObject->setEnabled(!objectp->isPermanentEnforced());
}

void LLPanelContents::onFilterEdit()
{
    const std::string& filter_substring = mFilterEditor->getText();
    if (!mPanelInventoryObject->hasInventory())
    {
        mDirtyFilter = true;
    }
    else
    {
        LLFolderView* root_folder = mPanelInventoryObject->getRootFolder();
        if (filter_substring.empty())
        {
            if (mPanelInventoryObject->getFilter().getFilterSubString().empty())
            {
                // The current filter and the new filter are empty, nothing to do
                return;
            }

            if (mDirtyFilter && !mSavedFolderState.hasOpenFolders())
            {
                if (root_folder)
                {
                    root_folder->setOpenArrangeRecursively(true, LLFolderViewFolder::ERecurseType::RECURSE_DOWN);
                }
            }
            else
            {
                mSavedFolderState.setApply(true);
                if (root_folder)
                {
                    root_folder->applyFunctorRecursively(mSavedFolderState);
                }
            }
            mDirtyFilter = false;

            // Add a folder with the current item to the list of previously opened folders
            if (root_folder)
            {
                LLOpenFoldersWithSelection opener;
                root_folder->applyFunctorRecursively(opener);
                root_folder->scrollToShowSelection();
            }
        }
        else if (mPanelInventoryObject->getFilter().getFilterSubString().empty())
        {
            // The first letter in search term, save existing folder open state
            if (!mPanelInventoryObject->getFilter().isNotDefault())
            {
                mSavedFolderState.setApply(false);
                if (root_folder)
                {
                    root_folder->applyFunctorRecursively(mSavedFolderState);
                }
                mDirtyFilter = false;
            }
        }
    }
    mPanelInventoryObject->getFilter().setFilterSubString(filter_substring);
}

void LLPanelContents::refresh()
{
    const bool children_ok = true;
    LLViewerObject* object = LLSelectMgr::getInstance()->getSelection()->getFirstRootObject(children_ok);

    getState(object);
    if (mPanelInventoryObject)
    {
        mPanelInventoryObject->refresh();
    }
}

void LLPanelContents::clearContents()
{
    if (mPanelInventoryObject)
    {
        mPanelInventoryObject->clearInventoryTask();
    }
}

//
// Static functions
//

// static
void LLPanelContents::onClickNewScript(void *userdata)
{
    // <FS:mjr> [FIRE-36685] - Toolbox Window - Add new notecard button to Content tab
    FSNewItemCtrl* new_item_ctrl = FSNewItemCtrl::getInstance();
    // Get the Panel Contents from the userdata void pointer.
    LLPanelContents* self = (LLPanelContents*)userdata;
    // </FS:mjr> [FIRE-36685]
    const bool children_ok = true;
    LLViewerObject* object = LLSelectMgr::getInstance()->getSelection()->getFirstRootObject(children_ok);
    if(object)
    {
// [RLVa:KB] - Checked: 2010-03-31 (RLVa-1.2.0c) | Modified: RLVa-1.0.5a
        if (rlv_handler_t::isEnabled()) // Fallback code [see LLPanelContents::getState()]
        {
            if (gRlvAttachmentLocks.isLockedAttachment(object->getRootEdit()))
            {
                return;                 // Disallow creating new scripts in a locked attachment
            }
            else if ( (gRlvHandler.hasBehaviour(RLV_BHVR_UNSIT)) || (gRlvHandler.hasBehaviour(RLV_BHVR_SITTP)) )
            {
                if ( (isAgentAvatarValid()) && (gAgentAvatarp->isSitting()) && (gAgentAvatarp->getRoot() == object->getRootEdit()) )
                    return;             // .. or in a linkset the avie is sitting on under @unsit=n/@sittp=n
            }
        }
// [/RLVa:KB]

        // <FS:PP> FIRE-36059 Optional custom script template for New Script button
        if (gSavedPerAccountSettings.getBOOL("FSBuildPrefs_UseCustomScript"))
        {
            if (LLUUID custom_script_id(gSavedPerAccountSettings.getString("FSBuildPrefs_CustomScriptItem")); custom_script_id.notNull())
            {
                if (auto custom_script = gInventory.getItem(custom_script_id); custom_script && custom_script->getType() == LLAssetType::AT_LSL_TEXT)
                {
                    // <FS:mjr> [FIRE-36685] - Toolbox Window - Add new notecard button to Content tab
                    // Flag the new script to also needs to be scrolled to and opened if needed.
                    new_item_ctrl->startDNDInvToObject(LLUUID::null, std::bind(&LLPanelContents::onFinishCreateItem, self));
                    // </FS:mjr> [FIRE-36685]
                    LLToolDragAndDrop::dropScript(object, custom_script, true, LLToolDragAndDrop::SOURCE_AGENT, gAgentID);
                    return;
                }
            }
        }
        // </FS:PP>

        LLPermissions perm;
        perm.init(gAgent.getID(), gAgent.getID(), LLUUID::null, LLUUID::null);

        // Parameters are base, owner, everyone, group, next
        perm.initMasks(
            PERM_ALL,
            PERM_ALL,
            LLFloaterPerms::getEveryonePerms("Scripts"),
            LLFloaterPerms::getGroupPerms("Scripts"),
            PERM_MOVE | LLFloaterPerms::getNextOwnerPerms("Scripts"));
        std::string desc;
        LLViewerAssetType::generateDescriptionFor(LLAssetType::AT_LSL_TEXT, desc);
        // <FS:mjr> [FIRE-36685] - Toolbox Window - Add new notecard button to Content tab
        // Flag the new script to also needs to be scrolled to and opened if needed.
        new_item_ctrl->startDNDInvToObject(LLUUID::null, std::bind(&LLPanelContents::onFinishCreateItem, self));
        // </FS:mjr> [FIRE-36685]
        LLPointer<LLViewerInventoryItem> new_item =
            new LLViewerInventoryItem(
                LLUUID::null,
                LLUUID::null,
                perm,
                LLUUID::null,
                LLAssetType::AT_LSL_TEXT,
                LLInventoryType::IT_LSL,
                "New Script",
                desc,
                LLSaleInfo::DEFAULT,
                LLInventoryItemFlags::II_FLAGS_NONE,
                time_corrected());
        object->saveScript(new_item, true, true);

        std::string name = new_item->getName();

        // *NOTE: In order to resolve SL-22177, we needed to create
        // the script first, and then you have to click it in
        // inventory to edit it.
        // *TODO: The script creation should round-trip back to the
        // viewer so the viewer can auto-open the script and start
        // editing ASAP.
    }
}

// static
void LLPanelContents::onClickPermissions(void *userdata)
{
    LLPanelContents* self = (LLPanelContents*)userdata;
    gFloaterView->getParentFloater(self)->addDependentFloater(LLFloaterReg::showInstance("bulk_perms"));
}

// <FS> Script reset in edit floater
// static
void LLPanelContents::onClickResetScripts(void *userdata)
{
    handle_selected_script_action("reset");
}
// </FS>

// static
void LLPanelContents::onClickRefresh(void *userdata)
{
    LLPanelContents* self = (LLPanelContents*)userdata;
    self->refresh();
}

// <FS:mjr> [FIRE-36685] - Toolbox Window - Add new notecard button to Content tab
// static
void LLPanelContents::onClickNewNotecard(void* userdata)
{
    FSNewItemCtrl* new_item_ctrl = FSNewItemCtrl::getInstance();
    // Get the Panel Contents from the userdata void pointer.
    LLPanelContents* self = (LLPanelContents*)userdata;
    // Maintain RLV support for notecard object type being added.
    const bool children_ok = true;
    LLViewerObject* object = LLSelectMgr::getInstance()->getSelection()->getFirstRootObject(children_ok);

    // [RLVa:KB] - Checked: 2010-03-31 (RLVa-1.2.0c) | Modified: RLVa-1.0.5a
    if (rlv_handler_t::isEnabled()) // Fallback code [see LLPanelContents::getState()]
    {
        if (gRlvAttachmentLocks.isLockedAttachment(object->getRootEdit()))
        {
            return; // Disallow creating new scripts in a locked attachment
        }
        else if ((gRlvHandler.hasBehaviour(RLV_BHVR_UNSIT)) || (gRlvHandler.hasBehaviour(RLV_BHVR_SITTP)))
        {
            if ((isAgentAvatarValid()) && (gAgentAvatarp->isSitting()) && (gAgentAvatarp->getRoot() == object->getRootEdit()))
                return; // .. or in a linkset the avie is sitting on under @unsit=n/@sittp=n
        }
    }
    // [/RLVa:KB]

    // Need to disable the new notecard button to prevent spaming the button and causing inventory issues
    self->getChildView("button new notecard")->setEnabled(false);
    // Need to save the filter first so it is not lost
    std::string save_filter = self->mFilterEditor->getText();
    // Clear the actual filter
    self->mFilterEditor->setText(LLStringExplicit(""));
    // Update the filter state
    self->onFilterEdit();
    // Create the LLSD paramater for the new notecard
    LLSD component("notecard");

    // Callback handle to allow for moving the new item to the object from the avatar inventory
    // Also moves the new item in the users avatar inventory to the trash.
    // Will also open up the dialog for Gesture and Notecards in the object's inventory.
    LLHandle<LLPanel> handle = self->getHandle();
    std::function<void(const LLUUID&)> callback_item_created = [handle](const LLUUID& new_id)
    {
        // Call the on Create Inventory Done of the new item controller which will kick off next steps
        FSNewItemCtrl::getInstance()->onCreateInvDone(handle, new_id, std::bind(&LLPanelContents::onFinishCreateItem, (LLPanelContents*)handle.get()));
    };

    // Save and clear the Show Inventroy flags used to prevent the floaters appearing
    // when items that have floaters like the Notecard, Gesture, Material and Script
    // from opening the temp item in the avatars inventory.
    new_item_ctrl->saveAndClearInventoryFlags();
    // Create the new item using the menu system in the user's avatar inventory.
    // The callback from above will move the item to the object and remove it from the user's inventory after.
    menu_create_inventory_item(NULL, LLUUID::null, component, LLUUID::null, callback_item_created);
    // Restore the filter
    self->mFilterEditor->setText(LLStringExplicit(save_filter));
}

// Callback method for when the item finishes
void LLPanelContents::onFinishCreateItem()
{
    getChildView("button new notecard")->setEnabled(true);
}
// </FS:mjr> [FIRE-36685]
