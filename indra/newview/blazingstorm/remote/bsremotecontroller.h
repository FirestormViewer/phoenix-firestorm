/**
 * @file bsremotecontroller.h
 * @brief Controller-side possession session state.
 */

#ifndef BS_REMOTE_CONTROLLER_H
#define BS_REMOTE_CONTROLLER_H

#include "blazingstorm/remote/bsremoteprotocol.h"

#include <cstdint>
#include <string>

namespace BlazingStorm
{
    class RemoteController final
    {
    public:
        static RemoteController& instance();

        void begin(std::string session_id, std::string host_id);
        void end();

        bool isActive() const;
        const std::string& sessionId() const;
        const std::string& hostId() const;

        RemoteCommand makeCommand(RemoteCommandType type, std::string text = {}, std::string target_id = {});

    private:
        RemoteController() = default;

        bool mActive = false;
        std::string mSessionId;
        std::string mHostId;
        std::uint64_t mNextSequence = 1;
    };
}

#endif // BS_REMOTE_CONTROLLER_H
