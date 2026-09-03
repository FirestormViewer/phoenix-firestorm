/**
 * @file llvkuitestscene.cpp
 * @brief Implementation of the deterministic GL<->Vulkan A/B test scene.
 *        See llvkuitestscene.h for the contract and coordinate convention.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, The Vulkanstorm Project
 * $/LicenseInfo$
 */

#include "linden_common.h"

#include "llvkuitestscene.h"

#include <cstdlib>

#include "v4color.h"     // LLColor4
#include "llvkui2d.h"    // LLVKUI2DSink
#include "llvkuiimage.h" // LLVKUIImage registry (GL-free)

namespace LLVKUITestScene
{

bool enabled()
{
    static const bool s_enabled = getenv("VULKANSTORM_UITEST") != nullptr;
    return s_enabled;
}

const Scene& scene()
{
    // Fixed layout. All items live in the top-left ~800x420 so the scene fits
    // the smallest practical login window. Keep in sync with NOTHING — this
    // table is the only definition; both backends read it.
    static const Scene s_scene = []()
    {
        Scene s;

        // --- Opaque solid rects (REPLACE blend) — byte-exact anchors (tol 0).
        s.rects.push_back({  32.f,  32.f, 232.f, 132.f,  1.f, 0.f, 0.f, 1.f, true }); // red
        s.rects.push_back({ 272.f,  32.f, 472.f, 132.f,  0.f, 1.f, 0.f, 1.f, true }); // green
        s.rects.push_back({ 512.f,  32.f, 712.f, 132.f,  0.f, 0.f, 1.f, 1.f, true }); // blue

        // --- Alpha-blended rect (ALPHA blend) over the rect bottoms — blend
        // math probe. Alpha compositing has an irreducible +/-1 hardware
        // rounding class (user-accepted parity policy: alpha = tol 1).
        s.rects.push_back({ 100.f,  90.f, 600.f, 190.f,  1.f, 1.f, 1.f, 0.5f, false });

        // --- 1px axis-aligned lines (REPLACE blend). Position/orientation
        // probes. Caveat: GL (diamond-exit) and Vulkan (perturbed Bresenham)
        // may differ by one pixel at the final endpoint — a known, bounded
        // rasterization-rule difference, not an orientation bug.
        s.lines.push_back({  32.f, 240.f, 712.f, 240.f,  1.f, 0.f, 0.f, 1.f }); // horizontal
        s.lines.push_back({ 760.f,  32.f, 760.f, 240.f,  0.f, 1.f, 0.f, 1.f }); // vertical

        // --- Textured images (ALPHA blend — the real UI pass state; white
        // tint so the texture shows unmodified). Images resolve by name on
        // both sides: GL via LLUIImageList/LLUIImage::draw, Vulkan via the
        // GL-free LLVKUIImage registry (same textures.xml source).
        const float W = 1.f;
        // Plain image at natural 16x16: directional flip canary, 1:1
        // texel:pixel so filtering cannot mask orientation (tol-0 expected).
        s.images.push_back({ "Arrow_Down",    64.f, 280.f,  80.f, 296.f,  W, W, W, 1.f });
        // Plain image stretched 64x64: UV/filter probe. May legitimately
        // differ if the two samplers' filters disagree on non-1:1 stretches
        // (GL UI textures are LINEAR; LLVKUIImage currently uploads NEAREST).
        s.images.push_back({ "Arrow_Down",    64.f, 320.f, 128.f, 384.f,  W, W, W, 1.f });
        // 9-slice (scale_inner) at natural 32x23: 1:1 anchor for slice math.
        s.images.push_back({ "PushButton_Off", 200.f, 280.f, 232.f, 303.f, W, W, W, 1.f });
        // 9-slice (scale_inner) stretched 64x69: 4px borders fixed, center
        // stretches — exposes slice-region mapping, orientation and filtering.
        s.images.push_back({ "PushButton_Off", 200.f, 320.f, 264.f, 389.f, W, W, W, 1.f });
        // 9-slice (scale_outer) stretched 32x32: covers the outer slice style.
        s.images.push_back({ "Arrow_Left_Unscaled", 320.f, 280.f, 352.f, 312.f, W, W, W, 1.f });
        // Gradient orientation canary: PushButton_Login (explicit scale region)
        // has a bright->dark vertical gradient in source. Rendered at native
        // size so the gradient direction directly exposes any content flip.
        s.images.push_back({ "PushButton_Login", 400.f, 280.f, 699.f, 344.f, W, W, W, 1.f });
        // Same image SCALED DOWN (the live login button is scaled) — reproduces
        // the gradient flip that only appears under scaling.
        s.images.push_back({ "PushButton_Login", 400.f, 360.f, 540.f, 396.f, W, W, W, 1.f });
        // Same image at the LIVE login button's exact target size (960x140),
        // strongly downscaled in X only — the case that flipped on screen.
        s.images.push_back({ "PushButton_Login", 400.f, 410.f, 1360.f, 550.f, W, W, W, 1.f });

        // --- The login-screen dropdown + checkbox images, at their natural
        // sizes (the exact textures those widgets use), so their rendered
        // size + content can be diffed GL vs VK directly.
        s.images.push_back({ "ComboButton_Off", 720.f, 280.f, 742.f, 303.f, W, W, W, 1.f }); // dropdown arrow button
        s.images.push_back({ "DropDown_Off",    760.f, 280.f, 882.f, 303.f, W, W, W, 1.f }); // start-loc dropdown
        s.images.push_back({ "Checkbox_On",     900.f, 280.f, 914.f, 295.f, W, W, W, 1.f }); // checkbox (checked)
        s.images.push_back({ "Checkbox_Off",    930.f, 280.f, 944.f, 295.f, W, W, W, 1.f }); // checkbox (unchecked)

        return s;
    }();
    return s_scene;
}

void emitVulkan()
{
    LLVKUI2D& sink = LLVKUI2DSink::get();
    if (!sink.isActive())
    {
        return;
    }

    const Scene& s = scene();

    // Emission order must match the GL emitter exactly (painter's order):
    // rects, then lines, then images.
    for (const Rect& r : s.rects)
    {
        sink.setBlend(r.opaque ? LLVKBlend::Replace : LLVKBlend::Alpha);
        sink.rect(r.l, r.t, r.r, r.b, r.cr, r.cg, r.cb, r.ca);
    }

    sink.setBlend(LLVKBlend::Replace);
    for (const Line& l : s.lines)
    {
        const float xy[4] = { l.x0, l.y0, l.x1, l.y1 };
        sink.lineStrip(xy, 2, l.cr, l.cg, l.cb, l.ca);
    }

    sink.setBlend(LLVKBlend::Alpha);
    for (const Image& img : s.images)
    {
        LLVKUIImage::draw(img.name, img.l, img.t, img.r, img.b,
                          LLColor4(img.cr, img.cg, img.cb, img.ca));
    }
}

} // namespace LLVKUITestScene
