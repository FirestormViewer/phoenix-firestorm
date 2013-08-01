/**
 * @file fsfloatervolumecontrols.cpp
 * @brief Class for the standalone volume controls floater in Firestorm
 *
 * $LicenseInfo:firstyear=2012&license=viewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (c) 2013 Ansariel Hiller @ Second Life
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
 * The Phoenix Firestorm Project, Inc., 1831 Oakwood Drive, Fairmont, Minnesota 56031-3225 USA
 * http://www.firestormviewer.org
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "fsfloatervolumecontrols.h"

#include "llcheckboxctrl.h"
#include "llfloaterreg.h"
#include "llpanel.h"
#include "lltabcontainer.h"

#include "llfloaterpreference.h"
#include "utilitybar.h"
#include "llviewercontrol.h"

FSFloaterVolumeControls::FSFloaterVolumeControls(const LLSD& key)
:	LLFloater(key)
{
	//<FS:KC> Handled centrally now
	/*
	mCommitCallbackRegistrar.add("Vol.GoAudioPrefs", boost::bind(&FSFloaterVolumeControls::onAudioPrefsButtonClicked, this));
	mCommitCallbackRegistrar.add("Vol.setControlFalse", boost::bind(&FSFloaterVolumeControls::setControlFalse, this, _2));
	mCommitCallbackRegistrar.add("Vol.SetSounds", boost::bind(&FSFloaterVolumeControls::setSounds, this));
	*/
}

FSFloaterVolumeControls::~FSFloaterVolumeControls()
{
}

BOOL FSFloaterVolumeControls::postBuild()
{
	return TRUE;
}

// virtual
void FSFloaterVolumeControls::handleVisibilityChange(BOOL new_visibility)
{
	UtilityBar::instance().setVolumeControlsButtonExpanded(new_visibility);
	LLFloater::handleVisibilityChange(new_visibility);
}

//<FS:KC> Handled centrally now
/*
void FSFloaterVolumeControls::onAudioPrefsButtonClicked()
{
	// bring up the prefs floater
	LLFloaterPreference* prefsfloater = LLFloaterReg::showTypedInstance<LLFloaterPreference>("preferences");
	if (prefsfloater)
	{
		// grab the 'audio' panel from the preferences floater and
		// bring it the front!
		LLTabContainer* tabcontainer = prefsfloater->getChild<LLTabContainer>("pref core");
		LLPanel* audiopanel = prefsfloater->getChild<LLPanel>("audio");
		if (tabcontainer && audiopanel)
		{
			tabcontainer->selectTabPanel(audiopanel);
		}
	}
}

void FSFloaterVolumeControls::setControlFalse(const LLSD& user_data)
{
	std::string control_name = user_data.asString();
	LLControlVariable* control = findControl(control_name);
	
	if (control)
	{
		control->set(LLSD(FALSE));
	}
}

void FSFloaterVolumeControls::setSounds()
{
	// Disable Enable gesture/collisions sounds checkbox if the master sound is disabled
	// or if sound effects are disabled.
	getChild<LLCheckBoxCtrl>("gesture_audio_play_btn")->setEnabled(!gSavedSettings.getBOOL("MuteSounds"));
	getChild<LLCheckBoxCtrl>("collisions_audio_play_btn")->setEnabled(!gSavedSettings.getBOOL("MuteSounds"));
}
*/
