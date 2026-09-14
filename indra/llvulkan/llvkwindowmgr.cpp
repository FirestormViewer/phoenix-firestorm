#include "llvkwindowmgr.h"
#include "llvkwidgetgpu.h"
#include "llvktexturepreview.h"
#include "llvkaudio.h"
#include "llvktranslation.h"
#include "llvkjoystick.h"
#include "llwebrtc.h"
#include "llcontrol.h"
#include <mutex>
#include <cmath>
#include <fstream>
#include <windows.h>
#include <windowsx.h>
#include <chrono>
#include <shellapi.h>
#include <shlobj.h>
#include "llstring.h"
#include "lluri.h"
#include <intrin.h>
#include <psapi.h>
#include <cstring>
#include <commdlg.h>
#include <objbase.h>
#include <atomic>
#include <thread>
#include <curl/curl.h>

namespace
{
    KEY preferenceKey(WPARAM key)
    {
        if ((key>='A' && key<='Z') || (key>='0' && key<='9')) return static_cast<KEY>(key);
        if (key>=VK_NUMPAD0 && key<=VK_NUMPAD9) return static_cast<KEY>('0'+key-VK_NUMPAD0);
        if (key>=VK_F1 && key<=VK_F12) return static_cast<KEY>(KEY_F1+key-VK_F1);
        static const std::map<WPARAM,KEY> keys{{VK_SPACE,' '},{VK_OEM_1,';'},{VK_OEM_PLUS,'='},{VK_OEM_COMMA,','},
            {VK_OEM_MINUS,'-'},{VK_OEM_PERIOD,'.'},{VK_OEM_2,KEY_DIVIDE},{VK_OEM_3,'`'},{VK_OEM_4,'['},{VK_OEM_5,'\\'},
            {VK_OEM_6,']'},{VK_OEM_7,'\''},{VK_ESCAPE,KEY_ESCAPE},{VK_RETURN,KEY_RETURN},{VK_LEFT,KEY_LEFT},{VK_RIGHT,KEY_RIGHT},
            {VK_UP,KEY_UP},{VK_DOWN,KEY_DOWN},{VK_BACK,KEY_BACKSPACE},{VK_INSERT,KEY_INSERT},{VK_DELETE,KEY_DELETE},
            {VK_SHIFT,KEY_SHIFT},{VK_CONTROL,KEY_CONTROL},{VK_MENU,KEY_ALT},{VK_CAPITAL,KEY_CAPSLOCK},{VK_HOME,KEY_HOME},
            {VK_END,KEY_END},{VK_PRIOR,KEY_PAGE_UP},{VK_NEXT,KEY_PAGE_DOWN},{VK_TAB,KEY_TAB},{VK_ADD,KEY_ADD},
            {VK_SUBTRACT,KEY_SUBTRACT},{VK_MULTIPLY,KEY_MULTIPLY},{VK_DIVIDE,KEY_DIVIDE},{VK_CLEAR,KEY_PAD_CENTER},{VK_APPS,KEY_CONTEXT_MENU}};
        const auto found=keys.find(key);
        return found==keys.end() ? KEY_NONE : found->second;
    }

    class VoiceDevices final
    {
    public:
        VoiceDevices(LLVKViewerUi& ui,LLVKVoice& voice) : mUi(ui), mVoice(voice)
        {
            LLVKViewerUi::VoiceDeviceServices services;
            services.refresh=[this](std::string& error)
            { return mVoice.refresh(error); };
            services.state=[this](std::string& error) { return mVoice.state(error); };
            services.select=[this](bool input,const std::string& device,std::string& error)
            { return mVoice.select(input,device,error); };
            services.tune=[this](bool enabled,float gain,std::string& error)
            { return mVoice.tune(enabled,gain,error); };
            mUi.setVoiceDeviceServices(std::move(services));
        }
        ~VoiceDevices()
        {
            mUi.setVoiceDeviceServices({});
        }
    private:
        LLVKViewerUi& mUi;
        LLVKVoice& mVoice;
    };

    class TranslationVerification
    {
    public:
        TranslationVerification(LLVKViewerUi& ui,const std::filesystem::path& certificate) : mUi(ui)
        {
            mInitialized=curl_global_init(CURL_GLOBAL_DEFAULT)==CURLE_OK;
            if (mInitialized) mMulti=curl_multi_init();
            const auto bytes=certificate.u8string(); mCertificate.assign(bytes.begin(),bytes.end());
            ui.setTranslationVerifier([this](const std::string& service,const LLSD& key,auto response,std::string& error)
            { return start(service,key,std::move(response),error); });
        }
        ~TranslationVerification()
        {
            mUi.setTranslationVerifier({});
            for (const auto& [handle,job] : mJobs) curl_multi_remove_handle(mMulti,handle);
            mJobs.clear();
            if (mMulti) curl_multi_cleanup(mMulti);
            if (mInitialized) curl_global_cleanup();
        }
        bool pump(std::string& error)
        {
            if (!mMulti || mJobs.empty()) return true;
            int running=0;
            if (curl_multi_perform(mMulti,&running)!=CURLM_OK) { error="Native translation transport pump failed"; return false; }
            int remaining=0;
            while (const auto message=curl_multi_info_read(mMulti,&remaining))
            {
                if (message->msg!=CURLMSG_DONE) continue;
                const auto found=mJobs.find(message->easy_handle);
                if (found==mJobs.end()) continue;
                long status=0;
                if (curl_easy_getinfo(message->easy_handle,CURLINFO_RESPONSE_CODE,&status)!=CURLE_OK) status=0;
                const bool ok=message->data.result==CURLE_OK && LLVKTranslation::verified(found->second->service,static_cast<int>(status),found->second->body);
                const auto callback=found->second->response;
                if (curl_multi_remove_handle(mMulti,message->easy_handle)!=CURLM_OK)
                { error="Native translation request retirement failed"; return false; }
                mJobs.erase(found);
                callback(ok,static_cast<int>(status));
            }
            return true;
        }
    private:
        struct Job
        {
            CURL* handle=curl_easy_init();
            curl_slist* headers=nullptr;
            std::string service,body;
            LLVKTranslation::Request request;
            std::function<void(bool,int)> response;
            ~Job() { if (handle) curl_easy_cleanup(handle); if (headers) curl_slist_free_all(headers); }
            static std::size_t write(char* data,std::size_t size,std::size_t count,void* pointer)
            {
                auto& job=*static_cast<Job*>(pointer);
                if (size && count>(1024*1024-job.body.size())/size) return 0;
                try { job.body.append(data,size*count); return size*count; } catch (...) { return 0; }
            }
        };
        bool start(const std::string& service,const LLSD& key,std::function<void(bool,int)> response,std::string& error)
        {
            error.clear();
            if (!mMulti || mJobs.size()>=8) { error="Native translation transport is unavailable or busy"; return false; }
            const auto request=LLVKTranslation::verification(service,key,error);
            if (!request) return false;
            auto job=std::make_unique<Job>();
            if (!job->handle) { error="Native translation request allocation failed"; return false; }
            job->request=*request; job->service=service; job->response=std::move(response);
            for (const auto& [name,value] : request->headers)
            {
                const auto headers=curl_slist_append(job->headers,(name+": "+value).c_str());
                if (!headers) { error="Native translation header allocation failed"; return false; }
                job->headers=headers;
            }
            const auto handle=job->handle;
            const auto option=[&](auto name,auto value) { return curl_easy_setopt(handle,name,value)==CURLE_OK; };
            const auto proxy=mUi.httpProxy(error);
            if (!proxy) return false;
            if (!option(CURLOPT_PROXY,proxy->host.c_str()) || !option(CURLOPT_PROXYPORT,static_cast<long>(proxy->port)) ||
                !option(CURLOPT_NOPROXY,"") ||
                !option(CURLOPT_PROXYTYPE,proxy->type==LLVKProxy::Type::Socks5 ? CURLPROXY_SOCKS5 : CURLPROXY_HTTP))
            { error="Native HTTP proxy configuration failed"; return false; }
            if (proxy->passwordAuthentication)
            {
                const auto credentials=mUi.proxyCredentials(error);
                if (!credentials) return false;
                if (credentials->username.empty() || credentials->username.size()>255 ||
                    credentials->password.empty() || credentials->password.size()>255 ||
                    credentials->username.find('\0')!=std::string::npos || credentials->password.find('\0')!=std::string::npos)
                { error="SOCKS5 requires valid protected username and password"; return false; }
                if (!option(CURLOPT_PROXYUSERNAME,credentials->username.c_str()) ||
                    !option(CURLOPT_PROXYPASSWORD,credentials->password.c_str()))
                { error="Native SOCKS5 authentication configuration failed"; return false; }
            }
            if (!option(CURLOPT_URL,job->request.url.c_str()) || !option(CURLOPT_HTTPHEADER,job->headers) ||
                !option(CURLOPT_USERAGENT,"Vulkanstorm native translation") || !option(CURLOPT_WRITEFUNCTION,&Job::write) ||
                !option(CURLOPT_WRITEDATA,job.get()) || !option(CURLOPT_CONNECTTIMEOUT_MS,10000L) || !option(CURLOPT_TIMEOUT_MS,30000L) ||
                !option(CURLOPT_NOSIGNAL,1L) || !option(CURLOPT_SSL_VERIFYPEER,1L) || !option(CURLOPT_SSL_VERIFYHOST,2L) ||
                !option(CURLOPT_CAINFO,mCertificate.c_str()) || !option(CURLOPT_PROTOCOLS,static_cast<long>(CURLPROTO_HTTPS)) || !option(CURLOPT_FOLLOWLOCATION,0L))
            { error="Native translation request configuration failed"; return false; }
            if (job->request.post && (!option(CURLOPT_POST,1L) || !option(CURLOPT_POSTFIELDS,job->request.body.c_str()) ||
                !option(CURLOPT_POSTFIELDSIZE,static_cast<long>(job->request.body.size()))))
            { error="Native translation POST configuration failed"; return false; }
            mJobs.emplace(handle,std::move(job));
            if (curl_multi_add_handle(mMulti,handle)!=CURLM_OK)
            { mJobs.erase(handle); error="Native translation submission failed"; return false; }
            return true;
        }
        LLVKViewerUi& mUi;
        CURLM* mMulti=nullptr;
        bool mInitialized=false;
        std::string mCertificate;
        std::map<CURL*,std::unique_ptr<Job>> mJobs;
    };

    class XmlFilePicker
    {
        enum class Kind { Xml, Dictionary, Directory, Executable };
    public:
        XmlFilePicker(LLVKViewerUi& ui,HWND owner) : mUi(ui),mOwner(owner)
        {
            ui.setXmlFilePicker([this](bool save,const std::string& name,LLVKViewerUi::XmlFileResult callback,std::string& error)
            { return start(save,name,std::move(callback),Kind::Xml,error); });
            ui.setDictionaryFilePicker([this](bool save,const std::string& name,LLVKViewerUi::XmlFileResult callback,std::string& error)
            { return start(save,name,std::move(callback),Kind::Dictionary,error); });
            ui.setExecutableFilePicker([this](bool save,const std::string& name,LLVKViewerUi::XmlFileResult callback,std::string& error)
            { return start(save,name,std::move(callback),Kind::Executable,error); });
            ui.setDirectoryPicker([this](const std::filesystem::path& path,LLVKViewerUi::XmlFileResult callback,std::string& error)
            {
                const auto bytes=path.u8string();
                return start(false,std::string(bytes.begin(),bytes.end()),std::move(callback),Kind::Directory,error);
            });
            ui.setDirectoryOpener([owner](const std::filesystem::path& path,std::string& error)
            {
                error.clear();
                std::error_code status;
                if (!path.is_absolute() || !std::filesystem::is_directory(path,status) || status)
                { error="The selected directory does not exist"; return false; }
                if (reinterpret_cast<INT_PTR>(ShellExecuteW(owner,L"open",path.c_str(),nullptr,nullptr,SW_SHOWNORMAL))<=32)
                { error="Windows could not open the selected directory"; return false; }
                return true;
            });
        }
        ~XmlFilePicker()
        {
            mUi.setXmlFilePicker({});
            mUi.setDictionaryFilePicker({});
            mUi.setExecutableFilePicker({});
            mUi.setDirectoryPicker({});
            mUi.setDirectoryOpener({});
            mCancelled.store(true);
            if (mWorker.joinable())
            {
                const HANDLE worker=mWorker.native_handle();
                bool quitting=false;
                while (MsgWaitForMultipleObjects(1,&worker,FALSE,INFINITE,QS_ALLINPUT)==WAIT_OBJECT_0+1)
                {
                    MSG message;
                    while (PeekMessageW(&message,nullptr,0,0,PM_REMOVE))
                    {
                        if (message.message==WM_QUIT) { quitting=true; continue; }
                        TranslateMessage(&message); DispatchMessageW(&message);
                    }
                }
                mWorker.join();
                if (quitting) PostQuitMessage(0);
            }
        }
        void pump()
        {
            if (!mDone.load()) return;
            mWorker.join(); mDone.store(false);
            const auto callback=std::move(mCallback);
            if (callback) callback(std::move(mPath),std::move(mError));
        }
    private:
        static UINT_PTR CALLBACK hook(HWND window,UINT message,WPARAM parameter,LPARAM data)
        {
            if (message==WM_INITDIALOG)
            {
                const auto* configuration=reinterpret_cast<const OPENFILENAMEW*>(data);
                SetWindowLongPtrW(window,GWLP_USERDATA,configuration->lCustData);
                if (!SetTimer(window,1,20,nullptr)) PostMessageW(GetParent(window),WM_COMMAND,IDCANCEL,0);
            }
            const auto* owner=reinterpret_cast<XmlFilePicker*>(GetWindowLongPtrW(window,GWLP_USERDATA));
            if (owner && (message==WM_INITDIALOG || (message==WM_TIMER && parameter==1)) && owner->mCancelled.load())
            { KillTimer(window,1); PostMessageW(GetParent(window),WM_COMMAND,IDCANCEL,0); }
            if (message==WM_DESTROY) KillTimer(window,1);
            return 0;
        }
        struct DirectoryContext
        {
            XmlFilePicker* owner;
            const std::wstring* initial;
        };
        static int CALLBACK directoryCallback(HWND window,UINT message,LPARAM,LPARAM data)
        {
            if (message==BFFM_INITIALIZED)
            {
                const auto* context=reinterpret_cast<const DirectoryContext*>(data);
                if (!SetPropW(window,L"NativeDirectoryPicker",context->owner) || !SetTimer(window,1,20,directoryTimer))
                { PostMessageW(window,WM_COMMAND,IDCANCEL,0); return 0; }
                if (!context->initial->empty()) SendMessageW(window,BFFM_SETSELECTIONW,TRUE,reinterpret_cast<LPARAM>(context->initial->c_str()));
            }
            return 0;
        }
        static void CALLBACK directoryTimer(HWND window,UINT,UINT_PTR timer,DWORD)
        {
            const auto* owner=static_cast<const XmlFilePicker*>(GetPropW(window,L"NativeDirectoryPicker"));
            if (owner && owner->mCancelled.load())
            { KillTimer(window,timer); PostMessageW(window,WM_COMMAND,IDCANCEL,0); }
        }
        bool start(bool save,const std::string& name,LLVKViewerUi::XmlFileResult callback,Kind kind,std::string& error)
        {
            error.clear();
            if (mWorker.joinable()) { error="A native file picker is already active"; return false; }
            const auto wide=ll_convert<std::wstring>(name);
            if (wide.size()>=32768) { error="Native file picker name exceeds limit"; return false; }
            mPath.reset(); mError.clear(); mCallback=std::move(callback); mCancelled.store(false);
            mWorker=std::thread([this,save,wide,kind]
            {
                const auto initialized=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
                if (FAILED(initialized)) { mError="Native file picker COM initialization failed"; mDone.store(true); return; }
                struct ComScope { ~ComScope() { CoUninitialize(); } } com;
                try
                {
                    if (kind==Kind::Directory)
                    {
                        DirectoryContext context{this,&wide};
                        BROWSEINFOW configuration{};
                        configuration.hwndOwner=mOwner;
                        configuration.ulFlags=BIF_RETURNONLYFSDIRS|BIF_USENEWUI;
                        configuration.lpfn=directoryCallback;
                        configuration.lParam=reinterpret_cast<LPARAM>(&context);
                        const auto selected=SHBrowseForFolderW(&configuration);
                        if (selected)
                        {
                            wchar_t path[32768]{};
                            if (SHGetPathFromIDListEx(selected,path,32768,GPFIDL_DEFAULT)) mPath=std::filesystem::path(path);
                            else mError="Windows could not resolve the selected directory";
                            CoTaskMemFree(selected);
                        }
                        mDone.store(true);
                        return;
                    }
                    std::vector<wchar_t> filename(32768,0);
                    std::copy(wide.begin(),wide.end(),filename.begin());
                    OPENFILENAMEW configuration{}; configuration.lStructSize=sizeof(configuration);
                    configuration.hwndOwner=mOwner; configuration.lpstrFile=filename.data(); configuration.nMaxFile=static_cast<DWORD>(filename.size());
                    configuration.lpstrFilter=kind==Kind::Executable ? L"Executable Files (*.exe)\0*.exe\0\0" :
                        kind==Kind::Dictionary ? L"Dictionary Files (*.dic;*.aff;*.xcu)\0*.dic;*.aff;*.xcu\0\0" : L"XML File (*.xml)\0*.xml\0\0";
                    configuration.nFilterIndex=1; configuration.lpstrDefExt=kind==Kind::Executable ? L"exe" : kind==Kind::Dictionary ? L"dic" : L"xml";
                    configuration.Flags=OFN_EXPLORER|OFN_ENABLEHOOK|OFN_NOCHANGEDIR|OFN_HIDEREADONLY|
                        (save ? OFN_OVERWRITEPROMPT|OFN_PATHMUSTEXIST : OFN_FILEMUSTEXIST);
                    configuration.lpfnHook=hook; configuration.lCustData=reinterpret_cast<LPARAM>(this);
                    const auto selected=save ? GetSaveFileNameW(&configuration) : GetOpenFileNameW(&configuration);
                    if (selected) mPath=std::filesystem::path(filename.data());
                    else if (const auto failure=CommDlgExtendedError()) mError="Native file picker failed: "+std::to_string(failure);
                }
                catch (...) { mError="Native file picker operation failed"; }
                mDone.store(true);
            });
            return true;
        }
        LLVKViewerUi& mUi;
        HWND mOwner;
        std::thread mWorker;
        std::atomic<bool> mDone{false},mCancelled{false};
        LLVKViewerUi::XmlFileResult mCallback;
        std::optional<std::filesystem::path> mPath;
        std::string mError;
    };

    struct WindowState
    {
        HWND window = nullptr;
        LLVKViewerUi* ui = nullptr;
        LLVKBrowser* browser = nullptr;
        std::map<LLVKWidgetTree::Id,LLVKBrowser*> browserViews;
        LLVKWidgetPaint::Input input;
        std::function<void()> audioVolumeChanged;
        std::string error;
        bool close = false, quitRequested = false, resize = true;
        std::uint32_t width = 1024, height = 768;
        char32_t surrogate = 0;
        std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now(), keystroke = start;
        ~WindowState() { if (window) DestroyWindow(window); }
        double elapsed() const { return std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count(); }
        LLVKWidgetTree::Id browserAt(int x,int y)
        {
            if (!ui || ui->modalNotice() || ui->tree().topControl()) return 0;
            const auto overFloater=ui->pointOverFloater(x,y);
            for (const auto& [id,view] : browserViews)
            {
                if (view->state()!=LLVKBrowser::State::Running) continue;
                auto parent=id;
                bool visible=true;
                while (const auto* node=ui->tree().get(parent))
                {
                    if (!node->params.visible) { visible=false; break; }
                    if (node->floater) break;
                    parent=node->parent;
                }
                if (!visible || (overFloater ? parent!=ui->activeFloater() : parent!=0)) continue;
                const auto rect=ui->tree().screenRect(id,error);
                if (rect && x>=rect->left && x<rect->right && y>=rect->bottom && y<rect->top) return id;
            }
            return 0;
        }
        LRESULT message(UINT message, WPARAM parameter, LPARAM data)
        {
            if (message == WM_CLOSE) { quitRequested = true; return 0; }
            if (message == WM_SIZE) { width = LOWORD(data); height = HIWORD(data); resize = true; return 0; }
            if (!ui) return DefWindowProcW(window,message,parameter,data);
            auto& tree = ui->tree();
            tree.setInputModifiers({bool(GetKeyState(VK_SHIFT)&0x8000),bool(GetKeyState(VK_CONTROL)&0x8000),bool(GetKeyState(VK_MENU)&0x8000)});
            const auto focused = tree.keyboardFocus();
            const auto* focus = tree.get(focused);
            if (!ui->keyCaptureDialog() && (message==WM_KEYUP || message==WM_SYSKEYUP) &&
                ui->recordPreferenceKey(preferenceKey(parameter),0,false,error)) return 0;
            if (ui->keyCaptureDialog())
            {
                const MASK mask=((GetKeyState(VK_CONTROL)&0x8000) ? MASK_CONTROL : 0) |
                    ((GetKeyState(VK_SHIFT)&0x8000) ? MASK_SHIFT : 0) | ((GetKeyState(VK_MENU)&0x8000) ? MASK_ALT : 0);
                if (message==WM_KEYDOWN || message==WM_KEYUP || message==WM_SYSKEYDOWN || message==WM_SYSKEYUP)
                {
                    ui->recordPreferenceKey(preferenceKey(parameter),mask,message==WM_KEYDOWN || message==WM_SYSKEYDOWN,error);
                    return 0;
                }
                if (message==WM_CHAR || message==WM_SYSCHAR || message==WM_MOUSEWHEEL) return 0;
                if (message==WM_LBUTTONDOWN || message==WM_LBUTTONUP || message==WM_LBUTTONDBLCLK ||
                    message==WM_RBUTTONDOWN || message==WM_RBUTTONUP || message==WM_MBUTTONDOWN || message==WM_MBUTTONUP ||
                    message==WM_XBUTTONDOWN || message==WM_XBUTTONUP)
                {
                    const bool down=message==WM_LBUTTONDOWN || message==WM_LBUTTONDBLCLK || message==WM_RBUTTONDOWN || message==WM_MBUTTONDOWN || message==WM_XBUTTONDOWN;
                    const auto click=message==WM_LBUTTONDBLCLK ? CLICK_DOUBLELEFT : message==WM_RBUTTONDOWN || message==WM_RBUTTONUP ? CLICK_RIGHT :
                        message==WM_MBUTTONDOWN || message==WM_MBUTTONUP ? CLICK_MIDDLE : message==WM_XBUTTONDOWN || message==WM_XBUTTONUP ?
                        (GET_XBUTTON_WPARAM(parameter)==XBUTTON1 ? CLICK_BUTTON4 : CLICK_BUTTON5) : CLICK_LEFT;
                    LLVKWidgetTree::PointerEvent event;
                    event.x=GET_X_LPARAM(data); event.y=static_cast<int>(height)-1-GET_Y_LPARAM(data); event.time=elapsed();
                    event.kind=down ? LLVKWidgetTree::PointerKind::LeftDown : LLVKWidgetTree::PointerKind::LeftUp;
                    ui->recordPreferenceMouse(event,click,down,mask,error);
                    if (tree.mouseCapture()) SetCapture(window); else if (GetCapture()==window) ReleaseCapture();
                    return message==WM_XBUTTONDOWN || message==WM_XBUTTONUP ? TRUE : 0;
                }
            }
            LLVKWidgetTree::Id multiline=0;
            if (focus && focus->plainText)
                for (auto parent=focused; tree.get(parent); parent=tree.get(parent)->parent)
                    if (tree.get(parent)->textEditor) { multiline=parent; break; }
            if (ui->modalNotice())
            {
                if ((message==WM_KEYDOWN || message==WM_SYSKEYDOWN) && (parameter==VK_RETURN || parameter==VK_ESCAPE))
                {
                    ui->noticeKey(parameter==VK_RETURN,(GetKeyState(VK_SHIFT)&0x8000) ||
                        (GetKeyState(VK_CONTROL)&0x8000) || (GetKeyState(VK_MENU)&0x8000),error);
                    return 0;
                }
                if (message==WM_SYSCHAR || message==WM_SYSKEYDOWN || message==WM_SYSKEYUP || message==WM_MOUSEWHEEL) return 0;
            }
            if (message == WM_SETFOCUS || message == WM_KILLFOCUS)
            {
                const bool focused=message==WM_SETFOCUS;
                if (focused && !input.editor.applicationFocused)
                {
                    tree.advanceTime(elapsed(),error);
                    tree.triggerFocusFlash();
                }
                input.editor.applicationFocused=focused;
                if (!focused)
                {
                    ui->menu().dismiss();
                    tree.setMouseCapture(0,error);
                    if (GetCapture()==window) ReleaseCapture();
                }
                if (audioVolumeChanged) audioVolumeChanged();
                return 0;
            }
            if (message == WM_CAPTURECHANGED)
            { if (reinterpret_cast<HWND>(data) != window) tree.setMouseCapture(0,error); return 0; }
            if (message==WM_RBUTTONDOWN && !ui->modalNotice() && !tree.topControl() && !tree.mouseCapture() &&
                ui->previewPointer(GET_X_LPARAM(data),static_cast<int>(height)-1-GET_Y_LPARAM(data),error)) return 0;
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
                if (!ui->floaterPointer(event,error))
                {
                    const auto target=browserAt(event.x,event.y);
                    tree.routePointer(target ? target : ui->root(),event,error);
                }
                if (tree.mouseCapture()) SetCapture(window); else if (GetCapture() == window) ReleaseCapture();
                if (message != WM_MOUSEMOVE) keystroke = std::chrono::steady_clock::now();
                if (message == WM_MOUSEMOVE)
                {
                    const auto id=browserAt(event.x,event.y);
                    const auto rectangle=id ? tree.screenRect(id,error) : std::nullopt;
                    if (rectangle)
                    {
                        auto& browser=*browserViews.at(id);
                        const auto display=LLVKBrowserSurface::displayRect(rectangle->right-rectangle->left,rectangle->top-rectangle->bottom,browser.surface().width(),browser.surface().height());
                        browser.hover(event.x-rectangle->left-display.left,rectangle->bottom+display.top-1-event.y,error);
                    }
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
                if (const auto id=browserAt(point.x,bottom))
                {
                    const auto rect=tree.screenRect(id,error);
                    if (rect)
                    {
                        auto& browser=*browserViews.at(id);
                        const auto display=LLVKBrowserSurface::displayRect(rect->right-rect->left,rect->top-rect->bottom,browser.surface().width(),browser.surface().height());
                        browser.wheel(point.x-rect->left-display.left,rect->bottom+display.top-1-bottom,0,GET_WHEEL_DELTA_WPARAM(parameter),error);
                    }
                    return 0;
                }
                if (!ui->floaterWheel(point.x,bottom,clicks,error) && !ui->pointOverFloater(point.x,bottom))
                    tree.routeWheel(ui->root(),point.x,bottom,clicks,false,error);
                return 0;
            }
            if (!ui->modalNotice() && (message == WM_KEYDOWN || message == WM_SYSKEYDOWN))
            {
                if (ui->guidebook() && ui->activeFloater()==ui->guidebook() && (parameter==VK_F1 || parameter==VK_ESCAPE))
                { ui->closeFloater(error); return 0; }
                std::string shortcut;
                if (parameter >= 'A' && parameter <= 'Z') shortcut.assign(1,static_cast<char>(parameter));
                else if (parameter >= VK_F1 && parameter <= VK_F12) shortcut = "F"+std::to_string(parameter-VK_F1+1);
                if (!shortcut.empty() && ui->menu().shortcut(shortcut,bool(GetKeyState(VK_CONTROL)&0x8000),
                    bool(GetKeyState(VK_SHIFT)&0x8000),bool(GetKeyState(VK_MENU)&0x8000))) return 0;
                if (parameter == VK_F10) { ui->menu().key(LLVKMenu::Key::Activate); return 0; }
                if (ui->menu().open())
                {
                    using Key = LLVKMenu::Key;
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
            if (ui->menu().open() && (message == WM_CHAR || message == WM_SYSCHAR))
            { ui->menu().character(static_cast<char32_t>(parameter)); return 0; }
            if (ui->menu().open() && (message == WM_KEYUP || message == WM_SYSKEYUP)) return 0;
            if ((message == WM_KEYDOWN || message == WM_KEYUP || message == WM_CHAR || message == WM_SYSKEYDOWN || message == WM_SYSKEYUP) && focus && focus->browser)
            {
                const auto target=browserViews.find(focused);
                if (target!=browserViews.end()) target->second->keyboard(message,static_cast<std::uint32_t>(parameter),static_cast<std::uint64_t>(data),error);
                return 0;
            }
            if (message == WM_CHAR && focus && (focus->lineEditor || multiline))
            {
                char32_t character = static_cast<char32_t>(parameter);
                if (character >= 0xd800 && character <= 0xdbff) { surrogate = character; return 0; }
                if (character >= 0xdc00 && character <= 0xdfff)
                { if (!surrogate) return 0; character = 0x10000+((surrogate-0xd800)<<10)+(character-0xdc00); }
                surrogate = 0;
                if (character >= 32 && character != 127 && !(GetKeyState(VK_CONTROL)&0x8000))
                {
                    if (multiline) tree.insertTextEditorText(multiline,std::u32string(1,character),error);
                    else tree.lineEditorUnicode(focused,character,error);
                }
                keystroke = std::chrono::steady_clock::now();
                return 0;
            }
            if (message==WM_CHAR && focus && focus->colorSwatch && parameter==' ')
            { tree.showColorSwatchPicker(focused,true,error); return 0; }
            if (message == WM_KEYDOWN || message == WM_SYSKEYDOWN)
            {
                keystroke = std::chrono::steady_clock::now();
                LLVKLineEditor::Modifiers modifiers{bool(GetKeyState(VK_SHIFT)&0x8000),bool(GetKeyState(VK_CONTROL)&0x8000),bool(GetKeyState(VK_MENU)&0x8000)};
                if (multiline && !tree.get(multiline)->textEditor->readOnly)
                {
                    if (modifiers.control)
                    {
                        if (parameter=='X') { tree.cutTextEditor(multiline,error); return 0; }
                        if (parameter=='V') { tree.pasteTextEditor(multiline,error); return 0; }
                        if (parameter=='Z') { tree.undoTextEditor(multiline,modifiers.shift,error); return 0; }
                        if (parameter=='Y') { tree.undoTextEditor(multiline,true,error); return 0; }
                    }
                    if (parameter==VK_BACK || parameter==VK_DELETE)
                    { tree.deleteTextEditor(multiline,parameter==VK_BACK,modifiers.control,error); return 0; }
                    if (parameter==VK_RETURN && !modifiers.control && !modifiers.alt && !modifiers.shift)
                    { tree.insertTextEditorText(multiline,U"\n",error); return 0; }
                }
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
                        if (tree.get(parent)->scrollList && tree.scrollListKey(parent,key,modifiers,error)) return 0;
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
                const auto focusRoot=ui->modalNotice() ? ui->modalNotice() : dialog ? dialog : ui->root();
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

    class ApplicationServices final : public LLVKSessionOwner::Service
    {
    public:
        using Code=LLVKSessionOwner::Code;
        struct Access { ApplicationServices* service=nullptr; };
        explicit ApplicationServices(WindowState& window) : access(std::make_shared<Access>()),mWindow(&window)
        { access->service=this; }
        ~ApplicationServices() override { access->service=nullptr; }
        Code acquire(const LLVKSessionOwner::Context&) override { return Code::Ok; }
        void detach()
        {
            if (!mWindow) return;
            auto& window=*mWindow;
            auto* ui=window.ui;
            window.audioVolumeChanged={};
            if (ui)
            {
                ui->setUiSoundPlayer({});
                for (const auto subscription : audioSubscriptions) ui->tree().unsubscribeSetting(subscription);
                ui->setJoystickServices({});
                ui->setGuidebookService({},{});
                ui->setBrowserCommand({});
                for (const auto& [id,view] : window.browserViews)
                {
                    ui->tree().setEvents(id,{});
                    ui->tree().setVisible(id,false);
                }
            }
            audioSubscriptions.clear();
            window.browser=nullptr;
            window.browserViews.clear();
            window.input.browsers.clear();
            window.input.browserEpochs.clear();
            voiceDevices.reset();
            translation.reset();
            previews.reset();
            picker.reset();
            mWindow=nullptr;
        }
        Code retire() override
        {
            detach();
            std::string error;
            if (!mCloseDeadline) mCloseDeadline=std::chrono::steady_clock::now()+std::chrono::seconds(15);
            bool pending=false,failed=false;
            const auto close=[&](std::unique_ptr<LLVKBrowser>& view)
            {
                if (!view) return;
                if (view->state()==LLVKBrowser::State::Fresh || view->state()==LLVKBrowser::State::Failed ||
                    view->state()==LLVKBrowser::State::Closed) { view.reset(); return; }
                const bool requested=view->requestClose(error);
                const bool updated=view->update(error);
                if (view->state()==LLVKBrowser::State::Closed) view.reset();
                else { pending=true; failed|=!requested || !updated; }
            };
            for (auto& [id,view] : browsers) close(view);
            std::erase_if(browsers,[](const auto& entry) { return !entry.second; });
            close(browser);
            if (failed || (pending && std::chrono::steady_clock::now()>=*mCloseDeadline))
            { mCloseDeadline.reset(); return Code::CleanupFailed; }
            if (pending) return Code::Pending;
            if (voice && !voice->stop(error)) return Code::CleanupFailed;
            voice.reset();
            if (audio && !audio->stop(error)) return Code::CleanupFailed;
            audio.reset();
            if (joystick) joystick->stop();
            joystick.reset();
            return Code::Ok;
        }
        bool active() const { return mWindow!=nullptr; }
        std::shared_ptr<Access> access;
        std::unique_ptr<LLVKAudio> audio;
        std::unique_ptr<LLVKVoice> voice;
        std::unique_ptr<LLVKJoystick> joystick;
        std::unique_ptr<VoiceDevices> voiceDevices;
        std::unique_ptr<TranslationVerification> translation;
        std::unique_ptr<XmlFilePicker> picker;
        std::unique_ptr<LLVKTexturePreview> previews;
        std::unique_ptr<LLVKBrowser> browser;
        std::map<LLVKWidgetTree::Id,std::unique_ptr<LLVKBrowser>> browsers;
        std::vector<std::uint64_t> audioSubscriptions;
    private:
        WindowState* mWindow;
        std::optional<std::chrono::steady_clock::time_point> mCloseDeadline;
    };

    struct ApplicationRun final
    {
        std::unique_ptr<LLVKSessionOwner> local;
        LLVKSessionOwner* owner=nullptr;
        std::shared_ptr<ApplicationServices::Access> access;
        LLVKViewerUi& ui;
        LLVKError::Resolver errorResolver;
        ~ApplicationRun()
        {
            ui.setSessionOwner(nullptr);
            if (access && access->service) access->service->detach();
            if (!owner) return;
            try
            {
                const auto snapshot=owner->snapshot();
                if (snapshot.state!=LLVKSessionOwner::State::Stopped && snapshot.state!=LLVKSessionOwner::State::Disconnecting)
                    owner->shutdown();
                if (local && owner->snapshot().state!=LLVKSessionOwner::State::Stopped)
                {
                    local.release();
                    llvkPresentErrorFallback({LLVKError::Code::ShutdownFailed,LLVKError::Operation::Shutdown,1,0},nullptr,errorResolver);
                }
            }
            catch (...) { local.release(); }
        }
    };

    class VisualServices final
    {
    public:
        explicit VisualServices(WindowState& window) : mWindow(window) {}
        ~VisualServices()
        {
            std::string error;
            if (!prepareShutdown(error))
                LL_WARNS("NativeShutdown") << (LLVKError{LLVKError::Code::ShutdownFailed,LLVKError::Operation::Shutdown,1,0}).diagnostic() << LL_ENDL;
            gpu.reset();
            ui.reset();
            renderer.destroy();
        }
        bool prepareShutdown(std::string& error)
        {
            if (!mRetirementAttempted)
            {
                mRetirementAttempted=true;
                mWindow.ui=nullptr;
                mWindow.browser=nullptr;
                mWindow.browserViews.clear();
                if (ui)
                {
                    ui->setFontTextureDumpHandler({});
                    ui->tree().setClipboard({});
                    ui->setDialogClipboard({});
                }
                if (renderer.device()!=VK_NULL_HANDLE)
                {
                    const auto result=vkDeviceWaitIdle(renderer.device());
                    if (result!=VK_SUCCESS) mRetirementError="Native GPU retirement failed: "+std::to_string(result);
                }
            }
            error=mRetirementError;
            return error.empty();
        }
        bool initializeUi(const LLVKViewerUi::Configuration& configuration,std::string& error)
        {
            ui=LLVKViewerUi::create(configuration,error);
            if (!ui) return false;
            mWindow.ui=ui.get();
            return true;
        }
        bool initializeRenderer(bool validation,std::string& error)
        {
            if (!renderer.createInstance(validation,error)) return false;
            surface=renderer.createSurface(mWindow.window,GetModuleHandleW(nullptr));
            if (!surface) { error="Native login Vulkan surface creation failed"; return false; }
            if (!renderer.pickPhysicalDevice(surface,error) || !renderer.createDevice(surface,error))
            { vkDestroySurfaceKHR(renderer.instance(),surface,nullptr); return false; }
            const auto synchronized=ui->tree().setting("RenderVSyncEnable").value_or(LLSD(false)).asBoolean();
            if (!renderer.createSwapchain(surface,mWindow.width,mWindow.height,error,synchronized) ||
                !renderer.create2DPipeline(error)) return false;
            gpu=std::make_unique<LLVKWidgetGpu>(LLVKGlyphUpload::Device{renderer.physicalDevice(),renderer.device(),
                renderer.allocator(),renderer.graphicsQueue(),renderer.graphicsQueueFamily(),renderer.samplerAnisotropyEnabled()});
            return true;
        }
        std::unique_ptr<LLVKViewerUi> ui;
        LLVKContext renderer;
        VkSurfaceKHR surface=VK_NULL_HANDLE;
        std::unique_ptr<LLVKWidgetGpu> gpu;
    private:
        WindowState& mWindow;
        bool mRetirementAttempted=false;
        std::string mRetirementError;
    };
}

bool LLVKWindowMgr::run(const Configuration& configuration,std::string& error)
{
    error.clear();
    using Code = LLVKError::Code;
    using Operation = LLVKError::Operation;
    if (configuration.failureCode) *configuration.failureCode = Code::WindowUnavailable;
    const auto fail = [&](Code code)
    {
        if (configuration.failureCode) *configuration.failureCode = code;
        return false;
    };
    WindowState state;
    std::uint64_t noticeGeneration = 0;
    const auto notice = [&](Code code)
    {
        const LLVKError failure{code,Operation::Window,++noticeGeneration,0};
        try { LL_WARNS("NativeWindow") << failure.diagnostic() << LL_ENDL; }
        catch (...) {}
        std::string problem;
        if (!state.ui || !state.ui->showError(failure,problem))
            llvkPresentErrorFallback(failure,state.window,configuration.errorResolver);
    };
    auto uiConfiguration=configuration.ui;
    wchar_t executablePath[32768]{};
    const auto executableLength=GetModuleFileNameW(nullptr,executablePath,32768);
    if (!executableLength || executableLength>=32768) { error="Native executable path is unavailable"; return false; }
    uiConfiguration.viewerExecutable=std::filesystem::path(executablePath);
    uiConfiguration.pluginLauncher=uiConfiguration.viewerExecutable.parent_path()/"SLPlugin.exe";
    uiConfiguration.voiceExecutable=uiConfiguration.viewerExecutable.parent_path()/"SLVoice.exe";
    uiConfiguration.browserHelper=configuration.browser.helperDirectory/"dullahan_host.exe";
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
        if (restart) state.quitRequested=true;
        return true;
    };
    VisualServices visuals(state);
    if (!visuals.initializeUi(uiConfiguration,error)) return fail(Code::StartupResources);
    auto& ui=visuals.ui;
    ApplicationRun application{{},configuration.sessionOwner,{},*ui,configuration.errorResolver};
    if (!application.owner)
    {
        application.local=std::make_unique<LLVKSessionOwner>();
        application.owner=application.local.get();
    }
    auto ownedServices=std::make_unique<ApplicationServices>(state);
    application.access=ownedServices->access;
    auto& services=*ownedServices;
    std::unique_ptr<LLVKSessionOwner::Service> adoptedServices=std::move(ownedServices);
    if (!application.owner->install(LLVKSessionOwner::Lifetime::Application,LLVKWindowMgr::applicationServiceId,adoptedServices).ok())
    { error="Native application service adoption failed"; return fail(Code::StartupResources); }
    ui->setSessionOwner(application.owner);
    ui->menu().bind("File.Quit",[&state](const auto&,const auto&) { state.quitRequested = true; });
    ui->setQuitRequestHandler([&state] { state.quitRequested=true; });
    ui->setWindowSizeService([&state](int width,int height,std::string& problem)
    {
        if (IsZoomed(state.window)) ShowWindow(state.window,SW_RESTORE);
        RECT rectangle{0,0,width,height};
        const auto style=static_cast<DWORD>(GetWindowLongPtrW(state.window,GWL_STYLE));
        const auto extended=static_cast<DWORD>(GetWindowLongPtrW(state.window,GWL_EXSTYLE));
        if (!AdjustWindowRectEx(&rectangle,style,GetMenu(state.window)!=nullptr,extended) ||
            !SetWindowPos(state.window,nullptr,0,0,rectangle.right-rectangle.left,rectangle.bottom-rectangle.top,
                SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE))
        { problem="Native window resize failed: "+std::to_string(GetLastError()); return false; }
        RECT client{};
        if (!GetClientRect(state.window,&client) || client.right-client.left!=width || client.bottom-client.top!=height)
        { problem="The operating system could not apply the requested client dimensions"; return false; }
        return true;
    });
    const auto openUrl=[&state,&notice](const std::string& url)
    {
        const LLURI uri(url);
        auto scheme = uri.scheme();
        LLStringUtil::toLower(scheme);
        if ((scheme != "https" && scheme != "http" && scheme != "ftp") || uri.hostName().empty()) return;
        const auto wide = ll_convert<std::wstring>(url);
        if (MessageBoxW(state.window,(L"Open this page in your web browser?\n\n"+wide).c_str(),L"Vulkanstorm",MB_YESNO|MB_ICONQUESTION) == IDYES)
            if (reinterpret_cast<INT_PTR>(ShellExecuteW(state.window,L"open",wide.c_str(),nullptr,nullptr,SW_SHOWNORMAL)) <= 32)
                notice(Code::OperationFailed);
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
    const auto savedWidth=ui->tree().setting("WindowWidth").value_or(LLSD(1024)).asInteger();
    const auto savedHeight=ui->tree().setting("WindowHeight").value_or(LLSD(768)).asInteger();
    if (savedWidth<=0 || savedHeight<=0 || savedWidth>16384 || savedHeight>16384)
    { error="Native window dimensions are invalid"; return false; }
    RECT rectangle{0,0,savedWidth,savedHeight};
    AdjustWindowRect(&rectangle,WS_OVERLAPPEDWINDOW,FALSE);
    const auto windowX=ui->tree().setting("WindowX").value_or(LLSD(CW_USEDEFAULT)).asInteger();
    const auto windowY=ui->tree().setting("WindowY").value_or(LLSD(CW_USEDEFAULT)).asInteger();
    if (!CreateWindowExW(0,windowClass.lpszClassName,L"Vulkanstorm",WS_OVERLAPPEDWINDOW,windowX,windowY,
        rectangle.right-rectangle.left,rectangle.bottom-rectangle.top,nullptr,nullptr,windowClass.hInstance,&state))
    { error = "Native login window creation failed"; return false; }
    services.audio=std::make_unique<LLVKAudio>();
    auto& audio=*services.audio;
    std::string audioError;
    if (!audio.start(ui->tree().setting("NoAudio").value_or(LLSD(false)).asBoolean(),audioError))
        LL_WARNS("NativeAudio") << (LLVKError{Code::OperationFailed,Operation::Window,1,0}).diagnostic() << LL_ENDL;
    state.audioVolumeChanged=[&]
    {
        LLVKAudio::Volume volume;
        volume.master=static_cast<float>(ui->tree().setting("AudioLevelMaster").value_or(LLSD(1.f)).asReal());
        volume.muted=ui->tree().setting("MuteAudio").value_or(LLSD(false)).asBoolean();
        volume.muteWhenInactive=ui->tree().setting("MuteWhenMinimized").value_or(LLSD(false)).asBoolean();
        volume.windowActive=state.input.editor.applicationFocused;
        std::string problem;
        if (!audio.setVolume(volume,problem)) LL_WARNS("NativeAudio") << (LLVKError{Code::OperationFailed,Operation::Window,1,0}).diagnostic() << LL_ENDL;
        const auto uiGain=static_cast<float>(ui->tree().setting("AudioLevelUI").value_or(LLSD(1.f)).asReal());
        const auto uiMuted=ui->tree().setting("MuteUI").value_or(LLSD(false)).asBoolean();
        if (!audio.setUiGain(uiGain,uiMuted,problem)) LL_WARNS("NativeAudio") << (LLVKError{Code::OperationFailed,Operation::Window,1,0}).diagnostic() << LL_ENDL;
    };
    for (const auto name : {"AudioLevelMaster","MuteAudio","MuteWhenMinimized","AudioLevelUI","MuteUI"})
        if (const auto subscription=ui->tree().subscribeSetting(name,[&](const LLSD&,const LLSD&) { state.audioVolumeChanged(); }))
            services.audioSubscriptions.push_back(*subscription);
    state.audioVolumeChanged();
    ui->setUiSoundPlayer([&](const std::string& asset,std::string& problem)
    {
        if (!audio.active()) return true;
        if (!LLUUID::validate(asset) || configuration.soundCacheDirectory.empty())
        { problem="Native sound cache is unavailable"; return false; }
        std::ifstream file(configuration.soundCacheDirectory/(asset+".dsf"),std::ios::binary|std::ios::ate);
        if (!file) { problem="Native UI sound is not decoded in cache; asset fetching is not yet integrated: "+asset; return false; }
        const auto size=file.tellg();
        if (size<=0 || size>16*1024*1024) { problem="Native UI sound file exceeds the size limit"; return false; }
        std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
        file.seekg(0);
        if (!file.read(reinterpret_cast<char*>(bytes.data()),size)) { problem="Native UI sound read failed"; return false; }
        return audio.playUiWav(bytes,problem);
    });
    auto clipboard = LLVKClipboard::forWindow(state.window,error);
    if (!clipboard) return false;
    ui->setDialogClipboard(clipboard);
    ui->tree().setClipboard(std::move(clipboard));
    auto& renderer=visuals.renderer;
    if (!visuals.initializeRenderer(configuration.validation,error)) return fail(Code::RendererUnavailable);
    const auto surface=visuals.surface;
    const auto synchronizedPresentation = [&] { return ui->tree().setting("RenderVSyncEnable").value_or(LLSD(false)).asBoolean(); };
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
    switch (properties.vendorID)
    {
        case 0x1002: aboutInfo["GRAPHICS_CARD_VENDOR"]="AMD"; break;
        case 0x10de: aboutInfo["GRAPHICS_CARD_VENDOR"]="NVIDIA"; break;
        case 0x8086: aboutInfo["GRAPHICS_CARD_VENDOR"]="Intel"; break;
        default: aboutInfo["GRAPHICS_CARD_VENDOR"]="Vulkan vendor ID "+std::to_string(properties.vendorID); break;
    }
    VkPhysicalDeviceMemoryProperties deviceMemory{};
    vkGetPhysicalDeviceMemoryProperties(renderer.physicalDevice(),&deviceMemory);
    std::uint64_t localBytes=0;
    for (std::uint32_t heap=0; heap<deviceMemory.memoryHeapCount; ++heap)
        if (deviceMemory.memoryHeaps[heap].flags&VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) localBytes+=deviceMemory.memoryHeaps[heap].size;
    aboutInfo["GRAPHICS_CARD_MEMORY"]=std::to_string(localBytes/(1024*1024));
    aboutInfo["GRAPHICS_CARD_MEMORY_DETECTED"]=aboutInfo["GRAPHICS_CARD_MEMORY"];
    LLVKGraphicsPolicy::Device graphicsDevice;
    graphicsDevice.vendor=properties.vendorID;
    graphicsDevice.videoBytes=localBytes;
    MEMORYSTATUSEX memoryStatus{}; memoryStatus.dwLength=sizeof(memoryStatus);
    if (GlobalMemoryStatusEx(&memoryStatus)) graphicsDevice.systemBytes=memoryStatus.ullTotalPhys;
    ui->setGraphicsDevice(graphicsDevice);
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
    auto& gpu=*visuals.gpu;
    ui->setFontTextureDumpHandler([&gpu,logs=configuration.ui.skin.userAppDirectory/"logs"](std::string& problem)
    {
        const auto directory=logs/("native-font-atlases-"+LLUUID::generateNewID().asString());
        const auto files=gpu.dumpFontAtlases(directory,problem);
        if (!files) return false;
        LL_INFOS("NativeFonts") << "Saved " << files->size() << " native font atlas pages to " << directory.string() << LL_ENDL;
        return true;
    });
    auto browserConfiguration = configuration.browser;
    const auto browserProxy=LLVKProxy::select(configuration.ui.settings,true,error);
    if (!browserProxy) return false;
    browserConfiguration.proxy=*browserProxy;
    if (const auto value=ui->tree().setting("BrowserJavascriptEnabled")) browserConfiguration.javascriptEnabled=value->asBoolean();
    if (const auto value=ui->tree().setting("CookiesEnabled")) browserConfiguration.cookiesEnabled=value->asBoolean();
    const auto browserId = ui->find("login_html");
    auto browserRect = ui->tree().screenRect(browserId,error);
    if (!browserRect) return false;
    browserConfiguration.width = browserRect->right-browserRect->left;
    browserConfiguration.height = browserRect->top-browserRect->bottom;
    services.browser=std::make_unique<LLVKBrowser>();
    auto& browser=*services.browser;
    const auto browserLaunchFailed=[&](LLVKWidgetTree::Id widget,std::string& problem)
    {
        ui->tree().setVisible(widget,false);
        LLSD arguments;
        arguments["PLUGIN"]="media_plugin_cef";
        return ui->queueNotice("MediaPluginFailed",arguments,{},problem);
    };
    const bool loginBrowserStarted=browser.start(browserConfiguration,error);
    if (!loginBrowserStarted)
    {
        if (!browserLaunchFailed(browserId,error)) return fail(Code::BrowserUnavailable);
    }
    else
    {
        state.browser=&browser;
        if (!browser.navigate(configuration.loginPage,error)) return fail(Code::BrowserUnavailable);
        aboutInfo["LIBCEF_VERSION"]=browser.versionInfo(error);
        if (!error.empty() || !ui->setAboutInfo(aboutInfo,error)) return false;
    }
    const auto bindBrowser=[&](LLVKWidgetTree::Id widget,LLVKBrowser& view)
    {
        state.browserViews[widget]=&view;
        LLVKWidgetTree::Events events;
        events.pointer = [&](auto id,const auto& event)
        {
            const auto* node = ui->tree().get(id);
            const auto target=state.browserViews.find(id);
            if (!node || target==state.browserViews.end() || target->second->state()!=LLVKBrowser::State::Running) return;
            const auto height = node->params.rect.top-node->params.rect.bottom;
            const auto display=LLVKBrowserSurface::displayRect(node->params.rect.right-node->params.rect.left,height,
                target->second->surface().width(),target->second->surface().height());
            if (event.kind == LLVKWidgetTree::PointerKind::LeftDown || event.kind == LLVKWidgetTree::PointerKind::DoubleClick)
            { ui->tree().setKeyboardFocus(id,false,false,state.error); ui->tree().setMouseCapture(id,state.error); target->second->pointer(event.x-display.left,display.top-1-event.y,0,true,state.error); }
            else if (event.kind == LLVKWidgetTree::PointerKind::LeftUp)
            { target->second->pointer(event.x-display.left,display.top-1-event.y,0,false,state.error); ui->tree().setMouseCapture(0,state.error); }
        };
        ui->tree().setEvents(widget,std::move(events));
    };
    if (loginBrowserStarted) bindBrowser(browserId,browser);
    ui->setGuidebookService([&](auto id,const std::string& url,std::string& problem)
    {
        if (services.browsers.size()>=16) { problem="Native embedded browser view limit reached"; return false; }
        const auto rect=ui->tree().screenRect(id,problem);
        if (!rect) return false;
        auto settings=browserConfiguration;
        settings.width=rect->right-rect->left; settings.height=rect->top-rect->bottom;
        const auto [entry,inserted]=services.browsers.try_emplace(id,std::make_unique<LLVKBrowser>());
        if (!inserted) { problem="Native embedded browser already has an owner"; return false; }
        auto& view=*entry->second;
        if (!view.start(settings,problem))
        {
            services.browsers.erase(entry);
            return browserLaunchFailed(id,problem);
        }
        if (!view.navigate(url,problem)) return false;
        bindBrowser(id,view);
        return true;
    },[&](auto id)
    {
        ui->tree().setEvents(id,{});
        state.browserViews.erase(id);
        state.input.browsers.erase(id);
        state.input.browserEpochs.erase(id);
        const auto view=services.browsers.find(id);
        if (view!=services.browsers.end()) view->second->requestClose(state.error);
    });
    ui->setBrowserCommand([&](auto id,const std::string& action,const std::string& url,std::string& problem)
    {
        const auto found=services.browsers.find(id);
        if (found==services.browsers.end()) { problem="Native browser owner is unavailable"; return false; }
        auto& view=*found->second;
        if (action=="Navigate") return view.navigate(url,problem);
        using Command=LLVKBrowser::Command;
        if (action=="Back") return view.command(Command::Back,problem);
        if (action=="Forward") return view.command(Command::Forward,problem);
        if (action=="Reload") return view.command(Command::Reload,problem);
        if (action=="Stop") return view.command(Command::Stop,problem);
        problem="Unknown native browser action"; return false;
    });
    if (ui->tree().setting("floater_vis_guidebook").value_or(LLSD(false)).asBoolean() && !ui->toggleGuidebook(error)) return false;
    services.joystick=std::make_unique<LLVKJoystick>();
    auto& joystick=*services.joystick;
    bool joystickStarted=false;
    LLVKViewerUi::JoystickServices joystickServices;
    const auto startJoystick=[&](std::string& problem)
    {
        if (joystickStarted) return true;
        joystickStarted=joystick.start(state.window,problem);
        return joystickStarted;
    };
    joystickServices.enumerate=[&](std::string& problem) -> std::optional<std::vector<LLVKJoystick::Device>>
    {
        if (!startJoystick(problem) || !joystick.enumerate(problem)) return std::nullopt;
        return joystick.devices();
    };
    joystickServices.select=[&](const LLSD& requested,std::string& problem) -> std::optional<std::string>
    {
        if (!startJoystick(problem)) return std::nullopt;
        LLSD id=requested;
        if (requested.isString())
        {
            const auto value=requested.asString();
            if (value.empty()) id=joystick.devices().empty() ? LLSD(0) : joystick.devices().front().id;
            else
            {
                GUID guid{};
                if (FAILED(CLSIDFromString(ll_convert<std::wstring>(value).c_str(),&guid)))
                { problem="Invalid saved native joystick GUID"; return std::nullopt; }
                LLSD::Binary bytes(sizeof(guid)); std::memcpy(bytes.data(),&guid,sizeof(guid)); id=LLSD(bytes);
                const auto found=std::find_if(joystick.devices().begin(),joystick.devices().end(),[&](const auto& device)
                { return device.id.asBinary()==bytes; });
                if (found==joystick.devices().end())
                { if (!joystick.select(LLSD(0),problem)) return std::nullopt; return value; }
            }
        }
        if (!joystick.select(id,problem)) return std::nullopt;
        if (!joystick.selected().isBinary()) return std::string();
        GUID guid{}; const auto bytes=joystick.selected().asBinary(); std::memcpy(&guid,bytes.data(),sizeof(guid));
        wchar_t identity[40]{};
        if (!StringFromGUID2(guid,identity,40)) { problem="Native joystick identity conversion failed"; return std::nullopt; }
        return ll_convert<std::string>(std::wstring(identity));
    };
    joystickServices.poll=[&](std::string& problem) -> std::optional<LLVKJoystick::State>
    {
        if (!startJoystick(problem) || !joystick.poll(problem)) return std::nullopt;
        return joystick.state();
    };
    ui->setJoystickServices(std::move(joystickServices));
    services.translation=std::make_unique<TranslationVerification>(*ui,configuration.ui.skin.executableDirectory/"ca-bundle.crt");
    const auto voiceSetting=[&](const std::string& name,const LLSD& fallback)
    {
        if (configuration.ui.settingsGroup)
            if (const auto control=configuration.ui.settingsGroup->getControl(name)) return control->getValue();
        const auto found=configuration.ui.settings.find(name);
        return found==configuration.ui.settings.end() ? fallback : found->second;
    };
    services.voice=std::make_unique<LLVKVoice>(voiceSetting("VoiceInputAudioDevice",LLSD("Default")).asString(),
        voiceSetting("VoiceOutputAudioDevice",LLSD("Default")).asString());
    auto& voice=*services.voice;
    const auto updateVoice=[&]()
    {
        LLVKVoice::AudioConfig processing;
        processing.mEchoCancellation=voiceSetting("VoiceEchoCancellation",LLSD(true)).asBoolean();
        processing.mAGC=voiceSetting("VoiceAutomaticGainControl",LLSD(true)).asBoolean();
        const auto level=voiceSetting("VoiceNoiseSuppressionLevel",LLSD(4)).asInteger();
        if (level<0 || level>4) { error="Invalid native voice noise suppression setting"; return false; }
        processing.mNoiseSuppressionLevel=static_cast<LLVKVoice::AudioConfig::ENoiseSuppressionLevel>(level);
        return voice.configure(processing,error);
    };
    if (!updateVoice()) return fail(Code::VoiceFailed);
    services.voiceDevices=std::make_unique<VoiceDevices>(*ui,voice);
    services.picker=std::make_unique<XmlFilePicker>(*ui,state.window);
    if (configuration.textureCache)
        services.previews=std::make_unique<LLVKTexturePreview>(*configuration.textureCache,ui->tree(),ui->root());
    ShowWindow(state.window,ui->tree().setting("WindowMaximized").value_or(LLSD(false)).asBoolean() ? SW_SHOWMAXIMIZED : SW_SHOW);
    if (configuration.bindServices) configuration.bindServices(*ui);
    LLVKUiPacket packet(renderer.swapchainExtent());
    std::uint32_t frames = 0;
    auto previous = std::chrono::steady_clock::now();
    while (!state.close)
    {
        if (configuration.fatalError)
            if (const auto failure=configuration.fatalError()) return fail(*failure);
        {
            const auto snapshot=application.owner->snapshot();
            if (snapshot.state==LLVKSessionOwner::State::Stopped) break;
            if (snapshot.state==LLVKSessionOwner::State::Disconnecting && snapshot.cleanup.action==LLVKSessionOwner::Action::Wait)
                application.owner->retryCleanup(snapshot.tag);
            else application.owner->pumpOne();
            if (!ui->refreshSession(error)) return fail(Code::StartupResources);
            if (application.owner->snapshot().state==LLVKSessionOwner::State::Stopped) break;
        }
        if (application.access->service && application.access->service->active())
        {
            if (services.previews && !services.previews->update(error)) return fail(Code::PreviewFailed);
            services.picker->pump();
            if (!services.translation->pump(error)) return fail(Code::TranslationFailed);
            if (!audio.update(error)) return fail(Code::AudioFailed);
            if (!updateVoice()) return fail(Code::VoiceFailed);
        }
        if (!ui->advanceNotices(state.elapsed(),error)) return false;
        MSG message;
        while (PeekMessageW(&message,nullptr,0,0,PM_REMOVE))
        { if (message.message == WM_QUIT) state.quitRequested = true; TranslateMessage(&message); DispatchMessageW(&message); }
        if (!state.error.empty()) { error = state.error; return false; }
        if (state.quitRequested)
        {
            std::map<std::string,LLSD> placement;
            if (!IsIconic(state.window))
            {
                const bool maximized=IsZoomed(state.window)!=FALSE;
                placement["WindowMaximized"]=maximized;
                if (!maximized)
                {
                    RECT bounds{},client{};
                    if (!GetWindowRect(state.window,&bounds) || !GetClientRect(state.window,&client))
                    { error="Native window placement could not be saved"; return false; }
                    placement["WindowX"]=static_cast<int>(bounds.left);
                    placement["WindowY"]=static_cast<int>(bounds.top);
                    placement["WindowWidth"]=static_cast<int>(client.right-client.left);
                    placement["WindowHeight"]=static_cast<int>(client.bottom-client.top);
                }
            }
            const auto shutdown=ui->prepareShutdown(error,placement);
            if (shutdown==LLVKViewerUi::ShutdownStatus::Failed) return fail(Code::SettingsWrite);
            if (shutdown==LLVKViewerUi::ShutdownStatus::Ready)
            {
                if (application.owner->snapshot().state!=LLVKSessionOwner::State::Disconnecting)
                    application.owner->shutdown();
                if (application.owner->snapshot().state==LLVKSessionOwner::State::Stopped) break;
                state.quitRequested=false;
                if (!ui->refreshSession(error,true)) return fail(Code::StartupResources);
            }
        }
        if (const auto problem=ui->takeDialogError(); !problem.empty())
            notice(Code::OperationFailed);
        if (application.owner->snapshot().state==LLVKSessionOwner::State::Stopped) break;
        if (application.access->service && application.access->service->active())
        {
        if (loginBrowserStarted && !browser.update(error)) return fail(Code::BrowserUnavailable);
        for (const auto& event : browser.takeEvents())
            if (event.kind == LLVKBrowser::EventKind::LoadError)
            { error = "Native login page failed"; return fail(Code::BrowserUnavailable); }
        std::vector<std::pair<std::string,std::string>> popups;
        for (auto iterator=services.browsers.begin(); iterator!=services.browsers.end(); )
        {
            const auto id=iterator->first;
            auto& view=*iterator->second;
            if (!view.update(error)) return fail(Code::BrowserUnavailable);
            if (view.state()==LLVKBrowser::State::Closed)
            {
                ui->webBrowserEvent(id,"Closed","",false,false);
                state.browserViews.erase(id); state.input.browsers.erase(id); state.input.browserEpochs.erase(id);
                iterator=services.browsers.erase(iterator); continue;
            }
            const auto navigation=view.state()==LLVKBrowser::State::Running ? view.navigation(error) : std::nullopt;
            for (const auto& event : view.takeEvents())
            {
                if (event.kind==LLVKBrowser::EventKind::LoadError && event.code!=-3)
                    LL_WARNS("NativeGuidebook") << (LLVKError{Code::OperationFailed,Operation::Browser,1,0}).diagnostic() << LL_ENDL;
                if (!navigation) continue;
                std::string kind;
                using Event=LLVKBrowser::EventKind;
                switch (event.kind)
                {
                case Event::Address: kind="Address"; break;
                case Event::Title: kind="Title"; break;
                case Event::Status: kind="Status"; break;
                case Event::LoadStart: kind="LoadStart"; break;
                case Event::LoadEnd: kind="LoadEnd"; break;
                case Event::Popup: popups.emplace_back(event.text,event.detail); break;
                default: break;
                }
                if (!kind.empty()) ui->webBrowserEvent(id,kind,event.text,navigation->back,navigation->forward);
            }
            if (view.state()==LLVKBrowser::State::Running && ui->tree().get(id))
            {
                const auto rectangle=ui->tree().screenRect(id,error);
                if (!rectangle) return false;
                const auto width=rectangle->right-rectangle->left,height=rectangle->top-rectangle->bottom;
                if (width>0 && height>0 && (view.surface().width()!=static_cast<std::uint32_t>(width) ||
                    view.surface().height()!=static_cast<std::uint32_t>(height)) && !view.resize(width,height,error)) return false;
                state.input.browsers[id]=view.surface().frame();
                state.input.browserEpochs[id]=view.surface().epoch();
            }
            ++iterator;
        }
        for (const auto& [url,target] : popups)
        {
            if (target=="_external") openUrl(url);
            else
            {
                std::string problem;
                if (!ui->showMediaBrowser(url,problem,target))
                    LL_WARNS("NativeBrowser") << (LLVKError{Code::OperationFailed,Operation::Browser,1,0}).diagnostic() << LL_ENDL;
            }
        }
        }
        if ((state.resize || renderer.synchronizedPresentationRequested() != synchronizedPresentation()) && state.width && state.height)
        {
            ui->menu().dismiss();
            if (!renderer.createSwapchain(surface,state.width,state.height,error,synchronizedPresentation())) return fail(Code::RendererUnavailable);
            if (!ui->tree().reshape(ui->root(),renderer.swapchainExtent().width,renderer.swapchainExtent().height,error)) return false;
            if (!ui->tree().prepareLayoutStacks(ui->root(),0,error)) return false;
            browserRect = ui->tree().screenRect(browserId,error);
            if (!browserRect) return false;
            if (loginBrowserStarted && application.access->service && application.access->service->active() &&
                !browser.resize(browserRect->right-browserRect->left,browserRect->top-browserRect->bottom,error)) return false;
            state.resize = false;
            aboutInfo["WINDOW_WIDTH"]=static_cast<int>(renderer.swapchainExtent().width);
            aboutInfo["WINDOW_HEIGHT"]=static_cast<int>(renderer.swapchainExtent().height);
            if (!ui->setAboutInfo(aboutInfo,error)) return false;
        }
        const auto now = std::chrono::steady_clock::now();
        state.input.button.frameDelta = std::chrono::duration<float>(now-previous).count();
        state.input.animationSeconds=state.elapsed();
        previous = now;
        state.input.button.spaceDown = bool(GetKeyState(VK_SPACE)&0x8000);
        state.input.button.returnDown = bool(GetKeyState(VK_RETURN)&0x8000);
        state.input.editor.secondsSinceKeystroke = std::chrono::duration<double>(now-state.keystroke).count();
        if (loginBrowserStarted && application.access->service && application.access->service->active())
        {
            state.input.browsers[browserId] = browser.surface().frame();
            state.input.browserEpochs[browserId] = browser.surface().epoch();
        }
        ui->tree().setInputModifiers({bool(GetKeyState(VK_SHIFT)&0x8000),bool(GetKeyState(VK_CONTROL)&0x8000),bool(GetKeyState(VK_MENU)&0x8000)});
        ui->tree().advanceTime(state.elapsed(),error);
        if (state.width && state.height)
        {
            const auto paint = ui->preparePaint(state.input,error);
            if (!paint) return fail(Code::StartupResources);
            const auto ready = gpu.prepare(*paint,renderer.swapchainExtent(),packet,error);
            if (ready == LLVKWidgetGpu::Status::Failed) return fail(Code::RendererUnavailable);
            if (ready == LLVKWidgetGpu::Status::Ready)
            {
                if (renderer.begin2DFrame(0.16f,0.16f,0.16f,1))
                {
                    if (!renderer.recordUiPacket(packet.vertices(),packet.draws())) { error = renderer.frameError(); return fail(Code::RendererUnavailable); }
                    if (!renderer.end2DFrame() && renderer.frameResult() != LLVKContext::FrameResult::OutOfDate)
                    { error = renderer.frameError(); return fail(Code::RendererUnavailable); }
                    if (configuration.presentedFrame) configuration.presentedFrame(*ui,state.input);
                    if (configuration.stopAfterFrames && ++frames >= configuration.stopAfterFrames &&
                        !PostMessageW(state.window,WM_CLOSE,0,0))
                    { error="Native test close request failed"; return false; }
                }
                if (renderer.frameResult() == LLVKContext::FrameResult::OutOfDate) state.resize = true;
                else if (renderer.frameResult() == LLVKContext::FrameResult::Fatal) { error = renderer.frameError(); return fail(Code::RendererUnavailable); }
            }
        }
        MsgWaitForMultipleObjectsEx(0,nullptr,16,QS_ALLINPUT,MWMO_INPUTAVAILABLE);
    }
    if (application.owner->snapshot().state!=LLVKSessionOwner::State::Stopped)
    { error="Native window exited before application retirement completed"; return fail(Code::ShutdownFailed); }
    ui->setSessionOwner(nullptr);
    if (!visuals.prepareShutdown(error)) return fail(Code::ShutdownFailed);
    return true;
}