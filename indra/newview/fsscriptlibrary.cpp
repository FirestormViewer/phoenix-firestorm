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

#include "lluictrlfactory.h"
#include "llxmlnode.h"

LLScriptLibrary::LLScriptLibrary()
{
}

bool LLScriptLibrary::loadLibrary(const std::string& filename)
{
    LLXMLNodePtr xml_root;
    if ( (!LLUICtrlFactory::getLayeredXMLNode(filename, xml_root)) || (xml_root.isNull()) || (!xml_root->hasName("script_library")) )
    {
        LL_WARNS() << "Could not read the script library (" << filename << ")" << LL_ENDL;
        return false;
    }
    LL_INFOS() << "Loading script library at: " << filename << LL_ENDL;
    for (LLXMLNode* pNode = xml_root->getFirstChild(); pNode != NULL; pNode = pNode->getNextSibling())
    {
        if (pNode->hasName("functions"))
        {
            std::string function;
            for (LLXMLNode* pStringNode = pNode->getFirstChild(); pStringNode != NULL; pStringNode = pStringNode->getNextSibling())
            {
                if (!pStringNode->getAttributeString("name", function) ||
                    !pStringNode->hasName("function"))
                {
                    continue;
                }
                bool god_only = false;
                std::string tool_tip;
                F32 sleep_time;
                F32 energy;
                if (!pStringNode->getAttribute_bool("god-only", god_only))
                {
                    god_only = false;
                }
                if (!pStringNode->getAttributeString("desc", tool_tip))
                {
                    // TODO: Should this be localized? Tooltips aren't translated. <FS:CR>
                    tool_tip = "No Description available";
                }
                if (!pStringNode->getAttributeF32("sleep", sleep_time))
                {
                    sleep_time = 0;
                }
                if (!pStringNode->getAttributeF32("energy", energy))
                {
                    energy = 10;
                }
                addFunction(function, tool_tip, sleep_time, energy, god_only);
            }
        }
    }
    return true;
}

void LLScriptLibrary::addFunction(std::string name, std::string desc, F32 sleep, F32 energy, bool god_only)
{
    LLScriptLibraryFunction func(name, desc, sleep, energy, god_only);
    mFunctions.push_back(func);
}

LLScriptLibraryFunction::LLScriptLibraryFunction(std::string name, std::string desc, F32 sleep, F32 energy, bool god_only)
:   mName(name),
mDesc(desc),
mSleepTime(sleep),
mEnergy(energy),
mGodOnly(god_only)
{
}

LLScriptLibrary gScriptLibrary;
