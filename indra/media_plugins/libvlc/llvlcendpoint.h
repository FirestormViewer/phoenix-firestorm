#ifndef LL_LLVLCENDPOINT_H
#define LL_LLVLCENDPOINT_H

#include "llvlcaudio.h"
#include <limits>

namespace llvlc
{
class EndpointTimeline
{
public:
    Result observe(const EndpointObservation& next, std::uint64_t submitted) noexcept
    {
        if (failed) return Result::DeviceError;
        if (!next.valid || !next.frequency || next.paddingFrames > submitted ||
            submitted < lastSubmitted || (initialized &&
            (next.position < position || next.frequency != frequency || next.latency100ns != latency)))
            return fail();
        const auto released = submitted - next.paddingFrames;
        if (released < lastReleased) return fail();
        if (!initialized)
        {
            if (next.latency100ns > (std::numeric_limits<std::uint64_t>::max() - 9999999) / next.frequency)
                return fail();
            guard = (next.latency100ns * next.frequency + 9999999) / 10000000;
            frequency = next.frequency;
            latency = next.latency100ns;
            initialized = true;
        }
        position = next.position;
        lastSubmitted = submitted;
        lastReleased = released;
        if (pending && position - observedAt >= guard)
        {
            consumed = boundary;
            pending = false;
        }
        if (!pending && released > consumed)
        {
            boundary = released;
            observedAt = position;
            pending = true;
            if (!guard)
            {
                consumed = boundary;
                pending = false;
            }
        }
        return Result::Ok;
    }

    std::uint64_t consumed = 0;

private:
    Result fail() noexcept { failed = true; return Result::DeviceError; }
    std::uint64_t lastSubmitted = 0;
    std::uint64_t lastReleased = 0;
    std::uint64_t position = 0;
    std::uint64_t frequency = 0;
    std::uint64_t latency = 0;
    std::uint64_t guard = 0;
    std::uint64_t boundary = 0;
    std::uint64_t observedAt = 0;
    bool initialized = false;
    bool pending = false;
    bool failed = false;
};
}
#endif