/**
 * @file llfloatercamera.cpp
 * @brief Container for camera control buttons (zoom, pan, orbit)
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

#include "llfloatercamera.h"

// Library includes
#include "llfloaterreg.h"

// Viewer includes
#include "llagent.h"
#include "llagentcamera.h"
#include "llpresetsmanager.h"
#include "lljoystickbutton.h"
#include "llviewercontrol.h"
#include "llviewercamera.h"
#include "lltoolmgr.h"
#include "lltoolfocus.h"
#include "llslider.h"
#include "llfirstuse.h"
#include "llhints.h"
#include "lltabcontainer.h"
#include "llviewercamera.h"
#include "llvoavatarself.h"
// [RLVa:KB] - @setcam
#include "rlvactions.h"
// [/RLVa:KB]

static LLDefaultChildRegistry::Register<LLPanelCameraItem> r("panel_camera_item");

const F32 NUDGE_TIME = 0.25f;       // in seconds
const F32 ORBIT_NUDGE_RATE = 0.05f; // fraction of normal speed

// constants
#define ORBIT "cam_rotate_stick"
#define PAN "cam_track_stick"
#define ZOOM "zoom"
#define CONTROLS "controls"

bool LLFloaterCamera::sFreeCamera = false;
bool LLFloaterCamera::sAppearanceEditing = false;

// Zoom the camera in and out
class LLPanelCameraZoom
:   public LLPanel
{
    LOG_CLASS(LLPanelCameraZoom);

public:
    struct Params : public LLInitParam::Block<Params, LLPanel::Params> {};

    LLPanelCameraZoom() { onCreate(); }

    /* virtual */ bool  postBuild();
    /* virtual */ void  draw();

protected:
    LLPanelCameraZoom(const Params& p) { onCreate(); }

    void    onCreate();
    void    onZoomPlusHeldDown();
    void    onZoomMinusHeldDown();
    void    onSliderValueChanged();
    void    onCameraTrack();
    void    onCameraRotate();
    F32     getOrbitRate(F32 time);
// <FS:Chanayane> Camera roll (from Alchemy)
    void    onRollLeftHeldDown();
    void    onRollRightHeldDown();
// </FS:Chanayane>

private:
    LLButton*   mPlusBtn { nullptr };
    LLButton*   mMinusBtn{ nullptr };
    LLSlider*   mSlider{ nullptr };
    LLButton*   mRollLeft{ nullptr };
    LLButton*   mRollRight{ nullptr };

    friend class LLUICtrlFactory;
};

LLPanelCameraItem::Params::Params()
:   icon_over("icon_over"),
    icon_selected("icon_selected"),
    picture("picture"),
    text("text"),
    selected_picture("selected_picture"),
    mousedown_callback("mousedown_callback")
{
}

LLPanelCameraItem::LLPanelCameraItem(const LLPanelCameraItem::Params& p)
:   LLPanel(p)
{
    LLIconCtrl::Params icon_params = p.picture;
    mPicture = LLUICtrlFactory::create<LLIconCtrl>(icon_params);
    addChild(mPicture);

    icon_params = p.icon_over;
    mIconOver = LLUICtrlFactory::create<LLIconCtrl>(icon_params);
    addChild(mIconOver);

    icon_params = p.icon_selected;
    mIconSelected = LLUICtrlFactory::create<LLIconCtrl>(icon_params);
    addChild(mIconSelected);

    icon_params = p.selected_picture;
    mPictureSelected = LLUICtrlFactory::create<LLIconCtrl>(icon_params);
    addChild(mPictureSelected);

    LLTextBox::Params text_params = p.text;
    mText = LLUICtrlFactory::create<LLTextBox>(text_params);
    addChild(mText);

    if (p.mousedown_callback.isProvided())
    {
        setCommitCallback(initCommitCallback(p.mousedown_callback));
    }
}

void set_view_visible(LLView* parent, const std::string& name, bool visible)
{
    parent->getChildView(name)->setVisible(visible);
}

bool LLPanelCameraItem::postBuild()
{
    setMouseEnterCallback(boost::bind(set_view_visible, this, "hovered_icon", true));
    setMouseLeaveCallback(boost::bind(set_view_visible, this, "hovered_icon", false));
    setMouseDownCallback(boost::bind(&LLPanelCameraItem::onAnyMouseClick, this));
    setRightMouseDownCallback(boost::bind(&LLPanelCameraItem::onAnyMouseClick, this));
    return true;
}

void LLPanelCameraItem::onAnyMouseClick()
{
    if (mCommitSignal) (*mCommitSignal)(this, LLSD());
}

void LLPanelCameraItem::setValue(const LLSD& value)
{
    if (!value.isMap()) return;;
    if (!value.has("selected")) return;
    getChildView("selected_icon")->setVisible( value["selected"]);
    getChildView("picture")->setVisible( !value["selected"]);
    getChildView("selected_picture")->setVisible( value["selected"]);
}

static LLPanelInjector<LLPanelCameraZoom> t_camera_zoom_panel("camera_zoom_panel");

//-------------------------------------------------------------------------------
// LLPanelCameraZoom
//-------------------------------------------------------------------------------

void LLPanelCameraZoom::onCreate()
{
    mCommitCallbackRegistrar.add("Zoom.minus", boost::bind(&LLPanelCameraZoom::onZoomMinusHeldDown, this));
    mCommitCallbackRegistrar.add("Zoom.plus", boost::bind(&LLPanelCameraZoom::onZoomPlusHeldDown, this));
    mCommitCallbackRegistrar.add("Slider.value_changed", boost::bind(&LLPanelCameraZoom::onSliderValueChanged, this));
    mCommitCallbackRegistrar.add("Camera.track", boost::bind(&LLPanelCameraZoom::onCameraTrack, this));
    mCommitCallbackRegistrar.add("Camera.rotate", boost::bind(&LLPanelCameraZoom::onCameraRotate, this));
// <FS:Chanayane> Camera roll (from Alchemy)
    mCommitCallbackRegistrar.add("Camera.roll_left", boost::bind(&LLPanelCameraZoom::onRollLeftHeldDown, this));
    mCommitCallbackRegistrar.add("Camera.roll_right", boost::bind(&LLPanelCameraZoom::onRollRightHeldDown, this));
// </FS:Chanayane>
}

bool LLPanelCameraZoom::postBuild()
{
    mPlusBtn  = getChild<LLButton>("zoom_plus_btn");
    mMinusBtn = getChild<LLButton>("zoom_minus_btn");
    mSlider   = getChild<LLSlider>("zoom_slider");
// <FS:Chanayane> Camera roll (from Alchemy)
    mRollLeft   = getChild<LLButton>("roll_left");
    mRollRight  = getChild<LLButton>("roll_right");
// </FS:Chanayane>
    return LLPanel::postBuild();
}

void LLPanelCameraZoom::draw()
{
    mSlider->setValue(gAgentCamera.getCameraZoomFraction());
    LLPanel::draw();
}

void LLPanelCameraZoom::onZoomPlusHeldDown()
{
    F32 val = mSlider->getValueF32();
    F32 inc = mSlider->getIncrement();
    mSlider->setValue(val - inc);
    F32 time = mPlusBtn->getHeldDownTime();
    gAgentCamera.unlockView();
    gAgentCamera.setOrbitInKey(getOrbitRate(time));
}

void LLPanelCameraZoom::onZoomMinusHeldDown()
{
    F32 val = mSlider->getValueF32();
    F32 inc = mSlider->getIncrement();
    mSlider->setValue(val + inc);
    F32 time = mMinusBtn->getHeldDownTime();
    gAgentCamera.unlockView();
    gAgentCamera.setOrbitOutKey(getOrbitRate(time));
}

// <FS:Chanayane> Camera roll (from Alchemy)
void LLPanelCameraZoom::onRollLeftHeldDown()
{
    F32 time = mRollLeft->getHeldDownTime();
    gAgentCamera.unlockView();
    gAgentCamera.setRollLeftKey(getOrbitRate(time));
}

void LLPanelCameraZoom::onRollRightHeldDown()
{
    F32 time = mRollRight->getHeldDownTime();
    gAgentCamera.unlockView();
    gAgentCamera.setRollRightKey(getOrbitRate(time));
}
// </FS:Chanayane>

void LLPanelCameraZoom::onCameraTrack()
{
    // EXP-202 when camera panning activated, remove the hint
    LLFirstUse::viewPopup( false );
}

void LLPanelCameraZoom::onCameraRotate()
{
    // EXP-202 when camera rotation activated, remove the hint
    LLFirstUse::viewPopup( false );
}

F32 LLPanelCameraZoom::getOrbitRate(F32 time)
{
    if( time < NUDGE_TIME )
    {
        F32 rate = ORBIT_NUDGE_RATE + time * (1 - ORBIT_NUDGE_RATE)/ NUDGE_TIME;
        return rate;
    }
    else
    {
        return 1;
    }
}

void  LLPanelCameraZoom::onSliderValueChanged()
{
    F32 zoom_level = mSlider->getValueF32();
    gAgentCamera.setCameraZoomFraction(zoom_level);
}

void activate_camera_tool()
{
    LLToolMgr::getInstance()->setTransientTool(LLToolCamera::getInstance());
};

class LLCameraInfoPanel : public LLPanel
{
public:
    typedef std::function<LLVector3()> get_vector_t;

    LLCameraInfoPanel(
        const LLView* parent,
        const char* title,
        const LLCoordFrame& camera,
        const get_vector_t get_focus
    )
    : LLPanel([&]() -> LLPanel::Params
        {
            LLPanel::Params params;
            params.rect = LLRect(parent->getLocalRect());
            return params;
        }())
    , mTitle(title)
    , mCamera(camera)
    , mGetFocus(get_focus)
    , mFont(LLFontGL::getFontSansSerifBig())
    {
    }

    virtual void draw() override
    {
        LLPanel::draw();

        static const U32 HPADDING = 10;
        static const U32 VPADDING = 5;
        LLVector3 focus = mGetFocus();
        LLVector3 sight = focus - mCamera.mOrigin;
        std::pair<const char*, const LLVector3&> const data[] =
        {
            { "Origin:", mCamera.mOrigin },
            { "X Axis:", mCamera.mXAxis },
            { "Y Axis:", mCamera.mYAxis },
            { "Z Axis:", mCamera.mZAxis },
            { "Focus:", focus },
            { "Sight:", sight }
        };
        S32 width = getRect().getWidth();
        S32 height = getRect().getHeight();
        S32 row_count = 1 + sizeof(data) / sizeof(*data);
        S32 row_height = (height - VPADDING * 2) / row_count;
        S32 top = height - VPADDING - row_height / 2;
        mFont->renderUTF8(mTitle, 0, HPADDING, top, LLColor4::white, LLFontGL::LEFT, LLFontGL::VCENTER);
        for (const auto& row : data)
        {
            top -= row_height;
            mFont->renderUTF8(row.first, 0, HPADDING, top, LLColor4::white, LLFontGL::LEFT, LLFontGL::VCENTER);
            const LLVector3& vector = row.second;
            for (S32 i = 0; i < 3; ++i)
            {
                std::string text = llformat("%.6f", vector[i]);
                S32 right = width / 4 * (i + 2) - HPADDING;
                mFont->renderUTF8(text, 0, right, top, LLColor4::white, LLFontGL::RIGHT, LLFontGL::VCENTER);
            }
        }
    }

private:
    const char* mTitle;
    const LLCoordFrame& mCamera;
    const get_vector_t mGetFocus;
    const LLFontGL* mFont;
};

//
// Member functions
//

// static
bool LLFloaterCamera::inFreeCameraMode()
{
    LLFloaterCamera* floater_camera = LLFloaterCamera::findInstance();

    // <FS:Ansariel> Phototools camera
    if (!floater_camera)
    {
        floater_camera = LLFloaterCamera::findPhototoolsInstance();
    }
    // <FS:Ansariel>

    // <FS:Ansariel> Optional small camera floater
    if (!floater_camera)
    {
        floater_camera = LLFloaterCamera::findSmallInstance();
    }
    // <FS:Ansariel>

    if (floater_camera && floater_camera->mCurrMode == CAMERA_CTRL_MODE_FREE_CAMERA && gAgentCamera.getCameraMode() != CAMERA_MODE_MOUSELOOK)
    {
        return true;
    }
    return false;
}

// static
void LLFloaterCamera::resetCameraMode()
{
    LLFloaterCamera* floater_camera = LLFloaterCamera::findInstance();
    if (!floater_camera) return;
    floater_camera->switchMode(CAMERA_CTRL_MODE_PAN);

    // <FS:Ansariel> Phototools camera
    floater_camera = LLFloaterCamera::findPhototoolsInstance();
    if (!floater_camera) return;
    floater_camera->switchMode(CAMERA_CTRL_MODE_PAN);
    // <FS:Ansariel>

    // <FS:Ansariel> Optional small camera floater
    floater_camera = LLFloaterCamera::findSmallInstance();
    if (!floater_camera) return;
    floater_camera->switchMode(CAMERA_CTRL_MODE_PAN);
    // </FS:Ansariel>
}

// static
void LLFloaterCamera::onAvatarEditingAppearance(bool editing)
{
    sAppearanceEditing = editing;
    LLFloaterCamera* floater_camera = LLFloaterCamera::findInstance();
    if (!floater_camera) return;
    floater_camera->handleAvatarEditingAppearance(editing);

    // <FS:Ansariel> Phototools camera
    floater_camera = LLFloaterCamera::findPhototoolsInstance();
    if (!floater_camera) return;
    floater_camera->handleAvatarEditingAppearance(editing);
    // <FS:Ansariel>

    // <FS:Ansariel> Optional small camera floater
    floater_camera = LLFloaterCamera::findSmallInstance();
    if (!floater_camera) return;
    floater_camera->handleAvatarEditingAppearance(editing);
    // <FS:Ansariel>
}

// static
void LLFloaterCamera::onDebugCameraToggled()
{
    if (LLFloaterCamera* instance = LLFloaterCamera::findInstance())
    {
        instance->showDebugInfo(LLView::sDebugCamera);
    }

    if (LLView::sDebugCamera)
    {
        LLFloaterReg::showInstanceOrBringToFront("camera");
    }
}

void LLFloaterCamera::showDebugInfo(bool show)
{
    // Initially LLPanel contains 1 child "view_border"
    if (show && mViewerCameraInfo->getChildCount() < 2)
    {
        mViewerCameraInfo->addChild(new LLCameraInfoPanel(mViewerCameraInfo, "Viewer Camera", *LLViewerCamera::getInstance(),
            []() { return LLViewerCamera::getInstance()->getPointOfInterest(); }));
        mAgentCameraInfo->addChild(new LLCameraInfoPanel(mAgentCameraInfo, "Agent Camera", gAgent.getFrameAgent(),
            []() { return gAgent.getPosAgentFromGlobal(gAgentCamera.calcFocusPositionTargetGlobal()); }));
    }

    mAgentCameraInfo->setVisible(show);
    mViewerCameraInfo->setVisible(show);
}

void LLFloaterCamera::handleAvatarEditingAppearance(bool editing)
{
}

void LLFloaterCamera::update()
{
    ECameraControlMode mode = determineMode();
    if (mode != mCurrMode)
    {
        setMode(mode);
    }
}


void LLFloaterCamera::toPrevMode()
{
    switchMode(mPrevMode);
}

// static
void LLFloaterCamera::onLeavingMouseLook()
{
    LLFloaterCamera* floater_camera = LLFloaterCamera::findInstance();
    if (floater_camera)
    {
        floater_camera->updateItemsSelection();
        if(floater_camera->inFreeCameraMode())
        {
            activate_camera_tool();
        }
    }

    // <FS:Ansariel> Phototools camera
    floater_camera = LLFloaterCamera::findPhototoolsInstance();
    if (floater_camera)
    {
        floater_camera->updateItemsSelection();
        if(floater_camera->inFreeCameraMode())
        {
            activate_camera_tool();
        }
    }
    // </FS:Ansariel>

    // <FS:Ansariel> Optional small camera floater
    floater_camera = LLFloaterCamera::findSmallInstance();
    if (floater_camera)
    {
        floater_camera->updateItemsSelection();
        if(floater_camera->inFreeCameraMode())
        {
            activate_camera_tool();
        }
    }
    // </FS:Ansariel>
}

LLFloaterCamera* LLFloaterCamera::findInstance()
{
    return LLFloaterReg::findTypedInstance<LLFloaterCamera>("camera");
}

// <FS:Ansariel> Phototools camera
LLFloaterCamera* LLFloaterCamera::findPhototoolsInstance()
{
    return LLFloaterReg::findTypedInstance<LLFloaterCamera>("phototools_camera");
}
// </FS:Ansariel>

// <FS:Ansariel> Optional small camera floater
LLFloaterCamera* LLFloaterCamera::findSmallInstance()
{
    return LLFloaterReg::findTypedInstance<LLFloaterCamera>("fs_camera_small");
}
// </FS:Ansariel>

void LLFloaterCamera::onOpen(const LLSD& key)
{
    LLFirstUse::viewPopup();

    mZoom->onOpen(key);

    // Returns to previous mode, see EXT-2727(View tool should remember state).
    // In case floater was just hidden and it isn't reset the mode
    // just update state to current one. Else go to previous.
    if ( !mClosed )
        updateState();
    else
        toPrevMode();
    mClosed = false;

    // <FS:Ansariel> Optional small camera floater
    if (mPresetCombo)
        populatePresetCombo();

    showDebugInfo(LLView::sDebugCamera);
}

void LLFloaterCamera::onClose(bool app_quitting)
{
    //We don't care of camera mode if app is quitting
    if (app_quitting)
        return;
    // It is necessary to reset mCurrMode to CAMERA_CTRL_MODE_PAN so
    // to avoid seeing an empty floater when reopening the control.
    if (mCurrMode == CAMERA_CTRL_MODE_FREE_CAMERA)
        mCurrMode = CAMERA_CTRL_MODE_PAN;
    // When mCurrMode is in CAMERA_CTRL_MODE_PAN
    // switchMode won't modify mPrevMode, so force it here.
    // It is needed to correctly return to previous mode on open, see EXT-2727.
    if (mCurrMode == CAMERA_CTRL_MODE_PAN)
        mPrevMode = CAMERA_CTRL_MODE_PAN;

    switchMode(CAMERA_CTRL_MODE_PAN);
    mClosed = true;

    gAgent.setMovementLocked(false);
}

LLFloaterCamera::LLFloaterCamera(const LLSD& val)
:   LLFloater(val),
    mClosed(false),
    mCurrMode(CAMERA_CTRL_MODE_PAN),
    mPrevMode(CAMERA_CTRL_MODE_PAN),
    mPresetCombo(nullptr) // <FS:Ansariel> Optional small camera floater
{
    LLHints::getInstance()->registerHintTarget("view_popup", getHandle());
    mCommitCallbackRegistrar.add("CameraPresets.ChangeView", boost::bind(&LLFloaterCamera::onClickCameraItem, _2));
    mCommitCallbackRegistrar.add("CameraPresets.Save", boost::bind(&LLFloaterCamera::onSavePreset, this));
    mCommitCallbackRegistrar.add("CameraPresets.ShowPresetsList", boost::bind(&LLFloaterReg::showInstance, "camera_presets", LLSD(), false));
}

// virtual
bool LLFloaterCamera::postBuild()
{
    mControls = getChild<LLPanel>("controls");
    mAgentCameraInfo = getChild<LLPanel>("agent_camera_info");
    mViewerCameraInfo = getChild<LLPanel>("viewer_camera_info");
    mRotate = getChild<LLJoystickCameraRotate>(ORBIT);
    mZoom = getChild<LLPanelCameraZoom>(ZOOM);
    mTrack = getChild<LLJoystickCameraTrack>(PAN);
    mPresetCombo = findChild<LLComboBox>("preset_combo"); // <FS:Ansariel> Optional small camera floater
    // <FS:Ansariel> Improved camera floater
    //mPreciseCtrls = getChild<LLTextBox>("precise_ctrs_label");

    //mPreciseCtrls->setShowCursorHand(false);
    //mPreciseCtrls->setSoundFlags(LLView::MOUSE_UP);
    //mPreciseCtrls->setClickedCallback(boost::bind(&LLFloaterReg::showInstance, "prefs_view_advanced", LLSD(), false));
    // </FS:Ansariel>

    // <FS:Ansariel> Phototools support
    LLButton* presets_btn = findChild<LLButton>("presets_btn");
    if (presets_btn)
    {
        presets_btn->setCommitCallback(boost::bind(&LLFloaterCamera::switchViews, this, CAMERA_CTRL_MODE_PRESETS));
    }
    LLButton* modes_btn = findChild<LLButton>("avatarview_btn");
    if (modes_btn)
    {
        modes_btn->setCommitCallback(boost::bind(&LLFloaterCamera::switchViews, this, CAMERA_CTRL_MODE_MODES));
    }
    LLButton* pan_btn = findChild<LLButton>("pan_btn");
    if (pan_btn)
    {
        pan_btn->setCommitCallback(boost::bind(&LLFloaterCamera::switchViews, this, CAMERA_CTRL_MODE_PAN));
    }
    // </FS:Ansariel>

    // <FS:Ansariel> Optional small camera floater
    //mPresetCombo->setCommitCallback(boost::bind(&LLFloaterCamera::onCustomPresetSelected, this));
    //LLPresetsManager::getInstance()->setPresetListChangeCameraCallback(boost::bind(&LLFloaterCamera::populatePresetCombo, this));
    if (mPresetCombo)
    {
        mPresetCombo->setCommitCallback(boost::bind(&LLFloaterCamera::onCustomPresetSelected, this));
        LLPresetsManager::getInstance()->setPresetListChangeCameraCallback(boost::bind(&LLFloaterCamera::populatePresetCombo, this));
    }
    // </FS:Ansariel>

    update();

    // ensure that appearance mode is handled while building. See EXT-7796.
    handleAvatarEditingAppearance(sAppearanceEditing);

    return LLFloater::postBuild();
}

F32 LLFloaterCamera::getCurrentTransparency()
{

    static LLCachedControl<F32> camera_opacity(gSavedSettings, "CameraOpacity");
    static LLCachedControl<F32> active_floater_transparency(gSavedSettings, "ActiveFloaterTransparency");
    return llmin(camera_opacity(), active_floater_transparency());

}

void LLFloaterCamera::fillFlatlistFromPanel (LLFlatListView* list, LLPanel* panel)
{
    // copying child list and then iterating over a copy, because list itself
    // is changed in process
    const child_list_t child_list = *panel->getChildList();
    child_list_t::const_reverse_iterator iter = child_list.rbegin();
    child_list_t::const_reverse_iterator end = child_list.rend();
    for ( ; iter != end; ++iter)
    {
        LLView* view = *iter;
        LLPanel* item = dynamic_cast<LLPanel*>(view);
        if (panel)
            list->addItem(item);
    }

}

ECameraControlMode LLFloaterCamera::determineMode()
{
    if (sAppearanceEditing)
    {
        // this is the only enabled camera mode while editing agent appearance.
        return CAMERA_CTRL_MODE_PAN;
    }

    LLTool* curr_tool = LLToolMgr::getInstance()->getCurrentTool();
    if (curr_tool == LLToolCamera::getInstance())
    {
        return CAMERA_CTRL_MODE_FREE_CAMERA;
    }

    if (gAgentCamera.getCameraMode() == CAMERA_MODE_MOUSELOOK)
    {
        return CAMERA_CTRL_MODE_PRESETS;
    }

    return CAMERA_CTRL_MODE_PAN;
}


void clear_camera_tool()
{
    LLToolMgr* tool_mgr = LLToolMgr::getInstance();
    if (tool_mgr->usingTransientTool() &&
        tool_mgr->getCurrentTool() == LLToolCamera::getInstance())
    {
        tool_mgr->clearTransientTool();
    }
}


void LLFloaterCamera::setMode(ECameraControlMode mode)
{
    if (mode != mCurrMode)
    {
        mPrevMode = mCurrMode;
        mCurrMode = mode;
    }

    updateState();
}

void LLFloaterCamera::switchMode(ECameraControlMode mode)
{
    switch (mode)
    {
    case CAMERA_CTRL_MODE_PRESETS:
    case CAMERA_CTRL_MODE_PAN:
        sFreeCamera = false;
        setMode(mode); // depends onto sFreeCamera
        clear_camera_tool();
        break;

    case CAMERA_CTRL_MODE_FREE_CAMERA:
        sFreeCamera = true;
        setMode(mode);
        activate_camera_tool();
        break;

    default:
        //normally we won't occur here
        llassert_always(false);
    }
}

void LLFloaterCamera::updateState()
{
    updateItemsSelection();

    if (CAMERA_CTRL_MODE_FREE_CAMERA == mCurrMode)
    {
        return;
    }

    //updating buttons
    std::map<ECameraControlMode, LLButton*>::const_iterator iter = mMode2Button.begin();
    for (; iter != mMode2Button.end(); ++iter)
    {
        iter->second->setToggleState(iter->first == mCurrMode);
    }
}

void LLFloaterCamera::updateItemsSelection()
{
    ECameraPreset preset = (ECameraPreset) gSavedSettings.getU32("CameraPresetType");
    LLSD argument;
    argument["selected"] = (preset == CAMERA_PRESET_REAR_VIEW) && !sFreeCamera;
    getChild<LLPanelCameraItem>("rear_view")->setValue(argument);
    argument["selected"] = (preset == CAMERA_PRESET_GROUP_VIEW) && !sFreeCamera;
    getChild<LLPanelCameraItem>("group_view")->setValue(argument);
    argument["selected"] = (preset == CAMERA_PRESET_FRONT_VIEW) && !sFreeCamera;
    getChild<LLPanelCameraItem>("front_view")->setValue(argument);
// <FS:PP> Third Person Perspective camera
    argument["selected"] = (preset == CAMERA_PRESET_TPP_VIEW) && !sFreeCamera;
    getChild<LLPanelCameraItem>("tpp_view")->setValue(argument);
// </FS:PP>
    argument["selected"] = gAgentCamera.getCameraMode() == CAMERA_MODE_MOUSELOOK;
    getChild<LLPanelCameraItem>("mouselook_view")->setValue(argument);
    argument["selected"] = mCurrMode == CAMERA_CTRL_MODE_FREE_CAMERA;
    getChild<LLPanelCameraItem>("object_view")->setValue(argument);
}

// static
void LLFloaterCamera::onClickCameraItem(const LLSD& param)
{
    std::string name = param.asString();

    if ("mouselook_view" == name)
    {
        gAgentCamera.changeCameraToMouselook();
    }
    else if ("object_view" == name)
    {
        LLFloaterCamera* camera_floater = LLFloaterCamera::findInstance();
        if (camera_floater)
        {
            // <FS:Ansariel> FIRE-29950: Re-add weird legacy object view camera toggle
            //camera_floater->switchMode(CAMERA_CTRL_MODE_FREE_CAMERA);
            camera_floater->mCurrMode == CAMERA_CTRL_MODE_FREE_CAMERA ? camera_floater->switchMode(CAMERA_CTRL_MODE_PAN) : camera_floater->switchMode(CAMERA_CTRL_MODE_FREE_CAMERA);
            // </FS:Ansariel>
            camera_floater->updateItemsSelection();
        }

        // <FS:Ansariel> Phototools camera
        camera_floater = LLFloaterCamera::findPhototoolsInstance();
        if (camera_floater)
        {
            // <FS:Ansariel> FIRE-29950: Re-add weird legacy object view camera toggle
            //camera_floater->switchMode(CAMERA_CTRL_MODE_FREE_CAMERA);
            camera_floater->mCurrMode == CAMERA_CTRL_MODE_FREE_CAMERA ? camera_floater->switchMode(CAMERA_CTRL_MODE_PAN) : camera_floater->switchMode(CAMERA_CTRL_MODE_FREE_CAMERA);
            // </FS:Ansariel>
            camera_floater->updateItemsSelection();
        }
        // </FS:Ansariel>

        // <FS:Ansariel> Optional small camera floater
        camera_floater = LLFloaterCamera::findSmallInstance();
        if (camera_floater)
        {
            // <FS:Ansariel> FIRE-29950: Re-add weird legacy object view camera toggle
            //camera_floater->switchMode(CAMERA_CTRL_MODE_FREE_CAMERA);
            camera_floater->mCurrMode == CAMERA_CTRL_MODE_FREE_CAMERA ? camera_floater->switchMode(CAMERA_CTRL_MODE_PAN) : camera_floater->switchMode(CAMERA_CTRL_MODE_FREE_CAMERA);
            // </FS:Ansariel>
            camera_floater->updateItemsSelection();
        }
        // </FS:Ansariel>
    }
    // <FS:Ansariel> Improved camera floater
    else if ("reset_view" == name)
    {
        LLFloaterCamera* camera_floater = LLFloaterCamera::findInstance();
        if (camera_floater)
            camera_floater->switchMode(CAMERA_CTRL_MODE_PAN);

        // <FS:Ansariel> Phototools camera
        camera_floater = LLFloaterCamera::findPhototoolsInstance();
        if (camera_floater)
            camera_floater->switchMode(CAMERA_CTRL_MODE_PAN);
        // </FS:Ansariel>

        // <FS:Ansariel> Optional small camera floater
        camera_floater = LLFloaterCamera::findSmallInstance();
        if (camera_floater)
            camera_floater->switchMode(CAMERA_CTRL_MODE_PAN);
        // </FS:Ansariel>

        gAgentCamera.changeCameraToDefault();
        switchToPreset("rear_view");
    }
    // </FS:Ansariel>
    else
    {
        LLFloaterCamera* camera_floater = LLFloaterCamera::findInstance();
        if (camera_floater)
            camera_floater->switchMode(CAMERA_CTRL_MODE_PAN);

        // <FS:Ansariel> Phototools camera
        camera_floater = LLFloaterCamera::findPhototoolsInstance();
        if (camera_floater)
            camera_floater->switchMode(CAMERA_CTRL_MODE_PAN);
        // </FS:Ansariel>

        // <FS:Ansariel> Optional small camera floater
        camera_floater = LLFloaterCamera::findSmallInstance();
        if (camera_floater)
            camera_floater->switchMode(CAMERA_CTRL_MODE_PAN);
        // </FS:Ansariel>

        switchToPreset(name);
    }
}

// static
void LLFloaterCamera::switchToPreset(const std::string& name)
{
// [RLVa:KB] - @setcam family
    if (RlvActions::isCameraPresetLocked())
    {
        return;
    }
// [/RLVa:KB]

    sFreeCamera = false;
    clear_camera_tool();
    if (PRESETS_REAR_VIEW == name)
    {
        gAgentCamera.switchCameraPreset(CAMERA_PRESET_REAR_VIEW);
    }
    else if (PRESETS_SIDE_VIEW == name)
    {
        gAgentCamera.switchCameraPreset(CAMERA_PRESET_GROUP_VIEW);
    }
    else if (PRESETS_FRONT_VIEW == name)
    {
        gAgentCamera.switchCameraPreset(CAMERA_PRESET_FRONT_VIEW);
    }
    // <FS:PP> Third Person Perspective camera
    else if (PRESETS_TPP_VIEW == name)
    {
        gAgentCamera.switchCameraPreset(CAMERA_PRESET_TPP_VIEW);
    }
    // </FS:PP>
    else
    {
        gAgentCamera.switchCameraPreset(CAMERA_PRESET_CUSTOM);
    }

    if (gSavedSettings.getString("PresetCameraActive") != name)
    {
        LLPresetsManager::getInstance()->loadPreset(PRESETS_CAMERA, name);
    }

    if (isAgentAvatarValid() && gAgentAvatarp->getParent())
    {
        LLQuaternion sit_rot = gSavedSettings.getQuaternion("AvatarSitRotation");
        if (sit_rot != LLQuaternion())
        {
            gAgent.rotate(~gAgent.getFrameAgent().getQuaternion());
            gAgent.rotate(sit_rot);
        }
        else
        {
            gAgentCamera.rotateToInitSitRot();
        }
    }
    gAgentCamera.resetCameraZoomFraction();

    LLFloaterCamera* camera_floater = LLFloaterCamera::findInstance();
    if (camera_floater)
    {
        camera_floater->updateItemsSelection();
        camera_floater->switchMode(CAMERA_CTRL_MODE_PRESETS);
    }

    // <FS:Ansariel> Phototools camera
    camera_floater = LLFloaterCamera::findPhototoolsInstance();
    if (camera_floater)
    {
        camera_floater->updateItemsSelection();
        camera_floater->switchMode(CAMERA_CTRL_MODE_PRESETS);
    }
    // </FS:Ansariel>

    // <FS:Ansariel> Optional small camera floater
    camera_floater = LLFloaterCamera::findSmallInstance();
    if (camera_floater)
    {
        camera_floater->updateItemsSelection();
        camera_floater->switchMode(CAMERA_CTRL_MODE_PRESETS);
    }
    // </FS:Ansariel>
}

void LLFloaterCamera::populatePresetCombo()
{
    LLPresetsManager::getInstance()->setPresetNamesInComboBox(PRESETS_CAMERA, mPresetCombo, EDefaultOptions::DEFAULT_HIDE);
    std::string active_preset_name = gSavedSettings.getString("PresetCameraActive");
    if (active_preset_name.empty())
    {
        gSavedSettings.setU32("CameraPresetType", CAMERA_PRESET_CUSTOM);
        updateItemsSelection();
        mPresetCombo->setLabel(getString("inactive_combo_text"));
    }
    else if ((ECameraPreset)gSavedSettings.getU32("CameraPresetType") == CAMERA_PRESET_CUSTOM)
    {
        mPresetCombo->selectByValue(active_preset_name);
    }
    else
    {
        mPresetCombo->setLabel(getString("inactive_combo_text"));
    }
    updateItemsSelection();
}

void LLFloaterCamera::onSavePreset()
{
    LLFloaterReg::hideInstance("delete_pref_preset", PRESETS_CAMERA);
    LLFloaterReg::hideInstance("load_pref_preset", PRESETS_CAMERA);

    // <FS:Ansariel> Preselect correct radio button on save camera presets floater
    //LLFloaterReg::showInstance("save_camera_preset");
    LLSD key;
    std::string current_preset = gSavedSettings.getString("PresetCameraActive");
    bool is_custom_preset = current_preset != "" && !LLPresetsManager::getInstance()->isDefaultCameraPreset(current_preset);
    key["index"] = is_custom_preset ? 1 : 0;
    LLFloaterReg::showInstance("save_camera_preset", key);
    // </FS:Ansariel>
}

void LLFloaterCamera::onCustomPresetSelected()
{
    std::string selected_preset = mPresetCombo->getSelectedItemLabel();
    if (getString("inactive_combo_text") != selected_preset)
    {
        switchToPreset(selected_preset);
    }
}

// <FS:Ansariel> Phototools support
void LLFloaterCamera::switchViews(ECameraControlMode mode)
{
    switch (mode)
    {
        case CAMERA_CTRL_MODE_PRESETS:
            getChildView("preset_views_list")->setVisible(true);
            getChildView("camera_modes_list")->setVisible(false);
            getChildView("zoom")->setVisible(false);
            getChild<LLButton>("presets_btn")->setToggleState(true);
            getChild<LLButton>("avatarview_btn")->setToggleState(false);
            getChild<LLButton>("pan_btn")->setToggleState(false);
            break;
        case CAMERA_CTRL_MODE_MODES:
            getChildView("preset_views_list")->setVisible(false);
            getChildView("camera_modes_list")->setVisible(true);
            getChildView("zoom")->setVisible(false);
            getChild<LLButton>("presets_btn")->setToggleState(false);
            getChild<LLButton>("avatarview_btn")->setToggleState(true);
            getChild<LLButton>("pan_btn")->setToggleState(false);
            break;
        case CAMERA_CTRL_MODE_PAN:
            getChildView("preset_views_list")->setVisible(false);
            getChildView("camera_modes_list")->setVisible(false);
            getChildView("zoom")->setVisible(true);
            getChild<LLButton>("presets_btn")->setToggleState(false);
            getChild<LLButton>("avatarview_btn")->setToggleState(false);
            getChild<LLButton>("pan_btn")->setToggleState(true);
            break;
        default:
            LL_WARNS() << "Tried to switch to unsupported mode: " << mode << LL_ENDL;
            break;
    }
}
// </FS:Ansariel>
