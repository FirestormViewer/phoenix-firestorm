/**
 * @file fsmoneytracker.cpp
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

#include "llviewerprecompiledheaders.h"

#include "fsmoneytracker.h"

#include "llclipboard.h"
#include "llfloaterreg.h"
#include "llnamelistctrl.h"
#include "lltextbox.h"
#include "lltrans.h"
#include "llviewercontrol.h"

FSMoneyTracker::FSMoneyTracker(const LLSD& key)
    : LLFloater(key),
    mAmountPaid(0),
    mAmountReceived(0)
{
}

bool FSMoneyTracker::postBuild()
{
    mSummary = getChild<LLTextBox>("summary");
    mTransactionHistory = getChild<LLNameListCtrl>("payment_list");
    mTransactionHistory->setContextMenu(&gFSMoneyTrackerListMenu);
    clear();

    // Button Actions
    childSetAction("Clear", boost::bind(&FSMoneyTracker::clear, this));

    return true;
}

void FSMoneyTracker::onClose(bool app_quitting)
{
    if (!gSavedSettings.getBOOL("FSAlwaysTrackPayments"))
    {
        clear();
    }
}

void FSMoneyTracker::addPayment(const LLUUID other_id, bool is_group, S32 amount, bool incoming)
{
    time_t utc_time = time_corrected();

    S32 scroll_pos = mTransactionHistory->getScrollPos();
    bool at_end = mTransactionHistory->getScrollbar()->isAtEnd();

    LLNameListCtrl::NameItem item_params;
    item_params.value = other_id;
    item_params.name = LLTrans::getString("AvatarNameWaiting");
    item_params.target = is_group ? LLNameListCtrl::GROUP : LLNameListCtrl::INDIVIDUAL;
    item_params.columns.add().column("time").value(getTime(utc_time));
    item_params.columns.add().column("name");
    item_params.columns.add().column("amount")
        .value((incoming ? amount : -amount))
        .color(LLUIColorTable::instance().getColor((incoming ? "MoneyTrackerIncrease" : "MoneyTrackerDecrease")));
    item_params.columns.add().column("time_sort_column").value(getDate(utc_time));

    mTransactionHistory->addNameItemRow(item_params);

    if (at_end)
    {
        mTransactionHistory->setScrollPos(scroll_pos + 1);
    }

    if (incoming)
    {
        mAmountReceived += amount;
        mSummary->setTextArg("RECEIVED", llformat("%d", mAmountReceived));
    }
    else
    {
        mAmountPaid += amount;
        mSummary->setTextArg("PAID", llformat("%d", mAmountPaid));
    }
}

std::string FSMoneyTracker::getTime(time_t utc_time)
{
    std::string timeStr = "[" + LLTrans::getString("TimeHour") + "]:[" + LLTrans::getString("TimeMin") + "]:[" + LLTrans::getString("TimeSec") + "]";

    LLSD substitution;
    substitution["datetime"] = (S32)utc_time;
    LLStringUtil::format(timeStr, substitution);

    return timeStr;
}

std::string FSMoneyTracker::getDate(time_t utc_time)
{
    LLDate curdate = LLDate((double)utc_time);
    return curdate.asString();
}

void FSMoneyTracker::clear()
{
    LL_INFOS() << "Cleared." << LL_ENDL;
    mAmountPaid = 0;
    mAmountReceived = 0;
    mTransactionHistory->clearRows();
    mSummary->setTextArg("RECEIVED", llformat("%d", mAmountReceived));
    mSummary->setTextArg("PAID", llformat("%d", mAmountPaid));
}

//////////////////////////////////////////////////////////////////////////////
// FSMoneyTrackerListMenu
//////////////////////////////////////////////////////////////////////////////
LLContextMenu* FSMoneyTrackerListMenu::createMenu()
{
    LLUICtrl::CommitCallbackRegistry::ScopedRegistrar registrar;
    LLUICtrl::EnableCallbackRegistry::ScopedRegistrar enable_registrar;

    registrar.add("MoneyTrackerList.Action", boost::bind(&FSMoneyTrackerListMenu::onContextMenuItemClick, this, _2));
    enable_registrar.add("MoneyTrackerList.Enable", boost::bind(&FSMoneyTrackerListMenu::onContextMenuItemEnable, this, _2));

    return createFromFile("menu_fs_moneytracker_list.xml");
}

void FSMoneyTrackerListMenu::onContextMenuItemClick(const LLSD& userdata)
{
    std::string option = userdata.asString();

    if (option == "copy")
    {
        if (FSMoneyTracker* floater = LLFloaterReg::findTypedInstance<FSMoneyTracker>("money_tracker"); floater)
        {
            std::string copy_text;
            LLNameListCtrl* list = floater->getChild<LLNameListCtrl>("payment_list");

            for (auto item : list->getAllSelected())
            {
                copy_text += ( (copy_text.empty() ? "" : "\n")
                                + item->getColumn(0)->getValue().asString() + ";"
                                + item->getColumn(1)->getValue().asString() + ";"
                                + item->getColumn(2)->getValue().asString()
                                );
            }

            if (!copy_text.empty())
            {
                LLClipboard::instance().copyToClipboard(utf8str_to_wstring(copy_text), 0, static_cast<S32>(copy_text.size()));
            }
        }
    }
    else if (option == "delete")
    {
        FSMoneyTracker* floater = LLFloaterReg::findTypedInstance<FSMoneyTracker>("money_tracker");
        if (floater)
        {
            LLNameListCtrl* list = floater->getChild<LLNameListCtrl>("payment_list");

            list->operateOnSelection(LLCtrlListInterface::OP_DELETE);
        }
    }
}

bool FSMoneyTrackerListMenu::onContextMenuItemEnable(const LLSD& userdata)
{
    std::string item = userdata.asString();

    if (item == "can_copy" || item == "can_delete")
    {
        FSMoneyTracker* floater = LLFloaterReg::findTypedInstance<FSMoneyTracker>("money_tracker");
        if (floater)
        {
            LLNameListCtrl* list = floater->getChild<LLNameListCtrl>("payment_list");
            return (list->getNumSelected() > 0);
        }
    }

    return false;
}

FSMoneyTrackerListMenu gFSMoneyTrackerListMenu;
