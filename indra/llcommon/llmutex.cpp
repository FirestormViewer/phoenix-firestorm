/** 
 * @file llmutex.cpp
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

#include "linden_common.h"
#include "llapr.h"

#include "apr_portable.h"

#include "llmutex.h"
#include "llthread.h"

//============================================================================

LLMutex::LLMutex() :
 mCount(0), mLockingThread(NO_THREAD)
{

	mMutexp = new std::mutex();
}


LLMutex::~LLMutex()
{
#if MUTEX_DEBUG
	//bad assertion, the subclass LLSignal might be "locked", and that's OK
	//llassert_always(!isLocked()); // better not be locked!
#endif
	delete mMutexp;
	mMutexp = nullptr;
}


void LLMutex::lock()
{
	if(isSelfLocked())
	{ //redundant lock
		mCount++;
		return;
	}
	
	mMutexp->lock();
	
#if MUTEX_DEBUG
	// Have to have the lock before we can access the debug info
	U32 id = LLThread::currentID();
	if (mIsLocked[id] != FALSE)
		LL_ERRS() << "Already locked in Thread: " << id << LL_ENDL;
	mIsLocked[id] = TRUE;
#endif

	mLockingThread = LLThread::currentID();
}

void LLMutex::unlock()
{
	if (mCount > 0)
	{ //not the root unlock
		mCount--;
		return;
	}
	
#if MUTEX_DEBUG
	// Access the debug info while we have the lock
	U32 id = LLThread::currentID();
	if (mIsLocked[id] != TRUE)
		LL_ERRS() << "Not locked in Thread: " << id << LL_ENDL;	
	mIsLocked[id] = FALSE;
#endif

	mLockingThread = NO_THREAD;
	mMutexp->unlock();
}

bool LLMutex::isLocked()
{
	if (!mMutexp->try_lock())
	{
		return true;
	}
	else
	{
		mMutexp->unlock();
		return false;
	}
}

bool LLMutex::isSelfLocked()
{
	return mLockingThread == LLThread::currentID();
}

U32 LLMutex::lockingThread() const
{
	return mLockingThread;
}

bool LLMutex::trylock()
{
	if(isSelfLocked())
	{ //redundant lock
		mCount++;
		return true;
	}
	
	if (!mMutexp->try_lock())
	{
		return false;
	}
	
#if MUTEX_DEBUG
	// Have to have the lock before we can access the debug info
	U32 id = LLThread::currentID();
	if (mIsLocked[id] != FALSE)
		LL_ERRS() << "Already locked in Thread: " << id << LL_ENDL;
	mIsLocked[id] = TRUE;
#endif

	mLockingThread = LLThread::currentID();
	return true;
}

//============================================================================

LLCondition::LLCondition()

{
	// base class (LLMutex) has already ensured that mAPRPoolp is set up.
	mCondp = new std::condition_variable();
}


LLCondition::~LLCondition()
{
	delete mCondp;
	mCondp = nullptr;
}


void LLCondition::wait()
{
	std::unique_lock< std::mutex > lock( *mMutexp );
	mCondp->wait( lock );
}

void LLCondition::signal()
{
	mCondp->notify_one() ;
}

void LLCondition::broadcast()
{
	mCondp->notify_all();
}


//============================================================================
