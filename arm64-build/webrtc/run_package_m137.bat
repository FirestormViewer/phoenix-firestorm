@echo off
setlocal

REM --- Find Visual Studio 2022 ---
for /f "usebackq delims=" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath`) do set "VS_PATH=%%i"
call "%VS_PATH%\Common7\Tools\VsDevCmd.bat" -arch=arm64 -host_arch=arm64

set DEPOT_TOOLS_WIN_TOOLCHAIN=0
set GYP_MSVS_VERSION=2022
set PYTHONIOENCODING=utf-8
set "PATH=%~dp0_source\windows_arm64\depot_tools;%PATH%"
set "PYTHON=C:\Work\Research\Firestorm\.env\Scripts\python.exe"

cd /d "%~dp0"
echo Packaging WebRTC m137 ARM64...
"%PYTHON%" run.py package windows_arm64
