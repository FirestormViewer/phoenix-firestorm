@echo off
setlocal

for /f "usebackq delims=" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath`) do set "VS_PATH=%%i"
call "%VS_PATH%\Common7\Tools\VsDevCmd.bat" -arch=arm64 -host_arch=arm64

set "ROOT=C:\Work\Research\Firestorm\arm64-libs"
set "PKG=C:\Work\Research\Firestorm\phoenix-firestorm\build-vc170-64\packages\lib\release"

echo === Configuring OpenXR for ARM64 ===
cmake -G "Visual Studio 17 2022" -A ARM64 ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DBUILD_SHARED_LIBS=OFF ^
    -DDYNAMIC_LOADER=OFF ^
    -DBUILD_TESTS=OFF ^
    -DBUILD_API_LAYERS=OFF ^
    -S "%ROOT%\openxr" ^
    -B "%ROOT%\openxr\build_arm64"
if errorlevel 1 (
    echo ERROR: Configure failed
    exit /b 1
)

echo === Building OpenXR ===
cmake --build "%ROOT%\openxr\build_arm64" --config Release --target openxr_loader --parallel
if errorlevel 1 (
    echo ERROR: Build failed
    exit /b 1
)

echo === Copying library ===
copy /Y "%ROOT%\openxr\build_arm64\src\loader\Release\openxr_loader.lib" "%PKG%\openxr_loader.lib"
if not exist "%PKG%\openxr_loader.lib" (
    echo Trying to find output...
    for /R "%ROOT%\openxr\build_arm64" %%f in (*openxr*.lib) do echo Found: %%f
)
echo === OpenXR build complete ===
