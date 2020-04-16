/** 
 * @file fsregioncross.h
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

#ifndef FS_FSREGIONCROSS_H
#define FS_FSREGIONCROSS_H

//
//  Improved region crossing support.
//

class LLViewerObject;                                               // forward declaration

//
//  LowPassFilter -- a simple Kalman low-pass filter.
//
//  Supports nonuniform time deltas between samples, since object update times are not consistent.
//
class LowPassFilter
{
private:
    LLVector3 mFiltered;                                            // filtered value
    BOOL mInitialized;                                              // true if initialized
public:
    LowPassFilter() :                                               // constructor
    mInitialized(false),
    mFiltered(0.0,0.0,0.0)
    {}
    void update(const LLVector3& val, F32 dt);                      // add new value into filter
    
    const LLVector3& get() const                                    // get filtered output
    {
        return(mFiltered);                                          // already stored
    }
    
    void clear() 
    {
        mInitialized = false;                                       // not initialized yet
    }
};

//
//  RegionCrossExtrapolateImpl -- the extrapolation limit calculator.
//
//  One of these is created when an object is sat upon. If the
//  seat moves, it's effectively a vehicle, so we start calculating
//  region crossing safe extrapolation times.  If the seat never moves,
//  we still allocate one of these, but it doesn't do anything.
//  When the avatar stands, this object is released. 
//  If the LLViewerObject is deleted, so is this object.
//
class RegionCrossExtrapolateImpl                                    // Implementation of region cross extrapolation control 
{
private:
    const LLViewerObject& mOwner;                                   // ref to owning object
    F64 mPreviousUpdateTime;                                        // previous update time
    LowPassFilter mFilteredVel;                                     // filtered velocity
    LowPassFilter mFilteredAngVel;                                  // filtered angular velocity
    BOOL mMoved;                                                    // seen to move at least once

public:
    RegionCrossExtrapolateImpl(const LLViewerObject& vo);           // constructor
    void update();                                                  // update on object update message  
    F32 getextraptimelimit() const;                                 // don't extrapolate more than this
    BOOL hasmoved() const { return(mMoved); }                       // true if has been seen to move with sitter
};

//
//  RegionCrossExtrapolate -- calculate safe limit on how long to extrapolate after a region crossing
//
//  Member object of llViewerObject. For vehicles, a RegionCrossExtrapolateImpl is allocated to do the real work.
//  Call "update" for each new object update.
//  Call "changedlink" for any object update which changes parenting.
//  Get the extrapolation limit time with getextraptimelimit.
//
class LLViewerObject;                                               // forward
class RegionCrossExtrapolate {
private:
    std::unique_ptr<RegionCrossExtrapolateImpl> mImpl;              // pointer to region cross extrapolator, if present
    
protected:
    BOOL ifsaton(const LLViewerObject& vo);                         // true if root object and being sat on
    
public:
    void update(const LLViewerObject& vo)                           // new object update message received
    {   if (mImpl.get()) { mImpl->update(); }                       // update extrapolator if present
    }
    
    void changedlink(const LLViewerObject& vo)                      // parent or child changed, check if extrapolation object needed
    {
        if (ifsaton(vo))                                            // if this object is now the root of a linkset with an avatar
        {   if (!mImpl.get())                                       // if no extrapolation implementor
            {   mImpl.reset(new RegionCrossExtrapolateImpl(vo)); }  // add an extrapolator       
        } else {                                                    // not a vehicle
            if (mImpl.get())
            {   mImpl.reset(); }                                    // no longer needed                           
        }
    }
    
    BOOL ismovingssaton(const LLViewerObject &vo)
    {   if (!mImpl.get()) { return(false); }                        // not sat on
        return(mImpl->hasmoved());                                  // sat on, check for moving
    }
    
    F32 getextraptimelimit() const                                  // get extrapolation time limit
    {   if (mImpl.get()) { return(mImpl->getextraptimelimit()); }   // get extrapolation time limit if vehicle
        return(std::numeric_limits<F32>::infinity());               // no limit if not a vehicle
    }
};

#endif // FS_REGIONCROSS_H
