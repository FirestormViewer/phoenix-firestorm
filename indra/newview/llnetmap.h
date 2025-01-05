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
// [SL:KB] - Patch: World-MinimapOverlay | Checked: 2012-06-20 (Catznip-3.3)
class LLViewerRegion;
class LLAvatarName;
// [/SL:KB]

class LLNetMap : public LLUICtrl
{
public:
    struct Params
    :   public LLInitParam::Block<Params, LLUICtrl::Params>
    {
        Optional<LLUIColor> bg_color;

        Params()
        :   bg_color("bg_color")
        {}
    };

protected:
    LLNetMap (const Params & p);
    friend class LLUICtrlFactory;
    friend class LLFloaterMap;

public:
    virtual ~LLNetMap();

    static const F32 MAP_SCALE_MIN;
    static const F32 MAP_SCALE_FAR;
    static const F32 MAP_SCALE_MEDIUM;
    static const F32 MAP_SCALE_CLOSE;
    static const F32 MAP_SCALE_VERY_CLOSE;
    static const F32 MAP_SCALE_MAX;

    /*virtual*/ void    draw();
    /*virtual*/ bool    handleScrollWheel(S32 x, S32 y, S32 clicks);
    /*virtual*/ bool    handleMouseDown(S32 x, S32 y, MASK mask);
    /*virtual*/ bool    handleMouseUp(S32 x, S32 y, MASK mask);
    /*virtual*/ bool    handleHover( S32 x, S32 y, MASK mask );
    /*virtual*/ bool    handleToolTip( S32 x, S32 y, MASK mask);
    /*virtual*/ void    reshape(S32 width, S32 height, bool called_from_parent = true);

    /*virtual*/ bool    postBuild();
    /*virtual*/ bool    handleRightMouseDown( S32 x, S32 y, MASK mask );
    /*virtual*/ bool    handleClick(S32 x, S32 y, MASK mask);
    /*virtual*/ bool    handleDoubleClick( S32 x, S32 y, MASK mask );

// [SL:KB] - Patch: World-MinimapOverlay | Checked: 2012-06-20 (Catznip-3.3)
    void            refreshParcelOverlay() { mUpdateParcelImage = true; }
// [/SL:KB]
    void            setScale(F32 scale);

    void            setToolTipMsg(const std::string& msg) { mToolTipMsg = msg; }
    void            setParcelNameMsg(const std::string& msg) { mParcelNameMsg = msg; }
    void            setParcelSalePriceMsg(const std::string& msg) { mParcelSalePriceMsg = msg; }
    void            setParcelSaleAreaMsg(const std::string& msg) { mParcelSaleAreaMsg = msg; }
    void            setParcelOwnerMsg(const std::string& msg) { mParcelOwnerMsg = msg; }
    void            setRegionNameMsg(const std::string& msg) { mRegionNameMsg = msg; }
    void            setToolTipHintMsg(const std::string& msg) { mToolTipHintMsg = msg; }
    void            setAltToolTipHintMsg(const std::string& msg) { mAltToolTipHintMsg = msg; }

    void            renderScaledPointGlobal( const LLVector3d& pos, const LLColor4U &color, F32 radius );
    LLVector3d      viewPosToGlobal(S32 x,S32 y);
    LLUUID          getClosestAgentToCursor() const { return mClosestAgentToCursor; }
    LLVector3d      getClosestAgentPosition() const { return mClosestAgentPosition; }

    // <FS:Ansariel> Synchronize double click handling throughout instances
    void            performDoubleClickAction(LLVector3d pos_global);

    // <FS:Ansariel> Mark avatar feature
    static bool     hasAvatarMarkColor(const LLUUID& avatar_id) { return sAvatarMarksMap.find(avatar_id) != sAvatarMarksMap.end(); }
    static bool     getAvatarMarkColor(const LLUUID& avatar_id, LLColor4& color);
    static void     setAvatarMarkColor(const LLUUID& avatar_id, const LLSD& color);
    static void     setAvatarMarkColors(const uuid_vec_t& avatar_ids, const LLSD& color);
    static void     clearAvatarMarkColor(const LLUUID& avatar_id);
    static void     clearAvatarMarkColors(const uuid_vec_t& avatar_ids);
    static void     clearAvatarMarkColors();
    static LLColor4 getAvatarColor(const LLUUID& avatar_id);
    // </FS:Ansariel>

private:
    const LLVector3d& getObjectImageCenterGlobal()  { return mObjectImageCenterGlobal; }
    void            renderPoint(const LLVector3 &pos, const LLColor4U &color,
                                S32 diameter, S32 relative_height = 0);

    LLVector3       globalPosToView(const LLVector3d& global_pos);

    void            drawTracking( const LLVector3d& pos_global,
                                  const LLColor4& color,
                                  bool draw_arrow = true);
    void            drawRing(const F32 radius, LLVector3 pos_map, const LLUIColor& color);
    bool            isMouseOnPopupMenu();
    void            updateAboutLandPopupButton();
    bool            handleToolTipAgent(const LLUUID& avatar_id);
    static void     showAvatarInspector(const LLUUID& avatar_id);

// [SL:KB] - Patch: World-MinimapOverlay | Checked: 2012-06-20 (Catznip-3.3)
    bool            createImage(LLPointer<LLImageRaw>& rawimagep) const;
    void            createObjectImage();
    void            createParcelImage();

    F32             getScaleForName(std::string scale_name);
    void            renderPropertyLinesForRegion(const LLViewerRegion* pRegion, const LLColor4U& clrOverlay);
// [/SL:KB]
//  void            createObjectImage();

    static bool     outsideSlop(S32 x, S32 y, S32 start_x, S32 start_y, S32 slop);

//  bool            mUpdateNow;
// [SL:KB] - Patch: World-MinimapOverlay | Checked: 2012-06-20 (Catznip-3.3)
    bool            mUpdateObjectImage;
    bool            mUpdateParcelImage;
// [/SL:KB]

    LLUIColor       mBackgroundColor;

    F32             mScale;                 // Size of a region in pixels
    static F32      sScale;                 // <FS:Ansariel> Used to synchronize netmaps throughout instances

    F32             mPixelsPerMeter;        // world meters to map pixels
    F32             mObjectMapTPM;          // texels per meter on map
    F32             mObjectMapPixels;       // Width of object map in pixels
    F32             mDotRadius;             // Size of avatar markers

    bool            mPanning; // map is being dragged
    bool            mCentering; // map is being re-centered around the agent
    LLVector2       mCurPan;
    LLVector2       mStartPan; // pan offset at start of drag
    LLVector3d      mPopupWorldPos; // world position picked under mouse when context menu is opened
    LLCoordGL       mMouseDown; // pointer position at start of drag

    LLVector3d      mObjectImageCenterGlobal;
    LLPointer<LLImageRaw> mObjectRawImagep;
    LLPointer<LLViewerTexture>  mObjectImagep;
// [SL:KB] - Patch: World-MinimapOverlay | Checked: 2012-06-20 (Catznip-3.3)
    LLVector3d      mParcelImageCenterGlobal;
    LLPointer<LLImageRaw> mParcelRawImagep;
    LLPointer<LLViewerTexture>  mParcelImagep;

    boost::signals2::connection mParcelMgrConn;
    boost::signals2::connection mParcelOverlayConn;
// [/SL:KB]

    LLUUID          mClosestAgentToCursor;
// [SL:KB] - Patch: World-MiniMap | Checked: 2012-07-08 (Catznip-3.3)
    LLVector3d      mClosestAgentPosition;
// [/SL:KB]

    std::string     mToolTipMsg;
    std::string     mParcelNameMsg;
    std::string     mParcelSalePriceMsg;
    std::string     mParcelSaleAreaMsg;
    std::string     mParcelOwnerMsg;
    std::string     mRegionNameMsg;
    std::string     mToolTipHintMsg;
    std::string     mAltToolTipHintMsg;

    // <FS:Ansariel> Mark avatar feature
    typedef std::map<LLUUID, LLColor4> avatar_marks_map_t;
    static avatar_marks_map_t sAvatarMarksMap;

public:
    void            setSelected(uuid_vec_t uuids) { gmSelected=uuids; };
// <FS:CR> Minimap improvements
    void            handleShowProfile(const LLSD& sdParam) const;
    uuid_vec_t      mClosestAgentsToCursor;
    LLUUID          mClosestAgentRightClick;
    uuid_vec_t      mClosestAgentsRightClick;
// </FS:CR>

private:
    bool isZoomChecked(const LLSD& userdata);
    void setZoom(const LLSD& userdata);
    void handleStopTracking(const LLSD& userdata);
    void activateCenterMap(const LLSD& userdata);
    bool isMapOrientationChecked(const LLSD& userdata);
    void setMapOrientation(const LLSD& userdata);
    void popupShowAboutLand(const LLSD& userdata);
    void handleStartTracking();
    void handleMark(const LLSD& userdata);
    void handleClearMark();
    void handleClearMarks();
    void handleCam();
// [SL:KB] - Patch: World-MiniMap | Checked: 2012-07-08 (Catznip-3.3)
    void handleOverlayToggle(const LLSD& sdParam);
    void setAvatarProfileLabel(const LLUUID& av_id, const LLAvatarName& avName, const std::string& item_name);
    typedef std::map<LLUUID, boost::signals2::connection> avatar_name_cache_connection_map_t;
    avatar_name_cache_connection_map_t mAvatarNameCacheConnections;
// [/SL:KB]

    bool canAddFriend();
    bool canRemoveFriend();
    bool canCall();
    bool canMap();
    bool canShare();
    bool canOfferTeleport();
    bool canBlock();
    bool canFreezeEject();
    bool canKickTeleportHome();
    bool isBlocked();
    // <FS:Ansariel> Extra request teleport
    bool canRequestTeleport();

    void handleAddFriend();
    void handleAddToContactSet();
    void handleRemoveFriend();
    void handleIM();
    void handleCall();
    void handleMap();
    void handleShare();
    void handlePay();
    void handleOfferTeleport();
    void handleRequestTeleport();
    void handleTeleportToAvatar();
    void handleGroupInvite();
    void handleGetScriptInfo();
    void handleBlockUnblock();
    void handleReport();
    void handleFreeze();
    void handleEject();
    void handleKick();
    void handleTeleportHome();
    void handleEstateBan();
    void handleDerender(bool permanent);

    LLHandle<LLView> mPopupMenuHandle;
    uuid_vec_t      gmSelected;
};


#endif
