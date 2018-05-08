/** 
 * @file fsradarmenu.h
 * @brief Menu used by Firestorm radar
 *
 * $LicenseInfo:firstyear=2013&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (c) 2013 Ansariel Hiller @ Second Life
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

#ifndef FS_RADARMENU_H
#define FS_RADARMENU_H

#include "lllistcontextmenu.h"

namespace FSFloaterRadarMenu
{

/**
 * Menu used in the nearby people list.
 */
class FSRadarMenu : public LLListContextMenu
{
public:
	/*virtual*/ LLContextMenu* createMenu();
private:
	bool enableContextMenuItem(const LLSD& userdata);
	bool checkContextMenuItem(const LLSD& userdata);
	void offerTeleport();
	void teleportToAvatar();
	void onTrackAvatarMenuItemClick();
	void addToContactSet();
	void onSetRenderMode(const LLSD& userdata);
	bool checkSetRenderMode(const LLSD& userdata);
};

extern FSRadarMenu gFSRadarMenu;

} // namespace FSFloaterRadarMenu

#endif // FS_RADARMENU_H
