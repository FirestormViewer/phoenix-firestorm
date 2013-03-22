/** 
 * @file llfloatermap.cpp
 * @brief The "mini-map" or radar in the upper right part of the screen.
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

#include "llviewerprecompiledheaders.h"

// self include
#include "llfloatermap.h"

// Library includes
#include "llfloaterreg.h"
#include "llfontgl.h"
#include "llglheaders.h"

// Viewer includes
#include "llagentcamera.h"
#include "llviewercontrol.h"
#include "llnetmap.h"
#include "lltracker.h"
#include "llviewercamera.h"
#include "lldraghandle.h"
#include "lltextbox.h"
#include "llfloaterworldmap.h"
#include "llagent.h"

// Firestorm includes
#include "rlvhandler.h"
#include "llavataractions.h"
#include "llavatarnamecache.h"
#include "llfloatersidepanelcontainer.h"
#include "llviewerobject.h"
#include "llviewerparceloverlay.h"
#include "llviewerparcelmgr.h"
#include "llviewermenu.h"
#include "llworld.h"	// <FS:CR> Aurora Sim

//
// Constants
//

// The minor cardinal direction labels are hidden if their height is more
// than this proportion of the map.
const F32 MAP_MINOR_DIR_THRESHOLD = 0.07f;
const S32 MAP_PADDING_LEFT = 0;
const S32 MAP_PADDING_TOP = 2;
const S32 MAP_PADDING_RIGHT = 2;
const S32 MAP_PADDING_BOTTOM = 0;
//
// Member functions
//

LLFloaterMap::LLFloaterMap(const LLSD& key) 
	: LLFloater(key),
	  mTextBoxEast(NULL),
	  mTextBoxNorth(NULL),
	  mTextBoxWest(NULL),
	  mTextBoxSouth(NULL),
	  mTextBoxSouthEast(NULL),
	  mTextBoxNorthEast(NULL),
	  mTextBoxNorthWest(NULL),
	  mTextBoxSouthWest(NULL),
	  mPopupMenu(NULL), // <FS:CR>
	  mMap(NULL)
{
}

LLFloaterMap::~LLFloaterMap()
{
}

BOOL LLFloaterMap::postBuild()
{
	mMap = getChild<LLNetMap>("Net Map");
	// <FS:Ansariel> Synchronize tooltips throughout instances
	//if (gSavedSettings.getBOOL("DoubleClickTeleport"))
	//{
	//	mMap->setToolTipMsg(getString("AltToolTipMsg"));
	//}
	//else if (gSavedSettings.getBOOL("DoubleClickShowWorldMap"))
	//{
	//	mMap->setToolTipMsg(getString("ToolTipMsg"));
	//}
	// </FS:Ansariel> Synchronize tooltips throughout instances
	sendChildToBack(mMap);
	
	mTextBoxNorth = getChild<LLTextBox> ("floater_map_north");
	mTextBoxEast = getChild<LLTextBox> ("floater_map_east");
	mTextBoxWest = getChild<LLTextBox> ("floater_map_west");
	mTextBoxSouth = getChild<LLTextBox> ("floater_map_south");
	mTextBoxSouthEast = getChild<LLTextBox> ("floater_map_southeast");
	mTextBoxNorthEast = getChild<LLTextBox> ("floater_map_northeast");
	mTextBoxSouthWest = getChild<LLTextBox> ("floater_map_southwest");
	mTextBoxNorthWest = getChild<LLTextBox> ("floater_map_northwest");

	// <Firestorm Minimap changes>
	LLUICtrl::CommitCallbackRegistry::ScopedRegistrar registrar;
	
	registrar.add("Minimap.Zoom", boost::bind(&LLFloaterMap::handleZoom, this, _2));
	registrar.add("Minimap.Tracker", boost::bind(&LLFloaterMap::handleStopTracking, this, _2));

	registrar.add("Minimap.Mark", boost::bind(&LLFloaterMap::handleMark, this, _2));
	registrar.add("Minimap.ClearMarks", boost::bind(&LLFloaterMap::handleClearMarks, this));

	registrar.add("Minimap.Cam", boost::bind(&LLFloaterMap::handleCam, this));
	registrar.add("Minimap.StartTracking", boost::bind(&LLFloaterMap::handleStartTracking, this));
	
	// [SL:KB] - Patch: World-MiniMap | Checked: 2012-07-08 (Catznip-3.3.0)
	registrar.add("Minimap.ShowProfile", boost::bind(&LLFloaterMap::handleShowProfile, this, _2));
	registrar.add("Minimap.TextureType", boost::bind(&LLFloaterMap::handleTextureType, this, _2));
	registrar.add("Minimap.ToggleOverlay", boost::bind(&LLFloaterMap::handleOverlayToggle, this, _2));
	
	LLUICtrl::EnableCallbackRegistry::ScopedRegistrar enable_registrar;
	enable_registrar.add("Minimap.CheckTextureType", boost::bind(&LLFloaterMap::checkTextureType, this, _2));
	// [/SL:KB]
	
	// [SL:KB] - Patch: World-MinimapOverlay | Checked: 2012-06-20 (Catznip-3.3.0)
	LLViewerParcelMgr::instance().setCollisionUpdateCallback(boost::bind(&LLFloaterMap::refreshParcelOverlay, this));
	LLViewerParcelOverlay::setUpdateCallback(boost::bind(&LLFloaterMap::refreshParcelOverlay, this));
	// [/SL:KB]

	mPopupMenu = LLUICtrlFactory::getInstance()->createFromFile<LLMenuGL>("menu_mini_map.xml", gMenuHolder, LLViewerMenuHolderGL::child_registry_t::instance());
	if (mPopupMenu && !LLTracker::isTracking(0))
	{
		mPopupMenu->setItemEnabled ("Stop Tracking", false);
	}
	// </Firestorm minimap changes>

	stretchMiniMap(getRect().getWidth() - MAP_PADDING_LEFT - MAP_PADDING_RIGHT,
		getRect().getHeight() - MAP_PADDING_TOP - MAP_PADDING_BOTTOM);

	updateMinorDirections();

	// Get the drag handle all the way in back
	sendChildToBack(getDragHandle());

	setIsChrome(TRUE);
	getDragHandle()->setTitleVisible(TRUE);
	
	// keep onscreen
	gFloaterView->adjustToFitScreen(this, FALSE);

	return TRUE;
}

BOOL LLFloaterMap::handleDoubleClick(S32 x, S32 y, MASK mask)
{
	// If floater is minimized, minimap should be shown on doubleclick (STORM-299)
	if (isMinimized())
	{
		setMinimized(FALSE);
		return TRUE;
	}

	LLVector3d pos_global = mMap->viewPosToGlobal(x, y);
	
	// <FS:Ansariel> Synchronize double click handling throughout instances
	//LLTracker::stopTracking(NULL);
	//LLFloaterWorldMap* world_map = LLFloaterWorldMap::getInstance();
	//if (world_map)
	//{
	//	world_map->trackLocation(pos_global);
	//}
	//
	//if (gSavedSettings.getBOOL("DoubleClickTeleport"))
	//{
	//	// If DoubleClickTeleport is on, double clicking the minimap will teleport there
	//	gAgent.teleportViaLocationLookAt(pos_global);
	//}
	//else if (gSavedSettings.getBOOL("DoubleClickShowWorldMap"))
	//{
	//	LLFloaterReg::showInstance("world_map");
	//}
	mMap->performDoubleClickAction(pos_global);
	// </FS:Ansariel> Synchronize double click handling throughout instances
    return TRUE;
}

BOOL LLFloaterMap::handleRightMouseDown(S32 x, S32 y, MASK mask)
{
	if (mPopupMenu)
	{
		// [SL:KB] - Patch: World-MiniMap | Checked: 2012-07-08 (Catznip-3.3.0)
		mMap->mPosGlobalRightClick = mMap->viewPosToGlobal(x, y);
		
		mPopupMenu->setItemVisible("View Profile", mMap->mClosestAgentsToCursor.size() == 1);
		
		LLMenuItemBranchGL* pProfilesMenu = mPopupMenu->getChild<LLMenuItemBranchGL>("View Profiles");
		if (pProfilesMenu)
		{
			pProfilesMenu->setVisible(mMap->mClosestAgentsToCursor.size() > 1);
			
			pProfilesMenu->getBranch()->empty();
			for (uuid_vec_t::const_iterator itAgent = mMap->mClosestAgentsToCursor.begin(); itAgent != mMap->mClosestAgentsToCursor.end(); ++itAgent)
			{
				LLMenuItemCallGL::Params p;
				p.name = llformat("Profile Item %d", itAgent - mMap->mClosestAgentsToCursor.begin());
				
				LLAvatarName avName; const LLUUID& idAgent = *itAgent;
				if (LLAvatarNameCache::get(idAgent, &avName))
				{
					p.label = avName.getCompleteName();
				}
				else
				{
					p.label = LLTrans::getString("LoadingData");
					LLAvatarNameCache::get(idAgent, boost::bind(&LLFloaterMap::setAvatarProfileLabel, this, _2, p.name.getValue()));
				}
				p.on_click.function = boost::bind(&LLAvatarActions::showProfile, _2);
				p.on_click.parameter = idAgent;
				
				LLMenuItemCallGL* pMenuItem  = LLUICtrlFactory::create<LLMenuItemCallGL>(p);
				if (pMenuItem)
					pProfilesMenu->getBranch()->addChild(pMenuItem);
			}
		}
		F32 range = dist_vec(mMap->getClosestAgentPosition(), gAgent.getPositionGlobal());
		mPopupMenu->setItemVisible("Cam", (range < gSavedSettings.getF32("RenderFarClip")
										   || gObjectList.findObject(mMap->getClosestAgentRightClick()) != NULL));
		mPopupMenu->setItemVisible("MarkAvatar", mMap->getClosestAgentToCursor().notNull());
		mPopupMenu->setItemVisible("Start Tracking", mMap->getClosestAgentToCursor().notNull());
		mPopupMenu->setItemVisible("Profile Separator", (mMap->mClosestAgentsToCursor.size() >= 1
														 || mMap->getClosestAgentToCursor().notNull()));
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

void LLFloaterMap::setDirectionPos( LLTextBox* text_box, F32 rotation )
{
	// Rotation is in radians.
	// Rotation of 0 means x = 1, y = 0 on the unit circle.

	F32 map_half_height = (F32)(getRect().getHeight() / 2) - getHeaderHeight()/2;
	F32 map_half_width = (F32)(getRect().getWidth() / 2) ;
	F32 text_half_height = (F32)(text_box->getRect().getHeight() / 2);
	F32 text_half_width = (F32)(text_box->getRect().getWidth() / 2);
	F32 radius = llmin( map_half_height - text_half_height, map_half_width - text_half_width );

	// Inset by a little to account for position display.
	radius -= 8.f;

	text_box->setOrigin( 
		llround(map_half_width - text_half_width + radius * cos( rotation )),
		llround(map_half_height - text_half_height + radius * sin( rotation )) );
}

void LLFloaterMap::updateMinorDirections()
{
	if (mTextBoxNorthEast == NULL)
	{
		return;
	}

	// Hide minor directions if they cover too much of the map
	bool show_minors = mTextBoxNorthEast->getRect().getHeight() < MAP_MINOR_DIR_THRESHOLD *
		llmin(getRect().getWidth(), getRect().getHeight());

	mTextBoxNorthEast->setVisible(show_minors);
	mTextBoxNorthWest->setVisible(show_minors);
	mTextBoxSouthWest->setVisible(show_minors);
	mTextBoxSouthEast->setVisible(show_minors);
}

// virtual
void LLFloaterMap::draw()
{
	F32 rotation = 0;

	static LLUICachedControl<bool> rotate_map("MiniMapRotate", true);
	if( rotate_map )
	{
		// rotate subsequent draws to agent rotation
		rotation = atan2( LLViewerCamera::getInstance()->getAtAxis().mV[VX], LLViewerCamera::getInstance()->getAtAxis().mV[VY] );
	}

	setDirectionPos( mTextBoxEast,  rotation );
	setDirectionPos( mTextBoxNorth, rotation + F_PI_BY_TWO );
	setDirectionPos( mTextBoxWest,  rotation + F_PI );
	setDirectionPos( mTextBoxSouth, rotation + F_PI + F_PI_BY_TWO );

	setDirectionPos( mTextBoxNorthEast, rotation +						F_PI_BY_TWO / 2);
	setDirectionPos( mTextBoxNorthWest, rotation + F_PI_BY_TWO +		F_PI_BY_TWO / 2);
	setDirectionPos( mTextBoxSouthWest, rotation + F_PI +				F_PI_BY_TWO / 2);
	setDirectionPos( mTextBoxSouthEast, rotation + F_PI + F_PI_BY_TWO + F_PI_BY_TWO / 2);

	// Note: we can't just gAgent.check cameraMouselook() because the transition states are wrong.
	if(gAgentCamera.cameraMouselook())
	{
		setMouseOpaque(FALSE);
		getDragHandle()->setMouseOpaque(FALSE);
	}
	else
	{
		setMouseOpaque(TRUE);
		getDragHandle()->setMouseOpaque(TRUE);
	}

	if (LLTracker::isTracking(0))
	{
		mPopupMenu->setItemEnabled("Stop Tracking", true);
	}
	
// <FS:CR> Aurora Sim
	//LLFloater::draw();
	if(LLWorld::getInstance()->getAllowMinimap())
	{
		LLFloater::draw();
	}
// <FS:CR> Aurora Sim
}

void LLFloaterMap::stretchMiniMap(S32 width,S32 height)
{
	//fix for ext-7112
	//by default ctrl can't overlap caption area
	if(mMap)
	{
		LLRect map_rect;
		map_rect.setLeftTopAndSize( MAP_PADDING_LEFT, getRect().getHeight() - MAP_PADDING_TOP, width, height);
		mMap->reshape( width, height, 1);
		mMap->setRect(map_rect);
	}
}

void LLFloaterMap::reshape(S32 width, S32 height, BOOL called_from_parent)
{
	LLFloater::reshape(width, height, called_from_parent);
	
	stretchMiniMap(width - MAP_PADDING_LEFT - MAP_PADDING_RIGHT,
		height - MAP_PADDING_TOP - MAP_PADDING_BOTTOM);

	updateMinorDirections();
}

void LLFloaterMap::handleZoom(const LLSD& userdata)
{
	std::string level = userdata.asString();
	
	F32 scale = 0.0f;

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
		mMap->setScale(scale);
	}
}

LLFloaterMap* LLFloaterMap::getInstance()
{
	return LLFloaterReg::getTypedInstance<LLFloaterMap>("mini_map");
}

void	LLFloaterMap::setMinimized(BOOL b)
{
	LLFloater::setMinimized(b);
	if(b)
	{
		setTitle(getString("mini_map_caption"));
	}
	else
	{
		setTitle("");
	}
}

void LLFloaterMap::handleMark(const LLSD& userdata)
{
	mMap->setAvatarMark(userdata);
}

void LLFloaterMap::handleClearMarks()
{
	mMap->clearAvatarMarks();
}

void LLFloaterMap::handleCam()
{
	mMap->camAvatar();
}

void LLFloaterMap::handleStopTracking (const LLSD& userdata)
{
	if (mPopupMenu)
	{
		mPopupMenu->setItemEnabled ("Stop Tracking", false);
		LLTracker::stopTracking ((void*)(ptrdiff_t)LLTracker::isTracking(NULL));
	}
}

// <FS:Ansariel> Avatar tracking feature
void LLFloaterMap::handleStartTracking()
{
	mMap->startTracking();
}
// </FS:Ansariel> Avatar tracking feature

// <FS:CR> FIXME: Cut and paste duplicate code from llnetmap.cpp
void LLFloaterMap::setAvatarProfileLabel(const LLAvatarName& avName, const std::string& item_name)
{
	LLMenuItemGL* pItem = mPopupMenu->findChild<LLMenuItemGL>(item_name, TRUE /*recurse*/);
	if (pItem)
	{
		pItem->setLabel(avName.getCompleteName());
		pItem->getMenu()->arrange();
	}
}

void LLFloaterMap::handleOverlayToggle(const LLSD& sdParam)
{
	// Toggle the setting
	const std::string strControl = sdParam.asString();
	BOOL fCurValue = gSavedSettings.getBOOL(strControl);
	gSavedSettings.setBOOL(strControl, !fCurValue);
	
	// Force an overlay update
	refreshParcelOverlay();
}

void LLFloaterMap::handleShowProfile(const LLSD& sdParam) const
{
	const std::string strParam = sdParam.asString();
	if ("closest" == strParam)
	{
		LLAvatarActions::showProfile(mMap->getClosestAgentRightClick());
	}
	else if ("place" == strParam)
	{
		LLSD sdParams;
		sdParams["type"] = "remote_place";
		sdParams["x"] = mMap->mPosGlobalRightClick.mdV[VX];
		sdParams["y"] = mMap->mPosGlobalRightClick.mdV[VY];
		sdParams["z"] = mMap->mPosGlobalRightClick.mdV[VZ];
		
		LLFloaterSidePanelContainer::showPanel("places", sdParams);
	}
}

bool LLFloaterMap::checkTextureType(const LLSD& sdParam) const
{
	const std::string strParam = sdParam.asString();
	
	bool fWorldMapTextures = gSavedSettings.getBOOL("MiniMapWorldMapTextures");
	if ("maptile" == strParam)
		return fWorldMapTextures;
	else if ("terrain" == strParam)
		return !fWorldMapTextures;
	return false;
}

void LLFloaterMap::handleTextureType(const LLSD& sdParam) const
{
	gSavedSettings.setBOOL("MiniMapWorldMapTextures", ("maptile" == sdParam.asString()));
}

void LLFloaterMap::refreshParcelOverlay()
{
	mMap->mUpdateParcelImage = true;
}
