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
#include "fsdata.h"
#include "fsradar.h"
#include "llagent.h"
#include "llavatarnamecache.h"
#include "llcallingcard.h"
#include "lldir.h"
#include "llsdutil.h"
#include "llmutelist.h"
#include "llnotifications.h"
#include "llnotificationsutil.h"
#include "llsdserialize.h"
#include "lluicolor.h"
#include "lluicolortable.h"
#include "llviewercontrol.h"
#include "llvoavatar.h"
#include "rlvactions.h"
#include "rlvhandler.h"

constexpr F32 COLOR_DAMPENING = 0.8f;
constexpr char CONTACT_SETS_FILE[] = "settings_friends_groups.xml";

LGGContactSets::LGGContactSets()
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
    if (S32 option = LLNotification::getSelectedOption(notification, response); option == 0)
    {
        clearPseudonym(notification["payload"]["agent_id"].asUUID());
    }
    return false;
}

std::string LGGContactSets::getFilename() const
{
    std::string path = gDirUtilp->getExpandedFilename(LL_PATH_PER_SL_ACCOUNT, "");

    if (!path.empty())
    {
        path = gDirUtilp->getExpandedFilename(LL_PATH_PER_SL_ACCOUNT, CONTACT_SETS_FILE);
    }
    return path;
}

std::string LGGContactSets::getDefaultFilename() const
{
    std::string path = gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, "");

    if (!path.empty())
    {
        path = gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, CONTACT_SETS_FILE);
    }
    return path;
}

LLSD LGGContactSets::exportContactSet(std::string_view set_name)
{
    LLSD ret;

    if (ContactSet* set = getContactSet(set_name); set)
    {
        ret["groupname"] = set->mName;
        ret["color"] = set->mColor.getValue();
        ret["notices"] = set->mNotify;
        for (const auto& friend_id : set->mFriends)
        {
            ret["friends"][friend_id.asString()] = "";
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

bool LGGContactSets::saveContactSetToDisk(std::string_view set_name, std::string_view filename)
{
    if (isValidSet(set_name))
    {
        llofstream file;
        file.open(filename.data());
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
    for (const auto& avid : mExtraAvatars)
    {
        output[CS_SET_EXTRA_AVS][avid.asString()] = "";
    }

    // Pseudonyms
    for (const auto& [avid, pseudonym] : mPseudonyms)
    {
        output[CS_SET_PSEUDONYM][avid.asString()] = pseudonym;
    }

    // Contact Sets
    for (const auto& [name, set] : mContactSets)
    {
        output[name]["color"] = set->mColor.getValue();
        output[name]["notify"] = set->mNotify;
        for (const auto& friend_id : set->mFriends)
        {
            output[name]["friends"][friend_id.asString()] = "";
        }
    }

    return output;
}

void LGGContactSets::importFromLLSD(const LLSD& data)
{
    for (const auto& [key, value] : llsd::inMap(data))
    {
        if (isInternalSetName(key))
        {
            if (key == CS_GLOBAL_SETTINGS)
            {
                LLSD global_setting_data = value;

                LLColor4 color = LLColor4::grey;
                if (global_setting_data.has("defaultColor"))
                {
                    color = global_setting_data["defaultColor"];
                }
                mDefaultColor = color;
            }

            if (key == CS_SET_EXTRA_AVS)
            {
                for (const auto& [av_key, av_value] : llsd::inMap(value))
                {
                    mExtraAvatars.insert(LLUUID(av_key));
                }
            }

            if (key == CS_SET_PSEUDONYM)
            {
                for (const auto& [av_key, av_value] : llsd::inMap(value))
                {
                    mPseudonyms.emplace(LLUUID(av_key), av_value.asString());
                }
            }
        }
        else
        {
            LLSD set_data = value;

            ContactSet* new_set = new ContactSet();
            new_set->mName = key;

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
                for (const auto& [av_key, av_value] : llsd::inMap(set_data["friends"]))
                {
                    new_set->mFriends.emplace(LLUUID(av_key));
                }
            }

            mContactSets[key] = new_set;
        }
    }
}

LLColor4 LGGContactSets::getSetColor(std::string_view set_name) const
{
    if (ContactSet* set = getContactSet(set_name); set)
    {
        return set->mColor;
    }

    return getDefaultColor();
};

LLColor4 LGGContactSets::colorize(const LLUUID& uuid, LLColor4 color, ContactSetType type) const
{
    static LLCachedControl<bool> legacy_radar_friend(gSavedSettings, "FSLegacyRadarFriendColoring");
    static LLCachedControl<bool> legacy_radar_linden(gSavedSettings, "FSLegacyRadarLindenColoring");
    bool rlv_shownames = !RlvActions::canShowName(RlvActions::SNC_DEFAULT, uuid);

    if (uuid == gAgentID)
    {
        switch (type)
        {
            case ContactSetType::CHAT:
            case ContactSetType::IM:
                color = LLUIColorTable::instance().getColor("UserChatColor", LLColor4::white).get();
                break;
            case ContactSetType::TAG:
                color = LLUIColorTable::instance().getColor("NameTagSelf", LLColor4::white).get();
                break;
            case ContactSetType::MINIMAP:
                color = LLUIColorTable::instance().getColor("MapAvatarSelfColor", LLColor4::white).get();
                break;
            case ContactSetType::RADAR:
                // Do nothing
                break;
            default:
                LL_DEBUGS("ContactSets") << "Unhandled colorize case!" << LL_ENDL;
                break;
        }
    }
    else if (LLAvatarTracker::instance().getBuddyInfo(uuid))
    {
        switch (type)
        {
            case ContactSetType::CHAT:
                if (!rlv_shownames)
                    color = LLUIColorTable::instance().getColor("FriendsChatColor", LLColor4::white).get();
                break;
            case ContactSetType::IM:
                color = LLUIColorTable::instance().getColor("FriendsChatColor", LLColor4::white).get();
                break;
            case ContactSetType::TAG:
            {
                // This is optional per prefs.
                static LLCachedControl<bool> color_friends(gSavedSettings, "NameTagShowFriends");
                if (color_friends && !rlv_shownames)
                {
                    color = LLUIColorTable::instance().getColor("NameTagFriend", LLColor4::white).get();
                }
            }
            break;
            case ContactSetType::MINIMAP:
                if (!rlv_shownames)
                    color = LLUIColorTable::instance().getColor("MapAvatarFriendColor", LLColor4::white).get();
                break;
            case ContactSetType::RADAR:
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
            case ContactSetType::CHAT:
            case ContactSetType::IM:
                color = LLUIColorTable::instance().getColor("MutedChatColor", LLColor4::grey3).get();
                break;
            case ContactSetType::TAG:
                color = LLUIColorTable::instance().getColor("NameTagMuted", LLColor4::grey3).get();
                break;
            case ContactSetType::MINIMAP:
                color = LLUIColorTable::instance().getColor("MapAvatarMutedColor", LLColor4::grey3).get();
                break;
            case ContactSetType::RADAR:
                // Do nothing
                break;
            default:
                LL_DEBUGS("ContactSets") << "Unhandled colorize case!" << LL_ENDL;
                break;
        }
    }
    else if (FSData::instance().isAgentFlag(uuid, FSData::CHAT_COLOR))
    {
        switch (type)
        {
            case ContactSetType::CHAT:
            case ContactSetType::IM:
                color = LLUIColorTable::instance().getColor("FirestormChatColor", LLColor4::red).get();
                break;
            case ContactSetType::TAG:
                color = LLUIColorTable::instance().getColor("NameTagFirestorm", LLColor4::red).get();
                break;
            case ContactSetType::MINIMAP:
                color = LLUIColorTable::instance().getColor("MapAvatarFirestormColor", LLColor4::red).get();
                break;
            case ContactSetType::RADAR:
                // Do nothing
                break;
            default:
                LL_DEBUGS("ContactSets") << "Unhandled colorize case!" << LL_ENDL;
                break;
        }
    }
    else
    {
        if (auto entry = FSRadar::getInstance()->getEntry(uuid); (entry && entry->getIsLinden()) || (!entry && FSCommon::isLinden(uuid)) )
        {
            switch (type)
            {
                case ContactSetType::CHAT:
                case ContactSetType::IM:
                    color = LLUIColorTable::instance().getColor("LindenChatColor", LLColor4::blue).get();
                    break;
                case ContactSetType::TAG:
                    color = LLUIColorTable::instance().getColor("NameTagLinden", LLColor4::blue).get();
                    break;
                case ContactSetType::MINIMAP:
                    color = LLUIColorTable::instance().getColor("MapAvatarLindenColor", LLColor4::blue).get();
                    break;
                case ContactSetType::RADAR:
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

LLColor4 LGGContactSets::getFriendColor(const LLUUID& friend_id, std::string_view ignored_set_name) const
{
    LLColor4 color = getDefaultColor();
    if (ignored_set_name == CS_SET_NO_SETS)
    {
        return color;
    }

    U32 lowest{ U32_MAX };
    for (const auto& set_name : getFriendSets(friend_id))
    {
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

                color = mContactSets.at(set_name)->mColor;
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
            return mContactSets.at(ignored_set_name.data())->mColor;
        }
    }
    return color;
}

bool LGGContactSets::hasFriendColorThatShouldShow(const LLUUID& friend_id, ContactSetType type) const
{
    LLColor4 color = LLColor4::white;
    return hasFriendColorThatShouldShow(friend_id, type, color);
}

// handle all settings and rlv that would prevent us from showing the cs color
bool LGGContactSets::hasFriendColorThatShouldShow(const LLUUID& friend_id, ContactSetType type, LLColor4& color) const
{
    if (!RlvActions::canShowName(RlvActions::SNC_DEFAULT, friend_id))
    {
        return false; // don't show colors if we cant show names
    }

    static LLCachedControl<bool> fsContactSetsColorizeChat(gSavedSettings, "FSContactSetsColorizeChat", false);
    static LLCachedControl<bool> fsContactSetsColorizeTag(gSavedSettings,"FSContactSetsColorizeNameTag", false);
    static LLCachedControl<bool> fsContactSetsColorizeRadar(gSavedSettings,"FSContactSetsColorizeRadar", false);
    static LLCachedControl<bool> fsContactSetsColorizeMiniMap(gSavedSettings,"FSContactSetsColorizeMiniMap", false);

    switch (type)
    {
        case ContactSetType::CHAT:
        case ContactSetType::IM:
            if (!fsContactSetsColorizeChat)
                return false;
            break;
        case ContactSetType::TAG:
            if (!fsContactSetsColorizeTag)
                return false;
            break;
        case ContactSetType::RADAR:
            if (!fsContactSetsColorizeRadar)
                return false;
            break;
        case ContactSetType::MINIMAP:
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

string_vec_t LGGContactSets::getFriendSets(const LLUUID& friend_id) const
{
    string_vec_t sets{};

    for (const auto& [set_name, set] : mContactSets)
    {
        if (set->hasFriend(friend_id))
        {
            sets.emplace_back(set->mName);
        }
    }
    return sets;
}

uuid_vec_t LGGContactSets::getFriendsInSet(std::string_view set_name) const
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

    if (ContactSet* set = getContactSet(set_name); set)
    {
        for (const auto& id : set->mFriends)
        {
            friends.emplace_back(id);
        }
    }

    return friends;
}

string_vec_t LGGContactSets::getAllContactSets() const
{
    string_vec_t sets{};

    for (const auto& [set_name, set] : mContactSets)
    {
        sets.push_back(set->mName);
    }

    return sets;
}

uuid_vec_t LGGContactSets::getFriendsInAnySet() const
{
    uuid_set_t friendsInAnySet{};

    for (const auto& [set_name, set] : mContactSets)
    {
        for (const auto& friend_id : set->mFriends)
        {
            friendsInAnySet.emplace(friend_id);
        }
    }

    return uuid_vec_t(friendsInAnySet.begin(), friendsInAnySet.end());
}

bool LGGContactSets::isFriendInAnySet(const LLUUID& friend_id) const
{
    for (const auto& [set_name, set] : mContactSets)
    {
        if (set->hasFriend(friend_id))
        {
            return true;
        }
    }

    return false;
}

bool LGGContactSets::isFriendInSet(const LLUUID& friend_id, std::string_view set_name) const
{
    if (set_name == CS_SET_ALL_SETS)
    {
        return isFriendInAnySet(friend_id);
    }
    else if (set_name == CS_SET_NO_SETS)
    {
        return !isFriendInAnySet(friend_id);
    }
    else if (set_name == CS_SET_PSEUDONYM)
    {
        return hasPseudonym(friend_id);
    }
    else if (set_name == CS_SET_EXTRA_AVS)
    {
        return isNonFriend(friend_id);
    }

    if (set_name.empty())
        return false;

    if (ContactSet* set = getContactSet(set_name); set)
    {
        return set->hasFriend(friend_id);
    }

    return false;
}

bool LGGContactSets::notifyForFriend(const LLUUID& friend_id) const
{
    for (const auto& set_name : getFriendSets(friend_id))
    {
        if (mContactSets.at(set_name)->mNotify)
        {
            return true;
        }
    }
    return false;
}

void LGGContactSets::addToSet(const uuid_vec_t& avatar_ids, std::string_view set_name)
{
    LLAvatarTracker& tracker = LLAvatarTracker::instance();

    for (auto const& avatar_id : avatar_ids)
    {
        if (!tracker.isBuddy(avatar_id))
        {
            mExtraAvatars.insert(avatar_id);
        }

        if (isValidSet(set_name))
        {
            mContactSets[set_name.data()]->mFriends.insert(avatar_id);
        }
    }

    saveToDisk();
    mChangedSignal(UPDATED_MEMBERS);
}

void LGGContactSets::removeNonFriendFromList(const LLUUID& non_friend_id, bool save_changes /*= true*/)
{
    LLAvatarTracker& tracker = LLAvatarTracker::instance();
    if (auto found = mExtraAvatars.find(non_friend_id); found != mExtraAvatars.end())
    {
        mExtraAvatars.erase(found);

        if (!tracker.isBuddy(non_friend_id))
        {
            clearPseudonym(non_friend_id, save_changes);
            removeFriendFromAllSets(non_friend_id, save_changes);
        }

        if (save_changes)
        {
            saveToDisk();
            mChangedSignal(UPDATED_MEMBERS);
        }
    }
}

void LGGContactSets::removeFriendFromAllSets(const LLUUID& friend_id, bool save_changes /*= true*/)
{
    for (const auto& set_name : getFriendSets(friend_id))
    {
        removeFriendFromSet(friend_id, set_name, save_changes);
    }
}

bool LGGContactSets::isNonFriend(const LLUUID& non_friend_id) const
{
    if (LLAvatarTracker::instance().isBuddy(non_friend_id))
    {
        return false;
    }

    return (mExtraAvatars.find(non_friend_id) != mExtraAvatars.end());
}

uuid_vec_t LGGContactSets::getListOfNonFriends() const
{
    LLAvatarTracker& tracker = LLAvatarTracker::instance();
    uuid_vec_t nonfriends{};

    for (const auto& friend_id : mExtraAvatars)
    {
        if (!tracker.isBuddy(friend_id))
        {
            nonfriends.emplace_back(friend_id);
        }
    }

    return nonfriends;
}

uuid_vec_t LGGContactSets::getListOfPseudonymAvs() const
{
    uuid_vec_t pseudonyms{};

    for (const auto& [id, pseudonym] : mPseudonyms)
    {
        pseudonyms.emplace_back(pseudonym);
    }

    return pseudonyms;
}

void LGGContactSets::setPseudonym(const LLUUID& friend_id, std::string_view pseudonym)
{
    LLAvatarNameCache* inst = LLAvatarNameCache::getInstance();
    mPseudonyms[friend_id] = pseudonym;
    inst->erase(friend_id);
    inst->fetch(friend_id);
    LLVOAvatar::invalidateNameTag(friend_id);

    if (LLAvatarName av_name; LLAvatarNameCache::get(friend_id, &av_name))
    {
        mChangedSignal(UPDATED_MEMBERS);
    }
    else
    {
        if (auto it = mAvatarNameCacheConnections.find(friend_id); it != mAvatarNameCacheConnections.end())
        {
            if (it->second.connected())
            {
                it->second.disconnect();
            }
            mAvatarNameCacheConnections.erase(it);
        }

        mAvatarNameCacheConnections.try_emplace(friend_id, LLAvatarNameCache::get(friend_id, boost::bind(&LGGContactSets::onAvatarNameCache, this, _1)));
    }
    saveToDisk();
}

std::string LGGContactSets::getPseudonym(const LLUUID& friend_id) const
{
    if (auto found = mPseudonyms.find(friend_id); found != mPseudonyms.end())
    {
        return llformat("'%s'", found->second.c_str());
    }
    return std::string();
}

void LGGContactSets::clearPseudonym(const LLUUID& friend_id, bool save_changes /*= true*/)
{
    if (auto found = mPseudonyms.find(friend_id); found != mPseudonyms.end())
    {
        mPseudonyms.erase(found);
        LLAvatarNameCache* inst = LLAvatarNameCache::getInstance();
        inst->erase(friend_id);
        inst->fetch(friend_id); // update
        LLVOAvatar::invalidateNameTag(friend_id);
        if (!LLAvatarTracker::instance().isBuddy(friend_id) && getFriendSets(friend_id).size() == 0)
        {
            removeNonFriendFromList(friend_id, save_changes);
        }

        if (LLAvatarName av_name; LLAvatarNameCache::get(friend_id, &av_name))
        {
            mChangedSignal(UPDATED_MEMBERS);
        }
        else
        {
            if (auto it = mAvatarNameCacheConnections.find(friend_id); it != mAvatarNameCacheConnections.end())
            {
                if (it->second.connected())
                {
                    it->second.disconnect();
                }
                mAvatarNameCacheConnections.erase(it);
            }

            mAvatarNameCacheConnections.try_emplace(friend_id,  LLAvatarNameCache::get(friend_id, boost::bind(&LGGContactSets::onAvatarNameCache, this, _1)));
        }

        if (save_changes)
        {
            saveToDisk();
        }
    }
}

void LGGContactSets::onAvatarNameCache(const LLUUID& av_id)
{
    if (auto it = mAvatarNameCacheConnections.find(av_id); it != mAvatarNameCacheConnections.end())
    {
        if (it->second.connected())
        {
            it->second.disconnect();
        }
        mAvatarNameCacheConnections.erase(it);
    }
    mChangedSignal(UPDATED_MEMBERS);
}

bool LGGContactSets::hasPseudonym(const LLUUID& friend_id) const
{
    return (!getPseudonym(friend_id).empty());
}

bool LGGContactSets::hasPseudonym(const uuid_vec_t& ids) const
{
    bool has_pseudonym = false;
    for (const auto& id : ids)
    {
        if (hasPseudonym(id))
        {
            has_pseudonym = true;
            break;
        }
    }
    return has_pseudonym;
}

bool LGGContactSets::hasDisplayNameRemoved(const LLUUID& friend_id) const
{
    return (getPseudonym(friend_id) == CS_PSEUDONYM_QUOTED);
}

bool LGGContactSets::hasDisplayNameRemoved(const uuid_vec_t& ids) const
{
    bool has_pseudonym = false;
    for (const auto& id : ids)
    {
        if (hasDisplayNameRemoved(id))
        {
            has_pseudonym = true;
            break;
        }
    }
    return has_pseudonym;
}

bool LGGContactSets::hasVisuallyDifferentPseudonym(const LLUUID& friend_id) const
{
    return (hasPseudonym(friend_id) && (!hasDisplayNameRemoved(friend_id)));
}

void LGGContactSets::removeDisplayName(const LLUUID& friend_id)
{
    setPseudonym(friend_id, CS_PSEUDONYM);
}

void LGGContactSets::removeFriendFromSet(const LLUUID& friend_id, std::string_view set_name, bool save_changes /*= true*/)
{
    if (set_name == CS_SET_EXTRA_AVS)
    {
        return removeNonFriendFromList(friend_id, save_changes);
    }
    else if (set_name == CS_SET_PSEUDONYM)
    {
        return clearPseudonym(friend_id, save_changes);
    }

    if (ContactSet* set = getContactSet(set_name); set)
    {
        set->mFriends.erase(friend_id);
        if (save_changes)
        {
            saveToDisk();
            mChangedSignal(UPDATED_MEMBERS);
        }
    }
}

bool LGGContactSets::isValidSet(std::string_view set_name) const
{
    return (mContactSets.find(set_name.data()) != mContactSets.end());
}

void LGGContactSets::addSet(std::string_view set_name)
{
    if (!isInternalSetName(set_name) && !isValidSet(set_name))
    {
        ContactSet* set = new ContactSet();
        set->mName = set_name;
        set->mColor = LLColor4::red;
        set->mNotify = false;
        mContactSets[set_name.data()] = set;
        saveToDisk();
        mChangedSignal(UPDATED_LISTS);
    }
}

bool LGGContactSets::renameSet(std::string_view set_name, std::string_view new_set_name)
{
    if (!isInternalSetName(set_name) && isValidSet(set_name) &&
        !isInternalSetName(new_set_name) && !isValidSet(new_set_name))
    {
        ContactSet* set = getContactSet(set_name);
        set->mName = new_set_name;
        mContactSets.erase(set_name.data());
        mContactSets[new_set_name.data()] = set;
        saveToDisk();
        mChangedSignal(UPDATED_LISTS);
        return true;
    }
    return false;
}

void LGGContactSets::removeSet(std::string_view set_name)
{
    contact_set_map_t::iterator found = mContactSets.find(set_name.data());
    if (found != mContactSets.end())
    {
        LLAvatarTracker& tracker = LLAvatarTracker::instance();
        uuid_vec_t non_friends_for_removal;
        ContactSet* cset = found->second;
        for (const auto& friend_id : cset->mFriends)
        {
            if (!tracker.isBuddy(friend_id) &&
                getFriendSets(friend_id).size() == 1 && // Current set is only set!
                !hasPseudonym(friend_id))
            {
                non_friends_for_removal.emplace_back(friend_id);
            }
        }

        for (const auto& non_friend_id : non_friends_for_removal)
        {
            removeNonFriendFromList(non_friend_id, false);
        }

        delete found->second;
        mContactSets.erase(found);
        saveToDisk();
        mChangedSignal(UPDATED_LISTS);
    }
}

void LGGContactSets::setNotifyForSet(std::string_view set_name, bool notify)
{
    ContactSet* set = getContactSet(set_name);
    if (set)
    {
        set->mNotify = notify;
        saveToDisk();
    }
}

bool LGGContactSets::getNotifyForSet(std::string_view set_name) const
{
    if (ContactSet* set = getContactSet(set_name); set)
    {
        return set->mNotify;
    }
    return false;
}

void LGGContactSets::setSetColor(std::string_view set_name, const LLColor4& color)
{
    if (ContactSet* set = getContactSet(set_name); set)
    {
        set->mColor = color;
        saveToDisk();
    }
}

bool LGGContactSets::isInternalSetName(std::string_view set_name) const
{
    return (set_name.empty() ||
            set_name == CS_SET_EXTRA_AVS ||
            set_name == CS_SET_PSEUDONYM ||
            set_name == CS_SET_NO_SETS ||
            set_name == CS_SET_ALL_SETS ||
            set_name == CS_GLOBAL_SETTINGS);
}

LGGContactSets::ContactSet* LGGContactSets::getContactSet(std::string_view set_name) const
{
    if (set_name.empty())
    {
        LL_WARNS("ContactSets") << "No contact set specified" << LL_ENDL;
        return nullptr;
    }

    if (auto found = mContactSets.find(set_name.data()); found != mContactSets.end())
    {
        return found->second;
    }
    LL_WARNS("ContactSets") << "No contact set named " << set_name << LL_ENDL;
    return nullptr;
}

bool LGGContactSets::checkCustomName(const LLUUID& id, bool& dn_removed, std::string& pseudonym) const
{
    dn_removed = hasDisplayNameRemoved(id);
    pseudonym = getPseudonym(id);
    return hasPseudonym(id);
}

// static
bool LGGContactSets::handleAddContactSetCallback(const LLSD& notification, const LLSD& response)
{
    if (S32 option = LLNotificationsUtil::getSelectedOption(notification, response); option == 0)
    {
        const std::string set_name = response["message"].asString();
        LGGContactSets::getInstance()->addSet(set_name);
    }
    return false;
}

// static
bool LGGContactSets::handleRemoveContactSetCallback(const LLSD& notification, const LLSD& response)
{
    if (S32 option = LLNotificationsUtil::getSelectedOption(notification, response); option == 0)
    {
        LGGContactSets::getInstance()->removeSet(notification["payload"]["contact_set"].asString());
    }
    return false;
}

// static
bool LGGContactSets::handleRemoveAvatarFromSetCallback(const LLSD& notification, const LLSD& response)
{
    if (S32 option = LLNotificationsUtil::getSelectedOption(notification, response); option == 0)
    {
        LGGContactSets& instance = LGGContactSets::instance();
        LLAvatarTracker& tracker = LLAvatarTracker::instance();

        const std::string set_name = notification["payload"]["contact_set"].asString();
        for (const auto& item : llsd::inArray(notification["payload"]["ids"]))
        {
            const LLUUID& id = item.asUUID();

            instance.removeFriendFromSet(id, set_name, false);

            if (!tracker.isBuddy(id) &&
                instance.getFriendSets(id).empty() &&
                !instance.hasPseudonym(id))
            {
                instance.removeNonFriendFromList(id, false);
            }
        }

        instance.saveToDisk();
        instance.mChangedSignal(UPDATED_MEMBERS);
    }
    return false;
}

// static
bool LGGContactSets::handleSetAvatarPseudonymCallback(const LLSD& notification, const LLSD& response)
{
    if (S32 option = LLNotificationsUtil::getSelectedOption(notification, response); option == 0)
    {
        const std::string pseudonym(response["message"].asString());
        const LLUUID id(notification["payload"]["id"].asUUID());
        LGGContactSets::getInstance()->setPseudonym(id, pseudonym);
    }
    return false;
}
