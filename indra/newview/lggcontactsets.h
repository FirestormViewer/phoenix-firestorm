/* @file lggcontactsets.h
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

#ifndef LGG_CONTACTSETS_H
#define LGG_CONTACTSETS_H

#include "v4color.h"
#include "llsingleton.h"

typedef enum e_lgg_cs
{
	LGG_CS_CHAT,
	LGG_CS_TAG,
	LGG_CS_RADAR,
	LGG_CS_MINIMAP
} ELGGCSType;

class LGGContactSets : public LLSingleton<LGGContactSets>
{
	friend class LLSingleton<LGGContactSets>;
	LOG_CLASS(LGGContactSets);

public:
	static LLColor4 toneDownColor(const LLColor4& inColor, float strength, bool usedForBackground = false);

	void setGroupColor(const std::string& groupName, const LLColor4& color);
	LLColor4 getGroupColor(const std::string& groupName);
	LLColor4 getFriendColor(const LLUUID& friend_id, const std::string& ignoredGroupName = "");

	void setDefaultColor(const LLColor4& dColor);
	LLColor4 getDefaultColor();

	std::string getPseudonym(const LLUUID& friend_id);
	BOOL hasPseudonym(const LLUUID& friend_id);
	void clearPseudonym(const LLUUID& friend_id);

	void removeDisplayName(const LLUUID& friend_id);
	BOOL hasDisplayNameRemoved(const LLUUID& friend_id);

	std::vector<std::string> getInnerGroups(const std::string& groupName);
	std::vector<std::string> getFriendGroups(const LLUUID& friend_id);
	std::vector<std::string> getAllGroups(BOOL extraGroups = TRUE);

	void addFriendToGroup(const LLUUID& friend_id, const std::string& groupName);
	void removeFriendFromGroup(const LLUUID& friend_id, const std::string& groupName);
	void removeFriendFromAllGroups(const LLUUID& friend_id);
	BOOL isFriendInGroup(const LLUUID& friend_id, const std::string& groupName);
	BOOL hasFriendColorThatShouldShow(const LLUUID& friend_id, ELGGCSType type);

	void addGroup(const std::string& groupName);
	void deleteGroup(const std::string& groupName);
	bool isAGroup(const std::string& groupName);

	void addNonFriendToList(const LLUUID& non_friend_id);
	void removeNonFriendFromList(const LLUUID& non_friend_id);
	BOOL isNonFriend(const LLUUID& non_friend_id);
	std::vector<LLUUID> getListOfNonFriends();

	BOOL notifyForFriend(const LLUUID& friend_id);
	void setNotifyForGroup(const std::string& groupName, BOOL notify);
	BOOL getNotifyForGroup(const std::string& groupName);

	bool callbackAliasReset(const LLSD& notification, const LLSD& response);

private:
	LGGContactSets();
	~LGGContactSets();
	
	LLSD getContactSets();

	std::vector<LLUUID> getFriendsInGroup(const std::string& groupName);
	BOOL isFriendInAnyGroup(const LLUUID& friend_id);
	std::vector<LLUUID> getFriendsInAnyGroup();

	void setPseudonym(const LLUUID& friend_id, const std::string& pseudonym);
	std::vector<LLUUID> getListOfPseudonymAvs();
	BOOL hasVisuallyDifferentPseudonym(const LLUUID& friend_id);

	void loadFromDisk();
	LLSD exportGroup(const std::string& groupName);
	void saveToDisk(const LLSD& newSettings);
	BOOL saveGroupToDisk(const std::string& groupName, const std::string& fileName);

	std::string getFileName();
	std::string getDefaultFileName();
	std::string getOldFileName();

	LLSD mContactSets;
};

#endif // LGG_CONTACTSETS_H
