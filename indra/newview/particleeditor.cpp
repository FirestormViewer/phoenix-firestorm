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

#include <iostream>
#include <fstream>

#include "llagent.h"
#include "llassetuploadresponders.h"
#include "llcheckboxctrl.h"
#include "llclipboard.h"
#include "llcolorswatch.h"
#include "llcombobox.h"
#include "lldir.h"
#include "llfoldertype.h"
#include "llhttpclient.h"
#include "llinventoryfunctions.h"	// for ROOT_FIRESTORM_FOLDER
#include "llinventorytype.h"
#include "lllineeditor.h"
#include "llnotificationsutil.h"
#include "llpermissions.h"
#include "llpreviewscript.h"
#include "llsd.h"
#include "llspinctrl.h"
#include "lltexturectrl.h"
#include "lltoolmgr.h"
#include "lltoolobjpicker.h"
#include "llviewerobject.h"
#include "llviewerpartsim.h"
#include "llviewerpartsource.h"
#include "llviewerregion.h"
#include "llwindow.h"
#include "v4math.h"

ParticleEditor::ParticleEditor(const LLSD& key)
:	LLFloater(key),
	mObject(0),
	mParticleScriptInventoryItem(0)
{
	mPatternMap["drop"]=LLPartSysData::LL_PART_SRC_PATTERN_DROP;
	mPatternMap["explode"]=LLPartSysData::LL_PART_SRC_PATTERN_EXPLODE;
	mPatternMap["angle"]=LLPartSysData::LL_PART_SRC_PATTERN_ANGLE;
	mPatternMap["angle_cone"]=LLPartSysData::LL_PART_SRC_PATTERN_ANGLE_CONE;
	mPatternMap["angle_cone_empty"]=LLPartSysData::LL_PART_SRC_PATTERN_ANGLE_CONE_EMPTY;

	mScriptPatternMap["drop"]="PSYS_SRC_PATTERN_DROP";
	mScriptPatternMap["explode"]="PSYS_SRC_PATTERN_EXPLODE";
	mScriptPatternMap["angle"]="PSYS_SRC_PATTERN_ANGLE";
	mScriptPatternMap["angle_cone"]="PSYS_SRC_PATTERN_ANGLE_CONE";
	mScriptPatternMap["angle_cone_empty"]="PSYS_SRC_PATTERN_ANGLE_CONE_EMPTY";

	// I don't really like referencing the particle texture name here, but it's being done
	// like this all over the viewer, so this is apparently how it's meant to be. -Zi
	mDefaultParticleTexture=LLViewerTextureManager::getFetchedTextureFromFile("pixiesmall.j2c");
}

ParticleEditor::~ParticleEditor()
{
	lldebugs << "destroying particle editor " << mObject->getID() << llendl;

	clearParticles();
}

BOOL ParticleEditor::postBuild()
{
	LLPanel* panel=getChild<LLPanel>("particle_editor_panel");

	mMainPanel=panel;

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
	mCopyToLSLButton->setCommitCallback(boost::bind(&ParticleEditor::onCopyButtonClicked,this));

	mInjectScriptButton=panel->getChild<LLButton>("inject_button");
	mInjectScriptButton->setCommitCallback(boost::bind(&ParticleEditor::onInjectButtonClicked,this));

	mClearTargetButton=panel->getChild<LLButton>("clear_target_button");
	mClearTargetButton->setCommitCallback(boost::bind(&ParticleEditor::onClearTargetButtonClicked,this));

	mPickTargetButton=panel->getChild<LLButton>("pick_target_button");
	mPickTargetButton->setCommitCallback(boost::bind(&ParticleEditor::onTargetPickerButtonClicked,this));

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
	pss->setImage(mTexture);

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

	// remember the selected texture here to give updateParticles() a UUID to work with
	mTexture=LLViewerTextureManager::getFetchedTexture(mTexturePicker->getImageAssetID());

	if(mTexture->getID()==IMG_DEFAULT || mTexture->getID().isNull())
		mTexture=mDefaultParticleTexture;

	// limit burst rate to 0.01 to avoid internal freeze, script still gets the real value
	mParticles.mBurstRate=llmax(0.01f,mBurstRateSpinner->getValueF32());
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

	updateUI();
	updateParticles();
}

void ParticleEditor::updateUI()
{
	U8 pattern=mPatternMap[mPatternTypeCombo->getValue()];
	BOOL dropPattern=(pattern==LLPartSysData::LL_PART_SRC_PATTERN_DROP);
	BOOL explodePattern=(pattern==LLPartSysData::LL_PART_SRC_PATTERN_EXPLODE);
	BOOL targetLinear=mTargetLinearCheckBox->getValue();
	BOOL interpolateColor=mInterpolateColorCheckBox->getValue();
	BOOL interpolateScale=mInterpolateScaleCheckBox->getValue();
	BOOL targetEnabled=targetLinear | (mTargetPositionCheckBox->getValue().asBoolean() ? TRUE : FALSE);

	mBurstRadiusSpinner->setEnabled(!(targetLinear | (mFollowSourceCheckBox->getValue().asBoolean() ? TRUE : FALSE) | dropPattern));
	mBurstSpeedMinSpinner->setEnabled(!(targetLinear | dropPattern));
	mBurstSpeedMaxSpinner->setEnabled(!(targetLinear | dropPattern));

	// disabling a color swatch does nothing visually, so we also set alpha
	LLColor4 endColor=mEndColorSelector->get();
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
	ParticleEditor* self =(ParticleEditor*) userdata;
	LLToolObjPicker::getInstance()->setExitCallback(ParticleEditor::onTargetPicked,self);
	LLToolMgr::getInstance()->setTransientTool(LLToolObjPicker::getInstance());
}

// static
void ParticleEditor::onTargetPicked(void* userdata)
{
	ParticleEditor* self=(ParticleEditor*) userdata;

	LLUUID picked=LLToolObjPicker::getInstance()->getObjectID();

	LLToolMgr::getInstance()->clearTransientTool();

	self->mPickTargetButton->setEnabled(TRUE);
	self->mPickTargetButton->setToggleState(FALSE);

	if(picked.notNull())
	{
		self->mTargetKeyInput->setValue(picked.asString());
		self->onParameterChange();
	}
}

std::string ParticleEditor::lslVector(F32 x,F32 y,F32 z)
{
	return llformat("<%f,%f,%f>",x,y,z);
}

std::string ParticleEditor::lslColor(const LLColor4& color)
{
	return lslVector(color.mV[0],color.mV[1],color.mV[2]);
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

	LLUUID targetKey=mTargetKeyInput->getValue().asUUID();
	std::string keyString="llGetKey()";

	if(!targetKey.isNull() && targetKey!=mObject->getID())
		keyString="(key) \""+targetKey.asString()+"\"";

	LLUUID textureKey=mTexture->getID();
	std::string textureString;
	if(!textureKey.isNull() && textureKey!=IMG_DEFAULT && textureKey!=mDefaultParticleTexture->getID())
		textureString=textureKey.asString();

	LLStringUtil::replaceString(script,"[PATTERN]",mScriptPatternMap[mPatternTypeCombo->getValue()]);
	LLStringUtil::replaceString(script,"[BURST_RADIUS]",mBurstRadiusSpinner->getValue().asString());
	LLStringUtil::replaceString(script,"[ANGLE_BEGIN]",mAngleBeginSpinner->getValue().asString());
	LLStringUtil::replaceString(script,"[ANGLE_END]",mAngleEndSpinner->getValue().asString());
	LLStringUtil::replaceString(script,"[TARGET_KEY]",keyString);
	LLStringUtil::replaceString(script,"[START_COLOR]",lslColor(mStartColorSelector->get()));
	LLStringUtil::replaceString(script,"[END_COLOR]",lslColor(mEndColorSelector->get()));
	LLStringUtil::replaceString(script,"[START_ALPHA]",mStartAlphaSpinner->getValue().asString());
	LLStringUtil::replaceString(script,"[END_ALPHA]",mEndAlphaSpinner->getValue().asString());
	LLStringUtil::replaceString(script,"[START_SCALE]",lslVector(mScaleStartXSpinner->getValueF32(),mScaleStartYSpinner->getValueF32(),0.0f));
	LLStringUtil::replaceString(script,"[END_SCALE]",lslVector(mScaleEndXSpinner->getValueF32(),mScaleEndYSpinner->getValueF32(),0.0f));
	LLStringUtil::replaceString(script,"[TEXTURE]",textureString);
	LLStringUtil::replaceString(script,"[SOURCE_MAX_AGE]",mSourceMaxAgeSpinner->getValue().asString());
	LLStringUtil::replaceString(script,"[PART_MAX_AGE]",mParticlesMaxAgeSpinner->getValue().asString());
	LLStringUtil::replaceString(script,"[BURST_RATE]",mBurstRateSpinner->getValue().asString());
	LLStringUtil::replaceString(script,"[BURST_COUNT]",mBurstCountSpinner->getValue());
	LLStringUtil::replaceString(script,"[ACCELERATION]",lslVector(mAcellerationXSpinner->getValueF32(),mAcellerationYSpinner->getValueF32(),mAcellerationZSpinner->getValueF32()));
	LLStringUtil::replaceString(script,"[OMEGA]",lslVector(mOmegaXSpinner->getValueF32(),mOmegaYSpinner->getValueF32(),mOmegaZSpinner->getValueF32()));
	LLStringUtil::replaceString(script,"[BURST_SPEED_MIN]",mBurstSpeedMinSpinner->getValue().asString());
	LLStringUtil::replaceString(script,"[BURST_SPEED_MAX]",mBurstSpeedMaxSpinner->getValue().asString());

	std::string delimiter=" |\n                ";
	std::string flagsString;

	if(mBounceCheckBox->getValue())
		flagsString+=delimiter+"PSYS_PART_BOUNCE_MASK";
	if(mEmissiveCheckBox->getValue())
		flagsString+=delimiter+"PSYS_PART_EMISSIVE_MASK";
	if(mFollowSourceCheckBox->getValue())
		flagsString+=delimiter+"PSYS_PART_FOLLOW_SRC_MASK";
	if(mFollowVelocityCheckBox->getValue())
		flagsString+=delimiter+"PSYS_PART_FOLLOW_VELOCITY_MASK";
	if(mInterpolateColorCheckBox->getValue())
		flagsString+=delimiter+"PSYS_PART_INTERP_COLOR_MASK";
	if(mInterpolateScaleCheckBox->getValue())
		flagsString+=delimiter+"PSYS_PART_INTERP_SCALE_MASK";
	if(mTargetLinearCheckBox->getValue())
		flagsString+=delimiter+"PSYS_PART_TARGET_LINEAR_MASK";
	if(mTargetPositionCheckBox->getValue())
		flagsString+=delimiter+"PSYS_PART_TARGET_POS_MASK";
	if(mWindCheckBox->getValue())
		flagsString+=delimiter+"PSYS_PART_WIND_MASK";

	LLStringUtil::replaceString(script,"[FLAGS]",flagsString);
	lldebugs << "\n" << script << llendl;

	return script;
}

void ParticleEditor::onCopyButtonClicked()
{
	std::string script=createScript();
	if(!script.empty())
	{
		getWindow()->copyTextToClipboard(utf8str_to_wstring(script));
		LLNotificationsUtil::add("ParticleScriptCopiedToClipboard");
	}
}

void ParticleEditor::onInjectButtonClicked()
{
	// TODO: move searching and creating the #Firestorm folder into a helper class for all to use
	LLUUID categoryID;

	// first try to find the #Firestorm folder
	categoryID=gInventory.findCategoryByName(ROOT_FIRESTORM_FOLDER);

	// if no #Firestorm folder was found, create one
	if(categoryID.isNull())
		categoryID=gInventory.createNewCategory(gInventory.getRootFolderID(),LLFolderType::FT_NONE,ROOT_FIRESTORM_FOLDER);

	// if still no #Firestorm folder was found, try to find the default "Scripts" folder
	if(categoryID.isNull())
	{
		std::string scriptFolderName=LLFolderType::lookup(LLFolderType::FT_LSL_TEXT);
		gInventory.findCategoryByName(scriptFolderName);
	}

	// if still no valid folder found bail out and complain
	if(categoryID.isNull())
	{
		LLNotificationsUtil::add("ParticleScriptFindFolderFailed");
		return;
	}

	// setup permissions
	LLPermissions perm;
	perm.init(gAgent.getID(),gAgent.getID(),LLUUID::null,LLUUID::null);
	perm.initMasks(PERM_ALL,PERM_ALL,PERM_ALL,PERM_ALL,PERM_ALL);

	// create new script inventory item and wait for it to come back (callback)
	LLPointer<LLInventoryCallback> callback=new ParticleScriptCreationCallback(this);
	create_inventory_item(
		gAgent.getID(), 
		gAgent.getSessionID(),
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

void ParticleEditor::callbackReturned(const LLUUID& inventoryItemID)
{
	setCanClose(TRUE);

	if(inventoryItemID.isNull())
	{
		LLNotificationsUtil::add("ParticleScriptCreationFailed");
		return;
	}

	mParticleScriptInventoryItem=gInventory.getItem(inventoryItemID);
	if(!mParticleScriptInventoryItem) 
	{
		LLNotificationsUtil::add("ParticleScriptNotFound");
		return;
	}
    gInventory.updateItem(mParticleScriptInventoryItem);
    gInventory.notifyObservers();

	//caps import 
	std::string url=gAgent.getRegion()->getCapability("UpdateScriptAgent");

	if(!url.empty())  
	{
		std::string script=createScript();

		std::string tempFileName=gDirUtilp->getExpandedFilename(LL_PATH_CACHE,"particle_script.lsltxt");

		std::ofstream tempFile;

		tempFile.open(tempFileName.c_str());
		if(!tempFile.is_open())
		{
			LLNotificationsUtil::add("ParticleScriptCreateTempFileFailed");
			return;
		}

		tempFile << script.c_str();
		tempFile.close();

		LLSD body;
		body["task_id"]=mObject->getID();	// probably has no effect
		body["item_id"]=inventoryItemID;
		body["target"]="mono";
		body["is_script_running"]=true;

		// responder will alert us when the job is done
		LLHTTPClient::post(url,body,new ParticleScriptUploadResponder(body,tempFileName,LLAssetType::AT_LSL_TEXT,this));

		mMainPanel->setEnabled(FALSE);
		setCanClose(FALSE);
	}
	else
	{
		LLNotificationsUtil::add("ParticleScriptCapsFailed");
		return;
	}
}

void ParticleEditor::scriptInjectReturned(const LLSD& content)
{
	setCanClose(TRUE);

	// play it safe, because some time may have passed
	LLViewerObject* object=gObjectList.findObject(mObject->getID());
	if(!object)
	{
		lldebugs << "object went away!" << llendl;
		mMainPanel->setEnabled(TRUE);
		return;
	}

	mObject->saveScript(mParticleScriptInventoryItem,TRUE,FALSE);
	LLNotificationsUtil::add("ParticleScriptInjected");

	delete this;
}

// ---------------------------------- Callbacks ----------------------------------

ParticleScriptCreationCallback::ParticleScriptCreationCallback(ParticleEditor* editor)
{
	mEditor=editor;
}

ParticleScriptCreationCallback::~ParticleScriptCreationCallback()
{
}

void ParticleScriptCreationCallback::fire(const LLUUID& inventoryItem)
{
	mEditor->callbackReturned(inventoryItem);
}

// ---------------------------------- Responders ----------------------------------

ParticleScriptUploadResponder::ParticleScriptUploadResponder(const LLSD& post_data,
															 const std::string& file_name,
															 LLAssetType::EType asset_type,
															 ParticleEditor* editor
															) :
	LLUpdateAgentInventoryResponder(post_data,file_name,asset_type)
{
	mEditor=editor;
}

void ParticleScriptUploadResponder::uploadComplete(const LLSD& content)
{
	LLUpdateAgentInventoryResponder::uploadComplete(content);
	mEditor->scriptInjectReturned(content);
}
