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

#include "llfloater.h"
#include "llpartdata.h"
#include "llviewerinventory.h"

#define PARTICLE_SCRIPT_NAME "New Particle Script"

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
    friend class ParticleScriptCreationCallback;
    friend class ParticleScriptUploadResponder;

public:
    ParticleEditor(const LLSD& key);
    ~ParticleEditor();

    bool postBuild() override;

    void setObject(LLViewerObject* objectp);
    void scriptInjectReturned();

protected:
    void clearParticles();
    void updateParticles();
    void updateUI();

    std::string createScript();
    void        createScriptInventoryItem(LLUUID categoryID);

    void onParameterChange();
    void onCopyButtonClicked();
    void onInjectButtonClicked();

    void        onClearTargetButtonClicked();
    void        onTargetPickerButtonClicked();
    void        startPicking();
    static void onTargetPicked(void* userdata);

    void callbackReturned(const LLUUID& inv_item);

    std::string lslVector(F32 x, F32 y, F32 z);
    std::string lslColor(const LLColor4& color);

    LLViewerObject*        mObject{ nullptr };
    LLViewerTexture*       mTexture{ nullptr };
    LLViewerInventoryItem* mParticleScriptInventoryItem{ nullptr };

    LLViewerTexture* mDefaultParticleTexture{ nullptr };

    LLPartSysData mParticles;

    std::map<std::string, U8>          mPatternMap;
    std::map<std::string, std::string> mScriptPatternMap;

    std::map<std::string, U8>          mBlendMap;
    std::map<std::string, std::string> mScriptBlendMap;

    LLPanel* mMainPanel{ nullptr };

    LLComboBox*    mPatternTypeCombo{ nullptr };
    LLTextureCtrl* mTexturePicker{ nullptr };

    LLSpinCtrl* mBurstRateSpinner{ nullptr };
    LLSpinCtrl* mBurstCountSpinner{ nullptr };
    LLSpinCtrl* mBurstRadiusSpinner{ nullptr };
    LLSpinCtrl* mAngleBeginSpinner{ nullptr };
    LLSpinCtrl* mAngleEndSpinner{ nullptr };
    LLSpinCtrl* mBurstSpeedMinSpinner{ nullptr };
    LLSpinCtrl* mBurstSpeedMaxSpinner{ nullptr };
    LLSpinCtrl* mStartAlphaSpinner{ nullptr };
    LLSpinCtrl* mEndAlphaSpinner{ nullptr };
    LLSpinCtrl* mScaleStartXSpinner{ nullptr };
    LLSpinCtrl* mScaleStartYSpinner{ nullptr };
    LLSpinCtrl* mScaleEndXSpinner{ nullptr };
    LLSpinCtrl* mScaleEndYSpinner{ nullptr };
    LLSpinCtrl* mSourceMaxAgeSpinner{ nullptr };
    LLSpinCtrl* mParticlesMaxAgeSpinner{ nullptr };
    LLSpinCtrl* mStartGlowSpinner{ nullptr };
    LLSpinCtrl* mEndGlowSpinner{ nullptr };

    LLComboBox* mBlendFuncSrcCombo{ nullptr };
    LLComboBox* mBlendFuncDestCombo{ nullptr };

    LLCheckBoxCtrl* mBounceCheckBox{ nullptr };
    LLCheckBoxCtrl* mEmissiveCheckBox{ nullptr };
    LLCheckBoxCtrl* mFollowSourceCheckBox{ nullptr };
    LLCheckBoxCtrl* mFollowVelocityCheckBox{ nullptr };
    LLCheckBoxCtrl* mInterpolateColorCheckBox{ nullptr };
    LLCheckBoxCtrl* mInterpolateScaleCheckBox{ nullptr };
    LLCheckBoxCtrl* mTargetPositionCheckBox{ nullptr };
    LLCheckBoxCtrl* mTargetLinearCheckBox{ nullptr };
    LLCheckBoxCtrl* mWindCheckBox{ nullptr };
    LLCheckBoxCtrl* mRibbonCheckBox{ nullptr };

    LLLineEditor* mTargetKeyInput{ nullptr };
    LLButton*     mClearTargetButton{ nullptr };
    LLButton*     mPickTargetButton{ nullptr };

    LLSpinCtrl* mAcellerationXSpinner{ nullptr };
    LLSpinCtrl* mAcellerationYSpinner{ nullptr };
    LLSpinCtrl* mAcellerationZSpinner{ nullptr };

    LLSpinCtrl* mOmegaXSpinner{ nullptr };
    LLSpinCtrl* mOmegaYSpinner{ nullptr };
    LLSpinCtrl* mOmegaZSpinner{ nullptr };

    LLColorSwatchCtrl* mStartColorSelector{ nullptr };
    LLColorSwatchCtrl* mEndColorSelector{ nullptr };

    LLButton* mCopyToLSLButton{ nullptr };
    LLButton* mInjectScriptButton{ nullptr };
};

class ParticleScriptCreationCallback : public LLInventoryCallback
{
public:
    ParticleScriptCreationCallback(ParticleEditor* editor);
    void fire(const LLUUID& inventoryItem);

protected:
    ~ParticleScriptCreationCallback() = default;

    ParticleEditor* mEditor;
};

#endif // PARTICLEEDITOR_H
