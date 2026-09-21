# Vulkan SDK package metadata

The previous common archive (`vulkan_sdk-1.4.350-common-73f1373.tar.zst`)
omitted `autobuild-package.xml`. Autobuild 3.9.3 therefore marked the dependency
dirty on both Windows and Linux. This was a provenance/packaging defect, not a
Linux compiler or Vulkan runtime problem.

The replacement is the `v1.4.350-metadata1` release of
`anne-skydancer/3p-vulkan-sdk`, built from `47952d7e3673b81020ce65ec36ad325d65b6639e`
on Ubuntu 24.04 in Actions run `35610560576`. Its archive is
`vulkan_sdk-1.4.350-common-3.tar.zst` with SHA-256:

`26fe911a2d6adb80ce933db873263339df529386c97f460a8a943140ca218f54`

It contains Autobuild-generated metadata, a version file, a complete 57-file
payload manifest, licenses and the unchanged upstream component versions:
Vulkan-Headers `vulkan-sdk-1.4.350.1`, volk `1.4.350`, and VMA `v3.4.0`.
It remains a `common` headers/source package. The viewer compiles volk itself;
there is no separate precompiled Linux SDK library to supply or relabel.

The producer now has a valid common-platform Autobuild configuration and runs
`autobuild build` followed by `autobuild package --clean-only`, rather than
publishing a hand-created archive. Its validator checks metadata, payload
coverage and every packaged file against the assembled tree. A separate install
probe exercises normal Autobuild installation and verifies a clean dependency.

Validation: the published archive was built and validated on Linux. A fresh
Windows install using this viewer manifest successfully downloaded and verified
the release and reported no dirty packages. No full viewer rebuild or runtime
rendering test was needed or performed for this manifest-only change.

NV-00/NV-01: the consumer-visible contract is the same include/source/license
layout and component versions for both backends/platforms; the change restores
package provenance and checksum verification. No UI/rendering behavior, GPU
ownership, synchronization or retirement changes. Existing release assets remain
intact; the viewer selects a new immutable release URL and SHA-256 together.
