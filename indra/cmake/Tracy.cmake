# Tracy Profiler support.
if (USE_TRACY_PROFILER)
    include(Prebuilt)
    use_prebuilt_binary(Tracy)
    if (WINDOWS)
        # set(TRACY_LIBRARIES 
        #     ${ARCH_PREBUILT_DIRS_RELEASE}/Tracy.lib
        add_definitions(-DTRACY_ENABLE=1 -DTRACY_NO_FASTTIMERS)
        set(TRACY_INCLUDE_DIR ${LIBS_PREBUILT_DIR}/include/tracy)
    elseif (DARWIN)
        message(FATAL_ERROR "Tracy is not yet implemented in FS for OSX.")
    else (WINDOWS)
        message(FATAL_ERROR "Tracy is not yet implemented in FS for Linux.")
    endif (WINDOWS)
endif (USE_TRACY_PROFILER)
