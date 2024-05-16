#ifndef NDETW_H
#define NDETW_H

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

#include "llpreprocessor.h"
#include "lltimer.h"

namespace nd
{
    namespace etw
    {
#ifdef LL_WINDOWS
        typedef bool (_cdecl *tpInitialize)(void);

        typedef void (_cdecl *tpLogFrame)(double);
        typedef void (_cdecl *tpLogStartTask)(wchar_t const*);
        typedef void (_cdecl *tpLogEndTask)(wchar_t const*);
        typedef void (_cdecl *tpLogTickTask)(wchar_t const*);

        extern tpLogFrame pLogFrame;
        extern tpLogStartTask pLogStartTask;
        extern tpLogEndTask pLogEndTask;
        extern tpLogTickTask pLogTickTask;

        extern LLTimer etwFrameTimer;

        void init();

        inline void logFrame( )
        {
            if( pLogFrame )
                (*pLogFrame)( etwFrameTimer.getElapsedTimeAndResetF64() );
        }

        inline void logTaskStart( wchar_t const *aTask )
        {
            if( pLogStartTask )
                (*pLogStartTask)( aTask );
        }

        inline void logTaskEnd( wchar_t const *aTask )
        {
            if( pLogEndTask )
                (*pLogEndTask)( aTask );
        }

        inline void tickTask( wchar_t const *aTask )
        {
            if( pLogTickTask )
                (*pLogTickTask)( aTask );
        }
#else
        inline void init()
        { }

        inline void logFrame( )
        { }

        inline void logTaskStart( wchar_t const *aTask )
        { }

        inline void logTaskEnd( wchar_t const *aTask )
        { }

        inline void tickTask( wchar_t const *aTask )
        { }
#endif
    }
}
#endif
