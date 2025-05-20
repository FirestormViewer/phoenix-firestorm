/**
 * @file fsprimfeedauth.cpp
 * @file fsprimfeedauth.h
 * @brief Primfeed Authorisation workflow class
 * @author beq@firestorm
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

/* 
 * Handles Primfeed authentication and authorisation through a multi-factor OAuth flow.
 *
 * This module integrates with Primfeed’s Third Party Viewers API.
 * The authentication flow is as follows:
 *   1. Initiate a login request:
 *      POST https://api.primfeed.com/pf/viewer/create-login-request
 *      Headers:
 *         pf-viewer-api-key: <viewer_api_key>
 *         pf-user-uuid: <avatar_uuid>
 *      Response:
 *         { "requestId": "<64-char string>" }
 *
 *   2. Redirect the user to:
 *      https://www.primfeed.com/oauth/viewer?r=<requestId>&v=<viewer_api_key>
 *
 *   3. The user is shown an approval screen. When they click Authorize,
 *      an in-world message is sent:
 *         #PRIMFEED_OAUTH: <oauth_token>
 *      We intercept this code through an onChat handle then call onOauthTokenReceived().
 *
 *   4. Validate the login request:
 *      POST https://api.primfeed.com/pf/viewer/validate-request
 *      Headers:
 *         Authorization: Bearer <oauth_token>
 *         pf-viewer-api-key: <viewer_api_key>
 *         pf-viewer-request-id: <requestId>
 *      Response: HTTP 204
 *
 *   5. Optionally, check user status:
 *      GET https://api.primfeed.com/pf/viewer/user
 *      Headers:
 *         Authorization: Bearer <oauth_token>
 *         pf-viewer-api-key: <viewer_api_key>
 *      Response: { "plan": "free" } (or "pro")
 */
#include "llviewerprecompiledheaders.h"
#include "fsprimfeedauth.h"
#include "fsprimfeedconnect.h"
#include "llimview.h"
#include "llnotificationsutil.h"
#include "llfloaterimnearbychathandler.h"
#include "llnotificationmanager.h"
#include "llagent.h"
#include "llevents.h"
#include "fscorehttputil.h"
#include "llwindow.h"
#include "llviewerwindow.h"
#include "lluri.h"
#include "llsdjson.h"
#include <string_view>

using Callback = FSPrimfeedAuth::authorized_callback_t;

// private instance variable
std::shared_ptr<FSPrimfeedAuth> FSPrimfeedAuth::sPrimfeedAuth;
std::unique_ptr<LLEventPump> FSPrimfeedAuth::sPrimfeedAuthPump = std::make_unique<LLEventStream>("PrimfeedAuthResponse");

// Helper callback that unpacks HTTP POST response data.
void FSPrimfeedAuthResponse(LLSD const &aData, Callback callback)
{
    LLSD header = aData[LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS][LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS_HEADERS];
    LLCore::HttpStatus status = LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(
        aData[LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS]);

    const LLSD::Binary &rawData = aData[LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS_RAW].asBinary();
    std::string result;
    result.assign(rawData.begin(), rawData.end());

    // Assume JSON response.

    LLSD resultLLSD;
    if(!result.empty())
    {
        resultLLSD = LlsdFromJson(boost::json::parse(result));
    }
    callback((status.getType() == HTTP_OK ||
            status.getType() == HTTP_NO_CONTENT), resultLLSD);
}

void FSPrimfeedAuth::initiateAuthRequest()
{
    // This function is called to initiate the authentication request.
    // It should be called when the user clicks the "Authenticate" button.
    // Also triggered on opening the floater.
    // The actual implementation is in the create() method.
    
    if (!isAuthorized())
    {
        if (sPrimfeedAuth)
        {
            LLNotificationsUtil::add("PrimfeedAuthorizationAlreadyInProgress");
            return;
        }
        // If no token stored, begin the login request; otherwise check user status.
        sPrimfeedAuth = FSPrimfeedAuth::create(
            [](bool success, const LLSD &response) 
            {
                LLSD event_data = response;
                event_data["success"] = success;
                sPrimfeedAuthPump->post(event_data);
                // Now that auth is complete, clear the static pointer.
                sPrimfeedAuth.reset();
            }
        );
    }
    else
    {
        LLNotificationsUtil::add("PrimfeedAlreadyAuthorized");
    }
}

void FSPrimfeedAuth::resetAuthStatus()
{
    sPrimfeedAuth.reset();
    gSavedPerAccountSettings.setString("FSPrimfeedOAuthToken", "");
    gSavedPerAccountSettings.setString("FSPrimfeedProfileLink", "");
    gSavedPerAccountSettings.setString("FSPrimfeedPlan", "");
    gSavedPerAccountSettings.setString("FSPrimfeedUsername", "");
    LLSD event_data;
    event_data["status"] = "reset";
    event_data["success"] = "false";
    sPrimfeedAuthPump->post(event_data);
}


FSPrimfeedAuth::FSPrimfeedAuth(authorized_callback_t callback)
    : mCallback(callback), mAuthenticating(false)
{
    mInstantMessageConnection = LLIMModel::instance().addNewMsgCallback(
        [this](const LLSD &message) {
            LL_DEBUGS("FSPrimfeedAuth") << "Received chat message: " << message["message"].asString() << LL_ENDL;
            this->onChatMessage(message);
        });
    mChatMessageConnection = LLNotificationsUI::LLNotificationManager::instance().getChatHandler()->addNewChatCallback(
        [this](const LLSD &message) {
            LL_DEBUGS("FSPrimfeedAuth") << "Received instant message: " << message["message"].asString() << LL_ENDL;
            this->onChatMessage(message);
        });
}

FSPrimfeedAuth::~FSPrimfeedAuth()
{
    if (mChatMessageConnection.connected())
    {
        try
        {
            mChatMessageConnection.disconnect();
        }
        catch (const std::exception& e)
        {
            LL_WARNS("FSPrimfeedAuth") << "Exception during chat connection disconnect: " << e.what() << LL_ENDL;
        }
        catch (...)
        {
            LL_WARNS("FSPrimfeedAuth") << "Unknown exception during chat connection disconnect." << LL_ENDL;
        }
    }     
    if (mInstantMessageConnection.connected())
    {
        try
        {
            mInstantMessageConnection.disconnect();
        }
        catch (const std::exception& e)
        {
            LL_WARNS("FSPrimfeedAuth") << "Exception during instant message disconnect: " << e.what() << LL_ENDL;
        }
        catch (...)
        {
            LL_WARNS("FSPrimfeedAuth") << "Unknown exception during instant message disconnect." << LL_ENDL;
        }
    }     
}

// Factory method to create a shared pointer to FSPrimfeedAuth.
std::shared_ptr<FSPrimfeedAuth> FSPrimfeedAuth::create(authorized_callback_t callback)
{
    // Ensure only one authentication attempt is in progress.
    if (sPrimfeedAuth)
    {
        // Already in progress; return the existing instance.
        return sPrimfeedAuth;
    }
    auto auth = std::shared_ptr<FSPrimfeedAuth>(new FSPrimfeedAuth(callback));
    if(!auth)
    {
        return nullptr;
    }

    auth->mAuthenticating = true;

    // If no token stored, begin the login request; otherwise check user status.
    if (gSavedPerAccountSettings.getString("FSPrimfeedOAuthToken").empty())
    {
        auth->beginLoginRequest();
    }
    else
    {
        auth->checkUserStatus();
    }
    return auth;
}

void FSPrimfeedAuth::beginLoginRequest()
{
    // Get our API key and user UUID.
    std::string viewer_api_key = gSavedSettings.getString("FSPrimfeedViewerApiKey");
    std::string user_uuid = gAgent.getID().asString();

    std::string url = "https://api.primfeed.com/pf/viewer/create-login-request";
    std::string post_data = ""; // No body parameters required.

    // Create the headers object.
    LLCore::HttpHeaders::ptr_t pHeader(new LLCore::HttpHeaders());
    LLCore::HttpOptions::ptr_t options(new LLCore::HttpOptions());

    pHeader->append("pf-viewer-api-key", viewer_api_key);
    pHeader->append("pf-user-uuid", user_uuid);

    // Set up HTTP options
    options->setWantHeaders(true);
    options->setRetries(0);
    options->setTimeout(PRIMFEED_CONNECT_TIMEOUT);

    // Capture shared_ptr to self
    auto self = shared_from_this();

    const auto end(pHeader->end());
    for (auto it(pHeader->begin()); end != it; ++it)
    {
        LL_DEBUGS("Primfeed") << "Header: " << it->first << " = " << it->second << LL_ENDL;
    }

    // Pass both success and failure callbacks
    FSCoreHttpUtil::callbackHttpPostRaw(
        url, 
        post_data, 
        [self](LLSD const &aData) {
            LL_DEBUGS("FSPrimfeedAuth") << "Login request response(OK): " << aData << LL_ENDL;
            FSPrimfeedAuthResponse(aData,
                [self](bool success, const LLSD &response) {
                    self->gotRequestId(success, response);
                }
            );
        },
        [self](LLSD const &aData) {
            LL_DEBUGS("FSPrimfeedAuth") << "Login request response(FAIL): " << aData << LL_ENDL;
            FSPrimfeedAuthResponse(aData,
                [self](bool success, const LLSD &response) {
                    self->gotRequestId(success, response);
                }
            );
        },
        pHeader, 
        options
    );
}

void FSPrimfeedAuth::gotRequestId(bool success, const LLSD &response)
{
    if (!success)
    {
        LLNotificationsUtil::add("PrimfeedLoginRequestFailed");
        mCallback(false, LLSD());
        return;
    }
    mRequestId = response["requestId"].asString();
    if (mRequestId.empty())
    {
        LLNotificationsUtil::add("PrimfeedLoginRequestFailed");
        mCallback(false, LLSD());
        return;
    }
    // Open the browser for user approval.
    std::string viewer_api_key = gSavedSettings.getString("FSPrimfeedViewerApiKey");
    std::string auth_url = "https://www.primfeed.com/oauth/viewer?r=" + mRequestId + "&v=" + viewer_api_key;
    gViewerWindow->getWindow()->spawnWebBrowser(auth_url, true);
    
}

/// This function is called by the chat interceptor when the message
/// "#PRIMFEED_OAUTH: <oauth_token>" is intercepted.
void FSPrimfeedAuth::onOauthTokenReceived(const std::string_view& oauth_token)
{
    if (oauth_token.empty())
    {
        mCallback(false, LLSD());
        return;
    }
    mOauthToken = oauth_token;
    validateRequest();
}

void FSPrimfeedAuth::onChatMessage(const LLSD& message)
{
    constexpr std::string_view oauth_msg_prefix = "#PRIMFEED_OAUTH: ";
    const std::string msg = message["message"].asString();
    if (msg.find(std::string(oauth_msg_prefix)) == 0)
    {
        std::string_view oauth_token(msg.data() + oauth_msg_prefix.size(), msg.size() - oauth_msg_prefix.size());
        LL_DEBUGS("Primfeed") << "Received OAuth token: " << msg << "extracted:<" << oauth_token << ">" << LL_ENDL;
        onOauthTokenReceived(oauth_token);
    }
}


void FSPrimfeedAuth::validateRequest()
{
    // No POST body needed.
    std::string post_data = "";
    std::string url = "https://api.primfeed.com/pf/viewer/validate-request";

    // Retrieve the viewer API key.
    std::string viewer_api_key = gSavedSettings.getString("FSPrimfeedViewerApiKey");

    // Create and populate the headers.
    LLCore::HttpHeaders::ptr_t pHeader(new LLCore::HttpHeaders());
    pHeader->append("Authorization", "Bearer " + mOauthToken);
    pHeader->append("pf-viewer-api-key", viewer_api_key);
    pHeader->append("pf-viewer-request-id", mRequestId);

    // Set HTTP options
    LLCore::HttpOptions::ptr_t options(new LLCore::HttpOptions());
    options->setWantHeaders(true);
    options->setRetries(0);
    options->setTimeout(PRIMFEED_CONNECT_TIMEOUT); 

    // print out pHeader for debuging using iterating over pHeader and using LL_DEBUGS
    const auto end(pHeader->end());
    for (auto it(pHeader->begin()); end != it; ++it)
    {
        LL_DEBUGS("Primfeed") << "Header: " << it->first << " = " << it->second << LL_ENDL;
    }
    
    auto self = shared_from_this();
    try
    {
        FSCoreHttpUtil::callbackHttpPostRaw(
            url,
            post_data,
            [self](LLSD const &aData) {
                LL_DEBUGS("FSPrimfeedAuth") << "Validation-request response(OK): " << aData << LL_ENDL;
                FSPrimfeedAuthResponse(aData,
                    [self](bool success, const LLSD &response) {
                        self->gotValidateResponse(success, response);
                    }
                );
            },
            [self](LLSD const &aData) {
                LL_INFOS("FSPrimfeedAuth") << "Validation-request response(FAIL): " << aData << LL_ENDL;
                FSPrimfeedAuthResponse(aData,
                    [self](bool success, const LLSD &response) {
                        self->gotValidateResponse(success, response);
                    }
                );
            },
            pHeader,
            options
        );
    }
    catch(const std::exception& e)
    {
        LL_WARNS("Primfeed") << "Primfeed validation failed " << e.what() << LL_ENDL;
    }
    
}


void FSPrimfeedAuth::gotValidateResponse(bool success, const LLSD &response)
{
    if (!success)
    {
        LLNotificationsUtil::add("PrimfeedValidateFailed");
        mCallback(false, response);
        return;
    }
    checkUserStatus();
}

void FSPrimfeedAuth::checkUserStatus()
{
    std::string viewer_api_key = gSavedSettings.getString("FSPrimfeedViewerApiKey");

    // Build the base URL without query parameters.
    std::string url = "https://api.primfeed.com/pf/viewer/user";
    LL_DEBUGS("Primfeed") << "URL: " << url << LL_ENDL;

    // Create and populate the headers.
    LLCore::HttpHeaders::ptr_t pHeader(new LLCore::HttpHeaders());
    pHeader->append("Authorization", "Bearer " + mOauthToken);
    pHeader->append("pf-viewer-api-key", viewer_api_key);

    // Set HTTP options.
    LLCore::HttpOptions::ptr_t options(new LLCore::HttpOptions());
    options->setWantHeaders(true);
    options->setRetries(0);
    options->setTimeout(PRIMFEED_CONNECT_TIMEOUT);

    // Make the HTTP GET request, passing in the headers and options.
    FSCoreHttpUtil::callbackHttpGetRaw(
        url,
        [this](LLSD const &aData) {
            LL_DEBUGS("FSPrimfeedAuth") << "Check-user-status response: " << aData << LL_ENDL;
            FSPrimfeedAuthResponse(aData, [this](bool success, const LLSD &response) {
                this->gotUserStatus(success, response);
            });
        },
        [this](LLSD const &aData) {
            LL_INFOS("FSPrimfeedAuth") << "Check-user-status response (failure): " << aData << LL_ENDL;
            // Optionally, call the same processing for failure or handle separately.
            FSPrimfeedAuthResponse(aData, [this](bool success, const LLSD &response){
                this->gotUserStatus(success, response);
            });
        },
        pHeader,
        options
    );
}


void FSPrimfeedAuth::gotUserStatus(bool success, const LLSD &response)
{
    LL_INFOS("Primfeed") << "User status: " << response << "(" << success << ")" << LL_ENDL;
    if (success && response.has("plan"))
    {
        gSavedPerAccountSettings.setString("FSPrimfeedOAuthToken", mOauthToken);
        gSavedPerAccountSettings.setString("FSPrimfeedRequestId", mRequestId);
        gSavedPerAccountSettings.setString("FSPrimfeedPlan", response["plan"].asString());
        gSavedPerAccountSettings.setString("FSPrimfeedProfileLink", response["link"].asString());
        gSavedPerAccountSettings.setString("FSPrimfeedUsername", response["username"].asString());
        FSPrimfeedConnect::instance().setConnectionState(FSPrimfeedConnect::PRIMFEED_CONNECTED);
        mCallback(true, response);
    }
    else
    {
        LLNotificationsUtil::add("PrimfeedUserStatusFailed");
        FSPrimfeedConnect::instance().setConnectionState(FSPrimfeedConnect::PRIMFEED_DISCONNECTED);
        mCallback(false, response);
    }
}
