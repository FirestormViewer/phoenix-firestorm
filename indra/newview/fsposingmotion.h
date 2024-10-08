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
        const size_t MaximumUndoQueueLength = 20;

        /// <summary>
        /// The constant time interval, in seconds, 
        /// </summary>
        std::chrono::duration<double> const _undoUpdateInterval = std::chrono::duration<double>(0.3);

        std::string              _jointName = "";  // expected to be a match to LLJoint.getName() for a joint implementation.
        LLPointer<LLJointState>  _jointState;

        /// <summary>
        /// Collision Volumes require special treatment when we stop animating an avatar, as they do not revert to their original state natively.
        /// </summary>
        bool                     _isCollisionVolume = false;

        LLQuaternion             _targetRotation;
        LLQuaternion             _beginningRotation;
        std::deque<LLQuaternion> _lastSetRotations;
        size_t                   _undoneRotationIndex = 0;
        std::chrono::system_clock::time_point _timeLastUpdatedRotation = std::chrono::system_clock::now();

        LLVector3               _targetPosition;
        LLVector3               _beginningPosition;
        std::deque<LLVector3>   _lastSetPositions;
        size_t                  _undonePositionIndex = 0;
        std::chrono::system_clock::time_point _timeLastUpdatedPosition = std::chrono::system_clock::now();

        /// <summary>
        /// Joint scales require special treatment, as they do not revert when we stop animating an avatar.
        /// </summary>
        LLVector3             _targetScale;
        LLVector3             _beginningScale;
        std::deque<LLVector3> _lastSetScales;
        size_t                _undoneScaleIndex = 0;
        std::chrono::system_clock::time_point _timeLastUpdatedScale = std::chrono::system_clock::now();

        /// <summary>
        /// Adds a last position to the deque.
        /// </summary>
        void addLastPositionToUndo()
        {
            if (_undonePositionIndex > 0)
            {
                for (int i = 0; i < _undonePositionIndex; i++)
                    _lastSetPositions.pop_front();

                _undonePositionIndex = 0;
            }

            _lastSetPositions.push_front(_targetPosition);

            while (_lastSetPositions.size() > MaximumUndoQueueLength)
                _lastSetPositions.pop_back();
        }

        /// <summary>
        /// Adds a last rotation to the deque.
        /// </summary>
        void addLastRotationToUndo()
        {
            if (_undoneRotationIndex > 0)
            {
                for (int i = 0; i < _undoneRotationIndex; i++)
                    _lastSetRotations.pop_front();

                _undoneRotationIndex = 0;
            }

            _lastSetRotations.push_front(_targetRotation);

            while (_lastSetRotations.size() > MaximumUndoQueueLength)
                _lastSetRotations.pop_back();
        }

        /// <summary>
        /// Adds a last rotation to the deque.
        /// </summary>
        void addLastScaleToUndo()
        {
            if (_undoneScaleIndex > 0)
            {
                for (int i = 0; i < _undoneScaleIndex; i++)
                    _lastSetScales.pop_front();

                _undoneScaleIndex = 0;
            }

            _lastSetScales.push_front(_targetScale);

            while (_lastSetScales.size() > MaximumUndoQueueLength)
                _lastSetScales.pop_back();
        }

        void setScale(LLVector3 scale)
        {
            LLJoint* joint = _jointState->getJoint();
            if (!joint)
                return;

            joint->setScale(scale);
        }

      public:
        /// <summary>
        /// Gets the name of the joint.
        /// </summary>
        std::string jointName() const { return _jointName; }

        /// <summary>
        /// Gets whether this represents a collision volume.
        /// </summary>
        /// <returns></returns>
        bool isCollisionVolume() const { return _isCollisionVolume; }

        /// <summary>
        /// Gets whether a redo of this joints rotation may be performed.
        /// </summary>
        /// <returns></returns>
        bool canRedoRotation() const { return _undoneRotationIndex > 0; }

        /// <summary>
        /// Gets the position the joint was in when the animation was initialized.
        /// </summary>
        LLVector3 getBeginningPosition() const { return _beginningPosition; }

        /// <summary>
        /// Gets the position the animator wishes the joint to be in.
        /// </summary>
        LLVector3 getTargetPosition() const { return _targetPosition; }

        /// <summary>
        /// Sets the position the animator wishes the joint to be in.
        /// </summary>
        void setTargetPosition(const LLVector3& pos)
        {
            auto timeIntervalSinceLastRotationChange = std::chrono::system_clock::now() - _timeLastUpdatedPosition;
            if (timeIntervalSinceLastRotationChange > _undoUpdateInterval)
                addLastPositionToUndo();

            _timeLastUpdatedPosition = std::chrono::system_clock::now();
            _targetPosition.set(pos);
        }

        /// <summary>
        /// Gets the rotation the joint was in when the animation was initialized.
        /// </summary>
        LLQuaternion getBeginningRotation() const { return _beginningRotation; }

        /// <summary>
        /// Gets the rotation the animator wishes the joint to be in.
        /// </summary>
        LLQuaternion getTargetRotation() const { return _targetRotation; }

        /// <summary>
        /// Sets the rotation the animator wishes the joint to be in.
        /// </summary>
        void setTargetRotation(const LLQuaternion& rot)
        {
            auto timeIntervalSinceLastRotationChange = std::chrono::system_clock::now() - _timeLastUpdatedRotation;
            if (timeIntervalSinceLastRotationChange > _undoUpdateInterval)
                addLastRotationToUndo();

            _timeLastUpdatedRotation = std::chrono::system_clock::now();
            _targetRotation.set(rot);
        }

        /// <summary>
        /// Undoes the last position set, if any.
        /// </summary>
        void undoLastPositionSet()
        {
            if (_lastSetPositions.empty())
                return;

            if (_undonePositionIndex == 0)  // at the top of the queue add the current
                addLastPositionToUndo();

            _undonePositionIndex++;
            _undonePositionIndex = llclamp(_undonePositionIndex, 0, _lastSetPositions.size() - 1);
            _targetPosition.set(_lastSetPositions[_undonePositionIndex]);
        }

        /// <summary>
        /// Undoes the last position set, if any.
        /// </summary>
        void redoLastPositionSet()
        {
            if (_lastSetPositions.empty())
                return;

            _undonePositionIndex--;
            _undonePositionIndex = llclamp(_undonePositionIndex, 0, _lastSetPositions.size() - 1);

            _targetPosition.set(_lastSetPositions[_undonePositionIndex]);
            if (_undonePositionIndex == 0)
                _lastSetPositions.pop_front();
        }

        /// <summary>
        /// Undoes the last rotation set, if any.
        /// Ordinarily the queue does not contain the current rotation, because we rely on time to add, and not button-up.
        /// When we undo, if we are at the top of the queue, we need to add the current rotation so we can redo back to it.
        /// Thus when we start undoing, _undoneRotationIndex points at the current rotation.
        /// </summary>
        void undoLastRotationSet()
        {
            if (_lastSetRotations.empty())
                return;

            if (_undoneRotationIndex == 0) // at the top of the queue add the current
                addLastRotationToUndo();

            _undoneRotationIndex++;
            _undoneRotationIndex = llclamp(_undoneRotationIndex, 0, _lastSetRotations.size() - 1);
            _targetRotation.set(_lastSetRotations[_undoneRotationIndex]);
        }

        /// <summary>
        /// Undoes the last rotation set, if any.
        /// </summary>
        void redoLastRotationSet()
        {
            if (_lastSetRotations.empty())
                return;

            _undoneRotationIndex--;
            _undoneRotationIndex = llclamp(_undoneRotationIndex, 0, _lastSetRotations.size() - 1);

            _targetRotation.set(_lastSetRotations[_undoneRotationIndex]);
            if (_undoneRotationIndex == 0)
                _lastSetRotations.pop_front();
        }

        /// <summary>
        /// Collision Volumes do not 'reset' their position/rotation when the animation stops.
        /// This requires special treatment to revert changes we've made this animation session.
        /// </summary>
        void revertCollisionVolume()
        {
            if (!_isCollisionVolume)
                return;

            LLJoint* joint = _jointState->getJoint();
            if (!joint)
                return;

            joint->setRotation(_beginningRotation);
            joint->setPosition(_beginningPosition);
            joint->setScale(_beginningScale);
        }

        LLVector3 getJointScale() const { return _targetScale; }
        void      setJointScale(LLVector3 scale)
        {
            auto timeIntervalSinceLastScaleChange = std::chrono::system_clock::now() - _timeLastUpdatedScale;
            if (timeIntervalSinceLastScaleChange > _undoUpdateInterval)
                addLastScaleToUndo();

            _timeLastUpdatedScale = std::chrono::system_clock::now();

            _targetScale.set(scale);
            setScale(_targetScale);
        }

        void undoLastScaleSet()
        {
            if (_lastSetScales.empty())
                return;

            if (_undoneScaleIndex == 0)  // at the top of the queue add the current
                addLastScaleToUndo();

            _undoneScaleIndex++;
            _undoneScaleIndex = llclamp(_undoneScaleIndex, 0, _lastSetScales.size() - 1);
            _targetScale.set(_lastSetScales[_undoneScaleIndex]);

            setScale(_targetScale);
        }

        void redoLastScaleSet()
        {
            if (_lastSetScales.empty())
                return;

            _undoneScaleIndex--;
            _undoneScaleIndex = llclamp(_undoneScaleIndex, 0, _lastSetScales.size() - 1);

            _targetScale.set(_lastSetScales[_undoneScaleIndex]);
            if (_undoneScaleIndex == 0)
                _lastSetScales.pop_front();

            setScale(_targetScale);
        }

        /// <summary>
        /// Restores the joint represented by this to the scale it had when this motion started.
        /// </summary>
        void revertJointScale()
        {
            _targetScale.set(_beginningScale);
            setScale(_beginningScale);
        }

        /// <summary>
        /// Restores the joint represented by this to the position it had when this motion started.
        /// </summary>
        void revertJointPosition()
        {
            LLJoint* joint = _jointState->getJoint();
            if (!joint)
                return;

            joint->setPosition(_beginningPosition);
        }

        /// <summary>
        /// Gets the pointer to the jointstate for the joint this represents.
        /// </summary>
        LLPointer<LLJointState> getJointState() const { return _jointState; }

        FSJointPose(LLJoint* joint, bool isCollisionVolume = false)
        {
            _jointState = new LLJointState;
            _jointState->setJoint(joint);
            _jointState->setUsage(POSER_JOINT_STATE);

            _jointName = joint->getName();
            _isCollisionVolume = isCollisionVolume;

            _beginningRotation = _targetRotation = joint->getRotation();
            _beginningPosition = _targetPosition = joint->getPosition();
            _beginningScale = _targetScale = joint->getScale();
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
    /// <returns>The unique, per-session, per-character motion identity.</returns>
    LLAssetID motionId() const { return _motionID; }

private:
    /// <summary>
    /// The kind of joint state this animation is concerned with changing.
    /// </summary>
    static const U32 POSER_JOINT_STATE = LLJointState::POS | LLJointState::ROT /* | LLJointState::SCALE*/;
    LLAssetID _motionID;

    /// <summary>
    /// The amount of time, in seconds, we use for transitioning between one animation-state to another; this affects the 'fluidity'
    /// of motion between changed to a joint.
    /// Use caution making this larger than the perceptual amount of time between adjusting a joint and then choosing to use 'undo'.
    /// Undo-function waits a similar amount of time after the last user-incited joint change to add a 'restore point'.
    /// </summary>
    const F32 _interpolationTime = 0.25f;

    /// <summary>
    /// The collection of joint poses this motion uses to pose the joints of the character this is animating. 
    /// </summary>
    std::vector<FSJointPose> _jointPoses;

    /// <summary>
    /// Because changes to positions, scales and collision volumes do not end when the animation stops,
    /// this is required to revert them manually.
    /// </summary>
    void revertChangesToPositionsScalesAndCollisionVolumes();
};

#endif // FS_POSINGMOTION_H

