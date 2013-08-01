/**
 * @file llmeshsearch.cpp
 * @brief Search meshes efficiently
 * @author Karl Stiefvater <qarl@qarl.com>
 *
 * $LicenseInfo:firstyear=2013&license=viewerlgpl$
 * InWorldz Viewer Source Code
 * Copyright (C) 2013, InWorldz, LLC.
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License or later.
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

class LLVector3;
class LLBBox;

#ifndef LL_MESHSEARCH_H
#define LL_MESHSEARCH_H

class LLMeshSearch
{
	std::vector<LLVector3> mPositions;
    std::vector<S32> mIndices;
    LLBBoxLocal mBBox;
    
    // voxel table
    F32 mVoxelSize;
    U32 mWidth;
    U32 mHeight;
    U32 mDepth;
    
    std::vector<std::vector<S32>* > mVoxels;
      
    
public:
    LLMeshSearch(std::vector<LLVector3>& positions, std::vector<S32>& indices);
    ~LLMeshSearch();
    
    void cellLocation(LLVector3& position, S32& x, S32& y, S32& z);
    void insertTriangle(S32 triangle_index, S32 x, S32 y, S32 z);

    
    BOOL findClosestPoint(LLVector3 target, F32 within, F32& distance, S32& index0, S32& index1, S32& index2, F32& bary_a, F32& bary_b);
};


#endif // LL_MESHSEARCH_H








