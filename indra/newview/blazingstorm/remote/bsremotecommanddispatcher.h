/**
 * @file bsremotecommanddispatcher.h
 * @brief Applies validated controller commands to the possessed viewer.
 */

#ifndef BS_REMOTE_COMMAND_DISPATCHER_H
#define BS_REMOTE_COMMAND_DISPATCHER_H

#include "blazingstorm/remote/bsremoteprotocol.h"

#include <cstdint>

namespace BlazingStorm
{
    class RemoteCommandDispatcher final
    {
    public:
        static RemoteCommandDispatcher& instance();

        bool dispatch(const RemoteCommand& command);
        void reset();

    private:
        RemoteCommandDispatcher() = default;

        std::uint64_t mLastSequence = 0;
    };
}

#endif // BS_REMOTE_COMMAND_DISPATCHER_H
