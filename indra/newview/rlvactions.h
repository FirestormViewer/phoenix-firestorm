/**
 *
 * Copyright (c) 2009-2016, Kitty Barnett
 *
 * The source code in this file is provided to you under the terms of the
 * GNU Lesser General Public License, version 2.1, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE. Terms of the LGPL can be found in doc/LGPL-licence.txt
 * in this distribution, or online at http://www.gnu.org/licenses/lgpl-2.1.txt
 *
 * By copying, modifying or distributing this software, you acknowledge that
 * you have read and understood your obligations described above, and agree to
 * abide by those obligations.
 *
 */

#ifndef RLV_ACTIONS_H
#define RLV_ACTIONS_H

#include "rlvdefines.h"

// ============================================================================
// RlvActions class declaration - developer-friendly non-RLVa code facing class, use in lieu of RlvHandler whenever possible
//

class RlvActions
{
	// ======
	// Camera
	// ======
public:
	/*
	 * Returns true if the specified object cannot manipulate the camera FOV
	 */
	static bool canChangeCameraFOV(const LLUUID& idRlvObject);

	/*
	 * Returns true if the specified object can manipulate the camera offset and/or focus offset values
	 */
	static bool canChangeCameraPreset(const LLUUID& idRlvObject);

	/*
	 * Returns true if the user can switch to mouselook
	 */
	static bool canChangeToMouselook();

	/*
	 * Returns true if the camera's distance (from either the avatar of the focus) is currently restricted/clamped
	 */
	static bool isCameraDistanceClamped();

	/*
	 * Returns true if the camera's FOV is currently restricted/clamped
	 */
	static bool isCameraFOVClamped();

	/*
	 * Returns true if the camera offset and focus offset are locked (prevents changing the current camera preset)
	 */
	static bool isCameraPresetLocked();

	/*
	 * Retrieves the current (avatar or focus) camera distance limits
	 */
	static bool getCameraAvatarDistanceLimits(float& nDistMin, float& nDistMax);
	static bool getCameraOriginDistanceLimits(float& nDistMin, float& nDistMax);

	/*
	 * Retrieves the current camera FOV limits - returns isCameraFOVClamped()
	 */
	static bool getCameraFOVLimits(float& nFOVMin, float& nFOVMax);

	// ================================
	// Communication/Avatar interaction
	// ================================
public:
	/*
	 * Returns true if the user is allowed to change their currently active group
	 */
	static bool canChangeActiveGroup(const LLUUID& idRlvObject = LLUUID::null);

	/*
	 * Returns true if the user is allowed to receive IMs from the specified sender (can be an avatar or a group)
	 */
	static bool canReceiveIM(const LLUUID& idSender);

	/*
	 * Returns true if the user is allowed to send/play gestures (whether active ones from the chat bar or using the gesture preview floater)
	 */
	static bool canPlayGestures();

	/*
	 * Returns true if the user is allowed to chat on the specified channel
	 */
	static bool canSendChannel(int nChannel);

	/*
	 * Returns true if the user is allowed to send IMs to the specified recipient (can be an avatar or a group)
	 */
	static bool canSendIM(const LLUUID& idRecipient);

	/*
	 * Returns true if the user is allowed to start a - P2P or group - conversation with the specified UUID (or if the session already exists)
	 */
	static bool canStartIM(const LLUUID& idRecipient);

	/*
	 * Returns true if an avatar's name should be hidden for the requested operation/context
	 * (This is used to hide an avatar name in one case but not a near-identical case - such as teleporting a friend vs a nearby agent -
	 *  in a way that limits the amount of code that needs to be changed to carry context from one function to another)
	 */
	enum EShowNamesContext { SNC_DEFAULT = 0, SNC_NAMETAG, SNC_TELEPORTOFFER, SNC_TELEPORTREQUEST, SNC_COUNT };
	static bool canShowName(EShowNamesContext eContext, const LLUUID& idAgent = LLUUID::null);
	static void setShowName(EShowNamesContext eContext, bool fCanShowName) { if ( (eContext < SNC_COUNT) && (isRlvEnabled()) ) { s_BlockNamesContexts[eContext] = !fCanShowName; } }

	/*
	 * Returns true if the user is allowed to see the presence of nearby avatars in UI elements (anonymized or otherwise)
	 */
	static bool canShowNearbyAgents();

protected:
	// Backwards logic so that we can initialize to 0 and it won't block when we forget to/don't check if RLVa is disabled
	static bool s_BlockNamesContexts[SNC_COUNT];

	// ========
	// Movement
	// ========
public:
	/*
	 * Returns true if the user can accept an incoming teleport offer from the specified avatar
	 */
	static bool canAcceptTpOffer(const LLUUID& idSender);

	/*
	 * Returns true if a teleport offer from the specified avatar should be auto-accepted
	 * (pass the null UUID to check if all teleport offers should be auto-accepted regardless of sender)
	 */
	static bool autoAcceptTeleportOffer(const LLUUID& idSender);

	/*
	 * Returns true if the user can accept an incoming teleport request from the specified avatar
	 */
	static bool canAcceptTpRequest(const LLUUID& idSender);

	/*
	 * Returns true if a teleport request from the specified avatar should be auto-accepted
	 * (pass the null UUID to check if all teleport requests should be auto-accepted regardless of requester)
	 */
	static bool autoAcceptTeleportRequest(const LLUUID& idRequester);

	// ===========
	// Teleporting
	// ===========
public:
	/*
	 * Returns true if the user can teleport locally (short distances)
	 */
	static bool canTeleportToLocal(const LLVector3d& posGlobal);

	/*
	 * Returns true if the user can teleport to a (remote) location
	 */
	static bool canTeleportToLocation();

	/*
	 * Returns true if the teleport is considered local (e.g. double-click tp)
	 */
	static bool isLocalTp(const LLVector3d& posGlobal);

	// =================
	// World interaction
	// =================
public:
	/*
	 * Returns true if the user can edit the specified object
	 */
	static bool canEdit(const LLViewerObject* pObj);

	/*
	 * Returns true if the user can sit up on the specified object
	 */
	static bool canSit(const LLViewerObject* pObj, const LLVector3& posOffset = LLVector3::zero);

	/*
	 * Returns true if the user can stand up (returns true if the user isn't currently sitting)
	 */
	static bool canStand();
	static bool canStand(const LLUUID& idRlvObjExcept);

	/*
	 * Returns true if the user can see their in-world location
	 */
	static bool canShowLocation();

	// ================
	// Helper functions
	// ================
public:
	/*
	 * Convenience function to get the current/active value of a behaviour modifier
	 */
	template<typename T> static const T& getModifierValue(ERlvBehaviourModifier eBhvrMod);

	/*
	 * Convenience function to check for a behaviour without having to include rlvhandler.h.
	 * Do NOT call this function if speed is important (i.e. per-frame)
	 */
	static bool hasBehaviour(ERlvBehaviour eBhvr);

	/*
	 * Returns true if a - P2P or group - IM session is open with the specified UUID
	 */
	static bool hasOpenP2PSession(const LLUUID& idAgent);
	static bool hasOpenGroupSession(const LLUUID& idGroup);

	/*
	 * Convenience function to check if RLVa is enabled without having to include rlvhandler.h
	 */
	static bool isRlvEnabled();
};

// ============================================================================

#endif // RLV_ACTIONS_H
