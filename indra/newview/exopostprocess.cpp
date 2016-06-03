/** 
 * @file exopostprocess.cpp
 *
 * @brief This implements the Exodus post processing chain.
 *
 * $LicenseInfo:firstyear=2011&license=viewerlgpl$
 * Copyright (C) 2011 Geenz Spad
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
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "exopostprocess.h"
#include "llviewershadermgr.h"
#include "pipeline.h"
#include "llviewercontrol.h"

static LLStaticHashedString sScreen_Res("screen_res");

LLVector3	exoPostProcess::sExodusRenderVignette;

exoPostProcess::exoPostProcess()
{
	mExoPostBuffer = new LLVertexBuffer(LLVertexBuffer::MAP_VERTEX | LLVertexBuffer::MAP_TEXCOORD0 | LLVertexBuffer::MAP_TEXCOORD1, 0);
	mExoPostBuffer->allocateBuffer(8, 0, true);

	LLStrider<LLVector3> vert; 
	mExoPostBuffer->getVertexStrider(vert);
	LLStrider<LLVector2> tc0;
	LLStrider<LLVector2> tc1;
	mExoPostBuffer->getTexCoord0Strider(tc0);
	mExoPostBuffer->getTexCoord1Strider(tc1);

	vert[0].set(-1.f, 1.f, 0.f);
	vert[1].set(-1.f, -3.f, 0.f);
	vert[2].set(3.f,1.f, 0.f);
	
	sExodusRenderVignette = LLVector3(0.f, 0.f, 0.f);
}

exoPostProcess::~exoPostProcess()
{
	mExoPostBuffer = NULL;
}

void exoPostProcess::ExodusRenderPostStack(LLRenderTarget *src, LLRenderTarget *dst)
{
	if (mVertexShaderLevel > 0)
	{
		if (sExodusRenderVignette.mV[0] > 0 && LLPipeline::sRenderDeferred)
			ExodusRenderVignette(src, dst); // Don't render vignette here in non-deferred. Do it in the glow combine shader.
	}
}
void exoPostProcess::ExodusRenderPostSettingsUpdate()
{
	mVertexShaderLevel = LLViewerShaderMgr::instance()->getVertexShaderLevel(LLViewerShaderMgr::SHADER_AVATAR);
	sExodusRenderVignette = gSavedSettings.getVector3("FSRenderVignette");
}
void exoPostProcess::ExodusRenderPostUpdate()
{
	etc1.setVec(0.f, 0.f);
	etc2.setVec((F32) gPipeline.mScreen.getWidth(),
				(F32) gPipeline.mScreen.getHeight());
	if (!gPipeline.sRenderDeferred)
	{
		// Destroy our old buffer, and create a new vertex buffer for the screen (shamelessly ganked from pipeline.cpp).
		mExoPostBuffer = NULL;
		mExoPostBuffer = new LLVertexBuffer(LLVertexBuffer::MAP_VERTEX | LLVertexBuffer::MAP_TEXCOORD0 | LLVertexBuffer::MAP_TEXCOORD1, 0);
		mExoPostBuffer->allocateBuffer(3,0,TRUE);

		LLStrider<LLVector3> v;
		LLStrider<LLVector2> uv1;
		LLStrider<LLVector2> uv2;

		mExoPostBuffer->getVertexStrider(v);
		mExoPostBuffer->getTexCoord0Strider(uv1);
		mExoPostBuffer->getTexCoord1Strider(uv2);
		
		uv1[0] = LLVector2(0.f, 0.f);
		uv1[1] = LLVector2(0.f, 2.f);
		uv1[2] = LLVector2(2.f, 0.f);
		
		uv2[0] = LLVector2(0.f, 0.f);
		uv2[1] = LLVector2(0.f, etc2.mV[1] * 2.f);
		uv2[2] = LLVector2(etc2.mV[0] * 2.f, 0.f);
		
		v[0] = LLVector3(-1.f, -1.f, 0.f);
		v[1] = LLVector3(-1.f, 3.f, 0.f);
		v[2] = LLVector3(3.f, -1.f, 0.f);

		mExoPostBuffer->flush();
	}
	else
	{
		mExoPostBuffer = NULL;
		mExoPostBuffer = new LLVertexBuffer(LLVertexBuffer::MAP_VERTEX | LLVertexBuffer::MAP_TEXCOORD0 | LLVertexBuffer::MAP_TEXCOORD1, 0);
		mExoPostBuffer->allocateBuffer(8, 0, true);

		LLStrider<LLVector3> vert; 
		mExoPostBuffer->getVertexStrider(vert);
		LLStrider<LLVector2> tc0;
		LLStrider<LLVector2> tc1;
		mExoPostBuffer->getTexCoord0Strider(tc0);
		mExoPostBuffer->getTexCoord1Strider(tc1);

		vert[0].set(-1.f, 1.f, 0.f);
		vert[1].set(-1.f, -3.f, 0.f);
		vert[2].set(3.f, 1.f, 0.f);
	}
}

void exoPostProcess::ExodusRenderPost(LLRenderTarget* src, LLRenderTarget* dst, S32 type)
{
	if (type == EXODUS_RENDER_VIGNETTE_POST)
	{
		ExodusRenderVignette(src, dst);
	}
}

void exoPostProcess::ExodusRenderVignette(LLRenderTarget* src, LLRenderTarget* dst)
{
	dst->bindTarget();
	LLGLSLShader *shader = &gPostVignetteProgram;
	shader->bind();

	mExoPostBuffer->setBuffer(LLVertexBuffer::MAP_VERTEX);

	exoShader::BindRenderTarget(dst, shader, LLShaderMgr::EXO_RENDER_SCREEN);

	shader->uniform3fv(LLShaderMgr::EXO_RENDER_VIGNETTE, 1, sExodusRenderVignette.mV);
	mExoPostBuffer->drawArrays(LLRender::TRIANGLES, 0, 3);
	stop_glerror();

	shader->unbind();
	dst->flush();
}

void exoShader::BindTex2D(LLTexture *tex2D, LLGLSLShader *shader, S32 uniform, S32 unit, LLTexUnit::eTextureType mode, LLTexUnit::eTextureAddressMode addressMode, LLTexUnit::eTextureFilterOptions filterMode)
{
	if (gPipeline.sRenderDeferred)
	{
		S32 channel = 0;
		channel = shader->enableTexture(uniform);
		if (channel > -1)
		{
			gGL.getTexUnit(channel)->bind(tex2D);
			gGL.getTexUnit(channel)->setTextureFilteringOption(filterMode);
			gGL.getTexUnit(channel)->setTextureAddressMode(addressMode);
		}
	}
	else
	{
		gGL.getTexUnit(unit)->bind(tex2D);
	}
}

void exoShader::BindRenderTarget(LLRenderTarget* tgt, LLGLSLShader* shader, S32 uniform, S32 unit, LLTexUnit::eTextureType mode)
{
	if (gPipeline.sRenderDeferred)
	{
		S32 channel = 0;
		channel = shader->enableTexture(uniform, tgt->getUsage());
		if (channel > -1)
		{
			tgt->bindTexture(0,channel);
			gGL.getTexUnit(channel)->setTextureFilteringOption(LLTexUnit::TFO_POINT);
		}
	}
	else
	{
		S32 reftex = shader->enableTexture(uniform, tgt->getUsage());
		if (reftex > -1)
		{
			gGL.getTexUnit(reftex)->activate();
			gGL.getTexUnit(reftex)->bind(tgt);
			gGL.getTexUnit(0)->activate();
		}
	}
	shader->uniform2f(sScreen_Res, tgt->getWidth(), tgt->getHeight());
}
