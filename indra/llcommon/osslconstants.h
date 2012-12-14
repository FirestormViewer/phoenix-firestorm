/*
 * @file osconstants.h
 * @brief Constants used in OSSL and ASL
 *
 * Copyright (c) 2012 Cinder Roxley <cinder@cinderblocks.biz> All rights reserved.
 *
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * Cinder Roxley wrote this file. As long as you retain this notice you can do
 * whatever you want with this stuff. If we meet some day, and you think this
 * stuff is worth it, you can buy me a beer in return <cinder@cinderblocks.biz>
 * ----------------------------------------------------------------------------
 */

#ifndef LL_OSSLCONSTANTS_H
#define LL_OSSLCONSTANTS_H

// <FS:CR> Current as of OpenSimulator 0.7.5 dev and Aurora 0.5.0.1 (13 December 2012)

/// OSSL Constants
/// Reference: https://github.com/opensim/opensim/blob/master/OpenSim/Region/ScriptEngine/Shared/Api/Runtime/LSL_Constants.cs

// osMessageAttachments
const S32 OS_ATTACH_MSG_ALL				= -65535;
const S32 OS_ATTACH_MSG_INVERT_POINTS	= 1;
const S32 OS_ATTACH_MSG_OBJECT_CREATOR	= 2;
const S32 OS_ATTACH_MSG_SCRIPT_CREATOR	= 4;

// llSetCameraParams
const S32 CAMERA_FOCUS_OFFSET_X			= 2;
const S32 CAMERA_FOCUS_OFFSET_Y			= 3;
const S32 CAMERA_FOCUS_OFFSET_Z			= 4;
const S32 CAMERA_POSITION_X				= 14;
const S32 CAMERA_POSITION_Y				= 15;
const S32 CAMERA_POSITION_Z				= 16;
const S32 CAMERA_FOCUS_X				= 18;
const S32 CAMERA_FOCUS_Y				= 19;
const S32 CAMERA_FOCUS_Z				= 20;

// osSetParcelDetails
const S32 PARCEL_DETAILS_CLAIMDATE		= 10;

// osGetRegionStats
const S32 STATS_TIME_DILATION			= 0;
const S32 STATS_SIM_FPS					= 1;
const S32 STATS_PHYSICS_FPS				= 2;
const S32 STATS_AGENT_UPDATES			= 3;
const S32 STATS_ROOT_AGENTS				= 4;
const S32 STATS_CHILD_AGENTS			= 5;
const S32 STATS_TOTAL_PRIMS				= 6;
const S32 STATS_ACTIVE_PRIMS			= 7;
const S32 STATS_FRAME_MS				= 8;
const S32 STATS_NET_MS					= 9;
const S32 STATS_PHYSICS_MS				= 10;
const S32 STATS_IMAGE_MS				= 11;
const S32 STATS_OTHER_MS				= 12;
const S32 STATS_IN_PACKETS_PER_SECOND	= 13;
const S32 STATS_OUT_PACKETS_PER_SECOND	= 14;
const S32 STATS_UNACKED_BYTES			= 15;
const S32 STATS_AGENT_MS				= 16;
const S32 STATS_PENDING_DOWNLOADS		= 17;
const S32 STATS_PENDING_UPLOADS			= 18;
const S32 STATS_ACTIVE_SCRIPTS			= 19;
const S32 STATS_SCRIPT_LPS				= 20;

// osNPC*
const U32 NPC							= 0x20;
const U32 OS_NPC						= 0x01000000;
const S32 OS_NPC_FLY					= 0;
const S32 OS_NPC_NO_FLY					= 1;
const S32 OS_NPC_LAND_AT_TARGET			= 2;
const S32 OS_NPC_RUNNING				= 4;
const S32 OS_NPC_SIT_NOW				= 0;
const U32 OS_NPC_CREATOR_OWNED			= 0x1;
const U32 OS_NPC_NOT_OWNED				= 0x2;
const U32 OS_NPC_SENSE_AS_AGENT			= 0x4;

// osListenRegex
const U32 OS_LISTEN_REGEX_NAME			= 0x1;
const U32 OS_LISTEN_REGEX_MESSAGE		= 0x2;

// changed() event
const S32 CHANGED_ANIMATION				= 16384;

/// Meta7 Lightshare Constants
/// Reference: https://github.com/opensim/opensim/blob/master/OpenSim/Region/ScriptEngine/Shared/Api/Runtime/CM_Constants.cs
const S32 WL_WATER_COLOR				= 0;
const S32 WL_WATER_FOG_DENSITY_EXPONENT	= 1;
const S32 WL_UNDERWATER_FOG_MODIFIER	= 2;
const S32 WL_REFLECTION_WAVELET_SCALE	= 3;
const S32 WL_FRESNEL_SCALE				= 4;
const S32 WL_FRESNEL_OFFSET				= 5;
const S32 WL_REFRACT_SCALE_ABOVE		= 6;
const S32 WL_REFRACT_SCALE_BELOW		= 7;
const S32 WL_BLUR_MULTIPLIER			= 8;
const S32 WL_BIG_WAVE_DIRECTION			= 9;
const S32 WL_LITTLE_WAVE_DIRECTION		= 10;
const S32 WL_NORMAL_MAP_TEXTURE			= 11;
const S32 WL_HORIZON					= 12;
const S32 WL_HAZE_HORIZON				= 13;
const S32 WL_BLUE_DENSITY				= 14;
const S32 WL_HAZE_DENSITY				= 15;
const S32 WL_DENSITY_MULTIPLIER			= 16;
const S32 WL_DISTANCE_MULTIPLIER		= 17;
const S32 WL_MAX_ALTITUDE				= 18;
const S32 WL_SUN_MOON_COLOR				= 19;
const S32 WL_AMBIENT					= 20;
const S32 WL_EAST_ANGLE					= 21;
const S32 WL_SUN_GLOW_FOCUS				= 22;
const S32 WL_SUN_GLOW_SIZE				= 23;
const S32 WL_SCENE_GAMMA				= 24;
const S32 WL_STAR_BRIGHTNESS			= 25;
const S32 WL_CLOUD_COLOR				= 26;
const S32 WL_CLOUD_XY_DENSITY			= 27;
const S32 WL_CLOUD_COVERAGE				= 28;
const S32 WL_CLOUD_SCALE				= 29;
const S32 WL_CLOUD_DETAIL_XY_DENSITY	= 30;
const S32 WL_CLOUD_SCROLL_X				= 31;
const S32 WL_CLOUD_SCROLL_Y				= 32;
const S32 WL_CLOUD_SCROLL_Y_LOCK		= 33;
const S32 WL_CLOUD_SCROLL_X_LOCK		= 34;
const S32 WL_DRAW_CLASSIC_CLOUDS		= 35;
const S32 WL_SUN_MOON_POSITION			= 36;

/// Aurora-Sim Constants
/// Reference: https://github.com/aurora-sim/Aurora-Sim/blob/master/Aurora/AuroraDotNetEngine/APIs/AA_Constants.cs

char const* const ENABLE_GRAVITY			= "enable_gravity";
char const* const GRAVITY_FORCE_X			= "gravity_force_x";
char const* const GRAVITY_FORCE_Y			= "gravity_force_y";
char const* const GRAVITY_FORCE_Z			= "gravity_force_z";
char const* const ADD_GRAVITY_POINT			= "add_gravity_point";
char const* const ADD_GRAVITY_FORCE			= "add_gravity_force";
char const* const START_TIME_REVERSAL_SAVING = "start_time_reversal_saving";
char const* const STOP_TIME_REVERSAL_SAVING	= "stop_time_reversal_saving";
char const* const START_TIME_REVERSAL		= "start_time_reversal";
char const* const STOP_TIME_REVERSAL		= "stop_time_reversal";

//Aurora bot constants
const S32 BOT_FOLLOW_FLAG_NONE				= 0;
const S32 BOT_FOLLOW_FLAG_INDEFINITELY		= 1;
const S32 BOT_FOLLOW_WALK					= 0;
const S32 BOT_FOLLOW_RUN					= 1;
const S32 BOT_FOLLOW_FLY					= 2;
const S32 BOT_FOLLOW_TELEPORT				= 3;
const S32 BOT_FOLLOW_WAIT					= 4;
const S32 BOT_FOLLOW_TRIGGER_HERE_EVENT		= 1;
const S32 BOT_FOLLOW_FLAG_FORCEDIRECTPATH	= 4;

// Aurora Lightshare Constants
const S32 WL_OK								= -1;
const S32 WL_ERROR							= -2;
const S32 WL_ERROR_NO_SCENE_SET				= -3;
const S32 WL_ERROR_SCENE_MUST_BE_STATIC		= -4;
const S32 WL_ERROR_SCENE_MUST_NOT_BE_STATIC	= -5;
const S32 WL_ERROR_BAD_SETTING				= -6;
const S32 WL_ERROR_NO_PRESET_FOUND			= -7;

#endif // LL_OSSLCONSTANTS_H
