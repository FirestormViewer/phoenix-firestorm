# SoLoud audio backend

SoLoud is the default viewer sound-effects, UI-sound and wind backend.
FMOD Studio (`--fmodstudio`) and OpenAL (`--openal`) remain optional build
selections. `--soloud` restores the default. These configure-script options
explicitly reset all three backend flags and the deprecated FMOD/OpenAL aliases,
including on reconfiguration. The last named backend option wins.

For direct CMake configuration, the default is SoLoud unless an optional backend
is enabled. When switching an existing cache, explicitly disable the previous
backend. A SoLoud/optional-backend conflict fails configuration, rather than
silently choosing a backend. `INSTALL_PROPRIETARY` does not imply FMOD.

## Dependency

The viewer uses an autobuild package, not vendored SoLoud source or `SOLOUD_ROOT`.
The standalone repository is https://github.com/anne-skydancer/3p-soloud.
Its revision 2 is based on SoLoud source
`e82fd32c1f62183922f08c14c814a02b58db1873`, with bounded 1024-frame miniaudio
mixing and device-selection/native-format-negotiation patches.

- Repository revision: `ee0970a9c85e870fb62865972e2297bf602a7766`.
- Package version: `202002.e82fd32c1f.vulkanstorm2`, build ID `2`.
- Release: `v202002-e82fd32c1f-2`.
- SHA256: `605017ece00f03e7f45f6cb13888559e053c58d515a1f8990284dc3617b532e7`.

Only a Windows x64 package is currently published. Other platforms and Windows
32-bit fail explicitly when SoLoud is selected; select an optional backend until
matching packages exist. Their builds are not validated by the Windows tests.
SoLoud is linked statically. The Windows manifest stages package-owned notices
as `licenses/soloud.txt` and `licenses/soloud-miniaudio.txt`.

## Behavior

SoLoud does not replace the VLC media plugin. Parcel/music streaming and its
metadata continue through `LLStreamingAudio_MediaPlugins`; that plugin and voice
have separate device-routing behavior from the sound-effects output selector.

Device UUIDs use endpoint identities, not display names. WASAPI endpoint IDs
survive duplicate names and renaming; legacy WinMM numeric IDs are only stable
while Windows retains their enumeration. Device lists are refreshed periodically.
Saved device IDs from another audio backend may require selecting the endpoint
again; the migration does not rewrite user settings.
Device changes are serialized with voice device access. A mutex timeout is
retried; an unavailable requested endpoint is reported without changing output.
If reopening fails, the previous device is explicitly restored or the engine
remains unavailable. No other audio engine is selected as a fallback.
Restart stops callbacks, invalidates channel handles, detaches retained WAV source
IDs and destroys the old mixer before creating a replacement. This avoids the
pinned library's repeated-init mixer-table leak without changing the package.
Independent media-stream updates continue even when effects output is unavailable.

Wind target values cross to the callback atomically; scratch storage is bounded
and preallocated. Native output rates and 1/2/4/6/8-channel layouts are negotiated
by the package. The adapter maps 7.1 spatial back/side positions to Windows order.
Listener/source velocities, Doppler settings and changing rolloff are forwarded.
The archived short PCM endpoint ramps and stop fades are retained; audible
quality, especially moving-source Doppler, still requires physical acceptance.

## Validation

On Windows/Cygwin, run `bash scripts/validate_soloud.sh` from this worktree.
The runner sources the canonical RelWithDebInfo build variables below
`LOCALAPPDATA/VulkanStorm/fs-build-variables` (override with
`FS_BUILD_VARIABLES_DIR`), configures the default without `--openal`, builds/runs
`llsoloud_adapter_test`, builds `llvk_ui_shaders`, then `vulkanstorm-bin`.
It never launches the viewer or builds an installer.

The configure-only matrix exercises the real backend CMake modules with package
installation stubbed: defaults, proprietary mode, explicit/legacy optional flags,
cache transitions, conflicting flags and unsupported platform/architecture errors.
This is selection evidence, not an FMOD/OpenAL link or runtime test.
Autobuild presets no longer inject FMOD/OpenAL selections; direct-CMake presets
reset to SoLoud. Override both `USE_SOLOUD=OFF` and the optional backend flag when
using a direct-CMake preset to select another engine.
The headless adapter test opens only SoLoud's null backend at 2/6/8 channels.
Prior dependency callback bounds tests, including 32769 frames, remain applicable:
the published package and callback were not modified by this migration.

Physical acceptance remains required for output selection, duplicate device names,
unplug/replug and voice contention, stereo/surround directionality, motion/Doppler,
mute/category gains, wind, queued/synchronized looping, and VLC stream metadata.
No physical audio device or viewer is launched by these checks.