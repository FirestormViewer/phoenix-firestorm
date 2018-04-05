/** 
 * @file llthreadsafequeue.h
 * @brief Base classes for thread, mutex and condition handling.
 *
 * $LicenseInfo:firstyear=2004&license=viewerlgpl$
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

#ifndef LL_LLTHREADSAFEQUEUE_H
#define LL_LLTHREADSAFEQUEUE_H

#include "llexception.h"
#include "llmutex.h"
#include "lltimer.h"
#include <string>
#include <deque>

//
// A general queue exception.
//
class LL_COMMON_API LLThreadSafeQueueError:
	public LLException
{
public:
	LLThreadSafeQueueError(std::string const & message):
		LLException(message)
	{
		; // No op.
	}
};


//
// An exception raised when blocking operations are interrupted.
//
class LL_COMMON_API LLThreadSafeQueueInterrupt:
	public LLThreadSafeQueueError
{
public:
	LLThreadSafeQueueInterrupt(void):
		LLThreadSafeQueueError("queue operation interrupted")
	{
		; // No op.
	}
};



//
// Implements a thread safe FIFO.
//
template<typename ElementT>
class LLThreadSafeQueue
{
public:
	typedef ElementT value_type;
	
	// If the pool is set to NULL one will be allocated and managed by this
	// queue.
	LLThreadSafeQueue( U32 capacity = 1024);
	
	// Add an element to the front of queue (will block if the queue has
	// reached capacity).
	//
	// This call will raise an interrupt error if the queue is deleted while
	// the caller is blocked.
	void pushFront(ElementT const & element);
	
	// Try to add an element to the front ofqueue without blocking. Returns
	// true only if the element was actually added.
	bool tryPushFront(ElementT const & element);
	
	// Pop the element at the end of the queue (will block if the queue is
	// empty).
	//
	// This call will raise an interrupt error if the queue is deleted while
	// the caller is blocked.
	ElementT popBack(void);
	
	// Pop an element from the end of the queue if there is one available.
	// Returns true only if an element was popped.
	bool tryPopBack(ElementT & element);
	
	// Returns the size of the queue.
	size_t size();

private:
	std::deque< ElementT > mStorage;
	LLMutex mLock;
	U32 mCapacity; // Really needed?
};



// LLThreadSafeQueue
//-----------------------------------------------------------------------------


template<typename ElementT>
LLThreadSafeQueue<ElementT>::LLThreadSafeQueue( U32 capacity):
	mCapacity( capacity )
{
	; // No op.
}


template<typename ElementT>
void LLThreadSafeQueue<ElementT>::pushFront(ElementT const & element)
{
	while( true )
	{
		{
			LLMutexLock lck( &mLock );
			if( mStorage.size() < mCapacity )
			{
				mStorage.push_front( element );
				return;
			}
		}
		ms_sleep( 100 );
	}
}


template<typename ElementT>
bool LLThreadSafeQueue<ElementT>::tryPushFront(ElementT const & element)
{
	LLMutexTrylock lck( &mLock );
	if( !lck.isLocked() )
		return false;

	if( mStorage.size() >= mCapacity )
		return false;

	mStorage.push_front( element );
	return true;
}


template<typename ElementT>
ElementT LLThreadSafeQueue<ElementT>::popBack(void)
{
	while( true )
	{
		{
			LLMutexLock lck( &mLock );
			if( !mStorage.empty() )
			{
				ElementT value = mStorage.back();
				mStorage.pop_back();
				return value;
			}
		}
		ms_sleep( 100 );
	}
}


template<typename ElementT>
bool LLThreadSafeQueue<ElementT>::tryPopBack(ElementT & element)
{
	LLMutexTrylock lck( &mLock );

	if( !lck.isLocked() )
		return false;
	if( mStorage.empty() )
		return false;

	element = mStorage.back();
	mStorage.pop_back();
	return true;
}


template<typename ElementT>
size_t LLThreadSafeQueue<ElementT>::size(void)
{
	// Nicky: apr_queue_size is/was NOT threadsafe. I still play it safe here and rather lock the storage

	LLMutexLock lck( &mLock );
	return mStorage.size();
}


#endif
