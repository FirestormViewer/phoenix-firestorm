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
    /// Gets the 'public' rotation of the joint.
    /// </summary>
    LLQuaternion getPublicRotation() const { return mCurrentState.mRotation; }

    /// <summary>
    /// Sets the 'public' rotation of the joint.
    /// </summary>
    /// <remarks>
    /// 'Public rotation' is the amount of rotation the user has added to the initial state.
    /// Public rotation is what a user may save to an external format (such as BVH).
    /// This distinguishes 'private' rotation, which is the state inherited from something like a pose in-world.
    /// </remarks>
    void setPublicRotation(const LLQuaternion& rot);

    /// <summary>
    /// Reflects the base and delta rotation of the represented joint left-right.
    /// </summary>
    void reflectRotation();

    /// <summary>
    /// Sets the private rotation of the represented joint to zero.
    /// </summary>
    void zeroBaseRotation();

    /// <summary>
    /// Queries whether the represented joint is zero.
    /// </summary>
    /// <returns>True if the represented joint is zero, otherwise false.</returns>
    bool isBaseRotationZero() const;

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
    void recaptureJointAsDelta();


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
            mBaseRotation.set(joint->getRotation());
            mBasePosition.set(joint->getPosition());
            mBaseScale.set(joint->getScale());
        }

        FSJointState() = default;
        LLQuaternion mDeltaRotation;
        LLQuaternion getTargetRotation() const { return mRotation * mBaseRotation; }
        LLVector3    getTargetPosition() const { return mPosition + mBasePosition; }
        LLVector3    getTargetScale() const { return mScale + mBaseScale; }
        void updateRotation(const LLQuaternion& newRotation)
        { 
            auto inv_base = mBaseRotation;
            inv_base.conjugate();
            mDeltaRotation = newRotation * inv_base; 
        };
        

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
        }

        bool baseRotationIsZero() const { return mBaseRotation == LLQuaternion::DEFAULT; }

        void zeroBaseRotation() { mBaseRotation = LLQuaternion::DEFAULT; }

        void revertJointToBase(LLJoint* joint) const
        {
            if (!joint)
                return;

            joint->setRotation(mBaseRotation);
            joint->setPosition(mBasePosition);
            joint->setScale(mBaseScale);
        }

      private:
        FSJointState(FSJointState* state)
        {
            mBaseRotation.set(state->mBaseRotation);
            mBasePosition.set(state->mBasePosition);
            mBaseScale.set(state->mBaseScale);

            mRotation.set(state->mRotation);
            mPosition.set(state->mPosition);
            mScale.set(state->mScale);
        }

      public:
        LLQuaternion mRotation;
        LLVector3    mPosition;
        LLVector3    mScale;

      private:
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
