/** 
 * @file streamingaudio_fmodex.h
 * @brief Definition of LLStreamingAudio_FMODEX implementation
 *
 * $LicenseInfo:firstyear=2002&license=viewerlgpl$
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

#ifndef LL_STREAMINGAUDIO_FMODEX_H
#define LL_STREAMINGAUDIO_FMODEX_H

#include "stdtypes.h" // from llcommon

#include "llstreamingaudio.h"
#include "lltimer.h"

//Stubs
class LLAudioStreamManagerFMODEX;
namespace FMOD
{
	class System;
	class Channel;
}

//Interfaces
class LLStreamingAudio_FMODEX : public LLStreamingAudioInterface
{
 public:
	LLStreamingAudio_FMODEX(FMOD::System *system);
	/*virtual*/ ~LLStreamingAudio_FMODEX();

	/*virtual*/ void start(const std::string& url);
	/*virtual*/ void stop();
	/*virtual*/ void pause(S32 pause);
	/*virtual*/ void update();
	/*virtual*/ S32 isPlaying();
	/*virtual*/ void setGain(F32 vol);
	/*virtual*/ F32 getGain();
	/*virtual*/ std::string getURL();

	/*virtual*/ bool supportsAdjustableBufferSizes(){return true;}
	/*virtual*/ void setBufferSizes(U32 streambuffertime, U32 decodebuffertime);

	// <FS:CR> Streamtitle display
	virtual bool getNewMetadata(LLSD& metadata);
	// </FS:CR>

private:
	// <FS> FMOD fixes
	bool releaseDeadStreams();

	FMOD::System *mSystem;

	LLAudioStreamManagerFMODEX *mCurrentInternetStreamp;
	FMOD::Channel *mFMODInternetStreamChannelp;
	std::list<LLAudioStreamManagerFMODEX *> mDeadStreams;

	std::string mURL;
	std::string mPendingURL; // <FS> FMOD fixes
	F32 mGain;

	// <FS:CR> Streamtitle display
	bool mNewMetadata;
	LLSD mMetadata;
	// </FS:CR> Streamtitle display
};


#endif // LL_STREAMINGAUDIO_FMODEX_H
