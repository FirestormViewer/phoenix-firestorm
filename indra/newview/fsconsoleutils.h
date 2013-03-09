/**
 * @file fsconsoleutils.h
 * @brief Class for console-related functions in Firestorm
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

#ifndef FS_CONSOLEUTILS_H
#define FS_CONSOLEUTILS_H

#include "llavatarnamecache.h"
#include "llviewerchat.h"

class FSConsoleUtils
{
public:

	static bool ProcessChatMessage(const LLChat& chat_msg, const LLSD &args);
	static bool ProcessInstantMessage(const LLUUID& session_id, const LLUUID& from_id, const std::string& message);

protected:

	static void onProcessChatAvatarNameLookup(const LLUUID& agent_id, const LLAvatarName& av_name, const LLChat& chat_msg);
	static void onProccessInstantMessageNameLookup(const LLUUID& agent_id, const LLAvatarName& av_name, const std::string& message_str, const std::string& group);

};

#endif // FS_CONSOLEUTILS_H