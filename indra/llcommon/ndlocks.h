#ifndef NDLOCKS_H
#define NDLOCKS_H

#include "ndintrin.h"

namespace ndLocks
{
	inline void lock( volatile U32 *aLock )
	{
		while( 0 != ndIntrin::CAS( aLock, 0, 1 ) )
			;
	}

	inline void unlock ( volatile U32 *aLock )
	{
		*aLock = 0;
	}

	inline bool tryLock( volatile U32 *aLock )
	{
		return 0 == ndIntrin::CAS( aLock, 0, 1 );
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

#endif
