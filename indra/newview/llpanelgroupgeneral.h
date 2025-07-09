/**
 * @file llpanelgroupgeneral.h
 * @brief General information about a group.
 *
 * $LicenseInfo:firstyear=2006&license=viewerlgpl$
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

#ifndef LL_LLPANELGROUPGENERAL_H
#define LL_LLPANELGROUPGENERAL_H

#include "llpanelgroup.h"

#include <unordered_map>

class LLLineEditor;
class LLTextBox;
class LLTextureCtrl;
class LLTextEditor;
class LLButton;
class LLNameListCtrl;
class LLCheckBoxCtrl;
class LLComboBox;
class LLSpinCtrl;
class LLAvatarName;

class LLPanelGroupGeneral : public LLPanelGroupTab
{
public:
    LLPanelGroupGeneral();
    virtual ~LLPanelGroupGeneral();

    // LLPanelGroupTab
    virtual void activate();
    virtual bool needsApply(std::string& mesg);
    virtual bool apply(std::string& mesg);
    virtual void cancel();

    virtual void update(LLGroupChange gc);

    virtual bool postBuild();

    virtual void draw();

    virtual void setGroupID(const LLUUID& id);

    virtual void setupCtrls (LLPanel* parent);

    // <FS:Ansariel> Re-add group member list on general panel
    void onNameCache(const LLUUID& update_id, LLGroupMemberData* member, const LLAvatarName& av_name, const LLUUID& av_id);

    // <FS:Ansariel> FIRE-20149: Refresh insignia texture when clicking the refresh button
    void refreshInsigniaTexture();

// <FS> Copy button callbacks
protected:
    void onCopyURI();
    void onCopyName();
// </FS>

private:
    void    reset();

    void    resetDirty();

    static void onFocusEdit(LLFocusableElement* ctrl, void* data);
    static void onCommitAny(LLUICtrl* ctrl, void* data);
    static void onCommitUserOnly(LLUICtrl* ctrl, void* data);
    static void onCommitEnrollment(LLUICtrl* ctrl, void* data);
    static void onClickInfo(void* userdata);
    static void onReceiveNotices(LLUICtrl* ctrl, void* data);

    static bool joinDlgCB(const LLSD& notification, const LLSD& response);

    void updateChanged();
    bool confirmMatureApply(const LLSD& notification, const LLSD& response);

    bool            mChanged;
    bool            mFirstUse;
    std::string     mIncompleteMemberDataStr;

    // Group information (include any updates in updateChanged)
    LLLineEditor        *mGroupNameEditor;
    LLTextBox           *mFounderName;
    LLTextureCtrl       *mInsignia;
    LLTextEditor        *mEditCharter;

    // Options (include any updates in updateChanged)
    LLCheckBoxCtrl  *mCtrlShowInGroupList;
    LLCheckBoxCtrl  *mCtrlOpenEnrollment;
    LLCheckBoxCtrl  *mCtrlEnrollmentFee;
    LLSpinCtrl      *mSpinEnrollmentFee;
    LLCheckBoxCtrl  *mCtrlReceiveNotices;
    LLCheckBoxCtrl  *mCtrlListGroup;
    LLTextBox       *mActiveTitleLabel;
    LLComboBox      *mComboActiveTitle;
    LLComboBox      *mComboMature;
    LLCheckBoxCtrl  *mCtrlReceiveGroupChat; // <exodus/>

    LLUUID mIteratorGroup; // <FS:ND/> FIRE-6074; UUID of the group mMemberProgress belongs to.

    // <FS:Ansariel> For storing group name for copy name button
    std::string     mGroupName;

    // <FS:Ansariel> Re-add group member list on general panel
    static void openProfile(void* data);
    void addMember(LLGroupMemberData* member);
    void updateMembers();
    S32 sortMembersList(S32,const LLScrollListItem*,const LLScrollListItem*);

    LLGroupMgrGroupData::member_list_t::iterator mMemberProgress;
    typedef std::unordered_map<LLUUID, boost::signals2::connection> avatar_name_cache_connection_map_t;
    avatar_name_cache_connection_map_t mAvatarNameCacheConnections;

    bool            mPendingMemberUpdate;
    LLNameListCtrl* mListVisibleMembers;
    // </FS:Ansariel>
};

#endif
