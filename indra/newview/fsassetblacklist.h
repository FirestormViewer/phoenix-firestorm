/**
 * @file fsassetblacklist.h
 * @brief Asset Blacklist and Derender
 *
 * $LicenseInfo:firstyear=2012&license=fsviewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (C) 2012, Wolfspirit Magic
 * Copyright (C) 2016, Ansariel Hiller
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

#ifndef FS_ASSETBLACKLIST_H
#define FS_ASSETBLACKLIST_H

#include <unordered_map>
#include <unordered_set>

#include "llsingleton.h"
#include "llassettype.h"

using blacklisted_uuid_container_t = std::unordered_set<LLUUID>;
using blacklist_type_map_t = std::map<LLAssetType::EType, blacklisted_uuid_container_t>;
using blacklist_data_t = std::unordered_map<LLUUID, LLSD>;

class FSAssetBlacklist : public LLSingleton<FSAssetBlacklist>
{
    LLSINGLETON_EMPTY_CTOR(FSAssetBlacklist);

public:
    void init();
    bool isBlacklisted(const LLUUID& id, LLAssetType::EType type);
    void addNewItemToBlacklist(const LLUUID& id, const std::string& name, const std::string& region, LLAssetType::EType type, bool permanent = true, bool save = true);
    void addNewItemToBlacklistData(const LLUUID& id, const LLSD& data, bool save = true);
    void removeItemFromBlacklist(const LLUUID& id);
    void removeItemsFromBlacklist(const uuid_vec_t& ids);
    void saveBlacklist();

    blacklist_data_t getBlacklistData() const { return mBlacklistData; };

    enum class eBlacklistOperation
    {
        BLACKLIST_ADD,
        BLACKLIST_REMOVE
    };

    typedef boost::signals2::signal<void(const LLSD& data, eBlacklistOperation op)> blacklist_changed_callback_t;
    boost::signals2::connection setBlacklistChangedCallback(const blacklist_changed_callback_t::slot_type& cb)
    {
        return mBlacklistChangedCallback.connect(cb);
    }

private:
    void loadBlacklist();
    bool removeItem(const LLUUID& id);
    bool addEntryToBlacklistMap(const LLUUID& id, LLAssetType::EType type);

    std::string             mBlacklistFileName;
    blacklist_type_map_t    mBlacklistTypeContainer;
    blacklist_data_t        mBlacklistData;

    blacklist_changed_callback_t mBlacklistChangedCallback;
};

#endif // FS_ASSETBLACKLIST_H
