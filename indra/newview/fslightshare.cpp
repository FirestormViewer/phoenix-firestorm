/*
 * @file fslightshare.cpp
 * @brief Firestorm Lightshare handler
 *
 * $LicenseInfo:firstyear=2012&license=fsviewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (C) 2012, Cinder Roxley <cinder@cinderblocks.biz>
 * Lightshare is a registered trademark of Magne Metaverse Research, LLC
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
 * The Phoenix Firestorm Project, Inc., 1831 Oakwood Drive, Fairmont, Minnesota 56031-3225 USA
 * http://www.phoenixviewer.com
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "fslightshare.h"
#include "llviewermessage.h"
#include "llwaterparammanager.h"
#include "llwlparammanager.h"
#include "llenvmanager.h"
#include "llviewercontrol.h"
#include "llstatusbar.h"

FSLightshare::FSLightshare() : mLightshareState(false)
{}

// virtual
FSLightshare::~FSLightshare()
{}

void FSLightshare::processLightshareMessage(LLMessageSystem* msg)
{
	/// <FS:CR> TODO: It would be nice to handle this per event like parcel wl rather than a blanket boolean turning
	/// Lightshare on or off
	if (!gSavedSettings.getBOOL("FSOpenSimLightshare"))
	{
		llinfos << "Received Lightshare message from the region, but Lightshare is disabled." << llendl;
		return;
	}
	// Be paranoid!
	if (!msg)
	{
		return;
	}
	std::string method;
	msg->getStringFast(_PREHASH_MethodData, _PREHASH_Method, method);
	if (method != "Windlight")
	{
		return;
	}
	S32 size = msg->getSizeFast(_PREHASH_ParamList, 0, _PREHASH_Parameter);
	if (size < 0 || 250 < size)
	{
		return;
	}
	// Ok, it checks out (sorta)
	
	S32 count = msg->getNumberOfBlocksFast(_PREHASH_ParamList);
	for (S32 i = 0; i < count; ++i)
	{
		S32 size = msg->getSizeFast(_PREHASH_ParamList, i, _PREHASH_Parameter);
		if (size >= 0)
		{
			char buffer[250];
			msg->getBinaryDataFast(_PREHASH_ParamList, _PREHASH_Parameter, buffer, size, i, 249);
			LightsharePacket* ls_packet = (LightsharePacket*)buffer; // <-- warning! ugly stupid, not byte-order safe!

			llinfos << "Received Lightshare message from the region, processing it." << llendl;
			processWater(ls_packet);
			processSky(ls_packet);
		}
	}
	setState(true);
	gStatusBar->updateParcelIcons();
}

void FSLightshare::processLightshareRefresh()
{
	LLEnvManagerNew::getInstance()->usePrefs();
	setState(false);
	gStatusBar->updateParcelIcons();
}

// static
void FSLightshare::processSky(LightsharePacket* ls_packet)
{
	LLWLParamManager* wlparammgr = LLWLParamManager::getInstance();
	wlparammgr->mAnimator.deactivate();
	LLWLParamSet & param_set = wlparammgr->mCurParams;
	
	param_set.setSunAngle(F_TWO_PI * ls_packet->sunMoonPosiiton);
	param_set.setEastAngle(F_TWO_PI * ls_packet->eastAngle);
	param_set.set("sunlight_color", ls_packet->sunMoonColor.red * 3.0f, ls_packet->sunMoonColor.green * 3.0f, ls_packet->sunMoonColor.blue * 3.0f, ls_packet->sunMoonColor.alpha * 3.0f);
	param_set.set("ambient", ls_packet->ambient.red * 3.0f, ls_packet->ambient.green * 3.0f, ls_packet->ambient.blue * 3.0f, ls_packet->ambient.alpha * 3.0f);
	param_set.set("blue_horizon", ls_packet->horizon.red * 2.0f, ls_packet->horizon.green *2.0f, ls_packet->horizon.blue * 2.0f, ls_packet->horizon.alpha * 2.0f);
	param_set.set("blue_density", ls_packet->blueDensity.red * 2.0f, ls_packet->blueDensity.green * 2.0f, ls_packet->blueDensity.blue * 2.0f, ls_packet->blueDensity.alpha * 2.0f);
	param_set.set("haze_horizon", ls_packet->hazeHorizon, ls_packet->hazeHorizon, ls_packet->hazeHorizon, 1.f);
	param_set.set("haze_density", ls_packet->hazeDensity, ls_packet->hazeDensity, ls_packet->hazeDensity, 1.f);
	param_set.set("cloud_shadow", ls_packet->cloudCoverage, ls_packet->cloudCoverage, ls_packet->cloudCoverage, ls_packet->cloudCoverage);
	param_set.set("density_multiplier", ls_packet->densityMultiplier / 1000.0f);
	param_set.set("distance_multiplier", ls_packet->distanceMultiplier, ls_packet->distanceMultiplier, ls_packet->distanceMultiplier, ls_packet->distanceMultiplier);
	param_set.set("max_y",(F32)ls_packet->maxAltitude);
	param_set.set("cloud_color", ls_packet->cloudColor.red, ls_packet->cloudColor.green, ls_packet->cloudColor.blue, ls_packet->cloudColor.alpha);
	param_set.set("cloud_pos_density1", ls_packet->cloudXYDensity.X, ls_packet->cloudXYDensity.Y, ls_packet->cloudXYDensity.Z);
	param_set.set("cloud_pos_density2", ls_packet->cloudDetailXYDensity.X, ls_packet->cloudDetailXYDensity.Y, ls_packet->cloudDetailXYDensity.Z);
	param_set.set("cloud_scale", ls_packet->cloudScale, 0.f, 0.f, 1.f);
	param_set.set("gamma", ls_packet->sceneGamma, ls_packet->sceneGamma, ls_packet->sceneGamma, 0.0f);
	param_set.set("glow",(2 - ls_packet->sunGlowSize) * 20 , 0.f, -ls_packet->sunGlowFocus * 5);
	param_set.setCloudScrollX(ls_packet->cloudScrollX + 10.0f);
	param_set.setCloudScrollY(ls_packet->cloudScrollY + 10.0f);
	param_set.setEnableCloudScrollX(!ls_packet->cloudScrollXLock);
	param_set.setEnableCloudScrollY(!ls_packet->cloudScrollYLock);
	param_set.setStarBrightness(ls_packet->starBrightness);
	
	LLWLParamKey ls_key;
	ls_key.name = "LightshareCurrentRegion";
	ls_key.scope = LLEnvKey::SCOPE_LOCAL;
	wlparammgr->setParamSet(ls_key, param_set);
	
	wlparammgr->propagateParameters();
}

// static
void FSLightshare::processWater(LightsharePacket* ls_packet)
{
	LLWaterParamManager* wwparammgr = LLWaterParamManager::getInstance();
	LLWaterParamSet & param_set = wwparammgr->mCurParams;
	
	param_set.set("waterFogColor", ls_packet->waterColor.red / 256.f, ls_packet->waterColor.green / 256.f, ls_packet->waterColor.blue / 256.f);
	param_set.set("waterFogDensity", pow(2.0f, ls_packet->waterFogDensityExponent));
	param_set.set("underWaterFogMod", ls_packet->underwaterFogModifier);
	param_set.set("normScale", ls_packet->reflectionWaveletScale.X,ls_packet->reflectionWaveletScale.Y,ls_packet->reflectionWaveletScale.Z);
	param_set.set("fresnelScale", ls_packet->fresnelScale);
	param_set.set("fresnelOffset", ls_packet->fresnelOffset);
	param_set.set("scaleAbove", ls_packet->refractScaleAbove);
	param_set.set("scaleBelow", ls_packet->refractScaleBelow);
	param_set.set("blurMultiplier", ls_packet->blurMultiplier);
	param_set.set("wave1Dir", ls_packet->littleWaveDirection.X, ls_packet->littleWaveDirection.Y);
	param_set.set("wave2Dir", ls_packet->bigWaveDirection.X, ls_packet->bigWaveDirection.Y);
	
	LLUUID normalMapTexture;
	
	std::string out = llformat("%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
							   (U8)(ls_packet->normalMapTexture[0]),
							   (U8)(ls_packet->normalMapTexture[1]),
							   (U8)(ls_packet->normalMapTexture[2]),
							   (U8)(ls_packet->normalMapTexture[3]),
							   (U8)(ls_packet->normalMapTexture[4]),
							   (U8)(ls_packet->normalMapTexture[5]),
							   (U8)(ls_packet->normalMapTexture[6]),
							   (U8)(ls_packet->normalMapTexture[7]),
							   (U8)(ls_packet->normalMapTexture[8]),
							   (U8)(ls_packet->normalMapTexture[9]),
							   (U8)(ls_packet->normalMapTexture[10]),
							   (U8)(ls_packet->normalMapTexture[11]),
							   (U8)(ls_packet->normalMapTexture[12]),
							   (U8)(ls_packet->normalMapTexture[13]),
							   (U8)(ls_packet->normalMapTexture[14]),
							   (U8)(ls_packet->normalMapTexture[15]));
	
	normalMapTexture.set(out);
	
	wwparammgr->setNormalMapID(normalMapTexture);
	wwparammgr->setParamSet("LightshareCurrentRegion", param_set);
	wwparammgr->setNormalMapID(normalMapTexture);
	
	wwparammgr->propagateParameters();
}

void FSLightshare::setState(bool has_lightshare)
{
		mLightshareState = has_lightshare;
}
