/**
 * @file fsradarlistctrl.cpp
 * @brief A radar-specific implementation of scrolllist
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

#include "llviewerprecompiledheaders.h"

#include "fsradarlistctrl.h"
#include "llscrolllistitem.h"
#include "rlvhandler.h"

static LLDefaultChildRegistry::Register<FSRadarListCtrl> r("radar_list");

FSRadarListCtrl::FSRadarListCtrl(const Params& p)
:   FSScrollListCtrl(p)
{
}

bool FSRadarListCtrl::handleRightMouseDown(S32 x, S32 y, MASK mask)
{
    bool handled = LLUICtrl::handleRightMouseDown(x, y, mask);
    if ( (mContextMenu) && (!gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES)) )
    {
        if (std::vector<LLScrollListItem*> selected_items = getAllSelected(); selected_items.size() > 1)
        {
            uuid_vec_t selected_uuids;
            for (auto selected_item : selected_items)
            {
                selected_uuids.emplace_back(selected_item->getUUID());
            }
            mContextMenu->show(this, selected_uuids, x, y);
        }
        else
        {
            if (LLScrollListItem* hit_item = hitItem(x, y); hit_item)
            {
                selectByID(hit_item->getValue());
                LLUUID av = hit_item->getValue();
                uuid_vec_t selected_uuids;
                selected_uuids.emplace_back(av);
                mContextMenu->show(this, selected_uuids, x, y);
            }
        }
    }
    return handled;
}
