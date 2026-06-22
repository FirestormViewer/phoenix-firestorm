/**
 * @file ao.h
 * @brief Anything concerning the Viewer Side Animation Overrider GUI
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 * Copyright (C) 2011, Zi Ree @ Second Life
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

#ifndef AO_H
#define AO_H

#include "lleventtimer.h"
#include "lltransientdockablefloater.h"
#include "aoset.h"

#include <map>
#include <set>

class LLButton;
class LLComboBox;
class LLCheckBoxCtrl;
class LLContextMenu;
class LLMenuButton;
class LLMenuItemCallGL;
class LLScrollListCtrl;
class LLScrollListItem;
class LLSpinCtrl;
class LLTextBox;

class FloaterAO
:   public LLTransientDockableFloater,
    public LLEventTimer
{
    friend class LLFloaterReg;

    private:
        FloaterAO(const LLSD& key);
        ~FloaterAO();

    public:
        bool postBuild() override;
        void onOpen(const LLSD& key) override;
        void onClose(bool app_quitting) override;
        void draw() override;
        void updateList();
        void updateSetParameters();
        void updateAnimationList();

        bool handleDragAndDrop(S32 x, S32 y, MASK mask, bool drop, EDragAndDropType cargo_type, void* cargo_data,
                                  EAcceptance* accept, std::string& tooltip_msg) override;

    protected:

        struct RowInfo
        {
            std::string mKind;          // "single", "group", "member", "trackmember"
            S32         mIndex{ -1 };
            S32         mMember{ -1 };  // member index within a group or track, -1 for the row itself
        };

        LLScrollListItem* addRow(LLScrollListCtrl* list, const std::string& icon, const std::string& name, const RowInfo& info, const std::string& tooltip, bool indented, bool bold);
        bool getRowInfo(LLScrollListItem* item, RowInfo& info) const;
        LLUUID getRowInventoryUUID(const RowInfo& info) const;
        static LLSD encodeRowValue(const RowInfo& info);

        void rebuildAnimationList();
        void rebuildTrackSelector();

        std::string expansionKey() const;
        bool isFolderExpanded(const LLUUID& folderID) const;
        void setFolderExpanded(const LLUUID& folderID, bool expanded);
        void requestListRebuild();

        void onSelectSet();
        void onSelectSetSmall();
        void onRenameSet();
        void onSelectState();
        void onChangeAnimationSelection();
        void onClickReload();
        void buildAddMenu();
        void onAddMenuShow();
        void onAddSet(bool clone);
        void onClickRemove();
        void onClickActivate();
        void onCheckDefault();
        void onCheckOverrideSits();
        void onCheckOverrideSitsSmall();
        void onCheckSmart();
        void onCheckDisableStands();
        void onClickMoveUp();
        void onClickMoveDown();
        void onClickTrash();
        void onCheckCycle();
        void onCheckRandomize();
        void onChangeCycleTime();
        void onClickPrevious();
        void onClickNext();

        void onClickMore();
        void onClickLess();
        void onDoubleClick();

        void onClickRename();
        bool renameGroupCallback(const LLSD& notification, const LLSD& response, S32 stepIndex);

        void onAnimationListRightClick(LLUICtrl* ctrl, S32 x, S32 y, MASK mask);
        void showContextMenu(LLScrollListCtrl* list, S32 x, S32 y);
        void onToggleRowExpand(const RowInfo& info);
        void onExtractMember(const RowInfo& info);
        void onDeleteRow(const RowInfo& info);
        void onRenameRow(const RowInfo& info);

        void onSelectTrackTab();
        void onClickAddTrack();
        void updateCycleControlValues();

        void onAnimationChanged(const LLUUID& animation);

        void reloading(bool reload);

        void updateSmart();
        void updateCycleParameters();

        void enableSetControls(bool enable);
        void enableStateControls(bool enable);

        bool newSetCallback(const LLSD& notification, const LLSD& response);
        bool removeSetCallback(const LLSD& notification, const LLSD& response);

        bool tick() override;

        std::vector<AOSet*> mSetList;
        AOSet* mSelectedSet;
        AOSet::AOState* mSelectedState;

        LLPanel* mReloadCoverPanel;

        // Full interface

        LLPanel* mMainInterfacePanel;

        LLComboBox* mSetSelector;
        LLButton* mActivateSetButton;
        LLMenuButton* mAddButton;
        LLMenuItemCallGL* mCloneSetItem;
        LLButton* mRemoveButton;
        LLCheckBoxCtrl* mDefaultCheckBox;
        LLCheckBoxCtrl* mOverrideSitsCheckBox;
        LLCheckBoxCtrl* mSmartCheckBox;
        LLCheckBoxCtrl* mDisableMouselookCheckBox;

        LLComboBox* mStateSelector;
        LLScrollListCtrl* mAnimationList;
        LLScrollListItem* mCurrentBoldItem;
        LLButton* mMoveUpButton;
        LLButton* mMoveDownButton;
        LLButton* mRenameButton;
        LLButton* mTrashButton;
        LLCheckBoxCtrl* mCycleCheckBox;
        LLCheckBoxCtrl* mRandomizeCheckBox;
        LLTextBox* mCycleTimeTextLabel;
        LLSpinCtrl* mCycleTimeSpinner;

        LLComboBox* mTrackSelector;
        LLButton* mTrackAddButton;
        LLPanel* mDropZonePanel;
        bool mDropZoneHovered;

        LLButton* mReloadButton;

        LLButton* mPreviousButton;
        LLButton* mNextButton;
        LLButton* mLessButton;

        // Small interface

        LLPanel* mSmallInterfacePanel;

        LLComboBox* mSetSelectorSmall;
        LLButton* mMoreButton;
        LLButton* mPreviousButtonSmall;
        LLButton* mNextButtonSmall;
        LLCheckBoxCtrl* mOverrideSitsCheckBoxSmall;

        bool mCanDragAndDrop;
        bool mImportRunning;
        bool mMore;

        bool mRebuildPending;
        S32 mCurrentTrack;                      // 0 = main state list, N = mTracks[N-1]
        std::string mLastSelectedStateName;     // "set|state" key, resets track tab on real switches
        std::string mPendingTrackSelection;     // track to select after the next reload
        std::string mLastSelectedRowValue;      // detects selection transitions for auto-expand
        std::map<std::string, std::set<LLUUID>> mExpandedFolders;
        LLHandle<LLContextMenu> mContextMenuHandle;

};

#endif // AO_H
