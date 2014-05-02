/**
 * @file exoflickrauth.h
 * @brief Handles all Flickr authentication issues
 *
 * $LicenseInfo:firstyear=2012&license=viewerlgpl$
 * Copyright (C) 2012 Katharine Berry
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * $/LicenseInfo$
 */

#include <boost/bind.hpp>

// This class's function is described by the flow chart at
// http://www.flickr.com/services/api/auth.oauth.html
class exoFlickrAuth
{
public:
	typedef boost::function<void(bool success, const LLSD& params)> authorized_callback_t;

	exoFlickrAuth(authorized_callback_t callback);
	~exoFlickrAuth();

private:
	authorized_callback_t mCallback;
	bool mAuthenticating;
	static bool sAuthorisationInProgress;

	void beginAuthorisation();
	void checkAuthorisation();
	void checkResult(bool success, const LLSD& response);
	void explanationCallback(const LLSD& notification, const LLSD& response);

	void gotRequestToken(bool success, const LLSD& params); // Stage 1 response (HTTP)
	void gotVerifier(const LLSD& notification, const LLSD& response); // Stage 2 response (LLNotification)
	void gotAccessToken(bool success, const LLSD& params); // Stage 3 response (HTTP)
};
