/**
 * @file bsremotetransport.h
 * @brief Transport abstraction for controller/host communication.
 *
 * The transport carries only Blazing Storm pairing/session messages. It must
 * never carry Second Life usernames, passwords, login tokens or session keys.
 */

#ifndef BS_REMOTE_TRANSPORT_H
#define BS_REMOTE_TRANSPORT_H

#include "blazingstorm/remote/bsremoteprotocol.h"

#include <functional>
#include <string>

namespace BlazingStorm
{
    class IRemoteTransport
    {
    public:
        using PairingHandler = std::function<void(const PairingRequest&)>;
        using CommandHandler = std::function<void(const RemoteCommand&)>;

        virtual ~IRemoteTransport() = default;

        virtual bool startHost(const std::string& pairing_code) = 0;
        virtual bool connectController(const std::string& pairing_code) = 0;
        virtual void disconnect() = 0;

        virtual bool sendCommand(const RemoteCommand& command) = 0;

        virtual void setPairingHandler(PairingHandler handler) = 0;
        virtual void setCommandHandler(CommandHandler handler) = 0;
    };
}

#endif // BS_REMOTE_TRANSPORT_H
