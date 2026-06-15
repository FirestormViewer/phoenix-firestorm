@echo off
setlocal

for /f "usebackq delims=" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath`) do set "VS_PATH=%%i"
call "%VS_PATH%\Common7\Tools\VsDevCmd.bat" -arch=arm64 -host_arch=arm64

set "ROOT=C:\Work\Research\Firestorm\arm64-libs"
set "PKG=C:\Work\Research\Firestorm\phoenix-firestorm\build-vc170-64\packages\lib\release"

echo === Configuring minizip-ng for ARM64 ===
cmake -G "Visual Studio 17 2022" -A ARM64 ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DMZ_COMPAT=ON ^
    -DMZ_BZIP2=OFF ^
    -DMZ_LZMA=OFF ^
    -DMZ_ZSTD=OFF ^
    -DMZ_OPENSSL=OFF ^
    -DMZ_FETCH_LIBS=OFF ^
    -DMZ_ZLIB=ON ^
    -DZLIB_LIBRARY="%ROOT%\zlib-ng\build\Release\zlibstatic.lib" ^
    -DZLIB_INCLUDE_DIR="%ROOT%\zlib-ng\build" ^
    -S "%ROOT%\minizip-ng" ^
    -B "%ROOT%\minizip-ng\build_arm64"
if errorlevel 1 (
    echo ERROR: Configure failed
    exit /b 1
)

echo === Building minizip-ng ===
cmake --build "%ROOT%\minizip-ng\build_arm64" --config Release --parallel
if errorlevel 1 (
    echo ERROR: Build failed
    exit /b 1
)

echo === Copying library ===
copy /Y "%ROOT%\minizip-ng\build_arm64\Release\minizip.lib" "%PKG%\minizip.lib"
if not exist "%PKG%\minizip.lib" (
    echo Trying alternate output name...
    for /R "%ROOT%\minizip-ng\build_arm64" %%f in (*.lib) do (
        echo Found: %%f
    )
)
echo === minizip-ng build complete ===
