#include "llvkwidgetimage.h"

#include <algorithm>
#include <cstring>

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#define STBI_ONLY_TGA
#define STBI_NO_STDIO
#define STBI_NO_LINEAR
#define STBI_NO_HDR
#define STBI_NO_SIMD
#define STBI_MAX_DIMENSIONS 8192
#ifdef _MSC_VER
#pragma warning(push,0)
#endif
#include <stb_image.h>
#ifdef _MSC_VER
#pragma warning(pop)
#endif

std::shared_ptr<const LLVKWidgetImage> LLVKWidgetImage::decodeTga(std::string name,
    std::span<const std::uint8_t> encoded, std::string& error)
{
    return decodeTgaPixels(std::move(name),encoded,error);
}

std::shared_ptr<LLVKWidgetImage> LLVKWidgetImage::decodeTgaPixels(std::string name,
    std::span<const std::uint8_t> encoded, std::string& error)
{
    error.clear();
    if (encoded.size() < 18 || encoded.size() > 64*1024*1024)
    { error = "Invalid or oversized native TGA payload"; return nullptr; }
    const auto word = [&](std::size_t offset) { return unsigned(encoded[offset])+(unsigned(encoded[offset+1])<<8); };
    const auto width = word(12), height = word(14);
    const auto type = encoded[2];
    const bool indexed = type == 1 || type == 9;
    const bool grayscale = type == 3 || type == 11;
    const bool rle = type == 9 || type == 10 || type == 11;
    if (!width || !height || width > 8192 || height > 8192 || std::uint64_t(width)*height > 16*1024*1024 ||
        (type != 1 && type != 2 && type != 3 && type != 9 && type != 10 && type != 11) ||
        encoded[1] > 1 || (indexed && encoded[1] != 1) || (encoded[17] & 0xd0))
    { error = "Unsupported native TGA header, orientation or dimensions"; return nullptr; }
    const unsigned paletteStart = word(3), paletteLength = word(5);
    const unsigned pixelBits = encoded[16], colorBits = indexed ? encoded[7] : pixelBits;
    if ((indexed && (pixelBits != 8 || !paletteLength)) || (grayscale && pixelBits != 8) ||
        (colorBits != 8 && colorBits != 15 && colorBits != 16 && colorBits != 24 && colorBits != 32))
    { error = "Unsupported native TGA pixel or palette format"; return nullptr; }
    const unsigned pixelBytes = (pixelBits+7)/8;
    const std::size_t paletteBytes = encoded[1] ? std::size_t(paletteLength)*((encoded[7]+7)/8) : 0;
    std::size_t offset = 18+encoded[0];
    if (offset > encoded.size() || paletteBytes > encoded.size()-offset)
    { error = "Truncated native TGA ID or palette"; return nullptr; }
    std::vector<std::uint8_t> normalized(encoded.begin(),encoded.begin()+18);
    normalized[0] = 0;
    if (indexed)
    {
        normalized[3] = normalized[4] = 0;
        normalized.insert(normalized.end(),encoded.begin()+offset,encoded.begin()+offset+paletteBytes);
        if (rle) normalized[17] |= 0x20;
    }
    else
    {
        normalized[1] = 0;
        for (std::size_t field = 3; field <= 7; ++field) normalized[field] = 0;
    }
    offset += paletteBytes;
    std::size_t remaining = std::size_t(width)*height;
    while (remaining)
    {
        unsigned count = 1;
        bool repeated = false;
        if (rle)
        {
            if (offset == encoded.size()) { error = "Truncated native TGA packet header"; return nullptr; }
            const auto packet = encoded[offset++];
            count = (packet & 127)+1;
            repeated = (packet & 128) != 0;
            normalized.push_back(packet);
        }
        if (count > remaining) { error = "Native TGA packet exceeds pixel count"; return nullptr; }
        const auto stored = repeated ? 1u : count;
        const auto bytes = std::size_t(stored)*pixelBytes;
        if (bytes > encoded.size()-offset) { error = "Truncated native TGA pixels"; return nullptr; }
        if (indexed)
        {
            for (unsigned pixel = 0; pixel < stored; ++pixel)
            {
                const auto index = std::clamp(int(encoded[offset+pixel])-int(paletteStart),0,int(paletteLength)-1);
                normalized.push_back(static_cast<std::uint8_t>(index));
            }
        }
        else normalized.insert(normalized.end(),encoded.begin()+offset,encoded.begin()+offset+bytes);
        offset += bytes;
        remaining -= count;
    }
    int decodedWidth = 0, decodedHeight = 0, components = 0;
    std::unique_ptr<stbi_uc,decltype(&stbi_image_free)> pixels(
        stbi_load_from_memory(normalized.data(),static_cast<int>(normalized.size()),&decodedWidth,&decodedHeight,&components,4),
        &stbi_image_free);
    if (!pixels) { error = stbi_failure_reason() ? stbi_failure_reason() : "Native TGA decoder failed"; return nullptr; }
    if (decodedWidth != int(width) || decodedHeight != int(height))
    { error = "Native TGA decoder dimensions disagree with validated header"; return nullptr; }
    auto result = std::shared_ptr<LLVKWidgetImage>(new LLVKWidgetImage);
    result->mName = std::move(name);
    result->mWidth = result->mLogicalWidth = width;
    result->mHeight = result->mLogicalHeight = height;
    result->mPixels.resize(std::size_t(width)*height*4);
    for (std::size_t row = 0; row < height; ++row)
        std::memcpy(result->mPixels.data()+row*width*4,pixels.get()+(height-1-row)*width*4,std::size_t(width)*4);
    if (colorBits == 15 || colorBits == 16)
        for (std::size_t pixel = 0; pixel < result->mPixels.size(); pixel += 4)
            for (std::size_t channel = 0; channel < 3; ++channel)
            {
                const auto fiveBits = (unsigned(result->mPixels[pixel+channel])*31+254)/255;
                result->mPixels[pixel+channel] = static_cast<std::uint8_t>((fiveBits*255+15)/31);
            }
    result->mHasAlpha = colorBits == 32 && indexed;
    if (colorBits == 32 && !indexed)
        for (std::size_t alpha = 3; alpha < result->mPixels.size(); alpha += 4)
            if (result->mPixels[alpha] != 255) { result->mHasAlpha = true; break; }
    return result;
}