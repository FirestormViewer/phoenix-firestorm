/**
* @file fsfloaterbantime.cpp
* @brief Floater for ban time dialog
*
* $LicenseInfo:firstyear=2012&license=fsviewerlgpl$
* Phoenix Firestorm Viewer Source Code
* Copyright (C) 2018, Liny Odell
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
#include "fsfloaterbantime.h"

#include "llfloaterreg.h"
#include "llspinctrl.h"

FSFloaterBanTime::FSFloaterBanTime(const LLSD& target)
:	LLFloater(target)
{
}

BOOL FSFloaterBanTime::postBuild()
{
	childSetAction("BanConfirmButton", boost::bind(&FSFloaterBanTime::onClickBan, this));
	childSetAction("BanCancelButton", boost::bind(&FSFloaterBanTime::onClickCancel, this));
	
	return TRUE;
}

FSFloaterBanTime* FSFloaterBanTime::show(select_callback_t callback, LLUUID id)
{
	FSFloaterBanTime* floater =
		LLFloaterReg::showTypedInstance<FSFloaterBanTime>("Ban duration");
	if (!floater)
	{
		LL_WARNS() << "Cannot instantiate avatar picker" << LL_ENDL;
		return NULL;
	}

	floater->mSelectionCallback = callback;
	floater->mAvatar_id = id;

	return floater;
}

void FSFloaterBanTime::onClickCancel()
{
	closeFloater();
}

void FSFloaterBanTime::onClickBan()
{
	if (mSelectionCallback)
	{
		LLSpinCtrl* timectrl = getChild<LLSpinCtrl>("ban_time");
		if (timectrl)
		{
			S32 time = timectrl->getValue().asInteger();
			if (time != 0)
			{
				LLDate now = LLDate::now();
				time = now.secondsSinceEpoch() + (time * 3600);
			}
			mSelectionCallback(mAvatar_id, time);
			timectrl->set(0.0);
		}
	}
	closeFloater();
}


