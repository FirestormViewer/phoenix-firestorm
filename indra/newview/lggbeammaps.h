/** 
 * @file lggbeammaps.h
 * @brief Manager for Beam Shapes
 * @copyright Copyright (c) 2011 LordGregGreg Back
 *
 * This code is free. It comes
 * WITHOUT ANY WARRANTY, without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. You can redistribute it
 * and/or modify it under the terms of the Do What The Fuck You Want
 * To Public License, Version 2, as published by Sam Hocevar. See
 * http://sam.zoy.org/wtfpl/COPYING for more details.
 */

#ifndef LGG_BEAMMAPS_H
#define LGG_BEAMMAPS_H

#include "llhudeffecttrail.h"
#include "llviewerprecompiledheaders.h"
#include "lggbeamscolors.h"
#include "llframetimer.h"
class lggBeamData
{
	public:
		LLVector3d p;
		LLColor4U c;
	
};
class lggBeamMaps
{
	public:
		lggBeamMaps():lastFileName(""),scale(0.0f),duration(0.25f),sPartsNow(FALSE),sBeamLastAt(LLVector3d::zero){}
		~lggBeamMaps() {}
	public:
		F32		setUpAndGetDuration();
		void	fireCurrentBeams(LLPointer<LLHUDEffectSpiral>, LLColor4U rgb);
		void	forceUpdate();
		static LLColor4U beamColorFromData(lggBeamsColors data);
		LLColor4U getCurrentColor(LLColor4U agentColor);
		std::vector<std::string> getFileNames();
		std::vector<std::string> getColorsFileNames();
		void stopBeamChat();
		void updateBeamChat(LLVector3d currentPos);
	private:
		LLSD	getPic(std::string filename); 
		std::string lastFileName;
		std::string lastColorFileName;
		BOOL		sPartsNow;
		LLVector3d sBeamLastAt;
		lggBeamsColors lastColorsData;
		F32 duration;
		F32 scale;
		std::vector<lggBeamData> dots;     
};


extern lggBeamMaps gLggBeamMaps;

#endif // LGG_BEAMMAPS_H