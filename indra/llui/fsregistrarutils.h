/**
 * @file fsregistrarutils.h
 * @brief Utility class to allow registrars access information from dependent projects
 *
 * $LicenseInfo:firstyear=2013&license=viewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (c) 2013 Ansariel Hiller @ Second Life
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

#ifndef FS_REGISTRARUTILS_H
#define FS_REGISTRARUTILS_H

#include <boost/function.hpp>

enum EFSRegistrarFunctionActionType
{
	FS_RGSTR_ACT_ADD_FRIEND,
	FS_RGSTR_ACT_REMOVE_FRIEND,
	FS_RGSTR_ACT_SEND_IM,
	FS_RGSTR_ACT_VIEW_TRANSCRIPT,
	FS_RGSTR_ACT_ZOOM_IN,
	FS_RGSTR_ACT_OFFER_TELEPORT,
	FS_RGSTR_ACT_SHOW_PROFILE,
	FS_RGSTR_ACT_TRACK_AVATAR,
	FS_RGSTR_ACT_TELEPORT_TO,
	FS_RGSTR_ACT_REQUEST_TELEPORT,
	FS_RGSTR_CHK_AVATAR_BLOCKED,
	FS_RGSTR_CHK_IS_SELF,
	FS_RGSTR_CHK_IS_NOT_SELF,
	FS_RGSTR_CHK_WAITING_FOR_GROUP_DATA,
	FS_RGSTR_CHK_HAVE_GROUP_DATA,
	FS_RGSTR_CHK_CAN_LEAVE_GROUP,
	FS_RGSTR_CHK_CAN_JOIN_GROUP,
	FS_RGSTR_CHK_GROUP_NOT_ACTIVE
};

class FSRegistrarUtils
{
public:
	FSRegistrarUtils();
	~FSRegistrarUtils() { };

	typedef boost::function<bool(const LLUUID&, EFSRegistrarFunctionActionType)> enable_check_function_t;
	void setEnableCheckFunction(const enable_check_function_t& func)
	{
		mEnableCheckFunction = func;
	}

	bool checkIsEnabled(LLUUID av_id, EFSRegistrarFunctionActionType action);

private:
	enable_check_function_t mEnableCheckFunction;
};

extern FSRegistrarUtils gFSRegistrarUtils;

#endif // FS_REGISTRARUTILS_H
