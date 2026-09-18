#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
variables_dir="${FS_BUILD_VARIABLES_DIR:-$(cygpath -u "$LOCALAPPDATA")/VulkanStorm/fs-build-variables}"
source "$variables_dir/convenience" RelWithDebInfo
export LL_BUILD AUTOBUILD_VSVER=170 AUTOBUILD_BUILD_ID="$(git rev-list --count HEAD)"
export AUTOBUILD="$(cygpath -d "$(command -v autobuild)")"
runner="$(cygpath -u "$AUTOBUILD")"
mkdir -p build-vc170-64/logs
cmake "-DTEST_BINARY_DIR=$(cygpath -m "$PWD/build-vc170-64/audio-selection-tests")" -P indra/llaudio/tests/audio_backend_selection.cmake
"$runner" configure -A 64 -c RelWithDebInfoFS_open -- --no-opensim --zink --avx2 --no-package -DUSE_DISCORD:BOOL=OFF -DOPENSIM:BOOL=FALSE -DLL_TESTS:BOOL=FALSE
cmake --build build-vc170-64 --config RelWithDebInfo --target llsoloud_adapter_test -- /m:2 /p:CL_MPCount=2 /verbosity:minimal /nologo
./build-vc170-64/llaudio/RelWithDebInfo/llsoloud_adapter_test.exe
cmake --build build-vc170-64 --config RelWithDebInfo --target llvk_ui_shaders -- /verbosity:minimal /nologo
cmake --build build-vc170-64 --config RelWithDebInfo --target vulkanstorm-bin -- /m:2 /p:CL_MPCount=2 /verbosity:minimal /nologo
echo 'PASS: SoLoud RelWithDebInfo viewer and headless adapter validation'