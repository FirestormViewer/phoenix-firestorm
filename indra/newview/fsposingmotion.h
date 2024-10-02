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

#ifndef LL_FSPOSINGMOTION_H
#define LL_FSPOSINGMOTION_H

//-----------------------------------------------------------------------------
// Header files
//-----------------------------------------------------------------------------
#include "llmotion.h"
#include "lljointsolverrp3.h"
#include "v3dmath.h"

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
        std::string             _jointName = "";  // expected to be a match to LLJoint.getName() for a joint implementation.
        LLQuaternion            _beginningRotation;
        LLQuaternion            _targetRotation;
        LLVector3               _targetPosition;
        LLVector3               _beginningPosition;
        LLPointer<LLJointState> _jointState;

      public:
        /// <summary>
        /// Gets the name of the joint.
        /// </summary>
        std::string jointName() const { return _jointName; }

        LLVector3 getBeginningPosition() const { return _beginningPosition; }
        LLVector3 getTargetPosition() const { return _targetPosition; }
        void      setTargetPosition(const LLVector3& pos) { _targetPosition.set(pos) ; }

        LLQuaternion getBeginningRotation() const { return _beginningRotation; }
        LLQuaternion getTargetRotation() const { return _targetRotation; }
        void         setTargetRotation(const LLQuaternion& rot) { _targetRotation.set(rot); }

        LLPointer<LLJointState> getJointState() const { return _jointState; }

        FSJointPose(LLJoint* joint)
        {
            _jointState = new LLJointState;
            _jointState->setJoint(joint);
            _jointState->setUsage(POSER_JOINT_STATE);

            _jointName = joint->getName();

            _beginningRotation = _targetRotation = joint->getRotation();
            _beginningPosition = _targetPosition = joint->getPosition();
        }
    };

public:
    virtual bool getLoop() { return TRUE; }

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
    /// Adds the supplied joint to the current animation-state.
    /// </summary>
    /// <param name="joint">The joint to animate.</param>
    void addJointToState(LLJoint *joint);

    /// <summary>
    /// Removes the supplied joint to the current animation-state.
    /// </summary>
    /// <param name="joint">The joint to stop animating.</param>
    void removeJointFromState(LLJoint *joint);

    /// <summary>
    /// Queries whether the supplied joint is being animated.
    /// </summary>
    /// <param name="joint">The joint to query.</param>
    bool currentlyPosingJoint(LLJoint *joint);

    /// <summary>
    /// Removes the current joint state, and adds a new one.
    /// </summary>
    void setJointState(LLJoint* joint, U32 state);

    /// <summary>
    /// Gets the joint pose by name.
    /// </summary>
    /// <param name="name">The name of the joint to get the pose for.</param>
    /// <returns>The matching joint pose, if found, otherwise null.</returns>
    FSJointPose* getJointPoseByJointName(std::string name);

    /// <summary>
    /// Gets the motion identity for this animation.
    /// </summary>
    /// <returns>The unique, per-session motion identity.</returns>
    LLAssetID motionId() const { return _motionID; }

private:
    static const U32 POSER_JOINT_STATE = LLJointState::POS | LLJointState::ROT /* | LLJointState::SCALE*/;
    LLAssetID _motionID;

    const F32    _interpolationTime = 0.25f;

    LLFrameTimer _interpolationTimer;
    LLCharacter *_character;
    std::vector<FSJointPose> _jointPoses;
};

#endif // LL_LLKEYFRAMEMOTION_H

