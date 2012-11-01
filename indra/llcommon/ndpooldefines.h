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


#ifndef NDPOOLDEFINES_H
#define NDPOOLDEFINES_H

#ifndef LL_STDTYPES_H
	#if LL_WINDOWS
		#include <Windows.h>
		typedef LONG U32;
	#else
		typedef unsigned int U32;
	#endif

	typedef unsigned char U8;
#endif

#define FROM_MB( mbVal ) (mbVal*1024*1024)
#define TO_MB( bVal ) ( bVal / (1024*1024) )
#define BITS_PER_U8 (8)
#define BITS_PER_U32 ( sizeof(U32) * BITS_PER_U8 )

#define MAX_PAGES (0)
#define CHUNK_SIZE (64)
#define CHUNK_ALIGNMENT (16)
#define PAGE_SIZE (FROM_MB(1) )
#define BITMAP_SIZE ( PAGE_SIZE / BITS_PER_U8 / CHUNK_SIZE )

#define STATS_FREQ ( 15 )

#endif
