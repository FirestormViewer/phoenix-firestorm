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
#include "fsnewitemctrl.h" // <FS:minerjr> [FIRE-36685] - Toolbox Window - Add dropdown of new items to Content tab

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

    //childSetAction("button new script",&LLPanelContents::onClickNewScript, this); // <FS:minerjr> [FIRE-36685] - Toolbox Window - Add dropdown of new items to Content tab
    childSetAction("button permissions",&LLPanelContents::onClickPermissions, this);
    childSetAction("btn_reset_scripts", &LLPanelContents::onClickResetScripts, this); // <FS> Script reset in edit floater
    childSetAction("button refresh",&LLPanelContents::onClickRefresh, this);

    mFilterEditor = getChild<LLFilterEditor>("contents_filter");
    mFilterEditor->setCommitCallback([&](LLUICtrl*, const LLSD&) { onFilterEdit(); });

    mPanelInventoryObject = getChild<LLPanelObjectInventory>("contents_inventory");

    // <FS:minerjr> [FIRE-36685] - Toolbox Window - Add dropdown of new items to Content tab
    // Get the Add item button and assign a callback method to actually add the selected item from
    // the set item combobox.
    mBTNAddItemToObject = getChild<LLButton>("btn_add_item");
    if (mBTNAddItemToObject)
    {
        mBTNAddItemToObject->setCommitCallback(boost::bind(&LLPanelContents::onClickAddItemToObject, this));
    }

    // Get the set item combobox and set the callback method to a method which sets up
    // the Add button to be able to use the correct item to add to the inventory of the object.
    mCMBSetItem = getChild<LLComboBox>("cmb_set_item");
    if (mCMBSetItem)
    {
        mCMBSetItem->setCommitCallback(boost::bind(&LLPanelContents::onCommitSetItem, this));
    }
    
    // Auto translate the new item labels. This is based upon the values from menu_inventory_add.xml
    // There are four new strings in strings.xml to support this.
    // Get he combobox's scroll list, to iterator over all the items
    LLScrollListCtrl* cmb_scrolllist = mCMBSetItem->getChild<LLScrollListCtrl>("ComboBox");
    if (cmb_scrolllist)
    {
        // Iterate over all the items in the scroll list. Need to use indexing due to limits
        for (S32 index = 0; index < cmb_scrolllist->getItemCount(); index++)
        {
            LLScrollListItem *cmb_item = cmb_scrolllist->getItemByIndex(index);
            // Get the translated version of the text column from the combobox item.
            // Used 2 columns per item, to support an Icon and Text cells.
            std::string translated = LLTrans::getString(cmb_item->getColumn(CMB_SET_ITEM_TEXT_COL)->getValue().asString());
            // Assign the translated text to the combo box value
            cmb_item->getColumn(CMB_SET_ITEM_TEXT_COL)->setValue(translated);
        }
    }
    
    // Force the combobox to update, this will update the label and icon to the correct version.
    // With more then 1 column, what ever value is stored on the first column will appear in the label
    // when a row is selcted. In the case of having an icon, the icon image name will appear.
    onCommitSetItem();
    // <FS:minerjr> [FIRE-36685]
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
        // <FS:minerjr> [FIRE-36685] - Toolbox Window - Add dropdown of new items to Content tab
        //getChildView("button new script")->setEnabled(false);
        getChildView("btn_reset_scripts")->setEnabled(false); // <FS> Script reset in edit floater
        // <FS:minerjr> [FIRE-36685] - Toolbox Window - Add dropdown of new items to Content tab
        getChildView("cmb_set_item")->setEnabled(false);
        getChildView("btn_add_item")->setEnabled(false);
        // Store a static pointer to the singleton FSNewItemCtrl so that it only has to be looked up once.
        static FSNewItemCtrl* new_item_ctrl = FSNewItemCtrl::getInstance();
        // Want to clear the saved New Item UUID as we may click on another object
        new_item_ctrl->setNewItemUUID(LLUUID::null);
        // </FS:minerjr> [FIRE-36685]
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

    // <FS:minerjr> [FIRE-36685] - Toolbox Window - Add dropdown of new items to Content tab
    //getChildView("button new script")->setEnabled(objectIsOK);
    getChildView("btn_reset_scripts")->setEnabled(objectIsOK);
    // </FS:PP>
    getChildView("cmb_set_item")->setEnabled(objectIsOK); 
    getChildView("btn_add_item")->setEnabled(objectIsOK);
    // </FS:minerjr> [FIRE-36685]

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
        // <FS:minerjr> [FIRE-36685] - Toolbox Window - Add dropdown of new items to Content tab
        // Store a static pointer to the singleton FSNewItemCtrl so that it only has to be looked up once.
        static FSNewItemCtrl* new_item_ctrl = FSNewItemCtrl::getInstance();
        // Set the Add Item to OBject button enabled state to only be enabled while
        // the do action state is IDLE.
        mBTNAddItemToObject->setEnabled(new_item_ctrl->isStateIdle());
        // </FS:minerjr> [FIRE-36685]
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
    // <FS:minerjr> [FIRE-36685] - Toolbox Window - Add dropdown of new items to Content tab
    // Store a static pointer to the singleton FSNewItemCtrl so that it only has to be looked up once.
    static FSNewItemCtrl* new_item_ctrl = FSNewItemCtrl::getInstance();
    // Get the Panel Contents from the userdata void pointer.
    LLPanelContents* self = (LLPanelContents*)userdata;
    // </FS:minerjr> [FIRE-36685]
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
                    // <FS:minerjr> [FIRE-36685] - Toolbox Window - Add dropdown of new items to Content tab
                    // Flag the new script to also needs to be scrolled to and opened if needed.
                    new_item_ctrl->startDNDInvToObject(LLUUID::null, std::bind(&LLPanelContents::onFinishCreateItem, self));
                    // </FS:minerjr> [FIRE-36685]
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
        // <FS:minerjr> [FIRE-36685] - Toolbox Window - Add dropdown of new items to Content tab
        // Flag the new script to also needs to be scrolled to and opened if needed.
        new_item_ctrl->startDNDInvToObject(LLUUID::null, std::bind(&LLPanelContents::onFinishCreateItem, self));
        // </FS:minerjr> [FIRE-36685]
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

// <FS:minerjr> [FIRE-36685] - Toolbox Window - Add dropdown of new items to Content tab
// Handle the selection change of the Set Item comboxbox, used to flag which type of new item
// to add to the selected object in the Build Window
void LLPanelContents::onCommitSetItem()
{
    if (mCMBSetItem)
    {
        // Get the selected index of the object component combobox
        S32 selected_index = mCMBSetItem->getCurrentIndex();

        // Store pointers to the combobox scroll list(used to get the information on the button to be created
        LLScrollListCtrl* cmb_scrolllist = mCMBSetItem->getChild<LLScrollListCtrl>("ComboBox");
        // Get the main combobox button, as we need to update it's icon and text manually, as
        // when you use icon's, the label gets filled out with the name of the icon and not the text.
        LLButton* cmb_button = mCMBSetItem->getChild<LLButton>("cmb_set_item_drop_down");

        if (cmb_scrolllist && cmb_button)
        {
            // Now get the actual selected item from the selected index
            LLScrollListItem* selected_item = cmb_scrolllist->getItemByIndex(selected_index);
            if (selected_item)
            {
                // We stored the commands to be run in the value of the combo_box.item
                std::string value = selected_item->getValue().asString();

                // Skip over empty selections (Should not happen as they are disabled, but just in case)
                if (!value.empty())
                {
                    // Create a string stream to parse out the commands, used spaces in the xml value field, so we can do this.
                    std::stringstream parser_stream(value);
                    // Clear all the current commands and parameters
                    mCMBCommand = "";
                    mCMBItemName = "";
                    mCMBLabel = "";

                    // Read in each command one at a time as long as the command succeeds, will contine to the next token.
                    if (parser_stream >> mCMBCommand)
                    {
                        // Get the item name. Used for picking the right type of item
                        parser_stream >> mCMBItemName;
                    }

                    // The icon information is stored on the 1st column of the combobox item.
                    cmb_button->setImageOverlay(selected_item->getColumn(0)->getValue().asString(), LLFontGL::LEFT);
                    // Added a second column to the combobox item to store the actual label text
                    // Need to use this as the default behavior is to take the first column and use it's value as the
                    // label text, but to use the icon, you have to set the first columns value to the icon name.
                    // This seems to be a bug in the underlying implimentation of the Combobox.
                    // This acts as work around.
                    mCMBLabel = selected_item->getColumn(1)->getValue().asString();
                    cmb_button->setLabel(mCMBLabel);
                }
            }
        }
    }
}

// Adds an item from the Combobox Set Item
void LLPanelContents::onClickAddItemToObject()
{
    // <FS:minerjr> [FIRE-36685] - Toolbox Window - Add dropdown of new items to Content tab
    // Store a static pointer to the singleton FSNewItemCtrl so that it only has to be looked up once.
    static FSNewItemCtrl* new_item_ctrl = FSNewItemCtrl::getInstance();
    // </FS:minerjr> [FIRE-36685]
    // Making this a special first check as it should be the most frequent command and uses the orginal
    // method as is.
    if (mCMBCommand == "NewScript")
    {
        // Need to disable the add button to prevent spaming the button and causing inventory issues
        mBTNAddItemToObject->setEnabled(false);
        return LLPanelContents::onClickNewScript(this);
    }

    // Maintain RLV support for other object types being added. Added here as the LLPanelContents::onClickNewScript() already performs RLV checks so don't duplicate work.
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

    if (mCMBCommand == "DoCreate")
    {
        // Need to disable the add button to prevent spaming the button and causing inventory issues
        mBTNAddItemToObject->setEnabled(false);
        // Disable the Add button
        // <FS:Ansariel> FIRE-20108: Can't create new folder in secondary inventory if view is filtered
        // reset_inventory_filter();
        // Need to save the filter first so it is not lost
        std::string save_filter = mFilterEditor->getText();
        // Clear the actual filter
        mFilterEditor->setText(LLStringExplicit(""));
        // Update the filter state
        onFilterEdit();
        // </FS:Ansariel>
        // Create the LLSD paramater from the combobox item name parsed out
        LLSD component(mCMBItemName);

        // Callback handle to allow for moving the new item to the object from the avatar inventory
        // Also moves the new item in the users avatar inventory to the trash.
        // Will also open up the dialog for Gesture and Notecards in the object's inventory.
        LLHandle<LLPanel> handle = getHandle();
        std::function<void(const LLUUID&)> callback_item_created =
            [handle](const LLUUID& new_id)
            {
                // Call the on Create Inventory Done of the new item controller which will kick off next steps
                new_item_ctrl->onCreateInvDone(handle, new_id, std::bind(&LLPanelContents::onFinishCreateItem, (LLPanelContents*)handle.get()));
            };

        // Save and clear the Show Inventroy flags used to prevent the floaters appearing
        // when items that have floaters like the Notecard, Gesture, Material and Script
        // from opening the temp item in the avatars inventory.
        new_item_ctrl->saveAndClearInventoryFlags();
        // Create the new item using the menu system in the user's avatar inventory.
        // The callback from above will move the item to the object and remove it from the user's inventory after.
        menu_create_inventory_item(NULL, LLUUID::null, component, LLUUID::null, callback_item_created);
        // Restore the filter
        mFilterEditor->setText(LLStringExplicit(save_filter));
    }
}

// Callback method for when the item finishes
void LLPanelContents::onFinishCreateItem()
{
    mBTNAddItemToObject->setEnabled(true);
}
// </FS:minerjr> [FIRE-36685]
