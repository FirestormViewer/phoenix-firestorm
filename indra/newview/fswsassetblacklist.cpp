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

#include "fsfloaterwsassetblacklist.h"
#include "llfloaterreg.h"
#include "llsdserialize.h"
#include "llvfs.h"
#include "llxorcipher.h"


const LLUUID MAGIC_ID("3c115e51-04f4-523c-9fa6-98aff1034730");

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
	mBlacklistFileName = gDirUtilp->getExpandedFilename(LL_PATH_PER_SL_ACCOUNT, "asset_blacklist.xml");
	loadBlacklist();
}

bool FSWSAssetBlacklist::isBlacklisted(const LLUUID& id, LLAssetType::EType type)
{
	if (mBlacklistData.empty())
	{
		return false;
	}

	t_blacklist_type_map::iterator it;
	it = mBlacklistTypeContainer.find(type);	
	
	if (it == mBlacklistTypeContainer.end())
	{
		return false;
	}

	t_blacklisted_uuid_container uuids = it->second;
	if (std::find(uuids.begin(), uuids.end(), id) != uuids.end())
	{
		return true;
	}

	return false;
}

void FSWSAssetBlacklist::addNewItemToBlacklist(const LLUUID& id, const std::string& name, const std::string& region, LLAssetType::EType type, bool save)
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

	addNewItemToBlacklistData(id, data, save);
}

void FSWSAssetBlacklist::removeItemFromBlacklist(const LLUUID& id)
{
	t_blacklist_data::iterator it;
	it = mBlacklistData.find(id);
	if (it == mBlacklistData.end())
	{
		return;
	}

	LLSD data = it->second;
	LLAssetType::EType type = S32toAssetType(data["asset_type"].asInteger());

	mBlacklistTypeContainer[type].erase(
			std::remove(mBlacklistTypeContainer[type].begin(),
			mBlacklistTypeContainer[type].end(), id),
			mBlacklistTypeContainer[type].end());

	mBlacklistData.erase(id);

	saveBlacklist();	
}

void FSWSAssetBlacklist::addNewItemToBlacklistData(const LLUUID& id, const LLSD& data, bool save)
{
	LLAssetType::EType type = S32toAssetType(data["asset_type"].asInteger());

	addEntryToBlacklistMap(id,type);
	mBlacklistData.insert(std::pair<LLUUID,LLSD>(id,data));

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

bool FSWSAssetBlacklist::addEntryToBlacklistMap(const LLUUID& id, LLAssetType::EType type)
{
	if (id.isNull())
	{
		return false;
	}

	std::stringstream typesstream;
	typesstream << (int)type;
	std::string types = typesstream.str();
	t_blacklist_type_map::iterator it;
	it = mBlacklistTypeContainer.find(type);
	
	if (it != mBlacklistTypeContainer.end())
	{
		mBlacklistTypeContainer[type].push_back(id);
	}
	else
	{
		t_blacklisted_uuid_container vec;
		vec.push_back(id);
		mBlacklistTypeContainer[type] = vec;
	}
	return true;
}

void FSWSAssetBlacklist::loadBlacklist()
{
	if (gDirUtilp->fileExists(mBlacklistFileName))
	{
		llifstream mBlacklistData(mBlacklistFileName);
		if (mBlacklistData.is_open())
		{
			LLSD data;
			if (LLSDSerialize::fromXML(data, mBlacklistData) >= 1)
			{
				for (LLSD::map_const_iterator itr = data.beginMap(); itr != data.endMap(); ++itr)
				{
					LLUUID uid = LLUUID(itr->first);
					LLXORCipher cipher(MAGIC_ID.mData, UUID_BYTES);
					cipher.decrypt(uid.mData, UUID_BYTES);
					LLSD data = itr->second;
					if (uid.isNull())
					{
						continue;
					}
					LLAssetType::EType type = S32toAssetType(data["asset_type"].asInteger());

					if(type == LLAssetType::AT_NONE) continue;
					
					addNewItemToBlacklistData(uid, data, false);
				}
			}
		}
		mBlacklistData.close();
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
				for (LLSD::map_const_iterator itr = datallsd.beginMap(); itr != datallsd.endMap(); ++itr)
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

					//if (!data["ID_hashed"].asBoolean())
					//{
					//	uid = LLUUID::generateNewID(uid.asString() + "hash");
					//}
					
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
	llofstream save_file(mBlacklistFileName);
	LLSD savedata;

	for (t_blacklist_data::const_iterator itr = mBlacklistData.begin(); itr != mBlacklistData.end(); ++itr)
	{
		// <FS:CR> Apply "cheesy encryption" to obfuscate these to the user.
		LLUUID shadow_id(itr->first);
		LLXORCipher cipher(MAGIC_ID.mData, UUID_BYTES);
		cipher.encrypt(shadow_id.mData, UUID_BYTES);
		savedata[shadow_id.asString()] = itr->second;
	}

	LLSDSerialize::toPrettyXML(savedata, save_file);	
	save_file.close();
}
