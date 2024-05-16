/**
 * @file fsnamelistavatarmenu.cpp
 * @brief Special avatar menu used LLNameListCtrls
 *
 * $LicenseInfo:firstyear=2015&license=viewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (c) 2015 Ansariel Hiller @ Second Life
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

#include "fsnamelistavatarmenu.h"

#include "fsradar.h"
#include "llagent.h"
#include "llavataractions.h"
#include "lllogchat.h"
#include "llmenugl.h"
#include "llslurl.h"
#include "llurlaction.h"
#include "rlvactions.h"

FSNameListAvatarMenu gFSNameListAvatarMenu;

LLContextMenu* FSNameListAvatarMenu::createMenu()
{
    LLUICtrl::CommitCallbackRegistry::ScopedRegistrar registrar;
    LLUICtrl::EnableCallbackRegistry::ScopedRegistrar enable_registrar;

    if (mUUIDs.size() == 1)
    {
        const LLUUID& id = mUUIDs.front();
        registrar.add("Namelist.ShowProfile",                   boost::bind(&LLAvatarActions::showProfile,                      id));
        registrar.add("Namelist.SendIM",                        boost::bind(&LLAvatarActions::startIM,                          id));
        registrar.add("Namelist.Calllog",                       boost::bind(&LLAvatarActions::viewChatHistory,                  id));
        registrar.add("Namelist.AddFriend",                     boost::bind(&LLAvatarActions::requestFriendshipDialog,          id));
        registrar.add("Namelist.AddToContactSet",               boost::bind(&FSNameListAvatarMenu::addToContactSet,             this));
        registrar.add("Namelist.ZoomIn",                        boost::bind(&LLAvatarActions::zoomIn,                           id));
        registrar.add("Namelist.TeleportToTarget",              boost::bind(&FSNameListAvatarMenu::teleportToAvatar,            this));
        registrar.add("Namelist.OfferTeleport",                 boost::bind(&FSNameListAvatarMenu::offerTeleport,               this));
        registrar.add("Namelist.RequestTeleport",               boost::bind(&LLAvatarActions::teleportRequest,                  id));
        registrar.add("Namelist.TrackAvatar",                   boost::bind(&FSNameListAvatarMenu::onTrackAvatarMenuItemClick,  this));
        registrar.add("Namelist.RemoveFriend",                  boost::bind(&LLAvatarActions::removeFriendDialog,               id));
        registrar.add("Namelist.BlockAvatar",                   boost::bind(&LLAvatarActions::toggleBlock,                      id));
        registrar.add("Namelist.CopyLabel",                     boost::bind(&FSNameListAvatarMenu::copyNameToClipboard,         this, id));
        registrar.add("Namelist.CopyUrl",                       boost::bind(&FSNameListAvatarMenu::copySLURLToClipboard,        this, id));

        enable_registrar.add("Namelist.EnableItem",             boost::bind(&FSNameListAvatarMenu::enableContextMenuItem,       this, _2));
        enable_registrar.add("Namelist.EnableZoomIn",           boost::bind(&LLAvatarActions::canZoomIn,                        id));
        enable_registrar.add("Namelist.CheckIsAgentBlocked",    boost::bind(&LLAvatarActions::isBlocked,                        id));

        return createFromFile("menu_fs_namelist_avatar.xml");
    }
    else
    {
        registrar.add("Namelist.SendIM",                        boost::bind(&LLAvatarActions::startConference,                  mUUIDs, LLUUID::null));
        registrar.add("Namelist.AddToContactSet",               boost::bind(&FSNameListAvatarMenu::addToContactSet,             this));
        registrar.add("Namelist.OfferTeleport",                 boost::bind(&FSNameListAvatarMenu::offerTeleport,               this));
        registrar.add("Namelist.RemoveFriend",                  boost::bind(&LLAvatarActions::removeFriendsDialog,              mUUIDs));

        enable_registrar.add("Namelist.EnableItem",             boost::bind(&FSNameListAvatarMenu::enableContextMenuItem,       this, _2));

        return createFromFile("menu_fs_namelist_avatar_multiselect.xml");
    }
}

bool FSNameListAvatarMenu::enableContextMenuItem(const LLSD& userdata)
{
    std::string item = userdata.asString();
    bool isSelf = !mUUIDs.empty() && mUUIDs.front() == gAgentID;

    if (item == "remove_friend")
    {
        bool result = !mUUIDs.empty();

        for (const auto& avid : mUUIDs)
        {
            if (!LLAvatarActions::isFriend(avid))
            {
                result = false;
                break;
            }
        }

        return result;
    }
    else if (item == "can_add_friend")
    {
        if (mUUIDs.size() == 1)
        {
            return (!isSelf && !LLAvatarActions::isFriend(mUUIDs.front()));
        }
        return false;
    }
    else if (item == "can_add_set")
    {
        if (mUUIDs.size() == 1)
        {
            return (!isSelf);
        }
        return true;
    }
    else if (item == "can_send_im")
    {
        if (mUUIDs.size() == 1)
        {
            return (!isSelf && RlvActions::canStartIM(mUUIDs.front()));
        }
        else if (mUUIDs.size() > 1)
        {
            // Prevent starting a conference if IMs are blocked for a member
            for (const auto& avid : mUUIDs)
            {
                if (avid == gAgentID)
                {
                    continue;
                }

                if (!RlvActions::canStartIM(avid))
                {
                    return false;
                }
            }
            return true;
        }
        return false;
    }
    else if (item == "teleport_to")
    {
        if (mUUIDs.size() == 1)
        {
            return (!isSelf && FSRadar::getInstance()->getEntry(mUUIDs.front()) != nullptr);
        }
        return false;
    }
    else if (item == "offer_teleport")
    {
        return (!isSelf && LLAvatarActions::canOfferTeleport(mUUIDs));
    }
    else if (item == "request_teleport")
    {
        if (mUUIDs.size() == 1)
        {
            return (!isSelf && LLAvatarActions::canRequestTeleport(mUUIDs.front()));
        }
        return false;
    }
    else if (item == "track_avatar")
    {
        if (mUUIDs.size() == 1)
        {
            return (!isSelf && FSRadar::getInstance()->getEntry(mUUIDs.front()) != nullptr);
        }
        return false;
    }
    else if (item == "can_callog")
    {
        if (mUUIDs.size() == 1)
        {
            return (!isSelf && LLLogChat::isTranscriptExist(mUUIDs.front()));
        }
        return false;
    }
    else if (item == "can_block")
    {
        if (mUUIDs.size() == 1)
        {
            return !isSelf;
        }
    }
    return false;
}

void FSNameListAvatarMenu::offerTeleport()
{
    uuid_vec_t uuids{};
    uuids.reserve(mUUIDs.size());
    for (const auto& avid : mUUIDs)
    {
        // Skip ourself if sending a TP to more than one agent
        if (avid != gAgentID)
        {
            uuids.emplace_back(avid);
        }
    }
    LLAvatarActions::offerTeleport(uuids);
}

void FSNameListAvatarMenu::teleportToAvatar()
{
    LLAvatarActions::teleportTo(mUUIDs.front());
}

void FSNameListAvatarMenu::onTrackAvatarMenuItemClick()
{
    LLAvatarActions::track(mUUIDs.front());
}

void FSNameListAvatarMenu::addToContactSet()
{
    uuid_vec_t uuids{};
    uuids.reserve(mUUIDs.size());
    for (const auto& avid : mUUIDs)
    {
        // Skip ourself if adding more than one agent
        if (avid != gAgentID)
        {
            uuids.emplace_back(avid);
        }
    }
    LLAvatarActions::addToContactSet(uuids);
}

void FSNameListAvatarMenu::copyNameToClipboard(const LLUUID& id)
{
    LLAvatarName av_name;
    LLAvatarNameCache::get(id, &av_name);
    LLUrlAction::copyURLToClipboard(av_name.getAccountName());
}

void FSNameListAvatarMenu::copySLURLToClipboard(const LLUUID& id)
{
    LLUrlAction::copyURLToClipboard(LLSLURL("agent", id, "about").getSLURLString());
}
