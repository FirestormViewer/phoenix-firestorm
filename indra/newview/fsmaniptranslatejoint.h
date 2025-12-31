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

#include "llselectmgr.h"
#include "fsmaniprotatejoint.h"
#include "llmaniptranslate.h"

class LLJoint;
class LLVOAvatar; // for LLVOAvatarSelf, etc.

class FSManipTranslateJoint : public LLManipTranslate
{
public:
    class ManipulatorHandle
    {
    public:
        LLVector3   mStartPosition;
        LLVector3   mEndPosition;
        EManipPart  mManipID;
        F32         mHotSpotRadius;

        ManipulatorHandle(LLVector3 start_pos, LLVector3 end_pos, EManipPart id, F32 radius):mStartPosition(start_pos), mEndPosition(end_pos), mManipID(id), mHotSpotRadius(radius){}
    };


    FSManipTranslateJoint(LLToolComposite* composite);
    virtual ~FSManipTranslateJoint() {};

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

    static  U32     getGridTexName() ;
    static  void    destroyGL();
    static  void    restoreGL();
    virtual bool    handleMouseDown(S32 x, S32 y, MASK mask);
    virtual bool    handleMouseUp(S32 x, S32 y, MASK mask);
    virtual bool    handleHover(S32 x, S32 y, MASK mask);
    virtual void    render();
    virtual void    handleSelect();

    virtual void    highlightManipulators(S32 x, S32 y);
    virtual bool    handleMouseDownOnPart(S32 x, S32 y, MASK mask);
    virtual bool    canAffectSelection();

protected:
    enum EHandleType {
        HANDLE_CONE,
        HANDLE_BOX,
        HANDLE_SPHERE
    };

    void        renderArrow(S32 which_arrow, S32 selected_arrow, F32 box_size, F32 arrow_size, F32 handle_size, bool reverse_direction);
    void        renderTranslationHandles();
    void        renderText();

private:
    bool isAvatarJointSafeToUse();

    /// <summary>
    /// Determines if the currently selected joint is one to move with manip.
    /// </summary>
    /// <returns>True if we can move the joint with manip, otherwise false.</returns>
    bool isMoveableJoint();
    void getManipJointNormal(LLJoint* joint, EManipPart manip, LLVector3& normal) { getManipNormal(nullptr, manip, normal);};
    bool getManipJointAxis(LLJoint* joint, EManipPart manip, LLVector3& axis) { return getManipAxis(nullptr, manip, axis); };

    LLVector3 getChangeInPosition(LLVector3 newPosition) const;

    S32         mLastHoverMouseX;
    S32         mLastHoverMouseY;
    bool        mMouseOutsideSlop;      // true after mouse goes outside slop region
    bool        mCopyMadeThisDrag;
    S32         mMouseDownX;
    S32         mMouseDownY;
    F32         mAxisArrowLength;       // pixels
    F32         mConeSize;              // meters, world space
    F32         mArrowLengthMeters;     // meters
    F32         mPlaneManipOffsetMeters;
    LLVector3   mManipNormal;
    LLVector3   mDragCursorLastGlobal;
    LLVector3d  mDragCursorStartGlobal;
    LLVector3d  mDragSelectionStartGlobal;
    LLTimer     mUpdateTimer;
    LLVector4   mManipulatorVertices[18];
    F32         mSnapOffsetMeters;
    LLVector3   mSnapOffsetAxis;
    LLQuaternion mGridRotation;
    LLVector3   mGridOrigin;
    LLVector3   mGridScale;
    LLVector3   mArrowScales;
    LLVector3   mPlaneScales;
    LLVector4   mPlaneManipPositions;

    LLJoint*              mJoint          = nullptr;
    LLVOAvatar*           mAvatar         = nullptr;
    E_PoserReferenceFrame mReferenceFrame = POSER_FRAME_BONE;
};

#endif
