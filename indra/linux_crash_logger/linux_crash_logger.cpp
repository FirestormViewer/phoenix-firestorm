/** 
 * @file linux_crash_logger.cpp
 * @brief Linux crash logger implementation
 *
 * $LicenseInfo:firstyear=2003&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
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
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#include <iostream>
#include <string>
#include <curl/curl.h>
#include <FL/fl_ask.H>

/* Called via 
        execl( gCrashLogger.c_str(), gCrashLogger.c_str(), descriptor.path(), gVersion.c_str(), gBugsplatDB.c_str(), nullptr );
*/
int main(int argc, char **argv)
{
    std::cerr << "linux crash logger called: ";
    for( int i = 1; i < argc; ++i )
        std::cerr << argv[i] << " ";

    std::cerr << std::endl;

    if( argc < 4 )
    {
        std::cerr << argv[0] << "Not enough arguments" << std::endl;
        return 1;
    }

    std::string dmpFile{ argv[1] };
    std::string version{ argv[2] };
    std::string strDb{ argv[3] };
	std::string strAsk{ argv[4] };

	if( strAsk == "ask" )
	{
		auto choice = fl_choice( "Firestorm has crashed, submit the minidump?", "No", "Yes", nullptr );
		if( choice == 0 )
		{
			std::cerr << "Abort send due to users choice" << std::endl;
			return 0;
		}
	}

    std::string url{ "https://" };
    url += strDb;
    url += ".bugsplat.com/post/bp/crash/crashpad.php";
    
    curl_global_init(CURL_GLOBAL_ALL);

    auto curl = curl_easy_init();

//Not compatible with LL's crusty old curl - support for the curl_mime functions
//was introduced in curl 7.57.0, LL is still using 7.54 :(

/*
    if( curl)
    {
        auto form = curl_mime_init(curl);
        
        auto field = curl_mime_addpart(form);
        curl_mime_name(field, "upload_file_minidump");
        curl_mime_filedata(field, dmpFile.c_str() );

        field = curl_mime_addpart(form);
        curl_mime_name(field, "product");
        curl_mime_data(field, "Firestorm-Releasex64", CURL_ZERO_TERMINATED);

        field = curl_mime_addpart(form);
        curl_mime_name(field, "version");
        curl_mime_data(field, version.c_str(), CURL_ZERO_TERMINATED);

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str() );
        curl_easy_setopt(curl, CURLOPT_MIMEPOST, form);
 
        auto res = curl_easy_perform(curl);

        if(res != CURLE_OK)
            std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl; 
 
        curl_easy_cleanup(curl);

        curl_mime_free(form);
    }*/

if (auto curl_handle = curl_easy_init()) {
    struct curl_httppost* formpost = NULL;
    struct curl_httppost* lastptr = NULL;

    // Add the file part
    curl_formadd(&formpost, &lastptr,
                 CURLFORM_COPYNAME, "upload_file_minidump",
                 CURLFORM_FILE, dmpFile.c_str(),
                 CURLFORM_END);

    // Add the 'product' part
    curl_formadd(&formpost, &lastptr,
                 CURLFORM_COPYNAME, "product",
                 CURLFORM_COPYCONTENTS, "Firestorm-Releasex64",
                 CURLFORM_END);

    // Add the 'version' part
    curl_formadd(&formpost, &lastptr,
                 CURLFORM_COPYNAME, "version",
                 CURLFORM_COPYCONTENTS, version.c_str(),
                 CURLFORM_END);

    // Set the URL
    curl_easy_setopt(curl_handle, CURLOPT_URL, url.c_str());

    // Set the form post data
    curl_easy_setopt(curl_handle, CURLOPT_HTTPPOST, formpost);

    // Perform the request
    auto res = curl_easy_perform(curl_handle);

    // Cleanup
    curl_easy_cleanup(curl_handle);
    curl_formfree(formpost);

    // Check for errors
    if (res != CURLE_OK)
        std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
}

    return 0;
}
