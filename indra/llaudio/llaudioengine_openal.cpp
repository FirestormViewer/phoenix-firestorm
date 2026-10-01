/**
 * @file audioengine_openal.cpp
 * @brief implementation of audio engine using OpenAL
 * support as a OpenAL 3D implementation
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
#include "lldir.h"

#include "llaudioengine_openal.h"
#include "lllistener_openal.h"

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

const float LLAudioEngine_OpenAL::WIND_BUFFER_SIZE_SEC = 0.05f;

LLAudioEngine_OpenAL::LLAudioEngine_OpenAL()
    :
    mWindGen(NULL),
    mWindBuf(NULL),
    mWindBufFreq(0),
    mWindBufSamples(0),
    mWindBufBytes(0),
    mWindSource(AL_NONE),
    mNumEmptyWindALBuffers(MAX_NUM_WIND_BUFFERS)
{
}

// virtual
LLAudioEngine_OpenAL::~LLAudioEngine_OpenAL()
{
}

// virtual
bool LLAudioEngine_OpenAL::init(void* userdata, const std::string &app_title)
{
    mWindGen = NULL;
    LLAudioEngine::init(userdata, app_title);

    if(!alutInit(NULL, NULL))
    {
        LL_WARNS() << "LLAudioEngine_OpenAL::init() ALUT initialization failed: " << alutGetErrorString (alutGetError ()) << LL_ENDL;
        return false;
    }

    LL_INFOS() << "LLAudioEngine_OpenAL::init() OpenAL successfully initialized" << LL_ENDL;

    LL_INFOS() << "OpenAL version: "
        << ll_safe_string(alGetString(AL_VERSION)) << LL_ENDL;
    LL_INFOS() << "OpenAL vendor: "
        << ll_safe_string(alGetString(AL_VENDOR)) << LL_ENDL;
    LL_INFOS() << "OpenAL renderer: "
        << ll_safe_string(alGetString(AL_RENDERER)) << LL_ENDL;

    ALint major = alutGetMajorVersion ();
    ALint minor = alutGetMinorVersion ();
    LL_INFOS() << "ALUT version: " << major << "." << minor << LL_ENDL;

    // <FS:Ansariel> Output device selection
    mCurrentDevice = alcOpenDevice(nullptr);
    mCurrentContext = alcCreateContext(mCurrentDevice, nullptr);
    alcMakeContextCurrent(mCurrentContext);
    // </FS:Ansariel>

    ALCdevice *device = alcGetContextsDevice(alcGetCurrentContext());

    alcGetIntegerv(device, ALC_MAJOR_VERSION, 1, &major);
    alcGetIntegerv(device, ALC_MINOR_VERSION, 1, &minor);
    LL_INFOS() << "ALC version: " << major << "." << minor << LL_ENDL;

    LL_INFOS() << "ALC default device: "
        << ll_safe_string(alcGetString(device,
                           ALC_DEFAULT_DEVICE_SPECIFIER))
        << LL_ENDL;

    // <FS:Ansariel> Output device selection
    setupDeviceEvents(device);

    return true;
}

// virtual
std::string LLAudioEngine_OpenAL::getDriverName(bool verbose)
{
    ALCdevice *device = alcGetContextsDevice(alcGetCurrentContext());
    std::ostringstream version;

    version <<
        "OpenAL";

    if (verbose)
    {
        version <<
            ", version " <<
            ll_safe_string(alGetString(AL_VERSION)) <<
            " / " <<
            ll_safe_string(alGetString(AL_VENDOR)) <<
            " / " <<
            ll_safe_string(alGetString(AL_RENDERER));

        if (device)
            version <<
                ": " <<
                ll_safe_string(alcGetString(device,
                    ALC_DEFAULT_DEVICE_SPECIFIER));
    }

    return version.str();
}

// virtual
void LLAudioEngine_OpenAL::allocateListener()
{
    mListenerp = (LLListener *) new LLListener_OpenAL();
    if(!mListenerp)
    {
        LL_WARNS() << "LLAudioEngine_OpenAL::allocateListener() Listener creation failed" << LL_ENDL;
    }
}

// virtual
void LLAudioEngine_OpenAL::shutdown()
{
    LL_INFOS() << "About to LLAudioEngine::shutdown()" << LL_ENDL;
    LLAudioEngine::shutdown();

    // If a subsequent error occurs while there is still an error recorded
    // internally, the second error will simply be ignored.
    // Clear previous error to make sure we will captuare a valid failure reason
    ALenum error = alutGetError();
    if (error != ALUT_ERROR_NO_ERROR)
    {
        LL_WARNS() << "Uncleared error state prior to shutdown: "
            << alutGetErrorString(error) << LL_ENDL;
    }

    LL_INFOS() << "About to alutExit()" << LL_ENDL;
    if(!alutExit())
    {
        LL_WARNS() << "LLAudioEngine_OpenAL::shutdown() ALUT shutdown failed: " << alutGetErrorString (alutGetError ()) << LL_ENDL;
    }

    LL_INFOS() << "LLAudioEngine_OpenAL::shutdown() OpenAL successfully shut down" << LL_ENDL;

    delete mListenerp;
    mListenerp = NULL;
}

LLAudioBuffer *LLAudioEngine_OpenAL::createBuffer()
{
    return new LLAudioBufferOpenAL();
}

LLAudioChannel *LLAudioEngine_OpenAL::createChannel()
{
    return new LLAudioChannelOpenAL();
}

void LLAudioEngine_OpenAL::setInternalGain(F32 gain)
{
    //LL_INFOS() << "LLAudioEngine_OpenAL::setInternalGain() Gain: " << gain << LL_ENDL;
    alListenerf(AL_GAIN, gain);
}

LLAudioChannelOpenAL::LLAudioChannelOpenAL()
    :
    mALSource(AL_NONE),
    mLastSamplePos(0)
{
    alGenSources(1, &mALSource);
}

LLAudioChannelOpenAL::~LLAudioChannelOpenAL()
{
    cleanup();
    alDeleteSources(1, &mALSource);
}

void LLAudioChannelOpenAL::cleanup()
{
    alSourceStop(mALSource);
    alSourcei(mALSource, AL_BUFFER, AL_NONE);

    mCurrentBufferp = NULL;
}

void LLAudioChannelOpenAL::play()
{
    if (mALSource == AL_NONE)
    {
        LL_WARNS() << "Playing without a mALSource, aborting" << LL_ENDL;
        return;
    }

    if(!isPlaying())
    {
        alSourcePlay(mALSource);
        getSource()->setPlayedOnce(true);
    }
}

void LLAudioChannelOpenAL::playSynced(LLAudioChannel *channelp)
{
    if (channelp)
    {
        LLAudioChannelOpenAL *masterchannelp =
            (LLAudioChannelOpenAL*)channelp;
        if (mALSource != AL_NONE &&
            masterchannelp->mALSource != AL_NONE)
        {
            // we have channels allocated to master and slave
            ALfloat master_offset;
            alGetSourcef(masterchannelp->mALSource, AL_SEC_OFFSET,
                     &master_offset);

            LL_INFOS() << "Syncing with master at " << master_offset
                << "sec" << LL_ENDL;
            // *TODO: detect when this fails, maybe use AL_SAMPLE_
            alSourcef(mALSource, AL_SEC_OFFSET, master_offset);
        }
    }
    play();
}

bool LLAudioChannelOpenAL::isPlaying()
{
    if (mALSource != AL_NONE)
    {
        ALint state;
        alGetSourcei(mALSource, AL_SOURCE_STATE, &state);
        if(state == AL_PLAYING)
        {
            return true;
        }
    }

    return false;
}

bool LLAudioChannelOpenAL::updateBuffer()
{
    if (!mCurrentSourcep)
    {
        // This channel isn't associated with any source, nothing
        // to be updated
        return false;
    }

    if (LLAudioChannel::updateBuffer())
    {
        // Base class update returned true, which means that we need to actually
        // set up the source for a different buffer.
        LLAudioBufferOpenAL *bufferp = (LLAudioBufferOpenAL *)mCurrentSourcep->getCurrentBuffer();
        ALuint buffer = bufferp->getBuffer();
        alSourcei(mALSource, AL_BUFFER, buffer);
        mLastSamplePos = 0;
    }

    if (mCurrentSourcep)
    {
        alSourcef(mALSource, AL_GAIN,
              mCurrentSourcep->getGain() * getSecondaryGain());
        alSourcei(mALSource, AL_LOOPING,
              mCurrentSourcep->isLoop() ? AL_TRUE : AL_FALSE);
        alSourcef(mALSource, AL_ROLLOFF_FACTOR,
              gAudiop->mListenerp->getRolloffFactor());
    }

    return true;
}


void LLAudioChannelOpenAL::updateLoop()
{
    if (mALSource == AL_NONE)
    {
        return;
    }

    // Hack:  We keep track of whether we looped or not by seeing when the
    // sample position looks like it's going backwards.  Not reliable; may
    // yield false negatives.
    //
    ALint cur_pos;
    alGetSourcei(mALSource, AL_SAMPLE_OFFSET, &cur_pos);
    if (cur_pos < mLastSamplePos)
    {
        mLoopedThisFrame = true;
    }
    mLastSamplePos = cur_pos;
}


void LLAudioChannelOpenAL::update3DPosition()
{
    if(!mCurrentSourcep)
    {
        return;
    }
    if (mCurrentSourcep->isForcedPriority())
    {
        alSource3f(mALSource, AL_POSITION, 0.0, 0.0, 0.0);
        alSource3f(mALSource, AL_VELOCITY, 0.0, 0.0, 0.0);
        alSourcei (mALSource, AL_SOURCE_RELATIVE, AL_TRUE);
    } else {
        LLVector3 float_pos;
        float_pos.setVec(mCurrentSourcep->getPositionGlobal());
        alSourcefv(mALSource, AL_POSITION, float_pos.mV);
        alSourcefv(mALSource, AL_VELOCITY, mCurrentSourcep->getVelocity().mV);
        alSourcei (mALSource, AL_SOURCE_RELATIVE, AL_FALSE);
    }

    alSourcef(mALSource, AL_GAIN, mCurrentSourcep->getGain() * getSecondaryGain());
}

LLAudioBufferOpenAL::LLAudioBufferOpenAL()
{
    mALBuffer = AL_NONE;
}

LLAudioBufferOpenAL::~LLAudioBufferOpenAL()
{
    cleanup();
}

void LLAudioBufferOpenAL::cleanup()
{
    if(mALBuffer != AL_NONE)
    {
        alGetError(); // <ND/>
        alDeleteBuffers(1, &mALBuffer);

        // <FS:ND> Print warning on possible leak.
        ALenum error = alGetError();
        if( AL_NO_ERROR != error )
            LL_WARNS() << "openal error: " << error << " possible memory leak hit" << LL_ENDL;
        // </FS:ND>

        mALBuffer = AL_NONE;
    }
}

bool LLAudioBufferOpenAL::loadWAV(const std::string& filename)
{
    cleanup();
    mALBuffer = alutCreateBufferFromFile(filename.c_str());
    if(mALBuffer == AL_NONE)
    {
        ALenum error = alutGetError();
        if (gDirUtilp->fileExists(filename))
        {
            LL_WARNS() <<
                "LLAudioBufferOpenAL::loadWAV() Error loading "
                << filename
                << " " << alutGetErrorString(error) << LL_ENDL;
        }
        else
        {
            // It's common for the file to not actually exist.
            LL_DEBUGS() <<
                "LLAudioBufferOpenAL::loadWAV() Error loading "
                 << filename
                 << " " << alutGetErrorString(error) << LL_ENDL;
        }
        return false;
    }

    return true;
}

U32 LLAudioBufferOpenAL::getLength()
{
    if(mALBuffer == AL_NONE)
    {
        return 0;
    }
    ALint length;
    alGetBufferi(mALBuffer, AL_SIZE, &length);
    return length / 2; // convert size in bytes to size in (16-bit) samples
}

// ------------

bool LLAudioEngine_OpenAL::initWind()
{
    ALenum error;
    LL_INFOS() << "LLAudioEngine_OpenAL::initWind() start" << LL_ENDL;

    mNumEmptyWindALBuffers = MAX_NUM_WIND_BUFFERS;

    alGetError(); /* clear error */

    alGenSources(1,&mWindSource);

    if((error=alGetError()) != AL_NO_ERROR)
    {
        LL_WARNS() << "LLAudioEngine_OpenAL::initWind() Error creating wind sources: "<<error<<LL_ENDL;
    }

    mWindGen = new LLWindGen<WIND_SAMPLE_T>;

    mWindBufFreq = mWindGen->getInputSamplingRate();
    mWindBufSamples = llceil(mWindBufFreq * WIND_BUFFER_SIZE_SEC);
    mWindBufBytes = mWindBufSamples * 2 /*stereo*/ * sizeof(WIND_SAMPLE_T);

    mWindBuf = new WIND_SAMPLE_T [mWindBufSamples * 2 /*stereo*/];

    if(mWindBuf==NULL)
    {
        LL_ERRS() << "LLAudioEngine_OpenAL::initWind() Error creating wind memory buffer" << LL_ENDL;
        return false;
    }

    LL_INFOS() << "LLAudioEngine_OpenAL::initWind() done" << LL_ENDL;

    return true;
}

void LLAudioEngine_OpenAL::cleanupWind()
{
    LL_INFOS() << "LLAudioEngine_OpenAL::cleanupWind()" << LL_ENDL;

    if (mWindSource != AL_NONE)
    {
        // detach and delete all outstanding buffers on the wind source
        alSourceStop(mWindSource);
        ALint processed;
        alGetSourcei(mWindSource, AL_BUFFERS_PROCESSED, &processed);
        while (processed--)
        {
            ALuint buffer = AL_NONE;
            alSourceUnqueueBuffers(mWindSource, 1, &buffer);
            alDeleteBuffers(1, &buffer);
        }

        // delete the wind source itself
        alDeleteSources(1, &mWindSource);

        mWindSource = AL_NONE;
    }

    delete[] mWindBuf;
    mWindBuf = NULL;

    delete mWindGen;
    mWindGen = NULL;
}

void LLAudioEngine_OpenAL::updateWind(LLVector3 wind_vec, F32 camera_altitude)
{
    LLVector3 wind_pos;
    F64 pitch;
    F64 center_freq;
    ALenum error;

    if (!mEnableWind)
        return;

    if(!mWindBuf)
        return;

    if (mWindUpdateTimer.checkExpirationAndReset(LL_WIND_UPDATE_INTERVAL))
    {

        // wind comes in as Linden coordinate (+X = forward, +Y = left, +Z = up)
        // need to convert this to the conventional orientation DS3D and OpenAL use
        // where +X = right, +Y = up, +Z = backwards

        wind_vec.setVec(-wind_vec.mV[1], wind_vec.mV[2], -wind_vec.mV[0]);

        pitch = 1.0 + mapWindVecToPitch(wind_vec);
        center_freq = 80.0 * pow(pitch,2.5*(mapWindVecToGain(wind_vec)+1.0));

        mWindGen->mTargetFreq = (F32)center_freq;
        mWindGen->mTargetGain = (F32)mapWindVecToGain(wind_vec) * mMaxWindGain;
        mWindGen->mTargetPanGainR = (F32)mapWindVecToPan(wind_vec);

        alSourcei(mWindSource, AL_LOOPING, AL_FALSE);
        alSource3f(mWindSource, AL_POSITION, 0.0, 0.0, 0.0);
        alSource3f(mWindSource, AL_VELOCITY, 0.0, 0.0, 0.0);
        alSourcef(mWindSource, AL_ROLLOFF_FACTOR, 0.0);
        alSourcei(mWindSource, AL_SOURCE_RELATIVE, AL_TRUE);
    }

    // ok lets make a wind buffer now

    ALint processed, queued, unprocessed;
    alGetSourcei(mWindSource, AL_BUFFERS_PROCESSED, &processed);
    alGetSourcei(mWindSource, AL_BUFFERS_QUEUED, &queued);
    unprocessed = queued - processed;

    // ensure that there are always at least 3x as many filled buffers
    // queued as we managed to empty since last time.
    mNumEmptyWindALBuffers = llmin(mNumEmptyWindALBuffers + processed * 3 - unprocessed, MAX_NUM_WIND_BUFFERS-unprocessed);
    mNumEmptyWindALBuffers = llmax(mNumEmptyWindALBuffers, 0);

    //LL_INFOS() << "mNumEmptyWindALBuffers: " << mNumEmptyWindALBuffers    <<" (" << unprocessed << ":" << processed << ")" << LL_ENDL;

    while(processed--) // unqueue old buffers
    {
        ALuint buffer;
        ALenum error;
        alGetError(); /* clear error */
        alSourceUnqueueBuffers(mWindSource, 1, &buffer);
        error = alGetError();
        if(error != AL_NO_ERROR)
        {
            LL_WARNS() << "LLAudioEngine_OpenAL::updateWind() error swapping (unqueuing) buffers" << LL_ENDL;
        }
        else
        {
            alDeleteBuffers(1, &buffer);
        }
    }

    unprocessed += mNumEmptyWindALBuffers;
    while (mNumEmptyWindALBuffers > 0) // fill+queue new buffers
    {
        ALuint buffer;
        alGetError(); /* clear error */
        alGenBuffers(1,&buffer);
        if((error=alGetError()) != AL_NO_ERROR)
        {
            LL_WARNS() << "LLAudioEngine_OpenAL::updateWind() Error creating wind buffer: " << error << LL_ENDL;
            break;
        }

        alBufferData(buffer,
                 AL_FORMAT_STEREO_FLOAT32,
                 mWindGen->windGenerate(mWindBuf,
                            mWindBufSamples),
                 mWindBufBytes,
                 mWindBufFreq);
        error = alGetError();
        if(error != AL_NO_ERROR)
        {
            LL_WARNS() << "LLAudioEngine_OpenAL::updateWind() error swapping (bufferdata) buffers" << LL_ENDL;
        }

        alSourceQueueBuffers(mWindSource, 1, &buffer);
        error = alGetError();
        if(error != AL_NO_ERROR)
        {
            LL_WARNS() << "LLAudioEngine_OpenAL::updateWind() error swapping (queuing) buffers" << LL_ENDL;
        }

        --mNumEmptyWindALBuffers;
    }

    ALint playing;
    alGetSourcei(mWindSource, AL_SOURCE_STATE, &playing);
    if(playing != AL_PLAYING)
    {
        alSourcePlay(mWindSource);

        LL_DEBUGS() << "Wind had stopped - probably ran out of buffers - restarting: " << (unprocessed+mNumEmptyWindALBuffers) << " now queued." << LL_ENDL;
    }
}

// <FS:Ansariel> Output device selection
static LLUUID generate_uuid_from_string(const std::string& str)
{
    // Setup standard DNS namespace
    boost::uuids::string_generator string_generator;
    boost::uuids::uuid dns_namespace = string_generator("6144c9de-e8f3-4603-b8f7-a195db46f0c0");

    // Setup SHA1 name generator with the namespace
    boost::uuids::name_generator_sha1 uuid_generator(dns_namespace);

    // Generate deterministic UUID based on a string
    boost::uuids::uuid deterministic_uuid = uuid_generator(str);

    return LLUUID(boost::uuids::to_string(deterministic_uuid));
}

static void ALC_APIENTRY device_change_callback(ALCenum eventType, ALCenum deviceType, ALCdevice* device, ALCsizei length, const ALCchar* message, void* userParam) noexcept
{
    LLAudioEngine_OpenAL* audio_engine = (LLAudioEngine_OpenAL*)userParam;

    if (!audio_engine)
        return;

    if (eventType == ALC_EVENT_TYPE_DEVICE_ADDED_SOFT || eventType == ALC_EVENT_TYPE_DEVICE_REMOVED_SOFT)
    {
        if (deviceType == ALC_PLAYBACK_DEVICE_SOFT)
        {
            LL_INFOS() << "LLAudioEngine_OpenAL:: Playback device list changed" << LL_ENDL;
            audio_engine->mDeviceListChanged = true;
            return;
        }
    }

    if (eventType == ALC_EVENT_TYPE_DEFAULT_DEVICE_CHANGED_SOFT)
    {
        LL_INFOS() << "LLAudioEngine_OpenAL:: Default playback device changed" << LL_ENDL;
        audio_engine->mDeviceListChanged = true;
    }
}

void LLAudioEngine_OpenAL::setupDeviceEvents(ALCdevice* device)
{
    if (!alcIsExtensionPresent(device, "ALC_SOFT_system_events"))
    {
        LL_WARNS() << "LLAudioEngine_OpenAL:: ALC_SOFT_system_events not supported by backend" << LL_ENDL;
        return;
    }

    auto alcEventCallbackSOFT = (LPALCEVENTCALLBACKSOFT)alcGetProcAddress(device, "alcEventCallbackSOFT");
    auto alcEventControlSOFT = (LPALCEVENTCONTROLSOFT)alcGetProcAddress(device, "alcEventControlSOFT");
    if (!alcEventCallbackSOFT || !alcEventControlSOFT)
    {
        LL_WARNS() << "LLAudioEngine_OpenAL:: ALC_SOFT_system_events functions not available" << LL_ENDL;
        return;
    }

    // Register the callback function
    alcEventCallbackSOFT(device_change_callback, this);

    // Define which events we want to listen to
    ALCenum events[] = {
        ALC_EVENT_TYPE_DEVICE_ADDED_SOFT,
        ALC_EVENT_TYPE_DEVICE_REMOVED_SOFT,
        ALC_EVENT_TYPE_DEFAULT_DEVICE_CHANGED_SOFT
    };

    // Enable events
    alcEventControlSOFT(3, events, AL_TRUE);
}

void LLAudioEngine_OpenAL::changeDevice()
{
    LPALCREOPENDEVICESOFT alcReopenDeviceSOFT = (LPALCREOPENDEVICESOFT)alcGetProcAddress(mCurrentDevice, "alcReopenDeviceSOFT");

    if (!alcReopenDeviceSOFT || !alcIsExtensionPresent(mCurrentDevice, "ALC_SOFT_reopen_device"))
    {
        LL_WARNS() << "LLAudioEngine_OpenAL::setDevice(): Cannot change output device - OpenAL ALC_SOFT_reopen_device extension missing" << LL_ENDL;
        return;
    }

    std::optional<std::string> device_name{};

    if (mSelectedDeviceUUID.isNull())
    {
        LL_INFOS() << "LLAudioEngine_OpenAL::setDevice(): Setting new output to system default (" << mSelectedDeviceUUID.asString() << ")" << LL_ENDL;
    }
    else
    {
        for (const auto& [id, name] : getDevices())
        {
            if (id == mSelectedDeviceUUID)
            {
                LL_INFOS() << "LLAudioEngine_OpenAL::setDevice(): Setting new output " << name << " (" << id.asString() << ")" << LL_ENDL;
                device_name = name;
                break;
            }
        }

        if (!device_name.has_value())
        {
            LL_WARNS() << "LLAudioEngine_OpenAL::setDevice(): Output device not found - falling back to system default" << LL_ENDL;
        }
    }

    if (alcReopenDeviceSOFT(mCurrentDevice, device_name.has_value() ? device_name.value().c_str() : nullptr, nullptr))
    {
        alcMakeContextCurrent(mCurrentContext);
    }
    else
    {
        LL_WARNS() << "LLAudioEngine_OpenAL::setDevice() error: Reopen failed" << LL_ENDL;
    }
}

LLAudioEngine_OpenAL::output_device_map_t LLAudioEngine_OpenAL::getDevices()
{
    if (!alcIsExtensionPresent(nullptr, "ALC_ENUMERATE_ALL_EXT"))
    {
        LL_WARNS() << "LLAudioEngine_OpenAL::getDevices(): Cannot determine playback devices - OpenAL ALC_ENUMERATE_ALL_EXT extension missing" << LL_ENDL;
        return{};
    }

    const ALCchar* deviceList = alcGetString(nullptr, ALC_ALL_DEVICES_SPECIFIER);

    output_device_map_t devices;
    const ALCchar* nextDevice = deviceList;

    // Parse double-null terminated string block
    while (nextDevice && *nextDevice != '\0')
    {
        std::string device(nextDevice);
        devices.emplace(generate_uuid_from_string(device), device);
        nextDevice += strlen(nextDevice) + 1;
    }

    return devices;
}

void LLAudioEngine_OpenAL::setDevice(const LLUUID& device_uuid)
{
    mSelectedDeviceUUID = device_uuid;
    changeDevice();
}

void LLAudioEngine_OpenAL::idle()
{
    if (mDeviceListChanged)
    {
        mDeviceListChanged = false;
        changeDevice();
        OnOutputDeviceListChanged(getDevices());
    }

    LLAudioEngine::idle();
}
