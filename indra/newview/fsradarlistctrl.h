/** 
 * @file fs_radarlistctr.h
 * @brief A radar-specific scrolllist implementation, so we can subclass custom methods.
 *
 * $LicenseInfo:firstyear=2003&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
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
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#ifndef LL_LLRADARLISTCTRL_H
#define LL_LLRADARLISTCTRL_H

#include <set>
#include "llscrolllistctrl.h"
#include "lllistcontextmenu.h"

class LLRadarListCtrl
: public LLScrollListCtrl, public LLInstanceTracker<LLRadarListCtrl>
{
public:
	
	struct Params : public LLInitParam::Block<Params, LLScrollListCtrl::Params>
	{
		// behavioral flags
		Optional<bool>	multi_select,
		commit_on_keyboard_movement,
		mouse_wheel_opaque;
		
		// display flags
		Optional<bool>	has_border,
		draw_heading,
		draw_stripes,
		background_visible,
		scroll_bar_bg_visible;
		
		// layout
		Optional<S32>	column_padding,
		page_lines,
		heading_height;
		
		// sort and search behavior
		Optional<S32>	search_column,
		sort_column;
		Optional<bool>	sort_ascending;
		
		// colors
		Optional<LLUIColor>	fg_unselected_color,
		fg_selected_color,
		bg_selected_color,
		fg_disable_color,
		bg_writeable_color,
		bg_readonly_color,
		bg_stripe_color,
		hovered_color,
		highlighted_color,
		scroll_bar_bg_color;
		
		Optional<Contents> contents;
		
		Optional<LLViewBorder::Params> border;
		
		Params();
	};	
	
	void	setContextMenu(LLListContextMenu* menu) { mContextMenu = menu; }
	BOOL	handleRightMouseDown(S32 x, S32 y, MASK mask);
	
	
protected:
	LLRadarListCtrl(const Params&);
	friend class LLUICtrlFactory;

private:
	LLListContextMenu*	mContextMenu;
	
};

#endif
