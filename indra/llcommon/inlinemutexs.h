/**
* @file inlinemutexs.h
* @brief Declaration of inline mutexs
* @author minerjr@firestorm
*
 * $LicenseInfo:firstyear=2025&license=fsviewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (C) 2025, Minerjr
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
 * The Phoenix Firestorm Project, Inc., 1831 Oakwood Drive, Fairmont, Minnesota 56031-3225 USA
 * http://www.firestormviewer.org
 * $/LicenseInfo$
*/

#ifndef INLINE_MUTEXS_HEADER
#define INLINE_MUTEXS_HEADER
#include <mutex>
#include <chrono>
#include <atomic>

// Audio device mutex to be shared between audio engine and Voice systems to
// syncronize on when audio hardware accessed for disconnected/connecting hardware
// Uses Timed Mutex so as to not lockup the threads forever.
inline std::timed_mutex gAudioDeviceMutex;
#endif
