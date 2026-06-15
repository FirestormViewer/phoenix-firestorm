@echo off
setlocal

for /f "usebackq delims=" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath`) do set "VS_PATH=%%i"
call "%VS_PATH%\Common7\Tools\VsDevCmd.bat" -arch=arm64 -host_arch=arm64

set "ROOT=C:\Work\Research\Firestorm\arm64-libs"
set "PKG=C:\Work\Research\Firestorm\phoenix-firestorm\build-vc170-64\packages\lib\release"

echo === Configuring libxml2 for ARM64 ===
cmake -G "Visual Studio 17 2022" -A ARM64 ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DBUILD_SHARED_LIBS=OFF ^
    -DLIBXML2_WITH_ICONV=OFF ^
    -DLIBXML2_WITH_LZMA=OFF ^
    -DLIBXML2_WITH_PYTHON=OFF ^
    -DLIBXML2_WITH_TESTS=OFF ^
    -DLIBXML2_WITH_PROGRAMS=OFF ^
    -DLIBXML2_WITH_ZLIB=ON ^
    -DZLIB_LIBRARY="%ROOT%\zlib-ng\build\Release\zlibstatic.lib" ^
    -DZLIB_INCLUDE_DIR="%ROOT%\zlib-ng\build" ^
    -S "%ROOT%\libxml2" ^
    -B "%ROOT%\libxml2\build_arm64"
if errorlevel 1 (
    echo ERROR: Configure failed
    exit /b 1
)

echo === Building libxml2 ===
cmake --build "%ROOT%\libxml2\build_arm64" --config Release --parallel
if errorlevel 1 (
    echo ERROR: Build failed
    exit /b 1
)

echo === Copying library ===
copy /Y "%ROOT%\libxml2\build_arm64\Release\libxml2s.lib" "%PKG%\libxml2.lib"
if not exist "%PKG%\libxml2.lib" (
    echo Trying alternate output name...
    for /R "%ROOT%\libxml2\build_arm64" %%f in (*.lib) do (
        echo Found: %%f
    )
)
echo === libxml2 build complete ===
