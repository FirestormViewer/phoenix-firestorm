@echo off
setlocal

for /f "usebackq delims=" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath`) do set "VS_PATH=%%i"
call "%VS_PATH%\Common7\Tools\VsDevCmd.bat" -arch=arm64 -host_arch=arm64

set "ROOT=C:\Work\Research\Firestorm\arm64-libs\hunspell"
set "PKG=C:\Work\Research\Firestorm\phoenix-firestorm\build-vc170-64\packages\lib\release"

echo === Building hunspell for ARM64 ===

cl /c /O2 /MD /std:c++17 /EHsc /DHUNSPELL_STATIC ^
    /I"%ROOT%\msvc" /I"%ROOT%\src\hunspell" ^
    "%ROOT%\src\hunspell\affentry.cxx" ^
    "%ROOT%\src\hunspell\affixmgr.cxx" ^
    "%ROOT%\src\hunspell\csutil.cxx" ^
    "%ROOT%\src\hunspell\filemgr.cxx" ^
    "%ROOT%\src\hunspell\hashmgr.cxx" ^
    "%ROOT%\src\hunspell\hunspell.cxx" ^
    "%ROOT%\src\hunspell\hunzip.cxx" ^
    "%ROOT%\src\hunspell\phonet.cxx" ^
    "%ROOT%\src\hunspell\replist.cxx" ^
    "%ROOT%\src\hunspell\suggestmgr.cxx"
if errorlevel 1 (
    echo ERROR: Compile failed
    exit /b 1
)

lib /OUT:"%PKG%\libhunspell.lib" affentry.obj affixmgr.obj csutil.obj filemgr.obj hashmgr.obj hunspell.obj hunzip.obj phonet.obj replist.obj suggestmgr.obj
if errorlevel 1 (
    echo ERROR: lib failed
    exit /b 1
)

echo === hunspell build complete ===
