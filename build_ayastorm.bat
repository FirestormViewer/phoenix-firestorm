@echo off
set PYTHONUTF8=1
set AUTOBUILD_VSVER=170
set AUTOBUILD_VARIABLES_FILE=c:\firestorm\fs-build-variables\variables
set AUTOBUILD_CONFIG_FILE=my_autobuild.xml
set PATH=C:\cygwin64\bin;%PATH%

cd c:\firestorm\phoenix-firestorm

echo [1] 特殊文字を修正中...
python fixall2.py

echo [2] Configure中...
autobuild configure -A 64 -c ReleaseFS_open -- --fmodstudio -DLL_TESTS:BOOL=FALSE --package --chan Ayastorm-release
if errorlevel 1 (
    echo Configure失敗
    pause
    exit /b 1
)

echo [3] ビルド中...
autobuild build -A 64 -c ReleaseFS_open --no-configure
if errorlevel 1 (
    echo ビルド失敗
    pause
    exit /b 1
)

echo 完了
pause