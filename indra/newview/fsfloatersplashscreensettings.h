/**
 * @file fsfloatersplashscreensettings.h
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

#ifndef FS_FLOATERSPLASHSCREENSETTINGS_H
#define FS_FLOATERSPLASHSCREENSETTINGS_H

#include "llfloater.h"

class LLCheckBoxCtrl;

class FSFloaterSplashScreenSettings : public LLFloater
{
public:
    FSFloaterSplashScreenSettings(const LLSD& key);
    virtual ~FSFloaterSplashScreenSettings();

    /*virtual*/ bool postBuild();
    /*virtual*/ void onOpen(const LLSD& key);

private:
    void onSettingChanged();
    void loadSettings();
    void saveSettings();

    LLCheckBoxCtrl* mHideTopBarCheck;
    LLCheckBoxCtrl* mHideBlogsCheck;
    LLCheckBoxCtrl* mHideDestinationsCheck;
    LLCheckBoxCtrl* mUseGrayModeCheck;
    LLCheckBoxCtrl* mUseHighContrastCheck;
    LLCheckBoxCtrl* mUseAllCapsCheck;
    LLCheckBoxCtrl* mUseLargerFontsCheck;
    LLCheckBoxCtrl* mNoTransparencyCheck;
};

#endif // FS_FLOATERSPLASHSCREENSETTINGS_H

