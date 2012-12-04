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
 * http://www.phoenixviewer.com
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "fsareasearch.h"

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
#include "llviewermenu.h"		// handle_object_touch(), handle_buy()
#include "llclickaction.h"
#include "lltabcontainer.h"
#include "llspinctrl.h"
#include "lltoolgrab.h"
#include "fslslbridge.h"
#include "llcombobox.h"
#include "llnotificationsutil.h"
#include "fswsassetblacklist.h"


// max number of objects that can be (de-)selected in a single packet.
const S32 MAX_OBJECTS_PER_PACKET = 254;

// time in seconds between refreshes when active
const F32 REFRESH_INTERVAL = 1.0f;

// this is used to prevent refreshing too often and affecting performance.
const F32 MIN_REFRESH_INTERVAL = 0.25f;

// how far the avatar needs to move to trigger a distance update
const F32 MIN_DISTANCE_MOVED = 1.0f;


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

FSAreaSearch::FSAreaSearch(const LLSD& key) :  
	LLFloater(key),
	mActive(false),
	mFilterForSale(false),
	mFilterForSaleMin(0),
	mFilterForSaleMax(999999),
	mFilterPhysicial(false),
	mFilterTemporary(false),
	mRegexSearch(false),
	mFilterClickAction(false),
	mFilterLocked(false),
	mFilterPhantom(false),
	mFilterAttachment(false),
	mFilterMoaP(false),
	mFilterDistance(false),
	mFilterDistanceMin(0),
	mFilterDistanceMax(999999),
	mBeaconColor(),
	mBeaconTextColor(),
	mBeacons(false),
	mExcludeAttachment(true),
	mExcludeTempary(true),
	mExcludePhysics(true)
{
	//TODO: Multi-floater support and get rid of the singletin.
	mInstance = this;
  
	mFactoryMap["area_search_list_panel"] = LLCallbackMap(createPanelList, this);
	mFactoryMap["area_search_find_panel"] = LLCallbackMap(createPanelFind, this);
	mFactoryMap["area_search_filter_panel"] = LLCallbackMap(createPanelFilter, this);
	mFactoryMap["area_search_advanced_panel"] = LLCallbackMap(createPanelAdvanced, this);
	mFactoryMap["area_search_options_panel"] = LLCallbackMap(createPanelOptions, this);
  
	// Register an idle update callback
	gIdleCallbacks.addFunction(idle, this);
	
	mParcelChangedObserver = new FSParcelChangeObserver(this);
	LLViewerParcelMgr::getInstance()->addObserver(mParcelChangedObserver);
}

FSAreaSearch::~FSAreaSearch()
{
	if (!gIdleCallbacks.deleteFunction(idle, this))
	{
		LL_WARNS("FSAreaSearch") << "FSAreaSearch::~FSAreaSearch() failed to delete callback" << LL_ENDL;
	}

	if (mParcelChangedObserver)
	{
		LLViewerParcelMgr::getInstance()->removeObserver(mParcelChangedObserver);
		delete mParcelChangedObserver;
		mParcelChangedObserver = NULL;
	}
}

BOOL FSAreaSearch::postBuild()
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

	// TODO: add area search settings to the color.xml file
	mBeaconColor = LLUIColorTable::getInstance()->getColor("PathfindingLinksetBeaconColor");
	mBeaconTextColor = LLUIColorTable::getInstance()->getColor("PathfindingDefaultBeaconTextColor");
	mBeaconLineWidth = gSavedSettings.getS32("DebugBeaconLineWidth");

	return LLFloater::postBuild();
}

void FSAreaSearch::draw()
{
	LLFloater::draw();
	
	if (mBeacons)
	{
		std::vector<LLScrollListItem*> items = mPanelList->getResultList()->getAllData();

		for (std::vector<LLScrollListItem*>::const_iterator item_it = items.begin();
			item_it != items.end();
			++item_it)
		{
			const LLScrollListItem* item = (*item_it);
			LLViewerObject* objectp = gObjectList.findObject(item->getUUID());

			if (objectp)
			{
				const std::string &objectName = mObjectDetails[item->getUUID()].description;
				gObjectList.addDebugBeacon(objectp->getPositionAgent(), objectName, mBeaconColor, mBeaconTextColor, mBeaconLineWidth);
			}
		}
	}
}

//static
void FSAreaSearch::idle(void* user_data)
{
	FSAreaSearch* self = (FSAreaSearch*)user_data;
	self->findObjects();
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
	self->mPanelAdvanced = new FSPanelAreaSearchAdvanced(self);
	return self->mPanelAdvanced;
}

// static
void* FSAreaSearch::createPanelOptions(void* data)
{
	FSAreaSearch* self = (FSAreaSearch*)data;
	self->mPanelOptions = new FSPanelAreaSearchOptions(self);
	return self->mPanelOptions;
}

void FSAreaSearch::checkRegion()
{
	if (mInstance && mActive)
	{
		// Check if we changed region, and if we did, clear the object details cache.
		LLViewerRegion* region = gAgent.getRegion();
		if (region != mLastRegion)
		{
			mLastRegion = region;
			mRequested = 0;
			mObjectDetails.clear();
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
	}
	else
	{
		for (std::map<LLUUID, FSObjectProperties>::iterator object_it = mObjectDetails.begin();
		object_it != mObjectDetails.end();
		++object_it)
		{
			 object_it->second.listed = false;
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
	
	LL_DEBUGS("FSAreaSearch") << "Doing a FSAreaSearch::findObjects" << LL_ENDL;
	
	mLastUpdateTimer.stop(); // stop sets getElapsedTimeF32() time to zero.
	checkRegion();
	mRefresh = false;
	mSearchableObjects = 0;
	S32 object_count = gObjectList.getNumObjects();
	std::vector<U32> request_list;
	LLViewerRegion* our_region = gAgent.getRegion();

	for (S32 i = 0; i < object_count; i++)
	{
		LLViewerObject *objectp = gObjectList.getObject(i);
		if (!(objectp && isSearchableObject(objectp, our_region)))
		{
			continue;
		}

		LLUUID object_id = objectp->getID();

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
			request_list.push_back(objectp->getLocalID());

			mRequested++;
		}
		else
		{
			FSObjectProperties& details = mObjectDetails[object_id];
			if (details.valid == true)
			{
				matchObject(details, objectp);
			}
		}

	}

	// Select then de-deslect the objects to get properties.
	requestObjectProperties(request_list, true);
	requestObjectProperties(request_list, false);

	mPanelList->updateScrollList();

	//TODO: make mRequested more accruate.
	// requests for non-existent objects will never arrive,
	// update mRequested counter to reflect that.
	for (std::map<LLUUID, FSObjectProperties>::iterator object_it = mObjectDetails.begin();
		object_it != mObjectDetails.end();
		++object_it)
	{
		if (!object_it->second.valid)
		{
			LLUUID id = object_it->second.id;
			LLViewerObject* objectp = gObjectList.findObject(id);
			if (((!objectp) || (!isSearchableObject(objectp, our_region))) && (mRequested > 0))
			{
				mRequested--;
			}
		}
	}

	updateCounterText();
	mLastUpdateTimer.start(); // start also reset elapsed time to zero
}

bool FSAreaSearch::isSearchableObject(LLViewerObject* objectp, LLViewerRegion* our_region)
{ 
	//TODO: add child prim support
	if (!(objectp->isRoot() || (objectp->isAttachment() && objectp->isRootEdit())))
	{
		return false;
	}
	
	//TODO: add mult-region support
	if (!(objectp->getRegion() == our_region))
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

	if (mExcludeAttachment && objectp->isAttachment())
	{
		return false;
	}
	
	if (mExcludePhysics && objectp->flagUsePhysics())
	{
		return false;
	}
	
	if (mExcludeTempary && objectp->flagTemporaryOnRez())
	{
		return false;
	}

	return true;
}

void FSAreaSearch::requestObjectProperties(const std::vector<U32>& request_list, bool select)
{
	bool start_new_message = true;
	S32 select_count = 0;
	LLMessageSystem* msg = gMessageSystem;
	LLViewerRegion* regionp = gAgent.getRegion();
	
	for (std::vector<U32>::const_iterator iter = request_list.begin();
			iter != request_list.end(); ++iter)
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
			msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
			msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
			select_count++;
			start_new_message = false;
		}
		
		msg->nextBlockFast(_PREHASH_ObjectData);
		msg->addU32Fast(_PREHASH_ObjectLocalID, (*iter) );
		select_count++;
		
		if(msg->isSendFull(NULL) || select_count >= MAX_OBJECTS_PER_PACKET)
		{
			LL_DEBUGS("FSAreaSearch") << "Sent one" << (select ? "ObjectSelect" : "ObjectDeselect") << "message" << LL_ENDL;
			msg->sendReliable(regionp->getHost() );
			select_count = 0;
			start_new_message = true;
		}
	}

	if (!start_new_message)
	{
		LL_DEBUGS("FSAreaSearch") << "Sent one" << (select ? "ObjectSelect" : "ObjectDeselect") << "message" << LL_ENDL;
		msg->sendReliable(regionp->getHost() );
	}
}

void FSAreaSearch::processObjectProperties(LLMessageSystem* msg)
{
	// This fuction is called by llviewermessage even if no floater has been created.
	if (!(mInstance && mActive))
	{
	      return;
	}

	LLViewerRegion* our_region = gAgent.getRegion();
	bool counter_text_update = false;

	S32 count = msg->getNumberOfBlocksFast(_PREHASH_ObjectData);
	LL_DEBUGS("FSAreaSearch")  << "Got processObjectProperties message with " << count << "object(s)" << LL_ENDL;
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
		if (!(objectp && objectp->isRootEdit()))
		{
			continue;
		}

		FSObjectProperties& details = mObjectDetails[object_id];
		if (!details.valid)
		{
			// We cache un-requested objects (to avoid having to request them later)
			// and requested objects.

			details.valid = true;

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
					memcpy(tid.mData, packed_buffer + buf_offset, UUID_BYTES);		/* Flawfinder: ignore */
					details.texture_ids.push_back(tid);
				}
			}

			details.permissions.init(details.creator_id, details.owner_id, details.last_owner_id, details.group_id);
			details.permissions.initMasks(details.base_mask, details.owner_mask, details.everyone_mask, details.group_mask, details.next_owner_mask);

			// Sets the group owned BOOL and real owner id, group or owner depending if object is group owned.
			details.permissions.getOwnership(details.ownership_id, details.group_owned);
			
			LL_DEBUGS("FSAreaSearch") << "Got properties for object: " << object_id << LL_ENDL;

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
		S32 distance = dist_vec(mPanelList->getAgentLastPosition(), objectp->getPositionGlobal());// used mAgentLastPosition instead of gAgent->getPositionGlobal for performace
		if (!(distance >= mFilterDistanceMin && distance <= mFilterDistanceMax))
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
	
	//TODO: texture id search
// 	for (uuid_vec_t::const_iterator texture_it = details.texture_ids.begin();
// 			 texture_it != details.texture_ids.end(); ++texture_it)
// 		{
// 			if ( "" == (*texture_it).asString())
// 			{
// 			}
// 		}

	if (mFilterPhysicial && !objectp->flagUsePhysics())
	{
		return;
	}

	if (mFilterTemporary && !objectp->flagTemporaryOnRez())
	{
		return;
	}
	
	if (mFilterLocked && !objectp->flagObjectPermanent())
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
		for(U8 i = 0; i < texture_count; i++)
		{
			if(objectp->getTE(i)->hasMedia())
			{
				moap = true;
				break;
			}
		}

		if(!moap)
		{
			return;
		}
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
	
	LLScrollListColumn* column;
	LLSD element;
	element["id"] = object_id;

	column = mPanelList->getResultList()->getColumn("distance");
	if (column)
	{
		element["columns"][column->mIndex]["column"] = "distance";
		element["columns"][column->mIndex]["value"] = llformat("%1.0f m", dist_vec(mPanelList->getAgentLastPosition(), objectp->getPositionGlobal())); // used mAgentLastPosition instead of gAgent->getPositionGlobal for performace
	}
	
	column = mPanelList->getResultList()->getColumn("name");
	if (column)
	{
		element["columns"][column->mIndex]["column"] = "name";
		element["columns"][column->mIndex]["value"] = details.name;
	}

	column = mPanelList->getResultList()->getColumn("description");
	if (column)
	{
		element["columns"][column->mIndex]["column"] = "description";
		element["columns"][column->mIndex]["value"] = details.description;
	}

	column = mPanelList->getResultList()->getColumn("owner");
	if (column)
	{
		element["columns"][column->mIndex]["column"] = "owner";
		element["columns"][column->mIndex]["value"] = owner_name;
	}

	column = mPanelList->getResultList()->getColumn("group");
	if (column)
	{
		element["columns"][column->mIndex]["column"] = "group";
		element["columns"][column->mIndex]["value"] = group_name;
	}

	column = mPanelList->getResultList()->getColumn("creator");
	if (column)
	{
		element["columns"][column->mIndex]["column"] = "creator";
		element["columns"][column->mIndex]["value"] = creator_name;
	}

	column = mPanelList->getResultList()->getColumn("last_owner");
	if (column)
	{
		element["columns"][column->mIndex]["column"] = "last_owner";
		element["columns"][column->mIndex]["value"] = last_owner_name;
	}
	
	LLScrollListItem* list_row =  mPanelList->getResultList()->addElement(element);

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
}

void FSAreaSearch::getNameFromUUID(LLUUID& id, std::string& name, BOOL group, bool& name_requested)
{
	BOOL is_group;
	
	if(!gCacheName->getIfThere(id, name, is_group))
	{
		if(std::find(mNamesRequested.begin(), mNamesRequested.end(), id) == mNamesRequested.end())
		{
			mNamesRequested.push_back(id);
			gCacheName->get(id, group, boost::bind(&FSAreaSearch::callbackLoadFullName, this, _1, _2));
		}
		name_requested = true;
	}
}

void FSAreaSearch::callbackLoadFullName(const LLUUID& id, const std::string& full_name)
{
	LLViewerRegion* our_region = gAgent.getRegion();
	
	for (std::map<LLUUID, FSObjectProperties>::iterator object_it = mObjectDetails.begin();
		object_it != mObjectDetails.end();
		++object_it)
	{
		if (object_it->second.name_requested && !object_it->second.listed)
		{
			LLUUID id = object_it->second.id;
			LLViewerObject* objectp = gObjectList.findObject(id);
			if (objectp && isSearchableObject(objectp, our_region))
			{
				matchObject(object_it->second, objectp);
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

bool FSAreaSearch::regexTest(std::string text)
{
	// couple regex patters one can use for testing. The regex will match a UUID.
	// boost::regex pattern("[\\w]{8}-[\\w]{4}-[\\w]{4}-[\\w]{4}-[\\w]{12}");
	// [\p{XDigit}]{8}(-[\p{XDigit}]{4}){3}-[\p{XDigit}]{12}
	// to find all objects that don't belong to a group, use (?!^Name of the group$).* in the group field.
  
	try
	{
		std::string test_text = "asdfghjklqwerty1234567890";
		boost::regex pattern(text.c_str());
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


//---------------------------------------------------------------------------
// List panel
//---------------------------------------------------------------------------

FSPanelAreaSearchList::FSPanelAreaSearchList(FSAreaSearch* pointer)
:	LLPanel(),
	mCounterText(0),
	mResultList(0),
	mFSAreaSearch(pointer)
{	
	// Set up context menu.
	LLUICtrl::CommitCallbackRegistry::ScopedRegistrar registrar;
	LLUICtrl::EnableCallbackRegistry::ScopedRegistrar enable_registrar;

	registrar.add("AreaSearch.Action", boost::bind(&FSPanelAreaSearchList::onContextMenuItemClick, this, _2));
	enable_registrar.add("AreaSearch.Enable", boost::bind(&FSPanelAreaSearchList::onContextMenuItemEnable, this, _2));
	
	mPopupMenu = LLUICtrlFactory::getInstance()->createFromFile<LLContextMenu>(
				"menu_fs_area_search.xml", LLContextMenu::sMenuContainer, LLMenuHolderGL::child_registry_t::instance());
}

BOOL FSPanelAreaSearchList::postBuild()
{
	mResultList = getChild<LLScrollListCtrl>("result_list");
	mResultList->setDoubleClickCallback(boost::bind(&FSPanelAreaSearchList::onDoubleClick, this));
	mResultList->sortByColumn("name", TRUE);

	mCounterText = getChild<LLTextBox>("counter");

	mRefreshButton = getChild<LLButton>("Refresh");
	mRefreshButton->setClickedCallback(boost::bind(&FSPanelAreaSearchList::onClickRefresh, this));
	
	mCheckboxBeacons = getChild<LLCheckBoxCtrl>("beacons");
	mCheckboxBeacons->setCommitCallback(boost::bind(&FSPanelAreaSearchList::onCommitCheckboxBeacons, this));

	mAgentLastPosition = gAgent.getPositionGlobal();

	return LLPanel::postBuild();
}

// virtual
FSPanelAreaSearchList::~FSPanelAreaSearchList()
{ }

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
	LLScrollListItem *item = mResultList->getFirstSelected();
	if (!item) return;
	LLUUID object_id = item->getUUID();
	LLViewerObject* objectp = gObjectList.findObject(object_id);
	if (objectp)
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
	for (std::vector<LLScrollListItem*>::iterator item_it = items.begin();
		item_it != items.end();
		++item_it)
	{
		LLScrollListItem* item = (*item_it);
		LLUUID row_id = item->getUUID();
		LLViewerObject* objectp = gObjectList.findObject(row_id);
		
		if ((!objectp) || (!mFSAreaSearch->isSearchableObject(objectp, our_region)))
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
				item->getColumn(distance_column->mIndex)->setValue(LLSD(llformat("%1.0f m", dist_vec(current_agent_position, objectp->getPositionGlobal()))));
			}
		}
	}

	if (deleted || agent_moved)
	{
		mResultList->updateLayout();
	}
}

void FSPanelAreaSearchList::updateName(LLUUID id, std::string name)
{
	LLScrollListColumn* creator_column = mResultList->getColumn("creator");
	LLScrollListColumn* owner_column = mResultList->getColumn("owner");
	LLScrollListColumn* group_column = mResultList->getColumn("group");
	LLScrollListColumn* last_owner_column = mResultList->getColumn("last_owner");
  
	// Iterate over the rows in the list, updating the ones with matching id.
	std::vector<LLScrollListItem*> items = mResultList->getAllData();

	for (std::vector<LLScrollListItem*>::iterator item_it = items.begin();
		item_it != items.end();
		++item_it)
	{
		LLScrollListItem* item = (*item_it);
		LLUUID row_id = item->getUUID();
		FSObjectProperties& details = mFSAreaSearch->mObjectDetails[row_id];

		if (creator_column && (id == details.creator_id))
		{
			item->getColumn(creator_column->mIndex)->setValue(LLSD(name));
		}

		if (owner_column && (id == details.owner_id))
		{
			item->getColumn(owner_column->mIndex)->setValue(LLSD(name));
		}

		if (group_column && (id == details.group_id))
		{
			item->getColumn(group_column->mIndex)->setValue(LLSD(name));
		}

		if (last_owner_column && (id == details.last_owner_id))
		{
			item->getColumn(last_owner_column->mIndex)->setValue(LLSD(name));
		}
	}
}

// virtual
BOOL FSPanelAreaSearchList::handleRightMouseDown(S32 x, S32 y, MASK mask)
{
	BOOL handled = LLUICtrl::handleRightMouseDown(x, y, mask);
	
	// BUG: hitItem is off by three rows up.
	// when fixed, selectByID(item->getValue()) if nothing was selected.
// 	LLScrollListItem *item = mResultList->hitItem(x, y);
// 	if (item)
// 	{
// 		mResultList->selectByID(item->getValue());
// 		
// 	}
	
	if (mPopupMenu)
	{
		if (mResultList->getNumSelected() !=0)
		{
			mPopupMenu->show(x, y);
			LLMenuGL::showPopup(this, mPopupMenu, x, y);
			return TRUE;
		}
	}

	return handled;
}

bool FSPanelAreaSearchList::onContextMenuItemEnable(const LLSD& userdata)
{
	std::string parameter = userdata.asString();
	if (parameter == "one")
	{
		// return true if just one item is selected.
		return (mResultList->getNumSelected() == 1);
	}
	else
	{
		// return true if more then one is selected, but not just one.
		return (mResultList->getNumSelected() > 1);
	}
}

bool FSPanelAreaSearchList::onContextMenuItemClick(const LLSD& userdata)
{
	std::string action = userdata.asString();
	LL_DEBUGS("FSAreaSearch") << "Right click menu " << action << " was selected." << LL_ENDL;

	// NOTE that each action command MUST begin with a different letter.
	char c = action.at(0);
	switch(c)
	{
	case 't': // touch
	case 's': // script
	case 'l': // blacklist
	{
		std::vector<LLScrollListItem*> selected = mResultList->getAllSelected();

		for(std::vector<LLScrollListItem*>::iterator item_it = selected.begin();
		  item_it != selected.end(); ++item_it)
		{
			switch (c)
			{
			case 't': // touch
			{
				LLViewerObject* objectp = gObjectList.findObject((*item_it)->getUUID());
				if (objectp)
				{
					touchObject(objectp);
				}
			}
				break;
			case 's': // script
				FSLSLBridge::instance().viewerToLSL("getScriptInfo|" + (*item_it)->getUUID().asString());
				break;
			case 'l': // blacklist
			{
				LLUUID object_id = (*item_it)->getUUID();
				LLViewerObject* objectp = gObjectList.findObject(object_id);
				if (objectp)
				{
					std::string region_name;
					LLViewerRegion* region = objectp->getRegion();
					if (region)
					{
						region_name = objectp->getRegion()->getName();
					}
					FSWSAssetBlacklist::getInstance()->addNewItemToBlacklist(object_id, mFSAreaSearch->mObjectDetails[object_id].name, region_name, LLAssetType::AT_OBJECT);
					gObjectList.killObject(objectp);
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
	{
		LLUUID object_id = mResultList->getFirstSelected()->getUUID();
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
			default:
				break;
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

		for(std::vector<LLScrollListItem*>::iterator item_it = selected.begin();
		  item_it != selected.end(); ++item_it)
		{
			LLUUID object_id = (*item_it)->getUUID();
			LLViewerObject* objectp = gObjectList.findObject(object_id);
			if (objectp)
			{
				LLSelectMgr::getInstance()->selectObjectAndFamily(objectp);
				if ( c == 'r' )
				{
					// need to set permissions for object return
					LLSelectNode* node = LLSelectMgr::getInstance()->getSelection()->findNode(objectp);
					FSObjectProperties& details = mFSAreaSearch->mObjectDetails[object_id];
					node->mValid = TRUE;
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

void FSPanelAreaSearchList::touchObject(LLViewerObject* objectp)
{
	// *NOTE: Hope the packets arrive safely and in order or else
	// there will be some problems.
	LLPickInfo pick; // default constructor will set sane values.
	send_ObjectGrab_message(objectp, pick, LLVector3::zero);
	send_ObjectDeGrab_message(objectp, pick);
}

void FSPanelAreaSearchList::buyObject(FSObjectProperties& details, LLViewerObject* objectp)
{
	LLSelectMgr::getInstance()->deselectAll();
	LLSelectMgr::getInstance()->selectObjectAndFamily(objectp);
	LLSelectNode* node = LLSelectMgr::getInstance()->getSelection()->findNode(objectp);
	
	if (node)
	{
		node->mValid = TRUE;
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

//---------------------------------------------------------------------------
// Find panel
//---------------------------------------------------------------------------

FSPanelAreaSearchFind::FSPanelAreaSearchFind(FSAreaSearch* pointer)
:	LLPanel(),
	mFSAreaSearch(pointer)
{
}

BOOL FSPanelAreaSearchFind::postBuild()
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

// virtual
FSPanelAreaSearchFind::~FSPanelAreaSearchFind()
{ }

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
BOOL FSPanelAreaSearchFind::handleKeyHere(KEY key, MASK mask)
{
	if( KEY_RETURN == key )
	{
		mFSAreaSearch->onButtonClickedSearch();
		return TRUE;
	}

	return LLPanel::handleKeyHere(key, mask);
}


//---------------------------------------------------------------------------
// Filter panel
//---------------------------------------------------------------------------

FSPanelAreaSearchFilter::FSPanelAreaSearchFilter(FSAreaSearch* pointer)
:	LLPanel(),
	mFSAreaSearch(pointer)
{
}

BOOL FSPanelAreaSearchFilter::postBuild()
{
	mCheckboxLocked = getChild<LLCheckBoxCtrl>("filter_locked");
	mCheckboxLocked->setCommitCallback(boost::bind(&FSPanelAreaSearchFilter::onCommitCheckbox, this));
	
	mCheckboxPhysical = getChild<LLCheckBoxCtrl>("filter_physical");
	mCheckboxPhysical->setEnabled(FALSE);
	mCheckboxPhysical->setCommitCallback(boost::bind(&FSPanelAreaSearchFilter::onCommitCheckbox, this));
	
	mCheckboxTemporary = getChild<LLCheckBoxCtrl>("filter_temporary");
	mCheckboxTemporary->setEnabled(FALSE);
	mCheckboxTemporary->setCommitCallback(boost::bind(&FSPanelAreaSearchFilter::onCommitCheckbox, this));

	mCheckboxPhantom = getChild<LLCheckBoxCtrl>("filter_phantom");
	mCheckboxPhantom->setCommitCallback(boost::bind(&FSPanelAreaSearchFilter::onCommitCheckbox, this));
  
	mCheckboxForSale = getChild<LLCheckBoxCtrl>("filter_for_sale");
	mCheckboxForSale->setCommitCallback(boost::bind(&FSPanelAreaSearchFilter::onCommitCheckbox, this));
	
	mCheckboxAttachment = getChild<LLCheckBoxCtrl>("filter_attachment");
	mCheckboxAttachment->setEnabled(FALSE);
	mCheckboxAttachment->setCommitCallback(boost::bind(&FSPanelAreaSearchFilter::onCommitCheckbox, this));
	
	mSpinForSaleMinValue= getChild<LLSpinCtrl>("min_price");
	mSpinForSaleMinValue->setCommitCallback(boost::bind(&FSPanelAreaSearchFilter::onCommitSpin, this));
	
	mSpinForSaleMaxValue= getChild<LLSpinCtrl>("max_price");
	mSpinForSaleMaxValue->setCommitCallback(boost::bind(&FSPanelAreaSearchFilter::onCommitSpin, this));

	mComboClickAction = getChild<LLComboBox>("click_action");
	mComboClickAction->setCommitCallback(boost::bind(&FSPanelAreaSearchFilter::onCommitCombo, this));
	
	mCheckboxExcludeAttachment = getChild<LLCheckBoxCtrl>("exclude_attachment");
	mCheckboxExcludeAttachment->set(TRUE);
	mCheckboxExcludeAttachment->setCommitCallback(boost::bind(&FSPanelAreaSearchFilter::onCommitCheckbox, this));
	
	mCheckboxExcludePhysics = getChild<LLCheckBoxCtrl>("exclude_physical");
	mCheckboxExcludePhysics->set(TRUE);
	mCheckboxExcludePhysics->setCommitCallback(boost::bind(&FSPanelAreaSearchFilter::onCommitCheckbox, this));
	
	mCheckboxExcludeTempary = getChild<LLCheckBoxCtrl>("exclude_temporary");
	mCheckboxExcludeTempary->set(TRUE);
	mCheckboxExcludeTempary->setCommitCallback(boost::bind(&FSPanelAreaSearchFilter::onCommitCheckbox, this));
	
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
	
	return LLPanel::postBuild();
}

// virtual
FSPanelAreaSearchFilter::~FSPanelAreaSearchFilter()
{ }

void FSPanelAreaSearchFilter::onCommitCheckbox()
{
	mFSAreaSearch->setFilterLocked(mCheckboxLocked->get());
	mFSAreaSearch->setFilterPhantom(mCheckboxPhantom->get());
	mFSAreaSearch->setFilterForSale(mCheckboxForSale->get());
	mFSAreaSearch->setFilterDistance(mCheckboxDistance->get());
	mFSAreaSearch->setFilterMoaP(mCheckboxMoaP->get());

	if (mCheckboxExcludePhysics->get())
	{
		mFSAreaSearch->setFilterPhysicial(false);
		mCheckboxPhysical->set(FALSE);
		mCheckboxPhysical->setEnabled(FALSE);
		mFSAreaSearch->setExcludePhysics(true);
	}
	else
	{
		mCheckboxPhysical->setEnabled(TRUE);
		mFSAreaSearch->setExcludePhysics(false);
	}
	mFSAreaSearch->setFilterPhysicial(mCheckboxPhysical->get());

	if (mCheckboxExcludeTempary->get())
	{
		mFSAreaSearch->setFilterTemporary(false);
		mCheckboxTemporary->set(FALSE);
		mCheckboxTemporary->setEnabled(FALSE);
		mFSAreaSearch->setExcludeTempary(true);
	}
	else
	{
		mCheckboxTemporary->setEnabled(TRUE);
		mFSAreaSearch->setExcludeTempary(false);
	}
	mFSAreaSearch->setFilterTemporary(mCheckboxTemporary->get());

	if (mCheckboxExcludeAttachment->get())
	{
		mFSAreaSearch->setFilterAttachment(false);
		mCheckboxAttachment->set(FALSE);
		mCheckboxAttachment->setEnabled(FALSE);
		mFSAreaSearch->setExcludeAttachment(true);
	}
	else
	{
		mCheckboxAttachment->setEnabled(TRUE);
		mFSAreaSearch->setExcludeAttachment(false);
	}
	mFSAreaSearch->setFilterAttachment(mCheckboxAttachment->get());
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
:	LLPanel(),
	mFSAreaSearch(pointer)
{
	mCommitCallbackRegistrar.add("AreaSearch.DisplayColumn", boost::bind(&FSPanelAreaSearchOptions::onCommitCheckboxDisplayColumn, this, _2));
}

// virtual
FSPanelAreaSearchOptions::~FSPanelAreaSearchOptions()
{ }

void FSPanelAreaSearchOptions::onCommitCheckboxDisplayColumn(const LLSD& userdata)
{
	std::string column_name = userdata.asString();
	if (column_name.empty())
	{
		LL_WARNS("FSAreaSearch") << "Missing action text." << LL_ENDL;
		return;
	}

	LLScrollListCtrl* result_list = mFSAreaSearch->getPanelList()->getResultList();
	result_list->deleteAllItems();
	LLCheckBoxCtrl* checkboxctrl = getChild<LLCheckBoxCtrl>("show_" + column_name);

	if (checkboxctrl)
	{
		if (checkboxctrl->get())
		{
			result_list->addColumn(mColumnParms[column_name]);
		}
		else
		{
			mColumnParms[column_name] = result_list->delColumn(column_name);
		}
	}

	result_list->updateLayout();
}


//---------------------------------------------------------------------------
// Advanced tab
//---------------------------------------------------------------------------

FSPanelAreaSearchAdvanced::FSPanelAreaSearchAdvanced(FSAreaSearch* pointer)
:	LLPanel(),
	mFSAreaSearch(pointer)
{
}

BOOL FSPanelAreaSearchAdvanced::postBuild()
{
	mCheckboxClickTouch = getChild<LLCheckBoxCtrl>("double_click_touch");
	mCheckboxClickBuy = getChild<LLCheckBoxCtrl>("double_click_buy");

	return LLPanel::postBuild();
}

// virtual
FSPanelAreaSearchAdvanced::~FSPanelAreaSearchAdvanced()
{ }

