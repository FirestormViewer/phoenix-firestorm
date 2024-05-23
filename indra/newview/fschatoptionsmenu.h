/**
 * @file fschatoptionsmenu.h
 * @brief Handler for chat options menu
 *
 * $LicenseInfo:firstyear=2015&license=viewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (c) 2015 Ansariel Hiller
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

#ifndef FS_CHATOPTIONSMENU_H
#define FS_CHATOPTIONSMENU_H

#include "lluictrl.h"

namespace FSChatOptionsMenu
{
    void onMenuItemClick(const LLSD& userdata, LLUICtrl* source);
    bool onMenuItemEnable(const LLSD& userdata, LLUICtrl* source);
    bool onMenuItemVisible(const LLSD& userdata, LLUICtrl* source);
    bool onMenuItemCheck(const LLSD& userdata, LLUICtrl* source);
};

#endif // FS_CHATOPTIONSMENU_H
