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

	static BOOL getDebugUIEnablers()		{ return rlvGetSettingBOOL(RLV_SETTING_DEBUGUIENABLERS, FALSE); }
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
// Extensibility classes
//

class RlvCommandHandler
{
public:
	virtual ~RlvCommandHandler() {}
	virtual bool onAddRemCommand(const LLUUID& idObj, const RlvCommand& rlvCmd, ERlvCmdRet& cmdRet) { return false; }
	virtual bool onClearCommand(const LLUUID& idObj, const RlvCommand& rlvCmd, ERlvCmdRet& cmdRet)  { return false; }
	virtual bool onReplyCommand(const LLUUID& idObj, const RlvCommand& rlvCmd, ERlvCmdRet& cmdRet)  { return false; }
	virtual bool onForceCommand(const LLUUID& idObj, const RlvCommand& rlvCmd, ERlvCmdRet& cmdRet)  { return false; }
};
typedef bool (RlvCommandHandler::*rlvCommandHandler)(const LLUUID& idObj, const RlvCommand& rlvCmd, ERlvCmdRet& cmdRet);

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

#endif // RLV_COMMON_H
