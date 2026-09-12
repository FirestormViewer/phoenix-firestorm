include(FetchContent)
FetchContent_Declare(llvk_cef_headers
    URL "https://cef-builds.spotifycdn.com/cef_binary_139.0.40%2Bg465474a%2Bchromium-139.0.7258.139_windows64_minimal.tar.bz2"
    URL_HASH SHA1=403afa6001a7cea20f64bff5132bad4530541755
    SOURCE_SUBDIR native_headers_only
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_Declare(llvk_dullahan_source
    URL "https://codeload.github.com/secondlife/dullahan/zip/49a551c0216ac7db03e36c9cc7ec44650c0be1c4"
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

set(native_dullahan_sources "${CMAKE_CURRENT_BINARY_DIR}/native_dullahan.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/native_dullahan_impl.cpp")
foreach(source dullahan_browser_client.cpp dullahan_callback_manager.cpp
    dullahan_impl_keyboard_win.cpp dullahan_impl_mouse.cpp dullahan_render_handler.cpp)
    list(APPEND native_dullahan_sources "${llvk_dullahan_source_SOURCE_DIR}/src/${source}")
endforeach()
add_library(llvk_dullahan STATIC ${native_dullahan_sources})
target_include_directories(llvk_dullahan SYSTEM PRIVATE
    "${llvk_cef_headers_SOURCE_DIR}" "${llvk_cef_headers_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_BINARY_DIR}" "${llvk_dullahan_source_SOURCE_DIR}/src")
target_link_libraries(llvk_dullahan PRIVATE libcef.lib libcef_dll_wrapper.lib)
target_compile_options(llvk_dullahan PRIVATE /W0)