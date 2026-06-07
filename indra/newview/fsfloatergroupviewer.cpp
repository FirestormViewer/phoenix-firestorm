/**
 * @file fsfloatergroupviewer.cpp
 * @brief List the active groups worn on the current region, with per-group
 *        wearer counts. Group identity comes from the nameplate group-tint
 *        resolution machinery (profile groups + attachment-group fallback).
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Phoenix Firestorm Viewer Source Code
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

#include "fsfloatergroupviewer.h"

#include "fsscrolllistctrl.h"
#include "llagent.h"
#include "llcachename.h"
#include "llcheckboxctrl.h"
#include "llclipboard.h"
#include "llgroupactions.h"
#include "llscrolllistitem.h"
#include "lltextbox.h"
#include "lluictrlfactory.h"
#include "llviewerregion.h"
#include "llvoavatar.h"

static void no_op_name_callback(const LLUUID&, const std::string&, bool) {}

static const F32 REFRESH_INTERVAL = 2.f;        // seconds between list refreshes
static const F64 INACTIVITY_SECONDS = 60.0;     // stationary for longer = inactive
static const F64 ACTIVITY_EPSILON_SQ = 0.0625;  // (0.25m)^2 positional dead zone

// ---------------------------------------------------------------------------
// FSGroupViewerContextMenu
// ---------------------------------------------------------------------------

LLContextMenu* FSGroupViewerContextMenu::createMenu()
{
    LLUICtrl::CommitCallbackRegistry::ScopedRegistrar registrar;

    const LLUUID& id = mUUIDs.front();
    registrar.add("GroupViewer.ViewProfile", boost::bind(&LLGroupActions::show, id, false));
    registrar.add("GroupViewer.CopyUUID",    boost::bind(&FSGroupViewerContextMenu::copyUUID, this));
    registrar.add("GroupViewer.CopyURI",     boost::bind(&FSGroupViewerContextMenu::copyURI, this));

    return createFromFile("menu_fs_group_viewer.xml");
}

void FSGroupViewerContextMenu::copyUUID()
{
    const std::string uuid_str = mUUIDs.front().asString();
    LLClipboard::instance().copyToClipboard(utf8str_to_wstring(uuid_str), 0, static_cast<S32>(uuid_str.length()));
}

void FSGroupViewerContextMenu::copyURI()
{
    const std::string uri = "secondlife:///app/group/" + mUUIDs.front().asString() + "/about";
    LLClipboard::instance().copyToClipboard(utf8str_to_wstring(uri), 0, static_cast<S32>(uri.length()));
}

// ---------------------------------------------------------------------------
// FSFloaterGroupViewer
// ---------------------------------------------------------------------------

FSFloaterGroupViewer::FSFloaterGroupViewer(const LLSD& key)
:   LLFloater(key)
{
}

bool FSFloaterGroupViewer::postBuild()
{
    mGroupList = getChild<FSScrollListCtrl>("group_list");
    mGroupList->setContextMenu(&mContextMenu);

    mIgnoreInactiveCheck = getChild<LLCheckBoxCtrl>("ignore_inactive_check");
    mIgnoreInactiveCheck->setCommitCallback(boost::bind(&FSFloaterGroupViewer::refreshList, this));

    mStatusText = getChild<LLTextBox>("status_text");

    return true;
}

void FSFloaterGroupViewer::onOpen(const LLSD& key)
{
    mActivity.clear();
    refreshList();
    mRefreshTimer.reset();
}

void FSFloaterGroupViewer::draw()
{
    if (mRefreshTimer.getElapsedTimeF32() > REFRESH_INTERVAL)
    {
        refreshList();
        mRefreshTimer.reset();
    }
    LLFloater::draw();
}

void FSFloaterGroupViewer::refreshList()
{
    const F64 now = LLFrameTimer::getTotalSeconds();
    const bool ignore_inactive = mIgnoreInactiveCheck->getValue().asBoolean();
    const LLViewerRegion* own_region = gAgent.getRegion();

    std::map<LLUUID, S32> group_counts;
    S32 total_avatars = 0;
    S32 unresolved = 0;
    S32 inactive_skipped = 0;
    uuid_set_t seen;

    for (LLCharacter* character : LLCharacter::sInstances)
    {
        LLVOAvatar* avatarp = static_cast<LLVOAvatar*>(character);
        if (!avatarp || avatarp->isDead() || avatarp->isControlAvatar() || avatarp->getRegion() != own_region)
        {
            continue;
        }

        const LLUUID& av_id = avatarp->getID();
        seen.insert(av_id);
        ++total_avatars;

        // Positional activity: only sampled here, on the refresh tick.
        // A nonzero velocity counts as movement immediately so walking
        // avatars never read as inactive between samples.
        const LLVector3d pos = avatarp->getPositionGlobal();
        AvatarActivity& rec = mActivity[av_id];
        if (rec.mLastMoveTime == 0.0 ||
            dist_vec_squared(pos, rec.mLastPos) > ACTIVITY_EPSILON_SQ ||
            avatarp->getVelocity().lengthSquared() > 0.01f)
        {
            rec.mLastPos = pos;
            rec.mLastMoveTime = now;
        }

        if (ignore_inactive && (now - rec.mLastMoveTime) > INACTIVITY_SECONDS)
        {
            ++inactive_skipped;
            continue;
        }

        // Own avatar: the agent knows its active group directly.
        LLUUID group_id = avatarp->isSelf() ? gAgent.getGroupID() : avatarp->getActiveGroupID();
        if (group_id.isNull())
        {
            if (!avatarp->isSelf())
            {
                ++unresolved;
                // Hidden-group avatar (or still resolving): nudge the
                // attachment-group probe. Internally cooldown-throttled.
                avatarp->requestGroupProbeIfUnresolved();
            }
            continue;
        }

        ++group_counts[group_id];
    }

    // Drop activity records for avatars that left the region.
    for (activity_map_t::iterator it = mActivity.begin(); it != mActivity.end(); )
    {
        if (seen.find(it->first) == seen.end())
        {
            it = mActivity.erase(it);
        }
        else
        {
            ++it;
        }
    }

    // Rebuild the list, preserving selection and scroll position.
    const LLUUID selected = mGroupList->getSelectedValue().asUUID();
    const S32 scroll_pos = mGroupList->getScrollPos();
    mGroupList->clearRows();

    for (const auto& entry : group_counts)
    {
        std::string group_name;
        if (!gCacheName->getGroupName(entry.first, group_name))
        {
            group_name = "(loading...)";
            // Request the name; a later refresh tick picks it up from cache.
            gCacheName->getGroup(entry.first, boost::bind(&no_op_name_callback, _1, _2, _3));
        }

        LLSD row;
        row["value"] = entry.first;
        row["columns"][0]["column"] = "group_name";
        row["columns"][0]["value"] = group_name;
        row["columns"][1]["column"] = "count";
        row["columns"][1]["value"] = entry.second;
        mGroupList->addElement(row);
    }

    mGroupList->setScrollPos(scroll_pos);
    if (selected.notNull())
    {
        mGroupList->selectByValue(selected);
    }

    std::string status = llformat("%d avatars, %d groups", total_avatars, (S32)group_counts.size());
    if (unresolved > 0)
    {
        status += llformat(", %d resolving", unresolved);
    }
    if (ignore_inactive && inactive_skipped > 0)
    {
        status += llformat(", %d inactive ignored", inactive_skipped);
    }
    mStatusText->setText(status);
}
