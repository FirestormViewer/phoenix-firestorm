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
 * The Phoenix Firestorm Project, Inc., 1831 Oakwood Drive, Fairmont, Minnesota 56031-3225 USA
 * http://www.phoenixviewer.com
 * $/LicenseInfo$
 */

#ifndef NDMEMORY_H
#define NDMEMORY_H

//#define ND_USE_NATIVE_ALLOCS

namespace ndAllocators
{
	void *ndMalloc( size_t aSize, size_t aAlign );
	void ndFree( void* ptr );
	void *ndRealloc( void *ptr, size_t aSize, size_t aAlign );
}

#if 0

#define ll_aligned_malloc( size, align ) ndAllocators::ndMalloc(size, align)
#define ll_aligned_malloc_16( size ) ndAllocators::ndMalloc( size, 16 )
#define ll_aligned_malloc_32(size) ndAllocators::ndMalloc( size, 32 )

#define ll_aligned_free( ptr ) ndAllocators::ndFree(ptr)
#define ll_aligned_free_16( ptr ) ndAllocators::ndFree( ptr )
#define ll_aligned_free_32( ptr ) ndAllocators::ndFree( ptr )

#define ll_aligned_realloc_16( ptr, size, x ) ndAllocators::ndRealloc( ptr, size, 16 )

#define ALLOCATE_MEM(poolp, size) ll_aligned_malloc_16(size);
#define FREE_MEM(poolp, addr) ll_aligned_free_16( addr );

#else

#ifdef ND_USE_NATIVE_ALLOCS
	#define ll_aligned_malloc( size, align ) _aligned_malloc(size, align)
	#define ll_aligned_malloc_16( size ) _aligned_malloc( size, 16 )
	#define ll_aligned_malloc_32(size) _aligned_malloc( size, 32 )

	#define ll_aligned_free( ptr ) _aligned_free (ptr)
	#define ll_aligned_free_16( ptr ) _aligned_free ( ptr )
	#define ll_aligned_free_32( ptr ) _aligned_free ( ptr )

	#define ll_aligned_realloc_16( ptr, size, x ) _aligned_realloc( ptr, size, 16 )

	#define ALLOCATE_MEM(poolp, size) ll_aligned_malloc_16(size);
	#define FREE_MEM(poolp, addr) ll_aligned_free_16( addr );
#else
	inline void* ll_aligned_malloc( size_t aSize, size_t aAlign ) { return ndAllocators::ndMalloc(aSize, aAlign); }
	inline void* ll_aligned_malloc_16( size_t aSize )             { return ndAllocators::ndMalloc(aSize, 16); }
	inline void* ll_aligned_malloc_32( size_t aSize )             { return ndAllocators::ndMalloc(aSize, 32); }

	inline void ll_aligned_free( void *aPtr )    { ndAllocators::ndFree( aPtr ); }
	inline void ll_aligned_free_16( void *aPtr ) { ndAllocators::ndFree( aPtr ); }
	inline void ll_aligned_free_32( void *aPtr ) { ndAllocators::ndFree( aPtr ); }

	inline void* ll_aligned_realloc_16( void *aPtr, size_t aSize, size_t  ) { return ndAllocators::ndRealloc( aPtr, aSize, 16 ); }

	inline void* ALLOCATE_MEM( void *aPool, size_t aSize ) { return ll_aligned_malloc_16(aSize); }
	inline void FREE_MEM( void *poolp, void *aPtr)         { ll_aligned_free_16( aPtr ); }
#endif

#endif

#endif

