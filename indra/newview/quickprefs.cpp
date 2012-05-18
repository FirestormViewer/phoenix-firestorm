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
	LLEnvManagerNew& env_mgr = LLEnvManagerNew::instance();
	LLComboBox* WWcomboBox = getChild<LLComboBox>("WaterPresetsCombo");
	if(WWcomboBox != NULL) {
		
		std::list<std::string> user_presets, system_presets;
		LLWaterParamManager::instance().getPresetNames(user_presets, system_presets);

		// Add user presets first.
		for (std::list<std::string>::const_iterator mIt = user_presets.begin(); mIt != user_presets.end(); ++mIt)
		{
				if((*mIt).length()>0) WWcomboBox->add(*mIt);
		}

		
		if (user_presets.size() > 0)
		{
			WWcomboBox->addSeparator();
		}

		// Add system presets.
		for (std::list<std::string>::const_iterator mIt = system_presets.begin(); mIt != system_presets.end(); ++mIt)
		{
				if((*mIt).length()>0) WWcomboBox->add(*mIt);
		}
		WWcomboBox->selectByValue(env_mgr.getWaterPresetName());
	}

	LLComboBox* WLcomboBox = getChild<LLComboBox>("WLPresetsCombo");
	
	if(WLcomboBox != NULL) {
		LLWLParamManager::preset_name_list_t user_presets, sys_presets, region_presets;
		LLWLParamManager::instance().getPresetNames(region_presets, user_presets, sys_presets);

		// Add user presets.
		for (LLWLParamManager::preset_name_list_t::const_iterator it = user_presets.begin(); it != user_presets.end(); ++it)
		{
			if((*it).length()>0) WLcomboBox->add(*it);
		}

		if (!user_presets.empty())
		{
			WLcomboBox->addSeparator();
		}

		// Add system presets.
		for (LLWLParamManager::preset_name_list_t::const_iterator it = sys_presets.begin(); it != sys_presets.end(); ++it)
		{
			if((*it).length()>0) WLcomboBox->add(*it);
		}
		WLcomboBox->selectByValue(env_mgr.getSkyPresetName());
	}
		getChild<LLMultiSliderCtrl>("WLSunPos")->addSlider(12.f);
		initCallbacks();
	return LLDockableFloater::postBuild();
}

void FloaterQuickPrefs::onChangeWaterPreset(LLUICtrl* ctrl)
{
	std::string data = ctrl->getValue().asString();
	if(!data.empty())
	{
	//-TT 2.8.2 - changed from pointer to object call
	//AO commented out for merge
		LLEnvManagerNew::instance().setUseWaterPreset(data);
	}
}

void FloaterQuickPrefs::onChangeSkyPreset(LLUICtrl* ctrl)
{

	std::string data = ctrl->getValue().asString();
	if(!data.empty())
	{	
		LLEnvManagerNew::instance().setUseSkyPreset(data);
	}
}

void FloaterQuickPrefs::deactivateAnimator()
{
	//-TT 2.8.2 - commented out for merge
	//LLWLParamManager::instance()->mAnimator.mIsRunning = false;
	//LLWLParamManager::instance()->mAnimator.mUseLindenTime = false;
}

void FloaterQuickPrefs::onClickWaterPrev()
{
	LLComboBox* box = getChild<LLComboBox>("WaterPresetsCombo");
	int curid=box->getCurrentIndex();
	int lastitem=box->getItemCount()-1;
	curid--;
	if(curid<0) curid=lastitem;
	else if(curid>lastitem) curid=0;
	box->setCurrentByIndex(curid);
	LLEnvManagerNew::instance().setUseWaterPreset(box->getValue().asString());
}

void FloaterQuickPrefs::onClickWaterNext()
{

	LLComboBox* box = getChild<LLComboBox>("WaterPresetsCombo");
	int curid=box->getCurrentIndex();
	int lastitem=box->getItemCount()-1;
	curid++;
	if(curid<0) curid=lastitem;
	else if(curid>lastitem) curid=0;
	box->setCurrentByIndex(curid);
	LLEnvManagerNew::instance().setUseWaterPreset(box->getValue().asString());
}

void FloaterQuickPrefs::onClickSkyPrev()
{
	LLComboBox* box = getChild<LLComboBox>("WLPresetsCombo");
	int curid=box->getCurrentIndex();
	int lastitem=box->getItemCount()-1;
	curid--;
	if(curid<0) curid=lastitem;
	else if(curid>lastitem) curid=0;
	box->setCurrentByIndex(curid);
	LLEnvManagerNew::instance().setUseSkyPreset(box->getValue().asString());
}

void FloaterQuickPrefs::onClickSkyNext()
{
	//-TT 2.8.2 - commented out for merge
	//// find place of current param
	//std::map<std::string, LLWLParamSet>::iterator mIt = 
	//	LLWLParamManager::instance()->mParamList.find(LLWLParamManager::instance()->mCurParams.mName);

	//// shouldn't happen unless you delete every preset but Default
	//if (mIt == LLWLParamManager::instance()->mParamList.end())
	//{
	//	llwarns << "No more presets left!" << llendl;
	//	return;
	//}

	//// if at the end, loop
	//std::map<std::string, LLWLParamSet>::iterator last = LLWLParamManager::instance()->mParamList.end(); --last;
	//if(mIt == last) 
	//{
	//	mIt = LLWLParamManager::instance()->mParamList.begin();
	//}
	//else
	//{
	//	mIt++;
	//}
	//	deactivateAnimator();
	//LLWLParamManager::instance()->loadPreset(mIt->first, true);
	LLComboBox* box = getChild<LLComboBox>("WLPresetsCombo");
	//WLcomboBox->setSimple(mIt->first);

	int curid=box->getCurrentIndex();
	int lastitem=box->getItemCount()-1;
	curid++;
	if(curid<0) curid=lastitem;
	else if(curid>lastitem) curid=0;
	box->setCurrentByIndex(curid);
	LLEnvManagerNew::instance().setUseSkyPreset(box->getValue().asString());
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

	LLWLParamManager * param_mgr = LLWLParamManager::getInstance();

	F32 time24 = sun_pos_to_time24(param_mgr->mCurParams.getFloat("sun_angle",err) / F_TWO_PI);
	getChild<LLMultiSliderCtrl>("WLSunPos")->setCurSliderValue(time24, TRUE);
}

void FloaterQuickPrefs::onSunMoved(LLUICtrl* ctrl, void* userdata)
{
	LLMultiSliderCtrl* sun_msldr = getChild<LLMultiSliderCtrl>("WLSunPos");
		F32 val = sun_msldr->getCurSliderValue() / 24.0f;
	LLWLParamManager::getInstance()->mAnimator.setDayTime((F64)val);
	LLWLParamManager::getInstance()->mAnimator.deactivate();
	LLWLParamManager::getInstance()->mAnimator.update(
		LLWLParamManager::getInstance()->mCurParams);
}

void FloaterQuickPrefs::onClickRegionWL()
{
	LLComboBox* WLbox = getChild<LLComboBox>("WLPresetsCombo");
	LLComboBox* WWbox = getChild<LLComboBox>("WaterPresetsCombo");
	WLbox->setSimple(LLStringExplicit("Default"));
	WWbox->setSimple(LLStringExplicit("Default"));
	LLEnvManagerNew& env_mgr = LLEnvManagerNew::instance();
	env_mgr.useRegionSettings();
}
