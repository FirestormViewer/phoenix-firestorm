/**
 * @file fswsassetblacklist.h
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
 * http://www.phoenixviewer.com
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"
#include "llsingleton.h"
#include "llfloater.h"
#include "llassettype.h"


typedef std::map<LLAssetType::EType,std::vector<LLUUID> > BlacklistMAP;

class FSWSAssetBlacklist : public LLSingleton<FSWSAssetBlacklist>
{
public:
	void init();
	bool isBlacklisted(LLUUID id, LLAssetType::EType type);
	void addNewItemToBlacklist(LLUUID id, std::string name, std::string region, LLAssetType::EType type, bool save = true);
	void addNewItemToBlacklistData(LLUUID id, LLSD data, bool save = true);
	void removeItemFromBlacklist(LLUUID id);
	static std::map<LLUUID,LLSD> BlacklistData;

private:
	void loadBlacklist();
	void saveBlacklist();
	bool addEntryToBlacklistMap(LLUUID id, LLAssetType::EType type);
	
	static std::string blacklist_file_name;
	static BlacklistMAP BlacklistIDs;
};
