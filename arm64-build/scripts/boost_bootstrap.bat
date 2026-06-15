@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" -arch=arm64 -host_arch=arm64 >nul 2>&1
cd /d "C:\Work\Research\Firestorm\arm64-libs\boost_1_86_0"
echo Current dir: %cd%
call "C:\Work\Research\Firestorm\arm64-libs\boost_1_86_0\bootstrap.bat" vc143
