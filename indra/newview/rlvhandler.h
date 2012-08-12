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

#ifndef RLV_HANDLER_H
#define RLV_HANDLER_H

#include <stack>

#include "rlvcommon.h"
#if LL_GNUC
#include "rlvhelper.h"		// Needed to make GCC happy
#endif // LL_GNUC

// ============================================================================

class RlvHandler : public LLOldEvents::LLSimpleListener
{
public:
	RlvHandler();
	~RlvHandler();

	// --------------------------------

	/*
	 * Rule checking functions
	 */
	// NOTE: - to check @detach=n    -> (see RlvAttachmentLocks)
	//       - to check @addattach=n -> (see RlvAttachmentLocks)
	//       - to check @remattach=n -> (see RlvAttachmentLocks)
	//       - to check @addoutfit=n -> (see RlvWearableLocks)
	//       - to check @remoutfit=n -> (see RlvWearableLocks)
	//       - to check exceptions   -> isException()
public:
	// Returns TRUE is at least one object contains the specified behaviour (and optional option)
	bool hasBehaviour(ERlvBehaviour eBhvr) const { return (eBhvr < RLV_BHVR_COUNT) ? (0 != m_Behaviours[eBhvr]) : false; }
	bool hasBehaviour(ERlvBehaviour eBhvr, const std::string& strOption) const;
	// Returns TRUE if at least one object (except the specified one) contains the specified behaviour (and optional option)
	bool hasBehaviourExcept(ERlvBehaviour eBhvr, const LLUUID& idObj) const;
	bool hasBehaviourExcept(ERlvBehaviour eBhvr, const std::string& strOption, const LLUUID& idObj) const;
	// Returns TRUE if at least one object in the linkset with specified root ID contains the specified behaviour (and optional option)
	bool hasBehaviourRoot(const LLUUID& idObjRoot, ERlvBehaviour eBhvr, const std::string& strOption = LLStringUtil::null) const;

	// Adds or removes an exception for the specified behaviour
	void addException(const LLUUID& idObj, ERlvBehaviour eBhvr, const RlvExceptionOption& varOption);
	void removeException(const LLUUID& idObj, ERlvBehaviour eBhvr, const RlvExceptionOption& varOption);
	// Returns TRUE if the specified behaviour has an added exception 
	bool hasException(ERlvBehaviour eBhvr) const;
	// Returns TRUE if the specified option was added as an exception for the specified behaviour
	bool isException(ERlvBehaviour eBhvr, const RlvExceptionOption& varOption, ERlvExceptionCheck typeCheck = RLV_CHECK_DEFAULT) const;
	// Returns TRUE if the specified behaviour should behave "permissive" (rather than "strict"/"secure")
	bool isPermissive(ERlvBehaviour eBhvr) const;

	#ifdef RLV_EXPERIMENTAL_COMPOSITEFOLDERS
	// Returns TRUE if the composite folder doesn't contain any "locked" items
	bool canTakeOffComposite(const LLInventoryCategory* pFolder) const;
	// Returns TRUE if the composite folder doesn't replace any "locked" items
	bool canWearComposite(const LLInventoryCategory* pFolder) const;
	// Returns TRUE if the folder is a composite folder and optionally returns the name
	bool getCompositeInfo(const LLInventoryCategory* pFolder, std::string* pstrName) const;
	// Returns TRUE if the inventory item belongs to a composite folder and optionally returns the name and composite folder
	bool getCompositeInfo(const LLUUID& idItem, std::string* pstrName, LLViewerInventoryCategory** ppFolder) const;
	// Returns TRUE if the folder is a composite folder
	bool isCompositeFolder(const LLInventoryCategory* pFolder) const { return getCompositeInfo(pFolder, NULL); }
	// Returns TRUE if the inventory item belongs to a composite folder
	bool isCompositeDescendent(const LLUUID& idItem) const { return getCompositeInfo(idItem, NULL, NULL); }
	// Returns TRUE if the inventory item is part of a folded composite folder and should be hidden from @getoufit or @getattach
	bool isHiddenCompositeItem(const LLUUID& idItem, const std::string& strItemType) const;
	#endif // RLV_EXPERIMENTAL_COMPOSITEFOLDERS

	// --------------------------------

	/*
	 * Helper functions 
	 */
public:
	// Accessors
	bool              getCanCancelTp() const		{ return m_fCanCancelTp; }					// @accepttp and @tpto
	void              setCanCancelTp(bool fAllow)	{ m_fCanCancelTp = fAllow; }				// @accepttp and @tpto
	const LLVector3d& getSitSource() const						{ return m_posSitSource; }		// @standtp
	void              setSitSource(const LLVector3d& posSource)	{ m_posSitSource = posSource; }	// @standtp

	// Command specific helper functions
	bool canEdit(const LLViewerObject* pObj) const;												// @edit and @editobj
	bool canReceiveIM(const LLUUID& idSender) const;											// @recvim and @recvimfrom
	bool canShowHoverText(const LLViewerObject* pObj) const;									// @showhovertext* command family
	bool canSendIM(const LLUUID& idRecipient) const;											// @sendim and @sendimto
	bool canSit(LLViewerObject* pObj, const LLVector3& posOffset = LLVector3::zero) const;
	bool canStartIM(const LLUUID& idRecipient) const;											// @startim and @startimto
	bool canStand() const;
	bool canTeleportViaLure(const LLUUID& idAgent) const;
	bool canTouch(const LLViewerObject* pObj, const LLVector3& posOffset = LLVector3::zero) const;	// @touch
	bool filterChat(std::string& strUTF8Text, bool fFilterEmote) const;							// @sendchat, @recvchat and @redirchat
	bool redirectChatOrEmote(const std::string& strUTF8Test) const;								// @redirchat and @rediremote

	// Command processing helper functions
	ERlvCmdRet processCommand(const LLUUID& idObj, const std::string& strCommand, bool fFromObj);
	void       processRetainedCommands(ERlvBehaviour eBhvrFilter = RLV_BHVR_UNKNOWN, ERlvParamType eTypeFilter = RLV_TYPE_UNKNOWN);

	// Returns a pointer to the currently executing command (do *not* save this pointer)
	const RlvCommand* getCurrentCommand() const { return (!m_CurCommandStack.empty()) ? m_CurCommandStack.top() : NULL; }
	// Returns the UUID of the object we're currently executing a command for
	const LLUUID&     getCurrentObject() const	{ return (!m_CurObjectStack.empty()) ? m_CurObjectStack.top() : LLUUID::null; }

	// Initialization
	static BOOL canDisable();
	static BOOL isEnabled()	{ return m_fEnabled; }
	static BOOL setEnabled(BOOL fEnable);
protected:
	void clearState();

	// --------------------------------

	/*
	 * Event handling
	 */
public:
	// The behaviour signal is triggered whenever a command is successfully processed and resulted in adding or removing a behaviour
	typedef boost::signals2::signal<void (ERlvBehaviour, ERlvParamType)> rlv_behaviour_signal_t;
	boost::signals2::connection setBehaviourCallback(const rlv_behaviour_signal_t::slot_type& cb )		 { return m_OnBehaviour.connect(cb); }
	boost::signals2::connection setBehaviourToggleCallback(const rlv_behaviour_signal_t::slot_type& cb ) { return m_OnBehaviourToggle.connect(cb); }
	// The command signal is triggered whenever a command is processed
	typedef boost::signals2::signal<void (const RlvCommand&, ERlvCmdRet, bool)> rlv_command_signal_t;
	boost::signals2::connection setCommandCallback(const rlv_command_signal_t::slot_type& cb )			 { return m_OnCommand.connect(cb); }

	void addCommandHandler(RlvCommandHandler* pHandler);
	void removeCommandHandler(RlvCommandHandler* pHandler);
protected:
	void clearCommandHandlers();
	bool notifyCommandHandlers(rlvCommandHandler f, const RlvCommand& rlvCmd, ERlvCmdRet& eRet, bool fNotifyAll) const;

	// Externally invoked event handlers
public:
	bool handleEvent(LLPointer<LLOldEvents::LLEvent> event, const LLSD& sdUserdata);			// Implementation of public LLSimpleListener
	void onAttach(const LLViewerObject* pAttachObj, const LLViewerJointAttachment* pAttachPt);
	void onDetach(const LLViewerObject* pAttachObj, const LLViewerJointAttachment* pAttachPt);
	bool onGC();
	void onLoginComplete();
	void onSitOrStand(bool fSitting);
	void onTeleportFailed();
	void onTeleportFinished(const LLVector3d& posArrival);
	static void onIdleStartup(void* pParam);

	/*
	 * Command processing
	 */
protected:
	ERlvCmdRet processCommand(const RlvCommand& rlvCmd, bool fFromObj);
	ERlvCmdRet processClearCommand(const RlvCommand& rlvCmd);

	// Command handlers (RLV_TYPE_ADD and RLV_TYPE_CLEAR)
	ERlvCmdRet processAddRemCommand(const RlvCommand& rlvCmd);
	ERlvCmdRet onAddRemAttach(const RlvCommand& rlvCmd, bool& fRefCount);
	ERlvCmdRet onAddRemDetach(const RlvCommand& rlvCmd, bool& fRefCount);
	ERlvCmdRet onAddRemFolderLock(const RlvCommand& rlvCmd, bool& fRefCount);
	ERlvCmdRet onAddRemFolderLockException(const RlvCommand& rlvCmd, bool& fRefCount);
	// Command handlers (RLV_TYPE_FORCE)
	ERlvCmdRet processForceCommand(const RlvCommand& rlvCmd) const;
	ERlvCmdRet onForceRemAttach(const RlvCommand& rlvCmd) const;
	ERlvCmdRet onForceRemOutfit(const RlvCommand& rlvCmd) const;
	ERlvCmdRet onForceGroup(const RlvCommand& rlvCmd) const;
	ERlvCmdRet onForceSit(const RlvCommand& rlvCmd) const;
	ERlvCmdRet onForceWear(const LLViewerInventoryCategory* pFolder, ERlvBehaviour eBhvr) const;
	void       onForceWearCallback(const uuid_vec_t& idItems, ERlvBehaviour eBhvr) const;
	// Command handlers (RLV_TYPE_REPLY)
	ERlvCmdRet processReplyCommand(const RlvCommand& rlvCmd) const;
	ERlvCmdRet onFindFolder(const RlvCommand& rlvCmd, std::string& strReply) const;
	ERlvCmdRet onGetAttach(const RlvCommand& rlvCmd, std::string& strReply) const;
	ERlvCmdRet onGetAttachNames(const RlvCommand& rlvCmd, std::string& strReply) const;
	ERlvCmdRet onGetInv(const RlvCommand& rlvCmd, std::string& strReply) const;
	ERlvCmdRet onGetInvWorn(const RlvCommand& rlvCmd, std::string& strReply) const;
	ERlvCmdRet onGetOutfit(const RlvCommand& rlvCmd, std::string& strReply) const;
	ERlvCmdRet onGetOutfitNames(const RlvCommand& rlvCmd, std::string& strReply) const;
	ERlvCmdRet onGetPath(const RlvCommand& rlvCmd, std::string& strReply) const;

	// --------------------------------

	/*
	 * Member variables
	 */
public:
	typedef std::map<LLUUID, RlvObject> rlv_object_map_t;
	typedef std::multimap<ERlvBehaviour, RlvException> rlv_exception_map_t;
protected:
	rlv_object_map_t      m_Objects;				// Map of objects that have active restrictions (idObj -> RlvObject)
	rlv_exception_map_t   m_Exceptions;				// Map of currently active restriction exceptions (ERlvBehaviour -> RlvException)
	S16                   m_Behaviours[RLV_BHVR_COUNT];

	rlv_command_list_t    m_Retained;
	RlvGCTimer*           m_pGCTimer;

	std::stack<const RlvCommand*> m_CurCommandStack;// Convenience (see @tpto)
	std::stack<LLUUID>    m_CurObjectStack;			// Convenience (see @tpto)

	rlv_behaviour_signal_t m_OnBehaviour;
	rlv_behaviour_signal_t m_OnBehaviourToggle;
	rlv_command_signal_t   m_OnCommand;
	mutable std::list<RlvCommandHandler*> m_CommandHandlers;

	static BOOL			  m_fEnabled;				// Use setEnabled() to toggle this

	bool				m_fCanCancelTp;				// @accepttp=n and @tpto=force
	mutable LLVector3d	m_posSitSource;				// @standtp=n (mutable because onForceXXX handles are all declared as const)
	LLUUID				m_idAgentGroup;				// @setgroup=n

	friend class RlvSharedRootFetcher;				// Fetcher needs access to m_fFetchComplete
	friend class RlvGCTimer;						// Timer clear its own point at destruction

	// --------------------------------

	/*
	 * Internal access functions used by unit tests
	 */
public:
	const rlv_object_map_t*    getObjectMap() const		{ return &m_Objects; }
	//const rlv_exception_map_t* getExceptionMap() const	{ return &m_Exceptions; }
};

typedef RlvHandler rlv_handler_t;
extern rlv_handler_t gRlvHandler;

// ============================================================================
// Inlined member functions
//

// Checked: 2010-11-29 (RLVa-1.3.0c) | Added: RLVa-1.3.0c
inline bool RlvHandler::canEdit(const LLViewerObject* pObj) const
{
	// The specified object can be edited if:
	//   - not generally restricted from editing (or the object's root is an exception)
	//   - not specifically restricted from editing this object's root
	return 
		(pObj) &&
		((!hasBehaviour(RLV_BHVR_EDIT)) || (isException(RLV_BHVR_EDIT, pObj->getRootEdit()->getID()))) &&
		((!hasBehaviour(RLV_BHVR_EDITOBJ)) || (!isException(RLV_BHVR_EDITOBJ, pObj->getRootEdit()->getID())));
}

// Checked: 2010-11-30 (RLVa-1.3.0c) | Added: RLVa-1.3.0c
inline bool RlvHandler::canReceiveIM(const LLUUID& idSender) const
{
	// User can receive an IM from "sender" (could be an agent or a group) if:
	//   - not generally restricted from receiving IMs (or the sender is an exception)
	//   - not specifically restricted from receiving an IM from the sender
	return 
		( (!hasBehaviour(RLV_BHVR_RECVIM)) || (isException(RLV_BHVR_RECVIM, idSender)) ) &&
		( (!hasBehaviour(RLV_BHVR_RECVIMFROM)) || (!isException(RLV_BHVR_RECVIMFROM, idSender)) );
}

// Checked: 2010-11-30 (RLVa-1.3.0c) | Added: RLVa-1.3.0c
inline bool RlvHandler::canSendIM(const LLUUID& idRecipient) const
{
	// User can send an IM to "recipient" (could be an agent or a group) if:
	//   - not generally restricted from sending IMs (or the recipient is an exception)
	//   - not specifically restricted from sending an IM to the recipient
	return 
		( (!hasBehaviour(RLV_BHVR_SENDIM)) || (isException(RLV_BHVR_SENDIM, idRecipient)) ) &&
		( (!hasBehaviour(RLV_BHVR_SENDIMTO)) || (!isException(RLV_BHVR_SENDIMTO, idRecipient)) );
}

// Checked: 2010-03-27 (RLVa-1.4.0a) | Modified: RLVa-1.0.0f
inline bool RlvHandler::canShowHoverText(const LLViewerObject *pObj) const
{
	return ( (!pObj) || (LL_PCODE_VOLUME != pObj->getPCode()) ||
		    !( (hasBehaviour(RLV_BHVR_SHOWHOVERTEXTALL)) ||
			   ( (hasBehaviour(RLV_BHVR_SHOWHOVERTEXTWORLD)) && (!pObj->isHUDAttachment()) ) ||
			   ( (hasBehaviour(RLV_BHVR_SHOWHOVERTEXTHUD)) && (pObj->isHUDAttachment()) ) ||
			   (isException(RLV_BHVR_SHOWHOVERTEXT, pObj->getID(), RLV_CHECK_PERMISSIVE)) ) );
}

inline bool RlvHandler::canStartIM(const LLUUID& idRecipient) const
{
	// User can start an IM session with "recipient" (could be an agent or a group) if:
	//   - not generally restricted from starting IM sessions (or the recipient is an exception)
	//   - not specifically restricted from starting an IM session with the recipient
	return 
		( (!hasBehaviour(RLV_BHVR_STARTIM)) || (isException(RLV_BHVR_STARTIM, idRecipient)) ) &&
		( (!hasBehaviour(RLV_BHVR_STARTIMTO)) || (!isException(RLV_BHVR_STARTIMTO, idRecipient)) );
}

// Checked: 2010-12-11 (RLVa-1.2.2c) | Added: RLVa-1.2.2c
inline bool RlvHandler::canTeleportViaLure(const LLUUID& idAgent) const
{
	return ((!hasBehaviour(RLV_BHVR_TPLURE)) || (isException(RLV_BHVR_TPLURE, idAgent))) && (canStand());
}

inline bool RlvHandler::hasBehaviour(ERlvBehaviour eBhvr, const std::string& strOption) const
{
	return hasBehaviourExcept(eBhvr, strOption, LLUUID::null);
}

inline bool RlvHandler::hasBehaviourExcept(ERlvBehaviour eBhvr, const LLUUID& idObj) const
{
	return hasBehaviourExcept(eBhvr, LLStringUtil::null, idObj);
}

// ============================================================================

#endif // RLV_HANDLER_H
