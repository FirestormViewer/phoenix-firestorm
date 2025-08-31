/**
 * @file llpanelpeoplemenus.h
 * @brief Menus used by the side tray "People" panel
 *
 * $LicenseInfo:firstyear=2009&license=viewerlgpl$
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

// libs
#include "llmenugl.h"
#include "lluictrlfactory.h"
#include "llfolderview.h"   // <FS:CR> CHUI merge

#include "llpanelpeoplemenus.h"

// newview
#include "llagent.h"
#include "llagentdata.h"            // for gAgentID
#include "llavataractions.h"
#include "llcallingcard.h"          // for LLAvatarTracker
#include "lllogchat.h"
#include "llparcel.h"
#include "llviewermenu.h"           // for gMenuHolder
#include "llconversationmodel.h"
#include "llviewerobjectlist.h"
#include "llviewerparcelmgr.h"
#include "llviewerregion.h"
#include "llvoavatarself.h"
#include "roles_constants.h"
#include "llinventorybridge.h"  // <FS:CR> CHUI merge
// [RLVa:KB] - Checked: RLVa-2.0.1
#include "rlvactions.h"
// [/RLVa:KB]

namespace LLPanelPeopleMenus
{

PeopleContextMenu gPeopleContextMenu;
NearbyPeopleContextMenu gNearbyPeopleContextMenu;

//== PeopleContextMenu ===============================================================

LLContextMenu* PeopleContextMenu::createMenu()
{
    // set up the callbacks for all of the avatar menu items
    LLUICtrl::CommitCallbackRegistry::ScopedRegistrar registrar;
    LLUICtrl::EnableCallbackRegistry::ScopedRegistrar enable_registrar;
    LLContextMenu* menu;

    if ( mUUIDs.size() == 1 )
    {
        // Set up for one person selected menu

        const LLUUID& id = mUUIDs.front();
        registrar.add("Avatar.Profile",         boost::bind(&LLAvatarActions::showProfile,              id));
        registrar.add("Avatar.AddFriend",       boost::bind(&LLAvatarActions::requestFriendshipDialog,  id));
        registrar.add("Avatar.RemoveFriend",    boost::bind(&LLAvatarActions::removeFriendDialog,       id));
        registrar.add("Avatar.IM",              boost::bind(&LLAvatarActions::startIM,                  id));
        registrar.add("Avatar.Call",            boost::bind(&LLAvatarActions::startCall,                id));
        registrar.add("Avatar.OfferTeleport",   boost::bind(&PeopleContextMenu::offerTeleport,          this));
        // <FS:CR> CHUI merge - Make GCC happy
        //registrar.add("Avatar.ZoomIn",            boost::bind(&handle_zoom_to_object,                     id));
        registrar.add("Avatar.ZoomIn",          boost::bind(&LLAvatarActions::zoomIn,                       id));
        // </FS:CR>
        registrar.add("Avatar.ShowOnMap",       boost::bind(&LLAvatarActions::showOnMap,                id));
        registrar.add("Avatar.Share",           boost::bind(&LLAvatarActions::share,                    id));
        registrar.add("Avatar.Pay",             boost::bind(&LLAvatarActions::pay,                      id));
        registrar.add("Avatar.BlockUnblock",    boost::bind(&LLAvatarActions::toggleBlock,              id));
        registrar.add("Avatar.InviteToGroup",   boost::bind(&LLAvatarActions::inviteToGroup,            id));
        registrar.add("Avatar.TeleportRequest", boost::bind(&PeopleContextMenu::requestTeleport,        this));
        registrar.add("Avatar.Calllog",         boost::bind(&LLAvatarActions::viewChatHistory,          id));
        // <FS:Ansariel> Circumvent LL screw up
        //registrar.add("Avatar.Freeze",            boost::bind(&LLAvatarActions::freezeAvatar,                 id));
        //registrar.add("Avatar.Eject",         boost::bind(&PeopleContextMenu::eject,                  this));
        registrar.add("Avatar.Freeze",          boost::bind(&LLAvatarActions::landFreeze,                   id));
        registrar.add("Avatar.Eject",           boost::bind(&LLAvatarActions::landEject,                    id));
        // </FS:Ansariel>

        // <FS:Ansariel> Firestorm additions
        registrar.add("Avatar.GroupInvite",     boost::bind(&LLAvatarActions::inviteToGroup,            id));
        registrar.add("Avatar.AddToContactSet", boost::bind(&PeopleContextMenu::addToContactSet,        this)); // [FS:CR]

        enable_registrar.add("Avatar.EnableItem", boost::bind(&PeopleContextMenu::enableContextMenuItem, this, _2));
        enable_registrar.add("Avatar.CheckItem",  boost::bind(&PeopleContextMenu::checkContextMenuItem, this, _2));
        // <FS:Ansariel> Circumvent LL screw up
        //enable_registrar.add("Avatar.EnableFreezeEject", boost::bind(&PeopleContextMenu::enableFreezeEject, this, _2));
        enable_registrar.add("Avatar.EnableFreezeEject", boost::bind(&LLAvatarActions::canLandFreezeOrEject, id));
        // </FS:Ansariel>

        // create the context menu from the XUI
        menu = createFromFile("menu_people_nearby.xml");
        buildContextMenu(*menu, 0x0);
    }
    else
    {
        // Set up for multi-selected People

        // registrar.add("Avatar.AddFriend",    boost::bind(&LLAvatarActions::requestFriendshipDialog,  mUUIDs)); // *TODO: unimplemented
        registrar.add("Avatar.IM",              boost::bind(&PeopleContextMenu::startConference,        this));
        registrar.add("Avatar.Call",            boost::bind(&LLAvatarActions::startAdhocCall,           mUUIDs, LLUUID::null));
        registrar.add("Avatar.OfferTeleport",   boost::bind(&PeopleContextMenu::offerTeleport,          this));
        registrar.add("Avatar.RemoveFriend",    boost::bind(&LLAvatarActions::removeFriendsDialog,      mUUIDs));
        // registrar.add("Avatar.Share",        boost::bind(&LLAvatarActions::startIM,                  mUUIDs)); // *TODO: unimplemented
        // registrar.add("Avatar.Pay",          boost::bind(&LLAvatarActions::pay,                      mUUIDs)); // *TODO: unimplemented
        registrar.add("Avatar.AddToContactSet", boost::bind(&PeopleContextMenu::addToContactSet,        this));   // <FS:Ansariel>

        enable_registrar.add("Avatar.EnableItem",   boost::bind(&PeopleContextMenu::enableContextMenuItem, this, _2));

        // create the context menu from the XUI
        menu = createFromFile("menu_people_nearby_multiselect.xml");
        buildContextMenu(*menu, ITEM_IN_MULTI_SELECTION);
    }

    return menu;
}

void PeopleContextMenu::buildContextMenu(class LLMenuGL& menu, U32 flags)
{
    menuentry_vec_t items;
    menuentry_vec_t disabled_items;

    if (flags & ITEM_IN_MULTI_SELECTION)
    {
        items.push_back(std::string("add_friends"));
        items.push_back(std::string("remove_friends"));
        items.push_back(std::string("im"));
        items.push_back(std::string("call"));
        items.push_back(std::string("share"));
        items.push_back(std::string("pay"));
        items.push_back(std::string("offer_teleport"));

        items.push_back(std::string("Add to Set")); // <FS:Ansariel> Contact sets
    }
    else
    {
        items.push_back(std::string("view_profile"));
        items.push_back(std::string("im"));
        items.push_back(std::string("offer_teleport"));
        items.push_back(std::string("request_teleport"));
        items.push_back(std::string("voice_call"));
        items.push_back(std::string("chat_history"));
        items.push_back(std::string("separator_chat_history"));
        items.push_back(std::string("add_friend"));
        items.push_back(std::string("remove_friend"));
        items.push_back(std::string("invite_to_group"));
        items.push_back(std::string("separator_invite_to_group"));
        items.push_back(std::string("map"));
        items.push_back(std::string("share"));
        items.push_back(std::string("pay"));
        items.push_back(std::string("block_unblock"));

        items.push_back(std::string("Add to Set")); // [FS:CR] Contact sets
    }

    hide_context_entries(menu, items, disabled_items);
}

bool PeopleContextMenu::enableContextMenuItem(const LLSD& userdata)
{
    std::string item = userdata.asString();
    if(gAgent.getID() == mUUIDs.front())
    {
        if (item == std::string("can_zoom_in"))
        {
            return true;
        }
        return false;
    }

    // Note: can_block and can_delete is used only for one person selected menu
    // so we don't need to go over all uuids.

    if (item == std::string("can_block"))
    {
        const LLUUID& id = mUUIDs.front();
        return LLAvatarActions::canBlock(id);
    }
    else if (item == std::string("can_add"))
    {
        // We can add friends if:
        // - there are selected people
        // - and there are no friends among selection yet.

        //EXT-7389 - disable for more than 1
        if(mUUIDs.size() > 1)
        {
            return false;
        }

        bool result = (mUUIDs.size() > 0);

        uuid_vec_t::const_iterator
            id = mUUIDs.begin(),
            uuids_end = mUUIDs.end();

        for (;id != uuids_end; ++id)
        {
//          if ( LLAvatarActions::isFriend(*id) )
// [RLVa:KB] - Checked: 2014-03-31 (RLVa-2.0.1)
            if ( (LLAvatarActions::isFriend(*id)) || (!RlvActions::canShowName(RlvActions::SNC_DEFAULT, *id)) )
// [/RLVa:KB]
            {
                result = false;
                break;
            }
        }

        return result;
    }
    else if (item == std::string("can_delete"))
    {
        // We can remove friends if:
        // - there are selected people
        // - and there are only friends among selection.

        bool result = (mUUIDs.size() > 0);

        uuid_vec_t::const_iterator
            id = mUUIDs.begin(),
            uuids_end = mUUIDs.end();

        for (;id != uuids_end; ++id)
        {
//          if ( !LLAvatarActions::isFriend(*id) )
// [RLVa:KB] - Checked: 2014-03-31 (RLVa-2.0.1)
            if ( (!LLAvatarActions::isFriend(*id)) || (!RlvActions::canShowName(RlvActions::SNC_DEFAULT, *id)) )
// [/RLVa:KB]
            {
                result = false;
                break;
            }
        }

        return result;
    }
    else if (item == std::string("can_call"))
    {
        return LLAvatarActions::canCall();
    }
    else if (item == std::string("can_zoom_in"))
    {
        const LLUUID& id = mUUIDs.front();

        return gObjectList.findObject(id);
    }
    else if (item == std::string("can_show_on_map"))
    {
        const LLUUID& id = mUUIDs.front();

        return (LLAvatarTracker::instance().isBuddyOnline(id) && is_agent_mappable(id))
                    || gAgent.isGodlike();
    }
    else if(item == std::string("can_offer_teleport"))
    {
        return LLAvatarActions::canOfferTeleport(mUUIDs);
    }
    // <FS:Ansariel> Extra request teleport
    else if(item == std::string("can_request_teleport"))
    {
        if (mUUIDs.size() == 1)
        {
            return LLAvatarActions::canRequestTeleport(mUUIDs.front());
        }
        return false;
    }
    // </FS:Ansariel>
    // <FS:Ansariel> FIRE-8804: Prevent opening inventory from using share in radar context menu
    else if (item == std::string("can_open_inventory"))
    {
        return (!RlvActions::hasBehaviour(RLV_BHVR_SHOWINV));
    }
    // </FS:Ansariel>
    else if (item == std::string("can_callog"))
    {
        return LLLogChat::isTranscriptExist(mUUIDs.front());
    }
// [RLVa:KB] - @pay
    else if (item == std::string("can_pay"))
    {
        return RlvActions::canPayAvatar(mUUIDs.front());
    }
// [/RLVa:KB]
    else if (item == std::string("can_im") || item == std::string("can_invite"))
    {
        return true;
    }
    else if (item == std::string("can_share") || item == std::string("can_pay"))
    {
        return mUUIDs.size() == 1;
    }
    return false;
}

bool PeopleContextMenu::checkContextMenuItem(const LLSD& userdata)
{
    std::string item = userdata.asString();
    const LLUUID& id = mUUIDs.front();

    if (item == std::string("is_blocked"))
    {
        return LLAvatarActions::isBlocked(id);
    }

    return false;
}

bool PeopleContextMenu::enableFreezeEject(const LLSD& userdata)
{
    if((gAgent.getID() == mUUIDs.front()) || (mUUIDs.size() != 1))
    {
        return false;
    }

    const LLUUID& id = mUUIDs.front();

    // Use avatar_id if available, otherwise default to right-click avatar
    LLVOAvatar* avatar = NULL;
    if (id.notNull())
    {
        LLViewerObject* object = gObjectList.findObject(id);
        if (object)
        {
            if( !object->isAvatar() )
            {
                object = NULL;
            }
            avatar = (LLVOAvatar*) object;
        }
    }
    if (!avatar) return false;

    // Gods can always freeze
    if (gAgent.isGodlike()) return true;

    // Estate owners / managers can freeze
    // Parcel owners can also freeze
    const LLVector3& pos = avatar->getPositionRegion();
    const LLVector3d& pos_global = avatar->getPositionGlobal();
    LLParcel* parcel = LLViewerParcelMgr::getInstance()->selectParcelAt(pos_global)->getParcel();
    LLViewerRegion* region = avatar->getRegion();
    if (!region) return false;

    bool new_value = region->isOwnedSelf(pos);
    if (!new_value || region->isOwnedGroup(pos))
    {
        new_value = LLViewerParcelMgr::getInstance()->isParcelOwnedByAgent(parcel,GP_LAND_ADMIN);
    }
    return new_value;
}

void PeopleContextMenu::requestTeleport()
{
    // boost::bind cannot recognize overloaded method LLAvatarActions::teleportRequest(),
    // so we have to use a wrapper.
// [RLVa:KB] - Checked: RLVa-2.0.1
    bool fRlvCanShowName = (!m_fRlvCheck) || (RlvActions::canShowName(RlvActions::SNC_DEFAULT, mUUIDs.front()));
    RlvActions::setShowName(RlvActions::SNC_TELEPORTREQUEST, fRlvCanShowName);
    LLAvatarActions::teleportRequest(mUUIDs.front());
    RlvActions::setShowName(RlvActions::SNC_TELEPORTREQUEST, true);
// [/RLVa:KB]
//  LLAvatarActions::teleportRequest(mUUIDs.front());
}

void PeopleContextMenu::offerTeleport()
{
    // boost::bind cannot recognize overloaded method LLAvatarActions::offerTeleport(),
    // so we have to use a wrapper.
// [RLVa:KB] - Checked: RLVa-2.0.1
    bool fRlvCanShowName = true;
    if ( (m_fRlvCheck) && (RlvActions::isRlvEnabled()) )
        std::for_each(mUUIDs.begin(), mUUIDs.end(), [&fRlvCanShowName](const LLUUID& idAgent) { fRlvCanShowName &= RlvActions::canShowName(RlvActions::SNC_DEFAULT, idAgent); });

    RlvActions::setShowName(RlvActions::SNC_TELEPORTOFFER, fRlvCanShowName);
    LLAvatarActions::offerTeleport(mUUIDs);
    RlvActions::setShowName(RlvActions::SNC_TELEPORTOFFER, true);
// [/RLVa:KB]
//  LLAvatarActions::offerTeleport(mUUIDs);
}

void PeopleContextMenu::eject()
{
    if((gAgent.getID() == mUUIDs.front()) || (mUUIDs.size() != 1))
    {
        return;
    }

    const LLUUID& id = mUUIDs.front();

    // Use avatar_id if available, otherwise default to right-click avatar
    LLVOAvatar* avatar = NULL;
    if (id.notNull())
    {
        LLViewerObject* object = gObjectList.findObject(id);
        if (object)
        {
            if( !object->isAvatar() )
            {
                object = NULL;
            }
            avatar = (LLVOAvatar*) object;
        }
    }

    if (!avatar)
        return;

    LLSD payload;
    payload["avatar_id"] = avatar->getID();
    std::string fullname = avatar->getFullname();

    const LLVector3d& pos = avatar->getPositionGlobal();
    LLParcel* parcel = LLViewerParcelMgr::getInstance()->selectParcelAt(pos)->getParcel();
    LLAvatarActions::ejectAvatar(id ,LLViewerParcelMgr::getInstance()->isParcelOwnedByAgent(parcel,GP_LAND_MANAGE_BANNED));
}

void PeopleContextMenu::startConference()
{
    uuid_vec_t uuids;
    for (uuid_vec_t::const_iterator it = mUUIDs.begin(); it != mUUIDs.end(); ++it)
    {
        if(*it != gAgentID)
        {
            uuids.push_back(*it);
        }
    }
    LLAvatarActions::startConference(uuids);
}

// <FS:Ansariel> Add to contact set
void PeopleContextMenu::addToContactSet()
{
    LLAvatarActions::addToContactSet(mUUIDs);
}
// </FS:Ansariel>

//== NearbyPeopleContextMenu ===============================================================

void NearbyPeopleContextMenu::buildContextMenu(class LLMenuGL& menu, U32 flags)
{
    menuentry_vec_t items;
    menuentry_vec_t disabled_items;

// [RLVa:KB] - Checked: RLVa-1.5.0
    bool fRlvCanShowName = true;
    if ( (m_fRlvCheck) && (RlvActions::isRlvEnabled()) )
        std::for_each(mUUIDs.begin(), mUUIDs.end(), [&fRlvCanShowName](const LLUUID& idAgent) { fRlvCanShowName &= RlvActions::canShowName(RlvActions::SNC_DEFAULT, idAgent); });

    if (!fRlvCanShowName)
    {
        if (flags & ITEM_IN_MULTI_SELECTION)
        {
            items.push_back(std::string("offer_teleport"));
        }
        else
        {
            items.push_back(std::string("offer_teleport"));
            items.push_back(std::string("request_teleport"));
            items.push_back(std::string("separator_invite_to_group"));
            items.push_back(std::string("zoom_in"));
            items.push_back(std::string("block_unblock"));
        }
    }
    else if (flags & ITEM_IN_MULTI_SELECTION)
// [/RLVa:KB]
//  if (flags & ITEM_IN_MULTI_SELECTION)
    {
        items.push_back(std::string("add_friends"));
        items.push_back(std::string("remove_friends"));
        items.push_back(std::string("im"));
        items.push_back(std::string("call"));
        items.push_back(std::string("share"));
        items.push_back(std::string("pay"));
        items.push_back(std::string("offer_teleport"));
    }
    else
    {
        items.push_back(std::string("view_profile"));
        items.push_back(std::string("im"));
        items.push_back(std::string("offer_teleport"));
        items.push_back(std::string("request_teleport"));
        items.push_back(std::string("voice_call"));
        items.push_back(std::string("chat_history"));
        items.push_back(std::string("separator_chat_history"));
        items.push_back(std::string("add_friend"));
        items.push_back(std::string("remove_friend"));
        items.push_back(std::string("invite_to_group"));
        items.push_back(std::string("separator_invite_to_group"));
        items.push_back(std::string("zoom_in"));
        items.push_back(std::string("map"));
        items.push_back(std::string("share"));
        items.push_back(std::string("pay"));
        items.push_back(std::string("block_unblock"));
        items.push_back(std::string("freeze"));
        items.push_back(std::string("eject"));
    }

    hide_context_entries(menu, items, disabled_items);
}

} // namespace LLPanelPeopleMenus
