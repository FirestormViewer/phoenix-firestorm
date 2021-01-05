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

public:
	bool hitTest(const LLCoordGL& ptMouse) const;
	void run() override;
	void tweenAlpha(float endAlpha, double duration)    { m_nAlpha.start(endAlpha, duration); }
	void tweenColor(LLColor3 endColor, double duration) { m_Color.start(endColor, duration); }
	static ERlvCmdRet onAlphaValueChanged(const LLUUID& idRlvObj, const boost::optional<RlvBehaviourModifierValue> newValue);
	static ERlvCmdRet onBlockTouchValueChanged(const LLUUID& idRlvObj, const boost::optional<RlvBehaviourModifierValue> newValue);
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

public:
	void run() override;
	static ERlvCmdRet onColorChanged(const LLUUID& idRlvObj, const boost::optional<RlvBehaviourModifierValue> newValue);
	static ERlvCmdRet onMinAlphaChanged(const LLUUID& idRlvObj, const boost::optional<RlvBehaviourModifierValue> newValue);
	static ERlvCmdRet onMaxAlphaChanged(const LLUUID& idRlvObj, const boost::optional<RlvBehaviourModifierValue> newValue);
	static ERlvCmdRet onMinDistChanged(const LLUUID& idRlvObj, const boost::optional<RlvBehaviourModifierValue> newValue);
	static ERlvCmdRet onMaxDistChanged(const LLUUID& idRlvObj, const boost::optional<RlvBehaviourModifierValue> newValue);
protected:
	void setShaderUniforms(LLGLSLShader* pShader, LLRenderTarget* pRenderTarget);

	/*
	 * Member variables
	 */
protected:
	LLColor3 m_Color;
	float    m_nMinAlpha;
	float    m_nMaxAlpha;
	float    m_nMinDistance;
	float    m_nMaxDistance;
};

// ====================================================================================
