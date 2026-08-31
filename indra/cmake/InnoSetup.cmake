# -*- cmake -*-
# Inno Setup 7 installer integration for the VulkanStorm build.
# https://jrsoftware.org/isinfo.php

include_guard()

# USE_INNOSETUP controls whether the installer is built with Inno Setup 7
# instead of the legacy NSIS packager. Inno Setup is a build-machine tool
# (ISCC.exe); the viewer itself links nothing extra, so no prebuilt package
# is required here. The option value is forwarded to viewer_manifest.py.
option(USE_INNOSETUP "Use Inno Setup 7 for installer packaging (default; set OFF to use the legacy NSIS packager)" ON)
