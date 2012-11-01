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
#include <string.h>

#include <iostream>
#include <iomanip>
#include <vector>

#include <time.h>

#include "ndmemorypool.h"
#include "ndintrin.h"
#include "ndlocks.h"

inline size_t nd_min( size_t a1, size_t a2 )
{
	if( a1 < a2 )
		return a1;

	return a2;
}

namespace OSAllocator
{
	inline void OSbaseToAlloc( void *aPtr, void *aOSBase )
	{
		uintptr_t *pTmp = reinterpret_cast<uintptr_t*>(aPtr);
		pTmp[-1] = reinterpret_cast<uintptr_t>(aOSBase);
	}

	inline void* OSbaseFromAlloc( void *aPtr )
	{
		uintptr_t *pTmp = reinterpret_cast<uintptr_t*>(aPtr);
		return reinterpret_cast< void* >( pTmp[-1] );
	}

	inline size_t sizeFromAlloc( void *aPtr )
	{
		uintptr_t *pTmp = reinterpret_cast<uintptr_t*>(aPtr);
		return pTmp[-2];
	}

	inline void sizeToAlloc( void *aPtr, size_t aSize )
	{
		uintptr_t *pTmp = reinterpret_cast<uintptr_t*>(aPtr);
		pTmp[-2] = aSize;
	}

	void *malloc( size_t aSize, size_t aAlign )
	{
		aSize += sizeof(uintptr_t)*2;
		aSize += aAlign;

		void* pMem = ::malloc( aSize );
	
		if( !pMem )
			return 0;
	
		uintptr_t nMem = reinterpret_cast<uintptr_t>(pMem);
		nMem += sizeof(uintptr_t)*2;
		nMem += aAlign;
		nMem &= ~(aAlign - 1);

		char *pRet = reinterpret_cast< char* >( nMem );

		OSbaseToAlloc( pRet, pMem );
		sizeToAlloc( pRet, aSize );

		return pRet;
	}

	void *realloc( void *aPtr, size_t aSize, size_t aAlign )
	{
		void* pMem = malloc( aSize, aAlign );
	
		if( !pMem )
			return 0;
	
		if( aPtr )
		{
			int nToCopy = nd_min( aSize, sizeFromAlloc( aPtr ) );
			memcpy( pMem, aPtr, nToCopy );
			::free( OSbaseFromAlloc( aPtr ) );
		}

		return pMem;

	}

	void free( void* ptr )
	{
		if( ptr )
			::free( OSbaseFromAlloc( ptr ) );
	}
}

#if MAX_PAGES > 0
namespace ndMemoryPool
{
	struct Page
	{
		U32 mFree;
		U32 mLocked;
		U32 mBitmap[BITMAP_SIZE];
		U8 *mMemory;
		U8 *mMemoryEnd;

		Page()
			: mFree(0)
			, mLocked(0)
			, mMemory(0)
			, mMemoryEnd(0)
		{
			memset( mBitmap, 0, sizeof(mBitmap) );
		}
	};

	Page sPages[ MAX_PAGES ];
	U32 mPageLock;
	bool sActive = false;
	time_t sLastLog;

	void allocMemoryForPage( Page &aPage )
	{
		aPage.mMemory = static_cast<U8*>(OSAllocator::malloc( PAGE_SIZE, CHUNK_ALIGNMENT ));
		aPage.mMemoryEnd = aPage.mMemory + PAGE_SIZE;
		aPage.mFree = PAGE_SIZE;
	}

	void allocPage( int i )
	{
		ndLocks::LockHolder( &sPages[i].mLocked );

		if( sPages[i].mMemory )
			return;

		allocMemoryForPage( sPages[i] );
	}

	void freePage( int i )
	{
		ndLocks::LockHolder( &sPages[i].mLocked );

		if( !sPages[i].mMemory )
			return;

		OSAllocator::free( sPages[i].mMemory );
		new (&sPages[i]) Page;
	}
}

namespace ndMemoryPool
{
	int allocNewPage( )
	{
		for( int i = 0; i < MAX_PAGES; ++i )
		{
			if( 0 == sPages[i].mMemory && ndLocks::tryLock( &sPages[i].mLocked ) )
			{
				if( 0 != sPages[i].mFree && 0 != sPages[i].mMemory  )
					return i;

				if( 0 == sPages[i].mMemory  )
				{
					allocMemoryForPage( sPages[i] );
					return i;
				}

				ndLocks::unlock( &sPages[i].mLocked );
			}
		}

		return -1;
	}

	int findPageIndex( )
	{
		for( int i = 0; i < MAX_PAGES; ++i )
		{
			if( 0 != sPages[i].mFree && ndLocks::tryLock( &sPages[i].mLocked ) )
			{
				if( 0 != sPages[i].mFree )
					return i;

				ndLocks::unlock( &sPages[i].mLocked );
			}
		}
		return allocNewPage();
	}

	bool isActive()
	{
		return sActive;
	}

	bool usePool( size_t aAllocSize )
	{
		return isActive() && aAllocSize <= CHUNK_SIZE;
	}

	int toPoolIndex( void *aMemory )
	{
		if( !isActive() || !aMemory )
			return -1;

		for( int i = 0; i < MAX_PAGES; ++i )
			if( aMemory >= sPages[i].mMemory && aMemory < sPages[ i ].mMemoryEnd )
				return i;

		return -1;
	}

	void startUp()
	{
		allocPage( 0 );
		sActive = true;
	}

	void tearDown()
	{
		for( int i = 0; i < MAX_PAGES; ++i )
			freePage( i );
	}

	void *malloc( size_t aSize, size_t aAlign )
	{
		if( !usePool( aSize ) )
			return OSAllocator::malloc( aSize, aAlign );

		int nPageIdx( findPageIndex() );

		if( -1 == nPageIdx )
			return OSAllocator::malloc( aSize, aAlign );

		Page &oPage = sPages[ nPageIdx ];
		ndLocks::LockHolder oLock(0);
		oLock.attach( &oPage.mLocked );

		int nBitmapIdx = 0;
		while( nBitmapIdx < BITMAP_SIZE && oPage.mBitmap[ nBitmapIdx ] == 0xFFFFFFFF )
			++nBitmapIdx;

		if( BITMAP_SIZE == nBitmapIdx )
			return OSAllocator::malloc( aSize, aAlign );

		U32 bBitmap = oPage.mBitmap[ nBitmapIdx ];
		int nBit = 0;

		while( (bBitmap & 0x1) )
		{
			bBitmap = (bBitmap & 0xFFFFFFFE) >> 1;
			nBit++;
		}

		oPage.mBitmap[ nBitmapIdx ] |= ( 0x1 << nBit );
		oPage.mFree -= CHUNK_SIZE;

		size_t nOffset = ( ( nBitmapIdx * BITS_PER_U32 ) + nBit)  * CHUNK_SIZE;
		void *pRet = oPage.mMemory + nOffset;
		return pRet;
	}

	void *realloc( void *ptr, size_t aSize, size_t aAlign )
	{
		int nPoolIdx = toPoolIndex( ptr );

		// Not in pool or 0 == ptr
		if( -1 == nPoolIdx )
			return OSAllocator::realloc( ptr, aSize, aAlign );

		void *pRet = OSAllocator::malloc( aSize, aAlign );

		int nToCopy = nd_min( aSize, CHUNK_SIZE );
		memcpy( pRet, ptr, nToCopy );

		free( ptr );

		return pRet;

	}

	void free( void* ptr )
	{
		int nPoolIdx = toPoolIndex( ptr );

		if( -1 == nPoolIdx )
		{
			OSAllocator::free( ptr );
			return;
		}

		ndLocks::LockHolder oLocker( &sPages[nPoolIdx].mLocked );
		Page &oPage = sPages[nPoolIdx];

		U8 *pChunk = reinterpret_cast< U8* >( ptr );
		int nDiff = pChunk - oPage.mMemory;
		
		int nBitmapIdx = nDiff / CHUNK_SIZE;
		int nBit( nBitmapIdx % BITS_PER_U32);

		nBitmapIdx -= nBit;
		nBitmapIdx /= BITS_PER_U32;

		oPage.mBitmap[ nBitmapIdx ] &= ~ ( 0x1 << nBit );
		oPage.mFree += CHUNK_SIZE;
	}

	void dumpStats( )
	{
		if( (time(0) - sLastLog ) < STATS_FREQ )
			return;

		sLastLog = time(0);

		int nPagesActive(0);
		int nUnusedPages(0);
		int nTotalUsed(0);

		std::vector< double > vPercentages;

		for( int i = 0; i < MAX_PAGES; ++i )
		{
			if( sPages[i].mMemory )
			{
				U32 nFree = sPages[i].mFree;
				nTotalUsed += PAGE_SIZE - nFree ;
				++nPagesActive;

				if( PAGE_SIZE == nFree )
					++nUnusedPages;
	
				double dPercent = nFree;
				dPercent *= 100;
				dPercent /= PAGE_SIZE;
				vPercentages.push_back( dPercent );
			}
		}

		int nTotal = PAGE_SIZE * nPagesActive;
		int nTotalUnused = PAGE_SIZE * nUnusedPages;

		double dPercent = nTotalUsed;
		dPercent *= 100;
		dPercent /= nTotal;

		double dTotal( TO_MB( nTotal ) );
		double dUnused( TO_MB( nTotalUnused ) );
		double dUsed( TO_MB( nTotalUsed) );

		std::cerr	<< "total pages: " << nPagesActive << " unused pages: " << nUnusedPages << " total bytes: " << std::fixed << std::setprecision( 2 ) << dTotal
					<< " used: " << dUsed << " (" << dPercent << "%) unused: " << dUnused << std::endl;

		std::cerr << "page usage (% free): ";
		std::cerr << std::setprecision( 1 );
		for( std::vector<double>::iterator itr = vPercentages.begin(); vPercentages.end() != itr; ++itr )
			std::cerr << *itr << " ";
		std::cerr << std::endl;
	}

	void tryShrink( )
	{
	}

}

#else
namespace ndMemoryPool
{
	void startUp()
	{
	}

	void tearDown()
	{
	}

	void *malloc( size_t aSize, size_t aAlign )
	{
		return OSAllocator::malloc( aSize, aAlign );
	}

	void *realloc( void *ptr, size_t aSize, size_t aAlign )
	{
		return OSAllocator::realloc( ptr, aSize, aAlign );
	}

	void free( void* ptr )
	{
		OSAllocator::free( ptr );
	}

	void dumpStats( )
	{
	}

	void tryShrink()
	{
	}
}
#endif
