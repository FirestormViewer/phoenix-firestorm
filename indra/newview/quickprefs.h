/** 
 * @file quickprefs.h
 * @brief Quick preferences access panel for bottomtray
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
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
 * $/LicenseInfo$
 */

#ifndef QUICKPREFS_H
#define QUICKPREFS_H

#include "lltransientdockablefloater.h"
#include "llwlparamset.h"
#include "llmultisliderctrl.h"
#include "llcombobox.h"
#include "rlvdefines.h"

const std::string PHOTOTOOLS_FLOATER = "phototools";

// <FS:Zi> Dynamic quick prefs
class LLLayoutStack;
class LLLayoutPanel;
class LLSpinCtrl;
// </FS:Zi>

#define PRESET_NAME_REGION_DEFAULT "__Regiondefault__"
#define PRESET_NAME_SKY_DAY_CYCLE "__Sky_Day_Cycle__"
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
	void onSunMoved();

	void selectSkyPreset(const std::string& preset_name);
	void selectWaterPreset(const std::string& preset_name);
	void selectDayCyclePreset(const std::string& preset_name);

	bool isValidPresetName(const std::string& preset_name);
	std::string stepComboBox(LLComboBox* ctrl, bool forward);

	void initCallbacks();
	void loadPresets();

	void onChangeUseRegionWindlight();
	void onChangeWaterPreset();
	void onChangeSkyPreset();
	void onChangeDayCyclePreset();
	void deactivateAnimator();
	void onClickSkyPrev();
	void onClickSkyNext();
	void onClickWaterPrev();
	void onClickWaterNext();
	void onClickDayCyclePrev();
	void onClickDayCycleNext();
	void onClickResetToRegionDefault();

	void updateRlvRestrictions(ERlvBehaviour behavior, ERlvParamType type);
	void enableWindlightButtons(BOOL enable);

public:
	/*virtual*/ BOOL postBuild();
	/*virtual*/ void draw();
	virtual void onOpen(const LLSD& key);


	static void updateParam(EQuickPrefUpdateParam param, const LLSD& value);
	void setSelectedSky(const std::string& preset_name);
	void setSelectedWater(const std::string& preset_name);
	void setSelectedDayCycle(const std::string& preset_name);

	// Phototools additions
	void refreshSettings();
	bool getIsPhototools() const { return getName() == PHOTOTOOLS_FLOATER; };

private:

	LLMultiSliderCtrl*	mWLSunPos;
	LLComboBox*			mWLPresetsCombo;
	LLComboBox*			mWaterPresetsCombo;
	LLComboBox*			mDayCyclePresetsCombo;
	LLCheckBoxCtrl*		mUseRegionWindlight;

	// Phototools additions
	LLCheckBoxCtrl*		mCtrlShaderEnable;
	LLCheckBoxCtrl*		mCtrlWindLight;
	LLCheckBoxCtrl*		mCtrlDeferred;
	LLCheckBoxCtrl*		mCtrlUseSSAO;
	LLCheckBoxCtrl*		mCtrlUseDoF;
	LLComboBox*			mCtrlShadowDetail;
	LLComboBox*			mCtrlReflectionDetail;

// <FS:Zi> Dynamic quick prefs
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
		BOOL integer;
		F32 min_value;
		F32 max_value;
		F32 increment;
		LLSD value;		// used temporary during updateControls()
	};

	// XUI definition of a control entry in quick_preferences.xml
	struct QuickPrefsXMLEntry : public LLInitParam::Block<QuickPrefsXMLEntry>
	{
		Mandatory<std::string>	control_name;
		Mandatory<std::string>	label;
		Mandatory<U32>			control_type;
		Mandatory<BOOL>			integer;
		Mandatory<F32>			min_value;	// "min" is frowned upon by a braindead windows include
		Mandatory<F32>			max_value;	// "max" see "min"
		Mandatory<F32>			increment;

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
	std::string getSettingsPath(BOOL save_mode);

	// adds a new control and returns a pointer to the chosen widget
	LLUICtrl* addControl(const std::string& controlName,const std::string& controlLabel,LLView* slot=NULL,ControlType type=ControlTypeRadio,BOOL integer=FALSE,F32 min_value=-1000000.0f,F32 max_value=1000000.0,F32 increment=0.0f);
	// removes a control
	void removeControl(const std::string& controlName,BOOL remove_slot=TRUE);
	// updates a single control
	void updateControl(const std::string& controlName,ControlEntry& entry);

	// make this control the currently selected one
	void selectControl(std::string controlName);

	// toggles edit mode
	void onDoubleClickLabel(LLUICtrl* ctrl,void* userdata);	// userdata is the associated panel
	// selects a control in edit mode
	void onClickLabel(LLUICtrl* ctrl,void* userdata);		// userdata is the associated panel

	// will save settings when leaving edit mode
	void onEditModeChanged();
	// updates the control when a value in the edit panel was changed by the user
	void onValuesChanged();

	void onAddNewClicked();
	void onRemoveClicked(LLUICtrl* ctrl,void* userdata);	// userdata is the associated panel
	void onAlphaChanged(LLUICtrl* ctrl,void* userdata);		// userdata is the associated color swatch
	void onMoveUpClicked();
	void onMoveDownClicked();

	// swaps two controls, used for move up and down
	void swapControls(const std::string& control1,const std::string& control2);
// </FS:Zi>
};
#endif
