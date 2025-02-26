/**
 * @file fsposingmotion.h
 * @brief Model for posing your (and other) avatar(s).
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

#ifndef FS_POSINGMOTION_H
#define FS_POSINGMOTION_H

//-----------------------------------------------------------------------------
// Header files
//-----------------------------------------------------------------------------
#include "llmotion.h"
#include "fsjointpose.h"

#define MIN_REQUIRED_PIXEL_AREA_POSING 500.f

//-----------------------------------------------------------------------------
// class FSPosingMotion
//-----------------------------------------------------------------------------
class FSPosingMotion :
    public LLMotion
{
public:
    FSPosingMotion(const LLUUID &id);
    virtual ~FSPosingMotion(){};

public:
    static LLMotion *create(const LLUUID &id) { return new FSPosingMotion(id); }

    virtual bool getLoop() { return true; }

    virtual F32 getDuration() { return 0.0; }

    virtual F32 getEaseInDuration() { return 0.0f; }

    virtual F32 getEaseOutDuration() { return 0.5f; }

    virtual LLJoint::JointPriority getPriority() { return LLJoint::ADDITIVE_PRIORITY; }

    virtual LLMotionBlendType getBlendType() { return NORMAL_BLEND; }

    // called to determine when a motion should be activated/deactivated based on avatar pixel coverage
    virtual F32 getMinPixelArea() { return MIN_REQUIRED_PIXEL_AREA_POSING; }

    // run-time (post constructor) initialization,
    // called after parameters have been set
    // must return true to indicate success and be available for activation
    virtual LLMotionInitStatus onInitialize(LLCharacter *character);

    // called when a motion is activated
    // must return TRUE to indicate success, or else
    // it will be deactivated
    virtual bool onActivate();

    // called per time step
    // must return TRUE while it is active, and
    // must return FALSE when the motion is completed.
    virtual bool onUpdate(F32 time, U8 *joint_mask);

    // called when a motion is deactivated
    virtual void onDeactivate();

    /// <summary>
    /// Queries whether the supplied joint is being animated.
    /// </summary>
    /// <param name="joint">The joint to query.</param>
    bool currentlyPosingJoint(FSJointPose* joint);

    /// <summary>
    /// Adds the supplied joint to the current animation-state.
    /// </summary>
    /// <param name="joint">The joint to animate.</param>
    void addJointToState(FSJointPose* joint);

    /// <summary>
    /// Removes the supplied joint to the current animation-state.
    /// </summary>
    /// <param name="joint">The joint to stop animating.</param>
    void removeJointFromState(FSJointPose* joint);

    /// <summary>
    /// Gets the joint pose by name.
    /// </summary>
    /// <param name="name">The name of the joint to get the pose for.</param>
    /// <returns>The matching joint pose, if found, otherwise null.</returns>
    FSJointPose* getJointPoseByJointName(const std::string& name);

    /// <summary>
    /// Gets the motion identity for this animation.
    /// </summary>
    /// <returns>The unique, per-session, per-character motion identity.</returns>
    LLAssetID motionId() const { return mMotionID; }

    /// <summary>
    /// Gets whether all starting rotations are zero.
    /// </summary>
    /// <returns>True if all starting rotations are zero, otherwise false.</returns>
    bool allStartingRotationsAreZero() const;

    /// <summary>
    /// Sets all of the non-Collision Volume rotations to zero.
    /// </summary>
    void setAllRotationsToZero();

private:
    /// <summary>
    /// The axial difference considered close enough to be the same.
    /// </summary>
    /// <remarks>
    /// This is intended to minimize lerps and slerps, preventing wasted CPU time over fractionally small rotation/position/scale differences.
    /// Too small and it's inefficient. Too large and there is noticeable error in the pose.
    /// This takes advantage of how the actual vector migrates to equality with the target vector.
    /// Certain physics settings (bouncing whatnots) account for some longer term work, but as this is applied per joint, it tends to reduce a lot of work.
    /// </remarks>
    const F32 closeEnough = 1e-6;

    /// <summary>
    /// The kind of joint state this animation is concerned with changing.
    /// </summary>
    static const U32 POSER_JOINT_STATE = LLJointState::POS | LLJointState::ROT | LLJointState::SCALE;

    /// <summary>
    /// The unique identity of this motion.
    /// </summary>
    LLAssetID mMotionID;

    /// <summary>
    /// The time constant, in seconds, we use for transitioning between one animation-state to another; this affects the 'damping'
    /// of motion between changes to a joint. 'Constant' in this context is not a reference to the language-idea of 'const' value.
    /// Smaller is less damping => faster transition.
    /// As implemented, the actual rotation of a joint decays towards the target rotation in something akin to (if not) an exponential.
    /// This time constant effects how fast this decay occurs rather than how long it takes to complete (it often never completes).
    /// This contributes to the fact that actual rotation != target rotation (in addition to rotation being non-monotonic).
    /// Use caution making this larger than the subjective amount of time between adjusting a joint and then choosing to use 'undo' it.
    /// Undo-function waits an amount of time after the last user-incited joint change to add a 'restore point'.
    /// Important to note is that the actual rotation/position/scale never reaches the target, which seems absurd, however
    /// it's the user that closes the feedback loop here: if they want more change, they input more until the result is as they like it.
    /// </summary>
    const F32 mInterpolationTime = 0.25f;

    /// <summary>
    /// The collection of joint poses this motion uses to pose the joints of the character this is animating. 
    /// </summary>
    std::vector<FSJointPose> mJointPoses;

    /// <summary>
    /// Removes the current joint state for the supplied joint, and adds a new one.
    /// </summary>
    void setJointState(LLJoint* joint, U32 state);

    /// <summary>
    /// Because changes to positions, scales and collision volumes do not end when the animation stops,
    /// this is required to revert them manually.
    /// </summary>
    void revertJointsAndCollisionVolumes();

    /// <summary>
    /// Queries whether the supplied joint is being animated.
    /// </summary>
    /// <param name="joint">The joint to query.</param>
    bool currentlyPosingJoint(LLJoint* joint);

    /// <summary>
    /// Adds the supplied joint to the current animation-state.
    /// </summary>
    /// <param name="joint">The joint to animate.</param>
    void addJointToState(LLJoint* joint);

    /// <summary>
    /// Removes the supplied joint to the current animation-state.
    /// </summary>
    /// <param name="joint">The joint to stop animating.</param>
    void removeJointFromState(LLJoint* joint);

    /// <summary>
    /// Determines if two vectors are near enough to equal.
    /// </summary>
    /// <param name="v1">The first vector to compare.</param>
    /// <param name="v2">The sceond vector to compare.</param>
    /// <returns>true if the vectors are "close enough", otherwise false.</returns>
    bool vectorsNotQuiteEqual(LLVector3 v1, LLVector3 v2) const;

    /// <summary>
    /// Determines if two quaternions are near enough to equal.
    /// </summary>
    /// <param name="v1">The first quaternion to compare.</param>
    /// <param name="v2">The sceond quaternion to compare.</param>
    /// <returns>true if the quaternion are "close enough", otherwise false.</returns>
    bool quatsNotQuiteEqual(const LLQuaternion& q1, const LLQuaternion& q2) const;

    bool vectorAxesAlmostEqual(F32 qA, F32 qB) const { return llabs(qA - qB) < closeEnough; }
};

#endif // FS_POSINGMOTION_H
