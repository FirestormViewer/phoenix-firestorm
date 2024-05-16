/**
 * @file fsmoneytracker.h
 * @brief Tip Tracker Window
 *
 * $LicenseInfo:firstyear=2011&license=viewerlgpl$
 * Copyright (c) 2011 Arrehn Oberlander
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

#ifndef FS_MONEYTRACKER_H
#define FS_MONEYTRACKER_H

#include "llfloater.h"
#include "lllistcontextmenu.h"

class LLNameListCtrl;
class LLTextBox;

class FSMoneyTracker: public LLFloater
{
public:
    FSMoneyTracker(const LLSD& key);
    virtual ~FSMoneyTracker() {};
    virtual void onClose(bool app_quitting);

    bool postBuild();
    void addPayment(const LLUUID other_id, bool is_group, S32 amount, bool incoming);

private:
    void clear();
    std::string getTime(time_t utc_time);
    std::string getDate(time_t utc_time);

    LLNameListCtrl* mTransactionHistory;
    LLTextBox*      mSummary;

    S32 mAmountPaid;
    S32 mAmountReceived;
};

class FSMoneyTrackerListMenu : public LLListContextMenu
{
public:
    /*virtual*/ LLContextMenu* createMenu();
private:
    void onContextMenuItemClick(const LLSD& userdata);
    bool onContextMenuItemEnable(const LLSD& userdata);
};

extern FSMoneyTrackerListMenu gFSMoneyTrackerListMenu;

#endif // FS_MONEYTRACKER_H
