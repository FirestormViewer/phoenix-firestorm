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
 * THIS SOFTWARE IS PROVIDED BY MODULAR SYSTEMS AND CONTRIBUTORS "AS IS"
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

#include "llviewerprecompiledheaders.h"

#include "fsdata.h"

#include "llbufferstream.h"

#include "llappviewer.h"

#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <boost/lexical_cast.hpp>

#include "llsdserialize.h"

#include "llversionviewer.h"
#include "llprimitive.h"
#include "llagent.h"
#include "llnotifications.h"
#include "llimview.h"
#include "llfloaterabout.h"
#include "llviewercontrol.h"
//#include "floaterblacklist.h"
#include "llsys.h"
#include "llviewermedia.h"
#include "llagentui.h"
#include "llversioninfo.h"
#include "lltrans.h"

static const std::string versionid = llformat("%s %d.%d.%d (%d)", LL_CHANNEL, LL_VERSION_MAJOR, LL_VERSION_MINOR, LL_VERSION_PATCH, LL_VERSION_BUILD);
static const std::string fsdata_url = "http://phoenixviewer.com/app/fsdata/data.xml";
static const std::string releases_url = "http://phoenixviewer.com/app/fsdata/releases.xml";
static const std::string agents_url = "http://phoenixviewer.com/app/fsdata/agents.xml";
static const std::string legacy_client_list = "http://phoenixviewer.com/app/client_tags/client_list_v2.xml";

//static const std::string blacklist_url = "http://phoenixviewer.com/app/fsdata/blacklist.xml";


class FSDownloader : public LLHTTPClient::Responder
{
	LOG_CLASS(FSDownloader);
public:
	FSDownloader(std::string url) :
		mURL(url)
	{
	}
	
	virtual void completedRaw(U32 status,
				const std::string& reason,
				const LLChannelDescriptors& channels,
				const LLIOPipe::buffer_ptr_t& buffer)
	{
		if (!isGoodStatus(status))
		{
			llinfos << mURL << " [" << status << "]: " << reason << llendl;
			if (mURL == legacy_client_list)
			{
				LL_WARNS("ClientTags") << "client_list_v2.xml download failed with status of " << status << LL_ENDL;
				// Wolfspirit: If something failes, try to use the local file
				FSData::getInstance()->updateClientTagsLocal();
			}
			return;
		}
	  
		LLSD content;
		LLBufferStream istr(channels, buffer.get());
		if (!LLSDSerialize::fromXML(content, istr))
		{
			llinfos << "Failed to deserialize LLSD. " << mURL << " [" << status << "]: " << reason << llendl;
			if (mURL == legacy_client_list)
			{
				LL_WARNS("ClientTags") << "Downloaded client_list_v2.xml decode failed." << LL_ENDL;
				// Wolfspirit: If something failes, try to use the local file
				FSData::getInstance()->updateClientTagsLocal();
			}
			return;
		}
		
		FSData::getInstance()->processResponder(content, mURL);
	}
	
private:
	std::string mURL;
};

void FSData::processResponder(const LLSD& content, const std::string& url)
{
	if (url == fsdata_url)
	{
		processData(content);
	}
	else if (url == releases_url)
	{
		processReleases(content);
	}
	else if (url == agents_url)
	{
		processAgents(content);
	}
	else if (url == legacy_client_list)
	{
		processClientTags(content);
	}
}

void FSData::startDownload()
{
	LLSD headers;
	headers.insert("User-Agent", LLViewerMedia::getCurrentUserAgent());
	headers.insert("viewer-version", versionid);
	LL_INFOS("Data") << "Downloading data.xml" << LL_ENDL;
	LLHTTPClient::get(fsdata_url,new FSDownloader(fsdata_url),headers);
}
	
void FSData::processData(const LLSD& fsData)
{
	// Set Message Of The Day if present
	if(fsData.has("MOTD"))
	{
		gAgent.mMOTD.assign(fsData["MOTD"]);
	}

	bool local_file = false;
	if (!(fsData["Releases"].asInteger() == 0))
	{
		const std::string releases_filename = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, "releases.xml");
		llifstream releases_file(releases_filename);
		LLSD releases;
		if(releases_file.is_open())
		{
			if(LLSDSerialize::fromXML(releases, releases_file) >= 1)
			{
				if(releases.has("ReleaseVersion"))
				{
					if (fsData["Releases"].asInteger() <= releases["ReleaseVersion"].asInteger())
					{
						processReleasesLLSD(releases);
						local_file = true;
					}
				}
			}
			releases_file.close();
		}
	}

	if (!local_file)
	{
		LL_INFOS("Data") << "Downloading " << releases_url << LL_ENDL;
		LLHTTPClient::get(releases_url,new FSDownloader(releases_url));
	}


	local_file = false;
	if (!(fsData["Agents"].asInteger() == 0))
	{
		const std::string agents_filename = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, "agents.xml");
		llifstream agents_file(agents_filename);
		LLSD agents;
		if(agents_file.is_open())
		{
			if(LLSDSerialize::fromXML(agents, agents_file) >= 1)
			{
				if(agents.has("AgentsVersion"))
				{
					if (fsData["Agents"].asInteger() <= agents["AgentsVersion"].asInteger())
					{
						processAgentsLLSD(agents);
						local_file = true;
					}
				}
			}
			agents_file.close();
		}
	}

	if (!local_file)
	{
		LL_INFOS("Data") << "Downloading " << agents_url << LL_ENDL;
		LLHTTPClient::get(agents_url,new FSDownloader(agents_url));
	}

	//TODO: add blacklist support
// 	LL_INFOS("Blacklist") << "Downloading blacklist.xml" << LL_ENDL;
// 	LLHTTPClient::get(url,new FSDownloader( FSData::msblacklist ),headers);

	// FSUseLegacyClienttags: 0=Off, 1=Local Clienttags, 2=Download Clienttags
	if(gSavedSettings.getU32("FSUseLegacyClienttags") > 1)
	{
		LLSD headers;
		headers.insert("User-Agent", LLViewerMedia::getCurrentUserAgent());
		headers.insert("viewer-version", versionid);
		LLHTTPClient::get(legacy_client_list,new FSDownloader(legacy_client_list),headers);
		LL_INFOS("CLIENTTAGS DOWNLOADER") << "Getting new tags" << LL_ENDL;
	}
	else if(gSavedSettings.getU32("FSUseLegacyClienttags") > 0)
	{
		updateClientTagsLocal();
	}
}

void FSData::processReleases(const LLSD& releases)
{
	processReleasesLLSD(releases);
	
	// save the download to a file
	const std::string releases_filename = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, "releases.xml");
	saveLLSD(releases, releases_filename);
}

void FSData::processReleasesLLSD(const LLSD& releases)
{
	const LLSD& fs_versions = releases["FirestormReleases"];
	versions2.clear();
	for(LLSD::map_const_iterator itr = fs_versions.beginMap(); itr != fs_versions.endMap(); ++itr)
	{
		std::string key = (*itr).first;
		key += "\n";
		const LLSD& content = (*itr).second;
		U8 val = 0;
		if(content.has("beta"))val = val | PH_BETA;
		if(content.has("release"))val = val | PH_RELEASE;
		versions2[key] = val;
	}
	
	if(releases.has("BlockedReleases"))
	{
		const LLSD& blocked = releases["BlockedReleases"];
		blocked_versions.clear();
		for (LLSD::map_const_iterator itr = blocked.beginMap(); itr != blocked.endMap(); ++itr)
		{
			std::string vers = itr->first;
			const LLSD& content = itr->second;
			blocked_versions[vers] = content;
		}
	}
}

void FSData::processAgents(const LLSD& agents)
{
	processAgentsLLSD(agents);
	
	// save the download to a file
	const std::string agents_filename = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, "agents.xml");
	saveLLSD(agents, agents_filename);
}

void FSData::processAgentsLLSD(const LLSD& agents)
{
	const LLSD& support_agents = agents["SupportAgents"];
	mSupportAgentList.clear();
	for(LLSD::map_const_iterator iter = support_agents.beginMap(); iter != support_agents.endMap(); ++iter)
	{
		LLUUID key = LLUUID(iter->first);
		const LLSD& content = iter->second;
		if(content.has("support"))
		{
			mSupportAgentList[key].support = true;
		}
		else
		{
			mSupportAgentList[key].support = false;
		}

		if(content.has("developer"))
		{
			mSupportAgentList[key].developer = true;
		}
		else
		{
			mSupportAgentList[key].developer = false;
		}
	}
	
	const LLSD& support_groups = agents["SupportGroups"];
	mSupportGroup.clear();
	for(LLSD::map_const_iterator itr = support_groups.beginMap(); itr != support_groups.endMap(); ++itr)
	{
		mSupportGroup.insert(LLUUID(itr->first));
	}
}

void FSData::processClientTags(const LLSD& tags)
{
	if(tags.has("isComplete"))
	{
		LegacyClientList = tags;
		// save the download to a file
		const std::string tags_filename = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, "client_list_v2.xml");
		saveLLSD(tags, tags_filename);
	}
}

LLSD FSData::resolveClientTag(LLUUID id){
	//WS: Some helper function to make the request for old tags easier (if someone needs it)
	return resolveClientTag(id, false, LLColor4::black);
}

LLSD FSData::resolveClientTag(LLUUID id, bool new_system, LLColor4 color){
	//WS: Create a new LLSD based on the data from the LegacyClientList if
	LLSD curtag;
	curtag["uuid"]=id.asString();
	curtag["id_based"]=new_system;	
	curtag["tex_color"]=color.getValue();	
	// If we don't want to display anything...return
	if(gSavedSettings.getU32("FSClientTagsVisibility2") == 0)
	{
		return curtag;
	}

	//WS: Do we want to use Legacy Clienttags?
	if(gSavedSettings.getU32("FSUseLegacyClienttags") > 0)
	{
		if(LegacyClientList.has(id.asString()))
		{
			curtag=LegacyClientList[id.asString()];
		}
		else
		{		
			if(id == LLUUID("5d9581af-d615-bc16-2667-2f04f8eeefe4"))//green
			{
				curtag["name"]="Phoenix";
				curtag["color"] = LLColor4::green.getValue();
				curtag["alt"] = "ed63fbd0-589e-fe1d-a3d0-16905efaa96b";
			}
			else if(id == LLUUID("e35f7d40-6071-4b29-9727-5647bdafb5d5"))//white
			{
				curtag["name"] = "Phoenix";			
				curtag["color"] = LLColor4::white.getValue();
				curtag["alt"] = "ed63fbd0-589e-fe1d-a3d0-16905efaa96b";
			}
			else if(id == LLUUID("ae4e92fb-023d-23ba-d060-3403f953ab1a"))//pink
			{
				curtag["name"] = "Phoenix";
				curtag["color"] = LLColor4::pink.getValue();
				curtag["alt"] = "ed63fbd0-589e-fe1d-a3d0-16905efaa96b";
			}
			else if(id == LLUUID("e71b780e-1a57-400d-4649-959f69ec7d51"))//red
			{
				curtag["name"] = "Phoenix";
				curtag["color"] = LLColor4::red.getValue();
				curtag["alt"] = "ed63fbd0-589e-fe1d-a3d0-16905efaa96b";
			}
			else if(id == LLUUID("c1c189f5-6dab-fc03-ea5a-f9f68f90b018"))//orange
			{
				curtag["name"] = "Phoenix";
				curtag["color"] = LLColor4::orange.getValue();
				curtag["alt"] = "ed63fbd0-589e-fe1d-a3d0-16905efaa96b";
			}
			else if(id == LLUUID("8cf0577c-22d3-6a73-523c-15c0a90d6c27")) //purple
			{
				curtag["name"] = "Phoenix";
				curtag["color"] = LLColor4::purple.getValue();
				curtag["alt"] = "ed63fbd0-589e-fe1d-a3d0-16905efaa96b";
			}
			else if(id == LLUUID("5f0e7c32-38c3-9214-01f0-fb16a5b40128"))//yellow
			{
				curtag["name"] = "Phoenix";
				curtag["color"] = LLColor4::yellow.getValue();
				curtag["alt"] = "ed63fbd0-589e-fe1d-a3d0-16905efaa96b";
			}
			else if(id == LLUUID("5bb6e4a6-8e24-7c92-be2e-91419bb0ebcb"))//blue
			{
				curtag["name"] = "Phoenix";
				curtag["color"] = LLColor4::blue.getValue();
				curtag["alt"] = "ed63fbd0-589e-fe1d-a3d0-16905efaa96b";
			}
			else if(id == LLUUID("ed63fbd0-589e-fe1d-a3d0-16905efaa96b"))//default (red)
			{
				curtag["name"] = "Phoenix";
				curtag["color"] = LLColor4::red.getValue();
			}	
			else if(id == LLUUID("c228d1cf-4b5d-4ba8-84f4-899a0796aa97"))//viewer 2.0
			{
				curtag["name"] = "LL Viewer";
			}
			else if(id == LLUUID("cc7a030f-282f-c165-44d2-b5ee572e72bf"))
			{
				curtag["name"] = "Imprudence";
			}
			else if(id == LLUUID("54d93609-1392-2a93-255c-a9dd429ecca5"))
			{
				curtag["name"] = "Emergence";
			}
			else if(id == LLUUID("8873757c-092a-98fb-1afd-ecd347566fcd"))
			{
				curtag["name"] = "Ascent";
			}
			else if(id == LLUUID("f25263b7-6167-4f34-a4ef-af65213b2e39"))
			{
				curtag["name"] = "Singularity";
			}
			if(curtag.has("name")) curtag["tpvd"]=true;
		}
	}
	
	
	// Filtering starts here:
	//WS: If the current tag has an "alt" definied and we don't want multiple colors. Resolve the alt.
	if((gSavedSettings.getU32("FSColorClienttags") == 1) && curtag.has("alt"))
	{
		curtag = resolveClientTag(curtag["alt"], new_system, color);
	}

	//WS: If we have a tag using the new system, check if we want to display it's name and/or color
	if(new_system)
	{
		if(gSavedSettings.getU32("FSClientTagsVisibility2") >= 3)
		{
			// strnlen() doesn't exist on OS X before 10.7. -- TS
			char tag_temp[UUID_BYTES+1];
			strncpy(tag_temp,(const char*)&id.mData[0], UUID_BYTES);
			tag_temp[UUID_BYTES] = '\0';
			U32 tag_len = strlen(tag_temp);
			std::string clienttagname = std::string((const char*)&id.mData[0], tag_len);
			LLStringFn::replace_ascii_controlchars(clienttagname, LL_UNKNOWN_CHAR);
			curtag["name"] = clienttagname;
		}
		if(gSavedSettings.getU32("FSColorClienttags") >= 3 || curtag["tpvd"].asBoolean())
		{
			if(curtag["tpvd"].asBoolean() && gSavedSettings.getU32("FSColorClienttags") < 3)
			{
				if(color == LLColor4::blue || color == LLColor4::yellow ||
				   color == LLColor4::purple || color == LLColor4((F32)0.99,(F32)0.39,(F32)0.12,(F32)1) ||
				   color == LLColor4::red || color == LLColor4((F32)0.99,(F32)0.56,(F32)0.65,(F32)1) ||
				   color == LLColor4::white || color == LLColor4::green)
				{
					curtag["color"] = color.getValue();
				}
			}
			else 
			{
				curtag["color"] = color.getValue();
			}
		}
	}

	//If we only want to display tpvd viewer. And "tpvd" is not available or false, then
	// clear the data, but keep the basedata (like uuid, id_based and tex_color) for (maybe) later displaying.
	if(gSavedSettings.getU32("FSClientTagsVisibility2") <= 1 && (!curtag.has("tpvd") || !curtag["tpvd"].asBoolean()))
	{
		curtag.clear();
	}

	curtag["uuid"]=id.asString();
	curtag["id_based"]=new_system;	
	curtag["tex_color"]=color.getValue();	

	return curtag;
}

void FSData::updateClientTagsLocal()
{
	std::string client_list_filename = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, "client_list_v2.xml");
	llifstream xml_file(client_list_filename);
	LLSD data;
	if(!xml_file.is_open()) return;
	if(LLSDSerialize::fromXML(data, xml_file) >= 1)
	{
		if(data.has("isComplete"))
		{
			LegacyClientList = data;
		}

		xml_file.close();
	}
}

void FSData::saveLLSD(const LLSD& data, const std::string& filename)
{
	LL_INFOS("Data") << "Saving " << filename << LL_ENDL;
	llofstream file;
	file.open(filename);
	if(!file.is_open())
	{
		LL_WARNS("Data") << "Unable to open " << filename << LL_ENDL;
		return;
	}
	if (!LLSDSerialize::toPrettyXML(data, file))
	{
		LL_WARNS("Data") << "Failed to save LLSD for " << filename << LL_ENDL;
	}
	file.close();
}

#if (0)
void FSData::msblacklist(U32 status,std::string body)
{
	if(status != 200)
	{
		LL_WARNS("Blacklist") << "Something went wrong with the blacklist download status code " << status << LL_ENDL;
	}

	std::istringstream istr(body);
	if (body.size() > 0)
	{
		LLSD data;
		if(LLSDSerialize::fromXML(data,istr) >= 1)
		{
			LL_INFOS("Blacklist") << body.size() << " bytes received, updating local blacklist" << LL_ENDL;
			for(LLSD::map_iterator itr = data.beginMap(); itr != data.endMap(); ++itr)
			{
				if(itr->second.has("name"))
					LLFloaterBlacklist::addEntry(LLUUID(itr->first),itr->second);
			}
		}
		else
		{
			LL_INFOS("Blacklist") << "Failed to parse blacklist.xml" << LL_ENDL;
		}
	}
	else
	{
		LL_INFOS("Blacklist") << "Empty blacklist.xml" << LL_ENDL;
	}
}
#endif

FSDataAgent* FSData::getAgent(LLUUID avatar_id)
{
	std::map<LLUUID, FSDataAgent>::iterator iter = mSupportAgentList.find(avatar_id);
	if (iter == mSupportAgentList.end())
	{
		return NULL;
	}
	return &iter->second;
}

bool FSData::is_support(LLUUID avatar_id)
{
	std::map<LLUUID, FSDataAgent>::iterator iter = mSupportAgentList.find(avatar_id);
	if (iter == mSupportAgentList.end())
	{
		return false;
	}
	return iter->second.support;
}

BOOL FSData::is_BetaVersion(std::string version)
{
	if(versions2.find(version) != versions2.end())
	{
		return ((versions2[version] & PH_BETA) != 0) ? TRUE : FALSE;
	}
	return FALSE;
}

BOOL FSData::is_ReleaseVersion(std::string version)
{
	if(versions2.find(version) != versions2.end())
	{
		return ((versions2[version] & PH_RELEASE) != 0) ? TRUE : FALSE;
	}
	return FALSE;
}

bool FSData::is_developer(LLUUID avatar_id)
{
	std::map<LLUUID, FSDataAgent>::iterator iter = mSupportAgentList.find(avatar_id);
	if (iter == mSupportAgentList.end())
	{
		return false;
	}
	return iter->second.developer;
}

LLSD FSData::allowed_login()
{
	std::map<std::string, LLSD>::iterator iter = blocked_versions.find(versionid);
	if (iter == blocked_versions.end())
	{
		LLSD empty;
		return empty; 
	}
	else
	{
		return iter->second;
	}
}

BOOL FSData::isSupportGroup(LLUUID id)
{
	return (mSupportGroup.count(id));
}

std::string FSData::processRequestForInfo(LLUUID requester, std::string message, std::string name, LLUUID sessionid)
{
	std::string detectstring = "/reqsysinfo";
	if(!message.find(detectstring) == 0)
	{
		return message;
	}

	if(!(is_support(requester)||is_developer(requester)))
	{
		return message;
	}

	std::string outmessage("I am requesting information about your system setup.");
	std::string reason("");
	if(message.length() > detectstring.length())
	{
		reason = std::string(message.substr(detectstring.length()));
		//there is more to it!
		outmessage = std::string("I am requesting information about your system setup for this reason : " + reason);
		reason = "The reason provided was : " + reason;
	}
	
	LLSD args;
	args["REASON"] = reason;
	args["NAME"] = name;
	args["FROMUUID"] = requester;
	args["SESSIONID"] = sessionid;
	LLNotifications::instance().add("FireStormReqInfo", args, LLSD(), callbackReqInfo);

	return outmessage;
}

//static
void FSData::sendInfo(LLUUID destination, LLUUID sessionid, std::string myName, EInstantMessage dialog)
{
	LLSD system_info = getSystemInfo();
	std::string part1 = system_info["Part1"].asString();
	std::string part2 = system_info["Part2"].asString();

	pack_instant_message(
		gMessageSystem,
		gAgent.getID(),
		FALSE,
		gAgent.getSessionID(),
		destination,
		myName,
		part1,
		IM_ONLINE,
		dialog,
		sessionid
		);
	gAgent.sendReliableMessage();
	pack_instant_message(
		gMessageSystem,
		gAgent.getID(),
		FALSE,
		gAgent.getSessionID(),
		destination,
		myName,
		part2,
		IM_ONLINE,
		dialog,
		sessionid
		);
	gAgent.sendReliableMessage();

	gIMMgr->addMessage(gIMMgr->computeSessionID(dialog,destination),destination,myName,
				"Information Sent: " + part1 + "\n" + part2);
}

//static
void FSData::callbackReqInfo(const LLSD &notification, const LLSD &response)
{
	S32 option = LLNotification::getSelectedOption(notification, response);
	std::string my_name;
	LLSD subs = LLNotification(notification).getSubstitutions();
	LLUUID uid = subs["FROMUUID"].asUUID();
	LLUUID sessionid = subs["SESSIONID"].asUUID();

	llinfos << "the uuid is " << uid.asString().c_str() << llendl;
	LLAgentUI::buildFullname(my_name);

	if ( option == 0 )//yes
	{
		sendInfo(uid,sessionid,my_name,IM_NOTHING_SPECIAL);
	}
	else
	{
		pack_instant_message(
			gMessageSystem,
			gAgent.getID(),
			FALSE,
			gAgent.getSessionID(),
			uid,
			my_name,
			"Request Denied.",
			IM_ONLINE,
			IM_NOTHING_SPECIAL,
			sessionid
			);
		gAgent.sendReliableMessage();
		gIMMgr->addMessage(sessionid,uid,my_name,"Request Denied");
	}
}

//static
LLSD FSData::getSystemInfo()
{
	LLSD info=LLFloaterAbout::getInfo();

	std::string sysinfo1("\n");
	sysinfo1 += llformat("%s %s (%d) %s %s (%s)\n\n", LLAppViewer::instance()->getSecondLifeTitle().c_str(), LLVersionInfo::getShortVersion().c_str(), LLVersionInfo::getBuild(), info["BUILD_DATE"].asString().c_str(), info["BUILD_TIME"].asString().c_str(), LLVersionInfo::getChannel().c_str()),
//<FS:CR> FIRE-8273: Add Havok/Opensim indicator to getSystemInfo()
		info["BUILD_TYPE"].asString().c_str();
// </FS:CR>
	sysinfo1 += llformat("Build with %s version %s\n\n", info["COMPILER"].asString().c_str(), info["COMPILER_VERSION"].asString().c_str());
	sysinfo1 += llformat("I am in %s located at %s (%s)\n", info["REGION"].asString().c_str(), info["HOSTNAME"].asString().c_str(), info["HOSTIP"].asString().c_str());
	sysinfo1 += llformat("%s\n", info["SERVER_VERSION"].asString().c_str());

	std::string sysinfo2("\n");
	sysinfo2 += llformat("CPU: %s\n", info["CPU"].asString().c_str());
	sysinfo2 += llformat("Memory: %d MB\n", info["MEMORY_MB"].asInteger());
	sysinfo2 += llformat("OS: %s\n", info["OS_VERSION"].asString().c_str());
	sysinfo2 += llformat("Graphics Card Vendor: %s\n", info["GRAPHICS_CARD_VENDOR"].asString().c_str());
	sysinfo2 += llformat("Graphics Card: %s\n", info["GRAPHICS_CARD"].asString().c_str());
	
	if (info.has("GRAPHICS_DRIVER_VERSION"))
	{
		sysinfo2 += llformat("Graphics Card Driver Version: %s\n", info["GRAPHICS_DRIVER_VERSION"].asString().c_str());
	}

	sysinfo2 += llformat("OpenGL Version: %s\n\n", info["OPENGL_VERSION"].asString().c_str());
	sysinfo2 += llformat("libcurl Version: %s\n", info["LIBCURL_VERSION"].asString().c_str());
	sysinfo2 += llformat("J2C Decoder Version: %s\n", info["J2C_VERSION"].asString().c_str());
	sysinfo2 += llformat("Audio Driver Version: %s\n", info["AUDIO_DRIVER_VERSION"].asString().c_str());
	sysinfo2 += llformat("Qt Webkit Version: %s\n", info["QT_WEBKIT_VERSION"].asString().c_str());
	sysinfo2 += llformat("Vivox Version: %s\n", info["VOICE_VERSION"].asString().c_str());
	sysinfo2 += llformat("Packets Lost: %.0f/%.0f (%.1f%%)\n\n", info["PACKETS_LOST"].asReal(), info["PACKETS_IN"].asReal(), info["PACKETS_PCT"].asReal());

	sysinfo2 += llformat("RLVa: %s\n", info["RLV_VERSION"].asString().c_str());
	sysinfo2 += llformat("Mode: %s\n", info["MODE"].asString().c_str());
	sysinfo2 += llformat("Skin: %s (%s)\n", info["SKIN"].asString().c_str(), info["THEME"].asString().c_str());
	sysinfo2 += llformat("Font: %s\n", info["FONT"].asString().c_str());
	sysinfo2 += llformat("Font Size Adjustment: %d pt\n", info["FONT_SIZE"].asInteger());
	sysinfo2 += llformat("Font Screen DPI: %d\n", info["FONT_SCREEN_DPI"].asInteger());
	sysinfo2 += llformat("Draw Distance: %d m\n", info["DRAW_DISTANCE"].asInteger());
	sysinfo2 += llformat("Bandwidth: %d kbit/s\n", info["BANDWIDTH"].asInteger());
	sysinfo2 += llformat("LOD Factor: %.3f", info["LOD"].asReal());

	LLSD sysinfos;
	sysinfos["Part1"] = sysinfo1;
	sysinfos["Part2"] = sysinfo2;

	return sysinfos;
}

