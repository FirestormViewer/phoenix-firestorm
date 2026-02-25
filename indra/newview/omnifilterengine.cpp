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
    for (const auto& [needle_name, needle]: mNeedles)
    {
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
    return mNeedles[needle_name];
}

void OmnifilterEngine::renameNeedle(const std::string& old_name, const std::string& new_name)
{
    // https://stackoverflow.com/a/44883472
    auto node_handler = mNeedles.extract(old_name);
    node_handler.key() = new_name;
    mNeedles.insert(std::move(node_handler));

    setDirty(true);
}

void OmnifilterEngine::deleteNeedle(const std::string& needle_name)
{
    mNeedles.erase(needle_name);
    setDirty(true);
}

OmnifilterEngine::needle_list_t& OmnifilterEngine::getNeedleList()
{
    return mNeedles;
}

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

    for (const auto& [needle_name, needle] : mNeedles)
    {
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
