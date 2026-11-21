/**
 * @file fspinnedfriends.h
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

// Mirrors fsfavoritegroups.h's pattern exactly, just for the friends list
// instead of the groups list. This is an FS-only file, so no original LL
// code is being preserved and no <FS:TP> tags are needed per CONTRIBUTING.md.
#ifndef FS_PINNEDFRIENDS_H
#define FS_PINNEDFRIENDS_H

#include "llsingleton.h"
#include "lluuid.h"

class FSPinnedFriends : public LLSingleton<FSPinnedFriends>
{
    LLSINGLETON(FSPinnedFriends);
    ~FSPinnedFriends();

public:
    bool isPinned(const LLUUID& avatar_id) const;
    void addPinned(const LLUUID& avatar_id);
    void removePinned(const LLUUID& avatar_id);
    void togglePinned(const LLUUID& avatar_id);
    const uuid_set_t& getPinned() const { return mPinnedFriends; }
    bool hasPinned() const { return !mPinnedFriends.empty(); }
    void loadPinned();
    void savePinned();

    typedef boost::signals2::signal<void()> pinned_changed_signal_t;
    boost::signals2::connection setPinnedChangedCallback(const pinned_changed_signal_t::slot_type& cb)
    {
        return mPinnedChangedSignal.connect(cb);
    }

private:
    uuid_set_t mPinnedFriends;
    pinned_changed_signal_t mPinnedChangedSignal;
};

#endif // FS_PINNEDFRIENDS_H
