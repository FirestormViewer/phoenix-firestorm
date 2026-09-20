/**
 * @file lldrawpoolalpha.cpp
 * @brief LLDrawPoolAlpha class implementation
 *
 * $LicenseInfo:firstyear=2002&license=viewerlgpl$
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

#include <optional>

#include "lldrawpoolalpha.h"

#include "llglheaders.h"
#include "llviewercontrol.h"
#include "llcriticaldamp.h"
#include "llfasttimer.h"
#include "llrender.h"

#include "llcubemap.h"
#include "llsky.h"
#include "lldrawable.h"
#include "llface.h"
#include "llviewercamera.h"
#include "llviewertexturelist.h"    // For debugging
#include "llviewerobjectlist.h" // For debugging
#include "llviewerwindow.h"
#include "llviewerdisplay.h"
#include "llappviewer.h"
#include "pipeline.h"
#include "llviewershadermgr.h"
#include "llviewerregion.h"
#include "lldrawpoolwater.h"
#include "llspatialpartition.h"
#include "llglcommonfunc.h"
#include "llvoavatar.h"
#include "gltfscenemanager.h"

#include "llenvironment.h"

bool LLDrawPoolAlpha::sShowDebugAlpha = false;
bool LLDrawPoolAlpha::sShowDebugAlphaRigged = false;

#define current_shader (LLGLSLShader::sCurBoundShaderPtr)

LLVector4 LLDrawPoolAlpha::sWaterPlane;

// minimum alpha before discarding a fragment
static const F32 MINIMUM_ALPHA = 0.004f; // ~ 1/255

// minimum alpha before discarding a fragment when rendering impostors
static const F32 MINIMUM_IMPOSTOR_ALPHA = 0.1f;

LLDrawPoolAlpha::LLDrawPoolAlpha(U32 type) :
        LLRenderPass(type), target_shader(NULL),
        mColorSFactor(LLRender::BF_UNDEF), mColorDFactor(LLRender::BF_UNDEF),
        mAlphaSFactor(LLRender::BF_UNDEF), mAlphaDFactor(LLRender::BF_UNDEF)
{

}

LLDrawPoolAlpha::~LLDrawPoolAlpha()
{
}


void LLDrawPoolAlpha::prerender()
{
    mShaderLevel = LLViewerShaderMgr::instance()->getShaderLevel(LLViewerShaderMgr::SHADER_OBJECT);
}

S32 LLDrawPoolAlpha::getNumPostDeferredPasses()
{
    return 1;
}

// set some common parameters on the given shader to prepare for alpha rendering
static void prepare_alpha_shader(LLGLSLShader* shader, bool deferredEnvironment, F32 water_sign)
{
    static LLCachedControl<F32> displayGamma(gSavedSettings, "RenderDeferredDisplayGamma");
    F32 gamma = displayGamma;

    static LLStaticHashedString waterSign("waterSign");

    // Does this deferred shader need environment uniforms set such as sun_dir, etc. ?
    // NOTE: We don't actually need a gbuffer since we are doing forward rendering (for transparency) post deferred rendering
    // TODO: bindDeferredShader() probably should have the updating of the environment uniforms factored out into updateShaderEnvironmentUniforms()
    // i.e. shaders\class1\deferred\alphaF.glsl
    if (deferredEnvironment)
    {
        shader->mCanBindFast = false;
    }

    shader->bind();
    shader->uniform1f(LLShaderMgr::DISPLAY_GAMMA, (gamma > 0.1f) ? 1.0f / gamma : (1.0f / 2.2f));

    if (LLPipeline::sRenderingHUDs)
    { // for HUD attachments, only the pre-water pass is executed and we never want to clip anything
        LLVector4 near_clip(0, 0, -1, 0);
        shader->uniform1f(waterSign, 1.f);
        shader->uniform4fv(LLShaderMgr::WATER_WATERPLANE, 1, near_clip.mV);
    }
    else
    {
        shader->uniform1f(waterSign, water_sign);
        shader->uniform4fv(LLShaderMgr::WATER_WATERPLANE, 1, LLDrawPoolAlpha::sWaterPlane.mV);
    }

    if (LLPipeline::sImpostorRender)
    {
        shader->setMinimumAlpha(MINIMUM_IMPOSTOR_ALPHA);
    }
    else
    {
        shader->setMinimumAlpha(MINIMUM_ALPHA);
    }

    //also prepare rigged variant
    if (shader->mRiggedVariant && shader->mRiggedVariant != shader)
    {
        prepare_alpha_shader(shader->mRiggedVariant, deferredEnvironment, water_sign);
    }
}

extern bool gCubeSnapshot;

void LLDrawPoolAlpha::renderPostDeferred(S32 pass)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DRAWPOOL;

    if (LLPipeline::isWaterClip() && getType() == LLDrawPool::POOL_ALPHA_PRE_WATER)
    { // don't render alpha objects on the other side of the water plane if water is opaque
        return;
    }

    F32 water_sign = 1.f;

    if (getType() == LLDrawPool::POOL_ALPHA_PRE_WATER)
    {
        water_sign = -1.f;
    }

    if (LLPipeline::sUnderWaterRender)
    {
        water_sign *= -1.f;
    }

    // prepare shaders
    llassert(LLPipeline::sRenderDeferred);

    // <FS> interleave rigged + world alpha for the post-water world pass
    const bool interleave = LLPipeline::canUseInterleavedAlpha() &&
                            getType() == LLDrawPool::POOL_ALPHA_POST_WATER;
    // </FS>

    emissive_shader = &gDeferredEmissiveProgram;
    prepare_alpha_shader(emissive_shader, false, water_sign);

    pbr_emissive_shader = &gPBRGlowProgram;
    prepare_alpha_shader(pbr_emissive_shader, false, water_sign);


    fullbright_shader   =
        (LLPipeline::sImpostorRender) ? &gDeferredFullbrightAlphaMaskProgram :
        (LLPipeline::sRenderingHUDs) ? &gHUDFullbrightAlphaMaskAlphaProgram :
        &gDeferredFullbrightAlphaMaskAlphaProgram;
    prepare_alpha_shader(fullbright_shader, true, water_sign);

    simple_shader   =
        (LLPipeline::sImpostorRender) ? &gDeferredAlphaImpostorProgram :
        (LLPipeline::sRenderingHUDs) ? &gHUDAlphaProgram :
        &gDeferredAlphaProgram;

    prepare_alpha_shader(simple_shader, true, water_sign); //prime simple shader (loads shadow relevant uniforms)

    LLGLSLShader* materialShader = gDeferredMaterialProgram;
    for (int i = 0; i < LLMaterial::SHADER_COUNT*2; ++i)
    {
        prepare_alpha_shader(&materialShader[i], true, water_sign);
    }

    pbr_shader =
        (LLPipeline::sRenderingHUDs) ? &gHUDPBRAlphaProgram :
        &gDeferredPBRAlphaProgram;

    prepare_alpha_shader(pbr_shader, true, water_sign);

    // explicitly unbind here so render loop doesn't make assumptions about the last shader
    // already being setup for rendering
    LLGLSLShader::unbind();

    // <FS> Vulkanstorm: alpha OIT (PPLL / bounded depth peeling) dispatch. OIT is an
    // alternative to both the legacy two-pass and the interleaved stream, deliberately
    // limited to the main post-water view. Mirrors/hero probes, cube snapshots, HUDs,
    // impostors and pre-water alpha retain the stock path.
    static LLCachedControl<U32> alpha_method(gSavedSettings, "RenderAlphaSortMethod", 0);
    const bool oit_view_eligible =
        getType() == LLDrawPool::POOL_ALPHA_POST_WATER &&
        gPipeline.mRT == &gPipeline.mMainRT &&
        !LLPipeline::sRenderingHUDs &&
        !LLPipeline::sImpostorRender &&
        !LLPipeline::sReflectionRender &&
        !gCubeSnapshot;

    bool ppll_resources = true;
    if (alpha_method == 1 && oit_view_eligible)
    {
        gPipeline.allocateAlphaOITBuffers(
            gPipeline.mRT->screen.getWidth(), gPipeline.mRT->screen.getHeight());
        ppll_resources = LLPipeline::sRenderAlphaOITSupported;
    }
    bool depth_resources = true;
    if (alpha_method == 2 && oit_view_eligible)
    {
        depth_resources = gPipeline.allocateAlphaDepthPeelBuffers(
            gPipeline.mRT->screen.getWidth(), gPipeline.mRT->screen.getHeight());
    }
    bool use_ppll = alpha_method == 1
                 && oit_view_eligible
                 && ppll_resources
                 && LLPipeline::sRenderAlphaOITSupported;
    bool use_depth_peel = alpha_method == 2
                       && depth_resources
                       && oit_view_eligible
                       && gSavedSettings.getBOOL("RenderAlphaDepthPeelAvailable");

    // Depth peeling replays the complete alpha draw list twice per exact layer.
    // Region arrival is also the worst possible moment to multiply that work because
    // geometry and drawable rebuild queues are at their peak.  Break the feedback loop
    // after a teleport or a long frame and allow those queues to drain on the stock path.
    static LLFrameTimer depth_peel_recovery_timer;
    static bool depth_peel_recovering = false;
    const bool transient_load = gTeleportDisplay || gFrameIntervalSeconds.value() > 0.100f;
    if (transient_load)
    {
        depth_peel_recovery_timer.reset();
        depth_peel_recovering = true;
    }
    else if (depth_peel_recovering && depth_peel_recovery_timer.getElapsedTimeF32() >= 2.f)
    {
        depth_peel_recovering = false;
    }
    use_depth_peel = use_depth_peel && !depth_peel_recovering;

    if (use_ppll)
    {
        drawGLTFScene();
        if (gPipeline.beginAlphaOITCapture())
        {
            setOITMode(1);
            forwardRender(EAlphaStream::RIGGED, ALPHA_OIT_CAPTURE);
            forwardRender(EAlphaStream::WORLD, ALPHA_OIT_CAPTURE);
            setOITMode(0);
            gPipeline.endAlphaOITCapture();
            gPipeline.compositeAlphaOIT();

            // PPLL capture cannot write the shared scene depth: doing so would reject
            // transparent fragments before the per-pixel resolve can order them. Restore
            // the legacy rigged depth contribution before residual alpha and emissive
            // replay. The legacy path establishes this depth while drawing rigged alpha;
            // without it, avatar-linked glow is tested only against opaque scene depth
            // and can bloom through nearby geometry as camera-dependent bright sprites.
            {
                LLGLDepthTest rigged_depth(GL_TRUE, GL_TRUE);
                gGL.setColorMask(false, false);
                renderAlpha(getVertexDataMask() | LLVertexBuffer::MAP_TEXTURE_INDEX |
                                LLVertexBuffer::MAP_TANGENT | LLVertexBuffer::MAP_TEXCOORD1 |
                                LLVertexBuffer::MAP_TEXCOORD2,
                            true, EAlphaStream::RIGGED, ALPHA_OIT_NONE);
                gGL.setColorMask(true, false);
            }

            // Particles and unsupported blend equations always remain on the stock
            // residual path. Fullbright source-over alpha was captured with lit alpha.
            forwardRender(EAlphaStream::RIGGED, ALPHA_OIT_RESIDUAL);
            forwardRender(EAlphaStream::WORLD, ALPHA_OIT_RESIDUAL);
        }
        else
        {
            if (!LLPipeline::sRenderingHUDs)
            {
                forwardRender(EAlphaStream::RIGGED);
            }
            forwardRender();
        }
    }
    else if (use_depth_peel)
    {
        drawGLTFScene();
        static LLCachedControl<U32> peel_layers(gSavedSettings, "RenderAlphaDepthPeelLayers", 8);
        static LLCachedControl<U32> peel_budget_ms(gSavedSettings, "RenderAlphaDepthPeelTimeBudgetMS", 4);
        const U32 layer_count = llclamp((U32)peel_layers, 1u, 32u);
        const F32 time_budget = (F32)llclamp((U32)peel_budget_ms, 1u, 50u) * 0.001f;
        bool valid = true;
        U32 completed_layers = 0;
        LLTimer peel_timer;

        for (U32 layer = 0; layer < layer_count; ++layer)
        {
            if (!gPipeline.beginAlphaDepthPeelSelection(layer))
            {
                valid = false;
                break;
            }

            setOITMode(2, gPipeline.getAlphaDepthPeelPrevious(layer), layer == 0);
            forwardRender(EAlphaStream::RIGGED, ALPHA_DEPTH_PEEL_SELECT);
            forwardRender(EAlphaStream::WORLD, ALPHA_DEPTH_PEEL_SELECT);
            gPipeline.endAlphaDepthPeelSelection();

            setOITMode(3, gPipeline.getAlphaDepthPeelSelected(layer));
            forwardRender(EAlphaStream::RIGGED, ALPHA_DEPTH_PEEL_REPLAY);
            forwardRender(EAlphaStream::WORLD, ALPHA_DEPTH_PEEL_REPLAY);
            completed_layers = layer + 1;

            // This is a CPU submission budget, not a GPU timestamp.  It nevertheless
            // prevents a busy region from multiplying an expensive traversal by every
            // configured layer in one frame.  The remaining fragments use the tail pass.
            if (peel_timer.getElapsedTimeF32() >= time_budget)
            {
                break;
            }
        }
        setOITMode(0);

        if (valid && completed_layers > 0)
        {
            // Layers are selected farthest-to-nearest.  Render everything nearer than
            // the last exact layer once with the legacy source-over ordering.  Without
            // this pass, ordinary alpha beyond the configured peel count disappears
            // (windows, foliage, decals, appliers and other stacked alpha cards).
            setOITMode(4, gPipeline.getAlphaDepthPeelSelected(completed_layers - 1));
            forwardRender(EAlphaStream::RIGGED, ALPHA_DEPTH_PEEL_TAIL);
            forwardRender(EAlphaStream::WORLD, ALPHA_DEPTH_PEEL_TAIL);
            setOITMode(0);

            // Particles and custom blend equations remain entirely legacy.
            // Standard fullbright alpha was peeled together with lit alpha.
            forwardRender(EAlphaStream::RIGGED, ALPHA_OIT_RESIDUAL);
            forwardRender(EAlphaStream::WORLD, ALPHA_OIT_RESIDUAL);
        }
        else
        {
            forwardRender(EAlphaStream::RIGGED);
            forwardRender();
        }
    }
    else if (interleave)
    {
        // single pass: depth-interleave whole avatars with the distance-sorted
        // alpha groups so world alpha composites correctly around avatars.
        // The centralized eligibility gate limits the merged walk to the world
        // camera; HUD, cube, and reflection renders keep legacy two-pass order.
        // postSort deliberately leaves the shared lists in legacy order for
        // pre-water; switch them only for the consumer that performs the merge.
        gPipeline.sortAlphaGroupsForInterleaving();
        forwardRender(EAlphaStream::INTERLEAVED);
    }
    else
    {
        if (!LLPipeline::sRenderingHUDs)
        {
            // first pass, render rigged objects only and render to depth buffer
            forwardRender(EAlphaStream::RIGGED);
        }

        // second pass, regular forward alpha rendering
        forwardRender();
    }
    // </FS>

    // final pass, render to depth for depth of field effects
    if (!LLPipeline::sImpostorRender && LLPipeline::RenderDepthOfField && !gCubeSnapshot && !LLPipeline::sRenderingHUDs && getType() == LLDrawPool::POOL_ALPHA_POST_WATER)
    {
        //update depth buffer sampler
        simple_shader = fullbright_shader = &gDeferredFullbrightAlphaMaskProgram;

        simple_shader->bind();
        simple_shader->setMinimumAlpha(0.33f);

        // mask off color buffer writes as we're only writing to depth buffer
        gGL.setColorMask(false, false);

        // If the face is more than 90% transparent, then don't update the Depth buffer for Dof
        // We don't want the nearly invisible objects to cause of DoF effects
        renderAlpha(getVertexDataMask() | LLVertexBuffer::MAP_TEXTURE_INDEX | LLVertexBuffer::MAP_TANGENT | LLVertexBuffer::MAP_TEXCOORD1 | LLVertexBuffer::MAP_TEXCOORD2,
            true); // <--- discard mostly transparent faces

        gGL.setColorMask(true, false);
    }
}

// <FS> void LLDrawPoolAlpha::forwardRender(bool rigged)
void LLDrawPoolAlpha::forwardRender(EAlphaStream stream, EAlphaOITPhase oit_phase)
// </FS>
{
    gPipeline.enableLightsDynamic();

    LLGLSPipelineAlpha gls_pipeline_alpha;

    // <FS> Vulkanstorm: capture shaders append and discard, so the framebuffer remains untouched
    if (oit_phase != ALPHA_OIT_CAPTURE)
    {
        //enable writing to alpha for emissive effects
        gGL.setColorMask(true, true);
    }
    // </FS>

    // <FS> Vulkanstorm: write_depth = rigged || ... on the stock paths; OIT capture/peel
    // passes never write the main depth buffer
    bool write_depth = (oit_phase == ALPHA_OIT_NONE || oit_phase == ALPHA_OIT_RESIDUAL) &&
        (stream == EAlphaStream::RIGGED ||
        LLDrawPoolWater::sSkipScreenCopy
        // we want depth written so that rendered alpha will
        // contribute to the alpha mask used for impostors
        || LLPipeline::sImpostorRenderAlphaDepthPass
        || getType() == LLDrawPoolAlpha::POOL_ALPHA_PRE_WATER); // needed for accurate water fog
    // </FS>


    // <FS> in interleaved mode depth writes are decided per group inside renderAlpha
    LLGLDepthTest depth(GL_TRUE, (write_depth && stream != EAlphaStream::INTERLEAVED) ? GL_TRUE : GL_FALSE);
    // </FS>

    mColorSFactor = LLRender::BF_SOURCE_ALPHA;           // } regular alpha blend
    mColorDFactor = LLRender::BF_ONE_MINUS_SOURCE_ALPHA; // }
    mAlphaSFactor = LLRender::BF_ZERO;                         // } glow suppression
    mAlphaDFactor = LLRender::BF_ONE_MINUS_SOURCE_ALPHA;       // }
    gGL.blendFunc(mColorSFactor, mColorDFactor, mAlphaSFactor, mAlphaDFactor);

    // <FS> if (rigged && mType == LLDrawPool::POOL_ALPHA_POST_WATER)
    // Vulkanstorm: OIT passes draw the GLTF scene to depth via drawGLTFScene() beforehand
    if (oit_phase == ALPHA_OIT_NONE && stream != EAlphaStream::WORLD && mType == LLDrawPool::POOL_ALPHA_POST_WATER)
    // </FS>
    { // draw GLTF scene to depth buffer before rigged alpha
        // <FS> (explicit depth-write scope: in interleaved mode the pass-wide depth
        // state above has writes off)
        LLGLDepthTest gltf_depth(GL_TRUE, GL_TRUE);
        // </FS>
        LL::GLTFSceneManager::instance().render(false, false);
        LL::GLTFSceneManager::instance().render(false, true);
        LL::GLTFSceneManager::instance().render(false, false, true);
        LL::GLTFSceneManager::instance().render(false, true, true);
    }

    // If the face is more than 90% transparent, then don't update the Depth buffer for Dof
    // We don't want the nearly invisible objects to cause of DoF effects
    renderAlpha(getVertexDataMask() | LLVertexBuffer::MAP_TEXTURE_INDEX | LLVertexBuffer::MAP_TANGENT | LLVertexBuffer::MAP_TEXCOORD1 | LLVertexBuffer::MAP_TEXCOORD2, false, stream, oit_phase);

    // <FS> Vulkanstorm: the capture pass left the color mask untouched above
    if (oit_phase != ALPHA_OIT_CAPTURE)
    {
        gGL.setColorMask(true, false);
    }
    // </FS>

    // <FS> if (!rigged && (LLPipeline::sRenderingHUDs || getType() == LLDrawPoolAlpha::POOL_ALPHA_POST_WATER))
    if (stream != EAlphaStream::RIGGED &&
        (oit_phase == ALPHA_OIT_NONE || oit_phase == ALPHA_OIT_RESIDUAL) &&
        (LLPipeline::sRenderingHUDs || getType() == LLDrawPoolAlpha::POOL_ALPHA_POST_WATER))
    // </FS>
    { //render "highlight alpha" on final non-rigged pass for non-HUDs (HUDs only run pre-water alpha pass)
        // NOTE -- hacky call here protected by !rigged instead of alongside "forwardRender"
        // so renderDebugAlpha is executed while gls_pipeline_alpha and depth GL state
        // variables above are still in scope
        renderDebugAlpha();
    }
}

void LLDrawPoolAlpha::drawGLTFScene()
{
    if (getType() != LLDrawPool::POOL_ALPHA_POST_WATER)
    {
        return;
    }

    LL::GLTFSceneManager::instance().render(false, false);
    LL::GLTFSceneManager::instance().render(false, true);
    LL::GLTFSceneManager::instance().render(false, false, true);
    LL::GLTFSceneManager::instance().render(false, true, true);
}

// <FS> Vulkanstorm: push the OIT mode/uniforms (and the depth-peel selected-depth
// sampler) into every alpha shader variant used by this pool
void LLDrawPoolAlpha::setOITMode(S32 mode, LLRenderTarget* peel_depth, bool first_peel)
{
    static const LLStaticHashedString sOITMode("oit_mode");
    static const LLStaticHashedString sOITNodeCap("oit_node_cap");
    static const LLStaticHashedString sPeelFirst("alpha_peel_first");
    const S32 node_cap = (S32)llmin<U32>(gPipeline.getAlphaOITNodeCap(), 0x7fffffffu);
    mAlphaPeelDepth = (mode == 2 || mode == 3 || mode == 4) ? peel_depth : nullptr;

    auto apply = [&](LLGLSLShader* shader)
    {
        if (!shader)
        {
            return;
        }
        shader->bind();
        shader->uniform1i(sOITMode, mode);
        shader->uniform1i(sOITNodeCap, node_cap);
        shader->uniform1i(sPeelFirst, first_peel ? 1 : 0);
        if (mAlphaPeelDepth)
        {
            shader->bindTexture(LLShaderMgr::ALPHA_PEEL_DEPTH, mAlphaPeelDepth,
                                false, LLTexUnit::TFO_POINT);
        }
        if (shader->mRiggedVariant && shader->mRiggedVariant != shader)
        {
            shader->mRiggedVariant->bind();
            shader->mRiggedVariant->uniform1i(sOITMode, mode);
            shader->mRiggedVariant->uniform1i(sOITNodeCap, node_cap);
            shader->mRiggedVariant->uniform1i(sPeelFirst, first_peel ? 1 : 0);
            if (mAlphaPeelDepth)
            {
                shader->mRiggedVariant->bindTexture(LLShaderMgr::ALPHA_PEEL_DEPTH, mAlphaPeelDepth,
                                                    false, LLTexUnit::TFO_POINT);
            }
        }
    };

    apply(simple_shader);
    apply(fullbright_shader);
    apply(pbr_shader);

    LLGLSLShader* material_shader = gDeferredMaterialProgram;
    for (S32 i = 0; i < LLMaterial::SHADER_COUNT * 2; ++i)
    {
        if ((i & 0x3) == 1)
        {
            apply(&material_shader[i]);
        }
    }
    LLGLSLShader::unbind();
}
// </FS>

void LLDrawPoolAlpha::renderDebugAlpha()
{
    if (sShowDebugAlpha && !gCubeSnapshot && !LLPipeline::sReflectionRender)
    {
        gHighlightProgram.bind();
        gGL.diffuseColor4f(1, 0, 0, 1);
        gGL.getTexUnit(0)->bindFast(LLViewerFetchedTexture::getSmokeImage());


        renderAlphaHighlight();

        pushUntexturedBatches(LLRenderPass::PASS_ALPHA_MASK);
        pushUntexturedBatches(LLRenderPass::PASS_ALPHA_INVISIBLE);

        // Material alpha mask
        gGL.diffuseColor4f(0, 0, 1, 1);
        pushUntexturedBatches(LLRenderPass::PASS_MATERIAL_ALPHA_MASK);
        pushUntexturedBatches(LLRenderPass::PASS_NORMMAP_MASK);
        pushUntexturedBatches(LLRenderPass::PASS_SPECMAP_MASK);
        pushUntexturedBatches(LLRenderPass::PASS_NORMSPEC_MASK);
        pushUntexturedBatches(LLRenderPass::PASS_FULLBRIGHT_ALPHA_MASK);
        pushUntexturedBatches(LLRenderPass::PASS_GLTF_PBR_ALPHA_MASK);

        gGL.diffuseColor4f(0, 1, 0, 1);
        pushUntexturedBatches(LLRenderPass::PASS_INVISIBLE);
        // <FS:Beq> FIRE-32132 et al. Allow rigged mesh transparency highlights to be toggled
        if (sShowDebugAlphaRigged)
        {
        // </FS:Beq>
        gHighlightProgram.mRiggedVariant->bind();
        gGL.diffuseColor4f(0, 1, 0, 1);// <FS:Beq/> FIRE-32132 et al. (can plain PASS_ALPHA_MASK_RIGGED exist?) paint it green if so.
        pushRiggedBatches(LLRenderPass::PASS_ALPHA_MASK_RIGGED, false);
        pushRiggedBatches(LLRenderPass::PASS_ALPHA_INVISIBLE_RIGGED, false);

        // Material alpha mask
        gGL.diffuseColor4f(0, 1, 1, 1);// <FS:Beq/> FIRE-32132 et al. Allow rigged mesh transparency highlights to be toggled
        pushRiggedBatches(LLRenderPass::PASS_MATERIAL_ALPHA_MASK_RIGGED, false);
        pushRiggedBatches(LLRenderPass::PASS_NORMMAP_MASK_RIGGED, false);
        pushRiggedBatches(LLRenderPass::PASS_SPECMAP_MASK_RIGGED, false);
        pushRiggedBatches(LLRenderPass::PASS_NORMSPEC_MASK_RIGGED, false);
        pushRiggedBatches(LLRenderPass::PASS_FULLBRIGHT_ALPHA_MASK_RIGGED, false);
        pushRiggedBatches(LLRenderPass::PASS_GLTF_PBR_ALPHA_MASK_RIGGED, false);

        gGL.diffuseColor4f(0, 1, 0, 1);
        pushRiggedBatches(LLRenderPass::PASS_INVISIBLE_RIGGED, false);
        // <FS:Beq> FIRE-32132 et al. Allow rigged mesh transparency highlights to be toggled
        }
        // </FS:Beq>
        LLGLSLShader::sCurBoundShaderPtr->unbind();
    }
}

void LLDrawPoolAlpha::renderAlphaHighlight()
{
    for (int pass = 0; pass < 2; ++pass)
    { //two passes, one rigged and one not
        const LLVOAvatar* lastAvatar = nullptr;
        U64 lastMeshId = 0;
        bool skipLastSkin = false;

        LLCullResult::sg_iterator begin = pass == 0 ? gPipeline.beginAlphaGroups() : gPipeline.beginRiggedAlphaGroups();
        LLCullResult::sg_iterator end = pass == 0 ? gPipeline.endAlphaGroups() : gPipeline.endRiggedAlphaGroups();

        for (LLCullResult::sg_iterator i = begin; i != end; ++i)
        {
            LLSpatialGroup* group = *i;
            if (group->getSpatialPartition()->mRenderByGroup &&
                !group->isDead())
            {
                LLSpatialGroup::drawmap_elem_t& draw_info = group->mDrawMap[LLRenderPass::PASS_ALPHA+pass]; // <-- hacky + pass to use PASS_ALPHA_RIGGED on second pass

                for (LLSpatialGroup::drawmap_elem_t::iterator k = draw_info.begin(); k != draw_info.end(); ++k)
                {
                    LLDrawInfo& params = **k;

                    bool rigged = (params.mAvatar != nullptr);
                    gHighlightProgram.bind(rigged);

                    if (rigged)
                    {
                        if (!uploadMatrixPalette(params.mAvatar, params.mSkinInfo, lastAvatar, lastMeshId, skipLastSkin))
                        { // failed to upload matrix palette, skip rendering
                            continue;
                        }
                    }

                    // <FS:Beq> FIRE-32132 et al. Allow rigged mesh transparency highlights to be toggled
                    if (rigged && !sShowDebugAlphaRigged)
                    {
                        // if we don't want to show rigged alpha highlights then skip
                        continue;
                    }
                    else if (rigged && sShowDebugAlphaRigged)
                    {
                        // if we do and this is rigged then use a different colour
                        gGL.diffuseColor4f(1, 0.5, 0, 1);
                    }
                    else // NB dangling else to drop through to "normal behaviour"
                    // </FS:Beq>
                    gGL.diffuseColor4f(1, 0, 0, 1);
                    LLRenderPass::applyModelMatrix(params);
                    params.mVertexBuffer->setBuffer();
                    params.mVertexBuffer->drawRange(LLRender::TRIANGLES, params.mStart, params.mEnd, params.mCount, params.mOffset);
                }
            }
        }
    }

    // make sure static version of highlight shader is bound before returning
    gHighlightProgram.bind();
}

inline bool IsFullbright(LLDrawInfo& params)
{
    return params.mFullbright;
}

inline bool IsMaterial(LLDrawInfo& params)
{
    return params.mMaterial != nullptr;
}

inline bool IsEmissive(LLDrawInfo& params)
{
    return params.mVertexBuffer->hasDataType(LLVertexBuffer::TYPE_EMISSIVE);
}

inline void Draw(LLDrawInfo* draw, U32 mask)
{
    draw->mVertexBuffer->setBuffer();
    LLRenderPass::applyModelMatrix(*draw);
    draw->mVertexBuffer->drawRange(LLRender::TRIANGLES, draw->mStart, draw->mEnd, draw->mCount, draw->mOffset);
}

bool LLDrawPoolAlpha::TexSetup(LLDrawInfo* draw, bool use_material)
{
    bool tex_setup = false;

    if (draw->mGLTFMaterial)
    {
        if (draw->mTextureMatrix)
        {
            tex_setup = true;
            gGL.getTexUnit(0)->activate();
            gGL.matrixMode(LLRender::MM_TEXTURE);
            gGL.loadMatrix((GLfloat*)draw->mTextureMatrix->mMatrix);
            gPipeline.mTextureMatrixOps++;
        }
    }
    else
    {
        if (!LLPipeline::sRenderingHUDs && use_material && current_shader)
        {
            if (draw->mNormalMap)
            {
                current_shader->bindTexture(LLShaderMgr::BUMP_MAP, draw->mNormalMap);
            }

            if (draw->mSpecularMap)
            {
                current_shader->bindTexture(LLShaderMgr::SPECULAR_MAP, draw->mSpecularMap);
            }
        }
        else if (current_shader == simple_shader || current_shader == simple_shader->mRiggedVariant)
        {
            current_shader->bindTexture(LLShaderMgr::BUMP_MAP, LLViewerFetchedTexture::sFlatNormalImagep);
            current_shader->bindTexture(LLShaderMgr::SPECULAR_MAP, LLViewerFetchedTexture::sWhiteImagep);
        }
        if (draw->mTextureList.size() > 1)
        {
            for (U32 i = 0; i < draw->mTextureList.size(); ++i)
            {
                if (draw->mTextureList[i].notNull())
                {
                    gGL.getTexUnit(i)->bindFast(draw->mTextureList[i]);
                }
            }
        }
        else
        { //not batching textures or batch has only 1 texture -- might need a texture matrix
            if (draw->mTexture.notNull())
            {
                if (use_material)
                {
                    current_shader->bindTexture(LLShaderMgr::DIFFUSE_MAP, draw->mTexture);
                }
                else
                {
                    gGL.getTexUnit(0)->bindFast(draw->mTexture);
                }

                if (draw->mTextureMatrix)
                {
                    tex_setup = true;
                    gGL.getTexUnit(0)->activate();
                    gGL.matrixMode(LLRender::MM_TEXTURE);
                    gGL.loadMatrix((GLfloat*)draw->mTextureMatrix->mMatrix);
                    gPipeline.mTextureMatrixOps++;
                }
            }
            else
            {
                gGL.getTexUnit(0)->unbindFast(LLTexUnit::TT_TEXTURE);
            }
        }
    }

    return tex_setup;
}

void LLDrawPoolAlpha::RestoreTexSetup(bool tex_setup)
{
    if (tex_setup)
    {
        gGL.getTexUnit(0)->activate();
        gGL.matrixMode(LLRender::MM_TEXTURE);
        gGL.loadIdentity();
        gGL.matrixMode(LLRender::MM_MODELVIEW);
    }
}

void LLDrawPoolAlpha::drawEmissive(LLDrawInfo* draw)
{
    LLGLSLShader::sCurBoundShaderPtr->uniform1f(LLShaderMgr::EMISSIVE_BRIGHTNESS, 1.f);
    draw->mVertexBuffer->setBuffer();
    // OIT residual replay can queue this face without drawing its normal batch.
    // Establish its transform here instead of inheriting the previous batch's.
    LLRenderPass::applyModelMatrix(*draw);
    draw->mVertexBuffer->drawRange(LLRender::TRIANGLES, draw->mStart, draw->mEnd, draw->mCount, draw->mOffset);
}


void LLDrawPoolAlpha::renderEmissives(std::vector<LLDrawInfo*>& emissives)
{
    emissive_shader->bind();
    emissive_shader->uniform1f(LLShaderMgr::EMISSIVE_BRIGHTNESS, 1.f);

    for (LLDrawInfo* draw : emissives)
    {
        bool tex_setup = TexSetup(draw, false);
        drawEmissive(draw);
        RestoreTexSetup(tex_setup);
    }
}

void LLDrawPoolAlpha::renderPbrEmissives(std::vector<LLDrawInfo*>& emissives)
{
    pbr_emissive_shader->bind();

    for (LLDrawInfo* draw : emissives)
    {
        llassert(draw->mGLTFMaterial);
        LLGLDisable cull_face(draw->mGLTFMaterial->mDoubleSided ? GL_CULL_FACE : 0);
        draw->mGLTFMaterial->bind(draw->mTexture);
        draw->mVertexBuffer->setBuffer();
        LLRenderPass::applyModelMatrix(*draw);
        draw->mVertexBuffer->drawRange(LLRender::TRIANGLES, draw->mStart, draw->mEnd, draw->mCount, draw->mOffset);
    }
}

void LLDrawPoolAlpha::renderRiggedEmissives(std::vector<LLDrawInfo*>& emissives)
{
    LLGLDepthTest depth(GL_TRUE, GL_FALSE); //disable depth writes since "emissive" is additive so sorting doesn't matter
    LLGLSLShader* shader = emissive_shader->mRiggedVariant;
    shader->bind();
    shader->uniform1f(LLShaderMgr::EMISSIVE_BRIGHTNESS, 1.f);

    const LLVOAvatar* lastAvatar = nullptr;
    U64 lastMeshId = 0;
    bool skipLastSkin = false;

    for (LLDrawInfo* draw : emissives)
    {
        LL_PROFILE_ZONE_NAMED_CATEGORY_DRAWPOOL("Emissives");

        if (uploadMatrixPalette(draw->mAvatar, draw->mSkinInfo, lastAvatar, lastMeshId, skipLastSkin))
        {
            bool tex_setup = TexSetup(draw, false);
            drawEmissive(draw);
            RestoreTexSetup(tex_setup);
        }
    }
}

void LLDrawPoolAlpha::renderRiggedPbrEmissives(std::vector<LLDrawInfo*>& emissives)
{
    LLGLDepthTest depth(GL_TRUE, GL_FALSE); //disable depth writes since "emissive" is additive so sorting doesn't matter
    pbr_emissive_shader->bind(true);

    const LLVOAvatar* lastAvatar = nullptr;
    U64 lastMeshId = 0;
    bool skipLastSkin = false;

    for (LLDrawInfo* draw : emissives)
    {
        if (!uploadMatrixPalette(draw->mAvatar, draw->mSkinInfo, lastAvatar, lastMeshId, skipLastSkin))
        { // failed to upload matrix palette, skip rendering
            continue;
        }

        LLGLDisable cull_face(draw->mGLTFMaterial->mDoubleSided ? GL_CULL_FACE : 0);
        draw->mGLTFMaterial->bind(draw->mTexture);
        draw->mVertexBuffer->setBuffer();
        LLRenderPass::applyModelMatrix(*draw);
        draw->mVertexBuffer->drawRange(LLRender::TRIANGLES, draw->mStart, draw->mEnd, draw->mCount, draw->mOffset);
    }
}

// <FS> void LLDrawPoolAlpha::renderAlpha(U32 mask, bool depth_only, bool rigged)
void LLDrawPoolAlpha::renderAlpha(U32 mask, bool depth_only, EAlphaStream stream, EAlphaOITPhase oit_phase)
// </FS>
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DRAWPOOL;
    // <FS> merged mode walks both streams; stream flips per group
    const bool merged = stream == EAlphaStream::INTERLEAVED;
    bool rigged = stream == EAlphaStream::RIGGED;
    // </FS>
    bool initialized_lighting = false;
    bool light_enabled = true;

    const LLVOAvatar* lastAvatar = nullptr;
    U64 lastMeshId = 0;
    const LLGLSLShader* lastAvatarShader = nullptr;
    bool skipLastSkin = false;

    // <FS> dual iterators for the merged walk
    LLCullResult::sg_iterator iter = nullptr;
    LLCullResult::sg_iterator iter_end = nullptr;
    LLCullResult::sg_iterator rigged_iter = nullptr;
    LLCullResult::sg_iterator rigged_end = nullptr;

    if (merged || rigged)
    {
        rigged_iter = gPipeline.beginRiggedAlphaGroups();
        rigged_end = gPipeline.endRiggedAlphaGroups();
    }

    if (merged || !rigged)
    {
        iter = gPipeline.beginAlphaGroups();
        iter_end = gPipeline.endAlphaGroups();
    }
    // </FS>

    LLEnvironment& env = LLEnvironment::instance();
    F32 water_height = env.getWaterHeight();

    bool above_water = getType() == LLDrawPool::POOL_ALPHA_POST_WATER;
    if (LLPipeline::sUnderWaterRender)
    {
        above_water = !above_water;
    }

    // <FS> depth-write conditions that apply to every group in this pass; in merged
    // mode, stamped rigged groups additionally write depth (see below)
    const bool write_depth_always = LLDrawPoolWater::sSkipScreenCopy ||
                                    LLPipeline::sImpostorRenderAlphaDepthPass ||
                                    getType() == LLDrawPoolAlpha::POOL_ALPHA_PRE_WATER;

    // merged mode: per-group depth-write guard, re-emplaced only when the
    // write state changes (one glDepthMask per run, not per group)
    std::optional<LLGLDepthTest> depth_state;
    bool depth_state_writes = false;
    // </FS>

    // <FS> for (LLCullResult::sg_iterator i = begin; i != end; ++i)
    while (iter != iter_end || rigged_iter != rigged_end)
    // </FS>
    {
        LL_PROFILE_ZONE_NAMED_CATEGORY_DRAWPOOL("renderAlpha - group");
        // <FS> in merged mode, take the farther of the two stream heads; an
        // ensemble's groups share one avatar depth, so each avatar drains
        // contiguously -- rigged run first (ties go rigged), then its unrigged
        // attachment groups, which composite over it
        if (merged)
        {
            if (rigged_iter == rigged_end)
            {
                rigged = false;
            }
            else if (iter == iter_end)
            {
                rigged = true;
            }
            else
            {
                F32 rigged_depth = (*rigged_iter)->mAvatarDepth;
                F32 world_depth = (*iter)->worldAlphaDepth();
                if (rigged_depth != world_depth)
                {
                    rigged = rigged_depth > world_depth;
                }
                else
                {
                    // equal depth: keep each avatar's ensemble contiguous. Its
                    // own rigged draws before its own unrigged; a plain world
                    // group composites over it (rigged first); two coincident
                    // avatars drain in the sorts' std::less identity order.
                    const LLVOAvatar* world_av = (*iter)->mAvatarp;
                    const LLVOAvatar* rigged_av = (*rigged_iter)->mAvatarp;
                    rigged = !rigged_av || !world_av || world_av == rigged_av ||
                             std::less<const LLVOAvatar*>()(rigged_av, world_av);
                }
            }
        }

        LLSpatialGroup* group = rigged ? *rigged_iter++ : *iter++;
        // </FS>
        llassert(group);
        llassert(group->getSpatialPartition());

        if (group->getSpatialPartition()->mRenderByGroup &&
            !group->isDead())
        {

            LLSpatialBridge* bridge = group->getSpatialPartition()->asBridge();
            const LLVector4a* ext = bridge ? bridge->getSpatialExtents() : group->getExtents();

            if (!LLPipeline::sRenderingHUDs) // ignore above/below water for HUD render
            {
                if (above_water)
                { // reject any spatial groups that have no part above water
                    if (ext[1].getF32ptr()[2] < water_height)
                    {
                        continue;
                    }
                }
                else
                { // reject any spatial groups that he no part below water
                    if (ext[0].getF32ptr()[2] > water_height)
                    {
                        continue;
                    }
                }
            }

            // <FS> merged mode: stamped rigged groups write depth so attachment-order
            // layering holds; an unstamped group has no defined position in the
            // rigged order and must not depth-reject geometry behind it (it
            // still blends). Non-merged passes keep the caller's depth state.
            if (merged)
            {
                bool write_depth = write_depth_always || (rigged && group->mAvatarp != nullptr);
                if (!depth_state || write_depth != depth_state_writes)
                {
                    depth_state.emplace(GL_TRUE, write_depth ? GL_TRUE : GL_FALSE);
                    depth_state_writes = write_depth;
                }
            }
            // </FS>

            static std::vector<LLDrawInfo*> emissives;
            static std::vector<LLDrawInfo*> rigged_emissives;
            static std::vector<LLDrawInfo*> pbr_emissives;
            static std::vector<LLDrawInfo*> pbr_rigged_emissives;

            emissives.resize(0);
            rigged_emissives.resize(0);
            pbr_emissives.resize(0);
            pbr_rigged_emissives.resize(0);

            bool is_particle_or_hud_particle = group->getSpatialPartition()->mPartitionType == LLViewerRegion::PARTITION_PARTICLE
                                                      || group->getSpatialPartition()->mPartitionType == LLViewerRegion::PARTITION_HUD_PARTICLE;

            // <FS> Vulkanstorm: particle partitions are always rendered by the stock alpha
            // path. They must never consume PPLL nodes / peel layers, even when their
            // batches happen to be fullbright.
            if ((oit_phase == ALPHA_OIT_CAPTURE ||
                 oit_phase == ALPHA_DEPTH_PEEL_SELECT ||
                 oit_phase == ALPHA_DEPTH_PEEL_REPLAY ||
                 oit_phase == ALPHA_DEPTH_PEEL_TAIL) &&
                is_particle_or_hud_particle)
            {
                continue;
            }
            // </FS>

            // <FS:LO> Dont suspend partical processing while particles are hidden, just skip over drawing them
            if(!(gPipeline.sRenderParticles) && (
                                                 group->getSpatialPartition()->mPartitionType == LLViewerRegion::PARTITION_PARTICLE ||
                                                 group->getSpatialPartition()->mPartitionType == LLViewerRegion::PARTITION_HUD_PARTICLE))
            {
                continue;
            }
            // </FS:LO>

            bool disable_cull = is_particle_or_hud_particle;
            LLGLDisable cull(disable_cull ? GL_CULL_FACE : 0);

            LLSpatialGroup::drawmap_elem_t& draw_info = rigged ? group->mDrawMap[LLRenderPass::PASS_ALPHA_RIGGED] : group->mDrawMap[LLRenderPass::PASS_ALPHA];

            for (LLSpatialGroup::drawmap_elem_t::iterator k = draw_info.begin(); k != draw_info.end(); ++k)
            {
                LLDrawInfo& params = **k;
                if ((bool)params.mAvatar != rigged)
                {
                    continue;
                }

                // <FS> Vulkanstorm: OIT phase filtering. Capture/peel passes skip residual
                // (particle or custom-blend) draws; the residual pass skips captured draws
                // but preserves their additive glow contribution.
                const bool custom_blend =
                    params.mBlendFuncSrc != LLRender::BF_SOURCE_ALPHA ||
                    params.mBlendFuncDst != LLRender::BF_ONE_MINUS_SOURCE_ALPHA;
                const bool residual_draw = is_particle_or_hud_particle || custom_blend;

                if (oit_phase == ALPHA_OIT_CAPTURE && residual_draw)
                {
                    continue;
                }

                if ((oit_phase == ALPHA_DEPTH_PEEL_SELECT ||
                     oit_phase == ALPHA_DEPTH_PEEL_REPLAY ||
                     oit_phase == ALPHA_DEPTH_PEEL_TAIL) && residual_draw)
                {
                    continue;
                }

                if (oit_phase == ALPHA_OIT_RESIDUAL && !residual_draw)
                {
                    // Standard source-over fullbright and lit alpha were both captured.
                    // Preserve their independent additive glow contribution here without
                    // repeating material setup or RGB blending.
                    if (!depth_only &&
                        getType() != LLDrawPool::POOL_ALPHA_PRE_WATER &&
                        params.mVertexBuffer->hasDataType(LLVertexBuffer::TYPE_EMISSIVE))
                    {
                        if (params.mAvatar)
                        {
                            (params.mGLTFMaterial.isNull() ? rigged_emissives : pbr_rigged_emissives).push_back(&params);
                        }
                        else
                        {
                            (params.mGLTFMaterial.isNull() ? emissives : pbr_emissives).push_back(&params);
                        }
                    }
                    continue;
                }
                // </FS>

                LL_PROFILE_ZONE_NAMED_CATEGORY_DRAWPOOL("ra - push batch");

                LLRenderPass::applyModelMatrix(params);

                LLMaterial* mat = NULL;
                LLGLTFMaterial *gltf_mat = params.mGLTFMaterial;

                LLGLDisable cull_face(gltf_mat && gltf_mat->mDoubleSided ? GL_CULL_FACE : 0);

                if (gltf_mat && gltf_mat->mAlphaMode == LLGLTFMaterial::ALPHA_MODE_BLEND)
                {
                    target_shader = pbr_shader;
                    if (params.mAvatar != nullptr)
                    {
                        target_shader = target_shader->mRiggedVariant;
                    }

                    // shader must be bound before LLGLTFMaterial::bind
                    if (current_shader != target_shader)
                    {
                        gPipeline.bindDeferredShaderFast(*target_shader);
                        // <FS> Vulkanstorm: texture units are global state; restore the
                        // selected-depth sampler on every shader transition
                        if (mAlphaPeelDepth)
                        {
                            target_shader->bindTexture(LLShaderMgr::ALPHA_PEEL_DEPTH,
                                                       mAlphaPeelDepth, false, LLTexUnit::TFO_POINT);
                        }
                        // </FS>
                    }

                    params.mGLTFMaterial->bind(params.mTexture);
                }
                else
                {
                    mat = LLPipeline::sRenderingHUDs ? nullptr : params.mMaterial;

                    if (params.mFullbright)
                    {
                        // Turn off lighting if it hasn't already been so.
                        if (light_enabled || !initialized_lighting)
                        {
                            initialized_lighting = true;
                            target_shader = fullbright_shader;

                            light_enabled = false;
                        }
                    }
                    // Turn on lighting if it isn't already.
                    else if (!light_enabled || !initialized_lighting)
                    {
                        initialized_lighting = true;
                        target_shader = simple_shader;
                        light_enabled = true;
                    }

                    if (LLPipeline::sRenderingHUDs)
                    {
                        target_shader = fullbright_shader;
                    }
                    else if (mat)
                    {
                        U32 mask = params.mShaderMask;

                        llassert(mask < LLMaterial::SHADER_COUNT);
                        target_shader = &(gDeferredMaterialProgram[mask]);
                    }
                    else if (!params.mFullbright)
                    {
                        target_shader = simple_shader;
                    }
                    else
                    {
                        target_shader = fullbright_shader;
                    }

                    if (params.mAvatar != nullptr)
                    {
                        llassert(target_shader->mRiggedVariant != nullptr);
                        target_shader = target_shader->mRiggedVariant;
                    }

                    if (current_shader != target_shader)
                    {// If we need shaders, and we're not ALREADY using the proper shader, then bind it
                    // (this way we won't rebind shaders unnecessarily).
                        gPipeline.bindDeferredShaderFast(*target_shader);

                        // <FS> Vulkanstorm: texture units are global state; restore the
                        // selected-depth sampler on every shader transition
                        if (mAlphaPeelDepth)
                        {
                            target_shader->bindTexture(LLShaderMgr::ALPHA_PEEL_DEPTH,
                                                       mAlphaPeelDepth, false, LLTexUnit::TFO_POINT);
                        }
                        // </FS>

                        if (params.mFullbright)
                        { // make sure the bind the exposure map for fullbright shaders so they can cancel out exposure
                            S32 channel = target_shader->enableTexture(LLShaderMgr::EXPOSURE_MAP);
                            if (channel > -1)
                            {
                                gGL.getTexUnit(channel)->bind(&gPipeline.mExposureMap);
                            }
                        }
                    }

                    LLVector4 spec_color(1, 1, 1, 1);
                    F32 env_intensity = 0.0f;
                    F32 brightness = 1.0f;

                    // We have a material.  Supply the appropriate data here.
                    if (mat)
                    {
                        spec_color = params.mSpecColor;
                        env_intensity = params.mEnvIntensity;
                        brightness = params.mFullbright ? 1.f : 0.f;
                    }

                    if (current_shader)
                    {
                        current_shader->uniform4f(LLShaderMgr::SPECULAR_COLOR, spec_color.mV[VRED], spec_color.mV[VGREEN], spec_color.mV[VBLUE], spec_color.mV[VALPHA]);
                        current_shader->uniform1f(LLShaderMgr::ENVIRONMENT_INTENSITY, env_intensity);
                        current_shader->uniform1f(LLShaderMgr::EMISSIVE_BRIGHTNESS, brightness);
                    }
                }

                if (params.mAvatar && !uploadMatrixPalette(params.mAvatar, params.mSkinInfo, lastAvatar, lastMeshId, lastAvatarShader, skipLastSkin))
                {
                    continue;
                }

                bool tex_setup = TexSetup(&params, (mat != nullptr));

                {
                    gGL.blendFunc((LLRender::eBlendFactor) params.mBlendFuncSrc, (LLRender::eBlendFactor) params.mBlendFuncDst, mAlphaSFactor, mAlphaDFactor);

                    bool reset_minimum_alpha = false;
                    if (!LLPipeline::sImpostorRender &&
                        params.mBlendFuncDst != LLRender::BF_SOURCE_ALPHA &&
                        params.mBlendFuncSrc != LLRender::BF_SOURCE_ALPHA)
                    { // this draw call has a custom blend function that may require rendering of "invisible" fragments
                        current_shader->setMinimumAlpha(0.f);
                        reset_minimum_alpha = true;
                    }

                    params.mVertexBuffer->setBuffer();
                    params.mVertexBuffer->drawRange(LLRender::TRIANGLES, params.mStart, params.mEnd, params.mCount, params.mOffset);
                    stop_glerror();

                    if (reset_minimum_alpha)
                    {
                        current_shader->setMinimumAlpha(MINIMUM_ALPHA);
                    }
                }

                // If this alpha mesh has glow, then draw it a second time to add the destination-alpha (=glow).  Interleaving these state-changing calls is expensive, but glow must be drawn Z-sorted with alpha.
                // <FS> Vulkanstorm: capture/peel passes emit no glow; the residual pass re-adds it
                if ((oit_phase == ALPHA_OIT_NONE || oit_phase == ALPHA_OIT_RESIDUAL) &&
                    getType() != LLDrawPool::POOL_ALPHA_PRE_WATER &&
                    params.mVertexBuffer->hasDataType(LLVertexBuffer::TYPE_EMISSIVE))
                {
                    if (params.mAvatar != nullptr)
                    {
                        if (params.mGLTFMaterial.isNull())
                        {
                            rigged_emissives.push_back(&params);
                        }
                        else
                        {
                            pbr_rigged_emissives.push_back(&params);
                        }
                    }
                    else
                    {
                        if (params.mGLTFMaterial.isNull())
                        {
                            emissives.push_back(&params);
                        }
                        else
                        {
                            pbr_emissives.push_back(&params);
                        }
                    }
                }

                if (tex_setup)
                {
                    gGL.getTexUnit(0)->activate();
                    gGL.matrixMode(LLRender::MM_TEXTURE);
                    gGL.loadIdentity();
                    gGL.matrixMode(LLRender::MM_MODELVIEW);
                }
            }

            // render emissive faces into alpha channel for bloom effects
            // <FS> Vulkanstorm: capture/peel passes emit no glow; residual re-adds it
            if (!depth_only &&
                (oit_phase == ALPHA_OIT_NONE || oit_phase == ALPHA_OIT_RESIDUAL))
            {
                gPipeline.enableLightsDynamic();

                // install glow-accumulating blend mode
                // don't touch color, add to alpha (glow)
                gGL.blendFunc(LLRender::BF_ZERO, LLRender::BF_ONE, LLRender::BF_ONE, LLRender::BF_ONE);

                bool rebind = false;
                LLGLSLShader* lastShader = current_shader;
                if (!emissives.empty())
                {
                    light_enabled = true;
                    renderEmissives(emissives);
                    rebind = true;
                }

                if (!pbr_emissives.empty())
                {
                    light_enabled = true;
                    renderPbrEmissives(pbr_emissives);
                    rebind = true;
                }

                if (!rigged_emissives.empty())
                {
                    light_enabled = true;
                    renderRiggedEmissives(rigged_emissives);
                    rebind = true;
                }

                if (!pbr_rigged_emissives.empty())
                {
                    light_enabled = true;
                    renderRiggedPbrEmissives(pbr_rigged_emissives);
                    rebind = true;
                }

                // restore our alpha blend mode
                gGL.blendFunc(mColorSFactor, mColorDFactor, mAlphaSFactor, mAlphaDFactor);

                if (lastShader && rebind)
                {
                    lastShader->bind();
                }
            }
        }
    }

    gGL.setSceneBlendType(LLRender::BT_ALPHA);

    LLVertexBuffer::unbind();

    if (!light_enabled)
    {
        gPipeline.enableLightsDynamic();
    }
}
