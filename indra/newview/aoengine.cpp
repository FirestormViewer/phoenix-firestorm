/**
 * @file aoengine.cpp
 * @brief The core Animation Overrider engine
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2011, Zi Ree @ Second Life
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
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"


#include "roles_constants.h"

#include "aoengine.h"
#include "aoset.h"

#include "llagent.h"
#include "llagentcamera.h"
#include "llanimationstates.h"
#include "llassetstorage.h"
#include "llcommon.h"
#include "llinventoryfunctions.h"		// for ROOT_FIRESTORM_FOLDER
#include "llinventorymodel.h"
#include "llinventoryobserver.h"  
#include "llnotificationsutil.h"
#include "llstring.h"
#include "llvfs.h"
#include "llviewercontrol.h"
#include "llviewerinventory.h"
#include "llinventorybridge.h"

//#define ROOT_FIRESTORM_FOLDER 	"#Firestorm" //moved to llinventoryfunctions.h
#define ROOT_AO_FOLDER			"#AO"
#include <boost/graph/graph_concepts.hpp>

const F32 INVENTORY_POLLING_INTERVAL=5.0f;

AOEngine::AOEngine() :
	LLSingleton<AOEngine>(),
	mCurrentSet(0),
	mDefaultSet(0),
	mEnabled(FALSE),
	mInMouselook(FALSE),
	mUnderWater(FALSE),
	mImportSet(0),
	mImportCategory(LLUUID::null),
	mAOFolder(LLUUID::null),
	mLastMotion(ANIM_AGENT_STAND),
	mLastOverriddenMotion(ANIM_AGENT_STAND)
{
	gSavedPerAccountSettings.getControl("UseAO")->getCommitSignal()->connect(boost::bind(&AOEngine::onToggleAOControl, this));
}

AOEngine::~AOEngine()
{
	clear(false);
}

void AOEngine::init()
{
	enable(mEnabled);
}

// static
void AOEngine::onLoginComplete()
{
	AOEngine::instance().init();
}

void AOEngine::onToggleAOControl()
{
	enable(gSavedPerAccountSettings.getBOOL("UseAO"));
}

void AOEngine::clear( bool aFromTimer )
{
	mOldSets.insert( mOldSets.end(), mSets.begin(), mSets.end() );
	mSets.clear();

	mCurrentSet=0;

	//<ND/> FIRE-3801; We cannot delete any AOSet object if we're called from a timer tick. AOSet is derived from LLEventTimer and destruction will
	// fail in ~LLInstanceTracker when a destructor runs during iteration.
	if( !aFromTimer )
	{
		std::for_each( mOldSets.begin(), mOldSets.end(), DeletePointer( ) );
		mOldSets.clear();

		std::for_each( mOldImportSets.begin(), mOldImportSets.end(), DeletePointer() );
		mOldImportSets.clear();
	}
}

void AOEngine::stopAllStandVariants()
{
	lldebugs << "stopping all STAND variants." << llendl;
	gAgent.sendAnimationRequest(ANIM_AGENT_STAND_1,ANIM_REQUEST_STOP);
	gAgent.sendAnimationRequest(ANIM_AGENT_STAND_2,ANIM_REQUEST_STOP);
	gAgent.sendAnimationRequest(ANIM_AGENT_STAND_3,ANIM_REQUEST_STOP);
	gAgent.sendAnimationRequest(ANIM_AGENT_STAND_4,ANIM_REQUEST_STOP);
	gAgentAvatarp->LLCharacter::stopMotion(ANIM_AGENT_STAND_1);
	gAgentAvatarp->LLCharacter::stopMotion(ANIM_AGENT_STAND_2);
	gAgentAvatarp->LLCharacter::stopMotion(ANIM_AGENT_STAND_3);
	gAgentAvatarp->LLCharacter::stopMotion(ANIM_AGENT_STAND_4);
}

void AOEngine::stopAllSitVariants()
{
	lldebugs << "stopping all SIT variants." << llendl;
	gAgent.sendAnimationRequest(ANIM_AGENT_SIT_FEMALE,ANIM_REQUEST_STOP);
	gAgent.sendAnimationRequest(ANIM_AGENT_SIT_GENERIC,ANIM_REQUEST_STOP);
	gAgent.sendAnimationRequest(ANIM_AGENT_SIT_GROUND,ANIM_REQUEST_STOP);
	gAgent.sendAnimationRequest(ANIM_AGENT_SIT_GROUND_CONSTRAINED,ANIM_REQUEST_STOP);
	gAgentAvatarp->LLCharacter::stopMotion(ANIM_AGENT_SIT_FEMALE);
	gAgentAvatarp->LLCharacter::stopMotion(ANIM_AGENT_SIT_GENERIC);
	gAgentAvatarp->LLCharacter::stopMotion(ANIM_AGENT_SIT_GROUND);
	gAgentAvatarp->LLCharacter::stopMotion(ANIM_AGENT_SIT_GROUND_CONSTRAINED);
}

void AOEngine::setLastMotion(const LLUUID& motion)
{
	if(motion!=ANIM_AGENT_TYPE)
		mLastMotion=motion;
}

void AOEngine::setLastOverriddenMotion(const LLUUID& motion)
{
	if(motion!=ANIM_AGENT_TYPE)
		mLastOverriddenMotion=motion;
}

BOOL AOEngine::foreignAnimations(const LLUUID& seat)
{
	for(LLVOAvatar::AnimSourceIterator sourceIterator=gAgentAvatarp->mAnimationSources.begin();
		sourceIterator!=gAgentAvatarp->mAnimationSources.end();sourceIterator++)
	{
		if(sourceIterator->first!=gAgent.getID())
		{
			if(seat.isNull() || sourceIterator->first==seat)
				return TRUE;
		}
	}
	return FALSE;
}

const LLUUID& AOEngine::mapSwimming(const LLUUID& motion) const
{
	S32 stateNum;

	if(motion==ANIM_AGENT_HOVER)
		stateNum=AOSet::Floating;
	else if(motion==ANIM_AGENT_FLY)
		stateNum=AOSet::SwimmingForward;
	else if(motion==ANIM_AGENT_HOVER_UP)
		stateNum=AOSet::SwimmingUp;
	else if(motion==ANIM_AGENT_HOVER_DOWN)
		stateNum=AOSet::SwimmingDown;
	else
		return LLUUID::null;

	AOSet::AOState* state=mCurrentSet->getState(stateNum);
	return mCurrentSet->getAnimationForState(state);
}

void AOEngine::checkBelowWater(BOOL yes)
{
	if(mUnderWater==yes)
		return;

	// only restart underwater/above water motion if the overridden motion is the one currently playing
	if(mLastMotion!=mLastOverriddenMotion)
		return;

	gAgent.sendAnimationRequest(override(mLastOverriddenMotion,FALSE),ANIM_REQUEST_STOP);
	mUnderWater=yes;
	gAgent.sendAnimationRequest(override(mLastOverriddenMotion,TRUE),ANIM_REQUEST_START);
}

void AOEngine::enable(BOOL yes)
{
	lldebugs << "using " << mLastMotion << " enable " << yes << llendl;
	mEnabled=yes;

	if(!mCurrentSet)
	{
		lldebugs << "enable(" << yes << ") without animation set loaded." << llendl;
		return;
	}

	AOSet::AOState* state=mCurrentSet->getStateByRemapID(mLastMotion);
	if(mEnabled)
	{
		if(state && !state->mAnimations.empty())
		{
			lldebugs << "Enabling animation state " << state->mName << llendl;

			gAgent.sendAnimationRequest(mLastOverriddenMotion,ANIM_REQUEST_STOP);

			LLUUID animation=override(mLastMotion,TRUE);
			if(animation.isNull())
				return;

			if(mLastMotion==ANIM_AGENT_STAND)
			{
				stopAllStandVariants();
			}
			else if(mLastMotion==ANIM_AGENT_WALK)
			{
				lldebugs << "Last motion was a WALK, stopping all variants." << llendl;
				gAgent.sendAnimationRequest(ANIM_AGENT_WALK_NEW,ANIM_REQUEST_STOP);
				gAgent.sendAnimationRequest(ANIM_AGENT_FEMALE_WALK,ANIM_REQUEST_STOP);
				gAgent.sendAnimationRequest(ANIM_AGENT_FEMALE_WALK_NEW,ANIM_REQUEST_STOP);
				gAgentAvatarp->LLCharacter::stopMotion(ANIM_AGENT_WALK_NEW);
				gAgentAvatarp->LLCharacter::stopMotion(ANIM_AGENT_FEMALE_WALK);
				gAgentAvatarp->LLCharacter::stopMotion(ANIM_AGENT_FEMALE_WALK_NEW);
			}
			else if(mLastMotion==ANIM_AGENT_RUN)
			{
				lldebugs << "Last motion was a RUN, stopping all variants." << llendl;
				gAgent.sendAnimationRequest(ANIM_AGENT_RUN_NEW,ANIM_REQUEST_STOP);
				gAgent.sendAnimationRequest(ANIM_AGENT_FEMALE_RUN_NEW,ANIM_REQUEST_STOP);
				gAgentAvatarp->LLCharacter::stopMotion(ANIM_AGENT_RUN_NEW);
				gAgentAvatarp->LLCharacter::stopMotion(ANIM_AGENT_FEMALE_RUN_NEW);
			}
			else if(mLastMotion==ANIM_AGENT_SIT)
			{
				stopAllSitVariants();
				gAgent.sendAnimationRequest(ANIM_AGENT_SIT_GENERIC,ANIM_REQUEST_START);
			}
			else
				llwarns << "Unhandled last motion id " << mLastMotion << llendl;

			gAgent.sendAnimationRequest(animation,ANIM_REQUEST_START);
		}
	}
	else
	{
		gAgent.sendAnimationRequest(ANIM_AGENT_SIT_GENERIC,ANIM_REQUEST_STOP);
		// stop all overriders, catch leftovers
		for(S32 index=0;index<AOSet::AOSTATES_MAX;index++)
		{
			state=mCurrentSet->getState(index);
			if(state)
			{
				LLUUID animation=state->mCurrentAnimationID;
				if(animation.notNull())
				{
					lldebugs << "Stopping leftover animation from state " << state->mName << llendl;
					gAgent.sendAnimationRequest(animation,ANIM_REQUEST_STOP);
					gAgentAvatarp->LLCharacter::stopMotion(animation);
					state->mCurrentAnimationID.setNull();
				}
			}
			else
				lldebugs << "state "<< index <<" returned NULL." << llendl;
		}

		if(!foreignAnimations(LLUUID::null))
			gAgent.sendAnimationRequest(mLastMotion,ANIM_REQUEST_START);

		mCurrentSet->stopTimer();
	}
}

void AOEngine::setStateCycleTimer(const AOSet::AOState* state)
{
	F32 timeout=state->mCycleTime;
	lldebugs << "Setting cycle timeout for state " << state->mName << " of " << timeout << llendl;
	if(timeout>0.0f)
		mCurrentSet->startTimer(timeout);
}

const LLUUID AOEngine::override(const LLUUID& pMotion,BOOL start)
{
	LLUUID animation;

	LLUUID motion=pMotion;

	if(!mEnabled)
	{
		if(start && mCurrentSet)
		{
			const AOSet::AOState* state=mCurrentSet->getStateByRemapID(motion);
			if(state)
			{
				setLastMotion(motion);
				lldebugs << "(disabled AO) setting last motion id to " <<  gAnimLibrary.animationName(mLastMotion) << llendl;
				if(!state->mAnimations.empty())
				{
					setLastOverriddenMotion(motion);
					lldebugs << "(disabled AO) setting last overridden motion id to " <<  gAnimLibrary.animationName(mLastOverriddenMotion) << llendl;
				}
			}
		}
		return animation;
	}

	if(mSets.empty())
	{
		lldebugs << "No sets loaded. Skipping overrider." << llendl;
		return animation;
	}

	if(!mCurrentSet)
	{
		lldebugs << "No current AO set chosen. Skipping overrider." << llendl;
		return animation;
	}

	// we don't distinguish between these two
	if(motion==ANIM_AGENT_SIT_GROUND_CONSTRAINED)
		motion=ANIM_AGENT_SIT_GROUND;

	AOSet::AOState* state=mCurrentSet->getStateByRemapID(motion);
	if(!state)
	{
		lldebugs << "No current AO state for motion " << motion << " (" << gAnimLibrary.animationName(motion) << ")." << llendl;
//		This part of the code was added to capture an edge case where animations got stuck
//		However, it seems it isn't needed anymore and breaks other, more important cases.
//		So we disable this code for now, unless bad things happen and the stuck animations
//		come back again. -Zi
//		if(!gAnimLibrary.animStateToString(motion) && !start)
//		{
//			state=mCurrentSet->getStateByRemapID(mLastOverriddenMotion);
//			if(state && state->mCurrentAnimationID==motion)
//			{
//				lldebugs << "Stop requested for current overridden animation UUID " << motion << " - Skipping." << llendl;
//			}
//			else
//			{
//				lldebugs << "Stop requested for unknown UUID " << motion << " - Stopping it just in case." << llendl;
//				gAgent.sendAnimationRequest(motion,ANIM_REQUEST_STOP);
//				gAgentAvatarp->LLCharacter::stopMotion(motion);
//			}
//		}
		return animation;
	}

	mCurrentSet->stopTimer();
	if(start)
	{
		// Disable start stands in Mouselook
		if(mCurrentSet->getMouselookDisable() &&
			motion==ANIM_AGENT_STAND &&
			mInMouselook)
		{
			setLastMotion(motion);
			lldebugs << "(enabled AO, mouselook stand stopped) setting last motion id to " <<  gAnimLibrary.animationName(mLastMotion) << llendl;
			return animation;
		}

		// Do not start override sits if not selected
		if(!mCurrentSet->getSitOverride() && motion==ANIM_AGENT_SIT)
		{
			setLastMotion(motion);
			lldebugs << "(enabled AO, sit override stopped) setting last motion id to " <<  gAnimLibrary.animationName(mLastMotion) << llendl;
			return animation;
		}

		setLastMotion(motion);
		lldebugs << "(enabled AO) setting last motion id to " <<  gAnimLibrary.animationName(mLastMotion) << llendl;

		if(!state->mAnimations.empty())
		{
			setLastOverriddenMotion(motion);
			lldebugs << "(enabled AO) setting last overridden motion id to " <<  gAnimLibrary.animationName(mLastOverriddenMotion) << llendl;
		}

		// do not remember typing as set-wide motion
		if(motion!=ANIM_AGENT_TYPE)
			mCurrentSet->setMotion(motion);

		mUnderWater=gAgentAvatarp->mBelowWater;
		if(mUnderWater)
			animation=mapSwimming(motion);

		if(animation.isNull())
			animation=mCurrentSet->getAnimationForState(state);

		if(state->mCurrentAnimationID.notNull())
		{
			lldebugs	<< "Previous animation for state "
						<< gAnimLibrary.animationName(motion)
						<< " was not stopped, but we were asked to start a new one. Killing old animation." << llendl;
			gAgent.sendAnimationRequest(state->mCurrentAnimationID,ANIM_REQUEST_STOP);
			gAgentAvatarp->LLCharacter::stopMotion(state->mCurrentAnimationID);
		}

		state->mCurrentAnimationID=animation;
		lldebugs	<< "overriding " <<  gAnimLibrary.animationName(motion)
					<< " with " << animation
					<< " in state " << state->mName
					<< " of set " << mCurrentSet->getName()
					<< " (" << mCurrentSet << ")" << llendl;

		setStateCycleTimer(state);

		if(motion==ANIM_AGENT_SIT)
		{
			// Use ANIM_AGENT_SIT_GENERIC, so we don't create an overrider loop with ANIM_AGENT_SIT
			// while still having a base sitting pose to cover up cycle points
			gAgent.sendAnimationRequest(ANIM_AGENT_SIT_GENERIC,ANIM_REQUEST_START);
			if(mCurrentSet->getSmart())
				mSitCancelTimer.oneShot();
		}
		// special treatment for "transient animations" because the viewer needs the Linden animation to know the agent's state
		else if(motion==ANIM_AGENT_SIT_GROUND ||
				motion==ANIM_AGENT_PRE_JUMP ||
				motion==ANIM_AGENT_STANDUP ||
				motion==ANIM_AGENT_LAND ||
				motion==ANIM_AGENT_MEDIUM_LAND)
		{
			gAgent.sendAnimationRequest(animation,ANIM_REQUEST_START);
			return LLUUID::null;
		}
	}
	else
	{
		animation=state->mCurrentAnimationID;
		state->mCurrentAnimationID.setNull();
		
		// for typing animaiton, just return the stored animation, reset the state timer, and don't memorize anything else
		if(motion==ANIM_AGENT_TYPE)
		{
			AOSet::AOState* previousState=mCurrentSet->getStateByRemapID(mLastMotion);
			if(previousState)
				setStateCycleTimer(previousState);
			return animation;
		}

		if(motion!=mCurrentSet->getMotion())
		{
			llwarns << "trying to stop-override motion " <<  gAnimLibrary.animationName(motion)
					<< " but the current set has motion " <<  gAnimLibrary.animationName(mCurrentSet->getMotion()) << llendl;
			return animation;
		}

		mCurrentSet->setMotion(LLUUID::null);

		// again, special treatment for "transient" animations to make sure our own animation gets stopped properly
		if(	motion==ANIM_AGENT_SIT_GROUND ||
			motion==ANIM_AGENT_PRE_JUMP ||
			motion==ANIM_AGENT_STANDUP ||
			motion==ANIM_AGENT_LAND ||
			motion==ANIM_AGENT_MEDIUM_LAND)
		{
			gAgent.sendAnimationRequest(animation,ANIM_REQUEST_STOP);
			gAgentAvatarp->LLCharacter::stopMotion(animation);
			setStateCycleTimer(state);
			return LLUUID::null;
		}

		// stop the underlying Linden Lab motion, in case it's still running.
		// frequently happens with sits, so we keep it only for those currently.
		if(mLastMotion==ANIM_AGENT_SIT)
			stopAllSitVariants();

		lldebugs << "stopping cycle timer for motion " <<  gAnimLibrary.animationName(motion) <<
					" using animation " << animation <<
					" in state " << state->mName << llendl;
	}

	return animation;
}

void AOEngine::checkSitCancel()
{
	LLUUID seat;

	const LLViewerObject* agentRoot=dynamic_cast<LLViewerObject*>(gAgentAvatarp->getRoot());
	if(agentRoot)
		seat=agentRoot->getID();

	if(foreignAnimations(seat))
	{
		LLUUID animation=mCurrentSet->getStateByRemapID(ANIM_AGENT_SIT)->mCurrentAnimationID;
		if(animation.notNull())
		{
			lldebugs << "Stopping sit animation due to foreign animations running" << llendl;
			gAgent.sendAnimationRequest(animation,ANIM_REQUEST_STOP);
			// remove cycle point cover-up
			gAgent.sendAnimationRequest(ANIM_AGENT_SIT_GENERIC,ANIM_REQUEST_STOP);
			gAgentAvatarp->LLCharacter::stopMotion(animation);
			mSitCancelTimer.stop();
			// stop cycle tiemr
			mCurrentSet->stopTimer();
		}
	}
}

void AOEngine::cycleTimeout(const AOSet* set)
{
	if(!mEnabled)
		return;

	if(set!=mCurrentSet)
	{
		llwarns << "cycleTimeout for set " << set->getName() << " but current set is " << mCurrentSet->getName() << llendl;
		return;
	}

	cycle(CycleAny);
}

void AOEngine::cycle(eCycleMode cycleMode)
{
	if(!mCurrentSet)
	{
		lldebugs << "cycle without set." << llendl;
		return;
	}

	LLUUID motion=mCurrentSet->getMotion();

	// assume stand if no motion is registered, happens after login when the avatar hasn't moved at all yet
	if(motion.isNull())
		motion=ANIM_AGENT_STAND;
	// do not cycle if we're sitting and sit-override is off
	else if(motion==ANIM_AGENT_SIT && !mCurrentSet->getSitOverride())
		return;
	// do not cycle if we're standing and mouselook stand override is disabled while being in mouselook
	else if(motion==ANIM_AGENT_STAND && mCurrentSet->getMouselookDisable() && mInMouselook)
		return;

	AOSet::AOState* state=mCurrentSet->getStateByRemapID(motion);
	if(!state)
	{
		lldebugs << "cycle without state." << llendl;
		return;
	}

	if(!state->mAnimations.size())
	{
		lldebugs << "cycle without animations in state." << llendl;
		return;
	}

	// make sure we disable cycling only for timed cycle, so manual cycling still works, even with cycling switched off
	if(!state->mCycle && cycleMode==CycleAny)
	{
		lldebugs << "cycle timeout, but state is set to not cycling." << llendl;
		return;
	}

	LLUUID oldAnimation=state->mCurrentAnimationID;
	LLUUID animation;

	if(cycleMode==CycleAny)
	{
		animation=mCurrentSet->getAnimationForState(state);
	}
	else
	{
		if(cycleMode==CyclePrevious)
		{
			if(state->mCurrentAnimation==0)
				state->mCurrentAnimation=state->mAnimations.size()-1;
			else
				state->mCurrentAnimation--;
		}
		else if(cycleMode==CycleNext)
		{
			state->mCurrentAnimation++;
			if(state->mCurrentAnimation==state->mAnimations.size())
				state->mCurrentAnimation=0;
		}
		animation=state->mAnimations[state->mCurrentAnimation].mAssetUUID;
	}

	// don't do anything if the animation didn't change
	if(animation==oldAnimation)
		return;

	state->mCurrentAnimationID=animation;
	if(!animation.isNull())
	{
		lldebugs << "requesting animation start for motion " << gAnimLibrary.animationName(motion) << ": " << animation << llendl;
		gAgent.sendAnimationRequest(animation,ANIM_REQUEST_START);
	}
	else
		lldebugs << "overrider came back with NULL animation for motion " << gAnimLibrary.animationName(motion) << "." << llendl;

	if(!oldAnimation.isNull())
	{
		lldebugs << "Cycling state " << state->mName << " - stopping animation " << oldAnimation << llendl;
		gAgent.sendAnimationRequest(oldAnimation,ANIM_REQUEST_STOP);
		gAgentAvatarp->LLCharacter::stopMotion(oldAnimation);
	}
}

void AOEngine::updateSortOrder(AOSet::AOState* state)
{
	for(U32 index=0;index<state->mAnimations.size();index++)
	{
		U32 sortOrder=state->mAnimations[index].mSortOrder;

		if(sortOrder!=index)
		{
			std::ostringstream numStr("");
			numStr << index;

			lldebugs	<< "sort order is " << sortOrder << " but index is " << index
						<< ", setting sort order description: " << numStr.str() << llendl;

			state->mAnimations[index].mSortOrder=index;

			LLViewerInventoryItem* item=gInventory.getItem(state->mAnimations[index].mInventoryUUID);
			if(!item)
			{
				llwarns << "NULL inventory item found while trying to copy " << state->mAnimations[index].mInventoryUUID << llendl;
				continue;
			}
			LLPointer<LLViewerInventoryItem> newItem=
				new LLViewerInventoryItem(item);

			newItem->setDescription(numStr.str());
			newItem->setComplete(TRUE);
			newItem->updateServer(FALSE);

			gInventory.updateItem(newItem);
		}
	}
}

LLUUID AOEngine::addSet(const std::string& name,BOOL reload)
{
	if(mAOFolder.isNull())
	{
		llwarns << ROOT_AO_FOLDER << " folder not there yet. Requesting recreation." << llendl;
		tick();
		return LLUUID::null;
	}

	BOOL wasProtected=gSavedPerAccountSettings.getBOOL("ProtectAOFolders");
	gSavedPerAccountSettings.setBOOL("ProtectAOFolders",FALSE);
	lldebugs << "adding set folder " << name << llendl;
	LLUUID newUUID=gInventory.createNewCategory(mAOFolder,LLFolderType::FT_NONE,name);
	gSavedPerAccountSettings.setBOOL("ProtectAOFolders",wasProtected);

	if(reload)
		mTimerCollection.enableReloadTimer(TRUE);
	return newUUID;
}

BOOL AOEngine::createAnimationLink(const AOSet* set,AOSet::AOState* state,const LLInventoryItem* item)
{
	lldebugs << "Asset ID " << item->getAssetUUID() << " inventory id " << item->getUUID() << " category id " << state->mInventoryUUID << llendl;
	if(state->mInventoryUUID.isNull())
	{
		lldebugs << "no " << state->mName << " folder yet. Creating ..." << llendl;
		gInventory.createNewCategory(set->getInventoryUUID(),LLFolderType::FT_NONE,state->mName);

		lldebugs << "looking for folder to get UUID ..." << llendl;
		LLUUID newStateFolderUUID;

		LLInventoryModel::item_array_t* items;
		LLInventoryModel::cat_array_t* cats;
		gInventory.getDirectDescendentsOf(set->getInventoryUUID(),cats,items);

		for(S32 index=0;index<cats->count();index++)
		{
			if(cats->get(index)->getName().compare(state->mName)==0)
			{
				lldebugs << "UUID found!" << llendl;
				newStateFolderUUID=cats->get(index)->getUUID();
				state->mInventoryUUID=newStateFolderUUID;
				break;
			}
		}
	}

	if(state->mInventoryUUID.isNull())
	{
		lldebugs << "state inventory UUID not found, failing." << llendl;
		return FALSE;
	}

	link_inventory_item(
		gAgent.getID(),
		item->getUUID(),
		state->mInventoryUUID,
		item->getName(),
		item->getDescription(),
		LLAssetType::AT_LINK,
		NULL);

	return TRUE;
}

BOOL AOEngine::addAnimation(const AOSet* set,AOSet::AOState* state,const LLInventoryItem* item,BOOL reload)
{
	AOSet::AOAnimation anim;
	anim.mAssetUUID=item->getAssetUUID();
	anim.mInventoryUUID=item->getUUID();
	anim.mName=item->getName();
	anim.mSortOrder=state->mAnimations.size()+1;
	state->mAnimations.push_back(anim);

	BOOL wasProtected=gSavedPerAccountSettings.getBOOL("ProtectAOFolders");
	gSavedPerAccountSettings.setBOOL("ProtectAOFolders",FALSE);
	createAnimationLink(set,state,item);
	gSavedPerAccountSettings.setBOOL("ProtectAOFolders",wasProtected);

	if(reload)
		mTimerCollection.enableReloadTimer(TRUE);
	return TRUE;
}

BOOL AOEngine::findForeignItems(const LLUUID& uuid) const
{
	BOOL moved=FALSE;

	LLInventoryModel::item_array_t* items;
	LLInventoryModel::cat_array_t* cats;

	gInventory.getDirectDescendentsOf(uuid,cats,items);
	for(S32 index=0;index<cats->count();index++)
	{
		// recurse into subfolders
		if(findForeignItems(cats->get(index)->getUUID()))
		{
			moved=TRUE;
		}
	}

	// count backwards in case we have to remove items
	BOOL wasProtected=gSavedPerAccountSettings.getBOOL("ProtectAOFolders");
	gSavedPerAccountSettings.setBOOL("ProtectAOFolders",FALSE);
	for(S32 index=items->count()-1;index>=0;index--)
	{
		BOOL move=FALSE;

		LLPointer<LLViewerInventoryItem> item=items->get(index);
		if(item->getIsLinkType())
		{
			if(item->getInventoryType()!=LLInventoryType::IT_ANIMATION)
			{
				lldebugs << item->getName() << " is a link but does not point to an animation." << llendl;
				move=TRUE;
			}
			else
			{
				lldebugs << item->getName() << " is an animation link." << llendl;
			}
		}
		else
		{
			lldebugs << item->getName() << " is not a link!" << llendl;
			move=TRUE;
		}

		if(move)
		{
			moved=TRUE;
			LLInventoryModel* model = &gInventory;
			model->changeItemParent(item,gInventory.findCategoryUUIDForType(LLFolderType::FT_LOST_AND_FOUND),FALSE);
			lldebugs << item->getName() << " moved to lost and found!" << llendl;
		}
	}
	gSavedPerAccountSettings.setBOOL("ProtectAOFolders",wasProtected);

	return moved;
}

// needs a three-step process, since purge of categories only seems to work from trash
void AOEngine::purgeFolder(const LLUUID& uuid) const
{
	// unprotect it
	BOOL wasProtected=gSavedPerAccountSettings.getBOOL("ProtectAOFolders");
	gSavedPerAccountSettings.setBOOL("ProtectAOFolders",FALSE);

	// move everything that's not an animation link to "lost and found"
	if(findForeignItems(uuid))
	{
		LLNotificationsUtil::add("AOForeignItemsFound", LLSD());
	}

	// trash it
	gInventory.removeCategory(uuid);

	// clean it
	gInventory.purgeDescendentsOf(uuid);
	gInventory.notifyObservers();

	// purge it
	gInventory.purgeObject(uuid);
	gInventory.notifyObservers();

	// protect it
	gSavedPerAccountSettings.setBOOL("ProtectAOFolders",wasProtected);
}

BOOL AOEngine::removeSet(AOSet* set)
{
	purgeFolder(set->getInventoryUUID());

	mTimerCollection.enableReloadTimer(TRUE);
	return TRUE;
}

BOOL AOEngine::removeAnimation(const AOSet* set,AOSet::AOState* state,S32 index)
{
	S32 numOfAnimations=state->mAnimations.size();
	if(numOfAnimations==0)
		return FALSE;

	LLViewerInventoryItem* item=gInventory.getItem(state->mAnimations[index].mInventoryUUID);

	// check if this item is actually an animation link
	BOOL move=TRUE;
	if(item->getIsLinkType())
	{
		if(item->getInventoryType()==LLInventoryType::IT_ANIMATION)
		{
			// it is an animation link, so mark it to be purged
			move=FALSE;
		}
	}

	// this item was not an animation link, move it to lost and found
	if(move)
	{
		LLInventoryModel* model = &gInventory;
		model->changeItemParent(item,gInventory.findCategoryUUIDForType(LLFolderType::FT_LOST_AND_FOUND),FALSE);
		LLNotificationsUtil::add("AOForeignItemsFound", LLSD());
		update();
		return FALSE;
	}

	// purge the item from inventory
	lldebugs << __LINE__ << " purging: " << state->mAnimations[index].mInventoryUUID << llendl;
	gInventory.purgeObject(state->mAnimations[index].mInventoryUUID); // item->getUUID());
	gInventory.notifyObservers();

	state->mAnimations.erase(state->mAnimations.begin()+index);

	if(state->mAnimations.size()==0)
	{
		lldebugs << "purging folder " << state->mName << " from inventory because it's empty." << llendl;

		LLInventoryModel::item_array_t* items;
		LLInventoryModel::cat_array_t* cats;
		gInventory.getDirectDescendentsOf(set->getInventoryUUID(),cats,items);

		for(S32 index=0;index<cats->count();index++)
		{
			std::vector<std::string> params;
			LLStringUtil::getTokens(cats->get(index)->getName(),params,":");
			std::string stateName=params[0];

			if(state->mName.compare(stateName)==0)
			{
				lldebugs << "folder found: " << cats->get(index)->getName() << " purging uuid " << cats->get(index)->getUUID() << llendl;

				purgeFolder(cats->get(index)->getUUID());
				state->mInventoryUUID.setNull();
				break;
			}
		}
	}
	else
		updateSortOrder(state);

	return TRUE;
}

BOOL AOEngine::swapWithPrevious(AOSet::AOState* state,S32 index)
{
	S32 numOfAnimations=state->mAnimations.size();
	if(numOfAnimations<2 || index==0)
		return FALSE;

	AOSet::AOAnimation tmpAnim=state->mAnimations[index];
	state->mAnimations.erase(state->mAnimations.begin()+index);
	state->mAnimations.insert(state->mAnimations.begin()+index-1,tmpAnim);

	updateSortOrder(state);

	return TRUE;
}

BOOL AOEngine::swapWithNext(AOSet::AOState* state,S32 index)
{
	S32 numOfAnimations=state->mAnimations.size();
	if(numOfAnimations<2 || index==(numOfAnimations-1))
		return FALSE;

	AOSet::AOAnimation tmpAnim=state->mAnimations[index];
	state->mAnimations.erase(state->mAnimations.begin()+index);
	state->mAnimations.insert(state->mAnimations.begin()+index+1,tmpAnim);

	updateSortOrder(state);

	return TRUE;
}

void AOEngine::reloadStateAnimations(AOSet::AOState* state)
{
	LLInventoryModel::item_array_t* items;
	LLInventoryModel::cat_array_t* dummy;

	state->mAnimations.clear();

	gInventory.getDirectDescendentsOf(state->mInventoryUUID,dummy,items);
	for(S32 num=0;num<items->count();num++)
	{
		lldebugs	<< "Found animation link " << items->get(num)->LLInventoryItem::getName()
					<< " desc " << items->get(num)->LLInventoryItem::getDescription()
					<< " asset " << items->get(num)->getAssetUUID() << llendl;

		AOSet::AOAnimation anim;
		anim.mAssetUUID=items->get(num)->getAssetUUID();
		LLViewerInventoryItem* linkedItem=items->get(num)->getLinkedItem();
		if(linkedItem==0)
		{
			llwarns << "linked item for link " << items->get(num)->LLInventoryItem::getName() << " not found (broken link). Skipping." << llendl;
			continue;
		}
		anim.mName=linkedItem->LLInventoryItem::getName();
		anim.mInventoryUUID=items->get(num)->getUUID();

		S32 sortOrder;
		if(!LLStringUtil::convertToS32(items->get(num)->LLInventoryItem::getDescription(),sortOrder))
			sortOrder=-1;
		anim.mSortOrder=sortOrder;

		lldebugs << "current sort order is " << sortOrder << llendl;

		if(sortOrder==-1)
		{
			llwarns << "sort order was unknown so append to the end of the list" << llendl;
			state->mAnimations.push_back(anim);
		}
		else
		{
			BOOL inserted=FALSE;
			for(U32 index=0;index<state->mAnimations.size();index++)
			{
				if(state->mAnimations[index].mSortOrder>sortOrder)
				{
					lldebugs << "inserting at index " << index << llendl;
					state->mAnimations.insert(state->mAnimations.begin()+index,anim);
					inserted=TRUE;
					break;
				}
			}
			if(!inserted)
			{
				lldebugs << "not inserted yet, appending to the list instead" << llendl;
				state->mAnimations.push_back(anim);
			}
		}
		lldebugs << "Animation count now: " << state->mAnimations.size() << llendl;
	}

	updateSortOrder(state);
}

void AOEngine::update()
{
	if(mAOFolder.isNull())
		return;

	// move everything that's not an animation link to "lost and found"
	if(findForeignItems(mAOFolder))
	{
		LLNotificationsUtil::add("AOForeignItemsFound", LLSD());
	}

	LLInventoryModel::cat_array_t* categories;
	LLInventoryModel::item_array_t* items;

	BOOL allComplete=TRUE;
	mTimerCollection.enableSettingsTimer(FALSE);

	gInventory.getDirectDescendentsOf(mAOFolder,categories,items);
	for(S32 index=0;index<categories->count();index++)
	{
		LLViewerInventoryCategory* currentCategory=categories->get(index);
		const std::string& setFolderName=currentCategory->getName();
		std::vector<std::string> params;
		LLStringUtil::getTokens(setFolderName,params,":");

		AOSet* newSet=getSetByName(params[0]);
		if(newSet==0)
		{
			lldebugs << "Adding set " << setFolderName << " to AO." << llendl;
			newSet=new AOSet(currentCategory->getUUID());
			newSet->setName(params[0]);
			mSets.push_back(newSet);
		}
		else
		{
			if(newSet->getComplete())
			{
				lldebugs << "Set " << params[0] << " already complete. Skipping." << llendl;
				continue;
			}
			lldebugs << "Updating set " << setFolderName << " in AO." << llendl;
		}
		allComplete=FALSE;

		for(U32 num=1;num<params.size();num++)
		{
			if(params[num].size()!=2)
				llwarns << "Unknown AO set option " << params[num] << llendl;
			else if(params[num]=="SO")
				newSet->setSitOverride(TRUE);
			else if(params[num]=="SM")
				newSet->setSmart(TRUE);
			else if(params[num]=="DM")
				newSet->setMouselookDisable(TRUE);
			else if(params[num]=="**")
			{
				mDefaultSet=newSet;
				mCurrentSet=newSet;
			}
			else
				llwarns << "Unknown AO set option " << params[num] << llendl;
		}

		if(gInventory.isCategoryComplete(currentCategory->getUUID()))
		{
			lldebugs << "Set " << params[0] << " is complete, reading states ..." << llendl;

			LLInventoryModel::cat_array_t* stateCategories;
			gInventory.getDirectDescendentsOf(currentCategory->getUUID(),stateCategories,items);
			newSet->setComplete(TRUE);

			for(S32 index=0;index<stateCategories->count();index++)
			{
				std::vector<std::string> params;
				LLStringUtil::getTokens(stateCategories->get(index)->getName(),params,":");
				std::string stateName=params[0];

				AOSet::AOState* state=newSet->getStateByName(stateName);
				if(state==NULL)
				{
					llwarns << "Unknown state " << stateName << ". Skipping." << llendl;
					continue;
				}
				lldebugs << "Reading state " << stateName << llendl;

				state->mInventoryUUID=stateCategories->get(index)->getUUID();
				for(U32 num=1;num<params.size();num++)
				{
					if(params[num]=="CY")
					{
						state->mCycle=TRUE;
						lldebugs << "Cycle on" << llendl;
					}
					else if(params[num]=="RN")
					{
						state->mRandom=TRUE;
						lldebugs << "Random on" << llendl;
					}
					else if(params[num].substr(0,2)=="CT")
					{
						LLStringUtil::convertToS32(params[num].substr(2,params[num].size()-2),state->mCycleTime);
						lldebugs << "Cycle Time specified:" << state->mCycleTime << llendl;
					}
					else
						llwarns << "Unknown AO set option " << params[num] << llendl;
				}

				if(!gInventory.isCategoryComplete(state->mInventoryUUID))
				{
					lldebugs << "State category " << stateName << " is incomplete, fetching descendents" << llendl;
					gInventory.fetchDescendentsOf(state->mInventoryUUID);
					allComplete=FALSE;
					newSet->setComplete(FALSE);
					continue;
				}
				reloadStateAnimations(state);
			}
		}
		else
		{
			lldebugs << "Set " << params[0] << " is incomplete, fetching descendents" << llendl;
			gInventory.fetchDescendentsOf(currentCategory->getUUID());
		}
	}

	if(allComplete)
	{
		mEnabled=gSavedPerAccountSettings.getBOOL("UseAO");

		if(!mCurrentSet && !mSets.empty())
		{
			lldebugs << "No default set defined, choosing the first in the list." << llendl;
			selectSet(mSets[0]);
		}

		mTimerCollection.enableInventoryTimer(FALSE);
		mTimerCollection.enableSettingsTimer(TRUE);

		llwarns << "sending update signal" << llendl;
		mUpdatedSignal();
		enable(mEnabled);
	}
}

void AOEngine::reload( bool aFromTimer )
{
	BOOL wasEnabled=mEnabled;

	mTimerCollection.enableReloadTimer(FALSE);

	if(wasEnabled)
		enable(FALSE);

	gAgent.stopCurrentAnimations();
	mLastOverriddenMotion=ANIM_AGENT_STAND;

	clear( aFromTimer );
	mAOFolder.setNull();
	mTimerCollection.enableInventoryTimer(TRUE);
	tick();

	if(wasEnabled)
		enable(TRUE);
}

AOSet* AOEngine::getSetByName(const std::string& name) const
{
	AOSet* found=0;
	for(U32 index=0;index<mSets.size();index++)
	{
		if(mSets[index]->getName().compare(name)==0)
		{
			found=mSets[index];
			break;
		}
	}
	return found;
}

const std::string AOEngine::getCurrentSetName() const
{
	if(mCurrentSet)
		return mCurrentSet->getName();
	return std::string();
}

const AOSet* AOEngine::getDefaultSet() const
{
	return mDefaultSet;
}

void AOEngine::selectSet(AOSet* set)
{
	if(mEnabled && mCurrentSet)
	{
		AOSet::AOState* state=mCurrentSet->getStateByRemapID(mLastOverriddenMotion);
		if(state)
		{
			gAgent.sendAnimationRequest(state->mCurrentAnimationID,ANIM_REQUEST_STOP);
			state->mCurrentAnimationID.setNull();
			mCurrentSet->stopTimer();
		}
	}

	mCurrentSet=set;

	if(mEnabled)
	{
		lldebugs << "enabling with motion " << gAnimLibrary.animationName(mLastMotion) << llendl;
		gAgent.sendAnimationRequest(override(mLastMotion,TRUE),ANIM_REQUEST_START);
	}
}

AOSet* AOEngine::selectSetByName(const std::string& name)
{
	AOSet* set=getSetByName(name);
	if(set)
	{
		selectSet(set);
		return set;
	}
	llwarns << "Could not find AO set " << name << llendl;
	return NULL;
}

const std::vector<AOSet*> AOEngine::getSetList() const
{
	return mSets;
}

void AOEngine::saveSet(const AOSet* set)
{
	if(!set)
		return;

	std::string setParams=set->getName();
	if(set->getSitOverride())
		setParams+=":SO";
	if(set->getSmart())
		setParams+=":SM";
	if(set->getMouselookDisable())
		setParams+=":DM";
	if(set==mDefaultSet)
		setParams+=":**";

/*
	// This works fine, but LL seems to have added a few helper functions in llinventoryfunctions.h
	// so let's make use of them. This code is just for reference

	LLViewerInventoryCategory* cat=gInventory.getCategory(set->getInventoryUUID());
	llwarns << cat << llendl;
	cat->rename(setParams);
	cat->updateServer(FALSE);
	gInventory.addChangedMask(LLInventoryObserver::LABEL, cat->getUUID());
	gInventory.notifyObservers();
*/
	BOOL wasProtected=gSavedPerAccountSettings.getBOOL("ProtectAOFolders");
	gSavedPerAccountSettings.setBOOL("ProtectAOFolders",FALSE);
	rename_category(&gInventory,set->getInventoryUUID(),setParams);
	gSavedPerAccountSettings.setBOOL("ProtectAOFolders",wasProtected);

	llwarns << "sending update signal" << llendl;
	mUpdatedSignal();
}

BOOL AOEngine::renameSet(AOSet* set,const std::string& name)
{
	if(name.empty() || name.find(":")!=std::string::npos)
		return FALSE;
	set->setName(name);
	set->setDirty(TRUE);

	return TRUE;
}

void AOEngine::saveState(const AOSet::AOState* state)
{
	std::string stateParams=state->mName;
	F32 time=state->mCycleTime;
	if(time>0.0)
	{
		std::ostringstream timeStr;
		timeStr << ":CT" << state->mCycleTime;
		stateParams+=timeStr.str();
	}
	if(state->mCycle)
		stateParams+=":CY";
	if(state->mRandom)
		stateParams+=":RN";

	BOOL wasProtected=gSavedPerAccountSettings.getBOOL("ProtectAOFolders");
	gSavedPerAccountSettings.setBOOL("ProtectAOFolders",FALSE);
	rename_category(&gInventory,state->mInventoryUUID,stateParams);
	gSavedPerAccountSettings.setBOOL("ProtectAOFolders",wasProtected);
}

void AOEngine::saveSettings()
{
	for(U32 index=0;index<mSets.size();index++)
	{
		AOSet* set=mSets[index];
		if(set->getDirty())
		{
			saveSet(set);
			llwarns << "dirty set saved " << set->getName() << llendl;
			set->setDirty(FALSE);
		}

		for(S32 stateIndex=0;stateIndex<AOSet::AOSTATES_MAX;stateIndex++)
		{
			AOSet::AOState* state=set->getState(stateIndex);
			if(state->mDirty)
			{
				saveState(state);
				llwarns << "dirty state saved " << state->mName << llendl;
				state->mDirty=FALSE;
			}
		}
	}
}

void AOEngine::inMouselook(BOOL yes)
{
	if(mInMouselook==yes)
		return;

	mInMouselook=yes;

	if(!mCurrentSet)
		return;

	if(!mCurrentSet->getMouselookDisable())
		return;

	if(!mEnabled)
		return;

	if(mLastMotion!=ANIM_AGENT_STAND)
		return;

	if(yes)
	{
		AOSet::AOState* state=mCurrentSet->getState(AOSet::Standing);
		if(!state)
			return;

		LLUUID animation=state->mCurrentAnimationID;
		if(!animation.isNull())
		{
			gAgent.sendAnimationRequest(animation,ANIM_REQUEST_STOP);
			gAgentAvatarp->LLCharacter::stopMotion(animation);
			state->mCurrentAnimationID.setNull();
			lldebugs << " stopped animation " << animation << " in state " << state->mName << llendl;
		}
		gAgent.sendAnimationRequest(ANIM_AGENT_STAND,ANIM_REQUEST_START);
	}
	else
	{
		stopAllStandVariants();
		gAgent.sendAnimationRequest(override(ANIM_AGENT_STAND,TRUE),ANIM_REQUEST_START);
	}
}

void AOEngine::setDefaultSet(AOSet* set)
{
	mDefaultSet=set;
	for(U32 index=0;index<mSets.size();index++)
		mSets[index]->setDirty(TRUE);
}

void AOEngine::setOverrideSits(AOSet* set,BOOL yes)
{
	set->setSitOverride(yes);
	set->setDirty(TRUE);

	if(mCurrentSet!=set)
		return;

	if(mLastMotion!=ANIM_AGENT_SIT)
		return;

	if(yes)
	{
		stopAllSitVariants();
		gAgent.sendAnimationRequest(override(ANIM_AGENT_SIT,TRUE),ANIM_REQUEST_START);
	}
	else
	{
		AOSet::AOState* state=mCurrentSet->getState(AOSet::Sitting);
		if(!state)
			return;

		LLUUID animation=state->mCurrentAnimationID;
		if(!animation.isNull())
		{
			gAgent.sendAnimationRequest(animation,ANIM_REQUEST_STOP);
			gAgentAvatarp->LLCharacter::stopMotion(animation);
			state->mCurrentAnimationID.setNull();
		}

		gAgent.sendAnimationRequest(ANIM_AGENT_SIT,ANIM_REQUEST_START);
	}
}

void AOEngine::setSmart(AOSet* set,BOOL yes)
{
	set->setSmart(yes);
	set->setDirty(TRUE);
}

void AOEngine::setDisableStands(AOSet* set,BOOL yes)
{
	set->setMouselookDisable(yes);
	set->setDirty(TRUE);

	if(mCurrentSet!=set)
		return;

	// make sure an update happens if needed
	mInMouselook=!gAgentCamera.cameraMouselook();
	inMouselook(!mInMouselook);
}

void AOEngine::setCycle(AOSet::AOState* state,BOOL yes)
{
	state->mCycle=yes;
	state->mDirty=TRUE;
}

void AOEngine::setRandomize(AOSet::AOState* state,BOOL yes)
{
	state->mRandom=yes;
	state->mDirty=TRUE;
}

void AOEngine::setCycleTime(AOSet::AOState* state,F32 time)
{
	state->mCycleTime=time;
	state->mDirty=TRUE;
}

void AOEngine::tick()
{
	// <FS:ND> make sure agent is alive and kicking before doing anything
	if ( !isAgentAvatarValid() )
		return;
	// </FS:ND>

	const LLUUID categoryID=gInventory.findCategoryByName(ROOT_FIRESTORM_FOLDER);

	if(categoryID.isNull())
	{
		llwarns << "no " << ROOT_FIRESTORM_FOLDER << " folder yet. Creating ..." << llendl;
		gInventory.createNewCategory(gInventory.getRootFolderID(),LLFolderType::FT_NONE,ROOT_FIRESTORM_FOLDER);
		mAOFolder.setNull();
	}
	else
	{
		LLInventoryModel::cat_array_t* categories;
		LLInventoryModel::item_array_t* items;
		gInventory.getDirectDescendentsOf(categoryID,categories,items);
		lldebugs << "cat " << categories->count() << " items " << items->count() << llendl;

		for(S32 index=0;index<categories->count();index++)
		{
			const std::string& catName=categories->get(index)->getName();
			if(catName.compare(ROOT_AO_FOLDER)==0)
			{
				mAOFolder=categories->get(index)->getUUID();
				break;
			}
		}

		if(mAOFolder.isNull())
		{
			llwarns << "no " << ROOT_AO_FOLDER << " folder yet. Creating ..." << llendl;
			gInventory.createNewCategory(categoryID,LLFolderType::FT_NONE,ROOT_AO_FOLDER);
		}
		else
		{
			llwarns << "AO basic folder structure intact." << llendl;
			update();
		}
	}
}

BOOL AOEngine::importNotecard(const LLInventoryItem* item)
{
	if(item)
	{
		llwarns << "importing AO notecard: " << item->getName() << llendl;
		if(getSetByName(item->getName()))
		{
			LLNotificationsUtil::add("AOImportSetAlreadyExists", LLSD());
			return FALSE;
		}

		if(!gAgent.allowOperation(PERM_COPY,item->getPermissions(),GP_OBJECT_MANIPULATE) && !gAgent.isGodlike())
		{
			LLNotificationsUtil::add("AOImportPermissionDenied", LLSD());
			return FALSE;
		}

		if(item->getAssetUUID().notNull())
		{
			mImportSet=new AOSet(item->getParentUUID());
			if(!mImportSet)
			{
				LLNotificationsUtil::add("AOImportCreateSetFailed", LLSD());
				return FALSE;
			}
			mImportSet->setName(item->getName());

			LLUUID* newUUID=new LLUUID(item->getAssetUUID());
			const LLHost sourceSim=LLHost::invalid;

			gAssetStorage->getInvItemAsset
			(
				sourceSim,
				gAgent.getID(),
				gAgent.getSessionID(),
				item->getPermissions().getOwner(),
				LLUUID::null,
				item->getUUID(),
				item->getAssetUUID(),
				item->getType(),
				&onNotecardLoadComplete,
				(void*) newUUID,
				TRUE
			);

			return TRUE;
		}
	}
	return FALSE;
}

// static
void AOEngine::onNotecardLoadComplete(	LLVFS* vfs,const LLUUID& assetUUID,LLAssetType::EType type,
											void* userdata,S32 status,LLExtStat extStatus)
{
	if(status!=LL_ERR_NOERR)
	{
		// AOImportDownloadFailed
		LLNotificationsUtil::add("AOImportDownloadFailed", LLSD());
		// NULL tells the importer to cancel all operations and free the import set memory
		AOEngine::instance().parseNotecard(NULL);
		return;
	}
	lldebugs << "Downloading import notecard complete." << llendl;

	S32 notecardSize=vfs->getSize(assetUUID,type);
	char* buffer=new char[notecardSize];
	vfs->getData(assetUUID,type,(U8*) buffer,0,notecardSize);

	AOEngine::instance().parseNotecard(buffer);
}

void AOEngine::parseNotecard(const char* buffer)
{
	lldebugs << "parsing import notecard" << llendl;

	BOOL isValid=FALSE;

	if(!buffer)
	{
		llwarns << "buffer==NULL - aborting import" << llendl;
		// NOTE: cleanup is always the same, needs streamlining
		delete mImportSet;
		mImportSet=0;
		mUpdatedSignal();
		return;
	}

	std::string text(buffer);
	delete buffer;

	std::vector<std::string> lines;
	LLStringUtil::getTokens(text,lines,"\n");

	S32 found=-1;
	for(U32 index=0;index<lines.size();index++)
	{
		if(lines[index].find("Text length ")==0)
		{
			found=index;
			break;
		}
	}

	if(found==-1)
	{
		LLNotificationsUtil::add("AOImportNoText", LLSD());
		delete mImportSet;
		mImportSet=0;
		mUpdatedSignal();
		return;
	}

	LLViewerInventoryCategory* importCategory=gInventory.getCategory(mImportSet->getInventoryUUID());
	if(!importCategory)
	{
		LLNotificationsUtil::add("AOImportNoFolder", LLSD());
		delete mImportSet;
		mImportSet=0;
		mUpdatedSignal();
		return;
	}

	std::map<std::string,LLUUID> animationMap;
	LLInventoryModel::cat_array_t* dummy;
	LLInventoryModel::item_array_t* items;

	gInventory.getDirectDescendentsOf(mImportSet->getInventoryUUID(),dummy,items);
	for(U32 index=0;index<items->size();index++)
	{
		animationMap[items->get(index)->getName()]=items->get(index)->getUUID();
		lldebugs	<<	"animation " << items->get(index)->getName() <<
						" has inventory UUID " << animationMap[items->get(index)->getName()] << llendl;
	}

	// [ State ]Anim1|Anim2|Anim3
	for(U32 index=found+1;index<lines.size();index++)
	{
		std::string line=lines[index];

		// cut off the trailing } of a notecard asset's text portion in the last line
		if(index==lines.size()-1)
			line=line.substr(0,line.size()-1);

		LLStringUtil::trim(line);

		if(line.empty())
			continue;

		if(line[0]=='#') // <ND/> FIRE-3801; skip comments to reduce spam to local chat.
			continue;

		if(line.find("[")!=0)
		{
			LLSD args;
			args["LINE"]=(S32) index;
			LLNotificationsUtil::add("AOImportNoStatePrefix",args);
			continue;
		}

		if(line.find("]") == std::string::npos)
		{
			LLSD args;
			args["LINE"]=(S32) index;
			LLNotificationsUtil::add("AOImportNoValidDelimiter",args);
			continue;
		}
		U32 endTag = line.find("]");

		std::string stateName=line.substr(1,endTag-1);
		LLStringUtil::trim(stateName);

		AOSet::AOState* newState=mImportSet->getStateByName(stateName);
		if(!newState)
		{
			LLSD args;
			args["NAME"]=stateName;
			LLNotificationsUtil::add("AOImportStateNameNotFound",args);
			continue;
		}

		std::string animationLine=line.substr(endTag+1);
		std::vector<std::string> animationList;
		LLStringUtil::getTokens(animationLine,animationList,"|,");

		for(U32 animIndex=0;animIndex<animationList.size();animIndex++)
		{
			AOSet::AOAnimation animation;
			animation.mName=animationList[animIndex];
			animation.mInventoryUUID=animationMap[animation.mName];
			if(animation.mInventoryUUID.isNull())
			{
				LLSD args;
				args["NAME"]=animation.mName;
				LLNotificationsUtil::add("AOImportAnimationNotFound",args);
				continue;
			}
			animation.mSortOrder=animIndex;
			newState->mAnimations.push_back(animation);
			isValid=TRUE;
		}
	}

	if(!isValid)
	{
		LLNotificationsUtil::add("AOImportInvalid",LLSD());
		// NOTE: cleanup is always the same, needs streamlining
		delete mImportSet;
		mImportSet=0;
		mUpdatedSignal();
		return;
	}

	mTimerCollection.enableImportTimer(TRUE);
	mImportRetryCount=0;
	processImport(false);
}

void AOEngine::processImport( bool aFromTimer )
{
	if(mImportCategory.isNull())
	{
		mImportCategory=addSet(mImportSet->getName(),FALSE);
		if(mImportCategory.isNull())
		{
			mImportRetryCount++;
			if(mImportRetryCount==5)
			{
				// NOTE: cleanup is the same as at the end of this function. Needs streamlining.
				mTimerCollection.enableImportTimer(FALSE);
				delete mImportSet;
				mImportSet=0;
				mImportCategory.setNull();
				mUpdatedSignal();
				LLSD args;
				args["NAME"]=mImportSet->getName();
				LLNotificationsUtil::add("AOImportAbortCreateSet",args);
			}
			else
			{
				LLSD args;
				args["NAME"]=mImportSet->getName();
				LLNotificationsUtil::add("AOImportRetryCreateSet",args);
			}
			return;
		}
		mImportSet->setInventoryUUID(mImportCategory);
	}

	BOOL allComplete=TRUE;
	for(S32 index=0;index<AOSet::AOSTATES_MAX;index++)
	{
		AOSet::AOState* state=mImportSet->getState(index);
		if(state->mAnimations.size())
		{
			allComplete=FALSE;
			lldebugs << "state " << state->mName << " still has animations to link." << llendl;

			for(S32 animationIndex=state->mAnimations.size()-1;animationIndex>=0;animationIndex--)
			{
				lldebugs << "linking animation " << state->mAnimations[animationIndex].mName << llendl;
				if(createAnimationLink(mImportSet,state,gInventory.getItem(state->mAnimations[animationIndex].mInventoryUUID)))
				{
					lldebugs	<< "link success, size "<< state->mAnimations.size() << ", removing animation "
								<< (*(state->mAnimations.begin()+animationIndex)).mName << " from import state" << llendl;
					state->mAnimations.erase(state->mAnimations.begin()+animationIndex);
					lldebugs << "deleted, size now: " << state->mAnimations.size() << llendl;
				}
				else
				{
					LLSD args;
					args["NAME"]=state->mAnimations[animationIndex].mName;
					LLNotificationsUtil::add("AOImportLinkFailed",args);
				}
			}
		}
	}

	if(allComplete)
	{
		mTimerCollection.enableImportTimer(FALSE);
		mOldImportSets.push_back( mImportSet ); //<ND/> FIRE-3801; Cannot delete here, or LLInstanceTracker gets upset. Just remember and delete mOldImportSets once we can. 
		mImportSet=0;
		mImportCategory.setNull();
		reload( aFromTimer );
	}
}

const LLUUID& AOEngine::getAOFolder() const
{
	return mAOFolder;
}

// ----------------------------------------------------

AOSitCancelTimer::AOSitCancelTimer()
:	LLEventTimer(0.1f),
	mTickCount(0)
{
	mEventTimer.stop();
}

AOSitCancelTimer::~AOSitCancelTimer()
{
}

void AOSitCancelTimer::oneShot()
{
	mTickCount=0;
	mEventTimer.start();
}

void AOSitCancelTimer::stop()
{
	mEventTimer.stop();
}

BOOL AOSitCancelTimer::tick()
{
	mTickCount++;
	AOEngine::instance().checkSitCancel();
	if(mTickCount==10)
		mEventTimer.stop();
	return FALSE;
}

// ----------------------------------------------------

AOTimerCollection::AOTimerCollection()
:	 LLEventTimer(INVENTORY_POLLING_INTERVAL),
	mInventoryTimer(TRUE),
	mSettingsTimer(FALSE),
	mReloadTimer(FALSE),
	mImportTimer(FALSE)
{
	updateTimers();
}

AOTimerCollection::~AOTimerCollection()
{
}

BOOL AOTimerCollection::tick()
{
	if(mInventoryTimer)
	{
		lldebugs << "Inventory timer tick()" << llendl;
		AOEngine::instance().tick();
	}
	if(mSettingsTimer)
	{
		lldebugs << "Settings timer tick()" << llendl;
		AOEngine::instance().saveSettings();
	}
	if(mReloadTimer)
	{
		lldebugs << "Reload timer tick()" << llendl;
		AOEngine::instance().reload(true);
	}
	if(mImportTimer)
	{
		lldebugs << "Import timer tick()" << llendl;
		AOEngine::instance().processImport(true);
	}

// always return FALSE or the LLEventTimer will be deleted -> crash
	return FALSE;
}

void AOTimerCollection::enableInventoryTimer(BOOL yes)
{
	mInventoryTimer=yes;
	updateTimers();
}

void AOTimerCollection::enableSettingsTimer(BOOL yes)
{
	mSettingsTimer=yes;
	updateTimers();
}

void AOTimerCollection::enableReloadTimer(BOOL yes)
{
	mReloadTimer=yes;
	updateTimers();
}

void AOTimerCollection::enableImportTimer(BOOL yes)
{
	mImportTimer=yes;
	updateTimers();
}

void AOTimerCollection::updateTimers()
{
	if(!mInventoryTimer && !mSettingsTimer && !mReloadTimer && !mImportTimer)
	{
		lldebugs << "no timer needed, stopping internal timer." << llendl;
		mEventTimer.stop();
	}
	else
	{
		lldebugs << "timer needed, starting internal timer." << llendl;
		mEventTimer.start();
	}
}
