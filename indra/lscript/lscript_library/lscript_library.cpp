/** 
 * @file lscript_library.cpp
 * @brief external library interface
 *
 * $LicenseInfo:firstyear=2002&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
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


//  ##      ##    ###    ########  ##    ## #### ##    ##  ######   #### ####
//  ##  ##  ##   ## ##   ##     ## ###   ##  ##  ###   ## ##    ##  #### ####
//  ##  ##  ##  ##   ##  ##     ## ####  ##  ##  ####  ## ##        #### ####
//  ##  ##  ## ##     ## ########  ## ## ##  ##  ## ## ## ##   ####  ##   ##
//  ##  ##  ## ######### ##   ##   ##  ####  ##  ##  #### ##    ##
//  ##  ##  ## ##     ## ##    ##  ##   ###  ##  ##   ### ##    ##  #### ####
//   ###  ###  ##     ## ##     ## ##    ## #### ##    ##  ######   #### #### 
//
// When adding functions, they <b>MUST</b> be appended to the end of
// the init() method. The init() associates the name with a number,
// which is then serialized into the bytecode. Inserting a new
// function in the middle will lead to many sim crashes. Phoenix 2006-04-10.

#include "linden_common.h"

#include "lscript_library.h"

LLScriptLibrary::LLScriptLibrary()
{
	init();
}

LLScriptLibrary::~LLScriptLibrary()
{
}

void dummy_func(LLScriptLibData *retval, LLScriptLibData *args, const LLUUID &id)
{
}

void LLScriptLibrary::init()
{
	// IF YOU ADD NEW SCRIPT CALLS, YOU MUST PUT THEM AT THE END OF THIS LIST.
	// Otherwise the bytecode numbers for each call will be wrong, and all
	// existing scripts will crash.

	// energy, sleep, dummy_func, name, return type, parameters, gods-only
	addFunction(10.f, 0.f, dummy_func, "llSin", "f", "f");
	addFunction(10.f, 0.f, dummy_func, "llCos", "f", "f");
	addFunction(10.f, 0.f, dummy_func, "llTan", "f", "f");
	addFunction(10.f, 0.f, dummy_func, "llAtan2", "f", "ff");
	addFunction(10.f, 0.f, dummy_func, "llSqrt", "f", "f");
	addFunction(10.f, 0.f, dummy_func, "llPow", "f", "ff");
	addFunction(10.f, 0.f, dummy_func, "llAbs", "i", "i");
	addFunction(10.f, 0.f, dummy_func, "llFabs", "f", "f");
	addFunction(10.f, 0.f, dummy_func, "llFrand", "f", "f");
	addFunction(10.f, 0.f, dummy_func, "llFloor", "i", "f");
	addFunction(10.f, 0.f, dummy_func, "llCeil", "i", "f");
	addFunction(10.f, 0.f, dummy_func, "llRound", "i", "f");
	addFunction(10.f, 0.f, dummy_func, "llVecMag", "f", "v");
	addFunction(10.f, 0.f, dummy_func, "llVecNorm", "v", "v");
	addFunction(10.f, 0.f, dummy_func, "llVecDist", "f", "vv");
	addFunction(10.f, 0.f, dummy_func, "llRot2Euler", "v", "q");
	addFunction(10.f, 0.f, dummy_func, "llEuler2Rot", "q", "v");
	addFunction(10.f, 0.f, dummy_func, "llAxes2Rot", "q", "vvv");
	addFunction(10.f, 0.f, dummy_func, "llRot2Fwd", "v", "q");
	addFunction(10.f, 0.f, dummy_func, "llRot2Left", "v", "q");
	addFunction(10.f, 0.f, dummy_func, "llRot2Up", "v", "q");
	addFunction(10.f, 0.f, dummy_func, "llRotBetween", "q", "vv");
	addFunction(10.f, 0.f, dummy_func, "llWhisper", NULL, "is");
	addFunction(10.f, 0.f, dummy_func, "llSay", NULL, "is");
	addFunction(10.f, 0.f, dummy_func, "llShout", NULL, "is");
	addFunction(10.f, 0.f, dummy_func, "llListen", "i", "isks");
	addFunction(10.f, 0.f, dummy_func, "llListenControl", NULL, "ii");
	addFunction(10.f, 0.f, dummy_func, "llListenRemove", NULL, "i");
	addFunction(10.f, 0.f, dummy_func, "llSensor", NULL, "skiff");
	addFunction(10.f, 0.f, dummy_func, "llSensorRepeat", NULL, "skifff");
	addFunction(10.f, 0.f, dummy_func, "llSensorRemove", NULL, NULL);
	addFunction(10.f, 0.f, dummy_func, "llDetectedName", "s", "i");
	addFunction(10.f, 0.f, dummy_func, "llDetectedKey", "k", "i");
	addFunction(10.f, 0.f, dummy_func, "llDetectedOwner", "k", "i");
	addFunction(10.f, 0.f, dummy_func, "llDetectedType", "i", "i");
	addFunction(10.f, 0.f, dummy_func, "llDetectedPos", "v", "i");
	addFunction(10.f, 0.f, dummy_func, "llDetectedVel", "v", "i");
	addFunction(10.f, 0.f, dummy_func, "llDetectedGrab", "v", "i");
	addFunction(10.f, 0.f, dummy_func, "llDetectedRot", "q", "i");
	addFunction(10.f, 0.f, dummy_func, "llDetectedGroup", "i", "i");
	addFunction(10.f, 0.f, dummy_func, "llDetectedLinkNumber", "i", "i");
	addFunction(0.f, 0.f, dummy_func, "llDie", NULL, NULL);
	addFunction(10.f, 0.f, dummy_func, "llGround", "f", "v");
	addFunction(10.f, 0.f, dummy_func, "llCloud", "f", "v");
	addFunction(10.f, 0.f, dummy_func, "llWind", "v", "v");
	addFunction(10.f, 0.f, dummy_func, "llSetStatus", NULL, "ii");
	addFunction(10.f, 0.f, dummy_func, "llGetStatus", "i", "i");
	addFunction(10.f, 0.f, dummy_func, "llSetScale", NULL, "v");
	addFunction(10.f, 0.f, dummy_func, "llGetScale", "v", NULL);
	addFunction(10.f, 0.f, dummy_func, "llSetColor", NULL, "vi");
	addFunction(10.f, 0.f, dummy_func, "llGetAlpha", "f", "i");
	addFunction(10.f, 0.f, dummy_func, "llSetAlpha", NULL, "fi");
	addFunction(10.f, 0.f, dummy_func, "llGetColor", "v", "i");
	addFunction(10.f, 0.2f, dummy_func, "llSetTexture", NULL, "si");
	addFunction(10.f, 0.2f, dummy_func, "llScaleTexture", NULL, "ffi");
	addFunction(10.f, 0.2f, dummy_func, "llOffsetTexture", NULL, "ffi");
	addFunction(10.f, 0.2f, dummy_func, "llRotateTexture", NULL, "fi");
	addFunction(10.f, 0.f, dummy_func, "llGetTexture", "s", "i");
	addFunction(10.f, 0.2f, dummy_func, "llSetPos", NULL, "v");
	addFunction(10.f, 0.f, dummy_func, "llGetPos", "v", NULL);
	addFunction(10.f, 0.f, dummy_func, "llGetLocalPos", "v", NULL);
	addFunction(10.f, 0.2f, dummy_func, "llSetRot", NULL, "q");
	addFunction(10.f, 0.f, dummy_func, "llGetRot", "q", NULL);
	addFunction(10.f, 0.f, dummy_func, "llGetLocalRot", "q", NULL);
	addFunction(10.f, 0.f, dummy_func, "llSetForce", NULL, "vi");
	addFunction(10.f, 0.f, dummy_func, "llGetForce", "v", NULL);
	addFunction(10.f, 0.f, dummy_func, "llTarget", "i", "vf");
	addFunction(10.f, 0.f, dummy_func, "llTargetRemove", NULL, "i");
	addFunction(10.f, 0.f, dummy_func, "llRotTarget", "i", "qf");
	addFunction(10.f, 0.f, dummy_func, "llRotTargetRemove", NULL, "i");
	addFunction(10.f, 0.f, dummy_func, "llMoveToTarget", NULL, "vf");
	addFunction(10.f, 0.f, dummy_func, "llStopMoveToTarget", NULL, NULL);
	addFunction(10.f, 0.f, dummy_func, "llApplyImpulse", NULL, "vi");
	addFunction(10.f, 0.f, dummy_func, "llApplyRotationalImpulse", NULL, "vi");
	addFunction(10.f, 0.f, dummy_func, "llSetTorque", NULL, "vi");
	addFunction(10.f, 0.f, dummy_func, "llGetTorque", "v", NULL);
	addFunction(10.f, 0.f, dummy_func, "llSetForceAndTorque", NULL, "vvi");
	addFunction(10.f, 0.f, dummy_func, "llGetVel", "v", NULL);
	addFunction(10.f, 0.f, dummy_func, "llGetAccel", "v", NULL);
	addFunction(10.f, 0.f, dummy_func, "llGetOmega", "v", NULL);
	addFunction(10.f, 0.f, dummy_func, "llGetTimeOfDay", "f", "");
	addFunction(10.f, 0.f, dummy_func, "llGetWallclock", "f", "");
	addFunction(10.f, 0.f, dummy_func, "llGetTime", "f", NULL);
	addFunction(10.f, 0.f, dummy_func, "llResetTime", NULL, NULL);
	addFunction(10.f, 0.f, dummy_func, "llGetAndResetTime", "f", NULL);
	addFunction(10.f, 0.f, dummy_func, "llSound", NULL, "sfii");
	addFunction(10.f, 0.f, dummy_func, "llPlaySound", NULL, "sf");
	addFunction(10.f, 0.f, dummy_func, "llLoopSound", NULL, "sf");
	addFunction(10.f, 0.f, dummy_func, "llLoopSoundMaster", NULL, "sf");
	addFunction(10.f, 0.f, dummy_func, "llLoopSoundSlave", NULL, "sf");
	addFunction(10.f, 0.f, dummy_func, "llPlaySoundSlave", NULL, "sf");
	addFunction(10.f, 0.f, dummy_func, "llTriggerSound", NULL, "sf");
	addFunction(10.f, 0.f, dummy_func, "llStopSound", NULL, "");
	addFunction(10.f, 1.f, dummy_func, "llPreloadSound", NULL, "s");
	addFunction(10.f, 0.f, dummy_func, "llGetSubString", "s", "sii");
	addFunction(10.f, 0.f, dummy_func, "llDeleteSubString", "s", "sii");
	addFunction(10.f, 0.f, dummy_func, "llInsertString", "s", "sis");
	addFunction(10.f, 0.f, dummy_func, "llToUpper", "s", "s");
	addFunction(10.f, 0.f, dummy_func, "llToLower", "s", "s");
	addFunction(10.f, 0.f, dummy_func, "llGiveMoney", "i", "ki");
	addFunction(10.f, 0.1f, dummy_func, "llMakeExplosion", NULL, "iffffsv");
	addFunction(10.f, 0.1f, dummy_func, "llMakeFountain", NULL, "iffffisvf");
	addFunction(10.f, 0.1f, dummy_func, "llMakeSmoke", NULL, "iffffsv");
	addFunction(10.f, 0.1f, dummy_func, "llMakeFire", NULL, "iffffsv");
	addFunction(200.f, 0.1f, dummy_func, "llRezObject", NULL, "svvqi");
	addFunction(10.f, 0.f, dummy_func, "llLookAt", NULL, "vff");
	addFunction(10.f, 0.f, dummy_func, "llStopLookAt", NULL, NULL);
	addFunction(10.f, 0.f, dummy_func, "llSetTimerEvent", NULL, "f");
	addFunction(0.f, 0.f, dummy_func, "llSleep", NULL, "f");
	addFunction(10.f, 0.f, dummy_func, "llGetMass", "f", NULL);
	addFunction(10.f, 0.f, dummy_func, "llCollisionFilter", NULL, "ski");
	addFunction(10.f, 0.f, dummy_func, "llTakeControls", NULL, "iii");
	addFunction(10.f, 0.f, dummy_func, "llReleaseControls", NULL, NULL);
	addFunction(10.f, 0.f, dummy_func, "llAttachToAvatar", NULL, "i");
	addFunction(10.f, 0.f, dummy_func, "llDetachFromAvatar", NULL, NULL);
	addFunction(10.f, 0.f, dummy_func, "llTakeCamera", NULL, "k");
	addFunction(10.f, 0.f, dummy_func, "llReleaseCamera", NULL, "k");
	addFunction(10.f, 0.f, dummy_func, "llGetOwner", "k", NULL);
	addFunction(10.f, 2.f, dummy_func, "llInstantMessage", NULL, "ks");
	addFunction(10.f, 20.f, dummy_func, "llEmail", NULL, "sss");
	addFunction(10.f, 0.f, dummy_func, "llGetNextEmail", NULL, "ss");
	addFunction(10.f, 0.f, dummy_func, "llGetKey", "k", NULL);
	addFunction(10.f, 0.f, dummy_func, "llSetBuoyancy", NULL, "f");
	addFunction(10.f, 0.f, dummy_func, "llSetHoverHeight", NULL, "fif");
	addFunction(10.f, 0.f, dummy_func, "llStopHover", NULL, NULL);
	addFunction(10.f, 0.f, dummy_func, "llMinEventDelay", NULL, "f");
	addFunction(10.f, 0.f, dummy_func, "llSoundPreload", NULL, "s");
	addFunction(10.f, 0.f, dummy_func, "llRotLookAt", NULL, "qff");
	addFunction(10.f, 0.f, dummy_func, "llStringLength", "i", "s");
	addFunction(10.f, 0.f, dummy_func, "llStartAnimation", NULL, "s");
	addFunction(10.f, 0.f, dummy_func, "llStopAnimation", NULL, "s");
	addFunction(10.f, 0.f, dummy_func, "llPointAt", NULL, "v");
	addFunction(10.f, 0.f, dummy_func, "llStopPointAt", NULL, NULL);
	addFunction(10.f, 0.f, dummy_func, "llTargetOmega", NULL, "vff");
	addFunction(10.f, 0.f, dummy_func, "llGetStartParameter", "i", NULL);
	addFunction(10.f, 0.f, dummy_func, "llGodLikeRezObject", NULL, "kv", TRUE);
	addFunction(10.f, 0.f, dummy_func, "llRequestPermissions", NULL, "ki");
	addFunction(10.f, 0.f, dummy_func, "llGetPermissionsKey", "k", NULL);
	addFunction(10.f, 0.f, dummy_func, "llGetPermissions", "i", NULL);
	addFunction(10.f, 0.f, dummy_func, "llGetLinkNumber", "i", NULL);
	addFunction(10.f, 0.f, dummy_func, "llSetLinkColor", NULL, "ivi");
	addFunction(10.f, 1.f, dummy_func, "llCreateLink", NULL, "ki");
	addFunction(10.f, 0.f, dummy_func, "llBreakLink", NULL, "i");
	addFunction(10.f, 0.f, dummy_func, "llBreakAllLinks", NULL, NULL);
	addFunction(10.f, 0.f, dummy_func, "llGetLinkKey", "k", "i");
	addFunction(10.f, 0.f, dummy_func, "llGetLinkName", "s", "i");
	addFunction(10.f, 0.f, dummy_func, "llGetInventoryNumber", "i", "i");
	addFunction(10.f, 0.f, dummy_func, "llGetInventoryName", "s", "ii");
	addFunction(10.f, 0.f, dummy_func, "llSetScriptState", NULL, "si");
	addFunction(10.f, 0.f, dummy_func, "llGetEnergy", "f", NULL);
	addFunction(10.f, 0.f, dummy_func, "llGiveInventory", NULL, "ks");
	addFunction(10.f, 0.f, dummy_func, "llRemoveInventory", NULL, "s");
	addFunction(10.f, 0.f, dummy_func, "llSetText", NULL, "svf");
	addFunction(10.f, 0.f, dummy_func, "llWater", "f", "v");
	addFunction(10.f, 0.f, dummy_func, "llPassTouches", NULL, "i");
	addFunction(10.f, 0.1f, dummy_func, "llRequestAgentData", "k", "ki");
	addFunction(10.f, 1.f, dummy_func, "llRequestInventoryData", "k", "s");
	addFunction(10.f, 0.f, dummy_func, "llSetDamage", NULL, "f");
	addFunction(100.f, 5.f, dummy_func, "llTeleportAgentHome", NULL, "k");
	addFunction(10.f, 0.f, dummy_func, "llModifyLand", NULL, "ii");
	addFunction(10.f, 0.f, dummy_func, "llCollisionSound", NULL, "sf");
	addFunction(10.f, 0.f, dummy_func, "llCollisionSprite", NULL, "s");
	addFunction(10.f, 0.f, dummy_func, "llGetAnimation", "s", "k");
	addFunction(10.f, 0.f, dummy_func, "llResetScript", NULL, NULL);
	addFunction(10.f, 0.f, dummy_func, "llMessageLinked", NULL, "iisk");
	addFunction(10.f, 0.f, dummy_func, "llPushObject", NULL, "kvvi");
	addFunction(10.f, 0.f, dummy_func, "llPassCollisions", NULL, "i");
	addFunction(10.f, 0.f, dummy_func, "llGetScriptName", "s", NULL);
	addFunction(10.f, 0.f, dummy_func, "llGetNumberOfSides", "i", NULL);
	addFunction(10.f, 0.f, dummy_func, "llAxisAngle2Rot", "q", "vf");
	addFunction(10.f, 0.f, dummy_func, "llRot2Axis", "v", "q");
	addFunction(10.f, 0.f, dummy_func, "llRot2Angle", "f", "q");
	addFunction(10.f, 0.f, dummy_func, "llAcos", "f", "f");
	addFunction(10.f, 0.f, dummy_func, "llAsin", "f", "f");
	addFunction(10.f, 0.f, dummy_func, "llAngleBetween", "f", "qq");
	addFunction(10.f, 0.f, dummy_func, "llGetInventoryKey", "k", "s");
	addFunction(10.f, 0.f, dummy_func, "llAllowInventoryDrop", NULL, "i");
	addFunction(10.f, 0.f, dummy_func, "llGetSunDirection", "v", NULL);
	addFunction(10.f, 0.f, dummy_func, "llGetTextureOffset", "v", "i");
	addFunction(10.f, 0.f, dummy_func, "llGetTextureScale", "v", "i");
	addFunction(10.f, 0.f, dummy_func, "llGetTextureRot", "f", "i");
	addFunction(10.f, 0.f, dummy_func, "llSubStringIndex", "i", "ss");
	addFunction(10.f, 0.f, dummy_func, "llGetOwnerKey", "k", "k");
	addFunction(10.f, 0.f, dummy_func, "llGetCenterOfMass", "v", NULL);
	addFunction(10.f, 0.f, dummy_func, "llListSort", "l", "lii");
	addFunction(10.f, 0.f, dummy_func, "llGetListLength", "i", "l");
	addFunction(10.f, 0.f, dummy_func, "llList2Integer", "i", "li");
	addFunction(10.f, 0.f, dummy_func, "llList2Float", "f", "li");
	addFunction(10.f, 0.f, dummy_func, "llList2String", "s", "li");
	addFunction(10.f, 0.f, dummy_func, "llList2Key", "k", "li");
	addFunction(10.f, 0.f, dummy_func, "llList2Vector", "v", "li");
	addFunction(10.f, 0.f, dummy_func, "llList2Rot", "q", "li");
	addFunction(10.f, 0.f, dummy_func, "llList2List", "l", "lii");
	addFunction(10.f, 0.f, dummy_func, "llDeleteSubList", "l", "lii");
	addFunction(10.f, 0.f, dummy_func, "llGetListEntryType", "i", "li");
	addFunction(10.f, 0.f, dummy_func, "llList2CSV", "s", "l");
	addFunction(10.f, 0.f, dummy_func, "llCSV2List", "l", "s");
	addFunction(10.f, 0.f, dummy_func, "llListRandomize", "l", "li");
	addFunction(10.f, 0.f, dummy_func, "llList2ListStrided", "l", "liii");
	addFunction(10.f, 0.f, dummy_func, "llGetRegionCorner", "v", NULL);
	addFunction(10.f, 0.f, dummy_func, "llListInsertList", "l", "lli");
	addFunction(10.f, 0.f, dummy_func, "llListFindList", "i", "ll");
	addFunction(10.f, 0.f, dummy_func, "llGetObjectName", "s", NULL);
	addFunction(10.f, 0.f, dummy_func, "llSetObjectName", NULL, "s");
	addFunction(10.f, 0.f, dummy_func, "llGetDate", "s", NULL);
	addFunction(10.f, 0.f, dummy_func, "llEdgeOfWorld", "i", "vv");
	addFunction(10.f, 0.f, dummy_func, "llGetAgentInfo", "i", "k");
	addFunction(10.f, 0.1f, dummy_func, "llAdjustSoundVolume", NULL, "f");
	addFunction(10.f, 0.f, dummy_func, "llSetSoundQueueing", NULL, "i");
	addFunction(10.f, 0.f, dummy_func, "llSetSoundRadius", NULL, "f");
	addFunction(10.f, 0.f, dummy_func, "llKey2Name", "s", "k");
	addFunction(10.f, 0.f, dummy_func, "llSetTextureAnim", NULL, "iiiifff");
	addFunction(10.f, 0.f, dummy_func, "llTriggerSoundLimited", NULL, "sfvv");
	addFunction(10.f, 0.f, dummy_func, "llEjectFromLand", NULL, "k");
	addFunction(10.f, 0.f, dummy_func, "llParseString2List", "l", "sll");
	addFunction(10.f, 0.f, dummy_func, "llOverMyLand", "i", "k");
	addFunction(10.f, 0.f, dummy_func, "llGetLandOwnerAt", "k", "v");
	addFunction(10.f, 0.1f, dummy_func, "llGetNotecardLine", "k", "si");
	addFunction(10.f, 0.f, dummy_func, "llGetAgentSize", "v", "k");
	addFunction(10.f, 0.f, dummy_func, "llSameGroup", "i", "k");
	addFunction(10.f, 0.f, dummy_func, "llUnSit", NULL, "k");
	addFunction(10.f, 0.f, dummy_func, "llGroundSlope", "v", "v");
	addFunction(10.f, 0.f, dummy_func, "llGroundNormal", "v", "v");
	addFunction(10.f, 0.f, dummy_func, "llGroundContour", "v", "v");
	addFunction(10.f, 0.f, dummy_func, "llGetAttached", "i", NULL);
	addFunction(10.f, 0.f, dummy_func, "llGetFreeMemory", "i", NULL);
	addFunction(10.f, 0.f, dummy_func, "llGetRegionName", "s", NULL);
	addFunction(10.f, 0.f, dummy_func, "llGetRegionTimeDilation", "f", NULL);
	addFunction(10.f, 0.f, dummy_func, "llGetRegionFPS", "f", NULL);

	addFunction(10.f, 0.f, dummy_func, "llParticleSystem", NULL, "l");
	addFunction(10.f, 0.f, dummy_func, "llGroundRepel", NULL, "fif");
	addFunction(10.f, 3.f, dummy_func, "llGiveInventoryList", NULL, "ksl");

// script calls for vehicle action
	addFunction(10.f, 0.f, dummy_func, "llSetVehicleType", NULL, "i");
	addFunction(10.f, 0.f, dummy_func, "llSetVehicleFloatParam", NULL, "if");
	addFunction(10.f, 0.f, dummy_func, "llSetVehicleVectorParam", NULL, "iv");
	addFunction(10.f, 0.f, dummy_func, "llSetVehicleRotationParam", NULL, "iq");
	addFunction(10.f, 0.f, dummy_func, "llSetVehicleFlags", NULL, "i");
	addFunction(10.f, 0.f, dummy_func, "llRemoveVehicleFlags", NULL, "i");
	addFunction(10.f, 0.f, dummy_func, "llSitTarget", NULL, "vq");
	addFunction(10.f, 0.f, dummy_func, "llAvatarOnSitTarget", "k", NULL);
	addFunction(10.f, 0.1f, dummy_func, "llAddToLandPassList", NULL, "kf");
	addFunction(10.f, 0.f, dummy_func, "llSetTouchText", NULL, "s");
	addFunction(10.f, 0.f, dummy_func, "llSetSitText", NULL, "s");
	addFunction(10.f, 0.f, dummy_func, "llSetCameraEyeOffset", NULL, "v");
	addFunction(10.f, 0.f, dummy_func, "llSetCameraAtOffset", NULL, "v");

	addFunction(10.f, 0.f, dummy_func, "llDumpList2String", "s", "ls");
	addFunction(10.f, 0.f, dummy_func, "llScriptDanger", "i", "v");
	addFunction(10.f, 1.f, dummy_func, "llDialog", NULL, "ksli");
	addFunction(10.f, 0.f, dummy_func, "llVolumeDetect", NULL, "i");
	addFunction(10.f, 0.f, dummy_func, "llResetOtherScript", NULL, "s");
	addFunction(10.f, 0.f, dummy_func, "llGetScriptState", "i", "s");
	addFunction(10.f, 3.f, dummy_func, "llRemoteLoadScript", NULL, "ksii");

	addFunction(10.f, 0.2f, dummy_func, "llSetRemoteScriptAccessPin", NULL, "i");
	addFunction(10.f, 3.f, dummy_func, "llRemoteLoadScriptPin", NULL, "ksiii");
	
	addFunction(10.f, 1.f, dummy_func, "llOpenRemoteDataChannel", NULL, NULL);
	addFunction(10.f, 3.f, dummy_func, "llSendRemoteData", "k", "ksis");
	addFunction(10.f, 3.f, dummy_func, "llRemoteDataReply", NULL, "kksi");
	addFunction(10.f, 1.f, dummy_func, "llCloseRemoteDataChannel", NULL, "k");

	addFunction(10.f, 0.f, dummy_func, "llMD5String", "s", "si");
	addFunction(10.f, 0.2f, dummy_func, "llSetPrimitiveParams", NULL, "l");
	addFunction(10.f, 0.f, dummy_func, "llStringToBase64", "s", "s");
	addFunction(10.f, 0.f, dummy_func, "llBase64ToString", "s", "s");
	addFunction(10.f, 0.3f, dummy_func, "llXorBase64Strings", "s", "ss");
	addFunction(10.f, 0.f, dummy_func, "llRemoteDataSetRegion", NULL, NULL);
	addFunction(10.f, 0.f, dummy_func, "llLog10", "f", "f");
	addFunction(10.f, 0.f, dummy_func, "llLog", "f", "f");
	addFunction(10.f, 0.f, dummy_func, "llGetAnimationList", "l", "k");
	addFunction(10.f, 2.f, dummy_func, "llSetParcelMusicURL", NULL, "s");
	
	addFunction(10.f, 0.f, dummy_func, "llGetRootPosition", "v", NULL);
	addFunction(10.f, 0.f, dummy_func, "llGetRootRotation", "q", NULL);

	addFunction(10.f, 0.f, dummy_func, "llGetObjectDesc", "s", NULL);
	addFunction(10.f, 0.f, dummy_func, "llSetObjectDesc", NULL, "s");
	addFunction(10.f, 0.f, dummy_func, "llGetCreator", "k", NULL);
	addFunction(10.f, 0.f, dummy_func, "llGetTimestamp", "s", NULL);
	addFunction(10.f, 0.f, dummy_func, "llSetLinkAlpha", NULL, "ifi");
	addFunction(10.f, 0.f, dummy_func, "llGetNumberOfPrims", "i", NULL);
	addFunction(10.f, 0.1f, dummy_func, "llGetNumberOfNotecardLines", "k", "s");

	addFunction(10.f, 0.f, dummy_func, "llGetBoundingBox", "l", "k");
	addFunction(10.f, 0.f, dummy_func, "llGetGeometricCenter", "v", NULL);
	addFunction(10.f, 0.2f, dummy_func, "llGetPrimitiveParams", "l", "l");
	addFunction(10.f, 0.0f, dummy_func, "llIntegerToBase64", "s", "i");
	addFunction(10.f, 0.0f, dummy_func, "llBase64ToInteger", "i", "s");
	addFunction(10.f, 0.f, dummy_func, "llGetGMTclock", "f", "");
	addFunction(10.f, 10.f, dummy_func, "llGetSimulatorHostname", "s", "");
	
	addFunction(10.f, 0.2f, dummy_func, "llSetLocalRot", NULL, "q");

	addFunction(10.f, 0.f, dummy_func, "llParseStringKeepNulls", "l", "sll");
	addFunction(200.f, 0.1f, dummy_func, "llRezAtRoot", NULL, "svvqi");

	addFunction(10.f, 0.f, dummy_func, "llGetObjectPermMask", "i", "i", FALSE);
	addFunction(10.f, 0.f, dummy_func, "llSetObjectPermMask", NULL, "ii", TRUE);

	addFunction(10.f, 0.f, dummy_func, "llGetInventoryPermMask", "i", "si", FALSE);
	addFunction(10.f, 0.f, dummy_func, "llSetInventoryPermMask", NULL, "sii", TRUE);
	addFunction(10.f, 0.f, dummy_func, "llGetInventoryCreator", "k", "s", FALSE);
	addFunction(10.f, 0.f, dummy_func, "llOwnerSay", NULL, "s");
	addFunction(10.f, 1.f, dummy_func, "llRequestSimulatorData", "k", "si");
	addFunction(10.f, 0.f, dummy_func, "llForceMouselook", NULL, "i");
	addFunction(10.f, 0.f, dummy_func, "llGetObjectMass", "f", "k");
	addFunction(10.f, 0.f, dummy_func, "llListReplaceList", "l", "llii");
	addFunction(10.f, 10.f, dummy_func, "llLoadURL", NULL, "kss");

	addFunction(10.f, 2.f, dummy_func, "llParcelMediaCommandList", NULL, "l");
	addFunction(10.f, 2.f, dummy_func, "llParcelMediaQuery", "l", "l");

	addFunction(10.f, 1.f, dummy_func, "llModPow", "i", "iii");
	
	addFunction(10.f, 0.f, dummy_func, "llGetInventoryType", "i", "s");
	addFunction(10.f, 0.f, dummy_func, "llSetPayPrice", NULL, "il");
	addFunction(10.f, 0.f, dummy_func, "llGetCameraPos", "v", "");
	addFunction(10.f, 0.f, dummy_func, "llGetCameraRot", "q", "");
	
	addFunction(10.f, 20.f, dummy_func, "llSetPrimURL", NULL, "s");
	addFunction(10.f, 20.f, dummy_func, "llRefreshPrimURL", NULL, "");
	addFunction(10.f, 0.f, dummy_func, "llEscapeURL", "s", "s");
	addFunction(10.f, 0.f, dummy_func, "llUnescapeURL", "s", "s");

	addFunction(10.f, 1.f, dummy_func, "llMapDestination", NULL, "svv");
	addFunction(10.f, 0.1f, dummy_func, "llAddToLandBanList", NULL, "kf");
	addFunction(10.f, 0.1f, dummy_func, "llRemoveFromLandPassList", NULL, "k");
	addFunction(10.f, 0.1f, dummy_func, "llRemoveFromLandBanList", NULL, "k");

	addFunction(10.f, 0.f, dummy_func, "llSetCameraParams", NULL, "l");
	addFunction(10.f, 0.f, dummy_func, "llClearCameraParams", NULL, NULL);
	
	addFunction(10.f, 0.f, dummy_func, "llListStatistics", "f", "il");
	addFunction(10.f, 0.f, dummy_func, "llGetUnixTime", "i", NULL);
	addFunction(10.f, 0.f, dummy_func, "llGetParcelFlags", "i", "v");
	addFunction(10.f, 0.f, dummy_func, "llGetRegionFlags", "i", NULL);
	addFunction(10.f, 0.f, dummy_func, "llXorBase64StringsCorrect", "s", "ss");

	addFunction(10.f, 0.f, dummy_func, "llHTTPRequest", "k", "sls");

	addFunction(10.f, 0.1f, dummy_func, "llResetLandBanList", NULL, NULL);
	addFunction(10.f, 0.1f, dummy_func, "llResetLandPassList", NULL, NULL);

	addFunction(10.f, 0.f, dummy_func, "llGetObjectPrimCount", "i", "k");
	addFunction(10.f, 2.0f, dummy_func, "llGetParcelPrimOwners", "l", "v");
	addFunction(10.f, 0.f, dummy_func, "llGetParcelPrimCount", "i", "vii");
	addFunction(10.f, 0.f, dummy_func, "llGetParcelMaxPrims", "i", "vi");
	addFunction(10.f, 0.f, dummy_func, "llGetParcelDetails", "l", "vl");


	addFunction(10.f, 0.2f, dummy_func, "llSetLinkPrimitiveParams", NULL, "il");
	addFunction(10.f, 0.2f, dummy_func, "llSetLinkTexture", NULL, "isi");

	
	addFunction(10.f, 0.f, dummy_func, "llStringTrim", "s", "si");
	addFunction(10.f, 0.f, dummy_func, "llRegionSay", NULL, "is");
	addFunction(10.f, 0.f, dummy_func, "llGetObjectDetails", "l", "kl");
	addFunction(10.f, 0.f, dummy_func, "llSetClickAction", NULL, "i");

	addFunction(10.f, 0.f, dummy_func, "llGetRegionAgentCount", "i", NULL);
	addFunction(10.f, 1.f, dummy_func, "llTextBox", NULL, "ksi");
	addFunction(10.f, 0.f, dummy_func, "llGetAgentLanguage", "s", "k");
	addFunction(10.f, 0.f, dummy_func, "llDetectedTouchUV", "v", "i");
	addFunction(10.f, 0.f, dummy_func, "llDetectedTouchFace", "i", "i");
	addFunction(10.f, 0.f, dummy_func, "llDetectedTouchPos", "v", "i");
	addFunction(10.f, 0.f, dummy_func, "llDetectedTouchNormal", "v", "i");
	addFunction(10.f, 0.f, dummy_func, "llDetectedTouchBinormal", "v", "i");
	addFunction(10.f, 0.f, dummy_func, "llDetectedTouchST", "v", "i");

	addFunction(10.f, 0.f, dummy_func, "llSHA1String", "s", "s");

	addFunction(10.f, 0.f, dummy_func, "llGetFreeURLs", "i", NULL);
	addFunction(10.f, 0.f, dummy_func, "llRequestURL", "k", NULL);
	addFunction(10.f, 0.f, dummy_func, "llRequestSecureURL", "k", NULL);
	addFunction(10.f, 0.f, dummy_func, "llReleaseURL", NULL, "s");
	addFunction(10.f, 0.f, dummy_func, "llHTTPResponse", NULL, "kis");
	addFunction(10.f, 0.f, dummy_func, "llGetHTTPHeader", "s", "ks");

	// Prim media (see lscript_prim_media.h)
	addFunction(10.f, 1.0f, dummy_func, "llSetPrimMediaParams", "i", "il");
	addFunction(10.f, 1.0f, dummy_func, "llGetPrimMediaParams", "l", "il");
	addFunction(10.f, 1.0f, dummy_func, "llClearPrimMedia", "i", "i");
	addFunction(10.f, 0.f, dummy_func, "llSetLinkPrimitiveParamsFast", NULL, "il");
	addFunction(10.f, 0.f, dummy_func, "llGetLinkPrimitiveParams", "l", "il");
	addFunction(10.f, 0.f, dummy_func, "llLinkParticleSystem", NULL, "il");
	addFunction(10.f, 0.f, dummy_func, "llSetLinkTextureAnim", NULL, "iiiiifff");
	
	addFunction(10.f, 0.f, dummy_func, "llGetLinkNumberOfSides", "i", "i");
	
	// IDEVO Name lookup calls, see lscript_avatar_names.h
	addFunction(10.f, 0.f, dummy_func, "llGetUsername", "s", "k");
	addFunction(10.f, 0.f, dummy_func, "llRequestUsername", "k", "k");
	addFunction(10.f, 0.f, dummy_func, "llGetDisplayName", "s", "k");
	addFunction(10.f, 0.f, dummy_func, "llRequestDisplayName", "k", "k");

	addFunction(10.f, 0.f, dummy_func, "llGetEnv", "s", "s");
	addFunction(10.f, 0.f, dummy_func, "llRegionSayTo", NULL, "kis");

	// energy, sleep, dummy_func, name, return type, parameters, help text, gods-only

	// IF YOU ADD NEW SCRIPT CALLS, YOU MUST PUT THEM AT THE END OF THIS LIST.
	// Otherwise the bytecode numbers for each call will be wrong, and all
	// existing scripts will crash.

	// <FS:Ansariel> According to Kelly Linden we don't need to obey the function ID order in the viewer!
	// Server v11.08.10.238207 new functions:
	addFunction(10.f, 0.f, dummy_func, "llSetMemoryLimit", "i", "i");
	addFunction(10.f, 0.f, dummy_func, "llGetMemoryLimit", "i", NULL);
	addFunction(10.f, 0.f, dummy_func, "llSetLinkMedia", "i", "iil");
	addFunction(10.f, 0.f, dummy_func, "llGetLinkMedia", "l", "iil");
	addFunction(10.f, 0.f, dummy_func, "llClearLinkMedia", "i", "ii");
	addFunction(10.f, 0.f, dummy_func, "llSetLinkCamera", NULL, "ivv");	
	addFunction(10.f, 0.f, dummy_func, "llSetContentType", NULL, "ki");	
	addFunction(10.f, 0.f, dummy_func, "llLinkSitTarget", NULL, "ivq");
	addFunction(10.f, 0.f, dummy_func, "llAvatarOnLinkSitTarget", "k", "i");
	addFunction(10.f, 0.f, dummy_func, "llSetVelocity", NULL, "vi");	

	// Server v11.09.09.240509 new functions:
	addFunction(10.f, 0.f, dummy_func, "llCastRay", "l", "vvl");
	addFunction(10.f, 0.f, dummy_func, "llGetMassMKS", "f", NULL);
	addFunction(10.f, 0.f, dummy_func, "llSetPhysicsMaterial", NULL, "iffff");
	addFunction(10.f, 0.f, dummy_func, "llGetPhysicsMaterial", "l", NULL);

	// Server v11.10.18.243270 new functions:
	addFunction(10.f, 0.f, dummy_func, "llManageEstateAccess", "i", "ik");

	// Server RC magnum v11.10.31.244254 new function:
	addFunction(10.f, 0.f, dummy_func, "llSetKeyframedMotion", NULL, "ll");

	// Server RC Le Tigre v11.10.30.245889 new function:
	addFunction(10.f, 0.f, dummy_func, "llTransferLindenDollars", "k", "ki");

	// Server new function 2011-12-13:
	addFunction(10.f, 0.f, dummy_func, "llGetParcelMusicURL", "s", NULL);

	// Missing script functions as of 2011-12-13
	addFunction(10.f, 0.f, dummy_func, "llScriptProfiler", NULL, "i");
	addFunction(10.f, 0.f, dummy_func, "llGetSPMaxMemory", "i", NULL);
	addFunction(10.f, 0.f, dummy_func, "llGetUsedMemory", "i", NULL);
	addFunction(10.f, 0.f, dummy_func, "llSetAngularVelocity", NULL, "vi");

	// Server 12.01.24.248357 new functions
	addFunction(0.f, 0.f, dummy_func, "llSetRegionPos", "i", "v");

	// Server 12.04.13.253827 new function
	addFunction(0.f, 0.f, dummy_func, "llGetAgentList", "l", "il");
	
	// Server RC Magnum 12.05.25.258071 new functions:
	addFunction(0.f, 0.f, dummy_func, "llAttachToAvatarTemp", NULL, "i");
	addFunction(0.f, 0.f, dummy_func, "llTeleportAgent", NULL, "ksvv");
	addFunction(0.f, 0.f, dummy_func, "llTeleportAgentGlobalCoords", NULL, "kvvv");

	// LSL functions received as per Pathfinding tools merge
	addFunction(0.f, 0.f, dummy_func, "llGenerateKey", "k", NULL);
	addFunction(0.f, 0.f, dummy_func, "llNavigateTo", NULL, "vl");
	addFunction(0.f, 0.f, dummy_func, "llCreateCharacter", NULL, "l");
	addFunction(0.f, 0.f, dummy_func, "llPursue", NULL, "kl");
	addFunction(0.f, 0.f, dummy_func, "llWanderWithin", NULL, "vfl");
	addFunction(0.f, 0.f, dummy_func, "llFleeFrom", NULL, "vfl");
	addFunction(0.f, 0.f, dummy_func, "llPatrolPoints", NULL, "ll");
	addFunction(0.f, 0.f, dummy_func, "llExecCharacterCmd", NULL, "il");
	addFunction(0.f, 0.f, dummy_func, "llDeleteCharacter", NULL, NULL);
	addFunction(0.f, 0.f, dummy_func, "llUpdateCharacter", NULL, "l");
	addFunction(0.f, 0.f, dummy_func, "llEvade", NULL, "kl");
	addFunction(0.f, 0.f, dummy_func, "llGetClosestNavPoint", "l", "vl");
	addFunction(0.f, 0.f, dummy_func, "llGetStaticPath", "l", "vvfl");

	// Server RC LeTigre 12.10.12.265819 new function
	addFunction(0.f, 0.f, dummy_func, "llGetSimStats", "f", "i");
	// </FS:Ansariel> According to Kelly Linden we don't need to obey the function ID order in the viewer!
	
// <FS:CR> Opensim/Aurora scripting functions - Please add Second Life LSL functions above this line!
#ifdef HAS_OPENSIM_SUPPORT
	/// OSSL/Aurora Functions (current as of 13 December 2012)
	/// Reference: https://github.com/opensim/opensim/blob/master/OpenSim/Region/ScriptEngine/Shared/Api/Runtime/OSSL_Stub.cs
	addFunction(10.f, 0.f, dummy_func, "osSetRegionWaterHeight", NULL, "f");
	addFunction(10.f, 0.f, dummy_func, "osSetRegionSunSettings", NULL, "iif");
	addFunction(10.f, 0.f, dummy_func, "osSetEstateSunSettings", NULL, "if");
	addFunction(10.f, 0.f, dummy_func, "osGetCurrentSunHour", "f", NULL);
	addFunction(10.f, 0.f, dummy_func, "osGetSunParam","f", "s");
	addFunction(10.f, 0.f, dummy_func, "osSetSunParam", "sf", NULL);
	// Wind Module Functions
	addFunction(10.f, 0.f, dummy_func, "osWindActiveModelPluginName", "s", NULL);
	addFunction(10.f, 0.f, dummy_func, "osSetWindParam", NULL, "ssf");
	addFunction(10.f, 0.f, dummy_func, "osGetWindParam", "f", "ss");
	// Parcel commands
	addFunction(10.f, 0.f, dummy_func, "osParcelJoin", NULL, "vv");
	addFunction(10.f, 0.f, dummy_func, "osParcelSubdivide", NULL, "vv");
	addFunction(10.f, 0.f, dummy_func, "osSetParcelDetails", NULL, "vl");
	
	addFunction(10.f, 0.f, dummy_func, "osList2Double", "f", "li");
	
	addFunction(10.f, 0.f, dummy_func, "osSetDynamicTextureURL", NULL, "ksssi");
	addFunction(10.f, 0.f, dummy_func, "osSetDynamicTextureURLBlend", NULL, "ksssii");
	addFunction(10.f, 0.f, dummy_func, "osSetDynamicTextureURLBlendFace", NULL, "ksssfiiii");
	addFunction(10.f, 0.f, dummy_func, "osSetDynamicTextureData", NULL, "ksssi");
	addFunction(10.f, 0.f, dummy_func, "osSetDynamicTextureDataBlend", NULL, "ksssii");
	addFunction(10.f, 0.f, dummy_func, "osSetDynamicTextureDataBlendFace", NULL, "ksssfiiii");
	addFunction(10.f, 0.f, dummy_func, "osGetTerrainHeight", "f", "ii");
	addFunction(10.f, 0.f, dummy_func, "osSetTerrainHeight", NULL, "iif");
	addFunction(10.f, 0.f, dummy_func, "osTerrainFlush", NULL, NULL);
	addFunction(10.f, 0.f, dummy_func, "osRegionRestart", "i", "f");
	addFunction(10.f, 0.f, dummy_func, "osRegionNotice",NULL, "s");
	addFunction(10.f, 0.f, dummy_func, "osConsoleCommand", NULL, "s");
	addFunction(10.f, 0.f, dummy_func, "osSetParcelMediaURL", NULL, "s");
	addFunction(10.f, 0.f, dummy_func, "osSetParcelSIPaddress", NULL, "s");
	addFunction(10.f, 0.f, dummy_func, "osSetPrimFloatOnWater", NULL, "i");
	// Teleport functions
	addFunction(10.f, 0.f, dummy_func, "osTeleportAgent", NULL, "ksvv");
	addFunction(10.f, 0.f, dummy_func, "osTeleportOwner", NULL, "svv");
	// Avatar Info functions
	addFunction(10.f, 0.f, dummy_func, "osGetAgentIP", "s", "k");
	addFunction(10.f, 0.f, dummy_func, "osGetAgents", "l", NULL);
	// Animation functions
	addFunction(10.f, 0.f, dummy_func, "osAvatarPlayAnimation", NULL, "ks");
	addFunction(10.f, 0.f, dummy_func, "osAvatarStopAnimation", NULL, "ks");
	// #region Attachment functions
	addFunction(10.f, 0.f, dummy_func, "osForceAttachToAvatar", NULL, "i");
	addFunction(10.f, 0.f, dummy_func, "osForceAttachToAvatarFromInventory", NULL, "si");
	addFunction(10.f, 0.f, dummy_func, "osForceAttachToOtherAvatarFromInventory", NULL, "ssi");
	addFunction(10.f, 0.f, dummy_func, "osForceDetachFromAvatar", NULL, NULL);
	addFunction(10.f, 0.f, dummy_func, "osGetNumberOfAttachments", "l", "kl");
	addFunction(10.f, 0.f, dummy_func, "osMessageAttachments", NULL, "ksli");
	// Texture Draw functions
	addFunction(10.f, 0.f, dummy_func, "osMovePen", NULL, "sii");
	addFunction(10.f, 0.f, dummy_func, "osDrawLine", NULL, "siiii");
	addFunction(10.f, 0.f, dummy_func, "osDrawText", NULL, "ss");
	addFunction(10.f, 0.f, dummy_func, "osDrawEllipse", NULL, "sii");
	addFunction(10.f, 0.f, dummy_func, "osDrawRectangle", NULL, "sii");
	addFunction(10.f, 0.f, dummy_func, "osDrawFilledRectangle", NULL, "sii");
	addFunction(10.f, 0.f, dummy_func, "osDrawPolygon", "s", "sll");
	addFunction(10.f, 0.f, dummy_func, "osDrawFilledPolygon", "s", "sll");
	addFunction(10.f, 0.f, dummy_func, "osSetFontSize", NULL, "si");
	addFunction(10.f, 0.f, dummy_func, "osSetFontName", NULL, "ss");
	addFunction(10.f, 0.f, dummy_func, "osSetPenSize", NULL, "si");
	addFunction(10.f, 0.f, dummy_func, "osSetPenColor", NULL, "ss");
	addFunction(10.f, 0.f, dummy_func, "osSetPenCap", NULL, "sss");
	addFunction(10.f, 0.f, dummy_func, "osDrawImage", NULL, "siis");
	addFunction(10.f, 0.f, dummy_func, "osGetDrawStringSize", "v", "sssi");
	
	// DEPRECIATED addFunction(10.f, 0.f, dummy_func, "osSetStateEvents", NULL, "i");
	addFunction(10.f, 0.f, dummy_func, "osGetScriptEngineName", "s", NULL);
	addFunction(10.f, 0.f, dummy_func, "osGetSimulatorVersion", "s", NULL);
	addFunction(10.f, 0.f, dummy_func, "osParseJSON", "s", "s");
	addFunction(10.f, 0.f, dummy_func, "osParseJSONNew", "s", "s");
	
	addFunction(10.f, 0.f, dummy_func, "osMessageObject", NULL, "ks");
	
	addFunction(10.f, 0.f, dummy_func, "osMakeNotecard", NULL, "sl");
	addFunction(10.f, 0.f, dummy_func, "osGetNotecardLine", "s", "si");
	addFunction(10.f, 0.f, dummy_func, "osGetNotecard", "s", "s");
	addFunction(10.f, 0.f, dummy_func, "osGetNumberOfNotecardLines", "i", "s");
	addFunction(10.f, 0.f, dummy_func, "osAvatarName2Key", "k", "ss");
	addFunction(10.f, 0.f, dummy_func, "osKey2Name", "s", "k");
	addFunction(10.f, 0.f, dummy_func, "osGetGridNick", "s", NULL);
	addFunction(10.f, 0.f, dummy_func, "osGetGridName", "s", NULL);
	addFunction(10.f, 0.f, dummy_func, "osGetGridLoginURI", "s", NULL);
	addFunction(10.f, 0.f, dummy_func, "osGetGridHomeURI","s",NULL);
	addFunction(10.f, 0.f, dummy_func, "osGetGridCustom","s","k");
	addFunction(10.f, 0.f, dummy_func, "osFormatString", "s", "sl");
	addFunction(10.f, 0.f, dummy_func, "osMatchString", "l", "ssi");
	addFunction(10.f, 0.f, dummy_func, "osReplaceString", "s", "sssi");
	// Information about data loaded into the region
	addFunction(10.f, 0.f, dummy_func, "osLoadedCreationDate", "s", NULL);
	addFunction(10.f, 0.f, dummy_func, "osLoadedCreationTime", "s", NULL);
	addFunction(10.f, 0.f, dummy_func, "osLoadedCreationID", "s", NULL);
	addFunction(10.f, 0.f, dummy_func, "osGetLinkPrimitiveParams", "l", "il");
	// osNpc
	addFunction(10.f, 0.f, dummy_func, "osIsNpc", "k", "k");
	addFunction(10.f, 0.f, dummy_func, "osNpcCreate", "k", "ssvk");
	addFunction(10.f, 0.f, dummy_func, "osNpcSaveAppearance", NULL, "ks");
	addFunction(10.f, 0.f, dummy_func, "osNpcLoadAppearance", NULL,"ks");
	addFunction(10.f, 0.f, dummy_func, "osNpcGetOwner","k","k");
	addFunction(10.f, 0.f, dummy_func, "osNpcGetPos","k","k");
	addFunction(10.f, 0.f, dummy_func, "osNpcMoveTo", NULL, "kv");
	addFunction(10.f, 0.f, dummy_func, "osNpcMoveToTarget",NULL,"kvi");
	addFunction(10.f, 0.f, dummy_func, "osNpcGetRot","r","k");
	addFunction(10.f, 0.f, dummy_func, "osNpcSetRot", NULL, "kr");
	addFunction(10.f, 0.f, dummy_func, "osNpcStopMoveToTarget", NULL, "k");
	addFunction(10.f, 0.f, dummy_func, "osNpcSay", NULL, "ks");
	addFunction(10.f, 0.f, dummy_func, "osNpcShout", NULL, "ks");
	addFunction(10.f, 0.f, dummy_func, "osNpcSit", NULL, "kki");
	addFunction(10.f, 0.f, dummy_func, "osNpcStand", NULL, "kki");
	addFunction(10.f, 0.f, dummy_func, "osNpcRemove", NULL, "k");
	addFunction(10.f, 0.f, dummy_func, "osNpcPlayAnimation", NULL, "ks");
	addFunction(10.f, 0.f, dummy_func, "osNpcStopAnimation", NULL, "ks");
	addFunction(10.f, 0.f, dummy_func, "osNpcWhisper", NULL, "ks");
	addFunction(10.f, 0.f, dummy_func, "osNpcTouch", NULL, "kki");
	addFunction(10.f, 0.f, dummy_func, "osOwnerSaveAppearance", NULL, "s");
	addFunction(10.f, 0.f, dummy_func, "osAgentSaveAppearance", NULL, "ks");
	
	addFunction(10.f, 0.f, dummy_func, "osGetMapTexture", "k", NULL);
	addFunction(10.f, 0.f, dummy_func, "osGetRegionMapTexture", "k", "s");
	addFunction(10.f, 0.f, dummy_func, "osGetRegionStats", "l", NULL);
	addFunction(10.f, 0.f, dummy_func, "osGetSimulatorMemory", "i", NULL);
	addFunction(10.f, 0.f, dummy_func, "osKickAvatar", NULL, "sss");
	addFunction(10.f, 0.f, dummy_func, "osSetSpeed", NULL, "kf");
	addFunction(10.f, 0.f, dummy_func, "osGetHealth", "f", "k");
	addFunction(10.f, 0.f, dummy_func, "osCauseDamage", NULL, "kf");
	addFunction(10.f, 0.f, dummy_func, "osCauseHealing", NULL, "kf");
	addFunction(10.f, 0.f, dummy_func, "osGetPrimitiveParams", "l", "kl");
	addFunction(10.f, 0.f, dummy_func, "osSetPrimitiveParams", NULL, "kl");
	addFunction(10.f, 0.f, dummy_func, "osSetProjectionParams", NULL, "kikfff");
	addFunction(10.f, 0.f, dummy_func, "osGetAvatarList", "l", NULL);
	addFunction(10.f, 0.f, dummy_func, "osUnixTimeToTimestamp", "s", "i");
	addFunction(10.f, 0.f, dummy_func, "osGetInventoryDesc", "s", "s");
	addFunction(10.f, 0.f, dummy_func, "osInviteToGroup", "i", "i");
	addFunction(10.f, 0.f, dummy_func, "osEjectFromGroup", "i", "i");
	addFunction(10.f, 0.f, dummy_func, "osSetTerrainTexture", NULL, "ik");
	addFunction(10.f, 0.f, dummy_func, "osSetTerrainTextureHeight", NULL, "iff");
	
	addFunction(10.f, 0.f, dummy_func, "osIsUUID", "i","s");
	
	addFunction(10.f, 0.f, dummy_func, "osMin", "f", "ff");
	addFunction(10.f, 0.f, dummy_func, "osMax", "f", "ff");
	addFunction(10.f, 0.f, dummy_func, "osGetRezzingObject", "k", NULL);
	addFunction(10.f, 0.f, dummy_func, "osSetContentType", NULL, "ks");
	
	addFunction(10.f, 0.f, dummy_func, "osDropAttachment", NULL, NULL);
	addFunction(10.f, 0.f, dummy_func, "osDropAttachmentAt", NULL, "vr");
	addFunction(10.f, 0.f, dummy_func, "osForceDropAttachment", NULL, NULL);
	addFunction(10.f, 0.f, dummy_func, "osForceDropAttachmentAt", NULL, "vr");
	
	addFunction(10.f, 0.f, dummy_func, "osListenRegex", "i", "issi");
	addFunction(10.f, 0.f, dummy_func, "osRegexIsMatch", "i", "ss");
	/// LightShare functions
	/// Reference: https://github.com/opensim/opensim/blob/master/OpenSim/Region/ScriptEngine/Shared/Api/Runtime/LS_Stub.cs
	addFunction(10.f, 0.f, dummy_func, "lsGetWindlightScene", "l", "l");
	addFunction(10.f, 0.f, dummy_func, "lsSetWindlightScene", "i", "l");
	addFunction(10.f, 0.f, dummy_func, "lsSetWindlightSceneTargeted", "i", "lk");
	/// Aurora-exclusive OSSL functions
	/// Reference: https://github.com/aurora-sim/Aurora-Sim/blob/master/Aurora/AuroraDotNetEngine/APIs/OSSL_Api.cs
	addFunction(10.f, 0.f, dummy_func, "osReturnObject", NULL, "k");
	addFunction(10.f, 0.f, dummy_func, "osReturnObjects", NULL, "f");
	addFunction(10.f, 0.f, dummy_func, "osShutDown", NULL, NULL);
	addFunction(10.f, 0.f, dummy_func, "osAddAgentToGroup", NULL, "kss");
	addFunction(10.f, 0.f, dummy_func, "osRezObject", NULL, "svvriiiii");
	/// Aurora-only functions
	/// Reference: https://github.com/aurora-sim/Aurora-Sim/blob/master/Aurora/AuroraDotNetEngine/APIs/AA_API.cs
	addFunction(10.f, 0.f, dummy_func, "aaSetCloudDensity", NULL, "f");
	addFunction(10.f, 0.f, dummy_func, "aaUpdateDatabase", NULL, "sss");
	addFunction(10.f, 0.f, dummy_func, "aaQueryDatabase", "l", "ss");
	addFunction(10.f, 0.f, dummy_func, "aaSerializeXML", "s", "ll");
	addFunction(10.f, 0.f, dummy_func, "aaDeserializeXMLKeys","l", "s");
	addFunction(10.f, 0.f, dummy_func, "aaDeserializeXMLValues", "l", "s");
	addFunction(10.f, 0.f, dummy_func, "aaSetConeOfSilence", NULL, "f");
	addFunction(10.f, 0.f, dummy_func, "aaJoinCombatTeam", NULL, "ks");
	addFunction(10.f, 0.f, dummy_func, "aaLeaveCombat", NULL, "k");
	addFunction(10.f, 0.f, dummy_func, "aaJoinCombat", NULL, "k");
	addFunction(10.f, 0.f, dummy_func, "aaGetHealth", "f", "k");
	addFunction(10.f, 0.f, dummy_func, "aaGetTeam", "s", "k");
	addFunction(10.f, 0.f, dummy_func, "aaGetTeamMembers", "l", "s");
	addFunction(10.f, 0.f, dummy_func, "aaGetLastOwner", "s", NULL);
	addFunction(10.f, 0.f, dummy_func, "aaSayDistance", NULL, "ifs");
	addFunction(10.f, 0.f, dummy_func, "aaSayTo", NULL, "ss");
	addFunction(10.f, 0.f, dummy_func, "aaGetText", "s", NULL);
	addFunction(10.f, 0.f, dummy_func, "aaGetTextColor", "r", NULL);
	addFunction(10.f, 0.f, dummy_func, "aaRaiseError", NULL, "s");
	addFunction(10.f, 0.f, dummy_func, "aaFreezeAvatar", NULL, "s");
	addFunction(10.f, 0.f, dummy_func, "aaThawAvatar", NULL, "s");
	addFunction(10.f, 0.f, dummy_func, "aaRequestCombatPermission", NULL, "s");
	addFunction(10.f, 0.f, dummy_func, "aaGetWalkDisabled", "i", "s");
	addFunction(10.f, 0.f, dummy_func, "aaSetWalkDisabled", NULL, "si");
	addFunction(10.f, 0.f, dummy_func, "aaGetFlyDisabled", "i", "s");
	addFunction(10.f, 0.f, dummy_func, "aaSetFlyDisabled", NULL, "sf");
	addFunction(10.f, 0.f, dummy_func, "aaAvatarFullName2Key", "s", "s");
	addFunction(10.f, 0.f, dummy_func, "aaSetEnv", NULL, "sl");
	addFunction(10.f, 0.f, dummy_func, "aaGetIsInfiniteRegion", "i", NULL);
	addFunction(10.f, 0.f, dummy_func, "aaAllRegionInstanceSay", NULL, "is");
	addFunction(10.f, 0.f, dummy_func, "aaWindlightGetSceneIsStatic", "i", NULL);
	addFunction(10.f, 0.f, dummy_func, "aaWindlightGetSceneDayCycleKeyFrameCount", "i", NULL);
	addFunction(10.f, 0.f, dummy_func, "aaWindlightGetDayCycle", "l", NULL);
	addFunction(10.f, 0.f, dummy_func, "aaWindlightAddDayCycleFrame", "i", "fi");
	addFunction(10.f, 0.f, dummy_func, "aaWindlightRemoveDayCycleFrame", "i", "i");
	addFunction(10.f, 0.f, dummy_func, "aaWindlightGetScene", "l", "il");
	addFunction(10.f, 0.f, dummy_func, "aaWindlightSetScene", "i", "il");
	/// Aurora bot functions
	/// Reference: https://github.com/aurora-sim/Aurora-Sim/blob/master/Aurora/BotManager/Bot_API.cs
	addFunction(10.f, 0.f, dummy_func, "botCreateBot", "s", "sssv");
	addFunction(10.f, 0.f, dummy_func, "botGetWaitingTime", "v", "i");
	addFunction(10.f, 0.f, dummy_func, "botPauseMovement", NULL, "s");
	addFunction(10.f, 0.f, dummy_func, "botResumeMovement", NULL, "s");
	addFunction(10.f, 0.f, dummy_func, "botSetShouldFly", NULL, "si");
	addFunction(10.f, 0.f, dummy_func, "botSetMap", NULL, "slii");
	addFunction(10.f, 0.f, dummy_func, "botRemoveBot", NULL, "s");
	addFunction(10.f, 0.f, dummy_func, "botFollowAvatar", NULL, "ssff");
	addFunction(10.f, 0.f, dummy_func, "botStopFollowAvatar", NULL, "s");
	addFunction(10.f, 0.f, dummy_func, "botSendChatMessage", NULL, "ssii");
	addFunction(10.f, 0.f, dummy_func, "botSendIM",NULL,"sss");
	addFunction(10.f, 0.f, dummy_func, "botTouchObject", NULL, "ss");
	addFunction(10.f, 0.f, dummy_func, "botSitObject", NULL, "ssv");
	addFunction(10.f, 0.f, dummy_func, "botStandUp", NULL, "s");
	addFunction(10.f, 0.f, dummy_func, "botAddTag", NULL, "ss");
	addFunction(10.f, 0.f, dummy_func, "botGetBotsWithTag", "l", "s");
	addFunction(10.f, 0.f, dummy_func, "botRemoveBotsWithTag", NULL, "s");
#endif // HAS_OPENSIM_SUPPORT
// </FS:CR> Opensim/Aurora scripting functions
}

LLScriptLibraryFunction::LLScriptLibraryFunction(F32 eu, F32 st, void (*exec_func)(LLScriptLibData *, LLScriptLibData *, const LLUUID &), const char *name, const char *ret_type, const char *args, BOOL god_only)
		: mEnergyUse(eu), mSleepTime(st), mExecFunc(exec_func), mName(name), mReturnType(ret_type), mArgs(args), mGodOnly(god_only)
{
}

LLScriptLibraryFunction::~LLScriptLibraryFunction()
{
}

void LLScriptLibrary::addFunction(F32 eu, F32 st, void (*exec_func)(LLScriptLibData *, LLScriptLibData *, const LLUUID &), const char *name, const char *ret_type, const char *args, BOOL god_only)
{
	LLScriptLibraryFunction func(eu, st, exec_func, name, ret_type, args, god_only);
	mFunctions.push_back(func);
}

void LLScriptLibrary::assignExec(const char *name, void (*exec_func)(LLScriptLibData *, LLScriptLibData *, const LLUUID &))
{
	for (std::vector<LLScriptLibraryFunction>::iterator i = mFunctions.begin(); 
		 i != mFunctions.end(); ++i)
	{
		if (!strcmp(name, i->mName))
		{
			i->mExecFunc = exec_func;
			return;
		}
	}
	
	llerrs << "Unknown LSL function in assignExec: " << name << llendl;
}

void LLScriptLibData::print(std::ostream &s, BOOL b_prepend_comma)
{
	char tmp[1024];	/*Flawfinder: ignore*/
	if (b_prepend_comma)
	{
	        s << ", ";
	}
	switch (mType)
    {
	case LST_INTEGER:
	     s << mInteger;
	     break;
	case LST_FLOATINGPOINT:
	     snprintf(tmp, 1024, "%f", mFP);	/* Flawfinder: ignore */
	     s << tmp;
	     break;
	case LST_KEY:
	     s << mKey;
	     break;
	case LST_STRING:
	     s << mString;
	     break;
	case LST_VECTOR:
	     snprintf(tmp, 1024, "<%f, %f, %f>", mVec.mV[VX], /* Flawfinder: ignore */
		      mVec.mV[VY], mVec.mV[VZ]);
	     s << tmp;
	     break;
	case LST_QUATERNION:
	     snprintf(tmp, 1024, "<%f, %f, %f, %f>", mQuat.mQ[VX], mQuat.mQ[VY], /* Flawfinder: ignore */
		      mQuat.mQ[VZ], mQuat.mQ[VS]);
	     s << tmp;
	     break;
	default:
	     break;
	}
}

void LLScriptLibData::print_separator(std::ostream& ostr, BOOL b_prepend_sep, char* sep)
{
	if (b_prepend_sep)
	{
		ostr << sep;
	}
	//print(ostr, FALSE);
	{
		char tmp[1024];	/* Flawfinder: ignore */
		switch (mType)
		{
		case LST_INTEGER:
		     ostr << mInteger;
		     break;
		case LST_FLOATINGPOINT:
		     snprintf(tmp, 1024, "%f", mFP);	/* Flawfinder: ignore */
		     ostr << tmp;
		     break;
		case LST_KEY:
		     ostr << mKey;
		     break;
		case LST_STRING:
		     ostr << mString;
		     break;
		case LST_VECTOR:
		     snprintf(tmp, 1024, "<%f, %f, %f>", mVec.mV[VX], /* Flawfinder: ignore */
			      mVec.mV[VY], mVec.mV[VZ]);
		     ostr << tmp;
		     break;
		case LST_QUATERNION:
		     snprintf(tmp, 1024, "<%f, %f, %f, %f>", mQuat.mQ[VX], mQuat.mQ[VY], /* Flawfinder: ignore */
			      mQuat.mQ[VZ], mQuat.mQ[VS]);
		     ostr << tmp;
		     break;
		default:
		     break;
		}
	}
}


LLScriptLibrary gScriptLibrary;
