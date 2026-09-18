#ifndef LL_LLVLCWASAPI_H
#define LL_LLVLCWASAPI_H

static ma_result llvlc_wasapi_loop(ma_device* device)
{
    using llvlc::WasapiEndpoint;
    auto* client = static_cast<ma_IAudioClient*>(device->wasapi.pAudioClientPlayback);
    auto* renderer = static_cast<ma_IAudioRenderClient*>(device->wasapi.pRenderClient);
    struct ClockOwner
    {
        IAudioClock* clock = nullptr;
        ~ClockOwner() { if (clock) clock->Release(); }
    } owner;
    const auto fail = [device]()
    {
        if (WasapiEndpoint::stopping(device)) return MA_SUCCESS;
        WasapiEndpoint::invalidate(device);
        return MA_ERROR;
    };
    const auto clockId = __uuidof(IAudioClock);
    if (ma_IAudioClient_GetService(client, &clockId, reinterpret_cast<void**>(&owner.clock)) != S_OK)
        return fail();
    UINT64 frequency = 0;
    MA_REFERENCE_TIME latency = 0;
    if (owner.clock->GetFrequency(&frequency) != S_OK || !frequency ||
        ma_IAudioClient_GetStreamLatency(client, &latency) != S_OK || latency < 0)
        return fail();
    llvlc::EndpointObservation observation;
    observation.deviceEpoch = WasapiEndpoint::epoch(device);
    observation.frequency = frequency;
    observation.latency100ns = static_cast<std::uint64_t>(latency);
    bool started = false;
    while (ma_device_get_state(device) == MA_STATE_STARTED && WasapiEndpoint::healthy(device))
    {
        ma_uint32 padding = 0;
        if (ma_IAudioClient_GetCurrentPadding(client, &padding) != S_OK ||
            padding > device->wasapi.actualPeriodSizeInFramesPlayback) return fail();
        UINT64 position = 0;
        const auto clockResult = owner.clock->GetPosition(&position, nullptr);
        if (clockResult != S_OK) return fail();
        observation.paddingFrames = padding;
        observation.position = position;
        if (!WasapiEndpoint::observe(device, observation)) return fail();
        const auto available = device->wasapi.actualPeriodSizeInFramesPlayback - padding;
        if (available >= device->wasapi.periodSizeInFramesPlayback)
        {
            BYTE* buffer = nullptr;
            if (ma_IAudioRenderClient_GetBuffer(renderer, available, &buffer) != S_OK) return fail();
            ma_device__read_frames_from_client(device, available, buffer);
            const auto flags = WasapiEndpoint::healthy(device) ? 0u : AUDCLNT_BUFFERFLAGS_SILENT;
            if (ma_IAudioRenderClient_ReleaseBuffer(renderer, available, flags) != S_OK) return fail();
            if (!WasapiEndpoint::healthy(device))
                return WasapiEndpoint::stopping(device) ? MA_SUCCESS : fail();
            if (!WasapiEndpoint::submitted(device, available)) return fail();
        }
        if (!started)
        {
            if (ma_IAudioClient_Start(client) != S_OK) return fail();
            c89atomic_exchange_32(&device->wasapi.isStartedPlayback, MA_TRUE);
            started = true;
        }
        if (WaitForSingleObject(device->wasapi.hEventPlayback, MA_WASAPI_WAIT_TIMEOUT_MILLISECONDS) != WAIT_OBJECT_0)
            return fail();
    }
    return MA_SUCCESS;
}
#endif