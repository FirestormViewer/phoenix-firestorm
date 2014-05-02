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

lggBeamsColors lggBeamsColors::fromLLSD(LLSD inputData)
{

	lggBeamsColors toReturn;
	
	if(inputData.has("startHue")) toReturn.startHue = (F32)inputData["startHue"].asReal();
	if(inputData.has("endHue")) toReturn.endHue = (F32)inputData["endHue"].asReal();
	if(inputData.has("rotateSpeed")) toReturn.rotateSpeed = (F32)inputData["rotateSpeed"].asReal();

	return toReturn;
}
LLSD lggBeamsColors::toLLSD()
{

	LLSD out;
	out["startHue"]=startHue;
	out["endHue"]=endHue;
	out["rotateSpeed"]=rotateSpeed;
	return out;
}

std::string lggBeamsColors::toString()
{

	return llformat("Start Hue %d\nEnd Hue is %d\nRotate Speed is %d",
		startHue,endHue,rotateSpeed
		
		);
}
lggBeamsColors::lggBeamsColors(F32 istartHue, F32 iendHue, F32 irotateSpeed):
startHue(istartHue),endHue(iendHue),rotateSpeed(irotateSpeed)
{
}
lggBeamsColors::lggBeamsColors():
startHue(0.0f),endHue(360.0f),rotateSpeed(1.0f)
{
}
lggBeamsColors::~lggBeamsColors()
{

}
