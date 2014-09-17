/** 
 * @file fsareasearchmenu.cpp
 * @brief Menu used by area search
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

#include "fsareasearchmenu.h"

#include "fsareasearch.h"
#include "llfloaterreg.h"

FSAreaSearchMenu gFSAreaSearchMenu;

LLContextMenu* FSAreaSearchMenu::createMenu()
{
	LLUICtrl::CommitCallbackRegistry::ScopedRegistrar registrar;
	LLUICtrl::EnableCallbackRegistry::ScopedRegistrar enable_registrar;

	registrar.add("AreaSearch.Action", boost::bind(&FSAreaSearchMenu::onContextMenuItemClick, this, _2));
	enable_registrar.add("AreaSearch.Enable", boost::bind(&FSAreaSearchMenu::onContextMenuItemEnable, this, _2));

	return createFromFile("menu_fs_area_search.xml");
}

void FSAreaSearchMenu::onContextMenuItemClick(const LLSD& userdata)
{
	FSAreaSearch* search = LLFloaterReg::findTypedInstance<FSAreaSearch>("area_search");
	if (search && search->getPanelList())
	{
		search->getPanelList()->onContextMenuItemClick(userdata);
	}
}

bool FSAreaSearchMenu::onContextMenuItemEnable(const LLSD& userdata)
{
	FSAreaSearch* search = LLFloaterReg::findTypedInstance<FSAreaSearch>("area_search");
	if (search && search->getPanelList())
	{
		return search->getPanelList()->onContextMenuItemEnable(userdata);
	}
	return false;
}
