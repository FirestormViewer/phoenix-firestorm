/**
 * @file fsfloaterteleporthistory.cpp
 * @brief Class for the standalone teleport history in Firestorm
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

#include "fsfloaterteleporthistory.h"

#include "lldarray.h"
#include "llpanelteleporthistory.h"
#include "llbutton.h"
#include "llfiltereditor.h"

FSFloaterTeleportHistory::FSFloaterTeleportHistory(const LLSD& seed)
	: LLFloater(seed),
	mHistoryPanel(NULL)
{
}

FSFloaterTeleportHistory::~FSFloaterTeleportHistory()
{
	delete mHistoryPanel;
}

BOOL FSFloaterTeleportHistory::postBuild()
{
	mHistoryPanel = new LLTeleportHistoryPanel;

	if (mHistoryPanel)
	{
		mHistoryPanel->mTeleportBtn = getChild<LLButton>("teleport_btn");
		mHistoryPanel->mShowOnMapBtn = getChild<LLButton>("map_btn");
		mHistoryPanel->mShowProfile = getChild<LLButton>("profile_btn");

		mHistoryPanel->mTeleportBtn->setClickedCallback(boost::bind(&LLTeleportHistoryPanel::onTeleport, mHistoryPanel));
		mHistoryPanel->mShowProfile->setClickedCallback(boost::bind(&LLTeleportHistoryPanel::onShowProfile, mHistoryPanel));
		mHistoryPanel->mShowOnMapBtn->setClickedCallback(boost::bind(&LLTeleportHistoryPanel::onShowOnMap, mHistoryPanel));

		mFilterEditor = getChild<LLFilterEditor>("Filter");
		if (mFilterEditor)
		{
			//when list item is being clicked the filter editor looses focus
			//committing on focus lost leads to detaching list items
			//BUT a detached list item cannot be made selected and must not be clicked onto
			mFilterEditor->setCommitOnFocusLost(false);

			mFilterEditor->setCommitCallback(boost::bind(&FSFloaterTeleportHistory::onFilterEdit, this, _2, false));
		}

		getChildView("history_placeholder")->addChild(mHistoryPanel);
		mHistoryPanel->onSearchEdit("");
	}
	else
	{
		return FALSE;
	}

	return TRUE;
}

void FSFloaterTeleportHistory::onFilterEdit(const std::string& search_string, bool force_filter)
{
	if (!mHistoryPanel)
	{
		return;
	}

	if (force_filter || mHistoryPanel->getFilterSubString() != search_string)
	{
		std::string string = search_string;

		// Searches are case-insensitive
		// but we don't convert the typed string to upper-case so that it can be fed to the web search as-is.

		mHistoryPanel->onSearchEdit(string);
	}
}
