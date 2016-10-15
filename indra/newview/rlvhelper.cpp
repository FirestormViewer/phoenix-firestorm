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
#include "llappearancemgr.h"
#include "llattachmentsmgr.h"
#include "llgesturemgr.h"
#include "llnotificationsutil.h"
#include "llviewerobjectlist.h"

#include "rlvcommon.h"
#include "rlvhelper.h"
#include "rlvhandler.h"
#include "rlvinventory.h"

#include <boost/algorithm/string.hpp>

// ============================================================================
// RlvBehaviourDictionary
//

/*
 * Processing of RLVa commands used to be a big switch/case loop with one function for each command type(addrem, reply
 * and force). This is slowly being replaced with templated command handling which might be more confusing intially
 * (also due to my poor naming schemes) but is actually far simpler and less error-prone than the old way.
 *
 * In the general case you just add a definition for the command below and then write the function body in rlvhandler.cpp
 * and you're done! Told you this was easy.
 *
 * Reply command
 * =============
 * Definition: RlvReplyProcessor<RLV_BHVR_COMMANDNAME>("commandname"[, <options>]));
 * Implement : ERlvCmdRet RlvReplyHandler<RLV_BHVR_COMMANDNAME>::onCommand(const RlvCommand& rlvCmd, std::string& strReply)
 *
 * Force command
 * =============
 * Definition: new RlvForceProcessor<RLV_BHVR_COMMANDNAME>("commandname"[, <options>]));
 * Implement : ERlvCmdRet RlvForceProcessor<RLV_BHVR_COMMANDNAME>::onCommand(const RlvCommand& rlvCmd)
 *
 * Behaviours
 * ==========
 * Behaviours come in many forms but the added complexity is only in the variety of choices. The implementation is as
 * easy as reply or force commands.
 *
 * For simple behaviours that only require recordkeeping and don't run code when set/unset (see ERlvBehaviourOptionType):
 *   Definition: RlvBehaviourGenericProcessor<RLV_OPTION_TYPE>("commandname", RLV_BHVR_COMMANDNAME)
 *   Implement : nothing! (it automagically works)
 * For simple behaviours that only require recordkeeping and only run code when they toggle:
 *   Definition: RlvBehaviourGenericToggleProcessor<RLV_BHVR_COMMANDNAME, RLV_OPTION_TYPE>("commandname"))
 *   Implement : void RlvBehaviourToggleHandler<RLV_BHVR_COMMANDNAME>::onCommandToggle(ERlvBehaviour eBhvr, bool fHasBhvr)
 * For behaviours that require manual processing:
 *   Definition: RlvBehaviourProcessor<RLV_BHVR_COMMANDNAME>("commandname"))
 *   Implement : ERlvCmdRet RlvBehaviourHandler<RLV_BHVR_COMMANDNAME>::onCommand(const RlvCommand& rlvCmd, bool& fRefCount)
 * For behaviours that run code when their modifier changes:
 *   Definition: addModifier(RLV_BHVR_COMMANDNAME, RLV_MODIFIER_COMMANDNAME, new RlvBehaviourModifierHandler<RLV_MODIFIER_COMMANDNAME>(<default>, <auto-add>, <comparator>));
 *   Implement : void RlvBehaviourModifierHandler::onValueChanged()
 *
 */
RlvBehaviourDictionary::RlvBehaviourDictionary()
{
	// Array auto-initialization to 0 is still not supported in VS2013
	memset(m_BehaviourModifiers, 0, sizeof(RlvBehaviourModifier*) * RLV_MODIFIER_COUNT);

	//
	// Restrictions
	//
	addEntry(new RlvBehaviourGenericProcessor<RLV_OPTION_NONE>("acceptpermission", RLV_BHVR_ACCEPTPERMISSION));
	addEntry(new RlvBehaviourGenericProcessor<RLV_OPTION_NONE_OR_EXCEPTION>("accepttp", RLV_BHVR_ACCEPTTP, RlvBehaviourInfo::BHVR_STRICT));
	addEntry(new RlvBehaviourGenericProcessor<RLV_OPTION_NONE_OR_EXCEPTION>("accepttprequest", RLV_BHVR_ACCEPTTPREQUEST, RlvBehaviourInfo::BHVR_STRICT | RlvBehaviourInfo::BHVR_EXTENDED));
	addEntry(new RlvBehaviourProcessor<RLV_BHVR_ADDATTACH, RlvBehaviourAddRemAttachHandler>("addattach"));
	addEntry(new RlvBehaviourInfo("addoutfit",				RLV_BHVR_ADDOUTFIT,				RLV_TYPE_ADDREM));
	addEntry(new RlvBehaviourGenericProcessor<RLV_OPTION_NONE>("allowidle", RLV_BHVR_ALLOWIDLE, RlvBehaviourInfo::BHVR_EXPERIMENTAL));
	addEntry(new RlvBehaviourGenericProcessor<RLV_OPTION_NONE>("alwaysrun", RLV_BHVR_ALWAYSRUN));
	addEntry(new RlvBehaviourInfo("attachthis",				RLV_BHVR_ATTACHTHIS,			RLV_TYPE_ADDREM, RlvBehaviourInfo::FORCEWEAR_NODE));
	addEntry(new RlvBehaviourInfo("attachallthis",			RLV_BHVR_ATTACHTHIS,			RLV_TYPE_ADDREM, RlvBehaviourInfo::FORCEWEAR_SUBTREE));
	addEntry(new RlvBehaviourInfo("attachthis_except",		RLV_BHVR_ATTACHTHISEXCEPT,		RLV_TYPE_ADDREM, RlvBehaviourInfo::FORCEWEAR_NODE));
	addEntry(new RlvBehaviourInfo("attachallthis_except",	RLV_BHVR_ATTACHTHISEXCEPT,		RLV_TYPE_ADDREM, RlvBehaviourInfo::FORCEWEAR_SUBTREE));
	addEntry(new RlvBehaviourGenericProcessor<RLV_OPTION_NONE>("chatwhisper", RLV_BHVR_CHATWHISPER));
	addEntry(new RlvBehaviourGenericProcessor<RLV_OPTION_NONE>("chatnormal", RLV_BHVR_CHATNORMAL));
	addEntry(new RlvBehaviourGenericProcessor<RLV_OPTION_NONE>("chatshout", RLV_BHVR_CHATSHOUT));
	addEntry(new RlvBehaviourProcessor<RLV_BHVR_DETACH>("detach"));
	addEntry(new RlvBehaviourInfo("detachthis",				RLV_BHVR_DETACHTHIS,			RLV_TYPE_ADDREM, RlvBehaviourInfo::FORCEWEAR_NODE));
	addEntry(new RlvBehaviourInfo("detachallthis",			RLV_BHVR_DETACHTHIS,			RLV_TYPE_ADDREM, RlvBehaviourInfo::FORCEWEAR_SUBTREE));
	addEntry(new RlvBehaviourInfo("detachthis_except",		RLV_BHVR_DETACHTHISEXCEPT,		RLV_TYPE_ADDREM, RlvBehaviourInfo::FORCEWEAR_NODE));
	addEntry(new RlvBehaviourInfo("detachallthis_except",	RLV_BHVR_DETACHTHISEXCEPT,		RLV_TYPE_ADDREM, RlvBehaviourInfo::FORCEWEAR_SUBTREE));
	addEntry(new RlvBehaviourGenericToggleProcessor<RLV_BHVR_EDIT, RLV_OPTION_NONE_OR_EXCEPTION>("edit"));
	addEntry(new RlvBehaviourGenericProcessor<RLV_OPTION_EXCEPTION>("editobj", RLV_BHVR_EDITOBJ));
	addEntry(new RlvBehaviourGenericProcessor<RLV_OPTION_NONE>("emote", RLV_BHVR_EMOTE));
	addEntry(new RlvBehaviourGenericProcessor<RLV_OPTION_NONE_OR_MODIFIER>("fartouch", RLV_BHVR_FARTOUCH));
	addModifier(RLV_BHVR_FARTOUCH, RLV_MODIFIER_FARTOUCHDIST, new RlvBehaviourModifier("Fartouch Distance", RLV_MODIFIER_FARTOUCH_DEFAULT, true, new RlvBehaviourModifier_CompMin));
	addEntry(new RlvBehaviourGenericProcessor<RLV_OPTION_NONE>("fly", RLV_BHVR_FLY));
	addEntry(new RlvBehaviourGenericProcessor<RLV_OPTION_NONE>("interact", RLV_BHVR_INTERACT, RlvBehaviourInfo::BHVR_EXTENDED));
	addEntry(new RlvBehaviourInfo("notify",					RLV_BHVR_NOTIFY,				RLV_TYPE_ADDREM));
	addEntry(new RlvBehaviourGenericProcessor<RLV_OPTION_NONE>("permissive", RLV_BHVR_PERMISSIVE));
	addEntry(new RlvBehaviourGenericProcessor<RLV_OPTION_NONE_OR_EXCEPTION>("recvchat", RLV_BHVR_RECVCHAT, RlvBehaviourInfo::BHVR_STRICT));
	addEntry(new RlvBehaviourGenericProcessor<RLV_OPTION_EXCEPTION>("recvchatfrom", RLV_BHVR_RECVCHATFROM, RlvBehaviourInfo::BHVR_STRICT));
	addEntry(new RlvBehaviourGenericProcessor<RLV_OPTION_NONE_OR_EXCEPTION>("recvemote", RLV_BHVR_RECVEMOTE, RlvBehaviourInfo::BHVR_STRICT));
	addEntry(new RlvBehaviourGenericProcessor<RLV_OPTION_EXCEPTION>("recvemotefrom", RLV_BHVR_RECVEMOTEFROM, RlvBehaviourInfo::BHVR_STRICT));
	addEntry(new RlvBehaviourProcessor<RLV_BHVR_RECVIM, RlvBehaviourRecvSendStartIMHandler>("recvim", RlvBehaviourInfo::BHVR_STRICT));
	addModifier(RLV_BHVR_RECVIM, RLV_MODIFIER_RECVIMDISTMIN, new RlvBehaviourModifier("RecvIM Distance (Min)", F32_MAX, true, new RlvBehaviourModifier_CompMax));
	addModifier(RLV_BHVR_RECVIM, RLV_MODIFIER_RECVIMDISTMAX, new RlvBehaviourModifier("RecvIM Distance (Max)", F32_MAX, true, new RlvBehaviourModifier_CompMin));
	addEntry(new RlvBehaviourGenericProcessor<RLV_OPTION_EXCEPTION>("recvimfrom", RLV_BHVR_RECVIMFROM, RlvBehaviourInfo::BHVR_STRICT));
	addEntry(new RlvBehaviourInfo("redirchat",				RLV_BHVR_REDIRCHAT,				RLV_TYPE_ADDREM));
	addEntry(new RlvBehaviourInfo("rediremote",				RLV_BHVR_REDIREMOTE,			RLV_TYPE_ADDREM));
	addEntry(new RlvBehaviourProcessor<RLV_BHVR_REMATTACH, RlvBehaviourAddRemAttachHandler>("remattach"));
	addEntry(new RlvBehaviourInfo("remoutfit",				RLV_BHVR_REMOUTFIT,				RLV_TYPE_ADDREM));
	addEntry(new RlvBehaviourGenericProcessor<RLV_OPTION_NONE>("rez", RLV_BHVR_REZ));
	addEntry(new RlvBehaviourProcessor<RLV_BHVR_SENDCHANNEL, RlvBehaviourSendChannelHandler>("sendchannel", RlvBehaviourInfo::BHVR_STRICT));
	addEntry(new RlvBehaviourProcessor<RLV_BHVR_SENDCHANNELEXCEPT, RlvBehaviourSendChannelHandler>("sendchannel_except", RlvBehaviourInfo::BHVR_STRICT | RlvBehaviourInfo::BHVR_EXPERIMENTAL));
	addEntry(new RlvBehaviourGenericProcessor<RLV_OPTION_NONE>("sendchat", RLV_BHVR_SENDCHAT));
	addEntry(new RlvBehaviourToggleProcessor<RLV_BHVR_SENDIM, RlvBehaviourRecvSendStartIMHandler>("sendim", RlvBehaviourInfo::BHVR_STRICT));
	addModifier(RLV_BHVR_SENDIM, RLV_MODIFIER_SENDIMDISTMIN, new RlvBehaviourModifier("SendIM Distance (Min)", F32_MAX, true, new RlvBehaviourModifier_CompMax));
	addModifier(RLV_BHVR_SENDIM, RLV_MODIFIER_SENDIMDISTMAX, new RlvBehaviourModifier("SendIM Distance (Max)", F32_MAX, true, new RlvBehaviourModifier_CompMin));
	addEntry(new RlvBehaviourGenericProcessor<RLV_OPTION_EXCEPTION>("sendimto", RLV_BHVR_SENDIMTO, RlvBehaviourInfo::BHVR_STRICT));
	addEntry(new RlvBehaviourGenericProcessor<RLV_OPTION_NONE>("sendgesture", RLV_BHVR_SENDGESTURE, RlvBehaviourInfo::BHVR_EXPERIMENTAL));
	addEntry(new RlvBehaviourGenericToggleProcessor<RLV_BHVR_SETDEBUG, RLV_OPTION_NONE>("setdebug"));
	addEntry(new RlvBehaviourGenericToggleProcessor<RLV_BHVR_SETENV, RLV_OPTION_NONE>("setenv"));
	addEntry(new RlvBehaviourGenericProcessor<RLV_OPTION_NONE>("setgroup", RLV_BHVR_SETGROUP));
	addEntry(new RlvBehaviourInfo("sharedunwear",			RLV_BHVR_SHAREDUNWEAR,			RLV_TYPE_ADDREM, RlvBehaviourInfo::BHVR_EXTENDED));
	addEntry(new RlvBehaviourInfo("sharedwear",				RLV_BHVR_SHAREDWEAR,			RLV_TYPE_ADDREM, RlvBehaviourInfo::BHVR_EXTENDED));
	addEntry(new RlvBehaviourProcessor<RLV_BHVR_SHOWHOVERTEXT>("showhovertext"));
	addEntry(new RlvBehaviourGenericProcessor<RLV_OPTION_NONE>("showhovertextall", RLV_BHVR_SHOWHOVERTEXTALL));
	addEntry(new RlvBehaviourGenericProcessor<RLV_OPTION_NONE>("showhovertexthud", RLV_BHVR_SHOWHOVERTEXTHUD));
	addEntry(new RlvBehaviourGenericProcessor<RLV_OPTION_NONE>("showhovertextworld", RLV_BHVR_SHOWHOVERTEXTWORLD));
	addEntry(new RlvBehaviourGenericToggleProcessor<RLV_BHVR_SHOWINV, RLV_OPTION_NONE>("showinv"));
	addEntry(new RlvBehaviourGenericProcessor<RLV_OPTION_NONE>("showloc", RLV_BHVR_SHOWLOC));
	addEntry(new RlvBehaviourGenericProcessor<RLV_OPTION_NONE>("showminimap", RLV_BHVR_SHOWMINIMAP));
	addEntry(new RlvBehaviourToggleProcessor<RLV_BHVR_SHOWNAMES>("shownames", RlvBehaviourInfo::BHVR_STRICT));
	addEntry(new RlvBehaviourToggleProcessor<RLV_BHVR_SHOWNAMETAGS>("shownametags", RlvBehaviourInfo::BHVR_STRICT ));
	addEntry(new RlvBehaviourGenericToggleProcessor<RLV_BHVR_SHOWNEARBY, RLV_OPTION_NONE>("shownearby", RlvBehaviourInfo::BHVR_EXPERIMENTAL));
	addEntry(new RlvBehaviourGenericToggleProcessor<RLV_BHVR_SHOWSELF, RLV_OPTION_NONE, RlvBehaviourShowSelfToggleHandler>("showself", RlvBehaviourInfo::BHVR_EXPERIMENTAL));
	addEntry(new RlvBehaviourGenericToggleProcessor<RLV_BHVR_SHOWSELFHEAD, RLV_OPTION_NONE, RlvBehaviourShowSelfToggleHandler>("showselfhead", RlvBehaviourInfo::BHVR_EXPERIMENTAL));
	addEntry(new RlvBehaviourGenericProcessor<RLV_OPTION_NONE>("showworldmap", RLV_BHVR_SHOWWORLDMAP));
	addEntry(new RlvBehaviourGenericProcessor<RLV_OPTION_NONE>("sit", RLV_BHVR_SIT));
	addEntry(new RlvBehaviourGenericProcessor<RLV_OPTION_NONE_OR_MODIFIER>("sittp", RLV_BHVR_SITTP));
	addModifier(RLV_BHVR_SITTP, RLV_MODIFIER_SITTPDIST, new RlvBehaviourModifier("SitTp Distance", RLV_MODIFIER_SITTP_DEFAULT, true, new RlvBehaviourModifier_CompMin));
	addEntry(new RlvBehaviourGenericProcessor<RLV_OPTION_NONE>("standtp", RLV_BHVR_STANDTP));
	addEntry(new RlvBehaviourProcessor<RLV_BHVR_STARTIM, RlvBehaviourRecvSendStartIMHandler>("startim", RlvBehaviourInfo::BHVR_STRICT));
	addModifier(RLV_BHVR_STARTIM, RLV_MODIFIER_STARTIMDISTMIN, new RlvBehaviourModifier("StartIM Distance (Min)", F32_MAX, true, new RlvBehaviourModifier_CompMax));
	addModifier(RLV_BHVR_STARTIM, RLV_MODIFIER_STARTIMDISTMAX, new RlvBehaviourModifier("StartIM Distance (Max)", F32_MAX, true, new RlvBehaviourModifier_CompMin));
	addEntry(new RlvBehaviourGenericProcessor<RLV_OPTION_EXCEPTION>("startimto", RLV_BHVR_STARTIMTO, RlvBehaviourInfo::BHVR_STRICT));
	addEntry(new RlvBehaviourGenericProcessor<RLV_OPTION_NONE>("temprun", RLV_BHVR_TEMPRUN));
	addEntry(new RlvBehaviourGenericProcessor<RLV_OPTION_NONE>("touchall", RLV_BHVR_TOUCHALL));
	addEntry(new RlvBehaviourGenericProcessor<RLV_OPTION_NONE_OR_EXCEPTION>("touchattach", RLV_BHVR_TOUCHATTACH));
	addEntry(new RlvBehaviourGenericProcessor<RLV_OPTION_NONE>("touchattachother", RLV_BHVR_TOUCHATTACHOTHER));
	addEntry(new RlvBehaviourGenericProcessor<RLV_OPTION_NONE>("touchattachself", RLV_BHVR_TOUCHATTACHSELF));
	addEntry(new RlvBehaviourGenericProcessor<RLV_OPTION_NONE_OR_MODIFIER>("touchfar", RLV_BHVR_FARTOUCH, RlvBehaviourInfo::BHVR_SYNONYM));
	addEntry(new RlvBehaviourGenericProcessor<RLV_OPTION_NONE_OR_EXCEPTION>("touchhud", RLV_BHVR_TOUCHHUD, RlvBehaviourInfo::BHVR_EXTENDED));
	addEntry(new RlvBehaviourGenericProcessor<RLV_OPTION_NONE>("touchme", RLV_BHVR_TOUCHME));
	addEntry(new RlvBehaviourGenericProcessor<RLV_OPTION_EXCEPTION>("touchthis", RLV_BHVR_TOUCHTHIS));
	addEntry(new RlvBehaviourGenericProcessor<RLV_OPTION_NONE_OR_EXCEPTION>("touchworld", RLV_BHVR_TOUCHWORLD));
	addEntry(new RlvBehaviourGenericProcessor<RLV_OPTION_NONE>("tplm", RLV_BHVR_TPLM));
	addEntry(new RlvBehaviourGenericProcessor<RLV_OPTION_NONE>("tploc", RLV_BHVR_TPLOC));
	addEntry(new RlvBehaviourGenericProcessor<RLV_OPTION_NONE_OR_MODIFIER>("tplocal", RLV_BHVR_TPLOCAL, RlvBehaviourInfo::BHVR_EXPERIMENTAL));
	addModifier(RLV_BHVR_TPLOCAL, RLV_MODIFIER_TPLOCALDIST, new RlvBehaviourModifier("Local Teleport Distance", RLV_MODIFIER_TPLOCAL_DEFAULT, true, new RlvBehaviourModifier_CompMin));
	addEntry(new RlvBehaviourGenericProcessor<RLV_OPTION_NONE_OR_EXCEPTION>("tplure", RLV_BHVR_TPLURE, RlvBehaviourInfo::BHVR_STRICT));
	addEntry(new RlvBehaviourGenericProcessor<RLV_OPTION_NONE_OR_EXCEPTION>("tprequest", RLV_BHVR_TPREQUEST, RlvBehaviourInfo::BHVR_STRICT | RlvBehaviourInfo::BHVR_EXTENDED));
	addEntry(new RlvBehaviourInfo("unsharedunwear",			RLV_BHVR_UNSHAREDUNWEAR,		RLV_TYPE_ADDREM));
	addEntry(new RlvBehaviourInfo("unsharedwear",			RLV_BHVR_UNSHAREDWEAR,			RLV_TYPE_ADDREM));
	addEntry(new RlvBehaviourGenericProcessor<RLV_OPTION_NONE>("unsit", RLV_BHVR_UNSIT));
	addEntry(new RlvBehaviourGenericProcessor<RLV_OPTION_NONE>("viewnote", RLV_BHVR_VIEWNOTE));
	addEntry(new RlvBehaviourGenericProcessor<RLV_OPTION_NONE>("viewscript", RLV_BHVR_VIEWSCRIPT));
	addEntry(new RlvBehaviourGenericProcessor<RLV_OPTION_NONE>("viewtexture", RLV_BHVR_VIEWTEXTURE));
	// Camera
	addEntry(new RlvBehaviourGenericToggleProcessor<RLV_BHVR_SETCAM, RLV_OPTION_NONE>("setcam"));
	addEntry(new RlvBehaviourGenericProcessor<RLV_OPTION_MODIFIER>("setcam_avdistmin", RLV_BHVR_SETCAM_AVDISTMIN, RlvBehaviourInfo::BHVR_EXPERIMENTAL));
	addModifier(RLV_BHVR_SETCAM_AVDISTMIN, RLV_MODIFIER_SETCAM_AVDISTMIN, new RlvBehaviourModifierHandler<RLV_MODIFIER_SETCAM_AVDISTMIN>("Camera - Avatar Distance (Min)", 0.0f, false, new RlvBehaviourModifier_CompMax()));
	addEntry(new RlvBehaviourGenericProcessor<RLV_OPTION_MODIFIER>("setcam_avdistmax", RLV_BHVR_SETCAM_AVDISTMAX, RlvBehaviourInfo::BHVR_EXPERIMENTAL));
	addModifier(RLV_BHVR_SETCAM_AVDISTMAX, RLV_MODIFIER_SETCAM_AVDISTMAX, new RlvBehaviourModifier("Camera - Avatar Distance (Max)", F32_MAX, false, new RlvBehaviourModifier_CompMin));
	addEntry(new RlvBehaviourGenericProcessor<RLV_OPTION_MODIFIER>("setcam_origindistmin", RLV_BHVR_SETCAM_ORIGINDISTMIN, RlvBehaviourInfo::BHVR_EXPERIMENTAL));
	addModifier(RLV_BHVR_SETCAM_ORIGINDISTMIN, RLV_MODIFIER_SETCAM_ORIGINDISTMIN, new RlvBehaviourModifier("Camera - Focus Distance (Min)", 0.0f, true, new RlvBehaviourModifier_CompMax));
	addEntry(new RlvBehaviourGenericProcessor<RLV_OPTION_MODIFIER>("setcam_origindistmax", RLV_BHVR_SETCAM_ORIGINDISTMAX, RlvBehaviourInfo::BHVR_EXPERIMENTAL));
	addModifier(RLV_BHVR_SETCAM_ORIGINDISTMAX, RLV_MODIFIER_SETCAM_ORIGINDISTMAX, new RlvBehaviourModifier("Camera - Focus Distance (Max)", F32_MAX, true, new RlvBehaviourModifier_CompMin));
	addEntry(new RlvBehaviourGenericToggleProcessor<RLV_BHVR_SETCAM_EYEOFFSET, RLV_OPTION_MODIFIER, RlvBehaviourCamEyeFocusOffsetHandler>("setcam_eyeoffset", RlvBehaviourInfo::BHVR_EXPERIMENTAL));
	addModifier(RLV_BHVR_SETCAM_EYEOFFSET, RLV_MODIFIER_SETCAM_EYEOFFSET, new RlvBehaviourModifierHandler<RLV_MODIFIER_SETCAM_EYEOFFSET>("Camera - Eye Offset", LLVector3::zero, true, nullptr));
	addEntry(new RlvBehaviourGenericToggleProcessor<RLV_BHVR_SETCAM_FOCUSOFFSET, RLV_OPTION_MODIFIER, RlvBehaviourCamEyeFocusOffsetHandler>("setcam_focusoffset", RlvBehaviourInfo::BHVR_EXPERIMENTAL));
	addModifier(RLV_BHVR_SETCAM_FOCUSOFFSET, RLV_MODIFIER_SETCAM_FOCUSOFFSET, new RlvBehaviourModifierHandler<RLV_MODIFIER_SETCAM_FOCUSOFFSET>("Camera - Focus Offset", LLVector3::zero, true, nullptr));
	addEntry(new RlvBehaviourProcessor<RLV_BHVR_SETCAM_FOVMIN, RlvBehaviourSetCamFovHandler>("setcam_fovmin"));
	addModifier(RLV_BHVR_SETCAM_FOVMIN, RLV_MODIFIER_SETCAM_FOVMIN, new RlvBehaviourModifierHandler<RLV_MODIFIER_SETCAM_FOVMIN>("Camera - FOV (Min)", DEFAULT_FIELD_OF_VIEW, true, new RlvBehaviourModifier_CompMax));
	addEntry(new RlvBehaviourProcessor<RLV_BHVR_SETCAM_FOVMAX, RlvBehaviourSetCamFovHandler>("setcam_fovmax"));
	addModifier(RLV_BHVR_SETCAM_FOVMAX, RLV_MODIFIER_SETCAM_FOVMAX, new RlvBehaviourModifierHandler<RLV_MODIFIER_SETCAM_FOVMAX>("Camera - FOV (Max)", DEFAULT_FIELD_OF_VIEW, true, new RlvBehaviourModifier_CompMin));
	addEntry(new RlvBehaviourGenericToggleProcessor<RLV_BHVR_SETCAM_MOUSELOOK, RLV_OPTION_NONE>("setcam_mouselook"));
	addEntry(new RlvBehaviourGenericProcessor<RLV_OPTION_NONE_OR_MODIFIER>("setcam_textures", RLV_BHVR_SETCAM_TEXTURES));
	addModifier(RLV_BHVR_SETCAM_TEXTURES, RLV_MODIFIER_SETCAM_TEXTURE, new RlvBehaviourModifierHandler<RLV_MODIFIER_SETCAM_TEXTURE>("Camera - Forced Texture", IMG_DEFAULT, true, nullptr));
	addEntry(new RlvBehaviourGenericToggleProcessor<RLV_BHVR_SETCAM_UNLOCK, RLV_OPTION_NONE>("setcam_unlock"));
	// Camera (compatibility shim - to be deprecated)
	addEntry(new RlvBehaviourGenericProcessor<RLV_OPTION_MODIFIER>("camdistmin", RLV_BHVR_SETCAM_AVDISTMIN, RlvBehaviourInfo::BHVR_SYNONYM | RlvBehaviourInfo::BHVR_DEPRECATED));
	addEntry(new RlvBehaviourGenericProcessor<RLV_OPTION_MODIFIER>("camdistmax", RLV_BHVR_SETCAM_AVDISTMAX, RlvBehaviourInfo::BHVR_SYNONYM | RlvBehaviourInfo::BHVR_DEPRECATED));
	addEntry(new RlvBehaviourGenericProcessor<RLV_OPTION_NONE_OR_MODIFIER>("camtextures", RLV_BHVR_SETCAM_TEXTURES, RlvBehaviourInfo::BHVR_SYNONYM | RlvBehaviourInfo::BHVR_DEPRECATED));
	addEntry(new RlvBehaviourProcessor<RLV_BHVR_CAMZOOMMIN, RlvBehaviourCamZoomMinMaxHandler>("camzoommin", RlvBehaviourInfo::BHVR_DEPRECATED));
	addEntry(new RlvBehaviourProcessor<RLV_BHVR_CAMZOOMMAX, RlvBehaviourCamZoomMinMaxHandler>("camzoommax", RlvBehaviourInfo::BHVR_DEPRECATED));
	addEntry(new RlvBehaviourGenericToggleProcessor<RLV_BHVR_SETCAM_UNLOCK, RLV_OPTION_NONE>("camunlock", RlvBehaviourInfo::BHVR_SYNONYM | RlvBehaviourInfo::BHVR_DEPRECATED));

	//
	// Force-wear
	//
	addEntry(new RlvBehaviourInfo("attach",					RLV_CMD_FORCEWEAR,	RLV_TYPE_FORCE, RlvBehaviourInfo::FORCEWEAR_WEAR_REPLACE | RlvBehaviourInfo::FORCEWEAR_NODE    | RlvBehaviourInfo::FORCEWEAR_CONTEXT_NONE));
	addEntry(new RlvBehaviourInfo("attachall",				RLV_CMD_FORCEWEAR,	RLV_TYPE_FORCE, RlvBehaviourInfo::FORCEWEAR_WEAR_REPLACE | RlvBehaviourInfo::FORCEWEAR_SUBTREE | RlvBehaviourInfo::FORCEWEAR_CONTEXT_NONE));
	addEntry(new RlvBehaviourInfo("attachover",				RLV_CMD_FORCEWEAR,	RLV_TYPE_FORCE, RlvBehaviourInfo::FORCEWEAR_WEAR_ADD     | RlvBehaviourInfo::FORCEWEAR_NODE    | RlvBehaviourInfo::FORCEWEAR_CONTEXT_NONE));
	addEntry(new RlvBehaviourInfo("attachallover",			RLV_CMD_FORCEWEAR,	RLV_TYPE_FORCE, RlvBehaviourInfo::FORCEWEAR_WEAR_ADD     | RlvBehaviourInfo::FORCEWEAR_SUBTREE | RlvBehaviourInfo::FORCEWEAR_CONTEXT_NONE));
	addEntry(new RlvBehaviourInfo("attachthis",				RLV_CMD_FORCEWEAR,	RLV_TYPE_FORCE, RlvBehaviourInfo::FORCEWEAR_WEAR_REPLACE | RlvBehaviourInfo::FORCEWEAR_NODE    | RlvBehaviourInfo::FORCEWEAR_CONTEXT_OBJECT));
	addEntry(new RlvBehaviourInfo("attachallthis",			RLV_CMD_FORCEWEAR,	RLV_TYPE_FORCE, RlvBehaviourInfo::FORCEWEAR_WEAR_REPLACE | RlvBehaviourInfo::FORCEWEAR_SUBTREE | RlvBehaviourInfo::FORCEWEAR_CONTEXT_OBJECT));
	addEntry(new RlvBehaviourInfo("attachthisover",			RLV_CMD_FORCEWEAR,	RLV_TYPE_FORCE, RlvBehaviourInfo::FORCEWEAR_WEAR_ADD     | RlvBehaviourInfo::FORCEWEAR_NODE    | RlvBehaviourInfo::FORCEWEAR_CONTEXT_OBJECT));
	addEntry(new RlvBehaviourInfo("attachallthisover",		RLV_CMD_FORCEWEAR,	RLV_TYPE_FORCE, RlvBehaviourInfo::FORCEWEAR_WEAR_ADD     | RlvBehaviourInfo::FORCEWEAR_SUBTREE | RlvBehaviourInfo::FORCEWEAR_CONTEXT_OBJECT));
	addEntry(new RlvForceProcessor<RLV_BHVR_DETACH, RlvForceRemAttachHandler>("detach",			RlvBehaviourInfo::FORCEWEAR_WEAR_REMOVE  | RlvBehaviourInfo::FORCEWEAR_NODE    | RlvBehaviourInfo::FORCEWEAR_CONTEXT_NONE));
	addEntry(new RlvBehaviourInfo("detachall",				RLV_CMD_FORCEWEAR,	RLV_TYPE_FORCE, RlvBehaviourInfo::FORCEWEAR_WEAR_REMOVE  | RlvBehaviourInfo::FORCEWEAR_SUBTREE | RlvBehaviourInfo::FORCEWEAR_CONTEXT_NONE));
	addEntry(new RlvBehaviourInfo("detachthis",				RLV_CMD_FORCEWEAR,	RLV_TYPE_FORCE, RlvBehaviourInfo::FORCEWEAR_WEAR_REMOVE  | RlvBehaviourInfo::FORCEWEAR_NODE    | RlvBehaviourInfo::FORCEWEAR_CONTEXT_OBJECT));
	addEntry(new RlvBehaviourInfo("detachallthis",			RLV_CMD_FORCEWEAR,	RLV_TYPE_FORCE, RlvBehaviourInfo::FORCEWEAR_WEAR_REMOVE  | RlvBehaviourInfo::FORCEWEAR_SUBTREE | RlvBehaviourInfo::FORCEWEAR_CONTEXT_OBJECT));
	addEntry(new RlvForceProcessor<RLV_BHVR_REMATTACH, RlvForceRemAttachHandler>("remattach",	RlvBehaviourInfo::FORCEWEAR_WEAR_REMOVE  | RlvBehaviourInfo::FORCEWEAR_NODE    | RlvBehaviourInfo::FORCEWEAR_CONTEXT_NONE));
	addEntry(new RlvForceProcessor<RLV_BHVR_REMOUTFIT>("remoutfit",                             RlvBehaviourInfo::FORCEWEAR_WEAR_REMOVE  | RlvBehaviourInfo::FORCEWEAR_NODE    | RlvBehaviourInfo::FORCEWEAR_CONTEXT_NONE));
	// Synonyms (addoutfit* -> attach*)														   
	addEntry(new RlvBehaviourInfo("addoutfit",				RLV_CMD_FORCEWEAR,	RLV_TYPE_FORCE, RlvBehaviourInfo::FORCEWEAR_WEAR_REPLACE | RlvBehaviourInfo::FORCEWEAR_NODE    | RlvBehaviourInfo::FORCEWEAR_CONTEXT_NONE   | RlvBehaviourInfo::BHVR_SYNONYM));
	addEntry(new RlvBehaviourInfo("addoutfitall",			RLV_CMD_FORCEWEAR,	RLV_TYPE_FORCE, RlvBehaviourInfo::FORCEWEAR_WEAR_REPLACE | RlvBehaviourInfo::FORCEWEAR_SUBTREE | RlvBehaviourInfo::FORCEWEAR_CONTEXT_NONE   | RlvBehaviourInfo::BHVR_SYNONYM));
	addEntry(new RlvBehaviourInfo("addoutfitover",			RLV_CMD_FORCEWEAR,	RLV_TYPE_FORCE, RlvBehaviourInfo::FORCEWEAR_WEAR_ADD     | RlvBehaviourInfo::FORCEWEAR_NODE    | RlvBehaviourInfo::FORCEWEAR_CONTEXT_NONE   | RlvBehaviourInfo::BHVR_SYNONYM));
	addEntry(new RlvBehaviourInfo("addoutfitallover",		RLV_CMD_FORCEWEAR,	RLV_TYPE_FORCE, RlvBehaviourInfo::FORCEWEAR_WEAR_ADD     | RlvBehaviourInfo::FORCEWEAR_SUBTREE | RlvBehaviourInfo::FORCEWEAR_CONTEXT_NONE   | RlvBehaviourInfo::BHVR_SYNONYM));
	addEntry(new RlvBehaviourInfo("addoutfitthis",			RLV_CMD_FORCEWEAR,	RLV_TYPE_FORCE, RlvBehaviourInfo::FORCEWEAR_WEAR_REPLACE | RlvBehaviourInfo::FORCEWEAR_NODE    | RlvBehaviourInfo::FORCEWEAR_CONTEXT_OBJECT | RlvBehaviourInfo::BHVR_SYNONYM));
	addEntry(new RlvBehaviourInfo("addoutfitallthis",		RLV_CMD_FORCEWEAR,	RLV_TYPE_FORCE, RlvBehaviourInfo::FORCEWEAR_WEAR_REPLACE | RlvBehaviourInfo::FORCEWEAR_SUBTREE | RlvBehaviourInfo::FORCEWEAR_CONTEXT_OBJECT | RlvBehaviourInfo::BHVR_SYNONYM));
	addEntry(new RlvBehaviourInfo("addoutfitthisover",		RLV_CMD_FORCEWEAR,	RLV_TYPE_FORCE, RlvBehaviourInfo::FORCEWEAR_WEAR_ADD     | RlvBehaviourInfo::FORCEWEAR_NODE    | RlvBehaviourInfo::FORCEWEAR_CONTEXT_OBJECT | RlvBehaviourInfo::BHVR_SYNONYM));
	addEntry(new RlvBehaviourInfo("addoutfitallthisover",	RLV_CMD_FORCEWEAR,	RLV_TYPE_FORCE, RlvBehaviourInfo::FORCEWEAR_WEAR_ADD     | RlvBehaviourInfo::FORCEWEAR_SUBTREE | RlvBehaviourInfo::FORCEWEAR_CONTEXT_OBJECT | RlvBehaviourInfo::BHVR_SYNONYM));
	// Synonyms (attach*overorreplace -> attach*)														   
	addEntry(new RlvBehaviourInfo("attachoverorreplace",	RLV_CMD_FORCEWEAR,  RLV_TYPE_FORCE, RlvBehaviourInfo::FORCEWEAR_WEAR_REPLACE | RlvBehaviourInfo::FORCEWEAR_NODE    | RlvBehaviourInfo::FORCEWEAR_CONTEXT_NONE   | RlvBehaviourInfo::BHVR_SYNONYM));
	addEntry(new RlvBehaviourInfo("attachalloverorreplace",	RLV_CMD_FORCEWEAR,  RLV_TYPE_FORCE, RlvBehaviourInfo::FORCEWEAR_WEAR_REPLACE | RlvBehaviourInfo::FORCEWEAR_SUBTREE | RlvBehaviourInfo::FORCEWEAR_CONTEXT_NONE   | RlvBehaviourInfo::BHVR_SYNONYM));
	addEntry(new RlvBehaviourInfo("attachthisoverorreplace",RLV_CMD_FORCEWEAR,  RLV_TYPE_FORCE, RlvBehaviourInfo::FORCEWEAR_WEAR_REPLACE | RlvBehaviourInfo::FORCEWEAR_NODE    | RlvBehaviourInfo::FORCEWEAR_CONTEXT_OBJECT | RlvBehaviourInfo::BHVR_SYNONYM));
	addEntry(new RlvBehaviourInfo("attachallthisoverorreplace",RLV_CMD_FORCEWEAR,RLV_TYPE_FORCE,RlvBehaviourInfo::FORCEWEAR_WEAR_REPLACE | RlvBehaviourInfo::FORCEWEAR_SUBTREE | RlvBehaviourInfo::FORCEWEAR_CONTEXT_OBJECT | RlvBehaviourInfo::BHVR_SYNONYM));

	//
	// Force-only
	//
	addEntry(new RlvBehaviourInfo("adjustheight",			RLV_BHVR_ADJUSTHEIGHT,			RLV_TYPE_FORCE));
	addEntry(new RlvForceProcessor<RLV_BHVR_DETACHME>("detachme"));
	addEntry(new RlvForceProcessor<RLV_BHVR_SETCAM_FOCUS>("setcam_focus", RlvBehaviourInfo::BHVR_EXPERIMENTAL));
	addEntry(new RlvForceProcessor<RLV_BHVR_SETCAM_EYEOFFSET, RlvForceCamEyeFocusOffsetHandler>("setcam_eyeoffset", RlvBehaviourInfo::BHVR_EXPERIMENTAL));
	addEntry(new RlvForceProcessor<RLV_BHVR_SETCAM_FOCUSOFFSET, RlvForceCamEyeFocusOffsetHandler>("setcam_focusoffset", RlvBehaviourInfo::BHVR_EXPERIMENTAL));
	addEntry(new RlvForceProcessor<RLV_BHVR_SETCAM_FOV>("setcam_fov", RlvBehaviourInfo::BHVR_EXPERIMENTAL));
	addEntry(new RlvForceProcessor<RLV_BHVR_SETCAM_MODE>("setcam_mode", RlvBehaviourInfo::BHVR_EXPERIMENTAL));
	addEntry(new RlvForceProcessor<RLV_BHVR_SETGROUP>("setgroup"));
	addEntry(new RlvForceProcessor<RLV_BHVR_SIT>("sit"));
	addEntry(new RlvForceProcessor<RLV_BHVR_TPTO>("tpto"));
	addEntry(new RlvBehaviourInfo("unsit",					RLV_BHVR_UNSIT,					RLV_TYPE_FORCE));

	//
	// Reply-only
	//
	addEntry(new RlvBehaviourInfo("findfolder",				RLV_BHVR_FINDFOLDER,			RLV_TYPE_REPLY));
	addEntry(new RlvBehaviourInfo("findfolders",			RLV_BHVR_FINDFOLDERS,			RLV_TYPE_REPLY, RlvBehaviourInfo::BHVR_EXTENDED));
	addEntry(new RlvBehaviourInfo("getaddattachnames",		RLV_BHVR_GETADDATTACHNAMES,		RLV_TYPE_REPLY, RlvBehaviourInfo::BHVR_EXPERIMENTAL));
	addEntry(new RlvBehaviourInfo("getaddoutfitnames",		RLV_BHVR_GETADDOUTFITNAMES,		RLV_TYPE_REPLY, RlvBehaviourInfo::BHVR_EXPERIMENTAL));
	addEntry(new RlvBehaviourInfo("getattach",				RLV_BHVR_GETATTACH,				RLV_TYPE_REPLY));
	addEntry(new RlvBehaviourInfo("getattachnames",			RLV_BHVR_GETATTACHNAMES,		RLV_TYPE_REPLY, RlvBehaviourInfo::BHVR_EXPERIMENTAL));
	addEntry(new RlvReplyProcessor<RLV_BHVR_GETCAM_AVDIST>("getcam_avdist", RlvBehaviourInfo::BHVR_EXPERIMENTAL));
	addEntry(new RlvReplyProcessor<RLV_BHVR_GETCAM_AVDISTMIN, RlvReplyCamMinMaxModifierHandler>("getcam_avdistmin", RlvBehaviourInfo::BHVR_EXPERIMENTAL));
	addEntry(new RlvReplyProcessor<RLV_BHVR_GETCAM_AVDISTMAX, RlvReplyCamMinMaxModifierHandler>("getcam_avdistmax", RlvBehaviourInfo::BHVR_EXPERIMENTAL));
	addEntry(new RlvReplyProcessor<RLV_BHVR_GETCAM_FOV>("getcam_fov", RlvBehaviourInfo::BHVR_EXPERIMENTAL));
	addEntry(new RlvReplyProcessor<RLV_BHVR_GETCAM_FOVMIN, RlvReplyCamMinMaxModifierHandler>("getcam_fovmin", RlvBehaviourInfo::BHVR_EXPERIMENTAL));
	addEntry(new RlvReplyProcessor<RLV_BHVR_GETCAM_FOVMAX, RlvReplyCamMinMaxModifierHandler>("getcam_fovmax", RlvBehaviourInfo::BHVR_EXPERIMENTAL));
	addEntry(new RlvReplyProcessor<RLV_BHVR_GETCAM_TEXTURES>("getcam_textures", RlvBehaviourInfo::BHVR_EXPERIMENTAL));
	addEntry(new RlvReplyProcessor<RLV_BHVR_GETCOMMAND>("getcommand", RlvBehaviourInfo::BHVR_EXTENDED));
	addEntry(new RlvBehaviourInfo("getgroup",				RLV_BHVR_GETGROUP,				RLV_TYPE_REPLY));
	addEntry(new RlvBehaviourInfo("getinv",					RLV_BHVR_GETINV,				RLV_TYPE_REPLY));
	addEntry(new RlvBehaviourInfo("getinvworn",				RLV_BHVR_GETINVWORN,			RLV_TYPE_REPLY));
	addEntry(new RlvBehaviourInfo("getoutfit",				RLV_BHVR_GETOUTFIT,				RLV_TYPE_REPLY));
	addEntry(new RlvBehaviourInfo("getoutfitnames",			RLV_BHVR_GETOUTFITNAMES,		RLV_TYPE_REPLY, RlvBehaviourInfo::BHVR_EXPERIMENTAL));
	addEntry(new RlvBehaviourInfo("getpath",				RLV_BHVR_GETPATH,				RLV_TYPE_REPLY));
	addEntry(new RlvBehaviourInfo("getpathnew",				RLV_BHVR_GETPATHNEW,			RLV_TYPE_REPLY));
	addEntry(new RlvBehaviourInfo("getremattachnames",		RLV_BHVR_GETREMATTACHNAMES,		RLV_TYPE_REPLY, RlvBehaviourInfo::BHVR_EXPERIMENTAL));
	addEntry(new RlvBehaviourInfo("getremoutfitnames",		RLV_BHVR_GETREMOUTFITNAMES,		RLV_TYPE_REPLY, RlvBehaviourInfo::BHVR_EXPERIMENTAL));
	addEntry(new RlvBehaviourInfo("getsitid",				RLV_BHVR_GETSITID,				RLV_TYPE_REPLY));
	addEntry(new RlvBehaviourInfo("getstatus",				RLV_BHVR_GETSTATUS,				RLV_TYPE_REPLY));
	addEntry(new RlvBehaviourInfo("getstatusall",			RLV_BHVR_GETSTATUSALL,			RLV_TYPE_REPLY));
	addEntry(new RlvBehaviourInfo("version",				RLV_BHVR_VERSION,				RLV_TYPE_REPLY));
	addEntry(new RlvBehaviourInfo("versionnew",				RLV_BHVR_VERSIONNEW,			RLV_TYPE_REPLY));
	addEntry(new RlvBehaviourInfo("versionnum",				RLV_BHVR_VERSIONNUM,			RLV_TYPE_REPLY));

	// Populate m_String2InfoMap (the tuple <behaviour, type> should be unique)
	for (const RlvBehaviourInfo* pBhvrInfo : m_BhvrInfoList)
	{
		RLV_VERIFY(m_String2InfoMap.insert(std::make_pair(std::make_pair(pBhvrInfo->getBehaviour(), (ERlvParamType)pBhvrInfo->getParamTypeMask()), pBhvrInfo)).second == true);
	}

	// Populate m_Bhvr2InfoMap (there can be multiple entries per ERlvBehaviour)
	for (const RlvBehaviourInfo* pBhvrInfo : m_BhvrInfoList)
	{
		if ( (pBhvrInfo->getParamTypeMask() & RLV_TYPE_ADDREM) && (!pBhvrInfo->isSynonym()) )
		{
#ifdef RLV_DEBUG
			for (const rlv_bhvr2info_map_t::value_type& itBhvr : boost::make_iterator_range(m_Bhvr2InfoMap.lower_bound(pBhvrInfo->getBehaviourType()), m_Bhvr2InfoMap.upper_bound(pBhvrInfo->getBehaviourType())))
			{
				RLV_ASSERT( (itBhvr.first != pBhvrInfo->getBehaviourType()) || (itBhvr.second->getBehaviourFlags() != pBhvrInfo->getBehaviourFlags()) );
			}
#endif // RLV_DEBUG
			m_Bhvr2InfoMap.insert(std::pair<ERlvBehaviour, const RlvBehaviourInfo*>(pBhvrInfo->getBehaviourType(), pBhvrInfo));
		}
	}

	// Process blocked commands
	if (RlvSettings::getNoSetEnv())
		toggleBehaviourFlag("setenv", RLV_TYPE_ADDREM, RlvBehaviourInfo::BHVR_BLOCKED, true);
}

RlvBehaviourDictionary::~RlvBehaviourDictionary()
{
	for (const RlvBehaviourInfo* pBhvrInfo : m_BhvrInfoList)
		delete pBhvrInfo;
	m_BhvrInfoList.clear();

	for (int idxBhvrMod = 0; idxBhvrMod < RLV_MODIFIER_COUNT; idxBhvrMod++)
	{
		delete m_BehaviourModifiers[idxBhvrMod];
		m_BehaviourModifiers[idxBhvrMod] = nullptr;
	}
}

void RlvBehaviourDictionary::addEntry(const RlvBehaviourInfo* pEntry)
{
	// Filter experimental commands (if disabled)
	static LLCachedControl<bool> sEnableExperimental(gSavedSettings, "RLVaExperimentalCommands");
	if ( (!pEntry) || ((!sEnableExperimental) && (pEntry->isExperimental())) )
	{
		return;
	}

	// Sanity check for duplicate entries
#ifndef LL_RELEASE_FOR_DOWNLOAD
	std::for_each(m_BhvrInfoList.begin(), m_BhvrInfoList.end(), 
	              [&pEntry](const RlvBehaviourInfo* pBhvrInfo) { 
					RLV_ASSERT_DBG( ((pBhvrInfo->getBehaviour()) != (pEntry->getBehaviour())) || ((pBhvrInfo->getParamTypeMask() & pEntry->getParamTypeMask()) == 0) );
	              });
#endif // LL_RELEASE_FOR_DOWNLOAD

	m_BhvrInfoList.push_back(pEntry);
}

void RlvBehaviourDictionary::addModifier(ERlvBehaviour eBhvr, ERlvBehaviourModifier eModifier, RlvBehaviourModifier* pModifierEntry)
{
	if (eModifier < RLV_MODIFIER_COUNT)
	{
		m_BehaviourModifiers[eModifier] = pModifierEntry;
		m_Bhvr2ModifierMap.insert(std::make_pair(eBhvr, eModifier));
	}
}

const RlvBehaviourInfo* RlvBehaviourDictionary::getBehaviourInfo(const std::string& strBhvr, ERlvParamType eParamType, bool* pfStrict) const
{
	bool fStrict = boost::algorithm::ends_with(strBhvr, "_sec");
	if (pfStrict)
		*pfStrict = fStrict;

	rlv_string2info_map_t::const_iterator itBhvr = m_String2InfoMap.find(std::make_pair( (!fStrict) ? strBhvr : strBhvr.substr(0, strBhvr.size() - 4), (eParamType & RLV_TYPE_ADDREM) ? RLV_TYPE_ADDREM : eParamType));
	return ( (itBhvr != m_String2InfoMap.end()) && ((!fStrict) || (itBhvr->second->hasStrict())) ) ? itBhvr->second : NULL;
}

ERlvBehaviour RlvBehaviourDictionary::getBehaviourFromString(const std::string& strBhvr, ERlvParamType eParamType, bool* pfStrict) const
{
	const RlvBehaviourInfo* pBhvrInfo = getBehaviourInfo(strBhvr, eParamType, pfStrict);
	return (pBhvrInfo) ? pBhvrInfo->getBehaviourType() : RLV_BHVR_UNKNOWN;
}

bool RlvBehaviourDictionary::getCommands(const std::string& strMatch, ERlvParamType eParamType, std::list<std::string>& cmdList) const
{
	cmdList.clear();
	for (const RlvBehaviourInfo* pBhvrInfo : m_BhvrInfoList)
	{
		if ( (pBhvrInfo->getParamTypeMask() & eParamType) || (RLV_TYPE_UNKNOWN == eParamType) )
		{
			std::string strCmd = pBhvrInfo->getBehaviour();
			if ( (std::string::npos != strCmd.find(strMatch)) || (strMatch.empty()) )
				cmdList.push_back(strCmd);
			if ( (pBhvrInfo->hasStrict()) && ((std::string::npos != strCmd.append("_sec").find(strMatch)) || (strMatch.empty())) )
				cmdList.push_back(strCmd);
		}
	}
	return !cmdList.empty();
}

bool RlvBehaviourDictionary::getHasStrict(ERlvBehaviour eBhvr) const
{
	for (const rlv_bhvr2info_map_t::value_type& itBhvr : boost::make_iterator_range(m_Bhvr2InfoMap.lower_bound(eBhvr), m_Bhvr2InfoMap.upper_bound(eBhvr)))
	{
		// Only restrictions can be strict
		if (RLV_TYPE_ADDREM != itBhvr.second->getParamTypeMask())
			continue;
		return itBhvr.second->hasStrict();
	}
	RLV_ASSERT(false);
	return false;
}

RlvBehaviourModifier* RlvBehaviourDictionary::getModifierFromBehaviour(ERlvBehaviour eBhvr) const
{
	rlv_bhvr2mod_map_t::const_iterator itMod = m_Bhvr2ModifierMap.find(eBhvr);
	ERlvBehaviourModifier eBhvrMod = (m_Bhvr2ModifierMap.end() != itMod) ? itMod->second : RLV_MODIFIER_UNKNOWN;
	return (eBhvrMod < RLV_MODIFIER_UNKNOWN) ? m_BehaviourModifiers[eBhvrMod] : nullptr;
}

void RlvBehaviourDictionary::toggleBehaviourFlag(const std::string& strBhvr, ERlvParamType eParamType, RlvBehaviourInfo::EBehaviourFlags eBhvrFlag, bool fEnable)
{
	rlv_string2info_map_t::const_iterator itBhvr = m_String2InfoMap.find(std::make_pair(strBhvr, (eParamType & RLV_TYPE_ADDREM) ? RLV_TYPE_ADDREM : eParamType));
	if (m_String2InfoMap.end() != itBhvr)
	{
		const_cast<RlvBehaviourInfo*>(itBhvr->second)->toggleBehaviourFlag(eBhvrFlag, fEnable);
	}
}

// ============================================================================
// RlvBehaviourModifier
//

RlvBehaviourModifier::RlvBehaviourModifier(std::string strName, const RlvBehaviourModifierValue& defaultValue, bool fAddDefaultOnEmpty, RlvBehaviourModifier_Comp* pValueComparator)
	: m_strName(strName), m_DefaultValue(defaultValue), m_fAddDefaultOnEmpty(fAddDefaultOnEmpty), m_pValueComparator(pValueComparator)
{
	m_pValueComparator = (pValueComparator) ? pValueComparator : new RlvBehaviourModifier_Comp();
}

RlvBehaviourModifier::~RlvBehaviourModifier()
{
	if (m_pValueComparator)
	{
		delete m_pValueComparator;
		m_pValueComparator = NULL;
	}
}

bool RlvBehaviourModifier::addValue(const RlvBehaviourModifierValue& modValue, const LLUUID& idObject)
{
	if (modValue.which() == m_DefaultValue.which())
	{
		m_Values.insert((m_pValueComparator) ? std::lower_bound(m_Values.begin(), m_Values.end(), std::make_pair(modValue, idObject), boost::bind(&RlvBehaviourModifier_Comp::operator(), m_pValueComparator, _1, _2)) : m_Values.end(), std::make_pair(modValue, idObject));
		// NOTE: change signal needs to trigger before modifier handlers so cached values have a chance to update properly
		m_ChangeSignal(getValue());
		onValueChange();
		return true;
	}
	return false;
}

const LLUUID& RlvBehaviourModifier::getPrimaryObject() const
{
	return (m_pValueComparator) ? m_pValueComparator->m_idPrimaryObject : LLUUID::null;
}

bool RlvBehaviourModifier::hasValue() const {
	// If no primary object is set this returns "any value set"; otherwise it returns "any value set by the primary object"
	if ( (!m_pValueComparator) || (m_pValueComparator->m_idPrimaryObject.isNull()) )
		return !m_Values.empty();
	return (!m_Values.empty()) ? m_Values.front().second == m_pValueComparator->m_idPrimaryObject : false;
}

void RlvBehaviourModifier::removeValue(const RlvBehaviourModifierValue& modValue, const LLUUID& idObject)
{
	if ( (modValue.which() == m_DefaultValue.which()) )
	{
		auto itValue = std::find(m_Values.begin(), m_Values.end(), std::make_pair(modValue, idObject));
		if (m_Values.end() != itValue)
		{
			m_Values.erase(itValue);
			onValueChange();
			m_ChangeSignal(getValue());
		}
	}
}

void RlvBehaviourModifier::setPrimaryObject(const LLUUID& idPrimaryObject)
{
	if (m_pValueComparator)
	{
		m_pValueComparator->m_idPrimaryObject = idPrimaryObject;
		m_Values.sort(boost::bind(&RlvBehaviourModifier_Comp::operator(), m_pValueComparator, _1, _2));
		onValueChange();
		m_ChangeSignal(getValue());
	}
}

bool RlvBehaviourModifier::convertOptionValue(const std::string& optionValue, RlvBehaviourModifierValue& modValue) const
{
	try
	{
		if (typeid(float) == m_DefaultValue.type())
		{
			modValue = std::stof(optionValue);
			return true;
		}
		else if (typeid(int) == m_DefaultValue.type())
		{
			modValue = std::stoi(optionValue);
			return true;
		}
		else if (typeid(LLVector3) == m_DefaultValue.type())
		{
			LLVector3 vecOption;
			if (3 == sscanf(optionValue.c_str(), "%f/%f/%f", vecOption.mV + 0, vecOption.mV + 1, vecOption.mV + 2))
			{
				modValue = vecOption;
				return true;
			}
		}
		else if (typeid(LLUUID) == m_DefaultValue.type())
		{
			LLUUID idOption;
			if (LLUUID::parseUUID(optionValue, &idOption))
			{
				modValue = idOption;
				return true;
			}
		}
		return false;
	}
	catch (const std::invalid_argument&)
	{
		return false;
	}
}

// ============================================================================
// RlvCommmand
//

RlvCommand::RlvCommand(const LLUUID& idObj, const std::string& strCommand)
	: m_fValid(false), m_idObj(idObj), m_pBhvrInfo(NULL), m_eParamType(RLV_TYPE_UNKNOWN), m_fStrict(false), m_fRefCounted(false)
{
	if (m_fValid = parseCommand(strCommand, m_strBehaviour, m_strOption, m_strParam))
	{
		S32 nTemp = 0;
		if ( ("n" == m_strParam) || ("add" == m_strParam) )
			m_eParamType = RLV_TYPE_ADD;
		else if ( ("y" == m_strParam) || ("rem" == m_strParam) )
			m_eParamType = RLV_TYPE_REMOVE;
		else if (m_strBehaviour == "clear")						// clear is the odd one out so just make it its own type
			m_eParamType = RLV_TYPE_CLEAR;
		else if ("force" == m_strParam)
			m_eParamType = RLV_TYPE_FORCE;
		else if (LLStringUtil::convertToS32(m_strParam, nTemp))	// Assume it's a reply command if we can convert <param> to an S32
			m_eParamType = RLV_TYPE_REPLY;
		else
		{
			m_eParamType = RLV_TYPE_UNKNOWN;
			m_fValid = false;
		}
	}

	if (!m_fValid)
	{
		m_strOption = m_strParam = "";
		return;
	}

	m_pBhvrInfo = RlvBehaviourDictionary::instance().getBehaviourInfo(m_strBehaviour, m_eParamType, &m_fStrict);
}

RlvCommand::RlvCommand(const RlvCommand& rlvCmd, ERlvParamType eParamType)
	: m_fValid(rlvCmd.m_fValid), m_idObj(rlvCmd.m_idObj), m_strBehaviour(rlvCmd.m_strBehaviour), m_pBhvrInfo(rlvCmd.m_pBhvrInfo),
	  m_eParamType( (RLV_TYPE_UNKNOWN == eParamType) ? rlvCmd.m_eParamType : eParamType),m_fStrict(rlvCmd.m_fStrict), m_strOption(rlvCmd.m_strOption), m_strParam(rlvCmd.m_strParam), m_fRefCounted(false)
{
}

bool RlvCommand::parseCommand(const std::string& strCommand, std::string& strBehaviour, std::string& strOption, std::string& strParam)
{
	// (See behaviour notes for the command parsing truth table)

	// Format: <behaviour>[:<option>]=<param>
	int idxParam  = strCommand.find('=');
	int idxOption = (idxParam > 0) ? strCommand.find(':') : -1;
	if (idxOption > idxParam - 1)
		idxOption = -1;

	// If <behaviour> is missing it's always an improperly formatted command
	if ( (0 == idxOption) || (0 == idxParam) )
		return false;

	strBehaviour = strCommand.substr(0, (-1 != idxOption) ? idxOption : idxParam);
	strOption = strParam = "";

	// If <param> is missing it's an improperly formatted command
	if ( (-1 == idxParam) || ((int)strCommand.length() - 1 == idxParam) )
	{
		// Unless "<behaviour> == "clear" AND (idxOption == 0)" 
		// OR <behaviour> == "clear" AND (idxParam != 0) [see table above]
		if ( ("clear" == strBehaviour) && ( (!idxOption) || (idxParam) ) )
			return true;
		return false;
	}

	if ( (-1 != idxOption) && (idxOption + 1 != idxParam) )
		strOption = strCommand.substr(idxOption + 1, idxParam - idxOption - 1);
	strParam = strCommand.substr(idxParam + 1);

	return true;
}

// ============================================================================
// Command option parsing utility classes
//

template<>
bool RlvCommandOptionHelper::parseOption<LLUUID>(const std::string& strOption, LLUUID& idOption)
{
	idOption.set(strOption);
	return idOption.notNull();
}

template<>
bool RlvCommandOptionHelper::parseOption<int>(const std::string& strOption, int& nOption)
{
	try
	{
		nOption = std::stoi(strOption);
	}
	catch (const std::invalid_argument&)
	{
		return false;
	}
	return true;
}

template<>
bool RlvCommandOptionHelper::parseOption<float>(const std::string& strOption, float& nOption)
{
	try
	{
		nOption = std::stof(strOption);
	}
	catch (const std::invalid_argument&)
	{
		return false;
	}
	return true;
}

template<>
bool RlvCommandOptionHelper::parseOption<LLWearableType::EType>(const std::string& strOption, LLWearableType::EType& wtOption)
{
	wtOption = LLWearableType::typeNameToType(strOption);
	return (LLWearableType::WT_INVALID != wtOption) && (LLWearableType::WT_NONE != wtOption);
}

template<>
bool RlvCommandOptionHelper::parseOption<LLViewerJointAttachment*>(const std::string& strOption, LLViewerJointAttachment*& pAttachPt)
{
	pAttachPt = RlvAttachPtLookup::getAttachPoint(strOption);
	return pAttachPt != NULL;
}

template<>
bool RlvCommandOptionHelper::parseOption<ERlvAttachGroupType>(const std::string& strOption, ERlvAttachGroupType& eAttachGroup)
{
	eAttachGroup = rlvAttachGroupFromString(strOption);
	return eAttachGroup != RLV_ATTACHGROUP_INVALID;
}

template<>
bool RlvCommandOptionHelper::parseOption<LLViewerInventoryCategory*>(const std::string& strOption, LLViewerInventoryCategory*& pFolder)
{
	pFolder = RlvInventory::instance().getSharedFolder(strOption);
	return pFolder != NULL;
}

template<>
bool RlvCommandOptionHelper::parseOption<LLVector3>(const std::string& strOption, LLVector3& vecOption)
 {
	if (!strOption.empty())
	{
		S32 cntToken = sscanf(strOption.c_str(), "%f/%f/%f", vecOption.mV + 0, vecOption.mV + 1, vecOption.mV + 2);
		return (3 == cntToken);
	}
	return false;
}

template<>
bool RlvCommandOptionHelper::parseOption<LLVector3d>(const std::string& strOption, LLVector3d& vecOption)
 {
	if (!strOption.empty())
	{
		S32 cntToken = sscanf(strOption.c_str(), "%lf/%lf/%lf", vecOption.mdV + 0, vecOption.mdV + 1, vecOption.mdV + 2);
		return (3 == cntToken);
	}
	return false;
}

template<>
bool RlvCommandOptionHelper::parseOption<RlvCommandOptionGeneric>(const std::string& strOption, RlvCommandOptionGeneric& genericOption)
{
	LLWearableType::EType wtType(LLWearableType::WT_INVALID); LLUUID idOption; ERlvAttachGroupType eAttachGroup(RLV_ATTACHGROUP_INVALID);
	LLViewerJointAttachment* pAttachPt = NULL; LLViewerInventoryCategory* pFolder = NULL; LLVector3d posOption; float nOption;

	if (!strOption.empty())																		// <option> could be an empty string
	{
		if (RlvCommandOptionHelper::parseOption(strOption, wtType))
			genericOption = wtType;																// ... or specify a (valid) clothing layer
		else if (RlvCommandOptionHelper::parseOption(strOption, pAttachPt))
			genericOption = pAttachPt;															// ... or specify an attachment point
		else if (RlvCommandOptionHelper::parseOption(strOption, idOption))
			genericOption = idOption;															// ... or specify an UUID
		else if (RlvCommandOptionHelper::parseOption(strOption, pFolder))
			genericOption = pFolder;															// ... or specify a shared folder path
		else if (RlvCommandOptionHelper::parseOption(strOption, eAttachGroup))
			genericOption = eAttachGroup;														// ... or specify an attachment point group
		else if (RlvCommandOptionHelper::parseOption(strOption, posOption))
			genericOption = posOption;															// ... or specify a vector (region or global coordinates)
		else if (RlvCommandOptionHelper::parseOption(strOption, nOption))
			genericOption = nOption;															// ... or specify a number
		else
			genericOption = strOption;															// ... or it might just be a string
	}
	return true;
}

bool RlvCommandOptionHelper::parseStringList(const std::string& strOption, std::vector<std::string>& optionList, const std::string& strSeparator)
{
	if (!strOption.empty())
		boost::split(optionList, strOption, boost::is_any_of(strSeparator));
	return !optionList.empty();
}

// ============================================================================
// RlvCommandOption structures
//

// Checked: 2012-07-28 (RLVa-1.4.7)
class RlvCommandOptionGetPathCallback
{
public:
	RlvCommandOptionGetPathCallback(const LLUUID& idAttachObj, RlvCommandOptionGetPath::getpath_callback_t cb)
		: mObjectId(idAttachObj), mCallback(cb)
	{
		if (isAgentAvatarValid())
			mAttachmentConnection = gAgentAvatarp->setAttachmentCallback(boost::bind(&RlvCommandOptionGetPathCallback::onAttachment, this, _1, _3));
		gIdleCallbacks.addFunction(&onIdle, this);
	}

	~RlvCommandOptionGetPathCallback()
	{
		if (mAttachmentConnection.connected())
			mAttachmentConnection.disconnect();
		gIdleCallbacks.deleteFunction(&onIdle, this);
	}

	void onAttachment(LLViewerObject* pAttachObj, LLVOAvatarSelf::EAttachAction eAction)
	{
		if ( (LLVOAvatarSelf::ACTION_ATTACH == eAction) && (pAttachObj->getID() == mObjectId) )
		{
			uuid_vec_t idItems(1, pAttachObj->getAttachmentItemID());
			mCallback(idItems);
			delete this;
		}
	}

	static void onIdle(void* pData)
	{
		RlvCommandOptionGetPathCallback* pInstance = reinterpret_cast<RlvCommandOptionGetPathCallback*>(pData);
		if (pInstance->mExpirationTimer.getElapsedTimeF32() > 30.0f)
			delete pInstance;
	}

protected:
	LLUUID                      mObjectId;
	RlvCommandOptionGetPath::getpath_callback_t mCallback;
	boost::signals2::connection mAttachmentConnection;
	LLFrameTimer                mExpirationTimer;
};

// Checked: 2010-11-30 (RLVa-1.3.0b) | Modified: RLVa-1.3.0b
RlvCommandOptionGetPath::RlvCommandOptionGetPath(const RlvCommand& rlvCmd, getpath_callback_t cb)
	: m_fCallback(false)
{
	m_fValid = true;	// Assume the option will be a valid one until we find out otherwise

	// @getpath[:<option>]=<channel> => <option> is transformed to a list of inventory item UUIDs to get the path of
	RlvCommandOptionGeneric rlvCmdOption;
	RlvCommandOptionHelper::parseOption(rlvCmd.getOption(), rlvCmdOption);
	if (rlvCmdOption.isWearableType())			// <option> can be a clothing layer
	{
		getItemIDs(rlvCmdOption.getWearableType(), m_idItems);
	}
	else if (rlvCmdOption.isAttachmentPoint())	// ... or it can specify an attachment point
	{
		getItemIDs(rlvCmdOption.getAttachmentPoint(), m_idItems);
	}
	else if (rlvCmdOption.isUUID())				// ... or it can specify a specific attachment
	{
		const LLViewerObject* pAttachObj = gObjectList.findObject(rlvCmdOption.getUUID());
		if ( (pAttachObj) && (pAttachObj->isAttachment()) && (pAttachObj->permYouOwner()) )
			m_idItems.push_back(pAttachObj->getAttachmentItemID());
		else
			m_fValid = false;
	}
	else if (rlvCmdOption.isEmpty())			// ... or it can be empty (in which case we act on the object that issued the command)
	{
		const LLViewerObject* pObj = gObjectList.findObject(rlvCmd.getObjectID());
		if (pObj)
		{
			if (pObj->isAttachment())
				m_idItems.push_back(pObj->getAttachmentItemID());
		}
		else if (!cb.empty())
		{
			new RlvCommandOptionGetPathCallback(rlvCmd.getObjectID(), cb);
			m_fCallback = true;
			return;
		}
	}
	else										// ... but anything else isn't a valid option
	{
		m_fValid = false;
		return;
	}

	if (!cb.empty())
	{
		cb(getItemIDs());
	}
}

// Checked: 2013-10-12 (RLVa-1.4.9)
bool RlvCommandOptionGetPath::getItemIDs(const LLViewerJointAttachment* pAttachPt, uuid_vec_t& idItems)
{
	uuid_vec_t::size_type cntItemsPrev = idItems.size();

	LLInventoryModel::cat_array_t folders; LLInventoryModel::item_array_t items;
	RlvFindAttachmentsOnPoint f(pAttachPt);
	gInventory.collectDescendentsIf(LLAppearanceMgr::instance().getCOF(), folders, items, false, f);
	for (LLInventoryModel::item_array_t::const_iterator itItem = items.begin(); itItem != items.end(); ++itItem)
	{
		const LLViewerInventoryItem* pItem = *itItem;
		if (pItem)
			idItems.push_back(pItem->getLinkedUUID());
	}

	return (cntItemsPrev != idItems.size());
}

// Checked: 2013-10-12 (RLVa-1.4.9)
bool RlvCommandOptionGetPath::getItemIDs(LLWearableType::EType wtType, uuid_vec_t& idItems)
{
	uuid_vec_t::size_type cntItemsPrev = idItems.size();

	LLInventoryModel::cat_array_t folders; LLInventoryModel::item_array_t items;
	LLFindWearablesOfType f(wtType);
	gInventory.collectDescendentsIf(LLAppearanceMgr::instance().getCOF(), folders, items, false, f);
	for (LLInventoryModel::item_array_t::const_iterator itItem = items.begin(); itItem != items.end(); ++itItem)
	{
		const LLViewerInventoryItem* pItem = *itItem;
		if (pItem)
			idItems.push_back(pItem->getLinkedUUID());
	}

	return (cntItemsPrev != idItems.size());
}

// Checked: 2015-03-30 (RLVa-1.5.0)
RlvCommandOptionAdjustHeight::RlvCommandOptionAdjustHeight(const RlvCommand& rlvCmd)
	: m_nPelvisToFoot(0.0f), m_nPelvisToFootDeltaMult(0.0f), m_nPelvisToFootOffset(0.0f)
{
	std::vector<std::string> cmdTokens;
	boost::split(cmdTokens, rlvCmd.getOption(), boost::is_any_of(std::string(";")));
	if (1 == cmdTokens.size())
	{
		m_fValid = (LLStringUtil::convertToF32(cmdTokens[0], m_nPelvisToFootOffset));
		m_nPelvisToFootOffset /= 100;
	}
	else if ( (2 <= cmdTokens.size()) && (cmdTokens.size() <= 3) )
	{
		m_fValid = (LLStringUtil::convertToF32(cmdTokens[0], m_nPelvisToFoot)) &&
			 (LLStringUtil::convertToF32(cmdTokens[1], m_nPelvisToFootDeltaMult)) && 
			 ( (2 == cmdTokens.size()) || (LLStringUtil::convertToF32(cmdTokens[2], m_nPelvisToFootOffset)) );
	}
}

// =========================================================================
// RlvObject
//

RlvObject::RlvObject(const LLUUID& idObj) : m_idObj(idObj), m_nLookupMisses(0)
{
	LLViewerObject* pObj = gObjectList.findObject(idObj);
	m_fLookup = (NULL != pObj);
	m_idxAttachPt = (pObj) ? ATTACHMENT_ID_FROM_STATE(pObj->getState()) : 0;
	m_idRoot = (pObj) ? pObj->getRootEdit()->getID() : LLUUID::null;
}

bool RlvObject::addCommand(const RlvCommand& rlvCmd)
{
	RLV_ASSERT(RLV_TYPE_ADD == rlvCmd.getParamType());

	// Don't add duplicate commands for this object (ie @detach=n followed by another @detach=n later on)
	for (rlv_command_list_t::iterator itCmd = m_Commands.begin(); itCmd != m_Commands.end(); ++itCmd)
	{
		if ( (itCmd->getBehaviour() == rlvCmd.getBehaviour()) && (itCmd->getOption() == rlvCmd.getOption()) && 
			 (itCmd->isStrict() == rlvCmd.isStrict() ) )
		{
			return false;
		}
	}

	// Now that we know it's not a duplicate, add it to the end of the list
	m_Commands.push_back(rlvCmd);

	return true;
}

bool RlvObject::removeCommand(const RlvCommand& rlvCmd)
{
	RLV_ASSERT(RLV_TYPE_REMOVE == rlvCmd.getParamType());

	for (rlv_command_list_t::iterator itCmd = m_Commands.begin(); itCmd != m_Commands.end(); ++itCmd)
	{
		//if (*itCmd == rlvCmd) <- commands will never be equal since one is an add and the other is a remove *rolls eyes*
		if ( (itCmd->getBehaviour() == rlvCmd.getBehaviour()) && (itCmd->getOption() == rlvCmd.getOption()) && 
			 (itCmd->isStrict() == rlvCmd.isStrict() ) )
		{
			m_Commands.erase(itCmd);
			return true;
		}
	}
	return false;	// Command was never added so nothing to remove now
}

bool RlvObject::hasBehaviour(ERlvBehaviour eBehaviour, bool fStrictOnly) const
{
	for (rlv_command_list_t::const_iterator itCmd = m_Commands.begin(); itCmd != m_Commands.end(); ++itCmd)
		if ( (itCmd->getBehaviourType() == eBehaviour) && (itCmd->getOption().empty()) && ((!fStrictOnly) || (itCmd->isStrict())) )
			return true;
	return false;
}

bool RlvObject::hasBehaviour(ERlvBehaviour eBehaviour, const std::string& strOption, bool fStrictOnly) const
{
	for (const RlvCommand& rlvCmd : m_Commands)
	{
		// The specified behaviour is contained within the current object if:
		//   - the (parsed) behaviour matches
		//   - the option matches (or we're checking for an empty option and the command was reference counted)
		//   - we're not matching on strict (or it is a strict command)
		if ( (rlvCmd.getBehaviourType() == eBehaviour) &&
			 ( (rlvCmd.getOption() == strOption) || ((strOption.empty()) && (rlvCmd.isRefCounted())) ) &&
			 ( (!fStrictOnly) ||(rlvCmd.isStrict()) ) )
		{
			return true;
		}
	}
	return false;
}

// Checked: 2009-11-27 (RLVa-1.1.0f) | Modified: RLVa-1.1.0f
std::string RlvObject::getStatusString(const std::string& strFilter, const std::string& strSeparator) const
{
	std::string strStatus, strCmd;

	for (rlv_command_list_t::const_iterator itCmd = m_Commands.begin(); itCmd != m_Commands.end(); ++itCmd)
	{
		strCmd = itCmd->asString();
		if ( (strFilter.empty()) || (std::string::npos != strCmd.find(strFilter)) )
			strStatus.append(strSeparator).append(strCmd);
	}

	return strStatus;
}

// ============================================================================
// RlvForceWear
//

// Checked: 2010-04-05 (RLVa-1.2.0d) | Modified: RLVa-1.2.0d
bool RlvForceWear::isWearingItem(const LLInventoryItem* pItem)
{
	if (pItem)
	{
		switch (pItem->getActualType())
		{
			case LLAssetType::AT_BODYPART:
			case LLAssetType::AT_CLOTHING:
				return gAgentWearables.isWearingItem(pItem->getUUID());
			case LLAssetType::AT_OBJECT:
				return (isAgentAvatarValid()) && (gAgentAvatarp->isWearingAttachment(pItem->getUUID()));
			case LLAssetType::AT_GESTURE:
				return LLGestureMgr::instance().isGestureActive(pItem->getUUID());
			case LLAssetType::AT_LINK:
				return isWearingItem(gInventory.getItem(pItem->getLinkedUUID()));
			default:
				break;
		}
	}
	return false;
}

// Checked: 2010-03-21 (RLVa-1.2.0a) | Modified: RLVa-1.2.0a
void RlvForceWear::forceFolder(const LLViewerInventoryCategory* pFolder, EWearAction eAction, EWearFlags eFlags)
{
	// [See LLWearableBridge::wearOnAvatar(): don't wear anything until initial wearables are loaded, can destroy clothing items]
	if (!gAgentWearables.areWearablesLoaded())
	{
		LLNotificationsUtil::add("CanNotChangeAppearanceUntilLoaded");
		return;
	}
	if (!isAgentAvatarValid())
		return;

	// Grab a list of all the items we'll be wearing/attaching
	LLInventoryModel::cat_array_t folders; LLInventoryModel::item_array_t items;
	RlvWearableItemCollector f(pFolder, eAction, eFlags);
	gInventory.collectDescendentsIf(pFolder->getUUID(), folders, items, FALSE, f, true);

	// TRUE if we've already encountered this LLWearableType::EType (used only on wear actions and only for AT_CLOTHING)
	bool fSeenWType[LLWearableType::WT_COUNT] = { false };

	EWearAction eCurAction = eAction;
	for (S32 idxItem = 0, cntItem = items.size(); idxItem < cntItem; idxItem++)
	{
		LLViewerInventoryItem* pRlvItem = items.at(idxItem);
		LLViewerInventoryItem* pItem = (LLAssetType::AT_LINK == pRlvItem->getActualType()) ? pRlvItem->getLinkedItem() : pRlvItem;

		// If it's wearable it should be worn on detach
//		if ( (ACTION_DETACH == eAction) && (isWearableItem(pItem)) && (!isWearingItem(pItem)) )
//			continue;

		// Each folder can specify its own EWearAction override
		if (isWearAction(eAction))
			eCurAction = f.getWearAction(pRlvItem->getParentUUID());
		else
			eCurAction = eAction;

		//  NOTES: * if there are composite items then RlvWearableItemCollector made sure they can be worn (or taken off depending)
		//         * some scripts issue @remattach=force,attach:worn-items=force so we need to attach items even if they're currently worn
		switch (pItem->getType())
		{
			case LLAssetType::AT_BODYPART:
				RLV_ASSERT(isWearAction(eAction));	// RlvWearableItemCollector shouldn't be supplying us with body parts on detach
			case LLAssetType::AT_CLOTHING:
				if (isWearAction(eAction))
				{
					// The first time we encounter any given clothing type we use 'eCurAction' (replace or add)
					// The second time we encounter a given clothing type we'll always add (rather than replace the previous iteration)
					eCurAction = (!fSeenWType[pItem->getWearableType()]) ? eCurAction : ACTION_WEAR_ADD;

					ERlvWearMask eWearMask = gRlvWearableLocks.canWear(pRlvItem);
					if ( ((ACTION_WEAR_REPLACE == eCurAction) && (eWearMask & RLV_WEAR_REPLACE)) ||
						 ((ACTION_WEAR_ADD == eCurAction) && (eWearMask & RLV_WEAR_ADD)) )
					{
						// The check for whether we're replacing a currently worn composite item happens in onWearableArrived()
						if (!isAddWearable(pItem))
							addWearable(pRlvItem, eCurAction);
						fSeenWType[pItem->getWearableType()] = true;
					}
				}
				else
				{
					const LLViewerWearable* pWearable = gAgentWearables.getWearableFromItemID(pItem->getUUID());
					if ( (pWearable) && (isForceRemovable(pWearable, false)) )
						remWearable(pWearable);
				}
				break;

			case LLAssetType::AT_OBJECT:
				if (isWearAction(eAction))
				{
					ERlvWearMask eWearMask = gRlvAttachmentLocks.canAttach(pRlvItem);
					if ( ((ACTION_WEAR_REPLACE == eCurAction) && (eWearMask & RLV_WEAR_REPLACE)) ||
						 ((ACTION_WEAR_ADD == eCurAction) && (eWearMask & RLV_WEAR_ADD)) )
					{
						if (!isAddAttachment(pRlvItem))
						{
							#ifdef RLV_EXPERIMENTAL_COMPOSITEFOLDERS
							// We still need to check whether we're about to replace a currently worn composite item
							// (which we're not if we're just reattaching an attachment we're already wearing)
							LLViewerInventoryCategory* pCompositeFolder = NULL;
							if ( (pAttachPt->getObject()) && (RlvSettings::getEnableComposites()) && 
								 (pAttachPt->getItemID() != pItem->getUUID()) &&
								 (gRlvHandler.getCompositeInfo(pAttachPt->getItemID(), NULL, &pCompositeFolder)) )
							{
								// If we can't take off the composite folder this item would replace then don't allow it to get attached
								if (gRlvHandler.canTakeOffComposite(pCompositeFolder))
								{
									forceFolder(pCompositeFolder, ACTION_DETACH, FLAG_DEFAULT);
									addAttachment(pRlvItem);
								}
							}
							else
							#endif // RLV_EXPERIMENTAL_COMPOSITEFOLDERS
							{
								addAttachment(pRlvItem, eCurAction);
							}
						}
					}
				}
				else
				{
					const LLViewerObject* pAttachObj = gAgentAvatarp->getWornAttachment(pItem->getUUID());
					if ( (pAttachObj) && (isForceDetachable(pAttachObj, false)) )
						remAttachment(pAttachObj);
				}
				break;

			case LLAssetType::AT_GESTURE:
				if (isWearAction(eAction))
				{
					if (std::find_if(m_addGestures.begin(), m_addGestures.end(), RlvPredIsEqualOrLinkedItem(pRlvItem)) == m_addGestures.end())
						m_addGestures.push_back(pRlvItem);
				}
				else
				{
					if (std::find_if(m_remGestures.begin(), m_remGestures.end(), RlvPredIsEqualOrLinkedItem(pRlvItem)) == m_remGestures.end())
						m_remGestures.push_back(pRlvItem);
				}
				break;

			default:
				break;
		}
	}
}

// Checked: 2010-03-19 (RLVa-1.2.0a) | Modified: RLVa-1.2.0a
bool RlvForceWear::isForceDetachable(const LLViewerObject* pAttachObj, bool fCheckComposite /*=true*/, const LLUUID& idExcept /*=LLUUID::null*/)
{
	// Attachment can be detached by an RLV command if:
	//   - it's not "remove locked" by anything (or anything except the object specified by pExceptObj)
	//   - it's strippable
	//   - composite folders are disabled *or* it isn't part of a composite folder that has at least one item locked
	#ifdef RLV_EXPERIMENTAL_COMPOSITEFOLDERS
	LLViewerInventoryCategory* pFolder = NULL;
	#endif // RLV_EXPERIMENTAL_COMPOSITEFOLDERS
	return 
	  (
		(pAttachObj) && (pAttachObj->isAttachment())
		&& ( (idExcept.isNull()) ? (!gRlvAttachmentLocks.isLockedAttachment(pAttachObj))
		                         : (!gRlvAttachmentLocks.isLockedAttachmentExcept(pAttachObj, idExcept)) )
		&& (isStrippable(pAttachObj->getAttachmentItemID()))
		#ifdef RLV_EXPERIMENTAL_COMPOSITEFOLDERS
		&& ( (!fCheckComposite) || (!RlvSettings::getEnableComposites()) || 
	         (!gRlvHandler.getCompositeInfo(pAttachPt->getItemID(), NULL, &pFolder)) || (gRlvHandler.canTakeOffComposite(pFolder)) )
		#endif // RLV_EXPERIMENTAL_COMPOSITEFOLDERS
	  );
}

// Checked: 2010-03-19 (RLVa-1.2.0a) | Added: RLVa-1.2.0a
bool RlvForceWear::isForceDetachable(const LLViewerJointAttachment* pAttachPt, bool fCheckComposite /*=true*/, const LLUUID& idExcept /*=LLUUID::null*/)
{
	// Attachment point can be detached by an RLV command if there's at least one attachment that can be removed
	for (LLViewerJointAttachment::attachedobjs_vec_t::const_iterator itAttachObj = pAttachPt->mAttachedObjects.begin();
			itAttachObj != pAttachPt->mAttachedObjects.end(); ++itAttachObj)
	{
		if (isForceDetachable(*itAttachObj, fCheckComposite, idExcept))
			return true;
	}
	return false;
}

// Checked: 2010-03-19 (RLVa-1.2.0a) | Added: RLVa-1.1.0i
void RlvForceWear::forceDetach(const LLViewerObject* pAttachObj)
{
	// Sanity check - no need to process duplicate removes
	if ( (!pAttachObj) || (isRemAttachment(pAttachObj)) )
		return;

	if (isForceDetachable(pAttachObj))
	{
		#ifdef RLV_EXPERIMENTAL_COMPOSITEFOLDERS
		LLViewerInventoryCategory* pFolder = NULL;
		if ( (RlvSettings::getEnableComposites()) && 
			 (gRlvHandler.getCompositeInfo(pAttachPt->getItemID(), NULL, &pFolder)) )
		{
			// Attachment belongs to a composite folder so detach the entire folder (if we can take it off)
			if (gRlvHandler.canTakeOffComposite(pFolder))
				forceFolder(pFolder, ACTION_DETACH, FLAG_DEFAULT);
		}
		else
		#endif // RLV_EXPERIMENTAL_COMPOSITEFOLDERS
		{
			remAttachment(pAttachObj);
		}
	}
}

// Checked: 2010-03-19 (RLVa-1.2.0a) | Added: RLVa-1.2.0a
void RlvForceWear::forceDetach(const LLViewerJointAttachment* pAttachPt)
{
	for (LLViewerJointAttachment::attachedobjs_vec_t::const_iterator itAttachObj = pAttachPt->mAttachedObjects.begin();
			itAttachObj != pAttachPt->mAttachedObjects.end(); ++itAttachObj)
	{
		forceDetach(*itAttachObj);
	}
}

// Checked: 2010-03-19 (RLVa-1.2.0c) | Modified: RLVa-1.2.0a
bool RlvForceWear::isForceRemovable(const LLViewerWearable* pWearable, bool fCheckComposite /*=true*/, const LLUUID& idExcept /*=LLUUID::null*/)
{
	// Wearable can be removed by an RLV command if:
	//   - its asset type is AT_CLOTHING
	//   - it's not "remove locked" by anything (or anything except the object specified by idExcept)
	//   - it's strippable
	//   - composite folders are disabled *or* it isn't part of a composite folder that has at least one item locked
	#ifdef RLV_EXPERIMENTAL_COMPOSITEFOLDERS
	LLViewerInventoryCategory* pFolder = NULL;
	#endif // RLV_EXPERIMENTAL_COMPOSITEFOLDERS
	return 
	  (
		(pWearable) && (LLAssetType::AT_CLOTHING == pWearable->getAssetType()) 
		&& ( (idExcept.isNull()) ? !gRlvWearableLocks.isLockedWearable(pWearable)
		                         : !gRlvWearableLocks.isLockedWearableExcept(pWearable, idExcept) )
		&& (isStrippable(pWearable->getItemID()))
		#ifdef RLV_EXPERIMENTAL_COMPOSITEFOLDERS
		&& ( (!fCheckComposite) || (!RlvSettings::getEnableComposites()) || 
		     (!gRlvHandler.getCompositeInfo(pWearable->getItemID(), NULL, &pFolder)) || (gRlvHandler.canTakeOffComposite(pFolder)) )
		#endif // RLV_EXPERIMENTAL_COMPOSITEFOLDERS
	  );
}

// Checked: 2010-03-19 (RLVa-1.2.0a) | Added: RLVa-1.2.0a
bool RlvForceWear::isForceRemovable(LLWearableType::EType wtType, bool fCheckComposite /*=true*/, const LLUUID& idExcept /*=LLUUID::null*/)
{
	// Wearable type can be removed by an RLV command if there's at least one currently worn wearable that can be removed
	for (U32 idxWearable = 0, cntWearable = gAgentWearables.getWearableCount(wtType); idxWearable < cntWearable; idxWearable++)
		if (isForceRemovable(gAgentWearables.getViewerWearable(wtType, idxWearable), fCheckComposite, idExcept))
			return true;
	return false;
}

// Checked: 2010-03-19 (RLVa-1.2.0a) | Modified: RLVa-1.2.0a
void RlvForceWear::forceRemove(const LLViewerWearable* pWearable)
{
	// Sanity check - no need to process duplicate removes
	if ( (!pWearable) || (isRemWearable(pWearable)) )
		return;

	if (isForceRemovable(pWearable))
	{
		#ifdef RLV_EXPERIMENTAL_COMPOSITEFOLDERS
		LLViewerInventoryCategory* pFolder = NULL;
		if ( (RlvSettings::getEnableComposites()) && 
			 (gRlvHandler.getCompositeInfo(gAgent.getWearableItem(wtType), NULL, &pFolder)) )
		{
			// Wearable belongs to a composite folder so detach the entire folder (if we can take it off)
			if (gRlvHandler.canTakeOffComposite(pFolder))
				forceFolder(pFolder, ACTION_DETACH, FLAG_DEFAULT);
		}
		else
		#endif // RLV_EXPERIMENTAL_COMPOSITEFOLDERS
		{
			remWearable(pWearable);
		}
	}
}

// Checked: 2010-03-19 (RLVa-1.2.0a) | Added: RLVa-1.2.0a
void RlvForceWear::forceRemove(LLWearableType::EType wtType)
{
	for (U32 idxWearable = 0, cntWearable = gAgentWearables.getWearableCount(wtType); idxWearable < cntWearable; idxWearable++)
		forceRemove(gAgentWearables.getViewerWearable(wtType, idxWearable));
}

// Checked: 2010-03-19 (RLVa-1.2.0c) | Modified: RLVa-1.2.0a
bool RlvForceWear::isStrippable(const LLInventoryItem* pItem)
{
	// An item is exempt from @detach or @remoutfit if:
	//   - its name contains "nostrip" (anywhere in the name)
	//   - its parent folder contains "nostrip" (anywhere in the name)
	if (pItem)
	{
		// If the item is an inventory link then we first examine its target before examining the link itself (and never its name)
		if (LLAssetType::AT_LINK == pItem->getActualType())
		{
			if (!isStrippable(pItem->getLinkedUUID()))
				return false;
		}
		else
		{
			if (std::string::npos != pItem->getName().find(RLV_FOLDER_FLAG_NOSTRIP))
				return false;
		}

		LLViewerInventoryCategory* pFolder = gInventory.getCategory(pItem->getParentUUID());
		while (pFolder)
		{
			if (std::string::npos != pFolder->getName().find(RLV_FOLDER_FLAG_NOSTRIP))
				return false;
			// If the item's parent is a folded folder then we need to check its parent as well
			if ( (gInventory.getRootFolderID() != pFolder->getParentUUID()) && (RlvInventory::isFoldedFolder(pFolder, true)) )
				pFolder = gInventory.getCategory(pFolder->getParentUUID());
			else
				pFolder = NULL;
		}
	}
	return true;
}

// Checked: 2010-08-30 (RLVa-1.2.1c) | Modified: RLVa-1.2.1c
void RlvForceWear::addAttachment(const LLViewerInventoryItem* pItem, EWearAction eAction)
{
	// Remove it from 'm_remAttachments' if it's queued for detaching
	const LLViewerObject* pAttachObj = (isAgentAvatarValid()) ? gAgentAvatarp->getWornAttachment(pItem->getLinkedUUID()) : NULL;
	if ( (pAttachObj) && (isRemAttachment(pAttachObj)) )
		m_remAttachments.erase(std::remove(m_remAttachments.begin(), m_remAttachments.end(), pAttachObj), m_remAttachments.end());

	S32 idxAttachPt = RlvAttachPtLookup::getAttachPointIndex(pItem, true);
	if (ACTION_WEAR_ADD == eAction)
	{
		// Insert it at the back if it's not already there
		idxAttachPt |= ATTACHMENT_ADD;
		if (!isAddAttachment(pItem))
		{
			addattachments_map_t::iterator itAddAttachments = m_addAttachments.find(idxAttachPt);
			if (itAddAttachments == m_addAttachments.end())
			{
				m_addAttachments.insert(addattachment_pair_t(idxAttachPt, LLInventoryModel::item_array_t()));
				itAddAttachments = m_addAttachments.find(idxAttachPt);
			}
			itAddAttachments->second.push_back((LLViewerInventoryItem*)pItem);
		}
	}
	else if (ACTION_WEAR_REPLACE == eAction)
	{
		// Replace all pending attachments on this attachment point with the specified item (don't clear if it's the default attach point)
		addattachments_map_t::iterator itAddAttachments = m_addAttachments.find(idxAttachPt | ATTACHMENT_ADD);
		if ( (0 != idxAttachPt) && (itAddAttachments != m_addAttachments.end()) )
			itAddAttachments->second.clear();

		itAddAttachments = m_addAttachments.find(idxAttachPt);
		if (itAddAttachments == m_addAttachments.end())
		{
			m_addAttachments.insert(addattachment_pair_t(idxAttachPt, LLInventoryModel::item_array_t()));
			itAddAttachments = m_addAttachments.find(idxAttachPt);
		}

		if (0 != idxAttachPt)
			itAddAttachments->second.clear();
		itAddAttachments->second.push_back((LLViewerInventoryItem*)pItem);
	}
}

// Checked: 2010-08-30 (RLVa-1.2.1c) | Modified: RLVa-1.2.1c
void RlvForceWear::remAttachment(const LLViewerObject* pAttachObj)
{
	// Remove it from 'm_addAttachments' if it's queued for attaching
	const LLViewerInventoryItem* pItem = (pAttachObj->isAttachment()) ? gInventory.getItem(pAttachObj->getAttachmentItemID()) : NULL;
	if (pItem)
	{
		addattachments_map_t::iterator itAddAttachments = m_addAttachments.begin();
		while (itAddAttachments != m_addAttachments.end())
		{
			LLInventoryModel::item_array_t& wearItems = itAddAttachments->second;
			if (std::find_if(wearItems.begin(), wearItems.end(), RlvPredIsEqualOrLinkedItem(pItem)) != wearItems.end())
				wearItems.erase(std::remove_if(wearItems.begin(), wearItems.end(), RlvPredIsEqualOrLinkedItem(pItem)), wearItems.end());

			if (wearItems.empty())
				m_addAttachments.erase(itAddAttachments++);
			else
				++itAddAttachments;
		}
	}

	// Add it to 'm_remAttachments' if it's not already there
	if (!isRemAttachment(pAttachObj))
	{
		m_remAttachments.push_back(const_cast<LLViewerObject*>(pAttachObj));
	}
}

// Checked: 2010-08-30 (RLVa-1.2.1c) | Modified: RLVa-1.2.1c
void RlvForceWear::addWearable(const LLViewerInventoryItem* pItem, EWearAction eAction)
{
	const LLViewerWearable* pWearable = gAgentWearables.getWearableFromItemID(pItem->getLinkedUUID());
	// When replacing remove all currently worn wearables of this type *unless* the item is currently worn
	if ( (ACTION_WEAR_REPLACE == eAction) && (!pWearable) )
		forceRemove(pItem->getWearableType());
	// Remove it from 'm_remWearables' if it's pending removal
	if ( (pWearable) && (isRemWearable(pWearable)) )
		m_remWearables.erase(std::remove(m_remWearables.begin(), m_remWearables.end(), pWearable), m_remWearables.end());

	addwearables_map_t::iterator itAddWearables = m_addWearables.find(pItem->getWearableType());
	if (itAddWearables == m_addWearables.end())
	{
		m_addWearables.insert(addwearable_pair_t(pItem->getWearableType(), LLInventoryModel::item_array_t()));
		itAddWearables = m_addWearables.find(pItem->getWearableType());
	}

	if (ACTION_WEAR_ADD == eAction)				// Add it at the back if it's not already there
	{
		if (!isAddWearable(pItem))
			itAddWearables->second.push_back((LLViewerInventoryItem*)pItem);
	}
	else if (ACTION_WEAR_REPLACE == eAction)	// Replace all pending wearables of this type with the specified item
	{
		itAddWearables->second.clear();
		itAddWearables->second.push_back((LLViewerInventoryItem*)pItem);
	}
}

// Checked: 2010-08-30 (RLVa-1.2.1c) | Modified: RLVa-1.2.1c
void RlvForceWear::remWearable(const LLViewerWearable* pWearable)
{
	// Remove it from 'm_addWearables' if it's queued for wearing
	const LLViewerInventoryItem* pItem = gInventory.getItem(pWearable->getItemID());
	if ( (pItem) && (isAddWearable(pItem)) )
	{
		addwearables_map_t::iterator itAddWearables = m_addWearables.find(pItem->getWearableType());

		LLInventoryModel::item_array_t& wearItems = itAddWearables->second;
		wearItems.erase(std::remove_if(wearItems.begin(), wearItems.end(), RlvPredIsEqualOrLinkedItem(pItem)), wearItems.end());
		if (wearItems.empty())
			m_addWearables.erase(itAddWearables);
	}

	// Add it to 'm_remWearables' if it's not already there
	if (!isRemWearable(pWearable))
		m_remWearables.push_back(pWearable);
}

// Checked: 2015-05-05 (RLVa-1.4.12)
void RlvForceWear::updatePendingAttachments()
{
	if (RlvForceWear::instanceExists())
	{
		RlvForceWear* pThis = RlvForceWear::getInstance();
		for (const pendingattachments_map_t::value_type& itAttach : pThis->m_pendingAttachments)
			LLAttachmentsMgr::instance().addAttachmentRequest(itAttach.first, itAttach.second & ~ATTACHMENT_ADD, itAttach.second & ATTACHMENT_ADD);
		pThis->m_pendingAttachments.clear();
	}
}

// Checked: 2015-05-05 (RLVa-1.4.12)
void RlvForceWear::addPendingAttachment(const LLUUID& idItem, U8 idxPoint)
{
	auto itAttach = m_pendingAttachments.find(idItem);
	if (m_pendingAttachments.end() == itAttach)
		m_pendingAttachments.insert(std::make_pair(idItem, idxPoint));
	else
		itAttach->second = idxPoint;
}

// Checked: 2015-05-05 (RLVa-1.4.12)
void RlvForceWear::remPendingAttachment(const LLUUID& idItem)
{
	m_pendingAttachments.erase(idItem);
}

// Checked: 2015-05-05 (RLVa-1.4.12)
void RlvForceWear::done()
{
	// Sanity check - don't go through all the motions below only to find out there's nothing to actually do
	if ( (m_remWearables.empty()) && (m_remAttachments.empty()) && (m_remGestures.empty()) &&
		 (m_addWearables.empty()) && (m_addAttachments.empty()) && (m_addGestures.empty()) )
	{
		return;
	}

	//
	// Process removals
	//

	uuid_vec_t remItems;

	// Wearables
	if (m_remWearables.size())
	{
		for (const LLViewerWearable* pWearable : m_remWearables)
			remItems.push_back(pWearable->getItemID());
		m_remWearables.clear();
	}

	// Gestures
	if (m_remGestures.size())
	{
		// NOTE: LLGestureMgr::deactivateGesture() will call LLAppearanceMgr::removeCOFItemLinks() for us and supply its own callback
		for (const LLViewerInventoryItem* pItem : m_remGestures)
			LLGestureMgr::instance().deactivateGesture(pItem->getUUID());
		m_remGestures.clear();
	}

	// Attachments
	if (m_remAttachments.size())
	{
		LLAgentWearables::userRemoveMultipleAttachments(m_remAttachments);
		for (const LLViewerObject* pAttachObj : m_remAttachments)
			remItems.push_back(pAttachObj->getAttachmentItemID());
		m_remAttachments.clear();
	}

	//
	// Process additions
	//

	// Wearables need to be split into AT_BODYPART and AT_CLOTHING for COF
	LLInventoryModel::item_array_t addBodyParts, addClothing;
	for (addwearables_map_t::const_iterator itAddWearables = m_addWearables.cbegin(); itAddWearables != m_addWearables.cend(); ++itAddWearables)
	{
		// NOTE: LLAppearanceMgr will filter our duplicates so no need for us to check here
		for (LLViewerInventoryItem* pItem : itAddWearables->second)
		{
			if (LLAssetType::AT_BODYPART == pItem->getType())
				addBodyParts.push_back(pItem);
			else
				addClothing.push_back(pItem);
		}
	}
	m_addWearables.clear();

	// Until LL provides a way for updateCOF to selectively attach add/replace we have to deal with attachments ourselves
	for (addattachments_map_t::const_iterator itAddAttachments = m_addAttachments.cbegin(); itAddAttachments != m_addAttachments.cend(); ++itAddAttachments)
	{
		for (const LLViewerInventoryItem* pItem : itAddAttachments->second)
			addPendingAttachment(pItem->getLinkedUUID(), itAddAttachments->first);
	}
	m_addAttachments.clear();

	//
	// Tie it all together
	//

	//          |    Wearables    |   Attachments    |   Gestures      |
	//          |======================================================|
	// Add    : | LLAppearanceMgr | <custom>         | LLAppearanceMgr |
	// Remove : | LLAppearanceMgr | LLAppearanceMgr  | LLGestureMgr    |
	LLPointer<LLInventoryCallback> cb = new LLUpdateAppearanceOnDestroy(false, false, boost::bind(RlvForceWear::updatePendingAttachments));

	if (!remItems.empty())
	{
		LLAppearanceMgr::instance().removeItemsFromAvatar(remItems, cb, true);
	}

	if ( (addBodyParts.empty()) && (!addClothing.empty()) && (m_addGestures.empty()) )
	{
		// Clothing items only
		uuid_vec_t idClothing;
		for (const LLViewerInventoryItem* pItem : addClothing)
			idClothing.push_back(pItem->getUUID());
		LLAppearanceMgr::instance().wearItemsOnAvatar(idClothing, false, false, cb);
	}
	else if ( (!addBodyParts.empty()) || (!addClothing.empty()) || (!m_addGestures.empty()) )
	{
		// Mixture of body parts, clothing and/or gestures
		LLInventoryModel::item_array_t addAttachments;
		LLAppearanceMgr::instance().updateCOF(addBodyParts, addClothing, addAttachments, m_addGestures, true, LLUUID::null, cb);

		m_addGestures.clear();
	}

	// Make sure there are no leftovers for the next cycle
	RLV_ASSERT( (m_remWearables.empty()) && (m_remAttachments.empty()) && (m_remGestures.empty()) );
	RLV_ASSERT( (m_addWearables.empty()) && (m_addAttachments.empty()) && (m_addGestures.empty()) );
}

// Checked: 2010-02-17 (RLVa-1.1.0o) | Modified: RLVa-1.1.0o
/*
void RlvForceWear::onWearableArrived(LLWearable* pWearable, void* pParam)
{
	#ifdef RLV_EXPERIMENTAL_COMPOSITEFOLDERS
	// If this wearable will end up replacing a currently worn one that belongs to a composite folder then we need to detach the composite
	LLViewerInventoryCategory* pFolder = NULL;
	if ( (RlvSettings::getEnableComposites()) && (pWearable) && (gAgent.getWearable(pWearable->getType())) )
	{
		// If we're just rewearing the same item we're already wearing then we're not replacing a composite folder
		LLWearableHoldingPattern* pWearData = (LLWearableHoldingPattern*)pParam; LLUUID idItem;
		for (LLWearableHoldingPattern::found_list_t::const_iterator itWearable = pWearData->mFoundList.begin();
				itWearable != pWearData->mFoundList.end(); ++itWearable)
		{
			LLFoundData* pFound = *itWearable;
			if (pWearable->getID() == pFound->mAssetID)
			{
				idItem = pFound->mItemID;
				break;
			}
		}
		if ( (idItem.notNull()) && (idItem != gAgent.getWearableItem(pWearable->getType())) && 
			 (gRlvHandler.getCompositeInfo(gAgent.getWearableItem(pWearable->getType()), NULL, &pFolder)) )
		{
			RlvForceWear rlvWear;
			rlvWear.forceFolder(pFolder, ACTION_DETACH, FLAG_DEFAULT);
			rlvWear.done();
		}
	}
	#endif // RLV_EXPERIMENTAL_COMPOSITEFOLDERS

	wear_inventory_category_on_avatar_loop(pWearable, pParam);
}
*/

// ============================================================================
// RlvBehaviourNotifyObserver
//

// Checked: 2010-03-03 (RLVa-1.2.0a) | Added: RLVa-1.2.0a
RlvBehaviourNotifyHandler::RlvBehaviourNotifyHandler()
{
	// NOTE: the reason we use rlv_command_signal_t instead of the better-suited rlv_behaviour_signal_t is because
	//       RLV will notify scripts about "invalid" commands so we need to as well
	m_ConnCommand = gRlvHandler.setCommandCallback(boost::bind(&RlvBehaviourNotifyHandler::onCommand, this, _1, _2, _3));
}

// Checked: 2010-03-03 (RLVa-1.2.0a) | Modified: RLVa-1.2.0a
void RlvBehaviourNotifyHandler::onCommand(const RlvCommand& rlvCmd, ERlvCmdRet eRet, bool fInternal)
{
	if (fInternal)
		return;

	switch (rlvCmd.getParamType())
	{
		case RLV_TYPE_ADD:
		case RLV_TYPE_REMOVE:
			sendNotification(rlvCmd.asString(), "=" + rlvCmd.getParam());
			break;
		case RLV_TYPE_CLEAR:
			sendNotification(rlvCmd.asString());
			break;
		default:
			break;
	}
}

// Checked: 2011-03-31 (RLVa-1.3.0f) | Modify: RLVa-1.3.0f
void RlvBehaviourNotifyHandler::sendNotification(const std::string& strText, const std::string& strSuffix)
{
	if (instanceExists())
	{
		RlvBehaviourNotifyHandler* pThis = getInstance();

		// NOTE: notifications have two parts (which are concatenated without token) where only the first part is subject to the filter
		for (std::multimap<LLUUID, notifyData>::const_iterator itNotify = pThis->m_Notifications.begin(); 
				itNotify != pThis->m_Notifications.end(); ++itNotify)
		{
			if ( (itNotify->second.strFilter.empty()) || (boost::icontains(strText, itNotify->second.strFilter)) )
				RlvUtil::sendChatReply(itNotify->second.nChannel, "/" + strText + strSuffix);
		}
	}
}

// Checked: 2011-03-31 (RLVa-1.3.0f) | Added: RLVa-1.3.0f
void RlvBehaviourNotifyHandler::onWear(LLWearableType::EType eType, bool fAllowed)
{
	sendNotification(llformat("worn %s %s", (fAllowed) ? "legally" : "illegally", LLWearableType::getTypeName(eType).c_str()));
}

// Checked: 2011-03-31 (RLVa-1.3.0f) | Added: RLVa-1.3.0f
void RlvBehaviourNotifyHandler::onTakeOff(LLWearableType::EType eType, bool fAllowed)
{
	sendNotification(llformat("unworn %s %s", (fAllowed) ? "legally" : "illegally", LLWearableType::getTypeName(eType).c_str()));
}

// Checked: 2011-03-31 (RLVa-1.3.0f) | Added: RLVa-1.3.0f
void RlvBehaviourNotifyHandler::onAttach(const LLViewerJointAttachment* pAttachPt, bool fAllowed)
{
	sendNotification(llformat("attached %s %s", (fAllowed) ? "legally" : "illegally", pAttachPt->getName().c_str()));
}

// Checked: 2011-03-31 (RLVa-1.3.0f) | Added: RLVa-1.3.0f
void RlvBehaviourNotifyHandler::onDetach(const LLViewerJointAttachment* pAttachPt, bool fAllowed)
{
	sendNotification(llformat("detached %s %s", (fAllowed) ? "legally" : "illegally", pAttachPt->getName().c_str()));
}

// Checked: 2011-03-31 (RLVa-1.3.0f) | Added: RLVa-1.3.0f
void RlvBehaviourNotifyHandler::onReattach(const LLViewerJointAttachment* pAttachPt, bool fAllowed)
{
	sendNotification(llformat("reattached %s %s", (fAllowed) ? "legally" : "illegally", pAttachPt->getName().c_str()));
}

// =========================================================================
// Various helper classes/timers/functors
//

// Checked: 2010-03-13 (RLVa-1.2.0a) | Modified: RLVa-1.2.0a
BOOL RlvGCTimer::tick()
{
	bool fContinue = gRlvHandler.onGC();
	if (!fContinue)
		gRlvHandler.m_pGCTimer = NULL;
	return !fContinue;
}

// ============================================================================
// Various helper functions
//

// ============================================================================
// Attachment group helper functions
//

// Has to match the order of ERlvAttachGroupType
const std::string cstrAttachGroups[RLV_ATTACHGROUP_COUNT] = { "head", "torso", "arms", "legs", "hud" };

// Checked: 2009-10-19 (RLVa-1.1.0e) | Added: RLVa-1.1.0e
ERlvAttachGroupType rlvAttachGroupFromIndex(S32 idxGroup)
{
	switch (idxGroup)
	{
		case 0: // Right Hand
		case 1: // Right Arm
		case 3: // Left Arm
		case 4: // Left Hand
			return RLV_ATTACHGROUP_ARMS;
		case 2: // Head
			return RLV_ATTACHGROUP_HEAD;
		case 5: // Left Leg
		case 7: // Right Leg
			return RLV_ATTACHGROUP_LEGS;
		case 6: // Torso
			return RLV_ATTACHGROUP_TORSO;
		case 8: // HUD
			return RLV_ATTACHGROUP_HUD;
		default:
			return RLV_ATTACHGROUP_INVALID;
	}
}

// Checked: 2009-10-19 (RLVa-1.1.0e) | Added: RLVa-1.1.0e
ERlvAttachGroupType rlvAttachGroupFromString(const std::string& strGroup)
{
	for (int idx = 0; idx < RLV_ATTACHGROUP_COUNT; idx++)
		if (cstrAttachGroups[idx] == strGroup)
			return (ERlvAttachGroupType)idx;
	return RLV_ATTACHGROUP_INVALID;
}

// =========================================================================
// String helper functions
//

// Checked: 2009-07-29 (RLVa-1.0.1b) | Added: RLVa-1.0.1b
std::string rlvGetFirstParenthesisedText(const std::string& strText, std::string::size_type* pidxMatch /*=NULL*/)
{
	if (pidxMatch)
		*pidxMatch = std::string::npos;	// Assume we won't find anything

	std::string::size_type idxIt, idxStart; int cntLevel = 1;
	if ((idxStart = strText.find_first_of('(')) == std::string::npos)
		return std::string();

	const char* pstrText = strText.c_str(); idxIt = idxStart;
	while ( (cntLevel > 0) && (++idxIt < strText.length()) )
	{
		if ('(' == pstrText[idxIt])
			cntLevel++;
		else if (')' == pstrText[idxIt])
			cntLevel--;
	}

	if (idxIt < strText.length())
	{
		if (pidxMatch)
			*pidxMatch = idxStart;	// Return the character index of the starting '('
		return strText.substr(idxStart + 1, idxIt - idxStart - 1);
	}
	return std::string();
}

// Checked: 2009-07-29 (RLVa-1.0.1b) | Added: RLVa-1.0.1b
std::string rlvGetLastParenthesisedText(const std::string& strText, std::string::size_type* pidxStart /*=NULL*/)
{
	if (pidxStart)
		*pidxStart = std::string::npos;	// Assume we won't find anything

	// Extracts the last - matched - parenthesised text from the input string
	std::string::size_type idxIt, idxEnd; int cntLevel = 1;
	if ((idxEnd = strText.find_last_of(')')) == std::string::npos)
		return std::string();

	const char* pstrText = strText.c_str(); idxIt = idxEnd;
	while ( (cntLevel > 0) && (--idxIt >= 0) && (idxIt < strText.length()) )
	{
		if (')' == pstrText[idxIt])
			cntLevel++;
		else if ('(' == pstrText[idxIt])
			cntLevel--;
	}

	if ( (idxIt >= 0) && (idxIt < strText.length()) )	// NOTE: allow for std::string::size_type to be signed or unsigned
	{
		if (pidxStart)
			*pidxStart = idxIt;		// Return the character index of the starting '('
		return strText.substr(idxIt + 1, idxEnd - idxIt - 1);
	}
	return std::string();
}

// =========================================================================
