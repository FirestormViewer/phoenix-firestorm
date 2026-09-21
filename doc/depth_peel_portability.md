# Depth-peeling capability portability

Source review: `d02261fab4`, 2026-09-21. The availability setting in
`LLPipeline::allocateScreenBufferInternal` and the allocation gate in
`allocateAlphaDepthPeelBuffers` must use the same capability predicate.

`llglheaders.h` declares `glBlendEquationSeparate` as a dynamically loaded
pointer under `LL_WINDOWS` when `LL_MESA` is false. Linux and Mesa headers
declare a linked function, whose address cannot be null. Keep the existing
OpenGL 4.1 minimum on every platform and retain the pointer check only for
the Windows loader declaration. Use one helper for both consumers.

No blend operations, rendering output, allocation failure handling or GPU
resource lifetimes change. Verification should compile the helper against both
declaration forms and check low-version rejection plus Windows null/non-null
loader results. Full Linux viewer compilation and runtime visuals remain CI
and interactive validation respectively.

Validation completed: the helper extracted from pipeline.cpp compiled with LLVM-MinGW Clang using -Wall -Wextra -Werror for Linux-style, Windows-pointer and Mesa declarations. All three executables passed version and applicable pointer-state assertions. This is a bounded host-side check, not a full GCC/Linux viewer build.
