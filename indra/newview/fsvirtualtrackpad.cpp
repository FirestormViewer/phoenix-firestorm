/**
* @file irtualTrackpad.cpp
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

#include "fsvirtualtrackpad.h"
#include "llrect.h"
#include "lluictrlfactory.h"

// Globals
static LLDefaultChildRegistry::Register<FSVirtualTrackpad> register_virtual_trackball("fs_virtual_trackpad");

FSVirtualTrackpad::Params::Params()
  : border("border"),
    image_moon_back("image_moon_back"),
    image_moon_front("image_moon_front"),
    image_sphere("image_sphere"),
    image_sun_back("image_sun_back"),
    image_sun_front("image_sun_front"),
    pinch_mode("pinch_mode")
{
}

FSVirtualTrackpad::FSVirtualTrackpad(const FSVirtualTrackpad::Params &p)
  : LLUICtrl(p),
    mImgMoonBack(p.image_moon_back),
    mImgMoonFront(p.image_moon_front),
    mImgSunBack(p.image_sun_back),
    mImgSunFront(p.image_sun_front),
    mImgSphere(p.image_sphere),
    mAllowPinchMode(p.pinch_mode)
{
    LLRect border_rect = getLocalRect();
    S32 centerX = border_rect.getCenterX();
    S32 centerY = border_rect.getCenterY();

    mValue.set(centerX, centerY, 0);
    mPinchValue.set(centerX, centerY, 0);
    mLastValue.set(centerX, centerY, 0);
    mLastPinchValue.set(centerX, centerY, 0);

    LLViewBorder::Params border = p.border;
    border.rect(border_rect);
    mBorder = LLUICtrlFactory::create<LLViewBorder>(border);
    addChild(mBorder);

    LLPanel::Params touch_area;
    touch_area.rect = LLRect(border_rect);
    mTouchArea = LLUICtrlFactory::create<LLPanel>(touch_area);
    addChild(mTouchArea);
}

FSVirtualTrackpad::~FSVirtualTrackpad() {}

bool FSVirtualTrackpad::postBuild()
{
    return true;
}

void FSVirtualTrackpad::drawThumb(const LLVector3 vec, bool isPinchThumb)
{
    LLUIImage *thumb = isPinchThumb ? mImgSunFront : mImgMoonFront;

    thumb->draw(LLRect(vec.mV[VX] - thumb->getWidth() / 2,
                       vec.mV[VY] + thumb->getHeight() / 2,
                       vec.mV[VX] + thumb->getWidth() / 2,
                       vec.mV[VY] - thumb->getHeight() / 2));
}

bool FSVirtualTrackpad::isPointInTouchArea(S32 x, S32 y) const
{
    if (!mTouchArea)
        return false;

    return mTouchArea->getRect().localPointInRect(x, y);
}

LLVector3 FSVirtualTrackpad::getThumbClickError(S32 x, S32 y, bool isPinchThumb) const
{
    LLVector3 zeroVector; 
    if (!mTouchArea)
        return zeroVector;

    LLVector3 currentSpot = isPinchThumb ? mPinchValue : mValue;
    LLVector3 errorVec    = LLVector3(currentSpot.mV[VX] - x, currentSpot.mV[VY] - y, 0);

    LLUIImage *thumb = isPinchThumb ? mImgSunFront : mImgMoonFront;

    if (fabs(errorVec.mV[VX]) > thumb->getWidth() / 2.0)
        return zeroVector;
    if (fabs(errorVec.mV[VX]) > thumb->getHeight() / 2.0)
        return zeroVector;

    return errorVec;
}

void FSVirtualTrackpad::draw()
{
    mImgSphere->draw(mTouchArea->getRect(), mTouchArea->isInEnabledChain() ? UI_VERTEX_COLOR : UI_VERTEX_COLOR % 0.5f);

    if (mAllowPinchMode)
        drawThumb(mPinchValue, true);

    drawThumb(mValue, false);

    LLView::draw();
}

void FSVirtualTrackpad::setValue(const LLSD& value)
{
    if (value.isArray() && value.size() == 2)
        mValue.setValue(value);
}

void FSVirtualTrackpad::setValue(F32 x, F32 y) { mValue = convertNormalizedToPixelPos(x, y); }

void FSVirtualTrackpad::setPinchValue(F32 x, F32 y) { mPinchValue = convertNormalizedToPixelPos(x, y); }

void FSVirtualTrackpad::undoLastValue() { setValueAndCommit(mLastValue); }

void FSVirtualTrackpad::undoLastSetPinchValue() { setPinchValueAndCommit(mLastValue); }

void FSVirtualTrackpad::setValueAndCommit(const LLVector3 vec)
{
    mValue.set(vec);
    onCommit();
}

void FSVirtualTrackpad::setPinchValueAndCommit(const LLVector3 vec)
{
    mPinchValue.set(vec);
    onCommit();
}

LLSD FSVirtualTrackpad::getValue()
{
    LLSD result   = normalizePixelPosToCenter(mValue).getValue();
    mValue.mV[VZ] = 0;

    return result;
}

LLSD FSVirtualTrackpad::getPinchValue()
{
    LLSD result = normalizePixelPosToCenter(mPinchValue).getValue();
    mPinchValue.mV[VZ] = 0;

    return result;
}

bool FSVirtualTrackpad::handleHover(S32 x, S32 y, MASK mask)
{
    if (!hasMouseCapture())
        return true;

    LLRect rect = mTouchArea->getRect();
    rect.clampPointToRect(x, y);

    if (doingPinchMode)
    {
        mPinchValue.mV[VX] = x + mPinchThumbClickOffset.mV[VX];
        mPinchValue.mV[VY] = y + mPinchThumbClickOffset.mV[VY];
    }
    else
    {
        mValue.mV[VX] = x + mThumbClickOffset.mV[VX];
        mValue.mV[VY] = y + mThumbClickOffset.mV[VY];
    }

    onCommit();

    return true;
}

LLVector3 FSVirtualTrackpad::normalizePixelPosToCenter(LLVector3 pixelPos) const
{
    LLVector3 result;
    if (!mTouchArea)
        return result;

    LLRect rect = mTouchArea->getRect();
    S32    centerX = rect.getCenterX();
    S32    centerY = rect.getCenterY();
    S32    width = rect.getWidth();
    S32    height = rect.getHeight();

    result.mV[VX] = (pixelPos.mV[VX] - centerX) / width * 2;
    result.mV[VY] = (pixelPos.mV[VY] - centerY) / height * 2;
    result.mV[VZ] = pixelPos.mV[VZ];

    return result;
}

LLVector3 FSVirtualTrackpad::convertNormalizedToPixelPos(F32 x, F32 y)
{
    LLVector3 result;
    if (!mTouchArea)
        return result;

    LLRect rect    = mTouchArea->getRect();
    S32    centerX = rect.getCenterX();
    S32    centerY = rect.getCenterY();
    S32    width   = rect.getWidth();
    S32    height  = rect.getHeight();

    result.mV[VX] = (centerX + llclamp(x, -1, 1) * width / 2);
    result.mV[VY] = (centerY + llclamp(y, -1, 1) * height / 2);

    return result;
}

bool FSVirtualTrackpad::handleMouseUp(S32 x, S32 y, MASK mask)
{
    if (hasMouseCapture())
    {
        doingPinchMode = false; 
        gFocusMgr.setMouseCapture(NULL);

        make_ui_sound("UISndClickRelease");
    }

    return LLView::handleMouseUp(x, y, mask);
}

bool FSVirtualTrackpad::handleMouseDown(S32 x, S32 y, MASK mask)
{
    if (isPointInTouchArea(x, y))
    {
        mThumbClickOffset = getThumbClickError(x, y, false);
        mValue.mV[VZ] = 0;
        mLastValue.set(mValue);
        gFocusMgr.setMouseCapture(this);

        make_ui_sound("UISndClick");
    }

    return LLView::handleMouseDown(x, y, mask);
}

// move pinch cursor
bool FSVirtualTrackpad::handleRightMouseDown(S32 x, S32 y, MASK mask)
{
    if (!mAllowPinchMode)
        return LLView::handleRightMouseDown(x, y, mask);

    if (isPointInTouchArea(x, y))
    {
        mPinchThumbClickOffset = getThumbClickError(x, y, true);
        mPinchValue.mV[VZ] = 0;
        mLastPinchValue.set(mPinchValue);
        doingPinchMode = true;
        gFocusMgr.setMouseCapture(this);

        make_ui_sound("UISndClick");
    }

    return LLView::handleRightMouseDown(x, y, mask);
}

// pass wheel-clicks to third axis
bool FSVirtualTrackpad::handleScrollWheel(S32 x, S32 y, S32 clicks)
{
    if (hasMouseCapture())
    {
        if (doingPinchMode)
            mPinchValue.mV[VZ] = clicks;
        else
            mValue.mV[VZ] = clicks;

        return true;
    }
    else
        return LLUICtrl::handleScrollWheel(x, y, clicks);
}
