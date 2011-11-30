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

#include "particleeditor.h"

#include "llcheckboxctrl.h"
#include "llcolorswatch.h"
#include "llcombobox.h"
#include "lllineeditor.h"
#include "llsd.h"
#include "llspinctrl.h"
#include "lltexturectrl.h"
#include "llviewerobject.h"
#include "llviewerpartsim.h"
#include "llviewerpartsource.h"
#include "llv4math.h"

ParticleEditor::ParticleEditor(const LLSD& key)
:	LLFloater(key),
	mObject(0)
{
	mPatternMap["drop"]=LLPartSysData::LL_PART_SRC_PATTERN_DROP;
	mPatternMap["explode"]=LLPartSysData::LL_PART_SRC_PATTERN_EXPLODE;
	mPatternMap["angle"]=LLPartSysData::LL_PART_SRC_PATTERN_ANGLE;
	mPatternMap["angle_cone"]=LLPartSysData::LL_PART_SRC_PATTERN_ANGLE_CONE;
	mPatternMap["angle_cone_empty"]=LLPartSysData::LL_PART_SRC_PATTERN_ANGLE_CONE_EMPTY;
}

ParticleEditor::~ParticleEditor()
{
	lldebugs << "destroying particle editor " << mObject->getID() << llendl;

	clearParticles();
}

BOOL ParticleEditor::postBuild()
{
	LLPanel* panel=getChild<LLPanel>("particle_editor_panel");

	mPatternTypeCombo=panel->getChild<LLComboBox>("pattern_type_combo");
	mTexturePicker=panel->getChild<LLTextureCtrl>("texture_picker");

	mBurstRateSpinner=panel->getChild<LLSpinCtrl>("burst_rate_spinner");
	mBurstCountSpinner=panel->getChild<LLSpinCtrl>("burst_count_spinner");
	mBurstRadiusSpinner=panel->getChild<LLSpinCtrl>("burst_radius_spinner");
	mAngleBeginSpinner=panel->getChild<LLSpinCtrl>("angle_begin_spinner");
	mAngleEndSpinner=panel->getChild<LLSpinCtrl>("angle_end_spinner");
	mBurstSpeedMinSpinner=panel->getChild<LLSpinCtrl>("burst_speed_min_spinner");
	mBurstSpeedMaxSpinner=panel->getChild<LLSpinCtrl>("burst_speed_max_spinner");
	mStartAlphaSpinner=panel->getChild<LLSpinCtrl>("start_alpha_spinner");
	mEndAlphaSpinner=panel->getChild<LLSpinCtrl>("end_alpha_spinner");
	mScaleStartXSpinner=panel->getChild<LLSpinCtrl>("scale_start_x_spinner");
	mScaleStartYSpinner=panel->getChild<LLSpinCtrl>("scale_start_y_spinner");
	mScaleEndXSpinner=panel->getChild<LLSpinCtrl>("scale_end_x_spinner");
	mScaleEndYSpinner=panel->getChild<LLSpinCtrl>("scale_end_y_spinner");
	mSourceMaxAgeSpinner=panel->getChild<LLSpinCtrl>("source_max_age_spinner");
	mParticlesMaxAgeSpinner=panel->getChild<LLSpinCtrl>("particles_max_age_spinner");

	mBounceCheckBox=panel->getChild<LLCheckBoxCtrl>("bounce_checkbox");
	mEmissiveCheckBox=panel->getChild<LLCheckBoxCtrl>("emissive_checkbox");
	mFollowSourceCheckBox=panel->getChild<LLCheckBoxCtrl>("follow_source_checkbox");
	mFollowVelocityCheckBox=panel->getChild<LLCheckBoxCtrl>("follow_velocity_checkbox");
	mInterpolateColorCheckBox=panel->getChild<LLCheckBoxCtrl>("interpolate_color_checkbox");
	mInterpolateScaleCheckBox=panel->getChild<LLCheckBoxCtrl>("interpolate_scale_checkbox");
	mTargetPositionCheckBox=panel->getChild<LLCheckBoxCtrl>("target_position_checkbox");
	mTargetLinearCheckBox=panel->getChild<LLCheckBoxCtrl>("target_linear_checkbox");
	mWindCheckBox=panel->getChild<LLCheckBoxCtrl>("wind_checkbox");

	mTargetKeyInput=panel->getChild<LLLineEditor>("target_key_input");

	mAcellerationXSpinner=panel->getChild<LLSpinCtrl>("acceleration_x_spinner");
	mAcellerationYSpinner=panel->getChild<LLSpinCtrl>("acceleration_y_spinner");
	mAcellerationZSpinner=panel->getChild<LLSpinCtrl>("acceleration_z_spinner");

	mOmegaXSpinner=panel->getChild<LLSpinCtrl>("omega_x_spinner");
	mOmegaYSpinner=panel->getChild<LLSpinCtrl>("omega_y_spinner");
	mOmegaZSpinner=panel->getChild<LLSpinCtrl>("omega_z_spinner");

	mStartColorSelector=panel->getChild<LLColorSwatchCtrl>("start_color_selector");
	mEndColorSelector=panel->getChild<LLColorSwatchCtrl>("end_color_selector");

	mCopyToLSLButton=panel->getChild<LLButton>("copy_button");

	mPatternTypeCombo->setCommitCallback(boost::bind(&ParticleEditor::onParameterChange,this));
	mTexturePicker->setCommitCallback(boost::bind(&ParticleEditor::onParameterChange,this));

	mBurstRateSpinner->setCommitCallback(boost::bind(&ParticleEditor::onParameterChange,this));
	mBurstCountSpinner->setCommitCallback(boost::bind(&ParticleEditor::onParameterChange,this));
	mBurstRadiusSpinner->setCommitCallback(boost::bind(&ParticleEditor::onParameterChange,this));
	mAngleBeginSpinner->setCommitCallback(boost::bind(&ParticleEditor::onParameterChange,this));
	mAngleEndSpinner->setCommitCallback(boost::bind(&ParticleEditor::onParameterChange,this));
	mBurstSpeedMinSpinner->setCommitCallback(boost::bind(&ParticleEditor::onParameterChange,this));
	mBurstSpeedMaxSpinner->setCommitCallback(boost::bind(&ParticleEditor::onParameterChange,this));
	mStartAlphaSpinner->setCommitCallback(boost::bind(&ParticleEditor::onParameterChange,this));
	mEndAlphaSpinner->setCommitCallback(boost::bind(&ParticleEditor::onParameterChange,this));
	mScaleStartXSpinner->setCommitCallback(boost::bind(&ParticleEditor::onParameterChange,this));
	mScaleStartYSpinner->setCommitCallback(boost::bind(&ParticleEditor::onParameterChange,this));
	mScaleEndXSpinner->setCommitCallback(boost::bind(&ParticleEditor::onParameterChange,this));
	mScaleEndYSpinner->setCommitCallback(boost::bind(&ParticleEditor::onParameterChange,this));
	mSourceMaxAgeSpinner->setCommitCallback(boost::bind(&ParticleEditor::onParameterChange,this));
	mParticlesMaxAgeSpinner->setCommitCallback(boost::bind(&ParticleEditor::onParameterChange,this));

	mBounceCheckBox->setCommitCallback(boost::bind(&ParticleEditor::onParameterChange,this));
	mEmissiveCheckBox->setCommitCallback(boost::bind(&ParticleEditor::onParameterChange,this));
	mFollowSourceCheckBox->setCommitCallback(boost::bind(&ParticleEditor::onParameterChange,this));
	mFollowVelocityCheckBox->setCommitCallback(boost::bind(&ParticleEditor::onParameterChange,this));
	mInterpolateColorCheckBox->setCommitCallback(boost::bind(&ParticleEditor::onParameterChange,this));
	mInterpolateScaleCheckBox->setCommitCallback(boost::bind(&ParticleEditor::onParameterChange,this));
	mTargetPositionCheckBox->setCommitCallback(boost::bind(&ParticleEditor::onParameterChange,this));
	mTargetLinearCheckBox->setCommitCallback(boost::bind(&ParticleEditor::onParameterChange,this));
	mWindCheckBox->setCommitCallback(boost::bind(&ParticleEditor::onParameterChange,this));

	mTargetKeyInput->setCommitCallback(boost::bind(&ParticleEditor::onParameterChange,this));

	mAcellerationXSpinner->setCommitCallback(boost::bind(&ParticleEditor::onParameterChange,this));
	mAcellerationYSpinner->setCommitCallback(boost::bind(&ParticleEditor::onParameterChange,this));
	mAcellerationZSpinner->setCommitCallback(boost::bind(&ParticleEditor::onParameterChange,this));

	mOmegaXSpinner->setCommitCallback(boost::bind(&ParticleEditor::onParameterChange,this));
	mOmegaYSpinner->setCommitCallback(boost::bind(&ParticleEditor::onParameterChange,this));
	mOmegaZSpinner->setCommitCallback(boost::bind(&ParticleEditor::onParameterChange,this));

	mStartColorSelector->setCommitCallback(boost::bind(&ParticleEditor::onParameterChange,this));
	mEndColorSelector->setCommitCallback(boost::bind(&ParticleEditor::onParameterChange,this));

	mStartColorSelector->setCanApplyImmediately(TRUE);
	mEndColorSelector->setCanApplyImmediately(TRUE);
	mTexturePicker->setCanApplyImmediately(TRUE);

	onParameterChange();

	return TRUE;
}

void ParticleEditor::clearParticles()
{
	if(!mObject)
		return;

	lldebugs << "clearing particles from " << mObject->getID() << llendl;

	LLViewerPartSim::getInstance()->clearParticlesByOwnerID(mObject->getID());
}

void ParticleEditor::updateParticles()
{
	if(!mObject)
		return;

	clearParticles();
	LLPointer<LLViewerPartSourceScript> pss=LLViewerPartSourceScript::createPSS(mObject,mParticles);
	pss->setOwnerUUID(mObject->getID());
	LLViewerPartSim::getInstance()->addPartSource(pss);
}

void ParticleEditor::setObject(LLViewerObject* objectp)
{
	if(objectp)
	{
		mObject=objectp;

		lldebugs << "adding particles to " << mObject->getID() << llendl;

		updateParticles();
	}
}

void ParticleEditor::onParameterChange()
{
	mParticles.mPattern=mPatternMap[mPatternTypeCombo->getSelectedValue()];

	mParticles.mPartImageID=mTexturePicker->getImageAssetID();

	mParticles.mBurstRate=mBurstRateSpinner->getValueF32();
	mParticles.mBurstPartCount=mBurstCountSpinner->getValue().asInteger();
	mParticles.mBurstRadius=mBurstRadiusSpinner->getValueF32();
	mParticles.mInnerAngle=mAngleBeginSpinner->getValueF32();
	mParticles.mOuterAngle=mAngleEndSpinner->getValueF32();
	mParticles.mBurstSpeedMin=mBurstSpeedMinSpinner->getValueF32();
	mParticles.mBurstSpeedMax=mBurstSpeedMaxSpinner->getValueF32();
	mParticles.mPartData.setStartAlpha(mStartAlphaSpinner->getValueF32());
	mParticles.mPartData.setEndAlpha(mEndAlphaSpinner->getValueF32());
	mParticles.mPartData.setStartScale(mScaleStartXSpinner->getValueF32(),mScaleStartYSpinner->getValueF32());
	mParticles.mPartData.setEndScale(mScaleEndXSpinner->getValueF32(),mScaleEndYSpinner->getValueF32());
	mParticles.mMaxAge=mSourceMaxAgeSpinner->getValueF32();
	mParticles.mPartData.setMaxAge(mParticlesMaxAgeSpinner->getValueF32());

	U32 flags=0;
	if(mBounceCheckBox->getValue().asBoolean()) flags|=LLPartData::LL_PART_BOUNCE_MASK;
	if(mEmissiveCheckBox->getValue().asBoolean()) flags|=LLPartData::LL_PART_EMISSIVE_MASK;
	if(mFollowSourceCheckBox->getValue().asBoolean()) flags|=LLPartData::LL_PART_FOLLOW_SRC_MASK;
	if(mFollowVelocityCheckBox->getValue().asBoolean()) flags|=LLPartData::LL_PART_FOLLOW_VELOCITY_MASK;
	if(mInterpolateColorCheckBox->getValue().asBoolean()) flags|=LLPartData::LL_PART_INTERP_COLOR_MASK;
	if(mInterpolateScaleCheckBox->getValue().asBoolean()) flags|=LLPartData::LL_PART_INTERP_SCALE_MASK;
	if(mTargetPositionCheckBox->getValue().asBoolean()) flags|=LLPartData::LL_PART_TARGET_POS_MASK;
	if(mTargetLinearCheckBox->getValue().asBoolean()) flags|=LLPartData::LL_PART_TARGET_LINEAR_MASK;
	if(mWindCheckBox->getValue().asBoolean()) flags|=LLPartData::LL_PART_WIND_MASK;
	mParticles.mPartData.setFlags(flags);
	
	mParticles.mTargetUUID=mTargetKeyInput->getValue().asUUID();

	mParticles.mPartAccel=LLVector3(mAcellerationXSpinner->getValueF32(),mAcellerationYSpinner->getValueF32(),mAcellerationZSpinner->getValueF32());
	mParticles.mAngularVelocity=LLVector3(mOmegaXSpinner->getValueF32(),mOmegaYSpinner->getValueF32(),mOmegaZSpinner->getValueF32());

	LLColor4 color=mStartColorSelector->get();
	mParticles.mPartData.setStartColor(LLVector3(color.mV[0],color.mV[1],color.mV[2]));
	color=mEndColorSelector->get();
	mParticles.mPartData.setEndColor(LLVector3(color.mV[0],color.mV[1],color.mV[2]));

	updateParticles();
}
