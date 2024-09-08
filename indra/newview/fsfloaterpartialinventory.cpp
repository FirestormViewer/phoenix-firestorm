/**
 * @file fsfloaterpartialinventory.cpp
 * @brief Displays the inventory underneath a particular starting folder
 *
 * $LicenseInfo:firstyear=2022&license=viewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (c) 2022 Ansariel Hiller @ Second Life
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

#include "fsfloaterpartialinventory.h"
#include "llfiltereditor.h"

//=========================================================================

//=========================================================================
FSFloaterPartialInventory::FSFloaterPartialInventory(const LLSD& key) : LLFloater(key)
{
    mRootFolderId = key["start_folder_id"].asUUID();
}

FSFloaterPartialInventory::~FSFloaterPartialInventory() = default;

bool FSFloaterPartialInventory::postBuild()
{
    LLInventoryPanel::Params params;
    params.start_folder.id(mRootFolderId);
    params.follows.flags(FOLLOWS_ALL);
    params.layout("topleft");
    params.name("inv_panel");
    mInventoryList = LLUICtrlFactory::create<LLInventoryPanel>(params);

    auto wrapper_panel = getChild<LLPanel>("pnl_inv_wrap");
    wrapper_panel->addChild(mInventoryList);
    mInventoryList->reshape(wrapper_panel->getRect().getWidth(), wrapper_panel->getRect().getHeight());
    mInventoryList->setOrigin(0, 0);

    mFilterEdit = getChild<LLFilterEditor>("flt_search");
    mFilterEdit->setCommitCallback([this](LLUICtrl*, const LLSD& param){ mInventoryList->setFilterSubString(param.asString()); });

    return true;
}

void FSFloaterPartialInventory::onOpen(const LLSD& key)
{
    LLStringUtil::format_map_t args;
    args["FOLDERNAME"] = key["start_folder_name"].asString();
    setTitle(getString("title", args));
}
