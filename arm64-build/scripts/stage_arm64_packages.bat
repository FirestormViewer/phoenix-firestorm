@echo off
setlocal EnableDelayedExpansion
REM ==========================================================================
REM  Authoritative ARM64 3p-library staging for the Firestorm viewer.
REM
REM  Run this AFTER:
REM    1. the viewer has been cmake-configured (autobuild has installed the x64
REM       prebuilt packages into build-vc170-64\packages -- these provide the
REM       arch-independent HEADERS; their x64 .libs are overwritten below), and
REM    2. all the ARM64 3p libs have been built (build_all_arm64.bat,
REM       boost_build.bat, rebuild_boost_fs.bat, rebuild_boost_fiber.bat,
REM       build_openssl/curl/libxml2/minizip/openxr.bat, and the apr/expat/zlib
REM       builds) so their outputs exist in the per-lib build dirs.
REM
REM  This overlays the ARM64 .libs over the x64 ones, stages the OpenAL runtime
REM  + headers (openal use_prebuilt is guarded off for ARM64), the m137 webrtc
REM  lib, and rebuilds hunspell + colladadom (which emit straight to the package
REM  dir). It exists because the manual overlay kept missing pieces (OpenAL
REM  DLL/headers, the /Zc:wchar_t- boost_filesystem, the patched boost_fiber).
REM ==========================================================================

set "ROOT=C:\Work\Research\Firestorm\arm64-libs"
set "WRTC=C:\Work\Research\Firestorm\libs\3p-webrtc-build\build\_package\windows_arm64\webrtc"
set "PKG=C:\Work\Research\Firestorm\phoenix-firestorm\build-vc170-64\packages\lib\release"
set "INC=C:\Work\Research\Firestorm\phoenix-firestorm\build-vc170-64\packages\include"

if not exist "%PKG%" ( echo ERROR: package dir not found - configure the viewer first: "%PKG%" & exit /b 1 )

echo === Overlaying ARM64 static libs ===
call :cp "%ROOT%\zlib-ng\build\Release\zlibstatic.lib"                 "%PKG%\zlib.lib"
call :cp "%ROOT%\libexpat\build\Release\libexpatMD.lib"               "%PKG%\libexpat.lib"
call :cp "%ROOT%\apr-1_merged.lib"                                    "%PKG%\apr-1.lib"
call :cp "%ROOT%\apr-util\build\Release\aprutil-1.lib"               "%PKG%\aprutil-1.lib"
call :cp "%ROOT%\meshoptimizer\build_arm64\Release\meshoptimizer.lib" "%PKG%\meshoptimizer.lib"
call :cp "%ROOT%\libjpeg-turbo\build_arm64\Release\jpeg-static.lib"   "%PKG%\jpeg.lib"
call :cp "%ROOT%\libpng\build_arm64\Release\libpng16_static.lib"      "%PKG%\libpng16.lib"
call :cp "%ROOT%\libxml2\build_arm64\Release\libxml2s.lib"            "%PKG%\libxml2.lib"
call :cp "%ROOT%\minizip-ng\build_arm64\Release\minizip.lib"          "%PKG%\minizip.lib"
call :cp "%ROOT%\ogg\build_arm64\Release\ogg.lib"                     "%PKG%\libogg.lib"
call :cp "%ROOT%\vorbis\build_arm64\lib\Release\vorbis.lib"           "%PKG%\libvorbis.lib"
call :cp "%ROOT%\vorbis\build_arm64\lib\Release\vorbisenc.lib"        "%PKG%\libvorbisenc.lib"
call :cp "%ROOT%\vorbis\build_arm64\lib\Release\vorbisfile.lib"       "%PKG%\libvorbisfile.lib"
call :cp "%ROOT%\freetype\build_arm64\Release\freetype.lib"           "%PKG%\freetype.lib"
call :cp "%ROOT%\openjpeg\build_arm64\bin\Release\openjp2.lib"        "%PKG%\openjp2.lib"
call :cp "%ROOT%\openxr\build_arm64\src\loader\Release\openxr_loader.lib" "%PKG%\openxr_loader.lib"
call :cp "%ROOT%\nghttp2\build_arm64\lib\Release\nghttp2.lib"         "%PKG%\nghttp2.lib"
call :cp "%ROOT%\curl\build_arm64\lib\Release\libcurl.lib"            "%PKG%\libcurl.lib"
call :cp "%ROOT%\openssl\libssl.lib"                                 "%PKG%\libssl.lib"
call :cp "%ROOT%\openssl\libcrypto.lib"                              "%PKG%\libcrypto.lib"

echo === Overlaying boost (18 libs, -a64; filesystem must be /Zc:wchar_t-, fiber must be winfib-patched) ===
for %%f in ("%ROOT%\boost_1_86_0\stage\lib\libboost_*-mt-a64.lib") do call :cp "%%~f" "%PKG%\%%~nxf"

echo === OpenAL (lib + dll + headers; package guarded off for ARM64) ===
call :cp "%ROOT%\openal-soft\build_arm64\Release\OpenAL32.lib" "%PKG%\OpenAL32.lib"
call :cp "%ROOT%\openal-soft\build_arm64\Release\OpenAL32.dll" "%PKG%\OpenAL32.dll"
if not exist "%INC%\AL" mkdir "%INC%\AL"
for %%h in (al.h alc.h alext.h efx.h efx-presets.h efx-creative.h) do call :cp "%ROOT%\openal-soft\include\AL\%%h" "%INC%\AL\%%h"

echo === webrtc m137 (ARM64 lib over the x64 package lib) ===
call :cp "%WRTC%\lib\webrtc.lib" "%PKG%\webrtc.lib"

echo === hunspell + colladadom (compiled straight into the package dir) ===
call "%ROOT%\build_hunspell.bat"
call "%ROOT%\build_colladadom.bat"

echo === Stage complete ===
goto :eof

:cp
if exist "%~1" ( copy /Y "%~1" "%~2" >nul && echo   staged %~nx2 ) else ( echo   MISSING SOURCE: %~1 ^(-^> %~nx2^) )
goto :eof
