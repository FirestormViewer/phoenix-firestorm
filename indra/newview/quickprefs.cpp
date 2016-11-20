/** 
 * @file quickprefs.cpp
 * @brief Quick preferences access panel for bottomtray
 *
 * $LicenseInfo:firstyear=2011&license=viewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (C) 2011, WoLf Loonie @ Second Life
 * Copyright (C) 2013, Zi Ree @ Second Life
 * Copyright (C) 2013, Ansariel Hiller @ Second Life
 * Copyright (C) 2013, Cinder Biscuits @ Me too
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
 * http://www.firestormviewer.org
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "quickprefs.h"

#include "fscommon.h"
#include "llagent.h"
#include "llappviewer.h"
#include "llcheckboxctrl.h"
#include "llcolorswatch.h"
#include "llcombobox.h"
#include "llcubemap.h"
#include "lldaycyclemanager.h"
#include "llenvmanager.h"
#include "llf32uictrl.h"
#include "llfeaturemanager.h"
#include "llfloaterpreference.h" // for LLAvatarComplexityControls
#include "llfloaterreg.h"
#include "lllayoutstack.h"
#include "llmultisliderctrl.h"
#include "llnotificationsutil.h"
#include "llsliderctrl.h"
#include "llspinctrl.h"
#include "lltoolbarview.h"
#include "llviewercontrol.h"
#include "llviewerregion.h"
#include "llvoavatar.h"
#include "llvoavatarself.h"
#include "llwaterparammanager.h"
#include "llwlparamset.h"
#include "llwlparammanager.h"
#include "rlvhandler.h"
#include <boost/foreach.hpp>


static F32 sun_pos_to_time24(F32 sun_pos)
{
	return fmodf(sun_pos * 24.0f + 6.f, 24.0f);
}

static F32 time24_to_sun_pos(F32 time)
{
	F32 ret = time - 6.f;
	if (ret < 0.f)
	{
		ret += 24.f;
	}

	return (ret / 24.f);
}


FloaterQuickPrefs::QuickPrefsXML::QuickPrefsXML()
:	entries("entries")
{}

FloaterQuickPrefs::QuickPrefsXMLEntry::QuickPrefsXMLEntry()
:	control_name("control_name"),
	label("label"),
	translation_id("translation_id"),
	control_type("control_type"),
	integer("integer"),
	min_value("min"),		// "min" is frowned upon by a braindead windows include
	max_value("max"),		// "max" see "min"
	increment("increment")
{}
// </FS:Zi>

FloaterQuickPrefs::FloaterQuickPrefs(const LLSD& key)
:	LLTransientDockableFloater(NULL, true, key),
	mAvatarZOffsetSlider(NULL),
	mRlvBehaviorCallbackConnection(),
	mRegionChangedSlot()
{
	// For Phototools
	mCommitCallbackRegistrar.add("Quickprefs.ShaderChanged", boost::bind(&handleSetShaderChanged, LLSD()));

	if (!getIsPhototools() && !FSCommon::isLegacySkin())
	{
		LLTransientFloaterMgr::getInstance()->addControlView(this);
	}
}

FloaterQuickPrefs::~FloaterQuickPrefs()
{
	if (mRlvBehaviorCallbackConnection.connected())
	{
		mRlvBehaviorCallbackConnection.disconnect();
	}

	if (mRegionChangedSlot.connected())
	{
		mRegionChangedSlot.disconnect();
	}

	if (!getIsPhototools() && !FSCommon::isLegacySkin())
	{
		LLTransientFloaterMgr::getInstance()->removeControlView(this);
	}
}

void FloaterQuickPrefs::onOpen(const LLSD& key)
{
	// Make sure IndirectMaxNonImpostors gets set properly
	LLAvatarComplexityControls::setIndirectMaxNonImpostors();

	// bail out here if this is a reused Phototools floater
	if (getIsPhototools())
	{
		return;
	}

	// Make sure IndirectMaxComplexity gets set properly
	LLAvatarComplexityControls::setIndirectMaxArc();
	LLAvatarComplexityControls::setText(gSavedSettings.getU32("RenderAvatarMaxComplexity"), mMaxComplexityLabel);

	gSavedSettings.setBOOL("QuickPrefsEditMode", FALSE);

	// Scan widgets and reapply control variables because some control types
	// (LLSliderCtrl for example) don't update their GUI when hidden
	control_list_t::iterator it;
	for (it = mControlsList.begin(); it != mControlsList.end(); ++it)
	{
		const ControlEntry& entry = it->second;

		LLUICtrl* current_widget = entry.widget;
		if (!current_widget)
		{
			LL_WARNS() << "missing widget for control " << it->first << LL_ENDL;
			continue;
		}

		LLControlVariable* var = current_widget->getControlVariable();
		if (var)
		{
			current_widget->setValue(var->getValue());
		}
	}

	dockToToolbarButton();
}


void FloaterQuickPrefs::initCallbacks()
{
	getChild<LLUICtrl>("UseRegionWindlight")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onChangeUseRegionWindlight, this));
	getChild<LLUICtrl>("WaterPresetsCombo")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onChangeWaterPreset, this));
	getChild<LLUICtrl>("WLPresetsCombo")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onChangeSkyPreset, this));
	getChild<LLUICtrl>("DCPresetsCombo")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onChangeDayCyclePreset, this));
	getChild<LLUICtrl>("WLPrevPreset")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onClickSkyPrev, this));
	getChild<LLUICtrl>("WLNextPreset")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onClickSkyNext, this));
	getChild<LLUICtrl>("WWPrevPreset")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onClickWaterPrev, this));
	getChild<LLUICtrl>("WWNextPreset")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onClickWaterNext, this));
	getChild<LLUICtrl>("DCPrevPreset")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onClickDayCyclePrev, this));
	getChild<LLUICtrl>("DCNextPreset")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onClickDayCycleNext, this));
	getChild<LLUICtrl>("ResetToRegionDefault")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onClickResetToRegionDefault, this));
	getChild<LLUICtrl>("WLSunPos")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onSunMoved, this));

	// Phototools additions
	if (getIsPhototools())
	{
		gSavedSettings.getControl("VertexShaderEnable")->getSignal()->connect(boost::bind(&FloaterQuickPrefs::refreshSettings, this));
		gSavedSettings.getControl("WindLightUseAtmosShaders")->getSignal()->connect(boost::bind(&FloaterQuickPrefs::refreshSettings, this));
		gSavedSettings.getControl("RenderDeferred")->getSignal()->connect(boost::bind(&FloaterQuickPrefs::refreshSettings, this));
		gSavedSettings.getControl("RenderAvatarVP")->getSignal()->connect(boost::bind(&FloaterQuickPrefs::refreshSettings, this));
		gSavedSettings.getControl("RenderShadowDetail")->getSignal()->connect(boost::bind(&FloaterQuickPrefs::refreshSettings, this));
		gSavedSettings.getControl("FSRenderVignette")->getSignal()->connect(boost::bind(&FloaterQuickPrefs::refreshSettings, this));
		gSavedSettings.getControl("RenderShadowSplitExponent")->getSignal()->connect(boost::bind(&FloaterQuickPrefs::refreshSettings, this));
		gSavedSettings.getControl("RenderShadowGaussian")->getSignal()->connect(boost::bind(&FloaterQuickPrefs::refreshSettings, this));
		gSavedSettings.getControl("RenderSSAOEffect")->getSignal()->connect(boost::bind(&FloaterQuickPrefs::refreshSettings, this));

		// Vignette UI controls
		getChild<LLSpinCtrl>("VignetteSpinnerX")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onChangeVignetteSpinnerX, this));
		getChild<LLSlider>("VignetteSliderX")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onChangeVignetteX, this));
		getChild<LLButton>("Reset_VignetteX")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onClickResetVignetteX, this));
		getChild<LLSpinCtrl>("VignetteSpinnerY")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onChangeVignetteSpinnerY, this));
		getChild<LLSlider>("VignetteSliderY")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onChangeVignetteY, this));
		getChild<LLButton>("Reset_VignetteY")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onClickResetVignetteY, this));
		getChild<LLSpinCtrl>("VignetteSpinnerZ")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onChangeVignetteSpinnerZ, this));
		getChild<LLSlider>("VignetteSliderZ")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onChangeVignetteZ, this));
		getChild<LLButton>("Reset_VignetteZ")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onClickResetVignetteZ, this));

		getChild<LLSlider>("SB_Shd_Clarity")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onChangeRenderShadowSplitExponentSlider, this));
		getChild<LLSpinCtrl>("S_Shd_Clarity")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onChangeRenderShadowSplitExponentSpinner, this));
		getChild<LLButton>("Shd_Clarity_Reset")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onClickResetRenderShadowSplitExponentY, this));

		getChild<LLSlider>("SB_Shd_Soften")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onChangeRenderShadowGaussianSlider, this));
		getChild<LLSlider>("SB_AO_Soften")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onChangeRenderShadowGaussianSlider, this));
		getChild<LLSpinCtrl>("S_Shd_Soften")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onChangeRenderShadowGaussianSpinner, this));
		getChild<LLSpinCtrl>("S_AO_Soften")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onChangeRenderShadowGaussianSpinner, this));
		getChild<LLButton>("Shd_Soften_Reset")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onClickResetRenderShadowGaussianX, this));
		getChild<LLButton>("Reset_AO_Soften")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onClickResetRenderShadowGaussianY, this));

		getChild<LLSlider>("SB_Effect")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onChangeRenderSSAOEffectSlider, this));
		getChild<LLSpinCtrl>("S_Effect")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onChangeRenderSSAOEffectSpinner, this));
		getChild<LLButton>("Reset_Effect")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onClickResetRenderSSAOEffectX, this));
	}
	else
	{
		getChild<LLButton>("Restore_Btn")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onClickRestoreDefaults, this));
		gSavedSettings.getControl("QuickPrefsEditMode")->getSignal()->connect(boost::bind(&FloaterQuickPrefs::onEditModeChanged, this));	// <FS:Zi> Dynamic Quickprefs

		mAvatarZOffsetSlider->setSliderMouseUpCallback(boost::bind(&FloaterQuickPrefs::onAvatarZOffsetFinalCommit, this));
		mAvatarZOffsetSlider->setSliderEditorCommitCallback(boost::bind(&FloaterQuickPrefs::onAvatarZOffsetFinalCommit, this));
		mAvatarZOffsetSlider->setCommitCallback(boost::bind(&FloaterQuickPrefs::onAvatarZOffsetSliderMoved, this));

		mMaxComplexitySlider->setCommitCallback(boost::bind(&FloaterQuickPrefs::updateMaxComplexity, this));
		gSavedSettings.getControl("RenderAvatarMaxComplexity")->getCommitSignal()->connect(boost::bind(&FloaterQuickPrefs::updateMaxComplexityLabel, this, _2));

		syncAvatarZOffsetFromPreferenceSetting();
		// Update slider on future pref changes.
		if (gSavedPerAccountSettings.getControl("AvatarHoverOffsetZ"))
		{
			gSavedPerAccountSettings.getControl("AvatarHoverOffsetZ")->getCommitSignal()->connect(boost::bind(&FloaterQuickPrefs::syncAvatarZOffsetFromPreferenceSetting, this));
		}
		else
		{
			LL_WARNS() << "Control not found for AvatarHoverOffsetZ" << LL_ENDL;
		}

		if (!mRegionChangedSlot.connected())
		{
			mRegionChangedSlot = gAgent.addRegionChangedCallback(boost::bind(&FloaterQuickPrefs::onRegionChanged, this));
		}
	}

	mRlvBehaviorCallbackConnection = gRlvHandler.setBehaviourCallback(boost::bind(&FloaterQuickPrefs::updateRlvRestrictions, this, _1, _2));
	gSavedSettings.getControl("IndirectMaxNonImpostors")->getCommitSignal()->connect(boost::bind(&FloaterQuickPrefs::updateMaxNonImpostors, this, _2));

	LLWLParamManager::instance().setPresetListChangeCallback(boost::bind(&FloaterQuickPrefs::reloadPresetsAndSelect, QP_PARAM_SKY));
	LLWaterParamManager::instance().setPresetListChangeCallback(boost::bind(&FloaterQuickPrefs::reloadPresetsAndSelect, QP_PARAM_WATER));
	LLDayCycleManager::instance().setModifyCallback(boost::bind(&FloaterQuickPrefs::reloadPresetsAndSelect, QP_PARAM_DAYCYCLE));
}

void FloaterQuickPrefs::loadDayCyclePresets()
{
	LLDayCycleManager::preset_name_list_t user_presets, sys_presets;
	LLDayCycleManager::instance().getPresetNames(user_presets, sys_presets);

	mDayCyclePresetsCombo->operateOnAll(LLComboBox::OP_DELETE);
	mDayCyclePresetsCombo->add(LLTrans::getString("QP_WL_Region_Default"), LLSD(PRESET_NAME_REGION_DEFAULT));
	mDayCyclePresetsCombo->add(LLTrans::getString("QP_WL_None"), LLSD(PRESET_NAME_NONE));
	mDayCyclePresetsCombo->addSeparator();

	// Add user presets.
	for (LLDayCycleManager::preset_name_list_t::const_iterator it = user_presets.begin(); it != user_presets.end(); ++it)
	{
		std::string preset_name = *it;
		if (!preset_name.empty())
		{
			mDayCyclePresetsCombo->add(preset_name, LLSD(preset_name));
		}
	}

	if (!user_presets.empty() && !sys_presets.empty())
	{
		mDayCyclePresetsCombo->addSeparator();
	}

	// Add system presets.
	for (LLDayCycleManager::preset_name_list_t::const_iterator it = sys_presets.begin(); it != sys_presets.end(); ++it)
	{
		std::string preset_name = *it;
		if (!preset_name.empty())
		{
			mDayCyclePresetsCombo->add(preset_name, LLSD(preset_name));
		}
	}
}

void FloaterQuickPrefs::loadSkyPresets()
{
	LLWLParamManager::preset_name_list_t user_presets, sys_presets, region_presets;
	LLWLParamManager::instance().getPresetNames(region_presets, user_presets, sys_presets);

	mWLPresetsCombo->operateOnAll(LLComboBox::OP_DELETE);
	mWLPresetsCombo->add(LLTrans::getString("QP_WL_Region_Default"), LLSD(PRESET_NAME_REGION_DEFAULT));
	mWLPresetsCombo->add(LLTrans::getString("QP_WL_Day_Cycle_Based"), LLSD(PRESET_NAME_SKY_DAY_CYCLE));
	mWLPresetsCombo->addSeparator();

	// Add user presets.
	for (LLWLParamManager::preset_name_list_t::const_iterator it = user_presets.begin(); it != user_presets.end(); ++it)
	{
		std::string preset_name = *it;
		if (!preset_name.empty())
		{
			mWLPresetsCombo->add(preset_name, LLSD(preset_name));
		}
	}

	if (!user_presets.empty() && !sys_presets.empty())
	{
		mWLPresetsCombo->addSeparator();
	}

	// Add system presets.
	for (LLWLParamManager::preset_name_list_t::const_iterator it = sys_presets.begin(); it != sys_presets.end(); ++it)
	{
		std::string preset_name = *it;
		if (!preset_name.empty())
		{
			mWLPresetsCombo->add(preset_name, LLSD(preset_name));
		}
	}
}

void FloaterQuickPrefs::loadWaterPresets()
{
	std::list<std::string> user_presets, system_presets;
	LLWaterParamManager::instance().getPresetNames(user_presets, system_presets);

	mWaterPresetsCombo->operateOnAll(LLComboBox::OP_DELETE);
	mWaterPresetsCombo->add(LLTrans::getString("QP_WL_Region_Default"), LLSD(PRESET_NAME_REGION_DEFAULT));
	mWaterPresetsCombo->addSeparator();

	// Add user presets first.
	for (std::list<std::string>::const_iterator mIt = user_presets.begin(); mIt != user_presets.end(); ++mIt)
	{
		std::string preset_name = *mIt;
		if (!preset_name.empty())
		{
			mWaterPresetsCombo->add(preset_name, LLSD(preset_name));
		}
	}

	if (!user_presets.empty() && !system_presets.empty())
	{
		mWaterPresetsCombo->addSeparator();
	}

	// Add system presets.
	for (std::list<std::string>::const_iterator mIt = system_presets.begin(); mIt != system_presets.end(); ++mIt)
	{
		std::string preset_name = *mIt;
		if (!preset_name.empty())
		{
			mWaterPresetsCombo->add(preset_name, LLSD(preset_name));
		}
	}
}

void FloaterQuickPrefs::loadPresets()
{
	loadWaterPresets();
	loadSkyPresets();
	loadDayCyclePresets();
}

BOOL FloaterQuickPrefs::postBuild()
{
	// Phototools additions
	if (getIsPhototools())
	{
		mCtrlShaderEnable = getChild<LLCheckBoxCtrl>("BasicShaders");
		mCtrlWindLight = getChild<LLCheckBoxCtrl>("WindLightUseAtmosShaders");
		mCtrlDeferred = getChild<LLCheckBoxCtrl>("RenderDeferred");
		mCtrlUseSSAO = getChild<LLCheckBoxCtrl>("UseSSAO");
		mCtrlUseDoF = getChild<LLCheckBoxCtrl>("UseDepthofField");
		mCtrlShadowDetail = getChild<LLComboBox>("ShadowDetail");
		mCtrlAvatarShadowDetail = getChild<LLComboBox>("AvatarShadowDetail");
		mCtrlReflectionDetail = getChild<LLComboBox>("Reflections");

		// Vignette UI controls
		mSpinnerVignetteX = getChild<LLSpinCtrl>("VignetteSpinnerX");
		mSpinnerVignetteY = getChild<LLSpinCtrl>("VignetteSpinnerY");
		mSpinnerVignetteZ = getChild<LLSpinCtrl>("VignetteSpinnerZ");
		mSliderVignetteX = getChild<LLSlider>("VignetteSliderX");
		mSliderVignetteY = getChild<LLSlider>("VignetteSliderY");
		mSliderVignetteZ = getChild<LLSlider>("VignetteSliderZ");

		mSliderRenderShadowSplitExponentY = getChild<LLSlider>("SB_Shd_Clarity");
		mSpinnerRenderShadowSplitExponentY = getChild<LLSpinCtrl>("S_Shd_Clarity");

		mSliderRenderShadowGaussianX = getChild<LLSlider>("SB_Shd_Soften");
		mSliderRenderShadowGaussianY = getChild<LLSlider>("SB_AO_Soften");
		mSpinnerRenderShadowGaussianX = getChild<LLSpinCtrl>("S_Shd_Soften");
		mSpinnerRenderShadowGaussianY = getChild<LLSpinCtrl>("S_AO_Soften");

		mSliderRenderSSAOEffectX = getChild<LLSlider>("SB_Effect");
		mSpinnerRenderSSAOEffectX = getChild<LLSpinCtrl>("S_Effect");

		refreshSettings();
	}
	else
	{
		mBtnResetDefaults = getChild<LLButton>("Restore_Btn");

		mAvatarZOffsetSlider = getChild<LLSliderCtrl>("HoverHeightSlider");
		mAvatarZOffsetSlider->setMinValue(MIN_HOVER_Z);
		mAvatarZOffsetSlider->setMaxValue(MAX_HOVER_Z);

		mMaxComplexitySlider = getChild<LLSliderCtrl>("IndirectMaxComplexity");
		mMaxComplexityLabel = getChild<LLTextBox>("IndirectMaxComplexityText");
	}

	mWaterPresetsCombo = getChild<LLComboBox>("WaterPresetsCombo");
	mWLPresetsCombo = getChild<LLComboBox>("WLPresetsCombo");
	mDayCyclePresetsCombo = getChild<LLComboBox>("DCPresetsCombo");
	mWLSunPos = getChild<LLMultiSliderCtrl>("WLSunPos");
	mUseRegionWindlight = getChild<LLCheckBoxCtrl>("UseRegionWindlight");
	mWLSunPos->addSlider(12.f);

	initCallbacks();
	loadPresets();

	if (gRlvHandler.isEnabled())
	{
		enableWindlightButtons(!gRlvHandler.hasBehaviour(RLV_BHVR_SETENV));
	}

	// <FS:Zi> Dynamic quick prefs

	// bail out here if this is a reused Phototools floater
	if (getIsPhototools())
	{
		return LLTransientDockableFloater::postBuild();
	}

	// find the layout_stack to insert the controls into
	mOptionsStack = getChild<LLLayoutStack>("options_stack");

	// get the path to the user defined or default quick preferences settings
	loadSavedSettingsFromFile(getSettingsPath(false));
	
	// get edit widget pointers
	mControlLabelEdit = getChild<LLLineEditor>("label_edit");
	mControlNameCombo = getChild<LLComboBox>("control_name_combo");
	mControlTypeCombo = getChild<LLComboBox>("control_type_combo_box");
	mControlIntegerCheckbox = getChild<LLCheckBoxCtrl>("control_integer_checkbox");
	mControlMinSpinner = getChild<LLSpinCtrl>("control_min_edit");
	mControlMaxSpinner = getChild<LLSpinCtrl>("control_max_edit");
	mControlIncrementSpinner = getChild<LLSpinCtrl>("control_increment_edit");

	// wire up callbacks for changed values
	mControlLabelEdit->setCommitCallback(boost::bind(&FloaterQuickPrefs::onValuesChanged, this));
	mControlNameCombo->setCommitCallback(boost::bind(&FloaterQuickPrefs::onValuesChanged, this));
	mControlTypeCombo->setCommitCallback(boost::bind(&FloaterQuickPrefs::onValuesChanged, this));
	mControlIntegerCheckbox->setCommitCallback(boost::bind(&FloaterQuickPrefs::onValuesChanged, this));
	mControlMinSpinner->setCommitCallback(boost::bind(&FloaterQuickPrefs::onValuesChanged, this));
	mControlMaxSpinner->setCommitCallback(boost::bind(&FloaterQuickPrefs::onValuesChanged, this));
	mControlIncrementSpinner->setCommitCallback(boost::bind(&FloaterQuickPrefs::onValuesChanged, this));

	// wire up ordering and adding buttons
	getChild<LLButton>("move_up_button")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onMoveUpClicked, this));
	getChild<LLButton>("move_down_button")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onMoveDownClicked, this));
	getChild<LLButton>("add_new_button")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onAddNewClicked, this));

	// functor to add debug settings to the editor dropdown
	struct f : public LLControlGroup::ApplyFunctor
	{
		LLComboBox* combo;
		f(LLComboBox* c) : combo(c) {}
		virtual void apply(const std::string& name, LLControlVariable* control)
		{
			// do not add things that are hidden in the debug settings floater
			if (!control->isHiddenFromSettingsEditor())
			{
				// don't add floater positions, sizes or visibility values
				if (name.find("floater_") != 0)
				{
					(*combo).addSimpleElement(name);
				}
			}
		}
	} func(mControlNameCombo);

	// add global and per account settings to the dropdown
	gSavedSettings.applyToAll(&func);
	gSavedPerAccountSettings.applyToAll(&func);
	mControlNameCombo->sortByName();
	// </FS:Zi>

	updateAvatarZOffsetEditEnabled();
	onRegionChanged();

	return LLTransientDockableFloater::postBuild();
}

void FloaterQuickPrefs::loadSavedSettingsFromFile(const std::string& settings_path)
{
	QuickPrefsXML xml;
	LLXMLNodePtr root;
	
	if (!LLXMLNode::parseFile(settings_path, root, NULL))
	{
		LL_WARNS() << "Unable to load quick preferences from file: " << settings_path << LL_ENDL;
	}
	else if (!root->hasName("quickprefs"))
	{
		LL_WARNS() << settings_path << " is not a valid quick preferences definition file" << LL_ENDL;
	}
	else
	{
		// Parse the quick preferences settings
		LLXUIParser parser;
		parser.readXUI(root, xml, settings_path);
		
		if (!xml.validateBlock())
		{
			LL_WARNS() << "Unable to validate quick preferences from file: " << settings_path << LL_ENDL;
		}
		else
		{
			bool save_settings = false;

			// add the elements from the XML file to the internal list of controls
			BOOST_FOREACH(const QuickPrefsXMLEntry& xml_entry, xml.entries)
			{
				// get the label
				std::string label = xml_entry.label;

				if (xml_entry.translation_id.isProvided())
				{
					// replace label with translated version, if available
					LLTrans::findString(label, xml_entry.translation_id);
				}

				// Convert old RenderAvatarMaxVisible setting to IndirectMaxNonImpostors
				if (xml_entry.control_name.getValue() != "RenderAvatarMaxVisible")
				{
					U32 type = xml_entry.control_type;
					addControl(
						xml_entry.control_name,
						label,
						NULL,
						(ControlType)type,
						xml_entry.integer,
						xml_entry.min_value,
						xml_entry.max_value,
						xml_entry.increment
						);

					// put it at the bottom of the ordering stack
					mControlsOrder.push_back(xml_entry.control_name);
				}
				else
				{
					U32 type = xml_entry.control_type;
					addControl(
						"IndirectMaxNonImpostors",
						label,
						NULL,
						(ControlType)type,
						xml_entry.integer,
						1,
						66,
						1
						);

					// put it at the bottom of the ordering stack
					mControlsOrder.push_back("IndirectMaxNonImpostors");

					save_settings = true;
				}
			}

			if (save_settings)
			{
				// Saves settings
				onEditModeChanged();
			}
		}
	}
}


bool FloaterQuickPrefs::isValidPresetName(const std::string& preset_name)
{
	return (!preset_name.empty() &&
		preset_name != PRESET_NAME_REGION_DEFAULT &&
		preset_name != PRESET_NAME_SKY_DAY_CYCLE &&
		preset_name != PRESET_NAME_NONE);
}

std::string FloaterQuickPrefs::stepComboBox(LLComboBox* ctrl, bool forward)
{
	S32 increment = (forward ? 1 : -1);
	S32 lastitem = ctrl->getItemCount() - 1;
	S32 curid = ctrl->getCurrentIndex();
	std::string preset_name;
	std::string start_preset_name;

	start_preset_name = ctrl->getSelectedValue().asString();
	do
	{
		curid += increment;
		if (curid < 0)
		{
			curid = lastitem;
		}
		else if (curid > lastitem)
		{
			curid = 0;
		}
		ctrl->setCurrentByIndex(curid);
		preset_name = ctrl->getSelectedValue().asString();
	}
	while (!isValidPresetName(preset_name) && preset_name != start_preset_name);

	return preset_name;
}

void FloaterQuickPrefs::selectSkyPreset(const std::string& preset_name)
{
	if (isValidPresetName(preset_name))
	{	
		deactivateAnimator();
		LLEnvManagerNew::instance().setUseSkyPreset(preset_name, (bool)gSavedSettings.getBOOL("FSInterpolateSky"));
	}
}

void FloaterQuickPrefs::selectWaterPreset(const std::string& preset_name)
{
	if (isValidPresetName(preset_name))
	{
		deactivateAnimator();
		LLEnvManagerNew::instance().setUseWaterPreset(preset_name, (bool)gSavedSettings.getBOOL("FSInterpolateWater"));
	}
}

void FloaterQuickPrefs::selectDayCyclePreset(const std::string& preset_name)
{
	if (isValidPresetName(preset_name))
	{
		deactivateAnimator();
		LLEnvManagerNew::instance().setUseDayCycle(preset_name, (bool)gSavedSettings.getBOOL("FSInterpolateSky"));
	}
}

void FloaterQuickPrefs::onChangeUseRegionWindlight()
{
	LLEnvManagerNew &envmgr = LLEnvManagerNew::instance();
	// reset all environmental settings to track the region defaults, make this reset 'sticky' like the other sun settings.
	bool use_fixed_sky = !mUseRegionWindlight->get();
	bool use_region_settings = !use_fixed_sky;
	envmgr.setUserPrefs(envmgr.getWaterPresetName(),
				envmgr.getSkyPresetName(),
				envmgr.getDayCycleName(),
				use_fixed_sky, use_region_settings,
				(gSavedSettings.getBOOL("FSInterpolateSky") || gSavedSettings.getBOOL("FSInterpolateWater")));
}

void FloaterQuickPrefs::onChangeWaterPreset()
{
	std::string preset_name = mWaterPresetsCombo->getSelectedValue().asString();
	if (!isValidPresetName(preset_name))
	{
		preset_name = stepComboBox(mWaterPresetsCombo, true);
	}
	selectWaterPreset(preset_name);
}

void FloaterQuickPrefs::onChangeSkyPreset()
{
	std::string preset_name = mWLPresetsCombo->getSelectedValue().asString();
	if (!isValidPresetName(preset_name))
	{
		preset_name = stepComboBox(mWLPresetsCombo, true);
	}
	selectSkyPreset(preset_name);
}

void FloaterQuickPrefs::onChangeDayCyclePreset()
{
	std::string preset_name = mDayCyclePresetsCombo->getSelectedValue().asString();
	if (!isValidPresetName(preset_name))
	{
		preset_name = stepComboBox(mDayCyclePresetsCombo, true);
	}
	selectDayCyclePreset(preset_name);
}

void FloaterQuickPrefs::deactivateAnimator()
{
	LLWLParamManager::instance().mAnimator.deactivate();
}

void FloaterQuickPrefs::onClickWaterPrev()
{
	std::string preset_name = stepComboBox(mWaterPresetsCombo, false);
	selectWaterPreset(preset_name);
}

void FloaterQuickPrefs::onClickWaterNext()
{
	std::string preset_name = stepComboBox(mWaterPresetsCombo, true);
	selectWaterPreset(preset_name);
}

void FloaterQuickPrefs::onClickSkyPrev()
{
	std::string preset_name = stepComboBox(mWLPresetsCombo, false);
	selectSkyPreset(preset_name);
}

void FloaterQuickPrefs::onClickSkyNext()
{
	std::string preset_name = stepComboBox(mWLPresetsCombo, true);
	selectSkyPreset(preset_name);
}

void FloaterQuickPrefs::onClickDayCyclePrev()
{
	std::string preset_name = stepComboBox(mDayCyclePresetsCombo, false);
	selectDayCyclePreset(preset_name);
}

void FloaterQuickPrefs::onClickDayCycleNext()
{
	std::string preset_name = stepComboBox(mDayCyclePresetsCombo, true);
	selectDayCyclePreset(preset_name);
}


void FloaterQuickPrefs::draw()
{
	F32 val;

	if (LLEnvManagerNew::instance().getUseRegionSettings() ||
		LLEnvManagerNew::instance().getUseDayCycle())
	{
		val = (F32)(LLWLParamManager::instance().mAnimator.getDayTime() * 24.f);
	}
	else
	{
		val = sun_pos_to_time24(LLWLParamManager::instance().mCurParams.getSunAngle() / F_TWO_PI);
	}

	mWLSunPos->setCurSliderValue(val);

	LLTransientDockableFloater::draw();
}

void FloaterQuickPrefs::onSunMoved()
{
	if (LLEnvManagerNew::instance().getUseRegionSettings() ||
		LLEnvManagerNew::instance().getUseDayCycle())
	{
		F32 val = mWLSunPos->getCurSliderValue() / 24.0f;

		LLWLParamManager& mgr = LLWLParamManager::instance();
		mgr.mAnimator.setDayTime((F64)val);
		mgr.mAnimator.deactivate();
		mgr.mAnimator.update(mgr.mCurParams);
	}
	else
	{
		deactivateAnimator();
		F32 val = time24_to_sun_pos(mWLSunPos->getCurSliderValue()) * F_TWO_PI;
		LLWLParamManager::instance().mCurParams.setSunAngle(val);
	}
}

void FloaterQuickPrefs::onClickResetToRegionDefault()
{
	deactivateAnimator();
	LLWLParamManager::instance().mAnimator.stopInterpolation();
	LLEnvManagerNew::instance().useRegionSettings();
}

// This method is invoked by LLEnvManagerNew when a particular preset is applied
// static
void FloaterQuickPrefs::updateParam(EQuickPrefUpdateParam param, const std::string& preset_name)
{
	FloaterQuickPrefs* qp_floater = LLFloaterReg::getTypedInstance<FloaterQuickPrefs>("quickprefs");
	FloaterQuickPrefs* pt_floater = LLFloaterReg::getTypedInstance<FloaterQuickPrefs>(PHOTOTOOLS_FLOATER);

	switch (param)
	{
		case QP_PARAM_SKY:
			qp_floater->setSelectedSky(preset_name);
			pt_floater->setSelectedSky(preset_name);
			break;
		case QP_PARAM_WATER:
			qp_floater->setSelectedWater(preset_name);
			pt_floater->setSelectedWater(preset_name);
			break;
		case QP_PARAM_DAYCYCLE:
			qp_floater->setSelectedDayCycle(preset_name);
			pt_floater->setSelectedDayCycle(preset_name);
			break;
		default:
			break;
	}
}

// static
void FloaterQuickPrefs::reloadPresetsAndSelect(EQuickPrefUpdateParam param)
{
	FloaterQuickPrefs* qp_floater = LLFloaterReg::getTypedInstance<FloaterQuickPrefs>("quickprefs");
	FloaterQuickPrefs* pt_floater = LLFloaterReg::getTypedInstance<FloaterQuickPrefs>(PHOTOTOOLS_FLOATER);

	std::string preset_name;

	switch (param)
	{
		case QP_PARAM_SKY:
			qp_floater->loadSkyPresets();
			pt_floater->loadSkyPresets();
			preset_name = LLEnvManagerNew::instance().getSkyPresetName();
			break;
		case QP_PARAM_WATER:
			qp_floater->loadWaterPresets();
			pt_floater->loadWaterPresets();
			preset_name = LLEnvManagerNew::instance().getWaterPresetName();
			break;
		case QP_PARAM_DAYCYCLE:
			qp_floater->loadDayCyclePresets();
			pt_floater->loadDayCyclePresets();
			preset_name = LLEnvManagerNew::instance().getDayCycleName();
			break;
		default:
			break;
	}

	updateParam(param, preset_name);
}

void FloaterQuickPrefs::setSelectedSky(const std::string& preset_name)
{
	mWLPresetsCombo->setValue(LLSD(preset_name));
	mDayCyclePresetsCombo->setValue(LLSD(PRESET_NAME_NONE));
}

void FloaterQuickPrefs::setSelectedWater(const std::string& preset_name)
{
	mWaterPresetsCombo->setValue(LLSD(preset_name));
}

void FloaterQuickPrefs::setSelectedDayCycle(const std::string& preset_name)
{
	mDayCyclePresetsCombo->setValue(LLSD(preset_name));
	mWLPresetsCombo->setValue(LLSD(PRESET_NAME_SKY_DAY_CYCLE));
}

// Phototools additions
void FloaterQuickPrefs::refreshSettings()
{
	BOOL reflections = gSavedSettings.getBOOL("VertexShaderEnable") 
		&& gGLManager.mHasCubeMap
		&& LLCubeMap::sUseCubeMaps;
	mCtrlReflectionDetail->setEnabled(reflections);

	bool fCtrlShaderEnable = LLFeatureManager::getInstance()->isFeatureAvailable("VertexShaderEnable");
	mCtrlShaderEnable->setEnabled(
		fCtrlShaderEnable && ((!gRlvHandler.hasBehaviour(RLV_BHVR_SETENV)) || (!gSavedSettings.getBOOL("VertexShaderEnable"))) );
	BOOL shaders = mCtrlShaderEnable->get();

	bool fCtrlWindLightEnable = fCtrlShaderEnable && shaders;
	mCtrlWindLight->setEnabled(
		fCtrlWindLightEnable && ((!gRlvHandler.hasBehaviour(RLV_BHVR_SETENV)) || (!gSavedSettings.getBOOL("WindLightUseAtmosShaders"))) );

	LLTextBox* sky_label = getChild<LLTextBox>("T_Sky_Detail");
	LLSlider* sky_slider = getChild<LLSlider>("SB_Sky_Detail");
	LLSpinCtrl* sky_spinner = getChild<LLSpinCtrl>("S_Sky_Detail");
	LLButton* sky_default_button = getChild<LLButton>("Reset_Sky_Detail");

	BOOL sky_enabled = mCtrlWindLight->get() && shaders;
	sky_label->setEnabled(sky_enabled);
	sky_slider->setEnabled(sky_enabled);
	sky_spinner->setEnabled(sky_enabled);
	sky_default_button->setEnabled(sky_enabled);

	BOOL enabled = LLFeatureManager::getInstance()->isFeatureAvailable("RenderDeferred") && 
						shaders && 
						gGLManager.mHasFramebufferObject &&
						gSavedSettings.getBOOL("RenderAvatarVP") &&
						(mCtrlWindLight->get()) ? TRUE : FALSE;

	mCtrlDeferred->setEnabled(enabled);

	enabled = enabled && LLFeatureManager::getInstance()->isFeatureAvailable("RenderDeferredSSAO") && (mCtrlDeferred->get() ? TRUE : FALSE);
		
	mCtrlUseSSAO->setEnabled(enabled);
	mCtrlUseDoF->setEnabled(enabled);

	enabled = enabled && LLFeatureManager::getInstance()->isFeatureAvailable("RenderShadowDetail");

	mCtrlShadowDetail->setEnabled(enabled);

	mCtrlAvatarShadowDetail->setEnabled(enabled && mCtrlShadowDetail->getValue().asInteger() > 0);

	// if vertex shaders off, disable all shader related products
	if (!LLFeatureManager::getInstance()->isFeatureAvailable("VertexShaderEnable"))
	{
		mCtrlShaderEnable->setEnabled(FALSE);
		mCtrlShaderEnable->setValue(FALSE);
		
		mCtrlWindLight->setEnabled(FALSE);
		mCtrlWindLight->setValue(FALSE);

		sky_label->setEnabled(FALSE);
		sky_slider->setEnabled(FALSE);
		sky_spinner->setEnabled(FALSE);
		sky_default_button->setEnabled(FALSE);
		
		mCtrlReflectionDetail->setEnabled(FALSE);
		mCtrlReflectionDetail->setValue(0);

		mCtrlShadowDetail->setEnabled(FALSE);
		mCtrlShadowDetail->setValue(0);

		mCtrlAvatarShadowDetail->setEnabled(FALSE);
		mCtrlAvatarShadowDetail->setValue(0);
		
		mCtrlUseSSAO->setEnabled(FALSE);
		mCtrlUseSSAO->setValue(FALSE);

		mCtrlUseDoF->setEnabled(FALSE);
		mCtrlUseDoF->setValue(FALSE);

		mCtrlDeferred->setEnabled(FALSE);
		mCtrlDeferred->setValue(FALSE);
	}
	
	// disabled windlight
	if (!LLFeatureManager::getInstance()->isFeatureAvailable("WindLightUseAtmosShaders"))
	{
		mCtrlWindLight->setEnabled(FALSE);
		mCtrlWindLight->setValue(FALSE);

		sky_label->setEnabled(FALSE);
		sky_slider->setEnabled(FALSE);
		sky_spinner->setEnabled(FALSE);
		sky_default_button->setEnabled(FALSE);

		//deferred needs windlight, disable deferred
		mCtrlShadowDetail->setEnabled(FALSE);
		mCtrlShadowDetail->setValue(0);
		
		mCtrlAvatarShadowDetail->setEnabled(FALSE);
		mCtrlAvatarShadowDetail->setValue(0);

		mCtrlUseSSAO->setEnabled(FALSE);
		mCtrlUseSSAO->setValue(FALSE);

		mCtrlUseDoF->setEnabled(FALSE);
		mCtrlUseDoF->setValue(FALSE);

		mCtrlDeferred->setEnabled(FALSE);
		mCtrlDeferred->setValue(FALSE);
	}

	// disabled deferred
	if (!LLFeatureManager::getInstance()->isFeatureAvailable("RenderDeferred") ||
		!gGLManager.mHasFramebufferObject)
	{
		mCtrlShadowDetail->setEnabled(FALSE);
		mCtrlShadowDetail->setValue(0);

		mCtrlAvatarShadowDetail->setEnabled(FALSE);
		mCtrlAvatarShadowDetail->setValue(0);

		mCtrlUseSSAO->setEnabled(FALSE);
		mCtrlUseSSAO->setValue(FALSE);

		mCtrlUseDoF->setEnabled(FALSE);
		mCtrlUseDoF->setValue(FALSE);

		mCtrlDeferred->setEnabled(FALSE);
		mCtrlDeferred->setValue(FALSE);
	}
	
	// disabled deferred SSAO
	if (!LLFeatureManager::getInstance()->isFeatureAvailable("RenderDeferredSSAO"))
	{
		mCtrlUseSSAO->setEnabled(FALSE);
		mCtrlUseSSAO->setValue(FALSE);
	}
	
	// disabled deferred shadows
	if (!LLFeatureManager::getInstance()->isFeatureAvailable("RenderShadowDetail"))
	{
		mCtrlShadowDetail->setEnabled(FALSE);
		mCtrlShadowDetail->setValue(0);

		mCtrlAvatarShadowDetail->setEnabled(FALSE);
		mCtrlAvatarShadowDetail->setValue(0);
	}

	// disabled reflections
	if (!LLFeatureManager::getInstance()->isFeatureAvailable("RenderReflectionDetail"))
	{
		mCtrlReflectionDetail->setEnabled(FALSE);
		mCtrlReflectionDetail->setValue(FALSE);
	}

	// disabled av
	if (!LLFeatureManager::getInstance()->isFeatureAvailable("RenderAvatarVP"))
	{
		//deferred needs AvatarVP, disable deferred
		mCtrlShadowDetail->setEnabled(FALSE);
		mCtrlShadowDetail->setValue(0);

		mCtrlAvatarShadowDetail->setEnabled(FALSE);
		mCtrlAvatarShadowDetail->setValue(0);

		
		mCtrlUseSSAO->setEnabled(FALSE);
		mCtrlUseSSAO->setValue(FALSE);

		mCtrlUseDoF->setEnabled(FALSE);
		mCtrlUseDoF->setValue(FALSE);

		mCtrlDeferred->setEnabled(FALSE);
		mCtrlDeferred->setValue(FALSE);
	}
	
	// <FS:CR> FIRE-9630 - Vignette UI controls
	if (getIsPhototools())
	{
		LLVector3 vignette = gSavedSettings.getVector3("FSRenderVignette");
		mSpinnerVignetteX->setValue(vignette.mV[VX]);
		mSpinnerVignetteY->setValue(vignette.mV[VY]);
		mSpinnerVignetteZ->setValue(vignette.mV[VZ]);
		mSliderVignetteX->setValue(vignette.mV[VX]);
		mSliderVignetteY->setValue(vignette.mV[VY]);
		mSliderVignetteZ->setValue(vignette.mV[VZ]);

		LLVector3 renderShadowSplitExponent = gSavedSettings.getVector3("RenderShadowSplitExponent");
		mSpinnerRenderShadowSplitExponentY->setValue(renderShadowSplitExponent.mV[VY]);
		mSliderRenderShadowSplitExponentY->setValue(renderShadowSplitExponent.mV[VY]);

		LLVector3 renderRenderShadowGaussian = gSavedSettings.getVector3("RenderShadowGaussian");
		mSpinnerRenderShadowGaussianX->setValue(renderRenderShadowGaussian.mV[VX]);
		mSpinnerRenderShadowGaussianY->setValue(renderRenderShadowGaussian.mV[VY]);
		mSliderRenderShadowGaussianX->setValue(renderRenderShadowGaussian.mV[VX]);
		mSliderRenderShadowGaussianY->setValue(renderRenderShadowGaussian.mV[VY]);

		LLVector3 renderSSAOEffect = gSavedSettings.getVector3("RenderSSAOEffect");
		mSpinnerRenderSSAOEffectX->setValue(renderSSAOEffect.mV[VX]);
		mSliderRenderSSAOEffectX->setValue(renderSSAOEffect.mV[VX]);
	}
	// </FS:CR>
}

void FloaterQuickPrefs::updateRlvRestrictions(ERlvBehaviour behavior, ERlvParamType type)
{
	if (behavior == RLV_BHVR_SETENV)
	{
		if (type == RLV_TYPE_ADD)
		{
			enableWindlightButtons(FALSE);
		}
		else
		{
			enableWindlightButtons(TRUE);
		}
	}
}

void FloaterQuickPrefs::enableWindlightButtons(BOOL enable)
{
	childSetEnabled("WLPresetsCombo", enable);
	childSetEnabled("WLPrevPreset", enable);
	childSetEnabled("WLNextPreset", enable);
	childSetEnabled("WaterPresetsCombo", enable);
	childSetEnabled("WWPrevPreset", enable);
	childSetEnabled("WWNextPreset", enable);
	childSetEnabled("ResetToRegionDefault", enable);
	childSetEnabled("UseRegionWindlight", enable);
	childSetEnabled("DCPresetsCombo", enable);
	childSetEnabled("DCPrevPreset", enable);
	childSetEnabled("DCNextPreset", enable);
	//<FS:TS> FIRE-13448: Quickprefs daycycle slider allows evading @setenv=n
	childSetEnabled("WLSunPos", enable);
	//</FS:TS> FIRE-13448

	if (getIsPhototools())
	{
		childSetEnabled("Sunrise", enable);
		childSetEnabled("Noon", enable);
		childSetEnabled("Sunset", enable);
		childSetEnabled("Midnight", enable);
		childSetEnabled("Revert to Region Default", enable);
		childSetEnabled("new_sky_preset", enable);
		childSetEnabled("edit_sky_preset", enable);
		childSetEnabled("new_water_preset", enable);
		childSetEnabled("edit_water_preset", enable);
	}
}

// <FS:Zi> Dynamic quick prefs
std::string FloaterQuickPrefs::getSettingsPath(bool save_mode)
{
	// get the settings file name
	std::string settings_file = LLAppViewer::instance()->getSettingsFilename("Default", "QuickPreferences");
	// expand to user defined path
	std::string settings_path = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, settings_file);

	// if not in save mode, and the file was not found, use the default path
	if (!save_mode && !LLFile::isfile(settings_path))
	{
		settings_path = gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, settings_file);
	}
	return settings_path;
}

void FloaterQuickPrefs::updateControl(const std::string& controlName, ControlEntry& entry)
{
	// rename the panel to contain the control's name, for identification later
	entry.panel->setName(controlName);

	// build a list of all possible control widget types
	std::map<ControlType, std::string> typeMap;
	std::map<ControlType, std::string>::iterator it;

	typeMap[ControlTypeCheckbox]	= "option_checkbox_control";
	typeMap[ControlTypeText]		= "option_text_control";
	typeMap[ControlTypeSpinner]		= "option_spinner_control";
	typeMap[ControlTypeSlider]		= "option_slider_control";
	typeMap[ControlTypeRadio]		= "option_radio_control";
	typeMap[ControlTypeColor3]		= "option_color3_control";
	typeMap[ControlTypeColor4]		= "option_color4_control";

	// hide all widget types except for the one the user wants
	LLUICtrl* widget;
	for (it = typeMap.begin(); it != typeMap.end(); ++it)
	{
		if (entry.type != it->first)
		{
			widget = entry.panel->getChild<LLUICtrl>(it->second);

			if (widget)
			{
				// dummy to disable old control
				widget->setControlName("QuickPrefsEditMode");
				widget->setVisible(FALSE);
				widget->setEnabled(FALSE);
			}
		}
	}

	// get the widget type the user wanted from the panel
	widget = entry.panel->getChild<LLUICtrl>(typeMap[entry.type]);

	// use 3 decimal places by default
	S32 decimals = 3;

	// save pointer to the widget in our internal list
	entry.widget = widget;

	// add the settings control to the widget and enable/show it
	widget->setControlName(controlName);
	widget->setVisible(TRUE);
	widget->setEnabled(TRUE);

	// if no increment is given, try to guess a good number
	if (entry.increment == 0.0f)
	{
		// finer grained for sliders
		if (entry.type == ControlTypeSlider)
		{
			entry.increment = (entry.max_value - entry.min_value) / 100.0f;
		}
		// a little less for spinners
		else if (entry.type == ControlTypeSpinner)
		{
			entry.increment = (entry.max_value - entry.min_value) / 20.0f;
		}
	}

	// if it's an integer entry, round the numbers
	if (entry.integer)
	{
		entry.min_value = ll_round(entry.min_value);
		entry.max_value = ll_round(entry.max_value);

		// recalculate increment
		entry.increment = ll_round(entry.increment);
		if (entry.increment == 0.f)
		{
			entry.increment = 1.f;
		}

		// no decimal places for integers
		decimals = 0;
	}

	// set up values for special case control widget types
	LLUICtrl* alpha_widget = entry.panel->getChild<LLUICtrl>("option_color_alpha_control");
	alpha_widget->setVisible(FALSE);

	// sadly, using LLF32UICtrl does not work properly, so we have to use a branch
	// for each floating point type
	if (entry.type == ControlTypeSpinner)
	{
		LLSpinCtrl* spinner = (LLSpinCtrl*)widget;
		spinner->setPrecision(decimals);
		spinner->setMinValue(entry.min_value);
		spinner->setMaxValue(entry.max_value);
		spinner->setIncrement(entry.increment);
	}
	else if (entry.type == ControlTypeSlider)
	{
		LLSliderCtrl* slider = (LLSliderCtrl*)widget;
		slider->setPrecision(decimals);
		slider->setMinValue(entry.min_value);
		slider->setMaxValue(entry.max_value);
		slider->setIncrement(entry.increment);
	}
	else if (entry.type == ControlTypeColor4)
	{
		LLColorSwatchCtrl* color_widget = (LLColorSwatchCtrl*)widget;
		alpha_widget->setVisible(TRUE);
		alpha_widget->setValue(color_widget->get().mV[VALPHA]);
	}

	// reuse a previously created text label if possible
	LLTextBox* label_textbox = entry.label_textbox;
	// if the text label is not known yet, this is a brand new control panel
	if (!label_textbox)
	{
		// otherwise, get the pointer to the new label
		label_textbox = entry.panel->getChild<LLTextBox>("option_label");

		// add double click and single click callbacks on the text label
		label_textbox->setDoubleClickCallback(boost::bind(&FloaterQuickPrefs::onDoubleClickLabel, this, _1, entry.panel));
		label_textbox->setMouseUpCallback(boost::bind(&FloaterQuickPrefs::onClickLabel, this, _1, entry.panel));

		// since this is a new control, wire up the remove button signal, too
		LLButton* remove_button = entry.panel->getChild<LLButton>("remove_button");
		remove_button->setCommitCallback(boost::bind(&FloaterQuickPrefs::onRemoveClicked, this, _1, entry.panel));

		// and the commit signal for the alpha value in a color4 control
		alpha_widget->setCommitCallback(boost::bind(&FloaterQuickPrefs::onAlphaChanged, this, _1, widget));

		// save the text label pointer in the internal list
		entry.label_textbox = label_textbox;
	}
	// set the value(visible text) for the text label
	label_textbox->setValue(entry.label + ":");

	// get the named control variable from global or per account settings
	LLControlVariable* var = gSavedSettings.getControl(controlName);
	if (!var)
	{
		var = gSavedPerAccountSettings.getControl(controlName);
	}

	// if we found the control, set up the chosen widget to use it
	if (var)
	{
		widget->setValue(var->getValue());
		widget->setToolTip(var->getComment());
		label_textbox->setToolTip(var->getComment());
	}
	else
	{
		LL_WARNS() << "Could not find control variable " << controlName << LL_ENDL;
	}
}

LLUICtrl* FloaterQuickPrefs::addControl(const std::string& controlName, const std::string& controlLabel, LLView* slot, ControlType type, BOOL integer, F32 min_value, F32 max_value, F32 increment)
{
	// create a new controls panel
	LLLayoutPanel* panel = LLUICtrlFactory::createFromFile<LLLayoutPanel>("panel_quickprefs_item.xml", NULL, LLLayoutStack::child_registry_t::instance());
	if (!panel)
	{
		LL_WARNS() << "could not add panel" << LL_ENDL;
		return NULL;
	}

	// sanity checks
	if (max_value < min_value)
	{
		max_value = min_value;
	}

	// 0.0 will make updateControl calculate the increment itself
	if (increment < 0.0f)
	{
		increment = 0.0f;
	}

	// create a new internal entry for this control
	ControlEntry newControl;
	newControl.panel = panel->getChild<LLPanel>("option_ordering_panel");
	newControl.widget = NULL;
	newControl.label_textbox = NULL;
	newControl.label = controlLabel;
	newControl.type = type;
	newControl.integer = integer;
	newControl.min_value = min_value;
	newControl.max_value = max_value;
	newControl.increment = increment;

	// update the new control
	updateControl(controlName, newControl);

	// add the control to the internal list
	mControlsList[controlName] = newControl;

	// if we have a slot already, reparent our new ordering panel and delete the old layout_panel
	if (slot)
	{
		// add the ordering panel to the slot
		slot->addChild(newControl.panel);
		// make sure the panel moves to the top left corner
		newControl.panel->setOrigin(0, 0);
		// resize it to make it fill the slot
		newControl.panel->reshape(slot->getRect().getWidth(), slot->getRect().getHeight());
		// remove the old layout panel from memory
		delete panel;
	}
	// otherwise create a new slot
	else
	{
		// add a new layout_panel to the stack
		mOptionsStack->addPanel(panel, LLLayoutStack::NO_ANIMATE);
		// add the panel to the list of ordering slots
		mOrderingSlots.push_back(panel);
		// make the floater fit the newly added control panel
		reshape(getRect().getWidth(), getRect().getHeight() + panel->getRect().getHeight());
		// show the panel
		panel->setVisible(TRUE);
	}

	// hide the border
	newControl.panel->setBorderVisible(FALSE);

	return newControl.widget;
}

void FloaterQuickPrefs::removeControl(const std::string& controlName, bool remove_slot)
{
	// find the control panel to remove
	const control_list_t::iterator it = mControlsList.find(controlName);
	if (it == mControlsList.end())
	{
		LL_WARNS() << "Couldn't find control entry " << controlName << LL_ENDL;
		return;
	}

	// get a pointer to the panel to remove
	LLPanel* panel = it->second.panel;
	// remember the panel's height because it will be deleted by removeChild() later
	S32 height = panel->getRect().getHeight();

	// remove the panel from the internal list
	mControlsList.erase(it);

	// get a pointer to the layout slot used
	LLLayoutPanel* slot = (LLLayoutPanel*)panel->getParent();
	// remove the panel from the slot
	slot->removeChild(panel);
	// clear the panel from memory
	delete panel;

	// remove the layout_panel if desired
	if (remove_slot)
	{
		// remove the slot from our list
		mOrderingSlots.remove(slot);
		// and delete it from the user interface stack
		mOptionsStack->removeChild(slot);

		// make the floater shrink to its new size
		reshape(getRect().getWidth(), getRect().getHeight() - height);
	}
}

void FloaterQuickPrefs::selectControl(std::string controlName)
{
	// remove previously selected marker, if any
	if (!mSelectedControl.empty() && hasControl(mSelectedControl))
	{
		mControlsList[mSelectedControl].panel->setBorderVisible(FALSE);
	}

	// save the currently selected name in a volatile settings control to
	// enable/disable the editor widgets
	mSelectedControl = controlName;
	gSavedSettings.setString("QuickPrefsSelectedControl", controlName);

	if (mSelectedControl.size() && !hasControl(mSelectedControl))
	{
		mSelectedControl = "";
		return;
	}

	// if we are not in edit mode, we can stop here
	if (!gSavedSettings.getBOOL("QuickPrefsEditMode"))
	{
		return;
	}

	// select the topmost entry in the name dropdown, in case we don't find the name
	mControlNameCombo->selectNthItem(0);

	// assume we don't need the min/max/increment/integer widgets by default
	BOOL enable_floating_point = FALSE;

	// if actually a selection is present, set up the editor widgets
	if (!mSelectedControl.empty())
	{
		// draw the new selection border
		mControlsList[mSelectedControl].panel->setBorderVisible(TRUE);

		// set up editor values
		mControlLabelEdit->setValue(LLSD(mControlsList[mSelectedControl].label));
		mControlNameCombo->setValue(LLSD(mSelectedControl));
		mControlTypeCombo->setValue(mControlsList[mSelectedControl].type);
		mControlIntegerCheckbox->setValue(LLSD(mControlsList[mSelectedControl].integer));
		mControlMinSpinner->setValue(LLSD(mControlsList[mSelectedControl].min_value));
		mControlMaxSpinner->setValue(LLSD(mControlsList[mSelectedControl].max_value));
		mControlIncrementSpinner->setValue(LLSD(mControlsList[mSelectedControl].increment));

		// special handling to enable min/max/integer/increment widgets
		switch (mControlsList[mSelectedControl].type)
		{
			// enable floating point widgets for these types
			case ControlTypeSpinner:	// fall through
			case ControlTypeSlider:		// fall through
			{
				enable_floating_point = TRUE;

				// assume we have floating point widgets
				mControlIncrementSpinner->setIncrement(0.1f);
				// use 3 decimal places by default
				S32 decimals = 3;
				// unless we have an integer control
				if (mControlsList[mSelectedControl].integer)
				{
					decimals = 0;
					mControlIncrementSpinner->setIncrement(1.0f);
				}
				// set up floating point widgets
				mControlMinSpinner->setPrecision(decimals);
				mControlMaxSpinner->setPrecision(decimals);
				mControlIncrementSpinner->setPrecision(decimals);
				break;
			}
			// the rest will not need them
			default:
			{
			}
		}
	}

	// enable/disable floating point widgets
	mControlMinSpinner->setEnabled(enable_floating_point);
	mControlMaxSpinner->setEnabled(enable_floating_point);
	mControlIntegerCheckbox->setEnabled(enable_floating_point);
	mControlIncrementSpinner->setEnabled(enable_floating_point);
}

void FloaterQuickPrefs::onClickLabel(LLUICtrl* ctrl, void* userdata)
{
	// don't do anything when we are not in edit mode
	if (!gSavedSettings.getBOOL("QuickPrefsEditMode"))
	{
		return;
	}
	// get the associated panel from the submitted userdata
	LLUICtrl* panel = (LLUICtrl*)userdata;
	// select the clicked control, identified by its name
	selectControl(panel->getName());
}

void FloaterQuickPrefs::onDoubleClickLabel(LLUICtrl* ctrl, void* userdata)
{
	// toggle edit mode
	BOOL edit_mode = !gSavedSettings.getBOOL("QuickPrefsEditMode");
	gSavedSettings.setBOOL("QuickPrefsEditMode", edit_mode);

	// select the double clicked control if we toggled edit on
	if (edit_mode)
	{
		// get the associated widget from the submitted userdata
		LLUICtrl* panel = (LLUICtrl*)userdata;
		selectControl(panel->getName());
	}
}

void FloaterQuickPrefs::onEditModeChanged()
{
	// if edit mode was enabled, stop here
	if (gSavedSettings.getBOOL("QuickPrefsEditMode"))
	{
		return;
	}

	// deselect the current control
	selectControl("");

	QuickPrefsXML xml;
	std::string settings_path = getSettingsPath(true);

	// loop through the list of controls, in the displayed order
	std::list<std::string>::iterator it;
	for (it = mControlsOrder.begin(); it != mControlsOrder.end(); ++it)
	{
		const ControlEntry& entry = mControlsList[*it];
		QuickPrefsXMLEntry xml_entry;

		// add control values to the XML entry
		xml_entry.control_name = *it;
		xml_entry.label = entry.label;
		xml_entry.control_type = (U32)entry.type;
		xml_entry.integer = entry.integer;
		xml_entry.min_value = entry.min_value;
		xml_entry.max_value = entry.max_value;
		xml_entry.increment = entry.increment;

		// add the XML entry to the overall XML container
		xml.entries.add(xml_entry);
	}

	// Serialize the parameter tree
	LLXMLNodePtr output_node = new LLXMLNode("quickprefs", false);
	LLXUIParser parser;
	parser.writeXUI(output_node, xml);

	// Write the resulting XML to file
	if (!output_node->isNull())
	{
		LLFILE* fp = LLFile::fopen(settings_path, "w");
		if (fp)
		{
			LLXMLNode::writeHeaderToFile(fp);
			output_node->writeToFile(fp);
			fclose(fp);
		}
	}
}

void FloaterQuickPrefs::onValuesChanged()
{
	// safety, do nothing if we are not in edit mode
	if (!gSavedSettings.getBOOL("QuickPrefsEditMode"))
	{
		return;
	}

	// don't crash when we try to update values without having a control selected
	if (mSelectedControl.empty())
	{
		return;
	}

	// remember the current and possibly new control names
	std::string old_control_name = mSelectedControl;
	std::string new_control_name = mControlNameCombo->getValue().asString();

	// if we changed the control's variable, rebuild the user interface
	if (!new_control_name.empty() && old_control_name != new_control_name)
	{
		if (mControlsList.find(new_control_name) != mControlsList.end())
		{
			LL_WARNS() << "Selected control has already been added" << LL_ENDL;
			LLNotificationsUtil::add("QuickPrefsDuplicateControl");
			return;
		}

		// remember the old control parameters so we can restore them later
		ControlEntry old_parameters = mControlsList[mSelectedControl];
		// disable selection so the border doesn't cause a crash
		selectControl("");
		// rename the old ordering entry
		std::list<std::string>::iterator it;
		for (it = mControlsOrder.begin(); it != mControlsOrder.end(); ++it)
		{
			if (*it == old_control_name)
			{
				*it = new_control_name;
				break;
			}
		}

		// remember the old slot
		LLView* slot = old_parameters.panel->getParent();
		// remove the old control name from the internal list but keep the slot available
		removeControl(old_control_name, false);
		// add new control with the old slot
		addControl(new_control_name, new_control_name, slot);
		// grab the new values and make the selection border go to the right panel
		selectControl(new_control_name);
		// restore the old UI settings
		mControlsList[mSelectedControl].label = old_parameters.label;
		// find the control variable in global or per account settings
		LLControlVariable* var = gSavedSettings.getControl(mSelectedControl);
		if (!var)
		{
			var = gSavedPerAccountSettings.getControl(mSelectedControl);
		}

		if (var && hasControl(mSelectedControl))
		{
			// choose sane defaults for floating point controls, so the control value won't be destroyed
			// start with these
			F32 min_value = 0.0f;
			F32 max_value = 1.0f;
			F32 value = var->getValue().asReal();

			// if the value was negative and smaller than the current minimum
			if (value < 0.0f)
			{
				// make the minimum even smaller
				min_value = value * 2.0f;
			}
			// if the value is above zero, set max to double of the current value
			else if (value > 0.0f)
			{
				max_value = value * 2.0f;
			}

			// do a best guess on variable types and control widgets
			ControlType type;
			switch (var->type())
			{
				// Boolean gets the full set
				case TYPE_BOOLEAN:
				{
					// increment will be calculated below
					min_value = 0.0f;
					max_value = 1.0f;
					type = ControlTypeRadio;
					break;
				}
				// LLColor3/4 are just colors
				case TYPE_COL3:
				{
					type = ControlTypeColor3;
					break;
				}
				case TYPE_COL4:
				{
					type = ControlTypeColor4;
					break;
				}
				// U32 can never be negative
				case TYPE_U32:
				{
					min_value = 0.0f;
				}
				// Fallthrough, S32 and U32 are integer values
				case TYPE_S32:
				// Fallthrough, S32, U32 and F32 should use sliders
				case TYPE_F32:
				{
					type = ControlTypeSlider;
					break;
				}
				// Everything else gets a text widget for now
				default:
				{
					type=ControlTypeText;
				}
			}

			// choose a sane increment
			F32 increment = 0.1f;
			if (mControlsList[mSelectedControl].type == ControlTypeSlider)
			{
				// fine grained control for sliders
				increment = (max_value - min_value) / 100.0f;
			}
			else if (mControlsList[mSelectedControl].type == ControlTypeSpinner)
			{
				// not as fine grained for spinners
				increment = (max_value - min_value) / 20.0f;
			}

			// don't let values go too small
			if (increment < 0.1f)
			{
				increment = 0.1f;
			}

			// save calculated values to the edit widgets
			mControlsList[mSelectedControl].min_value = min_value;
			mControlsList[mSelectedControl].max_value = max_value;
			mControlsList[mSelectedControl].increment = increment;
			mControlsList[mSelectedControl].type = type; // old_parameters.type;
			mControlsList[mSelectedControl].widget->setValue(var->getValue());
		}
		// rebuild controls UI (probably not needed)
		// updateControls();
		// update our new control
		updateControl(mSelectedControl, mControlsList[mSelectedControl]);
	}
	// the control's setting variable is still the same, so just update the values
	else if (hasControl(mSelectedControl))
	{
		mControlsList[mSelectedControl].label = mControlLabelEdit->getValue().asString();
		mControlsList[mSelectedControl].type = (ControlType)mControlTypeCombo->getValue().asInteger();
		mControlsList[mSelectedControl].integer = mControlIntegerCheckbox->getValue().asBoolean();
		mControlsList[mSelectedControl].min_value = mControlMinSpinner->getValue().asReal();
		mControlsList[mSelectedControl].max_value = mControlMaxSpinner->getValue().asReal();
		mControlsList[mSelectedControl].increment = mControlIncrementSpinner->getValue().asReal();
		// and update the user interface
		updateControl(mSelectedControl, mControlsList[mSelectedControl]);
	}
	// select the control
	selectControl(mSelectedControl);
}

void FloaterQuickPrefs::onAddNewClicked()
{
	// count a number to keep control names unique
	static S32 sCount = 0;
	std::string new_control_name = "NewControl" + llformat("%d", sCount);
	// add the new control to the internal list and user interface
	addControl(new_control_name, new_control_name);
	// put it at the bottom of the ordering stack
	mControlsOrder.push_back(new_control_name);
	sCount++;
	// select the newly created control
	selectControl(new_control_name);
}

void FloaterQuickPrefs::onRemoveClicked(LLUICtrl* ctrl, void* userdata)
{
	// get the associated panel from the submitted userdata
	LLUICtrl* panel = (LLUICtrl*)userdata;
	// deselect the current entry
	selectControl("");
	// first remove the control from the ordering list
	mControlsOrder.remove(panel->getName());
	// then remove it from the internal list and from memory
	removeControl(panel->getName());
	// reinstate focus in case we lost it
	setFocus(TRUE);
}

void FloaterQuickPrefs::onAlphaChanged(LLUICtrl* ctrl, void* userdata)
{
	// get the associated color swatch from the submitted userdata
	LLColorSwatchCtrl* color_swatch = (LLColorSwatchCtrl*)userdata;
	// get the current color
	LLColor4 color = color_swatch->get();
	// replace the alpha value of the color with the value in the alpha spinner
	color.setAlpha(ctrl->getValue().asReal());
	// save the color back into the color swatch
	color_swatch->getControlVariable()->setValue(color.getValue());
}

void FloaterQuickPrefs::swapControls(const std::string& control1, const std::string& control2)
{
	// get the control entries of both controls
	ControlEntry temp_entry_1 = mControlsList[control1];
	ControlEntry temp_entry_2 = mControlsList[control2];

	// find the respective ordering slots
	LLView* temp_slot_1 = temp_entry_1.panel->getParent();
	LLView* temp_slot_2 = temp_entry_2.panel->getParent();

	// swap the controls around
	temp_slot_1->addChild(temp_entry_2.panel);
	temp_slot_2->addChild(temp_entry_1.panel);
}

void FloaterQuickPrefs::onMoveUpClicked()
{
	// find the control in the ordering list
	std::list<std::string>::iterator it;
	for (it = mControlsOrder.begin(); it != mControlsOrder.end(); ++it)
	{
		if (*it == mSelectedControl)
		{
			// if it's already on top of the list, do nothing
			if (it == mControlsOrder.begin())
			{
				return;
			}

			// get the iterator of the previous item
			std::list<std::string>::iterator previous = it;
			--previous;

			// copy the previous item to the one we want to move
			*it = *previous;
			// copy the moving item to previous
			*previous = mSelectedControl;
			// update the user interface
			swapControls(mSelectedControl, *it);
			return;
		}
	}
	return;
}

void FloaterQuickPrefs::onMoveDownClicked()
{
	// find the control in the ordering list
	std::list<std::string>::iterator it;
	for (it = mControlsOrder.begin(); it != mControlsOrder.end(); ++it)
	{
		if (*it == mSelectedControl)
		{
			// if it's already at the end of the list, do nothing
			if (*it == mControlsOrder.back())
			{
				return;
			}

			// get the iterator of the next item
			std::list<std::string>::iterator next = it;
			++next;

			// copy the next item to the one we want to move
			*it = *next;
			// copy the moving item to next
			*next = mSelectedControl;
			// update the user interface
			swapControls(mSelectedControl, *it);
			return;
		}
	}
	return;
}

void FloaterQuickPrefs::onClose(bool app_quitting)
{
	// bail out here if this is a reused Phototools floater
	if (getIsPhototools())
	{
		return;
	}

	// close edit mode and save settings
	gSavedSettings.setBOOL("QuickPrefsEditMode", FALSE);
}
// </FS:Zi>

// <FS:CR> FIRE-9630 - Vignette UI callbacks
void FloaterQuickPrefs::onChangeVignetteX()
{
	LLVector3 vignette = gSavedSettings.getVector3("FSRenderVignette");
	vignette.mV[VX] = mSliderVignetteX->getValueF32();
	mSpinnerVignetteX->setValue(vignette.mV[VX]);
	gSavedSettings.setVector3("FSRenderVignette", vignette);
}

void FloaterQuickPrefs::onChangeVignetteY()
{
	LLVector3 vignette = gSavedSettings.getVector3("FSRenderVignette");
	vignette.mV[VY] = mSliderVignetteY->getValueF32();
	mSpinnerVignetteY->setValue(vignette.mV[VY]);
	gSavedSettings.setVector3("FSRenderVignette", vignette);
}

void FloaterQuickPrefs::onChangeVignetteZ()
{
	LLVector3 vignette = gSavedSettings.getVector3("FSRenderVignette");
	vignette.mV[VZ] = mSliderVignetteZ->getValueF32();
	mSpinnerVignetteZ->setValue(vignette.mV[VZ]);
	gSavedSettings.setVector3("FSRenderVignette", vignette);
}

void FloaterQuickPrefs::onChangeVignetteSpinnerX()
{
	LLVector3 vignette = gSavedSettings.getVector3("FSRenderVignette");
	vignette.mV[VX] = mSpinnerVignetteX->getValueF32();
	mSliderVignetteX->setValue(vignette.mV[VX]);
	gSavedSettings.setVector3("FSRenderVignette", vignette);
}

void FloaterQuickPrefs::onChangeVignetteSpinnerY()
{
	LLVector3 vignette = gSavedSettings.getVector3("FSRenderVignette");
	vignette.mV[VY] = mSpinnerVignetteY->getValueF32();
	mSliderVignetteY->setValue(vignette.mV[VY]);
	gSavedSettings.setVector3("FSRenderVignette", vignette);
}

void FloaterQuickPrefs::onChangeVignetteSpinnerZ()
{
	LLVector3 vignette = gSavedSettings.getVector3("FSRenderVignette");
	vignette.mV[VZ] = mSpinnerVignetteZ->getValueF32();
	mSliderVignetteZ->setValue(vignette.mV[VZ]);
	gSavedSettings.setVector3("FSRenderVignette", vignette);
}

void FloaterQuickPrefs::onClickResetVignetteX()
{
	LLVector3 vignette_default = LLVector3(gSavedSettings.getControl("FSRenderVignette")->getDefault());
	LLVector3 vignette = gSavedSettings.getVector3("FSRenderVignette");
	vignette.mV[VX] = vignette_default.mV[VX];
	mSliderVignetteX->setValue(vignette.mV[VX]);
	mSpinnerVignetteX->setValue(vignette.mV[VX]);
	gSavedSettings.setVector3("FSRenderVignette", vignette);
}

void FloaterQuickPrefs::onClickResetVignetteY()
{
	LLVector3 vignette_default = LLVector3(gSavedSettings.getControl("FSRenderVignette")->getDefault());
	LLVector3 vignette = gSavedSettings.getVector3("FSRenderVignette");
	vignette.mV[VY] = vignette_default.mV[VY];
	mSliderVignetteY->setValue(vignette.mV[VY]);
	mSpinnerVignetteY->setValue(vignette.mV[VY]);
	gSavedSettings.setVector3("FSRenderVignette", vignette);
}

void FloaterQuickPrefs::onClickResetVignetteZ()
{
	LLVector3 vignette_default = LLVector3(gSavedSettings.getControl("FSRenderVignette")->getDefault());
	LLVector3 vignette = gSavedSettings.getVector3("FSRenderVignette");
	vignette.mV[VZ] = vignette_default.mV[VZ];
	mSliderVignetteZ->setValue(vignette.mV[VZ]);
	mSpinnerVignetteZ->setValue(vignette.mV[VZ]);
	gSavedSettings.setVector3("FSRenderVignette", vignette);
}
// </FS:CR> FIRE-9630 - Vignette UI callbacks


void FloaterQuickPrefs::onChangeRenderShadowSplitExponentSlider()
{
	LLVector3 renderShadowSplitExponent = gSavedSettings.getVector3("RenderShadowSplitExponent");
	renderShadowSplitExponent.mV[VY] = mSliderRenderShadowSplitExponentY->getValueF32();
	mSpinnerRenderShadowSplitExponentY->setValue(renderShadowSplitExponent.mV[VY]);
	gSavedSettings.setVector3("RenderShadowSplitExponent", renderShadowSplitExponent);
}

void FloaterQuickPrefs::onChangeRenderShadowSplitExponentSpinner()
{
	LLVector3 renderShadowSplitExponent = gSavedSettings.getVector3("RenderShadowSplitExponent");
	renderShadowSplitExponent.mV[VY] = mSpinnerRenderShadowSplitExponentY->getValueF32();
	mSliderRenderShadowSplitExponentY->setValue(renderShadowSplitExponent.mV[VY]);
	gSavedSettings.setVector3("RenderShadowSplitExponent", renderShadowSplitExponent);
}

void FloaterQuickPrefs::onClickResetRenderShadowSplitExponentY()
{
	LLVector3 renderShadowSplitExponentDefault = LLVector3(gSavedSettings.getControl("RenderShadowSplitExponent")->getDefault());
	LLVector3 renderShadowSplitExponent = gSavedSettings.getVector3("RenderShadowSplitExponent");
	renderShadowSplitExponent.mV[VY] = renderShadowSplitExponentDefault.mV[VY];
	mSpinnerRenderShadowSplitExponentY->setValue(renderShadowSplitExponent.mV[VY]);
	mSliderRenderShadowSplitExponentY->setValue(renderShadowSplitExponent.mV[VY]);
	gSavedSettings.setVector3("RenderShadowSplitExponent", renderShadowSplitExponent);
}

void FloaterQuickPrefs::onChangeRenderShadowGaussianSlider()
{
	LLVector3 renderShadowGaussian = gSavedSettings.getVector3("RenderShadowGaussian");
	renderShadowGaussian.mV[VX] = mSliderRenderShadowGaussianX->getValueF32();
	renderShadowGaussian.mV[VY] = mSliderRenderShadowGaussianY->getValueF32();
	mSpinnerRenderShadowGaussianX->setValue(renderShadowGaussian.mV[VX]);
	mSpinnerRenderShadowGaussianY->setValue(renderShadowGaussian.mV[VY]);
	gSavedSettings.setVector3("RenderShadowGaussian", renderShadowGaussian);
}

void FloaterQuickPrefs::onChangeRenderShadowGaussianSpinner()
{
	LLVector3 renderShadowGaussian = gSavedSettings.getVector3("RenderShadowGaussian");
	renderShadowGaussian.mV[VX] = mSpinnerRenderShadowGaussianX->getValueF32();
	renderShadowGaussian.mV[VY] = mSpinnerRenderShadowGaussianY->getValueF32();
	mSliderRenderShadowGaussianX->setValue(renderShadowGaussian.mV[VX]);
	mSliderRenderShadowGaussianY->setValue(renderShadowGaussian.mV[VY]);
	gSavedSettings.setVector3("RenderShadowGaussian", renderShadowGaussian);
}

void FloaterQuickPrefs::onClickResetRenderShadowGaussianX()
{
	LLVector3 renderShadowGaussianDefault = LLVector3(gSavedSettings.getControl("RenderShadowGaussian")->getDefault());
	LLVector3 renderShadowGaussian = gSavedSettings.getVector3("RenderShadowGaussian");
	renderShadowGaussian.mV[VX] = renderShadowGaussianDefault.mV[VX];
	mSpinnerRenderShadowGaussianX->setValue(renderShadowGaussian.mV[VX]);
	mSliderRenderShadowGaussianX->setValue(renderShadowGaussian.mV[VX]);
	gSavedSettings.setVector3("RenderShadowGaussian", renderShadowGaussian);
}

void FloaterQuickPrefs::onClickResetRenderShadowGaussianY()
{
	LLVector3 renderShadowGaussianDefault = LLVector3(gSavedSettings.getControl("RenderShadowGaussian")->getDefault());
	LLVector3 renderShadowGaussian = gSavedSettings.getVector3("RenderShadowGaussian");
	renderShadowGaussian.mV[VY] = renderShadowGaussianDefault.mV[VY];
	mSpinnerRenderShadowGaussianY->setValue(renderShadowGaussian.mV[VY]);
	mSliderRenderShadowGaussianY->setValue(renderShadowGaussian.mV[VY]);
	gSavedSettings.setVector3("RenderShadowGaussian", renderShadowGaussian);
}

void FloaterQuickPrefs::onChangeRenderSSAOEffectSlider()
{
	LLVector3 renderSSAOEffect = gSavedSettings.getVector3("RenderSSAOEffect");
	renderSSAOEffect.mV[VX] = mSliderRenderSSAOEffectX->getValueF32();
	mSpinnerRenderSSAOEffectX->setValue(renderSSAOEffect.mV[VX]);
	gSavedSettings.setVector3("RenderSSAOEffect", renderSSAOEffect);
}

void FloaterQuickPrefs::onChangeRenderSSAOEffectSpinner()
{
	LLVector3 renderSSAOEffect = gSavedSettings.getVector3("RenderSSAOEffect");
	renderSSAOEffect.mV[VX] = mSpinnerRenderSSAOEffectX->getValueF32();
	mSliderRenderSSAOEffectX->setValue(renderSSAOEffect.mV[VX]);
	gSavedSettings.setVector3("RenderSSAOEffect", renderSSAOEffect);
}

void FloaterQuickPrefs::onClickResetRenderSSAOEffectX()
{
	LLVector3 renderSSAOEffectDefault = LLVector3(gSavedSettings.getControl("RenderSSAOEffect")->getDefault());
	LLVector3 renderSSAOEffect = gSavedSettings.getVector3("RenderSSAOEffect");
	renderSSAOEffect.mV[VX] = renderSSAOEffectDefault.mV[VX];
	mSpinnerRenderSSAOEffectX->setValue(renderSSAOEffect.mV[VX]);
	mSliderRenderSSAOEffectX->setValue(renderSSAOEffect.mV[VX]);
	gSavedSettings.setVector3("RenderSSAOEffect", renderSSAOEffect);
}

// <FS:CR> FIRE-9407 - Restore Quickprefs Defaults
void FloaterQuickPrefs::callbackRestoreDefaults(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	if ( option == 0 ) // YES
	{
		selectControl("");
		BOOST_FOREACH(const std::string& control, mControlsOrder)
		{
			removeControl(control);
		}
		mControlsOrder.clear();
		std::string settings_file = LLAppViewer::instance()->getSettingsFilename("Default", "QuickPreferences");
		LLFile::remove(gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, settings_file));
		loadSavedSettingsFromFile(gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, settings_file));
		gSavedSettings.setBOOL("QuickPrefsEditMode", FALSE);
	}
	else
	{
		LL_INFOS() << "User cancelled the reset." << LL_ENDL;
	}
}

void FloaterQuickPrefs::onClickRestoreDefaults()
{
	LLNotificationsUtil::add("ConfirmRestoreQuickPrefsDefaults", LLSD(), LLSD(), boost::bind(&FloaterQuickPrefs::callbackRestoreDefaults, this, _1, _2));
}
// </FS:CR>

void FloaterQuickPrefs::dockToToolbarButton()
{
	LLCommandId command_id("quickprefs");
	S32 toolbar_loc = gToolBarView->hasCommand(command_id);
	
	if (toolbar_loc != LLToolBarEnums::TOOLBAR_NONE && !FSCommon::isLegacySkin())
	{
		LLDockControl::DocAt doc_at = LLDockControl::TOP;
		switch (toolbar_loc)
		{
			case LLToolBarEnums::TOOLBAR_LEFT:
				doc_at = LLDockControl::RIGHT;
				break;
			
			case LLToolBarEnums::TOOLBAR_RIGHT:
				doc_at = LLDockControl::LEFT;
				break;
		}
		setCanDock(true);
		LLView* anchor_panel = gToolBarView->findChildView("quickprefs");
		setUseTongue(anchor_panel);
		// Garbage collected by std::auto_ptr
		// Do the actions from setDockControl here, but call setDock with pop_on_undock = false
		// or we will translate the floater 12px up on the y-axis
		mDockControl.reset(new LLDockControl(anchor_panel, this, getDockTongue(doc_at), doc_at));
		setDocked(isDocked(), false);
	}
	else
	{
		setUseTongue(false);
		setDocked(false, false);
		setCanDock(false);
		setDockControl(NULL);
	}
}

void FloaterQuickPrefs::onAvatarZOffsetSliderMoved()
{
	F32 value = mAvatarZOffsetSlider->getValueF32();
	LLVector3 offset(0.0f, 0.0f, llclamp(value, MIN_HOVER_Z, MAX_HOVER_Z));
	LL_INFOS("Avatar") << "setting hover from slider moved" << offset[VZ] << LL_ENDL;
	if (gAgent.getRegion() && gAgent.getRegion()->avatarHoverHeightEnabled())
	{
		gAgentAvatarp->setHoverOffset(offset, false);
	}
	else if (!gAgentAvatarp->isUsingServerBakes())
	{
		gSavedPerAccountSettings.setF32("AvatarHoverOffsetZ", value);
	}
}

void FloaterQuickPrefs::onAvatarZOffsetFinalCommit()
{
	F32 value = mAvatarZOffsetSlider->getValueF32();
	LLVector3 offset(0.0f, 0.0f, llclamp(value,MIN_HOVER_Z,MAX_HOVER_Z));
	gSavedPerAccountSettings.setF32("AvatarHoverOffsetZ",value);

	LL_INFOS("Avatar") << "setting hover from slider final commit " << offset[VZ] << LL_ENDL;
}

void FloaterQuickPrefs::updateAvatarZOffsetEditEnabled()
{
	bool enabled = gAgent.getRegion() && gAgent.getRegion()->avatarHoverHeightEnabled();
	if (!enabled && isAgentAvatarValid() && !gAgentAvatarp->isUsingServerBakes())
	{
		enabled = true;
	}
	mAvatarZOffsetSlider->setEnabled(enabled);
	if (enabled)
	{
		syncAvatarZOffsetFromPreferenceSetting();
	}
}

void FloaterQuickPrefs::onRegionChanged()
{
	LLViewerRegion* region = gAgent.getRegion();
	if (region && region->simulatorFeaturesReceived())
	{
		updateAvatarZOffsetEditEnabled();
	}
	else if (region)
	{
		region->setSimulatorFeaturesReceivedCallback(boost::bind(&FloaterQuickPrefs::onSimulatorFeaturesReceived, this, _1));
	}
}

void FloaterQuickPrefs::onSimulatorFeaturesReceived(const LLUUID &region_id)
{
	LLViewerRegion *region = gAgent.getRegion();
	if (region && (region->getRegionID() == region_id))
	{
		updateAvatarZOffsetEditEnabled();
	}
}

void FloaterQuickPrefs::syncAvatarZOffsetFromPreferenceSetting()
{
	F32 value = gSavedPerAccountSettings.getF32("AvatarHoverOffsetZ");
	mAvatarZOffsetSlider->setValue(value, FALSE);
}

void FloaterQuickPrefs::updateMaxNonImpostors(const LLSD& newvalue)
{
	// Called when the IndirectMaxNonImpostors control changes
	// Responsible for fixing the setting RenderAvatarMaxNonImpostors
	U32 value = newvalue.asInteger();

	if (0 == value || LLVOAvatar::IMPOSTORS_OFF <= value)
	{
		value=0;
	}
	gSavedSettings.setU32("RenderAvatarMaxNonImpostors", value);
	LLVOAvatar::updateImpostorRendering(value); // make it effective immediately
}

void FloaterQuickPrefs::updateMaxComplexity()
{
	// Called when the IndirectMaxComplexity control changes
	LLAvatarComplexityControls::updateMax(mMaxComplexitySlider, mMaxComplexityLabel);
}

void FloaterQuickPrefs::updateMaxComplexityLabel(const LLSD& newvalue)
{
	U32 value = newvalue.asInteger();

	LLAvatarComplexityControls::setText(value, mMaxComplexityLabel);
}
