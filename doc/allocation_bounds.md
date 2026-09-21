# Allocation bounds exposed by Linux LTO

Source review: `51ef28b317`, Windows/Linux x64, 2026-09-21.

Terrain: login-provided dimensions reach parcel initialization, region composition
and surface arrays. Validate nonzero dimensions, patch divisibility, finite
positive width and representable grid/patch counts before those allocations.
The region constructor validates before allocating its implementation, and direct
surface creation validates before changing its members. Counts are multiplied
only after division-based overflow checks. Array sizes must fit signed viewer
indices and the platform's maximum object size. Existing non-power-of-two regions
whose edges divide into patches (for example 768/16) remain supported. Texture
rounding is clamped before shifting when its result would exceed the existing
1024 limit.

String tables: retain zero as the default 256 and preserve nearest-power-of-two
rounding, including upward midpoint ties. Use unsigned arithmetic and reject
negative requests or results outside signed indexing range before allocation.

Coroutine queues: retain all current 2048/524288 capacities and bound the public
capacity contract to powers of two from 2 through DEFAULT_QUEUE_SIZE (524288).
Validate immediately at the Boost allocation boundary. Boost sources are unchanged.

Invalid sizes throw std::invalid_argument or std::length_error. This is fail-fast
validation, not a new graceful login-disconnect flow. Valid sizes are unchanged;
allocation failure due to insufficient physical memory remains possible. These
checks do not impose a new memory budget or change rendering algorithms, queue
scheduling, GPU ownership or resource retirement (NV-00/NV-01).

Validation: standalone production-helper tests cover signed limits, square
overflow, negative/zero requests, rounding boundaries and queue limits; compare
all table sizes 1..65536 with the old rounding rule. Linux CI runs the tests with
GCC 14, LTO and warnings as errors. Host Clang LTO tests passed. A bounded probe
of the actual surface-validation body passed valid 256/768/4096 regions and
invalid zero, indivisible, oversized, negative-width, infinite and NaN inputs.
That probe substitutes geometry element types and is not a live terrain test.
Full Linux viewer LTO warning elimination remains to be confirmed by CI.

The actual llsurface.cpp, llviewerregion.cpp, llstartup.cpp, llworld.cpp, llcoproceduremanager.cpp and llxmltree.cpp also compiled in an isolated MSVC RelWithDebInfo object build against the existing package set. This was not a full viewer relink; existing dependency macro-redefinition warnings remained.
