/**
 * @file fsfloaterblocklist.cpp
 * @brief Class for the standalone blocklist in Firestorm
 *
 * $LicenseInfo:firstyear=2012&license=viewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (c) 2012 Ansariel Hiller @ Second Life
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

#include "fsfloaterblocklist.h"
#include "llviewercontrol.h"

FSFloaterBlocklist::FSFloaterBlocklist(const LLSD& seed)
	: LLFloater(seed),
	mBlockedListPanel(NULL)
{
}

FSFloaterBlocklist::~FSFloaterBlocklist()
{
}

BOOL FSFloaterBlocklist::postBuild()
{
	mBlockedListPanel = getChild<LLPanel>("panel_block_list_sidetray");
	if (!mBlockedListPanel)
	{
		return FALSE;
	}

	LLView* back_button = mBlockedListPanel->getChildView("back_button_container");
	back_button->setVisible(FALSE);

	return TRUE;
}

void FSFloaterBlocklist::onOpen(const LLSD& key)
{
	if (mBlockedListPanel)
	{
		mBlockedListPanel->onOpen(key);
	}
}