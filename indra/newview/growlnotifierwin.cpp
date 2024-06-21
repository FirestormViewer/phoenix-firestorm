/* Copyright (c) 2009
*
* Greg Hendrickson (LordGregGreg Back). All rights reserved.
*
* @file growlnotifierwin.cpp
* @Implementation of desktop notification system (aka growl).
*
* Redistribution and use in source and binary forms, with or
* without modification, are permitted provided that the following
* conditions are met:
*
*   1. Redistributions of source code must retain the above copyright
*      notice, this list of conditions and the following disclaimer.
*   2. Redistributions in binary form must reproduce the above
*      copyright notice, this list of conditions and the following
*      disclaimer in the documentation and/or other materials provided
*      with the distribution.
*   3. Neither the name Modular Systems nor the names of its contributors
*      may be used to endorse or promote products derived from this
*      software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY MODULAR SYSTEMS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
* THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
* PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MODULAR SYSTEMS OR CONTRIBUTORS
* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
* INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
* CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
* ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
* THE POSSIBILITY OF SUCH DAMAGE.
*/


#include "llviewerprecompiledheaders.h"
#include "growlnotifierwin.h"

#include "growl.h"

#include "llviewercontrol.h"

#ifdef GROWL_HAS_LOG
#if LL_WINDOWS
#pragma comment(lib,"growl.lib")
#endif
void __cdecl GrowlLogSink( char const *aMessage, int aLevel )
{
    LL_INFOS( "Growl" ) << aMessage << LL_ENDL;
}
#endif

GrowlNotifierWin::GrowlNotifierWin() :
    mApplicationName("")
{
    mGrowlImpl = NULL;
    LL_INFOS("GrowlNotifierWin") << "Windows growl notifier initialized." << LL_ENDL;

#ifdef GROWL_HAS_LOG
    if( gSavedSettings.controlExists( "FSGrowlLog" ) && gSavedSettings.getBOOL( "FSGrowlLog" ) )
        growl_set_log_sink( GrowlLogSink );
#endif
}

GrowlNotifierWin::~GrowlNotifierWin()
{
    LL_INFOS("GrowlNotifierWin") << "Windows growl notifier destroyed." << LL_ENDL;
    if (mGrowlImpl)
    {
        delete mGrowlImpl;
        mGrowlImpl = NULL;
    }
}

void GrowlNotifierWin::registerApplication(const std::string& application, const std::set<std::string>& notificationTypes)
{
    mApplicationName = application;

    char **arr = (char**)malloc(sizeof(*arr) * notificationTypes.size());
    S32 i = 0;
    for (std::set<std::string>::const_iterator it = notificationTypes.begin(); it != notificationTypes.end(); ++it, ++i)
    {
        char *string = (char*)malloc(it->size() + 1);
        strcpy(string, it->c_str());
        arr[i] = string;
    }
    mGrowlImpl = new Growl (GROWL_TCP, NULL, application.c_str(), (const char **const)arr, static_cast<int>(notificationTypes.size()),
        std::string(gDirUtilp->getDefaultSkinDir() + gDirUtilp->getDirDelimiter() + "textures" + gDirUtilp->getDirDelimiter() + "firestorm_icon.png").c_str());

    for (i = 0; i < (S32)notificationTypes.size(); ++i)
    {
        free(arr[i]);
    }
    free(arr);
}

void GrowlNotifierWin::showNotification(const std::string& notification_title, const std::string& notification_message,
                                         const std::string& notification_type)
{
    if (mGrowlImpl)
    {
        mGrowlImpl->Notify(notification_type.c_str(), notification_title.c_str(), notification_message.c_str());
    }
}

bool GrowlNotifierWin::isUsable()
{
    return (mGrowlImpl && mGrowlImpl->isConnected());
}
