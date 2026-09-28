@echo off
setlocal EnableExtensions

rem ============================================================================
rem Blazing Storm - Windows build environment helper
rem
rem Run this from a normal Windows Command Prompt:
rem     setup-build.cmd
rem
rem Optional actions:
rem     setup-build.cmd configure
rem     setup-build.cmd build
rem     setup-build.cmd rebuild
rem     setup-build.cmd shell
rem
rem "shell" opens a new cmd.exe with the environment kept active.
rem ============================================================================

set "PROJECT_DIR=%~dp0"
if "%PROJECT_DIR:~-1%"=="\" set "PROJECT_DIR=%PROJECT_DIR:~0,-1%"

set "CYGWIN_BIN=C:\cygwin64\bin"
set "AUTOBUILD_VSVER=170"

rem The standard layout used by the Firestorm build instructions is:
rem   <parent>\Blazing-Storm
rem   <parent>\fs-build-variables
for %%I in ("%PROJECT_DIR%\..") do set "PROJECT_PARENT=%%~fI"
set "AUTOBUILD_VARIABLES_FILE=%PROJECT_PARENT%\fs-build-variables\variables"

rem Put Cygwin first so Autobuild uses C:\cygwin64\bin\bash.exe instead of
rem Windows' C:\Windows\System32\bash.exe.
set "PATH=%CYGWIN_BIN%;%PATH%"

echo.
echo ============================================================
echo  Blazing Storm build environment
echo ============================================================
echo  Project:        %PROJECT_DIR%
echo  Visual Studio:  vc170 / VS2022
echo  Cygwin:         %CYGWIN_BIN%
echo  Variables:      %AUTOBUILD_VARIABLES_FILE%
echo ============================================================
echo.

if not exist "%CYGWIN_BIN%\bash.exe" (
    echo ERROR: Cygwin bash was not found at:
    echo        %CYGWIN_BIN%\bash.exe
    echo.
    echo Install 64-bit Cygwin or update CYGWIN_BIN in setup-build.cmd.
    exit /b 1
)

if not exist "%AUTOBUILD_VARIABLES_FILE%" (
    echo ERROR: Firestorm build variables were not found at:
    echo        %AUTOBUILD_VARIABLES_FILE%
    echo.
    echo Expected the repositories to be side-by-side:
    echo        %PROJECT_PARENT%\Blazing-Storm
    echo        %PROJECT_PARENT%\fs-build-variables
    echo.
    echo Clone them with:
    echo        cd /d "%PROJECT_PARENT%"
    echo        git clone https://github.com/FirestormViewer/fs-build-variables.git
    exit /b 1
)

where autobuild >nul 2>&1
if errorlevel 1 (
    echo ERROR: autobuild was not found on PATH.
    echo.
    echo Install it in your Python environment, then open a new Command Prompt.
    echo Example:
    echo        python -m pip install -r requirements.txt
    echo        python -m pip install --upgrade git+https://github.com/secondlife/autobuild.git#egg=autobuild
    exit /b 1
)

for /f "delims=" %%B in ('where bash 2^>nul') do (
    echo  bash:            %%B
    goto :bash_found
)
:bash_found
echo  AUTOBUILD_VSVER: %AUTOBUILD_VSVER%
echo.

cd /d "%PROJECT_DIR%"

if "%~1"=="" goto :usage
if /I "%~1"=="configure" goto :configure
if /I "%~1"=="build" goto :build
if /I "%~1"=="rebuild" goto :rebuild
if /I "%~1"=="shell" goto :shell

echo Unknown action: %~1
echo.
goto :usage

:configure
echo Configuring Blazing Storm...
autobuild configure -A 64 -c ReleaseFS_open -- --chan BlazingStorm -DLL_TESTS:BOOL=FALSE
exit /b %ERRORLEVEL%

:build
echo Building Blazing Storm...
autobuild build -A 64 -c ReleaseFS_open --no-configure
exit /b %ERRORLEVEL%

:rebuild
call :configure
if errorlevel 1 exit /b %ERRORLEVEL%
call :build
exit /b %ERRORLEVEL%

:shell
echo Opening a build-ready Command Prompt...
echo.
cmd /K "cd /d ""%PROJECT_DIR%"""
exit /b 0

:usage
echo Environment is ready for this script process.
echo.
echo Usage:
echo   setup-build.cmd configure   Configure CMake / Autobuild
echo   setup-build.cmd build       Compile using the existing configuration
echo   setup-build.cmd rebuild     Configure, then build
echo   setup-build.cmd shell       Open a build-ready Command Prompt
echo.
echo Recommended:
echo   setup-build.cmd rebuild
echo.
exit /b 0
