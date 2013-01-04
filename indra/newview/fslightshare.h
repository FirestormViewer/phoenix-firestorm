/*
 * @file fslightshare.h
 * @brief Firestorm Lightshare handler definitions
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
 * http://www.firestormviewer.org
 * $/LicenseInfo$
 */

#ifndef FS_LIGHTSHARE_H
#define FS_LIGHTSHARE_H

#include "llsingleton.h"

struct LSColor3
{
	LSColor3() {};
	LSColor3(F32 pRed, F32 pGreen, F32 pBlue)
	{
		red=pRed;
		green=pGreen;
		blue=pBlue;
	}
	F32	red;
	F32	green;
	F32	blue;
};

struct LSVector3
{
	LSVector3() {}
	LSVector3(F32 pX, F32 pY, F32 pZ)
	{
		X=pX;
		Y=pY;
		Z=pZ;
	}
	F32	X;
	F32	Y;
	F32	Z;
};

struct LSVector2
{
	LSVector2() {}
	LSVector2(F32 pX, F32 pY)
	{
		X=pX;
		Y=pY;
	}
	F32	X;
	F32	Y;
};

struct LSColor4
{
	LSColor4() {}
	LSColor4(F32 pRed, F32 pGreen, F32 pBlue, F32 pAlpha)
	{
		red=pRed;
		green=pGreen;
		blue=pBlue;
		alpha=pAlpha;
	}
	F32	red;
	F32	green;
	F32	blue;
	F32 alpha;
};

struct LightsharePacket
{
	LightsharePacket() {}
	LSColor3 waterColor;
	F32 waterFogDensityExponent;
	F32 underwaterFogModifier;
	LSVector3 reflectionWaveletScale;
	F32 fresnelScale;
	F32 fresnelOffset;
	F32 refractScaleAbove;
	F32 refractScaleBelow;
	F32 blurMultiplier;
	LSVector2 littleWaveDirection;
	LSVector2 bigWaveDirection;
	unsigned char normalMapTexture[16];
	LSColor4 horizon;
	F32 hazeHorizon;
	LSColor4 blueDensity;
	F32 hazeDensity;
	F32 densityMultiplier;
	F32 distanceMultiplier;
	LSColor4 sunMoonColor;
	F32 sunMoonPosiiton;
	LSColor4 ambient;
	F32 eastAngle;
	F32 sunGlowFocus;
	F32 sunGlowSize;
	F32 sceneGamma;
	F32 starBrightness;
	LSColor4 cloudColor;
	LSVector3 cloudXYDensity;
	F32 cloudCoverage;
	F32 cloudScale;
	LSVector3 cloudDetailXYDensity;
	F32 cloudScrollX;
	F32 cloudScrollY;
	unsigned short maxAltitude;
	char cloudScrollXLock;
	char cloudScrollYLock;
	char drawClassicClouds;
};

class FSLightshare : public LLSingleton<FSLightshare>
{
	LOG_CLASS(FSLightshare);
public:
	FSLightshare();
	virtual ~FSLightshare();
	void processLightshareMessage(LLMessageSystem* msg);
	void processLightshareRefresh();
	bool getState() { return mLightshareState; };
private:
	friend class LLSingleton<FSLightshare>;
	static void processWater(LightsharePacket* ls_packet);
	static void processSky(LightsharePacket* ls_packet);
	void setState(bool has_lightshare);
	
	bool mLightshareState;
};

#endif // FS_LIGHTSHARE_H
