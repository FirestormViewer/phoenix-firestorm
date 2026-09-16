#include "linden_common.h"
#include "llvkglyphupload.h"
#include "llvkglyphatlas.h"
#include "llvktextdraw.h"
#include "lltut.h"

#include <atomic>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>

namespace tut
{
    struct glyphgpu_data
    {
        VkInstance instance = VK_NULL_HANDLE;
        VkDebugUtilsMessengerEXT messenger = VK_NULL_HANDLE;
        LLVKGlyphUpload::Device device;
        VkBuffer output = VK_NULL_HANDLE;
        VmaAllocation outputAllocation = VK_NULL_HANDLE;
        VkDescriptorSetLayout outputLayout = VK_NULL_HANDLE;
        VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
        VkShaderModule shader = VK_NULL_HANDLE;
        VkPipeline pipeline = VK_NULL_HANDLE;
        VkCommandPool commandPool = VK_NULL_HANDLE;
        VkFence fence = VK_NULL_HANDLE;
        VkImage targetImage = VK_NULL_HANDLE;
        VmaAllocation targetAllocation = VK_NULL_HANDLE;
        VkImageView targetView = VK_NULL_HANDLE;
        VkImage depthImage = VK_NULL_HANDLE;
        VmaAllocation depthAllocation = VK_NULL_HANDLE;
        VkImageView depthView = VK_NULL_HANDLE;
        std::atomic<unsigned> validationErrors{0};

        void drawCheck(bool withDepth, LLVKTextDraw::Shadow shadow = LLVKTextDraw::Shadow::None,
                   bool syntheticBold = false);

        static VKAPI_ATTR VkBool32 VKAPI_CALL debug(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                                    VkDebugUtilsMessageTypeFlagsEXT,
                                                    const VkDebugUtilsMessengerCallbackDataEXT* message, void* user)
        {
            if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
            {
                ++static_cast<glyphgpu_data*>(user)->validationErrors;
                std::cerr << message->pMessage << '\n';
            }
            return VK_FALSE;
        }

        void initialize()
        {
            ensure_equals("Vulkan loader", volkInitialize(), VK_SUCCESS);
            const char* layers[] = {"VK_LAYER_KHRONOS_validation"};
            const char* extensions[] = {VK_EXT_DEBUG_UTILS_EXTENSION_NAME, VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME};
            VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
            app.pApplicationName = "Native glyph resource tests";
            app.apiVersion = VK_API_VERSION_1_1;
            VkDebugUtilsMessengerCreateInfoEXT debugInfo{VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
            debugInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            debugInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT;
            debugInfo.pfnUserCallback = debug;
            debugInfo.pUserData = this;
            VkValidationFeatureEnableEXT enabled = VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT;
            VkValidationFeaturesEXT validation{VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT};
            validation.enabledValidationFeatureCount = 1;
            validation.pEnabledValidationFeatures = &enabled;
            validation.pNext = &debugInfo;
            VkInstanceCreateInfo create{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
            create.pApplicationInfo = &app;
            create.enabledLayerCount = 1;
            create.ppEnabledLayerNames = layers;
            create.enabledExtensionCount = 2;
            create.ppEnabledExtensionNames = extensions;
            create.pNext = &validation;
            ensure_equals("instance with required validation", vkCreateInstance(&create, nullptr, &instance), VK_SUCCESS);
            volkLoadInstance(instance);
            ensure_equals("validation messenger", vkCreateDebugUtilsMessengerEXT(instance, &debugInfo, nullptr, &messenger), VK_SUCCESS);
            std::uint32_t count = 0;
            ensure_equals("enumerate devices", vkEnumeratePhysicalDevices(instance, &count, nullptr), VK_SUCCESS);
            std::vector<VkPhysicalDevice> devices(count);
            ensure_equals("get devices", vkEnumeratePhysicalDevices(instance, &count, devices.data()), VK_SUCCESS);
            for (auto candidate : devices)
            {
                std::uint32_t families = 0;
                vkGetPhysicalDeviceQueueFamilyProperties(candidate, &families, nullptr);
                std::vector<VkQueueFamilyProperties> queues(families);
                vkGetPhysicalDeviceQueueFamilyProperties(candidate, &families, queues.data());
                for (std::uint32_t index = 0; index < families; ++index)
                {
                    constexpr auto required = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;
                    if ((queues[index].queueFlags & required) == required)
                    {
                        device.physical = candidate;
                        device.queueFamily = index;
                        break;
                    }
                }
                if (device.physical) break;
            }
            ensure("graphics and compute device", device.physical != VK_NULL_HANDLE);
            VkPhysicalDeviceProperties properties{};
            vkGetPhysicalDeviceProperties(device.physical, &properties);
            std::cout << "Glyph GPU device: " << properties.deviceName << "; Khronos + synchronization validation enabled\n";
            float priority = 1.f;
            VkDeviceQueueCreateInfo queue{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
            queue.queueFamilyIndex = device.queueFamily;
            queue.queueCount = 1;
            queue.pQueuePriorities = &priority;
            VkDeviceCreateInfo logical{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
            logical.queueCreateInfoCount = 1;
            logical.pQueueCreateInfos = &queue;
            ensure_equals("logical device", vkCreateDevice(device.physical, &logical, nullptr, &device.logical), VK_SUCCESS);
            volkLoadDevice(device.logical);
            vkGetDeviceQueue(device.logical, device.queueFamily, 0, &device.queue);
            VmaVulkanFunctions functions{};
            functions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
            functions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;
            VmaAllocatorCreateInfo allocator{};
            allocator.instance = instance;
            allocator.physicalDevice = device.physical;
            allocator.device = device.logical;
            allocator.vulkanApiVersion = VK_API_VERSION_1_1;
            allocator.pVulkanFunctions = &functions;
            ensure_equals("allocator", vmaCreateAllocator(&allocator, &device.allocator), VK_SUCCESS);
        }

        void shutdown()
        {
            if (device.logical)
            {
                vkDeviceWaitIdle(device.logical);
                if (fence) vkDestroyFence(device.logical, fence, nullptr);
                if (commandPool) vkDestroyCommandPool(device.logical, commandPool, nullptr);
                if (pipeline) vkDestroyPipeline(device.logical, pipeline, nullptr);
                if (shader) vkDestroyShaderModule(device.logical, shader, nullptr);
                if (pipelineLayout) vkDestroyPipelineLayout(device.logical, pipelineLayout, nullptr);
                if (descriptorPool) vkDestroyDescriptorPool(device.logical, descriptorPool, nullptr);
                if (outputLayout) vkDestroyDescriptorSetLayout(device.logical, outputLayout, nullptr);
                if (output) vmaDestroyBuffer(device.allocator, output, outputAllocation);
                if (targetView) vkDestroyImageView(device.logical, targetView, nullptr);
                if (targetImage) vmaDestroyImage(device.allocator, targetImage, targetAllocation);
                if (depthView) vkDestroyImageView(device.logical, depthView, nullptr);
                if (depthImage) vmaDestroyImage(device.allocator, depthImage, depthAllocation);
                if (device.allocator) vmaDestroyAllocator(device.allocator);
                vkDestroyDevice(device.logical, nullptr);
                device.logical = VK_NULL_HANDLE;
            }
            if (messenger) vkDestroyDebugUtilsMessengerEXT(instance, messenger, nullptr);
            if (instance) vkDestroyInstance(instance, nullptr);
            messenger = VK_NULL_HANDLE;
            instance = VK_NULL_HANDLE;
        }
        ~glyphgpu_data() { shutdown(); }
    };
    typedef test_group<glyphgpu_data> glyphgpu_group;
    typedef glyphgpu_group::object object;
    glyphgpu_group glyphgpu_tests("llvkglyphupload");

    void glyphgpu_data::drawCheck(bool withDepth, LLVKTextDraw::Shadow shadow, bool syntheticBold)
    {
        initialize();
        std::string error;
        LLVKFont::LineLayout line;
        auto glyph = std::make_shared<LLVKFont::Glyph>();
        glyph->raster.width = glyph->raster.height = 4;
        glyph->raster.bottomUpPixels.assign(16, 255);
        line.glyphs.push_back({0,false,2,6,6,2,glyph});
        auto displaced=line;
        for (auto& draw : displaced.glyphs)
        {
            draw.left+=17.25f; draw.right+=17.25f;
            draw.bottom+=9.5f; draw.top+=9.5f;
        }
        auto atlas = LLVKGlyphAtlas::prepare(displaced, 16, 1024, error);
        ensure(error, atlas.has_value());
        const auto atlasPixels=atlas->pages()[0].rgba;
        auto upload = LLVKGlyphUpload::submit(device, {16,16}, atlas->pages()[0].rgba, error);
        ensure(error, upload != nullptr);
        ensure("published after own fence", upload->wait(UINT64_MAX,error) == LLVKGlyphUpload::Status::Ready);
        auto image = upload->published();
        upload.reset();
        ensure("existing glyph placements can move without repacking",atlas->updateLayout(line));
        ensure("placement update preserves every atlas byte",atlas->pages()[0].rgba==atlasPixels);
        auto incompatible=displaced;
        incompatible.glyphs[0].glyph=std::make_shared<LLVKFont::Glyph>(*glyph);
        ensure("different raster owner rejects placement reuse",!atlas->updateLayout(incompatible));
        ensure_equals("failed reuse preserves prior placement",atlas->placements()[0].draw.left,2.f);
        incompatible.glyphs.clear();
        ensure("different glyph count rejects placement reuse",!atlas->updateLayout(incompatible));
        auto textPipeline = LLVKTextPipeline::create(device, VK_FORMAT_R8G8B8A8_UNORM, error,
                                 withDepth ? VK_FORMAT_D32_SFLOAT : VK_FORMAT_UNDEFINED);
        ensure(error, textPipeline != nullptr);
        VkImageCreateInfo target{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        target.imageType = VK_IMAGE_TYPE_2D;
        target.format = VK_FORMAT_R8G8B8A8_UNORM;
        target.extent = {16,16,1};
        target.mipLevels = target.arrayLayers = 1;
        target.samples = VK_SAMPLE_COUNT_1_BIT;
        target.tiling = VK_IMAGE_TILING_OPTIMAL;
        target.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        VmaAllocationCreateInfo local{};
        local.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        ensure_equals("text target", vmaCreateImage(device.allocator, &target, &local, &targetImage, &targetAllocation, nullptr), VK_SUCCESS);
        VkImageViewCreateInfo targetInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        targetInfo.image = targetImage;
        targetInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        targetInfo.format = target.format;
        targetInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
        ensure_equals("text target view", vkCreateImageView(device.logical, &targetInfo, nullptr, &targetView), VK_SUCCESS);
        if (withDepth)
        {
            auto depthTarget = target;
            depthTarget.format = VK_FORMAT_D32_SFLOAT;
            depthTarget.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
            ensure_equals("depth image", vmaCreateImage(device.allocator,&depthTarget,&local,&depthImage,&depthAllocation,nullptr),VK_SUCCESS);
            auto depthInfo = targetInfo;
            depthInfo.image = depthImage;
            depthInfo.format = VK_FORMAT_D32_SFLOAT;
            depthInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            ensure_equals("depth view",vkCreateImageView(device.logical,&depthInfo,nullptr,&depthView),VK_SUCCESS);
        }
        VkBufferCreateInfo readback{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        readback.size = 16 * 16 * 4;
        readback.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        VmaAllocationCreateInfo host{};
        host.usage = VMA_MEMORY_USAGE_AUTO;
        host.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
        VmaAllocationInfo mapped{};
        ensure_equals("text readback", vmaCreateBuffer(device.allocator, &readback, &host, &output, &outputAllocation, &mapped), VK_SUCCESS);
        auto submission = LLVKGlyphSubmission::begin(device, {image}, error);
        ensure(error, submission != nullptr);
        auto command = submission->commands();
        VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        barrier.image = targetImage;
        barrier.subresourceRange = targetInfo.subresourceRange;
        barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             0,0,nullptr,0,nullptr,1,&barrier);
        VkClearColorValue background{{0.f,0.f,1.f,1.f}};
        vkCmdClearColorImage(command,targetImage,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,&background,1,&barrier.subresourceRange);
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                             0,0,nullptr,0,nullptr,1,&barrier);
        if (withDepth)
        {
            VkImageMemoryBarrier depthBarrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
            depthBarrier.image = depthImage;
            depthBarrier.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT,0,1,0,1};
            depthBarrier.srcQueueFamilyIndex = depthBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            depthBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            depthBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            depthBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 0,0,nullptr,0,nullptr,1,&depthBarrier);
            const VkClearDepthStencilValue depthClear{0.5f,0};
            vkCmdClearDepthStencilImage(command,depthImage,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,&depthClear,1,&depthBarrier.subresourceRange);
            depthBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            depthBarrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            depthBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            depthBarrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                                 0,0,nullptr,0,nullptr,1,&depthBarrier);
        }
        LLVKTextDraw::View view;
        view.target = targetView;
        view.depthTarget = depthView;
        view.extent = {16,16};
        view.clip = {{0,0},{4,16}};
        view.projection = {2.f/16,0,0,0, 0,-2.f/16,0,0, 0,0,1,0, -1,1,0,1};
        view.textureTransform[12] = 1.f;
        view.textureTransform[13] = -1.f;
        LLVKTextDraw::Style style;
        style.color = {255,0,0,128};
        style.shadow = shadow;
        style.shadowStrength = 1.f;
        style.syntheticBold = syntheticBold;
        if (withDepth)
        {
            style.depth = 0.75f;
            ensure("record rejected depth",LLVKTextDraw::record(*submission,textPipeline,*atlas,{image},view,style,error));
            style.depth = 0.25f;
        }
        ensure("record graphics text", LLVKTextDraw::record(*submission,textPipeline,*atlas,{image},view,style,error));
        if (withDepth)
        {
            style.depth = 0.4f;
            style.color = {0,255,0,128};
            ensure("record subsequent depth",LLVKTextDraw::record(*submission,textPipeline,*atlas,{image},view,style,error));
        }
        std::weak_ptr<LLVKTextPipeline> pipelineLifetime = textPipeline;
        std::weak_ptr<const LLVKGlyphImage> imageLifetime = image;
        ensure("later placement updates cannot alter recorded geometry",atlas->updateLayout(displaced));
        textPipeline.reset();
        image.reset();
        atlas.reset();
        line.glyphs.clear();
        glyph.reset();
        ensure("pipeline retained during recording", !pipelineLifetime.expired());
        barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,
                             0,0,nullptr,0,nullptr,1,&barrier);
        VkBufferImageCopy copy{};
        copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};
        copy.imageExtent = {16,16,1};
        vkCmdCopyImageToBuffer(command,targetImage,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,output,1,&copy);
        VkMemoryBarrier hostRead{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
        hostRead.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        hostRead.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
        vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,
                             0,1,&hostRead,0,nullptr,0,nullptr);
        ensure("graphics submit", submission->submit(error));
        const auto completion = submission->wait(UINT64_MAX,error);
         ensure("graphics fence complete: status=" + std::to_string(static_cast<int>(completion)) + " " + error,
             completion == LLVKGlyphSubmission::Status::Complete);
        ensure("pipeline retired by consumer", pipelineLifetime.expired());
        ensure("image retired by consumer", imageLifetime.expired());
        ensure_equals("readback invalidate", vmaInvalidateAllocation(device.allocator,outputAllocation,0,readback.size),VK_SUCCESS);
        const auto* pixels = static_cast<const std::uint8_t*>(mapped.pMappedData);
        for (unsigned row = 0; row < 16; ++row)
        {
            for (unsigned column = 0; column < 16; ++column)
            {
                std::array<std::uint8_t,4> expected{0,0,255,255};
                const auto composite = [&](int offsetX,int offsetY,std::array<std::uint8_t,4> source)
                {
                    if (column >= 4 || static_cast<int>(column) < 2+offsetX || static_cast<int>(column) >= 6+offsetX ||
                        static_cast<int>(row) < 10-offsetY || static_cast<int>(row) >= 14-offsetY) return;
                    const unsigned alpha = source[3];
                    for (unsigned channel = 0; channel < 4; ++channel)
                        expected[channel] = static_cast<std::uint8_t>((source[channel]*alpha + expected[channel]*(255-alpha) + 127)/255);
                };
                if (syntheticBold)
                {
                    composite(0,0,{255,0,0,128});
                    composite(1,0,{255,0,0,128});
                }
                else
                {
                    if (shadow == LLVKTextDraw::Shadow::Hard) composite(1,-1,{0,0,0,128});
                    else if (shadow == LLVKTextDraw::Shadow::Soft)
                    {
                        composite(-1,-1,{0,0,0,38}); composite(1,-1,{0,0,0,38});
                        composite(1,1,{0,0,0,38}); composite(-1,1,{0,0,0,38});
                        composite(0,-2,{0,0,0,38});
                    }
                    composite(0,0,{255,0,0,128});
                }
                if (withDepth) composite(0,0,{0,255,0,128});
                for (unsigned channel = 0; channel < 4; ++channel)
                    ensure_equals("graphics output channel", pixels[(row*16+column)*4+channel], expected[channel]);
            }
        }
        submission.reset();
        shutdown();
        ensure_equals("graphics validation errors", validationErrors.load(),0u);
    }

    template<> template<>
    void object::test<2>()
    {
        set_test_name("native graphics text blending clipping and resource retention");
        drawCheck(false);
    }

    template<> template<>
    void object::test<3>()
    {
        set_test_name("native text depth rejection and disabled depth writes");
        drawCheck(true);
    }

    template<> template<>
    void object::test<4>()
    {
        set_test_name("native hard shadow ordering and alpha");
        drawCheck(false,LLVKTextDraw::Shadow::Hard);
    }

    template<> template<>
    void object::test<5>()
    {
        set_test_name("native five-pass soft shadow ordering and alpha");
        drawCheck(false,LLVKTextDraw::Shadow::Soft);
    }

    template<> template<>
    void object::test<6>()
    {
        set_test_name("native synthetic bold suppresses shadow");
        drawCheck(false,LLVKTextDraw::Shadow::Soft,true);
    }

    template<> template<>
    void object::test<1>()
    {
        set_test_name("immutable glyph publication sampled under Vulkan validation");
        initialize();
        std::string error;
        std::ifstream fontStream(LLVK_FONT_FIXTURE, std::ios::binary);
        ensure("packaged font", fontStream.good());
        std::vector<std::uint8_t> fontBytes{std::istreambuf_iterator<char>(fontStream), std::istreambuf_iterator<char>()};
        auto font = LLVKFont::create({fontBytes, {}}, {}, false, error);
        ensure(error, font != nullptr);
        auto line = font->layoutLine(U"A VA", 0, 4, {}, error);
        ensure(error, line.has_value());
        auto atlas = LLVKGlyphAtlas::prepare(*line, 32, 4096, error);
        ensure(error, atlas.has_value());
        ensure_equals("one native atlas page", atlas->pages().size(), std::size_t(1));
        std::vector<std::uint8_t> pixels = atlas->pages().front().rgba;
        const VkExtent2D extent{atlas->pageSize(), atlas->pageSize()};
        const auto expected = pixels;
        auto upload = LLVKGlyphUpload::submit(device, extent, pixels, error);
        ensure(error, upload != nullptr);
        ensure("not published without completion observation", !upload->published());
        pixels.assign(pixels.size(), 0);
        atlas.reset();
        line.reset();
        font.reset();
        ensure("ready after own fence", upload->wait(UINT64_MAX,error) == LLVKGlyphUpload::Status::Ready);
        auto image = upload->published();
        ensure("published", image != nullptr);
        upload.reset();
        ensure_equals("width", image->extent().width, extent.width);
        auto replacement = LLVKGlyphUpload::submit(device, extent, pixels, error);
        ensure(error, replacement != nullptr);
        ensure("replacement not published before observation", !replacement->published());
        replacement.reset();

        VkBufferCreateInfo buffer{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        buffer.size = expected.size();
        buffer.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        VmaAllocationCreateInfo host{};
        host.usage = VMA_MEMORY_USAGE_AUTO;
        host.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
        VmaAllocationInfo mapped{};
        ensure_equals("readback buffer", vmaCreateBuffer(device.allocator, &buffer, &host, &output, &outputAllocation, &mapped), VK_SUCCESS);
        VkDescriptorSetLayoutBinding binding{0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};
        VkDescriptorSetLayoutCreateInfo layout{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        layout.bindingCount = 1;
        layout.pBindings = &binding;
        ensure_equals("output layout", vkCreateDescriptorSetLayout(device.logical, &layout, nullptr, &outputLayout), VK_SUCCESS);
        VkDescriptorPoolSize poolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1};
        VkDescriptorPoolCreateInfo pool{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        pool.maxSets = pool.poolSizeCount = 1;
        pool.pPoolSizes = &poolSize;
        ensure_equals("output descriptor pool", vkCreateDescriptorPool(device.logical, &pool, nullptr, &descriptorPool), VK_SUCCESS);
        VkDescriptorSetAllocateInfo allocateSet{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        allocateSet.descriptorPool = descriptorPool;
        allocateSet.descriptorSetCount = 1;
        allocateSet.pSetLayouts = &outputLayout;
        VkDescriptorSet outputSet = VK_NULL_HANDLE;
        ensure_equals("output set", vkAllocateDescriptorSets(device.logical, &allocateSet, &outputSet), VK_SUCCESS);
        VkDescriptorBufferInfo outputInfo{output, 0, expected.size()};
        VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        write.dstSet = outputSet;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        write.pBufferInfo = &outputInfo;
        vkUpdateDescriptorSets(device.logical, 1, &write, 0, nullptr);
        VkDescriptorSetLayout layouts[]{image->descriptorLayout(), outputLayout};
        VkPipelineLayoutCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        pipelineInfo.setLayoutCount = 2;
        pipelineInfo.pSetLayouts = layouts;
        ensure_equals("pipeline layout", vkCreatePipelineLayout(device.logical, &pipelineInfo, nullptr, &pipelineLayout), VK_SUCCESS);
        std::ifstream stream(LLVK_GLYPH_READBACK_SHADER, std::ios::binary | std::ios::ate);
        ensure("readback shader exists", stream.good());
        const auto size = static_cast<std::size_t>(stream.tellg());
        ensure("word-sized SPIR-V", size > 0 && size % 4 == 0);
        std::vector<std::uint32_t> code(size / 4);
        stream.seekg(0);
        stream.read(reinterpret_cast<char*>(code.data()), size);
        ensure("shader read", stream.good());
        VkShaderModuleCreateInfo module{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        module.codeSize = size;
        module.pCode = code.data();
        ensure_equals("shader module", vkCreateShaderModule(device.logical, &module, nullptr, &shader), VK_SUCCESS);
        VkComputePipelineCreateInfo compute{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
        compute.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        compute.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        compute.stage.module = shader;
        compute.stage.pName = "main";
        compute.layout = pipelineLayout;
        ensure_equals("compute pipeline", vkCreateComputePipelines(device.logical, VK_NULL_HANDLE, 1, &compute, nullptr, &pipeline), VK_SUCCESS);
        auto submission = LLVKGlyphSubmission::begin(device, {image}, error);
        ensure(error, submission != nullptr);
        auto secondSubmission = LLVKGlyphSubmission::begin(device, {image}, error);
        ensure(error, secondSubmission != nullptr);
        VkCommandBuffer command = submission->commands();
        VkDescriptorSet descriptors[]{image->descriptor(), outputSet};
        vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
        vkCmdBindDescriptorSets(command, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 2, descriptors, 0, nullptr);
        vkCmdDispatch(command, (extent.width + 7) / 8, (extent.height + 7) / 8, 1);
        VkMemoryBarrier hostRead{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
        hostRead.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        hostRead.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
        vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0,
                             1, &hostRead, 0, nullptr, 0, nullptr);
        VkCommandBuffer secondCommand = secondSubmission->commands();
        VkMemoryBarrier previousWrite{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
        previousWrite.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        previousWrite.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        vkCmdPipelineBarrier(secondCommand, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                     0, 1, &previousWrite, 0, nullptr, 0, nullptr);
        vkCmdBindPipeline(secondCommand, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
        vkCmdBindDescriptorSets(secondCommand, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 2, descriptors, 0, nullptr);
        vkCmdDispatch(secondCommand, (extent.width + 7) / 8, (extent.height + 7) / 8, 1);
        vkCmdPipelineBarrier(secondCommand, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0,
                     1, &hostRead, 0, nullptr, 0, nullptr);
        std::weak_ptr<const LLVKGlyphImage> lifetime = image;
        image.reset();
        ensure("retained during recording", !lifetime.expired());
        ensure("submit production owner", submission->submit(error));
        ensure("second consumer submit", secondSubmission->submit(error));
        ensure("commands inaccessible after submit", submission->commands() == VK_NULL_HANDLE);
        ensure("duplicate submit rejected", !submission->submit(error));
        ensure("image retained for submission", !lifetime.expired());
        ensure("fence-complete owner", submission->wait(UINT64_MAX,error) == LLVKGlyphSubmission::Status::Complete);
        ensure("first retirement cannot release second consumer", !lifetime.expired());
        ensure("second fence-complete owner", secondSubmission->wait(UINT64_MAX,error) == LLVKGlyphSubmission::Status::Complete);
        ensure("image retired after completion", lifetime.expired());
        ensure_equals("host invalidate", vmaInvalidateAllocation(device.allocator, outputAllocation, 0, expected.size()), VK_SUCCESS);
        const auto* actual = static_cast<const std::uint8_t*>(mapped.pMappedData);
        for (std::size_t index = 0; index < expected.size(); ++index)
            ensure_equals("sampled byte", actual[index], expected[index]);
        submission.reset();
        secondSubmission.reset();
        auto invalidQueue = device;
        invalidQueue.queueFamily = UINT32_MAX;
        ensure("invalid queue family rejected", !LLVKGlyphUpload::submit(invalidQueue, extent, expected, error));
        ensure("bad extent rejected", !LLVKGlyphUpload::submit(device, {0,2}, expected, error));
        ensure("bad payload rejected", !LLVKGlyphUpload::submit(device, {3,3}, expected, error));
        shutdown();
        ensure_equals("validation errors including teardown", validationErrors.load(), 0u);
    }
}