/**
* @file fsprimfeedconect.h
* @brief Primfeed connector class
* @author beq@firestorm
*
 * $LicenseInfo:firstyear=2025&license=fsviewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (C) 2025, Beq Janus
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
#ifndef FS_PRIMFEEDCONNECT_H
#define FS_PRIMFEEDCONNECT_H

#include "llsingleton.h"
#include "llsd.h"
#include "llimage.h"
#include "fsprimfeedauth.h"
#include "llcorehttputil.h"
#include "bufferarray.h"
#include "llcoros.h"
#include "llviewercontrol.h" // for gSavedSettings/gSavedPerAccountSettings
#include <functional>

// Coro based connector designed to interface with floater designed along the same principles as LLFloaterFlickr.cpp

class FSPrimfeedConnect : public LLSingleton<FSPrimfeedConnect>
{
    LLSINGLETON(FSPrimfeedConnect);
public:
    // Connection states for Primfeed operations
    enum EConnectionState
    {
        PRIMFEED_DISCONNECTED = 0,
        PRIMFEED_CONNECTING,
        PRIMFEED_CONNECTED,
        PRIMFEED_POSTING,
        PRIMFEED_POSTED,
        PRIMFEED_POST_FAILED,
        PRIMFEED_DISCONNECTING
    };

    // Callback invoked on post completion: success flag and URL (empty on failure)
    using post_callback_t = std::function<void(bool success, const std::string& url)>;

    // Posts a snapshot to Primfeed; requires FSPrimfeedAuth::isAuthorized()
    void uploadPhoto(const LLSD& params, LLImageFormatted* image, post_callback_t callback);

    // Retrieve and update account info from Primfeed (not used kept for compatibility)
    void loadPrimfeedInfo();

    void setConnectionState(EConnectionState state);
    EConnectionState getConnectionState() const;
    bool isTransactionOngoing() const;

private:
    // Internal coroutine entry-point for uploads
    void uploadPhotoCoro(const LLSD& params, LLImageFormatted* image);

    // Cached callback until coroutine completes
    post_callback_t mPostCallback;

    // Current connection/post state
    EConnectionState mConnectionState = PRIMFEED_DISCONNECTED;
};
#endif // FS_PRIMFEEDCONNECT_H