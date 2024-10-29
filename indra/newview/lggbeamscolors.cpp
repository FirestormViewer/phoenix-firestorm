/**
 * @file lggbeamscolors.cpp
 * @brief Manager for beams colors
 * @copyright Copyright (c) 2011 LordGregGreg Back
 *
 * This code is free. It comes
 * WITHOUT ANY WARRANTY, without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. You can redistribute it
 * and/or modify it under the terms of the Do What The Fuck You Want
 * To Public License, Version 2, as published by Sam Hocevar. See
 * http://sam.zoy.org/wtfpl/COPYING for more details.
 */


////////////////////////////////////////////////////
//////////////DATA TYPE/////////////////////////////

#include "llviewerprecompiledheaders.h"
#include "lggbeamscolors.h"

lggBeamsColors lggBeamsColors::fromLLSD(const LLSD& inputData)
{
    lggBeamsColors toReturn;

    if (inputData.has("startHue"))
    {
        toReturn.mStartHue = (F32)inputData["startHue"].asReal();
    }

    if (inputData.has("endHue"))
    {
        toReturn.mEndHue = (F32)inputData["endHue"].asReal();
    }

    if (inputData.has("rotateSpeed"))
    {
        toReturn.mRotateSpeed = (F32)inputData["rotateSpeed"].asReal();
    }

    return toReturn;
}

LLSD lggBeamsColors::toLLSD() const
{
    LLSD out;
    out["startHue"] = mStartHue;
    out["endHue"] = mEndHue;
    out["rotateSpeed"] = mRotateSpeed;
    return out;
}

std::string lggBeamsColors::toString() const
{
    return llformat("Start Hue %d\nEnd Hue is %d\nRotate Speed is %d", mStartHue, mEndHue, mRotateSpeed);
}

lggBeamsColors::lggBeamsColors(F32 startHue, F32 endHue, F32 rotateSpeed) :
    mStartHue(startHue),
    mEndHue(endHue),
    mRotateSpeed(rotateSpeed)
{
}
