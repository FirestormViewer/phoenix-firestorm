/**
 * @file pieautohide.h
 * @brief Pie menu autohide base class
 *
 * $LicenseInfo:firstyear=2024&license=fsviewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (C) 2024, The Phoenix Firestorm Project, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * The Phoenix Firestorm Project, Inc., 1831 Oakwood Drive, Fairmont, Minnesota 56031-3225 USA
 * http://www.firestormviewer.org
 * $/LicenseInfo$
 */

#ifndef PIEAUTOHIDE_H
#define PIEAUTOHIDE_H

#include "lluictrl.h"

// A slice in the pie that supports the auto-hide function.
class PieAutohide
{
public:
    PieAutohide(bool autohide, bool startAutohide);

    // accessor to expose the autohide feature
    bool getStartAutohide() const;
    bool getAutohide() const;

protected:
    bool mStartAutohide{ false };
    bool mAutohide{ false };
};

#endif // PIEAUTOHIDE_H
