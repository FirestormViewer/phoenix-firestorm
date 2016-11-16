/* @file lggcontactsets.h
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

#ifndef LGG_CONTACTSETS_H
#define LGG_CONTACTSETS_H

#include "v4color.h"
#include "llsingleton.h"
#include <boost/signals2.hpp>
#include <boost/unordered_map.hpp>
#include <boost/unordered_set.hpp>

typedef enum e_lgg_cs
{
	LGG_CS_CHAT,
	LGG_CS_IM,
	LGG_CS_TAG,
	LGG_CS_RADAR,
	LGG_CS_MINIMAP
} ELGGCSType;

const std::string CS_SET_ALL_SETS = "All Sets";
const std::string CS_SET_NO_SETS = "No Sets";
const std::string CS_SET_EXTRA_AVS = "extraAvs";
const std::string CS_SET_PSEUDONYM = "Pseudonyms";
const std::string CS_GLOBAL_SETTINGS = "globalSettings";
const std::string CS_PSEUDONYM = "--- ---";

class LGGContactSets : public LLSingleton<LGGContactSets>
{
	LOG_CLASS(LGGContactSets);

	LLSINGLETON(LGGContactSets);
	~LGGContactSets();

public:
	typedef std::vector<std::string> string_vec_t;
	typedef boost::unordered_set<LLUUID, FSUUIDHash> uuid_set_t;

	void loadFromDisk();

	void setSetColor(const std::string& set_name, const LLColor4& color);
	LLColor4 getSetColor(const std::string& set_name);
	LLColor4 getFriendColor(const LLUUID& friend_id, const std::string& ignored_set_name = "");
	LLColor4 colorize(const LLUUID& uuid, const LLColor4& cur_color, ELGGCSType type);

	void setDefaultColor(const LLColor4& default_color) { mDefaultColor = default_color; };
	LLColor4 getDefaultColor() { return mDefaultColor; };

	std::string getPseudonym(const LLUUID& friend_id);
	bool hasPseudonym(const LLUUID& friend_id);
	bool hasPseudonym(uuid_vec_t ids);
	void clearPseudonym(const LLUUID& friend_id);

	void removeDisplayName(const LLUUID& friend_id);
	bool hasDisplayNameRemoved(const LLUUID& friend_id);
	bool hasDisplayNameRemoved(uuid_vec_t ids);

	string_vec_t getFriendSets(const LLUUID& friend_id);
	string_vec_t getAllContactSets();

	void addFriendToSet(const LLUUID& friend_id, const std::string& set_name);
	void removeFriendFromSet(const LLUUID& friend_id, const std::string& set_name);
	void removeFriendFromAllSets(const LLUUID& friend_id);
	bool isFriendInSet(const LLUUID& friend_id, const std::string& set_name);
	bool hasFriendColorThatShouldShow(const LLUUID& friend_id, ELGGCSType type);
	bool hasFriendColorThatShouldShow(const LLUUID& friend_id, ELGGCSType type, LLColor4& color);

	void addSet(const std::string& set_name);
	bool renameSet(const std::string& set_name, const std::string& new_set_name);
	void removeSet(const std::string& set_name);
	bool isValidSet(const std::string& set_name);

	void addNonFriendToList(const LLUUID& non_friend_id);
	void removeNonFriendFromList(const LLUUID& non_friend_id);
	bool isNonFriend(const LLUUID& non_friend_id);
	bool isFriendInSet(const LLUUID& friend_id);
	uuid_vec_t getListOfNonFriends();
	uuid_vec_t getListOfPseudonymAvs();

	bool notifyForFriend(const LLUUID& friend_id);
	void setNotifyForSet(const std::string& set_name, bool notify);
	bool getNotifyForSet(const std::string& set_name);

	bool callbackAliasReset(const LLSD& notification, const LLSD& response);

	bool isInternalSetName(const std::string& set_name);
	bool hasSets() { return !mContactSets.empty(); }

	class ContactSet
	{
	public:
		bool hasFriend(const LLUUID& avatar_id)
		{
			return (mFriends.find(avatar_id) != mFriends.end());
		}

		std::string		mName;
		uuid_set_t		mFriends;
		bool			mNotify;
		LLColor4		mColor;
	};
	ContactSet* getContactSet(const std::string& set_name);
	
	// [FS:CR] Signals for updating the various UI
	typedef enum e_contact_set_update {
		UPDATED_MEMBERS = 0,
		UPDATED_LISTS
	} EContactSetUpdate;
	typedef boost::signals2::signal<void(EContactSetUpdate type)> contact_set_changed_signal_t;
	contact_set_changed_signal_t mChangedSignal;
	boost::signals2::connection setContactSetChangeCallback(const contact_set_changed_signal_t::slot_type& cb)
	{
		return mChangedSignal.connect(cb);
	};
	
	// Notification callbacks
	static bool handleAddContactSetCallback(const LLSD& notification, const LLSD& response);
	static bool handleRemoveContactSetCallback(const LLSD& notification, const LLSD& response);
	static bool handleRemoveAvatarFromSetCallback(const LLSD& notification, const LLSD& response);
	static bool handleSetAvatarPseudonymCallback(const LLSD& notification, const LLSD& response);
	// [/FS:CR]
	
private:
	void toneDownColor(LLColor4& color) const;
	uuid_vec_t getFriendsInSet(const std::string& set_name);
	uuid_vec_t getFriendsInAnySet();

	void setPseudonym(const LLUUID& friend_id, const std::string& pseudonym);
	bool hasVisuallyDifferentPseudonym(const LLUUID& friend_id);

	LLSD exportContactSet(const std::string& set_name);
	bool saveContactSetToDisk(const std::string& set_name, const std::string& filename);

	std::string getFilename();
	std::string getDefaultFilename();

	void importFromLLSD(const LLSD& data);
	LLSD exportToLLSD();
	void saveToDisk();
	
	typedef boost::unordered_map<LLUUID, std::string, FSUUIDHash> uuid_map_t;
	typedef std::map<std::string, ContactSet*> contact_set_map_t;
	contact_set_map_t mContactSets;

	LLColor4		mDefaultColor;
	uuid_set_t		mExtraAvatars;
	uuid_map_t		mPseudonyms;

	typedef std::map<LLUUID, boost::signals2::connection> avatar_name_cache_connection_map_t;
	avatar_name_cache_connection_map_t mAvatarNameCacheConnections;
	void onAvatarNameCache(const LLUUID& av_id);
};

#endif // LGG_CONTACTSETS_H
