/**
 * @file fsassetblacklist.h
 * @brief Asset Blacklist and Derender
 *
 * $LicenseInfo:firstyear=2012&license=fsviewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (C) 2012, Wolfspirit Magic
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

#include <boost/unordered_map.hpp>
#include <boost/unordered_set.hpp>

#include "llsingleton.h"
#include "llassettype.h"

typedef boost::unordered_set<LLUUID, FSUUIDHash> blacklisted_uuid_container_t;
typedef std::map<LLAssetType::EType, blacklisted_uuid_container_t> blacklist_type_map_t;
typedef boost::unordered_map<LLUUID, LLSD, FSUUIDHash> blacklist_data_t;

class FSAssetBlacklist : public LLSingleton<FSAssetBlacklist>
{
public:
	void init();
	bool isBlacklisted(const LLUUID& id, LLAssetType::EType type);
	void addNewItemToBlacklist(const LLUUID& id, const std::string& name, const std::string& region, LLAssetType::EType type, bool permanent = true, bool save = true);
	void addNewItemToBlacklistData(const LLUUID& id, const LLSD& data, bool save = true);
	void removeItemFromBlacklist(const LLUUID& id);
	void removeItemsFromBlacklist(const uuid_vec_t& ids);
	void saveBlacklist();

	blacklist_data_t getBlacklistData() const { return mBlacklistData; };

private:
	void loadBlacklist();
	void removeItem(const LLUUID& id);
	bool addEntryToBlacklistMap(const LLUUID& id, LLAssetType::EType type);
	
	std::string				mBlacklistFileName;
	blacklist_type_map_t	mBlacklistTypeContainer;
	blacklist_data_t		mBlacklistData;
};

#endif // FS_ASSETBLACKLIST_H
