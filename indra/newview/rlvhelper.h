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

#ifndef RLV_HELPER_H
#define RLV_HELPER_H

#include "lleventtimer.h"
#include "llinventorymodel.h"
#include "llviewerinventory.h"
#include "llwearabletype.h"
#include "llwlparamset.h"

#include "rlvdefines.h"
#include "rlvcommon.h"

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
	ERlvCmdRet         getReturnType() const	{ return m_eRet; }
	bool               hasOption() const		{ return !m_strOption.empty(); }
	bool               isStrict() const			{ return m_fStrict; }
	bool               isValid() const			{ return m_fValid; }

	typedef std::map<std::string, ERlvBehaviour> bhvr_map_t;
	static ERlvBehaviour		getBehaviourFromString(const std::string& strBhvr, bool* pfStrict = NULL);
	static bool					getCommands(bhvr_map_t& cmdList, const std::string& strMatch);
	static const std::string&	getStringFromBehaviour(ERlvBehaviour eBhvr);
	static bool					hasStrictVariant(ERlvBehaviour eBhvr);

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
	ERlvCmdRet    m_eRet;

	static bhvr_map_t m_BhvrMap;

	friend class RlvHandler;
	friend class RlvObject;
};

// ============================================================================
// RlvCommandOption (and derived classed)
//

struct RlvCommandOption
{
protected:
	RlvCommandOption() : m_fValid(false) {}
public:
	virtual ~RlvCommandOption() {}

public:
	virtual bool isEmpty() const { return false; }
	virtual bool isValid() const { return m_fValid; }
protected:
	bool m_fValid;
};

struct RlvCommandOptionGeneric : public RlvCommandOption
{
	explicit RlvCommandOptionGeneric(const std::string& strOption);

	bool isAttachmentPoint() const		{ return (!isEmpty()) && (typeid(LLViewerJointAttachment*) == m_varOption.type()); }
	bool isAttachmentPointGroup() const	{ return (!isEmpty()) && (typeid(ERlvAttachGroupType) == m_varOption.type()); }
	bool isEmpty() const				{ return m_fEmpty; }
	bool isSharedFolder() const			{ return (!isEmpty()) && (typeid(LLViewerInventoryCategory*) == m_varOption.type()); }
	bool isString() const				{ return (!isEmpty()) && (typeid(std::string) == m_varOption.type()); }
	bool isUUID() const					{ return (!isEmpty()) && (typeid(LLUUID) == m_varOption.type()); }
	bool isWearableType() const			{ return (!isEmpty()) && (typeid(LLWearableType::EType) == m_varOption.type()); }

	LLViewerJointAttachment*   getAttachmentPoint() const
		{ return (isAttachmentPoint()) ? boost::get<LLViewerJointAttachment*>(m_varOption) : NULL; }
	ERlvAttachGroupType        getAttachmentPointGroup() const
		{ return (isAttachmentPointGroup()) ? boost::get<ERlvAttachGroupType>(m_varOption) : RLV_ATTACHGROUP_INVALID; }
	LLViewerInventoryCategory* getSharedFolder() const
		{ return (isSharedFolder()) ? boost::get<LLViewerInventoryCategory*>(m_varOption) : NULL; }
	const std::string&         getString() const
		{ return (isString()) ? boost::get<std::string>(m_varOption) : LLStringUtil::null; }
	const LLUUID&              getUUID() const
		{ return (isUUID()) ? boost::get<LLUUID>(m_varOption) : LLUUID::null; }
	LLWearableType::EType      getWearableType() const
		{ return (isWearableType()) ? boost::get<LLWearableType::EType>(m_varOption) : LLWearableType::WT_INVALID; }

protected:
	bool m_fEmpty;
	boost::variant<LLViewerJointAttachment*, ERlvAttachGroupType, LLViewerInventoryCategory*, std::string, LLUUID, LLWearableType::EType> m_varOption;
};

struct RlvCommandOptionGetPath : public RlvCommandOption
{
	RlvCommandOptionGetPath(const RlvCommand& rlvCmd);

	/*virtual*/ bool  isEmpty() const	 { return m_idItems.empty(); }
	const uuid_vec_t& getItemIDs() const { return m_idItems; }

	static bool getItemIDs(const LLViewerJointAttachment* pAttachPt, uuid_vec_t& idItems, bool fClear = true);
	static bool getItemIDs(LLWearableType::EType wtType, uuid_vec_t& idItems, bool fClear = true);

protected:
	uuid_vec_t m_idItems;
};

struct RlvCommandOptionAdjustHeight : public RlvCommandOption
{
	RlvCommandOptionAdjustHeight(const RlvCommand& rlvCmd);

	F32 m_nPelvisToFoot;
	F32 m_nPelvisToFootDeltaMult;
	F32 m_nPelvisToFootOffset;
};

struct RlvCommandOptionTpTo : public RlvCommandOption
{
	RlvCommandOptionTpTo(const RlvCommand& rlvCmd);

	LLVector3d m_posGlobal;
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
	void setCommandRet(const RlvCommand& rlvCmd, ERlvCmdRet eRet);

	std::string getStatusString(const std::string& strMatch) const;
	bool        hasBehaviour(ERlvBehaviour eBehaviour, bool fStrictOnly) const;
	bool        hasBehaviour(ERlvBehaviour eBehaviour, const std::string& strOption, bool fStrictOnly) const;

	const rlv_command_list_t* getCommandList() const { return &m_Commands; }

	const LLUUID&		getObjectID() const	{ return m_idObj; }
	const LLUUID&		getRootID() const	{ return m_idRoot; }

	/*
	 * Member variables
	 */
protected:
	S32                m_idxAttachPt;		// The object's attachment point (or 0 if it's not an attachment)
	LLUUID             m_idObj;				// The object's UUID
	LLUUID             m_idRoot;			// The UUID of the object's root (may or may not be different from m_idObj)
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
	static void sendNotification(const std::string& strText, const std::string& strSuffix = LLStringUtil::null);

	/*
	 * Event handlers
	 */
public:
	static void	onWear(LLWearableType::EType eType, bool fAllowed);
	static void	onTakeOff(LLWearableType::EType eType, bool fAllowed);
	static void	onAttach(const LLViewerJointAttachment* pAttachPt, bool fAllowed);
	static void	onDetach(const LLViewerJointAttachment* pAttachPt, bool fAllowed);
	static void	onReattach(const LLViewerJointAttachment* pAttachPt, bool fAllowed);
protected:
	void		onCommand(const RlvCommand& rlvCmd, ERlvCmdRet eRet, bool fInternal);

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
// Various helper classes/timers/functors
//

class RlvGCTimer : public LLEventTimer
{
public:
	RlvGCTimer() : LLEventTimer(30.0) {}
	virtual BOOL tick();
};

class RlvCallbackTimerOnce : public LLEventTimer
{
public:
	typedef boost::function<void ()> nullary_func_t;
public:
	RlvCallbackTimerOnce(F32 nPeriod, nullary_func_t cb) : LLEventTimer(nPeriod), m_Callback(cb) {}
	/*virtual*/ BOOL tick()
	{
		m_Callback();
		return TRUE;
	}
protected:
	nullary_func_t m_Callback;
};

inline void rlvCallbackTimerOnce(F32 nPeriod, RlvCallbackTimerOnce::nullary_func_t cb)
{
	// Timer will automatically delete itself after the callback
	new RlvCallbackTimerOnce(nPeriod, cb);
}

// ============================================================================
// Various helper functions
//

ERlvAttachGroupType rlvAttachGroupFromIndex(S32 idxGroup);
ERlvAttachGroupType rlvAttachGroupFromString(const std::string& strGroup);

std::string rlvGetFirstParenthesisedText(const std::string& strText, std::string::size_type* pidxMatch = NULL);
std::string rlvGetLastParenthesisedText(const std::string& strText, std::string::size_type* pidxStart = NULL);

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
	LLAssetType::EType assetType = (pItem) ? pItem->getType() : LLAssetType::AT_NONE;
	return 
		(LLAssetType::AT_BODYPART == assetType) || (LLAssetType::AT_CLOTHING == assetType) ||
		(LLAssetType::AT_OBJECT == assetType) || (LLAssetType::AT_GESTURE == assetType);
}

// ============================================================================

#endif // RLV_HELPER_H
