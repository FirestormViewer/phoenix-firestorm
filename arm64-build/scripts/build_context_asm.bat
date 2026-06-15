@echo off
setlocal

call "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" -arch=arm64 -host_arch=arm64 >nul 2>&1

set "ASM_DIR=C:\Work\Research\Firestorm\arm64-libs\boost_1_86_0\libs\context\src\asm"
set "OUT_DIR=C:\Work\Research\Firestorm\arm64-libs\context_asm"
set "LIB_DIR=C:\Work\Research\Firestorm\arm64-libs\boost_1_86_0\stage\lib"

if not exist "%OUT_DIR%" mkdir "%OUT_DIR%"

echo === Assembling ARM64 context routines ===

armasm64 -nologo "%ASM_DIR%\jump_arm64_aapcs_pe_armasm.asm" -o "%OUT_DIR%\jump_fcontext.obj"
if errorlevel 1 (echo FAILED: jump_fcontext & exit /b 1)
echo Built jump_fcontext.obj

armasm64 -nologo "%ASM_DIR%\make_arm64_aapcs_pe_armasm.asm" -o "%OUT_DIR%\make_fcontext.obj"
if errorlevel 1 (echo FAILED: make_fcontext & exit /b 1)
echo Built make_fcontext.obj

armasm64 -nologo "%ASM_DIR%\ontop_arm64_aapcs_pe_armasm.asm" -o "%OUT_DIR%\ontop_fcontext.obj"
if errorlevel 1 (echo FAILED: ontop_fcontext & exit /b 1)
echo Built ontop_fcontext.obj

echo === Adding to libboost_context-mt-a64.lib ===
lib /nologo /out:"%LIB_DIR%\libboost_context-mt-a64.lib" "%LIB_DIR%\libboost_context-mt-a64.lib" "%OUT_DIR%\jump_fcontext.obj" "%OUT_DIR%\make_fcontext.obj" "%OUT_DIR%\ontop_fcontext.obj"
if errorlevel 1 (echo FAILED: lib merge & exit /b 1)

echo === Done ===
copy "%LIB_DIR%\libboost_context-mt-a64.lib" "C:\Work\Research\Firestorm\phoenix-firestorm\build-vc170-64\packages\lib\release\libboost_context-mt-a64.lib" >nul
echo Copied updated library to packages
