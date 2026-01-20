/**
 * @file fsmaniptranslatejoint.h
 * @brief custom manipulator for moving joints
 *
 * $LicenseInfo:firstyear=2024&license=viewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (c) 2025 Angeldark Raymaker @ Second Life
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

#ifndef FS_MANIP_TRANSLATE_JOINT_H
#define FS_MANIP_TRANSLATE_JOINT_H

#include "fsmaniprotatejoint.h"
#include "llmaniptranslate.h"

class LLJoint;
class LLVOAvatar; // for LLVOAvatarSelf, etc.

class FSManipTranslateJoint : public LLManipTranslate
{
public:
    FSManipTranslateJoint(LLToolComposite* composite);
    virtual ~FSManipTranslateJoint() = default;

    /// <summary>
    /// Sets the joint we are going to manipulate.
    /// </summary>
    /// <param name="joint">The joint to interact with.</param>
    void setJoint(LLJoint* joint);

    /// <summary>
    /// Sets the avatar the manip should interact with.
    /// </summary>
    /// <param name="avatar">The avatar to interact with.</param>
    void setAvatar(LLVOAvatar* avatar);

    /// <summary>
    /// Sets the avatar the manip should interact with.
    /// </summary>
    /// <param name="avatar">The avatar to interact with.</param>
    void setReferenceFrame(const E_PoserReferenceFrame frame) { mReferenceFrame = frame; };

    static U32  getGridTexName();
    static void destroyGL();
    static void restoreGL();
    bool        handleMouseDown(S32 x, S32 y, MASK mask) override;
    bool        handleMouseUp(S32 x, S32 y, MASK mask) override;
    bool        handleHover(S32 x, S32 y, MASK mask) override;
    void        render() override;
    void        handleSelect() override;

    void highlightManipulators(S32 x, S32 y) override;
    bool handleMouseDownOnPart(S32 x, S32 y, MASK mask) override;
    bool canAffectSelection() override;

protected:
    void renderArrow(S32 which_arrow, S32 selected_arrow, F32 box_size, F32 arrow_size, F32 handle_size, bool reverse_direction);
    void renderTranslationHandles();
    void renderText();

private:
    bool isAvatarJointSafeToUse();

    /// <summary>
    /// Determines if the currently selected joint is one to move with manip.
    /// </summary>
    /// <returns>True if we can move the joint with manip, otherwise false.</returns>
    bool isMoveableJoint();
    void getManipJointNormal(LLJoint* joint, EManipPart manip, LLVector3& normal) { getManipNormal(nullptr, manip, normal); };
    bool getManipJointAxis(LLJoint* joint, EManipPart manip, LLVector3& axis) { return getManipAxis(nullptr, manip, axis); };

    LLVector3 getChangeInPosition(const LLVector3& newPosition) const;

    LLVector3 mDragCursorLastGlobal;

    LLJoint*              mJoint          = nullptr;
    LLVOAvatar*           mAvatar         = nullptr;
    E_PoserReferenceFrame mReferenceFrame = POSER_FRAME_BONE;
};

#endif
