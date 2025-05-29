/**
 * @file llstatusbar.h
 * @brief LLStatusBar class definition
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

#ifndef LL_LLSTATUSBAR_H
#define LL_LLSTATUSBAR_H

#include "llpanel.h"

// <FS:Ansariel> Pathfinding support
#include "llpathfindingnavmesh.h"

// "Constants" loaded from settings.xml at start time
extern S32 STATUS_BAR_HEIGHT;

class LLButton;
class LLLineEditor;
class LLMessageSystem;
class LLTextBox;
class LLTextEditor;
class LLUICtrl;
class LLUUID;
class LLFrameTimer;
class LLStatGraph;
class LLPanelPresetsCameraPulldown;
class LLPanelPresetsPulldown;
class LLPanelVolumePulldown;
class LLPanelNearByMedia;
class LLIconCtrl;
class LLSearchEditor;
class LLParcelChangeObserver;
class LLPanel;

// <FS:Ansariel> Pathfinding support
class LLPathfindingNavMeshStatus;

class LLRegionDetails
{
public:
    LLRegionDetails() :
        mRegionName("Unknown"),
        mParcelName("Unknown"),
        mAccessString("Unknown"),
        mX(0),
        mY(0),
        mZ(0),
        mArea (0),
        mForSale(false),
        mOwner("Unknown"),
        mTraffic(0),
        mBalance(0),
        mPing(0)
    {
    }
    std::string mRegionName;
    std::string mParcelName;
    std::string mAccessString;
    S32     mX;
    S32     mY;
    S32     mZ;
    S32     mArea;
    bool    mForSale;
    std::string mOwner;
    F32     mTraffic;
    S32     mBalance;
    std::string mTime;
    U32     mPing;
};

namespace ll
{
    namespace statusbar
    {
        struct SearchData;
    }
}
class LLStatusBar
:   public LLPanel
{
public:
    LLStatusBar(const LLRect& rect );
    /*virtual*/ ~LLStatusBar();

    /*virtual*/ void draw();

    /*virtual*/ bool handleRightMouseDown(S32 x, S32 y, MASK mask);
    /*virtual*/ bool postBuild();

    // MANIPULATORS
    void        setBalance(S32 balance);
    void        debitBalance(S32 debit);
    void        creditBalance(S32 credit);

    // Request the latest currency balance from the server
    static void sendMoneyBalanceRequest();

    void        setHealth(S32 percent);

    void setLandCredit(S32 credit);
    void setLandCommitted(S32 committed);

    void        refresh();
    void setVisibleForMouselook(bool visible);
        // some elements should hide in mouselook

    /**
     * Updates location and parcel icons on login complete
     */
    void handleLoginComplete();

    // ACCESSORS
    S32         getBalance() const;
    S32         getHealth() const;

    bool isUserTiered() const;
    S32 getSquareMetersCredit() const;
    S32 getSquareMetersCommitted() const;
    S32 getSquareMetersLeft() const;
    LLRegionDetails mRegionDetails;

    void setBalanceVisible(bool visible);

    LLPanelNearByMedia* getNearbyMediaPanel() { return mPanelNearByMedia; }
    bool getAudioStreamEnabled() const;

    void setBackgroundColor( const LLColor4& color );

    // <FS:Zi> External toggles for media and streams
    void toggleMedia(bool enable);
    void toggleStream(bool enable);
    // </FS:Zi>

    // <COLOSI opensim multi-currency support>
    // force update of the "BUY L$" button when currency symbol is changed.
    void updateCurrencySymbols();
    // </COLOSI opensim multi-currency support>

    // <FS:Zi> FIRE-20390, FIRE-4269 - Option for 12/24 hour clock and seconds display
    void onTimeFormatChanged(const std::string& format);

private:

    void onClickBuyCurrency();
    void onVolumeChanged(const LLSD& newvalue);
    //void onVoiceChanged(const LLSD& newvalue); // <FS:Ansariel> Fix LL voice disabled on 2nd instance nonsense

    void onMouseEnterPresetsCamera();
    void onMouseEnterPresets();
    void onMouseEnterVolume();
    void onMouseEnterNearbyMedia();

    static void onClickStreamToggle(void* data);        // <FS:Zi> Media/Stream separation
    static void onClickMediaToggle(void* data);
    static void onClickVolume(void* data); // <FS:Ansariel> Open popup panels on click if FSStatusBarMenuButtonPopupOnRollover is disabled

    static void onClickBalance(void* data);

    LLSearchEditor *mFilterEdit;
    LLPanel *mSearchPanel;
    void onUpdateFilterTerm();

    std::unique_ptr< ll::statusbar::SearchData > mSearchData;
    void collectSearchableItems();
    void updateMenuSearchVisibility( const LLSD& data );
    void updateMenuSearchPosition(); // depends onto balance position
    void updateBalancePanelPosition();

    class LLParcelChangeObserver;

    friend class LLParcelChangeObserver;

    // <FS:Ansariel> This enum also defines the order of visibility in the
    //               status bar in reverse order!
    enum EParcelIcon
    {
        VOICE_ICON = 0,
        FLY_ICON,
        PUSH_ICON,
        BUILD_ICON,
        SCRIPTS_ICON,
        SEE_AVATARS_ICON,
        PATHFINDING_DIRTY_ICON,
        PATHFINDING_DISABLED_ICON,
        DAMAGE_ICON,
        ICON_COUNT
    };

    /**
     * Initializes parcel icons controls. Called from the constructor.
     */
    void initParcelIcons();

    /**
     * Handles clicks on the parcel icons.
     */
    void onParcelIconClick(EParcelIcon icon);

    /**
     * Handles clicks on the info buttons.
     */
    void onInfoButtonClicked();

    // <FS:PP> FIRE-6287: Clicking on traffic indicator toggles Lag Meter window
    /**
     * Handles clicks on the connection status indicator.
     */
    void onBandwidthGraphButtonClicked();
    // </FS:PP> FIRE-6287: Clicking on traffic indicator toggles Lag Meter window

    /**
     * Called when agent changes the parcel.
     */
    void onAgentParcelChange();

    /**
     * Called when context menu item is clicked.
     */
    void onContextMenuItemClicked(const LLSD::String& userdata);

    /**
     * Called when user checks/unchecks Show Paracel Properies menu item
     */
    void onNavBarShowParcelPropertiesCtrlChanged();

    /**
     * Called when user checks/unchecks Show Coordinates menu item.
     */
    void onNavBarShowCoordinatesCtrlChanged();

    /**
     * Shorthand to call updateParcelInfoText() and updateParcelIcons().
     */
    void update();

    /**
     * Updates parcel info text (mParcelInfoText).
     */
    void updateParcelInfoText();

    /**
     * Updates the visibility state of the parcel icons according to parcel properties
     */
     // <FS:Ansariel> Does not exist 15-02-2021
    //void updateParcelIconVisibility();

    void onBuyLandClicked();

    // <FS:Ansariel> FIRE-19697: Add setting to disable graphics preset menu popup on mouse over
    void onPopupRolloverChanged(const LLSD& newvalue);

    // <FS:Ansariel> FIRE-14482: Show FPS in status bar
    void onShowFPSChanged(const LLSD& newvalue);

    /**
     * Updates parcel panel pos (mParcelPanel).
     */
    void updateParcelPanel();

    /**
     * Updates health information (mDamageText).
     */
    void updateHealth();

    /**
     * Lays out all parcel icons starting from right edge of the mParcelInfoText + 11px
     * (see screenshots in EXT-5808 for details).
     */
    void layoutParcelIcons();

    /**
     * Lays out a widget. Widget's rect mLeft becomes equal to the 'left' argument.
     */
    S32 layoutWidget(LLUICtrl* ctrl, S32 left);

    /**
     * Generates location string and returns it in the loc_str parameter.
     */
    void buildLocationString(std::string& loc_str, bool show_coords);

    /**
     * Sets new value to the mParcelInfoText and updates the size of the top bar.
     */
    void setParcelInfoText(const std::string& new_text);

    void updateNetstatVisibility(const LLSD& data);
    void updateVolumeControlsVisibility(const LLSD& data); // <FS:PP> Option to hide volume controls (sounds, media, stream) in upper right

public:

    /**
     * Updates parcel icons (mParcelIcon[]).
     */
    void updateParcelIcons();

    void setRebakeStuck(bool stuck) { mRebakeStuck = stuck;} // <FS:LO> FIRE-7639 - Stop the blinking after a while

private:
    LLTextBox   *mTextBalance;
    LLTextBox   *mTextHealth;
    LLTextBox   *mTextTime;
    LLTextBox   *mFPSText; // <FS:Ansariel> FIRE-14482: Show FPS in status bar

    LLTextBox*  mTextParcelName;

    LLStatGraph *mSGBandwidth;
    LLStatGraph *mSGPacketLoss;

    LLButton    *mIconPresetsCamera;
    LLButton    *mIconPresetsGraphic;
    LLButton    *mBtnVolume;
    LLTextBox   *mBoxBalance;
    LLButton    *mStreamToggle;     // ## Zi: Media/Stream separation
    LLButton    *mMediaToggle;
    LLButton    *mBandwidthButton; // <FS:PP> FIRE-6287: Clicking on traffic indicator toggles Lag Meter window
    // <FS:Ansariel> Script debug
    LLIconCtrl  *mScriptOut;
    // </FS:Ansariel> Script debug
    LLFrameTimer    mClockUpdateTimer;
    LLFrameTimer    mFPSUpdateTimer; // <FS:Ansariel> FIRE-14482: Show FPS in status bar
    LLFrameTimer    mNetStatUpdateTimer; // <FS:Ansariel> Less frequent update of net stats

    S32             mVolumeIconsWidth; // <FS:PP> Option to hide volume controls (sounds, media, stream) in upper right
    S32             mBalance;
    S32             mHealth;
    S32             mSquareMetersCredit;
    S32             mSquareMetersCommitted;
    bool            mAudioStreamEnabled;
    bool            mShowParcelIcons;
    LLFrameTimer*   mBalanceTimer;
    LLFrameTimer*   mHealthTimer;
    LLPanelPresetsCameraPulldown* mPanelPresetsCameraPulldown;
    LLPanelPresetsPulldown* mPanelPresetsPulldown;
    LLPanelVolumePulldown* mPanelVolumePulldown;
    LLPanelNearByMedia* mPanelNearByMedia;

    LLPanel*                mParcelInfoPanel;
    LLButton*               mInfoBtn;
    LLTextBox*              mParcelInfoText;
    LLTextBox*              mDamageText;
    LLIconCtrl*             mParcelIcon[ICON_COUNT];
    LLParcelChangeObserver* mParcelChangedObserver;
    LLPanel*                mBalancePanel;
    LLButton*               mBuyParcelBtn;
    LLPanel*                mTimeMediaPanel;

    boost::signals2::connection mParcelPropsCtrlConnection;
    boost::signals2::connection mShowCoordsCtrlConnection;
    boost::signals2::connection mParcelMgrConnection;

    // <FS:Ansariel> FIRE-19697: Add setting to disable graphics preset menu popup on mouse over
    boost::signals2::connection mMouseEnterPresetsConnection;
    boost::signals2::connection mMouseEnterPresetsCameraConnection;
    boost::signals2::connection mMouseEnterVolumeConnection;
    boost::signals2::connection mMouseEnterNearbyMediaConnection;
    // </FS:Ansariel>

    // <FS:Zi> Pathfinding rebake functions
    bool            rebakeRegionCallback(const LLSD& notification,const LLSD& response);

    LLFrameTimer    mRebakingTimer;
    bool            mPathfindingFlashOn;
    // </FS:Zi>

    // <FS:Ansariel> Script debug
    bool            mNearbyIcons;

    bool    mRebakeStuck; // <FS:LO> FIRE-7639 - Stop the blinking after a while

// <FS:Zi> Make hovering over parcel info actually work
    void    onMouseEnterParcelInfo();
    void    onMouseLeaveParcelInfo();
// </FS:Zi>

// <FS:Zi> FIRE-20390, FIRE-4269 - Option for 12/24 hour clock and seconds display
    std::map<std::string, std::string> mClockFormatChoices;
    std::string mClockFormat;

    void    updateClockDisplay();
// </FS:Zi>

    std::string mCurrentLocationString;
};

// *HACK: Status bar owns your cached money balance. JC
bool can_afford_transaction(S32 cost);

extern LLStatusBar *gStatusBar;

#endif
