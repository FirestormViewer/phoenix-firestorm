/**
 * @file fsfloatersearch.h
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
 * The Phoenix Viewer Project, Inc., 1831 Oakwood Drive, Fairmont, Minnesota 56031-3225 USA
 * http://www.phoenixviewer.com
 * $/LicenseInfo$
 */

#ifndef FS_FSFLOATERSEARCH_H
#define FS_FSFLOATERSEARCH_H

#include "llmultifloater.h"

class LLFloaterSearch;
class LLTabContainer;

class FSFloaterSearch : public LLMultiFloater
{
public:
	FSFloaterSearch(const LLSD& key);
	/* virtual */ ~FSFloaterSearch();
	
	/* virtual */ BOOL postBuild();
	/* virtual */ void onOpen(const LLSD& key);
	/* virtual */ void addFloater(LLFloater* floaterp,
								  BOOL fSelect,
								  LLTabContainer::eInsertionPoint insertion_point = LLTabContainer::END);
protected:
	LLFloater* mWebSearch;
	LLFloater* mLegacySearch;
};

#endif // FS_FSFLOATERSEARCH_H
