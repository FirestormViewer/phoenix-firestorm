/** 
 * @file particleeditor.h
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

#ifndef PARTICLEEDITOR_H
#define PARTICLEEDITOR_H

#include "v4color.h"
#include "llfloater.h"
#include "llpartdata.h"

class LLButton;
class LLCheckBoxCtrl;
class LLColorSwatchCtrl;
class LLComboBox;
class LLLineEditor;
class LLSpinCtrl;
class LLTextureCtrl;
class LLViewerObject;
class LLViewerTexture;

class ParticleEditor : public LLFloater
{
	public:
		ParticleEditor(const LLSD& key);
		/* virtual */ ~ParticleEditor();

		/* virtual */ BOOL postBuild();

		void setObject(LLViewerObject* objectp);

	protected:
		void clearParticles();
		void updateParticles();

		void onParameterChange();
		void onCopyButtonClicked();

		std::string lslVector(F32 x,F32 y,F32 z);
		std::string lslColor(const LLColor4& color);

		LLViewerObject* mObject;
		LLViewerTexture* mTexture;
		LLViewerTexture* mDefaultParticleTexture;

		LLPartSysData mParticles;

		std::map<std::string,U8> mPatternMap;
		std::map<std::string,std::string> mScriptPatternMap;

		LLComboBox* mPatternTypeCombo;
		LLTextureCtrl* mTexturePicker;

		LLSpinCtrl* mBurstRateSpinner;
		LLSpinCtrl* mBurstCountSpinner;
		LLSpinCtrl* mBurstRadiusSpinner;
		LLSpinCtrl* mAngleBeginSpinner;
		LLSpinCtrl* mAngleEndSpinner;
		LLSpinCtrl* mBurstSpeedMinSpinner;
		LLSpinCtrl* mBurstSpeedMaxSpinner;
		LLSpinCtrl* mStartAlphaSpinner;
		LLSpinCtrl* mEndAlphaSpinner;
		LLSpinCtrl* mScaleStartXSpinner;
		LLSpinCtrl* mScaleStartYSpinner;
		LLSpinCtrl* mScaleEndXSpinner;
		LLSpinCtrl* mScaleEndYSpinner;
		LLSpinCtrl* mSourceMaxAgeSpinner;
		LLSpinCtrl* mParticlesMaxAgeSpinner;

		LLCheckBoxCtrl* mBounceCheckBox;
		LLCheckBoxCtrl* mEmissiveCheckBox;
		LLCheckBoxCtrl* mFollowSourceCheckBox;
		LLCheckBoxCtrl* mFollowVelocityCheckBox;
		LLCheckBoxCtrl* mInterpolateColorCheckBox;
		LLCheckBoxCtrl* mInterpolateScaleCheckBox;
		LLCheckBoxCtrl* mTargetPositionCheckBox;
		LLCheckBoxCtrl* mTargetLinearCheckBox;
		LLCheckBoxCtrl* mWindCheckBox;

		LLLineEditor* mTargetKeyInput;

		LLSpinCtrl* mAcellerationXSpinner;
		LLSpinCtrl* mAcellerationYSpinner;
		LLSpinCtrl* mAcellerationZSpinner;

		LLSpinCtrl* mOmegaXSpinner;
		LLSpinCtrl* mOmegaYSpinner;
		LLSpinCtrl* mOmegaZSpinner;

		LLColorSwatchCtrl* mStartColorSelector;
		LLColorSwatchCtrl* mEndColorSelector;

		LLButton* mCopyToLSLButton;
};

#endif // PARTICLEEDITOR_H
