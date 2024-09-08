/**
 * @file fsfloaterwhitelisthelper.h
 * @brief Helper tool header file to display paths to whitelist in antivirus tools
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

#ifndef FSFLOATERWHITELISTHELPER_H
#define FSFLOATERWHITELISTHELPER_H

#include "llfloater.h"

class FSFloaterWhiteListHelper : public LLFloater
{
public:
    explicit FSFloaterWhiteListHelper(const LLSD& key);
    ~FSFloaterWhiteListHelper() final = default;

    bool postBuild() override final;

private:
    void populateWhitelistInfo();
};
#endif // FSFLOATERWHITELISTHELPER_H
