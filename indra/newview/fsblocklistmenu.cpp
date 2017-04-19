/** 
 * @file fsblocklistmenu.cpp
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

#include "llviewerprecompiledheaders.h"

#include "fsblocklistmenu.h"
#include "fspanelblocklist.h"
#include "lluictrl.h"

FSBlockListMenu gFSBlockListMenu;

LLContextMenu* FSBlockListMenu::createMenu()
{
	LLUICtrl::CommitCallbackRegistry::ScopedRegistrar registrar;
	LLUICtrl::EnableCallbackRegistry::ScopedRegistrar enable_registrar;

	registrar.add("Block.Action",			boost::bind(&FSBlockListMenu::onContextMenuItemClick,  this, _2));
	enable_registrar.add("Block.Check",		boost::bind(&FSBlockListMenu::onContextMenuItemCheck, this, _2));
	enable_registrar.add("Block.Enable",	boost::bind(&FSBlockListMenu::onContextMenuItemEnable, this, _2));
	enable_registrar.add("Block.Visible",	boost::bind(&FSBlockListMenu::onContextMenuItemVisible, this, _2));

	return createFromFile("menu_fs_block_list.xml");
}

void FSBlockListMenu::show(LLView* spawning_view, const uuid_vec_t& uuids, S32 x, S32 y)
{
	mSpawningCtrl = spawning_view;
	LLListContextMenu::show(spawning_view, uuids, x, y);
}

void FSBlockListMenu::onContextMenuItemClick(const LLSD& userdata)
{
	if (mSpawningCtrl)
	{
		FSPanelBlockList* blocklist = mSpawningCtrl->getParentByType<FSPanelBlockList>();
		if (blocklist)
		{
			blocklist->onCustomAction(userdata);
		}
	}
}

bool FSBlockListMenu::onContextMenuItemCheck(const LLSD& userdata)
{
	if (mSpawningCtrl)
	{
		FSPanelBlockList* blocklist = mSpawningCtrl->getParentByType<FSPanelBlockList>();
		if (blocklist)
		{
			return blocklist->isActionChecked(userdata);
		}
	}

	return false;
}

bool FSBlockListMenu::onContextMenuItemEnable(const LLSD& userdata)
{
	if (mSpawningCtrl)
	{
		FSPanelBlockList* blocklist = mSpawningCtrl->getParentByType<FSPanelBlockList>();
		if (blocklist)
		{
			return blocklist->isActionEnabled(userdata);
		}
	}

	return false;
}

bool FSBlockListMenu::onContextMenuItemVisible(const LLSD& userdata)
{
	if (mSpawningCtrl)
	{
		FSPanelBlockList* blocklist = mSpawningCtrl->getParentByType<FSPanelBlockList>();
		if (blocklist)
		{
			return blocklist->isActionVisible(userdata);
		}
	}

	return false;
}
