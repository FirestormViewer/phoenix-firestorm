/**
 * @file fsfloaterteleporthistory.h
 * @brief Class for the standalone teleport history in Firestorm
 *
 * $LicenseInfo:firstyear=2012&license=viewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (c) 2012 Ansariel Hiller @ Second Life
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

#ifndef FS_FLOATERTELEPORTHISTORY_H
#define FS_FLOATERTELEPORTHISTORY_H

#include "llfloater.h"

class LLFilterEditor;
class LLTeleportHistoryPanel;
class LLMenuButton;

class FSFloaterTeleportHistory : public LLFloater
{
public:
    FSFloaterTeleportHistory(const LLSD& seed);
    ~FSFloaterTeleportHistory() override;

    bool postBuild() override;
    bool handleKeyHere(KEY key, MASK mask) override;
    bool hasAccelerators() const override { return true; }

    void resetFilter();

private:
    void onFilterEdit(const std::string& search_string, bool force_filter);
    void onGearMenuClick();
    void onSortingMenuClick();

    LLTeleportHistoryPanel* mHistoryPanel;
    LLFilterEditor*         mFilterEditor;
    LLMenuButton*               mGearMenuButton;
    LLMenuButton*               mSortingMenuButton;
};

#endif // FS_FLOATERTELEPORTHISTORY_H
