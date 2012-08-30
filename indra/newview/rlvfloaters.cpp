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
#include "llappearancemgr.h"
#include "llavatarnamecache.h"
#include "llclipboard.h"
#include "llinventoryfunctions.h"
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

// Checked: 2012-07-14 (RLVa-1.4.7)
std::string rlvGetItemName(const LLViewerInventoryItem* pItem)
{
	if ( (pItem) && ((LLAssetType::AT_BODYPART == pItem->getType()) || (LLAssetType::AT_CLOTHING == pItem->getType())) )
		return llformat("%s (%s)", pItem->getName().c_str(), LLWearableType::getTypeName(pItem->getWearableType()).c_str());
	else if ( (pItem) && (LLAssetType::AT_OBJECT == pItem->getType()) && (isAgentAvatarValid()) )
		return llformat("%s (%s)", pItem->getName().c_str(), gAgentAvatarp->getAttachedPointName(pItem->getUUID()).c_str());
	return (pItem) ? pItem->getName() : LLStringUtil::null;
}

// Checked: 2012-07-14 (RLVa-1.4.7)
std::string rlvGetItemType(const LLViewerInventoryItem* pItem)
{
	if (pItem)
	{
		switch (pItem->getType())
		{
			case LLAssetType::AT_BODYPART:
			case LLAssetType::AT_CLOTHING:
				return "Wearable";
			case LLAssetType::AT_OBJECT:
				return "Attachment";
			default:
				break;
		}
	}
	return "Unknown";
}

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
	return llformat("%s (%s%s)", strItemName.c_str(), strAttachPtName.c_str(), (pObj == pObjRoot) ? "" : ", child");
}

// Checked: 2011-05-23 (RLVa-1.3.0c) | Added: RLVa-1.3.0c
bool rlvGetShowException(ERlvBehaviour eBhvr)
{
	switch (eBhvr)
	{
		case RLV_BHVR_RECVCHAT:
		case RLV_BHVR_RECVEMOTE:
		case RLV_BHVR_SENDIM:
		case RLV_BHVR_RECVIM:
		case RLV_BHVR_STARTIM:
		case RLV_BHVR_TPLURE:
		case RLV_BHVR_ACCEPTTP:
			return true;
		default:
			return false;
	}
}

// Checked: 2012-07-29 (RLVa-1.4.7)
std::string rlvLockMaskToString(ERlvLockMask eLockType)
{
	switch (eLockType)
	{
		case RLV_LOCK_ADD:
			return "add";
		case RLV_LOCK_REMOVE:
			return "rem";
		default:
			return "unknown";
	}
}

// Checked: 2012-07-29 (RLVa-1.4.7)
std::string rlvFolderLockPermissionToString(RlvFolderLocks::ELockPermission eLockPermission)
{
	switch (eLockPermission)
	{
		case RlvFolderLocks::PERM_ALLOW:
			return "allow";
		case RlvFolderLocks::PERM_DENY:
			return "deny";
		default:
			return "unknown";
	}
}

// Checked: 2012-07-29 (RLVa-1.4.7)
std::string rlvFolderLockScopeToString(RlvFolderLocks::ELockScope eLockScope)
{
	switch (eLockScope)
	{
		case RlvFolderLocks::SCOPE_NODE:
			return "node";
		case RlvFolderLocks::SCOPE_SUBTREE:
			return "subtree";
		default:
			return "unknown";
	}
}

// Checked: 2012-07-29 (RLVa-1.4.7)
std::string rlvFolderLockSourceToTarget(RlvFolderLocks::folderlock_source_t lockSource)
{
	switch (lockSource.first)
	{
		case RlvFolderLocks::ST_ATTACHMENT:
			{
				std::string strAttachName = rlvGetItemNameFromObjID(boost::get<LLUUID>(lockSource.second));
				return llformat("Attachment (%s)", strAttachName.c_str());
			}
		case RlvFolderLocks::ST_ATTACHMENTPOINT:
			{
				const LLViewerJointAttachment* pAttachPt = RlvAttachPtLookup::getAttachPoint(boost::get<S32>(lockSource.second));
				return llformat("Attachment point (%s)", (pAttachPt) ? pAttachPt->getName().c_str() : "Unknown");
			}
		case RlvFolderLocks::ST_FOLDER:
			{
				return "Folder: <todo>";
			}
		case RlvFolderLocks::ST_ROOTFOLDER:
			{
				return "Root folder";
			}
		case RlvFolderLocks::ST_SHAREDPATH:
			{
				const std::string& strPath = boost::get<std::string>(lockSource.second);
				return llformat("Shared path (#RLV%s%s)", (!strPath.empty()) ? "/" : "", strPath.c_str());
			}
		case RlvFolderLocks::ST_WEARABLETYPE:
			{
				const std::string& strTypeName = LLWearableType::getTypeName(boost::get<LLWearableType::EType>(lockSource.second));
				return llformat("Wearable type (%s)", strTypeName.c_str());
			}
		default:
			{
				return "(Unknown)";
			}
	}
}

// ============================================================================
// RlvFloaterBehaviours member functions
//

// Checked: 2010-04-18 (RLVa-1.3.1c) | Modified: RLVa-1.2.0e
void RlvFloaterBehaviours::onOpen(const LLSD& sdKey)
{
	m_ConnRlvCommand = gRlvHandler.setCommandCallback(boost::bind(&RlvFloaterBehaviours::onCommand, this, _1, _2));

	refreshAll();
}

// Checked: 2010-04-18 (RLVa-1.3.1c) | Modified: RLVa-1.2.0e
void RlvFloaterBehaviours::onClose(bool fQuitting)
{
	m_ConnRlvCommand.disconnect();
}

// Checked: 2010-04-18 (RLVa-1.3.1c) | Modified: RLVa-1.2.0e
void RlvFloaterBehaviours::onAvatarNameLookup(const LLUUID& idAgent, const LLAvatarName& avName)
{
	uuid_vec_t::iterator itLookup = std::find(m_PendingLookup.begin(), m_PendingLookup.end(), idAgent);
	if (itLookup != m_PendingLookup.end())
		m_PendingLookup.erase(itLookup);
	if (getVisible())
		refreshAll();
}

// Checked: 2011-05-26 (RLVa-1.3.1c) | Added: RLVa-1.3.1c
void RlvFloaterBehaviours::onBtnCopyToClipboard()
{
	std::ostringstream strRestrictions;

	strRestrictions << RlvStrings::getVersion() << "\n";

	const RlvHandler::rlv_object_map_t* pObjects = gRlvHandler.getObjectMap();
	for (RlvHandler::rlv_object_map_t::const_iterator itObj = pObjects->begin(), endObj = pObjects->end(); itObj != endObj; ++itObj)
	{
		strRestrictions << "\n" << rlvGetItemNameFromObjID(itObj->first) << ":\n";

		const rlv_command_list_t* pCommands = itObj->second.getCommandList();
		for (rlv_command_list_t::const_iterator itCmd = pCommands->begin(), endCmd = pCommands->end(); itCmd != endCmd; ++itCmd)
		{
			std::string strOption; LLUUID idOption;
			if ( (itCmd->hasOption()) && (idOption.set(itCmd->getOption(), FALSE)) && (idOption.notNull()) )
			{
				LLAvatarName avName;
				if (gObjectList.findObject(idOption))
					strOption = rlvGetItemNameFromObjID(idOption, true);
				else if (LLAvatarNameCache::get(idOption, &avName))
					strOption = (!avName.mUsername.empty()) ? avName.mUsername : avName.mDisplayName;
				else if (!gCacheName->getGroupName(idOption, strOption))
					strOption = itCmd->getOption();
			}

			strRestrictions << "  -> " << itCmd->asString();
			if ( (!strOption.empty()) && (strOption != itCmd->getOption()) )
				strRestrictions << "  [" << strOption << "]";
			if (RLV_RET_SUCCESS != itCmd->getReturnType())
				strRestrictions << "  (" << RlvStrings::getStringFromReturnCode(itCmd->getReturnType()) << ")";
			strRestrictions << "\n";
		}
	}

	LLWString wstrRestrictions = utf8str_to_wstring(strRestrictions.str());
	LLClipboard::instance().copyToClipboard(wstrRestrictions, 0, wstrRestrictions.length());
}

// Checked: 2011-05-23 (RLVa-1.3.1c) | Modified: RLVa-1.3.1c
void RlvFloaterBehaviours::onCommand(const RlvCommand& rlvCmd, ERlvCmdRet eRet)
{
	if ( (RLV_TYPE_ADD == rlvCmd.getParamType()) || (RLV_TYPE_REMOVE == rlvCmd.getParamType()) )
		refreshAll();
}

// Checked: 2011-05-23 (RLVa-1.3.1c) | Added: RLVa-1.3.1c
BOOL RlvFloaterBehaviours::postBuild()
{
	getChild<LLUICtrl>("copy_btn")->setCommitCallback(boost::bind(&RlvFloaterBehaviours::onBtnCopyToClipboard, this));
	return TRUE;
}

// Checked: 2011-05-23 (RLVa-1.3.1c) | Modified: RLVa-1.3.1c
void RlvFloaterBehaviours::refreshAll()
{
	LLCtrlListInterface* pBhvrList = childGetListInterface("behaviour_list");
	LLCtrlListInterface* pExceptList = childGetListInterface("exception_list");
	if ( (!pBhvrList) || (!pExceptList) )
		return;
	pBhvrList->operateOnAll(LLCtrlListInterface::OP_DELETE);
	pExceptList->operateOnAll(LLCtrlListInterface::OP_DELETE);

	if (!isAgentAvatarValid())
		return;

	//
	// Set-up a row we can just reuse
	//
	LLSD sdBhvrRow; LLSD& sdBhvrColumns = sdBhvrRow["columns"];
	sdBhvrColumns[0] = LLSD().with("column", "behaviour").with("type", "text");
	sdBhvrColumns[1] = LLSD().with("column", "issuer").with("type", "text");

	LLSD sdExceptRow; LLSD& sdExceptColumns = sdExceptRow["columns"];
	sdExceptColumns[0] = LLSD().with("column", "behaviour").with("type", "text");
	sdExceptColumns[1] = LLSD().with("column", "option").with("type", "text");
	sdExceptColumns[2] = LLSD().with("column", "issuer").with("type", "text");

	//
	// List behaviours
	//
	const RlvHandler::rlv_object_map_t* pObjects = gRlvHandler.getObjectMap();
	for (RlvHandler::rlv_object_map_t::const_iterator itObj = pObjects->begin(), endObj = pObjects->end(); itObj != endObj; ++itObj)
	{
		const std::string strIssuer = rlvGetItemNameFromObjID(itObj->first);

		const rlv_command_list_t* pCommands = itObj->second.getCommandList();
		for (rlv_command_list_t::const_iterator itCmd = pCommands->begin(), endCmd = pCommands->end(); itCmd != endCmd; ++itCmd)
		{
			std::string strOption; LLUUID idOption;
			if ( (itCmd->hasOption()) && (idOption.set(itCmd->getOption(), FALSE)) && (idOption.notNull()) )
			{
				LLAvatarName avName;
				if (gObjectList.findObject(idOption))
				{
					strOption = rlvGetItemNameFromObjID(idOption, true);
				}
				else if (LLAvatarNameCache::get(idOption, &avName))
				{
					strOption = (!avName.mUsername.empty()) ? avName.mUsername : avName.mDisplayName;
				}
				else if (!gCacheName->getGroupName(idOption, strOption))
				{
					if (m_PendingLookup.end() == std::find(m_PendingLookup.begin(), m_PendingLookup.end(), idOption))
					{
						LLAvatarNameCache::get(idOption, boost::bind(&RlvFloaterBehaviours::onAvatarNameLookup, this, _1, _2));
						m_PendingLookup.push_back(idOption);
					}
					strOption = itCmd->getOption();
				}
			}

			if ( (itCmd->hasOption()) && (rlvGetShowException(itCmd->getBehaviourType())) )
			{
				// List under the "Exception" tab
				sdExceptRow["enabled"] = gRlvHandler.isException(itCmd->getBehaviourType(), idOption);
				sdExceptColumns[0]["value"] = itCmd->getBehaviour();
				sdExceptColumns[1]["value"] = strOption;
				sdExceptColumns[2]["value"] = strIssuer;
				pExceptList->addElement(sdExceptRow, ADD_BOTTOM);
			}
			else
			{
				// List under the "Restrictions" tab
				sdBhvrRow["enabled"] = (RLV_RET_SUCCESS == itCmd->getReturnType());
				sdBhvrColumns[0]["value"] = (strOption.empty()) ? itCmd->asString() : itCmd->getBehaviour() + ":" + strOption;
				sdBhvrColumns[1]["value"] = strIssuer;
				pBhvrList->addElement(sdBhvrRow, ADD_BOTTOM);
			}
		}
	}
}

// ============================================================================
// RlvFloaterLocks member functions
//

// Checked: 2010-03-11 (RLVa-1.2.0)
void RlvFloaterLocks::onOpen(const LLSD& sdKey)
{
	m_ConnRlvCommand = gRlvHandler.setCommandCallback(boost::bind(&RlvFloaterLocks::onRlvCommand, this, _1, _2));

	refreshAll();
}

// Checked: 2010-03-11 (RLVa-1.2.0)
void RlvFloaterLocks::onClose(bool fQuitting)
{
	m_ConnRlvCommand.disconnect();
}

// Checked: 2012-07-14 (RLVa-1.4.7)
BOOL RlvFloaterLocks::postBuild()
{
	getChild<LLUICtrl>("refresh_btn")->setCommitCallback(boost::bind(&RlvFloaterLocks::refreshAll, this));

	return TRUE;
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

// Checked: 2010-03-18 (RLVa-1.2.0)
void RlvFloaterLocks::refreshAll()
{
	LLScrollListCtrl* pLockList = getChild<LLScrollListCtrl>("lock_list");
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

	//
	// List "nostrip" (soft) locks
	//
	sdColumns[1]["value"] = "nostrip";
	sdColumns[3]["value"] = "(Agent)";

	LLInventoryModel::cat_array_t folders; LLInventoryModel::item_array_t items;
	LLFindWearablesEx f(true, true);
	gInventory.collectDescendentsIf(LLAppearanceMgr::instance().getCOF(), folders, items, FALSE, f);

	for (LLInventoryModel::item_array_t::const_iterator itItem = items.begin(); itItem != items.end(); ++itItem)
	{
		const LLViewerInventoryItem* pItem = *itItem;
		if (!RlvForceWear::instance().isStrippable(pItem->getUUID()))
		{
			sdColumns[0]["value"] = rlvGetItemType(pItem);
			sdColumns[2]["value"] = rlvGetItemName(pItem);

			pLockList->addElement(sdRow, ADD_BOTTOM);
		}
	}

	//
	// List folder locks
	//
	{
		// Folder lock descriptors
		const RlvFolderLocks::folderlock_list_t& folderLocks = RlvFolderLocks::instance().getFolderLocks();
		for (RlvFolderLocks::folderlock_list_t::const_iterator itFolderLock = folderLocks.begin(); 
				itFolderLock != folderLocks.end(); ++itFolderLock)
		{
			const RlvFolderLocks::folderlock_descr_t* pLockDescr = *itFolderLock;
			if (pLockDescr)
			{
				sdColumns[0]["value"] = "Folder Descriptor";
				sdColumns[1]["value"] =
					rlvLockMaskToString(pLockDescr->eLockType) + "/" +
					rlvFolderLockPermissionToString(pLockDescr->eLockPermission) + "/" +
					rlvFolderLockScopeToString(pLockDescr->eLockScope);
				sdColumns[2]["value"] = rlvFolderLockSourceToTarget(pLockDescr->lockSource);
				sdColumns[3]["value"] = rlvGetItemNameFromObjID(pLockDescr->idRlvObj);

				pLockList->addElement(sdRow, ADD_BOTTOM);
			}
		}
	}

	{
		// Folder locked attachments and wearables
		uuid_vec_t idItems;
		const uuid_vec_t& folderLockAttachmentsIds = RlvFolderLocks::instance().getAttachmentLookups();
		idItems.insert(idItems.end(), folderLockAttachmentsIds.begin(), folderLockAttachmentsIds.end());
		const uuid_vec_t& folderLockWearabels = RlvFolderLocks::instance().getWearableLookups();
		idItems.insert(idItems.end(), folderLockWearabels.begin(), folderLockWearabels.end());

		for (uuid_vec_t::const_iterator itItemId = idItems.begin(); itItemId != idItems.end(); ++itItemId)
		{
			const LLViewerInventoryItem* pItem = gInventory.getItem(*itItemId);
			if (pItem)
			{
				sdColumns[0]["value"] = rlvGetItemType(pItem);
				sdColumns[1]["value"] = rlvLockMaskToString(RLV_LOCK_REMOVE);
				sdColumns[2]["value"] = rlvGetItemName(pItem);
				sdColumns[3]["value"] = "<Folder Lock>";

				pLockList->addElement(sdRow, ADD_BOTTOM);
			}
		}
	}
}

// ============================================================================
