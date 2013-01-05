#ifndef NDSTACKWALK_h
#define NDSTACKWALK_h

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

namespace nd
{
	namespace debugging
	{
		struct sEBP
		{
			sEBP *pPrev;
			void *pRet;
		};

		class IFunctionStack
		{
		public:
			virtual ~IFunctionStack()
			{}

			virtual bool isFull() const = 0;
			virtual int getDepth() const = 0;
			virtual int getMaxDepth() const = 0;

			virtual void pushReturnAddress( void *aReturnAddress ) = 0;
			virtual void* getReturnAddress( int aFrame ) = 0;
		};

		extern "C"
		{
#if defined( LL_WINDOWS )
			__forceinline __declspec(naked) sEBP* getEBP()
			{
				__asm{
					MOV EAX,EBP
					RET
				}
			}
#elif defined( LL_LINUX )
			asm( "getEBP: MOVL %EBP,%EAX; RET;" );
			sEBP* getEBP();
#else
			inline sEBP* getEBP()
			{ return 0; }
#endif
		}

		inline void getCallstack( sEBP *aEBP, IFunctionStack *aStack )
		{
			sEBP *pEBP(aEBP);

			while( pEBP && pEBP->pRet && !aStack->isFull() )
			{
				aStack->pushReturnAddress( pEBP->pRet );
				pEBP = pEBP->pPrev;
			}
		}
	}
}
#endif
