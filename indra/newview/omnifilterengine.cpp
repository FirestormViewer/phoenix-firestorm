/**
 * @file omnifilterengine.cpp
 * @brief The core Omnifilter engine
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2025, Zi Ree @ Second Life
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
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "omnifilterengine.h"

#include "fsyspath.h"
#include "llnotificationsutil.h"
#include "llsdserialize.h"
#include "llviewercontrol.h"

#include <fstream>
#include <filesystem>

OmnifilterEngine::OmnifilterEngine()
    : LLSingleton<OmnifilterEngine>()
    , LLEventTimer(5.0f)
    , mDirty(false)
{
    mEventTimer.stop();
}

OmnifilterEngine::~OmnifilterEngine()
{
    // delete Xxx;
}

void OmnifilterEngine::init()
{
    mNeedlesXMLPath = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, "omnifilter.xml");
    if (mNeedlesXMLPath.empty())
    {
        LL_DEBUGS("Omnifilter") << "Got empty file name for omnifilter XML storage." << LL_ENDL;
    }

    loadNeedles();
}

const OmnifilterEngine::Needle* OmnifilterEngine::logMatch(const std::string& needle_name, const Needle& needle)
{
    time_t now = (time_t)LLDate::now().secondsSinceEpoch();
    mLog.emplace_back(now, needle_name);
    mLogSignal(now, needle_name);

    return &needle;
}

bool OmnifilterEngine::matchStrings(std::string_view needle_string, std::string_view haystack_string, eMatchType match_type, bool case_insensitive)
{
    static LLCachedControl<bool> use_omnifilter(gSavedSettings, "OmnifilterEnabled", false);
    if (!use_omnifilter)
    {
        return false;
    }

    std::string_view needle_content = needle_string;
    std::string_view haystack_content = haystack_string;

    std::string needle_lc_content;
    std::string haystack_lc_content;
    if (case_insensitive)
    {
        needle_lc_content = needle_string;
        haystack_lc_content = haystack_string;

        LLStringUtil::toLower(needle_lc_content);
        LLStringUtil::toLower(haystack_lc_content);
        needle_content = needle_lc_content;
        haystack_content = haystack_lc_content;
    }

    switch (match_type)
    {
        case eMatchType::Regex:
        {
            boost::regbase::flag_type re_flags = boost::regex::normal;
            if (case_insensitive)
            {
                re_flags |= boost::regex::icase;
            }
            boost::regex re(std::string(needle_string), re_flags);
            if (boost::regex_match(std::string(haystack_string), re))
            {
                return true;
            }
            break;
        }
        case eMatchType::Substring:
        {
            if (haystack_content.find(needle_content) != haystack_content.npos)
            {
                return true;
            }
            break;
        }
        case eMatchType::Exact:
        {
            if (haystack_content == needle_content)
            {
                return true;
            }
            break;
        }
        default:
        {
            LL_DEBUGS("Omnifilter") << "match type " << match_type << " unknown!" << LL_ENDL;
            break;
        }
    }
    return false;
}

const OmnifilterEngine::Needle* OmnifilterEngine::match(const Haystack& haystack)
{
    // <FS:minerjr> [FIRE-36649] - Add reordering to OmniFilter
    // Use ordered needle list to get the names of the needles in specified order and not the order added to the map.
    for (const auto& needle_name : mOrderedNeedles)
    //for (const auto& [needle_name, needle]: mNeedles)
    {
        const auto& needle = mNeedles[needle_name];
    // </FS:minerjr> [FIRE-36649]
        if (!needle.mEnabled)
        {
            continue;
        }

        if (!needle.mSenderName.empty())
        {
            if (!matchStrings(needle.mSenderName, haystack.mSenderName, needle.mSenderNameMatchType, needle.mSenderNameCaseInsensitive))
            {
                continue;
            }
        }

        if (needle.mOwnerID.notNull())
        {
            if (needle.mOwnerID != haystack.mOwnerID)
            {
                continue;
            }
        }

        if (!needle.mRegionName.empty())
        {
            if (needle.mRegionName != haystack.mRegionName)
            {
                continue;
            }
        }

        if (!needle.mTypes.empty() && !needle.mTypes.contains(haystack.mType))
        {
            continue;
        }

        if (needle.mContent.empty() || matchStrings(needle.mContent, haystack.mContent, needle.mContentMatchType, needle.mContentCaseInsensitive))
        {
            return logMatch(needle_name, needle);
        }
    }

    return nullptr;
}

OmnifilterEngine::Needle& OmnifilterEngine::newNeedle(const std::string& needle_name)
{
    if (!mNeedles.contains(needle_name))
    {
        Needle new_needle;
        new_needle.mEnabled = false;
        new_needle.mSenderNameCaseInsensitive = true;
        new_needle.mSenderNameMatchType = eMatchType::Substring;
        new_needle.mContentCaseInsensitive = true;
        new_needle.mContentMatchType = eMatchType::Substring;

        mNeedles[needle_name] = new_needle;
    }
    setDirty(true);
    // <FS:minerjr> [FIRE-36649] - Add reordering to OmniFilter
    // Add to the ordered needle vector the name of the new needle
    mOrderedNeedles.push_back(needle_name);
    // <F/S:minerjr> [FIRE-36649]
    return mNeedles[needle_name];
}

void OmnifilterEngine::renameNeedle(const std::string& old_name, const std::string& new_name)
{
    // https://stackoverflow.com/a/44883472
    auto node_handler = mNeedles.extract(old_name);
    node_handler.key() = new_name;
    mNeedles.insert(std::move(node_handler));
    // <FS:minerjr> [FIRE-36649] - Add reordering to OmniFilter
    // Find the index of the given old name
    S32 found_index = getOrderedNeedleIndex(old_name);
    // If the name was found (-1 when not found), set the ordered needle vector at the found index to the new value
    if (found_index >= 0)
        mOrderedNeedles[found_index] = new_name;
    // </FS:minerjr> [FIRE-36649]

    setDirty(true);
}

void OmnifilterEngine::deleteNeedle(const std::string& needle_name)
{
    // <FS:minerjr> [FIRE-36649] - Add reordering to OmniFilter
    // Find the index of the given needle name
    S32 found_index = getOrderedNeedleIndex(needle_name);
    // If the name was found (-1 when not found), erase the need based upon the offset
    if (found_index >= 0)
        mOrderedNeedles.erase(mOrderedNeedles.begin() + found_index);
    // </FS:minerjr> [FIRE-36649]
    mNeedles.erase(needle_name);
    setDirty(true);
}

OmnifilterEngine::needle_list_t& OmnifilterEngine::getNeedleList()
{
    return mNeedles;
}

// <FS:minerjr> [FIRE-36649] - Add reordering to OmniFilter
// Get the name from the vector of ordered needles at the specified index
std::string_view OmnifilterEngine::getOrderedNeedleName(const S32 index) const
{
    // If the index is within the range of the vector, return the stored value
    if (index >= 0 && index < mOrderedNeedles.size() && mOrderedNeedles.size() > 0)
    {
        return mOrderedNeedles[index];
    }

    // Return an empty string if not found.
    return "";
}

// Get a needle at a specified index, returns nullptr if none is found ("")
const OmnifilterEngine::Needle* OmnifilterEngine::getOrderedNeedle(const S32 index)
{
    std::string_view found_needle = getOrderedNeedleName(index);
    if (found_needle.empty())
    {
        return nullptr;
    }

    return &mNeedles[std::string(found_needle)];
}

// Re-assigns a name to a specified needle location in the Ordered list
bool OmnifilterEngine::setOrderedNeedleName(const S32 needle_index, std::string_view new_name)
{
    // If the index is invalid, return false
    if (needle_index < 0 || needle_index > mOrderedNeedles.size())
    {
        return false;
    }

    // Otherwise assign the new name to the ordered needle at the specified index.
    mOrderedNeedles[needle_index] = new_name;

    return true;
}

S32 OmnifilterEngine::getOrderedNeedleIndex(std::string_view lookup_name)
{
    for (S32 index = 0; index < mOrderedNeedles.size(); index++)
    {
        if (mOrderedNeedles[index] == lookup_name)
        {
            return index;
        }
    }

    return -1;
}

// Swaps 2 ordered needles
bool OmnifilterEngine::swapNeedles(S32 index1, S32 index2)
{
    // Validation check to make sure the indexs are valid and if not to return false
    if (index1 < 0 || index1 > mOrderedNeedles.size() || index2 < 0 || index2 > mOrderedNeedles.size() || index1 == index2)
    {
        return false;
    }
    // Perform the actual swap
    std::swap(mOrderedNeedles[index1], mOrderedNeedles[index2]);
    // Force a view update
    setDirty(true);

    return true;
}
// </FS:minerjr> [FIRE-36649]

void OmnifilterEngine::setDirty(bool dirty)
{
    mDirty = dirty;

    if (mDirty)
    {
        mEventTimer.start();
    }
    else
    {
        mEventTimer.stop();
    }
}

void OmnifilterEngine::loadNeedles()
{
    if (mNeedlesXMLPath.empty())
    {
        return;
    }

    if (!std::filesystem::exists(fsyspath(mNeedlesXMLPath)))
    {
        // file does not exist (yet), just return empty
        return;
    }

    if (!std::filesystem::is_regular_file(fsyspath(mNeedlesXMLPath)))
    {
        LL_DEBUGS("Omnifilter") << "Omnifilter storage at '" << mNeedlesXMLPath << "' is not a regular file." << LL_ENDL;
        LLSD args;
        args["FILE_NAME"] = mNeedlesXMLPath;
        LLNotificationsUtil::add("NotRegularFileError", args, LLSD());
        return;
    }

    if (std::filesystem::file_size(fsyspath(mNeedlesXMLPath)) == 0)
    {
        // file exists but is empty, this should not happen, so alert the user, they might have lost their needles
        LL_DEBUGS("Omnifilter") << "Omnifilter storage file is empty." << mNeedlesXMLPath << LL_ENDL;
        LLSD args;
        args["ERROR_CODE"] = errno;
        args["ERROR_MESSAGE"] = strerror(errno);
        args["FILE_NAME"] = mNeedlesXMLPath;
        LLNotificationsUtil::add("GenericFileEmptyError", args, LLSD());
        return;
    }

    LL_DEBUGS("Omnifilter") << "Loading needles from: " << mNeedlesXMLPath << LL_ENDL;

    llifstream file(mNeedlesXMLPath.c_str());
    if (file.fail())
    {
        LL_DEBUGS("Omnifilter") << "Unable to open Omnifilter storage at '" << mNeedlesXMLPath << "' for reading." << LL_ENDL;
        LLSD args;
        args["ERROR_CODE"] = errno;
        args["ERROR_MESSAGE"] = strerror(errno);
        args["FILE_NAME"] = mNeedlesXMLPath;
        LLNotificationsUtil::add("GenericFileOpenReadError", args, LLSD());
        return;
    }

    LLSD needles_llsd;
    if (file.is_open())
    {
        LLSDSerialize::fromXML(needles_llsd, file);
        file.close();
    }

    if (file.fail())
    {
        LL_DEBUGS("Omnifilter") << "Unable to read Omnifilter needles from '" << mNeedlesXMLPath << "'." << LL_ENDL;
        LLSD args;
        args["ERROR_CODE"] = errno;
        args["ERROR_MESSAGE"] = strerror(errno);
        args["FILE_NAME"] = mNeedlesXMLPath;
        LLNotificationsUtil::add("GenericFileReadError", args, LLSD());
        return;
    }

    // <FS:minerjr> [FIRE-36649] - Add reordering to OmniFilter
    // Clear the vector of filters
    mOrderedNeedles.clear();
    // Pre-allocate space for the list of needle names, so we can use an index into it for assignments down below
    mOrderedNeedles.resize(needles_llsd.size());
    S32 index = 0;
    // </FS:minerjr> [FIRE-36649]
    for (const auto& [new_needle_name, needle_data] : llsd::inMap(needles_llsd))
    {
        Needle new_needle;
        new_needle.mSenderName = needle_data["sender_name"].asString();
        new_needle.mContent = needle_data["content"].asString();
        new_needle.mRegionName = needle_data["region_name"].asString();
        new_needle.mChatReplace = needle_data["chat_replace"].asString();
        new_needle.mButtonReply = needle_data["button_reply"].asString();
        new_needle.mTextBoxReply = needle_data["textbox_reply"].asString();
        new_needle.mSenderNameMatchType = static_cast<OmnifilterEngine::eMatchType>(needle_data["sender_name_match_type"].asInteger());
        new_needle.mContentMatchType = static_cast<OmnifilterEngine::eMatchType>(needle_data["content_match_type"].asInteger());

        LLSD types_llsd = needle_data["types"];

        for (const auto& needle_type : llsd::inArray(types_llsd))
        {
            new_needle.mTypes.insert(static_cast<OmnifilterEngine::eType>(needle_type.asInteger()));
        }

        new_needle.mEnabled = needle_data["enabled"].asBoolean();
        new_needle.mSenderNameCaseInsensitive = needle_data["sender_name_case_insensitive"].asBoolean();
        new_needle.mContentCaseInsensitive = needle_data["content_case_insensitive"].asBoolean();

        const std::string owner_id_str = needle_data["owner_id"].asString();
        if (!owner_id_str.empty())
        {
            new_needle.mOwnerID.set(owner_id_str);
        }

        mNeedles[new_needle_name] = new_needle;
        // <FS:minerjr> [FIRE-36649] - Add reordering to OmniFilter
        // Add the loaded needle name to the ordered needle list
        // Needles are stored in order added to the map originally so use the
        // order value stored to restore the order back to the user
        // defined order.
        if (needle_data.has("order"))
        {
            mOrderedNeedles[needle_data["order"].asInteger()] = new_needle_name;
        }
        else
        {

            mOrderedNeedles[index++] = new_needle_name;
        }
        // <FS:minerjr> [/FIRE-36649]
    }
}

void OmnifilterEngine::saveNeedles()
{
    if (mNeedlesXMLPath.empty())
    {
        return;
    }

    LL_DEBUGS("Omnifilter") << "Saving needles to: " << mNeedlesXMLPath << LL_ENDL;

    llofstream file(mNeedlesXMLPath.c_str());
    if (file.fail())
    {
        LL_DEBUGS("Omnifilter") << "Unable to open Omnifilter storage at '" << mNeedlesXMLPath << "' for writing." << LL_ENDL;
        LLSD args;
        args["ERROR_CODE"] = errno;
        args["ERROR_MESSAGE"] = strerror(errno);
        args["FILE_NAME"] = mNeedlesXMLPath;
        LLNotificationsUtil::add("GenericFileOpenWriteError", args, LLSD());
        return;
    }

    LLSD needles_llsd;

    // <FS:minerjr> [FIRE-36649] - Add reordering to OmniFilter
    // Use ordered needle list to get the names of the needles in specified order and not the order added to the map.
    //for (const auto& [needle_name, needle] : mNeedles)
    S32 order = 0;
    for (const auto& needle_name : mOrderedNeedles)
    {
        const auto& needle = mNeedles[needle_name];
        // Store the order of the needle
        needles_llsd[needle_name]["order"] = order++;
    // </FS:minerjr> [FIRE-36649]
        needles_llsd[needle_name]["sender_name"] = needle.mSenderName;
        needles_llsd[needle_name]["content"] = needle.mContent;
        needles_llsd[needle_name]["region_name"] = needle.mRegionName;
        needles_llsd[needle_name]["chat_replace"] = needle.mChatReplace;
        needles_llsd[needle_name]["button_reply"] = needle.mButtonReply;
        needles_llsd[needle_name]["textbox_reply"] = needle.mTextBoxReply;
        needles_llsd[needle_name]["sender_name_match_type"] = needle.mSenderNameMatchType;
        needles_llsd[needle_name]["content_match_type"] = needle.mContentMatchType;
        needles_llsd[needle_name]["owner_id"] = needle.mOwnerID;

        LLSD types_llsd;
        for (auto type : needle.mTypes)
        {
            types_llsd.append(static_cast<S32>(type));
        }

        needles_llsd[needle_name]["types"] = types_llsd;
        needles_llsd[needle_name]["enabled"] = needle.mEnabled;
        needles_llsd[needle_name]["sender_name_case_insensitive"] = needle.mSenderNameCaseInsensitive;
        needles_llsd[needle_name]["content_case_insensitive"] = needle.mContentCaseInsensitive;
    }

    LLSDSerialize::toXML(needles_llsd, file);

    file.close();

    if (file.fail())
    {
        LL_DEBUGS("Omnifilter") << "Unable to save Omnifilter needles at '" << mNeedlesXMLPath << "'." << LL_ENDL;
        LLSD args;
        args["ERROR_CODE"] = errno;
        args["ERROR_MESSAGE"] = strerror(errno);
        args["FILE_NAME"] = mNeedlesXMLPath;
        LLNotificationsUtil::add("GenericFileWriteError", args, LLSD());
    }

    setDirty(false);
}

// virtual
bool OmnifilterEngine::tick()
{
    saveNeedles();
    setDirty(false);
    return false;
}
