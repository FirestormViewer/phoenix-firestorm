/* Copyright (c) 2009
 *
 * Modular Systems Ltd. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the following
 * conditions are met:
 *
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *   3. Neither the name Modular Systems Ltd nor the names of its contributors
 *      may be used to endorse or promote products derived from this
 *      software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY MODULAR SYSTEMS LTD AND CONTRIBUTORS “AS IS”
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MODULAR SYSTEMS OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Modified, debugged, optimized and improved by Henri Beauchamp Feb 2010.
 * Refactored for Viewer2 by Kadah Coba, April 2011
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

const std::string request_string = "FSAreaSearch::Requested_ø§µ";
const F32 min_refresh_interval = 0.25f;	// Minimum interval between list refreshes in seconds.


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
mCounterText(0),
mResultList(0)
{
	mLastUpdateTimer.reset();
}

FSAreaSearch::~FSAreaSearch()
{
	if (mParcelChangedObserver)
	{
		LLViewerParcelMgr::getInstance()->removeObserver(mParcelChangedObserver);
		delete mParcelChangedObserver;
	}
}

BOOL FSAreaSearch::postBuild()
{
	mResultList = getChild<LLScrollListCtrl>("result_list");
	mResultList->setDoubleClickCallback( boost::bind(&FSAreaSearch::onDoubleClick, this));
	mResultList->sortByColumn("Name", TRUE);

	mCounterText = getChild<LLTextBox>("counter");

	childSetAction("Refresh", boost::bind(&FSAreaSearch::search, this));
	childSetAction("Stop", boost::bind(&FSAreaSearch::cancel, this));
	
	getChild<LLLineEditor>("Name query chunk")->setKeystrokeCallback( boost::bind(&FSAreaSearch::onCommitLine, this, _1, _2),NULL);
	getChild<LLLineEditor>("Description query chunk")->setKeystrokeCallback( boost::bind(&FSAreaSearch::onCommitLine, this, _1, _2),NULL);
	getChild<LLLineEditor>("Owner query chunk")->setKeystrokeCallback( boost::bind(&FSAreaSearch::onCommitLine, this, _1, _2),NULL);
	getChild<LLLineEditor>("Group query chunk")->setKeystrokeCallback( boost::bind(&FSAreaSearch::onCommitLine, this, _1, _2),NULL);

	mParcelChangedObserver = new FSParcelChangeObserver(this);
	LLViewerParcelMgr::getInstance()->addObserver(mParcelChangedObserver);

	return TRUE;
}

void FSAreaSearch::checkRegion()
{
	// Check if we changed region, and if we did, clear the object details cache.
	LLViewerRegion* region = gAgent.getRegion();
	if (region != mLastRegion)
	{
		mLastRegion = region;
		mRequested = 0;
		mObjectDetails.clear();
		
		mResultList->deleteAllItems();
		mCounterText->setText(std::string("Listed/Pending/Total"));
	}
}

void FSAreaSearch::onDoubleClick()
{
 	LLScrollListItem *item = mResultList->getFirstSelected();
	if (!item) return;
	LLUUID object_id = item->getUUID();
	LLViewerObject* objectp = gObjectList.findObject(object_id);
	if (objectp)
	{
		LLTracker::trackLocation(objectp->getPositionGlobal(), mObjectDetails[object_id].name, "", LLTracker::LOCATION_ITEM);
	}
}

void FSAreaSearch::cancel()
{
	checkRegion();
	closeFloater();
	
	mSearchedName = "";
	mSearchedDesc = "";
	mSearchedOwner = "";
	mSearchedGroup = "";
}

void FSAreaSearch::search()
{
	checkRegion();
	results();
}

void FSAreaSearch::onCommitLine(LLLineEditor* line, void* user_data)
{
	std::string name = line->getName();
	std::string text = line->getText();

	if (name == "Name query chunk") mSearchedName = text;
	else if (name == "Description query chunk") mSearchedDesc = text;
	else if (name == "Owner query chunk") mSearchedOwner = text;
	else if (name == "Group query chunk") mSearchedGroup = text;

	if (text.length() > 3)
	{
		checkRegion();
		results();
	}
}

void FSAreaSearch::requestIfNeeded(LLViewerObject *objectp)
{
	LLUUID object_id = objectp->getID();
	if (mObjectDetails.count(object_id) == 0)
	{
		AObjectDetails* details = &mObjectDetails[object_id];
		details->name = request_string;
		details->desc = request_string;
		details->owner_id = LLUUID::null;
		details->group_id = LLUUID::null;

		LLMessageSystem* msg = gMessageSystem;
		msg->newMessageFast(_PREHASH_RequestObjectPropertiesFamily);
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
		msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
		msg->nextBlockFast(_PREHASH_ObjectData);
		msg->addU32Fast(_PREHASH_RequestFlags, 0 );
		msg->addUUIDFast(_PREHASH_ObjectID, object_id);
		gAgent.sendReliableMessage();
		mRequested++;
	}
}

void FSAreaSearch::results()
{
	if (!getVisible()) return;
	if (mRequested > 0 && mLastUpdateTimer.getElapsedTimeF32() < min_refresh_interval) return;

	const LLUUID selected = mResultList->getCurrentID();
	const S32 scrollpos = mResultList->getScrollPos();
	mResultList->deleteAllItems();

	S32 i;
	S32 total = gObjectList.getNumObjects();
	LLViewerRegion* our_region = gAgent.getRegion();
	for (i = 0; i < total; i++)
	{
		LLViewerObject *objectp = gObjectList.getObject(i);
		if (objectp)
		{
			if (objectp->getRegion() == our_region && !objectp->isAvatar() && objectp->isRoot() &&
				!objectp->flagTemporary() && !objectp->flagTemporaryOnRez())
			{
				LLUUID object_id = objectp->getID();
				if (mObjectDetails.count(object_id) == 0)
				{
					requestIfNeeded(objectp);
				}
				else
				{
					AObjectDetails* details = &mObjectDetails[object_id];
					std::string object_name = details->name;
					std::string object_desc = details->desc;
					std::string object_owner;
					std::string object_group;
					gCacheName->getFullName(details->owner_id, object_owner);
					gCacheName->getGroupName(details->group_id, object_group);
					if (object_name != request_string)
					{
						if ((mSearchedName == "" || boost::ifind_first(object_name, mSearchedName)) &&
							(mSearchedDesc == "" || boost::ifind_first(object_desc, mSearchedDesc)) &&
							(mSearchedOwner == "" || boost::ifind_first(object_owner, mSearchedOwner)) &&
							(mSearchedGroup == "" || boost::ifind_first(object_group, mSearchedGroup)))
						{
							LLSD element;
							element["id"] = object_id;
							element["columns"][LIST_OBJECT_NAME]["column"] = "Name";
							element["columns"][LIST_OBJECT_NAME]["type"] = "text";
							element["columns"][LIST_OBJECT_NAME]["value"] = details->name;
							element["columns"][LIST_OBJECT_DESC]["column"] = "Description";
							element["columns"][LIST_OBJECT_DESC]["type"] = "text";
							element["columns"][LIST_OBJECT_DESC]["value"] = details->desc;
							element["columns"][LIST_OBJECT_OWNER]["column"] = "Owner";
							element["columns"][LIST_OBJECT_OWNER]["type"] = "text";
							element["columns"][LIST_OBJECT_OWNER]["value"] = object_owner;
							element["columns"][LIST_OBJECT_GROUP]["column"] = "Group";
							element["columns"][LIST_OBJECT_GROUP]["type"] = "text";
							element["columns"][LIST_OBJECT_GROUP]["value"] = object_group;
							mResultList->addElement(element, ADD_BOTTOM);
						}
					}
				}
			}
		}
	}

	mResultList->updateSort();
	mResultList->selectByID(selected);
	mResultList->setScrollPos(scrollpos);
	mCounterText->setText(llformat("%d listed/%d pending/%d total", mResultList->getItemCount(), mRequested, mObjectDetails.size()));
	mLastUpdateTimer.reset();
}


void FSAreaSearch::callbackLoadOwnerName(const LLUUID& id, const std::string& full_name)
{
	results();
}

void FSAreaSearch::processObjectPropertiesFamily(LLMessageSystem* msg)
{
	checkRegion();

	LLUUID object_id;
	msg->getUUIDFast(_PREHASH_ObjectData, _PREHASH_ObjectID, object_id);

	bool exists = (mObjectDetails.count(object_id) != 0);
	AObjectDetails* details = &mObjectDetails[object_id];
	if (!exists || details->name == request_string)
	{
		// We cache unknown objects (to avoid having to request them later)
		// and requested objects.
		if (exists && mRequested > 0) mRequested--;
		msg->getUUIDFast(_PREHASH_ObjectData, _PREHASH_OwnerID, details->owner_id);
		msg->getUUIDFast(_PREHASH_ObjectData, _PREHASH_GroupID, details->group_id);
		msg->getStringFast(_PREHASH_ObjectData, _PREHASH_Name, details->name);
		msg->getStringFast(_PREHASH_ObjectData, _PREHASH_Description, details->desc);
		gCacheName->get(details->owner_id, false, boost::bind(
							&FSAreaSearch::callbackLoadOwnerName, this, _1, _2));
		gCacheName->get(details->group_id, true, boost::bind(
							&FSAreaSearch::callbackLoadOwnerName, this, _1, _2));
	}
}
