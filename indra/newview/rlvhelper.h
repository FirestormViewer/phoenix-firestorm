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

#ifndef RLV_HELPER_H
#define RLV_HELPER_H

#include "lleventtimer.h"
#include "llwlparamset.h"

#include "rlvdefines.h"
#include "rlvcommon.h"

#ifdef LL_WINDOWS
	#pragma warning (push)
	#pragma warning (disable : 4702) // warning C4702: unreachable code
#endif
#include <boost/variant.hpp>
#ifdef LL_WINDOWS
	#pragma warning (pop)
#endif

// ============================================================================
// RlvCommand
//

class RlvCommand
{
public:
	explicit RlvCommand(const LLUUID& idObj, const std::string& strCommand);

	/*
	 * Member functions
	 */
public:
	std::string        asString() const;
	const std::string& getBehaviour() const		{ return m_strBehaviour; }
	ERlvBehaviour      getBehaviourType() const	{ return m_eBehaviour; }
	const LLUUID&      getObjectID() const		{ return m_idObj; }
	const std::string& getOption() const		{ return m_strOption; }
	const std::string& getParam() const			{ return m_strParam; }
	ERlvParamType      getParamType() const		{ return m_eParamType; }
	bool               isStrict() const			{ return m_fStrict; }
	bool               isValid() const			{ return m_fValid; }

	static ERlvBehaviour      getBehaviourFromString(const std::string& strBhvr, bool* pfStrict = NULL);
	static const std::string& getStringFromBehaviour(ERlvBehaviour eBhvr);
	static bool               hasStrictVariant(ERlvBehaviour eBhvr);

	static void initLookupTable();
protected:
	static bool parseCommand(const std::string& strCommand, std::string& strBehaviour, std::string& strOption,  std::string& strParam);

	/*
	 * Operators
	 */
public:
	bool operator ==(const RlvCommand&) const;

	/*
	 * Member variables
	 */
protected:
	bool          m_fValid;
	LLUUID        m_idObj;
	std::string   m_strBehaviour;
	ERlvBehaviour m_eBehaviour;
	bool          m_fStrict;
	std::string   m_strOption;
	std::string   m_strParam;
	ERlvParamType m_eParamType;

	typedef std::map<std::string, ERlvBehaviour> RlvBhvrTable;
	static RlvBhvrTable m_BhvrMap;

	friend class RlvHandler;
};
typedef std::list<RlvCommand> rlv_command_list_t;

// ============================================================================
// RlvCommandOption (and derived classed)
//

class RlvCommandOption
{
protected:
	RlvCommandOption() {}
public:
	virtual ~RlvCommandOption() {}

public:
	virtual bool isAttachmentPoint() const		{ return false; }
	virtual bool isAttachmentPointGroup() const	{ return false; }
	virtual bool isEmpty() const = 0;
	virtual bool isSharedFolder() const			{ return false; }
	virtual bool isString() const				{ return false; }
	virtual bool isUUID() const					{ return false; }
	virtual bool isValid() const = 0;
	virtual bool isWearableType() const			{ return false; }

	virtual LLViewerJointAttachment*   getAttachmentPoint() const		{ return NULL; }
	virtual ERlvAttachGroupType        getAttachmentPointGroup() const	{ return RLV_ATTACHGROUP_INVALID; }
	virtual LLViewerInventoryCategory* getSharedFolder() const			{ return NULL; }
	virtual const std::string&         getString() const				{ return LLStringUtil::null; }
	virtual const LLUUID&              getUUID() const					{ return LLUUID::null; }
	virtual LLWearableType::EType      getWearableType() const			{ return LLWearableType::WT_INVALID; }
};

class RlvCommandOptionGeneric : public RlvCommandOption
{
public:
	explicit RlvCommandOptionGeneric(const std::string& strOption);
	RlvCommandOptionGeneric(LLViewerJointAttachment* pAttachPt) : m_fEmpty(false)	{ m_varOption = pAttachPt; }
	RlvCommandOptionGeneric(LLViewerInventoryCategory* pFolder) : m_fEmpty(false)	{ m_varOption = pFolder; }
	RlvCommandOptionGeneric(const LLUUID& idOption) : m_fEmpty(false)				{ m_varOption = idOption; }
	RlvCommandOptionGeneric(LLWearableType::EType wtType) : m_fEmpty(false)			{ m_varOption = wtType; }
	/*virtual*/ ~RlvCommandOptionGeneric() {}

public:
	/*virtual*/ bool isAttachmentPoint() const		{ return (!isEmpty()) && (typeid(LLViewerJointAttachment*) == m_varOption.type()); }
	/*virtual*/ bool isAttachmentPointGroup() const	{ return (!isEmpty()) && (typeid(ERlvAttachGroupType) == m_varOption.type()); }
	/*virtual*/ bool isEmpty() const				{ return m_fEmpty; }
	/*virtual*/ bool isSharedFolder() const			{ return (!isEmpty()) && (typeid(LLViewerInventoryCategory*) == m_varOption.type()); }
	/*virtual*/ bool isString() const				{ return (!isEmpty()) && (typeid(std::string) == m_varOption.type()); }
	/*virtual*/ bool isUUID() const					{ return (!isEmpty()) && (typeid(LLUUID) == m_varOption.type()); }
	/*virtual*/ bool isValid() const				{ return true; } // This doesn't really have any significance for the generic class
	/*virtual*/ bool isWearableType() const			{ return (!isEmpty()) && (typeid(LLWearableType::EType) == m_varOption.type()); }

	/*virtual*/ LLViewerJointAttachment*   getAttachmentPoint() const
		{ return (isAttachmentPoint()) ? boost::get<LLViewerJointAttachment*>(m_varOption) : RlvCommandOption::getAttachmentPoint(); }
	/*virtual*/ ERlvAttachGroupType        getAttachmentPointGroup() const
		{ return (isAttachmentPointGroup()) ? boost::get<ERlvAttachGroupType>(m_varOption) : RlvCommandOption::getAttachmentPointGroup(); }
	/*virtual*/ LLViewerInventoryCategory* getSharedFolder() const
		{ return (isSharedFolder()) ? boost::get<LLViewerInventoryCategory*>(m_varOption) : RlvCommandOption::getSharedFolder(); }
	/*virtual*/ const std::string&         getString() const
		{ return (isString()) ? boost::get<std::string>(m_varOption) : RlvCommandOption::getString(); }
	/*virtual*/ const LLUUID&              getUUID() const
		{ return (isUUID()) ? boost::get<LLUUID>(m_varOption) : RlvCommandOption::getUUID(); }
	/*virtual*/ LLWearableType::EType      getWearableType() const
		{ return (isWearableType()) ? boost::get<LLWearableType::EType>(m_varOption) : RlvCommandOption::getWearableType(); }

protected:
	bool m_fEmpty;
	boost::variant<LLViewerJointAttachment*, ERlvAttachGroupType, LLViewerInventoryCategory*, std::string, LLUUID, LLWearableType::EType> m_varOption;
};

class RlvCommandOptionGetPath : public RlvCommandOption
{
public:
	RlvCommandOptionGetPath(const RlvCommand& rlvCmd);
	/*virtual*/ ~RlvCommandOptionGetPath() {}

	/*virtual*/ bool  isEmpty() const	 { return m_idItems.empty(); }
	/*virtual*/ bool  isValid() const	 { return m_fValid; }
	const uuid_vec_t& getItemIDs() const { return m_idItems; }

protected:
	bool       m_fValid;
	uuid_vec_t m_idItems;
};

// ============================================================================
// RlvObject
//

class RlvObject
{
public:
	RlvObject(const LLUUID& idObj);

	/*
	 * Member functions
	 */
public:
	bool addCommand(const RlvCommand& rlvCmd);
	bool removeCommand(const RlvCommand& rlvCmd);

	std::string getStatusString(const std::string& strMatch) const;
	bool        hasBehaviour(ERlvBehaviour eBehaviour, bool fStrictOnly) const;
	bool        hasBehaviour(ERlvBehaviour eBehaviour, const std::string& strOption, bool fStrictOnly) const;

	const rlv_command_list_t* getCommandList() const { return &m_Commands; }

	/*
	 * Member variables
	 */
protected:
	LLUUID             m_UUID;				// The object's UUID
	S32                m_idxAttachPt;		// The object's attachment point (or 0 if it's not an attachment)
	LLUUID             m_idRoot;			// The UUID of the object's root (may or may not be different from m_UUID)
	bool               m_fLookup;			// TRUE if the object existed in gObjectList at one point in time
	S16                m_nLookupMisses;		// Count of unsuccessful lookups in gObjectList by the GC
	rlv_command_list_t m_Commands;			// List of behaviours held by this object (in the order they were received)

	friend class RlvHandler;
};

// ============================================================================
// RlvForceWear
//

class RlvForceWear : public LLSingleton<RlvForceWear>
{
protected:
	RlvForceWear() {}

public:
	// Folders
	enum EWearAction { ACTION_WEAR_REPLACE, ACTION_WEAR_ADD, ACTION_REMOVE };
	enum EWearFlags { FLAG_NONE = 0x00, FLAG_MATCHALL = 0x01, FLAG_DEFAULT = FLAG_NONE };
	void forceFolder(const LLViewerInventoryCategory* pFolder, EWearAction eAction, EWearFlags eFlags);

	// Generic
	static bool isWearAction(EWearAction eAction) { return (ACTION_WEAR_REPLACE == eAction) || (ACTION_WEAR_ADD == eAction); }
	static bool isWearableItem(const LLInventoryItem* pItem);
	static bool isWearingItem(const LLInventoryItem* pItem);

	// Nostrip
	static bool isStrippable(const LLUUID& idItem) { return isStrippable(gInventory.getItem(idItem)); }
	static bool isStrippable(const LLInventoryItem* pItem);

	// Attachments
	static bool isForceDetachable(const LLViewerObject* pAttachObj, bool fCheckComposite = true, const LLUUID& idExcept = LLUUID::null);
	static bool isForceDetachable(const LLViewerJointAttachment* pAttachPt, bool fCheckComposite = true, const LLUUID& idExcept = LLUUID::null);
	void forceDetach(const LLViewerObject* pAttachObj);
	void forceDetach(const LLViewerJointAttachment* ptAttachPt);

	// Wearables
	static bool isForceRemovable(const LLWearable* pWearable, bool fCheckComposite = true, const LLUUID& idExcept = LLUUID::null);
	static bool isForceRemovable(LLWearableType::EType wtType, bool fCheckComposite = true, const LLUUID& idExcept = LLUUID::null);
	void forceRemove(const LLWearable* pWearable);
	void forceRemove(LLWearableType::EType wtType);

public:
	void done();
protected:
	void addAttachment(const LLViewerInventoryItem* pItem, EWearAction eAction);
	void remAttachment(const LLViewerObject* pAttachObj);
	void addWearable(const LLViewerInventoryItem* pItem, EWearAction eAction);
	void remWearable(const LLWearable* pWearable);

	// Convenience (prevents long lines that run off the screen elsewhere)
	bool isAddAttachment(const LLViewerInventoryItem* pItem) const
	{
		bool fFound = false;
		for (addattachments_map_t::const_iterator itAddAttachments = m_addAttachments.begin(); 
				(!fFound) && (itAddAttachments != m_addAttachments.end()); ++itAddAttachments)
		{
			const LLInventoryModel::item_array_t& wearItems = itAddAttachments->second;
			fFound = (std::find_if(wearItems.begin(), wearItems.end(), RlvPredIsEqualOrLinkedItem(pItem)) != wearItems.end());
		}
		return fFound;
	}
	bool isRemAttachment(const LLViewerObject* pAttachObj) const
	{
		return std::find(m_remAttachments.begin(), m_remAttachments.end(), pAttachObj) != m_remAttachments.end();
	}
	bool isAddWearable(const LLViewerInventoryItem* pItem) const
	{
		bool fFound = false;
		for (addwearables_map_t::const_iterator itAddWearables = m_addWearables.begin(); 
				(!fFound) && (itAddWearables != m_addWearables.end()); ++itAddWearables)
		{
			const LLInventoryModel::item_array_t& wearItems = itAddWearables->second;
			fFound = (std::find_if(wearItems.begin(), wearItems.end(), RlvPredIsEqualOrLinkedItem(pItem)) != wearItems.end());
		}
		return fFound;
	}
	bool isRemWearable(const LLWearable* pWearable) const
	{
		return std::find(m_remWearables.begin(), m_remWearables.end(), pWearable) != m_remWearables.end();
	}

protected:
	typedef std::pair<LLWearableType::EType, LLInventoryModel::item_array_t> addwearable_pair_t;
	typedef std::map<LLWearableType::EType, LLInventoryModel::item_array_t> addwearables_map_t;
	addwearables_map_t               m_addWearables;
	typedef std::pair<S32, LLInventoryModel::item_array_t> addattachment_pair_t;
	typedef std::map<S32, LLInventoryModel::item_array_t> addattachments_map_t;
	addattachments_map_t             m_addAttachments;
	LLInventoryModel::item_array_t   m_addGestures;
	std::list<const LLViewerObject*> m_remAttachments;
	std::list<const LLWearable*>     m_remWearables;
	LLInventoryModel::item_array_t   m_remGestures;

private:
	friend class LLSingleton<RlvForceWear>;
};

// ============================================================================
// RlvBehaviourNotifyObserver
//

// TODO-RLVa: [RLVa-1.2.1] See about just reintegrating this back into RlvHandler
class RlvBehaviourNotifyHandler : public LLSingleton<RlvBehaviourNotifyHandler>
{
	friend class LLSingleton<RlvBehaviourNotifyHandler>;
protected:
	RlvBehaviourNotifyHandler();
	virtual ~RlvBehaviourNotifyHandler() { if (m_ConnCommand.connected()) m_ConnCommand.disconnect(); }

public:
	void addNotify(const LLUUID& idObj, S32 nChannel, const std::string& strFilter)
	{
		m_Notifications.insert(std::pair<LLUUID, notifyData>(idObj, notifyData(nChannel, strFilter)));
	}
	void removeNotify(const LLUUID& idObj, S32 nChannel, const std::string& strFilter)
	{
		for (std::multimap<LLUUID, notifyData>::iterator itNotify = m_Notifications.lower_bound(idObj), 
				endNotify = m_Notifications.upper_bound(idObj); itNotify != endNotify; ++itNotify)
		{
			if ( (itNotify->second.nChannel == nChannel) && (itNotify->second.strFilter == strFilter) )
			{
				m_Notifications.erase(itNotify);
				break;
			}
		}
		if (m_Notifications.empty())
			delete this;	// Delete ourself if we have nothing to do
	}
protected:
	void onCommand(const RlvCommand& rlvCmd, ERlvCmdRet eRet, bool fInternal);

protected:
	struct notifyData
	{
		S32         nChannel;
		std::string strFilter;
		notifyData(S32 channel, const std::string& filter) : nChannel(channel), strFilter(filter) {}
	};
	std::multimap<LLUUID, notifyData> m_Notifications;
	boost::signals2::connection m_ConnCommand;
};

// ============================================================================
// RlvException
//

typedef boost::variant<std::string, LLUUID, S32, ERlvBehaviour> RlvExceptionOption;

struct RlvException
{
public:
	LLUUID				idObject;    // UUID of the object that added the exception
	ERlvBehaviour		eBehaviour;  // Behaviour the exception applies to
	RlvExceptionOption	varOption;   // Exception data (type is dependent on eBehaviour)

	RlvException(const LLUUID& idObj, ERlvBehaviour eBhvr, const RlvExceptionOption& option) : idObject(idObj), eBehaviour(eBhvr), varOption(option) {}
private:
	RlvException();
};

// ============================================================================
// RlvWLSnapshot
//

struct RlvWLSnapshot
{
public:
	static void           restoreSnapshot(const RlvWLSnapshot* pWLSnapshot);
	static RlvWLSnapshot* takeSnapshot();
private:
	RlvWLSnapshot() {}

	bool		 fIsRunning;
	bool		 fUseLindenTime;
	LLWLParamSet WLParams;
};

// ============================================================================
// Various helper classes/timers/functors
//

class RlvGCTimer : public LLEventTimer
{
public:
	RlvGCTimer() : LLEventTimer(30.0) {}
	virtual BOOL tick();
};

// ============================================================================
// Various helper functions
//

bool rlvCanDeleteOrReturn();

ERlvAttachGroupType rlvAttachGroupFromIndex(S32 idxGroup);
ERlvAttachGroupType rlvAttachGroupFromString(const std::string& strGroup);

std::string rlvGetFirstParenthesisedText(const std::string& strText, std::string::size_type* pidxMatch = NULL);
std::string rlvGetLastParenthesisedText(const std::string& strText, std::string::size_type* pidxStart = NULL);
void        rlvStringReplace(std::string& strText, std::string strFrom, const std::string& strTo);

// ============================================================================
// Inlined class member functions
//

// Checked: 2009-09-19 (RLVa-1.0.3d)
inline std::string RlvCommand::asString() const
{
	// NOTE: @clear=<param> should be represented as clear:<param>
	return (m_eParamType != RLV_TYPE_CLEAR)
		? (!m_strOption.empty()) ? (std::string(m_strBehaviour)).append(":").append(m_strOption) : (std::string(m_strBehaviour))
	    : (!m_strParam.empty())  ? (std::string(m_strBehaviour)).append(":").append(m_strParam)  : (std::string(m_strBehaviour));
}

inline bool RlvCommand::operator ==(const RlvCommand& rhs) const
{
	// The specification notes that "@detach=n" is semantically identical to "@detach=add" (same for "y" and "rem"
	return (m_strBehaviour == rhs.m_strBehaviour) && (m_strOption == rhs.m_strOption) &&
		( (RLV_TYPE_UNKNOWN != m_eParamType) ? (m_eParamType == rhs.m_eParamType) : (m_strParam == rhs.m_strParam) );
}

inline bool RlvCommand::hasStrictVariant(ERlvBehaviour eBhvr)
{
	switch (eBhvr)
	{
		case RLV_BHVR_RECVCHAT:
		case RLV_BHVR_RECVEMOTE:
		case RLV_BHVR_RECVIM:
		case RLV_BHVR_SENDIM:
		case RLV_BHVR_TPLURE:
		case RLV_BHVR_SENDCHANNEL:
			return true;
		default:
			return false;
	}
}

// Checked: 2010-04-05 (RLVa-1.2.0d) | Modified: RLVa-1.2.0d
inline bool RlvForceWear::isWearableItem(const LLInventoryItem* pItem)
{
	LLAssetType::EType assetType = pItem->getType();
	return 
		(LLAssetType::AT_BODYPART == assetType) || (LLAssetType::AT_CLOTHING == assetType) ||
		(LLAssetType::AT_OBJECT == assetType) || (LLAssetType::AT_GESTURE == assetType);
}

// ============================================================================

#endif // RLV_HELPER_H
