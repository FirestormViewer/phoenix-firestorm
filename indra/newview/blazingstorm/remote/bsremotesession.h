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
        InstantMessage = 1u << 7,
        ManageSubjectRestrictions = 1u << 8
        // Deliberately no Money permission. A controller may never perform
        // payments, purchases, tips or other money-bearing actions.
    };

    enum class SubjectRestriction : std::uint32_t
    {
        None          = 0,
        Movement      = 1u << 0,
        NearbyChat    = 1u << 1,
        Touch         = 1u << 2,
        SitStand      = 1u << 3,
        Teleport      = 1u << 4,
        Camera        = 1u << 5,
        Inventory     = 1u << 6,
        InstantMessage= 1u << 7
        // Emergency release is intentionally not represented here.
    };

    using RemotePermissionMask = std::uint32_t;
    using SubjectRestrictionMask = std::uint32_t;

    constexpr RemotePermissionMask toMask(RemotePermission permission)
    {
        return static_cast<RemotePermissionMask>(permission);
    }

    constexpr SubjectRestrictionMask toMask(SubjectRestriction restriction)
    {
        return static_cast<SubjectRestrictionMask>(restriction);
    }

    constexpr RemotePermissionMask operator|(RemotePermission lhs, RemotePermission rhs)
    {
        return toMask(lhs) | toMask(rhs);
    }

    class RemoteSession final
    {
    public:
        static RemoteSession& instance();

        void begin(std::string controller_id,
                   RemotePermissionMask permissions,
                   SubjectRestrictionMask restrictions = 0);
        void end();

        bool isActive() const;
        const std::string& controllerId() const;

        RemotePermissionMask permissions() const;
        void setPermissions(RemotePermissionMask permissions);
        bool hasPermission(RemotePermission permission) const;

        SubjectRestrictionMask subjectRestrictions() const;
        void setSubjectRestrictions(SubjectRestrictionMask restrictions);
        bool isSubjectRestricted(SubjectRestriction restriction) const;

        // Always succeeds locally and is intentionally outside the permission
        // and restriction systems.
        void emergencyRelease();

    private:
        RemoteSession() = default;

        static RemotePermissionMask sanitizePermissions(RemotePermissionMask permissions);

        bool mActive = false;
        std::string mControllerId;
        RemotePermissionMask mPermissions = 0;
        SubjectRestrictionMask mSubjectRestrictions = 0;
    };
}

#endif // BS_REMOTE_SESSION_H
