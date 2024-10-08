/* Copyright (c) 2010 Katharine Berry All rights reserved.
 *
 * @file growlmanager.h
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
 *   3. Neither the name Katharine Berry nor the names of any contributors
 *      may be used to endorse or promote products derived from this
 *      software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY KATHARINE BERRY AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL KATHARINE BERRY OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef GROWLMANAGER_H
#define GROWLMANAGER_H

#include "llnotifications.h"
#include "growlnotifier.h"
#include "lleventtimer.h"
#include "llavatarnamecache.h"

struct GrowlNotification
{
    std::string growlName;
    std::string growlTitle;
    std::string growlBody;
    bool useDefaultTextForTitle;
    bool useDefaultTextForBody;
};

constexpr U64 GROWL_THROTTLE_TIME = 1000000u; // Maximum spam rate (in microseconds).
constexpr F32 GROWL_THROTTLE_CLEANUP_PERIOD = 300.f; // How often we clean up the list (in seconds).
constexpr int GROWL_MAX_BODY_LENGTH = 255; // Arbitrary.
constexpr char GROWL_IM_MESSAGE_TYPE[] = "Instant Message received";
constexpr char GROWL_KEYWORD_ALERT_TYPE[] = "Keyword Alert";

class GrowlManager : public LLEventTimer
{
    LOG_CLASS(GrowlManager);

public:
    GrowlManager();
    ~GrowlManager();
    bool tick() override;

    static void initiateManager();
    static void destroyManager();
    static bool isUsable();
    static void notify(const std::string& title, const std::string& message, const std::string& type);

private:
    GrowlNotifier*                              mNotifier;
    std::map<std::string, GrowlNotification>    mNotifications;
    std::map<std::string, U64>                  mTitleTimers;
    LLNotificationChannelPtr                    mGrowlNotificationsChannel;

    void loadConfig();
    void performNotification(const std::string& title, const std::string& message, const std::string& type);
    static bool onLLNotification(const LLSD& notice);
    static bool filterOldNotifications(LLNotificationPtr pNotification);
    static void onInstantMessage(const LLSD& im);
    static void onScriptDialog(const LLSD& data);
    static void onNearbyChatMessage(const LLSD& chat);
    static inline bool shouldNotify();
    static void onAvatarNameCache(const LLAvatarName& av_name, const std::string& message, const std::string& type);

    LLBoundListener             mNotificationConnection;
    boost::signals2::connection mInstantMessageConnection;
    boost::signals2::connection mScriptDialogConnection;
    boost::signals2::connection mChatMessageConnection;
};

extern GrowlManager *gGrowlManager;

#endif // GROWLMANAGER_H
