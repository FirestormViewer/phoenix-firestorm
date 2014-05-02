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

#include "llviewerprecompiledheaders.h"
#include <leap-motion/Leap.h>

#include "fsleapexampletool.h"

namespace nd
{
	namespace leap
	{
		std::string ExampleTool::getName()
		{
			return "Example tool";
		}

		std::string ExampleTool::getDebugString()
		{
			std::stringstream strm;
			strm << mHands << " hands deteced; " << mFingers << " fingers in total";
			return strm.str();
		}

		S32 ExampleTool::getId()
		{
			return 111;
		}

		void ExampleTool::onLeapFrame( Leap::Frame const &aFrame )
		{
			mHands = aFrame.hands().count();
			mFingers = 0;
			for( int i = 0; i < mHands; ++ i )
				mFingers += aFrame.hands()[ i ].fingers().count();
		}

		void ExampleTool::onRenderFrame( Leap::Frame const &aFrame )
		{
		}

		void ExampleTool::render()
		{
		}
	}
}