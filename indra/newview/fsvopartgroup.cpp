/** 
 * @file fsvopartgroup.cpp
 * @brief fsvopartgroup base class
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
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
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "llviewerpartsim.h" // For LL_MAX_PARTICLE_COUNT

#include "fsvopartgroup.h"


FSVOPartGroup::FSVOPartGroup()
{
	for(int i = 0; i < sFreeIndexSize; i++)
	{
		setFreeIndex(i,0);
	}
	setIndexGeneration(1);
	setTotalParticles(0);

	U32 bitMasksTmp[ 32 ] = { 0xFFFFFFFF, 0x80000000, 0xC0000000, 0xE0000000,
					   0xF0000000, 0xF8000000, 0xFC000000, 0xFE000000,
					   0xFF000000, 0xFF800000, 0xFFC00000, 0xFFE00000,
					   0xFFF00000, 0xFFF80000, 0xFFFC0000, 0xFFFE0000,
					   0xFFFF0000, 0xFFFF8000, 0xFFFFC000, 0xFFFFE000,
					   0xFFFFF000, 0xFFFFF800, 0xFFFFFC00, 0xFFFFFE00,
					   0xFFFFFF00, 0xFFFFFF80, 0xFFFFFFC0, 0xFFFFFFE0,
					   0xFFFFFFF0, 0xFFFFFFF8, 0xFFFFFFFC, 0xFFFFFFFE };

	for(int i = 0;i<32;i++)
	{
		bitMasks[i] = bitMasksTmp[i];
	}
}


bool FSVOPartGroup::findAvailableVBSlots( S32 &idxStart, S32 &idxEnd, U32 amount )
{
	idxStart = idxEnd = -1;

	if( amount + getTotalParticles() > LL_MAX_PARTICLE_COUNT )
		amount = LL_MAX_PARTICLE_COUNT - getTotalParticles();

	U32 u32Count = amount/32;
	U32 bitsLeft = amount - (u32Count*32);
	if( bitsLeft )
		++u32Count;
	U32 maskLast = bitMasks[ bitsLeft ];

	int i = 0;
	int maxI = LL_MAX_PARTICLE_COUNT/32-u32Count;
	do
	{
		while( getFreeIndex(i) != 0xFFFFFFFF && i <= maxI  )
			++i;

		if( i > maxI )
			continue;

		int j = i;
		while( getFreeIndex(j) == 0xFFFFFFFF && j <= LL_MAX_PARTICLE_COUNT/32 && (j-i) != u32Count )
			++j;

		if( j > LL_MAX_PARTICLE_COUNT/32 || (i-j) != u32Count || (getFreeIndex(j-1) & maskLast) != maskLast )
		{
			++i;
			continue;
		}

		int k = 0;
		idxStart = i*32;
		idxEnd = i + amount;
		while( k != amount )
		{
			for( int l = 0; k != amount && l < 32; ++l )
			{
				U32 mask = 1 << l;
				setFreeIndex(i, getFreeIndex(i) & ~mask);
				++k;
			}
			++i;
		}

		setTotalParticles(getTotalParticles() + amount);
		return true;

	} while( i <= maxI);
	return false;
}
