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

#ifndef RLV_LOCKS_H
#define RLV_LOCKS_H

#include "llagentconstants.h"
#include "llagentwearables.h"
#include "lleventtimer.h"
#include "llvoavatarself.h"

#include "rlvdefines.h"
#include "rlvcommon.h"

// ============================================================================
// RlvAttachPtLookup class declaration
//

class RlvAttachPtLookup
{
public:
	static LLViewerJointAttachment* getAttachPoint(S32 idxAttachPt);
	static LLViewerJointAttachment* getAttachPoint(const std::string& strText);
	static LLViewerJointAttachment* getAttachPoint(const LLInventoryItem* pItem);

	static S32 getAttachPointIndex(std::string strText);
	static S32 getAttachPointIndex(const LLViewerObject* pAttachObj);
	static S32 getAttachPointIndex(const LLViewerJointAttachment* pAttachPt);
	static S32 getAttachPointIndex(const LLInventoryCategory* pFolder);
	static S32 getAttachPointIndex(const LLInventoryItem* pItem, bool fFollowLinks = true);

	static bool hasAttachPointName(const LLInventoryItem* pItem) { return (0 != getAttachPointIndex(pItem)); }
private:
	static S32 getAttachPointIndexLegacy(const LLInventoryCategory* pFolder);

public:
	static void initLookupTable();
private:
	static std::map<std::string, S32> m_AttachPtLookupMap;
};

// ============================================================================
// RlvAttachmentLocks class declaration
//

// TODO-RLVa: [RLVa-1.2.1] Once everything is working for SL-2.0 thin out the member functions since a few of them are duplicates/unneeded
class RlvAttachmentLocks
{
public:
	RlvAttachmentLocks() : m_fHasLockedHUD(false) {}

public:
	// Adds an RLV_LOCK_REMOVE lock (held by idRlvObj) for the attachment
	void addAttachmentLock(const LLUUID& idAttachObj, const LLUUID& idRlvObj);
	// Adds an eLock type lock (held by idRlvObj) for the attachment point
	void addAttachmentPointLock(S32 idxAttachPt, const LLUUID& idRlvObj, ERlvLockMask eLock);

	// Returns TRUE if there is at least 1 non-detachable attachment currently attached to the attachment point
	bool hasLockedAttachment(const LLViewerJointAttachment* pAttachPt) const;
	// Returns TRUE if there is at least 1 eLock type locked attachment point (RLV_LOCK_ANY = RLV_LOCK_ADD *or* RLV_LOCK_REMOVE)
	//   - RLV_LOCK_REMOVE: specific attachment locked *or* any attachment point locked (regardless of whether it currently has attachments)
	bool hasLockedAttachmentPoint(ERlvLockMask eLock) const;
	// Returns TRUE if there is at least 1 non-detachable HUD attachment
	bool hasLockedHUD() const { return m_fHasLockedHUD; }

	// Returns TRUE if the attachment is RLV_LOCK_REMOVE locked
	bool isLockedAttachment(const LLViewerObject* pAttachObj) const;
	// Returns TRUE if the attachment point is RLV_LOCK_REMOVE locked by anything other than idRlvObj
	bool isLockedAttachmentExcept(const LLViewerObject* pObj, const LLUUID& idRlvObj) const;
	// Returns TRUE if the attachment point is eLock type locked (RLV_LOCK_ANY = RLV_LOCK_ADD *or* RLV_LOCK_REMOVE)
	bool isLockedAttachmentPoint(S32 idxAttachPt, ERlvLockMask eLock) const;
	bool isLockedAttachmentPoint(const LLViewerJointAttachment* pAttachPt, ERlvLockMask eLock) const;

	// Removes an RLV_LOCK_REMOVE lock (held by idRlvObj) for the attachment
	void removeAttachmentLock(const LLUUID& idAttachObj, const LLUUID& idRlvObj);
	// Removes an eLock type lock (held by idRlvObj) for the attachment point
	void removeAttachmentPointLock(S32 idxAttachPt, const LLUUID& idRlvObj, ERlvLockMask eLock);

	// Refreshes locked HUD attachment state
	void updateLockedHUD();
	// Iterates over all current attachment and attachment point locks and verifies their status (returns TRUE if verification succeeded)
	bool verifyAttachmentLocks();

protected:
	// Returns TRUE if the attachment point is eLock type locked by anything other than idRlvObj
	bool isLockedAttachmentPointExcept(S32 idxAttachPt, ERlvLockMask eLock, const LLUUID& idRlvObj) const;

	/*
	 * canAttach/canDetach trivial helper functions (note that a more approriate name might be userCanAttach/userCanDetach)
	 */
public:
	// Returns TRUE if there is at least one attachment point that can be attached to
	bool         canAttach() const;
	// Returns TRUE if the inventory item can be attached by the user (and optionally provides the attachment point - which may be NULL)
	ERlvWearMask canAttach(const LLInventoryItem* pItem, LLViewerJointAttachment** ppAttachPtOut = NULL) const;
	// Returns TRUE if the attachment point can be attached to by the user
	ERlvWearMask canAttach(const LLViewerJointAttachment* pAttachPt) const;

	// Returns TRUE if the inventory item can be detached by the user
	bool canDetach(const LLInventoryItem* pItem) const;
	// Returns TRUE if the attachment point has at least one attachment that can be detached by the user
	bool canDetach(const LLViewerJointAttachment* pAttachPt, bool fDetachAll = false) const;

	/*
	 * Member variables
	 */
public:
	typedef std::multimap<LLUUID, LLUUID> rlv_attachobjlock_map_t;
	typedef std::multimap<S32, LLUUID> rlv_attachptlock_map_t;
	// Accessors for RlvFloaterLocks
	const rlv_attachptlock_map_t& getAttachPtLocks(ERlvLockMask eLock) { return (RLV_LOCK_ADD == eLock) ? m_AttachPtAdd : m_AttachPtRem; }
	const rlv_attachobjlock_map_t& getAttachObjLocks() { return m_AttachObjRem; }
private:
	rlv_attachptlock_map_t	m_AttachPtAdd;		// Map of attachment points that can't be attached to (idxAttachPt -> idObj)
	rlv_attachptlock_map_t	m_AttachPtRem;		// Map of attachment points whose attachments can't be detached (idxAttachPt -> idObj)
	rlv_attachobjlock_map_t	m_AttachObjRem;		// Map of attachments that can't be detached (idAttachObj -> idObj)

	bool m_fHasLockedHUD;
};

extern RlvAttachmentLocks gRlvAttachmentLocks;

// ============================================================================
// RlvAttachmentLockWatchdog - Self contained class that automagically takes care of enforcing attachment locks (ie reattach-on-detach)
//

// TODO-RLVa: [RLVa-1.2.1] This class really looks rather cluttered so look into cleaning it up/simplifying it a bit
class RlvAttachmentLockWatchdog : public LLSingleton<RlvAttachmentLockWatchdog>
{
	friend class LLSingleton<RlvAttachmentLockWatchdog>;
protected:
	RlvAttachmentLockWatchdog() : m_pTimer(NULL) {}
	~RlvAttachmentLockWatchdog() { delete m_pTimer; }

	/*
	 * Member functions
	 */
protected:
	// NOTE: detach does *not* respect attachment locks so use with care
	void detach(const LLViewerObject* pAttachObj);
	void detach(S32 idxAttachPt) { uuid_vec_t idsAttachObjExcept; detach(idxAttachPt, idsAttachObjExcept); }
	void detach(S32 idxAttachPt, const uuid_vec_t& idsAttachObjExcept);

	void startTimer() { if (!m_pTimer) m_pTimer = new RlvAttachmentLockWatchdogTimer(this); }

	/*
	 * Event handlers
	 */
public:
	void onAttach(const LLViewerObject* pAttachObj, const LLViewerJointAttachment* pAttachPt);
	void onDetach(const LLViewerObject* pAttachObj, const LLViewerJointAttachment* pAttachPt);
	void onSavedAssetIntoInventory(const LLUUID& idItem);
	BOOL onTimer();
	void onWearAttachment(const LLInventoryItem* pItem, ERlvWearMask eWearAction);
	void onWearAttachment(const LLUUID& idItem, ERlvWearMask eWearAction);

	/*
	 * Member variables
	 */
protected:
	typedef std::list<LLUUID> rlv_detach_map_t;
	rlv_detach_map_t m_PendingDetach;

	struct RlvReattachInfo
	{
		RlvReattachInfo(const LLUUID& itemid) : idItem(itemid), fAssetSaved(false), tsAttach(0) 
			{ tsDetach = LLFrameTimer::getElapsedSeconds(); }

		LLUUID idItem;
		bool   fAssetSaved;
		F64    tsDetach;
		F64    tsAttach;
	protected:
		RlvReattachInfo();
	};
	typedef std::multimap<S32, RlvReattachInfo> rlv_attach_map_t;
	rlv_attach_map_t m_PendingAttach;

	struct RlvWearInfo
	{
		RlvWearInfo(const LLUUID& itemid, ERlvWearMask wearaction) : idItem(itemid), eWearAction(wearaction)
			{ tsWear = LLFrameTimer::getElapsedSeconds(); }

		bool isAddLockedAttachPt(S32 idxAttachPt) const;
		void dumpInstance() const;

		LLUUID       idItem;
		ERlvWearMask eWearAction;
		F64          tsWear;
		std::map<S32, uuid_vec_t> attachPts;
	protected:
		RlvWearInfo();
	};
	typedef std::map<LLUUID, RlvWearInfo> rlv_wear_map_t;
	rlv_wear_map_t m_PendingWear;

	class RlvAttachmentLockWatchdogTimer : public LLEventTimer
	{
	public:
		RlvAttachmentLockWatchdogTimer(RlvAttachmentLockWatchdog* pWatchdog) : LLEventTimer(10), m_pWatchdog(pWatchdog) {}
		virtual ~RlvAttachmentLockWatchdogTimer() { m_pWatchdog->m_pTimer = NULL; }
		virtual BOOL tick() { return m_pWatchdog->onTimer(); }
		RlvAttachmentLockWatchdog* m_pWatchdog;
	} *m_pTimer;
};

// ============================================================================
// RlvWearableLocks class declaration - modelled on RlvAttachmentLocks (attach pt = wearable type - attachment = wearable)
//

class RlvWearableLocks
{
public:
	// Adds an eLock type lock (held by idRlvObj) for the wearable type
	void addWearableTypeLock(LLWearableType::EType eType, const LLUUID& idRlvObj, ERlvLockMask eLock);

	// Returns TRUE if there is at least 1 non-removable wearable currently worn on this wearable type
	bool hasLockedWearable(LLWearableType::EType eType) const;
	// Returns TRUE if there is at least 1 eLock type locked wearable type (RLV_LOCK_ANY = RLV_LOCK_ADD *or* RLV_LOCK_REMOVE)
	//   - RLV_LOCK_REMOVE: specific wearable locked *or* any wearable type locked (regardless of whether it currently has wearables)
	bool hasLockedWearableType(ERlvLockMask eLock) const;

	// Removes an eLock type lock (held by idRlvObj) for the wearable type
	void removeWearableTypeLock(LLWearableType::EType eType, const LLUUID& idRlvObj, ERlvLockMask eLock);

	// Returns TRUE if the wearable is RLV_LOCK_REMOVE locked
	bool isLockedWearable(const LLWearable* pWearable) const;
	// Returns TRUE if the wearable is RLV_LOCK_REMOVE locked by anything other than idRlvObj
	bool isLockedWearableExcept(const LLWearable* pWearable, const LLUUID& idRlvObj) const;

	// NOTE: isLockedWearableType doesn't check if a worn wearable is a specific wearable lock so don't let these be called by the outside
protected:
	// Returns TRUE if the wearable type is eLock type locked
	bool isLockedWearableType(LLWearableType::EType eType, ERlvLockMask eLock) const;
	// Returns TRUE if the wearable type is eLock type locked by anything other than idRlvObj
	bool isLockedWearableTypeExcept(LLWearableType::EType eType, ERlvLockMask eLock, const LLUUID& idRlvObj) const;

	/*
	 * canWear/canRemove trivial helper functions (note that a more approriate name might be userCanWear/userCanRemove)
	 */
public:
	// Returns whether the inventory item can be worn by the user
	ERlvWearMask canWear(const LLViewerInventoryItem* pItem) const;
	// Returns whether the wearable type can be worn to by the user
	ERlvWearMask canWear(LLWearableType::EType eType) const;

	// Returns TRUE if the inventory item can be removed by the user
	bool canRemove(const LLInventoryItem* pItem) const;
	// Returns TRUE if the wearable type has at least one wearable that can be removed by the user
	bool canRemove(LLWearableType::EType eType) const;

	/*
	 * Member variables
	 */
public:
	typedef std::multimap<LLWearableType::EType, LLUUID> rlv_wearabletypelock_map_t;
	// Accessors for RlvFloaterLocks
	const rlv_wearabletypelock_map_t& getWearableTypeLocks(ERlvLockMask eLock) { return (RLV_LOCK_ADD == eLock) ? m_WearableTypeAdd : m_WearableTypeRem; }
protected:
	rlv_wearabletypelock_map_t m_WearableTypeAdd;
	rlv_wearabletypelock_map_t m_WearableTypeRem;
};

extern RlvWearableLocks gRlvWearableLocks;

// ============================================================================
// RlvFolderLocks class declaration
//

class RlvFolderLocks : public LLSingleton<RlvFolderLocks>
{
	friend class RlvLockedDescendentsCollector;
public:
	RlvFolderLocks();

	// Specifies the source of a folder lock
	enum ELockSourceType
	{ 
		ST_ATTACHMENT = 0x01, ST_ATTACHMENTPOINT = 0x02, ST_FOLDER = 0x04, ST_ROOTFOLDER = 0x08,
		ST_SHAREDPATH = 0x10, ST_WEARABLETYPE = 0x20, ST_NONE= 0x00, ST_MASK_ANY = 0xFF
	};
	typedef boost::variant<LLUUID, std::string, S32, LLWearableType::EType> lock_source_t;
	typedef std::pair<ELockSourceType, lock_source_t> folderlock_source_t;
	// Specifies options for the folder lock
	enum ELockPermission { PERM_ALLOW = 0x1, PERM_DENY = 0x2, PERM_MASK_ANY = 0x3 };
	enum ELockScope	{ SCOPE_NODE, SCOPE_SUBTREE } ;

	struct folderlock_descr_t
	{
		LLUUID				idRlvObj;
		ERlvLockMask		eLockType;
		folderlock_source_t	lockSource;
		ELockPermission		eLockPermission;
		ELockScope			eLockScope;

		folderlock_descr_t(const LLUUID& rlvObj, ERlvLockMask lockType, folderlock_source_t source, ELockPermission perm, ELockScope scope);
		bool operator ==(const folderlock_descr_t& rhs) const;
	};

public:
	// Adds an eLock type lock (held by idRlvObj) for the specified folder source (with ePerm and eScope lock options)
	void addFolderLock(const folderlock_source_t& lockSource, ELockPermission ePerm, ELockScope eScope, const LLUUID& idRlvObj, ERlvLockMask eLockType);

	// Returns TRUE if there is at least 1 non-detachable attachment as a result of a RLV_LOCK_REMOVE folder PERM_DENY lock
	bool hasLockedAttachment() const;
	// Returns TRUE if there is at least 1 eLock type PERM_DENY locked folder (RLV_LOCK_ANY = RLV_LOCK_ADD *or* RLV_LOCK_REMOVE)
	bool hasLockedFolder(ERlvLockMask eLockTypeMask) const;
	// Returns TRUE if the folder has a descendent folder lock with the specified charateristics
	bool hasLockedFolderDescendent(const LLUUID& idFolder, int eSourceTypeMask, ELockPermission ePermMask, 
	                               ERlvLockMask eLockTypeMask, bool fCheckSelf) const;
	// Returns TRUE if there is at least 1 non-removable wearable as a result of a RLV_LOCK_REMOVE folder PERM_DENY lock
	bool hasLockedWearable() const;
	// Returns TRUE if the attachment (specified by item UUID) is non-detachable as a result of a RLV_LOCK_REMOVE folder PERM_DENY lock
	bool isLockedAttachment(const LLUUID& idItem) const;
	// Returns TRUE if the folder is locked as a result of a RLV_LOCK_REMOVE folder PERM_DENY lock
	bool isLockedFolder(LLUUID idFolder, ERlvLockMask eLock, int eSourceTypeMask = ST_MASK_ANY, folderlock_source_t* plockSource = NULL) const;
	// Returns TRUE if the wearable (specified by item UUID) is non-removable as a result of a RLV_LOCK_REMOVE folder PERM_DENY lock
	bool isLockedWearable(const LLUUID& idItem) const;

	// Removes an eLock type lock (held by idRlvObj) for the specified folder source (with ePerm and eScope lock options)
	void removeFolderLock(const folderlock_source_t& lockSource, ELockPermission ePerm, ELockScope eScope, const LLUUID& idRlvObj, ERlvLockMask eLockType);

protected:
	// Returns TRUE if the folder has an explicit folder lock entry with the specified charateristics
	bool isLockedFolderEntry(const LLUUID& idFolder, int eSourceTypeMask, ELockPermission ePermMask, ERlvLockMask eLockTypeMask) const;

	/*
	 * canXXX helper functions (note that a more approriate name might be userCanXXX)
	 */
public:
	bool canMoveFolder(const LLUUID& idFolder, const LLUUID& idFolderDest) const;
	bool canRemoveFolder(const LLUUID& idFolder) const;
	bool canRenameFolder(const LLUUID& idFolder) const;
	bool canMoveItem(const LLUUID& idItem, const LLUUID& idFolderDest) const;
	bool canRemoveItem(const LLUUID& idItem) const;
	bool canRenameItem(const LLUUID& idItem) const;

	/*
	 * Cached item/folder look-up helper functions
	 */
protected:
	bool getLockedFolders(const folderlock_source_t& lockSource, LLInventoryModel::cat_array_t& lockFolders) const;
	bool getLockedItems(const LLUUID& idFolder, LLInventoryModel::item_array_t& lockItems) const;
	void onNeedsLookupRefresh();
	void refreshLockedLookups() const;

	/*
	 * Member variables
	 */
public:
	typedef std::list<const folderlock_descr_t*> folderlock_list_t;
	// Accessors for RlvFloaterLocks
	const folderlock_list_t& getFolderLocks() { return m_FolderLocks; }
	const uuid_vec_t& getAttachmentLookups()  { return m_LockedAttachmentRem; }
	const uuid_vec_t& getWearableLookups()    { return m_LockedWearableRem; }
protected:
	// Map of folder locks (idRlvObj -> lockDescr)
	folderlock_list_t	m_FolderLocks;			// List of add and remove locked folder descriptions
	S32					m_cntLockAdd;			// Number of RLV_LOCK_ADD locked folders in m_FolderLocks
	S32					m_cntLockRem;			// Number of RLV_LOCK_REMOVE locked folders in m_FolderLocks

	// Cached item look-up variables
	typedef std::multimap<LLUUID, const folderlock_descr_t*> folderlock_map_t;
	mutable bool				m_fLookupDirty;
	mutable bool				m_fLockedRoot;
	mutable uuid_vec_t			m_LockedAttachmentRem;
	mutable folderlock_map_t	m_LockedFolderMap;
	mutable uuid_vec_t			m_LockedWearableRem;
private:
	friend class LLSingleton<RlvFolderLocks>;
};

// ============================================================================
// RlvAttachPtLookup inlined member functions
//

// Checked: 2010-11-30 (RLVa-1.4.0b) | Added: RLVa-1.4.0b
inline LLViewerJointAttachment* RlvAttachPtLookup::getAttachPoint(S32 idxAttachPt)
{
	return (isAgentAvatarValid()) ? get_if_there(gAgentAvatarp->mAttachmentPoints, idxAttachPt, (LLViewerJointAttachment*)NULL) : NULL;
}

// Checked: 2010-03-03 (RLVa-1.2.0a) | Modified: RLVa-0.2.0d
inline LLViewerJointAttachment* RlvAttachPtLookup::getAttachPoint(const std::string& strText)
{
	return (isAgentAvatarValid()) ? get_if_there(gAgentAvatarp->mAttachmentPoints, getAttachPointIndex(strText), (LLViewerJointAttachment*)NULL) : NULL;
}

// Checked: 2010-03-03 (RLVa-1.2.0a) | Modified: RLVa-1.0.1b
inline LLViewerJointAttachment* RlvAttachPtLookup::getAttachPoint(const LLInventoryItem* pItem)
{
	return (isAgentAvatarValid()) ? get_if_there(gAgentAvatarp->mAttachmentPoints, getAttachPointIndex(pItem), (LLViewerJointAttachment*)NULL) : NULL;
}

// Checked: 2010-03-03 (RLVa-1.2.0a) | Modified: RLVa-1.2.0a
inline S32 RlvAttachPtLookup::getAttachPointIndex(std::string strText)
{
	LLStringUtil::toLower(strText);
	RLV_ASSERT(m_AttachPtLookupMap.size() > 0);
	std::map<std::string, S32>::const_iterator itAttachPt = m_AttachPtLookupMap.find(strText);
	return (itAttachPt != m_AttachPtLookupMap.end()) ? itAttachPt->second : 0;
}

// Checked: 2010-03-03 (RLVa-1.2.0a) | Modified: RLVa-0.2.0d
inline S32 RlvAttachPtLookup::getAttachPointIndex(const LLViewerObject* pObj)
{
	return (pObj) ? ATTACHMENT_ID_FROM_STATE(pObj->getState()) : 0;
}

// ============================================================================
// RlvAttachmentLocks inlined member functions
//

// Checked: 2011-05-22 (RLVa-1.3.1b) | Modified: RLVa-1.3.1b
inline ERlvWearMask RlvAttachmentLocks::canAttach(const LLInventoryItem* pItem, LLViewerJointAttachment** ppAttachPtOut /*=NULL*/) const
{
	// The specified item can be attached if:
	//   - it doesn't specify an attachment point and there is at least one attachment point that can be attached to
	//   - the attachment point it specifies can be attached to
	LLViewerJointAttachment* pAttachPt = RlvAttachPtLookup::getAttachPoint(pItem);
	if (ppAttachPtOut)
		*ppAttachPtOut = pAttachPt;
	return ((canAttach()) && (pItem) && (!RlvFolderLocks::instance().isLockedFolder(pItem->getParentUUID(), RLV_LOCK_ADD)))
		? ((!pAttachPt) ? RLV_WEAR : canAttach(pAttachPt)) : RLV_WEAR_LOCKED;
}

// Checked: 2010-08-07 (RLVa-1.2.0i) | Modified: RLVa-1.2.0i
inline ERlvWearMask RlvAttachmentLocks::canAttach(const LLViewerJointAttachment* pAttachPt) const
{
	// Non-attachable attachment point  => RLV_WEAR_LOCKED
	// One or more locked attachment(s) => RLV_WEAR_ADD
	// Unlocked attachment(s)           => RLV_WEAR_ADD | RLV_WEAR_REPLACE
	// Empty attachment point           => RLV_WEAR_ADD | RLV_WEAR_REPLACE
	RLV_ASSERT(pAttachPt);	// TODO-RLVa: [RLVa-1.2.1] Maybe it's better to just return something similar like above?
	return 
		(ERlvWearMask)(((pAttachPt) && (!isLockedAttachmentPoint(pAttachPt, RLV_LOCK_ADD))) 
			? ((canDetach(pAttachPt, true)) ? RLV_WEAR_REPLACE : 0) | RLV_WEAR_ADD
			: RLV_WEAR_LOCKED);
}

// Checked: 2010-02-28 (RLVa-1.2.0a) | Added: RLVa-1.0.5a
inline bool RlvAttachmentLocks::canDetach(const LLInventoryItem* pItem) const
{
	const LLViewerObject* pAttachObj = 
		((pItem) && (isAgentAvatarValid())) ? gAgentAvatarp->getWornAttachment(pItem->getLinkedUUID()) : NULL;
	return (pAttachObj) && (!isLockedAttachment(pAttachObj));
}

// Checked: 2010-11-30 (RLVa-1.3.0b) | Modified: RLVa-1.3.0b
inline bool RlvAttachmentLocks::hasLockedAttachmentPoint(ERlvLockMask eLock) const
{
	// Remove locks are more common so check those first
	return
		((eLock & RLV_LOCK_REMOVE) && ((!m_AttachPtRem.empty()) || (!m_AttachObjRem.empty()) || (RlvFolderLocks::instance().hasLockedAttachment()))) || 
		((eLock & RLV_LOCK_ADD) && (!m_AttachPtAdd.empty()) );
}

// Checked: 2010-11-30 (RLVa-1.3.0b) | Modified: RLVa-1.3.0b
inline bool RlvAttachmentLocks::isLockedAttachment(const LLViewerObject* pAttachObj) const
{
	// If pObj is valid then it should always specify a root since we store root UUIDs in m_AttachObjRem
	RLV_ASSERT( (!pAttachObj) || (pAttachObj == pAttachObj->getRootEdit()) );

	// Object is locked if:
	//   - it's specifically marked as non-detachable (ie @detach=n)
	//   - it's attached to an attachment point that is RLV_LOCK_REMOVE locked (ie @remattach:<attachpt>=n)
	//   - it's part of a locked folder
	return 
		(pAttachObj) && (pAttachObj->isAttachment()) &&
		( (m_AttachObjRem.find(pAttachObj->getID()) != m_AttachObjRem.end()) || 
		  (isLockedAttachmentPoint(RlvAttachPtLookup::getAttachPointIndex(pAttachObj), RLV_LOCK_REMOVE)) ||
		  (RlvFolderLocks::instance().isLockedAttachment(pAttachObj->getAttachmentItemID())) );
}

// Checked: 2010-02-28 (RLVa-1.2.0a) | Added: RLVa-1.0.5a
inline bool RlvAttachmentLocks::isLockedAttachmentPoint(S32 idxAttachPt, ERlvLockMask eLock) const
{
	return
		( (eLock & RLV_LOCK_REMOVE) && (m_AttachPtRem.find(idxAttachPt) != m_AttachPtRem.end()) ) ||
		( (eLock & RLV_LOCK_ADD) && (m_AttachPtAdd.find(idxAttachPt) != m_AttachPtAdd.end()) );
}

// Checked: 2010-02-28 (RLVa-1.2.0a) | Modified: RLVa-1.2.0a
inline bool RlvAttachmentLocks::isLockedAttachmentPoint(const LLViewerJointAttachment* pAttachPt, ERlvLockMask eLock) const
{
	return (pAttachPt) && (isLockedAttachmentPoint(RlvAttachPtLookup::getAttachPointIndex(pAttachPt), eLock));
}

// ============================================================================
// RlvWearableLocks inlined member functions
//

// Checked: 2010-03-19 (RLVa-1.2.0c) | Added: RLVa-1.2.0a
inline bool RlvWearableLocks::canRemove(const LLInventoryItem* pItem) const
{
	// The specified item can be removed if its wearable can be removed
	RLV_ASSERT( (pItem) && (LLInventoryType::IT_WEARABLE == pItem->getInventoryType()) );
	const LLWearable* pWearable = (pItem) ? gAgentWearables.getWearableFromItemID(pItem->getLinkedUUID()) : NULL;
	return (pWearable) && (!isLockedWearable(pWearable));
}

// Checked: 2011-03-27 (RLVa-1.3.0g) | Modified: RLVa-1.3.0g
inline ERlvWearMask RlvWearableLocks::canWear(const LLViewerInventoryItem* pItem) const
{
	// The specified item can be worn if the wearable type it specifies can be worn on
	RLV_ASSERT( (pItem) && (LLInventoryType::IT_WEARABLE == pItem->getInventoryType()) );
	return ((pItem) && (!RlvFolderLocks::instance().isLockedFolder(pItem->getParentUUID(), RLV_LOCK_ADD))) 
		? canWear(pItem->getWearableType()) : RLV_WEAR_LOCKED;
}

// Checked: 2010-05-14 (RLVa-1.2.0g) | Modified: RLVa-1.2.0g
inline ERlvWearMask RlvWearableLocks::canWear(LLWearableType::EType eType) const
{
	// The specified wearable type can be worn on if:
	//   - the wearable type itself isn't RLV_LOCK_ADD locked => RLV_WEAR_ADD
	//     (and there are less than the maximum amount currently worn)
	//   - it doesn't have any non-removable wearables        => RLV_WEAR_REPLACE | RLV_WEAR_ADD = RLV_WEAR
	// TODO-RLVa: [RLVa-1.2.1] We don't have the ability to lock a specific wearable yet so rewrite this when we do
	return (!isLockedWearableType(eType, RLV_LOCK_ADD))
	  ? ((!hasLockedWearable(eType)) 
			? RLV_WEAR 
			: (gAgentWearables.getWearableCount(eType) < LLAgentWearables::MAX_CLOTHING_PER_TYPE) ? RLV_WEAR_ADD : RLV_WEAR_LOCKED)
	  : RLV_WEAR_LOCKED;
}

// Checked: 2010-03-18 (RLVa-1.2.0c) | Modified: RLVa-1.2.0a
inline bool RlvWearableLocks::hasLockedWearableType(ERlvLockMask eLock) const
{
	// Remove locks are more common so check those first
	// TODO-RLVa: [RLVa-1.2.1] We don't have the ability to lock a specific wearable yet so rewrite this when we do
	return ( (eLock & RLV_LOCK_REMOVE) && (!m_WearableTypeRem.empty()) ) || ( (eLock & RLV_LOCK_ADD) && (!m_WearableTypeAdd.empty()) );
}

// Checked: 2010-11-30 (RLVa-1.3.0b) | Modified: RLVa-1.2.0a
inline bool RlvWearableLocks::isLockedWearable(const LLWearable* pWearable) const
{
	// Wearable is locked if:
	//   - it's specifically marked as non-removable
	//   - it's worn on a wearable type that is RLV_LOCK_REMOVE locked
	//   - it's part of a locked folder
	// TODO-RLVa: [RLVa-1.2.1] We don't have the ability to lock a specific wearable yet so rewrite this when we do
	RLV_ASSERT(pWearable);
	return 
		(pWearable) &&
		( (isLockedWearableType(pWearable->getType(), RLV_LOCK_REMOVE)) || (RlvFolderLocks::instance().isLockedWearable(pWearable->getItemID())) );
}

// Checked: 2010-03-19 (RLVa-1.2.0c) | Added: RLVa-1.2.0a
inline bool RlvWearableLocks::isLockedWearableType(LLWearableType::EType eType, ERlvLockMask eLock) const
{
	return
		( (eLock & RLV_LOCK_REMOVE) && (m_WearableTypeRem.find(eType) != m_WearableTypeRem.end()) ) ||
		( (eLock & RLV_LOCK_ADD) && (m_WearableTypeAdd.find(eType) != m_WearableTypeAdd.end()) );
}

// ============================================================================
// RlvAttachmentLockWatchdog inlined member functions
//

// Checked: 2010-08-07 (RLVa-1.2.0i) | Added: RLVa-1.2.0i
inline void RlvAttachmentLockWatchdog::onWearAttachment(const LLInventoryItem* pItem, ERlvWearMask eWearAction)
{
	onWearAttachment(pItem->getLinkedUUID(), eWearAction);
}

// ============================================================================
// RlvFolderLocks member functions
//

// Checked: 2011-03-27 (RLVa-1.3.0g) | Added: RLVa-1.3.0g
inline RlvFolderLocks::folderlock_descr_t::folderlock_descr_t(const LLUUID& rlvObj, ERlvLockMask lockType, folderlock_source_t source, 
															  ELockPermission perm, ELockScope scope)
	: idRlvObj(rlvObj), eLockType(lockType), lockSource(source), eLockPermission(perm), eLockScope(scope)
{
}

// Checked: 2011-03-27 (RLVa-1.3.0g) | Added: RLVa-1.3.0g
inline bool RlvFolderLocks::folderlock_descr_t::operator ==(const folderlock_descr_t& rhs) const
{
	return (idRlvObj == rhs.idRlvObj) && (eLockType == rhs.eLockType) && (lockSource == rhs.lockSource) && 
		(eLockPermission == rhs.eLockPermission) && (eLockScope == rhs.eLockScope);
}

// Checked: 2011-03-29 (RLVa-1.3.0g) | Added: RLVa-1.3.0g
inline bool RlvFolderLocks::canMoveFolder(const LLUUID& idFolder, const LLUUID& idFolderDest) const
{
	// Block moving the folder to destination if:
	//   - the folder (or one of its descendents) is explicitly locked
	//   - folder and destination are subject to different locks
	//		-> Possible combinations:
	//			* folder   locked + destination unlocked => block move
	//			* folder unlocked + destination   locked => block move
	//			* folder   locked + destination   locked => allow move only if both are subject to the same folder lock
	//			* folder unlocked + destination unlocked => allow move (special case of above since both locks are equal when there is none)
	//		=> so the above becomes (isLockedFolder(A) == isLockedFolder(B)) && (lockA == lockB)
	folderlock_source_t lockSource(ST_NONE, 0), lockSourceDest(ST_NONE, 0);
	return 
		(!hasLockedFolderDescendent(idFolder, ST_MASK_ANY, PERM_MASK_ANY, RLV_LOCK_ANY, true)) &&
		( (isLockedFolder(idFolder, RLV_LOCK_ANY, ST_MASK_ANY, &lockSource) == isLockedFolder(idFolderDest, RLV_LOCK_ANY, ST_MASK_ANY, &lockSourceDest)) && 
		  (lockSource == lockSourceDest) );
}

// Checked: 2011-03-29 (RLVa-1.3.0g) | Added: RLVa-1.3.0g
inline bool RlvFolderLocks::canRemoveFolder(const LLUUID& idFolder) const
{
	// Block removing a folder if:
	//   - the folder (or one of its descendents) is explicitly locked
	//   - the folder itself is locked (but disregard root folder locks)
	return 
		(!hasLockedFolderDescendent(idFolder, ST_MASK_ANY, PERM_MASK_ANY, RLV_LOCK_ANY, true)) && 
		(!isLockedFolder(idFolder, RLV_LOCK_ANY, ST_MASK_ANY & ~ST_ROOTFOLDER));
}

// Checked: 2011-03-29 (RLVa-1.3.0g) | Added: RLVa-1.3.0g
inline bool RlvFolderLocks::canRenameFolder(const LLUUID& idFolder) const
{
	// Block renaming a folder if:
	//   - the folder (or one of its descendents) is explicitly locked by:
	//		-> a "shared path" => renaming the folder would change the shared path and hence invalidate the lock
	//		-> an attachment point -|
	//		-> an attachment        |--> renaming the folder to a "dot" (=invisible) folder would invalidate the lock
	//		-> a wearable type     -|
	return !hasLockedFolderDescendent(idFolder, ST_SHAREDPATH | ST_ATTACHMENT | ST_ATTACHMENTPOINT | ST_WEARABLETYPE, PERM_MASK_ANY, RLV_LOCK_ANY, true);
}

// Checked: 2011-03-30 (RLVa-1.3.0g) | Added: RLVa-1.3.0g
inline bool RlvFolderLocks::canMoveItem(const LLUUID& idItem, const LLUUID& idFolderDest) const
{
	// Block moving the folder to destination if:
	//   - folder and destination are subject to different locks [see canMoveFolder() for more details]
	const LLViewerInventoryItem* pItem = gInventory.getItem(idItem); const LLUUID& idFolder = (pItem) ? pItem->getParentUUID() : LLUUID::null;
	int maskSource = ST_MASK_ANY & ~ST_ROOTFOLDER; folderlock_source_t lockSource(ST_NONE, 0), lockSourceDest(ST_NONE, 0);
	return 
		(idFolder.notNull()) && 
		(isLockedFolder(idFolder, RLV_LOCK_ANY, maskSource, &lockSource) == isLockedFolder(idFolderDest, RLV_LOCK_ANY, maskSource, &lockSourceDest)) && 
		(lockSource == lockSourceDest);
}

// Checked: 2011-03-30 (RLVa-1.3.0g) | Added: RLVa-1.3.0g
inline bool RlvFolderLocks::canRemoveItem(const LLUUID& idItem) const
{
	// Block removing items from locked folders (but disregard root folder locks)
	const LLViewerInventoryItem* pItem = gInventory.getItem(idItem); const LLUUID& idFolder = (pItem) ? pItem->getParentUUID() : LLUUID::null;
	int maskSource = ST_MASK_ANY & ~ST_ROOTFOLDER;
	return (idFolder.notNull()) && (!isLockedFolder(idFolder, RLV_LOCK_ANY, maskSource));
}

// Checked: 2011-03-30 (RLVa-1.3.0g) | Added: RLVa-1.3.0g
inline bool RlvFolderLocks::canRenameItem(const LLUUID& idItem) const
{
	// Items can always be renamed, regardless of folder locks
	return true;
}

// Checked: 2010-11-30 (RLVa-1.3.0g) | Added: RLVa-1.3.0b
inline bool RlvFolderLocks::hasLockedAttachment() const
{
	if (m_fLookupDirty)
		refreshLockedLookups();
	return !m_LockedAttachmentRem.empty();
}

// Checked: 2011-03-27 (RLVa-1.3.0g) | Modified: RLVa-1.3.0g
inline bool RlvFolderLocks::hasLockedFolder(ERlvLockMask eLock) const
{
	// Remove locks are more common so check those first
	return ((eLock & RLV_LOCK_REMOVE) && (m_cntLockRem)) || ((eLock & RLV_LOCK_ADD) && (m_cntLockAdd));
}

// Checked: 2010-11-30 (RLVa-1.3.0g) | Added: RLVa-1.3.0b
inline bool RlvFolderLocks::hasLockedWearable() const
{
	if (m_fLookupDirty)
		refreshLockedLookups();
	return !m_LockedWearableRem.empty();
}

// Checked: 2010-11-30 (RLVa-1.3.0g) | Added: RLVa-1.3.0b
inline bool RlvFolderLocks::isLockedAttachment(const LLUUID& idItem) const
{
	if (m_fLookupDirty)
		refreshLockedLookups();
	return (std::find(m_LockedAttachmentRem.begin(), m_LockedAttachmentRem.end(), idItem) != m_LockedAttachmentRem.end());
}

// Checked: 2010-11-30 (RLVa-1.3.0g) | Added: RLVa-1.3.0b
inline bool RlvFolderLocks::isLockedWearable(const LLUUID& idItem) const
{
	if (m_fLookupDirty)
		refreshLockedLookups();
	return (std::find(m_LockedWearableRem.begin(), m_LockedWearableRem.end(), idItem) != m_LockedWearableRem.end());
}

// ============================================================================

#endif // RLV_LOCKS_H
