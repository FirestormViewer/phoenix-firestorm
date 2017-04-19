/** 
 * @file animationexplorer.cpp
 * @brief Animation Explorer floater implementation
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 * Copyright (C) 2013, Zi Ree @ Second Life
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
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"
#include "animationexplorer.h"

#include "indra_constants.h"		// for MASK_ALT etc.
#include "message.h"				// for gMessageSystem
//#include "stdenums.h"				// for ADD_TOP
#include "llagent.h"				// for gAgent
#include "llanimationstates.h"
#include "llbutton.h"
#include "llcachename.h"			// for gCacheName
#include "llcheckboxctrl.h"
#include "llfloater.h"
#include "llfloaterreg.h"
#include "llkeyframefallmotion.h"
#include "llrect.h"
#include "llscrolllistctrl.h"
#include "lltimer.h"
#include "lltoolmgr.h"				// for MASK_ORBIT etc.
#include "lltrans.h"
#include "lluuid.h"
#include "llview.h"
#include "llviewerobjectlist.h"
#include "llviewerregion.h"
#include "llviewerwindow.h"			// for gViewerWindow
#include "llvoavatar.h"
#include "llvoavatarself.h"			// for gAgentAvatarp

const S32 MAX_ANIMATIONS=100;

// --------------------------------------------------------------------------

RecentAnimationList::RecentAnimationList()
:	LLSingleton<RecentAnimationList>()
{
}

RecentAnimationList::~RecentAnimationList()
{
}

void RecentAnimationList::addAnimation(const LLUUID& id, const LLUUID& playedBy)
{
	AnimationEntry entry;

	entry.animationID = id;
	entry.playedBy = playedBy;
	entry.time = LLTimer::getElapsedSeconds();

	AnimationExplorer* explorer = LLFloaterReg::findTypedInstance<AnimationExplorer>("animation_explorer");

	// only remember animation when it wasn't played by ourselves or the explorer window is open,
	// so the list doesn't get polluted
	if (playedBy != gAgentAvatarp->getID() || explorer != NULL)
	{
		mAnimationList.push_back(entry);

		// only keep a certain number of entries
		if (mAnimationList.size() > MAX_ANIMATIONS)
		{
			mAnimationList.pop_front();
		}
	}

	// if the animation explorer floater is open, send this animation over immediately
	if (explorer)
	{
		explorer->addAnimation(id, playedBy, LLTimer::getElapsedSeconds());
	}
}

void RecentAnimationList::requestList(AnimationExplorer* explorer)
{
	if (explorer)
	{
		// send the list of recent animations to the given animation explorer floater
		for (std::deque<AnimationEntry>::iterator iter = mAnimationList.begin();
			iter != mAnimationList.end();
			++iter)
		{
			AnimationEntry entry = *iter;
			explorer->addAnimation(entry.animationID, entry.playedBy, entry.time);
		}
	}
}

// --------------------------------------------------------------------------

AnimationExplorer::AnimationExplorer(const LLSD& key)
:	LLFloater(key),
	mPreviewCtrl(NULL),
	mLastMouseX(0),
	mLastMouseY(0)
{
}

AnimationExplorer::~AnimationExplorer()
{
	mAnimationPreview = NULL;
}

void AnimationExplorer::startMotion(const LLUUID& motionID)
{
	if (!mAnimationPreview)
	{
		return;
	}

	LLVOAvatar* avatarp = mAnimationPreview->getDummyAvatar();

	avatarp->deactivateAllMotions();
	avatarp->startMotion(ANIM_AGENT_STAND, 0.0f);

	if (motionID.notNull())
	{
		avatarp->startMotion(motionID, 0.0f);
	}
}

BOOL AnimationExplorer::postBuild()
{
	mAnimationScrollList = getChild<LLScrollListCtrl>("animation_list");
	mStopButton = getChild<LLButton>("stop_btn");
	mRevokeButton = getChild<LLButton>("revoke_btn");
	mStopAndRevokeButton = getChild<LLButton>("stop_and_revoke_btn");
	mNoOwnedAnimationsCheckBox = getChild<LLCheckBoxCtrl>("no_owned_animations_check");

	mAnimationScrollList->setCommitCallback(boost::bind(&AnimationExplorer::onSelectAnimation, this));
	mStopButton->setCommitCallback(boost::bind(&AnimationExplorer::onStopPressed, this));
	mRevokeButton->setCommitCallback(boost::bind(&AnimationExplorer::onRevokePressed, this));
	mStopAndRevokeButton->setCommitCallback(boost::bind(&AnimationExplorer::onStopAndRevokePressed, this));
	mNoOwnedAnimationsCheckBox->setCommitCallback(boost::bind(&AnimationExplorer::onOwnedCheckToggled, this));

	mPreviewCtrl = findChild<LLView>("animation_preview");
	if (mPreviewCtrl)
	{
		if (isAgentAvatarValid())
		{
			mAnimationPreview = new LLPreviewAnimation(mPreviewCtrl->getRect().getWidth(), mPreviewCtrl->getRect().getHeight());
			mAnimationPreview->setZoom(2.0f);
			startMotion(LLUUID::null);
		}
	}
	else
	{
		LL_WARNS("AnimationExplorer") << "Could not find animation preview control to place animation texture" << LL_ENDL;
	}

	// request list of recent animations
	update();

	return TRUE;
}

void AnimationExplorer::onSelectAnimation()
{
	LLScrollListItem* item = mAnimationScrollList->getFirstSelected();
	if (!item)
	{
		return;
	}

	S32 column = mAnimationScrollList->getColumn("animation_id")->mIndex;
	mCurrentAnimationID = item->getColumn(column)->getValue().asUUID();

	column = mAnimationScrollList->getColumn("played_by")->mIndex;
	mCurrentObject = item->getColumn(column)->getValue().asUUID();

	startMotion(mCurrentAnimationID);
}

void AnimationExplorer::onStopPressed()
{
	if (mCurrentAnimationID.notNull())
	{
		gAgentAvatarp->stopMotion(mCurrentAnimationID);
		gAgent.sendAnimationRequest(mCurrentAnimationID, ANIM_REQUEST_STOP);
	}
}

void AnimationExplorer::onRevokePressed()
{
	if (mCurrentObject.notNull())
	{
		LLViewerObject* vo = gObjectList.findObject(mCurrentObject);

		if (vo)
		{
			gAgentAvatarp->revokePermissionsOnObject(vo);
		}
	}
}

void AnimationExplorer::onStopAndRevokePressed()
{
	onStopPressed();
	onRevokePressed();
}

void AnimationExplorer::onOwnedCheckToggled()
{
	update();
	updateList(LLTimer::getElapsedSeconds());
}

void AnimationExplorer::draw()
{
	LLFloater::draw();
	LLRect r = mPreviewCtrl->getRect();

	if (mAnimationPreview)
	{
		mAnimationPreview->requestUpdate();

		gGL.color3f(1.0f, 1.0f, 1.0f);
		gGL.getTexUnit(0)->bind(mAnimationPreview);
		gGL.begin(LLRender::TRIANGLES);
		{
			gGL.texCoord2f(0.0f, 1.0f);
			gGL.vertex2i(r.mLeft, r.mTop);
			gGL.texCoord2f(0.0f, 0.0f);
			gGL.vertex2i(r.mLeft, r.mBottom);
			gGL.texCoord2f(1.0f, 0.0f);
			gGL.vertex2i(r.mRight, r.mBottom);

			gGL.texCoord2f(0.0f, 1.0f);
			gGL.vertex2i(r.mLeft, r.mTop);
			gGL.texCoord2f(1.0f, 0.0f);
			gGL.vertex2i(r.mRight, r.mBottom);
			gGL.texCoord2f(1.0f, 1.0f);
			gGL.vertex2i(r.mRight, r.mTop);
		}
		gGL.end();
		gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
	}

	// update tiems and "Still playing" status in the list once every few seconds
	static F64 last_update = 0.0;
	F64 time = LLTimer::getElapsedSeconds();
	if (time - last_update > 5.0)
	{
		last_update = time;
		updateList(time);
	}
}

void AnimationExplorer::update()
{
	// stop playing preview animations when reloading the list
	startMotion(LLUUID::null);

	mAnimationScrollList->deleteAllItems();
	RecentAnimationList::instance().requestList(this);
}

void AnimationExplorer::updateList(F64 current_timestamp)
{
	S32 played_column = mAnimationScrollList->getColumn("played")->mIndex;
	S32 timestamp_column = mAnimationScrollList->getColumn("timestamp")->mIndex;
	S32 priority_column = mAnimationScrollList->getColumn("priority")->mIndex;
	S32 object_id_column = mAnimationScrollList->getColumn("object_id")->mIndex;
	S32 anim_id_column = mAnimationScrollList->getColumn("animation_id")->mIndex;

	// go through the full animation scroll list
	std::vector<LLScrollListItem*> items = mAnimationScrollList->getAllData();
	for (std::vector<LLScrollListItem*>::iterator list_iter = items.begin(); list_iter != items.end(); ++list_iter)
	{
		LLScrollListItem* item = *list_iter;

		// get a pointer to the "Played" column text
		LLScrollListText* played_text = dynamic_cast<LLScrollListText*>(item->getColumn(played_column));

		// get the object ID from the list
		LLUUID object_id = item->getColumn(object_id_column)->getValue().asUUID();

		LLUUID anim_id = item->getColumn(anim_id_column)->getValue().asUUID();

		// assume this animation is not running first
		bool is_running = false;

		// go through the list of playing animations to find out if this animation played by
		// this object is still running
		for (LLVOAvatar::AnimSourceIterator anim_iter = gAgentAvatarp->mAnimationSources.begin();
			anim_iter != gAgentAvatarp->mAnimationSources.end(); ++anim_iter)
		{
			// object and animation found
			if (anim_iter->first == object_id && anim_iter->second == anim_id)
			{
				// set text to "Still playing" and break out of this loop
				played_text->setText(LLTrans::getString("animation_explorer_still_playing"));
				is_running = true;
				break;
			}
		}

		// animation was not found to be running
		if (!is_running)
		{
			// get timestamp when this animation was started
			F64 timestamp = item->getColumn(timestamp_column)->getValue().asReal();

			// update text to show the number of seconds ago when this animation was started
			LLStringUtil::format_map_t args;
			args["SECONDS"] = llformat("%d", (S32) (current_timestamp - timestamp));

			played_text->setText(LLTrans::getString("animation_explorer_seconds_ago", args));
		}

		std::string prio_text = LLTrans::getString("animation_explorer_unknown_priority");
		LLKeyframeMotion* motion = dynamic_cast<LLKeyframeMotion*>(gAgentAvatarp->findMotion(anim_id));
		if (motion)
		{
			prio_text = llformat("%d", (S32)motion->getPriority());
		}
		dynamic_cast<LLScrollListText*>(item->getColumn(priority_column))->setText(prio_text);
	}
}

void AnimationExplorer::addAnimation(const LLUUID& id, const LLUUID& played_by, F64 time)
{
	// don't add animations that are played by ourselves when the filter box is checked
	if (played_by == gAgentAvatarp->getID())
	{
		if (mNoOwnedAnimationsCheckBox->getValue().asBoolean())
		{
			return;
		}
	}

	// set object name to UUID at first
	std::string playedByName = played_by.asString();

	// find out if the object is still in reach
	LLViewerObject* vo = gObjectList.findObject(played_by);
	if (vo)
	{
		// if it was an avatar, get the name here
		if (vo->isAvatar())
		{
			playedByName = std::string(vo->getNVPair("FirstName")->getString()) + " " +
				std::string(vo->getNVPair("LastName")->getString());
		}
		// not an avatar, do a lookup by UUID
		else
		{
			// find out if we know the name to this UUID already
			std::map<LLUUID, std::string>::iterator iter = mKnownIDs.find(played_by);
			// if we don't know it yet, start a lookup
			if (iter == mKnownIDs.end())
			{
				// if we are not already looking up this object's name, send a request out
				if (std::find(mRequestedIDs.begin(), mRequestedIDs.end(), played_by) == mRequestedIDs.end())
				{
					// remember which object names we already requested
					mRequestedIDs.push_back(played_by);

					LLMessageSystem* msg = gMessageSystem;
					
					msg->newMessageFast(_PREHASH_ObjectSelect);
					msg->nextBlockFast(_PREHASH_AgentData);
					msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
					msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
					msg->nextBlockFast(_PREHASH_ObjectData);
					msg->addU32Fast(_PREHASH_ObjectLocalID, vo->getLocalID());
					msg->sendReliable(gAgentAvatarp->getRegion()->getHost());

					msg->newMessageFast(_PREHASH_ObjectDeselect);
					msg->nextBlockFast(_PREHASH_AgentData);
					msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
					msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
					msg->nextBlockFast(_PREHASH_ObjectData);
					msg->addU32Fast(_PREHASH_ObjectLocalID, vo->getLocalID());
					msg->sendReliable(gAgentAvatarp->getRegion()->getHost());
				}
			}
			else
			{
				// we know the name already
				playedByName = mKnownIDs[played_by];
			}
		}
	}

	// insert the item into the scroll list
	LLSD item;
	item["columns"][0]["column"] = "played_by";
	item["columns"][0]["value"] = playedByName;
	item["columns"][1]["column"] = "played";
	item["columns"][1]["value"] = LLTrans::getString("animation_explorer_still_playing");
	item["columns"][2]["column"] = "priority";
	item["columns"][2]["value"] = LLTrans::getString("animation_explorer_unknown_priority");
	item["columns"][3]["column"] = "timestamp";
	item["columns"][3]["value"] = time;
	item["columns"][4]["column"] = "animation_id";
	item["columns"][4]["value"] = id;
	item["columns"][5]["column"] = "object_id";
	item["columns"][5]["value"] = played_by;

	mAnimationScrollList->addElement(item, ADD_TOP);
}

void AnimationExplorer::requestNameCallback(LLMessageSystem* msg)
{
	// if we weren't looking for any IDs, ignore this callback
	if (mRequestedIDs.empty())
	{
		return;
	}

	// we might have received more than one answer in one block
	S32 num = msg->getNumberOfBlocksFast(_PREHASH_ObjectData);
	for (S32 index = 0; index < num; ++index)
	{
		LLUUID object_id;
		msg->getUUIDFast(_PREHASH_ObjectData, _PREHASH_ObjectID, object_id, index);

		uuid_vec_t::iterator iter;
		iter = std::find(mRequestedIDs.begin(), mRequestedIDs.end(), object_id);
		// if this is one of the objects we were looking for, process the data
		if (iter != mRequestedIDs.end())
		{
			// get the name of the object
			std::string object_name;
			msg->getStringFast(_PREHASH_ObjectData, _PREHASH_Name, object_name, index);

			// remove the object from the lookup list and add it to the known names list
			mRequestedIDs.erase(iter);
			mKnownIDs[object_id] = object_name;

			S32 object_id_column = mAnimationScrollList->getColumn("object_id")->mIndex;
			S32 played_by_column = mAnimationScrollList->getColumn("played_by")->mIndex;

			// find all scroll list entries with this object UUID and update the names there
			std::vector<LLScrollListItem*> items = mAnimationScrollList->getAllData();
			for (std::vector<LLScrollListItem*>::iterator list_iter = items.begin(); list_iter != items.end(); ++list_iter)
			{
				LLScrollListItem* item = *list_iter;
				LLUUID list_object_id = item->getColumn(object_id_column)->getValue().asUUID();

				if (object_id == list_object_id)
				{
					LLScrollListText* played_by_text = (LLScrollListText*)item->getColumn(played_by_column);
					played_by_text->setText(object_name);
				}
			}
		}
	}
}

// Copied from llfloaterbvhpreview.cpp
BOOL AnimationExplorer::handleMouseDown(S32 x, S32 y, MASK mask)
{
	if (mPreviewCtrl && mPreviewCtrl->getRect().pointInRect(x, y))
	{
		bringToFront( x, y );
		gFocusMgr.setMouseCapture(this);
		gViewerWindow->hideCursor();
		mLastMouseX = x;
		mLastMouseY = y;
		return TRUE;
	}

	return LLFloater::handleMouseDown(x, y, mask);
}

// Copied from llfloaterbvhpreview.cpp
BOOL AnimationExplorer::handleMouseUp(S32 x, S32 y, MASK mask)
{
	gFocusMgr.setMouseCapture(FALSE);
	gViewerWindow->showCursor();
	return LLFloater::handleMouseUp(x, y, mask);
}

// (Almost) Copied from llfloaterbvhpreview.cpp
BOOL AnimationExplorer::handleHover(S32 x, S32 y, MASK mask)
{
	if (!mPreviewCtrl || !mAnimationPreview || !mPreviewCtrl->getRect().pointInRect(x, y))
	{
		return LLFloater::handleHover(x, y, mask);
	}

	MASK local_mask = mask & ~MASK_ALT;
	if (mAnimationPreview && hasMouseCapture())
	{
		if (local_mask == MASK_PAN)
		{
			// pan here
			mAnimationPreview->pan((F32)(x - mLastMouseX) * -0.005f, (F32)(y - mLastMouseY) * -0.005f);
		}
		else if (local_mask == MASK_ORBIT)
		{
			F32 yaw_radians = (F32)(x - mLastMouseX) * -0.01f;
			F32 pitch_radians = (F32)(y - mLastMouseY) * 0.02f;
			mAnimationPreview->rotate(yaw_radians, pitch_radians);
		}
		else
		{
			F32 yaw_radians = (F32)(x - mLastMouseX) * -0.01f;
			F32 zoom_amt = (F32)(y - mLastMouseY) * 0.02f;
			mAnimationPreview->rotate(yaw_radians, 0.f);
			mAnimationPreview->zoom(zoom_amt);
		}
		mAnimationPreview->requestUpdate();
		LLUI::setMousePositionLocal(this, mLastMouseX, mLastMouseY);
	}
	else if (local_mask == MASK_ORBIT)
	{
		gViewerWindow->setCursor(UI_CURSOR_TOOLCAMERA);
	}
	else if (local_mask == MASK_PAN)
	{
		gViewerWindow->setCursor(UI_CURSOR_TOOLPAN);
	}
	else
	{
		gViewerWindow->setCursor(UI_CURSOR_TOOLZOOMIN);
	}
	return TRUE;
}

// (Almost) Copied from llfloaterbvhpreview.cpp
BOOL AnimationExplorer::handleScrollWheel(S32 x, S32 y, S32 clicks)
{
	if (mPreviewCtrl && mPreviewCtrl->getRect().pointInRect(x, y))
	{
		mAnimationPreview->zoom((F32)clicks * -0.2f);
		mAnimationPreview->requestUpdate();
		return TRUE;
	}
	return LLFloater::handleScrollWheel(x, y, clicks);
}

// Copied from llfloaterbvhpreview.cpp
void AnimationExplorer::onMouseCaptureLost()
{
	gViewerWindow->showCursor();
}
