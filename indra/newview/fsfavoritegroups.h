/**
 * @file fsfavoritegroups.h
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

#ifndef FS_FAVORITEGROUPS_H
#define FS_FAVORITEGROUPS_H

#include "llsingleton.h"
#include "lluuid.h"

class FSFavoriteGroups : public LLSingleton<FSFavoriteGroups>
{
    LLSINGLETON(FSFavoriteGroups);
    ~FSFavoriteGroups();

public:
    bool isFavorite(const LLUUID& group_id) const;
    void addFavorite(const LLUUID& group_id);
    void removeFavorite(const LLUUID& group_id);
    void toggleFavorite(const LLUUID& group_id);
    const uuid_set_t& getFavorites() const { return mFavoriteGroups; }
    bool hasFavorites() const { return !mFavoriteGroups.empty(); }
    void loadFavorites();
    void saveFavorites();

    typedef boost::signals2::signal<void()> favorites_changed_signal_t;
    boost::signals2::connection setFavoritesChangedCallback(const favorites_changed_signal_t::slot_type& cb)
    {
        return mFavoritesChangedSignal.connect(cb);
    }

private:
    uuid_set_t mFavoriteGroups;
    favorites_changed_signal_t mFavoritesChangedSignal;
};

#endif // FS_FAVORITEGROUPS_H
