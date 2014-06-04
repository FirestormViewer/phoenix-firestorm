/**
 * $LicenseInfo:firstyear=2014&license=fsviewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (C) 2014, Nicky Dasmijn
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

#include "ndetw.h"

#ifdef LL_WINDOWS
#include <Windows.h>

namespace nd
{
	namespace etw
	{
		HMODULE hETW;
		tpLogFrame pLogFrame;
		tpLogStartTask pLogStartTask;
		tpLogEndTask pLogEndTask;
		tpLogTickTask pLogTickTask;
		
		LLTimer etwFrameTimer;

		void init()
		{
			pLogFrame = NULL;
			pLogStartTask = NULL;
			pLogEndTask = NULL;
			pLogTickTask = NULL;
			
			// This is okay for now. We'll only need that for measuring under well defined circumstances.
			// Maybe create an installer for this one day (very low prio) that copies and registers the provider.

			hETW = ::LoadLibrary( L"c:\\xperf\\fs_etw.dll" );
			if( hETW )
			{
				tpInitialize pInitialize = reinterpret_cast< tpInitialize>( ::GetProcAddress( hETW, "initialize" ) );
				if( pInitialize && (*pInitialize)() )
				{
					pLogFrame = reinterpret_cast< tpLogFrame >( ::GetProcAddress( hETW, "log_beginFrame" ) );
					pLogStartTask = reinterpret_cast< tpLogStartTask >( ::GetProcAddress( hETW, "log_startTask" ) );
					pLogEndTask = reinterpret_cast< tpLogEndTask >( ::GetProcAddress( hETW, "log_endTask" ) );
					pLogTickTask = reinterpret_cast< tpLogTickTask >( ::GetProcAddress( hETW, "log_tickTask" ) );
					etwFrameTimer.reset();
				}
			}
		}
	}
}
#endif
