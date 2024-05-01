/**
 * @file permissionstracker.h
 * @brief Permissions Tracker declaration
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

#ifndef PERMISSIONSTRACKER_H
#define PERMISSIONSTRACKER_H

#include "llfloater.h"
#include "llsingleton.h"

// --------------------------------------------------------------------------
// PermissionsTracker: holds a list of requested script permissions to be
// referred to e.g. when trying to reset the camera view and a script is
// holding a follow cam which micht prevent it from working.
// --------------------------------------------------------------------------

class PermissionsTracker
:   public LLSingleton<PermissionsTracker>
{
    LLSINGLETON(PermissionsTracker);
    ~PermissionsTracker();

    public:
        enum PERM_TYPE
        {
            PERM_NONE = 0,
            PERM_FOLLOWCAM = 1,
        };

        struct PermissionsEntry
        {
            LLUUID ownerID;         // agent who owns the requesting object
            LLUUID attachmentID;    // attachment ID in inventory for attached objects
            std::string ownerName;
            std::string objectName;
            LLDate time;            // time when the permission was granted
            U32 type;               // what kind of permissions were granted
        };

        std::map<LLUUID, PermissionsEntry> mPermissionsList;    // UUID of the requesting object

        void addPermissionsEntry(
            const LLUUID& source_id,
            PermissionsTracker::PERM_TYPE permission_type);     // called in llviewermessage.cpp

        void removePermissionsEntry(
            const LLUUID& source_id,
            PermissionsTracker::PERM_TYPE permission_type);     // called in llviewermessage.cpp

        void purgePermissionsEntries();

        void objectPropertiesCallback(LLMessageSystem* msg);
        void avatarNameCallback(const LLUUID& avatar_id, const LLAvatarName& av_name);

        void warnFollowcam();                                   // Warn the user if a script is holding followcam parameters

        std::vector<LLUUID> mRequestedIDs;                      // list of object IDs we requested named for

        typedef std::map<LLUUID, boost::signals2::connection> avatar_name_cache_connection_map_t;
        avatar_name_cache_connection_map_t mAvatarNameCacheConnections;
};

#endif // PERMISSIONSTRACKER_H
