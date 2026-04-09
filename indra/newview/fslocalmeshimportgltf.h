/**
 * @file fslocalmeshimportgltf.h
 * @author Beq Janus
 * @brief Local Mesh glTF importer header
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

#pragma once

#include "fslocalmeshimportbase.h"

// GLM headers
#include <glm/mat4x4.hpp>

// STL headers
#include <string>
#include <vector>

namespace LL
{
    namespace GLTF
    {
        class Asset;
        class Primitive;
        class Node;
    }
}

class FSLocalMeshImportGLTF : public FSLocalMeshImportBase
{
public:
    using loadFile_return = FSLocalMeshImportBase::loadFile_return;
    FSLocalMeshImportGLTF();
    loadFile_return loadFile(const std::string& filename,
                             LLLocalMeshFileLOD lod,
                             std::vector<std::unique_ptr<LLLocalMeshObject>>& object_vector);

private:
    bool processNodeMesh(const LL::GLTF::Asset& asset, const LL::GLTF::Node& node, LLLocalMeshObject* object);
    bool appendPrimitiveToObject(const LL::GLTF::Asset& asset,
                                 const LL::GLTF::Primitive& prim,
                                 const glm::mat4& mesh_transform,
                                 bool flip_winding,
                                 const std::vector<S32>& joint_index_remap,
                                 const std::vector<std::string>& skin_joint_names,
                                 LLLocalMeshObject* object,
                                 S32 skin_idx);
    bool initSkinInfo(const LL::GLTF::Asset& asset, S32 skin_idx, LLLocalMeshObject* object);
    void finalizeSkinInfo(LLLocalMeshObject* object) const;
private:
    std::vector<S32> mParentMap;
};
