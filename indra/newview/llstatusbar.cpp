
/**
* @file llstatusbar.cpp
* @brief LLStatusBar class implementation
*
* $LicenseInfo:firstyear=2002&license=viewerlgpl$
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

#include "llstatusbar.h"

// viewer includes
#include "llagent.h"
#include "llagentcamera.h"
#include "llbutton.h"
#include "llcommandhandler.h"
#include "llfirstuse.h"
#include "llviewercontrol.h"
#include "llfloaterbuycurrency.h"
#include "llbuycurrencyhtml.h"
#include "llpanelnearbymedia.h"
#include "llpanelpresetscamerapulldown.h"
#include "llpanelpresetspulldown.h"
#include "llpanelvolumepulldown.h"
#include "llfloaterregioninfo.h"
#include "llfloaterscriptdebug.h"
#include "llhints.h"
#include "llhudicon.h"
#include "llnavigationbar.h"
#include "llkeyboard.h"
#include "lllineeditor.h"
#include "llmenugl.h"
#include "llrootview.h"
#include "llsd.h"
#include "lltextbox.h"
#include "llui.h"
#include "llviewerparceloverlay.h"
#include "llviewerregion.h"
#include "llviewerstats.h"
#include "llviewerwindow.h"
#include "llframetimer.h"
#include "llvoavatarself.h"
#include "llresmgr.h"
#include "llworld.h"
#include "llstatgraph.h"
#include "llurlaction.h"
#include "llviewermedia.h"
#include "llviewermenu.h"   // for gMenuBarView
#include "llviewerparcelmgr.h"
#include "llviewerthrottle.h"
#include "llwindow.h"
#include "lluictrlfactory.h"

#include "lltoolmgr.h"
#include "llfocusmgr.h"
#include "llappviewer.h"
#include "lltrans.h"

// library includes
#include "llfloaterreg.h"
#include "llfontgl.h"
#include "llrect.h"
#include "llerror.h"
#include "llnotificationsutil.h"
#include "llparcel.h"
#include "llstring.h"
#include "message.h"
#include "llsearchableui.h"
#include "llsearcheditor.h"

// system includes
#include <iomanip>

// Firestorm includes
#include "llagentui.h"
#include "llaudioengine.h"
#include "llclipboard.h"
#include "llfloatersidepanelcontainer.h"
#include "lllandmarkactions.h"
#include "lllocationinputctrl.h"
#include "llmenuoptionpathfindingrebakenavmesh.h"
#include "llparcel.h"
#include "llslurl.h"
#include "llvieweraudio.h"
#include "llviewerinventory.h"
#ifdef OPENSIM
#include "llviewernetwork.h"
#endif // OPENSIM
#include "llviewerparcelmedia.h"
#include "rlvhandler.h"

//
// Globals
//

LLStatusBar *gStatusBar = NULL;
S32 STATUS_BAR_HEIGHT = 26;
extern S32 MENU_BAR_HEIGHT;

// <FS:LO> FIRE-7639 - Stop the blinking after a while
LLViewerRegion* agent_region;
bool bakingStarted = false;

class LORebakeStuck;
LORebakeStuck *bakeTimeout = NULL;

class LORebakeStuck: public LLEventTimer
{
public:
    LORebakeStuck(LLStatusBar *bar) : LLEventTimer(30.0f)
    {
        mbar=bar;
    }
    ~LORebakeStuck()
    {
        bakeTimeout=NULL;
    }
    bool tick()
    {
        mbar->setRebakeStuck(true);
        return true;
    }
private:
    LLStatusBar *mbar;
};
// </FS:LO>


// TODO: these values ought to be in the XML too
const S32 MENU_PARCEL_SPACING = 1;  // Distance from right of menu item to parcel information
const S32 SIM_STAT_WIDTH = 8;
const LLColor4 SIM_OK_COLOR(0.f, 1.f, 0.f, 1.f);
const LLColor4 SIM_WARN_COLOR(1.f, 1.f, 0.f, 1.f);
const LLColor4 SIM_FULL_COLOR(1.f, 0.f, 0.f, 1.f);
const F32 ICON_TIMER_EXPIRY     = 3.f; // How long the balance and health icons should flash after a change.

// <FS:Ansariel> Open popup panels on click if FSStatusBarMenuButtonPopupOnRollover is disabled
//static void onClickVolume(void*);

class LLStatusBar::LLParcelChangeObserver : public LLParcelObserver
{
public:
    LLParcelChangeObserver(LLStatusBar* topInfoBar) : mTopInfoBar(topInfoBar) {}

private:
    /*virtual*/ void changed()
    {
        if (mTopInfoBar)
        {
            mTopInfoBar->updateParcelIcons();
        }
    }

    LLStatusBar* mTopInfoBar;
};

LLStatusBar::LLStatusBar(const LLRect& rect)
:   LLPanel(),
    mTextTime(NULL),
    mSGBandwidth(NULL),
    mSGPacketLoss(NULL),
    mBandwidthButton(NULL), // <FS:PP> FIRE-6287: Clicking on traffic indicator toggles Lag Meter window
    mBtnVolume(NULL),
    mBoxBalance(NULL),
    mBalance(0),
    mHealth(100),
    mShowParcelIcons(true),
    mSquareMetersCredit(0),
    mSquareMetersCommitted(0),
    mFilterEdit(NULL),          // Edit for filtering
    mSearchPanel(NULL),         // Panel for filtering
    mPathfindingFlashOn(true),  // <FS:Zi> Pathfinding rebake functions
    mAudioStreamEnabled(false), // <FS:Zi> Media/Stream separation
    mRebakeStuck(false),        // <FS:LO> FIRE-7639 - Stop the blinking after a while
    mNearbyIcons(false),        // <FS:Ansariel> Script debug
    mIconPresetsGraphic(NULL),
    mIconPresetsCamera(NULL),
    mMediaToggle(NULL),
    mMouseEnterPresetsConnection(),
    mMouseEnterPresetsCameraConnection(),
    mMouseEnterVolumeConnection(),
    mMouseEnterNearbyMediaConnection(),
    mCurrentLocationString()
{
    setRect(rect);

    // status bar can possible overlay menus?
    setMouseOpaque(false);

    mBalanceTimer = new LLFrameTimer();
    mHealthTimer = new LLFrameTimer();

    LLUICtrl::CommitCallbackRegistry::currentRegistrar()
            .add("TopInfoBar.Action", boost::bind(&LLStatusBar::onContextMenuItemClicked, this, _2));

    gSavedSettings.getControl("ShowNetStats")->getSignal()->connect(boost::bind(&LLStatusBar::updateNetstatVisibility, this, _2));

    // <FS:PP> Option to hide volume controls (sounds, media, stream) in upper right
    gSavedSettings.getControl("FSEnableVolumeControls")->getSignal()->connect(boost::bind(&LLStatusBar::updateVolumeControlsVisibility, this, _2));

    buildFromFile("panel_status_bar.xml");
}

LLStatusBar::~LLStatusBar()
{
    delete mBalanceTimer;
    mBalanceTimer = NULL;

    delete mHealthTimer;
    mHealthTimer = NULL;

    if (mParcelChangedObserver)
    {
        LLViewerParcelMgr::getInstance()->removeObserver(mParcelChangedObserver);
        delete mParcelChangedObserver;
    }

    if (mParcelPropsCtrlConnection.connected())
    {
        mParcelPropsCtrlConnection.disconnect();
    }

    if (mParcelMgrConnection.connected())
    {
        mParcelMgrConnection.disconnect();
    }

    if (mShowCoordsCtrlConnection.connected())
    {
        mShowCoordsCtrlConnection.disconnect();
    }

    // <FS:Ansariel> FIRE-19697: Add setting to disable graphics preset menu popup on mouse over
    if (mMouseEnterPresetsConnection.connected())
    {
        mMouseEnterPresetsConnection.disconnect();
    }
    if (mMouseEnterPresetsCameraConnection.connected())
    {
        mMouseEnterPresetsCameraConnection.disconnect();
    }
    if (mMouseEnterVolumeConnection.connected())
    {
        mMouseEnterVolumeConnection.disconnect();
    }
    if (mMouseEnterNearbyMediaConnection.connected())
    {
        mMouseEnterNearbyMediaConnection.disconnect();
    }
    // </FS:Ansariel>

    // LLView destructor cleans up children
}

//-----------------------------------------------------------------------
// Overrides
//-----------------------------------------------------------------------

// virtual
void LLStatusBar::draw()
{
    refresh();
    updateParcelInfoText();
    updateHealth();
    LLPanel::draw();
}

bool LLStatusBar::handleRightMouseDown(S32 x, S32 y, MASK mask)
{
    show_navbar_context_menu(this,x,y);
    return true;
}

bool LLStatusBar::postBuild()
{
    gMenuBarView->setRightMouseDownCallback(boost::bind(&show_navbar_context_menu, _1, _2, _3));

    mTextTime = getChild<LLTextBox>("TimeText" );

    getChild<LLUICtrl>("buyL")->setCommitCallback(
        boost::bind(&LLStatusBar::onClickBuyCurrency, this));

    // <FS:Ansariel> Not used in Firestorm
    //getChild<LLUICtrl>("goShop")->setCommitCallback(boost::bind(&LLWeb::loadURL, gSavedSettings.getString("MarketplaceURL"), LLStringUtil::null, LLStringUtil::null));

    mBoxBalance = getChild<LLTextBox>("balance");
    mBoxBalance->setClickedCallback( &LLStatusBar::onClickBalance, this );

    mIconPresetsCamera = getChild<LLButton>( "presets_icon_camera" );
    //mIconPresetsCamera->setMouseEnterCallback(boost::bind(&LLStatusBar::mIconPresetsCamera, this));
    if (gSavedSettings.getBOOL("FSStatusBarMenuButtonPopupOnRollover"))
    {
        mMouseEnterPresetsCameraConnection = mIconPresetsCamera->setMouseEnterCallback(boost::bind(&LLStatusBar::onMouseEnterPresetsCamera, this));
    }
    mIconPresetsCamera->setClickedCallback(boost::bind(&LLStatusBar::onMouseEnterPresetsCamera, this));

    mIconPresetsGraphic = getChild<LLButton>( "presets_icon_graphic" );
    // <FS: KC> FIRE-19697: Add setting to disable graphics preset menu popup on mouse over
    // mIconPresetsGraphic->setMouseEnterCallback(boost::bind(&LLStatusBar::onMouseEnterPresets, this));
    if (gSavedSettings.getBOOL("FSStatusBarMenuButtonPopupOnRollover"))
    {
        mMouseEnterPresetsConnection = mIconPresetsGraphic->setMouseEnterCallback(boost::bind(&LLStatusBar::onMouseEnterPresets, this));
    }
    // </FS: KC> FIRE-19697: Add setting to disable graphics preset menu popup on mouse over
    mIconPresetsGraphic->setClickedCallback(boost::bind(&LLStatusBar::onMouseEnterPresets, this));

    mBtnVolume = getChild<LLButton>( "volume_btn" );
    mBtnVolume->setClickedCallback( onClickVolume, this );
    // <FS: KC> FIRE-19697: Add setting to disable status bar icon menu popup on mouseover
    // mBtnVolume->setMouseEnterCallback(boost::bind(&LLStatusBar::onMouseEnterVolume, this));
    if (gSavedSettings.getBOOL("FSStatusBarMenuButtonPopupOnRollover"))
    {
        mMouseEnterVolumeConnection = mBtnVolume->setMouseEnterCallback(boost::bind(&LLStatusBar::onMouseEnterVolume, this));
    }
    // </FS: KC> FIRE-19697: Add setting to disable status bar icon menu popup on mouseover

    // <FS:Zi> Media/Stream separation
    mStreamToggle = getChild<LLButton>("stream_toggle_btn");
    mStreamToggle->setClickedCallback(&LLStatusBar::onClickStreamToggle, this);
    // </FS:Zi> Media/Stream separation

    mMediaToggle = getChild<LLButton>("media_toggle_btn");
    mMediaToggle->setClickedCallback( &LLStatusBar::onClickMediaToggle, this );
    // <FS: KC> FIRE-19697: Add setting to disable status bar icon menu popup on mouseover
    // mMediaToggle->setMouseEnterCallback(boost::bind(&LLStatusBar::onMouseEnterNearbyMedia, this));
    if (gSavedSettings.getBOOL("FSStatusBarMenuButtonPopupOnRollover"))
    {
        mMouseEnterNearbyMediaConnection = mMediaToggle->setMouseEnterCallback(boost::bind(&LLStatusBar::onMouseEnterNearbyMedia, this));
    }
    // </FS: KC> FIRE-19697: Add setting to disable status bar icon menu popup on mouseover

    LLHints::getInstance()->registerHintTarget("linden_balance", getChild<LLView>("balance_bg")->getHandle());

    gSavedSettings.getControl("MuteAudio")->getSignal()->connect(boost::bind(&LLStatusBar::onVolumeChanged, this, _2));
    // <FS:Ansariel> Fix LL voice disabled on 2nd instance nonsense
    //gSavedSettings.getControl("EnableVoiceChat")->getSignal()->connect(boost::bind(&LLStatusBar::onVoiceChanged, this, _2));

    //if (!gSavedSettings.getBOOL("EnableVoiceChat") && LLAppViewer::instance()->isSecondInstance())
    //{
    //    // Indicate that second instance started without sound
    //    mBtnVolume->setImageUnselected(LLUI::getUIImage("VoiceMute_Off"));
    //}
    // </FS:Ansariel>

    // <FS:Ansariel> FIRE-19697: Add setting to disable graphics preset menu popup on mouse over
    gSavedSettings.getControl("FSStatusBarMenuButtonPopupOnRollover")->getSignal()->connect(boost::bind(&LLStatusBar::onPopupRolloverChanged, this, _2));

    // Adding Net Stat Graph
    S32 x = getRect().getWidth() - 2;
    S32 y = 0;
    LLRect r;

    // <FS:PP> FIRE-6287: Clicking on traffic indicator toggles Lag Meter window
    r.set( x-((SIM_STAT_WIDTH*2)+2), y+MENU_BAR_HEIGHT-1, x, y+1);
    LLButton::Params BandwidthButton;
    BandwidthButton.name(std::string("BandwidthGraphButton"));
    BandwidthButton.label("");
    BandwidthButton.rect(r);
    BandwidthButton.follows.flags(FOLLOWS_BOTTOM | FOLLOWS_RIGHT);
    BandwidthButton.click_callback.function(boost::bind(&LLStatusBar::onBandwidthGraphButtonClicked, this));
    mBandwidthButton = LLUICtrlFactory::create<LLButton>(BandwidthButton);
    addChild(mBandwidthButton);
    LLColor4 BandwidthButtonOpacity;
    BandwidthButtonOpacity.setAlpha(0);
    mBandwidthButton->setColor(BandwidthButtonOpacity);
    // </FS:PP> FIRE-6287: Clicking on traffic indicator toggles Lag Meter window

    r.set( x-SIM_STAT_WIDTH, y+MENU_BAR_HEIGHT-1, x, y+1);
    LLStatGraph::Params sgp;
    sgp.name("BandwidthGraph");
    sgp.rect(r);
    sgp.follows.flags(FOLLOWS_BOTTOM | FOLLOWS_RIGHT);
    sgp.mouse_opaque(false);
    sgp.stat.count_stat_float(&LLStatViewer::ACTIVE_MESSAGE_DATA_RECEIVED);
    sgp.units("Kbps");
    sgp.precision(0);
    sgp.per_sec(true);
    mSGBandwidth = LLUICtrlFactory::create<LLStatGraph>(sgp);
    addChild(mSGBandwidth);
    x -= SIM_STAT_WIDTH + 2;

    r.set( x-SIM_STAT_WIDTH, y+MENU_BAR_HEIGHT-1, x, y+1);
    //these don't seem to like being reused
    LLStatGraph::Params pgp;
    pgp.name("PacketLossPercent");
    pgp.rect(r);
    pgp.follows.flags(FOLLOWS_BOTTOM | FOLLOWS_RIGHT);
    pgp.mouse_opaque(false);
    pgp.stat.sample_stat_float(&LLStatViewer::PACKETS_LOST_PERCENT);
    pgp.units("%");
    pgp.min(0.f);
    pgp.max(5.f);
    pgp.precision(1);
    pgp.per_sec(false);
    LLStatGraph::Thresholds thresholds;
    thresholds.threshold.add(LLStatGraph::ThresholdParams().value(0.1f).color(LLColor4::green))
                        .add(LLStatGraph::ThresholdParams().value(0.25f).color(LLColor4::yellow))
                        .add(LLStatGraph::ThresholdParams().value(0.6f).color(LLColor4::red));

    pgp.thresholds(thresholds);

    mSGPacketLoss = LLUICtrlFactory::create<LLStatGraph>(pgp);
    addChild(mSGPacketLoss);

    mPanelPresetsCameraPulldown = new LLPanelPresetsCameraPulldown();
    addChild(mPanelPresetsCameraPulldown);
    mPanelPresetsCameraPulldown->setFollows(FOLLOWS_TOP|FOLLOWS_RIGHT);
    mPanelPresetsCameraPulldown->setVisible(false);

    mPanelPresetsPulldown = new LLPanelPresetsPulldown();
    addChild(mPanelPresetsPulldown);
    mPanelPresetsPulldown->setFollows(FOLLOWS_TOP|FOLLOWS_RIGHT);
    mPanelPresetsPulldown->setVisible(false);

    mPanelVolumePulldown = new LLPanelVolumePulldown();
    addChild(mPanelVolumePulldown);
    mPanelVolumePulldown->setFollows(FOLLOWS_TOP|FOLLOWS_RIGHT);
    mPanelVolumePulldown->setVisible(false);

    mPanelNearByMedia = new LLPanelNearByMedia();
    addChild(mPanelNearByMedia);
    mPanelNearByMedia->setFollows(FOLLOWS_TOP|FOLLOWS_RIGHT);
    mPanelNearByMedia->setVisible(false);

    updateBalancePanelPosition();

    // Hook up and init for filtering
    mFilterEdit = getChild<LLSearchEditor>( "search_menu_edit" );
    mSearchPanel = getChild<LLPanel>( "menu_search_panel" );

    bool search_panel_visible = gSavedSettings.getBOOL("MenuSearch");
    mSearchPanel->setVisible(search_panel_visible);
    mFilterEdit->setKeystrokeCallback(boost::bind(&LLStatusBar::onUpdateFilterTerm, this));
    mFilterEdit->setCommitCallback(boost::bind(&LLStatusBar::onUpdateFilterTerm, this));
    collectSearchableItems();
    gSavedSettings.getControl("MenuSearch")->getCommitSignal()->connect(boost::bind(&LLStatusBar::updateMenuSearchVisibility, this, _2));

    if (search_panel_visible)
    {
        updateMenuSearchPosition();
    }

    // <FS:Ansariel> Script debug
    mScriptOut = getChild<LLIconCtrl>("scriptout");
    mScriptOut->setMouseDownCallback(boost::bind(&LLFloaterScriptDebug::show, LLUUID::null));
    mNearbyIcons = LLHUDIcon::scriptIconsNearby();
    // </FS:Ansariel> Script debug

    mParcelInfoPanel = getChild<LLPanel>("parcel_info_panel");
    mParcelInfoText = getChild<LLTextBox>("parcel_info_text");

    // Ansariel: Removed the info button in favor of clickable parcel info text
    mParcelInfoText->setClickedCallback(boost::bind(&LLStatusBar::onInfoButtonClicked, this));
    // <FS:Zi> Make hovering over parcel info actually work
    //         Since <text ...> doesn't have any hover functions, add this in code
    mParcelInfoText->setMouseEnterCallback(boost::bind(&LLStatusBar::onMouseEnterParcelInfo, this));
    mParcelInfoText->setMouseLeaveCallback(boost::bind(&LLStatusBar::onMouseLeaveParcelInfo, this));
    // </FS:Zi>

    mDamageText = getChild<LLTextBox>("damage_text");

    mBuyParcelBtn = getChild<LLButton>("buy_land_btn");
    mBuyParcelBtn->setClickedCallback(boost::bind(&LLStatusBar::onBuyLandClicked, this));

    mBalancePanel = getChild<LLPanel>("balance_bg");
    mTimeMediaPanel = getChild<LLPanel>("time_and_media_bg");

    // <FS:Beq> Make FPS a clickable button with contextual colour
    // mFPSText = getChild<LLButton>("FPSText");
    mFPSText = getChild<LLTextBox>("FPSText");
    mFPSText->setClickedCallback(std::bind(&LLUrlAction::executeSLURL, "secondlife:///app/openfloater/preferences?search=limitframerate", true));
    // </FS:Beq>
    mVolumeIconsWidth = mBtnVolume->getRect().mRight - mStreamToggle->getRect().mLeft;

    initParcelIcons();

    mParcelChangedObserver = new LLParcelChangeObserver(this);
    LLViewerParcelMgr::getInstance()->addObserver(mParcelChangedObserver);

    // Connecting signal for updating parcel icons on "Show Parcel Properties" setting change.
    LLControlVariable* ctrl = gSavedSettings.getControl("NavBarShowParcelProperties").get();
    if (ctrl)
    {
        mParcelPropsCtrlConnection = ctrl->getSignal()->connect(boost::bind(&LLStatusBar::onNavBarShowParcelPropertiesCtrlChanged, this));
        onNavBarShowParcelPropertiesCtrlChanged();
    }

    // Connecting signal for updating parcel text on "Show Coordinates" setting change.
    ctrl = gSavedSettings.getControl("NavBarShowCoordinates").get();
    if (ctrl)
    {
        mShowCoordsCtrlConnection = ctrl->getSignal()->connect(boost::bind(&LLStatusBar::onNavBarShowCoordinatesCtrlChanged, this));
    }

    mParcelMgrConnection = gAgent.addParcelChangedCallback(
            boost::bind(&LLStatusBar::onAgentParcelChange, this));

    if (!gSavedSettings.getBOOL("ShowNetStats"))
    {
        updateNetstatVisibility(LLSD(false));
    }

    // <FS:Ansariel> FIRE-14482: Show FPS in status bar
    gSavedSettings.getControl("FSStatusBarShowFPS")->getSignal()->connect(boost::bind(&LLStatusBar::onShowFPSChanged, this, _2));
    if (!gSavedSettings.getBOOL("FSStatusBarShowFPS"))
    {
        onShowFPSChanged(LLSD(false));
    }
    // </FS:Ansariel>

    // <FS:PP> Option to hide volume controls (sounds, media, stream) in upper right
    if (!gSavedSettings.getBOOL("FSEnableVolumeControls"))
    {
        updateVolumeControlsVisibility(LLSD(false));
    }
    // </FS:PP>

    // <FS:Zi> FIRE-20390, FIRE-4269 - Option for 12/24 hour clock and seconds display
    mClockFormatChoices["12 Hour"] = "[hour12, datetime, slt]:[min, datetime, slt] [ampm, datetime, slt]";
    mClockFormatChoices["12 Hour Seconds"] = "[hour12, datetime, slt]:[min, datetime, slt]:[second, datetime, slt] [ampm, datetime, slt]";
    mClockFormatChoices["12 Hour TZ"] = "[hour12, datetime, slt]:[min, datetime, slt] [ampm, datetime, slt] [timezone,datetime, slt]";
    mClockFormatChoices["12 Hour TZ Seconds"] = "[hour12, datetime, slt]:[min, datetime, slt]:[second, datetime, slt] [ampm, datetime, slt] [timezone,datetime, slt]";
    mClockFormatChoices["24 Hour"] = "[hour24, datetime, slt]:[min, datetime, slt]";
    mClockFormatChoices["24 Hour Seconds"] = "[hour24, datetime, slt]:[min, datetime, slt]:[second, datetime, slt]";
    mClockFormatChoices["24 Hour TZ"] = "[hour24, datetime, slt]:[min, datetime, slt] [timezone, datetime, slt]";
    mClockFormatChoices["24 Hour TZ Seconds"] = "[hour24, datetime, slt]:[min, datetime, slt]:[second, datetime, slt] [timezone, datetime, slt]";

    // use the time format defined in the language's panel_status_bar.xml (default)
    mClockFormatChoices["Language"] = getString("time");

    mClockFormat = gSavedSettings.getString("FSStatusBarTimeFormat");
    // </FS:Zi>

    return true;
}

// <FS:Zi> FIRE-20390, FIRE-4269 - Option for 12/24 hour clock and seconds display
void LLStatusBar::updateClockDisplay()
{
    // Get current UTC time, adjusted for the user's clock
    // being off.
    time_t utc_time;
    utc_time = time_corrected();

    std::string timeStr = mClockFormatChoices[mClockFormat];
    LLSD substitution;
    substitution["datetime"] = (S32) utc_time;
    LLStringUtil::format (timeStr, substitution);
    mTextTime->setText(timeStr);

    // <FS:Ansariel> Add seconds to clock
    static const std::string tooltip_template = getString("timeTooltip");
    std::string dtStr = tooltip_template;
    // </FS:Ansariel>
    LLStringUtil::format (dtStr, substitution);
    mTextTime->setToolTip (dtStr);
}
// </FS:Zi>

// Per-frame updates of visibility
void LLStatusBar::refresh()
{
    static LLCachedControl<bool> show_net_stats(gSavedSettings, "ShowNetStats", false);
    bool net_stats_visible = show_net_stats;

    // <FS:Ansariel> Less frequent update of net stats
    //if (net_stats_visible)
    //{
    if (net_stats_visible && mNetStatUpdateTimer.getElapsedTimeF32() > 0.5f)
    {
        mNetStatUpdateTimer.reset();
    // </FS:Ansariel>

        // Adding Net Stat Meter back in
        F32 bwtotal = gViewerThrottle.getMaxBandwidth() / 1000.f;
        mSGBandwidth->setMin(0.f);
        mSGBandwidth->setMax(bwtotal*1.25f);
        //mSGBandwidth->setThreshold(0, bwtotal*0.75f);
        //mSGBandwidth->setThreshold(1, bwtotal);
        //mSGBandwidth->setThreshold(2, bwtotal);
    }

    // <FS:Ansariel> FIRE-14482: Show FPS in status bar
    static LLCachedControl<bool> fsStatusBarShowFPS(gSavedSettings, "FSStatusBarShowFPS");
    if (fsStatusBarShowFPS && mFPSUpdateTimer.getElapsedTimeF32() > 1.f)
    {
        static LLCachedControl<U32>  max_fps(gSavedSettings, "FramePerSecondLimit");
        static LLCachedControl<bool> limit_fps_enabled(gSavedSettings, "FSLimitFramerate");
        static LLCachedControl<bool> vsync_enabled(gSavedSettings, "RenderVSyncEnable");

        static const auto fps_below_limit_color     = LLUIColorTable::instance().getColor("Yellow");
        static const auto fps_limit_reached_color   = LLUIColorTable::instance().getColor("Green");
        static const auto vsync_limit_reached_color = LLUIColorTable::instance().getColor("Green");
        static const auto fps_uncapped_color        = LLUIColorTable::instance().getColor("White");
        static const auto fps_unfocussed_color      = LLUIColorTable::instance().getColor("Gray");
        static auto       current_fps_color         = fps_uncapped_color;

        mFPSUpdateTimer.reset();
        const auto fps = LLTrace::get_frame_recording().getPeriodMedianPerSec(LLStatViewer::FPS);
        mFPSText->setText(llformat("%.1f", fps));

        // if background, go grey, else go white unless we have a cap (checked next)
        auto fps_color{ fps_uncapped_color };
        auto window = gViewerWindow ? gViewerWindow->getWindow() : nullptr;
        if ((window && !window->getVisible()) || !gFocusMgr.getAppHasFocus())
        {
            fps_color = fps_unfocussed_color;
        }
        else
        {
            S32 vsync_freq{ -1 };
            if (window)
            {
                vsync_freq = window->getRefreshRate();
            }

            if (limit_fps_enabled && max_fps > 0)
            {
                fps_color = (fps >= max_fps - 1) ? fps_limit_reached_color : fps_below_limit_color;
            }
            // use vsync if enabled and the freq is lower than the max_fps
            if (vsync_enabled && vsync_freq > 0 && (!limit_fps_enabled || vsync_freq < (S32)max_fps))
            {
                fps_color = (fps >= vsync_freq - 1) ? vsync_limit_reached_color : fps_below_limit_color;
            }
        }

        if (current_fps_color != fps_color)
        {
            mFPSText->setColor(fps_color);
            current_fps_color = fps_color;
        }
    }
    // </FS:Ansariel>

    // update clock every 10 seconds
    // <FS:Ansariel> Add seconds to clock
    //if(mClockUpdateTimer.getElapsedTimeF32() > 10.f)
    if(mClockUpdateTimer.getElapsedTimeF32() > 1.f)
    // </FS:Ansariel>
    {
        mClockUpdateTimer.reset();

        // <FS:Zi> FIRE-20390, FIRE-4269 - Option for 12/24 hour clock and seconds display
        // // Get current UTC time, adjusted for the user's clock
        // // being off.
        // time_t utc_time;
        // utc_time = time_corrected();

        // std::string timeStr = getString("time");
        // LLSD substitution;
        // substitution["datetime"] = (S32) utc_time;
        // LLStringUtil::format (timeStr, substitution);
        // mTextTime->setText(timeStr);

        // // set the tooltip to have the date
        // std::string dtStr = getString("timeTooltip");
        // LLStringUtil::format (dtStr, substitution);
        // mTextTime->setToolTip (dtStr);

        updateClockDisplay();
        // </FS:Zi>
    }

    // <FS:Zi> Pathfinding rebake functions
    LLMenuOptionPathfindingRebakeNavmesh& navmesh = LLMenuOptionPathfindingRebakeNavmesh::instance();
    static LLMenuOptionPathfindingRebakeNavmesh::ERebakeNavMeshMode pathfinding_mode = LLMenuOptionPathfindingRebakeNavmesh::kRebakeNavMesh_Default;

    LLViewerRegion* current_region = gAgent.getRegion();
    if (current_region != agent_region)
    {
        agent_region = current_region;
        bakingStarted = false;
        mRebakeStuck = false;
    }
    if (navmesh.isRebaking())
    {
        if (!bakingStarted)
        {
            bakingStarted = true;
            mRebakeStuck = false;
            bakeTimeout = new LORebakeStuck(this);
        }

        if (agent_region &&
            agent_region->dynamicPathfindingEnabled() &&
            mRebakingTimer.getElapsedTimeF32() > 0.5f)
        {
            mRebakingTimer.reset();
            mPathfindingFlashOn = !mPathfindingFlashOn;
            updateParcelIcons();
        }
    }
    else if (pathfinding_mode != navmesh.getMode())
    {
        pathfinding_mode = navmesh.getMode();
        updateParcelIcons();
    }
    // </FS:Zi>

    LLRect r;

    const S32 MENU_RIGHT = gMenuBarView->getRightmostMenuEdge();

    // reshape menu bar to its content's width
    if (MENU_RIGHT != gMenuBarView->getRect().getWidth())
    {
        gMenuBarView->reshape(MENU_RIGHT, gMenuBarView->getRect().getHeight());
    }
    // also update the parcel info panel pos -KC
    if ((MENU_RIGHT + MENU_PARCEL_SPACING) != mParcelInfoPanel->getRect().mLeft)
    {
        updateParcelPanel();
    }

    // <FS:Ansariel> This is done in LLStatusBar::updateNetstatVisibility()
    //mSGBandwidth->setVisible(net_stats_visible);
    //mSGPacketLoss->setVisible(net_stats_visible);

    // It seems this button does no longer exist. I believe this was an overlay button on top of
    // the net stats graphs to call up the lag meter floater. In case someone revives this, make
    // sure to use a mSGStatsButton variable, because this function here is called an awful lot
    // while the viewer runs, and dynamic lookup is very expensive. -Zi
    //mBtnStats->setEnabled(net_stats_visible);

    // update the master volume button state
    bool mute_audio = LLAppViewer::instance()->getMasterSystemAudioMute();
    mBtnVolume->setToggleState(mute_audio);

    LLViewerMedia* media_inst = LLViewerMedia::getInstance();

    // Disable media toggle if there's no media, parcel media, and no parcel audio
    // (or if media is disabled)
    static LLCachedControl<bool> audio_streaming_media(gSavedSettings, "AudioStreamingMedia");
    bool button_enabled = (audio_streaming_media) &&    // ## Zi: Media/Stream separation
                          (media_inst->hasInWorldMedia() || media_inst->hasParcelMedia()    // || media_inst->hasParcelAudio()  // ## Zi: Media/Stream separation
                          );
    mMediaToggle->setEnabled(button_enabled);
    // Note the "sense" of the toggle is opposite whether media is playing or not
    bool any_media_playing = (media_inst->isAnyMediaPlaying() ||
                              media_inst->isParcelMediaPlaying());
    mMediaToggle->setValue(!any_media_playing);

    // <FS:Zi> Media/Stream separation
    static LLCachedControl<bool> audio_streaming_music(gSavedSettings, "AudioStreamingMusic");
    button_enabled = (audio_streaming_music && media_inst->hasParcelAudio());

    mStreamToggle->setEnabled(button_enabled);
    mStreamToggle->setValue(!media_inst->isParcelAudioPlaying());
    // </FS:Zi> Media/Stream separation

    mParcelInfoText->setEnabled(!gRlvHandler.hasBehaviour(RLV_BHVR_SHOWLOC));

    // <FS:Ansariel> Script debug
    if (mNearbyIcons != LLHUDIcon::scriptIconsNearby())
    {
        mNearbyIcons = LLHUDIcon::scriptIconsNearby();
        updateParcelIcons();
    }
    // </FS:Ansariel> Script debug
}

void LLStatusBar::setVisibleForMouselook(bool visible)
{
    mTextTime->setVisible(visible);
    mBalancePanel->setVisible(visible && gSavedSettings.getBOOL("FSShowCurrencyBalanceInStatusbar"));
    mBoxBalance->setVisible(visible);
    // <FS:PP> Option to hide volume controls (sounds, media, stream) in upper right
    // mBtnVolume->setVisible(visible);
    // mStreamToggle->setVisible(visible);      // ## Zi: Media/Stream separation
    // mMediaToggle->setVisible(visible);
    mSearchPanel->setVisible(visible && gSavedSettings.getBOOL("MenuSearch"));
    bool FSEnableVolumeControls = gSavedSettings.getBOOL("FSEnableVolumeControls");
    mBtnVolume->setVisible(visible && FSEnableVolumeControls);
    mStreamToggle->setVisible(visible && FSEnableVolumeControls); // ## Zi: Media/Stream separation
    mMediaToggle->setVisible(visible && FSEnableVolumeControls);
    // </FS:PP>
    bool showNetStats = gSavedSettings.getBOOL("ShowNetStats");
    mSGBandwidth->setVisible(visible && showNetStats);
    mSGPacketLoss->setVisible(visible && showNetStats);
    mBandwidthButton->setVisible(visible && showNetStats); // <FS:PP> FIRE-6287: Clicking on traffic indicator toggles Lag Meter window
    mTimeMediaPanel->setVisible(visible);
    setBackgroundVisible(visible);
    mIconPresetsCamera->setVisible(visible);
    mIconPresetsGraphic->setVisible(visible);
}

void LLStatusBar::debitBalance(S32 debit)
{
    setBalance(getBalance() - debit);
}

void LLStatusBar::creditBalance(S32 credit)
{
    setBalance(getBalance() + credit);
}

void LLStatusBar::setBalance(S32 balance)
{
    if (balance > getBalance() && getBalance() != 0)
    {
        LLFirstUse::receiveLindens();
    }

    std::string money_str = LLResMgr::getInstance()->getMonetaryString( balance );

    LLStringUtil::format_map_t string_args;
    string_args["[AMT]"] = llformat("%s", money_str.c_str());
    std::string label_str = getString("buycurrencylabel", string_args);
    mBoxBalance->setValue(label_str);

    updateBalancePanelPosition();

    // If the search panel is shown, move this according to the new balance width. Parcel text will reshape itself in setParcelInfoText
    if (mSearchPanel && mSearchPanel->getVisible())
    {
        updateMenuSearchPosition();
    }

    if (mBalance && (fabs((F32)(mBalance - balance)) > gSavedSettings.getF32("UISndMoneyChangeThreshold")))
    {
        if (mBalance > balance)
            make_ui_sound("UISndMoneyChangeDown");
        else
            make_ui_sound("UISndMoneyChangeUp");
    }

    if( balance != mBalance )
    {
        mBalanceTimer->reset();
        mBalanceTimer->setTimerExpirySec( ICON_TIMER_EXPIRY );
        mBalance = balance;
    }
}


// static
void LLStatusBar::sendMoneyBalanceRequest()
{
    LLMessageSystem* msg = gMessageSystem;
    msg->newMessageFast(_PREHASH_MoneyBalanceRequest);
    msg->nextBlockFast(_PREHASH_AgentData);
    msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
    msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
    msg->nextBlockFast(_PREHASH_MoneyData);
    msg->addUUIDFast(_PREHASH_TransactionID, LLUUID::null );

    if (gDisconnected)
    {
        LL_DEBUGS() << "Trying to send message when disconnected, skipping balance request!" << LL_ENDL;
        return;
    }
    if (!gAgent.getRegion())
    {
        LL_DEBUGS() << "LLAgent::sendReliableMessage No region for agent yet, skipping balance request!" << LL_ENDL;
        return;
    }
    // Double amount of retries due to this request initially happening during busy stage
    // Ideally this should be turned into a capability
    gMessageSystem->sendReliable(gAgent.getRegionHost(), LL_DEFAULT_RELIABLE_RETRIES * 2, true, LL_PING_BASED_TIMEOUT_DUMMY, NULL, NULL);
}


void LLStatusBar::setHealth(S32 health)
{
    //LL_INFOS() << "Setting health to: " << buffer << LL_ENDL;
    if( mHealth > health )
    {
        if (mHealth > (health + gSavedSettings.getF32("UISndHealthReductionThreshold")))
        {
            if (isAgentAvatarValid())
            {
                if (gAgentAvatarp->getSex() == SEX_FEMALE)
                {
                    make_ui_sound("UISndHealthReductionF");
                }
                else
                {
                    make_ui_sound("UISndHealthReductionM");
                }
            }
        }

        mHealthTimer->reset();
        mHealthTimer->setTimerExpirySec( ICON_TIMER_EXPIRY );
    }

    mHealth = health;
}

S32 LLStatusBar::getBalance() const
{
    return mBalance;
}


S32 LLStatusBar::getHealth() const
{
    return mHealth;
}

void LLStatusBar::setLandCredit(S32 credit)
{
    mSquareMetersCredit = credit;
}
void LLStatusBar::setLandCommitted(S32 committed)
{
    mSquareMetersCommitted = committed;
}

bool LLStatusBar::isUserTiered() const
{
    return (mSquareMetersCredit > 0);
}

S32 LLStatusBar::getSquareMetersCredit() const
{
    return mSquareMetersCredit;
}

S32 LLStatusBar::getSquareMetersCommitted() const
{
    return mSquareMetersCommitted;
}

S32 LLStatusBar::getSquareMetersLeft() const
{
    return mSquareMetersCredit - mSquareMetersCommitted;
}

void LLStatusBar::onClickBuyCurrency()
{
    // open a currency floater - actual one open depends on
    // value specified in settings.xml
    LLBuyCurrencyHTML::openCurrencyFloater();
    LLFirstUse::receiveLindens(false);
}

void LLStatusBar::onMouseEnterPresetsCamera()
{
    LLView* popup_holder = gViewerWindow->getRootView()->getChildView("popup_holder");
    // <FS:Ansariel> Changed presets icon to LLButton
    //LLIconCtrl* icon =  getChild<LLIconCtrl>( "presets_icon_camera" );
    //LLRect icon_rect = icon->getRect();
    LLRect icon_rect = mIconPresetsCamera->getRect();
    // </FS:Ansariel>
    LLRect pulldown_rect = mPanelPresetsCameraPulldown->getRect();
    pulldown_rect.setLeftTopAndSize(icon_rect.mLeft -
         (pulldown_rect.getWidth() - icon_rect.getWidth()),
                   icon_rect.mBottom,
                   pulldown_rect.getWidth(),
                   pulldown_rect.getHeight());

    pulldown_rect.translate(popup_holder->getRect().getWidth() - pulldown_rect.mRight, 0);
    mPanelPresetsCameraPulldown->setShape(pulldown_rect);

    // show the master presets pull-down
    LLUI::getInstance()->clearPopups();
    LLUI::getInstance()->addPopup(mPanelPresetsCameraPulldown);
    mPanelNearByMedia->setVisible(false);
    mPanelVolumePulldown->setVisible(false);
    mPanelPresetsPulldown->setVisible(false);
    mPanelPresetsCameraPulldown->setVisible(true);
}

void LLStatusBar::onMouseEnterPresets()
{
    LLView* popup_holder = gViewerWindow->getRootView()->getChildView("popup_holder");
    // <FS:Ansariel> Changed presets icon to LLButton
    //LLIconCtrl* icon =  getChild<LLIconCtrl>( "presets_icon_graphic" );
    //LLRect icon_rect = icon->getRect();
    LLRect icon_rect = mIconPresetsGraphic->getRect();
    // </FS:Ansariel>
    LLRect pulldown_rect = mPanelPresetsPulldown->getRect();
    pulldown_rect.setLeftTopAndSize(icon_rect.mLeft -
         (pulldown_rect.getWidth() - icon_rect.getWidth()),
                   icon_rect.mBottom,
                   pulldown_rect.getWidth(),
                   pulldown_rect.getHeight());

    pulldown_rect.translate(popup_holder->getRect().getWidth() - pulldown_rect.mRight, 0);
    mPanelPresetsPulldown->setShape(pulldown_rect);

    // show the master presets pull-down
    LLUI::getInstance()->clearPopups();
    LLUI::getInstance()->addPopup(mPanelPresetsPulldown);
    mPanelNearByMedia->setVisible(false);
    mPanelVolumePulldown->setVisible(false);
    mPanelPresetsPulldown->setVisible(true);
}

void LLStatusBar::onMouseEnterVolume()
{
    LLView* popup_holder = gViewerWindow->getRootView()->getChildView("popup_holder");
    LLButton* volbtn =  getChild<LLButton>( "volume_btn" );
    LLRect vol_btn_rect = volbtn->getRect();
    LLPanel* media_panel = getChild<LLPanel>("time_and_media_bg");
    LLRect media_panel_rect = media_panel->getRect();
    LLRect volume_pulldown_rect = mPanelVolumePulldown->getRect();
    volume_pulldown_rect.setLeftTopAndSize(
        (vol_btn_rect.mLeft + media_panel_rect.mLeft) -
        (volume_pulldown_rect.getWidth() - vol_btn_rect.getWidth()),
                   vol_btn_rect.mBottom,
                   volume_pulldown_rect.getWidth(),
                   volume_pulldown_rect.getHeight());

    volume_pulldown_rect.translate(popup_holder->getRect().getWidth() - volume_pulldown_rect.mRight, 0);
    mPanelVolumePulldown->setShape(volume_pulldown_rect);


    // show the master volume pull-down
    LLUI::getInstance()->clearPopups();
    LLUI::getInstance()->addPopup(mPanelVolumePulldown);
    mPanelPresetsCameraPulldown->setVisible(false);
    mPanelPresetsPulldown->setVisible(false);
    mPanelNearByMedia->setVisible(false);
    mPanelVolumePulldown->setVisible(true);
}

void LLStatusBar::onMouseEnterNearbyMedia()
{
    LLView* popup_holder = gViewerWindow->getRootView()->getChildView("popup_holder");
    LLRect nearby_media_rect = mPanelNearByMedia->getRect();
    LLButton* nearby_media_btn =  getChild<LLButton>( "media_toggle_btn" );
    LLRect nearby_media_btn_rect = nearby_media_btn->getRect();
    nearby_media_rect.setLeftTopAndSize(nearby_media_btn_rect.mLeft -
                                        (nearby_media_rect.getWidth() - nearby_media_btn_rect.getWidth())/2,
                                        nearby_media_btn_rect.mBottom,
                                        nearby_media_rect.getWidth(),
                                        nearby_media_rect.getHeight());
    // force onscreen
    nearby_media_rect.translate(popup_holder->getRect().getWidth() - nearby_media_rect.mRight, 0);

    // show the master volume pull-down
    mPanelNearByMedia->setShape(nearby_media_rect);
    LLUI::getInstance()->clearPopups();
    LLUI::getInstance()->addPopup(mPanelNearByMedia);

    mPanelPresetsCameraPulldown->setVisible(false);
    mPanelPresetsPulldown->setVisible(false);
    mPanelVolumePulldown->setVisible(false);
    mPanelNearByMedia->setVisible(true);
}


// <FS:Ansariel> Open popup panels on click if FSStatusBarMenuButtonPopupOnRollover is disabled
//static void onClickVolume(void* data)
void LLStatusBar::onClickVolume(void* data)
// </FS:Ansariel>
{
    // <FS:Ansariel> Open popup panels on click if FSStatusBarMenuButtonPopupOnRollover is disabled
    if (gSavedSettings.getBOOL("FSStatusBarMenuButtonPopupOnRollover"))
    {
    // </FS:Ansariel>
    // toggle the master mute setting
    bool mute_audio = LLAppViewer::instance()->getMasterSystemAudioMute();
    LLAppViewer::instance()->setMasterSystemAudioMute(!mute_audio);
    // <FS:Ansariel> Open popup panels on click if FSStatusBarMenuButtonPopupOnRollover is disabled
    }
    else
    {
        ((LLStatusBar*)data)->onMouseEnterVolume();
    }
    // </FS:Ansariel>
}

//static
void LLStatusBar::onClickBalance(void* )
{
    // Force a balance request message:
    LLStatusBar::sendMoneyBalanceRequest();
    // The refresh of the display (call to setBalance()) will be done by process_money_balance_reply()
}

//static
void LLStatusBar::onClickMediaToggle(void* data)
{
    LLStatusBar *status_bar = (LLStatusBar*)data;
    // "Selected" means it was showing the "play" icon (so media was playing), and now it shows "pause", so turn off media
// <FS:Zi> Split up handling here to allow external controls to switch media on/off
//  bool pause = status_bar->mMediaToggle->getValue();
//  LLViewerMedia::getInstance()->setAllMediaPaused(pause);
// }
    bool enable = ! status_bar->mMediaToggle->getValue();
    // <FS:Ansariel> Open popup panels on click if FSStatusBarMenuButtonPopupOnRollover is disabled
    if (gSavedSettings.getBOOL("FSStatusBarMenuButtonPopupOnRollover"))
    {
    // </FS:Ansariel>
    status_bar->toggleMedia(enable);
    // <FS:Ansariel> Open popup panels on click if FSStatusBarMenuButtonPopupOnRollover is disabled
    }
    else
    {
        status_bar->onMouseEnterNearbyMedia();
    }
    // </FS:Ansariel>
}

void LLStatusBar::toggleMedia(bool enable)
{
// </FS:Zi>
    LLViewerMedia::getInstance()->setAllMediaEnabled(enable);
}

// <FS:Zi> Media/Stream separation
// static
void LLStatusBar::onClickStreamToggle(void* data)
{
    if (!gAudiop)
        return;

    LLStatusBar *status_bar = (LLStatusBar*)data;
    bool enable = ! status_bar->mStreamToggle->getValue();
    status_bar->toggleStream(enable);
}

void LLStatusBar::toggleStream(bool enable)
{
    if (!gAudiop)
    {
        return;
    }

    if(enable)
    {
        if (LLAudioEngine::AUDIO_PAUSED == gAudiop->isInternetStreamPlaying())
        {
            // 'false' means unpause
            LLViewerAudio::getInstance()->startInternetStreamWithAutoFade(LLViewerMedia::getInstance()->getParcelAudioURL());
        }
        else
        {
            if (gSavedSettings.getBOOL("MediaEnableFilter"))
            {
                LLViewerParcelMedia::getInstance()->filterAudioUrl(LLViewerMedia::getInstance()->getParcelAudioURL());
            }
            else
            {
                LLViewerAudio::getInstance()->startInternetStreamWithAutoFade(LLViewerMedia::getInstance()->getParcelAudioURL());
            }
        }
    }
    else
    {
        LLViewerAudio::getInstance()->stopInternetStreamWithAutoFade();
    }

    mAudioStreamEnabled = enable;
}

bool LLStatusBar::getAudioStreamEnabled() const
{
    return mAudioStreamEnabled;
}
// </FS:Zi> Media/Stream separation

bool can_afford_transaction(S32 cost)
{
    return((cost <= 0)||((gStatusBar) && (gStatusBar->getBalance() >=cost)));
}

void LLStatusBar::onVolumeChanged(const LLSD& newvalue)
{
    refresh();
}

// <FS:Ansariel> Fix LL voice disabled on 2nd instance nonsense
//void LLStatusBar::onVoiceChanged(const LLSD& newvalue)
//{
//    if (newvalue.asBoolean())
//    {
//        // Second instance starts with "VoiceMute_Off" icon, fix it
//        mBtnVolume->setImageUnselected(LLUI::getUIImage("Audio_Off"));
//    }
//    refresh();
//}
// </FS:Ansariel>

void LLStatusBar::onUpdateFilterTerm()
{
    LLWString searchValue = utf8str_to_wstring( mFilterEdit->getValue() );
    LLWStringUtil::toLower( searchValue );

    if( !mSearchData || mSearchData->mLastFilter == searchValue )
        return;

    mSearchData->mLastFilter = searchValue;

    mSearchData->mRootMenu->hightlightAndHide( searchValue );
    gMenuBarView->needsArrange();
}

void collectChildren( LLMenuGL *aMenu, ll::statusbar::SearchableItemPtr aParentMenu )
{
    for( U32 i = 0; i < aMenu->getItemCount(); ++i )
    {
        LLMenuItemGL *pMenu = aMenu->getItem( i );

        ll::statusbar::SearchableItemPtr pItem( new ll::statusbar::SearchableItem );
        pItem->mCtrl = pMenu;
        pItem->mMenu = pMenu;
        pItem->mLabel = utf8str_to_wstring( pMenu->ll::ui::SearchableControl::getSearchText() );
        LLWStringUtil::toLower( pItem->mLabel );
        aParentMenu->mChildren.push_back( pItem );

        LLMenuItemBranchGL *pBranch = dynamic_cast< LLMenuItemBranchGL* >( pMenu );
        if( pBranch )
            collectChildren( pBranch->getBranch(), pItem );
    }

}

void LLStatusBar::collectSearchableItems()
{
    mSearchData.reset( new ll::statusbar::SearchData );
    ll::statusbar::SearchableItemPtr pItem( new ll::statusbar::SearchableItem );
    mSearchData->mRootMenu = pItem;
    collectChildren( gMenuBarView, pItem );
}

void LLStatusBar::updateMenuSearchVisibility(const LLSD& data)
{
    bool visible = data.asBoolean();
    mSearchPanel->setVisible(visible);
    if (!visible)
    {
        mFilterEdit->setText(LLStringUtil::null);
        onUpdateFilterTerm();
    }
    else
    {
        updateMenuSearchPosition();
    }
}

void LLStatusBar::updateMenuSearchPosition()
{
    const S32 HPAD = 12;
    LLRect balanceRect = getChildView("balance_bg")->getRect();
    LLRect searchRect = mSearchPanel->getRect();
    S32 w = searchRect.getWidth();
    searchRect.mLeft = balanceRect.mLeft - w - HPAD;
    searchRect.mRight = searchRect.mLeft + w;
    mSearchPanel->setShape( searchRect );
}

void LLStatusBar::updateBalancePanelPosition()
{
    // <COLOSI opensim currency support>
    // Unclear if call to getTextBoundingRect updates text but assuming it calls length()
    // when getting the bounding box which will update the text and get the length of the
    // wrapped (Tea::wrapCurrency) text (see lluistring).  If not, and currency symbols
    // that are not two characters have the wrong size bounding rect, then the correct
    // place to fix this is in the getTextBoundingRect() function, not here.
    // buy_rect below should be properly set to dirty() when we modify the currency and
    // should also be updated and wrapped before width is determined.
    // </COLOSI opensim currency support>

    // Resize the L$ balance background to be wide enough for your balance plus the buy button
    const S32 HPAD = 24;
    LLRect balance_rect = mBoxBalance->getTextBoundingRect();
    LLRect buy_rect = getChildView("buyL")->getRect();
    // <FS:Ansariel> Not used in Firestorm
    //LLRect shop_rect = getChildView("goShop")->getRect();
    LLView* balance_bg_view = getChildView("balance_bg");
    LLRect balance_bg_rect = balance_bg_view->getRect();
    // <FS:Ansariel> Not used in Firestorm
    //balance_bg_rect.mLeft = balance_bg_rect.mRight - (buy_rect.getWidth() + shop_rect.getWidth() + balance_rect.getWidth() + HPAD);
    balance_bg_rect.mLeft = balance_bg_rect.mRight - (buy_rect.getWidth() + balance_rect.getWidth() + HPAD);
    // </FS:Ansariel>
    balance_bg_view->setShape(balance_bg_rect);
}

//////////////////////////////////////////////////////////////////////////////
// Firestorm methods

void LLStatusBar::showBalance(bool show)
{
    mBoxBalance->setVisible(show);
}

// <COLOSI opensim multi-currency support>
void LLStatusBar::updateCurrencySymbols()
{
    // Update "Buy L$" button because it is only evaluated once when panel is loaded
    LLButton* buyButton = findChild<LLButton>("buyL");
    if (buyButton)
    {
        buyButton->updateCurrencySymbols();
    }
    // Should not need to update the balance display because it is updated frequently.
    // If it does require an update, do so via a balance update request so we don't
    // switch the symbol without updating the balance.
}
// </COLOSI opensim multi-currency support>

void LLStatusBar::onBandwidthGraphButtonClicked()
{
    if (gSavedSettings.getBOOL("FSUseStatsInsteadOfLagMeter"))
    {
        LLFloaterReg::toggleInstance("stats");
    }
    else
    {
        LLFloaterReg::toggleInstance("lagmeter");
    }
}

void LLStatusBar::initParcelIcons()
{
    mParcelIcon[VOICE_ICON] = getChild<LLIconCtrl>("voice_icon");
    mParcelIcon[FLY_ICON] = getChild<LLIconCtrl>("fly_icon");
    mParcelIcon[PUSH_ICON] = getChild<LLIconCtrl>("push_icon");
    mParcelIcon[BUILD_ICON] = getChild<LLIconCtrl>("build_icon");
    mParcelIcon[SCRIPTS_ICON] = getChild<LLIconCtrl>("scripts_icon");
    mParcelIcon[DAMAGE_ICON] = getChild<LLIconCtrl>("damage_icon");
    mParcelIcon[SEE_AVATARS_ICON] = getChild<LLIconCtrl>("see_avs_icon");
    mParcelIcon[PATHFINDING_DIRTY_ICON] = getChild<LLIconCtrl>("pathfinding_dirty_icon");
    mParcelIcon[PATHFINDING_DISABLED_ICON] = getChild<LLIconCtrl>("pathfinding_disabled_icon");

    mParcelIcon[VOICE_ICON]->setMouseDownCallback(boost::bind(&LLStatusBar::onParcelIconClick, this, VOICE_ICON));
    mParcelIcon[FLY_ICON]->setMouseDownCallback(boost::bind(&LLStatusBar::onParcelIconClick, this, FLY_ICON));
    mParcelIcon[PUSH_ICON]->setMouseDownCallback(boost::bind(&LLStatusBar::onParcelIconClick, this, PUSH_ICON));
    mParcelIcon[BUILD_ICON]->setMouseDownCallback(boost::bind(&LLStatusBar::onParcelIconClick, this, BUILD_ICON));
    mParcelIcon[SCRIPTS_ICON]->setMouseDownCallback(boost::bind(&LLStatusBar::onParcelIconClick, this, SCRIPTS_ICON));
    mParcelIcon[DAMAGE_ICON]->setMouseDownCallback(boost::bind(&LLStatusBar::onParcelIconClick, this, DAMAGE_ICON));
    mParcelIcon[SEE_AVATARS_ICON]->setMouseDownCallback(boost::bind(&LLStatusBar::onParcelIconClick, this, SEE_AVATARS_ICON));
    mParcelIcon[PATHFINDING_DIRTY_ICON]->setMouseDownCallback(boost::bind(&LLStatusBar::onParcelIconClick, this, PATHFINDING_DIRTY_ICON));
    mParcelIcon[PATHFINDING_DISABLED_ICON]->setMouseDownCallback(boost::bind(&LLStatusBar::onParcelIconClick, this, PATHFINDING_DISABLED_ICON));

    mDamageText->setText(LLStringExplicit("100%"));
}

void LLStatusBar::handleLoginComplete()
{
    // An agent parcel update hasn't occurred yet, so
    // we have to manually set location and the icons.
    update();
}

void LLStatusBar::onNavBarShowParcelPropertiesCtrlChanged()
{
    mShowParcelIcons = gSavedSettings.getBOOL("NavBarShowParcelProperties");
    mCurrentLocationString = ""; // Need to do this to force an update of the text and panel width calculation as result of calling update()
    update();
}

void LLStatusBar::onNavBarShowCoordinatesCtrlChanged()
{
    std::string new_text;

    // don't need to have separate show_coords variable; if user requested the coords to be shown
    // they will be added during the next call to the draw() method.
    buildLocationString(new_text, false);
    setParcelInfoText(new_text);
}

void LLStatusBar::buildLocationString(std::string& loc_str, bool show_coords)
{
    LLAgentUI::ELocationFormat format =
        (show_coords ? LLAgentUI::LOCATION_FORMAT_V1 : LLAgentUI::LOCATION_FORMAT_V1_NO_COORDS);

    if (!LLAgentUI::buildLocationString(loc_str, format))
    {
        loc_str = "???";
    }
}

void LLStatusBar::setParcelInfoText(const std::string& new_text)
{
    const S32 ParcelInfoSpacing = 5;
    const LLFontGL* font = mParcelInfoText->getFont();
    S32 new_text_width = font->getWidth(new_text);

    if (new_text != mCurrentLocationString)
    {
        mParcelInfoText->setText(new_text);
        mCurrentLocationString = new_text;
    }

    LLRect rect = mParcelInfoText->getRect();
    rect.setOriginAndSize(rect.mLeft, rect.mBottom, new_text_width, rect.getHeight());

    // Recalculate panel size so we are able to click the whole text
    LLRect panelParcelInfoRect = mParcelInfoPanel->getRect();
    LLRect panelBalanceRect = mBalancePanel->getRect();

    // The menu search editor is left from the balance rect. If it is shown, use that rect
    if (mSearchPanel && mSearchPanel->getVisible())
        panelBalanceRect = mSearchPanel->getRect();

    panelParcelInfoRect.mRight = panelParcelInfoRect.mLeft + rect.mRight;
    S32 borderRight = panelBalanceRect.mLeft - ParcelInfoSpacing;

    // If we're about to float away under the L$ balance, cut off
    // the parcel description so it doesn't happen.
    if (panelParcelInfoRect.mRight > borderRight)
    {
        panelParcelInfoRect.mRight = borderRight;
        rect.mRight = panelParcelInfoRect.getWidth();
    }

    mParcelInfoText->reshape(rect.getWidth(), rect.getHeight(), true);
    mParcelInfoText->setRect(rect);
    mParcelInfoPanel->setRect(panelParcelInfoRect);
}

void LLStatusBar::update()
{
    std::string new_text;

    updateParcelIcons();

    // don't need to have separate show_coords variable; if user requested the coords to be shown
    // they will be added during the next call to the draw() method.
    buildLocationString(new_text, false);
    setParcelInfoText(new_text);

    updateParcelPanel();
}

void LLStatusBar::updateParcelPanel()
{
    const S32 MENU_RIGHT = gMenuBarView->getRightmostMenuEdge();
    S32 left = MENU_RIGHT + MENU_PARCEL_SPACING;
    LLRect rect = mParcelInfoPanel->getRect();
    rect.mRight = left + rect.getWidth();
    rect.mLeft = left;

    mParcelInfoPanel->setRect(rect);
}

void LLStatusBar::updateParcelInfoText()
{
    static LLUICachedControl<bool> show_coords("NavBarShowCoordinates", false);
    std::string new_text;

    buildLocationString(new_text, show_coords);
    setParcelInfoText(new_text);
}

void LLStatusBar::updateParcelIcons()
{
    LLViewerParcelMgr* vpm = LLViewerParcelMgr::getInstance();

    LLViewerRegion* agent_region = gAgent.getRegion();
    LLParcel* agent_parcel = vpm->getAgentParcel();
    if (!agent_region || !agent_parcel)
        return;

    if (mShowParcelIcons)
    {
        LLParcel* current_parcel;
        LLViewerRegion* selection_region = vpm->getSelectionRegion();
        LLParcel* selected_parcel = vpm->getParcelSelection()->getParcel();

        // If agent is in selected parcel we use its properties because
        // they are updated more often by LLViewerParcelMgr than agent parcel properties.
        // See LLViewerParcelMgr::processParcelProperties().
        // This is needed to reflect parcel restrictions changes without having to leave
        // the parcel and then enter it again. See EXT-2987
        if (selected_parcel && selected_parcel->getLocalID() == agent_parcel->getLocalID()
                && selection_region == agent_region)
        {
            current_parcel = selected_parcel;
        }
        else
        {
            current_parcel = agent_parcel;
        }

        bool is_opensim = false;
#ifdef OPENSIM
        is_opensim = LLGridManager::getInstance()->isInOpenSim();
#endif // OPENSIM
        bool allow_voice    = vpm->allowAgentVoice(agent_region, current_parcel);
        bool allow_fly      = vpm->allowAgentFly(agent_region, current_parcel);
        bool allow_push     = vpm->allowAgentPush(agent_region, current_parcel);
        bool allow_build    = vpm->allowAgentBuild(current_parcel); // true when anyone is allowed to build. See EXT-4610.
        bool allow_scripts  = vpm->allowAgentScripts(agent_region, current_parcel);
        bool allow_damage   = vpm->allowAgentDamage(agent_region, current_parcel);
        bool see_avatars    = current_parcel->getSeeAVs();
        bool is_for_sale    = (!current_parcel->isPublic() && vpm->canAgentBuyParcel(current_parcel, false));
        bool pathfinding_dynamic_enabled = agent_region->dynamicPathfindingEnabled();

        LLMenuOptionPathfindingRebakeNavmesh& navmesh = LLMenuOptionPathfindingRebakeNavmesh::instance();
        bool pathfinding_navmesh_dirty = navmesh.isRebakeNeeded();
        F32 pathfinding_dirty_icon_alpha = 1.0f;
        if (navmesh.isRebaking())
        {
            // Stop the blinking after a while
            if (mRebakeStuck)
            {
                pathfinding_dirty_icon_alpha = 0.5;
            }
            else
            {
                pathfinding_dirty_icon_alpha = mPathfindingFlashOn ? 1.0f : 0.25f;
            }
            pathfinding_navmesh_dirty = true;
        }

        // Most icons are "block this ability"
        mParcelIcon[VOICE_ICON]->setVisible(   !allow_voice );
        mParcelIcon[FLY_ICON]->setVisible(     !allow_fly );
        mParcelIcon[PUSH_ICON]->setVisible(    !allow_push );
        mParcelIcon[BUILD_ICON]->setVisible(   !allow_build );
        mParcelIcon[SCRIPTS_ICON]->setVisible( !allow_scripts );
        mParcelIcon[DAMAGE_ICON]->setVisible(  allow_damage );
        mParcelIcon[SEE_AVATARS_ICON]->setVisible(!see_avatars);
        mParcelIcon[PATHFINDING_DIRTY_ICON]->setVisible(pathfinding_navmesh_dirty);
        mParcelIcon[PATHFINDING_DIRTY_ICON]->setColor(LLColor4::white % pathfinding_dirty_icon_alpha);
        mParcelIcon[PATHFINDING_DISABLED_ICON]->setVisible(!pathfinding_navmesh_dirty && !pathfinding_dynamic_enabled && !is_opensim);
        mDamageText->setVisible(allow_damage);
        mBuyParcelBtn->setVisible(is_for_sale);
        mScriptOut->setVisible(LLHUDIcon::scriptIconsNearby());
    }
    else
    {
        for (S32 i = 0; i < ICON_COUNT; ++i)
        {
            mParcelIcon[i]->setVisible(false);
        }
        mDamageText->setVisible(false);
        mBuyParcelBtn->setVisible(false);
        mScriptOut->setVisible(false);
    }

    layoutParcelIcons();
}

void LLStatusBar::updateHealth()
{
    // *FIXME: Status bar owns health information, should be in agent
    if (mShowParcelIcons && gStatusBar)
    {
        static S32 last_health = -1;
        S32 health = gStatusBar->getHealth();
        if (health != last_health)
        {
            std::string text = llformat("%d%%", health);
            mDamageText->setText(text);
            last_health = health;
        }
    }
}

void LLStatusBar::layoutParcelIcons()
{
    // TODO: remove hard-coded values and read them as xml parameters
    static const S32 FIRST_ICON_HPAD = 2; // 2 padding; See also ICON_HEAD

    S32 left = FIRST_ICON_HPAD;

    left = layoutWidget(mScriptOut, left);
    left = layoutWidget(mDamageText, left);

    for (S32 i = ICON_COUNT - 1; i >= 0; --i)
    {
        left = layoutWidget(mParcelIcon[i], left);
    }
    left = layoutWidget(mBuyParcelBtn, left);

    LLRect infoTextRect = mParcelInfoText->getRect();
    infoTextRect.mLeft = left;
    mParcelInfoText->setRect(infoTextRect);
}

S32 LLStatusBar::layoutWidget(LLUICtrl* ctrl, S32 left)
{
    // TODO: remove hard-coded values and read them as xml parameters
    static const S32 ICON_HPAD = 2;

    if (ctrl->getVisible())
    {
        LLRect rect = ctrl->getRect();
        rect.mRight = left + rect.getWidth();
        rect.mLeft = left;

        ctrl->setRect(rect);
        left += rect.getWidth() + ICON_HPAD;
    }

    return left;
}

void LLStatusBar::onParcelIconClick(EParcelIcon icon)
{
    switch (icon)
    {
    case VOICE_ICON:
        LLNotificationsUtil::add("NoVoice");
        break;
    case FLY_ICON:
        LLNotificationsUtil::add("NoFly");
        break;
    case PUSH_ICON:
        LLNotificationsUtil::add("PushRestricted");
        break;
    case BUILD_ICON:
        LLNotificationsUtil::add("NoBuild");
        break;
    case SCRIPTS_ICON:
    {
        LLViewerRegion* region = gAgent.getRegion();
        if(region && region->getRegionFlags() & REGION_FLAGS_ESTATE_SKIP_SCRIPTS)
        {
            LLNotificationsUtil::add("ScriptsStopped");
        }
        else if(region && region->getRegionFlags() & REGION_FLAGS_SKIP_SCRIPTS)
        {
            LLNotificationsUtil::add("ScriptsNotRunning");
        }
        else
        {
            LLNotificationsUtil::add("NoOutsideScripts");
        }
        break;
    }
    case DAMAGE_ICON:
        LLNotificationsUtil::add("NotSafe");
        break;
    case SEE_AVATARS_ICON:
        LLNotificationsUtil::add("SeeAvatars");
        break;
    case PATHFINDING_DIRTY_ICON:
        LLNotificationsUtil::add("PathfindingDirty", LLSD(), LLSD(), boost::bind(&LLStatusBar::rebakeRegionCallback, this, _1, _2));
        break;
    case PATHFINDING_DISABLED_ICON:
        LLNotificationsUtil::add("DynamicPathfindingDisabled");
        break;
    case ICON_COUNT:
        break;
    // no default to get compiler warning when a new icon gets added
    }
}

void LLStatusBar::onAgentParcelChange()
{
    update();
}

void LLStatusBar::onContextMenuItemClicked(const LLSD::String& item)
{
    if (item == "landmark")
    {
        if (!gRlvHandler.hasBehaviour(RLV_BHVR_SHOWLOC))
        {
            LLViewerInventoryItem* landmark = LLLandmarkActions::findLandmarkForAgentPos();

            if(landmark == NULL)
            {
                LLFloaterSidePanelContainer::showPanel("panel_places", LLSD().with("type", "create_landmark"));
            }
            else
            {
                LLFloaterSidePanelContainer::showPanel("panel_places",
                        LLSD().with("type", "landmark").with("id",landmark->getUUID()));
            }
        }
    }
    else if (item == "copy")
    {
        LLSLURL slurl;
        LLAgentUI::buildSLURL(slurl, false);
        LLUIString location_str(slurl.getSLURLString());

        LLClipboard::instance().copyToClipboard( location_str, 0, location_str.length() );
    }
}

void LLStatusBar::onInfoButtonClicked()
{
    if (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWLOC))
    {
        return;
    }
    LLFloaterReg::showInstance("about_land");
}

void LLStatusBar::onBuyLandClicked()
{
    if (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWLOC))
    {
        return;
    }
    handle_buy_land();
}

void LLStatusBar::setBackgroundColor( const LLColor4& color )
{
    LLPanel::setBackgroundColor(color);
    mBalancePanel->setBackgroundColor(color);
    mTimeMediaPanel->setBackgroundColor(color);
}

void LLStatusBar::updateNetstatVisibility(const LLSD& data)
{
    const S32 NETSTAT_WIDTH = (SIM_STAT_WIDTH + 2) * 2;
    bool showNetStat = data.asBoolean();
    S32 translateFactor = (showNetStat ? -1 : 1);

    mSGBandwidth->setVisible(showNetStat);
    mSGPacketLoss->setVisible(showNetStat);
    mBandwidthButton->setVisible(showNetStat);

    LLRect rect = mTimeMediaPanel->getRect();
    rect.translate(NETSTAT_WIDTH * translateFactor, 0);
    mTimeMediaPanel->setRect(rect);

    rect = mBalancePanel->getRect();
    rect.translate(NETSTAT_WIDTH * translateFactor, 0);
    mBalancePanel->setRect(rect);

    updateMenuSearchPosition();
    update();
}

void LLStatusBar::updateVolumeControlsVisibility(const LLSD& data)
{
    const S32 cVolumeIconsWidth = mVolumeIconsWidth;
    bool showVolumeControls = data.asBoolean();
    S32 translateFactor = (showVolumeControls ? -1 : 1);

    mBtnVolume->setVisible(showVolumeControls);
    mStreamToggle->setVisible(showVolumeControls);
    mMediaToggle->setVisible(showVolumeControls);

    LLRect rect = mTimeMediaPanel->getRect();
    rect.translate(cVolumeIconsWidth * translateFactor, 0);
    rect.mRight -= cVolumeIconsWidth * translateFactor;
    mTimeMediaPanel->setRect(rect);

    mFPSText->translate(-(cVolumeIconsWidth * translateFactor), 0);

    rect = mBalancePanel->getRect();
    rect.translate(cVolumeIconsWidth * translateFactor, 0);
    mBalancePanel->setRect(rect);

    updateMenuSearchPosition();
    update();
}

void LLStatusBar::onShowFPSChanged(const LLSD& newvalue)
{
    const S32 text_width = mFPSText->getRect().getWidth() + 4; // left_pad = 4
    bool show_fps = newvalue.asBoolean();
    S32 translateFactor = (show_fps ? -1 : 1);

    mFPSText->setVisible(show_fps);

    LLRect rect = mTimeMediaPanel->getRect();
    rect.translate(text_width * translateFactor, 0);
    rect.mRight -= text_width * translateFactor;
    mTimeMediaPanel->setRect(rect);

    rect = mBalancePanel->getRect();
    rect.translate(text_width * translateFactor, 0);
    mBalancePanel->setRect(rect);

    updateMenuSearchPosition();
    update();
}

bool LLStatusBar::rebakeRegionCallback(const LLSD& notification, const LLSD& response)
{
    S32 option = LLNotificationsUtil::getSelectedOption(notification, response);

    if (option == 0)
    {
        if (LLMenuOptionPathfindingRebakeNavmesh::getInstance()->isRebakeNeeded())
        {
            LLMenuOptionPathfindingRebakeNavmesh::getInstance()->rebakeNavmesh();
        }
        return true;
    }
    return false;
}

void LLStatusBar::onMouseEnterParcelInfo()
{
    mParcelInfoText->setColor(LLUIColorTable::instance().getColor("ParcelHoverColor"));
}

void LLStatusBar::onMouseLeaveParcelInfo()
{
    mParcelInfoText->setColor(LLUIColorTable::instance().getColor("ParcelNormalColor"));
}

void LLStatusBar::onPopupRolloverChanged(const LLSD& newvalue)
{
    bool new_value = newvalue.asBoolean();

    if (mMouseEnterPresetsConnection.connected())
    {
        mMouseEnterPresetsConnection.disconnect();
    }
    if (mMouseEnterPresetsCameraConnection.connected())
    {
        mMouseEnterPresetsCameraConnection.disconnect();
    }
    if (mMouseEnterVolumeConnection.connected())
    {
        mMouseEnterVolumeConnection.disconnect();
    }
    if (mMouseEnterNearbyMediaConnection.connected())
    {
        mMouseEnterNearbyMediaConnection.disconnect();
    }

    if (new_value)
    {
        mMouseEnterPresetsConnection = mIconPresetsGraphic->setMouseEnterCallback(boost::bind(&LLStatusBar::onMouseEnterPresets, this));
        mMouseEnterPresetsCameraConnection = mIconPresetsCamera->setMouseEnterCallback(boost::bind(&LLStatusBar::onMouseEnterPresetsCamera, this));
        mMouseEnterVolumeConnection = mBtnVolume->setMouseEnterCallback(boost::bind(&LLStatusBar::onMouseEnterVolume, this));
        mMouseEnterNearbyMediaConnection = mMediaToggle->setMouseEnterCallback(boost::bind(&LLStatusBar::onMouseEnterNearbyMedia, this));
    }
}

// <FS:Zi> FIRE-20390, FIRE-4269 - Option for 12/24 hour clock and seconds display
void LLStatusBar::onTimeFormatChanged(const std::string& format)
{
    mClockFormat = format;
    updateClockDisplay();
}
// </FS:Zi>

// Implements secondlife:///app/balance/request to request a L$ balance
// update via UDP message system. JC
class LLBalanceHandler : public LLCommandHandler
{
public:
    // Requires "trusted" browser/URL source
    LLBalanceHandler() : LLCommandHandler("balance", UNTRUSTED_BLOCK) { }
    bool handle(const LLSD& tokens, const LLSD& query_map, const std::string& grid, LLMediaCtrl* web)
    {
        if (tokens.size() == 1
            && tokens[0].asString() == "request")
        {
            LLStatusBar::sendMoneyBalanceRequest();
            return true;
        }
        return false;
    }
};
// register with command dispatch system
LLBalanceHandler gBalanceHandler;
