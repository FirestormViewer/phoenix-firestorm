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

class LLButton;
class LLComboBox;
class LLCheckBoxCtrl;
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
        /*virtual*/ bool postBuild();
        virtual void onOpen(const LLSD& key);
        virtual void onClose(bool app_quitting);
        void updateList();
        void updateScrollListData();
        void updateSetParameters();
        void updateAnimationList();

        bool handleDragAndDrop(S32 x, S32 y, MASK mask, bool drop, EDragAndDropType cargo_type, void* cargo_data,
                                  EAcceptance* accept, std::string& tooltip_msg);

    protected:
        LLScrollListItem* addAnimation(const std::string& name);

        void onSelectSet();
        void onSelectSetSmall();
        void onRenameSet();
        void onSelectState();
        void onChangeAnimationSelection();
        void onClickReload();
        void onClickAdd();
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

// <AS:Chanayane> Double click on animation in AO
        void onDoubleClick();
// </AS:Chanayane>

        void onAnimationChanged(const LLUUID& animation);

        void reloading(bool reload);

        void updateSmart();
        void updateCycleParameters();

        void enableSetControls(bool enable);
        void enableStateControls(bool enable);

        bool newSetCallback(const LLSD& notification, const LLSD& response);
        bool removeSetCallback(const LLSD& notification, const LLSD& response);

        virtual bool tick();

        std::vector<AOSet*> mSetList;
        AOSet* mSelectedSet;
        AOSet::AOState* mSelectedState;

        LLPanel* mReloadCoverPanel;

        // Full interface

        LLPanel* mMainInterfacePanel;

        LLComboBox* mSetSelector;
        LLButton* mActivateSetButton;
        LLButton* mAddButton;
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
        LLButton* mTrashButton;
        LLCheckBoxCtrl* mCycleCheckBox;
        LLCheckBoxCtrl* mRandomizeCheckBox;
        LLTextBox* mCycleTimeTextLabel;
        LLSpinCtrl* mCycleTimeSpinner;

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
};

#endif // AO_H
