@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" -arch=arm64 -host_arch=arm64 >nul 2>&1
set "MSVC_LIB=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\lib\arm64"
echo Searching MSVC ARM64 libs for POPCNT64...
for %%f in ("%MSVC_LIB%\*.lib") do (
    dumpbin /symbols "%%f" 2>nul | findstr /C:"POPCNT64" >nul 2>&1
    if not errorlevel 1 echo FOUND in: %%~nxf
)
set "SDK_LIB=C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\um\arm64"
echo Searching Windows SDK ARM64 libs for POPCNT64...
for %%f in ("%SDK_LIB%\*.lib") do (
    dumpbin /symbols "%%f" 2>nul | findstr /C:"POPCNT64" >nul 2>&1
    if not errorlevel 1 echo FOUND in: %%~nxf
)
echo Done searching.
