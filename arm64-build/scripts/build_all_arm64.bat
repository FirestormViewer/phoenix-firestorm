@echo off
setlocal EnableDelayedExpansion

REM ==========================================================================
REM  Build all ARM64 third-party libraries for Firestorm Viewer
REM ==========================================================================

REM --- Find Visual Studio 2022 ---
for /f "usebackq delims=" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath`) do set "VS_PATH=%%i"
call "%VS_PATH%\Common7\Tools\VsDevCmd.bat" -arch=arm64 -host_arch=arm64

set "ROOT=C:\Work\Research\Firestorm\arm64-libs"
set "PKG=C:\Work\Research\Firestorm\phoenix-firestorm\build-vc170-64\packages\lib\release"
set "CMAKE_COMMON=-G "Visual Studio 17 2022" -A ARM64 -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF"

REM ==========================================================================
REM  1. meshoptimizer
REM ==========================================================================
echo.
echo === Building meshoptimizer ===
if not exist "%ROOT%\meshoptimizer\build_arm64" mkdir "%ROOT%\meshoptimizer\build_arm64"
cmake %CMAKE_COMMON% -S "%ROOT%\meshoptimizer" -B "%ROOT%\meshoptimizer\build_arm64"
cmake --build "%ROOT%\meshoptimizer\build_arm64" --config Release --parallel
copy /Y "%ROOT%\meshoptimizer\build_arm64\Release\meshoptimizer.lib" "%PKG%\meshoptimizer.lib"
if errorlevel 1 echo FAILED: meshoptimizer

REM ==========================================================================
REM  2. libjpeg-turbo
REM ==========================================================================
echo.
echo === Building libjpeg-turbo ===
if not exist "%ROOT%\libjpeg-turbo\build_arm64" mkdir "%ROOT%\libjpeg-turbo\build_arm64"
REM WITH_JPEG8=ON: match the Firestorm prebuilt package's JPEG_LIB_VERSION 80 ABI.
REM Without it libjpeg-turbo defaults to v62, mismatching the package header (v80)
REM and aborting at runtime with "Wrong JPEG library version: library is 62, caller expects 80".
cmake %CMAKE_COMMON% -DWITH_TURBOJPEG=OFF -DENABLE_SHARED=OFF -DWITH_JPEG8=ON -S "%ROOT%\libjpeg-turbo" -B "%ROOT%\libjpeg-turbo\build_arm64"
cmake --build "%ROOT%\libjpeg-turbo\build_arm64" --config Release --parallel
copy /Y "%ROOT%\libjpeg-turbo\build_arm64\Release\jpeg-static.lib" "%PKG%\jpeg.lib"
if errorlevel 1 echo FAILED: libjpeg-turbo

REM ==========================================================================
REM  3. openjpeg
REM ==========================================================================
echo.
echo === Building openjpeg ===
if not exist "%ROOT%\openjpeg\build_arm64" mkdir "%ROOT%\openjpeg\build_arm64"
cmake %CMAKE_COMMON% -DBUILD_CODEC=OFF -S "%ROOT%\openjpeg" -B "%ROOT%\openjpeg\build_arm64"
cmake --build "%ROOT%\openjpeg\build_arm64" --config Release --parallel
copy /Y "%ROOT%\openjpeg\build_arm64\bin\Release\openjp2.lib" "%PKG%\openjp2.lib"
if errorlevel 1 echo FAILED: openjpeg

REM ==========================================================================
REM  4. libpng (depends on zlib already built)
REM ==========================================================================
echo.
echo === Building libpng ===
if not exist "%ROOT%\libpng\build_arm64" mkdir "%ROOT%\libpng\build_arm64"
cmake %CMAKE_COMMON% ^
    -DPNG_SHARED=OFF -DPNG_TESTS=OFF ^
    -DZLIB_LIBRARY="%ROOT%\zlib-ng\build_arm64\Release\zlibstatic.lib" ^
    -DZLIB_INCLUDE_DIR="%ROOT%\zlib-ng;%ROOT%\zlib-ng\build_arm64" ^
    -S "%ROOT%\libpng" -B "%ROOT%\libpng\build_arm64"
cmake --build "%ROOT%\libpng\build_arm64" --config Release --parallel
copy /Y "%ROOT%\libpng\build_arm64\Release\libpng16_static.lib" "%PKG%\libpng16.lib"
if errorlevel 1 echo FAILED: libpng

REM ==========================================================================
REM  5. libogg
REM ==========================================================================
echo.
echo === Building libogg ===
if not exist "%ROOT%\ogg\build_arm64" mkdir "%ROOT%\ogg\build_arm64"
cmake %CMAKE_COMMON% -S "%ROOT%\ogg" -B "%ROOT%\ogg\build_arm64"
cmake --build "%ROOT%\ogg\build_arm64" --config Release --parallel
if errorlevel 1 echo FAILED: libogg

REM ==========================================================================
REM  6. libvorbis (depends on ogg)
REM ==========================================================================
echo.
echo === Building libvorbis ===
if not exist "%ROOT%\vorbis\build_arm64" mkdir "%ROOT%\vorbis\build_arm64"
cmake %CMAKE_COMMON% ^
    -DOGG_ROOT="%ROOT%\ogg\build_arm64;%ROOT%\ogg" ^
    -S "%ROOT%\vorbis" -B "%ROOT%\vorbis\build_arm64"
cmake --build "%ROOT%\vorbis\build_arm64" --config Release --parallel
copy /Y "%ROOT%\vorbis\build_arm64\lib\Release\vorbis.lib" "%PKG%\vorbis.lib"
copy /Y "%ROOT%\vorbis\build_arm64\lib\Release\vorbisenc.lib" "%PKG%\vorbisenc.lib"
copy /Y "%ROOT%\vorbis\build_arm64\lib\Release\vorbisfile.lib" "%PKG%\vorbisfile.lib"
copy /Y "%ROOT%\ogg\build_arm64\Release\ogg.lib" "%PKG%\ogg.lib"
if errorlevel 1 echo FAILED: libvorbis

REM ==========================================================================
REM  7. freetype (depends on zlib and libpng)
REM ==========================================================================
echo.
echo === Building freetype ===
if not exist "%ROOT%\freetype\build_arm64" mkdir "%ROOT%\freetype\build_arm64"
cmake %CMAKE_COMMON% ^
    -DFT_DISABLE_BZIP2=ON -DFT_DISABLE_BROTLI=ON -DFT_DISABLE_HARFBUZZ=ON ^
    -DZLIB_LIBRARY="%ROOT%\zlib-ng\build_arm64\Release\zlibstatic.lib" ^
    -DZLIB_INCLUDE_DIR="%ROOT%\zlib-ng;%ROOT%\zlib-ng\build_arm64" ^
    -DPNG_LIBRARY="%ROOT%\libpng\build_arm64\Release\libpng16_static.lib" ^
    -DPNG_PNG_INCLUDE_DIR="%ROOT%\libpng;%ROOT%\libpng\build_arm64" ^
    -S "%ROOT%\freetype" -B "%ROOT%\freetype\build_arm64"
cmake --build "%ROOT%\freetype\build_arm64" --config Release --parallel
copy /Y "%ROOT%\freetype\build_arm64\Release\freetype.lib" "%PKG%\freetype.lib"
if errorlevel 1 echo FAILED: freetype

REM ==========================================================================
REM  8. OpenAL Soft (audio backend)
REM ==========================================================================
echo.
echo === Building OpenAL Soft ===
if not exist "%ROOT%\openal-soft\build_arm64" mkdir "%ROOT%\openal-soft\build_arm64"
if not exist "%PKG%\..\..\include\AL" mkdir "%PKG%\..\..\include\AL"
REM CMAKE_POLICY_VERSION_MINIMUM=3.5: OpenAL Soft's cmake_minimum_required predates modern
REM CMake's accepted minimum. LIBTYPE=SHARED produces OpenAL32.dll + import lib.
REM The viewer's OpenAL backend uses raw ALC + a built-in WAV loader, so freealut/ALUT is
REM NOT needed.
cmake -G "Visual Studio 17 2022" -A ARM64 -DCMAKE_POLICY_VERSION_MINIMUM=3.5 ^
    -DCMAKE_BUILD_TYPE=Release -DLIBTYPE=SHARED ^
    -DALSOFT_EXAMPLES=OFF -DALSOFT_UTILS=OFF -DALSOFT_TESTS=OFF -DALSOFT_INSTALL=OFF ^
    -S "%ROOT%\openal-soft" -B "%ROOT%\openal-soft\build_arm64"
cmake --build "%ROOT%\openal-soft\build_arm64" --config Release --target OpenAL --parallel
copy /Y "%ROOT%\openal-soft\build_arm64\Release\OpenAL32.dll"  "%PKG%\OpenAL32.dll"
copy /Y "%ROOT%\openal-soft\build_arm64\Release\OpenAL32.lib"  "%PKG%\OpenAL32.lib"
copy /Y "%ROOT%\openal-soft\include\AL\al.h"           "%PKG%\..\..\include\AL\al.h"
copy /Y "%ROOT%\openal-soft\include\AL\alc.h"          "%PKG%\..\..\include\AL\alc.h"
copy /Y "%ROOT%\openal-soft\include\AL\alext.h"        "%PKG%\..\..\include\AL\alext.h"
copy /Y "%ROOT%\openal-soft\include\AL\efx.h"          "%PKG%\..\..\include\AL\efx.h"
copy /Y "%ROOT%\openal-soft\include\AL\efx-presets.h"  "%PKG%\..\..\include\AL\efx-presets.h"
copy /Y "%ROOT%\openal-soft\include\AL\efx-creative.h" "%PKG%\..\..\include\AL\efx-creative.h"
if errorlevel 1 echo FAILED: OpenAL Soft

echo.
echo === All basic libraries done ===
