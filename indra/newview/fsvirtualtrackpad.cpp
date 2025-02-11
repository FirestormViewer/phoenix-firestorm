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
#include "llkeyboard.h"

// Globals
static LLDefaultChildRegistry::Register<FSVirtualTrackpad> register_virtual_trackball("fs_virtual_trackpad");

FSVirtualTrackpad::Params::Params()
  : border("border"),
    image_moon_back("image_moon_back"),
    image_moon_front("image_moon_front"),
    image_sphere("image_sphere"),
    image_sun_back("image_sun_back"),
    image_sun_front("image_sun_front"),
    pinch_mode("pinch_mode"),
    infinite_scroll_mode("infinite_scroll_mode")
{
}

FSVirtualTrackpad::FSVirtualTrackpad(const FSVirtualTrackpad::Params &p)
  : LLUICtrl(p),
    mImgMoonBack(p.image_moon_back),
    mImgMoonFront(p.image_moon_front),
    mImgSunBack(p.image_sun_back),
    mImgSunFront(p.image_sun_front),
    mImgSphere(p.image_sphere),
    mAllowPinchMode(p.pinch_mode),
    mInfiniteScrollMode(p.infinite_scroll_mode)
{
    LLRect border_rect = getLocalRect();
    mCursorValueX = mPinchCursorValueX = border_rect.getCenterX();
    mCursorValueY = mPinchCursorValueY = border_rect.getCenterY();
    mCursorValueZ = mPinchCursorValueZ = 0;

    mThumbClickOffsetX = mThumbClickOffsetY = mPinchThumbClickOffsetX = mPinchThumbClickOffsetY = 0;
    mPosXwhenCtrlDown = mPosYwhenCtrlDown = 0;
    mPinchValueDeltaX = mPinchValueDeltaY = mPinchValueDeltaZ = 0;
    mValueDeltaX = mValueDeltaY = mValueDeltaZ = 0;

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

void FSVirtualTrackpad::drawThumb(bool isPinchThumb)
{
    LLUIImage *thumb;
    if (mTouchArea->isInEnabledChain())
        thumb = isPinchThumb ? mImgSunFront : mImgMoonFront;
    else
        thumb = isPinchThumb ? mImgSunBack : mImgMoonBack;

    S32 x = isPinchThumb ? mPinchCursorValueX : mCursorValueX;
    S32 y = isPinchThumb ? mPinchCursorValueY : mCursorValueY;

    wrapOrClipCursorPosition(&x, &y);

    S32 halfThumbWidth  = thumb->getWidth() / 2;
    S32 halfThumbHeight = thumb->getHeight() / 2;

    thumb->draw(LLRect(x - halfThumbWidth,
                       y + halfThumbHeight,
                       x + halfThumbWidth,
                       y - halfThumbHeight));
}

bool FSVirtualTrackpad::isPointInTouchArea(S32 x, S32 y) const
{
    if (!mTouchArea)
        return false;

    return mTouchArea->getRect().localPointInRect(x, y);
}

void FSVirtualTrackpad::determineThumbClickError(S32 x, S32 y)
{
    if (!mTouchArea)
        return;
    if (!mImgSunFront)
        return;

    mThumbClickOffsetX = 0;
    mThumbClickOffsetY = 0;

    S32 errorX = mCursorValueX;
    S32 errorY = mCursorValueY;
    wrapOrClipCursorPosition(&errorX, &errorY);
    errorX -= x;
    errorY -= y;

    if (fabs(errorX) > mImgSunFront->getWidth() / 2.0)
        return;
    if (fabs(errorY) > mImgSunFront->getHeight() / 2.0)
        return;

    mThumbClickOffsetX = errorX;
    mThumbClickOffsetY = errorY;
}

void FSVirtualTrackpad::updateClickErrorIfInfiniteScrolling()
{
    if (!mInfiniteScrollMode)
        return;
    S32 errorX = mCursorValueX;
    S32 errorY = mCursorValueY;

    LLRect rect = mTouchArea->getRect();
    while (errorX > rect.mRight)
    {
        errorX -= rect.getWidth();
        mThumbClickOffsetX += rect.getWidth();
    }
    while (errorX < rect.mLeft)
    {
        errorX += rect.getWidth();
        mThumbClickOffsetX -= rect.getWidth();
    }
    while (errorY > rect.mTop)
    {
        errorY -= rect.getHeight();
        mThumbClickOffsetY += rect.getHeight();
    }
    while (errorY < rect.mBottom)
    {
        errorY += rect.getHeight();
        mThumbClickOffsetY -= rect.getHeight();
    }
}

void FSVirtualTrackpad::determineThumbClickErrorForPinch(S32 x, S32 y)
{
    if (!mTouchArea)
        return;
    if (!mImgMoonFront)
        return;

    mPinchThumbClickOffsetX = 0;
    mPinchThumbClickOffsetY = 0;

    S32 errorX = mPinchCursorValueX;
    S32 errorY = mPinchCursorValueY;
    wrapOrClipCursorPosition(&errorX, &errorY);
    errorX -= x;
    errorY -= y;

    if (fabs(errorX) > mImgMoonFront->getWidth() / 2.0)
        return;
    if (fabs(errorY) > mImgMoonFront->getHeight() / 2.0)
        return;

    mPinchThumbClickOffsetX = errorX;
    mPinchThumbClickOffsetY = errorY;
}

void FSVirtualTrackpad::updateClickErrorIfInfiniteScrollingForPinch()
{
    if (!mInfiniteScrollMode)
        return;
    S32 errorX = mCursorValueX;
    S32 errorY = mCursorValueY;

    LLRect rect = mTouchArea->getRect();
    while (errorX > rect.mRight)
    {
        errorX -= rect.getWidth();
        mPinchThumbClickOffsetX += rect.getWidth();
    }
    while (errorX < rect.mLeft)
    {
        errorX += rect.getWidth();
        mPinchThumbClickOffsetX -= rect.getWidth();
    }
    while (errorY > rect.mTop)
    {
        errorY -= rect.getHeight();
        mPinchThumbClickOffsetY += rect.getHeight();
    }
    while (errorY < rect.mBottom)
    {
        errorY += rect.getHeight();
        mPinchThumbClickOffsetY -= rect.getHeight();
    }
}

void FSVirtualTrackpad::draw()
{
    mImgSphere->draw(mTouchArea->getRect(), mTouchArea->isInEnabledChain() ? UI_VERTEX_COLOR : UI_VERTEX_COLOR % 0.5f);

    if (mAllowPinchMode)
        drawThumb(true);

    drawThumb(false);

    LLView::draw();
}

void FSVirtualTrackpad::setValue(const LLSD& value)
{
    if (!value.isArray())
        return;

    if (value.size() == 2)
    {
        LLVector2 vec2;
        vec2.setValue(value);
        setValue(vec2.mV[VX], vec2.mV[VY], 0);
    }
    else if (value.size() == 3)
    {
        LLVector3 vec3;
        vec3.setValue(value);
        setValue(vec3.mV[VX], vec3.mV[VY], vec3.mV[VZ]);
    }
}

void FSVirtualTrackpad::setValue(F32 x, F32 y, F32 z)
{
    convertNormalizedToPixelPos(x, y, z, &mCursorValueX, &mCursorValueY, &mCursorValueZ);
    mValueX = mCursorValueX;
    mValueY = mCursorValueY;
    mValueZ = mCursorValueZ;
}

void FSVirtualTrackpad::setPinchValue(F32 x, F32 y, F32 z)
{
    convertNormalizedToPixelPos(x, y, z, &mPinchCursorValueX, &mPinchCursorValueY, &mPinchCursorValueZ);
    mPinchValueX = mPinchCursorValueX;
    mPinchValueY = mPinchCursorValueY;
    mPinchValueZ = mPinchCursorValueZ;
}

LLSD FSVirtualTrackpad::getValue() const { return normalizePixelPos(mValueX, mValueY, mValueZ).getValue(); }
LLSD FSVirtualTrackpad::getValueDelta() { return normalizeDelta(mValueDeltaX, mValueDeltaY, mValueDeltaZ).getValue(); }

LLSD FSVirtualTrackpad::getPinchValue() { return normalizePixelPos(mPinchValueX, mPinchValueY, mPinchValueZ).getValue(); }
LLSD FSVirtualTrackpad::getPinchValueDelta() { return normalizeDelta(mPinchValueDeltaX, mPinchValueDeltaY, mPinchValueDeltaZ).getValue(); }

void FSVirtualTrackpad::wrapOrClipCursorPosition(S32* x, S32* y) const
{
    if (!x || !y)
        return;

    LLRect rect = mTouchArea->getRect();
    if (mInfiniteScrollMode)
    {
        while (*x > rect.mRight)
            *x -= rect.getWidth();
        while (*x < rect.mLeft)
            *x += rect.getWidth();
        while (*y > rect.mTop)
            *y -= rect.getHeight();
        while (*y < rect.mBottom)
            *y += rect.getHeight();
    }
    else
    {
        rect.clampPointToRect(*x, *y);
    }
}

bool FSVirtualTrackpad::handleHover(S32 x, S32 y, MASK mask)
{
    if (!hasMouseCapture())
        return true;

    S32 deltaX, deltaY, deltaZ;
    getHoverMovementDeltas(x, y, mask, &deltaX, &deltaY, &deltaZ);
    applyHoverMovementDeltas(deltaX, deltaY, mask);
    applyDeltasToValues(deltaX, deltaY, mask);
    applyDeltasToDeltaValues(deltaX, deltaY, deltaZ, mask);

    onCommit();

    return true;
}

void FSVirtualTrackpad::getHoverMovementDeltas(S32 x, S32 y, MASK mask, S32* deltaX, S32* deltaY, S32* deltaZ)
{
    if (!deltaX || !deltaY || !deltaZ)
        return;

    S32 fromX, fromY;
    fromX = mDoingPinchMode ? mPinchCursorValueX : mCursorValueX;
    fromY = mDoingPinchMode ? mPinchCursorValueY : mCursorValueY;

    *deltaZ = mWheelClicksSinceLastDelta;
    mWheelClicksSinceLastDelta = 0;

    if (mask & MASK_CONTROL)
    {
        if (!mHeldDownControlBefore)
        {
            mPosXwhenCtrlDown = x;
            mPosYwhenCtrlDown = y;
            mHeldDownControlBefore = true;
        }

        if (mDoingPinchMode)
        {
            *deltaX = mPosXwhenCtrlDown - (mPosXwhenCtrlDown - x) / 8 + mPinchThumbClickOffsetX - fromX;
            *deltaY = mPosYwhenCtrlDown - (mPosYwhenCtrlDown - y) / 8 + mPinchThumbClickOffsetY - fromY;
        }
        else
        {
            *deltaX = mPosXwhenCtrlDown - (mPosXwhenCtrlDown - x) / 8 + mThumbClickOffsetX - fromX;
            *deltaY = mPosYwhenCtrlDown - (mPosYwhenCtrlDown - y) / 8 + mThumbClickOffsetY - fromY;
        }
    }
    else
    {
        if (mHeldDownControlBefore)
        {
            mThumbClickOffsetX = fromX - x;
            mThumbClickOffsetY = fromY - y;
            mHeldDownControlBefore = false;
        }

        if (mDoingPinchMode)
        {
            *deltaX = x + mPinchThumbClickOffsetX - fromX;
            *deltaY = y + mPinchThumbClickOffsetY - fromY;
        }
        else
        {
            *deltaX = x + mThumbClickOffsetX - fromX;
            *deltaY = y + mThumbClickOffsetY - fromY;
        }
    }
}

void FSVirtualTrackpad::applyHoverMovementDeltas(S32 deltaX, S32 deltaY, MASK mask)
{
    if (mDoingPinchMode)
    {
        mPinchCursorValueX += deltaX;
        mPinchCursorValueY += deltaY;
        if (!mInfiniteScrollMode)  // then constrain the cursor within control area
            wrapOrClipCursorPosition(&mPinchCursorValueX, &mPinchCursorValueY);
    }
    else
    {
        mCursorValueX += deltaX;
        mCursorValueY += deltaY;

        if (!mInfiniteScrollMode)  // then constrain the cursor within control area
            wrapOrClipCursorPosition(&mCursorValueX, &mCursorValueY);
    }
}

void FSVirtualTrackpad::applyDeltasToValues(S32 deltaX, S32 deltaY, MASK mask)
{
    if (mDoingPinchMode)
    {
        if ((mask & (MASK_SHIFT | MASK_ALT)) == MASK_ALT)
        {
            mPinchValueY += deltaY;
            mPinchValueZ += deltaX;
        }
        else if ((mask & (MASK_SHIFT | MASK_ALT)) == MASK_SHIFT)
        {
            mPinchValueX += deltaX;
            mPinchValueZ += deltaY;
        }
        else
        {
            mPinchValueX += deltaX;
            mPinchValueY += deltaY;
        }
    }
    else
    {
        if ((mask & (MASK_SHIFT | MASK_ALT)) == MASK_ALT)
        {
            mValueY += deltaY;
            mValueZ += deltaX;
        }
        else if ((mask & (MASK_SHIFT | MASK_ALT)) == MASK_SHIFT)
        {
            mValueX += deltaX;
            mValueZ += deltaY;
        }
        else
        {
            mValueX += deltaX;
            mValueY += deltaY;
        }
    }
}

void FSVirtualTrackpad::applyDeltasToDeltaValues(S32 deltaX, S32 deltaY, S32 deltaZ, MASK mask)
{
    if (mDoingPinchMode)
    {
        if ((mask & (MASK_SHIFT | MASK_ALT)) == MASK_ALT)
        {
            mPinchValueDeltaX = deltaZ;
            mPinchValueDeltaY = deltaY;
            mPinchValueDeltaZ = deltaX;
        }
        else if ((mask & (MASK_SHIFT | MASK_ALT)) == MASK_SHIFT)
        {
            mPinchValueDeltaX = deltaX;
            mPinchValueDeltaY = deltaZ;
            mPinchValueDeltaZ = deltaY;
        }
        else
        {
            mPinchValueDeltaX = deltaX;
            mPinchValueDeltaY = deltaY;
            mPinchValueDeltaZ = deltaZ;
        }
    }
    else
    {
        if ((mask & (MASK_SHIFT | MASK_ALT)) == MASK_ALT)
        {
            mValueDeltaX = deltaZ;
            mValueDeltaY = deltaY;
            mValueDeltaZ = deltaX;
        }
        else if ((mask & (MASK_SHIFT | MASK_ALT)) == MASK_SHIFT)
        {
            mValueDeltaX = deltaX;
            mValueDeltaY = deltaZ;
            mValueDeltaZ = deltaY;
        }
        else
        {
            mValueDeltaX = deltaX;
            mValueDeltaY = deltaY;
            mValueDeltaZ = deltaZ;
        }
    }
}

LLVector3 FSVirtualTrackpad::normalizePixelPos(S32 x, S32 y, S32 z) const
{
    LLVector3 result;
    if (!mTouchArea)
        return result;

    LLRect rect = mTouchArea->getRect();
    S32    centerX = rect.getCenterX();
    S32    centerY = rect.getCenterY();
    S32    width = rect.getWidth();
    S32    height = rect.getHeight();

    result.mV[VX] = (F32) (x - centerX) / width * 2;
    result.mV[VY] = (F32) (y - centerY) / height * 2;
    result.mV[VZ] = (F32) z * ThirdAxisQuantization;

    return result;
}

LLVector3 FSVirtualTrackpad::normalizeDelta(S32 x, S32 y, S32 z) const
{
    LLVector3 result;
    if (!mTouchArea)
        return result;

    LLRect rect = mTouchArea->getRect();
    S32    width = rect.getWidth();
    S32    height = rect.getHeight();

    result.mV[VX] = (F32) x / width * 2;
    result.mV[VY] = (F32) y / height * 2;
    result.mV[VZ] = (F32) z * ThirdAxisQuantization;

    return result;
}

void FSVirtualTrackpad::convertNormalizedToPixelPos(F32 x, F32 y, F32 z, S32 *valX, S32 *valY, S32 *valZ)
{
    if (!mTouchArea)
        return;

    LLRect rect    = mTouchArea->getRect();
    S32    centerX = rect.getCenterX();
    S32    centerY = rect.getCenterY();
    S32    width   = rect.getWidth();
    S32    height  = rect.getHeight();

    if (mInfiniteScrollMode)
    {
        *valX = centerX + ll_round(x * width / 2);
        *valY = centerY + ll_round(y * height / 2);
    }
    else
    {
        *valX = centerX + ll_round(llclamp(x, -1, 1) * width / 2);
        *valY = centerY + ll_round(llclamp(y, -1, 1) * height / 2);
    }

    *valZ = ll_round(z / ThirdAxisQuantization);
}

bool FSVirtualTrackpad::handleMouseUp(S32 x, S32 y, MASK mask)
{
    if (hasMouseCapture())
    {
        gFocusMgr.setMouseCapture(NULL);
        mHeldDownControlBefore = false;
        make_ui_sound("UISndClickRelease");
    }

    return LLView::handleMouseUp(x, y, mask);
}

bool FSVirtualTrackpad::handleMouseDown(S32 x, S32 y, MASK mask)
{
    if (isPointInTouchArea(x, y))
    {
        determineThumbClickError(x, y);
        updateClickErrorIfInfiniteScrolling();
        gFocusMgr.setMouseCapture(this);
        make_ui_sound("UISndClick");
    }

    return LLView::handleMouseDown(x, y, mask);
}

bool FSVirtualTrackpad::handleRightMouseUp(S32 x, S32 y, MASK mask)
{
    if (hasMouseCapture())
    {
        mDoingPinchMode = false;
        gFocusMgr.setMouseCapture(NULL);

        make_ui_sound("UISndClickRelease");
    }

    return LLView::handleRightMouseUp(x, y, mask);
}

// move pinch cursor
bool FSVirtualTrackpad::handleRightMouseDown(S32 x, S32 y, MASK mask)
{
    if (!mAllowPinchMode)
        return LLView::handleRightMouseDown(x, y, mask);

    if (isPointInTouchArea(x, y))
    {
        determineThumbClickErrorForPinch(x, y);
        updateClickErrorIfInfiniteScrollingForPinch();
        mDoingPinchMode = true;
        gFocusMgr.setMouseCapture(this);

        make_ui_sound("UISndClick");
    }

    return LLView::handleRightMouseDown(x, y, mask);
}

// pass wheel-clicks to third axis
bool FSVirtualTrackpad::handleScrollWheel(S32 x, S32 y, S32 clicks)
{
    if (hasMouseCapture() || isPointInTouchArea(x, y))
    {
        MASK mask = gKeyboard->currentMask(true);

        S32 changeAmount = WheelClickQuanta;
        if (mask & MASK_CONTROL)
            changeAmount /= 5;

        mWheelClicksSinceLastDelta = -1 * clicks * changeAmount;

        if (mDoingPinchMode)
        {
            if ((mask & (MASK_SHIFT | MASK_ALT)) == MASK_ALT)
            {
                mPinchValueX += mWheelClicksSinceLastDelta;
                mPinchValueDeltaX = mWheelClicksSinceLastDelta;
            }
            else if ((mask & (MASK_SHIFT | MASK_ALT)) == MASK_SHIFT)
            {
                mPinchValueY += mWheelClicksSinceLastDelta;
                mPinchValueDeltaY = mWheelClicksSinceLastDelta;
            }
            else
            {
                mPinchValueZ += mWheelClicksSinceLastDelta;
                mPinchValueDeltaZ = mWheelClicksSinceLastDelta;
            }
        }
        else
        {
            if ((mask & (MASK_SHIFT | MASK_ALT)) == MASK_ALT)
            {
                mValueX += mWheelClicksSinceLastDelta;
                mValueDeltaX = mWheelClicksSinceLastDelta;
            }
            else if ((mask & (MASK_SHIFT | MASK_ALT)) == MASK_SHIFT)
            {
                mValueY += mWheelClicksSinceLastDelta;
                mValueDeltaY = mWheelClicksSinceLastDelta;
            }
            else
            {
                mValueZ += mWheelClicksSinceLastDelta;
                mValueDeltaZ = mWheelClicksSinceLastDelta;
            }
        }

        if (!hasMouseCapture())
            onCommit();

        return true;
    }
    else
        return LLUICtrl::handleScrollWheel(x, y, clicks);
}
