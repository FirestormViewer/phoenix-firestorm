/** 
* @file llcrashlookupwindows.h
* @brief Basic Windows crash analysis
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

#ifndef LLCRASHLOOKUPWINDOWS_H
#define LLCRASHLOOKUPWINDOWS_H

#ifdef LL_SEND_CRASH_REPORTS

#include <DbgEng.h>
#include "llcrashlookup.h"

class LLCrashLookupWindows : public LLCrashLookup
{
public:
	LLCrashLookupWindows();
	virtual ~LLCrashLookupWindows();

public:
	/*virtual*/ bool initFromDump(const std::string& strDumpPath);

protected:
	IDebugClient*	m_pDbgClient;
	IDebugClient4*	m_pDbgClient4;
	IDebugControl4*	m_pDbgControl;
	IDebugSymbols2*	m_pDbgSymbols;
};
#else

#endif // LL_SEND_CRASH_REPORTS

#endif // LLCRASHLOOKUP_H
