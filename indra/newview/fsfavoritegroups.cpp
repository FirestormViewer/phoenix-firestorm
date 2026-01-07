/**
 * @file fsfavoritegroups.cpp
 * @brief Favorite groups storage management
 *
 * $LicenseInfo:firstyear=2025&license=viewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (C) 2025, The Phoenix Firestorm Project, Inc.
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

#include "fsfavoritegroups.h"

#include "llagent.h"
#include "llviewercontrol.h"

FSFavoriteGroups::FSFavoriteGroups()
{
}

FSFavoriteGroups::~FSFavoriteGroups()
{
    saveFavorites();
}

bool FSFavoriteGroups::isFavorite(const LLUUID& group_id) const
{
    return mFavoriteGroups.contains(group_id);
}

void FSFavoriteGroups::addFavorite(const LLUUID& group_id)
{
    if (group_id.isNull())
    {
        return;
    }

    if (mFavoriteGroups.insert(group_id).second)
    {
        saveFavorites();
        mFavoritesChangedSignal();
    }
}

void FSFavoriteGroups::removeFavorite(const LLUUID& group_id)
{
    if (mFavoriteGroups.erase(group_id) > 0)
    {
        saveFavorites();
        mFavoritesChangedSignal();
    }
}

void FSFavoriteGroups::toggleFavorite(const LLUUID& group_id)
{
    if (isFavorite(group_id))
    {
        removeFavorite(group_id);
    }
    else
    {
        addFavorite(group_id);
    }
}

void FSFavoriteGroups::loadFavorites()
{
    mFavoriteGroups.clear();


    if (LLSD favorites = gSavedPerAccountSettings.getLLSD("FSFavoriteGroups"); favorites.isArray())
    {
        for (const auto& groupId : llsd::inArray(favorites))
        {
            if (groupId.isUUID())
            {
                mFavoriteGroups.insert(groupId.asUUID());
            }
        }
    }

    LL_INFOS("FavoriteGroups") << "Loaded " << mFavoriteGroups.size() << " favorite groups from per-account settings" << LL_ENDL;
}

void FSFavoriteGroups::saveFavorites()
{
    uuid_set_t stale_groups;
    for (const auto& group_id : mFavoriteGroups)
    {
        if (!gAgent.isInGroup(group_id))
        {
            stale_groups.insert(group_id);
        }
    }
    
    if (!stale_groups.empty())
    {
        for (const auto& group_id : stale_groups)
        {
            mFavoriteGroups.erase(group_id);
        }
        LL_INFOS("FavoriteGroups") << "Removed " << stale_groups.size() << " stale group(s) from favorites (no longer a member)" << LL_ENDL;
    }

    LLSD favorites_array = LLSD::emptyArray();

    for (const auto& group_id : mFavoriteGroups)
    {
        favorites_array.append(group_id);
    }

    gSavedPerAccountSettings.setLLSD("FSFavoriteGroups", favorites_array);

    LL_DEBUGS("FavoriteGroups") << "Saved " << mFavoriteGroups.size() << " favorite groups to per-account settings" << LL_ENDL;
}
