/**
 * @file bsremotecommanddispatcher.cpp
 * @brief Applies validated controller commands to the possessed viewer.
 */

#include "llviewerprecompiledheaders.h"

#include "blazingstorm/remote/bsremotecommanddispatcher.h"

#include "blazingstorm/remote/bsremoteactions.h"
#include "blazingstorm/remote/bsremotesession.h"

namespace BlazingStorm
{
    RemoteCommandDispatcher& RemoteCommandDispatcher::instance()
    {
        static RemoteCommandDispatcher dispatcher;
        return dispatcher;
    }

    bool RemoteCommandDispatcher::dispatch(const RemoteCommand& command)
    {
        auto& session = RemoteSession::instance();
        if (!session.isActive())
        {
            return false;
        }

        // Reject stale/replayed controller commands.
        if (command.sequence != 0 && command.sequence <= mLastSequence)
        {
            return false;
        }

        if (command.sequence != 0)
        {
            mLastSequence = command.sequence;
        }

        auto& actions = RemoteActions::instance();

        switch (command.type)
        {
            case RemoteCommandType::MoveForward:  return actions.moveForward();
            case RemoteCommandType::MoveBackward: return actions.moveBackward();
            case RemoteCommandType::StrafeLeft:   return actions.strafeLeft();
            case RemoteCommandType::StrafeRight:  return actions.strafeRight();
            case RemoteCommandType::TurnLeft:     return actions.turnLeft();
            case RemoteCommandType::TurnRight:    return actions.turnRight();
            case RemoteCommandType::Jump:         return actions.jump();
            case RemoteCommandType::Stop:
                actions.stopMovement();
                return true;
            case RemoteCommandType::Say:
                return actions.say(command.text);
            case RemoteCommandType::EmergencyRelease:
                actions.stopMovement();
                session.emergencyRelease();
                reset();
                return true;
            default:
                return false;
        }
    }

    void RemoteCommandDispatcher::reset()
    {
        mLastSequence = 0;
    }
}
