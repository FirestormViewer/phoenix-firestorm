@echo off
setlocal EnableDelayedExpansion

REM ==========================================================================
REM  Firestorm Viewer ARM64 Build Script
REM  Runs CMake and MSBuild directly (bypasses autobuild for ARM64).
REM ==========================================================================

REM --- Find Visual Studio 2022 ---
set "VSWHERE=C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo ERROR: vswhere.exe not found.
    exit /b 1
)
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -property installationPath`) do set "VS_PATH=%%i"
if not defined VS_PATH (
    echo ERROR: Visual Studio 2022 not found.
    exit /b 1
)
echo Using Visual Studio at: %VS_PATH%

REM --- Set up ARM64 developer environment ---
call "%VS_PATH%\Common7\Tools\VsDevCmd.bat" -arch=arm64 -host_arch=arm64
if errorlevel 1 (
    echo ERROR: Failed to set up VS developer environment.
    exit /b 1
)

REM --- Paths ---
set "FIRESTORM_ROOT=C:\Work\Research\Firestorm"
set "SOURCE_DIR=%FIRESTORM_ROOT%\phoenix-firestorm"
set "BUILD_DIR=%SOURCE_DIR%\build-vc170-64"
set "VENV=%FIRESTORM_ROOT%\.env"

REM --- Add venv and autobuild to PATH ---
set "PATH=%VENV%\Scripts;%PATH%"
set "AUTOBUILD=%VENV%\Scripts\autobuild.exe"
set "AUTOBUILD_CONFIG_FILE=my_autobuild.xml"

REM --- LL_BUILD: compiler flags for ReleaseFS_open (from fs-build-variables) ---
set "LL_BUILD=/MD /O2 /Ob2 /std:c++17 /Zc:wchar_t- /Zi /GR /DEBUG /DLL_RELEASE=1 /DLL_RELEASE_FOR_DOWNLOAD=1 /DNDEBUG /D_SECURE_STL=0 /D_HAS_ITERATOR_DEBUGGING=0 /DWIN32 /D_WINDOWS /DLL_WINDOWS=1 /DUNICODE /D_UNICODE /DWINVER=0x0601 /D_WIN32_WINNT=0x0601 /DLL_OS_DRAGDROP_ENABLED=1"

REM --- Parse arguments ---
set "DO_CONFIGURE=1"
set "DO_BUILD=1"
set "BUILD_CONFIG=Release"

:parse_args
if "%~1"=="" goto done_args
if /i "%~1"=="--configure-only" (
    set "DO_BUILD=0"
    shift
    goto parse_args
)
if /i "%~1"=="--build-only" (
    set "DO_CONFIGURE=0"
    shift
    goto parse_args
)
if /i "%~1"=="--config" (
    set "BUILD_CONFIG=%~2"
    shift
    shift
    goto parse_args
)
echo Unknown argument: %~1
exit /b 1
:done_args

echo.
echo ===================================
echo  Firestorm ARM64 Build
echo  Config: %BUILD_CONFIG%
echo ===================================
echo.

REM --- Ensure build directory exists ---
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

REM --- Configure ---
if "%DO_CONFIGURE%"=="1" (
    echo [1/2] Configuring with CMake...
    cmake -G "Visual Studio 17 2022" -A ARM64 ^
        -S "%SOURCE_DIR%\indra" ^
        -B "%BUILD_DIR%" ^
        -DLL_TESTS:BOOL=OFF ^
        -DADDRESS_SIZE:STRING=64 ^
        -DCMAKE_BUILD_TYPE:STRING=Release ^
        -DUSE_FMODSTUDIO:BOOL=OFF ^
        -DUSE_KDU:BOOL=OFF ^
        -DOPENSIM:BOOL=ON ^
        -DHAVOK:BOOL=OFF ^
        -DHAVOK_TPV:BOOL=OFF ^
        -DUNATTENDED:BOOL=ON ^
        -DRELEASE_CRASH_REPORTING:BOOL=OFF ^
        -DUSE_OPENAL:BOOL=ON ^
        -DVIEWER_CHANNEL:STRING="Firestorm-ARM64" ^
        -DGITHASH:STRING=arm64dev
    if errorlevel 1 (
        echo ERROR: Configure step failed.
        exit /b 1
    )
    echo Configure completed successfully.
    echo.
)

REM --- Build ---
if "%DO_BUILD%"=="1" (
    echo [2/2] Building with MSBuild...
    msbuild.exe "%BUILD_DIR%\Firestorm.sln" ^
        -p:Configuration=%BUILD_CONFIG% ^
        -p:Platform=ARM64 ^
        -p:useenv=true ^
        -t:Build ^
        -verbosity:normal ^
        -m ^
        -flp:LogFile="%BUILD_DIR%\logs\build_arm64.log" ^
        -flp1:"errorsonly;LogFile=%BUILD_DIR%\logs\build_arm64_errors.log"
    if errorlevel 1 (
        echo ERROR: Build step failed.
        exit /b 1
    )
    echo Build completed successfully.
    echo.
)

echo ===================================
echo  Firestorm ARM64 build finished!
echo ===================================
