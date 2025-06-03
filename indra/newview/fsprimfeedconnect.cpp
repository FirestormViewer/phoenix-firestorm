/**
* @file fsprimfeedconnect.cpp
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
#include "fsprimfeedconnect.h"
#include "fsprimfeedauth.h"
#include "llviewercontrol.h"
#include "llcoros.h"
#include "llsdjson.h"

// The connector workflow for Primfeed is realtively simple and mostly just builds on top of the established Auth workflow
// and the posting endpoint documented at https://docs.primfeed.com/api/third-party-viewers#creating-a-post

FSPrimfeedConnect::FSPrimfeedConnect() = default;

void FSPrimfeedConnect::uploadPhoto(const LLSD& params, LLImageFormatted* image, post_callback_t callback)
{
    LL_DEBUGS("primfeed") << "uploadPhoto() called" << LL_ENDL;
    if (!FSPrimfeedAuth::isAuthorized())
    {
        LL_WARNS("primfeed") << "Authorization failed, aborting.\n" << LL_ENDL;
        callback(false, "");
        return;
    }
    LL_DEBUGS("primfeed") << "Authorization successful" << LL_ENDL;

    mPostCallback = callback;
    LL_DEBUGS("primfeed") << "Launching upload coroutine" << LL_ENDL;
    LLCoros::instance().launch(
        "FSPrimfeedConnect::uploadPhotoCoro",
        [this, params, image]() { uploadPhotoCoro(params, image); }
    );
}

void FSPrimfeedConnect::uploadPhotoCoro(const LLSD& params, LLImageFormatted* image)
{
    LL_DEBUGS("primfeed") << "Entered uploadPhotoCoro" << LL_ENDL;
    setConnectionState(PRIMFEED_POSTING);
    LL_DEBUGS("primfeed") << "Connection state set to PRIMFEED_POSTING" << LL_ENDL;

    const std::string fmt = (image->getCodec() == EImageCodec::IMG_CODEC_JPEG) ? "jpg" : "png";
    LL_DEBUGS("primfeed") << "Image format: " << fmt << LL_ENDL;

    const std::string boundary = "----------------------------0123456789abcdef";
    const std::string sep      = "\n";
    const std::string dash     = "--" + boundary;

    LL_DEBUGS("primfeed") << "Building multipart body" << LL_ENDL;
    LLCore::BufferArray::ptr_t raw(new LLCore::BufferArray());
    LLCore::BufferArrayStream body(raw.get());
    auto addPart = [&](const std::string& name, const std::string& val)
    {
        LL_DEBUGS("primfeed") << "Adding part: " << name << "=" << val << LL_ENDL;
        body << dash << sep
             << "Content-Disposition: form-data; name=\"" << name << "\"" << sep << sep
             << val << sep;
    };

    addPart("commercial", params["commercial"].asBoolean() ? "true" : "false");
    addPart("rating",     params["rating"].asString());
    addPart("content",    params["content"].asString());
    addPart("publicGallery", params["post_to_public_gallery"].asBoolean()? "true" : "false");

    if (params.has("location") && !params["location"].asString().empty())
    {
        addPart("location", params["location"].asString());
    }

    LL_DEBUGS("primfeed") << "Adding image file header" << LL_ENDL;
    body << dash << sep
         << "Content-Disposition: form-data; name=\"image\"; filename=\"snapshot." << fmt << "\"" << sep
         << "Content-Type: image/" << fmt << sep << sep;

    U8* data = image->getData();
    S32 size = image->getDataSize();
    LL_DEBUGS("primfeed") << "Appending image data, size=" << size << LL_ENDL;
    // yep this seems inefficient, but all other occurrences in the codebase do it this way.
    for (S32 i = 0; i < size; ++i)
    {
        body << data[i];
    }
    body << sep;

    body << dash << "--" << sep;
    LL_DEBUGS("primfeed") << "Multipart body ready" << LL_ENDL;

    // Setup HTTP
    LL_DEBUGS("primfeed") << "Preparing HTTP request" << LL_ENDL;
    LLCore::HttpRequest::policy_t policy = LLCore::HttpRequest::DEFAULT_POLICY_ID;
    LLCoreHttpUtil::HttpCoroutineAdapter adapter("PrimfeedUpload", policy);
    LLCore::HttpRequest::ptr_t request(new LLCore::HttpRequest);
    LLCore::HttpOptions::ptr_t options(new LLCore::HttpOptions);
    options->setWantHeaders(true);

    LL_DEBUGS("primfeed") << "Setting HTTP headers" << LL_ENDL;
    LLCore::HttpHeaders::ptr_t headers(new LLCore::HttpHeaders);
    std::string token  = gSavedPerAccountSettings.getString("FSPrimfeedOAuthToken");
    std::string apiKey = gSavedSettings.getString("FSPrimfeedViewerApiKey");
    headers->append(HTTP_OUT_HEADER_USER_AGENT, FS_PF_USER_AGENT);
    headers->append("Authorization",    "Bearer " + token);
    headers->append("pf-viewer-api-key", apiKey);
    headers->append("Content-Type",     "multipart/form-data; boundary=" + boundary);
    LL_DEBUGS("primfeed") << "Dumping HTTP headers for POST:" << LL_ENDL;
    for (auto it = headers->begin(); it != headers->end(); ++it)
    {
        LL_DEBUGS("primfeed") << it->first << ": " << it->second << LL_ENDL;
    }    
    LL_DEBUGS("primfeed") << "Headers set" << LL_ENDL;

    LL_DEBUGS("primfeed") << "Starting HTTP POST" << LL_ENDL;
    LLSD result = adapter.postRawAndSuspend(request,
        "https://api.primfeed.com/pf/viewer/post",
        raw,
        options,
        headers);
    LL_DEBUGS("primfeed") << "HTTP POST complete" << LL_ENDL;

    const LLSD::Binary &rawData = result[LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS_RAW].asBinary();
    std::string response_raw;
    response_raw.assign(rawData.begin(), rawData.end());
    LLSD result_LLSD;
    if(!response_raw.empty())
    {
        result_LLSD = LlsdFromJson(boost::json::parse(response_raw));
    }    
    LLCore::HttpStatus status = LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(result[LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS]);
    bool success = (status.getType() == HTTP_OK);
    LL_DEBUGS("primfeed") << "HTTP status =" << (success?"OK":"FAIL") << " "<< status.getMessage() << LL_ENDL;

    std::string url;
    if (success)
    {
        url = result_LLSD["url"].asString();
        LL_DEBUGS("primfeed") << "Received URL=" << url << LL_ENDL;
    }

    LL_DEBUGS("primfeed") << "Invoking callback" << LL_ENDL;
    mPostCallback(success, url);
    setConnectionState(success ? PRIMFEED_POSTED : PRIMFEED_POST_FAILED);
    LL_DEBUGS("primfeed") << "Final state set" << LL_ENDL;
}

// Handle connection state transitions
void FSPrimfeedConnect::setConnectionState(EConnectionState state)
{
    LL_DEBUGS("primfeed") << "setConnectionState(" << state << ")" << LL_ENDL;
    mConnectionState = state;
}

FSPrimfeedConnect::EConnectionState FSPrimfeedConnect::getConnectionState() const
{
    return mConnectionState;
}

bool FSPrimfeedConnect::isTransactionOngoing() const
{
    return (mConnectionState == PRIMFEED_CONNECTING ||
            mConnectionState == PRIMFEED_POSTING ||
            mConnectionState == PRIMFEED_DISCONNECTING);
}

void FSPrimfeedConnect::loadPrimfeedInfo()
{
    LL_DEBUGS("primfeed") << "loadPrimfeedInfo() called" << LL_ENDL;
    // Nothing to do here for Primfeed
    setConnectionState(PRIMFEED_CONNECTED);
}