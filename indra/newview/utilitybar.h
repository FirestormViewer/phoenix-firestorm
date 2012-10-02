/**
 * @file utilitybar.h
 * @brief Helper class for the utility bar controls
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2012, Zi Ree @ Second Life
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

#ifndef UTILITYBAR_H
#define UTILITYBAR_H

#include "lleventtimer.h"
#include "llsingleton.h"
#include "lluuid.h"

class LLButton;

class UtilityBar
:	public LLSingleton<UtilityBar>,
	public LLEventTimer
{
	friend class LLSingleton<UtilityBar>;

	private:
		UtilityBar();
		~UtilityBar();

	public:
		void init();
		virtual BOOL tick();
		void setAOInterfaceButtonExpanded(bool expanded);

	protected:
		void onParcelStreamClicked();
		void onParcelMediaClicked();

		LLButton* mParcelStreamPlayButton;
		LLButton* mParcelMediaPlayButton;
		LLButton* mTalkButton;
		LLButton* mAOInterfaceButton;
};

#endif // UTILITYBAR_H
