/** 
 * @file llleapmotioncontroller.h
 * @brief LLLeapMotionController class definition
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
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

#ifndef LL_LLLEAPMOTIONCONTROLLER_H
#define LL_LLLEAPMOTIONCONTROLLER_H

#include "llsingleton.h"

class	LLLMImpl;

class LLLeapMotionController : public LLSingleton<LLLeapMotionController>
{
	friend class LLSingleton<LLLeapMotionController>;
public:
	LLLeapMotionController();
	~LLLeapMotionController();

	void cleanup();

	// Called every viewer frame
	void stepFrame();

	std::string getDebugString();

	void render();

protected:
	LLLMImpl	* mController;

// <FS:Zi> Leap Motion flycam
public:
	// returns true if in leapmotion flycam mode
	bool getOverrideCamera();

	// Called every viewer frame when in flycam mode
	void moveFlycam();
// </FS:Zi>
};

#ifndef USE_LEAPMOTION
inline LLLeapMotionController::LLLeapMotionController()
{
}

inline LLLeapMotionController::~LLLeapMotionController()
{
}

inline void LLLeapMotionController::cleanup()
{
}

inline void LLLeapMotionController::stepFrame()
{
}

inline std::string LLLeapMotionController::getDebugString()
{
	return "";
}

inline void LLLeapMotionController::render()
{
}

#endif

#endif  // LL_LLLEAPMOTIONCONTROLLER_H
