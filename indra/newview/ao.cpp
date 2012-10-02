/** 
 * @file ao.cpp
 * @brief Anything concerning the Viewer Side Animation Overrider GUI
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
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
 * 
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "ao.h"
#include "aoengine.h"
#include "aoset.h"
#include "llcheckboxctrl.h"
#include "llcombobox.h"
#include "llnotificationsutil.h"
#include "llspinctrl.h"
#include "llviewercontrol.h"
#include "llviewerinventory.h"
#include "utilitybar.h"
#include <boost/graph/graph_concepts.hpp>

FloaterAO::FloaterAO(const LLSD& key)
:	LLTransientDockableFloater(NULL,true,key),LLEventTimer(10.0),
	mSetList(0),
	mSelectedSet(0),
	mSelectedState(0),
	mCanDragAndDrop(FALSE),
	mImportRunning(FALSE),
	mMore(TRUE)
{
	mEventTimer.stop();
}

FloaterAO::~FloaterAO()
{
}

void FloaterAO::reloading(BOOL yes)
{
	if(yes)
		mEventTimer.start();
	else
		mEventTimer.stop();

	mReloadCoverPanel->setVisible(yes);
	enableSetControls(!yes);
	enableStateControls(!yes);
}

BOOL FloaterAO::tick()
{
	// reloading took too long, probably missed the signal, so we hide the reload cover
	llwarns << "AO reloading timeout." << llendl;
	updateList();
	return FALSE;
}

void FloaterAO::updateSetParameters()
{
	mOverrideSitsCheckBox->setValue(mSelectedSet->getSitOverride());
	mOverrideSitsCheckBoxSmall->setValue(mSelectedSet->getSitOverride());
	mSmartCheckBox->setValue(mSelectedSet->getSmart());
	mDisableMouselookCheckBox->setValue(mSelectedSet->getMouselookDisable());
	BOOL isDefault=(mSelectedSet==AOEngine::instance().getDefaultSet()) ? TRUE : FALSE;
	mDefaultCheckBox->setValue(isDefault);
	mDefaultCheckBox->setEnabled(!isDefault);
	updateSmart();
}

void FloaterAO::updateAnimationList()
{
	S32 currentStateSelected=mStateSelector->getCurrentIndex();

	mStateSelector->removeall();
	onChangeAnimationSelection();

	if(!mSelectedSet)
	{
		mStateSelector->setEnabled(FALSE);
		mStateSelector->add(getString("ao_no_animations_loaded"));
		return;
	}

	for(U32 index=0;index<mSelectedSet->mStateNames.size();index++)
	{
		std::string stateName=mSelectedSet->mStateNames[index];
		AOSet::AOState* state=mSelectedSet->getStateByName(stateName);
		mStateSelector->add(stateName,state,ADD_BOTTOM,TRUE);
	}

	enableStateControls(TRUE);

	if(currentStateSelected==-1)
		mStateSelector->selectFirstItem();
	else
		mStateSelector->selectNthItem(currentStateSelected);

	onSelectState();
}

void FloaterAO::updateList()
{
	mReloadButton->setEnabled(TRUE);
	mImportRunning=FALSE;

	std::string currentSetName=mSetSelector->getSelectedItemLabel();
	if(currentSetName.empty())
		currentSetName=AOEngine::instance().getCurrentSetName();

	mSetList=AOEngine::instance().getSetList();
	mSetSelector->removeall();
	mSetSelectorSmall->removeall();
	mSetSelector->clear();
	mSetSelectorSmall->clear();

	mAnimationList->deleteAllItems();
	reloading(FALSE);

	if(mSetList.empty())
	{
		lldebugs << "empty set list" << llendl;
		mSetSelector->add(getString("ao_no_sets_loaded"));
		mSetSelectorSmall->add(getString("ao_no_sets_loaded"));
		mSetSelector->selectNthItem(0);
		mSetSelectorSmall->selectNthItem(0);
		enableSetControls(FALSE);
		return;
	}

	for(U32 index=0;index<mSetList.size();index++)
	{
		std::string setName=mSetList[index]->getName();
		mSetSelector->add(setName,&mSetList[index],ADD_BOTTOM,TRUE);
		mSetSelectorSmall->add(setName,&mSetList[index],ADD_BOTTOM,TRUE);
		if(setName.compare(currentSetName)==0)
		{
			mSelectedSet=AOEngine::instance().selectSetByName(currentSetName);
			mSetSelector->selectNthItem(index);
			mSetSelectorSmall->selectNthItem(index);
			updateSetParameters();
			updateAnimationList();
		}
	}
	enableSetControls(TRUE);
}

BOOL FloaterAO::postBuild()
{
	LLPanel* aoPanel=getChild<LLPanel>("animation_overrider_outer_panel");
	mMainInterfacePanel=aoPanel->getChild<LLPanel>("animation_overrider_panel");
	mSmallInterfacePanel=aoPanel->getChild<LLPanel>("animation_overrider_panel_small");
	mReloadCoverPanel=aoPanel->getChild<LLPanel>("ao_reload_cover");

	mSetSelector=mMainInterfacePanel->getChild<LLComboBox>("ao_set_selection_combo");
	mActivateSetButton=mMainInterfacePanel->getChild<LLButton>("ao_activate");
	mAddButton=mMainInterfacePanel->getChild<LLButton>("ao_add");
	mRemoveButton=mMainInterfacePanel->getChild<LLButton>("ao_remove");
	mDefaultCheckBox=mMainInterfacePanel->getChild<LLCheckBoxCtrl>("ao_default");
	mOverrideSitsCheckBox=mMainInterfacePanel->getChild<LLCheckBoxCtrl>("ao_sit_override");
	mSmartCheckBox=mMainInterfacePanel->getChild<LLCheckBoxCtrl>("ao_smart");
	mDisableMouselookCheckBox=mMainInterfacePanel->getChild<LLCheckBoxCtrl>("ao_disable_stands_in_mouselook");

	mStateSelector=mMainInterfacePanel->getChild<LLComboBox>("ao_state_selection_combo");
	mAnimationList=mMainInterfacePanel->getChild<LLScrollListCtrl>("ao_state_animation_list");
	mMoveUpButton=mMainInterfacePanel->getChild<LLButton>("ao_move_up");
	mMoveDownButton=mMainInterfacePanel->getChild<LLButton>("ao_move_down");
	mTrashButton=mMainInterfacePanel->getChild<LLButton>("ao_trash");
	mCycleCheckBox=mMainInterfacePanel->getChild<LLCheckBoxCtrl>("ao_cycle");
	mRandomizeCheckBox=mMainInterfacePanel->getChild<LLCheckBoxCtrl>("ao_randomize");
	mCycleTimeTextLabel=mMainInterfacePanel->getChild<LLTextBox>("ao_cycle_time_seconds_label");
	mCycleTimeSpinner=mMainInterfacePanel->getChild<LLSpinCtrl>("ao_cycle_time");

	mReloadButton=mMainInterfacePanel->getChild<LLButton>("ao_reload");
	mPreviousButton=mMainInterfacePanel->getChild<LLButton>("ao_previous");
	mNextButton=mMainInterfacePanel->getChild<LLButton>("ao_next");
	mLessButton=mMainInterfacePanel->getChild<LLButton>("ao_less");

	mSetSelectorSmall=mSmallInterfacePanel->getChild<LLComboBox>("ao_set_selection_combo_small");
	mMoreButton=mSmallInterfacePanel->getChild<LLButton>("ao_more");
	mPreviousButtonSmall=mSmallInterfacePanel->getChild<LLButton>("ao_previous_small");
	mNextButtonSmall=mSmallInterfacePanel->getChild<LLButton>("ao_next_small");
	mOverrideSitsCheckBoxSmall=mSmallInterfacePanel->getChild<LLCheckBoxCtrl>("ao_sit_override_small");

	mSetSelector->setCommitCallback(boost::bind(&FloaterAO::onSelectSet,this));
	mActivateSetButton->setCommitCallback(boost::bind(&FloaterAO::onClickActivate,this));
	mAddButton->setCommitCallback(boost::bind(&FloaterAO::onClickAdd,this));
	mRemoveButton->setCommitCallback(boost::bind(&FloaterAO::onClickRemove,this));
	mDefaultCheckBox->setCommitCallback(boost::bind(&FloaterAO::onCheckDefault,this));
	mOverrideSitsCheckBox->setCommitCallback(boost::bind(&FloaterAO::onCheckOverrideSits,this));
	mSmartCheckBox->setCommitCallback(boost::bind(&FloaterAO::onCheckSmart,this));
	mDisableMouselookCheckBox->setCommitCallback(boost::bind(&FloaterAO::onCheckDisableStands,this));

	mAnimationList->setCommitOnSelectionChange(TRUE);

	mStateSelector->setCommitCallback(boost::bind(&FloaterAO::onSelectState,this));
	mAnimationList->setCommitCallback(boost::bind(&FloaterAO::onChangeAnimationSelection,this));
	mMoveUpButton->setCommitCallback(boost::bind(&FloaterAO::onClickMoveUp,this));
	mMoveDownButton->setCommitCallback(boost::bind(&FloaterAO::onClickMoveDown,this));
	mTrashButton->setCommitCallback(boost::bind(&FloaterAO::onClickTrash,this));
	mCycleCheckBox->setCommitCallback(boost::bind(&FloaterAO::onCheckCycle,this));
	mRandomizeCheckBox->setCommitCallback(boost::bind(&FloaterAO::onCheckRandomize,this));
	mCycleTimeSpinner->setCommitCallback(boost::bind(&FloaterAO::onChangeCycleTime,this));

	mReloadButton->setCommitCallback(boost::bind(&FloaterAO::onClickReload,this));
	mPreviousButton->setCommitCallback(boost::bind(&FloaterAO::onClickPrevious,this));
	mNextButton->setCommitCallback(boost::bind(&FloaterAO::onClickNext,this));
	mLessButton->setCommitCallback(boost::bind(&FloaterAO::onClickLess,this));
	mOverrideSitsCheckBoxSmall->setCommitCallback(boost::bind(&FloaterAO::onCheckOverrideSitsSmall,this));

	mSetSelectorSmall->setCommitCallback(boost::bind(&FloaterAO::onSelectSetSmall,this));
	mMoreButton->setCommitCallback(boost::bind(&FloaterAO::onClickMore,this));
	mPreviousButtonSmall->setCommitCallback(boost::bind(&FloaterAO::onClickPrevious,this));
	mNextButtonSmall->setCommitCallback(boost::bind(&FloaterAO::onClickNext,this));

	updateSmart();

	AOEngine::instance().setReloadCallback(boost::bind(&FloaterAO::updateList,this));

	onChangeAnimationSelection();
	mMainInterfacePanel->setVisible(TRUE);
	mSmallInterfacePanel->setVisible(FALSE);
	reloading(TRUE);

	updateList();

	if(gSavedPerAccountSettings.getBOOL("UseFullAOInterface"))
		onClickMore();
	else
		onClickLess();

	return LLDockableFloater::postBuild();
}

void FloaterAO::enableSetControls(BOOL yes)
{
	mSetSelector->setEnabled(yes);
	mSetSelectorSmall->setEnabled(yes);
	mActivateSetButton->setEnabled(yes);
	mRemoveButton->setEnabled(yes);
	mDefaultCheckBox->setEnabled(yes);
	mOverrideSitsCheckBox->setEnabled(yes);
	mOverrideSitsCheckBoxSmall->setEnabled(yes);
	mDisableMouselookCheckBox->setEnabled(yes);
	if(!yes)
		enableStateControls(yes);
}

void FloaterAO::enableStateControls(BOOL yes)
{
	mStateSelector->setEnabled(yes);
	mAnimationList->setEnabled(yes);
	mCycleCheckBox->setEnabled(yes);
	if(yes)
		updateCycleParameters();
	else
	{
		mRandomizeCheckBox->setEnabled(yes);
		mCycleTimeTextLabel->setEnabled(yes);
		mCycleTimeSpinner->setEnabled(yes);
	}
	mPreviousButton->setEnabled(yes);
	mPreviousButtonSmall->setEnabled(yes);
	mNextButton->setEnabled(yes);
	mNextButtonSmall->setEnabled(yes);
	mCanDragAndDrop=yes;
}

void FloaterAO::onOpen(const LLSD& key)
{
	UtilityBar::instance().setAOInterfaceButtonExpanded(true);
}

void FloaterAO::onClose(bool app_quitting)
{
	if (!app_quitting)
	{
		UtilityBar::instance().setAOInterfaceButtonExpanded(false);
	}
}

void FloaterAO::onSelectSet()
{
	AOSet* set=AOEngine::instance().getSetByName(mSetSelector->getSelectedItemLabel());
	if(!set)
	{
		onRenameSet();
		return;
	}
	
	mSelectedSet=set;

	updateSetParameters();
	updateAnimationList();
}

void FloaterAO::onSelectSetSmall()
{
	// sync main set selector with small set selector
	mSetSelector->selectNthItem(mSetSelectorSmall->getCurrentIndex());

	mSelectedSet=AOEngine::instance().getSetByName(mSetSelectorSmall->getSelectedItemLabel());
	if(mSelectedSet)
	{
		updateSetParameters();
		updateAnimationList();

		// small selector activates the selected set immediately
		onClickActivate();
	}
}

void FloaterAO::onRenameSet()
{
	if(!mSelectedSet)
	{
		llwarns << "Rename AO set without set selected." << llendl;
		return;
	}

	std::string name=mSetSelector->getSimple();
	LLStringUtil::trim(name);

	if(!name.empty())
	{
		if(name.find_first_of(":|")==std::string::npos)
		{
			if(AOEngine::instance().renameSet(mSelectedSet,name))
			{
				reloading(TRUE);
				return;
			}
		}
		else
		{
			LLSD args;
			args["AO_SET_NAME"]=name;
			LLNotificationsUtil::add("RenameAOCantContainColon",args);
		}
	}
	mSetSelector->setSimple(mSelectedSet->getName());
}

void FloaterAO::onClickActivate()
{
	// sync small set selector with main set selector
	mSetSelectorSmall->selectNthItem(mSetSelector->getCurrentIndex());

	lldebugs << "Set activated: " << mSetSelector->getSelectedItemLabel() << llendl;
	AOEngine::instance().selectSet(mSelectedSet);
}

LLScrollListItem* FloaterAO::addAnimation(const std::string& name)
{
	LLSD row;
	row["columns"][0]["column"]="icon";
	row["columns"][0]["type"]="icon";
	row["columns"][0]["value"]="Inv_Animation";

	row["columns"][1]["column"]="animation_name";
	row["columns"][1]["type"]="text";
	row["columns"][1]["value"]=name;

	return mAnimationList->addElement(row);
}

void FloaterAO::onSelectState()
{
	mAnimationList->deleteAllItems();
	mAnimationList->setCommentText(getString("ao_no_animations_loaded"));
	mAnimationList->setEnabled(FALSE);

	onChangeAnimationSelection();

	if(!mSelectedSet)
		return;

	mSelectedState=mSelectedSet->getStateByName(mStateSelector->getSelectedItemLabel());
	if(!mSelectedState)
		return;

	mSelectedState=(AOSet::AOState*) mStateSelector->getCurrentUserdata();
	if(mSelectedState->mAnimations.size())
	{
		for(U32 index=0;index<mSelectedState->mAnimations.size();index++)
		{
			LLScrollListItem* item=addAnimation(mSelectedState->mAnimations[index].mName);
			if(item)
				item->setUserdata(&mSelectedState->mAnimations[index].mInventoryUUID);
		}

		mAnimationList->setCommentText("");
		mAnimationList->setEnabled(TRUE);
	}

	mCycleCheckBox->setValue(mSelectedState->mCycle);
	mRandomizeCheckBox->setValue(mSelectedState->mRandom);
	mCycleTimeSpinner->setValue(mSelectedState->mCycleTime);

	updateCycleParameters();
}

void FloaterAO::onClickReload()
{
	reloading(TRUE);

	mSelectedSet=0;
	mSelectedState=0;

	AOEngine::instance().reload(false);
	updateList();
}

void FloaterAO::onClickAdd()
{
	LLNotificationsUtil::add("NewAOSet",LLSD(),LLSD(),boost::bind(&FloaterAO::newSetCallback,this,_1,_2));
}

BOOL FloaterAO::newSetCallback(const LLSD& notification,const LLSD& response)
{
	std::string newSetName=response["message"].asString();
	S32 option=LLNotificationsUtil::getSelectedOption(notification,response);

	LLStringUtil::trim(newSetName);

	if(newSetName.empty())
		return FALSE;
	else if(newSetName.find_first_of(":|")!=std::string::npos)
	{
		LLSD args;
		args["AO_SET_NAME"]=newSetName;
		LLNotificationsUtil::add("NewAOCantContainColon",args);
		return FALSE;
	}

	if(option==0)
	{
		if(AOEngine::instance().addSet(newSetName).notNull())
		{
			reloading(TRUE);
			return TRUE;
		}
	}
	return FALSE;
}

void FloaterAO::onClickRemove()
{
	if(!mSelectedSet)
		return;

	LLSD args;
	args["AO_SET_NAME"]=mSelectedSet->getName();
	LLNotificationsUtil::add("RemoveAOSet",args,LLSD(),boost::bind(&FloaterAO::removeSetCallback,this,_1,_2));
}

BOOL FloaterAO::removeSetCallback(const LLSD& notification,const LLSD& response)
{
	std::string newSetName=response["message"].asString();
	S32 option=LLNotificationsUtil::getSelectedOption(notification,response);

	if(option==0)
	{
		if(AOEngine::instance().removeSet(mSelectedSet))
		{
			reloading(TRUE);
			// to prevent snapping back to deleted set
			mSetSelector->removeall();
			mSetSelectorSmall->removeall();
			// visually indicate there are no items left
			mSetSelector->clear();
			mSetSelectorSmall->clear();
			mAnimationList->deleteAllItems();
			return TRUE;
		}
	}
	return FALSE;
}

void FloaterAO::onCheckDefault()
{
	if(mSelectedSet)
		AOEngine::instance().setDefaultSet(mSelectedSet);
}

void FloaterAO::onCheckOverrideSits()
{
	mOverrideSitsCheckBoxSmall->setValue(mOverrideSitsCheckBox->getValue());
	if(mSelectedSet)
		AOEngine::instance().setOverrideSits(mSelectedSet,mOverrideSitsCheckBox->getValue().asBoolean());
	updateSmart();
}

void FloaterAO::onCheckOverrideSitsSmall()
{
	mOverrideSitsCheckBox->setValue(mOverrideSitsCheckBoxSmall->getValue());
	onCheckOverrideSits();
}

void FloaterAO::updateSmart()
{
	mSmartCheckBox->setEnabled(mOverrideSitsCheckBox->getValue());
}

void FloaterAO::onCheckSmart()
{
	if(mSelectedSet)
		AOEngine::instance().setSmart(mSelectedSet,mSmartCheckBox->getValue().asBoolean());
}

void FloaterAO::onCheckDisableStands()
{
	if(mSelectedSet)
		AOEngine::instance().setDisableStands(mSelectedSet,mDisableMouselookCheckBox->getValue().asBoolean());
}

void FloaterAO::onChangeAnimationSelection()
{
	std::vector<LLScrollListItem*> list=mAnimationList->getAllSelected();
	lldebugs << "Selection count: " << list.size() << llendl;

	BOOL resortEnable=FALSE;
	BOOL trashEnable=FALSE;

	// Linden Lab bug: scroll lists still select the first item when you click on them, even when they are disabled.
	// The control does not memorize it's enabled/disabled state, so mAnimationList->mEnabled() doesn't seem to work.
	// So we need to safeguard against it.
	if(!mCanDragAndDrop)
	{
		mAnimationList->deselectAllItems();
		lldebugs << "Selection count now: " << list.size() << llendl;
	}
	else if(list.size()>0)
	{
		if(list.size()==1)
			resortEnable=TRUE;
		trashEnable=TRUE;
	}

	mMoveDownButton->setEnabled(resortEnable);
	mMoveUpButton->setEnabled(resortEnable);
	mTrashButton->setEnabled(trashEnable);
}

void FloaterAO::onClickMoveUp()
{
	if(!mSelectedState)
		return;

	std::vector<LLScrollListItem*> list=mAnimationList->getAllSelected();
	if(list.size()!=1)
		return;

	S32 currentIndex=mAnimationList->getFirstSelectedIndex();
	if(currentIndex==-1)
		return;

	if(AOEngine::instance().swapWithPrevious(mSelectedState,currentIndex))
		mAnimationList->swapWithPrevious(currentIndex);
}

void FloaterAO::onClickMoveDown()
{
	if(!mSelectedState)
		return;

	std::vector<LLScrollListItem*> list=mAnimationList->getAllSelected();
	if(list.size()!=1)
		return;

	S32 currentIndex=mAnimationList->getFirstSelectedIndex();
	if(currentIndex>=(mAnimationList->getItemCount()-1))
		return;

	if(AOEngine::instance().swapWithNext(mSelectedState,currentIndex))
		mAnimationList->swapWithNext(currentIndex);
}

void FloaterAO::onClickTrash()
{
	if(!mSelectedState)
		return;

	std::vector<LLScrollListItem*> list=mAnimationList->getAllSelected();
	if(list.size()==0)
		return;

	for(S32 index=list.size()-1;index!=-1;index--)
		AOEngine::instance().removeAnimation(mSelectedSet,mSelectedState,mAnimationList->getItemIndex(list[index]));

	mAnimationList->deleteSelectedItems();
}

void FloaterAO::updateCycleParameters()
{
	BOOL yes=mCycleCheckBox->getValue().asBoolean();
	mRandomizeCheckBox->setEnabled(yes);
	mCycleTimeTextLabel->setEnabled(yes);
	mCycleTimeSpinner->setEnabled(yes);
}

void FloaterAO::onCheckCycle()
{
	if(mSelectedState)
	{
		BOOL yes=mCycleCheckBox->getValue().asBoolean();
		AOEngine::instance().setCycle(mSelectedState,yes);
		updateCycleParameters();
	}
}

void FloaterAO::onCheckRandomize()
{
	if(mSelectedState)
		AOEngine::instance().setRandomize(mSelectedState,mRandomizeCheckBox->getValue().asBoolean());
}

void FloaterAO::onChangeCycleTime()
{
	if(mSelectedState)
		AOEngine::instance().setCycleTime(mSelectedState,mCycleTimeSpinner->getValueF32());
}

void FloaterAO::onClickPrevious()
{
	AOEngine::instance().cycle(AOEngine::CyclePrevious);
}

void FloaterAO::onClickNext()
{
	AOEngine::instance().cycle(AOEngine::CycleNext);
}

void FloaterAO::onClickMore()
{
	LLRect fullSize=gSavedPerAccountSettings.getRect("floater_rect_animation_overrider_full");
	LLRect smallSize=getRect();

	if(fullSize.getHeight()<getMinHeight())
		fullSize.setOriginAndSize(fullSize.mLeft,fullSize.mBottom,fullSize.getWidth(),getRect().getHeight());

	if(fullSize.getWidth()<getMinWidth())
		fullSize.setOriginAndSize(fullSize.mLeft,fullSize.mBottom,getRect().getWidth(),fullSize.getHeight());

	mMore=TRUE;

	mSmallInterfacePanel->setVisible(FALSE);
	mMainInterfacePanel->setVisible(TRUE);
	setCanResize(TRUE);

	gSavedPerAccountSettings.setBOOL("UseFullAOInterface",TRUE);

	reshape(getRect().getWidth(),fullSize.getHeight());
}

void FloaterAO::onClickLess()
{
	LLRect fullSize=getRect();
	LLRect smallSize=mSmallInterfacePanel->getRect();
	smallSize.setLeftTopAndSize(0,0,smallSize.getWidth(),smallSize.getHeight()+getHeaderHeight());

	gSavedPerAccountSettings.setRect("floater_rect_animation_overrider_full",fullSize);

	mMore=FALSE;

	mSmallInterfacePanel->setVisible(TRUE);
	mMainInterfacePanel->setVisible(FALSE);
	setCanResize(FALSE);

	gSavedPerAccountSettings.setBOOL("UseFullAOInterface",FALSE);

	reshape(getRect().getWidth(),smallSize.getHeight());

	// save current size and position
	gSavedPerAccountSettings.setRect("floater_rect_animation_overrider_full",fullSize);
}

// virtual
BOOL FloaterAO::handleDragAndDrop(S32 x,S32 y,MASK mask,BOOL drop,EDragAndDropType type,void* data,
									EAcceptance* accept,std::string& tooltipMsg)
{
	// no drag & drop on small interface
	if(!mMore)
	{
		tooltipMsg="ao_dnd_only_on_full_interface";
		*accept=ACCEPT_NO;
		return TRUE;
	}

	LLInventoryItem* item=(LLInventoryItem*) data;

	if(type==DAD_NOTECARD)
	{
		if(mImportRunning)
		{
			*accept=ACCEPT_NO;
			return TRUE;
		}
		*accept=ACCEPT_YES_SINGLE;
		if(item && drop)
		{
			if(AOEngine::instance().importNotecard(item))
			{
				reloading(TRUE);
				mReloadButton->setEnabled(FALSE);
				mImportRunning=TRUE;
			}
		}
	}
	else if(type==DAD_ANIMATION)
	{
		if(!drop && (!mSelectedSet || !mSelectedState || !mCanDragAndDrop))
		{
			*accept=ACCEPT_NO;
			return TRUE;
		}
		*accept=ACCEPT_YES_MULTI;
		if(item && drop)
		{
			if(AOEngine::instance().addAnimation(mSelectedSet,mSelectedState,item))
			{
				addAnimation(item->getName());

				// TODO: this would be the right thing to do, but it blocks multi drop
				// before final release this must be resolved
				reloading(TRUE);
			}
		}
	}
	else
		*accept=ACCEPT_NO;

	return TRUE;
}
