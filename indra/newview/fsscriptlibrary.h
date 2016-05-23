/*
 * @file fsscriptlibrary.h
 * @brief Dynamically loaded script library
 *
 * $LicenseInfo:firstyear=2013&license=fsviewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (C) 2013, Cinder Roxley <cinder.roxley@phoenixviewer.com>
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

#ifndef FS_SCRIPT_LIBRARY_H
#define FS_SCRIPT_LIBRARY_H

class LLScriptLibraryFunction
{
public:
    LLScriptLibraryFunction(std::string name, std::string desc, F32 sleep, F32 energy, bool god_only = false);
	
    std::string mName;
	std::string mDesc;
	F32 mSleepTime;
	F32 mEnergy;
    bool mGodOnly;
};

class LLScriptLibrary
{
public:
    LLScriptLibrary();
	bool loadLibrary(const std::string& filename);
	
	std::vector<LLScriptLibraryFunction> mFunctions;
	
private:
    void addFunction(std::string name, std::string desc, F32 sleep, F32 energy, bool god_only = false);
};

extern LLScriptLibrary gScriptLibrary;

#endif // FS_SCRIPT_LIBRARY_H
