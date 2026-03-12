/**
* @file llfloaterhoverheight.cpp
* @brief Controller for self avatar hover height
* @author vir@lindenlab.com
*
* $LicenseInfo:firstyear=2014&license=viewerlgpl$
* Second Life Viewer Source Code
* Copyright (C) 2014, Linden Research, Inc.
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

#include "llfloaterhoverheight.h"
#include "llsliderctrl.h"
#include "llviewercontrol.h"
#include "llsdserialize.h"
#include "llagent.h"
#include "llviewerregion.h"
#include "llvoavatarself.h"

#ifdef OPENSIM
#include "llviewernetwork.h"
#endif

LLFloaterHoverHeight::LLFloaterHoverHeight(const LLSD& key) : LLFloater(key)
,   mModifiers(MASK_NONE)  // <FS:Zi> FIRE-33859 - Add +/- and reset buttons to hover height slider
{
}

void LLFloaterHoverHeight::syncFromPreferenceSetting(void *user_data, bool update_offset)
{
    F32 value = gSavedPerAccountSettings.getF32("AvatarHoverOffsetZ");

    LLFloaterHoverHeight *self = static_cast<LLFloaterHoverHeight*>(user_data);
    LLSliderCtrl* sldrCtrl = self->getChild<LLSliderCtrl>("HoverHeightSlider");
    sldrCtrl->setValue(value,false);

    // <FS:Ansariel> Legacy baking avatar z-offset
    //if (isAgentAvatarValid() && update_offset)
    //{
    //  LLVector3 offset(0.0, 0.0, llclamp(value,MIN_HOVER_Z,MAX_HOVER_Z));
    //  LL_INFOS("Avatar") << "setting hover from preference setting " << offset[2] << LL_ENDL;
    //  gAgentAvatarp->setHoverOffset(offset);

    //  //gAgentAvatarp->sendHoverHeight();
    //}
    // </FS:Ansariel>
}

bool LLFloaterHoverHeight::postBuild()
{
    LLSliderCtrl* sldrCtrl = getChild<LLSliderCtrl>("HoverHeightSlider");
    sldrCtrl->setMinValue(MIN_HOVER_Z);
    sldrCtrl->setMaxValue(MAX_HOVER_Z);
    sldrCtrl->setSliderMouseUpCallback(boost::bind(&LLFloaterHoverHeight::onFinalCommit,this));
    sldrCtrl->setSliderEditorCommitCallback(boost::bind(&LLFloaterHoverHeight::onFinalCommit,this));
    childSetCommitCallback("HoverHeightSlider", &LLFloaterHoverHeight::onSliderMoved, NULL);
    // <FS:Zi> FIRE-33859 - Add +/- and reset buttons to hover height slider
    childSetCommitCallback("PlusButton", boost::bind(&LLFloaterHoverHeight::onPlusButtonClicked, this), nullptr);
    childSetCommitCallback("MinusButton", boost::bind(&LLFloaterHoverHeight::onMinusButtonClicked, this), nullptr);
    childSetCommitCallback("ResetButton", boost::bind(&LLFloaterHoverHeight::onResetButtonClicked, this), nullptr);
    // </FS:Zi>

    // Initialize slider from pref setting.
    syncFromPreferenceSetting(this);
    // Update slider on future pref changes.
    if (gSavedPerAccountSettings.getControl("AvatarHoverOffsetZ"))
    {
        gSavedPerAccountSettings.getControl("AvatarHoverOffsetZ")->getCommitSignal()->connect(boost::bind(&syncFromPreferenceSetting, this, false));
    }
    else
    {
        LL_WARNS() << "Control not found for AvatarHoverOffsetZ" << LL_ENDL;
    }

    updateEditEnabled();

    if (!mRegionChangedSlot.connected())
    {
        mRegionChangedSlot = gAgent.addRegionChangedCallback(boost::bind(&LLFloaterHoverHeight::onRegionChanged,this));
    }
    // Set up based on initial region.
    onRegionChanged();

    return true;
}

void LLFloaterHoverHeight::onClose(bool app_quitting)
{
    if (mRegionChangedSlot.connected())
    {
        mRegionChangedSlot.disconnect();
    }
}

// static
void LLFloaterHoverHeight::onSliderMoved(LLUICtrl* ctrl, void* userData)
{
    if (isAgentAvatarValid())
    {
        LLSliderCtrl* sldrCtrl = static_cast<LLSliderCtrl*>(ctrl);
        F32 value = sldrCtrl->getValueF32();
        LLVector3 offset(0.0, 0.0, llclamp(value,MIN_HOVER_Z,MAX_HOVER_Z));
        LL_INFOS("Avatar") << "setting hover from slider moved" << offset[2] << LL_ENDL;
        // <FS:Ansariel> Legacy baking avatar z-offset
        //gAgentAvatarp->setHoverOffset(offset, false);
        if (gAgent.getRegion() && gAgent.getRegion()->avatarHoverHeightEnabled())
        {
            if (sldrCtrl->isMouseHeldDown())
            {
                gAgentAvatarp->setHoverOffset(offset, false);
            }
            else
            {
                gSavedPerAccountSettings.setF32("AvatarHoverOffsetZ", value);
            }
        }
        else if (!gAgentAvatarp->isUsingServerBakes())
        {
            gSavedPerAccountSettings.setF32("AvatarHoverOffsetZ", value);
        }
        // </FS:Ansariel>
    }
}

// Do send-to-the-server work when slider drag completes, or new
// value entered as text.
void LLFloaterHoverHeight::onFinalCommit()
{
    LLSliderCtrl* sldrCtrl = getChild<LLSliderCtrl>("HoverHeightSlider");
    F32 value = sldrCtrl->getValueF32();
    // <FS:Ansariel> Legacy baking avatar z-offset
    LLVector3 offset(0.0, 0.0, llclamp(value,MIN_HOVER_Z,MAX_HOVER_Z));
    gSavedPerAccountSettings.setF32("AvatarHoverOffsetZ",value);
}

// <FS:Zi> FIRE-33859 - Add +/- and reset buttons to hover height slider
bool LLFloaterHoverHeight::handleKey(KEY key, MASK mask, bool called_from_parent)
{
    mModifiers = mask;
    return LLUICtrl::handleKey(key, mask, called_from_parent);
}

bool LLFloaterHoverHeight::handleKeyUp(KEY key, MASK mask, bool called_from_parent)
{
    mModifiers = mask;
    return LLUICtrl::handleKeyUp(key, mask, called_from_parent);
}

void LLFloaterHoverHeight::onResetButtonClicked()
{
    onButtonClicked(0.0f);
}

void LLFloaterHoverHeight::onPlusButtonClicked()
{
    onButtonClicked(0.01f);
}

void LLFloaterHoverHeight::onMinusButtonClicked()
{
    onButtonClicked(-0.01f);
}

void LLFloaterHoverHeight::onButtonClicked(F32 value)
{
    LLSliderCtrl* sldrCtrl = getChild<LLSliderCtrl>("HoverHeightSlider");

    if (value == 0.0f)
    {
        sldrCtrl->setValue(0.0f);
    }
    else
    {
        switch (mModifiers)
        {
            case MASK_ALT:
            {
                value *= 10.0f;
                break;
            }
            case MASK_CONTROL:
            case MASK_SHIFT:
            {
                value *= 0.1f;
                break;
            }
        }
        sldrCtrl->setValue(sldrCtrl->getValueF32() + value);
    }

    onSliderMoved(sldrCtrl, nullptr);
    onFinalCommit();
}
// </FS:Zi>

void LLFloaterHoverHeight::onRegionChanged()
{
    LLViewerRegion *region = gAgent.getRegion();
    if (region && region->simulatorFeaturesReceived())
    {
        updateEditEnabled();
    }
    else if (region)
    {
        region->setSimulatorFeaturesReceivedCallback(boost::bind(&LLFloaterHoverHeight::onSimulatorFeaturesReceived,this,_1));
    }
}

void LLFloaterHoverHeight::onSimulatorFeaturesReceived(const LLUUID &region_id)
{
    LLViewerRegion *region = gAgent.getRegion();
    if (region && (region->getRegionID()==region_id))
    {
        updateEditEnabled();
    }
}

void LLFloaterHoverHeight::updateEditEnabled()
{
    bool enabled = gAgent.getRegion() && gAgent.getRegion()->avatarHoverHeightEnabled();
    // <FS:Ansariel> Legacy baking avatar z-offset
    if (!enabled && isAgentAvatarValid() && !gAgentAvatarp->isUsingServerBakes())
    {
        enabled = true;
    }
    // </FS:Ansariel>
    LLSliderCtrl* sldrCtrl = getChild<LLSliderCtrl>("HoverHeightSlider");
    sldrCtrl->setEnabled(enabled);
    // <FS:Zi> FIRE-33859 - Add +/- and reset buttons to hover height slider
    LLButton* button = getChild<LLButton>("PlusButton");
    button->setEnabled(enabled);
    button = getChild<LLButton>("MinusButton");
    button->setEnabled(enabled);
    button = getChild<LLButton>("ResetButton");
    button->setEnabled(enabled);
    // </FS:Zi>
    if (enabled)
    {
        syncFromPreferenceSetting(this);
    }
}


