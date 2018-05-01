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

#ifndef FS_FLOATERBANTIME_H
#define FS_FLOATERBANTIME_H

#include "llfloater.h"

class FSFloaterBanTime : public LLFloater
{
public:
	FSFloaterBanTime(const LLSD& target);
	BOOL postBuild();
	typedef boost::function<void(const LLUUID&, const S32 time)> select_callback_t;
	static FSFloaterBanTime* show(select_callback_t callback, LLUUID id);
	
private:
	~FSFloaterBanTime(){};
	void onClickBan();
	void onClickCancel();
	LLUUID mAvatar_id;
	select_callback_t mSelectionCallback;
};

#endif // FS_FLOATERBANTIME_H
