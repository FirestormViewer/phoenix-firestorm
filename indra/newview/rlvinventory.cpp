/** 
 * @file rlvinventory.cpp
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
#include "llappearancemgr.h"
#include "llinventoryobserver.h"
#include "llstartup.h"
#include "llviewerfoldertype.h"
#include "llviewermessage.h"
#include "llviewerobject.h"
#include "llvoavatarself.h"

#include "rlvdefines.h"
#include "rlvcommon.h"
#include "rlvlocks.h"
#include "rlvinventory.h"
#include "rlvhandler.h"

#include "boost/algorithm/string.hpp"

// ============================================================================
// Static variable initialization
//

const std::string RlvInventory::cstrSharedRoot = RLV_ROOT_FOLDER;

// ============================================================================
// Helper classes
//

// TODO-RLVa: [RLVa-1.2.1] This class really shouldn't be calling "fetchSharedLinks" directly so find a better way
class RlvSharedInventoryFetcher : public LLInventoryFetchDescendentsObserver
{
public:
	RlvSharedInventoryFetcher(const uuid_vec_t& idFolders): LLInventoryFetchDescendentsObserver(idFolders) {}
	virtual ~RlvSharedInventoryFetcher() {}

	virtual void done()
	{
		RLV_INFOS << "Shared folders fetch completed" << LL_ENDL;
		RlvInventory::instance().m_fFetchComplete = true;
		RlvInventory::instance().fetchSharedLinks();

		gInventory.removeObserver(this);
		delete this;
	}
};

// ============================================================================
// RlvInventory member functions
//

// Checked: 2011-03-28 (RLVa-1.3.0g) | Modified: RLVa-1.3.0g
RlvInventory::RlvInventory()
	: m_fFetchStarted(false), m_fFetchComplete(false)
{
}

// Checked: 2011-03-28 (RLVa-1.3.0g) | Added: RLVa-1.3.0g
RlvInventory::~RlvInventory()
{
	if (gInventory.containsObserver(this))
		gInventory.removeObserver(this);
}

// Checked: 2011-03-28 (RLVa-1.3.0g) | Added: RLVa-1.3.0g
void RlvInventory::changed(U32 mask)
{
	const LLInventoryModel::changed_items_t& idsChanged = gInventory.getChangedIDs();
	if (std::find(idsChanged.begin(), idsChanged.end(), m_idRlvRoot) != idsChanged.end())
	{
		gInventory.removeObserver(this);

		LLUUID idRlvRootPrev = m_idRlvRoot;
		m_idRlvRoot.setNull();

		if (idRlvRootPrev != getSharedRootID())
			m_OnSharedRootIDChanged();
	}
}

// Checked: 2010-02-28 (RLVa-1.2.0a) | Modified: RLVa-1.0.0h
void RlvInventory::fetchSharedInventory()
{
	// Sanity check - don't fetch if we're already fetching, or if we don't have a shared root
	const LLViewerInventoryCategory* pRlvRoot = getSharedRoot();
	if ( (m_fFetchStarted) || (!pRlvRoot) )
		return;

	// Grab all the folders under the shared root
	LLInventoryModel::cat_array_t folders; LLInventoryModel::item_array_t items;
	gInventory.collectDescendents(pRlvRoot->getUUID(), folders, items, FALSE);

	// Add them to the "to fetch" list
	uuid_vec_t idFolders;
	idFolders.push_back(pRlvRoot->getUUID());
	for (S32 idxFolder = 0, cntFolder = folders.count(); idxFolder < cntFolder; idxFolder++)
		idFolders.push_back(folders.get(idxFolder)->getUUID());

	// Now fetch them all in one go
	RlvSharedInventoryFetcher* pFetcher = new RlvSharedInventoryFetcher(idFolders);

	RLV_INFOS << "Starting fetch of " << idFolders.size() << " shared folders" << RLV_ENDL;
	pFetcher->startFetch();
	m_fFetchStarted = true;

	if (pFetcher->isFinished())
		pFetcher->done();
	else
		gInventory.addObserver(pFetcher);
}

// Checked: 2010-02-28 (RLVa-1.2.0a) | Modified: RLVa-1.0.0h
void RlvInventory::fetchSharedLinks()
{
	// TOFIX-RLVa: [RLVa-1.2.1] Finish adding support for AT_LINK_FOLDER
	const LLViewerInventoryCategory* pRlvRoot = getSharedRoot();
	if (!pRlvRoot)
		return;

	// Grab all the inventory links under the shared root
	LLInventoryModel::cat_array_t folders; LLInventoryModel::item_array_t items; RlvIsLinkType f;
	gInventory.collectDescendentsIf(pRlvRoot->getUUID(), folders, items, FALSE, f, FALSE);

	// Add them to the "to fetch" list based on link type
	uuid_vec_t idFolders, idItems;
	for (S32 idxItem = 0, cntItem = items.count(); idxItem < cntItem; idxItem++)
	{
		const LLViewerInventoryItem* pItem = items.get(idxItem);
		switch (pItem->getActualType())
		{
			case LLAssetType::AT_LINK:
				idItems.push_back(pItem->getLinkedUUID());
				break;
			case LLAssetType::AT_LINK_FOLDER:
				idFolders.push_back(pItem->getLinkedUUID());
				break;
			default:
				break;;
		}
	}

	RLV_INFOS << "Starting link target fetch of " << idItems.size() << " items and " << idFolders.size() << " folders" << RLV_ENDL;

	// Fetch all the link item targets
	LLInventoryFetchItemsObserver itemFetcher(idItems);
	itemFetcher.startFetch();

	// Fetch all the link folder targets
	// TODO!
}

// Checked: 2010-02-28 (RLVa-1.2.0a) | Modified: RLVa-1.2.0a
void RlvInventory::fetchWornItems()
{
	uuid_vec_t idItems;

	// Fetch all currently worn clothing layers and body parts
	for (int type = 0; type < LLWearableType::WT_COUNT; type++)
	{
		// RELEASE-RLVa: [SL-2.0.0] Needs rewriting once 'LLAgentWearables::MAX_WEARABLES_PER_TYPE > 1'
		const LLUUID& idItem = gAgentWearables.getWearableItemID((LLWearableType::EType)type, 0);
		if (idItem.notNull())
			idItems.push_back(idItem);
	}

	// Fetch all currently worn attachments
	if (isAgentAvatarValid())
	{
		for (LLVOAvatar::attachment_map_t::const_iterator itAttachPt = gAgentAvatarp->mAttachmentPoints.begin(); 
				itAttachPt != gAgentAvatarp->mAttachmentPoints.end(); ++itAttachPt)
		{
			const LLViewerJointAttachment* pAttachPt = itAttachPt->second;
			if (pAttachPt)
			{
				for (LLViewerJointAttachment::attachedobjs_vec_t::const_iterator itAttachObj = pAttachPt->mAttachedObjects.begin();
					 itAttachObj != pAttachPt->mAttachedObjects.end(); ++itAttachObj)
				{
					const LLViewerObject* pAttachObj = (*itAttachObj);
					if ( (pAttachObj) && (pAttachObj->getAttachmentItemID().notNull()) )
						idItems.push_back(pAttachObj->getAttachmentItemID());
				}
			}
		}
	}

	LLInventoryFetchItemsObserver itemFetcher(idItems);
	itemFetcher.startFetch();
}

// Checked: 2010-04-07 (RLVa-1.2.0a) | Modified: RLVa-1.0.0h
bool RlvInventory::findSharedFolders(const std::string& strCriteria, LLInventoryModel::cat_array_t& folders) const
{
	// Sanity check - can't do anything without a shared root
	const LLViewerInventoryCategory* pRlvRoot = RlvInventory::instance().getSharedRoot();
	if (!pRlvRoot)
		return false;

	folders.clear();
	LLInventoryModel::item_array_t items;
	RlvCriteriaCategoryCollector f(strCriteria);
	gInventory.collectDescendentsIf(pRlvRoot->getUUID(), folders, items, FALSE, f);

	return (folders.count() != 0);
}

// Checked: 2010-08-30 (RLVa-1.2.1c) | Modified: RLVa-1.2.1c
bool RlvInventory::getPath(const uuid_vec_t& idItems, LLInventoryModel::cat_array_t& folders) const
{
	// Sanity check - can't do anything without a shared root
	const LLViewerInventoryCategory* pRlvRoot = RlvInventory::instance().getSharedRoot();
	if (!pRlvRoot)
		return false;

	folders.clear();
	for (uuid_vec_t::const_iterator itItem = idItems.begin(); itItem != idItems.end(); ++itItem)
	{
		const LLInventoryItem* pItem = gInventory.getItem(*itItem);
		if ( (pItem) && (gInventory.isObjectDescendentOf(pItem->getUUID(), pRlvRoot->getUUID())) )
		{
			// If the containing folder is a folded folder we need its parent
			LLViewerInventoryCategory* pFolder = gInventory.getCategory(pItem->getParentUUID());
			if (RlvInventory::instance().isFoldedFolder(pFolder, true))
				pFolder = gInventory.getCategory(pFolder->getParentUUID());
			folders.push_back(pFolder);
		}
	}

	return (folders.count() != 0);
}

// Checked: 2011-10-06 (RLVa-1.4.2a) | Modified: RLVa-1.4.2a
const LLUUID& RlvInventory::getSharedRootID() const
{
	if ( (m_idRlvRoot.isNull()) && (gInventory.isInventoryUsable()) )
	{
		LLInventoryModel::cat_array_t* pFolders; LLInventoryModel::item_array_t* pItems;
		gInventory.getDirectDescendentsOf(gInventory.getRootFolderID(), pFolders, pItems);
		if (pFolders)
		{
			// NOTE: we might have multiple #RLV folders (pick the first one with sub-folders; otherwise the last one with no sub-folders)
			const LLViewerInventoryCategory* pFolder;
			for (S32 idxFolder = 0, cntFolder = pFolders->count(); idxFolder < cntFolder; idxFolder++)
			{
				if ( ((pFolder = pFolders->get(idxFolder)) != NULL) && (cstrSharedRoot == pFolder->getName()) )
				{
					m_idRlvRoot = pFolder->getUUID();
					if (getDirectDescendentsFolderCount(pFolder) > 0)
						break;
				}
			}
			if ( (m_idRlvRoot.notNull()) && (!gInventory.containsObserver((RlvInventory*)this)) )
				gInventory.addObserver((RlvInventory*)this);
		}
	}
	return m_idRlvRoot;
}

// Checked: 2010-02-28 (RLVa-1.2.0a) | Modified: RLVa-1.0.1a
LLViewerInventoryCategory* RlvInventory::getSharedFolder(const LLUUID& idParent, const std::string& strFolderName) const
{
	LLInventoryModel::cat_array_t* pFolders; LLInventoryModel::item_array_t* pItems;
	gInventory.getDirectDescendentsOf(idParent, pFolders, pItems);
	if ( (!pFolders) || (strFolderName.empty()) )
		return NULL;

	// If we can't find an exact match then we'll settle for a "contains" match
	LLViewerInventoryCategory* pPartial = NULL;

	//LLStringUtil::toLower(strFolderName); <- everything was already converted to lower case before

	std::string strName;
	for (S32 idxFolder = 0, cntFolder = pFolders->count(); idxFolder < cntFolder; idxFolder++)
	{
		LLViewerInventoryCategory* pFolder = pFolders->get(idxFolder);

		strName = pFolder->getName();
		if (strName.empty())
			continue;
		LLStringUtil::toLower(strName);

		if (strFolderName == strName)
			return pFolder;		// Found an exact match, no need to keep on going
		else if ( (!pPartial) && (RLV_FOLDER_PREFIX_HIDDEN != strName[0]) && (std::string::npos != strName.find(strFolderName)) )
			pPartial = pFolder;	// Found a partial (non-hidden) match, but we might still find an exact one (first partial match wins)
	}

	return pPartial;
}

// Checked: 2010-02-28 (RLVa-1.2.0a) | Modified: RLVa-0.2.0e
LLViewerInventoryCategory* RlvInventory::getSharedFolder(const std::string& strPath) const
{
	// Sanity check - no shared root => no shared folder
	LLViewerInventoryCategory* pRlvRoot = getSharedRoot(), *pFolder = pRlvRoot;
	if (!pRlvRoot)
		return NULL;

	// Walk the path (starting at the root)
	boost_tokenizer tokens(strPath, boost::char_separator<char>("/", "", boost::drop_empty_tokens));
	for (boost_tokenizer::const_iterator itToken = tokens.begin(); itToken != tokens.end(); ++itToken)
	{
		pFolder = getSharedFolder(pFolder->getUUID(), *itToken);
		if (!pFolder)
			return NULL;	// No such folder
	}

	return pFolder;			// If strPath was empty or just a bunch of //// then: pFolder == pRlvRoot
}

// Checked: 2010-03-02 (RLVa-1.2.0a) | Modified: RLVa-0.2.0g
std::string RlvInventory::getSharedPath(const LLViewerInventoryCategory* pFolder) const
{
	// Sanity check - no shared root or no folder => no path
	const LLViewerInventoryCategory* pRlvRoot = getSharedRoot();
	if ( (!pRlvRoot) || (!pFolder) || (pRlvRoot->getUUID() == pFolder->getUUID()) )
		return std::string();

	const LLUUID& idRLV  = pRlvRoot->getUUID();
	const LLUUID& idRoot = gInventory.getRootFolderID();
	std::string strPath;

	// Walk up the tree until we reach the top
	RLV_ASSERT(gInventory.isObjectDescendentOf(pFolder->getUUID(), pRlvRoot->getUUID()));
	while (pFolder)
	{
		strPath = "/" + pFolder->getName() + strPath;

		const LLUUID& idParent = pFolder->getParentUUID();
		if (idRLV == idParent)			// Reached the shared root, we're done
			break;
		else if (idRoot == idParent)	// We reached the agent's inventory root (indicative of a logic error elsewhere)
			return std::string();

		pFolder = gInventory.getCategory(idParent);
	}

	return strPath.erase(0, 1);
}

// Checked: 2011-10-06 (RLVa-1.4.2a) | Added: RLVa-1.4.2a
S32 RlvInventory::getDirectDescendentsFolderCount(const LLInventoryCategory* pFolder)
{
	LLInventoryModel::cat_array_t* pFolders = NULL; LLInventoryModel::item_array_t* pItems = NULL;
	if (pFolder)
		gInventory.getDirectDescendentsOf(pFolder->getUUID(), pFolders, pItems);
	return (pFolders) ? pFolders->size() : 0;
}

// Checked: 2009-05-26 (RLVa-0.2.0d) | Modified: RLVa-0.2.0d
S32 RlvInventory::getDirectDescendentsItemCount(const LLInventoryCategory* pFolder, LLAssetType::EType filterType)
{
	S32 cntType = 0;
	if (pFolder)
	{
		LLInventoryModel::cat_array_t* pFolders; LLInventoryModel::item_array_t* pItems;
		gInventory.getDirectDescendentsOf(pFolder->getUUID(), pFolders, pItems);

		if (pItems)
		{
			for (S32 idxItem = 0, cntItem = pItems->count(); idxItem < cntItem; idxItem++)
				if (pItems->get(idxItem)->getType() == filterType)
					cntType++;
		}
	}
	return cntType;
}

// Checked: 2012-11-28 (RLVa-1.4.8)
bool RlvInventory::isGiveToRLVOffer(const LLOfferInfo& offerInfo)
{
	if ( (!RlvSettings::getForbidGiveToRLV()) && (RlvInventory::instance().getSharedRoot()) )
	{
		if (offerInfo.mFromObject)
		{
			return 
				(IM_TASK_INVENTORY_OFFERED == offerInfo.mIM) && 
				(LLAssetType::AT_CATEGORY == offerInfo.mType) && (offerInfo.mDesc.find(RLV_PUTINV_PREFIX) == 1);
		}
		else
		{
			return
				(IM_INVENTORY_OFFERED == offerInfo.mIM) && 
				(LLAssetType::AT_CATEGORY == offerInfo.mType) && (offerInfo.mDesc.find(RLV_PUTINV_PREFIX) == 0);
		}
	}
	return false;
}

// ============================================================================
// RlvRenameOnWearObserver member functions
//

// Checked: 2010-03-14 (RLVa-1.2.0a) | Added: RLVa-1.2.0a
void RlvRenameOnWearObserver::done()
{
	gInventory.removeObserver(this);

	// We shouldn't be messing with inventory items during LLInventoryModel::notifyObservers()
	doOnIdleOneTime(boost::bind(&RlvRenameOnWearObserver::doneIdle, this));
}

// Checked: 2010-03-14 (RLVa-1.2.0a) | Added: RLVa-1.2.0a
void RlvRenameOnWearObserver::doneIdle()
{
	const LLViewerInventoryCategory* pRlvRoot = NULL;
	if ( (RlvSettings::getEnableSharedWear()) || (!RlvSettings::getSharedInvAutoRename()) || (LLStartUp::getStartupState() < STATE_STARTED) || 
		 (!isAgentAvatarValid()) || ((pRlvRoot = RlvInventory::instance().getSharedRoot()) == NULL) )
	{
		delete this;
		return;
	}

	const LLViewerJointAttachment* pAttachPt = NULL; S32 idxAttachPt = 0;
	// Kitty says this check really doesn't do much of anything. -- TS
	// RLV_ASSERT(mComplete.size() > 0);	// Catch instances where we forgot to call startFetch()
	for (uuid_vec_t::const_iterator itItem = mComplete.begin(); itItem != mComplete.end(); ++itItem)
	{
		const LLUUID& idAttachItem = *itItem;

		// If the item resides under #RLV we'll rename it directly; otherwise settle for "renaming" all of its links residing under #RLV
		LLInventoryModel::item_array_t items;
		if (gInventory.isObjectDescendentOf(idAttachItem, pRlvRoot->getUUID()))
			items.push_back(gInventory.getItem(idAttachItem));
		else
			items = gInventory.collectLinkedItems(idAttachItem, pRlvRoot->getUUID());
		if (items.empty())
			continue;

		if ( ((pAttachPt = gAgentAvatarp->getWornAttachmentPoint(idAttachItem)) == NULL) ||
			 ((idxAttachPt = RlvAttachPtLookup::getAttachPointIndex(pAttachPt)) == 0) )
		{
			RLV_ASSERT(false);
			continue;
		}

		for (S32 idxItem = 0, cntItem = items.count(); idxItem < cntItem; idxItem++)
		{
			LLViewerInventoryItem* pItem = items.get(idxItem);
			if (!pItem)
				continue;

			S32 idxAttachPtItem = RlvAttachPtLookup::getAttachPointIndex(pItem);
			if ( (idxAttachPt == idxAttachPtItem) || (idxAttachPtItem) )
				continue;

			std::string strAttachPt = pAttachPt->getName();
			LLStringUtil::toLower(strAttachPt);

			// If we can modify the item then we rename it directly, otherwise we create a new folder and move it
			if (pItem->getPermissions().allowModifyBy(gAgent.getID()))
			{
				std::string strName = pItem->getName();
				LLStringUtil::truncate(strName, DB_INV_ITEM_NAME_STR_LEN - strAttachPt.length() - 3);

				strName += " (" + strAttachPt + ")";

				pItem->rename(strName);
				pItem->updateServer(FALSE);
				gInventory.addChangedMask(LLInventoryObserver::LABEL, pItem->getUUID());
			}
			else
			{
				// Don't do anything if the item is a direct descendant of the shared root, or a folded folder
				LLViewerInventoryCategory* pFolder = gInventory.getCategory(pItem->getParentUUID());
				if ( (pFolder) && (pFolder->getUUID() != pRlvRoot->getUUID()) && (!RlvInventory::isFoldedFolder(pFolder, false)) )
				{
					std::string strFolderName = ".(" + strAttachPt + ")";

					// Rename the item's parent folder if it's called "New Folder", isn't directly under #RLV and contains exactly 1 object
					if ( (LLViewerFolderType::lookupNewCategoryName(LLFolderType::FT_NONE) == pFolder->getName()) && 
						 (pFolder->getParentUUID() != pRlvRoot->getUUID()) && 
						 (1 == RlvInventory::getDirectDescendentsItemCount(pFolder, LLAssetType::AT_OBJECT)) )
					{
						pFolder->rename(strFolderName);
						pFolder->updateServer(FALSE);
						gInventory.addChangedMask(LLInventoryObserver::LABEL, pFolder->getUUID());
					}
					else
					{
						// "No modify" item with a non-renameable parent: create a new folder named and move the item into it
						LLUUID idFolder = gInventory.createNewCategory(pFolder->getUUID(), LLFolderType::FT_NONE, strFolderName,
						                                               &RlvRenameOnWearObserver::onCategoryCreate, new LLUUID(pItem->getUUID()));
						if (idFolder.notNull())
						{
							// Not using the new 'CreateInventoryCategory' cap so manually invoke the callback
							RlvRenameOnWearObserver::onCategoryCreate(LLSD().with("folder_id", idFolder), new LLUUID(pItem->getUUID()));
						}
					}
				}
			}
		}
	}
	gInventory.notifyObservers();

	delete this;
}

// Checked: 2012-03-22 (RLVa-1.4.6) | Added: RLVa-1.4.6
void RlvRenameOnWearObserver::onCategoryCreate(const LLSD& sdData, void* pParam)
{
	LLUUID idFolder = sdData["folder_id"].asUUID();
	LLUUID* pidItem = (LLUUID*)pParam;

	if ( (idFolder.notNull()) && (pidItem) && (pidItem->notNull()) )
		move_inventory_item(gAgent.getID(), gAgent.getSessionID(), *pidItem, idFolder, std::string(), NULL);

	delete pidItem;
}

// ============================================================================
// "Give to #RLV" helper classes
//

// Checked: 2010-04-18 (RLVa-1.2.0e) | Added: RLVa-1.2.0e
void RlvGiveToRLVTaskOffer::changed(U32 mask)
{
	if (mask & LLInventoryObserver::ADD)
	{
		LLMessageSystem* pMsg = gMessageSystem;
		if ( (pMsg->getMessageName()) && (0 == strcmp(pMsg->getMessageName(), "BulkUpdateInventory")) )
		{
			LLUUID idTransaction;
			pMsg->getUUIDFast(_PREHASH_AgentData, _PREHASH_TransactionID, idTransaction);
			if(m_idTransaction == idTransaction)
			{
				LLUUID idInvObject;
				for(S32 idxBlock = 0, cntBlock = pMsg->getNumberOfBlocksFast(_PREHASH_FolderData); idxBlock < cntBlock; idxBlock++)
				{
					pMsg->getUUIDFast(_PREHASH_FolderData, _PREHASH_FolderID, idInvObject, idxBlock);
					if ( (idInvObject.notNull()) && (std::find(m_Folders.begin(), m_Folders.end(), idInvObject) == m_Folders.end()) )
						m_Folders.push_back(idInvObject);
				}

				done();
			}
		}
	}
}

// Checked: 2010-04-18 (RLVa-1.2.0e) | Added: RLVa-1.2.0e
void RlvGiveToRLVTaskOffer::done()
{
	gInventory.removeObserver(this);

	// We shouldn't be messing with inventory items during LLInventoryModel::notifyObservers()
	doOnIdleOneTime(boost::bind(&RlvGiveToRLVTaskOffer::doneIdle, this));
}

// Checked: 2010-04-18 (RLVa-1.2.0e) | Added: RLVa-1.2.0e
void RlvGiveToRLVTaskOffer::doneIdle()
{
	const LLViewerInventoryCategory* pRlvRoot = RlvInventory::instance().getSharedRoot();
	if (pRlvRoot)
	{
		for (folder_ref_t::const_iterator itFolder = m_Folders.begin(); itFolder != m_Folders.end(); ++itFolder)
		{
			const LLViewerInventoryCategory* pFolder = gInventory.getCategory(*itFolder);
			if (!pFolder)
				continue;

			std::string strFolder = pFolder->getName();
			if ( (pRlvRoot->getUUID() == pFolder->getParentUUID() ) && (strFolder.find(RLV_PUTINV_PREFIX) == 0))
			{
				LLPointer<LLViewerInventoryCategory> pNewFolder = new LLViewerInventoryCategory(pFolder);
				pNewFolder->rename(strFolder.erase(0, strFolder.find(RLV_FOLDER_PREFIX_PUTINV)));
				pNewFolder->updateServer(FALSE);
				gInventory.updateCategory(pNewFolder);

				RlvBehaviourNotifyHandler::sendNotification("accepted_in_rlv inv_offer " + pNewFolder->getName());

				gInventory.notifyObservers();
				break;
			}
		}
	}
	delete this;
}

// Checked: 2010-04-18 (RLVa-1.2.0e) | Modified: RLVa-1.2.0e
void RlvGiveToRLVAgentOffer::done()
{
	gInventory.removeObserver(this);

	// We shouldn't be messing with inventory items during LLInventoryModel::notifyObservers()
	doOnIdleOneTime(boost::bind(&RlvGiveToRLVAgentOffer::doneIdle, this));
}

// Checked: 2010-04-18 (RLVa-1.2.0e) | Modified: RLVa-1.2.0e
void RlvGiveToRLVAgentOffer::doneIdle()
{
	const LLViewerInventoryCategory* pRlvRoot = RlvInventory::instance().getSharedRoot();
	const LLViewerInventoryCategory* pFolder = (mComplete.size()) ? gInventory.getCategory(mComplete[0]) : NULL;
	if ( (pRlvRoot) && (pFolder) )
	{
		std::string strName = pFolder->getName();
		if (strName.find(RLV_PUTINV_PREFIX) == 0)
		{
			LLInventoryModel::update_list_t update;
			LLInventoryModel::LLCategoryUpdate updOldParent(pFolder->getParentUUID(), -1);
			update.push_back(updOldParent);
			LLInventoryModel::LLCategoryUpdate updNewParent(pRlvRoot->getUUID(), 1);
			update.push_back(updNewParent);
			gInventory.accountForUpdate(update);

			LLPointer<LLViewerInventoryCategory> pNewFolder = new LLViewerInventoryCategory(pFolder);
			pNewFolder->setParent(pRlvRoot->getUUID());
			pNewFolder->updateParentOnServer(FALSE);
			pNewFolder->rename(strName.erase(0, strName.find(RLV_FOLDER_PREFIX_PUTINV)));
			pNewFolder->updateServer(FALSE);
			gInventory.updateCategory(pNewFolder);

			gInventory.notifyObservers();
		}
	}
	delete this;
}

// ============================================================================
// RlvWearableItemCollector
//

// Checked: 2010-09-25 (RLVa-1.2.1c) | Added: RLVa-1.2.1c
RlvWearableItemCollector::RlvWearableItemCollector(const LLInventoryCategory* pFolder, RlvForceWear::EWearAction eAction, RlvForceWear::EWearFlags eFlags)
	: m_idFolder(pFolder->getUUID()), m_eWearAction(eAction), m_eWearFlags(eFlags), 
	  m_strWearAddPrefix(RlvSettings::getWearAddPrefix()), m_strWearReplacePrefix(RlvSettings::getWearReplacePrefix())
{
	m_Wearable.push_back(m_idFolder);

	// Wear prefixes can't/shouldn't start with '.'
	if ( (m_strWearAddPrefix.length() > 1) && (RLV_FOLDER_PREFIX_HIDDEN == m_strWearAddPrefix[0]) )
		m_strWearAddPrefix.clear();
	if ( (m_strWearReplacePrefix.length() > 1) && (RLV_FOLDER_PREFIX_HIDDEN == m_strWearReplacePrefix[0]) )
		m_strWearReplacePrefix.clear();

	// If there's a prefix on the "root" folder then it will override what we were passed in the constructor
	m_eWearAction = getWearActionNormal(pFolder);
	m_WearActionMap.insert(std::pair<LLUUID, RlvForceWear::EWearAction>(m_idFolder, m_eWearAction));
}

// Checked: 2010-09-25 (RLVa-1.2.1c) | Added: RLVa-1.2.1c
RlvForceWear::EWearAction RlvWearableItemCollector::getWearActionNormal(const LLInventoryCategory* pFolder)
{
	RLV_ASSERT_DBG(!RlvInventory::isFoldedFolder(pFolder, false));
	if ( (RlvForceWear::ACTION_WEAR_REPLACE == m_eWearAction) && (!m_strWearAddPrefix.empty()) &&
		 (boost::algorithm::starts_with(pFolder->getName(), m_strWearAddPrefix)))
	{
		return RlvForceWear::ACTION_WEAR_ADD;
	}
	else if ( (RlvForceWear::ACTION_WEAR_ADD == m_eWearAction) && (!m_strWearReplacePrefix.empty()) &&
		      (boost::algorithm::starts_with(pFolder->getName(), m_strWearReplacePrefix)) )
	{
		return RlvForceWear::ACTION_WEAR_REPLACE;
	}
	return (pFolder->getUUID() != m_idFolder) ? getWearAction(pFolder->getParentUUID()) : m_eWearAction;
}

// Checked: 2010-04-07 (RLVa-1.2.0d) | Added: RLVa-0.2.0e
const LLUUID& RlvWearableItemCollector::getFoldedParent(const LLUUID& idFolder) const
{
	std::map<LLUUID, LLUUID>::const_iterator itFolder = m_FoldingMap.end(), itCur = m_FoldingMap.find(idFolder);
	while (itCur != m_FoldingMap.end())
	{
		itFolder = itCur;
		itCur = m_FoldingMap.find(itFolder->second);
	}
	return (m_FoldingMap.end() == itFolder) ? idFolder : itFolder->second;
}

// Checked: 2010-09-25 (RLVa-1.2.1c) | Added: RLVa-1.2.1c
RlvForceWear::EWearAction RlvWearableItemCollector::getWearAction(const LLUUID& idFolder) const
{
	LLUUID idCurFolder(idFolder); std::map<LLUUID, RlvForceWear::EWearAction>::const_iterator itCurFolder;
	while ((itCurFolder = m_WearActionMap.find(idCurFolder)) == m_WearActionMap.end())
	{
		const LLViewerInventoryCategory* pFolder = gInventory.getCategory(idCurFolder);
		if ((!pFolder) || (gInventory.getRootFolderID() == pFolder->getParentUUID()))
			break;
		idCurFolder = pFolder->getParentUUID();
	}
	return (itCurFolder != m_WearActionMap.end()) ? itCurFolder->second : m_eWearAction;
}

// Checked: 2010-09-30 (RLVa-1.2.1d) | Modified: RLVa-1.2.1d
bool RlvWearableItemCollector::onCollectFolder(const LLInventoryCategory* pFolder)
{
	// We treat folder links differently since they won't exist in the wearable folder list yet and their ancestry isn't relevant
	bool fLinkedFolder = isLinkedFolder(pFolder->getUUID());
	if ( (!fLinkedFolder) && (m_Wearable.end() == std::find(m_Wearable.begin(), m_Wearable.end(), pFolder->getParentUUID())) )
		return false;															// Not a linked folder or the child of a wearable folder

	const std::string& strFolder = pFolder->getName();
	if (strFolder.empty())														// Shouldn't happen but does... naughty Lindens
		return false;

	bool fAttach = RlvForceWear::isWearAction(m_eWearAction);
	bool fMatchAll = (!fLinkedFolder) && (m_eWearFlags & RlvForceWear::FLAG_MATCHALL);

	if ( (!fLinkedFolder) && (RlvInventory::isFoldedFolder(pFolder, false)) )	// Check for folder that should get folded under its parent
	{
		if ( (!fAttach) || (1 == RlvInventory::getDirectDescendentsItemCount(pFolder, LLAssetType::AT_OBJECT)) )
		{																		// When attaching there should only be 1 attachment in it
			m_Folded.push_front(pFolder->getUUID());
			m_FoldingMap.insert(std::pair<LLUUID, LLUUID>(pFolder->getUUID(), pFolder->getParentUUID()));
		}
	}
	else if ( (RLV_FOLDER_PREFIX_HIDDEN != strFolder[0]) && 					// Collect from any non-hidden child folder for *all
		      ( (fMatchAll) || (fLinkedFolder) ) && 							// ... and collect from linked folders
			  (!isLinkedFolder(pFolder->getParentUUID())) )						// ... but never from non-folded linked folder descendents
	{
		#ifdef RLV_EXPERIMENTAL_COMPOSITEFOLDERS
		if ( (!RlvSettings::getEnableComposites()) ||							// ... if we're not checking composite folders
			 (!gRlvHandler.isCompositeFolder(pFolder)) ||						// ... or if it's not a composite folder
		     ((m_fAttach) && (gRlvHandler.canWearComposite(pFolder))) ||		// ... or if we're attaching and can attach it OR
			 (!m_fAttach) && (gRlvHandler.canTakeOffComposite(pFolder)) )		// ... or if we're detaching and can detach it
		#endif // RLV_EXPERIMENTAL_COMPOSITEFOLDERS
		{
			m_Wearable.push_front(pFolder->getUUID());
			m_WearActionMap.insert(std::pair<LLUUID, RlvForceWear::EWearAction>(pFolder->getUUID(), getWearActionNormal(pFolder)));
		}
		return (!fLinkedFolder) && (pFolder->getParentUUID() == m_idFolder);	// Convenience for @getinvworn
	}
	#ifdef RLV_EXPERIMENTAL_COMPOSITEFOLDERS
	else if ( (RlvSettings::getEnableComposites()) &&
			  (RLV_FOLDER_PREFIX_HIDDEN == strFolder[0]) &&						// Hidden folder that's a... 
			  (gRlvHandler.isCompositeFolder(pFolder)) &&						// ... composite folder which we...
		      ( ((m_fAttach) && (gRlvHandler.canWearComposite(pFolder))) ||		// ... are attaching and can attach OR
			    (!m_fAttach) && (gRlvHandler.canTakeOffComposite(pFolder)) ) )	// ... are detaching and can detach
	{
		m_Wearable.push_front(pFolder->getUUID());
		m_FoldingMap.insert(std::pair<LLUUID, LLUUID>(pFolder->getUUID(), idParent));
	}
	#endif // RLV_EXPERIMENTAL_COMPOSITEFOLDERS

	return false;
}

// Checked: 2010-09-30 (RLVa-1.2.1d) | Modified: RLVa-1.2.1d
bool RlvWearableItemCollector::onCollectItem(const LLInventoryItem* pItem)
{
	bool fAttach = RlvForceWear::isWearAction(m_eWearAction);

	if ( (!fAttach) && (!RlvForceWear::isStrippable(pItem)) )							// Don't process "nostrip" items on detach
		return false;

	const LLUUID& idParent = pItem->getParentUUID(); bool fRet = false;
	switch (pItem->getType())
	{
		case LLAssetType::AT_BODYPART:
			if (!fAttach)
				break;																	// Don't process body parts on detach
		case LLAssetType::AT_CLOTHING:
			fRet = ( (m_Wearable.end() != std::find(m_Wearable.begin(), m_Wearable.end(), idParent)) ||
					 ( (fAttach) && (m_Folded.end() != std::find(m_Folded.begin(), m_Folded.end(), idParent)) &&
					   (RlvForceWear::isStrippable(pItem)) ) );
			break;
		case LLAssetType::AT_OBJECT:
			fRet = ( (m_Wearable.end() != std::find(m_Wearable.begin(), m_Wearable.end(), idParent)) || 
				     (m_Folded.end() != std::find(m_Folded.begin(), m_Folded.end(), idParent)) ) &&
				   ( (!fAttach) || (RlvAttachPtLookup::hasAttachPointName(pItem)) || (RlvSettings::getEnableSharedWear()) );
			break;
		#ifdef RLV_EXTENSION_FORCEWEAR_GESTURES
		case LLAssetType::AT_GESTURE:
			fRet = (m_Wearable.end() != std::find(m_Wearable.begin(), m_Wearable.end(), idParent));
			break;
		#endif // RLV_EXTENSION_FORCEWEAR_GESTURES
		#ifdef RLV_EXTENSION_FORCEWEAR_FOLDERLINKS
		case LLAssetType::AT_CATEGORY:
			if (LLAssetType::AT_LINK_FOLDER == pItem->getActualType())
			{
				const LLUUID& idLinkedFolder = pItem->getLinkedUUID();
				LLViewerInventoryCategory* pLinkedFolder = gInventory.getCategory(idLinkedFolder);
				// Link can't point to an outfit folder, or start a second level of indirection, or have the base folder as an ancestor
				if ( (pLinkedFolder) && (LLFolderType::FT_OUTFIT != pLinkedFolder->getPreferredType()) &&
					 (gInventory.isObjectDescendentOf(pItem->getUUID(), m_idFolder)) && 
					 (!gInventory.isObjectDescendentOf(idLinkedFolder, m_idFolder)) )
				{
					// Fold the contents of the linked folder under the folder the link is a child of
					m_FoldingMap.insert(std::pair<LLUUID, LLUUID>(idLinkedFolder, pItem->getParentUUID()));
					m_Linked.push_front(idLinkedFolder);
				}
			}
			break;
		#endif // RLV_EXTENSION_FORCEWEAR_FOLDERLINKS
		default:
			break;
	}
	return fRet;
}

// Checked: 2010-03-20 (RLVa-1.2.0a) | Modified: RLVa-0.2.0d
bool RlvWearableItemCollector::operator()(LLInventoryCategory* pFolder, LLInventoryItem* pItem)
{
	// NOTE: this is used for more than was originally intended so only modify if you're sure it won't break something obscure
	return (pFolder) ? onCollectFolder(pFolder) : ( (pItem) ? onCollectItem(pItem) : false );
}

// ============================================================================
