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

set(native_dullahan_sources "${CMAKE_CURRENT_BINARY_DIR}/native_dullahan.cpp")
foreach(source dullahan_browser_client.cpp dullahan_callback_manager.cpp dullahan_impl.cpp
    dullahan_impl_keyboard_win.cpp dullahan_impl_mouse.cpp dullahan_render_handler.cpp)
    list(APPEND native_dullahan_sources "${llvk_dullahan_source_SOURCE_DIR}/src/${source}")
endforeach()
add_library(llvk_dullahan STATIC ${native_dullahan_sources})
target_include_directories(llvk_dullahan SYSTEM PRIVATE
    "${llvk_cef_headers_SOURCE_DIR}" "${llvk_cef_headers_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_BINARY_DIR}" "${llvk_dullahan_source_SOURCE_DIR}/src")
target_link_libraries(llvk_dullahan PRIVATE libcef.lib libcef_dll_wrapper.lib)
target_compile_options(llvk_dullahan PRIVATE /W0)