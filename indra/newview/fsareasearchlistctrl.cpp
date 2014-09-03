/** 
 * @file fsareasearchlistctrl.cpp
 * @brief A area search-specific implementation of scrolllist
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

#include "llviewerprecompiledheaders.h"

#include "fsareasearchlistctrl.h"
#include "llscrolllistitem.h"

static LLDefaultChildRegistry::Register<FSAreaSearchListCtrl> r("area_search_list");

FSAreaSearchListCtrl::FSAreaSearchListCtrl(const Params& p)
:	FSScrollListCtrl(p)
{
}

BOOL FSAreaSearchListCtrl::handleRightMouseDown(S32 x, S32 y, MASK mask)
{
	BOOL handled = LLUICtrl::handleRightMouseDown(x, y, mask);
	if (mContextMenu)
	{
		std::vector<LLScrollListItem*> selected_items = getAllSelected();
		if (selected_items.size() > 1)
		{
			uuid_vec_t selected_uuids;
			for (std::vector<LLScrollListItem*>::iterator it = selected_items.begin(); it != selected_items.end(); ++it)
			{
				selected_uuids.push_back((*it)->getUUID());
			}
			mContextMenu->show(this, selected_uuids, x, y);
		}
		else
		{
			LLScrollListItem* hit_item = hitItem(x, y);
			if (hit_item)
			{
				LLUUID val = hit_item->getValue();
				selectByID(val);
				uuid_vec_t selected_uuids;
				selected_uuids.push_back(val);
				mContextMenu->show(this, selected_uuids, x, y);
			}
		}
	}
	return handled;
}
