/** 
 * @file fsblocklistmenu.h
 * @brief Menu used by the block list
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

#ifndef FS_BLOCKLISTMENU_H
#define FS_BLOCKLISTMENU_H

#include "lllistcontextmenu.h"

class FSBlockListMenu : public LLListContextMenu
{
public:
	FSBlockListMenu()
		: LLListContextMenu(),
		mSpawningCtrl(NULL)
	{ }
	virtual ~FSBlockListMenu() { };

	/*virtual*/ LLContextMenu* createMenu();
	/*virtual*/ void show(LLView* spawning_view, const uuid_vec_t& uuids, S32 x, S32 y);

private:
	void onContextMenuItemClick(const LLSD& userdata);
	bool onContextMenuItemCheck(const LLSD& userdata);
	bool onContextMenuItemEnable(const LLSD& userdata);
	bool onContextMenuItemVisible(const LLSD& userdata);

	LLView* mSpawningCtrl;
};

extern FSBlockListMenu gFSBlockListMenu;

#endif // FS_BLOCKLISTMENU_H
