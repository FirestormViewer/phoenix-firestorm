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
	llinfos << "handled=" << handled << llendl;
	if ( mContextMenu )
	//  if ( (mContextMenu) && ((!mRlvCheckShowNames) || (!gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES))) ) //TODO, handle RLV
	{
		llinfos << "x= " << x << llendl;
		LLScrollListItem* hit_item = hitItem(x, y);
		if (hit_item)
		{
			llinfos << "got hit item" << llendl;
			LLUUID av = hit_item->getColumn(5)->getValue().asUUID();
			llinfos << "av = " << av << llendl;
			uuid_vec_t selected_uuids;
			llinfos << "about to make selected id" << llendl;
			selected_uuids.push_back(av);
			llinfos << "about to show menu" << llendl;
			mContextMenu->show(this, selected_uuids, x, y);
		}
	}
	return handled;
}
