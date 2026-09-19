#ifndef LL_LLVLCWASAPI_H
#define LL_LLVLCWASAPI_H

static ma_result llvlc_wasapi_loop(ma_device* device)
{
    using llvlc::WasapiEndpoint;
    using llvlc::DeviceFailure;
    auto* client = static_cast<ma_IAudioClient*>(device->wasapi.pAudioClientPlayback);
    auto* renderer = static_cast<ma_IAudioRenderClient*>(device->wasapi.pRenderClient);
    struct ClockOwner
    {
        IAudioClock* clock = nullptr;
        ~ClockOwner() { if (clock) clock->Release(); }
    } owner;
    const auto fail = [device](DeviceFailure reason, std::uint32_t code = 0)
    {
        if (WasapiEndpoint::stopping(device)) return MA_SUCCESS;
        WasapiEndpoint::invalidate(device, reason, code);
        return MA_ERROR;
    };
    const auto clockId = __uuidof(IAudioClock);
    const auto serviceResult = ma_IAudioClient_GetService(client, &clockId, reinterpret_cast<void**>(&owner.clock));
    if (serviceResult != S_OK)
        return fail(DeviceFailure::ClockService, static_cast<std::uint32_t>(serviceResult));
    UINT64 frequency = 0;
    MA_REFERENCE_TIME latency = 0;
    const auto frequencyResult = owner.clock->GetFrequency(&frequency);
    if (frequencyResult != S_OK || !frequency)
        return fail(DeviceFailure::ClockFrequency, static_cast<std::uint32_t>(frequencyResult));
    const auto latencyResult = ma_IAudioClient_GetStreamLatency(client, &latency);
    if (latencyResult != S_OK || latency < 0)
        return fail(DeviceFailure::StreamLatency, static_cast<std::uint32_t>(latencyResult));
    llvlc::EndpointObservation observation;
    observation.deviceEpoch = WasapiEndpoint::epoch(device);
    observation.frequency = frequency;
    observation.latency100ns = static_cast<std::uint64_t>(latency);
    bool started = false;
    while (ma_device_get_state(device) == MA_STATE_STARTED && WasapiEndpoint::healthy(device))
    {
        ma_uint32 padding = 0;
        const auto paddingResult = ma_IAudioClient_GetCurrentPadding(client, &padding);
        if (paddingResult != S_OK || padding > device->wasapi.actualPeriodSizeInFramesPlayback)
            return fail(DeviceFailure::Padding, static_cast<std::uint32_t>(paddingResult));
        UINT64 position = 0;
        const auto clockResult = owner.clock->GetPosition(&position, nullptr);
        if (clockResult != S_OK) return fail(DeviceFailure::ClockPosition, static_cast<std::uint32_t>(clockResult));
        observation.paddingFrames = padding;
        observation.position = position;
        if (!WasapiEndpoint::observe(device, observation)) return fail(DeviceFailure::Timeline);
        const auto available = device->wasapi.actualPeriodSizeInFramesPlayback - padding;
        if (available >= device->wasapi.periodSizeInFramesPlayback)
        {
            BYTE* buffer = nullptr;
            const auto bufferResult = ma_IAudioRenderClient_GetBuffer(renderer, available, &buffer);
            if (bufferResult != S_OK) return fail(DeviceFailure::GetBuffer, static_cast<std::uint32_t>(bufferResult));
            ma_device__read_frames_from_client(device, available, buffer);
            const auto flags = WasapiEndpoint::healthy(device) ? 0u : AUDCLNT_BUFFERFLAGS_SILENT;
            const auto releaseResult = ma_IAudioRenderClient_ReleaseBuffer(renderer, available, flags);
            if (releaseResult != S_OK) return fail(DeviceFailure::ReleaseBuffer, static_cast<std::uint32_t>(releaseResult));
            if (!WasapiEndpoint::healthy(device))
                return WasapiEndpoint::stopping(device) ? MA_SUCCESS : fail(DeviceFailure::Submission);
            if (!WasapiEndpoint::submitted(device, available)) return fail(DeviceFailure::Submission);
        }
        if (!started)
        {
            const auto startResult = ma_IAudioClient_Start(client);
            if (startResult != S_OK) return fail(DeviceFailure::Start, static_cast<std::uint32_t>(startResult));
            c89atomic_exchange_32(&device->wasapi.isStartedPlayback, MA_TRUE);
            started = true;
        }
        const auto waitResult = WaitForSingleObject(device->wasapi.hEventPlayback, MA_WASAPI_WAIT_TIMEOUT_MILLISECONDS);
        if (waitResult != WAIT_OBJECT_0)
            return fail(DeviceFailure::Wait, waitResult == WAIT_FAILED ? GetLastError() : waitResult);
    }
    return MA_SUCCESS;
}
#endif