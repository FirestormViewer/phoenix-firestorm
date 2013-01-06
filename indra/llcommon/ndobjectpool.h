#ifndef NDOBJECTPOOL_H
#define NDOBJECTPOOL_H

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

#include "ndlocks.h"
#include "ndallocators.h"

namespace nd
{
	namespace objectpool
	{
		template<typename T, typename Lock = nd::locks::NoLock, int Alignment = 16, int AllocationSize = 32 > class ObjectPool
		{
			struct ObjectMemory
			{
				ObjectMemory *mNext;
			};

			Lock mLock;
			unsigned int mObjectSize;
			ObjectMemory mMemory;

			void grow()
			{
				char *pMemory = reinterpret_cast< char* >( nd::allocators::malloc( mObjectSize * AllocationSize, Alignment ) );
				ObjectMemory *pPrev = &mMemory;

				for( int i = 0; i < AllocationSize; ++i )
				{
					ObjectMemory *pCur = reinterpret_cast< ObjectMemory* >( pMemory );
					pPrev->mNext = pCur;
					pPrev = pCur;
					pPrev->mNext = 0;

					pMemory += mObjectSize;
				}
			}

			void destroy()
			{
				// Just leak. Process is exiting and the OS will clean up after us. This is ok.
			}

		public:
			ObjectPool()
			{
				mObjectSize = sizeof( T ) > sizeof(ObjectMemory) ? sizeof( T ) : sizeof(ObjectMemory);
				mObjectSize += Alignment-1;
				mObjectSize &= ~(Alignment-1);
				mMemory.mNext = 0;
			}

			~ObjectPool()
			{
				destroy();
			}

			T *allocMemoryForObject()
			{
				mLock.lock();
				if( !mMemory.mNext )
					grow();

				ObjectMemory *pRet = mMemory.mNext;
				mMemory.mNext = pRet->mNext;

				mLock.unlock();

				T *pT = reinterpret_cast< T* >( pRet );
				return pT;
			}

			T *allocObject()
			{
				T *pT = allocMemoryForObject();
				new (pT)T;
				return pT;
			}

			void freeMemoryOfObject( void *aObject )
			{
				mLock.lock();
				ObjectMemory *pMemory = reinterpret_cast< ObjectMemory* >( aObject );
				pMemory->mNext = mMemory.mNext;
				mMemory.mNext = pMemory;
				mLock.unlock();
			}

			void freeObject( T *aObject )
			{
				if( !aObject)
					return;

				aObject->~T();
				freeMemoryOfObject( aObject );
			}
		};
	}
}

#endif
