/**
 * @file llpanellandaudio.h
 * @brief Allows configuration of "audio" for a land parcel.
 *   
 *
 * $LicenseInfo:firstyear=2007&license=viewerlgpl$
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

#ifndef LLPANELLANDAUDIO_H
#define LLPANELLANDAUDIO_H

// <FS:CR> FIRE-593 - We use a combobox now, not a lineeditor
//#include "lllineeditor.h"
#include "llcombobox.h"
#include "llbutton.h"
// </FS:CR>
#include "llpanel.h"
#include "llparcelselection.h"
#include "lluifwd.h"	// widget pointer types

class LLPanelLandAudio
	:	public LLPanel
{
public:
	LLPanelLandAudio(LLSafeHandle<LLParcelSelection>& parcelp);
	/*virtual*/ ~LLPanelLandAudio();
	/*virtual*/ BOOL postBuild();
	void refresh();

private:
	static void onCommitAny(LLUICtrl* ctrl, void *userdata);
// <FS:CR> FIRE-593 - Add/remove streams from the list
	static void onBtnStreamAdd(LLUICtrl* ctrl, void *userdata);
	static void onBtnStreamDelete(LLUICtrl* ctrl, void *userdata);
// </FS:CR>

private:
	LLCheckBoxCtrl* mCheckSoundLocal;
	LLCheckBoxCtrl* mCheckParcelEnableVoice;
	LLCheckBoxCtrl* mCheckEstateDisabledVoice;
	LLCheckBoxCtrl* mCheckParcelVoiceLocal;
// <FS:CR> FIRE-593 - Use a combobox, also add buttons so we can add/remove items from it.
	//LLLineEditor*	mMusicURLEdit;
	LLComboBox*	mMusicURLEdit;
	LLButton* mBtnStreamAdd;
	LLButton* mBtnStreamDelete;
// </FS:CR>
	LLCheckBoxCtrl* mMusicUrlCheck;
	LLCheckBoxCtrl* mCheckAVSoundAny;
	LLCheckBoxCtrl* mCheckAVSoundGroup;

	LLSafeHandle<LLParcelSelection>&	mParcel;
};

#endif
