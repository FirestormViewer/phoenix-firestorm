@echo off
setlocal EnableExtensions

rem ============================================================================
rem Blazing Storm - launch two local development viewers for possession testing.
rem
rem The raw Firestorm development executable expects the source newview folder
rem as its working directory so it can find app_settings, skins, and other
rem development resources. DLLs continue to load from the executable directory.
rem ============================================================================

set "PROJECT_DIR=%~dp0"
if "%PROJECT_DIR:~-1%"=="\" set "PROJECT_DIR=%PROJECT_DIR:~0,-1%"
set "WORK_DIR=%PROJECT_DIR%\indra\newview"

set "VIEWER_EXE=%PROJECT_DIR%\build-vc170-64\newview\Release\firestorm-bin.exe"

if not exist "%VIEWER_EXE%" (
    set "VIEWER_EXE=%PROJECT_DIR%\build-vc170-64\newview\RelWithDebInfo\firestorm-bin.exe"
)

if not exist "%VIEWER_EXE%" (
    set "VIEWER_EXE=%PROJECT_DIR%\build-vc170-64\newview\Debug\firestorm-bin.exe"
)

if not exist "%VIEWER_EXE%" (
    echo.
    echo ERROR: Could not find firestorm-bin.exe.
    echo.
    echo Expected it under:
    echo   %PROJECT_DIR%\build-vc170-64\newview\Release
    echo   %PROJECT_DIR%\build-vc170-64\newview\RelWithDebInfo
    echo   %PROJECT_DIR%\build-vc170-64\newview\Debug
    echo.
    echo Build first with:
    echo   setup-build.cmd rebuild
    echo.
    exit /b 1
)

if not exist "%WORK_DIR%\app_settings" (
    echo ERROR: Development resource folder was not found:
    echo        %WORK_DIR%\app_settings
    exit /b 1
)

echo.
echo ============================================================
echo  Blazing Storm two-viewer test
echo ============================================================
echo  Executable: %VIEWER_EXE%
echo  Working dir: %WORK_DIR%
echo ============================================================
echo.
echo Launching subject viewer...
start "Blazing Storm - Subject" /D "%WORK_DIR%" "%VIEWER_EXE%" --multiple

rem Give Windows a moment to create the first process before starting another.
timeout /t 2 /nobreak >nul

echo Launching controller viewer...
start "Blazing Storm - Controller" /D "%WORK_DIR%" "%VIEWER_EXE%" --multiple

echo.
echo Two viewer processes were started.
echo Log in with different Second Life accounts.
echo.
echo Subject:
echo   /blaze host
echo.
echo Controller:
echo   /blaze connect ^<pairing-code^>
echo.
echo Subject:
echo   /blaze accept
echo.
exit /b 0
