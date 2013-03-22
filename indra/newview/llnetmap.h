/** 
 * @file llnetmap.h
 * @brief A little map of the world with network information
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

#ifndef LL_LLNETMAP_H
#define LL_LLNETMAP_H

#include "llmath.h"
#include "lluictrl.h"
#include "v3math.h"
#include "v3dmath.h"
#include "v4color.h"
#include "llpointer.h"
#include "llcoord.h"

class LLColor4U;
class LLImageRaw;
class LLViewerTexture;
class LLFloaterMap;
class LLMenuGL;
// [SL:KB] - Patch: World-MinimapOverlay | Checked: 2012-06-20 (Catznip-3.3.0)
class LLViewerRegion;
class LLAvatarName;
// [/SL:KB]

class LLNetMap : public LLUICtrl
{
public:
	struct Params 
	:	public LLInitParam::Block<Params, LLUICtrl::Params>
	{
		Optional<LLUIColor>	bg_color;

		Params()
		:	bg_color("bg_color") 
		{}
	};

protected:
	LLNetMap (const Params & p);
	friend class LLUICtrlFactory;
	friend class LLFloaterMap;

public:
	virtual ~LLNetMap();

	static const F32 MAP_SCALE_MIN;
	static const F32 MAP_SCALE_MID;
	static const F32 MAP_SCALE_MAX;

	/*virtual*/ void	draw();
	/*virtual*/ BOOL	handleScrollWheel(S32 x, S32 y, S32 clicks);
	/*virtual*/ BOOL	handleMouseDown(S32 x, S32 y, MASK mask);
	/*virtual*/ BOOL	handleMouseUp(S32 x, S32 y, MASK mask);
	/*virtual*/ BOOL	handleHover( S32 x, S32 y, MASK mask );
	/*virtual*/ BOOL	handleToolTip( S32 x, S32 y, MASK mask);
	/*virtual*/ void	reshape(S32 width, S32 height, BOOL called_from_parent = TRUE);

	/*virtual*/ BOOL 	postBuild();
	/*virtual*/ BOOL	handleRightMouseDown( S32 x, S32 y, MASK mask );
	/*virtual*/ BOOL	handleClick(S32 x, S32 y, MASK mask);
	/*virtual*/ BOOL	handleDoubleClick( S32 x, S32 y, MASK mask );

// [SL:KB] - Patch: World-MinimapOverlay | Checked: 2012-06-20 (Catznip-3.3.0)
	void			refreshParcelOverlay() { mUpdateParcelImage = true; }
// [/SL:KB]
	void			setScale( F32 scale );
	// <FS:Ansariel> Synchronize tooltips throughout instances
	//void			setToolTipMsg(const std::string& msg) { mToolTipMsg = msg; }
	static void		setToolTipMsg(const std::string& msg) { sToolTipMsg = msg; }
	static void		updateToolTipMsg();
	// </FS:Ansariel> Synchronize tooltips throughout instances
	void			renderScaledPointGlobal( const LLVector3d& pos, const LLColor4U &color, F32 radius );
	LLVector3d		viewPosToGlobal(S32 x,S32 y);
	LLUUID			getClosestAgentToCursor() const { return mClosestAgentToCursor; }
	LLVector3d		getClosestAgentPosition() const { return mClosestAgentPosition; }
	bool			isZoomable();

	// <FS:Ansariel> Synchronize double click handling throughout instances
	void			performDoubleClickAction(LLVector3d pos_global);

private:
	const LLVector3d& getObjectImageCenterGlobal()	{ return mObjectImageCenterGlobal; }
	void 			renderPoint(const LLVector3 &pos, const LLColor4U &color, 
								S32 diameter, S32 relative_height = 0);

	LLVector3		globalPosToView(const LLVector3d& global_pos);

	void			drawTracking( const LLVector3d& pos_global, 
								  const LLColor4& color,
								  BOOL draw_arrow = TRUE);
	void			drawRing(const F32 radius, LLVector3 pos_map, const LLUIColor& color);
	BOOL			handleToolTipAgent(const LLUUID& avatar_id);
	static void		showAvatarInspector(const LLUUID& avatar_id);

// [SL:KB] - Patch: World-MinimapOverlay | Checked: 2012-06-20 (Catznip-3.3.0)
	bool			createImage(LLPointer<LLImageRaw>& rawimagep) const;
	void			createObjectImage();
	void			createParcelImage();

	void			renderPropertyLinesForRegion(const LLViewerRegion* pRegion, const LLColor4U& clrOverlay);
// [/SL:KB]
//	void			createObjectImage();

	static bool		outsideSlop(S32 x, S32 y, S32 start_x, S32 start_y, S32 slop);

//	bool			mUpdateNow;
// [SL:KB] - Patch: World-MinimapOverlay | Checked: 2012-06-20 (Catznip-3.3.0)
	bool			mUpdateObjectImage;
	bool			mUpdateParcelImage;
// [/SL:KB]

	LLUIColor		mBackgroundColor;

	F32				mScale;					// Size of a region in pixels
	static F32			sScale;					// <FS:Ansariel> Used to synchronize netmaps throughout instances

	F32				mPixelsPerMeter;		// world meters to map pixels
	F32				mObjectMapTPM;			// texels per meter on map
	F32				mObjectMapPixels;		// Width of object map in pixels
	F32				mDotRadius;				// Size of avatar markers

	bool			mPanning;			// map is being dragged
	LLVector2		mTargetPan;
	LLVector2		mCurPan;
	LLVector2		mStartPan;		// pan offset at start of drag
	LLCoordGL		mMouseDown;			// pointer position at start of drag

	LLVector3d		mObjectImageCenterGlobal;
	LLPointer<LLImageRaw> mObjectRawImagep;
	LLPointer<LLViewerTexture>	mObjectImagep;
// [SL:KB] - Patch: World-MinimapOverlay | Checked: 2012-06-20 (Catznip-3.3.0)
	LLVector3d		mParcelImageCenterGlobal;
	LLPointer<LLImageRaw> mParcelRawImagep;
	LLPointer<LLViewerTexture>	mParcelImagep;
// [/SL:KB]

	LLUUID			mClosestAgentToCursor;
// [SL:KB] - Patch: World-MiniMap | Checked: 2012-07-08 (Catznip-3.3.0)
	LLVector3d		mClosestAgentPosition;
// [/SL:KB]
	// <FS:Ansariel> Synchronize tooltips throughout instances
	//std::string		mToolTipMsg;
	static std::string	sToolTipMsg;
	// </FS:Ansariel> Synchronize tooltips throughout instances

	static std::map<LLUUID, LLColor4> sAvatarMarksMap;

public:
	void			setSelected(uuid_vec_t uuids) { gmSelected=uuids; };
	void			setAvatarMark(const LLSD& userdata);
	void			clearAvatarMarks();
	void			camAvatar();
// <FS:CR> Minimap improvements
	void			handleShowProfile(const LLSD& sdParam) const;
	uuid_vec_t		mClosestAgentsToCursor;
	LLVector3d		mPosGlobalRightClick;
	LLUUID			mClosestAgentRightClick;
// </FS:CR>
	void			startTracking();

private:
	void handleZoom(const LLSD& userdata);
	void handleStopTracking(const LLSD& userdata);
	void handleMark(const LLSD& userdata);
	void handleClearMarks();
	void handleCam();
	void handleStartTracking();
// [SL:KB] - Patch: World-MiniMap | Checked: 2012-07-08 (Catznip-3.3.0)
	void handleOverlayToggle(const LLSD& sdParam);
	bool checkTextureType(const LLSD& sdParam) const;
	void handleTextureType(const LLSD& sdParam) const;
	void setAvatarProfileLabel(const LLAvatarName& avName, const std::string& item_name);
// [/SL:KB]

	LLMenuGL*		mPopupMenu;
	uuid_vec_t		gmSelected;
};


#endif
