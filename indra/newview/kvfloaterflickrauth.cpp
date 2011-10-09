/** 
 * @file kvfloaterflickrauth.cpp
 * @brief Flickr authentication floater
 * @copyright Copyright (c) 2011 Katharine Berry
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "llviewerprecompiledheaders.h"

#include "llfloaterreg.h"
#include "llsd.h"
#include "llslurl.h"
#include "lluri.h"
#include "llurlaction.h"
#include "llviewercontrol.h"

#include "kvflickr.h"
#include "kvfloaterflickrauth.h"

KVFloaterFlickrAuth::KVFloaterFlickrAuth(const LLSD& key) :
	LLFloater(key), mCallback(NULL), mBrowser(NULL)
{
}

BOOL KVFloaterFlickrAuth::postBuild()
{
	mBrowser = getChild<LLMediaCtrl>("browser");
	mBrowser->addObserver(this);

	// Work out URL.
	LLSD query;
	query["api_key"] = std::string(KV_FLICKR_API_KEY);
	query["perms"] = "write";
	query["api_sig"] = KVFlickrRequest::getSignatureForCall(query, true);
	std::string query_string = LLURI::mapToQueryString(query);
	LL_INFOS("FlickrAPI") << "Auth query: " << query_string << LL_ENDL;
	std::string full_url = "http://www.flickr.com/services/auth/" + query_string;
	mBrowser->navigateTo(full_url, "text/html");

	return true;
}

void KVFloaterFlickrAuth::onClose(bool app_quitting)
{
	// If we still have an mCallback here, we know it wasn't successful,
	// because we always set it to NULL after using it.
	if(mCallback)
	{
		mCallback(false);
		mCallback = NULL;
	}
	destroy(); // Die die die!
}

void KVFloaterFlickrAuth::handleMediaEvent(LLPluginClassMedia* media, EMediaEvent event)
{
	if(event == MEDIA_EVENT_LOCATION_CHANGED)
	{
		std::string uri_string = media->getLocation();
		LLURI uri(uri_string);
		// We use this moronic data: hack because the internal browser crashes on
		// secondlife:/// redirects, doesn't raise any events on nonexistent links,
		// and gets confused by about:blank. At least this is prettier.
		if(uri.scheme() == "data")
		{
			// Turns out we have to parse query string out ourselves because LLURI won't do it
			// unless it's a http(s), ftp, secondlife or x-grid-location-info link.
			std::string::size_type q = uri_string.find('?');
			if(q != std::string::npos)
			{
				std::string query_string = uri_string.substr(q + 1);
				LLSD query = LLURI::queryMap(query_string);
				if(query.has("frob"))
				{
					std::string frob = query["frob"];
					LLSD params;
					params["frob"] = frob;
					KVFlickrRequest::request("flickr.auth.getToken", params, boost::bind(&KVFloaterFlickrAuth::gotToken, this, _1, _2));
				}
			}
		}
		// We don't get anything if authentication is rejected; they're just redirected to the
		// home page. This is mildly problematic, given the restricted view they're in.
		// Therefore, if they click outside where we want them to be, we close the view.
		// If they go to the homepage (because they clicked "Home", the logo, or (most importantly)
		// the "Do not allow" button), it is noted that they refused permission. Otherwise,
		// we open the link they clicked in a standard browser. In either case we close
		// our floater.
		else if(uri.hostName() == "www.flickr.com" && uri.path() != "/services/auth/")
		{
			LL_WARNS("FlickrAPI") << "API permission not granted." << LL_ENDL;
			if(uri.path() != "/")
			{
				LLUrlAction::openURL(uri_string);
			}
			closeFloater(false);
		}
	}
}

void KVFloaterFlickrAuth::gotToken(bool success, const LLSD& response)
{
	std::string token = response["auth"]["token"]["_content"];
	std::string username = response["auth"]["user"]["username"];
	std::string nsid = response["auth"]["user"]["nsid"];
	LL_INFOS("FlickrAPI") << "Got token " << token << " for user " << username << " (" << nsid << ")." << LL_ENDL;
	gSavedPerAccountSettings.setString("KittyFlickrToken", token);
	gSavedPerAccountSettings.setString("KittyFlickrUsername", username);
	gSavedPerAccountSettings.setString("KittyFlickrNSID", nsid);
	if(mCallback)
	{
		mCallback((token != ""));
		mCallback = NULL;
	}
	closeFloater(false);
}

//static
KVFloaterFlickrAuth* KVFloaterFlickrAuth::showFloater()
{
	return KVFloaterFlickrAuth::showFloater(NULL);
}

//static
KVFloaterFlickrAuth* KVFloaterFlickrAuth::showFloater(auth_callback_t callback)
{
	KVFloaterFlickrAuth *floater = dynamic_cast<KVFloaterFlickrAuth*>(LLFloaterReg::getInstance("flickr_auth"));
	if(floater)
	{
		floater->mCallback = callback;
		floater->setVisible(true);
		floater->setFrontmost(true);
		floater->center();
		return floater;
	}
	else
	{
		LL_WARNS("FlickrAPI") << "Can't find flickr auth!" << LL_ENDL;
		return NULL;
	}
}
