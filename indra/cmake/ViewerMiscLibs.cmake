# -*- cmake -*-
include(Prebuilt)

if (NOT USESYSTEMLIBS)
  if (LINUX)
    use_prebuilt_binary(libuuid)
    use_prebuilt_binary(fontconfig)
  endif (LINUX)
  use_prebuilt_binary(libhunspell)
  use_prebuilt_binary(slvoice)
  # <FS:Ansariel> FIRE-22709: Local voice not working in OpenSim
  if (OPENSIM)
    if (WINDOWS OR DARWIN)
      use_prebuilt_binary(slvoice_os)
    endif (WINDOWS OR DARWIN)
  endif (OPENSIM)
  # </FS:Ansariel>
#  use_prebuilt_binary(libidn)
endif(NOT USESYSTEMLIBS)

