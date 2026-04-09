/**
 * @file fslocalmeshimportgltf.cpp
 * @author Beq Janus
 * @brief Local Mesh glTF importer source
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (C) 2026, Beq Janus.
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

// precompiled headers
#include "llviewerprecompiledheaders.h"

// own header
#include "fslocalmeshimportgltf.h"

// linden headers
#include "llerror.h"
#include "llmodelloader.h"
#include "llvoavatarself.h"
#include "llmatrix4a.h"
#include "lljointdata.h"
#include "llviewercontrol.h"

// glTF headers
#include "gltf/asset.h"
#include "gltf/accessor.h"

// glm headers
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/packing.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>

// STL headers
#include <algorithm>
#include <set>
#include <sstream>

namespace
{
    using local_joint_map_t = std::map<std::string, std::string, std::less<>>;

    // Premade rotation matrix, GLTF is Y-up while SL is Z-up
    const glm::mat4 kCoordSystemRotation(
        1.f, 0.f, 0.f, 0.f,
        0.f, 0.f, 1.f, 0.f,
        0.f, -1.f, 0.f, 0.f,
        0.f, 0.f, 0.f, 1.f
    );

    const glm::mat4 kCoordSystemRotationXY(
        0.f, 1.f, 0.f, 0.f,
        -1.f, 0.f, 0.f, 0.f,
        0.f, 0.f, 1.f, 0.f,
        0.f, 0.f, 0.f, 1.f
    );

    glm::mat4 buildViewerBasisTransform(bool apply_xy_rotation)
    {
        glm::mat4 basis = kCoordSystemRotation;
        if (apply_xy_rotation)
        {
            basis = kCoordSystemRotationXY * basis;
        }
        return basis;
    }

    glm::mat4 convertTransformToViewerBasis(const glm::mat4& transform, bool apply_xy_rotation)
    {
        const glm::mat4 basis = buildViewerBasisTransform(apply_xy_rotation);
        return basis * transform * glm::inverse(basis);
    }

    std::vector<S32> buildParentMap(const LL::GLTF::Asset& asset)
    {
        std::vector<S32> parent_map(asset.mNodes.size(), -1);
        for (size_t i = 0; i < asset.mNodes.size(); ++i)
        {
            for (S32 child_idx : asset.mNodes[i].mChildren)
            {
                if (child_idx >= 0 && child_idx < static_cast<S32>(parent_map.size()))
                {
                    parent_map[child_idx] = static_cast<S32>(i);
                }
            }
        }
        return parent_map;
    }

    std::vector<bool> buildSkinJointMembership(const LL::GLTF::Asset& asset, const LL::GLTF::Skin& skin)
    {
        std::vector<bool> is_skin_joint(asset.mNodes.size(), false);
        for (S32 joint_node_idx : skin.mJoints)
        {
            if (joint_node_idx >= 0 && joint_node_idx < static_cast<S32>(is_skin_joint.size()))
            {
                is_skin_joint[joint_node_idx] = true;
            }
        }
        return is_skin_joint;
    }

    void collectMeshNodes(const LL::GLTF::Asset& asset, S32 node_idx, std::vector<S32>& mesh_nodes)
    {
        if (node_idx < 0 || node_idx >= static_cast<S32>(asset.mNodes.size()))
        {
            return;
        }

        const LL::GLTF::Node& node = asset.mNodes[node_idx];
        if (node.mMesh >= 0)
        {
            mesh_nodes.push_back(node_idx);
        }

        for (S32 child_idx : node.mChildren)
        {
            collectMeshNodes(asset, child_idx, mesh_nodes);
        }
    }

    void computeCombinedNodeTransform(const LL::GLTF::Asset& asset,
                                      const std::vector<S32>& parent_map,
                                      S32 node_index,
                                      glm::mat4& combined_transform)
    {
        if (node_index < 0 || node_index >= static_cast<S32>(asset.mNodes.size()))
        {
            combined_transform = glm::mat4(1.f);
            return;
        }

        combined_transform = asset.mNodes[node_index].mMatrix;
        S32 parent_idx = (node_index < static_cast<S32>(parent_map.size())) ? parent_map[node_index] : -1;
        while (parent_idx >= 0 && parent_idx < static_cast<S32>(asset.mNodes.size()))
        {
            combined_transform = asset.mNodes[parent_idx].mMatrix * combined_transform;
            parent_idx = (parent_idx < static_cast<S32>(parent_map.size())) ? parent_map[parent_idx] : -1;
        }
    }

    std::string normalizeJointName(const local_joint_map_t& joint_map, const std::string& joint_name)
    {
        auto joint_it = joint_map.find(joint_name);
        return joint_it != joint_map.end() ? joint_it->second : joint_name;
    }

    std::vector<S32> buildJointIndexRemap(const LL::GLTF::Asset& asset,
                                          const LL::GLTF::Skin& skin,
                                          const local_joint_map_t& joint_map,
                                          LLPointer<LLMeshSkinInfo> canonical_skin)
    {
        std::vector<S32> joint_index_remap(skin.mJoints.size(), -1);
        if (canonical_skin.isNull() || canonical_skin->mJointNames.empty())
        {
            for (size_t i = 0; i < joint_index_remap.size(); ++i)
            {
                joint_index_remap[i] = static_cast<S32>(i);
            }
            return joint_index_remap;
        }

        std::map<std::string, S32, std::less<>> canonical_indices;
        for (size_t i = 0; i < canonical_skin->mJointNames.size(); ++i)
        {
            canonical_indices.emplace(canonical_skin->mJointNames[i], static_cast<S32>(i));
        }

        for (size_t i = 0; i < skin.mJoints.size(); ++i)
        {
            const S32 joint_node_idx = skin.mJoints[i];
            if (joint_node_idx < 0 || joint_node_idx >= static_cast<S32>(asset.mNodes.size()))
            {
                continue;
            }

            const std::string joint_name = normalizeJointName(joint_map, asset.mNodes[joint_node_idx].mName);
            auto canonical_it = canonical_indices.find(joint_name);
            if (canonical_it != canonical_indices.end())
            {
                joint_index_remap[i] = canonical_it->second;
            }
        }

        return joint_index_remap;
    }

    bool buildTriangleIndexArray(const LL::GLTF::Primitive& prim, std::vector<U32>& indices_out, std::string& error_out)
    {
        indices_out.clear();
        error_out.clear();

        std::vector<U32> base_indices = prim.mIndexArray;
        if (base_indices.empty())
        {
            base_indices.resize(prim.mPositions.size());
            for (U32 i = 0; i < base_indices.size(); ++i)
            {
                base_indices[i] = i;
            }
        }

        switch (prim.mMode)
        {
            case LL::GLTF::Primitive::Mode::TRIANGLES:
            {
                if (base_indices.size() % 3 != 0)
                {
                    error_out = "Index count is not divisible by 3.";
                    return false;
                }
                indices_out = std::move(base_indices);
                return true;
            }
            case LL::GLTF::Primitive::Mode::TRIANGLE_STRIP:
            {
                if (base_indices.size() < 3)
                {
                    error_out = "Triangle strip has fewer than 3 indices.";
                    return false;
                }
                for (size_t i = 2; i < base_indices.size(); ++i)
                {
                    U32 i0 = base_indices[i - 2];
                    U32 i1 = base_indices[i - 1];
                    U32 i2 = base_indices[i];
                    if (i % 2 == 1)
                    {
                        std::swap(i0, i1);
                    }
                    indices_out.push_back(i0);
                    indices_out.push_back(i1);
                    indices_out.push_back(i2);
                }
                return true;
            }
            case LL::GLTF::Primitive::Mode::TRIANGLE_FAN:
            {
                if (base_indices.size() < 3)
                {
                    error_out = "Triangle fan has fewer than 3 indices.";
                    return false;
                }
                for (size_t i = 2; i < base_indices.size(); ++i)
                {
                    indices_out.push_back(base_indices[0]);
                    indices_out.push_back(base_indices[i - 1]);
                    indices_out.push_back(base_indices[i]);
                }
                return true;
            }
            default:
            {
                error_out = "Unsupported primitive mode.";
                return false;
            }
        }
    }

    struct JointNodeData
    {
        JointNodeData()
            : mNodeIdx(-1)
            , mParentNodeIdx(-1)
            , mIsValidViewerJoint(false)
            , mIsParentValidViewerJoint(false)
            , mIsOverrideValid(false)
        {
        }

        S32 mNodeIdx;
        S32 mParentNodeIdx;
        glm::mat4 mGltfRestMatrix;
        glm::mat4 mViewerRestMatrix;
        glm::mat4 mOverrideRestMatrix;
        glm::mat4 mGltfMatrix;
        glm::mat4 mOverrideMatrix;
        std::string mName;
        bool mIsValidViewerJoint;
        bool mIsParentValidViewerJoint;
        bool mIsOverrideValid;
    };

    using joints_data_map_t = std::map<S32, JointNodeData>;
    using joints_name_to_node_map_t = std::map<std::string, S32, std::less<>>;

    glm::mat4 buildGltfRestMatrix(const LL::GLTF::Asset& asset,
                                  const std::vector<S32>& parent_map,
                                  const std::vector<bool>& is_skin_joint,
                                  S32 joint_node_index)
    {
        if (joint_node_index < 0 || joint_node_index >= static_cast<S32>(asset.mNodes.size()))
        {
            return glm::mat4(1.0f);
        }

        glm::mat4 rest_matrix = asset.mNodes[joint_node_index].mMatrix;
        S32 parent_idx = (joint_node_index < static_cast<S32>(parent_map.size())) ? parent_map[joint_node_index] : -1;
        while (parent_idx >= 0
               && parent_idx < static_cast<S32>(asset.mNodes.size())
               && parent_idx < static_cast<S32>(is_skin_joint.size())
               && is_skin_joint[parent_idx])
        {
            rest_matrix = asset.mNodes[parent_idx].mMatrix * rest_matrix;
            parent_idx = (parent_idx < static_cast<S32>(parent_map.size())) ? parent_map[parent_idx] : -1;
        }

        return rest_matrix;
    }

    bool checkForXYrotation(const LL::GLTF::Asset& asset,
                            const LL::GLTF::Skin& skin,
                            const std::vector<S32>& parent_map,
                            const local_joint_map_t& joint_map)
    {
        const std::vector<bool> is_skin_joint = buildSkinJointMembership(asset, skin);
        constexpr char right_shoulder[] = "mShoulderRight";
        constexpr char left_shoulder[] = "mShoulderLeft";

        const S32 joint_count = static_cast<S32>(skin.mJoints.size());
        S32 joints_found = 0;
        for (S32 i = 0; i < joint_count; ++i)
        {
            if (i >= static_cast<S32>(skin.mInverseBindMatricesData.size()))
            {
                continue;
            }

            const S32 joint_node_idx = skin.mJoints[i];
            if (joint_node_idx < 0 || joint_node_idx >= static_cast<S32>(asset.mNodes.size()))
            {
                continue;
            }

            std::string joint_name = asset.mNodes[joint_node_idx].mName;
            auto map_it = joint_map.find(joint_name);
            if (map_it == joint_map.end())
            {
                continue;
            }

            joint_name = map_it->second;
            if (joint_name != right_shoulder && joint_name != left_shoulder)
            {
                continue;
            }

            glm::mat4 gltf_joint_rest = buildGltfRestMatrix(asset, parent_map, is_skin_joint, joint_node_idx);
            glm::mat4 test_mat = glm::inverse(gltf_joint_rest) * skin.mInverseBindMatricesData[i];

            const bool is_xy_rotated =
                (fabsf(test_mat[0][0]) < 0.5f) &&
                (fabsf(test_mat[1][1]) < 0.5f) &&
                (fabsf(test_mat[2][2]) < 0.5f);

            if (!is_xy_rotated)
            {
                return false;
            }

            ++joints_found;
        }

        return joints_found == 2;
    }

    void buildOverrideMatrix(const LLJointData& viewer_data,
                             joints_data_map_t& gltf_nodes,
                             joints_name_to_node_map_t& names_to_nodes,
                             const glm::mat4& parent_rest,
                             const glm::mat4& parent_support_rest,
                             bool apply_xy_rotation)
    {
        glm::mat4 rest(1.f);
        const auto found_node = names_to_nodes.find(viewer_data.mName);
        if (found_node != names_to_nodes.end())
        {
            S32 gltf_node_idx = found_node->second;
            JointNodeData& node = gltf_nodes[gltf_node_idx];
            node.mIsOverrideValid = true;
            node.mViewerRestMatrix = viewer_data.mRestMatrix;

            glm::mat4 gltf_joint_rest_pose = convertTransformToViewerBasis(node.mGltfRestMatrix, apply_xy_rotation);

            glm::mat4 translated_joint;
            if (viewer_data.mIsJoint)
            {
                translated_joint = glm::inverse(parent_rest) * gltf_joint_rest_pose;
            }
            else
            {
                translated_joint = glm::inverse(parent_support_rest) * gltf_joint_rest_pose;
            }

            glm::vec3 translation_override(0.0f);
            glm::vec3 skew;
            glm::vec3 scale;
            glm::vec4 perspective;
            glm::quat rotation;
            glm::decompose(translated_joint, scale, rotation, translation_override, skew, perspective);

            node.mOverrideMatrix = glm::recompose(glm::vec3(1, 1, 1),
                                                  glm::identity<glm::quat>(),
                                                  translation_override,
                                                  glm::vec3(0, 0, 0),
                                                  glm::vec4(0, 0, 0, 1));

            if (viewer_data.mIsJoint)
            {
                // Keep the full local TRS for bind/rest propagation. Using the
                // translation-only override here causes parent rotations to be
                // re-encoded as rotated child translations down the chain.
                rest = parent_rest * translated_joint;
                node.mOverrideRestMatrix = rest;
            }
            else
            {
                rest = parent_support_rest * translated_joint;
                node.mOverrideRestMatrix = rest;
                rest = node.mOverrideRestMatrix;
            }
        }
        else
        {
            rest = parent_rest * viewer_data.mJointMatrix;
        }

        glm::mat4 support_rest = parent_support_rest;
        if (viewer_data.mSupport == LLJointData::SUPPORT_BASE)
        {
            support_rest = rest;
        }

        for (const LLJointData& child_data : viewer_data.mChildren)
        {
            buildOverrideMatrix(child_data, gltf_nodes, names_to_nodes, rest, support_rest, apply_xy_rotation);
        }
    }

    glm::mat4 computeGltfToViewerSkeletonTransform(const joints_data_map_t& joints_data_map,
                                                   S32 gltf_node_index,
                                                   bool apply_xy_rotation)
    {
        const auto it = joints_data_map.find(gltf_node_index);
        if (it == joints_data_map.end())
        {
            return glm::mat4(1.0f);
        }

        const JointNodeData& node_data = it->second;
        if (!node_data.mIsOverrideValid)
        {
            return glm::mat4(1.0f);
        }

        glm::mat4 rest_pose = convertTransformToViewerBasis(node_data.mGltfRestMatrix, apply_xy_rotation);
        return node_data.mOverrideRestMatrix * glm::inverse(rest_pose);
    }

}

FSLocalMeshImportGLTF::FSLocalMeshImportGLTF()
{
    mLogToInfo = false;
}

FSLocalMeshImportGLTF::loadFile_return FSLocalMeshImportGLTF::loadFile(const std::string& filename,
                                                                       LLLocalMeshFileLOD lod,
                                                                       std::vector<std::unique_ptr<LLLocalMeshObject>>& object_vector)
{
    pushLog("GLTF Importer", "Starting");
    LL_DEBUGS("LocalMesh") << "GLTF Importer: Starting" << LL_ENDL;

    LL::GLTF::Asset asset;
    setLod(lod);

    if (!asset.load(filename, false))
    {
        pushLog("GLTF Importer", "Failed to load glTF asset: " + filename, true);
        return loadFile_return(false, mLoadingLog);
    }

    for (auto& node : asset.mNodes)
    {
        node.makeMatrixValid();
    }

    mParentMap = buildParentMap(asset);

    if (asset.mScenes.empty())
    {
        pushLog("GLTF Importer", "GLTF asset contains no scenes.");
        return loadFile_return(false, mLoadingLog);
    }

    const S32 scene_idx = (asset.mScene >= 0 && asset.mScene < static_cast<S32>(asset.mScenes.size()))
        ? asset.mScene
        : 0;
    LL::GLTF::Scene& scene = asset.mScenes[scene_idx];

    // Update transforms without uploading to GL
    scene.updateTransforms(asset);

    std::vector<S32> mesh_nodes;
    for (S32 root_idx : scene.mNodes)
    {
        collectMeshNodes(asset, root_idx, mesh_nodes);
    }

    if (mesh_nodes.empty())
    {
        pushLog("GLTF Importer", "GLTF asset contains no mesh nodes.");
        return loadFile_return(false, mLoadingLog);
    }

    for (size_t object_idx = 0; object_idx < mesh_nodes.size(); ++object_idx)
    {
        S32 node_idx = mesh_nodes[object_idx];
        const LL::GLTF::Node& node = asset.mNodes[node_idx];

        if (node.mMesh < 0 || node.mMesh >= static_cast<S32>(asset.mMeshes.size()))
        {
            pushLog("GLTF Importer", "Mesh index out of bounds for node " + std::to_string(node_idx) + ", skipping.");
            continue;
        }

        std::string object_name = node.mName;
        if (object_name.empty())
        {
            object_name = asset.mMeshes[node.mMesh].mName;
        }
        if (object_name.empty())
        {
            object_name = "node_" + std::to_string(node_idx);
        }

        // LOD3 loads objects, lower LODs reuse them
        if (mLod == LLLocalMeshFileLOD::LOCAL_LOD_HIGH)
        {
            std::unique_ptr<LLLocalMeshObject> current_object = std::make_unique<LLLocalMeshObject>(object_name);
            bool object_success = processNodeMesh(asset, node, current_object.get());

            if (object_success)
            {
                pushLog("GLTF Importer", "Object loaded successfully.");
                LLMatrix4 identity_transform;
                identity_transform.setIdentity();
                postProcessObject(*current_object, identity_transform, true);
                object_vector.push_back(std::move(current_object));
            }
            else
            {
                pushLog("GLTF Importer", "Object loading failed, skipping.");
            }
        }
        else
        {
            if (object_vector.size() <= object_idx)
            {
                pushLog("GLTF Importer", "LOD" + std::to_string(mLod) + " is requesting an object that LOD3 did not have or failed to load, skipping.");
                continue;
            }

            auto current_object_ptr = object_vector[object_idx].get();
            if (!current_object_ptr)
            {
                pushLog("GLTF Importer", "Bad object reference given, skipping.");
                continue;
            }

            bool object_success = processNodeMesh(asset, node, current_object_ptr);
            if (object_success)
            {
                pushLog("GLTF Importer", "Object loaded successfully.");
                LLMatrix4 identity_transform;
                identity_transform.setIdentity();
                postProcessObject(*current_object_ptr, identity_transform, false);
            }
            else
            {
                pushLog("GLTF Importer", "Object loading failed.");
            }
        }
    }

    if (object_vector.empty())
    {
        pushLog("GLTF Importer", "No objects have been successfully loaded, stopping.");
        return loadFile_return(false, mLoadingLog);
    }

    if (mLod == LLLocalMeshFileLOD::LOCAL_LOD_HIGH)
    {
        for (const auto& object : object_vector)
        {
            finalizeSkinInfo(object.get());
        }
    }

    pushLog("GLTF Importer", "Object and face parsing complete.");
    return loadFile_return(true, mLoadingLog);
}

bool FSLocalMeshImportGLTF::processNodeMesh(const LL::GLTF::Asset& asset, const LL::GLTF::Node& node, LLLocalMeshObject* object)
{
    if (!object)
    {
        pushLog("GLTF Importer", "LLLocalMeshObject pointer is null.");
        return false;
    }

    if (node.mMesh < 0 || node.mMesh >= static_cast<S32>(asset.mMeshes.size()))
    {
        pushLog("GLTF Importer", "Invalid mesh index for node.");
        return false;
    }

    const LL::GLTF::Mesh& mesh = asset.mMeshes[node.mMesh];
    if (mesh.mPrimitives.empty())
    {
        pushLog("GLTF Importer", "Mesh has no primitives, skipping.");
        return false;
    }

    auto& object_faces = object->getFaces(mLod);
    object_faces.clear();

    S32 skin_idx = node.mSkin;
    bool apply_xy_rotation = false;
    std::vector<S32> joint_index_remap;
    std::vector<std::string> skin_joint_names;
    if (skin_idx >= 0 && skin_idx < static_cast<S32>(asset.mSkins.size()))
    {
        const auto joint_map = FSLocalMeshImportBase::loadJointMap();
        const LL::GLTF::Skin& skin = asset.mSkins[skin_idx];

        apply_xy_rotation = checkForXYrotation(asset, skin, mParentMap, joint_map);

        skin_joint_names.reserve(skin.mJoints.size());
        for (S32 joint_node_idx : skin.mJoints)
        {
            std::string joint_name;
            if (joint_node_idx >= 0 && joint_node_idx < static_cast<S32>(asset.mNodes.size()))
            {
                joint_name = asset.mNodes[joint_node_idx].mName;
            }
            skin_joint_names.push_back(normalizeJointName(joint_map, joint_name));
        }

        LLPointer<LLMeshSkinInfo> canonical_skin = object->getObjectMeshSkinInfo();
        if (mLod == LLLocalMeshFileLOD::LOCAL_LOD_HIGH
            || canonical_skin.isNull()
            || canonical_skin->mJointNames.empty())
        {
            if (!initSkinInfo(asset, skin_idx, object))
            {
                skin_idx = -1;
            }
            canonical_skin = object->getObjectMeshSkinInfo();
        }

        if (skin_idx >= 0)
        {
            joint_index_remap = buildJointIndexRemap(asset, skin, joint_map, canonical_skin);
        }
    }
    else
    {
        skin_idx = -1;
    }

    const S32 node_idx = static_cast<S32>(&node - asset.mNodes.data());
    glm::mat4 mesh_transform(1.f);
    computeCombinedNodeTransform(asset, mParentMap, node_idx, mesh_transform);
    mesh_transform = kCoordSystemRotation * mesh_transform;
    if (apply_xy_rotation)
    {
        mesh_transform = kCoordSystemRotationXY * mesh_transform;
    }

    const bool flip_winding = glm::determinant(mesh_transform) < 0.f;

    bool submesh_failure_found = false;
    bool stop_loading_additional_faces = false;

    for (size_t prim_idx = 0; prim_idx < mesh.mPrimitives.size(); ++prim_idx)
    {
        if (stop_loading_additional_faces)
        {
            break;
        }

        if (object_faces.size() >= LL_SCULPT_MESH_MAX_FACES)
        {
            pushLog("GLTF Importer", "NOTE: reached the limit of "
                + std::to_string(LL_SCULPT_MESH_MAX_FACES)
                + " faces per object, ignoring the rest.");
            stop_loading_additional_faces = true;
            break;
        }

        const LL::GLTF::Primitive& prim = mesh.mPrimitives[prim_idx];
        bool face_ok = appendPrimitiveToObject(asset,
                                               prim,
                                               mesh_transform,
                                               flip_winding,
                                               joint_index_remap,
                                               skin_joint_names,
                                               object,
                                               skin_idx);
        if (!face_ok)
        {
            submesh_failure_found = true;
        }
    }

    return !submesh_failure_found;
}

bool FSLocalMeshImportGLTF::appendPrimitiveToObject(const LL::GLTF::Asset& asset,
                                                    const LL::GLTF::Primitive& prim,
                                                    const glm::mat4& mesh_transform,
                                                    bool flip_winding,
                                                    const std::vector<S32>& joint_index_remap,
                                                    const std::vector<std::string>& skin_joint_names,
                                                    LLLocalMeshObject* object,
                                                    S32 skin_idx)
{
    auto current_submesh = std::make_unique<LLLocalMeshFace>();

    if (prim.mPositions.empty())
    {
        pushLog("GLTF Importer", "Primitive has no positions, skipping.");
        return false;
    }

    std::vector<U32> triangle_indices;
    std::string triangle_error;
    if (!buildTriangleIndexArray(prim, triangle_indices, triangle_error))
    {
        pushLog("GLTF Importer", "Primitive triangulation failed: " + triangle_error);
        return false;
    }

    for (U32 idx : triangle_indices)
    {
        if (idx >= prim.mPositions.size())
        {
            pushLog("GLTF Importer", "Primitive index out of bounds, skipping.");
            return false;
        }
    }

    const glm::mat3 normal_transform = glm::transpose(glm::inverse(glm::mat3(mesh_transform)));

    auto& list_positions = current_submesh->getPositions();
    auto& list_normals = current_submesh->getNormals();
    auto& list_uvs = current_submesh->getUVs();
    auto& list_indices = current_submesh->getIndices();

    list_positions.reserve(prim.mPositions.size());
    list_normals.reserve(prim.mPositions.size());
    list_uvs.reserve(prim.mPositions.size());

    for (size_t vert_idx = 0; vert_idx < prim.mPositions.size(); ++vert_idx)
    {
        const LLVector4a& pos = prim.mPositions[vert_idx];
        glm::vec4 transformed_pos = mesh_transform * glm::vec4(pos[0], pos[1], pos[2], 1.f);

        LLVector4 llpos;
        llpos.set(transformed_pos.x, transformed_pos.y, transformed_pos.z, 0.f);
        list_positions.push_back(llpos);

        if (vert_idx == 0)
        {
            current_submesh->setFaceBoundingBox(llpos, true);
        }
        else
        {
            current_submesh->setFaceBoundingBox(llpos);
        }

        if (!prim.mNormals.empty() && vert_idx < prim.mNormals.size())
        {
            const LLVector4a& norm = prim.mNormals[vert_idx];
            glm::vec3 transformed_norm = normal_transform * glm::vec3(norm[0], norm[1], norm[2]);
            LLVector4 llnorm;
            llnorm.set(transformed_norm.x, transformed_norm.y, transformed_norm.z, 0.f);
            list_normals.push_back(llnorm);
        }
        else
        {
            list_normals.emplace_back(0.f, 0.f, 1.f, 0.f);
        }

        if (!prim.mTexCoords0.empty() && vert_idx < prim.mTexCoords0.size())
        {
            const LLVector2& uv = prim.mTexCoords0[vert_idx];
            list_uvs.emplace_back(uv.mV[0], -uv.mV[1]);
        }
    }

    list_indices.reserve(triangle_indices.size());
    for (size_t idx = 0; idx < triangle_indices.size(); idx += 3)
    {
        list_indices.push_back(static_cast<S32>(triangle_indices[idx]));
        list_indices.push_back(static_cast<S32>(triangle_indices[idx + (flip_winding ? 2 : 1)]));
        list_indices.push_back(static_cast<S32>(triangle_indices[idx + (flip_winding ? 1 : 2)]));
    }

    // weights and joints
    if (skin_idx >= 0
        && skin_idx < static_cast<S32>(asset.mSkins.size())
        && !prim.mWeights.empty()
        && !prim.mJoints.empty()
        && prim.mWeights.size() == prim.mPositions.size()
        && prim.mJoints.size() == prim.mPositions.size())
    {
        const LL::GLTF::Skin& skin = asset.mSkins[skin_idx];

        LL::GLTF::Accessor::ComponentType joint_component_type = LL::GLTF::Accessor::ComponentType::UNSIGNED_BYTE;
        auto joint_attr_it = prim.mAttributes.find("JOINTS_0");
        if (joint_attr_it != prim.mAttributes.end() && joint_attr_it->second >= 0)
        {
            joint_component_type = asset.mAccessors[joint_attr_it->second].mComponentType;
        }

        auto& list_skin = current_submesh->getSkin();
        list_skin.reserve(prim.mWeights.size());
        S32 dropped_weighted_influences = 0;
        S32 affected_vertices = 0;
        std::set<std::string, std::less<>> dropped_joint_names;

        for (size_t vert_idx = 0; vert_idx < prim.mWeights.size(); ++vert_idx)
        {
            const LLVector4a& weight_vec = prim.mWeights[vert_idx];
            const float* weights = weight_vec.getF32ptr();

            std::array<int, 4> joint_indices{};
            if (joint_component_type == LL::GLTF::Accessor::ComponentType::UNSIGNED_SHORT)
            {
                const glm::u16vec4 unpacked_joints = glm::unpackUint4x16(prim.mJoints[vert_idx]);
                joint_indices = { static_cast<int>(unpacked_joints.x),
                                  static_cast<int>(unpacked_joints.y),
                                  static_cast<int>(unpacked_joints.z),
                                  static_cast<int>(unpacked_joints.w) };
            }
            else
            {
                const glm::u8vec4 unpacked_joints = glm::unpackUint4x8(static_cast<U32>(prim.mJoints[vert_idx] & 0xFFFFFFFF));
                joint_indices = { static_cast<int>(unpacked_joints.x),
                                  static_cast<int>(unpacked_joints.y),
                                  static_cast<int>(unpacked_joints.z),
                                  static_cast<int>(unpacked_joints.w) };
            }

            float weight_values[4] = { weights[0], weights[1], weights[2], weights[3] };

            // Normalize and clamp
            float total = weight_values[0] + weight_values[1] + weight_values[2] + weight_values[3];
            if (total > 0.f)
            {
                for (float& w : weight_values)
                {
                    w /= total;
                }
            }

            LLLocalMeshFace::LLLocalMeshSkinUnit unit{};
            bool vertex_dropped_joint = false;
            for (size_t j = 0; j < 4; ++j)
            {
                S32 remapped_joint = joint_indices[j];
                if (!joint_index_remap.empty())
                {
                    if (remapped_joint < 0 || remapped_joint >= static_cast<S32>(joint_index_remap.size()))
                    {
                        remapped_joint = -1;
                    }
                    else
                    {
                        remapped_joint = joint_index_remap[remapped_joint];
                    }
                }

                if (weight_values[j] > 0.f && remapped_joint < 0)
                {
                    ++dropped_weighted_influences;
                    vertex_dropped_joint = true;
                    if (joint_indices[j] >= 0 && joint_indices[j] < static_cast<S32>(skin_joint_names.size()))
                    {
                        dropped_joint_names.emplace(skin_joint_names[joint_indices[j]]);
                    }
                }

                if (remapped_joint < 0 || weight_values[j] <= 0.f)
                {
                    unit.mJointIndices[j] = -1;
                    unit.mJointWeights[j] = 0.f;
                }
                else
                {
                    unit.mJointIndices[j] = remapped_joint;
                    unit.mJointWeights[j] = llclamp(weight_values[j], 0.f, 0.999f);
                }
            }

            if (vertex_dropped_joint)
            {
                ++affected_vertices;
            }

            list_skin.emplace_back(unit);
        }

        if (dropped_weighted_influences > 0)
        {
            std::ostringstream joint_stream;
            bool first_joint = true;
            for (const auto& joint_name : dropped_joint_names)
            {
                if (!first_joint)
                {
                    joint_stream << ", ";
                }
                joint_stream << joint_name;
                first_joint = false;
            }

            std::string warning = "LOD" + std::to_string(mLod)
                + " object \"" + object->getObjectName()
                + "\" dropped " + std::to_string(dropped_weighted_influences)
                + " weighted joint influence(s) across " + std::to_string(affected_vertices)
                + " vertex/vertices while remapping to the high LOD skin";

            if (!dropped_joint_names.empty())
            {
                warning += " [" + joint_stream.str() + "]";
            }

            pushLog("GLTF Importer", "WARNING: " + warning);
            LL_WARNS("LocalMesh") << warning << LL_ENDL;
        }
    }
    else if (skin_idx >= 0)
    {
        pushLog("GLTF Importer", "Skinning data missing or mismatched for primitive, skipping weights.");
    }

    object->getFaces(mLod).push_back(std::move(current_submesh));
    return true;
}

bool FSLocalMeshImportGLTF::initSkinInfo(const LL::GLTF::Asset& asset, S32 skin_idx, LLLocalMeshObject* object)
{
    if (!object || skin_idx < 0 || skin_idx >= static_cast<S32>(asset.mSkins.size()))
    {
        return false;
    }

    LLPointer<LLMeshSkinInfo> skininfop = object->getObjectMeshSkinInfo();
    if (skininfop == nullptr)
    {
        LL_DEBUGS("LocalMesh") << "Object mesh skin info is nullptr. allocate a new skininfo." << LL_ENDL;
        try
        {
            skininfop = new LLMeshSkinInfo();
        }
        catch (const std::bad_alloc& ex)
        {
            LL_WARNS() << "Failed to allocate skin info with exception: " << ex.what() << LL_ENDL;
            return false;
        }
    }

    auto joint_map = FSLocalMeshImportBase::loadJointMap();
    const LL::GLTF::Skin& skin = asset.mSkins[skin_idx];
    const bool apply_xy_rotation = checkForXYrotation(asset, skin, mParentMap, joint_map);
    U32 recognized_joint_count = 0;
    const std::vector<bool> is_skin_joint = buildSkinJointMembership(asset, skin);
    for (S32 joint_node_idx : skin.mJoints)
    {
        if (joint_node_idx >= 0
            && joint_node_idx < static_cast<S32>(asset.mNodes.size())
            && joint_map.find(asset.mNodes[joint_node_idx].mName) != joint_map.end())
        {
            ++recognized_joint_count;
        }
    }

    joints_data_map_t joints_data;
    joints_name_to_node_map_t names_to_nodes;
    std::vector<LLJointData> viewer_skeleton;
    if (gAgentAvatarp)
    {
        gAgentAvatarp->getJointMatricesAndHierarhy(viewer_skeleton);
    }

    const bool can_build_overrides = !viewer_skeleton.empty();
    if (can_build_overrides)
    {
        for (size_t i = 0; i < skin.mJoints.size(); ++i)
        {
            S32 joint_node_idx = skin.mJoints[i];
            if (joint_node_idx < 0 || joint_node_idx >= static_cast<S32>(asset.mNodes.size()))
            {
                continue;
            }

            const LL::GLTF::Node& joint_node = asset.mNodes[joint_node_idx];
            JointNodeData& data = joints_data[joint_node_idx];
            data.mNodeIdx = joint_node_idx;
            data.mGltfRestMatrix = buildGltfRestMatrix(asset, mParentMap, is_skin_joint, joint_node_idx);
            data.mGltfMatrix = joint_node.mMatrix;
            data.mOverrideMatrix = glm::mat4(1.f);

            auto name_it = joint_map.find(joint_node.mName);
            if (name_it != joint_map.end())
            {
                data.mName = name_it->second;
                data.mIsValidViewerJoint = true;
            }
            else
            {
                data.mName = joint_node.mName;
                data.mIsValidViewerJoint = false;
            }

            names_to_nodes[data.mName] = joint_node_idx;

            for (S32 child_idx : joint_node.mChildren)
            {
                JointNodeData& child_data = joints_data[child_idx];
                child_data.mParentNodeIdx = joint_node_idx;
                child_data.mIsParentValidViewerJoint = data.mIsValidViewerJoint;
            }
        }

        glm::mat4 identity(1.0f);
        for (const LLJointData& viewer_data : viewer_skeleton)
        {
            buildOverrideMatrix(viewer_data, joints_data, names_to_nodes, identity, identity, apply_xy_rotation);
        }
    }

    if (!enforceRigJointLimit("GLTF Importer", *object, skininfop, recognized_joint_count))
    {
        return false;
    }

    // Always reset bind shape before rebuilding skin data so reloads do not reuse stale matrices.
    skininfop->mBindShapeMatrix = LLMatrix4a::identity();
    skininfop->mBindPoseMatrix.clear();

    skininfop->mJointNames.clear();
    skininfop->mJointNums.clear();
    skininfop->mInvBindMatrix.clear();
    skininfop->mAlternateBindMatrix.clear();
    skininfop->mInvalidJointsScrubbed = false;
    skininfop->mJointNumsInitialized = false;

    static LLCachedControl<bool> apply_joint_offsets(gSavedSettings, "FSLocalMeshApplyJointOffsets");

    for (size_t i = 0; i < skin.mJoints.size(); ++i)
    {
        S32 joint_node_idx = skin.mJoints[i];
        std::string joint_name;
        if (joint_node_idx >= 0 && joint_node_idx < static_cast<S32>(asset.mNodes.size()))
        {
            joint_name = asset.mNodes[joint_node_idx].mName;
        }

        joint_name = normalizeJointName(joint_map, joint_name);

        skininfop->mJointNames.push_back(joint_name);
        skininfop->mJointNums.push_back(-1);

        glm::mat4 original_bind = glm::mat4(1.0f);
        if (i < skin.mInverseBindMatricesData.size())
        {
            original_bind = glm::inverse(skin.mInverseBindMatricesData[i]);
        }

        glm::mat4 rotated_bind = convertTransformToViewerBasis(original_bind, apply_xy_rotation);
        glm::mat4 skeleton_transform = glm::mat4(1.0f);
        if (can_build_overrides)
        {
            skeleton_transform = computeGltfToViewerSkeletonTransform(joints_data, joint_node_idx, apply_xy_rotation);
        }

        glm::mat4 translated_bind = skeleton_transform * rotated_bind;
        glm::mat4 final_inverse_bind = glm::inverse(translated_bind);

        LLMatrix4 inv_bind_ll(glm::value_ptr(final_inverse_bind));
        skininfop->mInvBindMatrix.push_back(LLMatrix4a(inv_bind_ll));

        if (apply_joint_offsets && can_build_overrides)
        {
            LLMatrix4 alternate_bind(inv_bind_ll);
            auto joint_it = joints_data.find(joint_node_idx);
            if (joint_it != joints_data.end())
            {
                LLMatrix4 override_transform(glm::value_ptr(joint_it->second.mOverrideMatrix));
                alternate_bind.setTranslation(override_transform.getTranslation());
            }
            skininfop->mAlternateBindMatrix.push_back(LLMatrix4a(alternate_bind));
        }
    }

    object->setObjectMeshSkinInfo(skininfop);
    return true;
}

void FSLocalMeshImportGLTF::finalizeSkinInfo(LLLocalMeshObject* object) const
{
    if (!object)
    {
        return;
    }

    LLPointer<LLMeshSkinInfo> skininfop = object->getObjectMeshSkinInfo();
    if (skininfop.isNull() || skininfop->mInvBindMatrix.empty())
    {
        return;
    }

    LLMatrix4 normalized_transformation = FSLocalMeshImportBase::buildNormalizedTransformation(*object);
    bool bind_shape_finite = true;
    const F32* bind_shape_ptr = skininfop->mBindShapeMatrix.getF32ptr();
    for (int i = 0; i < 16; ++i)
    {
        if (!llfinite(bind_shape_ptr[i]))
        {
            bind_shape_finite = false;
            break;
        }
    }

    if (!bind_shape_finite)
    {
        skininfop->mBindShapeMatrix = LLMatrix4a::identity();
    }

    // Local mesh vertices are normalized into object space before upload; bind shape
    // must always carry the inverse normalization back into skin space.
    LLMatrix4a transform{normalized_transformation};
    matMul(transform, skininfop->mBindShapeMatrix, skininfop->mBindShapeMatrix);

    FSLocalMeshImportBase::buildBindPoseMatrix(skininfop);

    skininfop->updateHash();
    object->setObjectMeshSkinInfo(skininfop);
}
