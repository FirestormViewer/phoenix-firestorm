/** 
 * @file fsradarlistctrl.h
 * @brief A radar-specific scrolllist implementation, so we can subclass custom methods.
 *
 * $LicenseInfo:firstyear=2011&license=viewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (c) 2011 Arrehn Oberlander
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

#ifndef FS_RADARLISTCTRL_H
#define FS_RADARLISTCTRL_H

#include "fsscrolllistctrl.h"

class FSRadarListCtrl
: public FSScrollListCtrl, public LLInstanceTracker<FSRadarListCtrl>
{
public:

	struct Params : public LLInitParam::Block<Params, FSScrollListCtrl::Params>
	{
		Params()
		{}
	};

	virtual ~FSRadarListCtrl() {}
	/*virtual*/ BOOL handleRightMouseDown(S32 x, S32 y, MASK mask);

protected:
	friend class LLUICtrlFactory;
	FSRadarListCtrl(const Params&);
};

#endif // FS_RADARLISTCTRL_H
