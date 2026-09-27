/**
 * @file bsremotecontroller.cpp
 * @brief Controller-side possession session state.
 */

#include "llviewerprecompiledheaders.h"

#include "blazingstorm/remote/bsremotecontroller.h"

#include <utility>

namespace BlazingStorm
{
    RemoteController& RemoteController::instance()
    {
        static RemoteController controller;
        return controller;
    }

    void RemoteController::begin(std::string session_id, std::string host_id)
    {
        mSessionId = std::move(session_id);
        mHostId = std::move(host_id);
        mNextSequence = 1;
        mActive = true;
    }

    void RemoteController::end()
    {
        mActive = false;
        mSessionId.clear();
        mHostId.clear();
        mNextSequence = 1;
    }

    bool RemoteController::isActive() const
    {
        return mActive;
    }

    const std::string& RemoteController::sessionId() const
    {
        return mSessionId;
    }

    const std::string& RemoteController::hostId() const
    {
        return mHostId;
    }

    RemoteCommand RemoteController::makeCommand(RemoteCommandType type, std::string text)
    {
        RemoteCommand command;
        command.type = type;
        command.text = std::move(text);
        command.sequence = mNextSequence++;
        return command;
    }
}
