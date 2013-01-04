#ifndef NDINTRIN_H
#define NDINTRIN_H

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


#include "stdtypes.h"

namespace ndIntrin
{
#if LL_WINDOWS
	U32 CAS( volatile U32 *aLoc, U32 aCmp, U32 aVal );
	void* CASPTR( void * volatile *aLoc, void* aCmp, void * aVal );
	void FAA( volatile U32 *aLoc );
	U32 FAD( volatile U32 *aLoc );
#else
	inline U32  CAS( volatile U32 *aLoc, U32 aCmp, U32 aVal )
	{ return __sync_val_compare_and_swap( aLoc, aCmp, aVal ); }

	inline void* CASPTR( void * volatile *aLoc, void* aCmp, void * aVal )
	{ return __sync_val_compare_and_swap( aLoc, aCmp, aVal ); }

	inline void FAA( volatile U32 *aLoc )
	{ __sync_add_and_fetch( aLoc, 1 ); }

	inline U32 FAD( volatile U32 *aLoc )
	{ return __sync_sub_and_fetch( aLoc,1 ); }
#endif
}

#endif
