include(Linking)
include(Prebuilt)

include_guard()

add_library( ll::apr INTERFACE IMPORTED )

if (NOT LINUX)
  use_system_binary( apr apr-util )
  use_prebuilt_binary(apr_suite)
else ()
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(APR REQUIRED apr-1)
  pkg_check_modules(APRUTIL REQUIRED apr-util-1)
endif ()

if (WINDOWS)
  if (LLCOMMON_LINK_SHARED)
    set(APR_selector "lib")
  else (LLCOMMON_LINK_SHARED)
    set(APR_selector "")
  endif (LLCOMMON_LINK_SHARED)
  target_link_libraries( ll::apr INTERFACE
          ${ARCH_PREBUILT_DIRS_RELEASE}/${APR_selector}apr-1.lib
          ${ARCH_PREBUILT_DIRS_RELEASE}/${APR_selector}aprutil-1.lib
          )
elseif (DARWIN)
  if (LLCOMMON_LINK_SHARED)
    set(APR_selector     "0.dylib")
    set(APRUTIL_selector "0.dylib")
  else (LLCOMMON_LINK_SHARED)
    set(APR_selector     "a")
    set(APRUTIL_selector "a")
  endif (LLCOMMON_LINK_SHARED)

  target_link_libraries( ll::apr INTERFACE
          libapr-1.${APR_selector}
          libaprutil-1.${APRUTIL_selector}
          iconv
          )
else (WINDOWS)
  # linux
  target_link_libraries( ll::apr INTERFACE
          ${APR_LIBRARIES}
          ${APRUTIL_LIBRARIES}
          uuid
          rt
          )
endif (WINDOWS)

if (NOT LINUX)
  target_include_directories( ll::apr SYSTEM INTERFACE  ${LIBS_PREBUILT_DIR}/include/apr-1 )
else ()
  target_include_directories( ll::apr SYSTEM INTERFACE  ${APR_INCLUDE_DIRS} ${APRUTIL_INCLUDE_DIRS} )
endif ()