/**
 * @file llvkuimaps.cpp
 * @brief Vulkan UI hooks for LLNetMap (minimap) and LLWorldMapView (world map).
 *
 * @details
 * The greenfield Vulkan UI renderer (indra/llvulkan/llvkuirender.cpp) walks
 * the LLView tree and never executes the widgets' GL draw() code. The hooks
 * registered here reproduce the RESULT of LLNetMap::draw() and
 * LLWorldMapView::draw() — same draw order, same colors — from the GL-free
 * Vk* state accessors on the views:
 *
 *  - prepare hooks (pre-render-pass) drive the views' prepareVkDraw() (the
 *    pan/zoom animation + CPU map-layer refreshes + picking that draw() used
 *    to mutate per frame) and upload all dynamic map textures through
 *    LLVKUIImage::updateDynamic (queue work is not allowed inside the
 *    render pass).
 *  - render hooks emit the background, region tiles, object/parcel overlay
 *    layers, avatar dots, markers, rings and frustum into the LLVKUI2D sink.
 *
 * Coordinate frames: the views compute in GL bottom-left-origin local space;
 * the sink is top-left-origin (y_tl = (device_height/ui_scale_y) - y_gl).
 * Textured images (LLVKUIImage::draw/drawDynamic) take top-left-origin rects
 * in UI units directly; LLVKUI2DSink::rawTris takes UI units and applies the
 * UI scale at emit time.
 *
 * GL-free: no llgl / gGL / LLRender usage.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, The Vulkanstorm Project
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "llvkuimaps.h"

#include <cmath>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "indra_constants.h"     // REGION_WIDTH_METERS
#include "llformat.h"            // llformat
#include "llmath.h"
#include "llrect.h"
#include "llstring.h"            // utf8string_to_wstring
#include "lltimer.h"
#include "llui.h"                // LLUI::getScaleFactor / getMousePositionLocal
#include "lluicolortable.h"
#include "llimage.h"             // LLImageRaw

#include "llagent.h"
#include "llagentcamera.h"
#include "llcallingcard.h"       // LLAvatarTracker
#include "llregionhandle.h"      // from_region_handle
#include "llsurface.h"           // LLSurface::getSTexture
#include "lltracker.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"     // LLCachedControl / gSavedSettings
#include "llviewernetwork.h"     // LLGridManager
#include "llviewerregion.h"
#include "llviewertexture.h"
#include "llviewerwindow.h"      // gViewerWindow (scissor extent)
#include "llworld.h"
#include "llworldmap.h"
#include "llworldmipmap.h"

#include "llnetmap.h"
#include "llworldmapview.h"

#include "lfsimfeaturehandler.h"
#include "rlvactions.h"
#include "rlvcommon.h"

#include "llvkuirender.h"
#include "llvkuiimage.h"
#include "llvklegacytext.h"
#include "llvkui2d.h"

// ---------------------------------------------------------------------------
// Dynamic map textures
// ---------------------------------------------------------------------------

namespace
{
    struct VkDynEntry
    {
        const U8* data = nullptr;   // last uploaded source buffer (change detection)
        U64       serial = 0;       // serial handed to LLVKUIImage::updateDynamic
        S32       age = 0;          // frames since last forced refresh
        bool      ok = false;       // last upload succeeded (bounds the draw)
    };
    std::map<std::string, VkDynEntry> s_dyn;
}

// Bridge for view-owned CPU layers (netmap object/parcel images): the view
// bumps a serial each time it re-renders the pixels; updateDynamic suppresses
// redundant uploads.
bool vk_upload_raw(const std::string& key, const LLImageRaw* raw, uint64_t serial)
{
    const bool ok = LLVKUIImage::ready()
        && raw && raw->getData()
        && raw->getWidth() > 0 && raw->getHeight() > 0 && raw->getComponents() >= 3
        && LLVKUIImage::updateDynamic(key, raw->getData(), raw->getWidth(),
                                      raw->getHeight(), (int)raw->getComponents(),
                                      /*bgra=*/false, serial);
    s_dyn[key].ok = ok;
    return ok;
}

namespace
{
    bool vk_dyn_ok(const std::string& key)
    {
        auto it = s_dyn.find(key);
        return it != s_dyn.end() && it->second.ok;
    }

    // Upload an LLViewerFetchedTexture's CPU pixels, keyed by the caller
    // (texture UUID). The raw-image accessors live on LLViewerFetchedTexture
    // (llviewertexture.h), not the LLViewerTexture base; map tiles, land
    // textures and sale overlays are all fetched textures at runtime. Reads
    // the live raw image when valid, else the saved raw image;
    // heartbeat_frames > 0 forces a re-upload at that cadence to catch
    // in-place pixel updates under a stable buffer pointer.
    bool vk_upload_texture(const std::string& key, LLViewerFetchedTexture* tex, S32 heartbeat_frames)
    {
        if (!tex)
        {
            return false;
        }
        const LLImageRaw* raw = tex->isRawImageValid()
            ? (const LLImageRaw*)tex->getRawImage()
            : tex->getSavedRawImage();
        if (!raw || !raw->getData() || raw->getComponents() < 3)
        {
            s_dyn[key].ok = false;
            return false;
        }
        VkDynEntry& e = s_dyn[key];
        if (e.data != raw->getData() || (heartbeat_frames > 0 && ++e.age >= heartbeat_frames))
        {
            e.data = raw->getData();
            e.age = 0;
            ++e.serial;
        }
        e.ok = vk_upload_raw(key, raw, e.serial);
        return e.ok;
    }

    // -----------------------------------------------------------------------
    // Geometry helpers. All positions arrive in GL bottom-left-origin screen
    // space (UI units); conversion to the sink's top-left space happens at
    // emission: y_tl = (device_height/ui_scale_y) - y_gl.
    // -----------------------------------------------------------------------

    void vk_push_clip(const LLRect& gl_screen, unsigned device_height, float ui_scale_y)
    {
        // Mirrors LLVKUIRenderInternal::applyClip's conversion. NOTE: the sink
        // exposes no scissor getter, so this REPLACES (not intersects) any
        // ancestor scissor; map views are not hosted in clipped containers
        // (scroll lists) in practice. Cleared again at the end of the hook so
        // the walker draws children unclipped, matching the GL path where the
        // LLLocalClipRect scope ends before LLView::draw().
        const F32 ui_scale_x = LLUI::getScaleFactor().mV[VX];
        const S32 dev_w = gViewerWindow ? gViewerWindow->getWindowWidthRaw() : 0;
        const F32 ui_h = (F32)device_height / ui_scale_y;
        const S32 sx = llclamp(ll_round((F32)gl_screen.mLeft * ui_scale_x), 0, dev_w);
        const S32 sy = llclamp(ll_round((ui_h - (F32)gl_screen.mTop) * ui_scale_y), 0, (S32)device_height);
        const S32 sr = llclamp(ll_round((F32)gl_screen.mRight * ui_scale_x), sx, dev_w);
        const S32 sb = llclamp(ll_round((ui_h - (F32)gl_screen.mBottom) * ui_scale_y), sy, (S32)device_height);
        LLVKUI2DSink::get().setScissor(sx, sy, sr - sx, sb - sy);
    }

    void vk_pop_clip()
    {
        LLVKUI2DSink::get().clearScissor();
    }

    void vk_solid_rect_gl(F32 ui_h, F32 gl_left, F32 gl_bottom, F32 gl_right, F32 gl_top, const LLColor4& color)
    {
        LLVKUI2DSink::get().setTexture(VK_NULL_HANDLE);
        LLVKUI2DSink::get().rect(gl_left, ui_h - gl_top, gl_right, ui_h - gl_bottom,
                                 color.mV[VRED], color.mV[VGREEN], color.mV[VBLUE], color.mV[VALPHA]);
    }

    // gl_washer_segment_2d equivalent: annulus sector [inner_r, outer_r] from
    // start..end radians (both colors per-edge like GL), triangulated on the
    // CPU. angle_offset rotates the whole sector about (cx, cy).
    void vk_emit_washer(F32 ui_h, F32 cx, F32 cy, F32 inner_r, F32 outer_r,
                        F32 start_theta, F32 end_theta, S32 steps, F32 angle_offset,
                        const LLColor4& inner_color, const LLColor4& outer_color)
    {
        if (steps < 1 || outer_r <= 0.f || outer_r <= inner_r || inner_r < 0.f)
        {
            return;
        }
        std::vector<F32> xy;
        std::vector<F32> rgba;
        xy.reserve((size_t)steps * 12);
        rgba.reserve((size_t)steps * 24);
        auto push = [&](F32 gl_x, F32 gl_y, const LLColor4& c)
        {
            xy.push_back(gl_x);
            xy.push_back(ui_h - gl_y);
            rgba.push_back(c.mV[VRED]); rgba.push_back(c.mV[VGREEN]);
            rgba.push_back(c.mV[VBLUE]); rgba.push_back(c.mV[VALPHA]);
        };
        for (S32 i = 0; i < steps; ++i)
        {
            const F32 t0 = start_theta + (end_theta - start_theta) * (F32)i / (F32)steps + angle_offset;
            const F32 t1 = start_theta + (end_theta - start_theta) * (F32)(i + 1) / (F32)steps + angle_offset;
            const F32 o0x = cx + outer_r * cosf(t0), o0y = cy + outer_r * sinf(t0);
            const F32 o1x = cx + outer_r * cosf(t1), o1y = cy + outer_r * sinf(t1);
            const F32 i0x = cx + inner_r * cosf(t0), i0y = cy + inner_r * sinf(t0);
            const F32 i1x = cx + inner_r * cosf(t1), i1y = cy + inner_r * sinf(t1);
            push(i0x, i0y, inner_color); push(o0x, o0y, outer_color); push(o1x, o1y, outer_color);
            push(i0x, i0y, inner_color); push(o1x, o1y, outer_color); push(i1x, i1y, inner_color);
        }
        LLVKUI2DSink::get().rawTris(xy.data(), rgba.data(), (int)(xy.size() / 2));
    }

    // gl_circle_2d(filled=true) equivalent.
    void vk_emit_circle(F32 ui_h, F32 cx, F32 cy, F32 radius, const LLColor4& color, S32 steps)
    {
        if (radius <= 0.f || steps < 3)
        {
            return;
        }
        std::vector<F32> xy;
        std::vector<F32> rgba;
        xy.reserve((size_t)steps * 6);
        rgba.reserve((size_t)steps * 12);
        auto push = [&](F32 gl_x, F32 gl_y)
        {
            xy.push_back(gl_x);
            xy.push_back(ui_h - gl_y);
            rgba.push_back(color.mV[VRED]); rgba.push_back(color.mV[VGREEN]);
            rgba.push_back(color.mV[VBLUE]); rgba.push_back(color.mV[VALPHA]);
        };
        for (S32 i = 0; i < steps; ++i)
        {
            const F32 t0 = F_TWO_PI * (F32)i / (F32)steps;
            const F32 t1 = F_TWO_PI * (F32)(i + 1) / (F32)steps;
            push(cx, cy);
            push(cx + radius * cosf(t0), cy + radius * sinf(t0));
            push(cx + radius * cosf(t1), cy + radius * sinf(t1));
        }
        LLVKUI2DSink::get().rawTris(xy.data(), rgba.data(), (int)(xy.size() / 2));
    }

    // gl_ring(radius, width, color, color, steps, false) equivalent: a flat
    // annulus spanning [radius - width, radius].
    void vk_emit_ring(F32 ui_h, F32 cx, F32 cy, F32 radius, F32 width, const LLColor4& color, S32 steps)
    {
        vk_emit_washer(ui_h, cx, cy, radius - width, radius, 0.f, F_TWO_PI, steps, 0.f, color, color);
    }

    // Thick line segment (GL line width N) as a quad.
    void vk_emit_segment(F32 ui_h, F32 x1, F32 y1, F32 x2, F32 y2, F32 width, const LLColor4& color)
    {
        const F32 dx = x2 - x1, dy = y2 - y1;
        const F32 len = sqrtf(dx * dx + dy * dy);
        if (len <= 0.f)
        {
            return;
        }
        const F32 px = -dy / len * width * 0.5f;
        const F32 py = dx / len * width * 0.5f;
        const F32 xy[12] = { x1 - px, y1 - py,  x2 - px, y2 - py,  x2 + px, y2 + py,
                             x1 - px, y1 - py,  x2 + px, y2 + py,  x1 + px, y1 + py };
        F32 xy_tl[12];
        for (int i = 0; i < 6; ++i)
        {
            xy_tl[i * 2] = xy[i * 2];
            xy_tl[i * 2 + 1] = ui_h - xy[i * 2 + 1];
        }
        F32 rgba[24];
        for (int i = 0; i < 6; ++i)
        {
            rgba[i * 4 + 0] = color.mV[VRED]; rgba[i * 4 + 1] = color.mV[VGREEN];
            rgba[i * 4 + 2] = color.mV[VBLUE]; rgba[i * 4 + 3] = color.mV[VALPHA];
        }
        LLVKUI2DSink::get().rawTris(xy_tl, rgba, 6);
    }

    // Tracking arrow. GL draws the rotated "direction_arrow.tga" image; the
    // Vulkan image API has no rotated-draw entry point, so emit a solid arrow
    // triangle in the same color, rotated about the image square's center.
    void vk_emit_arrow(F32 ui_h, F32 x, F32 y, F32 size, F32 angle, const LLColor4& color)
    {
        const F32 cx = x + size * 0.5f;
        const F32 cy = y + size * 0.5f;
        const F32 dx = cosf(angle), dy = sinf(angle);
        const F32 px = -dy, py = dx;
        const F32 tip_x = cx + dx * size * 0.55f, tip_y = cy + dy * size * 0.55f;
        const F32 b1x = cx - dx * size * 0.30f + px * size * 0.35f;
        const F32 b1y = cy - dy * size * 0.30f + py * size * 0.35f;
        const F32 b2x = cx - dx * size * 0.30f - px * size * 0.35f;
        const F32 b2y = cy - dy * size * 0.30f - py * size * 0.35f;
        const F32 xy[6] = { tip_x, ui_h - tip_y, b1x, ui_h - b1y, b2x, ui_h - b2y };
        F32 rgba[12];
        for (int i = 0; i < 3; ++i)
        {
            rgba[i * 4 + 0] = color.mV[VRED]; rgba[i * 4 + 1] = color.mV[VGREEN];
            rgba[i * 4 + 2] = color.mV[VBLUE]; rgba[i * 4 + 3] = color.mV[VALPHA];
        }
        LLVKUI2DSink::get().rawTris(xy, rgba, 3);
    }

    // LLWorldMapView::drawTrackingCircle()'s math (arc that opens toward an
    // off-screen target), minus the GL emission.
    void vk_tracking_circle_params(const LLRect& rect, S32 x, S32 y, S32 min_thickness, S32 overlap,
                                   F32& inner_radius, F32& outer_radius,
                                   F32& start_theta, F32& end_theta)
    {
        start_theta = 0.f;
        end_theta = F_TWO_PI;
        F32 x_delta = 0.f;
        F32 y_delta = 0.f;

        if (x < 0)
        {
            x_delta = 0.f - (F32)x;
            start_theta = F_PI + F_PI_BY_TWO;
            end_theta = F_TWO_PI + F_PI_BY_TWO;
        }
        else if (x > rect.getWidth())
        {
            x_delta = (F32)(x - rect.getWidth());
            start_theta = F_PI_BY_TWO;
            end_theta = F_PI + F_PI_BY_TWO;
        }

        if (y < 0)
        {
            y_delta = 0.f - (F32)y;
            if (x < 0)
            {
                start_theta = 0.f;
                end_theta = F_PI_BY_TWO;
            }
            else if (x > rect.getWidth())
            {
                start_theta = F_PI_BY_TWO;
                end_theta = F_PI;
            }
            else
            {
                start_theta = 0.f;
                end_theta = F_PI;
            }
        }
        else if (y > rect.getHeight())
        {
            y_delta = (F32)(y - rect.getHeight());
            if (x < 0)
            {
                start_theta = F_PI + F_PI_BY_TWO;
                end_theta = F_TWO_PI;
            }
            else if (x > rect.getWidth())
            {
                start_theta = F_PI;
                end_theta = F_PI + F_PI_BY_TWO;
            }
            else
            {
                start_theta = F_PI;
                end_theta = F_TWO_PI;
            }
        }

        F32 distance = sqrtf(x_delta * x_delta + y_delta * y_delta);
        distance = llmax(0.1f, distance);

        outer_radius = distance + (1.f + (9.f * sqrtf(x_delta * y_delta) / distance)) * (F32)overlap;
        inner_radius = outer_radius - (F32)min_thickness;

        F32 angle_adjust_x = asin(x_delta / outer_radius);
        F32 angle_adjust_y = asin(y_delta / outer_radius);

        if (angle_adjust_x)
        {
            if (angle_adjust_y)
            {
                F32 angle_adjust = llmin(angle_adjust_x, angle_adjust_y);
                start_theta += angle_adjust;
                end_theta -= angle_adjust;
            }
            else
            {
                start_theta += angle_adjust_x;
                end_theta -= angle_adjust_x;
            }
        }
        else if (angle_adjust_y)
        {
            start_theta += angle_adjust_y;
            end_theta -= angle_adjust_y;
        }
    }

    void vk_draw_tracking_circle(const LLRect& view_screen, const LLRect& local_rect, S32 x, S32 y,
                                 const LLColor4& color, S32 min_thickness, S32 overlap, F32 ui_h)
    {
        F32 inner_radius, outer_radius, start_theta, end_theta;
        vk_tracking_circle_params(local_rect, x, y, min_thickness, overlap,
                                  inner_radius, outer_radius, start_theta, end_theta);
        vk_emit_washer(ui_h, (F32)view_screen.mLeft + x, (F32)view_screen.mBottom + y,
                       inner_radius, outer_radius, start_theta, end_theta, 40, 0.f, color, color);
    }

    // LLWorldMapView::drawTrackingArrow()'s clamp math, minus the GL emission.
    void vk_tracking_arrow_params(const LLRect& rect, S32 x, S32 y, S32 arrow_size,
                                  S32& out_x, S32& out_y, F32& out_angle)
    {
        F32 x_center = (F32)rect.getWidth() / 2.f;
        F32 y_center = (F32)rect.getHeight() / 2.f;

        F32 x_clamped = (F32)llclamp( x, 0, rect.getWidth() - arrow_size );
        F32 y_clamped = (F32)llclamp( y, 0, rect.getHeight() - arrow_size );

        F32 slope = (F32)(y - y_center) / (F32)(x - x_center);
        F32 window_ratio = (F32)rect.getHeight() / (F32)rect.getWidth();

        if (llabs(slope) > window_ratio && y_clamped != (F32)y)
        {
            x_clamped = (y_clamped - y_center) / slope + x_center;
            x_clamped = llclamp(x_clamped , 0.f, (F32)(rect.getWidth() - arrow_size) );
        }
        else if (x_clamped != (F32)x)
        {
            y_clamped = (x_clamped - x_center) * slope + y_center;
            y_clamped = llclamp( y_clamped, 0.f, (F32)(rect.getHeight() - arrow_size) );
        }

        S32 half_arrow_size = (S32) (0.5f * arrow_size);
        out_angle = atan2( y + half_arrow_size - y_center, x + half_arrow_size - x_center);
        out_x = llfloor(x_clamped);
        out_y = llfloor(y_clamped);
    }

    void vk_draw_tracking_arrow(const LLRect& view_screen, const LLRect& local_rect, S32 x, S32 y,
                                const LLColor4& color, S32 arrow_size, F32 ui_h)
    {
        S32 ax, ay;
        F32 angle;
        vk_tracking_arrow_params(local_rect, x, y, arrow_size, ax, ay, angle);
        vk_emit_arrow(ui_h, (F32)view_screen.mLeft + ax, (F32)view_screen.mBottom + ay,
                      (F32)arrow_size, angle, color);
    }

    // drawDot() (llworldmapview.cpp): the tracking circle image at native
    // size, or a V chevron when the target is far above/below.
    void vk_draw_dot(F32 x_local, F32 y_local, const LLColor4& color, F32 relative_z, F32 dot_radius,
                     const std::string& image_name, const LLRect& view_screen,
                     unsigned device_height, float ui_scale_y, F32 ui_h)
    {
        const F32 HEIGHT_THRESHOLD = 7.f;
        if (-HEIGHT_THRESHOLD <= relative_z && relative_z <= HEIGHT_THRESHOLD)
        {
            int w = 16, h = 16;
            LLVKUIImage::getSize(image_name, w, h);
            const S32 cx = ll_round(view_screen.mLeft + x_local);
            const S32 cy = ll_round(view_screen.mBottom + y_local);
            LLRect r(cx - w / 2, cy - h / 2 + h, cx - w / 2 + w, cy - h / 2);
            LLVKUIRender::emitScreenRect(r, device_height, ui_scale_y, image_name, color);
        }
        else
        {
            // V indicator (GL used 3px lines)
            const F32 left =   x_local - dot_radius;
            const F32 right =  x_local + dot_radius;
            const F32 center = (left + right) * 0.5f;
            const F32 top =    y_local + dot_radius;
            const F32 bottom = y_local - dot_radius;
            const F32 point = relative_z > HEIGHT_THRESHOLD ? top : bottom;
            const F32 back = relative_z > HEIGHT_THRESHOLD ? bottom : top;
            const F32 sx = (F32)view_screen.mLeft, sy = (F32)view_screen.mBottom;
            vk_emit_segment(ui_h, sx + left, sy + back, sx + center, sy + point, 3.f, color);
            vk_emit_segment(ui_h, sx + center, sy + point, sx + right, sy + back, 3.f, color);
        }
    }

    // LLWorldMapView::drawAvatar(): the level/above/below/unknown dot image
    // scaled to the dot diameter.
    void vk_draw_avatar_dot(F32 x_local, F32 y_local, const LLColor4& color, F32 relative_z,
                            F32 dot_radius, bool unknown_relative_z, const LLRect& view_screen,
                            unsigned device_height, float ui_scale_y)
    {
        const F32 HEIGHT_THRESHOLD = 7.f;
        const char* image_name = "map_avatar_32.tga";       // sAvatarLevelImage
        if (unknown_relative_z && llabs(relative_z) > HEIGHT_THRESHOLD)
        {
            image_name = "map_avatar_unknown.tga";          // sAvatarUnknownImage
        }
        else if (relative_z < -HEIGHT_THRESHOLD)
        {
            image_name = "map_avatar_below_32.tga";         // sAvatarBelowImage
        }
        else if (relative_z > HEIGHT_THRESHOLD)
        {
            image_name = "map_avatar_above_32.tga";         // sAvatarAboveImage
        }
        const S32 dot_width = ll_round(dot_radius * 2.f);
        const S32 rx = ll_round(view_screen.mLeft + x_local - dot_radius);
        const S32 ry = ll_round(view_screen.mBottom + y_local - dot_radius);
        LLRect r(rx, ry + dot_width, rx + dot_width, ry);
        LLVKUIRender::emitScreenRect(r, device_height, ui_scale_y, std::string(image_name), color);
    }

    // LLWorldMapView::drawImage(): a named image at native size centered on a
    // global position.
    void vk_draw_image_at(LLWorldMapView* map, const LLVector3d& global_pos, const std::string& image_name,
                          const LLColor4& color, const LLRect& view_screen,
                          unsigned device_height, float ui_scale_y, F32 y_offset = 0.f)
    {
        int w = 16, h = 16;
        LLVKUIImage::getSize(image_name, w, h);
        const LLVector3 pos_map = map->vkGlobalPosToView(global_pos);
        const S32 rx = ll_round(view_screen.mLeft + pos_map.mV[VX] - w / 2.f);
        const S32 ry = ll_round(view_screen.mBottom + pos_map.mV[VY] + y_offset - h / 2.f);
        LLRect r(rx, ry + h, rx + w, ry);
        LLVKUIRender::emitScreenRect(r, device_height, ui_scale_y, image_name, color);
    }

    // Emit a dynamic-texture quad given in map-frame local coords (origin at
    // the map center), rotated about the map center — the CPU equivalent of
    // LLNetMap::draw()'s gGL.translatef(center) + gGL.rotatef(rotation) block.
    // Source rows are GL-ordered (row 0 = image bottom), like the GL draws.
    void vk_emit_map_quad(const std::string& key, bool solid_fallback,
                          F32 local_left, F32 local_bottom, F32 local_right, F32 local_top,
                          F32 center_x, F32 center_y, F32 rotation,
                          F32 ui_h, const LLColor4& color)
    {
        const bool uploaded = !key.empty() && vk_dyn_ok(key);
        if (rotation == 0.f)
        {
            if (uploaded)
            {
                LLVKUIImage::drawDynamic(key,
                                         center_x + local_left, ui_h - (center_y + local_top),
                                         center_x + local_right, ui_h - (center_y + local_bottom),
                                         1.f, 1.f, /*coords_opengl=*/true, color);
            }
            else if (solid_fallback)
            {
                vk_solid_rect_gl(ui_h, center_x + local_left, center_y + local_bottom,
                                 center_x + local_right, center_y + local_top, color);
            }
            return;
        }

        // Rotate the four corners (GL frame, CCW) about the map center.
        const F32 c = cosf(rotation), s = sinf(rotation);
        const F32 lx[4] = { local_left, local_right, local_right, local_left };
        const F32 ly[4] = { local_bottom, local_bottom, local_top, local_top };
        const F32 tu[4] = { 0.f, 1.f, 1.f, 0.f };
        const F32 tv[4] = { 0.f, 0.f, 1.f, 1.f };
        F32 sx[4], sy[4];
        for (int i = 0; i < 4; ++i)
        {
            sx[i] = lx[i] * c - ly[i] * s + center_x;
            sy[i] = ui_h - (lx[i] * s + ly[i] * c + center_y);
        }
        // Triangles (SW, SE, NE) + (SW, NE, NW).
        const int idx[6] = { 0, 1, 2, 0, 2, 3 };
        F32 xy[12], uv[12], rgba[24];
        for (int i = 0; i < 6; ++i)
        {
            xy[i * 2] = sx[idx[i]];
            xy[i * 2 + 1] = sy[idx[i]];
            uv[i * 2] = tu[idx[i]];
            uv[i * 2 + 1] = tv[idx[i]];
            rgba[i * 4 + 0] = color.mV[VRED]; rgba[i * 4 + 1] = color.mV[VGREEN];
            rgba[i * 4 + 2] = color.mV[VBLUE]; rgba[i * 4 + 3] = color.mV[VALPHA];
        }
        if (uploaded)
        {
            // Bind the dynamic texture for the following batch: the sink reads
            // texture state at flush time, so a degenerate drawDynamic quad
            // (zero area, rasterizes nothing) followed by our pre-transformed
            // triangles flushes them in one run with this texture bound.
            LLVKUIImage::drawDynamic(key, 0.f, 0.f, 0.f, 0.f, 1.f, 1.f, true, LLColor4(0.f, 0.f, 0.f, 0.f));
            LLVKUI2DSink::get().texturedBatchPreTransformed(xy, uv, rgba, 6);
        }
        else if (solid_fallback)
        {
            LLVKUI2DSink::get().setTexture(VK_NULL_HANDLE);
            LLVKUI2DSink::get().rawTris(xy, rgba, 6);
        }
    }

    std::string vk_netmap_key(const LLNetMap* netmap, const char* layer)
    {
        return std::string("netmap:") + layer + ":" + std::to_string((uintptr_t)netmap);
    }

    // -----------------------------------------------------------------------
    // LLNetMap (minimap)
    // -----------------------------------------------------------------------

    void vk_prepare_net_map(const LLView* view, LLVKContext*)
    {
        LLNetMap* netmap = const_cast<LLNetMap*>(static_cast<const LLNetMap*>(view));
        netmap->prepareVkDraw();

        if (!LLWorld::instanceExists() || !LLWorld::getInstance()->getAllowMinimap())
        {
            return;
        }

        // CPU overlay layers owned by the view (serial-driven uploads)
        static LLCachedControl<bool> show_objects(gSavedSettings, "MiniMapObjects");
        if (show_objects)
        {
            vk_upload_raw(vk_netmap_key(netmap, "object"), netmap->getVkObjectRawImage(),
                          netmap->getVkObjectSerial());
        }
        static LLCachedControl<bool> show_property_lines(gSavedSettings, "MiniMapShowPropertyLines");
        if (show_property_lines)
        {
            vk_upload_raw(vk_netmap_key(netmap, "parcel"), netmap->getVkParcelRawImage(),
                          netmap->getVkParcelSerial());
        }

        // Region background tiles (LLViewerTexture-based; keyed by texture ID)
        if (!LLGridManager::getInstance()->isInSecondLife())
        {
            if (gAgent.getTeleportState() == LLAgent::TELEPORT_NONE)
            {
                for (LLViewerRegion* regionp : LLWorld::getInstance()->getRegionList())
                {
                    const LLViewerRegion::tex_matrix_t& tiles(regionp->getWorldMapTiles());
                    for (const LLPointer<LLViewerTexture>& tile_ptr : tiles)
                    {
                        if (tile_ptr.isNull())
                        {
                            continue;
                        }
                        LLPointer<LLViewerTexture> tile = tile_ptr; // non-const copy, like draw()
                        tile->setBoostLevel(LLViewerTexture::BOOST_MAP_VISIBLE);
                        vk_upload_texture("netmap:tile:" + tile->getID().asString(),
                                          dynamic_cast<LLViewerFetchedTexture*>(tile.get()), 600);
                    }
                }
            }
        }
        else
        {
            for (LLViewerRegion* regionp : LLWorld::getInstance()->getRegionList())
            {
                LLViewerTexture* land = regionp->getLand().getSTexture();
                if (land)
                {
                    vk_upload_texture("netmap:land:" + land->getID().asString(),
                                      dynamic_cast<LLViewerFetchedTexture*>(land), 45);
                }
            }
        }
    }

    void vk_netmap_draw_tracking(LLNetMap* netmap, const LLRect& screen, unsigned device_height,
                                 float ui_scale_y, F32 ui_h, const LLVector3d& pos_global,
                                 const LLColor4& color, bool draw_arrow = true)
    {
        LLVector3 pos_local = netmap->vkGlobalPosToView(pos_global);
        const LLRect& rect = netmap->getRect();
        if( (pos_local.mV[VX] < 0) ||
            (pos_local.mV[VY] < 0) ||
            (pos_local.mV[VX] >= rect.getWidth()) ||
            (pos_local.mV[VY] >= rect.getHeight()) )
        {
            if (draw_arrow)
            {
                const S32 x = ll_round( pos_local.mV[VX] );
                const S32 y = ll_round( pos_local.mV[VY] );
                vk_draw_tracking_circle(screen, rect, x, y, color, 1, 10, ui_h);
                vk_draw_tracking_arrow(screen, rect, x, y, color, DEFAULT_TRACKING_ARROW_SIZE, ui_h);
            }
        }
        else
        {
            vk_draw_dot(pos_local.mV[VX], pos_local.mV[VY], color, pos_local.mV[VZ], 5.f,
                        "map_track_16.tga", screen, device_height, ui_scale_y, ui_h);
        }
    }

    void vk_render_net_map(const LLView* view, unsigned device_height, float ui_scale_y, float alpha)
    {
        const LLNetMap* netmap = static_cast<const LLNetMap*>(view);
        LLNetMap* netmap_mut = const_cast<LLNetMap*>(netmap);
        if (!LLWorld::instanceExists() || !LLWorld::getInstance()->getAllowMinimap())
        {
            return;
        }

        // NOTE: like the GL draw(), map contents are NOT modulated by the
        // draw-context alpha (draw() sets absolute colors) — `alpha` is
        // intentionally unused.
        (void)alpha;

        const F32 ui_h = (F32)device_height / ui_scale_y;
        const LLRect screen = view->calcScreenRect();
        const S32 width = view->getRect().getWidth();
        const S32 height = view->getRect().getHeight();
        const F32 mScale = netmap->getVkScale();
        const F32 dot_radius = netmap->getVkDotRadius();

        static LLUIColor map_track_color = LLUIColorTable::instance().getColor("MapTrackColor", LLColor4::white);
        static LLUIColor map_frustum_color = LLUIColorTable::instance().getColor("MapFrustumColor", LLColor4::white);
        static LLUIColor map_whisper_ring_color = LLUIColorTable::instance().getColor("MapWhisperRingColor", LLColor4::blue);
        static LLUIColor map_chat_ring_color = LLUIColorTable::instance().getColor("MapChatRingColor", LLColor4::yellow);
        static LLUIColor map_shout_ring_color = LLUIColorTable::instance().getColor("MapShoutRingColor", LLColor4::red);
        static LLUIColor self_tag_color = LLUIColorTable::instance().getColor("MapAvatarSelfColor", LLColor4::yellow);

        // draw() wraps the map contents in LLLocalClipRect(getLocalRect())
        vk_push_clip(screen, device_height, ui_scale_y);

        // Background rectangle
        LLVKUIRender::emitScreenRect(screen, device_height, ui_scale_y,
                                     netmap->getVkBackgroundColor().get());

        // region 0,0 is in the middle
        const S32 center_sw_left = width / 2 + llfloor(netmap->getVkCurPan().mV[VX]);
        const S32 center_sw_bottom = height / 2 + llfloor(netmap->getVkCurPan().mV[VY]);

        static LLCachedControl<bool> rotate_map(gSavedSettings, "MiniMapRotate");
        F32 rotation = 0.f;
        if (rotate_map)
        {
            rotation = atan2f(LLViewerCamera::getInstance()->getAtAxis().mV[VX],
                              LLViewerCamera::getInstance()->getAtAxis().mV[VY]);
        }

        // Rotation center in GL screen coords
        const F32 rot_cx = (F32)screen.mLeft + (F32)center_sw_left;
        const F32 rot_cy = (F32)screen.mBottom + (F32)center_sw_bottom;

        const F32 region_width = REGION_WIDTH_METERS;
        const F32 scale_pixels_per_meter = mScale / region_width;
        const LLVector3 camera_position = gAgentCamera.getCameraPositionAgent();

        // Region background tiles
        for (LLViewerRegion* regionp : LLWorld::getInstance()->getRegionList())
        {
            LLVector3 origin_agent = regionp->getOriginAgent();
            LLVector3 rel_region_pos = origin_agent - camera_position;
            F32 relative_x = rel_region_pos.mV[0] * scale_pixels_per_meter;
            F32 relative_y = rel_region_pos.mV[1] * scale_pixels_per_meter;

            F32 bottom =    relative_y;
            F32 left =      relative_x;
            const F32 real_width(regionp->getWidth());
            F32 top =       bottom + (real_width / region_width) * mScale ;
            F32 right =     left + (real_width / region_width) * mScale ;

            LLColor4 tint(1.f, 1.f, 1.f, 1.f);
            if (regionp != gAgent.getRegion())
            {
                tint = LLColor4(0.8f, 0.8f, 0.8f, 1.f);
            }
            if (!regionp->isAlive())
            {
                tint = LLColor4(1.f, 0.5f, 0.5f, 1.f);
            }

            if (!LLGridManager::getInstance()->isInSecondLife())
            {
                bool isAgentTeleporting = gAgent.getTeleportState() != LLAgent::TELEPORT_NONE;
                if (!isAgentTeleporting)
                {
                    const LLViewerRegion::tex_matrix_t& tiles(regionp->getWorldMapTiles());
                    for (S32 i(0), scaled_width((S32)(real_width / region_width)), square_width(scaled_width * scaled_width);
                         i < square_width; ++i)
                    {
                        const F32 y = (F32)(i / scaled_width);
                        const F32 x = (F32)(i - y * scaled_width);
                        const F32 local_left(left + x * mScale);
                        const F32 local_right(local_left + mScale);
                        const F32 local_bottom(bottom + y * mScale);
                        const F32 local_top(local_bottom + mScale);
                        const LLViewerTexture* pRegionImage = tiles[(U64)(x * scaled_width + y)].get();
                        if (!pRegionImage)
                        {
                            continue;
                        }
                        // GL draws only tiles with a GL texture: the Vulkan
                        // analog is a successfully uploaded raw image.
                        vk_emit_map_quad("netmap:tile:" + pRegionImage->getID().asString(),
                                         /*solid_fallback=*/false,
                                         local_left, local_bottom, local_right, local_top,
                                         rot_cx, rot_cy, rotation, ui_h, tint);
                    }
                }
            }
            else
            {
                // GL binds the land texture unconditionally (a missing texture
                // still draws the tinted quad), so use the solid fallback.
                LLViewerTexture* land = regionp->getLand().getSTexture();
                vk_emit_map_quad(land ? "netmap:land:" + land->getID().asString() : std::string(),
                                 /*solid_fallback=*/true,
                                 left, bottom, right, top,
                                 rot_cx, rot_cy, rotation, ui_h, tint);
            }
        }

        // Object layer
        static LLCachedControl<bool> show_objects(gSavedSettings, "MiniMapObjects");
        const F32 image_half_width = 0.5f * netmap->getVkObjectMapPixels();
        const F32 image_half_height = 0.5f * netmap->getVkObjectMapPixels();
        if (show_objects)
        {
            LLVector3 map_center_agent = gAgent.getPosAgentFromGlobal(netmap->getVkObjectImageCenterGlobal());
            map_center_agent -= camera_position;
            map_center_agent.mV[VX] *= scale_pixels_per_meter;
            map_center_agent.mV[VY] *= scale_pixels_per_meter;
            vk_emit_map_quad(vk_netmap_key(netmap, "object"), /*solid_fallback=*/false,
                             map_center_agent.mV[VX] - image_half_width, map_center_agent.mV[VY] - image_half_height,
                             map_center_agent.mV[VX] + image_half_width, map_center_agent.mV[VY] + image_half_height,
                             rot_cx, rot_cy, rotation, ui_h, LLColor4(1.f, 1.f, 1.f, 1.f));
        }

        // Parcel overlay layer
        static LLCachedControl<bool> show_property_lines(gSavedSettings, "MiniMapShowPropertyLines");
        if (show_property_lines)
        {
            LLVector3 map_center_agent = gAgent.getPosAgentFromGlobal(netmap->getVkParcelImageCenterGlobal()) - camera_position;
            map_center_agent.mV[VX] *= mScale / region_width;
            map_center_agent.mV[VY] *= mScale / region_width;
            vk_emit_map_quad(vk_netmap_key(netmap, "parcel"), /*solid_fallback=*/false,
                             map_center_agent.mV[VX] - image_half_width, map_center_agent.mV[VY] - image_half_height,
                             map_center_agent.mV[VX] + image_half_width, map_center_agent.mV[VY] + image_half_height,
                             rot_cx, rot_cy, rotation, ui_h, LLColor4(1.f, 1.f, 1.f, 1.f));
        }

        // Avatar dots (cache built by prepareVkDraw: sorted non-friends first,
        // colors already RLVa-filtered)
        for (const LLNetMap::VkAvatarDot& dot : netmap->getVkAvatarDots())
        {
            vk_draw_avatar_dot(dot.pos_map.mV[VX], dot.pos_map.mV[VY], dot.color,
                               dot.pos_map.mV[VZ], dot_radius, dot.unknown_relative_z,
                               screen, device_height, ui_scale_y);
            if (dot.selected)
            {
                if( (dot.pos_map.mV[VX] < 0) ||
                    (dot.pos_map.mV[VY] < 0) ||
                    (dot.pos_map.mV[VX] >= width) ||
                    (dot.pos_map.mV[VY] >= height) )
                {
                    const S32 x = ll_round( dot.pos_map.mV[VX] );
                    const S32 y = ll_round( dot.pos_map.mV[VY] );
                    vk_draw_tracking_circle(screen, view->getRect(), x, y, dot.color, 1, 10, ui_h);
                }
                else
                {
                    vk_draw_dot(dot.pos_map.mV[VX], dot.pos_map.mV[VY], dot.color, 0.f, 5.f,
                                "map_track_16.tga", screen, device_height, ui_scale_y, ui_h);
                }
            }
        }

        // Dot for autopilot target / tracker
        if (gAgent.getAutoPilot())
        {
            vk_netmap_draw_tracking(netmap_mut, screen, device_height, ui_scale_y, ui_h,
                                    gAgent.getAutoPilotTargetGlobal(), map_track_color);
        }
        else
        {
            LLTracker::ETrackingStatus tracking_status = LLTracker::getTrackingStatus();
            if (  LLTracker::TRACKING_AVATAR == tracking_status )
            {
                vk_netmap_draw_tracking(netmap_mut, screen, device_height, ui_scale_y, ui_h,
                                        LLAvatarTracker::instance().getGlobalPos(), map_track_color);
            }
            else if ( LLTracker::TRACKING_LANDMARK == tracking_status
                    || LLTracker::TRACKING_LOCATION == tracking_status )
            {
                vk_netmap_draw_tracking(netmap_mut, screen, device_height, ui_scale_y, ui_h,
                                        LLTracker::getTrackedPositionGlobal(), map_track_color);
            }
        }

        // Dot for self avatar position (sAvatarYouLargeImage)
        LLVector3d pos_global = gAgent.getPositionGlobal();
        LLVector3 pos_map = netmap_mut->vkGlobalPosToView(pos_global);
        const S32 dot_width = ll_round(dot_radius * 2.f);
        {
            const S32 rx = ll_round(screen.mLeft + pos_map.mV[VX] - dot_radius);
            const S32 ry = ll_round(screen.mBottom + pos_map.mV[VY] - dot_radius);
            LLRect r(rx, ry + dot_width, rx + dot_width, ry);
            LLVKUIRender::emitScreenRect(r, device_height, ui_scale_y,
                                         std::string("map_avatar_you_32.tga"), self_tag_color.get());
        }

        // Chat range ring(s)
        static LLCachedControl<bool> chat_ring(gSavedSettings, "MiniMapChatRing");
        static LLCachedControl<bool> fs_whisper_ring(gSavedSettings, "FSMiniMapWhisperRing");
        static LLCachedControl<bool> fs_chat_ring(gSavedSettings, "FSMiniMapChatRing");
        static LLCachedControl<bool> fs_shout_ring(gSavedSettings, "FSMiniMapShoutRing");
        if (chat_ring)
        {
            const F32 meters_to_pixels = mScale / REGION_WIDTH_METERS;
            const F32 cx = (F32)screen.mLeft + pos_map.mV[VX];
            const F32 cy = (F32)screen.mBottom + pos_map.mV[VY];
            if (fs_whisper_ring)
            {
                vk_emit_ring(ui_h, cx, cy, (F32)LFSimFeatureHandler::getInstance()->whisperRange() * meters_to_pixels,
                             2.f /*WIDTH_PIXELS*/, map_whisper_ring_color.get(), 100 /*CIRCLE_STEPS*/);
            }
            if (fs_chat_ring)
            {
                vk_emit_ring(ui_h, cx, cy, (F32)LFSimFeatureHandler::getInstance()->sayRange() * meters_to_pixels,
                             2.f, map_chat_ring_color.get(), 100);
            }
            if (fs_shout_ring)
            {
                vk_emit_ring(ui_h, cx, cy, (F32)LFSimFeatureHandler::getInstance()->shoutRange() * meters_to_pixels,
                             2.f, map_shout_ring_color.get(), 100);
            }
        }

        // Pick radius at the mouse cursor (drawn unrotated in GL)
        S32 local_mouse_x;
        S32 local_mouse_y;
        LLUI::getInstance()->getMousePositionLocal(view, &local_mouse_x, &local_mouse_y);
        static LLCachedControl<F32> fsMinimapPickScale(gSavedSettings, "FSMinimapPickScale");
        static LLUIColor pick_radius_color = LLUIColorTable::instance().getColor("MapPickRadiusColor", map_frustum_color());
        vk_emit_circle(ui_h, (F32)screen.mLeft + local_mouse_x, (F32)screen.mBottom + local_mouse_y,
                       dot_radius * fsMinimapPickScale, pick_radius_color.get(), 32);

        // Frustum (washer sector; inner radius 0 => filled wedge). When the
        // map rotates, the sector is fixed to the rotated frame (camera looks
        // up); otherwise it counter-rotates to the camera heading — the two
        // branches of draw().
        const F32 meters_to_pixels = mScale / REGION_WIDTH_METERS;
        const F32 horiz_fov = LLViewerCamera::getInstance()->getView() * LLViewerCamera::getInstance()->getAspect();
        const F32 far_clip_pixels = LLViewerCamera::getInstance()->getFar() * meters_to_pixels;
        const F32 arc_start = -(horiz_fov / 2.0f) + F_PI_BY_TWO;
        const F32 arc_end = (horiz_fov / 2.0f) + F_PI_BY_TWO;
        const S32 steps = llmax(1, (S32)((horiz_fov * (40.0f / F_TWO_PI)) + 0.5f));
        vk_emit_washer(ui_h, rot_cx, rot_cy, 0.f, far_clip_pixels, arc_start, arc_end, steps,
                       rotate_map ? rotation : -rotation,
                       map_frustum_color.get(), map_frustum_color.get());

        vk_pop_clip();
    }

    // -----------------------------------------------------------------------
    // LLWorldMapView (world map)
    // -----------------------------------------------------------------------

    // File-local copies of llworldmapview.cpp's draw thresholds.
    constexpr F32 VK_DRAW_TEXT_THRESHOLD = 96.f;        // DRAW_TEXT_THRESHOLD
    constexpr S32 VK_DRAW_SIMINFO_THRESHOLD = 3;        // DRAW_SIMINFO_THRESHOLD
    constexpr S32 VK_DRAW_LANDFORSALE_THRESHOLD = 2;    // DRAW_LANDFORSALE_THRESHOLD

    struct VkMapText
    {
        LLWString   text;
        F32         x = 0.f;            // view-local, GL bottom-left origin
        F32         y = 0.f;
        S32         max_pixels = S32_MAX;
        bool        ellipses = false;
        bool        hcenter = false;    // HCENTER (tracking) vs LEFT (region names)
    };

    struct VkTrackMark
    {
        LLVector3d  pos_global;
        LLColor4    color;
        bool        draw_arrow = true;
        bool        is_self = false;
        std::string label;
        std::string tooltip;
        S32         vert_offset = 0;
    };

    // Region name / grid-coordinate strings (draw()'s text block).
    void vk_collect_region_texts(const LLWorldMapView* map, std::vector<VkMapText>& out)
    {
        static LLCachedControl<bool> print_coords(gSavedSettings, "MapShowGridCoords");
        static LLCachedControl<bool> drawAdvancedRegionInfo(gSavedSettings, "FSAdvancedWorldmapRegionInfo");

        const F32 mMapScale = map->getVkMapScale();
        const F32 mMapRatio = map->getVkMapRatio();
        const F32 half_width = map->getRect().getWidth() / 2.0f;
        const F32 half_height = map->getRect().getHeight() / 2.0f;
        const LLVector3d camera_global = gAgentCamera.getCameraPositionGlobal();

        LLWorldMap* world_map = LLWorldMap::getInstance();
        for (LLWorldMapView::handle_list_t::const_iterator iter = map->mVisibleRegions.begin();
             iter != map->mVisibleRegions.end(); ++iter)
        {
            LLSimInfo* info = world_map->simInfoFromHandle(*iter);
            if (!info)
            {
                continue;
            }
            LLVector3d origin_global = from_region_handle(*iter);
            LLVector3d rel_region_pos = origin_global - camera_global;
            const F32 left =   map->mPanX + half_width + (F32)(rel_region_pos.mdV[0] * mMapRatio);
            const F32 bottom = map->mPanY + half_height + (F32)(rel_region_pos.mdV[1] * mMapRatio);

            auto print = [&](const std::string& text, F32 dx, F32 dy, bool use_ellipses)
            {
                VkMapText t;
                t.text = utf8string_to_wstring(text);
                t.x = (F32)llfloor(left + dx);
                t.y = (F32)llfloor(bottom + dy);
                t.max_pixels = (S32)mMapScale;
                t.ellipses = use_ellipses;
                t.hcenter = false;
                out.push_back(t);
            };

            std::string mesg = info->getName();
            if (!mesg.empty() && RlvActions::canShowLocation())
            {
                print(mesg, 3.f, drawAdvancedRegionInfo ? 16.f : 2.f, true);

                if (drawAdvancedRegionInfo)
                {
                    std::string advanced_info = "(";
                    if (!info->isDown())
                    {
                        S32 agent_count = info->getAgentCount();
                        LLViewerRegion *region = gAgent.getRegion();
                        if (region && region->getHandle() == info->getHandle())
                        {
                            ++agent_count; // Bump by 1 if we're in this region
                        }
                        if (agent_count > 0)
                        {
                            advanced_info += llformat("%d - ", agent_count);
                        }
                    }
                    advanced_info += llformat("%s)", info->getAccessString().c_str());
                    print(advanced_info, 3.f, 2.f, true);
                }
            }

            if (print_coords)
            {
                LLVector3d origin = info->getGlobalOrigin();
                std::ostringstream coords;
                coords << "(" << origin.mdV[VX] / REGION_WIDTH_METERS << "," << origin.mdV[VY] / REGION_WIDTH_METERS << ")";
                print(coords.str(), 3.f, drawAdvancedRegionInfo ? 30.f : 16.f, false);
            }
        }
    }

    // The tracking marks draw() emits (self when off-map + tracker/loading),
    // in draw order.
    void vk_collect_tracking_marks(const LLWorldMapView* map, F64 current_time, std::vector<VkTrackMark>& out)
    {
        static LLUIColor map_track_color = LLUIColorTable::instance().getColor("MapTrackColor", LLColor4::white);
        LLWorldMapView* map_mut = const_cast<LLWorldMapView*>(map);

        // Self, when the agent's position is off the visible map
        LLVector3d pos_global = gAgent.getPositionGlobal();
        LLVector3 pos_map = map_mut->vkGlobalPosToView(pos_global);
        if (!map->pointInView(ll_round(pos_map.mV[VX]), ll_round(pos_map.mV[VY])))
        {
            VkTrackMark self;
            self.pos_global = pos_global;
            self.color = lerp(LLColor4::yellow, LLColor4::orange, 0.4f);
            self.draw_arrow = true;
            self.is_self = true;
            self.label = LLWorldMapView::sStringsMap["agent_position"];
            self.vert_offset = LLFontGL::getFontSansSerifSmall()->getLineHeight(); // avoid overlap with target tracking
            out.push_back(self);
        }

        LLTracker::ETrackingStatus tracking_status = LLTracker::getTrackingStatus();
        if ( LLTracker::TRACKING_AVATAR == tracking_status )
        {
            VkTrackMark m;
            m.pos_global = LLAvatarTracker::instance().getGlobalPos();
            m.color = map_track_color.get();
            m.label = LLTracker::getLabel();
            out.push_back(m);
        }
        else if ( LLTracker::TRACKING_LANDMARK == tracking_status
                  || LLTracker::TRACKING_LOCATION == tracking_status )
        {
            // While fetching landmarks, will have 0,0,0 location for a while, so don't draw. JC
            LLVector3d pos = LLTracker::getTrackedPositionGlobal();
            if (!pos.isExactlyZero())
            {
                VkTrackMark m;
                m.pos_global = pos;
                m.color = map_track_color.get();
                m.label = LLTracker::getLabel();
                m.tooltip = LLTracker::getToolTip();
                out.push_back(m);
            }
        }
        else if (LLWorldMap::getInstance()->isTracking())
        {
            VkTrackMark m;
            m.pos_global = LLWorldMap::getInstance()->getTrackedPositionGlobal();
            if (LLWorldMap::getInstance()->isTrackingInvalidLocation())
            {
                // We know this location to be invalid, draw a blue circle
                m.color = LLColor4(0.0, 0.5, 1.0, 1.0);
                m.label = map->getString("InvalidLocation");
            }
            else
            {
                // We don't know yet what that location is, draw a throbing blue circle
                double value = fmod(current_time, 2);
                value = 0.5 + 0.5*cos(value * F_PI);
                m.color = LLColor4(0.0, F32(value/2), F32(value), 1.0);
                m.label = map->getString("Loading");
            }
            out.push_back(m);
        }
    }

    // LLWorldMapView::drawTracking(): circle/arrow (off-view), plain circle
    // (location beacons) or the track-circle image (on-view), plus the clamped
    // label/tooltip text.
    void vk_world_map_draw_tracking(LLWorldMapView* map, const LLRect& screen, unsigned device_height,
                                    float ui_scale_y, F32 ui_h, const VkTrackMark& mark)
    {
        LLVector3 pos_local = map->vkGlobalPosToView(mark.pos_global);
        const S32 x = ll_round( pos_local.mV[VX] );
        const S32 y = ll_round( pos_local.mV[VY] );
        LLFontGL* font = LLFontGL::getFontSansSerifSmall();
        const LLRect rect = map->getRect();

        int track_w = 16, track_h = 16;
        LLVKUIImage::getSize("map_track_16.tga", track_w, track_h);
        S32 text_x = x;
        S32 text_y = (S32)(y - track_h/2 - font->getLineHeight());

        if(    x < 0
            || y < 0
            || x >= rect.getWidth()
            || y >= rect.getHeight() )
        {
            if (mark.draw_arrow)
            {
                vk_draw_tracking_circle(screen, rect, x, y, mark.color, 3, 15, ui_h);
                S32 ax, ay;
                F32 angle;
                vk_tracking_arrow_params(rect, x, y, DEFAULT_TRACKING_ARROW_SIZE, ax, ay, angle);
                vk_emit_arrow(ui_h, (F32)screen.mLeft + ax, (F32)screen.mBottom + ay,
                              (F32)DEFAULT_TRACKING_ARROW_SIZE, angle, mark.color);
                text_x = ax;
                text_y = ay;
            }
        }
        else if (LLTracker::getTrackingStatus() == LLTracker::TRACKING_LOCATION &&
            LLTracker::getTrackedLocationType() != LLTracker::LOCATION_AVATAR &&
            LLTracker::getTrackedLocationType() != LLTracker::LOCATION_NOTHING)
        {
            vk_draw_tracking_circle(screen, rect, x, y, mark.color, 3, 15, ui_h);
        }
        else
        {
            // drawImage(pos_global, sTrackCircleImage, color)
            const S32 rx = ll_round(screen.mLeft + pos_local.mV[VX] - track_w / 2.f);
            const S32 ry = ll_round(screen.mBottom + pos_local.mV[VY] - track_h / 2.f);
            LLRect r(rx, ry + track_h, rx + track_w, ry);
            LLVKUIRender::emitScreenRect(r, device_height, ui_scale_y,
                                         std::string("map_track_16.tga"), mark.color);
        }

        if ( !mark.label.empty() && RlvActions::canShowLocation() )
        {
            // clamp text position to on-screen
            const S32 TEXT_PADDING = DEFAULT_TRACKING_ARROW_SIZE + 2;

            LLWString wlabel = utf8string_to_wstring(mark.label);
            S32 half_text_width = llfloor(font->getWidthF32(wlabel.c_str()) * 0.5f);
            text_x = llclamp(text_x, half_text_width + TEXT_PADDING, rect.getWidth() - half_text_width - TEXT_PADDING);
            text_y = llclamp(text_y + mark.vert_offset, TEXT_PADDING + mark.vert_offset, rect.getHeight() - font->getLineHeight() - TEXT_PADDING - mark.vert_offset);

            if (LLVKText::ready())
            {
                LLVKText::render(font, wlabel,
                                 (F32)(screen.mLeft + text_x), (F32)(screen.mBottom + text_y),
                                 LLColor4::white, LLFontGL::HCENTER, LLFontGL::BASELINE,
                                 S32_MAX, false, LLFontGL::DROP_SHADOW);
                if (!mark.tooltip.empty())
                {
                    text_y -= font->getLineHeight();
                    LLVKText::render(font, utf8string_to_wstring(mark.tooltip),
                                     (F32)(screen.mLeft + text_x), (F32)(screen.mBottom + text_y),
                                     LLColor4::white, LLFontGL::HCENTER, LLFontGL::BASELINE,
                                     S32_MAX, false, LLFontGL::DROP_SHADOW);
                }
            }
        }
    }

    void vk_prepare_world_map(const LLView* view, LLVKContext*)
    {
        LLWorldMapView* map = const_cast<LLWorldMapView*>(static_cast<const LLWorldMapView*>(view));
        map->prepareVkDraw();

        // Upload the mipmap tiles collected by prepareVkDraw (keyed by texture ID)
        for (const LLWorldMapView::VkMapTile& tile : map->getVkMapTiles())
        {
            if (tile.image.notNull())
            {
                vk_upload_texture("wmap:tile:" + tile.image->getID().asString(), tile.image.get(), 600);
            }
        }

        // Upload the "land for sale" overlays for visible regions
        static LLCachedControl<bool> show_for_sale(gSavedSettings, "MapShowLandForSale");
        const S32 level = LLWorldMipmap::scaleToLevel(map->getVkMapScale());
#ifdef OPENSIM
        const bool draw_sale = (show_for_sale && (level <= VK_DRAW_LANDFORSALE_THRESHOLD))
            || LLGridManager::getInstance()->isInAuroraSim();
#else
        const bool draw_sale = show_for_sale && (level <= VK_DRAW_LANDFORSALE_THRESHOLD);
#endif //OPENSIM
        if (draw_sale)
        {
            LLWorldMap* world_map = LLWorldMap::getInstance();
            for (LLWorldMapView::handle_list_t::const_iterator iter = map->mVisibleRegions.begin();
                 iter != map->mVisibleRegions.end(); ++iter)
            {
                LLSimInfo* info = world_map->simInfoFromHandle(*iter);
                if (!info || info->isDown())
                {
                    continue;
                }
                LLViewerFetchedTexture* overlay = info->getLandForSaleImage();
                if (overlay)
                {
                    vk_upload_texture("wmap:sale:" + overlay->getID().asString(), overlay, 600);
                }
            }
        }

        // Pre-rasterize the frame's text (region names + tracking labels) so
        // the glyphs are in the atlas before the render pass.
        if (LLVKText::ready())
        {
            LLFontGL* font_bold = LLFontGL::getFontSansSerifSmallBold();
            LLFontGL* font_small = LLFontGL::getFontSansSerifSmall();
            if (map->getVkMapScale() >= VK_DRAW_TEXT_THRESHOLD)
            {
                std::vector<VkMapText> texts;
                vk_collect_region_texts(map, texts);
                for (const VkMapText& t : texts)
                {
                    LLVKText::prepare(font_bold, t.text);
                }
            }
            std::vector<VkTrackMark> marks;
            vk_collect_tracking_marks(map, LLTimer::getElapsedSeconds(), marks);
            for (const VkTrackMark& m : marks)
            {
                if (!m.label.empty())
                {
                    LLVKText::prepare(font_small, utf8string_to_wstring(m.label));
                }
                if (!m.tooltip.empty())
                {
                    LLVKText::prepare(font_small, utf8string_to_wstring(m.tooltip));
                }
            }
        }
    }

    void vk_world_map_draw_generic_items(LLWorldMapView* map, const LLSimInfo::item_info_list_t& items,
                                         const std::string& image_name, const LLColor4& color,
                                         const LLRect& screen, unsigned device_height, float ui_scale_y)
    {
        for (LLSimInfo::item_info_list_t::const_iterator e = items.begin(); e != items.end(); ++e)
        {
            vk_draw_image_at(map, e->getGlobalPosition(), image_name, color, screen, device_height, ui_scale_y);
        }
    }

    // drawFrustum(): the view-direction wedge fading out with distance.
    void vk_world_map_frustum(const LLWorldMapView* map, const LLRect& screen, F32 ui_h)
    {
        const F32 mMapRatio = map->getVkMapRatio();
        const F32 horiz_fov = LLViewerCamera::getInstance()->getView() * LLViewerCamera::getInstance()->getAspect();
        const F32 far_clip_meters = LLViewerCamera::getInstance()->getFar();
        const F32 far_clip_pixels = far_clip_meters * mMapRatio;
        const F32 half_width_meters = far_clip_meters * tanf( horiz_fov / 2 );
        const F32 half_width_pixels = half_width_meters * mMapRatio;

        const F32 ctr_x = (F32)screen.mLeft + map->getRect().getWidth() * 0.5f + map->mPanX;
        const F32 ctr_y = (F32)screen.mBottom + map->getRect().getHeight() * 0.5f + map->mPanY;

        // get camera look at and left axes
        LLVector3 at_axis = LLViewerCamera::instance().getAtAxis();
        LLVector3 left_axis = LLViewerCamera::instance().getLeftAxis();

        // grab components along XY plane
        LLVector2 cam_lookat(at_axis.mV[VX], at_axis.mV[VY]);
        LLVector2 cam_left(left_axis.mV[VX], left_axis.mV[VY]);

        // but, when looking near straight up or down...
        if (is_approx_zero(cam_lookat.magVecSquared()))
        {
            //...just fall back to looking down the x axis
            cam_lookat = LLVector2(1.f, 0.f); // x axis
            cam_left = LLVector2(0.f, 1.f); // y axis
        }

        // normalize to unit length
        cam_lookat.normVec();
        cam_left.normVec();

        const LLVector2 v1 = cam_lookat * far_clip_pixels + cam_left * half_width_pixels;
        const LLVector2 v2 = cam_lookat * far_clip_pixels - cam_left * half_width_pixels;

        const F32 xy[6] = { ctr_x, ui_h - ctr_y,
                            ctr_x + v1.mV[VX], ui_h - (ctr_y + v1.mV[VY]),
                            ctr_x + v2.mV[VX], ui_h - (ctr_y + v2.mV[VY]) };
        const F32 rgba[12] = { 1.f, 1.f, 1.f, 0.25f,
                               1.f, 1.f, 1.f, 0.02f,
                               1.f, 1.f, 1.f, 0.02f };
        LLVKUI2DSink::get().rawTris(xy, rgba, 3);
    }

    void vk_render_world_map(const LLView* view, unsigned device_height, float ui_scale_y, float alpha)
    {
        const LLWorldMapView* map = static_cast<const LLWorldMapView*>(view);
        LLWorldMapView* map_mut = const_cast<LLWorldMapView*>(map);

        // Like the GL draw(), map contents ignore draw-context alpha.
        (void)alpha;

        const F32 ui_h = (F32)device_height / ui_scale_y;
        const LLRect screen = view->calcScreenRect();
        const S32 level = LLWorldMipmap::scaleToLevel(map->getVkMapScale());

        // draw() wraps the map contents in LLLocalClipRect(getLocalRect())
        vk_push_clip(screen, device_height, ui_scale_y);

        // Background rectangle (ocean color)
        LLVKUIRender::emitScreenRect(screen, device_height, ui_scale_y, map->mBackgroundColor);

        // Mipmap tiles (collected by prepareVkDraw in drawMipmap order)
        for (const LLWorldMapView::VkMapTile& tile : map->getVkMapTiles())
        {
            if (tile.image.isNull())
            {
                continue;
            }
            LLVKUIImage::drawDynamic("wmap:tile:" + tile.image->getID().asString(),
                                     (F32)screen.mLeft + tile.left, ui_h - ((F32)screen.mBottom + tile.top),
                                     (F32)screen.mLeft + tile.right, ui_h - ((F32)screen.mBottom + tile.bottom),
                                     1.f, 1.f, /*coords_opengl=*/true, LLColor4::white);
        }

        // Per-region overlays: red tint over down regions, "land for sale" images
        static LLCachedControl<bool> show_for_sale(gSavedSettings, "MapShowLandForSale");
        const F32 mMapScale = map->getVkMapScale();
        const F32 mMapRatio = map->getVkMapRatio();
        const F32 half_width = map->getRect().getWidth() / 2.0f;
        const F32 half_height = map->getRect().getHeight() / 2.0f;
        const LLVector3d camera_global = gAgentCamera.getCameraPositionGlobal();

        LLWorldMap* world_map = LLWorldMap::getInstance();
        for (LLWorldMapView::handle_list_t::const_iterator iter = map->mVisibleRegions.begin();
             iter != map->mVisibleRegions.end(); ++iter)
        {
            LLSimInfo* info = world_map->simInfoFromHandle(*iter);
            if (!info)
            {
                continue;
            }
            LLVector3d origin_global = from_region_handle(*iter);
            LLVector3d rel_region_pos = origin_global - camera_global;
            const F32 left =   map->mPanX + half_width + (F32)(rel_region_pos.mdV[0] * mMapRatio);
            const F32 bottom = map->mPanY + half_height + (F32)(rel_region_pos.mdV[1] * mMapRatio);
            const F32 top =    bottom + (mMapScale * (info->mSizeY / REGION_WIDTH_METERS));
            const F32 right =  left   + (mMapScale * (info->mSizeX / REGION_WIDTH_METERS));

            if (info->isDown())
            {
                // Draw a transparent red square over down sims
                vk_solid_rect_gl(ui_h, (F32)screen.mLeft + left, (F32)screen.mBottom + bottom,
                                 (F32)screen.mLeft + right, (F32)screen.mBottom + top,
                                 LLColor4(0.2f, 0.0f, 0.0f, 0.4f));
            }
#ifdef OPENSIM
            else if ((show_for_sale && (level <= VK_DRAW_LANDFORSALE_THRESHOLD)) || LLGridManager::getInstance()->isInAuroraSim())
#else
            else if (show_for_sale && (level <= VK_DRAW_LANDFORSALE_THRESHOLD))
#endif //OPENSIM
            {
                LLViewerFetchedTexture* overlay = info->getLandForSaleImage();
                if (overlay)
                {
                    LLVKUIImage::drawDynamic("wmap:sale:" + overlay->getID().asString(),
                                             (F32)screen.mLeft + left, ui_h - ((F32)screen.mBottom + top),
                                             (F32)screen.mLeft + right, ui_h - ((F32)screen.mBottom + bottom),
                                             1.f, 1.f, /*coords_opengl=*/true, LLColor4::white);
                }
            }
        }

        // Region names + grid coords
        if (mMapScale >= VK_DRAW_TEXT_THRESHOLD && LLVKText::ready())
        {
            std::vector<VkMapText> texts;
            vk_collect_region_texts(map, texts);
            LLFontGL* font = LLFontGL::getFontSansSerifSmallBold();
            for (const VkMapText& t : texts)
            {
                LLVKText::render(font, t.text,
                                 (F32)screen.mLeft + t.x, (F32)screen.mBottom + t.y,
                                 LLColor4::white, LLFontGL::LEFT, LLFontGL::BASELINE,
                                 t.max_pixels, t.ellipses, LLFontGL::DROP_SHADOW);
            }
        }

        // Item icons (infohubs, telehubs, land for sale, events)
        static LLCachedControl<bool> show_infohubs(gSavedSettings, "MapShowInfohubs");
        static LLCachedControl<bool> show_telehubs(gSavedSettings, "MapShowTelehubs");
        static LLCachedControl<bool> show_events(gSavedSettings, "MapShowEvents");
        static LLCachedControl<bool> show_mature_events(gSavedSettings, "ShowMatureEvents");
        static LLCachedControl<bool> show_adult_events(gSavedSettings, "ShowAdultEvents");

        if ((level <= VK_DRAW_SIMINFO_THRESHOLD) && (show_infohubs ||
                                                     show_telehubs ||
                                                     show_for_sale ||
                                                     show_events ||
                                                     show_mature_events ||
                                                     show_adult_events))
        {
            const LLColor4 white(1.f, 1.f, 1.f, 1.f);
            for (LLWorldMapView::handle_list_t::const_iterator iter = map->mVisibleRegions.begin();
                 iter != map->mVisibleRegions.end(); ++iter)
            {
                LLSimInfo* info = world_map->simInfoFromHandle(*iter);
                if ((info == NULL) || (info->isDown()))
                {
                    continue;
                }
                if (show_infohubs)
                {
                    vk_world_map_draw_generic_items(map_mut, info->getInfoHub(), "map_infohub.tga", white, screen, device_height, ui_scale_y);
                }
                if (show_telehubs)
                {
                    vk_world_map_draw_generic_items(map_mut, info->getTeleHub(), "map_telehub.tga", white, screen, device_height, ui_scale_y);
                }
                if (show_for_sale)
                {
                    vk_world_map_draw_generic_items(map_mut, info->getLandForSale(), "icon_for_sale.tga", white, screen, device_height, ui_scale_y);
                    // for 1.23, normal land and adult land share the same UI
                    if (gAgent.canAccessAdult())
                    {
                        vk_world_map_draw_generic_items(map_mut, info->getLandForSaleAdult(), "icon_for_sale_adult.tga", white, screen, device_height, ui_scale_y);
                    }
                }
                if (show_events)
                {
                    vk_world_map_draw_generic_items(map_mut, info->getPGEvent(), "Parcel_PG_Dark", white, screen, device_height, ui_scale_y);
                }
                if (show_mature_events && gAgent.canAccessMature())
                {
                    vk_world_map_draw_generic_items(map_mut, info->getMatureEvent(), "Parcel_M_Dark", white, screen, device_height, ui_scale_y);
                }
                if (show_adult_events && gAgent.canAccessAdult())
                {
                    vk_world_map_draw_generic_items(map_mut, info->getAdultEvent(), "Parcel_R_Dark", white, screen, device_height, ui_scale_y);
                }
            }
        }

        // Home location (always)
        LLVector3d home;
        if (gAgent.getHomePosGlobal(&home))
        {
            vk_draw_image_at(map_mut, home, "map_home.tga", LLColor4::white, screen, device_height, ui_scale_y);
        }

        // The current agent, after all that other stuff (sAvatarYouImage)
        LLVector3d pos_global = gAgent.getPositionGlobal();
        vk_draw_image_at(map_mut, pos_global, "map_avatar_16.tga", LLColor4::white, screen, device_height, ui_scale_y);

        // Tracking marks: self first (draw() emits it before the frustum),
        // then the frustum, agents, and the remaining marks — draw()'s order.
        std::vector<VkTrackMark> marks;
        vk_collect_tracking_marks(map, LLTimer::getElapsedSeconds(), marks);
        for (const VkTrackMark& m : marks)
        {
            if (m.is_self)
            {
                vk_world_map_draw_tracking(map_mut, screen, device_height, ui_scale_y, ui_h, m);
            }
        }

        // The current agent viewing angle
        vk_world_map_frustum(map, screen, ui_h);

        // Icons for the avatars in each region (after the self avatar so one
        // can see nearby people)
        static LLCachedControl<bool> mapShowPeople(gSavedSettings, "MapShowPeople");
        if (mapShowPeople && (level <= VK_DRAW_SIMINFO_THRESHOLD))
        {
            static LLUIColor map_avatar_color = LLUIColorTable::instance().getColor("MapAvatarColor", LLColor4::white);
            for (LLWorldMapView::handle_list_t::const_iterator iter = map->mVisibleRegions.begin();
                 iter != map->mVisibleRegions.end(); ++iter)
            {
                LLSimInfo* siminfo = world_map->simInfoFromHandle(*iter);
                if ((siminfo == NULL) || (siminfo->isDown()))
                {
                    continue;
                }
                for (LLSimInfo::item_info_list_t::const_iterator it = siminfo->getAgentLocation().begin();
                     it != siminfo->getAgentLocation().end(); ++it)
                {
                    // drawImageStack(): the same image stacked with a 3px offset
                    for (S32 i = 0; i < it->getCount(); ++i)
                    {
                        vk_draw_image_at(map_mut, it->getGlobalPosition(), "map_avatar_8.tga",
                                         map_avatar_color.get(), screen, device_height, ui_scale_y, (F32)i * 3.f);
                    }
                }
            }
        }

        for (const VkTrackMark& m : marks)
        {
            if (!m.is_self)
            {
                vk_world_map_draw_tracking(map_mut, screen, device_height, ui_scale_y, ui_h, m);
            }
        }

        vk_pop_clip();
    }
} // anonymous namespace

void vk_register_map_hooks()
{
    static bool s_registered = false;
    if (s_registered)
    {
        return;
    }
    s_registered = true;
    LLVKUIRender::registerViewPrepareHook(typeid(LLNetMap), &vk_prepare_net_map);
    LLVKUIRender::registerViewHook(typeid(LLNetMap), &vk_render_net_map);
    LLVKUIRender::registerViewPrepareHook(typeid(LLWorldMapView), &vk_prepare_world_map);
    LLVKUIRender::registerViewHook(typeid(LLWorldMapView), &vk_render_world_map);
}
