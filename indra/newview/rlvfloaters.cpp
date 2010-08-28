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

#include "llviewerprecompiledheaders.h"
#include "llscrolllistctrl.h"
#include "llviewerjointattachment.h"
#include "llviewerobjectlist.h"
#include "llvoavatarself.h"

#include "rlvfloaters.h"
#include "rlvhelper.h"
#include "rlvhandler.h"
#include "rlvlocks.h"

// ============================================================================
// Helper functions
//

// Checked: 2010-03-11 (RLVa-1.2.0a) | Modified: RLVa-1.2.0g
std::string rlvGetItemNameFromObjID(const LLUUID& idObj, bool fIncludeAttachPt = true)
{
	const LLViewerObject* pObj = gObjectList.findObject(idObj);
	const LLViewerObject* pObjRoot = (pObj) ? pObj->getRootEdit() : NULL;
	const LLViewerInventoryItem* pItem = ((pObjRoot) && (pObjRoot->isAttachment())) ? gInventory.getItem(pObjRoot->getAttachmentItemID()) : NULL;

	std::string strItemName = (pItem) ? pItem->getName() : idObj.asString();
	if ( (!fIncludeAttachPt) || (!pObj) || (!pObj->isAttachment()) || (!isAgentAvatarValid()) )
		return strItemName;

	const LLViewerJointAttachment* pAttachPt = 
		get_if_there(gAgentAvatarp->mAttachmentPoints, RlvAttachPtLookup::getAttachPointIndex(pObjRoot), (LLViewerJointAttachment*)NULL);
	std::string strAttachPtName = (pAttachPt) ? pAttachPt->getName() : std::string("Unknown");
	return llformat("%s (%s, %s)", strItemName.c_str(), strAttachPtName.c_str(), (pObj == pObjRoot) ? "root" : "child");
}

// ============================================================================
// RlvFloaterLocks member functions
//

// Checked: 2010-04-18 (RLVa-1.2.0e) | Modified: RLVa-1.2.0e
void RlvFloaterBehaviours::onOpen(const LLSD& sdKey)
{
	m_ConnRlvCommand = gRlvHandler.setCommandCallback(boost::bind(&RlvFloaterBehaviours::onRlvCommand, this, _2, _3));

	refreshAll();
}

// Checked: 2010-04-18 (RLVa-1.2.0e) | Modified: RLVa-1.2.0e
void RlvFloaterBehaviours::onClose(bool fQuitting)
{
	m_ConnRlvCommand.disconnect();

	// LLFloaterPay::~LLFloaterPay(): Name callbacks will be automatically disconnected since LLFloater is trackable <- how does that work?
}

// Checked: 2010-04-18 (RLVa-1.2.0e) | Modified: RLVa-1.2.0e
void RlvFloaterBehaviours::onRlvCommand(const RlvCommand& rlvCmd, ERlvCmdRet eRet)
{
	// Refresh on any successful @XXX=y|n command
	if ( (RLV_RET_SUCCESS == eRet) && ((RLV_TYPE_ADD == rlvCmd.getParamType()) || (RLV_TYPE_REMOVE == rlvCmd.getParamType())) )
	{
		refreshAll();
	}
}

// Checked: 2010-04-18 (RLVa-1.2.0e) | Modified: RLVa-1.2.0e
void RlvFloaterBehaviours::refreshAll()
{
	LLCtrlListInterface* pBhvrList = childGetListInterface("behaviour_list");
	if (!pBhvrList)
		return;
	pBhvrList->operateOnAll(LLCtrlListInterface::OP_DELETE);

	if (!isAgentAvatarValid())
		return;

	//
	// Set-up a row we can just reuse
	//
	LLSD sdRow;
	LLSD& sdColumns = sdRow["columns"];
	sdColumns[0]["column"] = "behaviour";   sdColumns[0]["type"] = "text";
	sdColumns[1]["column"] = "name"; sdColumns[1]["type"] = "text";

	//
	// List behaviours
	//
	const RlvHandler::rlv_object_map_t* pRlvObjects = gRlvHandler.getObjectMap();
	for (RlvHandler::rlv_object_map_t::const_iterator itObj = pRlvObjects->begin(), endObj = pRlvObjects->end(); itObj != endObj; ++itObj)
	{
		sdColumns[1]["value"] = rlvGetItemNameFromObjID(itObj->first);

		const rlv_command_list_t* pCommands = itObj->second.getCommandList();
		for (rlv_command_list_t::const_iterator itCmd = pCommands->begin(), endCmd = pCommands->end(); itCmd != endCmd; ++itCmd)
		{
			std::string strBhvr = itCmd->asString();
			
			LLUUID idOption(itCmd->getOption());
			if (idOption.notNull())
			{
				std::string strLookup;
				if ( (gCacheName->getFullName(idOption, strLookup)) || (gCacheName->getGroupName(idOption, strLookup)) )
				{
					if (strLookup.find("???") == std::string::npos)
						strBhvr.assign(itCmd->getBehaviour()).append(":").append(strLookup);
				}
				else if (m_PendingLookup.end() == std::find(m_PendingLookup.begin(), m_PendingLookup.end(), idOption))
				{
					gCacheName->get(idOption, FALSE, boost::bind(&RlvFloaterBehaviours::onAvatarNameLookup, this, _1, _2, _3, _4));
					m_PendingLookup.push_back(idOption);
				}
			}

			sdColumns[0]["value"] = strBhvr;

			pBhvrList->addElement(sdRow, ADD_BOTTOM);
		}
	}
}

// Checked: 2010-04-18 (RLVa-1.2.0e) | Modified: RLVa-1.2.0e
void RlvFloaterBehaviours::onAvatarNameLookup(const LLUUID& idAgent, const std::string& strFirst, const std::string& strLast, BOOL fGroup)
{
	std::list<LLUUID>::iterator itLookup = std::find(m_PendingLookup.begin(), m_PendingLookup.end(), idAgent);
	if (itLookup != m_PendingLookup.end())
		m_PendingLookup.erase(itLookup);

	if (getVisible())
		refreshAll();
}

// ============================================================================
// RlvFloaterLocks member functions
//

// Checked: 2010-03-11 (RLVa-1.2.0a) | Added: RLVa-1.2.0a
void RlvFloaterLocks::onOpen(const LLSD& sdKey)
{
	m_ConnRlvCommand = gRlvHandler.setCommandCallback(boost::bind(&RlvFloaterLocks::onRlvCommand, this, _2, _3));

	refreshAll();
}

// Checked: 2010-03-11 (RLVa-1.2.0a) | Added: RLVa-1.2.0a
void RlvFloaterLocks::onClose(bool fQuitting)
{
	m_ConnRlvCommand.disconnect();
}

// Checked: 2010-03-11 (RLVa-1.2.0a) | Added: RLVa-1.2.0a
void RlvFloaterLocks::onRlvCommand(const RlvCommand& rlvCmd, ERlvCmdRet eRet)
{
	// Refresh on any successful @XXX=y|n command where XXX is any of the attachment or wearable locking behaviours
	if ( (RLV_RET_SUCCESS == eRet) && ((RLV_TYPE_ADD == rlvCmd.getParamType()) || (RLV_TYPE_REMOVE == rlvCmd.getParamType())) )
	{
		switch (rlvCmd.getBehaviourType())
		{
			case RLV_BHVR_DETACH:
			case RLV_BHVR_ADDATTACH:
			case RLV_BHVR_REMATTACH:
			case RLV_BHVR_ADDOUTFIT:
			case RLV_BHVR_REMOUTFIT:
				refreshAll();
				break;
			default:
				break;
		}
	}
}

// Checked: 2010-03-18 (RLVa-1.2.0a) | Added: RLVa-1.2.0a
void RlvFloaterLocks::refreshAll()
{
	LLCtrlListInterface* pLockList = childGetListInterface("lock_list");
	if (!pLockList)
		return;
	pLockList->operateOnAll(LLCtrlListInterface::OP_DELETE);

	if (!isAgentAvatarValid())
		return;

	//
	// Set-up a row we can just reuse
	//
	LLSD sdRow;
	LLSD& sdColumns = sdRow["columns"];
	sdColumns[0]["column"] = "lock_type";   sdColumns[0]["type"] = "text";
	sdColumns[1]["column"] = "lock_addrem"; sdColumns[1]["type"] = "text";
	sdColumns[2]["column"] = "lock_target"; sdColumns[2]["type"] = "text";
	sdColumns[3]["column"] = "lock_origin"; sdColumns[3]["type"] = "text";

	//
	// List attachment locks
	//
	sdColumns[0]["value"] = "Attachment";
	sdColumns[1]["value"] = "rem";

	const RlvAttachmentLocks::rlv_attachobjlock_map_t& attachObjRem = gRlvAttachmentLocks.getAttachObjLocks();
	for (RlvAttachmentLocks::rlv_attachobjlock_map_t::const_iterator itAttachObj = attachObjRem.begin(); 
			itAttachObj != attachObjRem.end(); ++itAttachObj)
	{
		sdColumns[2]["value"] = rlvGetItemNameFromObjID(itAttachObj->first);
		sdColumns[3]["value"] = rlvGetItemNameFromObjID(itAttachObj->second);

		pLockList->addElement(sdRow, ADD_BOTTOM);
	}

	//
	// List attachment point locks
	//
	sdColumns[0]["value"] = "Attachment Point";

	sdColumns[1]["value"] = "add";
	const RlvAttachmentLocks::rlv_attachptlock_map_t& attachPtAdd = gRlvAttachmentLocks.getAttachPtLocks(RLV_LOCK_ADD);
	for (RlvAttachmentLocks::rlv_attachptlock_map_t::const_iterator itAttachPt = attachPtAdd.begin(); 
			itAttachPt != attachPtAdd.end(); ++itAttachPt)
	{
		const LLViewerJointAttachment* pAttachPt = 
			get_if_there(gAgentAvatarp->mAttachmentPoints, itAttachPt->first, (LLViewerJointAttachment*)NULL);
		sdColumns[2]["value"] = pAttachPt->getName();
		sdColumns[3]["value"] = rlvGetItemNameFromObjID(itAttachPt->second);

		pLockList->addElement(sdRow, ADD_BOTTOM);
	}

	sdColumns[1]["value"] = "rem";
	const RlvAttachmentLocks::rlv_attachptlock_map_t& attachPtRem = gRlvAttachmentLocks.getAttachPtLocks(RLV_LOCK_REMOVE);
	for (RlvAttachmentLocks::rlv_attachptlock_map_t::const_iterator itAttachPt = attachPtRem.begin(); 
			itAttachPt != attachPtRem.end(); ++itAttachPt)
	{
		const LLViewerJointAttachment* pAttachPt = 
			get_if_there(gAgentAvatarp->mAttachmentPoints, itAttachPt->first, (LLViewerJointAttachment*)NULL);
		sdColumns[2]["value"] = pAttachPt->getName();
		sdColumns[3]["value"] = rlvGetItemNameFromObjID(itAttachPt->second);

		pLockList->addElement(sdRow, ADD_BOTTOM);
	}

	//
	// List wearable type locks
	//
	sdColumns[0]["value"] = "Wearable Type";

	sdColumns[1]["value"] = "add";
	const RlvWearableLocks::rlv_wearabletypelock_map_t& wearableTypeAdd = gRlvWearableLocks.getWearableTypeLocks(RLV_LOCK_ADD);
	for (RlvWearableLocks::rlv_wearabletypelock_map_t::const_iterator itWearableType = wearableTypeAdd.begin(); 
			itWearableType != wearableTypeAdd.end(); ++itWearableType)
	{
		sdColumns[2]["value"] = LLWearableType::getTypeLabel(itWearableType->first);
		sdColumns[3]["value"] = rlvGetItemNameFromObjID(itWearableType->second);

		pLockList->addElement(sdRow, ADD_BOTTOM);
	}

	sdColumns[1]["value"] = "rem";
	const RlvWearableLocks::rlv_wearabletypelock_map_t& wearableTypeRem = gRlvWearableLocks.getWearableTypeLocks(RLV_LOCK_REMOVE);
	for (RlvWearableLocks::rlv_wearabletypelock_map_t::const_iterator itWearableType = wearableTypeRem.begin(); 
			itWearableType != wearableTypeRem.end(); ++itWearableType)
	{
		sdColumns[2]["value"] = LLWearableType::getTypeName(itWearableType->first);
		sdColumns[3]["value"] = rlvGetItemNameFromObjID(itWearableType->second);

		pLockList->addElement(sdRow, ADD_BOTTOM);
	}
}

// ============================================================================
