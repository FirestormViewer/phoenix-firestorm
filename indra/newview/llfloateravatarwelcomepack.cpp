/**
 * @file llfloateravatarwelcomepack.cpp
 * @author Callum Prentice (callum@lindenlab.com)
 * @brief Floater container for the Avatar Welcome Pack we app
 *
 * $LicenseInfo:firstyear=2011&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2011, Linden Research, Inc.
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

#include "llfloateravatarwelcomepack.h"
#include "llmediactrl.h"
#include "lluictrlfactory.h"
#include "llviewercontrol.h"
#include "llweb.h"

#include "lfsimfeaturehandler.h"
#include "llhttpconstants.h"
#include "lllogininstance.h"
#include "llviewernetwork.h"

LLFloaterAvatarWelcomePack::LLFloaterAvatarWelcomePack(const LLSD& key)
    :   LLFloater(key),
    mAvatarPickerUrlChangedSignal() // <FS:Ansariel> Avatar chooser does not change between OpenSim grids
{
}

LLFloaterAvatarWelcomePack::~LLFloaterAvatarWelcomePack()
{
    if (mAvatarPicker)
    {
        mAvatarPicker->navigateStop();
        mAvatarPicker->clearCache();          //images are reloading each time already
        mAvatarPicker->unloadMediaSource();
    }

    // <FS:Ansariel> Avatar chooser does not change between OpenSim grids
    if (mAvatarPickerUrlChangedSignal.connected())
    {
        mAvatarPickerUrlChangedSignal.disconnect();
    }
    // </FS:Ansariel>
}

bool LLFloaterAvatarWelcomePack::postBuild()
{
    mAvatarPicker = findChild<LLMediaCtrl>("avatar_picker_contents");
    if (mAvatarPicker)
    {
        mAvatarPicker->clearCache();
        mAvatarPicker->setErrorPageURL(gSavedSettings.getString("GenericErrorPageURL"));
        // <FS:AW> optional opensim support
        //std::string url = gSavedSettings.getString("AvatarWelcomePack");
        //url = LLWeb::expandURLSubstitutions(url, LLSD());
        //mAvatarPicker->navigateTo(url, HTTP_CONTENT_TEXT_HTML);

        std::string avatar_picker_url;
#ifdef OPENSIM
        if (LLGridManager::getInstance()->isInOpenSim())
        {
            if (LLLoginInstance::getInstance()->hasResponse("avatar_picker_url"))
            {
                avatar_picker_url = LLLoginInstance::getInstance()->getResponse("avatar_picker_url").asString();
            }
        }
        else
#endif // OPENSIM
        {
            avatar_picker_url = gSavedSettings.getString("AvatarWelcomePack");
        }

        if (!avatar_picker_url.empty())
        {
            avatar_picker_url = LLWeb::expandURLSubstitutions(avatar_picker_url, LLSD());
            LL_DEBUGS("WebApi") << "AvatarPickerURL \"" << avatar_picker_url << "\"" << LL_ENDL;
            mAvatarPicker->navigateTo(avatar_picker_url, HTTP_CONTENT_TEXT_HTML);
        }
        // </FS:AW> optional opensim support
    }

    return true;
}

// <FS:Ansariel> Avatar chooser does not change between OpenSim grids
void LLFloaterAvatarWelcomePack::onOpen(const LLSD& key)
{
    // Connect during onOpen instead of ctor because LLFloaterAvatarWelcomePack instance
    // gets created before we can safely create a LFSimFeatureHandler instance!
    // Assuming we receive the avatar picker URL via login response and it
    // is the same URL being sent by region caps so we will be good for the initial
    // region the avatar logs into as well.
    if (!mAvatarPickerUrlChangedSignal.connected())
    {
        mAvatarPickerUrlChangedSignal = LFSimFeatureHandler::instance().setAvatarPickerCallback(boost::bind(&LLFloaterAvatarWelcomePack::handleUrlChanged, this, _1));
    }
}

void LLFloaterAvatarWelcomePack::handleUrlChanged(const std::string& url)
{
    getChild<LLMediaCtrl>("avatar_picker_contents")->navigateTo(LLWeb::expandURLSubstitutions(url, LLSD()), HTTP_CONTENT_TEXT_HTML);
}
// </FS:Ansariel>

