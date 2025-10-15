/**
 * @file fsposestate.h
 * @brief a means to save and restore the instantaneous state of animations posing an avatar.
 *
 * $LicenseInfo:firstyear=2025&license=viewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (c) 2025 Angeldark Raymaker @ Second Life
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

#ifndef LL_FSPoseState_H
#define LL_FSPoseState_H

#include "llvoavatar.h"
#include "fsposingmotion.h"

class FSPoseState
{
public:
    FSPoseState() = default;
    virtual ~FSPoseState() = default;

public:
    /// <summary>
    /// Captures the current animations posing the supplied avatar and how long they have been playing.
    /// </summary>
    /// <param name="avatar">The avatar whose animations are to be captured.</param>
    void captureMotionStates(LLVOAvatar* avatar);

    /// <summary>
    /// Updates the stored list of animations posing the avatar.
    /// </summary>
    /// <param name="avatar">The avatar whose animations are to be captured.</param>
    /// <param name="posingMotion">The posing motion.</param>
    /// <param name="jointNamesRecaptured">The names of the joints being recaptured.</param>
    void updateMotionStates(LLVOAvatar* avatar, FSPosingMotion* posingMotion, std::vector<S32> jointNamesRecaptured);

    /// <summary>
    /// Removes all current animation states for the supplied avatar.
    /// </summary>
    /// <param name="avatar">The avatar whose animations are to be purged.</param>
    void purgeMotionStates(LLVOAvatar* avatar);

    /// <summary>
    /// Writes any documented poses for the supplied avatar to the supplied stream.
    /// </summary>
    /// <param name="avatar">The avatar whose animations may have been captured.</param>
    /// <param name="ignoreOwnership">Whether to ignore ownership. For use when preparing saveRecord to send to another by collab.</param>
    /// <param name="saveRecord">The record to add to.</param>
    void writeMotionStates(LLVOAvatar* avatar, bool ignoreOwnership, LLSD* saveRecord);

    /// <summary>
    /// Restores pose state(s) from the supplied record.
    /// </summary>
    /// <param name="avatar">The avatar whose animations may have been captured.</param>
    /// <param name="ignoreOwnership">Whether to ignore ownership. For use when reading a local file.</param>
    /// <param name="pose">The record to read from.</param>
    void restoreMotionStates(LLVOAvatar* avatar, bool ignoreOwnership, LLSD pose);

    /// <summary>
    /// Applies the motion states for the supplied avatar to the supplied motion.
    /// </summary>
    /// <param name="avatar">The avatar to apply the motion state(s) to.</param>
    /// <param name="posingMotion">The posing motion to apply the state(s) to.</param>
    /// <returns>True if all the motion states for the supplied avatar have been applied, otherwise false.</returns>
    /// <remarks>
    /// In some ways this is like an AO: loading LLKeyframeMotions.
    /// Once loaded, the LLKeyframeMotion is put at time fsMotionState.lastUpdateTime.
    /// The joint-rotations for that LLKeyframeMotion are then restored to the base.
    /// This examines sMotionStates for any avatarId matches; such as after a restoreMotionStates(...).
    /// This could result in loading assets, thus a particular member of sMotionStates may take several attempts to load.
    /// Motion(s) that the avatar does not have permissions for are not considered in the return boolean.
    /// </remarks>
    bool applyMotionStatesToPosingMotion(LLVOAvatar* avatar, FSPosingMotion* posingMotion);

private:
    /// <summary>
    /// A class documenting the state of an animation for an avatar.
    /// </summary>
    class fsMotionState
    {
    public:
        /// <summary>
        /// The motion ID recorded animating the avatar ID.
        /// </summary>
        LLAssetID motionId;

        /// <summary>
        /// The play-time the motionId had progressed until the motion was captured.
        /// </summary>
        F32 lastUpdateTime = 0.f;

        /// <summary>
        /// Upon reloading, whether this record has been applied to the avatar.
        /// </summary>
        bool motionApplied = false;

        /// <summary>
        /// Whether gAgent owns the pose, or the pose was loaded from XML.
        /// </summary>
        bool gAgentOwnsPose = false;

        /// <summary>
        /// Represents 'capture layers: how the user layers animations 'on top of' others.
        /// </summary>
        int captureOrder = 0;

        /// <summary>
        /// Represents in-layer order of capture.
        /// </summary>
        int inLayerOrder = 0;

        /// <summary>
        /// When reloading, and if not-empty, the bone-numbers this motionId should affect.
        /// </summary>
        std ::vector<S32> jointNumbersAnimated;
    };

    /// <summary>
    /// Resets the priority for the named joints for the supplied posing motion at the supplied capture order.
    /// </summary>
    /// <param name="avatar">The avatar being posed by the motion.</param>
    /// <param name="posingMotion">The posing motion.</param>
    /// <param name="captureOrder">The order of the capture.</param>
    void resetPriorityForCaptureOrder(LLVOAvatar* avatar, FSPosingMotion* posingMotion, int captureOrder);

    /// <summary>
    /// Gets whether gAgentID owns, and thus can save information about the supplied asset ID.
    /// </summary>
    /// <param name="avatar">The avatar to query ownership for.</param>
    /// <param name="motionId">The asset ID of the object.</param>
    /// <returns>True if the avatar owns the asset, otherwise false.</returns>
    /// <remarks>
    /// This only works reliably if something other than poser started the animation.
    /// </remarks>
    bool canSaveMotionId(LLAssetID motionId);

    /// <summary>
    /// Tests if all the members of supplied vector2 are members of supplied vector1.
    /// </summary>
    /// <param name="vector1">The super-set.</param>
    /// <param name="vector2">The possible sub-set.</param>
    /// <returns>True if all members of vector2 are members of vector1, otherwise false.</returns>
    bool vector2IsSubsetOfVector1(std::vector<S32> vector1, std::vector<S32> vector2);

    /// <summary>
    /// Two symmetric methods for (de)serializing vectors to both XML and collab-safe short-as-possible strings and back again.
    /// </summary>
    /// <remarks>
    /// Collab-safe means ASCII-printable chars, and delimiter usage does not conflict with Collab's delimiter.
    /// </remarks>
    std::string encodeVectorToString(std::vector<S32> vector);
    std::vector<S32> decodeStringToVector(std::string vector);

    struct compareByCaptureOrder
    {
        bool operator()(const fsMotionState& a, const fsMotionState& b)
        {
            if (a.captureOrder < b.captureOrder)
                return true; // Ascending order
            if (a.captureOrder == b.captureOrder && a.inLayerOrder < b.inLayerOrder)
                return true; // Ascending order in layer

            return false;
        }
    };

    static std::map <LLUUID, std::vector<fsMotionState>> sMotionStates;
    static std::map<LLUUID, int> sCaptureOrder;
    static std::map<LLUUID, bool> sMotionStatesOwnedByMe;
};

#endif // LL_FSPoseState_H
