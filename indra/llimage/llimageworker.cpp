/** 
 * @file llimageworker.cpp
 * @brief Base class for images.
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
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
#include "fstelemetry.h" // <FS:Beq> add telemetry support.
#include "llimageworker.h"
#include "llimagedxt.h"

 // <FS:ND> Image thread pool from CoolVL
#include "boost/thread.hpp"
std::atomic< U32 > s_ChildThreads;

class PoolWorkerThread : public LLThread
{
public:
	PoolWorkerThread(std::string name) : LLThread(name),
		mCurrentRequest(NULL)
	{
	}
	virtual void run()
	{
		while (!isQuitting())
		{
			auto *pReq = mCurrentRequest.exchange(nullptr);

			if (pReq)
				pReq->processRequestIntern();
			checkPause();
		}
	}
	bool isBusy()
	{
		auto *pReq = mCurrentRequest.load();
		if (!pReq)
			return false;

		auto status = pReq->getStatus();

		return status  == LLQueuedThread::STATUS_QUEUED || status == LLQueuedThread::STATUS_INPROGRESS;
	}

	bool runCondition()
	{
		return mCurrentRequest != NULL;
	}

	bool setRequest(LLImageDecodeThread::ImageRequest* req)
	{
		LLImageDecodeThread::ImageRequest* pOld{ nullptr };
		bool bSuccess = mCurrentRequest.compare_exchange_strong(pOld, req);
		wake();

		return bSuccess;
	}

private:
	std::atomic< LLImageDecodeThread::ImageRequest * > mCurrentRequest;
};
// </FS:ND>

//----------------------------------------------------------------------------

// MAIN THREAD
LLImageDecodeThread::LLImageDecodeThread(bool threaded, U32 aSubThreads)
	: LLQueuedThread("imagedecode", threaded)
{
	mCreationMutex = new LLMutex();

	// <FS:ND> Image thread pool from CoolVL
	if (aSubThreads == 0)
	{
		aSubThreads = boost::thread::hardware_concurrency();
		if (!aSubThreads)
			aSubThreads = 4U; // Use a sane default: 4 cores
		if (aSubThreads > 8U)
		{
			// Using number of (virtual) cores - 1 (for the main image worker
			// thread) - 1 (for the viewer main loop thread), further bound to
			// a maximum of 32 threads (more than that is totally useless, even
			// when flying over main land with 512m draw distance).
			aSubThreads = llmin(aSubThreads - 2U, 32U);
		}
		else if (aSubThreads > 2U)
		{
			// Using number of (virtual) cores - 1 (for the main image worker
			// thread).
			--aSubThreads;
		}
	}
	else if (aSubThreads == 1) // Disable if only 1
		aSubThreads = 0;

	s_ChildThreads = aSubThreads;
	for (U32 i = 0; i < aSubThreads; ++i)
	{
		std::stringstream strm;
		strm << "imagedecodethread" << (i + 1);

		mThreadPool.push_back(std::make_shared< PoolWorkerThread>(strm.str()));
		mThreadPool[i]->start();
	}
	// </FS:ND>
}

//virtual 
LLImageDecodeThread::~LLImageDecodeThread()
{
	delete mCreationMutex ;
}

// MAIN THREAD
// virtual
S32 LLImageDecodeThread::update(F32 max_time_ms)
{
	FSZoneC(tracy::Color::Blue); // <FS:Beq/> instrument image decodes
	LLMutexLock lock(mCreationMutex);
	// <FS:Beq> instrument image decodes
	{
	FSZoneC(tracy::Color::Blue1);
	// </FS:Beq>
	for (creation_list_t::iterator iter = mCreationList.begin();
		 iter != mCreationList.end(); ++iter)
	{
		creation_info& info = *iter;
		// ImageRequest* req = new ImageRequest(info.handle, info.image,
		//				     info.priority, info.discard, info.needs_aux,
		//				     info.responder);
		ImageRequest* req = new ImageRequest(info.handle, info.image,
			info.priority, info.discard, info.needs_aux,
			info.responder, this);

		bool res = addRequest(req);
		if (!res)
		{
			LL_WARNS() << "request added after LLLFSThread::cleanupClass()" << LL_ENDL;
			return 0;
		}
	}
	mCreationList.clear();
	// <FS:Beq> instrument image decodes
	}
	{
	FSZoneC(tracy::Color::Blue2);
	// </FS:Beq>
	S32 res = LLQueuedThread::update(max_time_ms);
	// FSPlot("img_decode_pending", (int64_t)res); 	// <FS:Beq/> instrument image decodes
	return res;
	} 	// <FS:Beq/> instrument image decodes
}

LLImageDecodeThread::handle_t LLImageDecodeThread::decodeImage(LLImageFormatted* image, 
	U32 priority, S32 discard, BOOL needs_aux, Responder* responder)
{
	FSZoneC(tracy::Color::Orange); // <FS:Beq> instrument the image decode pipeline
	// <FS:Beq> De-couple texture threading from mainloop
	// LLMutexLock lock(mCreationMutex);
	// handle_t handle = generateHandle();
	// mCreationList.push_back(creation_info(handle, image, priority, discard, needs_aux, responder));
	handle_t handle = generateHandle();
	// If we have a thread pool dispatch this directly.
	// Note: addRequest could cause the handling to take place on the fetch thread, this is unlikely to be an issue. 
	// if this is an actual problem we move the fallback to here and place the unfulfilled request into the legacy queue
	if (s_ChildThreads > 0)
	{
		FSZoneNC("DecodeDecoupled", tracy::Color::Orange); // <FS:Beq> instrument the image decode pipeline
		ImageRequest* req = new ImageRequest(handle, image,
			priority, discard, needs_aux,
			responder, this);
		bool res = addRequest(req);
		if (!res)
		{
			LL_WARNS() << "Decode request not added because we are exiting." << LL_ENDL;
			return 0;
		}
	}
	else
	{
		FSZoneNC("DecodeQueued", tracy::Color::Orange); // <FS:Beq> instrument the image decode pipeline
		LLMutexLock lock(mCreationMutex);
		mCreationList.push_back(creation_info(handle, image, priority, discard, needs_aux, responder));
	}
	// </FS:Beq>
	return handle;
}

// Used by unit test only
// Returns the size of the mutex guarded list as an indication of sanity
S32 LLImageDecodeThread::tut_size()
{
	LLMutexLock lock(mCreationMutex);
	S32 res = mCreationList.size();
	return res;
}

LLImageDecodeThread::Responder::~Responder()
{
}

//----------------------------------------------------------------------------

LLImageDecodeThread::ImageRequest::ImageRequest(handle_t handle, LLImageFormatted* image, 
												U32 priority, S32 discard, BOOL needs_aux,
												LLImageDecodeThread::Responder* responder,
												LLImageDecodeThread *aQueue)
	: LLQueuedThread::QueuedRequest(handle, priority, FLAG_AUTO_COMPLETE),
	  mFormattedImage(image),
	  mDiscardLevel(discard),
	  mNeedsAux(needs_aux),
	  mDecodedRaw(FALSE),
	  mDecodedAux(FALSE),
	  mResponder(responder),
      mQueue( aQueue ) // <FS:ND/> Image thread pool from CoolVL
{
	//<FS:ND> Image thread pool from CoolVL
	if (s_ChildThreads > 0)
		mFlags |= FLAG_ASYNC;
	// </FS:ND>
}

LLImageDecodeThread::ImageRequest::~ImageRequest()
{
	mDecodedImageRaw = NULL;
	mDecodedImageAux = NULL;
	mFormattedImage = NULL;
}

//----------------------------------------------------------------------------


// Returns true when done, whether or not decode was successful.
bool LLImageDecodeThread::ImageRequest::processRequest()
{
	// <FS:ND> Image thread pool from CoolVL

	// If not async, decode using this thread
	if ((mFlags & FLAG_ASYNC) == 0)
		return processRequestIntern();

	// Try to dispatch to a new thread, if this isn't possible decode on this thread
	if (!mQueue->enqueRequest(this))
		return processRequestIntern();
	return true;
	// </FS:ND>
}

bool LLImageDecodeThread::ImageRequest::processRequestIntern()
{
	// <FS:Beq> allow longer timeout for async and add instrumentation
	// const F32 decode_time_slice = .1f;
	FSZoneC(tracy::Color::DarkOrange); 
	F32 decode_time_slice = .1f;
	if(mFlags & FLAG_ASYNC)
	{
		decode_time_slice = 10.0f;// long time out as this is not an issue with async
	}
	// </FS:Beq>
	bool done = true;
	if (!mDecodedRaw && mFormattedImage.notNull())
	{
		FSZoneC(tracy::Color::DarkOrange1); // <FS:Beq> instrument the image decode pipeline
		// Decode primary channels
		if (mDecodedImageRaw.isNull())
		{
			// parse formatted header
			if (!mFormattedImage->updateData())
			{
				return true; // done (failed)
			}
			if (0 == (mFormattedImage->getWidth() * mFormattedImage->getHeight() * mFormattedImage->getComponents()))
			{
				return true; // done (failed)
			}
			if (mDiscardLevel >= 0)
			{
				mFormattedImage->setDiscardLevel(mDiscardLevel);
			}
			mDecodedImageRaw = new LLImageRaw(mFormattedImage->getWidth(),
											  mFormattedImage->getHeight(),
											  mFormattedImage->getComponents());
		}

		// <FS:ND> Probably out of memory crash
		// done = mFormattedImage->decode(mDecodedImageRaw, decode_time_slice); // 1ms
		if( mDecodedImageRaw->getData() )
			done = mFormattedImage->decode(mDecodedImageRaw, decode_time_slice); // 1ms
		else
		{
			LL_WARNS() << "No memory for LLImageRaw of size " << (U32)mFormattedImage->getWidth() << "x" << (U32)mFormattedImage->getHeight() << "x"
					   << (U32)mFormattedImage->getComponents() << LL_ENDL;
			done = false;
		}
		// </FS:ND>
		
		// some decoders are removing data when task is complete and there were errors
		mDecodedRaw = done && mDecodedImageRaw->getData();
	}
	if (done && mNeedsAux && !mDecodedAux && mFormattedImage.notNull())
	{
		FSZoneC(tracy::Color::DarkOrange2); // <FS:Beq> instrument the image decode pipeline
		// Decode aux channel
		if (!mDecodedImageAux)
		{
			mDecodedImageAux = new LLImageRaw(mFormattedImage->getWidth(),
											  mFormattedImage->getHeight(),
											  1);
		}
		done = mFormattedImage->decodeChannels(mDecodedImageAux, decode_time_slice, 4, 4); // 1ms
		mDecodedAux = done && mDecodedImageAux->getData();
	}
	// <FS:Beq> report timeout on async thread (which leads to worker abort errors)
	if(!done)
	{
		LL_WARNS("ImageDecode") << "Image decoding failed to complete with time slice=" << decode_time_slice << LL_ENDL;
	}
	// </FS:Beq>
	//<FS:ND> Image thread pool from CoolVL
	if (mFlags & FLAG_ASYNC)
	{
		setStatus(STATUS_COMPLETE);
		finishRequest(true);
		// always autocomplete
		mQueue->completeRequest(mHashKey);
	}
	// </FS:ND>
	return done;
}

void LLImageDecodeThread::ImageRequest::finishRequest(bool completed)
{
	if (mResponder.notNull())
	{
		bool success = completed && mDecodedRaw && (!mNeedsAux || mDecodedAux);
		mResponder->completed(success, mDecodedImageRaw, mDecodedImageAux);
	}
	// Will automatically be deleted
}

// Used by unit test only
// Checks that a responder exists for this instance so that something can happen when completion is reached
bool LLImageDecodeThread::ImageRequest::tut_isOK()
{
	return mResponder.notNull();
}

bool LLImageDecodeThread::enqueRequest(ImageRequest * req)
{
	for (auto &pThread : mThreadPool)
	{
		if (!pThread->isBusy())
		{
			if( pThread->setRequest(req) )
				return true;
		}
	}
	return false;
}
