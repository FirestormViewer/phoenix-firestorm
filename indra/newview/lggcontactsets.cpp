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

LGGContactSets::LGGContactSets()
{
	loadFromDisk();
}

LGGContactSets::~LGGContactSets()
{
}

LLColor4 LGGContactSets::toneDownColor(LLColor4 inColor, float strength, bool usedForBackground)
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

LLSD LGGContactSets::exportGroup(std::string groupName)
{
	LLSD toReturn;
	if(mContactSets.has(groupName))
	{
		toReturn["groupname"]=groupName;
		toReturn["color"]=mContactSets[groupName]["color"];
		toReturn["notices"]=mContactSets[groupName]["notices"];
		toReturn["friends"]=mContactSets[groupName]["friends"];
	}
	return toReturn;
}

LLSD LGGContactSets::getContactSets()
{
	//loadFromDisk();
	return mContactSets;
}

void LGGContactSets::loadFromDisk()
{
	std::string filename=getFileName();
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
			saveToDisk(blankllsd);
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
				saveToDisk(blankllsd);
			}
			else
			{
				saveToDisk(mContactSets);
			}
		}
	}
	else
	{
		llifstream file;
		file.open(filename.c_str());
		if (file.is_open())
		{
			LLSDSerialize::fromXML(mContactSets, file);
		}
		file.close();
	}
}

void LGGContactSets::saveToDisk(LLSD newSettings)
{
	mContactSets=newSettings;
	std::string filename=getFileName();
	llofstream file;
	file.open(filename.c_str());
	LLSDSerialize::toPrettyXML(mContactSets, file);
	file.close();
}

BOOL LGGContactSets::saveGroupToDisk(std::string groupName, std::string fileName)
{
	if(mContactSets.has(groupName))
	{
		llofstream file;
		file.open(fileName.c_str());
		LLSDSerialize::toPrettyXML(exportGroup(groupName), file);
		file.close();
		return TRUE;
	}
	return FALSE;
}

LLColor4 LGGContactSets::getGroupColor(std::string groupName)
{
	if(groupName != "" && groupName != "All Sets" && groupName != "All Groups" && groupName != "globalSettings" && groupName != "No Sets" && groupName != "ReNamed" && groupName != "Non Friends")
	{
		if (mContactSets[groupName].has("color"))
		{
			return LLColor4(mContactSets[groupName]["color"]);
		}
	}
	return getDefaultColor();
};

LLColor4 LGGContactSets::getFriendColor(LLUUID friend_id, std::string ignoredGroupName)
{
	LLColor4 toReturn = getDefaultColor();
	if(ignoredGroupName == "No Sets")
	{
		return toReturn;
	}
	U32 lowest = 9999;
	std::vector<std::string> groups = getFriendGroups(friend_id);
	for (U32 i = 0; i < (U32)groups.size(); i++)
	{
		if(groups[i] != ignoredGroupName)
		{
			U32 membersNum = getFriendsInGroup(groups[i]).size();
			if (membersNum == 0)
			{
				continue;
			}
			if (membersNum < lowest)
			{
				lowest = membersNum;
				if (mContactSets[groups[i]].has("color"))
				{
					toReturn= LLColor4(mContactSets[groups[i]]["color"]);
					if (isNonFriend(friend_id))
					{
						toReturn=toneDownColor(toReturn,.8f);
					}
				}
			}
		}
	}
	if (lowest == 9999)
	{
		if (isFriendInGroup(friend_id,ignoredGroupName) && ignoredGroupName != "globalSettings" && ignoredGroupName != "Non Friends" && ignoredGroupName != "All Sets" && ignoredGroupName != "All Groups" && ignoredGroupName != "No Sets" &&ignoredGroupName != "ReNamed" && ignoredGroupName != "")
		{
			if(mContactSets[ignoredGroupName].has("color"))
			{
				return LLColor4(mContactSets[ignoredGroupName]["color"]);
			}
		}
	}
	return toReturn;
}

// handle all settings and rlv that would prevent us from showing the cs color
BOOL LGGContactSets::hasFriendColorThatShouldShow(LLUUID friend_id, ELGGCSType type)
{
	if(gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES))
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
			if (fsContactSetsColorizeChat)
				break;
		case LGG_CS_TAG:
			if (fsContactSetsColorizeTag)
				break;
		case LGG_CS_RADAR:
			if (fsContactSetsColorizeRadar)
				break;
		case LGG_CS_MINIMAP:
			if (fsContactSetsColorizeMiniMap)
				break;
		default:
			return FALSE;
			break;
	};

	/// don't show friend color if they are no longer a friend
	/// (and if are also not on the "non friends" list)
	if ((!LLAvatarTracker::instance().isBuddy(friend_id)) && (!isNonFriend(friend_id)))
	{
		return FALSE;
	}

	if(getFriendColor(friend_id) == getDefaultColor())
	{
		return FALSE;
	}
	return TRUE;
}

LLColor4 LGGContactSets::getDefaultColor()
{
	LLColor4 toReturn = LLColor4::grey;
	if(mContactSets.has("globalSettings"))
	{
		if(mContactSets["globalSettings"].has("defaultColor"))
		{
			toReturn = LLColor4(mContactSets["globalSettings"]["defaultColor"]);
		}
	}
	return toReturn;
}

void LGGContactSets::setDefaultColor(LLColor4 dColor)
{
	mContactSets["globalSettings"]["defaultColor"]=dColor.getValue();
}

std::vector<std::string> LGGContactSets::getInnerGroups(std::string groupName)
{
	std::vector<std::string> toReturn;
	toReturn.clear();
	static LLCachedControl<bool> useFolders(gSavedSettings, "FSContactSetsShowFolders");
	static LLCachedControl<bool> showOnline(gSavedSettings, "FSContactSetsShowOnline");
	static LLCachedControl<bool> showOffline(gSavedSettings, "FSContactSetsShowOffline");

	if(!(useFolders))return toReturn;

	std::set<std::string> newGroups;
	newGroups.clear();
	if(groupName!="All Sets" && getAllGroups(FALSE).size()>0)newGroups.insert("All Sets");
	std::vector<LLUUID> freindsInGroup = getFriendsInGroup(groupName);
	for(U32 fn = 0; fn < (U32)freindsInGroup.size(); fn++)
	{
		LLUUID friend_id = freindsInGroup[fn];
		BOOL online = LLAvatarTracker::instance().isBuddyOnline(friend_id);
		if(online && !(showOnline))continue;
		if(!online && !(showOffline))continue;

		std::vector<std::string> innerGroups = getFriendGroups(friend_id);
		for(U32 inIter=0; inIter < (U32)innerGroups.size(); inIter++)
		{
			std::string innerGroupName = innerGroups[inIter];
			if(groupName!=innerGroupName)
				newGroups.insert(innerGroupName);
		}
	}

	std::copy(newGroups.begin(), newGroups.end(), std::back_inserter(toReturn));
	return toReturn;
}
std::vector<std::string> LGGContactSets::getFriendGroups(LLUUID friend_id)
{
	std::vector<std::string> toReturn;
	toReturn.clear();

	LLSD::map_const_iterator loc_it = mContactSets.beginMap();
	LLSD::map_const_iterator loc_end = mContactSets.endMap();
	for ( ; loc_it != loc_end; ++loc_it)
	{
		const std::string& groupName = (*loc_it).first;
		if(groupName!="" && groupName !="globalSettings" && groupName!="All Sets" && groupName!="All Groups" && groupName!="All Groups"  && groupName!="No Sets" && groupName!="ReNamed" && groupName!="Non Friends" && groupName!="extraAvs" && groupName!="pseudonym")
			if(mContactSets[groupName].has("friends"))
				if(mContactSets[groupName]["friends"].has(friend_id.asString()))
					toReturn.push_back(groupName);
	}
	return toReturn;
}
std::vector<LLUUID> LGGContactSets::getFriendsInGroup(std::string groupName)
{
	std::vector<LLUUID> toReturn;
	toReturn.clear();
	if(groupName == "All Sets")
		return getFriendsInAnyGroup();
	if(groupName == "No Sets")
		return toReturn;
	if(groupName == "pseudonym" || groupName=="ReNamed")
		return getListOfPseudonymAvs();
	if(groupName == "Non Friends")
		return getListOfNonFriends();

	LLSD friends = mContactSets[groupName]["friends"];
	LLSD::map_const_iterator loc_it = friends.beginMap();
	LLSD::map_const_iterator loc_end = friends.endMap();
	for ( ; loc_it != loc_end; ++loc_it)
	{
		const LLSD& friendID = (*loc_it).first;

		toReturn.push_back(friendID.asUUID());
	}

	return toReturn;
}
std::vector<std::string> LGGContactSets::getAllGroups(BOOL extraGroups)
{
	std::vector<std::string> toReturn;
	toReturn.clear();
	if(extraGroups)
	{
		if(getAllGroups(FALSE).size()>0)
		{
			toReturn.push_back("All Sets");
			toReturn.push_back("No Sets");
		}
		if(getListOfPseudonymAvs().size()>0)
			toReturn.push_back("ReNamed");
		if(getListOfNonFriends().size()>0)
			toReturn.push_back("Non Friends");
	}

	LLSD::map_const_iterator loc_it = mContactSets.beginMap();
	LLSD::map_const_iterator loc_end = mContactSets.endMap();
	for ( ; loc_it != loc_end; ++loc_it)
	{
		const std::string& groupName = (*loc_it).first;
		if((groupName!="globalSettings")&&(groupName!="ReNamed")&&(groupName!="All Groups")&&(groupName!="No Groups")&&(groupName!="Non Friends")&&(groupName!="")&&(groupName!="extraAvs")&&(groupName!="pseudonym")&&(groupName!="All Sets")&&groupName!="No Sets")
			toReturn.push_back(groupName);
	}
	return toReturn;
}

std::vector<LLUUID> LGGContactSets::getFriendsInAnyGroup()
{
	std::set<LLUUID> friendsInAnyGroup;
	std::vector<std::string> groups = getAllGroups(FALSE);
	for(U32 g = 0; g < (U32)groups.size(); g++)
	{
		LLSD friends = mContactSets[groups[g]]["friends"];
		LLSD::map_const_iterator loc_it = friends.beginMap();
		LLSD::map_const_iterator loc_end = friends.endMap();
		for ( ; loc_it != loc_end; ++loc_it)
		{
			const LLSD& friendID = (*loc_it).first;
			friendsInAnyGroup.insert(friendID);
		}
	}
	return std::vector<LLUUID>(friendsInAnyGroup.begin(),friendsInAnyGroup.end());
}

BOOL LGGContactSets::isFriendInAnyGroup(LLUUID friend_id)
{
	std::vector<std::string> groups = getAllGroups(FALSE);
	for(U32 g = 0; g < (U32)groups.size(); g++)
	{
		if(mContactSets[groups[g]].has("friends"))
		{
			if(mContactSets[groups[g]]["friends"].has(friend_id.asString()))
			{
				return TRUE;
			}
		}
	}
	return FALSE;
}

BOOL LGGContactSets::isFriendInGroup(LLUUID friend_id, std::string groupName)
{
	if(groupName == "All Sets")
		return isFriendInAnyGroup(friend_id);
	if(groupName == "No Sets")
		return !isFriendInAnyGroup(friend_id);
	if(groupName == "ReNamed")
		return hasPseudonym(friend_id);
	if(groupName == "Non Friends")
		return isNonFriend(friend_id);
	if(mContactSets[groupName].has("friends"))
		return mContactSets[groupName]["friends"].has(friend_id.asString());
	return FALSE;
}

BOOL LGGContactSets::notifyForFriend(LLUUID friend_id)
{
	BOOL notify = FALSE;
	std::vector<std::string> groups = getFriendGroups(friend_id);
	for(U32 i = 0; i < (U32)groups.size(); i++)
	{
		if(mContactSets[groups[i]]["notify"].asBoolean())return TRUE;
	}
	return notify;
}

void LGGContactSets::addFriendToGroup(LLUUID friend_id, std::string groupName)
{
	if(friend_id.notNull() && groupName!="" && groupName != "extraAvs" && groupName!="pseudonym" && groupName !="globalSettings" && groupName!="No Sets" && groupName!="All Sets" && groupName!="All Groups"  && groupName!="ReNamed" && groupName!="Non Friends")
	{
		mContactSets[groupName]["friends"][friend_id.asString()]="";
		saveToDisk(mContactSets);
	}
}

void LGGContactSets::addNonFriendToList(LLUUID non_friend_id)
{
	mContactSets["extraAvs"][non_friend_id.asString()]="";
	saveToDisk(mContactSets);
}

void LGGContactSets::removeNonFriendFromList(LLUUID non_friend_id)
{
	if(mContactSets["extraAvs"].has(non_friend_id.asString()))
	{
		mContactSets["extraAvs"].erase(non_friend_id.asString());
		if(!LLAvatarTracker::instance().isBuddy(non_friend_id))
		{
			clearPseudonym(non_friend_id);
			removeFriendFromAllGroups(non_friend_id);
		}
		saveToDisk(mContactSets);
	}
}

void LGGContactSets::removeFriendFromAllGroups(LLUUID friend_id)
{
	std::vector<std::string> groups = getFriendGroups(friend_id);
	for (U32 i = 0; i < (U32)groups.size(); i++)
	{
		removeFriendFromGroup(friend_id,groups[i]);
	}
}

BOOL LGGContactSets::isNonFriend(LLUUID non_friend_id)
{
	if(LLAvatarTracker::instance().isBuddy(non_friend_id))
	{
		return FALSE;
	}
	if(mContactSets["extraAvs"].has(non_friend_id.asString()))
	{
		return TRUE;
	}
	return FALSE;
}

std::vector<LLUUID> LGGContactSets::getListOfNonFriends()
{
	std::vector<LLUUID> toReturn;
	toReturn.clear();

	LLSD friends = mContactSets["extraAvs"];
	LLSD::map_const_iterator loc_it = friends.beginMap();
	LLSD::map_const_iterator loc_end = friends.endMap();
	for ( ; loc_it != loc_end; ++loc_it)
	{
		const LLSD& friendID = (*loc_it).first;
		if(friendID.asString() == "friends")
		{
			friends.erase(friendID.asString());
			continue;
		}
		if(friendID.asUUID().notNull())
			if(!LLAvatarTracker::instance().isBuddy(friendID))
				toReturn.push_back(friendID.asUUID());
	}
	return toReturn;
}

std::vector<LLUUID> LGGContactSets::getListOfPseudonymAvs()
{
	std::vector<LLUUID> toReturn;
	toReturn.clear();

	LLSD friends = mContactSets["pseudonym"];
	LLSD::map_const_iterator loc_it = friends.beginMap();
	LLSD::map_const_iterator loc_end = friends.endMap();
	for ( ; loc_it != loc_end; ++loc_it)
	{
		const LLSD& friendID = (*loc_it).first;
		if(friendID.asString() == "friends")
		{
			friends.erase(friendID.asString());
			continue;
		}
		if(friendID.asUUID().notNull())
			toReturn.push_back(friendID.asUUID());
	}

	return toReturn;
}

void LGGContactSets::setPseudonym(LLUUID friend_id, std::string pseudonym)
{
	mContactSets["pseudonym"][friend_id.asString()]=pseudonym;
	saveToDisk(mContactSets);
}

std::string LGGContactSets::getPseudonym(LLUUID friend_id)
{
	if(mContactSets["pseudonym"].has(friend_id.asString()))
	{
		return mContactSets["pseudonym"][friend_id.asString()];
	}
	return "";
}

void LGGContactSets::clearPseudonym(LLUUID friend_id)
{
	if(mContactSets["pseudonym"].has(friend_id.asString()))
	{
		mContactSets["pseudonym"].erase(friend_id.asString());
		LLAvatarNameCache::fetch(friend_id);//update
		saveToDisk(mContactSets);
	}
}

BOOL LGGContactSets::hasPseudonym(LLUUID friend_id)
{
	if(getPseudonym(friend_id)!="")return TRUE;
	return FALSE;
}

BOOL LGGContactSets::hasDisplayNameRemoved(LLUUID friend_id)
{
	return (getPseudonym(friend_id) == "--- ---");
}

BOOL LGGContactSets::hasVisuallyDiferentPseudonym(LLUUID friend_id)
{
	return (hasPseudonym(friend_id) && (!hasDisplayNameRemoved(friend_id)));
}

void LGGContactSets::removeDisplayName(LLUUID friend_id)
{
	setPseudonym(friend_id,"--- ---");
}

void LGGContactSets::removeFriendFromGroup(LLUUID friend_id, std::string groupName)
{
	if(groupName == "extraAvs"||groupName == "Non Friends")
	{
		return removeNonFriendFromList(friend_id);
	}
	if(groupName == "ReNamed" || groupName == "pseudonym")
	{
		return clearPseudonym(friend_id);
	}
	if(friend_id.notNull() && groupName!="")
	{
		if(mContactSets[groupName]["friends"].has(friend_id.asString()))
		{
			mContactSets[groupName]["friends"].erase(friend_id.asString());
			saveToDisk(mContactSets);
		}
	}
}

bool LGGContactSets::isAGroup(std::string groupName)
{
	if(mContactSets.has(groupName))
	{
		if(mContactSets[groupName].has("color"))
			return TRUE;
	}
	return FALSE;
}

void LGGContactSets::addGroup(std::string groupName)
{

	if(groupName != "")
	{
		mContactSets[groupName]["color"] = LLColor4::red.getValue();
		saveToDisk(mContactSets);
	}
}

void LGGContactSets::deleteGroup(std::string groupName)
{
	if(mContactSets.has(groupName))
	{
		mContactSets.erase(groupName);
		saveToDisk(mContactSets);
	}
}

void LGGContactSets::setNotifyForGroup(std::string groupName, BOOL notify)
{
	if(groupName == "All Sets" || groupName == "globalSettings" || groupName == "" || groupName == "No Sets" ||groupName == "ReNamed" || groupName == "Non Friends")
		return;

	if(mContactSets.has(groupName))
	{
		mContactSets[groupName]["notify"] = notify;
		saveToDisk(mContactSets);
	}
}

BOOL LGGContactSets::getNotifyForGroup(std::string groupName)
{
	if(mContactSets.has(groupName))
	{
		if(mContactSets[groupName].has("notify"))
		{
			return mContactSets[groupName]["notify"].asBoolean();
		}
	}
	return FALSE;
}

void LGGContactSets::setGroupColor(std::string groupName, LLColor4 color)
{
	if(groupName == "All Sets" ||   groupName == "globalSettings" || groupName == "" || groupName == "No Sets" ||groupName == "ReNamed" || groupName == "Non Friends")
		return;

	if(mContactSets.has(groupName))
	{
		mContactSets[groupName]["color"] = color.getValue();
		saveToDisk(mContactSets);
	}
}
