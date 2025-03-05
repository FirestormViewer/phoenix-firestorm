/**
 * @file quickprefs.h
 * @brief Quick preferences access panel for bottomtray
 *
 * $LicenseInfo:firstyear=2011&license=viewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (C) 2011, WoLf Loonie @ Second Life
 * Copyright (C) 2013, Zi Ree @ Second Life
 * Copyright (C) 2013, Ansariel Hiller @ Second Life
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
 * http://www.firestormviewer.org
 * $/LicenseInfo$
 */

#ifndef QUICKPREFS_H
#define QUICKPREFS_H

#include "llenvironment.h"
#include "lltransientdockablefloater.h"
#include "rlvdefines.h"

const std::string PHOTOTOOLS_FLOATER = "phototools";

class LLCheckBoxCtrl;
class LLComboBox;
class LLLayoutPanel;
class LLLayoutStack;
class LLLineEditor;
class LLSlider;
class LLSliderCtrl;
class LLSpinCtrl;
class LLTextBox;
class LLColorSwatchCtrl;

#define PRESET_NAME_REGION_DEFAULT "__Regiondefault__"
#define PRESET_NAME_DAY_CYCLE "__Day_Cycle__"
#define PRESET_NAME_NONE "__None__"

typedef enum e_quickpref_update_param
{
    QP_PARAM_SKY,
    QP_PARAM_WATER,
    QP_PARAM_DAYCYCLE
} EQuickPrefUpdateParam;

class FloaterQuickPrefs : public LLTransientDockableFloater
{
    friend class LLFloaterReg;

private:
    FloaterQuickPrefs(const LLSD& key);
    ~FloaterQuickPrefs();

    void selectSkyPreset(const LLSD& preset);
    void selectWaterPreset(const LLSD& preset);
    void selectDayCyclePreset(const LLSD& preset);

    bool isValidPreset(const LLSD& preset);
    void stepComboBox(LLComboBox* ctrl, bool forward);

    void initCallbacks();
    void loadPresets();
    void loadDayCyclePresets(const std::multimap<std::string, LLUUID>& daycycle_map);
    void loadSkyPresets(const std::multimap<std::string, LLUUID>& sky_map);
    void loadWaterPresets(const std::multimap<std::string, LLUUID>& water_map);

    void onChangeWaterPreset();
    void onChangeSkyPreset();
    void onChangeDayCyclePreset();
    void onClickSkyPrev();
    void onClickSkyNext();
    void onClickWaterPrev();
    void onClickWaterNext();
    void onClickDayCyclePrev();
    void onClickDayCycleNext();
    void onClickResetToRegionDefault();

    boost::signals2::connection mRlvBehaviorCallbackConnection;
    void updateRlvRestrictions(ERlvBehaviour behavior, ERlvParamType type);
    void enableWindlightButtons(bool enable);

    void setDefaultPresetsEnabled(bool enabled);

public:
    /*virtual*/ bool postBuild();
    virtual void onOpen(const LLSD& key);

    void setSelectedSky(const std::string& preset_name);
    void setSelectedWater(const std::string& preset_name);
    void setSelectedDayCycle(const std::string& preset_name);
    void setSelectedEnvironment();

    // Phototools additions
    void refreshSettings();
    bool getIsPhototools() const { return getName() == PHOTOTOOLS_FLOATER; };

    void dockToToolbarButton();

private:

    // Windlight controls
    LLComboBox*         mWLPresetsCombo;
    LLComboBox*         mWaterPresetsCombo;
    LLComboBox*         mDayCyclePresetsCombo;

    // Phototools additions
    LLCheckBoxCtrl*     mCtrlUseSSAO;
    LLCheckBoxCtrl*     mCtrlUseDoF;
    LLComboBox*         mCtrlShadowDetail;

    // Vignette UI controls
    LLSpinCtrl*         mSpinnerVignetteX;
    LLSpinCtrl*         mSpinnerVignetteY;
    LLSpinCtrl*         mSpinnerVignetteZ;
    LLSlider*           mSliderVignetteX;
    LLSlider*           mSliderVignetteY;
    LLSlider*           mSliderVignetteZ;

    LLSlider*           mSliderRenderShadowSplitExponentY;
    LLSpinCtrl*         mSpinnerRenderShadowSplitExponentY;

    LLSlider*           mSliderRenderShadowGaussianX;
    LLSlider*           mSliderRenderShadowGaussianY;
    LLSpinCtrl*         mSpinnerRenderShadowGaussianX;
    LLSpinCtrl*         mSpinnerRenderShadowGaussianY;

    LLSlider*           mSliderRenderSSAOEffectX;
    LLSpinCtrl*         mSpinnerRenderSSAOEffectX;

    LLSliderCtrl*       mAvatarZOffsetSlider;

    LLButton*           mBtnResetDefaults;

    LLSliderCtrl*       mMaxComplexitySlider;
    LLTextBox*          mMaxComplexityLabel;

    LLSettingsSky::ptr_t        mLiveSky;
    LLSettingsWater::ptr_t      mLiveWater;
    LLSettingsDay::ptr_t        mLiveDay;
    LLEnvironment::connection_t mEnvChangedConnection;

    // Vignette UI callbacks
    void onChangeVignetteX();
    void onChangeVignetteY();
    void onChangeVignetteZ();
    void onChangeVignetteSpinnerX();
    void onChangeVignetteSpinnerY();
    void onChangeVignetteSpinnerZ();
    void onClickResetVignetteX();
    void onClickResetVignetteY();
    void onClickResetVignetteZ();

    void onChangeRenderShadowSplitExponentSlider();
    void onChangeRenderShadowSplitExponentSpinner();
    void onClickResetRenderShadowSplitExponentY();

    void onChangeRenderShadowGaussianSlider();
    void onChangeRenderShadowGaussianSpinner();
    void onClickResetRenderShadowGaussianX();
    void onClickResetRenderShadowGaussianY();

    void onChangeRenderSSAOEffectSlider();
    void onChangeRenderSSAOEffectSpinner();
    void onClickResetRenderSSAOEffectX();

    // Restore Quickprefs Defaults
    void onClickRestoreDefaults();
    void loadSavedSettingsFromFile(const std::string& settings_path);
    void callbackRestoreDefaults(const LLSD& notification, const LLSD& response);

	// <FS:WW> // **Insert onAnimationSpeedChanged declaration HERE:**
	void onAnimationSpeedChanged(LLUICtrl* control, const LLSD& data);  // </FS:WW>
	
	// <FS:WW> // Animation Speed Reset Button Callback:
	void onClickResetAnimationSpeed(LLUICtrl* control, const LLSD& data); // </FS:WW>

    void onAvatarZOffsetSliderMoved();
    void onAvatarZOffsetFinalCommit();
    void updateAvatarZOffsetEditEnabled();
    void onRegionChanged();
    void onSimulatorFeaturesReceived(const LLUUID &region_id);
    void syncAvatarZOffsetFromPreferenceSetting();
    void updateMaxNonImpostors(const LLSD& newvalue);
    void updateMaxComplexity();
    void updateMaxComplexityLabel(const LLSD& newvalue);
		
	// <FS:WW> // Animation Speed UI Elements (Add these lines):
	LLSlider* mAnimationSpeedSlider;
	LLUICtrl*     mAnimationSpeedSpinner; // </FS:WW>

    boost::signals2::connection mRegionChangedSlot;

public:
    virtual void onClose(bool app_quitting);

protected:
    enum ControlType
    {
        ControlTypeCheckbox,
        ControlTypeText,
        ControlTypeSpinner,
        ControlTypeSlider,
        ControlTypeRadio,
        ControlTypeColor3,
        ControlTypeColor4
    };

    struct ControlEntry
    {
        LLPanel* panel;
        LLUICtrl* widget;
        LLTextBox* label_textbox;
        std::string label;
        ControlType type;
        bool integer;
        F32 min_value;
        F32 max_value;
        F32 increment;
        LLSD value;     // used temporary during updateControls()
    };

    // XUI definition of a control entry in quick_preferences.xml
    struct QuickPrefsXMLEntry : public LLInitParam::Block<QuickPrefsXMLEntry>
    {
        Mandatory<std::string>  control_name;
        Mandatory<std::string>  label;
        Optional<std::string>   translation_id;
        Mandatory<U32>          control_type;
        Mandatory<bool>         integer;
        Mandatory<F32>          min_value;  // "min" is frowned upon by a braindead windows include
        Mandatory<F32>          max_value;  // "max" see "min"
        Mandatory<F32>          increment;

        QuickPrefsXMLEntry();
    };

    // overall XUI container in quick_preferences.xml
    struct QuickPrefsXML : public LLInitParam::Block<QuickPrefsXML>
    {
        Multiple<QuickPrefsXMLEntry> entries;

        QuickPrefsXML();
    };

    // internal list of user defined controls to display
    typedef std::map<std::string,ControlEntry> control_list_t;
    control_list_t mControlsList;

    // order of the controls on the user interface
    std::list<std::string> mControlsOrder;
    // list of layout_panel slots to put our options in
    std::list<LLLayoutPanel*> mOrderingSlots;

    // pointer to the layout_stack where the controls will be inserted
    LLLayoutStack* mOptionsStack;

    // currently selected for editing
    std::string mSelectedControl;

    // editor control widgets
    LLLineEditor* mControlLabelEdit;
    LLComboBox* mControlNameCombo;
    LLComboBox* mControlTypeCombo;
    LLCheckBoxCtrl* mControlIntegerCheckbox;
    LLSpinCtrl* mControlMinSpinner;
    LLSpinCtrl* mControlMaxSpinner;
    LLSpinCtrl* mControlIncrementSpinner;

    // returns the path to the quick_preferences.xml file. in save mode it will
    // always return the user_settings path, if not in save mode, it will return
    // the app_settings path in case the user_settings path does not (yet) exist
    std::string getSettingsPath(bool save_mode);

    // adds a new control and returns a pointer to the chosen widget
    LLUICtrl* addControl(const std::string& controlName, const std::string& controlLabel, LLView* slot = NULL, ControlType type = ControlTypeRadio, bool integer = false, F32 min_value = -1000000.0f, F32 max_value = 1000000.0f, F32 increment = 0.0f);
    // removes a control
    void removeControl(const std::string& controlName, bool remove_slot = true);
    // updates a single control
    void updateControl(const std::string& controlName, ControlEntry& entry);

    // make this control the currently selected one
    void selectControl(std::string controlName);

    // toggles edit mode
    void onDoubleClickLabel(LLUICtrl* ctrl, LLPanel* panel);
    // selects a control in edit mode
    void onClickLabel(LLUICtrl* ctrl, LLPanel* panel);

    // will save settings when leaving edit mode
    void onEditModeChanged();
    // updates the control when a value in the edit panel was changed by the user
    void onValuesChanged();

    void onAddNewClicked();
    void onRemoveClicked(LLUICtrl* ctrl, LLPanel* panel);
    void onAlphaChanged(LLUICtrl* ctrl, LLColorSwatchCtrl* color_swatch);
    void onMoveUpClicked();
    void onMoveDownClicked();

    // swaps two controls, used for move up and down
    void swapControls(const std::string& control1, const std::string& control2);

    bool hasControl( std::string const &aName ) const
    { return mControlsList.end() != mControlsList.find( aName ); }

};
#endif // QUICKPREFS_H
