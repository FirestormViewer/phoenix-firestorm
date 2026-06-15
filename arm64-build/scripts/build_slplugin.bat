@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" -arch=arm64 -host_arch=arm64 >nul 2>&1
set "LL_BUILD=/MD /O2 /Ob2 /std:c++17 /Zc:wchar_t- /Zi /GR /DEBUG /DLL_RELEASE=1 /DLL_RELEASE_FOR_DOWNLOAD=1 /DNDEBUG /D_SECURE_STL=0 /D_HAS_ITERATOR_DEBUGGING=0 /DWIN32 /D_WINDOWS /DLL_WINDOWS=1 /DUNICODE /D_UNICODE /DWINVER=0x0601 /D_WIN32_WINNT=0x0601 /DLL_OS_DRAGDROP_ENABLED=1 /DLIB_NDOF=1"
set "AUTOBUILD_CONFIG_FILE=my_autobuild.xml"
set "PATH=C:\Work\Research\Firestorm\.env\Scripts;%PATH%"
msbuild.exe "C:\Work\Research\Firestorm\phoenix-firestorm\build-vc170-64\llplugin\slplugin\SLPlugin.vcxproj" -p:Configuration=Release -p:Platform=ARM64 -p:useenv=true -t:Build -verbosity:minimal -m
