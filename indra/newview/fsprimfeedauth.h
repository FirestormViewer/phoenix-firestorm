/**
* @file fsprimfeedauth.h
* @brief Primfeed Authorisation workflow class
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
#ifndef FSPRIMFEEDAUTH_H
#define FSPRIMFEEDAUTH_H

#include "llsd.h"
#include "llviewercontrol.h"
#include <string>
#include <functional>

/*
* Primfeed authentication workflow class.
*
* This class handles the Primfeed OAuth login flow and provides methods to
* check the user status and receive a callback when the authentication
* process is complete.
* based on the workflow documented at https://docs.primfeed.com/api/third-party-viewers
*/
class FSPrimfeedAuth : public std::enable_shared_from_this<FSPrimfeedAuth>
{
public:
    // Callback type: first parameter indicates success and the second holds any LLSD response.
    using authorized_callback_t = std::function<void(bool, const LLSD&)>;
    static std::shared_ptr<FSPrimfeedAuth> create(authorized_callback_t callback);
    static std::unique_ptr<LLEventPump> sPrimfeedAuthPump;
    ~FSPrimfeedAuth();
    
    // Should be called by the chat interceptor when an oauth token is received.
    void onOauthTokenReceived(const std::string_view& oauth_token);
    void onInstantMessage(const LLSD& message);
    void onChatMessage(const LLSD& message);
    
    // Begin the login request flow.
    void beginLoginRequest();
    // Check the user status.
    void checkUserStatus();
    static bool isPendingAuth(){ return (sPrimfeedAuth != nullptr); }
    static bool isAuthorized(){ return (!gSavedPerAccountSettings.getString("FSPrimfeedOAuthToken").empty()); }
    static void initiateAuthRequest();    
    static void resetAuthStatus();
    
private:
    static std::shared_ptr<FSPrimfeedAuth> sPrimfeedAuth;

    explicit FSPrimfeedAuth(authorized_callback_t callback);
    authorized_callback_t mCallback;
    std::string mOauthToken;
    std::string mRequestId;
    
    // Callback when a login request response is received.
    void gotRequestId(bool success, const LLSD &response);
    // Validate the login request.
    void validateRequest();
    // Callback when the validate response is received.
    void gotValidateResponse(bool success, const LLSD &response);
    // Callback when the user status response is received.
    void gotUserStatus(bool success, const LLSD &response);

    boost::signals2::connection mInstantMessageConnection;
    boost::signals2::connection mChatMessageConnection;
    // Static flag to prevent duplicate authentication attempts.
    static std::atomic<bool> sAuthorisationInProgress;

    static constexpr U32 PRIMFEED_CONNECT_TIMEOUT = 300; // 5 minute timeout should work
};

#endif // FSPRIMFEEDAUTH_H