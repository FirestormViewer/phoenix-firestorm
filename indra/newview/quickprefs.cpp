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
#include "llcombobox.h"
#include "llwlparamset.h"
#include "llwlparammanager.h"
#include "llwaterparammanager.h"
#include "llfloatereditsky.h"
#include "llmultisliderctrl.h"
#include "lltimectrl.h"
#include "llenvmanager.h"
#include "llviewercontrol.h"

FloaterQuickPrefs::FloaterQuickPrefs(const LLSD& key)
:	LLTransientDockableFloater(NULL,true,key)
{
}

FloaterQuickPrefs::~FloaterQuickPrefs()
{
}

void FloaterQuickPrefs::onOpen(const LLSD& key)
{
}


void FloaterQuickPrefs::initCallbacks(void) {
	LLWLParamManager& param_mgr = LLWLParamManager::instance();
	getChild<LLUICtrl>("WaterPresetsCombo")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onChangeWaterPreset, this, _1));
	getChild<LLUICtrl>("WLPresetsCombo")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onChangeSkyPreset, this, _1));
	getChild<LLUICtrl>("WLPrevPreset")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onClickSkyPrev, this));
	getChild<LLUICtrl>("WLNextPreset")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onClickSkyNext, this));
	getChild<LLUICtrl>("WWPrevPreset")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onClickWaterPrev, this));
	getChild<LLUICtrl>("WWNextPreset")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onClickWaterNext, this));
	getChild<LLUICtrl>("UseRegionWL")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onClickRegionWL, this));
	getChild<LLUICtrl>("WLSunPos")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onSunMoved, this, _1, &param_mgr.mLightnorm));
}

BOOL FloaterQuickPrefs::postBuild()
{
	mWaterPresetsCombo = getChild<LLComboBox>("WaterPresetsCombo");
	mWLPresetsCombo = getChild<LLComboBox>("WLPresetsCombo");
	mWLSunPos = getChild<LLMultiSliderCtrl>("WLSunPos");

	LLEnvManagerNew& env_mgr = LLEnvManagerNew::instance();
	if (mWaterPresetsCombo != NULL)
	{
		
		std::list<std::string> user_presets, system_presets;
		LLWaterParamManager::instance().getPresetNames(user_presets, system_presets);

		// Add user presets first.
		for (std::list<std::string>::const_iterator mIt = user_presets.begin(); mIt != user_presets.end(); ++mIt)
		{
				if((*mIt).length()>0) mWaterPresetsCombo->add(*mIt);
		}

		
		if (user_presets.size() > 0)
		{
			mWaterPresetsCombo->addSeparator();
		}

		// Add system presets.
		for (std::list<std::string>::const_iterator mIt = system_presets.begin(); mIt != system_presets.end(); ++mIt)
		{
				if((*mIt).length()>0) mWaterPresetsCombo->add(*mIt);
		}
		mWaterPresetsCombo->selectByValue(env_mgr.getWaterPresetName());
	}

	
	if (mWLPresetsCombo != NULL)
	{
		LLWLParamManager::preset_name_list_t user_presets, sys_presets, region_presets;
		LLWLParamManager::instance().getPresetNames(region_presets, user_presets, sys_presets);

		// Add user presets.
		for (LLWLParamManager::preset_name_list_t::const_iterator it = user_presets.begin(); it != user_presets.end(); ++it)
		{
			if((*it).length()>0) mWLPresetsCombo->add(*it);
		}

		if (!user_presets.empty())
		{
			mWLPresetsCombo->addSeparator();
		}

		// Add system presets.
		for (LLWLParamManager::preset_name_list_t::const_iterator it = sys_presets.begin(); it != sys_presets.end(); ++it)
		{
			if((*it).length()>0) mWLPresetsCombo->add(*it);
		}
		mWLPresetsCombo->selectByValue(env_mgr.getSkyPresetName());
	}

	mWLSunPos->addSlider(12.f);
	initCallbacks();

	return LLDockableFloater::postBuild();
}

void FloaterQuickPrefs::onChangeWaterPreset(LLUICtrl* ctrl)
{
	std::string data = ctrl->getValue().asString();
	if(!data.empty())
	{
		LLEnvManagerNew::instance().setUseWaterPreset(data, (bool)gSavedSettings.getBOOL("FSInterpolateWater"));
		LLWaterParamManager::instance().getParamSet(data, LLWaterParamManager::instance().mCurParams);
	}
}

void FloaterQuickPrefs::onChangeSkyPreset(LLUICtrl* ctrl)
{
	std::string data = ctrl->getValue().asString();
	if(!data.empty())
	{	
		LLEnvManagerNew::instance().setUseSkyPreset(data, (bool)gSavedSettings.getBOOL("FSInterpolateSky"));
		LLWLParamManager::instance().getParamSet(LLWLParamKey(data, LLEnvKey::SCOPE_LOCAL), LLWLParamManager::instance().mCurParams);
	}
}

void FloaterQuickPrefs::deactivateAnimator()
{
	LLWLParamManager::instance().mAnimator.deactivate();
}

void FloaterQuickPrefs::onClickWaterPrev()
{
	LLWaterParamManager& mgr = LLWaterParamManager::instance();
	LLWaterParamSet& currentParams = mgr.mCurParams;

	std::map<std::string, LLWaterParamSet> param_list = LLWaterParamManager::instance().getPresets();

	// find place of current param
	std::map<std::string, LLWaterParamSet>::iterator mIt = param_list.find(currentParams.mName);

	// shouldn't happen unless you delete every preset but Default
	if (mIt == param_list.end())
	{
		llwarns << "No more presets left!" << llendl;
		return;
	}

	// if at the beginning, loop
	if (mIt == param_list.begin()) 
	{
		std::map<std::string, LLWaterParamSet>::iterator last = param_list.end();
		last--;
		mIt = last;
	}
	else
	{
		mIt--;
	}

	deactivateAnimator();

	mWaterPresetsCombo->setSimple(mIt->first);
	LLEnvManagerNew::instance().setUseWaterPreset(mIt->first, (bool)gSavedSettings.getBOOL("FSInterpolateWater"));
	mgr.getParamSet(mIt->first, mgr.mCurParams);
}

void FloaterQuickPrefs::onClickWaterNext()
{
	LLWaterParamManager& mgr = LLWaterParamManager::instance();
	LLWaterParamSet& currentParams = mgr.mCurParams;

	std::map<std::string, LLWaterParamSet> param_list = mgr.getPresets();

	// find place of current param
	std::map<std::string, LLWaterParamSet>::iterator mIt = param_list.find(currentParams.mName);

	// shouldn't happen unless you delete every preset but Default
	if (mIt == param_list.end())
	{
		llwarns << "No more presets left!" << llendl;
		return;
	}

	// if at the end, loop
	std::map<std::string, LLWaterParamSet>::iterator last = param_list.end();
	last--;
	if (mIt == last) 
	{
		mIt = param_list.begin();
	}
	else
	{
		mIt++;
	}

	deactivateAnimator();

	mWaterPresetsCombo->setSimple(mIt->first);
	LLEnvManagerNew::instance().setUseWaterPreset(mIt->first, (bool)gSavedSettings.getBOOL("FSInterpolateWater"));
	mgr.getParamSet(mIt->first, mgr.mCurParams);
}

void FloaterQuickPrefs::onClickSkyPrev()
{
	LLWLParamManager& mgr = LLWLParamManager::instance();
	std::map<LLWLParamKey, LLWLParamSet> param_list = mgr.getParamList();

	// find place of current param
	std::map<LLWLParamKey, LLWLParamSet>::iterator mIt;
	mIt = param_list.find(LLWLParamKey(mgr.mCurParams.mName, LLEnvKey::SCOPE_LOCAL));
	
	// shouldn't happen unless you delete every preset but Default
	if (mIt == param_list.end())
	{
		llwarns << "No more presets left!" << llendl;
		return;
	}

	// if at the beginning, loop
	if (mIt == param_list.begin())
	{
		std::map<LLWLParamKey, LLWLParamSet>::iterator last = param_list.end();
		last--;
		mIt = last;
	}
	else
	{
		mIt--;
	}

	deactivateAnimator();

	mWLPresetsCombo->setSimple(mIt->first.name);
	LLEnvManagerNew::instance().setUseSkyPreset(mIt->first.name, (bool)gSavedSettings.getBOOL("FSInterpolateSky"));
	mgr.getParamSet(mIt->first, mgr.mCurParams);
}

void FloaterQuickPrefs::onClickSkyNext()
{
	LLWLParamManager& mgr = LLWLParamManager::instance();

	std::map<LLWLParamKey, LLWLParamSet> param_list = mgr.getParamList();

	// find place of current param
	std::map<LLWLParamKey, LLWLParamSet>::iterator mIt;
	mIt = param_list.find(LLWLParamKey(mgr.mCurParams.mName, LLEnvKey::SCOPE_LOCAL));
	
	// shouldn't happen unless you delete every preset but Default
	if (mIt == param_list.end())
	{
		llwarns << "No more presets left!" << llendl;
		return;
	}

	// if at the end, loop
	std::map<LLWLParamKey, LLWLParamSet>::iterator last = param_list.end();
	last--;
	if (mIt == last)
	{
		mIt = param_list.begin();
	}
	else
	{
		mIt++;
	}

	deactivateAnimator();

	mWLPresetsCombo->setSimple(mIt->first.name);
	LLEnvManagerNew::instance().setUseSkyPreset(mIt->first.name, (bool)gSavedSettings.getBOOL("FSInterpolateSky"));
	mgr.getParamSet(mIt->first, mgr.mCurParams);
}

void FloaterQuickPrefs::draw()
{
	syncControls();
	LLFloater::draw();
}

static F32 sun_pos_to_time24(F32 sun_pos)
{
	return fmodf(sun_pos * 24.0f + 6, 24.0f);
}

void FloaterQuickPrefs::syncControls()
{
	bool err;

	LLWLParamManager& param_mgr = LLWLParamManager::instance();

	F32 time24 = sun_pos_to_time24(param_mgr.mCurParams.getFloat("sun_angle", err) / F_TWO_PI);
	mWLSunPos->setCurSliderValue(time24, TRUE);

	std::string current_wlpreset = param_mgr.mCurParams.mName;
	if (mWLPresetsCombo->getValue().asString() != current_wlpreset)
	{
		mWLPresetsCombo->setSimple(current_wlpreset);
	}

	std::string current_waterpreset = LLWaterParamManager::instance().mCurParams.mName;
	if (mWaterPresetsCombo->getValue().asString() != current_waterpreset)
	{
		mWaterPresetsCombo->setSimple(current_waterpreset);
	}
}

void FloaterQuickPrefs::onSunMoved(LLUICtrl* ctrl, void* userdata)
{
	F32 val = mWLSunPos->getCurSliderValue() / 24.0f;

	LLWLParamManager& mgr = LLWLParamManager::instance();
	mgr.mAnimator.setDayTime((F64)val);
	mgr.mAnimator.deactivate();
	mgr.mAnimator.update(mgr.mCurParams);
}

void FloaterQuickPrefs::onClickRegionWL()
{
	mWLPresetsCombo->setSimple(LLStringExplicit("Default"));
	mWaterPresetsCombo->setSimple(LLStringExplicit("Default"));
	LLEnvManagerNew::instance().useRegionSettings();
	LLWLParamManager::instance().getParamSet(LLWLParamKey("Default", LLEnvKey::SCOPE_LOCAL), LLWLParamManager::instance().mCurParams);
}
