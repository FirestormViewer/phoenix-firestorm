/* Copyright (c) 2009
 *
 * Greg Hendrickson (LordGregGreg Back) All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the following
 * conditions are met:
 *
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *   3. Neither the name Modular Systems nor the names of its contributors
 *      may be used to endorse or promote products derived from this
 *      software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY MODULAR SYSTEMS AND CONTRIBUTORS “AS IS”
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MODULAR SYSTEMS OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */


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
