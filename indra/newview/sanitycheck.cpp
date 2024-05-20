/**
 * @file sanitycheck.cpp
 * @brief Settings sanity check engine
 *
 * Copyright (C) 2011, Zi Ree @ Second Life
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
 */

#include "llviewerprecompiledheaders.h"

#include "llcontrol.h"
#include "llnotifications.h"
#include "llnotificationsutil.h"
#include "lltrans.h"
#include "llviewercontrol.h"
#include "sanitycheck.h"

#define SANITY_CHECK "SanityCheck"  // name of the notification we display

void SanityCheck::init()
{
    struct f : public LLControlGroup::ApplyFunctor
    {
        SanityCheck* chk;
        f(SanityCheck* s) : chk(s) {};
        virtual void apply(const std::string& name, LLControlVariable* control)
        {
            if (control->getSanityType() != SANITY_TYPE_NONE)
            {
                control->getSanitySignal()->connect(boost::bind(&SanityCheck::onSanity, _1, false));
                SanityCheck::instance().onSanity(control);
            }
        }
    } func(this);

    gSavedSettings.applyToAll(&func);
    gSavedPerAccountSettings.applyToAll(&func);
}

// static
void SanityCheck::onSanity(LLControlVariable* controlp, bool disregardLastControl /*= false*/)
{
    static LLControlVariable* lastControl = nullptr;

    if (controlp->isSane())
    {
        return;
    }

    if (disregardLastControl)
    {
        // clear "ignored" status for this control, so it can actually show up
        LLNotifications::instance().setIgnored(SANITY_CHECK, false);
    }
    else if (controlp == lastControl)
    {
        return;
    }

    lastControl = controlp;

    std::string checkType = SANITY_CHECK + LLControlGroup::sanityTypeEnumToString(controlp->getSanityType());
    std::vector<LLSD> sanityValues = controlp->getSanityValues();

    LLSD args;
    LLStringUtil::format_map_t map;
    map["VALUE_1"] = sanityValues[0].asString();
    map["VALUE_2"] = sanityValues[1].asString();
    map["CONTROL_NAME"] = controlp->getName();
    args["SANITY_MESSAGE"] = LLTrans::getString(checkType, map);
    args["SANITY_COMMENT"] = controlp->getSanityComment();
    args["CURRENT_VALUE"] = controlp->getValue().asString();
    LLNotificationsUtil::add(SANITY_CHECK, args, LLSD(), boost::bind(SanityCheck::onFixIt, _1, _2, controlp));
}

void SanityCheck::onFixIt(const LLSD& notification, const LLSD& response, LLControlVariable* controlp)
{
    S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
    if (option == 0) // Fix it
    {
        if (controlp)
        {
            controlp->resetToDefault(true);
            return;
        }
    }
}
