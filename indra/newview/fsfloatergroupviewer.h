/**
 * @file fsfloatergroupviewer.h
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

#ifndef FS_FLOATERGROUPVIEWER_H
#define FS_FLOATERGROUPVIEWER_H

#include "llfloater.h"
#include "llframetimer.h"
#include "lllistcontextmenu.h"

class FSScrollListCtrl;
class LLCheckBoxCtrl;
class LLTextBox;

// Right-click menu on a group row: view profile, copy UUID, copy URI.
class FSGroupViewerContextMenu : public LLListContextMenu
{
public:
    /*virtual*/ LLContextMenu* createMenu() override;

private:
    void copyUUID();
    void copyURI();
};

class FSFloaterGroupViewer : public LLFloater
{
public:
    FSFloaterGroupViewer(const LLSD& key);
    virtual ~FSFloaterGroupViewer() = default;

    bool postBuild() override;
    void onOpen(const LLSD& key) override;
    void draw() override;

private:
    void refreshList();

    // Positional activity tracking, sampled on the refresh tick only while
    // the floater is open. An avatar is "inactive" when it hasn't moved
    // more than ACTIVITY_EPSILON in the last INACTIVITY_SECONDS.
    struct AvatarActivity
    {
        LLVector3d  mLastPos;
        F64         mLastMoveTime{ 0.0 };
    };
    typedef std::map<LLUUID, AvatarActivity> activity_map_t;
    activity_map_t      mActivity;

    FSScrollListCtrl*   mGroupList{ nullptr };
    LLCheckBoxCtrl*     mIgnoreInactiveCheck{ nullptr };
    LLTextBox*          mStatusText{ nullptr };

    LLFrameTimer        mRefreshTimer;

    FSGroupViewerContextMenu mContextMenu;
};

#endif // FS_FLOATERGROUPVIEWER_H
