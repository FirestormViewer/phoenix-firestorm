# Native Dullahan target naming

Source review: `762aedb018`, Windows CMake integration, 2026-09-21.
Rules: NV-00 and NV-01; build naming only.

1. OpenGL uses the prebuilt `dullahan.lib` through `ll::cef` in
   `indra/cmake/CEFPlugin.cmake`. Its browser implementation is unchanged.
2. Native Vulkan builds adapted Dullahan sources in
   `indra/llvulkan/native_dullahan.cmake`, consumed by `llvkbrowser` in
   `indra/llvulkan/CMakeLists.txt`. Rename this static library target from
   `llvk_dullahan` to `vkdullahan` to distinguish it from the upstream dullahan library.
3. Rename the target and its consumer directly. Preserve the FetchContent name,
   generated source filenames, dependencies, compile options and runtime code.
   No callbacks, ownership, GPU publication or retirement behavior changes.

Focused verification: inspect all target references and run `git diff --check`.
The next CMake generation should produce `vkdullahan.lib` on Windows.
The static target would use `libvkdullahan.a` on Linux under standard CMake
naming. Its inclusion remains Windows-only; this rename does not enable Linux
browser integration.
This naming change does not establish build success or runtime visual parity.

The same source review covers renaming the native browser integration target
from `llvkbrowser` to `vk_media_plugin_cef`, following the OpenGL integration's
`media_plugin_cef` naming. Update the target declaration, properties, window
manager dependency and integration test link dependency. Keep the native library's
existing type and ownership; this rename does not turn it into the OpenGL shared
media plugin or change process boundaries. Source filenames and the test name
remain `llvkbrowser`; the test macro uses that name to locate its source file.
Verification must distinguish those source/test identifiers from library links.
