/**
 * @file fslauncherentrylinux.cpp
 * @brief Linux LauncherEntry unread badge integration.
 */

#include "llviewerprecompiledheaders.h"

#include "fslauncherentrylinux.h"

#include "fslauncherentrylinuxtransport.h"
#include "llviewercontrol.h"

#include <algorithm>
#include <cstdlib>

#ifndef FIRESTORM_LAUNCHER_DESKTOP_ID
#define FIRESTORM_LAUNCHER_DESKTOP_ID "firestorm-viewer.desktop"
#endif

namespace
{
constexpr char BADGE_SETTING[] = "FSShowUnreadLauncherBadge";
}

FSLauncherEntryLinux::FSLauncherEntryLinux()
    : mTransport(std::make_shared<FSLauncherEntryLinuxTransport>(
          FSLauncherEntryLinuxTransport::resolveDesktopId(
              FIRESTORM_LAUNCHER_DESKTOP_ID,
              std::getenv("FIRESTORM_LAUNCHER_DESKTOP_ID"))))
{
    mNewMessageConnection = LLIMModel::instance().addNewMsgCallback(
        [this](const LLSD&) { refreshCount(); });
    mNoUnreadConnection = LLIMModel::instance().addNoUnreadMsgsCallback(
        [this](const LLSD&) { refreshCount(); });

    if (gIMMgr)
    {
        gIMMgr->addSessionObserver(this);
    }

    if (LLControlVariable* control = gSavedSettings.getControl(BADGE_SETTING))
    {
        mBadgePreferenceConnection = control->getSignal()->connect(
            [this](LLControlVariable* variable, const LLSD& value, const LLSD& previous) {
                onBadgePreferenceChanged(variable, value, previous);
            });
    }

    refreshCount();
}

FSLauncherEntryLinux::~FSLauncherEntryLinux()
{
    cleanup();
}

void FSLauncherEntryLinux::cleanup()
{
    if (mCleanedUp)
    {
        return;
    }
    mCleanedUp = true;

    // Remove all model and setting callbacks before releasing transport state.
    // This ordering is required because base viewer cleanup tears down the
    // model/window objects after LLAppViewerLinux::cleanup() returns.
    if (gIMMgr)
    {
        gIMMgr->removeSessionObserver(this);
    }
    mNewMessageConnection.disconnect();
    mNoUnreadConnection.disconnect();
    mBadgePreferenceConnection.disconnect();
    mTransport.reset();
}

void FSLauncherEntryLinux::refreshCount()
{
    if (mCleanedUp || !mTransport)
    {
        return;
    }

    S32 count = 0;
    if (gSavedSettings.getBOOL(BADGE_SETTING) && gIMMgr)
    {
        // This is the authoritative participant-oriented aggregate used by
        // the existing IM well.  Do not derive a second count from signals.
        count = gIMMgr->getNumberOfUnreadParticipantMessages();
    }

    // Invalid offer-counter edge cases must never become a nonsensical badge.
    mTransport->setCount(std::max<S32>(0, count));
}

void FSLauncherEntryLinux::onBadgePreferenceChanged(LLControlVariable*, const LLSD&, const LLSD&)
{
    refreshCount();
}

void FSLauncherEntryLinux::sessionAdded(const LLUUID&, const std::string&, const LLUUID&, bool)
{
    refreshCount();
}

void FSLauncherEntryLinux::sessionActivated(const LLUUID&, const std::string&, const LLUUID&)
{
    refreshCount();
}

void FSLauncherEntryLinux::sessionVoiceOrIMStarted(const LLUUID&)
{
    refreshCount();
}

void FSLauncherEntryLinux::sessionRemoved(const LLUUID&)
{
    refreshCount();
}

void FSLauncherEntryLinux::sessionIDUpdated(const LLUUID&, const LLUUID&)
{
    refreshCount();
}
