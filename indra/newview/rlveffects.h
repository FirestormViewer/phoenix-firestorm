/**
 *
 * Copyright (c) 2021, Kitty Barnett
 *
 * The source code in this file is provided to you under the terms of the
 * GNU Lesser General Public License, version 2.1, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE. Terms of the LGPL can be found in doc/LGPL-licence.txt
 * in this distribution, or online at http://www.gnu.org/licenses/lgpl-2.1.txt
 *
 * By copying, modifying or distributing this software, you acknowledge that
 * you have read and understood your obligations described above, and agree to
 * abide by those obligations.
 *
 */

#pragma once

#include "llvisualeffect.h"
#include "rlvhelper.h"

// ============================================================================
// Forward declarations
//

class LLViewerFetchedTexture;

// ====================================================================================
// RlvOverlayEffect class
//

class RlvOverlayEffect : public LLVisualEffect
{
public:
    RlvOverlayEffect(const LLUUID& idRlvObj);
    ~RlvOverlayEffect();

    static const EVisualEffect EffectCode = EVisualEffect::RlvOverlay;
public:
    bool hitTest(const LLCoordGL& ptMouse) const;
    void run(const LLVisualEffectParams*) override;
    void setBlockTouch(bool block_touch)                { m_fBlockTouch = block_touch; }
    void tweenAlpha(float endAlpha, double duration)    { m_nAlpha.start(endAlpha, duration); }
    void tweenColor(LLColor3 endColor, double duration) { m_Color.start(endColor, duration); }
    static ERlvCmdRet onAlphaValueChanged(const LLUUID& idRlvObj, const boost::optional<RlvBehaviourModifierValue> newValue);
    static ERlvCmdRet onColorValueChanged(const LLUUID& idRlvObj, const boost::optional<RlvBehaviourModifierValue> newValue);
    static ERlvCmdRet onTextureChanged(const LLUUID& idRlvObj, const boost::optional<RlvBehaviourModifierValue> newValue);
protected:
    void clearImage();
    void setImage(const LLUUID& idTexture);

    /*
     * Member variables
     */
protected:
    LLTweenableValueLerp<float>    m_nAlpha;
    bool                           m_fBlockTouch;
    LLTweenableValueLerp<LLColor3> m_Color;

    LLPointer<LLViewerFetchedTexture> m_pImage = nullptr;
    int      m_nImageOrigBoost = 0;
};

// ====================================================================================
// RlvSphereEffect class
//

class RlvSphereEffect : public LLVisualEffect
{
public:
    RlvSphereEffect(const LLUUID& idRlvObj);
    ~RlvSphereEffect();

    static const EVisualEffect EffectCode = EVisualEffect::RlvSphere;
public:
    void run(const LLVisualEffectParams* pParams) override;
    static ERlvCmdRet onModeChanged(const LLUUID& idRlvObj, const boost::optional<RlvBehaviourModifierValue> newValue);
    static ERlvCmdRet onOriginChanged(const LLUUID& idRlvObj, const boost::optional<RlvBehaviourModifierValue> newValue);
    static ERlvCmdRet onColorChanged(const LLUUID& idRlvObj, const boost::optional<RlvBehaviourModifierValue> newValue);
    static ERlvCmdRet onDistMinChanged(const LLUUID& idRlvObj, const boost::optional<RlvBehaviourModifierValue> newValue);
    static ERlvCmdRet onDistMaxChanged(const LLUUID& idRlvObj, const boost::optional<RlvBehaviourModifierValue> newValue);
    static ERlvCmdRet onDistExtendChanged(const LLUUID& idRlvObj, const boost::optional<RlvBehaviourModifierValue> newValue);
    static ERlvCmdRet onParamsChanged(const LLUUID& idRlvObj, const boost::optional<RlvBehaviourModifierValue> newValue);
    static ERlvCmdRet onTweenDurationChanged(const LLUUID& idRlvObj, const boost::optional<RlvBehaviourModifierValue> newValue);
    static ERlvCmdRet onValueMinChanged(const LLUUID& idRlvObj, const boost::optional<RlvBehaviourModifierValue> newValue);
    static ERlvCmdRet onValueMaxChanged(const LLUUID& idRlvObj, const boost::optional<RlvBehaviourModifierValue> newValue);
protected:
    void renderPass(LLGLSLShader* pShader, const LLShaderEffectParams* pParams) const;
    void setShaderUniforms(LLGLSLShader* pShader);

    /*
     * Member variables
     */
protected:
    enum class ESphereMode { Blend = 0, Blur, BlurVariable, ChromaticAberration, Pixelate, Count };
    ESphereMode   m_eMode;
    enum class ESphereOrigin { Avatar = 0, Camera, Count };
    ESphereOrigin m_eOrigin;
    LLTweenableValueLerp<LLVector4> m_Params;
    LLTweenableValueLerp<float>     m_nDistanceMin;
    LLTweenableValueLerp<float>     m_nDistanceMax;
    enum class ESphereDistExtend { Max = 0x01, Min = 0x02, Both = 0x03 };
    ESphereDistExtend m_eDistExtend;
    LLTweenableValueLerp<float>     m_nValueMin;
    LLTweenableValueLerp<float>     m_nValueMax;
    float                           m_nTweenDuration;
};

// ====================================================================================
