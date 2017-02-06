/* @file lggcontactsets.cpp
 * Copyright (C) 2011 Greg Hendrickson (LordGregGreg Back)
 *
 * This is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the viewer; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include "llviewerprecompiledheaders.h"

#include "lggcontactsets.h"

#include "fscommon.h"
#include "fsradar.h"
#include "llagent.h"
#include "llavatarnamecache.h"
#include "llcallingcard.h"
#include "lldir.h"
#include "llmutelist.h"
#include "llnotifications.h"
#include "llnotificationsutil.h"
#include "llsdserialize.h"
#include "lluicolor.h"
#include "lluicolortable.h"
#include "llviewercontrol.h"
#include "llvoavatar.h"
#include "fsdata.h"
#include "rlvhandler.h"

const F32 COLOR_DAMPENING = 0.8f;
const std::string CONTACT_SETS_FILE = "settings_friends_groups.xml";
const std::string CS_PSEUDONYM_QUOTED = "'--- ---'";

LGGContactSets::LGGContactSets()
:	mDefaultColor(LLColor4::grey)
{
}

LGGContactSets::~LGGContactSets()
{
	for (avatar_name_cache_connection_map_t::iterator it = mAvatarNameCacheConnections.begin(); it != mAvatarNameCacheConnections.end(); ++it)
	{
		if (it->second.connected())
		{
			it->second.disconnect();
		}
	}
	mAvatarNameCacheConnections.clear();

	for (contact_set_map_t::iterator itr = mContactSets.begin(); itr != mContactSets.end(); ++itr)
	{
		delete itr->second;
	}
	mContactSets.clear();
}

void LGGContactSets::toneDownColor(LLColor4& color) const
{
	color.mV[VALPHA] = COLOR_DAMPENING;
}

bool LGGContactSets::callbackAliasReset(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotification::getSelectedOption(notification, response);
	if (option == 0)
	{
		clearPseudonym(notification["payload"]["agent_id"].asUUID());
	}
	return false;
}

std::string LGGContactSets::getFilename()
{
	std::string path = gDirUtilp->getExpandedFilename(LL_PATH_PER_SL_ACCOUNT, "");

	if (!path.empty())
	{
		path = gDirUtilp->getExpandedFilename(LL_PATH_PER_SL_ACCOUNT, CONTACT_SETS_FILE);
	}
	return path;
}

std::string LGGContactSets::getDefaultFilename()
{
	std::string path = gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, "");

	if (!path.empty())
	{
		path = gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, CONTACT_SETS_FILE);
	}
	return path;
}

LLSD LGGContactSets::exportContactSet(const std::string& set_name)
{
	LLSD ret;

	ContactSet* set = getContactSet(set_name);
	if (set)
	{
		ret["groupname"] = set->mName;
		ret["color"] = set->mColor.getValue();
		ret["notices"] = set->mNotify;
		for (uuid_set_t::iterator friend_itr = set->mFriends.begin(); friend_itr != set->mFriends.end(); ++friend_itr)
		{
			ret["friends"][(*friend_itr).asString()] = "";
		}
	}

	return ret;
}

void LGGContactSets::loadFromDisk()
{
	const std::string filename(getFilename());
	if (filename.empty())
	{
		LL_INFOS("ContactSets") << "No valid user directory." << LL_ENDL;
	}

	if (!gDirUtilp->fileExists(filename))
	{
		std::string defaultName(getDefaultFilename());
		LL_INFOS("ContactSets") << "User settings file doesnt exist, going to try and read default one from " << defaultName.c_str() << LL_ENDL;

		if (gDirUtilp->fileExists(defaultName))
		{
			LLSD blankllsd;
			llifstream file;
			file.open(defaultName.c_str());
			if (file.is_open())
			{
				LLSDSerialize::fromXMLDocument(blankllsd, file);
			}
			file.close();
			importFromLLSD(blankllsd);
			saveToDisk();
		}
		else
		{
			saveToDisk();
		}
	}
	else
	{
		LLSD data;
		llifstream file;
		file.open(filename.c_str());
		if (file.is_open())
		{
			LLSDSerialize::fromXML(data, file);
		}
		file.close();
		importFromLLSD(data);
	}
	mChangedSignal(UPDATED_LISTS);
}

void LGGContactSets::saveToDisk()
{
	llofstream file;
	file.open(getFilename().c_str());
	LLSDSerialize::toPrettyXML(exportToLLSD(), file);
	file.close();
}

bool LGGContactSets::saveContactSetToDisk(const std::string& set_name, const std::string& filename)
{
	if (isValidSet(set_name))
	{
		llofstream file;
		file.open(filename.c_str());
		LLSDSerialize::toPrettyXML(exportContactSet(set_name), file);
		file.close();
		return true;
	}

	return false;
}


LLSD LGGContactSets::exportToLLSD()
{
	LLSD output;

	// Global settings
	output[CS_GLOBAL_SETTINGS]["defaultColor"] = mDefaultColor.getValue();

	// Extra avatars
	for (uuid_set_t::iterator it = mExtraAvatars.begin(); it != mExtraAvatars.end(); ++it)
	{
		output[CS_SET_EXTRA_AVS][(*it).asString()] = "";
	}

	// Pseudonyms
	for (uuid_map_t::iterator it = mPseudonyms.begin(); it != mPseudonyms.end(); ++it)
	{
		output[CS_SET_PSEUDONYM][it->first.asString()] = it->second;
	}

	// Contact Sets
	for (contact_set_map_t::iterator it = mContactSets.begin(); it != mContactSets.end(); ++it)
	{
		std::string name = it->first;
		ContactSet* set = it->second;
		output[name]["color"] = set->mColor.getValue();
		output[name]["notify"] = set->mNotify;
		for (uuid_set_t::iterator friend_it = set->mFriends.begin(); friend_it != set->mFriends.end(); ++friend_it)
		{
			output[name]["friends"][(*friend_it).asString()] = "";
		}
	}

	return output;
}

void LGGContactSets::importFromLLSD(const LLSD& data)
{
	for (LLSD::map_const_iterator data_it = data.beginMap(); data_it != data.endMap(); ++data_it)
	{
		std::string name = data_it->first;
		if (isInternalSetName(name))
		{
			if (name == CS_GLOBAL_SETTINGS)
			{
				LLSD global_setting_data = data_it->second;

				LLColor4 color = LLColor4::grey;
				if (global_setting_data.has("defaultColor"))
				{
					color = global_setting_data["defaultColor"];
				}
				mDefaultColor = color;
			}

			if (name == CS_SET_EXTRA_AVS)
			{
				LLSD extra_avatar_data = data_it->second;

				for (LLSD::map_const_iterator extra_avatar_it = extra_avatar_data.beginMap(); extra_avatar_it != extra_avatar_data.endMap(); ++extra_avatar_it)
				{
					mExtraAvatars.insert(LLUUID(extra_avatar_it->first));
				}
			}

			if (name == CS_SET_PSEUDONYM)
			{
				LLSD pseudonym_data = data_it->second;

				for (LLSD::map_const_iterator pseudonym_data_it = pseudonym_data.beginMap(); pseudonym_data_it != pseudonym_data.endMap(); ++pseudonym_data_it)
				{
					mPseudonyms[LLUUID(pseudonym_data_it->first)] = pseudonym_data_it->second.asString();
				}
			}
		}
		else
		{
			LLSD set_data = data_it->second;

			ContactSet* new_set = new ContactSet();
			new_set->mName = name;

			LLColor4 color = getDefaultColor();
			if (set_data.has("color"))
			{
				color = LLColor4(set_data["color"]);
			}
			new_set->mColor = color;

			bool notify = false;
			if (set_data.has("notify"))
			{
				notify = set_data["notify"].asBoolean();
			}
			new_set->mNotify = notify;

			if (set_data.has("friends"))
			{
				LLSD friend_data = set_data["friends"];
				for (LLSD::map_const_iterator friend_it = friend_data.beginMap(); friend_it != friend_data.endMap(); ++friend_it)
				{
					new_set->mFriends.insert(LLUUID(friend_it->first));
				}
			}

			mContactSets[name] = new_set;
		}
	}
}

LLColor4 LGGContactSets::getSetColor(const std::string& set_name)
{
	ContactSet* set = getContactSet(set_name);
	if (set)
	{
		return set->mColor;
	}

	return getDefaultColor();
};

LLColor4 LGGContactSets::colorize(const LLUUID& uuid, const LLColor4& cur_color, ELGGCSType type)
{
	static LLCachedControl<bool> legacy_radar_friend(gSavedSettings, "FSLegacyRadarFriendColoring");
	static LLCachedControl<bool> legacy_radar_linden(gSavedSettings, "FSLegacyRadarLindenColoring");
	bool rlv_shownames = gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES);
	LLColor4 color = cur_color;
	
	if (uuid == gAgent.getID())
	{
		switch (type)
		{
			case LGG_CS_CHAT:
			case LGG_CS_IM:
				color = LLUIColorTable::instance().getColor("UserChatColor", LLColor4::white).get();
				break;
			case LGG_CS_TAG:
				color = LLUIColorTable::instance().getColor("NameTagSelf", LLColor4::white).get();
				break;
			case LGG_CS_MINIMAP:
				color = LLUIColorTable::instance().getColor("MapAvatarSelfColor", LLColor4::white).get();
				break;
			case LGG_CS_RADAR:
			default:
				LL_DEBUGS("ContactSets") << "Unhandled colorize case!" << LL_ENDL;
				break;
		}
	}
	else if (LLAvatarTracker::instance().getBuddyInfo(uuid) != NULL)
	{
		switch (type)
		{
			case LGG_CS_CHAT:
				if (!rlv_shownames)
					color = LLUIColorTable::instance().getColor("FriendsChatColor", LLColor4::white).get();
				break;
			case LGG_CS_IM:
				color = LLUIColorTable::instance().getColor("FriendsChatColor", LLColor4::white).get();
				break;
			case LGG_CS_TAG:
			{
				// This is optional per prefs.
				static LLCachedControl<bool> color_friends(gSavedSettings, "NameTagShowFriends");
				if (color_friends && !rlv_shownames)
				{
					color = LLUIColorTable::instance().getColor("NameTagFriend", LLColor4::white).get();
				}
			}
				break;
			case LGG_CS_MINIMAP:
				if (!rlv_shownames)
					color = LLUIColorTable::instance().getColor("MapAvatarFriendColor", LLColor4::white).get();
				break;
			case LGG_CS_RADAR:
				if (legacy_radar_friend && !rlv_shownames)
					color = LLUIColorTable::instance().getColor("MapAvatarFriendColor", LLColor4::white).get();
				break;
			default:
				LL_DEBUGS("ContactSets") << "Unhandled colorize case!" << LL_ENDL;
				break;
		}
	}
	else if (rlv_shownames)
	{
		// Don't bother with the rest if we're rlv_shownames restricted.
	}
	else if (LLMuteList::getInstance()->isMuted(uuid))
	{
		switch (type)
		{
			case LGG_CS_CHAT:
			case LGG_CS_IM:
				color = LLUIColorTable::instance().getColor("MutedChatColor", LLColor4::grey3).get();
				break;
			case LGG_CS_TAG:
				color = LLUIColorTable::instance().getColor("NameTagMuted", LLColor4::grey3).get();
				break;
			case LGG_CS_MINIMAP:
				color = LLUIColorTable::instance().getColor("MapAvatarMutedColor", LLColor4::grey3).get();
				break;
			case LGG_CS_RADAR:
			default:
				LL_DEBUGS("ContactSets") << "Unhandled colorize case!" << LL_ENDL;
				break;
		}
	}
	else if (FSData::instance().isAgentFlag(uuid, FSData::CHAT_COLOR))
	{
		switch (type)
		{
			case LGG_CS_CHAT:
			case LGG_CS_IM:
				color = LLUIColorTable::instance().getColor("FirestormChatColor", LLColor4::red).get();
				break;
			case LGG_CS_TAG:
				color = LLUIColorTable::instance().getColor("NameTagFirestorm", LLColor4::red).get();
				break;
			case LGG_CS_MINIMAP:
				color = LLUIColorTable::instance().getColor("MapAvatarFirestormColor", LLColor4::red).get();
				break;
			case LGG_CS_RADAR:
			default:
				LL_DEBUGS("ContactSets") << "Unhandled colorize case!" << LL_ENDL;
				break;
		}
	}
	else
	{
		FSRadarEntry* entry = FSRadar::getInstance()->getEntry(uuid);
		if ( (entry && entry->getIsLinden()) || (!entry && FSCommon::isLinden(uuid)) )
		{
			switch (type)
			{
				case LGG_CS_CHAT:
				case LGG_CS_IM:
					color = LLUIColorTable::instance().getColor("LindenChatColor", LLColor4::blue).get();
					break;
				case LGG_CS_TAG:
					color = LLUIColorTable::instance().getColor("NameTagLinden", LLColor4::blue).get();
					break;
				case LGG_CS_MINIMAP:
					color = LLUIColorTable::instance().getColor("MapAvatarLindenColor", LLColor4::blue).get();
					break;
				case LGG_CS_RADAR:
					if (legacy_radar_linden)
						color = LLUIColorTable::instance().getColor("MapAvatarLindenColor", LLColor4::blue).get();
					break;
				default:
					LL_DEBUGS("ContactSets") << "Unhandled colorize case!" << LL_ENDL;
					break;
			}
		}
	}
	
	if (!rlv_shownames && isNonFriend(uuid))
	{
		toneDownColor(color);
	}
	
	return color;
}

LLColor4 LGGContactSets::getFriendColor(const LLUUID& friend_id, const std::string& ignored_set_name)
{
	LLColor4 color = getDefaultColor();
	if (ignored_set_name == CS_SET_NO_SETS)
	{
		return color;
	}

	U32 lowest = U32_MAX;
	string_vec_t contact_sets = getFriendSets(friend_id);
	for (string_vec_t::iterator it = contact_sets.begin(); it != contact_sets.end(); ++it)
	{
		std::string set_name = *it;
		if (set_name != ignored_set_name)
		{
			U32 set_size = (U32)getFriendsInSet(set_name).size();
			if (!set_size)
			{
				continue;
			}
			if (set_size < lowest)
			{
				lowest = set_size;

				color = mContactSets[set_name]->mColor;
				if (isNonFriend(friend_id))
				{
					toneDownColor(color);
				}
			}
		}
	}

	if (lowest == U32_MAX)
	{
		if (isFriendInSet(friend_id, ignored_set_name) && !isInternalSetName(ignored_set_name))
		{
			return mContactSets[ignored_set_name]->mColor;
		}
	}
	return color;
}

bool LGGContactSets::hasFriendColorThatShouldShow(const LLUUID& friend_id, ELGGCSType type)
{
	LLColor4 color = LLColor4::white;
	return hasFriendColorThatShouldShow(friend_id, type, color);
}

// handle all settings and rlv that would prevent us from showing the cs color
bool LGGContactSets::hasFriendColorThatShouldShow(const LLUUID& friend_id, ELGGCSType type, LLColor4& color)
{
	if (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES))
	{
		return false; // don't show colors if we cant show names
	}

	static LLCachedControl<bool> fsContactSetsColorizeChat(gSavedSettings, "FSContactSetsColorizeChat", false);
	static LLCachedControl<bool> fsContactSetsColorizeTag(gSavedSettings,"FSContactSetsColorizeNameTag", false);
	static LLCachedControl<bool> fsContactSetsColorizeRadar(gSavedSettings,"FSContactSetsColorizeRadar", false);
	static LLCachedControl<bool> fsContactSetsColorizeMiniMap(gSavedSettings,"FSContactSetsColorizeMiniMap", false);

	switch (type)
	{
		case LGG_CS_CHAT:
		case LGG_CS_IM:
			if (!fsContactSetsColorizeChat)
				return false;
			break;
		case LGG_CS_TAG:
			if (!fsContactSetsColorizeTag)
				return false;
			break;
		case LGG_CS_RADAR:
			if (!fsContactSetsColorizeRadar)
				return false;
			break;
		case LGG_CS_MINIMAP:
			if (!fsContactSetsColorizeMiniMap)
				return false;
			break;
	};

	/// don't show friend color if they are no longer a friend
	/// (and if are also not on the "non friends" list)
	if ((!LLAvatarTracker::instance().isBuddy(friend_id)) && (!isNonFriend(friend_id)))
	{
		return false;
	}

	LLColor4 friend_color = getFriendColor(friend_id);
	if (friend_color == getDefaultColor())
	{
		return false;
	}

	color = friend_color;

	return true;
}

string_vec_t LGGContactSets::getFriendSets(const LLUUID& friend_id)
{
	string_vec_t sets;

	contact_set_map_t::iterator set_itr_end = mContactSets.end();
	for (contact_set_map_t::iterator itr = mContactSets.begin(); itr != set_itr_end; ++itr)
	{
		ContactSet* set = itr->second;
		if (set->hasFriend(friend_id))
		{
			sets.push_back(set->mName);
		}
	}
	return sets;
}

uuid_vec_t LGGContactSets::getFriendsInSet(const std::string& set_name)
{
	uuid_vec_t friends;

	if (set_name == CS_SET_ALL_SETS)
	{
		return getFriendsInAnySet();
	}
	else if (set_name == CS_SET_NO_SETS)
	{
		return friends;
	}
	else if (set_name == CS_SET_PSEUDONYM)
	{
		return getListOfPseudonymAvs();
	}
	else if (set_name == CS_SET_EXTRA_AVS)
	{
		return getListOfNonFriends();
	}

	ContactSet* set = getContactSet(set_name);
	if (set)
	{
		for (uuid_set_t::iterator itr = set->mFriends.begin(); itr != set->mFriends.end(); ++itr)
		{
			friends.push_back(*itr);
		}
	}

	return friends;
}

string_vec_t LGGContactSets::getAllContactSets()
{
	string_vec_t sets;

	for (contact_set_map_t::iterator itr = mContactSets.begin(); itr != mContactSets.end(); ++itr)
	{
		sets.push_back(itr->second->mName);
	}

	return sets;
}

uuid_vec_t LGGContactSets::getFriendsInAnySet()
{
	std::set<LLUUID> friendsInAnySet;

	for (contact_set_map_t::iterator set_itr = mContactSets.begin(); set_itr != mContactSets.end(); ++set_itr)
	{
		ContactSet* set = set_itr->second;
		for (uuid_set_t::iterator itr = set->mFriends.begin(); itr != set->mFriends.end(); ++itr)
		{
			friendsInAnySet.insert(*itr);
		}
	}

	return uuid_vec_t(friendsInAnySet.begin(), friendsInAnySet.end());
}

bool LGGContactSets::isFriendInSet(const LLUUID& friend_id)
{
	for (contact_set_map_t::iterator itr = mContactSets.begin(); itr != mContactSets.end(); ++itr)
	{
		ContactSet* set = itr->second;
		if (set->hasFriend(friend_id))
		{
			return true;
		}
	}

	return false;
}

bool LGGContactSets::isFriendInSet(const LLUUID& friend_id, const std::string& set_name)
{
	if (set_name == CS_SET_ALL_SETS)
	{
		return isFriendInSet(friend_id);
	}
	else if (set_name == CS_SET_NO_SETS)
	{
		return !isFriendInSet(friend_id);
	}
	else if (set_name == CS_SET_PSEUDONYM)
	{
		return hasPseudonym(friend_id);
	}
	else if (set_name == CS_SET_EXTRA_AVS)
	{
		return isNonFriend(friend_id);
	}

	if( set_name.empty() )
		return false;

	ContactSet* set = getContactSet(set_name);
	if (set)
	{
		return set->hasFriend(friend_id);
	}

	return false;
}

bool LGGContactSets::notifyForFriend(const LLUUID& friend_id)
{
	string_vec_t sets = getFriendSets(friend_id);
	for (string_vec_t::const_iterator itr = sets.begin(); itr != sets.end(); ++itr)
	{
		if (mContactSets[*itr]->mNotify)
		{
			return true;
		}
	}
	return false;
}

void LGGContactSets::addFriendToSet(const LLUUID& friend_id, const std::string& set_name)
{
	if (friend_id.notNull() && isValidSet(set_name))
	{
		mContactSets[set_name]->mFriends.insert(friend_id);
		saveToDisk();
		mChangedSignal(UPDATED_MEMBERS);
	}
}

void LGGContactSets::addNonFriendToList(const LLUUID& non_friend_id)
{
	mExtraAvatars.insert(non_friend_id);
	saveToDisk();
	mChangedSignal(UPDATED_MEMBERS);
}

void LGGContactSets::removeNonFriendFromList(const LLUUID& non_friend_id)
{
	uuid_set_t::iterator found = mExtraAvatars.find(non_friend_id);
	if (found != mExtraAvatars.end())
	{
		mExtraAvatars.erase(found);

		if (!LLAvatarTracker::instance().isBuddy(non_friend_id))
		{
			clearPseudonym(non_friend_id);
			removeFriendFromAllSets(non_friend_id);
		}

		saveToDisk();
		mChangedSignal(UPDATED_MEMBERS);
	}
}

void LGGContactSets::removeFriendFromAllSets(const LLUUID& friend_id)
{
	string_vec_t sets = getFriendSets(friend_id);
	for (string_vec_t::const_iterator itr = sets.begin(); itr != sets.end(); ++itr)
	{
		removeFriendFromSet(friend_id, (*itr));
	}
}

bool LGGContactSets::isNonFriend(const LLUUID& non_friend_id)
{
	if (LLAvatarTracker::instance().isBuddy(non_friend_id))
	{
		return false;
	}

	return (mExtraAvatars.find(non_friend_id) != mExtraAvatars.end());
}

uuid_vec_t LGGContactSets::getListOfNonFriends()
{
	uuid_vec_t nonfriends;

	for (uuid_set_t::iterator itr = mExtraAvatars.begin(); itr != mExtraAvatars.end(); ++itr)
	{
		LLUUID friend_id = *itr;

		if (!LLAvatarTracker::instance().isBuddy(friend_id))
		{
			nonfriends.push_back(friend_id);
		}
	}

	return nonfriends;
}

uuid_vec_t LGGContactSets::getListOfPseudonymAvs()
{
	uuid_vec_t pseudonyms;

	for (uuid_map_t::iterator itr = mPseudonyms.begin(); itr != mPseudonyms.end(); ++itr)
	{
		pseudonyms.push_back(itr->first);
	}

	return pseudonyms;
}

void LGGContactSets::setPseudonym(const LLUUID& friend_id, const std::string& pseudonym)
{
	mPseudonyms[friend_id] = pseudonym;
	LLAvatarNameCache::erase(friend_id);
	LLAvatarNameCache::fetch(friend_id);
	LLVOAvatar::invalidateNameTag(friend_id);

	avatar_name_cache_connection_map_t::iterator it = mAvatarNameCacheConnections.find(friend_id);
	if (it != mAvatarNameCacheConnections.end())
	{
		if (it->second.connected())
		{
			it->second.disconnect();
		}
		mAvatarNameCacheConnections.erase(it);
	}
	mAvatarNameCacheConnections[friend_id] = LLAvatarNameCache::get(friend_id, boost::bind(&LGGContactSets::onAvatarNameCache, this, _1));
	saveToDisk();
}

std::string LGGContactSets::getPseudonym(const LLUUID& friend_id)
{
	uuid_map_t::iterator found = mPseudonyms.find(friend_id);
	if (found != mPseudonyms.end())
	{
		return llformat("'%s'", found->second.c_str());
	}
	return std::string();
}

void LGGContactSets::clearPseudonym(const LLUUID& friend_id)
{
	uuid_map_t::iterator found = mPseudonyms.find(friend_id);
	if (found != mPseudonyms.end())
	{
		mPseudonyms.erase(found);
		LLAvatarNameCache::erase(friend_id);
		LLAvatarNameCache::fetch(friend_id); // update
		LLVOAvatar::invalidateNameTag(friend_id);
		if (!LLAvatarTracker::instance().isBuddy(friend_id) && LGGContactSets::getInstance()->getFriendSets(friend_id).size() == 0)
		{
			LGGContactSets::getInstance()->removeNonFriendFromList(friend_id);
		}

		avatar_name_cache_connection_map_t::iterator it = mAvatarNameCacheConnections.find(friend_id);
		if (it != mAvatarNameCacheConnections.end())
		{
			if (it->second.connected())
			{
				it->second.disconnect();
			}
			mAvatarNameCacheConnections.erase(it);
		}
		mAvatarNameCacheConnections[friend_id] = LLAvatarNameCache::get(friend_id, boost::bind(&LGGContactSets::onAvatarNameCache, this, _1));
		saveToDisk();
	}
}

void LGGContactSets::onAvatarNameCache(const LLUUID& av_id)
{
	avatar_name_cache_connection_map_t::iterator it = mAvatarNameCacheConnections.find(av_id);
	if (it != mAvatarNameCacheConnections.end())
	{
		if (it->second.connected())
		{
			it->second.disconnect();
		}
		mAvatarNameCacheConnections.erase(it);
	}
	mChangedSignal(UPDATED_MEMBERS);
}

bool LGGContactSets::hasPseudonym(const LLUUID& friend_id)
{
	return (!getPseudonym(friend_id).empty());
}

bool LGGContactSets::hasPseudonym(uuid_vec_t ids)
{
	bool has_pseudonym = false;
	for (uuid_vec_t::const_iterator id = ids.begin(); id != ids.end(); ++id)
	{
		if (LGGContactSets::getInstance()->hasPseudonym(*id))
		{
			has_pseudonym = true;
			break;
		}
	}
	return has_pseudonym;
}

bool LGGContactSets::hasDisplayNameRemoved(const LLUUID& friend_id)
{
	return (getPseudonym(friend_id) == CS_PSEUDONYM_QUOTED);
}

bool LGGContactSets::hasDisplayNameRemoved(uuid_vec_t ids)
{
	bool has_pseudonym = false;
	for (uuid_vec_t::const_iterator id = ids.begin(); id != ids.end(); ++id)
	{
		if (LGGContactSets::getInstance()->hasDisplayNameRemoved(*id))
		{
			has_pseudonym = true;
			break;
		}
	}
	return has_pseudonym;
}

bool LGGContactSets::hasVisuallyDifferentPseudonym(const LLUUID& friend_id)
{
	return (hasPseudonym(friend_id) && (!hasDisplayNameRemoved(friend_id)));
}

void LGGContactSets::removeDisplayName(const LLUUID& friend_id)
{
	setPseudonym(friend_id, CS_PSEUDONYM);
}

void LGGContactSets::removeFriendFromSet(const LLUUID& friend_id, const std::string& set_name)
{
	if (set_name == CS_SET_EXTRA_AVS)
	{
		return removeNonFriendFromList(friend_id);
	}
	else if (set_name == CS_SET_PSEUDONYM)
	{
		return clearPseudonym(friend_id);
	}

	ContactSet* set = getContactSet(set_name);
	if (set)
	{
		set->mFriends.erase(friend_id);
		saveToDisk();
		mChangedSignal(UPDATED_MEMBERS);
	}
}

bool LGGContactSets::isValidSet(const std::string& set_name)
{
	return (mContactSets.find(set_name) != mContactSets.end());
}

void LGGContactSets::addSet(const std::string& set_name)
{
	if (!isInternalSetName(set_name) && !isValidSet(set_name))
	{
		ContactSet* set = new ContactSet();
		set->mName = set_name;
		set->mColor = LLColor4::red;
		set->mNotify = false;
		mContactSets[set_name] = set;
		saveToDisk();
		mChangedSignal(UPDATED_LISTS);
	}
}

bool LGGContactSets::renameSet(const std::string& set_name, const std::string& new_set_name)
{
	if (!isInternalSetName(set_name) && isValidSet(set_name) &&
		!isInternalSetName(new_set_name) && !isValidSet(new_set_name))
	{
		ContactSet* set = getContactSet(set_name);
		set->mName = new_set_name;
		mContactSets.erase(set_name);
		mContactSets[new_set_name] = set;
		saveToDisk();
		mChangedSignal(UPDATED_LISTS);
		return true;
	}
	return false;
}

void LGGContactSets::removeSet(const std::string& set_name)
{
	contact_set_map_t::iterator found = mContactSets.find(set_name);
	if (found != mContactSets.end())
	{
		uuid_vec_t non_friends_for_removal;
		ContactSet* cset = found->second;
		for (uuid_set_t::iterator member_it = cset->mFriends.begin(); member_it != cset->mFriends.end(); ++member_it)
		{
			if (!LLAvatarTracker::instance().isBuddy(*member_it) &&
				LGGContactSets::getInstance()->getFriendSets(*member_it).size() == 1 && // Current set is only set!
				!LGGContactSets::getInstance()->hasPseudonym(*member_it))
			{
				non_friends_for_removal.push_back(*member_it);
			}
		}

		for (uuid_vec_t::iterator nf_it = non_friends_for_removal.begin(); nf_it != non_friends_for_removal.end(); ++nf_it)
		{
			LGGContactSets::getInstance()->removeNonFriendFromList(*nf_it);
		}

		delete found->second;
		mContactSets.erase(found);
		saveToDisk();
		mChangedSignal(UPDATED_LISTS);
	}
}

void LGGContactSets::setNotifyForSet(const std::string& set_name, bool notify)
{
	ContactSet* set = getContactSet(set_name);
	if (set)
	{
		set->mNotify = notify;
		saveToDisk();
	}
}

bool LGGContactSets::getNotifyForSet(const std::string& set_name)
{
	ContactSet* set = getContactSet(set_name);
	if (set)
	{
		return set->mNotify;
	}
	return false;
}

void LGGContactSets::setSetColor(const std::string& set_name, const LLColor4& color)
{
	ContactSet* set = getContactSet(set_name);
	if (set)
	{
		set->mColor = color;
		saveToDisk();
	}

}

bool LGGContactSets::isInternalSetName(const std::string& set_name)
{
	return (set_name.empty() ||
			set_name == CS_SET_EXTRA_AVS ||
			set_name == CS_SET_PSEUDONYM ||
			set_name == CS_SET_NO_SETS ||
			set_name == CS_SET_ALL_SETS ||
			set_name == CS_GLOBAL_SETTINGS);
}

LGGContactSets::ContactSet* LGGContactSets::getContactSet(const std::string& set_name)
{
	if (set_name.empty())
	{
		LL_WARNS("ContactSets") << "No contact set specified" << LL_ENDL;
		return NULL;
	}

	contact_set_map_t::iterator found = mContactSets.find(set_name);
	if (found != mContactSets.end())
	{
		return found->second;
	}
	LL_WARNS("ContactSets") << "No contact set named " << set_name << LL_ENDL;
	return NULL;
}

// static
bool LGGContactSets::handleAddContactSetCallback(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	if (option == 0)
	{
		const std::string set_name = response["message"].asString();
		LGGContactSets::getInstance()->addSet(set_name);
	}
	return false;
}

// static
bool LGGContactSets::handleRemoveContactSetCallback(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	if (option == 0)
	{
		LGGContactSets::getInstance()->removeSet(notification["payload"]["contact_set"].asString());
	}
	return false;
}

// static
bool LGGContactSets::handleRemoveAvatarFromSetCallback(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	if (option == 0)
	{
		for(LLSD::array_const_iterator it = notification["payload"]["ids"].beginArray();
			it != notification["payload"]["ids"].endArray();
			++it)
		{
			LLUUID id = it->asUUID();
			std::string set = notification["payload"]["contact_set"].asString();
			LGGContactSets::getInstance()->removeFriendFromSet(id, set);
			if (!LLAvatarTracker::instance().isBuddy(id) &&
				LGGContactSets::getInstance()->getFriendSets(id).size() == 0 &&
				!LGGContactSets::getInstance()->hasPseudonym(id))
			{
				LGGContactSets::getInstance()->removeNonFriendFromList(id);
			}
		}
	}
	return false;
}

// static
bool LGGContactSets::handleSetAvatarPseudonymCallback(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	if (option == 0)
	{
		const std::string pseudonym(response["message"].asString());
		const LLUUID id(notification["payload"]["id"].asUUID());
		LGGContactSets::getInstance()->setPseudonym(id, pseudonym);
	}
	return false;
}
