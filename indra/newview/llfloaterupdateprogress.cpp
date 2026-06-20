/**
 * @file llfloaterupdateprogress.cpp
 * @brief Progress dialog for SoapStorm auto-update downloads
 *
 * Copyright (c) 2026, SoapStorm Project
 */

#include "llviewerprecompiledheaders.h"

#include "llfloaterupdateprogress.h"
#include "llfloaterreg.h"
#include "llprogressbar.h"
#include "lltextbox.h"
#include "llfile.h"

LLFloaterUpdateProgress::LLFloaterUpdateProgress(const LLSD& key)
:   LLFloater(key),
    mProgressBar(NULL),
    mUpdateText(NULL),
    mProgressText(NULL),
    mExpectedSize(0)
{
}

LLFloaterUpdateProgress::~LLFloaterUpdateProgress()
{
}

bool LLFloaterUpdateProgress::postBuild()
{
    mProgressBar = getChild<LLProgressBar>("progress_bar");
    mUpdateText = getChild<LLTextBox>("update_text");
    mProgressText = getChild<LLTextBox>("progress_text");
    
    // Start with 0% progress
    if (mProgressBar)
    {
        mProgressBar->setValue(0.0f);
    }
    
    // Center the floater
    center();
    
    return true;
}

void LLFloaterUpdateProgress::draw()
{
    // Update progress every frame
    if (mUpdateTimer.getElapsedTimeF32() > 0.25f) // Update every 250ms
    {
        updateProgress();
        mUpdateTimer.reset();
    }
    
    LLFloater::draw();
}

void LLFloaterUpdateProgress::setDownloadInfo(const std::string& installerPath, S64 expectedSize)
{
    mInstallerPath = installerPath;
    mExpectedSize = expectedSize;
    
    // Reset timer
    mUpdateTimer.reset();
    
    LL_INFOS("AutoUpdate") << "Progress dialog tracking: " << installerPath 
                           << " expected size: " << expectedSize << " bytes" << LL_ENDL;
}

void LLFloaterUpdateProgress::updateProgress()
{
    if (mInstallerPath.empty() || mExpectedSize <= 0)
    {
        return;
    }
    
    // Check current file size
    S64 currentSize = 0;
    if (LLFile::isfile(mInstallerPath))
    {
        llstat file_stat;
        if (LLFile::stat(mInstallerPath, &file_stat) == 0)
        {
            currentSize = file_stat.st_size;
        }
    }
    
    // Calculate progress
    F32 progress = 0.0f;
    if (mExpectedSize > 0)
    {
        progress = (F32)currentSize / (F32)mExpectedSize;
        progress = llclamp(progress, 0.0f, 1.0f);
    }
    
    // Update progress bar
    if (mProgressBar)
    {
        mProgressBar->setValue(progress * 100.0f);
    }
    
    // Update text
    if (mProgressText)
    {
        F32 currentMB = (F32)currentSize / (1024.0f * 1024.0f);
        F32 expectedMB = (F32)mExpectedSize / (1024.0f * 1024.0f);
        
        std::string text = llformat("%.1f MB / %.1f MB", currentMB, expectedMB);
        mProgressText->setText(text);
    }
}
