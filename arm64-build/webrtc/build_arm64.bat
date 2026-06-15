@echo off
setlocal

REM --- Find Visual Studio 2022 using vswhere ---
for /f "usebackq delims=" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath`) do set "VS_PATH=%%i"

if not defined VS_PATH (
    echo ERROR: Visual Studio not found.
    exit /b 1
)

echo Using Visual Studio at: %VS_PATH%

REM --- Set up ARM64 developer environment ---
call "%VS_PATH%\Common7\Tools\VsDevCmd.bat" -arch=arm64 -host_arch=arm64

REM --- Set environment variables for WebRTC build ---
set DEPOT_TOOLS_WIN_TOOLCHAIN=0
set GYP_MSVS_VERSION=2022
set PYTHONIOENCODING=utf-8

REM --- Add depot_tools to PATH ---
set "PATH=%~dp0_source\windows_arm64\webrtc\depot_tools;%PATH%"

REM --- Use Python from the Firestorm virtual environment ---
set "PYTHON=C:\Work\Research\Firestorm\.env\Scripts\python.exe"

REM --- Change to the build directory (run.py expects CWD = build/) ---
cd /d "%~dp0"

REM --- Run the build ---
echo Starting WebRTC ARM64 build...
"%PYTHON%" run.py build windows_arm64 %*
