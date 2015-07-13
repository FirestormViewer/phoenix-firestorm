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
#include "lltooldraganddrop.h"

static LLDefaultChildRegistry::Register<FSScrollListCtrl> r("fs_scroll_list");

FSScrollListCtrl::FSScrollListCtrl(const Params& p)
:	LLScrollListCtrl(p),
	mContextMenu(NULL),
	mDesiredLineHeight(p.desired_line_height),
	mContentType(p.content_type),
	mHandleDaDCallback(NULL)
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

BOOL FSScrollListCtrl::handleRightMouseDown(S32 x, S32 y, MASK mask)
{
	BOOL handled = FALSE;

	// If we set our own context menu handler (mContextMenu != NULL), skip the
	// event handler in LLScrollListCtrl and perform our own context menu action.
	// If we didn't set our own context menu handler, fall back to the default
	// event handler, which will pop up the default context menu for a LLScrollListCtrl.
	if (mContextMenu)
	{
		handled = LLUICtrl::handleRightMouseDown(x, y, mask);

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
	else
	{
		handled = LLScrollListCtrl::handleRightMouseDown(x, y, mask);
	}
	return handled;
}

BOOL FSScrollListCtrl::handleMouseDown(S32 x, S32 y, MASK mask)
{
	if (mContentType == FSScrollListCtrl::AGENTS)
	{
		gFocusMgr.setMouseCapture(this);

		S32 screen_x;
		S32 screen_y;
		localPointToScreen(x, y, &screen_x, &screen_y);
		LLToolDragAndDrop::getInstance()->setDragStart(screen_x, screen_y);
	}

	return LLScrollListCtrl::handleMouseDown(x, y, mask);
}

BOOL FSScrollListCtrl::handleMouseUp(S32 x, S32 y, MASK mask)
{
	if (mContentType == FSScrollListCtrl::AGENTS)
	{
		if (hasMouseCapture())
		{
			gFocusMgr.setMouseCapture(NULL);
		}
	}

	return LLScrollListCtrl::handleMouseUp(x, y, mask);
}

BOOL FSScrollListCtrl::handleHover(S32 x, S32 y, MASK mask)
{
	if (mContentType == FSScrollListCtrl::AGENTS)
	{
		bool handled = hasMouseCapture();
		if (handled)
		{
			S32 screen_x;
			S32 screen_y;
			localPointToScreen(x, y, &screen_x, &screen_y);

			if (LLToolDragAndDrop::getInstance()->isOverThreshold(screen_x, screen_y))
			{
				// First, create the global drag and drop object
				std::vector<EDragAndDropType> types;
				uuid_vec_t cargo_ids;
				typedef std::vector<LLScrollListItem*> item_vec_t;
				item_vec_t selected_items = getAllSelected();
				for (item_vec_t::const_iterator it = selected_items.begin(); it != selected_items.end(); ++it)
				{
					cargo_ids.push_back((*it)->getUUID());
				}
				types.resize(cargo_ids.size(), DAD_PERSON);
				LLToolDragAndDrop::ESource src = LLToolDragAndDrop::SOURCE_PEOPLE;
				LLToolDragAndDrop::getInstance()->beginMultiDrag(types, cargo_ids, src);
			}
		}
		else
		{
			handled = LLScrollListCtrl::handleHover(x, y, mask);
		}

		return handled;
	}
	else
	{
		return LLScrollListCtrl::handleHover(x, y, mask);
	}
}

//virtual

BOOL FSScrollListCtrl::handleDragAndDrop(S32 x, S32 y, MASK mask, BOOL drop,
											EDragAndDropType cargo_type,
											void* cargo_data,
											EAcceptance* accept,
											std::string& tooltip_msg)
{
	if (mHandleDaDCallback)
	{
		return mHandleDaDCallback(x, y, mask, drop, cargo_type, cargo_data, accept, tooltip_msg);
	}

	return FALSE;
	
}

