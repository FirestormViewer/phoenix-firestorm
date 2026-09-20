# CI Release configuration parity

Reference: successful Windows Release build of master 2a55929ab7 on 2026-09-20,
version 7.2.5.79359. The local CMake cache is the configuration oracle.

Both CI entry points must use one Windows/Linux build definition: Release,
64 bit, Second Life only, AVX2, LTO, precompiled headers, SoLoud, OpenJPEG,
packaging enabled; KDU, proprietary packages, OpenAL, FMOD, Discord, Bugsplat,
Tracy and LL_TESTS disabled. Windows bundles Mesa/Zink and uses Inno Setup;
Linux uses its native archive packager and does not bundle Windows WGL DLLs.
The channel is Vulkanstorm-Release. Build numbers identify the CI run, so
installer filenames need not equal the local filename.

NV-00 reference/design: the existing CMake and viewer manifest consume build
options, select libraries, compile native shaders and stage runtime assets.
These are CPU build operations shared by both renderers. Configure the same
options explicitly and assert the resulting cache before compiling; provision
glslang independently of the header/source Vulkan package. No rendering code,
GL oracle, GPU resource lifetime or backend selection is changed (NV-01/NV-03).

Linux SoLoud requires a platform-specific static archive, Threads and dl, plus
the same notices as Windows. The separate package build must pass its bounded
callback tests on both hosts before its archives are pinned in autobuild.xml.
This does not establish Linux hardware playback or viewer visual parity.

Validation: parse workflow YAML and the LLSD manifest, run backend-selection
checks, verify package hashes and assert the configured options against this
contract. Hosted Windows/Linux builds remain required evidence for CI success.

The daily/manual legacy workflow calls the same reusable build. Inherited
private signing/upload services, macOS jobs and alternate grid/CPU matrices are
not part of this fork's default build. Tag-triggered releases still publish
Windows/Linux installer artifacts; installers are unsigned, as on the local PC.

Dependency changes retained from the separately requested portability/decoder
work: SoLoud revision 3 adds the Linux archive without changing upstream source;
OpenJPEG is updated to the published 2.5.4-r1 packages. These are dependency
updates, not a claim that CI's dependency bytes equal the older local build.
