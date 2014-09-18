/** 
 * @file fsblocklistctrl.h
 * @brief A block list-specific implementation of scrolllist
 *
 * $LicenseInfo:firstyear=2014&license=viewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (c) 2014 Ansariel Hiller
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

#ifndef FS_BLOCKLISTCTRL_H
#define FS_BLOCKLISTCTRL_H

#include "fsscrolllistctrl.h"

class FSBlockListCtrl
	: public FSScrollListCtrl, public LLInstanceTracker<FSBlockListCtrl>
{
public:

	struct Params : public LLInitParam::Block<Params, FSScrollListCtrl::Params>
	{
		Params()
		{}
	};

	BOOL	handleRightMouseDown(S32 x, S32 y, MASK mask);

protected:
	FSBlockListCtrl(const Params&);
	virtual ~FSBlockListCtrl() {}
	friend class LLUICtrlFactory;
};

#endif // FS_BLOCKLISTCTRL_H
