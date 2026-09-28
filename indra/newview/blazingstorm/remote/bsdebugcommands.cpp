/**
 * @file bsdebugcommands.cpp
 * @brief Local chat commands used to exercise Blazing Storm remote control.
 */

#include "llviewerprecompiledheaders.h"

#include "blazingstorm/remote/bsdebugcommands.h"

#include "blazingstorm/remote/bslocaltransport.h"
#include "blazingstorm/remote/bsremotecontroller.h"

#include "blazingstorm/remote/bsremoteactions.h"
#include "blazingstorm/remote/bsremoteevents.h"
#include "blazingstorm/remote/bsremotesession.h"
#include "fscommon.h"
#include "llagent.h"
#include "llagentui.h"
#include "llfloaterreg.h"

#include <sstream>
#include <string>

namespace
{
    constexpr const char* COMMAND = "/blaze";

    void report(const std::string& message)
    {
        FSCommon::report_to_nearby_chat("[Blazing Storm] " + message);
    }

    BlazingStorm::RemotePermissionMask localDebugPermissions()
    {
        return BlazingStorm::toMask(BlazingStorm::RemotePermission::Movement)
             | BlazingStorm::toMask(BlazingStorm::RemotePermission::Chat);
    }

    bool requireSession()
    {
        if (BlazingStorm::RemoteSession::instance().isActive())
        {
            return true;
        }

        report("No remote session is active. Use /blaze on first.");
        return false;
    }
}

namespace BlazingStorm
{
    bool handleDebugChatCommand(std::string_view text)
    {
        std::istringstream input(static_cast<std::string>(text));
        std::string command;
        input >> command;

        if (command != COMMAND)
        {
            return false;
        }

        std::string action;
        input >> action;

        auto& session = RemoteSession::instance();
        auto& actions = RemoteActions::instance();
        auto& transport = LocalTransport::instance();
        auto& controller = RemoteController::instance();

        if (action.empty() || action == "help")
        {
            report("Commands: /blaze ui | host | request <subject-uuid> | connect <code> | accept | reject | disconnect | remote <cmd> | allowim on|off | allowrestrictions on|off | on | off | release | status | restrictchat on|off | restrictim on|off | thoughts | forward | back | strafeleft | straferight | turnleft | turnright | jump | stop | say <text>");
            return true;
        }

        if (action == "ui")
        {
            LLFloaterReg::showInstance("blazing_storm_remote");
            return true;
        }

        if (action == "host")
        {
            actions.stopMovement();
            if (session.isActive())
            {
                session.emergencyRelease();
            }

            if (transport.startHost())
            {
                report("Local host started on 127.0.0.1:"
                    + std::to_string(transport.port())
                    + ". Pairing code: " + transport.pairingCode());
                report("On the controller viewer use: /blaze connect " + transport.pairingCode());
            }
            else
            {
                report(transport.lastStatus());
            }
            return true;
        }

        if (action == "request")
        {
            std::string subject_id;
            input >> subject_id;

            LLUUID subject_uuid(subject_id);
            if (subject_uuid.isNull())
            {
                report("Usage: /blaze request <subject-avatar-uuid>");
                return true;
            }

            std::string controller_name;
            LLAgentUI::buildFullname(controller_name);

            if (transport.requestController(subject_id, gAgentID.asString(), controller_name))
            {
                report("Possession request sent. Waiting for subject approval.");
            }
            else
            {
                report(transport.lastStatus());
            }
            return true;
        }

        if (action == "connect")
        {
            std::string code;
            input >> code;
            if (code.empty())
            {
                report("Usage: /blaze connect <pairing-code>");
                return true;
            }

            std::string controller_name;
            LLAgentUI::buildFullname(controller_name);

            if (transport.connectController(code, gAgentID.asString(), controller_name))
            {
                report("Connected to the local subject viewer. Waiting for subject approval.");
            }
            else
            {
                report(transport.lastStatus());
            }
            return true;
        }

        if (action == "accept")
        {
            if (transport.acceptPending())
            {
                report("Controller accepted. Movement + public chat permissions granted.");
                report("Controller IM permission is OFF by default. Use /blaze allowim on to grant it.");
                report("Controller restriction control is OFF by default. Use /blaze allowrestrictions on to grant it.");
            }
            else
            {
                report(transport.lastStatus());
            }
            return true;
        }

        if (action == "reject")
        {
            transport.rejectPending();
            report(transport.lastStatus());
            return true;
        }

        if (action == "disconnect")
        {
            actions.stopMovement();
            transport.disconnect();
            if (session.isActive())
            {
                session.emergencyRelease();
            }
            controller.end();
            report("Local viewer-to-viewer connection closed.");
            return true;
        }

        if (action == "remote")
        {
            if (!controller.isActive() || !transport.isPaired())
            {
                report("No accepted controller session is active.");
                return true;
            }

            std::string remote_action;
            input >> remote_action;

            RemoteCommandType type = RemoteCommandType::None;
            std::string target_id;
            std::string message;

            if (remote_action == "forward") type = RemoteCommandType::MoveForward;
            else if (remote_action == "back" || remote_action == "backward") type = RemoteCommandType::MoveBackward;
            else if (remote_action == "strafeleft") type = RemoteCommandType::StrafeLeft;
            else if (remote_action == "straferight") type = RemoteCommandType::StrafeRight;
            else if (remote_action == "turnleft") type = RemoteCommandType::TurnLeft;
            else if (remote_action == "turnright") type = RemoteCommandType::TurnRight;
            else if (remote_action == "up") type = RemoteCommandType::MoveUp;
            else if (remote_action == "down" || remote_action == "crouch") type = RemoteCommandType::MoveDown;
            else if (remote_action == "fly") type = RemoteCommandType::FlyOn;
            else if (remote_action == "land") type = RemoteCommandType::FlyOff;
            else if (remote_action == "flytoggle") type = RemoteCommandType::ToggleFly;
            else if (remote_action == "jump") type = RemoteCommandType::Jump;
            else if (remote_action == "stop") type = RemoteCommandType::Stop;
            else if (remote_action == "release") type = RemoteCommandType::EmergencyRelease;
            else if (remote_action == "say")
            {
                type = RemoteCommandType::Say;
                std::getline(input, message);
                if (!message.empty() && message.front() == ' ')
                {
                    message.erase(0, 1);
                }
            }
            else if (remote_action == "im")
            {
                type = RemoteCommandType::SendInstantMessage;
                input >> target_id;
                std::getline(input, message);
                if (!message.empty() && message.front() == ' ')
                {
                    message.erase(0, 1);
                }

                if (target_id.empty() || message.empty())
                {
                    report("Usage: /blaze remote im <avatar-uuid> <message>");
                    return true;
                }
            }
            else if (remote_action == "restrictmovement")
            {
                std::string state;
                input >> state;
                if (state == "on") type = RemoteCommandType::RestrictMovementOn;
                else if (state == "off") type = RemoteCommandType::RestrictMovementOff;
                else
                {
                    report("Usage: /blaze remote restrictmovement on|off");
                    return true;
                }
            }
            else if (remote_action == "restrictchat")
            {
                std::string state;
                input >> state;
                if (state == "on") type = RemoteCommandType::RestrictNearbyChatOn;
                else if (state == "off") type = RemoteCommandType::RestrictNearbyChatOff;
                else
                {
                    report("Usage: /blaze remote restrictchat on|off");
                    return true;
                }
            }
            else if (remote_action == "restrictim")
            {
                std::string state;
                input >> state;
                if (state == "on") type = RemoteCommandType::RestrictInstantMessageOn;
                else if (state == "off") type = RemoteCommandType::RestrictInstantMessageOff;
                else
                {
                    report("Usage: /blaze remote restrictim on|off");
                    return true;
                }
            }
            else
            {
                report("Remote commands: forward | back | strafeleft | straferight | turnleft | turnright | up | down/crouch | fly | land | flytoggle | jump | stop | say <text> | im <avatar-uuid> <text> | restrictmovement on|off | restrictchat on|off | restrictim on|off | release");
                return true;
            }

            RemoteCommand command = controller.makeCommand(type, message, target_id);
            if (!transport.sendCommand(command))
            {
                report(transport.lastStatus());
            }
            return true;
        }

        if (action == "on")
        {
            actions.stopMovement();
            session.begin("local-debug", localDebugPermissions());
            report("Local debug possession enabled (Movement + Chat).");
            return true;
        }

        if (action == "off")
        {
            actions.stopMovement();
            transport.disconnect();
            session.end();
            controller.end();
            report("Possession disabled and local transport disconnected.");
            return true;
        }

        if (action == "release")
        {
            // Local emergency release is never permission-gated.
            actions.stopMovement();
            transport.disconnect();
            session.emergencyRelease();
            controller.end();
            RemoteEvents::instance().clear();
            report("Emergency release: possession ended locally.");
            return true;
        }

        if (action == "status")
        {
            std::string role = "none";
            if (transport.role() == RemoteRole::Host) role = "host";
            else if (transport.role() == RemoteRole::Controller) role = "controller";

            report("Transport: role=" + role
                + "; connected=" + (transport.isConnected() ? "yes" : "no")
                + "; paired=" + (transport.isPaired() ? "yes" : "no")
                + "; status=" + transport.lastStatus());

            if (!session.isActive())
            {
                report("Subject session: inactive.");
            }
            else
            {
                report("Subject session: active; controller=" + session.controllerId()
                    + "; movement=" + (session.hasPermission(RemotePermission::Movement) ? "yes" : "no")
                    + "; chat=" + (session.hasPermission(RemotePermission::Chat) ? "yes" : "no")
                    + "; controller-im=" + (session.hasPermission(RemotePermission::InstantMessage) ? "yes" : "no")
                    + "; controller-restrictions=" + (session.hasPermission(RemotePermission::ManageSubjectRestrictions) ? "yes" : "no")
                    + "; subject-movement="
                    + (session.isSubjectRestricted(SubjectRestriction::Movement) ? "restricted" : "allowed")
                    + "; subject-nearby-chat="
                    + (session.isSubjectRestricted(SubjectRestriction::NearbyChat) ? "restricted" : "allowed")
                    + "; subject-direct-im="
                    + (session.isSubjectRestricted(SubjectRestriction::InstantMessage) ? "restricted" : "allowed")
                    + "; local-movement-debug="
                    + (session.subjectLocalMovementDisabled() ? "disabled" : "enabled"));
            }
            return true;
        }

        if (action == "allowim")
        {
            if (!session.isActive())
            {
                report("No subject possession session is active.");
                return true;
            }

            std::string state;
            input >> state;
            auto permissions = session.permissions();
            const auto direct_im = toMask(RemotePermission::InstantMessage);

            if (state == "on")
            {
                permissions |= direct_im;
                session.setPermissions(permissions);
                report("Controller may now send direct IMs as the subject.");
            }
            else if (state == "off")
            {
                permissions &= ~direct_im;
                session.setPermissions(permissions);
                report("Controller direct IM permission revoked.");
            }
            else
            {
                report("Usage: /blaze allowim on|off");
            }
            return true;
        }

        if (action == "allowrestrictions")
        {
            if (!session.isActive())
            {
                report("No subject possession session is active.");
                return true;
            }

            std::string state;
            input >> state;
            auto permissions = session.permissions();
            const auto manage_restrictions = toMask(RemotePermission::ManageSubjectRestrictions);

            if (state == "on")
            {
                permissions |= manage_restrictions;
                session.setPermissions(permissions);
                report("Controller may now change subject nearby-chat and direct-IM restrictions.");
            }
            else if (state == "off")
            {
                permissions &= ~manage_restrictions;
                session.setPermissions(permissions);
                report("Controller subject-restriction permission revoked.");
            }
            else
            {
                report("Usage: /blaze allowrestrictions on|off");
            }
            return true;
        }

        if (!requireSession())
        {
            return true;
        }

        if (action == "restrictchat")
        {
            std::string state;
            input >> state;

            auto restrictions = session.subjectRestrictions();
            const auto nearby_chat = toMask(SubjectRestriction::NearbyChat);

            if (state == "on")
            {
                restrictions |= nearby_chat;
                session.setSubjectRestrictions(restrictions);
                report("Subject public nearby chat is now diverted to controller thoughts.");
            }
            else if (state == "off")
            {
                restrictions &= ~nearby_chat;
                session.setSubjectRestrictions(restrictions);
                report("Subject public nearby chat is now allowed.");
            }
            else
            {
                report("Usage: /blaze restrictchat on|off");
            }
            return true;
        }

        if (action == "restrictim")
        {
            std::string state;
            input >> state;

            auto restrictions = session.subjectRestrictions();
            const auto direct_im = toMask(SubjectRestriction::InstantMessage);

            if (state == "on")
            {
                restrictions |= direct_im;
                session.setSubjectRestrictions(restrictions);
                report("Subject direct IM sending is now diverted to controller thoughts.");
            }
            else if (state == "off")
            {
                restrictions &= ~direct_im;
                session.setSubjectRestrictions(restrictions);
                report("Subject direct IM sending is now allowed.");
            }
            else
            {
                report("Usage: /blaze restrictim on|off");
            }
            return true;
        }

        if (action == "thoughts")
        {
            auto events = RemoteEvents::instance().takeAll();
            if (events.empty())
            {
                report("No pending remote events.");
            }
            for (const auto& event : events)
            {
                if (event.type == RemoteEventType::Thought)
                {
                    report("Pending thought: " + event.text);
                }
            }
            return true;
        }

        bool ok = false;

        if (action == "forward") ok = actions.moveForward();
        else if (action == "back" || action == "backward") ok = actions.moveBackward();
        else if (action == "strafeleft") ok = actions.strafeLeft();
        else if (action == "straferight") ok = actions.strafeRight();
        else if (action == "turnleft") ok = actions.turnLeft();
        else if (action == "turnright") ok = actions.turnRight();
        else if (action == "jump") ok = actions.jump();
        else if (action == "stop")
        {
            actions.stopMovement();
            ok = true;
        }
        else if (action == "say")
        {
            std::string message;
            std::getline(input, message);
            if (!message.empty() && message.front() == ' ')
            {
                message.erase(0, 1);
            }
            ok = actions.say(message);
        }
        else
        {
            report("Unknown command. Use /blaze help.");
            return true;
        }

        if (!ok)
        {
            report("Command was blocked by the current session permissions or viewer state.");
        }

        return true;
    }
}
