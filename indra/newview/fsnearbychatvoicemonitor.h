/**
 * @file fsnearbychatvoicemonitor.h
 * @brief Nearby chat bar with integrated nearby voice monitor
 *
 * $LicenseInfo:firstyear=2012&license=viewerlgpl$
 * Copyright (c) 2012 Ansariel Hiller @ Second Life
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
 */

#ifndef FS_NEARBYCHATVOICEMONITOR_H
#define FS_NEARBYCHATVOICEMONITOR_H

#include "fsnearbychatcontrol.h"
#include "lloutputmonitorctrl.h"

class LLUICtrlFactory;

class FSNearbyChatVoiceControl : public FSNearbyChatControl
{

public:
	struct Params : public LLInitParam::Block<Params, FSNearbyChatControl::Params>
	{
		Optional<S32>							voice_monitor_padding;
		Optional<NearbyVoiceMonitor::Params>	nearby_voice_monitor;

		Params();
	};

	FSNearbyChatVoiceControl(const Params& p);

	void draw();

protected:
	friend class LLUICtrlFactory;

	NearbyVoiceMonitor*	mVoiceMonitor;
	S32					mOriginalTextpadLeft;
	S32					mOriginalTextpadRight;
	S32					mVoiceMonitorPadding;
	bool				mVoiceMonitorVisible;
};

#endif // FS_NEARBYCHATVOICEMONITOR_H
