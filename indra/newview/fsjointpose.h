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
    /// Gets the 'public' position of the joint.
    /// </summary>
    LLVector3 getPublicPosition() const { return mCurrentState.mPosition; }

    /// <summary>
    /// Sets the 'public' position of the joint.
    /// </summary>
    void setPublicPosition(const LLVector3& pos);

    /// <summary>
    /// Undoes the last position set, if any.
    /// </summary>
    void undoLastChange();

    /// <summary>
    /// Undoes the last position set, if any.
    /// </summary>
    void redoLastChange();

    /// <summary>
    /// Resets the joint to its conditions when posing started.
    /// </summary>
    void resetJoint();

    /// <summary>
    /// Gets the 'public' rotation of the joint.
    /// </summary>
    LLQuaternion getPublicRotation() const { return mCurrentState.mRotation; }

    /// <summary>
    /// Sets the 'public' rotation of the joint.
    /// </summary>
    /// <param name="zeroBase">Whether to zero the base rotation on setting the supplied rotation.</param>
    /// <param name="rot">The change in rotation to apply.</param>
    /// <remarks>
    /// 'Public rotation' is the amount of rotation the user has added to the initial state.
    /// Public rotation is what a user may save to an external format (such as BVH).
    /// This distinguishes 'private' rotation, which is the state inherited from something like a pose in-world.
    /// </remarks>
    void setPublicRotation(bool zeroBase, const LLQuaternion& rot);

    /// <summary>
    /// Reflects the base and delta rotation of the represented joint left-right.
    /// </summary>
    void reflectRotation();

    /// <summary>
    /// Sets the private rotation of the represented joint to zero.
    /// </summary>
    /// <param name="lockInBvh">Whether the joint should be locked if exported to BVH.</param>
    void zeroBaseRotation(bool lockInBvh);

    /// <summary>
    /// Queries whether the represented joint is zero.
    /// </summary>
    /// <returns>True if the represented joint is zero, otherwise false.</returns>
    bool isBaseRotationZero() const;

    /// <summary>
    /// Gets whether an undo of this joint may be performed.
    /// </summary>
    /// <returns>true if the joint may have a undo applied, otherwise false.</returns>
    bool canPerformUndo() const;

    /// <summary>
    /// Gets whether a redo of this joint may be performed.
    /// </summary>
    /// <returns>true if the joint may have a redo applied, otherwise false.</returns>
    bool canPerformRedo() const { return mUndoneJointStatesIndex > 0; }

    /// <summary>
    /// Gets the 'public' scale of the joint.
    /// </summary>
    LLVector3 getPublicScale() const { return mCurrentState.mScale; }

    /// <summary>
    /// Sets the 'public' scale of the joint.
    /// </summary>
    void setPublicScale(const LLVector3& scale);

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
    /// Recalculates the delta reltive to the base for a new rotation.
    /// </summary>
    /// <param name="zeroBase">Whether to zero the base rotation on setting the supplied rotation.</param>
    /// <returns>The rotation of the public difference between before and after recapture.</returns>
    LLQuaternion recaptureJointAsDelta(bool zeroBase);

    /// <summary>
    /// Clears the undo/redo deque.
    /// </summary>
    void purgeUndoQueue();

    /// <summary>
    /// Gets whether the user has specified the base rotation of a joint to be zero.
    /// </summary>
    /// <returns>True if the user performed some action to specify zero rotation as the base, otherwise false.</returns>
    bool userHasSetBaseRotationToZero() const;

    /// <summary>
    /// Gets whether the rotation of a joint has been 'locked' so that its world rotation can remain constant while parent joints change.
    /// </summary>
    /// <returns>True if the joint is rotationally locked to the world, otherwise false.</returns>
    bool getWorldRotationLockState() const;

    /// <summary>
    /// Sets whether the world-rotation of a joint has been 'locked' so that as its parent joints change rotation or position, this joint keeps a constant world rotation.
    /// </summary>
    /// <param name="newState">The new state for the world-rotation lock.</param>
    void setWorldRotationLockState(bool newState);

    /// <summary>
    /// Reverts the position/rotation/scale to their values when the animation begun.
    /// This treatment is required for certain joints, particularly Collision Volumes and those bones not commonly animated by an AO.
    /// </summary>
    void revertJoint();

    LLQuaternion getTargetRotation() const { return mCurrentState.getTargetRotation(); }
    LLVector3    getTargetPosition() const { return mCurrentState.getTargetPosition(); }
    LLVector3    getTargetScale() const { return mCurrentState.getTargetScale(); }

    /// <summary>
    /// Gets the pointer to the jointstate for the joint this represents.
    /// </summary>
    LLPointer<LLJointState> getJointState() const { return mJointState; }

    class FSJointState
    {
      public:
        FSJointState(LLJoint* joint)
        {
            mStartingRotation.set(joint->getRotation());
            mBaseRotation.set(joint->getRotation());
            mBasePosition.set(joint->getPosition());
            mBaseScale.set(joint->getScale());
        }

        FSJointState() = default;
        LLQuaternion getTargetRotation() const { return mRotation * mBaseRotation; }
        LLVector3    getTargetPosition() const { return mPosition + mBasePosition; }
        LLVector3    getTargetScale() const { return mScale + mBaseScale; }

        void reflectRotation()
        {
            mBaseRotation.mQ[VX] *= -1;
            mBaseRotation.mQ[VZ] *= -1;
            mRotation.mQ[VX] *= -1;
            mRotation.mQ[VZ] *= -1;
        }

        void cloneRotationFrom(FSJointState otherState)
        {
            mBaseRotation.set(otherState.mBaseRotation);
            mRotation.set(otherState.mRotation);
            mUserSpecifiedBaseZero = otherState.mUserSpecifiedBaseZero;
        }

        bool baseRotationIsZero() const { return mBaseRotation == LLQuaternion::DEFAULT; }

        void resetJoint()
        {
            mUserSpecifiedBaseZero = false;
            mRotationIsWorldLocked = false;
            mBaseRotation.set(mStartingRotation);
            mRotation.set(LLQuaternion::DEFAULT);
            mPosition.setZero();
            mScale.setZero();
        }

        void zeroBaseRotation() { mBaseRotation = LLQuaternion::DEFAULT; }

        void revertJointToBase(LLJoint* joint) const
        {
            if (!joint)
                return;

            joint->setRotation(mBaseRotation);
            joint->setPosition(mBasePosition);
            joint->setScale(mBaseScale);
        }

        LLQuaternion updateFromJoint(LLJoint* joint, bool zeroBase)
        {
            if (!joint)
                return LLQuaternion::DEFAULT;

            LLQuaternion initalPublicRot = mRotation;
            LLQuaternion invRot = mBaseRotation;
            invRot.conjugate();
            LLQuaternion newPublicRot = joint->getRotation() * invRot;

            if (zeroBase)
            {
                mUserSpecifiedBaseZero = zeroBase;
                zeroBaseRotation();
            }

            mRotation.set(newPublicRot);
            mPosition.set(joint->getPosition() - mBasePosition);
            mScale.set(joint->getScale() - mBaseScale);

            return newPublicRot *= ~initalPublicRot;
        }

      private:
        FSJointState(FSJointState* state)
        {
            mStartingRotation.set(state->mStartingRotation);
            mBaseRotation.set(state->mBaseRotation);
            mBasePosition.set(state->mBasePosition);
            mBaseScale.set(state->mBaseScale);

            mRotation.set(state->mRotation);
            mPosition.set(state->mPosition);
            mScale.set(state->mScale);
            mUserSpecifiedBaseZero = state->mUserSpecifiedBaseZero;
            mRotationIsWorldLocked = state->mRotationIsWorldLocked;
        }

      public:
        LLQuaternion mRotation;
        LLVector3    mPosition;
        LLVector3    mScale;
        bool         mRotationIsWorldLocked = false;

        /// <summary>
        /// A value indicating whether the user has explicitly set the base rotation to zero.
        /// </summary>
        /// <remarks>
        /// The base-rotation, representing any 'current animation' state when posing starts, may become zero for several reasons.
        /// Loading a Pose, editing a rotation intended to save to BVH, or setting to 'T-Pose' being examples.
        /// If a user intends on creating a BVH, zero-rotation has a special meaning upon upload: the joint is free (is not animated by that BVH).
        /// This value represents the explicit intent to have that joint be 'free' in BVH (which is sometimes undesireable).
        /// </remarks>
        bool mUserSpecifiedBaseZero = false;

      private:
        LLQuaternion mStartingRotation;
        LLQuaternion mBaseRotation;
        LLVector3    mBasePosition;
        LLVector3    mBaseScale;
    };

  private:
    std::string             mJointName = "";  // expected to be a match to LLJoint.getName() for a joint implementation.
    LLPointer<LLJointState> mJointState{ nullptr };

    /// <summary>
    /// Collision Volumes require special treatment when we stop animating an avatar, as they do not revert to their original state
    /// natively.
    /// </summary>
    bool mIsCollisionVolume{ false };

    std::deque<FSJointState>              mLastSetJointStates;
    size_t                                mUndoneJointStatesIndex      = 0;
    std::chrono::system_clock::time_point mTimeLastUpdatedCurrentState = std::chrono::system_clock::now();

    FSJointState mCurrentState;

    void addStateToUndo(FSJointState stateToAddToUndo);
    FSJointState undoLastStateChange(FSJointState currentState);
    FSJointState redoLastStateChange(FSJointState currentState);
};

#endif // FS_JOINTPPOSE_H
