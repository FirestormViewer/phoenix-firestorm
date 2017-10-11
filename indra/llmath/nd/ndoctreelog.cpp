/**
 * $LicenseInfo:firstyear=2014&license=fsviewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (C) 2014, Nicky Dasmijn
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

#include "../llmath.h"
#include "ndoctreelog.h"
#include "llfile.h"

namespace nd
{
	namespace octree
	{
		namespace debug
		{
			U32 gOctreeDebug;
			llofstream *pLogStream;
			std::string mOctreeLogFilename;
			
			void setOctreeLogFilename( std::string const &aFilename )
			{
				mOctreeLogFilename = aFilename;
			}
			
			void doOctreeLog( std::string const &aStr )
			{
				if( pLogStream )
				{
					*pLogStream << aStr;
					pLogStream->flush();
				}
			}
			
			void checkOctreeLog()
			{
				if( !pLogStream && mOctreeLogFilename.size() )
				{
					pLogStream = new llofstream();
					pLogStream->open(  mOctreeLogFilename.c_str(), std::ios::out );
					if( pLogStream->is_open() )
					{
						time_t curTime;
						time(&curTime);
						tm *curTimeUTC = gmtime( &curTime );
						
						*pLogStream << "Starting octree log" << asctime( curTimeUTC ) << std::endl;
						pLogStream->flush();
					}
					else
						delete pLogStream;
				}
			}
		}
	}
}

