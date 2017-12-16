/** 
 * @file audioengine_fmodstudio.cpp
 * @brief Implementation of LLAudioEngine class abstracting the audio 
 * support as a FMOD Studio implementation
 *
 * $LicenseInfo:firstyear=2014&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2014, Linden Research, Inc.
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

#include "llstreamingaudio.h"
#include "llstreamingaudio_fmodstudio.h"

#include "llaudioengine_fmodstudio.h"
#include "lllistener_fmodstudio.h"

#include "llerror.h"
#include "llmath.h"
#include "llrand.h"

#include "fmod.hpp"
#include "fmod_errors.h"
#include "lldir.h"

#include "sound_ids.h"

#include "indra_constants.h"

const U32 EXTRA_SOUND_CHANNELS = 10;

FMOD_RESULT F_CALLBACK windDSPCallback(FMOD_DSP_STATE *dsp_state, float *inbuffer, float *outbuffer, unsigned int length, int inchannels, int *outchannels);

FMOD::ChannelGroup *LLAudioEngine_FMODSTUDIO::mChannelGroups[LLAudioEngine::AUDIO_TYPE_COUNT] = {0};

LLAudioEngine_FMODSTUDIO::LLAudioEngine_FMODSTUDIO(bool enable_profiler, U32 resample_method)
	: mInited(false)
	, mWindGen(NULL)
	, mWindDSPDesc(NULL)
	, mWindDSP(NULL)
	, mSystem(NULL)
	, mEnableProfiler(enable_profiler)
	, mResampleMethod(resample_method)
{
}

LLAudioEngine_FMODSTUDIO::~LLAudioEngine_FMODSTUDIO()
{
}

static inline bool Check_FMOD_Error(FMOD_RESULT result, const char *string)
{
	if(result == FMOD_OK)
		return false;

	if (result != FMOD_ERR_INVALID_HANDLE)
	{
		LL_WARNS() << string << " Error: " << FMOD_ErrorString(result) << LL_ENDL;
	}
	else
	{
		LL_DEBUGS() << string << " Error: " << FMOD_ErrorString(result) << LL_ENDL;
	}

	return true;
}

bool LLAudioEngine_FMODSTUDIO::init(const S32 num_channels, void* userdata)
{
	U32 version;
	FMOD_RESULT result;

	LL_DEBUGS("AppInit") << "LLAudioEngine_FMODSTUDIO::init() initializing FMOD" << LL_ENDL;

	result = FMOD::System_Create(&mSystem);
	if(Check_FMOD_Error(result, "FMOD::System_Create"))
		return false;

	//will call LLAudioEngine_FMODSTUDIO::allocateListener, which needs a valid mSystem pointer.
	LLAudioEngine::init(num_channels, userdata);	
	
	result = mSystem->getVersion(&version);
	Check_FMOD_Error(result, "FMOD::System::getVersion");

	if (version < FMOD_VERSION)
	{
		LL_WARNS("AppInit") << "Error : You are using the wrong FMOD Studio version (" << version
			<< ")!  You should be using FMOD Studio" << FMOD_VERSION << LL_ENDL;
	}

	// In this case, all sounds, PLUS wind and stream will be software.
	result = mSystem->setSoftwareChannels(num_channels + EXTRA_SOUND_CHANNELS);
	Check_FMOD_Error(result,"FMOD::System::setSoftwareChannels");

	FMOD_ADVANCEDSETTINGS adv_settings = { };
	adv_settings.cbSize = sizeof(FMOD_ADVANCEDSETTINGS);
	switch (mResampleMethod)
	{
	default:
	case RESAMPLE_LINEAR:
		adv_settings.resamplerMethod = FMOD_DSP_RESAMPLER_LINEAR;
		break;
	case RESAMPLE_CUBIC:
		adv_settings.resamplerMethod = FMOD_DSP_RESAMPLER_CUBIC;
		break;
	case RESAMPLE_SPLINE:
		adv_settings.resamplerMethod = FMOD_DSP_RESAMPLER_SPLINE;
		break;
	}

	// In this case, all sounds, PLUS wind and stream will be software.
	result = mSystem->setSoftwareChannels(num_channels + 2);
	Check_FMOD_Error(result,"FMOD::System::setSoftwareChannels");

	U32 fmod_flags = FMOD_INIT_NORMAL | FMOD_INIT_3D_RIGHTHANDED | FMOD_INIT_THREAD_UNSAFE;
	if(mEnableProfiler)
	{
		fmod_flags |= FMOD_INIT_PROFILE_ENABLE;
	}

#if LL_LINUX
	bool audio_ok = false;

	if (!audio_ok)
	{
		if (NULL == getenv("LL_BAD_FMOD_PULSEAUDIO")) /*Flawfinder: ignore*/
		{
			LL_DEBUGS("AppInit") << "Trying PulseAudio audio output..." << LL_ENDL;
			if((result = mSystem->setOutput(FMOD_OUTPUTTYPE_PULSEAUDIO)) == FMOD_OK &&
				(result = mSystem->init(num_channels + EXTRA_SOUND_CHANNELS, fmod_flags, const_cast<char*>(APP_NAME.c_str()))) == FMOD_OK)
			{
				LL_DEBUGS("AppInit") << "PulseAudio output initialized OKAY"	<< LL_ENDL;
				audio_ok = true;
			}
			else 
			{
				Check_FMOD_Error(result, "PulseAudio audio output FAILED to initialize");
			}
		} 
		else 
		{
			LL_DEBUGS("AppInit") << "PulseAudio audio output SKIPPED" << LL_ENDL;
		}	
	}
	if (!audio_ok)
	{
		if (NULL == getenv("LL_BAD_FMOD_ALSA"))		/*Flawfinder: ignore*/
		{
			LL_DEBUGS("AppInit") << "Trying ALSA audio output..." << LL_ENDL;
			if((result = mSystem->setOutput(FMOD_OUTPUTTYPE_ALSA)) == FMOD_OK &&
			    (result = mSystem->init(num_channels + EXTRA_SOUND_CHANNELS, fmod_flags, 0)) == FMOD_OK)
			{
				LL_DEBUGS("AppInit") << "ALSA audio output initialized OKAY" << LL_ENDL;
				audio_ok = true;
			} 
			else 
			{
				Check_FMOD_Error(result, "ALSA audio output FAILED to initialize");
			}
		} 
		else 
		{
			LL_DEBUGS("AppInit") << "ALSA audio output SKIPPED" << LL_ENDL;
		}
	}
	if (!audio_ok)
	{
		LL_WARNS("AppInit") << "Overall audio init failure." << LL_ENDL;
		return false;
	}

	// We're interested in logging which output method we
	// ended up with, for QA purposes.
	FMOD_OUTPUTTYPE output_type;
	if(!Check_FMOD_Error(mSystem->getOutput(&output_type), "FMOD::System::getOutput"))
	{
		switch (output_type)
		{
			case FMOD_OUTPUTTYPE_NOSOUND: 
				LL_INFOS("AppInit") << "Audio output: NoSound" << LL_ENDL; break;
			case FMOD_OUTPUTTYPE_PULSEAUDIO:	
				LL_INFOS("AppInit") << "Audio output: PulseAudio" << LL_ENDL; break;
			case FMOD_OUTPUTTYPE_ALSA: 
				LL_INFOS("AppInit") << "Audio output: ALSA" << LL_ENDL; break;
			default:
				LL_INFOS("AppInit") << "Audio output: Unknown!" << LL_ENDL; break;
		};
	}
#else // LL_LINUX

	// initialize the FMOD engine
	result = mSystem->init( num_channels + EXTRA_SOUND_CHANNELS, fmod_flags, 0);
	if (result == FMOD_ERR_OUTPUT_CREATEBUFFER)
	{
		/*
		Ok, the speaker mode selected isn't supported by this soundcard. Switch it
		back to stereo...
		*/
		result = mSystem->setSoftwareFormat(44100, FMOD_SPEAKERMODE_STEREO, 0);
		Check_FMOD_Error(result,"Error falling back to stereo mode");
		/*
		... and re-init.
		*/
		result = mSystem->init( num_channels + EXTRA_SOUND_CHANNELS, fmod_flags, 0);
	}
	if(Check_FMOD_Error(result, "Error initializing FMOD Studio"))
		return false;
#endif

	if (mEnableProfiler)
	{
		Check_FMOD_Error(mSystem->createChannelGroup("None", &mChannelGroups[AUDIO_TYPE_NONE]), "FMOD::System::createChannelGroup");
		Check_FMOD_Error(mSystem->createChannelGroup("SFX", &mChannelGroups[AUDIO_TYPE_SFX]), "FMOD::System::createChannelGroup");
		Check_FMOD_Error(mSystem->createChannelGroup("UI", &mChannelGroups[AUDIO_TYPE_UI]), "FMOD::System::createChannelGroup");
		Check_FMOD_Error(mSystem->createChannelGroup("Ambient", &mChannelGroups[AUDIO_TYPE_AMBIENT]), "FMOD::System::createChannelGroup");
	}

	// set up our favourite FMOD-native streaming audio implementation if none has already been added
	if (!getStreamingAudioImpl()) // no existing implementation added
		setStreamingAudioImpl(new LLStreamingAudio_FMODSTUDIO(mSystem));

	LL_INFOS("AppInit") << "LLAudioEngine_FMODSTUDIO::init() FMOD Studio initialized correctly" << LL_ENDL;

	int r_numbuffers, r_samplerate, r_channels;
	unsigned int r_bufferlength;
	mSystem->getDSPBufferSize(&r_bufferlength, &r_numbuffers);
	LL_INFOS("AppInit") << "LLAudioEngine_FMODSTUDIO::init(): r_bufferlength=" << r_bufferlength << " bytes" << LL_ENDL;
	LL_INFOS("AppInit") << "LLAudioEngine_FMODSTUDIO::init(): r_numbuffers=" << r_numbuffers << LL_ENDL;

	char r_name[512];
	mSystem->getDriverInfo(0, r_name, 511, NULL, &r_samplerate, NULL, &r_channels);
	r_name[511] = '\0';
	LL_INFOS("AppInit") << "LLAudioEngine_FMODSTUDIO::init(): r_name=\"" << r_name << "\"" <<  LL_ENDL;
	LL_INFOS("AppInit") << "LLAudioEngine_FMODSTUDIO::init(): r_samplerate=" << r_samplerate << "Hz" << LL_ENDL;
	LL_INFOS("AppInit") << "LLAudioEngine_FMODSTUDIO::init(): r_channels=" << r_channels << LL_ENDL;

	int latency = 100; // optimistic default - i suspect if sample rate is 0, everything breaks. 
	if ( r_samplerate != 0 )
		latency = (int)(1000.0f * r_bufferlength * r_numbuffers / r_samplerate);
	LL_INFOS("AppInit") << "LLAudioEngine_FMODSTUDIO::init(): latency=" << latency << "ms" << LL_ENDL;

	mInited = true;

	LL_INFOS("AppInit") << "LLAudioEngine_FMODSTUDIO::init(): initialization complete." << LL_ENDL;

	return true;
}


std::string LLAudioEngine_FMODSTUDIO::getDriverName(bool verbose)
{
	llassert_always(mSystem);
	if (verbose)
	{
		U32 version;
		if(!Check_FMOD_Error(mSystem->getVersion(&version), "FMOD::System::getVersion"))
		{
			return llformat("FMOD Studio %1x.%02x.%02x", version >> 16, version >> 8 & 0x000000FF, version & 0x000000FF);
		}
	}
	return "FMOD Studio";
}


void LLAudioEngine_FMODSTUDIO::allocateListener(void)
{	
	try
	{
		mListenerp = (LLListener *) new LLListener_FMODSTUDIO(mSystem);
	}
	catch (const std::bad_alloc& e)
	{
		LL_WARNS() << "Listener allocation failed due to: " << e.what() << LL_ENDL;
	}
}


void LLAudioEngine_FMODSTUDIO::shutdown()
{
	LL_INFOS() << "About to LLAudioEngine::shutdown()" << LL_ENDL;
	LLAudioEngine::shutdown();
	
	LL_INFOS() << "LLAudioEngine_FMODSTUDIO::shutdown() closing FMOD Studio" << LL_ENDL;
	if ( mSystem ) // speculative fix for MAINT-2657
	{
		Check_FMOD_Error(mSystem->close(), "FMOD::System::close");
		Check_FMOD_Error(mSystem->release(), "FMOD::System::release");
	}
	LL_INFOS() << "LLAudioEngine_FMODSTUDIO::shutdown() done closing FMOD Studio" << LL_ENDL;

	delete mListenerp;
	mListenerp = NULL;
}


LLAudioBuffer * LLAudioEngine_FMODSTUDIO::createBuffer()
{
	return new LLAudioBufferFMODSTUDIO(mSystem);
}


LLAudioChannel * LLAudioEngine_FMODSTUDIO::createChannel()
{
	return new LLAudioChannelFMODSTUDIO(mSystem);
}

bool LLAudioEngine_FMODSTUDIO::initWind()
{
	mNextWindUpdate = 0.0;

	cleanupWind();

	mWindDSPDesc = new FMOD_DSP_DESCRIPTION();
	memset(mWindDSPDesc, 0, sizeof(*mWindDSPDesc));	//Set everything to zero
	mWindDSPDesc->pluginsdkversion = FMOD_PLUGIN_SDK_VERSION;
	strncpy(mWindDSPDesc->name, "Wind Unit", sizeof(mWindDSPDesc->name));	//Set name to "Wind Unit"
	mWindDSPDesc->numoutputbuffers = 1;
	mWindDSPDesc->read = &windDSPCallback; //Assign callback.
	if (Check_FMOD_Error(mSystem->createDSP(mWindDSPDesc, &mWindDSP), "FMOD::createDSP") || !mWindDSP)
		return false;

	int frequency = 44100;
	FMOD_SPEAKERMODE mode;
	if (!Check_FMOD_Error(mSystem->getSoftwareFormat(&frequency, &mode, nullptr), "FMOD::System::getSoftwareFormat"))
	{
		mWindGen = new LLWindGen<MIXBUFFERFORMAT>((U32)frequency);

		if (!Check_FMOD_Error(mWindDSP->setUserData((void*)mWindGen), "FMOD::DSP::setUserData") &&
			!Check_FMOD_Error(mWindDSP->setChannelFormat(FMOD_CHANNELMASK_STEREO, 2, mode), "FMOD::DSP::setChannelFormat") &&
			!Check_FMOD_Error(mSystem->playDSP(mWindDSP, nullptr, false, nullptr), "FMOD::System::playDSP"))
			return true;	//Success
	}

	cleanupWind();
	return false;
}


void LLAudioEngine_FMODSTUDIO::cleanupWind()
{
	if (mWindDSP)
	{
		FMOD::ChannelGroup* mastergroup = NULL;
		if (!Check_FMOD_Error(mSystem->getMasterChannelGroup(&mastergroup), "FMOD::System::getMasterChannelGroup") && mastergroup)
			Check_FMOD_Error(mastergroup->removeDSP(mWindDSP), "FMOD::ChannelGroup::removeDSP");
		Check_FMOD_Error(mWindDSP->release(), "FMOD::DSP::release");
		mWindDSP = NULL;
	}

	delete mWindDSPDesc;
	mWindDSPDesc = NULL;

	delete mWindGen;
	mWindGen = NULL;
}


//-----------------------------------------------------------------------
void LLAudioEngine_FMODSTUDIO::updateWind(LLVector3 wind_vec, F32 camera_height_above_water)
{
	LLVector3 wind_pos;
	F64 pitch;
	F64 center_freq;

	if (!mEnableWind)
	{
		return;
	}
	
	if (mWindUpdateTimer.checkExpirationAndReset(LL_WIND_UPDATE_INTERVAL))
	{
		
		// wind comes in as Linden coordinate (+X = forward, +Y = left, +Z = up)
		// need to convert this to the conventional orientation DS3D and OpenAL use
		// where +X = right, +Y = up, +Z = backwards

		wind_vec.setVec(-wind_vec.mV[1], wind_vec.mV[2], -wind_vec.mV[0]);

		// cerr << "Wind update" << endl;

		pitch = 1.0 + mapWindVecToPitch(wind_vec);
		center_freq = 80.0 * pow(pitch,2.5*(mapWindVecToGain(wind_vec)+1.0));
		
		mWindGen->mTargetFreq = (F32)center_freq;
		mWindGen->mTargetGain = (F32)mapWindVecToGain(wind_vec) * mMaxWindGain;
		mWindGen->mTargetPanGainR = (F32)mapWindVecToPan(wind_vec);
  	}
}

//-----------------------------------------------------------------------
void LLAudioEngine_FMODSTUDIO::setInternalGain(F32 gain)
{
	if (!mInited)
	{
		return;
	}

	gain = llclamp( gain, 0.0f, 1.0f );

	FMOD::ChannelGroup *master_group;
	if(Check_FMOD_Error(mSystem->getMasterChannelGroup(&master_group), "FMOD::System::getMasterChannelGroup"))
		return;

	master_group->setVolume(gain);

	LLStreamingAudioInterface *saimpl = getStreamingAudioImpl();
	if ( saimpl )
	{
		// fmod likes its streaming audio channel gain re-asserted after
		// master volume change.
		saimpl->setGain(saimpl->getGain());
	}
}

//
// LLAudioChannelFMODSTUDIO implementation
//

LLAudioChannelFMODSTUDIO::LLAudioChannelFMODSTUDIO(FMOD::System *system) : LLAudioChannel(), mSystemp(system), mChannelp(NULL), mLastSamplePos(0)
{
}


LLAudioChannelFMODSTUDIO::~LLAudioChannelFMODSTUDIO()
{
	cleanup();
}

bool LLAudioChannelFMODSTUDIO::updateBuffer()
{
	if (mCurrentSourcep)
	{
		if (LLAudioChannel::updateBuffer())
		{
			// Base class update returned true, which means that we need to actually
			// set up the channel for a different buffer.

			LLAudioBufferFMODSTUDIO *bufferp = (LLAudioBufferFMODSTUDIO *) mCurrentSourcep->getCurrentBuffer();

			// Grab the FMOD sample associated with the buffer
			FMOD::Sound *soundp = bufferp->getSound();
			if (!soundp)
			{
				// This is bad, there should ALWAYS be a sound associated with a legit
				// buffer.
				LL_ERRS() << "No FMOD sound!" << LL_ENDL;
				return false;
			}


			// Actually play the sound.  Start it off paused so we can do all the necessary
			// setup.
			if (!mChannelp)
			{
				FMOD_RESULT result = getSystem()->playSound(soundp, nullptr, true, &mChannelp);
				Check_FMOD_Error(result, "FMOD::System::playSound");
			}

			//LL_INFOS() << "Setting up channel " << std::hex << mChannelID << std::dec << LL_ENDL;
		}

		//FMOD_RESULT result;

		mChannelp->setVolume(getSecondaryGain() * mCurrentSourcep->getGain());
		//Check_FMOD_Error(result, "FMOD::Channel::setVolume");

		mChannelp->setMode(mCurrentSourcep->isLoop() ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF);
		/*if(Check_FMOD_Error(result, "FMOD::Channel::setMode"))
		{
			S32 index;
			mChannelp->getIndex(&index);
			LL_WARNS() << "Channel " << index << "Source ID: " << mCurrentSourcep->getID()
					<< " at " << mCurrentSourcep->getPositionGlobal() << LL_ENDL;
		}*/
	}
	else
	{
		LL_DEBUGS() << "No source buffer!" << LL_ENDL;
		return false;
	}

	return true;
}


void LLAudioChannelFMODSTUDIO::update3DPosition()
{
	if (!mChannelp)
	{
		// We're not actually a live channel (i.e., we're not playing back anything)
		return;
	}

	LLAudioBufferFMODSTUDIO  *bufferp = (LLAudioBufferFMODSTUDIO  *)mCurrentBufferp;
	if (!bufferp)
	{
		// We don't have a buffer associated with us (should really have been picked up
		// by the above if.
		return;
	}

	if (mCurrentSourcep->isAmbient())
	{
		// Ambient sound, don't need to do any positional updates.
		set3DMode(false);
	}
	else
	{
		// Localized sound.  Update the position and velocity of the sound.
		set3DMode(true);

		LLVector3 float_pos;
		float_pos.setVec(mCurrentSourcep->getPositionGlobal());
		FMOD_RESULT result = mChannelp->set3DAttributes((FMOD_VECTOR*)float_pos.mV, (FMOD_VECTOR*)mCurrentSourcep->getVelocity().mV);
		Check_FMOD_Error(result, "FMOD::Channel::set3DAttributes");
	}
}


void LLAudioChannelFMODSTUDIO::updateLoop()
{
	if (!mChannelp)
	{
		// May want to clear up the loop/sample counters.
		return;
	}

	//
	// Hack:  We keep track of whether we looped or not by seeing when the
	// sample position looks like it's going backwards.  Not reliable; may
	// yield false negatives.
	//
	U32 cur_pos;
	Check_FMOD_Error(mChannelp->getPosition(&cur_pos,FMOD_TIMEUNIT_PCMBYTES),"FMOD::Channel::getPosition");

	if (cur_pos < (U32)mLastSamplePos)
	{
		mLoopedThisFrame = true;
	}
	mLastSamplePos = cur_pos;
}


void LLAudioChannelFMODSTUDIO::cleanup()
{
	if (!mChannelp)
	{
		//LL_INFOS() << "Aborting cleanup with no channel handle." << LL_ENDL;
		return;
	}

	//LL_INFOS() << "Cleaning up channel: " << mChannelID << LL_ENDL;
	Check_FMOD_Error(mChannelp->stop(),"FMOD::Channel::stop");

	mCurrentBufferp = NULL;
	mChannelp = NULL;
}


void LLAudioChannelFMODSTUDIO::play()
{
	if (!mChannelp)
	{
		LL_WARNS() << "Playing without a channel handle, aborting" << LL_ENDL;
		return;
	}

	Check_FMOD_Error(mChannelp->setPaused(false), "FMOD::Channel::setPaused");

	getSource()->setPlayedOnce(true);

	if(LLAudioEngine_FMODSTUDIO::mChannelGroups[getSource()->getType()])
		Check_FMOD_Error(mChannelp->setChannelGroup(LLAudioEngine_FMODSTUDIO::mChannelGroups[getSource()->getType()]),"FMOD::Channel::setChannelGroup");
}


void LLAudioChannelFMODSTUDIO::playSynced(LLAudioChannel *channelp)
{
	LLAudioChannelFMODSTUDIO *fmod_channelp = (LLAudioChannelFMODSTUDIO*)channelp;
	if (!(fmod_channelp->mChannelp && mChannelp))
	{
		// Don't have channels allocated to both the master and the slave
		return;
	}

	U32 cur_pos;
	if(Check_FMOD_Error(mChannelp->getPosition(&cur_pos,FMOD_TIMEUNIT_PCMBYTES), "Unable to retrieve current position"))
		return;

	cur_pos %= mCurrentBufferp->getLength();
	
	// Try to match the position of our sync master
	Check_FMOD_Error(mChannelp->setPosition(cur_pos,FMOD_TIMEUNIT_PCMBYTES),"Unable to set current position");

	// Start us playing
	play();
}


bool LLAudioChannelFMODSTUDIO::isPlaying()
{
	if (!mChannelp)
	{
		return false;
	}

	bool paused, playing;
	Check_FMOD_Error(mChannelp->getPaused(&paused),"FMOD::Channel::getPaused");
	Check_FMOD_Error(mChannelp->isPlaying(&playing),"FMOD::Channel::isPlaying");
	return !paused && playing;
}


//
// LLAudioChannelFMODSTUDIO implementation
//


LLAudioBufferFMODSTUDIO::LLAudioBufferFMODSTUDIO(FMOD::System *system) : mSystemp(system), mSoundp(NULL)
{
}


LLAudioBufferFMODSTUDIO::~LLAudioBufferFMODSTUDIO()
{
	if(mSoundp)
	{
		Check_FMOD_Error(mSoundp->release(),"FMOD::Sound::Release");
		mSoundp = NULL;
	}
}


bool LLAudioBufferFMODSTUDIO::loadWAV(const std::string& filename)
{
	// Try to open a wav file from disk.  This will eventually go away, as we don't
	// really want to block doing this.
	if (filename.empty())
	{
		// invalid filename, abort.
		return false;
	}

	if (!gDirUtilp->fileExists(filename))
	{
		// File not found, abort.
		return false;
	}
	
	if (mSoundp)
	{
		// If there's already something loaded in this buffer, clean it up.
		Check_FMOD_Error(mSoundp->release(),"FMOD::Sound::release");
		mSoundp = NULL;
	}

	FMOD_MODE base_mode = FMOD_LOOP_NORMAL;
	FMOD_CREATESOUNDEXINFO exinfo;
	memset(&exinfo,0,sizeof(exinfo));
	exinfo.cbsize = sizeof(exinfo);
	exinfo.suggestedsoundtype = FMOD_SOUND_TYPE_WAV;	//Hint to speed up loading.
	// Load up the wav file into an fmod sample
	FMOD_RESULT result = getSystem()->createSound(filename.c_str(), base_mode, &exinfo, &mSoundp);
	if (result != FMOD_OK)
	{
		// We failed to load the file for some reason.
		LL_WARNS() << "Could not load data '" << filename << "': " << FMOD_ErrorString(result) << LL_ENDL;

		//
		// If we EVER want to load wav files provided by end users, we need
		// to rethink this!
		//
		// file is probably corrupt - remove it.
		LLFile::remove(filename);
		return false;
	}

	// Everything went well, return true
	return true;
}


U32 LLAudioBufferFMODSTUDIO::getLength()
{
	if (!mSoundp)
	{
		return 0;
	}

	U32 length;
	Check_FMOD_Error(mSoundp->getLength(&length, FMOD_TIMEUNIT_PCMBYTES),"FMOD::Sound::getLength");
	return length;
}


void LLAudioChannelFMODSTUDIO::set3DMode(bool use3d)
{
	FMOD_MODE current_mode;
	if(Check_FMOD_Error(mChannelp->getMode(&current_mode),"FMOD::Channel::getMode"))
		return;
	FMOD_MODE new_mode = current_mode;	
	new_mode &= ~(use3d ? FMOD_2D : FMOD_3D);
	new_mode |= use3d ? FMOD_3D : FMOD_2D;

	if(current_mode != new_mode)
	{
		Check_FMOD_Error(mChannelp->setMode(new_mode),"FMOD::Channel::setMode");
	}
}

// *NOTE:  This is almost certainly being called on the mixer thread,
// not the main thread.  May have implications for callees or audio
// engine shutdown.

FMOD_RESULT F_CALLBACK windDSPCallback(FMOD_DSP_STATE *dsp_state, float *inbuffer, float *outbuffer, unsigned int length, int inchannels, int *outchannels)
{
	// inbuffer = incomming data.
	// newbuffer = outgoing data. AKA this DSP's output.
	// length = length in samples at this mix time. True buffer size, in bytes, would be (length * sizeof(float) * inchannels).
	// userdata = user-provided data attached this DSP via FMOD::DSP::setUserData.
	
	LLWindGen<LLAudioEngine_FMODSTUDIO::MIXBUFFERFORMAT> *windgen;
	FMOD::DSP *thisdsp = (FMOD::DSP *)dsp_state->instance;

	thisdsp->getUserData((void **)&windgen);
	
	if (windgen)
		windgen->windGenerate((LLAudioEngine_FMODSTUDIO::MIXBUFFERFORMAT *)outbuffer, length);

	return FMOD_OK;
}
