/* $LicenseInfo:firstyear=2011&license=viewerlgpl$
 * @file llpanelopenregionsettings.cpp
 * @brief Handler for OpenRegionInfo event queue message.
 * Copyright (C) 2011, Patrick Sapinski, Matthew Beardmore
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
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "llhttpnode.h"
#include "llviewercontrol.h"
#include "llagent.h"
#include "llsurface.h"
#include "llviewerregion.h"
#include "llviewerobject.h"
#include "llfloaterregioninfo.h"
#include "llworld.h"
#include "llagentcamera.h"
#include "llfloaterworldmap.h"
#include "llstartup.h"
#include "lldrawpoolterrain.h"
#include "llvlcomposition.h"

//DEBUG includes
#include "llsdserialize.h" //LLSDNotationStreamer - for dumping LLSD to string

class OpenRegionInfoUpdate : public LLHTTPNode
{
	/*virtual*/ void post(
		LLHTTPNode::ResponsePtr response,
		const LLSD& context,
		const LLSD& input) const
	{
		BOOL time_UTCDST = FALSE;

		if (!input || !context || !input.isMap() || !input.has("body"))
		{
			llinfos << "malformed OpenRegionInfo update!" << llendl;	 
			return;
		}

		std::string dump = input["body"].asString();
		llwarns << dump << llendl;

		LLWorld * regionlimits = LLWorld::getInstance();

		LLSD body = input["body"];
		//llinfos << "data: " << LLSDNotationStreamer(body) << llendl;	 
		//llinfos << "data: " << LLSDXMLStreamer(body) << llendl;

		//set the default limits/settings for this simulator type, as limits from our
		//previous region may not exist in this one
		regionlimits->refreshLimits();

		if ( body.has("AllowMinimap") )
		{
			regionlimits->setAllowMinimap(body["AllowMinimap"].asInteger() == 1);
		}
		if ( body.has("AllowPhysicalPrims") )
		{
			regionlimits->setAllowPhysicalPrims(body["AllowPhysicalPrims"].asInteger() == 1);
		}
		if ( gSavedSettings.getBOOL("OpenRegionSettingsEnableDrawDistance") )
		{
			if ( body.has("DrawDistance") )
			{
				F32 draw_distance = body["DrawDistance"].asReal();
				if (draw_distance > 0)
				{
					gAgentCamera.mDrawDistance = draw_distance;
					LLWorld::getInstance()->setLandFarClip(draw_distance);
				}
				regionlimits->setDrawDistance(body["DrawDistance"].asReal());
			}
			if ( body.has("ForceDrawDistance") )
			{
				regionlimits->setLockedDrawDistance(body["ForceDrawDistance"].asInteger() == 1  ? TRUE : FALSE);
			}
		}
		if ( body.has("LSLFunctions") )
		{
			//IMPLEMENT ME
		}
		if ( body.has("TerrainDetailScale") )
		{
			//gAgent.getRegion()->getComposition()->setScaleParams(body["TerrainDetailScale"].asReal(), body["TerrainDetailScale"].asReal());

			regionlimits->setTerrainDetailScale(body["TerrainDetailScale"].asReal());
			gSavedSettings.setF32("RenderTerrainScale", body["TerrainDetailScale"].asReal());
			LLDrawPoolTerrain::sDetailScale = 1.f/body["TerrainDetailScale"].asReal();
		}
		if ( body.has("MaxDragDistance") )
		{
			regionlimits->setMaxDragDistance(body["MaxDragDistance"].asReal());
		}
		if ( body.has("MinHoleSize") )
		{
			regionlimits->setRegionMinHoleSize(body["MinHoleSize"].asReal());
		}
		if ( body.has("MaxHollowSize") )
		{
			regionlimits->setRegionMaxHollowSize(body["MaxHollowSize"].asReal());
		}
		if ( body.has("MaxInventoryItemsTransfer") )
		{
			regionlimits->setMaxInventoryItemsTransfer(body["MaxInventoryItemsTransfer"].asReal());
		}
		if ( body.has("MaxLinkCount") )
		{
			regionlimits->setMaxLinkedPrims(body["MaxLinkCount"].asInteger());
		}
		if ( body.has("MaxLinkCountPhys") )
		{
			regionlimits->setMaxPhysLinkedPrims(body["MaxLinkCountPhys"].asInteger());
		}
		if ( body.has("MaxPos") )
		{
			regionlimits->setMaxPrimXPos(body["MaxPosX"].asReal());
			regionlimits->setMaxPrimYPos(body["MaxPosY"].asReal());
			regionlimits->setMaxPrimZPos(body["MaxPosZ"].asReal());
		}
		if ( body.has("MinPos") )
		{
			regionlimits->setMinPrimXPos(body["MinPosX"].asReal());
			regionlimits->setMinPrimYPos(body["MinPosY"].asReal());
			regionlimits->setMinPrimZPos(body["MinPosZ"].asReal());
		}
		if ( body.has("MaxPrimScale") )
		{
			regionlimits->setRegionMaxPrimScale(body["MaxPrimScale"].asReal());
			regionlimits->setRegionMaxPrimScaleNoMesh(body["MaxPrimScale"].asReal());
		}
		if ( body.has("MaxPhysPrimScale") )
		{
			regionlimits->setMaxPhysPrimScale(body["MaxPhysPrimScale"].asReal());
		}
		if ( body.has("MinPrimScale") )
		{
			regionlimits->setRegionMinPrimScale(body["MinPrimScale"].asReal());
		}
		if ( body.has("OffsetOfUTCDST") )
		{
			time_UTCDST = body["OffsetOfUTCDST"].asInteger() == 1 ? TRUE : FALSE;
		}
		if ( body.has("OffsetOfUTC") )
		{
			gUTCOffset = body["OffsetOfUTC"].asInteger();
			if(time_UTCDST) gUTCOffset++;
		}
		if ( body.has("RenderWater") )
		{
			regionlimits->setAllowRenderWater(body["RenderWater"].asInteger() == 1 ? TRUE : FALSE);
		}
		if ( body.has("SayDistance") )
		{
			regionlimits->setSayDistance(body["SayDistance"].asReal());
		}
		if ( body.has("ShoutDistance") )
		{
			regionlimits->setShoutDistance(body["ShoutDistance"].asReal());
		}
		if ( body.has("WhisperDistance") )
		{
			regionlimits->setWhisperDistance(body["WhisperDistance"].asReal());
		}
		if ( body.has("ToggleTeenMode") )
		{
			regionlimits->setEnableTeenMode(body["ToggleTeenMode"].asInteger() == 1 ? TRUE : FALSE);
		}
		if ( body.has("ShowTags") )
		{
			regionlimits->setAllowRenderName(body["ShowTags"].asReal());
		}
		if ( body.has("EnforceMaxBuild") )
		{
			regionlimits->setEnforceMaxBuild(body["EnforceMaxBuild"].asInteger() == 1 ? TRUE : FALSE);
		}
		if ( body.has("MaxGroups") )
		{
			gMaxAgentGroups = body["MaxGroups"].asInteger();
		}
		if ( body.has("AllowParcelWindLight") )
		{
			regionlimits->setAllowParcelWindLight(body["AllowParcelWindLight"].asInteger() == 1);
		}

		regionlimits->updateLimits();
	}
};

LLHTTPRegistration<OpenRegionInfoUpdate>
gHTTPRegistrationOpenRegionInfoUpdate(
	"/message/OpenRegionInfo");
