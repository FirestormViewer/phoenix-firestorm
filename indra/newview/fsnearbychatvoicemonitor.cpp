/**
 * @file fsnearbychatvoicemonitor.cpp
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

#include "llviewerprecompiledheaders.h"

#include "fsnearbychatvoicemonitor.h"
#include "llvoiceclient.h"

static LLDefaultChildRegistry::Register<FSNearbyChatVoiceControl> r("nearby_chat_voice_monitor");

FSNearbyChatVoiceControl::Params::Params() :
	voice_monitor_padding("voice_monitor_padding"),
	nearby_voice_monitor("nearby_voice_monitor")
{}

FSNearbyChatVoiceControl::FSNearbyChatVoiceControl(const FSNearbyChatVoiceControl::Params& p) :
	LLNearbyChatControl(p),
	mVoiceMonitor(NULL),
	mOriginalTextpadLeft(p.text_pad_left),
	mOriginalTextpadRight(p.text_pad_right),
	mVoiceMonitorPadding(p.voice_monitor_padding),
	mVoiceMonitorVisible(p.nearby_voice_monitor.visible)
{
	S32 vm_top = p.nearby_voice_monitor.top_pad + p.nearby_voice_monitor.rect.height;
	S32 vm_right = p.rect.right - p.rect.left;
	S32 vm_left = p.rect.right - p.rect.left - p.nearby_voice_monitor.rect.width;
	LLRect vm_rect(vm_left, vm_top, vm_right, p.nearby_voice_monitor.top_pad);

	NearbyVoiceMonitor::Params voice_monitor_params(p.nearby_voice_monitor);
	voice_monitor_params.rect(vm_rect);
	
	mVoiceMonitor = LLUICtrlFactory::create<NearbyVoiceMonitor>(voice_monitor_params);
	addChild(mVoiceMonitor);
}

void FSNearbyChatVoiceControl::draw()
{
	LLVoiceClient* voice_client = LLVoiceClient::getInstance();
	bool new_visibility_status = (voice_client->voiceEnabled() && voice_client->isVoiceWorking());
	if (mVoiceMonitorVisible != new_visibility_status)
	{
		mVoiceMonitorVisible = new_visibility_status;
		if (mVoiceMonitorVisible)
		{
			setTextPadding(mOriginalTextpadLeft, mOriginalTextpadRight + mVoiceMonitor->getRect().getWidth() + mVoiceMonitorPadding);
			mVoiceMonitor->setVisible(TRUE);
		}
		else
		{
			setTextPadding(mOriginalTextpadLeft, mOriginalTextpadRight);
			mVoiceMonitor->setVisible(FALSE);
		}
	}
	LLNearbyChatControl::draw();
}
