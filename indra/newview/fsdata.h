/**
 * @file fsdata.h
 * @brief Downloadable dymatic xml data for viewer features.
 *
 * $LicenseInfo:firstyear=2013&license=viewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (C) 2011-2013 Techwolf Lupindo
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
 * 
 * The Phoenix Firestorm Project, Inc., 1831 Oakwood Drive, Fairmont, Minnesota 56031-3225 USA
 * http://www.firestormviewer.org
 * $/LicenseInfo$
 */

#ifndef FS_DATA_H
#define FS_DATA_H

#include <map>

#include "llinstantmessage.h"
#include "llsd.h"
#include "llsingleton.h"

class FSData : public LLSingleton<FSData>
{
	friend class LLSingleton<FSData>;
	LOG_CLASS(FSData);
public:
	FSData();

	void startDownload();
	void downloadAgents();
	void processResponder(const LLSD& content, const std::string& url, bool save_to_file, const LLDate& last_modified);
	void addAgents();

	LLSD resolveClientTag(LLUUID id);
	LLSD resolveClientTag(LLUUID id, bool new_system, LLColor4 new_system_color);
	
	enum flags_t
	{
		SUPPORT		= (1 << 0), //0x01
		DEVELOPER	= (1 << 1), //0x02
		QA		= (1 << 2), //0x04
		CHAT_COLOR	= (1 << 3), //0x08
		NO_SUPPORT	= (1 << 4), //0x16
		NO_USE		= (1 << 5), //0x32
		NO_SPAM		= (1 << 6)  //0x64
	};

	std::set<LLUUID> mSupportGroup;

	bool isDeveloper(LLUUID avatar_id);
	bool isSupport(LLUUID avatar_id);
	BOOL isSupportGroup(LLUUID id); // BOOL is used due to used in a LL function.
	// returns -1 if agent is not found.
	S32 getAgentFlags(LLUUID avatar_id);

	LLSD allowedLogin();
	
	bool enableLegacySearch() {return mLegacySearch;}

	std::string processRequestForInfo(LLUUID requester,std::string message, std::string name, LLUUID sessionid);
	static LLSD getSystemInfo();
	static void callbackReqInfo(const LLSD &notification, const LLSD &response);
	
	std::string getOpenSimMOTD() { return mOpensimMOTD; }
	bool getFSDataDone() { return mFSDataDone; }
	bool getAgentsDone() { return mAgentsDone; }
	
	bool isAgentFlag(const LLUUID& agent_id, FSData::flags_t flag);
	

private:
	static void sendInfo(LLUUID destination, LLUUID sessionid, std::string myName, EInstantMessage dialog);
	void processAssets(const LLSD& assets);
	void processAgents(const LLSD& data);
	void saveLLSD(const LLSD& data, const std::string& filename, const LLDate& last_modified);
	bool loadFromFile(LLSD& data, std::string filename);
	void processData(const LLSD& fs_data);
	void processClientTags(const LLSD& tags);
	void updateClientTagsLocal();

	std::map<LLUUID, S32> mSupportAgents;
	std::map<std::string, LLSD> mBlockedVersions;

	LLSD mHeaders;
	LLSD mLegacyClientList;
	
	std::string mFSdataFilename;
	std::string mAgentsFilename;
	std::string mAssestsFilename;
	std::string mClientTagsFilename;
	
	std::string mAgentsURL;
	std::string mAssetsURL;
	
	std::string mOpensimMOTD;
	
	bool mLegacySearch;
	bool mFSDataDone;
	bool mAgentsDone;
};

#endif // FS_DATA_H
