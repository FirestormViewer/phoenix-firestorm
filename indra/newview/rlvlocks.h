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

#ifndef RLV_LOCKS_H
#define RLV_LOCKS_H

#include "llagentwearables.h"
#include "lleventtimer.h"

#include "rlvdefines.h"
#include "rlvcommon.h"

// ============================================================================
// RlvAttachPtLookup class declaration
//

#include "llagent.h"
#include "llviewerjointattachment.h"
#include "llviewerobject.h"
#include "llvoavatarself.h"

#include "rlvdefines.h"

class RlvAttachPtLookup
{
public:
	static LLViewerJointAttachment* getAttachPoint(const std::string& strText);
	static LLViewerJointAttachment* getAttachPoint(const LLInventoryItem* pItem);

	static S32 getAttachPointIndex(std::string strText);
	static S32 getAttachPointIndex(const LLViewerObject* pObj);
	static S32 getAttachPointIndex(const LLViewerJointAttachment* pObj);
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
	bool isLockedAttachment(const LLViewerObject* pObj) const;
	bool isLockedAttachment(const LLInventoryItem* pItem) const;
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
	// Returns TRUE if the inventory item can be attached by the user (and optionally provides the attachment point - which may be NULL)
	ERlvWearMask canAttach(const LLInventoryItem* pItem, LLViewerJointAttachment** ppAttachPtOut = NULL) const;
	// Returns TRUE if the attachment point can be attached to by the user
	ERlvWearMask canAttach(const LLViewerJointAttachment* pAttachPt) const;

	// Returns TRUE if the inventory item can be detached by the user
	bool canDetach(const LLInventoryItem* pItem) const { return !isLockedAttachment(pItem); }
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
	void detach(S32 idxAttachPt, const LLViewerObject* pAttachObjExcept = NULL);

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

		LLUUID       idItem;
		ERlvWearMask eWearAction;
		F64          tsWear;
		std::map<S32, std::list<LLUUID> > attachPts;
	protected:
		RlvWearInfo();
	};
	typedef std::map<LLUUID, RlvWearInfo> rlv_wear_map_t;
	rlv_wear_map_t   m_PendingWear;

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
	bool isLockedWearable(const LLInventoryItem* pItem) const;
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
// RlvAttachPtLookup inlined member functions
//

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

// Checked: 2010-08-07 (RLVa-1.2.0i) | Modified: RLVa-1.2.0i
inline ERlvWearMask RlvAttachmentLocks::canAttach(const LLInventoryItem* pItem, LLViewerJointAttachment** ppAttachPtOut /*=NULL*/) const
{
	// The specified item can be attached if:
	//   - it doesn't specify an attachment point
	//   - the attachment point it specifies can be attached to
	LLViewerJointAttachment* pAttachPt = RlvAttachPtLookup::getAttachPoint(pItem);
	if (ppAttachPtOut)
		*ppAttachPtOut = pAttachPt;
	return (!pAttachPt) ? RLV_WEAR : canAttach(pAttachPt);
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

// Checked: 2010-02-28 (RLVa-1.2.0a) | Modified: RLVa-1.2.0a
inline bool RlvAttachmentLocks::hasLockedAttachmentPoint(ERlvLockMask eLock) const
{
	// Remove locks are more common so check those first
	return
		( (eLock & RLV_LOCK_REMOVE) && ((!m_AttachPtRem.empty()) || (!m_AttachObjRem.empty())) ) || 
		( (eLock & RLV_LOCK_ADD) && (!m_AttachPtAdd.empty()) );
}

// Checked: 2010-02-28 (RLVa-1.2.0a) | Modified: RLVa-1.2.0a
inline bool RlvAttachmentLocks::isLockedAttachment(const LLViewerObject* pObj) const
{
	// If pObj is valid then it should always specify a root since we store root UUIDs in m_AttachObjRem
	RLV_ASSERT( (!pObj) || (pObj == pObj->getRootEdit()) );

	// Object is locked if:
	//   - it's specifically marked as non-detachable (ie @detach=n)
	//   - it's attached to an attachment point that is RLV_LOCK_REMOVE locked (ie @remattach:<attachpt>=n)
	return 
		(pObj) && (pObj->isAttachment()) &&
		( (m_AttachObjRem.find(pObj->getID()) != m_AttachObjRem.end()) || 
		  (isLockedAttachmentPoint(RlvAttachPtLookup::getAttachPointIndex(pObj), RLV_LOCK_REMOVE)) );
}

// Checked: 2010-02-28 (RLVa-1.2.0a) | Added: RLVa-1.0.5a
inline bool RlvAttachmentLocks::isLockedAttachment(const LLInventoryItem* pItem) const
{
	return (pItem) && (isAgentAvatarValid()) && (isLockedAttachment(gAgentAvatarp->getWornAttachment(pItem->getLinkedUUID())));
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
	return (pItem) ? !isLockedWearable(gAgentWearables.getWearableFromItemID(pItem->getLinkedUUID())) : false;
}

// Checked: 2010-05-14 (RLVa-1.2.0g) | Modified: RLVa-1.2.0g
inline ERlvWearMask RlvWearableLocks::canWear(const LLViewerInventoryItem* pItem) const
{
	// The specified item can be worn if the wearable type it specifies can be worn on
	RLV_ASSERT( (pItem) && (LLInventoryType::IT_WEARABLE == pItem->getInventoryType()) );
	return (pItem) ? canWear(pItem->getWearableType()) : RLV_WEAR_LOCKED;
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

// Checked: 2010-03-19 (RLVa-1.2.0c) | Added: RLVa-1.2.0a
inline bool RlvWearableLocks::isLockedWearable(const LLWearable* pWearable) const
{
	// Wearable is locked if:
	//   - it's specifically marked as non-removable
	//   - it's worn on a wearable type that is RLV_LOCK_REMOVE locked
	// TODO-RLVa: [RLVa-1.2.1] We don't have the ability to lock a specific wearable yet so rewrite this when we do
	RLV_ASSERT(pWearable);
	return (pWearable) && (isLockedWearableType(pWearable->getType(), RLV_LOCK_REMOVE));
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

#endif // RLV_LOCKS_H
