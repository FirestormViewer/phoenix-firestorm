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
 * The Phoenix Firestorm Project, Inc., 1831 Oakwood Drive, Fairmont, Minnesota 56031-3225 USA
 * http://www.firestormviewer.org
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
			virtual unsigned int getDepth() const = 0;
			virtual unsigned int getMaxDepth() const = 0;

			virtual void pushReturnAddress( void *aReturnAddress ) = 0;
			virtual void* getReturnAddress( unsigned int aFrame ) const = 0;
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
			__forceinline __declspec(naked) sEBP* getStackTop()
			{
				__asm{
					MOV EAX,FS:[0x04]
					RET
				}
			}
			__forceinline __declspec(naked) sEBP* getStackBottom()
			{
				__asm{
					MOV EAX,FS:[0x08]
					RET
				}
			}
#elif defined( LL_LINUX )
			asm( "getEBP: MOVL %EBP,%EAX; RET;" );
			sEBP* getEBP();
			inline void* getStackTop()
			{ return 0; }
			inline void* getStackBottom()
			{ return 0; }
#else
			inline sEBP* getEBP()
			{ return 0; }
			inline void* getStackTop()
			{ return 0; }
			inline void* getStackBottom()
			{ return 0; }
#endif
		}

		inline void getCallstack( sEBP *aEBP, IFunctionStack *aStack )
		{
			sEBP *pEBP(aEBP);
			void *pTop = getStackTop();
			void *pBottom = getStackBottom();

			while( pBottom < pEBP  && pEBP < pTop && pEBP->pRet && !aStack->isFull() )
			{
				aStack->pushReturnAddress( pEBP->pRet );
				pEBP = pEBP->pPrev;
			}
		}
	}
}
#endif
