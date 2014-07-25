/** 
 * @file fscontactsfriendsmenu.cpp
 * @brief Menu used by Firestorm contacts floater
 *
 * $LicenseInfo:firstyear=2013&license=viewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (c) 2014 Ansariel Hiller @ Second Life
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

#include "fscontactsfriendsmenu.h"

#include "fsradar.h"
#include "llavataractions.h"
#include "llmenugl.h"
#include "llslurl.h"
#include "lluictrlfactory.h"
#include "llurlaction.h"

FSContactsFriendsMenu gFSContactsFriendsMenu;

LLContextMenu* FSContactsFriendsMenu::createMenu()
{
	LLUICtrl::CommitCallbackRegistry::ScopedRegistrar registrar;
	LLUICtrl::EnableCallbackRegistry::ScopedRegistrar enable_registrar;

	if (mUUIDs.size() == 1)
	{
		const LLUUID& id = mUUIDs.front();
		registrar.add("Contacts.Friends.ShowProfile",			boost::bind(&LLAvatarActions::showProfile,						id));
		registrar.add("Contacts.Friends.RemoveFriend",			boost::bind(&LLAvatarActions::removeFriendDialog, 				id));
		registrar.add("Contacts.Friends.SendIM",				boost::bind(&LLAvatarActions::startIM,							id));
		registrar.add("Contacts.Friends.OfferTeleport",			boost::bind(&FSContactsFriendsMenu::offerTeleport,				this));
		registrar.add("Contacts.Friends.RequestTeleport",		boost::bind(&LLAvatarActions::teleportRequest,					id));
		registrar.add("Contacts.Friends.ZoomIn",				boost::bind(&LLAvatarActions::zoomIn,							id));
		registrar.add("Contacts.Friends.AddToContactSet",		boost::bind(&FSContactsFriendsMenu::addToContactSet,			this));
		registrar.add("Contacts.Friends.TeleportToTarget",		boost::bind(&FSContactsFriendsMenu::teleportToAvatar,			this));
		registrar.add("Contacts.Friends.TrackAvatar",			boost::bind(&FSContactsFriendsMenu::onTrackAvatarMenuItemClick,	this));
		registrar.add("Contacts.Friends.CopyLabel",				boost::bind(&FSContactsFriendsMenu::copyNameToClipboard,		this, id));
		registrar.add("Contacts.Friends.CopyUrl",				boost::bind(&FSContactsFriendsMenu::copySLURLToClipboard,		this, id));

		enable_registrar.add("Contacts.Friends.EnableItem",		boost::bind(&FSContactsFriendsMenu::enableContextMenuItem,		this, _2));
		enable_registrar.add("Contacts.Friends.EnableZoomIn",	boost::bind(&LLAvatarActions::canZoomIn,						id));

		return createFromFile("menu_fs_contacts_friends.xml");
	}
	else
	{
		registrar.add("Contacts.Friends.SendIM",				boost::bind(&LLAvatarActions::startConference,					mUUIDs, LLUUID::null));
		registrar.add("Contacts.Friends.OfferTeleport",			boost::bind(&FSContactsFriendsMenu::offerTeleport,				this));
		registrar.add("Contacts.Friends.RemoveFriend",			boost::bind(&LLAvatarActions::removeFriendsDialog,				mUUIDs));
		registrar.add("Contacts.Friends.AddToContactSet",		boost::bind(&FSContactsFriendsMenu::addToContactSet,			this));

		enable_registrar.add("Contacts.Friends.EnableItem",		boost::bind(&FSContactsFriendsMenu::enableContextMenuItem,		this, _2));
		
		return createFromFile("menu_fs_contacts_friends_multiselect.xml");
	}
}

bool FSContactsFriendsMenu::enableContextMenuItem(const LLSD& userdata)
{
	std::string item = userdata.asString();

	if (item == "remove_friend")
	{
		bool result = (mUUIDs.size() > 0);

		for (uuid_vec_t::const_iterator it = mUUIDs.begin(); it != mUUIDs.end(); ++it)
		{
			if (!LLAvatarActions::isFriend(*it))
			{
				result = false;
				break;
			}
		}

		return result;
	}
	else if (item == "teleport_to")
	{
		if (mUUIDs.size() == 1)
		{
			return (FSRadar::getInstance()->getEntry(mUUIDs.front()) != NULL);
		}
		return false;
	}
	else if (item == "offer_teleport")
	{
		return LLAvatarActions::canOfferTeleport(mUUIDs);
	}
	else if (item == "request_teleport")
	{
		if (mUUIDs.size() == 1)
		{
			return LLAvatarActions::canRequestTeleport(mUUIDs.front());
		}
		return false;
	}
	else if (item == "track_avatar")
	{
		if (mUUIDs.size() == 1)
		{
			return (FSRadar::getInstance()->getEntry(mUUIDs.front()) != NULL);
		}
		return false;
	}
	return false;
}

void FSContactsFriendsMenu::offerTeleport()
{
	LLAvatarActions::offerTeleport(mUUIDs);
}
	
void FSContactsFriendsMenu::teleportToAvatar()
{
	LLAvatarActions::teleportTo(mUUIDs.front());
}

void FSContactsFriendsMenu::onTrackAvatarMenuItemClick()
{
	LLAvatarActions::track(mUUIDs.front());
}

void FSContactsFriendsMenu::addToContactSet()
{
	LLAvatarActions::addToContactSet(mUUIDs);
}

void FSContactsFriendsMenu::copyNameToClipboard(const LLUUID& id)
{
	LLAvatarName av_name;
	LLAvatarNameCache::get(LLUUID(id), &av_name);
	LLUrlAction::copyURLToClipboard(av_name.getAccountName());
}

void FSContactsFriendsMenu::copySLURLToClipboard(const LLUUID& id)
{
	LLUrlAction::copyURLToClipboard(LLSLURL("agent", id, "about").getSLURLString());
}

