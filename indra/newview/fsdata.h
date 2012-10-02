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
 * THIS SOFTWARE IS PROVIDED BY MODULAR SYSTEMS AND CONTRIBUTORS AS IS
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

#include <map>
#include <llsd.h>
#include <llinstantmessage.h>
#include "llsingleton.h"

struct LLSDcontent
{
	LLSD content;
};

struct FSDataAgent
{
	bool support;
	bool developer;
};

class FSData : public LLSingleton<FSData>
{
	LOG_CLASS(FSData);
public:
	void startDownload();
	void processResponder(const LLSD& content, const std::string& url);
	void processData(const LLSD& fsData);
	void processReleases(const LLSD& releases);
	void processAgents(const LLSD& agents);
	void processClientTags(const LLSD& tags);
	static void msdata(U32 status, std::string body);
	static void msblacklist(U32 status, std::string body);

	void updateClientTagsLocal();
	LLSD resolveClientTag(LLUUID id);
	LLSD resolveClientTag(LLUUID id, bool new_system, LLColor4 new_system_color);

	static const U8 EM_SUPPORT		= 0x01;
	static const U8 EM_DEVELOPER	= 0x02;
	static const U8 PH_BETA			= 0x01;
	static const U8 PH_RELEASE		= 0x02;
	//static const U8 x = 0x04;
	//static const U8 x = 0x08;
	//static const U8 x = 0x10;

	std::map<LLUUID, U8> personnel;
	std::map<std::string, U8> versions2;
	std::set<LLUUID> mSupportGroup;

	BOOL is_BetaVersion(std::string version);
	BOOL is_ReleaseVersion(std::string version);
	bool is_developer(LLUUID avatar_id);
	bool is_support(LLUUID avatar_id);
	BOOL isSupportGroup(LLUUID id);
	FSDataAgent* getAgent(LLUUID avatar_id);

	LLSD allowed_login();

	std::string processRequestForInfo(LLUUID requester,std::string message, std::string name, LLUUID sessionid);
	static LLSD getSystemInfo();
	static void callbackReqInfo(const LLSD &notification, const LLSD &response);
	static void sendInfo(LLUUID destination, LLUUID sessionid, std::string myName, EInstantMessage dialog);
	
	LLSD LegacyClientList;
	
	std::map<std::string, LLSD> blocked_versions;

private:
	void processAgentsLLSD(const LLSD& agents);
	void processReleasesLLSD(const LLSD& releases);
	void saveLLSD(const LLSD& data, const std::string& filename);

	FSDataAgent mSupportAgent;
	std::map<LLUUID, FSDataAgent> mSupportAgentList;
};
