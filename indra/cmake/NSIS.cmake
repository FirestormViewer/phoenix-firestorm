# -*- cmake -*-
# Legacy NSIS installer integration for the VulkanStorm build.
# NSIS is the opt-in legacy packager; Inno Setup 7 (USE_INNOSETUP) is the default.

include_guard()

# USE_NSIS controls whether the installer is built with the legacy NSIS
# packager instead of the default Inno Setup 7. The option value is forwarded
# to viewer_manifest.py.
option(USE_NSIS "Use the legacy NSIS installer instead of Inno Setup 7" OFF)
