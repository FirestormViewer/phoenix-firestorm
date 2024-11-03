/**
 * @file fsposingmotion.cpp
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

    /// <summary>
    /// A class encapsulating the positions/rotations for a joint.
    /// </summary>
    class FSJointPose
    {
        std::string              mJointName = "";  // expected to be a match to LLJoint.getName() for a joint implementation.
        LLPointer<LLJointState>  mJointState{ nullptr };

        /// <summary>
        /// Collision Volumes require special treatment when we stop animating an avatar, as they do not revert to their original state natively.
        /// </summary>
        bool                     mIsCollisionVolume{ false };

        LLQuaternion             mTargetRotation;
        LLQuaternion             mBeginningRotation;
        std::deque<LLQuaternion> mLastSetRotations;
        size_t                   mUndoneRotationIndex = 0;
        std::chrono::system_clock::time_point mTimeLastUpdatedRotation = std::chrono::system_clock::now();

        LLVector3               mTargetPosition;
        LLVector3               mBeginningPosition;
        std::deque<LLVector3>   mLastSetPositions;
        size_t                  mUndonePositionIndex = 0;
        std::chrono::system_clock::time_point mTimeLastUpdatedPosition = std::chrono::system_clock::now();

        /// <summary>
        /// Joint scales require special treatment, as they do not revert when we stop animating an avatar.
        /// </summary>
        LLVector3             mTargetScale;
        LLVector3             mBeginningScale;
        std::deque<LLVector3> mLastSetScales;
        size_t                mUndoneScaleIndex = 0;
        std::chrono::system_clock::time_point mTimeLastUpdatedScale = std::chrono::system_clock::now();

        /// <summary>
        /// Adds a last position to the deque.
        /// </summary>
        void addLastPositionToUndo();

        /// <summary>
        /// Adds a last rotation to the deque.
        /// </summary>
        void addLastRotationToUndo();

        /// <summary>
        /// Adds a last rotation to the deque.
        /// </summary>
        void addLastScaleToUndo();

      public:
        /// <summary>
        /// Gets the name of the joint.
        /// </summary>
        std::string jointName() const { return mJointName; }

        /// <summary>
        /// Gets whether this represents a collision volume.
        /// </summary>
        /// <returns></returns>
        bool isCollisionVolume() const { return mIsCollisionVolume; }

        /// <summary>
        /// Gets whether a redo of this joints rotation may be performed.
        /// </summary>
        /// <returns></returns>
        bool canRedoRotation() const { return mUndoneRotationIndex > 0; }

        /// <summary>
        /// Gets the position the joint was in when the animation was initialized.
        /// </summary>
        LLVector3 getBeginningPosition() const { return mBeginningPosition; }

        /// <summary>
        /// Gets the position the animator wishes the joint to be in.
        /// </summary>
        LLVector3 getTargetPosition() const { return mTargetPosition; }

        /// <summary>
        /// Gets the position the animator wishes the joint to be in.
        /// </summary>
        LLVector3 getCurrentPosition();

        /// <summary>
        /// Sets the position the animator wishes the joint to be in.
        /// </summary>
        void setTargetPosition(const LLVector3& pos);

        /// <summary>
        /// Gets the rotation the joint was in when the animation was initialized.
        /// </summary>
        LLQuaternion getBeginningRotation() const { return mBeginningRotation; }

        /// <summary>
        /// Gets the rotation the animator wishes the joint to be in.
        /// </summary>
        LLQuaternion getTargetRotation() const { return mTargetRotation; }

        /// <summary>
        /// Gets the rotation of the joint.
        /// </summary>
        LLQuaternion getCurrentRotation();

        /// <summary>
        /// Sets the rotation the animator wishes the joint to be in.
        /// </summary>
        void setTargetRotation(const LLQuaternion& rot);

        /// <summary>
        /// Applies a delta to the rotation the joint currently targets.
        /// </summary>
        void applyDeltaRotation(const LLQuaternion& rot);

        /// <summary>
        /// Gets the scale the animator wishes the joint to have.
        /// </summary>
        LLVector3 getTargetScale() const { return mTargetScale; }

        /// <summary>
        /// Gets the scale the joint has.
        /// </summary>
        LLVector3 getCurrentScale();

        /// <summary>
        /// Gets the scale the joint had when the animation was initialized.
        /// </summary>
        LLVector3 getBeginningScale() const { return mBeginningScale; }

        /// <summary>
        /// Sets the scale the animator wishes the joint to have.
        /// </summary>
        void setTargetScale(LLVector3 scale);

        /// <summary>
        /// Undoes the last position set, if any.
        /// </summary>
        void undoLastPositionSet();

        /// <summary>
        /// Undoes the last position set, if any.
        /// </summary>
        void redoLastPositionSet();

        /// <summary>
        /// Undoes the last rotation set, if any.
        /// Ordinarily the queue does not contain the current rotation, because we rely on time to add, and not button-up.
        /// When we undo, if we are at the top of the queue, we need to add the current rotation so we can redo back to it.
        /// Thus when we start undoing, mUndoneRotationIndex points at the current rotation.
        /// </summary>
        void undoLastRotationSet();

        /// <summary>
        /// Undoes the last rotation set, if any.
        /// </summary>
        void redoLastRotationSet();

        void undoLastScaleSet();

        void redoLastScaleSet();

        /// <summary>
        /// Restores the joint represented by this to the scale it had when this motion started.
        /// </summary>
        void revertJointScale();

        /// <summary>
        /// Restores the joint represented by this to the position it had when this motion started.
        /// </summary>
        void revertJointPosition();

        /// <summary>
        /// Collision Volumes do not 'reset' their position/rotation when the animation stops.
        /// This requires special treatment to revert changes we've made this animation session.
        /// </summary>
        void revertCollisionVolume();

        /// <summary>
        /// Sets all rotations to zero.
        /// </summary>
        void setRotationsToZero();

        /// <summary>
        /// Gets the pointer to the jointstate for the joint this represents.
        /// </summary>
        LLPointer<LLJointState> getJointState() const { return mJointState; }

        FSJointPose(LLJoint* joint, bool isCollisionVolume = false);
    };

public:
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
    /// The kind of joint state this animation is concerned with changing.
    /// </summary>
    static const U32 POSER_JOINT_STATE = LLJointState::POS | LLJointState::ROT | LLJointState::SCALE;

    /// <summary>
    /// The unique identity of this motion.
    /// </summary>
    LLAssetID mMotionID;

    /// <summary>
    /// The amount of time, in seconds, we use for transitioning between one animation-state to another; this affects the 'fluidity'
    /// of motion between changes to a joint.
    /// Use caution making this larger than the subjective amount of time between adjusting a joint and then choosing to use 'undo' it.
    /// Undo-function waits a similar amount of time after the last user-incited joint change to add a 'restore point'.
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
    void revertChangesToPositionsScalesAndCollisionVolumes();

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
};

#endif // FS_POSINGMOTION_H

