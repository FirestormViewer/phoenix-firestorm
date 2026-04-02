/**
 * @file fsgrouptitleregionmgr.h
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

#ifndef FS_GROUPTITLEREGIONMGR_H
#define FS_GROUPTITLEREGIONMGR_H

#include "lleventtimer.h"
#include "llsingleton.h"
#include "lluuid.h"

#include <boost/signals2.hpp>
#include <map>
#include <string>
#include <vector>

struct FSRegionTitleAssignment
{
    LLUUID      mGroupID;
    LLUUID      mRoleID;
    std::string mDisplayName;
};

class FSGroupTitleRegionMgr : public LLSingleton<FSGroupTitleRegionMgr>
{
    LOG_CLASS(FSGroupTitleRegionMgr);
    LLSINGLETON(FSGroupTitleRegionMgr);
    ~FSGroupTitleRegionMgr();

public:
    void loadFromDisk();
    void saveToDisk();
    void setAssignment(const LLUUID& group_id, const LLUUID& role_id, const std::string& region_name);
    void setAssignmentForCurrentRegion(const LLUUID& group_id, const LLUUID& role_id);
    void clearAssignment(const LLUUID& group_id, const LLUUID& role_id);
    void clearAssignmentByRegion(const std::string& region_name);
    std::string getRegionForTitle(const LLUUID& group_id, const LLUUID& role_id) const;
    std::vector<std::string> getRegionDisplayNamesForTitle(const LLUUID& group_id, const LLUUID& role_id) const;
    bool getAssignmentForRegion(const std::string& region_name, LLUUID& group_id, LLUUID& role_id) const;
    void setNoneOnUnassigned(bool enabled);
    bool getNoneOnUnassigned() const { return mNoneOnUnassigned; }
    void showRegionInputDialog(const LLUUID& group_id, const LLUUID& role_id);
    static std::string sanitizeRegionName(const std::string& input);

    using assignments_changed_signal_t = boost::signals2::signal<void()>;
    boost::signals2::connection setAssignmentsChangedCallback(const assignments_changed_signal_t::slot_type& cb);

private:
    void validateAndSetAssignment(const LLUUID& group_id, const LLUUID& role_id, const std::string& region_name);
    void onRegionChanged();
    void cancelPendingValidation();
    void onValidationResult(U64 region_handle);
    void onValidationTimeout();
    std::string getFilename() const;
    static std::string normalizeRegionName(const std::string& name);
    static bool onRegionInputCallback(const LLSD& notification, const LLSD& response);
    bool mNoneOnUnassigned = false;
    bool mDataLoaded       = false;
    std::string mLastAppliedRegion;

    using assignment_map_t = std::map<std::string, FSRegionTitleAssignment>;
    assignment_map_t mAssignments;

    bool            mHasPendingValidation = false;
    LLUUID          mPendingGroupID;
    LLUUID          mPendingRoleID;
    std::string     mPendingRegionName;
    LLEventTimer*   mValidationTimer  = nullptr;
    LLEventTimer*   mLoginRetryTimer  = nullptr;

    boost::signals2::connection  mRegionChangedConnection;
    assignments_changed_signal_t mAssignmentsChangedSignal;
};

#endif // FS_GROUPTITLEREGIONMGR_H
