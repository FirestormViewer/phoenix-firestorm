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

#include "llviewerprecompiledheaders.h"
#include "llagent.h"
#include "llimview.h"
#include "llviewercamera.h"
#include "llvoavatarself.h"
#include "rlvactions.h"
#include "rlvhelper.h"
#include "rlvhandler.h"

// ============================================================================
// Camera
//

bool RlvActions::canChangeCameraFOV(const LLUUID& idRlvObject)
{
	// NOTE: if an object has exclusive camera controls then all other objects are locked out
	return (!gRlvHandler.hasBehaviour(RLV_BHVR_SETCAM)) || (gRlvHandler.hasBehaviour(idRlvObject, RLV_BHVR_SETCAM));
}

bool RlvActions::canChangeCameraPreset(const LLUUID& idRlvObject)
{
	// NOTE: if an object has exclusive camera controls then all other objects are locked out
	return
		( (!gRlvHandler.hasBehaviour(RLV_BHVR_SETCAM)) || (gRlvHandler.hasBehaviour(idRlvObject, RLV_BHVR_SETCAM)) ) &&
		(!gRlvHandler.hasBehaviour(RLV_BHVR_SETCAM_EYEOFFSET)) && (!gRlvHandler.hasBehaviour(RLV_BHVR_SETCAM_FOCUSOFFSET));
}

bool RlvActions::isCameraDistanceClamped()
{
	return
		(gRlvHandler.hasBehaviour(RLV_BHVR_SETCAM_AVDISTMIN)) || (gRlvHandler.hasBehaviour(RLV_BHVR_SETCAM_AVDISTMAX)) ||
		(gRlvHandler.hasBehaviour(RLV_BHVR_SETCAM_FOCUSDISTMIN)) || (gRlvHandler.hasBehaviour(RLV_BHVR_SETCAM_FOCUSDISTMAX));
}

bool RlvActions::isCameraFOVClamped()
{
	return (gRlvHandler.hasBehaviour(RLV_BHVR_SETCAM_FOVMIN)) || (gRlvHandler.hasBehaviour(RLV_BHVR_SETCAM_FOVMAX));
}

bool RlvActions::isCameraPresetLocked()
{
	return (gRlvHandler.hasBehaviour(RLV_BHVR_SETCAM)) || (gRlvHandler.hasBehaviour(RLV_BHVR_SETCAM_EYEOFFSET)) || (gRlvHandler.hasBehaviour(RLV_BHVR_SETCAM_FOCUSOFFSET));
}

bool RlvActions::getCameraAvatarDistanceLimits(float& nDistMin, float& nDistMax)
{
	bool fDistMin = gRlvHandler.hasBehaviour(RLV_BHVR_SETCAM_AVDISTMIN), fDistMax = gRlvHandler.hasBehaviour(RLV_BHVR_SETCAM_AVDISTMAX);
	if ( (fDistMin) || (fDistMax) )
	{
		static RlvCachedBehaviourModifier<float> sCamDistMin(RLV_MODIFIER_SETCAM_AVDISTMIN);
		static RlvCachedBehaviourModifier<float> sCamDistMax(RLV_MODIFIER_SETCAM_AVDISTMAX);

		nDistMax = (fDistMax) ? sCamDistMax : F32_MAX;
		nDistMin = (fDistMin) ? sCamDistMin : 0.0;
		return true;
	}
	return false;
}

bool RlvActions::getCameraFocusDistanceLimits(float& nDistMin, float& nDistMax)
{
	bool fDistMin = gRlvHandler.hasBehaviour(RLV_BHVR_SETCAM_FOCUSDISTMIN), fDistMax = gRlvHandler.hasBehaviour(RLV_BHVR_SETCAM_FOCUSDISTMAX);
	if ( (fDistMin) || (fDistMax) )
	{
		static RlvCachedBehaviourModifier<float> sCamDistMin(RLV_MODIFIER_SETCAM_FOCUSDISTMIN);
		static RlvCachedBehaviourModifier<float> sCamDistMax(RLV_MODIFIER_SETCAM_FOCUSDISTMAX);

		nDistMax = (fDistMax) ? sCamDistMax : F32_MAX;
		nDistMin = (fDistMin) ? sCamDistMin : 0.0;
		return true;
	}
	return false;
}

bool RlvActions::getCameraFOVLimits(F32& nFOVMin, F32& nFOVMax)
{
	bool fClampMin = gRlvHandler.hasBehaviour(RLV_BHVR_SETCAM_FOVMIN), fClampMax = gRlvHandler.hasBehaviour(RLV_BHVR_SETCAM_FOVMAX);
	if ( (fClampMin) || (fClampMax) )
	{
		static RlvCachedBehaviourModifier<float> sCamFovMin(RLV_MODIFIER_SETCAM_FOVMIN);
		static RlvCachedBehaviourModifier<float> sCamFovMax(RLV_MODIFIER_SETCAM_FOVMAX);

		nFOVMin = (fClampMin) ? sCamFovMin : LLViewerCamera::getInstance()->getMinView();
		nFOVMax = (fClampMax) ? sCamFovMax : LLViewerCamera::getInstance()->getMaxView();
		return true;
	}
	return false;
}

// ============================================================================
// Communication/Avatar interaction
// 

bool RlvActions::s_BlockNamesContexts[SNC_COUNT] = { 0 };

// Checked: 2010-11-30 (RLVa-1.3.0)
bool RlvActions::canReceiveIM(const LLUUID& idSender)
{
	// User can receive an IM from "sender" (could be an agent or a group) if:
	//   - not generally restricted from receiving IMs (or the sender is an exception)
	//   - not specifically restricted from receiving an IM from the sender
	return 
		(!rlv_handler_t::isEnabled()) ||
		( ( (!gRlvHandler.hasBehaviour(RLV_BHVR_RECVIM)) || (gRlvHandler.isException(RLV_BHVR_RECVIM, idSender)) ) &&
		  ( (!gRlvHandler.hasBehaviour(RLV_BHVR_RECVIMFROM)) || (!gRlvHandler.isException(RLV_BHVR_RECVIMFROM, idSender)) ) );
}

// Checked: 2010-11-30 (RLVa-1.3.0)
bool RlvActions::canSendIM(const LLUUID& idRecipient)
{
	// User can send an IM to "recipient" (could be an agent or a group) if:
	//   - not generally restricted from sending IMs (or the recipient is an exception)
	//   - not specifically restricted from sending an IM to the recipient
	return 
		(!rlv_handler_t::isEnabled()) ||
		( ( (!gRlvHandler.hasBehaviour(RLV_BHVR_SENDIM)) || (gRlvHandler.isException(RLV_BHVR_SENDIM, idRecipient)) ) &&
		  ( (!gRlvHandler.hasBehaviour(RLV_BHVR_SENDIMTO)) || (!gRlvHandler.isException(RLV_BHVR_SENDIMTO, idRecipient)) ) );
}

bool RlvActions::canStartIM(const LLUUID& idRecipient)
{
	// User can start an IM session with "recipient" (could be an agent or a group) if:
	//   - not generally restricted from starting IM sessions (or the recipient is an exception)
	//   - not specifically restricted from starting an IM session with the recipient
	//   - the session already exists
	return 
		(!rlv_handler_t::isEnabled()) ||
		( ( (!gRlvHandler.hasBehaviour(RLV_BHVR_STARTIM)) || (gRlvHandler.isException(RLV_BHVR_STARTIM, idRecipient)) ) &&
		  ( (!gRlvHandler.hasBehaviour(RLV_BHVR_STARTIMTO)) || (!gRlvHandler.isException(RLV_BHVR_STARTIMTO, idRecipient)) ) ) ||
		( (hasOpenP2PSession(idRecipient)) || (hasOpenGroupSession(idRecipient)) );
}

// ============================================================================
// Movement
// 

bool RlvActions::canAcceptTpOffer(const LLUUID& idSender)
{
	return ((!gRlvHandler.hasBehaviour(RLV_BHVR_TPLURE)) || (gRlvHandler.isException(RLV_BHVR_TPLURE, idSender))) && (canStand());
}

bool RlvActions::autoAcceptTeleportOffer(const LLUUID& idSender)
{
	return ((idSender.notNull()) && (gRlvHandler.isException(RLV_BHVR_ACCEPTTP, idSender))) || (gRlvHandler.hasBehaviour(RLV_BHVR_ACCEPTTP));
}

bool RlvActions::canAcceptTpRequest(const LLUUID& idSender)
{
	return (!gRlvHandler.hasBehaviour(RLV_BHVR_TPREQUEST)) || (gRlvHandler.isException(RLV_BHVR_TPREQUEST, idSender));
}

bool RlvActions::autoAcceptTeleportRequest(const LLUUID& idRequester)
{
	return ((idRequester.notNull()) && (gRlvHandler.isException(RLV_BHVR_ACCEPTTPREQUEST, idRequester))) || (gRlvHandler.hasBehaviour(RLV_BHVR_ACCEPTTPREQUEST));
}

// ============================================================================
// Teleporting
//

bool RlvActions::canTeleportToLocal()
{
	return (!gRlvHandler.hasBehaviour(RLV_BHVR_SITTP)) && (!gRlvHandler.hasBehaviour(RLV_BHVR_TPLOCAL)) && (RlvActions::canStand());
}

bool RlvActions::canTeleportToLocation()
{
	// NOTE: if we're teleporting due to an active command we should disregard any restrictions from the same object
	const LLUUID& idRlvObjExcept = gRlvHandler.getCurrentObject();
	return (!gRlvHandler.hasBehaviourExcept(RLV_BHVR_TPLOC, idRlvObjExcept)) && (!gRlvHandler.hasBehaviourExcept(RLV_BHVR_TPLOCAL, idRlvObjExcept)) && (RlvActions::canStand(idRlvObjExcept));
}

bool RlvActions::isLocalTp(const LLVector3d& posGlobal)
{
	F32 nDistSq = (LLVector2(posGlobal.mdV[0], posGlobal.mdV[1]) - LLVector2(gAgent.getPositionGlobal().mdV[0], gAgent.getPositionGlobal().mdV[1])).lengthSquared();
	return nDistSq < RLV_TELEPORT_LOCAL_RADIUS * RLV_TELEPORT_LOCAL_RADIUS;
}

// ============================================================================
// World interaction
//

bool RlvActions::canStand()
{
	// NOTE: return FALSE only if we're @unsit=n restricted and the avie is currently sitting on something and TRUE for everything else
	return (!gRlvHandler.hasBehaviour(RLV_BHVR_UNSIT)) || ((isAgentAvatarValid()) && (!gAgentAvatarp->isSitting()));
}

bool RlvActions::canStand(const LLUUID& idRlvObjExcept)
{
	// NOTE: must match generic function above
	return (!gRlvHandler.hasBehaviourExcept(RLV_BHVR_UNSIT, idRlvObjExcept)) || ((isAgentAvatarValid()) && (!gAgentAvatarp->isSitting()));
}

// Checked: 2014-02-24 (RLVa-1.4.10)
bool RlvActions::canShowLocation()
{
	return !gRlvHandler.hasBehaviour(RLV_BHVR_SHOWLOC);
}

// ============================================================================
// Helper functions
// 

// Checked: 2013-05-10 (RLVa-1.4.9)
bool RlvActions::hasBehaviour(ERlvBehaviour eBhvr)
{
	return gRlvHandler.hasBehaviour(eBhvr);
}

// Checked: 2013-05-09 (RLVa-1.4.9)
bool RlvActions::hasOpenP2PSession(const LLUUID& idAgent)
{
	const LLUUID idSession = LLIMMgr::computeSessionID(IM_NOTHING_SPECIAL, idAgent);
	return (idSession.notNull()) && (LLIMMgr::instance().hasSession(idSession));
}

// Checked: 2013-05-09 (RLVa-1.4.9)
bool RlvActions::hasOpenGroupSession(const LLUUID& idGroup)
{
	return (idGroup.notNull()) && (LLIMMgr::instance().hasSession(idGroup));
}

// Checked: 2013-11-08 (RLVa-1.4.9)
bool RlvActions::isRlvEnabled()
{
	return RlvHandler::isEnabled();
}

// ============================================================================
