/** 
 * @file llmutex.h
 * @brief Base classes for mutex and condition handling.
 *
 * $LicenseInfo:firstyear=2004&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2012, Linden Research, Inc.
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

#ifndef LL_LLMUTEX_H
#define LL_LLMUTEX_H

#include "stdtypes.h"
#include "lltimer.h"

#if LL_WINDOWS
#pragma warning(disable:4265)
#endif

#include <mutex>
#include <condition_variable>

#if LL_WINDOWS
#pragma warning(default:4265)
#endif
//============================================================================

#define MUTEX_DEBUG (LL_DEBUG || LL_RELEASE_WITH_DEBUG_INFO)

#if MUTEX_DEBUG
#include <map>
#endif


class LL_COMMON_API LLMutex
{
public:
	typedef enum
	{
		NO_THREAD = 0xFFFFFFFF
	} e_locking_thread;

	LLMutex(); // NULL pool constructs a new pool for the mutex
	virtual ~LLMutex();
	
	void lock();		// blocks
	bool trylock();		// non-blocking, returns true if lock held.
	void unlock();		// undefined behavior when called on mutex not being held
	bool isLocked(); 	// non-blocking, but does do a lock/unlock so not free
	bool isSelfLocked(); //return true if locked in a same thread
	U32 lockingThread() const; //get ID of locking thread
	
protected:
	std::mutex mMutex;
	mutable U32			mCount;
	mutable U32			mLockingThread;
	
	bool				mIsLocalPool;
	
#if MUTEX_DEBUG
	std::map<U32, BOOL> mIsLocked;
#endif
};

// Actually a condition/mutex pair (since each condition needs to be associated with a mutex).
class LL_COMMON_API LLCondition : public LLMutex
{
public:
	LLCondition(); // Defaults to global pool, could use the thread pool as well.
	~LLCondition();
	
	void wait();		// blocks
	void signal();
	void broadcast();
	
protected:
	std::condition_variable mCond;
};

class LLMutexLock
{
public:
	LLMutexLock(LLMutex* mutex)
	{
		mMutex = mutex;
		
		if(mMutex)
			mMutex->lock();
	}
	~LLMutexLock()
	{
		if(mMutex)
			mMutex->unlock();
	}
private:
	LLMutex* mMutex;
};

//============================================================================

// Scoped locking class similar in function to LLMutexLock but uses
// the trylock() method to conditionally acquire lock without
// blocking.  Caller resolves the resulting condition by calling
// the isLocked() method and either punts or continues as indicated.
//
// Mostly of interest to callers needing to avoid stalls who can
// guarantee another attempt at a later time.

class LLMutexTrylock
{
public:
	LLMutexTrylock(LLMutex* mutex)
		: mMutex(mutex),
		  mLocked(false)
	{
		if (mMutex)
			mLocked = mMutex->trylock();
	}
	LLMutexTrylock( LLMutex* mutex, U32 aTries )
		: mMutex( mutex ),
		mLocked( false )
	{
		if( !mMutex )
			return;

		U32 i = 0;
		while( i < aTries )
		{
			mLocked = mMutex->trylock();
			if( mLocked )
				break;
			++i;
			ms_sleep( 10 );
		}
	}

	~LLMutexTrylock()
	{
		if (mMutex && mLocked)
			mMutex->unlock();
	}

	bool isLocked() const
	{
		return mLocked;
	}
	
private:
	LLMutex*	mMutex;
	bool		mLocked;
};
#endif // LL_LLTHREAD_H
