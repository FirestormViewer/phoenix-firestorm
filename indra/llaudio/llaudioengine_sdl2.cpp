#include "llaudioengine_sdl2.h"
#include "llstreamingaudio.h"
#include "lllistener.h"

#include "SDL2/SDL_mixer.h"

#include "curl/curl.h"

namespace
{
    std::string getMixerError()
    {
        auto pError = Mix_GetError();
        if(!pError)
            return "Mix_Error: returned nullptr";

        return std::string{ "Mix_error: " } + pError;
    }
}

struct RWData
{
    std::string mURL;
    CURLM *mMulti{ nullptr };
    CURL *mEasy{ nullptr };
};

Sint64 SDLCALL stream_size (struct SDL_RWops * context)
{
    return -1;
}

Sint64(SDLCALL stream_seek) (struct SDL_RWops * context, Sint64 offset,
                         int whence)
{
    if( !context )
        return  -1;

    RWData *pData = reinterpret_cast<RWData*>(context->hidden.unknown.data1);
    return -1;
}

size_t SDLCALL stream_read (struct SDL_RWops * context, void *ptr, size_t size, size_t maxnum)
{
    if( !context )
        return  0;

    RWData *pData = reinterpret_cast<RWData*>(context->hidden.unknown.data1);
    return 0;
}

size_t SDLCALL stream_write (struct SDL_RWops * context, const void *ptr,
                          size_t size, size_t num)
{
    if( !context )
        return  0;

    RWData *pData = reinterpret_cast<RWData*>(context->hidden.unknown.data1);
    return 0;
}

int SDLCALL stream_close (struct SDL_RWops * context)
{
    if( !context )
        return  0;

    RWData *pData = reinterpret_cast<RWData*>(context->hidden.unknown.data1);
    // Free curl multi and easy


    return 0;
}


class LLListenerSDL2: public LLListener
{
    LLAudioEngineSDL2 *mEngine;
public:
    LLListenerSDL2(LLAudioEngineSDL2 *aEngine )
            : mEngine( aEngine )
    {

    }

};

class LLStreamingAudioSDL2: public LLStreamingAudioInterface
{
    std::string mURL;
    F32 mVolume { 1.0 };
    Mix_Music *mMusic{nullptr};
    SDL_RWops *mReader{ nullptr };
    RWData *mData;
public:

    void start(const std::string& url) override
    {
        mURL = url;
        //mMusic = Mix_LoadMUS( url.c_str() );
        mReader = SDL_AllocRW();
        mData = new RWData{};
        mReader->hidden.unknown.data1 = mData;

        mReader->size = stream_size;
        mReader->seek = stream_seek;
        mReader->read = stream_read;
        mReader->write = stream_write;
        mReader->close = stream_close;

        mMusic = Mix_LoadMUS_RW( mReader, 0 );
        if( mMusic )
        {
            auto x = Mix_PlayMusic( mMusic, 0 );
            LL_WARNS() << "SDL2: "  << getMixerError() << " "  << x << LL_ENDL;
        }
        else
        {
            LL_WARNS() << "SDL2: MixLoadMUS failed: " << getMixerError() << LL_ENDL;
        }

    }

    void stop() override
    {
    }

    void pause(int pause) override
    {
    }

    void update() override
    {
    }

    int isPlaying() override
    {
        return mMusic != nullptr;
    }

    void setGain(F32 vol) override
    {
        mVolume = vol;
    }

    F32 getGain() override
    {
        return mVolume;
    }

    std::string getURL() override
    {
        return mURL;
    }

    bool supportsAdjustableBufferSizes() override
    {
        return false;
    }

    void setBufferSizes(U32 streambuffertime, U32 decodebuffertime) override
    {
    }
};

class LLAudioBufferSDL2: public LLAudioBuffer
{
public:
    Mix_Chunk *mChunk;

    ~LLAudioBufferSDL2()
    {
        if( mChunk )
            Mix_FreeChunk(mChunk);
    }

    bool loadWAV(const std::string& filename)
    {
        mChunk = Mix_LoadWAV( filename.c_str() );
        return mChunk != nullptr;
    }

    U32 getLength()
    {
        return 0; // Not needed here
    }

};

class LLAudioChannelSDL2: public LLAudioChannel
{
    S32 mChannel;
    LLAudioEngineSDL2 *mEngine;
    bool mPlayback{false};

    uint32_t caclulateVolume()
    {
        if( mChannel < 0 || !mCurrentSourcep || !mEngine)
            return 0.0f;


        F32 gain =  mCurrentSourcep->getGain() * getSecondaryGain() * mEngine->getMasterGain();
        llclamp(gain, 0.0f, 1.f );
        gain *= 255.f;
        return static_cast<uint32_t>(gain);

    }
public:
    LLAudioChannelSDL2( S32 nChannel, LLAudioEngineSDL2 *aEngine )
            : mChannel( nChannel )
            , mEngine( aEngine )
    {

    }

    ~LLAudioChannelSDL2()
    {
        mEngine->deleteChannel( mChannel );
    }

    void play() override
    {
        startPlayback();
    }

    void startPlayback()
    {
        if( mChannel < 0 )
            return;

        //if( mPlayback )
        //    return;

        Mix_HaltChannel( mChannel );
        LLAudioBufferSDL2* pBuffer = (LLAudioBufferSDL2*)mCurrentBufferp;
        if( !pBuffer )
            return;

        mPlayback = true;
        Mix_PlayChannel( mChannel, pBuffer->mChunk, mCurrentSourcep->isLoop()?-1:0 );
        Mix_Volume( mChannel, caclulateVolume());
    }

    void playSynced(LLAudioChannel *channelp) override
    {
        play();
    }

    void cleanup() override
    {
        if( mChannel < 0 )
            return;

        mPlayback = false;
        Mix_HaltChannel( mChannel );
    }

    bool isPlaying() override
    {
        if( mChannel < 0 )
            return false;

        if( !mPlayback )
            return false;

        bool bRet = Mix_Playing( mChannel ) == 1;
        if( !bRet )
        {
            mPlayback = false;
            Mix_HaltChannel( mChannel );
        }

        return bRet;
    }

    bool updateBuffer() override
    {
        if( !mCurrentSourcep || mChannel < 0)
            return false;

        if( LLAudioChannel::updateBuffer() )
        {
            if(mCurrentSourcep)
            {
                startPlayback();
                return true;
            }
        }
        else if( mCurrentSourcep )
        {
            if( mPlayback )
                Mix_Volume( mChannel, caclulateVolume());
        }
        else
        {
            if( mPlayback )
                Mix_HaltChannel(mChannel);
            mPlayback = false;
        }
        return mCurrentSourcep != 0;
    }

    void update3DPosition() override
    {
        if(!mCurrentSourcep || mChannel < 0)
            return;

        if(mPlayback)
        {
            auto pos = mEngine->getListenerPos();
            LLVector3 soundpos{mCurrentSourcep->getPositionGlobal()};
            F32 dist = dist_vec(soundpos, pos);
            dist = llclamp(dist, 0.0f, 255.0f);
            Mix_SetPosition(mChannel, 0, (uint8_t) dist);
        }
    }

    void updateLoop() override
    {
    }

};

struct  LLAudioEngineSDL2::ImplData
{
    std::vector< uint8_t > mChannels;
    bool mReady { false };
};

LLAudioEngineSDL2::LLAudioEngineSDL2()
{

}

LLAudioEngineSDL2::~LLAudioEngineSDL2()
{

}

bool LLAudioEngineSDL2::init(const S32 num_channels, void *userdata, const std::string &app_title)
{
    LL_INFOS() << "Initializing SDL2 audio" << LL_ENDL;
    mData = std::make_unique<ImplData>();

    LLAudioEngine::init(num_channels, userdata, app_title);

    int flags = MIX_INIT_FLAC | MIX_INIT_MP3 | MIX_INIT_MP3;
    if( flags != Mix_Init( flags ) )
        LL_WARNS() << "Mix_Init failed to intialize all formats: "  << getMixerError () << LL_ENDL;

    if( 0 != Mix_OpenAudio(44100, AUDIO_S16LSB, MIX_DEFAULT_CHANNELS, 1024 ) )
    {
        LL_WARNS() << "Mix_OpenAudio failed " << getMixerError() << LL_ENDL;
        return false;
    }
    mData->mChannels.resize( num_channels+2 );
    Mix_AllocateChannels( num_channels );
    mData->mReady = true;

    // Impl is stubbed out, but nothing more
//    if (!getStreamingAudioImpl())
//        setStreamingAudioImpl( new LLStreamingAudioSDL2() );


    return true;

}
std::string LLAudioEngineSDL2::getDriverName(bool verbose)
{
    return "SDL mixer";
}

void LLAudioEngineSDL2::shutdown()
{
    Mix_CloseAudio();
    Mix_Quit();

    mData = nullptr;
}

void LLAudioEngineSDL2::updateWind(LLVector3 direction, F32 camera_height_above_water)
{

}

void LLAudioEngineSDL2::idle(F32 max_decode_time )
{
    LLAudioEngine::idle(max_decode_time);
}

void LLAudioEngineSDL2::updateChannels()
{
    LLAudioEngine::updateChannels();
}

LLAudioEngineSDL2::output_device_map_t LLAudioEngineSDL2::getDevices()
{
// Impl looks like this, but it might not be possible to support this right now
/*
    if(mDevices.size())
        return;

    auto numDevices{SDL_GetNumAudioDevices(0)};
    if( numDevices <=  0 )
        return {};

    LLAudioEngineSDL2::output_device_map_t devices{};
    for( auto i{0}; i < numDevices; ++i )
    {
        auto pName{SDL_GetAudioDeviceName(i, 0)};
        if( !pName )
            continue;

        std::string strName{ pName };

        devices[ LLUUID::generateNewID( strName )] = strName;
    }
*/

    return mDevices;
}

void LLAudioEngineSDL2::setDevice(const LLUUID &device_uuid)
{

}

LLAudioBuffer *LLAudioEngineSDL2::createBuffer()
{
    return new LLAudioBufferSDL2();
}

LLAudioChannel *LLAudioEngineSDL2::createChannel()
{
    S32 channelNum{ -1 };
    if( mData && mData->mReady )
    {
        for( S32 i = 0; i < mData->mChannels.size(); ++i )
        {
            if( !mData->mChannels[i])
            {
                channelNum = i;
                mData->mChannels[i] = 1;
                break;
            }
        }

        if( channelNum == -1 )
        {
            channelNum = mData->mChannels.size();
            mData->mChannels.push_back(0);
            Mix_AllocateChannels( mData->mChannels.size() );
        }
    }

    return new LLAudioChannelSDL2(channelNum, this);
}

void LLAudioEngineSDL2::deleteChannel(S32 aChannel)
{
    if( !mData )
        return;

    if( aChannel < 0 || aChannel >= mData->mChannels.size() )
        return;

    mData->mChannels[aChannel] = 0;
}

bool LLAudioEngineSDL2::initWind()
{
    // Not supported
    return false;
}

void LLAudioEngineSDL2::cleanupWind()
{

}

void LLAudioEngineSDL2::setInternalGain(F32 gain)
{

}

void LLAudioEngineSDL2::allocateListener()
{
    mListenerp = new LLListenerSDL2(this);
}

