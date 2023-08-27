/**
 * @file llmodelpreview.cpp
 * @brief LLModelPreview class implementation
 *
 * $LicenseInfo:firstyear=2020&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2020, Linden Research, Inc.
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

#include "llmodelpreview.h"

#include "llmodelloader.h"
#include "lldaeloader.h"
#include "llfloatermodelpreview.h"

#include "llagent.h"
#include "llanimationstates.h"
#include "llcallbacklist.h"
#include "lldatapacker.h"
#include "lldrawable.h"
#include "llface.h"
#include "lliconctrl.h"
#include "llmatrix4a.h"
#include "llmeshrepository.h"
#include "llmeshoptimizer.h"
#include "llrender.h"
#include "llsdutil_math.h"
#include "llskinningutil.h"
#include "llstring.h"
#include "llsdserialize.h"
#include "lltoolmgr.h"
#include "llui.h"
#include "llvector4a.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewerobjectlist.h"
#include "llviewernetwork.h"
#include "llviewershadermgr.h"
#include "llviewertexteditor.h"
#include "llviewertexturelist.h"
#include "llvoavatar.h"
#include "pipeline.h"

// ui controls (from floater)
#include "llbutton.h"
#include "llcombobox.h"
#include "llspinctrl.h"
#include "lltabcontainer.h"
#include "lltextbox.h"

#include <boost/algorithm/string.hpp>
// <AW: opensim-limits>
#include "llworld.h"
// </AW: opensim-limits>

bool LLModelPreview::sIgnoreLoadedCallback = false;
// <FS:Beq> fix up and restore stuff removed by lab
// // Extra configurability, to be exposed later in xml (LLModelPreview probably
// // should become UI control at some point or get split into preview control)
// static const LLColor4 PREVIEW_CANVAS_COL(0.169f, 0.169f, 0.169f, 1.f);
// static const LLColor4 PREVIEW_EDGE_COL(0.4f, 0.4f, 0.4f, 1.0);
// static const LLColor4 PREVIEW_BASE_COL(1.f, 1.f, 1.f, 1.f);
// static const LLColor3 PREVIEW_BRIGHTNESS(0.9f, 0.9f, 0.9f);
// static const F32 PREVIEW_EDGE_WIDTH(1.f);
// static const LLColor4 PREVIEW_PSYH_EDGE_COL(0.f, 0.25f, 0.5f, 0.25f);
// static const LLColor4 PREVIEW_PSYH_FILL_COL(0.f, 0.5f, 1.0f, 0.5f);
// static const F32 PREVIEW_PSYH_EDGE_WIDTH(1.f);
// static const LLColor4 PREVIEW_DEG_EDGE_COL(1.f, 0.f, 0.f, 1.f);
// static const LLColor4 PREVIEW_DEG_FILL_COL(1.f, 0.f, 0.f, 0.5f);
// static const F32 PREVIEW_DEG_EDGE_WIDTH(3.f);
// static const F32 PREVIEW_DEG_POINT_SIZE(8.f);
// static const F32 PREVIEW_ZOOM_LIMIT(10.f);
// </FS:Beq>
static const std::string DEFAULT_PHYSICS_MESH_NAME = "default_physics_shape";
const F32 SKIN_WEIGHT_CAMERA_DISTANCE = 16.f;

#include "glod/glod.h" // <FS:Beq/> More flexible LOD generation
// <FS:Beq> mesh loader suffix configuration
//static 
const std::array<std::string,5> LLModelPreview::sSuffixVarNames
{
    "FSMeshLowestLodSuffix",
    "FSMeshLowLodSuffix",
    "FSMeshMediumLodSuffix",
    "FSMeshHighLodSuffix",
    "FSMeshPhysicsSuffix"
};
// </FS:Beq>

// <FS:Beq> More flexible LOD generation
BOOL stop_gloderror()
{
    GLuint error = glodGetError();

    if (error != GLOD_NO_ERROR)
    {
   		std::ostringstream out;
		out << "GLOD error detected, cannot generate LOD (try another method?): " << std::hex << error;
		LL_WARNS("MeshUpload") << out.str() << LL_ENDL;
		LLFloaterModelPreview::addStringToLog(out, true);
        return TRUE;
    }

    return FALSE;
}
// </FS:Beq>

LLViewerFetchedTexture* bindMaterialDiffuseTexture(const LLImportMaterial& material)
{
    LLViewerFetchedTexture *texture = LLViewerTextureManager::getFetchedTexture(material.getDiffuseMap(), FTT_DEFAULT, TRUE, LLGLTexture::BOOST_PREVIEW);

    if (texture)
    {
        if (texture->getDiscardLevel() > -1)
        {
            gGL.getTexUnit(0)->bind(texture, true);
            return texture;
        }
    }

    return NULL;
}

std::string stripSuffix(std::string name)
{
    // <FS:Beq> Selectable suffixes
    //if ((name.find("_LOD") != -1) || (name.find("_PHYS") != -1))
    // {
    //     return name.substr(0, name.rfind('_'));
    // }
    for(int i=0; i < LLModel::NUM_LODS; i++)
    {
        const auto& suffix = gSavedSettings.getString(LLModelPreview::sSuffixVarNames[i]);
        if (suffix.size() && name.find(suffix) != std::string::npos) 
        {
            return name.substr(0, name.rfind('_'));
        }
    } // </FS:Beq>
    return name;
}

std::string getLodSuffix(S32 lod)
{
    std::string suffix;
    switch (lod)
    {
    // <FS:Beq> selectable suffixes
    // case LLModel::LOD_IMPOSTOR: suffix = "_LOD0"; break;
    // case LLModel::LOD_LOW:      suffix = "_LOD1"; break;
    // case LLModel::LOD_MEDIUM:   suffix = "_LOD2"; break;
    // case LLModel::LOD_PHYSICS:  suffix = "_PHYS"; break;
    // case LLModel::LOD_HIGH:                       break;
    case LLModel::LOD_IMPOSTOR: suffix = gSavedSettings.getString("FSMeshLowestLodSuffix"); break;
    case LLModel::LOD_LOW:      suffix = gSavedSettings.getString("FSMeshLowLodSuffix"); break;
    case LLModel::LOD_MEDIUM:   suffix = gSavedSettings.getString("FSMeshMediumLodSuffix"); break;
    case LLModel::LOD_HIGH:     suffix = gSavedSettings.getString("FSMeshHighLodSuffix"); break;
    case LLModel::LOD_PHYSICS:  suffix = gSavedSettings.getString("FSMeshPhysicsSuffix"); break;
    default:break;
    }
    if(suffix.size())
    {
        suffix = "_" + suffix;
    }
    // </FS:Beq>
    return suffix;
}

void FindModel(LLModelLoader::scene& scene, const std::string& name_to_match, LLModel*& baseModelOut, LLMatrix4& matOut)
{
    LLModelLoader::scene::iterator base_iter = scene.begin();
    bool found = false;
    while (!found && (base_iter != scene.end()))
    {
        matOut = base_iter->first;

        LLModelLoader::model_instance_list::iterator base_instance_iter = base_iter->second.begin();
        while (!found && (base_instance_iter != base_iter->second.end()))
        {
            LLModelInstance& base_instance = *base_instance_iter++;
            LLModel* base_model = base_instance.mModel;

            if (base_model && (base_model->mLabel == name_to_match))
            {
                baseModelOut = base_model;
                return;
            }
        }
        base_iter++;
    }
}

//-----------------------------------------------------------------------------
// LLModelPreview
//-----------------------------------------------------------------------------

LLModelPreview::LLModelPreview(S32 width, S32 height, LLFloater* fmp)
    : LLViewerDynamicTexture(width, height, 3, ORDER_MIDDLE, FALSE), LLMutex()
    , mLodsQuery()
    , mLodsWithParsingError()
    , mPelvisZOffset(0.0f)
    , mLegacyRigFlags(U32_MAX)
    , mRigValidJointUpload(false)
    , mPhysicsSearchLOD(LLModel::LOD_PHYSICS)
    , mResetJoints(false)
    , mModelNoErrors(true)
    , mLastJointUpdate(false)
    , mFirstSkinUpdate(true)
    , mHasDegenerate(false)
    , mImporterDebug(LLCachedControl<bool>(gSavedSettings, "ImporterDebug", false))
{
    mNeedsUpdate = TRUE;
    mCameraDistance = 0.f;
    mCameraYaw = 0.f;
    mCameraPitch = 0.f;
    mCameraZoom = 1.f;
    mTextureName = 0;
    mPreviewLOD = 0;
    mModelLoader = NULL;
    mMaxTriangleLimit = 0;
    mDirty = false;
    mGenLOD = false;
    mLoading = false;
    mLookUpLodFiles = false;
    mLoadState = LLModelLoader::STARTING;
    mGroup = 0;
    mLODFrozen = false;
    // <FS:Beq> Improved LOD generation
    mBuildShareTolerance = 0.f;
    mBuildQueueMode = GLOD_QUEUE_GREEDY;
    mBuildBorderMode = GLOD_BORDER_UNLOCK;
    mBuildOperator = GLOD_OPERATOR_EDGE_COLLAPSE;
    // </FS:Beq>
    mUVGuideTexture = LLViewerTextureManager::getFetchedTextureFromFile(gSavedSettings.getString("FSMeshPreviewUVGuideFile"), FTT_LOCAL_FILE, TRUE, LLGLTexture::BOOST_PREVIEW); // <FS:Beq> - Add UV guide overlay to pmesh preview

    for (U32 i = 0; i < LLModel::NUM_LODS; ++i)
    {
        mRequestedTriangleCount[i] = 0;
        mRequestedCreaseAngle[i] = -1.f;
        mRequestedLoDMode[i] = 0;
        mRequestedErrorThreshold[i] = 0.f;
    }

    mViewOption["show_textures"] = false;

    mFMP = fmp;
    glodInit(); // <FS:Beq/> Improved LOD generation
    mHasPivot = false;
    mModelPivot = LLVector3(0.0f, 0.0f, 0.0f);

    createPreviewAvatar();
}

LLModelPreview::~LLModelPreview()
{
    if (mModelLoader)
    {
        mModelLoader->shutdown();
    }

    if (mPreviewAvatar)
    {
        mPreviewAvatar->markDead();
        mPreviewAvatar = NULL;
    }
}

void LLModelPreview::updateDimentionsAndOffsets()
{
    assert_main_thread();

    rebuildUploadData();

    std::set<LLModel*> accounted;

    mPelvisZOffset = mFMP ? mFMP->childGetValue("pelvis_offset").asReal() : 3.0f;

    if (mFMP && mFMP->childGetValue("upload_joints").asBoolean())
    {
        // FIXME if preview avatar ever gets reused, this fake mesh ID stuff will fail.
        // see also call to addAttachmentPosOverride.
        LLUUID fake_mesh_id;
        fake_mesh_id.generate();
        getPreviewAvatar()->addPelvisFixup(mPelvisZOffset, fake_mesh_id);
    }

    for (U32 i = 0; i < mUploadData.size(); ++i)
    {
        LLModelInstance& instance = mUploadData[i];

        if (accounted.find(instance.mModel) == accounted.end())
        {
            accounted.insert(instance.mModel);

            //update instance skin info for each lods pelvisZoffset 
            for (int j = 0; j<LLModel::NUM_LODS; ++j)
            {
                if (instance.mLOD[j])
                {
                    instance.mLOD[j]->mSkinInfo.mPelvisOffset = mPelvisZOffset;
                }
            }
        }
    }

    F32 scale = mFMP ? mFMP->childGetValue("import_scale").asReal()*2.f : 2.f;

    mDetailsSignal((F32)(mPreviewScale[0] * scale), (F32)(mPreviewScale[1] * scale), (F32)(mPreviewScale[2] * scale));

    updateStatusMessages();
}

// <FS:Beq> relocate from llmodel and rewrite so it does what it is meant to
// Material matching should work as the comment below states (subsets are allowed)
// prior to this a mess in multiple places meant that all LODs are forced to carry unwanted triangles for unused materials
bool LLModelPreview::matchMaterialOrder(LLModel* lod, LLModel* ref, int& refFaceCnt, int& modelFaceCnt )
{
	//Is this a subset?
	//LODs cannot currently add new materials, e.g.
	//1. ref = a,b,c lod1 = d,e => This is not permitted
	//2. ref = a,b,c lod1 = c => This would be permitted
	
	LL_DEBUGS("MESHSKININFO") << "In matchMaterialOrder." << LL_ENDL;
	bool isASubset = lod->isMaterialListSubset( ref );
	if ( !isASubset )
	{
		LL_DEBUGS("MESHSKININFO")<<"Material of model is not a subset of reference."<<LL_ENDL;
		std::ostringstream out;
		out << "LOD model " << lod->getName() << "'s materials are not a subset of the High LOD (reference) model " << ref->getName();
		LL_DEBUGS() << out.str() << LL_ENDL;
		LLFloaterModelPreview::addStringToLog(out, true);
		return false;
	}

	if (lod->mMaterialList.size() > ref->mMaterialList.size())
	{
		LL_DEBUGS("MESHSKININFO") << "Material of model has more materials than a reference." << LL_ENDL;
		std::ostringstream out;
		out << "LOD model " << lod->getName() << " has more materials than the High LOD (reference) model " << ref->getName();
		LL_DEBUGS() << out.str() << LL_ENDL;
		LLFloaterModelPreview::addStringToLog(out, true);
		// We passed isMaterialListSubset, so materials are a subset, but subset isn't supposed to be
		// larger than original and if we keep going, reordering will cause a crash
		return false;
	}

	LL_DEBUGS("MESHSKININFO") << "subset check passed." << LL_ENDL;
	std::map<std::string, U32> index_map;
	
	//build a map of material slot names to face indexes
	bool reorder = false;
	auto max_lod_mats =  lod->mMaterialList.size();

	for ( U32 i = 0; i < ref->mMaterialList.size(); i++ )
	{
		// create the reference map for later
		index_map[ref->mMaterialList[i]] = i;
		LL_DEBUGS("MESHSKININFO") << "setting reference material " <<  ref->mMaterialList[i] << " as index " << i << LL_ENDL;
		if( i >= max_lod_mats ||  lod->mMaterialList[i] != ref->mMaterialList[i] )
		{
			// i is already out of range of the original material sets in this LOD OR is not matching.
			LL_DEBUGS("MESHSKININFO") << "mismatch at " << i << " " << ref->mMaterialList[i] 
									 << " != " << ((i >= max_lod_mats)? "Out-of-range":lod->mMaterialList[i]) << LL_ENDL;
			// we have a misalignment/ordering
			// check that ref[i] is in cur and if not add a blank
			U32 j{0};
			for ( ; j < max_lod_mats; j++ )
			{
				if( i != j && lod->mMaterialList[j] == ref->mMaterialList[i] )
				{
					LL_DEBUGS("MESHSKININFO") << "material " << ref->mMaterialList[i] 
											<< " found at " << j << LL_ENDL;
					// we found it but in the wrong place.
					reorder = true;
					break;
				}
			}
			if( j >= max_lod_mats )
			{
				std::ostringstream out;
				out << "material " << ref->mMaterialList[i] 
					<< " not found in lod adding placeholder";
				LL_DEBUGS("MESHSKININFO") << out.str() << LL_ENDL;
				if (mImporterDebug)
				{
					LLFloaterModelPreview::addStringToLog(out, false);
				}
			// The material is not in the submesh, add a placeholder.
				// this is appended to the existing data so we'll need to reorder
				// Note that this placeholder will be eliminated on the writeData (upload) and replaced with
				// "NoGeometry" in the LLSD
				reorder = true; 
				LLVolumeFace face;

				face.resizeIndices(3);
				face.resizeVertices(1);
				face.mPositions->clear();
				face.mNormals->clear();
				face.mTexCoords->setZero();
				memset(face.mIndices, 0, sizeof(U16)*3);
				lod->addFace(face);
				lod->mMaterialList.push_back( ref->mMaterialList[i] );
			}
		}
		//if any material name does not match reference, we need to reorder
	}
	LL_DEBUGS("MESHSKININFO") << "finished parsing materials" << LL_ENDL; 
	for ( U32 i = 0; i < lod->mMaterialList.size(); i++ )
	{
		LL_DEBUGS("MESHSKININFO") << "lod material " <<  lod->mMaterialList[i] << " has index " << i << LL_ENDL;    
	}
	// Sanity check. We have added placeholders for any mats in ref that are not in this.
	// the mat count MUST be equal now.
	if (lod->mMaterialList.size() != ref->mMaterialList.size())
	{
		std::ostringstream out;
		out << "Material of LOD model " << lod->getName() << " has more materials than the reference " << ref->getName() << ".";
		LL_INFOS("MESHSKININFO") << out.str() << LL_ENDL;
		LLFloaterModelPreview::addStringToLog(out, true);
		return false;
	}


	// <FS:Beq> Fix up material matching badness
	// if (reorder &&  (base_mat == cur_mat)) //don't reorder if material name sets don't match
	if ( reorder )
	{
		LL_DEBUGS("MESHSKININFO") << "re-ordering." << LL_ENDL;
		lod->sortVolumeFacesByMaterialName();
		lod->mMaterialList = ref->mMaterialList;
	}
	
	return true;
}
//</FS:Beq> 

void LLModelPreview::rebuildUploadData()
{
    assert_main_thread();

    mUploadData.clear();
    mTextureSet.clear();

    //fill uploaddata instance vectors from scene data

    std::string requested_name = mFMP->getChild<LLUICtrl>("description_form")->getValue().asString();

    LLSpinCtrl* scale_spinner = mFMP->getChild<LLSpinCtrl>("import_scale");

    F32 scale = scale_spinner->getValue().asReal();

    LLMatrix4 scale_mat;
    scale_mat.initScale(LLVector3(scale, scale, scale));

    F32 max_scale = 0.f;

    BOOL legacyMatching = gSavedSettings.getBOOL("ImporterLegacyMatching");
    U32 load_state = 0;

    for (LLModelLoader::scene::iterator iter = mBaseScene.begin(); iter != mBaseScene.end(); ++iter)
    { //for each transform in scene
        LLMatrix4 mat = iter->first;

        // compute position
        LLVector3 position = LLVector3(0, 0, 0) * mat;

        // compute scale
        LLVector3 x_transformed = LLVector3(1, 0, 0) * mat - position;
        LLVector3 y_transformed = LLVector3(0, 1, 0) * mat - position;
        LLVector3 z_transformed = LLVector3(0, 0, 1) * mat - position;
        F32 x_length = x_transformed.normalize();
        F32 y_length = y_transformed.normalize();
        F32 z_length = z_transformed.normalize();

        max_scale = llmax(llmax(llmax(max_scale, x_length), y_length), z_length);

        mat *= scale_mat;

        for (LLModelLoader::model_instance_list::iterator model_iter = iter->second.begin(); model_iter != iter->second.end();)
        { //for each instance with said transform applied 
            LLModelInstance instance = *model_iter++;

            LLModel* base_model = instance.mModel;

            if (base_model && !requested_name.empty())
            {
                base_model->mRequestedLabel = requested_name;
            }

            for (int i = LLModel::NUM_LODS - 1; i >= LLModel::LOD_IMPOSTOR; i--)
            {
                LLModel* lod_model = NULL;
                if (!legacyMatching)
                {
                    // Fill LOD slots by finding matching meshes by label with name extensions
                    // in the appropriate scene for each LOD. This fixes all kinds of issues
                    // where the indexed method below fails in spectacular fashion.
                    // If you don't take the time to name your LOD and PHYS meshes
                    // with the name of their corresponding mesh in the HIGH LOD,
                    // then the indexed method will be attempted below.

                    LLMatrix4 transform;
                    // <FS:Beq> user defined LOD names
                    // std::string name_to_match = instance.mLabel;
                    std::string name_to_match = stripSuffix(instance.mLabel);
                    // </FS:Beq>
                    llassert(!name_to_match.empty());

                    int extensionLOD;
                    if (i != LLModel::LOD_PHYSICS || mModel[LLModel::LOD_PHYSICS].empty())
                    {
                        extensionLOD = i;
                    }
                    else
                    {
                        //Physics can be inherited from other LODs or loaded, so we need to adjust what extension we are searching for
                        extensionLOD = mPhysicsSearchLOD;
                    }

                    std::string toAdd = getLodSuffix(extensionLOD);

                    // <FS:Ansariel> Bug fixes in mesh importer by Drake Arconis
                    //if (name_to_match.find(toAdd) == -1)
                    if (name_to_match.find(toAdd) == std::string::npos)
                    // </FS:Ansariel>
                    {
                        name_to_match += toAdd;
                    }

                    FindModel(mScene[i], name_to_match, lod_model, transform);

                    if (!lod_model && i != LLModel::LOD_PHYSICS)
                    {
                        if (mImporterDebug)
                        {
                            std::ostringstream out;
                            out << "Search of " << name_to_match;
                            out << " in LOD" << i;
                            out << " list failed. Searching for alternative among LOD lists.";
                            LL_INFOS() << out.str() << LL_ENDL;
                            LLFloaterModelPreview::addStringToLog(out, false);
                        }

                        int searchLOD = (i > LLModel::LOD_HIGH) ? LLModel::LOD_HIGH : i;
                        while ((searchLOD <= LLModel::LOD_HIGH) && !lod_model)
                        {
                            // <FS:Beq> user defined LOD names
                            // std::string name_to_match = instance.mLabel;
                            std::string name_to_match = stripSuffix(instance.mLabel);
                            // </FS:Beq>
                            llassert(!name_to_match.empty());

                            std::string toAdd = getLodSuffix(searchLOD);

                            // <FS:Ansariel> Bug fixes in mesh importer by Drake Arconis
                            //if (name_to_match.find(toAdd) == -1)
                            if (name_to_match.find(toAdd) == std::string::npos)
                            // </FS:Ansariel>
                            {
                                name_to_match += toAdd;
                            }

                            // See if we can find an appropriately named model in LOD 'searchLOD'
                            //
                            FindModel(mScene[searchLOD], name_to_match, lod_model, transform);
                            searchLOD++;
                        }
                    }
                }
                else
                {
                    // Use old method of index-based association
                    U32 idx = 0;
                    for (idx = 0; idx < mBaseModel.size(); ++idx)
                    {
                        // find reference instance for this model
                        if (mBaseModel[idx] == base_model)
                        {
                            if (mImporterDebug)
                            {
                                std::ostringstream out;
                                out << "Attempting to use model index " << idx;
                                // <FS:Beq> better debug (watch for dangling single line else)
                                if (i==4)
                                {
                                out << " for PHYS";
                                }
                                else
                                // </FS:Beq>
                                out << " for LOD" << i;
                                out << " of " << instance.mLabel;
                                LL_INFOS() << out.str() << LL_ENDL;
                                LLFloaterModelPreview::addStringToLog(out, false);
                            }
                            break;
                        }
                    }

                    // If the model list for the current LOD includes that index...
                    //
                    if (mModel[i].size() > idx)
                    {
                        // Assign that index from the model list for our LOD as the LOD model for this instance
                        //
                        lod_model = mModel[i][idx];
                        if (mImporterDebug)
                        {
                            std::ostringstream out;
                            out << "Indexed match of model index " << idx << " at LOD " << i << " to model named " << lod_model->mLabel;
                            LL_INFOS() << out.str() << LL_ENDL;
                            LLFloaterModelPreview::addStringToLog(out, false);
                        }
                    }
                    else if (mImporterDebug)
                    {
                        std::ostringstream out;
                        out << "LOD" << i << ": List of models does not include index " << idx << " scene is missing a LOD model";
                        LL_INFOS() << out.str() << LL_ENDL;
                        LLFloaterModelPreview::addStringToLog(out, false);
                    }
                }
                if (mWarnOfUnmatchedPhyicsMeshes && !lod_model && (i == LLModel::LOD_PHYSICS))
                {
                    // Despite the various strategies above, if we don't now have a physics model, we're going to end up with decomposition.
                    // That's ok, but might not what they wanted. Use default_physics_shape if found.
                    std::ostringstream out;
                    out << "No physics model specified for " << instance.mLabel;
                    if (mDefaultPhysicsShapeP)
                    {
                        out << " - using: " << DEFAULT_PHYSICS_MESH_NAME;
                        lod_model = mDefaultPhysicsShapeP;
                    }
                    LL_WARNS() << out.str() << LL_ENDL;
                    LLFloaterModelPreview::addStringToLog(out, !mDefaultPhysicsShapeP); // Flash log tab if no default.
                }

                if (lod_model)
                {
                    if (mImporterDebug)
                    {
                        std::ostringstream out;
                        if (i == LLModel::LOD_PHYSICS)
                        {
                            out << "Assigning collision for " << instance.mLabel << " to match " << lod_model->mLabel;
                        }
                        else
                        {
                            out << "Assigning LOD" << i << " for " << instance.mLabel << " to found match " << lod_model->mLabel;
                        }
                        LL_INFOS() << out.str() << LL_ENDL;
                        LLFloaterModelPreview::addStringToLog(out, false);
                    }
                    instance.mLOD[i] = lod_model;
                }
                else
                {
                    if (i < LLModel::LOD_HIGH && !lodsReady())
                    {
                        // assign a placeholder from previous LOD until lod generation is complete.
                        // Note: we might need to assign it regardless of conditions like named search does, to prevent crashes.
                        instance.mLOD[i] = instance.mLOD[i + 1];
                    }
                    if (mImporterDebug)
                    {
                        std::ostringstream out;
                        out << "LOD" << i << ": List of models does not include " << instance.mLabel;
                        LL_INFOS() << out.str() << LL_ENDL;
                        LLFloaterModelPreview::addStringToLog(out, false);
                    }
                }
            }

            LLModel* high_lod_model = instance.mLOD[LLModel::LOD_HIGH];
            if (!high_lod_model)
            {
                LLFloaterModelPreview::addStringToLog("Model " + instance.mLabel + " has no High Lod (LOD3).", true);
                load_state = LLModelLoader::ERROR_HIGH_LOD_MODEL_MISSING; // <FS:Beq/> FIRE-30965 Cleanup braindead mesh parsing error handlers
                mFMP->childDisable("calculate_btn");
            }
            else
            {
                for (U32 i = 0; i < LLModel::NUM_LODS - 1; i++)
                {
                    int refFaceCnt = 0;
                    int modelFaceCnt = 0;
                    llassert(instance.mLOD[i]);
                    // <FS:Beq> Fix material matching algorithm to work as per design
                    // if (instance.mLOD[i] && !instance.mLOD[i]->matchMaterialOrder(high_lod_model, refFaceCnt, modelFaceCnt))
                    if (instance.mLOD[i] && !matchMaterialOrder(instance.mLOD[i],high_lod_model, refFaceCnt, modelFaceCnt))
                    // </FS:Beq>
                    {
                        LLFloaterModelPreview::addStringToLog("Model " + instance.mLabel + " has mismatching materials between lods." , true);
                        load_state = LLModelLoader::ERROR_MATERIALS_NOT_A_SUBSET; // <FS:Beq/> more descriptive errors
                        mFMP->childDisable("calculate_btn");
                    }
                }
                LLFloaterModelPreview* fmp = (LLFloaterModelPreview*)mFMP;
                bool upload_skinweights = fmp && fmp->childGetValue("upload_skin").asBoolean();
                if (upload_skinweights && high_lod_model->mSkinInfo.mJointNames.size() > 0)
                {
                    LLQuaternion bind_rot = LLSkinningUtil::getUnscaledQuaternion(LLMatrix4(high_lod_model->mSkinInfo.mBindShapeMatrix));
                    LLQuaternion identity;
                    if (!bind_rot.isEqualEps(identity, 0.01f))
                    {
                        // Bind shape matrix is not in standard X-forward orientation.
                        // Might be good idea to only show this once. It can be spammy.
                        std::ostringstream out;
                        out << "non-identity bind shape rot. mat is ";
                        out << high_lod_model->mSkinInfo.mBindShapeMatrix;
                        out << " bind_rot ";
                        out << bind_rot;
                        LL_WARNS() << out.str() << LL_ENDL;

                        LLFloaterModelPreview::addStringToLog(out, getLoadState() != LLModelLoader::WARNING_BIND_SHAPE_ORIENTATION);
                        load_state = LLModelLoader::WARNING_BIND_SHAPE_ORIENTATION;
                    }
                }
            }
            instance.mTransform = mat;
            mUploadData.push_back(instance);
        }
    }

    for (U32 lod = 0; lod < LLModel::NUM_LODS - 1; lod++)
    {
        // Search for models that are not included into upload data
        // If we found any, that means something we loaded is not a sub-model.
        for (U32 model_ind = 0; model_ind < mModel[lod].size(); ++model_ind)
        {
            bool found_model = false;
            for (LLMeshUploadThread::instance_list::iterator iter = mUploadData.begin(); iter != mUploadData.end(); ++iter)
            {
                LLModelInstance& instance = *iter;
                if (instance.mLOD[lod] == mModel[lod][model_ind])
                {
                    found_model = true;
                    break;
                }
            }
            if (!found_model && mModel[lod][model_ind] && !mModel[lod][model_ind]->mSubmodelID)
            {
                // <FS:Beq> this is not debug, this is an important/useful error advisory
                // if (mImporterDebug) 
                {
                    std::ostringstream out;
                    out << "Model " << mModel[lod][model_ind]->mLabel << " was not used - mismatching lod models.";
                    LL_INFOS() << out.str() << LL_ENDL;
                    LLFloaterModelPreview::addStringToLog(out, true);
                }
                load_state = LLModelLoader::ERROR_LOD_MODEL_MISMATCH;
                mFMP->childDisable("calculate_btn");
            }
        }
    }

    // Update state for notifications
    if (load_state > 0)
    {
        // encountered issues
        setLoadState(load_state);
    }
    // <FS:Beq> FIRE-30965 Cleanup braindead mesh parsing error handlers
    // else if (getLoadState() == LLModelLoader::ERROR_MATERIALS
        else if (getLoadState() == LLModelLoader::ERROR_MATERIALS_NOT_A_SUBSET 
             || getLoadState() == LLModelLoader::ERROR_HIGH_LOD_MODEL_MISSING
             || getLoadState() == LLModelLoader::ERROR_LOD_MODEL_MISMATCH
    // </FS:Beq>
             || getLoadState() == LLModelLoader::WARNING_BIND_SHAPE_ORIENTATION)
    {
        // This is only valid for these two error types because they are 
        // only used inside rebuildUploadData() and updateStatusMessages()
        // updateStatusMessages() is called after rebuildUploadData()
        setLoadState(LLModelLoader::DONE);
    }

    // <FS:AW> OpenSim limits
    //F32 max_import_scale = (DEFAULT_MAX_PRIM_SCALE - 0.1f) / max_scale;
    F32 region_max_prim_scale = LLWorld::getInstance()->getRegionMaxPrimScale();
    F32 max_import_scale = region_max_prim_scale/max_scale;
    // </FS:AW>

    F32 max_axis = llmax(mPreviewScale.mV[0], mPreviewScale.mV[1]);
    max_axis = llmax(max_axis, mPreviewScale.mV[2]);
    max_axis *= 2.f;

    //clamp scale so that total imported model bounding box is smaller than 240m on a side
    max_import_scale = llmin(max_import_scale, 240.f / max_axis);

    scale_spinner->setMaxValue(max_import_scale);

    if (max_import_scale < scale)
    {
        scale_spinner->setValue(max_import_scale);
    }

}

void LLModelPreview::saveUploadData(bool save_skinweights, bool save_joint_positions, bool lock_scale_if_joint_position)
{
    if (!mLODFile[LLModel::LOD_HIGH].empty())
    {
        std::string filename = mLODFile[LLModel::LOD_HIGH];
        std::string slm_filename;

        if (LLModelLoader::getSLMFilename(filename, slm_filename))
        {
            saveUploadData(slm_filename, save_skinweights, save_joint_positions, lock_scale_if_joint_position);
        }
    }
}

void LLModelPreview::saveUploadData(const std::string& filename,
    bool save_skinweights, bool save_joint_positions, bool lock_scale_if_joint_position)
{

    std::set<LLPointer<LLModel> > meshes;
    std::map<LLModel*, std::string> mesh_binary;

    LLModel::hull empty_hull;

    LLSD data;

    data["version"] = SLM_SUPPORTED_VERSION;
    if (!mBaseModel.empty())
    {
        data["name"] = mBaseModel[0]->getName();
    }

    S32 mesh_id = 0;

    //build list of unique models and initialize local id
    for (U32 i = 0; i < mUploadData.size(); ++i)
    {
        LLModelInstance& instance = mUploadData[i];

        if (meshes.find(instance.mModel) == meshes.end())
        {
            instance.mModel->mLocalID = mesh_id++;
            meshes.insert(instance.mModel);

            std::stringstream str;
            LLModel::Decomposition& decomp =
                instance.mLOD[LLModel::LOD_PHYSICS].notNull() ?
                instance.mLOD[LLModel::LOD_PHYSICS]->mPhysics :
                instance.mModel->mPhysics;

            LLModel::writeModel(str,
                instance.mLOD[LLModel::LOD_PHYSICS],
                instance.mLOD[LLModel::LOD_HIGH],
                instance.mLOD[LLModel::LOD_MEDIUM],
                instance.mLOD[LLModel::LOD_LOW],
                instance.mLOD[LLModel::LOD_IMPOSTOR],
                decomp,
                save_skinweights,
                save_joint_positions,
                lock_scale_if_joint_position,
                FALSE, TRUE, instance.mModel->mSubmodelID);

            data["mesh"][instance.mModel->mLocalID] = str.str();
        }

        data["instance"][i] = instance.asLLSD();
    }

    llofstream out(filename.c_str(), std::ios_base::out | std::ios_base::binary);
    LLSDSerialize::toBinary(data, out);
    out.flush();
    out.close();
}

void LLModelPreview::clearModel(S32 lod)
{
    if (lod < 0 || lod > LLModel::LOD_PHYSICS)
    {
        return;
    }

    mVertexBuffer[lod].clear();
    mModel[lod].clear();
    mScene[lod].clear();
}

void LLModelPreview::getJointAliases(JointMap& joint_map)
{
    // Get all standard skeleton joints from the preview avatar.
    LLVOAvatar *av = getPreviewAvatar();

    //Joint names and aliases come from avatar_skeleton.xml

    joint_map = av->getJointAliases();

    std::vector<std::string> cv_names, attach_names;
    av->getSortedJointNames(1, cv_names);
    av->getSortedJointNames(2, attach_names);
    for (std::vector<std::string>::iterator it = cv_names.begin(); it != cv_names.end(); ++it)
    {
        joint_map[*it] = *it;
    }
    for (std::vector<std::string>::iterator it = attach_names.begin(); it != attach_names.end(); ++it)
    {
        joint_map[*it] = *it;
    }
}

void LLModelPreview::loadModel(std::string filename, S32 lod, bool force_disable_slm)
{
    assert_main_thread();

    LLMutexLock lock(this);

    if (lod < LLModel::LOD_IMPOSTOR || lod > LLModel::NUM_LODS - 1)
    {
        std::ostringstream out;
        out << "Invalid level of detail: ";
        out << lod;
        LL_WARNS() << out.str() << LL_ENDL;
        LLFloaterModelPreview::addStringToLog(out, true);
        assert(lod >= LLModel::LOD_IMPOSTOR && lod < LLModel::NUM_LODS);
        return;
    }

    // This triggers if you bring up the file picker and then hit CANCEL.
    // Just use the previous model (if any) and ignore that you brought up
    // the file picker.

    if (filename.empty())
    {
        if (mBaseModel.empty())
        {
            // this is the initial file picking. Close the whole floater
            // if we don't have a base model to show for high LOD.
            mFMP->closeFloater(false);
        }
        mLoading = false;
        return;
    }

    if (mModelLoader)
    {
        LL_WARNS() << "Incompleted model load operation pending." << LL_ENDL;
        return;
    }

    mLODFile[lod] = filename;
    // <FS:Beq> Improved LOD generation
    if (lod == LLModel::LOD_HIGH)
    {
        clearGLODGroup();
    }
    // </FS:Beq>
    std::map<std::string, std::string> joint_alias_map;
    getJointAliases(joint_alias_map);

    std::array<std::string, LLModel::NUM_LODS> lod_suffix;
	for(int i=0; i < LLModel::NUM_LODS; i++)
	{
		lod_suffix[i] = gSavedSettings.getString(sSuffixVarNames[i]);
	}

    mModelLoader = new LLDAELoader(
        filename,
        lod,
        &LLModelPreview::loadedCallback,
        &LLModelPreview::lookupJointByName,
        &LLModelPreview::loadTextures,
        &LLModelPreview::stateChangedCallback,
        this,
        mJointTransformMap,
        mJointsFromNode,
        joint_alias_map,
        LLSkinningUtil::getMaxJointCount(),
        gSavedSettings.getU32("ImporterModelLimit"),
        // <FS:Beq> allow LOD suffix configuration
        // gSavedSettings.getBOOL("ImporterPreprocessDAE"));
        gSavedSettings.getBOOL("ImporterPreprocessDAE"),
        lod_suffix);

    if (force_disable_slm)
    {
        mModelLoader->mTrySLM = false;
    }
    else
    {
        // For MAINT-6647, we have set force_disable_slm to true,
        // which means this code path will never be taken. Trying to
        // re-use SLM files has never worked properly; in particular,
        // it tends to force the UI into strange checkbox options
        // which cannot be altered.

        //only try to load from slm if viewer is configured to do so and this is the 
        //initial model load (not an LoD or physics shape)
        mModelLoader->mTrySLM = gSavedSettings.getBOOL("MeshImportUseSLM") && mUploadData.empty();
    }
    mModelLoader->start();

    mFMP->childSetTextArg("status", "[STATUS]", mFMP->getString("status_reading_file"));

    setPreviewLOD(lod);

    if (getLoadState() >= LLModelLoader::ERROR_PARSING)
    {
        mFMP->childDisable("ok_btn");
        mFMP->childDisable("calculate_btn");
    }

    if (lod == mPreviewLOD)
    {
        mFMP->childSetValue("lod_file_" + lod_name[lod], mLODFile[lod]);
    }
    else if (lod == LLModel::LOD_PHYSICS)
    {
        mFMP->childSetValue("physics_file", mLODFile[lod]);
    }

    mFMP->openFloater();
}

void LLModelPreview::setPhysicsFromLOD(S32 lod)
{
    assert_main_thread();

    if (lod >= 0 && lod <= 3)
    {
        mPhysicsSearchLOD = lod;
        mModel[LLModel::LOD_PHYSICS] = mModel[lod];
        mScene[LLModel::LOD_PHYSICS] = mScene[lod];
        mLODFile[LLModel::LOD_PHYSICS].clear();
        mFMP->childSetValue("physics_file", mLODFile[LLModel::LOD_PHYSICS]);
        mVertexBuffer[LLModel::LOD_PHYSICS].clear();
        rebuildUploadData();
        refresh();
        updateStatusMessages();
    }
}
// <FS:Beq> FIRE-30963 - better physics defaults
void LLModelPreview::setPhysicsFromPreset(S32 preset)
{
    assert_main_thread();

    mPhysicsSearchLOD = -1;
    mLODFile[LLModel::LOD_PHYSICS].clear();
    mFMP->childSetValue("physics_file", mLODFile[LLModel::LOD_PHYSICS]);
    mVertexBuffer[LLModel::LOD_PHYSICS].clear();
    if(preset == 1)
    {
        mPhysicsSearchLOD = LLModel::LOD_PHYSICS;
        loadModel( gDirUtilp->getExpandedFilename(LL_PATH_FS_RESOURCES, "cube_phys.dae"), LLModel::LOD_PHYSICS);
    }
    else if(preset == 2)
    {
        mPhysicsSearchLOD = LLModel::LOD_PHYSICS;
        loadModel( gDirUtilp->getExpandedFilename(LL_PATH_FS_RESOURCES, "hex_phys.dae"), LLModel::LOD_PHYSICS);
    }
    else if(preset == 3)
    {
        auto ud_physics = gSavedSettings.getString("FSPhysicsPresetUser1");
        LL_INFOS() << "Loading User defined Physics Preset [" << ud_physics << "]" << LL_ENDL;
        if (ud_physics != "" && gDirUtilp->fileExists(ud_physics))
        {
            // loading physics from file
            mPhysicsSearchLOD = LLModel::LOD_PHYSICS;
            loadModel( gDirUtilp->getExpandedFilename(LL_PATH_NONE, gDirUtilp->getDirName(ud_physics), gDirUtilp->getBaseFileName(ud_physics, false)), LLModel::LOD_PHYSICS);
        }
    }
}
// </FS:Beq>
void LLModelPreview::clearIncompatible(S32 lod)
{
    //Don't discard models if specified model is the physic rep
    if (lod == LLModel::LOD_PHYSICS)
    {
        return;
    }

    // at this point we don't care about sub-models,
    // different amount of sub-models means face count mismatch, not incompatibility
    U32 lod_size = countRootModels(mModel[lod]);
    bool replaced_base_model = (lod == LLModel::LOD_HIGH);
    for (U32 i = 0; i <= LLModel::LOD_HIGH; i++)
    {
        // Clear out any entries that aren't compatible with this model
        if (i != lod)
        {
            if (countRootModels(mModel[i]) != lod_size)
            {
                mModel[i].clear();
                mScene[i].clear();
                mVertexBuffer[i].clear();

                if (i == LLModel::LOD_HIGH)
                {
                    mBaseModel = mModel[lod];
                    mBaseScene = mScene[lod];
                    mVertexBuffer[5].clear();
                    replaced_base_model = true;

                    clearGLODGroup(); // <FS:Beq/> Improved LOD generation
                }
            }
        }
    }

    if (replaced_base_model && !mGenLOD)
    {
        // In case base was replaced, we might need to restart generation

        // Check if already started
        bool subscribe_for_generation = mLodsQuery.empty();
        
        // Remove previously scheduled work
        mLodsQuery.clear();

        LLFloaterModelPreview* fmp = LLFloaterModelPreview::sInstance;
        if (!fmp) return;

        // Schedule new work
        for (S32 i = LLModel::LOD_HIGH; i >= 0; --i)
        {
            if (mModel[i].empty())
            {
                // Base model was replaced, regenerate this lod if applicable
                LLComboBox* lod_combo = mFMP->findChild<LLComboBox>("lod_source_" + lod_name[i]);
                if (!lod_combo) return;

                S32 lod_mode = lod_combo->getCurrentIndex();
                if (lod_mode != LOD_FROM_FILE)
                {
                    mLodsQuery.push_back(i);
                }
            }
        }

        // Subscribe if we have pending work and not subscribed yet
        if (!mLodsQuery.empty() && subscribe_for_generation)
        {
            doOnIdleRepeating(lodQueryCallback);
        }
    }
}

// <FS:Beq> Improved LOD generation
void LLModelPreview::clearGLODGroup()
{
    if (mGroup)
    {
        for (std::map<LLPointer<LLModel>, U32>::iterator iter = mObject.begin(); iter != mObject.end(); ++iter)
        {
            glodDeleteObject(iter->second);
            stop_gloderror();
        }
        mObject.clear();

        glodDeleteGroup(mGroup);
        stop_gloderror();
        mGroup = 0;
    }
}
// </FS:Beq>
void LLModelPreview::loadModelCallback(S32 loaded_lod)
{
    assert_main_thread();

    LLMutexLock lock(this);
    if (!mModelLoader)
    {
        mLoading = false;
        return;
    }
    if (getLoadState() >= LLModelLoader::ERROR_PARSING)
    {
        mLoading = false;
        mModelLoader = NULL;
        mLodsWithParsingError.push_back(loaded_lod);
        return;
    }

    mLodsWithParsingError.erase(std::remove(mLodsWithParsingError.begin(), mLodsWithParsingError.end(), loaded_lod), mLodsWithParsingError.end());
    if (mLodsWithParsingError.empty())
    {
        mFMP->childEnable("calculate_btn");
    }

    // Copy determinations about rig so UI will reflect them
    //
    setRigValidForJointPositionUpload(mModelLoader->isRigValidForJointPositionUpload());
    setLegacyRigFlags(mModelLoader->getLegacyRigFlags());

    mModelLoader->loadTextures();

    if (loaded_lod == -1)
    { //populate all LoDs from model loader scene
        mBaseModel.clear();
        mBaseScene.clear();

        bool skin_weights = false;
        bool joint_overrides = false;
        bool lock_scale_if_joint_position = false;

        for (S32 lod = 0; lod < LLModel::NUM_LODS; ++lod)
        { //for each LoD

            //clear scene and model info
            mScene[lod].clear();
            mModel[lod].clear();
            mVertexBuffer[lod].clear();

            if (mModelLoader->mScene.begin()->second[0].mLOD[lod].notNull())
            { //if this LoD exists in the loaded scene

                //copy scene to current LoD
                mScene[lod] = mModelLoader->mScene;

                //touch up copied scene to look like current LoD
                for (LLModelLoader::scene::iterator iter = mScene[lod].begin(); iter != mScene[lod].end(); ++iter)
                {
                    LLModelLoader::model_instance_list& list = iter->second;

                    for (LLModelLoader::model_instance_list::iterator list_iter = list.begin(); list_iter != list.end(); ++list_iter)
                    {
                        //override displayed model with current LoD
                        list_iter->mModel = list_iter->mLOD[lod];

                        if (!list_iter->mModel)
                        {
                            continue;
                        }

                        //add current model to current LoD's model list (LLModel::mLocalID makes a good vector index)
                        S32 idx = list_iter->mModel->mLocalID;

                        if (mModel[lod].size() <= idx)
                        { //stretch model list to fit model at given index
                            mModel[lod].resize(idx + 1);
                        }

                        mModel[lod][idx] = list_iter->mModel;
                        if (!list_iter->mModel->mSkinWeights.empty())
                        {
                            skin_weights = true;

                            if (!list_iter->mModel->mSkinInfo.mAlternateBindMatrix.empty())
                            {
                                joint_overrides = true;
                            }
                            if (list_iter->mModel->mSkinInfo.mLockScaleIfJointPosition)
                            {
                                lock_scale_if_joint_position = true;
                            }
                        }
                    }
                }
            }
        }

        if (mFMP)
        {
            LLFloaterModelPreview* fmp = (LLFloaterModelPreview*)mFMP;

            if (skin_weights)
            { //enable uploading/previewing of skin weights if present in .slm file
                fmp->enableViewOption("show_skin_weight");
                mViewOption["show_skin_weight"] = true;
                fmp->childSetValue("upload_skin", true);
            }

            if (joint_overrides)
            {
                fmp->enableViewOption("show_joint_overrides");
                mViewOption["show_joint_overrides"] = true;
                fmp->childSetValue("show_joint_overrides", true); // <FS:Beq> make sure option appears checked, when value is being forced true
                fmp->enableViewOption("show_joint_positions");
                fmp->childSetValue("show_joint_positions", true); // <FS:Beq> make sure option appears checked, when value is being forced true
                mViewOption["show_joint_positions"] = true;
                fmp->childSetValue("upload_joints", true);
            }
            else
            {
                fmp->clearAvatarTab();
            }

            if (lock_scale_if_joint_position)
            {
                fmp->enableViewOption("lock_scale_if_joint_position");
                mViewOption["lock_scale_if_joint_position"] = true;
                fmp->childSetValue("lock_scale_if_joint_position", true);
            }
        }

        //copy high lod to base scene for LoD generation
        mBaseScene = mScene[LLModel::LOD_HIGH];
        mBaseModel = mModel[LLModel::LOD_HIGH];

        mDirty = true;
        resetPreviewTarget();
    }
    else
    { //only replace given LoD
        mModel[loaded_lod] = mModelLoader->mModelList;
        mScene[loaded_lod] = mModelLoader->mScene;
        mVertexBuffer[loaded_lod].clear();

        setPreviewLOD(loaded_lod);

        if (loaded_lod == LLModel::LOD_HIGH)
        { //save a copy of the highest LOD for automatic LOD manipulation
            if (mBaseModel.empty())
            { //first time we've loaded a model, auto-gen LoD
                mGenLOD = true;
            }

            mBaseModel = mModel[loaded_lod];
            clearGLODGroup(); // <FS:Beq/> Improved LOD generation

            mBaseScene = mScene[loaded_lod];
            mVertexBuffer[5].clear();
        }
        else
        {
            if (loaded_lod == LLModel::LOD_PHYSICS)
            {   // Explicitly loading physics. See if there is a default mesh.
                LLMatrix4 ignored_transform; // Each mesh that uses this will supply their own.
                mDefaultPhysicsShapeP = nullptr;
                FindModel(mScene[loaded_lod], DEFAULT_PHYSICS_MESH_NAME + getLodSuffix(loaded_lod), mDefaultPhysicsShapeP, ignored_transform);
                mWarnOfUnmatchedPhyicsMeshes = true;
            }
            BOOL legacyMatching = gSavedSettings.getBOOL("ImporterLegacyMatching");
            if (!legacyMatching)
            {
                if (!mBaseModel.empty())
                {
                    BOOL name_based = FALSE;
                    BOOL has_submodels = FALSE;
                    for (U32 idx = 0; idx < mBaseModel.size(); ++idx)
                    {
                        if (mBaseModel[idx]->mSubmodelID)
                        { // don't do index-based renaming when the base model has submodels
                            has_submodels = TRUE;
                            if (mImporterDebug)
                            {
                                std::ostringstream out;
                                out << "High LOD has submodels";
                                LL_INFOS() << out.str() << LL_ENDL;
                                LLFloaterModelPreview::addStringToLog(out, false);
                            }
                            break;
                        }
                    }

                    for (U32 idx = 0; idx < mModel[loaded_lod].size(); ++idx)
                    {
                        std::string loaded_name = stripSuffix(mModel[loaded_lod][idx]->mLabel);

                        LLModel* found_model = NULL;
                        LLMatrix4 transform;
                        FindModel(mBaseScene, loaded_name, found_model, transform);
                        if (found_model)
                        { // don't rename correctly named models (even if they are placed in a wrong order)
                            name_based = TRUE;
                        }

                        if (mModel[loaded_lod][idx]->mSubmodelID)
                        { // don't rename the models when loaded LOD model has submodels
                            has_submodels = TRUE;
                        }
                    }

                    if (mImporterDebug)
                    {
                        std::ostringstream out;
                        out << "Loaded LOD " << loaded_lod << ": correct names" << (name_based ? "" : "NOT ") << "found; submodels " << (has_submodels ? "" : "NOT ") << "found";
                        LL_INFOS() << out.str() << LL_ENDL;
                        LLFloaterModelPreview::addStringToLog(out, false);
                    }

                    if (!name_based && !has_submodels)
                    { // replace the name of the model loaded for any non-HIGH LOD to match the others (MAINT-5601)
                        // this actually works like "ImporterLegacyMatching" for this particular LOD
                        for (U32 idx = 0; idx < mModel[loaded_lod].size() && idx < mBaseModel.size(); ++idx)
                        {
                            std::string name = stripSuffix(mBaseModel[idx]->mLabel);
                            std::string loaded_name = stripSuffix(mModel[loaded_lod][idx]->mLabel);

                            if (loaded_name != name)
                            {
                                name += getLodSuffix(loaded_lod);

                                if (mImporterDebug)
                                {
                                    std::ostringstream out;
                                    out << "Loded model name " << mModel[loaded_lod][idx]->mLabel;
                                    out << " for LOD " << loaded_lod;
                                    out << " doesn't match the base model. Renaming to " << name;
                                    LL_WARNS() << out.str() << LL_ENDL;
                                    LLFloaterModelPreview::addStringToLog(out, false);
                                }
                                mModel[loaded_lod][idx]->mLabel = name;
                            }
                        }
                    }
                }
            }
        }

        clearIncompatible(loaded_lod);

        mDirty = true;

        if (loaded_lod == LLModel::LOD_HIGH)
        {
            resetPreviewTarget();
        }
    }

    mLoading = false;
    if (mFMP)
    {
        if (!mBaseModel.empty())
        {
            const std::string& model_name = mBaseModel[0]->getName();
            LLLineEditor* description_form = mFMP->getChild<LLLineEditor>("description_form");
            if (description_form->getText().empty())
            {
                description_form->setText(model_name);
            }
            // Add info to log that loading is complete (purpose: separator between loading and other logs)
            LLSD args;
            args["MODEL_NAME"] = model_name; // Teoretically shouldn't be empty, but might be better idea to add filename here
            LLFloaterModelPreview::addStringToLog("ModelLoaded", args, false, loaded_lod);
        }
    }
    refresh();

    mModelLoadedSignal();

    mModelLoader = NULL;
}

void LLModelPreview::resetPreviewTarget()
{
    if (mModelLoader)
    {
        mPreviewTarget = (mModelLoader->mExtents[0] + mModelLoader->mExtents[1]) * 0.5f;
        mPreviewScale = (mModelLoader->mExtents[1] - mModelLoader->mExtents[0]) * 0.5f;
    }

    setPreviewTarget(mPreviewScale.magVec()*10.f);
}

void LLModelPreview::generateNormals()
{
    assert_main_thread();

    S32 which_lod = mPreviewLOD;

    if (which_lod > 4 || which_lod < 0 ||
        mModel[which_lod].empty())
    {
        return;
    }

    F32 angle_cutoff = mFMP->childGetValue("crease_angle").asReal();

    mRequestedCreaseAngle[which_lod] = angle_cutoff;

    angle_cutoff *= DEG_TO_RAD;

    if (which_lod == 3 && !mBaseModel.empty())
    {
        if (mBaseModelFacesCopy.empty())
        {
            mBaseModelFacesCopy.reserve(mBaseModel.size());
            for (LLModelLoader::model_list::iterator it = mBaseModel.begin(), itE = mBaseModel.end(); it != itE; ++it)
            {
                v_LLVolumeFace_t faces;
                (*it)->copyFacesTo(faces);
                mBaseModelFacesCopy.push_back(faces);
            }
        }

        for (LLModelLoader::model_list::iterator it = mBaseModel.begin(), itE = mBaseModel.end(); it != itE; ++it)
        {
            (*it)->generateNormals(angle_cutoff);
        }

        mVertexBuffer[5].clear();
    }

    bool perform_copy = mModelFacesCopy[which_lod].empty();
    if (perform_copy) {
        mModelFacesCopy[which_lod].reserve(mModel[which_lod].size());
    }

    for (LLModelLoader::model_list::iterator it = mModel[which_lod].begin(), itE = mModel[which_lod].end(); it != itE; ++it)
    {
        if (perform_copy)
        {
            v_LLVolumeFace_t faces;
            (*it)->copyFacesTo(faces);
            mModelFacesCopy[which_lod].push_back(faces);
        }

        (*it)->generateNormals(angle_cutoff);
    }

    mVertexBuffer[which_lod].clear();
    refresh();
    updateStatusMessages();
}

void LLModelPreview::restoreNormals()
{
    S32 which_lod = mPreviewLOD;

    if (which_lod > 4 || which_lod < 0 ||
        mModel[which_lod].empty())
    {
        return;
    }

    if (!mBaseModelFacesCopy.empty())
    {
        llassert(mBaseModelFacesCopy.size() == mBaseModel.size());

        vv_LLVolumeFace_t::const_iterator itF = mBaseModelFacesCopy.begin();
        for (LLModelLoader::model_list::iterator it = mBaseModel.begin(), itE = mBaseModel.end(); it != itE; ++it, ++itF)
        {
            (*it)->copyFacesFrom((*itF));
        }

        mBaseModelFacesCopy.clear();
    }

    if (!mModelFacesCopy[which_lod].empty())
    {
        vv_LLVolumeFace_t::const_iterator itF = mModelFacesCopy[which_lod].begin();
        for (LLModelLoader::model_list::iterator it = mModel[which_lod].begin(), itE = mModel[which_lod].end(); it != itE; ++it, ++itF)
        {
            (*it)->copyFacesFrom((*itF));
        }

        mModelFacesCopy[which_lod].clear();
    }

    mVertexBuffer[which_lod].clear();
    refresh();
    updateStatusMessages();
}

// <FS:Beq> Improved LOD generation
// Restore the GLOD entry point. 
// There would appear to be quite a lot of commonality which would be well suited to refactoring but
// LL are still playing with Mesh Optimiser code.
void LLModelPreview::genGlodLODs(S32 which_lod, U32 decimation, bool enforce_tri_limit)
{
    // Allow LoD from -1 to LLModel::LOD_PHYSICS
    if (which_lod < -1 || which_lod > LLModel::NUM_LODS - 1)
    {
        std::ostringstream out;
        out << "Invalid level of detail: " << which_lod;
        LL_WARNS() << out.str() << LL_ENDL;
        LLFloaterModelPreview::addStringToLog(out, false);
        assert(which_lod >= -1 && which_lod < LLModel::NUM_LODS);
        return;
    }

    if (mBaseModel.empty())
    {
        return;
    }

    LLVertexBuffer::unbind();

    LLGLSLShader* shader = LLGLSLShader::sCurBoundShaderPtr;

    if (shader)
    {
        shader->unbind();
    }

    stop_gloderror();
    static U32 cur_name = 1;

    S32 limit = -1;

    U32 triangle_count = 0;

    U32 instanced_triangle_count = 0;

    //get the triangle count for the whole scene
    for (LLModelLoader::scene::iterator iter = mBaseScene.begin(), endIter = mBaseScene.end(); iter != endIter; ++iter)
    {
        for (LLModelLoader::model_instance_list::iterator instance = iter->second.begin(), end_instance = iter->second.end(); instance != end_instance; ++instance)
        {
            LLModel* mdl = instance->mModel;
            if (mdl)
            {
                instanced_triangle_count += mdl->getNumTriangles();
            }
        }
    }

    //get the triangle count for the non-instanced set of models
    for (U32 i = 0; i < mBaseModel.size(); ++i)
    {
        triangle_count += mBaseModel[i]->getNumTriangles();
    }

    //get ratio of uninstanced triangles to instanced triangles
    F32 triangle_ratio = (F32)triangle_count / (F32)instanced_triangle_count;

    U32 base_triangle_count = triangle_count;

    U32 type_mask = LLVertexBuffer::MAP_VERTEX | LLVertexBuffer::MAP_NORMAL | LLVertexBuffer::MAP_TEXCOORD0;

    U32 lod_mode = 0;

    F32 lod_error_threshold = 0;

    // The LoD should be in range from Lowest to High
    if (which_lod > -1 && which_lod < NUM_LOD)
    {
        LLCtrlSelectionInterface* iface = mFMP->childGetSelectionInterface("lod_mode_" + lod_name[which_lod]);
        if (iface)
        {
            lod_mode = iface->getFirstSelectedIndex();
        }

        lod_error_threshold = mFMP->childGetValue("lod_error_threshold_" + lod_name[which_lod]).asReal();
    }

    if (which_lod != -1)
    {
        mRequestedLoDMode[which_lod] = lod_mode;
    }

    if (lod_mode == 0)
    {
        lod_mode = GLOD_TRIANGLE_BUDGET;

        // The LoD should be in range from Lowest to High
        if (which_lod > -1 && which_lod < NUM_LOD)
        {
            limit = mFMP->childGetValue("lod_triangle_limit_" + lod_name[which_lod]).asInteger();
            //convert from "scene wide" to "non-instanced" triangle limit
            limit = (S32)((F32)limit*triangle_ratio);
        }
    }
    else
    {
        lod_mode = GLOD_ERROR_THRESHOLD;
    }

    bool object_dirty = false;

    if (mGroup == 0)
    {
        object_dirty = true;
        mGroup = cur_name++;
        glodNewGroup(mGroup);
    }

    if (object_dirty)
    {
        for (LLModelLoader::model_list::iterator iter = mBaseModel.begin(); iter != mBaseModel.end(); ++iter)
        { //build GLOD objects for each model in base model list
            LLModel* mdl = *iter;

            if (mObject[mdl] != 0)
            {
                glodDeleteObject(mObject[mdl]);
            }

            mObject[mdl] = cur_name++;

            glodNewObject(mObject[mdl], mGroup, GLOD_DISCRETE);
            stop_gloderror();

            if (iter == mBaseModel.begin() && !mdl->mSkinWeights.empty())
            { //regenerate vertex buffer for skinned models to prevent animation feedback during LOD generation
                mVertexBuffer[5].clear();
            }

            if (mVertexBuffer[5].empty())
            {
                genBuffers(5, false);
            }

            U32 tri_count = 0;
            for (U32 i = 0; i < mVertexBuffer[5][mdl].size(); ++i)
            {
                LLVertexBuffer* buff = mVertexBuffer[5][mdl][i];
                buff->setBuffer(type_mask & buff->getTypeMask());

                U32 num_indices = mVertexBuffer[5][mdl][i]->getNumIndices();
                if (num_indices > 2)
                {
                    // <FS:ND> Fix glod so it works when just using the opengl core profile
                    //glodInsertElements(mObject[mdl], i, GL_TRIANGLES, num_indices, GL_UNSIGNED_SHORT, (U8*)mVertexBuffer[5][mdl][i]->getIndicesPointer(), 0, 0.f);
                    LLStrider<LLVector3> vertex_strider;
                    LLStrider<LLVector3> normal_strider;
                    LLStrider<LLVector2> tc_strider;

                    LLStrider< U16 > index_strider;
                    buff->getIndexStrider( index_strider );

                    glodVBO vbo = {};

                    if( buff->hasDataType( LLVertexBuffer::TYPE_VERTEX ) )
                    {
                        buff->getVertexStrider( vertex_strider );
                        vbo.mV.p = vertex_strider.get();
                        vbo.mV.size = 3;
                        vbo.mV.stride = LLVertexBuffer::sTypeSize[ LLVertexBuffer::TYPE_VERTEX ];
                        vbo.mV.type = GL_FLOAT;
                    }
                    if( buff->hasDataType( LLVertexBuffer::TYPE_NORMAL ) )
                    {
                        buff->getNormalStrider( normal_strider );
                        vbo.mN.p = normal_strider.get();
                        vbo.mN.stride = LLVertexBuffer::sTypeSize[ LLVertexBuffer::TYPE_NORMAL ];
                        vbo.mN.type = GL_FLOAT;
                    }
                    if( buff->hasDataType( LLVertexBuffer::TYPE_TEXCOORD0 ) )
                    {
                        buff->getTexCoord0Strider( tc_strider );
                        vbo.mT.p = tc_strider.get();
                        vbo.mT.size = 2;
                        vbo.mT.stride = LLVertexBuffer::sTypeSize[ LLVertexBuffer::TYPE_TEXCOORD0 ];
                        vbo.mT.type = GL_FLOAT;
                    }

                    glodInsertElements( mObject[ mdl ], i, GL_TRIANGLES, num_indices, GL_UNSIGNED_SHORT, (U8*)index_strider.get(), 0, 0.f, &vbo );
                    // </FS:ND>
                }
                tri_count += num_indices / 3;
                stop_gloderror();
            }

            glodBuildObject(mObject[mdl]);
            stop_gloderror();
        }
    }


    S32 start = LLModel::LOD_HIGH;
    S32 end = 0;

    if (which_lod != -1)
    {
        start = end = which_lod;
    }

    mMaxTriangleLimit = base_triangle_count;

    for (S32 lod = start; lod >= end; --lod)
    {
        if (which_lod == -1)
        {
            if (lod < start)
            {
                triangle_count /= decimation;
            }
        }
        else
        {
            if (enforce_tri_limit)
            {
                triangle_count = limit;
            }
            else
            {
                for (S32 j = LLModel::LOD_HIGH; j>which_lod; --j)
                {
                    triangle_count /= decimation;
                }
            }
        }

        mModel[lod].clear();
        mModel[lod].resize(mBaseModel.size());
        mVertexBuffer[lod].clear();

        U32 actual_tris = 0;
        U32 actual_verts = 0;
        U32 submeshes = 0;

        mRequestedTriangleCount[lod] = (S32)((F32)triangle_count / triangle_ratio);
        mRequestedErrorThreshold[lod] = lod_error_threshold;

        glodGroupParameteri(mGroup, GLOD_ADAPT_MODE, lod_mode);
        stop_gloderror();

        glodGroupParameteri(mGroup, GLOD_ERROR_MODE, GLOD_OBJECT_SPACE_ERROR);
        stop_gloderror();

        glodGroupParameterf(mGroup, GLOD_OBJECT_SPACE_ERROR_THRESHOLD, lod_error_threshold);
        stop_gloderror();

        if (lod_mode != GLOD_TRIANGLE_BUDGET)
        {
            glodGroupParameteri(mGroup, GLOD_MAX_TRIANGLES, 0);
        }
        else
        {
            //SH-632: always add 1 to desired amount to avoid decimating below desired amount
            glodGroupParameteri(mGroup, GLOD_MAX_TRIANGLES, triangle_count + 1);
        }

        stop_gloderror();
        glodAdaptGroup(mGroup);
        stop_gloderror();

        for (U32 mdl_idx = 0; mdl_idx < mBaseModel.size(); ++mdl_idx)
        {
            LLModel* base = mBaseModel[mdl_idx];

            GLint patch_count = 0;
            glodGetObjectParameteriv(mObject[base], GLOD_NUM_PATCHES, &patch_count);
            stop_gloderror();

            LLVolumeParams volume_params;
            volume_params.setType(LL_PCODE_PROFILE_SQUARE, LL_PCODE_PATH_LINE);
            mModel[lod][mdl_idx] = new LLModel(volume_params, 0.f);

            std::string name = base->mLabel + getLodSuffix(lod);

            mModel[lod][mdl_idx]->mLabel = name;
            mModel[lod][mdl_idx]->mSubmodelID = base->mSubmodelID;

            GLint* sizes = new GLint[patch_count * 2];
            glodGetObjectParameteriv(mObject[base], GLOD_PATCH_SIZES, sizes);
            stop_gloderror();

            GLint* names = new GLint[patch_count];
            glodGetObjectParameteriv(mObject[base], GLOD_PATCH_NAMES, names);
            stop_gloderror();

            mModel[lod][mdl_idx]->setNumVolumeFaces(patch_count);

            LLModel* target_model = mModel[lod][mdl_idx];

            for (GLint i = 0; i < patch_count; ++i)
            {
                type_mask = mVertexBuffer[5][base][i]->getTypeMask();

                LLPointer<LLVertexBuffer> buff = new LLVertexBuffer(type_mask, 0);

                if (sizes[i * 2 + 1] > 0 && sizes[i * 2] > 0)
                {
                    if (!buff->allocateBuffer(sizes[i * 2 + 1], sizes[i * 2], true))
                    {
                        // Todo: find a way to stop preview in this case instead of crashing
                        LL_ERRS() << "Failed buffer allocation during preview LOD generation."
                            << " Vertices: " << sizes[i * 2 + 1]
                            << " Indices: " << sizes[i * 2] << LL_ENDL;
                    }
                    buff->setBuffer(type_mask);
                    // <FS:ND> Fix glod so it works when just using the opengl core profile
                    //glodFillElements(mObject[base], names[i], GL_UNSIGNED_SHORT, (U8*)buff->getIndicesPointer());
                    LLStrider<LLVector3> vertex_strider;
                    LLStrider<LLVector3> normal_strider;
                    LLStrider<LLVector2> tc_strider;

                    LLStrider< U16 > index_strider;
                    buff->getIndexStrider( index_strider );

                    glodVBO vbo = {};

                    if( buff->hasDataType( LLVertexBuffer::TYPE_VERTEX ) )
                    {
                        buff->getVertexStrider( vertex_strider );
                        vbo.mV.p = vertex_strider.get();
                        vbo.mV.size = 3;
                        vbo.mV.stride = LLVertexBuffer::sTypeSize[ LLVertexBuffer::TYPE_VERTEX ];
                        vbo.mV.type = GL_FLOAT;
                    }
                    if( buff->hasDataType( LLVertexBuffer::TYPE_NORMAL ) )
                    {
                        buff->getNormalStrider( normal_strider );
                        vbo.mN.p = normal_strider.get();
                        vbo.mN.stride = LLVertexBuffer::sTypeSize[ LLVertexBuffer::TYPE_NORMAL ];
                        vbo.mN.type = GL_FLOAT;
                    }
                    if( buff->hasDataType( LLVertexBuffer::TYPE_TEXCOORD0 ) )
                    {
                        buff->getTexCoord0Strider( tc_strider );
                        vbo.mT.p = tc_strider.get();
                        vbo.mT.size = 2;
                        vbo.mT.stride = LLVertexBuffer::sTypeSize[ LLVertexBuffer::TYPE_TEXCOORD0 ];
                        vbo.mT.type = GL_FLOAT;
                    }

                    glodFillElements( mObject[ base ], names[ i ], GL_UNSIGNED_SHORT, (U8*)index_strider.get(), &vbo );
                    // </FS:ND>
                    stop_gloderror();
                }
                else
                {
                    // This face was eliminated or we failed to allocate buffer,
                    // attempt to create a dummy triangle (one vertex, 3 indices, all 0)
                    buff->allocateBuffer(1, 3, true);
                    memset((U8*)buff->getMappedData(), 0, buff->getSize());
                    // <FS:ND> Fix when running with opengl core profile
                    //memset((U8*)buff->getIndicesPointer(), 0, buff->getIndicesSize());
                    LLStrider< U16 > index_strider;
                    buff->getIndexStrider( index_strider );

                    memset( (U8*)index_strider.get(), 0, buff->getIndicesSize() );
                    // </FS:ND>
                }

                buff->validateRange(0, buff->getNumVerts() - 1, buff->getNumIndices(), 0);

                LLStrider<LLVector3> pos;
                LLStrider<LLVector3> norm;
                LLStrider<LLVector2> tc;
                LLStrider<U16> index;

                buff->getVertexStrider(pos);
                if (type_mask & LLVertexBuffer::MAP_NORMAL)
                {
                    buff->getNormalStrider(norm);
                }
                if (type_mask & LLVertexBuffer::MAP_TEXCOORD0)
                {
                    buff->getTexCoord0Strider(tc);
                }

                buff->getIndexStrider(index);

                target_model->setVolumeFaceData(names[i], pos, norm, tc, index, buff->getNumVerts(), buff->getNumIndices());
                actual_tris += buff->getNumIndices() / 3;
                actual_verts += buff->getNumVerts();
                ++submeshes;

                if (!validate_face(target_model->getVolumeFace(names[i])))
                {
                    std::ostringstream out;
                    out << "Invalid face generated during LOD generation.";
                    LLFloaterModelPreview::addStringToLog(out,true);
                    LL_ERRS() << out.str() << LL_ENDL;
                }
            }

            //blind copy skin weights and just take closest skin weight to point on
            //decimated mesh for now (auto-generating LODs with skin weights is still a bit
            //of an open problem).
            target_model->mPosition = base->mPosition;
            target_model->mSkinWeights = base->mSkinWeights;
            target_model->mSkinInfo = base->mSkinInfo;
            //copy material list
            target_model->mMaterialList = base->mMaterialList;

            if (!validate_model(target_model))
            {
                LL_ERRS() << "Invalid model generated when creating LODs" << LL_ENDL;
            }

            delete[] sizes;
            delete[] names;
        }

        //rebuild scene based on mBaseScene
        mScene[lod].clear();
        mScene[lod] = mBaseScene;

        for (U32 i = 0; i < mBaseModel.size(); ++i)
        {
            LLModel* mdl = mBaseModel[i];
            LLModel* target = mModel[lod][i];
            if (target)
            {
                for (LLModelLoader::scene::iterator iter = mScene[lod].begin(); iter != mScene[lod].end(); ++iter)
                {
                    for (U32 j = 0; j < iter->second.size(); ++j)
                    {
                        if (iter->second[j].mModel == mdl)
                        {
                            iter->second[j].mModel = target;
                        }
                    }
                }
            }
        }
    }

    LLVertexBuffer::unbind();
    if (shader)
    {
        shader->bind();
    }
    refresh(); // <FS:ND/> refresh once to make sure render gets called with the updated vbos
}
// </FS:Beq>

// Runs per object, but likely it is a better way to run per model+submodels
// returns a ratio of base model indices to resulting indices
// returns -1 in case of failure
F32 LLModelPreview::genMeshOptimizerPerModel(LLModel *base_model, LLModel *target_model, F32 indices_decimator, F32 error_threshold, eSimplificationMode simplification_mode)
{
    // I. Weld faces together
    // Figure out buffer size
    S32 size_indices = 0;
    S32 size_vertices = 0;

    for (U32 face_idx = 0; face_idx < base_model->getNumVolumeFaces(); ++face_idx)
    {
        const LLVolumeFace &face = base_model->getVolumeFace(face_idx);
        size_indices += face.mNumIndices;
        size_vertices += face.mNumVertices;
    }

    if (size_indices < 3)
    {
        return -1;
    }

    // Allocate buffers, note that we are using U32 buffer instead of U16
    U32* combined_indices = (U32*)ll_aligned_malloc_32(size_indices * sizeof(U32));
    U32* output_indices = (U32*)ll_aligned_malloc_32(size_indices * sizeof(U32));

    // extra space for normals and text coords
    S32 tc_bytes_size = ((size_vertices * sizeof(LLVector2)) + 0xF) & ~0xF;
    LLVector4a* combined_positions = (LLVector4a*)ll_aligned_malloc<64>(sizeof(LLVector4a) * 2 * size_vertices + tc_bytes_size);
    LLVector4a* combined_normals = combined_positions + size_vertices;
    LLVector2* combined_tex_coords = (LLVector2*)(combined_normals + size_vertices);

    // copy indices and vertices into new buffers
    S32 combined_positions_shift = 0;
    S32 indices_idx_shift = 0;
    S32 combined_indices_shift = 0;
    for (U32 face_idx = 0; face_idx < base_model->getNumVolumeFaces(); ++face_idx)
    {
        const LLVolumeFace &face = base_model->getVolumeFace(face_idx);

        // Vertices
        S32 copy_bytes = face.mNumVertices * sizeof(LLVector4a);
        LLVector4a::memcpyNonAliased16((F32*)(combined_positions + combined_positions_shift), (F32*)face.mPositions, copy_bytes);

        // Normals
        LLVector4a::memcpyNonAliased16((F32*)(combined_normals + combined_positions_shift), (F32*)face.mNormals, copy_bytes);

        // Tex coords
        copy_bytes = face.mNumVertices * sizeof(LLVector2);
        memcpy((void*)(combined_tex_coords + combined_positions_shift), (void*)face.mTexCoords, copy_bytes);

        combined_positions_shift += face.mNumVertices;

        // Indices
        // Sadly can't do dumb memcpy for indices, need to adjust each value
        for (S32 i = 0; i < face.mNumIndices; ++i)
        {
            U16 idx = face.mIndices[i];

            combined_indices[combined_indices_shift] = idx + indices_idx_shift;
            combined_indices_shift++;
        }
        indices_idx_shift += face.mNumVertices;
    }

    // II. Generate a shadow buffer if nessesary.
    // Welds together vertices if possible

    U32* shadow_indices = NULL;
    // if MESH_OPTIMIZER_FULL, just leave as is, since generateShadowIndexBufferU32
    // won't do anything new, model was remaped on a per face basis.
    // Similar for MESH_OPTIMIZER_NO_TOPOLOGY, it's pointless
    // since 'simplifySloppy' ignores all topology, including normals and uvs.
    // Note: simplifySloppy can affect UVs significantly.
    if (simplification_mode == MESH_OPTIMIZER_NO_NORMALS)
    {
        // strip normals, reflections should restore relatively correctly
        shadow_indices = (U32*)ll_aligned_malloc_32(size_indices * sizeof(U32));
        LLMeshOptimizer::generateShadowIndexBufferU32(shadow_indices, combined_indices, size_indices, combined_positions, NULL, combined_tex_coords, size_vertices);
    }
    if (simplification_mode == MESH_OPTIMIZER_NO_UVS)
    {
        // strip uvs, can heavily affect textures
        shadow_indices = (U32*)ll_aligned_malloc_32(size_indices * sizeof(U32));
        LLMeshOptimizer::generateShadowIndexBufferU32(shadow_indices, combined_indices, size_indices, combined_positions, NULL, NULL, size_vertices);
    }

    U32* source_indices = NULL;
    if (shadow_indices)
    {
        source_indices = shadow_indices;
    }
    else
    {
        source_indices = combined_indices;
    }

    // III. Simplify
    S32 target_indices = 0;
    F32 result_error = 0; // how far from original the model is, 1 == 100%
    S32 size_new_indices = 0;

    if (indices_decimator > 0)
    {
        target_indices = llclamp(llfloor(size_indices / indices_decimator), 3, (S32)size_indices); // leave at least one triangle
    }
    else // indices_decimator can be zero for error_threshold based calculations
    {
        target_indices = 3;
    }

    size_new_indices = LLMeshOptimizer::simplifyU32(
        output_indices,
        source_indices,
        size_indices,
        combined_positions,
        size_vertices,
        LLVertexBuffer::sTypeSize[LLVertexBuffer::TYPE_VERTEX],
        target_indices,
        error_threshold,
        simplification_mode == MESH_OPTIMIZER_NO_TOPOLOGY,
        &result_error);

    if (result_error < 0)
    {
        // <FS:Beq> Log these properly
        // LL_WARNS() << "Negative result error from meshoptimizer for model " << target_model->mLabel
        //     << " target Indices: " << target_indices
        //     << " new Indices: " << size_new_indices
        //     << " original count: " << size_indices << LL_ENDL;
   		std::ostringstream out;
        out << "Negative result error from meshoptimizer for model " << target_model->mLabel
            << " target Indices: " << target_indices
            << " new Indices: " << size_new_indices
            << " original count: " << size_indices ;
		LL_WARNS() << out.str() << LL_ENDL;
		LLFloaterModelPreview::addStringToLog(out, true);
    }
    else 
    {
        if (mImporterDebug)
        {
            std::ostringstream out;
            out << "Good result error from meshoptimizer for model " << target_model->mLabel
                << " target Indices: " << target_indices
                << " new Indices: " << size_new_indices
                << " original count: " << size_indices << " (result error:" << result_error << ")";
		    LL_DEBUGS() << out.str() << LL_ENDL;
        	LLFloaterModelPreview::addStringToLog(out, true);
        }
        // </FS:Beq>
    }

    // free unused buffers
    ll_aligned_free_32(combined_indices);
    ll_aligned_free_32(shadow_indices);
    combined_indices = NULL;
    shadow_indices = NULL;

    if (size_new_indices < 3)
    {
        // Model should have at least one visible triangle
        ll_aligned_free<64>(combined_positions);
        ll_aligned_free_32(output_indices);

        return -1;
    }

    // IV. Repack back into individual faces

    LLVector4a* buffer_positions = (LLVector4a*)ll_aligned_malloc<64>(sizeof(LLVector4a) * 2 * size_vertices + tc_bytes_size);
    LLVector4a* buffer_normals = buffer_positions + size_vertices;
    LLVector2* buffer_tex_coords = (LLVector2*)(buffer_normals + size_vertices);
    S32 buffer_idx_size = (size_indices * sizeof(U16) + 0xF) & ~0xF;
    U16* buffer_indices = (U16*)ll_aligned_malloc_16(buffer_idx_size);
    S32* old_to_new_positions_map = new S32[size_vertices];

    S32 buf_positions_copied = 0;
    S32 buf_indices_copied = 0;
    indices_idx_shift = 0;
    S32 valid_faces = 0;

    // Crude method to copy indices back into face
    for (U32 face_idx = 0; face_idx < base_model->getNumVolumeFaces(); ++face_idx)
    {
        const LLVolumeFace &face = base_model->getVolumeFace(face_idx);

        // reset data for new run
        buf_positions_copied = 0;
        buf_indices_copied = 0;
        bool copy_triangle = false;
        S32 range = indices_idx_shift + face.mNumVertices;

        for (S32 i = 0; i < size_vertices; i++)
        {
            old_to_new_positions_map[i] = -1;
        }

        // Copy relevant indices and vertices
        for (S32 i = 0; i < size_new_indices; ++i)
        {
            U32 idx = output_indices[i];

            if ((i % 3) == 0)
            {
                copy_triangle = idx >= indices_idx_shift && idx < range;
            }

            if (copy_triangle)
            {
                if (old_to_new_positions_map[idx] == -1)
                {
                    // New position, need to copy it
                    // Validate size
                    if (buf_positions_copied >= U16_MAX)
                    {
                        // Normally this shouldn't happen since the whole point is to reduce amount of vertices
                        // but it might happen if user tries to run optimization with too large triangle or error value
                        // so fallback to 'per face' mode or verify requested limits and copy base model as is.
                        // <FS:Beq> Log this properly
                        // LL_WARNS() << "Over triangle limit. Failed to optimize in 'per object' mode, falling back to per face variant for"
                        //     << " model " << target_model->mLabel
                        //     << " target Indices: " << target_indices
                        //     << " new Indices: " << size_new_indices
                        //     << " original count: " << size_indices
                        //     << " error treshold: " << error_threshold
                        //     << LL_ENDL;
                        if (mImporterDebug)
                        {
                            std::ostringstream out;
                            out << "Over triangle limit. Failed to optimize in 'per object' mode, falling back to per face variant for"
                                << " model " << target_model->mLabel
                                << " target Indices: " << target_indices
                                << " new Indices: " << size_new_indices
                                << " original count: " << size_indices
                                << " error treshold: " << error_threshold;
                            LL_DEBUGS() << out.str() << LL_ENDL;
                            LLFloaterModelPreview::addStringToLog(out, true);
                        }
                        // U16 vertices overflow shouldn't happen, but just in case
                        size_new_indices = 0;
                        valid_faces = 0;
                        for (U32 face_idx = 0; face_idx < base_model->getNumVolumeFaces(); ++face_idx)
                        {
                            genMeshOptimizerPerFace(base_model, target_model, face_idx, indices_decimator, error_threshold, simplification_mode);
                            const LLVolumeFace &face = target_model->getVolumeFace(face_idx);
                            size_new_indices += face.mNumIndices;
                            if (face.mNumIndices >= 3)
                            {
                                valid_faces++;
                            }
                        }
                        if (valid_faces)
                        {
                            return (F32)size_indices / (F32)size_new_indices;
                        }
                        else
                        {
                            return -1;
                        }
                    }

                    // Copy vertice, normals, tcs
                    buffer_positions[buf_positions_copied] = combined_positions[idx];
                    buffer_normals[buf_positions_copied] = combined_normals[idx];
                    buffer_tex_coords[buf_positions_copied] = combined_tex_coords[idx];

                    old_to_new_positions_map[idx] = buf_positions_copied;

                    buffer_indices[buf_indices_copied] = (U16)buf_positions_copied;
                    buf_positions_copied++;
                }
                else
                {
                    // existing position
                    buffer_indices[buf_indices_copied] = (U16)old_to_new_positions_map[idx];
                }
                buf_indices_copied++;
            }
        }

        if (buf_positions_copied >= U16_MAX)
        {
            break;
        }

        LLVolumeFace &new_face = target_model->getVolumeFace(face_idx);
        //new_face = face; //temp

        if (buf_indices_copied < 3)
        {
            // face was optimized away
            new_face.resizeIndices(3);
            new_face.resizeVertices(1);
            memset(new_face.mIndices, 0, sizeof(U16) * 3);
            new_face.mPositions[0].clear(); // set first vertice to 0
            new_face.mNormals[0].clear();
            new_face.mTexCoords[0].setZero();
        }
        else
        {
            new_face.resizeIndices(buf_indices_copied);
            new_face.resizeVertices(buf_positions_copied);

            S32 idx_size = (buf_indices_copied * sizeof(U16) + 0xF) & ~0xF;
            LLVector4a::memcpyNonAliased16((F32*)new_face.mIndices, (F32*)buffer_indices, idx_size);

            LLVector4a::memcpyNonAliased16((F32*)new_face.mPositions, (F32*)buffer_positions, buf_positions_copied * sizeof(LLVector4a));
            LLVector4a::memcpyNonAliased16((F32*)new_face.mNormals, (F32*)buffer_normals, buf_positions_copied * sizeof(LLVector4a));

            U32 tex_size = (buf_positions_copied * sizeof(LLVector2) + 0xF)&~0xF;
            LLVector4a::memcpyNonAliased16((F32*)new_face.mTexCoords, (F32*)buffer_tex_coords, tex_size);

            valid_faces++;
        }

        indices_idx_shift += face.mNumVertices;
    }

    delete[]old_to_new_positions_map;
    ll_aligned_free<64>(combined_positions);
    ll_aligned_free<64>(buffer_positions);
    ll_aligned_free_32(output_indices);
    ll_aligned_free_16(buffer_indices);

    if (size_new_indices < 3 || valid_faces == 0)
    {
        // Model should have at least one visible triangle
        return -1;
    }

    return (F32)size_indices / (F32)size_new_indices;
}

F32 LLModelPreview::genMeshOptimizerPerFace(LLModel *base_model, LLModel *target_model, U32 face_idx, F32 indices_decimator, F32 error_threshold, eSimplificationMode simplification_mode)
{
    const LLVolumeFace &face = base_model->getVolumeFace(face_idx);
    S32 size_indices = face.mNumIndices;
    if (size_indices < 3)
    {
        return -1;
    }

    S32 size = (size_indices * sizeof(U16) + 0xF) & ~0xF;
    U16* output_indices = (U16*)ll_aligned_malloc_16(size);

    U16* shadow_indices = NULL;
    // if MESH_OPTIMIZER_FULL, just leave as is, since generateShadowIndexBufferU32
    // won't do anything new, model was remaped on a per face basis.
    // Similar for MESH_OPTIMIZER_NO_TOPOLOGY, it's pointless
    // since 'simplifySloppy' ignores all topology, including normals and uvs.
    if (simplification_mode == MESH_OPTIMIZER_NO_NORMALS)
    {
        U16* shadow_indices = (U16*)ll_aligned_malloc_16(size);
        LLMeshOptimizer::generateShadowIndexBufferU16(shadow_indices, face.mIndices, size_indices, face.mPositions, NULL, face.mTexCoords, face.mNumVertices);
    }
    if (simplification_mode == MESH_OPTIMIZER_NO_UVS)
    {
        U16* shadow_indices = (U16*)ll_aligned_malloc_16(size);
        LLMeshOptimizer::generateShadowIndexBufferU16(shadow_indices, face.mIndices, size_indices, face.mPositions, NULL, NULL, face.mNumVertices);
    }
    // Don't run ShadowIndexBuffer for MESH_OPTIMIZER_NO_TOPOLOGY, it's pointless

    U16* source_indices = NULL;
    if (shadow_indices)
    {
        source_indices = shadow_indices;
    }
    else
    {
        source_indices = face.mIndices;
    }

    S32 target_indices = 0;
    F32 result_error = 0; // how far from original the model is, 1 == 100%
    S32 size_new_indices = 0;

    if (indices_decimator > 0)
    {
        target_indices = llclamp(llfloor(size_indices / indices_decimator), 3, (S32)size_indices); // leave at least one triangle
    }
    else
    {
        target_indices = 3;
    }

    size_new_indices = LLMeshOptimizer::simplify(
        output_indices,
        source_indices,
        size_indices,
        face.mPositions,
        face.mNumVertices,
        LLVertexBuffer::sTypeSize[LLVertexBuffer::TYPE_VERTEX],
        target_indices,
        error_threshold,
        simplification_mode == MESH_OPTIMIZER_NO_TOPOLOGY,
        &result_error);

    if (result_error < 0)
    {
        // <FS:Beq> Log these properly
        // LL_WARNS() << "Negative result error from meshoptimizer for face " << face_idx
        //     << " of model " << target_model->mLabel
        //     << " target Indices: " << target_indices
        //     << " new Indices: " << size_new_indices
        //     << " original count: " << size_indices
        //     << " error treshold: " << error_threshold
        //     << LL_ENDL;
   		std::ostringstream out;
        out << "Negative result error from meshoptimizer for face " << face_idx
            << " of model " << target_model->mLabel
            << " target Indices: " << target_indices
            << " new Indices: " << size_new_indices
            << " original count: " << size_indices
            << " error treshold: " << error_threshold;
		LL_WARNS() << out.str() << LL_ENDL;
		LLFloaterModelPreview::addStringToLog(out, true);
    }
    else 
    {
        if (mImporterDebug)
        {
            std::ostringstream out;
            out << "Good result error from meshoptimizer for face " << face_idx
                << " of model " << target_model->mLabel
                << " target Indices: " << target_indices
                << " new Indices: " << size_new_indices
                << " original count: " << size_indices
                << " error treshold: " << error_threshold << " (result error:" << result_error << ")";
		    LL_DEBUGS("MeshUpload") << out.str() << LL_ENDL;
        	LLFloaterModelPreview::addStringToLog(out, true);
        }
        // </FS:Beq>
    }

    LLVolumeFace &new_face = target_model->getVolumeFace(face_idx);

    // Copy old values
    new_face = face;

    if (size_new_indices < 3)
    {
        if (simplification_mode != MESH_OPTIMIZER_NO_TOPOLOGY)
        {
            // meshopt_optimizeSloppy() can optimize triangles away even if target_indices is > 2,
            // but optimize() isn't supposed to
            // LL_INFOS() << "No indices generated by meshoptimizer for face " << face_idx
            //     << " of model " << target_model->mLabel
            //     << " target Indices: " << target_indices
            //     << " original count: " << size_indices
            //     << " error treshold: " << error_threshold
            //     << LL_ENDL;
            std::ostringstream out;
            out << "No indices generated by meshoptimizer for face " << face_idx
                << " of model " << target_model->mLabel
                << " target Indices: " << target_indices
                << " original count: " << size_indices
                << " error treshold: " << error_threshold;
            LL_INFOS("MeshUpload") << out.str() << LL_ENDL;
        	LLFloaterModelPreview::addStringToLog(out, true);
        }

        // Face got optimized away
        // Generate empty triangle
        new_face.resizeIndices(3);
        new_face.resizeVertices(1);
        memset(new_face.mIndices, 0, sizeof(U16) * 3);
        new_face.mPositions[0].clear(); // set first vertice to 0
        new_face.mNormals[0].clear();
        new_face.mTexCoords[0].setZero();
    }
    else
    {
        // Assign new values
        new_face.resizeIndices(size_new_indices); // will wipe out mIndices, so new_face can't substitute output
        S32 idx_size = (size_new_indices * sizeof(U16) + 0xF) & ~0xF;
        LLVector4a::memcpyNonAliased16((F32*)new_face.mIndices, (F32*)output_indices, idx_size);

        // Clear unused values
        new_face.optimize();
    }

    ll_aligned_free_16(output_indices);
    ll_aligned_free_16(shadow_indices);
     
    if (size_new_indices < 3)
    {
        // At least one triangle is needed
        return -1;
    }

    return (F32)size_indices / (F32)size_new_indices;
}

void LLModelPreview::genMeshOptimizerLODs(S32 which_lod, S32 meshopt_mode, U32 decimation, bool enforce_tri_limit)
{
    // <FS:Beq> Log things properly
    // LL_INFOS() << "Generating lod " << which_lod << " using meshoptimizer" << LL_ENDL;
    {
        std::ostringstream out;
        out << "Generating lod " << which_lod << " using meshoptimizer";
        LL_INFOS("MeshUpload") << out.str() << LL_ENDL;
        LLFloaterModelPreview::addStringToLog(out, false);
    }
    // </FS:Beq>
    // Allow LoD from -1 to LLModel::LOD_PHYSICS
    if (which_lod < -1 || which_lod > LLModel::NUM_LODS - 1)
    {
        std::ostringstream out; 
        out << "Invalid level of detail: " << which_lod;
        LL_WARNS() << out.str() << LL_ENDL;
        LLFloaterModelPreview::addStringToLog(out, true); // <FS:Beq/> if you don't flash the log tab on error when do you?
        assert(which_lod >= -1 && which_lod < LLModel::NUM_LODS); // <FS:PR-Aleric/> use the correct variable (which_lod).
        return;
    }

    if (mBaseModel.empty())
    {
        return;
    }

    //get the triangle count for all base models
    S32 base_triangle_count = 0;
    for (S32 i = 0; i < mBaseModel.size(); ++i)
    {
        base_triangle_count += mBaseModel[i]->getNumTriangles();
    }

    // Urgh...
    // TODO: add interface to mFMP to get error treshold or let mFMP write one into LLModelPreview
    // We should not be accesing views from other class!
    U32 lod_mode = LIMIT_TRIANGLES;
    F32 indices_decimator = 0;
    F32 triangle_limit = 0;
    F32 lod_error_threshold = 1; //100%

    // If requesting a single lod
    if (which_lod > -1 && which_lod < NUM_LOD)
    {
        LLCtrlSelectionInterface* iface = mFMP->childGetSelectionInterface("lod_mode_" + lod_name[which_lod]);
        if (iface)
        {
            lod_mode = iface->getFirstSelectedIndex();
        }

        if (lod_mode == LIMIT_TRIANGLES)
        {
            if (!enforce_tri_limit)
            {
                triangle_limit = base_triangle_count;
                // reset to default value for this lod
                F32 pw = pow((F32)decimation, (F32)(LLModel::LOD_HIGH - which_lod));

                triangle_limit /= pw; //indices_ratio can be 1/pw
            }
            else
            {

                // UI spacifies limit for all models of single lod
                triangle_limit = mFMP->childGetValue("lod_triangle_limit_" + lod_name[which_lod]).asInteger();

            }
            // meshoptimizer doesn't use triangle limit, it uses indices limit, so convert it to aproximate ratio
            // triangle_limit can be 0.
            indices_decimator = (F32)base_triangle_count / llmax(triangle_limit, 1.f);
        }
        else
        {
            // UI shows 0 to 100%, but meshoptimizer works with 0 to 1
            lod_error_threshold = mFMP->childGetValue("lod_error_threshold_" + lod_name[which_lod]).asReal() / 100.f;
        }
    }
    else
    {
        // we are genrating all lods and each lod will get own indices_decimator
        indices_decimator = 1;
        triangle_limit = base_triangle_count;
    }

    mMaxTriangleLimit = base_triangle_count;

    // Build models

    S32 start = LLModel::LOD_HIGH;
    S32 end = 0;

    if (which_lod != -1)
    {
        start = which_lod;
        end = which_lod;
    }

    for (S32 lod = start; lod >= end; --lod)
    {
        if (which_lod == -1)
        {
            // we are genrating all lods and each lod gets own indices_ratio
            if (lod < start)
            {
                indices_decimator *= decimation;
                triangle_limit /= decimation;
            }
        }

        mRequestedTriangleCount[lod] = triangle_limit;
        mRequestedErrorThreshold[lod] = lod_error_threshold * 100;
        mRequestedLoDMode[lod] = lod_mode;

        mModel[lod].clear();
        mModel[lod].resize(mBaseModel.size());
        mVertexBuffer[lod].clear();


        for (U32 mdl_idx = 0; mdl_idx < mBaseModel.size(); ++mdl_idx)
        {
            LLModel* base = mBaseModel[mdl_idx];

            LLVolumeParams volume_params;
            volume_params.setType(LL_PCODE_PROFILE_SQUARE, LL_PCODE_PATH_LINE);
            mModel[lod][mdl_idx] = new LLModel(volume_params, 0.f);

            // <FS:Beq> Support altenate LOD naming conventions
            // std::string name = base->mLabel + getLodSuffix(lod);
            std::string name = stripSuffix(base->mLabel);
            std::string suffix = getLodSuffix(lod);
            if (suffix.size() > 0)
            {
                name += suffix;
            }
            // </FS:Beq>

            mModel[lod][mdl_idx]->mLabel = name;
            mModel[lod][mdl_idx]->mSubmodelID = base->mSubmodelID;
            mModel[lod][mdl_idx]->setNumVolumeFaces(base->getNumVolumeFaces());

            LLModel* target_model = mModel[lod][mdl_idx];

            S32 model_meshopt_mode = meshopt_mode;

            // Ideally this should run not per model,
            // but combine all submodels with origin model as well
            if (model_meshopt_mode == MESH_OPTIMIZER_PRECISE)
            {
                // Run meshoptimizer for each face
                for (U32 face_idx = 0; face_idx < base->getNumVolumeFaces(); ++face_idx)
                {
                    F32 res = genMeshOptimizerPerFace(base, target_model, face_idx, indices_decimator, lod_error_threshold, MESH_OPTIMIZER_FULL);
                    if (res < 0)
                    {
                        // Mesh optimizer failed and returned an invalid model
                        const LLVolumeFace &face = base->getVolumeFace(face_idx);
                        LLVolumeFace &new_face = target_model->getVolumeFace(face_idx);
                        new_face = face;
                    }
                }
            }

            if (model_meshopt_mode == MESH_OPTIMIZER_SLOPPY)
            {
                // Run meshoptimizer for each face
                for (U32 face_idx = 0; face_idx < base->getNumVolumeFaces(); ++face_idx)
                {
                    if (genMeshOptimizerPerFace(base, target_model, face_idx, indices_decimator, lod_error_threshold, MESH_OPTIMIZER_NO_TOPOLOGY) < 0)
                    {
                        // Sloppy failed and returned an invalid model
                        genMeshOptimizerPerFace(base, target_model, face_idx, indices_decimator, lod_error_threshold, MESH_OPTIMIZER_FULL);
                    }
                }
            }

            if (model_meshopt_mode == MESH_OPTIMIZER_AUTO)
            {
                // Remove progressively more data if we can't reach the target.
                F32 allowed_ratio_drift = 1.8f;
                F32 precise_ratio = genMeshOptimizerPerModel(base, target_model, indices_decimator, lod_error_threshold, MESH_OPTIMIZER_FULL);

                if (precise_ratio < 0 || (precise_ratio * allowed_ratio_drift < indices_decimator))
                {
                    precise_ratio = genMeshOptimizerPerModel(base, target_model, indices_decimator, lod_error_threshold, MESH_OPTIMIZER_NO_NORMALS);
                }

                if (precise_ratio < 0 || (precise_ratio * allowed_ratio_drift < indices_decimator))
                {
                    precise_ratio = genMeshOptimizerPerModel(base, target_model, indices_decimator, lod_error_threshold, MESH_OPTIMIZER_NO_UVS);
                }
                
                if (precise_ratio < 0 || (precise_ratio * allowed_ratio_drift < indices_decimator))
                {
                    // Try sloppy variant if normal one failed to simplify model enough.
                    // Sloppy variant can fail entirely and has issues with precision,
                    // so code needs to do multiple attempts with different decimators.
                    // Todo: this is a bit of a mess, needs to be refined and improved

                    F32 last_working_decimator = 0.f;
                    F32 last_working_ratio = F32_MAX;

                    F32 sloppy_ratio = genMeshOptimizerPerModel(base, target_model, indices_decimator, lod_error_threshold, MESH_OPTIMIZER_NO_TOPOLOGY);

                    if (sloppy_ratio > 0)
                    {
                        // Would be better to do a copy of target_model here, but if
                        // we need to use sloppy decimation, model should be cheap
                        // and fast to generate and it won't affect end result
                        last_working_decimator = indices_decimator;
                        last_working_ratio = sloppy_ratio;
                    }

                    // Sloppy has a tendecy to error into lower side, so a request for 100
                    // triangles turns into ~70, so check for significant difference from target decimation
                    F32 sloppy_ratio_drift = 1.4f;
                    if (lod_mode == LIMIT_TRIANGLES
                        && (sloppy_ratio > indices_decimator * sloppy_ratio_drift || sloppy_ratio < 0))
                    {
                        // Apply a correction to compensate.

                        // (indices_decimator / res_ratio) by itself is likely to overshoot to a differend
                        // side due to overal lack of precision, and we don't need an ideal result, which
                        // likely does not exist, just a better one, so a partial correction is enough.
                        F32 sloppy_decimator{indices_decimator};
                        // if(sloppy_ratio > 0)
                        // {
                        sloppy_decimator = indices_decimator * (indices_decimator / sloppy_ratio + 1) / 2;
                        // }
                        sloppy_ratio = genMeshOptimizerPerModel(base, target_model, sloppy_decimator, lod_error_threshold, MESH_OPTIMIZER_NO_TOPOLOGY);
                    }

                    if (last_working_decimator > 0 && sloppy_ratio < last_working_ratio)
                    {
                        // Compensation didn't work, return back to previous decimator
                        sloppy_ratio = genMeshOptimizerPerModel(base, target_model, indices_decimator, lod_error_threshold, MESH_OPTIMIZER_NO_TOPOLOGY);
                    }

                    if (sloppy_ratio < 0)
                    {
                        // Sloppy method didn't work, try with smaller decimation values
                        {
                            // Find a decimator that does work
                            F32 sloppy_decimation_step = sqrt((F32)decimation); // example: 27->15->9->5->3
                            F32 sloppy_decimator = indices_decimator / sloppy_decimation_step;
                            U64Microseconds end_time = LLTimer::getTotalTime() + U64Seconds(5);

                            while (sloppy_ratio < 0
                                && sloppy_decimator > precise_ratio
                                && sloppy_decimator > 1 // precise_ratio isn't supposed to be below 1, but check just in case
                                && end_time > LLTimer::getTotalTime())
                            {
                                sloppy_ratio = genMeshOptimizerPerModel(base, target_model, sloppy_decimator, lod_error_threshold, MESH_OPTIMIZER_NO_TOPOLOGY);
                                sloppy_decimator = sloppy_decimator / sloppy_decimation_step;
                            }
                        }
                    }

                    if (sloppy_ratio < 0 || sloppy_ratio < precise_ratio)
                    {
                        // Sloppy variant failed to generate triangles or is worse.
                        // Can happen with models that are too simple as is.

                        if (precise_ratio < 0)
                        {
                            // Precise method failed as well, just copy face over
                            target_model->copyVolumeFaces(base);
                            precise_ratio = 1.f;
                        }
                        else
                        {
                            // Fallback to normal method
                            precise_ratio = genMeshOptimizerPerModel(base, target_model, indices_decimator, lod_error_threshold, MESH_OPTIMIZER_FULL);
                        }
                        // <FS:Beq> Log stuff properly
                        // LL_INFOS() << "Model " << target_model->getName()
                        //     << " lod " << which_lod
                        //     << " resulting ratio " << precise_ratio
                        //     << " simplified using per model method." << LL_ENDL;
                        {
                            std::ostringstream out;
                            out << "Model " << target_model->getName()
                                << " lod " << which_lod
                                << " resulting ratio " << precise_ratio
                                << " simplified using per model method.";
                            LL_INFOS() << out.str() << LL_ENDL;
                            LLFloaterModelPreview::addStringToLog(out, false);
                        }
                        // </FS:Beq>
                    }
                    else
                    {
                        // <FS:Beq> Log stuff properly
                        // LL_INFOS() << "Model " << target_model->getName()
                        //     << " lod " << which_lod
                        //     << " resulting ratio " << sloppy_ratio
                        //     << " sloppily simplified using per model method." << LL_ENDL;
                        std::ostringstream out;
                        out << "Model " << target_model->getName()
                            << " lod " << which_lod
                            << " resulting ratio " << sloppy_ratio
                            << " sloppily simplified using per model method.";
                        LL_INFOS() << out.str() << LL_ENDL;
                        LLFloaterModelPreview::addStringToLog(out, false);
                        // </FS:Beq>
                    }
                }
                else
                {
                        // <FS:Beq> Log stuff properly
                        // LL_INFOS() << "Model " << target_model->getName()
                        //     << " lod " << which_lod
                        //     << " resulting ratio " << precise_ratio
                        //     << " simplified using per model method." << LL_ENDL;
                        std::ostringstream out;
                        out << "Bad MeshOptimisation result for Model " << target_model->getName()
                            << " lod " << which_lod
                            << " resulting ratio " << precise_ratio
                            << " simplified using per model method.";
                        LL_WARNS() << out.str() << LL_ENDL;
                        LLFloaterModelPreview::addStringToLog(out, true);
                        // </FS:Beq>
                }
            }

            //blind copy skin weights and just take closest skin weight to point on
            //decimated mesh for now (auto-generating LODs with skin weights is still a bit
            //of an open problem).
            target_model->mPosition = base->mPosition;
            target_model->mSkinWeights = base->mSkinWeights;
            target_model->mSkinInfo = base->mSkinInfo;

            //copy material list
            target_model->mMaterialList = base->mMaterialList;

            if (!validate_model(target_model))
            {
                LL_ERRS() << "Invalid model generated when creating LODs" << LL_ENDL;
            }
        }

        //rebuild scene based on mBaseScene
        mScene[lod].clear();
        mScene[lod] = mBaseScene;

        for (U32 i = 0; i < mBaseModel.size(); ++i)
        {
            LLModel* mdl = mBaseModel[i];
            LLModel* target = mModel[lod][i];
            if (target)
            {
                for (LLModelLoader::scene::iterator iter = mScene[lod].begin(); iter != mScene[lod].end(); ++iter)
                {
                    for (U32 j = 0; j < iter->second.size(); ++j)
                    {
                        if (iter->second[j].mModel == mdl)
                        {
                            iter->second[j].mModel = target;
                        }
                    }
                }
            }
        }
    }
}

void LLModelPreview::updateStatusMessages()
{
    // bit mask values for physics errors. used to prevent overwrite of single line status
    // TODO: use this to provied multiline status
    enum PhysicsError
    {
        NONE = 0,
        NOHAVOK = 1,
        DEGENERATE = 2,
        TOOMANYHULLS = 4,
// <FS:Beq> fIRE-31602 thin mesh physics warning
        // TOOMANYVERTSINHULL = 8
        TOOMANYVERTSINHULL = 8,
        TOOTHIN = 16
// </FS:Beq>
    };

    assert_main_thread();

    U32 has_physics_error{ PhysicsError::NONE }; // physics error bitmap
    //triangle/vertex/submesh count for each mesh asset for each lod
    std::vector<S32> tris[LLModel::NUM_LODS];
    std::vector<S32> verts[LLModel::NUM_LODS];
    std::vector<S32> submeshes[LLModel::NUM_LODS];

    //total triangle/vertex/submesh count for each lod
    S32 total_tris[LLModel::NUM_LODS];
    S32 total_verts[LLModel::NUM_LODS];
    S32 total_submeshes[LLModel::NUM_LODS];

    for (U32 i = 0; i < LLModel::NUM_LODS - 1; i++)
    {
        total_tris[i] = 0;
        total_verts[i] = 0;
        total_submeshes[i] = 0;
    }

    for (LLMeshUploadThread::instance_list::iterator iter = mUploadData.begin(); iter != mUploadData.end(); ++iter)
    {
        LLModelInstance& instance = *iter;

        LLModel* model_high_lod = instance.mLOD[LLModel::LOD_HIGH];
        if (!model_high_lod)
        {
            setLoadState(LLModelLoader::ERROR_HIGH_LOD_MODEL_MISSING); // <FS:Beq/> FIRE-30965 Cleanup braindead mesh parsing error handlers
            mFMP->childDisable("calculate_btn");
            continue;
        }

        for (U32 i = 0; i < LLModel::NUM_LODS - 1; i++)
        {
            LLModel* lod_model = instance.mLOD[i];
            if (!lod_model)
            {
                setLoadState(LLModelLoader::ERROR_LOD_MODEL_MISMATCH); // <FS:Beq/> FIRE-30965 Cleanup braindead mesh parsing error handlers
                mFMP->childDisable("calculate_btn");
            }
            else
            {
                //for each model in the lod
                S32 cur_tris = 0;
                S32 cur_verts = 0;
                S32 cur_submeshes = lod_model->getNumVolumeFaces();

                for (S32 j = 0; j < cur_submeshes; ++j)
                { //for each submesh (face), add triangles and vertices to current total
                    const LLVolumeFace& face = lod_model->getVolumeFace(j);
                    cur_tris += face.mNumIndices / 3;
                    cur_verts += face.mNumVertices;
                }

                std::string instance_name = instance.mLabel;

                if (mImporterDebug)
                {
                    // Useful for debugging generalized complaints below about total submeshes which don't have enough
                    // context to address exactly what needs to be fixed to move towards compliance with the rules.
                    //
                    std::ostringstream out;
                    out << "Instance " << lod_model->mLabel << " LOD " << i << " Verts: " << cur_verts;
                    LL_INFOS() << out.str() << LL_ENDL;
                    LLFloaterModelPreview::addStringToLog(out, false);

                    out.str("");
                    out << "Instance " << lod_model->mLabel << " LOD " << i << " Tris:  " << cur_tris;
                    LL_INFOS() << out.str() << LL_ENDL;
                    LLFloaterModelPreview::addStringToLog(out, false);

                    out.str("");
                    out << "Instance " << lod_model->mLabel << " LOD " << i << " Faces: " << cur_submeshes;
                    LL_INFOS() << out.str() << LL_ENDL;
                    LLFloaterModelPreview::addStringToLog(out, false);

                    out.str("");
                    LLModel::material_list::iterator mat_iter = lod_model->mMaterialList.begin();
                    while (mat_iter != lod_model->mMaterialList.end())
                    {
                        out << "Instance " << lod_model->mLabel << " LOD " << i << " Material " << *(mat_iter);
                        LL_INFOS() << out.str() << LL_ENDL;
                        LLFloaterModelPreview::addStringToLog(out, false);
                        out.str("");
                        mat_iter++;
                    }
                }

                //add this model to the lod total
                total_tris[i] += cur_tris;
                total_verts[i] += cur_verts;
                total_submeshes[i] += cur_submeshes;

                //store this model's counts to asset data
                tris[i].push_back(cur_tris);
                verts[i].push_back(cur_verts);
                submeshes[i].push_back(cur_submeshes);
            }
        }
    }

    if (mMaxTriangleLimit == 0)
    {
        mMaxTriangleLimit = total_tris[LLModel::LOD_HIGH];
    }

// <FS:Beq> fIRE-31602 thin mesh physics warning
    static const float CONVEXIFICATION_SIZE_MESH {0.5};
    auto smallest_axis = llmin(mPreviewScale.mV[0], mPreviewScale.mV[1]);
    smallest_axis = llmin(smallest_axis, mPreviewScale.mV[2]);
    smallest_axis *= 2.f;
    if (smallest_axis < CONVEXIFICATION_SIZE_MESH)
    {
        has_physics_error |= PhysicsError::TOOTHIN;
    }
// </FS:Beq<

    mHasDegenerate = false;
    {//check for degenerate triangles in physics mesh
        U32 lod = LLModel::LOD_PHYSICS;
        const LLVector4a scale(0.5f);
        for (U32 i = 0; i < mModel[lod].size() && !mHasDegenerate; ++i)
        { //for each model in the lod
            if (mModel[lod][i] && mModel[lod][i]->mPhysics.mHull.empty())
            { //no decomp exists
                S32 cur_submeshes = mModel[lod][i]->getNumVolumeFaces();
                for (S32 j = 0; j < cur_submeshes && !mHasDegenerate; ++j)
                { //for each submesh (face), add triangles and vertices to current total
                    LLVolumeFace& face = mModel[lod][i]->getVolumeFace(j);
                    for (S32 k = 0; (k < face.mNumIndices) && !mHasDegenerate;)
                    {
                        U16 index_a = face.mIndices[k + 0];
                        U16 index_b = face.mIndices[k + 1];
                        U16 index_c = face.mIndices[k + 2];

                        if (index_c == 0 && index_b == 0 && index_a == 0) // test in reverse as 3rd index is less likely to be 0 in a normal case
                        {
                            LL_DEBUGS("MeshValidation") << "Empty placeholder triangle (3 identical index 0 verts) ignored" << LL_ENDL;
                        }
                        else
                        {
                            LLVector4a v1; v1.setMul(face.mPositions[index_a], scale);
                            LLVector4a v2; v2.setMul(face.mPositions[index_b], scale);
                            LLVector4a v3; v3.setMul(face.mPositions[index_c], scale);
                            if (ll_is_degenerate(v1, v2, v3))
                            {
                                mHasDegenerate = true;
                            }
                        }
                        k += 3;
                    }
                }
            }
        }
    }

    // flag degenerates here rather than deferring to a MAV error later
    // <FS>
    //mFMP->childSetVisible("physics_status_message_text", mHasDegenerate); //display or clear
    //auto degenerateIcon = mFMP->getChild<LLIconCtrl>("physics_status_message_icon");
    //degenerateIcon->setVisible(mHasDegenerate);
    // </FS>
    if (mHasDegenerate)
    {
        has_physics_error |= PhysicsError::DEGENERATE;
        // <FS>
        //mFMP->childSetValue("physics_status_message_text", mFMP->getString("phys_status_degenerate_triangles"));
        //LLUIImagePtr img = LLUI::getUIImage("ModelImport_Status_Error");
        //degenerateIcon->setImage(img);
        // </FS>
    }

    mFMP->childSetTextArg("submeshes_info", "[SUBMESHES]", llformat("%d", total_submeshes[LLModel::LOD_HIGH]));

    std::string mesh_status_na = mFMP->getString("mesh_status_na");

    S32 upload_status[LLModel::LOD_HIGH + 1];

    mModelNoErrors = true;

    const U32 lod_high = LLModel::LOD_HIGH;
    U32 high_submodel_count = mModel[lod_high].size() - countRootModels(mModel[lod_high]);

    for (S32 lod = 0; lod <= lod_high; ++lod)
    {
        upload_status[lod] = 0;

        std::string message = "mesh_status_good";

        if (total_tris[lod] > 0)
        {
            mFMP->childSetValue(lod_triangles_name[lod], llformat("%d", total_tris[lod]));
            mFMP->childSetValue(lod_vertices_name[lod], llformat("%d", total_verts[lod]));
        }
        else
        {
            if (lod == lod_high)
            {
                upload_status[lod] = 2;
                message = "mesh_status_missing_lod";
            }
            else
            {
                for (S32 i = lod - 1; i >= 0; --i)
                {
                    if (total_tris[i] > 0)
                    {
                        upload_status[lod] = 2;
                        message = "mesh_status_missing_lod";
                    }
                }
            }

            mFMP->childSetValue(lod_triangles_name[lod], mesh_status_na);
            mFMP->childSetValue(lod_vertices_name[lod], mesh_status_na);
        }

        if (lod != lod_high)
        {
            if (total_submeshes[lod] && total_submeshes[lod] != total_submeshes[lod_high])
            { //number of submeshes is different
                message = "mesh_status_submesh_mismatch";
                upload_status[lod] = 2;
            }
            else if (mModel[lod].size() - countRootModels(mModel[lod]) != high_submodel_count)
            {//number of submodels is different, not all faces are matched correctly.
                message = "mesh_status_submesh_mismatch";
                upload_status[lod] = 2;
                // Note: Submodels in instance were loaded from higher LOD and as result face count
                // returns same value and total_submeshes[lod] is identical to high_lod one.
            }
            else if (!tris[lod].empty() && tris[lod].size() != tris[lod_high].size())
            { //number of meshes is different
                message = "mesh_status_mesh_mismatch";
                upload_status[lod] = 2;
            }
            else if (!verts[lod].empty())
            {
                S32 sum_verts_higher_lod = 0;
                S32 sum_verts_this_lod = 0;
                for (U32 i = 0; i < verts[lod].size(); ++i)
                {
                    sum_verts_higher_lod += ((i < verts[lod + 1].size()) ? verts[lod + 1][i] : 0);
                    sum_verts_this_lod += verts[lod][i];
                }

                if ((sum_verts_higher_lod > 0) &&
                    (sum_verts_this_lod > sum_verts_higher_lod))
                {
                    //too many vertices in this lod
                    message = "mesh_status_too_many_vertices";
                    upload_status[lod] = 1;
                }
            }
        }

        LLIconCtrl* icon = mFMP->getChild<LLIconCtrl>(lod_icon_name[lod]);
        LLUIImagePtr img = LLUI::getUIImage(lod_status_image[upload_status[lod]]);
        icon->setVisible(true);
        icon->setImage(img);

        if (upload_status[lod] >= 2)
        {
            mModelNoErrors = false;
        }

        if (lod == mPreviewLOD)
        {
            mFMP->childSetValue("lod_status_message_text", mFMP->getString(message));
            icon = mFMP->getChild<LLIconCtrl>("lod_status_message_icon");
            icon->setImage(img);
        }

        updateLodControls(lod);
    }


    //warn if hulls have more than 256 points in them
    BOOL physExceededVertexLimit = FALSE;
    for (U32 i = 0; mModelNoErrors && (i < mModel[LLModel::LOD_PHYSICS].size()); ++i)
    {
        LLModel* mdl = mModel[LLModel::LOD_PHYSICS][i];

        if (mdl)
        {
            // <FS:Beq> Better error handling
            auto num_hulls = mdl->mPhysics.mHull.size();
            for (U32 j = 0; j < num_hulls; ++j)
            {
            // </FS:Beq>
                if (mdl->mPhysics.mHull[j].size() > 256)
                {
                    physExceededVertexLimit = TRUE;
                    // <FS:Beq> add new friendlier logging to mesh uploader
                    // LL_INFOS() << "Physical model " << mdl->mLabel << " exceeds vertex per hull limitations." << LL_ENDL;
                    std::ostringstream out;
                    out << "Physical model " << mdl->mLabel << " exceeds vertex per hull limitations.";
                    LL_INFOS() << out.str() << LL_ENDL;
                    LLFloaterModelPreview::addStringToLog(out, true);
                    out.str("");
                    // </FS:Beq>                     
                    break;
                }
            }
            // <FS:Beq> Better error handling
            if (num_hulls > 256) // decomp cannot have more than 256 hulls (http://wiki.secondlife.com/wiki/Mesh/Mesh_physics)
            {
                // <FS:Beq> improve uploader error reporting
                // LL_INFOS() << "Physical model " << mdl->mLabel << " exceeds 256 hull limitation." << LL_ENDL;
                std::ostringstream out;
                out << "Physical model " << mdl->mLabel << " exceeds 256 hull limitation.";
                LL_INFOS() << out.str() << LL_ENDL;
                LLFloaterModelPreview::addStringToLog(out, true);
                out.str("");
                // </FS:Beq>
                has_physics_error |= PhysicsError::TOOMANYHULLS;
            }
            // </FS:Beq>
        }
    }

    if (physExceededVertexLimit)
    {
        has_physics_error |= PhysicsError::TOOMANYVERTSINHULL;
    }

// <FS:Beq> standardise error handling
    //if (!(has_physics_error & PhysicsError::DEGENERATE)){ // only update this field (incluides clearing it) if it is not already in use.
    //    mFMP->childSetVisible("physics_status_message_text", physExceededVertexLimit);
    //    LLIconCtrl* physStatusIcon = mFMP->getChild<LLIconCtrl>("physics_status_message_icon");
    //    physStatusIcon->setVisible(physExceededVertexLimit);
    //    if (physExceededVertexLimit)
    //    {
    //        mFMP->childSetValue("physics_status_message_text", mFMP->getString("phys_status_vertex_limit_exceeded"));
    //        LLUIImagePtr img = LLUI::getUIImage("ModelImport_Status_Warning");
    //        physStatusIcon->setImage(img);
    //    }
    //}
#ifndef HAVOK_TPV
    has_physics_error |= PhysicsError::NOHAVOK;
#endif 

    auto physStatusIcon = mFMP->getChild<LLIconCtrl>("physics_status_message_icon");

    if (has_physics_error != PhysicsError::NONE)
    {
        mFMP->childSetVisible("physics_status_message_text", true); //display or clear
        physStatusIcon->setVisible(true);
        // The order here is important. 
        if (has_physics_error & PhysicsError::TOOMANYHULLS)
        {
            mFMP->childSetValue("physics_status_message_text", mFMP->getString("phys_status_hull_limit_exceeded"));
            LLUIImagePtr img = LLUI::getUIImage("ModelImport_Status_Error");
            physStatusIcon->setImage(img);
        }
        else if (has_physics_error & PhysicsError::TOOMANYVERTSINHULL)
        {
            mFMP->childSetValue("physics_status_message_text", mFMP->getString("phys_status_vertex_limit_exceeded"));
            LLUIImagePtr img = LLUI::getUIImage("ModelImport_Status_Error");
            physStatusIcon->setImage(img);
        }
        else if (has_physics_error & PhysicsError::DEGENERATE)
        {
            mFMP->childSetValue("physics_status_message_text", mFMP->getString("phys_status_degenerate_triangles"));
            LLUIImagePtr img = LLUI::getUIImage("ModelImport_Status_Error");
            physStatusIcon->setImage(img);
        }
// <FS:Beq> fIRE-31602 thin mesh physics warning
        else if (has_physics_error & PhysicsError::TOOTHIN)
        {
            mFMP->childSetValue("physics_status_message_text", mFMP->getString("phys_status_too_thin"));
            LLUIImagePtr img = LLUI::getUIImage("ModelImport_Status_Warning");
            physStatusIcon->setImage(img);
        }
// </FS:Beq>
        else if (has_physics_error & PhysicsError::NOHAVOK)
        {
            mFMP->childSetValue("physics_status_message_text", mFMP->getString("phys_status_no_havok"));
            LLUIImagePtr img = LLUI::getUIImage("ModelImport_Status_Warning");
            physStatusIcon->setImage(img);
        }
        else
        {
            // This should not happen
            mFMP->childSetValue("physics_status_message_text", mFMP->getString("phys_status_unknown_error"));
            LLUIImagePtr img = LLUI::getUIImage("ModelImport_Status_Warning");
            physStatusIcon->setImage(img);
        }
    }
    else
    {
        mFMP->childSetVisible("physics_status_message_text", false); //display or clear
        physStatusIcon->setVisible(false);
    }
// </FS:Beq>

    if (getLoadState() >= LLModelLoader::ERROR_PARSING)
    {
        mModelNoErrors = false;
        // <FS:Beq> improve uploader error reporting
        // LL_INFOS() << "Loader returned errors, model can't be uploaded" << LL_ENDL;
        std::ostringstream out;
        out << "Loader returned errors, model can't be uploaded";
        LL_INFOS() << out.str() << LL_ENDL;
        LLFloaterModelPreview::addStringToLog(out, true);
        // </FS:Beq>
    }

    bool uploadingSkin = mFMP->childGetValue("upload_skin").asBoolean();
    bool uploadingJointPositions = mFMP->childGetValue("upload_joints").asBoolean();

    if (uploadingSkin)
    {
        if (uploadingJointPositions && !isRigValidForJointPositionUpload())
        {
            mModelNoErrors = false;
            // <FS:Beq> improve uploader error reporting
            // LL_INFOS() << "Invalid rig, there might be issues with uploading Joint positions" << LL_ENDL;
            std::ostringstream out;
            out << "Invalid rig, there might be issues with uploading Joint positions";
            LL_INFOS() << out.str() << LL_ENDL;
            LLFloaterModelPreview::addStringToLog(out, true);
            // </FS:Beq>
        }
    }

    if (mModelNoErrors && mModelLoader)
    {
        if (!mModelLoader->areTexturesReady() && mFMP->childGetValue("upload_textures").asBoolean())
        {
            // Some textures are still loading, prevent upload until they are done
            mModelNoErrors = false;
        }
    }

    // <FS:Beq> Improve the error checking the TO DO here is no longer applicable but not an FS comment so edited to stop it being picked up
    //if (!mModelNoErrors || mHasDegenerate)
    if (!gSavedSettings.getBOOL("FSIgnoreClientsideMeshValidation") && (!mModelNoErrors || (has_physics_error > PhysicsError::NOHAVOK))) // block for all cases of phsyics error except NOHAVOK
    // </FS:Beq>
    {
        mFMP->childDisable("ok_btn");
        mFMP->childDisable("calculate_btn");
    }
    else
    {
        mFMP->childEnable("ok_btn");
        mFMP->childEnable("calculate_btn");
    }

    if (mModelNoErrors && mLodsWithParsingError.empty())
    {
        mFMP->childEnable("calculate_btn");
    }
    else
    {
        mFMP->childDisable("calculate_btn");
    }

    //add up physics triangles etc
    S32 phys_tris = 0;
    S32 phys_hulls = 0;
    S32 phys_points = 0;

    //get the triangle count for the whole scene
    for (LLModelLoader::scene::iterator iter = mScene[LLModel::LOD_PHYSICS].begin(), endIter = mScene[LLModel::LOD_PHYSICS].end(); iter != endIter; ++iter)
    {
        for (LLModelLoader::model_instance_list::iterator instance = iter->second.begin(), end_instance = iter->second.end(); instance != end_instance; ++instance)
        {
            LLModel* model = instance->mModel;
            if (model)
            {
                S32 cur_submeshes = model->getNumVolumeFaces();

                LLModel::convex_hull_decomposition& decomp = model->mPhysics.mHull;

                if (!decomp.empty())
                {
                    phys_hulls += decomp.size();
                    for (U32 i = 0; i < decomp.size(); ++i)
                    {
                        phys_points += decomp[i].size();
                    }
                }
                else
                { //choose physics shape OR decomposition, can't use both
                    for (S32 j = 0; j < cur_submeshes; ++j)
                    { //for each submesh (face), add triangles and vertices to current total
                        const LLVolumeFace& face = model->getVolumeFace(j);
                        phys_tris += face.mNumIndices / 3;
                    }
                }
            }
        }
    }

    if (phys_tris > 0)
    {
        mFMP->childSetTextArg("physics_triangles", "[TRIANGLES]", llformat("%d", phys_tris));
    }
    else
    {
        mFMP->childSetTextArg("physics_triangles", "[TRIANGLES]", mesh_status_na);
    }

    if (phys_hulls > 0)
    {
        mFMP->childSetTextArg("physics_hulls", "[HULLS]", llformat("%d", phys_hulls));
        mFMP->childSetTextArg("physics_points", "[POINTS]", llformat("%d", phys_points));
    }
    else
    {
        mFMP->childSetTextArg("physics_hulls", "[HULLS]", mesh_status_na);
        mFMP->childSetTextArg("physics_points", "[POINTS]", mesh_status_na);
    }

    LLFloaterModelPreview* fmp = LLFloaterModelPreview::sInstance;
    if (fmp)
    {
        if (phys_tris > 0 || phys_hulls > 0)
        {
            if (!fmp->isViewOptionEnabled("show_physics"))
            {
                fmp->enableViewOption("show_physics");
                mViewOption["show_physics"] = true;
                fmp->childSetValue("show_physics", true);
            }
        // <FS:Beq> handle hiding of hull only explode slider
        //}
        //else
        //{
        //    fmp->disableViewOption("show_physics");
        //    mViewOption["show_physics"] = false;
        //    fmp->childSetValue("show_physics", false);
        //}
            
            // mViewOption["show_physics"] = true; // <FS:Beq/> merge LL uploader changes
            if (phys_hulls > 0)
            {
                fmp->enableViewOption("physics_explode");
                fmp->enableViewOption("exploder_label");
                fmp->childSetVisible("physics_explode", true);
                fmp->childSetVisible("exploder_label", true);
            }
            else
            {
                fmp->disableViewOption("physics_explode");
                fmp->disableViewOption("exploder_label");
                fmp->childSetVisible("physics_explode", false);
                fmp->childSetVisible("exploder_label", false);
            }
        }
        else
        {
            fmp->disableViewOption("show_physics");
            fmp->childSetVisible("physics_explode", false);
            fmp->disableViewOption("physics_explode");
            fmp->childSetVisible("exploder_label", false);
            fmp->disableViewOption("exploder_label");
            mViewOption["show_physics"] = false;
            fmp->childSetValue("show_physics", false);

        }
        // </FS:Beq>

        //bool use_hull = fmp->childGetValue("physics_use_hull").asBoolean();

        //fmp->childSetEnabled("physics_optimize", !use_hull);

        bool enable = (phys_tris > 0 || phys_hulls > 0) && fmp->mCurRequest.empty();
        //enable = enable && !use_hull && fmp->childGetValue("physics_optimize").asBoolean();

        //enable/disable "analysis" UI
        LLPanel* panel = fmp->getChild<LLPanel>("physics analysis");
        LLView* child = panel->getFirstChild();
        while (child)
        {
            child->setEnabled(enable);
            child = panel->findNextSibling(child);
        }

        enable = phys_hulls > 0 && fmp->mCurRequest.empty();
        //enable/disable "simplification" UI
        panel = fmp->getChild<LLPanel>("physics simplification");
        child = panel->getFirstChild();
        while (child)
        {
            child->setEnabled(enable);
            child = panel->findNextSibling(child);
        }

        if (fmp->mCurRequest.empty())
        {
            fmp->childSetVisible("Simplify", true);
            fmp->childSetVisible("simplify_cancel", false);
            fmp->childSetVisible("Decompose", true);
            fmp->childSetVisible("decompose_cancel", false);

            if (phys_hulls > 0)
            {
                fmp->childEnable("Simplify");
            }

            if (phys_tris || phys_hulls > 0)
            {
                fmp->childEnable("Decompose");
            }
        }
        else
        {
            fmp->childEnable("simplify_cancel");
            fmp->childEnable("decompose_cancel");
        }
        // <FS:Beq> move the closing bracket for the if(fmp) to prevent possible crash 
        //    }


        LLCtrlSelectionInterface* iface = fmp->childGetSelectionInterface("physics_lod_combo");
        S32 which_mode = 0;
        S32 file_mode = 1;
        if (iface)
        {
            which_mode = iface->getFirstSelectedIndex();
            file_mode = iface->getItemCount() - 1;
        }

        if (which_mode == file_mode)
        {
            mFMP->childEnable("physics_file");
            mFMP->childEnable("physics_browse");
        }
        else
        {
            mFMP->childDisable("physics_file");
            mFMP->childDisable("physics_browse");
        }
    }
    // </FS:Beq>

    LLSpinCtrl* crease = mFMP->getChild<LLSpinCtrl>("crease_angle");

    if (mRequestedCreaseAngle[mPreviewLOD] == -1.f)
    {
        mFMP->childSetColor("crease_label", LLColor4::grey);
        crease->forceSetValue(75.f);
    }
    else
    {
        mFMP->childSetColor("crease_label", LLColor4::white);
        crease->forceSetValue(mRequestedCreaseAngle[mPreviewLOD]);
    }

    mModelUpdatedSignal(true);

}

void LLModelPreview::updateLodControls(S32 lod)
{
    if (lod < LLModel::LOD_IMPOSTOR || lod > LLModel::LOD_HIGH)
    {
        std::ostringstream out;
        out << "Invalid level of detail: " << lod;
        LL_WARNS() << out.str() << LL_ENDL;
        LLFloaterModelPreview::addStringToLog(out, false);
        assert(lod >= LLModel::LOD_IMPOSTOR && lod <= LLModel::LOD_HIGH);
        return;
    }

    const char* lod_controls[] =
    {
        "lod_mode_",
        "lod_triangle_limit_",
        "lod_error_threshold_"
    };
    const U32 num_lod_controls = sizeof(lod_controls) / sizeof(char*);

    const char* file_controls[] =
    {
        "lod_browse_",
        "lod_file_",
    };
    const U32 num_file_controls = sizeof(file_controls) / sizeof(char*);

    LLFloaterModelPreview* fmp = LLFloaterModelPreview::sInstance;
    if (!fmp) return;

    LLComboBox* lod_combo = mFMP->findChild<LLComboBox>("lod_source_" + lod_name[lod]);
    if (!lod_combo) return;

    S32 lod_mode = lod_combo->getCurrentIndex();
    if (lod_mode == LOD_FROM_FILE) // LoD from file
    {
        fmp->mLODMode[lod] = LOD_FROM_FILE;
        for (U32 i = 0; i < num_file_controls; ++i)
        {
            mFMP->childSetVisible(file_controls[i] + lod_name[lod], true);
        }

        for (U32 i = 0; i < num_lod_controls; ++i)
        {
            mFMP->childSetVisible(lod_controls[i] + lod_name[lod], false);
        }
    }
    else if (lod_mode == USE_LOD_ABOVE) // use LoD above
    {
        fmp->mLODMode[lod] = USE_LOD_ABOVE;
        for (U32 i = 0; i < num_file_controls; ++i)
        {
            mFMP->childSetVisible(file_controls[i] + lod_name[lod], false);
        }

        for (U32 i = 0; i < num_lod_controls; ++i)
        {
            mFMP->childSetVisible(lod_controls[i] + lod_name[lod], false);
        }

        if (lod < LLModel::LOD_HIGH)
        {
            mModel[lod] = mModel[lod + 1];
            mScene[lod] = mScene[lod + 1];
            mVertexBuffer[lod].clear();

            // Also update lower LoD
            if (lod > LLModel::LOD_IMPOSTOR)
            {
                updateLodControls(lod - 1);
            }
        }
    }
    else // auto generate, the default case for all LoDs except High
    {
        fmp->mLODMode[lod] = MESH_OPTIMIZER_AUTO;

        //don't actually regenerate lod when refreshing UI
        mLODFrozen = true;

        for (U32 i = 0; i < num_file_controls; ++i)
        {
            mFMP->getChildView(file_controls[i] + lod_name[lod])->setVisible(false);
        }

        for (U32 i = 0; i < num_lod_controls; ++i)
        {
            mFMP->getChildView(lod_controls[i] + lod_name[lod])->setVisible(true);
        }


        LLSpinCtrl* threshold = mFMP->getChild<LLSpinCtrl>("lod_error_threshold_" + lod_name[lod]);
        LLSpinCtrl* limit = mFMP->getChild<LLSpinCtrl>("lod_triangle_limit_" + lod_name[lod]);

        limit->setMaxValue(mMaxTriangleLimit);
        limit->forceSetValue(mRequestedTriangleCount[lod]);

        threshold->forceSetValue(mRequestedErrorThreshold[lod]);

        mFMP->getChild<LLComboBox>("lod_mode_" + lod_name[lod])->selectNthItem(mRequestedLoDMode[lod]);

        if (mRequestedLoDMode[lod] == 0)
        {
            limit->setVisible(true);
            threshold->setVisible(false);

            limit->setMaxValue(mMaxTriangleLimit);
            limit->setIncrement(llmax((U32)1, mMaxTriangleLimit / 32));
        }
        else
        {
            limit->setVisible(false);
            threshold->setVisible(true);
        }

        mLODFrozen = false;
    }
}

void LLModelPreview::setPreviewTarget(F32 distance)
{
    mCameraDistance = distance;
    mCameraZoom = 1.f;
    mCameraPitch = 0.f;
    mCameraYaw = 0.f;
    mCameraOffset.clearVec();
}

void LLModelPreview::clearBuffers()
{
    for (U32 i = 0; i < 6; i++)
    {
        mVertexBuffer[i].clear();
    }
}

void LLModelPreview::genBuffers(S32 lod, bool include_skin_weights)
{
    LLModelLoader::model_list* model = NULL;

    if (lod < 0 || lod > 4)
    {
        model = &mBaseModel;
        lod = 5;
    }
    else
    {
        model = &(mModel[lod]);
    }

    if (!mVertexBuffer[lod].empty())
    {
        mVertexBuffer[lod].clear();
    }

    mVertexBuffer[lod].clear();

    LLModelLoader::model_list::iterator base_iter = mBaseModel.begin();

    for (LLModelLoader::model_list::iterator iter = model->begin(); iter != model->end(); ++iter)
    {
        LLModel* mdl = *iter;
        if (!mdl)
        {
            continue;
        }

        base_iter++;

        S32 num_faces = mdl->getNumVolumeFaces();
        for (S32 i = 0; i < num_faces; ++i)
        {
            const LLVolumeFace &vf = mdl->getVolumeFace(i);
            U32 num_vertices = vf.mNumVertices;
            U32 num_indices = vf.mNumIndices;

            if (!num_vertices || !num_indices)
            {
                continue;
            }

            LLVertexBuffer* vb = NULL;

            bool skinned = include_skin_weights && !mdl->mSkinWeights.empty();

            U32 mask = LLVertexBuffer::MAP_VERTEX | LLVertexBuffer::MAP_NORMAL | LLVertexBuffer::MAP_TEXCOORD0;

            if (skinned)
            {
                mask |= LLVertexBuffer::MAP_WEIGHT4;
            }

            vb = new LLVertexBuffer(mask, 0);

            if (!vb->allocateBuffer(num_vertices, num_indices, TRUE))
            {
                // We are likely to crash due this failure, if this happens, find a way to gracefully stop preview
                std::ostringstream out;
                out << "Failed to allocate Vertex Buffer for model preview ";
                out << num_vertices << " vertices and ";
                out << num_indices << " indices";
                LL_WARNS() << out.str() << LL_ENDL;
                LLFloaterModelPreview::addStringToLog(out, true);
            }

            LLStrider<LLVector3> vertex_strider;
            LLStrider<LLVector3> normal_strider;
            LLStrider<LLVector2> tc_strider;
            LLStrider<U16> index_strider;
            // <FS:Ansariel> Vectorized Weight4Strider and ClothWeightStrider by Drake Arconis
            //LLStrider<LLVector4> weights_strider;
            LLStrider<LLVector4a> weights_strider;

            vb->getVertexStrider(vertex_strider);
            vb->getIndexStrider(index_strider);

            if (skinned)
            {
                vb->getWeight4Strider(weights_strider);
            }

            LLVector4a::memcpyNonAliased16((F32*)vertex_strider.get(), (F32*)vf.mPositions, num_vertices * 4 * sizeof(F32));

            if (vf.mTexCoords)
            {
                vb->getTexCoord0Strider(tc_strider);
                S32 tex_size = (num_vertices * 2 * sizeof(F32) + 0xF) & ~0xF;
                LLVector4a::memcpyNonAliased16((F32*)tc_strider.get(), (F32*)vf.mTexCoords, tex_size);
            }

            if (vf.mNormals)
            {
                vb->getNormalStrider(normal_strider);
                LLVector4a::memcpyNonAliased16((F32*)normal_strider.get(), (F32*)vf.mNormals, num_vertices * 4 * sizeof(F32));
            }

            if (skinned)
            {
                for (U32 i = 0; i < num_vertices; i++)
                {
                    //find closest weight to vf.mVertices[i].mPosition
                    LLVector3 pos(vf.mPositions[i].getF32ptr());

                    const LLModel::weight_list& weight_list = mdl->getJointInfluences(pos);
                    llassert(weight_list.size()>0 && weight_list.size() <= 4); // LLModel::loadModel() should guarantee this

                    LLVector4 w(0, 0, 0, 0);

                    for (U32 i = 0; i < weight_list.size(); ++i)
                    {
                        F32 wght = llclamp(weight_list[i].mWeight, 0.001f, 0.999f);
                        F32 joint = (F32)weight_list[i].mJointIdx;
                        w.mV[i] = joint + wght;
                        llassert(w.mV[i] - (S32)w.mV[i]>0.0f); // because weights are non-zero, and range of wt values
                        //should not cause floating point precision issues.
                    }

                    // <FS:Ansariel> Vectorized Weight4Strider and ClothWeightStrider by Drake Arconis
                    //*(weights_strider++) = w;
                    (*(weights_strider++)).loadua(w.mV);
                }
            }

            // build indices
            for (U32 i = 0; i < num_indices; i++)
            {
                *(index_strider++) = vf.mIndices[i];
            }

            vb->flush();

            mVertexBuffer[lod][mdl].push_back(vb);
        }
    }
}

void LLModelPreview::update()
{
    if (mGenLOD)
    {
        bool subscribe_for_generation = mLodsQuery.empty();
        mGenLOD = false;
        mDirty = true;
        mLodsQuery.clear();

        for (S32 lod = LLModel::LOD_HIGH; lod >= 0; --lod)
        {
            // adding all lods into query for generation
            mLodsQuery.push_back(lod);
        }

        if (subscribe_for_generation)
        {
            doOnIdleRepeating(lodQueryCallback);
        }
    }

    if (mDirty && mLodsQuery.empty())
    {
        mDirty = false;
        updateDimentionsAndOffsets();
        refresh();
    }
}

//-----------------------------------------------------------------------------
// createPreviewAvatar
//-----------------------------------------------------------------------------
void LLModelPreview::createPreviewAvatar(void)
{
    mPreviewAvatar = (LLVOAvatar*)gObjectList.createObjectViewer(LL_PCODE_LEGACY_AVATAR, gAgent.getRegion(), LLViewerObject::CO_FLAG_UI_AVATAR);
    if (mPreviewAvatar)
    {
        mPreviewAvatar->createDrawable(&gPipeline);
        mPreviewAvatar->mSpecialRenderMode = 1;
        mPreviewAvatar->startMotion(ANIM_AGENT_STAND);
        mPreviewAvatar->hideSkirt();
    }
    else
    {
        // <FS:Beq> improve uploader error reporting
        // LL_INFOS() << "Failed to create preview avatar for upload model window" << LL_ENDL;
        std::ostringstream out;
        out << "Failed to create preview avatar for upload model window";
        LL_INFOS() << out.str() << LL_ENDL;
        LLFloaterModelPreview::addStringToLog(out, true);
        // </FS:Beq>
    }
}

//static
U32 LLModelPreview::countRootModels(LLModelLoader::model_list models)
{
    U32 root_models = 0;
    model_list::iterator model_iter = models.begin();
    while (model_iter != models.end())
    {
        LLModel* mdl = *model_iter;
        if (mdl && mdl->mSubmodelID == 0)
        {
            root_models++;
        }
        model_iter++;
    }
    return root_models;
}

void LLModelPreview::loadedCallback(
    LLModelLoader::scene& scene,
    LLModelLoader::model_list& model_list,
    S32 lod,
    void* opaque)
{
    LLModelPreview* pPreview = static_cast< LLModelPreview* >(opaque);
    if (pPreview && !LLModelPreview::sIgnoreLoadedCallback)
    {
        // Load loader's warnings into floater's log tab
        const LLSD out = pPreview->mModelLoader->logOut();
        LLSD::array_const_iterator iter_out = out.beginArray();
        LLSD::array_const_iterator end_out = out.endArray();
        for (; iter_out != end_out; ++iter_out)
        {
            if (iter_out->has("Message"))
            {
                LLFloaterModelPreview::addStringToLog(iter_out->get("Message"), *iter_out, true, pPreview->mModelLoader->mLod);
            }
        }
        pPreview->mModelLoader->clearLog();
        pPreview->loadModelCallback(lod); // removes mModelLoader in some cases
        if (pPreview->mLookUpLodFiles && (lod != LLModel::LOD_HIGH))
        {
            pPreview->lookupLODModelFiles(lod);
        }

        const LLVOAvatar* avatarp = pPreview->getPreviewAvatar();
        if (avatarp) { // set up ground plane for possible rendering
            const LLVector3 root_pos = avatarp->mRoot->getPosition();
            const LLVector4a* ext = avatarp->mDrawable->getSpatialExtents();
            const LLVector4a min = ext[0], max = ext[1];
            const F32 center = (max[2] - min[2]) * 0.5f;
            const F32 ground = root_pos[2] - center;
            auto plane = pPreview->mGroundPlane;
            plane[0] = {min[0], min[1], ground};
            plane[1] = {max[0], min[1], ground};
            plane[2] = {max[0], max[1], ground};
            plane[3] = {min[0], max[1], ground};
        }
    }

}

void LLModelPreview::lookupLODModelFiles(S32 lod)
{
    if (lod == LLModel::LOD_PHYSICS)
    {
        mLookUpLodFiles = false;
        return;
    }
    S32 next_lod = (lod - 1 >= LLModel::LOD_IMPOSTOR) ? lod - 1 : LLModel::LOD_PHYSICS;

    std::string lod_filename = mLODFile[LLModel::LOD_HIGH];
    // <FS:Beq> BUG-230890 fix case-sensitive filename handling
    // std::string ext = ".dae";
    // LLStringUtil::toLower(lod_filename_lower);
    // std::string::size_type i = lod_filename.rfind(ext);
    // if (i != std::string::npos)
    // {
    //     lod_filename.replace(i, lod_filename.size() - ext.size(), getLodSuffix(next_lod) + ext);
    // }
    // Note: we cannot use gDirUtilp here because the getExtension forces a tolower which would then break uppercase extensions on Linux/Mac
    std::size_t offset = lod_filename.find_last_of('.');
	std::string ext = (offset == std::string::npos || offset == 0) ? "" : lod_filename.substr(offset+1);
    lod_filename = gDirUtilp->getDirName(lod_filename) + gDirUtilp->getDirDelimiter() + stripSuffix(gDirUtilp->getBaseFileName(lod_filename, true))  + getLodSuffix(next_lod) + "." + ext;
    std::ostringstream out;
    out << "Looking for file: " << lod_filename << " for LOD " << next_lod;
    LL_DEBUGS("MeshUpload") << out.str() << LL_ENDL;
    if(mImporterDebug)
    {
        LLFloaterModelPreview::addStringToLog(out, true);
    }
    out.str("");
    // </FS:Beq>
    if (gDirUtilp->fileExists(lod_filename))
    {
        // <FS:Beq> extra logging is helpful here, so add to log tab
        out << "Auto Loading LOD" << next_lod << " from " << lod_filename;
        LL_INFOS() << out.str() << LL_ENDL;
        LLFloaterModelPreview::addStringToLog(out, true);
        out.str("");
        // </FS:Beq>
        LLFloaterModelPreview* fmp = LLFloaterModelPreview::sInstance;
        if (fmp)
        {
            fmp->setCtrlLoadFromFile(next_lod);
        }
        loadModel(lod_filename, next_lod);
    }
    else
    {
        lookupLODModelFiles(next_lod);
    }
}

void LLModelPreview::stateChangedCallback(U32 state, void* opaque)
{
    LLModelPreview* pPreview = static_cast< LLModelPreview* >(opaque);
    if (pPreview)
    {
        pPreview->setLoadState(state);
    }
}

LLJoint* LLModelPreview::lookupJointByName(const std::string& str, void* opaque)
{
    LLModelPreview* pPreview = static_cast< LLModelPreview* >(opaque);
    if (pPreview)
    {
//<FS:ND> Query by JointKey rather than just a string, the key can be a U32 index for faster lookup
//        return pPreview->getPreviewAvatar()->getJoint(str);
        return pPreview->getPreviewAvatar()->getJoint( JointKey::construct( str ) );
// <FS:ND>
    }
    return NULL;
}

U32 LLModelPreview::loadTextures(LLImportMaterial& material, void* opaque)
{
    (void)opaque;

    if (material.mDiffuseMapFilename.size())
    {
        material.mOpaqueData = new LLPointer< LLViewerFetchedTexture >;
        LLPointer< LLViewerFetchedTexture >& tex = (*reinterpret_cast< LLPointer< LLViewerFetchedTexture > * >(material.mOpaqueData));

        tex = LLViewerTextureManager::getFetchedTextureFromUrl("file://" + LLURI::unescape(material.mDiffuseMapFilename), FTT_LOCAL_FILE, TRUE, LLGLTexture::BOOST_PREVIEW);
        tex->setLoadedCallback(LLModelPreview::textureLoadedCallback, 0, TRUE, FALSE, opaque, NULL, FALSE);
        tex->forceToSaveRawImage(0, F32_MAX);
        material.setDiffuseMap(tex->getID()); // record tex ID
        return 1;
    }

    material.mOpaqueData = NULL;
    return 0;
}

void LLModelPreview::addEmptyFace(LLModel* pTarget)
{
    U32 type_mask = LLVertexBuffer::MAP_VERTEX | LLVertexBuffer::MAP_NORMAL | LLVertexBuffer::MAP_TEXCOORD0;

    LLPointer<LLVertexBuffer> buff = new LLVertexBuffer(type_mask, 0);

    buff->allocateBuffer(1, 3, true);
    memset((U8*)buff->getMappedData(), 0, buff->getSize());
    // <FS:ND> Fix when running with opengl core profile
    //memset((U8*)buff->getIndicesPointer(), 0, buff->getIndicesSize());
    {
    LLStrider< U16 > index_strider;
    buff->getIndexStrider( index_strider );

    memset( (U8*)index_strider.get(), 0, buff->getIndicesSize() );
    }
    // </FS:ND>

    buff->validateRange(0, buff->getNumVerts() - 1, buff->getNumIndices(), 0);

    LLStrider<LLVector3> pos;
    LLStrider<LLVector3> norm;
    LLStrider<LLVector2> tc;
    LLStrider<U16> index;

    buff->getVertexStrider(pos);

    if (type_mask & LLVertexBuffer::MAP_NORMAL)
    {
        buff->getNormalStrider(norm);
    }
    if (type_mask & LLVertexBuffer::MAP_TEXCOORD0)
    {
        buff->getTexCoord0Strider(tc);
    }

    buff->getIndexStrider(index);

    //resize face array
    int faceCnt = pTarget->getNumVolumeFaces();
    pTarget->setNumVolumeFaces(faceCnt + 1);
    pTarget->setVolumeFaceData(faceCnt + 1, pos, norm, tc, index, buff->getNumVerts(), buff->getNumIndices());

}

//-----------------------------------------------------------------------------
// render()
//-----------------------------------------------------------------------------
// Todo: we shouldn't be setting all those UI elements on render.
// Note: Render happens each frame with skinned avatars
BOOL LLModelPreview::render()
{
    assert_main_thread();

    LLMutexLock lock(this);
    mNeedsUpdate = FALSE;

    bool edges = mViewOption["show_edges"];
    bool joint_overrides = mViewOption["show_joint_overrides"];
    bool joint_positions = mViewOption["show_joint_positions"];
    bool skin_weight = mViewOption["show_skin_weight"];
    bool textures = mViewOption["show_textures"];
    bool physics = mViewOption["show_physics"];
    bool uv_guide = mViewOption["show_uv_guide"]; // <FS:Beq> Add UV guide overlay in mesh preview

    // <FS:Beq> restore things lost by the lab during importer work
    // Extra configurability, to be exposed later as controls?
    static LLCachedControl<LLColor4> canvas_col(gSavedSettings, "MeshPreviewCanvasColor");
    static LLCachedControl<LLColor4> edge_col(gSavedSettings, "MeshPreviewEdgeColor");
    static LLCachedControl<LLColor4> base_col(gSavedSettings, "MeshPreviewBaseColor");
    static LLCachedControl<LLColor3> brightness(gSavedSettings, "MeshPreviewBrightnessColor");
    static LLCachedControl<F32> edge_width(gSavedSettings, "MeshPreviewEdgeWidth");
    static LLCachedControl<LLColor4> phys_edge_col(gSavedSettings, "MeshPreviewPhysicsEdgeColor");
    static LLCachedControl<LLColor4> phys_fill_col(gSavedSettings, "MeshPreviewPhysicsFillColor");
    static LLCachedControl<F32> phys_edge_width(gSavedSettings, "MeshPreviewPhysicsEdgeWidth");
    static LLCachedControl<LLColor4> deg_edge_col(gSavedSettings, "MeshPreviewDegenerateEdgeColor");
    static LLCachedControl<LLColor4> deg_fill_col(gSavedSettings, "MeshPreviewDegenerateFillColor");
    static LLCachedControl<F32> deg_edge_width(gSavedSettings, "MeshPreviewDegenerateEdgeWidth");
    static LLCachedControl<F32> deg_point_size(gSavedSettings, "MeshPreviewDegeneratePointSize");
    static LLCachedControl<bool> auto_enable_weight_upload(gSavedSettings, "FSMeshUploadAutoEnableWeights");
    static LLCachedControl<bool> auto_enable_show_weights(gSavedSettings, "FSMeshUploadAutoShowWeightsWhenEnabled");
    // </FS:Beq>

    S32 width = getWidth();
    S32 height = getHeight();

    LLGLSUIDefault def; // GL_BLEND, GL_ALPHA_TEST, GL_CULL_FACE, depth test
    LLGLDisable no_blend(GL_BLEND);
    LLGLEnable cull(GL_CULL_FACE);
    LLGLDepthTest depth(GL_FALSE); // SL-12781 disable z-buffer to render background color
    LLGLDisable fog(GL_FOG);

    {
        gUIProgram.bind();

        //clear background to grey
        gGL.matrixMode(LLRender::MM_PROJECTION);
        gGL.pushMatrix();
        gGL.loadIdentity();
        gGL.ortho(0.0f, width, 0.0f, height, -1.0f, 1.0f);

        gGL.matrixMode(LLRender::MM_MODELVIEW);
        gGL.pushMatrix();
        gGL.loadIdentity();

        gGL.color4fv(canvas_col().mV); // <FS:Beq/> restore changes removed by the lab
        gl_rect_2d_simple(width, height);

        gGL.matrixMode(LLRender::MM_PROJECTION);
        gGL.popMatrix();

        gGL.matrixMode(LLRender::MM_MODELVIEW);
        gGL.popMatrix();
        gUIProgram.unbind();
    }

    LLFloaterModelPreview* fmp = LLFloaterModelPreview::sInstance;

    bool has_skin_weights = false;
    bool upload_skin = mFMP->childGetValue("upload_skin").asBoolean();
    bool upload_joints = mFMP->childGetValue("upload_joints").asBoolean();

    if (upload_joints != mLastJointUpdate)
    {
        mLastJointUpdate = upload_joints;
        if (fmp)
        {
            fmp->clearAvatarTab();
        }
    }

    for (LLModelLoader::scene::iterator iter = mScene[mPreviewLOD].begin(); iter != mScene[mPreviewLOD].end(); ++iter)
    {
        for (LLModelLoader::model_instance_list::iterator model_iter = iter->second.begin(); model_iter != iter->second.end(); ++model_iter)
        {
            LLModelInstance& instance = *model_iter;
            LLModel* model = instance.mModel;
            model->mPelvisOffset = mPelvisZOffset;
            if (!model->mSkinWeights.empty())
            {
                has_skin_weights = true;
            }
        }
    }

    if (has_skin_weights && lodsReady())
    { //model has skin weights, enable view options for skin weights and joint positions
        U32 flags = getLegacyRigFlags();
        if (fmp)
        {
            if (flags == LEGACY_RIG_OK)
            {
                if (mFirstSkinUpdate)
                {
                    // auto enable weight upload if weights are present
                    // (note: all these UI updates need to be somewhere that is not render)
                    // <FS:Beq> BUG-229632 auto enable weights slows manual workflow
                    // fmp->childSetValue("upload_skin", true);
                    LL_DEBUGS("MeshUpload") << "FSU auto_enable_weights_upload = " << auto_enable_weight_upload() << LL_ENDL;
                    LL_DEBUGS("MeshUpload") << "FSU auto_enable_show_weights = " << auto_enable_show_weights() << LL_ENDL;
                    upload_skin=auto_enable_weight_upload();
                    fmp->childSetValue("upload_skin", upload_skin);

                    skin_weight = upload_skin && auto_enable_show_weights(); 
                    mViewOption["show_skin_weight"] = skin_weight;
                    mFMP->childSetValue("show_skin_weight", skin_weight);
                    fmp->setViewOptionEnabled("show_skin_weight", upload_skin);
                    fmp->setViewOptionEnabled("show_joint_overrides", upload_skin);
                    fmp->setViewOptionEnabled("show_joint_positions", upload_skin);
                    // </FS:Beq>
                    mFirstSkinUpdate = false;
                    upload_skin = true;
                    skin_weight = true;
                    mViewOption["show_skin_weight"] = true;
                }
                // <FS:Beq> BUG-229632 auto enable weights slows manual workflow
                // fmp->enableViewOption("show_skin_weight");
                // fmp->setViewOptionEnabled("show_joint_overrides", skin_weight);
                // fmp->setViewOptionEnabled("show_joint_posi
                else
                {
                    LL_DEBUGS("MeshUpload") << "NOT FSU auto_enable_weights_upload = " << auto_enable_weight_upload() << LL_ENDL;
                    LL_DEBUGS("MeshUpload") << "NOT FSU auto_enable_show_weights = " << auto_enable_show_weights() << LL_ENDL;
                    fmp->setViewOptionEnabled("show_skin_weight", upload_skin);
                    fmp->setViewOptionEnabled("show_joint_overrides", upload_skin);
                    fmp->setViewOptionEnabled("show_joint_positions", upload_skin);
                }
                // </FS:Beq>
                mFMP->childEnable("upload_skin");
                // mFMP->childSetValue("show_skin_weight", skin_weight); // <FS:Beq/> BUG-229632 

            }
            else if ((flags & LEGACY_RIG_FLAG_TOO_MANY_JOINTS) > 0)
            {
                mFMP->childSetVisible("skin_too_many_joints", true);
            }
            else if ((flags & LEGACY_RIG_FLAG_UNKNOWN_JOINT) > 0)
            {
                mFMP->childSetVisible("skin_unknown_joint", true);
            }
            // <FS:Beq> defensive code to wanr for incorrect flags - no behavioural change
            else
            {
                LL_WARNS("MeshUpload") << "Unexpected flags combination on weights check" << LL_ENDL;
            }
            // </FS:Beq>
        }
    }
    else
    {
        mFMP->childDisable("upload_skin");
        if (fmp)
        {
            mViewOption["show_skin_weight"] = false;
            fmp->disableViewOption("show_skin_weight");
            fmp->disableViewOption("show_joint_overrides");
            fmp->disableViewOption("show_joint_positions");

            skin_weight = false;
            mFMP->childSetValue("show_skin_weight", false);
            fmp->setViewOptionEnabled("show_skin_weight", skin_weight);
        }
    }

    if (upload_skin && !has_skin_weights)
    { //can't upload skin weights if model has no skin weights
        mFMP->childSetValue("upload_skin", false);
        upload_skin = false;
    }

    if (!upload_skin && upload_joints)
    { //can't upload joints if not uploading skin weights
        mFMP->childSetValue("upload_joints", false);
        upload_joints = false;
    }

    if (fmp)
    {
        if (upload_skin)
        {
            // will populate list of joints
            fmp->updateAvatarTab(upload_joints);
        }
        else
        {
            fmp->clearAvatarTab();
        }
    }

    if (upload_skin && upload_joints)
    {
        mFMP->childEnable("lock_scale_if_joint_position");
    }
    else
    {
        mFMP->childDisable("lock_scale_if_joint_position");
        mFMP->childSetValue("lock_scale_if_joint_position", false);
    }

    //Only enable joint offsets if it passed the earlier critiquing
    if (isRigValidForJointPositionUpload())
    {
        mFMP->childSetEnabled("upload_joints", upload_skin);
    }

    F32 explode = mFMP->childGetValue("physics_explode").asReal();

    LLGLDepthTest gls_depth(GL_TRUE); // SL-12781 re-enable z-buffer for 3D model preview

    LLRect preview_rect;

    preview_rect = mFMP->getChildView("preview_panel")->getRect();

    F32 aspect = (F32)preview_rect.getWidth() / preview_rect.getHeight();

    LLViewerCamera::getInstance()->setAspect(aspect);

    LLViewerCamera::getInstance()->setView(LLViewerCamera::getInstance()->getDefaultFOV() / mCameraZoom);

    LLVector3 offset = mCameraOffset;
    LLVector3 target_pos = mPreviewTarget + offset;

    F32 z_near = 0.001f;
    F32 z_far = mCameraDistance*10.0f + mPreviewScale.magVec() + mCameraOffset.magVec();

    if (skin_weight)
    {
        target_pos = getPreviewAvatar()->getPositionAgent() + offset;
        z_near = 0.01f;
        z_far = 1024.f;

        //render avatar previews every frame
        refresh();
    }

    gObjectPreviewProgram.bind();

    gGL.loadIdentity();
    gPipeline.enableLightsPreview();
    gObjectPreviewProgram.uniform4fv(LLShaderMgr::AMBIENT, 1, LLPipeline::PreviewAmbientColor.mV); // <FS:Beq> pass ambient setting to shader

    LLQuaternion camera_rot = LLQuaternion(mCameraPitch, LLVector3::y_axis) *
        LLQuaternion(mCameraYaw, LLVector3::z_axis);

    LLQuaternion av_rot = camera_rot;
    F32 camera_distance = skin_weight ? SKIN_WEIGHT_CAMERA_DISTANCE : mCameraDistance;
    LLViewerCamera::getInstance()->setOriginAndLookAt(
        target_pos + ((LLVector3(camera_distance, 0.f, 0.f) + offset) * av_rot),		// camera
        LLVector3::z_axis,																	// up
        target_pos);											// point of interest


    z_near = llclamp(z_far * 0.001f, 0.001f, 0.1f);

    LLViewerCamera::getInstance()->setPerspective(FALSE, mOrigin.mX, mOrigin.mY, width, height, FALSE, z_near, z_far);

    stop_glerror();

    gGL.pushMatrix();
    gGL.color4fv(edge_col().mV); // <FS:Beq/> restore changes removed by the lab

    const U32 type_mask = LLVertexBuffer::MAP_VERTEX | LLVertexBuffer::MAP_NORMAL | LLVertexBuffer::MAP_TEXCOORD0;

    LLGLEnable normalize(GL_NORMALIZE);

    if (!mBaseModel.empty() && mVertexBuffer[5].empty())
    {
        genBuffers(-1, skin_weight);
        //genBuffers(3);
    }

    if (!mModel[mPreviewLOD].empty())
    {
        mFMP->childEnable("reset_btn");

        bool regen = mVertexBuffer[mPreviewLOD].empty();
        if (!regen)
        {
            const std::vector<LLPointer<LLVertexBuffer> >& vb_vec = mVertexBuffer[mPreviewLOD].begin()->second;
            if (!vb_vec.empty())
            {
                const LLVertexBuffer* buff = vb_vec[0];
                regen = buff->hasDataType(LLVertexBuffer::TYPE_WEIGHT4) != skin_weight;
            }
            else
            {
                LL_INFOS() << "Vertex Buffer[" << mPreviewLOD << "]" << " is EMPTY!!!" << LL_ENDL;
                regen = TRUE;
            }
        }

        if (regen)
        {
            genBuffers(mPreviewLOD, skin_weight);
        }

        if (physics && mVertexBuffer[LLModel::LOD_PHYSICS].empty())
        {
            genBuffers(LLModel::LOD_PHYSICS, false);
        }

        if (!skin_weight)
        {
            for (LLMeshUploadThread::instance_list::iterator iter = mUploadData.begin(); iter != mUploadData.end(); ++iter)
            {
                LLModelInstance& instance = *iter;

                LLModel* model = instance.mLOD[mPreviewLOD];

                if (!model)
                {
                    continue;
                }

                gGL.pushMatrix();
                LLMatrix4 mat = instance.mTransform;

                gGL.multMatrix((GLfloat*)mat.mMatrix);


                U32 num_models = mVertexBuffer[mPreviewLOD][model].size();
                for (U32 i = 0; i < num_models; ++i)
                {
                    LLVertexBuffer* buffer = mVertexBuffer[mPreviewLOD][model][i];

                    buffer->setBuffer(type_mask & buffer->getTypeMask());

                    if (textures)
                    {
                        int materialCnt = instance.mModel->mMaterialList.size();
                        if (i < materialCnt)
                        {
                            const std::string& binding = instance.mModel->mMaterialList[i];
                            const LLImportMaterial& material = instance.mMaterial[binding];

                            gGL.diffuseColor4fv(material.mDiffuseColor.mV);

                            // Find the tex for this material, bind it, and add it to our set
                            //
                            LLViewerFetchedTexture* tex = bindMaterialDiffuseTexture(material);
                            if (tex)
                            {
                                mTextureSet.insert(tex);
                            }
                        }
                    }
                    // <FS:Beq> improved mesh uploader
                    else if (uv_guide)
                    {
                        if(mUVGuideTexture)
                        {
                            if (mUVGuideTexture->getDiscardLevel() > -1)
                            {
                                gGL.getTexUnit(0)->bind(mUVGuideTexture, true);
                            }
                        }
                        gGL.diffuseColor4fv(base_col().mV); // <FS:Beq/> restore changes removed by the lab

                    }
                    // </FS:Beq>
                    else
                    {
                        gGL.diffuseColor4fv(base_col().mV); // <FS:Beq/> restore changes removed by the lab
                    }

                    buffer->drawRange(LLRender::TRIANGLES, 0, buffer->getNumVerts() - 1, buffer->getNumIndices(), 0);
                    gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
                    gGL.diffuseColor4fv(edge_col().mV); // <FS:Beq/> restore changes removed by the lab
                    if (edges)
                    {
                        gGL.setLineWidth(edge_width()); // <FS:Beq/> restore changes removed by the lab
                        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
                        buffer->drawRange(LLRender::TRIANGLES, 0, buffer->getNumVerts() - 1, buffer->getNumIndices(), 0);
                        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
                        gGL.setLineWidth(1.f); // <FS> Line width OGL core profile fix by Rye Mutt
                    }
                    buffer->flush();
                }
                gGL.popMatrix();
            }

            if (physics)
            {
                glClear(GL_DEPTH_BUFFER_BIT);

                for (U32 pass = 0; pass < 2; pass++)
                {
                    if (pass == 0)
                    { //depth only pass
                        gGL.setColorMask(false, false);
                    }
                    else
                    {
                        gGL.setColorMask(true, true);
                    }

                    //enable alpha blending on second pass but not first pass
                    LLGLState blend(GL_BLEND, pass);

                    gGL.blendFunc(LLRender::BF_SOURCE_ALPHA, LLRender::BF_ONE_MINUS_SOURCE_ALPHA);

                    for (LLMeshUploadThread::instance_list::iterator iter = mUploadData.begin(); iter != mUploadData.end(); ++iter)
                    {
                        LLModelInstance& instance = *iter;

                        LLModel* model = instance.mLOD[LLModel::LOD_PHYSICS];

                        if (!model)
                        {
                            continue;
                        }

                        gGL.pushMatrix();
                        LLMatrix4 mat = instance.mTransform;

                        gGL.multMatrix((GLfloat*)mat.mMatrix);


                        bool render_mesh = true;
                        LLPhysicsDecomp* decomp = gMeshRepo.mDecompThread;
                        if (decomp)
                        {
                            LLMutexLock(decomp->mMutex);

                            LLModel::Decomposition& physics = model->mPhysics;

                            if (!physics.mHull.empty())
                            {
                                render_mesh = false;

                                if (physics.mMesh.empty())
                                { //build vertex buffer for physics mesh
                                    gMeshRepo.buildPhysicsMesh(physics);
                                }

                                if (!physics.mMesh.empty())
                                { //render hull instead of mesh
                                    // SL-16993 physics.mMesh[i].mNormals were being used to light the exploded
                                    // analyzed physics shape but the drawArrays() interface changed
                                    //  causing normal data <0,0,0> to be passed to the shader.
                                    // The Phyics Preview shader uses plain vertex coloring so the physics hull is full lit.
                                    // We could also use interface/ui shaders.
                                    gObjectPreviewProgram.unbind();
                                    gPhysicsPreviewProgram.bind();

                                    for (U32 i = 0; i < physics.mMesh.size(); ++i)
                                    {
                                        if (explode > 0.f)
                                        {
                                            gGL.pushMatrix();

                                            LLVector3 offset = model->mHullCenter[i] - model->mCenterOfHullCenters;
                                            offset *= explode;

                                            gGL.translatef(offset.mV[0], offset.mV[1], offset.mV[2]);
                                        }

                                        static std::vector<LLColor4U> hull_colors;

                                        if (i + 1 >= hull_colors.size())
                                        {
                                            hull_colors.push_back(LLColor4U(rand() % 128 + 127, rand() % 128 + 127, rand() % 128 + 127, 128));
                                        }

                                        gGL.diffuseColor4ubv(hull_colors[i].mV);
                                        LLVertexBuffer::drawArrays(LLRender::TRIANGLES, physics.mMesh[i].mPositions);

                                        if (explode > 0.f)
                                        {
                                            gGL.popMatrix();
                                        }
                                    }

                                    gPhysicsPreviewProgram.unbind();
                                    gObjectPreviewProgram.bind();
                                }
                            }
                        }

                        if (render_mesh)
                        {
                            U32 num_models = mVertexBuffer[LLModel::LOD_PHYSICS][model].size();
                            if (pass > 0){
                                for (U32 i = 0; i < num_models; ++i)
                                {
                                    LLVertexBuffer* buffer = mVertexBuffer[LLModel::LOD_PHYSICS][model][i];

                                    gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
                                    gGL.diffuseColor4fv(phys_fill_col().mV); // <FS:Beq/> restore changes removed by the lab

                                    buffer->setBuffer(type_mask & buffer->getTypeMask());
                                    buffer->drawRange(LLRender::TRIANGLES, 0, buffer->getNumVerts() - 1, buffer->getNumIndices(), 0);
                                    // <FS:Beq> restore changes removed by the lab
                                    // gGL.diffuseColor4fv(PREVIEW_PSYH_EDGE_COL.mV);
                                    // gGL.setLineWidth(PREVIEW_PSYH_EDGE_WIDTH); // <FS> Line width OGL core profile fix by Rye Mutt
                                    gGL.diffuseColor4fv(phys_edge_col().mV);
                                    gGL.setLineWidth(phys_edge_width());
                                    // </FS:Beq> 
                                    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
                                    buffer->drawRange(LLRender::TRIANGLES, 0, buffer->getNumVerts() - 1, buffer->getNumIndices(), 0);

                                    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
                                    gGL.setLineWidth(1.f); // <FS> Line width OGL core profile fix by Rye Mutt

                                    buffer->flush();
                                }
                            }
                        }
                        gGL.popMatrix();
                    }

                    // only do this if mDegenerate was set in the preceding mesh checks [Check this if the ordering ever breaks]
                    if (mHasDegenerate)
                    {
                        // <FS:Beq> restore older functionality lost in lab importer
                        // gGL.setLineWidth(PREVIEW_DEG_EDGE_WIDTH); // <FS> Line width OGL core profile fix by Rye Mutt
                        // glPointSize(PREVIEW_DEG_POINT_SIZE);
                        // gPipeline.enableLightsFullbright();
                        gGL.setLineWidth(deg_edge_width());
                        glPointSize(deg_point_size());
                        // gPipeline.enableLightsFullbright(); // This may need to be restored when I fined the cause of the black rendering
                        // </FS:Beq>
                        //show degenerate triangles
                        LLGLDepthTest depth(GL_TRUE, GL_TRUE, GL_ALWAYS);
                        LLGLDisable cull(GL_CULL_FACE);
                        
                        // gGL.diffuseColor4f(1.f, 0.f, 0.f, 1.f); // <FS:Beq/> restore proper functionality
                        const LLVector4a scale(0.5f);

                        for (LLMeshUploadThread::instance_list::iterator iter = mUploadData.begin(); iter != mUploadData.end(); ++iter)
                        {
                            LLModelInstance& instance = *iter;

                            LLModel* model = instance.mLOD[LLModel::LOD_PHYSICS];

                            if (!model)
                            {
                                continue;
                            }

                            gGL.pushMatrix();
                            LLMatrix4 mat = instance.mTransform;

                            gGL.multMatrix((GLfloat*)mat.mMatrix);


                            LLPhysicsDecomp* decomp = gMeshRepo.mDecompThread;
                            if (decomp)
                            {
                                LLMutexLock(decomp->mMutex);

                                LLModel::Decomposition& physics = model->mPhysics;

                                if (physics.mHull.empty())
                                {
                                    U32 num_models = mVertexBuffer[LLModel::LOD_PHYSICS][model].size();
                                    for (U32 v = 0; v < num_models; ++v)
                                    {
                                        LLVertexBuffer* buffer = mVertexBuffer[LLModel::LOD_PHYSICS][model][v];

                                        buffer->setBuffer(type_mask & buffer->getTypeMask());

                                        LLStrider<LLVector3> pos_strider;
                                        buffer->getVertexStrider(pos_strider, 0);
                                        LLVector4a* pos = (LLVector4a*)pos_strider.get();

                                        LLStrider<U16> idx;
                                        buffer->getIndexStrider(idx, 0);

                                        for (U32 i = 0; i < buffer->getNumIndices(); i += 3)
                                        {
                                            LLVector4a v1; v1.setMul(pos[*idx++], scale);
                                            LLVector4a v2; v2.setMul(pos[*idx++], scale);
                                            LLVector4a v3; v3.setMul(pos[*idx++], scale);

                                            if (ll_is_degenerate(v1, v2, v3))
                                            {
                                                // <FS:Beq> restore (configurable) coloured overlay
                                                glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
                                                gGL.diffuseColor4fv(deg_fill_col().mV);
                                                buffer->draw(LLRender::TRIANGLES, 3, i);
                                                glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
                                                gGL.diffuseColor3fv(deg_edge_col().mV);
                                                gGL.color3fv(deg_edge_col().mV);
                                                // </FS:Beq>
                                                buffer->draw(LLRender::LINE_LOOP, 3, i);
                                                buffer->draw(LLRender::POINTS, 3, i);
                                            }
                                        }

                                        buffer->flush();
                                    }
                                }
                            }

                            gGL.popMatrix();
                        }
                        gGL.setLineWidth(1.f); // <FS> Line width OGL core profile fix by Rye Mutt
                        glPointSize(1.f);
                        gPipeline.enableLightsPreview();
                        gGL.setSceneBlendType(LLRender::BT_ALPHA);
                    }
                }
            }
        }
        else
        {
            target_pos = getPreviewAvatar()->getPositionAgent();
            getPreviewAvatar()->clearAttachmentOverrides(); // removes pelvis fixup
            LLUUID fake_mesh_id;
            fake_mesh_id.generate();
            getPreviewAvatar()->addPelvisFixup(mPelvisZOffset, fake_mesh_id);
            bool pelvis_recalc = false;

            LLViewerCamera::getInstance()->setOriginAndLookAt(
                target_pos + ((LLVector3(camera_distance, 0.f, 0.f) + offset) * av_rot),		// camera
                LLVector3::z_axis,																	// up
                target_pos);											// point of interest

            for (LLModelLoader::scene::iterator iter = mScene[mPreviewLOD].begin(); iter != mScene[mPreviewLOD].end(); ++iter)
            {
                for (LLModelLoader::model_instance_list::iterator model_iter = iter->second.begin(); model_iter != iter->second.end(); ++model_iter)
                {
                    LLModelInstance& instance = *model_iter;
                    LLModel* model = instance.mModel;

                    if (!model->mSkinWeights.empty())
                    {
                        const LLMeshSkinInfo *skin = &model->mSkinInfo;
                        LLSkinningUtil::initJointNums(&model->mSkinInfo, getPreviewAvatar());// inits skin->mJointNums if nessesary
                        U32 joint_count = LLSkinningUtil::getMeshJointCount(skin);
                        U32 bind_count = skin->mAlternateBindMatrix.size();

                        if (joint_overrides
                            && bind_count > 0
                            && joint_count == bind_count)
                        {
                            // mesh_id is used to determine which mesh gets to
                            // set the joint offset, in the event of a conflict. Since
                            // we don't know the mesh id yet, we can't guarantee that
                            // joint offsets will be applied with the same priority as
                            // in the uploaded model. If the file contains multiple
                            // meshes with conflicting joint offsets, preview may be
                            // incorrect.
                            LLUUID fake_mesh_id;
                            fake_mesh_id.generate();
                            for (U32 j = 0; j < joint_count; ++j)
                            {
                                LLJoint *joint = getPreviewAvatar()->getJoint(skin->mJointNums[j]);
                                if (joint)
                                {
                                    const LLVector3& jointPos = LLVector3(skin->mAlternateBindMatrix[j].getTranslation());
                                    if (joint->aboveJointPosThreshold(jointPos))
                                    {
                                        bool override_changed;
                                        joint->addAttachmentPosOverride(jointPos, fake_mesh_id, "model", override_changed);

                                        if (override_changed)
                                        {
                                            //If joint is a pelvis then handle old/new pelvis to foot values
                                            if (joint->getName() == "mPelvis")// or skin->mJointNames[j]
                                            {
                                                pelvis_recalc = true;
                                            }
                                        }
                                        if (skin->mLockScaleIfJointPosition)
                                        {
                                            // Note that unlike positions, there's no threshold check here,
                                            // just a lock at the default value.
                                            joint->addAttachmentScaleOverride(joint->getDefaultScale(), fake_mesh_id, "model");
                                        }
                                    }
                                }
                            }
                        }

                        for (U32 i = 0, e = mVertexBuffer[mPreviewLOD][model].size(); i < e; ++i)
                        {
                            LLVertexBuffer* buffer = mVertexBuffer[mPreviewLOD][model][i];

                            const LLVolumeFace& face = model->getVolumeFace(i);

                            LLStrider<LLVector3> position;
                            buffer->getVertexStrider(position);

                            // <FS:Ansariel> Vectorized Weight4Strider and ClothWeightStrider by Drake Arconis
                            //LLStrider<LLVector4> weight;
                            LLStrider<LLVector4a> weight;
                            buffer->getWeight4Strider(weight);

                            //quick 'n dirty software vertex skinning

                            //build matrix palette

                            LLMatrix4a mat[LL_MAX_JOINTS_PER_MESH_OBJECT];
                            LLSkinningUtil::initSkinningMatrixPalette(mat, joint_count,
                                skin, getPreviewAvatar());

                            const LLMatrix4a& bind_shape_matrix = skin->mBindShapeMatrix;
                            U32 max_joints = LLSkinningUtil::getMaxJointCount();
                            for (U32 j = 0; j < buffer->getNumVerts(); ++j)
                            {
                                LLMatrix4a final_mat;
                                // <FS:Ansariel> Vectorized Weight4Strider and ClothWeightStrider by Drake Arconis
                                //F32 *wptr = weight[j].mV;
                                F32 *wptr = weight[j].getF32ptr();
                                // </FS:Ansariel>
                                LLSkinningUtil::getPerVertexSkinMatrix(wptr, mat, true, final_mat, max_joints);

                                //VECTORIZE THIS
                                LLVector4a& v = face.mPositions[j];

                                LLVector4a t;
                                LLVector4a dst;
                                bind_shape_matrix.affineTransform(v, t);
                                final_mat.affineTransform(t, dst);

                                position[j][0] = dst[0];
                                position[j][1] = dst[1];
                                position[j][2] = dst[2];
                            }

                            // <FS:ND> FIRE-13465 Make sure there's a material set before dereferencing it
                            if( instance.mModel->mMaterialList.size() > i &&
                                instance.mMaterial.end() != instance.mMaterial.find( instance.mModel->mMaterialList[ i ] ) )
                            {
                            // </FS:ND>
                            llassert(model->mMaterialList.size() > i);
                            const std::string& binding = instance.mModel->mMaterialList[i];
                            const LLImportMaterial& material = instance.mMaterial[binding];

                            buffer->setBuffer(type_mask & buffer->getTypeMask());
                            gGL.diffuseColor4fv(material.mDiffuseColor.mV);
                            gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);

                            // Find the tex for this material, bind it, and add it to our set
                            //
                            LLViewerFetchedTexture* tex = bindMaterialDiffuseTexture(material);
                            if (tex)
                            {
                                mTextureSet.insert(tex);
                            }
                            } else  // <FS:ND> FIRE-13465 Make sure there's a material set before dereferencing it, if none, set buffer type and unbind texture.
                            {
                                buffer->setBuffer(type_mask & buffer->getTypeMask());
                                gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
                            } // </FS:ND>

                            buffer->draw(LLRender::TRIANGLES, buffer->getNumIndices(), 0);

                            if (edges)
                            {
                                // <FS:Beq> restore behaviour removed by lab
                                // gGL.diffuseColor4fv(PREVIEW_EDGE_COL.mV);
                                // gGL.setLineWidth(PREVIEW_EDGE_WIDTH);
                                gGL.diffuseColor4fv(edge_col().mV);
                                gGL.setLineWidth(edge_width());
                                // </FS:Beq>
                                glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
                                buffer->draw(LLRender::TRIANGLES, buffer->getNumIndices(), 0);
                                glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
                                gGL.setLineWidth(1.f); // <FS> Line width OGL core profile fix by Rye Mutt
                            }
                        }
                    }
                }
            }

            if (joint_positions)
            {
                LLGLSLShader* shader = LLGLSLShader::sCurBoundShaderPtr;
                if (shader)
                {
                    gDebugProgram.bind();
                }
                getPreviewAvatar()->renderCollisionVolumes();
                if (fmp->mTabContainer->getCurrentPanelIndex() == fmp->mAvatarTabIndex)
                {
                    getPreviewAvatar()->renderBones(fmp->mSelectedJointName);
                }
                else
                {
                    getPreviewAvatar()->renderBones();
                }
                renderGroundPlane(mPelvisZOffset);
                if (shader)
                {
                    shader->bind();
                }
            }

            if (pelvis_recalc)
            {
                // size/scale recalculation
                getPreviewAvatar()->postPelvisSetRecalc();
            }
        }
    }

    gObjectPreviewProgram.unbind();

    gGL.popMatrix();

    return TRUE;
}

void LLModelPreview::renderGroundPlane(float z_offset)
{   // Not necesarilly general - beware - but it seems to meet the needs of LLModelPreview::render

	gGL.diffuseColor3f( 1.0f, 0.0f, 1.0f );

	gGL.begin(LLRender::LINES);
	gGL.vertex3fv(mGroundPlane[0].mV);
	gGL.vertex3fv(mGroundPlane[1].mV);

	gGL.vertex3fv(mGroundPlane[1].mV);
	gGL.vertex3fv(mGroundPlane[2].mV);

	gGL.vertex3fv(mGroundPlane[2].mV);
	gGL.vertex3fv(mGroundPlane[3].mV);

	gGL.vertex3fv(mGroundPlane[3].mV);
	gGL.vertex3fv(mGroundPlane[0].mV);

	gGL.end();
}


//-----------------------------------------------------------------------------
// refresh()
//-----------------------------------------------------------------------------
void LLModelPreview::refresh()
{
    mNeedsUpdate = TRUE;
}

//-----------------------------------------------------------------------------
// rotate()
//-----------------------------------------------------------------------------
void LLModelPreview::rotate(F32 yaw_radians, F32 pitch_radians)
{
    mCameraYaw = mCameraYaw + yaw_radians;

    mCameraPitch = llclamp(mCameraPitch + pitch_radians, F_PI_BY_TWO * -0.8f, F_PI_BY_TWO * 0.8f);
}

//-----------------------------------------------------------------------------
// zoom()
//-----------------------------------------------------------------------------
void LLModelPreview::zoom(F32 zoom_amt)
{
    F32 new_zoom = mCameraZoom + zoom_amt;
    // TODO: stop clamping in render
    // <FS:Beq> restore settings control
    // mCameraZoom = llclamp(new_zoom, 1.f, PREVIEW_ZOOM_LIMIT); 
    static LLCachedControl<F32> zoom_limit(gSavedSettings, "MeshPreviewZoomLimit");
    mCameraZoom = llclamp(new_zoom, 1.f, zoom_limit());
    // </FS:Beq>
}

void LLModelPreview::pan(F32 right, F32 up)
{
    bool skin_weight = mViewOption["show_skin_weight"];
    F32 camera_distance = skin_weight ? SKIN_WEIGHT_CAMERA_DISTANCE : mCameraDistance;
    mCameraOffset.mV[VY] = llclamp(mCameraOffset.mV[VY] + right * camera_distance / mCameraZoom, -1.f, 1.f);
    mCameraOffset.mV[VZ] = llclamp(mCameraOffset.mV[VZ] + up * camera_distance / mCameraZoom, -1.f, 1.f);
}

void LLModelPreview::setPreviewLOD(S32 lod)
{
    lod = llclamp(lod, 0, (S32)LLModel::LOD_HIGH);

    if (lod != mPreviewLOD)
    {
        mPreviewLOD = lod;

        LLComboBox* combo_box = mFMP->getChild<LLComboBox>("preview_lod_combo");
        combo_box->setCurrentByIndex((NUM_LOD - 1) - mPreviewLOD); // combo box list of lods is in reverse order
        mFMP->childSetValue("lod_file_" + lod_name[mPreviewLOD], mLODFile[mPreviewLOD]);

        LLColor4 highlight_color = LLUIColorTable::instance().getColor("MeshImportTableHighlightColor");
        LLColor4 normal_color = LLUIColorTable::instance().getColor("MeshImportTableNormalColor");

        for (S32 i = 0; i <= LLModel::LOD_HIGH; ++i)
        {
            const LLColor4& color = (i == lod) ? highlight_color : normal_color;

            mFMP->childSetColor(lod_status_name[i], color);
            mFMP->childSetColor(lod_label_name[i], color);
            mFMP->childSetColor(lod_triangles_name[i], color);
            mFMP->childSetColor(lod_vertices_name[i], color);
        }

        LLFloaterModelPreview* fmp = (LLFloaterModelPreview*)mFMP;
        if (fmp)
        {
            // make preview repopulate tab
            fmp->clearAvatarTab();
        }
    }
    refresh();
    updateStatusMessages();
}

//static
void LLModelPreview::textureLoadedCallback(
    BOOL success,
    LLViewerFetchedTexture *src_vi,
    LLImageRaw* src,
    LLImageRaw* src_aux,
    S32 discard_level,
    BOOL final,
    void* userdata)
{
    LLModelPreview* preview = (LLModelPreview*)userdata;
    preview->refresh();

    if (final && preview->mModelLoader)
    {
        if (preview->mModelLoader->mNumOfFetchingTextures > 0)
        {
            preview->mModelLoader->mNumOfFetchingTextures--;
        }
    }
}

// static
bool LLModelPreview::lodQueryCallback()
{
    // not the best solution, but model preview belongs to floater
    // so it is an easy way to check that preview still exists.
    LLFloaterModelPreview* fmp = LLFloaterModelPreview::sInstance;
    if (fmp && fmp->mModelPreview)
    {
        LLModelPreview* preview = fmp->mModelPreview;
        if (preview->mLodsQuery.size() > 0)
        {
            S32 lod = preview->mLodsQuery.back();
            preview->mLodsQuery.pop_back();
// <FS:Beq> Improved LOD generation
#ifdef USE_GLOD_AS_DEFAULT
            preview->genGlodLODs(lod, 3, false);
#else
            preview->genMeshOptimizerLODs(lod, MESH_OPTIMIZER_AUTO, 3, false);
#endif
// </FS:Beq>
            if (preview->mLookUpLodFiles && (lod == LLModel::LOD_HIGH))
            {
                preview->lookupLODModelFiles(LLModel::LOD_HIGH);
            }

            // return false to continue cycle
            return preview->mLodsQuery.empty();
        }
    }
    // nothing to process
    return true;
}

// <FS:Beq> Improved LOD generation
void LLModelPreview::onLODGLODParamCommit(S32 lod, bool enforce_tri_limit)
{
    if (mFMP && !mLODFrozen)
    {
        genGlodLODs(lod, 3, enforce_tri_limit);
        mFMP->refresh();
        refresh();
        mDirty = true;
    }

}
void LLModelPreview::onLODMeshOptimizerParamCommit(S32 requested_lod, bool enforce_tri_limit, S32 mode)
{
    if (mFMP && !mLODFrozen) // <FS:Beq> minor sidestep of potential crash
    {
        genMeshOptimizerLODs(requested_lod, mode, 3, enforce_tri_limit);
        mFMP->refresh(); // <FS:Beq/> BUG-231970 Fix b0rken upload floater refresh
        refresh();
        mDirty = true;
    }
}

