/**
 *
 * Copyright (c) 2009-2020, Kitty Barnett
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
#include "llvisualeffect.h"
#include "llvoavatarself.h"
#include "llworld.h"

#include "rlvactions.h"
#include "rlvhelper.h"
#include "rlvhandler.h"
#include "rlvinventory.h"
#include "rlvmodifiers.h"

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
        (!gRlvHandler.hasBehaviour(RLV_BHVR_SETCAM_EYEOFFSET)) && (!gRlvHandler.hasBehaviour(RLV_BHVR_SETCAM_EYEOFFSETSCALE)) && (!gRlvHandler.hasBehaviour(RLV_BHVR_SETCAM_FOCUSOFFSET));
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
    return
        (gRlvHandler.hasBehaviour(RLV_BHVR_SETCAM)) ||
        (gRlvHandler.hasBehaviour(RLV_BHVR_SETCAM_EYEOFFSET)) || (gRlvHandler.hasBehaviour(RLV_BHVR_SETCAM_EYEOFFSETSCALE)) || (gRlvHandler.hasBehaviour(RLV_BHVR_SETCAM_FOCUSOFFSET));
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

bool RlvActions::canGiveInventory()
{
    // User can give at least one (unspecified) avatar inventory if:
    //   - not specifically restricted from giving inventory (or at least one exception exists)
    return (!gRlvHandler.hasBehaviour(RLV_BHVR_SHARE)) || (gRlvHandler.hasException(RLV_BHVR_SHARE));
}

bool RlvActions::canGiveInventory(const LLUUID& idAgent)
{
    // User can give another avatar inventory if:
    //   - not specifically restricted from giving inventory (or the target is an exception)
    return (!gRlvHandler.hasBehaviour(RLV_BHVR_SHARE)) || (gRlvHandler.isException(RLV_BHVR_SHARE, idAgent));
}

// Little helper function to check the IM exclusion range for @recvim, @sendim and @startim (returns: min_dist <= (pos user - pos target) <= max_dist)
static bool rlvCheckAvatarIMDistance(const LLUUID& idAvatar, ERlvBehaviourModifier eModDistMin, ERlvBehaviourModifier eModDistMax)
{
    //                      | no limits | minimum set | min+max set |       !fHasMin | !fHasMax = INF | fHasMax
    // --------------------------------------------------------------       ------------------------------------
    // dist <= min  <= max  | block     | block       | block       |           F    |  F             |    F
    // min  <= dist <= max  | block     | allow       | allow       |           F    |  T             |    T
    // min  <= max  <= dist | block     | allow       | block       |           F    |  ^ (see above) |    F
    // off-region           | block     | allow       | block       |           F    |  ^ (see above) |    F

    const RlvBehaviourModifier *pBhvrModDistMin = RlvBehaviourDictionary::instance().getModifier(eModDistMin), *pBhvrModDistMax = RlvBehaviourDictionary::instance().getModifier(eModDistMax);
    if (pBhvrModDistMin->hasValue())
    {
        LLVector3d posAgent; bool fHasMax = pBhvrModDistMax->hasValue();
        float nMinDist = pBhvrModDistMin->getValue<float>(), nMaxDist = (fHasMax) ? pBhvrModDistMax->getValue<float>() : std::numeric_limits<float>::max();
        float nDist = (LLWorld::getInstance()->getAvatar(idAvatar, posAgent)) ? llabs(dist_vec_squared(gAgent.getPositionGlobal(), posAgent)) : std::numeric_limits<float>::max();
        return (nMinDist < nMaxDist) && (nMinDist <= nDist) && (nDist <= nMaxDist);
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

// Handles: @redirchat
bool RlvActions::canSendTypingStart()
{
    // The CHAT_TYPE_START indicator can be sent if:
    //   - nearby chat isn't being redirected
    //   - the user specifically indicated that they want to show typing under @redirchat
    return !RlvHandler::instance().hasBehaviour(RLV_BHVR_REDIRCHAT) || gSavedSettings.get<bool>(RlvSettingNames::ShowRedirectChatTyping);
}

bool RlvActions::canStartIM(const LLUUID& idRecipient, bool fIgnoreOpen)
{
    // User can start an IM session with "recipient" (could be an agent or a group) if:
    //   - not generally restricted from starting IM sessions (or the recipient is an exception or inside the exclusion range)
    //   - not specifically restricted from starting an IM session with the recipient
    //   - the session already exists
    return
        (!isRlvEnabled()) ||
        ( ( (!gRlvHandler.hasBehaviour(RLV_BHVR_STARTIM)) || (gRlvHandler.isException(RLV_BHVR_STARTIM, idRecipient)) || (rlvCheckAvatarIMDistance(idRecipient, RLV_MODIFIER_STARTIMDISTMIN, RLV_MODIFIER_STARTIMDISTMAX)) ) &&
          ( (!gRlvHandler.hasBehaviour(RLV_BHVR_STARTIMTO)) || (!gRlvHandler.isException(RLV_BHVR_STARTIMTO, idRecipient)) ) ) ||
        ( (!fIgnoreOpen) && ((hasOpenP2PSession(idRecipient)) || (hasOpenGroupSession(idRecipient))) );
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
            // Show/hide avatar name
            case SNC_DEFAULT:
            case SNC_TELEPORTOFFER:
            case SNC_TELEPORTREQUEST:
                return gRlvHandler.isException(RLV_BHVR_SHOWNAMES, idAgent) || (gAgentID == idAgent);
            default:
                return false;
        }
    }
    return false;
}

bool RlvActions::canShowNameTag(const LLVOAvatar* pAvatar)
{
    // An avatar's name tag can be shown if:
    //   - not restricted from seeing avatars' name tags
    //   - OR the avatar is a @shownametags exception
    //   - OR the avatar is within the distance that nametags can be shown
    if ( (!gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMETAGS)) || (gRlvHandler.isException(RLV_BHVR_SHOWNAMETAGS, pAvatar->getID())) || (gAgentID == pAvatar->getID()) )
        return true;

    const F32 nShowNameTagsDist = RlvBehaviourDictionary::instance().getModifier(RLV_MODIFIER_SHOWNAMETAGSDIST)->getValue<F32>();
    return (nShowNameTagsDist != 0.f) && (dist_vec_squared(pAvatar->getPositionGlobal(), gAgent.getPositionGlobal()) < nShowNameTagsDist * nShowNameTagsDist);
}

bool RlvActions::canShowNearbyAgents()
{
    return !gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNEARBY);
}

// Handles: @chatwhisper, @chatnormal and @chatshout
EChatType RlvActions::checkChatVolume(EChatType chatType)
{
    // In vs Bhvr | whisper |  normal |  shout  | n+w     |   n+s   |  s+w   |  s+n+w  |
    // ---------------------------------------------------------------------------------
    // whisper    | normal  | -       | -       | normal  | -       | normal | normal  |
    // normal     | -       | whisper | -       | whisper | whisper | -      | whisper |
    // shout      | -       | whisper | normal  | whisper | whisper | normal | whisper |

    RlvHandler& rlvHandler = RlvHandler::instance();
    if ( ((CHAT_TYPE_SHOUT == chatType) || (CHAT_TYPE_NORMAL == chatType)) && (rlvHandler.hasBehaviour(RLV_BHVR_CHATNORMAL)) )
        chatType = CHAT_TYPE_WHISPER;
    else if ( (CHAT_TYPE_SHOUT == chatType) && (rlvHandler.hasBehaviour(RLV_BHVR_CHATSHOUT)) )
        chatType = CHAT_TYPE_NORMAL;
    else if ( (CHAT_TYPE_WHISPER == chatType) && (rlvHandler.hasBehaviour(RLV_BHVR_CHATWHISPER)) )
        chatType = CHAT_TYPE_NORMAL;
    return chatType;
}

// ============================================================================
// Inventory
//

bool RlvActions::canPasteInventory(const LLInventoryCategory* pSourceCat, const LLInventoryCategory* pDestCat)
{
    // The user can paste the specified object into the destination if:
    //   - the source and destination are subject to the same lock type (or none at all) => NOTE: this happens to be the same logic we use for moving
    return (!isRlvEnabled()) ||
        ( (pSourceCat) && (pDestCat) && ((!RlvFolderLocks::instance().hasLockedFolder(RLV_LOCK_ANY)) || (RlvFolderLocks::instance().canMoveFolder(pSourceCat->getUUID(), pDestCat->getUUID()))) );
}

bool RlvActions::canPasteInventory(const LLInventoryItem* pSourceItem, const LLInventoryCategory* pDestCat)
{
    // The user can paste the specified object into the destination if:
    //   - the source and destination are subject to the same lock type (or none at all) => NOTE: this happens to be the same logic we use for moving
    return (!isRlvEnabled()) ||
        ( (pSourceItem) && (pDestCat) && ((!RlvFolderLocks::instance().hasLockedFolder(RLV_LOCK_ANY)) || (RlvFolderLocks::instance().canMoveItem(pSourceItem->getUUID(), pDestCat->getUUID()))) );
}

bool RlvActions::canPreviewTextures()
{
    return (!gRlvHandler.hasBehaviour(RLV_BHVR_VIEWTEXTURE));
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

bool RlvActions::canFly()
{
    return (!gRlvHandler.getCurrentCommand()) ? !gRlvHandler.hasBehaviour(RLV_BHVR_FLY) : !gRlvHandler.hasBehaviourExcept(RLV_BHVR_FLY, gRlvHandler.getCurrentObject());
}

bool RlvActions::canFly(const LLUUID& idRlvObjExcept)
{
    return !gRlvHandler.hasBehaviourExcept(RLV_BHVR_FLY, idRlvObjExcept);
}

bool RlvActions::canJump()
{
    return !gRlvHandler.hasBehaviour(RLV_BHVR_JUMP);
}

// ============================================================================
// Teleporting
//

bool RlvActions::canTeleportToLocal(const LLVector3d& posGlobal)
{
    // User can initiate a local teleport if:
    //   - can stand up (or isn't sitting)
    //   - not restricted from "sit teleporting" (or the destination is within the allowed xyz-radius)
    //   - not restricted from teleporting locally (or the destination is within the allowed xy-radius)
    // NOTE: if we're teleporting due to an active command we should disregard any restrictions from the same object
    const LLUUID& idRlvObjExcept = gRlvHandler.getCurrentObject();
    bool fCanTeleport = RlvActions::canStand(idRlvObjExcept);
    if ( (fCanTeleport) && (gRlvHandler.hasBehaviourExcept(RLV_BHVR_SITTP, idRlvObjExcept)) )
    {
        const F32 nDistSq = (posGlobal - gAgent.getPositionGlobal()).lengthSquared();
        const F32 nSitTpDist = RlvBehaviourDictionary::instance().getModifier(RLV_MODIFIER_SITTPDIST)->getValue<F32>();
        fCanTeleport = nDistSq < nSitTpDist * nSitTpDist;
    }
    if ( (fCanTeleport) && (gRlvHandler.hasBehaviourExcept(RLV_BHVR_TPLOCAL, idRlvObjExcept)) )
    {
        const F32 nDistSq = (LLVector2(posGlobal.mdV[0], posGlobal.mdV[1]) - LLVector2(gAgent.getPositionGlobal().mdV[0], gAgent.getPositionGlobal().mdV[1])).lengthSquared();
        const F32 nTpLocalDist = llmin(RlvBehaviourDictionary::instance().getModifier(RLV_MODIFIER_TPLOCALDIST)->getValue<float>(), RLV_MODIFIER_TPLOCAL_DEFAULT);
        fCanTeleport = nDistSq < nTpLocalDist * nTpLocalDist;
    }
    return fCanTeleport;
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
// WindLight
//

bool RlvActions::canChangeEnvironment(const LLUUID& idRlvObject)
{
    // User can (partially) change their environment settings if:
    //   - not specifically restricted from changing their environment (by any object other than the one specified)
    return (idRlvObject.isNull()) ? !gRlvHandler.hasBehaviour(RLV_BHVR_SETENV) : !gRlvHandler.hasBehaviourExcept(RLV_BHVR_SETENV, idRlvObject);
}

bool RlvActions::hasPostProcess()
{
    return LLVfxManager::instance().hasEffect(EVisualEffect::RlvSphere);
}

// ============================================================================
// World interaction
//

bool RlvActions::canBuild()
{
    // User can access the build floater if:
    //    - allowed to edit existing objects OR
    //    - allowed to rez/create objects
    return
        (!gRlvHandler.hasBehaviour(RLV_BHVR_EDIT)) ||
        (!gRlvHandler.hasBehaviour(RLV_BHVR_REZ));
}

// Handles: @buy
bool RlvActions::canBuyObject(const LLUUID& idObj)
{
    // User can buy an object set for sale if:
    //    - not restricted from buying objects
    return (!RlvHandler::instance().hasBehaviour(RLV_BHVR_BUY));
}

// Handles: @edit, @editobj, @editattach and @editworld
bool RlvActions::canEdit(ERlvCheckType eCheckType)
{
    RlvHandler& rlvHandler = RlvHandler::instance();
    switch (eCheckType)
    {
        case ERlvCheckType::All:
            // No edit restrictions of any kind
            return
                !rlvHandler.hasBehaviour(RLV_BHVR_EDIT) && !rlvHandler.hasBehaviour(RLV_BHVR_EDITOBJ) &&
                !rlvHandler.hasBehaviour(RLV_BHVR_EDITATTACH) && !rlvHandler.hasBehaviour(RLV_BHVR_EDITWORLD);

        case ERlvCheckType::Some:
            // Not @edit restricted (or at least one exception) and either not @editattach or not @editworld restricted
            return
                (!rlvHandler.hasBehaviour(RLV_BHVR_EDIT) || rlvHandler.hasException(RLV_BHVR_EDIT)) &&
                (!rlvHandler.hasBehaviour(RLV_BHVR_EDITATTACH) || rlvHandler.hasBehaviour(RLV_BHVR_EDITWORLD));

        case ERlvCheckType::Nothing:
            // Either @edit restricted with no exceptions or @editattach and @editworld restricted at the same time
            return
                (rlvHandler.hasBehaviour(RLV_BHVR_EDIT) && !rlvHandler.hasException(RLV_BHVR_EDIT)) ||
                (rlvHandler.hasBehaviour(RLV_BHVR_EDITATTACH) && rlvHandler.hasBehaviour(RLV_BHVR_EDITWORLD));

        default:
            RLV_ASSERT(false);
            return false;
    }
}

// Handles: @edit, @editobj, @editattach and @editworld
bool RlvActions::canEdit(const LLViewerObject* pObj)
{
    // User can edit the specified object if:
    //   - not generally restricted from editing (or the object's root is an exception)
    //   - not specifically restricted from editing this object's root
    //   - it's an attachment   and not restricted from editing attachments
    //     it's a rezzed object and not restricted from editing any world objects

    // NOTE-RLVa: edit checks should *never* be subject to @fartouch distance checks since we don't have the pick offset so
    //            instead just implicitly rely on the presence of a (transient) selection
    return
        (pObj) &&
        ( (!RlvHandler::instance().hasBehaviour(RLV_BHVR_EDIT)) || (RlvHandler::instance().isException(RLV_BHVR_EDIT, pObj->getRootEdit()->getID())) ) &&
        ( (!RlvHandler::instance().hasBehaviour(RLV_BHVR_EDITOBJ)) || (!RlvHandler::instance().isException(RLV_BHVR_EDITOBJ, pObj->getRootEdit()->getID())) ) &&
        ( (pObj->isAttachment()) ? !RlvHandler::instance().hasBehaviour(RLV_BHVR_EDITATTACH)
                                 : !RlvHandler::instance().hasBehaviour(RLV_BHVR_EDITWORLD) );
}

// Handles: @fartouch and @interact
bool RlvActions::canInteract(const LLViewerObject* pObj, const LLVector3& posOffset /*=LLVector3::zero*/)
{
    static RlvCachedBehaviourModifier<float> s_nFartouchDist(RLV_MODIFIER_FARTOUCHDIST);

    // User can interact with the specified object if:
    //   - not interaction restricted (or the specified object is a HUD attachment)
    //   - not prevented from touching faraway objects (or the object's center + pick offset is within range)
    RlvHandler& rlvHandler = RlvHandler::instance();
    return
        (!pObj) ||
        ( ( (!rlvHandler.hasBehaviour(RLV_BHVR_INTERACT)) || (pObj->isHUDAttachment())) &&
          ( (!rlvHandler.hasBehaviour(RLV_BHVR_FARTOUCH)) || (pObj->isHUDAttachment()) || (dist_vec_squared(gAgent.getPositionGlobal(), pObj->getPositionGlobal() + LLVector3d(posOffset)) <= s_nFartouchDist * s_nFartouchDist)) );
}

// Handles: @pay
bool RlvActions::canPayAvatar(const LLUUID& idAvatar)
{
    // User can pay an avatar if:
    //    - not restricted from paying avatars
    return (!RlvHandler::instance().hasBehaviour(RLV_BHVR_PAY));
}

// Handles: @buy
bool RlvActions::canPayObject(const LLUUID& idObj)
{
    // User can pay an object/vendor if:
    //    - not restricted from buying objects
    return (!RlvHandler::instance().hasBehaviour(RLV_BHVR_BUY));
}

bool RlvActions::canRez()
{
    return (!gRlvHandler.hasBehaviour(RLV_BHVR_REZ));
}

bool RlvActions::canGroundSit()
{
    // User can sit on the ground if:
    //   - not prevented from sitting
    //   - not prevented from standing up or not currently sitting
    return (!hasBehaviour(RLV_BHVR_SIT)) && (canStand());
}

bool RlvActions::canGroundSit(const LLUUID& idRlvObjExcept)
{
    // See canGroundSit() but disregard any restrictions held by the issuing object
    return (!gRlvHandler.hasBehaviourExcept(RLV_BHVR_SIT, idRlvObjExcept)) && (canStand(idRlvObjExcept));
}

bool RlvActions::canSit(const LLViewerObject* pObj, const LLVector3& posOffset /*=LLVector3::zero*/)
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

// Handles: @showhovertextall, @showhovertextworld, @showhovertexthud and @showhovertext
bool RlvActions::canShowHoverText(const LLViewerObject *pObj)
{
    // User cannot see this object's hover text if:
    //   - prevented from seeing any hover text
    //   - prevented from seeing hover text on world objects (= non-HUD attachments)
    //   - prevented from seeing hover text on HUD objects
    //   - specifically prevented from seeing that object's hover text)
    //       -> NOTE-RLVa: this is object-specific (as opposed to touch restricts which are linkset-wide)
    RlvHandler& rlvHandler = RlvHandler::instance();
    return
        ( (!pObj) || (LL_PCODE_VOLUME != pObj->getPCode()) ||
          !( (rlvHandler.hasBehaviour(RLV_BHVR_SHOWHOVERTEXTALL)) ||
             ( (rlvHandler.hasBehaviour(RLV_BHVR_SHOWHOVERTEXTWORLD)) && (!pObj->isHUDAttachment()) ) ||
             ( (rlvHandler.hasBehaviour(RLV_BHVR_SHOWHOVERTEXTHUD)) && (pObj->isHUDAttachment()) ) ||
             (rlvHandler.isException(RLV_BHVR_SHOWHOVERTEXT, pObj->getID(), ERlvExceptionCheck::Permissive)) ) );
}

// Handles: @touchall, @touchthis, @touchworld, @touchattach, @touchattachself, @touchattachother, @touchhud, @touchme and @fartouch
bool RlvActions::canTouch(const LLViewerObject* pObj, const LLVector3& posOffset /*=LLVector3::zero*/)
{
    static RlvCachedBehaviourModifier<float> s_nFartouchDist(RLV_MODIFIER_FARTOUCHDIST);

    // User can touch a
    //  (1) World object if
    //        - a) not prevented from touching any object
    //        - b) not specifically prevented from touching that object
    //        - c) not prevented from touching world objects (or the object is an exception)
    //        - h) not prevented from touching faraway objects (or the object's center + pick offset is within range)
    //        - i) specifically allowed to touch that object (overrides all restrictions)
    //  (2) Attachment (on another avatar)
    //        - a) not prevented from touching any object
    //        - b) not specifically prevented from touching that object
    //        - d) not prevented from touching attachments (or the attachment and/or its wearer is/are an exception)
    //        - e) not prevented from touching other avatar's attachments (or the attachment is worn by a specific avatar on the exception list)
    //        - h) not prevented from touching faraway objects (or the attachment's center + pick offset is within range)
    //        - i) specifically allowed to touch that object (overrides all restrictions)
    //  (3) Attachment (on own avatar)
    //        - a) not prevented from touching any object
    //        - b) not specifically prevented from touching that object
    //        - d) not prevented from touching attachments (or the attachment is an exception)
    //        - f) not prevented from touching their own avatar's attachments (or the attachment is an exception)
    //        - h) not prevented from touching faraway objects (or the attachment's center + pick offset is within range)
    //        - i) specifically allowed to touch that object (overrides all restrictions)
    //  (4) Attachment (on HUD)
    //        - b) not specifically prevented from touching that object
    //        - g) not prevented from touching their own HUD attachments (or the attachment is an exception)
    //        - i) specifically allowed to touch that object (overrides all restrictions)

    // NOTE-RLVa: * touch restrictions apply linkset-wide (as opposed to, for instance, hover text which is object-specific) but only the root object's restrictions are tested
    //            * @touchall affects world objects and world attachments (self and others') but >not< HUD attachments
    //            * @fartouch distance matches against the specified object + pick offset (so >not< the linkset root)
    //            * @touchattachother exceptions change when they specify an avatar id (=block all) or an object id (=allow indiviual - see general @touchattach exceptions)
    //            * @touchattachself exceptions are only checked under the general @touchattach exceptions
    //            * @touchme in any object of a linkset affects that entire linkset (= if you can specifically touch one prim in a linkset you can touch that entire linkset)
    const LLUUID& idRoot = (pObj) ? pObj->getRootEdit()->getID() : LLUUID::null;

    RlvHandler& rlvHandler = RlvHandler::instance();
    // Short circuit test for (1/2/3/4.a) and (1/2/3/4.b)
    bool fCanTouch =
        (idRoot.notNull()) &&
        ( (pObj->isHUDAttachment()) || (!rlvHandler.hasBehaviour(RLV_BHVR_TOUCHALL)) ) &&
        ( (!rlvHandler.hasBehaviour(RLV_BHVR_TOUCHTHIS)) || (!rlvHandler.isException(RLV_BHVR_TOUCHTHIS, idRoot, ERlvExceptionCheck::Permissive)) );
    if (fCanTouch)
    {
        if (!pObj->isAttachment())
        {
            // Rezzed object - test for (1.c) and (1.h)
            fCanTouch =
                ( (!rlvHandler.hasBehaviour(RLV_BHVR_TOUCHWORLD)) || (rlvHandler.isException(RLV_BHVR_TOUCHWORLD, idRoot, ERlvExceptionCheck::Permissive)) ) &&
                ( (!rlvHandler.hasBehaviour(RLV_BHVR_FARTOUCH)) || (dist_vec_squared(gAgent.getPositionGlobal(), pObj->getPositionGlobal() + LLVector3d(posOffset)) <= s_nFartouchDist * s_nFartouchDist) );
        }
        else if (!pObj->permYouOwner())
        {
            // Attachment worn by other - test for (2.d), (2.e) and (2.h)
            const LLUUID& idAttachAgent = static_cast<LLViewerObject*>(pObj->getRoot())->getID();
            fCanTouch =
                (
                    ( (!rlvHandler.hasBehaviour(RLV_BHVR_TOUCHATTACH) && !rlvHandler.hasBehaviour(RLV_BHVR_TOUCHATTACHOTHER)) ||
                      (rlvHandler.isException(RLV_BHVR_TOUCHATTACH, idRoot, ERlvExceptionCheck::Permissive) || rlvHandler.isException(RLV_BHVR_TOUCHATTACH, idAttachAgent, ERlvExceptionCheck::Permissive)) ) &&
                    (!rlvHandler.isException(RLV_BHVR_TOUCHATTACHOTHER, idAttachAgent))
                ) &&
                ( (!rlvHandler.hasBehaviour(RLV_BHVR_FARTOUCH)) || (dist_vec_squared(gAgent.getPositionGlobal(), pObj->getPositionGlobal() + LLVector3d(posOffset)) <= s_nFartouchDist * s_nFartouchDist) );
        }
        else if (!pObj->isHUDAttachment())
        {
            // Regular attachment worn by this avie - test for (3.d), (3.e) and (3.h)
            fCanTouch =
                ((!rlvHandler.hasBehaviour(RLV_BHVR_TOUCHATTACH)) || (rlvHandler.isException(RLV_BHVR_TOUCHATTACH, idRoot, ERlvExceptionCheck::Permissive))) &&
                ((!rlvHandler.hasBehaviour(RLV_BHVR_TOUCHATTACHSELF)) || (rlvHandler.isException(RLV_BHVR_TOUCHATTACH, idRoot, ERlvExceptionCheck::Permissive)));
        }
        else
        {
            // HUD attachment - test for (4.g)
            fCanTouch = (!hasBehaviour(RLV_BHVR_TOUCHHUD)) || (rlvHandler.isException(RLV_BHVR_TOUCHHUD, idRoot, ERlvExceptionCheck::Permissive));
        }
    }
    // Post-check for (1/2/3/4i)
    if ( (!fCanTouch) && (hasBehaviour(RLV_BHVR_TOUCHME)) )
        fCanTouch = rlvHandler.hasBehaviourRoot(idRoot, RLV_BHVR_TOUCHME);
    return fCanTouch;
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
// World (General)
//

bool RlvActions::canHighlightTransparent()
{
    // User cannot highlight transparent faces if:
    //   - prevented from editing (exceptions are not taken into account)
    //   - specifically prevented from highlight transparent faces
    return !gRlvHandler.hasBehaviour(RLV_BHVR_EDIT) && !gRlvHandler.hasBehaviour(RLV_BHVR_SHOWLOC);
}

bool RlvActions::canViewWireframe()
{
    // User can use wireframe rendering if:
    //   - no HUD attachment is (remove) locked
    //   - not specifically prevented from using wireframe mode
    return
        !gRlvAttachmentLocks.hasLockedHUD() &&                  // Trivial function so no overhead when RLV is not enabled
        !gRlvHandler.hasBehaviour(RLV_BHVR_VIEWWIREFRAME);
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

#ifdef CATZNIP_STRINGVIEW
void RlvActions::notifyBlocked(const boost::string_view& strNotifcation, const LLSD& sdArgs)
#else
void RlvActions::notifyBlocked(const std::string& strNotifcation, const LLSD& sdArgs)
#endif // CATZNIP_STRINGVIEW
{
    RlvUtil::notifyBlocked(strNotifcation, sdArgs);
}

// ============================================================================
