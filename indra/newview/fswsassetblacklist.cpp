/**
 * @file fswsassetblacklist.cpp
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

#include "llviewerprecompiledheaders.h"
#include "fswsassetblacklist.h"
#include "llsdserialize.h"
#include "llassettype.h"
#include "llstring.h"
#include "llviewerregion.h"
#include "llagent.h"
#include "fsfloaterwsassetblacklist.h"
#include "llfloaterreg.h"
#include "llvfs.h"
#include "llaudioengine.h"


std::string FSWSAssetBlacklist::blacklist_file_name;
std::map<LLUUID,LLSD> FSWSAssetBlacklist::BlacklistData;
BlacklistMAP FSWSAssetBlacklist::BlacklistIDs;

LLAssetType::EType S32toAssetType(S32 assetindex)
{
	LLAssetType::EType type;
	switch (assetindex)
	{
		case 0:
			type = LLAssetType::AT_TEXTURE;
			break;
		case 1:
			type = LLAssetType::AT_SOUND;
			break;
		case 6:
			type = LLAssetType::AT_OBJECT;
			break;
		default:
			type = LLAssetType::AT_NONE;
	}
	return type;
}

void FSWSAssetBlacklist::init()
{
	blacklist_file_name = gDirUtilp->getExpandedFilename(LL_PATH_PER_SL_ACCOUNT, "asset_blacklist.xml");
	loadBlacklist();
}

bool FSWSAssetBlacklist::isBlacklisted(LLUUID id, LLAssetType::EType type)
{
	if (BlacklistData.size() == 0)
	{
		return false;
	}
	
	id = LLUUID::generateNewID(id.asString() + "hash");

	BlacklistMAP::iterator it;
	it = BlacklistIDs.find(type);	
	
	if (it == BlacklistIDs.end())
	{
		return false;
	}

	std::vector<LLUUID> uuids = it->second;
	if (std::find(uuids.begin(), uuids.end(), id) != uuids.end())
	{
		return true;
	}

	return false;
}

void FSWSAssetBlacklist::addNewItemToBlacklist(LLUUID id, std::string name, std::string region, LLAssetType::EType type, bool save)
{
	LLDate curdate = LLDate(time_corrected());
	std::string input_date = curdate.asString();
	input_date.replace(input_date.find("T"),1," ");
	input_date.resize(input_date.size() - 1);
	
	LLSD data;
	data["asset_name"] = name;
	data["asset_region"] = region;
	data["asset_type"] = type;
	data["asset_date"] = input_date;

	addNewItemToBlacklistData(LLUUID::generateNewID(id.asString() + "hash"), data, save);
}

void FSWSAssetBlacklist::removeItemFromBlacklist(LLUUID id)
{
	std::map<LLUUID,LLSD>::iterator it;
	it = BlacklistData.find(id);
	if (it == BlacklistData.end())
	{
		return;
	}

	LLSD data = it->second;
	
	BlacklistIDs[S32toAssetType(data["asset_type"].asInteger())].erase(
			std::remove(BlacklistIDs[S32toAssetType(data["asset_type"].asInteger())].begin(),
			BlacklistIDs[S32toAssetType(data["asset_type"].asInteger())].end(), id),
			BlacklistIDs[S32toAssetType(data["asset_type"].asInteger())].end());

	BlacklistData.erase(id);

	saveBlacklist();	
}

void FSWSAssetBlacklist::addNewItemToBlacklistData(LLUUID id, LLSD data, bool save)
{
	LLAssetType::EType type = S32toAssetType(data["asset_type"].asInteger());

	addEntryToBlacklistMap(id,type);
	BlacklistData.insert(std::pair<LLUUID,LLSD>(id,data));

	if (save)
	{
		saveBlacklist();
	}

	FSFloaterWSAssetBlacklist* floater = LLFloaterReg::getTypedInstance<FSFloaterWSAssetBlacklist>("ws_asset_blacklist");
	if (floater)
	{
		floater->addElementToList(id, data);
	}
}

bool FSWSAssetBlacklist::addEntryToBlacklistMap(LLUUID id, LLAssetType::EType type)
{
	if (id.isNull())
	{
		return false;
	}

	std::stringstream typesstream;
	typesstream << (int)type;
	std::string types = typesstream.str();
	std::map<LLAssetType::EType,std::vector<LLUUID> >::iterator it;
	it = BlacklistIDs.find(type);
	
	if (it == BlacklistIDs.end())
	{
		std::vector<LLUUID> vec;
		vec.push_back(id);
		BlacklistIDs[type] = vec;
		it = BlacklistIDs.find(type);
		return true;
	} 
	
	if (it != BlacklistIDs.end())
	{
		BlacklistIDs[type].push_back(id);
		return true;
	}
	return false;
}

void FSWSAssetBlacklist::loadBlacklist()
{
	if (gDirUtilp->fileExists(blacklist_file_name))
	{
		llifstream blacklistdata(blacklist_file_name);
		if (blacklistdata.is_open())
		{
			LLSD data;
			if (LLSDSerialize::fromXML(data, blacklistdata) >= 1)
			{
				for(LLSD::map_iterator itr = data.beginMap(); itr != data.endMap(); ++itr)
				{
					LLUUID uid = LLUUID(itr->first);
					LLSD data = itr->second;
					if(uid.isNull()) continue;
					LLAssetType::EType type = S32toAssetType(data["asset_type"].asInteger());

					if(type == LLAssetType::AT_NONE) continue;
					
					addNewItemToBlacklistData(uid, data, false);
				}
			}
		}
		blacklistdata.close();
	}
	else
	{
		// Try to import old blacklist data from Phoenix
		std::string old_file = gDirUtilp->getOSUserDir() + gDirUtilp->getDirDelimiter() + "SecondLife" + gDirUtilp->getDirDelimiter() + "user_settings" + gDirUtilp->getDirDelimiter() + "floater_blist_settings.xml";
		if (gDirUtilp->fileExists(old_file))
		{
			LLSD datallsd;
			llifstream oldfile;
			oldfile.open(old_file.c_str());
			if (oldfile.is_open())
			{
				LLSDSerialize::fromXMLDocument(datallsd, oldfile);
				for (LLSD::map_iterator itr = datallsd.beginMap(); itr != datallsd.endMap(); ++itr)
				{
					LLUUID uid = LLUUID(itr->first);
					LLSD data = itr->second;
					if (uid.isNull() || !data.has("entry_name") || !data.has("entry_type") || !data.has("entry_date"))
					{
						continue;
					}
					LLAssetType::EType type = S32toAssetType(data["entry_type"].asInteger());
					
					LLSD newdata;
					newdata["asset_name"] = "[PHOENIX] " + data["entry_name"].asString();
					newdata["asset_type"] = type;
					newdata["asset_date"] = data["entry_date"].asString();

					if (!data["ID_hashed"].asBoolean())
					{
						uid = LLUUID::generateNewID(uid.asString() + "hash");
					}				
					
					addNewItemToBlacklistData(uid, newdata, false);
				}
			}
			oldfile.close();
			saveBlacklist();
			llinfos << "Using old Phoenix file: " << old_file << llendl;
		}
		else
		{
			llinfos << "No Settings file found." << old_file << llendl;
		}
	}
}

void FSWSAssetBlacklist::saveBlacklist()
{
	llofstream save_file(blacklist_file_name);
	LLSD savedata;

	for (std::map<LLUUID,LLSD>::iterator itr = BlacklistData.begin(); itr != BlacklistData.end(); ++itr)
	{
		savedata[itr->first.asString()] = itr->second;
	}

	LLSDSerialize::toPrettyXML(savedata, save_file);	
	save_file.close();
}
