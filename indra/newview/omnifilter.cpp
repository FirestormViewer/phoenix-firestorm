/**
 * @file omnifilter.cpp
 * @brief The Omnifilter editor
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2025, Zi Ree @ Second Life
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

#include "omnifilter.h"

#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "llcombobox.h"
#include "lllineeditor.h"
#include "lltexteditor.h"
// <FS:minerjr> [FIRE-36763] - Add rule sets to Omnifilter
#include "llviewercontrol.h" // Needed for gSavedSettings
#include "llnotificationsutil.h" // Needed for Notifications
// </FS:minerjr> [FIRE-36763]

#include "fsscrolllistctrl.h"

static constexpr S32 NEEDLE_CHECK_COLUMN = 0;
static constexpr S32 NEEDLE_NAME_COLUMN = 1;

static constexpr S32 LOG_DATE_COLUMN = 0;
static constexpr S32 LOG_CONTENT_COLUMN = 1;

Omnifilter::Omnifilter(const LLSD& key) :
    LLFloater(key)
{
}

LLScrollListItem* Omnifilter::addNeedle(const std::string& needle_name, const OmnifilterEngine::Needle& needle)
{
    LLSD row;
    row["columns"][NEEDLE_CHECK_COLUMN]["column"] = "enabled";
    row["columns"][NEEDLE_CHECK_COLUMN]["type"] = "checkbox";
    row["columns"][NEEDLE_CHECK_COLUMN]["value"] = false;

    row["columns"][NEEDLE_NAME_COLUMN]["column"] = "needle_name";
    row["columns"][NEEDLE_NAME_COLUMN]["type"] = "text";
    row["columns"][NEEDLE_NAME_COLUMN]["value"] = needle_name;

    LLScrollListItem* item = mNeedleListCtrl->addElement(row);
    item->getColumn(NEEDLE_CHECK_COLUMN)->setValue(needle.mEnabled);

    LLScrollListCheck* scroll_list_check = dynamic_cast<LLScrollListCheck*>(item->getColumn(NEEDLE_CHECK_COLUMN));
    if (scroll_list_check)
    {
        LLCheckBoxCtrl* check_box = scroll_list_check->getCheckBox();
        check_box->setCommitCallback(boost::bind(&Omnifilter::onNeedleCheckboxChanged, this, _1));
    }

    mNeedleListCtrl->refreshLineHeight();

    return item;
}

OmnifilterEngine::Needle* Omnifilter::getSelectedNeedle()
{
    const LLScrollListItem* needle_item = mNeedleListCtrl->getFirstSelected();
    if (needle_item)
    {
        const LLScrollListCell* needle_name_cell = needle_item->getColumn(NEEDLE_NAME_COLUMN);
        if (needle_name_cell)
        {
            const std::string& needle_name = needle_name_cell->getValue().asString();
            // <FS:minerjr> [FIRE-36763] - Add rule sets to Omnifilter
            //if (!needle_name.empty())
            // Only try to access the needle if the name is not empty and it is on the current needle list, otherwise it will cause an error
            if (!needle_name.empty() && OmnifilterEngine::getInstance()->getNeedleList().contains(needle_name))
            // </FS:minerjr> [FIRE-36763]
            {
                return &OmnifilterEngine::getInstance()->getNeedleList().at(needle_name);
            }
        }
    }

    mPanelDetails->setVisible(false);
    mRemoveNeedleBtn->setEnabled(false);
    return nullptr;
}

void Omnifilter::onSelectNeedle()
{
    const OmnifilterEngine::Needle* needle = getSelectedNeedle();
    if (!needle)
    {
        mPanelDetails->setVisible(false);
        mRemoveNeedleBtn->setEnabled(false);
        return;
    }

    if (!mNeedleListCtrl->getItemCount())
    {
        mPanelDetails->setVisible(false);
        mRemoveNeedleBtn->setEnabled(false);
        return;
    }

    mPanelDetails->setVisible(true);
    mRemoveNeedleBtn->setEnabled(true);

    mNeedleNameCtrl->setText(mNeedleListCtrl->getSelectedItemLabel(NEEDLE_NAME_COLUMN));
    mSenderNameCtrl->setText(needle->mSenderName);
    mSenderCaseSensitiveCheck->setValue(!needle->mSenderNameCaseInsensitive);
    mSenderMatchTypeCombo->selectByValue(needle->mSenderNameMatchType);
    mContentCtrl->setText(needle->mContent);
    mContentCaseSensitiveCheck->setValue(!needle->mContentCaseInsensitive);
    mContentMatchTypeCombo->selectByValue(needle->mContentMatchType);
    mRegionNameCtrl->setText(needle->mRegionName);
    mOwnerCtrl->clear();
    if (needle->mOwnerID.notNull())
    {
        mOwnerCtrl->setText(needle->mOwnerID.asString());
    }
    onOwnerChanged();

    mTypeNearbyBtn->setToggleState(needle->mTypes.find(OmnifilterEngine::eType::NearbyChat) != needle->mTypes.end());
    mTypeIMBtn->setToggleState(needle->mTypes.find(OmnifilterEngine::eType::InstantMessage) != needle->mTypes.end());
    mTypeGroupIMBtn->setToggleState(needle->mTypes.find(OmnifilterEngine::eType::GroupChat) != needle->mTypes.end());
    mTypeObjectChatBtn->setToggleState(needle->mTypes.find(OmnifilterEngine::eType::ObjectChat) != needle->mTypes.end());
    mTypeObjectIMBtn->setToggleState(needle->mTypes.find(OmnifilterEngine::eType::ObjectInstantMessage) != needle->mTypes.end());
    mTypeScriptErrorBtn->setToggleState(needle->mTypes.find(OmnifilterEngine::eType::ScriptError) != needle->mTypes.end());
    mTypeDialogBtn->setToggleState(needle->mTypes.find(OmnifilterEngine::eType::ScriptDialog) != needle->mTypes.end());
    mTypeOfferBtn->setToggleState(needle->mTypes.find(OmnifilterEngine::eType::InventoryOffer) != needle->mTypes.end());
    mTypeInviteBtn->setToggleState(needle->mTypes.find(OmnifilterEngine::eType::GroupInvite) != needle->mTypes.end());
    mTypeLureBtn->setToggleState(needle->mTypes.find(OmnifilterEngine::eType::Lure) != needle->mTypes.end());
    mTypeLoadURLBtn->setToggleState(needle->mTypes.find(OmnifilterEngine::eType::URLRequest) != needle->mTypes.end());
    mTypeFriendshipOfferBtn->setToggleState(needle->mTypes.find(OmnifilterEngine::eType::FriendshipOffer) != needle->mTypes.end());
    mTypeTeleportRequestBtn->setToggleState(needle->mTypes.find(OmnifilterEngine::eType::TeleportRequest) != needle->mTypes.end());
    mTypeGroupNoticeBtn->setToggleState(needle->mTypes.find(OmnifilterEngine::eType::GroupNotice) != needle->mTypes.end());

    mChatReplaceCtrl->setText(needle->mChatReplace);
    mButtonReplyCtrl->setText(needle->mButtonReply);
    mTextBoxReplyCtrl->setText(needle->mTextBoxReply);

    mContentCtrl->setFocus(true);
}

void Omnifilter::onNeedleChanged()
{
    OmnifilterEngine::Needle* needle = getSelectedNeedle();
    if (!needle)
    {
        return;
    }

    needle->mSenderName = mSenderNameCtrl->getValue().asString();
    needle->mSenderNameCaseInsensitive = !mSenderCaseSensitiveCheck->getValue();
    needle->mSenderNameMatchType = static_cast<OmnifilterEngine::eMatchType>(mSenderMatchTypeCombo->getValue().asInteger());
    needle->mContent = mContentCtrl->getValue().asString();
    needle->mContentCaseInsensitive = !mContentCaseSensitiveCheck->getValue();
    needle->mContentMatchType = static_cast<OmnifilterEngine::eMatchType>(mContentMatchTypeCombo->getValue().asInteger());
    needle->mRegionName = mRegionNameCtrl->getValue().asString();
    needle->mOwnerID.set(mOwnerCtrl->getValue().asString());

    needle->mTypes.clear();

    if (mTypeNearbyBtn->getValue()) needle->mTypes.insert(OmnifilterEngine::eType::NearbyChat);
    if (mTypeIMBtn->getValue()) needle->mTypes.insert(OmnifilterEngine::eType::InstantMessage);
    if (mTypeGroupIMBtn->getValue()) needle->mTypes.insert(OmnifilterEngine::eType::GroupChat);
    if (mTypeObjectChatBtn->getValue()) needle->mTypes.insert(OmnifilterEngine::eType::ObjectChat);
    if (mTypeObjectIMBtn->getValue()) needle->mTypes.insert(OmnifilterEngine::eType::ObjectInstantMessage);
    if (mTypeScriptErrorBtn->getValue()) needle->mTypes.insert(OmnifilterEngine::eType::ScriptError);
    if (mTypeDialogBtn->getValue()) needle->mTypes.insert(OmnifilterEngine::eType::ScriptDialog);
    if (mTypeOfferBtn->getValue()) needle->mTypes.insert(OmnifilterEngine::eType::InventoryOffer);
    if (mTypeInviteBtn->getValue()) needle->mTypes.insert(OmnifilterEngine::eType::GroupInvite);
    if (mTypeLureBtn->getToggleState()) needle->mTypes.insert(OmnifilterEngine::eType::Lure);
    if (mTypeLoadURLBtn->getValue()) needle->mTypes.insert(OmnifilterEngine::eType::URLRequest);
    if (mTypeFriendshipOfferBtn->getValue()) needle->mTypes.insert(OmnifilterEngine::eType::FriendshipOffer);
    if (mTypeTeleportRequestBtn->getValue()) needle->mTypes.insert(OmnifilterEngine::eType::TeleportRequest);
    if (mTypeGroupNoticeBtn->getValue()) needle->mTypes.insert(OmnifilterEngine::eType::GroupNotice);

    needle->mChatReplace = mChatReplaceCtrl->getValue().asString();
    needle->mButtonReply = mButtonReplyCtrl->getValue().asString();
    needle->mTextBoxReply = mTextBoxReplyCtrl->getValue().asString();

    OmnifilterEngine::getInstance()->setDirty(true);
}

void Omnifilter::onAddNeedleClicked()
{
    std::string new_needle_string = getString("OmnifilterNewNeedle");
    if (!mNeedleListCtrl->selectItemByLabel(new_needle_string, true, NEEDLE_NAME_COLUMN))
    {
        OmnifilterEngine::Needle& new_needle = OmnifilterEngine::getInstance()->newNeedle(new_needle_string);
        addNeedle(new_needle_string, new_needle)->setSelected(true);
    }

    onSelectNeedle();
    mNeedleNameCtrl->setFocus(true);
}

void Omnifilter::onRemoveNeedleClicked()
{
    S32 index = mNeedleListCtrl->getItemIndex(mNeedleListCtrl->getFirstSelected());
    OmnifilterEngine::getInstance()->deleteNeedle(mNeedleListCtrl->getSelectedItemLabel(NEEDLE_NAME_COLUMN));

    mNeedleListCtrl->selectPrevItem();
    mNeedleListCtrl->deleteSingleItem(index);

    if (!mNeedleListCtrl->getNumSelected())
    {
        mNeedleListCtrl->selectFirstItem();
    }

    onSelectNeedle();
}

// <FS:minerjr> [FIRE-36649] - Add reordering to OmniFilter
// Handles re-ordering the list of needle names when the UI is sorted.
void Omnifilter::onSortChanged()
{
    static OmnifilterEngine* omni_filter_engine = OmnifilterEngine::getInstance();
    if (!omni_filter_engine)
    {
        return;
    }
    // Loop over the list of thems
    for (S32 index = 0; index < mNeedleListCtrl->getItemCount(); index++)
    {
        const LLScrollListItem* needle_item = mNeedleListCtrl->getAllData()[index];
        if (needle_item)
        {
            const LLScrollListCell* needle_name_cell = needle_item->getColumn(NEEDLE_NAME_COLUMN);
            if (needle_name_cell)
            {
                const std::string& needle_name = needle_name_cell->getValue().asString();
                if (!needle_name.empty())
                {
                    omni_filter_engine->setOrderedNeedleName(index, needle_name);
                }
            }
        }
    }
    // Flag as dirty the system will save the changes to the order.
    omni_filter_engine->setDirty(true);
}

// Moves an selected filter up on place
void Omnifilter::onUpNeedleClicked()
{
    static OmnifilterEngine* omni_filter_engine = OmnifilterEngine::getInstance();
    S32 current_index = mNeedleListCtrl->getFirstSelectedIndex();

    // Don't try to move the first item up.
    if (current_index > 0)
    {
        // Order is based upon 0 is the top of the screen in the list, so have to subtract 1 to get up visually.
        omni_filter_engine->swapNeedles(current_index, current_index - 1);
        mNeedleListCtrl->swapWithPrevious(current_index);
    }
}

void Omnifilter::onDownNeedleClicked()
{
    static OmnifilterEngine* omni_filter_engine = OmnifilterEngine::getInstance();
    S32 current_index = mNeedleListCtrl->getFirstSelectedIndex();

    // If the value is within range of the filter list, prevents from going past the end of the list
    if (omni_filter_engine->getOrderedNeedleListSize()  > 1 && current_index < omni_filter_engine->getOrderedNeedleListSize() - 1)
    {
        // Order is based upon 0 is the top of the screen in the list, so have to add 1 to get down visually.
        omni_filter_engine->swapNeedles(current_index, current_index + 1);
        mNeedleListCtrl->swapWithNext(current_index);
    }
}
// </FS:minerjr> [FIRE-36649]

// <FS:minerjr> [FIRE-36763] - Add rule sets to Omnifilter
// Callback method for the New Rule Set button when clicked.
// Creates an notification to the user to ask for the name of the new rule set.
void Omnifilter::onNewRuleSetClicked()
{
    LLSD args;
    // Default name of the new rule set
    args["RULESETNAME"] = "New Rule Set";
    // Display the new rule set notification and if the user presses the OK button, call the new rule set name selected callback method
    LLNotificationsUtil::add("OmniFilterNewRuleSet", args, LLSD(),
                             boost::bind(&Omnifilter::onNewRuleSetNameSelectedCallback, this, _1, _2));
}

// Callback method for the Clone Rule Set button when clicked.
// Creates an notification to the user to ask for the name of the clone rule set.
void Omnifilter::onCloneRuleSetClicked()
{
    LLSD args;
    // Default name of the cloned rule set
    args["RULESETNAME"] = "Cloned Rule Set";
    // Display the new rule set notification and if the user presses the OK button, call the new rule set name selected callback method
    LLNotificationsUtil::add("OmniFilterCloneRuleSet", args, LLSD(),
                             boost::bind(&Omnifilter::onCloneRuleSetNameSelectedCallback, this, _1, _2));
}

// Callback method for the Remove Rule Set button when clicked.
// Creates a notification to the user to ask if they are sure they want to remove the specified Rule Set.
void Omnifilter::onRemoveRuleSetClicked()
{
    static OmnifilterEngine* instance = OmnifilterEngine::getInstance();
    // IF the user tries to remove the Default rule set, send an error as that is not allowed.
    if (mRuleSetsCmb->getCurrentIndex() == 0)
    {
        LLNotificationsUtil::add("OmniFilterRemoveRuleSetDefault", LLSD(), LLSD());
    }
    // Else prompt the user with a confirmation notification if they really want to remove the 
    else
    {
        LLSD args;
        args["RULESETNAME"] = instance->getCurrentSelectedRuleSet();
        // Display the remove rule set notification and if the user presses the OK button, call the remove rule set confirm callback method
        LLNotificationsUtil::add("OmniFilterRemoveRuleSet", args, LLSD(),
                                 boost::bind(&Omnifilter::onRemoveRuleSetConfirmedCallback, this, _1, _2));
    }
}

// New Rule Set callback, which does the actual cloning.
void Omnifilter::onNewRuleSetNameSelectedCallback(const LLSD& notification, const LLSD& response)
{
    static OmnifilterEngine* instance = OmnifilterEngine::getInstance();
    S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
    if (option == 0) // YES
    {
        std::string new_name = response["new_name"].asString();
        // Try to perform the actual creating a new rule set. 
        S32 return_value = instance->addNewRuleSet(new_name);

        if (return_value == 1)
        {
            S32 new_rule_index = gSavedSettings.getS32("OmnifilterRuleSetID");
            // Add the new rule set to the Rule Set Drop Down
            mRuleSetsCmb->add(new_name, LLSD(new_rule_index));
            // Select the new rule set
            mRuleSetsCmb->setCurrentByIndex(new_rule_index);

            // Clear the Rule list
            mNeedleListCtrl->clear();
            mNeedleListCtrl->clearRows();

            // Add the new item.
            onAddNeedleClicked();

            // Save the state of the OmniFilter Window
            instance->setDirty(true);
        }
        // Else the user tried to create a new rule set with a blank name, so show an error.
        else if (return_value == -1)
        {
            LLNotificationsUtil::add("OmniFilterrNewRuleSetBlank", LLSD(), LLSD(), boost::bind(&Omnifilter::onNewRuleSetClicked, this));
        }
        // Else the user tried to create a new rule set with a duplicate name, so show an error.
        else if (return_value == 0)
        {
            LLNotificationsUtil::add("OmniFilterNewRuleSetDuplicate", LLSD(), LLSD(), boost::bind(&Omnifilter::onNewRuleSetClicked, this));
        }
    }
}

// Clone Rule Set callback, which does the actual cloning.
void Omnifilter::onCloneRuleSetNameSelectedCallback(const LLSD& notification, const LLSD& response)
{
    static OmnifilterEngine* instance = OmnifilterEngine::getInstance();
    S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
    if (option == 0) // YES
    {
        std::string new_name = response["new_name"].asString();
        // Try to perform the actual cloning of the rule set. 
        S32 return_value = instance->addClonedRuleSet(new_name);
        // If the cloning succeeded
        if (return_value == 1)
        {
            // Get the index of the added rule set.
            S32 new_rule_index = gSavedSettings.getS32("OmnifilterRuleSetID");
            // Add the new rule set to the Rule Set Drop Down
            mRuleSetsCmb->add(new_name, LLSD(new_rule_index));

            // Select the new rule set
            mRuleSetsCmb->setCurrentByIndex(new_rule_index);

            // Save the state of the OmniFilter Window
            instance->setDirty(true);
        }
        // Else the user tried to create a cloned rule set with a blank name, so show an error.
        else if (return_value == -1)
        {
            LLNotificationsUtil::add("OmniFilterCloneRuleSetBlank", LLSD(), LLSD(), boost::bind(&Omnifilter::onCloneRuleSetClicked, this));
        }
        // Else the user tried to create a cloned rule set with a duplicate name, so show an error.
        else if (return_value == 0)
        {
            LLNotificationsUtil::add("OmniFilterCloneRuleSetDuplicate", LLSD(), LLSD(), boost::bind(&Omnifilter::onCloneRuleSetClicked, this));
        }
    }
}

// Remove Rule Set callback, which does the actual removal.
void Omnifilter::onRemoveRuleSetConfirmedCallback(const LLSD& notification, const LLSD& response)
{
    static OmnifilterEngine* instance = OmnifilterEngine::getInstance();
    // Get the user response
    S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
    if (option == 0) // YES
    {
        std::string tooltip_msg;
        // Get the index of the current rule set being removed.
        S32 current_index = gSavedSettings.getS32("OmnifilterRuleSetID");
        // Try to perform the actual removal. 
        S32 return_value = instance->removeCurrentRuleSet();
        // If the removal succeeded
        if (return_value == 1)
        {
            // Remove the rule set from the Rule Set Drop Down
            mRuleSetsCmb->remove(current_index);
            // Reload the current rule set and switch the UI over to the Default rule set.
            reloadRule();
            mRuleSetsCmb->setCurrentByIndex(0);

            // Save the state of the OmniFilter Window
            instance->setDirty(true);
        }
        // Else the user tried to remove the default rule set, so show an error.
        else if (return_value == 0)
        {
            LLNotificationsUtil::add("OmniFilterRemoveRuleSetDefault", LLSD(), LLSD());
        }
        // Else the user tried to remove a rule set that index was out of bounds, so show an error.
        else if (return_value == -1)
        {
            LLSD args;
            args["OUTOFBOUNDS"] = current_index;
            LLNotificationsUtil::add("OmniFilterRemoveRuleSetOutofBounds", args, LLSD());
        }
        // Else the user tried to remove a rule set that had an name that does not exist in the map of rule sets, so show an error.
        else if (return_value == -2)
        {
            LLSD args;
            args["INVALIDNAME"] = instance->getCurrentSelectedRuleSet();
            LLNotificationsUtil::add("OmniFilterRemoveRuleSetInvalidName", args, LLSD());
        }
    }
}

// Rule Set Drop Down on commit callback method
void Omnifilter::onRuleSetChanged()
{
    static OmnifilterEngine* instance = OmnifilterEngine::getInstance();

    // Save the current
    // Write the current rule set back to the storeage
    instance->assignRuleSet(false);
    // Store the current combo box index as the Needle Preset Index
    gSavedSettings.setS32("OmnifilterRuleSetID", mRuleSetsCmb->getCurrentIndex());
    // Assign and reload the current rule
    instance->assignRuleSetNameFromSettings();
    
    instance->assignRuleSet(true);
    reloadRule();
}

// Reloads the full rule sets drop down widget (combobox)
void Omnifilter::reloadRules()
{
    static OmnifilterEngine* instance = OmnifilterEngine::getInstance();
    // Clear the current needles
    mRuleSetsCmb->clear();
    S32 current_rule_set_id = gSavedSettings.getS32("OmnifilterRuleSetID");
    for (S32 index = 0; index < instance->getOrderedRuleSetSize(); index++)
    {
        // Get the name from the ordered list of rule sets.
        const std::string needle_rule_set_name(instance->getOrderedRuleSetName(index));
        LLSD value(index);
        // And add to the UI element. The value is the value that is passed along with the commit callback method.
        mRuleSetsCmb->add(needle_rule_set_name, value);
    }
    // Finally, select the stored rule set
    mRuleSetsCmb->selectByValue(LLSD(current_rule_set_id));
}

// Reloads the actual rule set UI elements, including the Rule scroll list and editor panel
void Omnifilter::reloadRule()
{
    // Use static pointer for the instance so that don't have to keep requesting every time this code is touched.
    static OmnifilterEngine* instance = OmnifilterEngine::getInstance();
    // Clear the current needles
    mNeedleListCtrl->clearRows();
    mNeedleListCtrl->clear();
    
    //  Loop over the ordered list
    for (const auto& needle_name : instance->getOrderedNeedleList())
    {
        // Get the current needle and use the addNeedle method to add the the UI.
        const auto& needle = instance->getNeedleList()[needle_name];
        addNeedle(needle_name, needle);
    }

    mNeedleListCtrl->selectFirstItem();
    onSelectNeedle();
}

// </FS:minerjr> [FIRE-36763]

void Omnifilter::onNeedleNameChanged()
{
    const std::string& old_name = mNeedleListCtrl->getSelectedItemLabel(NEEDLE_NAME_COLUMN);
    const std::string& new_name = mNeedleNameCtrl->getValue().asString();

    if (old_name == new_name)
    {
        return;
    }

    const LLScrollListItem* needle_item = mNeedleListCtrl->getFirstSelected();

    const LLScrollListItem* name_check_item = mNeedleListCtrl->getItemByLabel(new_name, true, NEEDLE_NAME_COLUMN);
    if (name_check_item && name_check_item != needle_item)
    {
        mNeedleNameCtrl->setValue(old_name);
        return;
    }

    OmnifilterEngine::getInstance()->renameNeedle(old_name, new_name);

    LLScrollListCell* needle_name_column = needle_item->getColumn(NEEDLE_NAME_COLUMN);
    needle_name_column->setValue(new_name);

    onSelectNeedle();
}

void Omnifilter::onNeedleCheckboxChanged(LLUICtrl* ctrl)
{
    const LLCheckBoxCtrl* check = static_cast<LLCheckBoxCtrl*>(ctrl);
    OmnifilterEngine::Needle* needle = getSelectedNeedle();
    if (!needle)
    {
        return;
    }

    needle->mEnabled = check->getValue();
    OmnifilterEngine::getInstance()->setDirty(true);
}

void Omnifilter::onOwnerChanged()
{
    static LLColor4 default_text_color = mOwnerCtrl->getFgColor();

    if (LLUUID::validate(mOwnerCtrl->getValue().asString()))
    {
        mOwnerCtrl->setFgColor(default_text_color);
    }
    else
    {
        mOwnerCtrl->setFgColor(LLUIColorTable::getInstance()->getColor("EmphasisColor"));
    }
}

void Omnifilter::onLogLine(time_t time, const std::string& log_line)
{
    LLDate date((F64)time);

    LLSD substitution;
    substitution["datetime"] = date;

    std::string time_str = "[hour,datetime,slt]:[min,datetime,slt]:[second,datetime,slt]";
    LLStringUtil::format(time_str, substitution);

    LLSD row;
    row["columns"][LOG_DATE_COLUMN]["column"] = "timestamp";
    row["columns"][LOG_DATE_COLUMN]["type"] = "text";
    row["columns"][LOG_DATE_COLUMN]["value"] = time_str;

    row["columns"][LOG_CONTENT_COLUMN]["column"] = "log_entry";
    row["columns"][LOG_CONTENT_COLUMN]["type"] = "text";
    row["columns"][LOG_CONTENT_COLUMN]["value"] = log_line;

    bool scroll_to_end;
    scroll_to_end = mFilterLogCtrl->getScrollbar()->isAtEnd();

    LLScrollListItem* item = mFilterLogCtrl->addElement(row);
    if (scroll_to_end)
    {
        mFilterLogCtrl->setScrollPos(INT32_MAX);
    }

    time_str = "[year, datetime, slt]-[mthnum, datetime, slt]-[day, datetime, slt] [hour,datetime,slt]:[min,datetime,slt]:[second,datetime,slt] SLT";
    LLStringUtil::format(time_str, substitution);
    item->getColumn(LOG_DATE_COLUMN)->setToolTip(time_str);
}

bool Omnifilter::postBuild()
{
    mNeedleListCtrl = getChild<FSScrollListCtrl>("needle_list");
    mAddNeedleBtn = getChild<LLButton>("add_needle");
    mRemoveNeedleBtn = getChild<LLButton>("remove_needle");
    // <FS:minerjr> [FIRE-36649] - Add reordering to OmniFilter
    // Add the up and down buttons for re-aranging the order of the needles
    mUpNeedleBtn = getChild<LLButton>("up_needle");
    mDownNeedleBtn = getChild<LLButton>("down_needle");
    // </FS:minerjr> [FIRE-36649]
    mFilterLogCtrl = getChild<FSScrollListCtrl>("filter_log");
    mPanelDetails = getChild<LLPanel>("panel_details");
    mNeedleNameCtrl = getChild<LLLineEditor>("needle_name");
    mSenderNameCtrl = getChild<LLLineEditor>("sender_name");
    mSenderCaseSensitiveCheck = getChild<LLCheckBoxCtrl>("sender_case");
    mSenderMatchTypeCombo = getChild<LLComboBox>("sender_match_type");
    mContentCtrl = getChild<LLTextEditor>("content");
    // <FS:minerjr> [FIRE-36763] - Add rule sets to Omnifilter
    // Add the preset controls
    mRuleSetsCmb = getChild<LLComboBox>("cmb_rule_sets"); // Rule Set Drop Down
    mNewRuleSetBtn = getChild<LLButton>("btn_rule_set_new"); // New Rule Set
    mCloneRuleSetBtn = getChild<LLButton>("btn_rule_set_clone"); // Clone Rule Set
    mRemoveRuleSetBtn = getChild<LLButton>("btn_rule_set_remove"); // Remove Rule Set
    // <//FS:minerjr> [FIRE-36763]
    mContentCaseSensitiveCheck = getChild<LLCheckBoxCtrl>("content_case");
    mContentMatchTypeCombo = getChild<LLComboBox>("content_match_type");
    mRegionNameCtrl = getChild<LLLineEditor>("region_name");
    mOwnerCtrl = getChild<LLLineEditor>("owner_uuid");

    mTypeNearbyBtn = getChild<LLButton>("type_nearby");
    mTypeIMBtn = getChild<LLButton>("type_im");
    mTypeGroupIMBtn = getChild<LLButton>("type_group_im");
    mTypeObjectChatBtn = getChild<LLButton>("type_object_chat");
    mTypeObjectIMBtn = getChild<LLButton>("type_object_im");
    mTypeScriptErrorBtn = getChild<LLButton>("type_script_error");
    mTypeDialogBtn = getChild<LLButton>("type_dialog");
    mTypeOfferBtn = getChild<LLButton>("type_inventory_offer");
    mTypeInviteBtn = getChild<LLButton>("type_invite");
    mTypeLureBtn = getChild<LLButton>("type_lure");
    mTypeLoadURLBtn = getChild<LLButton>("type_load_url");
    mTypeFriendshipOfferBtn = getChild<LLButton>("type_friendship");
    mTypeTeleportRequestBtn = getChild<LLButton>("type_tp_request");
    mTypeGroupNoticeBtn = getChild<LLButton>("type_group_notice");

    mChatReplaceCtrl = getChild<LLLineEditor>("chat_replace");
    mButtonReplyCtrl = getChild<LLLineEditor>("button_reply");
    mTextBoxReplyCtrl = getChild<LLTextEditor>("text_box_reply");

    mNeedleListCtrl->setSearchColumn(NEEDLE_NAME_COLUMN);
    mNeedleListCtrl->deleteAllItems();
    mNeedleListCtrl->setCommitOnSelectionChange(true);

    mFilterLogCtrl->deleteAllItems();

    auto& instance = OmnifilterEngine::instance();
    // <FS:minerjr> [FIRE-36763] - Add rule sets to Omnifilter
    // Reload the rules set UI elements, uses the settings stored rule set ID
    reloadRules();
    instance.assignRuleSetNameFromSettings();
    instance.assignRuleSet(true);
    // </FS:minerjr> [FIRE-36763]
    // <FS:minerjr> [FIRE-36649] - Add reordering to OmniFilter
    //for (const auto& [needle_name, needle] : instance.getNeedleList())
    // Loop over the  ordered list 
    for (const auto& needle_name : instance.getOrderedNeedleList())
    {
        const auto& needle = instance.getNeedleList()[needle_name];
    // </FS:minerjr> [FIRE-36649]
        addNeedle(needle_name, needle);
    }

    for (const auto& [log_time, log_message] : instance.mLog)
    {
        onLogLine(log_time, log_message);
    }

    instance.mLogSignal.connect(boost::bind(&Omnifilter::onLogLine, this, _1, _2));

    if (mNeedleListCtrl->getItemCount())
    {
        mNeedleListCtrl->selectFirstItem();
    }

    mContentCtrl->setCommitOnFocusLost(true);
    mTextBoxReplyCtrl->setCommitOnFocusLost(true);

    mNeedleListCtrl->setCommitCallback(boost::bind(&Omnifilter::onSelectNeedle, this));
    mAddNeedleBtn->setCommitCallback(boost::bind(&Omnifilter::onAddNeedleClicked, this));
    mRemoveNeedleBtn->setCommitCallback(boost::bind(&Omnifilter::onRemoveNeedleClicked, this));
    // <FS:minerjr> [FIRE-36649] - Add reordering to OmniFilter
    // Add the callbacks for the up and down buttons to re-order the filter list
    mNeedleListCtrl->setSortChangedCallback(boost::bind(&Omnifilter::onSortChanged, this));
    mUpNeedleBtn->setCommitCallback(boost::bind(&Omnifilter::onUpNeedleClicked, this));
    mDownNeedleBtn->setCommitCallback(boost::bind(&Omnifilter::onDownNeedleClicked, this));
    // </FS:minerjr> [FIRE-36649]
    // <FS:minerjr> [FIRE-36763] - Add rule sets to Omnifilter
    // Add the Rule Set controls
    mRuleSetsCmb->setCommitCallback(boost::bind(&Omnifilter::onRuleSetChanged, this)); // Rule Set Drop Down
    mNewRuleSetBtn->setCommitCallback(boost::bind(&Omnifilter::onNewRuleSetClicked, this)); // New Rule Set
    mCloneRuleSetBtn->setCommitCallback(boost::bind(&Omnifilter::onCloneRuleSetClicked, this)); // Clone Rule Set
    mRemoveRuleSetBtn->setCommitCallback(boost::bind(&Omnifilter::onRemoveRuleSetClicked, this)); // Remove Rule Set
    // <//FS:minerjr> [FIRE-36763]
    mNeedleNameCtrl->setCommitCallback(boost::bind(&Omnifilter::onNeedleNameChanged, this));
    mSenderNameCtrl->setCommitCallback(boost::bind(&Omnifilter::onNeedleChanged, this));
    mSenderCaseSensitiveCheck->setCommitCallback(boost::bind(&Omnifilter::onNeedleChanged, this));
    mSenderMatchTypeCombo->setCommitCallback(boost::bind(&Omnifilter::onNeedleChanged, this));
    mContentCtrl->setCommitCallback(boost::bind(&Omnifilter::onNeedleChanged, this));
    mContentCaseSensitiveCheck->setCommitCallback(boost::bind(&Omnifilter::onNeedleChanged, this));
    mContentMatchTypeCombo->setCommitCallback(boost::bind(&Omnifilter::onNeedleChanged, this));
    mRegionNameCtrl->setCommitCallback(boost::bind(&Omnifilter::onNeedleChanged, this));
    mChatReplaceCtrl->setCommitCallback(boost::bind(&Omnifilter::onNeedleChanged, this));
    mButtonReplyCtrl->setCommitCallback(boost::bind(&Omnifilter::onNeedleChanged, this));
    mTextBoxReplyCtrl->setCommitCallback(boost::bind(&Omnifilter::onNeedleChanged, this));
    mOwnerCtrl->setCommitCallback(boost::bind(&Omnifilter::onNeedleChanged, this));
    mOwnerCtrl->setKeystrokeCallback(boost::bind(&Omnifilter::onOwnerChanged, this), nullptr);
    mTypeNearbyBtn->setCommitCallback(boost::bind(&Omnifilter::onNeedleChanged, this));
    mTypeIMBtn->setCommitCallback(boost::bind(&Omnifilter::onNeedleChanged, this));
    mTypeGroupIMBtn->setCommitCallback(boost::bind(&Omnifilter::onNeedleChanged, this));
    mTypeObjectChatBtn->setCommitCallback(boost::bind(&Omnifilter::onNeedleChanged, this));
    mTypeObjectIMBtn->setCommitCallback(boost::bind(&Omnifilter::onNeedleChanged, this));
    mTypeScriptErrorBtn->setCommitCallback(boost::bind(&Omnifilter::onNeedleChanged, this));
    mTypeDialogBtn->setCommitCallback(boost::bind(&Omnifilter::onNeedleChanged, this));
    mTypeOfferBtn->setCommitCallback(boost::bind(&Omnifilter::onNeedleChanged, this));
    mTypeInviteBtn->setCommitCallback(boost::bind(&Omnifilter::onNeedleChanged, this));
    mTypeLureBtn->setCommitCallback(boost::bind(&Omnifilter::onNeedleChanged, this));
    mTypeLoadURLBtn->setCommitCallback(boost::bind(&Omnifilter::onNeedleChanged, this));
    mTypeFriendshipOfferBtn->setCommitCallback(boost::bind(&Omnifilter::onNeedleChanged, this));
    mTypeTeleportRequestBtn->setCommitCallback(boost::bind(&Omnifilter::onNeedleChanged, this));
    mTypeGroupNoticeBtn->setCommitCallback(boost::bind(&Omnifilter::onNeedleChanged, this));

    onSelectNeedle();

    return true;
}
