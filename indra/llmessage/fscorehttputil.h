/**
 * @file fscorehttputil.h
 * @brief Core HTTP utility classes.
 *
 * $LicenseInfo:firstyear=2015&license=viewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (c) 2015 Nicky Dasmijn
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

#ifndef FS_COREHTTPUTIL_H
#define FS_COREHTTPUTIL_H

#include "llcorehttputil.h"

namespace FSCoreHttpUtil
{
    typedef boost::function<void(const LLSD &)> completionCallback_t;

	void callbackHttpPostRaw(const std::string &url, std::string postData, completionCallback_t success = NULL, completionCallback_t failure = NULL,
							 LLCore::HttpHeaders::ptr_t aHeader = LLCore::HttpHeaders::ptr_t() );
    void callbackHttpGetRaw(const std::string &url, completionCallback_t success = NULL, completionCallback_t failure = NULL,
							LLCore::HttpHeaders::ptr_t aHeader = LLCore::HttpHeaders::ptr_t() );

	void trivialGetCoroRaw(std::string url, LLCore::HttpRequest::policy_t policyId, LLCore::HttpHeaders::ptr_t aHeader, completionCallback_t success, completionCallback_t failure);
    void trivialPostCoroRaw(std::string url, LLCore::HttpRequest::policy_t policyId, LLCore::BufferArray::ptr_t postData, LLCore::HttpHeaders::ptr_t aHeader,
							completionCallback_t success, completionCallback_t failure);
}

#endif // FS_COREHTTPUTIL_H
