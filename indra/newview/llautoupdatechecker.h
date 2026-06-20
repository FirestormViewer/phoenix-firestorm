/**
 * @file llautoupdatechecker.h
 * @brief Automatic update checker for SoapStorm viewer
 *
 * Checks for new viewer versions from a remote server and handles
 * the download and installation process with user consent.
 */

#ifndef LL_LLAUTOUPDATECHECKER_H
#define LL_LLAUTOUPDATECHECKER_H

#include "llsingleton.h"
#include "lleventcoro.h"
#include "llcoros.h"
#include "llsd.h"
#include <string>

class LLAutoUpdateChecker : public LLSingleton<LLAutoUpdateChecker>
{
    LLSINGLETON(LLAutoUpdateChecker);
    virtual ~LLAutoUpdateChecker();

public:
    // Main entry point - checks for updates
    void checkForUpdate();
    
    // Open browser to download the update installer
    void openDownloadPage();
    
    // Skip the current available version
    void skipThisVersion();
    
    // Get current update info
    const LLSD& getUpdateInfo() const { return mUpdateInfo; }
    
    // Check if an update is available
    bool isUpdateAvailable() const { return mUpdateAvailable; }

private:
    // Coroutine for checking updates
    void checkUpdateCoro();
    
    // Compare version strings
    bool isNewerVersion(const std::string& remote_version, const std::string& current_version);
    
    // Parse version string into components
    void parseVersion(const std::string& version_str, S32& major, S32& minor, S32& patch, S32& build);
    
    // Show update notification to user
    void showUpdateNotification();

private:
    LLSD mUpdateInfo;
    bool mUpdateAvailable;
    bool mCheckInProgress;
};

#endif // LL_LLAUTOUPDATECHECKER_H
