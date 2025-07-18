/**
 * @file llvosurfacepatch.cpp
 * @brief Viewer-object derived "surface patch", which is a piece of terrain
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
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

#include "llvosurfacepatch.h"

#include "lldrawpoolterrain.h"

#include "lldrawable.h"
#include "llface.h"
#include "llprimitive.h"
#include "llsky.h"
#include "llsurfacepatch.h"
#include "llsurface.h"
#include "llviewerobjectlist.h"
#include "llviewerregion.h"
#include "llvlcomposition.h"
#include "llvolume.h"
#include "llvovolume.h"
#include "pipeline.h"
#include "llspatialpartition.h"

F32 LLVOSurfacePatch::sLODFactor = 1.f;

LLVOSurfacePatch::LLVOSurfacePatch(const LLUUID &id, const LLPCode pcode, LLViewerRegion *regionp)
    :   LLStaticViewerObject(id, LL_VO_SURFACE_PATCH, regionp),
        mDirtiedPatch(false),
        mPool(NULL),
        mBaseComp(0),
        mPatchp(NULL),
        mDirtyTexture(false),
        mDirtyTerrain(false),
        mLastNorthStride(0),
        mLastEastStride(0),
        mLastStride(0),
        mLastLength(0)
{
    // Terrain must draw during selection passes so it can block objects behind it.
    mbCanSelect = true;
    setScale(LLVector3(16.f, 16.f, 16.f)); // Hack for setting scale for bounding boxes/visibility.
}


LLVOSurfacePatch::~LLVOSurfacePatch()
{
    mPatchp = NULL;
}


void LLVOSurfacePatch::markDead()
{
    if (mPatchp)
    {
        mPatchp->clearVObj();
        mPatchp = NULL;
    }
    LLViewerObject::markDead();
}


bool LLVOSurfacePatch::isActive() const
{
    return false;
}


void LLVOSurfacePatch::setPixelAreaAndAngle(LLAgent &agent)
{
    mAppAngle = 50;
    mPixelArea = 500*500;
}


void LLVOSurfacePatch::updateTextures()
{
}


LLFacePool *LLVOSurfacePatch::getPool()
{
    mPool = (LLDrawPoolTerrain*) gPipeline.getPool(LLDrawPool::POOL_TERRAIN, mPatchp->getSurface()->getSTexture());

    return mPool;
}


LLDrawable *LLVOSurfacePatch::createDrawable(LLPipeline *pipeline)
{
    pipeline->allocDrawable(this);

    mDrawable->setRenderType(LLPipeline::RENDER_TYPE_TERRAIN);

    mBaseComp = llfloor(mPatchp->getMinComposition());
    S32 min_comp, max_comp, range;
    min_comp = llfloor(mPatchp->getMinComposition());
    max_comp = llceil(mPatchp->getMaxComposition());
    range = (max_comp - min_comp);
    range++;
    if (range > 3)
    {
        if ((mPatchp->getMinComposition() - min_comp) > (max_comp - mPatchp->getMaxComposition()))
        {
            // The top side runs over more
            mBaseComp++;
        }
        range = 3;
    }

    LLFacePool *poolp = getPool();

    mDrawable->addFace(poolp, NULL);

    return mDrawable;
}


void LLVOSurfacePatch::updateGL()
{
    if (mPatchp)
    {
        LL_PROFILE_ZONE_SCOPED;
        mPatchp->updateGL();
    }
}

bool LLVOSurfacePatch::updateGeometry(LLDrawable *drawable)
{
    LL_PROFILE_ZONE_SCOPED;

    dirtySpatialGroup();

    S32 min_comp, max_comp, range;
    min_comp = lltrunc(mPatchp->getMinComposition());
    max_comp = lltrunc(ceil(mPatchp->getMaxComposition()));
    range = (max_comp - min_comp);
    range++;
    S32 new_base_comp = lltrunc(mPatchp->getMinComposition());
    if (range > 3)
    {
        if ((mPatchp->getMinComposition() - min_comp) > (max_comp - mPatchp->getMaxComposition()))
        {
            // The top side runs over more
            new_base_comp++;
        }
        range = 3;
    }

    // Pick the two closest detail textures for this patch...
    // Then create the draw pool for it.
    // Actually, should get the average composition instead of the center.
    mBaseComp = new_base_comp;

    //////////////////////////
    //
    // Figure out the strides
    //
    //

    U32 patch_width, render_stride, north_stride, east_stride, length;
    render_stride = mPatchp->getRenderStride();
    patch_width = mPatchp->getSurface()->getGridsPerPatchEdge();

    length = patch_width / render_stride;

    if (mPatchp->getNeighborPatch(NORTH))
    {
        north_stride = mPatchp->getNeighborPatch(NORTH)->getRenderStride();
    }
    else
    {
        north_stride = render_stride;
    }

    if (mPatchp->getNeighborPatch(EAST))
    {
        east_stride = mPatchp->getNeighborPatch(EAST)->getRenderStride();
    }
    else
    {
        east_stride = render_stride;
    }

    mLastLength = length;
    mLastStride = render_stride;
    mLastNorthStride = north_stride;
    mLastEastStride = east_stride;

    return true;
}

void LLVOSurfacePatch::updateFaceSize(S32 idx)
{
    LL_PROFILE_ZONE_SCOPED;
    if (idx != 0)
    {
        LL_WARNS() << "Terrain partition requested invalid face!!!" << LL_ENDL;
        return;
    }

    LLFace* facep = mDrawable->getFace(idx);
    if (facep)
    {
        S32 num_vertices = 0;
        S32 num_indices = 0;

        if (mLastStride)
        {
            getGeomSizesMain(mLastStride, num_vertices, num_indices);
            getGeomSizesNorth(mLastStride, mLastNorthStride, num_vertices, num_indices);
            getGeomSizesEast(mLastStride, mLastEastStride, num_vertices, num_indices);
        }

        facep->setSize(num_vertices, num_indices);
    }
}

bool LLVOSurfacePatch::updateLOD()
{
    return true;
}

void LLVOSurfacePatch::getTerrainGeometry(LLStrider<LLVector3> &verticesp,
                                              LLStrider<LLVector3> &normalsp,
                                              LLStrider<LLVector2> &texCoords0p,
                                              LLStrider<LLVector2> &texCoords1p,
                                              LLStrider<U16> &indicesp)
{
    LLFace* facep = mDrawable->getFace(0);
    if (!facep)
    {
        return;
    }

    U32 index_offset = facep->getGeomIndex();

    updateMainGeometry(facep,
                    verticesp,
                    normalsp,
                    texCoords0p,
                    texCoords1p,
                    indicesp,
                    index_offset);
    updateNorthGeometry(facep,
                        verticesp,
                        normalsp,
                        texCoords0p,
                        texCoords1p,
                        indicesp,
                        index_offset);
    updateEastGeometry(facep,
                        verticesp,
                        normalsp,
                        texCoords0p,
                        texCoords1p,
                        indicesp,
                        index_offset);
}

void LLVOSurfacePatch::updateMainGeometry(LLFace *facep,
                                        LLStrider<LLVector3> &verticesp,
                                        LLStrider<LLVector3> &normalsp,
                                        LLStrider<LLVector2> &texCoords0p,
                                        LLStrider<LLVector2> &texCoords1p,
                                        LLStrider<U16> &indicesp,
                                        U32 &index_offset)
{
    S32 i, j, x, y;

    U32 patch_size, render_stride;
    S32 num_vertices, num_indices;
    U32 index;

    llassert(mLastStride > 0);

    render_stride = mLastStride;
    patch_size = mPatchp->getSurface()->getGridsPerPatchEdge();
    S32 vert_size = patch_size / render_stride;

    ///////////////////////////
    //
    // Render the main patch
    //
    //

    num_vertices = 0;
    num_indices = 0;
    // First, figure out how many vertices we need...
    getGeomSizesMain(render_stride, num_vertices, num_indices);

    if (num_vertices > 0)
    {
        facep->mCenterAgent = mPatchp->getPointAgent(8, 8);

        // Generate patch points first
        for (j = 0; j < vert_size; j++)
        {
            for (i = 0; i < vert_size; i++)
            {
                x = i * render_stride;
                y = j * render_stride;
                mPatchp->eval(x, y, render_stride, verticesp.get(), normalsp.get(), texCoords0p.get(), texCoords1p.get());
                verticesp++;
                normalsp++;
                texCoords0p++;
                texCoords1p++;
            }
        }

        for (j = 0; j < (vert_size - 1); j++)
        {
            if (j % 2)
            {
                for (i = (vert_size - 1); i > 0; i--)
                {
                    index = (i - 1)+ j*vert_size;
                    *(indicesp++) = index_offset + index;

                    index = i + (j+1)*vert_size;
                    *(indicesp++) = index_offset + index;

                    index = (i - 1) + (j+1)*vert_size;
                    *(indicesp++) = index_offset + index;

                    index = (i - 1) + j*vert_size;
                    *(indicesp++) = index_offset + index;

                    index = i + j*vert_size;
                    *(indicesp++) = index_offset + index;

                    index = i + (j+1)*vert_size;
                    *(indicesp++) = index_offset + index;
                }
            }
            else
            {
                for (i = 0; i < (vert_size - 1); i++)
                {
                    index = i + j*vert_size;
                    *(indicesp++) = index_offset + index;

                    index = (i + 1) + (j+1)*vert_size;
                    *(indicesp++) = index_offset + index;

                    index = i + (j+1)*vert_size;
                    *(indicesp++) = index_offset + index;

                    index = i + j*vert_size;
                    *(indicesp++) = index_offset + index;

                    index = (i + 1) + j*vert_size;
                    *(indicesp++) = index_offset + index;

                    index = (i + 1) + (j + 1)*vert_size;
                    *(indicesp++) = index_offset + index;
                }
            }
        }
    }
    index_offset += num_vertices;
}


void LLVOSurfacePatch::updateNorthGeometry(LLFace *facep,
                                        LLStrider<LLVector3> &verticesp,
                                        LLStrider<LLVector3> &normalsp,
                                        LLStrider<LLVector2> &texCoords0p,
                                        LLStrider<LLVector2> &texCoords1p,
                                        LLStrider<U16> &indicesp,
                                        U32 &index_offset)
{
    S32 i, x, y;

    S32 num_vertices;

    U32 render_stride = mLastStride;
    S32 patch_size = mPatchp->getSurface()->getGridsPerPatchEdge();
    S32 length = patch_size / render_stride;
    S32 half_length = length / 2;
    U32 north_stride = mLastNorthStride;

    ///////////////////////////
    //
    // Render the north strip
    //
    //

    // Stride lengths are the same
    if (north_stride == render_stride)
    {
        num_vertices = 2 * length + 1;

        facep->mCenterAgent = (mPatchp->getPointAgent(8, 15) + mPatchp->getPointAgent(8, 16))*0.5f;

        // Main patch
        for (i = 0; i < length; i++)
        {
            x = i * render_stride;
            y = 16 - render_stride;

            mPatchp->eval(x, y, render_stride, verticesp.get(), normalsp.get(), texCoords0p.get(), texCoords1p.get());
            verticesp++;
            normalsp++;
            texCoords0p++;
            texCoords1p++;
        }

        // North patch
        for (i = 0; i <= length; i++)
        {
            x = i * render_stride;
            y = 16;
            mPatchp->eval(x, y, render_stride, verticesp.get(), normalsp.get(), texCoords0p.get(), texCoords1p.get());
            verticesp++;
            normalsp++;
            texCoords0p++;
            texCoords1p++;
        }


        for (i = 0; i < length; i++)
        {
            // Generate indices
            *(indicesp++) = index_offset + i;
            *(indicesp++) = index_offset + length + i + 1;
            *(indicesp++) = index_offset + length + i;

            if (i != length - 1)
            {
                *(indicesp++) = index_offset + i;
                *(indicesp++) = index_offset + i + 1;
                *(indicesp++) = index_offset + length + i + 1;
            }
        }
    }
    else if (north_stride > render_stride)
    {
        // North stride is longer (has less vertices)
        num_vertices = length + length/2 + 1;

        facep->mCenterAgent = (mPatchp->getPointAgent(7, 15) + mPatchp->getPointAgent(8, 16))*0.5f;

        // Iterate through this patch's points
        for (i = 0; i < length; i++)
        {
            x = i * render_stride;
            y = 16 - render_stride;

            mPatchp->eval(x, y, render_stride, verticesp.get(), normalsp.get(), texCoords0p.get(), texCoords1p.get());
            verticesp++;
            normalsp++;
            texCoords0p++;
            texCoords1p++;
        }

        // Iterate through the north patch's points
        for (i = 0; i <= length; i+=2)
        {
            x = i * render_stride;
            y = 16;

            mPatchp->eval(x, y, render_stride, verticesp.get(), normalsp.get(), texCoords0p.get(), texCoords1p.get());
            verticesp++;
            normalsp++;
            texCoords0p++;
            texCoords1p++;
        }


        for (i = 0; i < length; i++)
        {
            if (!(i % 2))
            {
                *(indicesp++) = index_offset + i;
                *(indicesp++) = index_offset + i + 1;
                *(indicesp++) = index_offset + length + (i/2);

                *(indicesp++) = index_offset + i + 1;
                *(indicesp++) = index_offset + length + (i/2) + 1;
                *(indicesp++) = index_offset + length + (i/2);
            }
            else if (i < (length - 1))
            {
                *(indicesp++) = index_offset + i;
                *(indicesp++) = index_offset + i + 1;
                *(indicesp++) = index_offset + length + (i/2) + 1;
            }
        }
    }
    else
    {
        // North stride is shorter (more vertices)
        length = patch_size / north_stride;
        half_length = length / 2;
        num_vertices = length + half_length + 1;

        facep->mCenterAgent = (mPatchp->getPointAgent(15, 7) + mPatchp->getPointAgent(16, 8))*0.5f;

        // Iterate through this patch's points
        for (i = 0; i < length; i+=2)
        {
            x = i * north_stride;
            y = 16 - render_stride;

            mPatchp->eval(x, y, render_stride, verticesp.get(), normalsp.get(), texCoords0p.get(), texCoords1p.get());
            verticesp++;
            normalsp++;
            texCoords0p++;
            texCoords1p++;
        }

        // Iterate through the north patch's points
        for (i = 0; i <= length; i++)
        {
            x = i * north_stride;
            y = 16;

            mPatchp->eval(x, y, render_stride, verticesp.get(), normalsp.get(), texCoords0p.get(), texCoords1p.get());
            verticesp++;
            normalsp++;
            texCoords0p++;
            texCoords1p++;
        }

        for (i = 0; i < length; i++)
        {
            if (!(i%2))
            {
                *(indicesp++) = index_offset + half_length + i;
                *(indicesp++) = index_offset + i/2;
                *(indicesp++) = index_offset + half_length + i + 1;
            }
            else if (i < (length - 2))
            {
                *(indicesp++) = index_offset + half_length + i;
                *(indicesp++) = index_offset + i/2;
                *(indicesp++) = index_offset + i/2 + 1;

                *(indicesp++) = index_offset + half_length + i;
                *(indicesp++) = index_offset + i/2 + 1;
                *(indicesp++) = index_offset + half_length + i + 1;
            }
            else
            {
                *(indicesp++) = index_offset + half_length + i;
                *(indicesp++) = index_offset + i/2;
                *(indicesp++) = index_offset + half_length + i + 1;
            }
        }
    }
    index_offset += num_vertices;
}

void LLVOSurfacePatch::updateEastGeometry(LLFace *facep,
                                          LLStrider<LLVector3> &verticesp,
                                          LLStrider<LLVector3> &normalsp,
                                          LLStrider<LLVector2> &texCoords0p,
                                          LLStrider<LLVector2> &texCoords1p,
                                          LLStrider<U16> &indicesp,
                                          U32 &index_offset)
{
    S32 i, x, y;

    S32 num_vertices;

    U32 render_stride = mLastStride;
    S32 patch_size = mPatchp->getSurface()->getGridsPerPatchEdge();
    S32 length = patch_size / render_stride;
    S32 half_length = length / 2;

    U32 east_stride = mLastEastStride;

    // Stride lengths are the same
    if (east_stride == render_stride)
    {
        num_vertices = 2 * length + 1;

        facep->mCenterAgent = (mPatchp->getPointAgent(8, 15) + mPatchp->getPointAgent(8, 16))*0.5f;

        // Main patch
        for (i = 0; i < length; i++)
        {
            x = 16 - render_stride;
            y = i * render_stride;

            mPatchp->eval(x, y, render_stride, verticesp.get(), normalsp.get(), texCoords0p.get(), texCoords1p.get());
            verticesp++;
            normalsp++;
            texCoords0p++;
            texCoords1p++;
        }

        // East patch
        for (i = 0; i <= length; i++)
        {
            x = 16;
            y = i * render_stride;
            mPatchp->eval(x, y, render_stride, verticesp.get(), normalsp.get(), texCoords0p.get(), texCoords1p.get());
            verticesp++;
            normalsp++;
            texCoords0p++;
            texCoords1p++;
        }


        for (i = 0; i < length; i++)
        {
            // Generate indices
            *(indicesp++) = index_offset + i;
            *(indicesp++) = index_offset + length + i;
            *(indicesp++) = index_offset + length + i + 1;

            if (i != length - 1)
            {
                *(indicesp++) = index_offset + i;
                *(indicesp++) = index_offset + length + i + 1;
                *(indicesp++) = index_offset + i + 1;
            }
        }
    }
    else if (east_stride > render_stride)
    {
        // East stride is longer (has less vertices)
        num_vertices = length + half_length + 1;

        facep->mCenterAgent = (mPatchp->getPointAgent(7, 15) + mPatchp->getPointAgent(8, 16))*0.5f;

        // Iterate through this patch's points
        for (i = 0; i < length; i++)
        {
            x = 16 - render_stride;
            y = i * render_stride;

            mPatchp->eval(x, y, render_stride, verticesp.get(), normalsp.get(), texCoords0p.get(), texCoords1p.get());
            verticesp++;
            normalsp++;
            texCoords0p++;
            texCoords1p++;
        }
        // Iterate through the east patch's points
        for (i = 0; i <= length; i+=2)
        {
            x = 16;
            y = i * render_stride;

            mPatchp->eval(x, y, render_stride, verticesp.get(), normalsp.get(), texCoords0p.get(), texCoords1p.get());
            verticesp++;
            normalsp++;
            texCoords0p++;
            texCoords1p++;
        }

        for (i = 0; i < length; i++)
        {
            if (!(i % 2))
            {
                *(indicesp++) = index_offset + i;
                *(indicesp++) = index_offset + length + (i/2);
                *(indicesp++) = index_offset + i + 1;

                *(indicesp++) = index_offset + i + 1;
                *(indicesp++) = index_offset + length + (i/2);
                *(indicesp++) = index_offset + length + (i/2) + 1;
            }
            else if (i < (length - 1))
            {
                *(indicesp++) = index_offset + i;
                *(indicesp++) = index_offset + length + (i/2) + 1;
                *(indicesp++) = index_offset + i + 1;
            }
        }
    }
    else
    {
        // East stride is shorter (more vertices)
        length = patch_size / east_stride;
        half_length = length / 2;
        num_vertices = length + length/2 + 1;

        facep->mCenterAgent = (mPatchp->getPointAgent(15, 7) + mPatchp->getPointAgent(16, 8))*0.5f;

        // Iterate through this patch's points
        for (i = 0; i < length; i+=2)
        {
            x = 16 - render_stride;
            y = i * east_stride;

            mPatchp->eval(x, y, render_stride, verticesp.get(), normalsp.get(), texCoords0p.get(), texCoords1p.get());
            verticesp++;
            normalsp++;
            texCoords0p++;
            texCoords1p++;
        }
        // Iterate through the east patch's points
        for (i = 0; i <= length; i++)
        {
            x = 16;
            y = i * east_stride;

            mPatchp->eval(x, y, render_stride, verticesp.get(), normalsp.get(), texCoords0p.get(), texCoords1p.get());
            verticesp++;
            normalsp++;
            texCoords0p++;
            texCoords1p++;
        }

        for (i = 0; i < length; i++)
        {
            if (!(i%2))
            {
                *(indicesp++) = index_offset + half_length + i;
                *(indicesp++) = index_offset + half_length + i + 1;
                *(indicesp++) = index_offset + i/2;
            }
            else if (i < (length - 2))
            {
                *(indicesp++) = index_offset + half_length + i;
                *(indicesp++) = index_offset + i/2 + 1;
                *(indicesp++) = index_offset + i/2;

                *(indicesp++) = index_offset + half_length + i;
                *(indicesp++) = index_offset + half_length + i + 1;
                *(indicesp++) = index_offset + i/2 + 1;
            }
            else
            {
                *(indicesp++) = index_offset + half_length + i;
                *(indicesp++) = index_offset + half_length + i + 1;
                *(indicesp++) = index_offset + i/2;
            }
        }
    }
    index_offset += num_vertices;
}

void LLVOSurfacePatch::setPatch(LLSurfacePatch *patchp)
{
    mPatchp = patchp;

    dirtyPatch();
};


void LLVOSurfacePatch::dirtyPatch()
{
    mDirtiedPatch = true;
    dirtyGeom();
    mDirtyTerrain = true;
    LLVector3 center = mPatchp->getCenterRegion();
    LLSurface *surfacep = mPatchp->getSurface();

    setPositionRegion(center);

    F32 scale_factor = surfacep->getGridsPerPatchEdge() * surfacep->getMetersPerGrid();
    setScale(LLVector3(scale_factor, scale_factor, mPatchp->getMaxZ() - mPatchp->getMinZ()));
}

void LLVOSurfacePatch::dirtyGeom()
{
    if (mDrawable)
    {
        gPipeline.markRebuild(mDrawable, LLDrawable::REBUILD_ALL);
        LLFace* facep = mDrawable->getFace(0);
        if (facep)
        {
            facep->setVertexBuffer(NULL);
        }
        mDrawable->movePartition();
    }
}

void LLVOSurfacePatch::getGeomSizesMain(const S32 stride, S32 &num_vertices, S32 &num_indices)
{
    S32 patch_size = mPatchp->getSurface()->getGridsPerPatchEdge();

    // First, figure out how many vertices we need...
    S32 vert_size = patch_size / stride;
    if (vert_size >= 2)
    {
        num_vertices += vert_size * vert_size;
        num_indices += 6 * (vert_size - 1)*(vert_size - 1);
    }
}

void LLVOSurfacePatch::getGeomSizesNorth(const S32 stride, const S32 north_stride,
                                         S32 &num_vertices, S32 &num_indices)
{
    S32 patch_size = mPatchp->getSurface()->getGridsPerPatchEdge();
    S32 length = patch_size / stride;
    // Stride lengths are the same
    if (north_stride == stride)
    {
        num_vertices += 2 * length + 1;
        num_indices += length * 6 - 3;
    }
    else if (north_stride > stride)
    {
        // North stride is longer (has less vertices)
        num_vertices += length + (length/2) + 1;
        num_indices += (length/2)*9 - 3;
    }
    else
    {
        // North stride is shorter (more vertices)
        length = patch_size / north_stride;
        num_vertices += length + (length/2) + 1;
        num_indices += 9*(length/2) - 3;
    }
}

void LLVOSurfacePatch::getGeomSizesEast(const S32 stride, const S32 east_stride,
                                        S32 &num_vertices, S32 &num_indices)
{
    S32 patch_size = mPatchp->getSurface()->getGridsPerPatchEdge();
    S32 length = patch_size / stride;
    // Stride lengths are the same
    if (east_stride == stride)
    {
        num_vertices += 2 * length + 1;
        num_indices += length * 6 - 3;
    }
    else if (east_stride > stride)
    {
        // East stride is longer (has less vertices)
        num_vertices += length + (length/2) + 1;
        num_indices += (length/2)*9 - 3;
    }
    else
    {
        // East stride is shorter (more vertices)
        length = patch_size / east_stride;
        num_vertices += length + (length/2) + 1;
        num_indices += 9*(length/2) - 3;
    }
}

bool LLVOSurfacePatch::lineSegmentIntersect(const LLVector4a& start, const LLVector4a& end, S32 face, bool pick_transparent, bool pick_rigged, bool pick_unselectable, S32 *face_hitp,
                                      LLVector4a* intersection,LLVector2* tex_coord, LLVector4a* normal, LLVector4a* tangent)

{

    if (!lineSegmentBoundingBox(start, end))
    {
        return false;
    }

    LLVector4a da;
    da.setSub(end, start);
    LLVector3 delta(da.getF32ptr());

    LLVector3 pdelta = delta;
    pdelta.mV[2] = 0;

    F32 plength = pdelta.length();

    F32 tdelta = 1.f/plength;

    LLVector3 v_start(start.getF32ptr());

    LLVector3 origin = v_start - mRegionp->getOriginAgent();

    if (mRegionp->getLandHeightRegion(origin) > origin.mV[2])
    {
        //origin is under ground, treat as no intersection
        return false;
    }

    //step one meter at a time until intersection point found

    //VECTORIZE THIS
    const LLVector4a* exta = mDrawable->getSpatialExtents();

    LLVector3 ext[2];
    ext[0].set(exta[0].getF32ptr());
    ext[1].set(exta[1].getF32ptr());

    F32 rad = (delta*tdelta).magVecSquared();

    F32 t = 0.f;
    while ( t <= 1.f)
    {
        LLVector3 sample = origin + delta*t;

        if (AABBSphereIntersectR2(ext[0], ext[1], sample+mRegionp->getOriginAgent(), rad))
        {
            F32 height = mRegionp->getLandHeightRegion(sample);
            if (height > sample.mV[2])
            { //ray went below ground, positive intersection
                //quick and dirty binary search to get impact point
                tdelta = -tdelta*0.5f;
                F32 err_dist = 0.001f;
                F32 dist = fabsf(sample.mV[2] - height);

                while (dist > err_dist && tdelta*tdelta > 0.0f)
                {
                    t += tdelta;
                    sample = origin+delta*t;
                    height = mRegionp->getLandHeightRegion(sample);
                    if ((tdelta < 0 && height < sample.mV[2]) ||
                        (height > sample.mV[2] && tdelta > 0))
                    { //jumped over intersection point, go back
                        tdelta = -tdelta;
                    }
                    tdelta *= 0.5f;
                    dist = fabsf(sample.mV[2] - height);
                }

                if (intersection)
                {
                    F32 height = mRegionp->getLandHeightRegion(sample);
                    if (fabsf(sample.mV[2]-height) < delta.length()*tdelta)
                    {
                        sample.mV[2] = mRegionp->getLandHeightRegion(sample);
                    }
                    intersection->load3((sample + mRegionp->getOriginAgent()).mV);
                }

                if (normal)
                {
                    normal->load3((mRegionp->getLand().resolveNormalGlobal(mRegionp->getPosGlobalFromRegion(sample))).mV);
                }

                return true;
            }
        }

        t += tdelta;
        if (t > 1 && t < 1.f+tdelta*0.99f)
        { //make sure end point is checked (saves vertical lines coming up negative)
            t = 1.f;
        }
    }


    return false;
}

void LLVOSurfacePatch::updateSpatialExtents(LLVector4a& newMin, LLVector4a &newMax)
{
    LLVector3 posAgent = getPositionAgent();
    LLVector3 scale = getScale();
    //make z-axis scale at least 1 to avoid shadow artifacts on totally flat land
    scale.mV[VZ] = llmax(scale.mV[VZ], 1.f);
    newMin.load3( (posAgent-scale*0.5f).mV); // Changing to 2.f makes the culling a -little- better, but still wrong
    newMax.load3( (posAgent+scale*0.5f).mV);
    LLVector4a pos;
    pos.setAdd(newMin,newMax);
    pos.mul(0.5f);
    mDrawable->setPositionGroup(pos);
}

U32 LLVOSurfacePatch::getPartitionType() const
{
    return LLViewerRegion::PARTITION_TERRAIN;
}

LLTerrainPartition::LLTerrainPartition(LLViewerRegion* regionp)
: LLSpatialPartition(LLDrawPoolTerrain::VERTEX_DATA_MASK, false, regionp)
{
    mOcclusionEnabled = false;
    mInfiniteFarClip = true;
    mDrawableType = LLPipeline::RENDER_TYPE_TERRAIN;
    mPartitionType = LLViewerRegion::PARTITION_TERRAIN;
}

// Do not add vertices; honor strict vertex count specified by strider_vertex_count
void gen_terrain_tangents(U32                    strider_vertex_count,
                          U32                    strider_index_count,
                          LLStrider<LLVector3>  &verticesp,
                          LLStrider<LLVector3>  &normalsp,
                          LLStrider<LLVector4a> &tangentsp,
                          LLStrider<U16>        &indicesp,
                          F32 region_width)
{
    LL_PROFILE_ZONE_SCOPED;

    LLVector4a            *vertices = new LLVector4a[strider_vertex_count];
    LLVector4a            *normals  = new LLVector4a[strider_vertex_count];
    LLVector4a            *tangents = new LLVector4a[strider_vertex_count];
    std::vector<LLVector2> texcoords(strider_vertex_count);
    std::vector<U16>       indices(strider_index_count);

    for (U32 v = 0; v < strider_vertex_count; ++v)
    {
        F32 *vert    = verticesp[v].mV;
        vertices[v]  = LLVector4a(vert[0], vert[1], vert[2], 1.f);
        F32 *n       = normalsp[v].mV;
        normals[v]   = LLVector4a(n[0], n[1], n[2], 1.f);
        tangents[v]  = tangentsp[v];

        // Calculate texcoords on-the-fly using the terrain positions
        texcoords[v].mV[VX] = verticesp[v].mV[VX] / region_width;
        texcoords[v].mV[VY] = verticesp[v].mV[VY] / region_width;
    }
    for (U32 i = 0; i < strider_index_count; ++i)
    {
        indices[i] = indicesp[i];
    }

    LLCalculateTangentArray(strider_vertex_count, vertices, normals, texcoords.data(), strider_index_count / 3, indices.data(), tangents);

    for (U32 v = 0; v < strider_vertex_count; ++v)
    {
        tangentsp[v] = tangents[v];
    }

    delete[] vertices;
    delete[] normals;
    delete[] tangents;
}

void LLTerrainPartition::getGeometry(LLSpatialGroup* group)
{
    LL_PROFILE_ZONE_SCOPED;

    LLVertexBuffer* buffer = group->mVertexBuffer;

    //get vertex buffer striders
    LLStrider<LLVector3> vertices_start;
    LLStrider<LLVector3> normals_start;
    LLStrider<LLVector4a> tangents_start;
    LLStrider<LLVector2> texcoords0_start; // ownership overlay
    LLStrider<LLVector2> texcoords2_start;
    LLStrider<U16> indices_start;

    llassert_always(buffer->getVertexStrider(vertices_start));
    llassert_always(buffer->getNormalStrider(normals_start));
    llassert_always(buffer->getTangentStrider(tangents_start));
    llassert_always(buffer->getTexCoord0Strider(texcoords0_start));
    llassert_always(buffer->getTexCoord1Strider(texcoords2_start));
    llassert_always(buffer->getIndexStrider(indices_start));

    U32 indices_index = 0;
    U32 index_offset = 0;

    {
        LLStrider<LLVector3> vertices = vertices_start;
        LLStrider<LLVector3> normals = normals_start;
        LLStrider<LLVector2> texcoords0 = texcoords0_start;
        LLStrider<LLVector2> texcoords2 = texcoords2_start;
        LLStrider<U16> indices = indices_start;

        for (std::vector<LLFace*>::iterator i = mFaceList.begin(); i != mFaceList.end(); ++i)
        {
            LLFace* facep = *i;

            facep->setIndicesIndex(indices_index);
            facep->setGeomIndex(index_offset);
            facep->setVertexBuffer(buffer);

            LLVOSurfacePatch* patchp = (LLVOSurfacePatch*) facep->getViewerObject();
            patchp->getTerrainGeometry(vertices, normals, texcoords0, texcoords2, indices);

            indices_index += facep->getIndicesCount();
            index_offset += facep->getGeomCount();
        }
    }

    const bool has_tangents = tangents_start.get() != nullptr;
    if (has_tangents && index_offset > 0) // <FS:Beq/> FIRE-34672 OPENSIM bugsplat crash
    {
        LLStrider<LLVector3> vertices = vertices_start;
        LLStrider<LLVector3> normals = normals_start;
        LLStrider<LLVector4a> tangents = tangents_start;
        LLStrider<U16> indices = indices_start;

        F32 region_width = 256.0f;
        if (mFaceList.empty())
        {
            llassert(false);
        }
        else
        {
            const LLViewerRegion* regionp = mFaceList[0]->getViewerObject()->getRegion();
            llassert(regionp == mFaceList.back()->getViewerObject()->getRegion()); // Assume this spatial group is confined to one region
            region_width = regionp->getWidth();
        }
        gen_terrain_tangents(index_offset, indices_index, vertices, normals, tangents, indices, region_width);
    }

    mFaceList.clear();
}

