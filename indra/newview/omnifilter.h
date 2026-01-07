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

#include "llfloater.h"

class FSScrollListCtrl;
class LLButton;
class LLCheckBoxCtrl;
class LLComboBox;
class LLLineEditor;
class LLPanel;
class LLTextEditor;

class Omnifilter
:   public LLFloater
{
    friend class LLFloaterReg;

    private:
        Omnifilter(const LLSD& key);

    public:
        bool postBuild() override final;
        LLScrollListItem* addNeedle(const std::string& name, const OmnifilterEngine::Needle& needle);

    protected:
        OmnifilterEngine::Needle* getSelectedNeedle();

        void onSelectNeedle();
        void onNeedleChanged();
        void onAddNeedleClicked();
        void onRemoveNeedleClicked();
        void onNeedleNameChanged();
        void onNeedleCheckboxChanged(LLUICtrl* ctrl);
        void onOwnerChanged();

        void onLogLine(time_t time, const std::string& logLine);

        FSScrollListCtrl* mNeedleListCtrl{ nullptr };
        LLButton* mAddNeedleBtn{ nullptr };
        LLButton* mRemoveNeedleBtn{ nullptr };
        FSScrollListCtrl* mFilterLogCtrl{ nullptr };
        LLPanel* mPanelDetails{ nullptr };
        LLLineEditor* mNeedleNameCtrl{ nullptr };
        LLLineEditor* mSenderNameCtrl{ nullptr };
        LLCheckBoxCtrl* mSenderCaseSensitiveCheck{ nullptr };
        LLComboBox* mSenderMatchTypeCombo{ nullptr };
        LLTextEditor* mContentCtrl{ nullptr };
        LLCheckBoxCtrl* mContentCaseSensitiveCheck{ nullptr };
        LLComboBox* mContentMatchTypeCombo{ nullptr };
        LLLineEditor* mRegionNameCtrl{ nullptr };
        LLLineEditor* mOwnerCtrl{ nullptr };

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
};
#endif // OMNIFILTER_H
