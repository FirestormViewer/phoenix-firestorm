/**
 * @file fslauncherentrylinux.h
 * @brief Linux LauncherEntry unread badge integration.
 */

#ifndef FS_LAUNCHER_ENTRY_LINUX_H
#define FS_LAUNCHER_ENTRY_LINUX_H

#include "llimview.h"

#include <boost/signals2/connection.hpp>

#include <memory>

class LLControlVariable;
class LLSD;
class FSLauncherEntryLinuxTransport;

/**
 * Owns the Linux desktop badge lifetime and observes Firestorm's existing IM
 * model.  It never stores or updates unread state itself; every publication
 * recomputes LLIMMgr's participant-message aggregate.
 */
class FSLauncherEntryLinux final : public LLIMSessionObserver
{
public:
    FSLauncherEntryLinux();
    ~FSLauncherEntryLinux() override;

    FSLauncherEntryLinux(const FSLauncherEntryLinux&) = delete;
    FSLauncherEntryLinux& operator=(const FSLauncherEntryLinux&) = delete;

    void cleanup();

    void sessionAdded(const LLUUID&, const std::string&, const LLUUID&, bool) override;
    void sessionActivated(const LLUUID&, const std::string&, const LLUUID&) override;
    void sessionVoiceOrIMStarted(const LLUUID&) override;
    void sessionRemoved(const LLUUID&) override;
    void sessionIDUpdated(const LLUUID&, const LLUUID&) override;

private:
    void refreshCount();
    void onBadgePreferenceChanged(LLControlVariable*, const LLSD&, const LLSD&);

    std::shared_ptr<FSLauncherEntryLinuxTransport> mTransport;
    boost::signals2::connection mNewMessageConnection;
    boost::signals2::connection mNoUnreadConnection;
    boost::signals2::connection mBadgePreferenceConnection;
    bool mCleanedUp{false};
};

#endif // FS_LAUNCHER_ENTRY_LINUX_H
