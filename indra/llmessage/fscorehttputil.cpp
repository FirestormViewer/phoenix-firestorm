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

#include "fscorehttputil.h"

namespace FSCoreHttpUtil
{
	void trivialPostCoroRaw(std::string url, LLCore::HttpRequest::policy_t policyId, LLCore::BufferArray::ptr_t postData, LLCore::HttpHeaders::ptr_t aHeader, completionCallback_t success, completionCallback_t failure)
	{
		LLCoreHttpUtil::HttpCoroutineAdapter::ptr_t httpAdapter(new LLCoreHttpUtil::HttpCoroutineAdapter("genericPostCoroRaw", policyId));
		LLCore::HttpRequest::ptr_t httpRequest(new LLCore::HttpRequest);
		LLCore::HttpOptions::ptr_t httpOpts(new LLCore::HttpOptions);

		httpOpts->setWantHeaders(true);

		LL_INFOS("HttpCoroutineAdapter", "genericPostCoroRaw") << "Generic POST for " << url << LL_ENDL;
	
		LLSD result = httpAdapter->postRawAndSuspend(httpRequest, url, postData, httpOpts, aHeader );

		LLSD httpResults = result[LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS];
		LLCore::HttpStatus status = LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(httpResults);

		if (!status)
		{
			// If a failure routine is provided do it.
			if (failure)
			{
				failure(httpResults);
			}
		}
		else
		{
			// If a success routine is provided do it.
			if (success)
			{
				success(result);
			}
		}
	}

	void callbackHttpPostRaw(const std::string &url, std::string postData, completionCallback_t success, completionCallback_t failure, LLCore::HttpHeaders::ptr_t aHeader )
	{

		LLCore::BufferArray::ptr_t postDataBuffer( new LLCore::BufferArray() );
		postDataBuffer->append( postData.c_str(), postData.size() );
	
		LLCoros::instance().launch("HttpCoroutineAdapter::genericPostCoroRaw",
								   boost::bind(trivialPostCoroRaw, url, LLCore::HttpRequest::DEFAULT_POLICY_ID, postDataBuffer, aHeader, success, failure));
	}

	void trivialGetCoroRaw(std::string url, LLCore::HttpRequest::policy_t policyId, LLCore::HttpHeaders::ptr_t aHeader, completionCallback_t success, completionCallback_t failure)
	{
		LLCoreHttpUtil::HttpCoroutineAdapter::ptr_t
			httpAdapter(new LLCoreHttpUtil::HttpCoroutineAdapter("genericGetCoro", policyId));
		LLCore::HttpRequest::ptr_t httpRequest(new LLCore::HttpRequest);
		LLCore::HttpOptions::ptr_t httpOpts(new LLCore::HttpOptions);

		httpOpts->setWantHeaders(true);

		LL_INFOS("HttpCoroutineAdapter", "genericGetCoroRaw") << "Generic GET for " << url << LL_ENDL;

		LLSD result = httpAdapter->getRawAndSuspend(httpRequest, url, httpOpts, aHeader );

		LLSD httpResults = result[LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS];
		LLCore::HttpStatus status = LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(httpResults);

		if (!status)
		{   
			if (failure)
			{
				failure(httpResults);
			}
		}
		else
		{
			if (success)
			{
				success(result);
			}
		}
	}

	void callbackHttpGetRaw(const std::string &url, completionCallback_t success, completionCallback_t failure, LLCore::HttpHeaders::ptr_t aHeader )
	{
		LLCoros::instance().launch("HttpCoroutineAdapter::genericGetCoroRaw",
								   boost::bind(trivialGetCoroRaw, url, LLCore::HttpRequest::DEFAULT_POLICY_ID, aHeader, success, failure));
	}

	LLCore::HttpHeaders::ptr_t createModifiedSinceHeader( time_t aTime )
	{
		std::string strDate = LLDate::toHTTPDateString( gmtime( &aTime ), "%A, %d %b %Y %H:%M:%S GMT" );

		LLCore::HttpHeaders::ptr_t pHeader( new LLCore::HttpHeaders() );
		pHeader->append( "If-Modified-Since", strDate );
	
		return pHeader;
	}
}
