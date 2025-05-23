/**
 * @file vjlocalmesh.h
 * @author Vaalith Jinn
 * @brief Local Mesh main mechanism header
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

#pragma once

// linden headers
#include "llmodel.h" // LLMeshSkinInfo

// STL headers
#include <future>

class LLFloaterLocalMesh;

enum LLLocalMeshFileLOD
{
    LOCAL_LOD_LOWEST,
    LOCAL_LOD_LOW,
    LOCAL_LOD_MEDIUM,
    LOCAL_LOD_HIGH,
    LOCAL_NUM_LODS
};

/*==========================================*/
/*  LLLocalMeshFace: aka submesh denoted by */
/*  material assignment. holds per-face     */
/*  values for indices, bounding box and    */
/*  vtx pos, normals, uv coords, weights.   */
/*==========================================*/
class LLLocalMeshFace
{
public:
    struct LLLocalMeshSkinUnit
    {
        std::array<int, 4>   mJointIndices;
        std::array<float, 4> mJointWeights;
    };

    void setFaceBoundingBox(const LLVector4& data_in, bool initial_values = false);

    S32 getNumVerts() const { return static_cast<S32>(mPositions.size()); }
    S32 getNumIndices() const { return static_cast<S32>(mIndices.size()); }

    std::vector<S32>&                 getIndices() { return mIndices; };
    std::vector<LLVector4>&           getPositions() { return mPositions; };
    std::vector<LLVector4>&           getNormals() { return mNormals; };
    std::vector<LLVector2>&           getUVs() { return mUVs; };
    std::vector<LLLocalMeshSkinUnit>& getSkin() { return mSkin; }
    std::pair<LLVector4, LLVector4>&  getFaceBoundingBox() { return mFaceBoundingBox; }
    void                              logFaceInfo() const;

private:
    std::vector<S32>                 mIndices;
    std::vector<LLVector4>           mPositions;
    std::vector<LLVector4>           mNormals;
    std::vector<LLVector2>           mUVs;
    std::vector<LLLocalMeshSkinUnit> mSkin;
    std::pair<LLVector4, LLVector4>  mFaceBoundingBox;
};

/*==========================================*/
/*  LLLocalMeshObject: collection of faces  */
/*  has object name, transform & skininfo,  */
/*  volumeid and volumeparams for vobj.     */
/*  when applied - fills vobj volume.       */
/*==========================================*/
class LLLocalMeshObject
{
public:
    // life cycle management
    explicit LLLocalMeshObject(std::string_view name);
    ~LLLocalMeshObject();

    // translation and scale
    void computeObjectBoundingBox();
    void computeObjectTransform(const LLMatrix4& scene_transform);
    void normalizeFaceValues(LLLocalMeshFileLOD lod_iter);

    // applying local object to viewer object
    void fillVolume(LLLocalMeshFileLOD lod);
    void attachSkinInfo();

    // getters
    std::vector<std::unique_ptr<LLLocalMeshFace>>& getFaces(LLLocalMeshFileLOD lod) { return mFaces[lod]; };
    std::pair<LLVector4, LLVector4>&               getObjectBoundingBox() { return mObjectBoundingBox; };
    LLVector4                                      getObjectTranslation() const { return mObjectTranslation; };
    std::string                                    getObjectName() const { return mObjectName; };
    LLVector4                                      getObjectSize() const { return mObjectSize; };
    LLVector4                                      getObjectScale() const { return mObjectScale; };
    LLPointer<LLMeshSkinInfo>                      getObjectMeshSkinInfo() { return mMeshSkinInfoPtr; };
    void           setObjectMeshSkinInfo(LLPointer<LLMeshSkinInfo> skininfop) { mMeshSkinInfoPtr = skininfop; };
    LLVolumeParams getVolumeParams() const { return mVolumeParams; };
    bool           getIsRiggedObject() const;
    void           logObjectInfo() const;

private:
    // internal data keeping
    std::array<std::vector<std::unique_ptr<LLLocalMeshFace>>, 4> mFaces;
    std::pair<LLVector4, LLVector4>                              mObjectBoundingBox;
    std::string                                                  mObjectName;
    LLVector4                                                    mObjectTranslation;
    LLVector4                                                    mObjectSize;
    LLVector4                                                    mObjectScale;

    // vovolume
    LLPointer<LLMeshSkinInfo> mMeshSkinInfoPtr{ nullptr };
    LLUUID                    mSculptID;
    LLVolumeParams            mVolumeParams;
};

/*==========================================*/
/*  LLLocalMeshFile:  Single Unit           */
/*  owns filenames [main and lods]          */
/*  owns the loaded local mesh objects      */
/*==========================================*/
class LLLocalMeshFile
{
    // class specific types
public:
    enum LLLocalMeshFileStatus
    {
        STATUS_NONE,
        STATUS_LOADING,
        STATUS_ACTIVE,
        STATUS_ERROR
    };

    // for future gltf support, possibly more.
    enum class LLLocalMeshFileExtension
    {
        EXTEN_DAE,
        EXTEN_NONE
    };

    struct LLLocalMeshFileInfo
    {
        std::string              mName;
        LLLocalMeshFileStatus    mStatus;
        LLUUID                   mLocalID;
        std::array<bool, 4>      mLODAvailability;
        std::vector<std::string> mObjectList;
    };

    struct LLLocalMeshLoaderReply
    {
        bool                     mChanged;
        std::vector<std::string> mLog;
        std::array<bool, 4>      mStatus;
    };

    // life cycle management
    LLLocalMeshFile(const std::string& filename, bool try_lods);
    ~LLLocalMeshFile();

    // disallowing copy
    LLLocalMeshFile(const LLLocalMeshFile& local_mesh_file)            = delete;
    LLLocalMeshFile& operator=(const LLLocalMeshFile& local_mesh_file) = delete;

    // file loading
    void                                             reloadLocalMeshObjects(bool initial_load = false);
    LLLocalMeshFileStatus                            reloadLocalMeshObjectsCheck();
    void                                             reloadLocalMeshObjectsCallback();
    bool                                             updateLastModified(LLLocalMeshFileLOD lod);
    std::vector<std::unique_ptr<LLLocalMeshObject>>& getObjectVector() { return mLoadedObjectList; };

    // info getters
    bool                     notifyNeedsUIUpdate();
    LLLocalMeshFileInfo      getFileInfo();
    std::string              getFilename(LLLocalMeshFileLOD lod) const { return mFilenames[lod]; };
    LLUUID                   getFileID() const { return mLocalMeshFileID; };
    std::vector<std::string> getFileLog() const { return mLoadingLog; };

    // viewer object
    void updateVObjects();
    void applyToVObject(LLUUID viewer_object_id, int object_index, bool use_scale);

    void pushLog(const std::string& who, const std::string& what, bool is_error = false);

private:
    std::array<std::string, LOCAL_NUM_LODS> mFilenames;
    std::array<LLSD, LOCAL_NUM_LODS>        mLastModified;
    std::array<bool, LOCAL_NUM_LODS>        mLoadedSuccessfully;
    bool                                    mTryLODFiles;
    std::string                             mShortName;
    std::vector<std::string>                mLoadingLog;
    LLLocalMeshFileExtension                mExtension;
    LLLocalMeshFileStatus                   mLocalMeshFileStatus;
    LLUUID                                  mLocalMeshFileID;
    bool                                    mLocalMeshFileNeedsUIUpdate;

    std::future<LLLocalMeshLoaderReply>             mAsyncFuture;
    std::vector<std::unique_ptr<LLLocalMeshObject>> mLoadedObjectList;
    std::vector<LLUUID>                             mSavedObjectSculptIDs;
};

/*=============================*/
/*  LLLocalMeshSystem:         */
/*  user facing manager class. */
/*=============================*/
class LLLocalMeshSystem : public LLSingleton<LLLocalMeshSystem>
{
    // life cycle management
    LLSINGLETON(LLLocalMeshSystem);

public:
    ~LLLocalMeshSystem();

    // file management
    void addFile(const std::string& filename, bool try_lods);
    void deleteFile(const LLUUID& local_file_id);
    void reloadFile(const LLUUID& local_file_id);

    // viewer object management
    void applyVObject(const LLUUID& viewer_object_id, const LLUUID& local_file_id, int object_index, bool use_scale);
    void clearVObject(const LLUUID& viewer_object_id);

    // high level async support
    void triggerCheckFileAsyncStatus();
    void checkFileAsyncStatus();

    // floater two-way communication
    void                                              registerFloaterPointer(LLFloaterLocalMesh* floater_ptr);
    LLFloaterLocalMesh*                               getFloaterPointer() { return mFloaterPtr; };
    void                                              triggerFloaterRefresh(bool keep_selection = true);
    std::vector<LLLocalMeshFile::LLLocalMeshFileInfo> getFileInfoVector() const;
    std::vector<std::string>                          getFileLog(const LLUUID& local_file_id) const;
    // misc
    void pushLog(const std::string& who, const std::string& what, bool is_error = false);

private:
    std::vector<std::string>                      mSystemLog;
    std::vector<std::unique_ptr<LLLocalMeshFile>> mLoadedFileList;
    bool                                          mFileAsyncsOngoing;
    LLFloaterLocalMesh*                           mFloaterPtr;
};
