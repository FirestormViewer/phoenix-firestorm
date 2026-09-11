#include "llvkuipacket.h"
#include "llvkglyphupload.h"

#include <cmath>

bool LLVKUiPacket::image(const LLVKWidgetImage& source, std::shared_ptr<const LLVKGlyphImage> resource,
    LLVKWidgetImage::Rect rectangle, const Transform& transform, VkRect2D clip,
    const LLVKColor::Value& color, std::string& error, bool alphaMask, LLVKContext::Blend2D blend)
{
    error.clear();
    if (!resource || resource->extent().width != source.pixelWidth() || resource->extent().height != source.pixelHeight())
    { error = "Native UI image requires a published resource matching its padded pixels"; return false; }
    if (static_cast<unsigned>(blend) >= static_cast<unsigned>(LLVKContext::Blend2D::Count))
    { error = "Native image blend mode is invalid"; return false; }
    const auto geometry = source.prepare(rectangle,transform.scaleX,transform.scaleY,transform.translateX,transform.translateY,error);
    const auto previous = mDraws.size();
    if (!geometry || !append(*geometry,std::move(resource),clip,color,error)) return false;
    if (mDraws.size() != previous) { mDraws.back().alphaMask = alphaMask; mDraws.back().blend = blend; }
    return true;
}

bool LLVKUiPacket::solid(LLVKWidgetImage::Region deviceRectangle, VkRect2D clip,
    const LLVKColor::Value& color, std::string& error)
{
    error.clear();
    if (deviceRectangle.left > deviceRectangle.right || deviceRectangle.bottom > deviceRectangle.top)
    { error = "Native UI solid rectangle is inverted"; return false; }
    LLVKWidgetImage::Geometry geometry;
    geometry.count = 1;
    geometry.quads[0] = {deviceRectangle,{0,0,1,1}};
    return append(geometry,{},clip,color,error);
}

bool LLVKUiPacket::append(const LLVKWidgetImage::Geometry& geometry, std::shared_ptr<const LLVKGlyphImage> resource,
    VkRect2D clip, const LLVKColor::Value& color, std::string& error)
{
    if (!mExtent.width || !mExtent.height || mExtent.width > INT32_MAX || mExtent.height > INT32_MAX ||
        clip.offset.x < 0 || clip.offset.y < 0 ||
        std::uint64_t(clip.offset.x)+clip.extent.width > mExtent.width ||
        std::uint64_t(clip.offset.y)+clip.extent.height > mExtent.height)
    { error = "Native UI packet clip or framebuffer extent is invalid"; return false; }
    for (const auto channel : color)
        if (!std::isfinite(channel)) { error = "Native UI packet color is nonfinite"; return false; }
    if (!clip.extent.width || !clip.extent.height) return true;
    const auto count = geometry.count*6;
    if (mDraws.size() >= 65536 || count > 1024*1024-mVertices.size())
    { error = "Native UI packet exceeds frame geometry budget"; return false; }
    std::array<LLVKContext::UiVertex,54> prepared;
    std::size_t used = 0;
    for (std::size_t index = 0; index < geometry.count; ++index)
    {
        const auto& quad = geometry.quads[index];
        const auto& position = quad.position;
        const std::array corners{
            LLVKContext::UiVertex{position.left,float(mExtent.height)-position.bottom,quad.uv.left,quad.uv.bottom,color[0],color[1],color[2],color[3]},
            LLVKContext::UiVertex{position.right,float(mExtent.height)-position.bottom,quad.uv.right,quad.uv.bottom,color[0],color[1],color[2],color[3]},
            LLVKContext::UiVertex{position.right,float(mExtent.height)-position.top,quad.uv.right,quad.uv.top,color[0],color[1],color[2],color[3]},
            LLVKContext::UiVertex{position.left,float(mExtent.height)-position.top,quad.uv.left,quad.uv.top,color[0],color[1],color[2],color[3]}};
        for (const auto corner : {0,1,2,0,2,3})
        {
            const auto& vertex = corners[corner];
            if (!std::isfinite(vertex.positionX) || !std::isfinite(vertex.positionY))
            { error = "Native UI packet geometry is nonfinite"; return false; }
            prepared[used++] = vertex;
        }
    }
    LLVKContext::UiDraw draw;
    draw.firstVertex = static_cast<std::uint32_t>(mVertices.size());
    draw.vertexCount = static_cast<std::uint32_t>(count);
    draw.clip = clip;
    draw.image = std::move(resource);
    mVertices.reserve(mVertices.size()+count);
    mDraws.reserve(mDraws.size()+1);
    mVertices.insert(mVertices.end(),prepared.begin(),prepared.begin()+count);
    mDraws.push_back(std::move(draw));
    return true;
}

bool LLVKUiPacket::text(const LLVKGlyphAtlas& atlas, std::span<const std::shared_ptr<const LLVKGlyphImage>> pages,
    float deviceOriginX, float deviceOriginY, VkRect2D clip, const LLVKTextDraw::Style& style, std::string& error)
{
    error.clear();
    if (!std::isfinite(deviceOriginX) || !std::isfinite(deviceOriginY) || style.depth != 0 || pages.size() != atlas.pages().size())
    { error = "Native UI text requires matching atlas pages, finite origin and disabled depth"; return false; }
    for (const auto& page : pages)
        if (!page || page->extent().width != atlas.pageSize() || page->extent().height != atlas.pageSize())
        { error = "Native UI text atlas page is unpublished or mismatched"; return false; }
    const auto quads = LLVKTextDraw::prepare(atlas,style,error);
    if (!quads) return false;
    const auto previousVertices = mVertices.size(), previousDraws = mDraws.size();
    const auto rollback = [&] { mVertices.resize(previousVertices); mDraws.resize(previousDraws); };
    try
    {
        for (const auto& quad : *quads)
        {
            LLVKWidgetImage::Geometry geometry;
            geometry.count = 1;
            geometry.quads[0] = {{quad.left+deviceOriginX,quad.bottom+deviceOriginY,quad.right+deviceOriginX,quad.top+deviceOriginY},
                {quad.leftU,quad.bottomV,quad.rightU,quad.topV}};
            LLVKColor::Value color;
            for (std::size_t channel = 0; channel < color.size(); ++channel) color[channel] = quad.color[channel]/255.f;
            if (!append(geometry,pages[quad.page],clip,color,error)) { rollback(); return false; }
        }
    }
    catch (...) { rollback(); throw; }
    return true;
}