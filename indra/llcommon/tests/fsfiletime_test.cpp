/**
 * @file fsfiletime_test.cpp
 * @brief Check filesystem timestamps against native stat, not only round trips.
 *
 * Copyright (C) 2026, The Phoenix Firestorm Project, Inc.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "linden_common.h"
#include "lltut.h"
#include "llfile.h"
#include "fsfiletime.h"
#include "lluuid.h"

namespace tut
{
struct fsfiletime_data
{
    std::filesystem::path path = std::filesystem::path(LLFile::tmpdir()) /
        std::filesystem::path(std::u8string(u8"filetime-\u30e6\u30cb\u30b3\u30fc\u30c9-"));

    fsfiletime_data()
    {
        LLUUID id;
        id.generate();
        path += id.asString();
        std::ofstream(path).put('x');
    }
    ~fsfiletime_data()
    {
        std::error_code error;
        std::filesystem::remove(path, error);
    }
};
using fsfiletime_group = test_group<fsfiletime_data>;
using fsfiletime_object = fsfiletime_group::object;
fsfiletime_group fsfiletime_tests("fsfiletime");

template<> template<>
void fsfiletime_object::test<1>()
{
    // Native stat exposes Unix time; this catches the wrong clock epoch and
    // UTC-vs-system leap-second offsets even if both helpers made the same error.
    const std::time_t expected = 1700000123;
    std::filesystem::last_write_time(path, fs_file_time_from_time_t(expected));
    llstat status{};
    ensure_equals("native stat on a Unicode path", LLFile::stat(path, &status), 0);
    ensure_equals("written timestamp matches Unix time", status.st_mtime, expected);
    ensure_equals("read timestamp matches native stat",
                  fs_file_time_to_time_t(std::filesystem::last_write_time(path)), status.st_mtime);
}

template<> template<>
void fsfiletime_object::test<2>()
{
    const auto original = std::filesystem::last_write_time(path);
    llstat status{};
    ensure_equals("native timestamp read", LLFile::stat(path, &status), 0);
    ensure_equals("existing timestamp uses the correct epoch", fs_file_time_to_time_t(original), status.st_mtime);
    ensure_equals("whole seconds are preserved", fs_file_time_to_time_t(fs_file_time_from_time_t(status.st_mtime)), status.st_mtime);
}

template<> template<>
void fsfiletime_object::test<3>()
{
    for (const std::time_t time : {std::time_t(0), std::time_t(946684800), std::time_t(1700000123)})
        ensure_equals("Unix timestamp round trip", fs_file_time_to_time_t(fs_file_time_from_time_t(time)), time);
}
}
