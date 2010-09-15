/** 
 *
 * Copyright (c) 2009-2010, Kitty Barnett
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

#ifndef RLV_DEFINES_H
#define RLV_DEFINES_H

// ============================================================================
// Extensions
//

// Provides access to "advanced" features through the RLVa debug menu
#define RLV_EXTENSION_FLOATER_RESTRICTIONS	// Enables the Advanced / RLVa / Restrictions... floater
#define RLV_EXTENSION_HIDELOCKED			// "Hide locked layers", "Hide locked attachments" and "Hide locked inventory"

// Extensions
#define RLV_EXTENSION_CMD_GETSETDEBUG_EX	// Extends the debug variables accessible through @getdebug_xxx/@setdebug_xxx
#define RLV_EXTENSION_CMD_FINDFOLDERS		// @findfolders:<option>=<channel> - @findfolder with multiple results
#define RLV_EXTENSION_FLAG_NOSTRIP			// Layers and attachments marked as "nostrip" are exempt from @detach/@remoutfit
#define RLV_EXTENSION_FORCEWEAR_GESTURES	// @attach*/detach* commands also (de)activate gestures
#define RLV_EXTENSION_GIVETORLV_A2A			// Allow "Give to #RLV" on avatar-to-avatar inventory offers
#define RLV_EXTENSION_NOTIFY_BEHAVIOUR		// Provides the option to show a customizable notification whenever a behaviour gets (un)set
#define RLV_EXTENSION_STARTLOCATION			// Reenables "Start Location" at login if not @tploc=n or @unsit=n restricted at last logoff
//#define RLV_EXPERIMENTAL					// Enables/disables experimental features en masse
#define RLV_EXPERIMENTAL_CMDS				// Enables/disables experimental commands en masse

// Experimental features
#ifdef RLV_EXPERIMENTAL
	// Stable (will mature to RLV_EXTENSION_XXX in next release if no bugs are found)

	// Under testing (stable, but requires further testing - safe for public release but may be quirky)
	#define RLV_EXPERIMENTAL_COMPOSITEFOLDERS

	// Under development (don't include in public release)
	#if LL_RELEASE_WITH_DEBUG_INFO || LL_DEBUG
		#define RLV_EXPERIMENTAL_FIRSTUSE				// Enables a number of "first use" popups
	#endif // LL_RELEASE_WITH_DEBUG_INFO || LL_DEBUG
#endif // RLV_EXPERIMENTAL

// Experimental commands (not part of the RLV API spec, disabled on public releases)
#ifdef RLV_EXPERIMENTAL_CMDS
	#define RLV_EXTENSION_CMD_ALLOWIDLE		// Forces "Away" status when idle (effect is the same as setting AllowIdleAFK to TRUE)
//	#define RLV_EXTENSION_CMD_GETXXXNAMES	// @get[add|rem]attachnames:<option>=<channel> and @get[add|rem]outfitnames=<channel>
	#define RLV_EXTENSION_CMD_INTERACT		// @interact=n
	#define RLV_EXTENSION_CMD_TOUCHXXX		// @touch:uuid=n|y, @touchworld[:<uuid>]=n|y, @touchattach[:<uuid>]=n|y, @touchud[:<uuid>]=n|y
#endif // RLV_EXPERIMENTAL_CMDS

// ============================================================================
// Defines
//

// Version of the specifcation we support
const S32 RLV_VERSION_MAJOR = 2;
const S32 RLV_VERSION_MINOR = 1;
const S32 RLV_VERSION_PATCH = 2;
const S32 RLV_VERSION_BUILD = 0;

// Implementation version
const S32 RLVa_VERSION_MAJOR = 1;
const S32 RLVa_VERSION_MINOR = 2;
const S32 RLVa_VERSION_PATCH = 1;
const S32 RLVa_VERSION_BUILD = 2;

// Uncomment before a final release
//#define RLV_RELEASE

// The official viewer version we're patching against
#define RLV_MAKE_TARGET(x, y, z)	((x << 16) | (y << 8) | z)
#define RLV_TARGET					RLV_MAKE_TARGET(2, 1, 2)

// Defining these makes it easier if we ever need to change our tag
#define RLV_WARNS		LL_WARNS("RLV")
#define RLV_INFOS		LL_INFOS("RLV")
#define RLV_DEBUGS		LL_DEBUGS("RLV")
#define RLV_ENDL		LL_ENDL
#define RLV_VERIFY(f)	if (!(f)) { RlvUtil::notifyFailedAssertion(#f, __FILE__, __LINE__); }

#if LL_RELEASE_WITH_DEBUG_INFO || LL_DEBUG
	// Turn on extended debugging information
	#define RLV_DEBUG
	// Make sure we halt execution on errors
	#define RLV_ERRS				LL_ERRS("RLV")
	// Keep our asserts separate from LL's
	#define RLV_ASSERT(f)			if (!(f)) { RLV_ERRS << "ASSERT (" << #f << ")" << RLV_ENDL; }
	#define RLV_ASSERT_DBG(f)		RLV_ASSERT(f)
#else
	// Don't halt execution on errors in release
	#define RLV_ERRS				LL_WARNS("RLV")
	// We don't want to check assertions in release builds
	#ifdef RLV_RELEASE
		#define RLV_ASSERT(f)		RLV_VERIFY(f)
		#define RLV_ASSERT_DBG(f)
	#else
		#define RLV_ASSERT(f)
		#define RLV_ASSERT_DBG(f)
	#endif // RLV_RELEASE
#endif // LL_RELEASE_WITH_DEBUG_INFO || LL_DEBUG

#define RLV_ROOT_FOLDER					"#RLV"
#define RLV_CMD_PREFIX					'@'
#define RLV_PUTINV_PREFIX				"#RLV/~"
#define RLV_SETROT_OFFSET				F_PI_BY_TWO		// @setrot is off by 90° with the rest of SL

#define RLV_FOLDER_FLAG_NOSTRIP			"nostrip"
#define RLV_FOLDER_PREFIX_HIDDEN		'.'
#define RLV_FOLDER_PREFIX_PUTINV    	'~'

// ============================================================================
// Enumeration declarations
//

// NOTE: any changes to this enumeration should be reflected in RlvCommand::initLookupTable()
enum ERlvBehaviour {
	RLV_BHVR_DETACH = 0,			// "detach"
	RLV_BHVR_ATTACH,				// "attach"
	RLV_BHVR_ADDATTACH,				// "addattach"
	RLV_BHVR_REMATTACH,				// "remattach"
	RLV_BHVR_ADDOUTFIT,				// "addoutfit"
	RLV_BHVR_REMOUTFIT,				// "remoutfit"
	RLV_BHVR_EMOTE,					// "emote"
	RLV_BHVR_SENDCHAT,				// "sendchat"
	RLV_BHVR_RECVCHAT,				// "recvchat"
	RLV_BHVR_RECVEMOTE,				// "recvemote"
	RLV_BHVR_REDIRCHAT,				// "redirchat"
	RLV_BHVR_REDIREMOTE,			// "rediremote"
	RLV_BHVR_CHATWHISPER,			// "chatwhisper"
	RLV_BHVR_CHATNORMAL,			// "chatnormal"
	RLV_BHVR_CHATSHOUT,				// "chatshout"
	RLV_BHVR_SENDCHANNEL,			// "sendchannel"
	RLV_BHVR_SENDIM,				// "sendim"
	RLV_BHVR_RECVIM,				// "recvim"
	RLV_BHVR_PERMISSIVE,			// "permissive"
	RLV_BHVR_NOTIFY,				// "notify"
	RLV_BHVR_SHOWINV,				// "showinv"
	RLV_BHVR_SHOWMINIMAP,			// "showminimap"
	RLV_BHVR_SHOWWORLDMAP,			// "showworldmap"
	RLV_BHVR_SHOWLOC,				// "showloc"
	RLV_BHVR_SHOWNAMES,				// "shownames"
	RLV_BHVR_SHOWHOVERTEXT,			// "showhovertext"
	RLV_BHVR_SHOWHOVERTEXTHUD,		// "showhovertexthud"
	RLV_BHVR_SHOWHOVERTEXTWORLD,	// "showhovertextworld"
	RLV_BHVR_SHOWHOVERTEXTALL,		// "showhovertextall"
	RLV_BHVR_TPLM,					// "tplm"
	RLV_BHVR_TPLOC,					// "tploc"
	RLV_BHVR_TPLURE,				// "tplure"
	RLV_BHVR_VIEWNOTE,				// "viewnote"
	RLV_BHVR_VIEWSCRIPT,			// "viewscript"
	RLV_BHVR_VIEWTEXTURE,			// "viewtexture"
	RLV_BHVR_ACCEPTPERMISSION,		// "acceptpermission"
	RLV_BHVR_ACCEPTTP,				// "accepttp"
	RLV_BHVR_ALLOWIDLE,				// "allowidle"
	RLV_BHVR_EDIT,					// "edit"
	RLV_BHVR_REZ,					// "rez"
	RLV_BHVR_FARTOUCH,				// "fartouch"
	RLV_BHVR_INTERACT,				// "interact"
	RLV_BHVR_TOUCH,					// "touch"
	RLV_BHVR_TOUCHATTACH,			// "touchattach"
	RLV_BHVR_TOUCHHUD,				// "touchhud"
	RLV_BHVR_TOUCHWORLD,			// "touchworld"
	RLV_BHVR_FLY,					// "fly"
	RLV_BHVR_UNSIT,					// "unsit"
	RLV_BHVR_SIT,					// "sit"
	RLV_BHVR_SITTP,					// "sittp"
	RLV_BHVR_STANDTP,				// "standtp"
	RLV_BHVR_SETDEBUG,				// "setdebug"
	RLV_BHVR_SETENV,				// "setenv"
	RLV_BHVR_DETACHME,				// "detachme"
	RLV_BHVR_ATTACHOVER,			// "attachover"
	RLV_BHVR_ATTACHTHIS,			// "attachthis"
	RLV_BHVR_ATTACHTHISOVER,		// "attachthisover"
	RLV_BHVR_DETACHTHIS,			// "detachthis"
	RLV_BHVR_ATTACHALL,				// "attachall"
	RLV_BHVR_ATTACHALLOVER,			// "attachallover"
	RLV_BHVR_DETACHALL,				// "detachall"
	RLV_BHVR_ATTACHALLTHIS,			// "attachallthis"
	RLV_BHVR_ATTACHALLTHISOVER,		// "attachallthisover"
	RLV_BHVR_DETACHALLTHIS,			// "detachallthis"
	RLV_BHVR_TPTO,					// "tpto"
	RLV_BHVR_VERSION,				// "version"
	RLV_BHVR_VERSIONNEW,			// "versionnew"
	RLV_BHVR_VERSIONNUM,			// "versionnum"
	RLV_BHVR_GETATTACH,				// "getattach"
	RLV_BHVR_GETATTACHNAMES,		// "getattachnames"
	RLV_BHVR_GETADDATTACHNAMES,		// "getaddattachnames"
	RLV_BHVR_GETREMATTACHNAMES,		// "getremattachnames"
	RLV_BHVR_GETOUTFIT,				// "getoutfit"
	RLV_BHVR_GETOUTFITNAMES,		// "getoutfitnames"
	RLV_BHVR_GETADDOUTFITNAMES,		// "getaddoutfitnames"
	RLV_BHVR_GETREMOUTFITNAMES,		// "getremoutfitnames"
	RLV_BHVR_FINDFOLDER,			// "findfolder"
	RLV_BHVR_FINDFOLDERS,			// "findfolders"
	RLV_BHVR_GETPATH,				// "getpath"
	RLV_BHVR_GETPATHNEW,			// "getpathnew"
	RLV_BHVR_GETINV,				// "getinv"
	RLV_BHVR_GETINVWORN,			// "getinvworn"
	RLV_BHVR_GETSITID,				// "getsitid"
	RLV_BHVR_GETSTATUS,				// "getstatus"
	RLV_BHVR_GETSTATUSALL,			// "getstatusall"

	RLV_BHVR_COUNT,
	RLV_BHVR_UNKNOWN
};

enum ERlvParamType {
	RLV_TYPE_UNKNOWN,
	RLV_TYPE_ADD,					// <param> == "n"|"add"
	RLV_TYPE_REMOVE,				// <param> == "y"|"rem"
	RLV_TYPE_FORCE,					// <param> == "force"
	RLV_TYPE_REPLY,					// <param> == <number>
	RLV_TYPE_CLEAR
};

enum ERlvCmdRet {
	RLV_RET_UNKNOWN     = 0x0000,	// Unknown error (should only be used internally)
	RLV_RET_RETAINED,				// Command was retained
	RLV_RET_SUCCESS     = 0x0100,	// Command executed succesfully
	RLV_RET_SUCCESS_UNSET,			// Command executed succesfully (RLV_TYPE_REMOVE for an unrestricted behaviour)
	RLV_RET_SUCCESS_DUPLICATE,		// Command executed succesfully (RLV_TYPE_ADD for an already restricted behaviour)
	RLV_RET_FAILED      = 0x0200,	// Command failed (general failure)
	RLV_RET_FAILED_SYNTAX,			// Command failed (syntax error)
	RLV_RET_FAILED_OPTION,			// Command failed (invalid option)
	RLV_RET_FAILED_PARAM,			// Command failed (invalid param)
	RLV_RET_FAILED_LOCK,			// Command failed (command is locked by another object)
	RLV_RET_FAILED_DISABLED,		// Command failed (command disabled by user)
	RLV_RET_FAILED_UNKNOWN,			// Command failed (unknown command)
	RLV_RET_FAILED_NOSHAREDROOT,	// Command failed (missing #RLV)
};

enum ERlvExceptionCheck
{
	RLV_CHECK_PERMISSIVE,			// Exception can be set by any object
	RLV_CHECK_STRICT,				// Exception must be set by all objects holding the restriction
	RLV_CHECK_DEFAULT				// Permissive or strict will be determined by currently enforced restrictions
};

enum ERlvLockMask
{
	RLV_LOCK_ADD    = 0x01,
	RLV_LOCK_REMOVE = 0x02,
	RLV_LOCK_ANY    = RLV_LOCK_ADD | RLV_LOCK_REMOVE
};

enum ERlvWearMask
{
	RLV_WEAR_LOCKED  = 0x00,		// User can not wear the item at all
	RLV_WEAR_ADD     = 0x01,		// User can wear the item in addition to what's already worn
	RLV_WEAR_REPLACE = 0x02,		// User can wear the item and replace what's currently worn
	RLV_WEAR         = 0x03			// Convenience: combines RLV_WEAR_ADD and RLV_WEAR_REPLACE
};

enum ERlvAttachGroupType
{
	RLV_ATTACHGROUP_HEAD = 0,
	RLV_ATTACHGROUP_TORSO,
	RLV_ATTACHGROUP_ARMS,
	RLV_ATTACHGROUP_LEGS,
	RLV_ATTACHGROUP_HUD,
	RLV_ATTACHGROUP_COUNT,
	RLV_ATTACHGROUP_INVALID
};

// ============================================================================
// Settings
//

#define RLV_SETTING_MAIN				"RestrainedLove"
#define RLV_SETTING_DEBUG				"RestrainedLoveDebug"
#define RLV_SETTING_NOSETENV			"RestrainedLoveNoSetEnv"
#define RLV_SETTING_FORBIDGIVETORLV		"RestrainedLoveForbidGiveToRLV"

#define RLV_SETTING_DEBUGHIDEUNSETDUP   "RLVaDebugHideUnsetDuplicate"
#define RLV_SETTING_ENABLECOMPOSITES	"RLVaEnableCompositeFolders"
#define RLV_SETTING_ENABLELEGACYNAMING	"RLVaEnableLegacyNaming"
#define RLV_SETTING_ENABLESHAREDWEAR	"RLVaEnableSharedWear"
#define RLV_SETTING_HIDELOCKEDLAYER		"RLVaHideLockedLayers"
#define RLV_SETTING_HIDELOCKEDATTACH	"RLVaHideLockedAttachments"
#define RLV_SETTING_HIDELOCKEDINVENTORY	"RLVaHideLockedInventory"
#define RLV_SETTING_LOGINLASTLOCATION	"RLVaLoginLastLocation"
#define RLV_SETTING_SHAREDINVAUTORENAME	"RLVaSharedInvAutoRename"
#define RLV_SETTING_SHOWNAMETAGS		"RLVaShowNameTags"

#define RLV_SETTING_FIRSTUSE_PREFIX		"FirstRLV"
#define RLV_SETTING_FIRSTUSE_GIVETORLV	RLV_SETTING_FIRSTUSE_PREFIX"GiveToRLV"

// ============================================================================
// Strings
//

#define RLV_STRING_HIDDEN					"hidden_generic"
#define RLV_STRING_HIDDEN_PARCEL			"hidden_parcel"
#define RLV_STRING_HIDDEN_REGION			"hidden_region"

#define RLV_STRING_BLOCKED_RECVIM			"blocked_recvim"
#define RLV_STRING_BLOCKED_RECVIM_REMOTE	"blocked_recvim_remote"
#define RLV_STRING_BLOCKED_SENDIM			"blocked_sendim"
#define RLV_STRING_BLOCKED_VIEWXXX			"blocked_viewxxx"
#define RLV_STRING_BLOCKED_TPLURE_REMOTE	"blocked_tplure_remote"

// ============================================================================

#endif // RLV_DEFINES_H
