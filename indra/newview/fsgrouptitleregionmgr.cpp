/**
 * @file fsgrouptitleregionmgr.cpp
 * @brief Per-region automatic group title switching manager
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (c) 2026 The Phoenix Firestorm Project, Inc.
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
 *
 * The Phoenix Firestorm Project, Inc., 1831 Oakwood Drive, Fairmont, Minnesota 56031-3225 USA
 * http://www.firestormviewer.org
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "fsgrouptitleregionmgr.h"

#include "llagent.h"
#include "lldir.h"
#include "llgroupactions.h"
#include "llgroupmgr.h"
#include "llnotificationsutil.h"
#include "llsdserialize.h"
#include "llviewerregion.h"
#include "llworldmap.h"
#include "llworldmapmessage.h"
#include "rlvactions.h"

static constexpr char   REGION_GROUP_TITLES_FILE[] = "region_group_titles.xml";
static constexpr S32    MAX_REGION_NAME_LENGTH     = 63;
static constexpr F32    REGION_VALIDATION_TIMEOUT  = 10.f;

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

FSGroupTitleRegionMgr::FSGroupTitleRegionMgr()
{
    mRegionChangedConnection = gAgent.addRegionChangedCallback([this]() { onRegionChanged(); });
}

FSGroupTitleRegionMgr::~FSGroupTitleRegionMgr()
{
    cancelPendingValidation();

    if (mLoginRetryTimer)
    {
        delete mLoginRetryTimer;
        mLoginRetryTimer = nullptr;
    }

    if (mRegionChangedConnection.connected())
    {
        gAgent.removeRegionChangedCallback(mRegionChangedConnection);
        mRegionChangedConnection.disconnect();
    }
}

// ---------------------------------------------------------------------------
// Persistence
// ---------------------------------------------------------------------------

std::string FSGroupTitleRegionMgr::getFilename() const
{
    auto path = gDirUtilp->getExpandedFilename(LL_PATH_PER_SL_ACCOUNT, "");
    if (!path.empty())
    {
        path = gDirUtilp->getExpandedFilename(LL_PATH_PER_SL_ACCOUNT, REGION_GROUP_TITLES_FILE);
    }
    return path;
}

void FSGroupTitleRegionMgr::loadFromDisk()
{
    if (mLoginRetryTimer)
    {
        delete mLoginRetryTimer;
        mLoginRetryTimer = nullptr;
    }

    mAssignments.clear();
    mNoneOnUnassigned = false;
    mLastAppliedRegion.clear();

    const auto filename = getFilename();
    if (filename.empty())
    {
        LL_WARNS() << "Could not determine per-account path for region group titles" << LL_ENDL;
        mDataLoaded = true;
        return;
    }

    llifstream file(filename.c_str());
    if (!file.is_open())
    {
        LL_INFOS() << "No region group titles file found at: " << filename << LL_ENDL;
        mDataLoaded = true;
        return;
    }

    LLSD data;
    if (LLSDSerialize::fromXML(data, file) == LLSDParser::PARSE_FAILURE)
    {
        LL_WARNS() << "Failed to parse region group titles file: " << filename << LL_ENDL;
        mDataLoaded = true;
        return;
    }
    file.close();

    mNoneOnUnassigned = data["none_on_unassigned"].asBoolean();

    if (data.has("assignments") && data["assignments"].isMap())
    {
        const auto& assignments = data["assignments"];
        for (auto it = assignments.beginMap(); it != assignments.endMap(); ++it)
        {
            FSRegionTitleAssignment assignment;
            assignment.mGroupID     = it->second["group_id"].asUUID();
            assignment.mRoleID      = it->second["role_id"].asUUID();
            assignment.mDisplayName = it->second["display_name"].asString();
            if (assignment.mDisplayName.empty())
            {
                assignment.mDisplayName = it->first;
            }
            mAssignments[it->first] = assignment;
        }
    }

    mDataLoaded = true;
    LL_INFOS() << "Loaded " << mAssignments.size() << " region group title assignments" << LL_ENDL;

    // The region changed callback likely already fired during login before data was loaded (and was ignored via the mDataLoaded guard).
    // Try now and schedule a deferred retry in case the region handshake hasn't finished yet (region name would be empty, causing an early return).
    onRegionChanged();

    static constexpr F32 LOGIN_RETRY_DELAY = 5.f;
    mLoginRetryTimer = LLEventTimer::run_after(LOGIN_RETRY_DELAY, [this]()
    {
        mLoginRetryTimer = nullptr;
        mLastAppliedRegion.clear();
        onRegionChanged();
    });
}

void FSGroupTitleRegionMgr::saveToDisk()
{
    const auto filename = getFilename();
    if (filename.empty())
    {
        LL_WARNS() << "Could not determine per-account path for saving region group titles" << LL_ENDL;
        return;
    }

    LLSD assignments;
    for (const auto& [region, assignment] : mAssignments)
    {
        LLSD entry;
        entry["group_id"]     = assignment.mGroupID;
        entry["role_id"]      = assignment.mRoleID;
        entry["display_name"] = assignment.mDisplayName;
        assignments[region] = entry;
    }

    LLSD data;
    data["none_on_unassigned"] = mNoneOnUnassigned;
    data["assignments"]        = assignments;

    llofstream file(filename.c_str());
    if (!file.is_open())
    {
        LL_WARNS() << "Could not open file for writing: " << filename << LL_ENDL;
        return;
    }
    LLSDSerialize::toPrettyXML(data, file);
    file.close();
}

// ---------------------------------------------------------------------------
// Assignment CRUD
// ---------------------------------------------------------------------------

void FSGroupTitleRegionMgr::setAssignment(const LLUUID& group_id, const LLUUID& role_id, const std::string& region_name)
{
    const auto normalized = normalizeRegionName(region_name);
    if (normalized.empty())
    {
        return;
    }

    if (auto it = mAssignments.find(normalized); it != mAssignments.end())
    {
        if (it->second.mGroupID == group_id && it->second.mRoleID == role_id)
        {
            return;
        }
        mAssignments.erase(it);
    }

    mAssignments[normalized] = { group_id, role_id, sanitizeRegionName(region_name) };
    saveToDisk();
    mAssignmentsChangedSignal();
}

void FSGroupTitleRegionMgr::setAssignmentForCurrentRegion(const LLUUID& group_id, const LLUUID& role_id)
{
    const auto* region = gAgent.getRegion();
    if (!region)
    {
        return;
    }
    setAssignment(group_id, role_id, region->getName());
}

void FSGroupTitleRegionMgr::clearAssignment(const LLUUID& group_id, const LLUUID& role_id)
{
    bool changed = false;
    for (auto it = mAssignments.begin(); it != mAssignments.end(); )
    {
        if (it->second.mGroupID == group_id && it->second.mRoleID == role_id)
        {
            it = mAssignments.erase(it);
            changed = true;
        }
        else
        {
            ++it;
        }
    }
    if (changed)
    {
        saveToDisk();
        mAssignmentsChangedSignal();
    }
}

void FSGroupTitleRegionMgr::clearAssignmentByRegion(const std::string& region_name)
{
    if (auto it = mAssignments.find(normalizeRegionName(region_name)); it != mAssignments.end())
    {
        mAssignments.erase(it);
        saveToDisk();
        mAssignmentsChangedSignal();
    }
}

// ---------------------------------------------------------------------------
// Lookups
// ---------------------------------------------------------------------------

std::string FSGroupTitleRegionMgr::getRegionForTitle(const LLUUID& group_id, const LLUUID& role_id) const
{
    std::string result;
    for (const auto& [region, assignment] : mAssignments)
    {
        if (assignment.mGroupID == group_id && assignment.mRoleID == role_id)
        {
            if (!result.empty())
            {
                result += ", ";
            }
            result += assignment.mDisplayName;
        }
    }
    return result;
}

std::vector<std::string> FSGroupTitleRegionMgr::getRegionDisplayNamesForTitle(const LLUUID& group_id, const LLUUID& role_id) const
{
    std::vector<std::string> result;
    for (const auto& [region, assignment] : mAssignments)
    {
        if (assignment.mGroupID == group_id && assignment.mRoleID == role_id)
        {
            result.push_back(assignment.mDisplayName);
        }
    }
    return result;
}

bool FSGroupTitleRegionMgr::getAssignmentForRegion(const std::string& region_name, LLUUID& group_id, LLUUID& role_id) const
{
    if (auto it = mAssignments.find(normalizeRegionName(region_name)); it != mAssignments.end())
    {
        group_id = it->second.mGroupID;
        role_id  = it->second.mRoleID;
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Preferences
// ---------------------------------------------------------------------------

void FSGroupTitleRegionMgr::setNoneOnUnassigned(bool enabled)
{
    mNoneOnUnassigned = enabled;
    saveToDisk();
}

boost::signals2::connection FSGroupTitleRegionMgr::setAssignmentsChangedCallback(const assignments_changed_signal_t::slot_type& cb)
{
    return mAssignmentsChangedSignal.connect(cb);
}

// ---------------------------------------------------------------------------
// Input sanitization / normalization
// ---------------------------------------------------------------------------

std::string FSGroupTitleRegionMgr::sanitizeRegionName(const std::string& input)
{
    std::string result = input;
    LLStringUtil::trim(result);
    std::string collapsed;
    collapsed.reserve(result.size());
    bool prev_space = false;
    for (const auto c : result)
    {
        if (c == ' ')
        {
            if (!prev_space)
            {
                collapsed += c;
            }
            prev_space = true;
        }
        else if (static_cast<unsigned char>(c) >= 32)
        {
            collapsed += c;
            prev_space = false;
        }
    }

    if (static_cast<S32>(collapsed.size()) > MAX_REGION_NAME_LENGTH)
    {
        collapsed.resize(MAX_REGION_NAME_LENGTH);
    }

    return collapsed;
}

std::string FSGroupTitleRegionMgr::normalizeRegionName(const std::string& name)
{
    auto normalized = sanitizeRegionName(name);
    LLStringUtil::toLower(normalized);
    return normalized;
}

// ---------------------------------------------------------------------------
// Region input dialog (offloaded from floater)
// ---------------------------------------------------------------------------

void FSGroupTitleRegionMgr::showRegionInputDialog(const LLUUID& group_id, const LLUUID& role_id)
{
    LLSD payload;
    payload["group_id"] = group_id;
    payload["role_id"]  = role_id;
    LLNotificationsUtil::add("FSSetTitleRegion", LLSD(), payload, &FSGroupTitleRegionMgr::onRegionInputCallback);
}

bool FSGroupTitleRegionMgr::onRegionInputCallback(const LLSD& notification, const LLSD& response)
{
    if (LLNotificationsUtil::getSelectedOption(notification, response) != 0)
    {
        return false;
    }

    const auto region_name = sanitizeRegionName(response["region_name"].asString());
    if (region_name.empty())
    {
        return false;
    }

    const auto group_id = notification["payload"]["group_id"].asUUID();
    const auto role_id  = notification["payload"]["role_id"].asUUID();
    FSGroupTitleRegionMgr::getInstance()->validateAndSetAssignment(group_id, role_id, region_name);
    return false;
}

// ---------------------------------------------------------------------------
// Async region name validation
// ---------------------------------------------------------------------------

void FSGroupTitleRegionMgr::validateAndSetAssignment(const LLUUID& group_id, const LLUUID& role_id, const std::string& region_name)
{
    const auto sanitized = sanitizeRegionName(region_name);
    if (sanitized.empty())
    {
        return;
    }

    cancelPendingValidation();
    mPendingGroupID  = group_id;
    mPendingRoleID   = role_id;
    mPendingRegionName = sanitized;
    mHasPendingValidation = true;
    LLWorldMapMessage::getInstance()->sendNamedRegionRequest(
        sanitized,
        [this](U64 region_handle, const std::string&, const LLUUID&, bool)
        {
            onValidationResult(region_handle);
        },
        "",
        false
    );

    mValidationTimer = LLEventTimer::run_after(REGION_VALIDATION_TIMEOUT, [this]()
    {
        mValidationTimer = nullptr;
        onValidationTimeout();
    });
}

void FSGroupTitleRegionMgr::cancelPendingValidation()
{
    mHasPendingValidation = false;
    if (mValidationTimer)
    {
        delete mValidationTimer;
        mValidationTimer = nullptr;
    }
}

void FSGroupTitleRegionMgr::onValidationResult(U64 region_handle)
{
    if (!mHasPendingValidation)
    {
        return;
    }

    auto* sim_info = LLWorldMap::getInstance()->simInfoFromHandle(region_handle);
    if (!sim_info)
    {
        cancelPendingValidation();
        LLSD args;
        args["REGION"] = mPendingRegionName;
        LLNotificationsUtil::add("FSSetTitleRegionNotFound", args);
        return;
    }

    const auto canonical_name = sim_info->getName();
    if (normalizeRegionName(canonical_name) != normalizeRegionName(mPendingRegionName))
    {
        cancelPendingValidation();
        LLSD args;
        args["REGION"] = mPendingRegionName;
        LLNotificationsUtil::add("FSSetTitleRegionNotFound", args);
        return;
    }

    const auto group_id = mPendingGroupID;
    const auto role_id  = mPendingRoleID;
    cancelPendingValidation();
    setAssignment(group_id, role_id, canonical_name);
}

void FSGroupTitleRegionMgr::onValidationTimeout()
{
    if (!mHasPendingValidation)
    {
        return;
    }
    mHasPendingValidation = false;
    LLSD args;
    args["REGION"] = mPendingRegionName;
    LLNotificationsUtil::add("FSSetTitleRegionNotFound", args);
}

// ---------------------------------------------------------------------------
// Region change handler
// ---------------------------------------------------------------------------

void FSGroupTitleRegionMgr::onRegionChanged()
{
    if (!mDataLoaded)
    {
        return;
    }

    const auto* region = gAgent.getRegion();
    if (!region)
    {
        return;
    }

    if (RlvActions::isRlvEnabled() && !RlvActions::canChangeActiveGroup())
    {
        LL_DEBUGS() << "Skipping region-based group title switch: RLVa @setgroup lock active" << LL_ENDL;
        return;
    }

    const auto region_name = normalizeRegionName(region->getName());
    if (region_name.empty())
    {
        return;
    }

    if (region_name == mLastAppliedRegion)
    {
        return;
    }
    mLastAppliedRegion = region_name;

    LLUUID group_id;
    LLUUID role_id;
    if (getAssignmentForRegion(region_name, group_id, role_id))
    {
        if (group_id.notNull() && !gAgent.isInGroup(group_id))
        {
            LL_WARNS() << "Region '" << region->getName() << "' has a title assignment for a group we are no longer in, skipping" << LL_ENDL;
            return;
        }

        bool title_already_active = false;
        if (group_id.notNull())
        {
            if (const auto* group_data = LLGroupMgr::getInstance()->getGroupData(group_id))
            {
                for (const auto& title : group_data->mTitles)
                {
                    if (title.mRoleID == role_id && title.mSelected)
                    {
                        title_already_active = true;
                        break;
                    }
                }
            }
        }

        LL_INFOS() << "Region '" << region->getName() << "', switching to assigned group title" << LL_ENDL;

        if (group_id.notNull() && !title_already_active)
        {
            LLGroupMgr::getInstance()->sendGroupTitleUpdate(group_id, role_id);
        }

        if (gAgent.getGroupID() != group_id)
        {
            LLGroupActions::activate(group_id);
        }
    }
    else if (mNoneOnUnassigned && gAgent.getGroupID().notNull())
    {
        LL_INFOS() << "Region '" << region->getName() << "' has no title preset, deactivating group" << LL_ENDL;
        LLGroupActions::activate(LLUUID::null);
    }
}
