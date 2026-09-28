/**
 * @file bsremoteprotocol.h
 * @brief Transport-neutral messages for Blazing Storm possession sessions.
 */

#ifndef BS_REMOTE_PROTOCOL_H
#define BS_REMOTE_PROTOCOL_H

#include <cstdint>
#include <string>

namespace BlazingStorm
{
    enum class RemoteRole
    {
        None,
        Host,
        Controller
    };

    enum class RemoteCommandType
    {
        None,
        MoveForward,
        MoveBackward,
        StrafeLeft,
        StrafeRight,
        TurnLeft,
        TurnRight,
        Jump,
        StopForward,
        StopStrafe,
        StopTurn,
        Stop,
        Say,
        SendInstantMessage,
        RestrictMovementOn,
        RestrictMovementOff,
        RestrictNearbyChatOn,
        RestrictNearbyChatOff,
        RestrictInstantMessageOn,
        RestrictInstantMessageOff,
        EmergencyRelease
    };

    struct RemoteCommand
    {
        RemoteCommandType type = RemoteCommandType::None;
        std::string targetId;
        std::string text;
        std::uint64_t sequence = 0;
    };

    struct PairingRequest
    {
        std::string code;
        std::string controllerId;
        std::string controllerName;
    };

    struct PairingResponse
    {
        bool accepted = false;
        std::string sessionId;
        std::string reason;
    };
}

#endif // BS_REMOTE_PROTOCOL_H
