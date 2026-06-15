@echo off
REM Wrapper: run the webrtc m137 ARM64 compile, capturing all output to a log.
call "C:\Work\Research\Firestorm\libs\3p-webrtc-build\build\build_arm64.bat" > "C:\Work\Research\Firestorm\libs\3p-webrtc-build\build\_build\m137_compile.log" 2>&1
echo BUILD_EXIT_CODE=%errorlevel% >> "C:\Work\Research\Firestorm\libs\3p-webrtc-build\build\_build\m137_compile.log"
