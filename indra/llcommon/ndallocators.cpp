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


#include <new>
#include <stdlib.h>
#include <stdint.h>
#include "ndmemorypool.h"

#ifdef ND_NO_TCMALLOC

namespace ndAllocators
{
#ifdef ND_USE_NATIVE_ALLOCS
	void *ndMalloc( size_t aSize, size_t aAlign )
	{ return _aligned_malloc( aSize, aAlign );	}

	void *ndRealloc( void *ptr, size_t aSize, size_t aAlign )
	{ return _aligned_realloc( ptr, aSize, aAlign );	}

	void ndFree( void* ptr )
	{ return _aligned_free( ptr );	}
#else
	void *ndMalloc( size_t aSize, size_t aAlign )
	{ return ndMemoryPool::malloc( aSize, aAlign );	}

	void *ndRealloc( void *ptr, size_t aSize, size_t aAlign )
	{ return ndMemoryPool::realloc( ptr, aSize, aAlign );	}

	void ndFree( void* ptr )
	{ return ndMemoryPool::free( ptr );	}
#endif
}
using namespace ndAllocators;

void *operator new(size_t nSize )
{
	return ndMalloc( nSize, 16 );
}

void *operator new[](size_t nSize )
{
	return ndMalloc( nSize, 16 );
}

void operator delete( void *pMem )
{
	ndFree( pMem );
}

void operator delete[]( void *pMem )
{
	ndFree( pMem );
}
#endif
