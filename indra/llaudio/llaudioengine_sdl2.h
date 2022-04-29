#ifndef LL_AUDIOENGINE_SDL2_H
#define LL_AUDIOENGINE_SDL2_H

#include "llaudioengine.h"
#include "llwindgen.h"

class LLAudioEngineSDL2 : public LLAudioEngine
{
public:
    LLAudioEngineSDL2();
    ~LLAudioEngineSDL2();

    bool init(const S32 num_channels, void *userdata, const std::string &app_title) override;
    std::string getDriverName(bool verbose)  override;
    void shutdown() override;

    void updateWind(LLVector3 direction, F32 camera_height_above_water)  override;
    void idle(F32 max_decode_time = 0.f) override;
    void updateChannels() override;

    output_device_map_t getDevices() override;
    void setDevice(const LLUUID& device_uuid) override;


    void deleteChannel( S32 aChannel );

protected:
    LLAudioBuffer *createBuffer()  override;
    LLAudioChannel *createChannel()  override;

    bool initWind()  override;
    void cleanupWind()  override;
    void setInternalGain(F32 gain)  override;

    void allocateListener() override;

    output_device_map_t mDevices;

    struct ImplData;
    std::unique_ptr< ImplData > mData;
};

#endif