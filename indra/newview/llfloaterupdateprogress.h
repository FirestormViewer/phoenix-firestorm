/**
 * @file llfloaterupdateprogress.h
 * @brief Progress dialog for SoapStorm auto-update downloads
 *
 * Copyright (c) 2026, SoapStorm Project
 */

#ifndef LL_LLFLOATERUPDATEPROGRESS_H
#define LL_LLFLOATERUPDATEPROGRESS_H

#include "llfloater.h"
#include "llframetimer.h"

class LLProgressBar;
class LLTextBox;

class LLFloaterUpdateProgress : public LLFloater
{
public:
    LLFloaterUpdateProgress(const LLSD& key);
    virtual ~LLFloaterUpdateProgress();

    /*virtual*/ bool postBuild();
    /*virtual*/ void draw();

    // Set the download information
    void setDownloadInfo(const std::string& installerPath, S64 expectedSize);
    
    // Update progress based on current file size
    void updateProgress();

private:
    LLProgressBar* mProgressBar;
    LLTextBox* mUpdateText;
    LLTextBox* mProgressText;
    
    std::string mInstallerPath;
    S64 mExpectedSize;
    LLFrameTimer mUpdateTimer;
};

#endif // LL_LLFLOATERUPDATEPROGRESS_H
