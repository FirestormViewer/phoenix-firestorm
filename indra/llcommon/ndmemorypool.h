/**
 * $LicenseInfo:firstyear=2012&license=fsviewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (C) 2012, Nicky Dasmijn
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

#include "llpreprocessor.h"
#include <ostream>

namespace ndMallocStats
{
	void startUp();
	void tearDown();

	void addStat( size_t aSize );
	void dumpStats( std::ostream &aOut );
}


namespace ndMemoryPool
{
	LL_COMMON_API void startUp();
	LL_COMMON_API void tearDown();

	LL_COMMON_API void *malloc( size_t aSize, size_t aAlign );
	LL_COMMON_API void *realloc( void *ptr, size_t aSize, size_t aAlign );

	LL_COMMON_API void free( void* ptr );

	LL_COMMON_API void dumpStats( std::ostream & );
	LL_COMMON_API void tryShrink( );
}
