/** 
 *
 * Copyright (c) 2009-2011, Kitty Barnett
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

#ifndef RLV_COMMON_H
#define RLV_COMMON_H

#include "llavatarname.h"
#include "llselectmgr.h"
#include "llviewercontrol.h"

#include "rlvdefines.h"

#ifdef LL_WINDOWS
	#pragma warning (push)
	#pragma warning (disable : 4702) // warning C4702: unreachable code
#endif
#include <boost/variant.hpp>
#ifdef LL_WINDOWS
	#pragma warning (pop)
#endif

// ============================================================================
// Forward declarations
//

//
// General viewer source
//
class LLInventoryItem;
class LLUICtrl;
class LLViewerInventoryCategory;
class LLViewerInventoryItem;
class LLViewerJointAttachment;
class LLViewerWearable;
class LLWearable;

//
// RLVa-specific
//
class RlvCommand;
typedef std::list<RlvCommand> rlv_command_list_t;
class RlvObject;

struct RlvException;
typedef boost::variant<std::string, LLUUID, S32, ERlvBehaviour> RlvExceptionOption;
typedef boost::variant<int, float, LLVector3, LLUUID> RlvBehaviourModifierValue;

class RlvGCTimer;

// ============================================================================
// RlvSettings
//

template<typename T> inline T rlvGetSetting(const std::string& strSetting, const T& defaultValue)
{
	RLV_ASSERT_DBG(gSavedSettings.controlExists(strSetting));
	return (gSavedSettings.controlExists(strSetting)) ? gSavedSettings.get<T>(strSetting) : defaultValue;
}

template<typename T> inline T rlvGetPerUserSetting(const std::string& strSetting, const T& defaultValue)
{
	RLV_ASSERT_DBG(gSavedPerAccountSettings.controlExists(strSetting));
	return (gSavedPerAccountSettings.controlExists(strSetting)) ? gSavedPerAccountSettings.get<T>(strSetting) : defaultValue;
}

class RlvSettings
{
public:
	static bool getDebug()						{ return rlvGetSetting<bool>(RLV_SETTING_DEBUG, false); }
	static bool getCanOOC()						{ return s_fCanOOC; }
	static bool getForbidGiveToRLV()			{ return rlvGetSetting<bool>(RLV_SETTING_FORBIDGIVETORLV, true); }
	static bool getNoSetEnv()					{ return s_fNoSetEnv; }

	static std::string getWearAddPrefix()		{ return rlvGetSetting<std::string>(RLV_SETTING_WEARADDPREFIX, LLStringUtil::null); }
	static std::string getWearReplacePrefix()	{ return rlvGetSetting<std::string>(RLV_SETTING_WEARREPLACEPREFIX, LLStringUtil::null); }

	static bool getDebugHideUnsetDup()			{ return rlvGetSetting<bool>(RLV_SETTING_DEBUGHIDEUNSETDUP, false); }
	#ifdef RLV_EXPERIMENTAL_COMPOSITEFOLDERS
	static BOOL getEnableComposites()			{ return s_fCompositeFolders; }
	#endif // RLV_EXPERIMENTAL_COMPOSITEFOLDERS
	static bool getEnableIMQuery()              { return rlvGetSetting<bool>("RLVaEnableIMQuery", true); }
	static bool getEnableLegacyNaming()			{ return s_fLegacyNaming; }
	static bool getEnableSharedWear()			{ return rlvGetSetting<bool>(RLV_SETTING_ENABLESHAREDWEAR, false); }
	static bool getEnableTemporaryAttachments() { return s_fTempAttach; }
	static bool getHideLockedLayers()			{ return rlvGetSetting<bool>(RLV_SETTING_HIDELOCKEDLAYER, false); }
	static bool getHideLockedAttach()			{ return rlvGetSetting<bool>(RLV_SETTING_HIDELOCKEDATTACH, false); }
	static bool getHideLockedInventory()		{ return rlvGetSetting<bool>(RLV_SETTING_HIDELOCKEDINVENTORY, false); }
	static bool getSharedInvAutoRename()		{ return rlvGetSetting<bool>(RLV_SETTING_SHAREDINVAUTORENAME, true); }

	static bool getLoginLastLocation()			{ return rlvGetPerUserSetting<bool>(RLV_SETTING_LOGINLASTLOCATION, true); }
	static void updateLoginLastLocation();

	static void initCompatibilityMode(std::string strCompatList);
	static bool isCompatibilityModeObject(const LLUUID& idRlvObject);

	static void initClass();
	static void onChangedSettingMain(const LLSD& sdValue);
protected:
	static bool onChangedMenuLevel();
	static bool onChangedSettingBOOL(const LLSD& sdValue, bool* pfSetting);

	#ifdef RLV_EXPERIMENTAL_COMPOSITEFOLDERS
	static BOOL s_fCompositeFolders;
	#endif // RLV_EXPERIMENTAL_COMPOSITEFOLDERS

	/*
	 * Member variables
	 */
protected:
	static bool s_fCanOOC;
	static bool s_fLegacyNaming;
	static bool s_fNoSetEnv;
	static bool s_fTempAttach;
	static std::list<LLUUID>      s_CompatItemCreators;
	static std::list<std::string> s_CompatItemNames;
};

// ============================================================================
// RlvStrings
//

class RlvStrings
{
public:
	static void initClass();
	static void loadFromFile(const std::string& strFilePath, bool fDefault);
	static void saveToFile(const std::string& strFilePath);

	static const std::string& getAnonym(const LLAvatarName& avName);		// @shownames
	static const std::string& getAnonym(const std::string& strName);		// @shownames
	static const std::string& getString(const std::string& strStringName);
	static const char*        getStringFromReturnCode(ERlvCmdRet eRet);
	static const std::string& getStringMapPath() { return m_StringMapPath; }
	static std::string        getVersion(const LLUUID& idRlvObject, bool fLegacy = false);
	static std::string        getVersionAbout();
	static std::string        getVersionNum(const LLUUID& idRlvObject);
	static bool               hasString(const std::string& strStringName, bool fCheckCustom = false);
	static void               setCustomString(const std::string& strStringName, const std::string& strStringValue);

protected:
	static std::vector<std::string> m_Anonyms;
	typedef std::map<std::string, std::list<std::string> > string_map_t;
	static string_map_t m_StringMap;
	static std::string  m_StringMapPath;
};

// ============================================================================
// RlvUtil - Collection of (static) helper functions
//

class RlvUtil
{
public:
	static bool isEmote(const std::string& strUTF8Text);
	static bool isNearbyAgent(const LLUUID& idAgent);								// @shownames
	static bool isNearbyRegion(const std::string& strRegion);						// @showloc

	static void filterLocation(std::string& strUTF8Text);							// @showloc
	static void filterNames(std::string& strUTF8Text, bool fFilterLegacy = true, bool fClearMatches = false);	// @shownames
	static void filterScriptQuestions(S32& nQuestions, LLSD& sdPayload); 

	static bool isForceTp()	{ return m_fForceTp; }
	static void forceTp(const LLVector3d& posDest);									// Ignores restrictions that might otherwise prevent tp'ing

	static void notifyBlocked(const std::string& strNotifcation, const LLSD& sdArgs = LLSD());
	static void notifyBlockedGeneric()	{ notifyBlocked(RLV_STRING_BLOCKED_GENERIC); }
	static void notifyBlockedViewXXX(LLAssetType::EType assetType) { notifyBlocked(RLV_STRING_BLOCKED_VIEWXXX, LLSD().with("[TYPE]", LLAssetType::lookup(assetType))); }
	static void notifyFailedAssertion(const std::string& strAssert, const std::string& strFile, int nLine);

	static void sendBusyMessage(const LLUUID& idTo, const std::string& strMsg, const LLUUID& idSession = LLUUID::null);
	static bool isValidReplyChannel(S32 nChannel, bool fLoopback = false);
	static bool sendChatReply(S32 nChannel, const std::string& strUTF8Text);
	static bool sendChatReply(const std::string& strChannel, const std::string& strUTF8Text);

	static void teleportCallback(U64 hRegion, const LLVector3& posRegion, const LLVector3& vecLookAt);
protected:
	static bool m_fForceTp;															// @standtp
};

// ============================================================================
// Extensibility classes
//

class RlvExtCommandHandler
{
public:
	virtual ~RlvExtCommandHandler() {}
	virtual bool onAddRemCommand(const RlvCommand& rlvCmd, ERlvCmdRet& cmdRet) { return false; }
	virtual bool onClearCommand(const RlvCommand& rlvCmd, ERlvCmdRet& cmdRet)  { return false; }
	virtual bool onReplyCommand(const RlvCommand& rlvCmd, ERlvCmdRet& cmdRet)  { return false; }
	virtual bool onForceCommand(const RlvCommand& rlvCmd, ERlvCmdRet& cmdRet)  { return false; }
};
typedef bool (RlvExtCommandHandler::*rlvExtCommandHandler)(const RlvCommand& rlvCmd, ERlvCmdRet& cmdRet);

// ============================================================================
// Generic menu enablers
//

bool rlvMenuMainToggleVisible(LLUICtrl* pMenuItem);
void rlvMenuToggleVisible();
bool rlvMenuCanShowName();
bool rlvMenuEnableIfNot(const LLSD& sdParam);

// ============================================================================
// Selection functors
//

bool rlvCanDeleteOrReturn();
bool rlvCanDeleteOrReturn(const LLViewerObject* pObj);

struct RlvSelectHasLockedAttach : public LLSelectedNodeFunctor
{
	RlvSelectHasLockedAttach() {}
	virtual bool apply(LLSelectNode* pNode);
};

// Filters out selected objects that can't be editable (i.e. getFirstNode() will return NULL if the selection is fully editable)
struct RlvSelectIsEditable : public LLSelectedNodeFunctor
{
	RlvSelectIsEditable() {}
	/*virtual*/ bool apply(LLSelectNode* pNode);
};

struct RlvSelectIsSittingOn : public LLSelectedNodeFunctor
{
	RlvSelectIsSittingOn(const LLVOAvatar* pAvatar) : m_pAvatar(pAvatar) {}
	/*virtual*/ bool apply(LLSelectNode* pNode);
protected:
	const LLVOAvatar* m_pAvatar;
};

// ============================================================================
// Predicates
//

bool rlvPredCanWearItem(const LLViewerInventoryItem* pItem, ERlvWearMask eWearMask);
bool rlvPredCanNotWearItem(const LLViewerInventoryItem* pItem, ERlvWearMask eWearMask);
bool rlvPredCanRemoveItem(const LLUUID& idItem);
bool rlvPredCanRemoveItem(const LLViewerInventoryItem* pItem);
bool rlvPredCanNotRemoveItem(const LLUUID& idItem);
bool rlvPredCanNotRemoveItem(const LLViewerInventoryItem* pItem);

struct RlvPredCanWearItem
{
	RlvPredCanWearItem(ERlvWearMask eWearMask) : m_eWearMask(eWearMask) {}
	bool operator()(const LLViewerInventoryItem* pItem) { return rlvPredCanWearItem(pItem, m_eWearMask); }
protected:
	ERlvWearMask m_eWearMask;
};

struct RlvPredCanNotWearItem
{
	RlvPredCanNotWearItem(ERlvWearMask eWearMask) : m_eWearMask(eWearMask) {}
	bool operator()(const LLViewerInventoryItem* pItem) { return rlvPredCanNotWearItem(pItem, m_eWearMask); }
protected:
	ERlvWearMask m_eWearMask;
};

struct RlvPredCanRemoveItem
{
	RlvPredCanRemoveItem() {}
	bool operator()(const LLViewerInventoryItem* pItem) { return rlvPredCanRemoveItem(pItem); }
};

struct RlvPredCanNotRemoveItem
{
	RlvPredCanNotRemoveItem() {}
	bool operator()(const LLViewerInventoryItem* pItem) { return rlvPredCanNotRemoveItem(pItem); }
};

struct RlvPredIsEqualOrLinkedItem
{
	RlvPredIsEqualOrLinkedItem(const LLViewerInventoryItem* pItem) : m_pItem(pItem) {}
	RlvPredIsEqualOrLinkedItem(const LLUUID& idItem);
	bool operator()(const LLViewerInventoryItem* pItem) const;
protected:
	const LLViewerInventoryItem* m_pItem;
};

template<typename T> struct RlvPredValuesEqual
{
	bool operator()(const T* pT2) const { return (pT1) && (pT2) && (*pT1 == *pT2); }
	const T* pT1;
};

// ============================================================================
// Inlined class member functions
//

// Checked: 2010-10-31 (RLVa-1.2.2a) | Added: RLVa-1.2.2a
inline const std::string& RlvStrings::getAnonym(const LLAvatarName& avName)
{
	return getAnonym(avName.getDisplayName());
}

// Checked: 2010-03-26 (RLVa-1.2.0b) | Modified: RLVa-1.0.2a
inline bool RlvUtil::isEmote(const std::string& strUTF8Text)
{
	return (strUTF8Text.length() > 4) && ( (strUTF8Text.compare(0, 4, "/me ") == 0) || (strUTF8Text.compare(0, 4, "/me'") == 0) );
}

// Checked: 2010-03-09 (RLVa-1.2.0b) | Added: RLVa-1.0.2a
inline bool RlvUtil::isValidReplyChannel(S32 nChannel, bool fLoopback /*=false*/)
{
	return (nChannel > ((!fLoopback) ? 0 : -1)) && (CHAT_CHANNEL_DEBUG != nChannel);
}

// Checked: 2009-08-05 (RLVa-1.0.1e) | Added: RLVa-1.0.0e
inline bool RlvUtil::sendChatReply(const std::string& strChannel, const std::string& strUTF8Text)
{
	S32 nChannel;
	return (LLStringUtil::convertToS32(strChannel, nChannel)) ? sendChatReply(nChannel, strUTF8Text) : false;
}

// ============================================================================

#endif // RLV_COMMON_H
