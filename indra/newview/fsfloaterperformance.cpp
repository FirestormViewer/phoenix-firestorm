/**
 * @file fsfloaterperformance.cpp
 *
 * $LicenseInfo:firstyear=2021&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2021, Linden Research, Inc.
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
#include "fsfloaterperformance.h"

#include "llagent.h"
#include "llagentcamera.h"
#include "llappearancemgr.h"
#include "llavataractions.h"
#include "llavatarrendernotifier.h"
#include "llcheckboxctrl.h"
#include "llfeaturemanager.h"
#include "llfloaterpreference.h" // LLAvatarComplexityControls
#include "llfloaterreg.h"
#include "llmoveview.h" // for LLPanelStandStopFlying
#include "llnamelistctrl.h"
#include "llradiogroup.h"
#include "llselectmgr.h"
#include "llsliderctrl.h"
#include "lltextbox.h"
#include "llcombobox.h"
#include "lltrans.h"
#include "llviewerobjectlist.h"
#include "llviewerjoystick.h"
#include "llviewermediafocus.h"
#include "llvoavatar.h"
#include "llvoavatarself.h"
#include "llperfstats.h"
#include "llworld.h"
#include "pipeline.h"
#include "llviewercontrol.h"
#include "fsavatarrenderpersistence.h"
#include "llpresetsmanager.h"
#include "llwindow.h"
#include "fslslbridge.h"
#include "llbutton.h"

extern F32 gSavedDrawDistance;

constexpr F32 REFRESH_INTERVAL = 1.0f;
constexpr S32 BAR_LEFT_PAD = 2;
constexpr S32 BAR_RIGHT_PAD = 5;
constexpr S32 BAR_BOTTOM_PAD = 9;

constexpr auto AvType       {LLPerfStats::ObjType_t::OT_AVATAR};
constexpr auto SceneType    {LLPerfStats::ObjType_t::OT_GENERAL};
class FSExceptionsContextMenu : public LLListContextMenu
{
public:
    explicit FSExceptionsContextMenu(FSFloaterPerformance* floater_settings)
        :   mFloaterPerformance(floater_settings)
    {}
protected:
    LLContextMenu* createMenu()
    {
        LLUICtrl::CommitCallbackRegistry::ScopedRegistrar registrar;
        LLUICtrl::EnableCallbackRegistry::ScopedRegistrar enable_registrar;
        registrar.add("Settings.SetRendering", boost::bind(&FSFloaterPerformance::onCustomAction, mFloaterPerformance, _2, mUUIDs.front()));
        enable_registrar.add("Settings.IsSelected", boost::bind(&FSFloaterPerformance::isActionChecked, mFloaterPerformance, _2, mUUIDs.front()));
        registrar.add("Avatar.Extended", boost::bind(&FSFloaterPerformance::onExtendedAction, mFloaterPerformance, _2, mUUIDs.front()));
        LLContextMenu* menu = createFromFile("menu_perf_avatar_rendering_settings.xml");

        return menu;
    }

    FSFloaterPerformance* mFloaterPerformance;
};



FSFloaterPerformance::FSFloaterPerformance(const LLSD& key)
:   LLFloater(key),
    mUpdateTimer(new LLTimer())
{
    mContextMenu = new FSExceptionsContextMenu(this);
}

FSFloaterPerformance::~FSFloaterPerformance()
{
    mComplexityChangedSignal.disconnect();
    delete mContextMenu;
    delete mUpdateTimer;
}

bool FSFloaterPerformance::postBuild()
{
    mMainPanel = getChild<LLPanel>("panel_performance_main");
    mNearbyPanel = getChild<LLPanel>("panel_performance_nearby");
    mComplexityPanel = getChild<LLPanel>("panel_performance_complexity");
    mSettingsPanel = getChild<LLPanel>("panel_performance_preferences");
    mHUDsPanel = getChild<LLPanel>("panel_performance_huds");
    mAutoTunePanel = getChild<LLPanel>("panel_performance_autotune");

    getChild<LLPanel>("nearby_subpanel")->setMouseDownCallback(boost::bind(&FSFloaterPerformance::showSelectedPanel, this, mNearbyPanel));
    getChild<LLPanel>("complexity_subpanel")->setMouseDownCallback(boost::bind(&FSFloaterPerformance::showSelectedPanel, this, mComplexityPanel));
    getChild<LLPanel>("settings_subpanel")->setMouseDownCallback(boost::bind(&FSFloaterPerformance::showSelectedPanel, this, mSettingsPanel));
    getChild<LLPanel>("huds_subpanel")->setMouseDownCallback(boost::bind(&FSFloaterPerformance::showSelectedPanel, this, mHUDsPanel));

    auto tgt_panel = findChild<LLPanel>("target_subpanel");
    if (tgt_panel)
    {
        tgt_panel->getChild<LLButton>("target_btn")->setCommitCallback(boost::bind(&FSFloaterPerformance::showSelectedPanel, this, mAutoTunePanel));
        tgt_panel->getChild<LLComboBox>("FSTuningFPSStrategy")->setCurrentByIndex(gSavedSettings.getU32("TuningFPSStrategy"));
        tgt_panel->getChild<LLButton>("PrefSaveButton")->setCommitCallback(boost::bind(&FSFloaterPerformance::savePreset, this));
        tgt_panel->getChild<LLButton>("PrefLoadButton")->setCommitCallback(boost::bind(&FSFloaterPerformance::loadPreset, this));
        tgt_panel->getChild<LLButton>("Defaults")->setCommitCallback(boost::bind(&FSFloaterPerformance::setHardwareDefaults, this));
    }

    initBackBtn(mNearbyPanel);
    initBackBtn(mComplexityPanel);
    initBackBtn(mSettingsPanel);
    initBackBtn(mHUDsPanel);
    initBackBtn(mAutoTunePanel);

    mHUDList = mHUDsPanel->getChild<LLNameListCtrl>("hud_list");
    mHUDList->setNameListType(LLNameListCtrl::SPECIAL);
    mHUDList->setHoverIconName("StopReload_Off");
    mHUDList->setIconClickedCallback(boost::bind(&FSFloaterPerformance::detachItem, this, _1));

    mObjectList = mComplexityPanel->getChild<LLNameListCtrl>("obj_list");
    mObjectList->setNameListType(LLNameListCtrl::SPECIAL);
    mObjectList->setHoverIconName("StopReload_Off");
    mObjectList->setIconClickedCallback(boost::bind(&FSFloaterPerformance::detachItem, this, _1));

    mSettingsPanel->getChild<LLSliderCtrl>("quality_vs_perf_selection")->setCommitCallback(boost::bind(&FSFloaterPerformance::onChangeQuality, this, _2));

    mNearbyPanel->getChild<LLButton>("exceptions_btn")->setCommitCallback(boost::bind(&FSFloaterPerformance::onClickExceptions, this));
    mNearbyPanel->getChild<LLCheckBoxCtrl>("hide_avatars")->setCommitCallback(boost::bind(&FSFloaterPerformance::onClickHideAvatars, this));
    mNearbyPanel->getChild<LLCheckBoxCtrl>("hide_avatars")->set(!LLPipeline::hasRenderTypeControl(LLPipeline::RENDER_TYPE_AVATAR));
    mNearbyList = mNearbyPanel->getChild<LLNameListCtrl>("nearby_list");
    mNearbyList->setRightMouseDownCallback(boost::bind(&FSFloaterPerformance::onAvatarListRightClick, this, _1, _2, _3));


    updateComplexityText();
    mComplexityChangedSignal = gSavedSettings.getControl("RenderAvatarMaxComplexity")->getCommitSignal()->connect(boost::bind(&FSFloaterPerformance::updateComplexityText, this));
    mNearbyPanel->getChild<LLSliderCtrl>("IndirectMaxComplexity")->setCommitCallback(boost::bind(&FSFloaterPerformance::updateMaxComplexity, this));

    mMaxARTChangedSignal = gSavedSettings.getControl("RenderAvatarMaxART")->getCommitSignal()->connect(boost::bind(&FSFloaterPerformance::updateMaxRenderTime, this));
    mNearbyPanel->getChild<LLSliderCtrl>("RenderAvatarMaxART")->setCommitCallback(boost::bind(&FSFloaterPerformance::updateMaxRenderTime, this));

    LLAvatarComplexityControls::setIndirectMaxArc();
    // store the current setting as the users desired reflection detail and DD
    if(!LLPerfStats::tunables.userAutoTuneEnabled)
    {
        if (gSavedDrawDistance)
        {
            gSavedSettings.setF32("AutoTuneRenderFarClipTarget", gSavedDrawDistance);
        }
        else
        {
            gSavedSettings.setF32("AutoTuneRenderFarClipTarget", LLPipeline::RenderFarClip);
        }
    }

    return true;
}

void FSFloaterPerformance::resetMaxArtSlider()
{
    LLPerfStats::renderAvatarMaxART_ns = 0;
    LLPerfStats::tunables.updateSettingsFromRenderCostLimit();
    LLPerfStats::tunables.applyUpdates();
    updateMaxRenderTime();
}

void FSFloaterPerformance::savePreset()
{
    LLFloaterReg::showInstance("save_pref_preset", "graphic" );
}

void FSFloaterPerformance::loadPreset()
{
    LLFloaterReg::showInstance("load_pref_preset", "graphic");
    resetMaxArtSlider();
}

void FSFloaterPerformance::setHardwareDefaults()
{
    LLFeatureManager::getInstance()->applyRecommendedSettings();
    // reset indirects before refresh because we may have changed what they control
    LLAvatarComplexityControls::setIndirectControls();
    gSavedSettings.setString("PresetGraphicActive", "");
    LLPresetsManager::getInstance()->triggerChangeSignal();
    resetMaxArtSlider();
}


void FSFloaterPerformance::showSelectedPanel(LLPanel* selected_panel)
{
    hidePanels();
    mMainPanel->setVisible(false);
    selected_panel->setVisible(true);

    if (mHUDsPanel == selected_panel)
    {
        populateHUDList();
    }
    else if (mNearbyPanel == selected_panel)
    {
        populateNearbyList();
    }
    else if (mComplexityPanel == selected_panel)
    {
        populateObjectList();
    }
}

void FSFloaterPerformance::draw()
{
    const S32 NUM_PERIODS = 50;
    constexpr auto NANOS = 1000000000;

    static LLCachedControl<U32> fpsCap(gSavedSettings, "FramePerSecondLimit"); // user limited FPS
    static LLCachedControl<U32> targetFPS(gSavedSettings, "TargetFPS"); // desired FPS
    static LLCachedControl<U32> tuningStrategy(gSavedSettings, "TuningFPSStrategy");
    static LLCachedControl<bool> vsyncEnabled(gSavedSettings, "RenderVSyncEnable");


    if (mUpdateTimer->hasExpired())
    {
        LLStringUtil::format_map_t args;

        auto fps = LLTrace::get_frame_recording().getPeriodMedianPerSec(LLStatViewer::FPS, NUM_PERIODS);
        getChild<LLTextBox>("fps_value")->setValue((S32)llround(fps));

        auto target_frame_time_ns = NANOS/(targetFPS==0?1:targetFPS);

        LLPerfStats::bufferToggleLock.lock(); // prevent toggle for a moment


        auto tot_frame_time_raw = LLPerfStats::StatsRecorder::getSceneStat(LLPerfStats::StatType_t::RENDER_FRAME);
        // cumulative avatar time (includes idle processing, attachments and base av)
        auto tot_avatar_time_raw = LLPerfStats::StatsRecorder::getSum(AvType, LLPerfStats::StatType_t::RENDER_COMBINED);
        // cumulative avatar render specific time (a bit arbitrary as the processing is too.)
        // auto tot_av_idle_time_raw = LLPerfStats::StatsRecorder::getSum(AvType, LLPerfStats::StatType_t::RENDER_IDLE);
        // auto tot_avatar_render_time_raw = tot_avatar_time_raw - tot_av_idle_time_raw;
        // the time spent this frame on the "display()" call. Treated as "tot time rendering"
        auto tot_render_time_raw = LLPerfStats::StatsRecorder::getSceneStat(LLPerfStats::StatType_t::RENDER_DISPLAY);
        // sleep time is basically forced sleep when window out of focus
        auto tot_sleep_time_raw = LLPerfStats::StatsRecorder::getSceneStat(LLPerfStats::StatType_t::RENDER_SLEEP);
        // time spent on UI
        auto tot_ui_time_raw = LLPerfStats::StatsRecorder::getSceneStat(LLPerfStats::StatType_t::RENDER_UI);
        // cumulative time spent rendering HUDS
        auto tot_huds_time_raw = LLPerfStats::StatsRecorder::getSceneStat(LLPerfStats::StatType_t::RENDER_HUDS);
        // "idle" time. This is the time spent in the idle poll section of the main loop
        auto tot_idle_time_raw = LLPerfStats::StatsRecorder::getSceneStat(LLPerfStats::StatType_t::RENDER_IDLE);
        // similar to sleep time, induced by FPS limit
        auto tot_limit_time_raw = LLPerfStats::StatsRecorder::getSceneStat(LLPerfStats::StatType_t::RENDER_FPSLIMIT);
        // swap time is time spent in swap buffer
        auto tot_swap_time_raw = LLPerfStats::StatsRecorder::getSceneStat(LLPerfStats::StatType_t::RENDER_SWAP);

        LLPerfStats::bufferToggleLock.unlock();

        auto unreliable = false; // if there is something to skew the stats such as sleep of fps cap
        auto tot_frame_time_ns = LLPerfStats::raw_to_ns(tot_frame_time_raw);
        auto tot_avatar_time_ns         = LLPerfStats::raw_to_ns( tot_avatar_time_raw );
        auto tot_huds_time_ns           = LLPerfStats::raw_to_ns( tot_huds_time_raw );
        // UI time includes HUD time so dedut that before we calc percentages
        auto tot_ui_time_ns             = LLPerfStats::raw_to_ns( tot_ui_time_raw - tot_huds_time_raw);

        // auto tot_sleep_time_ns          = LLPerfStats::raw_to_ns( tot_sleep_time_raw );
        // auto tot_limit_time_ns          = LLPerfStats::raw_to_ns( tot_limit_time_raw );

        // auto tot_render_time_ns         = LLPerfStats::raw_to_ns( tot_render_time_raw );
        auto tot_idle_time_ns   = LLPerfStats::raw_to_ns( tot_idle_time_raw );
        auto tot_swap_time_ns   = LLPerfStats::raw_to_ns( tot_swap_time_raw );
        auto tot_scene_time_ns  = LLPerfStats::raw_to_ns( tot_render_time_raw - tot_avatar_time_raw - tot_swap_time_raw - tot_ui_time_raw);
        // auto tot_overhead_time_ns  = LLPerfStats::raw_to_ns( tot_frame_time_raw - tot_render_time_raw - tot_idle_time_raw );

        // // remove time spent sleeping for fps limit or out of focus.
        // tot_frame_time_ns -= tot_limit_time_ns;
        // tot_frame_time_ns -= tot_sleep_time_ns;

        if(tot_frame_time_ns != 0)
        {
            auto pct_avatar_time = (tot_avatar_time_ns * 100)/tot_frame_time_ns;
            auto pct_huds_time = (tot_huds_time_ns * 100)/tot_frame_time_ns;
            auto pct_ui_time = (tot_ui_time_ns * 100)/tot_frame_time_ns;
            auto pct_idle_time = (tot_idle_time_ns * 100)/tot_frame_time_ns;
            auto pct_swap_time = (tot_swap_time_ns * 100)/tot_frame_time_ns;
            auto pct_scene_render_time = (tot_scene_time_ns * 100)/tot_frame_time_ns;
            pct_avatar_time = llclamp(pct_avatar_time,0.,100.);
            pct_huds_time = llclamp(pct_huds_time,0.,100.);
            pct_ui_time = llclamp(pct_ui_time,0.,100.);
            pct_idle_time = llclamp(pct_idle_time,0.,100.);
            pct_swap_time = llclamp(pct_swap_time,0.,100.);
            pct_scene_render_time = llclamp(pct_scene_render_time,0.,100.);

// update the top line status - also check whether we're sleeping/limiting

            args["TOT_FRAME_TIME"] = llformat("%02u", (U32)llround(tot_frame_time_ns/1000000));
            args["FPSCAP"] = llformat("%02u", (U32)fpsCap);
            args["FPSTARGET"] = llformat("%02u", (U32)targetFPS);
            S32 refresh_rate = gViewerWindow->getWindow()->getRefreshRate();
            args["VSYNCFREQ"] = llformat("%03d", (U32)refresh_rate);
            auto textbox = getChild<LLTextBox>("fps_warning");
            // Note: the ordering of these is important.
            // 1) background_yield should override others
            // 2) viewer fps limits take place irrespective of vsync and so should come first.
            // 3) vsync last.
            if (tot_sleep_time_raw > 0) // We are sleeping because view is not focussed
            {
                textbox->setVisible(true);
                textbox->setText(getString("focus_fps"));
                textbox->setColor(LLUIColorTable::instance().getColor("DrYellow"));
                unreliable = true;
            }
            else if (tot_limit_time_raw > 0)
            {
                textbox->setVisible(true);
                textbox->setText(getString("limit_fps", args));
                textbox->setColor(LLUIColorTable::instance().getColor("DrYellow"));
                unreliable = true;
            }
            else if (vsyncEnabled)
            {
                textbox->setVisible(true);
                textbox->setText(getString("max_fps", args));
                // TODO(Beq) : When FPS is more than the frequency we can notify the user.
                // When the FPS is lower than the frequency and also lower than the core stats then VSync might be the constraint
                // we can notify this too. For now just display the frequency until we are sure that refresh rate is detected properly.
                textbox->setColor(LLUIColorTable::instance().getColor("green"));
            }
            else if (LLPerfStats::tunables.userAutoTuneEnabled)
            {
                textbox->setVisible(true);
                textbox->setText(getString("tuning_fps", args));
                textbox->setColor(LLUIColorTable::instance().getColor("green"));
            }
            else
            {
                textbox->setVisible(false);
            }
// pre-fill the args
            if(!unreliable)
            {
                args["AV_FRAME_PCT"] = llformat("%02u", (U32)llround(pct_avatar_time));
                args["HUDS_FRAME_PCT"] = llformat("%02u", (U32)llround(pct_huds_time));
                args["UI_FRAME_PCT"] = llformat("%02u", (U32)llround(pct_ui_time));
                args["IDLE_FRAME_PCT"] = llformat("%02u", (U32)llround(pct_idle_time));
                args["SWAP_FRAME_PCT"] = llformat("%02u", (U32)llround(pct_swap_time));
                args["SCENERY_FRAME_PCT"] = llformat("%02u", (U32)llround(pct_scene_render_time));
            }
            else
            {
                args["AV_FRAME_PCT"] = "--";
                args["HUDS_FRAME_PCT"] = "--";
                args["UI_FRAME_PCT"] = "--";
                args["IDLE_FRAME_PCT"] = "--";
                args["SWAP_FRAME_PCT"] = "--";
                args["SCENERY_FRAME_PCT"] = "--";
            }

            getChild<LLTextBox>("av_frame_stats")->setText(getString("av_frame_pct", args));
            getChild<LLTextBox>("huds_frame_stats")->setText(getString("huds_frame_pct", args));
            getChild<LLTextBox>("frame_breakdown")->setText(getString("frame_stats", args));

            auto button = getChild<LLButton>("AutoTuneFPS");
            if (button->getToggleState() != LLPerfStats::tunables.userAutoTuneEnabled)
            {
                button->toggleState();
            }
            if (LLPerfStats::tunables.userAutoTuneEnabled && !unreliable )
            {
                // the tuning itself is managed from another thread but we can report progress here

                // Is our target frame time lower than current? If so we need to take action to reduce draw overheads.
                if (target_frame_time_ns <= tot_frame_time_ns)
                {
                    U32 non_avatar_time_ns = (U32)(tot_frame_time_ns - tot_avatar_time_ns);
                    // If the target frame time < non avatar frame time then we can pototentially reach it.
                    if (non_avatar_time_ns < (U32)target_frame_time_ns)
                    {
                        textbox->setColor(LLUIColorTable::instance().getColor("orange"));
                    }
                    else
                    {
                        // TODO(Beq): Set advisory text for further actions
                        textbox->setColor(LLUIColorTable::instance().getColor("red"));
                    }
                }
                else if (target_frame_time_ns > (tot_frame_time_ns + LLPerfStats::renderAvatarMaxART_ns))
                {
                    // if we have more time to spare. Display this (the service will update things)
                    textbox->setColor(LLUIColorTable::instance().getColor("green"));
                }
            }

            if (mHUDsPanel->getVisible())
            {
                populateHUDList();
            }
            else if (mNearbyPanel->getVisible())
            {
                populateNearbyList();
                mNearbyPanel->getChild<LLCheckBoxCtrl>("hide_avatars")->set(!LLPipeline::hasRenderTypeControl(LLPipeline::RENDER_TYPE_AVATAR));
            }
            else if (mComplexityPanel->getVisible())
            {
                populateObjectList();
            }
        }
        else
        {
            LL_WARNS("performance") << "Scene time 0. Skipping til we have data." << LL_ENDL;
        }

        mUpdateTimer->setTimerExpirySec(REFRESH_INTERVAL);
    }
    LLFloater::draw();
}

void FSFloaterPerformance::showMainPanel()
{
    hidePanels();
    mMainPanel->setVisible(true);
}

void FSFloaterPerformance::hidePanels()
{
    mNearbyPanel->setVisible(false);
    mComplexityPanel->setVisible(false);
    mHUDsPanel->setVisible(false);
    mSettingsPanel->setVisible(false);
    mAutoTunePanel->setVisible(false);
}

void FSFloaterPerformance::initBackBtn(LLPanel* panel)
{
    panel->getChild<LLButton>("back_btn")->setCommitCallback(boost::bind(&FSFloaterPerformance::showMainPanel, this));
    panel->getChild<LLTextBox>("back_lbl")->setShowCursorHand(false);
    panel->getChild<LLTextBox>("back_lbl")->setSoundFlags(LLView::MOUSE_UP);
    panel->getChild<LLTextBox>("back_lbl")->setClickedCallback(boost::bind(&FSFloaterPerformance::showMainPanel, this));
}

void FSFloaterPerformance::populateHUDList()
{
    S32 prev_pos = mHUDList->getScrollPos();
    LLUUID prev_selected_id = mHUDList->getSelectedSpecialId();
    mHUDList->clearRows();
    mHUDList->updateColumns(true);

    LLVOAvatar* avatar = gAgentAvatarp;

    gPipeline.profileAvatar(avatar, true);

    LLVOAvatar::attachment_map_t::iterator iter;
    LLVOAvatar::attachment_map_t::iterator begin = avatar->mAttachmentPoints.begin();
    LLVOAvatar::attachment_map_t::iterator end = avatar->mAttachmentPoints.end();

    // get max gpu render time of all attachments
    F32 max_gpu_time = -1.f;

    for (iter = begin; iter != end; ++iter)
    {
        LLViewerJointAttachment* attachment = iter->second;
        for (LLViewerJointAttachment::attachedobjs_vec_t::iterator attachment_iter = attachment->mAttachedObjects.begin();
            attachment_iter != attachment->mAttachedObjects.end();
            ++attachment_iter)
        {
            LLViewerObject* attached_object = attachment_iter->get();
            if (attached_object && attached_object->isHUDAttachment())
            {
                if (attached_object->getAttachmentItemName() == FSLSLBridge::instance().currentFullName())
                {
                    continue;
                }

                max_gpu_time = llmax(max_gpu_time, attached_object->mGPURenderTime);
            }
        }
    }


    for (iter = begin; iter != end; ++iter)
    {
        if (!iter->second)
        {
            continue;
        }

        LLViewerJointAttachment* attachment = iter->second;
        for (LLViewerJointAttachment::attachedobjs_vec_t::iterator attachment_iter = attachment->mAttachedObjects.begin();
            attachment_iter != attachment->mAttachedObjects.end();
            ++attachment_iter)
        {
            LLViewerObject* attached_object = attachment_iter->get();
            if (attached_object && attached_object->isHUDAttachment())
            {
                if (attached_object->getAttachmentItemName() == FSLSLBridge::instance().currentFullName())
                {
                    continue;
                }

                F32 gpu_time = attached_object->mGPURenderTime;

                LLSD item;
                item["special_id"] = attached_object->getID();
                item["target"] = LLNameListCtrl::SPECIAL;
                LLSD& row = item["columns"];
                row[0]["column"] = "art_visual";
                row[0]["type"] = "bar";
                LLSD& value = row[0]["value"];
                value["ratio"] = gpu_time / max_gpu_time;
                value["bottom"] = BAR_BOTTOM_PAD;
                value["left_pad"] = BAR_LEFT_PAD;
                value["right_pad"] = BAR_RIGHT_PAD;

                row[1]["column"] = "art_value";
                row[1]["type"] = "text";
                // show gpu time in us
                row[1]["value"] = llformat("%.f", gpu_time * 1000.f);
                row[1]["font"]["name"] = "SANSSERIF";

                row[2]["column"] = "name";
                row[2]["type"] = "text";
                row[2]["value"] = attached_object->getAttachmentItemName();
                row[2]["font"]["name"] = "SANSSERIF";

                LLScrollListItem* obj = mHUDList->addElement(item);
                if (obj)
                {
                    // ART value
                    LLScrollListText* value_text = dynamic_cast<LLScrollListText*>(obj->getColumn(1));
                    if (value_text)
                    {
                        value_text->setAlignment(LLFontGL::RIGHT);
                    }

                    // name
                    value_text = dynamic_cast<LLScrollListText*>(obj->getColumn(3));
                    if (value_text)
                    {
                        value_text->setAlignment(LLFontGL::LEFT);
                    }
                }
            }
        }
    }

    mHUDList->sortByColumnIndex(1, false);
    mHUDList->setScrollPos(prev_pos);
    mHUDList->selectItemBySpecialId(prev_selected_id);
}

void FSFloaterPerformance::populateObjectList()
{
    S32 prev_pos = mObjectList->getScrollPos();
    auto prev_selected_id = mObjectList->getSelectedSpecialId();

    std::string current_sort_col = mObjectList->getSortColumnName();
    bool current_sort_asc = mObjectList->getSortAscending();

    mObjectList->clearRows();
    mObjectList->updateColumns(true);

    object_complexity_list_t attachment_complexity_list = LLAvatarRenderNotifier::getInstance()->getObjectComplexityList();

    LLVOAvatar* avatar = gAgentAvatarp;

    gPipeline.profileAvatar(avatar, true);

    LLVOAvatar::attachment_map_t::iterator iter;
    LLVOAvatar::attachment_map_t::iterator begin = avatar->mAttachmentPoints.begin();
    LLVOAvatar::attachment_map_t::iterator end = avatar->mAttachmentPoints.end();

    // get max gpu render time of all attachments
    F32 max_gpu_time = -1.f;
    F32 total_gpu_time = 0.f;

    for (iter = begin; iter != end; ++iter)
    {
        LLViewerJointAttachment* attachment = iter->second;
        for (LLViewerJointAttachment::attachedobjs_vec_t::iterator attachment_iter = attachment->mAttachedObjects.begin();
            attachment_iter != attachment->mAttachedObjects.end();
            ++attachment_iter)
        {
            LLViewerObject* attached_object = attachment_iter->get();
            if (attached_object && !attached_object->isHUDAttachment())
            {
                max_gpu_time = llmax(max_gpu_time, attached_object->mGPURenderTime);
                total_gpu_time += attached_object->mGPURenderTime;
            }
        }
    }

    {
        for (iter = begin; iter != end; ++iter)
        {
            if (!iter->second)
            {
                continue;
            }

            LLViewerJointAttachment* attachment = iter->second;
            for (LLViewerJointAttachment::attachedobjs_vec_t::iterator attachment_iter = attachment->mAttachedObjects.begin();
                attachment_iter != attachment->mAttachedObjects.end();
                ++attachment_iter)
            {
                LLViewerObject* attached_object = attachment_iter->get();
                if (attached_object && !attached_object->isHUDAttachment())
                {
                    F32 gpu_time = attached_object->mGPURenderTime;

                    auto complex_it = std::find_if(attachment_complexity_list.begin(), attachment_complexity_list.end(), [attached_object](const auto& item) { return item.objectId == attached_object->getAttachmentItemID(); });
                    S32 obj_cost_short = complex_it != attachment_complexity_list.end() ? llmax((S32)(*complex_it).objectCost / 1000, 1) : 0;

                    LLSD item;
                    item["special_id"] = attached_object->getID();
                    item["target"] = LLNameListCtrl::SPECIAL;
                    LLSD& row = item["columns"];
                    row[0]["column"] = "art_visual";
                    row[0]["type"] = "bar";
                    LLSD& value = row[0]["value"];
                    value["ratio"] = gpu_time / max_gpu_time;
                    value["bottom"] = BAR_BOTTOM_PAD;
                    value["left_pad"] = BAR_LEFT_PAD;
                    value["right_pad"] = BAR_RIGHT_PAD;

                    row[1]["column"] = "art_value";
                    row[1]["type"] = "text";
                    // row[1]["value"] = std::to_string(obj_cost_short);
                    row[1]["value"] = llformat("%.2f", gpu_time * 1000.f);
                    row[1]["font"]["name"] = "SANSSERIF";

                    row[2]["column"] = "complex_value";
                    row[2]["type"] = "text";
                    row[2]["value"] = std::to_string(obj_cost_short);
                    row[2]["font"]["name"] = "SANSSERIF";

                    row[3]["column"] = "name";
                    row[3]["type"] = "text";
                    row[3]["value"] = attached_object->getAttachmentItemName();
                    row[3]["font"]["name"] = "SANSSERIF";

                    LLScrollListItem* obj = mObjectList->addElement(item);
                    if (obj)
                    {
                        // ART value
                        LLScrollListText* value_text = dynamic_cast<LLScrollListText*>(obj->getColumn(1));
                        if (value_text)
                        {
                            value_text->setAlignment(LLFontGL::RIGHT);
                        }
                        // ARC value
                        value_text = dynamic_cast<LLScrollListText*>(obj->getColumn(2));
                        if (value_text)
                        {
                            value_text->setAlignment(LLFontGL::RIGHT);
                        }
                    }
                }
            }

            auto textbox = getChild<LLTextBox>("tot_att_count");
            LLStringUtil::format_map_t args;
            args["TOT_ATT"] = llformat("%d", (int64_t)attachment_complexity_list.size());
            args["TOT_ATT_TIME"] = llformat("%.2f", total_gpu_time * 1000.f);
            textbox->setText(getString("tot_att_template", args));
        }
    }

    LL_DEBUGS("PerfFloater") << "Attachments for frame : " << gFrameCount << " COMPLETED" << LL_ENDL;
    mNearbyList->sortByColumn(current_sort_col, current_sort_asc);
    mObjectList->setScrollPos(prev_pos);
    mObjectList->selectItemBySpecialId(prev_selected_id);
}

void FSFloaterPerformance::populateNearbyList()
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_APP;
    static LLCachedControl<bool> showTunedART(gSavedSettings, "ShowTunedART");
    S32 prev_pos = mNearbyList->getScrollPos();
    LLUUID prev_selected_id = mNearbyList->getStringUUIDSelectedItem();
    std::string current_sort_col = mNearbyList->getSortColumnName();
    bool current_sort_asc = mNearbyList->getSortAscending();

    if (current_sort_col == "art_visual")
    {
        current_sort_col = "art_value";
        current_sort_asc = false;
    }

    mNearbyList->clearRows();
    mNearbyList->updateColumns(true);

    std::vector<LLVOAvatar*> valid_nearby_avs;
    mNearbyMaxGPUTime = LLWorld::getInstance()->getNearbyAvatarsAndMaxGPUTime(valid_nearby_avs);

    auto char_iter = valid_nearby_avs.begin();

    LLPerfStats::bufferToggleLock.lock();
    auto av_render_tot_raw = LLPerfStats::StatsRecorder::getSum(AvType, LLPerfStats::StatType_t::RENDER_COMBINED);
    LLPerfStats::bufferToggleLock.unlock();

    while (char_iter != valid_nearby_avs.end())
    {
        LLVOAvatar* avatar = *char_iter;
        if (avatar)
        {
            auto overall_appearance = avatar->getOverallAppearance();
            if (overall_appearance == LLVOAvatar::AOA_INVISIBLE)
            {
                char_iter++;
                continue;
            }

            S32 complexity_short = llmax((S32)avatar->getVisualComplexity() / 1000, 1);

            F32 render_av_gpu_ms = avatar->getGPURenderTime();
            LLPerfStats::bufferToggleLock.lock();
            auto render_av_geom  = LLPerfStats::StatsRecorder::get(AvType, avatar->getID(),LLPerfStats::StatType_t::RENDER_GEOMETRY);
            auto render_av_shadow  = LLPerfStats::StatsRecorder::get(AvType, avatar->getID(),LLPerfStats::StatType_t::RENDER_SHADOWS);
            auto render_av_idle  = LLPerfStats::StatsRecorder::get(AvType, avatar->getID(),LLPerfStats::StatType_t::RENDER_IDLE);
            LLPerfStats::bufferToggleLock.unlock();

            auto is_slow = avatar->isTooSlow();

            LLSD item;
            item["id"] = avatar->getID();
            LLSD& row = item["columns"];
            int colno = 0;
            row[colno]["column"] = "art_visual";
            row[colno]["type"] = "bar";
            LLSD& value = row[colno]["value"];
            // The ratio used in the bar is the current cost, as soon as we take action this changes so we keep the
            // pre-tune value for the numerical column and sorting.
            value["ratio"] = render_av_gpu_ms / mNearbyMaxGPUTime;
            value["bottom"] = BAR_BOTTOM_PAD;
            value["left_pad"] = BAR_LEFT_PAD;
            value["right_pad"] = BAR_RIGHT_PAD;
            colno++;

            row[colno]["column"] = "art_value";
            row[colno]["type"] = "text";
            row[colno]["value"] = llformat( "%.2f", render_av_gpu_ms * 1000.f);
            row[colno]["font"]["name"] = "SANSSERIF";
            row[colno]["width"] = "50";
            colno++;

            if (showTunedART)
            {
                row[colno]["column"] = "adj_art_value";
                row[colno]["type"] = "text";
                if (is_slow )
                {
                    row[colno]["value"] = llformat( "%.2f", render_av_gpu_ms * 1000.f);
                }
                else
                {
                    row[colno]["value"] = llformat( "--" );
                }
                row[colno]["font"]["name"] = "SANSSERIF";
                row[colno]["width"] = "50";
                colno++;
            }

            row[colno]["column"] = "complex_value";
            row[colno]["type"] = "text";
            row[colno]["value"] = std::to_string(complexity_short);
            row[colno]["font"]["name"] = "SANSSERIF";
            row[colno]["width"] = "50";

            colno++;
            row[colno]["column"] = "state";
            row[colno]["type"] = "text";
            if (is_slow)
            {
                if (avatar->isTooSlowWithoutShadows())
                {
                    row[colno]["value"] = std::string{"I"};
                }
                else
                {
                    row[colno]["value"] = std::string{"S"};
                }
            }
            else
            {
                row[colno]["value"] = std::string{" "};
            }

            row[colno]["font"]["name"] = "SANSSERIF";

            colno++;
            row[colno]["column"] = "name";
            colno++;
            row[colno]["column"] = "breakdown";
            row[colno]["type"] = "text";
            row[colno]["value"] = llformat( "%.2f/%.2f/%.2f", LLPerfStats::raw_to_us( render_av_geom ), LLPerfStats::raw_to_us( render_av_shadow ), LLPerfStats::raw_to_us( render_av_idle ) );
            colno++;

            LLScrollListItem* av_item = mNearbyList->addElement(item);
            if (av_item)
            {
                int colno{1};
                LLScrollListText* art_text = dynamic_cast<LLScrollListText*>(av_item->getColumn(colno));
                if (art_text)
                {
                    art_text->setAlignment(LLFontGL::RIGHT);
                }
                colno++;
                LLScrollListText* value_text = dynamic_cast<LLScrollListText*>(av_item->getColumn(colno));
                if (value_text)
                {
                    value_text->setAlignment(LLFontGL::RIGHT);
                }
                colno++;
                if (showTunedART)
                {
                    LLScrollListText* value_text = dynamic_cast<LLScrollListText*>(av_item->getColumn(colno));
                    if (value_text)
                    {
                        value_text->setAlignment(LLFontGL::RIGHT);
                    }
                    colno++;
                }
                LLScrollListText* state_text = dynamic_cast<LLScrollListText*>(av_item->getColumn(colno));
                if (state_text)
                {
                    state_text->setAlignment(LLFontGL::HCENTER);
                }
                colno++;
                LLScrollListText* name_text = dynamic_cast<LLScrollListText*>(av_item->getColumn(colno));
                if (name_text)
                {
                    if (avatar->isSelf())
                    {
                        name_text->setColor(LLUIColorTable::instance().getColor("DrYellow"));
                    }
                    else
                    {
                        std::string color = "white";
                        if (is_slow || LLVOAvatar::AOA_JELLYDOLL == overall_appearance)
                        {
                            color = "LabelDisabledColor";
                            LLScrollListBar* bar = dynamic_cast<LLScrollListBar*>(av_item->getColumn(0));
                            if (bar)
                            {
                                bar->setColor(LLUIColorTable::instance().getColor(color));
                            }
                        }
                        else if (LLVOAvatar::AOA_NORMAL == overall_appearance)
                        {
                            color = LLAvatarActions::isFriend(avatar->getID()) ? "ConversationFriendColor" : "white";
                        }
                        name_text->setColor(LLUIColorTable::instance().getColor(color));
                    }
                }
                colno++;
            }
        }
        char_iter++;
    }
    mNearbyList->sortByColumn(current_sort_col, current_sort_asc);
    mNearbyList->setScrollPos(prev_pos);
    mNearbyList->selectByID(prev_selected_id);

    auto textbox = getChild<LLTextBox>("tot_av_count");
    LLStringUtil::format_map_t args;
    args["TOT_AV"] = llformat("%d", (int64_t)valid_nearby_avs.size());
    args["TOT_AV_TIME"] = llformat("%.2f", LLPerfStats::raw_to_us(av_render_tot_raw));
    textbox->setText(getString("tot_av_template", args));
}

void FSFloaterPerformance::detachItem(const LLUUID& item_id)
{
    LLAppearanceMgr::instance().removeItemFromAvatar(item_id);
}

void FSFloaterPerformance::onChangeQuality(const LLSD& data)
{
    U32 level = (U32)(data.asReal());
    LLFeatureManager::getInstance()->setGraphicsLevel(level, true);
    refresh();
}

void FSFloaterPerformance::onClickHideAvatars()
{
    LLPipeline::toggleRenderTypeControl(LLPipeline::RENDER_TYPE_AVATAR);
}

void FSFloaterPerformance::onClickFocusAvatar()
{
    LLPerfStats::StatsRecorder::setFocusAv(mNearbyCombo->getSelectedValue().asUUID());
}

void FSFloaterPerformance::onClickExceptions()
{
    // <FS:Ansariel> [FS Persisted Avatar Render Settings]
    //LLFloaterReg::showInstance("avatar_render_settings");
    LLFloaterReg::showInstance("fs_avatar_render_settings");
    // </FS:Ansariel>
}

void FSFloaterPerformance::updateMaxComplexity()
{
    LLAvatarComplexityControls::updateMax(
        mNearbyPanel->getChild<LLSliderCtrl>("IndirectMaxComplexity"),
        mNearbyPanel->getChild<LLTextBox>("IndirectMaxComplexityText"),
        true);
}

void FSFloaterPerformance::updateMaxRenderTime()
{
    LLAvatarComplexityControls::updateMaxRenderTime(
        mNearbyPanel->getChild<LLSliderCtrl>("FSRenderAvatarMaxART"),
        mNearbyPanel->getChild<LLTextBox>("FSRenderAvatarMaxARTText"),
        true);
}

void FSFloaterPerformance::updateMaxRenderTimeText()
{
    LLAvatarComplexityControls::setRenderTimeText(
        gSavedSettings.getF32("RenderAvatarMaxART"),
        mNearbyPanel->getChild<LLTextBox>("RenderAvatarMaxARTText", true),
        true);
}

void FSFloaterPerformance::updateComplexityText()
{
    LLAvatarComplexityControls::setText(gSavedSettings.getU32("RenderAvatarMaxComplexity"),
        mNearbyPanel->getChild<LLTextBox>("IndirectMaxComplexityText", true),
        true);
}

static LLVOAvatar* find_avatar(const LLUUID& id)
{
    LLViewerObject *obj = gObjectList.findObject(id);
    while (obj && obj->isAttachment())
    {
        obj = (LLViewerObject *)obj->getParent();
    }

    if (obj && obj->isAvatar())
    {
        return (LLVOAvatar*)obj;
    }
    else
    {
        return NULL;
    }
}

void FSFloaterPerformance::onCustomAction(const LLSD& userdata, const LLUUID& av_id)
{
    const std::string command_name = userdata.asString();

    LLVOAvatar::VisualMuteSettings new_setting = LLVOAvatar::AV_RENDER_NORMALLY;
    if ("default" == command_name)
    {
        new_setting = LLVOAvatar::AV_RENDER_NORMALLY;
    }
    else if ("never" == command_name)
    {
        new_setting = LLVOAvatar::AV_DO_NOT_RENDER;
    }
    else if ("always" == command_name)
    {
        new_setting = LLVOAvatar::AV_ALWAYS_RENDER;
    }

    LLVOAvatar *avatarp = find_avatar(av_id);
    if (avatarp)
    {
        avatarp->setVisualMuteSettings(new_setting);
    }
    else
    {
        // <FS:Ansariel> [FS Persisted Avatar Render Settings]
        //LLRenderMuteList::getInstance()->saveVisualMuteSetting(av_id, new_setting);
        FSAvatarRenderPersistence::instance().setAvatarRenderSettings(av_id, LLVOAvatar::VisualMuteSettings(new_setting));
        // </FS:Ansariel>
    }
}

bool FSFloaterPerformance::isActionChecked(const LLSD& userdata, const LLUUID& av_id)
{
    const std::string command_name = userdata.asString();

    // <FS:Ansariel> [FS Persisted Avatar Render Settings]
    //S32 visual_setting = LLRenderMuteList::getInstance()->getSavedVisualMuteSetting(av_id);
    S32 visual_setting = (S32)FSAvatarRenderPersistence::instance().getAvatarRenderSettings(av_id);
    // </FS:Ansariel>
    if ("default" == command_name)
    {
        return (visual_setting == LLVOAvatar::AV_RENDER_NORMALLY);
    }
    else if ("non_default" == command_name)
    {
        return (visual_setting != LLVOAvatar::AV_RENDER_NORMALLY);
    }
    else if ("never" == command_name)
    {
        return (visual_setting == LLVOAvatar::AV_DO_NOT_RENDER);
    }
    else if ("always" == command_name)
    {
        return (visual_setting == LLVOAvatar::AV_ALWAYS_RENDER);
    }
    return false;
}

void FSFloaterPerformance::onAvatarListRightClick(LLUICtrl* ctrl, S32 x, S32 y)
{
    LLNameListCtrl* list = dynamic_cast<LLNameListCtrl*>(ctrl);
    if (!list) return;
    list->selectItemAt(x, y, MASK_NONE);
    uuid_vec_t selected_uuids;

    if ((list->getCurrentID().notNull()) && (list->getCurrentID() != gAgentID))
    {
        selected_uuids.push_back(list->getCurrentID());
        mContextMenu->show(ctrl, selected_uuids, x, y);
    }
}

void FSFloaterPerformance::onExtendedAction(const LLSD& userdata, const LLUUID& av_id)
{
    const std::string command_name = userdata.asString();

    LLViewerObject* objectp = gObjectList.findObject(av_id);
    if (!objectp)
    {
        return;
    }
    auto avp = objectp->asAvatar();
    if ("inspect" == command_name)
    {
        for (LLVOAvatar::attachment_map_t::iterator iter = avp->mAttachmentPoints.begin();
             iter != avp->mAttachmentPoints.end();
             ++iter)
        {
            LLViewerJointAttachment* attachment = iter->second;

            if (!attachment)
            {
                continue;
            }

            for (LLViewerJointAttachment::attachedobjs_vec_t::iterator attachment_iter = attachment->mAttachedObjects.begin();
                 attachment_iter != attachment->mAttachedObjects.end();
                 ++attachment_iter)
            {
                LLViewerObject* attached_object = attachment_iter->get();

                if ( attached_object && !attached_object->isDead() )
                {
                    LLSelectMgr::getInstance()->selectObjectAndFamily(attached_object);
                }
            }
        }
        LLFloaterReg::showInstance("inspect");
    }
    else if ("zoom" == command_name)
    {
        // Disable flycam if active.  Without this, the requested look-at doesn't happen because the flycam code overrides all other camera motion.
        bool fly_cam_status(LLViewerJoystick::getInstance()->getOverrideCamera());
        if (fly_cam_status)
        {
            LLViewerJoystick::getInstance()->setOverrideCamera(false);
            LLPanelStandStopFlying::clearStandStopFlyingMode(LLPanelStandStopFlying::SSFM_FLYCAM);
            // *NOTE: Above may not be the proper way to disable flycam.  What I really want to do is just be able to move the camera and then leave the flycam in the the same state it was in, just moved to the new location. ~Cron
        }

        LLViewerJoystick::getInstance()->setCameraNeedsUpdate(true); // Fixes an edge case where if the user has JUST disabled flycam themselves, the camera gets stuck waiting for input.

        gAgentCamera.setFocusOnAvatar(false, ANIMATE);

        gAgentCamera.setLookAt(LOOKAT_TARGET_SELECT, objectp);

        // Place the camera looking at the object, along the line from the camera to the object,
        //  and sufficiently far enough away for the object to fill 3/4 of the screen,
        //  but not so close that the bbox's nearest possible vertex goes inside the near clip.
        // Logic C&P'd from LLViewerMediaFocus::setCameraZoom() and then edited as needed

        LLBBox bbox = objectp->getBoundingBoxAgent();
        LLVector3d center(gAgent.getPosGlobalFromAgent(bbox.getCenterAgent()));
        F32 height;
        F32 width;
        F32 depth;
        F32 angle_of_view;
        F32 distance;

        LLVector3d target_pos(center);
        LLVector3d camera_dir(gAgentCamera.getCameraPositionGlobal() - target_pos);
        camera_dir.normalize();

        // We need the aspect ratio, and the 3 components of the bbox as height, width, and depth.
        F32 aspect_ratio(LLViewerMediaFocus::getBBoxAspectRatio(bbox, LLVector3(camera_dir), &height, &width, &depth));
        F32 camera_aspect(LLViewerCamera::getInstance()->getAspect());

        // We will normally use the side of the volume aligned with the short side of the screen (i.e. the height for
        // a screen in a landscape aspect ratio), however there is an edge case where the aspect ratio of the object is
        // more extreme than the screen.  In this case we invert the logic, using the longer component of both the object
        // and the screen.
        bool invert((camera_aspect > 1.0f && aspect_ratio > camera_aspect) || (camera_aspect < 1.0f && aspect_ratio < camera_aspect));

        // To calculate the optimum viewing distance we will need the angle of the shorter side of the view rectangle.
        // In portrait mode this is the width, and in landscape it is the height.
        // We then calculate the distance based on the corresponding side of the object bbox (width for portrait, height for landscape)
        // We will add half the depth of the bounding box, as the distance projection uses the center point of the bbox.
        if (camera_aspect < 1.0f || invert)
        {
            angle_of_view = llmax(0.1f, LLViewerCamera::getInstance()->getView() * LLViewerCamera::getInstance()->getAspect());
            distance = width * 0.5f * 1.1f / tanf(angle_of_view * 0.5f);
        }
        else
        {
            angle_of_view = llmax(0.1f, LLViewerCamera::getInstance()->getView());
            distance = height * 0.5f * 1.1f / tanf(angle_of_view * 0.5f);
        }

        distance += depth * 0.5f;

        // Verify that the bounding box isn't inside the near clip.  Using OBB-plane intersection to check if the
        // near-clip plane intersects with the bounding box, and if it does, adjust the distance such that the
        // object doesn't clip.
        LLVector3d bbox_extents(bbox.getExtentLocal());
        LLVector3d axis_x = LLVector3d(1, 0, 0) * bbox.getRotation();
        LLVector3d axis_y = LLVector3d(0, 1, 0) * bbox.getRotation();
        LLVector3d axis_z = LLVector3d(0, 0, 1) * bbox.getRotation();
        //Normal of nearclip plane is camera_dir.
        F32 min_near_clip_dist = (F32)(bbox_extents.mdV[VX] * (camera_dir * axis_x) + bbox_extents.mdV[VY] * (camera_dir * axis_y) + bbox_extents.mdV[VZ] * (camera_dir * axis_z)); // http://www.gamasutra.com/view/feature/131790/simple_intersection_tests_for_games.php?page=7
        F32 camera_to_near_clip_dist(LLViewerCamera::getInstance()->getNear());
        F32 min_camera_dist(min_near_clip_dist + camera_to_near_clip_dist);
        if (distance < min_camera_dist)
        {
            // Camera is too close to object, some parts MIGHT clip.  Move camera away to the position where clipping barely doesn't happen.
            distance = min_camera_dist;
        }

        LLVector3d camera_pos(target_pos + camera_dir * distance);

        if (camera_dir == LLVector3d::z_axis || camera_dir == LLVector3d::z_axis_neg)
        {
            // If the direction points directly up, the camera will "flip" around.
            // We try to avoid this by adjusting the target camera position a
            // smidge towards current camera position
            // *NOTE: this solution is not perfect.  All it attempts to solve is the
            // "looking down" problem where the camera flips around when it animates
            // to that position.  You still are not guaranteed to be looking at the
            // object in the correct orientation.  What this solution does is it will
            // put the camera into position keeping as best it can the current
            // orientation with respect to the direction wanted.  In other words, if
            // before zoom the object appears "upside down" from the camera, after
            /// zooming it will still be upside down, but at least it will not flip.
            LLVector3d cur_camera_pos = LLVector3d(gAgentCamera.getCameraPositionGlobal());
            LLVector3d delta = (cur_camera_pos - camera_pos);
            F64 len = delta.length();
            delta.normalize();
            // Move 1% of the distance towards original camera location
            camera_pos += 0.01 * len * delta;
        }

        gAgentCamera.setCameraPosAndFocusGlobal(camera_pos, target_pos, objectp->getID());

        // *TODO: Re-enable joystick flycam if we disabled it earlier...  Have to find some form of callback as re-enabling at this point causes the camera motion to not happen. ~Cron
        //if (fly_cam_status)
        //{
        //  LLViewerJoystick::getInstance()->toggleFlycam();
        //}
    }
}

// EOF
