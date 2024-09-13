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

#include "llmotion.h"
#include "lljointsolverrp3.h"
#include "v3dmath.h"
#include "llcontrolavatar.h"

/// <summary>
/// Describes how we will cluster the joints/bones/thingos.
/// Each joint/bone/thingo should have one of these, <see:"FSPoserAnimator.PoserJoints"/>.
/// </summary>
typedef enum E_BoneTypes
{
    WHOLEAVATAR = 0,  // possibly a single instance of, but this one manipulates everything
    BODY        = 1,
    FACE        = 2,
    HANDS       = 3,
    MISC        = 4
} E_BoneTypes;

/// <summary>
/// When we're adjusting a bone/joint/thingo we may want to do something else simultaneously.
/// This describes the other things we might do: eg: mirror the change to the opposite joint.
/// </summary>
typedef enum E_BoneDeflectionStyles
{
    NONE        = 0,  // do nothing additional
    MIRROR      = 1,  // change the other joint, like in a mirror, eg: one left one right
    SYMPATHETIC = 2,  // change the other joint, but opposite to a mirrored way, eg: both go right or both go left
} E_BoneDeflectionStyles;

/// <summary>
/// When we're going from bone-rotation to the UI sliders, some of the axes need swapping so they make sense in UI-terms.
/// eg: for one bone, the X-axis may mean up and down, but for another bone, the x-axis might be left-right.
/// These are translations of bone-TO-UI; an inverse translation needs to happen the other way.
/// It would be nice if these were defined in XML; but then they're not const, and I want const.
/// </summary>
typedef enum E_BoneAxisTranslation
{
    SWAP_NOTHING        = 0,
    SWAP_YAW_AND_ROLL   = 1,
    SWAP_YAW_AND_PITCH  = 2,
    SWAP_ROLL_AND_PITCH = 3,
} E_BoneAxisTranslation;

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
    FSPoserAnimator();

    virtual ~FSPoserAnimator();

    /// <summary>
    /// A class encapsulating 'metadata' for a joint, such as its catagory and its opposite joint name.
    /// </summary>
    class FSPoserJoint
    {
        std::string _jointName; // expected to be a match to LLJoint.getName() for a joint implementation.
        std::string _mirrorJointName;
        E_BoneTypes _boneList;
      public:
        /// <summary>
        /// Gets the name of the joint.
        /// </summary>
        std::string jointName() const { return _jointName; }

        /// <summary>
        /// Gets the name of the mirror of this joint, or an empty string if there is no mirror.
        /// </summary>
        std::string mirrorJointName() const { return _mirrorJointName; }

        /// <summary>
        /// Gets the E_BoneTypes of the joint.
        /// </summary>
        E_BoneTypes boneType() const { return _boneList; }

        /// <summary>
        /// Creates a new instance of a PoserJoint.
        /// </summary>
        /// <param name="a">
        /// The joint name, should be one of the well known bones/joints/thingos.
        /// An example for an LLJoints implementation would be what LLJoint.getName() returns, like 'mChest'.
        /// Very likely case-sensitive.
        /// </param>
        /// <param name="b">The opposite joint name, if any. Also expected to be a well-known name.</param>
        /// <param name="c">The </param>
        FSPoserJoint(std::string a, std::string b, E_BoneTypes c)
        {
            _jointName       = a;
            _mirrorJointName = b;
            _boneList        = c;
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
    const std::vector<FSPoserJoint> PoserJoints {
        // head, torso, legs
        {"mPelvis", "", WHOLEAVATAR}, {"mTorso", "", BODY}, {"mChest", "", BODY}, {"mNeck", "", BODY}, {"mHead", "", BODY},
        {"mCollarLeft", "mCollarRight", BODY}, {"mShoulderLeft", "mShoulderRight", BODY}, {"mElbowLeft", "mElbowRight", BODY}, {"mWristLeft", "mWristRight", BODY},
        {"mCollarRight", "mCollarLeft", BODY}, {"mShoulderRight", "mShoulderLeft", BODY},  {"mElbowRight", "mElbowLeft", BODY},  {"mWristRight", "mWristLeft", BODY},
        {"mHipLeft", "", BODY}, {"mKneeLeft", "", BODY},  {"mAnkleLeft", "", BODY},
        {"mHipRight", "", BODY}, {"mKneeRight", "", BODY},  {"mAnkleRight", "", BODY},

        // face
        {"mFaceForeheadLeft", "mFaceForeheadRight", FACE}, {"mFaceForeheadCenter", "", FACE},  {"mFaceForeheadRight", "mFaceForeheadLeft", FACE},
        {"mFaceEyeBrowOuterLeft", "mFaceEyeBrowOuterRight", FACE}, {"mFaceEyeBrowCenterLeft", "mFaceEyeBrowCenterRight", FACE},  {"mFaceEyeBrowInnerLeft", "mFaceEyeBrowInnerRight", FACE},
        {"mFaceEyeBrowOuterRight", "mFaceEyeBrowOuterLeft", FACE}, {"mFaceEyeBrowCenterRight", "mFaceEyeBrowCenterLeft", FACE},  {"mFaceEyeBrowInnerRight", "mFaceEyeBrowInnerLeft", FACE},

        {"mEyeLeft", "mEyeRight", FACE}, {"mEyeRight", "mEyeLeft", FACE},
        {"mFaceEyeLidUpperLeft", "mFaceEyeLidUpperRight", FACE}, {"mFaceEyeLidLowerLeft", "mFaceEyeLidLowerRight", FACE}, {"mFaceEyeLidUpperRight", "mFaceEyeLidUpperLeft", FACE}, {"mFaceEyeLidLowerRight", "mFaceEyeLidLowerLeft", FACE},

        {"mFaceCheekUpperLeft", "mFaceCheekUpperRight", FACE}, {"mFaceCheekLowerLeft", "mFaceCheekLowerRight", FACE}, {"mFaceCheekUpperRight", "mFaceCheekUpperLeft", FACE}, {"mFaceCheekLowerRight", "mFaceCheekLowerLeft", FACE},
        {"mFaceLipUpperLeft", "mFaceLipUpperRight", FACE}, {"mFaceLipUpperCenter", "", FACE}, {"mFaceLipUpperRight", "mFaceLipUpperLeft", FACE},
        {"mFaceLipCornerLeft", "mFaceLipCornerRight", FACE}, {"mFaceLipCornerRight", "mFaceLipCornerLeft", FACE},
        {"mFaceLipLowerLeft", "mFaceLipLowerRight", FACE}, {"mFaceLipLowerCenter", "", FACE}, {"mFaceLipLowerRight", "mFaceLipLowerLeft", FACE},
        {"mFaceJaw", "", FACE},

        //left hand
        {"mHandThumb1Left", "mHandThumb1Right", HANDS}, {"mHandThumb2Left", "mHandThumb2Right", HANDS}, {"mHandThumb3Left", "mHandThumb3Right", HANDS},
        {"mHandIndex1Left", "mHandIndex1Right", HANDS}, {"mHandIndex2Left", "mHandIndex2Right", HANDS}, {"mHandIndex3Left", "mHandIndex3Right", HANDS},
        {"mHandMiddle1Left", "mHandMiddle1Right", HANDS}, {"mHandMiddle2Left", "mHandMiddle2Right", HANDS}, {"mHandMiddle3Left", "mHandMiddle3Right", HANDS},
        {"mHandRing1Left", "mHandRing1Right", HANDS}, {"mHandRing2Left", "mHandRing2Right", HANDS}, {"mHandRing3Left", "mHandRing3Right", HANDS},
        {"mHandPinky1Left", "mHandPinky1Right", HANDS}, {"mHandPinky2Left", "mHandPinky2Right", HANDS}, {"mHandPinky3Left", "mHandPinky3Right", HANDS},

        // right hand
        {"mHandThumb1Right", "mHandThumb1Left", HANDS}, {"mHandThumb2Right", "mHandThumb2Left", HANDS}, {"mHandThumb3Right", "mHandThumb3Left", HANDS},
        {"mHandIndex1Right", "mHandIndex1Left", HANDS}, {"mHandIndex2Right", "mHandIndex2Left", HANDS}, {"mHandIndex3Right", "mHandIndex3Left", HANDS},
        {"mHandMiddle1Right", "mHandMiddle1Left", HANDS}, {"mHandMiddle2Right", "mHandMiddle2Left", HANDS}, {"mHandMiddle3Right", "mHandMiddle3Left", HANDS},
        {"mHandRing1Right", "mHandRing1Left", HANDS}, {"mHandRing2Right", "mHandRing2Left", HANDS}, {"mHandRing3Right", "mHandRing3Left", HANDS},
        {"mHandPinky1Right", "mHandPinky1Left", HANDS}, {"mHandPinky2Right", "mHandPinky2Left", HANDS}, {"mHandPinky3Right", "mHandPinky3Left", HANDS},

        // tail and hind limbs
        {"mTail1", "", MISC}, {"mTail2", "", MISC}, {"mTail3", "", MISC}, {"mTail4", "", MISC}, {"mTail5", "", MISC}, {"mTail6", "", MISC}, {"mGroin", "", MISC},
        {"mHindLimbsRoot", "", MISC},
        {"mHindLimb1Left", "mHindLimb1Right", MISC}, {"mHindLimb2Left", "mHindLimb2Right", MISC}, {"mHindLimb3Left", "mHindLimb3Right", MISC}, {"mHindLimb4Left", "mHindLimb4Right", MISC},
        {"mHindLimb1Right", "mHindLimb1Left", MISC}, {"mHindLimb2Right", "mHindLimb2Left", MISC}, {"mHindLimb3Right", "mHindLimb3Left", MISC}, {"mHindLimb4Right", "mHindLimb4Left", MISC},

        // wings
        {"mWingsRoot", "", MISC},
        {"mWing1Left", "mWing1Right", MISC}, {"mWing2Left", "mWing2Right", MISC}, {"mWing3Left", "mWing3Right", MISC}, {"mWing4Left", "mWing4Right", MISC}, {"mWing4FanLeft", "mWing4FanRight", MISC},
        {"mWing1Right", "mWing1Left", MISC}, {"mWing2Right", "mWing2Left", MISC}, {"mWing3Right", "mWing3Left", MISC}, {"mWing4Right", "mWing4Left", MISC}, {"mWing4FanRight", "mWing4FanLeft", MISC},
    };
    
public:
    /// <summary>
    /// Get a PoserJoint case-insensitive-matching the supplied name.
    /// </summary>
    /// <param name="jointName">The name of the joint to match.</param>
    /// <returns>The matching joint if found, otherwise nullptr</returns>
    const FSPoserJoint* getPoserJointByName(std::string jointName);

    /// <summary>
    /// Tries to being posing the supplied avatar.
    /// </summary>
    /// <param name="avatar">The avatar to begin posing.</param>
    /// <returns>True if the avatar was able to begin posing, otherwise false.</returns>
    bool tryPosingAvatar(LLVOAvatar *avatar);

    /// <summary>
    /// Stops posing the supplied avatar.
    /// </summary>
    /// <param name="avatar">The avatar to stop posing.</param>
    void stopPosingAvatar(LLVOAvatar *avatar);

    /// <summary>
    /// Determines if the supplied avatar is being posed by this.
    /// </summary>
    /// <param name="avatar">The avatar to query posing status for.</param>
    /// <returns>True if this is posing the supplied avatar, otherwise false.</returns>
    bool isPosingAvatar(LLVOAvatar *avatar);

    /// <summary>
    /// Determines whether the supplied PoserJoint for the supplied avatar is being posed.
    /// </summary>
    /// <param name="avatar">The avatar having the joint to which we refer.</param>
    /// <param name="joint">The joint being queried for.</param>
    /// <returns></returns>
    bool isPosingAvatarJoint(LLVOAvatar *avatar, FSPoserJoint joint);

    /// <summary>
    /// Sets whether the supplied PoserJoint for the supplied avatar shoubd be posed.
    /// </summary>
    /// <param name="avatar">The avatar having the joint to which we refer.</param>
    /// <param name="joint">The joint being queried for.</param>
    /// <param name="posing">Whether the joint should be posed.</param>
    /// <remarks>
    /// If this is not posing the joint, then it is free to be posed by other things.
    /// </remarks>
    void setPosingAvatarJoint(LLVOAvatar *avatar, FSPoserJoint joint, bool shouldPose);

    /// <summary>
    /// Gets the position of a joint for the supplied avatar.
    /// </summary>
    /// <param name="avatar">The avatar whose joint is being queried.</param>
    /// <param name="joint">The joint to determine the position for.</param>
    /// <returns>The position of the requested joint, if determinable, otherwise a default vector.</returns>
    LLVector3 getJointPosition(LLVOAvatar *avatar, FSPoserJoint joint);

    /// <summary>
    /// Sets the position of a joint for the supplied avatar.
    /// </summary>
    /// <param name="avatar">The avatar whose joint is to be set.</param>
    /// <param name="joint">The joint to set.</param>
    /// <param name="position">The position to set the joint to.</param>
    /// <param name="style">The axial translation form the supplied joint.</param>
    void setJointPosition(LLVOAvatar *avatar, const FSPoserJoint *joint, LLVector3 position, E_BoneDeflectionStyles style);

    /// <summary>
    /// Gets the rotation of a joint for the supplied avatar.
    /// </summary>
    /// <param name="avatar">The avatar whose joint is being queried.</param>
    /// <param name="joint">The joint to determine the rotation for.</param>
    /// <param name="translation">The joint to determine the rotation for.</param>
    /// <returns>The rotation of the requested joint, if determinable, otherwise a default vector.</returns>
    LLVector3 getJointRotation(LLVOAvatar *avatar, FSPoserJoint joint, E_BoneAxisTranslation translation, S32 negation);

    /// <summary>
    /// Sets the rotation of a joint for the supplied avatar.
    /// </summary>
    /// <param name="avatar">The avatar whose joint is to be set.</param>
    /// <param name="joint">The joint to set.</param>
    /// <param name="rotation">The rotation to set the joint to.</param>
    /// <param name="style">Any ancilliary action to be taken with the change to be made.</param>
    /// <param name="translation">The axial translation form the supplied joint.</param>
    void setJointRotation(LLVOAvatar *avatar, const FSPoserJoint *joint, LLVector3 rotation, E_BoneDeflectionStyles style,
                          E_BoneAxisTranslation translation, S32 negation);

    /// <summary>
    /// Gets the scale of a joint for the supplied avatar.
    /// </summary>
    /// <param name="avatar">The avatar whose joint is being queried.</param>
    /// <param name="joint">The joint to determine the scale for.</param>
    /// <returns>The scale of the requested joint, if determinable, otherwise a default vector.</returns>
    LLVector3 getJointScale(LLVOAvatar *avatar, FSPoserJoint joint);

    /// <summary>
    /// Sets the scale of a joint for the supplied avatar.
    /// </summary>
    /// <param name="avatar">The avatar whose joint is to be set.</param>
    /// <param name="joint">The joint to set.</param>
    /// <param name="scale">The scale to set the joint to.</param>
    /// <param name="style">Any ancilliary action to be taken with the change to be made.</param>
    void setJointScale(LLVOAvatar *avatar, const FSPoserJoint *joint, LLVector3 scale, E_BoneDeflectionStyles style);

  private:
    bool _currentlyPosingSelf = false;

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
    LLVector3 translateRotationFromQuaternion(E_BoneAxisTranslation translation, S32 negation, LLQuaternion rotation);
};

#endif // LL_FSPoserAnimator_H
