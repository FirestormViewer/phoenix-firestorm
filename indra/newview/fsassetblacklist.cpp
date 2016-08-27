/**
 * @file fsassetblacklist.cpp
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

#include "llviewerprecompiledheaders.h"

#include "fsassetblacklist.h"

#include "fsfloaterassetblacklist.h"
#include "llaudioengine.h"
#include "llfloaterreg.h"
#include "llsdserialize.h"
#include "llvfs.h"
#include "llxorcipher.h"
#include "llviewerobjectlist.h"

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
		case 45:
			type = LLAssetType::AT_PERSON;
			break;
		default:
			type = LLAssetType::AT_NONE;
	}
	return type;
}

void FSAssetBlacklist::init()
{
	mBlacklistFileName = gDirUtilp->getExpandedFilename(LL_PATH_PER_SL_ACCOUNT, "asset_blacklist.xml");
	loadBlacklist();
}

bool FSAssetBlacklist::isBlacklisted(const LLUUID& id, LLAssetType::EType type)
{
	if (mBlacklistData.empty())
	{
		return false;
	}

	blacklist_type_map_t::iterator it;
	it = mBlacklistTypeContainer.find(type);
	
	if (it == mBlacklistTypeContainer.end())
	{
		return false;
	}

	blacklisted_uuid_container_t uuids = it->second;
	return (uuids.find(id) != uuids.end());
}

void FSAssetBlacklist::addNewItemToBlacklist(const LLUUID& id, const std::string& name, const std::string& region, LLAssetType::EType type, bool permanent /*= true*/, bool save /*= true*/)
{
	if (isBlacklisted(id, type))
	{
		return;
	}

	LLDate curdate = LLDate(time_corrected());
	std::string input_date = curdate.asString();
	input_date.replace(input_date.find("T"), 1, " ");
	input_date.resize(input_date.size() - 1);
	
	LLSD data;
	data["asset_name"] = name;
	data["asset_region"] = region;
	data["asset_type"] = type;
	data["asset_date"] = input_date;
	data["asset_permanent"] = permanent;

	addNewItemToBlacklistData(id, data, save);
}

void FSAssetBlacklist::removeItem(const LLUUID& id)
{
	gObjectList.removeDerenderedItem(id);

	blacklist_data_t::iterator it;
	it = mBlacklistData.find(id);

	if (it == mBlacklistData.end())
	{
		return;
	}

	LLSD data = it->second;
	LLAssetType::EType type = S32toAssetType(data["asset_type"].asInteger());

	mBlacklistTypeContainer[type].erase(id);
	mBlacklistData.erase(it);
}

void FSAssetBlacklist::removeItemFromBlacklist(const LLUUID& id)
{
	uuid_vec_t ids;
	ids.push_back(id);
	removeItemsFromBlacklist(ids);
}

void FSAssetBlacklist::removeItemsFromBlacklist(const uuid_vec_t& ids)
{
	LLSD data;

	for (uuid_vec_t::const_iterator it = ids.begin(); it != ids.end(); ++it)
	{
		removeItem(*it);
		data.append((*it).asString());
	}
	saveBlacklist();

	if (!mBlacklistChangedCallback.empty())
	{
		mBlacklistChangedCallback(data, BLACKLIST_REMOVE);
	}
}

void FSAssetBlacklist::addNewItemToBlacklistData(const LLUUID& id, const LLSD& data, bool save)
{
	LLAssetType::EType type = S32toAssetType(data["asset_type"].asInteger());

	addEntryToBlacklistMap(id, type);
	mBlacklistData[id] = data;

	if (type == LLAssetType::AT_SOUND)
	{
		gVFS->removeFile(id, LLAssetType::AT_SOUND);
		std::string wav_path = gDirUtilp->getExpandedFilename(LL_PATH_FS_SOUND_CACHE, id.asString()) + ".dsf";
		if (gDirUtilp->fileExists(wav_path))
		{
			LLFile::remove(wav_path);
		}
		if (gAudiop)
		{
			gAudiop->removeAudioData(id);
		}
	}

	if (save)
	{
		saveBlacklist();
	}

	if (!mBlacklistChangedCallback.empty())
	{
		mBlacklistChangedCallback(LLSD().with(id.asString(), data), BLACKLIST_ADD);
	}
}

bool FSAssetBlacklist::addEntryToBlacklistMap(const LLUUID& id, LLAssetType::EType type)
{
	if (id.isNull())
	{
		return false;
	}

	blacklist_type_map_t::iterator it;
	it = mBlacklistTypeContainer.find(type);
	
	if (it != mBlacklistTypeContainer.end())
	{
		mBlacklistTypeContainer[type].insert(id);
	}
	else
	{
		blacklisted_uuid_container_t cont;
		cont.insert(id);
		mBlacklistTypeContainer[type] = cont;
	}
	return true;
}

void FSAssetBlacklist::loadBlacklist()
{
	if (gDirUtilp->fileExists(mBlacklistFileName))
	{
		llifstream blacklist_data_stream(mBlacklistFileName.c_str());
		if (blacklist_data_stream.is_open())
		{
			LLSD data;
			if (LLSDSerialize::fromXML(data, blacklist_data_stream) >= 1)
			{
				for (LLSD::map_const_iterator itr = data.beginMap(); itr != data.endMap(); ++itr)
				{
					LLUUID uid = LLUUID(itr->first);
					LLXORCipher cipher(MAGIC_ID.mData, UUID_BYTES);
					cipher.decrypt(uid.mData, UUID_BYTES);
					LLSD entry_data = itr->second;
					entry_data["asset_permanent"] = true; // For conversion of old data
					if (uid.isNull())
					{
						continue;
					}

					LLAssetType::EType type = S32toAssetType(entry_data["asset_type"].asInteger());
					if (type == LLAssetType::AT_NONE)
					{
						continue;
					}
					else if (type == LLAssetType::AT_OBJECT)
					{
						gObjectList.addDerenderedItem(uid, true);
					}
					
					addNewItemToBlacklistData(uid, entry_data, false);
				}
			}
		}
		blacklist_data_stream.close();
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
					if (type == LLAssetType::AT_OBJECT)
					{
						gObjectList.addDerenderedItem(uid, true);
					}

					LLSD newdata;
					newdata["asset_name"] = "[PHOENIX] " + data["entry_name"].asString();
					newdata["asset_type"] = type;
					newdata["asset_date"] = data["entry_date"].asString();
					newdata["asset_permanent"] = true; // For conversion of old data

					addNewItemToBlacklistData(uid, newdata, false);
				}
			}
			oldfile.close();
			saveBlacklist();
			LL_INFOS("AssetBlacklist") << "Using old Phoenix file: " << old_file << LL_ENDL;
		}
		else
		{
			LL_INFOS("AssetBlacklist") << "No Settings file found." << old_file << LL_ENDL;
		}
	}
}

void FSAssetBlacklist::saveBlacklist()
{
	llofstream save_file(mBlacklistFileName.c_str());
	LLSD savedata;

	for (blacklist_data_t::const_iterator itr = mBlacklistData.begin(); itr != mBlacklistData.end(); ++itr)
	{
		if (itr->second["asset_permanent"].asBoolean())
		{
			LLUUID shadow_id(itr->first);
			LLXORCipher cipher(MAGIC_ID.mData, UUID_BYTES);
			cipher.encrypt(shadow_id.mData, UUID_BYTES);
			savedata[shadow_id.asString()] = itr->second;
		}
	}

	LLSDSerialize::toPrettyXML(savedata, save_file);
	save_file.close();
}
