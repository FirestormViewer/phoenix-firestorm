/** 
 * @file lggbeamscolors.h
 * @brief Manager for beams colors
 * @copyright Copyright (c) 2011 LordGregGreg Back
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */
#ifndef  LGGBEAMCOLORDATA
#define LGGBEAMCOLORDATA

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

#endif
