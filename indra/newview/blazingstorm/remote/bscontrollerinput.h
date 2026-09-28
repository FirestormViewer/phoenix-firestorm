/**
 * @file bscontrollerinput.h
 * @brief Routes normal viewer movement input to a possessed subject.
 */

#ifndef BS_CONTROLLER_INPUT_H
#define BS_CONTROLLER_INPUT_H

#include "blazingstorm/remote/bsremoteprotocol.h"

namespace BlazingStorm
{
    class ControllerInput final
    {
    public:
        // Returns true when the local controller avatar movement should be
        // suppressed for this input event (Subject-only mode).
        static bool routeMovement(RemoteCommandType press_command,
                                  RemoteCommandType release_command,
                                  bool key_down,
                                  bool key_up);
    };
}

#endif // BS_CONTROLLER_INPUT_H
