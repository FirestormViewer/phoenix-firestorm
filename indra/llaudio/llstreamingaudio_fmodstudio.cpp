/** 
 * @file streamingaudio_fmodstudio.cpp
 * @brief LLStreamingAudio_FMODSTUDIO implementation
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

#include "llstreamingaudio_fmodstudio.h"

inline bool Check_FMOD_Error(FMOD_RESULT result, const char *string)
{
	if (result == FMOD_OK)
		return false;
	LL_WARNS("AudioImpl") << string << " Error: " << FMOD_ErrorString(result) << LL_ENDL;
	return true;
}

class LLAudioStreamManagerFMODSTUDIO
{
public:
	LLAudioStreamManagerFMODSTUDIO(FMOD::System *system, FMOD::ChannelGroup *group, const std::string& url);
	FMOD::Channel* startStream();
	bool stopStream(); // Returns true if the stream was successfully stopped.
	bool ready();

	const std::string& getURL() 	{ return mInternetStreamURL; }

	FMOD_RESULT getOpenState(FMOD_OPENSTATE& openstate, unsigned int* percentbuffered = NULL, bool* starving = NULL, bool* diskbusy = NULL);
protected:
	FMOD::System* mSystem;
	FMOD::ChannelGroup* mChannelGroup;
	FMOD::Channel* mStreamChannel;
	FMOD::Sound* mInternetStream;
	bool mReady;

	std::string mInternetStreamURL;
};



//---------------------------------------------------------------------------
// Internet Streaming
//---------------------------------------------------------------------------
LLStreamingAudio_FMODSTUDIO::LLStreamingAudio_FMODSTUDIO(FMOD::System *system) :
	mSystem(system),
	mCurrentInternetStreamp(NULL),
	mStreamGroup(NULL),
	mFMODInternetStreamChannelp(NULL),
	mGain(1.0f)
{
	FMOD_RESULT result;

	// Number of milliseconds of audio to buffer for the audio card.
	// Must be larger than the usual Second Life frame stutter time.
	const U32 buffer_seconds = 10;		//sec
	const U32 estimated_bitrate = 128;	//kbit/sec
	result = mSystem->setStreamBufferSize(estimated_bitrate * buffer_seconds * 128/*bytes/kbit*/, FMOD_TIMEUNIT_RAWBYTES);
	Check_FMOD_Error(result, "FMOD::System::setStreamBufferSize");

	Check_FMOD_Error(system->createChannelGroup("stream", &mStreamGroup), "FMOD::System::createChannelGroup");
}

LLStreamingAudio_FMODSTUDIO::~LLStreamingAudio_FMODSTUDIO()
{
	stop();
	for (U32 i = 0; i < 100; ++i)
	{
		if (releaseDeadStreams())
			break;
		ms_sleep(10);
	}

}

void LLStreamingAudio_FMODSTUDIO::start(const std::string& url)
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
		if(mDeadStreams.empty())
		{
			LL_INFOS() << "Starting internet stream: " << url << LL_ENDL;
			mCurrentInternetStreamp = new LLAudioStreamManagerFMODSTUDIO(mSystem, mStreamGroup, url);
			mURL = url;
		}
		else
		{
			LL_INFOS() << "Deferring stream load until buffer release: " << url << LL_ENDL;
			mPendingURL = url;
		}
	}
	else
	{
		LL_INFOS() << "Set internet stream to null" << LL_ENDL;
		mURL.clear();
	}
}


void LLStreamingAudio_FMODSTUDIO::update()
{
	if (!releaseDeadStreams())
	{
		llassert_always(mCurrentInternetStreamp == NULL);
		return;
	}

	if(!mPendingURL.empty())
	{
		llassert_always(mCurrentInternetStreamp == NULL);
		LL_INFOS() << "Starting internet stream: " << mPendingURL << LL_ENDL;
		mCurrentInternetStreamp = new LLAudioStreamManagerFMODSTUDIO(mSystem, mStreamGroup, mPendingURL);
		mURL = mPendingURL;
		mPendingURL.clear();
	}

	// Don't do anything if there are no streams playing
	if (!mCurrentInternetStreamp)
	{
		return;
	}

	unsigned int progress;
	bool starving;
	bool diskbusy;
	FMOD_OPENSTATE open_state;
	FMOD_RESULT result = mCurrentInternetStreamp->getOpenState(open_state, &progress, &starving, &diskbusy);

	if (result != FMOD_OK || open_state == FMOD_OPENSTATE_ERROR)
	{
		stop();
		return;
	}
	else if (open_state == FMOD_OPENSTATE_READY)
	{
		// Stream is live

		// start the stream if it's ready
		if (!mFMODInternetStreamChannelp &&
			(mFMODInternetStreamChannelp = mCurrentInternetStreamp->startStream()))
		{
			// Reset volume to previously set volume
			setGain(getGain());
			Check_FMOD_Error(mFMODInternetStreamChannelp->setPaused(false), "FMOD::Channel::setPaused");
		}
	}


	if(mFMODInternetStreamChannelp)
	{
		FMOD::Sound *sound = NULL;

		// <DKO> FmodEX Error checking
		//if(mFMODInternetStreamChannelp->getCurrentSound(&sound) == FMOD_OK && sound)
		if (!Check_FMOD_Error(mFMODInternetStreamChannelp->getCurrentSound(&sound), "FMOD::Channel::getCurrentSound") && sound)
		// </FS:CR>
		{
			FMOD_TAG tag;
			S32 tagcount, dirtytagcount;

			// <DKO> FmodEX Error checking
			//if(sound->getNumTags(&tagcount, &dirtytagcount) == FMOD_OK && dirtytagcount)
			if (!Check_FMOD_Error(sound->getNumTags(&tagcount, &dirtytagcount), "FMOD::Sound::getNumTags") && dirtytagcount)
			// </FS:CR>
			{
				// <DKO> Stream metadata - originally by Shyotl Khur
				mMetadata.clear();
				mNewMetadata = true;
				// </DKO>
				for(S32 i = 0; i < tagcount; ++i)
				{
					if(sound->getTag(NULL, i, &tag) != FMOD_OK)
						continue;

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
				}
			}

			if(starving)
			{
				bool paused = false;
				if (mFMODInternetStreamChannelp->getPaused(&paused) == FMOD_OK && !paused)
				{
					LL_INFOS() << "Stream starvation detected! Pausing stream until buffer nearly full." << LL_ENDL;
					LL_INFOS() << "  (diskbusy="<<diskbusy<<")" << LL_ENDL;
					LL_INFOS() << "  (progress="<<progress<<")" << LL_ENDL;
					Check_FMOD_Error(mFMODInternetStreamChannelp->setPaused(true), "FMOD::Channel::setPaused");
				}
			}
			else if(progress > 80)
			{
				Check_FMOD_Error(mFMODInternetStreamChannelp->setPaused(false), "FMOD::Channel::setPaused");
			}
		}
	}
}

void LLStreamingAudio_FMODSTUDIO::stop()
{
	mPendingURL.clear();

	if (mFMODInternetStreamChannelp)
	{
		Check_FMOD_Error(mFMODInternetStreamChannelp->setPaused(true), "FMOD::Channel::setPaused");
		Check_FMOD_Error(mFMODInternetStreamChannelp->setPriority(0), "FMOD::Channel::setPriority");
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
	}
}

void LLStreamingAudio_FMODSTUDIO::pause(int pauseopt)
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
int LLStreamingAudio_FMODSTUDIO::isPlaying()
{
	if (mCurrentInternetStreamp)
	{
		return 1; // Active and playing
	}
	else if (!mURL.empty() || !mPendingURL.empty())
	{
		return 2; // "Paused"
	}
	else
	{
		return 0;
	}
}


F32 LLStreamingAudio_FMODSTUDIO::getGain()
{
	return mGain;
}


std::string LLStreamingAudio_FMODSTUDIO::getURL()
{
	return mURL;
}


void LLStreamingAudio_FMODSTUDIO::setGain(F32 vol)
{
	mGain = vol;

	if (mFMODInternetStreamChannelp)
	{
		vol = llclamp(vol * vol, 0.f, 1.f);	//should vol be squared here?

		Check_FMOD_Error(mFMODInternetStreamChannelp->setVolume(vol), "FMOD::Channel::setVolume");
	}
}
// <DKO> Streamtitle display
// virtual
bool LLStreamingAudio_FMODSTUDIO::getNewMetadata(LLSD& metadata)
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
// </DKO>
///////////////////////////////////////////////////////
// manager of possibly-multiple internet audio streams

LLAudioStreamManagerFMODSTUDIO::LLAudioStreamManagerFMODSTUDIO(FMOD::System *system, FMOD::ChannelGroup *group, const std::string& url) :
	mSystem(system),
	mChannelGroup(group),
	mStreamChannel(NULL),
	mInternetStream(NULL),
	mReady(false)
{
	mInternetStreamURL = url;

	FMOD_RESULT result = mSystem->createStream(url.c_str(), FMOD_2D | FMOD_NONBLOCKING | FMOD_IGNORETAGS, 0, &mInternetStream);

	if (result!= FMOD_OK)
	{
		LL_WARNS() << "Couldn't open fmod stream, error "
			<< FMOD_ErrorString(result)
			<< LL_ENDL;
		mReady = false;
		return;
	}

	mReady = true;
}

FMOD::Channel *LLAudioStreamManagerFMODSTUDIO::startStream()
{
	// We need a live and opened stream before we try and play it.
	FMOD_OPENSTATE open_state;
	if (getOpenState(open_state) != FMOD_OK || open_state != FMOD_OPENSTATE_READY)
	{
		LL_WARNS() << "No internet stream to start playing!" << LL_ENDL;
		return NULL;
	}

	if(mStreamChannel)
		return mStreamChannel;	//Already have a channel for this stream.

	Check_FMOD_Error(mSystem->playSound(mInternetStream, mChannelGroup, true, &mStreamChannel), "FMOD::System::playSound");
	return mStreamChannel;
}

bool LLAudioStreamManagerFMODSTUDIO::stopStream()
{
	if (mInternetStream)
	{
		bool close = true;
		FMOD_OPENSTATE open_state;
		if (getOpenState(open_state) == FMOD_OK)
		{
			switch (open_state)
			{
			case FMOD_OPENSTATE_CONNECTING:
				close = false;
				break;
			default:
				close = true;
			}
		}

		if (close && mInternetStream->release() == FMOD_OK)
		{
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

FMOD_RESULT LLAudioStreamManagerFMODSTUDIO::getOpenState(FMOD_OPENSTATE& state, unsigned int* percentbuffered, bool* starving, bool* diskbusy)
{
	if (!mInternetStream)
		return FMOD_ERR_INVALID_HANDLE;
	FMOD_RESULT result = mInternetStream->getOpenState(&state, percentbuffered, starving, diskbusy);
	Check_FMOD_Error(result, "FMOD::Sound::getOpenState");
	return result;
}

void LLStreamingAudio_FMODSTUDIO::setBufferSizes(U32 streambuffertime, U32 decodebuffertime)
{
	Check_FMOD_Error(mSystem->setStreamBufferSize(streambuffertime / 1000 * 128 * 128, FMOD_TIMEUNIT_RAWBYTES), "FMOD::System::setStreamBufferSize");
	FMOD_ADVANCEDSETTINGS settings;
	memset(&settings,0,sizeof(settings));
	settings.cbSize=sizeof(settings);
	settings.defaultDecodeBufferSize = decodebuffertime;//ms
	Check_FMOD_Error(mSystem->setAdvancedSettings(&settings), "FMOD::System::setAdvancedSettings");
}

bool LLStreamingAudio_FMODSTUDIO::releaseDeadStreams()
{
	// Kill dead internet streams, if possible
	std::list<LLAudioStreamManagerFMODSTUDIO *>::iterator iter;
	for (iter = mDeadStreams.begin(); iter != mDeadStreams.end();)
	{
		LLAudioStreamManagerFMODSTUDIO *streamp = *iter;
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
