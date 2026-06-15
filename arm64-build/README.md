# ARM64 Windows third-party build & staging

This directory preserves the out-of-tree work needed to build the **third-party
libraries** for the native ARM64 Windows viewer (see
`doc/building_windows_arm64.md` for the viewer itself).

These libraries are built from source for ARM64 (there are no upstream ARM64
Windows autobuild packages). The viewer's CMake still downloads the **x64**
prebuilt packages for their (architecture-independent) **headers**; the ARM64
`.libs` are then overlaid on top of the x64 ones in the package tree.

> Paths in these scripts are hard-coded for the original build machine
> (`C:\Work\Research\Firestorm\...`). Adjust `ROOT` / `PKG` / source paths for
> your layout. They assume the 3p source trees are checked out under
> `arm64-libs\<lib>\` and the viewer under `phoenix-firestorm\`.

## `scripts/` — build & staging scripts

Build (produce the ARM64 `.lib` outputs in each lib's `build_arm64`/`build` dir):
- `boost_bootstrap.bat`, `boost_build.bat` — Boost 1.86.0, built with
  `context-impl=winfib` (Windows Fiber API, not fcontext asm — required for the
  ARM64 Pointer-Authentication fixes; see boost-patches below).
- `rebuild_boost_fs.bat` — rebuild **boost_filesystem** with `/Zc:wchar_t-`
  (the viewer treats `wchar_t` as `unsigned short`; the default boost build is
  native-`wchar_t` and won't link — unresolved `path_traits::convert` etc.).
- `rebuild_boost_fiber.bat` — rebuild **boost_fiber** after editing its sources
  (the thread-exit teardown leak; see boost-patches).
- `build_all_arm64.bat` — meshoptimizer, libjpeg-turbo (`-DWITH_JPEG8=ON`),
  openjpeg, libpng, ogg/vorbis, freetype, **OpenAL Soft**.
- `build_openssl.bat`, `build_curl.bat`, `build_libxml2.bat`,
  `build_minizip.bat`, `build_openxr.bat`, `build_colladadom.bat`,
  `build_hunspell.bat`, `build_physics_stubs.bat`, `build_context_asm.bat`,
  `find_popcnt.bat`, `build_slplugin.bat` — the remaining libs / helpers.

Stage (after the viewer is CMake-configured so autobuild has installed the x64
packages):
- **`stage_arm64_packages.bat`** — the authoritative overlay. Copies every ARM64
  `.lib` over the x64 ones with the correct package names, stages the OpenAL
  runtime + AL headers (OpenAL's `use_prebuilt_binary` is guarded off for ARM64),
  overlays the m137 `webrtc.lib`, and rebuilds hunspell + colladadom (which emit
  straight into the package dir). **Use this instead of hand-copying** — the
  manual overlay repeatedly missed pieces (OpenAL DLL/headers, the `/Zc:wchar_t-`
  boost_filesystem, the patched boost_fiber).
- **`refresh_runtime_data.bat`** — copies `app_settings` + `skins` from the
  viewer source into `build-vc170-64\newview\Release\`. Normal dev builds only
  stage these during the *packaging* manifest action, so after a source update
  the deployed data goes stale and the viewer crashes at startup ("couldn't
  access some of the files"). Run this when launching `firestorm-bin.exe`
  directly from `Release\` (or run the viewer with working dir = `indra\newview`).

## `boost-patches/` — patched Boost 1.86.0 sources

Copies of the three modified Boost source files (full files, mirroring their
paths under `boost_1_86_0/`). Copy each over the corresponding file in a fresh
Boost 1.86.0 checkout before building, then build with `context-impl=winfib`.

- `libs/context/src/fiber.cpp`, `libs/context/src/continuation.cpp` — ARM64
  Pointer Authentication fix: replace the `thread_local static` activation-record
  initializer with a heap-allocated one via a plain `thread_local` pointer +
  cleanup struct. MSVC's `thread_local static` uses `InitOnceExecuteOnce` whose
  per-access guard `autibsp` faults (`FAST_FAIL_POINTER_AUTH_INVALID_RETURN_ADDRESS`)
  during thread exit on ARM64.
- `libs/fiber/src/context.cpp` — the same PAC fix for `context::active()`, **plus**
  the winfib thread-exit teardown fix: under `#if defined(BOOST_USE_WINFIB)`,
  `deinitialize()` *leaks* the per-thread main context + scheduler instead of
  deleting them, because deleting runs `scheduler::~scheduler -> suspend() ->
  SwitchToFiber` from a `thread_local` destructor during thread exit, after the
  OS has torn down the thread's fiber state (null-deref crash — hit by the
  texture-cache purge thread on first launch).

## Build order (from clean)

1. Build all 3p libs (boost_build → rebuild_boost_fs → rebuild_boost_fiber →
   build_all_arm64 → the rest).
2. Configure the viewer (`build_arm64.bat --configure-only` at the repo root) so
   autobuild installs the x64 packages.
3. `stage_arm64_packages.bat` — overlay the ARM64 libs.
4. `build_arm64.bat --build-only` — build the viewer.
5. `refresh_runtime_data.bat` — so a direct `Release\` launch has current data.

## webrtc (m137)

webrtc is built separately via the `secondlife/3p-webrtc-build` tree
(`build/run.py`, shiguredo-style). The ARM64 customizations there (a
`windows_arm64` target, the `windows_arm64_*` patches, and
`build_arm64.bat` / `run_compile_m137.bat` / `run_package_m137.bat` launchers)
are committed in that repo. Target: webrtc `137.7151.03`
(commit `8538ce3b1ac15f0a1beeb5ef7c9368b73d2c0c92`). Stage its output with
`stage_arm64_packages.bat` (the `webrtc.lib` copy).
