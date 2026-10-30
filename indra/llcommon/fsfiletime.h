/**
 * @file fsfiletime.h
 * @brief Exact conversion between filesystem timestamps and Unix time.
 *
 * Copyright (C) 2026, The Phoenix Firestorm Project, Inc.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

// libc++ has file_clock::to_sys/from_sys but lacks clock_cast.
// MSVC instead implements file_clock through to_utc/from_utc, so retain its
// generic clock conversion. Never estimate the epoch using two calls to now().
#include <chrono>
#include <ctime>
#include <filesystem>

inline std::time_t fs_file_time_to_time_t(std::filesystem::file_time_type time)
{
#ifdef LL_MSVC
    const auto system_time = std::chrono::clock_cast<std::chrono::system_clock>(time);
#else
    const auto system_time = std::chrono::file_clock::to_sys(time);
#endif
    return std::chrono::system_clock::to_time_t(
        std::chrono::time_point_cast<std::chrono::system_clock::duration>(system_time));
}

inline std::filesystem::file_time_type fs_file_time_from_time_t(std::time_t time)
{
    const auto system_time = std::chrono::system_clock::from_time_t(time);
#ifdef LL_MSVC
    return std::chrono::clock_cast<std::chrono::file_clock>(system_time);
#else
    return std::chrono::file_clock::from_sys(system_time);
#endif
}
