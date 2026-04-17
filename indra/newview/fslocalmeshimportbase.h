/**
 * @file fslocalmeshimportbase.h
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

#pragma once

#include "vjlocalmesh.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

class FSLocalMeshImportBase
{
public:
    using JointMap = std::map<std::string, std::string, std::less<>>;
    using loadFile_return = std::pair<bool, std::vector<std::string>>;
    virtual ~FSLocalMeshImportBase() = default;

protected:
    void setLod(LLLocalMeshFileLOD lod);
    void pushLog(const std::string& who, const std::string& what, bool is_error = false);
    void postProcessObject(LLLocalMeshObject& object, const LLMatrix4& scene_transform, bool compute_bounds);
    bool enforceRigJointLimit(const std::string& who,
                              LLLocalMeshObject& object,
                              LLPointer<LLMeshSkinInfo> skininfop,
                              U32 recognized_joint_count);

    static JointMap loadJointMap();
    static LLMatrix4 buildNormalizedTransformation(const LLLocalMeshObject& object);
    static void buildBindPoseMatrix(LLPointer<LLMeshSkinInfo> skininfop);

protected:
    LLLocalMeshFileLOD mLod{ LLLocalMeshFileLOD::LOCAL_LOD_HIGH };
    std::vector<std::string> mLoadingLog;
    bool mLogToInfo{ true };
};
