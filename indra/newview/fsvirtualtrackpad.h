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
        Optional<bool>                 pinch_mode;
        Optional<bool>                 infinite_scroll_mode;

        Params();
    };

    virtual ~FSVirtualTrackpad();
    bool         postBuild();
    virtual bool handleHover(S32 x, S32 y, MASK mask);
    virtual bool handleMouseUp(S32 x, S32 y, MASK mask);
    virtual bool handleMouseDown(S32 x, S32 y, MASK mask);
    virtual bool handleRightMouseUp(S32 x, S32 y, MASK mask);
    virtual bool handleRightMouseDown(S32 x, S32 y, MASK mask);
    virtual bool handleScrollWheel(S32 x, S32 y, S32 clicks);
    virtual void draw();
    virtual void setValue(const LLSD& value);

    /// <summary>
    /// Sets the position of the cursor.
    /// </summary>
    /// <param name="x">The x-axis (left/right) position to set; expected range -1..1; left= -1</param>
    /// <param name="y">The y-axis (top/bottom) position to set; expected range 1..-1; top = 1</param>
    /// <param name="y">The z-axis position to set; expected range 1..-1; top = 1</param>
    void setValue(F32 x, F32 y, F32 z);

    /// <summary>
    /// Sets the position of the second cursor.
    /// </summary>
    /// <param name="x">The normalized x-axis value (ordinarily screen left-right), expected left-to-right range -1..1.</param>
    /// <param name="y">The normalized y-axis value (ordinarily screen up-down), expected top-to-bottom range 1..-1</param>
    /// <param name="z">The normalized z-axis value, expected top-to-bottom range 1..-1</param>
    void setPinchValue(F32 x, F32 y, F32 z);

    /// <summary>
    /// Gets the current position of the first 3-axis cursor.
    /// </summary>
    virtual LLSD getValue() const;

    /// <summary>
    /// Gets the most recent delta of position of the first 3-axis cursor.
    /// </summary>
    virtual LLSD getValueDelta();

    /// <summary>
    /// Gets the current position of the second 3-axis cursor.
    /// </summary>
    virtual LLSD getPinchValue();

    /// <summary>
    /// Gets the most recent delta of position of the second 3-axis cursor.
    /// </summary>
    virtual LLSD getPinchValueDelta();

protected:
    friend class LLUICtrlFactory;
    FSVirtualTrackpad(const Params&);

protected:
    LLPanel*      mTouchArea{ nullptr };
    LLViewBorder* mBorder{ nullptr };

private:
    const F32 ThirdAxisQuantization = 0.001f; // To avoid quantizing the third axis as we add integer wheel clicks, use this to preserve some precision as int.
    const S32 WheelClickQuanta = 10; // each click of the wheel moves the third axis by this normalized amount.

    void drawThumb(bool isPinchThumb);
    bool isPointInTouchArea(S32 x, S32 y) const;
    void wrapOrClipCursorPosition(S32* x, S32* y) const;
    void determineThumbClickError(S32 x, S32 y);
    void updateClickErrorIfInfiniteScrolling();
    void determineThumbClickErrorForPinch(S32 x, S32 y);
    void updateClickErrorIfInfiniteScrollingForPinch();

    void      convertNormalizedToPixelPos(F32 x, F32 y, F32 z, S32* valX, S32* valY, S32* valZ);
    LLVector3 normalizePixelPos(S32 x, S32 y, S32 z) const;
    LLVector3 normalizeDelta(S32 x, S32 y, S32 z) const;

    void getHoverMovementDeltas(S32 x, S32 y, MASK mask, S32* deltaX, S32* deltaY, S32* deltaZ);
    void applyHoverMovementDeltas(S32 deltaX, S32 deltaY, MASK mask);
    void applyDeltasToValues(S32 deltaX, S32 deltaY, MASK mask);
    void applyDeltasToDeltaValues(S32 deltaX, S32 deltaY, S32 deltaZ, MASK mask);

    LLUIImage* mImgMoonBack{ nullptr };
    LLUIImage* mImgMoonFront{ nullptr };
    LLUIImage* mImgSunBack{ nullptr };
    LLUIImage* mImgSunFront{ nullptr };
    LLUIImage* mImgSphere{ nullptr };

    /// <summary>
    /// Whether we allow the second cursor to appear.
    /// </summary>
    bool mAllowPinchMode{ false };

    /// <summary>
    /// Whether we should be moving the pinch cursor now
    /// </summary>
    bool mDoingPinchMode{ false };

    /// <summary>
    /// Whether to allow the cursor(s) to 'wrap'.
    /// </summary>
    /// <example>
    /// When false, the cursor is constrained to the control-area.
    /// When true, the cursor 'disappears' out the top, and starts from the bottom,
    /// effectively allowing infinite scrolling.
    /// </example>
    bool mInfiniteScrollMode{ false };

    bool mHeldDownControlBefore{ false };

    /// <summary>
    /// The values the owner will get and set.
    /// </summary>
    S32 mWheelClicksSinceLastDelta{ 0 };

    /// <summary>
    /// The values the owner will get and set.
    /// </summary>
    S32 mValueX{ 0 };
    S32 mValueY{ 0 };
    S32 mValueZ{ 0 };
    S32 mPinchValueX{ 0 };
    S32 mPinchValueY{ 0 };
    S32 mPinchValueZ{ 0 };

    /// <summary>
    /// The delta values the owner will get and set.
    /// </summary>
    S32 mValueDeltaX{ 0 };
    S32 mValueDeltaY{ 0 };
    S32 mValueDeltaZ{ 0 };
    S32 mPinchValueDeltaX{ 0 };
    S32 mPinchValueDeltaY{ 0 };
    S32 mPinchValueDeltaZ{ 0 };

    /// <summary>
    /// The various values placing the cursors and documenting behaviours.
    /// Where relevant, all are scaled in pixels.
    /// </summary>
    S32 mCursorValueX{ 0 };
    S32 mCursorValueY{ 0 };
    S32 mCursorValueZ{ 0 };
    S32 mPinchCursorValueX{ 0 };
    S32 mPinchCursorValueY{ 0 };
    S32 mPinchCursorValueZ{ 0 };

    // if one clicks on or about the thumb, we don't move it, instead we calculate the click-position error and factor it out
    S32 mThumbClickOffsetX{ 0 };
    S32 mThumbClickOffsetY{ 0 };
    S32 mPinchThumbClickOffsetX{ 0 };
    S32 mPinchThumbClickOffsetY{ 0 };
    S32 mPosXwhenCtrlDown{ 0 };
    S32 mPosYwhenCtrlDown{ 0 };
};
#endif

