/**
 * @file lloutputmonitorctrl.cpp
 * @brief LLOutputMonitorCtrl base class
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
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

#include "llviewerprecompiledheaders.h"
#include "lloutputmonitorctrl.h"

// library includes
#include "llfloaterreg.h"
#include "llui.h"

// viewer includes
#include "llvoiceclient.h"
#include "llmutelist.h"
#include "llagent.h"

#include "llspeakers.h"     // <FS:Zi> Add new control to have a nearby voice output monitor

// default options set in output_monitor.xml
static LLDefaultChildRegistry::Register<LLOutputMonitorCtrl> r("output_monitor");

// The defaults will be initialized in the constructor.
//LLColor4  LLOutputMonitorCtrl::sColorMuted;
//LLColor4  LLOutputMonitorCtrl::sColorOverdriven;
//LLColor4  LLOutputMonitorCtrl::sColorNormal;
LLColor4    LLOutputMonitorCtrl::sColorBound;
//S32           LLOutputMonitorCtrl::sRectsNumber       = 0;
//F32           LLOutputMonitorCtrl::sRectWidthRatio    = 0.f;
//F32           LLOutputMonitorCtrl::sRectHeightRatio   = 0.f;

LLOutputMonitorCtrl::Params::Params()
:   draw_border("draw_border"),
    image_mute("image_mute"),
    image_off("image_off"),
    image_on("image_on"),
    image_level_1("image_level_1"),
    image_level_2("image_level_2"),
    image_level_3("image_level_3"),
    auto_update("auto_update"),
    speaker_id("speaker_id")
{
};

LLOutputMonitorCtrl::LLOutputMonitorCtrl(const LLOutputMonitorCtrl::Params& p)
:   LLView(p),
    mPower(0),
    mImageMute(p.image_mute),
    mImageOff(p.image_off),
    mImageOn(p.image_on),
    mImageLevel1(p.image_level_1),
    mImageLevel2(p.image_level_2),
    mImageLevel3(p.image_level_3),
    mAutoUpdate(p.auto_update),
    mSpeakerId(p.speaker_id),
    mIsModeratorMuted(false),
    mIsAgentControl(false),
    mIndicatorToggled(false),
    mShowParticipantsSpeaking(false),
    mChannelState(INACTIVE_CHANNEL),
    mAlwaysVisible(false) // <FS:Ansariel> Option to not hide it if speaker ID is set to null
{
    //static LLUIColor output_monitor_muted_color = LLUIColorTable::instance().getColor("OutputMonitorMutedColor", LLColor4::orange);
    //static LLUIColor output_monitor_overdriven_color = LLUIColorTable::instance().getColor("OutputMonitorOverdrivenColor", LLColor4::red);
    //static LLUIColor output_monitor_normal_color = LLUIColorTable::instance().getColor("OutputMonitorNotmalColor", LLColor4::green);
    static LLUIColor output_monitor_bound_color = LLUIColorTable::instance().getColor("OutputMonitorBoundColor", LLColor4::white);
    //static LLUICachedControl<S32> output_monitor_rects_number("OutputMonitorRectanglesNumber", 20);
    //static LLUICachedControl<F32> output_monitor_rect_width_ratio("OutputMonitorRectangleWidthRatio", 0.5f);
    //static LLUICachedControl<F32> output_monitor_rect_height_ratio("OutputMonitorRectangleHeightRatio", 0.8f);

    // IAN BUG compare to existing pattern where these are members - some will change per-widget and need to be anyway
    // sent feedback to PE

    // *TODO: it looks suboptimal to load the defaults every time an output monitor is constructed.
    //sColorMuted           = output_monitor_muted_color;
    //sColorOverdriven  = output_monitor_overdriven_color;
    //sColorNormal      = output_monitor_normal_color;
    sColorBound         = output_monitor_bound_color;
    //sRectsNumber      = output_monitor_rects_number;
    //sRectWidthRatio       = output_monitor_rect_width_ratio;
    //sRectHeightRatio  = output_monitor_rect_height_ratio;

    mBorder = p.draw_border;

    //with checking mute state
    setSpeakerId(mSpeakerId);
}

LLOutputMonitorCtrl::~LLOutputMonitorCtrl()
{
    LLMuteList::getInstance()->removeObserver(this);
    LLSpeakingIndicatorManager::unregisterSpeakingIndicator(mSpeakerId, this);
}

void LLOutputMonitorCtrl::setPower(F32 val)
{
    mPower = llmax(0.f, llmin(1.f, val));
}

void LLOutputMonitorCtrl::draw()
{
    // Copied from llmediaremotectrl.cpp
    // *TODO: Give the LLOutputMonitorCtrl an agent-id to monitor, then
    // call directly into LLVoiceClient::getInstance() to ask if that agent-id is muted, is
    // speaking, and what power.  This avoids duplicating data, which can get
    // out of sync.
    // <FS:Ansariel> Centralized voice power level
    //const F32 LEVEL_0 = LLVoiceClient::OVERDRIVEN_POWER_LEVEL / 3.f;
    //const F32 LEVEL_1 = LLVoiceClient::OVERDRIVEN_POWER_LEVEL * 2.f / 3.f;
    //const F32 LEVEL_2 = LLVoiceClient::OVERDRIVEN_POWER_LEVEL;
    // </FS:Ansariel> Centralized voice power level

    LLVoiceClient* vocie_client = LLVoiceClient::getInstance();

    if (getVisible() && mAutoUpdate && !getIsMuted() && mSpeakerId.notNull())
    {
        setPower(vocie_client->getCurrentPower(mSpeakerId));
        if(mIsAgentControl)
        {
            setIsTalking(vocie_client->getUserPTTState());
        }
        else
        {
            setIsTalking(vocie_client->getIsSpeaking(mSpeakerId));
        }
    }

    if ((mPower == 0.f && !mIsTalking) && mShowParticipantsSpeaking)
    {
        std::set<LLUUID> participant_uuids;
        vocie_client->getParticipantList(participant_uuids);
        std::set<LLUUID>::const_iterator part_it = participant_uuids.begin();

        F32 power = 0;
        for (; part_it != participant_uuids.end(); ++part_it)
        {
            power = vocie_client->getCurrentPower(*part_it);
            if (power)
            {
                mPower = power;
                break;
            }
        }
    }

    LLPointer<LLUIImage> icon;
    // <FS:Ansariel> Centralized voice power level
    //if (getIsMuted())
    //{
    //  icon = mImageMute;
    //}
    //else if (mPower == 0.f && !mIsTalking)
    //{
    //  // only show off if PTT is not engaged
    //  icon = mImageOff;
    //}
    //else if (mPower < LEVEL_0)
    //{
    //  // PTT is on, possibly with quiet background noise
    //  icon = mImageOn;
    //}
    //else if (mPower < LEVEL_1)
    //{
    //  icon = mImageLevel1;
    //}
    //else if (mPower < LEVEL_2)
    //{
    //  icon = mImageLevel2;
    //}
    //else
    //{
    //  // overdriven
    //  icon = mImageLevel3;
    //}
    EVoicePowerLevel power_level = LLVoiceClient::getInstance()->getPowerLevel(mSpeakerId);
    switch (power_level)
    {
        case VPL_MUTED:
            icon = mImageMute;
            break;
        case VPL_PTT_Off:
            icon = mImageOff;
            break;
        case VPL_PTT_On:
            icon = mImageOn;
            break;
        case VPL_Level1:
            icon = mImageLevel1;
            break;
        case VPL_Level2:
            icon = mImageLevel2;
            break;
        case VPL_Level3:
            icon = mImageLevel3;
            break;
        default:
            break;
    }
    // </FS:Ansariel> Centralized voice power level

    if (icon)
    {
        icon->draw(0, 0);
    }

    //
    // Fill the monitor with a bunch of small rectangles.
    // The rectangles will be filled with gradient color,
    // beginning with sColorNormal and ending with sColorOverdriven.
    //
    // *TODO: would using a (partially drawn) pixmap instead be faster?
    //
    const int monh      = getRect().getHeight();
    const int monw      = getRect().getWidth();
    //int maxrects      = sRectsNumber;
    //const int period  = llmax(1, monw / maxrects, 0, 0);                    // "1" - min value for the period
    //const int rectw       = llmax(1, llfloor(period * sRectWidthRatio), 0, 0);  // "1" - min value for the rect's width
    //const int recth       = llfloor(monh * sRectHeightRatio);

    //if(period == 1 && rectw == 1) //if we have so small control, then "maxrects = monitor's_width - 2*monitor_border's_width
    //  maxrects = monw-2;

    //const int nrects  = mIsMuted ? maxrects : llfloor(mPower * maxrects); // how many rects to draw?
    //const int rectbtm = (monh - recth) / 2;
    //const int recttop = rectbtm + recth;
    //
    //LLColor4 rect_color;
    //
    //for (int i=1, xpos = 0; i <= nrects; i++)
    //{
    //  // Calculate color to use for the current rectangle.
    //  if (mIsMuted)
    //  {
    //      rect_color = sColorMuted;
    //  }
    //  else
    //  {
    //      F32 frac = (mPower * i/nrects) / LLVoiceClient::OVERDRIVEN_POWER_LEVEL;
    //      // Use overdriven color if the power exceeds overdriven level.
    //      if (frac > 1.0f)
    //          frac = 1.0f;
    //      rect_color = lerp(sColorNormal, sColorOverdriven, frac);
    //  }

    //  // Draw rectangle filled with the color.
    //  gl_rect_2d(xpos, recttop, xpos+rectw, rectbtm, rect_color, true);
    //  xpos += period;
    //}

    //
    // Draw bounding box.
    //
    if(mBorder)
        gl_rect_2d(0, monh, monw, 0, sColorBound, false);
}

// virtual
bool LLOutputMonitorCtrl::handleMouseUp(S32 x, S32 y, MASK mask)
{
    // <FS:Ansariel> Show voice volume instead of an empty floater
    //if (mSpeakerId != gAgentID)
    if (mSpeakerId.notNull() && mSpeakerId != gAgentID && !mShowParticipantsSpeaking)
    // </FS:Ansariel>
    {
        LLFloaterReg::showInstance("floater_voice_volume", LLSD().with("avatar_id", mSpeakerId));
    }
    // <FS:Ansariel> Show voice volume instead of an empty floater
    //else if (mShowParticipantsSpeaking)
    else if (mShowParticipantsSpeaking || mSpeakerId.isNull())
    // </FS:Ansariel>
    {
        LLFloaterReg::showInstance("chat_voice", LLSD());
    }

    return true;
}

void LLOutputMonitorCtrl::setIsActiveChannel(bool val)
{
    setChannelState(val ? ACTIVE_CHANNEL : INACTIVE_CHANNEL);
}

void LLOutputMonitorCtrl::setChannelState(EChannelState state)
{
    mChannelState = state;
    if (state == INACTIVE_CHANNEL)
    {
        // switchIndicator will set it to true when channel becomes active
        setVisible(false);
    }
}

void LLOutputMonitorCtrl::setSpeakerId(const LLUUID& speaker_id, const LLUUID& session_id/* = LLUUID::null*/, bool show_other_participants_speaking /* = false */)
{
    if (speaker_id.isNull() && mSpeakerId.notNull())
    {
        LLSpeakingIndicatorManager::unregisterSpeakingIndicator(mSpeakerId, this);
        // <FS:Ansariel> Option to not hide it if speaker ID is set to null
        if (!mAlwaysVisible)
            switchIndicator(false);
        mSpeakerId = speaker_id;
    }

    if (speaker_id.isNull() || (speaker_id == mSpeakerId))
    {
        return;
    }

    if (mSpeakerId.notNull())
    {
        // Unregister previous registration to avoid crash. EXT-4782.
        LLSpeakingIndicatorManager::unregisterSpeakingIndicator(mSpeakerId, this);
    }

    mShowParticipantsSpeaking = show_other_participants_speaking;
    mSpeakerId = speaker_id;
    LLSpeakingIndicatorManager::registerSpeakingIndicator(mSpeakerId, this, session_id);

    //mute management
    if (mAutoUpdate)
    {
        if (speaker_id == gAgentID)
        {
            mIsMuted = false;
        }
        else
        {
            // check only blocking on voice. EXT-3542
            mIsMuted = LLMuteList::getInstance()->isMuted(mSpeakerId, LLMute::flagVoiceChat);
            LLMuteList::getInstance()->addObserver(this);
        }
    }
}

void LLOutputMonitorCtrl::onChangeDetailed(const LLMute& mute)
{
    if (mute.mID == mSpeakerId)
    {
        // Check only blocking on voice.
        // Logic goes in reverse, if flag is set, action is allowed
        mIsMuted = !(LLMute::flagVoiceChat & mute.mFlags);
    }
}

// virtual
void LLOutputMonitorCtrl::switchIndicator(bool switch_on)
{
    // <FS:Ansariel> [FS communication UI]
    //if ((mChannelState != INACTIVE_CHANNEL) && (getVisible() != (bool)switch_on))
    if (getVisible() != switch_on)
    // </FS:Ansariel> [FS communication UI]
    {
        setVisible(switch_on);

        //Let parent adjust positioning of icons adjacent to speaker indicator
        //(when speaker indicator hidden, adjacent icons move to right and when speaker
        //indicator visible, adjacent icons move to the left)
        if (getParent() && getParent()->isInVisibleChain())
        {
            notifyParentVisibilityChanged();
            //Ignore toggled state in case it was set when parent visibility was hidden
            mIndicatorToggled = false;
        }
        else
        {
            //Makes sure to only adjust adjacent icons when parent becomes visible
            //(!mIndicatorToggled ensures that changes of TFT and FTF are discarded, real state changes are TF or FT)
            mIndicatorToggled = !mIndicatorToggled;
        }

    }
}

//////////////////////////////////////////////////////////////////////////
// PRIVATE SECTION
//////////////////////////////////////////////////////////////////////////
void LLOutputMonitorCtrl::notifyParentVisibilityChanged()
{
    LL_DEBUGS("SpeakingIndicator") << "Notify parent that visibility was changed: " << mSpeakerId << ", new_visibility: " << getVisible() << LL_ENDL;

    LLSD params = LLSD().with("visibility_changed", getVisible());

    notifyParent(params);
}

//////////////////////////////////////////////////////////////////////////
// <FS:Zi> Add new control to have a nearby voice output monitor

// default options set in output_monitor.xml
static LLDefaultChildRegistry::Register<NearbyVoiceMonitor> r2("nearby_voice_monitor");

NearbyVoiceMonitor::Params::Params()
:   auto_hide("auto_hide", false)
{
}

NearbyVoiceMonitor::NearbyVoiceMonitor(const NearbyVoiceMonitor::Params& p)
:   LLOutputMonitorCtrl(p),
    mAutoHide(p.auto_hide)
{
    mAlwaysVisible = true;
    mSpeakerMgr = LLLocalSpeakerMgr::getInstance();
}

void NearbyVoiceMonitor::draw()
{
    LLSpeakerMgr::speaker_list_t speaker_list;
    LLUUID id;
    bool draw = false;

    mSpeakerMgr->update(true);
    mSpeakerMgr->getSpeakerList(&speaker_list, false);

    for (LLSpeakerMgr::speaker_list_t::const_iterator it = speaker_list.begin(); it != speaker_list.end(); ++it)
    {
        LLPointer<LLSpeaker> speaker = *it;
        if (speaker->mSpeechVolume > 0.f || speaker->mStatus == LLSpeaker::STATUS_SPEAKING)
        {
            draw = true;
            id = speaker->mID;
            break;
        }
    }

    setSpeakerId(id);

    if (!mAutoHide || draw)
    {
        LLOutputMonitorCtrl::draw();
    }
}
// </FS:Zi> Add new control to have a nearby voice output monitor
//////////////////////////////////////////////////////////////////////////

// EOF
