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

#include "ndmallocstats.h"
#include "llpreprocessor.h"
#include "ndintrin.h"
#include "ndlocks.h"
#include "ndpooldefines.h"
#include "ndstlallocator.h"

#include <string.h>
#include <stdlib.h>

#include <iomanip>
#include <set>
#include <cstddef>

namespace nd
{
	namespace allocstats
	{
		template< typename T> struct compare_stack
		{
			bool operator()( T const &aRHS, T const &aLHS )
			{
				if( aRHS.getDepth() != aLHS.getDepth() )
					return aRHS.getDepth() < aLHS.getDepth();

				for( unsigned int i = 0; i < aRHS.getDepth(); ++i )
				{
					if( aRHS.getReturnAddress(i) != aLHS.getReturnAddress(i) )
						return aRHS.getReturnAddress(i) < aLHS.getReturnAddress(i);
				}
				return false;
			}
		};

#ifdef LOG_ALLOCATION_STACKS
		typedef nd::debugging::FunctionStack<LOG_STACKSIZE> tStack;
		typedef std::set< tStack , compare_stack<tStack>, nd::stl::allocator<tStack> > tSet;

		tSet stStacks;
		volatile U32 lStackLock;
#endif

		bool bStarted;
		U32 sStats[17];

		void startUp()
		{
			memset( &sStats[0], 0, sizeof(sStats) );
			bStarted = true;
		}

		void tearDown()
		{ bStarted = false; }

		void logAllocation( size_t aSize, nd::debugging::sEBP * aEBP )
		{
			if( !isEnabled() )
				return;

			nd::intrin::FAA( &sStats[0]  );

			if( aSize > 65536 )
				return;

			aSize -= 1;
			int i = 0;

			for( ; i <= 16; ++i )
			{
				if( 0 == aSize )
					break;
				aSize = aSize / 2;
			}

			if( 0 == i )
				i = 1;

			nd::intrin::FAA( sStats+i );

#ifdef LOG_ALLOCATION_STACKS
			if(  aEBP )
			{
				nd::debugging::FunctionStack< LOG_STACKSIZE > oStack;
				nd::debugging::getCallstack( aEBP, &oStack );

				nd::locks::LockHolder oLock( &lStackLock );
				tSet::iterator itr = stStacks.insert( oStack ).first;
				itr->incCallcount();
			}
#endif
		}

#ifdef LOG_ALLOCATION_STACKS
		void sortStacks( tStack *aStacks, int aCount )
		{
			for( int i = aCount-1; i > 0; --i )
			{
				if( aStacks[i].getCallcount() > aStacks[i-1].getCallcount() )
				{
					tStack tmp = aStacks[i];
					aStacks[i] = aStacks[i-1];
					aStacks[i-1] = tmp;
				}
				else
					break;
			}
		}
#endif

		void dumpHighesAllocations( std::ostream &aOut )
		{
#ifdef LOG_ALLOCATION_STACKS
			tStack topStacks[TOP_STACKS_TO_DUMP]; 
			{
				nd::locks::LockHolder oLock( &lStackLock );
				tSet::const_iterator itr = stStacks.begin(), itrE = stStacks.end();
				for( ; itr != itrE; ++itr )
				{
					if( itr->getCallcount() > topStacks[TOP_STACKS_TO_DUMP-1].getCallcount() )
					{
						topStacks[TOP_STACKS_TO_DUMP-1] = *itr;
						sortStacks( topStacks, TOP_STACKS_TO_DUMP );
					}
				}
			}
			aOut << std::hex;
			for( int i = 0; i < TOP_STACKS_TO_DUMP; ++i )
			{
				aOut << "Calls: " << topStacks[i].getCallcount() << std::endl;
				for( int j = 0; j < topStacks[i].getDepth() && topStacks[i].getReturnAddress(j) ; ++j )
					aOut << "0x" << topStacks[i].getReturnAddress(j) << std::endl;
			}

			aOut << std::dec;
#endif
		}

		void dumpStats( std::ostream &aOut )
		{
			if( !isEnabled() )
				return;

#ifdef LOG_ALLOCATION_STACKS
			dumpHighesAllocations( aOut );
#endif
			float fTotal = sStats[0];

			aOut << "small allocations (size/#/%):";
			aOut << std::setprecision(2) << std::fixed;

			int nSmallAllocs(0);
			int nSize = 2;
			for( int i = 1 ; i < sizeof(sStats)/sizeof(int); ++i  )
			{
				nSmallAllocs += sStats[i];

				if( sStats[i] > 0 )
				{
					float fPercent = sStats[i];
					fPercent *= 100.0;
					fPercent /= fTotal;

					aOut << " " << nSize << "/" << sStats[i] << "/" << fPercent;
				}
				nSize *= 2;
			}
			float fPercentSmall = nSmallAllocs;
			fPercentSmall *= 100.0;
			fPercentSmall /= fTotal;

			aOut << " t/s (% s) " << sStats[0] << "/" << nSmallAllocs << "(" << fPercentSmall << ")";
		}

		bool isEnabled()
		{
			return bStarted && 0 != sStats[0];
		}
	}
}
