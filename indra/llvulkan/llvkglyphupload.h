#ifndef LLVKGLYPHUPLOAD_H
#define LLVKGLYPHUPLOAD_H

#include <volk/volk.h>
#include <vma/vk_mem_alloc.h>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

class LLVKTextDraw;

class LLVKGlyphImage final
{
public:
    ~LLVKGlyphImage();
    LLVKGlyphImage(const LLVKGlyphImage&) = delete;
    LLVKGlyphImage& operator=(const LLVKGlyphImage&) = delete;
    VkImage image() const noexcept;
    VkImageView view() const noexcept;
    VkDescriptorSet descriptor() const noexcept;
    VkDescriptorSetLayout descriptorLayout() const noexcept;
    VkExtent2D extent() const noexcept;
    bool compatibleWith(VkDevice device, VmaAllocator allocator, VkQueue queue, std::uint32_t queueFamily) const noexcept;
private:
    friend class LLVKGlyphUpload;
    friend class LLVKGlyphSubmission;
    struct Impl;
    explicit LLVKGlyphImage(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> mImpl;
};

class LLVKGlyphUpload final
{
public:
    struct Device
    {
        VkPhysicalDevice physical = VK_NULL_HANDLE;
        VkDevice logical = VK_NULL_HANDLE;
        VmaAllocator allocator = VK_NULL_HANDLE;
        VkQueue queue = VK_NULL_HANDLE;
        std::uint32_t queueFamily = 0;
        bool samplerAnisotropyEnabled = false;
    };
    enum class Status { Pending, Ready, Failed };
    enum class Sampling { GlyphNearestRepeat, SkinLinearClamp, SkinAnisotropicClamp, BrowserLinearRepeat };
    static std::unique_ptr<LLVKGlyphUpload> submit(const Device& device,
                                                  VkExtent2D extent,
                                                  std::span<const std::uint8_t> rgba,
                                                  std::string& error,
                                                  Sampling sampling = Sampling::GlyphNearestRepeat);
    ~LLVKGlyphUpload();
    LLVKGlyphUpload(const LLVKGlyphUpload&) = delete;
    LLVKGlyphUpload& operator=(const LLVKGlyphUpload&) = delete;
    Status poll(std::string& error);
    Status wait(std::uint64_t timeout, std::string& error);
    std::shared_ptr<const LLVKGlyphImage> published() const noexcept;
    std::shared_ptr<const LLVKGlyphImage> submittedFor(const Device& consumer) const noexcept;
private:
    struct Impl;
    explicit LLVKGlyphUpload(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> mImpl;
};

class LLVKGlyphSubmission final
{
public:
    enum class Status { Recording, Pending, Complete, Failed };
    static std::unique_ptr<LLVKGlyphSubmission> begin(const LLVKGlyphUpload::Device& device,
        std::vector<std::shared_ptr<const LLVKGlyphImage>> images, std::string& error);
    ~LLVKGlyphSubmission();
    LLVKGlyphSubmission(const LLVKGlyphSubmission&) = delete;
    LLVKGlyphSubmission& operator=(const LLVKGlyphSubmission&) = delete;
    VkCommandBuffer commands() const noexcept;
    bool submit(std::string& error);
    Status poll(std::string& error);
    Status wait(std::uint64_t timeout, std::string& error);
private:
    friend class LLVKTextDraw;
    bool retainText(std::shared_ptr<const LLVKTextDraw> draw, const LLVKGlyphUpload::Device& device,
                    const std::vector<std::shared_ptr<const LLVKGlyphImage>>& images, std::string& error);
    struct Impl;
    explicit LLVKGlyphSubmission(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> mImpl;
};

#endif