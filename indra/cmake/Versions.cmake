include(BuildVersion)

if(VIEWER)
  build_version(viewer)
  build_channel(viewer)
endif(VIEWER)
