/**
 * @file llsoundnotecardupload.h
 * @brief Bulk-upload WAV sounds and emit a music-player notecard.
 *
 * $LicenseInfo:firstyear=2024&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2024, Linden Research, Inc.
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

#ifndef LL_LLSOUNDNOTECARDUPLOAD_H
#define LL_LLSOUNDNOTECARDUPLOAD_H

#include <string>
#include <vector>

// Entry point for the "Upload to Notecard..." action.
//
// Takes a list of WAV files (as picked from the bulk file picker), validates
// them as uploadable sounds, confirms the L$ cost with the user, then:
//   1. creates a subfolder inside the system Sounds folder, named from the
//      shared filename prefix,
//   2. uploads every WAV into that subfolder (ordered by the trailing number
//      in each filename),
//   3. once all uploads finish, writes a notecard into the same subfolder
//      whose first line is the modal clip length (seconds, one decimal) and
//      whose following lines are the sound asset UUIDs in sequence.
void start_sound_notecard_upload(const std::vector<std::string>& filenames);

#endif // LL_LLSOUNDNOTECARDUPLOAD_H
