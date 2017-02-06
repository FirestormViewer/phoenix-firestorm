/** 
 * @file exopostprocess.h
 * @brief exoPostProcess class definition
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

#ifndef exopostprocess_h
#define exopostprocess_h

#include "pipeline.h"
#include "llviewershadermgr.h"

/**
 * exoShader is just a few helper functions for binding different things to shaders.
 * Not really enough functionality here yet to warrant its own file.
 */

class exoShader {
public:
	static void BindTex2D(LLTexture *tex2D, LLGLSLShader *shader, S32 uniform, S32 unit = 0, LLTexUnit::eTextureType mode = LLTexUnit::TT_TEXTURE, LLTexUnit::eTextureAddressMode addressMode = LLTexUnit::TAM_CLAMP, LLTexUnit::eTextureFilterOptions filterMode = LLTexUnit::TFO_BILINEAR);
	static void BindRenderTarget(LLRenderTarget* tgt, LLGLSLShader* shader, S32 uniform, S32 unit = 0, LLTexUnit::eTextureType mode = LLTexUnit::TT_TEXTURE);
};

class exoPostProcess : public LLSingleton<exoPostProcess>
{
	LLSINGLETON(exoPostProcess);
	~exoPostProcess();

private:
	static exoPostProcess *postProcess;

public:
    enum ExodusRenderPostType
    {
        //EXODUS_RENDER_GAMMA_POST            = 1,
        //EXODUS_RENDER_GAMMA_PRE             = 2,
        //EXODUS_RENDER_COLOR_GRADE           = 4,
        //EXODUS_RENDER_TONE_LINEAR           = 8,
        //EXODUS_RENDER_TONE_REINHARD         = 10,
        //EXODUS_RENDER_TONE_FILMIC           = 20,
        //EXODUS_RENDER_COLOR_GRADE_LEGACY    = 40,
        EXODUS_RENDER_VIGNETTE_POST         = 80,
        //EXODUS_RENDER_TONE_FILMIC_ADV       = 100
    };
	
	void ExodusSetPostAttribute(U32 attribute, void* value = NULL);
	void ExodusRenderPostStack(LLRenderTarget* src, LLRenderTarget* dst);
    void ExodusRenderPost(LLRenderTarget* src, LLRenderTarget* dst, S32 type);
    void ExodusRenderVignette(LLRenderTarget* src, LLRenderTarget* dst);
    void ExodusRenderPostUpdate();
	void ExodusRenderPostSettingsUpdate();
	
    BOOL multisample;
    U32 mGammaFunc;
	U32 mInvGammaFunc;
private:
    LLVector2 etc1;
    LLVector2 etc2;
	LLPointer<LLVertexBuffer>	mExoPostBuffer;
	
protected:
	LLRenderTarget				mTex2DTargetBuffer; // Need this for texture2Dlod functionality.
public:
	// Cached settings.
	static LLVector3	sExodusRenderVignette;
	
	S32					mVertexShaderLevel;
};

#endif
