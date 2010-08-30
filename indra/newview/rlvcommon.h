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

#ifndef RLV_COMMON_H
#define RLV_COMMON_H

#include "llinventorymodel.h"
#include "llselectmgr.h"
#include "llviewercontrol.h"
#include "llviewerinventory.h"

#include "rlvdefines.h"

// ============================================================================
// Forward declarations
//

class RlvCommand;

// ============================================================================
// RlvSettings
//

inline BOOL rlvGetSettingBOOL(const std::string& strSetting, BOOL fDefault)
{
	RLV_ASSERT_DBG(gSavedSettings.controlExists(strSetting));
	return (gSavedSettings.controlExists(strSetting)) ? gSavedSettings.getBOOL(strSetting) : fDefault;
}
inline BOOL rlvGetPerUserSettingsBOOL(const std::string& strSetting, BOOL fDefault)
{
	RLV_ASSERT_DBG(gSavedPerAccountSettings.controlExists(strSetting));
	return (gSavedPerAccountSettings.controlExists(strSetting)) ? gSavedPerAccountSettings.getBOOL(strSetting) : fDefault;
}

class RlvSettings
{
public:
	static BOOL getDebug()					{ return rlvGetSettingBOOL(RLV_SETTING_DEBUG, FALSE); }
	static BOOL getForbidGiveToRLV()		{ return rlvGetSettingBOOL(RLV_SETTING_FORBIDGIVETORLV, TRUE); }
	static BOOL getNoSetEnv()				{ return fNoSetEnv; }

	static BOOL getDebugHideUnsetDup()		{ return rlvGetSettingBOOL(RLV_SETTING_DEBUGHIDEUNSETDUP, FALSE); }
	#ifdef RLV_EXPERIMENTAL_COMPOSITEFOLDERS
	static BOOL getEnableComposites()		{ return fCompositeFolders; }
	#endif // RLV_EXPERIMENTAL_COMPOSITEFOLDERS
	static BOOL getEnableLegacyNaming()		{ return fLegacyNaming; }
	static BOOL getEnableSharedWear();
	static BOOL getHideLockedLayers()		{ return rlvGetSettingBOOL(RLV_SETTING_HIDELOCKEDLAYER, FALSE); }		
	static BOOL getHideLockedAttach()		{ return rlvGetSettingBOOL(RLV_SETTING_HIDELOCKEDATTACH, FALSE); }
	static BOOL getHideLockedInventory()	{ return rlvGetSettingBOOL(RLV_SETTING_HIDELOCKEDINVENTORY, FALSE); }
	static BOOL getSharedInvAutoRename()	{ return rlvGetSettingBOOL(RLV_SETTING_SHAREDINVAUTORENAME, TRUE); }
	static BOOL getShowNameTags()			{ return fShowNameTags; }

	#ifdef RLV_EXTENSION_STARTLOCATION
	static BOOL getLoginLastLocation()		{ return rlvGetPerUserSettingsBOOL(RLV_SETTING_LOGINLASTLOCATION, TRUE); }
	static void updateLoginLastLocation();
	#endif // RLV_EXTENSION_STARTLOCATION

	static void initClass();
protected:
	static bool onChangedSettingBOOL(const LLSD& sdValue, BOOL* pfSetting);

	#ifdef RLV_EXPERIMENTAL_COMPOSITEFOLDERS
	static BOOL fCompositeFolders;
	#endif // RLV_EXPERIMENTAL_COMPOSITEFOLDERS
	static BOOL fLegacyNaming;
	static BOOL fNoSetEnv;
	static BOOL fShowNameTags;
};

#ifdef RLV_WORKAROUND_REZMULTIPLEATTACH
inline BOOL RlvSettings::getEnableSharedWear()
{
	return FALSE;
}
#endif // RLV_WORKAROUND_REZMULTIPLEATTACH

// ============================================================================
// RlvStrings
//

class RlvStrings
{
public:
	static void initClass();

	static const std::string& getAnonym(const std::string& strName);		// @shownames
	static const std::string& getBehaviourNotificationString(ERlvBehaviour eBhvr, ERlvParamType eType);
	static const std::string& getString(const std::string& strStringName);
	static const char*        getStringFromReturnCode(ERlvCmdRet eRet);
	static std::string        getVersion(bool fLegacy = false);				// @version
	static std::string        getVersionAbout();							// Shown in Help / About
	static std::string        getVersionNum();								// @versionnum
	static bool               hasString(const std::string& strStringName);

protected:
	static std::vector<std::string> m_Anonyms;
	static std::map<std::string, std::string> m_StringMap;
	#ifdef RLV_EXTENSION_NOTIFY_BEHAVIOUR
	static std::map<ERlvBehaviour, std::string> m_BhvrAddMap;
	static std::map<ERlvBehaviour, std::string> m_BhvrRemMap;
	#endif // RLV_EXTENSION_NOTIFY_BEHAVIOUR
};

// ============================================================================
// RlvUtil - Collection of (static) helper functions
//

class RlvUtil
{
public:
	static bool isEmote(const std::string& strUTF8Text);
	static bool isNearbyAgent(const LLUUID& idAgent);						// @shownames
	static bool isNearbyRegion(const std::string& strRegion);				// @showloc

	static void filterLocation(std::string& strUTF8Text);					// @showloc
	static void filterNames(std::string& strUTF8Text);						// @shownames

	static bool isForceTp()	{ return m_fForceTp; }
	static void forceTp(const LLVector3d& posDest);							// Ignores restrictions that might otherwise prevent tp'ing

	static void notifyFailedAssertion(const char* pstrAssert, const char* pstrFile, int nLine);

	static void sendBusyMessage(const LLUUID& idTo, const std::string& strMsg, const LLUUID& idSession = LLUUID::null);
	static bool isValidReplyChannel(S32 nChannel);
	static bool sendChatReply(S32 nChannel, const std::string& strUTF8Text);
	static bool sendChatReply(const std::string& strChannel, const std::string& strUTF8Text);

protected:
	static bool m_fForceTp;													// @standtp
};

// ============================================================================
// Extensibility classes
//

class RlvCommandHandler
{
public:
	virtual ~RlvCommandHandler() {}
	virtual bool onAddRemCommand(const RlvCommand& rlvCmd, ERlvCmdRet& cmdRet) { return false; }
	virtual bool onClearCommand(const RlvCommand& rlvCmd, ERlvCmdRet& cmdRet)  { return false; }
	virtual bool onReplyCommand(const RlvCommand& rlvCmd, ERlvCmdRet& cmdRet)  { return false; }
	virtual bool onForceCommand(const RlvCommand& rlvCmd, ERlvCmdRet& cmdRet)  { return false; }
};
typedef bool (RlvCommandHandler::*rlvCommandHandler)(const RlvCommand& rlvCmd, ERlvCmdRet& cmdRet);

// ============================================================================
// Generic menu enablers
//

bool rlvMenuCheckEnabled();
bool rlvMenuToggleEnabled();
bool rlvMenuEnableIfNot(const LLSD& sdParam);

// ============================================================================
// Selection functors
//

struct RlvSelectHasLockedAttach : public LLSelectedNodeFunctor
{
	RlvSelectHasLockedAttach() {}
	virtual bool apply(LLSelectNode* pNode);
};

struct RlvSelectIsOwnedByOrGroupOwned : public LLSelectedNodeFunctor
{
	RlvSelectIsOwnedByOrGroupOwned(const LLUUID& uuid) : m_idAgent(uuid) {}
	virtual bool apply(LLSelectNode* pNode);
protected:
	LLUUID m_idAgent;
};

struct RlvSelectIsSittingOn : public LLSelectedNodeFunctor
{
	RlvSelectIsSittingOn(LLXform* pObject) : m_pObject(pObject) {}
	virtual bool apply(LLSelectNode* pNode);
protected:
	LLXform* m_pObject;
};

// ============================================================================
// Predicates
//

bool rlvPredIsWearableItem(const LLViewerInventoryItem* pItem);
bool rlvPredIsNotWearableItem(const LLViewerInventoryItem* pItem);
bool rlvPredIsRemovableItem(const LLViewerInventoryItem* pItem);
bool rlvPredIsNotRemovableItem(const LLViewerInventoryItem* pItem);

struct RlvPredIsEqualOrLinkedItem
{
	RlvPredIsEqualOrLinkedItem(const LLViewerInventoryItem* pItem) : m_pItem(pItem) {}
	RlvPredIsEqualOrLinkedItem(const LLUUID& idItem) { m_pItem = gInventory.getItem(idItem); }

	bool operator()(const LLViewerInventoryItem* pItem) const
	{
		return (m_pItem) && (pItem) && (m_pItem->getLinkedUUID() == pItem->getLinkedUUID());
	}
protected:
	const LLViewerInventoryItem* m_pItem;
};

// ============================================================================
// Inlined class member functions
//

// Checked: 2010-03-26 (RLVa-1.2.0b) | Modified: RLVa-1.0.2a
inline bool RlvUtil::isEmote(const std::string& strUTF8Text)
{
	return (strUTF8Text.length() > 4) && ( (strUTF8Text.compare(0, 4, "/me ") == 0) || (strUTF8Text.compare(0, 4, "/me'") == 0) );
}

// Checked: 2010-03-09 (RLVa-1.2.0b) | Added: RLVa-1.0.2a
inline bool RlvUtil::isValidReplyChannel(S32 nChannel)
{
	return (nChannel > 0) && (CHAT_CHANNEL_DEBUG != nChannel);
}

// Checked: 2009-08-05 (RLVa-1.0.1e) | Added: RLVa-1.0.0e
inline bool RlvUtil::sendChatReply(const std::string& strChannel, const std::string& strUTF8Text)
{
	S32 nChannel;
	return (LLStringUtil::convertToS32(strChannel, nChannel)) ? sendChatReply(nChannel, strUTF8Text) : false;
}

// ============================================================================

#endif // RLV_COMMON_H
