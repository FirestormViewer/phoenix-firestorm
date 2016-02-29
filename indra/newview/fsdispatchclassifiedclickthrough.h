/**
 * @file fsdispatchclassifiedclickthrough.h
 * @brief Dispatcher class for classifiedclickthrough message
 *
 * $LicenseInfo:firstyear=2016&license=viewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (c) 2016 Ansariel Hiller @ Second Life
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

#ifndef FS_DISPATCHCLASSIFIEDCLICKTHROUGH_H
#define FS_DISPATCHCLASSIFIEDCLICKTHROUGH_H

#include "fspanelclassified.h"
#include "lldispatcher.h"

// "classifiedclickthrough"
// strings[0] = classified_id
// strings[1] = teleport_clicks
// strings[2] = map_clicks
// strings[3] = profile_clicks
class FSDispatchClassifiedClickThrough : public LLDispatchHandler
{
public:
	virtual bool operator()(
		const LLDispatcher* dispatcher,
		const std::string& key,
		const LLUUID& invoice,
		const sparam_t& strings)
	{
		if (strings.size() != 4) return false;
		LLUUID classified_id(strings[0]);
		S32 teleport_clicks = atoi(strings[1].c_str());
		S32 map_clicks = atoi(strings[2].c_str());
		S32 profile_clicks = atoi(strings[3].c_str());

		FSPanelClassifiedInfo::setClickThrough(
			classified_id, teleport_clicks, map_clicks, profile_clicks, false);

		return true;
	}
};

#endif // FS_DISPATCHCLASSIFIEDCLICKTHROUGH_H
