 /** 
* @file llcrashlogger.cpp
* @brief Crash logger implementation
*
* $LicenseInfo:firstyear=2003&license=viewerlgpl$
* Second Life Viewer Source Code
* Copyright (C) 2010, Linden Research, Inc.
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
* Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
* $/LicenseInfo$
*/
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <map>

#include "llcrashlogger.h"
#include "linden_common.h"
#include "llstring.h"
#include "indra_constants.h"	// CRASH_BEHAVIOR_...
#include "llerror.h"
#include "llerrorcontrol.h"
#include "lltimer.h"
#include "lldir.h"
#include "llfile.h"
#include "llsdserialize.h"
#include "lliopipe.h"
#include "llpumpio.h"
#include "llhttpclient.h"
#include "llsdserialize.h"
#include "llproxy.h"

// [SL:KB] - Patch: Viewer-CrashLookup | Checked: 2011-03-24 (Catznip-2.6.0a) | Added: Catznip-2.6.0a
#ifdef LL_WINDOWS
	#include <shellapi.h>
#endif // LL_WINDOWS
// [/SL:KB]

LLPumpIO* gServicePump;
BOOL gBreak = false;
BOOL gSent = false;

class LLCrashLoggerResponder : public LLHTTPClient::Responder
{
public:
	LLCrashLoggerResponder() 
	{
	}

	virtual void error(U32 status, const std::string& reason)
	{
		gBreak = true;
	}

	virtual void result(const LLSD& content)
	{
// [SL:KB] - Patch: Viewer-CrashLookup | Checked: 2011-03-24 (Catznip-2.6.0a) | Added: Catznip-2.6.0a
		if ( (content.has("crash_link")) && (!content["crash_link"].asString().empty()) )
		{
			((LLCrashLogger*)LLCrashLogger::instance())->setCrashInformationLink(content["crash_link"].asString());
		}
// [/SL:KB]

		gBreak = true;
		gSent = true;
	}
};

LLCrashLogger::LLCrashLogger() :
// [SL:KB] - Patch: Viewer-CrashLookup | Checked: 2011-03-24 (Catznip-2.6.0a) | Added: Catznip-2.6.0a
	mCrashLookup(NULL),
// [/SL:KB]
	mCrashBehavior(CRASH_BEHAVIOR_ASK),
	mCrashInPreviousExec(false),
	mCrashSettings("CrashSettings"),
	mSentCrashLogs(false),
	mCrashHost("")
{
	// Set up generic error handling
	setupErrorHandling();
}

LLCrashLogger::~LLCrashLogger()
{

}

// TRIM_SIZE must remain larger than LINE_SEARCH_SIZE.
const int TRIM_SIZE = 128000;
const int LINE_SEARCH_DIST = 500;
const std::string SKIP_TEXT = "\n ...Skipping... \n";
void trimSLLog(std::string& sllog)
{
	if(sllog.length() > TRIM_SIZE * 2)
	{
		std::string::iterator head = sllog.begin() + TRIM_SIZE;
		std::string::iterator tail = sllog.begin() + sllog.length() - TRIM_SIZE;
		std::string::iterator new_head = std::find(head, head - LINE_SEARCH_DIST, '\n');
		if(new_head != head - LINE_SEARCH_DIST)
		{
			head = new_head;
		}

		std::string::iterator new_tail = std::find(tail, tail + LINE_SEARCH_DIST, '\n');
		if(new_tail != tail + LINE_SEARCH_DIST)
		{
			tail = new_tail;
		}

		sllog.erase(head, tail);
		sllog.insert(head, SKIP_TEXT.begin(), SKIP_TEXT.end());
	}
}

std::string getStartupStateFromLog(std::string& sllog)
{
	std::string startup_state = "STATE_FIRST";
	std::string startup_token = "Startup state changing from ";

	int index = sllog.rfind(startup_token);
	if (index < 0 || index + startup_token.length() > sllog.length()) {
		return startup_state;
	}

	// find new line
	char cur_char = sllog[index + startup_token.length()];
	std::string::size_type newline_loc = index + startup_token.length();
	while(cur_char != '\n' && newline_loc < sllog.length())
	{
		newline_loc++;
		cur_char = sllog[newline_loc];
	}
	
	// get substring and find location of " to "
	std::string state_line = sllog.substr(index, newline_loc - index);
	std::string::size_type state_index = state_line.find(" to ");
	startup_state = state_line.substr(state_index + 4, state_line.length() - state_index - 4);

	return startup_state;
}

void LLCrashLogger::gatherFiles()
{
	updateApplication("Gathering logs...");

	// Figure out the filename of the debug log
	std::string db_file_name = gDirUtilp->getExpandedFilename(LL_PATH_LOGS,"debug_info.log");
	std::ifstream debug_log_file(db_file_name.c_str());

	// Look for it in the debug_info.log file
	if (debug_log_file.is_open())
	{
		LLSDSerialize::fromXML(mDebugLog, debug_log_file);

		mCrashInPreviousExec = mDebugLog["CrashNotHandled"].asBoolean();

		mFileMap["SecondLifeLog"] = mDebugLog["SLLog"].asString();
		mFileMap["SettingsXml"] = mDebugLog["SettingsFilename"].asString();
// [SL:KB] - Patch: Viewer-CrashReporting | Checked: 2010-11-27 (Catznip-2.6.0a) | Added: Catznip-2.4.0f
		// Remove the settings.xml path after we've retrieved it since it could contain the OS user name
		mDebugLog.erase("SettingsFilename");
// [/SL:KB]
		if(mDebugLog.has("CAFilename"))
		{
			LLCurl::setCAFile(mDebugLog["CAFilename"].asString());
		}
		else
		{
			LLCurl::setCAFile(gDirUtilp->getCAFile());
		}

		llinfos << "Using log file from debug log " << mFileMap["SecondLifeLog"] << llendl;
		llinfos << "Using settings file from debug log " << mFileMap["SettingsXml"] << llendl;
	}
/*
	else
	{
		// Figure out the filename of the second life log
		LLCurl::setCAFile(gDirUtilp->getCAFile());
		mFileMap["SecondLifeLog"] = gDirUtilp->getExpandedFilename(LL_PATH_LOGS,"SecondLife.log");
		mFileMap["SettingsXml"] = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS,"settings.xml");
	}
*/

	if(mCrashInPreviousExec)
	{
		// Restarting after freeze.
		// Replace the log file ext with .old, since the 
		// instance that launched this process has overwritten
		// SecondLife.log
		std::string log_filename = mFileMap["SecondLifeLog"];
		log_filename.replace(log_filename.size() - 4, 4, ".old");
		mFileMap["SecondLifeLog"] = log_filename;
	}

	gatherPlatformSpecificFiles();

	mFileMap.erase( "SecondLifeLog" ); // <FS:ND/> Don't send any Firestorm.log. It's likely huge and won't help for crashdump processing.
	mDebugLog.erase( "SLLog" ); // <FS:ND/> Remove SLLog, as it's a path that contains the OS user name.
	
	//Use the debug log to reconstruct the URL to send the crash report to
	if(mDebugLog.has("CrashHostUrl"))
	{
		// Crash log receiver has been manually configured.
		mCrashHost = mDebugLog["CrashHostUrl"].asString();
	}
/*
	else if(mDebugLog.has("CurrentSimHost"))
	{
		mCrashHost = "https://";
		mCrashHost += mDebugLog["CurrentSimHost"].asString();
		mCrashHost += ":12043/crash/report";
	}
	else if(mDebugLog.has("GridName"))
	{
		// This is a 'little' hacky, but its the best simple solution.
		std::string grid_host = mDebugLog["GridName"].asString();
		LLStringUtil::toLower(grid_host);

		mCrashHost = "https://login.";
		mCrashHost += grid_host;
		mCrashHost += ".lindenlab.com:12043/crash/report";
	}
*/

	// Use login servers as the alternate, since they are already load balanced and have a known name
//	mAltCrashHost = "https://login.agni.lindenlab.com:12043/crash/report";
// [SL:KB] - Patch: Viewer-CrashReporting | Checked: 2010-11-14 (Catznip-2.6.0a) | Added: Catznip-2.4.0a
	mAltCrashHost = "";
// [/SL:KB]

//	mCrashInfo["DebugLog"] = mDebugLog;
	mFileMap["StatsLog"] = gDirUtilp->getExpandedFilename(LL_PATH_LOGS,"stats.log");
	
	updateApplication("Encoding files...");

/*
	for(std::map<std::string, std::string>::iterator itr = mFileMap.begin(); itr != mFileMap.end(); ++itr)
	{
		std::ifstream f((*itr).second.c_str());
		if(!f.is_open())
		{
			std::cout << "Can't find file " << (*itr).second << std::endl;
			continue;
		}
		std::stringstream s;
		s << f.rdbuf();

		std::string crash_info = s.str();
		if(itr->first == "SecondLifeLog")
		{
			if(!mCrashInfo["DebugLog"].has("StartupState"))
			{
				mCrashInfo["DebugLog"]["StartupState"] = getStartupStateFromLog(crash_info);
			}
			trimSLLog(crash_info);
		}

		mCrashInfo[(*itr).first] = LLStringFn::strip_invalid_xml(rawstr_to_utf8(crash_info));
	}
*/
	
	// Add minidump as binary.
	std::string minidump_path = mDebugLog["MinidumpPath"];
// [SL:KB] - Patch: Viewer-CrashReporting | Checked: 2011-03-24 (Catznip-2.6.0a) | Modified: Catznip-2.6.0a
	if (gDirUtilp->fileExists(minidump_path))
	{
		mFileMap["Minidump"] = minidump_path;
		if (mCrashLookup)
		{
			mCrashLookup->initFromDump(minidump_path);
		}
		// Remove the minidump path after we've retrieved it since it could contain the OS user name
		mDebugLog.erase("MinidumpPath");
	}

	// Include debug_info.log as part of CrashReport.log
	mCrashInfo["DebugLog"] = mDebugLog;
// [/SL:KB]
/*
	if(minidump_path != "")
	{
		std::ifstream minidump_stream(minidump_path.c_str(), std::ios_base::in | std::ios_base::binary);
		if(minidump_stream.is_open())
		{
			minidump_stream.seekg(0, std::ios::end);
			size_t length = (size_t)minidump_stream.tellg();
			minidump_stream.seekg(0, std::ios::beg);
			
			LLSD::Binary data;
			data.resize(length);
			
			minidump_stream.read(reinterpret_cast<char *>(&(data[0])),length);
			minidump_stream.close();
			
			mCrashInfo["Minidump"] = data;
		}
	}
	mCrashInfo["DebugLog"].erase("MinidumpPath");
*/
}

LLSD LLCrashLogger::constructPostData()
{
	LLSD ret;
	return mCrashInfo;
}

S32 LLCrashLogger::loadCrashBehaviorSetting()
{
	// First check user_settings (in the user's home dir)
	std::string filename = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, CRASH_SETTINGS_FILE);
	if (! mCrashSettings.loadFromFile(filename))
	{
		// Next check app_settings (in the SL program dir)
		std::string filename = gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, CRASH_SETTINGS_FILE);
		mCrashSettings.loadFromFile(filename);
	}

	// If we didn't load any files above, this will return the default
	S32 value = mCrashSettings.getS32("CrashSubmitBehavior");

	// Whatever value we got, make sure it's valid
	switch (value)
	{
	case CRASH_BEHAVIOR_NEVER_SEND:
		return CRASH_BEHAVIOR_NEVER_SEND;
	case CRASH_BEHAVIOR_ALWAYS_SEND:
		return CRASH_BEHAVIOR_ALWAYS_SEND;
	}

	return CRASH_BEHAVIOR_ASK;
}

bool LLCrashLogger::saveCrashBehaviorSetting(S32 crash_behavior)
{
	switch (crash_behavior)
	{
	case CRASH_BEHAVIOR_ASK:
	case CRASH_BEHAVIOR_NEVER_SEND:
	case CRASH_BEHAVIOR_ALWAYS_SEND:
		break;
	default:
		return false;
	}

	mCrashSettings.setS32("CrashSubmitBehavior", crash_behavior);
	std::string filename = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, CRASH_SETTINGS_FILE);
	mCrashSettings.saveToFile(filename, FALSE);

	return true;
}

// [SL:KB] - Patch: Viewer-CrashReporting | Checked: 2011-03-24 (Catznip-2.6.0a) | Added: Catznip-2.6.0a
std::string getFormDataField(const std::string& strFieldName, const std::string& strFieldValue, const std::string& strBoundary)
{
	std::ostringstream streamFormPart;

	streamFormPart << "--" << strBoundary << "\r\n"
		<< "Content-Disposition: form-data; name=\"" << strFieldName << "\"\r\n\r\n"
		<< strFieldValue << "\r\n";

	return streamFormPart.str();
}
// [/SL:KB]

bool LLCrashLogger::runCrashLogPost(std::string host, LLSD data, std::string msg, int retries, int timeout)
{
	gBreak = false;
	for(int i = 0; i < retries; ++i)
	{
		//status_message = llformat("%s, try %d...", msg.c_str(), i+1);
//		LLHTTPClient::post(host, data, new LLCrashLoggerResponder(), timeout);
// [SL:KB] - Patch: Viewer-CrashReporting | Checked: 2011-03-24 (Catznip-2.6.0a) | Modified: Catznip-2.6.0a
		static const std::string BOUNDARY("------------abcdef012345xyZ");

		LLSD headers = LLSD::emptyMap();

		headers["Accept"] = "*/*";
		headers["Content-Type"] = "multipart/form-data; boundary=" + BOUNDARY;

		std::ostringstream body;

		/*
		 * Send viewer information for the upload handler's benefit
		 */
		if (mDebugLog.has("ClientInfo"))
		{
			body << getFormDataField("viewer_channel", mDebugLog["ClientInfo"]["Name"], BOUNDARY);
			body << getFormDataField("viewer_version", mDebugLog["ClientInfo"]["Version"], BOUNDARY);
			body << getFormDataField("viewer_platform", mDebugLog["ClientInfo"]["Platform"], BOUNDARY);
		}

		/*
		 * Include crash analysis pony
		 */
		if (mCrashLookup)
		{
			body << getFormDataField("crash_module_name", mCrashLookup->getModuleName(), BOUNDARY);
			body << getFormDataField("crash_module_version", llformat("%I64d", mCrashLookup->getModuleVersion()), BOUNDARY);
			body << getFormDataField("crash_module_versionstring", mCrashLookup->getModuleVersionString(), BOUNDARY);
			body << getFormDataField("crash_module_displacement", llformat("%I64d", mCrashLookup->getModuleDisplacement()), BOUNDARY);
		}

		/*
		 * Add the actual crash logs
		 */
		for (std::map<std::string, std::string>::const_iterator itFile = mFileMap.begin(), endFile = mFileMap.end();
				itFile != endFile; ++itFile)
		{
			if (itFile->second.empty())
				continue;

			body << "--" << BOUNDARY << "\r\n"
				 <<	"Content-Disposition: form-data; name=\"crash_report[]\"; "
				 << "filename=\"" << gDirUtilp->getBaseFileName(itFile->second) << "\"\r\n"
				 << "Content-Type: application/octet-stream"
				 << "\r\n\r\n";

			llifstream fstream(itFile->second, std::iostream::binary | std::iostream::out);
			if (fstream.is_open())
			{
				fstream.seekg(0, std::ios::end);

				U32 fileSize = fstream.tellg();
				fstream.seekg(0, std::ios::beg);
				std::vector<char> fileBuffer(fileSize);

				fstream.read(&fileBuffer[0], fileSize);
				body.write(&fileBuffer[0], fileSize);

				fstream.close();
			}

			body <<	"\r\n";
		}

		/*
		 * Close the post
		 */
		body << "--" << BOUNDARY << "--\r\n";

		// postRaw() takes ownership of the buffer and releases it later.
		size_t size = body.str().size();
		U8 *data = new U8[size];
		memcpy(data, body.str().data(), size);

		// Send request
		LLHTTPClient::postRaw(host, data, size, new LLCrashLoggerResponder(), headers);
// [/SL:KB]
		//updateApplication(llformat("%s, try %d...", msg.c_str(), i+1));
		//LLHTTPClient::post(host, data, new LLCrashLoggerResponder(), timeout);

		while(!gBreak)
		{
			updateApplication(); // No new message, just pump the IO
		}
		if(gSent)
		{
			return gSent;
		}
	}
	return gSent;
}

bool LLCrashLogger::sendCrashLogs()
{
	gatherFiles();

	LLSD post_data;
	post_data = constructPostData();

	updateApplication("Sending reports...");

//	std::string dump_path = gDirUtilp->getExpandedFilename(LL_PATH_LOGS,
//															   "SecondLifeCrashReport");
// [SL:KB] - Patch: Viewer-CrashReporting | Checked: 2010-11-14 (Catznip-2.6.0a) | Added: Catznip-2.4.0a
	std::string dump_path = gDirUtilp->getExpandedFilename(LL_PATH_LOGS,
															   "FirestormCrashReport");
// [/SL:KB]
	std::string report_file = dump_path + ".log";

	std::ofstream out_file(report_file.c_str());
	LLSDSerialize::toPrettyXML(post_data, out_file);
	out_file.close();

// [SL:KB] - Patch: Viewer-CrashReporting | Checked: 2010-11-14 (Catznip-2.6.0a) | Added: Catznip-2.4.0a
	mFileMap["CrashReportLog"] = report_file;
// [/SL:KB]

	bool sent = false;

	//*TODO: Translate
	if(mCrashHost != "")
	{
		sent = runCrashLogPost(mCrashHost, post_data, std::string("Sending to server"), 3, 5);
	}

//	if(!sent)
// [SL:KB] - Patch: Viewer-CrashReporting | Checked: 2010-11-14 (Catznip-2.6.0a) | Added: Catznip-2.4.0a
	if ( (!sent) && (!mAltCrashHost.empty()) )
// [/SL:KB]
	{
		sent = runCrashLogPost(mAltCrashHost, post_data, std::string("Sending to alternate server"), 3, 5);
	}
	    
	mSentCrashLogs = sent;

// [SL:KB] - Patch: Viewer-CrashLookup | Checked: 2011-03-24 (Catznip-2.6.0a) | Added: Catznip-2.6.0a
	if (!mCrashLink.empty())
	{
#if LL_WINDOWS && LL_SEND_CRASH_REPORTS
		if (IDYES == MessageBox(NULL, L"Additional information is available about this crash. Display?", L"Crash Information", MB_YESNO))
		{
			wchar_t wstrCrashLink[512];
			mbstowcs_s(NULL, wstrCrashLink, 512, mCrashLink.c_str(), _TRUNCATE);

			SHELLEXECUTEINFO sei = {0};
			sei.cbSize = sizeof(SHELLEXECUTEINFO);
			sei.fMask = SEE_MASK_NOASYNC;
			sei.lpVerb = L"open";
			sei.lpFile = wstrCrashLink;
			ShellExecuteEx(&sei); 
		}
#endif // LL_WINDOWS && LL_SEND_CRASH_REPORTS
	}
// [/SL:KB]

	return true;
}

void LLCrashLogger::updateApplication(const std::string& message)
{
	gServicePump->pump();
    gServicePump->callback();
	if (!message.empty()) llinfos << message << llendl;
}

bool LLCrashLogger::init()
{
	LLCurl::initClass(false);

	// We assume that all the logs we're looking for reside on the current drive
	gDirUtilp->initAppDirs("Firestorm");

	LLError::initForApplication(gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, ""));

	// Default to the product name "Second Life" (this is overridden by the -name argument)
	
	// <FS:ND> Change default to Firestorm
	//	mProductName = "Second Life";
	mProductName = "Firestorm";
	// </FS:ND>

	// Rename current log file to ".old"
	std::string old_log_file = gDirUtilp->getExpandedFilename(LL_PATH_LOGS, "crashreport.log.old");
	std::string log_file = gDirUtilp->getExpandedFilename(LL_PATH_LOGS, "crashreport.log");
	LLFile::rename(log_file.c_str(), old_log_file.c_str());

	// Set the log file to crashreport.log
	LLError::logToFile(log_file);
	
	mCrashSettings.declareS32("CrashSubmitBehavior", CRASH_BEHAVIOR_ALWAYS_SEND,
							  "Controls behavior when viewer crashes "
							  "(0 = ask before sending crash report, "
							  "1 = always send crash report, "
							  "2 = never send crash report)");

	llinfos << "Loading crash behavior setting" << llendl;
	mCrashBehavior = loadCrashBehaviorSetting();

	// If user doesn't want to send, bail out
	if (mCrashBehavior == CRASH_BEHAVIOR_NEVER_SEND)
	{
		llinfos << "Crash behavior is never_send, quitting" << llendl;
		return false;
	}

	gServicePump = new LLPumpIO(gAPRPoolp);
	gServicePump->prime(gAPRPoolp);
	LLHTTPClient::setPump(*gServicePump);

	//If we've opened the crash logger, assume we can delete the marker file if it exists
	if( gDirUtilp )
	{
		std::string marker_file = gDirUtilp->getExpandedFilename(LL_PATH_LOGS,
																 "SecondLife.exec_marker");
		LLAPRFile::remove( marker_file );
	}
	
	return true;
}

// For cleanup code common to all platforms.
void LLCrashLogger::commonCleanup()
{
	LLProxy::cleanupClass();
}
