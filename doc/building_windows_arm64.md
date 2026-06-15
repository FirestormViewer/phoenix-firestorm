# Building Firestorm for native ARM64 Windows

This document describes the changes that make Firestorm build and run as a
**native ARM64** binary on Windows (e.g. Qualcomm Snapdragon X / Adreno
hardware), and how to build it. This is a developer port; it is **not** part of
the standard autobuild-driven x64 build.

## Status

The native ARM64 build compiles, links, logs in, renders the world, and plays
sound effects. Verified on a Snapdragon X Elite (X1E80100) with the Adreno
X1-85 GPU. OpenGL is provided through Mesa's D3D12 layer (OpenGL 4.6 over
Direct3D 12) — there is no native desktop-GL driver on this hardware.

## High-level approach

Windows on ARM cannot run x86/x64 intrinsics natively, and several third-party
binaries Firestorm normally downloads via autobuild have no ARM64 build. The
port therefore:

1. Builds the third-party libraries from source for ARM64.
2. Replaces x86 SSE/CPUID/RDTSC code paths with ARM64 (NEON / Win32 API)
   equivalents, guarded by `#if defined(_M_ARM64)`.
3. Works around ARM64 Pointer Authentication (PAC) interactions in Boost.Context.
4. Disables features whose dependencies have no ARM64 build.
5. Adds an OpenAL Soft audio backend (the default FMOD Studio path is x64-only here).

Everything is gated on `_M_ARM64` (in C++) or
`CMAKE_GENERATOR_PLATFORM STREQUAL "ARM64"` (in CMake) so the x64 build is
unaffected.

## Prerequisites

- Visual Studio 2022 with the **ARM64** build tools (MSVC + ARM64 cross/native
  compilers) and a recent Windows 11 SDK.
- The Firestorm Python venv with `autobuild` (used only for the about-box
  package list during the manifest step; the ARM64 build otherwise drives CMake
  and MSBuild directly).
- The ARM64 third-party libraries staged into
  `build-vc170-64/packages/` (see below).

## Third-party libraries (built from source for ARM64)

These are built natively for ARM64 and staged into the prebuilt `packages`
tree, replacing the x64 autobuild packages. A local helper script
(`arm64-libs/build_all_arm64.bat`, outside this repo) builds them:

- zlib-ng, libexpat, APR / APR-util (+ LL custom stubs)
- Boost 1.86.0 — built with `context-impl=winfib` (see PAC notes)
- meshoptimizer, libjpeg-turbo, openjpeg, libpng, ogg / vorbis, freetype
- OpenSSL, nghttp2, libcurl, libxml2, minizip-ng, hunspell
- colladadom, OpenXR, OpenAL Soft (audio)
- ARM64 stubs for Havok physics / pathing / HACD

Two library notes worth calling out:

- **libjpeg-turbo must be built with `-DWITH_JPEG8=ON`.** The Firestorm package
  header declares `JPEG_LIB_VERSION 80`; libjpeg-turbo otherwise defaults to the
  v62 ABI, and the struct-layout mismatch makes the runtime abort with
  *"Wrong JPEG library version: library is 62, caller expects 80"* on the first
  JPEG decode.
- **Boost.Context is built with `context-impl=winfib`** (Windows Fiber API)
  instead of the hand-written `fcontext` assembly — see below.

## Source changes

### CPU / SIMD / intrinsics (no x86 SSE, CPUID or RDTSC on ARM64)

- `indra/llcommon/llprocessor.cpp` — ARM64 CPU-info implementation using
  `GetNativeSystemInfo` + the registry (`~MHz`, `ProcessorNameString`) instead
  of `__cpuid` / `__rdtsc`.
- `indra/llcommon/llmemory.h` — `ll_memcpy_nonaliased_aligned_16` uses NEON
  (`vld1q_f32` / `vst1q_f32`) and `__prefetch` instead of `_mm_*` SSE intrinsics.
- `indra/llcommon/llfasttimer.{h,cpp}` — disable the RDTSC timer
  (`LL_FASTTIMER_USE_RDTSC 0`) on ARM64; use the QueryPerformanceCounter path.
- `indra/llmath/llvector4a.h` — skip the x86 MXCSR setup
  (`_MM_SET_FLUSH_ZERO_MODE` / `_MM_SET_ROUNDING_MODE`) on ARM64.
- `indra/newview/llface.cpp` — `getGeometryVolume` index copy uses a scalar
  loop on ARM64; the x86 `_mm_storeu_si128` fast path routes through the
  soft-intrinsics shim and writes past the destination there.
- `indra/newview/llappviewer.cpp` — skip the "CPU lacks SSE2 → quit" startup
  gate on ARM64 (SSE2 does not exist; soft intrinsics are used instead).

The 00-Common build settings also enable Microsoft's x86-intrinsics shim
(`USE_SOFT_INTRINSICS`, `softintrin`, `cmake/arm64_stubs`) and force GLM's
software intrinsics so the large body of remaining `_mm_*` code still compiles.

### Pointer Authentication (PAC) / Boost.Context

ARM64 PAC checks crash the hand-written Boost.Context `fcontext` trampolines and
MSVC's `thread_local static` initialization (`InitOnceExecuteOnce`) during
thread exit. Fixes:

- `indra/cmake/00-Common.cmake` — compile with `/guard:signret`, and define
  `BOOST_USE_WINFIB` so the viewer includes `fiber_winfib.hpp` (Windows Fiber
  API) rather than `fiber_fcontext.hpp`. Without the define the link fails with
  unresolved `jump_fcontext` / `make_fcontext` / `ontop_fcontext`.
- Boost itself is built with `context-impl=winfib`, and its
  `libs/context/src/{fiber,continuation}.cpp` and `libs/fiber/src/context.cpp`
  are patched to replace `thread_local static` activation-record initializers
  with heap-allocated ones (PAC-safe). (These patches live with the Boost build,
  not in this repo.)

### Audio — OpenAL Soft backend

The default FMOD Studio backend has no ARM64 build here, so audio uses
**OpenAL Soft** (open source). The legacy **ALUT (freealut) dependency was
removed entirely** — it is unmaintained and only provided device setup and a WAV
loader, both of which are now done directly:

- `indra/llaudio/llaudioengine_openal.cpp` — `init()`/`shutdown()` use raw ALC
  (`alcOpenDevice` / `alcCreateContext` / `alcMakeContextCurrent` + teardown);
  `LLAudioBufferOpenAL::loadWAV` now parses the RIFF/WAVE PCM file itself and
  calls `alBufferData` (the decode manager writes canonical 16-bit WAV).
- `indra/llaudio/llaudioengine_openal.h` — owns the `ALCdevice*` / `ALCcontext*`.
- `indra/llaudio/lllistener_openal.h` — drop the `AL/alut.h` include.
- `indra/cmake/OPENAL.cmake` — link only `OpenAL32` (no `alut`); on ARM64 use the
  locally built OpenAL Soft instead of the x64 prebuilt.
- `indra/cmake/Copy3rdPartyLibs.cmake`, `indra/newview/CMakeLists.txt`,
  `indra/newview/viewer_manifest.py` — stage/bundle `OpenAL32.dll` only (no
  `alut.dll`).

Enable with `-DUSE_OPENAL=ON` at configure time (see `build_arm64.bat`).

**Limitation:** only sound *effects* work. Streaming parcel/media audio falls
back to the media-plugin path, and the media plugins (CEF/VLC) have no ARM64
build.

### Runtime crash fix — null audio engine

- `indra/newview/llviewermessage.cpp` — `process_attached_sound` now guards
  `gAudiop` like its sibling handlers. With audio disabled (or before init)
  `gAudiop` is null and the unguarded `isCorruptSound` call dereferenced null.

### Features disabled on ARM64 (no upstream ARM64 build)

CMake modules updated to no-op / skip on ARM64: CEF browser
(`CEFPlugin.cmake`), LibVLC (`LibVLCPlugin.cmake`), NVAPI (`NVAPI.cmake` +
`llappviewerwin32.cpp` guards), LibNDOF (`NDOF.cmake`), GLOD (`GLOD.cmake`),
Growl (`Growl.cmake`), Discord (`Discord.cmake`); plus `media_plugins`,
`newview`, `ViewerManager`, `ViewerMiscLibs` adjustments. FMOD Studio and KDU
are off; OpenSim is on; Havok uses stubs.

### Misc

- `indra/llcommon/StackWalker.cpp` — ARM64 crash stack-walk
  (`IMAGE_FILE_MACHINE_ARM64`, `Pc`/`Fp`/`Sp`).
- `indra/cmake/Boost.cmake` — ARM64 library suffix (`-a64`).
- `indra/cmake/{NGHTTP2,OpenJPEG,Variables}.cmake`, `indra/llwebrtc/CMakeLists.txt`,
  `scripts/configure_firestorm.sh`, `indra/newview/fs_viewer_manifest.py` —
  ARM64 wiring (static-lib defines, package layout, aarch64 arch string).

## Building

`build_arm64.bat` (repo root) drives CMake + MSBuild directly. **Edit the
absolute paths near the top** (`FIRESTORM_ROOT`, etc.) for your machine, then:

```
build_arm64.bat                 :: configure + build (Release)
build_arm64.bat --configure-only
build_arm64.bat --build-only
build_arm64.bat --config RelWithDebInfo
```

Key configure flags: `-A ARM64`, `-DUSE_FMODSTUDIO=OFF`, `-DUSE_KDU=OFF`,
`-DUSE_OPENAL=ON`, `-DHAVOK=OFF`. The third-party ARM64 libraries must be staged
into `build-vc170-64/packages/` first.

> The MSVC linker may print *"the 32-bit linker … failed to do memory mapped
> file I/O … restarting link with a 64-bit linker"* during the final link. This
> is expected for a link this large and is not an error.
