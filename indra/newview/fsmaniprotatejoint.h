/**
 * @file fsmaniproatejoint.h
 * @brief custom manipulator for rotating joints
 *
 * $LicenseInfo:firstyear=2024&license=viewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (c) 2025 Beq Janus @ Second Life
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

#ifndef FS_MANIP_ROTATE_JOINT_H
#define FS_MANIP_ROTATE_JOINT_H

#include "llselectmgr.h"
#include "llmaniprotate.h"

class LLJoint;
class LLVOAvatar;  // or LLVOAvatarSelf, etc.

namespace {
    const F32 AXIS_ONTO_CAM_TOLERANCE = cos( 85.f * DEG_TO_RAD ); // cos() is not constexpr til c++26
    constexpr F32 RADIUS_PIXELS = 100.f;        // size in screen space
    constexpr S32 CIRCLE_STEPS = 100;
    constexpr F32 CIRCLE_STEP_SIZE = 2.0f * F_PI / CIRCLE_STEPS;
    constexpr F32 SQ_RADIUS = RADIUS_PIXELS * RADIUS_PIXELS;
    constexpr F32 WIDTH_PIXELS = 8;
    constexpr F32 MAX_MANIP_SELECT_DISTANCE = 100.f;
    constexpr F32 SNAP_ANGLE_INCREMENT = 5.625f;
    constexpr F32 SNAP_ANGLE_DETENTE = SNAP_ANGLE_INCREMENT;
    constexpr F32 SNAP_GUIDE_RADIUS_1 = 2.8f;
    constexpr F32 SNAP_GUIDE_RADIUS_2 = 2.4f;
    constexpr F32 SNAP_GUIDE_RADIUS_3 = 2.2f;
    constexpr F32 SNAP_GUIDE_RADIUS_4 = 2.1f;
    constexpr F32 SNAP_GUIDE_RADIUS_5 = 2.05f;
    constexpr F32 SNAP_GUIDE_INNER_RADIUS = 2.f;
    constexpr F32 SELECTED_MANIPULATOR_SCALE = 1.05f;
    constexpr F32 MANIPULATOR_SCALE_HALF_LIFE = 0.07f;
    constexpr S32 VERTICAL_OFFSET = 100;
}

class FSManipRotateJoint : public LLManipRotate
{
    // Used for overriding the natural "up" direction of a joint.
    // if no override is set then the world up direction is used unless the joint is vertical in which case we pick an arbitrary normal
    static std::unordered_map<std::string, LLVector3> sReferenceUpVectors;

    struct BoneAxes
    {
        LLVector3 naturalX;
        LLVector3 naturalY;
        LLVector3 naturalZ;
    };    
    LLQuaternion computeAlignmentQuat( const BoneAxes& boneAxes ) const;
    BoneAxes computeBoneAxes() const;

public:
    FSManipRotateJoint(LLToolComposite* composite);
    virtual ~FSManipRotateJoint() {}
    static std::string getManipPartString(EManipPart part);
    // Called to designate which joint we are going to manipulate.
    void setJoint(LLJoint* joint);

    void setAvatar(LLVOAvatar* avatar);

    // Overrides
    void handleSelect() override;
    bool updateVisiblity();
    void render() override;
    void renderNameXYZ(const LLQuaternion& rot);
    bool handleMouseDown(S32 x, S32 y, MASK mask) override;
    bool handleMouseUp(S32 x, S32 y, MASK mask) override;
    bool handleHover(S32 x, S32 y, MASK mask) override;
    void drag(S32 x, S32 y) override;
    bool isAlwaysRendered() override { return true; }
    void highlightManipulators(S32 x, S32 y) override;
    bool handleMouseDownOnPart(S32 x, S32 y, MASK mask) override; 
    void highlightHoverSpheres(S32 mouseX, S32 mouseY);

protected:
    LLQuaternion dragUnconstrained( S32 x, S32 y );
    LLQuaternion dragConstrained( S32 x, S32 y );
    LLVector3 getConstraintAxis() const { return mConstraintAxis; };
    LLVector3 setConstraintAxis();

    // Instead of selecting an LLViewerObject, we have a single joint
    LLJoint*     mJoint        = nullptr;
    BoneAxes     mBoneAxes;
    LLQuaternion mNaturalAlignmentQuat;
    LLVOAvatar*  mAvatar       = nullptr;
    // We'll store the joint's original rotation for reference
    LLQuaternion mSavedJointRot;
    LLJoint * mHighlightedJoint = nullptr;
    F32       mHighlightedPartDistance = 0.f;
    LLVector3 mLastEuler = LLVector3::zero; // last euler angles in degrees
    LLVector3 mInitialIntersection;  // The initial point on the manipulator’s sphere (in agent space)
    const std::vector<std::string_view> getSelectableJoints(){ return sSelectableJoints; };

private:
    bool isMouseOverJoint(S32 mouseX, S32 mouseY, const LLVector3& jointWorldPos, F32 jointRadius, F32& outDistanceFromCamera, F32& outDistanceFromCenter) const;
    static const std::vector<std::string_view> sSelectableJoints;

    // Structure holding parameters needed to render one manipulator ring.
    struct RingRenderParams
    {
        EManipPart part;             // e.g. LL_ROT_Z, LL_ROT_Y, LL_ROT_X, etc.
        LLVector4 targetScale;       // Target scale for mManipulatorScales for this part.
        float extraRotateAngle;      // Extra rotation angle (in degrees) to apply.
        LLVector3 extraRotateAxis;   // Axis for the extra rotation.
        LLColor4 primaryColor;       // Primary ring color.
        LLColor4 secondaryColor;     // Secondary ring color.
        int scaleIndex;              // Which component of mManipulatorScales to use (0: X, 1: Y, 2: Z, 3: Roll).
    };

    static const std::unordered_map<EManipPart, RingRenderParams> sRingParams;
    void updateManipulatorScale(EManipPart part, LLVector4& scales);
    void renderActiveRing( F32 radius, F32 width, const LLColor4& front_color, const LLColor4& back_color);
    void renderManipulatorRings(const LLVector3& center, const LLQuaternion& finalAlignment);

    void renderCenterCircle(const F32 radius, const LLColor4& normal_color = LLColor4(0.7f,0.7f,0.7f,0.2f), const LLColor4& highlight_color = LLColor4(0.8f,0.8f,0.8f,0.3f)); // <FS:minerjr> add missing f for float
    void renderCenterSphere(const F32 radius, const LLColor4& normal_color = LLColor4(0.7f,0.7f,0.7f,0.2f), const LLColor4& highlight_color = LLColor4(0.8f,0.8f,0.8f,0.3f)); // <FS:minerjr> add missing f for float
    void renderRingPass(const RingRenderParams& params, float radius, float width, int pass);
    void renderAxes(const LLVector3& center, F32 size, const LLQuaternion& rotation);

    float mLastAngle = 0.f;
    LLVector3 mConstraintAxis;
};

#endif // FS_MANIP_ROTATE_JOINT_H
