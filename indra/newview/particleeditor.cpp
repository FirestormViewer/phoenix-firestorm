/** 
 * @file particleeditor.cpp
 * @brief Anything concerning the Viewer Side Particle Editor GUI
 *
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
 */

#include "llviewerprecompiledheaders.h"
#include "particleeditor.h"

#include <fstream>

#include "llagent.h"
#include "llappviewer.h"
#include "llcheckboxctrl.h"
#include "llcolorswatch.h"
#include "llcombobox.h"
#include "lldir.h"
#include "llfoldertype.h"
#include "llinventoryfunctions.h"	// for ROOT_FIRESTORM_FOLDER
#include "llinventorytype.h"
#include "lllineeditor.h"
#include "llnotificationsutil.h"
#include "llpermissions.h"
#include "llspinctrl.h"
#include "lltexturectrl.h"
#include "lltoolmgr.h"
#include "lltoolobjpicker.h"
#include "llviewerobject.h"
#include "llviewerpartsim.h"
#include "llviewerpartsource.h"
#include "llviewerregion.h"
#include "llwindow.h"
#include "llviewerassetupload.h"

ParticleEditor::ParticleEditor(const LLSD& key)
:	LLFloater(key),
	mObject(0),
	mParticleScriptInventoryItem(0)
{
	mPatternMap["drop"] = LLPartSysData::LL_PART_SRC_PATTERN_DROP;
	mPatternMap["explode"] = LLPartSysData::LL_PART_SRC_PATTERN_EXPLODE;
	mPatternMap["angle"] = LLPartSysData::LL_PART_SRC_PATTERN_ANGLE;
	mPatternMap["angle_cone"] = LLPartSysData::LL_PART_SRC_PATTERN_ANGLE_CONE;
	mPatternMap["angle_cone_empty"] = LLPartSysData::LL_PART_SRC_PATTERN_ANGLE_CONE_EMPTY;

	mScriptPatternMap["drop"] = "PSYS_SRC_PATTERN_DROP";
	mScriptPatternMap["explode"] = "PSYS_SRC_PATTERN_EXPLODE";
	mScriptPatternMap["angle"] = "PSYS_SRC_PATTERN_ANGLE";
	mScriptPatternMap["angle_cone"] = "PSYS_SRC_PATTERN_ANGLE_CONE";
	mScriptPatternMap["angle_cone_empty"] = "PSYS_SRC_PATTERN_ANGLE_CONE_EMPTY";

	mBlendMap["blend_one"] = LLPartData::LL_PART_BF_ONE;
	mBlendMap["blend_zero"] = LLPartData::LL_PART_BF_ZERO;
	mBlendMap["blend_dest_color"] = LLPartData::LL_PART_BF_DEST_COLOR;
	mBlendMap["blend_src_color"] = LLPartData::LL_PART_BF_SOURCE_COLOR;
	mBlendMap["blend_one_minus_dest_color"] = LLPartData::LL_PART_BF_ONE_MINUS_DEST_COLOR;
	mBlendMap["blend_one_minus_src_color"] = LLPartData::LL_PART_BF_ONE_MINUS_SOURCE_COLOR;
	mBlendMap["blend_src_alpha"] = LLPartData::LL_PART_BF_SOURCE_ALPHA;
	mBlendMap["blend_one_minus_src_alpha"] = LLPartData::LL_PART_BF_ONE_MINUS_SOURCE_ALPHA;

	mScriptBlendMap["blend_one"] = "PSYS_PART_BF_ONE";
	mScriptBlendMap["blend_zero"] = "PSYS_PART_BF_ZERO";
	mScriptBlendMap["blend_dest_color"] = "PSYS_PART_BF_DEST_COLOR";
	mScriptBlendMap["blend_src_color"] = "PSYS_PART_BF_SOURCE_COLOR";
	mScriptBlendMap["blend_one_minus_dest_color"] = "PSYS_PART_BF_ONE_MINUS_DEST_COLOR";
	mScriptBlendMap["blend_one_minus_src_color"] = "PSYS_PART_BF_ONE_MINUS_SOURCE_COLOR";
	mScriptBlendMap["blend_src_alpha"] = "PSYS_PART_BF_SOURCE_ALPHA";
	mScriptBlendMap["blend_one_minus_src_alpha"] = "PSYS_PART_BF_ONE_MINUS_SOURCE_ALPHA";

	// I don't really like referencing the particle texture name here, but it's being done
	// like this all over the viewer, so this is apparently how it's meant to be. -Zi
	mDefaultParticleTexture = LLViewerTextureManager::getFetchedTextureFromFile("pixiesmall.j2c");
}

ParticleEditor::~ParticleEditor()
{
	LL_DEBUGS() << "destroying particle editor " << mObject->getID() << LL_ENDL;

	clearParticles();
}

BOOL ParticleEditor::postBuild()
{
	mMainPanel = getChild<LLPanel>("particle_editor_panel");

	mPatternTypeCombo = mMainPanel->getChild<LLComboBox>("pattern_type_combo");
	mTexturePicker = mMainPanel->getChild<LLTextureCtrl>("texture_picker");

	mBurstRateSpinner = mMainPanel->getChild<LLSpinCtrl>("burst_rate_spinner");
	mBurstCountSpinner = mMainPanel->getChild<LLSpinCtrl>("burst_count_spinner");
	mBurstRadiusSpinner = mMainPanel->getChild<LLSpinCtrl>("burst_radius_spinner");
	mAngleBeginSpinner = mMainPanel->getChild<LLSpinCtrl>("angle_begin_spinner");
	mAngleEndSpinner = mMainPanel->getChild<LLSpinCtrl>("angle_end_spinner");
	mBurstSpeedMinSpinner = mMainPanel->getChild<LLSpinCtrl>("burst_speed_min_spinner");
	mBurstSpeedMaxSpinner = mMainPanel->getChild<LLSpinCtrl>("burst_speed_max_spinner");
	mStartAlphaSpinner = mMainPanel->getChild<LLSpinCtrl>("start_alpha_spinner");
	mEndAlphaSpinner = mMainPanel->getChild<LLSpinCtrl>("end_alpha_spinner");
	mScaleStartXSpinner = mMainPanel->getChild<LLSpinCtrl>("scale_start_x_spinner");
	mScaleStartYSpinner = mMainPanel->getChild<LLSpinCtrl>("scale_start_y_spinner");
	mScaleEndXSpinner = mMainPanel->getChild<LLSpinCtrl>("scale_end_x_spinner");
	mScaleEndYSpinner = mMainPanel->getChild<LLSpinCtrl>("scale_end_y_spinner");
	mSourceMaxAgeSpinner = mMainPanel->getChild<LLSpinCtrl>("source_max_age_spinner");
	mParticlesMaxAgeSpinner = mMainPanel->getChild<LLSpinCtrl>("particles_max_age_spinner");
	mStartGlowSpinner = mMainPanel->getChild<LLSpinCtrl>("start_glow_spinner");
	mEndGlowSpinner = mMainPanel->getChild<LLSpinCtrl>("end_glow_spinner");

	mBlendFuncSrcCombo = mMainPanel->getChild<LLComboBox>("blend_func_src_combo");
	mBlendFuncDestCombo = mMainPanel->getChild<LLComboBox>("blend_func_dest_combo");

	mBounceCheckBox = mMainPanel->getChild<LLCheckBoxCtrl>("bounce_checkbox");
	mEmissiveCheckBox = mMainPanel->getChild<LLCheckBoxCtrl>("emissive_checkbox");
	mFollowSourceCheckBox = mMainPanel->getChild<LLCheckBoxCtrl>("follow_source_checkbox");
	mFollowVelocityCheckBox = mMainPanel->getChild<LLCheckBoxCtrl>("follow_velocity_checkbox");
	mInterpolateColorCheckBox = mMainPanel->getChild<LLCheckBoxCtrl>("interpolate_color_checkbox");
	mInterpolateScaleCheckBox = mMainPanel->getChild<LLCheckBoxCtrl>("interpolate_scale_checkbox");
	mTargetPositionCheckBox = mMainPanel->getChild<LLCheckBoxCtrl>("target_position_checkbox");
	mTargetLinearCheckBox = mMainPanel->getChild<LLCheckBoxCtrl>("target_linear_checkbox");
	mWindCheckBox = mMainPanel->getChild<LLCheckBoxCtrl>("wind_checkbox");
	mRibbonCheckBox = mMainPanel->getChild<LLCheckBoxCtrl>("ribbon_checkbox");

	mTargetKeyInput = mMainPanel->getChild<LLLineEditor>("target_key_input");

	mAcellerationXSpinner = mMainPanel->getChild<LLSpinCtrl>("acceleration_x_spinner");
	mAcellerationYSpinner = mMainPanel->getChild<LLSpinCtrl>("acceleration_y_spinner");
	mAcellerationZSpinner = mMainPanel->getChild<LLSpinCtrl>("acceleration_z_spinner");

	mOmegaXSpinner = mMainPanel->getChild<LLSpinCtrl>("omega_x_spinner");
	mOmegaYSpinner = mMainPanel->getChild<LLSpinCtrl>("omega_y_spinner");
	mOmegaZSpinner = mMainPanel->getChild<LLSpinCtrl>("omega_z_spinner");

	mStartColorSelector = mMainPanel->getChild<LLColorSwatchCtrl>("start_color_selector");
	mEndColorSelector = mMainPanel->getChild<LLColorSwatchCtrl>("end_color_selector");

	mCopyToLSLButton = mMainPanel->getChild<LLButton>("copy_button");
	mCopyToLSLButton->setCommitCallback(boost::bind(&ParticleEditor::onCopyButtonClicked, this));

	mInjectScriptButton = mMainPanel->getChild<LLButton>("inject_button");
	mInjectScriptButton->setCommitCallback(boost::bind(&ParticleEditor::onInjectButtonClicked, this));

	mClearTargetButton = mMainPanel->getChild<LLButton>("clear_target_button");
	mClearTargetButton->setCommitCallback(boost::bind(&ParticleEditor::onClearTargetButtonClicked, this));

	mPickTargetButton = mMainPanel->getChild<LLButton>("pick_target_button");
	mPickTargetButton->setCommitCallback(boost::bind(&ParticleEditor::onTargetPickerButtonClicked, this));

	mPatternTypeCombo->setCommitCallback(boost::bind(&ParticleEditor::onParameterChange, this));
	mTexturePicker->setCommitCallback(boost::bind(&ParticleEditor::onParameterChange, this));

	mBurstRateSpinner->setCommitCallback(boost::bind(&ParticleEditor::onParameterChange, this));
	mBurstCountSpinner->setCommitCallback(boost::bind(&ParticleEditor::onParameterChange, this));
	mBurstRadiusSpinner->setCommitCallback(boost::bind(&ParticleEditor::onParameterChange, this));
	mAngleBeginSpinner->setCommitCallback(boost::bind(&ParticleEditor::onParameterChange, this));
	mAngleEndSpinner->setCommitCallback(boost::bind(&ParticleEditor::onParameterChange, this));
	mBurstSpeedMinSpinner->setCommitCallback(boost::bind(&ParticleEditor::onParameterChange, this));
	mBurstSpeedMaxSpinner->setCommitCallback(boost::bind(&ParticleEditor::onParameterChange, this));
	mStartAlphaSpinner->setCommitCallback(boost::bind(&ParticleEditor::onParameterChange, this));
	mEndAlphaSpinner->setCommitCallback(boost::bind(&ParticleEditor::onParameterChange, this));
	mScaleStartXSpinner->setCommitCallback(boost::bind(&ParticleEditor::onParameterChange, this));
	mScaleStartYSpinner->setCommitCallback(boost::bind(&ParticleEditor::onParameterChange, this));
	mScaleEndXSpinner->setCommitCallback(boost::bind(&ParticleEditor::onParameterChange, this));
	mScaleEndYSpinner->setCommitCallback(boost::bind(&ParticleEditor::onParameterChange, this));
	mSourceMaxAgeSpinner->setCommitCallback(boost::bind(&ParticleEditor::onParameterChange, this));
	mParticlesMaxAgeSpinner->setCommitCallback(boost::bind(&ParticleEditor::onParameterChange, this));
	mStartGlowSpinner->setCommitCallback(boost::bind(&ParticleEditor::onParameterChange, this));
	mEndGlowSpinner->setCommitCallback(boost::bind(&ParticleEditor::onParameterChange, this));

	mBlendFuncSrcCombo->setCommitCallback(boost::bind(&ParticleEditor::onParameterChange, this));
	mBlendFuncDestCombo->setCommitCallback(boost::bind(&ParticleEditor::onParameterChange, this));

	mBounceCheckBox->setCommitCallback(boost::bind(&ParticleEditor::onParameterChange, this));
	mEmissiveCheckBox->setCommitCallback(boost::bind(&ParticleEditor::onParameterChange, this));
	mFollowSourceCheckBox->setCommitCallback(boost::bind(&ParticleEditor::onParameterChange, this));
	mFollowVelocityCheckBox->setCommitCallback(boost::bind(&ParticleEditor::onParameterChange, this));
	mInterpolateColorCheckBox->setCommitCallback(boost::bind(&ParticleEditor::onParameterChange, this));
	mInterpolateScaleCheckBox->setCommitCallback(boost::bind(&ParticleEditor::onParameterChange, this));
	mTargetPositionCheckBox->setCommitCallback(boost::bind(&ParticleEditor::onParameterChange, this));
	mTargetLinearCheckBox->setCommitCallback(boost::bind(&ParticleEditor::onParameterChange, this));
	mWindCheckBox->setCommitCallback(boost::bind(&ParticleEditor::onParameterChange, this));
	mRibbonCheckBox->setCommitCallback(boost::bind(&ParticleEditor::onParameterChange, this));

	mTargetKeyInput->setCommitCallback(boost::bind(&ParticleEditor::onParameterChange, this));

	mAcellerationXSpinner->setCommitCallback(boost::bind(&ParticleEditor::onParameterChange, this));
	mAcellerationYSpinner->setCommitCallback(boost::bind(&ParticleEditor::onParameterChange, this));
	mAcellerationZSpinner->setCommitCallback(boost::bind(&ParticleEditor::onParameterChange, this));

	mOmegaXSpinner->setCommitCallback(boost::bind(&ParticleEditor::onParameterChange, this));
	mOmegaYSpinner->setCommitCallback(boost::bind(&ParticleEditor::onParameterChange, this));
	mOmegaZSpinner->setCommitCallback(boost::bind(&ParticleEditor::onParameterChange, this));

	mStartColorSelector->setCommitCallback(boost::bind(&ParticleEditor::onParameterChange, this));
	mEndColorSelector->setCommitCallback(boost::bind(&ParticleEditor::onParameterChange, this));

	mStartColorSelector->setCanApplyImmediately(TRUE);
	mEndColorSelector->setCanApplyImmediately(TRUE);
	mTexturePicker->setCanApplyImmediately(TRUE);

	mBlendFuncSrcCombo->setValue("blend_src_alpha");
	mBlendFuncDestCombo->setValue("blend_one_minus_src_alpha");

	onParameterChange();

	return TRUE;
}

void ParticleEditor::clearParticles()
{
	if (!mObject)
		return;

	LL_DEBUGS() << "clearing particles from " << mObject->getID() << LL_ENDL;

	LLViewerPartSim::getInstance()->clearParticlesByOwnerID(mObject->getID());
}

void ParticleEditor::updateParticles()
{
	if (!mObject)
		return;

	clearParticles();
	LLPointer<LLViewerPartSourceScript> pss = LLViewerPartSourceScript::createPSS(mObject, mParticles);

	pss->setOwnerUUID(mObject->getID());
	pss->setImage(mTexture);

	LLViewerPartSim::getInstance()->addPartSource(pss);
}

void ParticleEditor::setObject(LLViewerObject* objectp)
{
	if (objectp)
	{
		mObject = objectp;

		LL_DEBUGS() << "adding particles to " << mObject->getID() << LL_ENDL;

		updateParticles();
	}
}

void ParticleEditor::onParameterChange()
{
	mParticles.mPattern = mPatternMap[mPatternTypeCombo->getSelectedValue()];
	mParticles.mPartImageID = mTexturePicker->getImageAssetID();

	// remember the selected texture here to give updateParticles() a UUID to work with
	mTexture = LLViewerTextureManager::getFetchedTexture(mTexturePicker->getImageAssetID());

	if (mTexture->getID() == IMG_DEFAULT || mTexture->getID().isNull())
	{
		mTexture = mDefaultParticleTexture;
	}

	// limit burst rate to 0.01 to avoid internal freeze, script still gets the real value
	mParticles.mBurstRate = llmax(0.01f, mBurstRateSpinner->getValueF32());
	mParticles.mBurstPartCount = mBurstCountSpinner->getValue().asInteger();
	mParticles.mBurstRadius = mBurstRadiusSpinner->getValueF32();
	mParticles.mInnerAngle = mAngleBeginSpinner->getValueF32();
	mParticles.mOuterAngle = mAngleEndSpinner->getValueF32();
	mParticles.mBurstSpeedMin = mBurstSpeedMinSpinner->getValueF32();
	mParticles.mBurstSpeedMax = mBurstSpeedMaxSpinner->getValueF32();
	mParticles.mPartData.setStartAlpha(mStartAlphaSpinner->getValueF32());
	mParticles.mPartData.setEndAlpha(mEndAlphaSpinner->getValueF32());
	mParticles.mPartData.setStartScale(mScaleStartXSpinner->getValueF32(), mScaleStartYSpinner->getValueF32());
	mParticles.mPartData.setEndScale(mScaleEndXSpinner->getValueF32(), mScaleEndYSpinner->getValueF32());
	mParticles.mMaxAge = mSourceMaxAgeSpinner->getValueF32();
	mParticles.mPartData.setMaxAge(mParticlesMaxAgeSpinner->getValueF32());

	// different way to set values here, ask Linden Lab why -Zi
	mParticles.mPartData.mStartGlow = mStartGlowSpinner->getValueF32();
	mParticles.mPartData.mEndGlow = mEndGlowSpinner->getValueF32();

	mParticles.mPartData.mBlendFuncSource = mBlendMap[mBlendFuncSrcCombo->getSelectedValue()];
	mParticles.mPartData.mBlendFuncDest = mBlendMap[mBlendFuncDestCombo->getSelectedValue()];

	U32 flags = 0;
	if (mBounceCheckBox->getValue().asBoolean())			flags |= LLPartData::LL_PART_BOUNCE_MASK;
	if (mEmissiveCheckBox->getValue().asBoolean())			flags |= LLPartData::LL_PART_EMISSIVE_MASK;
	if (mFollowSourceCheckBox->getValue().asBoolean())		flags |= LLPartData::LL_PART_FOLLOW_SRC_MASK;
	if (mFollowVelocityCheckBox->getValue().asBoolean())	flags |= LLPartData::LL_PART_FOLLOW_VELOCITY_MASK;
	if (mInterpolateColorCheckBox->getValue().asBoolean())	flags |= LLPartData::LL_PART_INTERP_COLOR_MASK;
	if (mInterpolateScaleCheckBox->getValue().asBoolean())	flags |= LLPartData::LL_PART_INTERP_SCALE_MASK;
	if (mTargetPositionCheckBox->getValue().asBoolean())	flags |= LLPartData::LL_PART_TARGET_POS_MASK;
	if (mTargetLinearCheckBox->getValue().asBoolean())		flags |= LLPartData::LL_PART_TARGET_LINEAR_MASK;
	if (mWindCheckBox->getValue().asBoolean())				flags |= LLPartData::LL_PART_WIND_MASK;
	if (mRibbonCheckBox->getValue().asBoolean())			flags |= LLPartData::LL_PART_RIBBON_MASK;
	mParticles.mPartData.setFlags(flags);

	mParticles.mTargetUUID = mTargetKeyInput->getValue().asUUID();

	mParticles.mPartAccel = LLVector3(mAcellerationXSpinner->getValueF32(), mAcellerationYSpinner->getValueF32(), mAcellerationZSpinner->getValueF32());
	mParticles.mAngularVelocity = LLVector3(mOmegaXSpinner->getValueF32(), mOmegaYSpinner->getValueF32(), mOmegaZSpinner->getValueF32());

	LLColor4 color = mStartColorSelector->get();
	mParticles.mPartData.setStartColor(LLVector3(color.mV[VRED], color.mV[VGREEN], color.mV[VBLUE]));
	color = mEndColorSelector->get();
	mParticles.mPartData.setEndColor(LLVector3(color.mV[VRED], color.mV[VGREEN], color.mV[VBLUE]));

	updateUI();
	updateParticles();
}

void ParticleEditor::updateUI()
{
	U8 pattern = mPatternMap[mPatternTypeCombo->getValue()];
	BOOL dropPattern = (pattern == LLPartSysData::LL_PART_SRC_PATTERN_DROP);
	BOOL explodePattern = (pattern == LLPartSysData::LL_PART_SRC_PATTERN_EXPLODE);
	BOOL targetLinear = mTargetLinearCheckBox->getValue();
	BOOL interpolateColor = mInterpolateColorCheckBox->getValue();
	BOOL interpolateScale = mInterpolateScaleCheckBox->getValue();
	BOOL targetEnabled = targetLinear | (mTargetPositionCheckBox->getValue().asBoolean() ? TRUE : FALSE);

	mBurstRadiusSpinner->setEnabled(!(targetLinear | (mFollowSourceCheckBox->getValue().asBoolean() ? TRUE : FALSE) | dropPattern));
	mBurstSpeedMinSpinner->setEnabled(!(targetLinear | dropPattern));
	mBurstSpeedMaxSpinner->setEnabled(!(targetLinear | dropPattern));

	// disabling a color swatch does nothing visually, so we also set alpha
	LLColor4 endColor = mEndColorSelector->get();
	endColor.setAlpha(interpolateColor ? 1.0f : 0.0f);

	mEndAlphaSpinner->setEnabled(interpolateColor);
	mEndColorSelector->set(endColor);
	mEndColorSelector->setEnabled(interpolateColor);

	mScaleEndXSpinner->setEnabled(interpolateScale);
	mScaleEndYSpinner->setEnabled(interpolateScale);

	mTargetPositionCheckBox->setEnabled(!targetLinear);
	mTargetKeyInput->setEnabled(targetEnabled);
	mPickTargetButton->setEnabled(targetEnabled);
	mClearTargetButton->setEnabled(targetEnabled);

	mAcellerationXSpinner->setEnabled(!targetLinear);
	mAcellerationYSpinner->setEnabled(!targetLinear);
	mAcellerationZSpinner->setEnabled(!targetLinear);

	mOmegaXSpinner->setEnabled(!targetLinear);
	mOmegaYSpinner->setEnabled(!targetLinear);
	mOmegaZSpinner->setEnabled(!targetLinear);

	mAngleBeginSpinner->setEnabled(!(explodePattern | dropPattern));
	mAngleEndSpinner->setEnabled(!(explodePattern | dropPattern));
}

void ParticleEditor::onClearTargetButtonClicked()
{
	mTargetKeyInput->clear();
	onParameterChange();
}

void ParticleEditor::onTargetPickerButtonClicked()
{
	mPickTargetButton->setToggleState(TRUE);
	mPickTargetButton->setEnabled(FALSE);
	startPicking(this);
}

// inspired by the LLFloaterReporter object picker
// static
void ParticleEditor::startPicking(void* userdata)
{
	ParticleEditor* self = (ParticleEditor*) userdata;
	LLToolObjPicker::getInstance()->setExitCallback(ParticleEditor::onTargetPicked, self);
	LLToolMgr::getInstance()->setTransientTool(LLToolObjPicker::getInstance());
}

// static
void ParticleEditor::onTargetPicked(void* userdata)
{
	ParticleEditor* self = (ParticleEditor*)userdata;

	LLUUID picked = LLToolObjPicker::getInstance()->getObjectID();

	LLToolMgr::getInstance()->clearTransientTool();

	self->mPickTargetButton->setEnabled(TRUE);
	self->mPickTargetButton->setToggleState(FALSE);

	if (picked.notNull())
	{
		self->mTargetKeyInput->setValue(picked.asString());
		self->onParameterChange();
	}
}

std::string ParticleEditor::lslVector(F32 x, F32 y, F32 z)
{
	return llformat("<%f,%f,%f>", x, y, z);
}

std::string ParticleEditor::lslColor(const LLColor4& color)
{
	return lslVector(color.mV[VRED], color.mV[VGREEN], color.mV[VBLUE]);
}

std::string ParticleEditor::createScript()
{
	std::string script=
"\
default\n\
{\n\
    state_entry()\n\
    {\n\
        llParticleSystem(\n\
        [\n\
            PSYS_SRC_PATTERN,[PATTERN],\n\
            PSYS_SRC_BURST_RADIUS,[BURST_RADIUS],\n\
            PSYS_SRC_ANGLE_BEGIN,[ANGLE_BEGIN],\n\
            PSYS_SRC_ANGLE_END,[ANGLE_END],\n\
            PSYS_SRC_TARGET_KEY,[TARGET_KEY],\n\
            PSYS_PART_START_COLOR,[START_COLOR],\n\
            PSYS_PART_END_COLOR,[END_COLOR],\n\
            PSYS_PART_START_ALPHA,[START_ALPHA],\n\
            PSYS_PART_END_ALPHA,[END_ALPHA],\n\
            PSYS_PART_START_GLOW,[START_GLOW],\n\
            PSYS_PART_END_GLOW,[END_GLOW],\n\
            PSYS_PART_BLEND_FUNC_SOURCE,[BLEND_FUNC_SOURCE],\n\
            PSYS_PART_BLEND_FUNC_DEST,[BLEND_FUNC_DEST],\n\
            PSYS_PART_START_SCALE,[START_SCALE],\n\
            PSYS_PART_END_SCALE,[END_SCALE],\n\
            PSYS_SRC_TEXTURE,\"[TEXTURE]\",\n\
            PSYS_SRC_MAX_AGE,[SOURCE_MAX_AGE],\n\
            PSYS_PART_MAX_AGE,[PART_MAX_AGE],\n\
            PSYS_SRC_BURST_RATE,[BURST_RATE],\n\
            PSYS_SRC_BURST_PART_COUNT,[BURST_COUNT],\n\
            PSYS_SRC_ACCEL,[ACCELERATION],\n\
            PSYS_SRC_OMEGA,[OMEGA],\n\
            PSYS_SRC_BURST_SPEED_MIN,[BURST_SPEED_MIN],\n\
            PSYS_SRC_BURST_SPEED_MAX,[BURST_SPEED_MAX],\n\
            PSYS_PART_FLAGS,\n\
                0[FLAGS]\n\
        ]);\n\
    }\n\
}\n";

	LLUUID targetKey = mTargetKeyInput->getValue().asUUID();
	std::string keyString = "llGetKey()";

	if (targetKey.notNull() && targetKey != mObject->getID())
	{
		keyString="(key) \"" + targetKey.asString() + "\"";
	}

	LLUUID textureKey = mTexture->getID();
	std::string textureString;
	if (textureKey.notNull() && textureKey != IMG_DEFAULT && textureKey != mDefaultParticleTexture->getID())
	{
		textureString = textureKey.asString();
	}

	LLStringUtil::replaceString(script,"[PATTERN]", mScriptPatternMap[mPatternTypeCombo->getValue()]);
	LLStringUtil::replaceString(script,"[BURST_RADIUS]", mBurstRadiusSpinner->getValue().asString());
	LLStringUtil::replaceString(script,"[ANGLE_BEGIN]", mAngleBeginSpinner->getValue().asString());
	LLStringUtil::replaceString(script,"[ANGLE_END]", mAngleEndSpinner->getValue().asString());
	LLStringUtil::replaceString(script,"[TARGET_KEY]", keyString);
	LLStringUtil::replaceString(script,"[START_COLOR]", lslColor(mStartColorSelector->get()));
	LLStringUtil::replaceString(script,"[END_COLOR]", lslColor(mEndColorSelector->get()));
	LLStringUtil::replaceString(script,"[START_ALPHA]", mStartAlphaSpinner->getValue().asString());
	LLStringUtil::replaceString(script,"[END_ALPHA]", mEndAlphaSpinner->getValue().asString());
	LLStringUtil::replaceString(script,"[START_GLOW]", mStartGlowSpinner->getValue().asString());
	LLStringUtil::replaceString(script,"[END_GLOW]", mEndGlowSpinner->getValue().asString());
	LLStringUtil::replaceString(script,"[START_SCALE]", lslVector(mScaleStartXSpinner->getValueF32(), mScaleStartYSpinner->getValueF32(), 0.0f));
	LLStringUtil::replaceString(script,"[END_SCALE]", lslVector(mScaleEndXSpinner->getValueF32(), mScaleEndYSpinner->getValueF32(), 0.0f));
	LLStringUtil::replaceString(script,"[TEXTURE]", textureString);
	LLStringUtil::replaceString(script,"[SOURCE_MAX_AGE]", mSourceMaxAgeSpinner->getValue().asString());
	LLStringUtil::replaceString(script,"[PART_MAX_AGE]", mParticlesMaxAgeSpinner->getValue().asString());
	LLStringUtil::replaceString(script,"[BURST_RATE]", mBurstRateSpinner->getValue().asString());
	LLStringUtil::replaceString(script,"[BURST_COUNT]", mBurstCountSpinner->getValue());
	LLStringUtil::replaceString(script,"[ACCELERATION]", lslVector(mAcellerationXSpinner->getValueF32(), mAcellerationYSpinner->getValueF32(), mAcellerationZSpinner->getValueF32()));
	LLStringUtil::replaceString(script,"[OMEGA]", lslVector(mOmegaXSpinner->getValueF32(), mOmegaYSpinner->getValueF32(), mOmegaZSpinner->getValueF32()));
	LLStringUtil::replaceString(script,"[BURST_SPEED_MIN]", mBurstSpeedMinSpinner->getValue().asString());
	LLStringUtil::replaceString(script,"[BURST_SPEED_MAX]", mBurstSpeedMaxSpinner->getValue().asString());
	LLStringUtil::replaceString(script,"[BLEND_FUNC_SOURCE]", mScriptBlendMap[mBlendFuncSrcCombo->getValue().asString()]);
	LLStringUtil::replaceString(script,"[BLEND_FUNC_DEST]", mScriptBlendMap[mBlendFuncDestCombo->getValue().asString()]);

	std::string delimiter = " |\n                ";
	std::string flagsString;

	if (mBounceCheckBox->getValue())
		flagsString += delimiter + "PSYS_PART_BOUNCE_MASK";
	if (mEmissiveCheckBox->getValue())
		flagsString += delimiter + "PSYS_PART_EMISSIVE_MASK";
	if (mFollowSourceCheckBox->getValue())
		flagsString += delimiter + "PSYS_PART_FOLLOW_SRC_MASK";
	if (mFollowVelocityCheckBox->getValue())
		flagsString += delimiter + "PSYS_PART_FOLLOW_VELOCITY_MASK";
	if (mInterpolateColorCheckBox->getValue())
		flagsString += delimiter + "PSYS_PART_INTERP_COLOR_MASK";
	if (mInterpolateScaleCheckBox->getValue())
		flagsString += delimiter + "PSYS_PART_INTERP_SCALE_MASK";
	if (mTargetLinearCheckBox->getValue())
		flagsString += delimiter + "PSYS_PART_TARGET_LINEAR_MASK";
	if (mTargetPositionCheckBox->getValue())
		flagsString += delimiter + "PSYS_PART_TARGET_POS_MASK";
	if (mWindCheckBox->getValue())
		flagsString += delimiter + "PSYS_PART_WIND_MASK";
	if (mRibbonCheckBox->getValue())
		flagsString += delimiter + "PSYS_PART_RIBBON_MASK";

	LLStringUtil::replaceString(script, "[FLAGS]", flagsString);
	LL_DEBUGS() << "\n" << script << LL_ENDL;

	return script;
}

void ParticleEditor::onCopyButtonClicked()
{
	std::string script = createScript();
	if (!script.empty())
	{
		getWindow()->copyTextToClipboard(utf8str_to_wstring(script));
		LLNotificationsUtil::add("ParticleScriptCopiedToClipboard");
	}
}

void ParticleEditor::onInjectButtonClicked()
{
	// first try to find the #Firestorm folder
	LLUUID categoryID = gInventory.findCategoryByName(ROOT_FIRESTORM_FOLDER);

	// if no #Firestorm folder was found, create one
	if (categoryID.isNull())
	{
		categoryID = gInventory.createNewCategory(gInventory.getRootFolderID(), LLFolderType::FT_NONE, ROOT_FIRESTORM_FOLDER);
	}

	// if still no #Firestorm folder was found, try to find the default "Scripts" folder
	if (categoryID.isNull())
	{
		std::string scriptFolderName = LLFolderType::lookup(LLFolderType::FT_LSL_TEXT);
		gInventory.findCategoryByName(scriptFolderName);
	}

	// if still no valid folder found bail out and complain
	if (categoryID.isNull())
	{
		LLNotificationsUtil::add("ParticleScriptFindFolderFailed");
		return;
	}

	// setup permissions
	LLPermissions perm;
	perm.init(gAgentID, gAgentID, LLUUID::null, LLUUID::null);
	perm.initMasks(PERM_ALL, PERM_ALL, PERM_ALL, PERM_ALL, PERM_ALL);

	// create new script inventory item and wait for it to come back (callback)
	LLPointer<LLInventoryCallback> callback = new ParticleScriptCreationCallback(this);
	create_inventory_item(
		gAgentID,
		gAgentSessionID,
		categoryID,
		LLTransactionID::tnull,
		PARTICLE_SCRIPT_NAME,
		"",
		LLAssetType::AT_LSL_TEXT,
		LLInventoryType::IT_LSL,
		NOT_WEARABLE,
		perm.getMaskNextOwner(),
		callback);

	setCanClose(FALSE);
}

void scriptUploadDone( LLUUID itemId, LLUUID taskId, LLUUID newAssetId, LLSD response, ParticleEditor *aEditor )
{
	aEditor->scriptInjectReturned();
}

void ParticleEditor::callbackReturned(const LLUUID& inventoryItemID)
{
	setCanClose(TRUE);

	if (inventoryItemID.isNull())
	{
		LLNotificationsUtil::add("ParticleScriptCreationFailed");
		return;
	}

	mParticleScriptInventoryItem = gInventory.getItem(inventoryItemID);
	if (!mParticleScriptInventoryItem) 
	{
		LLNotificationsUtil::add("ParticleScriptNotFound");
		return;
	}
	gInventory.updateItem(mParticleScriptInventoryItem);
	gInventory.notifyObservers();

	//caps import 
	std::string url = gAgent.getRegion()->getCapability("UpdateScriptAgent");

	if (!url.empty())
	{
		std::string script = createScript();

        LLBufferedAssetUploadInfo::taskUploadFinish_f proc = boost::bind(scriptUploadDone, _1, _2, _3, _4, this );

        LLResourceUploadInfo::ptr_t uploadInfo(new LLScriptAssetUpload(mObject->getID(), inventoryItemID, LLScriptAssetUpload::MONO, true, LLUUID::null, script, proc));

        LLViewerAssetUpload::EnqueueInventoryUpload(url, uploadInfo);

		mMainPanel->setEnabled(FALSE);
		setCanClose(FALSE);
	}
	else
	{
		LLNotificationsUtil::add("ParticleScriptCapsFailed");
		return;
	}
}

void ParticleEditor::scriptInjectReturned( )
{
	setCanClose(TRUE);

	// play it safe, because some time may have passed
	LLViewerObject* object = gObjectList.findObject(mObject->getID());
	if (!object)
	{
		LL_DEBUGS() << "object went away!" << LL_ENDL;
		mMainPanel->setEnabled(TRUE);
		return;
	}

	mObject->saveScript(mParticleScriptInventoryItem, TRUE, FALSE);
	LLNotificationsUtil::add("ParticleScriptInjected");

	delete this;
}

// ---------------------------------- Callbacks ----------------------------------

ParticleScriptCreationCallback::ParticleScriptCreationCallback(ParticleEditor* editor)
{
	mEditor = editor;
}

ParticleScriptCreationCallback::~ParticleScriptCreationCallback()
{
}

void ParticleScriptCreationCallback::fire(const LLUUID& inventoryItem)
{
	if (!gDisconnected && !LLAppViewer::instance()->quitRequested() && mEditor)
	{
		mEditor->callbackReturned(inventoryItem);
	}
}

