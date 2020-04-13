/** 
 * @file fsregioncross.cpp
 * @brief Improvements to region crossing display
 * @author nagle@animats.com
 *
 * $LicenseInfo:firstyear=2013&license=viewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (C) 2020 Animats
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * The Phoenix Firestorm Project, Inc., 1831 Oakwood Drive, Fairmont, Minnesota 56031-3225 USA
 * http://www.firestormviewer.org
 */
 
#include "llviewerprecompiledheaders.h"                         // regular Firestorm includes
#include "llviewercontrol.h"
#include "llmath.h"                                             // normal build
#include "llviewerobject.h"
#include "llframetimer.h"                     
#include "fsregioncross.h"
//
//  Improved region crossing time limit prediction.
//
//  Applied when 
//  1) object has an avatar sitting on it
//  2) object has been seen to move since sat on
//  3) object is outside the 0..256m region boundaries.
//
//  In other words, a vehicle with an avatar doing a region crossing.
//
//  This computes a safe time limit for extrapolating vehicle position and 
//  rotation during a region crossing, when the simulator to simulator handoff
//  is in progress and no object updates are being received. Goal is to keep
//  the position and rotation errors within set bounds.
//
//  The smoother the movement of the vehicle approaching the sim crossing,
//  the longer the allowed extrapolation will be.
//
//
//  Constructor -- called when something sits on the object.
//
//  Potential vehicle, but might be just a chair.
//  We don't start the movement averaging until there's movement at least once.
//
RegionCrossExtrapolateImpl::RegionCrossExtrapolateImpl(const LLViewerObject& vo) :          // constructor
    mOwner(vo),                                                     // back ref to owner
    mPreviousUpdateTime(0),                                         // time of last update
    mMoved(false)                                                   // has not moved yet
    {
        LL_INFOS() << "Object " << vo.getID().asString() << " has sitter." << LL_ENDL;    // log sit event
    }

//
//  update -- called for each object update message to "vehicle" objects.
//
//  This is called for any object with an avatar sitting on it.
//  If the object never moves, it's not a vehicle and we can skip this.
//  If it has moved, we treat it as a vehicle.
//
void RegionCrossExtrapolateImpl::update()
{      
    if (!mMoved)                                                // if not seen to move
    {   LLVector3 rawvel = mOwner.getVelocity();                // velocity in world coords
        if (rawvel.mV[VX] != 0.0 || rawvel.mV[VY] != 0.0 || rawvel.mV[VZ] != 0) // check for nonzero velocity
        {   mMoved = true; }                                    // moved, has seated avatar, thus is vehicle
        else
        {   return; }                                           // sitting on stationary object, skip this
    }
    //  Moving seat - do the extrapolation calculations
    F64 dt = 1.0/45.0;                                          // dt used on first value - one physics frame on server  
    F64 now = LLFrameTimer::getElapsedSeconds();                // timestamp
    if (mPreviousUpdateTime != 0.0)
    {   dt = now - mPreviousUpdateTime;                         // change since last update
        //  Could adjust here for ping time and time dilation, but the filter isn't that
        //  sensitive to minor variations dt and it would just complicate things.
    }
    mPreviousUpdateTime = now;
    LLQuaternion rot = mOwner.getRotationRegion();              // transform in global coords
    const LLQuaternion& inverserot = rot.conjugate();           // transform global to local
    LLVector3 vel = mOwner.getVelocity()*inverserot;            // velocity in object coords
    LLVector3 angvel = mOwner.getAngularVelocity()*inverserot;  // angular velocity in object coords
    mFilteredVel.update(vel,dt);                                // accum into filter in object coords
    mFilteredAngVel.update(angvel,dt);                          // accum into filter in object coords
}
//
//  dividesafe -- floating divide with divide by zero check
//
//  Returns infinity for a divide by near zero.
//
static inline F32 dividesafe(F32 num, F32 denom)
{    return((denom > FP_MAG_THRESHOLD                           // avoid divide by zero
         || denom < -FP_MAG_THRESHOLD)        
        ? (num / denom)
        : std::numeric_limits<F32>::infinity());                // return infinity if zero divide
}
//
//  getextraptimelimit -- don't extrapolate further ahead than this during a region crossing.
//
//  Returns seconds of extrapolation that will probably stay within set limits of error.
//
F32 RegionCrossExtrapolateImpl::getextraptimelimit() const
{
    //  Debug tuning parameters. This code will try to limit the maximum position and angle error to the specified limits.
    //  The limits can be adjusted as debug symbols or in settings.xml, but that should not be necessary.
    static LLCachedControl<F32> fsRegionCrossingPositionErrorLimit(gSavedSettings, "FSRegionCrossingPositionErrorLimit");
    static LLCachedControl<F32> fsRegionCrossingAngleErrorLimit(gSavedSettings, "FSRegionCrossingAngleErrorLimit");   
   //  Time limit is max allowed error / error. Returns worst case (smallest) of vel and angular vel limits.
    LLQuaternion rot = mOwner.getRotationRegion();              // transform in global coords
    const LLQuaternion& inverserot = rot.conjugate();           // transform global to local
    //  Calculate safe extrapolation time limit.
    F32 extrapTimeLimit = std::min(
        dividesafe(fsRegionCrossingPositionErrorLimit,
            ((mOwner.getVelocity()*inverserot - mFilteredVel.get()).length())),
        dividesafe(fsRegionCrossingAngleErrorLimit,
            ((mOwner.getAngularVelocity()*inverserot - mFilteredAngVel.get()).length())));
    LL_INFOS() << "Region cross extrapolation safe limit " << extrapTimeLimit << " secs." << LL_ENDL;
    return(extrapTimeLimit);                                                   // do not extrapolate more than this
}  

//
//  ifsaton -- True if object is being sat upon.
//
//  Potential vehicle.
//
BOOL RegionCrossExtrapolate::ifsaton(const LLViewerObject& vo)  // true if root object and being sat on
{   if (!vo.isRoot()) { return(false); }                        // not root, cannot be sat upon
    for (auto iter = vo.getChildren().begin();                  // check for avatar as child of root
	    iter != vo.getChildren().end(); iter++)
	{
		LLViewerObject* child = *iter;                          // get child 
		if (child->isAvatar())                                  // avatar as child        
		{                                                       
		    return(true);                                       // we have a sitter
		}
	}
    return(false);                                              // no avatar children, not sat on
}

//
//  LowPassFilter  -- the low pass filter for smoothing velocities.
//
//  Works on vectors.
//
//  Filter function is scaled by 1- 1/((1+(1/filterval))^secs)
//  This is so samples which represent more time are weighted more.
//
//  The filterval function is a curve which goes through (0,0) and gradually
//  approaches 1 for larger values of secs.  Secs is the time since the last update.
//  We assume that the time covered by an update had reasonably uniform velocity
//  and angular velocity, since if those were changing rapidly, the server would send
//  more object updates.
//
//  Setting filter smoothing time to zero turns off the filter and results in the predictor
//  always returning a very large value and not affecting region crossing times.
//  So that's how to turn this off, if desired.
//
void LowPassFilter::update(const LLVector3& val, F32 secs)      // add new value into filter
{
    static LLCachedControl<F32> fsRegionCrossingSmoothingTime(gSavedSettings, "FSRegionCrossingSmoothingTime");
    if (!mInitialized)                                          // if not initialized yet
    {   mFiltered = val;                                        // just use new value
        mInitialized = true;
        return;
    }
    F32 filtermult = 1.0;                                       // no filtering if zero filter time
    if (fsRegionCrossingSmoothingTime > 0.001)                  // avoid divide by zero
    {   filtermult = 1.0 - 1.0/pow(1.0+1.0/fsRegionCrossingSmoothingTime,secs);  }        // filter scale factor
    mFiltered = val * filtermult + mFiltered*(1.0-filtermult);  // low pass filter
}




