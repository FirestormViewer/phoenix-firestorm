/* Copyright (c) 2010
 *
 * Modular Systems All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the following
 * conditions are met:
 *
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *   3. Neither the name Modular Systems nor the names of its contributors
 *      may be used to endorse or promote products derived from this
 *      software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY MODULAR SYSTEMS AND CONTRIBUTORS “AS IS”
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MODULAR SYSTEMS OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef FS_LSLPREPROC_H
#define FS_LSLPREPROC_H

#include "llviewerprecompiledheaders.h"
#include "llpreviewscript.h"

#define DARWINPREPROC
//force preproc on mac

class FSLSLPreprocessor
{
public:

	FSLSLPreprocessor(LLScriptEdCore* corep)
		: mCore(corep), waving(FALSE), mClose(FALSE)
	{}

	static bool mono_directive(std::string const& text, bool agent_inv = true);
	std::string encode(std::string script);
	std::string decode(std::string script);

	std::string lslopt(std::string script);
	std::string lslcomp(std::string script);

	static LLUUID findInventoryByName(std::string name);
	static void FSProcCacheCallback(LLVFS *vfs, const LLUUID& uuid, LLAssetType::EType type,
									void *userdata, S32 result, LLExtStat extstat);
	void preprocess_script(BOOL close = FALSE, BOOL defcache = FALSE);
	void start_process();
	void display_error(std::string err);

	std::string uncollide_string_literals(std::string script);

	//dual function, determines if files have been modified this session and if we have cached them
	//also assetids exposed in-preprocessing as a predefined macro for use in include once style include files, e.g. #define THISFILE file_ ## __ASSETIDRAW__
	//in case it isn't obvious, the viewer only sets the asset id on a successful script save (of a full perm script), or in preproc on-cache
	//so this is only applicable to fully permissive scripts; which is just fine, since if it isn't full perm it isn't really useful as a include anyway.
	//in the event of a no-trans script (only less than full thats readable), the server sends null key, and we will set a random uuid. 
	//This uuid should be overwritten if they edit that script, whether with the real id or null key is irrelevant in this case.
	//theoretically, if the asset IDs were exposed for full perm scripts without downloading the script at least once, it would save unnecessary caching
	//as this isn't the case I'm not going to preserve this structure across logins.

	//(it seems rather dumb that readable scripts don't show the asset id without a DL, but thats beside the point.)
	static std::map<std::string,LLUUID> cached_assetids;

	static std::map<std::string,std::string> decollided_literals;

	std::set<std::string> caching_files;
	std::set<std::string> defcached_files;
	bool mDefinitionCaching;

	LLScriptEdCore* mCore;
	BOOL waving;
	BOOL mClose;
	BOOL mHDDInclude;
	std::string mMainScriptName;
};

#endif // FS_LSLPREPROC_H