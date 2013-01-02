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
 * http://www.phoenixviewer.com
 * $/LicenseInfo$
 */


#ifndef NDEXCEPTIONS_H
#define NDEXCEPTIONS_H

#include <string>

/* Not derived from std::exception.
   std::exception::what differs between gcc and MSVC. gcc has a throw() gurantee. Same with std::exception::~exception.
   That would mean a lot of #ifdef for no real benefit of having the inheritance right now.
*/

namespace nd
{
	namespace exceptions
	{
		class xran
		{
			std::string mReason;
		public:
			xran( std::string const & );
			
			std::string const& what() const;
		};
	}
}

#endif
