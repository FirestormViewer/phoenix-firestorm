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
#include "llagentwearables.h"
#include "llappearancemgr.h"
#include "llgesturemgr.h"
#include "llnotifications.h"
#include "llnotificationsutil.h"
#include "llviewerobject.h"
#include "llviewerobjectlist.h"
#include "llwlparammanager.h"

#include "rlvhelper.h"
#include "rlvhandler.h"
#include "rlvinventory.h"

// ============================================================================
// RlvCommmand
//

RlvCommand::RlvBhvrTable RlvCommand::m_BhvrMap;

// Checked: 2009-12-27 (RLVa-1.1.0k) | Modified: RLVa-1.1.0k
RlvCommand::RlvCommand(const LLUUID& idObj, const std::string& strCommand)
	: m_idObj(idObj), m_eBehaviour(RLV_BHVR_UNKNOWN), m_fStrict(false), m_eParamType(RLV_TYPE_UNKNOWN)
{
	if ((m_fValid = parseCommand(strCommand, m_strBehaviour, m_strOption, m_strParam)))
	{
		S32 nTemp = 0;
		if ( ("n" == m_strParam) || ("add" == m_strParam) )
			m_eParamType = RLV_TYPE_ADD;
		else if ( ("y" == m_strParam) || ("rem" == m_strParam) )
			m_eParamType = RLV_TYPE_REMOVE;
		else if (m_strBehaviour == "clear")						// clear is the odd one out so just make it its own type
			m_eParamType = RLV_TYPE_CLEAR;
		else if ("force" == m_strParam)
			m_eParamType = RLV_TYPE_FORCE;
		else if (LLStringUtil::convertToS32(m_strParam, nTemp))	// Assume it's a reply command if we can convert <param> to an S32
			m_eParamType = RLV_TYPE_REPLY;
		else
		{
			m_eParamType = RLV_TYPE_UNKNOWN;
			m_fValid = false;
		}
	}

	if (!m_fValid)
	{
		m_strBehaviour = m_strOption = m_strParam = "";
		return;
	}

	m_eBehaviour = getBehaviourFromString(m_strBehaviour, &m_fStrict);
}


bool RlvCommand::parseCommand(const std::string& strCommand, std::string& strBehaviour, std::string& strOption, std::string& strParam)
{
	// (See behaviour notes for the command parsing truth table)

	// Format: <behaviour>[:<option>]=<param>
	int idxParam  = strCommand.find('=');
	int idxOption = (idxParam > 0) ? strCommand.find(':') : -1;
	if (idxOption > idxParam - 1)
		idxOption = -1;

	// If <behaviour> is missing it's always an improperly formatted command
	if ( (0 == idxOption) || (0 == idxParam) )
		return false;

	strBehaviour = strCommand.substr(0, (-1 != idxOption) ? idxOption : idxParam);
	strOption = strParam = "";

	// If <param> is missing it's an improperly formatted command
	if ( (-1 == idxParam) || ((int)strCommand.length() - 1 == idxParam) )
	{
		// Unless "<behaviour> == "clear" AND (idxOption == 0)" 
		// OR <behaviour> == "clear" AND (idxParam != 0) [see table above]
		if ( ("clear" == strBehaviour) && ( (!idxOption) || (idxParam) ) )
			return true;
		return false;
	}

	if ( (-1 != idxOption) && (idxOption + 1 != idxParam) )
		strOption = strCommand.substr(idxOption + 1, idxParam - idxOption - 1);
	strParam = strCommand.substr(idxParam + 1);

	return true;
}

// Checked: 2009-12-05 (RLVa-1.1.0h) | Added: RLVa-1.1.0h
ERlvBehaviour RlvCommand::getBehaviourFromString(const std::string& strBhvr, bool* pfStrict /*=NULL*/)
{
	std::string::size_type idxStrict = strBhvr.find("_sec");
	bool fStrict = (std::string::npos != idxStrict) && (idxStrict + 4 == strBhvr.length());
	if (pfStrict)
		*pfStrict = fStrict;

	RLV_ASSERT(m_BhvrMap.size() > 0);
	RlvBhvrTable::const_iterator itBhvr = m_BhvrMap.find( (!fStrict) ? strBhvr : strBhvr.substr(0, idxStrict));
	if ( (itBhvr != m_BhvrMap.end()) && ((!fStrict) || (hasStrictVariant(itBhvr->second))) )
		return itBhvr->second;
	return RLV_BHVR_UNKNOWN;
}

// Checked: 2010-02-27 (RLVa-1.2.0a) | Modified: RLVa-1.1.0h
void RlvCommand::initLookupTable()
{
	static bool fInitialized = false;
	if (!fInitialized)
	{
		// NOTE: keep this matched with the enumeration at all times
		std::string arBehaviours[RLV_BHVR_COUNT] =
			{
				"detach", "attach", "addattach", "remattach", "addoutfit", "remoutfit", "emote", "sendchat", "recvchat", "recvemote",
				"redirchat", "rediremote", "chatwhisper", "chatnormal", "chatshout", "sendchannel", "sendim", "recvim", "permissive",
				"notify", "showinv", "showminimap", "showworldmap", "showloc", "shownames", "showhovertext", "showhovertexthud",
				"showhovertextworld", "showhovertextall", "tplm", "tploc", "tplure", "viewnote", "viewscript", "viewtexture", 
				"acceptpermission", "accepttp", "allowidle", "edit", "rez", "fartouch", "interact", "touch", "touchattach", "touchhud", 
				"touchworld", "fly", "unsit", "sit", "sittp", "standtp", "setdebug", "setenv", "detachme", "detachthis", "detachall", 
				"detachallthis", "attachthis", "attachall", "attachallthis", "tpto", "version", "versionnew", "versionnum", 
				"getattach", "getattachnames", 	"getaddattachnames", "getremattachnames", "getoutfit", "getoutfitnames", 
				"getaddoutfitnames", "getremoutfitnames", "findfolder", "findfolders", "getpath", "getpathnew", "getinv", "getinvworn", 
				"getsitid", "getstatus", "getstatusall"
			};

		for (int idxBvhr = 0; idxBvhr < RLV_BHVR_COUNT; idxBvhr++)
			m_BhvrMap.insert(std::pair<std::string, ERlvBehaviour>(arBehaviours[idxBvhr], (ERlvBehaviour)idxBvhr));

		fInitialized = true;
	}
}

// ============================================================================
// RlvCommandOption
//

// Checked: 2010-09-28 (RLVa-1.2.1c) | Added: RLVa-1.2.1c
RlvCommandOptionGeneric::RlvCommandOptionGeneric(const std::string& strOption)
{
	LLWearableType::EType wtType(LLWearableType::WT_INVALID); LLUUID idOption; ERlvAttachGroupType eAttachGroup(RLV_ATTACHGROUP_INVALID);
	LLViewerJointAttachment* pAttachPt = NULL; LLViewerInventoryCategory* pFolder = NULL;

	if (!(m_fEmpty = strOption.empty()))														// <option> could be an empty string
	{
		if ((wtType = LLWearableType::typeNameToType(strOption)) != LLWearableType::WT_INVALID)
			m_varOption = wtType;																// ... or specify a clothing layer
		else if ((pAttachPt = RlvAttachPtLookup::getAttachPoint(strOption)) != NULL)
			m_varOption = pAttachPt;															// ... or specify an attachment point
		else if (idOption.set(strOption))
			m_varOption = idOption;																// ... or specify an UUID
		else if ((pFolder = RlvInventory::instance().getSharedFolder(strOption)) != NULL)
			m_varOption = pFolder;																// ... or specify a shared folder path
		else if ((eAttachGroup = rlvAttachGroupFromString(strOption)) != RLV_ATTACHGROUP_INVALID)
			m_varOption = eAttachGroup;															// ... or specify an attachment point group
		else
			m_varOption = strOption;															// ... or it might just be a string
	}
}

// Checked: 2010-09-28 (RLVa-1.2.1c) | Added: RLVa-1.2.1c
RlvCommandOptionGetPath::RlvCommandOptionGetPath(const RlvCommand& rlvCmd)
	: m_fValid(true)							// Assume the option will be a valid one until we find out otherwise
{
	// @getpath[:<option>]=<channel> => <option> is transformed to a list of inventory item UUIDs to get the path of

	RlvCommandOptionGeneric rlvCmdOption(rlvCmd.getOption());
	if (rlvCmdOption.isWearableType())			// <option> can be a clothing layer
	{
		LLWearableType::EType wtType = rlvCmdOption.getWearableType();
		for (S32 idxWearable = 0, cntWearable = gAgentWearables.getWearableCount(wtType); idxWearable < cntWearable; idxWearable++)
			m_idItems.push_back(gAgentWearables.getWearableItemID(wtType, idxWearable));
	}
	else if (rlvCmdOption.isAttachmentPoint())	// ... or it can specify an attachment point
	{
		const LLViewerJointAttachment* pAttachPt = rlvCmdOption.getAttachmentPoint();
		for (LLViewerJointAttachment::attachedobjs_vec_t::const_iterator itAttachObj = pAttachPt->mAttachedObjects.begin();
				itAttachObj != pAttachPt->mAttachedObjects.end(); ++itAttachObj)
		{
			m_idItems.push_back((*itAttachObj)->getAttachmentItemID());
		}
	}
	else if (rlvCmdOption.isEmpty())			// ... or it can be empty (in which case we act on the object that issued the command)
	{
		const LLViewerObject* pObj = gObjectList.findObject(rlvCmd.getObjectID());
		if ( (pObj) || (pObj->isAttachment()) )
			m_idItems.push_back(pObj->getAttachmentItemID());
	}
	else										// ... but anything else isn't a valid option
	{
		m_fValid = false;
	}
}

// =========================================================================
// RlvObject
//

RlvObject::RlvObject(const LLUUID& idObj) : m_UUID(idObj), m_nLookupMisses(0)
{ 
	LLViewerObject* pObj = gObjectList.findObject(idObj);
	m_fLookup = (NULL != pObj);
	m_idxAttachPt = (pObj) ? ATTACHMENT_ID_FROM_STATE(pObj->getState()) : 0;
	m_idRoot = (pObj) ? pObj->getRootEdit()->getID() : LLUUID::null;
}

bool RlvObject::addCommand(const RlvCommand& rlvCmd)
{
	RLV_ASSERT(RLV_TYPE_ADD == rlvCmd.getParamType());

	// Don't add duplicate commands for this object (ie @detach=n followed by another @detach=n later on)
	for (rlv_command_list_t::iterator itCmd = m_Commands.begin(); itCmd != m_Commands.end(); ++itCmd)
	{
		if ( (itCmd->getBehaviour() == rlvCmd.getBehaviour()) && (itCmd->getOption() == rlvCmd.getOption()) && 
			 (itCmd->isStrict() == rlvCmd.isStrict() ) )
		{
			return false;
		}
	}

	// Now that we know it's not a duplicate, add it to the end of the list
	m_Commands.push_back(rlvCmd);

	return true;
}

bool RlvObject::removeCommand(const RlvCommand& rlvCmd)
{
	RLV_ASSERT(RLV_TYPE_REMOVE == rlvCmd.getParamType());

	for (rlv_command_list_t::iterator itCmd = m_Commands.begin(); itCmd != m_Commands.end(); ++itCmd)
	{
		//if (*itCmd == rlvCmd) <- commands will never be equal since one is an add and the other is a remove *rolls eyes*
		if ( (itCmd->getBehaviour() == rlvCmd.getBehaviour()) && (itCmd->getOption() == rlvCmd.getOption()) && 
			 (itCmd->isStrict() == rlvCmd.isStrict() ) )
		{
			m_Commands.erase(itCmd);
			return true;
		}
	}
	return false;	// Command was never added so nothing to remove now
}

bool RlvObject::hasBehaviour(ERlvBehaviour eBehaviour, bool fStrictOnly) const
{
	for (rlv_command_list_t::const_iterator itCmd = m_Commands.begin(); itCmd != m_Commands.end(); ++itCmd)
		if ( (itCmd->getBehaviourType() == eBehaviour) && (itCmd->getOption().empty()) && ((!fStrictOnly) || (itCmd->isStrict())) )
			return true;
	return false;
}

bool RlvObject::hasBehaviour(ERlvBehaviour eBehaviour, const std::string& strOption, bool fStrictOnly) const
{
	for (rlv_command_list_t::const_iterator itCmd = m_Commands.begin(); itCmd != m_Commands.end(); ++itCmd)
		if ( (itCmd->getBehaviourType() == eBehaviour) && (itCmd->getOption() == strOption) && ((!fStrictOnly) || (itCmd->isStrict())) )
			return true;
	return false;
}

// Checked: 2009-11-27 (RLVa-1.1.0f) | Modified: RLVa-1.1.0f
std::string RlvObject::getStatusString(const std::string& strMatch) const
{
	std::string strStatus, strCmd;

	for (rlv_command_list_t::const_iterator itCmd = m_Commands.begin(); itCmd != m_Commands.end(); ++itCmd)
	{
		strCmd = itCmd->asString();
		if ( (strMatch.empty()) || (std::string::npos != strCmd.find(strMatch)) )
		{
			strStatus.push_back('/');
			strStatus += strCmd;
		}
	}

	return strStatus;
}

// ============================================================================
// RlvForceWear
//

// Checked: 2010-04-05 (RLVa-1.2.0d) | Modified: RLVa-1.2.0d
bool RlvForceWear::isWearingItem(const LLInventoryItem* pItem)
{
	if (pItem)
	{
		switch (pItem->getActualType())
		{
			case LLAssetType::AT_BODYPART:
			case LLAssetType::AT_CLOTHING:
				return gAgentWearables.isWearingItem(pItem->getUUID());
			case LLAssetType::AT_OBJECT:
				return (isAgentAvatarValid()) && (gAgentAvatarp->isWearingAttachment(pItem->getUUID()));
			case LLAssetType::AT_GESTURE:
				return LLGestureMgr::instance().isGestureActive(pItem->getUUID());
			case LLAssetType::AT_LINK:
				return isWearingItem(gInventory.getItem(pItem->getLinkedUUID()));
			default:
				break;
		}
	}
	return false;
}

// Checked: 2010-03-21 (RLVa-1.2.0a) | Modified: RLVa-1.2.0a
void RlvForceWear::forceFolder(const LLViewerInventoryCategory* pFolder, eWearAction eAction, eWearFlags eFlags)
{
	// [See LLWearableBridge::wearOnAvatar(): don't wear anything until initial wearables are loaded, can destroy clothing items]
	if (!gAgentWearables.areWearablesLoaded())
	{
		LLNotificationsUtil::add("CanNotChangeAppearanceUntilLoaded");
		return;
	}
	if (!isAgentAvatarValid())
		return;

	// Grab a list of all the items we'll be wearing/attaching
	LLInventoryModel::cat_array_t folders; LLInventoryModel::item_array_t items;
	RlvWearableItemCollector f(pFolder->getUUID(), (ACTION_ATTACH == eAction), (FLAG_MATCHALL & eFlags));
	gInventory.collectDescendentsIf(pFolder->getUUID(), folders, items, FALSE, f);

	for (S32 idxItem = 0, cntItem = items.count(); idxItem < cntItem; idxItem++)
	{
		LLViewerInventoryItem* pRlvItem = items.get(idxItem);
		LLViewerInventoryItem* pItem = (LLAssetType::AT_LINK == pRlvItem->getActualType()) ? pRlvItem->getLinkedItem() : pRlvItem;

		// If it's wearable it should be worn on detach
//		if ( (ACTION_DETACH == eAction) && (isWearableItem(pItem)) && (!isWearingItem(pItem)) )
//			continue;

		//  NOTES: * if there are composite items then RlvWearableItemCollector made sure they can be worn (or taken off depending)
		//         * some scripts issue @remattach=force,attach:worn-items=force so we need to attach items even if they're currently worn
		switch (pItem->getType())
		{
			case LLAssetType::AT_BODYPART:
				RLV_ASSERT(ACTION_ATTACH == eAction);	// RlvWearableItemCollector shouldn't be supplying us with body parts on detach
			case LLAssetType::AT_CLOTHING:
				if (ACTION_ATTACH == eAction)
				{
					// The check for whether we're replacing a currently worn composite item happens in onWearableArrived()
					if (!isAddWearable(pItem))
						addWearable(pRlvItem);
				}
				else
				{
					const LLWearable* pWearable = gAgentWearables.getWearableFromItemID(pItem->getUUID());
					if ( (pWearable) && (isForceRemovable(pWearable, false)) )
						remWearable(pWearable);
				}
				break;

			case LLAssetType::AT_OBJECT:
				if (ACTION_ATTACH == eAction)
				{
					if ( (gRlvAttachmentLocks.canAttach(pRlvItem)) || (RlvSettings::getEnableSharedWear()) )
					{
						if (!isAddAttachment(pRlvItem))
						{
							#ifdef RLV_EXPERIMENTAL_COMPOSITEFOLDERS
							// We still need to check whether we're about to replace a currently worn composite item
							// (which we're not if we're just reattaching an attachment we're already wearing)
							LLViewerInventoryCategory* pCompositeFolder = NULL;
							if ( (pAttachPt->getObject()) && (RlvSettings::getEnableComposites()) && 
								 (pAttachPt->getItemID() != pItem->getUUID()) &&
								 (gRlvHandler.getCompositeInfo(pAttachPt->getItemID(), NULL, &pCompositeFolder)) )
							{
								// If we can't take off the composite folder this item would replace then don't allow it to get attached
								if (gRlvHandler.canTakeOffComposite(pCompositeFolder))
								{
									forceFolder(pCompositeFolder, ACTION_DETACH, FLAG_DEFAULT);
									addAttachment(pRlvItem);
								}
							}
							else
							#endif // RLV_EXPERIMENTAL_COMPOSITEFOLDERS
							{
								addAttachment(pRlvItem);
							}
						}
					}
				}
				else
				{
					const LLViewerObject* pAttachObj = gAgentAvatarp->getWornAttachment(pItem->getUUID());
					if ( (pAttachObj) && (isForceDetachable(pAttachObj, false)) )
						remAttachment(pAttachObj);
				}
				break;

			#ifdef RLV_EXTENSION_FORCEWEAR_GESTURES
			case LLAssetType::AT_GESTURE:
				if (ACTION_ATTACH == eAction)
				{
					if (std::find_if(m_addGestures.begin(), m_addGestures.end(), RlvPredIsEqualOrLinkedItem(pRlvItem)) == m_addGestures.end())
						m_addGestures.push_back(pRlvItem);
				}
				else
				{
					if (std::find_if(m_remGestures.begin(), m_remGestures.end(), RlvPredIsEqualOrLinkedItem(pRlvItem)) == m_remGestures.end())
						m_remGestures.push_back(pRlvItem);
				}
				break;
			#endif // RLV_EXTENSION_FORCEWEAR_GESTURES

			default:
				break;
		}
	}
}

// Checked: 2010-03-19 (RLVa-1.2.0a) | Modified: RLVa-1.2.0a
bool RlvForceWear::isForceDetachable(const LLViewerObject* pAttachObj, bool fCheckComposite /*=true*/, const LLUUID& idExcept /*=LLUUID::null*/)
{
	// Attachment can be detached by an RLV command if:
	//   - it's not "remove locked" by anything (or anything except the object specified by pExceptObj)
	//   - it's strippable
	//   - composite folders are disabled *or* it isn't part of a composite folder that has at least one item locked
	#ifdef RLV_EXPERIMENTAL_COMPOSITEFOLDERS
	LLViewerInventoryCategory* pFolder = NULL;
	#endif // RLV_EXPERIMENTAL_COMPOSITEFOLDERS
	return 
	  (
	    (pAttachObj) && (pAttachObj->isAttachment())
		&& ( (idExcept.isNull()) ? (!gRlvAttachmentLocks.isLockedAttachment(pAttachObj))
								 : (!gRlvAttachmentLocks.isLockedAttachmentExcept(pAttachObj, idExcept)) )
		#ifdef RLV_EXTENSION_FLAG_NOSTRIP
		&& (isStrippable(pAttachObj->getAttachmentItemID()))
		#endif // RLV_EXTENSION_FLAG_NOSTRIP
		#ifdef RLV_EXPERIMENTAL_COMPOSITEFOLDERS
		&& ( (!fCheckComposite) || (!RlvSettings::getEnableComposites()) || 
		     (!gRlvHandler.getCompositeInfo(pAttachPt->getItemID(), NULL, &pFolder)) || (gRlvHandler.canTakeOffComposite(pFolder)) )
		#endif // RLV_EXPERIMENTAL_COMPOSITEFOLDERS
	  );
}

// Checked: 2010-03-19 (RLVa-1.2.0a) | Added: RLVa-1.2.0a
bool RlvForceWear::isForceDetachable(const LLViewerJointAttachment* pAttachPt, bool fCheckComposite /*=true*/, const LLUUID& idExcept /*=LLUUID::null*/)
{
	// Attachment point can be detached by an RLV command if there's at least one attachment that can be removed
	for (LLViewerJointAttachment::attachedobjs_vec_t::const_iterator itAttachObj = pAttachPt->mAttachedObjects.begin();
			itAttachObj != pAttachPt->mAttachedObjects.end(); ++itAttachObj)
	{
		if (isForceDetachable(*itAttachObj, fCheckComposite, idExcept))
			return true;
	}
	return false;
}

// Checked: 2010-03-19 (RLVa-1.2.0a) | Added: RLVa-1.1.0i
void RlvForceWear::forceDetach(const LLViewerObject* pAttachObj)
{
	// Sanity check - no need to process duplicate removes
	if ( (!pAttachObj) || (isRemAttachment(pAttachObj)) )
		return;

	if (isForceDetachable(pAttachObj))
	{
		#ifdef RLV_EXPERIMENTAL_COMPOSITEFOLDERS
		LLViewerInventoryCategory* pFolder = NULL;
		if ( (RlvSettings::getEnableComposites()) && 
			 (gRlvHandler.getCompositeInfo(pAttachPt->getItemID(), NULL, &pFolder)) )
		{
			// Attachment belongs to a composite folder so detach the entire folder (if we can take it off)
			if (gRlvHandler.canTakeOffComposite(pFolder))
				forceFolder(pFolder, ACTION_DETACH, FLAG_DEFAULT);
		}
		else
		#endif // RLV_EXPERIMENTAL_COMPOSITEFOLDERS
		{
			remAttachment(pAttachObj);
		}
	}
}

// Checked: 2010-03-19 (RLVa-1.2.0a) | Added: RLVa-1.2.0a
void RlvForceWear::forceDetach(const LLViewerJointAttachment* pAttachPt)
{
	for (LLViewerJointAttachment::attachedobjs_vec_t::const_iterator itAttachObj = pAttachPt->mAttachedObjects.begin();
			itAttachObj != pAttachPt->mAttachedObjects.end(); ++itAttachObj)
	{
		forceDetach(*itAttachObj);
	}
}

// Checked: 2010-03-19 (RLVa-1.2.0c) | Modified: RLVa-1.2.0a
bool RlvForceWear::isForceRemovable(const LLWearable* pWearable, bool fCheckComposite /*=true*/, const LLUUID& idExcept /*=LLUUID::null*/)
{
	// Wearable can be removed by an RLV command if:
	//   - its asset type is AT_CLOTHING
	//   - it's not "remove locked" by anything (or anything except the object specified by idExcept)
	//   - it's strippable
	//   - composite folders are disabled *or* it isn't part of a composite folder that has at least one item locked
	#ifdef RLV_EXPERIMENTAL_COMPOSITEFOLDERS
	LLViewerInventoryCategory* pFolder = NULL;
	#endif // RLV_EXPERIMENTAL_COMPOSITEFOLDERS
	return 
	  (
		(pWearable) && (LLAssetType::AT_CLOTHING == pWearable->getAssetType()) 
		&& ( (idExcept.isNull()) ? !gRlvWearableLocks.isLockedWearable(pWearable)
		                         : !gRlvWearableLocks.isLockedWearableExcept(pWearable, idExcept) )
		#ifdef RLV_EXTENSION_FLAG_NOSTRIP
		&& (isStrippable(pWearable->getItemID()))
		#endif // RLV_EXTENSION_FLAG_NOSTRIP
		#ifdef RLV_EXPERIMENTAL_COMPOSITEFOLDERS
		&& ( (!fCheckComposite) || (!RlvSettings::getEnableComposites()) || 
		     (!gRlvHandler.getCompositeInfo(pWearable->getItemID(), NULL, &pFolder)) || (gRlvHandler.canTakeOffComposite(pFolder)) )
		#endif // RLV_EXPERIMENTAL_COMPOSITEFOLDERS
	  );
}

// Checked: 2010-03-19 (RLVa-1.2.0a) | Added: RLVa-1.2.0a
bool RlvForceWear::isForceRemovable(LLWearableType::EType wtType, bool fCheckComposite /*=true*/, const LLUUID& idExcept /*=LLUUID::null*/)
{
	// Wearable type can be removed by an RLV command if there's at least one currently worn wearable that can be removed
	for (U32 idxWearable = 0, cntWearable = gAgentWearables.getWearableCount(wtType); idxWearable < cntWearable; idxWearable++)
		if (isForceRemovable(gAgentWearables.getWearable(wtType, idxWearable), fCheckComposite, idExcept))
			return true;
	return false;
}

// Checked: 2010-03-19 (RLVa-1.2.0a) | Modified: RLVa-1.2.0a
void RlvForceWear::forceRemove(const LLWearable* pWearable)
{
	// Sanity check - no need to process duplicate removes
	if ( (!pWearable) || (isRemWearable(pWearable)) )
		return;

	if (isForceRemovable(pWearable))
	{
		#ifdef RLV_EXPERIMENTAL_COMPOSITEFOLDERS
		LLViewerInventoryCategory* pFolder = NULL;
		if ( (RlvSettings::getEnableComposites()) && 
			 (gRlvHandler.getCompositeInfo(gAgent.getWearableItem(wtType), NULL, &pFolder)) )
		{
			// Wearable belongs to a composite folder so detach the entire folder (if we can take it off)
			if (gRlvHandler.canTakeOffComposite(pFolder))
				forceFolder(pFolder, ACTION_DETACH, FLAG_DEFAULT);
		}
		else
		#endif // RLV_EXPERIMENTAL_COMPOSITEFOLDERS
		{
			remWearable(pWearable);
		}
	}
}

// Checked: 2010-03-19 (RLVa-1.2.0a) | Added: RLVa-1.2.0a
void RlvForceWear::forceRemove(LLWearableType::EType wtType)
{
	for (U32 idxWearable = 0, cntWearable = gAgentWearables.getWearableCount(wtType); idxWearable < cntWearable; idxWearable++)
		forceRemove(gAgentWearables.getWearable(wtType, idxWearable));
}

#ifdef RLV_EXTENSION_FLAG_NOSTRIP
// Checked: 2010-03-19 (RLVa-1.2.0c) | Modified: RLVa-1.2.0a
bool RlvForceWear::isStrippable(const LLInventoryItem* pItem)
{
	// An item is exempt from @detach or @remoutfit if:
	//   - its name contains "nostrip" (anywhere in the name)
	//   - its parent folder contains "nostrip" (anywhere in the name)
	if (pItem)
	{
		// If the item is an inventory link then we first examine its target before examining the link itself (and never its name)
		if (LLAssetType::AT_LINK == pItem->getActualType())
		{
			if (!isStrippable(pItem->getLinkedUUID()))
				return false;
		}
		else
		{
			if (std::string::npos != pItem->getName().find(RLV_FOLDER_FLAG_NOSTRIP))
				return false;
		}

		LLViewerInventoryCategory* pFolder = gInventory.getCategory(pItem->getParentUUID());
		while ( (pFolder) && (gInventory.getRootFolderID() != pFolder->getParentUUID()) )
		{
			if (std::string::npos != pFolder->getName().find(RLV_FOLDER_FLAG_NOSTRIP))
				return false;
			// If the item's parent is a folded folder then we need to check its parent as well
			pFolder = 
				(RlvInventory::isFoldedFolder(pFolder, true)) ? gInventory.getCategory(pFolder->getParentUUID()) : NULL;
		}
	}
	return true;
}
#endif // RLV_EXTENSION_FLAG_NOSTRIP

// Checked: 2010-04-24 (RLVa-1.2.0f) | Added: RLVa-1.2.0f
void RlvForceWear::addAttachment(const LLViewerInventoryItem* pItem)
{
	// Remove it from 'm_remAttachments' if it's queued for detaching
	const LLViewerObject* pAttachObj = (isAgentAvatarValid()) ? gAgentAvatarp->getWornAttachment(pItem->getLinkedUUID()) : NULL;
	if ( (pAttachObj) && (isRemAttachment(pAttachObj)) )
		m_remAttachments.erase(std::remove(m_remAttachments.begin(), m_remAttachments.end(), pAttachObj), m_remAttachments.end());

	// Add it to 'm_addAttachments' if it's not already there
	if (!isAddAttachment(pItem))
		m_addAttachments.push_back((LLViewerInventoryItem*)pItem);
}

// Checked: 2010-04-24 (RLVa-1.2.0f) | Added: RLVa-1.2.0f
void RlvForceWear::remAttachment(const LLViewerObject* pAttachObj)
{
	// Remove it from 'm_addAttachments' if it's queued for attaching
	const LLViewerInventoryItem* pItem = (pAttachObj->isAttachment()) ? gInventory.getItem(pAttachObj->getAttachmentItemID()) : NULL;
	if ( (pItem) && (isAddAttachment(pItem)) )
	{
		m_addAttachments.erase(
			std::remove_if(m_addAttachments.begin(), m_addAttachments.end(), RlvPredIsEqualOrLinkedItem(pItem)), m_addAttachments.end());
	}

	// Add it to 'm_remAttachments' if it's not already there
	if (!isRemAttachment(pAttachObj))
		m_remAttachments.push_back(pAttachObj);
}

// Checked: 2010-04-24 (RLVa-1.2.0f) | Added: RLVa-1.2.0f
void RlvForceWear::addWearable(const LLViewerInventoryItem* pItem)
{
	// Remove it from 'm_remWearables' if it's queued for removal
	const LLWearable* pWearable = gAgentWearables.getWearableFromItemID(pItem->getLinkedUUID());;
	if ( (pWearable) && (isRemWearable(pWearable)) )
		m_remWearables.erase(std::remove(m_remWearables.begin(), m_remWearables.end(), pWearable), m_remWearables.end());

	// Add it to 'm_addWearables' if it's not already there
	if (!isAddWearable(pItem))
		m_addWearables.push_back((LLViewerInventoryItem*)pItem);
}

// Checked: 2010-04-24 (RLVa-1.2.0f) | Added: RLVa-1.2.0f
void RlvForceWear::remWearable(const LLWearable* pWearable)
{
	// Remove it from 'm_addWearables' if it's queued for wearing
	const LLViewerInventoryItem* pItem = gInventory.getItem(pWearable->getItemID());
	if ( (pItem) && (isAddWearable(pItem)) )
	{
		m_addWearables.erase(
			std::remove_if(m_addWearables.begin(), m_addWearables.end(), RlvPredIsEqualOrLinkedItem(pItem)), m_addWearables.end());
	}

	// Add it to 'm_remWearables' if it's not already there
	if (!isRemWearable(pWearable))
		m_remWearables.push_back(pWearable);
}

// Checked: 2010-03-22 (RLVa-1.2.0a) | Modified: RLVa-1.2.0a
void RlvForceWear::done()
{
	// Sanity check - don't go through all the motions below only to find out there's nothing to actually do
	if ( (m_remWearables.empty()) && (m_remAttachments.empty()) && (m_remGestures.empty()) &&
		 (m_addWearables.empty()) && (m_addAttachments.empty()) && (m_addGestures.empty()) )
	{
		return;
	}

	LLAppearanceMgr* pAppearanceMgr = LLAppearanceMgr::getInstance();
	bool fUpdateAppearance = false;
	
	//
	// Process removals
	//

	// Wearables
	if (m_remWearables.size())
	{
		for (std::list<const LLWearable*>::const_iterator itWearable = m_remWearables.begin(); itWearable != m_remWearables.end(); ++itWearable)
			pAppearanceMgr->removeCOFItemLinks((*itWearable)->getItemID(), false);
		m_remWearables.clear();
		fUpdateAppearance = true;
	}

	// Gestures
	if (m_remGestures.size())
	{
		for (S32 idxItem = 0, cntItem = m_remGestures.count(); idxItem < cntItem; idxItem++)
			pAppearanceMgr->removeCOFItemLinks(m_remGestures.get(idxItem)->getUUID(), false);
		m_remGestures.clear();
		fUpdateAppearance = true;
	}

	// Attachments
	if (m_remAttachments.size())
	{
		// Don't bother with COF if all we're doing is detaching some attachments (keeps people from rebaking on every @remattach=force)
		fUpdateAppearance = (fUpdateAppearance) || (!m_addWearables.empty()) || (!m_addAttachments.empty()) || (!m_addGestures.empty());
		if (!fUpdateAppearance)
		{
			gMessageSystem->newMessage("ObjectDetach");
			gMessageSystem->nextBlockFast(_PREHASH_AgentData);
			gMessageSystem->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
			gMessageSystem->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());

			for (std::list<const LLViewerObject*>::const_iterator itAttachObj = m_remAttachments.begin(); 
					itAttachObj != m_remAttachments.end(); ++itAttachObj)
			{
				gMessageSystem->nextBlockFast(_PREHASH_ObjectData);
				gMessageSystem->addU32Fast(_PREHASH_ObjectLocalID, (*itAttachObj)->getLocalID());
			}

			gMessageSystem->sendReliable(gAgent.getRegionHost());
		}

		for (std::list<const LLViewerObject*>::const_iterator itAttachObj = m_remAttachments.begin(); 
				itAttachObj != m_remAttachments.end(); ++itAttachObj)
		{
			pAppearanceMgr->removeCOFItemLinks((*itAttachObj)->getAttachmentItemID(), false);
		}

		m_remAttachments.clear();
	}

	//
	// Process additions
	//

	// Wearables need to be split into AT_BODYPART and AT_CLOTHING for COF
	LLInventoryModel::item_array_t addBodyParts, addClothing;
	for (S32 idxItem = 0, cntItem = m_addWearables.count(); idxItem < cntItem; idxItem++)
	{
		LLViewerInventoryItem* pItem = m_addWearables.get(idxItem);
		if (LLAssetType::AT_BODYPART == pItem->getType())
			addBodyParts.push_back(pItem);
		else
			addClothing.push_back(pItem);
	}

#ifdef RLV_WORKAROUND_REZMULTIPLEATTACH
	// Until SVC-5383 gets fixed we need to handle adding attachments ourselves instead of relying on COF
	if (m_addAttachments.size())
	{
		for (S32 idxItem = 0, cntItem = m_addAttachments.count(); idxItem < cntItem; idxItem++)
		{
			const LLViewerInventoryItem* pItem = m_addAttachments.get(idxItem);

			S32 idxAttachPt = RlvAttachPtLookup::getAttachPointIndex(pItem, true);
			if (0 != idxAttachPt)
			{
				LLSD payload;
				payload["item_id"] = pItem->getLinkedUUID();
				payload["attachment_point"] = idxAttachPt;
				LLNotifications::instance().forceResponse(LLNotification::Params("ReplaceAttachment").payload(payload), 0/*YES*/);
			}
		}

		m_addAttachments.clear();
	}
#endif // RLV_WORKAROUND_REZMULTIPLEATTACH

	// If there are additions we need to call LLAppearanceManager::updateCOF(), otherwise LLAppearanceManager::updateAppearanceFromCOF()
	if ( (!m_addWearables.empty()) || (!m_addAttachments.empty()) || (!m_addGestures.empty()) )
	{
#ifndef RLV_WORKAROUND_SHAREDINVREPLACE
		pAppearanceMgr->updateCOF(addBodyParts, addClothing, m_addAttachments, m_addGestures, true);
#else
		pAppearanceMgr->updateCOF(addBodyParts, addClothing, m_addAttachments, m_addGestures, true, 1);
#endif // RLV_WORKAROUND_SHAREDINVREPLACE

		m_addWearables.clear();
		m_addAttachments.clear();
		m_addGestures.clear();
	}
	else if (fUpdateAppearance)
	{
		pAppearanceMgr->updateAppearanceFromCOF();
	}

	// Since RlvForceWear is a singleton now we want to be sure there aren't any leftovers
	RLV_ASSERT( (m_remWearables.empty()) && (m_remAttachments.empty()) && (m_remGestures.empty()) );
	RLV_ASSERT( (m_addWearables.empty()) && (m_addAttachments.empty()) && (m_addGestures.empty()) );
}

// Checked: 2010-02-17 (RLVa-1.1.0o) | Modified: RLVa-1.1.0o
/*
void RlvForceWear::onWearableArrived(LLWearable* pWearable, void* pParam)
{
	#ifdef RLV_EXPERIMENTAL_COMPOSITEFOLDERS
	// If this wearable will end up replacing a currently worn one that belongs to a composite folder then we need to detach the composite
	LLViewerInventoryCategory* pFolder = NULL;
	if ( (RlvSettings::getEnableComposites()) && (pWearable) && (gAgent.getWearable(pWearable->getType())) )
	{
		// If we're just rewearing the same item we're already wearing then we're not replacing a composite folder
		LLWearableHoldingPattern* pWearData = (LLWearableHoldingPattern*)pParam; LLUUID idItem;
		for (LLWearableHoldingPattern::found_list_t::const_iterator itWearable = pWearData->mFoundList.begin();
				itWearable != pWearData->mFoundList.end(); ++itWearable)
		{
			LLFoundData* pFound = *itWearable;
			if (pWearable->getID() == pFound->mAssetID)
			{
				idItem = pFound->mItemID;
				break;
			}
		}
		if ( (idItem.notNull()) && (idItem != gAgent.getWearableItem(pWearable->getType())) && 
			 (gRlvHandler.getCompositeInfo(gAgent.getWearableItem(pWearable->getType()), NULL, &pFolder)) )
		{
			RlvForceWear rlvWear;
			rlvWear.forceFolder(pFolder, ACTION_DETACH, FLAG_DEFAULT);
			rlvWear.done();
		}
	}
	#endif // RLV_EXPERIMENTAL_COMPOSITEFOLDERS

	wear_inventory_category_on_avatar_loop(pWearable, pParam);
}
*/

// ============================================================================
// RlvBehaviourNotifyObserver
//

// Checked: 2010-03-03 (RLVa-1.2.0a) | Added: RLVa-1.2.0a
RlvBehaviourNotifyHandler::RlvBehaviourNotifyHandler()
{
	// NOTE: the reason we use rlv_command_signal_t instead of the better-suited rlv_behaviour_signal_t is because
	//       RLV will notify scripts about "invalid" commands so we need to as well
	m_ConnCommand = gRlvHandler.setCommandCallback(boost::bind(&RlvBehaviourNotifyHandler::onCommand, this, _1, _2, _3, _4));
}

// Checked: 2010-03-03 (RLVa-1.2.0a) | Modified: RLVa-1.2.0a
void RlvBehaviourNotifyHandler::onCommand(const LLUUID& idRlvObj, const RlvCommand& rlvCmd, ERlvCmdRet eRet, bool fInternal)
{
	if (fInternal)
		return;

	std::string strCmd = rlvCmd.asString(), strNotify; ERlvParamType eCmdType = rlvCmd.getParamType();
	if ( (RLV_TYPE_ADD == eCmdType) || (RLV_TYPE_REMOVE == eCmdType) )
		strNotify = llformat("/%s=%s", strCmd.c_str(), rlvCmd.getParam().c_str());
	else if (RLV_TYPE_CLEAR == eCmdType)
		strNotify = llformat("/%s", strCmd.c_str());
	else
		return;

	for (std::multimap<LLUUID, notifyData>::const_iterator itNotify = m_Notifications.begin(); 
			itNotify != m_Notifications.end(); ++itNotify)
	{
		if ( (itNotify->second.strFilter.empty()) || (std::string::npos != strCmd.find(itNotify->second.strFilter)) )
			RlvUtil::sendChatReply(itNotify->second.nChannel, strNotify);
	}
}

// ============================================================================
// RlvWLSnapshot
//

// Checked: 2010-03-18 (RLVa-1.2.0e) | Added: RLVa-0.2.0h
void RlvWLSnapshot::restoreSnapshot(const RlvWLSnapshot* pWLSnapshot)
{
	LLWLParamManager* pWLParams = LLWLParamManager::instance();
	if ( (pWLSnapshot) && (pWLParams) )
	{
		pWLParams->mAnimator.mIsRunning = pWLSnapshot->fIsRunning;
		pWLParams->mAnimator.mUseLindenTime = pWLSnapshot->fUseLindenTime;
		pWLParams->mCurParams = pWLSnapshot->WLParams;
		pWLParams->propagateParameters();
	}
}

// Checked: 2010-03-18 (RLVa-1.2.0e) | Modified: RLVa-1.2.0e
RlvWLSnapshot* RlvWLSnapshot::takeSnapshot()
{
	RlvWLSnapshot* pWLSnapshot = NULL;
	LLWLParamManager* pWLParams = LLWLParamManager::instance();
	if (pWLParams)
	{
		pWLSnapshot = new RlvWLSnapshot();
		pWLSnapshot->fIsRunning = pWLParams->mAnimator.mIsRunning;
		pWLSnapshot->fUseLindenTime = pWLParams->mAnimator.mUseLindenTime;
		pWLSnapshot->WLParams = pWLParams->mCurParams;
	}
	return pWLSnapshot;
}

// =========================================================================
// Various helper classes/timers/functors
//

// Checked: 2010-03-13 (RLVa-1.2.0a) | Modified: RLVa-1.2.0a
BOOL RlvGCTimer::tick()
{
	bool fContinue = gRlvHandler.onGC();
	if (!fContinue)
		gRlvHandler.m_pGCTimer = NULL;
	return !fContinue;
}

// ============================================================================
// Various helper functions
//

// Checked: 2010-04-11 (RLVa-1.2.0b) | Modified: RLVa-0.2.0g
bool rlvCanDeleteOrReturn()
{
	bool fIsAllowed = true;

	if (gRlvHandler.hasBehaviour(RLV_BHVR_REZ))
	{
		// We'll allow if none of the prims are owned by the avie or group owned
		LLObjectSelectionHandle handleSel = LLSelectMgr::getInstance()->getSelection();
		RlvSelectIsOwnedByOrGroupOwned f(gAgent.getID());
		if ( (handleSel.notNull()) && ((0 == handleSel->getRootObjectCount()) || (NULL != handleSel->getFirstRootNode(&f, FALSE))) )
			fIsAllowed = false;
	}
	
	if ( (gRlvHandler.hasBehaviour(RLV_BHVR_UNSIT)) && (isAgentAvatarValid()) )
	{
		// We'll allow if the avie isn't sitting on any of the selected objects
		LLObjectSelectionHandle handleSel = LLSelectMgr::getInstance()->getSelection();
		RlvSelectIsSittingOn f(gAgentAvatarp->getRoot());
		if ( (handleSel.notNull()) && (handleSel->getFirstRootNode(&f, TRUE)) )
			fIsAllowed = false;
	}

	return fIsAllowed;
}

// ============================================================================
// Attachment group helper functions
//

// Has to match the order of ERlvAttachGroupType
const std::string cstrAttachGroups[RLV_ATTACHGROUP_COUNT] = { "head", "torso", "arms", "legs", "hud" };

// Checked: 2009-10-19 (RLVa-1.1.0e) | Added: RLVa-1.1.0e
ERlvAttachGroupType rlvAttachGroupFromIndex(S32 idxGroup)
{
	switch (idxGroup)
	{
		case 0: // Right Hand
		case 1: // Right Arm
		case 3: // Left Arm
		case 4: // Left Hand
			return RLV_ATTACHGROUP_ARMS;
		case 2: // Head
			return RLV_ATTACHGROUP_HEAD;
		case 5: // Left Leg
		case 7: // Right Leg
			return RLV_ATTACHGROUP_LEGS;
		case 6: // Torso
			return RLV_ATTACHGROUP_TORSO;
		case 8: // HUD
			return RLV_ATTACHGROUP_HUD;
		default:
			return RLV_ATTACHGROUP_INVALID;
	}
}

// Checked: 2009-10-19 (RLVa-1.1.0e) | Added: RLVa-1.1.0e
ERlvAttachGroupType rlvAttachGroupFromString(const std::string& strGroup)
{
	for (int idx = 0; idx < RLV_ATTACHGROUP_COUNT; idx++)
		if (cstrAttachGroups[idx] == strGroup)
			return (ERlvAttachGroupType)idx;
	return RLV_ATTACHGROUP_INVALID;
}

// =========================================================================
// String helper functions
//

// Checked: 2009-07-04 (RLVa-1.0.0a)
void rlvStringReplace(std::string& strText, std::string strFrom, const std::string& strTo)
{
	if (strFrom.empty())
		return;

	size_t lenFrom = strFrom.length();
	size_t lenTo = strTo.length();

	std::string strTemp(strText);
	LLStringUtil::toLower(strTemp);
	LLStringUtil::toLower(strFrom);

	std::string::size_type idxCur, idxStart = 0, idxOffset = 0;
	while ( (idxCur = strTemp.find(strFrom, idxStart)) != std::string::npos)
	{
		strText.replace(idxCur + idxOffset, lenFrom, strTo);
		idxStart = idxCur + lenFrom;
		idxOffset += lenTo - lenFrom;
	}
}

// Checked: 2009-07-29 (RLVa-1.0.1b) | Added: RLVa-1.0.1b
std::string rlvGetFirstParenthesisedText(const std::string& strText, std::string::size_type* pidxMatch /*=NULL*/)
{
	if (pidxMatch)
		*pidxMatch = std::string::npos;	// Assume we won't find anything

	std::string::size_type idxIt, idxStart; int cntLevel = 1;
	if ((idxStart = strText.find_first_of('(')) == std::string::npos)
		return std::string();

	const char* pstrText = strText.c_str(); idxIt = idxStart;
	while ( (cntLevel > 0) && (idxIt < strText.length()) )
	{
		if ('(' == pstrText[++idxIt])
			cntLevel++;
		else if (')' == pstrText[idxIt])
			cntLevel--;
	}

	if (idxIt < strText.length())
	{
		if (pidxMatch)
			*pidxMatch = idxStart;	// Return the character index of the starting '('
		return strText.substr(idxStart + 1, idxIt - idxStart - 1);
	}
	return std::string();
}

// Checked: 2009-07-29 (RLVa-1.0.1b) | Added: RLVa-1.0.1b
std::string rlvGetLastParenthesisedText(const std::string& strText, std::string::size_type* pidxStart /*=NULL*/)
{
	if (pidxStart)
		*pidxStart = std::string::npos;	// Assume we won't find anything

	// Extracts the last - matched - parenthesised text from the input string
	std::string::size_type idxIt, idxEnd; int cntLevel = 1;
	if ((idxEnd = strText.find_last_of(')')) == std::string::npos)
		return std::string();

	const char* pstrText = strText.c_str(); idxIt = idxEnd;
	while ( (cntLevel > 0) && (idxIt >= 0) )
	{
		if (')' == pstrText[--idxIt])
			cntLevel++;
		else if ('(' == pstrText[idxIt])
			cntLevel--;
	}

	if ( (idxIt >= 0) && (idxIt < strText.length()) )	// NOTE: allow for std::string::size_type to be signed or unsigned
	{
		if (pidxStart)
			*pidxStart = idxIt;		// Return the character index of the starting '('
		return strText.substr(idxIt + 1, idxEnd - idxIt - 1);
	}
	return std::string();
}

// =========================================================================
