# -*- cmake -*-
include(Prebuilt)

if (NOT USESYSTEMLIBS)
  if (LINUX)
    use_prebuilt_binary(libuuid)
    find_package(Fontconfig REQUIRED) # <FS:PC> fontconfig and freetype should be taken from the
    # use_prebuilt_binary(fontconfig) #         user's system, and not be packaged with the viewer
  endif (LINUX)
  use_prebuilt_binary(libhunspell)
  use_prebuilt_binary(slvoice)
#  use_prebuilt_binary(libidn)
endif(NOT USESYSTEMLIBS)

