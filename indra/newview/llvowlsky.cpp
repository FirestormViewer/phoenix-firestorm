/**
 * @file llvowlsky.cpp
 * @brief LLVOWLSky class implementation
 *
 * $LicenseInfo:firstyear=2007&license=viewerlgpl$
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

#include "pipeline.h"

#include "llvowlsky.h"
#include "llsky.h"
#include "lldrawpoolwlsky.h"
#include "llface.h"
#include "llviewercontrol.h"
#include "llenvironment.h"
#include "llsettingssky.h"
// <AP:WW> For star sizing: camera FOV and window dimensions.
#include "llcamera.h"
#include "llviewerwindow.h"
// </AP:WW>

constexpr U32 MIN_SKY_DETAIL = 8;
constexpr U32 MAX_SKY_DETAIL = 180;

// <AP:WW> Procedural starfield: brighter primary stars plus a much larger,
// fainter "dust" population for density.
constexpr U32 NUM_PRIMARY_STARS = 120000; // Brighter procedural stars
constexpr U32 NUM_DUST_STARS    = 500000; // Fainter procedural stars for density
constexpr U32 TOTAL_STARS       = (NUM_PRIMARY_STARS + NUM_DUST_STARS);
// </AP:WW>

inline U32 LLVOWLSky::getNumStacks(void)
{
    return llmin(MAX_SKY_DETAIL, llmax(MIN_SKY_DETAIL, gSavedSettings.getU32("WLSkyDetail")));
}

inline U32 LLVOWLSky::getNumSlices(void)
{
    return 2 * llmin(MAX_SKY_DETAIL, llmax(MIN_SKY_DETAIL, gSavedSettings.getU32("WLSkyDetail")));
}

inline U32 LLVOWLSky::getStripsNumVerts(void)
{
    return (getNumStacks() - 1) * getNumSlices();
}

inline U32 LLVOWLSky::getStripsNumIndices(void)
{
    return 2 * ((getNumStacks() - 2) * (getNumSlices() + 1)) + 1 ;
}

inline U32 LLVOWLSky::getStarsNumVerts(void)
{
    // <AP:WW> Combined primary + dust procedural star count.
    return TOTAL_STARS;
}

inline U32 LLVOWLSky::getStarsNumIndices(void)
{
    return getStarsNumVerts() * 1000; // Theoretical max if indexed is 6 yet in original code it is set to 1000
}

LLVOWLSky::LLVOWLSky(const LLUUID &id, const LLPCode pcode, LLViewerRegion *regionp)
    : LLStaticViewerObject(id, pcode, regionp, true)
{
    initStars();
}

void LLVOWLSky::idleUpdate(LLAgent &agent, const F64 &time)
{

}

bool LLVOWLSky::isActive(void) const
{
    return false;
}

LLDrawable * LLVOWLSky::createDrawable(LLPipeline * pipeline)
{
    pipeline->allocDrawable(this);

    //LLDrawPoolWLSky *poolp = static_cast<LLDrawPoolWLSky *>(
        gPipeline.getPool(LLDrawPool::POOL_WL_SKY);

    mDrawable->setRenderType(LLPipeline::RENDER_TYPE_WL_SKY);

    return mDrawable;
}

// a tiny helper function for controlling the sky dome tesselation.
inline F32 calcPhi(const U32 &i, const F32 &reciprocal_num_stacks)
{
    // Calc: PI/8 * 1-((1-t^4)*(1-t^4))  { 0<t<1 }
    // Demos: \pi/8*\left(1-((1-x^{4})*(1-x^{4}))\right)\ \left\{0<x\le1\right\}

    // i should range from [0..SKY_STACKS] so t will range from [0.f .. 1.f]
    F32 t = float(i) * reciprocal_num_stacks; //SL-16127: remove: / float(getNumStacks());

    // ^4 the parameter of the tesselation to bias things toward 0 (the dome's apex)
    t *= t;
    t *= t;

    // invert and square the parameter of the tesselation to bias things toward 1 (the horizon)
    t = 1.f - t;
    t = t*t;
    t = 1.f - t;

    return (F_PI / 8.f) * t;
}

void LLVOWLSky::resetVertexBuffers()
{
    mStripsVerts.clear();
    mStarsVerts = nullptr;
    mFsSkyVerts = nullptr;

    gPipeline.markRebuild(mDrawable, LLDrawable::REBUILD_ALL);
}

void LLVOWLSky::cleanupGL()
{
    mStripsVerts.clear();
    mStarsVerts = nullptr;
    mFsSkyVerts = nullptr;

    LLDrawPoolWLSky::cleanupGL();
}

void LLVOWLSky::restoreGL()
{
    LLDrawPoolWLSky::restoreGL();
    gPipeline.markRebuild(mDrawable, LLDrawable::REBUILD_ALL);
}

bool LLVOWLSky::updateGeometry(LLDrawable * drawable)
{
    LL_PROFILE_ZONE_SCOPED;
    LLStrider<LLVector3>    vertices;
    LLStrider<LLVector2>    texCoords;
    LLStrider<U16>          indices;

    if (mFsSkyVerts.isNull())
    {
        mFsSkyVerts = new LLVertexBuffer(LLDrawPoolWLSky::ADV_ATMO_SKY_VERTEX_DATA_MASK);

        if (!mFsSkyVerts->allocateBuffer(4, 6))
        {
            LL_WARNS() << "Failed to allocate Vertex Buffer on full screen sky update" << LL_ENDL;
        }

        bool success = mFsSkyVerts->getVertexStrider(vertices)
                    && mFsSkyVerts->getTexCoord0Strider(texCoords)
                    && mFsSkyVerts->getIndexStrider(indices);

        if(!success)
        {
            LL_ERRS() << "Failed updating WindLight fullscreen sky geometry." << LL_ENDL;
        }

        *vertices++ = LLVector3(-1.0f, -1.0f, 0.0f);
        *vertices++ = LLVector3( 1.0f, -1.0f, 0.0f);
        *vertices++ = LLVector3(-1.0f,  1.0f, 0.0f);
        *vertices++ = LLVector3( 1.0f,  1.0f, 0.0f);

        *texCoords++ = LLVector2(0.0f, 0.0f);
        *texCoords++ = LLVector2(1.0f, 0.0f);
        *texCoords++ = LLVector2(0.0f, 1.0f);
        *texCoords++ = LLVector2(1.0f, 1.0f);

        *indices++ = 0;
        *indices++ = 1;
        *indices++ = 2;
        *indices++ = 1;
        *indices++ = 3;
        *indices++ = 2;

        mFsSkyVerts->unmapBuffer();
    }

    {
        const F32 dome_radius = LLEnvironment::instance().getCurrentSky()->getDomeRadius();
        LLCachedControl<S32> max_vbo_size(gSavedSettings, "RenderMaxVBOSize", 512);
        const U32 max_buffer_bytes = max_vbo_size * 1024;
        const U32 data_mask = LLDrawPoolWLSky::SKY_VERTEX_DATA_MASK;
        const U32 max_verts = max_buffer_bytes / LLVertexBuffer::calcVertexSize(data_mask);

        const U32 total_stacks = getNumStacks();

        const U32 verts_per_stack = getNumSlices();

        // each seg has to have one more row of verts than it has stacks
        // then round down
        const U32 stacks_per_seg = (max_verts - verts_per_stack) / verts_per_stack;

        // round up to a whole number of segments
        const U32 strips_segments = (total_stacks+stacks_per_seg-1) / stacks_per_seg;

        mStripsVerts.resize(strips_segments, NULL);

#if RELEASE_SHOW_DEBUG
        LL_INFOS() << "WL Skydome strips in " << strips_segments << " batches." << LL_ENDL;

        LLTimer timer;
        timer.start();
#endif

        for (U32 i = 0; i < strips_segments ;++i)
        {
            LLVertexBuffer * segment = new LLVertexBuffer(LLDrawPoolWLSky::SKY_VERTEX_DATA_MASK);
            mStripsVerts[i] = segment;

            U32 num_stacks_this_seg = stacks_per_seg;
            if ((i == strips_segments - 1) && (total_stacks % stacks_per_seg) != 0)
            {
                // for the last buffer only allocate what we'll use
                num_stacks_this_seg = total_stacks % stacks_per_seg;
            }

            // figure out what range of the sky we're filling
            const U32 begin_stack = i * stacks_per_seg;
            const U32 end_stack = begin_stack + num_stacks_this_seg;
            llassert(end_stack <= total_stacks);

            const U32 num_verts_this_seg = verts_per_stack * (num_stacks_this_seg+1);
            llassert(num_verts_this_seg <= max_verts);

            const U32 num_indices_this_seg = 1+num_stacks_this_seg*(2+2*verts_per_stack);
            llassert(num_indices_this_seg * sizeof(U16) <= max_buffer_bytes);

            bool allocated = segment->allocateBuffer(num_verts_this_seg, num_indices_this_seg);
#if RELEASE_SHOW_WARNS
            if( !allocated )
            {
                LL_WARNS() << "Failed to allocate Vertex Buffer on update to "
                    << num_verts_this_seg << " vertices and "
                    << num_indices_this_seg << " indices" << LL_ENDL;
            }
#else
            (void) allocated;
#endif

            // lock the buffer
            bool success = segment->getVertexStrider(vertices)
                && segment->getTexCoord0Strider(texCoords)
                && segment->getIndexStrider(indices);

#if RELEASE_SHOW_DEBUG
            if(!success)
            {
                LL_ERRS() << "Failed updating WindLight sky geometry." << LL_ENDL;
            }
#else
            (void) success;
#endif

            // fill it
            buildStripsBuffer(begin_stack, end_stack, vertices, texCoords, indices, dome_radius, verts_per_stack, total_stacks);

            // and unlock the buffer
            segment->unmapBuffer();
        }

#if RELEASE_SHOW_DEBUG
        LL_INFOS() << "completed in " << llformat("%.2f", timer.getElapsedTimeF32().value()) << "seconds" << LL_ENDL;
#endif
    }

    updateStarColors();
    updateStarGeometry(drawable);

    LLPipeline::sCompiles++;

    return true;
}

void LLVOWLSky::drawStars(void)
{
    //  render the stars as a sphere centered at viewer camera
    if (mStarsVerts.notNull())
    {
        mStarsVerts->setBuffer();
        mStarsVerts->drawArrays(LLRender::TRIANGLES, 0, getStarsNumVerts()*4);
    }
}

void LLVOWLSky::drawFsSky(void)
{
    if (mFsSkyVerts.isNull())
    {
        updateGeometry(mDrawable);
    }

    LLGLDisable disable_blend(GL_BLEND);

    mFsSkyVerts->setBuffer();
    mFsSkyVerts->drawRange(LLRender::TRIANGLES, 0, mFsSkyVerts->getNumVerts() - 1, mFsSkyVerts->getNumIndices(), 0);
    gPipeline.addTrianglesDrawn(mFsSkyVerts->getNumIndices());
    LLVertexBuffer::unbind();
}

void LLVOWLSky::drawDome(void)
{
    if (mStripsVerts.empty())
    {
        updateGeometry(mDrawable);
    }

    LLGLDepthTest gls_depth(GL_TRUE, GL_FALSE);

    std::vector< LLPointer<LLVertexBuffer> >::const_iterator strips_vbo_iter, end_strips;
    end_strips = mStripsVerts.end();
    for(strips_vbo_iter = mStripsVerts.begin(); strips_vbo_iter != end_strips; ++strips_vbo_iter)
    {
        LLVertexBuffer * strips_segment = strips_vbo_iter->get();

        strips_segment->setBuffer();

        strips_segment->drawRange(
            LLRender::TRIANGLE_STRIP,
            0, strips_segment->getNumVerts()-1, strips_segment->getNumIndices(),
            0);
        gPipeline.addTrianglesDrawn(strips_segment->getNumIndices());
    }

    LLVertexBuffer::unbind();
}

// <AP:WW> Black-body color for a star temperature (Kelvin).
LLColor4 LLVOWLSky::blackBodyColor(F32 temperature)
{
    LLColor4 color;
    temperature /= 100.0f;

    // Red
    if (temperature <= 66.0f) {
        color.mV[VRED] = 1.0f;
    } else {
        color.mV[VRED] = temperature - 60.0f;
        color.mV[VRED] = 329.698727446f * pow(color.mV[VRED], -0.1332047592f);
        color.mV[VRED] = llclamp(color.mV[VRED] / 255.0f, 0.0f, 1.0f);
    }

    // Green
    if (temperature <= 66.0f) {
        color.mV[VGREEN] = temperature;
        color.mV[VGREEN] = 99.4708025861f * log(color.mV[VGREEN]) - 161.1195681661f;
        color.mV[VGREEN] = llclamp(color.mV[VGREEN] / 255.0f, 0.0f, 1.0f);
    } else {
        color.mV[VGREEN] = temperature - 60.0f;
        color.mV[VGREEN] = 288.1221695283f * pow(color.mV[VGREEN], -0.0755148492f);
        color.mV[VGREEN] = llclamp(color.mV[VGREEN] / 255.0f, 0.0f, 1.0f);
    }

    // Blue
    if (temperature >= 66.0f) {
        color.mV[VBLUE] = 1.0f;
    } else if (temperature <= 19.0f) {
        color.mV[VBLUE] = 0.0f;
    } else {
        color.mV[VBLUE] = temperature - 10;
        color.mV[VBLUE] = 138.5177312231f * log(color.mV[VBLUE]) - 305.0447927307f;
        color.mV[VBLUE] = llclamp(color.mV[VBLUE] / 255.0f, 0.0f, 1.0f);
    }

    color.mV[VALPHA] = 1.0f;
    return color;
}

// The main initStars function orchestrates the generation
void LLVOWLSky::initStars()
{
    // Resize vectors to hold ALL stars (primary + dust)
    mStarVertices.resize(TOTAL_STARS);
    mStarColors.resize(TOTAL_STARS);
    mStarIntensities.resize(TOTAL_STARS);

    // --- Phase 1: Generate Primary Stars ---
    const F32 primary_min_intensity = 0.05f;
    const F32 primary_max_intensity = 1.0f;
    const F32 primary_brightness_exponent = 3.5f;
    const F32 primary_color_variation = 0.30f;

    generateProceduralStars(
        NUM_PRIMARY_STARS, 0,
        primary_min_intensity, primary_max_intensity, primary_brightness_exponent, primary_color_variation,
        mStarVertices, mStarColors, mStarIntensities
    );

    // --- Phase 2: Generate Dust Stars (fainter, denser) ---
    const F32 dust_min_intensity = 0.01f;
    const F32 dust_max_intensity = 0.15f;
    const F32 dust_brightness_exponent = 5.0f;
    const F32 dust_color_variation = 0.15f;

    generateProceduralStars(
        NUM_DUST_STARS, NUM_PRIMARY_STARS,
        dust_min_intensity, dust_max_intensity, dust_brightness_exponent, dust_color_variation,
        mStarVertices, mStarColors, mStarIntensities
    );
}

void LLVOWLSky::generateProceduralStars(
    U32 count, U32 startIndex,
    F32 min_intensity, F32 max_intensity, F32 brightness_exponent, F32 color_variation,
    std::vector<LLVector3>& vertices, std::vector<LLColor4>& colors, std::vector<F32>& intensities)
{
    const F32 DISTANCE_TO_STARS = LLEnvironment::instance().getCurrentSky()->getDomeRadius();

    // Milky Way band settings
    const bool enable_milky_way = true;
    const F32 milky_way_density_factor = 15.0f;
    LLVector3 milky_way_normal(0.5f, 0.0f, 0.866f);
    milky_way_normal.normVec();
    const F32 milky_way_thickness_factor = 0.4f;

    std::vector<LLVector3>::iterator v_p = vertices.begin() + startIndex;
    std::vector<LLColor4>::iterator v_c = colors.begin() + startIndex;
    std::vector<F32>::iterator v_i = intensities.begin() + startIndex;

    // Black-body temperature range (Kelvin)
    const F32 min_temperature = 3000.0f;   // Cool red M-type
    const F32 max_temperature = 25000.0f;  // Hot blue/white B/O-type
    const F32 temperature_range = max_temperature - min_temperature;

    U32 stars_generated = 0;
    while (stars_generated < count)
    {
        // 1. Candidate position on a full sphere
        LLVector3 candidate_pos;
        candidate_pos.mV[VX] = ll_frand() * 2.0f - 1.0f;
        candidate_pos.mV[VY] = ll_frand() * 2.0f - 1.0f;
        candidate_pos.mV[VZ] = ll_frand() * 2.0f - 1.0f;

        if (candidate_pos.magVec() < 1e-6f) { continue; }
        candidate_pos.normVec();

        // 2. Milky Way density probability
        F32 acceptance_probability = 1.0f;
        if (enable_milky_way)
        {
            F32 dist_from_plane = fabsf(candidate_pos * milky_way_normal);
            acceptance_probability *= pow(1.0f - dist_from_plane, 1.0f / milky_way_thickness_factor) * (milky_way_density_factor - 1.0f) + 1.0f;
        }

        // 3. Probabilistically keep or reject
        if (ll_frand() * acceptance_probability < 1.0f) { continue; }

        // 4. Distance tier
        F32 distance_scale = DISTANCE_TO_STARS;
        F32 distance_tier1 = DISTANCE_TO_STARS * 1.0f;
        F32 distance_tier2 = DISTANCE_TO_STARS * 100.0f;
        F32 distance_tier3 = DISTANCE_TO_STARS * 1000.0f;
        F32 prob = ll_frand();
        if (prob < 0.15) { distance_scale = distance_tier1; }
        else if (prob > .50) { distance_scale = distance_tier3; }
        else { distance_scale = distance_tier2; }
        *v_p = candidate_pos * distance_scale;

        // 5. Intrinsic intensity (before distance dimming)
        F32 intensity = pow(ll_frand(), brightness_exponent + (ll_frand() - 0.5f) * 1.0f);
        intensity = min_intensity + intensity * (max_intensity - min_intensity);
        intensity = llclamp(intensity, min_intensity, max_intensity);

        F32 intensity_for_color = intensity;

        // Apply distance dimming for final brightness/size
        F32 distance_dim_factor = 1.0f;
        if (distance_scale == distance_tier2) { distance_dim_factor = 0.50f; }
        else if (distance_scale == distance_tier3) { distance_dim_factor = 0.10f; }
        intensity *= distance_dim_factor;
        intensity = llmax(intensity, 0.0f);
        *v_i = intensity;

        // 6. Color from pre-dimming intensity via black-body temperature
        F32 intensity_factor_for_color = 0.0f;
        float layer_intensity_range = max_intensity - min_intensity;
        if (layer_intensity_range > 1e-5f)
        {
            intensity_factor_for_color = (intensity_for_color - min_intensity) / layer_intensity_range;
            intensity_factor_for_color = llclamp(intensity_factor_for_color, 0.0f, 1.0f);
        }

        float color_curve_exponent = 3.0f;
        float curved_intensity_factor = pow(intensity_factor_for_color, color_curve_exponent);

        F32 temperature = min_temperature + curved_intensity_factor * temperature_range;
        temperature = llclamp(temperature, min_temperature, max_temperature);

        LLColor4 star_color = blackBodyColor(temperature);

        star_color.mV[VRED]   += (ll_frand() * 2.0f - 1.0f) * color_variation;
        star_color.mV[VGREEN] += (ll_frand() * 2.0f - 1.0f) * color_variation;
        star_color.mV[VBLUE]  += (ll_frand() * 2.0f - 1.0f) * color_variation;
        star_color.mV[VALPHA] = 1.0f;
        star_color.clamp();
        *v_c = star_color;

        v_p++;
        v_c++;
        v_i++;
        stars_generated++;
    }
}
// </AP:WW>

void LLVOWLSky::buildStripsBuffer(U32 begin_stack,
                                  U32 end_stack,
                                  LLStrider<LLVector3> & vertices,
                                  LLStrider<LLVector2> & texCoords,
                                  LLStrider<U16> & indices,
                                  const F32 dome_radius,
                                  const U32& num_slices,
                                  const U32& num_stacks)
{
    U32 i, j;
    F32 phi0, theta, x0, y0, z0;
    const F32 reciprocal_num_stacks = 1.f / num_stacks;

    llassert(end_stack <= num_stacks);

    // stacks are iterated one-indexed since phi(0) was handled by the fan above
#if NEW_TESS
    for(i = begin_stack; i <= end_stack; ++i)
#else
    for(i = begin_stack + 1; i <= end_stack+1; ++i)
#endif
    {
        phi0 = calcPhi(i, reciprocal_num_stacks);

        for(j = 0; j < num_slices; ++j)
        {
            theta = F_TWO_PI * (float(j) / float(num_slices));

            // standard transformation from  spherical to
            // rectangular coordinates
            x0 = sin(phi0) * cos(theta);
            y0 = cos(phi0);
            z0 = sin(phi0) * sin(theta);

#if NEW_TESS
            *vertices++ = LLVector3(x0 * dome_radius, y0 * dome_radius, z0 * dome_radius);
#else
            if (i == num_stacks-2)
            {
                *vertices++ = LLVector3(x0*dome_radius, y0*dome_radius-1024.f*2.f, z0*dome_radius);
            }
            else if (i == num_stacks-1)
            {
                *vertices++ = LLVector3(0, y0*dome_radius-1024.f*2.f, 0);
            }
            else
            {
                *vertices++     = LLVector3(x0 * dome_radius, y0 * dome_radius, z0 * dome_radius);
            }
#endif

            // generate planar uv coordinates
            // note: x and z are transposed in order for things to animate
            // correctly in the global coordinate system where +x is east and
            // +y is north
            *texCoords++    = LLVector2((-z0 + 1.f) / 2.f, (-x0 + 1.f) / 2.f);
        }
    }

    //build triangle strip...
    *indices++ = 0 ;

    S32 k = 0 ;
    for(i = 1; i <= end_stack - begin_stack; ++i)
    {
        *indices++ = i * num_slices + k ;

        k = (k+1) % num_slices ;
        for(j = 0; j < num_slices ; ++j)
        {
            *indices++ = (i-1) * num_slices + k ;
            *indices++ = i * num_slices + k ;

            k = (k+1) % num_slices ;
        }

        if((--k) < 0)
        {
            k = num_slices - 1 ;
        }

        *indices++ = i * num_slices + k ;
    }
}

void LLVOWLSky::updateStarColors()
{
    std::vector<LLColor4>::iterator v_c = mStarColors.begin();
    std::vector<F32>::iterator v_i = mStarIntensities.begin();
    std::vector<LLVector3>::iterator v_p = mStarVertices.begin();

    const F32 var = 0.15f;
    const F32 min = 0.5f; //0.75f;
    //const F32 sunclose_max = 0.6f;
    //const F32 sunclose_range = 1 - sunclose_max;

    //F32 below_horizon = - llmin(0.0f, gSky.mVOSkyp->getToSunLast().mV[2]);
    //F32 brightness_factor = llmin(1.0f, below_horizon * 20);

    static S32 swap = 0;
    swap++;

    if ((swap % 2) == 1)
    {
        F32 intensity;                      //  max intensity of each star
        U32 x;
        for (x = 0; x < getStarsNumVerts(); ++x)
        {
            //F32 sundir_factor = 1;
            LLVector3 tostar = *v_p;
            tostar.normVec();
            //const F32 how_close_to_sun = tostar * gSky.mVOSkyp->getToSunLast();
            //if (how_close_to_sun > sunclose_max)
            //{
            //  sundir_factor = (1 - how_close_to_sun) / sunclose_range;
            //}
            intensity = *(v_i);
            F32 alpha = v_c->mV[VALPHA] + (ll_frand() - 0.5f) * var * intensity;
            if (alpha < min * intensity)
            {
                alpha = min * intensity;
            }
            if (alpha > intensity)
            {
                alpha = intensity;
            }
            //alpha *= brightness_factor * sundir_factor;

            alpha = llclamp(alpha, 0.f, 1.f);
            v_c->mV[VALPHA] = alpha;
            v_c++;
            v_i++;
            v_p++;
        }
    }
}

bool LLVOWLSky::updateStarGeometry(LLDrawable *drawable)
{
    LLStrider<LLVector3> verticesp;
    LLStrider<LLColor4U> colorsp;
    LLStrider<LLVector2> texcoordsp;
    LLStrider<F32> intensityp;

    if (mStarsVerts.isNull())
    {
        mStarsVerts = new LLVertexBuffer(LLDrawPoolWLSky::STAR_VERTEX_DATA_MASK);
        if (!mStarsVerts->allocateBuffer(getStarsNumVerts()*6, 0))
        {
            return false;
        }
    }

    bool success = mStarsVerts->getVertexStrider(verticesp)
        && mStarsVerts->getColorStrider(colorsp)
        && mStarsVerts->getTexCoord0Strider(texcoordsp)
        && mStarsVerts->getWeightStrider(intensityp);

    if(!success)
    {
        mStarsVerts->unmapBuffer();
        return false;
    }

    if (mStarVertices.size() < getStarsNumVerts() || mStarIntensities.size() < getStarsNumVerts())
    {
        mStarsVerts->unmapBuffer();
        return false;
    }

    for (U32 vtx = 0; vtx < getStarsNumVerts(); ++vtx)
    {
        LLVector3 at = mStarVertices[vtx];
        at.normVec();
        LLVector3 left = at%LLVector3(0,0,1);
        LLVector3 up = at%left;

        // <AP:WW> Resolution-dependent, intensity-scaled star sizing.
        // Determine the star's distance tier.
        const F32 DISTANCE_TO_STARS = LLEnvironment::instance().getCurrentSky()->getDomeRadius();
        F32 distance_tier1 = DISTANCE_TO_STARS * 1.0f;
        F32 distance_tier2 = DISTANCE_TO_STARS * 100.0f;
        F32 distance_tier3 = DISTANCE_TO_STARS * 1000.0f;

        F32 distance_scale;
        if (at.magVec() > (distance_tier3 - 0.1f))
        {
            distance_scale = distance_tier3;
        }
        else if (at.magVec() > (distance_tier2 - 0.1f))
        {
            distance_scale = distance_tier2;
        }
        else
        {
            distance_scale = distance_tier1;
        }

        // Vertical FOV (radians) and window size for pixel-accurate sizing.
        const F32 FOV_RADIANS = LLViewerCamera::instance().getView();
        const F32 screen_height = (F32)gViewerWindow->getWindowHeightRaw();

        // Base world size for a ~1-pixel star at this distance.
        F32 base = distance_scale / (screen_height / (2.0f * tanf(FOV_RADIANS / 2.0f)));

        // Max apparent pixel-size ratio per resolution category.
        const F32 height_720p = 720.0f;
        const F32 height_1080p = 1080.0f;
        const F32 height_4k_approx = 2160.0f;
        F32 target_max_pixel_ratio = 4.0f;
        if (screen_height <= height_720p) { target_max_pixel_ratio = 1.33f; }
        else if (screen_height <= height_1080p) { target_max_pixel_ratio = 2.0f; }
        else if (screen_height <= height_4k_approx) { target_max_pixel_ratio = 4.0f; }
        else { target_max_pixel_ratio = 8.0f; }

        // Scale from the 1-pixel base up to the target max by intensity.
        const F32 min_world_size = base;
        F32 world_size_influence = base * (target_max_pixel_ratio - 1.0f);
        if (world_size_influence < 0.0f) { world_size_influence = 0.0f; }
        F32 sc = min_world_size + mStarIntensities[vtx] * world_size_influence;
        // </AP:WW>

        left *= sc;
        up *= sc;

        *(verticesp++)  = mStarVertices[vtx];
        *(verticesp++) = mStarVertices[vtx]+up;
        *(verticesp++) = mStarVertices[vtx]+left+up;
        *(verticesp++)  = mStarVertices[vtx];
        *(verticesp++) = mStarVertices[vtx]+left+up;
        *(verticesp++) = mStarVertices[vtx]+left;

        *(texcoordsp++) = LLVector2(1,0);
        *(texcoordsp++) = LLVector2(1,1);
        *(texcoordsp++) = LLVector2(0,1);
        *(texcoordsp++) = LLVector2(1,0);
        *(texcoordsp++) = LLVector2(0,1);
        *(texcoordsp++) = LLVector2(0,0);

        // <FS:ND> Only convert to LLColour4U once

        // *(colorsp++)    = LLColor4U(mStarColors[vtx]);
        // *(colorsp++)    = LLColor4U(mStarColors[vtx]);
        // *(colorsp++)    = LLColor4U(mStarColors[vtx]);
        // *(colorsp++)    = LLColor4U(mStarColors[vtx]);
        // *(colorsp++)    = LLColor4U(mStarColors[vtx]);
        // *(colorsp++)    = LLColor4U(mStarColors[vtx]);

        LLColor4U color4u(mStarColors[vtx]);
        *(colorsp++)    = color4u;
        *(colorsp++)    = color4u;
        *(colorsp++)    = color4u;
        *(colorsp++)    = color4u;
        *(colorsp++)    = color4u;
        *(colorsp++)    = color4u;

        // </FS:ND>

        // <AP:WW> Write per-star intensity to the weight channel for the shader.
        float current_intensity = mStarIntensities[vtx];
        *(intensityp++) = current_intensity;
        *(intensityp++) = current_intensity;
        *(intensityp++) = current_intensity;
        *(intensityp++) = current_intensity;
        *(intensityp++) = current_intensity;
        *(intensityp++) = current_intensity;
        // </AP:WW>

    }

    mStarsVerts->unmapBuffer();
    return true;
}
