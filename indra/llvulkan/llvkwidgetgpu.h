#ifndef LLVKWIDGETGPU_H
#define LLVKWIDGETGPU_H

#include "llvkwidgetpaint.h"
#include "llvkuipacket.h"
#include "llvkimagepublication.h"
#include <filesystem>

class LLVKWidgetGpu final
{
public:
    enum class Status { Pending, Ready, Failed };
    explicit LLVKWidgetGpu(LLVKGlyphUpload::Device device) : mDevice(device) {}
    Status prepare(const LLVKWidgetPaint& paint, VkExtent2D extent, LLVKUiPacket& packet, std::string& error);
    bool waitPendingUploads(std::uint64_t timeout, std::string& error);
    std::optional<std::vector<std::filesystem::path>> dumpFontAtlases(const std::filesystem::path& directory,std::string& error) const;
private:
    struct Image
    {
        std::shared_ptr<const LLVKWidgetImage> source;
        std::unique_ptr<LLVKGlyphUpload> upload;
        std::shared_ptr<const LLVKGlyphImage> ready;
        std::uint64_t used = 0;
    };
    struct Text
    {
        LLVKFont::LineLayout layout;
        std::optional<LLVKGlyphAtlas> atlas;
        std::vector<std::unique_ptr<LLVKGlyphUpload>> uploads;
        std::vector<std::shared_ptr<const LLVKGlyphImage>> pages;
        std::uint64_t used = 0;
    };
    LLVKGlyphUpload::Device mDevice;
    struct Stream
    {
        std::unique_ptr<LLVKImagePublication> publication;
        std::uint64_t used = 0;
        std::uint64_t epoch = 0;
    };
    std::map<LLVKWidgetTree::Id,Stream> mStreams;
    std::map<const LLVKWidgetImage*,Image> mImages;
    std::map<std::pair<LLVKWidgetTree::Id,std::size_t>,Text> mTexts;
    std::uint64_t mFrame = 0;
};

#endif