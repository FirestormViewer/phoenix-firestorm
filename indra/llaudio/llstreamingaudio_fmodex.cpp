/** 
 * @file streamingaudio_fmodex.cpp
 * @brief LLStreamingAudio_FMODEX implementation
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

#include "linden_common.h"

#include "llmath.h"

#include "fmod.hpp"
#include "fmod_errors.h"

#include "llstreamingaudio_fmodex.h"

// <FS> FMOD fixes
static inline bool Check_FMOD_Error(FMOD_RESULT result, const char *string)
{
	if (result == FMOD_OK)
		return false;
	LL_WARNS("AudioImpl") << string << " Error: " << FMOD_ErrorString(result) << LL_ENDL;
	return true;
}
// </FS>

class LLAudioStreamManagerFMODEX
{
public:
	LLAudioStreamManagerFMODEX(FMOD::System *system, const std::string& url);
	FMOD::Channel* startStream();
	bool stopStream(); // Returns true if the stream was successfully stopped.
	bool ready();

	const std::string& getURL() 	{ return mInternetStreamURL; }

	// <FS> FMOD fixes
	//FMOD_OPENSTATE getOpenState(unsigned int* percentbuffered=NULL, bool* starving=NULL, bool* diskbusy=NULL);
	FMOD_RESULT getOpenState(FMOD_OPENSTATE& openstate, unsigned int* percentbuffered = NULL, bool* starving = NULL, bool* diskbusy = NULL);
	// </FS>
protected:
	FMOD::System* mSystem;
	FMOD::Channel* mStreamChannel;
	FMOD::Sound* mInternetStream;
	bool mReady;

	std::string mInternetStreamURL;
};



//---------------------------------------------------------------------------
// Internet Streaming
//---------------------------------------------------------------------------
LLStreamingAudio_FMODEX::LLStreamingAudio_FMODEX(FMOD::System *system) :
	mSystem(system),
	mCurrentInternetStreamp(NULL),
	mFMODInternetStreamChannelp(NULL),
	mGain(1.0f)
{
	// <FS> FMOD fixes
	FMOD_RESULT result;

	// Number of milliseconds of audio to buffer for the audio card.
	// Must be larger than the usual Second Life frame stutter time.
	const U32 buffer_seconds = 10;		//sec
	const U32 estimated_bitrate = 128;	//kbit/sec
	// <FS> FMOD fixes
	//mSystem->setStreamBufferSize(estimated_bitrate * buffer_seconds * 128/*bytes/kbit*/, FMOD_TIMEUNIT_RAWBYTES);
	result = mSystem->setStreamBufferSize(estimated_bitrate * buffer_seconds * 128/*bytes/kbit*/, FMOD_TIMEUNIT_RAWBYTES);
	Check_FMOD_Error(result, "FMOD::System::setStreamBufferSize");
	// </FS>

	// Here's where we set the size of the network buffer and some buffering 
	// parameters.  In this case we want a network buffer of 16k, we want it 
	// to prebuffer 40% of that when we first connect, and we want it 
	// to rebuffer 80% of that whenever we encounter a buffer underrun.

	// Leave the net buffer properties at the default.
	//FSOUND_Stream_Net_SetBufferProperties(20000, 40, 80);
}


LLStreamingAudio_FMODEX::~LLStreamingAudio_FMODEX()
{
	// <FS> FMOD fixes
	//// nothing interesting/safe to do.
	LL_INFOS() << "LLStreamingAudio_FMODEX::~LLStreamingAudio_FMODEX() destructing FMOD Ex Streaming" << LL_ENDL;
	stop();
	for (U32 i = 0; i < 100; ++i)
	{
		if (releaseDeadStreams())
			break;
		ms_sleep(10);
	}
	LL_INFOS() << "LLStreamingAudio_FMODEX::~LLStreamingAudio_FMODEX() finished" << LL_ENDL;
	// </FS>
}

void LLStreamingAudio_FMODEX::start(const std::string& url)
{
	//if (!mInited)
	//{
	//	LL_WARNS() << "startInternetStream before audio initialized" << LL_ENDL;
	//	return;
	//}

	// "stop" stream but don't clear url, etc. in case url == mInternetStreamURL
	stop();

	if (!url.empty())
	{
		// <FS> FMOD fixes
		//LL_INFOS() << "Starting internet stream: " << url << LL_ENDL;
		//mCurrentInternetStreamp = new LLAudioStreamManagerFMODEX(mSystem,url);
		//mURL = url;
		if(mDeadStreams.empty())
		{
			LL_INFOS() << "Starting internet stream: " << url << LL_ENDL;
			mCurrentInternetStreamp = new LLAudioStreamManagerFMODEX(mSystem,url);
			mURL = url;
		}
		else
		{
			LL_INFOS() << "Deferring stream load until buffer release: " << url << LL_ENDL;
			mPendingURL = url;
		}
		// <FS>
	}
	else
	{
		LL_INFOS() << "Set internet stream to null" << LL_ENDL;
		mURL.clear();
	}
}


void LLStreamingAudio_FMODEX::update()
{
	// <FS> FMOD fixes
	//// Kill dead internet streams, if possible
	//std::list<LLAudioStreamManagerFMODEX *>::iterator iter;
	//for (iter = mDeadStreams.begin(); iter != mDeadStreams.end();)
	//{
	//	LLAudioStreamManagerFMODEX *streamp = *iter;
	//	if (streamp->stopStream())
	//	{
	//		LL_INFOS() << "Closed dead stream" << LL_ENDL;
	//		delete streamp;
	//		mDeadStreams.erase(iter++);
	//	}
	//	else
	//	{
	//		iter++;
	//	}
	//}
	if (!releaseDeadStreams())
	{
		llassert_always(mCurrentInternetStreamp == NULL);
		return;
	}

	if(!mPendingURL.empty())
	{
		llassert_always(mCurrentInternetStreamp == NULL);
		LL_INFOS() << "Starting internet stream: " << mPendingURL << LL_ENDL;
		mCurrentInternetStreamp = new LLAudioStreamManagerFMODEX(mSystem,mPendingURL);
		mURL = mPendingURL;
		mPendingURL.clear();
	}
	// </FS>

	// Don't do anything if there are no streams playing
	if (!mCurrentInternetStreamp)
	{
		return;
	}

	unsigned int progress;
	bool starving;
	bool diskbusy;
	// <FS> FMOD fixes
	//FMOD_OPENSTATE open_state = mCurrentInternetStreamp->getOpenState(&progress, &starving, &diskbusy);

	//if (open_state == FMOD_OPENSTATE_READY)

	FMOD_OPENSTATE open_state;
	FMOD_RESULT result = mCurrentInternetStreamp->getOpenState(open_state, &progress, &starving, &diskbusy);

	if (result != FMOD_OK || open_state == FMOD_OPENSTATE_ERROR)
	{
		stop();
		return;
	}
	else if (open_state == FMOD_OPENSTATE_READY)
	// </FS>
	{
		// Stream is live

		// start the stream if it's ready
		if (!mFMODInternetStreamChannelp &&
			(mFMODInternetStreamChannelp = mCurrentInternetStreamp->startStream()))
		{
			// Reset volume to previously set volume
			setGain(getGain());
			// <FS> FMOD fixes
			//mFMODInternetStreamChannelp->setPaused(false);
			Check_FMOD_Error(mFMODInternetStreamChannelp->setPaused(false), "FMOD::Channel::setPaused");
			// </FS>
		}
	}
	// <FS> FMOD fixes
	//else if(open_state == FMOD_OPENSTATE_ERROR)
	//{
	//	stop();
	//	return;
	//}
	// </FS>

	if(mFMODInternetStreamChannelp)
	{
		FMOD::Sound *sound = NULL;

		// <FS:CR> FmodEX Error checking
		//if(mFMODInternetStreamChannelp->getCurrentSound(&sound) == FMOD_OK && sound)
		if (!Check_FMOD_Error(mFMODInternetStreamChannelp->getCurrentSound(&sound), "FMOD::Channel::getCurrentSound") && sound)
		// </FS:CR>
		{
			FMOD_TAG tag;
			S32 tagcount, dirtytagcount;

			// <FS:CR> FmodEX Error checking
			//if(sound->getNumTags(&tagcount, &dirtytagcount) == FMOD_OK && dirtytagcount)
			if (!Check_FMOD_Error(sound->getNumTags(&tagcount, &dirtytagcount), "FMOD::Sound::getNumTags") && dirtytagcount)
			// </FS:CR>
			{
				// <FS:CR> Stream metadata - originally by Shyotl Khur
				mMetadata.clear();
				mNewMetadata = true;
				// </FS:CR>
				for(S32 i = 0; i < tagcount; ++i)
				{
					if(sound->getTag(NULL, i, &tag) != FMOD_OK)
						continue;

					// <FS:CR> Stream metadata - originally by Shyotl Khur
					//if (tag.type == FMOD_TAGTYPE_FMOD)
					//{
					//	if (!strcmp(tag.name, "Sample Rate Change"))
					//	{
					//		LL_INFOS("FmodEX") << "Stream forced changing sample rate to " << *((float *)tag.data) << LL_ENDL;
					//		mFMODInternetStreamChannelp->setFrequency(*((float *)tag.data));
					//	}
					//	continue;
					//}
					std::string name = tag.name;
					switch(tag.type)
					{
						case(FMOD_TAGTYPE_ID3V2):
						{
							if(name == "TIT2") name = "TITLE";
							else if(name == "TPE1") name = "ARTIST";
							break;
						}
						case(FMOD_TAGTYPE_ASF):
						{
							if(name == "Title") name = "TITLE";
							else if(name == "WM/AlbumArtist") name = "ARTIST";
							break;
						}
						case(FMOD_TAGTYPE_FMOD):
						{
							if (!strcmp(tag.name, "Sample Rate Change"))
							{
							LL_INFOS() << "Stream forced changing sample rate to " << *((float *)tag.data) << LL_ENDL;
								mFMODInternetStreamChannelp->setFrequency(*((float *)tag.data));
							}
							continue;
						}
						default:
							break;
					}
					switch(tag.datatype)
					{
						case(FMOD_TAGDATATYPE_INT):
						{
							(mMetadata)[name]=*(LLSD::Integer*)(tag.data);
							LL_DEBUGS("StreamMetadata") << tag.name << ": " << *(int*)(tag.data) << LL_ENDL;
							break;
						}
						case(FMOD_TAGDATATYPE_FLOAT):
						{
							(mMetadata)[name]=*(LLSD::Real*)(tag.data);
							LL_DEBUGS("StreamMetadata") << tag.name << ": " << *(float*)(tag.data) << LL_ENDL;
							break;
						}
						case(FMOD_TAGDATATYPE_STRING):
						{
							std::string out = rawstr_to_utf8(std::string((char*)tag.data,tag.datalen));
							(mMetadata)[name]=out;
							LL_DEBUGS("StreamMetadata") << tag.name << ": " << out << LL_ENDL;
							break;
						}
						case(FMOD_TAGDATATYPE_STRING_UTF16):
						{
							std::string out((char*)tag.data,tag.datalen);
							(mMetadata)[std::string(tag.name)]=out;
							LL_DEBUGS("StreamMetadata") << tag.name << ": " << out << LL_ENDL;
							break;
						}
						case(FMOD_TAGDATATYPE_STRING_UTF16BE):
						{
							std::string out((char*)tag.data,tag.datalen);
							//U16* buf = (U16*)out.c_str();
							//for(U32 j = 0; j < out.size()/2; ++j)
								//(((buf[j] & 0xff)<<8) | ((buf[j] & 0xff00)>>8));
							(mMetadata)[std::string(tag.name)]=out;
							LL_DEBUGS("StreamMetadata") << tag.name << ": " << out << LL_ENDL;
							break;
						}
						default:
							break;
					}
					// </FS:CR> Stream metadata - originally by Shyotl Khur
				}
			}

			if(starving)
			{
				bool paused = false;
				// <FS> FMOD fixes
				//mFMODInternetStreamChannelp->getPaused(&paused);
				//if(!paused)
				if (mFMODInternetStreamChannelp->getPaused(&paused) == FMOD_OK && !paused)
				// </FS>
				{
					LL_INFOS() << "Stream starvation detected! Pausing stream until buffer nearly full." << LL_ENDL;
					LL_INFOS() << "  (diskbusy="<<diskbusy<<")" << LL_ENDL;
					LL_INFOS() << "  (progress="<<progress<<")" << LL_ENDL;
					// <FS> FMOD fixes
					//mFMODInternetStreamChannelp->setPaused(true);
					Check_FMOD_Error(mFMODInternetStreamChannelp->setPaused(true), "FMOD::Channel::setPaused");
					// </FS>
				}
			}
			else if(progress > 80)
			{
				// <FS> FMOD fixes
				//mFMODInternetStreamChannelp->setPaused(false);
				Check_FMOD_Error(mFMODInternetStreamChannelp->setPaused(false), "FMOD::Channel::setPaused");
				// </FS>
			}
		}
	}
}

void LLStreamingAudio_FMODEX::stop()
{
	// <FS> FMOD fixes
	mPendingURL.clear();

	if (mFMODInternetStreamChannelp)
	{
		// <FS> FMOD fixes
		//mFMODInternetStreamChannelp->setPaused(true);
		//mFMODInternetStreamChannelp->setPriority(0);
		Check_FMOD_Error(mFMODInternetStreamChannelp->setPaused(true), "FMOD::Channel::setPaused");
		Check_FMOD_Error(mFMODInternetStreamChannelp->setPriority(0), "FMOD::Channel::setPriority");
		// </FS>
		mFMODInternetStreamChannelp = NULL;
	}

	if (mCurrentInternetStreamp)
	{
		LL_INFOS() << "Stopping internet stream: " << mCurrentInternetStreamp->getURL() << LL_ENDL;
		if (mCurrentInternetStreamp->stopStream())
		{
			delete mCurrentInternetStreamp;
		}
		else
		{
			LL_WARNS() << "Pushing stream to dead list: " << mCurrentInternetStreamp->getURL() << LL_ENDL;
			mDeadStreams.push_back(mCurrentInternetStreamp);
		}
		mCurrentInternetStreamp = NULL;
		//mURL.clear();
	}
}

void LLStreamingAudio_FMODEX::pause(int pauseopt)
{
	if (pauseopt < 0)
	{
		pauseopt = mCurrentInternetStreamp ? 1 : 0;
	}

	if (pauseopt)
	{
		if (mCurrentInternetStreamp)
		{
			stop();
		}
	}
	else
	{
		start(getURL());
	}
}


// A stream is "playing" if it has been requested to start.  That
// doesn't necessarily mean audio is coming out of the speakers.
int LLStreamingAudio_FMODEX::isPlaying()
{
	if (mCurrentInternetStreamp)
	{
		return 1; // Active and playing
	}
	// <FS> FMOD fixes
	//else if (!mURL.empty())
	else if (!mURL.empty() || !mPendingURL.empty())
	// </FS>
	{
		return 2; // "Paused"
	}
	else
	{
		return 0;
	}
}


F32 LLStreamingAudio_FMODEX::getGain()
{
	return mGain;
}


std::string LLStreamingAudio_FMODEX::getURL()
{
	return mURL;
}


void LLStreamingAudio_FMODEX::setGain(F32 vol)
{
	mGain = vol;

	if (mFMODInternetStreamChannelp)
	{
		vol = llclamp(vol * vol, 0.f, 1.f);	//should vol be squared here?

		// <FS> FMOD fixes
		//mFMODInternetStreamChannelp->setVolume(vol);
		Check_FMOD_Error(mFMODInternetStreamChannelp->setVolume(vol), "FMOD::Channel::setVolume");
		// </FS>
	}
}

// <FS:CR> Streamtitle display
// virtual
bool LLStreamingAudio_FMODEX::getNewMetadata(LLSD& metadata)
{
	if (mCurrentInternetStreamp)
	{
		if (mNewMetadata)
		{
			metadata = mMetadata;
			mNewMetadata = false;
			return true;
		}
			
		return mNewMetadata;
	}

	metadata = LLSD();
	return false;
}
// </FS:CR>

///////////////////////////////////////////////////////
// manager of possibly-multiple internet audio streams

LLAudioStreamManagerFMODEX::LLAudioStreamManagerFMODEX(FMOD::System *system, const std::string& url) :
	mSystem(system),
	mStreamChannel(NULL),
	mInternetStream(NULL),
	mReady(false)
{
	mInternetStreamURL = url;

	FMOD_RESULT result = mSystem->createStream(url.c_str(), FMOD_2D | FMOD_NONBLOCKING | FMOD_IGNORETAGS, 0, &mInternetStream);

	if (result != FMOD_OK)
	{
		LL_WARNS() << "Couldn't open fmod stream, error "
			<< FMOD_ErrorString(result)
			<< LL_ENDL;
		mReady = false;
		return;
	}

	mReady = true;
}

FMOD::Channel *LLAudioStreamManagerFMODEX::startStream()
{
	// We need a live and opened stream before we try and play it.
	// <FS> FMOD fixes
	//if (!mInternetStream || getOpenState() != FMOD_OPENSTATE_READY)
	FMOD_OPENSTATE open_state;
	if (getOpenState(open_state) != FMOD_OK || open_state != FMOD_OPENSTATE_READY)
	// </FS>
	{
		LL_WARNS() << "No internet stream to start playing!" << LL_ENDL;
		return NULL;
	}

	if(mStreamChannel)
		return mStreamChannel;	//Already have a channel for this stream.

	// <FS> FMOD fixes
	//mSystem->playSound(FMOD_CHANNEL_FREE, mInternetStream, true, &mStreamChannel);
	Check_FMOD_Error(mSystem->playSound(FMOD_CHANNEL_FREE, mInternetStream, true, &mStreamChannel), "FMOD::System::playSound");
	// </FS>
	return mStreamChannel;
}

bool LLAudioStreamManagerFMODEX::stopStream()
{
	if (mInternetStream)
	{
		bool close = true;
		// <FS> FMOD fixes
		//switch (getOpenState())
		FMOD_OPENSTATE open_state;
		if (getOpenState(open_state) == FMOD_OK)
		{
			switch (open_state)
		// </FS>
		{
		case FMOD_OPENSTATE_CONNECTING:
			close = false;
			break;
		default:
			close = true;
		}
		// <FS> FMOD fixes
		}
		// </FS>

		// <FS> FMOD fixes
		//if (close)
		//{
		//	mInternetStream->release();
		if (close && mInternetStream->release() == FMOD_OK)
		{
		// </FS>
			mStreamChannel = NULL;
			mInternetStream = NULL;
			return true;
		}
		else
		{
			return false;
		}
	}
	else
	{
		return true;
	}
}

// <FS> FMOD fixes
//FMOD_OPENSTATE LLAudioStreamManagerFMODEX::getOpenState(unsigned int* percentbuffered, bool* starving, bool* diskbusy)
//{
//	FMOD_OPENSTATE state;
//	mInternetStream->getOpenState(&state, percentbuffered, starving, diskbusy);
//	return state;
//}
FMOD_RESULT LLAudioStreamManagerFMODEX::getOpenState(FMOD_OPENSTATE& state, unsigned int* percentbuffered, bool* starving, bool* diskbusy)
{
	if (!mInternetStream)
		return FMOD_ERR_INVALID_HANDLE;
	FMOD_RESULT result = mInternetStream->getOpenState(&state, percentbuffered, starving, diskbusy);
	Check_FMOD_Error(result, "FMOD::Sound::getOpenState");
	return result;
}
// </FS>

void LLStreamingAudio_FMODEX::setBufferSizes(U32 streambuffertime, U32 decodebuffertime)
{
	// <FS> FMOD fixes
	//mSystem->setStreamBufferSize(streambuffertime/1000*128*128, FMOD_TIMEUNIT_RAWBYTES);
	Check_FMOD_Error(mSystem->setStreamBufferSize(streambuffertime / 1000 * 128 * 128, FMOD_TIMEUNIT_RAWBYTES), "FMOD::System::setStreamBufferSize");
	// </FS>
	FMOD_ADVANCEDSETTINGS settings;
	memset(&settings,0,sizeof(settings));
	settings.cbsize=sizeof(settings);
	settings.defaultDecodeBufferSize = decodebuffertime;//ms
	// <FS> FMOD fixes
	//mSystem->setAdvancedSettings(&settings);
	Check_FMOD_Error(mSystem->setAdvancedSettings(&settings), "FMOD::System::setAdvancedSettings");
	// </FS>
}

// <FS> FMOD fixes
bool LLStreamingAudio_FMODEX::releaseDeadStreams()
{
	// Kill dead internet streams, if possible
	std::list<LLAudioStreamManagerFMODEX *>::iterator iter;
	for (iter = mDeadStreams.begin(); iter != mDeadStreams.end();)
	{
		LLAudioStreamManagerFMODEX *streamp = *iter;
		if (streamp->stopStream())
		{
			LL_INFOS() << "Closed dead stream" << LL_ENDL;
			delete streamp;
			mDeadStreams.erase(iter++);
		}
		else
		{
			iter++;
		}
	}

	return mDeadStreams.empty();
}
// </FS>
