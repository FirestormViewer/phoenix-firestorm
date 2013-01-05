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
 * http://www.firestormviewer.org
 * $/LicenseInfo$
 */

#ifndef NDLOCKS_H
#define NDLOCKS_H

#include "ndintrin.h"

namespace nd
{
	namespace locks
	{
		inline void lock( volatile U32 *aLock )
		{
			while( 0 != nd::intrin::CAS( aLock, 0, 1 ) )
				;
		}

		inline void unlock ( volatile U32 *aLock )
		{
			*aLock = 0;
		}

		inline bool tryLock( volatile U32 *aLock )
		{
			return 0 == nd::intrin::CAS( aLock, 0, 1 );
		}

		class LockHolder
		{
			volatile U32 *mLock;
		public:
			LockHolder( volatile U32 *aLock )
				: mLock( aLock )
			{
				if( mLock )
					lock( mLock );
			}

			~LockHolder()
			{
				if( mLock )
					unlock( mLock );
			}

			void attach( volatile U32 *aLock )
			{
				mLock = aLock;
			}
		};
	}
}

#endif
