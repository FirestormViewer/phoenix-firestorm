/** 
 * @file lggbeamscolors.h
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

#ifndef LGG_BEAMSCOLORS_H
#define LGG_BEAMSCOLORS_H

#include "llviewerprecompiledheaders.h"

#include "llfile.h"
#include "llsdserialize.h"

class lggBeamsColors
{
public:
	lggBeamsColors(
		F32 istartHue,
		F32 iendHue,
		F32 irotateSpeed);
	lggBeamsColors();

	~lggBeamsColors();

	F32 startHue;
	F32 endHue;
	F32 rotateSpeed;
public:
	LLSD toLLSD();
	static lggBeamsColors fromLLSD(LLSD inputData);

	std::string toString();
	// List sorted by name.
};

#endif // LGG_BEAMSCOLORS_H
