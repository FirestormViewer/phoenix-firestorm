/**
* @file llcrashlookup.cpp
* @brief Base crash analysis class
*
* Copyright (C) 2011, Kitty Barnett
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
*/

#include "linden_common.h"

#include "llcrashlookup.h"

std::string LLCrashLookup::getModuleVersionString() const
{
    std::string strVersion = llformat("%d.%d.%d.%d",
        (U32)(m_nModuleVersion >> 48), (U32)((m_nModuleVersion >> 32) & 0xFFFF),
        (U32)((m_nModuleVersion >> 16) & 0xFFFF), (U32)(m_nModuleVersion & 0xFFFF));
    return strVersion;
}
