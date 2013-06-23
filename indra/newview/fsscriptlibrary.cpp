/*
 * @file fsscriptlibrary.cpp
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

#include "llviewerprecompiledheaders.h"
#include "fsscriptlibrary.h"

#include "llsdserialize.h"

LLScriptLibrary::LLScriptLibrary()
{
}

void LLScriptLibrary::loadLibrary(const std::string& filename)
{
	llinfos << "Loading script library from " << filename << llendl;
	llifstream file(filename);
	LLSD func_list;
	if (file.is_open())
	{
		if(LLSDSerialize::fromXMLDocument(func_list, file) != LLSDParser::PARSE_FAILURE)
		{
			for(LLSD::map_iterator func_itr = func_list.beginMap(); func_itr != func_list.endMap(); ++func_itr)
			{
				LLSD const & func_map = func_itr->second;
				bool god_only = false;
				if (func_map.has("gods-only"))
				{
					god_only = func_map["gods-only"].asBoolean();
				}
				addFunction(func_itr->first,
							func_map["desc"].asString(),
							god_only);
			}
			file.close();
		}
		else
		{
			llwarns << "Failed to parse " << filename << llendl;
			file.close();
		}
	}
	else
	{
		llwarns << "Failed to open " << filename << llendl;
	}
}

void LLScriptLibrary::addFunction(std::string name, std::string desc, bool god_only)
{
    LLScriptLibraryFunction func(name, desc, god_only);
    mFunctions.push_back(func);
}

LLScriptLibraryFunction::LLScriptLibraryFunction(std::string name, std::string desc, bool god_only)
:	mName(name),
mDesc(desc),
mGodOnly(god_only)
{
}

LLScriptLibrary gScriptLibrary;
