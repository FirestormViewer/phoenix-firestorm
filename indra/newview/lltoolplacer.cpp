/**
 * @file lltoolplacer.cpp
 * @brief Tool for placing new objects into the world
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

// self header
#include "lltoolplacer.h"

// viewer headers
#include "llbutton.h"
#include "llviewercontrol.h"
//#include "llfirstuse.h"
#include "llfloatertools.h"
#include "llselectmgr.h"
#include "llstatusbar.h"
#include "lltoolcomp.h"
#include "lltoolmgr.h"
#include "llviewerobject.h"
#include "llviewerregion.h"
#include "llviewerwindow.h"
#include "llworld.h"
#include "llui.h"
// [RLVa:KB] - Checked: 2010-03-23 (RLVa-1.2.0a)
#include "rlvhandler.h"
#include "rlvmodifiers.h"
// [/RLVa:KB]

//Headers added for functions moved from viewer.cpp
#include "llvograss.h"
#include "llvotree.h"
#include "llvolumemessage.h"
#include "llhudmanager.h"
#include "llagent.h"
#include "llagentcamera.h"
#include "llaudioengine.h"
#include "llhudeffecttrail.h"
#include "llviewerobjectlist.h"
#include "llviewercamera.h"
#include "llviewerstats.h"
#include "llvoavatarself.h"

// linden library headers
#include "llprimitive.h"
#include "llwindow.h"           // incBusyCount()
#include "material_codes.h"
#include "lluiusage.h"

// [RLVa:KB] - Checked: 2010-03-23 (RLVa-1.2.0a)
#include "rlvhandler.h"
// [/RLVa:KB]
#include "llparcel.h"
#include "llviewerparcelmgr.h"
#include "fscommon.h"
#include "roles_constants.h"

const LLVector3 DEFAULT_OBJECT_SCALE(0.5f, 0.5f, 0.5f);

//static
LLPCode LLToolPlacer::sObjectType = LL_PCODE_CUBE;

// <FS:Ansariel> FIRE-7802: Grass and tree selection in build tool
template<class P>
U32 get_selected_plant(const std::map<U32, P*>& list, const std::string& type, S32 max) // MC
{
    if (!type.empty() && !list.empty())
    {
        std::string last_selected = gSavedSettings.getString("LastSelected"+type);
        if (!last_selected.empty())
        {
            for (size_t i = 0; i < list.size(); ++i)
            {
                if (list.at(static_cast<U32>(i)) && list.at(static_cast<U32>(i))->mName == last_selected)
                {
                    return (U32)i;
                }
            }
        }
    }
    return rand() % max;
}
// </FS:Ansariel>

LLToolPlacer::LLToolPlacer()
:   LLTool( "Create" )
{
}

bool LLToolPlacer::raycastForNewObjPos( S32 x, S32 y, LLViewerObject** hit_obj, S32* hit_face,
                             bool* b_hit_land, LLVector3* ray_start_region, LLVector3* ray_end_region, LLViewerRegion** region )
{
    // <FS:Ansariel> Performance tweak and selection fix
    static LLCachedControl<bool> limitSelectDistance(gSavedSettings, "LimitSelectDistance");
    static LLCachedControl<F32> max_dist_from_camera(gSavedSettings, "MaxSelectDistance");
    //F32 max_dist_from_camera = gSavedSettings.getF32( "MaxSelectDistance" ) - 1.f;
    // </FS:Ansariel>

    // Viewer-side pick to find the right sim to create the object on.
    // First find the surface the object will be created on.
    LLPickInfo pick = gViewerWindow->pickImmediate(x, y, false, false);

    // Note: use the frontmost non-flora version because (a) plants usually have lots of alpha and (b) pants' Havok
    // representations (if any) are NOT the same as their viewer representation.
    if (pick.mPickType == LLPickInfo::PICK_FLORA)
    {
        *hit_obj = NULL;
        *hit_face = -1;
    }
    else
    {
        *hit_obj = pick.getObject();
        *hit_face = pick.mObjectFace;
    }
    *b_hit_land = !(*hit_obj) && !pick.mPosGlobal.isExactlyZero();
    LLVector3d land_pos_global = pick.mPosGlobal;

    // Make sure there's a surface to place the new object on.
    bool bypass_sim_raycast = false;
    LLVector3d  surface_pos_global;
    if (*b_hit_land)
    {
        surface_pos_global = land_pos_global;
        bypass_sim_raycast = true;
    }
    else
    if (*hit_obj)
    {
        surface_pos_global = (*hit_obj)->getPositionGlobal();
    }
    else
    {
        return false;
    }

    // Make sure the surface isn't too far away.
    LLVector3d ray_start_global = gAgentCamera.getCameraPositionGlobal();
    // <FS:Ansariel> Performance tweak and selection fix
    //F32 dist_to_surface_sq = (F32)((surface_pos_global - ray_start_global).magVecSquared());
    //if( dist_to_surface_sq > (max_dist_from_camera * max_dist_from_camera) )
    F32 dist_to_surface_sq = (F32)((surface_pos_global - gAgent.getPositionGlobal()).magVecSquared());
    if(limitSelectDistance && dist_to_surface_sq > (max_dist_from_camera * max_dist_from_camera) )
    // </FS:Ansariel>
    {
        return false;
    }

// [RLVa:KB] - Checked: 2010-04-11 (RLVa-1.2.0e) | Modified: RLVa-0.2.0f
    // NOTE: don't use surface_pos_global since for prims it will be the center of the prim while we need center + offset
    if (gRlvHandler.hasBehaviour(RLV_BHVR_FARTOUCH))
    {
        static RlvCachedBehaviourModifier<float> s_nFartouchDist(RLV_MODIFIER_FARTOUCHDIST);
        if (dist_vec_squared(gAgent.getPositionGlobal(), pick.mPosGlobal) > s_nFartouchDist * s_nFartouchDist)
            return false;
    }
// [/RLVa:KB]

    // Find the sim where the surface lives.
    LLViewerRegion *regionp = LLWorld::getInstance()->getRegionFromPosGlobal(surface_pos_global);
    if (!regionp)
    {
        LL_WARNS() << "Trying to add object outside of all known regions!" << LL_ENDL;
        return false;
    }

    // Find the simulator-side ray that will be used to place the object accurately
    LLVector3d      mouse_direction;
    mouse_direction.setVec( gViewerWindow->mouseDirectionGlobal( x, y ) );

    *region = regionp;
    *ray_start_region = regionp->getPosRegionFromGlobal( ray_start_global );
    F32 near_clip = LLViewerCamera::getInstance()->getNear() + 0.01f;  // Include an epsilon to avoid rounding issues.
    *ray_start_region += LLViewerCamera::getInstance()->getAtAxis() * near_clip;

    if( bypass_sim_raycast )
    {
        // Hack to work around Havok's inability to ray cast onto height fields
        *ray_end_region = regionp->getPosRegionFromGlobal( surface_pos_global );  // ray end is the viewer's intersection point
    }
    else
    {
        // <FS:Zi> Fix not being able to rez a box on top of a prim when MaxSelectDistance is shorter
        //         than the surface distance, even though LimitSelectDistance is switched off.
        // LLVector3d       ray_end_global = ray_start_global + (1.f + max_dist_from_camera) * mouse_direction;  // add an epsilon to the sim version of the ray to avoid rounding problems.
        F32 max_raycast_dist = 129.0f;
        if (limitSelectDistance)
        {
            max_raycast_dist = max_dist_from_camera;
        }
        LLVector3d ray_end_global = ray_start_global + max_raycast_dist * mouse_direction;  // add an epsilon to the sim version of the ray to avoid rounding problems.
        // </FS:Zi>
        *ray_end_region = regionp->getPosRegionFromGlobal( ray_end_global );
    }

    return true;
}


bool LLToolPlacer::addObject( LLPCode pcode, S32 x, S32 y, U8 use_physics )
{
    LLVector3 ray_start_region;
    LLVector3 ray_end_region;
    LLViewerRegion* regionp = NULL;
    bool b_hit_land = false;
    S32 hit_face = -1;
    LLViewerObject* hit_obj = NULL;
    U8 state = 0;
    bool success = raycastForNewObjPos( x, y, &hit_obj, &hit_face, &b_hit_land, &ray_start_region, &ray_end_region, &regionp );
    if( !success )
    {
        return false;
    }

    if( hit_obj && (hit_obj->isAvatar() || hit_obj->isAttachment()) )
    {
        // Can't create objects on avatars or attachments
        return false;
    }

    if (NULL == regionp)
    {
        LL_WARNS() << "regionp was NULL; aborting function." << LL_ENDL;
        return false;
    }

    if (regionp->getRegionFlag(REGION_FLAGS_SANDBOX))
    {
        //LLFirstUse::useSandbox();
    }

    // Set params for new object based on its PCode.
    LLQuaternion    rotation;
    LLVector3       scale = LLVector3(
        gSavedSettings.getF32("FSBuildPrefs_Xsize"),
        gSavedSettings.getF32("FSBuildPrefs_Ysize"),
        gSavedSettings.getF32("FSBuildPrefs_Zsize"));

    U8              material = LL_MCODE_WOOD;
    const std::string default_material = gSavedSettings.getString("FSBuildPrefs_Material");
    if (default_material == "Wood")         material = LL_MCODE_WOOD;
    else if (default_material == "Stone")   material = LL_MCODE_STONE;
    else if (default_material == "Metal")   material = LL_MCODE_METAL;
    else if (default_material == "Glass")   material = LL_MCODE_GLASS;
    else if (default_material == "Flesh")   material = LL_MCODE_FLESH;
    else if (default_material == "Rubber")  material = LL_MCODE_RUBBER;
    else if (default_material == "Plastic") material = LL_MCODE_PLASTIC;

    bool            create_selected = false;
    LLVolumeParams  volume_params;

    switch (pcode)
    {
    case LL_PCODE_LEGACY_GRASS:
        //  Randomize size of grass patch
        scale.setVec(10.f + ll_frand(20.f), 10.f + ll_frand(20.f),  1.f + ll_frand(2.f));
        // <FS:Ansariel> FIRE-7802: Grass and tree selection in build tool
        //state = rand() % LLVOGrass::sMaxGrassSpecies;
        state = get_selected_plant(LLVOGrass::sSpeciesTable, "Grass", LLVOGrass::sMaxGrassSpecies);
        // </FS:Ansariel>
        break;


    case LL_PCODE_LEGACY_TREE:
    case LL_PCODE_TREE_NEW:
        // <FS:Ansariel> FIRE-7802: Grass and tree selection in build tool
        //state = rand() % LLVOTree::sMaxTreeSpecies;
        state = get_selected_plant(LLVOTree::sSpeciesTable, "Tree", LLVOTree::sMaxTreeSpecies);
        // </FS:Ansariel>
        break;

    case LL_PCODE_SPHERE:
    case LL_PCODE_CONE:
    case LL_PCODE_CUBE:
    case LL_PCODE_CYLINDER:
    case LL_PCODE_TORUS:
    case LLViewerObject::LL_VO_SQUARE_TORUS:
    case LLViewerObject::LL_VO_TRIANGLE_TORUS:
    default:
        create_selected = true;
        break;
    }

    // Play creation sound
    // <FS:PP> Configurable UI sounds
    //if (gAudiop)
    if (gAudiop && gSavedSettings.getBOOL("PlayModeUISndObjectCreate"))
    // </FS:PP>
    {
        gAudiop->triggerSound( LLUUID(gSavedSettings.getString("UISndObjectCreate")),
                               gAgent.getID(), 1.0f, LLAudioEngine::AUDIO_TYPE_UI);
    }

    LLUIUsage::instance().logCommand("Build.ObjectAdd");
    gMessageSystem->newMessageFast(_PREHASH_ObjectAdd);
    gMessageSystem->nextBlockFast(_PREHASH_AgentData);
    gMessageSystem->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
    gMessageSystem->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
    gMessageSystem->addUUIDFast(_PREHASH_GroupID, FSCommon::getGroupForRezzing());
    gMessageSystem->nextBlockFast(_PREHASH_ObjectData);
    gMessageSystem->addU8Fast(_PREHASH_Material,    material);

    U32 flags = 0;      // not selected
    if (use_physics)
    {
        flags |= FLAGS_USE_PHYSICS;
    }
//  if (create_selected)
// [RLVa:KB] - Checked: 2010-04-11 (RLVa-1.2.0e) | Added: RLVa-1.0.0b
    if ( (create_selected) && (!gRlvHandler.hasBehaviour(RLV_BHVR_EDIT)) )
// [/RLVa:KB]
    {
        flags |= FLAGS_CREATE_SELECTED;
    }
    gMessageSystem->addU32Fast(_PREHASH_AddFlags, flags );

    LLPCode volume_pcode;   // ...PCODE_VOLUME, or the original on error
    switch (pcode)
    {
    case LL_PCODE_SPHERE:
        rotation.setQuat(90.f * DEG_TO_RAD, LLVector3::y_axis);

        volume_params.setType( LL_PCODE_PROFILE_CIRCLE_HALF, LL_PCODE_PATH_CIRCLE );
        volume_params.setBeginAndEndS( 0.f, 1.f );
        volume_params.setBeginAndEndT( 0.f, 1.f );
        volume_params.setRatio  ( 1, 1 );
        volume_params.setShear  ( 0, 0 );
        LLVolumeMessage::packVolumeParams(&volume_params, gMessageSystem);
        volume_pcode = LL_PCODE_VOLUME;
        break;

    case LL_PCODE_TORUS:
        rotation.setQuat(90.f * DEG_TO_RAD, LLVector3::y_axis);

        volume_params.setType( LL_PCODE_PROFILE_CIRCLE, LL_PCODE_PATH_CIRCLE );
        volume_params.setBeginAndEndS( 0.f, 1.f );
        volume_params.setBeginAndEndT( 0.f, 1.f );
        volume_params.setRatio  ( 1.f, 0.25f ); // "top size"
        volume_params.setShear  ( 0, 0 );
        LLVolumeMessage::packVolumeParams(&volume_params, gMessageSystem);
        volume_pcode = LL_PCODE_VOLUME;
        break;

    case LLViewerObject::LL_VO_SQUARE_TORUS:
        rotation.setQuat(90.f * DEG_TO_RAD, LLVector3::y_axis);

        volume_params.setType( LL_PCODE_PROFILE_SQUARE, LL_PCODE_PATH_CIRCLE );
        volume_params.setBeginAndEndS( 0.f, 1.f );
        volume_params.setBeginAndEndT( 0.f, 1.f );
        volume_params.setRatio  ( 1.f, 0.25f ); // "top size"
        volume_params.setShear  ( 0, 0 );
        LLVolumeMessage::packVolumeParams(&volume_params, gMessageSystem);
        volume_pcode = LL_PCODE_VOLUME;
        break;

    case LLViewerObject::LL_VO_TRIANGLE_TORUS:
        rotation.setQuat(90.f * DEG_TO_RAD, LLVector3::y_axis);

        volume_params.setType( LL_PCODE_PROFILE_EQUALTRI, LL_PCODE_PATH_CIRCLE );
        volume_params.setBeginAndEndS( 0.f, 1.f );
        volume_params.setBeginAndEndT( 0.f, 1.f );
        volume_params.setRatio  ( 1.f, 0.25f ); // "top size"
        volume_params.setShear  ( 0, 0 );
        LLVolumeMessage::packVolumeParams(&volume_params, gMessageSystem);
        volume_pcode = LL_PCODE_VOLUME;
        break;

    case LL_PCODE_SPHERE_HEMI:
        volume_params.setType( LL_PCODE_PROFILE_CIRCLE_HALF, LL_PCODE_PATH_CIRCLE );
        //volume_params.setBeginAndEndS( 0.5f, 1.f );
        volume_params.setBeginAndEndT( 0.f, 0.5f );
        volume_params.setRatio  ( 1, 1 );
        volume_params.setShear  ( 0, 0 );
        LLVolumeMessage::packVolumeParams(&volume_params, gMessageSystem);
        volume_pcode = LL_PCODE_VOLUME;
        break;

    case LL_PCODE_CUBE:
        volume_params.setType( LL_PCODE_PROFILE_SQUARE, LL_PCODE_PATH_LINE );
        volume_params.setBeginAndEndS( 0.f, 1.f );
        volume_params.setBeginAndEndT( 0.f, 1.f );
        volume_params.setRatio  ( 1, 1 );
        volume_params.setShear  ( 0, 0 );
        LLVolumeMessage::packVolumeParams(&volume_params, gMessageSystem);
        volume_pcode = LL_PCODE_VOLUME;
        break;

    case LL_PCODE_PRISM:
        volume_params.setType( LL_PCODE_PROFILE_SQUARE, LL_PCODE_PATH_LINE );
        volume_params.setBeginAndEndS( 0.f, 1.f );
        volume_params.setBeginAndEndT( 0.f, 1.f );
        volume_params.setRatio  ( 0, 1 );
        volume_params.setShear  ( -0.5f, 0 );
        LLVolumeMessage::packVolumeParams(&volume_params, gMessageSystem);
        volume_pcode = LL_PCODE_VOLUME;
        break;

    case LL_PCODE_PYRAMID:
        volume_params.setType( LL_PCODE_PROFILE_SQUARE, LL_PCODE_PATH_LINE );
        volume_params.setBeginAndEndS( 0.f, 1.f );
        volume_params.setBeginAndEndT( 0.f, 1.f );
        volume_params.setRatio  ( 0, 0 );
        volume_params.setShear  ( 0, 0 );
        LLVolumeMessage::packVolumeParams(&volume_params, gMessageSystem);
        volume_pcode = LL_PCODE_VOLUME;
        break;

    case LL_PCODE_TETRAHEDRON:
        volume_params.setType( LL_PCODE_PROFILE_EQUALTRI, LL_PCODE_PATH_LINE );
        volume_params.setBeginAndEndS( 0.f, 1.f );
        volume_params.setBeginAndEndT( 0.f, 1.f );
        volume_params.setRatio  ( 0, 0 );
        volume_params.setShear  ( 0, 0 );
        LLVolumeMessage::packVolumeParams(&volume_params, gMessageSystem);
        volume_pcode = LL_PCODE_VOLUME;
        break;

    case LL_PCODE_CYLINDER:
        volume_params.setType( LL_PCODE_PROFILE_CIRCLE, LL_PCODE_PATH_LINE );
        volume_params.setBeginAndEndS( 0.f, 1.f );
        volume_params.setBeginAndEndT( 0.f, 1.f );
        volume_params.setRatio  ( 1, 1 );
        volume_params.setShear  ( 0, 0 );
        LLVolumeMessage::packVolumeParams(&volume_params, gMessageSystem);
        volume_pcode = LL_PCODE_VOLUME;
        break;

    case LL_PCODE_CYLINDER_HEMI:
        volume_params.setType( LL_PCODE_PROFILE_CIRCLE, LL_PCODE_PATH_LINE );
        volume_params.setBeginAndEndS( 0.25f, 0.75f );
        volume_params.setBeginAndEndT( 0.f, 1.f );
        volume_params.setRatio  ( 1, 1 );
        volume_params.setShear  ( 0, 0 );
        LLVolumeMessage::packVolumeParams(&volume_params, gMessageSystem);
        volume_pcode = LL_PCODE_VOLUME;
        break;

    case LL_PCODE_CONE:
        volume_params.setType( LL_PCODE_PROFILE_CIRCLE, LL_PCODE_PATH_LINE );
        volume_params.setBeginAndEndS( 0.f, 1.f );
        volume_params.setBeginAndEndT( 0.f, 1.f );
        volume_params.setRatio  ( 0, 0 );
        volume_params.setShear  ( 0, 0 );
        LLVolumeMessage::packVolumeParams(&volume_params, gMessageSystem);
        volume_pcode = LL_PCODE_VOLUME;
        break;

    case LL_PCODE_CONE_HEMI:
        volume_params.setType( LL_PCODE_PROFILE_CIRCLE, LL_PCODE_PATH_LINE );
        volume_params.setBeginAndEndS( 0.25f, 0.75f );
        volume_params.setBeginAndEndT( 0.f, 1.f );
        volume_params.setRatio  ( 0, 0 );
        volume_params.setShear  ( 0, 0 );
        LLVolumeMessage::packVolumeParams(&volume_params, gMessageSystem);
        volume_pcode = LL_PCODE_VOLUME;
        break;

    default:
        LLVolumeMessage::packVolumeParams(0, gMessageSystem);
        volume_pcode = pcode;
        break;
    }
    gMessageSystem->addU8Fast(_PREHASH_PCode, volume_pcode);

    gMessageSystem->addVector3Fast(_PREHASH_Scale,          scale );
    gMessageSystem->addQuatFast(_PREHASH_Rotation,          rotation );
    gMessageSystem->addVector3Fast(_PREHASH_RayStart,       ray_start_region );
    gMessageSystem->addVector3Fast(_PREHASH_RayEnd,         ray_end_region );
    gMessageSystem->addU8Fast(_PREHASH_BypassRaycast,       (U8)b_hit_land );
    gMessageSystem->addU8Fast(_PREHASH_RayEndIsIntersection, (U8)false );
    gMessageSystem->addU8Fast(_PREHASH_State, state);

    // Limit raycast to a single object.
    // Speeds up server raycast + avoid problems with server ray hitting objects
    // that were clipped by the near plane or culled on the viewer.
    LLUUID ray_target_id;
    if( hit_obj )
    {
        ray_target_id = hit_obj->getID();
    }
    else
    {
        ray_target_id.setNull();
    }
    gMessageSystem->addUUIDFast(_PREHASH_RayTargetID,           ray_target_id );

    // Pack in name value pairs
    gMessageSystem->sendReliable(regionp->getHost());

    // Spawns a message, so must be after above send
    if (create_selected)
    {
        FSCommon::sObjectAddMsg++;
        LLSelectMgr::getInstance()->deselectAll();
        gViewerWindow->getWindow()->incBusyCount();
    }

    // VEFFECT: AddObject
    LLHUDEffectSpiral *effectp = (LLHUDEffectSpiral *)LLHUDManager::getInstance()->createViewerEffect(LLHUDObject::LL_HUD_EFFECT_BEAM, true);
    effectp->setSourceObject((LLViewerObject*)gAgentAvatarp);
    effectp->setPositionGlobal(regionp->getPosGlobalFromRegion(ray_end_region));
    effectp->setDuration(LL_HUD_DUR_SHORT);
    effectp->setColor(LLColor4U(gAgent.getEffectColor()));

    add(LLStatViewer::OBJECT_CREATE, 1);

    return true;
}

// Used by the placer tool to add copies of the current selection.
// Inspired by add_object().  JC
bool LLToolPlacer::addDuplicate(S32 x, S32 y)
{
    LLVector3 ray_start_region;
    LLVector3 ray_end_region;
    LLViewerRegion* regionp = NULL;
    bool b_hit_land = false;
    S32 hit_face = -1;
    LLViewerObject* hit_obj = NULL;
    bool success = raycastForNewObjPos( x, y, &hit_obj, &hit_face, &b_hit_land, &ray_start_region, &ray_end_region, &regionp );
    if( !success )
    {
        make_ui_sound("UISndInvalidOp");
        return false;
    }
    if( hit_obj && (hit_obj->isAvatar() || hit_obj->isAttachment()) )
    {
        // Can't create objects on avatars or attachments
        make_ui_sound("UISndInvalidOp");
        return false;
    }


    // Limit raycast to a single object.
    // Speeds up server raycast + avoid problems with server ray hitting objects
    // that were clipped by the near plane or culled on the viewer.
    LLUUID ray_target_id;
    if( hit_obj )
    {
        ray_target_id = hit_obj->getID();
    }
    else
    {
        ray_target_id.setNull();
    }

    LLSelectMgr::getInstance()->selectDuplicateOnRay(ray_start_region,
                                        ray_end_region,
                                        b_hit_land,         // suppress raycast
                                        false,              // intersection
                                        ray_target_id,
                                        gSavedSettings.getBOOL("CreateToolCopyCenters"),
                                        gSavedSettings.getBOOL("CreateToolCopyRotates"),
                                        false);             // select copy

    if (regionp
        && (regionp->getRegionFlag(REGION_FLAGS_SANDBOX)))
    {
        //LLFirstUse::useSandbox();
    }

    return true;
}


bool LLToolPlacer::placeObject(S32 x, S32 y, MASK mask)
{
    bool added = true;

// [RLVa:KB] - Checked: 2010-03-23 (RLVa-1.2.0e) | Modified: RLVa-1.1.0l
    if ( (rlv_handler_t::isEnabled()) && ((gRlvHandler.hasBehaviour(RLV_BHVR_REZ)) || (gRlvHandler.hasBehaviour(RLV_BHVR_INTERACT))) )
    {
        return true; // Callers seem to expect a "did you handle it?" so we return true rather than false
    }
// [/RLVa:KB]

    if (gSavedSettings.getBOOL("CreateToolCopySelection"))
    {
        added = addDuplicate(x, y);
    }
    else
    {
        added = addObject( sObjectType, x, y, false );
    }

    // ...and go back to the default tool
    if (added && !gSavedSettings.getBOOL("CreateToolKeepSelected"))
    {
        LLToolMgr::getInstance()->getCurrentToolset()->selectTool( LLToolCompTranslate::getInstance() );
    }

    return added;
}

bool LLToolPlacer::handleHover(S32 x, S32 y, MASK mask)
{
    LL_DEBUGS("UserInput") << "hover handled by LLToolPlacer" << LL_ENDL;
    gViewerWindow->setCursor(UI_CURSOR_TOOLCREATE);
    return true;
}

void LLToolPlacer::handleSelect()
{
    if (gFloaterTools)
    {
        gFloaterTools->setStatusText("place");
    }
}

void LLToolPlacer::handleDeselect()
{
}

