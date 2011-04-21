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
#include "llbottomtray.h"
#include "llcheckboxctrl.h"
#include "llcombobox.h"
#include "llnotificationsutil.h"
#include "llspinctrl.h"
#include "llviewerinventory.h"

FloaterAO::FloaterAO(const LLSD& key)
:	LLTransientDockableFloater(NULL,true,key),
	mSetList(0),
	mSelectedSet(0),
	mSelectedState(0),
	mCanDragAndDrop(FALSE),
	mImportRunning(FALSE)
{
}

FloaterAO::~FloaterAO()
{
}

void FloaterAO::updateSetParameters()
{
	mOverrideSitsCheckBox->setValue(mSelectedSet->getSitOverride());
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
		mStateSelector->add("no_animations_loaded"); //              getString("no_animations_loaded"));
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
	mSetSelector->clear();
	mAnimationList->deleteAllItems();

	if(mSetList.empty())
	{
		lldebugs << "empty set list" << llendl;
		enableSetControls(FALSE);
		return;
	}

	for(U32 index=0;index<mSetList.size();index++)
	{
		std::string setName=mSetList[index]->getName();
		mSetSelector->add(setName,&mSetList[index],ADD_BOTTOM,TRUE);
		if(setName.compare(currentSetName)==0)
		{
			mSelectedSet=AOEngine::instance().selectSetByName(currentSetName);
			mSetSelector->selectNthItem(index);
			updateSetParameters();
			updateAnimationList();
		}
	}
	enableSetControls(TRUE);
}

BOOL FloaterAO::postBuild()
{
	LLPanel* aoPanel=getChild<LLPanel>("animation_overrider_panel");
	mSetSelector=aoPanel->getChild<LLComboBox>("ao_set_selection_combo");
	mActivateSetButton=aoPanel->getChild<LLButton>("ao_activate");
	mAddButton=aoPanel->getChild<LLButton>("ao_add");
	mRemoveButton=aoPanel->getChild<LLButton>("ao_remove");
	mDefaultCheckBox=aoPanel->getChild<LLCheckBoxCtrl>("ao_default");
	mOverrideSitsCheckBox=aoPanel->getChild<LLCheckBoxCtrl>("ao_sit_override");
	mSmartCheckBox=aoPanel->getChild<LLCheckBoxCtrl>("ao_smart");
	mDisableMouselookCheckBox=aoPanel->getChild<LLCheckBoxCtrl>("ao_disable_stands_in_mouselook");

	mStateSelector=aoPanel->getChild<LLComboBox>("ao_state_selection_combo");
	mAnimationList=aoPanel->getChild<LLScrollListCtrl>("ao_state_animation_list");
	mMoveUpButton=aoPanel->getChild<LLButton>("ao_move_up");
	mMoveDownButton=aoPanel->getChild<LLButton>("ao_move_down");
	mTrashButton=aoPanel->getChild<LLButton>("ao_trash");
	mCycleCheckBox=aoPanel->getChild<LLCheckBoxCtrl>("ao_cycle");
	mRandomizeCheckBox=aoPanel->getChild<LLCheckBoxCtrl>("ao_randomize");
	mCycleTimeTextLabel=aoPanel->getChild<LLTextBox>("ao_cycle_time_seconds_label");
	mCycleTimeSpinner=aoPanel->getChild<LLSpinCtrl>("ao_cycle_time");

	mPreviousButton=aoPanel->getChild<LLButton>("ao_previous");
	mNextButton=aoPanel->getChild<LLButton>("ao_next");
	mReloadButton=aoPanel->getChild<LLButton>("ao_reload");

	mSetSelector->setCommitCallback(boost::bind(&FloaterAO::onSelectSet,this));
	mActivateSetButton->setCommitCallback(boost::bind(&FloaterAO::onClickActivate,this));
	mAddButton->setCommitCallback(boost::bind(&FloaterAO::onClickAdd,this));
	mRemoveButton->setCommitCallback(boost::bind(&FloaterAO::onClickRemove,this));
	mDefaultCheckBox->setCommitCallback(boost::bind(&FloaterAO::onCheckDefault,this));
	mOverrideSitsCheckBox->setCommitCallback(boost::bind(&FloaterAO::onCheckOverrideSits,this));
	mSmartCheckBox->setCommitCallback(boost::bind(&FloaterAO::onCheckSmart,this));
	mDisableMouselookCheckBox->setCommitCallback(boost::bind(&FloaterAO::onCheckDisableStands,this));

	mPreviousButton->setCommitCallback(boost::bind(&FloaterAO::onClickPrevious,this));
	mNextButton->setCommitCallback(boost::bind(&FloaterAO::onClickNext,this));

	mAnimationList->setCommitOnSelectionChange(TRUE);

	mStateSelector->setCommitCallback(boost::bind(&FloaterAO::onSelectState,this));
	mAnimationList->setCommitCallback(boost::bind(&FloaterAO::onChangeAnimationSelection,this));
	mMoveUpButton->setCommitCallback(boost::bind(&FloaterAO::onClickMoveUp,this));
	mMoveDownButton->setCommitCallback(boost::bind(&FloaterAO::onClickMoveDown,this));
	mTrashButton->setCommitCallback(boost::bind(&FloaterAO::onClickTrash,this));
	mCycleCheckBox->setCommitCallback(boost::bind(&FloaterAO::onCheckCycle,this));
	mRandomizeCheckBox->setCommitCallback(boost::bind(&FloaterAO::onCheckRandomize,this));
	mCycleTimeSpinner->setCommitCallback(boost::bind(&FloaterAO::onChangeCycleTime,this));

	updateSmart();

	mReloadButton->setCommitCallback(boost::bind(&FloaterAO::onClickReload,this));

	AOEngine::instance().setReloadCallback(boost::bind(&FloaterAO::updateList,this));

	enableSetControls(FALSE);
	onChangeAnimationSelection();

	updateList();
	return LLDockableFloater::postBuild();
}

void FloaterAO::enableSetControls(BOOL yes)
{
	mSetSelector->setEnabled(yes);
	mActivateSetButton->setEnabled(yes);
	mRemoveButton->setEnabled(yes);
	mDefaultCheckBox->setEnabled(yes);
	mOverrideSitsCheckBox->setEnabled(yes);
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
	mNextButton->setEnabled(yes);
	mCanDragAndDrop=yes;
}

void FloaterAO::onOpen(const LLSD& key)
{
	LLButton* anchor_panel=LLBottomTray::instance().getChild<LLButton>("ao_toggle_btn");
	if(anchor_panel)
		setDockControl(new LLDockControl(anchor_panel,this,getDockTongue(),LLDockControl::TOP));
}

void FloaterAO::onSelectSet()
{
	mSelectedSet=AOEngine::instance().getSetByName(mSetSelector->getSelectedItemLabel());
	if(mSelectedSet)
	{
		updateSetParameters();
		updateAnimationList();
	}
}

void FloaterAO::onClickActivate()
{
	lldebugs << "Set activated: " << mSetSelector->getSelectedItemLabel() << llendl;
	AOEngine::instance().selectSet(mSelectedSet);
}

LLScrollListItem* FloaterAO::addAnimation(const std::string name)
{
	LLSD row;
	row["columns"][0]["type"]="icon";
	row["columns"][0]["value"]="Inv_Animation";
	row["columns"][0]["width"]=20;

	row["columns"][1]["type"]="text";
	row["columns"][1]["value"]=name;
	row["columns"][1]["width"]=120;			// 170 later

	return mAnimationList->addElement(row);
}

void FloaterAO::onSelectState()
{
	mAnimationList->deleteAllItems();
	onChangeAnimationSelection();

	if(!mSelectedSet)
		return;

	mSelectedState=mSelectedSet->getStateByName(mStateSelector->getSelectedItemLabel());
	if(!mSelectedState)
	{
//		mAnimationList->addSimpleElement("no_animations_loaded");   // 		getString("no_animations_loaded"));

		mAnimationList->setEnabled(FALSE);
		LLSD row;
/*		row["columns"][0]["type"]="icon";
		row["columns"][0]["value"]="";
		row["columns"][0]["width"]=20;
*/
		row["columns"][0]["type"]="text";
		row["columns"][0]["value"]="no_animations_loaded";  // 		getString("no_animations_loaded"));
		row["columns"][0]["width"]=190;
		mAnimationList->addElement(row);
		return;
	}

	mAnimationList->setEnabled(TRUE);
	mSelectedState=(AOSet::AOState*) mStateSelector->getCurrentUserdata();
	for(U32 index=0;index<mSelectedState->mAnimations.size();index++)
	{
		LLScrollListItem* item=addAnimation(mSelectedState->mAnimations[index].mName);
		if(item)
			item->setUserdata(&mSelectedState->mAnimations[index].mInventoryUUID);
	}

	mCycleCheckBox->setValue(mSelectedState->mCycle);
	mRandomizeCheckBox->setValue(mSelectedState->mRandom);
	mCycleTimeSpinner->setValue(mSelectedState->mCycleTime);

	updateCycleParameters();
}

void FloaterAO::onClickReload()
{
	enableSetControls(FALSE);
	enableStateControls(FALSE);

	mSelectedSet=0;
	mSelectedState=0;

	AOEngine::instance().reload();
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

	if(newSetName=="")
		return FALSE;
	else if(newSetName.find(":")!=std::string::npos)
	{
		LLNotificationsUtil::add("NewAOCantContainColon",LLSD());
		return FALSE;
	}

	if(option==0)
	{
		if(AOEngine::instance().addSet(newSetName).notNull())
		{
			enableSetControls(FALSE);
			enableStateControls(FALSE);
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
			// to prevent snapping back to deleted set
			mSetSelector->removeall();
			// visually indicate there are no items left
			mSetSelector->clear();
			mAnimationList->deleteAllItems();
			enableSetControls(FALSE);
			enableStateControls(FALSE);
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
	if(mSelectedSet)
		AOEngine::instance().setOverrideSits(mSelectedSet,mOverrideSitsCheckBox->getValue().asBoolean());
	updateSmart();
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

// virtual
BOOL FloaterAO::handleDragAndDrop(S32 x,S32 y,MASK mask,BOOL drop,EDragAndDropType type,void* data,
									EAcceptance* accept,std::string& tooltipMsg)
{
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
				mImportRunning=TRUE;
				enableSetControls(FALSE);
				enableStateControls(FALSE);
				mReloadButton->setEnabled(FALSE);
			}
		}
	}
	else if(type==DAD_ANIMATION)
	{
		if(!mSelectedSet || !mSelectedState || !mCanDragAndDrop)
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
				enableSetControls(FALSE);
				enableStateControls(FALSE);
			}
		}
	}
	else
		*accept=ACCEPT_NO;

	return TRUE;
}
