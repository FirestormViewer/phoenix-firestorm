/**
 * @file vjlocalmesh.cpp
 * @author Vaalith Jinn
 * @brief Local Mesh main mechanism source
 *
 * $LicenseInfo:firstyear=2022&license=viewerlgpl$
 * Local Mesh contribution source code
 * Copyright (C) 2022, Vaalith Jinn.
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

// linden headers
#include "llviewerprecompiledheaders.h"
#include "llcallbacklist.h"
#include "llsdutil.h"
#include "llviewerobjectlist.h"
#include "llvovolume.h"
#include "llmeshrepository.h"
#include "llvolumemgr.h"
#include "pipeline.h"
#include "llviewercontrol.h"
#include "llmodelpreview.h"

// STL headers
#include <chrono>
// boost headers
#include "fix_macros.h"
#include <boost/filesystem.hpp>

// local mesh headers
#include "vjlocalmesh.h"
#include "vjfloaterlocalmesh.h"

// local mesh importers
#include "vjlocalmeshimportdae.h"


/*==========================================*/
/*  LLLocalMeshFace: aka submesh denoted by */
/*  material assignment. holds per-face -   */
/*  values for indices, bounding box and    */
/*  vtx pos, normals, uv coords, weights.   */
/*==========================================*/
void LLLocalMeshFace::setFaceBoundingBox(LLVector4 data_in, bool initial_values)
{
    if (initial_values)
    {
        mFaceBoundingBox.first = data_in;
        mFaceBoundingBox.second = data_in;
        return;
    }

    for (S32 array_iter = 0; array_iter < 4; ++array_iter)
    {
        mFaceBoundingBox.first[array_iter] = std::min(mFaceBoundingBox.first[array_iter], data_in[array_iter]);
        mFaceBoundingBox.second[array_iter] = std::max(mFaceBoundingBox.second[array_iter], data_in[array_iter]);
    }
}

void LLLocalMeshFace::logFaceInfo() const
{
    // log all of the attribute of the face using LL_DEBUGS("LocalMesh")
    LL_DEBUGS("LocalMesh") << "LLLocalMeshFace: " << LL_ENDL;
    LL_DEBUGS("LocalMesh") << "  mFaceBoundingBox: [" << mFaceBoundingBox.first << "," << mFaceBoundingBox.second << LL_ENDL;
    // create a string stream from mIndices then output it to the log
    std::stringstream ss;
    for (const auto& index : mIndices)
    {
        ss << "[";
        ss << index << ",";
        ss << "]";
    }
    LL_DEBUGS("LocalMesh") << "  mFaceIndices: " << ss.str() << LL_ENDL;
    // create a local string stream for the vertex positions in mPositions then log it
    std::stringstream ss_pos;
    ss_pos << "[";
    for (const auto& pos : mPositions)
    {
        ss_pos << pos << ",";
    }
    ss_pos << "]";
    LL_DEBUGS("LocalMesh") << "  mFacePositions: " << ss_pos.str() << LL_ENDL;
    // create a local string stream for the UVcoords in mUVs then log it
    std::stringstream ss_uv;
    ss_uv << "[";
    for (const auto& uv : mUVs)
    {
        ss_uv << uv << ",";
    }
    ss_uv << "]";
    LL_DEBUGS("LocalMesh") << "  mFaceUVs: " << ss_uv.str() << LL_ENDL;
    // create a local string stream for the normals in mNormals then log it
    std::stringstream ss_norm;
    ss_norm << "[";
    for (const auto& norm : mNormals)
    {
        ss_norm << norm << ",";
    }
    ss_norm << "]";
    LL_DEBUGS("LocalMesh") << "  mFaceNormals: " << ss_norm.str() << LL_ENDL;
    int i = 0;
    for (const auto& skinUnit : mSkin)
    {
        // log the mJointIncdices and mJointWeights as "num: idx = weight" for each entry in th skinUnit vector
        LL_DEBUGS("LocalMesh") << "  mSkin[" << i << "]: " << LL_ENDL;
        for (auto j=0; j<4;j++)
        {
            auto index =skinUnit.mJointIndices[j];
            auto weight = skinUnit.mJointWeights[j];
            LL_DEBUGS("LocalMesh") << "    " << j <<": [" << index << "] = " << weight << LL_ENDL;
        }
        ++i;
    }
}

/*==========================================*/
/*  LLLocalMeshObject: collection of faces  */
/*  has object name, transform & skininfo,  */
/*  volumeid and volumeparams for vobj.     */
/*  when applied - fills vobj volume.       */
/*==========================================*/
LLLocalMeshObject::LLLocalMeshObject(std::string_view name):
    mObjectName(name)
{
    mSculptID.generate();
    mVolumeParams.setSculptID(mSculptID, LL_SCULPT_TYPE_MESH);
}

LLLocalMeshObject::~LLLocalMeshObject() = default;

void LLLocalMeshObject::logObjectInfo() const
{
    // log all of the attribute of the object using LL_DEBUGS("LocalMesh")
    LL_DEBUGS("LocalMesh") << "LLLocalMeshObject: " << mObjectName << LL_ENDL;
    LL_DEBUGS("LocalMesh") << "  mSculptID: " << mSculptID << LL_ENDL;
    LL_DEBUGS("LocalMesh") << "  mVolumeParams: " << mVolumeParams << LL_ENDL;
    LL_DEBUGS("LocalMesh") << "  mObjectTranslation: " << mObjectTranslation << LL_ENDL;
    LL_DEBUGS("LocalMesh") << "  mMeshSkinInfo: " << std::hex << std::showbase << (void *)mMeshSkinInfoPtr << LL_ENDL;
    LL_DEBUGS("LocalMesh") << ll_pretty_print_sd(mMeshSkinInfoPtr->asLLSD(true, true)) << LL_ENDL;
    LL_DEBUGS("LocalMesh") << "  mFaceBoundingBox: [" << mObjectBoundingBox.first << "," << mObjectBoundingBox.second << LL_ENDL;
    LL_DEBUGS("LocalMesh") << "  mObjectSize: " << mObjectSize << LL_ENDL;
    LL_DEBUGS("LocalMesh") << "  mObjectScale: " << mObjectScale << LL_ENDL;
    // Log the number of faces in mFaces and dump each one to the Log
    // LL_DEBUGS("LocalMesh") << "  num faces: " << mFaces.size() << LL_ENDL;
    // int i=0;
    // for (const auto& face : mFaces)
    // {
    //  LL_DEBUGS("LocalMesh") << "    face: " << i << LL_ENDL;
    //  face.logFaceInfo();
    //  i++
    // }
}



void LLLocalMeshObject::computeObjectBoundingBox()
{
    // for the purposes of a bounding box, we only care for LOD3
    // unless the user is misusing lower LODs in a weird way.
    // we've already checked that mFaces[3] isn't empty in the invoking function.

    // check anyway
    if (mFaces[3].empty())
    {
        // why are we still here, just to suffer?
        return;
    }

    auto& lod3_faces = mFaces[3];

    // .first is min, .second is max

    // set initial values[face 0] to compare against
    auto& init_values = lod3_faces[0]->getFaceBoundingBox();
    mObjectBoundingBox.first = init_values.first;
    mObjectBoundingBox.second = init_values.second;

    // process any additional faces
    for (size_t face_iter = 1; face_iter < lod3_faces.size(); ++face_iter)
    {
        const auto& [current_bbox_min, current_bbox_max] = lod3_faces[face_iter]->getFaceBoundingBox();


        for (size_t array_iter = 0; array_iter < 4; ++array_iter)
        {
            mObjectBoundingBox.first.mV[array_iter] = std::min(mObjectBoundingBox.first.mV[array_iter], current_bbox_min.mV[array_iter]);
            mObjectBoundingBox.second.mV[array_iter] = std::max(mObjectBoundingBox.second.mV[array_iter], current_bbox_max.mV[array_iter]);
        }
    }

}

void LLLocalMeshObject::computeObjectTransform(const LLMatrix4& scene_transform)
{
    // most things here were tactfully stolen from LLModel::normalizeVolumeFaces()

    // translation of bounding box to origin
    // We will use this to centre the object at the origin (hence the -ve 1/2)
    mObjectTranslation = mObjectBoundingBox.first;
    mObjectTranslation += mObjectBoundingBox.second;
    mObjectTranslation *= -0.5f;

    // actual bounding box size (max-min)
    mObjectSize = mObjectBoundingBox.second - mObjectBoundingBox.first;

    // make sure all axes of mObjectSize are non zero (don't adjust the 4th dim)
    for ( int i=0; i <3; i++ )
    {
        auto& axis_size = mObjectSize[i];
        // set size of 1.0 if < F_APPROXIMATELY_ZERO
        if (axis_size <= F_APPROXIMATELY_ZERO)
        {
            axis_size = 1.0f;
        }
    }
    // object scale is the inverse of the object size
    mObjectScale.set(1.f, 1.f, 1.f,1.f);
    for (int vec_iter = 0; vec_iter < 4; ++vec_iter)
    {
        mObjectScale[vec_iter] /= mObjectSize[vec_iter];
    }
}

void LLLocalMeshObject::normalizeFaceValues(LLLocalMeshFileLOD lod_iter)
{
    // NOTE: uv extents
    // doesn't actually seem necessary, UDIM support "just works".

    auto& lod_faces = mFaces[lod_iter];

    // if current lod isn't in use, skip.
    if (lod_faces.empty())
    {
        return;
    }

    // process current lod
    for (auto& current_face : lod_faces)
    {
        // first transform the bounding boxes ro be centred at 0,0,0
        auto& current_submesh_bbox = current_face->getFaceBoundingBox();
        current_submesh_bbox.first += mObjectTranslation;
        current_submesh_bbox.second += mObjectTranslation;

        LL_INFOS("LocalMesh") << "before squish:" << current_submesh_bbox.first << " " << current_submesh_bbox.second << LL_ENDL;
        for (int vec_iter = 0; vec_iter < 4; ++vec_iter)
        {
            current_submesh_bbox.first.mV[vec_iter] *= mObjectScale.mV[vec_iter];
            current_submesh_bbox.second.mV[vec_iter] *= mObjectScale.mV[vec_iter];
        }
        LL_INFOS("LocalMesh") << "after squish:" << current_submesh_bbox.first << " " << current_submesh_bbox.second << LL_ENDL;

        // then transform the positions
        auto& current_face_positions = current_face->getPositions();
        for (auto& current_position : current_face_positions)
        {
            current_position += mObjectTranslation;
            for (int vec_iter = 0; vec_iter < 4; ++vec_iter)
            {
                current_position.mV[vec_iter] *= mObjectScale.mV[vec_iter];
            }
        }

        // if we have no normals, or they somehow don't match pos - skip here.
        auto& current_face_normals = current_face->getNormals();
        if (current_face_normals.size() != current_face_positions.size())
        {
            continue;
        }

        // and last, transform the normals
        for (auto& current_normal : current_face_normals)
        {
            if (current_normal.isExactlyZero())
            {
                continue;
            }

            for (int vec_iter = 0; vec_iter < 4; ++vec_iter)
            {
                current_normal.mV[vec_iter] *= mObjectSize.mV[vec_iter];
            }

            current_normal.normalize();
        }
    }
}

void LLLocalMeshObject::fillVolume(LLLocalMeshFileLOD lod)
{
    // check if we have data for [lod]
    if (mFaces[lod].empty())
    {
        return;
    }

    // generate face data
    std::vector<LLVolumeFace> new_faces;

    for (auto& current_submesh : mFaces[lod])
    {
        LLVolumeFace new_face;
        new_face.resizeVertices(static_cast<S32>(current_submesh->getPositions().size()));
        new_face.resizeIndices(static_cast<S32>(current_submesh->getIndices().size()));

        // NOTE: uv extents
        // doesn't actually seem necessary, UDIM support "just works".

        // position attribute
        for (size_t position_iter = 0; position_iter < current_submesh->getPositions().size(); ++position_iter)
        {
            new_face.mPositions[position_iter].loadua(current_submesh->getPositions()[position_iter].mV);
        }

        // normal attribute
        for (size_t normal_iter = 0; normal_iter < current_submesh->getNormals().size(); ++normal_iter)
        {
            new_face.mNormals[normal_iter].loadua(current_submesh->getNormals()[normal_iter].mV);
        }

        // uv attribute
        for (size_t uv_iter = 0; uv_iter < current_submesh->getUVs().size(); ++uv_iter)
        {
            new_face.mTexCoords[uv_iter] = current_submesh->getUVs()[uv_iter];
        }

        // weight attribute
        if (!current_submesh->getSkin().empty())
        {
            new_face.allocateWeights(static_cast<S32>(current_submesh->getSkin().size()));
            for (size_t weight_iter = 0; weight_iter < current_submesh->getSkin().size(); ++weight_iter)
            {
                const auto& current_local_weight = current_submesh->getSkin()[weight_iter];
                LLVector4 current_v4_weight;

                for (int joint_iter = 0; joint_iter < 4; ++joint_iter)
                {
                    auto jidx = current_local_weight.mJointIndices[joint_iter];
                    if (jidx == -1)
                    {
                        continue;
                    }

                    float cweight = current_local_weight.mJointWeights[joint_iter];
                    float cjoint = (float)jidx;
                    float combined = cweight + cjoint;
                    current_v4_weight[joint_iter] = combined;
                }

                new_face.mWeights[weight_iter].loadua(current_v4_weight.mV);
            }
        }

        // transfer indices
        for (size_t index_iter = 0; index_iter < current_submesh->getNumIndices(); ++index_iter)
        {
            new_face.mIndices[index_iter] = current_submesh->getIndices()[index_iter];
        }

        new_faces.push_back(new_face);
    }

    // push data into relevant lod
    LLVolume* current_volume = LLPrimitive::getVolumeManager()->refVolume(mVolumeParams, lod);
    if (current_volume)
    {
        current_volume->copyFacesFrom(new_faces);
        current_volume->setMeshAssetLoaded(true);
        LLPrimitive::getVolumeManager()->unrefVolume(current_volume);
    }
}

void LLLocalMeshObject::attachSkinInfo()
{
    auto skinmap_seeker = gMeshRepo.mSkinMap.find(mSculptID);
    if (skinmap_seeker == gMeshRepo.mSkinMap.end())
    {
        // Not found create it
        mMeshSkinInfoPtr->mMeshID = mSculptID;
        gMeshRepo.mSkinMap[mSculptID] = mMeshSkinInfoPtr;
        LL_INFOS("LocalMesh") << "Skinmap entry for UUID " << mSculptID << " created with " << std::hex << std::showbase << (void *)mMeshSkinInfoPtr << LL_ENDL;
    }
    else
    {
        // found but we will update it to ours
        LL_INFOS("LocalMesh") << "Skinmap entry for UUID " << mSculptID << " updated from " << std::hex << std::showbase << (void *)skinmap_seeker->second << " to " << (void *)mMeshSkinInfoPtr << LL_ENDL;
        skinmap_seeker->second = mMeshSkinInfoPtr;
    }

}

bool LLLocalMeshObject::getIsRiggedObject() const
{
    bool result = false;
    auto& main_lod_faces = mFaces[LLLocalMeshFileLOD::LOCAL_LOD_HIGH];

    // main lod isn't empty
    if (!main_lod_faces.empty())
    {
        LL_INFOS("LocalMesh") << "Main lod is not empty" << LL_ENDL;
        auto& skin = main_lod_faces[0]->getSkin();
        if (!skin.empty())
        {
            result = true;
        }
    }

    return result;
}


/*==========================================*/
/*  LLLocalMeshFile:  Single Unit           */
/*  owns filenames [main and lods]          */
/*  owns the loaded local mesh objects      */
/*==========================================*/
LLLocalMeshFile::LLLocalMeshFile(const std::string& filename, bool try_lods)
{
    // initialize safe defaults
    for (size_t lod_iter = 0; lod_iter < LOCAL_NUM_LODS; ++lod_iter)
    {
        mFilenames[lod_iter].clear();
        mLastModified[lod_iter].clear();
        mLoadedSuccessfully[lod_iter] = false;
    }

    mTryLODFiles = try_lods;
    mShortName.clear();
    mLoadingLog.clear();
    mExtension = LLLocalMeshFileExtension::EXTEN_NONE;
    mLocalMeshFileStatus = LLLocalMeshFileStatus::STATUS_NONE;
    mLocalMeshFileID.setNull();
    mLocalMeshFileNeedsUIUpdate = false;
    mLoadedObjectList.clear();
    mSavedObjectSculptIDs.clear();

    mShortName = std::string(boost::filesystem::path(filename).stem().string());
    std::string stripSuffix(std::string);
    auto base_lod_filename {stripSuffix(mShortName)};
    pushLog("LLLocalMeshFile", "Initializing with base filename: " + base_lod_filename);

    // check if main filename exists, just in case
    if (!boost::filesystem::exists(filename))
    {
        // filename provided doesn't exist, just stop.
        mLocalMeshFileStatus = LLLocalMeshFileStatus::STATUS_ERROR;
        pushLog("LLLocalMeshFile", "Couldn't find filename: " + filename, true);
        return;
    }

    // for the high lod we just store the filename fromthe file picker
    mFilenames[LOCAL_LOD_HIGH] = filename;

    // check if we have a valid extension, can't switch with string can we?
    auto path = boost::filesystem::path(filename);
    if (std::string exten_str = path.extension().string();
    boost::iequals(exten_str, ".dae") )
    {
        mExtension = LLLocalMeshFileExtension::EXTEN_DAE;
        pushLog("LLLocalMeshFile", "Extension found: COLLADA");
    }
    // add more ifs for different types, in lieu of a better approach?

    // if no extensions found, stop.
    if (mExtension == LLLocalMeshFileExtension::EXTEN_NONE)
    {
        mLocalMeshFileStatus = LLLocalMeshFileStatus::STATUS_ERROR;
        pushLog("LLLocalMeshFile", "No valid file extension found.", true);
        return;
    }

    mLocalMeshFileID.generate();

    // actually loads the files
    reloadLocalMeshObjects(true);
}

LLLocalMeshFile::~LLLocalMeshFile() = default;

void LLLocalMeshFile::reloadLocalMeshObjects(bool initial_load)
{
    // if we're already loading - nothing should be calling for a reload,
    // but just in case..
    if (mLocalMeshFileStatus == LLLocalMeshFileStatus::STATUS_LOADING)
    {
        return;
    }

    // if we're reloading, may be a good time to clear the log
    if (!initial_load)
    {
        mLoadingLog.clear();

        // before reloading (and regenerating sculpt ids for each object) save these ids
        // we'll need them to find the list of vobjects affected by each local object,
        // that so we can update vobjects with the new local objects.

        // clear it first just in case
        mSavedObjectSculptIDs.clear();

        for (const auto& local_object : mLoadedObjectList)
        {
            mSavedObjectSculptIDs.emplace_back(local_object->getVolumeParams().getSculptID());
        }
    }

    mLocalMeshFileStatus = LLLocalMeshFileStatus::STATUS_LOADING;
    mLocalMeshFileNeedsUIUpdate = true;

    // another recheck that mFilenames[3] main file is present,
    // in case the file got deleted and the user hits reload - it'll error out here.
    if (!boost::filesystem::exists(mFilenames[LOCAL_LOD_HIGH]))
    {
        // filename provided doesn't exist, just stop.
        mLocalMeshFileStatus = LLLocalMeshFileStatus::STATUS_ERROR;
        pushLog("LLLocalMeshFile", "Couldn't find filename: " + mFilenames[LOCAL_LOD_HIGH], true);
        return;
    }

    // check for lod filenames
    if (mTryLODFiles)
    {
        pushLog("LLLocalMeshFile", "Seeking LOD files...");
        // up to LOD2, LOD3 being the highest is always done by this point.
        for (S32 lodfile_iter = LOCAL_LOD_LOWEST; lodfile_iter < LOCAL_LOD_HIGH; ++lodfile_iter)
        {
            // lod filenames may be empty because this is first time through or because the lod didn't exist before.
            if( mFilenames[lodfile_iter].empty() )
            {
                auto filepath { boost::filesystem::path(mFilenames[LOCAL_LOD_HIGH]).parent_path() };
                std::string getLodSuffix(S32);
                auto lod_suffix { getLodSuffix(lodfile_iter) };
                auto extension { boost::filesystem::path(mFilenames[LOCAL_LOD_HIGH]).extension() };

                boost::filesystem::path current_lod_filename = filepath / (mShortName + lod_suffix + extension.string());
                if ( boost::filesystem::exists( current_lod_filename ) )
                {
                    pushLog("LLLocalMeshFile", "LOD filename " + current_lod_filename.string() + " found, adding.");
                    mFilenames[lodfile_iter] = current_lod_filename.string();
                }

                else
                {
                    pushLog("LLLocalMeshFile", "LOD filename " + current_lod_filename.string() + " not found, skipping.");
                    mFilenames[lodfile_iter].clear();
                }
            }
        }
    }
    else
    {
        pushLog("LLLocalMeshFile", "Skipping LOD 2-0 files, as specified.");
    }

    // if we are here we can assume at least mFilenames[3] is present
    // here we'll define a lambda to call through std::async, accessible through mAsyncFuture.
    // the lamnda returns bool if change happened, individual lod logs, and their status.
    auto lambda_loadfiles = [&]() mutable -> LLLocalMeshLoaderReply
    {
        bool change_happened = false;
        std::vector<std::string> log;
        std::array<bool, 4> lod_success = {false, false, false, false};

        // iterate over every lod
        // we're counting back because LOD3 is most likely to have showstopper problems,
        // so i'd rather not load other lods if LOD3 is found to be broken.
        // int rather than size_t because we're iterating over LODS 3 to 0, stopping at -1
        for (signed int lod_idx = LOCAL_LOD_HIGH; lod_idx >= LOCAL_LOD_LOWEST; --lod_idx)
        {
            auto current_lod = static_cast<LLLocalMeshFileLOD>(lod_idx);

            // do we have a filename for this lod?
            if (mFilenames[lod_idx].empty())
            {
                log.push_back("[ LLLocalMeshFile ] File for LOD " + std::to_string(lod_idx) + " was not found, skipping.");
                continue;
            }

            // has the file been modified since we last checked?
            bool file_modified = updateLastModified(current_lod);
            if (!file_modified)
            {
                log.push_back("[ LLLocalMeshFile ] File for LOD " + std::to_string(lod_idx) + " was not modified, skipping.");
                lod_success[lod_idx] = mLoadedSuccessfully[lod_idx];
                continue;
            }

            // up until here, skipping loading a lod is fine, after here - it's a sign of an error.

            // clear the old objects, if any.
            if (lod_idx == LLLocalMeshFileLOD::LOCAL_LOD_HIGH)
            {
                mLoadedObjectList.clear();
            }

            log.push_back("[ LLLocalMeshFile ] Attempting to load file for LOD " + std::to_string(lod_idx));
            switch (mExtension)
            {
                case LLLocalMeshFileExtension::EXTEN_DAE:
                {
                    // pass it over to dae loader
                    LLLocalMeshImportDAE importer;
                    auto importer_result = importer.loadFile(this, current_lod);
                    lod_success[lod_idx] = importer_result.first;

                    // NOTE: if not success - do not indicate change as not to affect existing vobjects?
                    if (lod_success[lod_idx])
                    {
                        change_happened = true;
                    }

                    const auto& importer_log = importer_result.second;
                    log.reserve(log.size() + importer_log.size());
                    log.insert(log.end(), importer_log.begin(), importer_log.end());
                    break;
                }

                default:
                {
                    log.push_back("[ LLLocalMeshFile ] ERROR, Loader for LOD " + std::to_string(lod_idx) + " called with invalid extension.");
                    break;
                }
            }

            // by this point, it's only false if there's an actual error loading the file.
            if (!lod_success[lod_idx])
            {
                log.push_back("[ LLLocalMeshFile ] ERROR, attempted and failed to load LOD " + std::to_string(lod_idx) + ", stopping.");
                break;
            }
        }


        // just in case, recheck if we actually ended up loading anything
        auto& object_list = getObjectVector();
        if (object_list.empty())
        {
            log.push_back("[ LLLocalMeshFile ] ERROR, no objects loaded, stopping.");

            // seems like a pretty serious error, error out all the lods manually,
            // and set change happened to false.
            for (size_t lod_iter = 0; lod_iter < 4; ++lod_iter)
            {
                lod_success[lod_iter] = false;
            }

            change_happened = false;
        }

        LLLocalMeshLoaderReply result;
        result.mChanged = change_happened;
        result.mLog = log;
        result.mStatus = lod_success;
        return result;
    };

    mAsyncFuture = std::async(std::launch::async, lambda_loadfiles);
}

LLLocalMeshFile::LLLocalMeshFileStatus LLLocalMeshFile::reloadLocalMeshObjectsCheck()
{
    // only invalid if no loading is happening.
    if (mAsyncFuture.valid() == false)
    {
        return mLocalMeshFileStatus;
    }

    // actually check if the thread is done, if no - return that we're still loading.
    // _Is_ready() doesn't seem to actually do what it promises to, how punny.
    // so we'll wait for zero milliseconds instead to check if ready.
    auto timeout = std::chrono::milliseconds(0);
    if (mAsyncFuture.wait_for(timeout) != std::future_status::ready)
    {
        // thread is still working
        return mLocalMeshFileStatus;
    }
    else
    {
        // thread is done working
        reloadLocalMeshObjectsCallback();
    }

    // by now we have either active or error status, this return will lead to ui refresh.
    return mLocalMeshFileStatus;
}

void LLLocalMeshFile::reloadLocalMeshObjectsCallback()
{
    LLLocalMeshLoaderReply reply = mAsyncFuture.get();
    mLoadingLog.reserve(mLoadingLog.size() + reply.mLog.size());
    mLoadingLog.insert(mLoadingLog.end(), reply.mLog.begin(), reply.mLog.end());
    mLoadedSuccessfully = reply.mStatus;

    // if LOD3 is ok, we're technically fine.
    if (mLoadedSuccessfully[3])
    {
        mLocalMeshFileStatus = LLLocalMeshFileStatus::STATUS_ACTIVE;
        if (reply.mChanged)
        {
            updateVObjects();
        }
    }
    else
    {
        mLocalMeshFileStatus = LLLocalMeshFileStatus::STATUS_ERROR;
    }

    mLocalMeshFileNeedsUIUpdate = true;
}

bool LLLocalMeshFile::updateLastModified(LLLocalMeshFileLOD lod)
{
    bool file_updated = false;
    LLSD current_last_modified = mLastModified[lod];
    std::string current_filename = mFilenames[lod];

    #ifndef LL_WINDOWS
        const std::time_t temp_time = boost::filesystem::last_write_time(boost::filesystem::path(current_filename));
    #else
        const std::time_t temp_time = boost::filesystem::last_write_time(boost::filesystem::path(utf8str_to_utf16str(current_filename)));
    #endif


    if (LLSD new_last_modified = asctime(localtime(&temp_time)); new_last_modified.asString() != current_last_modified.asString())
    {
        file_updated = true;
        mLastModified[lod] = new_last_modified;
    }

    return file_updated;
}

bool LLLocalMeshFile::notifyNeedsUIUpdate()
{
    bool result = mLocalMeshFileNeedsUIUpdate;

    if (mLocalMeshFileNeedsUIUpdate)
    {
        // ui update can only be needed once.
        mLocalMeshFileNeedsUIUpdate = false;
    }

    return result;
}

LLLocalMeshFile::LLLocalMeshFileInfo LLLocalMeshFile::getFileInfo()
{
    LLLocalMeshFileInfo result;

    result.mName = mShortName;
    result.mStatus = mLocalMeshFileStatus;
    result.mLocalID = mLocalMeshFileID;
    result.mLODAvailability = mLoadedSuccessfully;
    result.mObjectList.clear();

    if (mLocalMeshFileStatus == LLLocalMeshFileStatus::STATUS_ACTIVE)
    {
        // fill object list
        auto& vector_objects = getObjectVector();
        for (auto& current_object : vector_objects)
        {
            std::string object_name = current_object->getObjectName();
            result.mObjectList.push_back(object_name);
        }
    }

    return result;
}

void LLLocalMeshFile::updateVObjects()
{
    for (size_t object_iter = 0; object_iter < mSavedObjectSculptIDs.size(); ++object_iter)
    {
        const auto& local_obj_sculpt_id = mSavedObjectSculptIDs[object_iter];
        auto affected_vobject_ids = gObjectList.findMeshObjectsBySculptID(local_obj_sculpt_id);
        for (const auto& current_vobject_id : affected_vobject_ids)
        {
            auto target_object_ptr = static_cast<LLVOVolume*>(gObjectList.findObject(current_vobject_id));

            if (!target_object_ptr->mIsLocalMesh)
            {
                // in case for some reason we got the id of an unrelated object?
                // should never get here.
                continue;
            }

            bool using_scale = target_object_ptr->mIsLocalMeshUsingScale;
            applyToVObject(current_vobject_id, static_cast<int>(object_iter), using_scale);
        }

        // also, remove old skin from
        gMeshRepo.mSkinMap.erase(local_obj_sculpt_id);
    }
}

void LLLocalMeshFile::applyToVObject(LLUUID viewer_object_id, int object_index, bool use_scale)
{
    // get the actual vovolume
    auto target_object = static_cast<LLVOVolume*>(gObjectList.findObject(viewer_object_id));
    if (target_object == nullptr)
    {
        return;
    }

    // check if object_index is sane
    if ((object_index < 0) || (object_index >= mLoadedObjectList.size()))
    {
        return;
    }

    auto object_params = mLoadedObjectList[object_index]->getVolumeParams();
    target_object->setVolume(object_params, 3);
    target_object->mIsLocalMesh = true;
    target_object->mIsLocalMeshUsingScale = use_scale;

    for (int lod_reverse_iter = 3; lod_reverse_iter >= 0; --lod_reverse_iter)
    {
        if (!mLoadedSuccessfully[lod_reverse_iter])
        {
            continue;
        }

        auto current_lod = static_cast<LLLocalMeshFileLOD>(lod_reverse_iter);
        mLoadedObjectList[object_index]->fillVolume(current_lod);
        LL_INFOS("LocalMesh") << "Loaded LOD " << current_lod << LL_ENDL;
    }
    LL_INFOS("LocalMesh") << "Loaded Object " << object_index << LL_ENDL;
    if (mLoadedObjectList[object_index]->getIsRiggedObject())
    {
        LL_INFOS("LocalMesh") << "Object " << object_index << " is rigged" << LL_ENDL;
        mLoadedObjectList[object_index]->attachSkinInfo();
        target_object->notifySkinInfoLoaded(mLoadedObjectList[object_index]->getObjectMeshSkinInfo());
    }

    if ((!target_object->isAttachment()) && use_scale)
    {
        LL_INFOS("LocalMesh") << "Object " << object_index << " is not attachment" << LL_ENDL;
        auto scale = mLoadedObjectList[object_index]->getObjectSize();
        target_object->setScale(LLVector3(scale), false);
    }

    // force refresh (selected/edit mode won't let it redraw otherwise)
    auto& target_drawable = target_object->mDrawable;

    if (target_drawable.notNull())
    {
        target_object->markForUpdate();
        // target_drawable->updateSpatialExtents();
        // target_drawable->movePartition();
        gPipeline.markRebuild(target_drawable, LLDrawable::REBUILD_ALL);
        if(auto floater_ptr = LLLocalMeshSystem::getInstance()->getFloaterPointer(); floater_ptr != nullptr)
        {
            floater_ptr->toggleSelectTool(false);
        }
    }
    // NOTE: this ^^ (or lod change) causes renderer crash on mesh with degenerate primitives.
    LL_INFOS("LocalMesh") << "Object " << object_index << " marked for rebuild" << LL_ENDL;
}

void LLLocalMeshFile::pushLog(const std::string& who, const std::string& what, bool is_error)
{
    std::string log_msg = "[ " + who + " ] ";
    if (is_error)
    {
        log_msg += "[ ERROR ] ";
    }

    log_msg += what;
    mLoadingLog.push_back(log_msg);
    LL_INFOS("LocalMesh") << log_msg << LL_ENDL;
}
/*==========================================*/
/*  LLLocalMeshSystem:  Main Manager Class  */
/*  user facing manager class               */
/*==========================================*/

void LLLocalMeshSystem::pushLog(const std::string& who, const std::string& what, bool is_error)
{
    std::string log_msg = "[ " + who + " ] ";
    if (is_error)
    {
        log_msg += "[ ERROR ] ";
    }

    log_msg += what;
    mSystemLog.push_back(log_msg);
    LL_INFOS("LocalMesh") << log_msg << LL_ENDL;
}

LLLocalMeshSystem::LLLocalMeshSystem()
{
    mLoadedFileList.clear();
    mFileAsyncsOngoing = false;
    mFloaterPtr = nullptr;
}

LLLocalMeshSystem::~LLLocalMeshSystem()
{
    /* clear files, triggers releasing donor objects
       and destructs any held localmesh objects. */
    mLoadedFileList.clear();
}

void LLLocalMeshSystem::addFile(const std::string& filename, bool try_lods)
{
    auto loaded_file = std::make_unique<LLLocalMeshFile>(filename, try_lods);
    mLoadedFileList.push_back(std::move(loaded_file));
    triggerFloaterRefresh(false);

    triggerCheckFileAsyncStatus();
}

void LLLocalMeshSystem::deleteFile(LLUUID local_file_id)
{
    bool delete_done = false;
    for(auto iterator = mLoadedFileList.begin(); iterator != mLoadedFileList.end();)
    {
        auto current_file = iterator->get();
        auto current_info = current_file->getFileInfo();

        if (current_info.mLocalID == local_file_id)
        {
            // check if the file is currently in a state of loading
            // even though the ui button will be disabled while loading -
            // this check still feels necessary.
            if (current_info.mStatus == LLLocalMeshFile::LLLocalMeshFileStatus::STATUS_LOADING)
            {
                break;
            }

            // found the requested by id file, it's not in the process of loading, time to delete it.
            iterator = mLoadedFileList.erase(std::move(iterator));
            delete_done = true;
        }
        else
        {
            ++iterator;
        }
    }

    if (delete_done)
    {
        triggerFloaterRefresh();
    }
}

void LLLocalMeshSystem::reloadFile(LLUUID local_file_id)
{
    bool reload_started = false;
    for (auto iterator = mLoadedFileList.begin(); iterator != mLoadedFileList.end(); ++iterator)
    {
        auto current_file = iterator->get();
        auto current_info = current_file->getFileInfo();

        if (current_info.mLocalID == local_file_id)
        {
            // check if the file is currently in a state of loading
            // same deal as above, ui button will be disabled but checking regardless.
            if (current_info.mStatus == LLLocalMeshFile::LLLocalMeshFileStatus::STATUS_LOADING)
            {
                continue;
            }

            // found the requested by id file, it's not loading, let's make it loading.
            current_file->reloadLocalMeshObjects();
            reload_started = true;
        }
    }

    if (reload_started)
    {
        triggerCheckFileAsyncStatus();
    }
}

void LLLocalMeshSystem::applyVObject(LLUUID viewer_object_id, LLUUID local_file_id, int object_index, bool use_scale)
{
    for (auto& loaded_file : mLoadedFileList)
    {
        if (loaded_file->getFileID() == local_file_id)
        {
            loaded_file->applyToVObject(viewer_object_id, object_index, use_scale);
            break;
        }
    }
}

void LLLocalMeshSystem::clearVObject(LLUUID viewer_object_id)
{
    auto target_object = (LLVOVolume*)gObjectList.findObject(viewer_object_id);
    if (!target_object)
    {
        return;
    }

    target_object->mIsLocalMesh = false;

    // get original params
    auto sculpt_params = (LLSculptParams*)target_object->getParameterEntry(LLNetworkData::PARAMS_SCULPT);
    auto sculpt_id = sculpt_params->getSculptTexture();

    LLVolumeParams volume_params;
    volume_params.setSculptID(sculpt_id, LL_SCULPT_TYPE_MESH);
    target_object->setVolume(volume_params, 3);
}

void LLLocalMeshSystem::triggerCheckFileAsyncStatus()
{
    // async already ongoing, just wait patiently.
    if (mFileAsyncsOngoing)
    {
        return;
    }

    // this is here so it's set off /manually/ rather than by a function called through doOnIdleOneTime
    mFileAsyncsOngoing = true;
    checkFileAsyncStatus();
}

void LLLocalMeshSystem::checkFileAsyncStatus()
{
    // in case of misuse.
    // use triggerCheckFileAsyncStatus to trigger this instead.
    // the reason is: this function calls itself through doOnIdleOneTime,
    // so triggerCheckFileAsyncStatus is used this function doesn't set mFileAsyncsOngoing to true
    if (!mFileAsyncsOngoing)
    {
        return;
    }

    bool found_active_asyncs = false;
    bool need_ui_update = false;

    for (auto& current_file : mLoadedFileList)
    {
        // reloadLocalMeshObjectsCheck actually pings the future to check if ready,
        // and if yes - sets proper status, so we're pinging all files.
        // and if the file isn't loading - it'll just safely return.
        auto file_status = current_file->reloadLocalMeshObjectsCheck();
        if (file_status == LLLocalMeshFile::LLLocalMeshFileStatus::STATUS_LOADING)
        {
            found_active_asyncs = true;
        }

        if (current_file->notifyNeedsUIUpdate())
        {
            need_ui_update = true;
        }
    }

    if (found_active_asyncs)
    {
        // come back here next tick to ping checkFileAsyncStatus again
        doOnIdleOneTime(boost::bind(&LLLocalMeshSystem::checkFileAsyncStatus, this));
    }
    else
    {
        // nothing is updating, reflect that in mFileAsyncsOngoing
        mFileAsyncsOngoing = false;
    }

    if (need_ui_update)
    {
        triggerFloaterRefresh();
    }
}

void LLLocalMeshSystem::registerFloaterPointer(LLFloaterLocalMesh* floater_ptr)
{
    // not checking for nullptr here, we use this to both register and de-register a floater,
    // so nullptr is legal here.
    mFloaterPtr = floater_ptr;
}

void LLLocalMeshSystem::triggerFloaterRefresh(bool keep_selection)
{
    if (mFloaterPtr)
    {
        mFloaterPtr->reloadFileList(keep_selection);
    }
}

std::vector<LLLocalMeshFile::LLLocalMeshFileInfo> LLLocalMeshSystem::getFileInfoVector() const
{
    std::vector<LLLocalMeshFile::LLLocalMeshFileInfo> result;

    for (auto& current_file : mLoadedFileList)
    {
        result.push_back(current_file->getFileInfo());
    }

    return result;
}

std::vector<std::string> LLLocalMeshSystem::getFileLog(LLUUID local_file_id) const
{
    std::vector<std::string> result;

    for (auto& current_file : mLoadedFileList)
    {
        if (current_file->getFileID() == local_file_id)
        {
            result = current_file->getFileLog();
            break;
        }
    }

    return result;
}

