#include "linden_common.h"
#include "llvkcontext.h"
#include "llvkglyphupload.h"
#include "llvkuipacket.h"
#include "llvkskinimages.h"
#include "llvkimagepublication.h"
#include "llvkwidgetgpu.h"
#include "lltut.h"
#include <fstream>
#include <iterator>
#if defined(LLVK_CONTEXT_PRESENT_TEST)
#include <windows.h>
#endif

struct LLVKContextFrameTest
{
    static void initialize(LLVKContext& context)
    {
        context.mDevice = reinterpret_cast<VkDevice>(1);
        context.mSwapchain = reinterpret_cast<VkSwapchainKHR>(1);
        context.mPipeline2D[0][0] = reinterpret_cast<VkPipeline>(1);
        context.mSwapchainExtent = {32,32};
        context.mFrames[0].cmd = reinterpret_cast<VkCommandBuffer>(1);
        context.mFrames[0].inFlight = reinterpret_cast<VkFence>(1);
        context.mFrames[0].imageAvailable = reinterpret_cast<VkSemaphore>(1);
        context.mSwapchainImages.push_back(reinterpret_cast<VkImage>(1));
        context.mSwapchainViews.push_back(reinterpret_cast<VkImageView>(1));
        context.mImagePresentSem.push_back(reinterpret_cast<VkSemaphore>(2));
    }
    static void activate(LLVKContext& context) { context.mFrameActive = true; }
    static void detach(LLVKContext& context)
    {
        context.mDevice = VK_NULL_HANDLE;
        context.mSwapchain = VK_NULL_HANDLE;
        context.mPipeline2D[0][0] = VK_NULL_HANDLE;
        context.mFrames[0] = {};
        context.mSwapchainImages.clear();
        context.mSwapchainViews.clear();
        context.mImagePresentSem.clear();
    }
};

namespace
{
    struct Script
    {
        VkResult wait = VK_SUCCESS, acquire = VK_SUCCESS, resetCommand = VK_SUCCESS;
        VkResult begin = VK_SUCCESS, end = VK_SUCCESS, resetFence = VK_SUCCESS, submit = VK_SUCCESS, present = VK_SUCCESS;
        int waits = 0, acquisitions = 0, resets = 0, submissions = 0;
    };
    Script* script = nullptr;
    VKAPI_ATTR VkResult VKAPI_CALL waitFence(VkDevice,uint32_t,const VkFence*,VkBool32,uint64_t)
    { ++script->waits; return script->wait; }
    VKAPI_ATTR VkResult VKAPI_CALL acquireImage(VkDevice,VkSwapchainKHR,uint64_t,VkSemaphore,VkFence,uint32_t* index)
    { ++script->acquisitions; *index = 0; return script->acquire; }
    VKAPI_ATTR VkResult VKAPI_CALL resetCommand(VkCommandBuffer,VkCommandBufferResetFlags) { return script->resetCommand; }
    VKAPI_ATTR VkResult VKAPI_CALL beginCommand(VkCommandBuffer,const VkCommandBufferBeginInfo*) { return script->begin; }
    VKAPI_ATTR VkResult VKAPI_CALL endCommand(VkCommandBuffer) { return script->end; }
    VKAPI_ATTR VkResult VKAPI_CALL resetFence(VkDevice,uint32_t,const VkFence*) { ++script->resets; return script->resetFence; }
    VKAPI_ATTR VkResult VKAPI_CALL submit(VkQueue,uint32_t,const VkSubmitInfo*,VkFence) { ++script->submissions; return script->submit; }
    VKAPI_ATTR VkResult VKAPI_CALL present(VkQueue,const VkPresentInfoKHR*) { return script->present; }
    VKAPI_ATTR void VKAPI_CALL barrier(VkCommandBuffer,VkPipelineStageFlags,VkPipelineStageFlags,VkDependencyFlags,
        uint32_t,const VkMemoryBarrier*,uint32_t,const VkBufferMemoryBarrier*,uint32_t,const VkImageMemoryBarrier*) {}
    VKAPI_ATTR void VKAPI_CALL endRendering(VkCommandBuffer) {}
    template<class Function> struct Hook
    {
        Function& slot;
        Function original;
        Hook(Function& function, Function replacement) : slot(function), original(function) { slot = replacement; }
        ~Hook() { slot = original; }
    };
}

namespace tut
{
    struct context_data
    {
        Script state;
        LLVKContext context;
        Hook<PFN_vkWaitForFences> wait{vkWaitForFences,waitFence};
        Hook<PFN_vkAcquireNextImageKHR> acquire{vkAcquireNextImageKHR,acquireImage};
        Hook<PFN_vkResetCommandBuffer> reset{vkResetCommandBuffer,resetCommand};
        Hook<PFN_vkBeginCommandBuffer> begin{vkBeginCommandBuffer,beginCommand};
        Hook<PFN_vkEndCommandBuffer> end{vkEndCommandBuffer,endCommand};
        Hook<PFN_vkResetFences> resetFences{vkResetFences,resetFence};
        Hook<PFN_vkQueueSubmit> queueSubmit{vkQueueSubmit,submit};
        Hook<PFN_vkQueuePresentKHR> queuePresent{vkQueuePresentKHR,present};
        Hook<PFN_vkCmdPipelineBarrier> pipelineBarrier{vkCmdPipelineBarrier,barrier};
        Hook<PFN_vkCmdEndRendering> finishRendering{vkCmdEndRendering,endRendering};
        context_data() { script = &state; LLVKContextFrameTest::initialize(context); }
        ~context_data() { LLVKContextFrameTest::detach(context); script = nullptr; }
    };
    typedef test_group<context_data> context_group;
    typedef context_group::object context_object;
    context_group context_tests("llvkcontext");

    template<> template<> void context_object::test<1>()
    {
        set_test_name("out-of-date acquire leaves the completed fence untouched");
        state.acquire = VK_ERROR_OUT_OF_DATE_KHR;
        ensure("no command returned",context.begin2DFrame(0,0,0,1) == VK_NULL_HANDLE);
        ensure("recreation requested",context.frameResult() == LLVKContext::FrameResult::OutOfDate);
        ensure_equals("fence never reset",state.resets,0);
        ensure_equals("not submitted",state.submissions,0);
    }
    template<> template<> void context_object::test<2>()
    {
        set_test_name("recording failure cannot reset fence or reuse acquired semaphore");
        state.begin = VK_ERROR_OUT_OF_HOST_MEMORY;
        ensure("recording fails",context.begin2DFrame(0,0,0,1) == VK_NULL_HANDLE);
        ensure("fatal status",context.frameResult() == LLVKContext::FrameResult::Fatal);
        ensure_equals("fence unchanged",state.resets,0);
        ensure("error names operation",context.frameError().find("begin") != std::string::npos);
        ensure("second frame rejected",context.begin2DFrame(0,0,0,1) == VK_NULL_HANDLE);
        ensure_equals("acquire semaphore not reused",state.acquisitions,1);
    }
    template<> template<> void context_object::test<3>()
    {
        set_test_name("failed submission is terminal rather than waiting on its reset fence");
        LLVKContextFrameTest::activate(context);
        state.submit = VK_ERROR_DEVICE_LOST;
        ensure("submit failure",!context.end2DFrame());
        ensure_equals("fence reset immediately for submit",state.resets,1);
        ensure_equals("submission attempted once",state.submissions,1);
        ensure("fatal status",context.frameResult() == LLVKContext::FrameResult::Fatal);
        ensure("next frame refused",context.begin2DFrame(0,0,0,1) == VK_NULL_HANDLE);
        ensure_equals("never waits on failed submission",state.waits,0);
    }
    template<> template<> void context_object::test<4>()
    {
        set_test_name("command finalization failure leaves fence signaled");
        LLVKContextFrameTest::activate(context);
        state.end = VK_ERROR_OUT_OF_DEVICE_MEMORY;
        ensure("finalization failure",!context.end2DFrame());
        ensure_equals("no reset before finalized recording",state.resets,0);
        ensure_equals("no submission",state.submissions,0);
    }
    template<> template<> void context_object::test<5>()
    {
        set_test_name("presentation out-of-date is recoverable after successful submission");
        LLVKContextFrameTest::activate(context);
        state.present = VK_ERROR_OUT_OF_DATE_KHR;
        ensure("presentation requires recreation",!context.end2DFrame());
        ensure("not device-fatal",context.frameResult() == LLVKContext::FrameResult::OutOfDate);
        ensure_equals("submission precedes presentation",state.submissions,1);
    }
    template<> template<> void context_object::test<6>()
    {
        set_test_name("native UI packet rejects out-of-image scissor before allocation");
        LLVKContextFrameTest::activate(context);
        LLVKContext::UiVertex vertices[3]{};
        LLVKContext::UiDraw draw;
        draw.vertexCount = 3;
        draw.clip = {{0,0},{33,32}};
        ensure("invalid scissor rejected",!context.recordUiPacket(vertices,std::span(&draw,1)));
        ensure("packet failure terminal",context.frameResult() == LLVKContext::FrameResult::Fatal);
        ensure_equals("no premature submission",state.submissions,0);
    }
    template<> template<> void context_object::test<7>()
    {
        set_test_name("native UI packet cannot overwrite vertices already recorded in its frame");
        LLVKContextFrameTest::activate(context);
        ensure("empty packet accepted",context.recordUiPacket({},{}));
        ensure("second packet rejected",!context.recordUiPacket({},{}));
        ensure("explicit duplicate error",context.frameError().find("fresh active frame") != std::string::npos);
    }
    template<> template<> void context_object::test<9>()
    {
        set_test_name("native UI packet rejects invalid draws without changing painter order");
        LLVKUiPacket packet({100,80});
        std::string error;
        const VkRect2D clip{{2,3},{90,70}};
        ensure("solid appended",packet.solid({10,20,30,40},clip,{0.25f,0.5f,0.75f,0.5f},error));
        ensure_equals("solid triangle count",packet.vertices().size(),std::size_t(6));
        ensure_equals("bottom-up bottom becomes framebuffer y",packet.vertices()[0].positionY,60.f);
        ensure_equals("bottom-up top becomes framebuffer y",packet.vertices()[2].positionY,40.f);
        ensure_equals("alpha preserved",packet.vertices()[0].alpha,0.5f);
        ensure("out-of-target clip rejected",!packet.solid({0,0,20,20},{{99,0},{2,80}},{1,1,1,1},error));
        ensure_equals("failed draw preserves vertices",packet.vertices().size(),std::size_t(6));
        ensure_equals("failed draw preserves painter order",packet.draws().size(),std::size_t(1));
        ensure("inverted geometry rejected",!packet.solid({30,20,10,40},clip,{1,1,1,1},error));
        ensure("empty clip is no draw",packet.solid({0,0,20,20},{{0,0},{0,80}},{1,1,1,1},error));
        ensure_equals("empty clip has no vertices",packet.vertices().size(),std::size_t(6));
        ensure("second solid appended",packet.solid({40,20,60,40},clip,{1,0,0,1},error));
        ensure_equals("next draw follows prior range",packet.draws()[1].firstVertex,6u);
        packet.clear();
        ensure("clear resets frame data",packet.vertices().empty() && packet.draws().empty());
    }
#if defined(LLVK_CONTEXT_PRESENT_TEST)
    template<> template<> void context_object::test<8>()
    {
        set_test_name("native owned UI packets present on a GL-free Win32 surface");
        struct Window
        {
            HWND handle = CreateWindowExW(0,L"STATIC",L"Native Vulkan presentation validation",
                WS_OVERLAPPEDWINDOW,0,0,256,256,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
            ~Window() { if (handle) DestroyWindow(handle); }
        } window;
        ensure("Win32 window created without GL",window.handle != nullptr);
        LLVKContext renderer;
        std::string error;
        const bool instance = renderer.createInstance(true,error);
        ensure(error,instance);
        const auto surface = renderer.createSurface(window.handle,GetModuleHandleW(nullptr));
        ensure("native surface",surface != VK_NULL_HANDLE);
        if (!renderer.pickPhysicalDevice(surface,error) || !renderer.createDevice(surface,error))
        {
            vkDestroySurfaceKHR(renderer.instance(),surface,nullptr);
            ensure(error,false);
        }
        const bool swapchain = renderer.createSwapchain(surface,256,256,error);
        ensure(error,swapchain);
        const bool pipeline = renderer.create2DPipeline(error);
        ensure(error,pipeline);
        const LLVKContext::UiVertex triangle[]{
            {8,8,0,0,1,0,0,1},{160,8,0,0,0,1,0,1},{8,160,0,0,0,0,1,1}
        };
        LLVKContext::UiDraw draw;
        draw.vertexCount = 3;
        draw.clip = {{0,0},renderer.swapchainExtent()};
        for (int frame = 0; frame < 6; ++frame)
        {
            ensure("acquired real frame",renderer.begin2DFrame(0,0,0,1) != VK_NULL_HANDLE);
            ensure("native packet recorded",renderer.recordUiPacket(triangle,std::span(&draw,1)));
            ensure("native frame presented",renderer.end2DFrame());
        }
        renderer.waitIdle();
        const LLVKGlyphUpload::Device uploadDevice{renderer.physicalDevice(),renderer.device(),renderer.allocator(),
            renderer.graphicsQueue(),renderer.graphicsQueueFamily()};
        LLVKSkinFiles::Configuration skinConfiguration;
        skinConfiguration.skinBaseDirectory = std::filesystem::path(LLVK_CONTEXT_SKIN_FIXTURE).parent_path();
        LLVKSkinImages skin(std::make_shared<LLVKSkinFiles>(skinConfiguration));
        const bool skinLoaded = skin.loadDeclarations(error);
        ensure(error,skinLoaded);
        const auto logo = skin.image("login_fs_logo",error);
        ensure(error,logo != nullptr);
        auto upload = LLVKGlyphUpload::submit(uploadDevice,{logo->pixelWidth(),logo->pixelHeight()},logo->bottomUpRgba(),error,
            LLVKGlyphUpload::Sampling::SkinLinearClamp);
        ensure(error,upload != nullptr);
        ensure("texture published after upload completion",upload->wait(UINT64_MAX,error) == LLVKGlyphUpload::Status::Ready);
        draw.image = upload->published();
        const std::weak_ptr<const LLVKGlyphImage> lifetime = draw.image;
        upload.reset();
        LLVKUiPacket packet(renderer.swapchainExtent());
        ensure("native panel background",packet.solid({0,0,256,256},draw.clip,{0.16f,0.16f,0.16f,1},error));
        const auto imageDraw = packet.image(*logo,draw.image,{16,16,141,141},{},draw.clip,{1,1,1,1},error);
        ensure(error,imageDraw);
        ensure("unpublished image rejected",!packet.image(*logo,{}, {16,16,141,141},{},draw.clip,{1,1,1,1},error));
        ensure_equals("image follows background",packet.draws().size(),std::size_t(2));
        ensure_equals("bottom-up position converted once",packet.vertices()[6].positionY,float(renderer.swapchainExtent().height)-16);
        ensure("native focus mask",packet.image(*logo,draw.image,{150,16,240,106},{},draw.clip,{0,1,0,0.5f},error,true,
            LLVKContext::Blend2D::AddWithAlpha));
        ensure("explicit mask specialization",packet.draws().back().alphaMask);
        ensure("explicit glow blending",packet.draws().back().blend == LLVKContext::Blend2D::AddWithAlpha);
        std::ifstream fontStream(LLVK_CONTEXT_FONT_FIXTURE,std::ios::binary);
        ensure("native presentation font",fontStream.good());
        std::vector<std::uint8_t> fontBytes{std::istreambuf_iterator<char>(fontStream),std::istreambuf_iterator<char>()};
        auto font = LLVKFont::create({fontBytes,{}},{},false,error);
        ensure(error,font != nullptr);
        const auto line = font->layoutLine(U"Log In",0,6,{},error);
        ensure(error,line.has_value());
        const auto atlas = LLVKGlyphAtlas::prepare(*line,128,1024*1024,error);
        ensure(error,atlas.has_value());
        std::vector<std::shared_ptr<const LLVKGlyphImage>> pages;
        for (const auto& page : atlas->pages())
        {
            auto glyphUpload = LLVKGlyphUpload::submit(uploadDevice,{atlas->pageSize(),atlas->pageSize()},page.rgba,error);
            ensure(error,glyphUpload != nullptr);
            ensure("font page completed",glyphUpload->wait(UINT64_MAX,error) == LLVKGlyphUpload::Status::Ready);
            pages.push_back(glyphUpload->published());
        }
        LLVKTextDraw::Style textStyle;
        textStyle.shadow = LLVKTextDraw::Shadow::Hard;
        textStyle.shadowStrength = 1.f;
        const auto textDraw = packet.text(*atlas,pages,16,160,draw.clip,textStyle,error);
        ensure(error,textDraw);
        ensure("glyph packet has real vertices",packet.vertices().size() > 12);
        pages.clear();
        draw.image.reset();
        for (int frame = 0; frame < 2; ++frame)
        {
            ensure("texture frame acquired",renderer.begin2DFrame(0,0,0,1) != VK_NULL_HANDLE);
            ensure("owned image packet",renderer.recordUiPacket(packet.vertices(),packet.draws()));
            ensure("owned image presented",renderer.end2DFrame());
        }
        packet.clear();
        ensure("both frames retain image without producer",!lifetime.expired());
        ensure("first consuming slot completes",renderer.begin2DFrame(0,0,0,1) != VK_NULL_HANDLE);
        ensure("second consuming slot still retains image",!lifetime.expired());
        ensure("empty replacement packet",renderer.recordUiPacket({},{}));
        ensure("replacement presented",renderer.end2DFrame());
        ensure("second consuming slot completes",renderer.begin2DFrame(0,0,0,1) != VK_NULL_HANDLE);
        ensure("image retired after both slot waits",lifetime.expired());
        ensure("final empty packet",renderer.recordUiPacket({},{}));
        ensure("final frame presented",renderer.end2DFrame());
        renderer.waitIdle();
        std::cout << "Native presentation device: " << renderer.deviceName() << std::endl;
        LLVKImagePublication publication(uploadDevice);
        ensure("asynchronous image begins",publication.advance(logo,error));
        ensure("not published before completion observation",!publication.current().image && publication.pending());
        ensure_equals("test waits only to observe completion",vkQueueWaitIdle(renderer.graphicsQueue()),VK_SUCCESS);
        const bool published = publication.advance(logo,error);
        ensure(error,published);
        ensure("matching source and image published",publication.current().source == logo && publication.current().image);
        const auto oldImage = publication.current().image;
        const std::uint8_t browserPixel[]{1,2,3,4};
        auto browserFrame = LLVKWidgetImage::browserFrame(1,1,browserPixel,error);
        ensure(error,browserFrame != nullptr);
        ensure("replacement upload begins",publication.advance(browserFrame,error));
        ensure("prior image valid during upload",publication.current().image == oldImage);
        ensure_equals("replacement upload completes",vkQueueWaitIdle(renderer.graphicsQueue()),VK_SUCCESS);
        ensure("replacement publishes",publication.advance(browserFrame,error));
        ensure("new image paired to browser frame",publication.current().source == browserFrame && publication.current().image != oldImage);
        ensure("invalidation clears publication",publication.advance({},error) && !publication.current().image);
        LLVKWidgetPaint paint;
        const LLVKWidgetTree::Rect paintClip{0,0,static_cast<std::int32_t>(renderer.swapchainExtent().width),static_cast<std::int32_t>(renderer.swapchainExtent().height)};
        paint.commands.push_back({1,{8,8,133,133},paintClip,{1,1,1,1},logo});
        paint.commands.push_back({2,{},paintClip,{1,1,1,1},{},*line});
        LLVKWidgetGpu widgetGpu(uploadDevice);
        LLVKUiPacket widgetPacket(renderer.swapchainExtent());
        ensure("widget resources await publication",widgetGpu.prepare(paint,renderer.swapchainExtent(),widgetPacket,error) == LLVKWidgetGpu::Status::Pending);
        ensure("test completes widget upload fences",widgetGpu.waitPendingUploads(5000000000ull,error));
        const auto ready = widgetGpu.prepare(paint,renderer.swapchainExtent(),widgetPacket,error);
        ensure(error,ready == LLVKWidgetGpu::Status::Ready);
        const auto imageIdentity = widgetPacket.draws()[0].image;
        ensure("unchanged paint ready without reupload",widgetGpu.prepare(paint,renderer.swapchainExtent(),widgetPacket,error) == LLVKWidgetGpu::Status::Ready);
        ensure("image cache identity retained",widgetPacket.draws()[0].image == imageIdentity);
        ensure("widget frame acquired",renderer.begin2DFrame(0,0,0,1) != VK_NULL_HANDLE);
        ensure("widget packet recorded",renderer.recordUiPacket(widgetPacket.vertices(),widgetPacket.draws()));
        ensure("widget packet presented",renderer.end2DFrame());
        renderer.waitIdle();
        paint.commands.resize(1);
        paint.commands[0].streamingImage = true;
        paint.commands[0].image = browserFrame;
        ensure("browser stream initially pending",widgetGpu.prepare(paint,renderer.swapchainExtent(),widgetPacket,error) == LLVKWidgetGpu::Status::Pending);
        ensure("test completes browser upload fence",widgetGpu.waitPendingUploads(5000000000ull,error));
        paint.commands[0].image = LLVKWidgetImage::browserFrame(1,1,browserPixel,error);
        const auto streamed=widgetGpu.prepare(paint,renderer.swapchainExtent(),widgetPacket,error);
        ensure("new frame publication status="+std::to_string(static_cast<int>(streamed))+": "+error,streamed == LLVKWidgetGpu::Status::Ready);
        ensure("browser packet owns a completed image",widgetPacket.draws()[0].image != nullptr);
        LLVKWidgetTree scrollTree;
        LLVKWidgetTree::Params scrollView;
        scrollView.rect={0,0,120,100};
        LLVKControl::Params scrollControl;
        scrollControl.font=std::move(font);
        LLVKWidgetTree::ScrollContainerParams scrollParams;
        scrollParams.size=16;
        scrollParams.scrollbarControl=scrollControl;
        scrollParams.vertical.decreaseControl=scrollParams.vertical.increaseControl=scrollControl;
        scrollParams.horizontal.decreaseControl=scrollParams.horizontal.increaseControl=scrollControl;
        const auto scrollRoot=scrollTree.createScrollContainer(scrollView,scrollControl,scrollParams,0,error);
        ensure(error,scrollRoot.has_value());
        scrollView.rect={0,0,100,300};
        LLVKPanel::Params scrollPanel;
        scrollPanel.backgroundVisible=scrollPanel.backgroundOpaque=true;
        scrollPanel.opaqueColor=LLVKColor{0.3f,0.5f,0.7f,1};
        const auto document=scrollTree.createPanel(scrollView,scrollControl,scrollPanel,*scrollRoot,error);
        ensure(error,document.has_value());
        ensure("native GPU scroll document attached",scrollTree.attachScrollContent(*scrollRoot,*document,0,error));
        const auto scrollPaint=LLVKWidgetPaint::prepare(scrollTree,*scrollRoot,{},error);
        ensure(error,scrollPaint.has_value());
        const auto scrollReady=widgetGpu.prepare(*scrollPaint,renderer.swapchainExtent(),widgetPacket,error);
        ensure(error,scrollReady==LLVKWidgetGpu::Status::Ready);
        ensure("scroll frame acquired",renderer.begin2DFrame(0,0,0,1)!=VK_NULL_HANDLE);
        ensure("native scroll packet recorded",renderer.recordUiPacket(widgetPacket.vertices(),widgetPacket.draws()));
        ensure("native scroll packet presented",renderer.end2DFrame());
        renderer.waitIdle();
        scrollView.rect={0,0,180,70};
        scrollControl.initialValue="[https://example.com/notes Release Notes]";
        LLVKPlainControl::Params linkedText;
        linkedText.parseWebLinks=linkedText.selectable=true;
        const auto textId=scrollTree.createPlainText(scrollView,scrollControl,linkedText,0,error);
        ensure(error,textId.has_value());
        auto linkPaint=LLVKWidgetPaint::prepare(scrollTree,*textId,{},error);
        ensure(error,linkPaint.has_value());
        ensure("link glyph upload begins",widgetGpu.prepare(*linkPaint,renderer.swapchainExtent(),widgetPacket,error)==LLVKWidgetGpu::Status::Pending);
        ensure("test completes link glyph upload fences",widgetGpu.waitPendingUploads(5000000000ull,error));
        ensure("native link packet ready",widgetGpu.prepare(*linkPaint,renderer.swapchainExtent(),widgetPacket,error)==LLVKWidgetGpu::Status::Ready);
        ensure("select linked display text",scrollTree.selectAllPlainText(*textId));
        linkPaint=LLVKWidgetPaint::prepare(scrollTree,*textId,{},error);
        ensure(error,linkPaint.has_value());
        auto linkReady=widgetGpu.prepare(*linkPaint,renderer.swapchainExtent(),widgetPacket,error);
        if (linkReady==LLVKWidgetGpu::Status::Pending)
        {
            ensure("test completes selected text upload fence",widgetGpu.waitPendingUploads(5000000000ull,error));
            linkReady=widgetGpu.prepare(*linkPaint,renderer.swapchainExtent(),widgetPacket,error);
        }
        ensure(error,linkReady==LLVKWidgetGpu::Status::Ready);
        ensure("selected link frame acquired",renderer.begin2DFrame(0,0,0,1)!=VK_NULL_HANDLE);
        ensure("selected link packet recorded",renderer.recordUiPacket(widgetPacket.vertices(),widgetPacket.draws()));
        ensure("selected link packet presented",renderer.end2DFrame());
        renderer.waitIdle();
        scrollView.rect={0,0,150,24};
        scrollControl.initialValue=LLSD(2.f);
        LLVKWidgetTree::SpinnerParams spinnerParams;
        spinnerParams.maximum=10.f;
        spinnerParams.buttonControl.font=spinnerParams.editorControl.font=scrollControl.font;
        spinnerParams.editor.textColor=LLVKColor{1,1,1,1};
        const auto spinner=scrollTree.createSpinner(scrollView,scrollControl,spinnerParams,0,error);
        ensure(error,spinner.has_value());
        ensure("numeric control advances",scrollTree.stepSpinner(*spinner,true,{},error));
        const auto spinnerPaint=LLVKWidgetPaint::prepare(scrollTree,*spinner,{},error);
        ensure(error,spinnerPaint.has_value());
        auto spinnerReady=widgetGpu.prepare(*spinnerPaint,renderer.swapchainExtent(),widgetPacket,error);
        if (spinnerReady==LLVKWidgetGpu::Status::Pending)
        {
            ensure("test completes spinner upload fences",widgetGpu.waitPendingUploads(5000000000ull,error));
            spinnerReady=widgetGpu.prepare(*spinnerPaint,renderer.swapchainExtent(),widgetPacket,error);
        }
        ensure(error,spinnerReady==LLVKWidgetGpu::Status::Ready);
        ensure("spinner frame acquired",renderer.begin2DFrame(0,0,0,1)!=VK_NULL_HANDLE);
        ensure("spinner packet recorded",renderer.recordUiPacket(widgetPacket.vertices(),widgetPacket.draws()));
        ensure("spinner packet presented",renderer.end2DFrame());
        renderer.waitIdle();
        scrollView.rect={0,0,80,60};
        scrollControl.initialValue.reset();
        LLVKWidgetTree::ColorSwatchParams swatchParams;
        swatchParams.color=LLVKColor{0.2f,0.6f,0.8f,0.5f};
        swatchParams.label="Tint";
        swatchParams.alphaBackground=skin.image("color_swatch_alpha.tga",error);
        ensure(error,swatchParams.alphaBackground!=nullptr);
        ensure("original checker pixels",scrollTree.registerImage(skin.image("Checker",error)));
        const auto swatch=scrollTree.createColorSwatch(scrollView,scrollControl,swatchParams,0,error);
        ensure(error,swatch.has_value());
        const auto swatchPaint=LLVKWidgetPaint::prepare(scrollTree,*swatch,{},error);
        ensure(error,swatchPaint.has_value());
        auto swatchReady=widgetGpu.prepare(*swatchPaint,renderer.swapchainExtent(),widgetPacket,error);
        if (swatchReady==LLVKWidgetGpu::Status::Pending)
        {
            ensure("test completes swatch uploads",widgetGpu.waitPendingUploads(5000000000ull,error));
            swatchReady=widgetGpu.prepare(*swatchPaint,renderer.swapchainExtent(),widgetPacket,error);
        }
        ensure(error,swatchReady==LLVKWidgetGpu::Status::Ready);
        ensure("swatch frame acquired",renderer.begin2DFrame(0,0,0,1)!=VK_NULL_HANDLE);
        ensure("swatch packet recorded",renderer.recordUiPacket(widgetPacket.vertices(),widgetPacket.draws()));
        ensure("swatch packet presented",renderer.end2DFrame());
        renderer.waitIdle();
        LLVKUiPacket trianglePacket(renderer.swapchainExtent());
        const VkRect2D triangleClip{{0,0},renderer.swapchainExtent()};
        ensure("native luminance marker triangle",trianglePacket.triangle({20,30,26,24,26,36},triangleClip,{0.75f,0.75f,0.75f,1},error));
        ensure_equals("triangle has three vertices",trianglePacket.vertices().size(),std::size_t(3));
        ensure_equals("triangle Y is converted once",trianglePacket.vertices()[0].positionY,float(renderer.swapchainExtent().height)-30.f);
        const auto verticesBefore=trianglePacket.vertices().size();
        ensure("invalid triangle clip rejected",!trianglePacket.triangle({0,0,1,0,0,1},{{-1,0},{1,1}},{1,1,1,1},error));
        ensure_equals("rejected triangle preserves packet",trianglePacket.vertices().size(),verticesBefore);
        ensure("marker frame acquired",renderer.begin2DFrame(0,0,0,1)!=VK_NULL_HANDLE);
        ensure("marker triangle recorded",renderer.recordUiPacket(trianglePacket.vertices(),trianglePacket.draws()));
        ensure("marker triangle presented",renderer.end2DFrame());
        renderer.waitIdle();
    }
#endif
}