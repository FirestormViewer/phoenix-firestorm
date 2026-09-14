#ifndef NOMINMAX
#define NOMINMAX
#endif
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#undef WINVER
#define WINVER 0x0A00
#include <windows.h>
#include <dwmapi.h>
#include <d3d11.h>
#include <windows.graphics.capture.interop.h>
#include <windows.graphics.directx.direct3d11.interop.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <mutex>
#include <vector>

using namespace winrt;
using namespace winrt::Windows::Graphics::Capture;
using namespace winrt::Windows::Graphics::DirectX;
using namespace winrt::Windows::Graphics::DirectX::Direct3D11;

struct Capture
{
    std::uint32_t width=0,height=0;
    std::vector<std::uint8_t> rgba;
};

Capture capture(HWND window)
{
    init_apartment(apartment_type::multi_threaded);
    if (!GraphicsCaptureSession::IsSupported() || !IsWindow(window) || !IsWindowVisible(window) || IsIconic(window))
        throw std::runtime_error("Capture requires a visible, non-minimized window and Windows Graphics Capture support");
    RECT client{},bounds{};
    POINT origin{};
    check_bool(GetClientRect(window,&client));
    check_bool(ClientToScreen(window,&origin));
    check_hresult(DwmGetWindowAttribute(window,DWMWA_EXTENDED_FRAME_BOUNDS,&bounds,sizeof(bounds)));
    const int left=origin.x-bounds.left,top=origin.y-bounds.top;
    const int width=client.right-client.left,height=client.bottom-client.top;
    if (left<0 || top<0 || width<=0 || height<=0) throw std::runtime_error("Invalid client capture bounds");
    com_ptr<ID3D11Device> device;
    com_ptr<ID3D11DeviceContext> context;
    check_hresult(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        nullptr,0,D3D11_SDK_VERSION,device.put(),nullptr,context.put()));
    com_ptr<IInspectable> inspectable;
    check_hresult(CreateDirect3D11DeviceFromDXGIDevice(device.as<IDXGIDevice>().get(),inspectable.put()));
    const auto runtimeDevice=inspectable.as<IDirect3DDevice>();
    auto interop=get_activation_factory<GraphicsCaptureItem,IGraphicsCaptureItemInterop>();
    GraphicsCaptureItem item{nullptr};
    check_hresult(interop->CreateForWindow(window,guid_of<GraphicsCaptureItem>(),put_abi(item)));
    const auto size=item.Size();
    if (size.Width!=bounds.right-bounds.left || size.Height!=bounds.bottom-bounds.top ||
        left+width>size.Width || top+height>size.Height)
        throw std::runtime_error("Capture item and physical window bounds disagree");
    auto pool=Direct3D11CaptureFramePool::CreateFreeThreaded(runtimeDevice,DirectXPixelFormat::B8G8R8A8UIntNormalized,2,size);
    auto session=pool.CreateCaptureSession(item);
    session.IsCursorCaptureEnabled(false);
    std::promise<Capture> promise;
    auto result=promise.get_future();
    std::mutex mutex;
    bool completed=false;
    const auto token=pool.FrameArrived([&](const auto& sender,const auto&)
    {
        const std::lock_guard lock(mutex);
        if (completed) return;
        try
        {
            const auto frame=sender.TryGetNextFrame();
            if (!frame) return;
            if (frame.ContentSize().Width!=size.Width || frame.ContentSize().Height!=size.Height)
                throw std::runtime_error("Window resized during capture");
            auto access=frame.Surface().template as<::Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess>();
            com_ptr<ID3D11Texture2D> source;
            check_hresult(access->GetInterface(__uuidof(ID3D11Texture2D),source.put_void()));
            D3D11_TEXTURE2D_DESC description{};
            source->GetDesc(&description);
            description.Width=width; description.Height=height;
            description.MipLevels=1; description.ArraySize=1;
            description.Usage=D3D11_USAGE_STAGING; description.BindFlags=0;
            description.CPUAccessFlags=D3D11_CPU_ACCESS_READ; description.MiscFlags=0;
            com_ptr<ID3D11Texture2D> staging;
            check_hresult(device->CreateTexture2D(&description,nullptr,staging.put()));
            const D3D11_BOX box{static_cast<UINT>(left),static_cast<UINT>(top),0,
                static_cast<UINT>(left+width),static_cast<UINT>(top+height),1};
            context->CopySubresourceRegion(staging.get(),0,0,0,0,source.get(),0,&box);
            Capture output{static_cast<std::uint32_t>(width),static_cast<std::uint32_t>(height),{}};
            output.rgba.resize(static_cast<std::size_t>(width)*height*4);
            D3D11_MAPPED_SUBRESOURCE mapped{};
            check_hresult(context->Map(staging.get(),0,D3D11_MAP_READ,0,&mapped));
            for (int row=0; row<height; ++row)
            {
                const auto* input=static_cast<const std::uint8_t*>(mapped.pData)+row*mapped.RowPitch;
                auto* destination=output.rgba.data()+static_cast<std::size_t>(row)*width*4;
                for (int column=0; column<width; ++column)
                {
                    destination[column*4]=input[column*4+2];
                    destination[column*4+1]=input[column*4+1];
                    destination[column*4+2]=input[column*4];
                    destination[column*4+3]=input[column*4+3];
                }
            }
            context->Unmap(staging.get(),0);
            RECT current{}; POINT currentOrigin{};
            check_bool(GetClientRect(window,&current)); check_bool(ClientToScreen(window,&currentOrigin));
            if (current.right!=client.right || current.bottom!=client.bottom || currentOrigin.x!=origin.x || currentOrigin.y!=origin.y)
                throw std::runtime_error("Window moved or resized during capture");
            completed=true;
            promise.set_value(std::move(output));
        }
        catch (...) { completed=true; promise.set_exception(std::current_exception()); }
    });
    session.StartCapture();
    const auto status=result.wait_for(std::chrono::seconds(15));
    pool.FrameArrived(token);
    session.Close();
    pool.Close();
    const std::lock_guard lock(mutex);
    if (status!=std::future_status::ready) throw std::runtime_error("No capture frame within 15 seconds");
    return result.get();
}

void save(const Capture& capture,const std::filesystem::path& path)
{
    if (std::filesystem::exists(path)) throw std::runtime_error("Refusing to overwrite capture evidence");
    std::ofstream file(path,std::ios::binary);
    file.write(reinterpret_cast<const char*>(&capture.width),4);
    file.write(reinterpret_cast<const char*>(&capture.height),4);
    file.write(reinterpret_cast<const char*>(capture.rgba.data()),capture.rgba.size());
    file.close();
    if (!file) throw std::runtime_error("Capture output failed");
}

int wmain(int count,wchar_t** arguments)
{
    try
    {
        SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        if (count==2 && std::wstring_view(arguments[1])==L"--self-test")
        {
            const auto window=CreateWindowExW(0,L"STATIC",L"Notification capture self-test",WS_OVERLAPPEDWINDOW|SS_WHITERECT,
                80,80,360,260,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
            check_bool(window!=nullptr);
            struct Close { HWND window; ~Close() { DestroyWindow(window); } } close{window};
            ShowWindow(window,SW_SHOW); UpdateWindow(window);
            auto future=std::async(std::launch::async,[window] { return capture(window); });
            while (future.wait_for(std::chrono::seconds(0))!=std::future_status::ready)
            {
                MSG message;
                while (PeekMessageW(&message,nullptr,0,0,PM_REMOVE)) { TranslateMessage(&message); DispatchMessageW(&message); }
                MsgWaitForMultipleObjectsEx(0,nullptr,16,QS_ALLINPUT,MWMO_INPUTAVAILABLE);
            }
            const auto image=future.get();
            const auto centre=(static_cast<std::size_t>(image.height/2)*image.width+image.width/2)*4;
            if (image.rgba.at(centre)!=255 || image.rgba.at(centre+1)!=255 || image.rgba.at(centre+2)!=255)
                throw std::runtime_error("Self-test did not capture the known white client");
            std::cout<<"Windows Graphics Capture client crop and RGBA conversion passed: "<<image.width<<"x"<<image.height<<"\n";
            return 0;
        }
        if (count!=3) throw std::runtime_error("Usage: notification_capture HWND output.rgba | --self-test");
        const auto window=reinterpret_cast<HWND>(std::stoull(arguments[1],nullptr,0));
        const auto image=capture(window);
        save(image,arguments[2]);
        std::cout<<"Captured top-origin RGBA8 client: "<<image.width<<"x"<<image.height<<"\n";
        return 0;
    }
    catch (const winrt::hresult_error& error) { std::cerr<<winrt::to_string(error.message())<<"\n"; }
    catch (const std::exception& error) { std::cerr<<error.what()<<"\n"; }
    return 1;
}