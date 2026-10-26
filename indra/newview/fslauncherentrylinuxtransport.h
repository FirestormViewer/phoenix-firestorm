/**
 * @file fslauncherentrylinuxtransport.h
 * @brief Asynchronous Unity LauncherEntry badge transport for Linux.
 */

#ifndef FS_LAUNCHER_ENTRY_LINUX_TRANSPORT_H
#define FS_LAUNCHER_ENTRY_LINUX_TRANSPORT_H

#include "stdtypes.h"

#include <memory>
#include <string>
#include <string_view>

/**
 * Publishes the effective unread count without owning a D-Bus service name.
 *
 * The transport caches only the last desired/published presentation state.
 * LLIMMgr remains the unread-state authority; callers must provide a freshly
 * recomputed effective count.
 */
class FSLauncherEntryLinuxTransport final
{
public:
    explicit FSLauncherEntryLinuxTransport(std::string desktop_id);
    ~FSLauncherEntryLinuxTransport();

    FSLauncherEntryLinuxTransport(const FSLauncherEntryLinuxTransport&) = delete;
    FSLauncherEntryLinuxTransport& operator=(const FSLauncherEntryLinuxTransport&) = delete;

    void setCount(S32 count);

    static bool isValidDesktopId(std::string_view desktop_id);
    static std::string resolveDesktopId(std::string_view configured_desktop_id,
                                        const char* runtime_override);

private:
    struct Impl;
    std::shared_ptr<Impl> mImpl;
};

#endif // FS_LAUNCHER_ENTRY_LINUX_TRANSPORT_H
