include (Prebuilt)
if(NOT CMAKE_GENERATOR_PLATFORM STREQUAL "ARM64")
  use_prebuilt_binary(viewer-manager)
endif()

