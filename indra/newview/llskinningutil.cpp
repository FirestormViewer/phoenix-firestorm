/** 
* @file llskinningutil.cpp
* @brief  Functions for mesh object skinning
* @author vir@lindenlab.com
*
* $LicenseInfo:firstyear=2015&license=viewerlgpl$
* Second Life Viewer Source Code
* Copyright (C) 2015, Linden Research, Inc.
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

#include "llskinningutil.h"
#include "llvoavatar.h"
#include "llviewercontrol.h"
#include "llmeshrepository.h"

// static
void LLSkinningUtil::initClass()
{
}

// static
U32 LLSkinningUtil::getMaxJointCount()
{
    U32 result = LL_MAX_JOINTS_PER_MESH_OBJECT;
	return result;
}

// static
U32 LLSkinningUtil::getMeshJointCount(const LLMeshSkinInfo *skin)
{
	return llmin((U32)getMaxJointCount(), (U32)skin->mJointNames.size());
}

// static
void LLSkinningUtil::scrubInvalidJoints(LLVOAvatar *avatar, LLMeshSkinInfo* skin)
{
    if (skin->mInvalidJointsScrubbed)
    {
        return;
    }
    for (U32 j = 0; j < skin->mJointNames.size(); ++j)
    {
        // Fix invalid names to "mPelvis". Currently meshes with
        // invalid names will be blocked on upload, so this is just
        // needed for handling of any legacy bad data.
        if (!avatar->getJoint(skin->mJointNames[j]))
        {
            //<FS:ND> Query by JointKey rather than just a string, the key can be a U32 index for faster lookup
            //LL_DEBUGS("Avatar") << "Mesh rigged to invalid joint" << skin->mJointNames[j] << LL_ENDL;
            //skin->mJointNames[ j ] = "mPelvis";
            LL_DEBUGS("Avatar") << "Mesh rigged to invalid joint" << skin->mJointNames[j].mName << LL_ENDL;
            skin->mJointNames[ j ] = JointKey::construct( "mPelvis" );
            //</FS:ND>
        }
    }
    skin->mInvalidJointsScrubbed = true;
}

// static
void LLSkinningUtil::initSkinningMatrixPalette(
    LLMatrix4* mat,
    S32 count, 
    const LLMeshSkinInfo* skin,
    LLVOAvatar *avatar)
{
    for (U32 j = 0; j < count; ++j)
    {
        LLJoint *joint = NULL;
        if (skin->mJointNums[j] == -1)
        {
            joint = avatar->getJoint(skin->mJointNames[j]);
            if (joint)
            {
                skin->mJointNums[j] = joint->getJointNum();
            }
        }
		else
		{
			joint = avatar->getJoint(skin->mJointNums[j]);
		}
        if (joint)
        {
#define MAT_USE_SSE
#ifdef MAT_USE_SSE
            LLMatrix4a bind, world, res;
            bind.loadu(skin->mInvBindMatrix[j]);
            world.loadu(joint->getWorldMatrix());
            matMul(bind,world,res);
            memcpy(mat[j].mMatrix,res.mMatrix,16*sizeof(float));
#else
            mat[j] = skin->mInvBindMatrix[j];
            mat[j] *= joint->getWorldMatrix();
#endif
        }
        else
        {
            mat[j] = skin->mInvBindMatrix[j];
            // This  shouldn't  happen   -  in  mesh  upload,  skinned
            // rendering  should  be disabled  unless  all joints  are
            // valid.  In other  cases of  skinned  rendering, invalid
            // joints should already have  been removed during scrubInvalidJoints().
            LL_WARNS_ONCE("Avatar") << "Rigged to invalid joint name " << skin->mJointNames[j] << LL_ENDL;
        }
    }
}

// static
void LLSkinningUtil::checkSkinWeights(LLVector4a* weights, U32 num_vertices, const LLMeshSkinInfo* skin)
{
#ifndef LL_RELEASE_FOR_DOWNLOAD
	const S32 max_joints = skin->mJointNames.size();
    for (U32 j=0; j<num_vertices; j++)
    {
        F32 *w = weights[j].getF32ptr();
            
        F32 wsum = 0.0;
        for (U32 k=0; k<4; ++k)
        {
            S32 i = llfloor(w[k]);
            llassert(i>=0);
            llassert(i<max_joints);
            wsum += w[k]-i;
        }
        llassert(wsum > 0.0f);
    }
#endif
}

void LLSkinningUtil::scrubSkinWeights(LLVector4a* weights, U32 num_vertices, const LLMeshSkinInfo* skin)
{
    const S32 max_joints = skin->mJointNames.size();
    for (U32 j=0; j<num_vertices; j++)
    {
        F32 *w = weights[j].getF32ptr();

        for (U32 k=0; k<4; ++k)
        {
            S32 i = llfloor(w[k]);
            F32 f = w[k]-i;
            i = llclamp(i,0,max_joints-1);
            w[k] = i + f;
        }
    }
	checkSkinWeights(weights, num_vertices, skin);
}

// static
void LLSkinningUtil::getPerVertexSkinMatrix(
    F32* weights,
    LLMatrix4a* mat,
    bool handle_bad_scale,
    LLMatrix4a& final_mat,
    U32 max_joints)
{
    bool valid_weights = true;
    final_mat.clear();

    S32 idx[4];

    LLVector4 wght;

    F32 scale = 0.f;
    for (U32 k = 0; k < 4; k++)
    {
        F32 w = weights[k];

        // BENTO potential optimizations
        // - Do clamping in unpackVolumeFaces() (once instead of every time)
        // - int vs floor: if we know w is
        // >= 0.0, we can use int instead of floorf; the latter
        // allegedly has a lot of overhead due to ieeefp error
        // checking which we should not need.
        idx[k] = llclamp((S32) floorf(w), (S32)0, (S32)max_joints-1);

        wght[k] = w - floorf(w);
        scale += wght[k];
    }
    if (handle_bad_scale && scale <= 0.f)
    {
        wght = LLVector4(1.0f, 0.0f, 0.0f, 0.0f);
        valid_weights = false;
    }
    else
    {
        // This is enforced  in unpackVolumeFaces()
        llassert(scale>0.f);
        wght *= 1.f/scale;
    }

    for (U32 k = 0; k < 4; k++)
    {
        F32 w = wght[k];

        LLMatrix4a src;
        src.setMul(mat[idx[k]], w);

        final_mat.add(src);
    }
    // SL-366 - with weight validation/cleanup code, it should no longer be
    // possible to hit the bad scale case.
    llassert(valid_weights);
}

namespace FSSkinningUtil
{
    void getPerVertexSkinMatrixSSE( LLVector4a const &weights, LLMatrix4a* mat, bool handle_bad_scale, LLMatrix4a& final_mat, U32 max_joints )
    {
        final_mat.clear();
        
        llassert_always( !handle_bad_scale );
    
        LL_ALIGN_16( S32 idx[4] );
        LL_ALIGN_16( F32 wght[4] );

        __m128i _mMaxIdx = _mm_set_epi16( max_joints-1, max_joints-1, max_joints-1, max_joints-1, max_joints-1, max_joints-1, max_joints-1, max_joints-1 );
        __m128i _mIdx = _mm_cvttps_epi32( (__m128)weights );
        __m128 _mWeight = _mm_sub_ps( (__m128)weights, _mm_cvtepi32_ps( _mIdx ) );

        _mIdx = _mm_min_epi16( _mIdx, _mMaxIdx );
        _mm_store_si128( (__m128i*)idx, _mIdx );
            
        __m128 _mScale = _mm_add_ps( _mWeight, _mm_movehl_ps( _mWeight, _mWeight ));
        _mScale = _mm_add_ss( _mScale, _mm_shuffle_ps( _mScale, _mScale, 1) );
        _mScale = _mm_shuffle_ps( _mScale, _mScale, 0 );
        
        _mWeight = _mm_div_ps( _mWeight, _mScale );
        _mm_store_ps( wght, _mWeight );
        
        for (U32 k = 0; k < 4; k++)
        {
            F32 w = wght[k];
            
            LLMatrix4a src;
            src.setMul(mat[idx[k]], w);
            
            final_mat.add(src);
        }
    }
}
