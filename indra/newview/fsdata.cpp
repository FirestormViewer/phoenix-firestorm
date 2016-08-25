/**
 * @file fsdata.cpp
 * @brief Downloadable dymatic xml data for viewer features.
 *
 * $LicenseInfo:firstyear=2013&license=viewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (C) 2011-2013 Techwolf Lupindo
 * Portions Copyright (C)
 *   2011 Wolfspirit Magic
 *   2012 Ansariel Hiller @ Second Life
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

#include "llviewerprecompiledheaders.h"

#include "fsdata.h"
#include "fscommon.h"
#include "fscorehttputil.h"
#include "fswsassetblacklist.h"

/* boost: will not compile unless equivalent is undef'd, beware. */
#include "fix_macros.h"
#include <boost/filesystem.hpp>
#include <boost/foreach.hpp>

#include "llappviewer.h"
#include "llagent.h"
#include "llagentui.h"
#include "llfloaterabout.h"
#include "llimview.h"
#include "llmutelist.h"
#include "llnotifications.h"
#include "llsdserialize.h"
#include "lltrans.h"
#include "llversioninfo.h"
#include "llviewercontrol.h"
#include "llviewermedia.h"
#include "llviewernetwork.h"
#include "llxorcipher.h"
#include "llvfs.h"
#include "message.h"

// [RLVa:KB]
#include "rlvactions.h"
#include "rlvhelper.h"
// [/RLVa:KB]

const std::string LEGACY_CLIENT_LIST_URL = "http://phoenixviewer.com/app/client_tags/client_list_v2.xml";
const LLUUID MAGIC_ID("3c115e51-04f4-523c-9fa6-98aff1034730");
const F32 HTTP_TIMEOUT = 30.f;

#if LL_DARWIN
size_t strnlen(const char *s, size_t n)
{
	const char *p = (const char *)memchr(s, 0, n);
	return(p ? p-s : n);
}
#endif

FSData::FSData() :
	mLegacySearch(true),
	mFSDataDone(false),
	mAgentsDone(false)
{
	mHeaders.insert("User-Agent", LLViewerMedia::getCurrentUserAgent());
	mHeaders.insert("viewer-version", LLVersionInfo::getChannelAndVersionFS());
	
	mBaseURL = gSavedSettings.getBOOL("FSdataQAtest") ? "http://phoenixviewer.com/app/fsdatatest" : "http://phoenixviewer.com/app/fsdata";
	mFSDataURL = mBaseURL + "/" + "data.xml";
}

void FSData::processResponder(const LLSD& content, const std::string& url, bool save_to_file, const LLDate& last_modified)
{
	if (url == mFSDataURL)
	{
		if (!save_to_file)
		{
			LLSD data;
			LL_DEBUGS("fsdata") << "Loading fsdata.xml from  " << mFSdataFilename << LL_ENDL;
			if (loadFromFile(data, mFSdataFilename))
			{
				processData(data);
			}
			else
			{
				LL_WARNS("fsdata") << "Unable to download or load fsdata.xml" << LL_ENDL;
			}
		}
		else
		{
			processData(content);
			saveLLSD(content , mFSdataFilename, last_modified);
		}
		mFSDataDone = true;
	}
	else if (url == mAssetsURL)
	{
		if (!save_to_file)
		{
			LLSD data;
			LL_DEBUGS("fsdata") << "Loading assets.xml from  " << mAssestsFilename << LL_ENDL;
			if (loadFromFile(data, mAssestsFilename))
			{
				processAssets(data);
			}
			else
			{
				LL_WARNS("fsdata") << "Unable to download or load assets.xml" << LL_ENDL;
			}
		}
		else
		{
			processAssets(content);
			saveLLSD(content , mAssestsFilename, last_modified);
		}
	}
	else if (url == mAgentsURL)
	{
		if (!save_to_file)
		{
			LLSD data;
			LL_DEBUGS("fsdata") << "Loading agents.xml from  " << mAgentsFilename << LL_ENDL;
			if (loadFromFile(data, mAgentsFilename))
			{
				processAgents(data);
			}
			else
			{
				LL_WARNS("fsdata") << "Unable to download or load agents.xml" << LL_ENDL;
			}
		}
		else
		{
			processAgents(content);
			saveLLSD(content , mAgentsFilename, last_modified);
		}
		mAgentsDone = true;
		addAgents();
	}
	else if (url == LEGACY_CLIENT_LIST_URL)
	{
		if (!save_to_file)
		{
			updateClientTagsLocal();
		}
		else
		{
			processClientTags(content);
			saveLLSD(content , mClientTagsFilename, last_modified);
		}
	}
	else if (url == mFSdataDefaultsUrl)
	{
		if (!save_to_file)
		{
			// do nothing as this file is loaded during app startup.
		}
		else
		{
			saveLLSD(content , mFSdataDefaultsFilename, last_modified);
		}
	}
}

bool FSData::loadFromFile(LLSD& data, std::string filename)
{
	llifstream file(filename.c_str());
	if(file.is_open())
	{
		if(LLSDSerialize::fromXML(data, file) != LLSDParser::PARSE_FAILURE)
		{
			file.close();
			return true;
		}
		else
		{
			// error reading file
			file.close();
			return false;
		}
	}
	else
	{
		// file does not exist or other error
		return false;
	}
}

void downloadComplete( LLSD const &aData, std::string const &aURL )
{
	LL_DEBUGS() << aData << LL_ENDL;
	
	LLSD header = aData[ LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS ][ LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS_HEADERS];

	LLDate lastModified;
	if (header.has("last-modified"))
	{
		lastModified.secondsSinceEpoch( FSCommon::secondsSinceEpochFromString( "%a, %d %b %Y %H:%M:%S %ZP", header["last-modified"].asString() ) );
	}

	LLSD data = aData;
	data.erase( LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS );
	
	FSData::getInstance()->processResponder( data, aURL, true, lastModified);
}

void downloadCompleteScript( LLSD const &aData, std::string const &aURL, std::string const &aFilename  )
{
	LL_DEBUGS() << aData << LL_ENDL;
	LLSD header = aData[ LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS ][ LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS_HEADERS];
    LLCore::HttpStatus status = LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD( aData[ LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS ] );

	LLDate lastModified;
	if (header.has("last-modified"))
	{
		lastModified.secondsSinceEpoch( FSCommon::secondsSinceEpochFromString( "%a, %d %b %Y %H:%M:%S %ZP", header["last-modified"].asString() ) );
	}
    const LLSD::Binary &rawData = aData[LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS_RAW].asBinary();

	if ( status.getType() == HTTP_NOT_MODIFIED )
	{
		LL_INFOS("fsdata") << "Got [304] not modified for " << aURL << LL_ENDL;
		return;
	}

	if (rawData.size() <= 0)
	{
		LL_WARNS("fsdata") << "Received zero data for " << aURL << LL_ENDL;
		return;
	}

	// basic check for valid data received
	LLXMLNodePtr xml_root;
	std::string stringData;
	stringData.assign( rawData.begin(), rawData.end() ); // LLXMLNode::parseBuffer wants a U8*, not a const U8*, so need to copy here just to be safe
	if ( (!LLXMLNode::parseBuffer( reinterpret_cast< U8*> ( &stringData[0] ), stringData.size(), xml_root, NULL)) || (xml_root.isNull()) || (!xml_root->hasName("script_library")) )
	{
		LL_WARNS("fsdata") << "Could not read the script library data from "<< aURL << LL_ENDL;
		return;
	}
		
	LLAPRFile outfile ;
	outfile.open(aFilename, LL_APR_WB);
	if (!outfile.getFileHandle())
	{
		LL_WARNS("fsdata") << "Unable to open file for writing: " << aFilename << LL_ENDL;
	}
	else
	{
		LL_INFOS("fsdata") << "Saving " << aFilename << LL_ENDL;
		outfile.write(  &rawData[0], rawData.size() );
		outfile.close() ;
	}
}

void downloadError( LLSD const &aData, std::string const &aURL )
{
	LL_WARNS() << "Failed to download " << aURL << ": " << aData << LL_ENDL;
	// FSData::getInstance()->processResponder(getContent(), mURL, false, mLastModified);
}

// call this just before the login screen and after the LLProxy has been setup.
void FSData::startDownload()
{
	mFSdataFilename = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, "fsdata.xml");
	mFSdataDefaultsFilename = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, "fsdata_defaults.xml");
	mClientTagsFilename = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, "client_list_v2.xml");

	// Stat the file to see if it exists and when it was last modified.
	time_t last_modified = 0;
	llstat stat_data;
	if(!LLFile::stat(mFSdataFilename, &stat_data))
	{
		last_modified = stat_data.st_mtime;
	}
	LL_INFOS("fsdata") << "Downloading data.xml from " << mFSDataURL << " with last modifed of " << last_modified << LL_ENDL;

	LLCoreHttpUtil::HttpCoroutineAdapter::callbackHttpGet( mFSDataURL, boost::bind( downloadComplete, _1, mFSDataURL ), boost::bind( downloadError, _1, mFSDataURL ) );

	//LLHTTPClient::getIfModified(mFSDataURL, new FSDownloader(mFSDataURL), last_modified, mHeaders, HTTP_TIMEOUT);
	
	last_modified = 0;
	if(!LLFile::stat(mFSdataDefaultsFilename, &stat_data))
	{
		last_modified = stat_data.st_mtime;
	}
	std::string filename = llformat("defaults.%s.xml", LLVersionInfo::getShortVersion().c_str());
	mFSdataDefaultsUrl = mBaseURL + "/" + filename;
	LL_INFOS("fsdata") << "Downloading defaults.xml from " << mFSdataDefaultsUrl << " with last modifed of " << last_modified << LL_ENDL;
	LLCoreHttpUtil::HttpCoroutineAdapter::callbackHttpGet( mFSdataDefaultsUrl, boost::bind( downloadComplete, _1, mFSdataDefaultsUrl ), boost::bind( downloadError, _1, mFSdataDefaultsUrl ) );

#if OPENSIM
	std::string filenames[] = {"scriptlibrary_ossl.xml", "scriptlibrary_aa.xml"};
	BOOST_FOREACH(std::string script_name, filenames)
	{
		filename = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, script_name);
		last_modified = 0;
		if(!LLFile::stat(filename, &stat_data))
		{
			last_modified = stat_data.st_mtime;
		}
		std::string url = mBaseURL + "/" + script_name;
		LL_INFOS("fsdata") << "Downloading " << script_name << " from " << url << " with last modifed of " << last_modified << LL_ENDL;
		FSCoreHttpUtil::callbackHttpGetRaw( url, boost::bind( downloadCompleteScript, _1, url, filename ), boost::bind( downloadError, _1, url ) );
	}
#endif
}

// call this _after_ the login screen to pick up grid data.
void FSData::downloadAgents()
{
#ifdef OPENSIM
	std::string filename_prefix = LLGridManager::getInstance()->getGridId();
#else
	std::string filename_prefix = "second_life";
#endif

#ifdef OPENSIM
	if (!LLGridManager::getInstance()->isInSecondLife())
	{
		// TODO: Let the opensim devs and opensim group figure out the best way
		// to add "agents.xml" URL to the gridinfo protocol.
		//getAgentsURL();
		
		// there is no need for assets.xml URL for opensim grids as the grid owner can just delete
		// the bad asset itself.
	}
	else
#endif
	{
		mAgentsURL = mBaseURL + "/" + "agents.xml";
		mAssetsURL = mBaseURL + "/" + "assets.xml";
	}
	
	if (mAgentsURL.empty())
	{
		// we can saftly presume if agents.xml URL is not present, assets.xml is not going to be present also
		return;
	}
	
	mAgentsFilename = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, filename_prefix + "_agents.xml");
	time_t last_modified = 0;
	llstat stat_data;
	if(!LLFile::stat(mAgentsFilename, &stat_data))
	{
		last_modified = stat_data.st_mtime;
	}
	LL_INFOS("fsdata") << "Downloading agents.xml from " << mAgentsURL << " with last modifed of " << last_modified << LL_ENDL;
	LLCoreHttpUtil::HttpCoroutineAdapter::callbackHttpGet( mAgentsURL, boost::bind( downloadComplete, _1, mAgentsURL ), boost::bind( downloadError, _1, mAgentsURL ) );
	
	if (mAssetsURL.empty())
	{
		return;
	}
	
	mAssestsFilename = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, filename_prefix + "_assets.xml");
	last_modified = 0;
	if(!LLFile::stat(mAssestsFilename, &stat_data))
	{
		last_modified = stat_data.st_mtime;
	}
	LL_INFOS("fsdata") << "Downloading assets.xml from " << mAssetsURL << " with last modifed of " << last_modified << LL_ENDL;
	LLCoreHttpUtil::HttpCoroutineAdapter::callbackHttpGet( mAssetsURL, boost::bind( downloadComplete, _1, mAssetsURL ), boost::bind( downloadError, _1, mAssetsURL ) );
}

void FSData::processData(const LLSD& fs_data)
{
	// Set Message Of The Day if present 
	if(fs_data.has("MOTD"))
	{
		gAgent.mMOTD.assign(fs_data["MOTD"]);
	}
	else if(fs_data.has("RandomMOTD")) // only used if MOTD is not presence in the xml file.
	{
		const LLSD& motd = fs_data["RandomMOTD"];
		LLSD::array_const_iterator iter = motd.beginArray();
		gAgent.mMOTD.assign((iter + (ll_rand((S32)motd.size())))->asString());
	}
	
	// If the event falls withen the current date, use that for MOTD instead.
	if (fs_data.has("EventsMOTD"))
	{
		const LLSD& events = fs_data["EventsMOTD"];
		for(LLSD::map_const_iterator iter = events.beginMap(); iter != events.endMap(); ++iter)
		{
			std::string name = iter->first;
			const LLSD& content = iter->second;
			LL_DEBUGS("fsdata") << "Found event MOTD: " << name << LL_ENDL;
			if (content["startDate"].asDate() < LLDate::now() && content["endDate"].asDate() > LLDate::now())
			{
				LL_INFOS("fsdata") << "Setting MOTD to " << name << LL_ENDL;
				gAgent.mMOTD.assign(content["EventMOTD"]); // note singler instead of plural above
				break; // Only use the first one found.
			}
		}
	}

	if(fs_data.has("OpensimMOTD"))
	{
		mOpensimMOTD.assign(fs_data["OpensimMOTD"]);
	}

	if(fs_data.has("BlockedReleases"))
	{
		const LLSD& blocked = fs_data["BlockedReleases"];
		for (LLSD::map_const_iterator iter = blocked.beginMap(); iter != blocked.endMap(); ++iter)
		{
			std::string version = iter->first;
			const LLSD& content = iter->second;
			mBlockedVersions[version] = content;
			LL_DEBUGS("fsdata") << "Added " << version << " to mBlockedVersions" << LL_ENDL;
		}
	}

	processAgents(fs_data);
	processAssets(fs_data);

	// FSUseLegacyClienttags: 0=Off, 1=Local Clienttags, 2=Download Clienttags
	static LLCachedControl<U32> use_legacy_tags(gSavedSettings, "FSUseLegacyClienttags");
	if(use_legacy_tags > 1)
	{
		time_t last_modified = 0;
		llstat stat_data;
		if(!LLFile::stat(mClientTagsFilename, &stat_data))
		{
			last_modified = stat_data.st_mtime;
		}
		LL_INFOS("fsdata") << "Downloading client_list_v2.xml from " << LEGACY_CLIENT_LIST_URL << " with last modifed of " << last_modified << LL_ENDL;
		LLCoreHttpUtil::HttpCoroutineAdapter::callbackHttpGet( LEGACY_CLIENT_LIST_URL, boost::bind( downloadComplete, _1, LEGACY_CLIENT_LIST_URL ), boost::bind( downloadError, _1, LEGACY_CLIENT_LIST_URL ) );
	}
	else if(use_legacy_tags)
	{
		updateClientTagsLocal();
	}

// [RLVa:KB]
	if ( (RlvActions::isRlvEnabled()) && (fs_data.has("rlva_compat_list")) )
	{
		RlvSettings::initCompatibilityMode(fs_data["rlva_compat_list"].asString());
	}
// [/RLVa:KB]
}

void FSData::processAssets(const LLSD& assets)
{
	if (!assets.has("assets"))
	{
		return;
	}
	const LLSD& asset = assets["assets"];
	for (LLSD::map_const_iterator itr = asset.beginMap(); itr != asset.endMap(); ++itr)
	{
		LLUUID uid = LLUUID(itr->first);
		LLXORCipher cipher(MAGIC_ID.mData, UUID_BYTES);
		cipher.decrypt(uid.mData, UUID_BYTES);
		LLSD data = itr->second;
		data["asset_permanent"] = false; // Don't save these locally!
		if (uid.isNull())
		{
			continue;
		}
		FSWSAssetBlacklist::instance().addNewItemToBlacklistData(uid, data, false);
		LL_DEBUGS("fsdata") << "Added " << uid << " to assets list." << LL_ENDL;
	}
}

void FSData::processAgents(const LLSD& data)
{
	if (data.has("Agents"))
	{
		const LLSD& agents = data["Agents"];
		for(LLSD::map_const_iterator iter = agents.beginMap(); iter != agents.endMap(); ++iter)
		{
			LLUUID key = LLUUID(iter->first);
			mSupportAgents[key] = iter->second.asInteger();
			LL_DEBUGS("fsdata") << "Added " << key << " with " << mSupportAgents[key] << " flag mask to mSupportAgentList" << LL_ENDL;
		}
	}
	else if (data.has("SupportAgents")) // Legacy format
	{
		const LLSD& support_agents = data["SupportAgents"];
		std::string newFormat;
		for(LLSD::map_const_iterator iter = support_agents.beginMap(); iter != support_agents.endMap(); ++iter)
		{
			LLUUID key = LLUUID(iter->first);
			mSupportAgents[key] = 0;
			const LLSD& content = iter->second;
			if(content.has("support"))
			{
				mSupportAgents[key] |= SUPPORT;
			}

			if(content.has("developer"))
			{
				mSupportAgents[key] |= DEVELOPER;
			}
			LL_DEBUGS("fsdata") << "Added Legacy " << key << " with " << mSupportAgents[key] << " flag mask to mSupportAgentList" << LL_ENDL;
			std::string text = llformat("<key>%s</key><!-- %s -->\n    <integer>%d</integer>\n", key.asString().c_str(), content["name"].asString().c_str(), mSupportAgents[key]);
			newFormat.append(text);
		}
		LL_DEBUGS("fsdata") << "New format for copy paste:\n" << newFormat << LL_ENDL;
	}

	if (data.has("SupportGroups"))
	{
		const LLSD& support_groups = data["SupportGroups"];
		for(LLSD::map_const_iterator itr = support_groups.beginMap(); itr != support_groups.endMap(); ++itr)
		{
			mSupportGroup.insert(LLUUID(itr->first));
			LL_DEBUGS("fsdata") << "Added " << itr->first << " to mSupportGroup" << LL_ENDL;
		}
	}
	
	// The presence of just the key is enough to determine that legacy search needs to be disabled on this grid.
	if (data.has("DisableLegacySearch"))
	{
		mLegacySearch = false;
		LL_WARNS("fsdata") << "Legacy Search has been disabled." << LL_ENDL;
	}
}

void FSData::processClientTags(const LLSD& tags)
{
	if(tags.has("isComplete"))
	{
		mLegacyClientList = tags;
	}
}

//WS: Create a new LLSD based on the data from the mLegacyClientList if
LLSD FSData::resolveClientTag(const LLUUID& id, bool new_system, const LLColor4& color)
{	
	LLSD curtag;
	curtag["uuid"] = id.asString();
	curtag["id_based"] = new_system;
	curtag["tex_color"] = color.getValue();

	static const LLUUID id_singularity("f25263b7-6167-4f34-a4ef-af65213b2e39");
	static const LLUUID id_kokua("4b6f6b75-bf77-d1ff-0000-000000000000");
	static const LLUUID id_radegast("b748af88-58e2-995b-cf26-9486dea8e830");
	static const LLUUID id_imprudence("cc7a030f-282f-c165-44d2-b5ee572e72bf");
	static const LLUUID id_teapot("7eab0700-f000-0000-0000-546561706f7");
	static const LLUUID id_lindenlab("c228d1cf-4b5d-4ba8-84f4-899a0796aa97");
	
	static LLCachedControl<U32> client_tag_visibility(gSavedSettings, "FSClientTagsVisibility");
	static LLCachedControl<U32> use_legacy_client_tags(gSavedSettings, "FSUseLegacyClienttags");
	static LLCachedControl<U32> color_client_tags(gSavedSettings, "FSColorClienttags");
	// If we don't want to display anything...return
	if (!client_tag_visibility)
	{
		return curtag;
	}

	//WS: Do we want to use Legacy Clienttags?
	if (use_legacy_client_tags)
	{
		if (mLegacyClientList.has(id.asString()))
		{
			curtag = mLegacyClientList[id.asString()];
		}
		else
		{
			if (id == id_singularity)
			{
				curtag["name"] = "Singularity";
			}
			else if (id == id_kokua)
			{
				curtag["name"] = "Kokua";
			}
			else if (id == id_radegast)
			{
				curtag["name"] = "Radegast";
			}
			else if (id == id_imprudence)
			{
				curtag["name"] = "Imprudence";
			}
			else if (id == id_teapot)
			{
				curtag["name"] = "Teapot";
			}
			/// [FS:CR] Since SL Viewer can't connect to Opensim, and client tags only work on OpenSim
			/// it doesn't make much sense to tag V3-based viewers as SL Viewer.
			/*if (id == id_lindenlab)
			{
				curtag["name"] = "SL Viewer";
			}*/
			if (curtag.has("name")) curtag["tpvd"] = true;
		}
	}
	
	
	// Filtering starts here:
	//WS: If we have a tag using the new system, check if we want to display it's name and/or color
	if (new_system)
	{
		if (client_tag_visibility >= 3)
		{
			U32 tag_len = strnlen((const char*)&id.mData[0], UUID_BYTES);
			std::string clienttagname = std::string((const char*)&id.mData[0], tag_len);
			LLStringFn::replace_ascii_controlchars(clienttagname, LL_UNKNOWN_CHAR);
			curtag["name"] = clienttagname;
		}
		if (color_client_tags >= 3 || curtag["tpvd"].asBoolean())
		{
			if (curtag["tpvd"].asBoolean() && color_client_tags < 3)
			{
				if (color == LLColor4::blue ||
					color == LLColor4::yellow ||
					color == LLColor4::purple ||
					color == LLColor4(0.99f, 0.39f, 0.12f, 1.f) ||
					color == LLColor4::red ||
					color == LLColor4(0.99f, 0.56f, 0.65f, 1.f) ||
					color == LLColor4::white ||
					color == LLColor4::green)
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
	if(client_tag_visibility <= 1 && (!curtag.has("tpvd") || !curtag["tpvd"].asBoolean()))
	{
		curtag.clear();
	}

	curtag["uuid"] = id.asString();
	curtag["id_based"] = new_system;
	curtag["tex_color"] = color.getValue();

	return curtag;
}

void FSData::updateClientTagsLocal()
{
	LLSD data;
	LL_DEBUGS("fsdata") << "Loading client_list_v2.xml from  " << mClientTagsFilename << LL_ENDL;
	if (loadFromFile(data, mClientTagsFilename))
	{
		processClientTags(data);
	}
	else
	{
		LL_WARNS("fsdata") << "Unable to download or load client_list_v2.xml" << LL_ENDL;
	}
}

void FSData::saveLLSD(const LLSD& data, const std::string& filename, const LLDate& last_modified)
{
	LL_INFOS("fsdata") << "Saving " << filename << LL_ENDL;
	llofstream file;
	file.open(filename.c_str());
	if(!file.is_open())
	{
		LL_WARNS("fsdata") << "Unable to open " << filename << LL_ENDL;
		return;
	}
	if (!LLSDSerialize::toPrettyXML(data, file))
	{
		LL_WARNS("fsdata") << "Failed to save LLSD for " << filename << LL_ENDL;
	}
	file.close();

	const std::time_t new_time = last_modified.secondsSinceEpoch();

#ifdef LL_WINDOWS
	boost::filesystem::last_write_time(boost::filesystem::path( utf8str_to_utf16str(filename) ), new_time);
#else
	boost::filesystem::last_write_time(boost::filesystem::path(filename), new_time);
#endif
}

S32 FSData::getAgentFlags(const LLUUID& avatar_id)
{
	std::map<LLUUID, S32>::iterator iter = mSupportAgents.find(avatar_id);
	if (iter == mSupportAgents.end())
	{
		return -1;
	}
	return iter->second;
}

bool FSData::isSupport(const LLUUID& avatar_id)
{
	std::map<LLUUID, S32>::iterator iter = mSupportAgents.find(avatar_id);
	if (iter == mSupportAgents.end())
	{
		return false;
	}
	return (iter->second & SUPPORT);
}

bool FSData::isDeveloper(const LLUUID& avatar_id)
{
	std::map<LLUUID, S32>::iterator iter = mSupportAgents.find(avatar_id);
	if (iter == mSupportAgents.end())
	{
		return false;
	}
	return (iter->second & DEVELOPER);
}

LLSD FSData::allowedLogin()
{
	std::map<std::string, LLSD>::iterator iter = mBlockedVersions.find(LLVersionInfo::getChannelAndVersionFS());
	if (iter == mBlockedVersions.end())
	{
		return LLSD(); 
	}
	else
	{
		LLSD block = iter->second;
		bool blocked = true; // default is to block all unless there is a gridtype or grids present.
		if(block.has("gridtype"))
		{
			blocked = false;
#ifdef OPENSIM
			if ((block["gridtype"].asString() == "opensim") && LLGridManager::getInstance()->isInOpenSim())
			{
				return block;
			}
#endif
			if ((block["gridtype"].asString() == "secondlife") && LLGridManager::getInstance()->isInSecondLife())
			{
				return block;
			}
		}
		if(block.has("grids"))
		{
			blocked = false;
			LLSD grids = block["grids"];
			for (LLSD::array_iterator grid_iter = grids.beginArray();
				grid_iter != grids.endArray();
				++grid_iter)
			{
				if ((*grid_iter).asString() == LLGridManager::getInstance()->getGrid())
				{
					return block;
				}
			}
		}
		return blocked ? block : LLSD();
	}
}

BOOL FSData::isSupportGroup(const LLUUID& id)
{
	return (mSupportGroup.count(id));
}

bool FSData::isAgentFlag(const LLUUID& agent_id, flags_t flag)
{
	std::map<LLUUID, S32>::iterator iter = mSupportAgents.find(agent_id);
	if (iter == mSupportAgents.end())
	{
		return false;
	}
	return (iter->second & flag);
}

// this is called in two different places due to can recieved .xml before gCacheName is created and vice versa.
void FSData::addAgents()
{
	if (!gCacheName)
	{
		return;
	}

	for (std::map<LLUUID, S32>::iterator iter = mSupportAgents.begin(); iter != mSupportAgents.end(); ++iter)
	{
		if (iter->second & NO_SPAM)
		{
			LLUUID id = iter->first;
			std::string name;
			gCacheName->getFullName(id, name);
			LLMute mute(id, name, LLMute::EXTERNAL);
			LLMuteList::getInstance()->add(mute);
		}
	}
}

std::string FSData::processRequestForInfo(const LLUUID& requester, const std::string& message, const std::string& name, const LLUUID& sessionid)
{
	const std::string detectstring = "/reqsysinfo";
	if (message.find(detectstring) != 0)
	{
		return message;
	}

	if (!isSupport(requester) && !isDeveloper(requester))
	{
		return message;
	}

	std::string outmessage = LLTrans::getString("Reqsysinfo_Chat_NoReason");
	std::string reason("");
	if (message.length() > detectstring.length())
	{
		//there is more to it!
		reason = message.substr(detectstring.length());
		LLStringUtil::format_map_t reason_args;
		reason_args["REASON"] = reason;
		outmessage = LLTrans::getString("Reqsysinfo_Chat_Reason", reason_args);
		reason = LLTrans::getString("Reqsysinfo_Reason", reason_args);
	}
	
	LLSD args;
	args["REASON"] = reason;
	args["NAME"] = name;
	LLNotifications::instance().add("FireStormReqInfo", args, LLSD().with("from_id", requester).with("session_id", sessionid), callbackReqInfo);

	return outmessage;
}

//static
void FSData::sendInfo(const LLUUID& destination, const LLUUID& sessionid, const std::string& my_name, EInstantMessage dialog)
{
	const LLSD system_info = getSystemInfo();
	const std::string part1 = system_info["Part1"].asString();
	const std::string part2 = system_info["Part2"].asString();

	pack_instant_message(
		gMessageSystem,
		gAgentID,
		FALSE,
		gAgentSessionID,
		destination,
		my_name,
		part1,
		IM_ONLINE,
		dialog,
		sessionid
		);
	gAgent.sendReliableMessage();
	pack_instant_message(
		gMessageSystem,
		gAgentID,
		FALSE,
		gAgentSessionID,
		destination,
		my_name,
		part2,
		IM_ONLINE,
		dialog,
		sessionid
		);
	gAgent.sendReliableMessage();

	LLStringUtil::format_map_t args;
	args["DATA"] = part1 + "\n" + part2;
	gIMMgr->addMessage(gIMMgr->computeSessionID(dialog, destination), destination, my_name,
						LLTrans::getString("Reqsysinfo_Chat_Information_sent", args));
}

//static
void FSData::callbackReqInfo(const LLSD &notification, const LLSD &response)
{
	S32 option = LLNotification::getSelectedOption(notification, response);
	std::string my_name;
	LLUUID from_id = notification["payload"]["from_id"].asUUID();
	LLUUID session_id = notification["payload"]["session_id"].asUUID();

	LLAgentUI::buildFullname(my_name);

	if (option == 0) //yes
	{
		sendInfo(from_id, session_id, my_name, IM_NOTHING_SPECIAL);
	}
	else
	{
		pack_instant_message(
			gMessageSystem,
			gAgentID,
			FALSE,
			gAgentSessionID,
			from_id,
			my_name,
			"Request Denied.", // Left English intentionally as it gets sent to the support staff
			IM_ONLINE,
			IM_NOTHING_SPECIAL,
			session_id
			);
		gAgent.sendReliableMessage();
		gIMMgr->addMessage(session_id, from_id, my_name, LLTrans::getString("Reqsysinfo_Chat_Request_Denied"));
	}
}

//static
LLSD FSData::getSystemInfo()
{
	LLSD info = LLAppViewer::instance()->getViewerInfo();

	std::string sysinfo1("\n");
	sysinfo1 += llformat("%s %s (%d) %s %s (%s) %s\n\n", LLAppViewer::instance()->getSecondLifeTitle().c_str(), LLVersionInfo::getShortVersion().c_str(), LLVersionInfo::getBuild(), info["BUILD_DATE"].asString().c_str(), info["BUILD_TIME"].asString().c_str(), LLVersionInfo::getChannel().c_str(),
//<FS:CR> FIRE-8273: Add Havok/Opensim indicator to getSystemInfo()
		info["BUILD_TYPE"].asString().c_str());
// </FS:CR>
	sysinfo1 += llformat("Build with %s version %s\n\n", info["COMPILER"].asString().c_str(), info["COMPILER_VERSION"].asString().c_str());
	sysinfo1 += llformat("I am in %s located at %s (%s)\n", info["REGION"].asString().c_str(), info["HOSTNAME"].asString().c_str(), info["HOSTIP"].asString().c_str());
	sysinfo1 += llformat("%s\n\n", info["SERVER_VERSION"].asString().c_str());

	sysinfo1 += llformat("CPU: %s\n", info["CPU"].asString().c_str());
	sysinfo1 += llformat("Memory: %d MB\n", info["MEMORY_MB"].asInteger());
	sysinfo1 += llformat("OS: %s\n", info["OS_VERSION"].asString().c_str());
	sysinfo1 += llformat("Graphics Card Vendor: %s\n", info["GRAPHICS_CARD_VENDOR"].asString().c_str());
	sysinfo1 += llformat("Graphics Card: %s\n", info["GRAPHICS_CARD"].asString().c_str());
	
	if (info.has("GRAPHICS_DRIVER_VERSION"))
	{
		sysinfo1 += llformat("Graphics Card Driver Version: %s\n", info["GRAPHICS_DRIVER_VERSION"].asString().c_str());
	}

	std::string sysinfo2("\n");
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
	sysinfo2 += llformat("UI Scaling: %.3f\n", info["UI_SCALE_FACTOR"].asReal());
	sysinfo2 += llformat("Draw Distance: %d m\n", info["DRAW_DISTANCE"].asInteger());
	sysinfo2 += llformat("Bandwidth: %d kbit/s\n", info["BANDWIDTH"].asInteger());
	sysinfo2 += llformat("LOD Factor: %.3f\n", info["LOD"].asReal());
	sysinfo2 += llformat("Render quality: %s\n", info["RENDERQUALITY_FSDATA_ENGLISH"].asString().c_str()); // <FS:PP> FIRE-4785: Current render quality setting in sysinfo / about floater
	sysinfo2 += llformat("ALM enabled: %s\n", info["ALMSTATUS_FSDATA_ENGLISH"].asString().c_str());
	sysinfo2 += llformat("Texture memory: %d MB (%.2f)\n", info["TEXTUREMEMORY"].asInteger(), info["TEXTUREMEMORYMULTIPLIER"].asReal());
	sysinfo2 += "VFS (cache) creation time (UTC) " + info["VFS_DATE"].asString();

	LLSD sysinfos;
	sysinfos["Part1"] = sysinfo1;
	sysinfos["Part2"] = sysinfo2;

	return sysinfos;
}

