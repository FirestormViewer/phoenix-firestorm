# CI shader compiler provisioning

Source baseline: eab0661834; observed PR #51 run 35536495415.
The macOS job fails at llvulkan/CMakeLists.txt's required glslangValidator
lookup. The matrix then cancels Windows through default fail-fast behavior.

NV-00 reference: native shader custom commands compile GLSL to SPIR-V targeting
Vulkan 1.1 before C++ compilation. The headers-only Vulkan prebuilt does not
provide the compiler. This build-time dependency is separate from the OpenGL
visual implementation and needs no GPU or rendering context.

Native design: provision the same pinned Khronos glslang tool on both runners,
cache its installation by host OS/architecture/revision, and verify its version
before viewer configuration. Build only the necessary GLSL/SPIR-V toolchain;
disable optional HLSL, optimizer integration and tool self-tests. Keep existing
shader compilation commands and targets unchanged. Disable matrix fail-fast so
one platform's failure cannot hide the other platform's result.

NV-01/NV-03: no rendering, runtime packaging or backend changes. No GL code
changes. Discriminating validation is a fresh CI run reaching viewer configuration
with glslangValidator found; later build failures must remain visible as failures.
