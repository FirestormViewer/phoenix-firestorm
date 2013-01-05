#ifndef NDCALLSTACK_H
#define NDCALLSTACK_H

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
 * $/LicenseInfo
 */

#include "ndstackwalk.h"
#include <string.h>

namespace nd
{
	namespace Debugging
	{
		template< int SIZE > class FunctionStack: public IFunctionStack
		{
		public:
			FunctionStack( )
			{ clear(); }

			~FunctionStack()
			{ }

			bool isFull() const
			{ return getDepth() >= getMaxDepth(); }

			int getDepth() const
			{ return mDepth; }

			int getMaxDepth() const
			{ return SIZE; }

			void clear()
			{
				memset( mStack, 0, sizeof(void*)*SIZE );
				mDepth = 0;
			}

			void pushReturnAddress( void *aReturnAddress )
			{ mStack[ mDepth++ ] = aReturnAddress; }

			void* getReturnAddress( int aFrame )
			{ return mStack[ aFrame ]; }

		private:
			int mDepth;
			void *mStack[SIZE];
		};
	}
}

#endif
