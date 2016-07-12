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

// Generic includes
#include "llviewerprecompiledheaders.h"
#include "llagent.h"
#include "llappearancemgr.h"
#include "llappviewer.h"
#include "llgroupactions.h"
#include "llhudtext.h"
#include "llmoveview.h"
#include "llstartup.h"
#include "llviewermessage.h"
#include "llviewermenu.h"
#include "llviewerobjectlist.h"
#include "llviewerparcelmgr.h"
#include "llviewerregion.h"

// Command specific includes
#include "llagentcamera.h"				// @setcam and related
#include "llavatarnamecache.h"			// @shownames
#include "llavatarlist.h"				// @shownames
#include "llenvmanager.h"				// @setenv
#include "llfloatersidepanelcontainer.h"// @shownames
#include "lloutfitslist.h"				// @showinv - "Appearance / My Outfits" panel
#include "llpaneloutfitsinventory.h"	// @showinv - "Appearance" floater
#include "llpanelpeople.h"				// @shownames
#include "llpanelwearing.h"				// @showinv - "Appearance / Current Outfit" panel
#include "llsidepanelappearance.h"		// @showinv - "Appearance / Edit appearance" panel
#include "lltabcontainer.h"				// @showinv - Tab container control for inventory tabs
#include "lltoolmgr.h"					// @edit
#include "llviewercamera.h"				// @setcam and related
#include "llworldmapmessage.h"			// @tpto
#include "llviewertexturelist.h"		// @setcam_texture

// RLVa includes
#include "rlvactions.h"
#include "rlvfloaters.h"
#include "rlvactions.h"
#include "rlvhandler.h"
#include "rlvhelper.h"
#include "rlvinventory.h"
#include "rlvlocks.h"
#include "rlvui.h"
#include "rlvextensions.h"

// Boost includes
#include <boost/algorithm/string.hpp>

// ============================================================================
// Static variable initialization
//

BOOL RlvHandler::m_fEnabled = FALSE;

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

// Checked: 2010-04-07 (RLVa-1.2.0d) | Modified: RLVa-1.0.1d
RlvHandler::~RlvHandler()
{
	gAgent.removeListener(this);

	//delete m_pGCTimer;	// <- deletes itself
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

// ============================================================================
// Behaviour exception handling
//

// Checked: 2009-10-04 (RLVa-1.0.4a) | Modified: RLVa-1.0.4a
void RlvHandler::addException(const LLUUID& idObj, ERlvBehaviour eBhvr, const RlvExceptionOption& varOption)
{
	m_Exceptions.insert(std::pair<ERlvBehaviour, RlvException>(eBhvr, RlvException(idObj, eBhvr, varOption)));
}

// Checked: 2009-10-04 (RLVa-1.0.4c) | Modified: RLVa-1.0.4c
bool RlvHandler::isException(ERlvBehaviour eBhvr, const RlvExceptionOption& varOption, ERlvExceptionCheck typeCheck) const
{
	// We need to "strict check" exceptions only if: the restriction is actually in place *and* (isPermissive(eBhvr) == FALSE)
	if (RLV_CHECK_DEFAULT == typeCheck)
		typeCheck = ( (hasBehaviour(eBhvr)) && (!isPermissive(eBhvr)) ) ? RLV_CHECK_STRICT : RLV_CHECK_PERMISSIVE;

	uuid_vec_t objList;
	if (RLV_CHECK_STRICT == typeCheck)
	{
		// If we're "strict checking" then we need the UUID of every object that currently has 'eBhvr' restricted
		for (rlv_object_map_t::const_iterator itObj = m_Objects.begin(); itObj != m_Objects.end(); ++itObj)
			if (itObj->second.hasBehaviour(eBhvr, !hasBehaviour(RLV_BHVR_PERMISSIVE)))
				objList.push_back(itObj->first);
	}

	for (rlv_exception_map_t::const_iterator itException = m_Exceptions.lower_bound(eBhvr), 
			endException = m_Exceptions.upper_bound(eBhvr); itException != endException; ++itException)
	{
		if (itException->second.varOption == varOption)
		{
			// For permissive checks we just return on the very first match
			if (RLV_CHECK_PERMISSIVE == typeCheck)
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

// Checked: 2009-10-04 (RLVa-1.0.4a) | Modified: RLVa-1.0.4a
bool RlvHandler::isPermissive(ERlvBehaviour eBhvr) const
{
	return (RlvBehaviourDictionary::instance().getHasStrict(eBhvr)) 
		? !((hasBehaviour(RLV_BHVR_PERMISSIVE)) || (isException(RLV_BHVR_PERMISSIVE, eBhvr, RLV_CHECK_PERMISSIVE)))
		: true;
}

// Checked: 2009-10-04 (RLVa-1.0.4a) | Modified: RLVa-1.0.4a
void RlvHandler::removeException(const LLUUID& idObj, ERlvBehaviour eBhvr, const RlvExceptionOption& varOption)
{
	for (rlv_exception_map_t::iterator itException = m_Exceptions.lower_bound(eBhvr), 
			endException = m_Exceptions.upper_bound(eBhvr); itException != endException; ++itException)
	{
		if ( (itException->second.idObject == idObj) && (itException->second.varOption == varOption) )
		{
			m_Exceptions.erase(itException);
			break;
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
ERlvCmdRet RlvHandler::processCommand(const RlvCommand& rlvCmd, bool fFromObj)
{
	RLV_DEBUGS << "[" << rlvCmd.getObjectID() << "]: " << rlvCmd.asString() << RLV_ENDL;

	if (!rlvCmd.isValid())
	{
		RLV_DEBUGS << "\t-> invalid syntax" << RLV_ENDL;
		return RLV_RET_FAILED_SYNTAX;
	}
	if (rlvCmd.isBlocked())
	{
		RLV_DEBUGS << "\t-> blocked command" << RLV_ENDL;
		return RLV_RET_FAILED_DISABLED;
	}

	// Using a stack for executing commands solves a few problems:
	//   - if we passed RlvObject::m_idObj for idObj somewhere and process a @clear then idObj points to invalid/cleared memory at the end
	//   - if command X triggers command Y along the way then getCurrentCommand()/getCurrentObject() still return Y even when finished
	m_CurCommandStack.push(&rlvCmd); m_CurObjectStack.push(rlvCmd.getObjectID());
	const LLUUID& idCurObj = m_CurObjectStack.top();

	ERlvCmdRet eRet = RLV_RET_UNKNOWN;
	switch (rlvCmd.getParamType())
	{
		case RLV_TYPE_ADD:		// Checked: 2009-11-26 (RLVa-1.1.0f) | Modified: RLVa-1.1.0f
			{
				if ( (m_Behaviours[rlvCmd.getBehaviourType()]) && 
					 ( (RLV_BHVR_SETCAM == rlvCmd.getBehaviourType()) || (RLV_BHVR_SETDEBUG == rlvCmd.getBehaviourType()) || (RLV_BHVR_SETENV == rlvCmd.getBehaviourType()) ) )
				{
					// Some restrictions can only be held by one single object to avoid deadlocks
					RLV_DEBUGS << "\t- " << rlvCmd.getBehaviour() << " is already set by another object => discarding" << RLV_ENDL;
					eRet = RLV_RET_FAILED_LOCK;
					break;
				}

				rlv_object_map_t::iterator itObj = m_Objects.find(idCurObj); bool fAdded = false;
				if (itObj != m_Objects.end())
				{
					RlvObject& rlvObj = itObj->second;
					fAdded = rlvObj.addCommand(rlvCmd);
				}
				else
				{
					RlvObject rlvObj(idCurObj);
					fAdded = rlvObj.addCommand(rlvCmd);
					itObj = m_Objects.insert(std::pair<LLUUID, RlvObject>(idCurObj, rlvObj)).first;
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
	if (STATE_STARTED != LLStartUp::getStartupState())
	{
		m_Retained.push_back(RlvCommand(idObj, strCommand));
		return RLV_RET_RETAINED;
	}
	return processCommand(RlvCommand(idObj, strCommand), fFromObj);
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
			processCommand(rlvCmd, true);
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

// ============================================================================
// Externally invoked event handlers
//

bool RlvHandler::handleEvent(LLPointer<LLOldEvents::LLEvent> event, const LLSD& sdUserdata)
{
	// NOTE: we'll fire once for every group the user belongs to so we need to manually keep track of pending changes
	static LLUUID s_idLastAgentGroup = LLUUID::null;
	static bool s_fGroupChanging = false;

	if (s_idLastAgentGroup != gAgent.getGroupID())
	{
		s_idLastAgentGroup = gAgent.getGroupID();
		s_fGroupChanging = false;
	}

	// If the user managed to change their active group (= newly joined or created group) we need to reactivate the previous one
	if ( (!RlvActions::canChangeActiveGroup()) && ("new group" == event->desc()) && (m_idAgentGroup != gAgent.getGroupID()) )
	{
		// Make sure they still belong to the group
		if ( (m_idAgentGroup.notNull()) && (!gAgent.isInGroup(m_idAgentGroup)) )
		{
			m_idAgentGroup.setNull();
			s_fGroupChanging = false;
		}

		if (!s_fGroupChanging)
		{
			RlvUtil::notifyBlocked(RLV_STRING_BLOCKED_GROUPCHANGE, LLSD().with("GROUP_SLURL", (m_idAgentGroup.notNull()) ? llformat("secondlife:///app/group/%s/about", m_idAgentGroup.asString()) : "(none)"));

			// [Copy/paste from LLGroupActions::activate()]
			LLMessageSystem* msg = gMessageSystem;
			msg->newMessageFast(_PREHASH_ActivateGroup);
			msg->nextBlockFast(_PREHASH_AgentData);
			msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
			msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
			msg->addUUIDFast(_PREHASH_GroupID, m_idAgentGroup);
			gAgent.sendReliableMessage();
			s_fGroupChanging = true;
			return true;
		}
	}
	else
	{
		m_idAgentGroup = gAgent.getGroupID();
	}
	return false;
}

// Checked: 2010-08-29 (RLVa-1.2.1c) | Modified: RLVa-1.2.1c
void RlvHandler::onSitOrStand(bool fSitting)
{
	if (rlv_handler_t::isEnabled())
	{
		RlvSettings::updateLoginLastLocation();
	}

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

	return (0 != m_Objects.size());	// GC will kill itself if it has nothing to do
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
			gRlvHandler.processRetainedCommands(RLV_BHVR_VERSION, RLV_TYPE_REPLY);
			gRlvHandler.processRetainedCommands(RLV_BHVR_VERSIONNEW, RLV_TYPE_REPLY);
			gRlvHandler.processRetainedCommands(RLV_BHVR_VERSIONNUM, RLV_TYPE_REPLY);
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

	LLViewerParcelMgr::getInstance()->setTeleportFailedCallback(boost::bind(&RlvHandler::onTeleportFailed, this));
	LLViewerParcelMgr::getInstance()->setTeleportFinishedCallback(boost::bind(&RlvHandler::onTeleportFinished, this, _1));

	processRetainedCommands();
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

// Checked: 2010-04-11 (RLVa-1.3.0h) | Modified: RLVa-1.3.0h
bool RlvHandler::canTouch(const LLViewerObject* pObj, const LLVector3& posOffset /*=LLVector3::zero*/) const
{
	const LLUUID& idRoot = (pObj) ? pObj->getRootEdit()->getID() : LLUUID::null;
	bool fCanTouch = (idRoot.notNull()) && ((pObj->isHUDAttachment()) || (!hasBehaviour(RLV_BHVR_TOUCHALL))) &&
		((!hasBehaviour(RLV_BHVR_TOUCHTHIS)) || (!isException(RLV_BHVR_TOUCHTHIS, idRoot, RLV_CHECK_PERMISSIVE)));

	static RlvCachedBehaviourModifier<float> s_nFartouchDist(RLV_MODIFIER_FARTOUCHDIST);
	if (fCanTouch)
	{
		if ( (!pObj->isAttachment()) || (!pObj->permYouOwner()) )
		{
			// User can touch an object (that's isn't one of their own attachments) if:
			//   - it's an in-world object and they're not prevented from touching it (or the object is an exception)
			//   - it's an attachment and they're not prevented from touching (another avatar's) attachments (or the attachment is an exception)
			//   - not fartouch restricted (or the object is within the currently enforced fartouch distance)
			fCanTouch =
				( (!pObj->isAttachment()) ? (!hasBehaviour(RLV_BHVR_TOUCHWORLD)) || (isException(RLV_BHVR_TOUCHWORLD, idRoot, RLV_CHECK_PERMISSIVE))
				                          : ((!hasBehaviour(RLV_BHVR_TOUCHATTACH)) && (!hasBehaviour(RLV_BHVR_TOUCHATTACHOTHER))) || (isException(RLV_BHVR_TOUCHATTACH, idRoot, RLV_CHECK_PERMISSIVE)) ) &&
				( (!hasBehaviour(RLV_BHVR_FARTOUCH)) || (dist_vec_squared(gAgent.getPositionGlobal(), pObj->getPositionGlobal() + LLVector3d(posOffset)) <= s_nFartouchDist * s_nFartouchDist) );
		}
		else if (!pObj->isHUDAttachment())
		{
			// Regular attachment worn by this avie
			fCanTouch =
				((!hasBehaviour(RLV_BHVR_TOUCHATTACH)) || (isException(RLV_BHVR_TOUCHATTACH, idRoot, RLV_CHECK_PERMISSIVE))) &&
				((!hasBehaviour(RLV_BHVR_TOUCHATTACHSELF)) || (isException(RLV_BHVR_TOUCHATTACH, idRoot, RLV_CHECK_PERMISSIVE)));
		}
		else
		{
			// HUD attachment
			fCanTouch = (!hasBehaviour(RLV_BHVR_TOUCHHUD)) || (isException(RLV_BHVR_TOUCHHUD, idRoot, RLV_CHECK_PERMISSIVE));
		}
	}
	if ( (!fCanTouch) && (hasBehaviour(RLV_BHVR_TOUCHME)) )
		fCanTouch = hasBehaviourRoot(idRoot, RLV_BHVR_TOUCHME);
	return fCanTouch;
}

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
		strUTF8Text = (gSavedSettings.getBOOL("RestrainedLoveShowEllipsis")) ? "..." : "";
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
			RlvUtil::sendChatReply(nChannel, strUTF8Text);
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

// Checked: 2010-02-27 (RLVa-1.2.0a) | Modified: RLVa-1.2.0a
BOOL RlvHandler::setEnabled(BOOL fEnable)
{
	// TODO-RLVa: [RLVa-1.2.1] Allow toggling at runtime if we haven't seen any llOwnerSay("@....");
	if (m_fEnabled == fEnable)
		return fEnable;

	if (fEnable)
	{
		RLV_INFOS << "Enabling Restrained Love API support - " << RlvStrings::getVersionAbout() << RLV_ENDL;
		m_fEnabled = TRUE;

		// Initialize static classes
		RlvSettings::initClass();
		RlvStrings::initClass();

		gRlvHandler.addCommandHandler(new RlvExtGetSet());

		// Make sure we get notified when login is successful
		if (LLStartUp::getStartupState() < STATE_STARTED)
			LLAppViewer::instance()->setOnLoginCompletedCallback(boost::bind(&RlvHandler::onLoginComplete, &gRlvHandler));
		else
			gRlvHandler.onLoginComplete();

		// Set up RlvUIEnabler
		RlvUIEnabler::getInstance();

		// Reset to show assertions if the viewer version changed
		if (gSavedSettings.getString("LastRunVersion") != gLastRunVersion)
			gSavedSettings.setBOOL("RLVaShowAssertionFailures", TRUE);
	}

	return m_fEnabled;
}

BOOL RlvHandler::canDisable()
{
	return FALSE;
}

void RlvHandler::clearState()
{
/*
	// TODO-RLVa: should restore all RLV controlled debug variables to their defaults

	// Issue @clear on behalf of every object that has a currently active RLV restriction (even if it's just an exception)
	LLUUID idObj; LLViewerObject* pObj; bool fDetachable;
	while (m_Objects.size())
	{
		idObj = m_Objects.begin()->first; // Need a copy since after @clear the data it points to will no longer exist
		fDetachable = ((pObj = gObjectList.findObject(idObj)) != NULL) ? isLockedAttachment(pObj, RLV_LOCK_REMOVE) : true;

		processCommand(idObj, "clear", false);
		if (!fDetachable)
			processCommand(idObj, "detachme=force", false);
	}

	// Sanity check - these should all be empty after we issue @clear on the last object
	if ( (!m_Objects.empty()) || !(m_Exceptions.empty()) || (!m_AttachAdd.empty()) || (!m_AttachRem.empty()) )
	{
		RLV_ERRS << "Object, exception or attachment map not empty after clearing state!" << LL_ENDL;
		m_Objects.clear();
		m_Exceptions.clear();
		m_AttachAdd.clear();
		m_AttachRem.clear();
	}

	// These all need manual clearing
	memset(m_LayersAdd, 0, sizeof(S16) * WT_COUNT);
	memset(m_LayersRem, 0, sizeof(S16) * WT_COUNT);
	memset(m_Behaviours, 0, sizeof(S16) * RLV_BHVR_COUNT);
	m_Retained.clear();
	clearCommandHandlers(); // <- calls delete on all registered command handlers

	// Clear dynamically allocated memory
	delete m_pGCTimer;
	m_pGCTimer = NULL;
*/
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

	// Try a command processor first
	ERlvCmdRet eRet = rlvCmd.processCommand();
	if (RLV_RET_NO_PROCESSOR != eRet)
	{
		return eRet;
	}

	// Process the command the legacy way
	ERlvBehaviour eBhvr = rlvCmd.getBehaviourType(); ERlvParamType eType = rlvCmd.getParamType();
	
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
			gRlvHandler.m_Behaviours[eBhvr]--;
		}

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
		gRlvHandler.addException(rlvCmd.getObjectID(), rlvCmd.getBehaviourType(), idException);
	else
		gRlvHandler.removeException(rlvCmd.getObjectID(), rlvCmd.getBehaviourType(), idException);

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
	if ( (!rlvCmd.hasOption()) || (!pBhvrModifier) || (!pBhvrModifier->convertOptionValue(rlvCmd.getOption(), modValue)) )
		return RLV_RET_FAILED_OPTION;

	// HACK-RLVa: reference counting doesn't happen until control returns to our caller but the modifier callbacks will happen now so we need to adjust the reference counts here
	if (RLV_TYPE_ADD == rlvCmd.getParamType())
	{
		gRlvHandler.m_Behaviours[rlvCmd.getBehaviourType()]++;
		pBhvrModifier->addValue(modValue, rlvCmd.getObjectID());
		gRlvHandler.m_Behaviours[rlvCmd.getBehaviourType()]--;
	}
	else
	{
		gRlvHandler.m_Behaviours[rlvCmd.getBehaviourType()]--;
		pBhvrModifier->removeValue(modValue, rlvCmd.getObjectID());
		gRlvHandler.m_Behaviours[rlvCmd.getBehaviourType()]++;
	}

	fRefCount = true;
	return RLV_RET_SUCCESS;
}

// Handles: @bhvr[:<modifier>]=n|y
template<>
ERlvCmdRet RlvBehaviourGenericHandler<RLV_OPTION_NONE_OR_MODIFIER>::onCommand(const RlvCommand& rlvCmd, bool& fRefCount)
{
	// If there is an option then it should specify a valid modifier (and reference count)
	if (rlvCmd.hasOption())
		return RlvBehaviourGenericHandler<RLV_OPTION_MODIFIER>::onCommand(rlvCmd, fRefCount);

	// Add the default option on an empty modifier if needed
	RlvBehaviourModifier* pBhvrModifier = RlvBehaviourDictionary::instance().getModifierFromBehaviour(rlvCmd.getBehaviourType());
	if ( (pBhvrModifier) && (pBhvrModifier->getAddDefault()) )
	{
		// HACK-RLVa: reference counting doesn't happen until control returns to our caller but the modifier callbacks will happen now so we need to adjust the reference counts here
		if (RLV_TYPE_ADD == rlvCmd.getParamType())
		{
			gRlvHandler.m_Behaviours[rlvCmd.getBehaviourType()]++;
			pBhvrModifier->addValue(pBhvrModifier->getDefaultValue(), rlvCmd.getObjectID());
			gRlvHandler.m_Behaviours[rlvCmd.getBehaviourType()]--;
		}
		else
		{
			gRlvHandler.m_Behaviours[rlvCmd.getBehaviourType()]--;
			pBhvrModifier->removeValue(pBhvrModifier->getDefaultValue(), rlvCmd.getObjectID());
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

		// Hide the build floater if it's currently visible
		if (LLFloaterReg::instanceVisible("build"))
			LLToolMgr::instance().toggleBuildMode();
	}

	// Start or stop filtering opening the beacons floater
	if (fHasBhvr)
		RlvUIEnabler::instance().addGenericFloaterFilter("beacons");
	else
		RlvUIEnabler::instance().removeGenericFloaterFilter("beacons");
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
			pBhvrModDistMin->addValue(nDistMin * nDistMin, rlvCmd.getObjectID());
			if (optionList.size() >= 2)
				pBhvrModDistMax->addValue(nDistMax * nDistMax, rlvCmd.getObjectID());
		}
		else
		{
			pBhvrModDistMin->removeValue(nDistMin * nDistMin, rlvCmd.getObjectID());
			if (optionList.size() >= 2)
				pBhvrModDistMax->removeValue(nDistMax * nDistMax, rlvCmd.getObjectID());
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

// Handles: @setcam_eyeoffset:<vector3>=n|y and @setcam_focusoffset:<vector3>=n|y toggles
template<> template<>
void RlvBehaviourCamEyeFocusOffsetHandler::onCommandToggle(ERlvBehaviour eBhvr, bool fHasBhvr)
{
	if (fHasBhvr)
	{
		gAgentCamera.switchCameraPreset(CAMERA_RLV_SETCAM_VIEW);
	}
	else
	{
		const RlvBehaviourModifier* pBhvrEyeModifier = RlvBehaviourDictionary::instance().getModifier(RLV_MODIFIER_SETCAM_EYEOFFSET);
		const RlvBehaviourModifier* pBhvrOffsetModifier = RlvBehaviourDictionary::instance().getModifier(RLV_MODIFIER_SETCAM_FOCUSOFFSET);
		if ( (!pBhvrEyeModifier->hasValue()) && (!pBhvrOffsetModifier->hasValue()) )
			gAgentCamera.switchCameraPreset(CAMERA_PRESET_REAR_VIEW);
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

// Handles: @setcam_focusoffset:<vector3>=n|y changes
template<>
void RlvBehaviourModifierHandler<RLV_MODIFIER_SETCAM_FOCUSOFFSET>::onValueChange() const
{
	if (RlvBehaviourModifier* pBhvrModifier = RlvBehaviourDictionary::instance().getModifier(RLV_MODIFIER_SETCAM_FOCUSOFFSET))
	{
		LLControlVariable* pControl = gSavedSettings.getControl("FocusOffsetRLVaView");
		if (pBhvrModifier->hasValue())
			pControl->setValue(pBhvrModifier->getValue<LLVector3>().getValue());
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
		gRlvHandler.m_Behaviours[RLV_BHVR_SETCAM_UNLOCK] = lObjects.size();
	}

	// Manually invoke the @setcam_unlock toggle handler if we toggled it on/off
	if (fHasCamUnlock != gRlvHandler.hasBehaviour(RLV_BHVR_SETCAM_UNLOCK))
		RlvBehaviourToggleHandler<RLV_BHVR_SETCAM_UNLOCK>::onCommandToggle(RLV_BHVR_SETCAM_UNLOCK, !fHasCamUnlock);

	gAgentCamera.switchCameraPreset( (fHasBhvr) ? CAMERA_RLV_SETCAM_VIEW : CAMERA_PRESET_REAR_VIEW );
	RlvBehaviourDictionary::instance().getModifier(RLV_MODIFIER_SETCAM_AVDISTMIN)->setPrimaryObject(idRlvObject);
	RlvBehaviourDictionary::instance().getModifier(RLV_MODIFIER_SETCAM_AVDISTMAX)->setPrimaryObject(idRlvObject);
	RlvBehaviourDictionary::instance().getModifier(RLV_MODIFIER_SETCAM_ORIGINDISTMIN)->setPrimaryObject(idRlvObject);
	RlvBehaviourDictionary::instance().getModifier(RLV_MODIFIER_SETCAM_ORIGINDISTMAX)->setPrimaryObject(idRlvObject);
	RlvBehaviourDictionary::instance().getModifier(RLV_MODIFIER_SETCAM_EYEOFFSET)->setPrimaryObject(idRlvObject);
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

// Handles: @edit=n|y toggles
template<> template<>
void RlvBehaviourToggleHandler<RLV_BHVR_SETENV>::onCommandToggle(ERlvBehaviour eBhvr, bool fHasBhvr)
{
	const std::string strEnvFloaters[] = { "env_post_process", "env_settings", "env_delete_preset", "env_edit_sky", "env_edit_water", "env_edit_day_cycle" };
	for (int idxFloater = 0, cntFloater = sizeof(strEnvFloaters) / sizeof(std::string); idxFloater < cntFloater; idxFloater++)
	{
		if (fHasBhvr)
		{
			// Hide the floater if it's currently visible
			LLFloaterReg::const_instance_list_t envFloaters = LLFloaterReg::getFloaterList(strEnvFloaters[idxFloater]);
			for (LLFloater* pFloater : envFloaters)
				pFloater->closeFloater();
			RlvUIEnabler::instance().addGenericFloaterFilter(strEnvFloaters[idxFloater]);
		}
		else
		{
			RlvUIEnabler::instance().removeGenericFloaterFilter(strEnvFloaters[idxFloater]);
		}
	}

	// Don't allow toggling "Basic Shaders" and/or "Atmopsheric Shaders" through the debug settings under @setenv=n
	gSavedSettings.getControl("VertexShaderEnable")->setHiddenFromSettingsEditor(fHasBhvr);
	gSavedSettings.getControl("WindLightUseAtmosShaders")->setHiddenFromSettingsEditor(fHasBhvr);

	// Restore the user's WindLight preferences when releasing
	if (!fHasBhvr)
		LLEnvManagerNew::instance().usePrefs();
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

// Handles: @edit=n|y toggles
template<> template<>
void RlvBehaviourToggleHandler<RLV_BHVR_SHOWINV>::onCommandToggle(ERlvBehaviour eBhvr, bool fHasBhvr)
{
	if (LLApp::isQuitting())
		return;	// Nothing to do if the viewer is shutting down

	//
	// When disabling, close any inventory floaters that may be open
	//
	if (fHasBhvr)
	{
		LLFloaterReg::const_instance_list_t invFloaters = LLFloaterReg::getFloaterList("inventory");
		for (LLFloater* pFloater : invFloaters)
			pFloater->closeFloater();
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
	if (fHasBhvr)
		RlvUIEnabler::instance().addGenericFloaterFilter("inventory");
	else
		RlvUIEnabler::instance().removeGenericFloaterFilter("inventory");
}

// Handles: @shownames[:<uuid>]=n|y toggles
template<> template<>
void RlvBehaviourToggleHandler<RLV_BHVR_SHOWNAMES>::onCommandToggle(ERlvBehaviour eBhvr, bool fHasBhvr)
{
	if (LLApp::isQuitting())
		return;	// Nothing to do if the viewer is shutting down

	// Update the shownames context
	RlvActions::setShowName(RlvActions::SNC_DEFAULT, !fHasBhvr);

	// Refresh the nearby people list
	LLPanelPeople* pPeoplePanel = LLFloaterSidePanelContainer::getPanel<LLPanelPeople>("people", "panel_people");
	RLV_ASSERT( (pPeoplePanel) && (pPeoplePanel->getNearbyList()) );
	if ( (pPeoplePanel) && (pPeoplePanel->getNearbyList()) )
	{
		if (pPeoplePanel->getNearbyList()->isInVisibleChain())
			pPeoplePanel->onCommit();
		pPeoplePanel->getNearbyList()->updateAvatarNames();
	}

	// Force the use of the "display name" cache so we can filter both display and legacy names (or return back to the user's preference)
	if (fHasBhvr)
	{
		LLAvatarNameCache::setForceDisplayNames(true);
	}
	else
	{
		LLAvatarNameCache::setForceDisplayNames(false);
		LLAvatarNameCache::setUseDisplayNames(gSavedSettings.getBOOL("UseDisplayNames"));
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
	if ( (RLV_RET_SUCCESS == eRet) && (rlvCmd.hasOption()) && (!LLApp::isQuitting()) )
	{
		const LLUUID idAgent = RlvCommandOptionHelper::parseOption<LLUUID>(rlvCmd.getOption());

		// Refresh the nearby people list (if necessary)
		LLPanelPeople* pPeoplePanel = LLFloaterSidePanelContainer::getPanel<LLPanelPeople>("people", "panel_people");
		RLV_ASSERT( (pPeoplePanel) && (pPeoplePanel->getNearbyList()) );
		if ( (pPeoplePanel) && (pPeoplePanel->getNearbyList()) && (pPeoplePanel->getNearbyList()->contains(idAgent)) )
		{
			if (pPeoplePanel->getNearbyList()->isInVisibleChain())
				pPeoplePanel->onCommit();
			pPeoplePanel->getNearbyList()->updateAvatarNames();
		}

		// Refresh that avatar's name tag and all HUD text
		LLVOAvatar::invalidateNameTag(idAgent);
		LLHUDText::refreshAllObjectText();
	}
	return eRet;
}

// Handles: @shownametags[:<uuid>]=n|y toggles
template<> template<>
void RlvBehaviourToggleHandler<RLV_BHVR_SHOWNAMETAGS>::onCommandToggle(ERlvBehaviour eBhvr, bool fHasBhvr)
{
	if (LLApp::isQuitting())
		return;	// Nothing to do if the viewer is shutting down

	// Update the shownames context
	RlvActions::setShowName(RlvActions::SNC_NAMETAG, !fHasBhvr);

	// Refresh all name tags
	LLVOAvatar::invalidateNameTags();
}

// Handles: @shownametags[:<uuid>]=n|y
template<> template<>
ERlvCmdRet RlvBehaviourHandler<RLV_BHVR_SHOWNAMETAGS>::onCommand(const RlvCommand& rlvCmd, bool& fRefCount)
{
	ERlvCmdRet eRet = RlvBehaviourGenericHandler<RLV_OPTION_NONE_OR_EXCEPTION>::onCommand(rlvCmd, fRefCount);
	if ( (RLV_RET_SUCCESS == eRet) && (rlvCmd.hasOption()) )
		LLVOAvatar::invalidateNameTag(RlvCommandOptionHelper::parseOption<LLUUID>(rlvCmd.getOption()));
	return eRet;
}

// Handles: @shownearby=n|y toggles
template<> template<>
void RlvBehaviourToggleHandler<RLV_BHVR_SHOWNEARBY>::onCommandToggle(ERlvBehaviour eBhvr, bool fHasBhvr)
{
	if (LLApp::isQuitting())
		return;	// Nothing to do if the viewer is shutting down

	// Refresh the nearby people list
	LLPanelPeople* pPeoplePanel = LLFloaterSidePanelContainer::getPanel<LLPanelPeople>("people", "panel_people");
	LLAvatarList* pNearbyList = (pPeoplePanel) ? pPeoplePanel->getNearbyList() : NULL;
	RLV_ASSERT( (pPeoplePanel) && (pNearbyList) );
	if (pNearbyList)
	{
		static std::string s_strNoItemsMsg = pNearbyList->getNoItemsMsg();
		pNearbyList->setNoItemsMsg( (fHasBhvr) ? RlvStrings::getString("blocked_nearby") : s_strNoItemsMsg );
		pNearbyList->clear();

		if (pNearbyList->isInVisibleChain())
			pPeoplePanel->onCommit();
		if (!fHasBhvr)
			pPeoplePanel->updateNearbyList();
	}

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

// ============================================================================
// Command handlers (RLV_TYPE_FORCE)
//

ERlvCmdRet RlvCommandHandlerBaseImpl<RLV_TYPE_FORCE>::processCommand(const RlvCommand& rlvCmd, RlvForceHandlerFunc* pHandler)
{
	return (*pHandler)(rlvCmd);
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
					if (gAgentAvatarp->getRegion()->avatarHoverHeightEnabled())
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

// Handles: @setcam_eyeoffset[:<vector3>]=force and @setcam_focusoffset[:<vector3>]=force
template<> template<>
ERlvCmdRet RlvForceCamEyeFocusOffsetHandler::onCommand(const RlvCommand& rlvCmd)
{
	// Enforce exclusive camera locks
	if (!RlvActions::canChangeCameraPreset(rlvCmd.getObjectID()))
		return RLV_RET_FAILED_LOCK;

	LLControlVariable* pOffsetControl = gSavedSettings.getControl("CameraOffsetRLVaView");
	LLControlVariable* pFocusControl = gSavedSettings.getControl("FocusOffsetRLVaView");
	LLControlVariable* pControl = (rlvCmd.getBehaviourType() == RLV_BHVR_SETCAM_EYEOFFSET) ? pOffsetControl : pFocusControl;
	if (rlvCmd.hasOption())
	{
		LLVector3 vecOffset;
		if (!RlvCommandOptionHelper::parseOption(rlvCmd.getOption(), vecOffset))
			return RLV_RET_FAILED_OPTION;
		pControl->setValue(vecOffset.getValue());
	}
	else
	{
		pControl->resetToDefault();
	}

	gAgentCamera.switchCameraPreset( ((pOffsetControl->isDefault()) && (pFocusControl->isDefault())) ? CAMERA_PRESET_REAR_VIEW : CAMERA_RLV_SETCAM_VIEW);
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

// Handles: @setgroup:<uuid|name>=force
template<> template<>
ERlvCmdRet RlvForceHandler<RLV_BHVR_SETGROUP>::onCommand(const RlvCommand& rlvCmd)
{
	if (!RlvActions::canChangeActiveGroup(rlvCmd.getObjectID()))
	{
		return RLV_RET_FAILED_LOCK;
	}

	LLUUID idGroup; bool fValid = false;
	if (idGroup.set(rlvCmd.getOption()))
	{
		fValid = (idGroup.isNull()) || (gAgent.isInGroup(idGroup, true));
	}
	else
	{
		for (S32 idxGroup = 0, cntGroup = gAgent.mGroups.size(); (idxGroup < cntGroup) && (idGroup.isNull()); idxGroup++)
			if (boost::iequals(gAgent.mGroups.at(idxGroup).mName, rlvCmd.getOption()))
				idGroup = gAgent.mGroups.at(idxGroup).mID;
		fValid = (idGroup.notNull()) || ("none" == rlvCmd.getOption());
	}

	if (fValid)
	{
		gRlvHandler.m_idAgentGroup = idGroup;
		LLGroupActions::activate(idGroup);
	}

	return (fValid) ? RLV_RET_SUCCESS : RLV_RET_FAILED_OPTION;
}

// Handles: @sit:<uuid>=force
template<> template<>
ERlvCmdRet RlvForceHandler<RLV_BHVR_SIT>::onCommand(const RlvCommand& rlvCmd)
{
	LLViewerObject* pObj = NULL; LLUUID idTarget(rlvCmd.getOption());
	// Sanity checking - we need to know about the object and it should identify a prim/linkset
	if ( (idTarget.isNull()) || ((pObj = gObjectList.findObject(idTarget)) == NULL) || (LL_PCODE_VOLUME != pObj->getPCode()) )
		return RLV_RET_FAILED_OPTION;

	if (!RlvActions::canSit(pObj))
		return RLV_RET_FAILED_LOCK;
	else if ( (gRlvHandler.hasBehaviour(RLV_BHVR_STANDTP)) && (isAgentAvatarValid()) )
	{
		if (gAgentAvatarp->isSitting())
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

		LLWorldMapMessage::url_callback_t cb = boost::bind(&RlvUtil::teleportCallback, _1, posRegion, vecLookAt);
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
			strReply = RlvStrings::getVersionNum(rlvCmd.getObjectID());
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
			bool fWorn = (pAttachPt->getNumObjects() > 0) && 
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
					fAdd = (pAttachPt->getNumObjects() > 0);
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
			pBhvrModifier->addValue(DEFAULT_FIELD_OF_VIEW / nMult, rlvCmd.getObjectID());
		}
		else
		{
			gRlvHandler.m_Behaviours[(RLV_BHVR_CAMZOOMMIN == rlvCmd.getBehaviourType()) ? RLV_BHVR_SETCAM_FOVMIN : RLV_BHVR_SETCAM_FOVMAX]--;
			pBhvrModifier->removeValue(DEFAULT_FIELD_OF_VIEW / nMult, rlvCmd.getObjectID());
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
		const std::string& strFolder = pFolders->at(idxFolder)->getName();
		if ( (!strFolder.empty()) && (RLV_FOLDER_PREFIX_HIDDEN != strFolder[0]) && 
			 (!RlvInventory::isFoldedFolder(pFolders->at(idxFolder).get(), false)) )
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
	if ( (rlvCmd.hasOption()) && ((wtType = LLWearableType::typeNameToType(rlvCmd.getOption())) == LLWearableType::WT_INVALID) )
		return RLV_RET_FAILED_OPTION;

	const LLWearableType::EType wtRlvTypes[] =
		{ 
			LLWearableType::WT_GLOVES, LLWearableType::WT_JACKET, LLWearableType::WT_PANTS, LLWearableType::WT_SHIRT, 
			LLWearableType::WT_SHOES, LLWearableType::WT_SKIRT, LLWearableType::WT_SOCKS, LLWearableType::WT_UNDERPANTS, 
			LLWearableType::WT_UNDERSHIRT, LLWearableType::WT_SKIN, LLWearableType::WT_EYES, LLWearableType::WT_HAIR, 
			LLWearableType::WT_SHAPE, LLWearableType::WT_ALPHA, LLWearableType::WT_TATTOO, LLWearableType::WT_PHYSICS
		};

	for (int idxType = 0, cntType = sizeof(wtRlvTypes) / sizeof(LLWearableType::EType); idxType < cntType; idxType++)
	{
		if ( (LLWearableType::WT_INVALID == wtType) || (wtRlvTypes[idxType] == wtType) )
		{
			// We never hide body parts, even if they're "locked" and we're hiding locked layers
			// (nor do we hide a layer if the issuing object is the only one that has this layer locked)
			bool fWorn = (gAgentWearables.getWearableCount(wtRlvTypes[idxType]) > 0) && 
				( (!RlvSettings::getHideLockedLayers()) || 
				  (LLAssetType::AT_BODYPART == LLWearableType::getAssetType(wtRlvTypes[idxType])) ||
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
			strReply.append(LLWearableType::getTypeName(wtType));
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
