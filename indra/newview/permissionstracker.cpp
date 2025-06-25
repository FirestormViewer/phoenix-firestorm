/**
 * @file permissionstracker.cpp
 * @brief Permissions Tracker implementation - Initially it's only tracking
 * camera control permissions, to warn users about attachments or seats that
 * took camera control permissions and might interfere with resetting the
 * camera view.
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 * Copyright (C) 2022, Zi Ree @ Second Life
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

#include "llagentdata.h"            // for gAgentID anf gAgentSessionID
#include "llnotificationsutil.h"
#include "llslurl.h"
#include "lltrans.h"
#include "llviewermenu.h"           // for handle_object_edit()
#include "llviewerregion.h"
#include "llviewerobject.h"
#include "llviewerobjectlist.h"     // for gObjectList
#include "llvoavatarself.h"         // for gAgentAvatarp
#include "llworld.h"

#include "permissionstracker.h"

constexpr F64 PERMISSION_ENTRY_EXPIRY_TIME{ 3600.0 };

PermissionsTracker::PermissionsTracker()
:   LLSingleton<PermissionsTracker>()
{
}

PermissionsTracker::~PermissionsTracker()
{
}

void PermissionsTracker::addPermissionsEntry(const LLUUID& source_id, PermissionsTracker::PERM_TYPE permission_type)
{
    // find out if this is a new entry in the list
    if (mPermissionsList.find(source_id) == mPermissionsList.end())
    {
        LL_DEBUGS("PermissionsTracker") << "Creating list entry for source " << source_id << LL_ENDL;
        mPermissionsList[source_id].objectName = LLTrans::getString("LoadingData");

        // find out if the object is still in reach
        if (LLViewerObject* vo = gObjectList.findObject(source_id); vo && isAgentAvatarValid() && gAgentAvatarp->getRegion())
        {
            mPermissionsList[source_id].attachmentID = vo->getAttachmentItemID();
            LL_DEBUGS("PermissionsTracker") << "Requesting ObjectProperties for source " << source_id << LL_ENDL;

            // remember which object names we already requested
            mRequestedIDs.emplace_back(source_id);

            // send a request out to get this object's details
            LLMessageSystem* msg = gMessageSystem;

            msg->newMessageFast(_PREHASH_ObjectSelect);
            msg->nextBlockFast(_PREHASH_AgentData);
            msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
            msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
            msg->nextBlockFast(_PREHASH_ObjectData);
            msg->addU32Fast(_PREHASH_ObjectLocalID, vo->getLocalID());
            msg->sendReliable(gAgentAvatarp->getRegion()->getHost());

            msg->newMessageFast(_PREHASH_ObjectDeselect);
            msg->nextBlockFast(_PREHASH_AgentData);
            msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
            msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
            msg->nextBlockFast(_PREHASH_ObjectData);
            msg->addU32Fast(_PREHASH_ObjectLocalID, vo->getLocalID());
            msg->sendReliable(gAgentAvatarp->getRegion()->getHost());
        }
        else
        {
            mPermissionsList[source_id].objectName = LLTrans::getString("ObjectOutOfRange");
        }
    }

    LL_DEBUGS("PermissionsTracker") << "Adding permission type " << permission_type << " to source " << source_id << LL_ENDL;
    mPermissionsList[source_id].type |= permission_type;
    mPermissionsList[source_id].time = LLDate(LLTimer::getTotalSeconds());

    purgePermissionsEntries();
}

void PermissionsTracker::removePermissionsEntry(const LLUUID& source_id, PermissionsTracker::PERM_TYPE permission_type)
{
    if (mPermissionsList.find(source_id) == mPermissionsList.end())
    {
        LL_DEBUGS("PermissionsTracker") << "Could not find list entry for source " << source_id << LL_ENDL;
        return;
    }

    LL_DEBUGS("PermissionsTracker") << "Removing permissions type " << permission_type << " from source " << source_id << LL_ENDL;
    mPermissionsList[source_id].type &= ~permission_type;
    mPermissionsList[source_id].time = LLDate(LLTimer::getTotalSeconds());

    purgePermissionsEntries();
}

void PermissionsTracker::purgePermissionsEntries()
{
    F64 expiry_time = LLDate(LLTimer::getTotalSeconds()).secondsSinceEpoch() - PERMISSION_ENTRY_EXPIRY_TIME;

    auto it = mPermissionsList.begin();
    while (it != mPermissionsList.end())
    {
        if (it->second.type == PERM_TYPE::PERM_NONE && it->second.time.secondsSinceEpoch() < expiry_time)
        {
            LL_DEBUGS("PermissionsTracker") << "Erasing list entry for source " << it->first << LL_ENDL;
            it = mPermissionsList.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void PermissionsTracker::warnFollowcam()
{
    std::string followcamList;
    for (const auto& entry : mPermissionsList)
    {
        if (entry.second.type & PermissionsTracker::PERM_FOLLOWCAM)
        {
            if (entry.second.attachmentID.notNull())
            {
                std::string attachment_point = "???";
                gAgentAvatarp->getAttachedPointName(entry.second.attachmentID, attachment_point);

                LLSD args;
                args["ATTACHMENT_POINT"] = attachment_point;

                std::string verb = "select?name=" + LLURI::escape(entry.second.objectName);
                followcamList += LLSLURL("inventory", entry.second.attachmentID, verb.c_str()).getSLURLString() + " " +
                    LLTrans::getString("WornOnAttachmentPoint", args) + "\n";
            }
            else
            {
                followcamList += LLSLURL("objectim", entry.first, "").getSLURLString() +
                    "?name=" + LLURI::escape(entry.second.objectName) +
                    "&owner=" + entry.second.ownerID.asString();

                LLSD args;
                std::string slurl = args["slurl"].asString();
                if (slurl.empty())
                {
                    if (LLViewerRegion* region = LLWorld::instance().getRegionFromPosAgent(gAgentAvatarp->getPositionAgent()); region)
                    {
                        LLSLURL region_slurl(region->getName(), gAgentAvatarp->getPositionAgent());
                        slurl = region_slurl.getLocationString();
                    }
                }

                followcamList += "&slurl=" + LLURI::escape(slurl) + "\n";
            }
        }
    }

    if (followcamList.empty())
    {
        return;
    }

    LLSD args;
    args["SOURCES"] = followcamList;
    LLNotificationsUtil::add("WarnScriptedCamera", args);
}

void PermissionsTracker::objectPropertiesCallback(LLMessageSystem* msg)
{
    LL_DEBUGS("PermissionsTracker") << "Received ObjectProperties message." << LL_ENDL;

    // if we weren't looking for any IDs, ignore this callback
    if (mRequestedIDs.empty())
    {
        LL_DEBUGS("PermissionsTracker") << "No objects in request list." << LL_ENDL;
        return;
    }

    // we might have received more than one answer in one block
    S32 num = msg->getNumberOfBlocksFast(_PREHASH_ObjectData);
    for (S32 index = 0; index < num; ++index)
    {
        LLUUID source_id;
        msg->getUUIDFast(_PREHASH_ObjectData, _PREHASH_ObjectID, source_id, index);

        // if this is one of the objects we were looking for, process the data
        if (auto iter = std::find(mRequestedIDs.begin(), mRequestedIDs.end(), source_id); iter != mRequestedIDs.end())
        {
            // get the name of the object
            std::string object_name;
            msg->getStringFast(_PREHASH_ObjectData, _PREHASH_Name, object_name, index);

            LLUUID object_owner;
            msg->getUUIDFast(_PREHASH_ObjectData, _PREHASH_OwnerID, object_owner, index);

            LL_DEBUGS("PermissionsTracker") << "Received object name " << object_name
                << " and owner " << object_owner.asString() << " for source " << source_id << LL_ENDL;

            // remove the object from the lookup list and add it to the known names list
            mRequestedIDs.erase(iter);

            mPermissionsList[source_id].objectName = object_name;
            mPermissionsList[source_id].ownerID = object_owner;

            if (LLAvatarName avatar_name; LLAvatarNameCache::get(object_owner, &avatar_name))
            {
                LL_DEBUGS("PermissionsTracker") << "Found cached entry for owner " << object_owner.asString()
                    << ": " << avatar_name.getCompleteName() << LL_ENDL;
                mPermissionsList[source_id].ownerName = avatar_name.getCompleteName();
            }
            else if (mAvatarNameCacheConnections.find(object_owner) != mAvatarNameCacheConnections.end())
            {
                LL_DEBUGS("PermissionsTracker") << "Requesting avatar name for owner " << object_owner.asString() << LL_ENDL;
                mAvatarNameCacheConnections.try_emplace(object_owner, LLAvatarNameCache::get(object_owner, boost::bind(&PermissionsTracker::avatarNameCallback, this, _1, _2)));
            }
        }
    }
}

void PermissionsTracker::avatarNameCallback(const LLUUID& avatar_id, const LLAvatarName& avatar_name)
{
    LL_DEBUGS("PermissionsTracker") << "Received avatar name " << avatar_name.getCompleteName() << LL_ENDL;

    if (auto iter = mAvatarNameCacheConnections.find(avatar_id); iter != mAvatarNameCacheConnections.end())
    {
        if (iter->second.connected())
        {
            iter->second.disconnect();
        }
        mAvatarNameCacheConnections.erase(iter);
    }

    for (auto& entry : mPermissionsList)
    {
        if (entry.second.ownerID == avatar_id)
        {
            LL_DEBUGS("PermissionsTracker") << "Saved avatar name " << avatar_name.getCompleteName()<< " for source " << entry.first.asString() << LL_ENDL;
            entry.second.ownerName = avatar_name.getCompleteName();
        }
    }
}
