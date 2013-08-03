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



#include "llviewerprecompiledheaders.h"


#include "llmeshsearch.h"

#include "llrand.h"
#include "llvolume.h"


LLMeshSearch::LLMeshSearch(std::vector<LLVector3>& positions, std::vector<S32>& indices) :
    mPositions(positions), mIndices(indices)
{
    // compute bounding box
    mBBox.setMin(mPositions[0]);
    mBBox.setMax(mPositions[0]);
    for (S32 i = 0; i < mPositions.size(); i++)
    {
        mBBox.addPoint(mPositions[i]);
    }
    
    // size of each voxel - emperically determined to give good performance
    mVoxelSize = 0.05f;
    
    mWidth = (S32)ceil(mBBox.getExtent().mV[VX] / mVoxelSize);
    mHeight = (S32)ceil(mBBox.getExtent().mV[VY] / mVoxelSize);
    mDepth = (S32)ceil(mBBox.getExtent().mV[VZ] / mVoxelSize);

    // the voxel table
    mVoxels.resize(mWidth * mHeight * mDepth, NULL);
    
    
    // insert triangles into voxel table
    for (S32 triangle_index = 0; triangle_index < mIndices.size(); triangle_index += 3)
    {
        S32 index0 = mIndices[triangle_index];
        S32 index1 = mIndices[triangle_index+1];
        S32 index2 = mIndices[triangle_index+2];
        
        LLVector3& position0 = mPositions[index0];
        LLVector3& position1 = mPositions[index1];
        LLVector3& position2 = mPositions[index2];
        
        S32 x, y, z;
        
        cellLocation(position0, x, y, z);
        insertTriangle(triangle_index, x, y, z);
        cellLocation(position1, x, y, z);
        insertTriangle(triangle_index, x, y, z);
        cellLocation(position2, x, y, z);
        insertTriangle(triangle_index, x, y, z);
    }
}


LLMeshSearch::~LLMeshSearch()
{
    for (S32 i = 0; i < mVoxels.size(); i++)
    {
        if (mVoxels[i])
        {
            delete mVoxels[i];
        }
    }
}

// given a point in space, find it's entry coordinates in the voxel table
void LLMeshSearch::cellLocation(LLVector3& position, S32& x, S32& y, S32& z)
{
    LLVector3 local_position = position - mBBox.getMin();
    x = (S32)floor(local_position.mV[VX] / mVoxelSize);
    y = (S32)floor(local_position.mV[VY] / mVoxelSize);
    z = (S32)floor(local_position.mV[VZ] / mVoxelSize);
}


// insert a new triangle into the voxel table
void LLMeshSearch::insertTriangle(S32 triangle_index, S32 x, S32 y, S32 z)
{
    S32 voxel_index = (x * mHeight + y) * mDepth + z;
    
    std::vector<S32>* vector_pointer = mVoxels[voxel_index];
    
    if (!vector_pointer)
    {
        vector_pointer = new std::vector<S32>;
        mVoxels[voxel_index] = vector_pointer;
    }
    
    // don't insert dupes (which will only appear sequentially so this test catches them all)
    if ((vector_pointer->size() == 0) || (vector_pointer->back() != triangle_index))
    {
        vector_pointer->push_back(triangle_index);
    }
}


// find point on mesh which is closest to target mesh, within a given distance, return TRUE if found

BOOL LLMeshSearch::findClosestPoint(LLVector3 target, F32 within,
                                    F32& distance, S32& index0, S32& index1, S32& index2, F32& bary_a, F32& bary_b)
{
    BOOL found = FALSE;
    
    F32 closest_distance_squared = powf(within, 2.0f);
    
    S32 target_x, target_y, target_z;
    cellLocation(target, target_x, target_y, target_z);
    
    // cycle over all voxels
    for (S32 x = 0; x < mWidth; x++)
    {
        F32 delta_x = llmax(abs(x-target_x)-1, 0) * mVoxelSize;
        F32 delta_x_squared = delta_x * delta_x;
        
        // if this plane is too far away, skip it
        if ((delta_x_squared) > closest_distance_squared)
            continue;
        

        for (S32 y = 0; y < mHeight; y++)
        {
            F32 delta_y = llmax(abs(y-target_y)-1, 0) * mVoxelSize;
            F32 delta_y_squared = delta_y * delta_y;
        
            // if this line is too far away, skip it
            if ((delta_x_squared + delta_y_squared) > closest_distance_squared)
                continue;
            

            for (S32 z = 0; z < mDepth; z++)
            {
                F32 delta_z = llmax(abs(z-target_z)-1, 0) * mVoxelSize;
                F32 delta_z_squared = delta_z * delta_z;
                
                // if this voxel is too far away, skip it
                if ((delta_x_squared + delta_y_squared + delta_z_squared) > closest_distance_squared)
                    continue;
                
                S32 voxel_index = (x * mHeight + y) * mDepth + z;
                
                std::vector<S32>* vector_pointer = mVoxels[voxel_index];
                
                // if the voxel is empty, skip it
                if (!vector_pointer)
                    continue;

                
                // cycle over all triangles
                for (S32 j = 0; j < vector_pointer->size(); j++)
                {
                    S32 triangle_index = (*vector_pointer)[j];
                    
                    // indices for triangle corners
                    S32 this_index0 = mIndices[triangle_index+0];
                    S32 this_index1 = mIndices[triangle_index+1];
                    S32 this_index2 = mIndices[triangle_index+2];
                    
                    // positions of triangle corners
                    LLVector3 this_position0 = mPositions[this_index0];
                    LLVector3 this_position1 = mPositions[this_index1];
                    LLVector3 this_position2 = mPositions[this_index2];
                    
                    // barycentric coord of closest point
                    F32 this_bary_a;
                    F32 this_bary_b;
                    
                    F32 this_distance_squared = LLTriangleClosestPoint(this_position0, this_position1, this_position2,
                                                                       target, this_bary_a, this_bary_b);
                    
                    
                    if (this_distance_squared < closest_distance_squared)
                    {
                        found = TRUE;
                        
                        // record corner indices of this triangle
                        index0 = this_index0;
                        index1 = this_index1;
                        index2 = this_index2;
                        
                        // compute weights from barycentric coordinates
                        bary_a = this_bary_a;
                        bary_b = this_bary_b;
                        
                        closest_distance_squared = this_distance_squared;
                    }
                    
                    
                }

            }
        }
    }
    
      
    if (found)
    {
        distance = sqrt(closest_distance_squared);
    }
     
    
    return found;
}
