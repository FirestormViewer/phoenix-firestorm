/** 
 * @file quickprefs.h
 * @brief Quick preferences access panel for bottomtray
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 * Copyright (C) 2011, WoLf Loonie @ Second Life
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

class FloaterQuickPrefs : public LLTransientDockableFloater
{
	friend class LLFloaterReg;

private:
	FloaterQuickPrefs(const LLSD& key);
	~FloaterQuickPrefs();
	void onSunMoved(LLUICtrl* ctrl, void* userdata);

public:
	/*virtual*/ BOOL postBuild();
	/*virtual*/ void	draw();
	void syncControls();
	virtual void onOpen(const LLSD& key);

	void initCallbacks(void);
	void onChangeWaterPreset(LLUICtrl* ctrl);
	void onChangeSkyPreset(LLUICtrl* ctrl);
	void deactivateAnimator();
	void onClickSkyPrev();
	void onClickSkyNext();
	void onClickWaterPrev();
	void onClickWaterNext();
	void onClickRegionWL();

	// Phototools additions
	void refreshSettings();

private:

	LLMultiSliderCtrl*	mWLSunPos;
	LLComboBox*			mWLPresetsCombo;
	LLComboBox*			mWaterPresetsCombo;

	// Phototools additions
	LLCheckBoxCtrl*		mCtrlShaderEnable;
	LLCheckBoxCtrl*		mCtrlWindLight;
	LLCheckBoxCtrl*		mCtrlDeferred;
	LLCheckBoxCtrl*		mCtrlUseSSAO;
	LLCheckBoxCtrl*		mCtrlUseDoF;
	LLComboBox*			mCtrlShadowDetail;
	LLComboBox*			mCtrlReflectionDetail;
};
#endif