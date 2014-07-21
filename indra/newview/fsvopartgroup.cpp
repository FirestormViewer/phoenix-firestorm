U32 sFreeIndex[LL_MAX_PARTICLE_COUNT/32] = {0};
U32 sIndexGeneration = 1;
U32 sTotalParticles = 0;

U32 bitMasks[ 32 ] = { 0xFFFFFFFF, 0x80000000, 0xC0000000, 0xE0000000,
					   0xF0000000, 0xF8000000, 0xFC000000, 0xFE000000,
					   0xFF000000, 0xFF800000, 0xFFC00000, 0xFFE00000,
					   0xFFF00000, 0xFFF80000, 0xFFFC0000, 0xFFFE0000,
					   0xFFFF0000, 0xFFFF8000, 0xFFFFC000, 0xFFFFE000,
					   0xFFFFF000, 0xFFFFF800, 0xFFFFFC00, 0xFFFFFE00,
					   0xFFFFFF00, 0xFFFFFF80, 0xFFFFFFC0, 0xFFFFFFE0,
					   0xFFFFFFF0, 0xFFFFFFF8, 0xFFFFFFFC, 0xFFFFFFFE };

bool findAvailableVBSlots( S32 &idxStart, S32 &idxEnd, U32 amount )
{
	idxStart = idxEnd = -1;

	if( amount + sTotalParticles > LL_MAX_PARTICLE_COUNT )
		amount = LL_MAX_PARTICLE_COUNT - sTotalParticles;

	U32 u32Count = amount/32;
	U32 bitsLeft = amount - (u32Count*32);
	if( bitsLeft )
		++u32Count;
	U32 maskLast = bitMasks[ bitsLeft ];

	int i = 0;
	int maxI = LL_MAX_PARTICLE_COUNT/32-u32Count;
	do
	{
		while( sFreeIndex[i] != 0xFFFFFFFF && i <= maxI  )
			++i;

		if( i > maxI )
			continue;

		int j = i;
		while( sFreeIndex[j] == 0xFFFFFFFF && j <= LL_MAX_PARTICLE_COUNT/32 && (j-i) != u32Count )
			++j;

		if( j > LL_MAX_PARTICLE_COUNT/32 || (i-j) != u32Count || (sFreeIndex[j-1] & maskLast) != maskLast )
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
				sFreeIndex[ i ] &= ~mask;
				++k;
			}
			++i;
		}

		sTotalParticles += amount;
		return true;

	} while( i <= maxI);
	return false;
}
