/**
 * @file bsremotesession.cpp
 * @brief Blazing Storm remote-control session state and permission model.
 *
 * This file is part of the Blazing Storm viewer fork.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 */

#include "llviewerprecompiledheaders.h"

#include "blazingstorm/remote/bsremotesession.h"

#include <utility>

namespace BlazingStorm
{
    RemoteSession& RemoteSession::instance()
    {
        static RemoteSession session;
        return session;
    }

    RemotePermissionMask RemoteSession::sanitizePermissions(RemotePermissionMask permissions)
    {
        const RemotePermissionMask allowed =
              toMask(RemotePermission::Movement)
            | toMask(RemotePermission::Chat)
            | toMask(RemotePermission::Touch)
            | toMask(RemotePermission::SitStand)
            | toMask(RemotePermission::Teleport)
            | toMask(RemotePermission::Camera)
            | toMask(RemotePermission::Inventory)
            | toMask(RemotePermission::InstantMessage)
            | toMask(RemotePermission::ManageSubjectRestrictions);

        return permissions & allowed;
    }

    void RemoteSession::begin(std::string controller_id,
                              RemotePermissionMask permissions,
                              SubjectRestrictionMask restrictions)
    {
        mControllerId = std::move(controller_id);
        mPermissions = sanitizePermissions(permissions);
        mSubjectRestrictions = restrictions;
        mActive = true;
    }

    void RemoteSession::end()
    {
        mActive = false;
        mControllerId.clear();
        mPermissions = 0;
        mSubjectRestrictions = 0;
    }

    bool RemoteSession::isActive() const
    {
        return mActive;
    }

    const std::string& RemoteSession::controllerId() const
    {
        return mControllerId;
    }

    RemotePermissionMask RemoteSession::permissions() const
    {
        return mPermissions;
    }

    void RemoteSession::setPermissions(RemotePermissionMask permissions)
    {
        mPermissions = sanitizePermissions(permissions);
    }

    bool RemoteSession::hasPermission(RemotePermission permission) const
    {
        return mActive && (mPermissions & toMask(permission)) != 0;
    }

    SubjectRestrictionMask RemoteSession::subjectRestrictions() const
    {
        return mSubjectRestrictions;
    }

    void RemoteSession::setSubjectRestrictions(SubjectRestrictionMask restrictions)
    {
        mSubjectRestrictions = restrictions;
    }

    bool RemoteSession::isSubjectRestricted(SubjectRestriction restriction) const
    {
        return mActive && (mSubjectRestrictions & toMask(restriction)) != 0;
    }

    void RemoteSession::emergencyRelease()
    {
        end();
    }
}
