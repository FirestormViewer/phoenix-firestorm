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

#include "llviewerprecompiledheaders.h"

#include "llglslshader.h"
#include "llrender2dutils.h"
#include "llviewertexturelist.h"
#include "llviewerwindow.h"

#include "rlveffects.h"
#include "rlvhandler.h"

// ====================================================================================
// RlvOverlayEffect class
//

const float c_DefaultAlpha = 1.0f;
const float c_DefaultColor[3] = { 1.0f, 1.0f, 1.0f };

RlvOverlayEffect::RlvOverlayEffect(const LLUUID& idRlvObj)
	: LLVisualEffect(idRlvObj, EVisualEffect::RlvOverlay, EVisualEffectType::Custom)
	, m_nAlpha(c_DefaultAlpha)
	, m_fBlockTouch(false)
	, m_Color(LLColor3(c_DefaultColor))
{
	if (RlvObject* pRlvObj = gRlvHandler.getObject(idRlvObj))
	{
		float nAlpha;
		if (pRlvObj->getModifierValue<float>(ERlvLocalBhvrModifier::OverlayAlpha, nAlpha))
			m_nAlpha = nAlpha;

		pRlvObj->getModifierValue<bool>(ERlvLocalBhvrModifier::OverlayTouch, m_fBlockTouch);

		LLVector3 vecColor;
		if (pRlvObj->getModifierValue<LLVector3>(ERlvLocalBhvrModifier::OverlayTint, vecColor))
			m_Color = LLColor3(vecColor.mV);

		LLUUID idTexture;
		if ( (pRlvObj) && (pRlvObj->getModifierValue<LLUUID>(ERlvLocalBhvrModifier::OverlayTexture, idTexture)) )
			setImage(idTexture);
	}
}

RlvOverlayEffect::~RlvOverlayEffect()
{
	clearImage();
}

// static
ERlvCmdRet RlvOverlayEffect::onAlphaValueChanged(const LLUUID& idRlvObj, const boost::optional<RlvBehaviourModifierValue> newValue)
{
	if (RlvOverlayEffect* pEffect = dynamic_cast<RlvOverlayEffect*>(LLVfxManager::instance().getEffect(idRlvObj)))
	{
		pEffect->m_nAlpha = (newValue) ? boost::get<float>(newValue.value()) : c_DefaultAlpha;
	}
	return RLV_RET_SUCCESS;
}

// static
ERlvCmdRet RlvOverlayEffect::onBlockTouchValueChanged(const LLUUID& idRlvObj, const boost::optional<RlvBehaviourModifierValue> newValue)
{
	if (RlvOverlayEffect* pEffect = dynamic_cast<RlvOverlayEffect*>(LLVfxManager::instance().getEffect(idRlvObj)))
	{
		pEffect->m_fBlockTouch = (newValue) ? boost::get<bool>(newValue.value()) : false;
	}
	return RLV_RET_SUCCESS;
}
// static
ERlvCmdRet RlvOverlayEffect::onColorValueChanged(const LLUUID& idRlvObj, const boost::optional<RlvBehaviourModifierValue> newValue)
{
	if (RlvOverlayEffect* pEffect = dynamic_cast<RlvOverlayEffect*>(LLVfxManager::instance().getEffect(idRlvObj)))
	{
		pEffect->m_Color = LLColor3( (newValue) ? boost::get<LLVector3>(newValue.value()).mV : c_DefaultColor);
	}
	return RLV_RET_SUCCESS;
}

// static
ERlvCmdRet RlvOverlayEffect::onTextureChanged(const LLUUID& idRlvObj, const boost::optional<RlvBehaviourModifierValue> newValue)
{
	if (RlvOverlayEffect* pEffect = dynamic_cast<RlvOverlayEffect*>(LLVfxManager::instance().getEffect(idRlvObj)))
	{
		if (newValue)
			pEffect->setImage(boost::get<LLUUID>(newValue.value()));
		else
			pEffect->clearImage();
	}
	return RLV_RET_SUCCESS;
}

void RlvOverlayEffect::clearImage()
{
	if (m_pImage)
	{
		m_pImage->setBoostLevel(m_nImageOrigBoost);
		m_pImage = nullptr;
	}
}

bool RlvOverlayEffect::hitTest(const LLCoordGL& ptMouse) const
{
	if (!m_pImage)
		return false;

	return (m_fBlockTouch) && (m_pImage->getMask(LLVector2((float)ptMouse.mX / gViewerWindow->getWorldViewWidthScaled(), (float)ptMouse.mY / gViewerWindow->getWorldViewHeightScaled())));
}

void RlvOverlayEffect::setImage(const LLUUID& idTexture)
{
	if ( (m_pImage) && (m_pImage->getID() == idTexture) )
		return;

	clearImage();
	m_pImage = LLViewerTextureManager::getFetchedTexture(idTexture, FTT_DEFAULT, MIPMAP_YES, LLGLTexture::BOOST_NONE, LLViewerTexture::LOD_TEXTURE);
	m_nImageOrigBoost = m_pImage->getBoostLevel();
	m_pImage->setBoostLevel(LLGLTexture::BOOST_PREVIEW);
	m_pImage->forceToSaveRawImage(0);
}

void RlvOverlayEffect::run()
{
	if (m_pImage)
	{
		if (LLGLSLShader::sNoFixedFunction)
		{
			gUIProgram.bind();
		}

		int nWidth = gViewerWindow->getWorldViewWidthScaled();
		int nHeight = gViewerWindow->getWorldViewHeightScaled();

		m_pImage->addTextureStats(nWidth * nHeight);
		m_pImage->setKnownDrawSize(nWidth, nHeight);

		gGL.pushMatrix();
		LLGLSUIDefault glsUI;
		gViewerWindow->setup2DRender();

		const LLVector2& displayScale = gViewerWindow->getDisplayScale();
		gGL.scalef(displayScale.mV[VX], displayScale.mV[VY], 1.f);

		gGL.getTexUnit(0)->bind(m_pImage);
		const LLColor3 col = m_Color.get();
		gGL.color4f(col.mV[0], col.mV[1], col.mV[2], llclamp(m_nAlpha.get(), 0.0f, 1.0f));

		gl_rect_2d_simple_tex(nWidth, nHeight);

		gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);

		gGL.popMatrix();
		gGL.flush();
		gViewerWindow->setup3DRender();

		if (LLGLSLShader::sNoFixedFunction)
		{
			gUIProgram.unbind();
		}
	}
}

// ====================================================================================
