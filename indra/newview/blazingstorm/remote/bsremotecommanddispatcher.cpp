/**
 * @file bsremotecommanddispatcher.cpp
 * @brief Applies validated controller commands to the possessed viewer.
 */

#include "llviewerprecompiledheaders.h"

#include "blazingstorm/remote/bsremotecommanddispatcher.h"

#include "blazingstorm/remote/bsremoteactions.h"
#include "blazingstorm/remote/bsremotesession.h"
#include "fscommon.h"
#include "llimview.h"
#include "llviewermessage.h"

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
            case RemoteCommandType::SendInstantMessage:
            {
                if (!session.hasPermission(RemotePermission::InstantMessage)
                    || command.targetId.empty()
                    || command.text.empty())
                {
                    return false;
                }

                LLUUID target_id(command.targetId);
                if (target_id.isNull())
                {
                    return false;
                }

                const LLUUID im_session_id =
                    LLIMMgr::computeSessionID(IM_NOTHING_SPECIAL, target_id);
                send_simple_im(target_id, command.text, IM_NOTHING_SPECIAL, im_session_id);

                // Keep controller-authored identity actions visible to the
                // subject even though send_simple_im() has no local chat echo.
                FSCommon::report_to_nearby_chat(
                    "[Blazing Storm] Controller IM sent as you to "
                    + target_id.asString() + ": " + command.text);
                return true;
            }
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
