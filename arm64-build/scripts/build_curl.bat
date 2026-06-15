@echo off
setlocal

for /f "usebackq delims=" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath`) do set "VS_PATH=%%i"
call "%VS_PATH%\Common7\Tools\VsDevCmd.bat" -arch=arm64 -host_arch=arm64

set "PATH=C:\Work\Research\Firestorm\arm64-libs\strawberry-perl\perl\bin;%PATH%"
set "ROOT=C:\Work\Research\Firestorm\arm64-libs"
set "PKG=C:\Work\Research\Firestorm\phoenix-firestorm\build-vc170-64\packages\lib\release"

echo === Configuring libcurl for ARM64 ===
cmake -G "Visual Studio 17 2022" -A ARM64 ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DBUILD_SHARED_LIBS=OFF ^
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5 ^
    -DCURL_STATICLIB=ON ^
    -DCMAKE_C_FLAGS="/DNGHTTP2_STATICLIB" ^
    -DBUILD_CURL_EXE=OFF ^
    -DBUILD_TESTING=OFF ^
    -DCMAKE_USE_OPENSSL=ON ^
    -DOPENSSL_ROOT_DIR="%ROOT%\openssl" ^
    -DOPENSSL_INCLUDE_DIR="%ROOT%\openssl\include" ^
    -DOPENSSL_CRYPTO_LIBRARY="%ROOT%\openssl\libcrypto.lib" ^
    -DOPENSSL_SSL_LIBRARY="%ROOT%\openssl\libssl.lib" ^
    -DUSE_NGHTTP2=ON ^
    -DNGHTTP2_INCLUDE_DIR="%ROOT%\nghttp2\lib\includes;%ROOT%\nghttp2\build_arm64\lib\includes" ^
    -DNGHTTP2_LIBRARY="%ROOT%\nghttp2\build_arm64\lib\Release\nghttp2.lib" ^
    -DZLIB_LIBRARY="%ROOT%\zlib-ng\build\Release\zlibstatic.lib" ^
    -DZLIB_INCLUDE_DIR="%ROOT%\zlib-ng\build" ^
    -S "%ROOT%\curl" ^
    -B "%ROOT%\curl\build_arm64"
if errorlevel 1 (
    echo ERROR: Configure failed
    exit /b 1
)

echo === Building libcurl ===
cmake --build "%ROOT%\curl\build_arm64" --config Release --target libcurl --parallel
if errorlevel 1 (
    echo ERROR: Build failed
    exit /b 1
)

echo === Copying library ===
copy /Y "%ROOT%\curl\build_arm64\lib\Release\libcurl.lib" "%PKG%\libcurl.lib"
echo === libcurl build complete ===
