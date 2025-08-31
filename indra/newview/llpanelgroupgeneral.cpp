/**
 * @file llpanelgroupgeneral.cpp
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

#include "llviewerprecompiledheaders.h"

#include "llpanelgroupgeneral.h"

#include "llavatarnamecache.h"
#include "llagent.h"
#include "llagentbenefits.h"
#include "llsdparam.h"
#include "lluictrlfactory.h"
#include "roles_constants.h"

// UI elements
#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "llcombobox.h"
#include "lldbstrings.h"
#include "llavataractions.h"
#include "llgroupactions.h"
#include "lllineeditor.h"
#include "llnamelistctrl.h"
#include "llnotificationsutil.h"
#include "llscrolllistitem.h"
#include "llspinctrl.h"
#include "llslurl.h"
#include "lltextbox.h"
#include "lltexteditor.h"
#include "lltexturectrl.h"
#include "lltrans.h"
#include "llviewerwindow.h"

// Firestorm includes
#include "exogroupmutelist.h"
#include "fsnamelistavatarmenu.h"
#include "llclipboard.h"
#include "lleconomy.h" // <FS:AW FIRE-7091 group creation cost inaccurate on opensim>
#include "llurlaction.h"
#include "llviewermenu.h"

static LLPanelInjector<LLPanelGroupGeneral> t_panel_group_general("panel_group_general");

// consts
const S32 MATURE_CONTENT = 1;
const S32 NON_MATURE_CONTENT = 2;
const S32 DECLINE_TO_STATE = 0;

// <FS:Ansariel> Re-add group member list on general panel
static F32 sSDTime = 0.0f;
static F32 sElementTime = 0.0f;
static F32 sAllTime = 0.0f;
// </FS:Ansariel>

LLPanelGroupGeneral::LLPanelGroupGeneral()
:   LLPanelGroupTab(),
    mChanged(false),
    mFirstUse(true),
    mGroupNameEditor(NULL),
    mFounderName(NULL),
    mInsignia(NULL),
    mEditCharter(NULL),
    mCtrlShowInGroupList(NULL),
    mComboMature(NULL),
    mCtrlOpenEnrollment(NULL),
    mCtrlEnrollmentFee(NULL),
    mSpinEnrollmentFee(NULL),
    mCtrlReceiveNotices(NULL),
    mCtrlListGroup(NULL),
    mActiveTitleLabel(NULL),
    mComboActiveTitle(NULL),
    mCtrlReceiveGroupChat(NULL), // <exodus/>
    // <FS:Ansariel> Re-add group member list on general panel
    mPendingMemberUpdate(false),
    mListVisibleMembers(NULL)
    // </FS:Ansariel>
{

}

LLPanelGroupGeneral::~LLPanelGroupGeneral()
{
    // <FS:Ansariel> Re-add group member list on general panel
    for (avatar_name_cache_connection_map_t::iterator it = mAvatarNameCacheConnections.begin(); it != mAvatarNameCacheConnections.end(); ++it)
    {
        if (it->second.connected())
        {
            it->second.disconnect();
        }
    }
    mAvatarNameCacheConnections.clear();
    // </FS:Ansariel>
}

bool LLPanelGroupGeneral::postBuild()
{
    constexpr bool recurse = true;

    mEditCharter = getChild<LLTextEditor>("charter", recurse);
    if(mEditCharter)
    {
        mEditCharter->setCommitCallback(onCommitAny, this);
        mEditCharter->setFocusReceivedCallback(boost::bind(onFocusEdit, _1, this));
        mEditCharter->setFocusChangedCallback(boost::bind(onFocusEdit, _1, this));
        mEditCharter->setContentTrusted(false);
    }
    // <FS> set up callbacks for copy URI and name buttons
    childSetCommitCallback("copy_uri", boost::bind(&LLPanelGroupGeneral::onCopyURI, this), NULL);
    childSetCommitCallback("copy_name", boost::bind(&LLPanelGroupGeneral::onCopyName, this), NULL);
    childSetEnabled("copy_name", false);
    // </FS>

    // <FS:Ansariel> Re-add group member list on general panel
    mListVisibleMembers = getChild<LLNameListCtrl>("visible_members", recurse);
    if (mListVisibleMembers)
    {
        mListVisibleMembers->setDoubleClickCallback(openProfile, this);
        // <FS:Ansariel> Special Firestorm menu also allowing multi-select action
        //mListVisibleMembers->setContextMenu(LLScrollListCtrl::MENU_AVATAR);
        mListVisibleMembers->setContextMenu(&gFSNameListAvatarMenu);

        mListVisibleMembers->setSortCallback(boost::bind(&LLPanelGroupGeneral::sortMembersList,this,_1,_2,_3));
    }
    // </FS:Ansariel>

    // Options
    mCtrlShowInGroupList = getChild<LLCheckBoxCtrl>("show_in_group_list", recurse);
    if (mCtrlShowInGroupList)
    {
        mCtrlShowInGroupList->setCommitCallback(onCommitAny, this);
    }

    mComboMature = getChild<LLComboBox>("group_mature_check", recurse);
    if(mComboMature)
    {
        mComboMature->setCurrentByIndex(0);
        mComboMature->setCommitCallback(onCommitAny, this);
        if (gAgent.isTeen())
        {
            // Teens don't get to set mature flag. JC
            mComboMature->setVisible(false);
            mComboMature->setCurrentByIndex(NON_MATURE_CONTENT);
        }
    }
    mCtrlOpenEnrollment = getChild<LLCheckBoxCtrl>("open_enrollement", recurse);
    if (mCtrlOpenEnrollment)
    {
        mCtrlOpenEnrollment->setCommitCallback(onCommitAny, this);
    }

    mCtrlEnrollmentFee = getChild<LLCheckBoxCtrl>("check_enrollment_fee", recurse);
    if (mCtrlEnrollmentFee)
    {
        mCtrlEnrollmentFee->setCommitCallback(onCommitEnrollment, this);
    }

    mSpinEnrollmentFee = getChild<LLSpinCtrl>("spin_enrollment_fee", recurse);
    if (mSpinEnrollmentFee)
    {
        mSpinEnrollmentFee->setCommitCallback(onCommitAny, this);
        mSpinEnrollmentFee->setPrecision(0);
        mSpinEnrollmentFee->resetDirty();
    }

    bool accept_notices = false;
    bool list_in_profile = false;
    LLGroupData data;
    if(gAgent.getGroupData(mGroupID,data))
    {
        accept_notices = data.mAcceptNotices;
        list_in_profile = data.mListInProfile;
    }
    // <FS:Ansariel> Groupdata debug
    else
    {
        LL_INFOS("Agent_GroupData") << "GROUPDEBUG: Group panel: No agent group data for group " << mGroupID.asString() << LL_ENDL;
    }
    // </FS:Ansariel>
    mCtrlReceiveNotices = getChild<LLCheckBoxCtrl>("receive_notices", recurse);
    if (mCtrlReceiveNotices)
    {
        mCtrlReceiveNotices->setCommitCallback(onCommitUserOnly, this);
        mCtrlReceiveNotices->set(accept_notices);
        mCtrlReceiveNotices->setEnabled(data.mID.notNull());
    }

    // <exodus>
    mCtrlReceiveGroupChat = getChild<LLCheckBoxCtrl>("receive_chat", recurse);
    if(mCtrlReceiveGroupChat)
    {
        mCtrlReceiveGroupChat->setCommitCallback(onCommitUserOnly, this);
        mCtrlReceiveGroupChat->setEnabled(data.mID.notNull());
        if(data.mID.notNull())
        {
            mCtrlReceiveGroupChat->set(!exoGroupMuteList::instance().isMuted(data.mID));
        }
    }
    // </exodus>

    mCtrlListGroup = getChild<LLCheckBoxCtrl>("list_groups_in_profile", recurse);
    if (mCtrlListGroup)
    {
        mCtrlListGroup->setCommitCallback(onCommitUserOnly, this);
        mCtrlListGroup->set(list_in_profile);
        mCtrlListGroup->setEnabled(data.mID.notNull());
        mCtrlListGroup->resetDirty();
    }

    mActiveTitleLabel = getChild<LLTextBox>("active_title_label", recurse);

    mComboActiveTitle = getChild<LLComboBox>("active_title", recurse);
    if (mComboActiveTitle)
    {
        mComboActiveTitle->setCommitCallback(onCommitAny, this);
    }

    mIncompleteMemberDataStr = getString("incomplete_member_data_str");

    // If the group_id is null, then we are creating a new group
    if (mGroupID.isNull())
    {
        mEditCharter->setEnabled(true);

        mCtrlShowInGroupList->setEnabled(true);
        mComboMature->setEnabled(true);
        mCtrlOpenEnrollment->setEnabled(true);
        mCtrlEnrollmentFee->setEnabled(true);
        mSpinEnrollmentFee->setEnabled(true);

    }

    return LLPanelGroupTab::postBuild();
}

void LLPanelGroupGeneral::setupCtrls(LLPanel* panel_group)
{
    mInsignia = getChild<LLTextureCtrl>("insignia");
    if (mInsignia)
    {
        mInsignia->setCommitCallback(onCommitAny, this);
        mInsignia->setAllowLocalTexture(false);
        mInsignia->setBakeTextureEnabled(false);
    }
    mFounderName = getChild<LLTextBox>("founder_name");


    mGroupNameEditor = panel_group->getChild<LLLineEditor>("group_name_editor");
    mGroupNameEditor->setPrevalidate( LLTextValidate::validateASCIINoLeadingSpace );


}

// static
void LLPanelGroupGeneral::onFocusEdit(LLFocusableElement* ctrl, void* data)
{
    LLPanelGroupGeneral* self = (LLPanelGroupGeneral*)data;
    self->updateChanged();
    self->notifyObservers();
}

// static
void LLPanelGroupGeneral::onCommitAny(LLUICtrl* ctrl, void* data)
{
    LLPanelGroupGeneral* self = (LLPanelGroupGeneral*)data;
    self->updateChanged();
    self->notifyObservers();
}

// static
void LLPanelGroupGeneral::onCommitUserOnly(LLUICtrl* ctrl, void* data)
{
    LLPanelGroupGeneral* self = (LLPanelGroupGeneral*)data;
    self->mChanged = true;
    self->notifyObservers();
}


// static
void LLPanelGroupGeneral::onCommitEnrollment(LLUICtrl* ctrl, void* data)
{
    onCommitAny(ctrl, data);

    LLPanelGroupGeneral* self = (LLPanelGroupGeneral*)data;
    // Make sure both enrollment related widgets are there.
    if (!self->mCtrlEnrollmentFee || !self->mSpinEnrollmentFee)
    {
        return;
    }

    // Make sure the agent can change enrollment info.
    if (!gAgent.hasPowerInGroup(self->mGroupID,GP_MEMBER_OPTIONS)
        || !self->mAllowEdit)
    {
        return;
    }

    if (self->mCtrlEnrollmentFee->get())
    {
        self->mSpinEnrollmentFee->setEnabled(true);
    }
    else
    {
        self->mSpinEnrollmentFee->setEnabled(false);
        self->mSpinEnrollmentFee->set(0);
    }
}

// static
void LLPanelGroupGeneral::onClickInfo(void *userdata)
{
    LLPanelGroupGeneral *self = (LLPanelGroupGeneral *)userdata;

    if ( !self ) return;

    LL_DEBUGS() << "open group info: " << self->mGroupID << LL_ENDL;

    LLGroupActions::show(self->mGroupID);

}

bool LLPanelGroupGeneral::needsApply(std::string& mesg)
{
    updateChanged();
    mesg = getString("group_info_unchanged");
    return mChanged || mGroupID.isNull();
}

void LLPanelGroupGeneral::activate()
{
    LLGroupMgrGroupData* gdatap = LLGroupMgr::getInstance()->getGroupData(mGroupID);
    if (mGroupID.notNull()
        && (!gdatap || mFirstUse))
    {
        LLGroupMgr::getInstance()->sendGroupTitlesRequest(mGroupID);
        LLGroupMgr::getInstance()->sendGroupPropertiesRequest(mGroupID);

        if (!gdatap || !gdatap->isMemberDataComplete() )
        {
            LLGroupMgr::getInstance()->sendCapGroupMembersRequest(mGroupID);
        }

        mFirstUse = false;
    }
    mChanged = false;

    update(GC_ALL);
}

void LLPanelGroupGeneral::draw()
{
    LLPanelGroupTab::draw();

    // <FS:Ansariel> Re-add group member list on general panel
    if (mPendingMemberUpdate)
    {
        updateMembers();
    }
    // </FS:Ansariel>
}

bool LLPanelGroupGeneral::apply(std::string& mesg)
{
    if (mGroupID.isNull())
    {
        return false;
    }

    if (!mGroupID.isNull() && mAllowEdit && mComboActiveTitle && mComboActiveTitle->isDirty())
    {
        LLGroupMgr::getInstance()->sendGroupTitleUpdate(mGroupID,mComboActiveTitle->getCurrentID());
        update(GC_TITLES);
        mComboActiveTitle->resetDirty();
    }

    bool has_power_in_group = gAgent.hasPowerInGroup(mGroupID,GP_GROUP_CHANGE_IDENTITY);

    if (has_power_in_group)
    {
        LL_INFOS() << "LLPanelGroupGeneral::apply" << LL_ENDL;

        // Check to make sure mature has been set
        if(mComboMature &&
           mComboMature->getCurrentIndex() == DECLINE_TO_STATE)
        {
            LLNotificationsUtil::add("SetGroupMature", LLSD(), LLSD(),
                                            boost::bind(&LLPanelGroupGeneral::confirmMatureApply, this, _1, _2));
            return false;
        }

        LLGroupMgrGroupData* gdatap = LLGroupMgr::getInstance()->getGroupData(mGroupID);
        if (!gdatap)
        {
            mesg = LLTrans::getString("NoGroupDataFound");
            mesg.append(mGroupID.asString());
            return false;
        }
        bool can_change_ident = false;
        bool can_change_member_opts = false;
        can_change_ident = gAgent.hasPowerInGroup(mGroupID,GP_GROUP_CHANGE_IDENTITY);
        can_change_member_opts = gAgent.hasPowerInGroup(mGroupID,GP_MEMBER_OPTIONS);

        if (can_change_ident)
        {
            if (mEditCharter) gdatap->mCharter = mEditCharter->getText();
            if (mInsignia) gdatap->mInsigniaID = mInsignia->getImageAssetID();
            if (mComboMature)
            {
                if (!gAgent.isTeen())
                {
                    gdatap->mMaturePublish =
                        mComboMature->getCurrentIndex() == MATURE_CONTENT;
                }
                else
                {
                    gdatap->mMaturePublish = false;
                }
            }
            if (mCtrlShowInGroupList) gdatap->mShowInList = mCtrlShowInGroupList->get();
        }

        if (can_change_member_opts)
        {
            if (mCtrlOpenEnrollment) gdatap->mOpenEnrollment = mCtrlOpenEnrollment->get();
            if (mCtrlEnrollmentFee && mSpinEnrollmentFee)
            {
                gdatap->mMembershipFee = (mCtrlEnrollmentFee->get()) ?
                    (S32) mSpinEnrollmentFee->get() : 0;
                // Set to the used value, and reset initial value used for isdirty check
                mSpinEnrollmentFee->set( (F32)gdatap->mMembershipFee );
            }
        }

        if (can_change_ident || can_change_member_opts)
        {
            LLGroupMgr::getInstance()->sendUpdateGroupInfo(mGroupID);
        }
    }

    bool receive_notices = false;
    bool list_in_profile = false;
    if (mCtrlReceiveNotices)
        receive_notices = mCtrlReceiveNotices->get();
    if (mCtrlListGroup)
        list_in_profile = mCtrlListGroup->get();

    gAgent.setUserGroupFlags(mGroupID, receive_notices, list_in_profile);

    // <exodus>
    if(mCtrlReceiveGroupChat)
    {
        if(mCtrlReceiveGroupChat->get())
        {
            exoGroupMuteList::instance().remove(mGroupID);
        }
        else
        {
            exoGroupMuteList::instance().add(mGroupID);
        }
    }
    // </exodus>

    resetDirty();

    mChanged = false;

    return true;
}

void LLPanelGroupGeneral::cancel()
{
    mChanged = false;

    //cancel out all of the click changes to, although since we are
    //shifting tabs or closing the floater, this need not be done...yet
    notifyObservers();
}

// invoked from callbackConfirmMature
bool LLPanelGroupGeneral::confirmMatureApply(const LLSD& notification, const LLSD& response)
{
    S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
    // 0 == Yes
    // 1 == No
    // 2 == Cancel
    switch(option)
    {
    case 0:
        mComboMature->setCurrentByIndex(MATURE_CONTENT);
        break;
    case 1:
        mComboMature->setCurrentByIndex(NON_MATURE_CONTENT);
        break;
    default:
        return false;
    }

    // If we got here it means they set a valid value
    std::string mesg = "";
    bool ret = apply(mesg);
    if ( !mesg.empty() )
    {
        LLSD args;
        args["MESSAGE"] = mesg;
        LLNotificationsUtil::add("GenericAlert", args);
    }

    return ret;
}

// virtual
void LLPanelGroupGeneral::update(LLGroupChange gc)
{
    if (mGroupID.isNull()) return;

    LLGroupMgrGroupData* gdatap = LLGroupMgr::getInstance()->getGroupData(mGroupID);

    if (!gdatap) return;

    LLGroupData agent_gdatap;
    bool is_member = false;
    if (gAgent.getGroupData(mGroupID,agent_gdatap)) is_member = true;

    if (mComboActiveTitle)
    {
        mComboActiveTitle->setVisible(is_member);
        mComboActiveTitle->setEnabled(mAllowEdit);

        if ( mActiveTitleLabel) mActiveTitleLabel->setVisible(is_member);

        if (is_member)
        {
            LLUUID current_title_role;

            mComboActiveTitle->clear();
            mComboActiveTitle->removeall();
            bool has_selected_title = false;

            if (1 == gdatap->mTitles.size())
            {
                // Only the everyone title.  Don't bother letting them try changing this.
                mComboActiveTitle->setEnabled(false);
            }
            else
            {
                mComboActiveTitle->setEnabled(true);
            }

            std::vector<LLGroupTitle>::const_iterator citer = gdatap->mTitles.begin();
            std::vector<LLGroupTitle>::const_iterator end = gdatap->mTitles.end();

            for ( ; citer != end; ++citer)
            {
                mComboActiveTitle->add(citer->mTitle,citer->mRoleID, (citer->mSelected ? ADD_TOP : ADD_BOTTOM));
                if (citer->mSelected)
                {
                    mComboActiveTitle->setCurrentByID(citer->mRoleID);
                    has_selected_title = true;
                }
            }

            if (!has_selected_title)
            {
                mComboActiveTitle->setCurrentByID(LLUUID::null);
            }
        }

    }

    // After role member data was changed in Roles->Members
    // need to update role titles. See STORM-918.
    if (gc == GC_ROLE_MEMBER_DATA)
        LLGroupMgr::getInstance()->sendGroupTitlesRequest(mGroupID);

    // If this was just a titles update, we are done.
    if (gc == GC_TITLES) return;

    bool can_change_ident = false;
    bool can_change_member_opts = false;
    can_change_ident = gAgent.hasPowerInGroup(mGroupID,GP_GROUP_CHANGE_IDENTITY);
    can_change_member_opts = gAgent.hasPowerInGroup(mGroupID,GP_MEMBER_OPTIONS);

    if (mCtrlShowInGroupList)
    {
        mCtrlShowInGroupList->set(gdatap->mShowInList);
        mCtrlShowInGroupList->setEnabled(mAllowEdit && can_change_ident);
    }
    if (mComboMature)
    {
        if(gdatap->mMaturePublish)
        {
            mComboMature->setCurrentByIndex(MATURE_CONTENT);
        }
        else
        {
            mComboMature->setCurrentByIndex(NON_MATURE_CONTENT);
        }
        mComboMature->setEnabled(mAllowEdit && can_change_ident);
        mComboMature->setVisible( !gAgent.isTeen() );
    }
    if (mCtrlOpenEnrollment)
    {
        mCtrlOpenEnrollment->set(gdatap->mOpenEnrollment);
        mCtrlOpenEnrollment->setEnabled(mAllowEdit && can_change_member_opts);
    }
    if (mCtrlEnrollmentFee)
    {
        mCtrlEnrollmentFee->set(gdatap->mMembershipFee > 0);
        mCtrlEnrollmentFee->setEnabled(mAllowEdit && can_change_member_opts);
    }

    if (mSpinEnrollmentFee)
    {
        S32 fee = gdatap->mMembershipFee;
        mSpinEnrollmentFee->set((F32)fee);
        mSpinEnrollmentFee->setEnabled( mAllowEdit &&
                        (fee > 0) &&
                        can_change_member_opts);
    }
    if (mCtrlReceiveNotices)
    {
        mCtrlReceiveNotices->setVisible(is_member);
        if (is_member)
        {
            mCtrlReceiveNotices->setEnabled(mAllowEdit);
        }
    }

    // <exodus>
    if (mCtrlReceiveGroupChat)
    {
        mCtrlReceiveGroupChat->setVisible(is_member);
        if (is_member)
        {
            mCtrlReceiveGroupChat->setEnabled(mAllowEdit);
        }
    }
    // </exodus>

    if (mInsignia) mInsignia->setEnabled(mAllowEdit && can_change_ident);
    if (mEditCharter) mEditCharter->setEnabled(mAllowEdit && can_change_ident);

    if (mGroupNameEditor) mGroupNameEditor->setVisible(false);
    if (mFounderName) mFounderName->setText(LLSLURL("agent", gdatap->mFounderID, "inspect").getSLURLString());
    if (mInsignia)
    {
        if (gdatap->mInsigniaID.notNull())
        {
            mInsignia->setImageAssetID(gdatap->mInsigniaID);
        }
        else
        {
            mInsignia->setImageAssetName(mInsignia->getDefaultImageName());
        }
    }

    if (mEditCharter)
    {
        mEditCharter->setParseURLs(!mAllowEdit || !can_change_ident);
        mEditCharter->setText(gdatap->mCharter);
    }

    // <FS:Ansariel> Re-add group member list on general panel
    if (mListVisibleMembers)
    {
        mListVisibleMembers->deleteAllItems();

        if (gdatap->isMemberDataComplete())
        {
            mMemberProgress = gdatap->mMembers.begin();
            mPendingMemberUpdate = true;
            mIteratorGroup = mGroupID; // <FS:ND/> FIRE-6074

            sSDTime = 0.0f;
            sElementTime = 0.0f;
            sAllTime = 0.0f;
        }
        else
        {
            std::stringstream pending;
            pending << "Retrieving member list (" << gdatap->mMembers.size() << "\\" << gdatap->mMemberCount  << ")";

            LLSD row;
            row["columns"][0]["value"] = pending.str();
            row["columns"][0]["column"] = "name";

            mListVisibleMembers->setEnabled(false);
            mListVisibleMembers->addElement(row);
        }
    }
    // </FS:Ansariel>

    // <FS:Ansariel> Copy group name button
    childSetEnabled("copy_name", !gdatap->mName.empty());
    mGroupName = gdatap->mName;
    // </FS:Ansariel>

    resetDirty();
}

void LLPanelGroupGeneral::updateChanged()
{
    // List all the controls we want to check for changes...
    LLUICtrl *check_list[] =
    {
        mGroupNameEditor,
        mFounderName,
        mInsignia,
        mEditCharter,
        mCtrlShowInGroupList,
        mComboMature,
        mCtrlOpenEnrollment,
        mCtrlEnrollmentFee,
        mSpinEnrollmentFee,
        mCtrlReceiveNotices,
        mCtrlListGroup,
        mActiveTitleLabel,
        mComboActiveTitle,
        mCtrlReceiveGroupChat // <exodus/>
    };

    mChanged = false;

    for( size_t i=0; i<LL_ARRAY_SIZE(check_list); i++ )
    {
        if( check_list[i] && check_list[i]->isDirty() )
        {
            mChanged = true;
            break;
        }
    }
}

void LLPanelGroupGeneral::reset()
{
    mFounderName->setVisible(false);


    mCtrlReceiveNotices->set(false);


    mCtrlListGroup->set(true);

    mCtrlReceiveNotices->setEnabled(false);
    mCtrlReceiveNotices->setVisible(true);

    mCtrlListGroup->setEnabled(false);

    mGroupNameEditor->setEnabled(true);
    mEditCharter->setEnabled(true);

    mCtrlShowInGroupList->setEnabled(false);
    mComboMature->setEnabled(true);

    mCtrlOpenEnrollment->setEnabled(true);

    mCtrlEnrollmentFee->setEnabled(true);

    mSpinEnrollmentFee->setEnabled(true);
    mSpinEnrollmentFee->set((F32)0);

    mGroupNameEditor->setVisible(true);

    mComboActiveTitle->setVisible(false);

    mInsignia->setImageAssetID(LLUUID::null);

    mInsignia->setEnabled(true);

    mInsignia->setImageAssetName(mInsignia->getDefaultImageName());

    // <exodus>
    mCtrlReceiveGroupChat->set(false);
    mCtrlReceiveGroupChat->setEnabled(false);
    mCtrlReceiveGroupChat->setVisible(true);
    // </exodus>

    {
        std::string empty_str = "";
        mEditCharter->setText(empty_str);
        mGroupNameEditor->setText(empty_str);
    }

    // <FS:Ansariel> Re-add group member list on general panel
    {
        LLSD row;
        row["columns"][0]["value"] = "no members yet";
        row["columns"][0]["column"] = "name";

        mListVisibleMembers->deleteAllItems();
        mListVisibleMembers->setEnabled(false);
        mListVisibleMembers->addElement(row);
    }
    // </FS:Ansariel>

    {
        mComboMature->setEnabled(true);
        mComboMature->setVisible( !gAgent.isTeen() );
        mComboMature->selectFirstItem();
    }


    resetDirty();
}

void    LLPanelGroupGeneral::resetDirty()
{
    // List all the controls we want to check for changes...
    LLUICtrl *check_list[] =
    {
        mGroupNameEditor,
        mFounderName,
        mInsignia,
        mEditCharter,
        mCtrlShowInGroupList,
        mComboMature,
        mCtrlOpenEnrollment,
        mCtrlEnrollmentFee,
        mSpinEnrollmentFee,
        mCtrlReceiveNotices,
        mCtrlListGroup,
        mActiveTitleLabel,
        mComboActiveTitle,
        mCtrlReceiveGroupChat // <exodus/>
    };

    for( size_t i=0; i<LL_ARRAY_SIZE(check_list); i++ )
    {
        if( check_list[i] )
            check_list[i]->resetDirty() ;
    }


}

void LLPanelGroupGeneral::setGroupID(const LLUUID& id)
{
    LLPanelGroupTab::setGroupID(id);
    // <FS> Get group key display and copy URI/name button pointers
    LLTextEditor* groupKeyEditor = getChild<LLTextEditor>("group_key");
    LLButton* copyURIButton = getChild<LLButton>("copy_uri");
    LLButton* copyNameButton = getChild<LLButton>("copy_name");
    // happens when a new group is created
    // </FS>
    if(id == LLUUID::null)
    {
        // <FS>
        if (groupKeyEditor)
            groupKeyEditor->setValue(LLSD());

        if (copyURIButton)
            copyURIButton->setEnabled(false);

        if (copyNameButton)
            copyNameButton->setEnabled(false);
        // </FS>

        reset();
        return;
    }
    // <FS>
    // fill in group key
    if (groupKeyEditor)
        groupKeyEditor->setValue(id.asString());

    // activate copy URI button
    if (copyURIButton)
        copyURIButton->setEnabled(true);
    // </FS>

    bool accept_notices = false;
    bool list_in_profile = false;
    LLGroupData data;
    if(gAgent.getGroupData(mGroupID,data))
    {
        accept_notices = data.mAcceptNotices;
        list_in_profile = data.mListInProfile;
    }
    // <FS:Ansariel> Groupdata debug
    else
    {
        LL_INFOS("Agent_GroupData") << "GROUPDEBUG: Group panel: No agent group data for group " << mGroupID.asString() << LL_ENDL;
    }
    // </FS:Ansariel>
    mCtrlReceiveNotices = getChild<LLCheckBoxCtrl>("receive_notices");
    if (mCtrlReceiveNotices)
    {
        mCtrlReceiveNotices->set(accept_notices);
        mCtrlReceiveNotices->setEnabled(data.mID.notNull());
    }

    mCtrlListGroup = getChild<LLCheckBoxCtrl>("list_groups_in_profile");
    if (mCtrlListGroup)
    {
        mCtrlListGroup->set(list_in_profile);
        mCtrlListGroup->setEnabled(data.mID.notNull());
    }

    // <exodus>
    mCtrlReceiveGroupChat = getChild<LLCheckBoxCtrl>("receive_chat");
    if (mCtrlReceiveGroupChat)
    {
        if(data.mID.notNull())
        {
            mCtrlReceiveGroupChat->set(!exoGroupMuteList::instance().isMuted(data.mID));
        }
        mCtrlReceiveGroupChat->setEnabled(data.mID.notNull());
    }
    // </exodus>

    mCtrlShowInGroupList->setEnabled(data.mID.notNull());

    mActiveTitleLabel = getChild<LLTextBox>("active_title_label");

    mComboActiveTitle = getChild<LLComboBox>("active_title");

    mFounderName->setVisible(true);

    mInsignia->setImageAssetID(LLUUID::null);

    resetDirty();

    activate();
}

// <FS> Copy button handlers
// Copy URI button callback
void LLPanelGroupGeneral::onCopyURI()
{
    LLUrlAction::copyURLToClipboard(LLSLURL("group", getGroupID(), "about").getSLURLString());
}

void LLPanelGroupGeneral::onCopyName()
{
    getWindow()->copyTextToClipboard(utf8str_to_wstring(mGroupName));
}
// </FS> Copy button handlers

// <FS:Ansariel> Re-add group member list on general panel
// static
void LLPanelGroupGeneral::openProfile(void* data)
{
    LLPanelGroupGeneral* self = (LLPanelGroupGeneral*)data;

    if (self && self->mListVisibleMembers)
    {
        LLScrollListItem* selected = self->mListVisibleMembers->getFirstSelected();
        if (selected)
        {
            LLAvatarActions::showProfile(selected->getUUID());
        }
    }
}

void LLPanelGroupGeneral::updateMembers()
{
    mPendingMemberUpdate = false;

    LLGroupMgrGroupData* gdatap = LLGroupMgr::getInstance()->getGroupData(mGroupID);

    if (!mListVisibleMembers
        || !gdatap
        || !gdatap->isMemberDataComplete()
        || gdatap->mMembers.empty())
    {
        return;
    }

    LLTimer update_time;
    update_time.setTimerExpirySec(UPDATE_MEMBERS_SECONDS_PER_FRAME);

    // <FS:ND> FIRE-6074; If the group changes, mMemberPRogresss is invalid, as it belongs to a different LLGroupMgrGroupData. Reset it, start over.
    if( mIteratorGroup != mGroupID )
    {
        mMemberProgress = gdatap->mMembers.begin();
        mIteratorGroup = mGroupID;
    }
    // </FS:ND> FIRE-6074

    // <FS:Ansariel> Clear old callbacks so we don't end up adding people twice
    for (avatar_name_cache_connection_map_t::iterator it = mAvatarNameCacheConnections.begin(); it != mAvatarNameCacheConnections.end(); ++it)
    {
        if (it->second.connected())
        {
            it->second.disconnect();
        }
    }
    mAvatarNameCacheConnections.clear();
    // </FS:Ansariel>

    LLAvatarName av_name;

    for( ; mMemberProgress != gdatap->mMembers.end() && !update_time.hasExpired();
            ++mMemberProgress)
    {
        LLGroupMemberData* member = mMemberProgress->second;
        if (!member)
        {
            continue;
        }

        if (LLAvatarNameCache::get(mMemberProgress->first, &av_name))
        {
            addMember(mMemberProgress->second);
        }
        else
        {
            avatar_name_cache_connection_map_t::iterator it = mAvatarNameCacheConnections.find(mMemberProgress->first);
            if (it != mAvatarNameCacheConnections.end())
            {
                if (it->second.connected())
                {
                    it->second.disconnect();
                }
                mAvatarNameCacheConnections.erase(it);
            }
            mAvatarNameCacheConnections[mMemberProgress->first] = LLAvatarNameCache::get(mMemberProgress->first, boost::bind(&LLPanelGroupGeneral::onNameCache, this, gdatap->getMemberVersion(), member, _2, _1));
        }
    }

    if (mMemberProgress == gdatap->mMembers.end())
    {
        LL_DEBUGS() << "   member list completed." << LL_ENDL;
        mListVisibleMembers->setEnabled(true);
    }
    else
    {
        mPendingMemberUpdate = true;
        mListVisibleMembers->setEnabled(false);
    }
}

void LLPanelGroupGeneral::addMember(LLGroupMemberData* member)
{
    LLNameListCtrl::NameItem item_params;
    item_params.value = member->getID();

    LLScrollListCell::Params column;
    item_params.columns.add().column("name").font.name("SANSSERIF_SMALL");

    item_params.columns.add().column("title").value(member->getTitle()).font.name("SANSSERIF_SMALL");

    item_params.columns.add().column("status").value(member->getOnlineStatus()).font.name("SANSSERIF_SMALL");

    LLScrollListItem* member_row = mListVisibleMembers->addNameItemRow(item_params);

    if ( member->isOwner() )
    {
        LLScrollListText* name_textp = dynamic_cast<LLScrollListText*>(member_row->getColumn(0));
        if (name_textp)
            name_textp->setFontStyle(LLFontGL::BOLD);
    }
}

void LLPanelGroupGeneral::onNameCache(const LLUUID& update_id, LLGroupMemberData* member, const LLAvatarName& av_name, const LLUUID& av_id)
{
    avatar_name_cache_connection_map_t::iterator it = mAvatarNameCacheConnections.find(av_id);
    if (it != mAvatarNameCacheConnections.end())
    {
        if (it->second.connected())
        {
            it->second.disconnect();
        }
        mAvatarNameCacheConnections.erase(it);
    }

    LLGroupMgrGroupData* gdatap = LLGroupMgr::getInstance()->getGroupData(mGroupID);

    if (!gdatap
        || !gdatap->isMemberDataComplete()
        || gdatap->getMemberVersion() != update_id)
    {
        // Stale data
        return;
    }

    addMember(member);
}

S32 LLPanelGroupGeneral::sortMembersList(S32 col_idx,const LLScrollListItem* i1,const LLScrollListItem* i2)
{
    const LLScrollListCell *cell1 = i1->getColumn(col_idx);
    const LLScrollListCell *cell2 = i2->getColumn(col_idx);

    if(col_idx == 2)
    {
        if(LLStringUtil::compareDict(cell1->getValue().asString(),"Online") == 0 )
            return 1;
        if(LLStringUtil::compareDict(cell2->getValue().asString(),"Online") == 0 )
            return -1;
    }

    return LLStringUtil::compareDict(cell1->getValue().asString(), cell2->getValue().asString());
}
// </FS:Ansariel>

// <FS:Ansariel> FIRE-20149: Refresh insignia texture when clicking the refresh button
void LLPanelGroupGeneral::refreshInsigniaTexture()
{
    if (mInsignia && mInsignia->getTexture())
    {
        destroy_texture(mInsignia->getTexture()->getID());
    }
}
// </FS:Ansariel>

