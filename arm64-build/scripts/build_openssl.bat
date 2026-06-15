@echo off
setlocal

for /f "usebackq delims=" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath`) do set "VS_PATH=%%i"
call "%VS_PATH%\Common7\Tools\VsDevCmd.bat" -arch=arm64 -host_arch=arm64

set "PATH=C:\Work\Research\Firestorm\arm64-libs\strawberry-perl\perl\bin;%PATH%"
set "ROOT=C:\Work\Research\Firestorm\arm64-libs\openssl"
set "PKG=C:\Work\Research\Firestorm\phoenix-firestorm\build-vc170-64\packages\lib\release"

cd /d "%ROOT%"

echo === Configuring OpenSSL 1.1.1w for ARM64 ===
perl Configure VC-WIN64-ARM no-asm no-shared no-tests --prefix=C:\Work\Research\Firestorm\arm64-libs\openssl_install --openssldir=C:\Work\Research\Firestorm\arm64-libs\openssl_install\ssl
if errorlevel 1 (
    echo ERROR: Configure failed
    exit /b 1
)

echo === Building OpenSSL ===
nmake
if errorlevel 1 (
    echo ERROR: Build failed
    exit /b 1
)

echo === Copying libraries ===
copy /Y "%ROOT%\libssl.lib" "%PKG%\libssl.lib"
copy /Y "%ROOT%\libcrypto.lib" "%PKG%\libcrypto.lib"

echo === OpenSSL build complete ===
