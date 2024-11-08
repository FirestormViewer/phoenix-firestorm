/**
 * @file fsjointpose.h
 * @brief Container for the pose of a joint.
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

#ifndef FS_JOINTPPOSE_H
#define FS_JOINTPPOSE_H

//-----------------------------------------------------------------------------
// Header files
//-----------------------------------------------------------------------------
#include "llmotion.h"

//-----------------------------------------------------------------------------
// class FSJointPose
//-----------------------------------------------------------------------------
class FSJointPose
{
   public:
     /// <summary>
     /// A class encapsulating the positions/rotations for a joint.
     /// </summary>
     /// <param name="joint">The joint this joint pose represents.</param>
     /// <param name="usage">The default usage the joint should have in a pose.</param>
     /// <param name="isCollisionVolume">Whether the supplied joint is a collision volume.</param>
     FSJointPose(LLJoint* joint, U32 usage, bool isCollisionVolume = false);

    /// <summary>
    /// Gets the name of the joint.
    /// </summary>
    std::string jointName() const { return mJointName; }

    /// <summary>
    /// Gets whether this represents a collision volume.
    /// </summary>
    /// <returns>true if the joint is a collision volume, otherwise false.</returns>
    bool isCollisionVolume() const { return mIsCollisionVolume; }

    /// <summary>
    /// Adds a last rotation to the deque.
    /// </summary>
    void addLastRotationToUndo();

    /// <summary>
    /// Gets whether a redo of this joints rotation may be performed.
    /// </summary>
    /// <returns>true if the joint can have a redo applied, otherwise false.</returns>
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
    /// Adds a last position to the deque.
    /// </summary>
    void addLastPositionToUndo();

    /// <summary>
    /// Undoes the last position set, if any.
    /// </summary>
    void undoLastPositionSet();

    /// <summary>
    /// Undoes the last position set, if any.
    /// </summary>
    void redoLastPositionSet();

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
    /// Sets the beginning and target rotations to the supplied rotation.
    /// </summary>
    void setJointStartRotations(LLQuaternion quat);

    /// <summary>
    /// Sets the rotation the animator wishes the joint to be in.
    /// </summary>
    void setTargetRotation(const LLQuaternion& rot);

    /// <summary>
    /// Applies a delta to the rotation the joint currently targets.
    /// </summary>
    void applyDeltaRotation(const LLQuaternion& rot);

    /// <summary>
    /// Undoes the last rotation set, if any.
    /// Ordinarily the queue does not contain the current rotation, because we rely on time to add, and not button-up.
    /// When we undo, if we are at the top of the queue, we need to add the current rotation so we can redo back to it.
    /// Thus when we start undoing, mUndoneRotationIndex points at the current rotation.
    /// </summary>
    void undoLastRotationSet();

    /// <summary>
    /// Redoes the last rotation set, if any.
    /// </summary>
    void redoLastRotationSet();

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
    /// Undoes the last scale set, if any.
    /// </summary>
    void undoLastScaleSet();

    /// <summary>
    /// Redoes the last scale set, if any.
    /// </summary>
    void redoLastScaleSet();

    /// <summary>
    /// Adds a last rotation to the deque.
    /// </summary>
    void addLastScaleToUndo();

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
    /// Gets the pointer to the jointstate for the joint this represents.
    /// </summary>
    LLPointer<LLJointState> getJointState() const { return mJointState; }

private:
    std::string             mJointName = "";  // expected to be a match to LLJoint.getName() for a joint implementation.
    LLPointer<LLJointState> mJointState{ nullptr };

    /// <summary>
    /// Collision Volumes require special treatment when we stop animating an avatar, as they do not revert to their original state
    /// natively.
    /// </summary>
    bool mIsCollisionVolume{ false };

    LLQuaternion                          mTargetRotation;
    LLQuaternion                          mBeginningRotation;
    std::deque<LLQuaternion>              mLastSetRotations;
    size_t                                mUndoneRotationIndex     = 0;
    std::chrono::system_clock::time_point mTimeLastUpdatedRotation = std::chrono::system_clock::now();

    LLVector3                             mTargetPosition;
    LLVector3                             mBeginningPosition;
    std::deque<LLVector3>                 mLastSetPositions;
    size_t                                mUndonePositionIndex     = 0;
    std::chrono::system_clock::time_point mTimeLastUpdatedPosition = std::chrono::system_clock::now();

    /// <summary>
    /// Joint scales require special treatment, as they do not revert when we stop animating an avatar.
    /// </summary>
    LLVector3                             mTargetScale;
    LLVector3                             mBeginningScale;
    std::deque<LLVector3>                 mLastSetScales;
    size_t                                mUndoneScaleIndex     = 0;
    std::chrono::system_clock::time_point mTimeLastUpdatedScale = std::chrono::system_clock::now();
};

#endif // FS_JOINTPPOSE_H

