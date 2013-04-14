/** 
 * @file llstrider.h
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

#ifndef LL_LLSTRIDER_H
#define LL_LLSTRIDER_H

#include "stdtypes.h"

template <class Object> class LLStrider
{
	union
	{
		Object* mObjectp;
		U8*		mBytep;
	};
	U32     mSkip;
public:

	LLStrider()  { mObjectp = NULL; mSkip = sizeof(Object); mBufferEnd = 0; } 
	~LLStrider() { } 

	const LLStrider<Object>& operator =  (Object *first)    { mObjectp = first; mBufferEnd = 0; return *this;}
	void setStride (S32 skipBytes)	{ mSkip = (skipBytes ? skipBytes : sizeof(Object));}

	LLStrider<Object> operator+(const S32& index) 
	{
		LLStrider<Object> ret;
		ret.mBytep = mBytep + mSkip*index;
		ret.mSkip = mSkip;
		ret.mBufferEnd = mBufferEnd;

		return ret;
	}

    void skip(const U32 index)     { mBytep += mSkip*index;}
    U32 getSkip() const            { return mSkip; }

	// <FS:ND> protect against buffer overflows

    // Object* get()                  { return mObjectp; }
    // Object* operator->()           { return mObjectp; }
    // Object& operator *()           { return *mObjectp; }
    // Object* operator ++(int)       { Object* old = mObjectp; mBytep += mSkip; return old; }
    // Object* operator +=(int i)     { mBytep += mSkip*i; return mObjectp; }

    // Object& operator[](U32 index)  { return *(Object*)(mBytep + (mSkip * index)); }

	Object* get()
	{
		if( !assertValid( mBytep ) )
			return &mDummy;

		return mObjectp;
	}

	Object* operator->()
	{
		if( !assertValid( mBytep ) )
			return &mDummy;

		return mObjectp;
	}

	Object& operator *()
	{
		if( !assertValid( mBytep ) )
			return mDummy;

		return *mObjectp;
	}

	Object* operator ++(int)
	{
		Object* old = mObjectp;
		mBytep += mSkip;

		if( !assertValid( (U8*)old ) )
			return &mDummy;

		return old;
	}

	Object* operator +=(int i)
	{
		mBytep += mSkip*i;
		assertValid( mBytep );
		return mObjectp;
	}

	Object& operator[](U32 index)
	{
		if( !assertValid( mBytep + mSkip*index ) )
			return mDummy;

		return *(Object*)(mBytep + (mSkip * index));
	}

	void setCount( U32 aCount )
	{
		mBufferEnd = mBytep + mSkip*aCount;
#if LL_RELEASE_WITH_DEBUG_INFO || LL_DEBUG
		mCount = aCount;
#endif
	}

	bool assertValid( U8 const *aBuffer )
	{
		if( !aBuffer || !mBufferEnd )
			return true;
		if( aBuffer < mBufferEnd )
			return true;

		// For final release, jsut warn and return true.
		llwarns << "Vertex buffer access beyond end of VBO" << llendl;
		llassert( false );
		return true;
	}

private:
	U8 *mBufferEnd;

#if LL_RELEASE_WITH_DEBUG_INFO || LL_DEBUG
	U32 mCount;
#endif

	Object mDummy;
	// <FS:ND>
};

#endif // LL_LLSTRIDER_H
