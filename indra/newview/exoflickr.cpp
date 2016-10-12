/**
 * @file exoflickr
 * @brief Lightweight wrapper around the Flickr API for signing and such
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

#include "llviewerprecompiledheaders.h"

#include "exoflickr.h"
#include "fscorehttputil.h"
#include "llbufferstream.h"
#include "lluri.h"
#include "llxmltree.h"
#include "llsdserialize.h"
#include "llviewercontrol.h"
#include "llbase64.h"
#include "llcorehttputil.h"
#include "llsdutil.h"

// third-party
#if LL_USESYSTEMLIBS
#include "jsoncpp/reader.h" // JSON
#else
#include "reader.h" // JSON
#endif

#include <openssl/hmac.h>
#include <openssl/evp.h>


const std::string UPLOAD_URL = "https://up.flickr.com/services/upload/";
const std::string API_URL = "https://api.flickr.com/services/rest/";

void exoFlickrUploadResponse( LLSD const &aData, exoFlickr::response_callback_t aCallback )
{
	LLSD header = aData[ LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS ][ LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS_HEADERS];
	LLCore::HttpStatus status = LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD( aData[ LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS ] );

	LL_INFOS("FlickrAPI") << "Status " << status.getType() << " aData " << ll_pretty_print_sd( aData ) << LL_ENDL;

	if (!aData.has( LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS_RAW ) )
	{
		LL_WARNS("FlickrAPI") << "No content data included in response" << LL_ENDL;
		if (aCallback)
		{
			aCallback(false, LLSD());
		}
		return;
	}
	
	const LLSD::Binary &rawData = aData[LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS_RAW].asBinary();
	std::string result;
	result.assign( rawData.begin(), rawData.end() );

	LLSD output;
	bool success;

	LLXmlTree tree;
	if (!tree.parseString(result))
	{
		LL_WARNS("FlickrAPI") << "Couldn't parse flickr response(" << status.getType() << "): " << result << LL_ENDL;
		if (aCallback)
		{
			aCallback(false, LLSD());
		}
		return;
	}
	LLXmlTreeNode* root = tree.getRoot();
	if (!root->hasName("rsp"))
	{
		LL_WARNS("FlickrAPI") << "Bad root node: " << root->getName() << LL_ENDL;
		if (aCallback)
		{
			aCallback(false, LLSD());
		}
		return;
	}
	std::string stat;
	root->getAttributeString("stat", stat);
	output["stat"] = stat;
	if (stat == "ok")
	{
		success = true;
		LLXmlTreeNode* photoid_node = root->getChildByName("photoid");
		if (photoid_node)
		{
			output["photoid"] = photoid_node->getContents();
		}
	}
	else
	{
		success = false;
		LLXmlTreeNode* err_node = root->getChildByName("err");
		if (err_node)
		{
			S32 code;
			std::string msg;
			err_node->getAttributeS32("code", code);
			err_node->getAttributeString("msg", msg);
			output["code"] = code;
			output["msg"] = msg;
		}
	}

	if (aCallback)
	{
		aCallback(success, output);
	}
}

static void JsonToLLSD(const Json::Value &root, LLSD &output)
{
	if (root.isObject())
	{
		Json::Value::Members keys = root.getMemberNames();
		for(Json::Value::Members::const_iterator itr = keys.begin(); itr != keys.end(); ++itr)
		{
			LLSD elem;
			JsonToLLSD(root[*itr], elem);
			output[*itr] = elem;
		}
	}
	else if (root.isArray())
	{
		for(Json::Value::const_iterator itr = root.begin(); itr != root.end(); ++itr)
		{
			LLSD elem;
			JsonToLLSD(*itr, elem);
			output.append(elem);
		}
	}
	else
	{
		switch (root.type())
		{
			case Json::intValue:
				output = root.asInt();
				break;
			case Json::realValue:
			case Json::uintValue:
				output = root.asDouble();
				break;
			case Json::stringValue:
				output = root.asString();
				break;
			case Json::booleanValue:
				output = root.asBool();
				break;
			case Json::nullValue:
				output = LLSD();
				break;
			default:
				break;
		}
	}
}

void exoFlickrResponse( LLSD const &aData, exoFlickr::response_callback_t aCallback )
{
	LLSD header = aData[ LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS ][ LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS_HEADERS];
	LLCore::HttpStatus status = LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD( aData[ LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS ] );

	const LLSD::Binary &rawData = aData[LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS_RAW].asBinary();
	std::string result;
	result.assign( rawData.begin(), rawData.end() );

	Json::Value root;
	Json::Reader reader;

	bool success = reader.parse(result, root);
	if(!success)
	{
		if (aCallback)
		{
			aCallback(false, LLSD());
		}
		return;
	}
	else
	{
		LL_INFOS("FlickrAPI") << "Got response string: " << result << LL_ENDL;
		LLSD response;
		JsonToLLSD(root, response);
		if (aCallback)
		{
			aCallback((status.getType() >= 200 && status.getType() < 300), response);
		}
	}
}

//static
void exoFlickr::request(const std::string& method, const LLSD& args, response_callback_t callback)
{
	LLSD params(args);
	params["format"] = "json";
	params["method"] = method;
	params["nojsoncallback"] = 1;
	signRequest(params, "GET", API_URL);
	
	std::string url = LLURI::buildHTTP( API_URL, LLSD::emptyArray(), params ).asString();
	FSCoreHttpUtil::callbackHttpGetRaw( url, boost::bind( exoFlickrResponse, _1, callback ) );
}

void exoFlickr::signRequest(LLSD& params, std::string method, std::string url)
{
	// Oauth junk
	params["oauth_consumer_key"] = EXO_FLICKR_API_KEY;
	std::string oauth_token = gSavedPerAccountSettings.getString("ExodusFlickrToken");
	if(oauth_token.length())
	{
	   params["oauth_token"] = oauth_token;
	}
	params["oauth_signature_method"] = "HMAC-SHA1";
	params["oauth_timestamp"] = (LLSD::Integer)time(NULL);
	params["oauth_nonce"] = ll_rand();
	params["oauth_version"] = "1.0";
	params["oauth_signature"] = getSignatureForCall(params, url, method); // This must be the last one set.
}

//static
void exoFlickr::uploadPhoto(const LLSD& args, LLImageFormatted *image, response_callback_t callback)
{
	LLSD params(args);
	signRequest(params, "POST", UPLOAD_URL);

	// It would be nice if there was an easy way to do multipart form data. Oh well.
	const std::string boundary = "------------abcdefgh012345";
	std::ostringstream post_stream;
	post_stream << "--" << boundary;
	// Add all the parameters from LLSD to the query.
	for(LLSD::map_const_iterator itr = params.beginMap(); itr != params.endMap(); ++itr)
	{
		post_stream << "\r\nContent-Disposition: form-data; name=\"" << itr->first << "\"";
		post_stream << "\r\n\r\n" << itr->second.asString();
		post_stream << "\r\n" << "--" << boundary;
	}
	// Headers for the photo
	post_stream << "\r\nContent-Disposition: form-data; name=\"photo\"; filename=\"snapshot." << image->getExtension() << "\"";
	post_stream << "\r\nContent-Type: ";
	// Apparently LLImageFormatted doesn't know what mimetype it has.
	if (image->getExtension() == "jpg")
	{
		post_stream << "image/jpeg";
	}
	else if (image->getExtension() == "png")
	{
		post_stream << "image/png";
	}
	else // This will (probably) only happen if someone decides to put the BMP entry back in the format selection floater.
	{    // I wonder if Flickr would do the right thing.
		post_stream << "application/x-wtf";
		LL_WARNS("FlickrAPI") << "Uploading unknown image type." << LL_ENDL;
	}
	post_stream << "\r\n\r\n";

	// Now we build the postdata array, including the photo in the middle of it.
	std::string post_str = post_stream.str();
	std::string post_tail = "\r\n--" + boundary + "--";

	std::string post_data = post_str;
	// check that we have enough capacity in the string to append the data
	S32 image_data_len = image->getDataSize();
	if (image_data_len <= post_data.max_size() - post_data.size() - post_tail.size())
	{
		post_data.append(reinterpret_cast<char const*>(image->getData()), image->getDataSize());
		post_data.append(post_tail.c_str(), post_tail.size());
	}
	else
	{
		LL_WARNS("FlickrAPI") << "Image data too large. Malformed post body produced." << LL_ENDL;
		if (callback)
		{
			callback(false, LLSD());
		}
	}

	// We have a post body! Now we can go about building the actual request...
	LLCore::HttpHeaders::ptr_t pHeader(new LLCore::HttpHeaders());
	LLCore::HttpOptions::ptr_t options(new LLCore::HttpOptions());
	pHeader->append("Content-Type", "multipart/form-data; boundary=" + boundary);
	options->setWantHeaders(true);
	options->setRetries(0);
	options->setTimeout(600);
	FSCoreHttpUtil::callbackHttpPostRaw(UPLOAD_URL, post_data, boost::bind( exoFlickrUploadResponse, _1, callback ), boost::bind( exoFlickrUploadResponse, _1, callback ), pHeader, options );
}

//static
std::string exoFlickr::getSignatureForCall(const LLSD& parameters, std::string url, std::string method)
{
	std::vector<std::string> keys;
	for(LLSD::map_const_iterator itr = parameters.beginMap(); itr != parameters.endMap(); ++itr)
	{
		keys.push_back(itr->first);
	}
	std::sort(keys.begin(), keys.end());
	std::ostringstream q;
	q << LLURI::escape(method);
	q << "&" << LLURI::escape(url) << "&";
	for(std::vector<std::string>::const_iterator itr  = keys.begin(); itr != keys.end(); ++itr)
	{
		if(itr != keys.begin())
		{
			q << "%26";
		}
		q << LLURI::escape(*itr);
		q << "%3D" << LLURI::escape(LLURI::escape(parameters[*itr]));
	}

	unsigned char data[EVP_MAX_MD_SIZE];
	unsigned int length;
	std::string key = std::string(EXO_FLICKR_API_SECRET) + "&" + gSavedPerAccountSettings.getString("ExodusFlickrTokenSecret");

	std::string to_hash = q.str();
	HMAC(EVP_sha1(), (void*)key.c_str(), key.length(), (unsigned char*)to_hash.c_str(), to_hash.length(), data, &length);
	std::string signature = LLBase64::encode((U8*)data, length);
	return signature;
}
