/** 
 * @file llnetmap.cpp
 * @author James Cook
 * @brief Display of surrounding regions, objects, and agents. 
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2001-2010, Linden Research, Inc.
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

#include "llviewerprecompiledheaders.h"

#include "llnetmap.h"

// Library includes (should move below)
#include "indra_constants.h"
#include "llavatarnamecache.h"
#include "llmath.h"
#include "llfloaterreg.h"
#include "llfocusmgr.h"
#include "lllocalcliprect.h"
#include "llrender.h"
#include "llui.h"
#include "lltooltip.h"

#include "llglheaders.h"

// Viewer includes
#include "llagent.h"
#include "llagentcamera.h"
#include "llappviewer.h" // for gDisconnected
// [SL:KB] - Patch: World-MiniMap | Checked: 2012-07-08 (Catznip-3.3.0)
#include "llavataractions.h"
#include "llfloatersidepanelcontainer.h"
// [/SL:KB]
#include "llcallingcard.h" // LLAvatarTracker
#include "llfloaterworldmap.h"
// [SL:KB] - Patch: World-MinimapOverlay | Checked: 2012-06-20 (Catznip-3.3.0)
#include "llparcel.h"
// [/SL:KB]
#include "lltracker.h"
#include "llsurface.h"
// [SL:KB] - Patch: World-MiniMap | Checked: 2012-07-08 (Catznip-3.3.0)
#include "lltrans.h"
// [/SL:KB]
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewertexture.h"
#include "llviewertexturelist.h"
#include "llviewermenu.h"
#include "llviewerobjectlist.h"
// [SL:KB] - Patch: World-MinimapOverlay | Checked: 2012-06-20 (Catznip-3.3.0)
#include "llviewerparcelmgr.h"
#include "llviewerparceloverlay.h"
// [/SL:KB]
#include "llviewerregion.h"
#include "llviewerwindow.h"
#include "llworld.h"
#include "llworldmapview.h"		// shared draw code
// [RLVa:KB] - Checked: 2010-04-19 (RLVa-1.2.0f)
#include "rlvhandler.h"
// [/RLVa:KB]
#include "lltrans.h"
#include "llmutelist.h"

// Ansariel: For accessing the radar data
#include "llavatarlist.h"
#include "llavatarlistitem.h"
#include "llpanelpeople.h"
#include "llfloatersidepanelcontainer.h"
#include "lggcontactsets.h"

#include "llavataractions.h"
#include "fscommon.h"

static LLDefaultChildRegistry::Register<LLNetMap> r1("net_map");

const F32 LLNetMap::MAP_SCALE_MIN = 32;
const F32 LLNetMap::MAP_SCALE_MID = 1024;
const F32 LLNetMap::MAP_SCALE_MAX = 4096;

const F32 MAP_SCALE_INCREMENT = 16;
const F32 MAP_SCALE_ZOOM_FACTOR = 1.04f; // Zoom in factor per click of scroll wheel (4%)
const F32 MIN_DOT_RADIUS = 3.5f;
const F32 DOT_SCALE = 0.75f;
const F32 MIN_PICK_SCALE = 2.f;
const S32 MOUSE_DRAG_SLOP = 2;		// How far the mouse needs to move before we think it's a drag

const F64 COARSEUPDATE_MAX_Z = 1020.0f;

const F32 WIDTH_PIXELS = 2.f;
const S32 CIRCLE_STEPS = 100;

std::map<LLUUID, LLColor4> LLNetMap::sAvatarMarksMap; // <FS:Ansariel>
F32 LLNetMap::sScale; // <FS:Ansariel> Synchronizing netmaps throughout instances

// <FS:Ansariel> Synchronize tooltips throughout instances
std::string LLNetMap::sToolTipMsg;
// </FS:Ansariel> Synchronize tooltips throughout instances

LLNetMap::LLNetMap (const Params & p)
:	LLUICtrl (p),
	mBackgroundColor (p.bg_color()),
	mScale( MAP_SCALE_MID ),
	mPixelsPerMeter( MAP_SCALE_MID / REGION_WIDTH_METERS ),
	mObjectMapTPM(0.f),
	mObjectMapPixels(0.f),
	mTargetPan(0.f, 0.f),
	mCurPan(0.f, 0.f),
	mStartPan(0.f, 0.f),
	mMouseDown(0, 0),
	mPanning(false),
//	mUpdateNow(false),
// [SL:KB] - Patch: World-MinimapOverlay | Checked: 2012-06-20 (Catznip-3.3.0)
	mUpdateObjectImage(false),
	mUpdateParcelImage(false),
// [/SL:KB]
	mObjectImageCenterGlobal( gAgentCamera.getCameraPositionGlobal() ),
	mObjectRawImagep(),
	mObjectImagep(),
// [SL:KB] - Patch: World-MinimapOverlay | Checked: 2012-06-20 (Catznip-3.3.0)
	mParcelImageCenterGlobal( gAgentCamera.getCameraPositionGlobal() ),
	mParcelRawImagep(),
	mParcelImagep(),
// [/SL:KB]
	mClosestAgentToCursor(),
//	mClosestAgentAtLastRightClick(),
	// <FS:Ansariel> Synchronize tooltips throughout instances
	//mToolTipMsg(),
	mPopupMenu(NULL)
{
	mDotRadius = llmax(DOT_SCALE * mPixelsPerMeter, MIN_DOT_RADIUS);
	setScale(gSavedSettings.getF32("MiniMapScale"));
}

LLNetMap::~LLNetMap()
{
	// <FS:Ansariel> Fixing borked minimap zoom level persistance
	//gSavedSettings.setF32("MiniMapScale", mScale);
	gSavedSettings.setF32("MiniMapScale", sScale);
	// </FS:Ansariel> Fixing borked minimap zoom level persistance
}

BOOL LLNetMap::postBuild()
{
	LLUICtrl::CommitCallbackRegistry::ScopedRegistrar registrar;
	
	registrar.add("Minimap.Zoom", boost::bind(&LLNetMap::handleZoom, this, _2));
	registrar.add("Minimap.Tracker", boost::bind(&LLNetMap::handleStopTracking, this, _2));
	// <FS:Ansariel>
	registrar.add("Minimap.Mark", boost::bind(&LLNetMap::handleMark, this, _2));
	registrar.add("Minimap.ClearMarks", boost::bind(&LLNetMap::handleClearMarks, this));
	// </FS:Ansariel>
	registrar.add("Minimap.Cam", boost::bind(&LLNetMap::handleCam, this));
	registrar.add("Minimap.StartTracking", boost::bind(&LLNetMap::handleStartTracking, this));
// [SL:KB] - Patch: World-MiniMap | Checked: 2012-07-08 (Catznip-3.3.0)
	registrar.add("Minimap.ShowProfile", boost::bind(&LLNetMap::handleShowProfile, this, _2));
	registrar.add("Minimap.TextureType", boost::bind(&LLNetMap::handleTextureType, this, _2));
	registrar.add("Minimap.ToggleOverlay", boost::bind(&LLNetMap::handleOverlayToggle, this, _2));

	LLUICtrl::EnableCallbackRegistry::ScopedRegistrar enable_registrar;
	enable_registrar.add("Minimap.CheckTextureType", boost::bind(&LLNetMap::checkTextureType, this, _2));
// [/SL:KB]

// [SL:KB] - Patch: World-MinimapOverlay | Checked: 2012-06-20 (Catznip-3.3.0)
	LLViewerParcelMgr::instance().setCollisionUpdateCallback(boost::bind(&LLNetMap::refreshParcelOverlay, this));
	LLViewerParcelOverlay::setUpdateCallback(boost::bind(&LLNetMap::refreshParcelOverlay, this));
// [/SL:KB]

	mPopupMenu = LLUICtrlFactory::getInstance()->createFromFile<LLMenuGL>("menu_mini_map.xml", gMenuHolder, LLViewerMenuHolderGL::child_registry_t::instance());

	// <FS:Ansariel> Synchronize tooltips throughout instances
	LLNetMap::updateToolTipMsg();
	return TRUE;
}

void LLNetMap::setScale( F32 scale )
{
	scale = llclamp(scale, MAP_SCALE_MIN, MAP_SCALE_MAX);
	mCurPan *= scale / mScale;
	mScale = scale;
	// <FS:Ansariel> Synchronize scale throughout instances
	sScale = scale;
	
	if (mObjectImagep.notNull())
	{
		F32 width = (F32)(getRect().getWidth());
		F32 height = (F32)(getRect().getHeight());
		F32 diameter = sqrt(width * width + height * height);
		F32 region_widths = diameter / mScale;
// <FS:CR> Aurora Sim
		//F32 meters = region_widths * LLWorld::getInstance()->getRegionWidthInMeters();
		F32 meters = region_widths * REGION_WIDTH_METERS;
// </FS:CR> Aurora Sim
		F32 num_pixels = (F32)mObjectImagep->getWidth();
		mObjectMapTPM = num_pixels / meters;
		mObjectMapPixels = diameter;
	}

	mPixelsPerMeter = mScale / REGION_WIDTH_METERS;
	mDotRadius = llmax(DOT_SCALE * mPixelsPerMeter, MIN_DOT_RADIUS);

// [SL:KB] - Patch: World-MinimapOverlay | Checked: 2012-06-20 (Catznip-3.3.0)
	mUpdateObjectImage = true;
	mUpdateParcelImage = true;
// [/SL:KB]
//	mUpdateNow = true;
}


///////////////////////////////////////////////////////////////////////////////////

void LLNetMap::draw()
{
	// <FS:Ansariel>: Synchronize netmap scale throughout instances
	if (mScale != sScale)
	{
		setScale(sScale);
	}
	// </FS:Ansariel>: Synchronize netmap scale throughout instances

 	static LLFrameTimer map_timer;
	static LLUIColor map_avatar_color = LLUIColorTable::instance().getColor("MapAvatarColor", LLColor4::white);
	static LLUIColor map_avatar_friend_color = LLUIColorTable::instance().getColor("MapAvatarFriendColor", LLColor4::white);
	static LLUIColor map_avatar_linden_color = LLUIColorTable::instance().getColor("MapAvatarLindenColor", LLColor4::blue);
	static LLUIColor map_avatar_muted_color = LLUIColorTable::instance().getColor("MapAvatarMutedColor", LLColor4::grey3);
	static LLUIColor map_track_color = LLUIColorTable::instance().getColor("MapTrackColor", LLColor4::white);
	//static LLUIColor map_track_disabled_color = LLUIColorTable::instance().getColor("MapTrackDisabledColor", LLColor4::white);
	static LLUIColor map_frustum_color = LLUIColorTable::instance().getColor("MapFrustumColor", LLColor4::white);
	static LLUIColor map_frustum_rotating_color = LLUIColorTable::instance().getColor("MapFrustumRotatingColor", LLColor4::white);
	static LLUIColor map_chat_ring_color = LLUIColorTable::instance().getColor("MapChatRingColor", LLColor4::yellow);
	static LLUIColor map_shout_ring_color = LLUIColorTable::instance().getColor("MapShoutRingColor", LLColor4::red);
// [SL:KB] - Patch: World-MinimapOverlay | Checked: 2012-08-17 (Catznip-3.3.0)
	static LLUIColor map_property_line = LLUIColorTable::instance().getColor("MiniMapPropertyLine", LLColor4::white);
// [/SL:KB]
	
	if (mObjectImagep.isNull())
	{
		createObjectImage();
	}

// [SL:KB] - Patch: World-MinimapOverlay | Checked: 2012-06-20 (Catznip-3.3.0)
	if (mParcelImagep.isNull())
	{
		createParcelImage();
	}
// [/SL:KB]

	static LLUICachedControl<bool> auto_center("MiniMapAutoCenter", true);
	if (auto_center)
	{
		mCurPan = lerp(mCurPan, mTargetPan, LLCriticalDamp::getInterpolant(0.1f));
	}

	// Prepare a scissor region
	F32 rotation = 0;

	gGL.pushMatrix();
	gGL.pushUIMatrix();
	
	LLVector3 offset = gGL.getUITranslation();
	LLVector3 scale = gGL.getUIScale();

	gGL.loadIdentity();
	gGL.loadUIIdentity();

	gGL.scalef(scale.mV[0], scale.mV[1], scale.mV[2]);
	gGL.translatef(offset.mV[0], offset.mV[1], offset.mV[2]);
	
	{
		LLLocalClipRect clip(getLocalRect());
		{
			gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);

			gGL.matrixMode(LLRender::MM_MODELVIEW);

			// Draw background rectangle
			LLColor4 background_color = mBackgroundColor.get();
			gGL.color4fv( background_color.mV );
			gl_rect_2d(0, getRect().getHeight(), getRect().getWidth(), 0);
		}

		// region 0,0 is in the middle
		S32 center_sw_left = getRect().getWidth() / 2 + llfloor(mCurPan.mV[VX]);
		S32 center_sw_bottom = getRect().getHeight() / 2 + llfloor(mCurPan.mV[VY]);

		gGL.pushMatrix();

		gGL.translatef( (F32) center_sw_left, (F32) center_sw_bottom, 0.f);

		static LLUICachedControl<bool> rotate_map("MiniMapRotate", true);
		if( rotate_map )
		{
			// rotate subsequent draws to agent rotation
			rotation = atan2( LLViewerCamera::getInstance()->getAtAxis().mV[VX], LLViewerCamera::getInstance()->getAtAxis().mV[VY] );
			gGL.rotatef( rotation * RAD_TO_DEG, 0.f, 0.f, 1.f);
		}

		// figure out where agent is
// <FS:CR> Aurora Sim
		//S32 region_width = llround(LLWorld::getInstance()->getRegionWidthInMeters());
		S32 region_width = llround(REGION_WIDTH_METERS);
// </FS:CR> Aurora Sim

		for (LLWorld::region_list_t::const_iterator iter = LLWorld::getInstance()->getRegionList().begin();
			 iter != LLWorld::getInstance()->getRegionList().end(); ++iter)
		{
			LLViewerRegion* regionp = *iter;
			// Find x and y position relative to camera's center.
			LLVector3 origin_agent = regionp->getOriginAgent();
			LLVector3 rel_region_pos = origin_agent - gAgentCamera.getCameraPositionAgent();
			F32 relative_x = (rel_region_pos.mV[0] / region_width) * mScale;
			F32 relative_y = (rel_region_pos.mV[1] / region_width) * mScale;

			// background region rectangle
			F32 bottom =	relative_y;
			F32 left =		relative_x;
// <FS:CR> Aurora Sim
			//F32 top =		bottom + mScale ;
			//F32 right =		left + mScale ;
			F32 top =		bottom + (regionp->getWidth() / region_width) * mScale ;
			F32 right =		left + (regionp->getWidth() / region_width) * mScale ;
// </FS:CR> Aurora Sim

			if (regionp == gAgent.getRegion())
			{
				gGL.color4f(1.f, 1.f, 1.f, 1.f);
			}
			else
			{
				gGL.color4f(0.8f, 0.8f, 0.8f, 1.f);
			}

			if (!regionp->isAlive())
			{
				gGL.color4f(1.f, 0.5f, 0.5f, 1.f);
			}

// [SL:KB] - Patch: World-MinimapOverlay | Checked: 2012-07-26 (Catznip-3.3)
			static LLCachedControl<bool> s_fUseWorldMapTextures(gSavedSettings, "MiniMapWorldMapTextures") ;
			bool fRenderTerrain = true;

			if (s_fUseWorldMapTextures)
			{
				LLViewerTexture* pRegionImage = regionp->getWorldMapTile();
				if ( (pRegionImage) && (pRegionImage->hasGLTexture()) )
				{
					gGL.getTexUnit(0)->bind(pRegionImage);
					gGL.begin(LLRender::QUADS);
						gGL.texCoord2f(0.f, 1.f);
						gGL.vertex2f(left, top);
						gGL.texCoord2f(0.f, 0.f);
						gGL.vertex2f(left, bottom);
						gGL.texCoord2f(1.f, 0.f);
						gGL.vertex2f(right, bottom);
						gGL.texCoord2f(1.f, 1.f);
						gGL.vertex2f(right, top);
					gGL.end();

					pRegionImage->setBoostLevel(LLViewerTexture::BOOST_MAP_VISIBLE);
					fRenderTerrain = false;
				}
			}
// [/SL:KB]

// [SL:KB] - Patch: World-MinimapOverlay | Checked: 2012-07-26 (Catznip-3.3)
			if (fRenderTerrain)
			{
// [/SL:KB]
				// Draw using texture.
				gGL.getTexUnit(0)->bind(regionp->getLand().getSTexture());
				gGL.begin(LLRender::QUADS);
					gGL.texCoord2f(0.f, 1.f);
					gGL.vertex2f(left, top);
					gGL.texCoord2f(0.f, 0.f);
					gGL.vertex2f(left, bottom);
					gGL.texCoord2f(1.f, 0.f);
					gGL.vertex2f(right, bottom);
					gGL.texCoord2f(1.f, 1.f);
					gGL.vertex2f(right, top);
				gGL.end();

				// Draw water
				gGL.setAlphaRejectSettings(LLRender::CF_GREATER, ABOVE_WATERLINE_ALPHA / 255.f);
				{
					if (regionp->getLand().getWaterTexture())
					{
						gGL.getTexUnit(0)->bind(regionp->getLand().getWaterTexture());
						gGL.begin(LLRender::QUADS);
							gGL.texCoord2f(0.f, 1.f);
							gGL.vertex2f(left, top);
							gGL.texCoord2f(0.f, 0.f);
							gGL.vertex2f(left, bottom);
							gGL.texCoord2f(1.f, 0.f);
							gGL.vertex2f(right, bottom);
							gGL.texCoord2f(1.f, 1.f);
							gGL.vertex2f(right, top);
						gGL.end();
					}
				}
				gGL.setAlphaRejectSettings(LLRender::CF_DEFAULT);
// [SL:KB] - Patch: World-MinimapOverlay | Checked: 2012-07-26 (Catznip-3.3)
			}
// [/SL:KB]
		}

		// Redraw object layer periodically
//		if (mUpdateNow || (map_timer.getElapsedTimeF32() > 0.5f))
//		{
//			mUpdateNow = false;
// [SL:KB] - Patch: World-MinimapOverlay | Checked: 2012-06-20 (Catznip-3.3.0)
		// Locate the center
		LLVector3 posCenter = globalPosToView(gAgentCamera.getCameraPositionGlobal());
		posCenter.mV[VX] -= mCurPan.mV[VX];
		posCenter.mV[VY] -= mCurPan.mV[VY];
		posCenter.mV[VZ] = 0.f;
		LLVector3d posCenterGlobal = viewPosToGlobal(llfloor(posCenter.mV[VX]), llfloor(posCenter.mV[VY]));

		static LLCachedControl<bool> s_fShowObjects(gSavedSettings, "MiniMapObjects") ;
		if ( (s_fShowObjects) && ((mUpdateObjectImage) || ((map_timer.getElapsedTimeF32() > 0.5f))) )
		{
			mUpdateObjectImage = false;
// [/SL:KB]

//			// Locate the centre of the object layer, accounting for panning
//			LLVector3 new_center = globalPosToView(gAgentCamera.getCameraPositionGlobal());
//			new_center.mV[VX] -= mCurPan.mV[VX];
//			new_center.mV[VY] -= mCurPan.mV[VY];
//			new_center.mV[VZ] = 0.f;
//			mObjectImageCenterGlobal = viewPosToGlobal(llfloor(new_center.mV[VX]), llfloor(new_center.mV[VY]));
// [SL:KB] - Patch: World-MinimapOverlay | Checked: 2012-06-20 (Catznip-3.3.0)
			mObjectImageCenterGlobal = posCenterGlobal;
// [/SL:KB]

			// Create the base texture.
			U8 *default_texture = mObjectRawImagep->getData();
			memset( default_texture, 0, mObjectImagep->getWidth() * mObjectImagep->getHeight() * mObjectImagep->getComponents() );

			// Draw objects
			gObjectList.renderObjectsForMap(*this);

			mObjectImagep->setSubImage(mObjectRawImagep, 0, 0, mObjectImagep->getWidth(), mObjectImagep->getHeight());
			
			map_timer.reset();
		}

// [SL:KB] - Patch: World-MinimapOverlay | Checked: 2012-06-20 (Catznip-3.3.0)
		static LLCachedControl<bool> s_fShowPropertyLines(gSavedSettings, "MiniMapPropertyLines") ;
		if ( (s_fShowPropertyLines) && ((mUpdateParcelImage) || (dist_vec_squared2D(mParcelImageCenterGlobal, posCenterGlobal) > 9.0f)) )
		{
			mUpdateParcelImage = false;
			mParcelImageCenterGlobal = posCenterGlobal;

			U8* pTextureData = mParcelRawImagep->getData();
			memset(pTextureData, 0, mParcelImagep->getWidth() * mParcelImagep->getHeight() * mParcelImagep->getComponents());

			// Process each region
			for (LLWorld::region_list_t::const_iterator itRegion = LLWorld::getInstance()->getRegionList().begin();
					itRegion != LLWorld::getInstance()->getRegionList().end(); ++itRegion)
			{
				const LLViewerRegion* pRegion = *itRegion; LLColor4U clrOverlay;
				if (pRegion->isAlive())
					clrOverlay = map_property_line.get();
				else
					clrOverlay = LLColor4U(255, 128, 128, 255);
				renderPropertyLinesForRegion(pRegion, clrOverlay);
			}

			mParcelImagep->setSubImage(mParcelRawImagep, 0, 0, mParcelImagep->getWidth(), mParcelImagep->getHeight());
		}
// [/SL:KB]

		LLVector3 map_center_agent = gAgent.getPosAgentFromGlobal(mObjectImageCenterGlobal);
		LLVector3 camera_position = gAgentCamera.getCameraPositionAgent();
		map_center_agent -= camera_position;
		map_center_agent.mV[VX] *= mScale/region_width;
		map_center_agent.mV[VY] *= mScale/region_width;

//		gGL.getTexUnit(0)->bind(mObjectImagep);
		F32 image_half_width = 0.5f*mObjectMapPixels;
		F32 image_half_height = 0.5f*mObjectMapPixels;

// [SL:KB] - Patch: World-MinimapOverlay | Checked: 2012-07-26 (Catznip-3.3)
		if (s_fShowObjects)
		{
			gGL.getTexUnit(0)->bind(mObjectImagep);
// [/SL:KB]
			gGL.begin(LLRender::QUADS);
				gGL.texCoord2f(0.f, 1.f);
				gGL.vertex2f(map_center_agent.mV[VX] - image_half_width, image_half_height + map_center_agent.mV[VY]);
				gGL.texCoord2f(0.f, 0.f);
				gGL.vertex2f(map_center_agent.mV[VX] - image_half_width, map_center_agent.mV[VY] - image_half_height);
				gGL.texCoord2f(1.f, 0.f);
				gGL.vertex2f(image_half_width + map_center_agent.mV[VX], map_center_agent.mV[VY] - image_half_height);
				gGL.texCoord2f(1.f, 1.f);
				gGL.vertex2f(image_half_width + map_center_agent.mV[VX], image_half_height + map_center_agent.mV[VY]);
			gGL.end();
// [SL:KB] - Patch: World-MinimapOverlay | Checked: 2012-07-26 (Catznip-3.3)
		}
// [/SL:KB]

// [SL:KB] - Patch: World-MinimapOverlay | Checked: 2012-06-20 (Catznip-3.3.0)
		if (s_fShowPropertyLines)
		{
			map_center_agent = gAgent.getPosAgentFromGlobal(mParcelImageCenterGlobal) - camera_position;
			map_center_agent.mV[VX] *= mScale / region_width;
			map_center_agent.mV[VY] *= mScale / region_width;

			gGL.getTexUnit(0)->bind(mParcelImagep);
			gGL.begin(LLRender::QUADS);
				gGL.texCoord2f(0.f, 1.f);
				gGL.vertex2f(map_center_agent.mV[VX] - image_half_width, image_half_height + map_center_agent.mV[VY]);
				gGL.texCoord2f(0.f, 0.f);
				gGL.vertex2f(map_center_agent.mV[VX] - image_half_width, map_center_agent.mV[VY] - image_half_height);
				gGL.texCoord2f(1.f, 0.f);
				gGL.vertex2f(image_half_width + map_center_agent.mV[VX], map_center_agent.mV[VY] - image_half_height);
				gGL.texCoord2f(1.f, 1.f);
				gGL.vertex2f(image_half_width + map_center_agent.mV[VX], image_half_height + map_center_agent.mV[VY]);
			gGL.end();
		}
// [/SL:KB]

		gGL.popMatrix();

		// Mouse pointer in local coordinates
		S32 local_mouse_x;
		S32 local_mouse_y;
		//localMouse(&local_mouse_x, &local_mouse_y);
		LLUI::getMousePositionLocal(this, &local_mouse_x, &local_mouse_y);
// [SL:KB] - Patch: World-MiniMap | Checked: 2012-07-08 (Catznip-3.3.0)
		bool local_mouse = this->pointInView(local_mouse_x, local_mouse_y);
// [/SL:KB]
		mClosestAgentToCursor.setNull();
// [SL:KB] - Patch: World-MiniMap | Checked: 2012-07-08 (Catznip-3.3.0)
		mClosestAgentsToCursor.clear();
// [/SL:KB]
		F32 closest_dist_squared = F32_MAX; // value will be overridden in the loop
		F32 min_pick_dist_squared = (mDotRadius * MIN_PICK_SCALE) * (mDotRadius * MIN_PICK_SCALE);

		LLVector3 pos_map;
		uuid_vec_t avatar_ids;
		std::vector<LLVector3d> positions;
		bool unknown_relative_z;

		LLWorld::getInstance()->getAvatars(&avatar_ids, &positions, gAgentCamera.getCameraPositionGlobal());

		// Draw avatars
		for (U32 i = 0; i < avatar_ids.size(); i++)
		{
			pos_map = globalPosToView(positions[i]);
			LLUUID uuid = avatar_ids[i];

// [RLVa:KB] - Checked: 2010-04-19 (RLVa-1.2.0f) | Modified: RLVa-1.2.0f
			bool show_as_friend = (LLAvatarTracker::instance().getBuddyInfo(uuid) != NULL) &&
				(!gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES));
// [/RLVa:KB]
//			bool show_as_friend = (LLAvatarTracker::instance().getBuddyInfo(uuid) != NULL);

			LLColor4 color = show_as_friend ? map_avatar_friend_color : map_avatar_color;

			// <FS:Ansariel> Check for unknown Z-offset => AVATAR_UNKNOWN_Z_OFFSET
			//unknown_relative_z = positions[i].mdV[VZ] == COARSEUPDATE_MAX_Z &&
			//		camera_position.mV[VZ] >= COARSEUPDATE_MAX_Z;
			unknown_relative_z = false;
			if (positions[i].mdV[VZ] == AVATAR_UNKNOWN_Z_OFFSET)
			{
				if (camera_position.mV[VZ] >= COARSEUPDATE_MAX_Z)
				{
					// No exact data and cam >=1020 => we don't know if
					// other avatar is above or below us => unknown
					unknown_relative_z = true;
				}
				else
				{
					// No exact data but cam is below 1020 => other avatar
					// is definitely above us => bump Z-offset to F32_MAX
					// so we get the up chevron
					pos_map.mV[VZ] = F32_MAX;
				}
			}
			// </FS:Ansariel>

			// Colorize muted avatars and Lindens
			std::string fullName;
			LLMuteList* muteListInstance = LLMuteList::getInstance();

			if (muteListInstance->isMuted(uuid)) color = map_avatar_muted_color;
			else if (gCacheName->getFullName(uuid, fullName) && muteListInstance->isLinden(fullName)) color = map_avatar_linden_color;			

			// <FS:Ansariel> Mark Avatars with special colors
			if (LLNetMap::sAvatarMarksMap.find(uuid) != LLNetMap::sAvatarMarksMap.end())
			{
				color = LLNetMap::sAvatarMarksMap[uuid];
			}
			// </FS:Ansariel> Mark Avatars with special colors
					
			//color based on contact sets prefs
			if(LGGContactSets::getInstance()->hasFriendColorThatShouldShow(uuid,FALSE,FALSE,FALSE,TRUE))
			{
				color = LGGContactSets::getInstance()->getFriendColor(uuid);
			}

// [RLVa:KB] - Checked: 2010-04-19 (RLVa-1.2.0f) | Modified: RLVa-1.2.0f | FS-Specific
			LLWorldMapView::drawAvatar(
				pos_map.mV[VX], pos_map.mV[VY],
				((!gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES)) ? color : map_avatar_color.get()),
				pos_map.mV[VZ], mDotRadius,
				unknown_relative_z);
// [/RLVa:KB]
//			LLWorldMapView::drawAvatar(
//				pos_map.mV[VX], pos_map.mV[VY], 
//				color, 
//				pos_map.mV[VZ], mDotRadius,
//				unknown_relative_z);

			if(uuid.notNull())
			{
				bool selected = false;
				uuid_vec_t::iterator sel_iter = gmSelected.begin();
				for (; sel_iter != gmSelected.end(); sel_iter++)
				{
					if(*sel_iter == uuid)
					{
						selected = true;
						break;
					}
				}
				if(selected)
				{
					if( (pos_map.mV[VX] < 0) ||
						(pos_map.mV[VY] < 0) ||
						(pos_map.mV[VX] >= getRect().getWidth()) ||
						(pos_map.mV[VY] >= getRect().getHeight()) )
					{
						S32 x = llround( pos_map.mV[VX] );
						S32 y = llround( pos_map.mV[VY] );
						LLWorldMapView::drawTrackingCircle( getRect(), x, y, color, 1, 10);
					} else
					{
						LLWorldMapView::drawTrackingDot(pos_map.mV[VX],pos_map.mV[VY],color,0.f);
					}
				}
			}

// [SL:KB] - Patch: World-MiniMap | Checked: 2012-07-08 (Catznip-3.3.0)
			if (local_mouse)
			{
// [/SL:KB]
				F32 dist_to_cursor_squared = dist_vec_squared(LLVector2(pos_map.mV[VX], pos_map.mV[VY]), 
												LLVector2(local_mouse_x,local_mouse_y));
				if (dist_to_cursor_squared < min_pick_dist_squared)
				{
					if (dist_to_cursor_squared < closest_dist_squared)
					{
						closest_dist_squared = dist_to_cursor_squared;
						mClosestAgentToCursor = uuid;
						mClosestAgentPosition = positions[i];
					}
					mClosestAgentsToCursor.push_back(uuid);
				}
// [SL:KB] - Patch: World-MiniMap | Checked: 2012-07-08 (Catznip-3.3.0)
			}
// [/SL:KB]
//			F32	dist_to_cursor_squared = dist_vec_squared(LLVector2(pos_map.mV[VX], pos_map.mV[VY]),
//											LLVector2(local_mouse_x,local_mouse_y));
//			if(dist_to_cursor_squared < min_pick_dist_squared && dist_to_cursor_squared < closest_dist_squared)
//			{
//				closest_dist_squared = dist_to_cursor_squared;
//				mClosestAgentToCursor = uuid;
//			}
		}

		// Draw dot for autopilot target
		if (gAgent.getAutoPilot())
		{
			drawTracking( gAgent.getAutoPilotTargetGlobal(), map_track_color );
		}
		else
		{
			LLTracker::ETrackingStatus tracking_status = LLTracker::getTrackingStatus();
			if (  LLTracker::TRACKING_AVATAR == tracking_status )
			{
				drawTracking( LLAvatarTracker::instance().getGlobalPos(), map_track_color );
			} 
			else if ( LLTracker::TRACKING_LANDMARK == tracking_status 
					|| LLTracker::TRACKING_LOCATION == tracking_status )
			{
				drawTracking( LLTracker::getTrackedPositionGlobal(), map_track_color );
			}
		}

		// Draw dot for self avatar position
		LLVector3d pos_global = gAgent.getPositionGlobal();
		pos_map = globalPosToView(pos_global);
		S32 dot_width = llround(mDotRadius * 2.f);
		LLUIImagePtr you = LLWorldMapView::sAvatarYouLargeImage;
		if (you)
		{
			you->draw(llround(pos_map.mV[VX] - mDotRadius),
					  llround(pos_map.mV[VY] - mDotRadius),
					  dot_width,
					  dot_width);

			F32	dist_to_cursor_squared = dist_vec_squared(LLVector2(pos_map.mV[VX], pos_map.mV[VY]),
										  LLVector2(local_mouse_x,local_mouse_y));
			if(dist_to_cursor_squared < min_pick_dist_squared && dist_to_cursor_squared < closest_dist_squared)
			{
				mClosestAgentToCursor = gAgent.getID();
				mClosestAgentPosition = pos_global;
			}

			// Draw chat range ring(s)
			static LLUICachedControl<bool> chat_ring("MiniMapChatRing", true);
			if(chat_ring)
			{
// <FS:CR> Aurora Sim
				//drawRing(20.0, pos_map, map_chat_ring_color);
				//drawRing(100.0, pos_map, map_shout_ring_color);
				drawRing(LLWorld::getInstance()->getSayDistance(), pos_map, map_chat_ring_color);
				drawRing(LLWorld::getInstance()->getShoutDistance(), pos_map, map_shout_ring_color);
// </FS:CR> Aurora Sim
			}
		}

		// Draw frustum
// <FS:CR> Aurora Sim
		//F32 meters_to_pixels = mScale/ LLWorld::getInstance()->getRegionWidthInMeters();
		F32 meters_to_pixels = mScale/ REGION_WIDTH_METERS;
// </FS:CR> Aurora Sim

		F32 horiz_fov = LLViewerCamera::getInstance()->getView() * LLViewerCamera::getInstance()->getAspect();
		F32 far_clip_meters = LLViewerCamera::getInstance()->getFar();
		F32 far_clip_pixels = far_clip_meters * meters_to_pixels;

		F32 half_width_meters = far_clip_meters * tan( horiz_fov / 2 );
		F32 half_width_pixels = half_width_meters * meters_to_pixels;
		
		F32 ctr_x = (F32)center_sw_left;
		F32 ctr_y = (F32)center_sw_bottom;


		gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);

		if( rotate_map )
		{
			gGL.color4fv((map_frustum_color()).mV);

			gGL.begin( LLRender::TRIANGLES  );
				gGL.vertex2f( ctr_x, ctr_y );
				gGL.vertex2f( ctr_x - half_width_pixels, ctr_y + far_clip_pixels );
				gGL.vertex2f( ctr_x + half_width_pixels, ctr_y + far_clip_pixels );
			gGL.end();
		}
		else
		{
			gGL.color4fv((map_frustum_rotating_color()).mV);
			
			// If we don't rotate the map, we have to rotate the frustum.
			gGL.pushMatrix();
				gGL.translatef( ctr_x, ctr_y, 0 );
				gGL.rotatef( atan2( LLViewerCamera::getInstance()->getAtAxis().mV[VX], LLViewerCamera::getInstance()->getAtAxis().mV[VY] ) * RAD_TO_DEG, 0.f, 0.f, -1.f);
				gGL.begin( LLRender::TRIANGLES  );
					gGL.vertex2f( 0, 0 );
					gGL.vertex2f( -half_width_pixels, far_clip_pixels );
					gGL.vertex2f(  half_width_pixels, far_clip_pixels );
				gGL.end();
			gGL.popMatrix();
		}
	}
	
	gGL.popMatrix();
	gGL.popUIMatrix();

	LLUICtrl::draw();
}

void LLNetMap::reshape(S32 width, S32 height, BOOL called_from_parent)
{
	LLUICtrl::reshape(width, height, called_from_parent);
	createObjectImage();
// [SL:KB] - Patch: World-MinimapOverlay | Checked: 2012-07-28 (Catznip-3.3)
	createParcelImage();
// [/SL:KB]
}

LLVector3 LLNetMap::globalPosToView(const LLVector3d& global_pos)
{
	LLVector3d camera_position = gAgentCamera.getCameraPositionGlobal();

	LLVector3d relative_pos_global = global_pos - camera_position;
	LLVector3 pos_local;
	pos_local.setVec(relative_pos_global);  // convert to floats from doubles

// <FS:CR> Aurora Sim
	mPixelsPerMeter = mScale / REGION_WIDTH_METERS;
// </FS:CR> Aurora Sim
	pos_local.mV[VX] *= mPixelsPerMeter;
	pos_local.mV[VY] *= mPixelsPerMeter;
	// leave Z component in meters

	static LLUICachedControl<bool> rotate_map("MiniMapRotate", true);
	if( rotate_map )
	{
		F32 radians = atan2( LLViewerCamera::getInstance()->getAtAxis().mV[VX], LLViewerCamera::getInstance()->getAtAxis().mV[VY] );
		LLQuaternion rot(radians, LLVector3(0.f, 0.f, 1.f));
		pos_local.rotVec( rot );
	}

	pos_local.mV[VX] += getRect().getWidth() / 2 + mCurPan.mV[VX];
	pos_local.mV[VY] += getRect().getHeight() / 2 + mCurPan.mV[VY];

	return pos_local;
}

void LLNetMap::drawRing(const F32 radius, const LLVector3 pos_map, const LLUIColor& color)

{
// <FS:CR> Aurora Sim
	//F32 meters_to_pixels = mScale / LLWorld::getInstance()->getRegionWidthInMeters();
	F32 meters_to_pixels = mScale / REGION_WIDTH_METERS;
// </FS:CR> Aurora Sim
	F32 radius_pixels = radius * meters_to_pixels;

	glMatrixMode(GL_MODELVIEW);
	gGL.pushMatrix();
	gGL.translatef((F32)pos_map.mV[VX], (F32)pos_map.mV[VY], 0.f);
	gl_ring(radius_pixels, WIDTH_PIXELS, color, color, CIRCLE_STEPS, FALSE);
	gGL.popMatrix();
}

void LLNetMap::drawTracking(const LLVector3d& pos_global, const LLColor4& color, 
							BOOL draw_arrow )
{
	LLVector3 pos_local = globalPosToView(pos_global);
	if( (pos_local.mV[VX] < 0) ||
		(pos_local.mV[VY] < 0) ||
		(pos_local.mV[VX] >= getRect().getWidth()) ||
		(pos_local.mV[VY] >= getRect().getHeight()) )
	{
		if (draw_arrow)
		{
			S32 x = llround( pos_local.mV[VX] );
			S32 y = llround( pos_local.mV[VY] );
			LLWorldMapView::drawTrackingCircle( getRect(), x, y, color, 1, 10 );
			LLWorldMapView::drawTrackingArrow( getRect(), x, y, color );
		}
	}
	else
	{
		LLWorldMapView::drawTrackingDot(pos_local.mV[VX], 
										pos_local.mV[VY], 
										color,
										pos_local.mV[VZ]);
	}
}

LLVector3d LLNetMap::viewPosToGlobal( S32 x, S32 y )
{
	x -= llround(getRect().getWidth() / 2 + mCurPan.mV[VX]);
	y -= llround(getRect().getHeight() / 2 + mCurPan.mV[VY]);

	LLVector3 pos_local( (F32)x, (F32)y, 0 );

	F32 radians = - atan2( LLViewerCamera::getInstance()->getAtAxis().mV[VX], LLViewerCamera::getInstance()->getAtAxis().mV[VY] );

	static LLUICachedControl<bool> rotate_map("MiniMapRotate", true);
	if( rotate_map )
	{
		LLQuaternion rot(radians, LLVector3(0.f, 0.f, 1.f));
		pos_local.rotVec( rot );
	}

// <FS:CR> Aurora Sim
	//pos_local *= ( LLWorld::getInstance()->getRegionWidthInMeters() / mScale );
	pos_local *= ( REGION_WIDTH_METERS / mScale );
// </FS:CR> Aurora Sim
	
	LLVector3d pos_global;
	pos_global.setVec( pos_local );
	pos_global += gAgentCamera.getCameraPositionGlobal();

	return pos_global;
}

BOOL LLNetMap::handleScrollWheel(S32 x, S32 y, S32 clicks)
{
	// note that clicks are reversed from what you'd think: i.e. > 0  means zoom out, < 0 means zoom in
	F32 new_scale = mScale * pow(MAP_SCALE_ZOOM_FACTOR, -clicks);
	F32 old_scale = mScale;

	setScale(new_scale);

	static LLUICachedControl<bool> auto_center("MiniMapAutoCenter", true);
	if (!auto_center)
	{
		// Adjust pan to center the zoom on the mouse pointer
		LLVector2 zoom_offset;
		zoom_offset.mV[VX] = x - getRect().getWidth() / 2;
		zoom_offset.mV[VY] = y - getRect().getHeight() / 2;
		mCurPan -= zoom_offset * mScale / old_scale - zoom_offset;
	}

	return TRUE;
}

BOOL LLNetMap::handleToolTip( S32 x, S32 y, MASK mask )
{
	if (gDisconnected)
	{
		return FALSE;
	}

	// If the cursor is near an avatar on the minimap, a mini-inspector will be
	// shown for the avatar, instead of the normal map tooltip.
//	if (handleToolTipAgent(mClosestAgentToCursor))
// [RLVa:KB] - Checked: 2010-10-31 (RLVa-1.2.2a) | Modified: RLVa-1.2.2a
	if ( (!gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES)) && (handleToolTipAgent(mClosestAgentToCursor)) )
// [/RLVa:KB]
	{
		return TRUE;
	}

// [RLVa:KB] - Checked: 2010-10-31 (RLVa-1.2.2a) | Modified: RLVa-1.2.2a
	LLStringUtil::format_map_t args;

	LLAvatarName avName;
	if ( (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES)) && 
		 (mClosestAgentToCursor.notNull()) && (LLAvatarNameCache::get(mClosestAgentToCursor, &avName)) )
	{
		args["[AGENT]"] = RlvStrings::getAnonym(avName) + "\n";
	}
	else
	{
		args["[AGENT]"] = "";
	}
// [/RLVa:KB]

	LLRect sticky_rect;
	std::string region_name;
	LLViewerRegion*	region = LLWorld::getInstance()->getRegionFromPosGlobal( viewPosToGlobal( x, y ) );
	if(region)
	{
		// set sticky_rect
		S32 SLOP = 4;
		localPointToScreen(x - SLOP, y - SLOP, &(sticky_rect.mLeft), &(sticky_rect.mBottom));
		sticky_rect.mRight = sticky_rect.mLeft + 2 * SLOP;
		sticky_rect.mTop = sticky_rect.mBottom + 2 * SLOP;

//		region_name = region->getName();
// [RLVa:KB] - Checked: 2010-10-19 (RLVa-1.2.2b) | Modified: RLVa-1.2.2b
		region_name = ((!gRlvHandler.hasBehaviour(RLV_BHVR_SHOWLOC)) ? region->getName() : RlvStrings::getString(RLV_STRING_HIDDEN_REGION));
// [/RLVa:KB]
		// <FS:Ansariel> Synchronize tooltips throughout instances
		//if (!region_name.empty())
		if (!region_name.empty() && LLNetMap::sToolTipMsg != "[REGION]")
		{
			region_name += "\n";
		}
	}

//	LLStringUtil::format_map_t args;
	args["[REGION]"] = region_name;
	// <FS:Ansariel> Synchronize tooltips throughout instances
	//std::string msg = mToolTipMsg;
	std::string msg = LLNetMap::sToolTipMsg;
	// </FS:Ansariel> Synchronize tooltips throughout instances
	LLStringUtil::format(msg, args);
	LLToolTipMgr::instance().show(LLToolTip::Params()
		.message(msg)
		.sticky_rect(sticky_rect));
		
	return TRUE;
}

BOOL LLNetMap::handleToolTipAgent(const LLUUID& avatar_id)
{
	LLAvatarName av_name;
	if (avatar_id.isNull() || !LLAvatarNameCache::get(avatar_id, &av_name))
	{
		return FALSE;
	}

	// only show tooltip if same inspector not already open
	LLFloater* existing_inspector = LLFloaterReg::findInstance("inspect_avatar");
	if (!existing_inspector
		|| !existing_inspector->getVisible()
		|| existing_inspector->getKey()["avatar_id"].asUUID() != avatar_id)
	{
		LLInspector::Params p;
		p.fillFrom(LLUICtrlFactory::instance().getDefaultParams<LLInspector>());
		
		// Add distance to avatars in hovertip for minimap
		if (avatar_id != gAgentID)
		{
			F32 distance(0.f);

			// If avatar is >=1020, the value for Z might be returned as AVATAR_UNKNOWN_Z_OFFSET
			bool isHigher1020mBug = (mClosestAgentPosition[VZ] == AVATAR_UNKNOWN_Z_OFFSET);

			// <FS:Ansariel> Try to get distance from the nearby people panel
			//               aka radar when above 1020m.
			if (isHigher1020mBug)
			{
				LLPanelPeople* panel_people = getPeoplePanel();
				if (panel_people)
				{
					LLAvatarListItem* avatar_list_item = panel_people->getNearbyList()->getAvatarListItem(avatar_id);
					if (avatar_list_item)
					{
						F32 radar_distance = avatar_list_item->getRange();

						if (radar_distance > AVATAR_UNKNOWN_RANGE)
						{
							distance = radar_distance;
							isHigher1020mBug = false;
						}
					}
				}
			}
			else
			{
				distance = dist_vec(gAgent.getPositionGlobal(), mClosestAgentPosition);
			}

			LLStringUtil::format_map_t args;

			if (!isHigher1020mBug)
			{
				args["DISTANCE"] = llformat("%.02f", distance);
			}
			else
			{
				static LLCachedControl<F32> farClip(gSavedSettings, "RenderFarClip");
				args["DISTANCE"] = llformat("> %.02f", F32(farClip));
			}
			std::string distanceLabel = LLTrans::getString("minimap_distance");
			LLStringUtil::format(distanceLabel, args);
			p.message(av_name.getCompleteName() + "\n" + distanceLabel);
		}
		else
		{
			p.message(av_name.getCompleteName());
		}
		
		// <FS:Ansariel> Get rid of the useless and clumsy I-button on the hovertip
		//p.image.name("Inspector_I");
		p.click_callback(boost::bind(showAvatarInspector, avatar_id));
		p.visible_time_near(6.f);
		p.visible_time_far(3.f);
		p.delay_time(0.35f);
		p.wrap(false);

		LLToolTipMgr::instance().show(p);
	}
	return TRUE;
}

// static
void LLNetMap::showAvatarInspector(const LLUUID& avatar_id)
{
	LLSD params;
	params["avatar_id"] = avatar_id;

	if (LLToolTipMgr::instance().toolTipVisible())
	{
		LLRect rect = LLToolTipMgr::instance().getToolTipRect();
		params["pos"]["x"] = rect.mLeft;
		params["pos"]["y"] = rect.mTop;
	}

	LLFloaterReg::showInstance("inspect_avatar", params);
}

void LLNetMap::renderScaledPointGlobal( const LLVector3d& pos, const LLColor4U &color, F32 radius_meters )
{
	LLVector3 local_pos;
	local_pos.setVec( pos - mObjectImageCenterGlobal );

	S32 diameter_pixels = llround(2 * radius_meters * mObjectMapTPM);
	renderPoint( local_pos, color, diameter_pixels );
}


void LLNetMap::renderPoint(const LLVector3 &pos_local, const LLColor4U &color, 
						   S32 diameter, S32 relative_height)
{
	if (diameter <= 0)
	{
		return;
	}

	const S32 image_width = (S32)mObjectImagep->getWidth();
	const S32 image_height = (S32)mObjectImagep->getHeight();

	S32 x_offset = llround(pos_local.mV[VX] * mObjectMapTPM + image_width / 2);
	S32 y_offset = llround(pos_local.mV[VY] * mObjectMapTPM + image_height / 2);

	if ((x_offset < 0) || (x_offset >= image_width))
	{
		return;
	}
	if ((y_offset < 0) || (y_offset >= image_height))
	{
		return;
	}

	U8 *datap = mObjectRawImagep->getData();

	S32 neg_radius = diameter / 2;
	S32 pos_radius = diameter - neg_radius;
	S32 x, y;

	if (relative_height > 0)
	{
		// ...point above agent
		S32 px, py;

		// vertical line
		px = x_offset;
		for (y = -neg_radius; y < pos_radius; y++)
		{
			py = y_offset + y;
			if ((py < 0) || (py >= image_height))
			{
				continue;
			}
			S32 offset = px + py * image_width;
			((U32*)datap)[offset] = color.asRGBA();
		}

		// top line
		py = y_offset + pos_radius - 1;
		for (x = -neg_radius; x < pos_radius; x++)
		{
			px = x_offset + x;
			if ((px < 0) || (px >= image_width))
			{
				continue;
			}
			S32 offset = px + py * image_width;
			((U32*)datap)[offset] = color.asRGBA();
		}
	}
	else
	{
		// ...point level with agent
		for (x = -neg_radius; x < pos_radius; x++)
		{
			S32 p_x = x_offset + x;
			if ((p_x < 0) || (p_x >= image_width))
			{
				continue;
			}

			for (y = -neg_radius; y < pos_radius; y++)
			{
				S32 p_y = y_offset + y;
				if ((p_y < 0) || (p_y >= image_height))
				{
					continue;
				}
				S32 offset = p_x + p_y * image_width;
				((U32*)datap)[offset] = color.asRGBA();
			}
		}
	}
}

// [SL:KB] - Patch: World-MinimapOverlay | Checked: 2012-06-20 (Catznip-3.3.0)
void LLNetMap::renderPropertyLinesForRegion(const LLViewerRegion* pRegion, const LLColor4U& clrOverlay)
{
	const S32 imgWidth = (S32)mParcelImagep->getWidth();
	const S32 imgHeight = (S32)mParcelImagep->getHeight();

	const LLVector3 originLocal(pRegion->getOriginGlobal() - mParcelImageCenterGlobal);
	const S32 originX = llround(originLocal.mV[VX] * mObjectMapTPM + imgWidth / 2);
	const S32 originY = llround(originLocal.mV[VY] * mObjectMapTPM + imgHeight / 2);

	U32* pTextureData = (U32*)mParcelRawImagep->getData();

	//
	// Draw the north and east region borders
	//
	const S32 borderY = originY + llround(REGION_WIDTH_METERS * mObjectMapTPM);
	if ( (borderY >= 0) && (borderY < imgHeight) )
	{
		S32 curX = llclamp(originX, 0, imgWidth), endX = llclamp(originX + llround(REGION_WIDTH_METERS * mObjectMapTPM), 0, imgWidth - 1);
		for (; curX <= endX; curX++)
			pTextureData[borderY * imgWidth + curX] = clrOverlay.asRGBA();
	}
	const S32 borderX = originX + llround(REGION_WIDTH_METERS * mObjectMapTPM);
	if ( (borderX >= 0) && (borderX < imgWidth) )
	{
		S32 curY = llclamp(originY, 0, imgHeight), endY = llclamp(originY + llround(REGION_WIDTH_METERS * mObjectMapTPM), 0, imgHeight - 1);
		for (; curY <= endY; curY++)
			pTextureData[curY * imgWidth + borderX] = clrOverlay.asRGBA();
	}

	//
	// Render parcel lines
	//
	static const F32 GRID_STEP = PARCEL_GRID_STEP_METERS;
	static const S32 GRIDS_PER_EDGE = REGION_WIDTH_METERS / GRID_STEP;

	const U8* pOwnership = pRegion->getParcelOverlay()->getOwnership();
	const U8* pCollision = (pRegion->getHandle() == LLViewerParcelMgr::instance().getCollisionRegionHandle()) ? LLViewerParcelMgr::instance().getCollisionBitmap() : NULL;
	for (S32 idxRow = 0; idxRow < GRIDS_PER_EDGE; idxRow++)
	{
		for (S32 idxCol = 0; idxCol < GRIDS_PER_EDGE; idxCol++)
		{
			S32 overlay = pOwnership[idxRow * GRIDS_PER_EDGE + idxCol];
			S32 idxCollision = idxRow * GRIDS_PER_EDGE + idxCol;
			bool fForSale = ((overlay & PARCEL_COLOR_MASK) == PARCEL_FOR_SALE);
			bool fCollision = (pCollision) && (pCollision[idxCollision / 8] & (1 << (idxCollision % 8)));
			if ( (!fForSale) && (!fCollision) && (0 == (overlay & (PARCEL_SOUTH_LINE | PARCEL_WEST_LINE))) )
				continue;

			const S32 posX = originX + llround(idxCol * GRID_STEP * mObjectMapTPM);
			const S32 posY = originY + llround(idxRow * GRID_STEP * mObjectMapTPM);

			static LLCachedControl<bool> s_fForSaleParcels(gSavedSettings, "MiniMapForSaleParcels");
			static LLCachedControl<bool> s_fShowCollisionParcels(gSavedSettings, "MiniMapCollisionParcels");
			if ( ((s_fForSaleParcels) && (fForSale)) || ((s_fShowCollisionParcels) && (fCollision)) )
			{
				S32 curY = llclamp(posY, 0, imgHeight), endY = llclamp(posY + llround(GRID_STEP * mObjectMapTPM), 0, imgHeight - 1);
				for (; curY <= endY; curY++)
				{
					S32 curX = llclamp(posX, 0, imgWidth) , endX = llclamp(posX + llround(GRID_STEP * mObjectMapTPM), 0, imgWidth - 1);
					for (; curX <= endX; curX++)
					{
						pTextureData[curY * imgWidth + curX] = (fForSale) ? LLColor4U(255, 255, 128, 192).asRGBA()
						                                                  : LLColor4U(255, 128, 128, 192).asRGBA();
					}
				}
			}
			if (overlay & PARCEL_SOUTH_LINE)
			{
				if ( (posY >= 0) && (posY < imgHeight) )
				{
					S32 curX = llclamp(posX, 0, imgWidth), endX = llclamp(posX + llround(GRID_STEP * mObjectMapTPM), 0, imgWidth - 1);
					for (; curX <= endX; curX++)
						pTextureData[posY * imgWidth + curX] = clrOverlay.asRGBA();
				}
			}
			if (overlay & PARCEL_WEST_LINE)
			{
				if ( (posX >= 0) && (posX < imgWidth) )
				{
					S32 curY = llclamp(posY, 0, imgHeight), endY = llclamp(posY + llround(GRID_STEP * mObjectMapTPM), 0, imgHeight - 1);
					for (; curY <= endY; curY++)
						pTextureData[curY * imgWidth + posX] = clrOverlay.asRGBA();
				}
			}
		}
	}
}
// [/SL:KB]

//void LLNetMap::createObjectImage()
// [SL:KB] - Patch: World-MinimapOverlay | Checked: 2012-06-20 (Catznip-3.3.0)
bool LLNetMap::createImage(LLPointer<LLImageRaw>& rawimagep) const
// [/SL:KB]
{
	// Find the size of the side of a square that surrounds the circle that surrounds getRect().
	// ... which is, the diagonal of the rect.
	F32 width = (F32)getRect().getWidth();
	F32 height = (F32)getRect().getHeight();
	S32 square_size = llround( sqrt(width*width + height*height) );

	// Find the least power of two >= the minimum size.
	const S32 MIN_SIZE = 64;
// [SL:KB] - Patch: World-MinimapOverlay | Checked: 2012-07-28 (Catznip-3.3)
	const S32 MAX_SIZE = 512;
// [/SL:KB]
//	const S32 MAX_SIZE = 256;
	S32 img_size = MIN_SIZE;
	while( (img_size*2 < square_size ) && (img_size < MAX_SIZE) )
	{
		img_size <<= 1;
	}

// [SL:KB] - Patch: World-MinimapOverlay | Checked: 2012-06-20 (Catznip-3.3.0)
	if( rawimagep.isNull() || (rawimagep->getWidth() != img_size) || (rawimagep->getHeight() != img_size) )
	{
		rawimagep = new LLImageRaw(img_size, img_size, 4);
		U8* data = rawimagep->getData();
		memset( data, 0, img_size * img_size * 4 );
		return true;
	}
	return false;
// [/SL:KB]
//	if( mObjectImagep.isNull() ||
//		(mObjectImagep->getWidth() != img_size) ||
//		(mObjectImagep->getHeight() != img_size) )
//	{
//		mObjectRawImagep = new LLImageRaw(img_size, img_size, 4);
//		U8* data = mObjectRawImagep->getData();
//		memset( data, 0, img_size * img_size * 4 );
//		mObjectImagep = LLViewerTextureManager::getLocalTexture( mObjectRawImagep.get(), FALSE);
//	}
//	setScale(mScale);
//	mUpdateNow = true;
}

// [SL:KB] - Patch: World-MinimapOverlay | Checked: 2012-06-20 (Catznip-3.3.0)
void LLNetMap::createObjectImage()
{
	if (createImage(mObjectRawImagep))
		mObjectImagep = LLViewerTextureManager::getLocalTexture( mObjectRawImagep.get(), FALSE);
	// <FS:Ansariel> Synchronize scale throughout instances
	//setScale(mScale);
	setScale(sScale);
	// </FS:Ansariel> Synchronize scale throughout instances
	mUpdateObjectImage = true;
}

void LLNetMap::createParcelImage()
{
	if (createImage(mParcelRawImagep))
		mParcelImagep = LLViewerTextureManager::getLocalTexture( mParcelRawImagep.get(), FALSE);
	mUpdateParcelImage = true;
}
// [/SL:KB]

BOOL LLNetMap::handleMouseDown( S32 x, S32 y, MASK mask )
{
	if (!(mask & MASK_SHIFT)) return FALSE;

	// Start panning
	gFocusMgr.setMouseCapture(this);

	mStartPan = mCurPan;
	mMouseDown.mX = x;
	mMouseDown.mY = y;
	return TRUE;
}

BOOL LLNetMap::handleMouseUp( S32 x, S32 y, MASK mask )
{
	if(abs(mMouseDown.mX-x)<3 && abs(mMouseDown.mY-y)<3)
		handleClick(x,y,mask);

	if (hasMouseCapture())
	{
		if (mPanning)
		{
			// restore mouse cursor
			S32 local_x, local_y;
			local_x = mMouseDown.mX + llfloor(mCurPan.mV[VX] - mStartPan.mV[VX]);
			local_y = mMouseDown.mY + llfloor(mCurPan.mV[VY] - mStartPan.mV[VY]);
			LLRect clip_rect = getRect();
			clip_rect.stretch(-8);
			clip_rect.clipPointToRect(mMouseDown.mX, mMouseDown.mY, local_x, local_y);
			LLUI::setMousePositionLocal(this, local_x, local_y);

			// finish the pan
			mPanning = false;

			mMouseDown.set(0, 0);

			// auto centre
			mTargetPan.setZero();
		}
		gViewerWindow->showCursor();
		gFocusMgr.setMouseCapture(NULL);
		return TRUE;
	}
	return FALSE;
}

// [SL:KB] - Patch: World-MiniMap | Checked: 2012-07-08 (Catznip-3.3.0)
void LLNetMap::setAvatarProfileLabel(const LLAvatarName& avName, const std::string& item_name)
{
	LLMenuItemGL* pItem = mPopupMenu->findChild<LLMenuItemGL>(item_name, TRUE /*recurse*/);
	if (pItem)
	{
		pItem->setLabel(avName.getCompleteName());
		pItem->getMenu()->arrange();
	}
}

void LLNetMap::handleOverlayToggle(const LLSD& sdParam)
{
	// Toggle the setting
	const std::string strControl = sdParam.asString();
	BOOL fCurValue = gSavedSettings.getBOOL(strControl);
	gSavedSettings.setBOOL(strControl, !fCurValue);

	// Force an overlay update
	mUpdateParcelImage = true;
}

void LLNetMap::handleShowProfile(const LLSD& sdParam) const
{
	const std::string strParam = sdParam.asString();
	if ("closest" == strParam)
	{
		LLAvatarActions::showProfile(mClosestAgentRightClick);
	}
	else if ("place" == strParam)
	{
		LLSD sdParams;
		sdParams["type"] = "remote_place";
		sdParams["x"] = mPosGlobalRightClick.mdV[VX];
		sdParams["y"] = mPosGlobalRightClick.mdV[VY];
		sdParams["z"] = mPosGlobalRightClick.mdV[VZ];

		if (gSavedSettings.getBOOL("FSUseStandalonePlaceDetailsFloater"))
		{
			LLFloaterReg::showInstance("fs_placedetails", sdParams);
		}
		else
		{
			LLFloaterSidePanelContainer::showPanel("places", sdParams);
		}
	}
}

bool LLNetMap::checkTextureType(const LLSD& sdParam) const
{
	const std::string strParam = sdParam.asString();

	bool fWorldMapTextures = gSavedSettings.getBOOL("MiniMapWorldMapTextures");
	if ("maptile" == strParam)
		return fWorldMapTextures;
	else if ("terrain" == strParam)
		return !fWorldMapTextures;
	return false;
}

void LLNetMap::handleTextureType(const LLSD& sdParam) const
{
	gSavedSettings.setBOOL("MiniMapWorldMapTextures", ("maptile" == sdParam.asString()));
}
// [/SL:KB]

BOOL LLNetMap::handleRightMouseDown(S32 x, S32 y, MASK mask)
{
	if (mPopupMenu)
	{
// [SL:KB] - Patch: World-MiniMap | Checked: 2012-07-08 (Catznip-3.3.0)
		mClosestAgentRightClick = mClosestAgentToCursor;
		mPosGlobalRightClick = viewPosToGlobal(x, y);

		mPopupMenu->setItemVisible("View Profile", mClosestAgentsToCursor.size() == 1);

		LLMenuItemBranchGL* pProfilesMenu = mPopupMenu->getChild<LLMenuItemBranchGL>("View Profiles");
		if (pProfilesMenu)
		{
			pProfilesMenu->setVisible(mClosestAgentsToCursor.size() > 1);

			pProfilesMenu->getBranch()->empty();
			for (uuid_vec_t::const_iterator itAgent = mClosestAgentsToCursor.begin(); itAgent != mClosestAgentsToCursor.end(); ++itAgent)
			{
				LLMenuItemCallGL::Params p;
				p.name = llformat("Profile Item %d", itAgent - mClosestAgentsToCursor.begin());

				LLAvatarName avName; const LLUUID& idAgent = *itAgent;
				if (LLAvatarNameCache::get(idAgent, &avName))
				{
					p.label = avName.getCompleteName();
				}
				else
				{
					p.label = LLTrans::getString("LoadingData");
					LLAvatarNameCache::get(idAgent, boost::bind(&LLNetMap::setAvatarProfileLabel, this, _2, p.name.getValue()));
				}
				p.on_click.function = boost::bind(&LLAvatarActions::showProfile, _2);
				p.on_click.parameter = idAgent;

				LLMenuItemCallGL* pMenuItem  = LLUICtrlFactory::create<LLMenuItemCallGL>(p);
				if (pMenuItem)
					pProfilesMenu->getBranch()->addChild(pMenuItem);
			}
		}
		F32 range = dist_vec(mClosestAgentPosition, gAgent.getPositionGlobal());
		mPopupMenu->setItemVisible("Cam", (range < gSavedSettings.getF32("RenderFarClip")
										   || gObjectList.findObject(mClosestAgentRightClick) != NULL));
		mPopupMenu->setItemVisible("MarkAvatar", mClosestAgentToCursor.notNull());
		mPopupMenu->setItemVisible("Start Tracking", mClosestAgentToCursor.notNull());
		mPopupMenu->setItemVisible("Profile Separator", (mClosestAgentsToCursor.size() >= 1
								   || mClosestAgentToCursor.notNull()));
// [/SL:KB]
		mPopupMenu->buildDrawLabels();
		mPopupMenu->updateParent(LLMenuGL::sMenuContainer);
// [SL:KB] - Patch: World-MiniMap | Checked: 2012-07-08 (Catznip-3.3.0)
		mPopupMenu->setItemVisible("Stop Tracking", LLTracker::isTracking(0));
		mPopupMenu->setItemVisible("Stop Tracking Separator", LLTracker::isTracking(0));
// [/SL:KB]
//		mPopupMenu->setItemEnabled("Stop Tracking", LLTracker::isTracking(0));
		LLMenuGL::showPopup(this, mPopupMenu, x, y);
	}
	return TRUE;
}

BOOL LLNetMap::handleClick(S32 x, S32 y, MASK mask)
{
	// TODO: allow clicking an avatar on minimap to select avatar in the nearby avatar list
	// if(mClosestAgentToCursor.notNull())
	//     mNearbyList->selectUser(mClosestAgentToCursor);
	// Needs a registered observer i guess to accomplish this without using
	// globals to tell the mNearbyList in llpeoplepanel to select the user
	return TRUE;
}

BOOL LLNetMap::handleDoubleClick(S32 x, S32 y, MASK mask)
{
	LLVector3d pos_global = viewPosToGlobal(x, y);

	// <FS:Ansariel> Synchronize double click handling throughout instances
	//bool double_click_teleport = gSavedSettings.getBOOL("DoubleClickTeleport");
	//bool double_click_show_world_map = gSavedSettings.getBOOL("DoubleClickShowWorldMap");

	//if (double_click_teleport || double_click_show_world_map)
	//{
	//	// If we're not tracking a beacon already, double-click will set one 
	//	if (!LLTracker::isTracking(NULL))
	//	{
	//		LLFloaterWorldMap* world_map = LLFloaterWorldMap::getInstance();
	//		if (world_map)
	//		{
	//			world_map->trackLocation(pos_global);
	//		}
	//	}
	//}

	//if (double_click_teleport)
	//{
	//	// If DoubleClickTeleport is on, double clicking the minimap will teleport there
	//	gAgent.teleportViaLocationLookAt(pos_global);
	//}
	//else if (double_click_show_world_map)
	//{
	//	LLFloaterReg::showInstance("world_map");
	//}
	performDoubleClickAction(pos_global);
	// </FS:Ansariel> Synchronize double click handling throughout instances

	return TRUE;
}

// static
bool LLNetMap::outsideSlop( S32 x, S32 y, S32 start_x, S32 start_y, S32 slop )
{
	S32 dx = x - start_x;
	S32 dy = y - start_y;

	return (dx <= -slop || slop <= dx || dy <= -slop || slop <= dy);
}

BOOL LLNetMap::handleHover( S32 x, S32 y, MASK mask )
{
	if (hasMouseCapture())
	{
		if (mPanning || outsideSlop(x, y, mMouseDown.mX, mMouseDown.mY, MOUSE_DRAG_SLOP))
		{
			if (!mPanning)
			{
				// just started panning, so hide cursor
				mPanning = true;
				gViewerWindow->hideCursor();
			}

			LLVector2 delta(static_cast<F32>(gViewerWindow->getCurrentMouseDX()),
							static_cast<F32>(gViewerWindow->getCurrentMouseDY()));

			// Set pan to value at start of drag + offset
			mCurPan += delta;
			mTargetPan = mCurPan;

			gViewerWindow->moveCursorToCenter();
		}

		// Doesn't really matter, cursor should be hidden
		gViewerWindow->setCursor( UI_CURSOR_TOOLPAN );
	}
	else
	{
		if (mask & MASK_SHIFT)
		{
			// If shift is held, change the cursor to hint that the map can be dragged
			gViewerWindow->setCursor( UI_CURSOR_TOOLPAN );
		}
		else
		{
			gViewerWindow->setCursor( UI_CURSOR_CROSS );
		}
	}

	return TRUE;
}

void LLNetMap::handleZoom(const LLSD& userdata)
{
	std::string level = userdata.asString();
	
	F32 scale = 0.0f;
// [SL:KB] - Patch: World-MinimapZoom | Checked: 2012-08-15 (Catznip-3.3)
	//if (level == "close")
	//	scale = 2048.f;
	//else if (level == "medium")
	//	scale = 512.f;
	//else if (level == "far")
	//	scale = 128.f;
// [/Sl:KB]
	if (level == std::string("default"))
	{
		LLControlVariable *pvar = gSavedSettings.getControl("MiniMapScale");
		if(pvar)
		{
			pvar->resetToDefault();
			scale = gSavedSettings.getF32("MiniMapScale");
		}
	}
	else if (level == std::string("close"))
		scale = LLNetMap::MAP_SCALE_MAX;
	else if (level == std::string("medium"))
		scale = LLNetMap::MAP_SCALE_MID;
	else if (level == std::string("far"))
		scale = LLNetMap::MAP_SCALE_MIN;
	if (scale != 0.0f)
	{
		setScale(scale);
	}
}

void LLNetMap::handleStopTracking (const LLSD& userdata)
{
	if (mPopupMenu)
	{
		mPopupMenu->setItemEnabled ("Stop Tracking", false);
		LLTracker::stopTracking ((void*)(ptrdiff_t)LLTracker::isTracking(NULL));
	}
}

// <FS:Ansariel> additional functions
void LLNetMap::handleMark(const LLSD& userdata)
{
	setAvatarMark(userdata);
}

void LLNetMap::handleClearMarks()
{
	clearAvatarMarks();
}

void LLNetMap::setAvatarMark(const LLSD& userdata)
{
	if (mClosestAgentRightClick.notNull())
	{
		// Use the name as color definition name from colors.xml
		LLColor4 color = LLUIColorTable::instance().getColor(userdata.asString(), LLColor4::green);
		LLNetMap::sAvatarMarksMap[mClosestAgentRightClick] = color;
		llinfos << "Minimap: Marking " << mClosestAgentRightClick.asString() << " in " << userdata.asString() << llendl;
	}
}

void LLNetMap::clearAvatarMarks()
{
	LLNetMap::sAvatarMarksMap.clear();
}
//</FS:Ansariel>

void LLNetMap::camAvatar()
{
	LLAvatarActions::zoomIn(mClosestAgentRightClick);
}

void LLNetMap::handleCam()
{
	camAvatar();
}

// <FS:Ansariel> Avatar tracking feature
void LLNetMap::handleStartTracking()
{
	startTracking();
}

void LLNetMap::startTracking()
{
	if (mClosestAgentRightClick.notNull())
	{
		LLPanelPeople* panel_people = getPeoplePanel();
		if (panel_people != NULL)
		{
			panel_people->startTracking(mClosestAgentRightClick);
		}
	}
}
// </FS:Ansariel> Avatar tracking feature

// <FS:Ansariel> Synchronize tooltips throughout instances
// static
void LLNetMap::updateToolTipMsg()
{
	S32 fsNetMapDoubleClickAction = gSavedSettings.getS32("FSNetMapDoubleClickAction");
	switch (fsNetMapDoubleClickAction)
	{
		case 1:
			LLNetMap::setToolTipMsg(LLTrans::getString("NetMapDoubleClickShowWorldMapToolTipMsg"));
			break;
		case 2:
			LLNetMap::setToolTipMsg(LLTrans::getString("NetMapDoubleClickTeleportToolTipMsg"));
			break;
		default:
			LLNetMap::setToolTipMsg(LLTrans::getString("NetMapDoubleClickNoActionToolTipMsg"));
			break;
	}
}
// </FS:Ansariel> Synchronize tooltips throughout instances

// <FS:Ansariel> Synchronize double click handling throughout instances
void LLNetMap::performDoubleClickAction(LLVector3d pos_global)
{
	S32 fsNetMapDoubleClickAction = gSavedSettings.getS32("FSNetMapDoubleClickAction");
	
	// 1 = Double click show world map
	// 2 = Double click teleport
	if (fsNetMapDoubleClickAction == 1 || fsNetMapDoubleClickAction == 2)
	{
		// If we're not tracking a beacon already, double-click will set one 
		if (!LLTracker::isTracking(NULL))
		{
			LLFloaterWorldMap* world_map = LLFloaterWorldMap::getInstance();
			if (world_map)
			{
				world_map->trackLocation(pos_global);
			}
		}
	}

	switch (fsNetMapDoubleClickAction)
	{
		case 1:
			LLFloaterReg::showInstance("world_map");
			break;
		case 2:
			gAgent.teleportViaLocationLookAt(pos_global);
			break;
		default:
			break;
	}
}
// </FS:Ansariel> Synchronize double click handling throughout instances
