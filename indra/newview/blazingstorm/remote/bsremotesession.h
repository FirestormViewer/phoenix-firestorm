/**
 * @file bsremotesession.h
 * @brief Blazing Storm remote-control session state and permission model.
 *
 * This file is part of the Blazing Storm viewer fork.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 */

#ifndef BS_REMOTE_SESSION_H
#define BS_REMOTE_SESSION_H

#include <cstdint>
#include <string>

namespace BlazingStorm
{
    enum class RemotePermission : std::uint32_t
    {
        None      = 0,
        Movement  = 1u << 0,
        Chat      = 1u << 1,
        Touch     = 1u << 2,
        SitStand  = 1u << 3,
        Teleport  = 1u << 4,
        Camera    = 1u << 5,
        Inventory = 1u << 6,
        Money     = 1u << 7
    };

    using RemotePermissionMask = std::uint32_t;

    constexpr RemotePermissionMask toMask(RemotePermission permission)
    {
        return static_cast<RemotePermissionMask>(permission);
    }

    constexpr RemotePermissionMask operator|(RemotePermission lhs, RemotePermission rhs)
    {
        return toMask(lhs) | toMask(rhs);
    }

    class RemoteSession final
    {
    public:
        static RemoteSession& instance();

        void begin(std::string controller_id, RemotePermissionMask permissions);
        void end();

        bool isActive() const;
        const std::string& controllerId() const;
        RemotePermissionMask permissions() const;
        bool hasPermission(RemotePermission permission) const;

    private:
        RemoteSession() = default;

        bool mActive = false;
        std::string mControllerId;
        RemotePermissionMask mPermissions = 0;
    };
}

#endif // BS_REMOTE_SESSION_H
