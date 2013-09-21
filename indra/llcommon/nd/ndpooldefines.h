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

#ifndef NDPOOLDEFINES_H
#define NDPOOLDEFINES_H

#include "stdtypes.h"

#define FROM_MB( mbVal ) (mbVal*1024*1024)
#define TO_MB( bVal ) ( bVal / (1024*1024) )
#define BITS_PER_U8 (8)
#define BITS_PER_U32 ( sizeof(U32) * BITS_PER_U8 )

#define ND_OVERRIDE_NEW 0

// Define those to log the stacktrace for allocations with certain size. You better know what you do when enabing this, ND_OVERRIDE_NEW is best set to 1
#if 0
 #undef ND_USE_MEMORY_POOL
 #define ND_USE_MEMORY_POOL 1

 #define LOG_ALLOCATION_STACKS
 #define MIN_ALLOC_SIZE_FOR_LOG_STACK 32
 #define MAX_ALLOC_SIZE_FOR_LOG_STACK 1024
 #define LOG_STACKSIZE 16
 #define TOP_STACKS_TO_DUMP 10
#endif

#ifdef ND_NO_TCMALLOC
 #define MAX_PAGES (150)
 #define POOL_CHUNK_SIZE (64)
 #define POOL_CHUNK_ALIGNMENT (16)
 #define PAGE_SIZE (FROM_MB(1) )
 #define BITMAP_SIZE ( PAGE_SIZE / BITS_PER_U8 / POOL_CHUNK_SIZE )
#else
 #define MAX_PAGES (0)
#endif

#define STATS_FREQ ( 15 )

#endif
