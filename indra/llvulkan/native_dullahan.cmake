include(FetchContent)
FetchContent_Declare(llvk_cef_headers
    URL "https://cef-builds.spotifycdn.com/cef_binary_152.0.6%2Bg708dc14%2Bchromium-152.0.7977.83_windows64_minimal.tar.bz2"
    URL_HASH SHA1=e5e3020627f4528bd43e22f4c4970000b0458e99
    SOURCE_SUBDIR native_headers_only
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_Declare(llvk_dullahan_source
    URL "https://codeload.github.com/secondlife/dullahan/zip/f75972f4cba3a01a23007ed79b6c204ececf352c"
    SOURCE_SUBDIR native_sources_only
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(llvk_cef_headers llvk_dullahan_source)

file(READ "${llvk_dullahan_source_SOURCE_DIR}/src/dullahan.cpp" facade)
set(constructor "DLNOUT(\"dullahan::dullahan()\");")
set(destructor "DLNOUT(\"dullahan::~dullahan()\");")
string(FIND "${facade}" "${constructor}" constructor_position)
string(FIND "${facade}" "${destructor}" destructor_position)
if(constructor_position LESS 0 OR destructor_position LESS 0)
    message(FATAL_ERROR "Pinned native Dullahan lifetime patch no longer matches")
endif()
string(REPLACE "${constructor}" "${constructor}\n    mImpl->AddRef();" facade "${facade}")
string(REPLACE "${destructor}" "${destructor}\n    mImpl.release()->Release();" facade "${facade}")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/native_dullahan.cpp" CONTENT "${facade}")
configure_file("${LIBS_PREBUILT_DIR}/include/cef/dullahan_version.h"
    "${CMAKE_CURRENT_BINARY_DIR}/dullahan_version.h" COPYONLY)

file(READ "${llvk_dullahan_source_SOURCE_DIR}/src/dullahan_impl.cpp" implementation)
function(native_browser_replace original replacement)
    string(FIND "${implementation}" "${original}" position)
    if(position LESS 0)
        message(FATAL_ERROR "Pinned native Dullahan runtime patch no longer matches: ${original}")
    endif()
    string(REPLACE "${original}" "${replacement}" implementation "${implementation}")
    set(implementation "${implementation}" PARENT_SCOPE)
endfunction()
native_browser_replace("        platformAddCommandLines(command_line);" [=[
    command_line->AppendSwitchWithValue("use-gl", "angle");
    command_line->AppendSwitchWithValue("use-angle", "d3d11");
    platformAddCommandLines(command_line);]=])
native_browser_replace("dullahan_impl::dullahan_impl() :" [=[
namespace
{
    CefRefPtr<dullahan_impl> nativeRuntimeOwner;
    unsigned int nativeRuntimeViews = 0;
    bool nativeRuntimeConsumed = false;
}

dullahan_impl::dullahan_impl() :]=])
native_browser_replace("bool result = CefInitialize(args, settings, this, nullptr);" [=[
bool result = nativeRuntimeOwner != nullptr;
    if (!result)
    {
        if (nativeRuntimeConsumed) return false;
        nativeRuntimeConsumed = true;
        result = CefInitialize(args, settings, this, nullptr);
        if (result) nativeRuntimeOwner = this;
    }
    if (result) ++nativeRuntimeViews;]=])
native_browser_replace("    CefBrowserSettings browser_settings;" "    mInitialized = true;\n    CefBrowserSettings browser_settings;")
native_browser_replace("    // important: set the size *after* we create a browser" [=[
    if (!mBrowser)
    {
        shutdown();
        return false;
    }

    // important: set the size *after* we create a browser]=])
native_browser_replace("void dullahan_impl::shutdown()\n{" "void dullahan_impl::shutdown()\n{\n    if (!mInitialized) return;\n    mInitialized = false;")
native_browser_replace("    CefShutdown();" [=[
    if (--nativeRuntimeViews == 0)
    {
        CefShutdown();
        nativeRuntimeOwner = nullptr;
    }]=])
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/native_dullahan_impl.cpp" CONTENT "${implementation}")

file(READ "${llvk_dullahan_source_SOURCE_DIR}/src/dullahan_render_handler.cpp" implementation)
native_browser_replace("        memcpy(mPopupBuffer, buffer, width * height * mBufferDepth);" [=[        if (!mPopupBuffer || !buffer || width != mPopupBufferRect.width || height != mPopupBufferRect.height) return;
        memcpy(mPopupBuffer, buffer, size_t(width) * height * mBufferDepth);]=])
native_browser_replace([=[    mPopupBufferRect = rect;
    if (mPopupBuffer == nullptr)
    {
        mPopupBuffer = new unsigned char[rect.width * rect.height * mBufferDepth];
        memset(mPopupBuffer, 0xff, rect.width * rect.height * mBufferDepth);
    }]=] [=[    delete[] mPopupBuffer;
    mPopupBuffer = nullptr;
    mPopupBufferRect.Set(0, 0, 0, 0);
    if (rect.width <= 0 || rect.height <= 0 || rect.width > 8192 || rect.height > 8192 ||
        uint64_t(rect.width) * rect.height > 16 * 1024 * 1024 || mBufferDepth != 4) return;
    mPopupBufferRect = rect;
    const auto bytes = size_t(rect.width) * rect.height * mBufferDepth;
    mPopupBuffer = new unsigned char[bytes];
    memset(mPopupBuffer, 0xff, bytes);]=])
native_browser_replace([=[    int popup_y = (mFlipYPixels ? (mPixelBufferHeight - mPopupBufferRect.y) : mPopupBufferRect.y);
    unsigned char* src = (unsigned char*)mPopupBuffer;
    unsigned char* dst = mPixelBuffer + popup_y * mPixelBufferWidth * mBufferDepth + mPopupBufferRect.x * mBufferDepth;
    while (src < (unsigned char*)mPopupBuffer + mPopupBufferRect.width * mPopupBufferRect.height * mBufferDepth)
    {
        memcpy(dst, src, mPopupBufferRect.width * mBufferDepth);
        src += mPopupBufferRect.width * mBufferDepth;
        dst += mPixelBufferWidth * mBufferDepth * (mFlipYPixels ? -1 : 1);
    }]=] [=[    if (!mPixelBuffer || !mPopupBuffer || mBufferDepth != 4) return;
    LLVKBrowserSurface::compositePopup(
        {mPixelBuffer, size_t(mPixelBufferWidth) * mPixelBufferHeight * 4}, mPixelBufferWidth, mPixelBufferHeight,
        {mPopupBuffer, size_t(mPopupBufferRect.width) * mPopupBufferRect.height * 4},
        mPopupBufferRect.width, mPopupBufferRect.height, mPopupBufferRect.x, mPopupBufferRect.y, mFlipYPixels);]=])
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/native_dullahan_render_handler.cpp"
    CONTENT "#include \"llvkbrowsersurface.h\"\n${implementation}")

set(native_dullahan_sources "${CMAKE_CURRENT_BINARY_DIR}/native_dullahan.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/native_dullahan_impl.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/native_dullahan_render_handler.cpp")
foreach(source dullahan_browser_client.cpp dullahan_callback_manager.cpp
    dullahan_embed_scheme.cpp dullahan_impl_keyboard_win.cpp dullahan_impl_mouse.cpp)
    configure_file("${llvk_dullahan_source_SOURCE_DIR}/src/${source}"
        "${CMAKE_CURRENT_BINARY_DIR}/native_${source}" COPYONLY)
    list(APPEND native_dullahan_sources "${CMAKE_CURRENT_BINARY_DIR}/native_${source}")
endforeach()
add_library(llvk_dullahan STATIC ${native_dullahan_sources})
target_include_directories(llvk_dullahan SYSTEM PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${llvk_cef_headers_SOURCE_DIR}" "${llvk_cef_headers_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_BINARY_DIR}" "${llvk_dullahan_source_SOURCE_DIR}/src")
target_link_libraries(llvk_dullahan PRIVATE libcef.lib libcef_dll_wrapper.lib)
target_compile_options(llvk_dullahan PRIVATE /W0)