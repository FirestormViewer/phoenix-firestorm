/** 
 * @file llfloaterdestinations.h
 * @author Leyla Farazha
 * @brief floater for the destinations guide
 *
 * $LicenseInfo:firstyear=2011&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2011, Linden Research, Inc.
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

/**
 * Floater that appears when buying an object, giving a preview
 * of its contents and their permissions.
 */

#include "llviewerprecompiledheaders.h"

#include "llfloaterdestinations.h"
#include "lluictrlfactory.h"

#include "lfsimfeaturehandler.h"
#include "llhttpconstants.h"
#include "llmediactrl.h"
#include "llweb.h"

LLFloaterDestinations::LLFloaterDestinations(const LLSD& key)
	:	LLFloater(key),
	mDestinationGuideUrlChangedSignal() // <FS:Ansariel> FIRE-16833: Destination guide does not change between OpenSim grids
{
}

LLFloaterDestinations::~LLFloaterDestinations()
{
	// <FS:Ansariel> FIRE-16833: Destination guide does not change between OpenSim grids
	if (mDestinationGuideUrlChangedSignal.connected())
	{
		mDestinationGuideUrlChangedSignal.disconnect();
	}
	// </FS:Ansariel>
}

BOOL LLFloaterDestinations::postBuild()
{
	enableResizeCtrls(true, true, false);
	return TRUE;
}

// <FS:Ansariel> FIRE-16833: Destination guide does not change between OpenSim grids
void LLFloaterDestinations::onOpen(const LLSD& key)
{
	// Connect during onOpen instead of ctor because LLFloaterDestinations instance
	// gets created before we can safely create a LFSimFeatureHandler instance!
	// Assuming we receive the destination guide URL via login response and it
	// is the same URL being sent by region caps so we will be good for the initial
	// region the avatar logs into as well.
	if (!mDestinationGuideUrlChangedSignal.connected())
	{
		mDestinationGuideUrlChangedSignal = LFSimFeatureHandler::instance().setDestinationGuideCallback(boost::bind(&LLFloaterDestinations::handleUrlChanged, this));
	}

	handleUrlChanged();
}

void LLFloaterDestinations::handleUrlChanged()
{
	getChild<LLMediaCtrl>("destination_guide_contents")->navigateTo(LLWeb::expandURLSubstitutions(LFSimFeatureHandler::instance().destinationGuideURL(), LLSD()), HTTP_CONTENT_TEXT_HTML);
}
// </FS:Ansariel>
