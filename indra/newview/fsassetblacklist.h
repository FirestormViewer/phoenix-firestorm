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

struct FSAssetBlacklistData
{
    std::string name;
    std::string region;
    LLAssetType::EType type;
    S32 flags{ 0 };
    LLDate date;
    bool permanent{ false };

    LLSD toLLSD() const;
    static FSAssetBlacklistData fromLLSD(const LLSD& data);
};

using blacklisted_uuid_container_t = std::unordered_set<LLUUID>;
using blacklist_type_map_t = std::map<LLAssetType::EType, blacklisted_uuid_container_t>;
using blacklist_data_t = std::unordered_map<LLUUID, FSAssetBlacklistData>;

class FSAssetBlacklist : public LLSingleton<FSAssetBlacklist>
{
    LLSINGLETON_EMPTY_CTOR(FSAssetBlacklist);

public:
    enum eBlacklistFlag
    {
        NONE = 0,
        WORN = 1 << 0,
        REZZED = 1 << 1,
        GESTURE = 1 << 2,

        LAST_FLAG = 1 << 2
    };

    void init();
    bool isBlacklisted(const LLUUID& id, LLAssetType::EType type, eBlacklistFlag flag = eBlacklistFlag::NONE) const;
    void addNewItemToBlacklist(const LLUUID& id, const std::string& name, const std::string& region, LLAssetType::EType type, eBlacklistFlag flag = eBlacklistFlag::NONE,bool permanent = true, bool save = true);
    void addNewItemToBlacklistData(const LLUUID& id, const FSAssetBlacklistData& data, bool save = true);
    void removeItemFromBlacklist(const LLUUID& id);
    void removeItemsFromBlacklist(const uuid_vec_t& ids);
    void removeFlagsFromItem(const LLUUID& id, S32 combined_flags);
    void saveBlacklist();

    blacklist_data_t getBlacklistData() const { return mBlacklistData; };

    enum class eBlacklistOperation
    {
        BLACKLIST_ADD,
        BLACKLIST_REMOVE
    };

    using changed_signal_data_t        = std::vector<std::pair<LLUUID, std::optional<FSAssetBlacklistData>>>;
    using blacklist_changed_callback_t = boost::signals2::signal<void(const changed_signal_data_t& data, eBlacklistOperation op)>;
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
