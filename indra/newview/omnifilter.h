/**
 * @file omnifilter.h
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

#ifndef OMNIFILTER_H
#define OMNIFILTER_H

#include "omnifilterengine.h"

#include "lltextvalidate.h"
#include "llfloater.h"

class FSScrollListCtrl;
class LLButton;
class LLCheckBoxCtrl;
class LLComboBox;
class LLLineEditor;
class LLPanel;
class LLTextEditor;

class Omnifilter : public LLFloater
{
    friend class LLFloaterReg;
    friend class OmnifilterMenuPanel;

private:
    Omnifilter(const LLSD& key);
    void onVisibilityChange(bool visible) override;
    ~Omnifilter();

public:
    bool              postBuild() override final;
    LLScrollListItem* addNeedle(const std::string& name, const OmnifilterEngine::Needle& needle);
    // Supports Notecard drag and drop for importing.
    bool handleDragAndDrop(S32 x, S32 y, MASK mask, bool drop, EDragAndDropType cargo_type, void* cargo_data, EAcceptance* accept,
                           std::string& tooltip_msg);

protected:
    OmnifilterEngine::Needle* getSelectedNeedle();

    void onSelectNeedle();
    void onNeedleChanged();
    void onAddNeedleClicked();
    void onRemoveNeedleClicked();
    void onSortChanged();
    void onUpNeedleClicked();
    void onDownNeedleClicked();
    void onNewRuleSetClicked();
    void onCloneRuleSetClicked();
    void onRemoveRuleSetClicked();
    void onExportRuleSetClicked();
    void onImportRuleSetClicked();
    void onMatchDialogButtonLabelClicked();
    void onNewRuleSetNameSelectedCallback(const LLSD& notification, const LLSD& response);
    void onCloneRuleSetNameSelectedCallback(const LLSD& notification, const LLSD& response);
    void onRemoveRuleSetConfirmedCallback(const LLSD& notification, const LLSD& response);
    void onExportRuleSetConfirmedCallback(const LLSD& notification, const LLSD& response);
    void onImportRuleSetConfirmedCallback(const LLSD& notification, const LLSD& response);
    void onExportRuleSetNotecardCallback(const LLUUID &notecard_uuid);
    void onRuleSetChanged();
    void reloadRules();
    void reloadRule();
    void changeRuleSet(S32 new_rule_set_index);
    void onRuleSetsUpdated();
    void onNeedleNameChanged();
    void onNeedleCheckboxChanged(LLUICtrl* ctrl);
    void onOwnerChanged();

    void onLogLine(time_t time, const std::string& logLine);

    FSScrollListCtrl* mNeedleListCtrl{ nullptr };
    LLButton*         mAddNeedleBtn{ nullptr };
    LLButton*         mRemoveNeedleBtn{ nullptr };
    LLButton*         mUpNeedleBtn{ nullptr };
    LLButton*         mDownNeedleBtn{ nullptr };
    LLButton*         mExportRuleSetBtn{ nullptr };
    LLButton*         mImportRuleSetBtn{ nullptr };
    LLComboBox* mRuleSetsCmb{ nullptr };
    LLButton* mNewRuleSetBtn{ nullptr };
    LLButton* mCloneRuleSetBtn{ nullptr };
    LLButton* mRemoveRuleSetBtn{ nullptr };
    FSScrollListCtrl* mFilterLogCtrl{ nullptr };
    LLPanel*          mPanelDetails{ nullptr };
    LLLineEditor*     mNeedleNameCtrl{ nullptr };
    LLLineEditor*     mSenderNameCtrl{ nullptr };
    LLCheckBoxCtrl*   mSenderCaseSensitiveCheck{ nullptr };
    LLComboBox*       mSenderMatchTypeCombo{ nullptr };
    LLTextEditor*     mContentCtrl{ nullptr };
    LLCheckBoxCtrl*   mContentCaseSensitiveCheck{ nullptr };
    LLComboBox*       mContentMatchTypeCombo{ nullptr };
    LLButton*         mMatchDialogButtonLabelBtn { nullptr }; // Helper button to add the text "button_name=BUTTON_NAME" to the content editor.
    LLLineEditor*     mRegionNameCtrl{ nullptr };
    LLLineEditor*     mOwnerCtrl{ nullptr };

    LLButton* mTypeNearbyBtn{ nullptr };
    LLButton* mTypeIMBtn{ nullptr };
    LLButton* mTypeGroupIMBtn{ nullptr };
    LLButton* mTypeObjectChatBtn{ nullptr };
    LLButton* mTypeObjectIMBtn{ nullptr };
    LLButton* mTypeScriptErrorBtn{ nullptr };
    LLButton* mTypeDialogBtn{ nullptr };
    LLButton* mTypeOfferBtn{ nullptr };
    LLButton* mTypeInviteBtn{ nullptr };
    LLButton* mTypeLureBtn{ nullptr };
    LLButton* mTypeLoadURLBtn{ nullptr };
    LLButton* mTypeFriendshipOfferBtn{ nullptr };
    LLButton* mTypeTeleportRequestBtn{ nullptr };
    LLButton* mTypeGroupNoticeBtn{ nullptr };

    LLLineEditor* mChatReplaceCtrl{ nullptr };
    LLLineEditor* mButtonReplyCtrl{ nullptr };
    LLTextEditor* mTextBoxReplyCtrl{ nullptr };

    LLTextValidate::Validator mPrevalidator;
};

/// <summary>
/// Omnifilter Menu Panel - Used by panel_status_bar.xml's omnifilter_menu_panel
/// </summary>

class OmnifilterMenuPanel : public LLPanel
{
public:
    OmnifilterMenuPanel();
    /*virtual*/ bool postBuild();

    virtual ~OmnifilterMenuPanel();

protected:
    void onRuleSetChanged();
    void updateOmnifilterRuleSets(const LLSD& data);
    void reloadRules();
    void onRuleSetsUpdated();

    LLComboBox*       mRuleSetsCmb{ nullptr };
    boost::signals2::connection mControlConnection;
    boost::signals2::connection mRuleSetUpdatedConnection;
};

#endif // OMNIFILTER_H
