/**
 * @file dialogstack.h
 * @brief Keeps track of number of stacked dialogs
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2013, Zi Ree @ Second Life
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
 * $/LicenseInfo$
 */

#ifndef DIALOGSTACK_H
#define DIALOGSTACK_H

#include "llsingleton.h"

class DialogStack
:   public LLSingleton<DialogStack>
{
    LLSINGLETON_EMPTY_CTOR(DialogStack);
    ~DialogStack() {}

protected:
    void update();

    // since we can't push a floater to the back we need to keep our own list of notification ids
    // to know which one to bring to the front instead
    std::list<LLUUID> mNotificationIDs;

public:
    void push(const LLUUID& uuid);
    void pop(const LLUUID& uuid);
    const LLUUID& flip(const LLUUID& uuid);
};

#endif // DIALOGSTACK_H
