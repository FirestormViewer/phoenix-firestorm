/** 
 *
 * Copyright (c) 2011-2012, Kitty Barnett
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
#include "llassetuploadresponders.h"
#include "llcheckboxctrl.h"
#include "lldiriterator.h"
#include "llfloaterreg.h"
#include "llfolderview.h"
#include "llinventoryfunctions.h"
#include "llinventorymodel.h"
#include "llinventorypanel.h"
#include "llscrolllistctrl.h"
#include "llviewerassettype.h"
#include "llviewerinventory.h"
#include "llviewerregion.h"

#include "llfloaterscriptrecover.h"

// ============================================================================
// LLFloaterScriptRecover
//

LLFloaterScriptRecover::LLFloaterScriptRecover(const LLSD& sdKey)
	: LLFloater(sdKey)
{
}

void LLFloaterScriptRecover::onOpen(const LLSD& sdKey)
{
	LLScrollListCtrl* pListCtrl = findChild<LLScrollListCtrl>("script_list");

	LLSD sdBhvrRow; LLSD& sdBhvrColumns = sdBhvrRow["columns"];
	sdBhvrColumns[0] = LLSD().with("column", "script_check").with("type", "checkbox");
	sdBhvrColumns[1] = LLSD().with("column", "script_name").with("type", "text");

	pListCtrl->clearRows();
	for (LLSD::array_const_iterator itFile = sdKey["files"].beginArray(), endFile = sdKey["files"].endArray(); 
			itFile != endFile;  ++itFile)
	{
		const LLSD& sdFile = *itFile;

		sdBhvrRow["value"] = sdFile;
		sdBhvrColumns[0]["value"] = true;
		sdBhvrColumns[1]["value"] = sdFile["name"];

		pListCtrl->addElement(sdBhvrRow, ADD_BOTTOM);
	}
}

BOOL LLFloaterScriptRecover::postBuild()
{
	findChild<LLUICtrl>("recover_btn")->setCommitCallback(boost::bind(&LLFloaterScriptRecover::onBtnRecover, this));
	findChild<LLUICtrl>("cancel_btn")->setCommitCallback(boost::bind(&LLFloaterScriptRecover::onBtnCancel, this));

	return TRUE;
}

void LLFloaterScriptRecover::onBtnCancel()
{
	LLScrollListCtrl* pListCtrl = findChild<LLScrollListCtrl>("script_list");

	// Delete all listed files
	std::vector<LLScrollListItem*> items = pListCtrl->getAllData();
	for (std::vector<LLScrollListItem*>::const_iterator itItem = items.begin(); itItem != items.end(); ++itItem)
	{
		LLFile::remove((*itItem)->getValue().asString());
	}

	closeFloater();
}

void LLFloaterScriptRecover::onBtnRecover()
{
	LLScrollListCtrl* pListCtrl = findChild<LLScrollListCtrl>("script_list");

	// Recover all selected, delete any unselected
	std::vector<LLScrollListItem*> items = pListCtrl->getAllData(); LLSD sdFiles;
	for (std::vector<LLScrollListItem*>::const_iterator itItem = items.begin(); itItem != items.end(); ++itItem)
	{
		LLScrollListCheck* pCheckColumn = dynamic_cast<LLScrollListCheck*>((*itItem)->getColumn(0));
		if (!pCheckColumn)
			continue;

		const LLSD sdFile = (*itItem)->getValue();
		if (pCheckColumn->getCheckBox()->getValue().asBoolean())
			sdFiles.append(sdFile);
		else
			LLFile::remove(sdFile["path"]);
	}

	if (!sdFiles.emptyArray())
		new LLScriptRecoverQueue(sdFiles);

	closeFloater();
}

// ============================================================================
// LLCreateRecoverScriptCallback
//

class LLCreateRecoverScriptCallback : public LLInventoryCallback
{
public:
	LLCreateRecoverScriptCallback(LLScriptRecoverQueue* pRecoverQueue)
		: LLInventoryCallback(), mRecoverQueue(pRecoverQueue)
	{
	}

	void fire(const LLUUID& idItem)
	{
		mRecoverQueue->onCreateScript(idItem);
	}

protected:
	LLScriptRecoverQueue* mRecoverQueue;
};

// ============================================================================
// LLScriptRecoverQueue
//

// static
void LLScriptRecoverQueue::recoverIfNeeded()
{
	const std::string strTempPath = LLFile::tmpdir();
	LLSD sdData, &sdFiles = sdData["files"];

	LLDirIterator itFiles(strTempPath, "*.lslbackup"); std::string strFilename;
	while (itFiles.next(strFilename))
	{
		// Build a friendly name for the file
		std::string strName = gDirUtilp->getBaseFileName(strFilename, true);
		std::string::size_type offset = strName.find_last_of("-");
		if ( (std::string::npos != offset) && (offset != 0) && (offset == strName.length() - 9))
			strName.erase(strName.length() - 9);

		LLStringUtil::trim(strName);
		if (0 == strName.length())
			strName = "(Unknown script)";

		sdFiles.append(LLSD().with("path", strTempPath + strFilename).with("name", strName));
	}

	if (sdFiles.size())
		LLFloaterReg::showInstance("script_recover", sdData);
}

LLScriptRecoverQueue::LLScriptRecoverQueue(const LLSD& sdFiles)
{
	for (LLSD::array_const_iterator itFile = sdFiles.beginArray(), endFile = sdFiles.endArray(); itFile != endFile;  ++itFile)
	{
		const LLSD& sdFile = *itFile;
		if (LLFile::isfile(sdFile["path"]))
			m_FileQueue.insert(std::pair<std::string, LLSD>(sdFile["path"], sdFile));
	}
	recoverNext();
}

bool LLScriptRecoverQueue::recoverNext()
{
	/**
	 * Steps:
	 *  (1) create a script inventory item under Lost and Found
	 *  (2) once we have the item's UUID we can upload the script
	 *  (3) once the script is uploaded we move on to the next item
	 */
	const LLUUID idFNF = gInventory.findCategoryUUIDForType(LLFolderType::FT_LOST_AND_FOUND);

	// Sanity check - if the associated UUID is non-null then this file is already being processed
	filename_queue_t::const_iterator itFile = m_FileQueue.begin();
	while ( (itFile != m_FileQueue.end()) && (itFile->second.has("item")) && (itFile->second["item"].asUUID().notNull()) )
		++itFile;

	if (m_FileQueue.end() == itFile) 
	{
		LLInventoryPanel* pInvPanel = LLInventoryPanel::getActiveInventoryPanel(TRUE);
		if (pInvPanel)
		{
			LLFolderViewFolder* pFVF = dynamic_cast<LLFolderViewFolder*>(pInvPanel->getRootFolder()->getItemByID(idFNF));
			if (pFVF)
			{
				pFVF->setOpenArrangeRecursively(TRUE, LLFolderViewFolder::RECURSE_UP);
				pInvPanel->setSelection(idFNF, TRUE);
			}
		}

		delete this;
		return false;
	}

	std::string strItemDescr;
	LLViewerAssetType::generateDescriptionFor(LLAssetType::AT_LSL_TEXT, strItemDescr);

	create_inventory_item(gAgent.getID(), gAgent.getSessionID(), idFNF, LLTransactionID::tnull, 
	                      itFile->second["name"].asString(), strItemDescr, LLAssetType::AT_LSL_TEXT, LLInventoryType::IT_LSL,
	                      NOT_WEARABLE, PERM_MOVE | PERM_TRANSFER, new LLCreateRecoverScriptCallback(this));
	return true;
}

void LLScriptRecoverQueue::onCreateScript(const LLUUID& idItem)
{
	const LLViewerInventoryItem* pItem = gInventory.getItem(idItem);
	if (!pItem)
	{
		// TODO: error handling
		return;
	}

	std::string strFileName;
	for (filename_queue_t::iterator itFile = m_FileQueue.begin(); itFile != m_FileQueue.end(); ++itFile)
	{
		if (itFile->second["name"].asString() != pItem->getName())
			continue;
		strFileName = itFile->second["path"].asString();
		itFile->second["item"] = idItem;
		break;
	}

	LLSD sdBody;
	sdBody["item_id"] = idItem;
	sdBody["target"] = "lsl2";

	std::string strCapsUrl = gAgent.getRegion()->getCapability("UpdateScriptAgent");
	LLHTTPClient::post(strCapsUrl, sdBody, 
	                   new LLUpdateAgentInventoryResponder(sdBody, strFileName, LLAssetType::AT_LSL_TEXT, 
	                                                       boost::bind(&LLScriptRecoverQueue::onSavedScript, this, _1, _2, _3),
	                                                       boost::bind(&LLScriptRecoverQueue::onUploadError, this, _1)));
}

void LLScriptRecoverQueue::onSavedScript(const LLUUID& idItem, const LLSD&, bool fSuccess)
{
	const LLViewerInventoryItem* pItem = gInventory.getItem(idItem);
	if (pItem)
	{
		filename_queue_t::iterator itFile = m_FileQueue.begin();
		while ( (itFile != m_FileQueue.end()) && ((!itFile->second.has("item")) || (itFile->second["item"].asUUID() != idItem)) )
			++itFile;
		if (itFile != m_FileQueue.end())
		{
			LLFile::remove(itFile->first);
			m_FileQueue.erase(itFile);
		}
	}
	recoverNext();
}

bool LLScriptRecoverQueue::onUploadError(const std::string& strFilename)
{
	// Skip over the file when there's an error, we can try again on the next relog
	filename_queue_t::iterator itFile = m_FileQueue.find(strFilename);
	if (itFile != m_FileQueue.end())
	{
		LLViewerInventoryItem* pItem = gInventory.getItem(itFile->second["item"]);
		if (pItem)
			gInventory.changeItemParent(pItem, gInventory.findCategoryUUIDForType(LLFolderType::FT_TRASH), FALSE);
		m_FileQueue.erase(itFile);
	}
	recoverNext();
	return false;
}

// ============================================================================
