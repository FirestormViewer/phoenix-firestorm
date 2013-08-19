/**
 * @file dialogstack.cpp
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

#include "llviewerprecompiledheaders.h"

#include "dialogstack.h"

#include "llviewercontrol.h"

DialogStack::DialogStack() :
	LLSingleton<DialogStack>()
{
}

DialogStack::~DialogStack()
{
}

void DialogStack::update()
{
	// show dialog stack browse icon when more than one dialog is on the screen
	gSavedSettings.setBOOL("DialogStackIconVisible",(notificationIDs.size()>1));
}

void DialogStack::push(const LLUUID& uuid)
{
	notificationIDs.push_back(uuid);
	update();
}

void DialogStack::pop(const LLUUID& uuid)
{
	if(notificationIDs.empty())
	{
		llwarns << "Dialog Stack count was 0 when pop() was called." << llendl;
	}
	else
	{
		notificationIDs.remove(uuid);
		update();
	}
}

const LLUUID& DialogStack::flip(const LLUUID& uuid)
{
	for (std::list<LLUUID>::iterator it=notificationIDs.begin();it!=notificationIDs.end();++it)
	{
		if(*it==uuid)
		{
			if(it==notificationIDs.begin())
			{
				return notificationIDs.back();
			}
			--it;
			return *it;
		}
	}
	return LLUUID::null;
}
