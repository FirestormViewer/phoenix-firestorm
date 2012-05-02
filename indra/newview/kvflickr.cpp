/** 
 * @file kvflickr.cpp
 * @brief Basic Flickr library for the client.
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

#include "llbufferstream.h"
#include "lluri.h"
#include "llmd5.h"
#include "llhttpclient.h"
#include "llxmltree.h"

#if LL_STANDALONE
#include "jsoncpp/json.h"
#else
#include "json.h"
#endif


#include "kvflickr.h"

class KVFlickrResponse : public LLHTTPClient::Responder
{
public:
	KVFlickrResponse(KVFlickrRequest::response_callback_t &callback);
	/* virtual */ void completedRaw(
									U32 status,
									const std::string& reason,
									const LLChannelDescriptors& channels,
									const LLIOPipe::buffer_ptr_t& buffer);
private:
	KVFlickrRequest::response_callback_t mCallback;
};

class KVFlickrUploadResponse : public LLHTTPClient::Responder
{
public:
	KVFlickrUploadResponse(KVFlickrRequest::response_callback_t &callback);
	/* virtual */ void completedRaw(
									U32 status,
									const std::string& reason,
									const LLChannelDescriptors& channels,
									const LLIOPipe::buffer_ptr_t& buffer);
private:
	KVFlickrRequest::response_callback_t mCallback;
};

void KVFlickrRequest::request(const std::string& method, const LLSD& args, response_callback_t callback) 
{
	LLSD params(args);
	params["format"] = "json";
	params["method"] = method;
	params["api_key"] = KV_FLICKR_API_KEY;
	params["nojsoncallback"] = 1;
	params["api_sig"] = getSignatureForCall(params, true); // This must be the last one set.
	LLHTTPClient::get("http://flickr.com/services/rest/", params, new KVFlickrResponse(callback));
}

//static
void KVFlickrRequest::uploadPhoto(const LLSD& args, LLImageFormatted *image, response_callback_t callback)
{
	LLSD params(args);
	params["api_key"] = KV_FLICKR_API_KEY;
	params["api_sig"] = getSignatureForCall(params, false);

	// It would be nice if there was an easy way to do multipart form data. Oh well.
	std::string boundary = "------------" + LLUUID::generateNewID().asString();
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
	if(image->getExtension() == "jpg")
	{
		post_stream << "image/jpeg";
	}
	else if(image->getExtension() == "png")
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
	// C memory operations abound!
	std::string post_str = post_stream.str();
	size_t total_data_size = image->getDataSize() + post_str.length() + boundary.length() + 6; // + 6 = "\r\n" + "--" + "--"
	char* post_data = new char[total_data_size + 1];
	memcpy(post_data, post_str.data(), post_str.length());
	char* address = post_data + post_str.length();
	memcpy(address, image->getData(), image->getDataSize());
	address += image->getDataSize();
	std::string post_tail = "\r\n--" + boundary + "--";
	memcpy(address, post_tail.data(), post_tail.length());
	address += post_tail.length();
	llassert(address <= post_data + total_data_size /* After all that, check we didn't overrun */);
	
	// We have a post body! Now we can go about building the actual request...
	LLSD headers;
	headers["Content-Type"] = "multipart/form-data; boundary=" + boundary;
	LLHTTPClient::postRaw("http://api.flickr.com/services/upload/", (U8*)post_data, total_data_size, new KVFlickrUploadResponse(callback), headers);
	// The HTTP client takes ownership of our post_data array,
	// and will delete it when it's done.
}

//static
std::string KVFlickrRequest::getSignatureForCall(const LLSD& parameters, bool encoded)
{
	std::vector<std::string> keys;
	for(LLSD::map_const_iterator itr = parameters.beginMap(); itr != parameters.endMap(); ++itr)
	{
		keys.push_back(itr->first);
	}
	std::sort(keys.begin(), keys.end());
	std::string to_hash(KV_FLICKR_API_SECRET);
	for(std::vector<std::string>::const_iterator itr  = keys.begin(); itr != keys.end(); ++itr)
	{
		to_hash += *itr;
		if(encoded)
		{
			to_hash += LLURI::escapeQueryValue(parameters[*itr].asString());
		}
		else
		{
			to_hash += parameters[*itr].asString();
		}
	}
	LLMD5 hashed((const unsigned char*)to_hash.c_str());
	char hex_hash[MD5HEX_STR_SIZE];
	hashed.hex_digest(hex_hash);
	return std::string(hex_hash);
}

KVFlickrUploadResponse::KVFlickrUploadResponse(KVFlickrRequest::response_callback_t &callback)
: mCallback(callback)
{
}

void KVFlickrUploadResponse::completedRaw(
									 U32 status,
									 const std::string& reason,
									 const LLChannelDescriptors& channels,
									 const LLIOPipe::buffer_ptr_t& buffer)
{
	LLBufferStream istr(channels, buffer.get());
	std::stringstream strstrm;
	strstrm << istr.rdbuf();
	std::string result = std::string(strstrm.str());

	LLSD output;
	bool success;

	LLXmlTree tree;
	if(!tree.parseString(result))
	{
		LL_WARNS("FlickrAPI") << "Couldn't parse flickr response: " << result << LL_ENDL;
		mCallback(false, LLSD());
		return;
	}
	LLXmlTreeNode* root = tree.getRoot();
	if(!root->hasName("rsp"))
	{
		LL_WARNS("FlickrAPI") << "Bad root node: " << root->getName() << LL_ENDL;
		mCallback(false, LLSD());
		return;
	}
	std::string stat;
	root->getAttributeString("stat", stat);
	output["stat"] = stat;
	if(stat == "ok")
	{
		success = true;
		LLXmlTreeNode* photoid_node = root->getChildByName("photoid");
		if(photoid_node)
		{
			output["photoid"] = photoid_node->getContents();
		}
	}
	else
	{
		success = false;
		LLXmlTreeNode* err_node = root->getChildByName("err");
		if(err_node)
		{
			S32 code;
			std::string msg;
			err_node->getAttributeS32("code", code);
			err_node->getAttributeString("msg", msg);
			output["code"] = code;
			output["msg"] = msg;
		}
	}
	mCallback(success, output);
}

void JsonToLLSD(const Json::Value &root, LLSD &output)
{
	if(root.isObject())
	{
		Json::Value::Members keys = root.getMemberNames();
		for(Json::Value::Members::const_iterator itr = keys.begin(); itr != keys.end(); ++itr)
		{
			LLSD elem;
			JsonToLLSD(root[*itr], elem);
			output[*itr] = elem;
		}
	}
	else if(root.isArray())
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
		switch(root.type())
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

KVFlickrResponse::KVFlickrResponse(KVFlickrRequest::response_callback_t &callback) : 
	mCallback(callback)
{
}

void KVFlickrResponse::completedRaw(
							   U32 status,
							   const std::string& reason,
							   const LLChannelDescriptors& channels,
							   const LLIOPipe::buffer_ptr_t& buffer)
{
	LLBufferStream istr(channels, buffer.get());
	std::stringstream strstrm;
	strstrm << istr.rdbuf();
	std::string result = std::string(strstrm.str());
	Json::Value root;
	Json::Reader reader;

	bool success = reader.parse(result, root);
	if(!success)
	{
		mCallback(false, LLSD());
		return;
	}
	else
	{
		LL_INFOS("FlickrAPI") << "Got response string: " << result << LL_ENDL;
		LLSD response;
		JsonToLLSD(root, response);
		LL_INFOS("FlickrAPI") << "As LLSD: " << response << LL_ENDL;
		mCallback(isGoodStatus(status), response);
	}
}
	
