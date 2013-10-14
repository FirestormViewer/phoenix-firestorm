/**
 * $LicenseInfo:firstyear=2013&license=fsviewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (C) 2013, Nicky Dasmijn
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

#include "ndallocstats.h"

#include <set>

namespace nd
{
	namespace allocstats
	{
		std::set< provider * > s_stProvider;
		
		void startUp()
		{
		}

		void tearDown()
		{
		}

		void registerProvider( provider *aProvider )
		{
			s_stProvider.insert( aProvider );
		}

		void unregisterProvider( provider *aProvider )
		{
			s_stProvider.erase( aProvider );
		}

		void dumpStats( std::ostream &aOut )
		{
			for( std::set< provider * >::iterator itr = s_stProvider.begin(); itr != s_stProvider.end(); ++itr )
				(*itr)->dumpStats( aOut );
		}
	}
}


