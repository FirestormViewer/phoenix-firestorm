/** 
 * @file fsscrolllistctrl.cpp
 * @brief A Firestorm specific implementation of scrolllist
 *
 * $LicenseInfo:firstyear=2014&license=viewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (c) 2014 Ansariel Hiller @ Second Life
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

#include "fsscrolllistctrl.h"

FSScrollListCtrl::FSScrollListCtrl(const Params& p)
:	LLScrollListCtrl(p),
	mContextMenu(NULL),
	mDesiredLineHeight(p.desired_line_height)
{
}

void FSScrollListCtrl::refreshLineHeight()
{
	if (mDesiredLineHeight > -1)
	{
		setLineHeight(mDesiredLineHeight);
	}
	else
	{
		updateLineHeight();
	}
	updateLayout();
}
