/**
 * @file fsfloaterprofile.h
 * @brief Legacy Profile Floater
 *
 * $LicenseInfo:firstyear=2012&license=fsviewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (C) 2012, Kadah Coba
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

#ifndef FS_FSFLOATERPROFILE_H
#define FS_FSFLOATERPROFILE_H

#include "llfloater.h"

class LLAvatarName;

class FSFloaterProfile : public LLFloater
{
    LOG_CLASS(FSFloaterProfile);
public:
    FSFloaterProfile(const LLSD& key);
    virtual ~FSFloaterProfile();

    /*virtual*/ void onOpen(const LLSD& key);

    /*virtual*/ BOOL postBuild();

    /**
     * Returns avatar ID.
     */
    const LLUUID& getAvatarId() const { return mAvatarId; }

protected:
    /**
     * Sets avatar ID, sets panel as observer of avatar related info replies from server.
     */
    void setAvatarId(const LLUUID& avatar_id) { mAvatarId = avatar_id; }

    void onOKBtn();
    void onCancelBtn();

private:
    void onAvatarNameCache(const LLUUID& agent_id, const LLAvatarName& av_name);

    LLUUID mAvatarId;
};

#endif // FS_FSFLOATERPROFILE_H
