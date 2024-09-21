/**
* @file fsvirtualtrackpad.h
* @author Angeldark Raymaker; derived from llVirtualTrackball by Andrey Lihatskiy
* @brief Header file for FSVirtualTrackpad
*
* $LicenseInfo:firstyear=2001&license=viewerlgpl$
* Second Life Viewer Source Code
* Copyright (C) 2018, Linden Research, Inc.
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
* Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
* $/LicenseInfo$
*/

// A two dimensional slider control with optional pinch-mode.

#ifndef LL_FSVIRTUALTRACKPAD_H
#define LL_FSVIRTUALTRACKPAD_H

#include "lluictrl.h"
#include "llpanel.h"

class FSVirtualTrackpad
    : public LLUICtrl
{
public:
    struct Params
        : public LLInitParam::Block<Params, LLUICtrl::Params>
    {
        Optional<LLViewBorder::Params> border;
        Optional<LLUIImage *>          image_moon_back, image_moon_front, image_sphere, image_sun_back, image_sun_front;
        Optional<bool> pinch_mode;

        Params();
    };


    virtual ~FSVirtualTrackpad();
    /*virtual*/ bool postBuild();

    virtual bool    handleHover(S32 x, S32 y, MASK mask);
    virtual bool    handleMouseUp(S32 x, S32 y, MASK mask);
    virtual bool    handleMouseDown(S32 x, S32 y, MASK mask);
    virtual bool    handleRightMouseDown(S32 x, S32 y, MASK mask);
    virtual bool    handleScrollWheel(S32 x, S32 y, S32 clicks);

    virtual void    draw();

    virtual void    setValue(const LLSD& value);

    /// <summary>
    /// Sets the position of the cursor.
    /// </summary>
    /// <param name="x">The x-axis (left/right) position to set; expected range -1..1; left= -1</param>
    /// <param name="y">The y-axis (top/bottom) position to set; expected range 1..-1; top = 1</param>
    void            setValue(F32 x, F32 y);

    void            undoLastValue();
    void            undoLastSetPinchValue();

    /// <summary>
    /// Sets the position of the second cursor.
    /// </summary>
    /// <param name="x">The normalized x-axis value (ordinarily screen left-right), expected left-to-right range -1..1.</param>
    /// <param name="y">The normalized y-axis value (ordinarily screen up-down), expected top-to-bottom range 1..-1</param>
    void            setPinchValue(F32 x, F32 y);

    virtual LLSD    getValue();
    virtual LLSD    getPinchValue();

protected:
    friend class LLUICtrlFactory;
    FSVirtualTrackpad(const Params&);

protected:
    LLPanel*            mTouchArea;
    LLViewBorder*       mBorder;

private:
    void setValueAndCommit(const S32 x, const S32 y);
    void setPinchValueAndCommit(const S32 x, const S32 y);
    void drawThumb(bool isPinchThumb);
    bool isPointInTouchArea(S32 x, S32 y) const;

    void determineThumbClickError(S32 x, S32 y);
    void determineThumbClickErrorForPinch(S32 x, S32 y);


    LLVector3 normalizePixelPos(S32 x, S32 y, S32 z) const;
    void      convertNormalizedToPixelPos(F32 x, F32 y, S32* valX, S32* valY);

    LLUIImage*     mImgMoonBack;
    LLUIImage*     mImgMoonFront;
    LLUIImage*     mImgSunBack;
    LLUIImage*     mImgSunFront;
    LLUIImage*     mImgSphere;

    /// <summary>
    /// Whether we allow the second cursor to appear.
    /// </summary>
    bool           mAllowPinchMode = false;

    /// <summary>
    /// Whether we should be moving the pinch cursor now
    /// </summary>
    bool doingPinchMode = false;

    S32       _valueX;
    S32       _valueY;
    S32       _valueWheelClicks;
    S32       _lastValueX;
    S32       _lastValueY;

    S32       _pinchValueX;
    S32       _pinchValueY;
    S32       _pinchValueWheelClicks;
    S32       _lastPinchValueX;
    S32       _lastPinchValueY;

    /// <summary>
    /// Rolling the wheel is pioneering a 'delta' mode: where changes are handled by the control-owner in a relative way.
    /// One could make all the axes behave this way, making the getValue just a delta (and requiring no set); the
    /// cursor would snap-back to centre on mouse-up...
    /// The control would then be used like a real trackball, which only tracks relative movement.
    /// </summary>
    S32 _wheelClicksSinceMouseDown = 0;

    // if one clicks on the thumb, don't move it, track the offset and factor the error out
    S32 _thumbClickOffsetX;
    S32 _thumbClickOffsetY;
    S32 _pinchThumbClickOffsetX;
    S32 _pinchThumbClickOffsetY;
};

#endif

