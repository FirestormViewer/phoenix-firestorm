/**
 * @file fslocalmeshimportbase.cpp
 * @brief Shared helpers for local mesh importers
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Local Mesh contribution source code
 * Copyright (C) 2026, The Phoenix Firestorm Project, Inc.
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

#include "fslocalmeshimportbase.h"

#include "llerror.h"
#include "llskinningutil.h"
#include "llvoavatarself.h"

void FSLocalMeshImportBase::setLod(LLLocalMeshFileLOD lod)
{
    mLod = lod;
    mLoadingLog.clear();
}

void FSLocalMeshImportBase::pushLog(const std::string& who, const std::string& what, bool is_error)
{
    std::string log_msg = "[ " + who + " ] ";
    if (is_error)
    {
        log_msg += "[ ERROR ] ";
    }

    log_msg += what;
    mLoadingLog.push_back(log_msg);

    if (mLogToInfo)
    {
        LL_INFOS("LocalMesh") << log_msg << LL_ENDL;
    }
}

void FSLocalMeshImportBase::postProcessObject(LLLocalMeshObject& object, const LLMatrix4& scene_transform, bool compute_bounds)
{
    if (compute_bounds)
    {
        object.computeObjectBoundingBox();
        object.computeObjectTransform(scene_transform);
    }

    object.normalizeFaceValues(mLod);
}

bool FSLocalMeshImportBase::enforceRigJointLimit(const std::string& who,
                                                 LLLocalMeshObject& object,
                                                 LLPointer<LLMeshSkinInfo> skininfop,
                                                 U32 recognized_joint_count)
{
    const S32 max_joints = LLSkinningUtil::getMaxJointCount();
    if ((S32)recognized_joint_count <= max_joints)
    {
        return true;
    }

    const std::string warning = "WARNING: Skinning disabled for object \"" + object.getObjectName()
        + "\" due to too many joints: " + std::to_string(recognized_joint_count)
        + ", maximum: " + std::to_string(max_joints) + ".";
    pushLog(who, warning);
    LL_WARNS("LocalMesh") << who << ": " << warning << LL_ENDL;

    if (skininfop.notNull())
    {
        skininfop->mJointNames.clear();
        skininfop->mJointNums.clear();
        skininfop->mInvBindMatrix.clear();
        skininfop->mAlternateBindMatrix.clear();
        skininfop->mBindPoseMatrix.clear();
        skininfop->mBindShapeMatrix = LLMatrix4a::identity();
        skininfop->mInvalidJointsScrubbed = false;
        skininfop->mJointNumsInitialized = false;
        skininfop->updateHash();
    }

    object.setObjectMeshSkinInfo(LLPointer<LLMeshSkinInfo>());

    for (S32 lod = 0; lod < LOCAL_NUM_LODS; ++lod)
    {
        for (auto& face : object.getFaces(static_cast<LLLocalMeshFileLOD>(lod)))
        {
            face->getSkin().clear();
        }
    }

    return false;
}

FSLocalMeshImportBase::JointMap FSLocalMeshImportBase::loadJointMap()
{
    JointMap joint_map = gAgentAvatarp->getJointAliases();

    // unfortunately getSortedJointNames clears the ref vector, and we need two extra lists.
    std::vector<std::string> extra_names, more_extra_names;
    gAgentAvatarp->getSortedJointNames(1, extra_names);
    gAgentAvatarp->getSortedJointNames(2, more_extra_names);
    extra_names.reserve(more_extra_names.size());
    extra_names.insert(extra_names.end(), more_extra_names.begin(), more_extra_names.end());

    // add the extras to jointmap
    for (const auto& extra_name : extra_names)
    {
        joint_map[extra_name] = extra_name;
    }

    return joint_map;
}

LLMatrix4 FSLocalMeshImportBase::buildNormalizedTransformation(const LLLocalMeshObject& object)
{
    LLVector4 inverse_translation = object.getObjectTranslation() * -1.f;
    LLVector4 object_size = object.getObjectSize();

    LLMatrix4 normalized_transformation;
    LLMatrix4 mesh_scale;
    normalized_transformation.setTranslation(LLVector3(inverse_translation));
    mesh_scale.initScale(LLVector3(object_size));
    mesh_scale *= normalized_transformation;
    normalized_transformation = mesh_scale;
    return normalized_transformation;
}

void FSLocalMeshImportBase::buildBindPoseMatrix(LLPointer<LLMeshSkinInfo> skininfop)
{
    if (skininfop.isNull())
    {
        return;
    }

    skininfop->mBindPoseMatrix.resize(skininfop->mInvBindMatrix.size());
    for (U32 i = 0; i < skininfop->mInvBindMatrix.size(); ++i)
    {
        matMul(skininfop->mBindShapeMatrix, skininfop->mInvBindMatrix[i], skininfop->mBindPoseMatrix[i]);
    }
}
