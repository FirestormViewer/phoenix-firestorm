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

LLVector3 exoPostProcess::sExodusRenderVignette;

exoPostProcess::exoPostProcess()
{
	initVB();
	sExodusRenderVignette = LLVector3(0.f, 0.f, 0.f);
}

exoPostProcess::~exoPostProcess()
{
	destroyVB();
}

void exoPostProcess::ExodusRenderPostStack(LLRenderTarget *src, LLRenderTarget *dst)
{
	if (mShaderLevel > 0 && sExodusRenderVignette.mV[0] > 0.f && LLPipeline::sRenderDeferred)
	{
		ExodusRenderVignette(src, dst);
	}
}

void exoPostProcess::ExodusRenderPostSettingsUpdate()
{
	mShaderLevel = LLViewerShaderMgr::instance()->getShaderLevel(LLViewerShaderMgr::SHADER_AVATAR);
	sExodusRenderVignette = gSavedSettings.getVector3("FSRenderVignette");
}

void exoPostProcess::ExodusRenderPostUpdate()
{
	if (!gPipeline.mRT)
		return;

	etc1.setVec(0.f, 0.f);
	etc2.setVec((F32) gPipeline.mRT->screen.getWidth(), (F32) gPipeline.mRT->screen.getHeight());
	initVB();
}

void exoPostProcess::initVB()
{
	if (!gPipeline.sRenderDeferred)
		return;

	destroyVB();
	mExoPostBuffer = new LLVertexBuffer(LLVertexBuffer::MAP_VERTEX | LLVertexBuffer::MAP_TEXCOORD0 | LLVertexBuffer::MAP_TEXCOORD1);
	if (mExoPostBuffer->allocateBuffer(8, 0))
	{
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
	else
	{
		LL_WARNS() << "Failed to allocate Vertex Buffer for exoPostProcessing" << LL_ENDL;
		destroyVB();
	}
}

void exoPostProcess::destroyVB()
{
	mExoPostBuffer = nullptr;
}

void exoPostProcess::ExodusRenderVignette(LLRenderTarget* src, LLRenderTarget* dst)
{
	if (mExoPostBuffer)
	{
		dst->bindTarget();
		LLGLSLShader *shader = &gPostVignetteProgram;
		shader->bind();

		mExoPostBuffer->setBuffer();

		exoShader::BindRenderTarget(dst, shader, LLShaderMgr::EXO_RENDER_SCREEN);

		shader->uniform3fv(LLShaderMgr::EXO_RENDER_VIGNETTE, 1, sExodusRenderVignette.mV);
		mExoPostBuffer->drawArrays(LLRender::TRIANGLES, 0, 3);
		stop_glerror();

		shader->unbind();
		dst->flush();
	}
}

void exoShader::BindRenderTarget(LLRenderTarget* tgt, LLGLSLShader* shader, S32 uniform, S32 unit, LLTexUnit::eTextureType mode)
{
	if (gPipeline.sRenderDeferred)
	{
		S32 channel = channel = shader->enableTexture(uniform, tgt->getUsage());
		if (channel > -1)
		{
			tgt->bindTexture(0, channel);
			gGL.getTexUnit(channel)->setTextureFilteringOption(LLTexUnit::TFO_POINT);
		}
		shader->uniform2f(sScreen_Res, tgt->getWidth(), tgt->getHeight());
	}
}
