/**
 * @file fsavatarsearchmenu.cpp
 * @brief Menu used by the avatar picker
 *
 * $LicenseInfo:firstyear=2014&license=viewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (c) 2014 Ansariel Hiller
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

#include "llviewerprecompiledheaders.h"

#include "fsavatarsearchmenu.h"

#include "llagent.h"
#include "llavataractions.h"
#include "llcallingcard.h"
#include "lluictrl.h"
#include "llviewermenu.h"
#include "rlvhandler.h"

FSAvatarSearchMenu gFSAvatarSearchMenu;

LLContextMenu* FSAvatarSearchMenu::createMenu()
{
    LLUICtrl::CommitCallbackRegistry::ScopedRegistrar registrar;
    LLUICtrl::EnableCallbackRegistry::ScopedRegistrar enable_registrar;

    if (mUUIDs.size() == 1)
    {
        // Set up for one person selected menu

        const LLUUID& id = mUUIDs.front();
        registrar.add("Avatar.Profile",                         boost::bind(&LLAvatarActions::showProfile,                  id));
        registrar.add("Avatar.AddFriend",                       boost::bind(&LLAvatarActions::requestFriendshipDialog,      id));
        registrar.add("Avatar.RemoveFriend",                    boost::bind(&LLAvatarActions::removeFriendDialog,           id));
        registrar.add("Avatar.IM",                              boost::bind(&LLAvatarActions::startIM,                      id));
        registrar.add("Avatar.Call",                            boost::bind(&LLAvatarActions::startCall,                    id));
        registrar.add("Avatar.OfferTeleport",                   boost::bind(&FSAvatarSearchMenu::offerTeleport,             this));
        registrar.add("Avatar.TeleportRequest",                 boost::bind(&LLAvatarActions::teleportRequest,              id));
        registrar.add("Avatar.GroupInvite",                     boost::bind(&LLAvatarActions::inviteToGroup,                id));
        registrar.add("Avatar.Share",                           boost::bind(&LLAvatarActions::share,                        id));
        registrar.add("Avatar.Pay",                             boost::bind(&LLAvatarActions::pay,                          id));
        registrar.add("Avatar.BlockUnblock",                    boost::bind(&LLAvatarActions::toggleBlock,                  id));

        enable_registrar.add("Avatar.EnableItem",               boost::bind(&FSAvatarSearchMenu::onContextMenuItemEnable,   this, _2));
        enable_registrar.add("Avatar.CheckItem",                boost::bind(&FSAvatarSearchMenu::onContextMenuItemCheck,    this, _2));

        // create the context menu from the XUI
        return createFromFile("menu_fs_avatar_search.xml");
    }
    else
    {
        // Set up for multi-selected People
        registrar.add("Avatar.IM",                              boost::bind(&LLAvatarActions::startConference,                      mUUIDs, LLUUID::null));
        registrar.add("Avatar.Call",                            boost::bind(&LLAvatarActions::startAdhocCall,                       mUUIDs, LLUUID::null));
        registrar.add("Avatar.OfferTeleport",                   boost::bind(&FSAvatarSearchMenu::offerTeleport,                     this));
        registrar.add("Avatar.RemoveFriend",                    boost::bind(&LLAvatarActions::removeFriendsDialog,                  mUUIDs));
        registrar.add("Avatar.AddToContactSet",                 boost::bind(&FSAvatarSearchMenu::addToContactSet,                   this));

        enable_registrar.add("Avatar.EnableItem",               boost::bind(&FSAvatarSearchMenu::onContextMenuItemEnable,           this, _2));

        // create the context menu from the XUI
        return createFromFile("menu_fs_avatar_search_multiselect.xml");
    }
}

bool FSAvatarSearchMenu::onContextMenuItemEnable(const LLSD& userdata)
{
    std::string item = userdata.asString();

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
            if ( LLAvatarActions::isFriend(*id) )
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
            if ( !LLAvatarActions::isFriend(*id) )
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
    else if(item == std::string("can_request_teleport"))
    {
        if (mUUIDs.size() == 1)
        {
            return LLAvatarActions::canRequestTeleport(mUUIDs.front());
        }
        return false;
    }
    else if (item == std::string("can_open_inventory"))
    {
        return (!gRlvHandler.hasBehaviour(RLV_BHVR_SHOWINV));
    }

    return false;
}

bool FSAvatarSearchMenu::onContextMenuItemCheck(const LLSD& userdata)
{
    std::string item = userdata.asString();
    const LLUUID& id = mUUIDs.front();

    if (item == std::string("is_blocked"))
    {
        return LLAvatarActions::isBlocked(id);
    }

    return false;
}

void FSAvatarSearchMenu::offerTeleport()
{
    LLAvatarActions::offerTeleport(mUUIDs);
}

void FSAvatarSearchMenu::addToContactSet()
{
    LLAvatarActions::addToContactSet(mUUIDs);
}

