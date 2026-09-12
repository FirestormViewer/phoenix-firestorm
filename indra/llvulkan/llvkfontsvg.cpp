#include "llvkfontsvg.h"

#include FT_MODULE_H
#include FT_OTSVG_H

#include <algorithm>
#include <cmath>
#include <memory>
#include <new>
#include <vector>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <unordered_map>

namespace
{
    struct AllocationDomain
    {
        static constexpr std::size_t budget = 64 * 1024 * 1024;
        std::unordered_map<void*, std::size_t> allocations;
        std::size_t used = 0;

        void clear() noexcept
        {
            for (const auto& allocation : allocations) std::free(allocation.first);
            allocations.clear();
            used = 0;
        }

        ~AllocationDomain() { clear(); }

        void* allocate(std::size_t size)
        {
            if (size > budget - used) throw std::bad_alloc();
            void* pointer = std::malloc(size ? size : 1);
            if (!pointer) throw std::bad_alloc();
            try { allocations.emplace(pointer, size); }
            catch (...) { std::free(pointer); throw; }
            used += size;
            return pointer;
        }

        void release(void* pointer) noexcept
        {
            if (!pointer) return;
            const auto found = allocations.find(pointer);
            if (found == allocations.end()) std::terminate();
            used -= found->second;
            allocations.erase(found);
            std::free(pointer);
        }

        void* resize(void* pointer, std::size_t size)
        {
            if (!pointer) return allocate(size);
            const auto found = allocations.find(pointer);
            if (found == allocations.end()) std::terminate();
            const auto oldSize = found->second;
            void* replacement = allocate(size);
            std::memcpy(replacement, pointer, std::min(size, oldSize));
            release(pointer);
            return replacement;
        }
    };

    thread_local AllocationDomain* activeDomain = nullptr;

    struct AllocationScope
    {
        AllocationDomain* previous;
        explicit AllocationScope(AllocationDomain& domain) : previous(activeDomain) { activeDomain = &domain; }
        ~AllocationScope() { activeDomain = previous; }
    };

    void* svgAllocate(std::size_t size) { return activeDomain->allocate(size); }
    void* svgResize(void* pointer, std::size_t size) { return activeDomain->resize(pointer, size); }
    void svgRelease(void* pointer) { activeDomain->release(pointer); }
}

#define malloc svgAllocate
#define realloc svgResize
#define free svgRelease
#define nsvgParse llvkNsvgParse
#define nsvgParseFromFile llvkNsvgParseFromFile
#define nsvgDuplicatePath llvkNsvgDuplicatePath
#define nsvgDelete llvkNsvgDelete
#define nsvgCreateRasterizer llvkNsvgCreateRasterizer
#define nsvgRasterize llvkNsvgRasterize
#define nsvgDeleteRasterizer llvkNsvgDeleteRasterizer
#define nsvg__parseXML llvkNsvgParseXML
#define nsvg__colors llvkNsvgColors
#define NANOSVG_IMPLEMENTATION
#include <nanosvg/nanosvg.h>
#define NANOSVGRAST_IMPLEMENTATION
#include <nanosvg/nanosvgrast.h>
#undef malloc
#undef realloc
#undef free

namespace
{
    struct SvgState
    {
        AllocationDomain allocations;
        NSVGimage* image = nullptr;
        FT_GlyphSlot slot = nullptr;
        FT_UInt glyph = 0;
        FT_Error error = FT_Err_Invalid_SVG_Document;
        float scale = 0;
    };

    FT_Error initialize(FT_Pointer* state)
    {
        *state = new (std::nothrow) SvgState;
        return *state ? FT_Err_Ok : FT_Err_Out_Of_Memory;
    }

    void shutdown(FT_Pointer* state)
    {
        delete static_cast<SvgState*>(*state);
        *state = nullptr;
    }

    FT_Error preset(FT_GlyphSlot slot, FT_Bool cache, FT_Pointer* pointer)
    {
        auto* state = static_cast<SvgState*>(*pointer);
        if (!state) return FT_Err_Invalid_Argument;
        if (cache && state->slot == slot && state->glyph == slot->glyph_index) return state->error;
        state->allocations.clear();
        state->image = nullptr;
        state->slot = slot;
        state->glyph = slot->glyph_index;
        state->error = FT_Err_Invalid_SVG_Document;
        try
        {
            AllocationScope allocationScope(state->allocations);
            const auto document = static_cast<FT_SVG_Document>(slot->other);
            if (!document || !document->svg_document || !document->svg_document_length ||
                document->svg_document_length > 4 * 1024 * 1024) return state->error;
            if (document->start_glyph_id != document->end_glyph_id ||
                document->transform.xx != 65536 || document->transform.yy != 65536 ||
                document->transform.xy || document->transform.yx || document->delta.x || document->delta.y)
                return state->error = FT_Err_Unimplemented_Feature;
            std::vector<char> source(document->svg_document, document->svg_document + document->svg_document_length);
            source.push_back('\0');
            state->image = nsvgParse(source.data(), "px", 0.f);
            if (!state->image) return state->error;
            float width = state->image->width;
            float height = state->image->height;
            if (width == 0 || height == 0) width = height = document->units_per_EM;
            if (!std::isfinite(width) || !std::isfinite(height) || width < 1 || height < 1)
                return state->error;
            width = std::floor(width);
            height = std::floor(height);
            const float scale = std::min(document->metrics.x_ppem / width, document->metrics.y_ppem / height);
            const float pixels_wide = std::floor(width * scale);
            const float pixels_high = std::floor(height * scale);
            if (pixels_wide < 1 || pixels_high < 1 || pixels_wide > 4096 || pixels_high > 4096 ||
                pixels_wide * pixels_high > 4 * 1024 * 1024) return state->error;
            state->scale = static_cast<float>(scale);
            slot->bitmap.width = static_cast<unsigned int>(pixels_wide);
            slot->bitmap.rows = static_cast<unsigned int>(pixels_high);
            slot->bitmap.pitch = static_cast<int>(slot->bitmap.width * 4);
            slot->bitmap.pixel_mode = FT_PIXEL_MODE_BGRA;
            slot->bitmap_left = (static_cast<int>(document->metrics.x_ppem) - static_cast<int>(slot->bitmap.width)) / 2;
            slot->bitmap_top = static_cast<int>(document->metrics.ascender / 64);
            const float verticalBearingX = slot->metrics.horiBearingX / 64.f - slot->metrics.horiAdvance / 128.f;
            const float verticalBearingY = (slot->metrics.vertAdvance / 64.f - slot->metrics.height / 64.f) / 2.f;
            slot->metrics.width = static_cast<FT_Pos>(slot->bitmap.width) * 64;
            slot->metrics.height = static_cast<FT_Pos>(slot->bitmap.rows) * 64;
            slot->metrics.horiBearingX = 0;
            slot->metrics.horiBearingY = -static_cast<FT_Pos>(slot->bitmap_top) * 64;
            slot->metrics.vertBearingX = static_cast<FT_Pos>(verticalBearingX * 64.f);
            slot->metrics.vertBearingY = static_cast<FT_Pos>(verticalBearingY * 64.f);
            if (!slot->metrics.vertAdvance) slot->metrics.vertAdvance = static_cast<FT_Pos>(slot->bitmap.rows * 1.2f * 64.f);
            return state->error = FT_Err_Ok;
        }
        catch (const std::bad_alloc&)
        {
            state->allocations.clear();
            state->image = nullptr;
            return state->error = FT_Err_Out_Of_Memory;
        }
        catch (...) { return state->error; }
    }

    FT_Error render(FT_GlyphSlot slot, FT_Pointer* pointer)
    {
        auto* state = static_cast<SvgState*>(*pointer);
        if (!state || state->slot != slot || state->glyph != slot->glyph_index) return FT_Err_Invalid_Argument;
        if (state->error) return state->error;
        if (!state->image || !slot->bitmap.buffer) return FT_Err_Invalid_Argument;
        try
        {
            AllocationScope allocationScope(state->allocations);
            auto* rasterizer = nsvgCreateRasterizer();
            if (!rasterizer) throw std::bad_alloc();
            nsvgRasterize(rasterizer, state->image, 0, 0, state->scale, slot->bitmap.buffer,
                static_cast<int>(slot->bitmap.width), static_cast<int>(slot->bitmap.rows), slot->bitmap.pitch);
        }
        catch (const std::bad_alloc&)
        {
            state->allocations.clear();
            state->image = nullptr;
            return state->error = FT_Err_Out_Of_Memory;
        }
        for (unsigned int row = 0; row < slot->bitmap.rows; ++row)
        {
            auto* pixels = slot->bitmap.buffer + static_cast<std::size_t>(row) * slot->bitmap.pitch;
            for (unsigned int column = 0; column < slot->bitmap.width; ++column)
            {
                auto* pixel = pixels + column * 4;
                const auto red = pixel[0];
                const unsigned int alpha = pixel[3];
                pixel[0] = static_cast<unsigned char>(pixel[2] * alpha / 255);
                pixel[1] = static_cast<unsigned char>(pixel[1] * alpha / 255);
                pixel[2] = static_cast<unsigned char>(red * alpha / 255);
            }
        }
        slot->bitmap.num_grays = 256;
        slot->format = FT_GLYPH_FORMAT_BITMAP;
        state->allocations.clear();
        state->image = nullptr;
        state->slot = nullptr;
        return FT_Err_Ok;
    }
}

FT_Error llvkInstallSvgHooks(FT_Library library)
{
    return FT_Property_Set(library, "ot-svg", "svg-hooks", &LLVKFontSvg::hooks());
}

const SVG_RendererHooks& LLVKFontSvg::hooks() noexcept
{
    static const SVG_RendererHooks callbacks{initialize, shutdown, render, preset};
    return callbacks;
}