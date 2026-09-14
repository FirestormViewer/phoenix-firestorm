#include "llvkglyphupload.h"

#include <cstring>
#include <exception>

namespace
{
    bool checked(VkResult result, const char* operation, std::string& error)
    {
        if (result == VK_SUCCESS) return true;
        error = std::string(operation) + ": " + std::to_string(result);
        return false;
    }

    void waitForRetirement(const LLVKGlyphUpload::Device& device, VkFence fence)
    {
        auto result = vkWaitForFences(device.logical, 1, &fence, VK_TRUE, UINT64_MAX);
        if (result != VK_SUCCESS && result != VK_ERROR_DEVICE_LOST)
        {
            result = vkDeviceWaitIdle(device.logical);
            if (result != VK_SUCCESS && result != VK_ERROR_DEVICE_LOST) std::terminate();
        }
    }

    bool validQueue(const LLVKGlyphUpload::Device& device, std::string& error)
    {
        if (!device.physical || !device.logical || !device.allocator || !device.queue)
        {
            error = "Missing native glyph device handles";
            return false;
        }
        std::uint32_t count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device.physical, &count, nullptr);
        std::vector<VkQueueFamilyProperties> families(count);
        vkGetPhysicalDeviceQueueFamilyProperties(device.physical, &count, families.data());
        constexpr auto required = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;
        if (device.queueFamily >= count || (families[device.queueFamily].queueFlags & required) != required)
        {
            error = "Native glyph queue must support graphics and compute";
            return false;
        }
        return true;
    }
}

struct LLVKGlyphImage::Impl
{
    LLVKGlyphUpload::Device device;
    VkExtent2D extent{};
    VkImage image = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkSampler sampler = VK_NULL_HANDLE;
    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    VkDescriptorPool pool = VK_NULL_HANDLE;
    VkDescriptorSet descriptor = VK_NULL_HANDLE;
    ~Impl()
    {
        if (pool) vkDestroyDescriptorPool(device.logical, pool, nullptr);
        if (layout) vkDestroyDescriptorSetLayout(device.logical, layout, nullptr);
        if (sampler) vkDestroySampler(device.logical, sampler, nullptr);
        if (view) vkDestroyImageView(device.logical, view, nullptr);
        if (image) vmaDestroyImage(device.allocator, image, allocation);
    }
};

struct LLVKGlyphUpload::Impl
{
    Device device;
    VkBuffer staging = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VkCommandPool pool = VK_NULL_HANDLE;
    VkFence fence = VK_NULL_HANDLE;
    bool submitted = false;
    Status status = Status::Pending;
    std::string failure;
    std::shared_ptr<LLVKGlyphImage> image;

    ~Impl()
    {
        if (submitted) waitForRetirement(device, fence);
        if (fence) vkDestroyFence(device.logical, fence, nullptr);
        if (pool) vkDestroyCommandPool(device.logical, pool, nullptr);
        if (staging) vmaDestroyBuffer(device.allocator, staging, allocation);
    }
};

LLVKGlyphImage::LLVKGlyphImage(std::unique_ptr<Impl> impl) : mImpl(std::move(impl)) {}
LLVKGlyphImage::~LLVKGlyphImage() = default;
VkImage LLVKGlyphImage::image() const noexcept { return mImpl->image; }
VkImageView LLVKGlyphImage::view() const noexcept { return mImpl->view; }
VkDescriptorSet LLVKGlyphImage::descriptor() const noexcept { return mImpl->descriptor; }
VkDescriptorSetLayout LLVKGlyphImage::descriptorLayout() const noexcept { return mImpl->layout; }
VkExtent2D LLVKGlyphImage::extent() const noexcept { return mImpl->extent; }
bool LLVKGlyphImage::compatibleWith(VkDevice device, VmaAllocator allocator, VkQueue queue, std::uint32_t queueFamily) const noexcept
{
    return mImpl->device.logical == device && mImpl->device.allocator == allocator &&
        mImpl->device.queue == queue && mImpl->device.queueFamily == queueFamily;
}
LLVKGlyphUpload::LLVKGlyphUpload(std::unique_ptr<Impl> impl) : mImpl(std::move(impl)) {}
LLVKGlyphUpload::~LLVKGlyphUpload() = default;

std::shared_ptr<const LLVKGlyphImage> LLVKGlyphUpload::published() const noexcept
{
    return mImpl->status == Status::Ready ? mImpl->image : nullptr;
}

LLVKGlyphUpload::Status LLVKGlyphUpload::poll(std::string& error)
{
    error.clear();
    if (mImpl->status != Status::Pending)
    {
        error = mImpl->failure;
        return mImpl->status;
    }
    const auto result = vkGetFenceStatus(mImpl->device.logical, mImpl->fence);
    if (result == VK_NOT_READY) return Status::Pending;
    if (!checked(result, "glyph upload completion", error))
    {
        mImpl->failure = error;
        return mImpl->status = Status::Failed;
    }
    mImpl->submitted = false;
    vmaDestroyBuffer(mImpl->device.allocator, mImpl->staging, mImpl->allocation);
    mImpl->staging = VK_NULL_HANDLE;
    mImpl->allocation = VK_NULL_HANDLE;
    vkDestroyCommandPool(mImpl->device.logical, mImpl->pool, nullptr);
    mImpl->pool = VK_NULL_HANDLE;
    return mImpl->status = Status::Ready;
}

std::unique_ptr<LLVKGlyphUpload> LLVKGlyphUpload::submit(const Device& device, VkExtent2D extent,
                                                      std::span<const std::uint8_t> rgba,
                                                      std::string& error, Sampling sampling)
{
    error.clear();
    if (sampling != Sampling::GlyphNearestRepeat && sampling != Sampling::SkinLinearClamp && sampling != Sampling::SkinAnisotropicClamp)
    { error = "Invalid native image sampling policy"; return nullptr; }
    if (sampling==Sampling::SkinAnisotropicClamp && !device.samplerAnisotropyEnabled)
    { error="Native anisotropic skin sampling was not enabled on the device"; return nullptr; }
    if (!validQueue(device, error)) return nullptr;
    const std::uint64_t pixels = std::uint64_t(extent.width) * extent.height;
    if (!device.physical || !device.logical || !device.allocator || !device.queue ||
        !extent.width || !extent.height || pixels > 16 * 1024 * 1024 || rgba.size() != pixels * 4)
    {
        error = "Invalid glyph upload device, extent or RGBA payload (64 MiB maximum)";
        return nullptr;
    }
    VkImageFormatProperties support{};
    constexpr auto usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    if (!checked(vkGetPhysicalDeviceImageFormatProperties(device.physical, VK_FORMAT_R8G8B8A8_UNORM,
                 VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, usage, 0, &support), "glyph image format", error)) return nullptr;
    VkFormatProperties format{};
    vkGetPhysicalDeviceFormatProperties(device.physical, VK_FORMAT_R8G8B8A8_UNORM, &format);
    if (extent.width > support.maxExtent.width || extent.height > support.maxExtent.height ||
        !(format.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) ||
        (sampling != Sampling::GlyphNearestRepeat && !(format.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)))
    {
        error = "Selected device cannot support glyph extent or sampling";
        return nullptr;
    }
    auto upload = std::make_unique<Impl>();
    upload->device = device;
    auto resource = std::make_unique<LLVKGlyphImage::Impl>();
    resource->device = device;
    resource->extent = extent;
    VkBufferCreateInfo buffer{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    buffer.size = rgba.size();
    buffer.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    VmaAllocationCreateInfo host{};
    host.usage = VMA_MEMORY_USAGE_AUTO;
    host.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
    VmaAllocationInfo mapped{};
    if (!checked(vmaCreateBuffer(device.allocator, &buffer, &host, &upload->staging, &upload->allocation, &mapped),
                 "glyph staging allocation", error)) return nullptr;
    std::memcpy(mapped.pMappedData, rgba.data(), rgba.size());
    if (!checked(vmaFlushAllocation(device.allocator, upload->allocation, 0, rgba.size()), "glyph staging flush", error)) return nullptr;
    VkImageCreateInfo image{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    image.imageType = VK_IMAGE_TYPE_2D;
    image.format = VK_FORMAT_R8G8B8A8_UNORM;
    image.extent = {extent.width, extent.height, 1};
    image.mipLevels = image.arrayLayers = 1;
    image.samples = VK_SAMPLE_COUNT_1_BIT;
    image.tiling = VK_IMAGE_TILING_OPTIMAL;
    image.usage = usage;
    VmaAllocationCreateInfo local{};
    local.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    if (!checked(vmaCreateImage(device.allocator, &image, &local, &resource->image, &resource->allocation, nullptr),
                 "glyph image allocation", error)) return nullptr;
    VkImageViewCreateInfo view{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    view.image = resource->image;
    view.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view.format = image.format;
    view.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    if (!checked(vkCreateImageView(device.logical, &view, nullptr, &resource->view), "glyph image view", error)) return nullptr;
    VkSamplerCreateInfo sampler{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    sampler.magFilter = sampler.minFilter = sampling != Sampling::GlyphNearestRepeat ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
    sampler.addressModeU = sampler.addressModeV = sampler.addressModeW = sampling != Sampling::GlyphNearestRepeat ?
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE : VK_SAMPLER_ADDRESS_MODE_REPEAT;
    if (sampling==Sampling::SkinAnisotropicClamp)
    {
        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(device.physical,&properties);
        sampler.anisotropyEnable=VK_TRUE;
        sampler.maxAnisotropy=properties.limits.maxSamplerAnisotropy;
    }
    if (!checked(vkCreateSampler(device.logical, &sampler, nullptr, &resource->sampler), "glyph sampler", error)) return nullptr;
    VkDescriptorSetLayoutBinding binding{0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1,
                                        VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT, nullptr};
    VkDescriptorSetLayoutCreateInfo layout{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    layout.bindingCount = 1;
    layout.pBindings = &binding;
    if (!checked(vkCreateDescriptorSetLayout(device.logical, &layout, nullptr, &resource->layout), "glyph descriptor layout", error)) return nullptr;
    VkDescriptorPoolSize poolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1};
    VkDescriptorPoolCreateInfo descriptorPool{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    descriptorPool.maxSets = 1;
    descriptorPool.poolSizeCount = 1;
    descriptorPool.pPoolSizes = &poolSize;
    if (!checked(vkCreateDescriptorPool(device.logical, &descriptorPool, nullptr, &resource->pool), "glyph descriptor pool", error)) return nullptr;
    VkDescriptorSetAllocateInfo descriptor{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    descriptor.descriptorPool = resource->pool;
    descriptor.descriptorSetCount = 1;
    descriptor.pSetLayouts = &resource->layout;
    if (!checked(vkAllocateDescriptorSets(device.logical, &descriptor, &resource->descriptor), "glyph descriptor", error)) return nullptr;
    VkDescriptorImageInfo descriptorImage{resource->sampler, resource->view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    write.dstSet = resource->descriptor;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo = &descriptorImage;
    vkUpdateDescriptorSets(device.logical, 1, &write, 0, nullptr);
    VkCommandPoolCreateInfo pool{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    pool.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    pool.queueFamilyIndex = device.queueFamily;
    if (!checked(vkCreateCommandPool(device.logical, &pool, nullptr, &upload->pool), "glyph command pool", error)) return nullptr;
    VkCommandBufferAllocateInfo allocate{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    allocate.commandPool = upload->pool;
    allocate.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocate.commandBufferCount = 1;
    VkCommandBuffer command = VK_NULL_HANDLE;
    if (!checked(vkAllocateCommandBuffers(device.logical, &allocate, &command), "glyph command buffer", error)) return nullptr;
    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (!checked(vkBeginCommandBuffer(command, &begin), "glyph begin recording", error)) return nullptr;
    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = resource->image;
    barrier.subresourceRange = view.subresourceRange;
    vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                         0, nullptr, 0, nullptr, 1, &barrier);
    VkBufferImageCopy copy{};
    copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    copy.imageExtent = image.extent;
    vkCmdCopyBufferToImage(command, upload->staging, resource->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);
    if (!checked(vkEndCommandBuffer(command), "glyph end recording", error)) return nullptr;
    VkFenceCreateInfo fence{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    if (!checked(vkCreateFence(device.logical, &fence, nullptr, &upload->fence), "glyph fence", error)) return nullptr;
    upload->image = std::shared_ptr<LLVKGlyphImage>(new LLVKGlyphImage(std::move(resource)));
    auto result = std::unique_ptr<LLVKGlyphUpload>(new LLVKGlyphUpload(std::move(upload)));
    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &command;
    if (!checked(vkQueueSubmit(device.queue, 1, &submit, result->mImpl->fence), "glyph submit", error)) return nullptr;
    result->mImpl->submitted = true;
    return result;
}

LLVKGlyphUpload::Status LLVKGlyphUpload::wait(std::uint64_t timeout, std::string& error)
{
    if (mImpl->status != Status::Pending) return poll(error);
    error.clear();
    const auto result = vkWaitForFences(mImpl->device.logical, 1, &mImpl->fence, VK_TRUE, timeout);
    if (result == VK_TIMEOUT) return Status::Pending;
    if (!checked(result, "glyph upload fence wait", error))
    {
        mImpl->failure = error;
        return mImpl->status = Status::Failed;
    }
    return poll(error);
}

struct LLVKGlyphSubmission::Impl
{
    LLVKGlyphUpload::Device device;
    VkCommandPool pool = VK_NULL_HANDLE;
    VkCommandBuffer command = VK_NULL_HANDLE;
    VkFence fence = VK_NULL_HANDLE;
    Status status = Status::Recording;
    bool submitted = false;
    std::string failure;
    std::vector<std::shared_ptr<const LLVKGlyphImage>> images;
    std::vector<std::shared_ptr<const LLVKTextDraw>> textDraws;
    ~Impl()
    {
        if (submitted) waitForRetirement(device, fence);
        if (fence) vkDestroyFence(device.logical, fence, nullptr);
        if (pool) vkDestroyCommandPool(device.logical, pool, nullptr);
    }
};

LLVKGlyphSubmission::LLVKGlyphSubmission(std::unique_ptr<Impl> impl) : mImpl(std::move(impl)) {}
LLVKGlyphSubmission::~LLVKGlyphSubmission() = default;

std::unique_ptr<LLVKGlyphSubmission> LLVKGlyphSubmission::begin(const LLVKGlyphUpload::Device& device,
    std::vector<std::shared_ptr<const LLVKGlyphImage>> images, std::string& error)
{
    error.clear();
    if (!validQueue(device, error)) return nullptr;
    for (const auto& image : images)
    {
        if (!image || image->mImpl->device.logical != device.logical ||
            image->mImpl->device.allocator != device.allocator ||
            image->mImpl->device.queue != device.queue ||
            image->mImpl->device.queueFamily != device.queueFamily)
        {
            error = "Glyph submission contains an image from a different device or queue";
            return nullptr;
        }
    }
    auto impl = std::make_unique<Impl>();
    impl->device = device;
    impl->images = std::move(images);
    VkCommandPoolCreateInfo pool{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    pool.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    pool.queueFamilyIndex = device.queueFamily;
    if (!checked(vkCreateCommandPool(device.logical, &pool, nullptr, &impl->pool), "glyph consumer pool", error)) return nullptr;
    VkCommandBufferAllocateInfo allocate{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    allocate.commandPool = impl->pool;
    allocate.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocate.commandBufferCount = 1;
    if (!checked(vkAllocateCommandBuffers(device.logical, &allocate, &impl->command), "glyph consumer commands", error)) return nullptr;
    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (!checked(vkBeginCommandBuffer(impl->command, &begin), "glyph consumer begin", error)) return nullptr;
    VkFenceCreateInfo fence{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    if (!checked(vkCreateFence(device.logical, &fence, nullptr, &impl->fence), "glyph consumer fence", error)) return nullptr;
    return std::unique_ptr<LLVKGlyphSubmission>(new LLVKGlyphSubmission(std::move(impl)));
}

VkCommandBuffer LLVKGlyphSubmission::commands() const noexcept
{
    return mImpl->status == Status::Recording ? mImpl->command : VK_NULL_HANDLE;
}

bool LLVKGlyphSubmission::submit(std::string& error)
{
    error.clear();
    if (mImpl->status != Status::Recording)
    {
        error = "Glyph submission is no longer recording";
        return false;
    }
    if (!checked(vkEndCommandBuffer(mImpl->command), "glyph consumer end", error))
    {
        mImpl->failure = error;
        mImpl->status = Status::Failed;
        return false;
    }
    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &mImpl->command;
    if (!checked(vkQueueSubmit(mImpl->device.queue, 1, &submit, mImpl->fence), "glyph consumer submit", error))
    {
        mImpl->failure = error;
        mImpl->status = Status::Failed;
        return false;
    }
    mImpl->submitted = true;
    mImpl->status = Status::Pending;
    return true;
}

LLVKGlyphSubmission::Status LLVKGlyphSubmission::poll(std::string& error)
{
    error = mImpl->failure;
    if (mImpl->status != Status::Pending) return mImpl->status;
    const auto result = vkGetFenceStatus(mImpl->device.logical, mImpl->fence);
    if (result == VK_NOT_READY) return Status::Pending;
    if (!checked(result, "glyph consumer completion", error))
    {
        mImpl->failure = error;
        return mImpl->status = Status::Failed;
    }
    mImpl->submitted = false;
    vkDestroyCommandPool(mImpl->device.logical, mImpl->pool, nullptr);
    mImpl->pool = VK_NULL_HANDLE;
    mImpl->command = VK_NULL_HANDLE;
    mImpl->images.clear();
    mImpl->textDraws.clear();
    return mImpl->status = Status::Complete;
}

bool LLVKGlyphSubmission::retainText(std::shared_ptr<const LLVKTextDraw> draw,
    const LLVKGlyphUpload::Device& device,
    const std::vector<std::shared_ptr<const LLVKGlyphImage>>& images, std::string& error)
{
    if (mImpl->status != Status::Recording || device.logical != mImpl->device.logical ||
        device.allocator != mImpl->device.allocator || device.queue != mImpl->device.queue)
    {
        error = "Text draw and submission owners do not match";
        return false;
    }
    for (const auto& image : images)
    {
        if (image->mImpl->device.logical != device.logical || image->mImpl->device.queue != device.queue)
        {
            error = "Text image and submission owners do not match";
            return false;
        }
    }
    mImpl->textDraws.push_back(std::move(draw));
    return true;
}

LLVKGlyphSubmission::Status LLVKGlyphSubmission::wait(std::uint64_t timeout, std::string& error)
{
    if (mImpl->status != Status::Pending) return poll(error);
    error.clear();
    const auto result = vkWaitForFences(mImpl->device.logical, 1, &mImpl->fence, VK_TRUE, timeout);
    if (result == VK_TIMEOUT) return Status::Pending;
    if (!checked(result, "glyph consumer fence wait", error))
    {
        mImpl->failure = error;
        return mImpl->status = Status::Failed;
    }
    return poll(error);
}