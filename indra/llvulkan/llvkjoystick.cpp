#include "llvkjoystick.h"
#include "llstring.h"
#include <windows.h>
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <algorithm>
#include <cstring>

struct LLVKJoystick::Impl
{
    IDirectInput8W* input = nullptr;
    IDirectInputDevice8W* device = nullptr;
    HWND window = nullptr;
    DWORD thread = 0;
    std::vector<Device> devices;
    State state;
    LLSD selected;
    std::string failure;
    static BOOL CALLBACK deviceFound(const DIDEVICEINSTANCEW* instance,void* pointer)
    {
        auto& owner=*static_cast<Impl*>(pointer);
        try
        {
            if (owner.devices.size()>=128) { owner.failure="Native joystick device limit exceeded"; return DIENUM_STOP; }
            LLSD::Binary id(sizeof(GUID)); std::memcpy(id.data(),&instance->guidInstance,sizeof(GUID));
            wchar_t identity[40]{};
            if (!StringFromGUID2(instance->guidInstance,identity,40)) { owner.failure="Native joystick GUID conversion failed"; return DIENUM_STOP; }
            owner.devices.push_back({ll_convert<std::string>(std::wstring(instance->tszProductName)),LLSD(id),ll_convert<std::string>(std::wstring(identity))});
        }
        catch (...) { owner.failure="Native joystick enumeration allocation failed"; return DIENUM_STOP; }
        return DIENUM_CONTINUE;
    }
    static BOOL CALLBACK axisFound(const DIDEVICEOBJECTINSTANCEW* object,void* pointer)
    {
        auto& owner=*static_cast<Impl*>(pointer);
        if (!(object->dwType&DIDFT_AXIS)) return DIENUM_CONTINUE;
        DIPROPRANGE range{};
        range.diph.dwSize=sizeof(range); range.diph.dwHeaderSize=sizeof(range.diph);
        range.diph.dwHow=DIPH_BYID; range.diph.dwObj=object->dwType;
        range.lMin=-3000; range.lMax=3000;
        if (FAILED(owner.device->SetProperty(DIPROP_RANGE,&range.diph)))
        { owner.failure="Native joystick axis range negotiation failed"; return DIENUM_STOP; }
        return DIENUM_CONTINUE;
    }
    void release()
    {
        if (device) { device->Unacquire(); device->Release(); device=nullptr; }
        selected=LLSD(); state={};
    }
};

LLVKJoystick::LLVKJoystick() : mImpl(std::make_unique<Impl>()) {}
LLVKJoystick::~LLVKJoystick() { stop(); }

bool LLVKJoystick::start(void* window,std::string& error)
{
    error.clear();
    if (mImpl->input) { error="Native joystick owner already started"; return false; }
    const auto handle=static_cast<HWND>(window);
    if (!IsWindow(handle) || GetWindowThreadProcessId(handle,nullptr)!=GetCurrentThreadId())
    { error="Native joystick requires a window on its owning thread"; return false; }
    mImpl->window=handle; mImpl->thread=GetCurrentThreadId();
    const auto result=DirectInput8Create(GetModuleHandleW(nullptr),DIRECTINPUT_VERSION,IID_IDirectInput8W,
        reinterpret_cast<void**>(&mImpl->input),nullptr);
    if (FAILED(result)) { error="Native DirectInput initialization failed"; return false; }
    return enumerate(error);
}

bool LLVKJoystick::enumerate(std::string& error)
{
    error.clear();
    if (!mImpl->input || mImpl->thread!=GetCurrentThreadId()) { error="Native joystick owner is unavailable"; return false; }
    auto previous=std::move(mImpl->devices); mImpl->devices.clear(); mImpl->failure.clear();
    const auto result=mImpl->input->EnumDevices(DI8DEVCLASS_GAMECTRL,Impl::deviceFound,mImpl.get(),DIEDFL_ATTACHEDONLY);
    if (FAILED(result) || !mImpl->failure.empty())
    { error=mImpl->failure.empty() ? "Native joystick enumeration failed" : mImpl->failure; mImpl->devices=std::move(previous); return false; }
    return true;
}

bool LLVKJoystick::select(const LLSD& id,std::string& error)
{
    error.clear();
    if (!mImpl->input || mImpl->thread!=GetCurrentThreadId()) { error="Native joystick owner is unavailable"; return false; }
    if (!id.isDefined() || (id.isInteger() && id.asInteger()==0)) { mImpl->release(); return true; }
    if (!id.isBinary() || id.asBinary().size()!=sizeof(GUID)) { error="Native joystick requires a binary device GUID"; return false; }
    GUID guid; const auto bytes=id.asBinary(); std::memcpy(&guid,bytes.data(),sizeof(guid));
    IDirectInputDevice8W* device=nullptr;
    if (FAILED(mImpl->input->CreateDevice(guid,&device,nullptr))) { error="Native joystick device is unavailable"; return false; }
    struct Release { IDirectInputDevice8W* device; ~Release() { if (device) device->Release(); } } cleanup{device};
    if (FAILED(device->SetDataFormat(&c_dfDIJoystick)) || FAILED(device->SetCooperativeLevel(mImpl->window,DISCL_BACKGROUND|DISCL_NONEXCLUSIVE)))
    { error="Native joystick device format or ownership failed"; return false; }
    Impl staged; staged.device=device;
    if (FAILED(device->EnumObjects(Impl::axisFound,&staged,DIDFT_AXIS)) || !staged.failure.empty())
    { error=staged.failure.empty() ? "Native joystick axis enumeration failed" : staged.failure; return false; }
    DIDEVCAPS capabilities{}; capabilities.dwSize=sizeof(capabilities);
    if (FAILED(device->GetCapabilities(&capabilities))) { error="Native joystick capabilities unavailable"; return false; }
    const auto acquired=device->Acquire();
    if (FAILED(acquired) && acquired!=DIERR_OTHERAPPHASPRIO)
    { error="Native joystick acquisition failed"; return false; }
    mImpl->release(); mImpl->device=device; cleanup.device=nullptr; mImpl->selected=id;
    mImpl->state.axisCount=std::min<std::size_t>(8,capabilities.dwAxes);
    mImpl->state.buttonCount=std::min<std::size_t>(32,capabilities.dwButtons);
    return poll(error);
}

bool LLVKJoystick::poll(std::string& error)
{
    error.clear();
    if (!mImpl->input || mImpl->thread!=GetCurrentThreadId()) { error="Native joystick owner is unavailable"; return false; }
    if (!mImpl->device) return true;
    auto result=mImpl->device->Poll();
    if (result==DIERR_INPUTLOST || result==DIERR_NOTACQUIRED)
    {
        result=mImpl->device->Acquire();
        if (SUCCEEDED(result)) result=mImpl->device->Poll();
    }
    DIJOYSTATE data{};
    if (SUCCEEDED(result)) result=mImpl->device->GetDeviceState(sizeof(data),&data);
    if (FAILED(result))
    {
        mImpl->state.axes.fill(0); mImpl->state.buttons.fill(false); mImpl->state.connected=false;
        return true;
    }
    const std::array<LONG,8> axes{data.lX,data.lY,data.lZ,data.lRx,data.lRy,data.lRz,data.rglSlider[0],data.rglSlider[1]};
    for (std::size_t index=0; index<axes.size(); ++index) mImpl->state.axes[index]=static_cast<float>(axes[index])/3000.f;
    for (std::size_t index=0; index<mImpl->state.buttons.size(); ++index) mImpl->state.buttons[index]=(data.rgbButtons[index]&0x80)!=0;
    mImpl->state.connected=true;
    return true;
}

void LLVKJoystick::stop()
{
    mImpl->release();
    if (mImpl->input) { mImpl->input->Release(); mImpl->input=nullptr; }
    mImpl->devices.clear(); mImpl->window=nullptr; mImpl->thread=0;
}
const std::vector<LLVKJoystick::Device>& LLVKJoystick::devices() const noexcept { return mImpl->devices; }
const LLVKJoystick::State& LLVKJoystick::state() const noexcept { return mImpl->state; }
LLSD LLVKJoystick::selected() const { return mImpl->selected; }