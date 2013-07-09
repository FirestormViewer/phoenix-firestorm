/** 
 * @file llworld.h
 * @brief Collection of viewer regions in the vacinity of the user.
 *
 * Represents the whole world, so far as 3D functionality is conserned.
 * Always contains the region that the user's avatar is in along with
 * neighboring regions. As the user crosses region boundaries, new
 * regions are added to the world and distant ones are rolled up.
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

#ifndef LL_LLWORLD_H
#define LL_LLWORLD_H

#include "llpatchvertexarray.h"

#include "llmath.h"
#include "v3math.h"
#include "llsingleton.h"
#include "llstring.h"
#include "llviewerpartsim.h"
#include "llviewertexture.h"
#include "llvowater.h"

class LLViewerRegion;
class LLVector3d;
class LLMessageSystem;
class LLNetMap;
class LLHost;

class LLViewerObject;
class LLSurfacePatch;

class LLCloudPuff;
class LLCloudGroup;
class LLVOAvatar;

// LLWorld maintains a stack of unused viewer_regions and an array of pointers to viewer regions
// as simulators are connected to, viewer_regions are popped off the stack and connected as required
// as simulators are removed, they are pushed back onto the stack

class LLWorld : public LLSingleton<LLWorld>
{
public:
	LLWorld();
	void destroyClass();

	void refreshLimits();// <AW: opensim-limits>
// <FS:CR> Aurora Sim
	void updateLimits(); // <FS:CR> Aurora Sim
	//LLViewerRegion*	addRegion(const U64 &region_handle, const LLHost &host);
	LLViewerRegion*	addRegion(const U64 &region_handle, const LLHost &host, const U32 &region_size_x, const U32 &region_size_y);
// <FS:CR> Aurora Sim
		// safe to call if already present, does the "right thing" if
		// hosts are same, or if hosts are different, etc...
	void			removeRegion(const LLHost &host);

	void	disconnectRegions(); // Send quit messages to all child regions

	LLViewerRegion*			getRegion(const LLHost &host);
	LLViewerRegion*			getRegionFromPosGlobal(const LLVector3d &pos);
	LLViewerRegion*			getRegionFromPosAgent(const LLVector3 &pos);
	LLViewerRegion*			getRegionFromHandle(const U64 &handle);
	BOOL					positionRegionValidGlobal(const LLVector3d& pos);			// true if position is in valid region
	LLVector3d				clipToVisibleRegions(const LLVector3d &start_pos, const LLVector3d &end_pos);

	void					updateAgentOffset(const LLVector3d &offset);

	// All of these should be in the agent coordinate frame
	LLViewerRegion*			resolveRegionGlobal(LLVector3 &localpos, const LLVector3d &position);
	LLViewerRegion*			resolveRegionAgent(LLVector3 &localpos, const LLVector3 &position);
	F32						resolveLandHeightGlobal(const LLVector3d &position);
	F32						resolveLandHeightAgent(const LLVector3 &position);

	// Return the lowest allowed Z point to prevent objects from being moved
	// underground.
	F32 getMinAllowedZ(LLViewerObject* object, const LLVector3d &global_pos);

	// takes a line segment defined by point_a and point_b, then
	// determines the closest (to point_a) point of intersection that is
	// on the land surface or on an object of the world.
	// Stores results in "intersection" and "intersection_normal" and
	// returns a scalar value that is the normalized (by length of line segment) 
	// distance along the line from "point_a" to "intersection".
	//
	// Currently assumes point_a and point_b only differ in z-direction,
	// but it may eventually become more general.
	F32 resolveStepHeightGlobal(const LLVOAvatar* avatarp, const LLVector3d &point_a, const LLVector3d &point_b,
							LLVector3d &intersection, LLVector3 &intersection_normal,
							LLViewerObject** viewerObjectPtr=NULL);

	LLSurfacePatch *		resolveLandPatchGlobal(const LLVector3d &position);
	LLVector3				resolveLandNormalGlobal(const LLVector3d &position);		// absolute frame

	U32						getRegionWidthInPoints() const	{ return mWidth; }
	F32						getRegionScale() const			{ return mScale; }

	// region X and Y size in meters
	F32						getRegionWidthInMeters() const	{ return mWidthInMeters; }
	F32						getRegionMinHeight() const		{ return -mWidthInMeters; }
// <AW: opensim-limits>
//	F32						getRegionMaxHeight() const		{ return MAX_OBJECT_Z; }
	F32 getRegionMaxHeight() const		{ return mRegionMaxHeight; }
	F32 getRegionMinPrimScale() const	{ return mRegionMinPrimScale; }
	F32 getRegionMaxPrimScale() const	{ return mRegionMaxPrimScale; }
	F32 getRegionMaxPrimScaleNoMesh() const	{ return mRegionMaxPrimScaleNoMesh; }
	F32 getRegionMaxHollowSize() const	{ return mRegionMaxHollowSize; }
	F32 getRegionMinHoleSize() const	{ return mRegionMinHoleSize; }
// <FS:CR> Aurora Sim
	int getMaxLinkedPrims() const         { return mMaxLinkedPrims; }
	int getMaxPhysLinkedPrims() const     { return mMaxPhysLinkedPrims; }
	int getMaxInventoryItemsTransfer() const { return mMaxInventoryItemsTransfer; }
	int getAllowRenderName() const           { return mAllowRenderName; }
	bool getAllowMinimap() const             { return mAllowMinimap; }
	bool getAllowPhysicalPrims() const       { return mAllowPhysicalPrims; }
	bool getAllowRenderWater() const         { return mAllowRenderWater; }

	F32 getMaxPrimXPos() const			{ return mMaxPrimXPos; }
	F32 getMaxPrimYPos() const			{ return mMaxPrimYPos; }
	F32 getMaxPrimZPos() const			{ return mMaxPrimZPos; }
	F32 getMinPrimXPos() const			{ return mMinPrimXPos; }
	F32 getMinPrimYPos() const			{ return mMinPrimYPos; }
	F32 getMinPrimZPos() const			{ return mMinPrimZPos; }
	F32 getMaxDragDistance() const		{ return mMaxDragDistance; }
	F32 getMaxPhysPrimScale() const		{ return mMaxPhysPrimScale; }
	BOOL getSkyUseClassicClouds() const	{ return mClassicCloudsEnabled; }
	BOOL getAllowParcelWindLight() const{ return mAllowParcelWindLight; }
	BOOL getEnableTeenMode() const		{ return mEnableTeenMode; }
	BOOL getEnforceMaxBuild() const		{ return mEnforceMaxBuild; }
	BOOL getLockedDrawDistance() const	{ return mLockedDrawDistance; }

	F32 getWhisperDistance() const		{ return mWhisperDistance; }
	F32 getSayDistance() const			{ return mSayDistance; }
	F32 getShoutDistance() const		{ return mShoutDistance; }

	F32 getDrawDistance() const			{ return mDrawDistance; }
	F32 getTerrainDetailScale() const	{ return mTerrainDetailScale; }

	//setters
	void setRegionMaxHeight(F32 val);
	void setRegionMinPrimScale(F32 val);
	void setRegionMaxPrimScale(F32 val);
	void setRegionMaxPrimScaleNoMesh(F32 val);
	void setRegionMaxHollowSize(F32 val);
	void setRegionMinHoleSize(F32 val);
	
	void setMaxLinkedPrims(S32 val);
	void setMaxPhysLinkedPrims(S32 val);
	void setMaxInventoryItemsTransfer(S32 val);
	void setAllowRenderName(S32 val);
	void setAllowMinimap(BOOL val);
	void setAllowPhysicalPrims(BOOL val);
	void setAllowRenderWater(BOOL val);

	void setMaxPrimXPos(F32 val);
	void setMaxPrimYPos(F32 val);
	void setMaxPrimZPos(F32 val);
	void setMinPrimXPos(F32 val);
	void setMinPrimYPos(F32 val);
	void setMinPrimZPos(F32 val);
	void setMaxDragDistance(F32 val);
	void setMaxPhysPrimScale(F32 val);
	void setSkyUseClassicClouds(BOOL val);
	void setAllowParcelWindLight(BOOL val);
	void setEnableTeenMode(BOOL val);
	void setEnforceMaxBuild(BOOL val);
	void setLockedDrawDistance(BOOL val);
	
	void setWhisperDistance(F32 val);
	void setSayDistance(F32 val);
	void setShoutDistance(F32 val);

	void setDrawDistance(F32 val);
	void setTerrainDetailScale(F32 val);
// <FS:CR> Aurora Sim
// </AW: opensim-limits>
	void					updateRegions(F32 max_update_time);
	void					updateVisibilities();
	void					updateParticles();
	void					updateClouds(const F32 dt);
	LLCloudGroup *			findCloudGroup(const LLCloudPuff &puff);

	void					renderPropertyLines();

	void resetStats();
	void updateNetStats(); // Update network statistics for all the regions...

	void printPacketsLost();
	void requestCacheMisses();

	// deal with map object updates in the world.
	static void processCoarseUpdate(LLMessageSystem* msg, void** user_data);

	F32 getLandFarClip() const;
	void setLandFarClip(const F32 far_clip);

	LLViewerTexture *getDefaultWaterTexture();
	void updateWaterObjects();
	void waterHeightRegionInfo(std::string const& sim_name, F32 water_height);
	void shiftRegions(const LLVector3& offset);

	void setSpaceTimeUSec(const U64 space_time_usec);
	U64 getSpaceTimeUSec() const;

	void getInfo(LLSD& info);

public:
	typedef std::list<LLViewerRegion*> region_list_t;
	const region_list_t& getRegionList() const { return mActiveRegionList; }

	// Returns lists of avatar IDs and their world-space positions within a given distance of a point.
	// All arguments are optional. Given containers will be emptied and then filled.
	// Not supplying origin or radius input returns data on all avatars in the known regions.
	void getAvatars(
		uuid_vec_t* avatar_ids = NULL,
		std::vector<LLVector3d>* positions = NULL, 
		const LLVector3d& relative_to = LLVector3d(), F32 radius = FLT_MAX) const;

	// Returns 'true' if the region is in mRegionList,
	// 'false' if the region has been removed due to region change
	// or if the circuit to this simulator had been lost.
	bool isRegionListed(const LLViewerRegion* region) const;

private:
	region_list_t	mActiveRegionList;
	region_list_t	mRegionList;
	region_list_t	mVisibleRegionList;
	region_list_t	mCulledRegionList;

	// Number of points on edge
// <FS:CR> Aurora Sim
	//static const U32 mWidth;
	static U32 mWidth;
// </FS:CR> Aurora Sim

	// meters/point, therefore mWidth * mScale = meters per edge
	static const F32 mScale;

// <FS:CR> Aurora Sim
	//static const F32 mWidthInMeters;
	static F32 mWidthInMeters;
// </FS:CR> Aurora Sim
// <AW: opensim-limits>
	F32 mRegionMaxHeight;
	F32 mRegionMinPrimScale;
	F32 mRegionMaxPrimScale;
	F32 mRegionMaxPrimScaleNoMesh;
	F32 mRegionMaxHollowSize;
	F32 mRegionMinHoleSize;
// <FS:CR> Aurora Sim
	S32 mMaxLinkedPrims;
	S32 mMaxPhysLinkedPrims;
	S32 mMaxInventoryItemsTransfer;
	S32 mAllowRenderName;
	BOOL mAllowMinimap;
	BOOL mAllowPhysicalPrims;
	BOOL mAllowRenderWater;

	F32		mMaxPrimXPos;
	F32		mMaxPrimYPos;
	F32		mMaxPrimZPos;
	F32		mMinPrimXPos;
	F32		mMinPrimYPos;
	F32		mMinPrimZPos;
	F32     mMaxDragDistance;
	F32		mMaxPhysPrimScale;
	BOOL    mAllowParcelWindLight;
	BOOL    mEnableTeenMode;
	BOOL    mEnforceMaxBuild;
	BOOL	mLockedDrawDistance;

	F32 mWhisperDistance;
	F32 mSayDistance;
	F32 mShoutDistance;

	F32 mDrawDistance;
	F32 mTerrainDetailScale;
// <FS:CR> Aurora Sim
	bool mLimitsNeedRefresh;
// </AW: opensim-limits>
	F32 mLandFarClip;					// Far clip distance for land.
	LLPatchVertexArray		mLandPatch;
	S32 mLastPacketsIn;
	S32 mLastPacketsOut;
	S32 mLastPacketsLost;

	U64 mSpaceTimeUSec;

	BOOL mClassicCloudsEnabled;

	////////////////////////////
	//
	// Data for "Fake" objects
	//

	std::list<LLVOWater*> mHoleWaterObjects;
	LLPointer<LLVOWater> mEdgeWaterObjects[8];

	LLPointer<LLViewerTexture> mDefaultWaterTexturep;
};


void process_enable_simulator(LLMessageSystem *mesgsys, void **user_data);
void process_disable_simulator(LLMessageSystem *mesgsys, void **user_data);

void process_region_handshake(LLMessageSystem* msg, void** user_data);

void send_agent_pause();
void send_agent_resume();

#endif
