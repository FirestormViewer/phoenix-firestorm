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
	static void BindRenderTarget(LLRenderTarget* tgt, LLGLSLShader* shader, S32 uniform, S32 unit = 0, LLTexUnit::eTextureType mode = LLTexUnit::TT_TEXTURE);
};

class exoPostProcess : public LLSingleton<exoPostProcess>
{
	LLSINGLETON(exoPostProcess);
	~exoPostProcess();

private:
	static exoPostProcess *postProcess;

public:
	void ExodusRenderPostStack(LLRenderTarget* src, LLRenderTarget* dst);
	void ExodusRenderVignette(LLRenderTarget* src, LLRenderTarget* dst);
	void ExodusRenderPostUpdate();
	void ExodusRenderPostSettingsUpdate();

	void initVB();
	void destroyVB();

private:
	LLVector2 etc1;
	LLVector2 etc2;
	LLPointer<LLVertexBuffer>	mExoPostBuffer;

public:
	// Cached settings.
	static LLVector3	sExodusRenderVignette;
	
	S32					mShaderLevel;
};

#endif
