/**
 * @file fsfloatersplashscreensettings.cpp
 * @brief Splash screen settings floater
 *
 * $LicenseInfo:firstyear=2025&license=viewerlgpl$
 * Copyright (c) 2025 The Phoenix Firestorm Project, Inc.
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

#include "fsfloatersplashscreensettings.h"
#include "llcheckboxctrl.h"
#include "llviewercontrol.h"

FSFloaterSplashScreenSettings::FSFloaterSplashScreenSettings(const LLSD& key) :
    LLFloater(key),
    mHideTopBarCheck(nullptr),
    mHideBlogsCheck(nullptr),
    mHideDestinationsCheck(nullptr),
    mUseGrayModeCheck(nullptr),
    mUseHighContrastCheck(nullptr),
    mUseAllCapsCheck(nullptr),
    mUseLargerFontsCheck(nullptr),
    mNoTransparencyCheck(nullptr)
{
}

FSFloaterSplashScreenSettings::~FSFloaterSplashScreenSettings()
{
}

bool FSFloaterSplashScreenSettings::postBuild()
{
    mHideTopBarCheck = getChild<LLCheckBoxCtrl>("hide_top_bar");
    mHideBlogsCheck = getChild<LLCheckBoxCtrl>("hide_blogs");
    mHideDestinationsCheck = getChild<LLCheckBoxCtrl>("hide_destinations");
    mUseGrayModeCheck = getChild<LLCheckBoxCtrl>("use_gray_mode");
    mUseHighContrastCheck = getChild<LLCheckBoxCtrl>("use_high_contrast");
    mUseAllCapsCheck = getChild<LLCheckBoxCtrl>("use_all_caps");
    mUseLargerFontsCheck = getChild<LLCheckBoxCtrl>("use_larger_fonts");
    mNoTransparencyCheck = getChild<LLCheckBoxCtrl>("no_transparency");

    mHideTopBarCheck->setCommitCallback(boost::bind(&FSFloaterSplashScreenSettings::onSettingChanged, this));
    mHideBlogsCheck->setCommitCallback(boost::bind(&FSFloaterSplashScreenSettings::onSettingChanged, this));
    mHideDestinationsCheck->setCommitCallback(boost::bind(&FSFloaterSplashScreenSettings::onSettingChanged, this));
    mUseGrayModeCheck->setCommitCallback(boost::bind(&FSFloaterSplashScreenSettings::onSettingChanged, this));
    mUseHighContrastCheck->setCommitCallback(boost::bind(&FSFloaterSplashScreenSettings::onSettingChanged, this));
    mUseAllCapsCheck->setCommitCallback(boost::bind(&FSFloaterSplashScreenSettings::onSettingChanged, this));
    mUseLargerFontsCheck->setCommitCallback(boost::bind(&FSFloaterSplashScreenSettings::onSettingChanged, this));
    mNoTransparencyCheck->setCommitCallback(boost::bind(&FSFloaterSplashScreenSettings::onSettingChanged, this));

    loadSettings();

    return true;
}

void FSFloaterSplashScreenSettings::onOpen(const LLSD& key)
{
    LLFloater::onOpen(key);
    loadSettings();
}

void FSFloaterSplashScreenSettings::loadSettings()
{
    mHideTopBarCheck->setValue(gSavedSettings.getBOOL("FSSplashScreenHideTopBar"));
    mHideBlogsCheck->setValue(gSavedSettings.getBOOL("FSSplashScreenHideBlogs"));
    mHideDestinationsCheck->setValue(gSavedSettings.getBOOL("FSSplashScreenHideDestinations"));
    mUseGrayModeCheck->setValue(gSavedSettings.getBOOL("FSSplashScreenUseGrayMode"));
    mUseHighContrastCheck->setValue(gSavedSettings.getBOOL("FSSplashScreenUseHighContrast"));
    mUseAllCapsCheck->setValue(gSavedSettings.getBOOL("FSSplashScreenUseAllCaps"));
    mUseLargerFontsCheck->setValue(gSavedSettings.getBOOL("FSSplashScreenUseLargerFonts"));
    mNoTransparencyCheck->setValue(gSavedSettings.getBOOL("FSSplashScreenNoTransparency"));
}

void FSFloaterSplashScreenSettings::saveSettings()
{
    gSavedSettings.setBOOL("FSSplashScreenHideTopBar", mHideTopBarCheck->getValue().asBoolean());
    gSavedSettings.setBOOL("FSSplashScreenHideBlogs", mHideBlogsCheck->getValue().asBoolean());
    gSavedSettings.setBOOL("FSSplashScreenHideDestinations", mHideDestinationsCheck->getValue().asBoolean());
    gSavedSettings.setBOOL("FSSplashScreenUseGrayMode", mUseGrayModeCheck->getValue().asBoolean());
    gSavedSettings.setBOOL("FSSplashScreenUseHighContrast", mUseHighContrastCheck->getValue().asBoolean());
    gSavedSettings.setBOOL("FSSplashScreenUseAllCaps", mUseAllCapsCheck->getValue().asBoolean());
    gSavedSettings.setBOOL("FSSplashScreenUseLargerFonts", mUseLargerFontsCheck->getValue().asBoolean());
    gSavedSettings.setBOOL("FSSplashScreenNoTransparency", mNoTransparencyCheck->getValue().asBoolean());
}

void FSFloaterSplashScreenSettings::onSettingChanged()
{
    saveSettings();
}

