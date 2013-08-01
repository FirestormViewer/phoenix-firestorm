/** 
 * @file lldeformerworker.h
 * @brief Threaded worker to compute deformation tables
 *
 * $LicenseInfo:firstyear=2012&license=viewerlgpl$
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

#include "llviewerprecompiledheaders.h"

#include "lldrawable.h"
#include "llvoavatar.h"
#include "llvovolume.h"
#include "llviewerobjectlist.h"

#include "lldeformerworker.h"

// timer to watch the completed queue and process completed requests
// (needs to be done on main thread, hence this timer on 1 sec repeat.)
LLDeformerWorker::CompletedTimer::CompletedTimer() : LLEventTimer(1.0)
{
}

BOOL LLDeformerWorker::CompletedTimer::tick()
{
	LLDeformerWorker::processCompletedQ();
	return FALSE;
}

// yes, global instance - 
// LLSingleton appears to be incompatible with LLThread - they both keep track of the instance and
// want to delete it.

LLDeformerWorker* gDeformerWorker;




LLDeformerWorker::LLDeformerWorker()
: LLThread("Deformer Worker")
{
    mQuitting = false;
	
	mSignal = new LLCondition(NULL);
    mMutex = new LLMutex(NULL);
}


LLDeformerWorker::~LLDeformerWorker()
{
    shutdown();
	
	if (mSignal)
	{
		delete mSignal;
		mSignal = NULL;
	}
	
	if (mMutex)
	{
		delete mMutex;
		mMutex = NULL;
	}
}


void LLDeformerWorker::shutdown()
{
    if (mSignal)
    {
        mQuitting = true;
        mSignal->signal();
				
		while (!isStopped())
        {
            apr_sleep(10);
        }
    }
}


// static
void LLDeformerWorker::submitRequest(LLPointer<Request> request)
{
	if (!gDeformerWorker)
	{
		gDeformerWorker = new LLDeformerWorker;
	}
	
	gDeformerWorker->instanceSubmitRequest(request);
}


void LLDeformerWorker::instanceSubmitRequest(LLPointer<Request> request)
{
	if (isStopped())
	{
		start();
	}
	
    LLMutexLock lock(mMutex);
    mRequestQ.push_back(request);
    mSignal->signal();
}


// static
BOOL LLDeformerWorker::alreadyQueued(LLPointer<Request> request)
{
	if (!gDeformerWorker)
	{
		return FALSE;
	}
	
	for (request_queue::iterator iterator = gDeformerWorker->mRequestQ.begin();
		 iterator != gDeformerWorker->mRequestQ.end();
		 iterator++)
	{
		// the request is already here if we match MeshID, VertexCount, and Face
		
		LLPointer<LLDeformerWorker::Request> old_request = *iterator;
		

		if ((request->mMeshID == old_request->mMeshID) &&
			(request->mFace == old_request->mFace) &&
			(request->mVertexCount == old_request->mVertexCount))
		{
			return TRUE;
		}
		
	}
	
	return FALSE;
	
}


void LLDeformerWorker::buildDeformation(LLPointer<Request> request)
{
	request->mDeformedVolume->computeDeformTable(request);
}

void LLDeformerWorker::run()
{
	while (!mQuitting)
	{
		mSignal->wait();
		while (!mQuitting && !mRequestQ.empty())
		{
			LLPointer<Request> request;
			
			{
				// alter Q while locked
				LLMutexLock lock(mMutex);
				request = mRequestQ.front();
				mRequestQ.pop_front();
			}
			
			buildDeformation(request);
			
			{
				// alter Q while locked
				LLMutexLock lock(mMutex);
				mCompletedQ.push_back(request);
			}
			
		}

	}
}

// static
void LLDeformerWorker::processCompletedQ()
{
	if (!gDeformerWorker)
	{
		return;
	}
	
	while ( !gDeformerWorker->mCompletedQ.empty())
	{
		LLPointer<Request> request;
		
		{
			// alter Q while locked
			LLMutexLock lock(gDeformerWorker->mMutex);
			request = gDeformerWorker->mCompletedQ.front();
			gDeformerWorker->mCompletedQ.pop_front();
		}
		
		// tag the drawable for rebuild
		request->mDrawable->setState(LLDrawable::REBUILD_ALL);
	}
}	
