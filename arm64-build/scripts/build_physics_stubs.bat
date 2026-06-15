@echo off
setlocal

for /f "usebackq delims=" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath`) do set "VS_PATH=%%i"
call "%VS_PATH%\Common7\Tools\VsDevCmd.bat" -arch=arm64 -host_arch=arm64

set "ROOT=C:\Work\Research\Firestorm\arm64-libs"
set "INC=C:\Work\Research\Firestorm\phoenix-firestorm\build-vc170-64\packages\include"
set "PKG=C:\Work\Research\Firestorm\phoenix-firestorm\build-vc170-64\packages\lib\release"

echo === Building nd_Pathing stub for ARM64 ===
cl /c /O2 /MD /std:c++17 /EHsc /I"%INC%" "%ROOT%\nd_pathing_stub.cpp" /Fo"%ROOT%\nd_pathing_stub.obj"
if errorlevel 1 (
    echo ERROR: nd_Pathing compile failed
    exit /b 1
)
lib /OUT:"%PKG%\nd_Pathing.lib" "%ROOT%\nd_pathing_stub.obj"

echo === Building nd_hacdConvexDecomposition stub for ARM64 ===
cl /c /O2 /MD /std:c++17 /EHsc /I"%INC%" "%ROOT%\nd_hacd_stub.cpp" /Fo"%ROOT%\nd_hacd_stub.obj"
if errorlevel 1 (
    echo ERROR: nd_hacd compile failed
    exit /b 1
)
lib /OUT:"%PKG%\nd_hacdConvexDecomposition.lib" "%ROOT%\nd_hacd_stub.obj"

echo === Creating empty hacd.lib for ARM64 ===
lib /OUT:"%PKG%\hacd.lib" /DEF /NAME:hacd_empty /MACHINE:ARM64

echo === Physics stubs built successfully ===
