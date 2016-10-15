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

#include "llviewerprecompiledheaders.h"
#include "llagent.h"
#include "llagentui.h"
#include "llavatarnamecache.h"
#include "llinstantmessage.h"
#include "llnotificationsutil.h"
#include "llregionhandle.h"
#include "llscriptruntimeperms.h"
#include "llsdserialize.h"
#include "lltrans.h"
#include "llversioninfo.h"
#include "llviewerparcelmgr.h"
#include "llviewermenu.h"
#include "llviewerobjectlist.h"
#include "llviewerregion.h"
#include "llworld.h"

#include "rlvactions.h"
#include "rlvcommon.h"
#include "rlvhelper.h"
#include "rlvhandler.h"
#include "rlvlocks.h"

#include <boost/algorithm/string.hpp>
#include <boost/regex.hpp>


// ============================================================================
// Forward declarations
//

// llviewermenu.cpp
LLVOAvatar* find_avatar_from_object(LLViewerObject* object);

// ============================================================================
// RlvNotifications
//

// Checked: 2009-11-13 (RLVa-1.1.0b) | Modified: RLVa-1.1.0b
/*
void RlvNotifications::warnGiveToRLV()
{
	if ( (gSavedSettings.getWarning(RLV_SETTING_FIRSTUSE_GIVETORLV)) && (RlvSettings::getForbidGiveToRLV()) )
		LLNotifications::instance().add(RLV_SETTING_FIRSTUSE_GIVETORLV, LLSD(), LLSD(), &RlvNotifications::onGiveToRLVConfirmation);
}
*/

// Checked: 2009-11-13 (RLVa-1.1.0b) | Modified: RLVa-1.1.0b
/*
void RlvNotifications::onGiveToRLVConfirmation(const LLSD& notification, const LLSD& response)
{
	gSavedSettings.setWarning(RLV_SETTING_FIRSTUSE_GIVETORLV, FALSE);

	S32 idxOption = LLNotification::getSelectedOption(notification, response);
	if ( (0 == idxOption) || (1 == idxOption) )
		gSavedSettings.setBOOL(RLV_SETTING_FORBIDGIVETORLV, (idxOption == 1));
}
*/

// =========================================================================
// RlvSettings
//

#ifdef RLV_EXPERIMENTAL_COMPOSITEFOLDERS
bool RlvSettings::s_fCompositeFolders = false;
#endif // RLV_EXPERIMENTAL_COMPOSITEFOLDERS
bool RlvSettings::s_fCanOOC = true;
bool RlvSettings::s_fLegacyNaming = true;
bool RlvSettings::s_fNoSetEnv = false;
bool RlvSettings::s_fTempAttach = true;
std::list<LLUUID> RlvSettings::s_CompatItemCreators;
std::list<std::string> RlvSettings::s_CompatItemNames;

// Checked: 2010-02-27 (RLVa-1.2.0a) | Modified: RLVa-1.1.0i
void RlvSettings::initClass()
{
	static bool fInitialized = false;
	if (!fInitialized)
	{
		initCompatibilityMode(LLStringUtil::null);

		s_fTempAttach = rlvGetSetting<bool>(RLV_SETTING_ENABLETEMPATTACH, true);
		if (gSavedSettings.controlExists(RLV_SETTING_ENABLETEMPATTACH))
			gSavedSettings.getControl(RLV_SETTING_ENABLETEMPATTACH)->getSignal()->connect(boost::bind(&onChangedSettingBOOL, _2, &s_fTempAttach));

		#ifdef RLV_EXPERIMENTAL_COMPOSITEFOLDERS
		s_fCompositeFolders = rlvGetSetting<bool>(RLV_SETTING_ENABLECOMPOSITES, false);
		if (gSavedSettings.controlExists(RLV_SETTING_ENABLECOMPOSITES))
			gSavedSettings.getControl(RLV_SETTING_ENABLECOMPOSITES)->getSignal()->connect(boost::bind(&onChangedSettingBOOL, _2, &s_fCompositeFolders));
		#endif // RLV_EXPERIMENTAL_COMPOSITEFOLDERS

		s_fLegacyNaming = rlvGetSetting<bool>(RLV_SETTING_ENABLELEGACYNAMING, true);
		if (gSavedSettings.controlExists(RLV_SETTING_ENABLELEGACYNAMING))
			gSavedSettings.getControl(RLV_SETTING_ENABLELEGACYNAMING)->getSignal()->connect(boost::bind(&onChangedSettingBOOL, _2, &s_fLegacyNaming));

		s_fCanOOC = rlvGetSetting<bool>(RLV_SETTING_CANOOC, true);
		s_fNoSetEnv = rlvGetSetting<bool>(RLV_SETTING_NOSETENV, false);

		// Don't allow toggling RLVaLoginLastLocation from the debug settings floater
		if (gSavedPerAccountSettings.controlExists(RLV_SETTING_LOGINLASTLOCATION))
			gSavedPerAccountSettings.getControl(RLV_SETTING_LOGINLASTLOCATION)->setHiddenFromSettingsEditor(true);

		if (gSavedSettings.controlExists(RLV_SETTING_TOPLEVELMENU))
			gSavedSettings.getControl(RLV_SETTING_TOPLEVELMENU)->getSignal()->connect(boost::bind(&onChangedMenuLevel));

		fInitialized = true;
	}
}

// Checked: 2010-04-01 (RLVa-1.2.0c) | Modified: RLVa-0.2.1d
void RlvSettings::updateLoginLastLocation()
{
	if ( (!LLApp::isQuitting()) && (gSavedPerAccountSettings.controlExists(RLV_SETTING_LOGINLASTLOCATION)) )
	{
		BOOL fValue = (gRlvHandler.hasBehaviour(RLV_BHVR_TPLOC)) || (!RlvActions::canStand());
		if (gSavedPerAccountSettings.getBOOL(RLV_SETTING_LOGINLASTLOCATION) != fValue)
		{
			gSavedPerAccountSettings.setBOOL(RLV_SETTING_LOGINLASTLOCATION, fValue);
			gSavedPerAccountSettings.saveToFile(gSavedSettings.getString("PerAccountSettingsFile"), TRUE);
		}
	}
}

// Checked: 2011-08-16 (RLVa-1.4.0b) | Added: RLVa-1.4.0b
bool RlvSettings::onChangedMenuLevel()
{
	rlvMenuToggleVisible();
	return true;
}

// Checked: 2010-02-27 (RLVa-1.2.0a) | Added: RLVa-1.1.0i
bool RlvSettings::onChangedSettingBOOL(const LLSD& sdValue, bool* pfSetting)
{
	if (pfSetting)
		*pfSetting = sdValue.asBoolean();
	return true;
}

// Checked: 2015-05-25 (RLVa-1.5.0)
void RlvSettings::onChangedSettingMain(const LLSD& sdValue)
{
	if (sdValue.asBoolean() != (bool)rlv_handler_t::isEnabled())
	{
		LLNotificationsUtil::add(
			"GenericAlert",
			LLSD().with("MESSAGE", llformat(LLTrans::getString("RLVaToggleMessage").c_str(), 
				(sdValue.asBoolean()) ? LLTrans::getString("RLVaToggleEnabled").c_str()
				                      : LLTrans::getString("RLVaToggleDisabled").c_str())));
	}
}

void RlvSettings::initCompatibilityMode(std::string strCompatList)
{
	// NOTE: this function can be called more than once
	s_CompatItemCreators.clear();
	s_CompatItemNames.clear();

	strCompatList.append(";").append(rlvGetSetting<std::string>("RLVaCompatibilityModeList", ""));

	boost_tokenizer tokCompatList(strCompatList, boost::char_separator<char>(";", "", boost::drop_empty_tokens));
	for (const std::string& strCompatEntry : tokCompatList)
	{
		if (boost::starts_with(strCompatEntry, "creator:"))
		{
			LLUUID idCreator;
			if ( (44 == strCompatEntry.size()) && (LLUUID::parseUUID(strCompatEntry.substr(8), &idCreator)) &&
			     (s_CompatItemCreators.end() == std::find(s_CompatItemCreators.begin(), s_CompatItemCreators.end(), idCreator)) )
			{
				s_CompatItemCreators.push_back(idCreator);
			}
		}
		else if (boost::starts_with(strCompatEntry, "name:"))
		{
			if (strCompatEntry.size() > 5)
				s_CompatItemNames.push_back(strCompatEntry.substr(5));
		}
	}
}

bool RlvSettings::isCompatibilityModeObject(const LLUUID& idRlvObject)
{
	bool fCompatMode = false;
	if (idRlvObject.notNull())
	{
		const LLViewerObject* pObj = gObjectList.findObject(idRlvObject);
		if ( (pObj) && (pObj->isAttachment()) )
		{
			const LLViewerInventoryItem* pItem = gInventory.getItem(pObj->getAttachmentItemID());
			if (pItem)
			{
				fCompatMode = s_CompatItemCreators.end() != std::find(s_CompatItemCreators.begin(), s_CompatItemCreators.end(), pItem->getCreatorUUID());
				if (!fCompatMode)
				{
					const std::string& strAttachName = pItem->getName();
					for (const std::string& strCompatName : s_CompatItemNames)
					{
					    boost::regex regexp(strCompatName, boost::regex::perl | boost::regex::icase);
						if (boost::regex_match(strAttachName, regexp))
						{
							fCompatMode = true;
							break;
						}
					}
				}
			}
		}
	}
	return fCompatMode;
}

// ============================================================================
// RlvStrings
//

std::vector<std::string> RlvStrings::m_Anonyms;
RlvStrings::string_map_t RlvStrings::m_StringMap;
std::string RlvStrings::m_StringMapPath;

// Checked: 2011-11-08 (RLVa-1.5.0)
void RlvStrings::initClass()
{
	static bool fInitialized = false;
	if (!fInitialized)
	{
		// Load the default string values
		std::vector<std::string> files = gDirUtilp->findSkinnedFilenames(LLDir::XUI, RLV_STRINGS_FILE, LLDir::ALL_SKINS);
		m_StringMapPath = (!files.empty()) ? files.front() : LLStringUtil::null;
		for (std::vector<std::string>::const_iterator itFile = files.begin(); itFile != files.end(); ++itFile)
		{
			loadFromFile(*itFile, false);
		}

		// Load the custom string overrides
		loadFromFile(gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, RLV_STRINGS_FILE), true);

		// Sanity check
		if ( (m_StringMap.empty()) || (m_Anonyms.empty()) )
		{
			RLV_ERRS << "Problem parsing RLVa string XML file" << RLV_ENDL;
			return;
		}

		fInitialized = true;
	}
}

// Checked: 2011-11-08 (RLVa-1.5.0)
void RlvStrings::loadFromFile(const std::string& strFilePath, bool fUserOverride)
{
	llifstream fileStream(strFilePath.c_str(), std::ios::binary); LLSD sdFileData;
	if ( (!fileStream.is_open()) || (!LLSDSerialize::fromXMLDocument(sdFileData, fileStream)) )
		return;
	fileStream.close();

	if (sdFileData.has("strings"))
	{
		const LLSD& sdStrings = sdFileData["strings"];
		for (LLSD::map_const_iterator itString = sdStrings.beginMap(); itString != sdStrings.endMap(); ++itString)
		{
			if ( (!itString->second.has("value")) || ((fUserOverride) && (!hasString(itString->first))) )
				continue;

			std::list<std::string>& listValues = m_StringMap[itString->first];
			if (!fUserOverride)
			{
				if (listValues.size() > 0)
					listValues.pop_front();
				listValues.push_front(itString->second["value"].asString());
			}
			else
			{
				while (listValues.size() > 1)
					listValues.pop_back();
				listValues.push_back(itString->second["value"].asString());
			}
		}
	}
	if (sdFileData.has("anonyms"))
	{
		const LLSD& sdAnonyms = sdFileData["anonyms"];
		for (LLSD::array_const_iterator itAnonym = sdAnonyms.beginArray(); itAnonym != sdAnonyms.endArray(); ++itAnonym)
		{
			m_Anonyms.push_back((*itAnonym).asString());
		}
	}
}

// Checked: 2011-11-08 (RLVa-1.5.0)
void RlvStrings::saveToFile(const std::string& strFilePath)
{
	LLSD sdFileData;

	LLSD& sdStrings = sdFileData["strings"];
	for (string_map_t::const_iterator itString = m_StringMap.begin(); itString != m_StringMap.end(); ++itString)
	{
		const std::list<std::string>& listValues = itString->second;
		if (listValues.size() > 1)
			sdStrings[itString->first]["value"] = listValues.back();
	}

	llofstream fileStream(strFilePath.c_str());
	if (!fileStream.good())
		return;

	LLSDSerialize::toPrettyXML(sdFileData, fileStream);
	fileStream.close();
}

// Checked: 2009-11-11 (RLVa-1.1.0a) | Modified: RLVa-1.1.0a
const std::string& RlvStrings::getAnonym(const std::string& strName)
{
	const char* pszName = strName.c_str(); U32 nHash = 0;
	
	// Test with 11,264 SL names showed a 3.33% - 3.82% occurance for each so we *should* get a very even spread
	for (int idx = 0, cnt = strName.length(); idx < cnt; idx++)
		nHash += pszName[idx];

	return m_Anonyms[nHash % m_Anonyms.size()];
}

// Checked: 2011-11-08 (RLVa-1.5.0)
const std::string& RlvStrings::getString(const std::string& strStringName)
{
	static const std::string strMissing = "(Missing RLVa string)";
	string_map_t::const_iterator itString = m_StringMap.find(strStringName);
	return (itString != m_StringMap.end()) ? itString->second.back() : strMissing;
}

// Checked: 2009-11-25 (RLVa-1.1.0f) | Added: RLVa-1.1.0f
const char* RlvStrings::getStringFromReturnCode(ERlvCmdRet eRet)
{
	// TODO-RLVa: [2009-11-25] clean this up along with the calling code in process_chat_from_simulator() once we're happy with the output
	switch (eRet)
	{
		case RLV_RET_SUCCESS_UNSET:
			return "unset";
		case RLV_RET_SUCCESS_DUPLICATE:
			return "duplicate";
		case RLV_RET_SUCCESS_DELAYED:
			return "delayed";
		case RLV_RET_SUCCESS_DEPRECATED:
			return "deprecated";
		case RLV_RET_FAILED_SYNTAX:
			return "thingy error";
		case RLV_RET_FAILED_OPTION:
			return "invalid option";
		case RLV_RET_FAILED_PARAM:
			return "invalid param";
		case RLV_RET_FAILED_LOCK:
			return "locked command";
		case RLV_RET_FAILED_DISABLED:
			return "disabled command";
		case RLV_RET_FAILED_UNKNOWN:
			return "unknown command";
		case RLV_RET_FAILED_NOSHAREDROOT:
			return "missing #RLV";
		case RLV_RET_FAILED_DEPRECATED:
			return "deprecated and disabled";
		// The following are identified by the chat verb
		case RLV_RET_RETAINED:
		case RLV_RET_SUCCESS:
		case RLV_RET_FAILED:
			break;
		// The following shouldn't occur
		case RLV_RET_UNKNOWN:
		default:
			RLV_ASSERT(false);
			break;
	};
	return NULL;
}

std::string RlvStrings::getVersion(const LLUUID& idRlvObject, bool fLegacy)
{
	bool fCompatMode = RlvSettings::isCompatibilityModeObject(idRlvObject);
	return llformat("%s viewer v%d.%d.%d (RLVa %d.%d.%d)",
		( (!fLegacy) ? "RestrainedLove" : "RestrainedLife" ),
		(!fCompatMode) ? RLV_VERSION_MAJOR : RLV_VERSION_MAJOR_COMPAT, (!fCompatMode) ? RLV_VERSION_MINOR : RLV_VERSION_MINOR_COMPAT, (!fCompatMode) ? RLV_VERSION_PATCH : RLV_VERSION_PATCH_COMPAT,
		RLVa_VERSION_MAJOR, RLVa_VERSION_MINOR, RLVa_VERSION_PATCH);
}

std::string RlvStrings::getVersionAbout()
{
	return llformat("RLV v%d.%d.%d / RLVa v%d.%d.%d.%d", RLV_VERSION_MAJOR, RLV_VERSION_MINOR, RLV_VERSION_PATCH, RLVa_VERSION_MAJOR, RLVa_VERSION_MINOR, RLVa_VERSION_PATCH, LLVersionInfo::getBuild());
}

std::string RlvStrings::getVersionNum(const LLUUID& idRlvObject)
{
	bool fCompatMode = RlvSettings::isCompatibilityModeObject(idRlvObject);
	return llformat("%d%02d%02d%02d",
		(!fCompatMode) ? RLV_VERSION_MAJOR : RLV_VERSION_MAJOR_COMPAT, (!fCompatMode) ? RLV_VERSION_MINOR : RLV_VERSION_MINOR_COMPAT,
		(!fCompatMode) ? RLV_VERSION_PATCH : RLV_VERSION_PATCH_COMPAT, (!fCompatMode) ? RLV_VERSION_BUILD : RLV_VERSION_BUILD_COMPAT);
}

// Checked: 2011-11-08 (RLVa-1.5.0)
bool RlvStrings::hasString(const std::string& strStringName, bool fCheckCustom)
{
	string_map_t::const_iterator itString = m_StringMap.find(strStringName);
	return (itString != m_StringMap.end()) && ((!fCheckCustom) || (itString->second.size() > 0));
}

// Checked: 2011-11-08 (RLVa-1.5.0)
void RlvStrings::setCustomString(const std::string& strStringName, const std::string& strStringValue)
{
	if (!hasString(strStringName))
		return;

	std::list<std::string>& listValues = m_StringMap[strStringName];
	while (listValues.size() > 1)
		listValues.pop_back();
	if (!strStringValue.empty())
		listValues.push_back(strStringValue);
}

// ============================================================================
// RlvUtil
//

bool RlvUtil::m_fForceTp = false;

// Checked: 2009-07-04 (RLVa-1.0.0a) | Modified: RLVa-1.0.0a
void RlvUtil::filterLocation(std::string& strUTF8Text)
{
	// Filter any mention of the surrounding region names
	LLWorld::region_list_t regions = LLWorld::getInstance()->getRegionList();
	const std::string& strHiddenRegion = RlvStrings::getString(RLV_STRING_HIDDEN_REGION);
	for (LLWorld::region_list_t::const_iterator itRegion = regions.begin(); itRegion != regions.end(); ++itRegion)
		boost::ireplace_all(strUTF8Text, (*itRegion)->getName(), strHiddenRegion);

	// Filter any mention of the parcel name
	LLViewerParcelMgr* pParcelMgr = LLViewerParcelMgr::getInstance();
	if (pParcelMgr)
		boost::ireplace_all(strUTF8Text, pParcelMgr->getAgentParcelName(), RlvStrings::getString(RLV_STRING_HIDDEN_PARCEL));
}

// Checked: 2010-12-08 (RLVa-1.2.2c) | Modified: RLVa-1.2.2c
void RlvUtil::filterNames(std::string& strUTF8Text, bool fFilterLegacy, bool fClearMatches)
{
	uuid_vec_t idAgents;
	LLWorld::getInstance()->getAvatars(&idAgents, NULL);
	for (int idxAgent = 0, cntAgent = idAgents.size(); idxAgent < cntAgent; idxAgent++)
	{
		LLAvatarName avName;
		// NOTE: if we're agressively culling nearby names then ignore exceptions
		if ( (LLAvatarNameCache::get(idAgents[idxAgent], &avName)) && ((fClearMatches) || (!RlvActions::canShowName(RlvActions::SNC_DEFAULT, idAgents[idxAgent]))) )
		{
			const std::string& strDisplayName = avName.getDisplayName();
			bool fFilterDisplay = (strDisplayName.length() > 2);
			const std::string& strLegacyName = avName.getLegacyName();
			fFilterLegacy &= (strLegacyName.length() > 2);
			const std::string& strAnonym = (!fClearMatches) ? RlvStrings::getAnonym(avName) : LLStringUtil::null;

			// If the display name is a subset of the legacy name we need to filter that first, otherwise it's the other way around
			if (boost::icontains(strLegacyName, strDisplayName))
			{
				if (fFilterLegacy)
					boost::ireplace_all(strUTF8Text, strLegacyName, strAnonym);
				if (fFilterDisplay)
					boost::ireplace_all(strUTF8Text, strDisplayName, strAnonym);
			}
			else
			{
				if (fFilterDisplay)
					boost::ireplace_all(strUTF8Text, strDisplayName, strAnonym);
				if (fFilterLegacy)
					boost::ireplace_all(strUTF8Text, strLegacyName, strAnonym);
			}
		}
	}
}

// Checked: 2012-08-19 (RLVa-1.4.7)
void RlvUtil::filterScriptQuestions(S32& nQuestions, LLSD& sdPayload)
{
	// Check SCRIPT_PERMISSION_ATTACH
	if ((!gRlvAttachmentLocks.canAttach()) && (SCRIPT_PERMISSIONS[SCRIPT_PERMISSION_ATTACH].permbit & nQuestions))
	{
		// Notify the user that we blocked it since they're not allowed to wear any new attachments
		sdPayload["rlv_blocked"] = RLV_STRING_BLOCKED_PERMATTACH;
		nQuestions &= ~SCRIPT_PERMISSIONS[SCRIPT_PERMISSION_ATTACH].permbit;
	}

	// Check SCRIPT_PERMISSION_TELEPORT
	if ((gRlvHandler.hasBehaviour(RLV_BHVR_TPLOC)) && (SCRIPT_PERMISSIONS[SCRIPT_PERMISSION_TELEPORT].permbit & nQuestions))
	{
		// Notify the user that we blocked it since they're not allowed to teleport
		sdPayload["rlv_blocked"] = RLV_STRING_BLOCKED_PERMTELEPORT;
		nQuestions &= ~SCRIPT_PERMISSIONS[SCRIPT_PERMISSION_TELEPORT].permbit;
	}

	sdPayload["questions"] = nQuestions;
}

// Checked: 2010-08-29 (RLVa-1.2.1c) | Added: RLVa-1.2.1c
void RlvUtil::forceTp(const LLVector3d& posDest)
{
	m_fForceTp = true;
	gAgent.teleportViaLocationLookAt(posDest);
	m_fForceTp = false;
}

// Checked: 2010-04-22 (RLVa-1.2.0f) | Modified: RLVa-1.2.0f
bool RlvUtil::isNearbyAgent(const LLUUID& idAgent)
{
	// Sanity check since we call this with notification payloads as well and those strings tend to change from one release to another
	RLV_ASSERT(idAgent.notNull());
	if ( (idAgent.notNull()) && (gAgent.getID() != idAgent) )
	{
		std::vector<LLUUID> idAgents;
		LLWorld::getInstance()->getAvatars(&idAgents, NULL);

		for (int idxAgent = 0, cntAgent = idAgents.size(); idxAgent < cntAgent; idxAgent++)
			if (idAgents[idxAgent] == idAgent)
				return true;
	}
	return false;
}

// Checked: 2010-04-05 (RLVa-1.2.0d) | Modified: RLVa-1.2.0d
bool RlvUtil::isNearbyRegion(const std::string& strRegion)
{
	LLWorld::region_list_t regions = LLWorld::getInstance()->getRegionList();
	for (LLWorld::region_list_t::const_iterator itRegion = regions.begin(); itRegion != regions.end(); ++itRegion)
		if ((*itRegion)->getName() == strRegion)
			return true;
	return false;
}

// Checked: 2011-04-11 (RLVa-1.3.0h) | Modified: RLVa-1.3.0h
void RlvUtil::notifyBlocked(const std::string& strNotifcation, const LLSD& sdArgs)
{
	std::string strMsg = RlvStrings::getString(strNotifcation);
	LLStringUtil::format(strMsg, sdArgs);

	LLSD sdNotify;
	sdNotify["MESSAGE"] = strMsg;
	LLNotificationsUtil::add("SystemMessageTip", sdNotify);
}

// Checked: 2010-11-11 (RLVa-1.2.1g) | Added: RLVa-1.2.1g
void RlvUtil::notifyFailedAssertion(const std::string& strAssert, const std::string& strFile, int nLine)
{
	// Don't show the same assertion over and over, or if the user opted out
	static std::string strAssertPrev, strFilePrev; static int nLinePrev;
	if ( ((strAssertPrev == strAssert) && (strFile == strFilePrev) && (nLine == nLinePrev)) ||
		 (!rlvGetSetting<bool>(RLV_SETTING_SHOWASSERTIONFAIL, true)) )
	{
		return;
	}

	strAssertPrev = strAssert;
	strFilePrev = strFile;
	nLinePrev = nLine;

	LLSD argsNotify;
	argsNotify["MESSAGE"] = llformat("RLVa assertion failure: %s (%s - %d)", strAssert.c_str(), strFile.c_str(), nLine);
	LLNotificationsUtil::add("SystemMessageTip", argsNotify);
}

// Checked: 2010-03-27 (RLVa-1.2.0b) | Modified: RLVa-1.2.0b
void RlvUtil::sendBusyMessage(const LLUUID& idTo, const std::string& strMsg, const LLUUID& idSession)
{
	// [See process_improved_im()]
	std::string strFullName;
	LLAgentUI::buildFullname(strFullName);

	pack_instant_message(gMessageSystem, gAgent.getID(), FALSE, gAgent.getSessionID(), idTo, strFullName,
		strMsg, IM_ONLINE, IM_DO_NOT_DISTURB_AUTO_RESPONSE, idSession);
	gAgent.sendReliableMessage();
}

// Checked: 2010-03-09 (RLVa-1.2.0a) | Modified: RLVa-1.0.1e
bool RlvUtil::sendChatReply(S32 nChannel, const std::string& strUTF8Text)
{
	if (!isValidReplyChannel(nChannel))
		return false;

	// Copy/paste from send_chat_from_viewer()
	gMessageSystem->newMessageFast(_PREHASH_ChatFromViewer);
	gMessageSystem->nextBlockFast(_PREHASH_AgentData);
	gMessageSystem->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
	gMessageSystem->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
	gMessageSystem->nextBlockFast(_PREHASH_ChatData);
	gMessageSystem->addStringFast(_PREHASH_Message, utf8str_truncate(strUTF8Text, MAX_MSG_STR_LEN));
	gMessageSystem->addU8Fast(_PREHASH_Type, CHAT_TYPE_SHOUT);
	gMessageSystem->addS32("Channel", nChannel);
	gAgent.sendReliableMessage();
	add(LLStatViewer::CHAT_COUNT, 1);

	return true;
}

void RlvUtil::teleportCallback(U64 hRegion, const LLVector3& posRegion, const LLVector3& vecLookAt)
{
	if (hRegion)
	{
		const LLVector3d posGlobal = from_region_handle(hRegion) + (LLVector3d)posRegion;
		if (vecLookAt.isExactlyZero())
			gAgent.teleportViaLocation(posGlobal);
		else
			gAgent.teleportViaLocationLookAt(posGlobal, vecLookAt);
	}
}

// ============================================================================
// Generic menu enablers
//

// Checked: 2015-05-25 (RLVa-1.5.0)
bool rlvMenuMainToggleVisible(LLUICtrl* pMenuCtrl)
{
	LLMenuItemCheckGL* pMenuItem = dynamic_cast<LLMenuItemCheckGL*>(pMenuCtrl);
	if (pMenuItem)
	{
		static std::string strLabel = pMenuItem->getLabel();
		if (gSavedSettings.getBOOL(RLV_SETTING_MAIN) == rlv_handler_t::isEnabled())
			pMenuItem->setLabel(strLabel);
		else
			pMenuItem->setLabel(strLabel + " " + LLTrans::getString("RLVaPendingRestart"));
	}
	return true;
}

// Checked: 2011-08-16 (RLVa-1.4.0b) | Added: RLVa-1.4.0b
void rlvMenuToggleVisible()
{
	bool fTopLevel = rlvGetSetting(RLV_SETTING_TOPLEVELMENU, true);
	bool fRlvEnabled = rlv_handler_t::isEnabled();

	LLMenuGL* pRLVaMenuMain = gMenuBarView->findChildMenuByName("RLVa Main", FALSE);
	LLMenuGL* pAdvancedMenu = gMenuBarView->findChildMenuByName("Advanced", FALSE);
	LLMenuGL* pRLVaMenuEmbed = pAdvancedMenu->findChildMenuByName("RLVa Embedded", FALSE);

	gMenuBarView->setItemVisible("RLVa Main", (fRlvEnabled) && (fTopLevel));
	pAdvancedMenu->setItemVisible("RLVa Embedded", (fRlvEnabled) && (!fTopLevel));

	if ( (rlv_handler_t::isEnabled()) && (pRLVaMenuMain) && (pRLVaMenuEmbed) && 
		 ( ((fTopLevel) && (1 == pRLVaMenuMain->getItemCount())) || ((!fTopLevel) && (1 == pRLVaMenuEmbed->getItemCount())) ) )
	{
		LLMenuGL* pMenuFrom = (fTopLevel) ? pRLVaMenuEmbed : pRLVaMenuMain;
		LLMenuGL* pMenuTo = (fTopLevel) ? pRLVaMenuMain : pRLVaMenuEmbed;
		while (LLMenuItemGL* pItem = pMenuFrom->getItem(1))
		{
			pMenuFrom->removeChild(pItem);
			pMenuTo->addChild(pItem);
			pItem->updateBranchParent(pMenuTo);
		}
	}
}

bool rlvMenuCanShowName()
{
  const LLVOAvatar* pAvatar = find_avatar_from_object(LLSelectMgr::getInstance()->getSelection()->getPrimaryObject());
  return (pAvatar) && (RlvActions::canShowName(RlvActions::SNC_DEFAULT, pAvatar->getID()));
}

// Checked: 2010-04-23 (RLVa-1.2.0g) | Modified: RLVa-1.2.0g
bool rlvMenuEnableIfNot(const LLSD& sdParam)
{
	bool fEnable = true;
	if (rlv_handler_t::isEnabled())
	{
		ERlvBehaviour eBhvr = RlvBehaviourDictionary::instance().getBehaviourFromString(sdParam.asString(), RLV_TYPE_ADDREM);
		fEnable = (eBhvr != RLV_BHVR_UNKNOWN) ? !gRlvHandler.hasBehaviour(eBhvr) : true;
	}
	return fEnable;
}

// ============================================================================
// Selection functors
//

// Checked: 2011-05-28 (RLVa-1.4.6) | Modified: RLVa-1.4.0
bool rlvCanDeleteOrReturn(const LLViewerObject* pObj)
{
	// Block if: @rez=n restricted and owned by us or a group *or* @unsit=n restricted and being sat on by us
	return
		( (!gRlvHandler.hasBehaviour(RLV_BHVR_REZ)) || ((!pObj->permYouOwner()) && (!pObj->permGroupOwner())) ) &&
		( (!gRlvHandler.hasBehaviour(RLV_BHVR_UNSIT)) || (!isAgentAvatarValid()) || (!pObj->getRootEdit()->isChild(gAgentAvatarp)) );
}

// Checked: 2011-05-28 (RLVa-1.4.6) | Modified: RLVa-1.4.0
bool rlvCanDeleteOrReturn()
{
	if ( (gRlvHandler.hasBehaviour(RLV_BHVR_REZ)) || (gRlvHandler.hasBehaviour(RLV_BHVR_UNSIT)) )
	{
		struct RlvCanDeleteOrReturn : public LLSelectedObjectFunctor
		{
			/*virtual*/ bool apply(LLViewerObject* pObj) { return rlvCanDeleteOrReturn(pObj); }
		} f;
		LLObjectSelectionHandle hSel = LLSelectMgr::getInstance()->getSelection();
		return (hSel.notNull()) && (0 != hSel->getRootObjectCount()) && (hSel->applyToRootObjects(&f, false));
	}
	return true;
}

// Checked: 2010-04-20 (RLVa-1.2.0f) | Modified: RLVa-0.2.0f
bool RlvSelectHasLockedAttach::apply(LLSelectNode* pNode)
{
	return (pNode->getObject()) ? gRlvAttachmentLocks.isLockedAttachment(pNode->getObject()->getRootEdit()) : false;
}

// Checked: 2010-11-29 (RLVa-1.3.0c) | Added: RLVa-1.3.0c
bool RlvSelectIsEditable::apply(LLSelectNode* pNode)
{
	const LLViewerObject* pObj = pNode->getObject();
	return (pObj) && (!RlvActions::canEdit(pObj));
}

// Checked: 2011-05-28 (RLVa-1.4.0a) | Modified: RLVa-1.4.0a
bool RlvSelectIsSittingOn::apply(LLSelectNode* pNode)
{
	return (pNode->getObject()) && (pNode->getObject()->getRootEdit()->isChild(m_pAvatar));
}

// ============================================================================
// Predicates
//

// Checked: 2010-11-11 (RLVa-1.2.1g) | Modified: RLVa-1.2.1g
bool rlvPredCanWearItem(const LLViewerInventoryItem* pItem, ERlvWearMask eWearMask)
{
	if ( (pItem) && (RlvForceWear::isWearableItem(pItem)) )
	{
		if (RlvForceWear::isWearingItem(pItem))
			return true; // Special exception for currently worn items
		switch (pItem->getType())
		{
			case LLAssetType::AT_BODYPART:
				// NOTE: only one body part of each type is allowed so the only way to wear one is if we can replace the current one
				return (RLV_WEAR_LOCKED != (gRlvWearableLocks.canWear(pItem) & RLV_WEAR_REPLACE & eWearMask));
			case LLAssetType::AT_CLOTHING:
				return (RLV_WEAR_LOCKED != (gRlvWearableLocks.canWear(pItem) & eWearMask));
			case LLAssetType::AT_OBJECT:
				return (RLV_WEAR_LOCKED != (gRlvAttachmentLocks.canAttach(pItem) & eWearMask));
			case LLAssetType::AT_GESTURE:
				return true;
			default:
				RLV_ASSERT(false);
		}
	}
	return false;
}

// Checked: 2010-03-22 (RLVa-1.2.0c) | Added: RLVa-1.2.0a
bool rlvPredCanNotWearItem(const LLViewerInventoryItem* pItem, ERlvWearMask eWearMask)
{
	return !rlvPredCanWearItem(pItem, eWearMask);
}

// Checked: 2014-11-02 (RLVa-1.4.11)
bool rlvPredCanRemoveItem(const LLUUID& idItem)
{
	// Check the inventory item if it's available
	const LLViewerInventoryItem* pItem = gInventory.getItem(idItem);
	if (pItem)
	{
		return rlvPredCanRemoveItem(pItem);
	}

	// Temporary attachments don't have inventory items associated with them so check the attachment itself
	if (isAgentAvatarValid())
	{
		const LLViewerObject* pAttachObj = gAgentAvatarp->getWornAttachment(idItem);
		return (pAttachObj) && (!gRlvAttachmentLocks.isLockedAttachment(pAttachObj));
	}

	return false;
}

// Checked: 2010-03-22 (RLVa-1.2.0)
bool rlvPredCanRemoveItem(const LLViewerInventoryItem* pItem)
{
	if (pItem)
	{
		switch (pItem->getType())
		{
			case LLAssetType::AT_BODYPART:
			case LLAssetType::AT_CLOTHING:
				return gRlvWearableLocks.canRemove(pItem);
			case LLAssetType::AT_OBJECT:
				return gRlvAttachmentLocks.canDetach(pItem);
			case LLAssetType::AT_GESTURE:
				return true;
			default:
				RLV_ASSERT(!RlvForceWear::isWearableItem(pItem));
		}
	}
	return false;
}

// Checked: 2010-03-22 (RLVa-1.2.0c) | Added: RLVa-1.2.0a
bool rlvPredCanNotRemoveItem(const LLViewerInventoryItem* pItem)
{
	return !rlvPredCanRemoveItem(pItem);
}

// Checked: 2010-04-24 (RLVa-1.2.0f) | Added: RLVa-1.2.0f
RlvPredIsEqualOrLinkedItem::RlvPredIsEqualOrLinkedItem(const LLUUID& idItem)
{
	m_pItem = gInventory.getItem(idItem);
}

// Checked: 2010-04-24 (RLVa-1.2.0f) | Added: RLVa-1.2.0f
bool RlvPredIsEqualOrLinkedItem::operator()(const LLViewerInventoryItem* pItem) const
{
	return (m_pItem) && (pItem) && (m_pItem->getLinkedUUID() == pItem->getLinkedUUID());
}

// ============================================================================
// Various public helper functions
//

// Checked: 2009-11-15 (RLVa-1.1.0c) | Added: RLVa-1.1.0c
/*
BOOL rlvEnableSharedWearEnabler(void* pParam)
{
	return false;
	// Visually disable the "Enable Shared Wear" option when at least one attachment is non-detachable
	return (!gRlvHandler.hasLockedAttachment(RLV_LOCK_REMOVE));
}
*/

// Checked: 2010-11-01 (RLVa-1.2.2a) | Added: RLVa-1.2.2a
const std::string& rlvGetAnonym(const LLAvatarName& avName)
{
	return RlvStrings::getAnonym(avName);
}

// ============================================================================
