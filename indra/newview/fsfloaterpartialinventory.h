/**
 * @file fsfloaterpartialinventory.h
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

#ifndef FS_FLOATERPARTIALINVENTORY_H
#define FS_FLOATERPARTIALINVENTORY_H

#include "llfloater.h"
#include "llinventorypanel.h"

class LLFilterEditor;

class FSFloaterPartialInventory : public LLFloater
{
public:
    FSFloaterPartialInventory(const LLSD& key);
    virtual ~FSFloaterPartialInventory();

    bool postBuild() override;
    void onOpen(const LLSD& key) override;

    LLInventoryPanel* getInventoryPanel() const { return mInventoryList; };

private:
    LLUUID              mRootFolderId;
    LLInventoryPanel*   mInventoryList{ nullptr };
    LLFilterEditor*     mFilterEdit{ nullptr };
};

#endif
