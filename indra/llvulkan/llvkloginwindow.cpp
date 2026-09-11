#include "llvkloginwindow.h"
#include "llvkwidgetgpu.h"
#include <windows.h>
#include <windowsx.h>
#include <chrono>

namespace
{
    struct WindowState
    {
        HWND window = nullptr;
        LLVKLoginUi* ui = nullptr;
        LLVKBrowser* browser = nullptr;
        LLVKWidgetPaint::Input input;
        std::string error;
        bool close = false, resize = true;
        std::uint32_t width = 1024, height = 768;
        char32_t surrogate = 0;
        std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now(), keystroke = start;
        ~WindowState() { if (window) DestroyWindow(window); }
        double elapsed() const { return std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count(); }
        LRESULT message(UINT message, WPARAM parameter, LPARAM data)
        {
            if (message == WM_CLOSE) { close = true; return 0; }
            if (message == WM_SIZE) { width = LOWORD(data); height = HIWORD(data); resize = true; return 0; }
            if (!ui) return DefWindowProcW(window,message,parameter,data);
            auto& tree = ui->tree();
            const auto focused = tree.keyboardFocus();
            const auto* focus = tree.get(focused);
            if (message == WM_ACTIVATEAPP) { input.editor.applicationFocused = parameter != 0; return 0; }
            if (message == WM_CAPTURECHANGED)
            { if (reinterpret_cast<HWND>(data) != window) tree.setMouseCapture(0,error); return 0; }
            if (message == WM_MOUSEMOVE || message == WM_LBUTTONDOWN || message == WM_LBUTTONUP || message == WM_LBUTTONDBLCLK)
            {
                LLVKWidgetTree::PointerEvent event;
                event.x = GET_X_LPARAM(data); event.y = static_cast<std::int32_t>(height)-1-GET_Y_LPARAM(data);
                event.time = elapsed();
                event.kind = message == WM_MOUSEMOVE ? LLVKWidgetTree::PointerKind::Hover : message == WM_LBUTTONDOWN ?
                    LLVKWidgetTree::PointerKind::LeftDown : message == WM_LBUTTONDBLCLK ? LLVKWidgetTree::PointerKind::DoubleClick : LLVKWidgetTree::PointerKind::LeftUp;
                input.button.mouseX = event.x; input.button.mouseY = event.y;
                if (GetKeyState(VK_SHIFT) & 0x8000) event.modifiers |= 1;
                tree.advanceTime(event.time,error);
                tree.routePointer(ui->root(),event,error);
                if (tree.mouseCapture()) SetCapture(window); else if (GetCapture() == window) ReleaseCapture();
                if (message != WM_MOUSEMOVE) keystroke = std::chrono::steady_clock::now();
                if (message == WM_MOUSEMOVE && browser && !tree.topControl())
                {
                    const auto rectangle = tree.screenRect(ui->find("login_html"),error);
                    if (rectangle && event.x >= rectangle->left && event.x < rectangle->right && event.y >= rectangle->bottom && event.y < rectangle->top)
                        browser->hover(event.x-rectangle->left,rectangle->top-1-event.y,error);
                }
                return 0;
            }
            if (message == WM_MOUSEWHEEL)
            {
                POINT point{GET_X_LPARAM(data),GET_Y_LPARAM(data)};
                ScreenToClient(window,&point);
                tree.routeWheel(ui->root(),point.x,static_cast<std::int32_t>(height)-1-point.y,-GET_WHEEL_DELTA_WPARAM(parameter)/WHEEL_DELTA,false,error);
                return 0;
            }
            if ((message == WM_KEYDOWN || message == WM_KEYUP || message == WM_CHAR || message == WM_SYSKEYDOWN || message == WM_SYSKEYUP) && focus && focus->browser)
            {
                if (browser) browser->keyboard(message,static_cast<std::uint32_t>(parameter),static_cast<std::uint64_t>(data),error);
                return 0;
            }
            if (message == WM_CHAR && focus && focus->lineEditor)
            {
                char32_t character = static_cast<char32_t>(parameter);
                if (character >= 0xd800 && character <= 0xdbff) { surrogate = character; return 0; }
                if (character >= 0xdc00 && character <= 0xdfff)
                { if (!surrogate) return 0; character = 0x10000+((surrogate-0xd800)<<10)+(character-0xdc00); }
                surrogate = 0;
                if (character >= 32 && character != 127 && !(GetKeyState(VK_CONTROL)&0x8000))
                    tree.lineEditorUnicode(focused,character,error);
                keystroke = std::chrono::steady_clock::now();
                return 0;
            }
            if (message == WM_KEYDOWN)
            {
                keystroke = std::chrono::steady_clock::now();
                LLVKLineEditor::Modifiers modifiers{bool(GetKeyState(VK_SHIFT)&0x8000),bool(GetKeyState(VK_CONTROL)&0x8000),bool(GetKeyState(VK_MENU)&0x8000)};
                if (parameter == VK_TAB) { tree.moveFocus(ui->root(),!modifiers.shift,false,error); return 0; }
                if (focus && focus->lineEditor)
                {
                    if (modifiers.control)
                    {
                        if (parameter == 'A') { tree.selectLineEditorAll(focused,error); return 0; }
                        if (parameter == 'C') { tree.copyLineEditor(focused,false,error); return 0; }
                        if (parameter == 'X') { tree.cutLineEditor(focused,error); return 0; }
                        if (parameter == 'V') { tree.pasteLineEditor(focused,false,error); return 0; }
                    }
                    std::optional<LLVKLineEditor::Key> key;
                    switch (parameter)
                    {
                        case VK_LEFT: key = LLVKLineEditor::Key::Left; break;
                        case VK_RIGHT: key = LLVKLineEditor::Key::Right; break;
                        case VK_HOME: key = LLVKLineEditor::Key::Home; break;
                        case VK_END: key = LLVKLineEditor::Key::End; break;
                        case VK_BACK: key = LLVKLineEditor::Key::Backspace; break;
                        case VK_DELETE: tree.deleteLineEditor(focused,error); return 0;
                        case VK_RETURN: key = LLVKLineEditor::Key::Return; break;
                        case VK_ESCAPE: key = LLVKLineEditor::Key::Escape; break;
                        case VK_INSERT: key = LLVKLineEditor::Key::Insert; break;
                        case VK_UP: key = LLVKLineEditor::Key::Up; break;
                        case VK_DOWN: key = LLVKLineEditor::Key::Down; break;
                    }
                    if (key) { tree.lineEditorKey(focused,*key,modifiers,error); return 0; }
                }
                if (parameter == VK_RETURN) tree.panelKey(ui->root(),LLVKWidgetTree::PanelKey::Return,modifiers,error);
                else if (parameter == VK_ESCAPE) tree.panelKey(ui->root(),LLVKWidgetTree::PanelKey::Escape,modifiers,error);
                return 0;
            }
            return DefWindowProcW(window,message,parameter,data);
        }
        static LRESULT CALLBACK procedure(HWND window,UINT message,WPARAM parameter,LPARAM data)
        {
            auto* state = reinterpret_cast<WindowState*>(GetWindowLongPtrW(window,GWLP_USERDATA));
            if (message == WM_NCCREATE)
            {
                state = static_cast<WindowState*>(reinterpret_cast<CREATESTRUCTW*>(data)->lpCreateParams);
                state->window = window;
                SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(state));
            }
            if (!state) return DefWindowProcW(window,message,parameter,data);
            try { return state->message(message,parameter,data); }
            catch (...) { state->close = true; state->error = "Native window event failed"; return 0; }
        }
    };
}

bool LLVKLoginWindow::run(const Configuration& configuration,std::string& error)
{
    error.clear();
    auto ui = LLVKLoginUi::create(configuration.ui,error);
    if (!ui) return false;
    if (configuration.bindServices) configuration.bindServices(*ui);
    WindowState state;
    state.ui = ui.get();
    WNDCLASSW windowClass{};
    windowClass.style = CS_DBLCLKS;
    windowClass.lpfnWndProc = WindowState::procedure;
    windowClass.hInstance = GetModuleHandleW(nullptr);
    windowClass.hCursor = LoadCursorW(nullptr,IDC_ARROW);
    windowClass.lpszClassName = L"VulkanstormNativeLogin";
    if (!RegisterClassW(&windowClass) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
    { error = "Native login window class registration failed"; return false; }
    RECT rectangle{0,0,1024,768};
    AdjustWindowRect(&rectangle,WS_OVERLAPPEDWINDOW,FALSE);
    if (!CreateWindowExW(0,windowClass.lpszClassName,L"Vulkanstorm",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,
        rectangle.right-rectangle.left,rectangle.bottom-rectangle.top,nullptr,nullptr,windowClass.hInstance,&state))
    { error = "Native login window creation failed"; return false; }
    auto clipboard = LLVKClipboard::forWindow(state.window,error);
    if (!clipboard) return false;
    ui->tree().setClipboard(std::move(clipboard));
    LLVKContext renderer;
    if (!renderer.createInstance(configuration.validation,error)) return false;
    const auto surface = renderer.createSurface(state.window,windowClass.hInstance);
    if (!surface) { error = "Native login Vulkan surface creation failed"; return false; }
    if (!renderer.pickPhysicalDevice(surface,error) || !renderer.createDevice(surface,error))
    { vkDestroySurfaceKHR(renderer.instance(),surface,nullptr); return false; }
    if (!renderer.createSwapchain(surface,state.width,state.height,error) || !renderer.create2DPipeline(error)) return false;
    LLVKWidgetGpu gpu({renderer.physicalDevice(),renderer.device(),renderer.allocator(),renderer.graphicsQueue(),renderer.graphicsQueueFamily()});
    LLVKBrowser browser;
    state.browser = &browser;
    struct DetachWindow
    {
        WindowState& state;
        LLVKLoginUi& ui;
        ~DetachWindow()
        {
            state.browser = nullptr;
            state.ui = nullptr;
            ui.tree().setEvents(ui.find("login_html"),{});
            ui.tree().setClipboard({});
        }
    } detach{state,*ui};
    auto browserConfiguration = configuration.browser;
    const auto browserId = ui->find("login_html");
    auto browserRect = ui->tree().screenRect(browserId,error);
    if (!browserRect) return false;
    browserConfiguration.width = browserRect->right-browserRect->left;
    browserConfiguration.height = browserRect->top-browserRect->bottom;
    if (!browser.start(browserConfiguration,error) || !browser.navigate(configuration.loginPage,error)) return false;
    LLVKWidgetTree::Events events;
    events.pointer = [&](auto id,const auto& event)
    {
        const auto* node = ui->tree().get(id);
        if (!node) return;
        const auto height = node->params.rect.top-node->params.rect.bottom;
        if (event.kind == LLVKWidgetTree::PointerKind::LeftDown)
        { ui->tree().setKeyboardFocus(id,false,false,state.error); ui->tree().setMouseCapture(id,state.error); browser.pointer(event.x,height-1-event.y,0,true,state.error); }
        else if (event.kind == LLVKWidgetTree::PointerKind::LeftUp)
        { browser.pointer(event.x,height-1-event.y,0,false,state.error); ui->tree().setMouseCapture(0,state.error); }
    };
    ui->tree().setEvents(browserId,std::move(events));
    ShowWindow(state.window,SW_SHOW);
    LLVKUiPacket packet(renderer.swapchainExtent());
    std::uint32_t frames = 0;
    auto previous = std::chrono::steady_clock::now();
    while (!state.close)
    {
        MSG message;
        while (PeekMessageW(&message,nullptr,0,0,PM_REMOVE))
        { if (message.message == WM_QUIT) state.close = true; TranslateMessage(&message); DispatchMessageW(&message); }
        if (!state.error.empty()) { error = state.error; return false; }
        if (!browser.update(error)) return false;
        for (const auto& event : browser.takeEvents())
            if (event.kind == LLVKBrowser::EventKind::LoadError) { error = "Native login page failed: "+event.detail; return false; }
        if (state.resize && state.width && state.height)
        {
            if (!renderer.createSwapchain(surface,state.width,state.height,error) ||
                !ui->tree().reshape(ui->root(),renderer.swapchainExtent().width,renderer.swapchainExtent().height,error)) return false;
            if (!ui->tree().prepareLayoutStacks(ui->root(),0,error)) return false;
            browserRect = ui->tree().screenRect(browserId,error);
            if (!browserRect || !browser.resize(browserRect->right-browserRect->left,browserRect->top-browserRect->bottom,error)) return false;
            state.resize = false;
        }
        const auto now = std::chrono::steady_clock::now();
        state.input.button.frameDelta = std::chrono::duration<float>(now-previous).count();
        previous = now;
        state.input.button.spaceDown = bool(GetKeyState(VK_SPACE)&0x8000);
        state.input.button.returnDown = bool(GetKeyState(VK_RETURN)&0x8000);
        state.input.editor.secondsSinceKeystroke = std::chrono::duration<double>(now-state.keystroke).count();
        state.input.browsers[browserId] = browser.surface().frame();
        ui->tree().advanceTime(state.elapsed(),error);
        if (state.width && state.height)
        {
            const auto paint = LLVKWidgetPaint::prepare(ui->tree(),ui->root(),state.input,error);
            if (!paint) return false;
            const auto ready = gpu.prepare(*paint,renderer.swapchainExtent(),packet,error);
            if (ready == LLVKWidgetGpu::Status::Failed) return false;
            if (ready == LLVKWidgetGpu::Status::Ready)
            {
                if (renderer.begin2DFrame(0.16f,0.16f,0.16f,1))
                {
                    if (!renderer.recordUiPacket(packet.vertices(),packet.draws())) { error = renderer.frameError(); return false; }
                    if (!renderer.end2DFrame() && renderer.frameResult() != LLVKContext::FrameResult::OutOfDate)
                    { error = renderer.frameError(); return false; }
                    if (configuration.stopAfterFrames && ++frames >= configuration.stopAfterFrames) state.close = true;
                }
                if (renderer.frameResult() == LLVKContext::FrameResult::OutOfDate) state.resize = true;
                else if (renderer.frameResult() == LLVKContext::FrameResult::Fatal) { error = renderer.frameError(); return false; }
            }
        }
        MsgWaitForMultipleObjectsEx(0,nullptr,16,QS_ALLINPUT,MWMO_INPUTAVAILABLE);
    }
    renderer.waitIdle();
    return true;
}