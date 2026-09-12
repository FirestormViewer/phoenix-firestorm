#ifndef LLVKUIPACKET_H
#define LLVKUIPACKET_H

#include "llvkcontext.h"
#include "llvkwidgetimage.h"
#include "llvkcolor.h"
#include "llvktextdraw.h"

class LLVKUiPacket final
{
public:
    struct Transform
    {
        float scaleX = 1.f, scaleY = 1.f;
        float translateX = 0.f, translateY = 0.f;
    };
    explicit LLVKUiPacket(VkExtent2D extent) : mExtent(extent) {}
    bool image(const LLVKWidgetImage& source, std::shared_ptr<const LLVKGlyphImage> resource,
        LLVKWidgetImage::Rect rectangle, const Transform& transform, VkRect2D clip,
        const LLVKColor::Value& color, std::string& error, bool alphaMask = false,
        LLVKContext::Blend2D blend = LLVKContext::Blend2D::Alpha);
    bool solid(LLVKWidgetImage::Region deviceRectangle, VkRect2D clip,
        const LLVKColor::Value& color, std::string& error);
    bool triangle(const std::array<float,6>& points, VkRect2D clip,
        const LLVKColor::Value& color, std::string& error);
    bool text(const LLVKGlyphAtlas& atlas, std::span<const std::shared_ptr<const LLVKGlyphImage>> pages,
        float deviceOriginX, float deviceOriginY, VkRect2D clip, const LLVKTextDraw::Style& style, std::string& error);
    std::span<const LLVKContext::UiVertex> vertices() const noexcept { return mVertices; }
    std::span<const LLVKContext::UiDraw> draws() const noexcept { return mDraws; }
    void clear() { mVertices.clear(); mDraws.clear(); }
private:
    bool append(const LLVKWidgetImage::Geometry& geometry, std::shared_ptr<const LLVKGlyphImage> resource,
        VkRect2D clip, const LLVKColor::Value& color, std::string& error);
    VkExtent2D mExtent;
    std::vector<LLVKContext::UiVertex> mVertices;
    std::vector<LLVKContext::UiDraw> mDraws;
};

#endif