/** 
 * @file llimageworker.h
 * @brief Object for managing images and their textures.
 *
 * $LicenseInfo:firstyear=2000&license=viewerlgpl$
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

#ifndef LL_LLIMAGEWORKER_H
#define LL_LLIMAGEWORKER_H

#include "llimage.h"
#include "llpointer.h"
#include "llworkerthread.h"

 // <FS:ND/> Image thread pool
class PoolWorkerThread;

class LLImageDecodeThread : public LLQueuedThread
{
public:
	class Responder : public LLThreadSafeRefCount
	{
	protected:
		virtual ~Responder();
	public:
		virtual void completed(bool success, LLImageRaw* raw, LLImageRaw* aux) = 0;
	};

	class ImageRequest : public LLQueuedThread::QueuedRequest
	{
	protected:
		virtual ~ImageRequest(); // use deleteRequest()
		
	public:
		ImageRequest(handle_t handle, LLImageFormatted* image,
					 U32 priority, S32 discard, BOOL needs_aux,
					 LLImageDecodeThread::Responder* responder, LLImageDecodeThread *aQueue);

		/*virtual*/ bool processRequest();
		bool processRequestIntern();
		/*virtual*/ void finishRequest(bool completed);

		// Used by unit tests to check the consitency of the request instance
		bool tut_isOK();
		
	private:
		// input
		LLPointer<LLImageFormatted> mFormattedImage;
		S32 mDiscardLevel;
		BOOL mNeedsAux;
		// output
		LLPointer<LLImageRaw> mDecodedImageRaw;
		LLPointer<LLImageRaw> mDecodedImageAux;
		LLImageDecodeThread * mQueue; // <FS:ND> Image thread pool from CoolVL
		BOOL mDecodedRaw;
		BOOL mDecodedAux;
		LLPointer<LLImageDecodeThread::Responder> mResponder;
	};
	
public:
	// <FS:ND> Image thread pool from CoolVL
	//LLImageDecodeThread(bool threaded = true);
	LLImageDecodeThread(bool threaded = true, U32 aSubThreads = 0 );
	// </FS:ND>

	virtual ~LLImageDecodeThread();

	handle_t decodeImage(LLImageFormatted* image,
						 U32 priority, S32 discard, BOOL needs_aux,
						 Responder* responder);
	S32 update(F32 max_time_ms);

	// Used by unit tests to check the consistency of the thread instance
	S32 tut_size();
	
private:
	struct creation_info
	{
		handle_t handle;
		LLPointer<LLImageFormatted> image;
		U32 priority;
		S32 discard;
		BOOL needs_aux;
		LLPointer<Responder> responder;
		creation_info(handle_t h, LLImageFormatted* i, U32 p, S32 d, BOOL aux, Responder* r)
			: handle(h), image(i), priority(p), discard(d), needs_aux(aux), responder(r)
		{}
	};
	typedef std::list<creation_info> creation_list_t;
	creation_list_t mCreationList;
	LLMutex* mCreationMutex;

	// <FS:ND> Image thread pool from CoolVL
	std::vector< std::shared_ptr< PoolWorkerThread > > mThreadPool;
	bool enqueRequest(ImageRequest*);
	// <FS:ND>
};

#endif
