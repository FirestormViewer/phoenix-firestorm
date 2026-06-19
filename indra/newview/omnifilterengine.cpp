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
    , mCurrentSelectedRuleSet("Default")  // <FS:minerjr> [FIRE-36763] - Add rule sets to Omnifilter
{
    mEventTimer.stop();
}

OmnifilterEngine::~OmnifilterEngine()
{
    // <FS:minerjr> [FIRE-36763] - Add rule sets to Omnifilter
    // If the user changed a setting and quickly closed the viewer, the changes would not be saved, but
    // the changes to the settings.xml would, which would cause issues. So save upon close just in case.
    saveNeedles();
    // </FS:minerjr> [FIRE-36763]
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

// <FS:minerjr> [FIRE-36763] - Add rule sets to Omnifilter
// Get the name from the vector of ordered rule sets at the specified index
std::string_view OmnifilterEngine::getOrderedRuleSetName(const S32 index) const
{
    // If the index is within the range of the vector, return the stored value
    if (index >= 0 && index < mOrderedRuleSets.size() && mOrderedRuleSets.size() > 0)
    {
        return mOrderedRuleSets[index];
    }

    // Return an empty string if not found.
    return "";
}

// Gets the ordered rule set index by looking up the name of a rule set.
S32 OmnifilterEngine::getOrderedRuleSetIndex(std::string_view lookup_name)
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

// Parses a passed in LLSD as a Map of rule sets, which contain another map, which contains the actual rule set data and an order value
// return false if the current LLSD is an older format, return true when finish importing the data.
bool OmnifilterEngine::importFromLLSD(const LLSD& needle_rule_sets_llsd)
{
    std::string preset_name = "Default";
    // If there is no default Rule Set return false
    if (!needle_rule_sets_llsd.has("Default"))
    {
        return false;
    }
    // Else there is a map named default, does it contain a variable of "order", if not, return false
    else if (!needle_rule_sets_llsd["Default"].has("order"))
    {
        return false;
    }
    mOrderedRuleSets.clear();
    // Use the number of rule sets to pre-allocate the ordered rule sets. So can use assignment operators without re-allocating.
    mOrderedRuleSets.resize(needle_rule_sets_llsd.size());
    // Loop over the outer Rule Set map
    S32 rule_set_index = 0;
    for (const auto&[new_rule_set_name, new_rule_set_llsd] : llsd::inMap(needle_rule_sets_llsd))
    {
        
        // If there is an order tag, then 
        if (new_rule_set_llsd.has("order"))
        {
            // Add the rule set name to the ordered list at the order position.
            mOrderedRuleSets[new_rule_set_llsd["order"].asInteger()] = new_rule_set_name;
        }
        else
        {
            // Else if there is no order tag, then add add the rule set name to the ordered list in the order read in.
            // NOTE maps don't store data order added, but optimized order for storage.
            mOrderedRuleSets[rule_set_index++] = new_rule_set_name;
        }
        // Generate a new rule set
        rule_set_t new_rule_set = rule_set_t(needle_ordered_list_t(), needle_list_t());
        // Apply the size of the rule set (number of rules) to the ordered rule set set.
        // First = Ordered Rule Set Vector, Second = Actual Rule Set Map
        new_rule_set.first.resize(new_rule_set_llsd["rule_set"].size());
        // Set the current needles_llsd to the actual rule set stored.
        // keeps old code valid.
        LLSD needles_llsd = new_rule_set_llsd["rule_set"];
        S32 index = 0;
        // Now loop over the rule map stored in the rule set.
        for (const auto& [new_needle_name, needle_data] : llsd::inMap(needles_llsd))
        {
            Needle new_needle;
            // Perform checks on all input values from the data format and skip any that don't exist.
            if (needle_data.has("sender_name"))
                new_needle.mSenderName = needle_data["sender_name"].asString();
            if (needle_data.has("content"))
                new_needle.mContent = needle_data["content"].asString();
            if (needle_data.has("region_name"))
                new_needle.mRegionName = needle_data["region_name"].asString();
            if (needle_data.has("chat_replace"))
                new_needle.mChatReplace = needle_data["chat_replace"].asString();
            if (needle_data.has("button_reply"))
                new_needle.mButtonReply = needle_data["button_reply"].asString();
            if (needle_data.has("textbox_reply"))
                new_needle.mTextBoxReply = needle_data["textbox_reply"].asString();
            if (needle_data.has("sender_name_match_type"))
                new_needle.mSenderNameMatchType = static_cast<OmnifilterEngine::eMatchType>(needle_data["sender_name_match_type"].asInteger());
            if (needle_data.has("content_match_type"))
                new_needle.mContentMatchType = static_cast<OmnifilterEngine::eMatchType>(needle_data["content_match_type"].asInteger());

            if (needle_data.has("types"))
            {
                LLSD types_llsd = needle_data["types"];

                for (const auto& needle_type : llsd::inArray(types_llsd))
                {
                    new_needle.mTypes.insert(static_cast<OmnifilterEngine::eType>(needle_type.asInteger()));
                }
            }
            if (needle_data.has("enabled"))
                new_needle.mEnabled = needle_data["enabled"].asBoolean();
            if (needle_data.has("sender_name_case_insensitive"))
                new_needle.mSenderNameCaseInsensitive = needle_data["sender_name_case_insensitive"].asBoolean();
            if (needle_data.has("content_case_insensitive"))
                new_needle.mContentCaseInsensitive = needle_data["content_case_insensitive"].asBoolean();

            if (needle_data.has("enabled"))
            {
                const std::string owner_id_str = needle_data["owner_id"].asString();
                if (!owner_id_str.empty())
                {
                    new_needle.mOwnerID.set(owner_id_str);
                }
            }
            // Assign the actuall new rule set to the parsed needle object
            new_rule_set.second[new_needle_name] = new_needle;
            // Add the loaded needle name to the ordered needle list
            // Needles are stored in order added to the map originally so use the
            // order value stored to restore the order back to the user
            // defined order.
            if (needle_data.has("order"))
            {
                new_rule_set.first[needle_data["order"].asInteger()] = new_needle_name;
            }
            else
            {
                new_rule_set.first[index++] = new_needle_name;
            }
        }
        mNeedleRuleSets[new_rule_set_name] = new_rule_set;
    }

    return true;
}

// Exports the current stored orered rules sets to an LLSD object and returns the object created.
LLSD OmnifilterEngine::exportToLLSD()
{
    LLSD output;
    // Loop over all the rulesets
    S32 rule_set_order = 0;
    // Loop over the ordered rule set names
    for (const auto& export_rule_set_name : mOrderedRuleSets)
    {
        LLSD needles_llsd;
        // Get the map of needles that have the name of the current ordered rule set
        const auto& export_rule_set = mNeedleRuleSets[export_rule_set_name];

        // <FS:minerjr> [FIRE-36649] - Add reordering to OmniFilter
        // Use ordered needle list to get the names of the needles in specified order and not the order added to the map.
        // for (const auto& [needle_name, needle] : mNeedles)
        S32 order = 0;
        for (const auto& needle_name : export_rule_set.first)
        {
            const Needle &needle = export_rule_set.second.at(needle_name);
            // Store the order of the needle
            needles_llsd[needle_name]["order"] = order++;
            // </FS:minerjr> [FIRE-36649]
            needles_llsd[needle_name]["sender_name"]            = needle.mSenderName;
            needles_llsd[needle_name]["content"]                = needle.mContent;
            needles_llsd[needle_name]["region_name"]            = needle.mRegionName;
            needles_llsd[needle_name]["chat_replace"]           = needle.mChatReplace;
            needles_llsd[needle_name]["button_reply"]           = needle.mButtonReply;
            needles_llsd[needle_name]["textbox_reply"]          = needle.mTextBoxReply;
            needles_llsd[needle_name]["sender_name_match_type"] = needle.mSenderNameMatchType;
            needles_llsd[needle_name]["content_match_type"]     = needle.mContentMatchType;
            needles_llsd[needle_name]["owner_id"]               = needle.mOwnerID;

            LLSD types_llsd;
            for (auto type : needle.mTypes)
            {
                types_llsd.append(static_cast<S32>(type));
            }

            needles_llsd[needle_name]["types"]                        = types_llsd;
            needles_llsd[needle_name]["enabled"]                      = needle.mEnabled;
            needles_llsd[needle_name]["sender_name_case_insensitive"] = needle.mSenderNameCaseInsensitive;
            needles_llsd[needle_name]["content_case_insensitive"]     = needle.mContentCaseInsensitive;
        }
        // Store the rule set into the otuput with the rule set store in the LLSD as well as the rule set order value
        output[export_rule_set_name]["rule_set"] = needles_llsd;
        output[export_rule_set_name]["order"] = rule_set_order++;
    }

    // Return the final LLSD which contains all the Rule Sets
    return output;
}

// Assignes a rule set to either fron the internal variable to the stored map of rule sets, or
// copies the current selected rule set from storage to the internal variables.
// This lets us keep all the original design and not require refactoring of code
// in other classes that interact with the Omnifilter.
bool OmnifilterEngine::assignRuleSet(const bool rule_set_to_internal)
{
    // If there is no current selected rule set or the rule set name is not in the list of rule sets, return false
    if (mCurrentSelectedRuleSet.empty() || !mNeedleRuleSets.contains(mCurrentSelectedRuleSet))
    {
        return false;
    }

    // If transfering data from the currently selected rule set to the internal variables
    if (rule_set_to_internal)
    {
        // Assign the needle map and vector of ordered neele names from the currently selected rule set.
        mNeedles = mNeedleRuleSets[mCurrentSelectedRuleSet].second;
        mOrderedNeedles = mNeedleRuleSets[mCurrentSelectedRuleSet].first;
    }
    // Else, want to move the data back the from the internal variables to selected data values
    else
    {
        mNeedleRuleSets[mCurrentSelectedRuleSet].second = mNeedles;
        mNeedleRuleSets[mCurrentSelectedRuleSet].first = mOrderedNeedles;
    }
    return true;
}

// Assigns the current selected rule set string to the value passed in
bool OmnifilterEngine::setCurrentRuleSet(std::string_view rule_set_name)
{
    // If blank, return false as we should not have a blank name.
    if (rule_set_name.empty())
    {
        return false;
    }
    // Store the pass in rule set name.
    mCurrentSelectedRuleSet = rule_set_name;

    // Apply the rule set to the current variables
    assignRuleSet();

    return true;
}

// Performs the actual removal of the current rule set.
S32 OmnifilterEngine::removeCurrentRuleSet()
{
    // If the current rule set list contains the currently selceted rule set, then erase it.
    if (mNeedleRuleSets.contains(mCurrentSelectedRuleSet))
    {        
        S32 current_index = gSavedSettings.getS32("OmnifilterRuleSetID");
        // If the current index is greater then the
        if (current_index == 0)
        {
            // Return 0 for unable to remove default
            return 0;
        }

        // If the index is within range of the needle rule sets, then
        if (current_index < mNeedleRuleSets.size() && current_index > 0)
        {
            // Erase both the the current rule set and the ordered rule set name
            mNeedleRuleSets.erase(mCurrentSelectedRuleSet);
            mOrderedRuleSets.erase(mOrderedRuleSets.begin() + current_index);
            // Reset the selected rule set ID back to 0, let the user pick the one to use next. 0 is also safe as it should never be removed.
            gSavedSettings.setS32("OmnifilterRuleSetID", 0);
            // Re-assign the current rule set name and reload the rule set from storage to the rule set
            assignRuleSetNameFromSettings();
            assignRuleSet();
            // Return 1 indicating that the removal worked correctly.
            return 1;
        }
        // Return -1 as the rule set index out of bounds
        return 0;
    }
    // Return -2 for rule set name does not exist
    return -2;
}

// Add a new rule set based upon the name passed in
S32 OmnifilterEngine::addNewRuleSet(std::string_view new_name)
{
    // If the name is blank, return an error code, to then let the user know with a notification
    if (new_name.empty())
    {
        return -1;
    }

    std::string str_new_name(new_name);
    // If the name already exists, return an error to let the user know
    if (mNeedleRuleSets.contains(str_new_name))
    {
        return 0;
    }

    // We want to clear the current rule set and ordered rules as a new rule set has only a single template rule that is disabled.
    mNeedles.clear();
    mOrderedNeedles.clear();
    // Set the rule set in the map to a new rule set pair.
    mNeedleRuleSets[str_new_name] = rule_set_t(needle_ordered_list_t(), needle_list_t());
    // Update the OmnifilterRuleSetID saved setting to store the location of the new rule set.
    // We want to do this to support auto-selecting the new rule set.
    gSavedSettings.setS32("OmnifilterRuleSetID", static_cast<S32>(mNeedleRuleSets.size() - 1));
    // Set the current selcted rule set to the new rule set
    mCurrentSelectedRuleSet = str_new_name;
    // Add the rule set name to the list of ordered rule sets
    mOrderedRuleSets.push_back(mCurrentSelectedRuleSet);

    return 1;
}

// Clone the current rule set based upon the name passed in
S32 OmnifilterEngine::addClonedRuleSet(std::string_view new_name)
{
    // If the name is blank, return an error code, to then let the user know with a notification
    if (new_name.empty())
    {
        return -1;
    }

    std::string str_new_name(new_name);

    // If the name already exists, return an error to let the user know
    if (mNeedleRuleSets.contains(str_new_name))
    {
        return 0;
    }

    // This method is different in the new rule set method by not clearing the current rules and ordered rules.

    // Set the rule set in the map to a new rule set pair.
    mNeedleRuleSets[str_new_name] = rule_set_t(needle_ordered_list_t(), needle_list_t());
    // Update the OmnifilterRuleSetID saved setting to store the location of the new rule set.
    // We want to do this to support auto-selecting the new rule set.
    gSavedSettings.setS32("OmnifilterRuleSetID", static_cast<S32>(mNeedleRuleSets.size() - 1));
    // Set the current selcted rule set to the new rule set
    mCurrentSelectedRuleSet = str_new_name;
    // Add the rule set name to the list of ordered rule sets
    mOrderedRuleSets.push_back(mCurrentSelectedRuleSet);

    // Copy the internal rule set to the stored rulset
    assignRuleSet(false);

    return 1;
}

// Takes the stored setting OmnifilterRuleSetID and assigns the current
// selected rule set to the ordered rule set at that location.
bool OmnifilterEngine::assignRuleSetNameFromSettings()
{
    // Get hte number of items to loop over
    S32 rule_set_id = gSavedSettings.getS32("OmnifilterRuleSetID");
    std::string found_rule_set(getOrderedRuleSetName(rule_set_id));

    if (found_rule_set.empty())
    {
        return false;
    }

    mCurrentSelectedRuleSet = found_rule_set;

    return true;
}
// </FS:minerjr> [FIRE-36763]

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

    // <FS:minerjr> [FIRE-36763] - Add rule sets to Omnifilter
    static LLCachedControl<S32> NeedlePresetIndex(gSavedSettings, "OmnifilterRuleSetID");

    // If this is not the first time loading, based upon the preset index being set
    // to an invalid valid, after this there should always be atleast 1 rule set (default)
    if (NeedlePresetIndex > -1)
    {
        mNeedleRuleSets.clear();
        // If the importing works out correctly, then use the reloaded rulset set name from the settings and assign that rule set.
        if (importFromLLSD(needles_llsd))
        {
            assignRuleSetNameFromSettings();
            // Assign the rule set
            assignRuleSet();
            // Return as we can skip the rest of the old loading.
            return;
        }
        // Otherwise, the import failed as it may be an existing older rule set that has a vector of ordered rules.
        // If so, we still want to use the older load code to parse the older form and convert to the newer format.
    }
    // Load the file with the old code to get the old .xml file data loaded, to then be outputted in the new format
    // <F/S:minerjr> [FIRE-36763]
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
    // <FS:minerjr> [FIRE-36649] - Add reordering to OmniFilter
    // Create a default from the current version
    gSavedSettings.setS32("OmnifilterRuleSetID", 0);
    // The first rule set is always called Default.
    mCurrentSelectedRuleSet = "Default";
    // Create a new rule set with the already parsed ordered ruules and map of rules.
    mNeedleRuleSets["Default"] = rule_set_t(mOrderedNeedles, mNeedles);
    mOrderedRuleSets.push_back(mCurrentSelectedRuleSet);
    // Save the ruleset to the storage rule set.
    assignRuleSet(false);
    // Save the changes back to the Omnifilter.xml file.
    saveNeedles();
    // </FS:minerjr> [FIRE-36649]
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

    // <FS:minerjr> [FIRE-36763] - Add rule sets to Omnifilter
    // First assign the current rule set back to the currently selected rule set.
    assignRuleSet(false);
    // Export the needles to LLSD storage
    needles_llsd = exportToLLSD();
    /*
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
    */
    // </FS:minerjr> [FIRE-36763]

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
