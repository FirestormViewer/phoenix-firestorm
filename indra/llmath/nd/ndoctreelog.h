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

#ifndef NDOCTREELOG_H_
#define NDOCTREELOG_H_

#include "stdtypes.h"
#include <sstream>

namespace nd
{
    namespace octree
    {
        namespace debug
        {
            extern U32 gOctreeDebug;
            void doOctreeLog( std::string const &aStr );
            void checkOctreeLog();
            void setOctreeLogFilename( std::string const & );
        }
    }
}

#define ND_OCTREE_LOG { if( nd::octree::debug::gOctreeDebug ){ nd::octree::debug::checkOctreeLog(); std::stringstream strm; strm << std::setprecision(10)
#define ND_OCTREE_LOG_END std::endl; nd::octree::debug::doOctreeLog( strm.str() ); } }

#endif
