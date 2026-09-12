#include "llvktextdraw.h"
#include "glyph_draw_vert_spv.h"
#include "glyph_draw_frag_spv.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace
{
    struct Vertex
    {
        float position[3];
        float uv[2];
        std::array<std::uint8_t, 4> color;
    };
    static_assert(sizeof(Vertex) == 24);
    bool check(VkResult result, const char* operation, std::string& error)
    {
        if (result == VK_SUCCESS) return true;
        error = std::string(operation) + ": " + std::to_string(result);
        return false;
    }
}

struct LLVKTextPipeline::Impl
{
    LLVKGlyphUpload::Device device;
    VkRenderPass pass = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptors = VK_NULL_HANDLE;
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkShaderModule vertex = VK_NULL_HANDLE;
    VkShaderModule fragment = VK_NULL_HANDLE;
    VkFormat depthFormat = VK_FORMAT_UNDEFINED;
    ~Impl()
    {
        if (pipeline) vkDestroyPipeline(device.logical, pipeline, nullptr);
        if (layout) vkDestroyPipelineLayout(device.logical, layout, nullptr);
        if (descriptors) vkDestroyDescriptorSetLayout(device.logical, descriptors, nullptr);
        if (vertex) vkDestroyShaderModule(device.logical, vertex, nullptr);
        if (fragment) vkDestroyShaderModule(device.logical, fragment, nullptr);
        if (pass) vkDestroyRenderPass(device.logical, pass, nullptr);
    }
};

struct LLVKTextDraw::Impl
{
    std::shared_ptr<LLVKTextPipeline> pipeline;
    std::vector<std::shared_ptr<const LLVKGlyphImage>> images;
    LLVKGlyphUpload::Device device;
    VkBuffer vertices = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    ~Impl()
    {
        if (framebuffer) vkDestroyFramebuffer(device.logical, framebuffer, nullptr);
        if (vertices) vmaDestroyBuffer(device.allocator, vertices, allocation);
    }
};

LLVKTextPipeline::LLVKTextPipeline(std::unique_ptr<Impl> impl) : mImpl(std::move(impl)) {}
LLVKTextPipeline::~LLVKTextPipeline() = default;
LLVKTextDraw::LLVKTextDraw(std::unique_ptr<Impl> impl) : mImpl(std::move(impl)) {}
LLVKTextDraw::~LLVKTextDraw() = default;

std::shared_ptr<LLVKTextPipeline> LLVKTextPipeline::create(const LLVKGlyphUpload::Device& device,
                                                        VkFormat colorFormat, std::string& error,
                                                        VkFormat depthFormat, VkCompareOp depthCompare,
                                                        bool depthWrite)
{
    error.clear();
    if (!device.logical || !device.physical || !device.allocator)
    {
        error = "Invalid native text pipeline device";
        return nullptr;
    }
    VkFormatProperties properties{};
    vkGetPhysicalDeviceFormatProperties(device.physical, colorFormat, &properties);
    constexpr auto required = VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT;
    if ((properties.optimalTilingFeatures & required) != required)
    {
        error = "Text target does not support color attachment blending";
        return nullptr;
    }
    auto impl = std::make_unique<Impl>();
    impl->device = device;
    impl->depthFormat = depthFormat;
    if (depthFormat != VK_FORMAT_UNDEFINED)
    {
        vkGetPhysicalDeviceFormatProperties(device.physical, depthFormat, &properties);
        if (!(properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) ||
            depthCompare < VK_COMPARE_OP_NEVER || depthCompare > VK_COMPARE_OP_ALWAYS)
        {
            error = "Unsupported text depth attachment or comparison";
            return nullptr;
        }
    }
    VkAttachmentDescription attachments[2]{};
    auto& attachment = attachments[0];
    attachment.format = colorFormat;
    attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment.initialLayout = attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    VkAttachmentReference color{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkAttachmentReference depthReference{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color;
    if (depthFormat != VK_FORMAT_UNDEFINED)
    {
        attachments[1] = attachment;
        attachments[1].format = depthFormat;
        attachments[1].initialLayout = attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        subpass.pDepthStencilAttachment = &depthReference;
    }
    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    if (depthFormat != VK_FORMAT_UNDEFINED)
    {
        dependency.srcStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        dependency.dstStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        dependency.srcAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        dependency.dstAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    }
    VkRenderPassCreateInfo pass{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    pass.attachmentCount = pass.subpassCount = pass.dependencyCount = 1;
    if (depthFormat != VK_FORMAT_UNDEFINED) pass.attachmentCount = 2;
    pass.pAttachments = attachments;
    pass.pSubpasses = &subpass;
    pass.pDependencies = &dependency;
    if (!check(vkCreateRenderPass(device.logical, &pass, nullptr, &impl->pass), "text render pass", error)) return nullptr;
    VkDescriptorSetLayoutBinding binding{0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1,
                                         VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT, nullptr};
    VkDescriptorSetLayoutCreateInfo descriptors{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    descriptors.bindingCount = 1;
    descriptors.pBindings = &binding;
    if (!check(vkCreateDescriptorSetLayout(device.logical, &descriptors, nullptr, &impl->descriptors), "text descriptors", error)) return nullptr;
    VkPushConstantRange push{VK_SHADER_STAGE_VERTEX_BIT, 0, 128};
    VkPipelineLayoutCreateInfo layout{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    layout.setLayoutCount = layout.pushConstantRangeCount = 1;
    layout.pSetLayouts = &impl->descriptors;
    layout.pPushConstantRanges = &push;
    if (!check(vkCreatePipelineLayout(device.logical, &layout, nullptr, &impl->layout), "text layout", error)) return nullptr;
    VkShaderModuleCreateInfo module{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    module.codeSize = sizeof(glyph_draw_vert_spv);
    module.pCode = glyph_draw_vert_spv;
    if (!check(vkCreateShaderModule(device.logical, &module, nullptr, &impl->vertex), "text vertex shader", error)) return nullptr;
    module.codeSize = sizeof(glyph_draw_frag_spv);
    module.pCode = glyph_draw_frag_spv;
    if (!check(vkCreateShaderModule(device.logical, &module, nullptr, &impl->fragment), "text fragment shader", error)) return nullptr;
    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType = stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[0].module = impl->vertex;
    stages[1].module = impl->fragment;
    stages[0].pName = stages[1].pName = "main";
    VkVertexInputBindingDescription vertexBinding{0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX};
    VkVertexInputAttributeDescription attributes[]{
        {0,0,VK_FORMAT_R32G32B32_SFLOAT,offsetof(Vertex,position)},
        {1,0,VK_FORMAT_R32G32_SFLOAT,offsetof(Vertex,uv)},
        {2,0,VK_FORMAT_R8G8B8A8_UNORM,offsetof(Vertex,color)}};
    VkPipelineVertexInputStateCreateInfo vertex{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vertex.vertexBindingDescriptionCount = 1;
    vertex.pVertexBindingDescriptions = &vertexBinding;
    vertex.vertexAttributeDescriptionCount = 3;
    vertex.pVertexAttributeDescriptions = attributes;
    VkPipelineInputAssemblyStateCreateInfo assembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkPipelineViewportStateCreateInfo viewport{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    viewport.viewportCount = viewport.scissorCount = 1;
    VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    raster.polygonMode = VK_POLYGON_MODE_FILL;
    raster.cullMode = VK_CULL_MODE_NONE;
    raster.lineWidth = 1.f;
    VkPipelineMultisampleStateCreateInfo samples{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    samples.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    VkPipelineDepthStencilStateCreateInfo depth{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    depth.depthTestEnable = depthFormat != VK_FORMAT_UNDEFINED;
    depth.depthWriteEnable = depth.depthTestEnable && depthWrite;
    depth.depthCompareOp = depthCompare;
    VkPipelineColorBlendAttachmentState blend{};
    blend.blendEnable = VK_TRUE;
    blend.srcColorBlendFactor = blend.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    blend.dstColorBlendFactor = blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blend.colorBlendOp = blend.alphaBlendOp = VK_BLEND_OP_ADD;
    blend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo blending{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    blending.attachmentCount = 1;
    blending.pAttachments = &blend;
    VkDynamicState dynamicStates[]{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamic{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dynamic.dynamicStateCount = 2;
    dynamic.pDynamicStates = dynamicStates;
    VkGraphicsPipelineCreateInfo create{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    create.stageCount = 2;
    create.pStages = stages;
    create.pVertexInputState = &vertex;
    create.pInputAssemblyState = &assembly;
    create.pViewportState = &viewport;
    create.pRasterizationState = &raster;
    create.pMultisampleState = &samples;
    create.pDepthStencilState = &depth;
    create.pColorBlendState = &blending;
    create.pDynamicState = &dynamic;
    create.layout = impl->layout;
    create.renderPass = impl->pass;
    if (!check(vkCreateGraphicsPipelines(device.logical, VK_NULL_HANDLE, 1, &create, nullptr, &impl->pipeline), "text pipeline", error)) return nullptr;
    return std::shared_ptr<LLVKTextPipeline>(new LLVKTextPipeline(std::move(impl)));
}

std::optional<std::vector<LLVKTextDraw::Quad>> LLVKTextDraw::prepare(const LLVKGlyphAtlas& atlas,
    const Style& style, std::string& error)
{
    error.clear();
    if (!std::isfinite(style.depth) || !std::isfinite(style.shadowStrength) || style.shadowStrength < 0 || style.shadowStrength > 1 ||
        (style.shadow != Shadow::None && style.shadow != Shadow::Hard && style.shadow != Shadow::Soft))
    { error = "Invalid native text style"; return std::nullopt; }
    std::vector<Quad> quads;
    for (const auto& placement : atlas.placements())
    {
        if (!placement.page) continue;
        if (quads.size()*6 > 1024*1024-36) { error = "Native text vertex budget exceeded"; return std::nullopt; }
        auto color = style.color;
        if (placement.draw.glyph->raster.encoding == LLVKFontFace::PixelEncoding::PremultipliedSrgbRgba8)
            color[0] = color[1] = color[2] = 255;
        const auto quad = [&](float offsetX, float offsetY, const std::array<std::uint8_t,4>& tint)
        {
            const auto& draw = placement.draw;
            quads.push_back({*placement.page,draw.left+offsetX,draw.bottom+offsetY,draw.right+offsetX,draw.top+offsetY,
                placement.leftU,placement.bottomV,placement.rightU,placement.topV,tint});
        };
        if (style.syntheticBold) { quad(0,0,color); quad(1,0,color); }
        else
        {
            if (style.shadow != Shadow::None)
            {
                auto shadow = style.shadowColor;
                shadow[3] = static_cast<std::uint8_t>(color[3]*style.shadowStrength*(style.shadow == Shadow::Soft ? 0.3f : 1.f));
                if (style.shadow == Shadow::Hard) quad(1,-1,shadow);
                else { quad(-1,-1,shadow); quad(1,-1,shadow); quad(1,1,shadow); quad(-1,1,shadow); quad(0,-2,shadow); }
            }
            quad(0,0,color);
        }
    }
    return quads;
}

bool LLVKTextDraw::record(LLVKGlyphSubmission& submission, std::shared_ptr<LLVKTextPipeline> pipeline,
                         const LLVKGlyphAtlas& atlas,
                         std::vector<std::shared_ptr<const LLVKGlyphImage>> images,
                         const View& view, const Style& style, std::string& error)
{
    error.clear();
    if (!pipeline || !submission.commands() || !view.target || !view.extent.width || !view.extent.height ||
        view.clip.offset.x < 0 || view.clip.offset.y < 0 ||
        std::uint64_t(view.clip.offset.x) + view.clip.extent.width > view.extent.width ||
        std::uint64_t(view.clip.offset.y) + view.clip.extent.height > view.extent.height ||
        images.size() != atlas.pages().size() || !std::isfinite(style.depth) ||
        !std::isfinite(style.shadowStrength) || style.shadowStrength < 0.f || style.shadowStrength > 1.f ||
        (style.shadow != Shadow::None && style.shadow != Shadow::Hard && style.shadow != Shadow::Soft))
    {
        error = "Invalid native text draw inputs";
        return false;
    }
    for (float value : view.projection) if (!std::isfinite(value)) { error = "Invalid text projection"; return false; }
    for (float value : view.textureTransform) if (!std::isfinite(value)) { error = "Invalid text UV transform"; return false; }
    if ((pipeline->mImpl->depthFormat != VK_FORMAT_UNDEFINED) != (view.depthTarget != VK_NULL_HANDLE))
    {
        error = "Text pipeline/target depth policy mismatch";
        return false;
    }
    for (const auto& image : images)
        if (!image || image->extent().width != atlas.pageSize() || image->extent().height != atlas.pageSize())
        { error = "Text atlas/image extent mismatch"; return false; }
    struct Run { std::size_t page; std::uint32_t first; std::uint32_t count; };
    std::vector<Vertex> vertices;
    std::vector<Run> runs;
    const auto prepared = prepare(atlas,style,error);
    if (!prepared) return false;
    for (const auto& quad : *prepared)
    {
        const auto first = static_cast<std::uint32_t>(vertices.size());
        const Vertex corners[]{
            {{quad.right,quad.top,style.depth},{quad.rightU,quad.topV},quad.color},
            {{quad.left,quad.top,style.depth},{quad.leftU,quad.topV},quad.color},
            {{quad.left,quad.bottom,style.depth},{quad.leftU,quad.bottomV},quad.color},
            {{quad.right,quad.bottom,style.depth},{quad.rightU,quad.bottomV},quad.color}};
        for (auto index : {0,1,2,0,2,3}) vertices.push_back(corners[index]);
        if (!runs.empty() && runs.back().page == quad.page) runs.back().count += 6;
        else runs.push_back({quad.page,first,6});
    }
    if (vertices.empty() || !view.clip.extent.width || !view.clip.extent.height) return true;
    auto impl = std::make_unique<Impl>();
    impl->device = pipeline->mImpl->device;
    impl->pipeline = std::move(pipeline);
    impl->images = std::move(images);
    VkBufferCreateInfo buffer{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    buffer.size = vertices.size() * sizeof(Vertex);
    buffer.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    VmaAllocationCreateInfo host{};
    host.usage = VMA_MEMORY_USAGE_AUTO;
    host.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
    VmaAllocationInfo mapped{};
    if (!check(vmaCreateBuffer(impl->device.allocator, &buffer, &host, &impl->vertices, &impl->allocation, &mapped), "text vertex allocation", error)) return false;
    std::memcpy(mapped.pMappedData, vertices.data(), static_cast<std::size_t>(buffer.size));
    if (!check(vmaFlushAllocation(impl->device.allocator, impl->allocation, 0, buffer.size), "text vertex flush", error)) return false;
    VkFramebufferCreateInfo framebuffer{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    const VkImageView attachments[]{view.target, view.depthTarget};
    framebuffer.renderPass = impl->pipeline->mImpl->pass;
    framebuffer.attachmentCount = view.depthTarget ? 2 : 1;
    framebuffer.pAttachments = attachments;
    framebuffer.width = view.extent.width;
    framebuffer.height = view.extent.height;
    framebuffer.layers = 1;
    if (!check(vkCreateFramebuffer(impl->device.logical, &framebuffer, nullptr, &impl->framebuffer), "text framebuffer", error)) return false;
    auto draw = std::shared_ptr<LLVKTextDraw>(new LLVKTextDraw(std::move(impl)));
    if (!submission.retainText(draw, draw->mImpl->device, draw->mImpl->images, error)) return false;
    const auto command = submission.commands();
    VkRenderPassBeginInfo begin{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    begin.renderPass = draw->mImpl->pipeline->mImpl->pass;
    begin.framebuffer = draw->mImpl->framebuffer;
    begin.renderArea.extent = view.extent;
    vkCmdBeginRenderPass(command, &begin, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_GRAPHICS, draw->mImpl->pipeline->mImpl->pipeline);
    VkViewport viewport{0,0,static_cast<float>(view.extent.width),static_cast<float>(view.extent.height),0,1};
    vkCmdSetViewport(command, 0, 1, &viewport);
    vkCmdSetScissor(command, 0, 1, &view.clip);
    const auto pipelineLayout = draw->mImpl->pipeline->mImpl->layout;
    vkCmdPushConstants(command, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, 64, view.projection.data());
    vkCmdPushConstants(command, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 64, 64, view.textureTransform.data());
    const VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(command, 0, 1, &draw->mImpl->vertices, &offset);
    for (const auto& run : runs)
    {
        const auto descriptor = draw->mImpl->images[run.page]->descriptor();
        vkCmdBindDescriptorSets(command, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptor, 0, nullptr);
        vkCmdDraw(command, run.count, 1, run.first, 0);
    }
    vkCmdEndRenderPass(command);
    return true;
}