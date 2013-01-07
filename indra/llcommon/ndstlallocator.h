#ifndef NDSTLALLOCATOR_H
#define NDSTLALLOCATOR_H

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

#include <limits>
#include <stdlib.h>
#include <cstddef>

namespace nd
{
	namespace stl
	{
		template <typename T> class allocator
		{
		public:
			typedef size_t    size_type;
			typedef ptrdiff_t difference_type;
			typedef T*        pointer;
			typedef const T*  const_pointer;
			typedef T&        reference;
			typedef const T&  const_reference;
			typedef T         value_type;

			template <typename V>	struct rebind
			{ typedef allocator<V> other; };

			pointer address (reference value) const
			{ return &value; }
			
			const_pointer address (const_reference value) const
			{ return &value; }

			allocator()
			{ }

			allocator(const allocator&)
			{ }

			template <typename V> allocator (const allocator<V>&)
			{ }
			
			~allocator()
			{ }

			size_type max_size () const
			{ return std::numeric_limits<size_t>::max() / sizeof(T); }

			pointer allocate (size_type num)
			{ return (pointer) ::malloc (num*sizeof(T)); }

			void construct (pointer p, const T& value)
			{ new((void*)p)T(value); }

			void destroy (pointer p)
			{ p->~T(); }

			void deallocate (pointer p, size_type num)
			{ ::free((void*)p); }
		};

		template <typename T1, typename T2> bool operator== (const allocator<T1>&, const allocator<T2>&) 
		{ return true; }
		template <typename T1, typename T2> bool operator!= (const allocator<T1>&, const allocator<T2>&) 
		{ return false; }
	}
}

#endif
