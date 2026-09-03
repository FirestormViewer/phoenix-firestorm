/**
 * @file llvkuiimage.h
 * @brief LLVKUIImage — a GL-free, Vulkan-native UI-image registry (Phase 3 v2 M2).
 *
 * @details
 * An INDEPENDENT parallel of the viewer's LLUIImageList. It re-parses the same
 * textures.xml data (public llxml) and decodes each image's pixels with the
 * GL-free llimage decoders (PNG/TGA/J2C), then uploads them to the Vulkan
 * device as LLVKContext::Texture2D. It never touches LLImageGL / LLTexture /
 * gGL, and never executes any GL code — it only re-reads the same source data
 * and reproduces the RESULT (which pixels + clip/scale regions + filtering).
 *
 * The renderer resolves a UI image by name (the name LLUIImage already
 * exposes via getName()) to its decoded Vulkan texture + region state, then
 * emits 9-slice / solid / plain textured primitives into the LLVKUI2D sink.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, The Vulkanstorm Project
 * $/LicenseInfo$
 */

#ifndef LLVKUIIMAGE_H
#define LLVKUIIMAGE_H

#include <string>

#include <vulkan/vulkan.h>   // VkDescriptorSet

#include "llrect.h"          // LLRectf

class LLVKContext;
class LLColor4;

namespace LLVKUIImage
{
    // The slice mode from textures.xml (scale_type).
    enum class ScaleStyle { Inner, Outer };

    // Parse all skinned textures.xml files (LLDir::findSkinnedFilenames /
    // LLDir::TEXTURES / ALL_SKINS), build the name->record map, decode + upload
    // every referenced image to the Vulkan device. GL-free. Call once after
    // the Vulkan device + 2D pipeline exist (llvksession::start / first UI
    // frame). Safe to call again (no-op once initialized).
    void init(LLVKContext* ctx);

    // True after a successful init().
    bool ready();

    // Decoded intrinsic dimensions for widgets whose GL contract draws an
    // image unscaled at its native size (checkboxes and combo arrows).
    bool getSize(const std::string& name, int& width, int& height);

    // Emit the image (by name) into the LLVKUI2D sink across the given
    // TOP-LEFT-origin sink rect, honoring the image's clip + 9-slice scale
    // region and modulating by color (per-component texel*vertexColor). If the
    // name is unknown or the image failed to load, emits a solid quad tinted
    // by color so the widget still gets *some* fill (never leaves a hole).
    void draw(const std::string& name,
              float left, float top, float right, float bottom,
              const LLColor4& color);

    // Same but draws only the border ring (drawBorder), border_width px.
    void drawBorder(const std::string& name,
                    float left, float top, float right, float bottom,
                    const LLColor4& color, int border_width);

    // Same but ignores the texture (drawSolid): fills with color only.
    void drawSolid(const std::string& name,
                   float left, float top, float right, float bottom,
                   const LLColor4& color);

    // Release all uploaded textures (on session stop).
    void shutdown();
}

#endif // LLVKUIIMAGE_H
