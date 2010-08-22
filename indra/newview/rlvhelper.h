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
	explicit RlvCommand(const std::string& strCommand);

	/*
	 * Member functions
	 */
public:
	std::string        asString() const;
	const std::string& getBehaviour() const     { return m_strBehaviour; }
	ERlvBehaviour      getBehaviourType() const { return m_eBehaviour; }
	const std::string& getOption() const        { return m_strOption; }
	const std::string& getParam() const         { return m_strParam; }
	ERlvParamType      getParamType() const     { return m_eParamType; }
	bool               isStrict() const			{ return m_fStrict; }
	bool               isValid() const          { return m_fValid; }

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
	bool         	m_fValid;
	std::string  	m_strBehaviour;
	ERlvBehaviour	m_eBehaviour;
	bool            m_fStrict;
	std::string  	m_strOption;
	std::string  	m_strParam;
	ERlvParamType	m_eParamType;

	typedef std::map<std::string, ERlvBehaviour> RlvBhvrTable;
	static RlvBhvrTable m_BhvrMap;

	friend class RlvHandler;
};
typedef std::list<RlvCommand> rlv_command_list_t;

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
	enum eWearAction { ACTION_ATTACH, ACTION_DETACH };
	enum eWearFlags { FLAG_NONE = 0x00, FLAG_MATCHALL = 0x01, FLAG_DEFAULT = FLAG_NONE };
	void forceFolder(const LLViewerInventoryCategory* pFolder, eWearAction eAction, eWearFlags eFlags);

	// Generic
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
	void addAttachment(const LLViewerInventoryItem* pItem);
	void remAttachment(const LLViewerObject* pAttachObj);
	void addWearable(const LLViewerInventoryItem* pItem);
	void remWearable(const LLWearable* pWearable);

	// Convenience (prevents long lines that run off the screen elsewhere)
	bool isAddAttachment(const LLViewerInventoryItem* pItem) const
	{
		return std::find_if(m_addAttachments.begin(), m_addAttachments.end(), RlvPredIsEqualOrLinkedItem(pItem)) != m_addAttachments.end();
	}
	bool isRemAttachment(const LLViewerObject* pAttachObj) const
	{
		return std::find(m_remAttachments.begin(), m_remAttachments.end(), pAttachObj) != m_remAttachments.end();
	}
	bool isAddWearable(const LLViewerInventoryItem* pItem) const
	{
		return std::find_if(m_addWearables.begin(), m_addWearables.end(), RlvPredIsEqualOrLinkedItem(pItem)) != m_addWearables.end();
	}
	bool isRemWearable(const LLWearable* pWearable) const
	{
		return std::find(m_remWearables.begin(), m_remWearables.end(), pWearable) != m_remWearables.end();
	}

protected:
	LLInventoryModel::item_array_t m_addAttachments, m_addWearables, m_addGestures, m_remGestures;
	std::list<const LLViewerObject*> m_remAttachments;
	std::list<const LLWearable*> m_remWearables;

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
	void onCommand(const LLUUID& idRlvObj, const RlvCommand& rlvCmd, ERlvCmdRet eRet, bool fInternal);

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
// RlvRetainedCommand
//

struct RlvRetainedCommand
{
public:
	LLUUID      idObject;
	RlvCommand  rlvCmd;

	RlvRetainedCommand(const LLUUID& uuid, const RlvCommand& cmd) : idObject(uuid), rlvCmd(cmd) {}
private:
	RlvRetainedCommand();
};
typedef std::list<RlvRetainedCommand> rlv_retained_list_t;

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
bool rlvIsEmote(const std::string& strUTF8Text);
bool rlvIsValidReplyChannel(S32 nChannel);

void rlvSendBusyMessage(const LLUUID& idTo, const std::string& strMsg, const LLUUID& idSession = LLUUID::null);
bool rlvSendChatReply(const std::string& strChannel, const std::string& strReply);
bool rlvSendChatReply(S32 nChannel, const std::string& strReply);

std::string rlvGetFirstParenthesisedText(const std::string& strText, std::string::size_type* pidxMatch = NULL);
std::string rlvGetLastParenthesisedText(const std::string& strText, std::string::size_type* pidxStart = NULL);
void        rlvStringReplace(std::string& strText, std::string strFrom, const std::string& strTo);

// ============================================================================
// Debug helper functions
//

void rlvDebugFailedAssertion(const char* pstrAssert, const char* pstrFile, int nLine);
void rlvNotifyFailedAssertion(const char* pstrAssert, const char* pstrFile, int nLine);

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
// Inlined helper functions
//

// Checked: 2010-03-26 (RLVa-1.2.0b) | Modified: RLVa-1.0.2a
inline bool rlvIsEmote(const std::string& strUTF8Text)
{
	return (strUTF8Text.length() > 4) && ( (strUTF8Text.compare(0, 4, "/me ") == 0) || (strUTF8Text.compare(0, 4, "/me'") == 0) );
}

// Checked: 2010-03-09 (RLVa-1.2.0b) | Added: RLVa-1.0.2a
inline bool rlvIsValidReplyChannel(S32 nChannel)
{
	return (nChannel > 0) && (CHAT_CHANNEL_DEBUG != nChannel);
}

// Checked: 2009-08-05 (RLVa-1.0.1e) | Added: RLVa-1.0.0e
inline bool rlvSendChatReply(const std::string& strChannel, const std::string& strReply)
{
	S32 nChannel;
	return (LLStringUtil::convertToS32(strChannel, nChannel)) ? rlvSendChatReply(nChannel, strReply) : false;
}

// ============================================================================

#endif // RLV_HELPER_H
