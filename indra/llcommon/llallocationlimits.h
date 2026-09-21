#ifndef LL_LLALLOCATIONLIMITS_H
#define LL_LLALLOCATIONLIMITS_H

#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace LLAllocationLimits
{
// These arrays are also indexed by signed 32-bit viewer counters.
template<class T>
std::size_t arrayCount(std::uint64_t count)
{
    if (count > static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max()) ||
        count > static_cast<std::uint64_t>(std::numeric_limits<std::ptrdiff_t>::max()) / sizeof(T))
    {
        throw std::length_error("Array size exceeds viewer indexing limits");
    }
    return static_cast<std::size_t>(count);
}

inline std::int32_t squareCount(std::uint64_t edge)
{
    constexpr auto limit = static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max());
    if (edge == 0 || edge > limit / edge)
    {
        throw std::length_error("Squared size exceeds viewer indexing limits");
    }
    return static_cast<std::int32_t>(edge * edge);
}

// Preserve nearest-power-of-two rounding, with midpoint ties rounded upward.
inline std::int32_t nearestPowerOfTwo(std::int32_t requested)
{
    if (requested <= 0)
    {
        throw std::invalid_argument("Table size must be positive");
    }
    const auto value = static_cast<std::uint32_t>(requested);
    std::uint32_t lower = 1;
    while (lower <= value / 2)
    {
        lower *= 2;
    }
    const auto rounded = value - lower >= (lower + 1) / 2 ? lower * 2 : lower;
    if (rounded > static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max()))
    {
        throw std::length_error("Rounded table size exceeds viewer indexing limits");
    }
    return static_cast<std::int32_t>(rounded);
}

inline std::size_t queueCapacity(std::size_t capacity, std::size_t maximum)
{
    if (capacity < 2 || capacity > maximum || (capacity & (capacity - 1)) != 0)
    {
        throw std::invalid_argument("Queue capacity must be a supported power of two");
    }
    return capacity;
}
}

#endif
