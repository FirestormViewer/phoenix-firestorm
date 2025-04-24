/**
 * @file llfloaternewfeaturenotification.cpp
 * @brief LLFloaterNewFeatureNotification class implementation
 *
 * $LicenseInfo:firstyear=2023&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2023, Linden Research, Inc.
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

#include "llviewerprecompiledheaders.h"

#include "llfloaternewfeaturenotification.h"


LLFloaterNewFeatureNotification::LLFloaterNewFeatureNotification(const LLSD& key)
  : LLFloater(key)
{
}

LLFloaterNewFeatureNotification::~LLFloaterNewFeatureNotification()
{
}

bool LLFloaterNewFeatureNotification::postBuild()
{
    setCanDrag(false);
    getChild<LLButton>("close_btn")->setCommitCallback(boost::bind(&LLFloaterNewFeatureNotification::onCloseBtn, this));

    const std::string title_txt = "title_txt";
    const std::string dsc_txt = "description_txt";
    // <FS:Beq> FIRE-35393 stop crashing just cos whirly does something daft and blames Atlas for it
    std::string feature = getKey().asString();
    if(feature.empty() )
    {
        LL_WARNS("FloaterNewFeature") << "Unexpected failure - No feature name NewFeatureNotification." << LL_ENDL;
        return false;
    }
    if (!hasString( title_txt + "_" + feature ) )
    {
        LL_WARNS("FloaterNewFeature") << "No string for " << title_txt + "_" + feature << LL_ENDL;
        return false;
    }
    if (!hasString( dsc_txt + "_" + feature ) )
    {
        LL_WARNS("FloaterNewFeature") << "No string for " << dsc_txt + "_" + feature << LL_ENDL;
        return false;
    }
    // </FS:Beq>
    
    getChild<LLUICtrl>(title_txt)->setValue(getString(title_txt + "_" + feature));
    getChild<LLUICtrl>(dsc_txt)->setValue(getString(dsc_txt + "_" + feature));

    if (getKey().asString() == "gltf")
    {
        LLRect rect = getRect();
        // make automatic?
        reshape(rect.getWidth() + 90, rect.getHeight() + 45);
    }

    return true;
}

void LLFloaterNewFeatureNotification::onOpen(const LLSD& key)
{
    centerOnScreen();
}

void LLFloaterNewFeatureNotification::onCloseBtn()
{
    closeFloater();
}

void LLFloaterNewFeatureNotification::centerOnScreen()
{
    LLVector2 window_size = LLUI::getInstance()->getWindowSize();
    centerWithin(LLRect(0, 0, ll_round(window_size.mV[VX]), ll_round(window_size.mV[VY])));
    LLFloaterView* parent = dynamic_cast<LLFloaterView*>(getParent());
    if (parent)
    {
        parent->bringToFront(this);
    }
}

