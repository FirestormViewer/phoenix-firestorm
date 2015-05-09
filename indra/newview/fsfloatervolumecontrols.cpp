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
#include "llpanel.h"

#include "utilitybar.h"
#include "llviewercontrol.h"

FSFloaterVolumeControls::FSFloaterVolumeControls(const LLSD& key)
:	LLFloater(key)
{
}

FSFloaterVolumeControls::~FSFloaterVolumeControls()
{
}

BOOL FSFloaterVolumeControls::postBuild()
{
	// <FS:PP> FIRE-9856: Mute sound effects disable plays sound from collisions and plays sound from gestures checkbox not disable after restart/relog
	bool mute_sound_effects = gSavedSettings.getBOOL("MuteSounds");
	bool mute_all_sounds = gSavedSettings.getBOOL("MuteAudio");
	getChild<LLCheckBoxCtrl>("gesture_audio_play_btn")->setEnabled(!(mute_sound_effects || mute_all_sounds));
	getChild<LLCheckBoxCtrl>("collisions_audio_play_btn")->setEnabled(!(mute_sound_effects || mute_all_sounds));
	// </FS:PP>
	return TRUE;
}

// virtual
void FSFloaterVolumeControls::onVisibilityChange(BOOL new_visibility)
{
	UtilityBar::instance().setVolumeControlsButtonExpanded(new_visibility);
	LLFloater::onVisibilityChange(new_visibility);
}
