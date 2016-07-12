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
#include "llworld.h"
#include "rlvactions.h"
#include "rlvhelper.h"
#include "rlvhandler.h"

// ============================================================================
// Camera
//

bool RlvActions::canChangeCameraFOV(const LLUUID& idRlvObject)
{
	// NOTE: if an object has exclusive camera control then all other objects are locked out
	return (!gRlvHandler.hasBehaviour(RLV_BHVR_SETCAM)) || (gRlvHandler.hasBehaviour(idRlvObject, RLV_BHVR_SETCAM));
}

bool RlvActions::canChangeCameraPreset(const LLUUID& idRlvObject)
{
	// NOTE: if an object has exclusive camera control then all other objects are locked out
	return
		( (!gRlvHandler.hasBehaviour(RLV_BHVR_SETCAM)) || (gRlvHandler.hasBehaviour(idRlvObject, RLV_BHVR_SETCAM)) ) &&
		(!gRlvHandler.hasBehaviour(RLV_BHVR_SETCAM_EYEOFFSET)) && (!gRlvHandler.hasBehaviour(RLV_BHVR_SETCAM_FOCUSOFFSET));
}

bool RlvActions::canChangeToMouselook()
{
	// User can switch to mouselook if:
	//   - not specifically prevented from going into mouselook (NOTE: if an object has exclusive camera control only that object can prevent mouselook)
	//   - there is no minimum camera distance defined (or it's higher than > 0m)
	const RlvBehaviourModifier* pCamDistMinModifier = RlvBehaviourDictionary::instance().getModifier(RLV_MODIFIER_SETCAM_AVDISTMIN);
	return
		( (!gRlvHandler.hasBehaviour(RLV_BHVR_SETCAM)) ? !gRlvHandler.hasBehaviour(RLV_BHVR_SETCAM_MOUSELOOK) : !gRlvHandler.hasBehaviour(pCamDistMinModifier->getPrimaryObject(), RLV_BHVR_SETCAM_MOUSELOOK) ) &&
		( (!pCamDistMinModifier->hasValue()) || (pCamDistMinModifier->getValue<float>() == 0.f) );
}

bool RlvActions::isCameraDistanceClamped()
{
	return
		(gRlvHandler.hasBehaviour(RLV_BHVR_SETCAM_AVDISTMIN)) || (gRlvHandler.hasBehaviour(RLV_BHVR_SETCAM_AVDISTMAX)) ||
		(gRlvHandler.hasBehaviour(RLV_BHVR_SETCAM_ORIGINDISTMIN)) || (gRlvHandler.hasBehaviour(RLV_BHVR_SETCAM_ORIGINDISTMAX));
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

bool RlvActions::getCameraOriginDistanceLimits(float& nDistMin, float& nDistMax)
{
	bool fDistMin = gRlvHandler.hasBehaviour(RLV_BHVR_SETCAM_ORIGINDISTMIN), fDistMax = gRlvHandler.hasBehaviour(RLV_BHVR_SETCAM_ORIGINDISTMAX);
	if ( (fDistMin) || (fDistMax) )
	{
		static RlvCachedBehaviourModifier<float> sCamDistMin(RLV_MODIFIER_SETCAM_ORIGINDISTMIN);
		static RlvCachedBehaviourModifier<float> sCamDistMax(RLV_MODIFIER_SETCAM_ORIGINDISTMAX);

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

bool RlvActions::canChangeActiveGroup(const LLUUID& idRlvObject)
{
	// User can change their active group if:
	//   - not specifically restricted (by another object that the one specified) from changing their active group
	return (idRlvObject.isNull()) ? !gRlvHandler.hasBehaviour(RLV_BHVR_SETGROUP) : !gRlvHandler.hasBehaviourExcept(RLV_BHVR_SETGROUP, idRlvObject);
}

// Little helper function to check the IM exclusion range for @recvim, @sendim and @startim (returns: min_dist <= (pos user - pos target) <= max_dist)
static bool rlvCheckAvatarIMDistance(const LLUUID& idAvatar, ERlvBehaviourModifier eModDistMin, ERlvBehaviourModifier eModDistMax)
{
	LLVector3d posAgent;
	const RlvBehaviourModifier *pBhvrModDistMin = RlvBehaviourDictionary::instance().getModifier(eModDistMin), *pBhvrModDistMax = RlvBehaviourDictionary::instance().getModifier(eModDistMax);
	if ( ((pBhvrModDistMin->hasValue()) || (pBhvrModDistMax->hasValue())) && (LLWorld::getInstance()->getAvatar(idAvatar, posAgent)) )
	{
		float nDist = llabs(dist_vec_squared(gAgent.getPositionGlobal(), posAgent));
		return (nDist >= pBhvrModDistMin->getValue<float>()) && (nDist <= pBhvrModDistMax->getValue<float>());
	}
	return false;
}

bool RlvActions::canReceiveIM(const LLUUID& idSender)
{
	// User can receive an IM from "sender" (could be an agent or a group) if:
	//   - not generally restricted from receiving IMs (or the sender is an exception or inside the exclusion range)
	//   - not specifically restricted from receiving an IM from the sender
	return
		(!isRlvEnabled()) ||
		( ( (!gRlvHandler.hasBehaviour(RLV_BHVR_RECVIM)) || (gRlvHandler.isException(RLV_BHVR_RECVIM, idSender)) || (rlvCheckAvatarIMDistance(idSender, RLV_MODIFIER_RECVIMDISTMIN, RLV_MODIFIER_RECVIMDISTMAX)) ) &&
		  ( (!gRlvHandler.hasBehaviour(RLV_BHVR_RECVIMFROM)) || (!gRlvHandler.isException(RLV_BHVR_RECVIMFROM, idSender)) ) );
}

bool RlvActions::canPlayGestures()
{
	return (!gRlvHandler.hasBehaviour(RLV_BHVR_SENDGESTURE));
}

bool RlvActions::canSendChannel(int nChannel)
{
	return
		( (!gRlvHandler.hasBehaviour(RLV_BHVR_SENDCHANNEL)) || (gRlvHandler.isException(RLV_BHVR_SENDCHANNEL, nChannel)) ) &&
		( (!gRlvHandler.hasBehaviour(RLV_BHVR_SENDCHANNELEXCEPT)) || (!gRlvHandler.isException(RLV_BHVR_SENDCHANNELEXCEPT, nChannel)) );
}

bool RlvActions::canSendIM(const LLUUID& idRecipient)
{
	// User can send an IM to "recipient" (could be an agent or a group) if:
	//   - not generally restricted from sending IMs (or the recipient is an exception or inside the exclusion range)
	//   - not specifically restricted from sending an IM to the recipient
	return
		(!isRlvEnabled()) ||
		( ( (!gRlvHandler.hasBehaviour(RLV_BHVR_SENDIM)) || (gRlvHandler.isException(RLV_BHVR_SENDIM, idRecipient)) || (rlvCheckAvatarIMDistance(idRecipient, RLV_MODIFIER_SENDIMDISTMIN, RLV_MODIFIER_SENDIMDISTMAX)) ) &&
		  ( (!gRlvHandler.hasBehaviour(RLV_BHVR_SENDIMTO)) || (!gRlvHandler.isException(RLV_BHVR_SENDIMTO, idRecipient)) ) );
}

bool RlvActions::canStartIM(const LLUUID& idRecipient)
{
	// User can start an IM session with "recipient" (could be an agent or a group) if:
	//   - not generally restricted from starting IM sessions (or the recipient is an exception or inside the exclusion range)
	//   - not specifically restricted from starting an IM session with the recipient
	//   - the session already exists
	return
		(!isRlvEnabled()) ||
		( ( (!gRlvHandler.hasBehaviour(RLV_BHVR_STARTIM)) || (gRlvHandler.isException(RLV_BHVR_STARTIM, idRecipient)) || (rlvCheckAvatarIMDistance(idRecipient, RLV_MODIFIER_STARTIMDISTMIN, RLV_MODIFIER_STARTIMDISTMAX)) ) &&
		  ( (!gRlvHandler.hasBehaviour(RLV_BHVR_STARTIMTO)) || (!gRlvHandler.isException(RLV_BHVR_STARTIMTO, idRecipient)) ) ) ||
		( (hasOpenP2PSession(idRecipient)) || (hasOpenGroupSession(idRecipient)) );
}

bool RlvActions::canShowName(EShowNamesContext eContext, const LLUUID& idAgent)
{
	// Handle most common case upfront
	if (!s_BlockNamesContexts[eContext])
		return true;

	if (idAgent.notNull())
	{
		switch (eContext)
		{
			// Show/hide avatar nametag
			case SNC_NAMETAG:
				return (gRlvHandler.isException(RLV_BHVR_SHOWNAMETAGS, idAgent)) || (gAgentID == idAgent);
			// Show/hide avatar name
			case SNC_DEFAULT:
			case SNC_TELEPORTOFFER:
			case SNC_TELEPORTREQUEST:
				return gRlvHandler.isException(RLV_BHVR_SHOWNAMES, idAgent);
		}
	}
	return false;
}

bool RlvActions::canShowNearbyAgents()
{
	return !gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNEARBY);
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

bool RlvActions::canTeleportToLocal(const LLVector3d& posGlobal)
{
	// User can initiate a local teleport if:
	//   - not restricted from "sit teleporting" (or the destination is within the allowed xy-radius)
	//   - not restricted from teleporting locally (or the destination is within the allowed xy-radius)
	//   - can stand up (or isn't sitting)
	// NOTE: if we're teleporting due to an active command we should disregard any restrictions from the same object
	const LLUUID& idRlvObjExcept = gRlvHandler.getCurrentObject();
	bool fCanStand = RlvActions::canStand(idRlvObjExcept);
	if ( (fCanStand) && ((gRlvHandler.hasBehaviourExcept(RLV_BHVR_SITTP, gRlvHandler.getCurrentObject())) || (gRlvHandler.hasBehaviourExcept(RLV_BHVR_TPLOCAL, gRlvHandler.getCurrentObject()))) )
	{
		// User can stand up but is either @sittp or @tplocal restricted so we need to distance check
		const F32 nDistSq = (LLVector2(posGlobal.mdV[0], posGlobal.mdV[1]) - LLVector2(gAgent.getPositionGlobal().mdV[0], gAgent.getPositionGlobal().mdV[1])).lengthSquared();
		F32 nMaxDist = llmin(RlvBehaviourDictionary::instance().getModifier(RLV_MODIFIER_TPLOCALDIST)->getValue<float>(), RLV_MODIFIER_TPLOCAL_DEFAULT);
		if (gRlvHandler.hasBehaviour(RLV_BHVR_SITTP))
			nMaxDist = llmin(nMaxDist, RlvBehaviourDictionary::instance().getModifier(RLV_MODIFIER_SITTPDIST)->getValue<F32>());
		return (nDistSq < nMaxDist * nMaxDist);
	}
	return fCanStand;
}

bool RlvActions::canTeleportToLocation()
{
	// NOTE: if we're teleporting due to an active command we should disregard any restrictions from the same object
	const LLUUID& idRlvObjExcept = gRlvHandler.getCurrentObject();
	return (!gRlvHandler.hasBehaviourExcept(RLV_BHVR_TPLOC, idRlvObjExcept)) && (RlvActions::canStand(idRlvObjExcept));
}

bool RlvActions::isLocalTp(const LLVector3d& posGlobal)
{
	const F32 nDistSq = (LLVector2(posGlobal.mdV[0], posGlobal.mdV[1]) - LLVector2(gAgent.getPositionGlobal().mdV[0], gAgent.getPositionGlobal().mdV[1])).lengthSquared();
	return nDistSq < RLV_MODIFIER_TPLOCAL_DEFAULT * RLV_MODIFIER_TPLOCAL_DEFAULT;
}

// ============================================================================
// World interaction
//

bool RlvActions::canEdit(const LLViewerObject* pObj)
{
	// User can edit the specified object if:
	//   - not generally restricted from editing (or the object's root is an exception)
	//   - not specifically restricted from editing this object's root
	return
		(pObj) &&
		((!hasBehaviour(RLV_BHVR_EDIT)) || (gRlvHandler.isException(RLV_BHVR_EDIT, pObj->getRootEdit()->getID()))) &&
		((!hasBehaviour(RLV_BHVR_EDITOBJ)) || (!gRlvHandler.isException(RLV_BHVR_EDITOBJ, pObj->getRootEdit()->getID())));
}


bool RlvActions::canSit(const LLViewerObject* pObj, const LLVector3& posOffset /*= LLVector3::zero*/)
{
	// User can sit on the specified object if:
	//   - not prevented from sitting
	//   - not prevented from standing up or not currently sitting
	//   - not standtp restricted or not currently sitting (if the user is sitting and tried to sit elsewhere the tp would just kick in)
	//   - not a regular sit (i.e. due to @sit:<uuid>=force)
	//   - not @sittp=n or @fartouch=n restricted or if they clicked on a point within the allowed radius
	static RlvCachedBehaviourModifier<float> s_nFarTouchDist(RLV_MODIFIER_FARTOUCHDIST);
	static RlvCachedBehaviourModifier<float> s_nSitTpDist(RLV_MODIFIER_SITTPDIST);
	return
		( (pObj) && (LL_PCODE_VOLUME == pObj->getPCode()) ) &&
		(!hasBehaviour(RLV_BHVR_SIT)) &&
		( ((!hasBehaviour(RLV_BHVR_UNSIT)) && (!hasBehaviour(RLV_BHVR_STANDTP))) ||
		  ((isAgentAvatarValid()) && (!gAgentAvatarp->isSitting())) ) &&
		( ( (NULL != gRlvHandler.getCurrentCommand()) && (RLV_BHVR_SIT == gRlvHandler.getCurrentCommand()->getBehaviourType()) ) ||
		  ( ((!hasBehaviour(RLV_BHVR_SITTP)) || (dist_vec_squared(gAgent.getPositionGlobal(), pObj->getPositionGlobal() + LLVector3d(posOffset)) < s_nSitTpDist * s_nSitTpDist)) &&
		    ((!hasBehaviour(RLV_BHVR_FARTOUCH)) || (dist_vec_squared(gAgent.getPositionGlobal(), pObj->getPositionGlobal() + LLVector3d(posOffset)) < s_nFarTouchDist * s_nFarTouchDist)) ) );
}

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

template<>
const float& RlvActions::getModifierValue<float>(ERlvBehaviourModifier eBhvrMod)
{
	return RlvBehaviourDictionary::instance().getModifier(eBhvrMod)->getValue<float>();
}

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
