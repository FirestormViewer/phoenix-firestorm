@echo off
setlocal

for /f "usebackq delims=" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath`) do set "VS_PATH=%%i"
call "%VS_PATH%\Common7\Tools\VsDevCmd.bat" -arch=arm64 -host_arch=arm64

set "ROOT=C:\Work\Research\Firestorm\arm64-libs\colladadom"
set "PKGINC=C:\Work\Research\Firestorm\phoenix-firestorm\build-vc170-64\packages\include"
set "PKG=C:\Work\Research\Firestorm\phoenix-firestorm\build-vc170-64\packages\lib\release"
set "OBJDIR=%ROOT%\build_arm64_obj"

if not exist "%OBJDIR%" mkdir "%OBJDIR%"

set CFLAGS=/c /O2 /MD /std:c++17 /EHsc /Zc:wchar_t- /DWIN32 /DNDEBUG /D_CRT_SECURE_NO_DEPRECATE /DDOM_INCLUDE_LIBXML /DLIBXML_STATIC
set INCLUDES=/I"%ROOT%\include" /I"%ROOT%\include\1.4" /I"%PKGINC%" /I"%PKGINC%\libxml2" /I"%PKGINC%\minizip-ng" /I"%PKGINC%\zlib-ng" /I"%ROOT%\external-libs\tinyxml"

echo === Compiling colladadom base sources ===
cl %CFLAGS% %INCLUDES% /Fo"%OBJDIR%\\" "%ROOT%\src\dae\*.cpp"
if errorlevel 1 (
    echo ERROR: dae compile failed
    exit /b 1
)

cl %CFLAGS% %INCLUDES% /Fo"%OBJDIR%\\" "%ROOT%\src\modules\LIBXMLPlugin\*.cpp"
if errorlevel 1 (
    echo ERROR: LIBXMLPlugin compile failed
    exit /b 1
)

cl %CFLAGS% %INCLUDES% /Fo"%OBJDIR%\\" "%ROOT%\src\modules\STLDatabase\*.cpp"
if errorlevel 1 (
    echo ERROR: STLDatabase compile failed
    exit /b 1
)

cl %CFLAGS% %INCLUDES% /Fo"%OBJDIR%\\" "%ROOT%\src\modules\stdErrPlugin\*.cpp"
if errorlevel 1 (
    echo ERROR: stdErrPlugin compile failed
    exit /b 1
)

echo === Compiling collada 1.4 DOM sources ===
cl %CFLAGS% %INCLUDES% /Fo"%OBJDIR%\\" "%ROOT%\src\1.4\dom\*.cpp"
if errorlevel 1 (
    echo ERROR: dom 1.4 compile failed
    exit /b 1
)

echo === Linking static library ===
lib /OUT:"%PKG%\libcollada14dom23-s.lib" "%OBJDIR%\*.obj"
if errorlevel 1 (
    echo ERROR: lib failed
    exit /b 1
)

echo === colladadom build complete ===
