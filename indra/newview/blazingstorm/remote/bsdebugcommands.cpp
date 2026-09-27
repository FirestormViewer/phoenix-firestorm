/**
 * @file bsdebugcommands.cpp
 * @brief Local chat commands used to exercise Blazing Storm remote control.
 */

#include "llviewerprecompiledheaders.h"

#include "blazingstorm/remote/bsdebugcommands.h"

#include "blazingstorm/remote/bsremoteactions.h"
#include "blazingstorm/remote/bsremoteevents.h"
#include "blazingstorm/remote/bsremotesession.h"
#include "fscommon.h"

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

        if (action.empty() || action == "help")
        {
            report("Commands: /blaze on | off | release | status | restrictchat on|off | thoughts | forward | back | strafeleft | straferight | turnleft | turnright | jump | stop | say <text>");
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
            session.end();
            report("Local debug possession disabled.");
            return true;
        }

        if (action == "release")
        {
            // Local emergency release is never permission-gated.
            actions.stopMovement();
            session.emergencyRelease();
            RemoteEvents::instance().clear();
            report("Emergency release: possession ended locally.");
            return true;
        }

        if (action == "status")
        {
            if (!session.isActive())
            {
                report("Remote session: inactive.");
            }
            else
            {
                report("Remote session: active; controller=" + session.controllerId()
                    + "; movement=" + (session.hasPermission(RemotePermission::Movement) ? "yes" : "no")
                    + "; chat=" + (session.hasPermission(RemotePermission::Chat) ? "yes" : "no")
                    + "; subject-nearby-chat="
                    + (session.isSubjectRestricted(SubjectRestriction::NearbyChat) ? "restricted" : "allowed"));
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
