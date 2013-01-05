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
 * The Phoenix Viewer Project, Inc., 1831 Oakwood Drive, Fairmont, Minnesota 56031-3225 USA
 * http://www.phoenixviewer.com
 * $/LicenseInfo$
 */

#include "ndmallocstats.h"
#include "llpreprocessor.h"
#include "ndintrin.h"

#include <iomanip>
#include <string.h>

namespace ndMallocStats
{
	U32 sStats[17];

	void startUp()
	{
		memset( &sStats[0], 0, sizeof(sStats) );
	}
	void tearDown()
	{ }

	void logAllocation( size_t aSize, nd::Debugging::IFunctionStack *aStack )
	{
		if( 0 == aSize )
			return;

		ndIntrin::FAA( &sStats[0]  );

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

		ndIntrin::FAA( sStats+i );
	}

	void dumpStats( std::ostream &aOut )
	{
		if( 0 == sStats[0] )
			return;

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
}
