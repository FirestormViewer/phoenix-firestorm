/**
 * @file bscontrollerinput.cpp
 * @brief Routes normal viewer movement input to a possessed subject.
 */

#include "llviewerprecompiledheaders.h"

#include "blazingstorm/remote/bscontrollerinput.h"

#include "blazingstorm/remote/bslocaltransport.h"
#include "blazingstorm/remote/bsremotecontroller.h"

namespace BlazingStorm
{
    bool ControllerInput::routeMovement(RemoteCommandType press_command,
                                        RemoteCommandType release_command,
                                        bool key_down,
                                        bool key_up)
    {
        auto& controller = RemoteController::instance();
        auto& transport = LocalTransport::instance();

        if (!controller.isActive() || !transport.isPaired())
        {
            return false;
        }

        if (controller.controlsSubject())
        {
            if (key_down)
            {
                transport.sendCommand(controller.makeCommand(press_command));
            }
            else if (key_up && release_command != RemoteCommandType::None)
            {
                transport.sendCommand(controller.makeCommand(release_command));
            }
        }

        return !controller.controlsController();
    }
}
