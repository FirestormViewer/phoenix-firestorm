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

#ifndef NDLEAPMANIPTOOL_H
#define NDLEAPMANIPTOOL_H

#pragma once

#include "fsleaptool.h"
#include <string.h>

class LLViewerObject;

namespace nd
{
	namespace leap
	{
		struct Finger;
		struct Fingers;
		class ManipTool: public Tool
		{
			enum EMAXKEPTFRAMES{ eMaxKeptFrames = 120 };

			inline U16 getNextFrameNo( U16 aFrame ) const
			{ return aFrame==eMaxKeptFrames-1?0:aFrame+1; }
			inline U16 getPrevFrameNo( U16 aFrame ) const
			{ return aFrame==0?eMaxKeptFrames-1:aFrame-1; }

            inline U32 getMaxBacktrackMicroseconds() const
			{ return 1500*1000; }

			U64 mLastExaminedFrame;
			U16 mLastStoredFrame;
			U16 mNextRenderedFrame;
			U64 mTotalStoredFrames;

			Fingers *mFingersPerFrame;

			void clearSelection();
			void doSelect();

			void selectWithFinger( U16 aIndex );
			void findPartner( U16 aIndex );

			void renderCone( Finger const & );
			void renderMovementDirection( Finger const & );
			void renderMovementAngle( Finger const &, U16 aIndex );
			void renderFinger( Finger const&, U16 aIndex );

		public:
			ManipTool();
			virtual ~ManipTool();

			virtual void onLeapFrame( Leap::Frame const& );
			virtual void onRenderFrame( Leap::Frame const& );
			virtual void render();
			virtual std::string getDebugString();
			virtual std::string getName();
			virtual S32 getId();
		};
	}
}

#endif
