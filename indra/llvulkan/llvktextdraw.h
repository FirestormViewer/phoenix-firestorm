#ifndef LLVKTEXTDRAW_H
#define LLVKTEXTDRAW_H

#include "llvkglyphatlas.h"
#include "llvkglyphupload.h"
#include <array>

class LLVKTextPipeline final
{
public:
    static std::shared_ptr<LLVKTextPipeline> create(const LLVKGlyphUpload::Device& device,
                                                  VkFormat colorFormat, std::string& error,
                                                  VkFormat depthFormat = VK_FORMAT_UNDEFINED,
                                                  VkCompareOp depthCompare = VK_COMPARE_OP_LESS_OR_EQUAL,
                                                  bool depthWrite = false);
    ~LLVKTextPipeline();
    LLVKTextPipeline(const LLVKTextPipeline&) = delete;
    LLVKTextPipeline& operator=(const LLVKTextPipeline&) = delete;
private:
    friend class LLVKTextDraw;
    struct Impl;
    explicit LLVKTextPipeline(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> mImpl;
};

class LLVKTextDraw final
{
public:
    enum class Shadow { None, Hard, Soft };
    struct Style
    {
        std::array<std::uint8_t, 4> color{255, 255, 255, 255};
        std::array<std::uint8_t, 4> shadowColor{0, 0, 0, 255};
        float shadowStrength = 0.f;
        float depth = 0.f;
        Shadow shadow = Shadow::None;
        bool syntheticBold = false;
    };
    struct View
    {
        VkImageView target = VK_NULL_HANDLE;
        VkImageView depthTarget = VK_NULL_HANDLE;
        VkExtent2D extent{};
        VkRect2D clip{};
        std::array<float, 16> projection{};
        std::array<float, 16> textureTransform{1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    };
    struct Quad
    {
        std::size_t page;
        float left, bottom, right, top;
        float leftU, bottomV, rightU, topV;
        std::array<std::uint8_t,4> color;
    };
    static std::optional<std::vector<Quad>> prepare(const LLVKGlyphAtlas& atlas, const Style& style, std::string& error);
    static bool record(LLVKGlyphSubmission& submission, std::shared_ptr<LLVKTextPipeline> pipeline,
                       const LLVKGlyphAtlas& atlas,
                       std::vector<std::shared_ptr<const LLVKGlyphImage>> images,
                       const View& view, const Style& style, std::string& error);
    ~LLVKTextDraw();
private:
    struct Impl;
    explicit LLVKTextDraw(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> mImpl;
};

#endif