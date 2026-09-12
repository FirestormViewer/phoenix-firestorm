#include "llvkwidgetimage.h"

#include <png.h>
#include <cstring>
#include <algorithm>
#include <cmath>

namespace
{
    struct Decode
    {
        png_structp png = nullptr;
        png_infop info = nullptr;
        std::span<const std::uint8_t> encoded;
        std::size_t offset = 0;
        char error[256]{};
        std::vector<std::uint8_t> pixels;
        std::vector<png_bytep> rows;
        ~Decode() { if (png) png_destroy_read_struct(&png,&info,nullptr); }

        static void fail(png_structp png, png_const_charp message)
        {
            auto* state = static_cast<Decode*>(png_get_error_ptr(png));
            if (state)
            {
                std::strncpy(state->error,message,sizeof(state->error)-1);
                state->error[sizeof(state->error)-1] = '\0';
            }
            png_longjmp(png,1);
        }
        static void warn(png_structp, png_const_charp) {}
        static void read(png_structp png, png_bytep output, png_size_t length)
        {
            auto* state = static_cast<Decode*>(png_get_io_ptr(png));
            if (length > state->encoded.size()-state->offset) png_error(png,"Truncated native PNG payload");
            std::memcpy(output,state->encoded.data()+state->offset,length);
            state->offset += length;
        }
    };
}

std::shared_ptr<const LLVKWidgetImage> LLVKWidgetImage::decodePng(std::string name,
    std::span<const std::uint8_t> encoded, std::string& error)
{
    return decode(std::move(name),encoded,error);
}

std::optional<std::vector<std::uint8_t>> LLVKWidgetImage::encodePng(std::uint32_t width,std::uint32_t height,
    std::span<const std::uint8_t> pixels,std::string& error)
{
    error.clear();
    const auto count=std::uint64_t(width)*height;
    if (!width || !height || width>8192 || height>8192 || count>16*1024*1024 || pixels.size()!=count*4)
    { error="Native PNG image has invalid dimensions or byte count"; return std::nullopt; }
    png_image image{};
    image.version=PNG_IMAGE_VERSION; image.width=width; image.height=height; image.format=PNG_FORMAT_RGBA;
    struct Cleanup { png_image& image; ~Cleanup() { png_image_free(&image); } } cleanup{image};
    png_alloc_size_t length=0;
    const auto stride=-static_cast<png_int_32>(width*4);
    if (!png_image_write_to_memory(&image,nullptr,&length,0,pixels.data(),stride,nullptr))
    { error=image.message; return std::nullopt; }
    if (length>128*1024*1024) { error="Native PNG output exceeds byte budget"; return std::nullopt; }
    std::vector<std::uint8_t> encoded(length);
    if (!png_image_write_to_memory(&image,encoded.data(),&length,0,pixels.data(),stride,nullptr))
    { error=image.message; return std::nullopt; }
    encoded.resize(length);
    return encoded;
}

std::shared_ptr<const LLVKWidgetImage> LLVKWidgetImage::fromRgba(std::string name,std::uint32_t width,
    std::uint32_t height,std::span<const std::uint8_t> pixels,std::string& error)
{
    error.clear();
    const auto count=std::uint64_t(width)*height;
    if (!width || !height || width>8192 || height>8192 || count>16*1024*1024 || pixels.size()!=count*4)
    { error="Native RGBA image has invalid dimensions or byte count"; return nullptr; }
    auto result=std::shared_ptr<LLVKWidgetImage>(new LLVKWidgetImage);
    result->mName=std::move(name);
    result->mWidth=result->mLogicalWidth=width;
    result->mHeight=result->mLogicalHeight=height;
    result->mPixels.assign(pixels.begin(),pixels.end());
    for (std::size_t offset=3; offset<pixels.size(); offset+=4)
        if (pixels[offset]!=255) { result->mHasAlpha=true; break; }
    return result;
}

std::shared_ptr<const LLVKWidgetImage> LLVKWidgetImage::browserFrame(std::uint32_t width, std::uint32_t height,
    std::span<const std::uint8_t> topDownBgra, std::string& error)
{
    error.clear();
    const std::uint64_t pixels = std::uint64_t(width)*height;
    if (!width || !height || width > 8192 || height > 8192 || pixels > 16*1024*1024 || topDownBgra.size() != pixels*4)
    { error = "Native browser frame has invalid dimensions or byte count"; return nullptr; }
    auto result = std::shared_ptr<LLVKWidgetImage>(new LLVKWidgetImage);
    result->mName = "native-browser";
    result->mWidth = result->mLogicalWidth = width;
    result->mHeight = result->mLogicalHeight = height;
    result->mPixels.resize(topDownBgra.size());
    for (std::uint32_t row = 0; row < height; ++row)
        for (std::uint32_t column = 0; column < width; ++column)
        {
            const auto source = (std::size_t(row)*width+column)*4;
            const auto destination = (std::size_t(height-1-row)*width+column)*4;
            result->mPixels[destination] = topDownBgra[source+2];
            result->mPixels[destination+1] = topDownBgra[source+1];
            result->mPixels[destination+2] = topDownBgra[source];
            result->mPixels[destination+3] = 255;
        }
    return result;
}

std::optional<LLVKWidgetImage::Geometry> LLVKWidgetImage::prepare(Rect target, float scaleX, float scaleY,
    float translateX, float translateY, std::string& error) const
{
    error.clear();
    const auto width64 = std::int64_t(target.right)-target.left;
    const auto height64 = std::int64_t(target.top)-target.bottom;
    if (width64 < 0 || height64 < 0 || width64 > INT32_MAX || height64 > INT32_MAX ||
        !std::isfinite(scaleX) || !std::isfinite(scaleY) || scaleX <= 0 || scaleY <= 0 ||
        !std::isfinite(translateX) || !std::isfinite(translateY))
    { error = "Invalid native image target or UI transform"; return std::nullopt; }
    const float width = static_cast<float>(width64), height = static_cast<float>(height64);
    const float originX = (translateX+target.left)*scaleX, originY = (translateY+target.bottom)*scaleY;
    const auto rounded = [](float value) { return std::floor(value+0.5f); };
    const bool simple = mScale.left == 0 && mScale.bottom == 0 && mScale.right == 1 && mScale.top == 1;
    Region outer{originX,originY,originX+(simple ? rounded(width*scaleX) : width*scaleX),
        originY+(simple ? rounded(height*scaleY) : height*scaleY)};
    Geometry geometry;
    if (simple) geometry.quads[geometry.count++] = {outer,mClip};
    else
    {
        const float uvWidth = mClip.right-mClip.left, uvHeight = mClip.top-mClip.bottom;
        const Region centerUv{mClip.left+mScale.left*uvWidth,mClip.bottom+mScale.bottom*uvHeight,
            mClip.left+mScale.right*uvWidth,mClip.bottom+mScale.top*uvHeight};
        Region center{centerUv.left*mWidth,centerUv.bottom*mHeight,centerUv.right*mWidth,centerUv.top*mHeight};
        if (mStyle == Scale::Inner)
        {
            const float naturalWidth = rounded(mWidth*uvWidth), naturalHeight = rounded(mHeight*uvHeight);
            center.right += width-naturalWidth;
            center.top += height-naturalHeight;
            const float spanX = mScale.right-mScale.left, spanY = mScale.top-mScale.bottom;
            const float shrinkX = spanX == 1.f ? 0.f : std::max(0.f,center.left-center.right)/(naturalWidth*(1.f-spanX));
            const float shrinkY = spanY == 1.f ? 0.f : std::max(0.f,center.bottom-center.top)/(naturalHeight*(1.f-spanY));
            const float borderScale = 1.f-std::max(shrinkX,shrinkY);
            center.left *= borderScale;
            center.bottom *= borderScale;
            center.right = width+(center.right-width)*borderScale;
            center.top = height+(center.top-height)*borderScale;
        }
        else
        {
            const float centerWidth = center.right-center.left, centerHeight = center.top-center.bottom;
            if (centerWidth <= 0 || centerHeight <= 0)
            { error = "Native outer-scaled image requires a nonempty center"; return std::nullopt; }
            const float factor = std::min({width/centerWidth,height/centerHeight,1.f});
            const float midpointX = (centerUv.left+centerUv.right)*0.5f*width;
            const float midpointY = (centerUv.bottom+centerUv.top)*0.5f*height;
            center = {midpointX-centerWidth*factor*0.5f,midpointY-centerHeight*factor*0.5f,
                midpointX+centerWidth*factor*0.5f,midpointY+centerHeight*factor*0.5f};
        }
        const std::array positionsX{outer.left,rounded(originX+center.left*scaleX),rounded(originX+center.right*scaleX),outer.right};
        const std::array positionsY{outer.bottom,rounded(originY+center.bottom*scaleY),rounded(originY+center.top*scaleY),outer.top};
        const std::array textureX{mClip.left,centerUv.left,centerUv.right,mClip.right};
        const std::array textureY{mClip.bottom,centerUv.bottom,centerUv.top,mClip.top};
        for (std::size_t row = 0; row < 3; ++row)
            for (std::size_t column = 0; column < 3; ++column)
                geometry.quads[geometry.count++] = {{positionsX[column],positionsY[row],positionsX[column+1],positionsY[row+1]},
                    {textureX[column],textureY[row],textureX[column+1],textureY[row+1]}};
    }
    for (std::size_t index = 0; index < geometry.count; ++index)
    {
        const auto& position = geometry.quads[index].position;
        for (const float coordinate : {position.left,position.bottom,position.right,position.top})
            if (!std::isfinite(coordinate) || double(coordinate) < INT32_MIN || double(coordinate) > INT32_MAX)
            { error = "Native image geometry exceeds device coordinate range"; return std::nullopt; }
    }
    return geometry;
}

std::shared_ptr<LLVKWidgetImage> LLVKWidgetImage::decode(std::string name,
    std::span<const std::uint8_t> encoded, std::string& error)
{
    error.clear();
    if (encoded.size() < 8 || encoded.size() > 64 * 1024 * 1024 || png_sig_cmp(encoded.data(),0,8))
    { error = "Invalid or oversized native PNG payload"; return nullptr; }
    auto state = std::make_unique<Decode>();
    state->encoded = encoded;
    state->png = png_create_read_struct(PNG_LIBPNG_VER_STRING,state.get(),Decode::fail,Decode::warn);
    if (!state->png) { error = "Native PNG decoder allocation failed"; return nullptr; }
    if (setjmp(png_jmpbuf(state->png)))
    { error = state->error; return nullptr; }
    state->info = png_create_info_struct(state->png);
    if (!state->info) { error = "Native PNG metadata allocation failed"; return nullptr; }
    png_set_read_fn(state->png,state.get(),Decode::read);
    png_set_user_limits(state->png,8192,8192);
    png_set_chunk_malloc_max(state->png,4 * 1024 * 1024);
    png_set_chunk_cache_max(state->png,128);
    png_read_info(state->png,state->info);
    const auto width = png_get_image_width(state->png,state->info);
    const auto height = png_get_image_height(state->png,state->info);
    if (!width || !height || std::uint64_t(width)*height > 16 * 1024 * 1024)
    { error = "Native widget image exceeds decoded pixel budget"; return nullptr; }
    const auto type = png_get_color_type(state->png,state->info);
    const auto depth = png_get_bit_depth(state->png,state->info);
    if (type == PNG_COLOR_TYPE_PALETTE) png_set_palette_to_rgb(state->png);
    if (type == PNG_COLOR_TYPE_GRAY && depth < 8) png_set_expand_gray_1_2_4_to_8(state->png);
    if (type == PNG_COLOR_TYPE_GRAY || type == PNG_COLOR_TYPE_GRAY_ALPHA) png_set_gray_to_rgb(state->png);
    const bool transparency = png_get_valid(state->png,state->info,PNG_INFO_tRNS) != 0;
    if (transparency) png_set_tRNS_to_alpha(state->png);
    if (depth < 8) png_set_packing(state->png);
    else if (depth == 16) png_set_strip_16(state->png);
    if (!(type & PNG_COLOR_MASK_ALPHA) && !transparency) png_set_add_alpha(state->png,255,PNG_FILLER_AFTER);
    double gamma = 1.0 / 2.2;
    png_get_gAMA(state->png,state->info,&gamma);
    png_set_gamma(state->png,2.2,gamma);
    png_set_interlace_handling(state->png);
    png_read_update_info(state->png,state->info);
    if (png_get_channels(state->png,state->info) != 4 ||
        png_get_rowbytes(state->png,state->info) != std::size_t(width)*4)
    { error = "Native PNG decoder produced an unexpected pixel layout"; return nullptr; }
    state->pixels.resize(std::size_t(width)*height*4);
    state->rows.resize(height);
    for (std::uint32_t row = 0; row < height; ++row)
        state->rows[row] = state->pixels.data()+std::size_t(height-1-row)*width*4;
    png_read_image(state->png,state->rows.data());
    png_read_end(state->png,nullptr);
    auto result = std::shared_ptr<LLVKWidgetImage>(new LLVKWidgetImage);
    result->mName = std::move(name);
    result->mWidth = width;
    result->mHeight = height;
    result->mLogicalWidth = width;
    result->mLogicalHeight = height;
    result->mHasAlpha = transparency || (type & PNG_COLOR_MASK_ALPHA);
    result->mPixels = std::move(state->pixels);
    return result;
}

std::shared_ptr<const LLVKWidgetImage> LLVKWidgetImage::decodeSkinPng(std::string name,
    std::span<const std::uint8_t> encoded, const Metadata& metadata, std::string& error)
{
    return applySkin(decode(std::move(name),encoded,error),metadata,error);
}

std::shared_ptr<const LLVKWidgetImage> LLVKWidgetImage::decodeSkin(std::string name,
    std::span<const std::uint8_t> encoded, const Metadata& metadata, std::string& error)
{
    if (encoded.size() >= 8 && !png_sig_cmp(encoded.data(),0,8))
        return decodeSkinPng(std::move(name),encoded,metadata,error);
    if (encoded.size() >= 2 && encoded[0] == 0xff && encoded[1] == 0xd8)
        return applySkin(decodeJpegPixels(std::move(name),encoded,error),metadata,error);
    if (encoded.size() >= 2 && encoded[0] == 0xff && encoded[1] == 0x4f)
        return applySkin(decodeJ2cPixels(std::move(name),encoded,error),metadata,error);
    return applySkin(decodeTgaPixels(std::move(name),encoded,error),metadata,error);
}

std::shared_ptr<const LLVKWidgetImage> LLVKWidgetImage::applySkin(std::shared_ptr<LLVKWidgetImage> result,
    const Metadata& metadata, std::string& error)
{
    if (!result) return nullptr;
    error.clear();
    if (metadata.style != Scale::Inner && metadata.style != Scale::Outer)
    { error = "Invalid native skin image scale style"; return nullptr; }
    const auto extent = [](std::uint32_t original)
    {
        std::uint32_t dimension = 4;
        while (dimension < original && dimension < 4096) dimension *= 2;
        return dimension;
    };
    const auto width = extent(result->mWidth), height = extent(result->mHeight);
    if (width != result->mWidth || height != result->mHeight)
    {
        std::vector<std::uint8_t> pixels(std::size_t(width)*height*4,0);
        if (!result->mHasAlpha)
            for (std::size_t alpha = 3; alpha < pixels.size(); alpha += 4) pixels[alpha] = 255;
        for (std::uint32_t row = 0; row < std::min(height,result->mHeight); ++row)
            std::memcpy(pixels.data()+std::size_t(row)*width*4,
                result->mPixels.data()+std::size_t(row)*result->mWidth*4,std::size_t(std::min(width,result->mWidth))*4);
        result->mPixels = std::move(pixels);
        result->mWidth = width;
        result->mHeight = height;
    }
    result->mClip = {0,0,float(result->mLogicalWidth)/width,float(result->mLogicalHeight)/height};
    const auto normalize = [](const Rect& rect, std::uint32_t width, std::uint32_t height)
    {
        return Region{std::clamp(float(rect.left)/width,0.f,1.f),std::clamp(float(rect.bottom)/height,0.f,1.f),
            std::clamp(float(rect.right)/width,0.f,1.f),std::clamp(float(rect.top)/height,0.f,1.f)};
    };
    if (metadata.clip && *metadata.clip != Rect{})
        result->mClip = normalize(*metadata.clip,width,height);
    const auto logicalWidth = std::floor(float(width)*(result->mClip.right-result->mClip.left)+0.5f);
    const auto logicalHeight = std::floor(float(height)*(result->mClip.top-result->mClip.bottom)+0.5f);
    if (logicalWidth <= 0.f || logicalHeight <= 0.f)
    { error = "Native skin image has an empty or inverted clip region"; return nullptr; }
    result->mLogicalWidth = static_cast<std::uint32_t>(logicalWidth);
    result->mLogicalHeight = static_cast<std::uint32_t>(logicalHeight);
    if (metadata.scale && *metadata.scale != Rect{})
        result->mScale = normalize(*metadata.scale,result->mLogicalWidth,result->mLogicalHeight);
    result->mStyle = metadata.style;
    result->mSkinPixels = true;
    return result;
}

std::shared_ptr<const LLVKWidgetImage> LLVKWidgetImage::skinView(std::string name,
    std::shared_ptr<const LLVKWidgetImage> pixels, const Metadata& metadata, std::string& error)
{
    error.clear();
    if (!pixels || !pixels->mSkinPixels || pixels->mPixelOwner)
    { error = "Native skin view requires an owning padded pixel image"; return nullptr; }
    auto result = std::shared_ptr<LLVKWidgetImage>(new LLVKWidgetImage);
    result->mName = std::move(name);
    result->mWidth = pixels->mWidth;
    result->mHeight = pixels->mHeight;
    result->mLogicalWidth = pixels->mLogicalWidth;
    result->mLogicalHeight = pixels->mLogicalHeight;
    result->mHasAlpha = pixels->mHasAlpha;
    result->mPixelOwner = std::move(pixels);
    return applySkin(std::move(result),metadata,error);
}