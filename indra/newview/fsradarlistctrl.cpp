/** 
 * @file llradarlistctrl.cpp
 * @brief A radar-specific implementation of scrolllist
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

#include "llviewerprecompiledheaders.h"
#include "fsradarlistctrl.h"
#include "lllistcontextmenu.h"
#include "rlvhandler.h"


#include <boost/tokenizer.hpp>

#include "llavatarnamecache.h"
#include "llcachename.h"
#include "llfloaterreg.h"
#include "llinventory.h"
#include "llscrolllistitem.h"
#include "llscrolllistcell.h"
#include "llscrolllistcolumn.h"
#include "llsdparam.h"
#include "lltooltip.h"
#include "lltrans.h"


static LLDefaultChildRegistry::Register<LLRadarListCtrl> r("radar_list");


LLRadarListCtrl::Params::Params()
{
	name = "radar_list";
}


LLRadarListCtrl::LLRadarListCtrl(const LLRadarListCtrl::Params& p)
:	LLScrollListCtrl(p),
mContextMenu(NULL)
{
}

BOOL LLRadarListCtrl::handleRightMouseDown(S32 x, S32 y, MASK mask)
{
	llinfos << "handleRightMouseDown" << llendl;
	BOOL handled = LLUICtrl::handleRightMouseDown(x, y, mask);
//	if ( mContextMenu )
// [RLVa:KB-FS] - Checked: 2011-06-11 (RLVa-1.3.1) | Added: RLVa-1.3.1
	if ( (mContextMenu) && (!gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES)) )
// [/RLVa:KB-FS]
	{
		if(getAllSelected().size() > 1)
		{
			//llinfos << "Right-click select by value: (multi-select)" << llendl;
			uuid_vec_t selected_uuids;
			for(size_t i=0;i<getAllSelected().size();i++)
			{
				llinfos << "Right-click select by value: " << getAllSelected().at(i)->getColumn(getColumn("uuid")->mIndex)->getValue().asUUID() << llendl;
				selected_uuids.push_back(getAllSelected().at(i)->getColumn(getColumn("uuid")->mIndex)->getValue().asUUID());
			}
			mContextMenu->show(this, selected_uuids, x, y);
		}
		else
		{
			LLScrollListItem* hit_item = hitItem(x, y);
			if (hit_item)
			{
				llinfos << "Right-click select by value: " << hit_item->getValue() << llendl;
				selectByID(hit_item->getValue());
				LLUUID av = hit_item->getValue();
				uuid_vec_t selected_uuids;
				selected_uuids.push_back(av);
				mContextMenu->show(this, selected_uuids, x, y);
			}
		}
	}
	return handled;
}
