/**
 *
 * Copyright (c) 2009-2018, Kitty Barnett
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

// Generic includes
#include "llviewerprecompiledheaders.h"
#include "llagent.h"
#include "llappearancemgr.h"
#include "llappviewer.h"
#include "llexperiencecache.h"
#include "llexperiencelog.h"
#include "llgroupactions.h"
#include "llhudtext.h"
#include "llmoveview.h"
#include "llslurl.h"
#include "llstartup.h"
#include "llviewermessage.h"
#include "llviewermenu.h"
#include "llviewerobjectlist.h"
#include "llviewerparcelmgr.h"
#include "llviewerregion.h"

// Command specific includes
#include "llagentcamera.h"				// @setcam and related
#include "llavataractions.h"            // @stopim IM query
#include "llavatarnamecache.h"			// @shownames
#include "llavatarlist.h"				// @shownames
#include "llfloatercamera.h"			// @setcam family
#include "llfloatersidepanelcontainer.h"// @shownames
#include "llnotifications.h"			// @list IM query
#include "llnotificationsutil.h"
#include "lloutfitslist.h"				// @showinv - "Appearance / My Outfits" panel
#include "llpaneloutfitsinventory.h"	// @showinv - "Appearance" floater
#include "llpanelpeople.h"				// @shownames
#include "llpanelwearing.h"				// @showinv - "Appearance / Current Outfit" panel
#include "llregionhandle.h"				// @tpto
#include "llsidepanelappearance.h"		// @showinv - "Appearance / Edit appearance" panel
#include "lltabcontainer.h"				// @showinv - Tab container control for inventory tabs
#include "lltoolmgr.h"					// @edit
#include "llviewercamera.h"				// @setcam and related
#include "llviewershadermgr.h"			// @setsphere
#include "llworldmapmessage.h"			// @tpto
#include "llviewertexturelist.h"		// @setcam_texture
#include "pipeline.h"					// @setsphere

// RLVa includes
#include "rlvactions.h"
#include "rlvenvironment.h"
#include "rlvfloaters.h"
#include "rlvactions.h"
#include "rlveffects.h"
#include "rlvhandler.h"
#include "rlvhelper.h"
#include "rlvinventory.h"
#include "rlvlocks.h"
#include "rlvmodifiers.h"
#include "rlvui.h"
#include "rlvextensions.h"

// <FS:Ansariel> [FS communication UI]
#include "fsfloatervoicecontrols.h"
// </FS:Ansariel> [FS communication UI]
#include "fslslbridge.h"
#include "fsradar.h"

// Boost includes
#include <boost/algorithm/string.hpp>

// llappviewer.cpp
extern BOOL gDoDisconnect;

// ============================================================================
// Static variable initialization
//

bool RlvHandler::m_fEnabled = false;

rlv_handler_t gRlvHandler;

// ============================================================================
// Command specific helper functions
//

// Checked: 2009-08-04 (RLVa-1.0.1d) | Added: RLVa-1.0.1d
static bool rlvParseNotifyOption(const std::string& strOption, S32& nChannel, std::string& strFilter)
{
	boost_tokenizer tokens(strOption, boost::char_separator<char>(";", "", boost::keep_empty_tokens));
	boost_tokenizer::const_iterator itTok = tokens.begin();

	// Extract and sanity check the first token (required) which is the channel
	if ( (itTok == tokens.end()) || (!LLStringUtil::convertToS32(*itTok, nChannel)) || (!RlvUtil::isValidReplyChannel(nChannel)) )
		return false;

	// Second token (optional) is the filter
	strFilter.clear();
	if (++itTok != tokens.end())
	{
		strFilter = *itTok;
		++itTok;
	}

	return (itTok == tokens.end());
}

// Checked: 2014-02-26 (RLVa-1.4.10)
static bool rlvParseGetStatusOption(const std::string& strOption, std::string& strFilter, std::string& strSeparator)
{
	// @getstatus:[<option>][;<separator>]
	//   * Parameters: first and second parameters are both optional
	//   * Examples  : @getstatus=123 ; @getstatus:tp=123 ; @getstatus:tp;|=123 ; @getstatus:;|=123

	boost_tokenizer tokens(strOption, boost::char_separator<char>(";", "", boost::keep_empty_tokens));
	boost_tokenizer::const_iterator itTok = tokens.begin();

	strSeparator = "/";
	strFilter.clear();

	// <option> optional parameter (defaults to empty string if unspecified)
	if (itTok != tokens.end())
		strFilter = *itTok++;
	else
		strFilter.clear();

	// <separator> optional paramter (defaults to '/' if unspecified)
	if ( (itTok != tokens.end()) && (!(*itTok).empty()) )
		strSeparator = *itTok++;
	else
		strSeparator = "/";

	return true;
}

// ============================================================================
// Constructor/destructor
//

// Checked: 2010-04-07 (RLVa-1.2.0d) | Modified: RLVa-1.0.1d
RlvHandler::RlvHandler() : m_fCanCancelTp(true), m_posSitSource(), m_pGCTimer(NULL)
{
	gAgent.addListener(this, "new group");

	// Array auto-initialization to 0 is still not supported in VS2013
	memset(m_Behaviours, 0, sizeof(S16) * RLV_BHVR_COUNT);
}

RlvHandler::~RlvHandler()
{
	cleanup();
}

void RlvHandler::cleanup()
{
	// Nothing to clean if we're not enabled (or already cleaned up)
	if (!m_fEnabled)
		return;

	//
	// Clean up any restrictions that are still active
	//
	RLV_ASSERT(LLApp::isExiting() || gDoDisconnect);	// Several commands toggle debug settings but won't if they know the viewer is quitting

	// Assume we have no way to predict how m_Objects will change so make a copy ahead of time
	uuid_vec_t idRlvObjects;
	idRlvObjects.reserve(m_Objects.size());
	std::transform(m_Objects.begin(), m_Objects.end(), std::back_inserter(idRlvObjects), [](const rlv_object_map_t::value_type& kvPair) {return kvPair.first; });
	for (const LLUUID & idRlvObj : idRlvObjects)
	{
		processCommand(idRlvObj, "clear", true);
	}

	// Sanity check
	RLV_ASSERT(m_Objects.empty());
	RLV_ASSERT(m_Exceptions.empty());
	RLV_ASSERT(std::all_of(m_Behaviours, m_Behaviours + RLV_BHVR_COUNT, [](S16 cnt) { return !cnt; }));
	RLV_ASSERT(m_CurCommandStack.empty());
	RLV_ASSERT(m_CurObjectStack.empty());

	//
	// Clean up what's left
	//
	gAgent.removeListener(this);
	m_Retained.clear();
	//delete m_pGCTimer;	// <- deletes itself

	if (m_PendingGroupChange.first.notNull())
	{
		if (LLGroupMgr::instanceExists())
			LLGroupMgr::instance().removeObserver(m_PendingGroupChange.first, this);
		m_PendingGroupChange = std::make_pair(LLUUID::null, LLStringUtil::null);
	}

	for (RlvExtCommandHandler* pCmdHandler : m_CommandHandlers)
	{
		delete pCmdHandler;
	}
	m_CommandHandlers.clear();

	m_fEnabled = false;
}

// ============================================================================
// Behaviour related functions
//

bool RlvHandler::findBehaviour(ERlvBehaviour eBhvr, std::list<const RlvObject*>& lObjects) const
{
	lObjects.clear();
	for (const auto& objEntry : m_Objects)
		if (objEntry.second.hasBehaviour(eBhvr, false))
			lObjects.push_back(&objEntry.second);
	return !lObjects.empty();
}

bool RlvHandler::hasBehaviour(const LLUUID& idRlvObj, ERlvBehaviour eBhvr, const std::string& strOption) const
{
	rlv_object_map_t::const_iterator itObj = m_Objects.find(idRlvObj);
	if (m_Objects.end() != itObj)
		return itObj->second.hasBehaviour(eBhvr, strOption, false);
	return false;
}

bool RlvHandler::hasBehaviourExcept(ERlvBehaviour eBhvr, const std::string& strOption, const LLUUID& idObj) const
{
	for (rlv_object_map_t::const_iterator itObj = m_Objects.begin(); itObj != m_Objects.end(); ++itObj)
		if ( (idObj != itObj->second.getObjectID()) && (itObj->second.hasBehaviour(eBhvr, strOption, false)) )
			return true;
	return false;
}

// Checked: 2011-04-11 (RLVa-1.3.0h) | Added: RLVa-1.3.0h
bool RlvHandler::hasBehaviourRoot(const LLUUID& idObjRoot, ERlvBehaviour eBhvr, const std::string& strOption) const
{
	for (rlv_object_map_t::const_iterator itObj = m_Objects.begin(); itObj != m_Objects.end(); ++itObj)
		if ( (idObjRoot == itObj->second.getRootID()) && (itObj->second.hasBehaviour(eBhvr, strOption, false)) )
			return true;
	return false;
}

bool RlvHandler::ownsBehaviour(const LLUUID& idObj, ERlvBehaviour eBhvr) const
{
	bool fHasBhvr = false;
	for (const auto& objEntry : m_Objects)
	{
		if (objEntry.second.hasBehaviour(eBhvr, false))
		{
			if (objEntry.first != idObj)
				return false;
			fHasBhvr = true;
		}
	}
	return fHasBhvr;
}

// ============================================================================
// Behaviour exception handling
//

void RlvHandler::addException(const LLUUID& idObj, ERlvBehaviour eBhvr, const RlvExceptionOption& varOption)
{
	m_Exceptions.insert(std::make_pair(eBhvr, RlvException(idObj, eBhvr, varOption)));
}

bool RlvHandler::isException(ERlvBehaviour eBhvr, const RlvExceptionOption& varOption, ERlvExceptionCheck eCheckType) const
{
	// We need to "strict check" exceptions only if: the restriction is actually in place *and* (isPermissive(eBhvr) == FALSE)
	if (ERlvExceptionCheck::Default == eCheckType)
		eCheckType = ( (hasBehaviour(eBhvr)) && (!isPermissive(eBhvr)) ) ? ERlvExceptionCheck::Strict : ERlvExceptionCheck::Permissive;

	uuid_vec_t objList;
	if (ERlvExceptionCheck::Strict == eCheckType)
	{
		// If we're "strict checking" then we need the UUID of every object that currently has 'eBhvr' restricted
		for (const auto& objEntry : m_Objects)
		{
			if (objEntry.second.hasBehaviour(eBhvr, !hasBehaviour(RLV_BHVR_PERMISSIVE)))
				objList.push_back(objEntry.first);
		}
	}

	for (rlv_exception_map_t::const_iterator itException = m_Exceptions.lower_bound(eBhvr), endException = m_Exceptions.upper_bound(eBhvr); itException != endException; ++itException)
	{
		if (itException->second.varOption == varOption)
		{
			// For permissive checks we just return on the very first match
			if (ERlvExceptionCheck::Permissive == eCheckType)
				return true;

			// For strict checks we don't return until the list is empty (every object with 'eBhvr' restricted also contains the exception)
			uuid_vec_t::iterator itList = std::find(objList.begin(), objList.end(), itException->second.idObject);
			if (itList != objList.end())
				objList.erase(itList);
			if (objList.empty())
				return true;
		}
	}
	return false;
}

bool RlvHandler::isPermissive(ERlvBehaviour eBhvr) const
{
	return (RlvBehaviourDictionary::instance().getHasStrict(eBhvr))
		? !((hasBehaviour(RLV_BHVR_PERMISSIVE)) || (isException(RLV_BHVR_PERMISSIVE, eBhvr, ERlvExceptionCheck::Permissive)))
		: true;
}

void RlvHandler::removeException(const LLUUID& idObj, ERlvBehaviour eBhvr, const RlvExceptionOption& varOption)
{
	for (rlv_exception_map_t::iterator itException = m_Exceptions.lower_bound(eBhvr), endException = m_Exceptions.upper_bound(eBhvr); itException != endException; ++itException)
	{
		if ( (itException->second.idObject == idObj) && (itException->second.varOption == varOption) )
		{
			m_Exceptions.erase(itException);
			break;
		}
	}
}

// ============================================================================
// Blocked object handling
//

void RlvHandler::addBlockedObject(const LLUUID& idObj, const std::string& strName)
{
	m_BlockedObjects.push_back(std::make_tuple(idObj, strName, LLTimer::getTotalSeconds()));
}

bool RlvHandler::hasUnresolvedBlockedObject() const
{
	return std::any_of(m_BlockedObjects.begin(), m_BlockedObjects.end(), [](const rlv_blocked_object_t& entry) { return std::get<0>(entry).isNull(); });
}

bool RlvHandler::isBlockedObject(const LLUUID& idObj) const
{
	return std::any_of(m_BlockedObjects.begin(), m_BlockedObjects.end(), [&idObj](const rlv_blocked_object_t& entry) { return std::get<0>(entry) == idObj; });
}

void RlvHandler::removeBlockedObject(const LLUUID& idObj)
{
	m_BlockedObjects.erase(std::remove_if(m_BlockedObjects.begin(), m_BlockedObjects.end(),
		[&idObj](const rlv_blocked_object_t& entry) {
			return (idObj.notNull()) ? std::get<0>(entry) == idObj : false;
		}), m_BlockedObjects.end());
}

void RlvHandler::getAttachmentResourcesCoro(const std::string& strUrl)
{
	LLCore::HttpRequest::policy_t httpPolicy(LLCore::HttpRequest::DEFAULT_POLICY_ID);
	LLCoreHttpUtil::HttpCoroutineAdapter::ptr_t httpAdapter(new LLCoreHttpUtil::HttpCoroutineAdapter("RlvHandler::getAttachmentResourcesCoro", httpPolicy));
	LLCore::HttpRequest::ptr_t httpRequest(new LLCore::HttpRequest);
	const LLSD sdResult = httpAdapter->getAndSuspend(httpRequest, strUrl);

	const LLCore::HttpStatus httpStatus = LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(sdResult[LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS]);
	if ( (httpStatus) && (sdResult.has("attachments")) )
	{
		const LLSD& sdAttachments = sdResult["attachments"];
		for (LLSD::array_const_iterator itAttach = sdAttachments.beginArray(), endAttach = sdAttachments.endArray(); itAttach != endAttach; ++itAttach)
		{
			if (!itAttach->has("objects"))
				continue;

			const LLSD& sdAttachObjects = itAttach->get("objects");
			for (LLSD::array_const_iterator itAttachObj = sdAttachObjects.beginArray(), endAttachObj = sdAttachObjects.endArray(); itAttachObj != endAttachObj; ++itAttachObj)
			{
				const LLUUID idObj = itAttachObj->get("id").asUUID();
				const std::string& strObjName = itAttachObj->get("name").asStringRef();

				// If it's an attachment, it should be a temporary one (NOTE: we might catch it before it's had a chance to attach)
				const LLViewerObject* pObj = gObjectList.findObject(idObj);
				if ( (pObj) && ((!pObj->isAttachment()) || (!pObj->isTempAttachment()) || (isBlockedObject(idObj))) )
					continue;

				// Find it by object name
				auto itBlockedObj = std::find_if(m_BlockedObjects.begin(), m_BlockedObjects.end(),
					[&strObjName](const rlv_blocked_object_t& entry) {
						return (std::get<0>(entry).isNull()) && (std::get<1>(entry) == strObjName);
					});
				if (m_BlockedObjects.end() != itBlockedObj)
				{
					std::get<0>(*itBlockedObj) = idObj;

					RLV_INFOS << "Clearing restrictions from blocked object " << idObj.asString() << RLV_ENDL;
					processCommand(idObj, "clear", true);
					return;
				}
			}
		}
	}
}

// ============================================================================
// Command processing functions
//

// Checked: 2010-04-07 (RLVa-1.2.0d) | Modified: RLVa-1.1.0f
void RlvHandler::addCommandHandler(RlvExtCommandHandler* pCmdHandler)
{
	if ( (pCmdHandler) && (std::find(m_CommandHandlers.begin(), m_CommandHandlers.end(), pCmdHandler) == m_CommandHandlers.end()) )
		m_CommandHandlers.push_back(pCmdHandler);
}

// Checked: 2010-04-07 (RLVa-1.2.0d) | Modified: RLVa-1.1.0f
void RlvHandler::removeCommandHandler(RlvExtCommandHandler* pCmdHandler)
{
	if (pCmdHandler)
		m_CommandHandlers.remove(pCmdHandler);
}

// Checked: 2010-04-07 (RLVa-1.2.0d) | Modified: RLVa-1.1.0a
void RlvHandler::clearCommandHandlers()
{
	std::list<RlvExtCommandHandler*>::const_iterator itHandler = m_CommandHandlers.begin();
	while (itHandler != m_CommandHandlers.end())
	{
		delete *itHandler;
		++itHandler;
	}
	m_CommandHandlers.clear();
}

// Checked: 2010-04-07 (RLVa-1.2.0d) | Modified: RLVa-1.1.0f
bool RlvHandler::notifyCommandHandlers(rlvExtCommandHandler f, const RlvCommand& rlvCmd, ERlvCmdRet& eRet, bool fNotifyAll) const
{
	std::list<RlvExtCommandHandler*>::const_iterator itHandler = m_CommandHandlers.begin(); bool fContinue = true; eRet = RLV_RET_UNKNOWN;
	while ( (itHandler != m_CommandHandlers.end()) && ((fContinue) || (fNotifyAll)) )
	{
		ERlvCmdRet eCmdRet = RLV_RET_UNKNOWN;
		if ((fContinue = !((*itHandler)->*f)(rlvCmd, eCmdRet)) == false)
			eRet = eCmdRet;
		++itHandler;
	}
	RLV_ASSERT( (fContinue) || (eRet != RLV_RET_UNKNOWN) );
	return !fContinue;
}

// Checked: 2009-11-25 (RLVa-1.1.0f) | Modified: RLVa-1.1.0f
ERlvCmdRet RlvHandler::processCommand(std::reference_wrapper<const RlvCommand> rlvCmd, bool fFromObj)
{
	{
		const RlvCommand& rlvCmdTmp = rlvCmd; // Reference to the temporary with limited variable scope since we don't want it to leak below

		RLV_DEBUGS << "[" << rlvCmdTmp.getObjectID() << "]: " << rlvCmdTmp.asString() << RLV_ENDL;

		if ( (isBlockedObject(rlvCmdTmp.getObjectID())) && (RLV_TYPE_REMOVE != rlvCmdTmp.getParamType()) && (RLV_TYPE_CLEAR != rlvCmdTmp.getParamType()) )
		{
			RLV_DEBUGS << "\t-> blocked object" << RLV_ENDL;
			return RLV_RET_FAILED_BLOCKED;
		}
		if (!rlvCmdTmp.isValid())
		{
			RLV_DEBUGS << "\t-> invalid syntax" << RLV_ENDL;
			return RLV_RET_FAILED_SYNTAX;
		}
		if (rlvCmdTmp.isBlocked())
		{
			RLV_DEBUGS << "\t-> blocked command" << RLV_ENDL;
			return RLV_RET_FAILED_DISABLED;
		}
	}

	// Using a stack for executing commands solves a few problems:
	//   - if we passed RlvObject::m_idObj for idObj somewhere and process a @clear then idObj points to invalid/cleared memory at the end
	//   - if command X triggers command Y along the way then getCurrentCommand()/getCurrentObject() still return Y even when finished
	m_CurCommandStack.push(rlvCmd); m_CurObjectStack.push(rlvCmd.get().getObjectID());
	const LLUUID& idCurObj = m_CurObjectStack.top();

	ERlvCmdRet eRet = RLV_RET_UNKNOWN;
	switch (rlvCmd.get().getParamType())
	{
		case RLV_TYPE_ADD:		// Checked: 2009-11-26 (RLVa-1.1.0f) | Modified: RLVa-1.1.0f
			{
				ERlvBehaviour eBhvr = rlvCmd.get().getBehaviourType();
				if(eBhvr == RLV_BHVR_UNKNOWN)
				{
					eRet = RLV_RET_FAILED_PARAM;
					RLV_DEBUGS << "\t- " << rlvCmd.get().getBehaviour() << " is UNKNOWN => Call Kitty!" << RLV_ENDL;
					break;
				}

				if ( (m_Behaviours[eBhvr]) && ( (RLV_BHVR_SETCAM == eBhvr) || (RLV_BHVR_SETDEBUG == eBhvr) || (RLV_BHVR_SETENV == eBhvr) ) )
				{
					// Some restrictions can only be held by one single object to avoid deadlocks
					RLV_DEBUGS << "\t- " << rlvCmd.get().getBehaviour() << " is already set by another object => discarding" << RLV_ENDL;
					eRet = RLV_RET_FAILED_LOCK;
					break;
				}

				rlv_object_map_t::iterator itObj = m_Objects.find(idCurObj); bool fAdded = false;
				if (itObj != m_Objects.end())
				{
					// Add the command to an existing object
					rlvCmd = itObj->second.addCommand(rlvCmd, fAdded);
				}
				else
				{
					// Create a new RLV object and then add the command to it (and grab its reference)
					itObj = m_Objects.insert(std::pair<LLUUID, RlvObject>(idCurObj, RlvObject(idCurObj))).first;
					rlvCmd = itObj->second.addCommand(rlvCmd, fAdded);
				}

				RLV_DEBUGS << "\t- " << ( (fAdded) ? "adding behaviour" : "skipping duplicate" ) << RLV_ENDL;

				if (fAdded) {	// If FALSE then this was a duplicate, there's no need to handle those
					if (!m_pGCTimer)
						m_pGCTimer = new RlvGCTimer();
					eRet = processAddRemCommand(rlvCmd);
					if (!RLV_RET_SUCCEEDED(eRet))
					{
						RlvCommand rlvCmdRem(rlvCmd, RLV_TYPE_REMOVE);
						itObj->second.removeCommand(rlvCmdRem);
						if (itObj->second.m_Commands.empty())
						{
							RLV_DEBUGS << "\t- command list empty => removing " << idCurObj << RLV_ENDL;
							m_Objects.erase(itObj);
						}
					}
//					notifyBehaviourObservers(rlvCmd, !fFromObj);
				}
				else
				{
					eRet = RLV_RET_SUCCESS_DUPLICATE;
				}
			}
			break;
		case RLV_TYPE_REMOVE:		// Checked: 2009-11-26 (RLVa-1.1.0f) | Modified: RLVa-1.1.0f
			{
				rlv_object_map_t::iterator itObj = m_Objects.find(idCurObj); bool fRemoved = false;
				if (itObj != m_Objects.end())
					fRemoved = itObj->second.removeCommand(rlvCmd);

				RLV_DEBUGS << "\t- " << ( (fRemoved) ? "removing behaviour"
													 : "skipping remove (unset behaviour or unknown object)") << RLV_ENDL;

				if (fRemoved) {	// Don't handle non-sensical removes
					eRet = processAddRemCommand(rlvCmd);
//					notifyBehaviourObservers(rlvCmd, !fFromObj);

					if (0 == itObj->second.m_Commands.size())
					{
						RLV_DEBUGS << "\t- command list empty => removing " << idCurObj << RLV_ENDL;
						RlvBehaviourDictionary::instance().clearModifiers(idCurObj);
						m_Objects.erase(itObj);
					}
				}
				else
				{
					eRet = RLV_RET_SUCCESS_UNSET;
				}
			}
			break;
		case RLV_TYPE_CLEAR:		// Checked: 2009-11-25 (RLVa-1.1.0f) | Modified: RLVa-1.1.0f
			eRet = processClearCommand(rlvCmd);
			break;
		case RLV_TYPE_FORCE:		// Checked: 2009-11-26 (RLVa-1.1.0f) | Modified: RLVa-1.1.0f
			eRet = processForceCommand(rlvCmd);
			break;
		case RLV_TYPE_REPLY:		// Checked: 2009-11-25 (RLVa-1.1.0f) | Modified: RLVa-1.1.0f
			eRet = processReplyCommand(rlvCmd);
			break;
		case RLV_TYPE_UNKNOWN:		// Checked: 2009-11-25 (RLVa-1.1.0f) | Modified: RLVa-1.1.0f
		default:
			eRet = RLV_RET_FAILED_PARAM;
			break;
	}
	RLV_ASSERT(RLV_RET_UNKNOWN != eRet);

	m_OnCommand(rlvCmd, eRet, !fFromObj);

	RLV_DEBUGS << "\t--> command " << ((eRet & RLV_RET_SUCCESS) ? "succeeded" : "failed") << RLV_ENDL;

	m_CurCommandStack.pop(); m_CurObjectStack.pop();
	return eRet;
}

// Checked: 2009-11-25 (RLVa-1.1.0f) | Modified: RLVa-1.1.0f
ERlvCmdRet RlvHandler::processCommand(const LLUUID& idObj, const std::string& strCommand, bool fFromObj)
{
	const RlvCommand rlvCmd(idObj, strCommand);
	if (STATE_STARTED != LLStartUp::getStartupState())
	{
		m_Retained.push_back(rlvCmd);
		return RLV_RET_RETAINED;
	}
	return processCommand(std::ref(rlvCmd), fFromObj);
}

// Checked: 2010-02-27 (RLVa-1.2.0a) | Modified: RLVa-1.1.0f
void RlvHandler::processRetainedCommands(ERlvBehaviour eBhvrFilter /*=RLV_BHVR_UNKNOWN*/, ERlvParamType eTypeFilter /*=RLV_TYPE_UNKNOWN*/)
{
	rlv_command_list_t::iterator itCmd = m_Retained.begin(), itCurCmd;
	while (itCmd != m_Retained.end())
	{
		itCurCmd = itCmd++;  // Point the loop iterator ahead

		const RlvCommand& rlvCmd = *itCurCmd;
		if ( ((RLV_BHVR_UNKNOWN == eBhvrFilter) || (rlvCmd.getBehaviourType() == eBhvrFilter)) && 
			 ((RLV_TYPE_UNKNOWN == eTypeFilter) || (rlvCmd.getParamType() == eTypeFilter)) )
		{
			processCommand(std::ref(rlvCmd), true);
			m_Retained.erase(itCurCmd);
		}
	}
}

ERlvCmdRet RlvHandler::processClearCommand(const RlvCommand& rlvCmd)
{
	const std::string& strFilter = rlvCmd.getParam(); std::string strCmdRem;

	rlv_object_map_t::const_iterator itObj = m_Objects.find(rlvCmd.getObjectID());
	if (itObj != m_Objects.end())	// No sense in clearing if we don't have any commands for this object
	{
		const RlvObject& rlvObj = itObj->second; bool fContinue = true;
		for (rlv_command_list_t::const_iterator itCmd = rlvObj.m_Commands.begin(), itCurCmd; 
				((fContinue) && (itCmd != rlvObj.m_Commands.end())); )
		{
			itCurCmd = itCmd++;		// Point itCmd ahead so it won't get invalidated if/when we erase a command

			const RlvCommand& rlvCmdRem = *itCurCmd; strCmdRem = rlvCmdRem.asString();
			if ( (strFilter.empty()) || (std::string::npos != strCmdRem.find(strFilter)) )
			{
				fContinue = (rlvObj.m_Commands.size() > 1); // rlvObj will become invalid once we remove the last command
				processCommand(rlvCmd.getObjectID(), strCmdRem.append("=y"), false);
			}
		}
	}

	// Let our observers know about clear commands
	ERlvCmdRet eRet = RLV_RET_SUCCESS;
	notifyCommandHandlers(&RlvExtCommandHandler::onClearCommand, rlvCmd, eRet, true);

	return RLV_RET_SUCCESS; // Don't fail clear commands even if the object didn't exist since it confuses people
}

bool RlvHandler::processIMQuery(const LLUUID& idSender, const std::string& strMessage)
{
	if ("@stopim" == strMessage)
	{
		// If the user can't start an IM session terminate it (if one is open) - always notify the sender in this case
		if (!RlvActions::canStartIM(idSender, true))
		{
			RlvUtil::sendBusyMessage(idSender, RlvStrings::getString(RlvStringKeys::StopIm::EndSessionRemote));
			if (RlvActions::hasOpenP2PSession(idSender))
			{
				LLAvatarActions::endIM(idSender);
				RlvUtil::notifyBlocked(RlvStringKeys::StopIm::EndSessionLocal, LLSD().with("NAME", LLSLURL("agent", idSender, "about").getSLURLString()), true);
			}
			return true;
		}

		// User can start an IM session so we do nothing - notify and hide it from the user only if IM queries are enabled
		if (!RlvSettings::getEnableIMQuery())
			return false;
		RlvUtil::sendBusyMessage(idSender, RlvStrings::getString(RlvStringKeys::StopIm::NoSession));
		return true;
	}
	else if (RlvSettings::getEnableIMQuery())
	{
		if ("@version" == strMessage)
		{
			RlvUtil::sendBusyMessage(idSender, RlvStrings::getVersion(LLUUID::null));
			return true;
		}
		else if ( ("@list" == strMessage) || ("@except" == strMessage) )
		{
			LLNotification::Params params;
			params.name = "RLVaListRequested";
			params.functor.function(boost::bind(&RlvHandler::onIMQueryListResponse, this, _1, _2));
			params.substitutions = LLSD().with("NAME_LABEL", LLSLURL("agent", idSender, "completename").getSLURLString()).with("NAME_SLURL", LLSLURL("agent", idSender, "about").getSLURLString());
			params.payload = LLSD().with("from_id", idSender).with("command", strMessage);

			class RlvPostponedOfferNotification : public LLPostponedNotification
			{
			protected:
				void modifyNotificationParams() override
				{
					LLSD substitutions = mParams.substitutions;
					substitutions["NAME"] = mName;
					mParams.substitutions = substitutions;
				}
			};
			LLPostponedNotification::add<RlvPostponedOfferNotification>(params, idSender, false);
			return true;
		}
	}
	return false;
}

void RlvHandler::onIMQueryListResponse(const LLSD& sdNotification, const LLSD sdResponse)
{
	const LLUUID idRequester = sdNotification["payload"]["from_id"].asUUID();

	const int idxOption = LLNotificationsUtil::getSelectedOption(sdNotification, sdResponse);
	if ( (idxOption == 0) || (idxOption == 1) )
	{
		if (idxOption == 1)
		{
			if (LLNotificationPtr pNotif = LLNotificationsUtil::find(sdNotification["id"]))
				pNotif->setIgnored(true);
		}

		const std::string& strCommand = sdNotification["payload"]["command"].asStringRef();
		if ("@list" == strCommand)
		{
			RlvUtil::sendIMMessage(idRequester, RlvFloaterBehaviours::getFormattedBehaviourString(ERlvBehaviourFilter::BEHAVIOURS_ONLY).append("\n").append(RlvStrings::getString("imquery_list_suffix")), '\n');
		}
		else if ("@except" == strCommand)
		{
			RlvUtil::sendIMMessage(idRequester, RlvFloaterBehaviours::getFormattedBehaviourString(ERlvBehaviourFilter::EXCEPTIONS_ONLY), '\n');
		}
	}
	else
	{
		RlvUtil::sendBusyMessage(idRequester, RlvStrings::getString("imquery_list_deny"));
	}
}

// ============================================================================
// Command specific helper functions - @setgroup
//

bool RlvHandler::checkActiveGroupThrottle(const LLUUID& idRlvObj)
{
	bool fAllow = m_GroupChangeExpiration.first.checkExpirationAndReset(llmax(RLV_SETGROUP_THROTTLE, 5.f));
	if (fAllow)
	{
		// (Un)owned expiration resets the last lock owner
		m_GroupChangeExpiration.second.setNull();
	}
	else if ( (!fAllow) && (m_GroupChangeExpiration.second.isNull()) && (ownsBehaviour(idRlvObj, RLV_BHVR_SETGROUP)) )
	{
		// The current lock owner wants to change the active group (title) before the expiration - allow once
		m_GroupChangeExpiration.second = idRlvObj;
		m_GroupChangeExpiration.first.setTimerExpirySec(llmax(RLV_SETGROUP_THROTTLE, 5.f));
		fAllow = true;
	}
	return fAllow;
}

void RlvHandler::changed(const LLUUID& idGroup, LLGroupChange change)
{
	// If we're receiving information about a group we're not interested in, we forgot a removeObserver somewhere
	RLV_ASSERT(idGroup == m_PendingGroupChange.first);

	if ( ((GC_ALL == change) || (GC_ROLE_DATA == change)) && (m_PendingGroupChange.first == idGroup) )
	{
		LLGroupMgr::instance().removeObserver(m_PendingGroupChange.first, this);
		setActiveGroupRole(m_PendingGroupChange.first, m_PendingGroupChange.second);
	}
}

bool RlvHandler::handleEvent(LLPointer<LLOldEvents::LLEvent> event, const LLSD& sdUserdata)
{
	// Ansariel: We really only want to handle "new group" events here. Registering for this event
	//           will still get us the custom "update grouptitle list" Firestorm events, sending us
	//           into a toggle-loop!
	if ("new group" != event->desc())
	{
		return false;
	}

	// NOTE: we'll fire once for every group the user belongs to so we need to manually keep track of pending changes
	static LLUUID s_idLastAgentGroup = LLUUID::null;
	if (s_idLastAgentGroup != gAgent.getGroupID())
	{
		s_idLastAgentGroup = gAgent.getGroupID();
		onActiveGroupChanged();
	}
	return false;
}

void RlvHandler::onActiveGroupChanged()
{
	// If the user managed to change their active group (= newly joined or created group) we need to reactivate the previous one
	if ( (!RlvActions::canChangeActiveGroup()) && (m_idAgentGroup != gAgent.getGroupID()) )
	{
		// Make sure they still belong to the group
		if ( (m_idAgentGroup.notNull()) && (!gAgent.isInGroup(m_idAgentGroup)) )
		{
			m_idAgentGroup.setNull();
		}

		// Notify them about the change
		const LLSD sdArgs = LLSD().with("GROUP_SLURL", (m_idAgentGroup.notNull()) ? llformat("secondlife:///app/group/%s/about", m_idAgentGroup.asString().c_str()) : "(none)");
		RlvUtil::notifyBlocked(RlvStringKeys::Blocked::GroupChange, sdArgs);

		setActiveGroup(m_idAgentGroup);
	}
	else
	{
		m_idAgentGroup = gAgent.getGroupID();

		// Allowed change - check if we still need to activate a role
		if ( (m_PendingGroupChange.first.notNull()) && (m_PendingGroupChange.first == m_idAgentGroup) )
		{
			setActiveGroupRole(m_PendingGroupChange.first, m_PendingGroupChange.second);
		}
	}
}

void RlvHandler::setActiveGroup(const LLUUID& idGroup)
{
	// If we have an existing observer fpr a different group, remove it
	if ( (m_PendingGroupChange.first.notNull()) && (m_PendingGroupChange.first != idGroup) )
	{
		LLGroupMgr::instance().removeObserver(m_PendingGroupChange.first, this);
		m_PendingGroupChange = std::make_pair(LLUUID::null, LLStringUtil::null);
	}

	if (gAgent.getGroupID() != idGroup)
	{
		// [Copy/paste from LLGroupActions::activate()]
		LLMessageSystem* msg = gMessageSystem;
		msg->newMessageFast(_PREHASH_ActivateGroup);
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
		msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
		msg->addUUIDFast(_PREHASH_GroupID, idGroup);
		gAgent.sendReliableMessage();
	}
	m_idAgentGroup = idGroup;
}

void RlvHandler::setActiveGroupRole(const LLUUID& idGroup, const std::string& strRole)
{
	// Check if we need a group change first
	if (gAgent.getGroupID() != idGroup)
	{
		setActiveGroup(idGroup);
		m_PendingGroupChange = std::make_pair(idGroup, strRole);
		return;
	}

	// Now that we have the correct group, check if we need to request the role information
	/*const*/ auto* pGroupData = LLGroupMgr::instance().getGroupData(idGroup);
	if ( ((!pGroupData) && (gAgent.isInGroup(idGroup))) || (!pGroupData->isRoleDataComplete()) )
	{
		if (m_PendingGroupChange.first.notNull())
			LLGroupMgr::instance().removeObserver(m_PendingGroupChange.first, this);
		m_PendingGroupChange = std::make_pair(idGroup, strRole);
		LLGroupMgr::instance().addObserver(idGroup, this);
		LLGroupMgr::instance().sendGroupRoleDataRequest(idGroup);
		return;
	}

	// We have everything - activate the requested role (if we can find it)
	if (pGroupData)
	{
		enum class EMatch { NoMatch, Partial, Exact } eMatch = EMatch::NoMatch; LLUUID idRole;
		for (const auto& roleData : pGroupData->mRoles)
		{
			// NOTE: exact matches take precedence over partial matches; in case of partial matches the last match wins
			const std::string& strRoleName = roleData.second->getRoleData().mRoleName;
			if (boost::istarts_with(strRoleName, strRole))
			{
				idRole = roleData.first;
				eMatch = (strRoleName.length() == strRole.length()) ? EMatch::Exact : EMatch::Partial;
				if (eMatch == EMatch::Exact)
					break;
			}
		}

		if (eMatch != EMatch::NoMatch)
		{
			RLV_INFOS << "Activating role '" << strRole << "' for group '" << pGroupData->mName << "'" << RLV_ENDL;
			LLGroupMgr::getInstance()->sendGroupTitleUpdate(idGroup, idRole);
		}
		else
		{
			RLV_INFOS << "Couldn't find role '" << strRole << "' in group '" << pGroupData->mName << "'" << RLV_ENDL;
		}
	}

	m_PendingGroupChange = std::make_pair(LLUUID::null, LLStringUtil::null);
}

// @setcam family
void RlvHandler::setCameraOverride(bool fOverride)
{
	if ( (fOverride) && (CAMERA_RLV_SETCAM_VIEW != gAgentCamera.getCameraPreset()) )
	{
		m_strCameraPresetRestore = gSavedSettings.getString("PresetCameraActive");
		gAgentCamera.switchCameraPreset(CAMERA_RLV_SETCAM_VIEW);
	}
	else if ( (!fOverride) && (CAMERA_RLV_SETCAM_VIEW == gAgentCamera.getCameraPreset() && (!RlvActions::isCameraPresetLocked())) )
	{
		// We need to clear it or it won't reset properly
		gSavedSettings.setString("PresetCameraActive", LLStringUtil::null);
		LLFloaterCamera::switchToPreset(m_strCameraPresetRestore);
		m_strCameraPresetRestore.clear();
	}
}

// ============================================================================
// Externally invoked event handlers
//

// Checked: 2010-08-29 (RLVa-1.2.1c) | Modified: RLVa-1.2.1c
void RlvHandler::onSitOrStand(bool fSitting)
{
	RlvSettings::updateLoginLastLocation();

	if ( (hasBehaviour(RLV_BHVR_STANDTP)) && (!fSitting) && (!m_posSitSource.isExactlyZero()) )
	{
		// NOTE: we need to do this due to the way @standtp triggers a forced teleport:
		//   - when standing we're called from LLVOAvatar::sitDown() which is called from LLVOAvatar::getOffObject()
		//   -> at the time sitDown() is called the avatar's parent is still the linkset it was sitting on so "isRoot()" on the avatar will
		//      return FALSE and we will crash in LLVOAvatar::getRenderPosition() when trying to teleport
		//   -> postponing the teleport until the next idle tick will ensure that everything has all been properly cleaned up
		doOnIdleOneTime(boost::bind(RlvUtil::forceTp, m_posSitSource));
		m_posSitSource.setZero();
	}
	else if ( (!fSitting) && (m_fPendingGroundSit) )
	{
		gAgent.setControlFlags(AGENT_CONTROL_SIT_ON_GROUND);
		send_agent_update(TRUE, TRUE);

		m_fPendingGroundSit = false;
		m_idPendingSitActor = m_idPendingUnsitActor;
	}

	if (isAgentAvatarValid())
	{
		const LLViewerObject* pSitObj = static_cast<const LLViewerObject*>(gAgentAvatarp->getParent());
		const LLUUID& idSitObj = (pSitObj) ? pSitObj->getID() : LLUUID::null;
		if (fSitting)
		{
			RlvBehaviourNotifyHandler::instance().onSit(idSitObj, !gRlvHandler.hasBehaviourExcept(RLV_BHVR_SIT, m_idPendingSitActor));
			m_idPendingSitActor.setNull();
		}
		else
		{
			RlvBehaviourNotifyHandler::instance().onStand(idSitObj, !gRlvHandler.hasBehaviourExcept(RLV_BHVR_UNSIT, m_idPendingUnsitActor));
			m_idPendingUnsitActor.setNull();
		}
	}
}

// Checked: 2010-03-11 (RLVa-1.2.0a) | Modified: RLVa-1.2.0a
void RlvHandler::onAttach(const LLViewerObject* pAttachObj, const LLViewerJointAttachment* pAttachPt)
{
	// Assertion - pAttachObj is never NULL always specifies the root
	RLV_ASSERT( (pAttachObj) && (pAttachObj == pAttachObj->getRootEdit()) );
	// Sanity check - we need to be called *after* LLViewerJointAttachment::addObject()
	RLV_ASSERT( (pAttachPt) && (pAttachPt->isObjectAttached(pAttachObj)) );
	if ( (!pAttachObj) || (!pAttachPt) || (!pAttachPt->isObjectAttached(pAttachObj)) )
		return;

	// Check if we already have an RlvObject instance for this object or one of its child prims
	for (rlv_object_map_t::iterator itObj = m_Objects.begin(); itObj != m_Objects.end(); ++itObj)
	{
		// Only if we haven't been able to find this object (= attachment that rezzed in) or if it's a rezzed prim attached from in-world
		if ( (!itObj->second.m_fLookup) || (!itObj->second.m_idxAttachPt) )
		{
			const LLViewerObject* pObj = gObjectList.findObject(itObj->first);
			if ( (pObj) && (pObj->getRootEdit()->getID() == pAttachObj->getID()) )
			{
				// Reset any lookup information we might have for this object
				itObj->second.m_fLookup = true;
				itObj->second.m_idxAttachPt = RlvAttachPtLookup::getAttachPointIndex(pAttachObj);
				itObj->second.m_idRoot = pAttachObj->getID();

				// We need to check this object for an active "@detach=n" and actually lock it down now that it's been attached somewhere
				if (itObj->second.hasBehaviour(RLV_BHVR_DETACH, false))
					gRlvAttachmentLocks.addAttachmentLock(pAttachObj->getID(), itObj->second.getObjectID());
			}
		}
	}

	// Fetch the inventory item if it isn't already (we need it in case of a reattach-on-detach) and rename it if appropriate
	if ( (STATE_STARTED == LLStartUp::getStartupState()) && (gInventory.isInventoryUsable()) )
	{
		RlvRenameOnWearObserver* pFetcher = new RlvRenameOnWearObserver(pAttachObj->getAttachmentItemID());
		pFetcher->startFetch();
		if (pFetcher->isFinished())
			pFetcher->done();
		else
			gInventory.addObserver(pFetcher);
	}
}

// Checked: 2010-03-11 (RLVa-1.2.0a) | Modified: RLVa-1.2.0a
void RlvHandler::onDetach(const LLViewerObject* pAttachObj, const LLViewerJointAttachment* pAttachPt)
{
	// Assertion - pAttachObj is never NULL always specifies the root
	RLV_ASSERT( (pAttachObj) && (pAttachObj == pAttachObj->getRootEdit()) );
	// Sanity check - we need to be called *before* LLViewerJointAttachment::removeObject()
	RLV_ASSERT( (pAttachPt) && (pAttachPt->isObjectAttached(pAttachObj)) );
	if ( (!pAttachObj) || (!pAttachPt) || (!pAttachPt->isObjectAttached(pAttachObj)) )
		return;

	// If the attachment is no longer attached then then the user "Drop"'ed this attachment somehow
	if (!pAttachObj->isAttachment())
	{
		// Check if we have any RlvObject instances for this object (or any of its child prims)
		for (rlv_object_map_t::iterator itObj = m_Objects.begin(); itObj != m_Objects.end(); ++itObj)
		{
			if ( (itObj->second.m_fLookup) && (itObj->second.m_idRoot == pAttachObj->getID()) )
			{
				// Clear the attachment point lookup since it's now an in-world prim
				itObj->second.m_idxAttachPt = false;

				// If this object has an active "@detach=n" then we need to release the attachment lock since it's no longer attached
				if (itObj->second.hasBehaviour(RLV_BHVR_DETACH, false))
					gRlvAttachmentLocks.removeAttachmentLock(pAttachObj->getID(), itObj->second.getObjectID());
			}
		}
	}
	else
	{
		// If it's still attached then we need to clean up any restrictions this object (or one of its child prims) may still have set
		rlv_object_map_t::iterator itObj = m_Objects.begin(), itCurObj;
		while (itObj != m_Objects.end())
		{
			itCurObj = itObj++;	// @clear will invalidate our iterator so point it ahead now
#ifdef RLV_DEBUG
			bool itObj = true;
			RLV_ASSERT(itObj);	// Little hack to push itObj out of scope and prevent it from being accidentally used below
#endif // RLV_DEBUG

			// NOTE: ObjectKill seems to happen in reverse (child prims are killed before the root is) so we can't use gObjectList here
			if (itCurObj->second.m_idRoot == pAttachObj->getID())
			{
				RLV_INFOS << "Clearing " << itCurObj->first.asString() << ":" << RLV_ENDL;
				processCommand(itCurObj->second.getObjectID(), "clear", true);
				RLV_INFOS << "\t-> done" << RLV_ENDL;
			}
		}

		if (pAttachObj->isTempAttachment())
		{
			removeBlockedObject(pAttachObj->getID());
		}
	}
}

void RlvHandler::onExperienceAttach(const LLSD& sdExperience, const std::string& strObjName)
{
	if (!RlvSettings::isAllowedExperience(sdExperience[LLExperienceCache::EXPERIENCE_ID].asUUID(), sdExperience[LLExperienceCache::MATURITY].asInteger()))
	{
		addBlockedObject(LLUUID::null, strObjName);

		const std::string strUrl = gAgent.getRegionCapability("AttachmentResources");
		if (!strUrl.empty())
		{
			LLCoros::instance().launch("RlvHandler::getAttachmentResourcesCoro", boost::bind(&RlvHandler::getAttachmentResourcesCoro, this, strUrl));
		}
	}
}

void RlvHandler::onExperienceEvent(const LLSD& sdEvent)
{
	const int nPermission = sdEvent["Permission"].asInteger();
	switch (nPermission)
	{
		case 4: // Attach
			{
				const LLUUID& idExperience = sdEvent["public_id"].asUUID();
				const std::string strObjName = sdEvent["ObjectName"].asString();
				LLExperienceCache::instance().get(idExperience, boost::bind(&RlvHandler::onExperienceAttach, this, _1, strObjName));
			}
			break;
		default:
			break;
	}
}

// Checked: 2010-03-13 (RLVa-1.2.0a) | Modified: RLVa-1.2.0a
bool RlvHandler::onGC()
{
	rlv_object_map_t::iterator itObj = m_Objects.begin(), itCurObj;
	while (itObj != m_Objects.end())
	{
		itCurObj = itObj++;	// @clear will invalidate our iterator so point it ahead now
#ifdef RLV_DEBUG
		bool itObj = true;
		RLV_ASSERT(itObj);
#endif // RLV_DEBUG

		// Temporary sanity check
		RLV_ASSERT(itCurObj->first == itCurObj->second.getObjectID());

		const LLViewerObject* pObj = gObjectList.findObject(itCurObj->second.getObjectID());
		if (!pObj)
		{
			// If the RlvObject once existed in gObjectList and now doesn't then expire it right away
			// If the RlvObject never existed in gObjectList and still doesn't then increase its "lookup misses" counter
			// but if that reaches 20 (we run every 30 seconds so that's about 10 minutes) then we'll expire it too
			if ( (itCurObj->second.m_fLookup) || (++itCurObj->second.m_nLookupMisses > 20) )
			{
				RLV_INFOS << "Garbage collecting " << itCurObj->first.asString() << ":" << RLV_ENDL;
				processCommand(itCurObj->first, "clear", true);
				RLV_INFOS << "\t-> done" << RLV_ENDL;
			}
		}
		else
		{
			// Assertion: if the GC encounters an RlvObject instance that hasn't existed in gObjectList up until now then
			//            it has to be a rezzed prim (if it was an attachment then RlvHandler::onAttach() should have caught it)
			RLV_ASSERT( (itCurObj->second.m_fLookup) || (!pObj->isAttachment()) );
			if (!itCurObj->second.m_fLookup)
			{
				RLV_INFOS << "Resolved missing object " << itCurObj->first.asString() << RLV_ENDL;
				itCurObj->second.m_fLookup = true;
				itCurObj->second.m_idxAttachPt = RlvAttachPtLookup::getAttachPointIndex(pObj);
				itCurObj->second.m_idRoot = pObj->getRootEdit()->getID();

				// NOTE: the following code should NEVER run (see assertion above), but just to be double-triple safety sure
				//	-> if it does run it likely means that there's a @detach=n in a *child* prim that we couldn't look up in onAttach()
				//  -> since RLV doesn't currently support @detach=n from child prims it's actually not such a big deal right now but still
				if ( (pObj->isAttachment()) && (itCurObj->second.hasBehaviour(RLV_BHVR_DETACH, false)) )
					gRlvAttachmentLocks.addAttachmentLock(pObj->getID(), itCurObj->second.getObjectID());
			}
		}
	}

	RLV_ASSERT(gRlvAttachmentLocks.verifyAttachmentLocks()); // Verify that we haven't leaked any attachment locks somehow

	// Clean up pending temp attachments that we were never able to resolve
	rlv_blocked_object_list_t::const_iterator itBlocked = m_BlockedObjects.cbegin(), itCurBlocked;
	while (itBlocked != m_BlockedObjects.end())
	{
		itCurBlocked = itBlocked++;
#ifdef RLV_DEBUG
		bool itBlocked = true;
		RLV_ASSERT(itBlocked);
#endif // RLV_DEBUG

		const LLUUID& idObj = std::get<0>(*itCurBlocked);
		if ( (idObj.notNull()) || (LLTimer::getTotalSeconds() - std::get<2>(*itCurBlocked) < 300.f) )
			continue;

		m_BlockedObjects.erase(itCurBlocked);
	}

	return (0 != m_Objects.size());	// GC will kill itself if it has nothing to do
}

// static
void RlvHandler::cleanupClass()
{
	gRlvHandler.cleanup();
}

// Checked: 2009-11-26 (RLVa-1.1.0f) | Added: RLVa-1.1.0f
void RlvHandler::onIdleStartup(void* pParam)
{
	LLTimer* pTimer = (LLTimer*)pParam;
	if (LLStartUp::getStartupState() < STATE_STARTED)
	{
		// We don't want to run this *too* often
		if ( (LLStartUp::getStartupState() >= STATE_MISC) && (pTimer->getElapsedTimeF32() >= 2.0) )
		{
			RlvHandler::instance().processRetainedCommands(RLV_BHVR_VERSION, RLV_TYPE_REPLY);
			RlvHandler::instance().processRetainedCommands(RLV_BHVR_VERSIONNEW, RLV_TYPE_REPLY);
			RlvHandler::instance().processRetainedCommands(RLV_BHVR_VERSIONNUM, RLV_TYPE_REPLY);
			pTimer->reset();
		}
	}
	else
	{
		// Clean-up
		gIdleCallbacks.deleteFunction(onIdleStartup, pParam);
		delete pTimer;
	}
}

// Checked: 2010-03-09 (RLVa-1.2.0a) | Added: RLVa-1.2.0a
void RlvHandler::onLoginComplete()
{
	RlvInventory::instance().fetchWornItems();
	RlvInventory::instance().fetchSharedInventory();
	RlvSettings::updateLoginLastLocation();

	m_ExperienceEventConn = LLExperienceLog::instance().addUpdateSignal(boost::bind(&RlvHandler::onExperienceEvent, this, _1));
	m_TeleportFailedConn = LLViewerParcelMgr::getInstance()->setTeleportFailedCallback(boost::bind(&RlvHandler::onTeleportFailed, this));
	m_TeleportFinishedConn = LLViewerParcelMgr::getInstance()->setTeleportFinishedCallback(boost::bind(&RlvHandler::onTeleportFinished, this, _1));

	processRetainedCommands();
}

void RlvHandler::onTeleportCallback(U64 hRegion, const LLVector3& posRegion, const LLVector3& vecLookAt, const LLUUID& idRlvObj)
{
	if (hRegion)
	{
		m_CurObjectStack.push(idRlvObj);

		const LLVector3d posGlobal = from_region_handle(hRegion) + (LLVector3d)posRegion;
		if (vecLookAt.isExactlyZero())
			gAgent.teleportViaLocation(posGlobal);
		else
			gAgent.teleportViaLocationLookAt(posGlobal, vecLookAt);

		m_CurObjectStack.pop();
	}
}

// Checked: 2010-04-05 (RLVa-1.2.0d) | Added: RLVa-1.2.0d
void RlvHandler::onTeleportFailed()
{
	setCanCancelTp(true);
}

// Checked: 2010-04-05 (RLVa-1.2.0d) | Added: RLVa-1.2.0d
void RlvHandler::onTeleportFinished(const LLVector3d& posArrival)
{
	setCanCancelTp(true);
}

// ============================================================================
// String/chat censoring functions
//

size_t utf8str_strlen(const std::string& utf8)
{
	const char* pUTF8 = utf8.c_str(); size_t length = 0;
	for (int idx = 0, cnt = utf8.length(); idx < cnt ;idx++)
	{
		// We're looking for characters that don't start with 10 as their high bits
		if ((pUTF8[idx] & 0xC0) != 0x80)
			length++;
	}
	return length;
}

std::string utf8str_chtruncate(const std::string& utf8, size_t length)
{
	if (0 == length)
		return std::string();
	if (utf8.length() <= length)
		return utf8;

	const char* pUTF8 = utf8.c_str(); int idx = 0;
	while ( (pUTF8[idx]) && (length > 0) )
	{
		// We're looking for characters that don't start with 10 as their high bits
		if ((pUTF8[idx] & 0xC0) != 0x80)
			length--;
		idx++;
	}

	return utf8.substr(0, idx);
}

// Checked: 2010-03-26 (RLVa-1.2.0b) | Modified: RLVa-1.0.0f
bool RlvHandler::filterChat(std::string& strUTF8Text, bool fFilterEmote) const
{
	if (strUTF8Text.empty())
		return false;

	bool fFilter = false;
	if (RlvUtil::isEmote(strUTF8Text))				// Check if it's an emote
	{
		if (fFilterEmote)							// Emote filtering depends on fFilterEmote
		{
			if ( (strUTF8Text.find_first_of("\"()*=^_?~") != std::string::npos) || 
				 (strUTF8Text.find(" -") != std::string::npos) || (strUTF8Text.find("- ") != std::string::npos) || 
				 (strUTF8Text.find("''") != std::string::npos) )
			{
				fFilter = true;						// Emote contains illegal character (or character sequence)
			}
			else if (!hasBehaviour(RLV_BHVR_EMOTE))
			{
				int idx = strUTF8Text.find('.');	// Truncate at 20 characters or at the dot (whichever is shorter)
				strUTF8Text = utf8str_chtruncate(strUTF8Text, ( (idx > 0) && (idx < 20) ) ? idx + 1 : 20);
			}
		}
	} 
	else if (strUTF8Text[0] == '/')					// Not an emote, but starts with a '/'
	{
		fFilter = (utf8str_strlen(strUTF8Text) > 7);// Allow as long if it's 6 characters or less
	}
	else if ( (!RlvSettings::getCanOOC()) ||
			  (strUTF8Text.length() < 4) || (strUTF8Text.compare(0, 2, "((")) || (strUTF8Text.compare(strUTF8Text.length() - 2, 2, "))")) )
	{
		fFilter = true;								// Regular chat (not OOC)
	}

	if (fFilter)
		strUTF8Text = (gSavedSettings.get<bool>(RlvSettingNames::ShowEllipsis)) ? "..." : "";
	return fFilter;
}

// Checked: 2010-11-29 (RLVa-1.3.0c) | Added: RLVa-1.3.0c
bool RlvHandler::hasException(ERlvBehaviour eBhvr) const
{
	return (m_Exceptions.find(eBhvr) != m_Exceptions.end());
}

// Checked: 2010-02-27 (RLVa-1.2.0b) | Modified: RLVa-1.2.0a
bool RlvHandler::redirectChatOrEmote(const std::string& strUTF8Text) const
{
	// Sanity check - @redirchat only for chat and @rediremote only for emotes
	ERlvBehaviour eBhvr = (!RlvUtil::isEmote(strUTF8Text)) ? RLV_BHVR_REDIRCHAT : RLV_BHVR_REDIREMOTE;
	if ( (strUTF8Text.empty()) || (!hasBehaviour(eBhvr)) )
		return false;

	if (RLV_BHVR_REDIRCHAT == eBhvr)
	{
		std::string strText = strUTF8Text;
		if (!filterChat(strText, false))
			return false;	// @sendchat wouldn't filter it so @redirchat won't redirect it either
	}

	for (rlv_exception_map_t::const_iterator itRedir = m_Exceptions.lower_bound(eBhvr), 
			endRedir = m_Exceptions.upper_bound(eBhvr); itRedir != endRedir; ++itRedir)
	{
		S32 nChannel = boost::get<S32>(itRedir->second.varOption);
		if (RlvActions::canSendChannel(nChannel))
		{
			if (!RlvSettings::getSplitRedirectChat())
				RlvUtil::sendChatReply(nChannel, strUTF8Text);
			else
				RlvUtil::sendChatReplySplit(nChannel, strUTF8Text);
		}
	}

	return true;
}

// ============================================================================
// Composite folders
//

#ifdef RLV_EXPERIMENTAL_COMPOSITEFOLDERS
	// Checked: 2009-12-18 (RLVa-1.1.0k) | Modified: RLVa-1.1.0i
	bool RlvHandler::getCompositeInfo(const LLInventoryCategory* pFolder, std::string* pstrName) const
	{
		if (pFolder)
		{
			// Composite folder naming: ^\.?[Folder]
			const std::string& cstrFolder = pFolder->getName();
			std::string::size_type idxStart = cstrFolder.find('['), idxEnd = cstrFolder.find(']', idxStart);
			if ( ((0 == idxStart) || (1 == idxStart)) && (idxEnd - idxStart > 1) )
			{
				if (pstrName)
					pstrName->assign(cstrFolder.substr(idxStart + 1, idxEnd - idxStart - 1));
				return true;
			}
		}
		return false;
	}

	// Checked: 2009-12-18 (RLVa-1.1.0k) | Modified: RLVa-1.1.0i
	bool RlvHandler::getCompositeInfo(const LLUUID& idItem, std::string* pstrName, LLViewerInventoryCategory** ppFolder) const
	{
/*
		LLViewerInventoryCategory* pRlvRoot; LLViewerInventoryItem* pItem;
		if ( (idItem.notNull()) && ((pRlvRoot = getSharedRoot()) != NULL) && 
			 (gInventory.isObjectDescendentOf(idItem, pRlvRoot->getUUID())) && ((pItem = gInventory.getItem(idItem)) != NULL) )
		{
			// We know it's an item in a folder under the shared root (we need its parent if it's a folded folder)
			LLViewerInventoryCategory* pFolder = gInventory.getCategory(pItem->getParentUUID());
			if (isFoldedFolder(pFolder, true, false))	// Don't check if the folder is a composite folder
				pFolder = gInventory.getCategory(pFolder->getParentUUID());

			if ( (pFolder) && (getCompositeInfo(pFolder, pstrName)) )
			{
				if (ppFolder)
					*ppFolder = pFolder;
				return true;
			}
		}
*/
		return false;
	}
#endif // RLV_EXPERIMENTAL_COMPOSITEFOLDERS

#ifdef RLV_EXPERIMENTAL_COMPOSITE_FOLDING
	// Checked:
	inline bool RlvHandler::isHiddenCompositeItem(const LLUUID& idItem, const std::string& cstrItemType) const
	{
		// An item that's part of a composite folder will be hidden from @getoutfit and @getattach if:
		//   (1) the composite name specifies either a wearable layer or an attachment point
		//   (2) the specified wearable layer or attachment point is worn and resides in the folder
		//   (3) cstrItemType isn't the specified wearable layer or attach point
		//
		// Example: #RLV/Separates/Shoes/ChiChi Pumps/.[shoes] with items: "Shoe Base", "Shoe (left foot)" and "Shoe (right foot)"
		//   -> as long as "Shoe Base" is worn, @getattach should not reflect "left foot", nor "right foot"
		std::string strComposite; LLViewerInventoryCategory* pFolder;
		LLWearableType::EType type; S32 idxAttachPt;
		if ( (getCompositeInfo(idItem, &strComposite, &pFolder)) && (cstrItemType != strComposite) )
		{
			LLUUID idCompositeItem;
			if ((type = LLWearable::typeNameToType(strComposite)) != WT_INVALID)
			{
				idCompositeItem = gAgent.getWearableItem(type);
			}
			else if ((idxAttachPt = getAttachPointIndex(strComposite, true)) != 0)
			{
				LLVOAvatar* pAvatar; LLViewerJointAttachment* pAttachmentPt;
				if ( ((pAvatar = gAgent.getAvatarObject()) != NULL) && 
					 ((pAttachmentPt = get_if_there(pAvatar->mAttachmentPoints, idxAttachPt, (LLViewerJointAttachment*)NULL)) != NULL) )
				{
					idCompositeItem = pAttachmentPt->getItemID();
				}
			}

			if ( (idCompositeItem.notNull()) && (gInventory.isObjectDescendentOf(idCompositeItem, pFolder->getUUID())) )
				return true;
		}
		return false;
	}
#endif // RLV_EXPERIMENTAL_COMPOSITEFOLDING

#ifdef RLV_EXPERIMENTAL_COMPOSITEFOLDERS
	// Checked: 2009-12-18 (RLVa-1.1.0k) | Modified: RLVa-1.1.0i
	bool RlvHandler::canTakeOffComposite(const LLInventoryCategory* pFolder) const
	{
		// Sanity check - if there's no folder or no avatar then there is nothing to take off
		LLVOAvatarSelf* pAvatar = gAgent.getAvatarObject();
		if ( (!pFolder) || (!pAvatar) )
			return false;
		// Sanity check - if nothing is locked then we can definitely take it off
		if ( (!gRlvAttachmentLocks.hasLockedAttachmentPoint(RLV_LOCK_REMOVE)) || 
			 (!gRlvWearableLocks.hasLockedWearableType(RLV_LOCK_REMOVE)) )
		{
			return true;
		}

/*
		LLInventoryModel::cat_array_t folders;
		LLInventoryModel::item_array_t items;
		RlvWearableItemCollector functor(pFolder->getUUID(), true, false);
		gInventory.collectDescendentsIf(pFolder->getUUID(), folders, items, FALSE, functor);

		for (S32 idxItem = 0, cntItem = items.count(); idxItem < cntItem; idxItem++)
		{
			const LLViewerInventoryItem* pItem = items.get(idxItem);
			switch (pItem->getType())
			{
				case LLAssetType::AT_BODYPART:
				case LLAssetType::AT_CLOTHING:
					{
						LLWearable* pWearable = gAgent.getWearableFromWearableItem(pItem->getUUID());
						if ( (pWearable) && (!isRemovable(pWearable->getType())) )
							return false;	// If one wearable in the folder is non-removeable then the entire folder should be
					}
					break;
				case LLAssetType::AT_OBJECT:
					{
						LLViewerObject* pObj = pAvatar->getWornAttachment(pItem->getUUID());
						if ( (pObj != NULL) && (isLockedAttachment(pObj, RLV_LOCK_REMOVE)) )
							return false;	// If one attachment in the folder is non-detachable then the entire folder should be
					}
					break;
				default:
					break;
			}
		}
*/
		return true;
	}

	// Checked: 2009-12-18 (RLVa-1.1.0k) | Modified: RLVa-1.1.0i
	bool RlvHandler::canWearComposite(const LLInventoryCategory* pFolder) const
	{
		// Sanity check - if there's no folder or no avatar then there is nothing to wear
		LLVOAvatar* pAvatar = gAgent.getAvatarObject();
		if ( (!pFolder) || (!pAvatar) )
			return false;
		// Sanity check - if nothing is locked then we can definitely wear it
		if ( (!gRlvAttachmentLocks.hasLockedAttachmentPoint(RLV_LOCK_ANY)) || (!gRlvWearableLocks.hacLockedWearableType(RLV_LOCK_ANY)) )
			return true;

/*
		LLInventoryModel::cat_array_t folders;
		LLInventoryModel::item_array_t items;
		RlvWearableItemCollector functor(pFolder->getUUID(), true, false);
		gInventory.collectDescendentsIf(pFolder->getUUID(), folders, items, FALSE, functor);

		for (S32 idxItem = 0, cntItem = items.count(); idxItem < cntItem; idxItem++)
		{
			LLViewerInventoryItem* pItem = items.get(idxItem);

			if (RlvForceWear::isWearingItem(pItem))
				continue; // Don't examine any items we're already wearing

			// A wearable layer or attachment point:
			//   - can't be "add locked"
			//   - can't be worn and "remove locked"
			//   - can't be worn and have its item belong to a *different* composite folder that we can't take off
			switch (pItem->getType())
			{
				case LLAssetType::AT_BODYPART:
				case LLAssetType::AT_CLOTHING:
					{
						// NOTE: without its asset we don't know what type the wearable is so we need to look at the item's flags instead
						LLWearableType::EType wtType = (LLWearableType::EType)(pItem->getFlags() & LLInventoryItem::II_FLAGS_WEARABLES_MASK);
						LLViewerInventoryCategory* pFolder;
						if ( (!isWearable(wtType)) ||
							 ( (gAgent.getWearable(wtType)) && (!isRemovable(wtType)) ) || 
							 ( (gRlvHandler.getCompositeInfo(gAgent.getWearableItem(wtType), NULL, &pFolder)) &&
							   (pFolder->getUUID() != pItem->getParentUUID()) && (!gRlvHandler.canTakeOffComposite(pFolder)) ) )
						{
							return false;
						}
					}
					break;
				case LLAssetType::AT_OBJECT:
					{
						// If we made it here then *something* is add/remove locked so we absolutely need to know its attachment point
						LLViewerJointAttachment* pAttachPt = getAttachPoint(pItem, true); 
						LLViewerInventoryCategory* pFolder;
						if ( (!pAttachPt) || (isLockedAttachment(pAttachPt, RLV_LOCK_ADD)) ||
							 ( (pAttachPt->getObject()) && (isLockedAttachment(pAttachPt, RLV_LOCK_REMOVE)) ) ||
							 ( (gRlvHandler.getCompositeInfo(pAttachPt->getItemID(), NULL, &pFolder)) &&
							   (pFolder->getUUID() != pItem->getParentUUID()) && (!gRlvHandler.canTakeOffComposite(pFolder)) ) )
						{
							return false;
						}
					}
					break;
				default:
					break;
			}
		}
*/
		return true;
	}
#endif // RLV_EXPERIMENTAL_COMPOSITEFOLDERS

// ============================================================================
// Initialization helper functions
//

bool RlvHandler::canEnable()
{
	return LLStartUp::getStartupState() <= STATE_LOGIN_CLEANUP;
}

bool RlvHandler::setEnabled(bool fEnable)
{
	if (m_fEnabled == fEnable)
		return fEnable;

	if ( (fEnable) && (canEnable()) )
	{
		RLV_INFOS << "Enabling Restrained Love API support - " << RlvStrings::getVersionAbout() << RLV_ENDL;
		m_fEnabled = true;

		// Initialize static classes
		RlvSettings::initClass();
		RlvStrings::initClass();

		RlvHandler::instance().addCommandHandler(new RlvEnvironment());
		RlvHandler::instance().addCommandHandler(new RlvExtGetSet());

		// Make sure we get notified when login is successful
		if (LLStartUp::getStartupState() < STATE_STARTED)
			LLAppViewer::instance()->setOnLoginCompletedCallback(boost::bind(&RlvHandler::onLoginComplete, RlvHandler::getInstance()));
		else
			RlvHandler::instance().onLoginComplete();

		// Set up RlvUIEnabler
		RlvUIEnabler::getInstance();

		// Reset to show assertions if the viewer version changed
		if (gSavedSettings.getString("LastRunVersion") != gLastRunVersion)
			gSavedSettings.set<bool>(RlvSettingNames::ShowAssertionFail, TRUE);
	}

	return m_fEnabled;
}

// ============================================================================
// Command handlers (RLV_TYPE_ADD and RLV_TYPE_REMOVE)
//

#define VERIFY_OPTION(x)		{ if (!(x)) { eRet = RLV_RET_FAILED_OPTION; break; } }
#define VERIFY_OPTION_REF(x)	{ if (!(x)) { eRet = RLV_RET_FAILED_OPTION; break; } fRefCount = true; }

// Checked: 2010-03-03 (RLVa-1.2.0a) | Modified: RLVa-1.2.0a
ERlvCmdRet RlvHandler::processAddRemCommand(const RlvCommand& rlvCmd)
{
	// NOTE: - at this point the command has already been:
	//            * added to the RlvObject
	//            * removed from the RlvObject (which still exists at this point even if this is the last restriction)
	//       - the object's UUID may or may not exist in gObjectList (see handling of @detach=n|y)

	ERlvBehaviour eBhvr = rlvCmd.getBehaviourType(); ERlvParamType eType = rlvCmd.getParamType();

	// Try a command processor first
	ERlvCmdRet eRet = rlvCmd.processCommand();
	if (RLV_RET_NO_PROCESSOR != eRet)
	{
		return eRet;
	}

	// Process the command the legacy way
	
	eRet = RLV_RET_SUCCESS; bool fRefCount = false; const std::string& strOption = rlvCmd.getOption();
	switch (eBhvr)
	{
		case RLV_BHVR_ATTACHTHIS:			// @attachthis[:<option>]=n|y
		case RLV_BHVR_DETACHTHIS:			// @detachthis[:<option>]=n|y
			eRet = onAddRemFolderLock(rlvCmd, fRefCount);
			break;
		case RLV_BHVR_ATTACHTHISEXCEPT:		// @attachthisexcept[:<option>]=n|y
		case RLV_BHVR_DETACHTHISEXCEPT:		// @detachthisexcept[:<option>]=n|y
			eRet = onAddRemFolderLockException(rlvCmd, fRefCount);
			break;
		case RLV_BHVR_ADDOUTFIT:			// @addoutfit[:<layer>]=n|y			- Checked: 2010-08-29 (RLVa-1.2.1c) | Modified: RLVa-1.2.1c
		case RLV_BHVR_REMOUTFIT:			// @remoutfit[:<layer>]=n|y			- Checked: 2010-08-29 (RLVa-1.2.1c) | Modified: RLVa-1.2.1c
			{
				// If there's an option it should specify a wearable type name (reference count on no option *and* a valid option)
				RlvCommandOptionGeneric rlvCmdOption;
				RlvCommandOptionHelper::parseOption(rlvCmd.getOption(), rlvCmdOption);
				VERIFY_OPTION_REF( (rlvCmdOption.isEmpty()) || (rlvCmdOption.isWearableType()) );

				// We need to flush any queued force-wear commands before changing the restrictions
				if (RlvForceWear::instanceExists())
					RlvForceWear::instance().done();

				ERlvLockMask eLock = (RLV_BHVR_ADDOUTFIT == eBhvr) ? RLV_LOCK_ADD : RLV_LOCK_REMOVE;
				for (int idxType = 0; idxType < LLWearableType::WT_COUNT; idxType++)
				{
					if ( (rlvCmdOption.isEmpty()) || ((LLWearableType::EType)idxType == rlvCmdOption.getWearableType()) )
					{
						if (RLV_TYPE_ADD == eType)
							gRlvWearableLocks.addWearableTypeLock((LLWearableType::EType)idxType, rlvCmd.getObjectID(), eLock);
						else
							gRlvWearableLocks.removeWearableTypeLock((LLWearableType::EType)idxType, rlvCmd.getObjectID(), eLock);
					}
				}
			}
			break;
		case RLV_BHVR_SHAREDWEAR:			// @sharedwear=n|y					- Checked: 2011-03-28 (RLVa-1.3.0g) | Added: RLVa-1.3.0g
		case RLV_BHVR_SHAREDUNWEAR:			// @sharedunwear=n|y				- Checked: 2011-03-28 (RLVa-1.3.0g) | Added: RLVa-1.3.0g
			{
				VERIFY_OPTION_REF(strOption.empty());

				RlvFolderLocks::folderlock_source_t lockSource(RlvFolderLocks::ST_SHAREDPATH, LLStringUtil::null);
				RlvFolderLocks::ELockScope eLockScope = RlvFolderLocks::SCOPE_SUBTREE;
				ERlvLockMask eLockType = (RLV_BHVR_SHAREDUNWEAR == eBhvr) ? RLV_LOCK_REMOVE : RLV_LOCK_ADD;

				if (RLV_TYPE_ADD == eType)
					RlvFolderLocks::instance().addFolderLock(lockSource, RlvFolderLocks::PERM_DENY, eLockScope, rlvCmd.getObjectID(), eLockType);
				else
					RlvFolderLocks::instance().removeFolderLock(lockSource, RlvFolderLocks::PERM_DENY, eLockScope, rlvCmd.getObjectID(), eLockType);
			}
			break;
		case RLV_BHVR_UNSHAREDWEAR:			// @unsharedwear=n|y				- Checked: 2011-03-28 (RLVa-1.3.0g) | Added: RLVa-1.3.0g
		case RLV_BHVR_UNSHAREDUNWEAR:		// @unsharedunwear=n|y				- Checked: 2011-03-28 (RLVa-1.3.0g) | Added: RLVa-1.3.0g
			{
				VERIFY_OPTION_REF(strOption.empty());

				// Lock down the inventory root
				RlvFolderLocks::folderlock_source_t lockSource(RlvFolderLocks::ST_ROOTFOLDER, 0);
				RlvFolderLocks::ELockScope eLockScope = RlvFolderLocks::SCOPE_SUBTREE;
				ERlvLockMask eLockType = (RLV_BHVR_UNSHAREDUNWEAR == eBhvr) ? RLV_LOCK_REMOVE : RLV_LOCK_ADD;

				if (RLV_TYPE_ADD == eType)
					RlvFolderLocks::instance().addFolderLock(lockSource, RlvFolderLocks::PERM_DENY, eLockScope, rlvCmd.getObjectID(), eLockType);
				else
					RlvFolderLocks::instance().removeFolderLock(lockSource, RlvFolderLocks::PERM_DENY, eLockScope, rlvCmd.getObjectID(), eLockType);

				// Add the #RLV shared folder as an exception
				lockSource = RlvFolderLocks::folderlock_source_t(RlvFolderLocks::ST_SHAREDPATH, LLStringUtil::null);
				if (RLV_TYPE_ADD == eType)
					RlvFolderLocks::instance().addFolderLock(lockSource, RlvFolderLocks::PERM_ALLOW, eLockScope, rlvCmd.getObjectID(), eLockType);
				else
					RlvFolderLocks::instance().removeFolderLock(lockSource, RlvFolderLocks::PERM_ALLOW, eLockScope, rlvCmd.getObjectID(), eLockType);
			}
			break;
		case RLV_BHVR_REDIRCHAT:			// @redirchat:<channel>=n|y			- Checked: 2010-03-26 (RLVa-1.2.0b) | Modified: RLVa-1.1.0h
		case RLV_BHVR_REDIREMOTE:			// @rediremote:<channel>=n|y		- Checked: 2010-03-26 (RLVa-1.2.0b) | Modified: RLVa-1.1.0h
			{
				// There should be an option which should specify a valid reply channel (if there's an empty option the command is invalid)
				S32 nChannel = 0;
				VERIFY_OPTION_REF( (LLStringUtil::convertToS32(strOption, nChannel)) && (RlvUtil::isValidReplyChannel(nChannel)) );

				if (RLV_TYPE_ADD == eType) 
					addException(rlvCmd.getObjectID(), eBhvr, nChannel);
				else
					removeException(rlvCmd.getObjectID(), eBhvr, nChannel);
			}
			break;
		case RLV_BHVR_NOTIFY:				// @notify:<params>=add|rem			- Checked: 2010-03-03 (RLVa-1.2.0a) | Modified: RLVa-1.2.0a
			{
				// There should be an option that we can successfully parse (if there's an empty option the command is invalid)
				S32 nChannel; std::string strFilter;
				VERIFY_OPTION_REF( (!strOption.empty()) && (rlvParseNotifyOption(strOption, nChannel, strFilter)) );

				if (RLV_TYPE_ADD == eType)
					RlvBehaviourNotifyHandler::getInstance()->addNotify(rlvCmd.getObjectID(), nChannel, strFilter);
				else
					RlvBehaviourNotifyHandler::getInstance()->removeNotify(rlvCmd.getObjectID(), nChannel, strFilter);
			}
			break;
		//
		// Unknown or invalid
		//
		case RLV_BHVR_UNKNOWN:
			// Pass unknown commands on to registered command handlers
			return (notifyCommandHandlers(&RlvExtCommandHandler::onAddRemCommand, rlvCmd, eRet, false)) ? eRet : RLV_RET_FAILED_UNKNOWN;
		default:
			// Fail with "Invalid param" if none of the above handled it
			eRet = RLV_RET_FAILED_PARAM;
			break;
	}

	// If this command represents a behaviour restriction that's been added/removed then we need to do some additional processing
	if ( (RLV_RET_SUCCESS == eRet) && (fRefCount) )
	{
		if (RLV_TYPE_ADD == eType)
		{
			if (rlvCmd.isStrict())
				addException(rlvCmd.getObjectID(), RLV_BHVR_PERMISSIVE, eBhvr);
			m_Behaviours[eBhvr]++;
			rlvCmd.markRefCounted();
		}
		else
		{
			if (rlvCmd.isStrict())
				removeException(rlvCmd.getObjectID(), RLV_BHVR_PERMISSIVE, eBhvr);
			m_Behaviours[eBhvr]--;
		}

		m_OnBehaviour(eBhvr, eType);
		if ( ((RLV_TYPE_ADD == eType) && (1 == m_Behaviours[eBhvr])) || ((RLV_TYPE_REMOVE == eType) && (0 == m_Behaviours[eBhvr])) )
			m_OnBehaviourToggle(eBhvr, eType);
	}

	return eRet;
}

// Handles reference counting of behaviours and tracks strict exceptions for @permissive (all restriction handlers should call this function)
ERlvCmdRet RlvCommandHandlerBaseImpl<RLV_TYPE_ADDREM>::processCommand(const RlvCommand& rlvCmd, RlvBhvrHandlerFunc* pHandlerFunc, RlvBhvrToggleHandlerFunc* pToggleHandlerFunc)
{
	ERlvBehaviour eBhvr = rlvCmd.getBehaviourType();
	bool fRefCount = false, fHasBhvr = gRlvHandler.hasBehaviour(eBhvr);

	ERlvCmdRet eRet = (*pHandlerFunc)(rlvCmd, fRefCount);

	// If this command represents a restriction that's been added/removed then we need to do some additional processing
	if ( (RLV_RET_SUCCESS == eRet) && (fRefCount) )
	{
		if (RLV_TYPE_ADD == rlvCmd.getParamType())
		{
			if (rlvCmd.isStrict())
				gRlvHandler.addException(rlvCmd.getObjectID(), RLV_BHVR_PERMISSIVE, eBhvr);
			gRlvHandler.m_Behaviours[eBhvr]++;
			rlvCmd.markRefCounted();
		}
		else
		{
			if (rlvCmd.isStrict())
				gRlvHandler.removeException(rlvCmd.getObjectID(), RLV_BHVR_PERMISSIVE, eBhvr);
			if (RlvObject* pRlvObj = gRlvHandler.getObject(rlvCmd.getObjectID()))
				pRlvObj->clearModifiers(eBhvr);
			gRlvHandler.m_Behaviours[eBhvr]--;
		}

		gRlvHandler.m_OnBehaviour(eBhvr, rlvCmd.getParamType());
		if (fHasBhvr != gRlvHandler.hasBehaviour(eBhvr))
		{
			if (pToggleHandlerFunc)
				(*pToggleHandlerFunc)(eBhvr, !fHasBhvr);
			gRlvHandler.m_OnBehaviourToggle(eBhvr, rlvCmd.getParamType());
		}
	}

	return eRet;
}

// Handles: @bhvr=n|y
template<>
ERlvCmdRet RlvBehaviourGenericHandler<RLV_OPTION_NONE>::onCommand(const RlvCommand& rlvCmd, bool& fRefCount)
{
	// There should be no option
	if (rlvCmd.hasOption())
		return RLV_RET_FAILED_OPTION;

	fRefCount = true;
	return RLV_RET_SUCCESS;
}

// Handles: @bhvr:<uuid>=n|y
template<>
ERlvCmdRet RlvBehaviourGenericHandler<RLV_OPTION_EXCEPTION>::onCommand(const RlvCommand& rlvCmd, bool& fRefCount)
{
	// There should be an option and it should specify a valid UUID
	LLUUID idException;
	if (!RlvCommandOptionHelper::parseOption(rlvCmd.getOption(), idException))
		return RLV_RET_FAILED_OPTION;

	if (RLV_TYPE_ADD == rlvCmd.getParamType())
		RlvHandler::instance().addException(rlvCmd.getObjectID(), rlvCmd.getBehaviourType(), idException);
	else
		RlvHandler::instance().removeException(rlvCmd.getObjectID(), rlvCmd.getBehaviourType(), idException);

	fRefCount = true;
	return RLV_RET_SUCCESS;
}

// Handles: @bhvr[:<uuid>]=n|y
template<>
ERlvCmdRet RlvBehaviourGenericHandler<RLV_OPTION_NONE_OR_EXCEPTION>::onCommand(const RlvCommand& rlvCmd, bool& fRefCount)
{
	// If there is an option then it should specify a valid UUID (but don't reference count)
	if (rlvCmd.hasOption())
	{
		ERlvCmdRet eRet = RlvBehaviourGenericHandler<RLV_OPTION_EXCEPTION>::onCommand(rlvCmd, fRefCount);
		fRefCount = false;
		return eRet;
	}
	return RlvBehaviourGenericHandler<RLV_OPTION_NONE>::onCommand(rlvCmd, fRefCount);
}

// Handles: @bhvr:<modifier>=n|y
template<>
ERlvCmdRet RlvBehaviourGenericHandler<RLV_OPTION_MODIFIER>::onCommand(const RlvCommand& rlvCmd, bool& fRefCount)
{
	// There should be an option and it should specify a valid modifier (RlvBehaviourModifier performs the appropriate type checks)
	RlvBehaviourModifier* pBhvrModifier = RlvBehaviourDictionary::instance().getModifierFromBehaviour(rlvCmd.getBehaviourType());
	RlvBehaviourModifierValue modValue;
	if ( (!rlvCmd.hasOption()) || (!pBhvrModifier) || (!pBhvrModifier->convertOptionValue(rlvCmd.getOption(), pBhvrModifier->getType(), modValue)) )
		return RLV_RET_FAILED_OPTION;

	// HACK-RLVa: reference counting doesn't happen until control returns to our caller but the modifier callbacks will happen now so we need to adjust the reference counts here
	if (RLV_TYPE_ADD == rlvCmd.getParamType())
	{
		gRlvHandler.m_Behaviours[rlvCmd.getBehaviourType()]++;
		pBhvrModifier->addValue(modValue, rlvCmd.getObjectID(), rlvCmd.getBehaviourType());
		gRlvHandler.m_Behaviours[rlvCmd.getBehaviourType()]--;
	}
	else
	{
		gRlvHandler.m_Behaviours[rlvCmd.getBehaviourType()]--;
		pBhvrModifier->removeValue(modValue, rlvCmd.getObjectID(), rlvCmd.getBehaviourType());
		gRlvHandler.m_Behaviours[rlvCmd.getBehaviourType()]++;
	}

	fRefCount = true;
	return RLV_RET_SUCCESS;
}

// Handles: @bhvr=n, @bhvr:<global modifier>=n|y and @bhvr:<local modifier>=force
template<>
ERlvCmdRet RlvBehaviourGenericHandler<RLV_OPTION_NONE_OR_MODIFIER>::onCommand(const RlvCommand& rlvCmd, bool& fRefCount)
{
	if ( (rlvCmd.getParamType() & RLV_TYPE_ADDREM) && (rlvCmd.hasOption()) )
	{
		// @bhvr:<global modifier>=n|y : if there is an option then it should specify a valid global modifier and if so we reference count
		return RlvBehaviourGenericHandler<RLV_OPTION_MODIFIER>::onCommand(rlvCmd, fRefCount);
	}
	else if (rlvCmd.getParamType() == RLV_TYPE_FORCE)
	{
		// @bhvr:<local modifier>=force : local modifiers hide behind their primary behaviour which knows how to handle them
		return rlvCmd.getBehaviourInfo()->processModifier(rlvCmd);
	}

	// @bhvr=n : add the default option on an empty modifier if needed
	RlvBehaviourModifier* pBhvrModifier = RlvBehaviourDictionary::instance().getModifierFromBehaviour(rlvCmd.getBehaviourType());
	if ( (pBhvrModifier) && (pBhvrModifier->getAddDefault()) )
	{
		// HACK-RLVa: reference counting doesn't happen until control returns to our caller but the modifier callbacks will happen now so we need to adjust the reference counts here
		if (RLV_TYPE_ADD == rlvCmd.getParamType())
		{
			gRlvHandler.m_Behaviours[rlvCmd.getBehaviourType()]++;
			pBhvrModifier->addValue(pBhvrModifier->getDefaultValue(), rlvCmd.getObjectID(), rlvCmd.getBehaviourType());
			gRlvHandler.m_Behaviours[rlvCmd.getBehaviourType()]--;
		}
		else
		{
			gRlvHandler.m_Behaviours[rlvCmd.getBehaviourType()]--;
			pBhvrModifier->removeValue(pBhvrModifier->getDefaultValue(), rlvCmd.getObjectID(), rlvCmd.getBehaviourType());
			gRlvHandler.m_Behaviours[rlvCmd.getBehaviourType()]++;
		}
	}

	fRefCount = true;
	return RLV_RET_SUCCESS;
}

// Handles: @addattach[:<attachpt>]=n|y and @remattach[:<attachpt>]=n|y
template<> template<>
ERlvCmdRet RlvBehaviourAddRemAttachHandler::onCommand(const RlvCommand& rlvCmd, bool& fRefCount)
{
	// Sanity check - if there's an option it should specify a valid attachment point name
	S32 idxAttachPt = RlvAttachPtLookup::getAttachPointIndex(rlvCmd.getOption());
	if ( (!idxAttachPt) && (!rlvCmd.getOption().empty())  )
		return RLV_RET_FAILED_OPTION;

	if (!isAgentAvatarValid())
		return RLV_RET_FAILED;

	// We need to flush any queued force-wear commands before changing the restrictions
	if (RlvForceWear::instanceExists())
		RlvForceWear::instance().done();

	ERlvLockMask eLock = (RLV_BHVR_REMATTACH == rlvCmd.getBehaviourType()) ? RLV_LOCK_REMOVE : RLV_LOCK_ADD;
	for (LLVOAvatar::attachment_map_t::const_iterator itAttach = gAgentAvatarp->mAttachmentPoints.begin(); 
			itAttach != gAgentAvatarp->mAttachmentPoints.end(); ++itAttach)
	{
		if ( (0 == idxAttachPt) || (itAttach->first == idxAttachPt) )
		{
			if (RLV_TYPE_ADD == rlvCmd.getParamType())
				gRlvAttachmentLocks.addAttachmentPointLock(itAttach->first, rlvCmd.getObjectID(), eLock);
			else
				gRlvAttachmentLocks.removeAttachmentPointLock(itAttach->first, rlvCmd.getObjectID(), eLock);
		}
	}

	fRefCount = rlvCmd.getOption().empty();	// Only reference count global locks
	return RLV_RET_SUCCESS;
}

// Handles: @buy=n|y toggles
template<> template<>
void RlvBehaviourToggleHandler<RLV_BHVR_BUY>::onCommandToggle(ERlvBehaviour eBhvr, bool fHasBhvr)
{
	// Start or stop filtering opening the buy, buy contents and pay object floaters
	if (fHasBhvr)
	{
		RLV_VERIFY(RlvUIEnabler::instance().addGenericFloaterFilter("buy_object", RlvStringKeys::Blocked::Generic));
		RLV_VERIFY(RlvUIEnabler::instance().addGenericFloaterFilter("buy_object_contents", RlvStringKeys::Blocked::Generic));
		RLV_VERIFY(RlvUIEnabler::instance().addGenericFloaterFilter("pay_object", RlvStringKeys::Blocked::Generic));
	}
	else
	{
		RLV_VERIFY(RlvUIEnabler::instance().removeGenericFloaterFilter("buy_object"));
		RLV_VERIFY(RlvUIEnabler::instance().removeGenericFloaterFilter("buy_object_contents"));
		RLV_VERIFY(RlvUIEnabler::instance().removeGenericFloaterFilter("pay_object"));
	}
}

// Handles: @detach[:<attachpt>]=n|y
template<> template<>
ERlvCmdRet RlvBehaviourHandler<RLV_BHVR_DETACH>::onCommand(const RlvCommand& rlvCmd, bool& fRefCount)
{
	// We need to flush any queued force-wear commands before changing the restrictions
	if (RlvForceWear::instanceExists())
		RlvForceWear::instance().done();

	if (rlvCmd.getOption().empty())	// @detach=n|y - RLV_LOCK_REMOVE locks an attachment *object*
	{
		// The object may or may not exist (it may not have rezzed yet, or it may have already been killed):
		//   * @detach=n: - if it has rezzed then we'll already have looked up what we need (see next line if it's not an attachment)
		//                - if it hasn't rezzed yet then it's a @detach=n from a non-attachment and RlvHandler::onAttach() takes care of it
		//   * @detach=y: - if it ever rezzed as an attachment we'll have cached the UUID of its root
		//                - if it never rezzed as an attachment there won't be a lock to remove
		RlvHandler::rlv_object_map_t::const_iterator itObj = gRlvHandler.m_Objects.find(rlvCmd.getObjectID());
		if ( (itObj != gRlvHandler.m_Objects.end()) && (itObj->second.hasLookup()) && (itObj->second.getAttachPt()) )
		{
			if (RLV_TYPE_ADD == rlvCmd.getParamType())
				gRlvAttachmentLocks.addAttachmentLock(itObj->second.getRootID(), itObj->first);
			else
				gRlvAttachmentLocks.removeAttachmentLock(itObj->second.getRootID(), itObj->first);
		}
	}
	else							// @detach:<attachpt>=n|y - RLV_LOCK_ADD and RLV_LOCK_REMOVE locks an attachment *point*
	{
		// The attachment point index should always be non-zero for @detach:<attachpt>=n|y
		S32 idxAttachPt = RlvAttachPtLookup::getAttachPointIndex(rlvCmd.getOption());
		if (0 == idxAttachPt)
			return RLV_RET_FAILED_OPTION;

		if (RLV_TYPE_ADD == rlvCmd.getParamType())
			gRlvAttachmentLocks.addAttachmentPointLock(idxAttachPt, rlvCmd.getObjectID(), (ERlvLockMask)(RLV_LOCK_ADD | RLV_LOCK_REMOVE));
		else
			gRlvAttachmentLocks.removeAttachmentPointLock(idxAttachPt, rlvCmd.getObjectID(), (ERlvLockMask)(RLV_LOCK_ADD | RLV_LOCK_REMOVE));
	}

	fRefCount = false;	// Don't reference count @detach[:<option>]=n
	return RLV_RET_SUCCESS;
}

// Checked: 2010-11-30 (RLVa-1.3.0b) | Added: RLVa-1.3.0b
ERlvCmdRet RlvHandler::onAddRemFolderLock(const RlvCommand& rlvCmd, bool& fRefCount)
{
	RlvCommandOptionGeneric rlvCmdOption;
	RlvCommandOptionHelper::parseOption(rlvCmd.getOption(), rlvCmdOption);

	RlvFolderLocks::folderlock_source_t lockSource;
	if (rlvCmdOption.isEmpty())
	{
		lockSource = RlvFolderLocks::folderlock_source_t(RlvFolderLocks::ST_ATTACHMENT, rlvCmd.getObjectID());
	}
	else if (rlvCmdOption.isSharedFolder())
	{
		lockSource = RlvFolderLocks::folderlock_source_t(RlvFolderLocks::ST_SHAREDPATH, rlvCmd.getOption());
	}
	else if (rlvCmdOption.isAttachmentPoint())
	{
		lockSource = RlvFolderLocks::folderlock_source_t(RlvFolderLocks::ST_ATTACHMENTPOINT, RlvAttachPtLookup::getAttachPointIndex(rlvCmdOption.getAttachmentPoint()));
	}
	else if (rlvCmdOption.isWearableType())
	{
		lockSource = RlvFolderLocks::folderlock_source_t(RlvFolderLocks::ST_WEARABLETYPE, rlvCmdOption.getWearableType());
	}
	else
	{
		fRefCount = false;	// Don't reference count failure
		return RLV_RET_FAILED_OPTION;
	}

	// Determine the lock type
	ERlvLockMask eLockType = (RLV_BHVR_ATTACHTHIS == rlvCmd.getBehaviourType()) ? RLV_LOCK_ADD : RLV_LOCK_REMOVE;

	// Determine the folder lock options from the issued behaviour
	RlvFolderLocks::ELockPermission eLockPermission = RlvFolderLocks::PERM_DENY;
	RlvFolderLocks::ELockScope eLockScope = (RlvBehaviourInfo::FORCEWEAR_SUBTREE & rlvCmd.getBehaviourFlags()) ? RlvFolderLocks::SCOPE_SUBTREE : RlvFolderLocks::SCOPE_NODE;

	if (RLV_TYPE_ADD == rlvCmd.getParamType())
		RlvFolderLocks::instance().addFolderLock(lockSource, eLockPermission, eLockScope, rlvCmd.getObjectID(), eLockType);
	else
		RlvFolderLocks::instance().removeFolderLock(lockSource, eLockPermission, eLockScope, rlvCmd.getObjectID(), eLockType);

	fRefCount = true;
	return RLV_RET_SUCCESS;
}

// Checked: 2011-03-27 (RLVa-1.3.0g) | Added: RLVa-1.3.0g
ERlvCmdRet RlvHandler::onAddRemFolderLockException(const RlvCommand& rlvCmd, bool& fRefCount)
{
	// Sanity check - the option should specify a shared folder path
	RlvCommandOptionGeneric rlvCmdOption;
	RlvCommandOptionHelper::parseOption(rlvCmd.getOption(), rlvCmdOption);
	if (!rlvCmdOption.isSharedFolder())
		return RLV_RET_FAILED_OPTION;

	ERlvBehaviour eBhvr = rlvCmd.getBehaviourType();

	// Determine the lock type
	ERlvLockMask eLockType = (RLV_BHVR_ATTACHTHISEXCEPT == eBhvr) ? RLV_LOCK_ADD : RLV_LOCK_REMOVE;

	// Determine the folder lock options from the issued behaviour
	RlvFolderLocks::ELockPermission eLockPermission = RlvFolderLocks::PERM_ALLOW;
	RlvFolderLocks::ELockScope eLockScope = (RlvBehaviourInfo::FORCEWEAR_SUBTREE & rlvCmd.getBehaviourFlags()) ? RlvFolderLocks::SCOPE_SUBTREE : RlvFolderLocks::SCOPE_NODE;

	RlvFolderLocks::folderlock_source_t lockSource(RlvFolderLocks::ST_SHAREDPATH, rlvCmd.getOption());
	if (RLV_TYPE_ADD == rlvCmd.getParamType())
		RlvFolderLocks::instance().addFolderLock(lockSource, eLockPermission, eLockScope, rlvCmd.getObjectID(), eLockType);
	else
		RlvFolderLocks::instance().removeFolderLock(lockSource, eLockPermission, eLockScope, rlvCmd.getObjectID(), eLockType);

	fRefCount = true;
	return RLV_RET_SUCCESS;
}

// Handles: @edit=n|y toggles
template<> template<>
void RlvBehaviourToggleHandler<RLV_BHVR_EDIT>::onCommandToggle(ERlvBehaviour eBhvr, bool fHasBhvr)
{
	if (fHasBhvr)
	{
		// Turn off "View / Highlight Transparent"
		LLDrawPoolAlpha::sShowDebugAlpha = FALSE;

		// Hide the beacons floater if it's currently visible
		if (LLFloaterReg::instanceVisible("beacons"))
			LLFloaterReg::hideInstance("beacons");

		// Hide the build floater
		LLToolMgr::instance().leaveBuildMode();
	}

	// Start or stop filtering opening the beacons floater
	if (fHasBhvr)
	{
		RLV_VERIFY(RlvUIEnabler::instance().addGenericFloaterFilter("beacons"));
	}
	else
	{
		RLV_VERIFY(RlvUIEnabler::instance().removeGenericFloaterFilter("beacons"));
	}
}

// Handles: @pay=n|y toggles
template<> template<>
void RlvBehaviourToggleHandler<RLV_BHVR_PAY>::onCommandToggle(ERlvBehaviour eBhvr, bool fHasBhvr)
{
	// Start or stop filtering opening the pay avatar floater
	if (fHasBhvr)
	{
		RLV_VERIFY(RlvUIEnabler::instance().addGenericFloaterFilter("pay_resident", RlvStringKeys::Blocked::Generic));
	}
	else
	{
		RLV_VERIFY(RlvUIEnabler::instance().removeGenericFloaterFilter("pay_resident"));
	}
}

// Handles: @setoverlay=n|y
template<> template<>
ERlvCmdRet RlvBehaviourHandler<RLV_BHVR_SETOVERLAY>::onCommand(const RlvCommand& rlvCmd, bool& fRefCount)
{
	ERlvCmdRet eRet = RlvBehaviourGenericHandler<RLV_OPTION_NONE_OR_MODIFIER>::onCommand(rlvCmd, fRefCount);
	if ( (RLV_RET_SUCCESS == eRet) && (!rlvCmd.isModifier()) )
	{
		if (gRlvHandler.hasBehaviour(rlvCmd.getObjectID(), rlvCmd.getBehaviourType()))
		{
			LLVfxManager::instance().addEffect(new RlvOverlayEffect(gRlvHandler.getCurrentObject()));
		}
		else
		{
			LLVfxManager::instance().removeEffect<RlvOverlayEffect>(gRlvHandler.getCurrentObject());
		}
	}

	// Refresh overlay effects according to object hierarchy
	std::list<RlvOverlayEffect*> effects;
	if (LLVfxManager::instance().getEffects<RlvOverlayEffect>(effects))
	{
		auto itActiveEffect = std::find_if(effects.begin(), effects.end(), [](const LLVisualEffect* pEffect) { return pEffect->getEnabled(); });
		if (effects.end() == itActiveEffect)
		{
			// If nothing is active just pick the first one to activate
			itActiveEffect = effects.begin();
		}

		const LLUUID idActiveRootObj = (effects.end() != itActiveEffect) ? Rlv::getObjectRootId((*itActiveEffect)->getId()) : LLUUID::null;
		for (RlvOverlayEffect* pEffect : effects)
		{
			bool isActive = (idActiveRootObj.isNull() && pEffect == effects.front()) || (Rlv::getObjectRootId(pEffect->getId()) == idActiveRootObj);
			int nPriority = (isActive) ? 256 - Rlv::getObjectLinkNumber(pEffect->getId()) : pEffect->getPriority();
			LLVfxManager::instance().updateEffect(pEffect, isActive, nPriority);
			pEffect->setBlockTouch(gRlvHandler.hasBehaviour(pEffect->getId(), RLV_BHVR_SETOVERLAY_TOUCH));
		}
	}

	return eRet;
}

// Handles: @setoverlay_touch=n
template<> template<>
ERlvCmdRet RlvBehaviourHandler<RLV_BHVR_SETOVERLAY_TOUCH>::onCommand(const RlvCommand& rlvCmd, bool& fRefCount)
{
	if (RlvOverlayEffect* pOverlayEffect = LLVfxManager::instance().getEffect<RlvOverlayEffect>(rlvCmd.getObjectID()))
	{
		pOverlayEffect->setBlockTouch( RLV_TYPE_ADD == rlvCmd.getParamType() );
	}

	return RLV_RET_SUCCESS;
}

// Handles: @setsphere=n|y
template<> template<>
ERlvCmdRet RlvBehaviourHandler<RLV_BHVR_SETSPHERE>::onCommand(const RlvCommand& rlvCmd, bool& fRefCount)
{
	// *TODO: this needs to be done in a cleaner way but FS needs to release ASAP
	if (RLV_TYPE_ADD == rlvCmd.getParamType() && gRlvHandler.m_Behaviours[RLV_BHVR_SETSPHERE] >= 6)
		return RLV_RET_FAILED_LOCK;

	ERlvCmdRet eRet = RlvBehaviourGenericHandler<RLV_OPTION_NONE_OR_MODIFIER>::onCommand(rlvCmd, fRefCount);
	if ( (RLV_RET_SUCCESS == eRet) && (!rlvCmd.isModifier()) )
	{
		if (gRlvHandler.hasBehaviour(rlvCmd.getObjectID(), rlvCmd.getBehaviourType()))
		{
			LLVfxManager::instance().addEffect(new RlvSphereEffect(rlvCmd.getObjectID()));

			Rlv::forceAtmosphericShadersIfAvailable();

			// If we're not using deferred but are using Windlight shaders we need to force use of FBO and depthmap texture
			if ( (!LLPipeline::sRenderDeferred) && (LLPipeline::WindLightUseAtmosShaders) && (!LLPipeline::sUseDepthTexture) )
			{
				LLRenderTarget::sUseFBO = true;
				LLPipeline::sUseDepthTexture = true;

				gPipeline.releaseGLBuffers();
				gPipeline.createGLBuffers();
				gPipeline.resetVertexBuffers();
				LLViewerShaderMgr::instance()->setShaders();
			}
			else if (!gPipeline.mDeferredLight.isComplete())
			{
				// In case of deferred with no shadows, no ambient occlusion, no depth of field, and no antialiasing
				gPipeline.releaseGLBuffers();
				gPipeline.createGLBuffers();
				RLV_ASSERT(gPipeline.mDeferredLight.isComplete());
			}
		}
		else
		{
			LLVfxManager::instance().removeEffect<RlvSphereEffect>(gRlvHandler.getCurrentObject());
		}
	}
	return eRet;
}

// Handles: @sendchannel[:<channel>]=n|y and @sendchannel_except[:<channel>]=n|y
template<> template<>
ERlvCmdRet RlvBehaviourSendChannelHandler::onCommand(const RlvCommand& rlvCmd, bool& fRefCount)
{
	// If there's an option then it should be a valid (= positive and non-zero) chat channel
	if (rlvCmd.hasOption())
	{
		S32 nChannel = 0;
		if ( (!RlvCommandOptionHelper::parseOption(rlvCmd.getOption(), nChannel)) || (nChannel <= 0) )
			return RLV_RET_FAILED_OPTION;

		if (RLV_TYPE_ADD == rlvCmd.getParamType())
			gRlvHandler.addException(rlvCmd.getObjectID(),  rlvCmd.getBehaviourType(), nChannel);
		else
			gRlvHandler.removeException(rlvCmd.getObjectID(),  rlvCmd.getBehaviourType(), nChannel);
	}
	else
	{
		fRefCount = true;
	}
	return RLV_RET_SUCCESS;
}

// Handles: @recvim[:<uuid|range>]=n|y, @sendim[:<uuid|range>]=n|y and @startim[:<uuid|range>]=n|y
template<> template<>
ERlvCmdRet RlvBehaviourRecvSendStartIMHandler::onCommand(const RlvCommand& rlvCmd, bool& fRefCount)
{
	ERlvCmdRet eRet = RlvBehaviourGenericHandler<RLV_OPTION_NONE_OR_EXCEPTION>::onCommand(rlvCmd, fRefCount);
	if ( (RLV_RET_SUCCESS != eRet) && (rlvCmd.hasOption()) )
	{
		// Check for <dist_min>[;<dist_max>] option
		std::vector<std::string> optionList; float nDistMin = F32_MAX, nDistMax = F32_MAX;
		if ( (!RlvCommandOptionHelper::parseStringList(rlvCmd.getOption(), optionList)) || (optionList.size() > 2) ||
			 (!RlvCommandOptionHelper::parseOption(optionList[0], nDistMin)) || (nDistMin < 0) ||
			 ( (optionList.size() >= 2) && (!RlvCommandOptionHelper::parseOption(optionList[1], nDistMax)) ) || (nDistMax < 0) )
		{
			return RLV_RET_FAILED_OPTION;
		}

		// Valid option(s) - figure out which modifier(s) to change
		ERlvBehaviourModifier eModDistMin, eModDistMax;
		switch (rlvCmd.getBehaviourType())
		{
			case RLV_BHVR_RECVIM:
				eModDistMin = RLV_MODIFIER_RECVIMDISTMIN; eModDistMax = RLV_MODIFIER_RECVIMDISTMAX;
				break;
			case RLV_BHVR_SENDIM:
				eModDistMin = RLV_MODIFIER_SENDIMDISTMIN; eModDistMax = RLV_MODIFIER_SENDIMDISTMAX;
				break;
			case RLV_BHVR_STARTIM:
				eModDistMin = RLV_MODIFIER_STARTIMDISTMIN; eModDistMax = RLV_MODIFIER_STARTIMDISTMAX;
				break;
			default:
				return RLV_RET_FAILED_OPTION;
		}

		RlvBehaviourModifier *pBhvrModDistMin = RlvBehaviourDictionary::instance().getModifier(eModDistMin), *pBhvrModDistMax = RlvBehaviourDictionary::instance().getModifier(eModDistMax);
		if (RLV_TYPE_ADD == rlvCmd.getParamType())
		{
			pBhvrModDistMin->addValue(nDistMin * nDistMin, rlvCmd.getObjectID(), rlvCmd.getBehaviourType());
			if (optionList.size() >= 2)
				pBhvrModDistMax->addValue(nDistMax * nDistMax, rlvCmd.getObjectID(), rlvCmd.getBehaviourType());
		}
		else
		{
			pBhvrModDistMin->removeValue(nDistMin * nDistMin, rlvCmd.getObjectID(), rlvCmd.getBehaviourType());
			if (optionList.size() >= 2)
				pBhvrModDistMax->removeValue(nDistMax * nDistMax, rlvCmd.getObjectID(), rlvCmd.getBehaviourType());
		}

		fRefCount = true;
		return RLV_RET_SUCCESS;
	}
	return eRet;
}

// Handles: @sendim=n|y toggles
template<> template<>
void RlvBehaviourToggleHandler<RLV_BHVR_SENDIM>::onCommandToggle(ERlvBehaviour eBhvr, bool fHasBhvr)
{
	gSavedPerAccountSettings.getControl("DoNotDisturbModeResponse")->setHiddenFromSettingsEditor(fHasBhvr);
}

// Handles: @setcam_avdistmin:<distance>=n|y changes
template<>
void RlvBehaviourModifierHandler<RLV_MODIFIER_SETCAM_AVDISTMIN>::onValueChange() const
{
	if ( (gAgentCamera.cameraMouselook()) && (!RlvActions::canChangeToMouselook()) )
		gAgentCamera.changeCameraToThirdPerson();
}

// Handles: @setcam_eyeoffset:<vector3>=n|y, @setcam_eyeoffsetscale:<float>=n|y and @setcam_focusoffset:<vector3>=n|y toggles
template<> template<>
void RlvBehaviourCamEyeFocusOffsetHandler::onCommandToggle(ERlvBehaviour eBhvr, bool fHasBhvr)
{
	if (fHasBhvr)
	{
		gRlvHandler.setCameraOverride(true);
	}
	else
	{
		const RlvBehaviourModifier* pBhvrEyeOffsetModifier = RlvBehaviourDictionary::instance().getModifier(RLV_MODIFIER_SETCAM_EYEOFFSET);
		const RlvBehaviourModifier* pBhvrEyeOffsetScaleModifier = RlvBehaviourDictionary::instance().getModifier(RLV_MODIFIER_SETCAM_EYEOFFSETSCALE);
		const RlvBehaviourModifier* pBhvrFocusOffsetModifier = RlvBehaviourDictionary::instance().getModifier(RLV_MODIFIER_SETCAM_FOCUSOFFSET);
		if ( (!pBhvrEyeOffsetModifier->hasValue()) && (!pBhvrEyeOffsetScaleModifier->hasValue()) && (!pBhvrFocusOffsetModifier->hasValue()) )
		{
			gRlvHandler.setCameraOverride(false);
		}
	}
}

// Handles: @setcam_eyeoffset:<vector3>=n|y changes
template<>
void RlvBehaviourModifierHandler<RLV_MODIFIER_SETCAM_EYEOFFSET>::onValueChange() const
{
	if (RlvBehaviourModifier* pBhvrModifier = RlvBehaviourDictionary::instance().getModifier(RLV_MODIFIER_SETCAM_EYEOFFSET))
	{
		LLControlVariable* pControl = gSavedSettings.getControl("CameraOffsetRLVaView");
		if (pBhvrModifier->hasValue())
			pControl->setValue(pBhvrModifier->getValue<LLVector3>().getValue());
		else
			pControl->resetToDefault();
	}
}

// Handles: @setcam_eyeoffsetscale:<float>=n|y changes
template<>
void RlvBehaviourModifierHandler<RLV_MODIFIER_SETCAM_EYEOFFSETSCALE>::onValueChange() const
{
	if (RlvBehaviourModifier* pBhvrModifier = RlvBehaviourDictionary::instance().getModifier(RLV_MODIFIER_SETCAM_EYEOFFSETSCALE))
	{
		LLControlVariable* pControl = gSavedSettings.getControl("CameraOffsetScaleRLVa");
		if (pBhvrModifier->hasValue())
			pControl->setValue(pBhvrModifier->getValue<float>());
		else
			pControl->resetToDefault();
	}
}

// Handles: @setcam_focusoffset:<vector3d>=n|y changes
template<>
void RlvBehaviourModifierHandler<RLV_MODIFIER_SETCAM_FOCUSOFFSET>::onValueChange() const
{
	if (RlvBehaviourModifier* pBhvrModifier = RlvBehaviourDictionary::instance().getModifier(RLV_MODIFIER_SETCAM_FOCUSOFFSET))
	{
		LLControlVariable* pControl = gSavedSettings.getControl("FocusOffsetRLVaView");
		if (pBhvrModifier->hasValue())
			pControl->setValue(pBhvrModifier->getValue<LLVector3d>().getValue());
		else
			pControl->resetToDefault();
	}
}

// Handles: @setcam_fovmin:<angle>=n|y and @setcam_fovmax:<angle>=n|y
template<> template<>
ERlvCmdRet RlvBehaviourSetCamFovHandler::onCommand(const RlvCommand& rlvCmd, bool& fRefCount)
{
	static float s_nLastCameraAngle = DEFAULT_FIELD_OF_VIEW;

	S32 nRefMinBhvr = gRlvHandler.m_Behaviours[RLV_BHVR_SETCAM_FOVMIN], nRefMaxBhvr = gRlvHandler.m_Behaviours[RLV_BHVR_SETCAM_FOVMAX];
	LLControlVariable* pSetting = gSavedSettings.getControl("CameraAngle");

	// Save the user's current FOV angle if nothing's been restricted (yet)
	if ( (!nRefMinBhvr) && (!nRefMaxBhvr) && (pSetting) )
		s_nLastCameraAngle = (pSetting->isPersisted()) ? LLViewerCamera::instance().getDefaultFOV() : DEFAULT_FIELD_OF_VIEW;

	// Perform default handling of the command
	ERlvCmdRet eRet = RlvBehaviourGenericHandler<RLV_OPTION_MODIFIER>::onCommand(rlvCmd, fRefCount);
	if ( (RLV_RET_SUCCESS == eRet) && (fRefCount) && (pSetting) )
	{
		if (RLV_TYPE_ADD == rlvCmd.getParamType())
		{
			// Don't persist changes from this point
			pSetting->setPersist(LLControlVariable::PERSIST_NO);
		}
		else if ( (RLV_TYPE_REMOVE == rlvCmd.getParamType()) && (1 == nRefMinBhvr + nRefMaxBhvr) )
		{
			// Restore the user's last FOV angle (and resume persistance)
			LLViewerCamera::instance().setDefaultFOV(s_nLastCameraAngle);
			pSetting->setPersist(LLControlVariable::PERSIST_NONDFT);
		}
	}
	return eRet;
}

// Handles: @setcam_fovmin:<angle>=n|y changes
template<>
void RlvBehaviourModifierHandler<RLV_MODIFIER_SETCAM_FOVMIN>::onValueChange() const
{
	LLViewerCamera::instance().setDefaultFOV(LLViewerCamera::instance().getDefaultFOV());
}

// Handles: @setcam_fovmax:<angle>=n|y changes
template<>
void RlvBehaviourModifierHandler<RLV_MODIFIER_SETCAM_FOVMAX>::onValueChange() const
{
	LLViewerCamera::instance().setDefaultFOV(LLViewerCamera::instance().getDefaultFOV());
}

// Handles: @setcam_mouselook=n|y toggles
template<> template<>
void RlvBehaviourToggleHandler<RLV_BHVR_SETCAM_MOUSELOOK>::onCommandToggle(ERlvBehaviour eBhvr, bool fHasBhvr)
{
	if ((fHasBhvr) && (gAgentCamera.cameraMouselook()))
		gAgentCamera.changeCameraToThirdPerson();
}

// Handles: @setcam_textures[:<uuid>=n|y changes
template<>
void RlvBehaviourModifierHandler<RLV_MODIFIER_SETCAM_TEXTURE>::onValueChange() const
{
	if (RlvBehaviourModifier* pBhvrModifier = RlvBehaviourDictionary::instance().getModifier(RLV_MODIFIER_SETCAM_TEXTURE))
	{
		if (pBhvrModifier->hasValue())
		{
			RLV_INFOS << "Toggling diffuse textures for @setcam_textures" << RLV_ENDL;
			LLViewerFetchedTexture::sDefaultDiffuseImagep = LLViewerTextureManager::getFetchedTexture(pBhvrModifier->getValue<LLUUID>(), FTT_DEFAULT, MIPMAP_YES, LLGLTexture::BOOST_NONE, LLViewerTexture::LOD_TEXTURE);
			gObjectList.setAllObjectDefaultTextures(LLRender::DIFFUSE_MAP, true);
		}
		else
		{
			RLV_INFOS << "Restoring diffuse textures for @setcam_textures" << RLV_ENDL;
			gObjectList.setAllObjectDefaultTextures(LLRender::DIFFUSE_MAP, false);
			LLViewerFetchedTexture::sDefaultDiffuseImagep = nullptr;
		}
	}
}

// Handles: @setcam_unlock=n|y toggles
template<> template<>
void RlvBehaviourToggleHandler<RLV_BHVR_SETCAM_UNLOCK>::onCommandToggle(ERlvBehaviour eBhvr, bool fHasBhvr)
{
	if (fHasBhvr)
		handle_reset_view();
}

// Handles: @setcam=n|y toggles
template<> template<>
void RlvBehaviourToggleHandler<RLV_BHVR_SETCAM>::onCommandToggle(ERlvBehaviour eBhvr, bool fHasBhvr)
{
	// Once an object has exclusive control over the camera only its behaviours should be active. This affects:
	//   - behaviour modifiers         => it's all handled for us once we set the primary object
	//   - RLV_BHVR_SETCAM_UNLOCK      => manually (re)set the reference count (and possibly invoke the toggle handler)

	LLUUID idRlvObject; bool fHasCamUnlock = gRlvHandler.hasBehaviour(RLV_BHVR_SETCAM_UNLOCK);
	if (fHasBhvr)
	{
		// Get the UUID of the primary object
		std::list<const RlvObject*> lObjects;
		gRlvHandler.findBehaviour(RLV_BHVR_SETCAM, lObjects);
		idRlvObject = lObjects.front()->getObjectID();
		// Reset the @setcam_unlock reference count
		gRlvHandler.m_Behaviours[RLV_BHVR_SETCAM_UNLOCK] = (lObjects.front()->hasBehaviour(RLV_BHVR_SETCAM_UNLOCK, false)) ? 1 : 0;
	}
	else
	{
		std::list<const RlvObject*> lObjects;
		// Restore the @setcam_unlock reference count
		gRlvHandler.findBehaviour(RLV_BHVR_SETCAM_UNLOCK, lObjects);
		llassert_always(lObjects.size() <= 0x7FFF);
		gRlvHandler.m_Behaviours[RLV_BHVR_SETCAM_UNLOCK] = static_cast<S16>(lObjects.size());
	}

	// Manually invoke the @setcam_unlock toggle handler if we toggled it on/off
	if (fHasCamUnlock != gRlvHandler.hasBehaviour(RLV_BHVR_SETCAM_UNLOCK))
		RlvBehaviourToggleHandler<RLV_BHVR_SETCAM_UNLOCK>::onCommandToggle(RLV_BHVR_SETCAM_UNLOCK, !fHasCamUnlock);

	gRlvHandler.setCameraOverride(fHasBhvr);
	RlvBehaviourDictionary::instance().getModifier(RLV_MODIFIER_SETCAM_AVDIST)->setPrimaryObject(idRlvObject);
	RlvBehaviourDictionary::instance().getModifier(RLV_MODIFIER_SETCAM_AVDISTMIN)->setPrimaryObject(idRlvObject);
	RlvBehaviourDictionary::instance().getModifier(RLV_MODIFIER_SETCAM_AVDISTMAX)->setPrimaryObject(idRlvObject);
	RlvBehaviourDictionary::instance().getModifier(RLV_MODIFIER_SETCAM_ORIGINDISTMIN)->setPrimaryObject(idRlvObject);
	RlvBehaviourDictionary::instance().getModifier(RLV_MODIFIER_SETCAM_ORIGINDISTMAX)->setPrimaryObject(idRlvObject);
	RlvBehaviourDictionary::instance().getModifier(RLV_MODIFIER_SETCAM_EYEOFFSET)->setPrimaryObject(idRlvObject);
	RlvBehaviourDictionary::instance().getModifier(RLV_MODIFIER_SETCAM_EYEOFFSETSCALE)->setPrimaryObject(idRlvObject);
	RlvBehaviourDictionary::instance().getModifier(RLV_MODIFIER_SETCAM_FOCUSOFFSET)->setPrimaryObject(idRlvObject);
	RlvBehaviourDictionary::instance().getModifier(RLV_MODIFIER_SETCAM_FOVMIN)->setPrimaryObject(idRlvObject);
	RlvBehaviourDictionary::instance().getModifier(RLV_MODIFIER_SETCAM_FOVMAX)->setPrimaryObject(idRlvObject);
	RlvBehaviourDictionary::instance().getModifier(RLV_MODIFIER_SETCAM_TEXTURE)->setPrimaryObject(idRlvObject);
}

// Handles: @setdebug=n|y toggles
template<> template<>
void RlvBehaviourToggleHandler<RLV_BHVR_SETDEBUG>::onCommandToggle(ERlvBehaviour eBhvr, bool fHasBhvr)
{
	for (const auto& dbgSetting : RlvExtGetSet::m_DbgAllowed)
	{
		if (dbgSetting.second & RlvExtGetSet::DBG_WRITE)
			gSavedSettings.getControl(dbgSetting.first)->setHiddenFromSettingsEditor(fHasBhvr);
	}
}

// Handles: @setenv=n|y toggles
template<> template<>
void RlvBehaviourToggleHandler<RLV_BHVR_SETENV>::onCommandToggle(ERlvBehaviour eBhvr, bool fHasBhvr)
{
	const std::string strEnvFloaters[] = { "env_adjust_snapshot", "env_edit_extdaycycle", "env_fixed_environmentent_sky", "env_fixed_environmentent_water", "my_environments" };
	for (int idxFloater = 0, cntFloater = sizeof(strEnvFloaters) / sizeof(std::string); idxFloater < cntFloater; idxFloater++)
	{
		if (fHasBhvr)
		{
			// Hide the floater if it's currently visible
			LLFloaterReg::const_instance_list_t envFloaters = LLFloaterReg::getFloaterList(strEnvFloaters[idxFloater]);
			for (LLFloater* pFloater : envFloaters)
				pFloater->closeFloater();
			RLV_VERIFY(RlvUIEnabler::instance().addGenericFloaterFilter(strEnvFloaters[idxFloater]));
		}
		else
		{
			RLV_VERIFY(RlvUIEnabler::instance().removeGenericFloaterFilter(strEnvFloaters[idxFloater]));
		}
	}

	if (fHasBhvr)
	{
		Rlv::forceAtmosphericShadersIfAvailable();

		// Usurp the 'edit' environment for RLVa locking so TPV tools like quick prefs and phototools are automatically locked out as well
		// (these needed per-feature awareness of RLV in the previous implementation which often wasn't implemented)
		LLEnvironment* pEnv = LLEnvironment::getInstance();
		LLSettingsSky::ptr_t pRlvSky = pEnv->getEnvironmentFixedSky(LLEnvironment::ENV_LOCAL, true)->buildClone();
		pEnv->setEnvironment(LLEnvironment::ENV_EDIT, pRlvSky);
		pEnv->setSelectedEnvironment(LLEnvironment::ENV_EDIT, LLEnvironment::TRANSITION_INSTANT);
		pEnv->updateEnvironment(LLEnvironment::TRANSITION_INSTANT);
	}
	else
	{
		// Restore the user's WindLight preferences when releasing
		LLEnvironment::instance().clearEnvironment(LLEnvironment::ENV_EDIT);
		LLEnvironment::instance().setSelectedEnvironment(LLEnvironment::ENV_LOCAL);
		LLEnvironment::instance().updateEnvironment();
	}
}

// Handles: @showhovertext:<uuid>=n|y
template<> template<>
ERlvCmdRet RlvBehaviourHandler<RLV_BHVR_SHOWHOVERTEXT>::onCommand(const RlvCommand& rlvCmd, bool& fRefCount)
{
	// There should be an option and it should specify a valid UUID
	LLUUID idException;
	if (!RlvCommandOptionHelper::parseOption(rlvCmd.getOption(), idException))
		return RLV_RET_FAILED_OPTION;

	if (RLV_TYPE_ADD == rlvCmd.getParamType())
		gRlvHandler.addException(rlvCmd.getObjectID(), rlvCmd.getBehaviourType(), idException);
	else
		gRlvHandler.removeException(rlvCmd.getObjectID(), rlvCmd.getBehaviourType(), idException);

	// Clear/restore the object's hover text as needed
	LLViewerObject* pObj = gObjectList.findObject(idException);
	if ( (pObj) && (pObj->mText.notNull()) && (!pObj->mText->getObjectText().empty()) )
		pObj->mText->setString( (RLV_TYPE_ADD == rlvCmd.getParamType()) ? "" : pObj->mText->getObjectText());

	fRefCount = true;
	return RLV_RET_SUCCESS;
}

// Handles: @showinv=n|y toggles
template<> template<>
void RlvBehaviourToggleHandler<RLV_BHVR_SHOWINV>::onCommandToggle(ERlvBehaviour eBhvr, bool fHasBhvr)
{
	if (LLApp::isExiting())
		return;	// Nothing to do if the viewer is shutting down

	//
	// When disabling, close any inventory floaters that may be open
	//
	if (fHasBhvr)
	{
		LLFloaterReg::const_instance_list_t invFloaters = LLFloaterReg::getFloaterList("inventory");
		for (LLFloater* pFloater : invFloaters)
			pFloater->closeFloater();

		LLFloaterReg::const_instance_list_t lSecFloaters = LLFloaterReg::getFloaterList("secondary_inventory");
		for (LLFloaterReg::const_instance_list_t::const_iterator itSecFloater = lSecFloaters.begin(); itSecFloater != lSecFloaters.end(); ++itSecFloater)
			(*itSecFloater)->closeFloater();
	}

	//
	// Enable/disable the "My Outfits" panel on the "My Appearance" sidebar tab
	//
	LLPanelOutfitsInventory* pAppearancePanel = LLPanelOutfitsInventory::findInstance();
	RLV_ASSERT(pAppearancePanel);
	if (pAppearancePanel)
	{
		LLTabContainer* pAppearanceTabs = pAppearancePanel->getAppearanceTabs();
		LLOutfitsList* pMyOutfitsPanel = pAppearancePanel->getMyOutfitsPanel();
		if ( (pAppearanceTabs) && (pMyOutfitsPanel) )
		{
			S32 idxTab = pAppearanceTabs->getIndexForPanel(pMyOutfitsPanel);
			RLV_ASSERT(-1 != idxTab);
			pAppearanceTabs->enableTabButton(idxTab, !fHasBhvr);

			// When disabling, switch to the COF tab if "My Outfits" is currently active
			if ( (fHasBhvr) && (pAppearanceTabs->getCurrentPanelIndex() == idxTab) )
				pAppearanceTabs->selectTabPanel(pAppearancePanel->getCurrentOutfitPanel());
		}

		LLSidepanelAppearance* pCOFPanel = pAppearancePanel->getAppearanceSP();
		RLV_ASSERT(pCOFPanel);
		if ( (fHasBhvr) && (pCOFPanel) && (pCOFPanel->isOutfitEditPanelVisible()) )
		{
			// TODO-RLVa: we should really just be collapsing the "Add more..." inventory panel (and disable the button)
			pCOFPanel->showOutfitsInventoryPanel();
		}
	}

	//
	// Filter (or stop filtering) opening new inventory floaters
	//
	// <FS:Ansariel> Modified for FIRE-8804
	if (fHasBhvr)
	{
		RLV_VERIFY(RlvUIEnabler::instance().addGenericFloaterFilter("inventory"));
		RLV_VERIFY(RlvUIEnabler::instance().addGenericFloaterFilter("secondary_inventory"));
	}
	else
	{
		RLV_VERIFY(RlvUIEnabler::instance().removeGenericFloaterFilter("inventory"));
		RLV_VERIFY(RlvUIEnabler::instance().removeGenericFloaterFilter("secondary_inventory"));
	}
}

// Handles: @shownames[:<uuid>]=n|y toggles
template<> template<>
void RlvBehaviourToggleHandler<RLV_BHVR_SHOWNAMES>::onCommandToggle(ERlvBehaviour eBhvr, bool fHasBhvr)
{
	if (LLApp::isExiting() || gDoDisconnect)
		return;	// Nothing to do if the viewer is shutting down

	// Update the shownames context
	RlvActions::setShowName(RlvActions::SNC_DEFAULT, !fHasBhvr);

	// Refresh the nearby people list
	// <FS:Ansariel> [Standalone radar]
	//LLPanelPeople* pPeoplePanel = LLFloaterSidePanelContainer::getPanel<LLPanelPeople>("people", "panel_people");
	//RLV_ASSERT( (pPeoplePanel) && (pPeoplePanel->getNearbyList()) );
	//if ( (pPeoplePanel) && (pPeoplePanel->getNearbyList()) )
	//	pPeoplePanel->getNearbyList()->updateAvatarNames();
	if (FSRadar::instanceExists())
	{
		FSRadar::instance().updateNames();
	}
	// </FS:Ansariel> [Standalone radar]

	// Refresh the speaker list
	// <FS:Ansariel> [FS communication UI]
	FSFloaterVoiceControls* pCallFloater = LLFloaterReg::findTypedInstance<FSFloaterVoiceControls>("fs_voice_controls");
	if (pCallFloater)
		pCallFloater->getAvatarCallerList()->updateAvatarNames();
	// </FS:Ansariel> [FS communication UI]

	// Force the use of the "display name" cache so we can filter both display and legacy names (or return back to the user's preference)
	if (fHasBhvr)
	{
		LLAvatarNameCache::instance().setForceDisplayNames(true);
	}
	else
	{
		LLAvatarNameCache::instance().setForceDisplayNames(false);
		LLAvatarNameCache::instance().setUseDisplayNames(gSavedSettings.getBOOL("UseDisplayNames"));
	}

	// Refresh all name tags and HUD text
	LLVOAvatar::invalidateNameTags();
	LLHUDText::refreshAllObjectText();
}

// Handles: @shownames[:<uuid>]=n|y
template<> template<>
ERlvCmdRet RlvBehaviourHandler<RLV_BHVR_SHOWNAMES>::onCommand(const RlvCommand& rlvCmd, bool& fRefCount)
{
	ERlvCmdRet eRet = RlvBehaviourGenericHandler<RLV_OPTION_NONE_OR_EXCEPTION>::onCommand(rlvCmd, fRefCount);
	if ( (RLV_RET_SUCCESS == eRet) && (rlvCmd.hasOption()) && (!LLApp::isExiting()) )
	{
		const LLUUID idAgent = RlvCommandOptionHelper::parseOption<LLUUID>(rlvCmd.getOption());

		// Refresh the nearby people list (if necessary)
		// <FS:Ansariel> [Standalone radar]
		//LLPanelPeople* pPeoplePanel = LLFloaterSidePanelContainer::getPanel<LLPanelPeople>("people", "panel_people");
		//RLV_ASSERT( (pPeoplePanel) && (pPeoplePanel->getNearbyList()) );
		//if ( (pPeoplePanel) && (pPeoplePanel->getNearbyList()) && (pPeoplePanel->getNearbyList()->contains(idAgent)) )
		//{
		//	if (pPeoplePanel->getNearbyList()->isInVisibleChain())
		//		pPeoplePanel->onCommit();
		//	pPeoplePanel->getNearbyList()->updateAvatarNames();
		//}
		FSRadar* pRadar = FSRadar::getInstance();
		RLV_ASSERT( (pRadar) );
		if ( (pRadar) )
			pRadar->updateNames();
		// </FS:Ansariel> [Standalone radar]

		// Refresh the speaker list
		// <FS:Ansariel> [FS communication UI]
		FSFloaterVoiceControls* pCallFloater = LLFloaterReg::findTypedInstance<FSFloaterVoiceControls>("fs_voice_controls");
		if (pCallFloater)
			pCallFloater->getAvatarCallerList()->updateAvatarNames();
		// </FS:Ansariel> [FS communication UI]

		// Refresh that avatar's name tag and all HUD text
		LLVOAvatar::invalidateNameTag(idAgent);
		LLHUDText::refreshAllObjectText();
	}
	return eRet;
}

// Handles: @shownametags[:<distance>] value changes
template<>
void RlvBehaviourModifierHandler<RLV_MODIFIER_SHOWNAMETAGSDIST>::onValueChange() const
{
	if (LLApp::isExiting())
		return;	// Nothing to do if the viewer is shutting down

	// Refresh all name tags
	LLVOAvatar::invalidateNameTags();
}

// Handles: @shownametags[:<distance|uuid>]=n|y
template<> template<>
ERlvCmdRet RlvBehaviourHandler<RLV_BHVR_SHOWNAMETAGS>::onCommand(const RlvCommand& rlvCmd, bool& fRefCount)
{
	LLUUID idOption;
	if ( (rlvCmd.hasOption()) && (RlvCommandOptionHelper::parseOption(rlvCmd.getOption(), idOption)) )
	{
		ERlvCmdRet eRet = RlvBehaviourGenericHandler<RLV_OPTION_EXCEPTION>::onCommand(rlvCmd, fRefCount);
		if (RLV_RET_SUCCESS == eRet)
			LLVOAvatar::invalidateNameTag(idOption);
		fRefCount = false;
		return eRet;
	}
	return RlvBehaviourGenericHandler<RLV_OPTION_NONE_OR_MODIFIER>::onCommand(rlvCmd, fRefCount);
}

// Handles: @shownearby=n|y toggles
template<> template<>
void RlvBehaviourToggleHandler<RLV_BHVR_SHOWNEARBY>::onCommandToggle(ERlvBehaviour eBhvr, bool fHasBhvr)
{
	if (LLApp::isExiting())
		return;	// Nothing to do if the viewer is shutting down

	// Refresh the nearby people list
	// <FS:Ansariel> [Standalone radar]
	// -> This is handled in FSRadar (filtering all items) and
	//    FSPanelRadar (setting the blocked message). Since the radar
	//    is updated once every second, we do not hassle to explicitly
	//    clear it here, since it will happening almost instantly anyway
	//LLPanelPeople* pPeoplePanel = LLFloaterSidePanelContainer::getPanel<LLPanelPeople>("people", "panel_people");
	//LLAvatarList* pNearbyList = (pPeoplePanel) ? pPeoplePanel->getNearbyList() : NULL;
	//RLV_ASSERT( (pPeoplePanel) && (pNearbyList) );
	//if (pNearbyList)
	//{
	//	static std::string s_strNoItemsMsg = pNearbyList->getNoItemsMsg();
	//	pNearbyList->setNoItemsMsg( (fHasBhvr) ? RlvStrings::getString("blocked_nearby") : s_strNoItemsMsg );
	//	pNearbyList->clear();

	//	if (pNearbyList->isInVisibleChain())
	//		pPeoplePanel->onCommit();
	//	if (!fHasBhvr)
	//		pPeoplePanel->updateNearbyList();
	//}
	// </FS:Ansariel> [Standalone radar]

	// <FS:Ansariel> [FS communication UI]
	FSFloaterVoiceControls* pCallFloater = LLFloaterReg::findTypedInstance<FSFloaterVoiceControls>("fs_voice_controls");
	if (pCallFloater)
	{
		pCallFloater->toggleRlvShowNearbyRestriction(fHasBhvr);
	}
	// </FS:Ansariel> [FS communication UI]

	// Refresh that avatar's name tag and all HUD text
	LLHUDText::refreshAllObjectText();
}

// Handles: @showself=n|y and @showselfhead=n|y toggles
template<> template<>
void RlvBehaviourShowSelfToggleHandler::onCommandToggle(ERlvBehaviour eBvhr, bool fHasBhvr)
{
	if (isAgentAvatarValid())
		gAgentAvatarp->updateAttachmentVisibility(gAgentCamera.getCameraMode());
}

// Handles: @viewtransparent toggles
template<> template<>
void RlvBehaviourToggleHandler<RLV_BHVR_VIEWTRANSPARENT>::onCommandToggle(ERlvBehaviour eBhvr, bool fHasBhvr)
{
	if (fHasBhvr)
	{
		LLDrawPoolAlpha::sShowDebugAlpha = false;
	}
}

// Handles: @viewwireframe toggles
template<> template<>
void RlvBehaviourToggleHandler<RLV_BHVR_VIEWWIREFRAME>::onCommandToggle(ERlvBehaviour eBhvr, bool fHasBhvr)
{
	if (fHasBhvr)
	{
		set_use_wireframe(false);
	}
}

// ============================================================================
// Command handlers (RLV_TYPE_FORCE)
//

ERlvCmdRet RlvCommandHandlerBaseImpl<RLV_TYPE_FORCE>::processCommand(const RlvCommand& rlvCmd, RlvForceHandlerFunc* pHandler)
{
	return (*pHandler)(rlvCmd);
}

// Handles: @bhvr:<modifier>=force
template<>
ERlvCmdRet RlvForceGenericHandler<RLV_OPTION_MODIFIER>::onCommand(const RlvCommand& rlvCmd)
{
	// The object should be holding at least one active behaviour
	if (!gRlvHandler.hasBehaviour(rlvCmd.getObjectID()))
		return RLV_RET_FAILED_NOBEHAVIOUR;

	// There should be an option and it should specify a valid modifier (RlvBehaviourModifier performs the appropriate type checks)
	RlvBehaviourModifier* pBhvrModifier = RlvBehaviourDictionary::instance().getModifierFromBehaviour(rlvCmd.getBehaviourType());
	RlvBehaviourModifierValue modValue;
	if ( (!rlvCmd.hasOption()) || (!pBhvrModifier) || (!pBhvrModifier->convertOptionValue(rlvCmd.getOption(), pBhvrModifier->getType(), modValue)) )
		return RLV_RET_FAILED_OPTION;

	pBhvrModifier->setValue(modValue, rlvCmd.getObjectID());
	return RLV_RET_SUCCESS;
}

// Checked: 2010-04-07 (RLVa-1.2.0d) | Modified: RLVa-1.1.0j
ERlvCmdRet RlvHandler::processForceCommand(const RlvCommand& rlvCmd) const
{
	RLV_ASSERT(RLV_TYPE_FORCE == rlvCmd.getParamType());

	// Try a command processor first
	ERlvCmdRet eRet = rlvCmd.processCommand();
	if (RLV_RET_NO_PROCESSOR != eRet)
	{
		return eRet;
	}

	// Process the command the legacy way
	eRet = RLV_RET_SUCCESS;
	switch (rlvCmd.getBehaviourType())
	{
		case RLV_BHVR_UNSIT:		// @unsit=force							- Checked: 2010-03-18 (RLVa-1.2.0c) | Modified: RLVa-0.2.0g
			{
				VERIFY_OPTION(rlvCmd.getOption().empty());
				if ( (isAgentAvatarValid()) && (gAgentAvatarp->isSitting()) && (!hasBehaviourExcept(RLV_BHVR_UNSIT, rlvCmd.getObjectID())) )
				{
					gAgent.setControlFlags(AGENT_CONTROL_STAND_UP);
					send_agent_update(TRUE, TRUE);	// See behaviour notes on why we have to force an agent update here

					gRlvHandler.m_idPendingSitActor.setNull();
					gRlvHandler.m_idPendingUnsitActor = gRlvHandler.getCurrentObject();
				}
			}
			break;
		case RLV_BHVR_ADJUSTHEIGHT:	// @adjustheight:<options>=force		- Checked: 2015-03-30 (RLVa-1.5.0)
			{
				RlvCommandOptionAdjustHeight rlvCmdOption(rlvCmd);
				VERIFY_OPTION(rlvCmdOption.isValid());
				if (isAgentAvatarValid())
				{
					F32 nValue = (rlvCmdOption.m_nPelvisToFoot - gAgentAvatarp->getPelvisToFoot()) * rlvCmdOption.m_nPelvisToFootDeltaMult;
					nValue += rlvCmdOption.m_nPelvisToFootOffset;
					if (gAgentAvatarp->getRegion()->avatarHoverHeightEnabled() || !gAgentAvatarp->isUsingServerBakes())
					{
						LLVector3 avOffset(0.0f, 0.0f, llclamp<F32>(nValue, MIN_HOVER_Z, MAX_HOVER_Z));
						gSavedPerAccountSettings.setF32("AvatarHoverOffsetZ", avOffset.mV[VZ]);
						gAgentAvatarp->setHoverOffset(avOffset, true);
					}
					else
					{
						eRet = RLV_RET_FAILED_DISABLED;
					}
				}
			}
			break;
		case RLV_CMD_FORCEWEAR:		// @forcewear[:<options>]=force			- Checked: 2011-09-12 (RLVa-1.5.0)
			{
				if (RlvBehaviourInfo::FORCEWEAR_CONTEXT_NONE & rlvCmd.getBehaviourFlags())
				{
					RlvCommandOptionGeneric rlvCmdOption;
					RlvCommandOptionHelper::parseOption(rlvCmd.getOption(), rlvCmdOption);
					VERIFY_OPTION(rlvCmdOption.isSharedFolder());
					eRet = onForceWear(rlvCmdOption.getSharedFolder(), rlvCmd.getBehaviourFlags());
				}
				else if (RlvBehaviourInfo::FORCEWEAR_CONTEXT_OBJECT & rlvCmd.getBehaviourFlags())
				{
					RlvCommandOptionGetPath rlvGetPathOption(rlvCmd, boost::bind(&RlvHandler::onForceWearCallback, this, _1, rlvCmd.getBehaviourFlags()));
					VERIFY_OPTION(rlvGetPathOption.isValid());
					eRet = (!rlvGetPathOption.isCallback()) ? RLV_RET_SUCCESS : RLV_RET_SUCCESS_DELAYED;
				}
			}
			break;
		case RLV_BHVR_UNKNOWN:
			// Pass unknown commands on to registered command handlers
			return (notifyCommandHandlers(&RlvExtCommandHandler::onForceCommand, rlvCmd, eRet, false)) ? eRet : RLV_RET_FAILED_UNKNOWN;
		default:
			// Fail with "Invalid param" if none of the above handled it
			eRet = RLV_RET_FAILED_PARAM;
			break;
	}
	return eRet;
}

// Handles: @detachme=force
template<> template<>
ERlvCmdRet RlvForceHandler<RLV_BHVR_DETACHME>::onCommand(const RlvCommand& rlvCmd)
{
	if (rlvCmd.hasOption())
		return RLV_RET_FAILED_OPTION;

	// NOTE: @detachme should respect locks but shouldn't respect things like nostrip
	const LLViewerObject* pAttachObj = gObjectList.findObject(rlvCmd.getObjectID());
	if ( (pAttachObj) && (pAttachObj->isAttachment()) )
		LLVOAvatarSelf::detachAttachmentIntoInventory(pAttachObj->getAttachmentItemID());

	return RLV_RET_SUCCESS;
}

// Handles: @fly:[true|false]=force
template<> template<>
ERlvCmdRet RlvForceHandler<RLV_BHVR_FLY>::onCommand(const RlvCommand& rlvCmd)
{
	bool fForceFly = true;
	if ( (rlvCmd.hasOption()) && (!RlvCommandOptionHelper::parseOption<bool>(rlvCmd.getOption(), fForceFly)) )
		return RLV_RET_FAILED_OPTION;

	if ( (fForceFly) && (!RlvActions::canFly(rlvCmd.getObjectID())) )
		return RLV_RET_FAILED_LOCK;

	if (fForceFly != (bool)gAgent.getFlying())
		gAgent.setFlying(fForceFly);

	return RLV_RET_SUCCESS;
}

// Handles: @remattach[:<folder|attachpt|attachgroup>]=force
template<> template<>
ERlvCmdRet RlvForceRemAttachHandler::onCommand(const RlvCommand& rlvCmd)
{
	if (!isAgentAvatarValid())
		return RLV_RET_FAILED;

	RlvCommandOptionGeneric rlvCmdOption;
	RlvCommandOptionHelper::parseOption(rlvCmd.getOption(), rlvCmdOption);
	if (rlvCmdOption.isSharedFolder())
		return gRlvHandler.onForceWear(rlvCmdOption.getSharedFolder(), rlvCmd.getBehaviourFlags());

	// @remattach:<attachpt>=force - force detach single attachment point
	if (rlvCmdOption.isAttachmentPoint())
	{
		RlvForceWear::instance().forceDetach(rlvCmdOption.getAttachmentPoint());
		return RLV_RET_SUCCESS;
	}
	// @remattach:<group>=force - force detach attachments points belonging to <group>
	// @remattach=force         - force detach all attachments points
	else if ( (rlvCmdOption.isAttachmentPointGroup()) || (rlvCmdOption.isEmpty()) )
	{
		for (const auto& entryAttachPt : gAgentAvatarp->mAttachmentPoints)
		{
			const LLViewerJointAttachment* pAttachPt = entryAttachPt.second;
			if ( (pAttachPt) && (pAttachPt->getNumObjects()) && ((rlvCmdOption.isEmpty()) || (rlvAttachGroupFromIndex(pAttachPt->getGroup()) == rlvCmdOption.getAttachmentPointGroup())) )
			{
				RlvForceWear::instance().forceDetach(pAttachPt);
			}
		}
		return RLV_RET_SUCCESS;
	}
	// @remattach:<uuid>=force - force detach a specific attachment
	else if (rlvCmdOption.isUUID())
	{
		const LLViewerObject* pAttachObj = gObjectList.findObject(rlvCmdOption.getUUID());
		if ( (pAttachObj) && (pAttachObj->isAttachment()) && (pAttachObj->permYouOwner()) )
			RlvForceWear::instance().forceDetach(pAttachObj);
		return RLV_RET_SUCCESS;
	}
	return RLV_RET_FAILED_OPTION;
}

// Handles: @remoutfit[:<folder|layer>]=force
template<> template<>
ERlvCmdRet RlvForceHandler<RLV_BHVR_REMOUTFIT>::onCommand(const RlvCommand& rlvCmd)
{
	RlvCommandOptionGeneric rlvCmdOption;
	RlvCommandOptionHelper::parseOption(rlvCmd.getOption(), rlvCmdOption);
	if (rlvCmdOption.isSharedFolder())
		return gRlvHandler.onForceWear(rlvCmdOption.getSharedFolder(), rlvCmd.getBehaviourFlags());

	if ( (!rlvCmdOption.isWearableType()) && (!rlvCmdOption.isEmpty()) )
		return RLV_RET_FAILED_OPTION;

	for (int idxType = 0; idxType < LLWearableType::WT_COUNT; idxType++)
	{
		if ( (rlvCmdOption.isEmpty()) || ((LLWearableType::EType)idxType == rlvCmdOption.getWearableType()))
			RlvForceWear::instance().forceRemove((LLWearableType::EType)idxType);
	}
	return RLV_RET_SUCCESS;
}

// Handles: @setcam_eyeoffset[:<vector3>]=force, @setcam_eyeoffsetscale[:<float>]=force and @setcam_focusoffset[:<vector3>]=force
template<> template<>
ERlvCmdRet RlvForceCamEyeFocusOffsetHandler::onCommand(const RlvCommand& rlvCmd)
{
	// Enforce exclusive camera locks
	if (!RlvActions::canChangeCameraPreset(rlvCmd.getObjectID()))
		return RLV_RET_FAILED_LOCK;

	LLControlVariable* pEyeOffsetControl = gSavedSettings.getControl("CameraOffsetRLVaView");
	LLControlVariable* pEyeOffsetScaleControl = gSavedSettings.getControl("CameraOffsetScaleRLVa");
	LLControlVariable* pFocusOffsetControl = gSavedSettings.getControl("FocusOffsetRLVaView");

	LLControlVariable* pControl; LLSD sdControlValue;
	switch (rlvCmd.getBehaviourType())
	{
		case RLV_BHVR_SETCAM_EYEOFFSET:
			if (rlvCmd.hasOption())
			{
				LLVector3 vecOffset;
				if (!RlvCommandOptionHelper::parseOption(rlvCmd.getOption(), vecOffset))
					return RLV_RET_FAILED_OPTION;
				sdControlValue = vecOffset.getValue();
			}
			pControl = pEyeOffsetControl;
			break;
		case RLV_BHVR_SETCAM_EYEOFFSETSCALE:
			if (rlvCmd.hasOption())
			{
				float nScale;
				if (!RlvCommandOptionHelper::parseOption(rlvCmd.getOption(), nScale))
					return RLV_RET_FAILED_OPTION;
				sdControlValue = nScale;
			}
			pControl = pEyeOffsetScaleControl;
			break;
		case RLV_BHVR_SETCAM_FOCUSOFFSET:
			if (rlvCmd.hasOption())
			{
				LLVector3d vecOffset;
				if (!RlvCommandOptionHelper::parseOption(rlvCmd.getOption(), vecOffset))
					return RLV_RET_FAILED_OPTION;
				sdControlValue = vecOffset.getValue();
			}
			pControl = pFocusOffsetControl;
			break;
		default:
			return RLV_RET_FAILED;
	}

	if (!sdControlValue.isUndefined())
		pControl->setValue(sdControlValue);
	else
		pControl->resetToDefault();

	// NOTE: this doesn't necessarily release the camera preset even if all 3 are at their default now (e.g. @setcam is currently set)
	gRlvHandler.setCameraOverride( (!pEyeOffsetControl->isDefault()) || (!pEyeOffsetScaleControl->isDefault()) || (!pFocusOffsetControl->isDefault()) );
	return RLV_RET_SUCCESS;
}

// Handles: @setcam_focus:<uuid>[;<dist>[;<direction>]]=force
template<> template<>
ERlvCmdRet RlvForceHandler<RLV_BHVR_SETCAM_FOCUS>::onCommand(const RlvCommand& rlvCmd)
{
	std::vector<std::string> optionList;
	if (!RlvCommandOptionHelper::parseStringList(rlvCmd.getOption(), optionList))
		return RLV_RET_FAILED_OPTION;

	LLVector3 posAgent;
	LLVector3d posGlobal;
	F32 camDistance;

	// Get the focus position/object (and verify it is known)
	LLUUID idObject; LLVector3 posRegion;
	if (RlvCommandOptionHelper::parseOption(optionList[0], idObject))
	{
		const LLViewerObject* pObj = gObjectList.findObject(idObject);
		if (!pObj)
			return RLV_RET_FAILED_OPTION;
		if (!pObj->isAvatar())
		{
			posAgent = pObj->getPositionAgent();
			posGlobal = pObj->getPositionGlobal();
		}
		else
		{
			/*const*/ LLVOAvatar* pAvatar = (/*const*/ LLVOAvatar*)pObj;
			if (pAvatar->mHeadp)
			{
				posAgent = pAvatar->mHeadp->getWorldPosition();
				posGlobal = pAvatar->getPosGlobalFromAgent(posAgent);
			}
		}
		camDistance = pObj->getScale().magVec();
	}
	else if (RlvCommandOptionHelper::parseOption(optionList[0], posRegion))
	{
		const LLViewerRegion* pRegion = gAgent.getRegion();
		if (!pRegion)
			return RLV_RET_FAILED_UNKNOWN;
		posAgent = pRegion->getPosAgentFromRegion(posRegion);
		posGlobal = pRegion->getPosGlobalFromRegion(posRegion);
		camDistance = 0.0f;
	}
	else
	{
		return RLV_RET_FAILED_OPTION;
	}

	// Get the camera distance
	if ( (optionList.size() > 1) && (!optionList[1].empty()) )
	{
		if (!RlvCommandOptionHelper::parseOption(optionList[1], camDistance))
			return RLV_RET_FAILED_OPTION;
	}

	// Get the directional vector (or calculate it from the current camera position)
	LLVector3 camDirection;
	if ( (optionList.size() > 2) && (!optionList[2].empty()) )
	{
		if (!RlvCommandOptionHelper::parseOption(optionList[2], camDirection))
			return RLV_RET_FAILED_OPTION;
	}
	else
	{
		camDirection = LLViewerCamera::getInstance()->getOrigin() - posAgent;
	}
	camDirection.normVec();

	// Move the camera in place
	gAgentCamera.setFocusOnAvatar(FALSE, ANIMATE);
	gAgentCamera.setCameraPosAndFocusGlobal(posGlobal + LLVector3d(camDirection * llmax(F_APPROXIMATELY_ZERO, camDistance)), posGlobal, idObject);

	return RLV_RET_SUCCESS;
}

// Handles: @setcam_fov[:<angle>]=force
template<> template<>
ERlvCmdRet RlvForceHandler<RLV_BHVR_SETCAM_FOV>::onCommand(const RlvCommand& rlvCmd)
{
	if (!RlvActions::canChangeCameraFOV(rlvCmd.getObjectID()))
		return RLV_RET_FAILED_LOCK;

	F32 nFOV = DEFAULT_FIELD_OF_VIEW;
	if ( (rlvCmd.hasOption()) && (!RlvCommandOptionHelper::parseOption(rlvCmd.getOption(), nFOV)) )
		return RLV_RET_FAILED_OPTION;

	LLViewerCamera::getInstance()->setDefaultFOV(nFOV);

	// Don't persist non-default changes that are due to RLV; but do resume persistance once reset back to the default
	if ( (!gRlvHandler.hasBehaviour(RLV_BHVR_SETCAM_FOVMIN)) && (!gRlvHandler.hasBehaviour(RLV_BHVR_SETCAM_FOVMAX)) )
	{
		if (LLControlVariable* pSetting = gSavedSettings.getControl("CameraAngle"))
			pSetting->setPersist( (pSetting->isDefault()) ? LLControlVariable::PERSIST_NONDFT : LLControlVariable::PERSIST_NO );
	}

	return RLV_RET_SUCCESS;
}

// Handles: @setcam_mode[:<option>]=force
template<> template<>
ERlvCmdRet RlvForceHandler<RLV_BHVR_SETCAM_MODE>::onCommand(const RlvCommand& rlvCmd)
{
	const std::string& strOption = rlvCmd.getOption();
	if ("mouselook" == strOption)
		gAgentCamera.changeCameraToMouselook();
	else if ("thirdperson" == strOption)
		gAgentCamera.changeCameraToThirdPerson();
	else if ( ("reset" == strOption) || (strOption.empty()) )
		handle_reset_view();
	else
		return RLV_RET_FAILED_OPTION;
	return RLV_RET_SUCCESS;
}

// Handles: @setoverlay_tween:[<alpha>];[<tint>];<duration>=force
template<> template<>
ERlvCmdRet RlvForceHandler<RLV_BHVR_SETOVERLAY_TWEEN>::onCommand(const RlvCommand& rlvCmd)
{
	RlvObject* pRlvObj = gRlvHandler.getObject(rlvCmd.getObjectID());
	if (!pRlvObj)
		return RLV_RET_FAILED_NOBEHAVIOUR;

	RlvOverlayEffect* pOverlayEffect = LLVfxManager::instance().getEffect<RlvOverlayEffect>(rlvCmd.getObjectID());
	if (!pOverlayEffect)
		return RLV_RET_FAILED_LOCK;

	std::vector<std::string> optionList;
	if ( (!RlvCommandOptionHelper::parseStringList(rlvCmd.getOption(), optionList)) || (3 != optionList.size()) )
		return RLV_RET_FAILED_OPTION;

	// Parse the duration first (required param)
	float tweenDuration = .0f;
	if (!RlvCommandOptionHelper::parseOption(optionList[2], tweenDuration))
		return RLV_RET_FAILED_OPTION;

	// Process the overlay alpha tween (if there is one and it is a valid value)
	float overlayAlpha = .0f;
	if (RlvCommandOptionHelper::parseOption(optionList[0], overlayAlpha))
	{
		pOverlayEffect->tweenAlpha(overlayAlpha, tweenDuration);
		pRlvObj->setModifierValue(ERlvLocalBhvrModifier::OverlayAlpha, overlayAlpha);
	}

	// Process the overlay tint tween (if there is one and it is a valid value)
	LLVector3 overlayColor;
	if (RlvCommandOptionHelper::parseOption(optionList[1], overlayColor))
	{
		pOverlayEffect->tweenColor(LLColor3(overlayColor.mV), tweenDuration);
		pRlvObj->setModifierValue(ERlvLocalBhvrModifier::OverlayTint, overlayColor);
	}

	return RLV_RET_SUCCESS;
}

// Checked: 2010-08-30 (RLVa-1.2.1c) | Modified: RLVa-1.2.1c
ERlvCmdRet RlvHandler::onForceWear(const LLViewerInventoryCategory* pFolder, U32 nFlags) const
{
	if ( (!pFolder) || (!RlvInventory::instance().isSharedFolder(pFolder->getUUID())) )
		return RLV_RET_FAILED_OPTION;

	RlvForceWear::EWearAction eAction = (RlvBehaviourInfo::FORCEWEAR_WEAR_REMOVE & nFlags) ? RlvForceWear::ACTION_REMOVE
																						   : ((RlvBehaviourInfo::FORCEWEAR_WEAR_ADD & nFlags) ? RlvForceWear::ACTION_WEAR_ADD
																																			  : RlvForceWear::ACTION_WEAR_REPLACE);
	RlvForceWear::EWearFlags eFlags = (RlvBehaviourInfo::FORCEWEAR_SUBTREE & nFlags) ? RlvForceWear::FLAG_MATCHALL : RlvForceWear::FLAG_DEFAULT;

	RlvForceWear::instance().forceFolder(pFolder, eAction, eFlags);
	return RLV_RET_SUCCESS;
}

void RlvHandler::onForceWearCallback(const uuid_vec_t& idItems, U32 nFlags) const
{
	LLInventoryModel::cat_array_t folders;
	if (RlvInventory::instance().getPath(idItems, folders))
	{
		for (S32 idxFolder = 0, cntFolder = folders.size(); idxFolder < cntFolder; idxFolder++)
			onForceWear(folders.at(idxFolder), nFlags);

		// If we're not executing a command then we're a delayed callback and need to manually call done()
		if ( (!getCurrentCommand()) && (RlvForceWear::instanceExists()) )
			RlvForceWear::instance().done();
	}
}

// Handles: @setgroup:<uuid|name>[;<role>]=force
template<> template<>
ERlvCmdRet RlvForceHandler<RLV_BHVR_SETGROUP>::onCommand(const RlvCommand& rlvCmd)
{
	if (!RlvActions::canChangeActiveGroup(rlvCmd.getObjectID()))
		return RLV_RET_FAILED_LOCK;

	std::vector<std::string> optionList;
	if ( (!RlvCommandOptionHelper::parseStringList(rlvCmd.getOption(), optionList)) || (optionList.size() < 1) || (optionList.size() > 2) )
		return RLV_RET_FAILED_OPTION;

	LLUUID idGroup; bool fValid = false;
	if ("none" == optionList[0])
	{
		idGroup.setNull();
		fValid = true;
	}
	else if (idGroup.set(optionList[0]))
	{
		fValid = (idGroup.isNull()) || (gAgent.isInGroup(idGroup, true));
	}
	else
	{
		bool fExactMatch = false;
		for (const auto& groupData : gAgent.mGroups)
		{
			// NOTE: exact matches take precedence over partial matches; in case of partial matches the last match wins
			if (boost::istarts_with(groupData.mName, optionList[0]))
			{
				idGroup = groupData.mID;
				fExactMatch = groupData.mName.length() == optionList[0].length();
				if (fExactMatch)
					break;
			}
		}
		fValid = idGroup.notNull();
	}

	if (fValid)
	{
		if (!gRlvHandler.checkActiveGroupThrottle(rlvCmd.getObjectID()))
			return RLV_RET_FAILED_THROTTLED;

		if (optionList.size() == 1)
			gRlvHandler.setActiveGroup(idGroup);
		else if (optionList.size() == 2)
			gRlvHandler.setActiveGroupRole(idGroup, optionList[1]);
	}

	return (fValid) ? RLV_RET_SUCCESS : RLV_RET_FAILED_OPTION;
}

// Handles: @sitground=force
template<> template<>
ERlvCmdRet RlvForceHandler<RLV_BHVR_SITGROUND>::onCommand(const RlvCommand& rlvCmd)
{
	if ( (!RlvActions::canGroundSit(rlvCmd.getObjectID())) || (!isAgentAvatarValid()) )
		return RLV_RET_FAILED_LOCK;

	if (!gAgentAvatarp->isSitting())
	{
		gAgent.setControlFlags(AGENT_CONTROL_SIT_ON_GROUND);

		gRlvHandler.m_fPendingGroundSit = false;
		gRlvHandler.m_idPendingSitActor = gRlvHandler.getCurrentObject();
		gRlvHandler.m_idPendingUnsitActor.setNull();
	}
	else if (gAgentAvatarp->getParent())
	{
		gAgent.setControlFlags(AGENT_CONTROL_STAND_UP);

		gRlvHandler.m_fPendingGroundSit = true;
		gRlvHandler.m_idPendingSitActor.setNull();
		gRlvHandler.m_idPendingUnsitActor = gRlvHandler.getCurrentObject();
	}
	send_agent_update(TRUE, TRUE);

	return RLV_RET_SUCCESS;
}

// Handles: @sit:<uuid>=force
template<> template<>
ERlvCmdRet RlvForceHandler<RLV_BHVR_SIT>::onCommand(const RlvCommand& rlvCmd)
{
	LLUUID idTarget;
	if (!RlvCommandOptionHelper::parseOption(rlvCmd.getOption(), idTarget))
		return RLV_RET_FAILED_OPTION;

	LLViewerObject* pObj = NULL;
	if (idTarget.isNull())
	{
		return RlvForceHandler<RLV_BHVR_SITGROUND>::onCommand(rlvCmd);
	}
	else if ( ((pObj = gObjectList.findObject(idTarget)) != NULL) && (LL_PCODE_VOLUME == pObj->getPCode()))
	{
		if (!RlvActions::canSit(pObj))
			return RLV_RET_FAILED_LOCK;

		if ((gRlvHandler.hasBehaviour(RLV_BHVR_STANDTP)) && (isAgentAvatarValid()))
		{
			if ( (isAgentAvatarValid()) && (gAgentAvatarp->isSitting()) )
				return RLV_RET_FAILED_LOCK;
			gRlvHandler.m_posSitSource = gAgent.getPositionGlobal();
		}

		// Copy/paste from handle_sit_or_stand()
		gMessageSystem->newMessageFast(_PREHASH_AgentRequestSit);
		gMessageSystem->nextBlockFast(_PREHASH_AgentData);
		gMessageSystem->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
		gMessageSystem->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
		gMessageSystem->nextBlockFast(_PREHASH_TargetObject);
		gMessageSystem->addUUIDFast(_PREHASH_TargetID, pObj->mID);
		gMessageSystem->addVector3Fast(_PREHASH_Offset, LLVector3::zero);
		pObj->getRegion()->sendReliableMessage();

		gRlvHandler.m_idPendingSitActor = gRlvHandler.getCurrentObject();
		gRlvHandler.m_idPendingUnsitActor.setNull();
	}
	else
	{
		return RLV_RET_FAILED_OPTION;
	}
	return RLV_RET_SUCCESS;
}

// Handles: @tpto:<vector>[;<angle>]=force and @tpto:<region>/<vector>[;<angle>]=force
template<> template<>
ERlvCmdRet RlvForceHandler<RLV_BHVR_TPTO>::onCommand(const RlvCommand& rlvCmd)
{
	std::vector<std::string> optionList;
	if (!RlvCommandOptionHelper::parseStringList(rlvCmd.getOption(), optionList))
		return RLV_RET_FAILED_OPTION;

	// We need the look-at first
	LLVector3 vecLookAt = LLVector3::zero;
	if (optionList.size() > 1)
	{
		float nAngle = 0.0f;
		if (!RlvCommandOptionHelper::parseOption(optionList[1], nAngle))
			return RLV_RET_FAILED_OPTION;

		vecLookAt = LLVector3::x_axis;
		vecLookAt.rotVec(nAngle, LLVector3::z_axis);
		vecLookAt.normalize();
	}

	// Next figure out the destination
	LLVector3d posGlobal;
	if (RlvCommandOptionHelper::parseOption(optionList[0], posGlobal))
	{
		if (optionList.size() == 1)
			gAgent.teleportViaLocation(posGlobal);
		else
		gAgent.teleportViaLocationLookAt(posGlobal, vecLookAt);
	}
	else
	{
		std::vector<std::string> posList; LLVector3 posRegion;
		if ( (!RlvCommandOptionHelper::parseStringList(optionList[0], posList, std::string("/"))) || (4 != posList.size()) ||
		     (!RlvCommandOptionHelper::parseOption(optionList[0].substr(posList[0].size() + 1), posRegion)) )
		{
			return RLV_RET_FAILED_OPTION;
		}

		LLWorldMapMessage::url_callback_t cb = boost::bind(&RlvHandler::onTeleportCallback, &gRlvHandler, _1, posRegion, vecLookAt, rlvCmd.getObjectID());
		LLWorldMapMessage::getInstance()->sendNamedRegionRequest(posList[0], cb, std::string(""), true);
	}

	return RLV_RET_SUCCESS;
}

// ============================================================================
// Command handlers (RLV_TYPE_REPLY)
//

ERlvCmdRet RlvCommandHandlerBaseImpl<RLV_TYPE_REPLY>::processCommand(const RlvCommand& rlvCmd, RlvReplyHandlerFunc* pHandler)
{
	// Sanity check - <param> should specify a - valid - reply channel
	S32 nChannel;
	if ( (!LLStringUtil::convertToS32(rlvCmd.getParam(), nChannel)) || (!RlvUtil::isValidReplyChannel(nChannel, rlvCmd.getObjectID() == gAgent.getID())) )
		return RLV_RET_FAILED_PARAM;

	std::string strReply;
	ERlvCmdRet eRet = (*pHandler)(rlvCmd, strReply);

	// If we made it this far then:
	//   - the command was handled successfully so we send off the response
	//   - the command failed but we still send off an - empty - response to keep the issuing script from blocking
	if (nChannel != 0)
		RlvUtil::sendChatReply(nChannel, strReply);
	else
	{
		if (RlvFloaterConsole* pConsole = LLFloaterReg::findTypedInstance<RlvFloaterConsole>("rlv_console"))
			pConsole->addCommandReply(rlvCmd.getBehaviour(), strReply);
	}

	return eRet;
}

// Checked: 2010-04-07 (RLVa-1.2.0d) | Modified: RLVa-1.1.0f
ERlvCmdRet RlvHandler::processReplyCommand(const RlvCommand& rlvCmd) const
{
	RLV_ASSERT(RLV_TYPE_REPLY == rlvCmd.getParamType());

	// Try a command processor first
	ERlvCmdRet eRet = rlvCmd.processCommand();
	if (RLV_RET_NO_PROCESSOR != eRet)
	{
		return eRet;
	}

	// Sanity check - <param> should specify a - valid - reply channel
	S32 nChannel;
	if ( (!LLStringUtil::convertToS32(rlvCmd.getParam(), nChannel)) || (!RlvUtil::isValidReplyChannel(nChannel, rlvCmd.getObjectID() == gAgent.getID())) )
		return RLV_RET_FAILED_PARAM;

	// Process the command the legacy way
	eRet = RLV_RET_SUCCESS; std::string strReply;
	switch (rlvCmd.getBehaviourType())
	{
		case RLV_BHVR_VERSION:			// @version=<channel>					- Checked: 2010-03-27 (RLVa-1.4.0a)
		case RLV_BHVR_VERSIONNEW:		// @versionnew=<channel>				- Checked: 2010-03-27 (RLVa-1.4.0a) | Added: RLVa-1.2.0b
			// NOTE: RLV will respond even if there's an option
			strReply = RlvStrings::getVersion(rlvCmd.getObjectID(), RLV_BHVR_VERSION == rlvCmd.getBehaviourType());
			break;
		case RLV_BHVR_VERSIONNUM:		// @versionnum=<channel>				- Checked: 2010-03-27 (RLVa-1.4.0a) | Added: RLVa-1.0.4b
			// NOTE: RLV will respond even if there's an option
			if (!rlvCmd.hasOption())
				strReply = RlvStrings::getVersionNum(rlvCmd.getObjectID());
			else if ("impl" == rlvCmd.getOption())
				strReply = RlvStrings::getVersionImplNum();
			break;
		case RLV_BHVR_GETATTACH:		// @getattach[:<layer>]=<channel>
			eRet = onGetAttach(rlvCmd, strReply);
			break;
		case RLV_BHVR_GETATTACHNAMES:	// @getattachnames[:<grp>]=<channel>
		case RLV_BHVR_GETADDATTACHNAMES:// @getaddattachnames[:<grp>]=<channel>
		case RLV_BHVR_GETREMATTACHNAMES:// @getremattachnames[:<grp>]=<channel>
			eRet = onGetAttachNames(rlvCmd, strReply);
			break;
		case RLV_BHVR_GETOUTFIT:		// @getoutfit[:<layer>]=<channel>
			eRet = onGetOutfit(rlvCmd, strReply);
			break;
		case RLV_BHVR_GETOUTFITNAMES:	// @getoutfitnames=<channel>
		case RLV_BHVR_GETADDOUTFITNAMES:// @getaddoutfitnames=<channel>
		case RLV_BHVR_GETREMOUTFITNAMES:// @getremoutfitnames=<channel>
			eRet = onGetOutfitNames(rlvCmd, strReply);
			break;
		case RLV_BHVR_FINDFOLDER:		// @findfolder:<criteria>=<channel>
		case RLV_BHVR_FINDFOLDERS:		// @findfolders:<criteria>=<channel>
			eRet = onFindFolder(rlvCmd, strReply);
			break;
		case RLV_BHVR_GETPATH:			// @getpath[:<option>]=<channel>
		case RLV_BHVR_GETPATHNEW:		// @getpathnew[:<option>]=<channel>
			eRet = onGetPath(rlvCmd, strReply);
			break;
		case RLV_BHVR_GETINV:			// @getinv[:<path>]=<channel>
			eRet = onGetInv(rlvCmd, strReply);
			break;
		case RLV_BHVR_GETINVWORN:		// @getinvworn[:<path>]=<channel>
			eRet = onGetInvWorn(rlvCmd, strReply);
			break;
		case RLV_BHVR_GETGROUP:			// @getgroup=<channel>					- Checked: 2011-03-28 (RLVa-1.4.1a) | Added: RLVa-1.3.0f
			strReply = (gAgent.getGroupID().notNull()) ? gAgent.getGroupName() : "none";
			break;
		case RLV_BHVR_GETSITID:			// @getsitid=<channel>					- Checked: 2010-03-09 (RLVa-1.2.0a) | Modified: RLVa-1.2.0a
			{
				// NOTE: RLV-1.16.1 returns a NULL UUID if we're not sitting
				LLUUID idSitObj;
				if ( (isAgentAvatarValid()) && (gAgentAvatarp->isSitting()) )
				{
					const LLViewerObject* pSeatObj = dynamic_cast<LLViewerObject*>(gAgentAvatarp->getRoot());
					if (pSeatObj)
						idSitObj = pSeatObj->getID();
				}
				strReply = idSitObj.asString();
			}
			break;
		case RLV_BHVR_GETSTATUS:		// @getstatus                           - Checked: 2010-04-07 (RLVa-1.2.0d) | Modified: RLVa-1.1.0f
			{
				std::string strFilter, strSeparator;
				if (rlvParseGetStatusOption(rlvCmd.getOption(), strFilter, strSeparator))
				{
					// NOTE: specification says response should start with '/' but RLV-1.16.1 returns an empty string when no rules are set
					rlv_object_map_t::const_iterator itObj = m_Objects.find(rlvCmd.getObjectID());
					if (itObj != m_Objects.end())
						strReply = itObj->second.getStatusString(strFilter, strSeparator);
				}
			}
			break;
		case RLV_BHVR_GETSTATUSALL:		// @getstatusall                        - Checked: 2010-04-07 (RLVa-1.2.0d) | Modified: RLVa-1.1.0f
			{
				std::string strFilter, strSeparator;
				if (rlvParseGetStatusOption(rlvCmd.getOption(), strFilter, strSeparator))
				{
					// NOTE: specification says response should start with '/' but RLV-1.16.1 returns an empty string when no rules are set
					for (rlv_object_map_t::const_iterator itObj = m_Objects.begin(); itObj != m_Objects.end(); ++itObj)
						strReply += itObj->second.getStatusString(strFilter, strSeparator);
				}
			}
			break;
		case RLV_BHVR_UNKNOWN:
			// Pass unknown commands on to registered command handlers
			return (notifyCommandHandlers(&RlvExtCommandHandler::onReplyCommand, rlvCmd, eRet, false)) ? eRet : RLV_RET_FAILED_UNKNOWN;
		default:
			// Fail with "Invalid param" if none of the above handled it
			return RLV_RET_FAILED_PARAM;
	}

	// If we made it this far then:
	//   - the command was handled successfully so we send off the response
	//   - the command failed but we still send off an - empty - response to keep the issuing script from blocking
	if (nChannel > 0)
		RlvUtil::sendChatReply(nChannel, strReply);
	else
	{
		if (RlvFloaterConsole* pConsole = LLFloaterReg::findTypedInstance<RlvFloaterConsole>("rlv_console"))
			pConsole->addCommandReply(rlvCmd.getBehaviour(), strReply);
	}

	return eRet;
}

// Checked: 2010-04-07 (RLVa-1.2.0d) | Modified: RLVa-1.1.0f
ERlvCmdRet RlvHandler::onFindFolder(const RlvCommand& rlvCmd, std::string& strReply) const
{
	RLV_ASSERT(RLV_TYPE_REPLY == rlvCmd.getParamType());
	RLV_ASSERT( (RLV_BHVR_FINDFOLDER == rlvCmd.getBehaviourType()) || (RLV_BHVR_FINDFOLDERS == rlvCmd.getBehaviourType()) ); 

	// (Compatibility: RLV 1.16.1 returns the first random folder it finds while we return a blank on no option)
	if (rlvCmd.getOption().empty())
		return RLV_RET_FAILED_OPTION;

	LLInventoryModel::cat_array_t folders;
	if (RlvInventory::instance().findSharedFolders(rlvCmd.getOption(), folders))
	{
		if (RLV_BHVR_FINDFOLDER == rlvCmd.getBehaviourType())
		{
			// We need to return an "in depth" result so whoever has the most '/' is our lucky winner
			// (maxSlashes needs to be initialized to -1 since children of the #RLV folder won't have '/' in their shared path)
			int maxSlashes = -1, curSlashes; std::string strFolderName;
			for (S32 idxFolder = 0, cntFolder = folders.size(); idxFolder < cntFolder; idxFolder++)
			{
				strFolderName = RlvInventory::instance().getSharedPath(folders.at(idxFolder));

				curSlashes = std::count(strFolderName.begin(), strFolderName.end(), '/');
				if (curSlashes > maxSlashes)
				{
					maxSlashes = curSlashes;
					strReply = strFolderName;
				}
			}
		}
		else if (RLV_BHVR_FINDFOLDERS == rlvCmd.getBehaviourType())
		{
			for (S32 idxFolder = 0, cntFolder = folders.size(); idxFolder < cntFolder; idxFolder++)
			{
				if (!strReply.empty())
					strReply.push_back(',');
				strReply += RlvInventory::instance().getSharedPath(folders.at(idxFolder));
			}
		}
	}
	return RLV_RET_SUCCESS;
}

// Checked: 2010-03-19 (RLVa-1.4.0a) | Modified: RLVa-1.1.0e
ERlvCmdRet RlvHandler::onGetAttach(const RlvCommand& rlvCmd, std::string& strReply) const
{
	RLV_ASSERT(RLV_TYPE_REPLY == rlvCmd.getParamType());
	RLV_ASSERT(RLV_BHVR_GETATTACH == rlvCmd.getBehaviourType());

	if (!isAgentAvatarValid())
		return RLV_RET_FAILED;

	// Sanity check - <option> should specify an attachment point or be empty
	S32 idxAttachPt = 0;
	if ( (rlvCmd.hasOption()) && ((idxAttachPt = RlvAttachPtLookup::getAttachPointIndex(rlvCmd.getOption())) == 0) )
		return RLV_RET_FAILED_OPTION;

	// If we're fetching all worn attachments then the reply should start with 0
	if (0 == idxAttachPt)
		strReply.push_back('0');

	for (LLVOAvatar::attachment_map_t::const_iterator itAttach = gAgentAvatarp->mAttachmentPoints.begin(); 
			itAttach != gAgentAvatarp->mAttachmentPoints.end(); ++itAttach)
	{
		const LLViewerJointAttachment* pAttachPt = itAttach->second;
		if ( (0 == idxAttachPt) || (itAttach->first == idxAttachPt) )
		{
			// Ansa: Do not include the bridge when checking for number of objects
			S32 bridge_correct = (pAttachPt->getName() == FS_BRIDGE_ATTACHMENT_POINT_NAME && FSLSLBridge::instance().isBridgeValid()) ? 1 : 0;
			bool fWorn = ((pAttachPt->getNumObjects() - bridge_correct) > 0) && 
				( (!RlvSettings::getHideLockedAttach()) || (RlvForceWear::isForceDetachable(pAttachPt, true, rlvCmd.getObjectID())) );
			strReply.push_back( (fWorn) ? '1' : '0' );
		}
	}
	return RLV_RET_SUCCESS;
}

// Checked: 2011-05-28 (RLVa-1.4.0a) | Modified: RLVa-1.4.0a
ERlvCmdRet RlvHandler::onGetAttachNames(const RlvCommand& rlvCmd, std::string& strReply) const
{
	RLV_ASSERT(RLV_TYPE_REPLY == rlvCmd.getParamType());
	RLV_ASSERT( (RLV_BHVR_GETATTACHNAMES == rlvCmd.getBehaviourType()) || (RLV_BHVR_GETADDATTACHNAMES == rlvCmd.getBehaviourType()) || 
				(RLV_BHVR_GETREMATTACHNAMES == rlvCmd.getBehaviourType()) );

	if (!isAgentAvatarValid())
		return RLV_RET_FAILED;

	ERlvAttachGroupType eAttachGroup = rlvAttachGroupFromString(rlvCmd.getOption());
	for (LLVOAvatar::attachment_map_t::const_iterator itAttach = gAgentAvatarp->mAttachmentPoints.begin(); 
			itAttach != gAgentAvatarp->mAttachmentPoints.end(); ++itAttach)
	{
		const LLViewerJointAttachment* pAttachPt = itAttach->second;
		if ( (RLV_ATTACHGROUP_INVALID == eAttachGroup) || (rlvAttachGroupFromIndex(pAttachPt->getGroup()) == eAttachGroup) )
		{
			bool fAdd = false;
			switch (rlvCmd.getBehaviourType())
			{
				case RLV_BHVR_GETATTACHNAMES:		// Every attachment point that has an attached object
					// Ansa: Do not include the bridge when checking for number of objects
					{
						S32 bridge_correct = ((pAttachPt->getName() == FS_BRIDGE_ATTACHMENT_POINT_NAME && FSLSLBridge::instance().isBridgeValid()) ? 1 : 0);
						fAdd = ((pAttachPt->getNumObjects() - bridge_correct) > 0);
					}
					break;
				case RLV_BHVR_GETADDATTACHNAMES:	// Every attachment point that can be attached to (wear replace OR wear add)
					fAdd = (gRlvAttachmentLocks.canAttach(pAttachPt) & RLV_WEAR);
					break;
				case RLV_BHVR_GETREMATTACHNAMES:	// Every attachment point that has at least one attachment that can be force-detached
					fAdd = RlvForceWear::isForceDetachable(pAttachPt);
					break;
				default:
					break;
			}

			if (fAdd)
			{
				if (!strReply.empty())
					strReply.push_back(',');
				strReply.append(pAttachPt->getName());
			}
		}
	}
	return RLV_RET_SUCCESS;
}

// Handles: @getcam_avdist=<channel>
template<> template<>
ERlvCmdRet RlvReplyHandler<RLV_BHVR_GETCAM_AVDIST>::onCommand(const RlvCommand& rlvCmd, std::string& strReply)
{
	if (rlvCmd.hasOption())
		return RLV_RET_FAILED_OPTION;
	strReply = llformat("%.3lf", (gAgentCamera.getCameraPositionGlobal() - gAgent.getPositionGlobal()).magVec());
	return RLV_RET_SUCCESS;
}

// Handles: @getcam_avdistmin=<channel>, @getcam_avdistmax=<channel>, @getcam_fovmin=<channel> and @getcam_fovmax=<channel>
template<> template<>
ERlvCmdRet RlvReplyCamMinMaxModifierHandler::onCommand(const RlvCommand& rlvCmd, std::string& strReply)
{
	if ( (rlvCmd.hasOption()) || (!boost::starts_with(rlvCmd.getBehaviour(), "getcam_")) )
		return RLV_RET_FAILED_OPTION;
	ERlvBehaviour eBhvr = RlvBehaviourDictionary::instance().getBehaviourFromString("setcam_" + rlvCmd.getBehaviour().substr(7), RLV_TYPE_ADDREM);
	if (RlvBehaviourModifier* pBhvrModifier = RlvBehaviourDictionary::instance().getModifierFromBehaviour(eBhvr))
		strReply = (pBhvrModifier->hasValue()) ? llformat("%.3f", pBhvrModifier->getValue<float>()) : LLStringUtil::null;
	return RLV_RET_SUCCESS;
}

// Handles: @camzoommin/max[:<multiplier>]=n|y - DEPRECATED
template<> template<>
ERlvCmdRet RlvBehaviourCamZoomMinMaxHandler::onCommand(const RlvCommand& rlvCmd, bool& fRefCount)
{
	// NOTE: @camzoommin/max are implemented as semi-synonyms of @setcam_fovmin/max
	F32 nMult = 1.0f;
	if ( (rlvCmd.hasOption()) && (!RlvCommandOptionHelper::parseOption(rlvCmd.getOption(), nMult)) )
		return RLV_RET_FAILED_OPTION;

	RlvBehaviourModifier* pBhvrModifier = RlvBehaviourDictionary::instance().getModifier( (RLV_BHVR_CAMZOOMMIN == rlvCmd.getBehaviourType()) ? RLV_MODIFIER_SETCAM_FOVMIN : RLV_MODIFIER_SETCAM_FOVMAX);
	if (pBhvrModifier)
	{
		if (RLV_TYPE_ADD == rlvCmd.getParamType())
		{
			gRlvHandler.m_Behaviours[(RLV_BHVR_CAMZOOMMIN == rlvCmd.getBehaviourType()) ? RLV_BHVR_SETCAM_FOVMIN : RLV_BHVR_SETCAM_FOVMAX]++;
			pBhvrModifier->addValue(DEFAULT_FIELD_OF_VIEW / nMult, rlvCmd.getObjectID(), rlvCmd.getBehaviourType());
		}
		else
		{
			gRlvHandler.m_Behaviours[(RLV_BHVR_CAMZOOMMIN == rlvCmd.getBehaviourType()) ? RLV_BHVR_SETCAM_FOVMIN : RLV_BHVR_SETCAM_FOVMAX]--;
			pBhvrModifier->removeValue(DEFAULT_FIELD_OF_VIEW / nMult, rlvCmd.getObjectID(), rlvCmd.getBehaviourType());
		}
	}

	fRefCount = true;
	return RLV_RET_SUCCESS;
}

// Handles: @getcam_fov=<channel>
template<> template<>
ERlvCmdRet RlvReplyHandler<RLV_BHVR_GETCAM_FOV>::onCommand(const RlvCommand& rlvCmd, std::string& strReply)
{
	if (rlvCmd.hasOption())
		return RLV_RET_FAILED_OPTION;
	strReply = llformat("%.3f", LLViewerCamera::getInstance()->getDefaultFOV());
	return RLV_RET_SUCCESS;
}

// Handles: @getcam_textures=<channel>
template<> template<>
ERlvCmdRet RlvReplyHandler<RLV_BHVR_GETCAM_TEXTURES>::onCommand(const RlvCommand& rlvCmd, std::string& strReply)
{
	if (rlvCmd.hasOption())
		return RLV_RET_FAILED_OPTION;
	if (RlvBehaviourModifier* pBhvrModifier = RlvBehaviourDictionary::instance().getModifier(RLV_MODIFIER_SETCAM_TEXTURE))
		strReply = (pBhvrModifier->hasValue()) ? pBhvrModifier->getValue<LLUUID>().asString() : LLStringUtil::null;
	return RLV_RET_SUCCESS;
}

// Handles: @getcommand[:<behaviour>[;<type>[;<separator>]]]=<channel>
template<> template<>
ERlvCmdRet RlvReplyHandler<RLV_BHVR_GETCOMMAND>::onCommand(const RlvCommand& rlvCmd, std::string& strReply)
{
	std::vector<std::string> optionList;
	RlvCommandOptionHelper::parseStringList(rlvCmd.getOption(), optionList);

	// If a second parameter is present it'll specify the command type
	ERlvParamType eType = RLV_TYPE_UNKNOWN;
	if (optionList.size() >= 2)
	{
		if ( (optionList[1] == "any") || (optionList[1].empty()) )
			eType = RLV_TYPE_UNKNOWN;
		else if (optionList[1] == "add")
			eType = RLV_TYPE_ADDREM;
		else if (optionList[1] == "force")
			eType = RLV_TYPE_FORCE;
		else if (optionList[1] == "reply")
			eType = RLV_TYPE_REPLY;
		else
			return RLV_RET_FAILED_OPTION;
	}

	std::list<std::string> cmdList;
	if (RlvBehaviourDictionary::instance().getCommands((optionList.size() >= 1) ? optionList[0] : LLStringUtil::null, eType, cmdList))
		strReply = boost::algorithm::join(cmdList, (optionList.size() >= 3) ? optionList[2] : std::string(RLV_OPTION_SEPARATOR) );
	return RLV_RET_SUCCESS;
}

// Handles: @getheightoffset=<channel>
template<> template<>
ERlvCmdRet RlvReplyHandler<RLV_BHVR_GETHEIGHTOFFSET>::onCommand(const RlvCommand& rlvCmd, std::string& strReply)
{
	if (!rlvCmd.getOption().empty())
		return RLV_RET_FAILED_OPTION;
	else if (!isAgentAvatarValid())
		return RLV_RET_FAILED_UNKNOWN;

	strReply = llformat("%.2f", gAgentAvatarp->getHoverOffset()[VZ] * 100);
	return RLV_RET_SUCCESS;
}

// Checked: 2010-03-09 (RLVa-1.2.0a) | Modified: RLVa-1.1.0f
ERlvCmdRet RlvHandler::onGetInv(const RlvCommand& rlvCmd, std::string& strReply) const
{
	RLV_ASSERT(RLV_TYPE_REPLY == rlvCmd.getParamType());
	RLV_ASSERT(RLV_BHVR_GETINV == rlvCmd.getBehaviourType());

	const LLViewerInventoryCategory* pFolder = RlvInventory::instance().getSharedFolder(rlvCmd.getOption());
	if (!pFolder)
		return (RlvInventory::instance().getSharedRoot() != NULL) ? RLV_RET_FAILED_OPTION : RLV_RET_FAILED_NOSHAREDROOT;

	LLInventoryModel::cat_array_t* pFolders; LLInventoryModel::item_array_t* pItems;
	gInventory.getDirectDescendentsOf(pFolder->getUUID(), pFolders, pItems);
	if (!pFolders)
		return RLV_RET_FAILED;

	for (S32 idxFolder = 0, cntFolder = pFolders->size(); idxFolder < cntFolder; idxFolder++)
	{
		// Return all folders that:
		//   - aren't hidden
		//   - aren't a folded folder (only really matters when "Enable Legacy Naming" is enabled - see related blog post)
		//     (we can skip checking for .<composite> folders since the ones we'll want to hide start with '.' anyway)
		//   - don't have any invalid characters
		const std::string& strFolder = pFolders->at(idxFolder)->getName();
		if ( (!strFolder.empty()) && (RLV_FOLDER_PREFIX_HIDDEN != strFolder[0]) &&
			 (!RlvInventory::isFoldedFolder(pFolders->at(idxFolder).get(), false)) && 
			 (std::string::npos == strFolder.find_first_of(RLV_FOLDER_INVALID_CHARS)) )
		{
			if (!strReply.empty())
				strReply.push_back(',');
			strReply += strFolder;
		}
	}
	return RLV_RET_SUCCESS;
}

struct rlv_wear_info { U32 cntWorn, cntTotal, cntChildWorn, cntChildTotal; };

// Checked: 2010-04-05 (RLVa-1.2.0d) | Modified: RLVa-1.1.0f
ERlvCmdRet RlvHandler::onGetInvWorn(const RlvCommand& rlvCmd, std::string& strReply) const
{
	// Sanity check - gAgentAvatarp can't be NULL [see RlvForceWear::isWearingItem()]
	if (!isAgentAvatarValid())
		return RLV_RET_FAILED;
	// Sanity check - folder should exist
	LLViewerInventoryCategory* pFolder = RlvInventory::instance().getSharedFolder(rlvCmd.getOption());
	if (!pFolder)
		return (RlvInventory::instance().getSharedRoot() != NULL) ? RLV_RET_FAILED_OPTION : RLV_RET_FAILED_NOSHAREDROOT;

	// Collect everything @attachall would be attaching
	LLInventoryModel::cat_array_t folders; LLInventoryModel::item_array_t items;
	RlvWearableItemCollector f(pFolder, RlvForceWear::ACTION_WEAR_REPLACE, RlvForceWear::FLAG_MATCHALL);
	gInventory.collectDescendentsIf(pFolder->getUUID(), folders, items, FALSE, f, true);

	rlv_wear_info wi = {0};

	// Add all the folders to a lookup map 
	std::map<LLUUID, rlv_wear_info> mapFolders;
	mapFolders.insert(std::pair<LLUUID, rlv_wear_info>(pFolder->getUUID(), wi));
	for (S32 idxFolder = 0, cntFolder = folders.size(); idxFolder < cntFolder; idxFolder++)
		mapFolders.insert(std::pair<LLUUID, rlv_wear_info>(folders.at(idxFolder)->getUUID(), wi));

	// Iterate over all the found items
	LLViewerInventoryItem* pItem; std::map<LLUUID, rlv_wear_info>::iterator itFolder;
	for (S32 idxItem = 0, cntItem = items.size(); idxItem < cntItem; idxItem++)
	{
		pItem = items.at(idxItem);
		if (!RlvForceWear::isWearableItem(pItem))
			continue;

		// The "folded parent" is the folder this item should be considered a direct descendent of (may or may not match actual parent)
		const LLUUID& idParent = f.getFoldedParent(pItem->getParentUUID());

		// Walk up the tree: sooner or later one of the parents will be a folder in the map
		LLViewerInventoryCategory* pParent = gInventory.getCategory(idParent);
		while ( (itFolder = mapFolders.find(pParent->getUUID())) == mapFolders.end() )
			pParent = gInventory.getCategory(pParent->getParentUUID());

		U32 &cntWorn  = (idParent == pParent->getUUID()) ? itFolder->second.cntWorn : itFolder->second.cntChildWorn, 
			&cntTotal = (idParent == pParent->getUUID()) ? itFolder->second.cntTotal : itFolder->second.cntChildTotal;

		if (RlvForceWear::isWearingItem(pItem))
			cntWorn++;
		cntTotal++;
	}

	// Extract the result for the main folder
	itFolder = mapFolders.find(pFolder->getUUID());
	wi.cntWorn = itFolder->second.cntWorn;
	wi.cntTotal = itFolder->second.cntTotal;
	mapFolders.erase(itFolder);

	// Build the result for each child folder
	for (itFolder = mapFolders.begin(); itFolder != mapFolders.end(); ++itFolder)
	{
		rlv_wear_info& wiFolder = itFolder->second;

		wi.cntChildWorn += wiFolder.cntWorn + wiFolder.cntChildWorn;
		wi.cntChildTotal += wiFolder.cntTotal + wiFolder.cntChildTotal;

		strReply += llformat(",%s|%d%d", gInventory.getCategory(itFolder->first)->getName().c_str(),
		 (0 == wiFolder.cntTotal) ? 0 : (0 == wiFolder.cntWorn) ? 1 : (wiFolder.cntWorn != wiFolder.cntTotal) ? 2 : 3,
		 (0 == wiFolder.cntChildTotal) ? 0 : (0 == wiFolder.cntChildWorn) ? 1 : (wiFolder.cntChildWorn != wiFolder.cntChildTotal) ? 2 : 3
		);
	}

	// Now just prepend the root and done
	strReply = llformat("|%d%d", (0 == wi.cntTotal) ? 0 : (0 == wi.cntWorn) ? 1 : (wi.cntWorn != wi.cntTotal) ? 2 : 3,
		(0 == wi.cntChildTotal) ? 0 : (0 == wi.cntChildWorn) ? 1 : (wi.cntChildWorn != wi.cntChildTotal) ? 2: 3) + strReply;

	return RLV_RET_SUCCESS;
}

// Checked: 2010-03-19 (RLVa-1.4.0a) | Modified: RLVa-1.2.0a
ERlvCmdRet RlvHandler::onGetOutfit(const RlvCommand& rlvCmd, std::string& strReply) const
{
	RLV_ASSERT(RLV_TYPE_REPLY == rlvCmd.getParamType());
	RLV_ASSERT(RLV_BHVR_GETOUTFIT == rlvCmd.getBehaviourType());

	// (Compatibility: RLV-1.16.1 will execute @getoutfit=<channel> if <layer> is invalid while we just return failure)
	LLWearableType::EType wtType = LLWearableType::WT_INVALID;
	if ( (rlvCmd.hasOption()) && ((wtType = LLWearableType::getInstance()->typeNameToType(rlvCmd.getOption())) == LLWearableType::WT_INVALID) )
		return RLV_RET_FAILED_OPTION;

	const LLWearableType::EType wtRlvTypes[] =
		{ 
			LLWearableType::WT_GLOVES, LLWearableType::WT_JACKET, LLWearableType::WT_PANTS, LLWearableType::WT_SHIRT, 
			LLWearableType::WT_SHOES, LLWearableType::WT_SKIRT, LLWearableType::WT_SOCKS, LLWearableType::WT_UNDERPANTS, 
			LLWearableType::WT_UNDERSHIRT, LLWearableType::WT_SKIN, LLWearableType::WT_EYES, LLWearableType::WT_HAIR, 
			LLWearableType::WT_SHAPE, LLWearableType::WT_ALPHA, LLWearableType::WT_TATTOO, LLWearableType::WT_PHYSICS,
			LLWearableType::WT_UNIVERSAL,
		};

	for (int idxType = 0, cntType = sizeof(wtRlvTypes) / sizeof(LLWearableType::EType); idxType < cntType; idxType++)
	{
		if ( (LLWearableType::WT_INVALID == wtType) || (wtRlvTypes[idxType] == wtType) )
		{
			// We never hide body parts, even if they're "locked" and we're hiding locked layers
			// (nor do we hide a layer if the issuing object is the only one that has this layer locked)
			bool fWorn = (gAgentWearables.getWearableCount(wtRlvTypes[idxType]) > 0) && 
				( (!RlvSettings::getHideLockedLayers()) || 
				  (LLAssetType::AT_BODYPART == LLWearableType::getInstance()->getAssetType(wtRlvTypes[idxType])) ||
				  (RlvForceWear::isForceRemovable(wtRlvTypes[idxType], true, rlvCmd.getObjectID())) );
			strReply.push_back( (fWorn) ? '1' : '0' );
		}
	}
	return RLV_RET_SUCCESS;
}

// Checked: 2011-05-28 (RLVa-1.4.0a) | Modified: RLVa-1.4.0a
ERlvCmdRet RlvHandler::onGetOutfitNames(const RlvCommand& rlvCmd, std::string& strReply) const
{
	RLV_ASSERT(RLV_TYPE_REPLY == rlvCmd.getParamType());
	RLV_ASSERT( (RLV_BHVR_GETOUTFITNAMES == rlvCmd.getBehaviourType()) || (RLV_BHVR_GETADDOUTFITNAMES == rlvCmd.getBehaviourType()) || 
				(RLV_BHVR_GETREMOUTFITNAMES == rlvCmd.getBehaviourType()) );

	// Sanity check - all these commands are optionless
	if (rlvCmd.hasOption())
		return RLV_RET_FAILED_OPTION;

	for (int idxType = 0; idxType < LLWearableType::WT_COUNT; idxType++)
	{
		bool fAdd = false; LLWearableType::EType wtType = (LLWearableType::EType)idxType;
		switch (rlvCmd.getBehaviourType())
		{
			case RLV_BHVR_GETOUTFITNAMES:		// Every layer that has at least one worn wearable
				fAdd = (gAgentWearables.getWearableCount(wtType) > 0);
				break;
			case RLV_BHVR_GETADDOUTFITNAMES:	// Every layer that can be worn on (wear replace OR wear add)
				fAdd = (gRlvWearableLocks.canWear(wtType) & RLV_WEAR);
				break;
			case RLV_BHVR_GETREMOUTFITNAMES:	// Every layer that has at least one wearable that can be force-removed
				fAdd = RlvForceWear::isForceRemovable(wtType);
				break;
			default:
				break;
		}

		if (fAdd)
		{
			if (!strReply.empty())
				strReply.push_back(',');
			strReply.append(LLWearableType::getInstance()->getTypeName(wtType));
		}
	}
	return RLV_RET_SUCCESS;
}

// Checked: 2010-08-30 (RLVa-1.2.1c) | Modified: RLVa-1.2.1c
ERlvCmdRet RlvHandler::onGetPath(const RlvCommand& rlvCmd, std::string& strReply) const
{
	RLV_ASSERT(RLV_TYPE_REPLY == rlvCmd.getParamType());
	RLV_ASSERT( (RLV_BHVR_GETPATH == rlvCmd.getBehaviourType()) || (RLV_BHVR_GETPATHNEW == rlvCmd.getBehaviourType()) ); 

	RlvCommandOptionGetPath rlvGetPathOption(rlvCmd);
	if (!rlvGetPathOption.isValid())
		return RLV_RET_FAILED_OPTION;

	LLInventoryModel::cat_array_t folders;
	if (RlvInventory::instance().getPath(rlvGetPathOption.getItemIDs(), folders))
	{
		if (RLV_BHVR_GETPATH == rlvCmd.getBehaviourType())
		{
			strReply = RlvInventory::instance().getSharedPath(folders.at(0));
		}
		else if (RLV_BHVR_GETPATHNEW == rlvCmd.getBehaviourType())
		{
			for (S32 idxFolder = 0, cntFolder = folders.size(); idxFolder < cntFolder; idxFolder++)
			{
				if (!strReply.empty())
					strReply.push_back(',');
				strReply += RlvInventory::instance().getSharedPath(folders.at(idxFolder));
			}
		}
	}
	return RLV_RET_SUCCESS;
}

// ============================================================================
// Command specific helper functions - @setoverlay
//

// ============================================================================
