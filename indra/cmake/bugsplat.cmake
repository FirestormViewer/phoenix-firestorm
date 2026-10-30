include_guard()

# <FS:Beq> Linux crash reporting uses our Breakpad package and BugSplat database
# metadata, rather than the proprietary Windows/macOS BugSplat SDK.
if (LINUX)
    if (RELEASE_CRASH_REPORTING OR NON_RELEASE_CRASH_REPORTING)
        if (NOT BUGSPLAT_DB)
            message(FATAL_ERROR "Linux crash reporting requires BUGSPLAT_DB")
        endif ()
        set(USE_BUGSPLAT ON CACHE BOOL "Use the BugSplat crash reporting system" FORCE)
    else ()
        set(USE_BUGSPLAT OFF CACHE BOOL "Use the BugSplat crash reporting system" FORCE)
        set(BUGSPLAT_DB "" CACHE STRING "BugSplat crash database name" FORCE)
    endif ()
elseif (INSTALL_PROPRIETARY)
    # Note that viewer_manifest.py makes decision based on BUGSPLAT_DB and not USE_BUGSPLAT
    if (BUGSPLAT_DB)
        set(USE_BUGSPLAT ON  CACHE BOOL "Use the BugSplat crash reporting system")
    else (BUGSPLAT_DB)
        set(USE_BUGSPLAT OFF CACHE BOOL "Use the BugSplat crash reporting system")
    endif (BUGSPLAT_DB)
else ()
    set(USE_BUGSPLAT OFF CACHE BOOL "Use the BugSplat crash reporting system")
    set(BUGSPLAT_DB "" CACHE STRING "BugSplat crash database name")
endif ()

# </FS:Beq>
add_library( ll::bugsplat INTERFACE IMPORTED )

# <FS:Beq> Keep Linux Breakpad reachable and select only its platform's package.
if (USE_BUGSPLAT)
    include(Prebuilt)
    if (WINDOWS)
        use_prebuilt_binary(bugsplat)
        target_link_libraries( ll::bugsplat INTERFACE
                ${ARCH_PREBUILT_DIRS_RELEASE}/bugsplat.lib
                )
        target_include_directories( ll::bugsplat SYSTEM INTERFACE ${LIBS_PREBUILT_DIR}/include/bugsplat)
    elseif (DARWIN)
        use_prebuilt_binary(bugsplat)
        find_library(BUGSPLAT_LIBRARIES BugsplatMac REQUIRED
                NO_DEFAULT_PATH PATHS "${ARCH_PREBUILT_DIRS_RELEASE}")
        find_library(CRASHREPORTED_LIBRARIES CrashReporter REQUIRED
                NO_DEFAULT_PATH PATHS "${ARCH_PREBUILT_DIRS_RELEASE}")
        find_library(HOCKEYSDK_LIBRARIES HockeySDK REQUIRED
                NO_DEFAULT_PATH PATHS "${ARCH_PREBUILT_DIRS_RELEASE}")
        target_link_libraries( ll::bugsplat INTERFACE
                ${BUGSPLAT_LIBRARIES}
                ${CRASHREPORTED_LIBRARIES}
                ${HOCKEYSDK_LIBRARIES}
                )
    else (WINDOWS) #ie: Linux...
               # Do not add directory-local definitions: these targets reuse a
               # common PCH. Modern C++ exposes inttypes format macros directly.
               use_prebuilt_binary(breakpad)
               target_link_libraries( ll::bugsplat INTERFACE
                       ${ARCH_PREBUILT_DIRS_RELEASE}/libbreakpad.a
                       ${ARCH_PREBUILT_DIRS_RELEASE}/libbreakpad_client.a
                       )
               target_include_directories( ll::bugsplat SYSTEM INTERFACE
                       ${LIBS_PREBUILT_DIR}/include
                       ${LIBS_PREBUILT_DIR}/include/breakpad)
    endif (WINDOWS)

    if( NOT BUGSPLAT_DB )
        message( FATAL_ERROR "You need to set BUGSPLAT_DB when setting USE_BUGSPLAT" )
    endif()

    set_property( TARGET ll::bugsplat APPEND PROPERTY INTERFACE_COMPILE_DEFINITIONS LL_BUGSPLAT)
else()
    set(USE_BUGSPLAT OFF CACHE BOOL "Use the BugSplat crash reporting system" FORCE)
    set(BUGSPLAT_DB "" CACHE STRING "BugSplat crash database name" FORCE)
endif ()
# </FS:Beq>

