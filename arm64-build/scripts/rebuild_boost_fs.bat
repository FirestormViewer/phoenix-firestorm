@echo off
setlocal

for /f "usebackq delims=" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath`) do set "VS_PATH=%%i"
call "%VS_PATH%\Common7\Tools\VsDevCmd.bat" -arch=arm64 -host_arch=arm64

set "ROOT=C:\Work\Research\Firestorm\arm64-libs\boost_1_86_0"
set "PKG=C:\Work\Research\Firestorm\phoenix-firestorm\build-vc170-64\packages\lib\release"

cd /d "%ROOT%"

echo === Rebuilding boost_filesystem with /Zc:wchar_t- ===
"%ROOT%\b2.exe" --with-filesystem architecture=arm address-model=64 variant=release link=static runtime-link=shared threading=multi --layout=tagged cxxflags="/Zc:wchar_t-" -a
if errorlevel 1 (
    echo ERROR: boost_filesystem build failed
    exit /b 1
)

echo === Copying library ===
copy /Y "%ROOT%\stage\lib\libboost_filesystem-mt-a64.lib" "%PKG%\libboost_filesystem-mt-a64.lib"

echo === boost_filesystem rebuild complete ===
