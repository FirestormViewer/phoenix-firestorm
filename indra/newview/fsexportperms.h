/*
 * @file fsexportperms.h
 * @brief Export permissions check definitions
 * @authors Cinder Biscuits
 *
 * $LicenseInfo:firstyear=2013&license=LGPLV2.1$
 * Copyright (C) 2013 Cinder Biscuits <cinder.roxley@phoenixviewer.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA
 */

#ifndef FS_EXPORTPERMS_H
#define FS_EXPORTPERMS_H

#include "llselectmgr.h"

const S32 OXP_FORMAT_VERSION = 2;

namespace FSExportPermsCheck
{
    bool canExportNode(LLSelectNode* node, bool dae);
    bool canExportAsset(LLUUID asset_id, std::string* name = NULL, std::string* description = NULL);
};

#endif // FS_EXPORTPERMS_H
