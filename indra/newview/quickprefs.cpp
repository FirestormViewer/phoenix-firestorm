/** 
 * @file quickprefs.cpp
 * @brief Quick preferences access panel for bottomtray
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 * Copyright (C) 2011, WoLf Loonie @ Second Life
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

#include "quickprefs.h"
#include "llboost.h"
#include "llbottomtray.h"
#include "llcombobox.h"
#include "llfloaterwindlight.h"
#include "llfloaterwater.h"
#include "llwlparamset.h"
#include "llwlparammanager.h"
#include "llwaterparammanager.h"


FloaterQuickPrefs::FloaterQuickPrefs(const LLSD& key)
:	LLTransientDockableFloater(NULL,true,key)
{
}

FloaterQuickPrefs::~FloaterQuickPrefs()
{
}

void FloaterQuickPrefs::onOpen(const LLSD& key)
{
	LLButton* anchor_panel=LLBottomTray::instance().getChild<LLButton>("quickprefs_toggle");
	if(anchor_panel)
		setDockControl(new LLDockControl(anchor_panel,this,getDockTongue(),LLDockControl::TOP));
}


void FloaterQuickPrefs::initCallbacks(void) {
	getChild<LLUICtrl>("WaterPresetsCombo")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onChangeWaterPreset, this, _1));
	getChild<LLUICtrl>("WLPresetsCombo")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onChangeSkyPreset, this, _1));
}

BOOL FloaterQuickPrefs::postBuild()
{
	childSetAction("WWnext", onClickWaterNext, this);
	childSetAction("WWprev", onClickWaterPrev, this);
	childSetAction("WLnext", onClickSkyNext, this);
	childSetAction("WLprev", onClickSkyPrev, this);

	LLComboBox* WWcomboBox = getChild<LLComboBox>("WaterPresetsCombo");
	if(WWcomboBox != NULL) {
		std::map<std::string, LLWaterParamSet>::iterator mIt =
			LLWaterParamManager::instance()->mParamList.begin();
		for(; mIt != LLWaterParamManager::instance()->mParamList.end(); mIt++) 
		{
			if (mIt->first.length() > 0)
				WWcomboBox->add(mIt->first);
		}
		WWcomboBox->add(LLStringUtil::null);
		WWcomboBox->setSimple(LLWaterParamManager::instance()->mCurParams.mName);
	}
	LLComboBox* WLcomboBox = getChild<LLComboBox>("WLPresetsCombo");
	if(WLcomboBox != NULL) {
		std::map<std::string, LLWLParamSet>::iterator mIt = 
			LLWLParamManager::instance()->mParamList.begin();
		for(; mIt != LLWLParamManager::instance()->mParamList.end(); mIt++) 
		{
			if (mIt->first.length() > 0)
				WLcomboBox->add(mIt->first);
		}
		WLcomboBox->add(LLStringUtil::null);
		WLcomboBox->setSimple(LLWLParamManager::instance()->mCurParams.mName);
	}
		initCallbacks();
	return LLDockableFloater::postBuild();
}

void FloaterQuickPrefs::onChangeWaterPreset(LLUICtrl* ctrl)
{
	std::string data = ctrl->getValue().asString();
	if(!data.empty())
	{
		LLWaterParamManager::instance()->loadPreset(data);
	}
}

void FloaterQuickPrefs::onChangeSkyPreset(LLUICtrl* ctrl)
{
	LLFloaterWindLight::deactivateAnimator();

	std::string data = ctrl->getValue().asString();
	if(!data.empty())
	{
		LLWLParamManager::instance()->loadPreset( data);
	}
}


void FloaterQuickPrefs::onClickWaterNext(void* user_data)
{
	FloaterQuickPrefs* self = (FloaterQuickPrefs*) user_data;
	
	LLWaterParamManager * param_mgr = LLWaterParamManager::instance();
	LLWaterParamSet& currentParams = param_mgr->mCurParams;

	// find place of current param
	std::map<std::string, LLWaterParamSet>::iterator mIt = 
		param_mgr->mParamList.find(currentParams.mName);

	// if at the end, loop
	std::map<std::string, LLWaterParamSet>::iterator last = param_mgr->mParamList.end(); --last;
	if(mIt == last) 
	{
		mIt = param_mgr->mParamList.begin();
	}
	else
	{
		mIt++;
	}
	param_mgr->loadPreset(mIt->first, true);
	LLComboBox* WWcomboBox = self->getChild<LLComboBox>("WaterPresetsCombo");
	WWcomboBox->setSimple(mIt->first);
}

void FloaterQuickPrefs::onClickWaterPrev(void* user_data)
{
	FloaterQuickPrefs* self = (FloaterQuickPrefs*) user_data;
	
	LLWaterParamManager * param_mgr = LLWaterParamManager::instance();
	LLWaterParamSet & currentParams = param_mgr->mCurParams;

	// find place of current param
	std::map<std::string, LLWaterParamSet>::iterator mIt = 
		param_mgr->mParamList.find(currentParams.mName);

	// if at the beginning, loop
	if(mIt == param_mgr->mParamList.begin()) 
	{
		std::map<std::string, LLWaterParamSet>::iterator last = param_mgr->mParamList.end(); --last;
		mIt = last;
	}
	else
	{
		mIt--;
	}
	param_mgr->loadPreset(mIt->first, true);
	LLComboBox* WWcomboBox = self->getChild<LLComboBox>("WaterPresetsCombo");
	WWcomboBox->setSimple(mIt->first);
}

void FloaterQuickPrefs::onClickSkyNext(void* user_data)
{
	FloaterQuickPrefs* self = (FloaterQuickPrefs*) user_data;
	
	// find place of current param
	std::map<std::string, LLWLParamSet>::iterator mIt = 
		LLWLParamManager::instance()->mParamList.find(LLWLParamManager::instance()->mCurParams.mName);

	// shouldn't happen unless you delete every preset but Default
	if (mIt == LLWLParamManager::instance()->mParamList.end())
	{
		llwarns << "No more presets left!" << llendl;
		return;
	}

	// if at the end, loop
	std::map<std::string, LLWLParamSet>::iterator last = LLWLParamManager::instance()->mParamList.end(); --last;
	if(mIt == last) 
	{
		mIt = LLWLParamManager::instance()->mParamList.begin();
	}
	else
	{
		mIt++;
	}
		LLFloaterWindLight::deactivateAnimator();
	LLWLParamManager::instance()->loadPreset(mIt->first, true);
	LLComboBox* WLcomboBox = self->getChild<LLComboBox>("WLPresetsCombo");
	WLcomboBox->setSimple(mIt->first);
}

void FloaterQuickPrefs::onClickSkyPrev(void* user_data)
{
	FloaterQuickPrefs* self = (FloaterQuickPrefs*) user_data;
	
	// find place of current param
	std::map<std::string, LLWLParamSet>::iterator mIt = 
		LLWLParamManager::instance()->mParamList.find(LLWLParamManager::instance()->mCurParams.mName);

	// shouldn't happen unless you delete every preset but Default
	if (mIt == LLWLParamManager::instance()->mParamList.end())
	{
		llwarns << "No more presets left!" << llendl;
		return;
	}

	// if at the beginning, loop
	if(mIt == LLWLParamManager::instance()->mParamList.begin()) 
	{
		std::map<std::string, LLWLParamSet>::iterator last = LLWLParamManager::instance()->mParamList.end(); --last;
		mIt = last;
	}
	else
	{
		mIt--;
	}
		LLFloaterWindLight::deactivateAnimator();
	LLWLParamManager::instance()->loadPreset(mIt->first, true);
	LLComboBox* WLcomboBox = self->getChild<LLComboBox>("WLPresetsCombo");
	WLcomboBox->setSimple(mIt->first);
}