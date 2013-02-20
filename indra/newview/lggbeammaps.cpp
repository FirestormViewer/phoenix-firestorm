/** 
 * @file lggbeammapps.cpp
 * @brief Manager for beam shapes
 * @copyright Copyright (c) 2011 LordGregGreg Back
 *
 * This code is free. It comes
 * WITHOUT ANY WARRANTY, without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. You can redistribute it
 * and/or modify it under the terms of the Do What The Fuck You Want
 * To Public License, Version 2, as published by Sam Hocevar. See
 * http://sam.zoy.org/wtfpl/COPYING for more details.
 */


#include "llviewerprecompiledheaders.h"
#include "lggbeammaps.h"
#include "lggbeamscolors.h"
#include "llappviewer.h"
#include "llfile.h"
#include "llagent.h"
#include "llsdserialize.h"
#include "llviewercontrol.h"
#include "llhudeffecttrail.h"
#include "llhudmanager.h"
//using namespace std;

#include "message.h"
#include <curl/curl.h>

lggBeamMaps gLggBeamMaps;
F32 hueToRgb ( F32 val1In, F32 val2In, F32 valHUeIn )
{
	while ( valHUeIn < 0.0f ) valHUeIn += 1.0f;
	while ( valHUeIn > 1.0f ) valHUeIn -= 1.0f;
	if ( ( 6.0f * valHUeIn ) < 1.0f ) return ( val1In + ( val2In - val1In ) * 6.0f * valHUeIn );
	if ( ( 2.0f * valHUeIn ) < 1.0f ) return ( val2In );
	if ( ( 3.0f * valHUeIn ) < 2.0f ) return ( val1In + ( val2In - val1In ) * ( ( 2.0f / 3.0f ) - valHUeIn ) * 6.0f );
	return ( val1In );
}

void hslToRgb ( F32 hValIn, F32 sValIn, F32 lValIn, F32& rValOut, F32& gValOut, F32& bValOut )
{
	if ( sValIn < 0.00001f )
	{
		rValOut = lValIn;
		gValOut = lValIn;
		bValOut = lValIn;
	}
	else
	{
		F32 interVal1;
		F32 interVal2;

		if ( lValIn < 0.5f )
			interVal2 = lValIn * ( 1.0f + sValIn );
		else
			interVal2 = ( lValIn + sValIn ) - ( sValIn * lValIn );

		interVal1 = 2.0f * lValIn - interVal2;

		rValOut = hueToRgb ( interVal1, interVal2, hValIn + ( 1.f / 3.f ) );
		gValOut = hueToRgb ( interVal1, interVal2, hValIn );
		bValOut = hueToRgb ( interVal1, interVal2, hValIn - ( 1.f / 3.f ) );
	}
}



LLSD lggBeamMaps::getPic(std::string filename)
{
	LLSD data;
	llifstream importer(filename);
	LLSDSerialize::fromXMLDocument(data, importer);

	return data;
	
}
LLColor4U lggBeamMaps::getCurrentColor(LLColor4U agentColor)
{
	static LLCachedControl<std::string> settingName(gSavedSettings, "FSBeamColorFile");
	std::string setName(settingName);

	if (setName == "===OFF===")
	{
		return agentColor;
	}

	if (setName != lastColorFileName)
	{
		lastColorFileName = settingName;
	
		std::string path_name(gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, "beamsColors", ""));
		std::string path_name2(gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS , "beamsColors", ""));
		std::string filename = path_name + setName + ".xml";
		if (!gDirUtilp->fileExists(filename))
		{
			filename = path_name2 + setName + ".xml";
			if (!gDirUtilp->fileExists(filename))
			{
				return agentColor;
			}
		}

		lastColorsData = lggBeamsColors::fromLLSD(getPic(filename));
	}
	agentColor = beamColorFromData(lastColorsData);
	
	return agentColor;
}

static LLFrameTimer timer;
LLColor4U lggBeamMaps::beamColorFromData(lggBeamsColors data)
{

	F32 r, g, b;
	LLColor4 output;
	LLColor4U toReturn;
	F32 timeinc =  timer.getElapsedTimeF32()*0.3f*((data.rotateSpeed+.01f)) * (360/(data.endHue-data.startHue));

	S32 diference = llround(data.endHue  - data.startHue);
	if(diference == 360 || diference == 720)
	{
		//full rainbow
		//liner one
		hslToRgb(fmod(timeinc,1.0f), 1.0f, 0.5f, r, g, b);
	}
	else
	{
		F32 variance = ((data.endHue/360.0f)-(data.startHue/360.0f))/2.0f;
		hslToRgb((data.startHue/360.0f) + variance + (sinf(timeinc)*(variance)), 1.0f, 0.5f, r, g, b);
	}
	output.set(r, g, b);

	toReturn.setVecScaleClamp(output);
	return toReturn;

}
void lggBeamMaps::fireCurrentBeams(LLPointer<LLHUDEffectSpiral> mBeam, LLColor4U rgb)
{
	if (scale == 0.0f)
	{
		return;
	}

	static LLCachedControl<std::string> colorf(gSavedSettings, "FSBeamColorFile");
	bool colorsDisabled = std::string(colorf) == "===OFF===";
	
	for(int i = 0; i < (int)dots.size(); i++)
	{
		LLColor4U myColor = rgb;
		if (colorsDisabled) myColor = dots[i].c;

		F32 distanceAdjust = dist_vec(mBeam->getPositionGlobal(),gAgent.getPositionGlobal()) ;
		F32 pulse = (F32)(.75f+sinf(gFrameTimeSeconds*1.0f)*0.25f);
		LLVector3d offset = dots[i].p;
		offset.mdV[VY] *= -1;
		offset *= pulse * scale * distanceAdjust * 0.1;
		
		//llinfos << "dist is " << distanceAdjust << "scale is " << scale << llendl;
		LLVector3 beamLine = LLVector3( mBeam->getPositionGlobal() - gAgent.getPositionGlobal());
		LLVector3 beamLineFlat = beamLine;
		beamLineFlat.mV[VZ]= 0.0f;

		LLVector3 newDirFlat = LLVector3::x_axis;
		beamLine.normalize();
		beamLineFlat.normalize();
		LLQuaternion change;
		change.shortestArc(newDirFlat,beamLineFlat);
		offset.rotVec(change);
		newDirFlat.rotVec(change);
		change.shortestArc(newDirFlat,beamLine);
		offset.rotVec(change);

		LLPointer<LLHUDEffectSpiral> myBeam =  (LLHUDEffectSpiral *)LLHUDManager::getInstance()->createViewerEffect(LLHUDObject::LL_HUD_EFFECT_BEAM);
		myBeam->setPositionGlobal(mBeam->getPositionGlobal() + offset + (LLVector3d(beamLine) * sinf(gFrameTimeSeconds*2.0f) * 0.2f));
		
		myBeam->setColor(myColor);
		myBeam->setTargetObject(mBeam->getTargetObject());
		myBeam->setSourceObject(mBeam->getSourceObject());
		myBeam->setNeedsSendToSim(mBeam->getNeedsSendToSim());
		myBeam->setDuration(duration* 1.2f);
	}
}

void lggBeamMaps::forceUpdate()
{
	dots.clear();
	scale = 0.0f;
	lastFileName="";
}

F32 lggBeamMaps::setUpAndGetDuration()
{
	static LLCachedControl<std::string> settingNameCached(gSavedSettings, "FSBeamShape");
	std::string settingName(settingNameCached);
	if(settingName != lastFileName)
	{
		lastFileName = settingName;
		if( settingName != "===OFF===" && settingName != "")
		{

			std::string path_name(gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, "beams", ""));
			std::string path_name2(gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS , "beams", ""));
			std::string filename = path_name + settingName + ".xml";
			if (!gDirUtilp->fileExists(filename))
			{
				filename =path_name2 + settingName +".xml";
			}
			LLSD mydata = getPic(filename);
			scale = (F32)mydata["scale"].asReal()/10.0f;
			LLSD myPicture = mydata["data"];	
			dots.clear();
			for (int i = 0; i < myPicture.size(); i++)
			{
				LLSD beamData = myPicture[i];
				lggBeamData dot;
				
				dot.p = LLVector3d(beamData["offset"]);
				// <FS:PP> Attempt to speed up things a little
				// dot.p *= (gSavedSettings.getF32("FSBeamShapeScale")*2.0f);
				static LLCachedControl<F32> FSBeamShapeScale(gSavedSettings, "FSBeamShapeScale");
				dot.p *= (FSBeamShapeScale*2.0f);
				// </FS:PP>
				LLColor4 color = LLColor4(beamData["color"]);
				dot.c = LLColor4U(color);
				dots.push_back(dot);
			}
			
			// <FS:PP> Attempt to speed up things a little
			// F32 maxBPerQS = gSavedSettings.getF32("FSMaxBeamsPerSecond") / 4.0f;
			static LLCachedControl<F32> FSMaxBeamsPerSecond(gSavedSettings, "FSMaxBeamsPerSecond");
			F32 maxBPerQS = FSMaxBeamsPerSecond / 4.0f;
			// </FS:PP>
			duration = llceil((F32)(myPicture.size()) / maxBPerQS) * 0.25f;
			llinfos << "reading it all now size is " << myPicture.size() << " and duration is " << duration << llendl;
		
		}
		else
		{
			dots.clear();
			scale = 0.0f;//used as a flag too
			duration = 0.25f;
		}
		
	}
	//llinfos << "sent final dur of " << duration << llendl;
		
	return duration;
}


std::vector<std::string> lggBeamMaps::getFileNames()
{
	
	std::vector<std::string> names;	
	std::string path_name(gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, "beams", ""));
	bool found = true;			
	while(found) 
	{
		std::string name;
		found = gDirUtilp->getNextFileInDir(path_name, "*.xml", name);
		if(found)
		{

			name=name.erase(name.length()-4);

			// bugfix for SL-46920: preventing filenames that break stuff.
			char * curl_str = curl_unescape(name.c_str(), name.size());
			std::string unescaped_name(curl_str);
			curl_free(curl_str);
			curl_str = NULL;

			names.push_back(name);
			
			//LL_DEBUGS2("AppInit", "Shaders") << "name: " << name << LL_ENDL;
			//loadPreset(unescaped_name,FALSE);
		}
	}
	std::string path_name2(gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, "beams", ""));
	found = true;			
	while(found) 
	{
		std::string name;
		found = gDirUtilp->getNextFileInDir(path_name2, "*.xml", name);
		if(found)
		{

			name=name.erase(name.length()-4);

			// bugfix for SL-46920: preventing filenames that break stuff.
			char * curl_str = curl_unescape(name.c_str(), name.size());
			std::string unescaped_name(curl_str);
			curl_free(curl_str);
			curl_str = NULL;

			names.push_back(name);
			
			//LL_DEBUGS2("AppInit", "Shaders") << "name: " << name << LL_ENDL;
			//loadPreset(unescaped_name,FALSE);
		}
	}
	return names;

	

}

std::vector<std::string> lggBeamMaps::getColorsFileNames()
{

	std::vector<std::string> names;	
	std::string path_name(gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, "beamsColors", ""));
	bool found = true;			
	while(found) 
	{
		std::string name;
		found = gDirUtilp->getNextFileInDir(path_name, "*.xml", name);
		if(found)
		{

			name=name.erase(name.length()-4);

			// bugfix for SL-46920: preventing filenames that break stuff.
			char * curl_str = curl_unescape(name.c_str(), name.size());
			std::string unescaped_name(curl_str);
			curl_free(curl_str);
			curl_str = NULL;

			names.push_back(name);

			//LL_DEBUGS2("AppInit", "Shaders") << "name: " << name << LL_ENDL;
			//loadPreset(unescaped_name,FALSE);
		}
	}
	std::string path_name2(gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, "beamsColors", ""));
	found = true;			
	while(found) 
	{
		std::string name;
		found = gDirUtilp->getNextFileInDir(path_name2, "*.xml", name);
		if(found)
		{

			name=name.erase(name.length()-4);

			// bugfix for SL-46920: preventing filenames that break stuff.
			char * curl_str = curl_unescape(name.c_str(), name.size());
			std::string unescaped_name(curl_str);
			curl_free(curl_str);
			curl_str = NULL;

			names.push_back(name);

			//LL_DEBUGS2("AppInit", "Shaders") << "name: " << name << LL_ENDL;
			//loadPreset(unescaped_name,FALSE);
		}
	}
	return names;



}
void lggBeamMaps::stopBeamChat()
{
	// <FS:PP> Attempt to speed up things a little
	// if(gSavedSettings.getBOOL("FSParticleChat"))
	static LLCachedControl<bool> FSParticleChat(gSavedSettings, "FSParticleChat");
	if(FSParticleChat)
	// </FS:PP>
	{
		if(sPartsNow != FALSE)
		{
			sPartsNow = FALSE;
			LLMessageSystem* msg = gMessageSystem;
			msg->newMessageFast(_PREHASH_ChatFromViewer);
			msg->nextBlockFast(_PREHASH_AgentData);
			msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
			msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
			msg->nextBlockFast(_PREHASH_ChatData);
			msg->addStringFast(_PREHASH_Message, "stop");
			msg->addU8Fast(_PREHASH_Type, 0);
			msg->addS32("Channel", 9000);

			gAgent.sendReliableMessage();
			sBeamLastAt  =  LLVector3d::zero;
		}
	}
}
void lggBeamMaps::updateBeamChat(LLVector3d currentPos)
{
	// <FS:PP> Attempt to speed up things a little
	// if(gSavedSettings.getBOOL("FSParticleChat"))
	static LLCachedControl<bool> FSParticleChat(gSavedSettings, "FSParticleChat");
	if(FSParticleChat)
	// </FS:PP>
	{
		if(sPartsNow != TRUE)
		{
			sPartsNow = TRUE;
			LLMessageSystem* msg = gMessageSystem;
			msg->newMessageFast(_PREHASH_ChatFromViewer);
			msg->nextBlockFast(_PREHASH_AgentData);
			msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
			msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
			msg->nextBlockFast(_PREHASH_ChatData);
			msg->addStringFast(_PREHASH_Message, "start");
			msg->addU8Fast(_PREHASH_Type, 0);
			msg->addS32("Channel", 9000);

			gAgent.sendReliableMessage();
		}
		//LLVector3d a = sBeamLastAt-gAgent.mPointAt->getPointAtPosGlobal();
		//if(a.length > 2)
		if( (sBeamLastAt-currentPos).length() > .2)
			//if(sBeamLastAt!=gAgent.mPointAt->getPointAtPosGlobal())
		{
			sBeamLastAt = currentPos;

			LLMessageSystem* msg = gMessageSystem;
			msg->newMessageFast(_PREHASH_ChatFromViewer);
			msg->nextBlockFast(_PREHASH_AgentData);
			msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
			msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
			msg->nextBlockFast(_PREHASH_ChatData);
			msg->addStringFast(_PREHASH_Message, llformat("<%.6f, %.6f, %.6f>",(F32)(sBeamLastAt.mdV[VX]),(F32)(sBeamLastAt.mdV[VY]),(F32)(sBeamLastAt.mdV[VZ])));
			msg->addU8Fast(_PREHASH_Type, 0);
			msg->addS32("Channel", 9000); // *TODO: make configurable

			gAgent.sendReliableMessage();
		}

	}
}

