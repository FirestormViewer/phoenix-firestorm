/**
 * @file bsremotecommanddispatcher.cpp
 * @brief Applies validated controller commands to the possessed viewer.
 */

#include "llviewerprecompiledheaders.h"

#include "blazingstorm/remote/bsremotecommanddispatcher.h"

#include "blazingstorm/remote/bsremoteactions.h"
#include "blazingstorm/remote/bsremotesession.h"
#include "blazingstorm/remote/bsworldinteraction.h"
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
            case RemoteCommandType::MoveUp:       return actions.moveUp();
            case RemoteCommandType::MoveDown:     return actions.moveDown();
            case RemoteCommandType::FlyOn:        return actions.flyOn();
            case RemoteCommandType::FlyOff:       return actions.flyOff();
            case RemoteCommandType::ToggleFly:    return actions.toggleFly();
            case RemoteCommandType::Jump:         return actions.jump();
            case RemoteCommandType::StopForward:
                actions.stopForward();
                return true;
            case RemoteCommandType::StopStrafe:
                actions.stopStrafe();
                return true;
            case RemoteCommandType::StopTurn:
                actions.stopTurn();
                return true;
            case RemoteCommandType::StopVertical:
                actions.stopVertical();
                return true;
            case RemoteCommandType::Stop:
                actions.stopMovement();
                return true;
            case RemoteCommandType::Say:
                return actions.say(command.text);
            case RemoteCommandType::SitObject:
                return session.hasPermission(RemotePermission::SitStand)
                    && WorldInteraction::instance().sitAsSubject(command);
            case RemoteCommandType::Stand:
                return session.hasPermission(RemotePermission::SitStand)
                    && WorldInteraction::instance().standAsSubject();
            case RemoteCommandType::TouchObject:
                return session.hasPermission(RemotePermission::Touch)
                    && WorldInteraction::instance().touchAsSubject(command);
            case RemoteCommandType::DialogReply:
            {
                if (!session.hasPermission(RemotePermission::ScriptDialogs))
                {
                    return false;
                }

                S32 button_index = -1;
                try
                {
                    std::size_t used = 0;
                    button_index = std::stoi(command.text, &used);
                    if (used != command.text.size()) return false;
                }
                catch (...)
                {
                    return false;
                }

                return WorldInteraction::instance().replyToScriptDialog(
                    command.targetId,
                    button_index);
            }

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
            case RemoteCommandType::RestrictMovementOn:
            case RemoteCommandType::RestrictMovementOff:
            case RemoteCommandType::RestrictNearbyChatOn:
            case RemoteCommandType::RestrictNearbyChatOff:
            case RemoteCommandType::RestrictInstantMessageOn:
            case RemoteCommandType::RestrictInstantMessageOff:
            {
                if (!session.hasPermission(RemotePermission::ManageSubjectRestrictions))
                {
                    return false;
                }

                auto restrictions = session.subjectRestrictions();
                SubjectRestriction restriction = SubjectRestriction::None;
                bool enable = false;

                switch (command.type)
                {
                    case RemoteCommandType::RestrictMovementOn:
                        restriction = SubjectRestriction::Movement;
                        enable = true;
                        break;
                    case RemoteCommandType::RestrictMovementOff:
                        restriction = SubjectRestriction::Movement;
                        break;
                    case RemoteCommandType::RestrictNearbyChatOn:
                        restriction = SubjectRestriction::NearbyChat;
                        enable = true;
                        break;
                    case RemoteCommandType::RestrictNearbyChatOff:
                        restriction = SubjectRestriction::NearbyChat;
                        break;
                    case RemoteCommandType::RestrictInstantMessageOn:
                        restriction = SubjectRestriction::InstantMessage;
                        enable = true;
                        break;
                    case RemoteCommandType::RestrictInstantMessageOff:
                        restriction = SubjectRestriction::InstantMessage;
                        break;
                    default:
                        break;
                }

                const auto mask = toMask(restriction);
                if (enable)
                {
                    restrictions |= mask;
                }
                else
                {
                    restrictions &= ~mask;
                }
                session.setSubjectRestrictions(restrictions);

                std::string restriction_name = "subject direct-IM restriction.";
                if (restriction == SubjectRestriction::Movement)
                {
                    restriction_name = "subject movement restriction.";
                }
                else if (restriction == SubjectRestriction::NearbyChat)
                {
                    restriction_name = "subject nearby-chat restriction.";
                }

                FSCommon::report_to_nearby_chat(
                    std::string("[Blazing Storm] Controller ")
                    + (enable ? "enabled " : "disabled ")
                    + restriction_name);
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
