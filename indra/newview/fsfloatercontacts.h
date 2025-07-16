/**
 * @file fsfloatercontacts.h
 * @brief Legacy contacts floater header file
 *
 * $LicenseInfo:firstyear=2011&license=fsviewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (C) 2013, The Phoenix Firestorm Project, Inc.
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
 * The Phoenix Firestorm Project, Inc., 1831 Oakwood Drive, Fairmont, Minnesota 56031-3225 USA
 * http://www.firestormviewer.org
 * $/LicenseInfo$
 */


#ifndef FS_FLOATERCONTACTS_H
#define FS_FLOATERCONTACTS_H

#include "llavatarnamecache.h"
#include "llcallingcard.h"
#include "llfloater.h"
#include "llscrolllistcolumn.h"
#include "rlvhandler.h"

class FSScrollListCtrl;
class LLButton;
class LLFilterEditor;
class LLGroupList;
class LLRelationship;
class LLPanel;
class LLTabContainer;
class LLTextBox;

class FSFloaterContacts : public LLFloater, LLFriendObserver, LLEventTimer
{
public:
    FSFloaterContacts(const LLSD& seed);
    virtual ~FSFloaterContacts();

    bool postBuild() override;
    void onOpen(const LLSD& key) override;
    void draw() override;
    bool tick() override;
    bool handleKeyHere(KEY key, MASK mask) override;
    bool hasAccelerators() const override  { return true; }

    // LLFriendObserver implementation
    void changed(U32 changed_mask) override;

    static FSFloaterContacts* getInstance();
    static FSFloaterContacts* findInstance();

    void openTab(const std::string& name);
    LLPanel* getPanelByName(const std::string& panel_name);

    void                    sortFriendList();
    void                    onDisplayNameChanged();
    void                    resetFriendFilter();

private:
    typedef std::vector<LLScrollListItem*> listitem_vec_t;

    std::string             getActiveTabName() const;
    LLUUID                  getCurrentItemID() const;
    void                    getCurrentItemIDs(uuid_vec_t& selected_uuids) const;
    void                    getCurrentFriendItemIDs(uuid_vec_t& selected_uuids) const;

    enum FRIENDS_COLUMN_ORDER
    {
        LIST_ONLINE_STATUS,
        LIST_FRIEND_USER_NAME,
        LIST_FRIEND_DISPLAY_NAME,
        LIST_FRIEND_NAME,
        LIST_VISIBLE_ONLINE,
        LIST_VISIBLE_MAP,
        LIST_EDIT_MINE,
        LIST_VISIBLE_MAP_THEIRS,
        LIST_EDIT_THEIRS,
        LIST_FRIEND_UPDATE_GEN
    };

    typedef std::map<LLUUID, S32> rights_map_t;
    void                    refreshRightsChangeList();
    void                    refreshUI();
    void                    onSelectName();
    void                    applyRightsToFriends();
    void                    addFriend(const LLUUID& agent_id);
    void                    updateFriendItem(const LLUUID& agent_id, const LLRelationship* relationship);
    void                    updateFriendItem(const LLUUID& agent_id, const LLRelationship* relationship, const LLUUID& request_id);

    typedef enum
    {
        GRANT,
        REVOKE
    } EGrantRevoke;
    void                    confirmModifyRights(rights_map_t& ids, EGrantRevoke command);
    void                    sendRightsGrant(rights_map_t& ids);
    bool                    modifyRightsConfirmation(const LLSD& notification, const LLSD& response, rights_map_t* rights);


    bool                    isItemsFreeOfFriends(const uuid_vec_t& uuids);

    // misc callbacks
    static void             onAvatarPicked(const uuid_vec_t& ids, const std::vector<LLAvatarName> names);
    void                    onColumnDisplayModeChanged(const std::string& settings_name = "");
    bool                    handleFriendsListDragAndDrop(S32 x, S32 y, MASK mask, bool drop,
                                                            EDragAndDropType cargo_type,
                                                            void* cargo_data,
                                                            EAcceptance* accept,
                                                            std::string& tooltip_msg);
    void                    onFriendFilterEdit(const std::string& search_string);
    void                    onGroupFilterEdit(const std::string& search_string);

    // friend buttons
    void                    onViewProfileButtonClicked();
    void                    onImButtonClicked();
    void                    onTeleportButtonClicked();
    void                    onPayButtonClicked();
    void                    onDeleteFriendButtonClicked();
    void                    onAddFriendWizButtonClicked(LLUICtrl* ctrl);
    void                    onMapButtonClicked();

    // group buttons
    void                    onGroupChatButtonClicked();
    void                    onGroupInfoButtonClicked();
    void                    onGroupActivateButtonClicked();
    void                    onGroupLeaveButtonClicked();
    void                    onGroupCreateButtonClicked();
    void                    onGroupSearchButtonClicked();
    void                    onGroupTitlesButtonClicked();
    void                    onGroupInviteButtonClicked();
    void                    updateGroupButtons();

    LLTabContainer*         mTabContainer{ nullptr };
    LLFilterEditor*         mFriendFilter{ nullptr };
    LLFilterEditor*         mGroupFilter{ nullptr };
    LLPanel*                mFriendsTab{ nullptr };
    FSScrollListCtrl*       mFriendsList{ nullptr };
    LLPanel*                mGroupsTab{ nullptr };
    LLGroupList*            mGroupList{ nullptr };

    LLButton*               mFriendsImBtn{ nullptr };
    LLButton*               mFriendsProfileBtn{ nullptr };
    LLButton*               mFriendsTpBtn{ nullptr };
    LLButton*               mFriendsMapBtn{ nullptr };
    LLButton*               mFriendsPayBtn{ nullptr };
    LLButton*               mFriendsAddBtn{ nullptr };
    LLButton*               mFriendsRemoveBtn{ nullptr };

    LLButton*               mGroupsChatBtn{ nullptr };
    LLButton*               mGroupsInfoBtn{ nullptr };
    LLButton*               mGroupsActivateBtn{ nullptr };
    LLButton*               mGroupsLeaveBtn{ nullptr };
    LLButton*               mGroupsCreateBtn{ nullptr };
    LLButton*               mGroupsSearchBtn{ nullptr };
    LLButton*               mGroupsTitlesBtn{ nullptr };
    LLButton*               mGroupsInviteBtn{ nullptr };

    LLTextBox*              mFriendsCountTb{ nullptr };
    LLTextBox*              mGroupssCountTb{ nullptr };

    bool                    mAllowRightsChange{ true };
    size_t                  mNumRightsChanged{ 0 };
    bool                    mRightsChangeNotificationTriggered{ false };

    std::string             mFriendListFontName{};

    std::string             mLastColumnDisplayModeChanged{};
    bool                    mResetLastColumnDisplayModeChanged{ false };
    bool                    mDirtyNames{ true };

    std::string             mFriendFilterSubString{ LLStringUtil::null };
    std::string             mFriendFilterSubStringOrig{ LLStringUtil::null };

    void childShowTab(const std::string& id, const std::string& tabname);

    void updateRlvRestrictions(ERlvBehaviour behavior);
    boost::signals2::connection mRlvBehaviorCallbackConnection{};

    std::string getFullName(const LLAvatarName& av_name);

    void setDirtyNames(const LLUUID& request_id);

    typedef std::map<LLUUID, LLAvatarNameCache::callback_connection_t> avatar_name_cb_t;
    avatar_name_cb_t    mAvatarNameCacheConnections;
    void                disconnectAvatarNameCacheConnection(const LLUUID& request_id);
};

#endif // FS_FLOATERCONTACTS_H
