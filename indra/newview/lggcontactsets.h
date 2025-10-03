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
#include <unordered_map>
#include <unordered_set>
#include <boost/signals2.hpp>

enum class ContactSetType
{
    CHAT,
    IM,
    TAG,
    RADAR,
    MINIMAP
};

constexpr char CS_SET_ALL_SETS[] = "All Sets";
constexpr char CS_SET_NO_SETS[] = "No Sets";
constexpr char CS_SET_EXTRA_AVS[] = "extraAvs";
constexpr char CS_SET_PSEUDONYM[] = "Pseudonyms";
constexpr char CS_GLOBAL_SETTINGS[] = "globalSettings";
constexpr char CS_PSEUDONYM[] = "--- ---";
constexpr char CS_PSEUDONYM_QUOTED[] = "'--- ---'";

class LGGContactSets : public LLSingleton<LGGContactSets>
{
    LOG_CLASS(LGGContactSets);

    LLSINGLETON(LGGContactSets);
    ~LGGContactSets();

public:
    typedef std::vector<std::string> string_vec_t;
    typedef std::unordered_set<LLUUID> uuid_set_t;

    void loadFromDisk();

    void setSetColor(std::string_view set_name, const LLColor4& color);
    LLColor4 getSetColor(std::string_view set_name) const;
    LLColor4 getFriendColor(const LLUUID& friend_id, std::string_view ignored_set_name = "") const;
    LLColor4 colorize(const LLUUID& uuid, LLColor4 color, ContactSetType type) const;

    void setDefaultColor(const LLColor4& default_color) { mDefaultColor = default_color; };
    LLColor4 getDefaultColor() const { return mDefaultColor; };

    std::string getPseudonym(const LLUUID& friend_id) const;
    bool hasPseudonym(const LLUUID& friend_id) const;
    bool hasPseudonym(const uuid_vec_t& ids) const;
    void clearPseudonym(const LLUUID& friend_id, bool save_changes = true);

    void removeDisplayName(const LLUUID& friend_id);
    bool hasDisplayNameRemoved(const LLUUID& friend_id) const;
    bool hasDisplayNameRemoved(const uuid_vec_t& ids) const;

    bool checkCustomName(const LLUUID& id, bool& dn_removed, std::string& pseudonym) const;

    string_vec_t getFriendSets(const LLUUID& friend_id) const;
    string_vec_t getAllContactSets() const;

    void addToSet(const uuid_vec_t&, std::string_view set_name);
    void removeFriendFromSet(const LLUUID& friend_id, std::string_view set_name, bool save_changes = true);
    void removeFriendFromAllSets(const LLUUID& friend_id, bool save_changes = true);
    bool isFriendInSet(const LLUUID& friend_id, std::string_view set_name) const;
    bool hasFriendColorThatShouldShow(const LLUUID& friend_id, ContactSetType type) const;
    bool hasFriendColorThatShouldShow(const LLUUID& friend_id, ContactSetType type, LLColor4& color) const;

    void addSet(std::string_view set_name);
    bool renameSet(std::string_view set_name, std::string_view new_set_name);
    void removeSet(std::string_view set_name);
    bool isValidSet(std::string_view set_name) const;

    void removeNonFriendFromList(const LLUUID& non_friend_id, bool save_changes = true);
    bool isNonFriend(const LLUUID& non_friend_id) const;
    bool isFriendInAnySet(const LLUUID& friend_id) const;
    uuid_vec_t getListOfNonFriends() const;
    uuid_vec_t getListOfPseudonymAvs() const;

    bool notifyForFriend(const LLUUID& friend_id) const;
    void setNotifyForSet(std::string_view set_name, bool notify);
    bool getNotifyForSet(std::string_view set_name) const;

    bool callbackAliasReset(const LLSD& notification, const LLSD& response);

    bool isInternalSetName(std::string_view set_name) const;
    bool hasSets() const { return !mContactSets.empty(); }

    class ContactSet
    {
    public:
        bool hasFriend(const LLUUID& avatar_id) const
        {
            return (mFriends.find(avatar_id) != mFriends.end());
        }

        std::string     mName;
        uuid_set_t      mFriends;
        bool            mNotify;
        LLColor4        mColor;
    };
    ContactSet* getContactSet(std::string_view set_name) const;

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
    uuid_vec_t getFriendsInSet(std::string_view set_name) const;
    uuid_vec_t getFriendsInAnySet() const;

    void setPseudonym(const LLUUID& friend_id, std::string_view pseudonym);
    bool hasVisuallyDifferentPseudonym(const LLUUID& friend_id) const;

    LLSD exportContactSet(std::string_view set_name);
    bool saveContactSetToDisk(std::string_view set_name, std::string_view filename);

    std::string getFilename() const;
    std::string getDefaultFilename() const;

    void importFromLLSD(const LLSD& data);
    LLSD exportToLLSD();
    void saveToDisk();

    typedef std::unordered_map<LLUUID, std::string> uuid_map_t;
    typedef std::map<std::string, ContactSet*> contact_set_map_t;
    contact_set_map_t mContactSets;

    LLColor4        mDefaultColor{ LLColor4::grey };
    uuid_set_t      mExtraAvatars;
    uuid_map_t      mPseudonyms;

    typedef std::map<LLUUID, boost::signals2::connection> avatar_name_cache_connection_map_t;
    avatar_name_cache_connection_map_t mAvatarNameCacheConnections;
    void onAvatarNameCache(const LLUUID& av_id);
};

#endif // LGG_CONTACTSETS_H
