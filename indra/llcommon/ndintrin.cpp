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

#if LL_WINDOWS
#include "stdtypes.h"
#include "ndintrin.h"

#include <Windows.h>

namespace ndIntrin
{
	U32 CAS( volatile U32 *aLoc, U32 aCmp, U32 aVal )
	{ return InterlockedCompareExchange( aLoc, aVal, aCmp ); }

	void* CASPTR( void * volatile *aLoc, void* aCmp, void * aVal )
	{ return InterlockedCompareExchangePointer ( aLoc, aVal, aCmp ); }

	void FAA( volatile U32 *aLoc )
	{ InterlockedIncrement( aLoc ); }

	U32 FAD( volatile U32 *aLoc )
	{ return InterlockedDecrement( aLoc ); }
}

#endif
