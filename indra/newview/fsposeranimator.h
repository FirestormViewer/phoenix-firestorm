/**
 * @file fsposeranimator.h
 * @brief business-layer for posing your (and other) avatar(s).
 *
 * $LicenseInfo:firstyear=2024&license=viewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (c) 2024 Angeldark Raymaker @ Second Life
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
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#ifndef LL_FSPoserAnimator_H
#define LL_FSPoserAnimator_H

#include "fsposingmotion.h"
#include "llvoavatar.h"

/// <summary>
/// Describes how we will cluster the joints/bones/thingos.
/// Each joint/bone/thingo should have one of these, FSPoserAnimator.PoserJoints.
/// </summary>
typedef enum E_BoneTypes
{
    WHOLEAVATAR = 0,  // required to be a single instance of, this one manipulates everything
    BODY        = 1,
    FACE        = 2,
    HANDS       = 3,
    MISC        = 4,
    COL_VOLUMES = 5
} E_BoneTypes;

/// <summary>
/// When we're adjusting a bone/joint/thingo we may want to do something else simultaneously.
/// This describes the other things we might do: eg: mirror the change to the opposite joint.
/// </summary>
typedef enum E_BoneDeflectionStyles
{
    NONE              = 0,  // do nothing additional
    MIRROR            = 1,  // change the other joint, like in a mirror, eg: one left one right
    SYMPATHETIC       = 2,  // change the other joint, but opposite to a mirrored way, eg: both go right or both go left
    DELTAMODE         = 3,  // each selected joint changes by the same supplied amount relative to their current
    MIRROR_DELTA      = 4,  // Applies a MIRROR delta, this limb and its opposite change by opposite amount
    SYMPATHETIC_DELTA = 5,  // Applies a SYMPATHETIC delta, this limb and the opposite change by the same amount
} E_BoneDeflectionStyles;

/// <summary>
/// Joints may have rotations applied by applying an absolute value or a delta value.
/// When applying a rotation as absolutes, feedback via the UI can tend to Gimbal lock control of the quaternion.
/// For certain joints, particularly "down the centreline", absolute rotations provide the best feel.
/// For other joints, such as hips, knees, elbows and wrists, Gimbal lock readily occurs (sitting poses particularly), and
/// applying small angle changes directly to the quaternion (rather than going via the locked absolute) makes for
/// a more sensible user experience.
/// </summary>
typedef enum E_RotationStyle
{
    ABSOLUTE_ROT = 0,  //  The rotation should be applied as an absolute value because while it can Gimbal lock, it doesn't happen often.
    DELTAIC_ROT  = 1,  //  The rotation should be applied as a delta value because it is apt to Gimbal lock.
} E_RotationStyle;

/// <summary>
/// When we're going from bone-rotation to the UI sliders, some of the axes need swapping so they make sense in UI-terms.
/// eg: for one bone, the X-axis may mean up and down, but for another bone, the x-axis might be left-right.
/// This is an ease-of-use option making the trackpad more 'natural' when manipulating a joint.
/// </summary>
typedef enum E_BoneAxisTranslation
{
    SWAP_NOTHING        = 0,
    SWAP_YAW_AND_ROLL   = 1,
    SWAP_YAW_AND_PITCH  = 2,
    SWAP_ROLL_AND_PITCH = 3,
    SWAP_X2Y_Y2Z_Z2X    = 4,
    SWAP_X2Z_Y2X_Z2Y    = 5,
} E_BoneAxisTranslation;

/// <summary>
/// Similar to translating axes from LLJoint rotations to UI sliders for up/down/left/right, these
/// negate (multiply by -1) axial changes.
/// This makes using the trackpad more 'natural' when manipulating a joint.
/// </summary>
typedef enum E_BoneAxisNegation
{
    NEGATE_NOTHING = 0,
    NEGATE_YAW     = 1,
    NEGATE_PITCH   = 2,
    NEGATE_ROLL    = 4,
    NEGATE_ALL     = 8,
} E_BoneAxisNegation;

class FSPoserAnimator
{
public:
    FSPoserAnimator() = default;

    virtual ~FSPoserAnimator() = default;

    /// <summary>
    /// A class encapsulating 'metadata' for a joint, such as its catagory and its opposite joint name.
    /// You'll note it's privates and methods: this is just emulating { get; private set; } from C#
    /// </summary>
    class FSPoserJoint
    {
        std::string              mJointName;  // expected to be a match to LLJoint.getName() for a joint implementation.
        std::string              mMirrorJointName;
        E_BoneTypes              mBoneList;
        std::vector<std::string> mBvhChildren;
        std::string              bvhOffsetValue;
        std::string              bvhEndSiteOffset;
        bool                     mDontFlipOnMirror = false;

      public:
        /// <summary>
        /// Gets the name of the joint.
        /// </summary>
        std::string jointName() const { return mJointName; }

        /// <summary>
        /// Gets the name of the mirror of this joint, or an empty string if there is no mirror.
        /// </summary>
        std::string mirrorJointName() const { return mMirrorJointName; }

        /// <summary>
        /// Gets the E_BoneTypes of the joint.
        /// </summary>
        E_BoneTypes boneType() const { return mBoneList; }

        /// <summary>
        /// Gets whether when mirroring the entire body, should this joint flip its counterpart.
        /// </summary>
        bool dontFlipOnMirror() const { return mDontFlipOnMirror; }

        /// <summary>
        /// Gets the collection of child bvh joints for this.
        /// </summary>
        std::vector<std::string> bvhChildren() const { return mBvhChildren; }

        /// <summary>
        /// Gets the bvh offset value for this joint.
        /// </summary>
        /// <remarks>
        /// These values are derived from \newview\character\avatar_skeleton.xml
        /// </remarks>
        std::string bvhOffset() const { return bvhOffsetValue; }

        /// <summary>
        /// Gets the bvh end site offset magic number for this joint.
        /// </summary>
        std::string bvhEndSite() const { return bvhEndSiteOffset; }

        /// <summary>
        /// Creates a new instance of a PoserJoint.
        /// </summary>
        /// <param name="joint_name">
        /// The joint name, should be one of the well known bones/joints/thingos.
        /// An example for an LLJoints implementation would be what LLJoint.getName() returns, like 'mChest'.
        /// Very likely case-sensitive.
        /// </param>
        /// <param name="mirror_joint_name">The opposite joint name, if any. Also expected to be a well-known name.</param>
        /// <param name="bone_list">The type of bone, often determining with which other bones the new instance would appear with.</param>
        /// <param name="bhv_children">The optional array of joints, needed for BVH saving, which are the direct decendent(s) of this joint.</param>
        /// <param name="dont_flip_on_mirror">The option for whether this joint should rotation-flip it counterpart when mirroring the pose of the entire body.</param>
        FSPoserJoint(std::string joint_name, std::string mirror_joint_name, E_BoneTypes bone_list,
                     std::vector<std::string> bhv_children = {}, std::string bvhOffset = "", std::string bvhEndSiteValue = "",
                     bool dont_flip_on_mirror = false)
        {
            mJointName           = joint_name;
            mMirrorJointName     = mirror_joint_name;
            mBoneList            = bone_list;
            mBvhChildren         = bhv_children;
            bvhOffsetValue       = bvhOffset;
            mDontFlipOnMirror    = dont_flip_on_mirror;
            bvhEndSiteOffset     = bvhEndSiteValue;
        }
    };

    /// <summary>
    /// An ordered list of poser joints, clustered by body-area.
    /// Order is based on ease-of-use.
    /// Not necessarily exhaustive, just the joints we care to edit without adding UI clutter.
    /// </summary>
    /// <remarks>
    /// For an implementation of something other than LLJoints, different name(s) may be required.
    /// </remarks>
    const std::vector<FSPoserJoint> PoserJoints{
        // head, torso, legs
        { "mHead", "", BODY, { "mEyeLeft", "mEyeRight", "mFaceRoot" }, "0.000 0.076 0.000" },
        { "mNeck", "", BODY, { "mHead" }, "0.000 0.251 -0.010" },
        { "mPelvis", "", WHOLEAVATAR, { "mTorso", "mHipLeft", "mHipRight", "mTail1", "mGroin", "mHindLimbsRoot" }, "0.000000 0.000000 0.000000" },
        { "mChest", "", BODY, { "mNeck", "mCollarLeft", "mCollarRight", "mWingsRoot" }, "0.000 0.205 -0.015" },
        { "mTorso", "", BODY, { "mChest" }, "0.000 0.084 0.000" },
        { "mCollarLeft", "mCollarRight", BODY, { "mShoulderLeft" }, "0.085 0.165 -0.021" },
        { "mShoulderLeft", "mShoulderRight", BODY, { "mElbowLeft" }, "0.079 0.000 0.000" },
        { "mElbowLeft", "mElbowRight", BODY, { "mWristLeft" }, "0.248 0.000 0.000" },
        { "mWristLeft", "mWristRight", BODY, { "mHandThumb1Left", "mHandIndex1Left", "mHandMiddle1Left", "mHandRing1Left", "mHandPinky1Left" }, "0.205 0.000 0.000" },
        { "mCollarRight", "mCollarLeft", BODY, { "mShoulderRight" }, "-0.085 0.165 -0.021", "", true },
        { "mShoulderRight", "mShoulderLeft", BODY, { "mElbowRight" }, "-0.079 0.000 0.000", "", true },
        { "mElbowRight", "mElbowLeft", BODY, { "mWristRight" }, "-0.248 0.000 0.000", "", true },
        { "mWristRight", "mWristLeft", BODY, { "mHandThumb1Right", "mHandIndex1Right", "mHandMiddle1Right", "mHandRing1Right", "mHandPinky1Right" }, "-0.205 0.000 0.000", "", true },
        { "mHipLeft", "mHipRight", BODY, { "mKneeLeft" }, "0.127 -0.041 0.034" },
        { "mKneeLeft", "mKneeRight", BODY, { "mAnkleLeft" }, "-0.046 -0.491 -0.001" },
        { "mAnkleLeft", "mAnkleRight", BODY, {}, "0.001 -0.468 -0.029", "0.000 -0.061 0.112" },
        { "mHipRight", "mHipLeft", BODY, { "mKneeRight" }, "-0.129 -0.041 0.034", "0.000 -0.061 0.112", true },
        { "mKneeRight", "mKneeLeft", BODY, { "mAnkleRight" }, "0.049 -0.491 -0.001", "", true },
        { "mAnkleRight", "mAnkleLeft", BODY, {}, "0.000 -0.468 -0.029", "0.000 -0.061 0.112", true },

        // face
        { "mFaceRoot",
          "",
          FACE,
          {
              "mFaceForeheadLeft", "mFaceForeheadCenter", "mFaceForeheadRight",
              "mFaceEyebrowOuterLeft", "mFaceEyebrowCenterLeft", "mFaceEyebrowInnerLeft",
              "mFaceEyebrowOuterRight", "mFaceEyebrowCenterRight", "mFaceEyebrowInnerRight",
              "mFaceEyeLidUpperLeft", "mFaceEyeLidLowerLeft",
              "mFaceEyeLidUpperRight", "mFaceEyeLidLowerRight",
              "mFaceEar1Left", "mFaceEar1Right",
              "mFaceNoseLeft", "mFaceNoseCenter", "mFaceNoseRight",
              "mFaceCheekUpperLeft", "mFaceCheekLowerLeft",
              "mFaceCheekUpperRight", "mFaceCheekLowerRight",
              "mFaceJaw", "mFaceTeethUpper"
          },
          "0.000 0.045 0.025" },
        { "mFaceForeheadLeft", "mFaceForeheadRight", FACE, {}, "0.035 0.083 0.061", "0.004 0.018 0.024" },
        { "mFaceForeheadCenter", "", FACE, {}, "0.000 0.065 0.069", "0.000 0.000 0.036" },
        { "mFaceForeheadRight", "mFaceForeheadLeft", FACE, {}, "-0.035 0.083 0.061", "-0.004 0.018 0.024", true },
        { "mFaceEyebrowOuterLeft", "mFaceEyebrowOuterRight", FACE, {}, "0.051 0.048 0.064", "0.013 0.000 0.023" },
        { "mFaceEyebrowCenterLeft", "mFaceEyebrowCenterRight", FACE, {}, "0.043 0.056 0.070", "0.000 0.000 0.027" },
        { "mFaceEyebrowInnerLeft", "mFaceEyebrowInnerRight", FACE, {}, "0.022 0.051 0.075", "0.000 0.000 0.026" },
        { "mFaceEyebrowOuterRight", "mFaceEyebrowOuterLeft", FACE, {}, "-0.051 0.048 0.064", "-0.013 0.000 0.023", true },
        { "mFaceEyebrowCenterRight", "mFaceEyebrowCenterLeft", FACE, {}, "-0.043 0.056 0.070", "0.000 0.000 0.027", true },
        { "mFaceEyebrowInnerRight", "mFaceEyebrowInnerLeft", FACE, {}, "-0.022 0.051 0.075", "0.000 0.000 0.026", true },

        { "mEyeLeft", "mEyeRight", FACE, {}, "-0.036 0.079 0.098", "0.000 0.000 0.025" },
        { "mEyeRight", "mEyeLeft", FACE, {}, "0.036 0.079 0.098", "0.000 0.000 0.025", true },
        { "mFaceEyeLidUpperLeft", "mFaceEyeLidUpperRight", FACE, {}, "0.036 0.034 0.073", "0.000 0.005 0.027" },
        { "mFaceEyeLidLowerLeft", "mFaceEyeLidLowerRight", FACE, {}, "0.036 0.034 0.073", "0.000 -0.007 0.024" },
        { "mFaceEyeLidUpperRight", "mFaceEyeLidUpperLeft", FACE, {}, "-0.036 0.034 0.073", "0.000 0.005 0.027", true },
        { "mFaceEyeLidLowerRight", "mFaceEyeLidLowerLeft", FACE, {}, "-0.036 0.034 0.073", "0.000 -0.007 0.024", true },

        { "mFaceEar1Left", "mFaceEar1Right", FACE, { "mFaceEar2Left" }, "0.080 0.002 0.000", "" },
        { "mFaceEar2Left", "mFaceEar2Right", FACE, {}, "0.018 0.025 -0.019", "0.000 0.033 0.000" },
        { "mFaceEar1Right", "mFaceEar1Left", FACE, { "mFaceEar2Right" }, "-0.080 0.002 0.000", "", true },
        { "mFaceEar2Right", "mFaceEar2Left", FACE, {}, "-0.018 0.025 -0.019", "0.000 0.033 0.000", true },
        { "mFaceNoseLeft", "mFaceNoseRight", FACE, {}, "0.015 -0.004 0.086", "0.004 0.000 0.015" },
        { "mFaceNoseCenter", "", FACE, {}, "0.000 0.000 0.102", "0.000 0.000 0.025" },
        { "mFaceNoseRight", "mFaceNoseLeft", FACE, {}, "-0.015 -0.004 0.086", "-0.004 0.000 0.015", true },

        { "mFaceCheekUpperLeft", "mFaceCheekUpperRight", FACE, {}, "0.034 -0.005 0.070", "0.015 0.000 0.022" },
        { "mFaceCheekLowerLeft", "mFaceCheekLowerRight", FACE, {}, "0.034 -0.031 0.050", "0.030 0.000 0.013" },
        { "mFaceCheekUpperRight", "mFaceCheekUpperLeft", FACE, {}, "-0.034 -0.005 0.070", "-0.015 0.000 0.022", true },
        { "mFaceCheekLowerRight", "mFaceCheekLowerLeft", FACE, {}, "-0.034 -0.031 0.050", "-0.030 0.000 0.013", true },
        { "mFaceLipUpperLeft", "mFaceLipUpperRight", FACE, {}, "0.000 -0.003 0.045", "0.015 0.000 0.041" },
        { "mFaceLipUpperCenter", "", FACE, {}, "0.000 -0.003 0.045", "0.000 0.002 0.043" },
        { "mFaceLipUpperRight", "mFaceLipUpperLeft", FACE, {}, "0.000 -0.003 0.045", "-0.015 0.000 0.041", true },
        { "mFaceLipCornerLeft", "mFaceLipCornerRight", FACE, {}, "-0.019 -0.010 0.028", "0.051 0.000 0.045" },
        { "mFaceLipCornerRight", "mFaceLipCornerLeft", FACE, {}, "0.019 -0.010 0.028", "-0.051 0.000 0.045", true },
        { "mFaceTeethUpper", "", FACE, { "mFaceLipUpperLeft","mFaceLipUpperCenter", "mFaceLipUpperRight", "mFaceLipCornerLeft", "mFaceLipCornerRight" }, "0.000 -0.030 0.020" },
        { "mFaceTeethLower", "", FACE, { "mFaceLipLowerLeft", "mFaceLipLowerCenter", "mFaceLipLowerRight", "mFaceTongueBase" }, "0.000 -0.039 0.021" },
        { "mFaceTongueBase", "", FACE, { "mFaceTongueTip" }, "0.000 0.005 0.039" },
        { "mFaceTongueTip", "", FACE, {}, "0.000 0.007 0.022", "0.000 0.000 0.010", true },
        { "mFaceLipLowerLeft", "mFaceLipLowerRight", FACE, {}, "0.000 0.000 0.045", "0.017 0.005 0.034" },
        { "mFaceLipLowerCenter", "", FACE, {}, "0.000 0.000 0.045", "0.000 0.002 0.040" },
        { "mFaceLipLowerRight", "mFaceLipLowerLeft", FACE, {}, "0.000 0.000 0.045", "-0.017 0.005 0.034", true },
        { "mFaceJaw", "", FACE, { "mFaceChin", "mFaceTeethLower" }, "0.000 -0.015 -0.001", "" },
        { "mFaceChin", "", FACE, {}, "0.000 -0.015 -0.001", "0.000 -0.018 0.021" },

        // left hand
        { "mHandThumb1Left", "mHandThumb1Right", HANDS, { "mHandThumb2Left" }, "0.026 0.004 0.031" },
        { "mHandThumb2Left", "mHandThumb2Right", HANDS, { "mHandThumb3Left" }, "0.032 -0.001 0.028" },
        { "mHandThumb3Left", "mHandThumb3Right", HANDS, {}, "0.031 -0.001 0.023", "0.025 0.000 0.015" },
        { "mHandIndex1Left", "mHandIndex1Right", HANDS, { "mHandIndex2Left" }, "0.097 0.015 0.038" },
        { "mHandIndex2Left", "mHandIndex2Right", HANDS, { "mHandIndex3Left" }, "0.036 -0.006 0.017" },
        { "mHandIndex3Left", "mHandIndex3Right", HANDS, {}, "0.032 -0.006 0.014", "0.025 -0.004 0.011" },
        { "mHandMiddle1Left", "mHandMiddle1Right", HANDS, { "mHandMiddle2Left" }, "0.101 0.015 0.013" },
        { "mHandMiddle2Left", "mHandMiddle2Right", HANDS, { "mHandMiddle3Left" }, "0.040 -0.006 -0.001" },
        { "mHandMiddle3Left", "mHandMiddle3Right", HANDS, {}, "0.049 -0.008 -0.001", "0.033 -0.006 -0.002" },
        { "mHandRing1Left", "mHandRing1Right", HANDS, { "mHandRing2Left" }, "0.099 0.009 -0.010" },
        { "mHandRing2Left", "mHandRing2Right", HANDS, { "mHandRing3Left" }, "0.038 -0.008 -0.013" },
        { "mHandRing3Left", "mHandRing3Right", HANDS, {}, "0.040 -0.009 -0.013", "0.028 -0.006 -0.010" },
        { "mHandPinky1Left", "mHandPinky1Right", HANDS, { "mHandPinky2Left" }, "0.095 0.003 -0.031" },
        { "mHandPinky2Left", "mHandPinky2Right", HANDS, { "mHandPinky3Left" }, "0.025 -0.006 -0.024" },
        { "mHandPinky3Left", "mHandPinky3Right", HANDS, {}, "0.018 -0.004 -0.015", "0.016 -0.004 -0.013" },

        // right hand
        { "mHandThumb1Right", "mHandThumb1Left", HANDS, { "mHandThumb2Right" }, "-0.026 0.004 0.031", "", true },
        { "mHandThumb2Right", "mHandThumb2Left", HANDS, { "mHandThumb3Right" }, "-0.032 -0.001 0.028", "", true },
        { "mHandThumb3Right", "mHandThumb3Left", HANDS, {}, "-0.031 -0.001 0.023", "-0.025 0.000 0.015", true },
        { "mHandIndex1Right", "mHandIndex1Left", HANDS, { "mHandIndex2Right" }, "-0.097 0.015 0.038", "", true },
        { "mHandIndex2Right", "mHandIndex2Left", HANDS, { "mHandIndex3Right" }, "-0.036 -0.006 0.017", "", true },
        { "mHandIndex3Right", "mHandIndex3Left", HANDS, {}, "-0.032 -0.006 0.014", "-0.025 -0.004 0.011", true },
        { "mHandMiddle1Right", "mHandMiddle1Left", HANDS, { "mHandMiddle2Right" }, "-0.101 0.015 0.013", "", true },
        { "mHandMiddle2Right", "mHandMiddle2Left", HANDS, { "mHandMiddle3Right" }, "-0.040 -0.006 -0.001", "", true },
        { "mHandMiddle3Right", "mHandMiddle3Left", HANDS, {}, "-0.049 -0.008 -0.001", "-0.033 -0.006 -0.002", true },
        { "mHandRing1Right", "mHandRing1Left", HANDS, { "mHandRing2Right" }, "-0.099 0.009 -0.010", "", true },
        { "mHandRing2Right", "mHandRing2Left", HANDS, { "mHandRing3Right" }, "-0.038 -0.008 -0.013", "", true },
        { "mHandRing3Right", "mHandRing3Left", HANDS, {}, "-0.040 -0.009 -0.013", "-0.028 -0.006 -0.010", true },
        { "mHandPinky1Right", "mHandPinky1Left", HANDS, { "mHandPinky2Right" }, "-0.095 0.003 -0.031", "", true },
        { "mHandPinky2Right", "mHandPinky2Left", HANDS, { "mHandPinky3Right" }, "-0.025 -0.006 -0.024", "", true },
        { "mHandPinky3Right", "mHandPinky3Left", HANDS, {}, "-0.018 -0.004 -0.015", "-0.016 -0.004 -0.013", true },

        // tail and hind limbs
        { "mTail1", "", MISC, { "mTail2" }, "0.000 0.047 -0.116" },
        { "mTail2", "", MISC, { "mTail3" }, "0.000 0.000 -0.197" },
        { "mTail3", "", MISC, { "mTail4" }, "0.000 0.000 -0.168" },
        { "mTail4", "", MISC, { "mTail5" }, "0.000 0.000 -0.142" },
        { "mTail5", "", MISC, { "mTail6" }, "0.000 0.000 -0.112" },
        { "mTail6", "", MISC, {}, "0.000 0.000 -0.094", "0.000 0.000 -0.089" },
        { "mGroin", "", MISC, {}, "0.000 -0.097 0.064", "0.000 -0.066 0.004" },
        { "mHindLimbsRoot", "", MISC, { "mHindLimb1Left", "mHindLimb1Right" }, "0.000 0.084 -0.200" },
        { "mHindLimb1Left", "mHindLimb1Right", MISC, { "mHindLimb2Left" }, "0.129 -0.125 -0.204" },
        { "mHindLimb2Left", "mHindLimb2Right", MISC, { "mHindLimb3Left" }, "-0.046 -0.491 0.002" },
        { "mHindLimb3Left", "mHindLimb3Right", MISC, { "mHindLimb4Left" }, "-0.003 -0.468 -0.030" },
        { "mHindLimb4Left", "mHindLimb4Right", MISC, {}, "0.000 -0.061 0.112", "0.008 0.000 0.105" },
        { "mHindLimb1Right", "mHindLimb1Left", MISC, { "mHindLimb2Right" }, "-0.129 -0.125 -0.204", "", true },
        { "mHindLimb2Right", "mHindLimb2Left", MISC, { "mHindLimb3Right" }, "0.046 -0.491 0.002", "", true },
        { "mHindLimb3Right", "mHindLimb3Left", MISC, { "mHindLimb4Right" }, "0.003 -0.468 -0.030", "", true },
        { "mHindLimb4Right", "mHindLimb4Left", MISC, {}, "0.000 -0.061 0.112", "-0.008 0.000 0.105", true },

        // wings
        { "mWingsRoot", "", MISC, { "mWing1Left", "mWing1Right" }, "0.000 0.000 -0.014" },
        { "mWing1Left", "mWing1Right", MISC, { "mWing2Left" }, "0.105 0.181 -0.099" },
        { "mWing2Left", "mWing2Right", MISC, { "mWing3Left" }, "0.169 0.067 -0.168" },
        { "mWing3Left", "mWing3Right", MISC, { "mWing4Left", "mWing4FanLeft" }, "0.183 0.000 -0.181" },
        { "mWing4Left", "mWing4Right", MISC, {}, "0.173 0.000 -0.171", "0.132 0.000 -0.146" },
        { "mWing4FanLeft", "mWing4FanRight", MISC, {}, "0.173 0.000 -0.171", "0.062 -0.159 -0.068" },
        { "mWing1Right", "mWing1Left", MISC, { "mWing2Right" }, "-0.105 0.181 -0.099", "", true },
        { "mWing2Right", "mWing2Left", MISC, { "mWing3Right" }, "-0.169 0.067 -0.168", "", true },
        { "mWing3Right", "mWing3Left", MISC, { "mWing4Right", "mWing4FanRight" }, "-0.183 0.000 -0.181", "", true },
        { "mWing4Right", "mWing4Left", MISC, {}, "-0.173 0.000 -0.171", "-0.132 0.000 -0.146", true },
        { "mWing4FanRight", "mWing4FanLeft", MISC, {}, "-0.173 0.000 -0.171", "-0.062 -0.159 -0.068", true },

        // Collision Volumes
        { "LEFT_PEC", "RIGHT_PEC", COL_VOLUMES },
        { "RIGHT_PEC", "LEFT_PEC", COL_VOLUMES, {}, "", "", true },
        { "BELLY", "", COL_VOLUMES },
        { "BUTT", "", COL_VOLUMES },
    };
    
public:
    /// <summary>
    /// Get a PoserJoint case-insensitive-matching the supplied name.
    /// </summary>
    /// <param name="jointName">The name of the joint to match.</param>
    /// <returns>The matching joint if found, otherwise nullptr</returns>
    const FSPoserJoint* getPoserJointByName(const std::string& jointName);

    /// <summary>
    /// Tries to start posing the supplied avatar.
    /// </summary>
    /// <param name="avatar">The avatar to begin posing.</param>
    /// <returns>True if the avatar was able to begin posing, otherwise false.</returns>
    bool tryPosingAvatar(LLVOAvatar* avatar);

    /// <summary>
    /// Stops posing the supplied avatar.
    /// </summary>
    /// <param name="avatar">The avatar to stop posing.</param>
    void stopPosingAvatar(LLVOAvatar* avatar);

    /// <summary>
    /// Determines if the supplied avatar is being posed by this.
    /// </summary>
    /// <param name="avatar">The avatar to query posing status for.</param>
    /// <returns>True if this is posing the supplied avatar, otherwise false.</returns>
    bool isPosingAvatar(LLVOAvatar* avatar) const;

    /// <summary>
    /// Determines whether the supplied PoserJoint for the supplied avatar is being posed.
    /// </summary>
    /// <param name="avatar">The avatar having the joint to which we refer.</param>
    /// <param name="joint">The joint being queried for.</param>
    /// <returns>True if this is joint is being posed for the supplied avatar, otherwise false.</returns>
    bool isPosingAvatarJoint(LLVOAvatar* avatar, const FSPoserJoint& joint);

    /// <summary>
    /// Sets whether the supplied PoserJoint for the supplied avatar should be posed.
    /// </summary>
    /// <param name="avatar">The avatar having the joint to which we refer.</param>
    /// <param name="joint">The joint being queried for.</param>
    /// <param name="posing">Whether the joint should be posed, or not.</param>
    /// <remarks>
    /// If this is not posing the joint, then it is free to be posed by other things.
    /// </remarks>
    void setPosingAvatarJoint(LLVOAvatar* avatar, const FSPoserJoint& joint, bool shouldPose);

    /// <summary>
    /// Resets the supplied PoserJoint to its position/rotation/scale it was when poser was started.
    /// </summary>
    /// <param name="avatar">The avatar having the joint to which we refer.</param>
    /// <param name="joint">The joint to be reset.</param>
    void resetAvatarJoint(LLVOAvatar* avatar, const FSPoserJoint& joint);

    /// <summary>
    /// Undoes the last applied rotation to the supplied PoserJoint.
    /// </summary>
    /// <param name="avatar">The avatar having the joint to which we refer.</param>
    /// <param name="joint">The joint with the rotation to undo.</param>
    void undoLastJointRotation(LLVOAvatar* avatar, const FSPoserJoint& joint, E_BoneDeflectionStyles style);

    /// <summary>
    /// Undoes the last applied position to the supplied PoserJoint.
    /// </summary>
    /// <param name="avatar">The avatar having the joint to which we refer.</param>
    /// <param name="joint">The joint with the position to undo.</param>
    void undoLastJointPosition(LLVOAvatar* avatar, const FSPoserJoint& joint, E_BoneDeflectionStyles style);

    /// <summary>
    /// Undoes the last applied scale to the supplied PoserJoint.
    /// </summary>
    /// <param name="avatar">The avatar having the joint to which we refer.</param>
    /// <param name="joint">The joint with the scale to undo.</param>
    void undoLastJointScale(LLVOAvatar* avatar, const FSPoserJoint& joint, E_BoneDeflectionStyles style);

    /// <summary>
    /// Resets the position of the supplied PoserJoint.
    /// </summary>
    /// <param name="avatar">The avatar having the joint to which we refer.</param>
    /// <param name="joint">The joint with the position to reset.</param>
    void resetJointPosition(LLVOAvatar* avatar, const FSPoserJoint& joint, E_BoneDeflectionStyles style);

    /// <summary>
    /// Resets the scale of the supplied joint to initial values.
    /// </summary>
    /// <param name="avatar">The avatar having the joint to which we refer.</param>
    /// <param name="joint">The joint with the scale to reset.</param>
    void resetJointScale(LLVOAvatar* avatar, const FSPoserJoint& joint, E_BoneDeflectionStyles style);

    /// <summary>
    /// Determines if a redo action is currently permitted for the supplied joint.
    /// </summary>
    /// <param name="avatar">The avatar having the joint to which we refer.</param>
    /// <param name="joint">The joint to query.</param>
    /// <returns>True if a redo action is available, otherwise false.</returns>
    bool canRedoJointRotation(LLVOAvatar* avatar, const FSPoserJoint& joint);

    /// <summary>
    /// Re-does the last undone rotation to the supplied PoserJoint.
    /// </summary>
    /// <param name="avatar">The avatar having the joint to which we refer.</param>
    /// <param name="joint">The joint with the rotation to redo.</param>
    void redoLastJointRotation(LLVOAvatar* avatar, const FSPoserJoint& joint, E_BoneDeflectionStyles style);

    /// <summary>
    /// Re-does the last undone position to the supplied PoserJoint.
    /// </summary>
    /// <param name="avatar">The avatar having the joint to which we refer.</param>
    /// <param name="joint">The joint with the position to redo.</param>
    void redoLastJointPosition(LLVOAvatar* avatar, const FSPoserJoint& joint, E_BoneDeflectionStyles style);

    /// <summary>
    /// Re-does the last undone scale to the supplied PoserJoint.
    /// </summary>
    /// <param name="avatar">The avatar having the joint to which we refer.</param>
    /// <param name="joint">The joint with the scale to redo.</param>
    void redoLastJointScale(LLVOAvatar* avatar, const FSPoserJoint& joint, E_BoneDeflectionStyles style);

    /// <summary>
    /// Gets the position of a joint for the supplied avatar.
    /// </summary>
    /// <param name="avatar">The avatar whose joint is being queried.</param>
    /// <param name="joint">The joint to determine the position for.</param>
    /// <returns>The position of the requested joint, if determinable, otherwise a default vector.</returns>
    LLVector3 getJointPosition(LLVOAvatar* avatar, const FSPoserJoint& joint) const;

    /// <summary>
    /// Sets the position of a joint for the supplied avatar.
    /// </summary>
    /// <param name="avatar">The avatar whose joint is to be set.</param>
    /// <param name="joint">The joint to set.</param>
    /// <param name="position">The position to set the joint to.</param>
    /// <param name="style">Any ancilliary action to be taken with the change to be made.</param>
    void setJointPosition(LLVOAvatar* avatar, const FSPoserJoint* joint, const LLVector3& position, E_BoneDeflectionStyles style);

    /// <summary>
    /// Gets the rotation of a joint for the supplied avatar.
    /// </summary>
    /// <param name="avatar">The avatar whose joint is being queried.</param>
    /// <param name="joint">The joint to determine the rotation for.</param>
    /// <param name="translation">The joint to determine the rotation for.</param>
    /// <param name="negation">The style of negation to dis-apply to the get.</param>
    /// <param name="rotType">The type of rotation to get from the supplied joint for the supplied avatar.</param>
    /// <returns>The rotation of the requested joint, if determinable, otherwise a default vector.</returns>
    LLVector3 getJointRotation(LLVOAvatar* avatar, const FSPoserJoint& joint, E_BoneAxisTranslation translation, S32 negation) const;

    /// <summary>
    /// Sets the rotation of a joint for the supplied avatar.
    /// </summary>
    /// <param name="avatar">The avatar whose joint is to be set.</param>
    /// <param name="joint">The joint to set.</param>
    /// <param name="absRotation">The absolute rotation to apply to the joint, if appropriate.</param>
    /// <param name="deltaRotation">The delta of rotation to apply to the joint, if appropriate.</param>
    /// <param name="style">Any ancilliary action to be taken with the change to be made.</param>
    /// <param name="translation">The axial translation form the supplied joint.</param>
    /// <param name="negation">The style of negation to apply to the set.</param>
    /// <param name="resetBaseRotationToZero">Whether to set the base rotation to zero on setting the rotation.</param>
    /// <param name="rotationStyle">Whether to apply the supplied rotation as a delta to the supplied joint.</param>
    void setJointRotation(LLVOAvatar* avatar, const FSPoserJoint* joint, const LLVector3& absRotation, const LLVector3& deltaRotation, E_BoneDeflectionStyles style,
                          E_BoneAxisTranslation translation, S32 negation, bool resetBaseRotationToZero, E_RotationStyle rotationStyle);

    /// <summary>
    /// Gets the scale of a joint for the supplied avatar.
    /// </summary>
    /// <param name="avatar">The avatar whose joint is being queried.</param>
    /// <param name="joint">The joint to determine the scale for.</param>
    /// <returns>The scale of the requested joint, if determinable, otherwise a default vector.</returns>
    LLVector3 getJointScale(LLVOAvatar* avatar, const FSPoserJoint& joint) const;

    /// <summary>
    /// Sets the scale of a joint for the supplied avatar.
    /// </summary>
    /// <param name="avatar">The avatar whose joint is to be set.</param>
    /// <param name="joint">The joint to set.</param>
    /// <param name="scale">The scale to set the joint to.</param>
    /// <param name="style">Any ancilliary action to be taken with the change to be made.</param>
    void setJointScale(LLVOAvatar* avatar, const FSPoserJoint* joint, const LLVector3& scale, E_BoneDeflectionStyles style);

    /// <summary>
    /// Reflects the joint with its opposite if it has one, or just mirror the rotation of itself.
    /// </summary>
    /// <param name="avatar">The avatar whose joint should flip left-right.</param>
    /// <param name="joint">The joint to mirror rotation for.</param>
    void reflectJoint(LLVOAvatar* avatar, const FSPoserJoint* joint);

    /// <summary>
    /// Reflects every joint of the supplied avatar with its opposite if it has one, or mirrors the rotation of the joint if it does not have an opposite.
    /// </summary>
    /// <param name="avatar">The avatar whose pose should flip left-right.</param>
    void flipEntirePose(LLVOAvatar* avatar);

    /// <summary>
    /// Recaptures the rotation, position and scale state of the supplied joint for the supplied avatar.
    /// </summary>
    /// <param name="avatar">The avatar whose joint is to be recaptured.</param>
    /// <param name="joint">The joint to recapture.</param>
    /// <param name="translation">The axial translation form the supplied joint.</param>
    /// <param name="negation">The style of negation to apply to the recapture.</param>
    void recaptureJoint(LLVOAvatar* avatar, const FSPoserJoint& joint, E_BoneAxisTranslation translation, S32 negation);

    /// <summary>
    /// Sets all of the joint rotations of the supplied avatar to zero.
    /// </summary>
    /// <param name="avatar">The avatar whose joint rotations should be set to zero.</param>
    void setAllAvatarStartingRotationsToZero(LLVOAvatar* avatar);

    /// <summary>
    /// Determines if the kind of save to perform should be a 'delta' save, or a complete save.
    /// </summary>
    /// <param name="avatar">The avatar whose pose-rotations are being considered for saving.</param>
    /// <returns>True if the save should save only 'deltas' to the rotation, otherwise false.</returns>
    /// <remarks>
    /// A save of the rotation 'deltas' facilitates a user saving their changes to an existing animation.
    /// Thus the save represents 'nothing other than the changes the user made', to some other pose which they may have limited rights to.
    /// </remarks>
    bool baseRotationIsZero(LLVOAvatar* avatar, const FSPoserJoint& joint) const;

    /// <summary>
    /// Determines if the kind of save to perform should be a 'delta' save, or a complete save.
    /// </summary>
    /// <param name="avatar">The avatar whose pose-rotations are being considered for saving.</param>
    /// <returns>True if the save should save only 'deltas' to the rotation, otherwise false.</returns>
    /// <remarks>
    /// A save of the rotation 'deltas' facilitates a user saving their changes to an existing animation.
    /// Thus the save represents 'nothing other than the changes the user made', to some other pose which they may have limited rights to.
    /// </remarks>
    bool allBaseRotationsAreZero(LLVOAvatar* avatar) const;

    /// <summary>
    /// Tries to get the rotation, position and scale changes from initial conditions, to save in some export container.
    /// </summary>
    /// <param name="avatar">The avatar whose pose is being considered for saving.</param>
    /// <param name="joint">The joint we are considering the save for.</param>
    /// <param name="rot">The quaternion to store the rotation to save in.</param>
    /// <param name="pos">The vector to store the position to save in.</param>
    /// <param name="scale">The vector to store the scale to save in.</param>
    /// <param name="baseRotationIsZero">The bool to store whether the base rotation is zero.</param>
    /// <returns>True if the joint should be saved, otherwise false.</returns>
    /// <remarks>
    /// Our objective is to protect peoples novel work: the poses created with this, and poses from other sources, such as in-world.
    /// In all scenarios, this yeilds 'deltas' of rotation/position/scale.
    /// The deltas represent the user's novel work, and may be relative to some initial values (as from a pose), or to 'nothing' (such as all rotations == 0, or, the 'T-Pose').
    /// </remarks>
    bool tryGetJointSaveVectors(LLVOAvatar* avatar, const FSPoserJoint& joint, LLVector3* rot, LLVector3* pos, LLVector3* scale, bool* baseRotationIsZero);

    /// <summary>
    /// Loads a joint rotation for the supplied joint on the supplied avatar.
    /// </summary>
    /// <param name="avatar">The avatar to load the rotation for.</param>
    /// <param name="joint">The joint to load the rotation for.</param>
    /// <param name="setBaseToZero">Whether to start from a zero base rotation.</param>
    /// <param name="rotation">The rotation to load.</param>
    /// <remarks>
    /// All rotations we load are deltas to the current rotation the supplied joint has.
    /// Whether the joint already has a rotation because some animation is playing,
    /// or whether its rotation is zero, the result is always the same: just 'add' the supplied rotation to the existing rotation.
    /// </remarks>
    void loadJointRotation(LLVOAvatar* avatar, const FSPoserJoint* joint, bool setBaseToZero, LLVector3 rotation);

    /// <summary>
    /// Loads a joint position for the supplied joint on the supplied avatar.
    /// </summary>
    /// <param name="avatar">The avatar to load the position for.</param>
    /// <param name="joint">The joint to load the position for.</param>
    /// <param name="loadPositionAsDelta">Whether to the supplied position as a delta to the current position, or not.</param>
    /// <param name="position">The Position to apply to the supplied joint.</param>
    /// <remarks>
    /// A position is saved as an absolute if the user created the pose from 'scratch' (at present the 'T-Pose').
    /// Otherwise the position is saved as a delta.
    /// The primary purpose is aesthetic: the numbers inside of a 'delta save file' have 'zeros everywhere'.
    /// A delta-save thus accurately reflects what the user changed, and not what the original pose is.
    /// 'Legacy' (pre save format version-4) poses we expect to load as absolutes.
    /// </remarks>
    void loadJointPosition(LLVOAvatar* avatar, const FSPoserJoint* joint, bool loadPositionAsDelta, LLVector3 position);

    /// <summary>
    /// Loads a joint scale for the supplied joint on the supplied avatar.
    /// </summary>
    /// <param name="avatar">The avatar to load the scale for.</param>
    /// <param name="joint">The joint to load the scale for.</param>
    /// <param name="loadScaleAsDelta">Whether to the supplied scale as a delta to the current scale, or not.</param>
    /// <param name="scale">The scale to apply to the supplied joint.</param>
    /// <remarks>
    /// A scale is saved as an absolute if the user created the pose from 'scratch' (at present the 'T-Pose').
    /// Otherwise the scale is saved as a delta.
    /// The primary purpose is somewhat aesthetic: the numbers inside of a 'pose modification XML' has zeros everywhere.
    /// A delta-save thus accurately reflects what the user changed, and not what the original creator of the modified pose specified.
    /// </remarks>
    void loadJointScale(LLVOAvatar* avatar, const FSPoserJoint* joint, bool loadScaleAsDelta, LLVector3 scale);

  private:
    /// <summary>
    /// Translates a rotation vector from the UI to a Quaternion for the bone.
    /// This also performs the axis-swapping the UI needs for up/down/left/right to make sense.
    /// </summary>
    /// <param name="translation">The axis translation to perform.</param>
    /// <param name="rotation">The rotation to transform to quaternion.</param>
    /// <returns>The rotation quaternion.</returns>
    LLQuaternion translateRotationToQuaternion(E_BoneAxisTranslation translation, S32 negation, LLVector3 rotation);

    /// <summary>
    /// Translates a bone-rotation quaternion to a vector usable easily on the UI.
    /// </summary>
    /// <param name="translation">The axis translation to perform.</param>
    /// <param name="rotation">The rotation to transform to matrix.</param>
    /// <returns>The rotation vector.</returns>
    LLVector3 translateRotationFromQuaternion(E_BoneAxisTranslation translation, S32 negation, const LLQuaternion& rotation) const;

    /// <summary>
    /// Creates a posing motion for the supplied avatar.
    /// </summary>
    /// <param name="avatar">The avatar to create the posing motion for.</param>
    /// <returns>The posing motion, if created, otherwise nullptr.</returns>
    /// <remarks>
    /// When a pose is created for the avatar, it is 'registered' with their character for use later on.
    /// Thus we start & stop posing the same animation.
    /// </remarks>
    FSPosingMotion* findOrCreatePosingMotion(LLVOAvatar* avatar);

    /// <summary>
    /// Gets the poser posing-motion for the supplied avatar.
    /// </summary>
    /// <param name="avatar">The avatar to get the posing motion for.</param>
    /// <returns>The posing motion if found, otherwise nullptr.</returns>
    FSPosingMotion* getPosingMotion(LLVOAvatar* avatar) const;

    /// <summary>
    /// Determines if the avatar can be used.
    /// </summary>
    /// <param name="avatar">The avatar to test if it is safe to animate.</param>
    /// <returns>True if the avatar is safe to manipulate, otherwise false.</returns>
    bool isAvatarSafeToUse(LLVOAvatar* avatar) const;

    /// <summary>
    /// Maps the avatar's ID to the animation registered to them.
    /// Thus we start/stop the same animation, and get/set the same rotations etc.
    /// Among other things this provides for the 'undo' of changes to shape/position when the poser stops animating someone.
    /// An avatar's animation exists so long as their session does, and there is consideration for renewal (like if they relog/crash and their character is renewed).
    /// Is static, so the animationId is not lost between sessions (such as when the UI floater is closed and reopened).
    /// </summary>
    static std::map<LLUUID, LLAssetID> sAvatarIdToRegisteredAnimationId;
};

#endif // LL_FSPoserAnimator_H
