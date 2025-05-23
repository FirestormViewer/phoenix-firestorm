/**
 * @file vjlocalmeshimportdae.h
 * @author Vaalith Jinn
 * @brief Local Mesh dae importer header
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

#include "vjlocalmesh.h"

 // collada dom magic
#include <dom/domMesh.h>

// formal declarations
class LLLocalMeshObject;
class LLLocalMeshFace;
class LLLocalMeshFileData;

/*
    NOTE: basically everything here is just a refactor of lldaeloader
    in what is hopefully more modernized and easier to understand c++
    and more importantly - less dependent on the caller.

    there's only so much of a wheel you can reinvent.

    one important note: i explicitly excluded the parts that add
    additional faces (on too many verts) and additional objects (on too many faces)
    because that perpetuates bad habits in bad creators.

    if someone wants to upload a mesh object with 400 faces -
    let them do it by hand, or eat cake, or something.
*/

class LLLocalMeshImportDAE
{
public:
    // use default constructor/destructor if we can get away with it.

public:
    typedef std::pair<bool, std::vector<std::string>> loadFile_return;
    loadFile_return loadFile(LLLocalMeshFile* data, LLLocalMeshFileLOD lod);
    bool processObject(domMesh* current_mesh, LLLocalMeshObject* current_object);
    bool processSkin(daeDatabase* collada_db, daeElement* collada_document_root, domMesh* current_mesh, domSkin* current_skin, std::unique_ptr<LLLocalMeshObject>& current_object);
    bool processSkeletonJoint(domNode* current_node, std::map<std::string, std::string, std::less<>>& joint_map, std::map<std::string, LLMatrix4>& joint_transforms, bool recurse_children = false);

    bool readMesh_CommonElements(const domInputLocalOffset_Array& inputs,
        int& offset_position, int& offset_normals, int& offset_uvmap, int& index_stride,
        domSource*& source_position, domSource*& source_normals, domSource*& source_uvmap);

    std::string getElementName(daeElement* element_current, int fallback_index);

    // mesh data read functions, basically refactored from what's already in lldaeloader
    // consolidating them into a single mega function makes for very sketchy readability.
    bool readMesh_Triangle(LLLocalMeshFace* data_out, const domTrianglesRef& data_in);
    bool readMesh_Polylist(LLLocalMeshFace* data_out, const domPolylistRef& data_in);

    void pushLog(const std::string& who, const std::string& what, bool is_error=false);

private:
    LLLocalMeshFileLOD mLod;
    std::vector<std::string> mLoadingLog;
};

