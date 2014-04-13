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

#include "fsleapmaniptool.h"

#include "llviewerwindow.h"
#include "llviewerobject.h"
#include "lldrawable.h"
#include "pipeline.h"
#include "llvoavatarself.h"
#include "llviewershadermgr.h"
#include "llselectmgr.h"

namespace nd
{
	namespace leap
	{
			struct Finger
			{
				Finger(  )
				{
					clear();
				}

				void clear()
				{
					memset( this, 0, sizeof( Finger ) );
				}

				bool isValid( ) const
				{ return mTimestamp > 0; }

				U64 mId;
				U64 mTimestamp;

				float mTip[3];
				float mDir[3];
				float mFromLast[3];
				float mLenFromLast;
				float mWidth;
				float mLength;
				U16 mPartner;
				LLViewerObject *mSelected;
			};

			struct Fingers
			{
				Fingers( )
				{
					clear();
				}

				void clear()
				{
					mTimestamp = 0;
					for( int i = 0; i < eMaxFingers; ++i )
						mFingers[ i ].clear();
					mStoredFingers = 0;
				}

				Finger* getFinger( U64 aId )
				{
					for( int i = 0; i < mStoredFingers; ++i )
					{
						if( aId == mFingers[ i ].mId )
							return mFingers + i;
					}

					return 0;
				}

				Finger* at( U16 aIndex )
			    {
                    return &mFingers[ aIndex ];
				}

				enum MAXFIGERS { eMaxFingers = 10 };

				U64 mTimestamp;
				U16 mStoredFingers;
				Finger mFingers[eMaxFingers];
			};


		/*	Do the rotation:
			|  0  0 -1 |   | x |
			| -1  0  0 | * | y |
			|  0  1  0 |   | z |

			in this case it's just value flipping.
		*/
		static LLVector3 toAgentCS( float aX, float aY, float aZ  )
		{
			return LLVector3( -aZ, -aX, aY );
		}
		static LLVector3 toAgentCS( LLVector3 const &aIn  )
		{
			return toAgentCS( aIn.mV[VX], aIn.mV[VY], aIn.mV[VZ] );
		}

		static float scaleY( float aIn )
		{
			return aIn - 150.f;
		}

		static LLVector3 scaleToSL( LLVector3 const &aIn )
		{
			return aIn * 1.f/25.f;
		}

		static void copy( Leap::Vector const &aFrom, float *aTo )
		{
			aTo[0] = aFrom[0];
			aTo[1] = aFrom[1];
			aTo[2] = aFrom[2];
		}

		static float length( float const *aVec )
		{
			return sqrt( aVec[0]*aVec[0] + aVec[1]*aVec[1] + aVec[2]*aVec[2] );
		}

		static float normalize(  float *aVec )
		{
			float len = length( aVec );
			if( fabs( len ) > 0.0001f )
			{
				aVec[0] /= len;
				aVec[1] /= len;
				aVec[2] /= len;
			}
			else
			{
				len = aVec[0] = aVec[1] = aVec[2] = 0.f;
			}
			return len;
		}

		static float dot( float const *aVec1, float const *aVec2, bool aUnitVectors = true )
		{
			float alpha = aVec1[0]*aVec2[0] + aVec1[1]*aVec2[1] + aVec1[2]*aVec2[2];

			if( !aUnitVectors )
				alpha /= ( length( aVec1 )*length(aVec2) );
	
			return acos( alpha );
		}

		static void subtract( float const *aLeft, float const *aRight, float *aOut )
		{
			aOut[0] = aLeft[0] - aRight[0];
			aOut[1] = aLeft[1] - aRight[1];
			aOut[2] = aLeft[2] - aRight[2];
		}

        static void move( float const *aStart, float const *aDirecton, float aScale, float *aOut )
		{
            aOut[0] = aStart[0] + aDirecton[0]*aScale;
            aOut[1] = aStart[1] + aDirecton[1]*aScale;
            aOut[2] = aStart[2] + aDirecton[2]*aScale;
            
		}

		ManipTool::ManipTool()
		{
			mLastExaminedFrame = 0;
			mLastStoredFrame = 0;
			mNextRenderedFrame = 0;
			mTotalStoredFrames = 0;
			mFingersPerFrame = new Fingers[ eMaxKeptFrames ];
		}

		ManipTool::~ManipTool()
		{
			delete []mFingersPerFrame;
		}

		std::string ManipTool::getName()
		{
			return "Manipulation tool";
		}

		std::string ManipTool::getDebugString()
		{
			return "";
		}

		S32 ManipTool::getId()
		{
			return 11;
		}

		void ManipTool::clearSelection()
		{
			//for( std::vector<  LLViewerObject*  >::iterator itr = mHighlighted.begin(); itr != mHighlighted.end(); ++itr )
			//{
			//	LLViewerObject *pO = *itr;
			//	LLSelectMgr::getInstance()->unhighlightObjectOnly( pO );
			//}
			//mHighlighted.clear();			

			LLSelectMgr::getInstance()->unhighlightAll();

		}

		void ManipTool::onLeapFrame( Leap::Frame const &aFrame )
		{
			if( (aFrame.timestamp() - mLastExaminedFrame ) < 16*1000 )
				return;

			if( aFrame.hands().count() > 2 )
				return;

			mLastExaminedFrame = aFrame.timestamp();
			U16 curFrame = getNextFrameNo( mLastStoredFrame );
			Fingers &curFingers = mFingersPerFrame[ curFrame ];
			U16 curFinger = 0;
			curFingers.clear();
			curFingers.mTimestamp = mLastExaminedFrame;

			Leap::HandList &hands = aFrame.hands();
			for( int i = 0; i < hands.count(); ++i )
			{
				for( int j = 0; j < hands[i].fingers().count(); ++j )
				{
					Leap::Finger oFinger( hands[i].fingers()[j] );

					Finger &oF = curFingers.mFingers[ curFinger++ ];

					oF.mId = oFinger.id();
					oF.mTimestamp = mLastExaminedFrame;
					copy( oFinger.direction(), oF.mDir );
					copy( oFinger.tipPosition(), oF.mTip );
					oF.mWidth = oFinger.width();
					oF.mLength = oFinger.length();
				}
			}

			curFingers.mStoredFingers = curFinger;

			if( mTotalStoredFrames > 0 )
			{
				Fingers &prevFingers = mFingersPerFrame[ mLastStoredFrame ];
	
				for( U16 i = 0; i < curFingers.mStoredFingers; ++i )
				{
					Finger &curFinger = curFingers.mFingers[i];
					Finger const *prevFinger = prevFingers.getFinger( curFinger.mId );
					if( !prevFinger )
						continue;

					subtract( curFinger.mTip, prevFinger->mTip, curFinger.mFromLast );
					curFinger.mLenFromLast = normalize( curFinger.mFromLast );
				}
			}

			mLastStoredFrame = curFrame;
			++mTotalStoredFrames;
		}

		void ManipTool::onRenderFrame( Leap::Frame const &aFrame )
		{
			clearSelection();
			doSelect(  );
		}

		void ManipTool::findPartner( U16 aIndex )
		{
			Fingers &curFingers = mFingersPerFrame[ mNextRenderedFrame ];
			Finger &curFinger = curFingers.mFingers[ aIndex ];

			if( !curFinger.mSelected )
				return;

			for( U16 i = 0; i < curFingers.mStoredFingers; ++i )
			{
				if( curFingers.mFingers[i].mSelected == curFinger.mSelected )
				{
					if( !curFingers.mFingers[i].mPartner )
					{
						curFingers.mFingers[i].mPartner = aIndex;
						curFinger.mPartner = i;
					}
					else
						curFinger.mSelected = 0;

					break;
				}
			}
		}

		void ManipTool::selectWithFinger( U16 aIndex )
		{
			Finger &oFinger = mFingersPerFrame[ mNextRenderedFrame ].mFingers[ aIndex ];
			GLfloat x( oFinger.mTip[0] );
			GLfloat y( scaleY( oFinger.mTip[1] ) );
			GLfloat z( oFinger.mTip[2] );
			LLVector3 oV1( scaleToSL( toAgentCS( x, y, z) ) );

			x += oFinger.mDir[0] * oFinger.mLength;
			y += oFinger.mDir[1] * oFinger.mLength;
			z += oFinger.mDir[2] * oFinger.mLength;

			LLVector3 oV2( scaleToSL( toAgentCS( x, y, z ) ) );

			oV1 *= gAgentAvatarp->getRotationRegion();
			oV2 *= gAgentAvatarp->getRotationRegion();

			oV1 += gAgentAvatarp->getPositionAgent(  );
			oV2 += gAgentAvatarp->getPositionAgent(  );

			LLVector4a oF1,oF2;
			oF1.load3( oV1.mV );
			oF2.load3( oV2.mV );

			S32 nFace(0);
			LLViewerObject *pHit = gPipeline.lineSegmentIntersectInWorld( oF1,oF2, FALSE, TRUE, &nFace );

			if( pHit )
				oFinger.mSelected = pHit;

			findPartner( aIndex );
		}

		void ManipTool::doSelect()
		{
			Fingers &curFingers = mFingersPerFrame[ mNextRenderedFrame ];
			for( U16 i = 0; i < curFingers.mStoredFingers; ++i )
				selectWithFinger( i );

			for( U16 i = 0; i < curFingers.mStoredFingers; ++i )
			{
				Finger &curFinger = curFingers.mFingers[ i ];
				if( curFinger.mPartner < i )
					continue;

				if( !curFinger.mSelected )
					continue;

				LLColor4 oCol( 0.f, 0.f, 1.f );
				if( curFinger.mPartner )
					oCol.set( 0.f, 1.f, 0.f );

				LLSelectMgr::getInstance()->highlightObjectOnly( curFinger.mSelected, oCol );
			}
		}

		void ManipTool::renderCone( Finger const &aFinger )
		{
			GLfloat x( aFinger.mTip[0] );
			GLfloat y( scaleY( aFinger.mTip[1] ) );
			GLfloat z( aFinger.mTip[2] );

			LLVector3 oTip( scaleToSL( toAgentCS( x, y, z ) ) );

			float dist = aFinger.mWidth*2;
			x -= aFinger.mDir[0] *dist;
			y -= aFinger.mDir[1] *dist;
			z -= aFinger.mDir[2] *dist;

			LLVector3 oM( scaleToSL( toAgentCS( x, y, z) ) );
			float r = 0.25;
			int slices = 16;
			float alpha = (F_PI*2)/slices;

			for( int i = 0; i < slices; ++i )
			{
				GLfloat cx = cos( alpha*i )*r;
				GLfloat cy = sin( alpha*i )*r;

				GLfloat cx2 = cos( alpha*(i+1) )*r;
				GLfloat cy2 = sin( alpha*(i+1) )*r;

				LLVector3 oV1( oM );
				LLVector3 oV2( oM );

				oV1 += LLVector3( 0, cy, -cx );
				oV2 += LLVector3( 0, cy2, -cx2 );

				gGL.vertex3fv( oTip.mV );
				gGL.vertex3fv( oV1.mV );

				gGL.vertex3fv( oV1.mV );
				gGL.vertex3fv( oV2.mV );

				gGL.vertex3fv( oV2.mV );
				gGL.vertex3fv( oTip.mV );
			}
		}

		void ManipTool::renderMovementDirection( Finger const &aFinger )
		{
			float lenDir = aFinger.mLenFromLast;
			U16 prevFrame = getPrevFrameNo( mNextRenderedFrame );
			Finger *prevFinger = mFingersPerFrame[ prevFrame ].getFinger( aFinger.mId );

			while( prevFinger && prevFinger->mTimestamp < aFinger.mTimestamp && (aFinger.mTimestamp - prevFinger->mTimestamp ) < getMaxBacktrackMicroseconds() )
			{	
				float alpha = dot( aFinger.mFromLast, prevFinger->mFromLast );
				if( alpha > F_PI/18 )
					break;

				lenDir += prevFinger->mLenFromLast;

				prevFrame = getPrevFrameNo( prevFrame );
				prevFinger = mFingersPerFrame[ prevFrame ].getFinger( aFinger.mId );
			}
#if 0
			float vec[3];
			bool haveVec( false );
			while(	aFinger.mTimestamp > mFingersPerFrame[ prevFrame ].mTimestamp &&
					(aFinger.mTimestamp > mFingersPerFrame[ prevFrame ].mTimestamp) < getMaxBacktrackMicroseconds() )
			{
				Finger *pFinger = mFingersPerFrame[ prevFrame ].getFinger( aFinger.mId );
				
				if( pFinger )
				{
					if( !haveVec )
					{
						subtract( aFinger.mTip, pFinger->mTip, vec );
						lenDir = normalize( vec );
						haveVec = true;
					}
					else
					{
						float vec2[3];

						subtract( aFinger.mTip, pFinger->mTip, vec2 );
						float lenDir2 = normalize( vec );
		
						float alpha = dot( vec, vec2 );
						if( alpha > F_PI/18 )
							break;
						lenDir += lenDir2;
					}
				}
				prevFrame = getPrevFrameNo( prevFrame );
			}
#endif

			if( lenDir > 0 )
			{
				GLfloat x( aFinger.mTip[0] );
				GLfloat y( scaleY( aFinger.mTip[1] ) );
				GLfloat z( aFinger.mTip[2] );

				LLVector3 oStart( scaleToSL( toAgentCS( x, y, z ) ) );

				x -= aFinger.mFromLast[0] *lenDir;
				y -= aFinger.mFromLast[1] *lenDir;
				z -= aFinger.mFromLast[2] *lenDir;

				LLVector3 oEnd( scaleToSL( toAgentCS( x, y, z) ) );
				gGL.vertex3fv( oStart.mV );
				gGL.vertex3fv( oEnd.mV );
			}
		}

		void ManipTool::renderMovementAngle( Finger const &aFinger, U16 aIndex )
		{
			if( !aFinger.mPartner || aFinger.mPartner < aIndex )
				return;

			U16 prevFrame = getPrevFrameNo( mNextRenderedFrame );
			Finger *prevFinger = mFingersPerFrame[ prevFrame ].getFinger( aFinger.mId );
            U16 startFrame(mNextRenderedFrame);

			while(  prevFinger &&
				    prevFinger->mTimestamp < aFinger.mTimestamp && (aFinger.mTimestamp - prevFinger->mTimestamp ) < getMaxBacktrackMicroseconds() && 
					prevFinger->mPartner )
			{	
                startFrame = prevFrame;
    			prevFrame = getPrevFrameNo( prevFrame );
				prevFinger = mFingersPerFrame[ prevFrame ].getFinger( aFinger.mId );
			}

            if( mNextRenderedFrame == startFrame )
                return;

            Finger const *startFinger( mFingersPerFrame[ startFrame ].getFinger( aFinger.mId ) );
            Finger const *startPartner( mFingersPerFrame[ startFrame ].at( startFinger->mPartner ) );

            Finger const *endFinger( &aFinger );
            Finger const *endPartner( mFingersPerFrame[ startFrame ].at( endFinger->mPartner ) );

            float vecStart[3], vecEnd[3];
            subtract( startFinger->mTip, startPartner->mTip, vecStart );
            subtract( endFinger->mTip, endPartner->mTip, vecEnd );

            float lenStart = normalize( vecStart );
            float lenEnd = normalize( vecEnd );

            float radius = lenStart>lenEnd?lenStart:lenEnd;

            float alpha = dot( vecStart, vecEnd );

            float ptStart[3], ptEnd[3];
            move( startFinger->mTip, vecStart, radius/2, ptStart );

            ptEnd[0] = ptStart[0]*cos(alpha) - ptStart[1]*sin(alpha);
            ptEnd[1] = ptStart[1]*cos(alpha) + ptStart[0]*sin(alpha);
            ptEnd[2] = ptStart[2];

            gGL.vertex3fv( ptStart );
            gGL.vertex3fv( ptEnd );
		}

		void ManipTool::renderFinger( Finger const &aFinger, U16 aIndex )
		{
			if( aFinger.mPartner )
				gGL.diffuseColor4f( 0.f, 1.0f, 0.0f, 1.f);
			else if( aFinger.mSelected )
				gGL.diffuseColor4f( 0.f, 0.0f, 1.0f, 1.f);
			else
				gGL.diffuseColor4f( 1.f, 1.0f, 1.0f, 1.f);

			renderCone( aFinger );
			renderMovementDirection( aFinger );
			renderMovementAngle( aFinger, aIndex );
		}

		void ManipTool::render()
		{
			LLGLEnable blend(GL_BLEND);
			gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);

			gGL.matrixMode(LLRender::MM_MODELVIEW);
			gGL.pushMatrix();
			gGL.begin(LLRender::LINES);

			LLQuaternion rot = gAgentAvatarp->getRotationRegion();
			LLVector3 pos = gAgentAvatarp->getPositionAgent();

			gGL.translatef( pos.mV[0], pos.mV[1], pos.mV[2] );
			gGL.multMatrix( (GLfloat*) gAgentAvatarp->getRotationRegion().getMatrix4().mMatrix );

			gGL.begin(LLRender::LINES);

			Fingers &curFingers = mFingersPerFrame[ mNextRenderedFrame ];
			for( U16 i = 0; i < curFingers.mStoredFingers; ++i )
			{
				Finger &curFinger = curFingers.mFingers[ i ];
				renderFinger( curFinger, i );
			}

			gGL.end();

			gGL.popMatrix();
			mNextRenderedFrame = mLastStoredFrame;
		}
	}
}