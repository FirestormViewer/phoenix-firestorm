/**
 * @file fspinnedfriends.cpp
 * @brief Pinned friends storage management
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (C) 2026, The Phoenix Firestorm Project, Inc.
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

// Mirrors fsfavoritegroups.cpp's pattern exactly, just for the friends list
// instead of the groups list. The only real difference is the staleness
// check below: groups drops an id you're no longer a member of, this drops
// an id that's no longer on your friends list at all
// (LLAvatarTracker::isBuddy). This is an FS-only file, so no original LL
// code is being preserved and no <FS:TP> tags are needed per CONTRIBUTING.md.
#include "llviewerprecompiledheaders.h"

#include "fspinnedfriends.h"

#include "llcallingcard.h" // for LLAvatarTracker
#include "llviewercontrol.h"

FSPinnedFriends::FSPinnedFriends()
{
}

FSPinnedFriends::~FSPinnedFriends()
{
    savePinned();
}

bool FSPinnedFriends::isPinned(const LLUUID& avatar_id) const
{
    return mPinnedFriends.contains(avatar_id);
}

void FSPinnedFriends::addPinned(const LLUUID& avatar_id)
{
    if (avatar_id.isNull())
    {
        return;
    }

    if (mPinnedFriends.insert(avatar_id).second)
    {
        savePinned();
        mPinnedChangedSignal();
    }
}

void FSPinnedFriends::removePinned(const LLUUID& avatar_id)
{
    if (mPinnedFriends.erase(avatar_id) > 0)
    {
        savePinned();
        mPinnedChangedSignal();
    }
}

void FSPinnedFriends::togglePinned(const LLUUID& avatar_id)
{
    if (isPinned(avatar_id))
    {
        removePinned(avatar_id);
    }
    else
    {
        addPinned(avatar_id);
    }
}

void FSPinnedFriends::loadPinned()
{
    mPinnedFriends.clear();

    if (LLSD pinned = gSavedPerAccountSettings.getLLSD("FSPinnedFriends"); pinned.isArray())
    {
        for (const auto& avatarId : llsd::inArray(pinned))
        {
            if (avatarId.isUUID())
            {
                mPinnedFriends.insert(avatarId.asUUID());
            }
        }
    }

    LL_INFOS("PinnedFriends") << "Loaded " << mPinnedFriends.size() << " pinned friends from per-account settings" << LL_ENDL;
}

void FSPinnedFriends::savePinned()
{
    uuid_set_t stale_friends;
    for (const auto& avatar_id : mPinnedFriends)
    {
        if (!LLAvatarTracker::instance().isBuddy(avatar_id))
        {
            stale_friends.insert(avatar_id);
        }
    }

    if (!stale_friends.empty())
    {
        for (const auto& avatar_id : stale_friends)
        {
            mPinnedFriends.erase(avatar_id);
        }
        LL_INFOS("PinnedFriends") << "Removed " << stale_friends.size() << " stale friend(s) from pinned list (no longer a friend)" << LL_ENDL;
    }

    LLSD pinned_array = LLSD::emptyArray();

    for (const auto& avatar_id : mPinnedFriends)
    {
        pinned_array.append(avatar_id);
    }

    gSavedPerAccountSettings.setLLSD("FSPinnedFriends", pinned_array);

    LL_DEBUGS("PinnedFriends") << "Saved " << mPinnedFriends.size() << " pinned friends to per-account settings" << LL_ENDL;
}
