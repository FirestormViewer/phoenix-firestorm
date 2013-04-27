/* @file lggcontactsets.cpp
   Copyright (C) 2011 Greg Hendrickson (LordGregGreg Back)
   
   This is free software; you can redistribute it and/or modify it
   under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation; either version 2.1 of
   the License, or (at your option) any later version.
 
   This is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
   License for more details.
 
   You should have received a copy of the GNU Lesser General Public
   License along with the viewer; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.  */

#include "llviewerprecompiledheaders.h"

#include "lggcontactsets.h"
#include "llsdserialize.h"
#include "llviewercontrol.h"
#include "llnotifications.h"
#include "lldir.h"
#include "llcallingcard.h"
#include "llavatarnamecache.h"
#include "rlvhandler.h"

LGGContactSets::LGGContactSets() :
	mDefaultColor(LLColor4::grey)
{
	loadFromDisk();
}

LGGContactSets::~LGGContactSets()
{
	for (group_map_t::iterator it = mGroups.begin(); it != mGroups.end(); ++it)
	{
		delete it->second;
	}
	mGroups.clear();
}

LLColor4 LGGContactSets::toneDownColor(const LLColor4& inColor, float strength, bool usedForBackground)
{
	if (usedForBackground)
	{
		if (strength < .4f)
		{
			strength = .4f;
		}
		static LLCachedControl<S32> maxAlphaInt(gSavedSettings,"FSContactSetsMaxColorStrength");
		strength *= ((F32)maxAlphaInt / 100.0f);
	}
	return LLColor4(LLColor3(inColor), strength);
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

std::string LGGContactSets::getFileName()
{
	std::string path = gDirUtilp->getExpandedFilename(LL_PATH_PER_SL_ACCOUNT, "");

	if (!path.empty())
	{
		path = gDirUtilp->getExpandedFilename(LL_PATH_PER_SL_ACCOUNT, "settings_friends_groups.xml");
	}
	return path;
}

std::string LGGContactSets::getOldFileName()
{
	std::string path = gDirUtilp->getOSUserDir() + gDirUtilp->getDirDelimiter() + "SecondLife" + gDirUtilp->getDirDelimiter();

	std::string normalPath=gDirUtilp->getExpandedFilename(LL_PATH_PER_SL_ACCOUNT, "");
	//we want to steal the last directory off this one
	std::string userNameDir = normalPath.substr(normalPath.find_last_of(gDirUtilp->getDirDelimiter()));
	path += userNameDir;

	if (!path.empty())
	{
		path=path+gDirUtilp->getDirDelimiter() + "settings_friends_groups.xml";
	}
	llinfos << "returning a old path name of  "<< path.c_str() << llendl;
	return path;
}

std::string LGGContactSets::getDefaultFileName()
{
	std::string path=gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, "");

	if (!path.empty())
	{
		path = gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, "settings_friends_groups.xml");
	}
	return path;
}

LLSD LGGContactSets::exportGroup(const std::string& groupName)
{
	LLSD toReturn;

	ContactSetGroup* group = getGroup(groupName);
	if (group)
	{
		toReturn["groupname"] = group->mName;
		toReturn["color"] = group->mColor.getValue();
		toReturn["notices"] = group->mNotify;
		for (uuid_set_t::iterator friend_it = group->mFriends.begin(); friend_it != group->mFriends.end(); ++friend_it)
		{
			toReturn["friends"][(*friend_it).asString()] = "";
		}
	}

	return toReturn;
}

void LGGContactSets::loadFromDisk()
{
	std::string filename = getFileName();
	if (filename.empty())
	{
		llinfos << "No valid user directory." << llendl;
	}

	if (!gDirUtilp->fileExists(filename))
	{
		//try to find the phoenix file to load
		std::string phoenixFileName = getOldFileName();
		if (gDirUtilp->fileExists(phoenixFileName))
		{
			LLSD blankllsd;
			llifstream file;
			file.open(phoenixFileName.c_str());
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
			std::string defaultName = getDefaultFileName();
			llinfos << "User settings file doesnt exist, going to try and read default one from " << defaultName.c_str() << llendl;

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
}

void LGGContactSets::saveToDisk()
{
	std::string filename = getFileName();
	llofstream file;
	file.open(filename);
	LLSDSerialize::toPrettyXML(exportToLLSD(), file);
	file.close();
}

BOOL LGGContactSets::saveGroupToDisk(const std::string& groupName, const std::string& fileName)
{
	if (isAGroup(groupName))
	{
		llofstream file;
		file.open(fileName.c_str());
		LLSDSerialize::toPrettyXML(exportGroup(groupName), file);
		file.close();
		return TRUE;
	}

	return FALSE;
}


LLSD LGGContactSets::exportToLLSD()
{
	LLSD output;

	// Global settings
	output[CS_GLOBAL_SETTINGS]["defaultColor"] = mDefaultColor.getValue();

	// Extra avatars
	for (uuid_set_t::iterator it = mExtraAvatars.begin(); it != mExtraAvatars.end(); ++it)
	{
		output[CS_GROUP_EXTRA_AVS][(*it).asString()] = "";
	}

	// Pseudonyms
	for (uuid_map_t::iterator it = mPseudonyms.begin(); it != mPseudonyms.end(); ++it)
	{
		output[CS_GROUP_PSEUDONYM][it->first.asString()] = it->second;
	}

	// Groups
	for (group_map_t::iterator it = mGroups.begin(); it != mGroups.end(); ++it)
	{
		std::string name = it->first;
		ContactSetGroup* group = it->second;
		output[name]["color"] = group->mColor.getValue();
		output[name]["notify"] = group->mNotify;
		for (uuid_set_t::iterator friend_it = group->mFriends.begin(); friend_it != group->mFriends.end(); ++friend_it)
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
		if (isInternalGroupName(name))
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

			if (name == CS_GROUP_EXTRA_AVS)
			{
				LLSD extra_avatar_data = data_it->second;

				for (LLSD::map_const_iterator extra_avatar_it = extra_avatar_data.beginMap(); extra_avatar_it != extra_avatar_data.endMap(); ++extra_avatar_it)
				{
					mExtraAvatars.insert(LLUUID(extra_avatar_it->first));
				}
			}

			if (name == CS_GROUP_PSEUDONYM)
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
			LLSD group_data = data_it->second;

			ContactSetGroup* new_group = new ContactSetGroup();
			new_group->mName = name;

			LLColor4 color = getDefaultColor();
			if (group_data.has("color"))
			{
				color = LLColor4(group_data["color"]);
			}
			new_group->mColor = color;

			bool notify = false;
			if (group_data.has("notify"))
			{
				notify = group_data["notify"].asBoolean();
			}
			new_group->mNotify = notify;

			if (group_data.has("friends"))
			{
				LLSD friend_data = group_data["friends"];
				for (LLSD::map_const_iterator friend_it = friend_data.beginMap(); friend_it != friend_data.endMap(); ++friend_it)
				{
					new_group->mFriends.insert(LLUUID(friend_it->first));
				}
			}

			mGroups[name] = new_group;
		}
	}
}

LLColor4 LGGContactSets::getGroupColor(const std::string& groupName)
{
	ContactSetGroup* group = getGroup(groupName);
	if (group)
	{
		return group->mColor;
	}

	return getDefaultColor();
};

LLColor4 LGGContactSets::getFriendColor(const LLUUID& friend_id, const std::string& ignoredGroupName)
{
	LLColor4 toReturn = getDefaultColor();
	if (ignoredGroupName == CS_GROUP_NO_SETS)
	{
		return toReturn;
	}

	U32 lowest = 9999;
	std::vector<std::string> groups = getFriendGroups(friend_id);
	for (U32 i = 0; i < (U32)groups.size(); i++)
	{
		if (groups[i] != ignoredGroupName)
		{
			U32 membersNum = getFriendsInGroup(groups[i]).size();
			if (membersNum == 0)
			{
				continue;
			}
			if (membersNum < lowest)
			{
				lowest = membersNum;

				toReturn = mGroups[groups[i]]->mColor;
				if (isNonFriend(friend_id))
				{
					toReturn = toneDownColor(toReturn,.8f);
				}
			}
		}
	}

	if (lowest == 9999)
	{
		if (isFriendInGroup(friend_id, ignoredGroupName) && !isInternalGroupName(ignoredGroupName))
		{
			return mGroups[ignoredGroupName]->mColor;
		}
	}
	return toReturn;
}

// handle all settings and rlv that would prevent us from showing the cs color
BOOL LGGContactSets::hasFriendColorThatShouldShow(const LLUUID& friend_id, ELGGCSType type)
{
	if (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES))
	{
		return FALSE; // don't show colors if we cant show names
	}
	static LLCachedControl<bool> fsContactSetsColorizeChat(gSavedSettings, "FSContactSetsColorizeChat", false);
	static LLCachedControl<bool> fsContactSetsColorizeTag(gSavedSettings,"FSContactSetsColorizeNameTag", false);
	static LLCachedControl<bool> fsContactSetsColorizeRadar(gSavedSettings,"FSContactSetsColorizeRadar", false);
	static LLCachedControl<bool> fsContactSetsColorizeMiniMap(gSavedSettings,"FSContactSetsColorizeMiniMap", false);

	switch (type)
	{
		case LGG_CS_CHAT:
			if (!fsContactSetsColorizeChat)
				return FALSE;
			break;
		case LGG_CS_TAG:
			if (!fsContactSetsColorizeTag)
				return FALSE;
			break;
		case LGG_CS_RADAR:
			if (!fsContactSetsColorizeRadar)
				return FALSE;
			break;
		case LGG_CS_MINIMAP:
			if (!fsContactSetsColorizeMiniMap)
				return FALSE;
			break;
	};

	/// don't show friend color if they are no longer a friend
	/// (and if are also not on the "non friends" list)
	if ((!LLAvatarTracker::instance().isBuddy(friend_id)) && (!isNonFriend(friend_id)))
	{
		return FALSE;
	}

	if (getFriendColor(friend_id) == getDefaultColor())
	{
		return FALSE;
	}
	return TRUE;
}

LLColor4 LGGContactSets::getDefaultColor()
{
	return mDefaultColor;
}

void LGGContactSets::setDefaultColor(const LLColor4& dColor)
{
	mDefaultColor = dColor;
}

std::vector<std::string> LGGContactSets::getInnerGroups(const std::string& groupName)
{
	std::vector<std::string> toReturn;
	static LLCachedControl<bool> useFolders(gSavedSettings, "FSContactSetsShowFolders");
	static LLCachedControl<bool> showOnline(gSavedSettings, "FSContactSetsShowOnline");
	static LLCachedControl<bool> showOffline(gSavedSettings, "FSContactSetsShowOffline");

	if (!useFolders)
	{
		return toReturn;
	}

	std::set<std::string> newGroups;

	std::vector<LLUUID> freindsInGroup = getFriendsInGroup(groupName);
	for (U32 fn = 0; fn < (U32)freindsInGroup.size(); fn++)
	{
		LLUUID friend_id = freindsInGroup[fn];
		BOOL online = LLAvatarTracker::instance().isBuddyOnline(friend_id);
		if (online && !showOnline)
		{
			continue;
		}
		if (!online && !showOffline)
		{
			continue;
		}

		std::vector<std::string> innerGroups = getFriendGroups(friend_id);
		for (U32 inIter = 0; inIter < (U32)innerGroups.size(); inIter++)
		{
			std::string innerGroupName = innerGroups[inIter];
			if (groupName != innerGroupName)
			{
				newGroups.insert(innerGroupName);
			}
		}
	}

	std::copy(newGroups.begin(), newGroups.end(), std::back_inserter(toReturn));
	return toReturn;
}

std::vector<std::string> LGGContactSets::getFriendGroups(const LLUUID& friend_id)
{
	std::vector<std::string> toReturn;

	group_map_t::iterator group_it_end = mGroups.end();
	for (group_map_t::iterator it = mGroups.begin(); it != group_it_end; ++it)
	{
		ContactSetGroup* group = it->second;
		if (group->hasFriend(friend_id))
		{
			toReturn.push_back(group->mName);
		}
	}
	return toReturn;
}

std::vector<LLUUID> LGGContactSets::getFriendsInGroup(const std::string& groupName)
{
	std::vector<LLUUID> toReturn;

	if (groupName == CS_GROUP_ALL_SETS)
	{
		return getFriendsInAnyGroup();
	}

	if (groupName == CS_GROUP_NO_SETS)
	{
		return toReturn;
	}

	if (groupName == CS_GROUP_PSEUDONYM)
	{
		return getListOfPseudonymAvs();
	}

	if (groupName == CS_GROUP_EXTRA_AVS)
	{
		return getListOfNonFriends();
	}

	ContactSetGroup* group = mGroups[groupName];
	for (uuid_set_t::iterator it = group->mFriends.begin(); it != group->mFriends.end(); ++it)
	{
		toReturn.push_back(*it);
	}

	return toReturn;
}

std::vector<std::string> LGGContactSets::getAllGroups()
{
	std::vector<std::string> toReturn;

	for (group_map_t::iterator it = mGroups.begin(); it != mGroups.end(); ++it)
	{
		toReturn.push_back(it->second->mName);
	}

	return toReturn;
}

std::vector<LLUUID> LGGContactSets::getFriendsInAnyGroup()
{
	std::set<LLUUID> friendsInAnyGroup;

	for (group_map_t::iterator group_it = mGroups.begin(); group_it != mGroups.end(); ++group_it)
	{
		ContactSetGroup* group = group_it->second;
		for (uuid_set_t::iterator it = group->mFriends.begin(); it != group->mFriends.end(); ++it)
		{
			friendsInAnyGroup.insert(*it);
		}
	}

	return std::vector<LLUUID>(friendsInAnyGroup.begin(), friendsInAnyGroup.end());
}

BOOL LGGContactSets::isFriendInAnyGroup(const LLUUID& friend_id)
{
	for (group_map_t::iterator it = mGroups.begin(); it != mGroups.end(); ++it)
	{
		ContactSetGroup* group = it->second;
		if (group->hasFriend(friend_id))
		{
			return TRUE;
		}
	}

	return FALSE;
}

BOOL LGGContactSets::isFriendInGroup(const LLUUID& friend_id, const std::string& groupName)
{
	if (groupName == CS_GROUP_ALL_SETS)
	{
		return isFriendInAnyGroup(friend_id);
	}

	if (groupName == CS_GROUP_NO_SETS)
	{
		return !isFriendInAnyGroup(friend_id);
	}

	if (groupName == CS_GROUP_PSEUDONYM)
	{
		return hasPseudonym(friend_id);
	}

	if (groupName == CS_GROUP_EXTRA_AVS)
	{
		return isNonFriend(friend_id);
	}

	ContactSetGroup* group = getGroup(groupName);
	if (group)
	{
		return group->hasFriend(friend_id);
	}

	return FALSE;
}

BOOL LGGContactSets::notifyForFriend(const LLUUID& friend_id)
{
	BOOL notify = FALSE;

	std::vector<std::string> groups = getFriendGroups(friend_id);
	for (U32 i = 0; i < (U32)groups.size(); i++)
	{
		if (mGroups[groups[i]]->mNotify)
		{
			return TRUE;
		}
	}
	return notify;
}

void LGGContactSets::addFriendToGroup(const LLUUID& friend_id, const std::string& groupName)
{
	if (friend_id.notNull() && isAGroup(groupName))
	{
		mGroups[groupName]->mFriends.insert(friend_id);
		saveToDisk();
	}
}

void LGGContactSets::addNonFriendToList(const LLUUID& non_friend_id)
{
	mExtraAvatars.insert(non_friend_id);
	saveToDisk();
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
			removeFriendFromAllGroups(non_friend_id);
		}

		saveToDisk();
	}
}

void LGGContactSets::removeFriendFromAllGroups(const LLUUID& friend_id)
{
	std::vector<std::string> groups = getFriendGroups(friend_id);
	for (U32 i = 0; i < (U32)groups.size(); i++)
	{
		removeFriendFromGroup(friend_id, groups[i]);
	}
}

BOOL LGGContactSets::isNonFriend(const LLUUID& non_friend_id)
{
	if (LLAvatarTracker::instance().isBuddy(non_friend_id))
	{
		return FALSE;
	}

	return (mExtraAvatars.find(non_friend_id) != mExtraAvatars.end());
}

std::vector<LLUUID> LGGContactSets::getListOfNonFriends()
{
	std::vector<LLUUID> toReturn;

	for (uuid_set_t::iterator it = mExtraAvatars.begin(); it != mExtraAvatars.end(); ++it)
	{
		LLUUID friend_id = *it;

		if (!LLAvatarTracker::instance().isBuddy(friend_id))
		{
			toReturn.push_back(friend_id);
		}
	}

	return toReturn;
}

std::vector<LLUUID> LGGContactSets::getListOfPseudonymAvs()
{
	std::vector<LLUUID> toReturn;

	for (uuid_map_t::iterator it = mPseudonyms.begin(); it != mPseudonyms.end(); ++it)
	{
		toReturn.push_back(it->first);
	}

	return toReturn;
}

void LGGContactSets::setPseudonym(const LLUUID& friend_id, const std::string& pseudonym)
{
	mPseudonyms[friend_id] = pseudonym;
	saveToDisk();
}

std::string LGGContactSets::getPseudonym(const LLUUID& friend_id)
{
	uuid_map_t::iterator found = mPseudonyms.find(friend_id);
	if (found != mPseudonyms.end())
	{
		return found->second;
	}
	return "";
}

void LGGContactSets::clearPseudonym(const LLUUID& friend_id)
{
	uuid_map_t::iterator found = mPseudonyms.find(friend_id);
	if (found != mPseudonyms.end())
	{
		mPseudonyms.erase(found);
		LLAvatarNameCache::fetch(friend_id); // update
		saveToDisk();
	}
}

BOOL LGGContactSets::hasPseudonym(const LLUUID& friend_id)
{
	return (!getPseudonym(friend_id).empty());
}

BOOL LGGContactSets::hasDisplayNameRemoved(const LLUUID& friend_id)
{
	return (getPseudonym(friend_id) == CS_PSEUDONYM);
}

BOOL LGGContactSets::hasVisuallyDifferentPseudonym(const LLUUID& friend_id)
{
	return (hasPseudonym(friend_id) && (!hasDisplayNameRemoved(friend_id)));
}

void LGGContactSets::removeDisplayName(const LLUUID& friend_id)
{
	setPseudonym(friend_id, CS_PSEUDONYM);
}

void LGGContactSets::removeFriendFromGroup(const LLUUID& friend_id, const std::string& groupName)
{
	if (groupName == CS_GROUP_EXTRA_AVS)
	{
		return removeNonFriendFromList(friend_id);
	}

	if (groupName == CS_GROUP_PSEUDONYM)
	{
		return clearPseudonym(friend_id);
	}

	ContactSetGroup* group = getGroup(groupName);
	if (group)
	{
		group->mFriends.erase(friend_id);
		saveToDisk();
	}
}

bool LGGContactSets::isAGroup(const std::string& groupName)
{
	return (mGroups.find(groupName) != mGroups.end());
}

void LGGContactSets::addGroup(const std::string& groupName)
{
	if (!isInternalGroupName(groupName) && !isAGroup(groupName))
	{
		ContactSetGroup* group = new ContactSetGroup();
		group->mName = groupName;
		group->mColor = LLColor4::red;
		group->mNotify = false;
		mGroups[groupName] = group;
		saveToDisk();
	}
}

void LGGContactSets::deleteGroup(const std::string& groupName)
{
	group_map_t::iterator found = mGroups.find(groupName);
	if (found != mGroups.end())
	{
		delete found->second;
		mGroups.erase(found);
		saveToDisk();
	}
}

void LGGContactSets::setNotifyForGroup(const std::string& groupName, BOOL notify)
{
	ContactSetGroup* group = getGroup(groupName);
	if (group)
	{
		group->mNotify = notify;
		saveToDisk();
	}
}

BOOL LGGContactSets::getNotifyForGroup(const std::string& groupName)
{
	ContactSetGroup* group = getGroup(groupName);
	if (group)
	{
		return group->mNotify;
	}
	return FALSE;
}

void LGGContactSets::setGroupColor(const std::string& groupName, const LLColor4& color)
{
	ContactSetGroup* group = getGroup(groupName);
	if (group)
	{
		group->mColor = color;
		saveToDisk();
	}

}

bool LGGContactSets::isInternalGroupName(const std::string& groupName)
{
	return (
		groupName.empty() ||
		groupName == CS_GROUP_EXTRA_AVS ||
		groupName == CS_GROUP_PSEUDONYM ||
		groupName == CS_GLOBAL_SETTINGS ||
		groupName == CS_GROUP_NO_SETS ||
		groupName == CS_GROUP_ALL_SETS
		);
}

LGGContactSets::ContactSetGroup* LGGContactSets::getGroup(const std::string& groupName)
{
	if (groupName.empty())
	{
		return NULL;
	}

	group_map_t::iterator found = mGroups.find(groupName);
	if (found != mGroups.end())
	{
		return found->second;
	}
	return NULL;
}