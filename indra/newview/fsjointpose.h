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
    /// Gets the position change the animator wishes the joint to have.
    /// </summary>
    LLVector3 getPositionDelta() const { return mPositionDelta; }

    /// <summary>
    /// Sets the position the animator wishes the joint to be in.
    /// </summary>
    void setPositionDelta(const LLVector3& pos);

    /// <summary>
    /// Undoes the last position set, if any.
    /// </summary>
    void undoLastPositionChange();

    /// <summary>
    /// Undoes the last position set, if any.
    /// </summary>
    void redoLastPositionChange();

    /// <summary>
    /// Gets the rotation the animator wishes the joint to be in.
    /// </summary>
    LLQuaternion getRotationDelta() const { return mRotation.deltaRotation; }

    /// <summary>
    /// Sets the rotation the animator wishes the joint to be in.
    /// </summary>
    void setRotationDelta(const LLQuaternion& rot);

    /// <summary>
    /// Reflects the base and delta rotation of the represented joint left-right.
    /// </summary>
    void reflectRotation();

    /// <summary>
    /// Sets the base rotation of the represented joint to zero.
    /// </summary>
    void zeroBaseRotation();

    /// <summary>
    /// Queries whether the represented joint is zero.
    /// </summary>
    /// <returns>True if the represented joint is zero, otherwise false.</returns>
    bool isBaseRotationZero() const;

    /// <summary>
    /// Undoes the last rotation set, if any.
    /// Ordinarily the queue does not contain the current rotation, because we rely on time to add, and not button-up.
    /// When we undo, if we are at the top of the queue, we need to add the current rotation so we can redo back to it.
    /// Thus when we start undoing, mUndoneRotationIndex points at the current rotation.
    /// </summary>
    void undoLastRotationChange();

    /// <summary>
    /// Redoes the last rotation set, if any.
    /// </summary>
    void redoLastRotationChange();

    /// <summary>
    /// Gets whether a redo of this joints rotation may be performed.
    /// </summary>
    /// <returns>true if the joint can have a redo applied, otherwise false.</returns>
    bool canRedoRotation() const { return mUndoneRotationIndex > 0; }

    /// <summary>
    /// Gets the scale the animator wishes the joint to have.
    /// </summary>
    LLVector3 getScaleDelta() const { return mScaleDelta; }

    /// <summary>
    /// Sets the scale the animator wishes the joint to have.
    /// </summary>
    void setScaleDelta(const LLVector3& scale);

    /// <summary>
    /// Undoes the last scale set, if any.
    /// </summary>
    void undoLastScaleChange();

    /// <summary>
    /// Redoes the last scale set, if any.
    /// </summary>
    void redoLastScaleChange();

    /// <summary>
    /// Exchanges the rotations between two joints.
    /// </summary>
    void swapRotationWith(FSJointPose* oppositeJoint);

    /// <summary>
    /// Clones the rotation to this from the supplied joint.
    /// </summary>
    void cloneRotationFrom(FSJointPose* fromJoint);

    /// <summary>
    /// Mirrors the rotation to this from the supplied joint.
    /// </summary>
    void mirrorRotationFrom(FSJointPose* fromJoint);

    /// <summary>
    /// Resets the beginning properties of the joint this represents.
    /// </summary>
    void recaptureJoint();

    /// <summary>
    /// Reverts the position/rotation/scale to their values when the animation begun.
    /// This treatment is required for certain joints, particularly Collision Volumes and those bones not commonly animated by an AO.
    /// </summary>
    void revertJoint();

    LLVector3 getTargetPosition() const { return mPositionDelta + mBeginningPosition; }
    LLQuaternion getTargetRotation() const { return mRotation.getTargetRotation(); }
    LLVector3 getTargetScale() const { return mScaleDelta + mBeginningScale; }

    /// <summary>
    /// Gets the pointer to the jointstate for the joint this represents.
    /// </summary>
    LLPointer<LLJointState> getJointState() const { return mJointState; }

    /// <summary>
    /// A class wrapping base and delta rotation, attempting to keep baseRotation as secret as possible.
    /// Among other things, facilitates easy undo/redo through the joint-recapture process.
    /// </summary>
    class FSJointRotation
    {
      public:
        FSJointRotation(LLQuaternion base) { baseRotation.set(base); }

        FSJointRotation(LLQuaternion base, LLQuaternion delta)
        {
            baseRotation.set(base);
            deltaRotation.set(delta);
        }

        FSJointRotation() = default;

        LLQuaternion baseRotation;
        LLQuaternion deltaRotation;
        LLQuaternion getTargetRotation() const { return deltaRotation * baseRotation; }

        void reflectRotation()
        {
            baseRotation.mQ[VX] *= -1;
            baseRotation.mQ[VZ] *= -1;
            deltaRotation.mQ[VX] *= -1;
            deltaRotation.mQ[VZ] *= -1;
        }

        void set(const FSJointRotation& jRot)
        {
            baseRotation.set(jRot.baseRotation);
            deltaRotation.set(jRot.deltaRotation);
        }
    };

private:
    std::string             mJointName = "";  // expected to be a match to LLJoint.getName() for a joint implementation.
    LLPointer<LLJointState> mJointState{ nullptr };

    /// <summary>
    /// Collision Volumes require special treatment when we stop animating an avatar, as they do not revert to their original state
    /// natively.
    /// </summary>
    bool mIsCollisionVolume{ false };

    FSJointRotation                       mRotation;
    std::deque<FSJointRotation>           mLastSetRotationDeltas;
    size_t                                mUndoneRotationIndex     = 0;
    std::chrono::system_clock::time_point mTimeLastUpdatedRotation = std::chrono::system_clock::now();

    LLVector3                             mPositionDelta;
    LLVector3                             mBeginningPosition;
    std::deque<LLVector3>                 mLastSetPositionDeltas;
    size_t                                mUndonePositionIndex     = 0;
    std::chrono::system_clock::time_point mTimeLastUpdatedPosition = std::chrono::system_clock::now();

    /// <summary>
    /// Joint scales require special treatment, as they do not revert when we stop animating an avatar.
    /// </summary>
    LLVector3                             mScaleDelta;
    LLVector3                             mBeginningScale;
    std::deque<LLVector3>                 mLastSetScaleDeltas;
    size_t                                mUndoneScaleIndex     = 0;
    std::chrono::system_clock::time_point mTimeLastUpdatedScale = std::chrono::system_clock::now();

    template <typename T>
    void addToUndo(T delta, size_t* undoIndex, std::deque<T>* dequeue, std::chrono::system_clock::time_point* timeLastUpdated);

    template <typename T> T undoLastChange(T thingToSet, size_t* undoIndex, std::deque<T>* dequeue);

    template <typename T> T redoLastChange(T thingToSet, size_t* undoIndex, std::deque<T>* dequeue);
};

#endif // FS_JOINTPPOSE_H
