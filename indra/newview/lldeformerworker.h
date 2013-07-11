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

#ifndef LL_DEFORMER_WORKER_H
#define LL_DEFORMER_WORKER_H

#include "llthread.h"
#include "llsingleton.h"
#include "llpointer.h"
#include "llrefcount.h"
#include "lleventtimer.h"
#include "m4math.h"
#include <vector>
#include <list>

class LLDeformedVolume;
class LLDrawable;

class LLDeformerWorker : LLThread
{
public:

	// request that the worker compute a deform table:
	class Request : public LLRefCount
	{
	public:
		LLPointer<LLDeformedVolume> mDeformedVolume;
		LLPointer<LLDrawable> mDrawable;
		LLUUID mMeshID;
		S32 mFace;
		S32 mVertexCount;
		
		// positions/normals of the volume vertices
		std::vector< LLVector3 > mVolumePositions;
		std::vector< LLVector3 > mVolumeNormals;
		
		// positions/normals/indices of the base avatar vertices
		std::vector<std::vector< LLVector3 > > mAvatarPositions;
		std::vector<std::vector< LLVector3 > > mAvatarNormals;
		std::vector<std::vector< S32 > > mAvatarIndices;


		LLMatrix4 mBindShapeMatrix;
		F32 mScale;
	};
	
	// timer handles completed requests on main thread
	class CompletedTimer : public LLEventTimer
	{
	public:
		CompletedTimer();
		BOOL tick();
	};
	
	
	LLDeformerWorker();
	virtual ~LLDeformerWorker();

	
	// add a request to the Q  (called from main thread)
	static void submitRequest(LLPointer<Request> request);
	
	// see if request is already on queue
	static BOOL alreadyQueued(LLPointer<Request> request);
	
	// do the work of building the deformation table
	void buildDeformation(LLPointer<Request> request);
	
	// do the necessary work on the completed queue (on the main thread)
	static void processCompletedQ();


	void shutdown();
	
	virtual void run();

	CompletedTimer mTimer;

	

	
private:
	void instanceSubmitRequest(LLPointer<Request> request);
	
	LLCondition* mSignal;
	LLMutex* mMutex;
	
	BOOL mQuitting;
	
	typedef std::list< LLPointer<Request> > request_queue;
	
	request_queue mRequestQ;
	request_queue mCompletedQ;
};

extern LLDeformerWorker* gDeformerWorker;

#endif

