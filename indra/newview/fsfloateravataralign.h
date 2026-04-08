 /**
 * @file fsfloateravataralign.h
 * @brief Floater for rotating the avatar to face cardinal directions or nearest avatar
 * @author chanayane@firestorm
 *
 * $LicenseInfo:firstyear=2026&license=fsviewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (C) 2026, Ayane Lyla
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

#ifndef FS_FLOATER_AVATAR_ALIGN_H
#define FS_FLOATER_AVATAR_ALIGN_H

#include "llfloater.h"

class LLVOAvatar;

// Base class: all non-UI logic and compass rendering.
// Subclasses provide the XUI layout and mode-specific behaviour.
class FSAvatarAlignBase : public LLFloater
{
    LOG_CLASS(FSAvatarAlignBase);
protected:
    FSAvatarAlignBase(const LLSD& key);
public:
    void draw() override;
    bool handleMouseDown(S32 x, S32 y, MASK mask) override;
    bool handleHover(S32 x, S32 y, MASK mask) override;

    // Face a specific avatar. Safe to call with nullptr (no-op).
    void faceAvatar(LLVOAvatar* avatar);
    // Face the nearest non-flying avatar within MAX_FACE_DISTANCE.
    void onClickFaceNearestAvatar();
    // Returns true if avatar is alive and within MAX_FACE_DISTANCE.
    bool isAvatarInRange(LLVOAvatar* avatar) const;

    // Returns whichever mode's floater instance exists (based on AvatarAlignMini setting).
    static FSAvatarAlignBase* getActive();

    static constexpr F32 MAX_FACE_DISTANCE = 20.f;

protected:
    virtual bool isMiniMode()       const { return false; }
    virtual S32  getToolbarHeight() const { return 0;     }
    virtual S32  getBottomReserve() const { return 104;   }
    virtual void onToggleMode()           = 0;

    // Repositions 'next' floater relative to this floater's pre-close rect.
    void repositionOnToggle(LLFloater* next, const LLRect& old_rect);

    void onClickCardinal(F32 target_deg);
    void onClickRotate(F32 delta_deg);
    void onClickNearest();
    void rotateAgentTo(F32 target_deg);
    void applyRotation(const LLVector3& direction);
    void snapAvatarBody(const LLVector3& target_at);
    void snapRemoteAvatarBody(LLVOAvatar* avatar);
    // All mode-dependent layout values needed to draw the compass, computed once per frame.
    struct CompassLayout
    {
        S32  R;             // compass radius in pixels
        F32  cx, cy;        // compass center in local screen coords
        bool mini_compact;  // true when mini mode and radius is too small for bearing/intercardinals
        bool show_labels;   // true when full mode and radius is large enough for N/S/E/W labels
        S32  bearing_y;     // Y position of the bearing text below the ring
        S32  sq_half;       // half-size of the bounding square (used for toggle button placement)
        std::string toggle_label; // "Mini" or "Full"
    };

    CompassLayout buildCompassLayout() const;
    void          drawCompass();

    LLVOAvatar* mTargetAvatar = nullptr;

    // Compass geometry (local screen coords), updated each draw().
    S32    mCompassCX   = 0;
    S32    mCompassCY   = 0;
    S32    mCompassR    = 0;
    // Hover: -1=none, -2=centre, 0-7=octant (index × 45° = degrees from North CW)
    S32    mHoverOctant = -1;
    bool   mHoverToggle = false;
    // Toggle-mode button rect (local coords), updated each draw().
    LLRect mToggleBtnRect;
};

// Full-mode floater: complete button set, large compass.
class FSFloaterAvatarAlign : public FSAvatarAlignBase
{
    LOG_CLASS(FSFloaterAvatarAlign);
    friend class LLFloaterReg;
private:
    FSFloaterAvatarAlign(const LLSD& key);
public:
    bool postBuild() override;
    void onOpen(const LLSD& key) override;
protected:
    void onToggleMode() override;
};

// Mini-mode floater: compact toolbar, fill-to-size compass.
class FSFloaterAvatarAlignMini : public FSAvatarAlignBase
{
    LOG_CLASS(FSFloaterAvatarAlignMini);
    friend class LLFloaterReg;
private:
    FSFloaterAvatarAlignMini(const LLSD& key);
public:
    bool postBuild() override;
    void onOpen(const LLSD& key) override;
protected:
    bool isMiniMode()        const override { return true; }
    void onToggleMode()            override;
    S32  getToolbarHeight()  const override { return 22;   }
    S32  getBottomReserve()  const override { return 3;    }
};

#endif // FS_FLOATER_AVATAR_ALIGN_H
