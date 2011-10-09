/** 
 * @file kvfloaterflickrauth.h
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

#ifndef KV_KVFLOATERFLICKRAUTH_H
#define KV_KVFLOATERFLICKRAUTH_H

#include "llfloater.h"
#include "llmediactrl.h"

#include <boost/bind.hpp>

class KVFloaterFlickrAuth : 
public LLFloater, 
public LLViewerMediaObserver
{
public:
	typedef boost::function<void(bool)> auth_callback_t;

	KVFloaterFlickrAuth(const LLSD& key);

	/*virtual*/ BOOL postBuild();
	/*virtual*/ void onClose(bool app_quitting);

	// inherited from LLViewerMediaObserver
	/*virtual*/ void handleMediaEvent(LLPluginClassMedia* media, EMediaEvent event);

	static KVFloaterFlickrAuth* showFloater();
	static KVFloaterFlickrAuth* showFloater(auth_callback_t callback);

private:
	void gotToken(bool success, const LLSD& response);
	LLMediaCtrl* mBrowser;
	auth_callback_t mCallback;
};

#endif  // KV_KVFLOATERFLICKRAUTH_H
