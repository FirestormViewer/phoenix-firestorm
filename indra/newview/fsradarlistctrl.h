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

#include "llscrolllistctrl.h"

class LLListContextMenu;

class FSRadarListCtrl
: public LLScrollListCtrl, public LLInstanceTracker<FSRadarListCtrl>
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
	FSRadarListCtrl(const Params&);
	friend class LLUICtrlFactory;

private:
	LLListContextMenu*	mContextMenu;
	
};

#endif // FS_RADARLISTCTRL_H
