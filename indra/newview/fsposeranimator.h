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

#include "fsposingMotion.h"
#include "llvoavatar.h"

/// <summary>
/// Describes how we will cluster the joints/bones/thingos.
/// Each joint/bone/thingo should have one of these, <see:"FSPoserAnimator.PoserJoints"/>.
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
    NONE        = 0,  // do nothing additional
    MIRROR      = 1,  // change the other joint, like in a mirror, eg: one left one right
    SYMPATHETIC = 2,  // change the other joint, but opposite to a mirrored way, eg: both go right or both go left
    DELTAMODE   = 3,  // each selected joint changes by the same supplied amount relative to their current
} E_BoneDeflectionStyles;

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
    FSPoserAnimator();

    virtual ~FSPoserAnimator();

    /// <summary>
    /// A class encapsulating 'metadata' for a joint, such as its catagory and its opposite joint name.
    /// You'll note it's privates and methods: this is just emulating { get; private set; } from C#
    /// </summary>
    class FSPoserJoint
    {
        std::string _jointName; // expected to be a match to LLJoint.getName() for a joint implementation.
        std::string _mirrorJointName;
        E_BoneTypes _boneList;
        std::vector<std::string> _bvhChildren;
        bool _dontFlipOnMirror = false;
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
        /// Gets whether when mirroring the entire body, should this joint flip its counterpart.
        /// </summary>
        bool dontFlipOnMirror() const { return _dontFlipOnMirror; }

        /// <summary>
        /// Gets the collection of child bvh joints for this.
        /// </summary>
        std::vector<std::string> bvhChildren() const { return _bvhChildren; }

        /// <summary>
        /// Creates a new instance of a PoserJoint.
        /// </summary>
        /// <param name="a">
        /// The joint name, should be one of the well known bones/joints/thingos.
        /// An example for an LLJoints implementation would be what LLJoint.getName() returns, like 'mChest'.
        /// Very likely case-sensitive.
        /// </param>
        /// <param name="b">The opposite joint name, if any. Also expected to be a well-known name.</param>
        /// <param name="c">The type of bone, often determining with which other bones the new instance would appear with.</param>
        /// <param name="d">The optional array of joints, needed for BVH saving, which are the direct decendent(s) of this joint.</param>
        /// <param name="e">The option for whether this joint should rotation-flip it counterpart when mirroring the pose of the entire body.</param>
        FSPoserJoint(std::string a, std::string b, E_BoneTypes c, std::vector<std::string> d = {}, bool e = false)
        {
            _jointName        = a;
            _mirrorJointName  = b;
            _boneList         = c;
            _bvhChildren      = d;
            _dontFlipOnMirror = e;
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
        {"mPelvis", "", WHOLEAVATAR, {"mTorso", "mHipLeft", "mHipRight"}}, {"mTorso", "", BODY, {"mChest"}}, {"mChest", "", BODY, {"mNeck", "mCollarLeft", "mCollarRight", "mWingsRoot"}}, {"mNeck", "", BODY, {"mHead"}}, {"mHead", "", BODY},
        {"mCollarLeft", "mCollarRight", BODY, {"mShoulderLeft"}}, {"mShoulderLeft", "mShoulderRight", BODY, {"mElbowLeft"}}, {"mElbowLeft", "mElbowRight", BODY, {"mWristLeft"}}, {"mWristLeft", "mWristRight", BODY},
        {"mCollarRight", "mCollarLeft", BODY, {"mShoulderRight"}, true}, {"mShoulderRight", "mShoulderLeft", BODY, {"mElbowRight"}, true},  {"mElbowRight", "mElbowLeft", BODY, {"mWristRight"}, true},  {"mWristRight", "mWristLeft", BODY, {}, true},
        {"mHipLeft", "mHipRight", BODY, {"mKneeLeft"}}, {"mKneeLeft", "mKneeRight", BODY, {"mAnkleLeft"}},  {"mAnkleLeft", "mAnkleRight", BODY},
        {"mHipRight", "mHipLeft", BODY, {"mKneeRight"}, true}, {"mKneeRight", "mKneeLeft", BODY, {"mAnkleRight"}, true},  {"mAnkleRight", "mAnkleLeft", BODY, {}, true},

        // face
        {"mFaceForeheadLeft", "mFaceForeheadRight", FACE}, {"mFaceForeheadCenter", "", FACE},  {"mFaceForeheadRight", "mFaceForeheadLeft", FACE, {}, true},
        {"mFaceEyebrowOuterLeft", "mFaceEyebrowOuterRight", FACE}, {"mFaceEyebrowCenterLeft", "mFaceEyebrowCenterRight", FACE},  {"mFaceEyebrowInnerLeft", "mFaceEyebrowInnerRight", FACE},
        {"mFaceEyebrowOuterRight", "mFaceEyebrowOuterLeft", FACE, {}, true}, {"mFaceEyebrowCenterRight", "mFaceEyebrowCenterLeft", FACE, {}, true},  {"mFaceEyebrowInnerRight", "mFaceEyebrowInnerLeft", FACE, {}, true},

        {"mEyeLeft", "mEyeRight", FACE}, {"mEyeRight", "mEyeLeft", FACE, {}, true},
        {"mFaceEyeLidUpperLeft", "mFaceEyeLidUpperRight", FACE}, {"mFaceEyeLidLowerLeft", "mFaceEyeLidLowerRight", FACE}, {"mFaceEyeLidUpperRight", "mFaceEyeLidUpperLeft", FACE, {}, true}, {"mFaceEyeLidLowerRight", "mFaceEyeLidLowerLeft", FACE, {}, true},

        {"mFaceCheekUpperLeft", "mFaceCheekUpperRight", FACE}, {"mFaceCheekLowerLeft", "mFaceCheekLowerRight", FACE}, {"mFaceCheekUpperRight", "mFaceCheekUpperLeft", FACE, {}, true}, {"mFaceCheekLowerRight", "mFaceCheekLowerLeft", FACE, {}, true},
        {"mFaceLipUpperLeft", "mFaceLipUpperRight", FACE}, {"mFaceLipUpperCenter", "", FACE}, {"mFaceLipUpperRight", "mFaceLipUpperLeft", FACE, {}, true},
        {"mFaceLipCornerLeft", "mFaceLipCornerRight", FACE}, {"mFaceLipCornerRight", "mFaceLipCornerLeft", FACE, {}, true},
        {"mFaceTongueBase", "", FACE}, {"mFaceTongueTip", "", FACE, {}, true},
        {"mFaceLipLowerLeft", "mFaceLipLowerRight", FACE}, {"mFaceLipLowerCenter", "", FACE}, {"mFaceLipLowerRight", "mFaceLipLowerLeft", FACE, {}, true},
        {"mFaceJaw", "", FACE},

        //left hand
        {"mHandThumb1Left", "mHandThumb1Right", HANDS}, {"mHandThumb2Left", "mHandThumb2Right", HANDS}, {"mHandThumb3Left", "mHandThumb3Right", HANDS},
        {"mHandIndex1Left", "mHandIndex1Right", HANDS}, {"mHandIndex2Left", "mHandIndex2Right", HANDS}, {"mHandIndex3Left", "mHandIndex3Right", HANDS},
        {"mHandMiddle1Left", "mHandMiddle1Right", HANDS}, {"mHandMiddle2Left", "mHandMiddle2Right", HANDS}, {"mHandMiddle3Left", "mHandMiddle3Right", HANDS},
        {"mHandRing1Left", "mHandRing1Right", HANDS}, {"mHandRing2Left", "mHandRing2Right", HANDS}, {"mHandRing3Left", "mHandRing3Right", HANDS},
        {"mHandPinky1Left", "mHandPinky1Right", HANDS}, {"mHandPinky2Left", "mHandPinky2Right", HANDS}, {"mHandPinky3Left", "mHandPinky3Right", HANDS},

        // right hand
        {"mHandThumb1Right", "mHandThumb1Left", HANDS, {}, true}, {"mHandThumb2Right", "mHandThumb2Left", HANDS, {}, true}, {"mHandThumb3Right", "mHandThumb3Left", HANDS, {}, true},
        {"mHandIndex1Right", "mHandIndex1Left", HANDS, {}, true}, {"mHandIndex2Right", "mHandIndex2Left", HANDS, {}, true}, {"mHandIndex3Right", "mHandIndex3Left", HANDS, {}, true},
        {"mHandMiddle1Right", "mHandMiddle1Left", HANDS, {}, true}, {"mHandMiddle2Right", "mHandMiddle2Left", HANDS, {}, true}, {"mHandMiddle3Right", "mHandMiddle3Left", HANDS, {}, true},
        {"mHandRing1Right", "mHandRing1Left", HANDS, {}, true}, {"mHandRing2Right", "mHandRing2Left", HANDS, {}, true}, {"mHandRing3Right", "mHandRing3Left", HANDS, {}, true},
        {"mHandPinky1Right", "mHandPinky1Left", HANDS, {}, true}, {"mHandPinky2Right", "mHandPinky2Left", HANDS, {}, true}, {"mHandPinky3Right", "mHandPinky3Left", HANDS, {}, true},

        // tail and hind limbs
        {"mTail1", "", MISC}, {"mTail2", "", MISC}, {"mTail3", "", MISC}, {"mTail4", "", MISC}, {"mTail5", "", MISC}, {"mTail6", "", MISC}, {"mGroin", "", MISC},
        {"mHindLimbsRoot", "", MISC},
        {"mHindLimb1Left", "mHindLimb1Right", MISC}, {"mHindLimb2Left", "mHindLimb2Right", MISC}, {"mHindLimb3Left", "mHindLimb3Right", MISC}, {"mHindLimb4Left", "mHindLimb4Right", MISC},
        {"mHindLimb1Right", "mHindLimb1Left", MISC, {}, true}, {"mHindLimb2Right", "mHindLimb2Left", MISC, {}, true}, {"mHindLimb3Right", "mHindLimb3Left", MISC, {}, true}, {"mHindLimb4Right", "mHindLimb4Left", MISC, {}, true},

        // wings
        {"mWingsRoot", "", MISC},
        {"mWing1Left", "mWing1Right", MISC}, {"mWing2Left", "mWing2Right", MISC}, {"mWing3Left", "mWing3Right", MISC}, {"mWing4Left", "mWing4Right", MISC}, {"mWing4FanLeft", "mWing4FanRight", MISC},
        {"mWing1Right", "mWing1Left", MISC, {}, true}, {"mWing2Right", "mWing2Left", MISC, {}, true}, {"mWing3Right", "mWing3Left", MISC, {}, true}, {"mWing4Right", "mWing4Left", MISC, {}, true}, {"mWing4FanRight", "mWing4FanLeft", MISC, {}, true},

        // Collision Volumes
        {"LEFT_PEC", "RIGHT_PEC", COL_VOLUMES}, {"RIGHT_PEC", "LEFT_PEC", COL_VOLUMES, {}, true}, {"BELLY", "", COL_VOLUMES}, {"BUTT", "", COL_VOLUMES},
    };
    
public:
    /// <summary>
    /// Get a PoserJoint case-insensitive-matching the supplied name.
    /// </summary>
    /// <param name="jointName">The name of the joint to match.</param>
    /// <returns>The matching joint if found, otherwise nullptr</returns>
    const FSPoserJoint* getPoserJointByName(std::string jointName);

    /// <summary>
    /// Tries to start posing the supplied avatar.
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
    /// <returns>True if this is joint is being posed for the supplied avatar, otherwise false.</returns>
    bool isPosingAvatarJoint(LLVOAvatar *avatar, FSPoserJoint joint);

    /// <summary>
    /// Sets whether the supplied PoserJoint for the supplied avatar should be posed.
    /// </summary>
    /// <param name="avatar">The avatar having the joint to which we refer.</param>
    /// <param name="joint">The joint being queried for.</param>
    /// <param name="posing">Whether the joint should be posed, or not.</param>
    /// <remarks>
    /// If this is not posing the joint, then it is free to be posed by other things.
    /// </remarks>
    void setPosingAvatarJoint(LLVOAvatar *avatar, FSPoserJoint joint, bool shouldPose);

    /// <summary>
    /// Resets the supplied PoserJoint to its position/rotation/scale it was when poser was started.
    /// </summary>
    /// <param name="avatar">The avatar having the joint to which we refer.</param>
    /// <param name="joint">The joint to be reset.</param>
    void resetAvatarJoint(LLVOAvatar *avatar, FSPoserJoint joint);

    /// <summary>
    /// Undoes the last applied rotation to the supplied PoserJoint.
    /// </summary>
    /// <param name="avatar">The avatar having the joint to which we refer.</param>
    /// <param name="joint">The joint with the rotation to undo.</param>
    void undoLastJointRotation(LLVOAvatar* avatar, FSPoserJoint joint, E_BoneDeflectionStyles style);

    /// <summary>
    /// Undoes the last applied position to the supplied PoserJoint.
    /// </summary>
    /// <param name="avatar">The avatar having the joint to which we refer.</param>
    /// <param name="joint">The joint with the position to undo.</param>
    void undoLastJointPosition(LLVOAvatar* avatar, FSPoserJoint joint, E_BoneDeflectionStyles style);

    /// <summary>
    /// Undoes the last applied scale to the supplied PoserJoint.
    /// </summary>
    /// <param name="avatar">The avatar having the joint to which we refer.</param>
    /// <param name="joint">The joint with the scale to undo.</param>
    void undoLastJointScale(LLVOAvatar* avatar, FSPoserJoint joint, E_BoneDeflectionStyles style);

    /// <summary>
    /// Resets the position of the supplied PoserJoint.
    /// </summary>
    /// <param name="avatar">The avatar having the joint to which we refer.</param>
    /// <param name="joint">The joint with the position to reset.</param>
    void resetJointPosition(LLVOAvatar* avatar, FSPoserJoint joint, E_BoneDeflectionStyles style);

    /// <summary>
    /// Resets the scale of the supplied joint to initial values.
    /// </summary>
    /// <param name="avatar">The avatar having the joint to which we refer.</param>
    /// <param name="joint">The joint with the scale to reset.</param>
    void resetJointScale(LLVOAvatar* avatar, FSPoserJoint joint, E_BoneDeflectionStyles style);

    /// <summary>
    /// Determines if a redo action is currently permitted for the supplied joint.
    /// </summary>
    /// <param name="avatar">The avatar having the joint to which we refer.</param>
    /// <param name="joint">The joint to query.</param>
    /// <returns>True if a redo action is available, otherwise false.</returns>
    bool canRedoJointRotation(LLVOAvatar* avatar, FSPoserJoint joint);

    /// <summary>
    /// Re-does the last undone rotation to the supplied PoserJoint.
    /// </summary>
    /// <param name="avatar">The avatar having the joint to which we refer.</param>
    /// <param name="joint">The joint with the rotation to redo.</param>
    void redoLastJointRotation(LLVOAvatar* avatar, FSPoserJoint joint, E_BoneDeflectionStyles style);

    /// <summary>
    /// Re-does the last undone position to the supplied PoserJoint.
    /// </summary>
    /// <param name="avatar">The avatar having the joint to which we refer.</param>
    /// <param name="joint">The joint with the position to redo.</param>
    void redoLastJointPosition(LLVOAvatar* avatar, FSPoserJoint joint, E_BoneDeflectionStyles style);

    /// <summary>
    /// Re-does the last undone scale to the supplied PoserJoint.
    /// </summary>
    /// <param name="avatar">The avatar having the joint to which we refer.</param>
    /// <param name="joint">The joint with the scale to redo.</param>
    void redoLastJointScale(LLVOAvatar* avatar, FSPoserJoint joint, E_BoneDeflectionStyles style);

    /// <summary>
    /// Gets the position of a joint for the supplied avatar.
    /// </summary>
    /// <param name="avatar">The avatar whose joint is being queried.</param>
    /// <param name="joint">The joint to determine the position for.</param>
    /// <returns>The position of the requested joint, if determinable, otherwise a default vector.</returns>
    LLVector3 getJointPosition(LLVOAvatar* avatar, FSPoserJoint joint, bool forRecapture = false);

    /// <summary>
    /// Sets the position of a joint for the supplied avatar.
    /// </summary>
    /// <param name="avatar">The avatar whose joint is to be set.</param>
    /// <param name="joint">The joint to set.</param>
    /// <param name="position">The position to set the joint to.</param>
    /// <param name="style">Any ancilliary action to be taken with the change to be made.</param>
    void setJointPosition(LLVOAvatar *avatar, const FSPoserJoint *joint, LLVector3 position, E_BoneDeflectionStyles style);

    /// <summary>
    /// Gets the rotation of a joint for the supplied avatar.
    /// </summary>
    /// <param name="avatar">The avatar whose joint is being queried.</param>
    /// <param name="joint">The joint to determine the rotation for.</param>
    /// <param name="translation">The joint to determine the rotation for.</param>
    /// <param name="negation">The style of negation to apply to the set.</param>
    /// <param name="forRecapture">Get the current non-poser rotation, for recapture opportunity.</param>
    /// <returns>The rotation of the requested joint, if determinable, otherwise a default vector.</returns>
    LLVector3 getJointRotation(LLVOAvatar *avatar, FSPoserJoint joint, E_BoneAxisTranslation translation, S32 negation,
                               bool forRecapture = false);

    /// <summary>
    /// Sets the rotation of a joint for the supplied avatar.
    /// </summary>
    /// <param name="avatar">The avatar whose joint is to be set.</param>
    /// <param name="joint">The joint to set.</param>
    /// <param name="rotation">The rotation to set the joint to.</param>
    /// <param name="style">Any ancilliary action to be taken with the change to be made.</param>
    /// <param name="translation">The axial translation form the supplied joint.</param>
    void setJointRotation(LLVOAvatar* avatar, const FSPoserJoint* joint, LLVector3 rotation, E_BoneDeflectionStyles style,
                          E_BoneAxisTranslation translation, S32 negation);

    /// <summary>
    /// Gets the scale of a joint for the supplied avatar.
    /// </summary>
    /// <param name="avatar">The avatar whose joint is being queried.</param>
    /// <param name="joint">The joint to determine the scale for.</param>
    /// <returns>The scale of the requested joint, if determinable, otherwise a default vector.</returns>
    LLVector3 getJointScale(LLVOAvatar* avatar, FSPoserJoint joint, bool forRecapture = false);

    /// <summary>
    /// Sets the scale of a joint for the supplied avatar.
    /// </summary>
    /// <param name="avatar">The avatar whose joint is to be set.</param>
    /// <param name="joint">The joint to set.</param>
    /// <param name="scale">The scale to set the joint to.</param>
    /// <param name="style">Any ancilliary action to be taken with the change to be made.</param>
    void setJointScale(LLVOAvatar *avatar, const FSPoserJoint *joint, LLVector3 scale, E_BoneDeflectionStyles style);

    /// <summary>
    /// Reflects the joint with its opposite if it has one, or just mirror the rotation of itself.
    /// </summary>
    /// <param name="avatar">The avatar whose joint should flip left-right.</param>
    /// <param name="joint">The joint to mirror rotation for.</param>
    void reflectJoint(LLVOAvatar *avatar, const FSPoserJoint *joint);

    /// <summary>
    /// Reflects every joint of the supplied avatar with its opposite if it has one, or mirrors the rotation of the joint if it does not have an opposite.
    /// </summary>
    /// <param name="avatar">The avatar whose pose should flip left-right.</param>
    void flipEntirePose(LLVOAvatar *avatar);

    /// <summary>
    /// Determines whether the supplied PoserJoint for the supplied avatar is being posed.
    /// </summary>
    /// <param name="avatar">The avatar having the joint to which we refer.</param>
    /// <param name="joint">The joint being queried for.</param>
    /// <returns>True if this is joint is being posed for the supplied avatar, otherwise false.</returns>
    bool writePoseAsBvh(llofstream *fileStream, LLVOAvatar* avatar);

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
    LLVector3 translateRotationFromQuaternion(E_BoneAxisTranslation translation, S32 negation, LLQuaternion rotation);

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
    FSPosingMotion* getPosingMotion(LLVOAvatar* avatar);

    /// <summary>
    /// Determines if the avatar can be used.
    /// </summary>
    /// <param name="avatar">The avatar to test if it is safe to animate.</param>
    /// <returns>True if the avatar is safe to manipulate, otherwise false.</returns>
    bool isAvatarSafeToUse(LLVOAvatar* avatar);

    /// <summary>
    /// Recursively writes a fragment of a BVH file format representation of the supplied joint, then that joints BVH child(ren).
    /// </summary>
    /// <param name="fileStream">The stream to write the fragment to.</param>
    /// <param name="avatar">The avatar owning the supplied joint.</param>
    /// <param name="joint">The joint whose fragment should be written, and whose child(ren) will also be written.</param>
    /// <param name="tabStops">The number of tab-stops to include for formatting purpose.</param>
    /// <returns>True if the fragment wrote successfully, otherwise false.</returns>
    bool writeBvhFragment(llofstream* fileStream, LLVOAvatar* avatar, const FSPoserJoint* joint, int tabStops);

    /// <summary>
    /// Writes a fragment of the 'single line' representing an animation frame within the BVH file respresenting the positions and/or rotations.
    /// </summary>
    /// <param name="fileStream">The stream to write the position and/or rotation to.</param>
    /// <param name="avatar">The avatar owning the supplied joint.</param>
    /// <param name="joint">The joint whose position and/or rotation should be written.</param>
    /// <returns></returns>
    bool writeBvhMotion(llofstream* fileStream, LLVOAvatar* avatar, const FSPoserJoint* joint);

    /// <summary>
    /// Converts an F32 to a nice string.
    /// </summary>
    std::string static f32ToString(F32 val);

    /// <summary>
    /// Generates a string with the supplied number of tab-chars.
    /// </summary>
    std::string static getTabs(int numOfTabstops);

    /// <summary>
    /// Transforms a rotation such that llbvhloader.cpp can resolve it to something vaguely approximating the supplied angle.
    /// When I say vague, I mean, it's numbers, buuuuut.
    /// </summary>
    std::string static rotationToYZXString(LLVector3 val);

    /// <summary>
    /// Transforms the supplied vector into a string of three numbers, format suiting to writing into a BVH file.
    /// </summary>
    std::string static vec3ToXYZString(LLVector3 val);

    /// <summary>
    /// Maps the avatar's ID to the animation registered to them.
    /// Thus we start/stop the same animation, and get/set the same rotations etc.
    /// Among other things this provides for the 'undo' of changes to shape/position when the poser stops animating someone.
    /// An avatar's animation exists so long as their session does, and there is consideration for renewal (like if they relog/crash and their character is renewed).
    /// Is static, so the animationId is not lost between sessions (such as when the UI floater is closed and reopened).
    /// </summary>
    static std::map<LLUUID, LLAssetID> _avatarIdToRegisteredAnimationId;
};

#endif // LL_FSPoserAnimator_H
