/**
 * @file llautoupdatechecker.cpp
 * @brief Implementation of automatic update checker
 */

#include "llviewerprecompiledheaders.h"

#include "llautoupdatechecker.h"
#include "llversioninfo.h"
#include "llcorehttputil.h"
#include "llnotificationsutil.h"
#include "llviewercontrol.h"
#include "llsdjson.h"
#include "llweb.h"
#include <boost/json.hpp>

#include <sstream>

// Update check URL
const std::string UPDATE_CHECK_URL = "https://chonks.net/soapstorm/update_info.json";

LLAutoUpdateChecker::LLAutoUpdateChecker()
:   mUpdateAvailable(false),
    mCheckInProgress(false)
{
    LL_INFOS("AutoUpdate") << "Auto-update checker initialized" << LL_ENDL;
}

LLAutoUpdateChecker::~LLAutoUpdateChecker()
{
}

void LLAutoUpdateChecker::checkForUpdate()
{
    // Don't check if already checking
    if (mCheckInProgress)
    {
        LL_DEBUGS("AutoUpdate") << "Update check already in progress" << LL_ENDL;
        return;
    }

    // Don't check if disabled in preferences
    if (!gSavedSettings.getBOOL("FSAutoUpdateCheck"))
    {
        LL_DEBUGS("AutoUpdate") << "Auto-update check disabled in preferences" << LL_ENDL;
        return;
    }

    // SkoomaStorm party build: never check for updates. The feed only
    // knows Soapstorm builds, so any offer would "upgrade" this install
    // to stock Soapstorm.
    if (LLVersionInfo::instance().getChannel().find("SkoomaStorm") != std::string::npos)
    {
        LL_INFOS("AutoUpdate") << "SkoomaStorm build, skipping update check" << LL_ENDL;
        return;
    }

    LL_INFOS("AutoUpdate") << "Starting update check" << LL_ENDL;
    mCheckInProgress = true;
    LLCoros::instance().launch("AutoUpdateCheck", 
        [this]() { checkUpdateCoro(); });
}

void LLAutoUpdateChecker::checkUpdateCoro()
{
    // Fetch update info from server
    LLCore::HttpRequest::policy_t httpPolicy(LLCore::HttpRequest::DEFAULT_POLICY_ID);
    LLCoreHttpUtil::HttpCoroutineAdapter::ptr_t httpAdapter =
        std::make_shared<LLCoreHttpUtil::HttpCoroutineAdapter>("AutoUpdateCheck", httpPolicy);

    LLCore::HttpRequest::ptr_t httpRequest = std::make_shared<LLCore::HttpRequest>();
    LLCore::HttpOptions::ptr_t httpOpts = std::make_shared<LLCore::HttpOptions>();
    httpOpts->setFollowRedirects(true);

    LL_INFOS("AutoUpdate") << "Fetching update info from: " << UPDATE_CHECK_URL << LL_ENDL;

    // Use getRawAndSuspend to get raw JSON instead of getAndSuspend which expects LLSD format
    LLSD result = httpAdapter->getRawAndSuspend(httpRequest, UPDATE_CHECK_URL, httpOpts);

    LLCore::HttpStatus status = LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(result);
    
    if (!status)
    {
        LL_WARNS("AutoUpdate") << "Failed to fetch update info: " << status.toString() << LL_ENDL;
        mCheckInProgress = false;
        return;
    }

    // Parse the response
    const LLSD::Binary &rawBody = result[LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS_RAW].asBinary();
    std::string body(rawBody.begin(), rawBody.end());
    
    LL_INFOS("AutoUpdate") << "Response body length: " << body.length() << " bytes" << LL_ENDL;
    LL_DEBUGS("AutoUpdate") << "Response body: " << body << LL_ENDL;
    
    // Parse JSON using boost::json with error handling
    // Note: Server doesn't send Content-Length (Cloudflare tunnel workaround),
    // but boost::json::parse can handle complete strings
    boost::system::error_code ec;
    boost::json::value jsonVal = boost::json::parse(body, ec);
    
    if (ec.failed())
    {
        LL_WARNS("AutoUpdate") << "Failed to parse update info JSON: " << ec.message() << LL_ENDL;
        mCheckInProgress = false;
        return;
    }
    
    LLSD updateData = LlsdFromJson(jsonVal);

    if (!updateData.has("version") || !updateData.has("download_url"))
    {
        LL_WARNS("AutoUpdate") << "Update info missing required fields" << LL_ENDL;
        mCheckInProgress = false;
        return;
    }

    mUpdateInfo = updateData;
    std::string remoteVersion = updateData["version"].asString();
    std::string currentVersion = LLVersionInfo::instance().getVersion();

    LL_INFOS("AutoUpdate") << "Remote version: " << remoteVersion 
                           << " Current version: " << currentVersion << LL_ENDL;

    // Check if user skipped this version
    std::string skippedVersion = gSavedSettings.getString("FSSkippedUpdateVersion");
    if (remoteVersion == skippedVersion)
    {
        LL_INFOS("AutoUpdate") << "User has skipped version " << remoteVersion << LL_ENDL;
        mCheckInProgress = false;
        return;
    }

    // Compare versions
    if (isNewerVersion(remoteVersion, currentVersion))
    {
        LL_INFOS("AutoUpdate") << "New version available: " << remoteVersion << LL_ENDL;
        mUpdateAvailable = true;
        showUpdateNotification();
    }
    else
    {
        LL_INFOS("AutoUpdate") << "No update available" << LL_ENDL;
        mUpdateAvailable = false;
    }
    
    mCheckInProgress = false;
}

bool LLAutoUpdateChecker::isNewerVersion(const std::string& remote_version, const std::string& current_version)
{
    S32 remote_major, remote_minor, remote_patch, remote_build;
    S32 current_major, current_minor, current_patch, current_build;

    parseVersion(remote_version, remote_major, remote_minor, remote_patch, remote_build);
    parseVersion(current_version, current_major, current_minor, current_patch, current_build);

    if (remote_major > current_major) return true;
    if (remote_major < current_major) return false;

    if (remote_minor > current_minor) return true;
    if (remote_minor < current_minor) return false;

    if (remote_patch > current_patch) return true;
    if (remote_patch < current_patch) return false;

    if (remote_build > current_build) return true;

    return false;
}

void LLAutoUpdateChecker::parseVersion(const std::string& version_str, S32& major, S32& minor, S32& patch, S32& build)
{
    major = minor = patch = build = 0;

    std::istringstream iss(version_str);
    char dot;
    
    iss >> major;
    if (iss.peek() == '.') iss >> dot;
    iss >> minor;
    if (iss.peek() == '.') iss >> dot;
    iss >> patch;
    if (iss.peek() == '.') iss >> dot;
    iss >> build;
}

void LLAutoUpdateChecker::showUpdateNotification()
{
    LLSD args;
    args["VERSION"] = mUpdateInfo["version"].asString();
    args["CURRENT_VERSION"] = LLVersionInfo::instance().getVersion();
    
    if (mUpdateInfo.has("file_size_mb"))
    {
        args["SIZE"] = llformat("%.1f MB", mUpdateInfo["file_size_mb"].asReal());
    }
    
    LLSD payload;
    payload["update_info"] = mUpdateInfo;

    LLNotificationsUtil::add("FSUpdateAvailable", args, payload,
        [this](const LLSD& notification, const LLSD& response)
        {
            S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
            
            if (option == 0) // Download Now
            {
                openDownloadPage();
            }
            else if (option == 1) // Remind Later
            {
                // Do nothing, will check again next startup
            }
            else if (option == 2) // Skip This Version
            {
                skipThisVersion();
            }
        });
}

void LLAutoUpdateChecker::openDownloadPage()
{
    if (!mUpdateInfo.has("download_url"))
    {
        LL_WARNS("AutoUpdate") << "No download URL available" << LL_ENDL;
        return;
    }

    std::string downloadUrl = mUpdateInfo["download_url"].asString();
    LL_INFOS("AutoUpdate") << "Opening download page: " << downloadUrl << LL_ENDL;
    
    // Open the download URL in the user's default browser
    LLWeb::loadURL(downloadUrl);
}

void LLAutoUpdateChecker::skipThisVersion()
{
    if (mUpdateInfo.has("version"))
    {
        std::string version = mUpdateInfo["version"].asString();
        gSavedSettings.setString("FSSkippedUpdateVersion", version);
        LL_INFOS("AutoUpdate") << "Skipped version: " << version << LL_ENDL;
    }
}
