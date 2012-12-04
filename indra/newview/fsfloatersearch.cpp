/**
 * @file fsfloatersearch.cpp
 * @brief Container for websearch and legacy search
 *
 * $LicenseInfo:firstyear=2012&license=fsviewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (C) 2012, Cinder Roxley <cinder@cinderblocks.biz>
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
 * http://www.phoenixviewer.com
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "fsfloatersearch.h"
#include "llfloaterreg.h"
#include "llfloatersearch.h"

FSFloaterSearch::FSFloaterSearch(const LLSD& key)
:	LLMultiFloater(key)
	, mWebSearch(NULL)
{
	mAutoResize = true;
}

FSFloaterSearch::~FSFloaterSearch()
{
}

BOOL FSFloaterSearch::postBuild()
{
	mTabContainer = getChild<LLTabContainer>("search_tabs");

	mWebSearch = LLFloaterReg::getTypedInstance<LLFloaterSearch>("search");
	addFloater(mWebSearch, true);

	mLegacySearch = LLFloaterReg::getTypedInstance<LLFloater>("search_legacy");
	addFloater(mLegacySearch, false);

	return LLMultiFloater::postBuild();
}

void FSFloaterSearch::onOpen(const LLSD& key)
{
	LLMultiFloater::onOpen(LLSD());

	if (key.has("category"))
	{
		// This is meant for the web search tab
		S32 idxTab = mTabContainer->getIndexForPanel(mWebSearch);
		if (-1 != idxTab)
		{
			mTabContainer->selectTab(idxTab);
			mWebSearch->onOpen(key);
		}
	}
	else
	{
		mTabContainer->getCurrentPanel()->onOpen(key);
	}
}

void FSFloaterSearch::addFloater(LLFloater* pFloater, BOOL fSelect, LLTabContainer::eInsertionPoint insertion_point)
{
	if (pFloater)
	{
		LLRect rctFloater = pFloater->getRect();
		rctFloater.mTop -= pFloater->getHeaderHeight();
		pFloater->setRect(rctFloater);

		LLMultiFloater::addFloater(pFloater, fSelect, insertion_point);
	}
}
