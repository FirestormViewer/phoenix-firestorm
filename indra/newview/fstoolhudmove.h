/**
 * @file fstoolhudmove.h
 * @brief Ctrl+drag moves a HUD on screen without the build tools
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (C) 2026, The Phoenix Firestorm Project, Inc.
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

#ifndef FS_TOOLHUDMOVE_H
#define FS_TOOLHUDMOVE_H

#include "lltool.h"
#include "llsingleton.h"
#include "llviewerobject.h"

class LLPickInfo;

// Ctrl+left drag moves a HUD you can modify across the screen without opening the build tools (FSCtrlDragHUDs).
// The HUD is moved locally while dragging and sends its new position once the user lets go, for efficiency.
class FSToolHudMove : public LLTool, public LLSingleton<FSToolHudMove>
{
    LLSINGLETON(FSToolHudMove);

public:
    // The HUD linkset Ctrl+drag (its root)
    static LLViewerObject* movable(LLViewerObject* object, MASK mask);

    static bool overMovableHUD(MASK mask);

    // True if a drag is successfully started
    bool start(const LLPickInfo& pick);

    bool handleHover(S32 x, S32 y, MASK mask) override;
    bool handleMouseUp(S32 x, S32 y, MASK mask) override;
    void onMouseCaptureLost() override;
    bool clipMouseWhenDown() override { return false; }

private:
    void moveTo(S32 x, S32 y);
    void finish();

    LLPointer<LLViewerObject>   mObject;
    LLVector3                   mStartPosition;     // relative to HUD attachment point
    LLVector3                   mStartMouseHUD;
    S32                         mMouseDownX = 0;
    S32                         mMouseDownY = 0;
    bool                        mMoved = false;
};

#endif
