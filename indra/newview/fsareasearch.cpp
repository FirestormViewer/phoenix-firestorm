/**
 * @file fsareasearch.cpp
 * @brief Floater to search and list objects in view or is known to the viewer.
 *
 * $LicenseInfo:firstyear=2012&license=viewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (c) 2012 Techwolf Lupindo
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * The Phoenix Firestorm Project, Inc., 1831 Oakwood Drive, Fairmont, Minnesota 56031-3225 USA
 * http://www.firestormviewer.org
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "fsareasearch.h"

#include "llavatarnamecache.h"
#include "llscrolllistctrl.h"
#include "lllineeditor.h"
#include "lltextbox.h"
#include "llfloaterreg.h"
#include "llagent.h"
#include "lltracker.h"
#include "llviewerobjectlist.h"
#include "llviewercontrol.h"
#include "llviewerparcelmgr.h"
#include <boost/algorithm/string/find.hpp> //for boost::ifind_first
#include "llviewerregion.h"
#include "llselectmgr.h"
#include "llcallbacklist.h"
#include "lltoolpie.h"
#include "llsaleinfo.h"
#include "llcheckboxctrl.h"
#include "llviewermenu.h"       // handle_object_touch(), handle_buy()
#include "lltabcontainer.h"
#include "llspinctrl.h"
#include "lltoolgrab.h"
#include "fslslbridge.h"
#include "llcombobox.h"
#include "llnotificationsutil.h"
#include "fsassetblacklist.h"
#include "llworld.h"
#include "lltrans.h"    // getString()
#include "llagentcamera.h" // gAgentCamera
#include "llviewerjoystick.h" // For disabling/re-enabling when requested to look at an object.
#include "llmoveview.h" // For LLPanelStandStopFlying::clearStandStopFlyingMode
#include "rlvactions.h"
#include "fsareasearchmenu.h"
#include "fsscrolllistctrl.h"
#include "llviewermediafocus.h"
#include "lltoolmgr.h"
#include "rlvhandler.h"

// max number of objects that can be (de-)selected in a single packet.
constexpr S32 MAX_OBJECTS_PER_PACKET = 255;

// time in seconds between refreshes when active
constexpr F32 REFRESH_INTERVAL = 1.0f;

// this is used to prevent refreshing too often and affecting performance.
constexpr F32 MIN_REFRESH_INTERVAL = 0.25f;

// how far the avatar needs to move to trigger a distance update
constexpr F32 MIN_DISTANCE_MOVED = 1.0f;

// timeout to resend object properties request again
constexpr F32 REQUEST_TIMEOUT = 30.0f;

static std::string RLVa_hideNameIfRestricted(std::string_view name)
{
    if (!gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES))
        return std::string(name);
    else
        return RlvStrings::getAnonym(std::string(name));
}

F32 calculateObjectDistance(LLVector3d agent_pos, LLViewerObject* object)
{
    if (object->isHUDAttachment())
    {
        return 0.f;
    }
    else
    {
        return (F32)dist_vec(agent_pos, object->getPositionGlobal());
    }
}

class FSAreaSearch::FSParcelChangeObserver : public LLParcelObserver
{
public:
    FSParcelChangeObserver(FSAreaSearch* area_search_floater) : mAreaSearchFloater(area_search_floater) {}

private:
    /*virtual*/ void changed()
    {
        if (mAreaSearchFloater)
        {
            mAreaSearchFloater->checkRegion();
        }
    }

    FSAreaSearch* mAreaSearchFloater;
};

class FSAreaSearchTouchTimer : public LLEventTimer
{
public:
    FSAreaSearchTouchTimer(const LLUUID& object_id, F32 timeout) :
        LLEventTimer(timeout),
        mObjectID(object_id)
    {
    }

    bool tick() override
    {
        LLViewerObject* objectp = gObjectList.findObject(mObjectID);
        if (objectp)
        {
            FSPanelAreaSearchList::touchObject(objectp);
        }

        return true;
    }

private:
    LLUUID  mObjectID;
};

FSAreaSearch::FSAreaSearch(const LLSD& key) :
    LLFloater(key),
    mActive(false),
    mFilterForSale(false),
    mFilterForSaleMin(0),
    mFilterForSaleMax(999999),
    mFilterPhysical(false),
    mFilterTemporary(false),
    mRegexSearch(false),
    mFilterClickAction(false),
    mFilterLocked(false),
    mFilterPhantom(false),
    mFilterAttachment(false),
    mFilterMoaP(false),
    mFilterReflectionProbe(false),
    mFilterDistance(false),
    mFilterDistanceMin(0),
    mFilterDistanceMax(999999),
    mFilterPermCopy(false),
    mFilterPermModify(false),
    mFilterPermTransfer(false),
    mFilterAgentParcelOnly(false),
    mBeacons(false),
    mExcludeAttachment(true),
    mExcludeTemporary(true),
    mExcludeReflectionProbe(false),
    mExcludePhysics(true),
    mExcludeChildPrims(true),
    mExcludeNeighborRegions(true),
    mRequestQueuePause(false),
    mRequestNeedsSent(false),
    mRlvBehaviorCallbackConnection()
{
    gAgent.setFSAreaSearchActive(true);
    gAgent.changeInterestListMode(IL_MODE_360);
    mFactoryMap["area_search_list_panel"] = LLCallbackMap(createPanelList, this);
    mFactoryMap["area_search_find_panel"] = LLCallbackMap(createPanelFind, this);
    mFactoryMap["area_search_filter_panel"] = LLCallbackMap(createPanelFilter, this);
    mFactoryMap["area_search_advanced_panel"] = LLCallbackMap(createPanelAdvanced, this);
    mFactoryMap["area_search_options_panel"] = LLCallbackMap(createPanelOptions, this);

    // Register an idle update callback
    gIdleCallbacks.addFunction(idle, this);

    mParcelChangedObserver = std::make_unique<FSParcelChangeObserver>(this);
    LLViewerParcelMgr::getInstance()->addObserver(mParcelChangedObserver.get());
}

FSAreaSearch::~FSAreaSearch()
{
    gAgent.setFSAreaSearchActive(false);

    // Tell the Simulator not to send us everything anymore
    // and revert to the regular "keyhole" frustum of interest
    // list updates.
    if( !LLApp::isExiting() )
    {
        gAgent.changeInterestListMode(IL_MODE_DEFAULT);
    }

    if (!gIdleCallbacks.deleteFunction(idle, this))
    {
        LL_WARNS("FSAreaSearch") << "FSAreaSearch::~FSAreaSearch() failed to delete callback" << LL_ENDL;
    }

    if (mRlvBehaviorCallbackConnection.connected())
    {
        mRlvBehaviorCallbackConnection.disconnect();
    }

    for (const auto& cb : mNameCacheConnections)
    {
        if (cb.second.connected())
        {
            cb.second.disconnect();
        }
    }
    mNameCacheConnections.clear();

    if (mParcelChangedObserver)
    {
        LLViewerParcelMgr::getInstance()->removeObserver(mParcelChangedObserver.get());
        mParcelChangedObserver = nullptr;
    }
}

bool FSAreaSearch::postBuild()
{
    mTab = getChild<LLTabContainer>("area_searchtab");

    if (!gSavedSettings.getBOOL("FSAreaSearchAdvanced"))
    {
        LLPanel* advanced_tab = mTab->getPanelByName("area_search_advanced_panel");
        if (advanced_tab)
        {
            mTab->removeTabPanel(advanced_tab);
        }
    }

    mRlvBehaviorCallbackConnection = gRlvHandler.setBehaviourCallback(boost::bind(&FSAreaSearch::updateRlvRestrictions, this, _1));

    return LLFloater::postBuild();
}

void FSAreaSearch::onOpen(const LLSD& key)
{
    mTab->selectTab(1);
}

void FSAreaSearch::draw()
{
    LLFloater::draw();

    static LLCachedControl<S32> beacon_line_width(gSavedSettings, "DebugBeaconLineWidth");
    static LLUIColor mBeaconColor = LLUIColorTable::getInstance()->getColor("AreaSearchBeaconColor");
    static LLUIColor mBeaconTextColor = LLUIColorTable::getInstance()->getColor("PathfindingDefaultBeaconTextColor");

    if (mBeacons)
    {
        std::vector<LLScrollListItem*> items = mPanelList->getResultList()->getAllData();

        for (const auto item : items)
        {
            if (LLViewerObject* objectp = gObjectList.findObject(item->getUUID()); objectp)
            {
                const std::string& objectName = mObjectDetails[item->getUUID()].description;
                gObjectList.addDebugBeacon(objectp->getPositionAgent(), objectName, mBeaconColor, mBeaconTextColor, beacon_line_width);
            }
        }
    }
}

//static
void FSAreaSearch::idle(void* user_data)
{
    FSAreaSearch* self = (FSAreaSearch*)user_data;
    self->findObjects();
    self->processRequestQueue();
}

// static
void* FSAreaSearch::createPanelList(void* data)
{
    FSAreaSearch* self = (FSAreaSearch*)data;
    self->mPanelList = new FSPanelAreaSearchList(self);
    return self->mPanelList;
}

// static
void* FSAreaSearch::createPanelFind(void* data)
{
    FSAreaSearch* self = (FSAreaSearch*)data;
    self->mPanelFind = new FSPanelAreaSearchFind(self);
    return self->mPanelFind;
}

// static
void* FSAreaSearch::createPanelFilter(void* data)
{
    FSAreaSearch* self = (FSAreaSearch*)data;
    self->mPanelFilter = new FSPanelAreaSearchFilter(self);
    return self->mPanelFilter;
}

// static
void* FSAreaSearch::createPanelAdvanced(void* data)
{
    FSAreaSearch* self = (FSAreaSearch*)data;
    self->mPanelAdvanced = new FSPanelAreaSearchAdvanced();
    return self->mPanelAdvanced;
}

// static
void* FSAreaSearch::createPanelOptions(void* data)
{
    FSAreaSearch* self = (FSAreaSearch*)data;
    self->mPanelOptions = new FSPanelAreaSearchOptions(self);
    return self->mPanelOptions;
}

void FSAreaSearch::updateRlvRestrictions(ERlvBehaviour behavior)
{
    if (behavior == RLV_BHVR_SHOWNAMES)
    {
        refreshList(false);
    }
}

void FSAreaSearch::checkRegion()
{
    if (mActive)
    {
        // Check if we changed region, and if we did, clear the object details cache.
        if (LLViewerRegion* region = gAgent.getRegion(); region && (region != mLastRegion))
        {
            if (!mExcludeNeighborRegions)
            {
                std::vector<LLViewerRegion*> uniqueRegions;
                region->getNeighboringRegions(uniqueRegions);
                if (std::find(uniqueRegions.begin(), uniqueRegions.end(), mLastRegion) != uniqueRegions.end())
                {
                    // Crossed into a neighboring region, no need to clear everything.
                    mLastRegion = region;
                    return;
                }
                // else teleported into a new region
            }
            mLastRegion = region;
            mRequested = 0;
            mObjectDetails.clear();
            mRegionRequests.clear();
            mLastPropertiesReceivedTimer.start();
            mPanelList->getResultList()->deleteAllItems();
            mPanelList->setCounterText();
            mPanelList->setAgentLastPosition(gAgent.getPositionGlobal());
            mRefresh = true;
        }
    }
}

void FSAreaSearch::refreshList(bool cache_clear)
{
    mActive = true;
    checkRegion();
    if (cache_clear)
    {
        mRequested = 0;
        mObjectDetails.clear();
        mRegionRequests.clear();
        mLastPropertiesReceivedTimer.start();
    }
    else
    {
        for (auto& object_it : mObjectDetails)
        {
             object_it.second.listed = false;
        }
    }
    mPanelList->getResultList()->deleteAllItems();
    mPanelList->setCounterText();
    mPanelList->setAgentLastPosition(gAgent.getPositionGlobal());
    mNamesRequested.clear();
    mRefresh = true;
    findObjects();
}

void FSAreaSearch::findObjects()
{
    // Only loop through the gObjectList every so often. There is a performance hit if done too often.
    if (!(mActive && ((mRefresh && mLastUpdateTimer.getElapsedTimeF32() > MIN_REFRESH_INTERVAL) || mLastUpdateTimer.getElapsedTimeF32() > REFRESH_INTERVAL)))
    {
        return;
    }

    LLViewerRegion* our_region = gAgent.getRegion();
    if (!our_region)
    {
        // Got disconnected or is in the middle of a teleport.
        return;
    }

    LL_DEBUGS("FSAreaSearch_spammy") << "Doing a FSAreaSearch::findObjects" << LL_ENDL;

    mLastUpdateTimer.stop(); // stop sets getElapsedTimeF32() time to zero.
    // Pause processing of requestqueue until done adding new requests.
    mRequestQueuePause = true;
    checkRegion();
    mRefresh = false;
    mSearchableObjects = 0;
    S32 object_count = gObjectList.getNumObjects();

    for (S32 i = 0; i < object_count; i++)
    {
        LLViewerObject* objectp = gObjectList.getObject(i);
        if (!objectp || !isSearchableObject(objectp, our_region))
        {
            continue;
        }

        const LLUUID& object_id = objectp->getID();

        if (object_id.isNull())
        {
            LL_WARNS("FSAreaSearch") << "WTF?! Selectable object with id of NULL!!" << LL_ENDL;
            continue;
        }

        mSearchableObjects++;

        if (mObjectDetails.count(object_id) == 0)
        {
            FSObjectProperties& details = mObjectDetails[object_id];
            details.id = object_id;
            details.local_id = objectp->getLocalID();
            details.region_handle = objectp->getRegion()->getHandle();
            mRequestNeedsSent = true;
            mRequested++;
        }
        else
        {
            FSObjectProperties& details = mObjectDetails[object_id];
            if (details.request == FSObjectProperties::FINISHED)
            {
                matchObject(details, objectp);
            }

            if (details.request == FSObjectProperties::FAILED)
            {
                // object came back into view
                details.request = FSObjectProperties::NEED;
                details.local_id = objectp->getLocalID();
                details.region_handle = objectp->getRegion()->getHandle();
                mRequestNeedsSent = true;
                mRequested++;
            }
        }
    }

    mPanelList->updateScrollList();

    S32 request_count = 0;
    // requests for non-existent objects will never arrive, check and update the queue.
    for (auto& object_it : mObjectDetails)
    {
        if (object_it.second.request == FSObjectProperties::NEED || object_it.second.request == FSObjectProperties::SENT)
        {
            const LLUUID& id = object_it.second.id;
            if (LLViewerObject* objectp = gObjectList.findObject(id); !objectp)
            {
                object_it.second.request = FSObjectProperties::FAILED;
                mRequested--;
            }
            else
            {
                request_count++;
            }
        }
    }

    if (mRequested != request_count)
    {
        LL_DEBUGS("FSAreaSearch") << "Requested mismatch: " << request_count << " actual vs. " << mRequested << LL_ENDL;
        mRequested = request_count;
    }

    updateCounterText();
    mLastUpdateTimer.start(); // start also reset elapsed time to zero
    mRequestQueuePause = false;
}

bool FSAreaSearch::isSearchableObject(LLViewerObject* objectp, LLViewerRegion* our_region)
{
    // need to be connected to region object is in.
    if (!objectp->getRegion())
    {
        return false;
    }

    // Land doesn't have object properties
    if (objectp->getPCode() == LLViewerObject::LL_VO_SURFACE_PATCH)
    {
        return false;
    }

    // Object needs to be selectable to get properties from the server
    if (!objectp->mbCanSelect)
    {
        return false;
    }

    // Avatars are spechiel objects that don't have normal properties
    if (objectp->isAvatar())
    {
        return false;
    }

    //-----------------------------------------------------------------------
    // Excludes
    //-----------------------------------------------------------------------

    if (mExcludeChildPrims && !(objectp->isRoot() || (objectp->isAttachment() && objectp->isRootEdit())))
    {
        return false;
    }

    if (mExcludeNeighborRegions && !(objectp->getRegion() == our_region))
    {
        return false;
    }

    if (mExcludeAttachment && objectp->isAttachment())
    {
        return false;
    }

    if (mExcludePhysics && objectp->flagUsePhysics())
    {
        return false;
    }

    if (mExcludeTemporary && objectp->flagTemporaryOnRez())
    {
        return false;
    }

    if (mExcludeReflectionProbe && objectp->mReflectionProbe.notNull())
    {
        return false;
    }

    return true;
}

void FSAreaSearch::processRequestQueue()
{
    if (!mActive || mRequestQueuePause)
    {
        return;
    }

    if (mLastPropertiesReceivedTimer.getElapsedTimeF32() > REQUEST_TIMEOUT)
    {
        LL_DEBUGS("FSAreaSearch") << "Timeout reached, resending requests."<< LL_ENDL;
        S32 request_count = 0;
        S32 failed_count = 0;
        for (auto& object_it : mObjectDetails)
        {
            if (object_it.second.request == FSObjectProperties::SENT)
            {
                object_it.second.request = FSObjectProperties::NEED;
                mRequestNeedsSent = true;
                request_count++;
            }

            if (object_it.second.request == FSObjectProperties::FAILED)
            {
                failed_count++;
            }
        }

        mRegionRequests.clear();
        mLastPropertiesReceivedTimer.start();

        if (!mRequestNeedsSent)
        {
            LL_DEBUGS("FSAreaSearch") << "No pending requests found."<< LL_ENDL;
        }
        else
        {
            LL_DEBUGS("FSAreaSearch") << request_count << " pending requests found."<< LL_ENDL;
        }

        LL_DEBUGS("FSAreaSearch") << failed_count << " failed requests found."<< LL_ENDL;
    }

    if (!mRequestNeedsSent)
    {
        return;
    }
    mRequestNeedsSent = false;

    for (const auto regionp : LLWorld::getInstance()->getRegionList())
    {
        U64 region_handle = regionp->getHandle();
        if (mRegionRequests[region_handle] > (MAX_OBJECTS_PER_PACKET + 128))
        {
            mRequestNeedsSent = true;
            return;
        }

        std::vector<U32> request_list;
        bool need_continue = false;

        for (auto& object_it : mObjectDetails)
        {
            if (object_it.second.request == FSObjectProperties::NEED && object_it.second.region_handle == region_handle)
            {
                request_list.push_back(object_it.second.local_id);
                object_it.second.request = FSObjectProperties::SENT;
                mRegionRequests[region_handle]++;
                if (mRegionRequests[region_handle] >= ((MAX_OBJECTS_PER_PACKET * 3) - 3))
                {
                    requestObjectProperties(request_list, true, regionp);
                    requestObjectProperties(request_list, false, regionp);
                    mRequestNeedsSent = true;
                    need_continue = true;
                    break;
                }
            }
        }

        if (need_continue)
        {
            continue;
        }

        if (!request_list.empty())
        {
            requestObjectProperties(request_list, true, regionp);
            requestObjectProperties(request_list, false, regionp);
        }
    }
}

void FSAreaSearch::requestObjectProperties(const std::vector<U32>& request_list, bool select, LLViewerRegion* regionp)
{
    bool start_new_message = true;
    S32 select_count = 0;
    LLMessageSystem* msg = gMessageSystem;

    for (const auto& id : request_list)
    {
        if (start_new_message)
        {
            if (select)
            {
                msg->newMessageFast(_PREHASH_ObjectSelect);
            }
            else
            {
                msg->newMessageFast(_PREHASH_ObjectDeselect);
            }
            msg->nextBlockFast(_PREHASH_AgentData);
            msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
            msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
            select_count++;
            start_new_message = false;
        }

        msg->nextBlockFast(_PREHASH_ObjectData);
        msg->addU32Fast(_PREHASH_ObjectLocalID, id);
        select_count++;

        if (msg->isSendFull(NULL) || select_count >= MAX_OBJECTS_PER_PACKET)
        {
            LL_DEBUGS("FSAreaSearch") << "Sent one full " << (select ? "ObjectSelect" : "ObjectDeselect") << " message with " << select_count << " object data blocks." << LL_ENDL;
            msg->sendReliable(regionp->getHost());
            select_count = 0;
            start_new_message = true;
        }
    }

    if (!start_new_message)
    {
        LL_DEBUGS("FSAreaSearch") << "Sent one partcial " << (select ? "ObjectSelect" : "ObjectDeselect") << " message with " << select_count << " object data blocks." << LL_ENDL;
        msg->sendReliable(regionp->getHost());
    }
}

void FSAreaSearch::processObjectProperties(LLMessageSystem* msg)
{
    if (!mActive)
    {
        return;
    }

    LLViewerRegion* our_region = gAgent.getRegion();
    bool counter_text_update = false;

    S32 count = msg->getNumberOfBlocksFast(_PREHASH_ObjectData);
    LL_DEBUGS("FSAreaSearch")  << "Got processObjectProperties message with " << count << " object(s)" << LL_ENDL;
    for (S32 i = 0; i < count; i++)
    {
        LLUUID object_id;
        msg->getUUIDFast(_PREHASH_ObjectData, _PREHASH_ObjectID, object_id, i);
        if (object_id.isNull())
        {
            LL_WARNS("FSAreaSearch") << "Got Object Properties with NULL id" << LL_ENDL;
            continue;
        }

        LLViewerObject* objectp = gObjectList.findObject(object_id);
        if (!objectp)
        {
            continue;
        }

        FSObjectProperties& details = mObjectDetails[object_id];
        if (details.request != FSObjectProperties::FINISHED)
        {
            // We cache un-requested objects (to avoid having to request them later)
            // and requested objects.

            details.request = FSObjectProperties::FINISHED;
            mLastPropertiesReceivedTimer.start();

            if (details.id.isNull())
            {
                // Recieved object properties without requesting it.
                details.id = object_id;
            }
            else
            {
                if (mRequested > 0)
                {
                    mRequested--;
                }
                mRegionRequests[details.region_handle]--;
                counter_text_update = true;
            }

            msg->getUUIDFast(_PREHASH_ObjectData, _PREHASH_CreatorID, details.creator_id, i);
            msg->getUUIDFast(_PREHASH_ObjectData, _PREHASH_OwnerID, details.owner_id, i);
            msg->getUUIDFast(_PREHASH_ObjectData, _PREHASH_GroupID, details.group_id, i);
            msg->getU64Fast(_PREHASH_ObjectData, _PREHASH_CreationDate, details.creation_date, i);
            msg->getU32Fast(_PREHASH_ObjectData, _PREHASH_BaseMask, details.base_mask, i);
            msg->getU32Fast(_PREHASH_ObjectData, _PREHASH_OwnerMask, details.owner_mask, i);
            msg->getU32Fast(_PREHASH_ObjectData,_PREHASH_GroupMask, details.group_mask, i);
            msg->getU32Fast(_PREHASH_ObjectData, _PREHASH_EveryoneMask, details.everyone_mask, i);
            msg->getU32Fast(_PREHASH_ObjectData, _PREHASH_NextOwnerMask, details.next_owner_mask, i);
            details.sale_info.unpackMultiMessage(msg, _PREHASH_ObjectData, i);
            details.ag_perms.unpackMessage(msg, _PREHASH_ObjectData, _PREHASH_AggregatePerms, i);
            details.ag_texture_perms.unpackMessage(msg, _PREHASH_ObjectData, _PREHASH_AggregatePermTextures, i);
            details.ag_texture_perms_owner.unpackMessage(msg, _PREHASH_ObjectData, _PREHASH_AggregatePermTexturesOwner, i);
            details.category.unpackMultiMessage(msg, _PREHASH_ObjectData, i);
            msg->getUUIDFast(_PREHASH_ObjectData, _PREHASH_LastOwnerID, details.last_owner_id, i);
            msg->getStringFast(_PREHASH_ObjectData, _PREHASH_Name, details.name, i);
            msg->getStringFast(_PREHASH_ObjectData, _PREHASH_Description, details.description, i);
            msg->getStringFast(_PREHASH_ObjectData, _PREHASH_TouchName, details.touch_name, i);
            msg->getStringFast(_PREHASH_ObjectData, _PREHASH_SitName, details.sit_name, i);

            S32 size = msg->getSizeFast(_PREHASH_ObjectData, i, _PREHASH_TextureID);
            if (size > 0)
            {
                S8 packed_buffer[SELECT_MAX_TES * UUID_BYTES];
                msg->getBinaryDataFast(_PREHASH_ObjectData, _PREHASH_TextureID, packed_buffer, 0, i, SELECT_MAX_TES * UUID_BYTES);

                for (S32 buf_offset = 0; buf_offset < size; buf_offset += UUID_BYTES)
                {
                    LLUUID tid;
                    memcpy(tid.mData, packed_buffer + buf_offset, UUID_BYTES);      /* Flawfinder: ignore */
                    details.texture_ids.push_back(tid);
                }
            }

            details.permissions.init(details.creator_id, details.owner_id, details.last_owner_id, details.group_id);
            details.permissions.initMasks(details.base_mask, details.owner_mask, details.everyone_mask, details.group_mask, details.next_owner_mask);

            // Sets the group owned bool and real owner id, group or owner depending if object is group owned.
            details.permissions.getOwnership(details.ownership_id, details.group_owned);

            LL_DEBUGS("FSAreaSearch_spammy") << "Got properties for object: " << object_id << LL_ENDL;

            if (isSearchableObject(objectp, our_region))
            {
                matchObject(details, objectp);
            }
        }
    }

    if (counter_text_update)
    {
        updateCounterText();
    }
}

void FSAreaSearch::matchObject(FSObjectProperties& details, LLViewerObject* objectp)
{
    if (details.listed)
    {
        // object allready listed on the scroll list.
        return;
    }

    //-----------------------------------------------------------------------
    // Filters
    //-----------------------------------------------------------------------

    if (mFilterForSale && !(details.sale_info.isForSale() && (details.sale_info.getSalePrice() >= mFilterForSaleMin && details.sale_info.getSalePrice() <= mFilterForSaleMax)))
    {
        return;
    }

    if (mFilterDistance)
    {
        S32 distance = (S32)calculateObjectDistance(mPanelList->getAgentLastPosition(), objectp);// used mAgentLastPosition instead of gAgent->getPositionGlobal for performace
        if (distance < mFilterDistanceMin || distance > mFilterDistanceMax)
        {
            return;
        }
    }

    if (mFilterClickAction)
    {
        switch(mFilterClickActionType)
        {
        case 0: // "(blank)", should not end up here, but just in case
            break;
        case 1: // "any" mouse click action
            if (!(objectp->flagHandleTouch() || objectp->getClickAction() != 0))
            {
                return;
            }
            break;
        case 2: // "touch" is a seperate mouse click action flag
            if (!objectp->flagHandleTouch())
            {
                return;
            }
            break;
        default: // all other mouse click action types
            if ((mFilterClickActionType - 2) != objectp->getClickAction())
            {
                return;
            }
            break;
        }
    }

    if (mFilterPhysical && !objectp->flagUsePhysics())
    {
        return;
    }

    if (mFilterTemporary && !objectp->flagTemporaryOnRez())
    {
        return;
    }

    if (mFilterLocked && (details.owner_mask & PERM_MOVE))
    {
        return;
    }

    if (mFilterPhantom && !objectp->flagPhantom())
    {
        return;
    }

    if (mFilterAttachment && !objectp->isAttachment())
    {
        return;
    }

    if (mFilterMoaP)
    {
        bool moap = false;
        U8 texture_count = objectp->getNumTEs();
        for (U8 i = 0; i < texture_count; i++)
        {
            if (objectp->getTE(i)->hasMedia())
            {
                moap = true;
                break;
            }
        }

        if (!moap)
        {
            return;
        }
    }

    if (mFilterAgentParcelOnly && !LLViewerParcelMgr::instance().inAgentParcel(objectp->getPositionGlobal()))
    {
        return;
    }

    if (mFilterPermCopy && !(details.owner_mask & PERM_COPY))
    {
        return;
    }

    if (mFilterPermModify && !(details.owner_mask & PERM_MODIFY))
    {
        return;
    }

    if (mFilterPermTransfer && !(details.owner_mask & PERM_TRANSFER))
    {
        return;
    }

    if (mFilterReflectionProbe && !objectp->mReflectionProbe.notNull())
    {
        return;
    }

    //-----------------------------------------------------------------------
    // Find text
    //-----------------------------------------------------------------------

    LLUUID object_id = details.id;
    std::string creator_name;
    std::string owner_name;
    std::string last_owner_name;
    std::string group_name;
    std::string object_name = details.name;
    std::string object_description = details.description;

    details.name_requested = false;
    getNameFromUUID(details.ownership_id, owner_name, details.group_owned, details.name_requested);
    getNameFromUUID(details.creator_id, creator_name, false, details.name_requested);
    getNameFromUUID(details.last_owner_id, last_owner_name, false, details.name_requested);
    getNameFromUUID(details.group_id, group_name, true, details.name_requested);

    owner_name = RLVa_hideNameIfRestricted(owner_name);
    last_owner_name = RLVa_hideNameIfRestricted(last_owner_name);

    if (mRegexSearch)
    {
        try
        {
            if (!mSearchName.empty() && !boost::regex_match(object_name, mRegexSearchName))
            {
                return;
            }
            if (!mSearchDescription.empty() && !boost::regex_match(object_description, mRegexSearchDescription))
            {
                return;
            }
            if (!mSearchOwner.empty() && !boost::regex_match(owner_name, mRegexSearchOwner))
            {
                return;
            }
            if (!mSearchGroup.empty() && !boost::regex_match(group_name, mRegexSearchGroup))
            {
                return;
            }
            if (!mSearchCreator.empty() && !boost::regex_match(creator_name, mRegexSearchCreator))
            {
                return;
            }
            if (!mSearchLastOwner.empty() && !boost::regex_match(last_owner_name, mRegexSearchLastOwner))
            {
                return;
            }
        }

        // Should not end up here due to error checking in Find class. However, some complex regexes may
        // cause excessive resources and boost will throw an execption.
        // Due to the possiablitey of hitting this block a 1000 times per second, only logonce it.
        catch(boost::regex_error& e)
        {
            LL_WARNS_ONCE("FSAreaSearch") << "boost::regex_error error in regex: "<< e.what() << LL_ENDL;
        }
        catch(const std::exception& e)
        {
            LL_WARNS_ONCE("FSAreaSearch") << "std::exception error in regex: "<< e.what() << LL_ENDL;
        }
        catch (...)
        {
            LL_WARNS_ONCE("FSAreaSearch") << "Unknown error in regex" << LL_ENDL;
        }
    }
    else
    {
        if (!mSearchName.empty() && boost::ifind_first(object_name, mSearchName).empty())
        {
            return;
        }
        if (!mSearchDescription.empty() && boost::ifind_first(object_description, mSearchDescription).empty())
        {
            return;
        }
        if (!mSearchOwner.empty() && boost::ifind_first(owner_name, mSearchOwner).empty())
        {
            return;
        }
        if (!mSearchGroup.empty() && boost::ifind_first(group_name, mSearchGroup).empty())
        {
            return;
        }
        if (!mSearchCreator.empty() && boost::ifind_first(creator_name, mSearchCreator).empty())
        {
            return;
        }
        if (!mSearchLastOwner.empty() && boost::ifind_first(last_owner_name, mSearchLastOwner).empty())
        {
            return;
        }
    }

    //-----------------------------------------------------------------------
    // Object passed all above tests, add it to the List tab.
    //-----------------------------------------------------------------------

    details.listed = true;

    LLScrollListCell::Params cell_params;
    cell_params.font = LLFontGL::getFontSansSerif();

    LLScrollListItem::Params row_params;
    row_params.value = object_id.asString();

    cell_params.column = "distance";
    cell_params.value = llformat("%1.0f m", calculateObjectDistance(mPanelList->getAgentLastPosition(), objectp)); // used mAgentLastPosition instead of gAgent->getPositionGlobal for performace
    row_params.columns.add(cell_params);

    cell_params.column = "name";
    cell_params.value = details.name;
    row_params.columns.add(cell_params);

    cell_params.column = "description";
    cell_params.value = details.description;
    row_params.columns.add(cell_params);

    cell_params.column = "price";
    if (details.sale_info.isForSale())
    {
        LLStringUtil::format_map_t args;
        args["COST"] = llformat("%d", details.sale_info.getSalePrice());
        std::string cost_label = LLTrans::getString("FSAreaSearch_Cost_Label", args);
        cell_params.value = cost_label;
    }
    else
    {
        cell_params.value = " ";
    }
    row_params.columns.add(cell_params);

    cell_params.column = "land_impact";
    if (F32 cost = objectp->getLinksetCost(); cost > F_ALMOST_ZERO)
    {
        cell_params.value = cost;
    }
    else
    {
        cell_params.value = "...";
    }
    row_params.columns.add(cell_params);

    cell_params.column = "prim_count";
    cell_params.value = objectp->numChildren() + 1;
    row_params.columns.add(cell_params);

    cell_params.column = "owner";
    cell_params.value = owner_name;
    row_params.columns.add(cell_params);

    cell_params.column = "group";
    cell_params.value = group_name;
    row_params.columns.add(cell_params);

    cell_params.column = "creator";
    cell_params.value = creator_name;
    row_params.columns.add(cell_params);

    cell_params.column = "last_owner";
    cell_params.value = last_owner_name;
    row_params.columns.add(cell_params);

    LLScrollListItem* list_row = mPanelList->getResultList()->addRow(row_params);

    if (objectp->flagTemporaryOnRez() || objectp->flagUsePhysics())
    {
        U8 font_style = LLFontGL::NORMAL;
        if (objectp->flagTemporaryOnRez())
        {
            font_style |= LLFontGL::ITALIC;
        }
        if (objectp->flagUsePhysics())
        {
            font_style |= LLFontGL::BOLD;
        }

        S32 num_colums = list_row->getNumColumns();
        for (S32 i = 0; i < num_colums; i++)
        {
            LLScrollListText* list_cell = (LLScrollListText*)list_row->getColumn(i);
            list_cell->setFontStyle(font_style);
        }
    }

    mPanelList->getResultList()->refreshLineHeight();
}

void FSAreaSearch::updateObjectCosts(const LLUUID& object_id, F32 object_cost, F32 link_cost, F32 physics_cost, F32 link_physics_cost)
{
    if (!mActive)
    {
        return;
    }

    if (FSScrollListCtrl* result_list = mPanelList->getResultList(); result_list)
    {
        if (LLScrollListItem* list_row = result_list->getItem(LLSD(object_id)); list_row)
        {
            if (LLScrollListColumn* list_column = result_list->getColumn("land_impact"); list_column)
            {
                LLScrollListCell* linkset_cost_cell = list_row->getColumn(list_column->mIndex);
                linkset_cost_cell->setValue(LLSD(link_cost));
                result_list->setNeedsSort(); // re-sort if needed.
            }
        }
    }
}

void FSAreaSearch::getNameFromUUID(const LLUUID& id, std::string& name, bool group, bool& name_requested)
{
    static const std::string unknown_name = LLTrans::getString("AvatarNameWaiting");

    if (group)
    {
        bool is_group;
        if (!gCacheName->getIfThere(id, name, is_group))
        {
            name = unknown_name;
            if (std::find(mNamesRequested.begin(), mNamesRequested.end(), id) == mNamesRequested.end())
            {
                mNamesRequested.push_back(id);
                boost::signals2::connection cb_connection = gCacheName->get(id, group, boost::bind(&FSAreaSearch::callbackLoadFullName, this, _1, _2));
                mNameCacheConnections.insert(std::make_pair(id, cb_connection)); // mNamesRequested will do the dupe check
            }
            name_requested = true;
        }
    }
    else
    {
        LLAvatarName av_name;
        if (!LLAvatarNameCache::get(id, &av_name))
        {
            name = unknown_name;
            if (std::find(mNamesRequested.begin(), mNamesRequested.end(), id) == mNamesRequested.end())
            {
                mNamesRequested.push_back(id);
                boost::signals2::connection cb_connection = LLAvatarNameCache::get(id, boost::bind(&FSAreaSearch::avatarNameCacheCallback, this, _1, _2));
                mNameCacheConnections.insert(std::make_pair(id, cb_connection)); // mNamesRequested will do the dupe check
            }
            name_requested = true;
        }
        else
            name = av_name.getCompleteName();
    }
}

void FSAreaSearch::avatarNameCacheCallback(const LLUUID& id, const LLAvatarName& av_name)
{
    callbackLoadFullName(id, av_name.getCompleteName());
}

void FSAreaSearch::callbackLoadFullName(const LLUUID& id, const std::string& full_name )
{
    if (auto iter = mNameCacheConnections.find(id); iter != mNameCacheConnections.end())
    {
        if (iter->second.connected())
        {
            iter->second.disconnect();
        }
        mNameCacheConnections.erase(iter);
    }

    LLViewerRegion* our_region = gAgent.getRegion();

    for (auto& entry : mObjectDetails)
    {
        if (entry.second.name_requested && !entry.second.listed)
        {
            const LLUUID& object_id = entry.second.id;
            LLViewerObject* objectp = gObjectList.findObject(object_id);
            if (objectp && isSearchableObject(objectp, our_region))
            {
                matchObject(entry.second, objectp);
            }
        }
    }

    mPanelList->updateName(id, full_name);
}

void FSAreaSearch::updateCounterText()
{
    LLStringUtil::format_map_t args;
    args["[LISTED]"] = llformat("%d", mPanelList->getResultList()->getItemCount());
    args["[PENDING]"] = llformat("%d", mRequested);
    args["[TOTAL]"] = llformat("%d", mSearchableObjects);
    mPanelList->setCounterText(args);
}

void FSAreaSearch::onCommitLine()
{
    mSearchName = mPanelFind->mNameLineEditor->getText();
    mSearchDescription = mPanelFind->mDescriptionLineEditor->getText();
    mSearchOwner = mPanelFind->mOwnerLineEditor->getText();
    mSearchGroup = mPanelFind->mGroupLineEditor->getText();
    mSearchCreator = mPanelFind->mCreatorLineEditor->getText();
    mSearchLastOwner = mPanelFind->mLastOwnerLineEditor->getText();

    if (mRegexSearch)
    {
        if (!mSearchName.empty())
        {
            if (regexTest(mSearchName))
            {
                mRegexSearchName = mSearchName.c_str();
            }
            else
            {
                // empty the search text to prevent error in matchObject
                mSearchName.erase();
            }
        }

        if (!mSearchDescription.empty())
        {
            if (regexTest(mSearchDescription))
            {
                mRegexSearchDescription = mSearchDescription.c_str();
            }
            else
            {
                mSearchDescription.erase();
            }
        }

        if (!mSearchOwner.empty())
        {
            if (regexTest(mSearchOwner))
            {
                mRegexSearchOwner = mSearchOwner.c_str();
            }
            else
            {
                mSearchOwner.erase();
            }
        }

        if (!mSearchGroup.empty())
        {
            if (regexTest(mSearchGroup))
            {
                mRegexSearchGroup = mSearchGroup.c_str();
            }
            else
            {
                mSearchGroup.erase();
            }
        }

        if (!mSearchCreator.empty())
        {
            if (regexTest(mSearchCreator))
            {
                mRegexSearchCreator = mSearchCreator.c_str();
            }
            else
            {
                mSearchCreator.erase();
            }
        }

        if (!mSearchLastOwner.empty())
        {
            if (regexTest(mSearchLastOwner))
            {
                mRegexSearchLastOwner = mSearchLastOwner.c_str();
            }
            else
            {
                mSearchLastOwner.erase();
            }
        }
    }
}

bool FSAreaSearch::regexTest(std::string_view text)
{
    // couple regex patters one can use for testing. The regex will match a UUID.
    // boost::regex pattern("[\\w]{8}-[\\w]{4}-[\\w]{4}-[\\w]{4}-[\\w]{12}");
    // [\p{XDigit}]{8}(-[\p{XDigit}]{4}){3}-[\p{XDigit}]{12}
    // to find all objects that don't belong to a group, use (?!^Name of the group$).* in the group field.

    try
    {
        static const std::string test_text = "asdfghjklqwerty1234567890";
        boost::regex pattern(text.data());
        boost::regex_match(test_text, pattern);
    }
    catch(boost::regex_error& e)
    {
        LLSD args;
        args["EWHAT"] = e.what();
        LLNotificationsUtil::add("RegExFail", args);
        LL_DEBUGS("FSAreaSearch") << "boost::regex_error error in regex: "<< e.what() << LL_ENDL;
        return false;
    }
    catch(const std::exception& e)
    {
        LLSD args;
        args["EWHAT"] = e.what();
        LLNotificationsUtil::add("RegExFail", args);
        LL_DEBUGS("FSAreaSearch") << "std::exception error in regex: "<< e.what() << LL_ENDL;
        return false;
    }
    catch (...)
    {
        LLSD args;
        args["EWHAT"] = "Unknown Error.";
        LLNotificationsUtil::add("RegExFail", args);
        LL_DEBUGS("FSAreaSearch") << "Unknown error in regex" << LL_ENDL;
        return false;
    }

    return true;
}

void FSAreaSearch::clearSearchText()
{
    mSearchName.erase();
    mSearchDescription.erase();
    mSearchOwner.erase();
    mSearchGroup.erase();
    mSearchCreator.erase();
    mSearchLastOwner.erase();
}

void FSAreaSearch::onButtonClickedSearch()
{
    // if the user blanks out a line, onCommitLine is not fired/called.
    // calling this will make sure to update any blanked out lines.
    onCommitLine();

    mTab->selectFirstTab();
    refreshList(false);
}

void FSAreaSearch::onCommitCheckboxRegex()
{
    mRegexSearch = mPanelFind->mCheckboxRegex->get();

    if (mRegexSearch)
    {
        onCommitLine();
    }
}

void FSAreaSearch::setFindOwnerText(std::string value)
{
    mPanelFind->mOwnerLineEditor->setText(value);
}


//---------------------------------------------------------------------------
// List panel
//---------------------------------------------------------------------------

FSPanelAreaSearchList::FSPanelAreaSearchList(FSAreaSearch* pointer)
:   LLPanel(),
    mCounterText(0),
    mResultList(0),
    mFSAreaSearch(pointer),
    mFSAreaSearchColumnConfigConnection()
{
    mColumnBits["distance"] = 1;
    mColumnBits["name"] = 2;
    mColumnBits["description"] = 4;
    mColumnBits["price"] = 8;
    mColumnBits["land_impact"] = 16;
    mColumnBits["prim_count"] = 32;
    mColumnBits["owner"] = 64;
    mColumnBits["group"] = 128;
    mColumnBits["creator"] = 256;
    mColumnBits["last_owner"] = 512;
}

bool FSPanelAreaSearchList::postBuild()
{
    mResultList = getChild<FSScrollListCtrl>("result_list");
    mResultList->setDoubleClickCallback(boost::bind(&FSPanelAreaSearchList::onDoubleClick, this));
    mResultList->sortByColumn("name", true);
    mResultList->setContextMenu(&gFSAreaSearchMenu);

    mCounterText = getChild<LLTextBox>("counter");

    mRefreshButton = getChild<LLButton>("Refresh");
    mRefreshButton->setClickedCallback(boost::bind(&FSPanelAreaSearchList::onClickRefresh, this));

    mCheckboxBeacons = getChild<LLCheckBoxCtrl>("beacons");
    mCheckboxBeacons->setCommitCallback(boost::bind(&FSPanelAreaSearchList::onCommitCheckboxBeacons, this));

    mAgentLastPosition = gAgent.getPositionGlobal();

    updateResultListColumns();
    mFSAreaSearchColumnConfigConnection = gSavedSettings.getControl("FSAreaSearchColumnConfig")->getSignal()->connect(boost::bind(&FSPanelAreaSearchList::updateResultListColumns, this));

    return LLPanel::postBuild();
}

// virtual
FSPanelAreaSearchList::~FSPanelAreaSearchList()
{
    if (mFSAreaSearchColumnConfigConnection.connected())
    {
        mFSAreaSearchColumnConfigConnection.disconnect();
    }
}

void FSPanelAreaSearchList::onClickRefresh()
{
    mFSAreaSearch->refreshList(true);
}

void FSPanelAreaSearchList::onCommitCheckboxBeacons()
{
    mFSAreaSearch->setBeacons(mCheckboxBeacons->get());
}

void FSPanelAreaSearchList::setCounterText()
{
    mCounterText->setText(getString("ListedPendingTotalBlank"));
}

void FSPanelAreaSearchList::setCounterText(LLStringUtil::format_map_t args)
{
    mCounterText->setText(getString("ListedPendingTotalFilled", args));
}

void FSPanelAreaSearchList::onDoubleClick()
{
    LLScrollListItem* item = mResultList->getFirstSelected();
    if (!item)
    {
        return;
    }

    const LLUUID& object_id = item->getUUID();
    if (LLViewerObject* objectp = gObjectList.findObject(object_id); objectp)
    {
        FSObjectProperties& details = mFSAreaSearch->mObjectDetails[object_id];
        LLTracker::trackLocation(objectp->getPositionGlobal(), details.name, "", LLTracker::LOCATION_ITEM);

        if (mFSAreaSearch->getPanelAdvanced()->mCheckboxClickBuy->get())
        {
            buyObject(details, objectp);
        }

        if (mFSAreaSearch->getPanelAdvanced()->mCheckboxClickTouch->get())
        {
            touchObject(objectp);
        }

        if (mFSAreaSearch->getPanelAdvanced()->mCheckboxClickSit->get())
        {
            sitOnObject(details, objectp);
        }
    }
}

void FSPanelAreaSearchList::updateScrollList()
{
    bool agent_moved = false;
    const LLVector3d current_agent_position = gAgent.getPositionGlobal();

    if (dist_vec(mAgentLastPosition, current_agent_position) > MIN_DISTANCE_MOVED)
    {
        agent_moved = true;
        mAgentLastPosition = current_agent_position;
    }

    bool deleted = false;
    LLScrollListColumn* distance_column = mResultList->getColumn("distance");
    LLViewerRegion* our_region = gAgent.getRegion();

    // Iterate over the rows in the list, deleting ones whose object has gone away.
    std::vector<LLScrollListItem*> items = mResultList->getAllData();
    for (const auto item : items)
    {
        const LLUUID& row_id = item->getUUID();
        LLViewerObject* objectp = gObjectList.findObject(row_id);

        if (!objectp || !mFSAreaSearch->isSearchableObject(objectp, our_region))
        {
            // This item's object has been deleted -- remove the row.
            // Removing the row won't throw off our iteration, since we have a local copy of the array.
            // We just need to make sure we don't access this item after the delete.
            mResultList->deleteSingleItem(mResultList->getItemIndex(row_id));

            mFSAreaSearch->mObjectDetails[row_id].listed = false ;
            deleted = true;
        }
        else
        {
            if (agent_moved && distance_column)
            {
                item->getColumn(distance_column->mIndex)->setValue(LLSD(llformat("%1.0f m", calculateObjectDistance(current_agent_position, objectp))));
            }
        }
    }

    if (deleted || agent_moved)
    {
        mResultList->updateLayout();
    }
}

void FSPanelAreaSearchList::updateResultListColumns()
{
    U32 column_config = gSavedSettings.getU32("FSAreaSearchColumnConfig");
    std::vector<LLScrollListColumn::Params> column_params = mResultList->getColumnInitParams();
    std::string current_sort_col = mResultList->getSortColumnName();
    bool current_sort_asc = mResultList->getSortAscending();

    mResultList->clearColumns();
    mResultList->updateLayout();

    for (const auto& p : column_params)
    {
        LLScrollListColumn::Params params;
        params.header = p.header;
        params.name = p.name;
        params.halign = p.halign;
        params.sort_direction = p.sort_direction;
        params.sort_column = p.sort_column;
        params.tool_tip = p.tool_tip;

        if (column_config & mColumnBits[p.name.getValue()])
        {
            params.width = p.width;
        }
        else
        {
            params.width.pixel_width.set(-1, true);
        }

        mResultList->addColumn(params);
    }

    mResultList->sortByColumn(current_sort_col, current_sort_asc);
    mResultList->dirtyColumns();
    mResultList->updateColumns(true);
}

void FSPanelAreaSearchList::onColumnVisibilityChecked(const LLSD& userdata)
{
    const std::string& column = userdata.asStringRef();
    U32 column_config = gSavedSettings.getU32("FSAreaSearchColumnConfig");

    U32 new_value;
    U32 enabled = (mColumnBits[column] & column_config);
    if (enabled)
    {
        new_value = (column_config & ~mColumnBits[column]);
    }
    else
    {
        new_value = (column_config | mColumnBits[column]);
    }

    gSavedSettings.setU32("FSAreaSearchColumnConfig", new_value);

    updateResultListColumns();
}

bool FSPanelAreaSearchList::onEnableColumnVisibilityChecked(const LLSD& userdata)
{
    const std::string& column = userdata.asStringRef();
    U32 column_config = gSavedSettings.getU32("FSAreaSearchColumnConfig");

    return (mColumnBits[column] & column_config);
}

void FSPanelAreaSearchList::updateName(const LLUUID& id, const std::string& name)
{
    LLScrollListColumn* creator_column = mResultList->getColumn("creator");
    LLScrollListColumn* owner_column = mResultList->getColumn("owner");
    LLScrollListColumn* group_column = mResultList->getColumn("group");
    LLScrollListColumn* last_owner_column = mResultList->getColumn("last_owner");

    // Iterate over the rows in the list, updating the ones with matching id.
    std::vector<LLScrollListItem*> items = mResultList->getAllData();

    for (const auto item : items)
    {
        const LLUUID& row_id = item->getUUID();
        FSObjectProperties& details = mFSAreaSearch->mObjectDetails[row_id];

        if (creator_column && (id == details.creator_id))
        {
            LLScrollListText* creator_text = (LLScrollListText*)item->getColumn(creator_column->mIndex);
            creator_text->setText(name);
            mResultList->setNeedsSort();
        }

        if (owner_column && (id == details.owner_id))
        {
            LLScrollListText* owner_text = (LLScrollListText*)item->getColumn(owner_column->mIndex);
            owner_text->setText(RLVa_hideNameIfRestricted(name));
            mResultList->setNeedsSort();
        }

        if (group_column && (id == details.group_id))
        {
            LLScrollListText* group_text = (LLScrollListText*)item->getColumn(group_column->mIndex);
            group_text->setText(name);
            mResultList->setNeedsSort();
        }

        if (last_owner_column && (id == details.last_owner_id))
        {
            LLScrollListText* last_owner_text = (LLScrollListText*)item->getColumn(last_owner_column->mIndex);
            last_owner_text->setText(RLVa_hideNameIfRestricted(name));
            mResultList->setNeedsSort();
        }
    }
}

bool FSPanelAreaSearchList::onContextMenuItemEnable(const LLSD& userdata)
{
    const std::string& parameter = userdata.asStringRef();
    if (parameter == "one")
    {
        // return true if just one item is selected.
        return (mResultList->getNumSelected() == 1);
    }
    else if (parameter == "in_dd")
    {
        // return true if the object is within the draw distance.
        if (mResultList->getNumSelected() == 1)
        {
            const LLUUID& object_id = mResultList->getFirstSelected()->getUUID();
            LLViewerObject* objectp = gObjectList.findObject(object_id);
            return (objectp && calculateObjectDistance(gAgent.getPositionGlobal(), objectp) < gAgentCamera.mDrawDistance);
        }
        else
        {
            return false;
        }
    }
    else if (parameter == "script")
    {
        return (mResultList->getNumSelected() > 0 && enable_bridge_function());
    }
    else
    {
        // return true if more then one is selected, but not just one.
        return (mResultList->getNumSelected() > 1);
    }
}

bool FSPanelAreaSearchList::onContextMenuItemVisibleRLV(const LLSD& userdata)
{
    if (!RlvActions::isRlvEnabled())
    {
        return true;
    }

    if (RlvActions::hasBehaviour(RLV_BHVR_INTERACT))
    {
        return false;
    }

    const std::string& parameter = userdata.asStringRef();
    std::vector<std::string> behavs;
    LLStringUtil::getTokens(parameter, behavs, "|");

    if (std::find(behavs.begin(), behavs.end(), "delete") != behavs.end())
    {
        if (!rlvCanDeleteOrReturn())
        {
            return false;
        }
    }

    std::vector<LLScrollListItem*> selected = mResultList->getAllSelected();
    for (const auto item : selected)
    {
        const LLUUID& object_id = item->getUUID();
        LLViewerObject* objectp = gObjectList.findObject(object_id);
        if (!objectp)
        {
            return false;
        }

        if (
            std::find(behavs.begin(), behavs.end(), "touch") != behavs.end()
            && !RlvActions::canTouch(objectp)
        )
        {
            return false;
        }
        else if (
            std::find(behavs.begin(), behavs.end(), "edit") != behavs.end()
            && !RlvActions::canEdit(objectp)
        )
        {
            return false;
        }
        else if (
            std::find(behavs.begin(), behavs.end(), "sit") != behavs.end()
            && !RlvActions::canSit(objectp)
        )
        {
            return false;
        }
        else if (
            std::find(behavs.begin(), behavs.end(), "buy") != behavs.end()
            && !RlvActions::canBuyObject(object_id)
        )
        {
            return false;
        }
        else if (
            std::find(behavs.begin(), behavs.end(), "teleport") != behavs.end()
            && !RlvActions::canTeleportToLocal(objectp->getPositionGlobal())
        )
        {
            return false;
        }
    }

    return true;
}

bool FSPanelAreaSearchList::onContextMenuItemClick(const LLSD& userdata)
{
    const std::string& action = userdata.asStringRef();
    LL_DEBUGS("FSAreaSearch") << "Right click menu " << action << " was selected." << LL_ENDL;

    if (action == "select_all")
    {
        std::vector<LLScrollListItem*> result_items = mResultList->getAllData();
        std::for_each(result_items.begin(), result_items.end(), [](LLScrollListItem* item) { item->setSelected(true); });
        return true;
    }
    if (action == "clear_selection")
    {
        std::vector<LLScrollListItem*> selected_items = mResultList->getAllSelected();
        std::for_each(selected_items.begin(), selected_items.end(), [](LLScrollListItem* item) { item->setSelected(false); });
        return true;
    }
    if (action == "filter_my_objects")
    {
        mFSAreaSearch->setFindOwnerText(gAgentUsername);
        mFSAreaSearch->onButtonClickedSearch();
        return true;
    }

    // NOTE that each action command MUST begin with a different letter.
    char c = action.at(0);
    switch(c)
    {
    case 't': // touch
    case 's': // script
    case 'l': // blacklist
    {
        std::vector<LLScrollListItem*> selected = mResultList->getAllSelected();
        S32 cnt = 0;

        for (const auto item : selected)
        {
            switch (c)
            {
            case 't': // touch
            {
                new FSAreaSearchTouchTimer(item->getUUID(), cnt * 0.2f);
                cnt++;
            }
                break;
            case 's': // script
                FSLSLBridge::instance().viewerToLSL("getScriptInfo|" + item->getUUID().asString() + "|" + (gSavedSettings.getBOOL("FSScriptInfoExtended") ? "1" : "0"));
                break;
            case 'l': // blacklist
            {
                const LLUUID& object_id = item->getUUID();
                LLViewerObject* objectp = gObjectList.findObject(object_id);
//              if (objectp)
// [RLVa:KB] - Checked: RLVa-2.0.0 | FS-specific
                // Don't allow derendering of own attachments when RLVa is enabled
                if ( (objectp) && (gAgentID != objectp->getID()) && ((!RlvActions::isRlvEnabled()) || (!objectp->isAttachment()) || (!objectp->permYouOwner())) )
// [/RLVa:KB]
                {
                    std::string region_name;
                    LLViewerRegion* region = objectp->getRegion();
                    if (region)
                    {
                        region_name = objectp->getRegion()->getName();
                    }
                    FSAssetBlacklist::getInstance()->addNewItemToBlacklist(object_id, mFSAreaSearch->mObjectDetails[object_id].name, region_name, LLAssetType::AT_OBJECT);

                    mFSAreaSearch->mObjectDetails.erase(object_id);
                    LLSelectMgr::getInstance()->deselectObjectOnly(objectp);
                    gObjectList.addDerenderedItem(object_id, true);
                    gObjectList.killObject(objectp);
                    if (LLViewerRegion::sVOCacheCullingEnabled && region)
                    {
                        region->killCacheEntry(objectp->getLocalID());
                    }

                    LLTool* tool = LLToolMgr::getInstance()->getCurrentTool();
                    LLViewerObject* tool_editing_object = tool->getEditingObject();
                    if (tool_editing_object && tool_editing_object->mID == object_id)
                    {
                        tool->stopEditing();
                    }
                }
            }
                break;
            default:
                break;
            }
        }
    }
        break;

    case 'b': // buy
    case 'p': // p_teleport
    case 'u': // sit
    case 'q': // q_zoom
    {
        if (mResultList->getNumSelected() == 1)
        {
            const LLUUID& object_id = mResultList->getFirstSelected()->getUUID();
            LLViewerObject* objectp = gObjectList.findObject(object_id);
            if (objectp)
            {
                switch (c)
                {
                case 'b': // buy
                    buyObject(mFSAreaSearch->mObjectDetails[object_id], objectp);
                    break;
                case 'p': // p_teleport
                    gAgent.teleportViaLocation(objectp->getPositionGlobal());
                    break;
                case 'u': // sit
                    sitOnObject(mFSAreaSearch->mObjectDetails[object_id], objectp);
                    break;
                case 'q': // q_zoom
                {
                    // Disable flycam if active.  Without this, the requested look-at doesn't happen because the flycam code overrides all other camera motion.
                    bool fly_cam_status(LLViewerJoystick::getInstance()->getOverrideCamera());
                    if (fly_cam_status)
                    {
                        LLViewerJoystick::getInstance()->setOverrideCamera(false);
                        LLPanelStandStopFlying::clearStandStopFlyingMode(LLPanelStandStopFlying::SSFM_FLYCAM);
                        // *NOTE: Above may not be the proper way to disable flycam.  What I really want to do is just be able to move the camera and then leave the flycam in the the same state it was in, just moved to the new location. ~Cron
                    }

                    LLViewerJoystick::getInstance()->setCameraNeedsUpdate(true); // Fixes an edge case where if the user has JUST disabled flycam themselves, the camera gets stuck waiting for input.

                    gAgentCamera.setFocusOnAvatar(false, ANIMATE);

                    gAgentCamera.setLookAt(LOOKAT_TARGET_SELECT, objectp);

                    // Place the camera looking at the object, along the line from the camera to the object,
                    //  and sufficiently far enough away for the object to fill 3/4 of the screen,
                    //  but not so close that the bbox's nearest possible vertex goes inside the near clip.
                    // Logic C&P'd from LLViewerMediaFocus::setCameraZoom() and then edited as needed

                    LLBBox bbox = objectp->getBoundingBoxAgent();
                    LLVector3d center(gAgent.getPosGlobalFromAgent(bbox.getCenterAgent()));
                    F32 height;
                    F32 width;
                    F32 depth;
                    F32 angle_of_view;
                    F32 distance;

                    LLVector3d target_pos(center);
                    LLVector3d camera_dir(gAgentCamera.getCameraPositionGlobal() - target_pos);
                    camera_dir.normalize();

                    // We need the aspect ratio, and the 3 components of the bbox as height, width, and depth.
                    F32 aspect_ratio(LLViewerMediaFocus::getBBoxAspectRatio(bbox, LLVector3(camera_dir), &height, &width, &depth));
                    F32 camera_aspect(LLViewerCamera::getInstance()->getAspect());

                    // We will normally use the side of the volume aligned with the short side of the screen (i.e. the height for
                    // a screen in a landscape aspect ratio), however there is an edge case where the aspect ratio of the object is
                    // more extreme than the screen.  In this case we invert the logic, using the longer component of both the object
                    // and the screen.
                    bool invert((camera_aspect > 1.0f && aspect_ratio > camera_aspect) || (camera_aspect < 1.0f && aspect_ratio < camera_aspect));

                    // To calculate the optimum viewing distance we will need the angle of the shorter side of the view rectangle.
                    // In portrait mode this is the width, and in landscape it is the height.
                    // We then calculate the distance based on the corresponding side of the object bbox (width for portrait, height for landscape)
                    // We will add half the depth of the bounding box, as the distance projection uses the center point of the bbox.
                    if (camera_aspect < 1.0f || invert)
                    {
                        angle_of_view = llmax(0.1f, LLViewerCamera::getInstance()->getView() * LLViewerCamera::getInstance()->getAspect());
                        distance = width * 0.5f * 1.1f / tanf(angle_of_view * 0.5f);
                    }
                    else
                    {
                        angle_of_view = llmax(0.1f, LLViewerCamera::getInstance()->getView());
                        distance = height * 0.5f * 1.1f / tanf(angle_of_view * 0.5f);
                    }

                    distance += depth * 0.5f;

                    // Verify that the bounding box isn't inside the near clip.  Using OBB-plane intersection to check if the
                    // near-clip plane intersects with the bounding box, and if it does, adjust the distance such that the
                    // object doesn't clip.
                    LLVector3d bbox_extents(bbox.getExtentLocal());
                    LLVector3d axis_x = LLVector3d(1, 0, 0) * bbox.getRotation();
                    LLVector3d axis_y = LLVector3d(0, 1, 0) * bbox.getRotation();
                    LLVector3d axis_z = LLVector3d(0, 0, 1) * bbox.getRotation();
                    //Normal of nearclip plane is camera_dir.
                    F32 min_near_clip_dist = (F32)(bbox_extents.mdV[0] * (camera_dir * axis_x) + bbox_extents.mdV[1] * (camera_dir * axis_y) + bbox_extents.mdV[2] * (camera_dir * axis_z)); // http://www.gamasutra.com/view/feature/131790/simple_intersection_tests_for_games.php?page=7
                    F32 camera_to_near_clip_dist(LLViewerCamera::getInstance()->getNear());
                    F32 min_camera_dist(min_near_clip_dist + camera_to_near_clip_dist);
                    if (distance < min_camera_dist)
                    {
                        // Camera is too close to object, some parts MIGHT clip.  Move camera away to the position where clipping barely doesn't happen.
                        distance = min_camera_dist;
                    }

                    LLVector3d camera_pos(target_pos + camera_dir * distance);

                    if (camera_dir == LLVector3d::z_axis || camera_dir == LLVector3d::z_axis_neg)
                    {
                        // If the direction points directly up, the camera will "flip" around.
                        // We try to avoid this by adjusting the target camera position a
                        // smidge towards current camera position
                        // *NOTE: this solution is not perfect.  All it attempts to solve is the
                        // "looking down" problem where the camera flips around when it animates
                        // to that position.  You still are not guaranteed to be looking at the
                        // object in the correct orientation.  What this solution does is it will
                        // put the camera into position keeping as best it can the current
                        // orientation with respect to the direction wanted.  In other words, if
                        // before zoom the object appears "upside down" from the camera, after
                        /// zooming it will still be upside down, but at least it will not flip.
                        LLVector3d cur_camera_pos = LLVector3d(gAgentCamera.getCameraPositionGlobal());
                        LLVector3d delta = (cur_camera_pos - camera_pos);
                        F64 len = delta.length();
                        delta.normalize();
                        // Move 1% of the distance towards original camera location
                        camera_pos += 0.01 * len * delta;
                    }

                    gAgentCamera.setCameraPosAndFocusGlobal(camera_pos, target_pos, objectp->getID());

                    // *TODO: Re-enable joystick flycam if we disabled it earlier...  Have to find some form of callback as re-enabling at this point causes the camera motion to not happen. ~Cron
                    //if (fly_cam_status)
                    //{
                    //  LLViewerJoystick::getInstance()->toggleFlycam();
                    //}
                }
                break;
                default:
                    break;
                }
            }
        }
    }
        break;

    case 'i': // inspect
    case 'e': // edit
    case 'd': // delete
    case 'r': // return
    {
        // select the objects first
        LLSelectMgr::getInstance()->deselectAll();
        std::vector<LLScrollListItem*> selected = mResultList->getAllSelected();

        for (const auto item : selected)
        {
            const LLUUID& object_id = item->getUUID();
            LLViewerObject* objectp = gObjectList.findObject(object_id);
            if (objectp)
            {
                LLSelectMgr::getInstance()->selectObjectAndFamily(objectp);
                if (c == 'r')
                {
                    // need to set permissions for object return
                    LLSelectNode* node = LLSelectMgr::getInstance()->getSelection()->findNode(objectp);
                    if (!node)
                    {
                        break;
                    }

                    if (!mFSAreaSearch || mFSAreaSearch->mObjectDetails.end() == mFSAreaSearch->mObjectDetails.find(object_id))
                    {
                        break;
                    }

                    FSObjectProperties& details = mFSAreaSearch->mObjectDetails[object_id];
                    node->mValid = true;
                    node->mPermissions->init(details.creator_id, details.owner_id, details.last_owner_id, details.group_id);
                    node->mPermissions->initMasks(details.base_mask, details.owner_mask, details.everyone_mask, details.group_mask, details.next_owner_mask);
                    node->mAggregatePerm = details.ag_perms;
                }
            }
        }

        // now act on those selected objects
        switch (c)
        {
        case 'i': // inspect
            LLFloaterReg::showInstance("inspect");
            break;
        case 'e': // edit
            handle_object_edit();
            break;
        case 'd': // delete
            handle_object_delete();
            break;
        case 'r': // return
            if (RlvActions::isRlvEnabled() && !rlvCanDeleteOrReturn())
            {
                break;
            }
            handle_object_return();
            break;
        default:
            break;
        }
    }
        break;
    default:
        break;
    }

    return true;
}

// static
void FSPanelAreaSearchList::touchObject(LLViewerObject* objectp)
{
    // *NOTE: Hope the packets arrive safely and in order or else
    // there will be some problems.
    if ( (!RlvActions::isRlvEnabled()) || (RlvActions::canTouch(objectp)) )
    {
        LLPickInfo pick; // default constructor will set sane values.
        send_ObjectGrab_message(objectp, pick, LLVector3::zero);
        send_ObjectDeGrab_message(objectp, pick);
    }
}

void FSPanelAreaSearchList::buyObject(FSObjectProperties& details, LLViewerObject* objectp)
{
    LLSelectMgr::getInstance()->deselectAll();
    LLSelectMgr::getInstance()->selectObjectAndFamily(objectp);
    LLSelectNode* node = LLSelectMgr::getInstance()->getSelection()->findNode(objectp);

    if (node)
    {
        node->mValid = true;
        node->mPermissions->init(details.creator_id, details.owner_id, details.last_owner_id, details.group_id);
        node->mPermissions->initMasks(details.base_mask, details.owner_mask, details.everyone_mask, details.group_mask, details.next_owner_mask);
        node->mSaleInfo = details.sale_info;
        node->mAggregatePerm = details.ag_perms;
        node->mCategory = details.category;
        node->mName.assign(details.name);
        node->mDescription.assign(details.description);

        handle_buy();
    }
    else
    {
        LL_WARNS("FSAreaSearch") << "No LLSelectNode node" << LL_ENDL;
    }
}

void FSPanelAreaSearchList::sitOnObject(FSObjectProperties& details, LLViewerObject* objectp)
{
    if ( (!RlvActions::isRlvEnabled()) || (RlvActions::canSit(objectp)) )
    {
        LLSelectMgr::getInstance()->deselectAll();
        LLSelectMgr::getInstance()->selectObjectAndFamily(objectp);
        LLSelectNode* node = LLSelectMgr::getInstance()->getSelection()->findNode(objectp);
        if (node)
        {
            gMessageSystem->newMessageFast(_PREHASH_AgentRequestSit);
            gMessageSystem->nextBlockFast(_PREHASH_AgentData);
            gMessageSystem->addUUIDFast(_PREHASH_AgentID, gAgentID);
            gMessageSystem->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
            gMessageSystem->nextBlockFast(_PREHASH_TargetObject);
            gMessageSystem->addUUIDFast(_PREHASH_TargetID, objectp->mID);
            gMessageSystem->addVector3Fast(_PREHASH_Offset, LLVector3::zero);
            objectp->getRegion()->sendReliableMessage();
        }
        else
        {
            LL_WARNS("FSAreaSearch") << "No LLSelectNode node" << LL_ENDL;
        }
    }
}

//---------------------------------------------------------------------------
// Find panel
//---------------------------------------------------------------------------

FSPanelAreaSearchFind::FSPanelAreaSearchFind(FSAreaSearch* pointer)
:   LLPanel(),
    mFSAreaSearch(pointer)
{
}

bool FSPanelAreaSearchFind::postBuild()
{
    mNameLineEditor = getChild<LLLineEditor>("name_search");
    mNameLineEditor->setCommitCallback(boost::bind(&FSAreaSearch::onCommitLine, mFSAreaSearch));

    mDescriptionLineEditor = getChild<LLLineEditor>("description_search");
    mDescriptionLineEditor->setCommitCallback(boost::bind(&FSAreaSearch::onCommitLine, mFSAreaSearch));

    mOwnerLineEditor = getChild<LLLineEditor>("owner_search");
    mOwnerLineEditor->setCommitCallback(boost::bind(&FSAreaSearch::onCommitLine, mFSAreaSearch));

    mGroupLineEditor = getChild<LLLineEditor>("group_search");
    mGroupLineEditor->setCommitCallback(boost::bind(&FSAreaSearch::onCommitLine, mFSAreaSearch));

    mCreatorLineEditor = getChild<LLLineEditor>("creator_search");
    mCreatorLineEditor->setCommitCallback(boost::bind(&FSAreaSearch::onCommitLine, mFSAreaSearch));

    mLastOwnerLineEditor = getChild<LLLineEditor>("last_owner_search");
    mLastOwnerLineEditor->setCommitCallback(boost::bind(&FSAreaSearch::onCommitLine, mFSAreaSearch));

    mCheckboxRegex = getChild<LLCheckBoxCtrl>("regular_expression");
    mCheckboxRegex->setCommitCallback(boost::bind(&FSAreaSearch::onCommitCheckboxRegex, mFSAreaSearch));

    mSearchButton = getChild<LLButton>("search");
    mSearchButton->setClickedCallback(boost::bind(&FSAreaSearch::onButtonClickedSearch, mFSAreaSearch));

    mClearButton = getChild<LLButton>("clear");
    mClearButton->setClickedCallback(boost::bind(&FSPanelAreaSearchFind::onButtonClickedClear, this));

    return LLPanel::postBuild();
}

void FSPanelAreaSearchFind::onButtonClickedClear()
{
    mNameLineEditor->clear();
    mDescriptionLineEditor->clear();
    mOwnerLineEditor->clear();
    mGroupLineEditor->clear();
    mCreatorLineEditor->clear();
    mLastOwnerLineEditor->clear();
    mFSAreaSearch->clearSearchText();
}

// handle the "enter" key
bool FSPanelAreaSearchFind::handleKeyHere(KEY key, MASK mask)
{
    if (KEY_RETURN == key)
    {
        mFSAreaSearch->onButtonClickedSearch();
        return true;
    }

    return LLPanel::handleKeyHere(key, mask);
}


//---------------------------------------------------------------------------
// Filter panel
//---------------------------------------------------------------------------

FSPanelAreaSearchFilter::FSPanelAreaSearchFilter(FSAreaSearch* pointer)
:   LLPanel(),
    mFSAreaSearch(pointer)
{
}

bool FSPanelAreaSearchFilter::postBuild()
{
    mCheckboxLocked = getChild<LLCheckBoxCtrl>("filter_locked");
    mCheckboxLocked->setCommitCallback(boost::bind(&FSPanelAreaSearchFilter::onCommitCheckbox, this));

    mCheckboxPhysical = getChild<LLCheckBoxCtrl>("filter_physical");
    mCheckboxPhysical->setEnabled(false);
    mCheckboxPhysical->setCommitCallback(boost::bind(&FSPanelAreaSearchFilter::onCommitCheckbox, this));

    mCheckboxTemporary = getChild<LLCheckBoxCtrl>("filter_temporary");
    mCheckboxTemporary->setEnabled(false);
    mCheckboxTemporary->setCommitCallback(boost::bind(&FSPanelAreaSearchFilter::onCommitCheckbox, this));

    mCheckboxPhantom = getChild<LLCheckBoxCtrl>("filter_phantom");
    mCheckboxPhantom->setCommitCallback(boost::bind(&FSPanelAreaSearchFilter::onCommitCheckbox, this));

    mCheckboxForSale = getChild<LLCheckBoxCtrl>("filter_for_sale");
    mCheckboxForSale->setCommitCallback(boost::bind(&FSPanelAreaSearchFilter::onCommitCheckbox, this));

    mCheckboxAttachment = getChild<LLCheckBoxCtrl>("filter_attachment");
    mCheckboxAttachment->setEnabled(false);
    mCheckboxAttachment->setCommitCallback(boost::bind(&FSPanelAreaSearchFilter::onCommitCheckbox, this));

    mSpinForSaleMinValue= getChild<LLSpinCtrl>("min_price");
    mSpinForSaleMinValue->setCommitCallback(boost::bind(&FSPanelAreaSearchFilter::onCommitSpin, this));

    mSpinForSaleMaxValue= getChild<LLSpinCtrl>("max_price");
    mSpinForSaleMaxValue->setCommitCallback(boost::bind(&FSPanelAreaSearchFilter::onCommitSpin, this));

    mComboClickAction = getChild<LLComboBox>("click_action");
    mComboClickAction->setCommitCallback(boost::bind(&FSPanelAreaSearchFilter::onCommitCombo, this));

    mCheckboxExcludeAttachment = getChild<LLCheckBoxCtrl>("exclude_attachment");
    mCheckboxExcludeAttachment->set(true);
    mCheckboxExcludeAttachment->setCommitCallback(boost::bind(&FSPanelAreaSearchFilter::onCommitCheckbox, this));

    mCheckboxExcludePhysics = getChild<LLCheckBoxCtrl>("exclude_physical");
    mCheckboxExcludePhysics->set(true);
    mCheckboxExcludePhysics->setCommitCallback(boost::bind(&FSPanelAreaSearchFilter::onCommitCheckbox, this));

    mCheckboxExcludetemporary = getChild<LLCheckBoxCtrl>("exclude_temporary");
    mCheckboxExcludetemporary->set(true);
    mCheckboxExcludetemporary->setCommitCallback(boost::bind(&FSPanelAreaSearchFilter::onCommitCheckbox, this));

    mCheckboxExcludeReflectionProbes = getChild<LLCheckBoxCtrl>("exclude_reflection_probes");
    mCheckboxExcludeReflectionProbes->set(false);
    mCheckboxExcludeReflectionProbes->setCommitCallback(boost::bind(&FSPanelAreaSearchFilter::onCommitCheckbox, this));

    mCheckboxExcludeChildPrim = getChild<LLCheckBoxCtrl>("exclude_childprim");
    mCheckboxExcludeChildPrim->set(true);
    mCheckboxExcludeChildPrim->setCommitCallback(boost::bind(&FSPanelAreaSearchFilter::onCommitCheckbox, this));

    mCheckboxExcludeNeighborRegions = getChild<LLCheckBoxCtrl>("exclude_neighbor_region");
    mCheckboxExcludeNeighborRegions->set(true);
    mCheckboxExcludeNeighborRegions->setCommitCallback(boost::bind(&FSPanelAreaSearchFilter::onCommitCheckbox, this));

    mButtonApply = getChild<LLButton>("apply");
    mButtonApply->setClickedCallback(boost::bind(&FSAreaSearch::onButtonClickedSearch, mFSAreaSearch));

    mCheckboxDistance = getChild<LLCheckBoxCtrl>("filter_distance");
    mCheckboxDistance->setCommitCallback(boost::bind(&FSPanelAreaSearchFilter::onCommitCheckbox, this));

    mSpinDistanceMinValue = getChild<LLSpinCtrl>("min_distance");
    mSpinDistanceMinValue->setCommitCallback(boost::bind(&FSPanelAreaSearchFilter::onCommitSpin, this));

    mSpinDistanceMaxValue= getChild<LLSpinCtrl>("max_distance");
    mSpinDistanceMaxValue->setCommitCallback(boost::bind(&FSPanelAreaSearchFilter::onCommitSpin, this));

    mCheckboxMoaP = getChild<LLCheckBoxCtrl>("filter_moap");
    mCheckboxMoaP->setCommitCallback(boost::bind(&FSPanelAreaSearchFilter::onCommitCheckbox, this));

    mCheckboxReflectionProbe = getChild<LLCheckBoxCtrl>("filter_reflection_probe");
    mCheckboxReflectionProbe->setCommitCallback(boost::bind(&FSPanelAreaSearchFilter::onCommitCheckbox, this));

    mCheckboxPermCopy = getChild<LLCheckBoxCtrl>("filter_perm_copy");
    mCheckboxPermCopy->setCommitCallback(boost::bind(&FSPanelAreaSearchFilter::onCommitCheckbox, this));

    mCheckboxPermModify = getChild<LLCheckBoxCtrl>("filter_perm_modify");
    mCheckboxPermModify->setCommitCallback(boost::bind(&FSPanelAreaSearchFilter::onCommitCheckbox, this));

    mCheckboxPermTransfer = getChild<LLCheckBoxCtrl>("filter_perm_transfer");
    mCheckboxPermTransfer->setCommitCallback(boost::bind(&FSPanelAreaSearchFilter::onCommitCheckbox, this));

    mCheckboxAgentParcelOnly = getChild<LLCheckBoxCtrl>("filter_agent_parcel_only");
    mCheckboxAgentParcelOnly->setCommitCallback(boost::bind(&FSPanelAreaSearchFilter::onCommitCheckbox, this));

    return LLPanel::postBuild();
}

void FSPanelAreaSearchFilter::onCommitCheckbox()
{
    mFSAreaSearch->setFilterLocked(mCheckboxLocked->get());
    mFSAreaSearch->setFilterPhantom(mCheckboxPhantom->get());
    mFSAreaSearch->setFilterForSale(mCheckboxForSale->get());
    mFSAreaSearch->setFilterDistance(mCheckboxDistance->get());
    mFSAreaSearch->setFilterMoaP(mCheckboxMoaP->get());
    mFSAreaSearch->setFilterReflectionProbe(mCheckboxReflectionProbe->get());

    if (mCheckboxExcludePhysics->get())
    {
        mFSAreaSearch->setFilterPhysical(false);
        mCheckboxPhysical->set(false);
        mCheckboxPhysical->setEnabled(false);
        mFSAreaSearch->setExcludePhysics(true);
    }
    else
    {
        mCheckboxPhysical->setEnabled(true);
        mFSAreaSearch->setExcludePhysics(false);
    }
    mFSAreaSearch->setFilterPhysical(mCheckboxPhysical->get());

    if (mCheckboxExcludetemporary->get())
    {
        mFSAreaSearch->setFilterTemporary(false);
        mCheckboxTemporary->set(false);
        mCheckboxTemporary->setEnabled(false);
        mFSAreaSearch->setExcludetemporary(true);
    }
    else
    {
        mCheckboxTemporary->setEnabled(true);
        mFSAreaSearch->setExcludetemporary(false);
    }
    mFSAreaSearch->setFilterTemporary(mCheckboxTemporary->get());

    if (mCheckboxExcludeReflectionProbes->get())
    {
        mFSAreaSearch->setFilterReflectionProbe(false);
        mCheckboxReflectionProbe->set(false);
        mCheckboxReflectionProbe->setEnabled(false);
        mFSAreaSearch->setExcludeReflectionProbe(true);
    }
    else
    {
        mCheckboxReflectionProbe->setEnabled(true);
        mFSAreaSearch->setExcludeReflectionProbe(false);
    }

    if (mCheckboxExcludeAttachment->get())
    {
        mFSAreaSearch->setFilterAttachment(false);
        mCheckboxAttachment->set(false);
        mCheckboxAttachment->setEnabled(false);
        mFSAreaSearch->setExcludeAttachment(true);
    }
    else
    {
        mCheckboxAttachment->setEnabled(true);
        mFSAreaSearch->setExcludeAttachment(false);
    }
    mFSAreaSearch->setFilterAttachment(mCheckboxAttachment->get());

    mFSAreaSearch->setExcludeChildPrims(mCheckboxExcludeChildPrim->get());

    mFSAreaSearch->setExcludeNeighborRegions(mCheckboxExcludeNeighborRegions->get());

    mFSAreaSearch->setFilterPermCopy(mCheckboxPermCopy->get());
    mFSAreaSearch->setFilterPermModify(mCheckboxPermModify->get());
    mFSAreaSearch->setFilterPermTransfer(mCheckboxPermTransfer->get());

    mFSAreaSearch->setFilterAgentParcelOnly(mCheckboxAgentParcelOnly->get());
}

void FSPanelAreaSearchFilter::onCommitSpin()
{
    mFSAreaSearch->setFilterForSaleMin(mSpinForSaleMinValue->getValue().asInteger());
    mFSAreaSearch->setFilterForSaleMax(mSpinForSaleMaxValue->getValue().asInteger());

    mFSAreaSearch->setFilterDistanceMin(mSpinDistanceMinValue->getValue().asInteger());
    mFSAreaSearch->setFilterDistanceMax(mSpinDistanceMaxValue->getValue().asInteger());
}

void FSPanelAreaSearchFilter::onCommitCombo()
{
    if (mComboClickAction->getCurrentIndex() > 0)
    {
        mFSAreaSearch->setFilterClickAction(true);
        mFSAreaSearch->setFilterClickActionType((U8)mComboClickAction->getCurrentIndex());
    }
    else
    {
        mFSAreaSearch->setFilterClickAction(false);
        mFSAreaSearch->setFilterClickActionType(0);
    }
}

//---------------------------------------------------------------------------
// Options tab
//---------------------------------------------------------------------------

FSPanelAreaSearchOptions::FSPanelAreaSearchOptions(FSAreaSearch* pointer)
:   LLPanel(),
    mFSAreaSearch(pointer)
{
    mCommitCallbackRegistrar.add("AreaSearch.DisplayColumn", boost::bind(&FSPanelAreaSearchOptions::onCommitCheckboxDisplayColumn, this, _2));
    mEnableCallbackRegistrar.add("AreaSearch.EnableColumn", boost::bind(&FSPanelAreaSearchOptions::onEnableColumnVisibilityChecked, this, _2));
}

void FSPanelAreaSearchOptions::onCommitCheckboxDisplayColumn(const LLSD& userdata)
{
    const std::string& column_name = userdata.asStringRef();
    if (column_name.empty())
    {
        LL_WARNS("FSAreaSearch") << "Missing action text." << LL_ENDL;
        return;
    }

    mFSAreaSearch->getPanelList()->onColumnVisibilityChecked(userdata);
}

bool FSPanelAreaSearchOptions::onEnableColumnVisibilityChecked(const LLSD& userdata)
{
    return mFSAreaSearch->getPanelList()->onEnableColumnVisibilityChecked(userdata);
}


//---------------------------------------------------------------------------
// Advanced tab
//---------------------------------------------------------------------------

bool FSPanelAreaSearchAdvanced::postBuild()
{
    mCheckboxClickTouch = getChild<LLCheckBoxCtrl>("double_click_touch");
    mCheckboxClickBuy = getChild<LLCheckBoxCtrl>("double_click_buy");
    mCheckboxClickSit = getChild<LLCheckBoxCtrl>("double_click_sit");

    return LLPanel::postBuild();
}
