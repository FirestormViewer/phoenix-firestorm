/** 
 * @file llpostcard.cpp
 * @brief Sending postcards.
 *
 * $LicenseInfo:firstyear=2011&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2011, Linden Research, Inc.
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

#include "llviewerprecompiledheaders.h"

#include "llpostcard.h"

#include "llvfile.h"
#include "llvfs.h"
#include "llviewerregion.h"

#include "message.h"

#include "llagent.h"
#include "llassetstorage.h"
#include "llviewerassetupload.h"

#include "llviewernetwork.h"
///////////////////////////////////////////////////////////////////////////////

// <FS:Ansariel> For OpenSim compatibility
//LLPostcardUploadInfo::LLPostcardUploadInfo(std::string nameFrom, std::string emailTo,
LLPostcardUploadInfo::LLPostcardUploadInfo(std::string emailFrom, std::string nameFrom, std::string emailTo,
// </FS:Ansariel>
        std::string subject, std::string message, LLVector3d globalPosition,
        LLPointer<LLImageFormatted> image, invnUploadFinish_f finish) :
    LLBufferedAssetUploadInfo(LLUUID::null, image, finish),
    mEmailFrom(emailFrom), // <FS:Ansariel> For OpenSim compatibility
    mNameFrom(nameFrom),
    mEmailTo(emailTo),
    mSubject(subject),
    mMessage(message),
    mGlobalPosition(globalPosition)
{
}

LLSD LLPostcardUploadInfo::generatePostBody()
{
    LLSD postcard = LLSD::emptyMap();
    postcard["pos-global"] = mGlobalPosition.getValue();
    postcard["to"] = mEmailTo;
    // <FS:Ansariel> For OpenSim compatibility
    if (!LLGridManager::instance().isInSecondLife())
    {
        postcard["from"] = mEmailFrom;
    }
    // </FS:Ansariel>
    postcard["name"] = mNameFrom;
    postcard["subject"] = mSubject;
    postcard["msg"] = mMessage;

    return postcard;
}


///////////////////////////////////////////////////////////////////////////////
// LLPostCard

LLPostCard::result_callback_t LLPostCard::mResultCallback;

// static
void LLPostCard::reportPostResult(bool ok)
{
	if (mResultCallback)
	{
		mResultCallback(ok);
	}
}
