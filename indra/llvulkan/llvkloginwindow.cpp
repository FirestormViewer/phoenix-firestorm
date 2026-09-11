#include "llvkloginwindow.h"
#include "llvkwidgetgpu.h"
#include "llvkaudio.h"
#include <windows.h>
#include <windowsx.h>
#include <chrono>
#include <shellapi.h>
#include "llstring.h"
#include "lluri.h"
#include <intrin.h>
#include <psapi.h>
#include <cstring>
#include <curl/curl.h>

namespace
{
    struct WindowState
    {
        HWND window = nullptr;
        LLVKLoginUi* ui = nullptr;
        LLVKBrowser* browser = nullptr;
        LLVKWidgetPaint::Input input;
        std::function<void()> audioVolumeChanged;
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
            tree.setInputModifiers({bool(GetKeyState(VK_SHIFT)&0x8000),bool(GetKeyState(VK_CONTROL)&0x8000),bool(GetKeyState(VK_MENU)&0x8000)});
            const auto focused = tree.keyboardFocus();
            const auto* focus = tree.get(focused);
            if (ui->modalNotice())
            {
                if (message==WM_KEYDOWN || message==WM_SYSKEYDOWN)
                {
                    ui->noticeKey(parameter==VK_RETURN,(GetKeyState(VK_SHIFT)&0x8000) ||
                        (GetKeyState(VK_CONTROL)&0x8000) || (GetKeyState(VK_MENU)&0x8000),error);
                    return 0;
                }
                if (message==WM_CHAR || message==WM_SYSCHAR || message==WM_KEYUP || message==WM_SYSKEYUP || message==WM_MOUSEWHEEL) return 0;
            }
            if (message == WM_ACTIVATEAPP)
            {
                input.editor.applicationFocused = parameter != 0;
                if (!parameter) ui->menu().dismiss();
                if (audioVolumeChanged) audioVolumeChanged();
                return 0;
            }
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
                if (message == WM_MOUSEMOVE) SetCursor(LoadCursorW(nullptr,IDC_ARROW));
                if (GetKeyState(VK_SHIFT) & 0x8000) event.modifiers |= 1;
                tree.advanceTime(event.time,error);
                if (ui->modalNotice())
                {
                    tree.routePointer(ui->modalNotice(),event,error);
                    if (tree.mouseCapture()) SetCapture(window); else if (GetCapture()==window) ReleaseCapture();
                    return 0;
                }
                if (!tree.mouseCapture() && ui->menu().pointer(event))
                {
                    if (ui->menu().open() && tree.topControl()) tree.setTopControl(0,error);
                    return 0;
                }
                if (!ui->floaterPointer(event,error)) tree.routePointer(ui->root(),event,error);
                if (tree.mouseCapture()) SetCapture(window); else if (GetCapture() == window) ReleaseCapture();
                if (message != WM_MOUSEMOVE) keystroke = std::chrono::steady_clock::now();
                if (message == WM_MOUSEMOVE && browser && !tree.topControl() && !ui->pointOverFloater(event.x,event.y))
                {
                    const auto rectangle = tree.screenRect(ui->find("login_html"),error);
                    if (rectangle && event.x >= rectangle->left && event.x < rectangle->right && event.y >= rectangle->bottom && event.y < rectangle->top)
                        browser->hover(event.x-rectangle->left,rectangle->top-1-event.y,error);
                }
                return 0;
            }
            if (message == WM_MOUSEWHEEL)
            {
                if (ui->menu().open()) return 0;
                POINT point{GET_X_LPARAM(data),GET_Y_LPARAM(data)};
                ScreenToClient(window,&point);
                const auto bottom=static_cast<std::int32_t>(height)-1-point.y;
                const auto clicks=-GET_WHEEL_DELTA_WPARAM(parameter)/WHEEL_DELTA;
                if (!ui->floaterWheel(point.x,bottom,clicks,error) && !ui->pointOverFloater(point.x,bottom))
                    tree.routeWheel(ui->root(),point.x,bottom,clicks,false,error);
                return 0;
            }
            if (message == WM_KEYDOWN || message == WM_SYSKEYDOWN)
            {
                std::string shortcut;
                if (parameter >= 'A' && parameter <= 'Z') shortcut.assign(1,static_cast<char>(parameter));
                else if (parameter >= VK_F1 && parameter <= VK_F12) shortcut = "F"+std::to_string(parameter-VK_F1+1);
                if (!shortcut.empty() && ui->menu().shortcut(shortcut,bool(GetKeyState(VK_CONTROL)&0x8000),
                    bool(GetKeyState(VK_SHIFT)&0x8000),bool(GetKeyState(VK_MENU)&0x8000))) return 0;
                if (parameter == VK_F10) { ui->menu().key(LLVKLoginMenu::Key::Activate); return 0; }
                if (ui->menu().open())
                {
                    using Key = LLVKLoginMenu::Key;
                    switch (parameter)
                    {
                        case VK_ESCAPE: ui->menu().key(Key::Escape); break;
                        case VK_LEFT: ui->menu().key(Key::Left); break;
                        case VK_RIGHT: ui->menu().key(Key::Right); break;
                        case VK_UP: ui->menu().key(Key::Up); break;
                        case VK_DOWN: ui->menu().key(Key::Down); break;
                        case VK_RETURN: ui->menu().key(Key::Return); break;
                    }
                    return 0;
                }
            }
            if (ui->menu().open() && (message == WM_CHAR || message == WM_KEYUP || message == WM_SYSCHAR || message == WM_SYSKEYUP)) return 0;
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
            if (message==WM_CHAR && focus && focus->colorSwatch && parameter==' ')
            { tree.showColorSwatchPicker(focused,true,error); return 0; }
            if (message == WM_KEYDOWN || message == WM_SYSKEYDOWN)
            {
                keystroke = std::chrono::steady_clock::now();
                LLVKLineEditor::Modifiers modifiers{bool(GetKeyState(VK_SHIFT)&0x8000),bool(GetKeyState(VK_CONTROL)&0x8000),bool(GetKeyState(VK_MENU)&0x8000)};
                if (focus && focus->plainText && focus->plainText->params.selectable && modifiers.control)
                {
                    if (parameter == 'A') { tree.selectAllPlainText(focused); return 0; }
                    if (parameter == 'C') { tree.copyPlainText(focused,error); return 0; }
                }
                if (focus && focus->plainText)
                {
                    std::optional<LLVKWidgetTree::ScrollKey> textKey;
                    using Key=LLVKWidgetTree::ScrollKey;
                    switch (parameter)
                    {
                    case VK_LEFT: textKey=Key::Left; break;
                    case VK_RIGHT: textKey=Key::Right; break;
                    case VK_UP: textKey=Key::Up; break;
                    case VK_DOWN: textKey=Key::Down; break;
                    case VK_HOME: textKey=Key::Home; break;
                    case VK_END: textKey=Key::End; break;
                    case VK_PRIOR: textKey=Key::PageUp; break;
                    case VK_NEXT: textKey=Key::PageDown; break;
                    }
                    if (textKey)
                        for (auto parent=focused; tree.get(parent); parent=tree.get(parent)->parent)
                            if (tree.get(parent)->textEditor)
                            {
                                if (tree.textEditorKey(parent,*textKey,modifiers,error) || !error.empty()) return 0;
                                break;
                            }
                }
                if (parameter == VK_LEFT || parameter == VK_RIGHT || parameter == VK_UP || parameter == VK_DOWN)
                {
                    const auto key = parameter == VK_LEFT ? LLVKWidgetTree::ScrollKey::Left : parameter == VK_RIGHT ? LLVKWidgetTree::ScrollKey::Right :
                        parameter == VK_UP ? LLVKWidgetTree::ScrollKey::Up : LLVKWidgetTree::ScrollKey::Down;
                    for (auto parent = focused; tree.get(parent); )
                    {
                        const auto ancestor = tree.get(parent)->parent;
                        if (tree.get(parent)->slider)
                        { tree.sliderStep(parent,parameter==VK_RIGHT || parameter==VK_UP ? 1 : -1,error); return 0; }
                        if (tree.get(parent)->radioGroup && !modifiers.shift && !modifiers.control && !modifiers.alt)
                        { tree.radioKey(parent,parameter==VK_RIGHT || parameter==VK_DOWN,error); return 0; }
                        if (tree.get(parent)->tabContainer && tree.tabContainerKey(parent,key,modifiers,error)) return 0;
                        if (!tree.get(parent) || !error.empty()) return 0;
                        parent = ancestor;
                    }
                }
                const auto dialog=ui->activeFloater();
                const auto focusRoot=dialog ? dialog : ui->root();
                if (parameter == VK_TAB) { tree.moveFocus(focusRoot,!modifiers.shift,false,error); return 0; }
                if (parameter == VK_ESCAPE && dialog && !tree.topControl()) { ui->closeFloater(error); return 0; }
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
                if (parameter == VK_RETURN) tree.panelKey(focusRoot,LLVKWidgetTree::PanelKey::Return,modifiers,error);
                else if (parameter == VK_ESCAPE) tree.panelKey(focusRoot,LLVKWidgetTree::PanelKey::Escape,modifiers,error);
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
    WindowState state;
    auto uiConfiguration=configuration.ui;
    const auto save=uiConfiguration.savePreferences;
    uiConfiguration.savePreferences=[&state,save](const auto& changes,std::string& problem)
    {
        const auto backend=changes.find("RenderBackend");
        const bool restart=backend!=changes.end() && backend->second.asString()!="Vulkan";
        if (restart)
        {
            const auto selected=backend->second.asString();
            if (selected!="OpenGL" && selected!="Zink") { problem="Invalid renderer preference"; return false; }
            const auto prompt=ll_convert<std::wstring>("Save preferences and shut down now?\n\nThe renderer will be switched to "+selected+" when you launch Vulkanstorm again.");
            if (MessageBoxW(state.window,prompt.c_str(),L"Change Renderer",MB_OKCANCEL|MB_ICONQUESTION)!=IDOK)
            { problem="Renderer change cancelled. Preferences were not saved."; return false; }
        }
        if (!save || !save(changes,problem)) { if (problem.empty()) problem="Native preference persistence is unavailable"; return false; }
        if (restart) state.close=true;
        return true;
    };
    auto ui = LLVKLoginUi::create(uiConfiguration,error);
    if (!ui) return false;
    if (configuration.bindServices) configuration.bindServices(*ui);
    state.ui = ui.get();
    struct DetachUi { WindowState& state; ~DetachUi() { state.ui=nullptr; } } detachUi{state};
    ui->menu().bind("File.Quit",[&state](const auto&,const auto&) { state.close = true; });
    const auto openUrl=[&state](const std::string& url)
    {
        const LLURI uri(url);
        auto scheme = uri.scheme();
        LLStringUtil::toLower(scheme);
        if ((scheme != "https" && scheme != "http" && scheme != "ftp") || uri.hostName().empty()) return;
        const auto wide = ll_convert<std::wstring>(url);
        if (MessageBoxW(state.window,(L"Open this page in your web browser?\n\n"+wide).c_str(),L"Vulkanstorm",MB_YESNO|MB_ICONQUESTION) == IDYES)
            if (reinterpret_cast<INT_PTR>(ShellExecuteW(state.window,L"open",wide.c_str(),nullptr,nullptr,SW_SHOWNORMAL)) <= 32)
                MessageBoxW(state.window,L"The web browser could not be opened.",L"Vulkanstorm",MB_OK|MB_ICONERROR);
    };
    ui->setOpenUrl(openUrl);
    ui->setPointerCursor([](bool hand) { SetCursor(LoadCursorW(nullptr,hand ? IDC_HAND : IDC_ARROW)); });
    ui->menu().bind("PromptShowURL",[openUrl](const auto&,const std::string& parameter)
    {
        const auto separator = parameter.find(',');
        if (separator != std::string::npos) openUrl(parameter.substr(separator+1));
    });
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
    LLVKAudio audio;
    std::string audioError;
    if (!audio.start(ui->tree().setting("NoAudio").value_or(LLSD(false)).asBoolean(),audioError))
        LL_WARNS("NativeAudio") << audioError << LL_ENDL;
    struct AudioBindings
    {
        LLVKWidgetTree& tree;
        WindowState& window;
        std::vector<std::uint64_t> subscriptions;
        ~AudioBindings()
        {
            window.audioVolumeChanged={};
            for (const auto subscription : subscriptions) tree.unsubscribeSetting(subscription);
        }
    } audioBindings{ui->tree(),state};
    state.audioVolumeChanged=[&]
    {
        LLVKAudio::Volume volume;
        volume.master=static_cast<float>(ui->tree().setting("AudioLevelMaster").value_or(LLSD(1.f)).asReal());
        volume.muted=ui->tree().setting("MuteAudio").value_or(LLSD(false)).asBoolean();
        volume.muteWhenInactive=ui->tree().setting("MuteWhenMinimized").value_or(LLSD(false)).asBoolean();
        volume.windowActive=state.input.editor.applicationFocused;
        std::string problem;
        if (!audio.setVolume(volume,problem)) LL_WARNS("NativeAudio") << problem << LL_ENDL;
    };
    for (const auto name : {"AudioLevelMaster","MuteAudio","MuteWhenMinimized"})
        if (const auto subscription=ui->tree().subscribeSetting(name,[&](const LLSD&,const LLSD&) { state.audioVolumeChanged(); }))
            audioBindings.subscriptions.push_back(*subscription);
    state.audioVolumeChanged();
    auto clipboard = LLVKClipboard::forWindow(state.window,error);
    if (!clipboard) return false;
    ui->setDialogClipboard(clipboard);
    ui->tree().setClipboard(std::move(clipboard));
    LLVKContext renderer;
    if (!renderer.createInstance(configuration.validation,error)) return false;
    const auto surface = renderer.createSurface(state.window,windowClass.hInstance);
    if (!surface) { error = "Native login Vulkan surface creation failed"; return false; }
    if (!renderer.pickPhysicalDevice(surface,error) || !renderer.createDevice(surface,error))
    { vkDestroySurfaceKHR(renderer.instance(),surface,nullptr); return false; }
    if (!renderer.createSwapchain(surface,state.width,state.height,error) || !renderer.create2DPipeline(error)) return false;
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(renderer.physicalDevice(),&properties);
    LLSD aboutInfo;
    for (const auto name : {"CPU","MEMORY_MB","USED_RAM","OS_VERSION","VRAM_BUDGET","LIBCURL_VERSION","J2C_VERSION",
        "AUDIO_DRIVER_VERSION","LIBCEF_VERSION","LIBVLC_VERSION","RLV_VERSION","VOICE_VERSION","VIEWER_VERSION_LL",
        "FONT","MODE","THEME","SKIN","RENDERQUALITY"}) aboutInfo[name]="Unavailable in native startup";
    aboutInfo["VIEWER_VERSION"]=LLSD::emptyArray();
    for (const auto part : {LLVK_VIEWER_MAJOR,LLVK_VIEWER_MINOR,LLVK_VIEWER_PATCH,LLVK_VIEWER_BUILD})
        aboutInfo["VIEWER_VERSION"].append(part);
    aboutInfo["VIEWER_VERSION_LL"]=LLVK_VIEWER_UPSTREAM;
    aboutInfo["BUILD_DATE"]=__DATE__;
    aboutInfo["BUILD_TIME"]=__TIME__;
    aboutInfo["ADDRESS_SIZE"]=static_cast<int>(sizeof(void*)*8);
    aboutInfo["CHANNEL"]=LLVK_VIEWER_CHANNEL;
    aboutInfo["BUILD_TYPE"]="";
#ifdef USE_AVX2_OPTIMIZATION
    aboutInfo["SIMD"]="AVX2";
#elif defined(USE_AVX_OPTIMIZATION)
    aboutInfo["SIMD"]="AVX";
#else
    aboutInfo["SIMD"]="SSE2";
#endif
    int cpu[4]{};
    __cpuid(cpu,static_cast<int>(0x80000000u));
    if (static_cast<unsigned>(cpu[0])>=0x80000004u)
    {
        char brand[49]{};
        for (unsigned leaf=0; leaf<3; ++leaf)
        { __cpuid(cpu,static_cast<int>(0x80000002u+leaf)); std::memcpy(brand+16*leaf,cpu,sizeof(cpu)); }
        std::string name(brand);
        LLStringUtil::trim(name);
        aboutInfo["CPU"]=name;
    }
    MEMORYSTATUSEX memory{sizeof(MEMORYSTATUSEX)};
    if (GlobalMemoryStatusEx(&memory)) aboutInfo["MEMORY_MB"]=std::to_string(memory.ullTotalPhys/(1024*1024));
    PROCESS_MEMORY_COUNTERS processMemory{sizeof(PROCESS_MEMORY_COUNTERS)};
    if (GetProcessMemoryInfo(GetCurrentProcess(),&processMemory,sizeof(processMemory)))
        aboutInfo["USED_RAM"]=std::to_string(processMemory.WorkingSetSize/(1024*1024));
    OSVERSIONINFOW os{sizeof(OSVERSIONINFOW)};
    using GetVersion=LONG(WINAPI*)(OSVERSIONINFOW*);
    const auto getVersion=reinterpret_cast<GetVersion>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"),"RtlGetVersion"));
    if (getVersion && getVersion(&os)==0)
        aboutInfo["OS_VERSION"]="Windows "+std::to_string(os.dwMajorVersion)+"."+std::to_string(os.dwMinorVersion)+" (Build "+std::to_string(os.dwBuildNumber)+") 64-bit";
    aboutInfo["CONCURRENCY"]=static_cast<int>(std::thread::hardware_concurrency());
    aboutInfo["GRAPHICS_CARD"]=properties.deviceName;
    VkPhysicalDeviceMemoryProperties deviceMemory{};
    vkGetPhysicalDeviceMemoryProperties(renderer.physicalDevice(),&deviceMemory);
    std::uint64_t localBytes=0;
    for (std::uint32_t heap=0; heap<deviceMemory.memoryHeapCount; ++heap)
        if (deviceMemory.memoryHeaps[heap].flags&VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) localBytes+=deviceMemory.memoryHeaps[heap].size;
    aboutInfo["GRAPHICS_CARD_MEMORY"]=std::to_string(localBytes/(1024*1024));
    aboutInfo["GRAPHICS_CARD_MEMORY_DETECTED"]=aboutInfo["GRAPHICS_CARD_MEMORY"];
    aboutInfo["RENDERING_API"]="Vulkan";
    aboutInfo["AUDIO_DRIVER_VERSION"]=audio.driverName();
    aboutInfo["LIBCURL_VERSION"]=curl_version();
    aboutInfo["RENDERING_API_VERSION"]=std::to_string(VK_VERSION_MAJOR(properties.apiVersion))+"."+
        std::to_string(VK_VERSION_MINOR(properties.apiVersion))+"."+std::to_string(VK_VERSION_PATCH(properties.apiVersion));
    aboutInfo["COMPILER"]="MSVC";
    aboutInfo["COMPILER_VERSION"]=_MSC_VER;
    aboutInfo["WINDOW_WIDTH"]=static_cast<int>(state.width);
    aboutInfo["WINDOW_HEIGHT"]=static_cast<int>(state.height);
    for (const auto& [field,setting] : {std::pair{"DRAW_DISTANCE","RenderFarClip"},std::pair{"LOD","RenderVolumeLODFactor"},
        std::pair{"FONT_SIZE","FSFontSizeAdjustment"},std::pair{"FONT_SCREEN_DPI","FontScreenDPI"},std::pair{"UI_SCALE_FACTOR","UIScaleFactor"}})
        aboutInfo[field]=ui->tree().setting(setting).value_or(LLSD("Unavailable"));
    aboutInfo["BANDWIDTH"]=ui->tree().setting("ThrottleBandwidthKBPS").value_or(LLSD("Unavailable"));
    aboutInfo["SKIN"]=configuration.ui.skin.skin;
    aboutInfo["THEME"]=configuration.ui.skin.theme;
    if (!ui->setAboutInfo(aboutInfo,error)) return false;
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
            ui.setDialogClipboard({});
        }
    } detach{state,*ui};
    auto browserConfiguration = configuration.browser;
    const auto browserId = ui->find("login_html");
    auto browserRect = ui->tree().screenRect(browserId,error);
    if (!browserRect) return false;
    browserConfiguration.width = browserRect->right-browserRect->left;
    browserConfiguration.height = browserRect->top-browserRect->bottom;
    if (!browser.start(browserConfiguration,error) || !browser.navigate(configuration.loginPage,error)) return false;
    aboutInfo["LIBCEF_VERSION"]=browser.versionInfo(error);
    if (!error.empty() || !ui->setAboutInfo(aboutInfo,error)) return false;
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
        if (!ui->advanceNotices(state.elapsed(),error)) return false;
        MSG message;
        while (PeekMessageW(&message,nullptr,0,0,PM_REMOVE))
        { if (message.message == WM_QUIT) state.close = true; TranslateMessage(&message); DispatchMessageW(&message); }
        if (!state.error.empty()) { error = state.error; return false; }
        if (const auto problem=ui->takeDialogError(); !problem.empty())
            MessageBoxW(state.window,ll_convert<std::wstring>(problem).c_str(),L"Vulkanstorm",MB_OK|MB_ICONWARNING);
        if (!browser.update(error)) return false;
        for (const auto& event : browser.takeEvents())
            if (event.kind == LLVKBrowser::EventKind::LoadError) { error = "Native login page failed: "+event.detail; return false; }
        if (state.resize && state.width && state.height)
        {
            ui->menu().dismiss();
            if (!renderer.createSwapchain(surface,state.width,state.height,error) ||
                !ui->tree().reshape(ui->root(),renderer.swapchainExtent().width,renderer.swapchainExtent().height,error)) return false;
            if (!ui->tree().prepareLayoutStacks(ui->root(),0,error)) return false;
            browserRect = ui->tree().screenRect(browserId,error);
            if (!browserRect || !browser.resize(browserRect->right-browserRect->left,browserRect->top-browserRect->bottom,error)) return false;
            state.resize = false;
            aboutInfo["WINDOW_WIDTH"]=static_cast<int>(renderer.swapchainExtent().width);
            aboutInfo["WINDOW_HEIGHT"]=static_cast<int>(renderer.swapchainExtent().height);
            if (!ui->setAboutInfo(aboutInfo,error)) return false;
        }
        const auto now = std::chrono::steady_clock::now();
        state.input.button.frameDelta = std::chrono::duration<float>(now-previous).count();
        previous = now;
        state.input.button.spaceDown = bool(GetKeyState(VK_SPACE)&0x8000);
        state.input.button.returnDown = bool(GetKeyState(VK_RETURN)&0x8000);
        state.input.editor.secondsSinceKeystroke = std::chrono::duration<double>(now-state.keystroke).count();
        state.input.browsers[browserId] = browser.surface().frame();
        ui->tree().setInputModifiers({bool(GetKeyState(VK_SHIFT)&0x8000),bool(GetKeyState(VK_CONTROL)&0x8000),bool(GetKeyState(VK_MENU)&0x8000)});
        ui->tree().advanceTime(state.elapsed(),error);
        if (state.width && state.height)
        {
            const auto paint = ui->preparePaint(state.input,error);
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
    return audio.stop(error);
}