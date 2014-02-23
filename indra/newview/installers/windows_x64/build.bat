@echo off
setlocal

md build_MSI
cd build_MSI

set VIEWER_BUILDDIR=%1
set PROGRAM_NAME=Firestorm_x64
set CHANNEL_NAME=%2
set SETTINGS_FILE=%4
set PROGRAM_FILE=Firestorm-bin.exe
set PROGRAM_VERSION=%2
set PLUGIN_SOURCEDIR=%VIEWER_BUILDDIR%
set OUTPUT_FILE=%5

set MAJOR=%6
set MINOR=%7
set HGCHANGE=%9


set PATH=%PATH%;%1\..\..\packages\bin\wix

heat dir %VIEWER_BUILDDIR%\character -gg -cg fs_character -var var.BUILDDIR -dr INSTALLDIR -out character.wxs
heat dir %VIEWER_BUILDDIR%\fonts -gg -cg fs_fonts -var var.BUILDDIR -dr INSTALLDIR -out fonts.wxs
heat dir %VIEWER_BUILDDIR%\fs_resources -gg -cg fs_fsres -var var.BUILDDIR -dr INSTALLDIR -out fs_resources.wxs

python %~dp0\compress_assets.py %VIEWER_BUILDDIR%\skins %VIEWER_BUILDDIR%\skins
python %~dp0\compress_assets.py %VIEWER_BUILDDIR%\app_settings %VIEWER_BUILDDIR%\app_settings

candle -dBUILDDIR=%VIEWER_BUILDDIR%\character character.wxs
candle -dBUILDDIR=%VIEWER_BUILDDIR%\fonts fonts.wxs
candle -dBUILDDIR=%VIEWER_BUILDDIR%\fs_resources fs_resources.wxs

candle -dPLUGIN_SOURCEDIR=%PLUGIN_SOURCEDIR% %~dp0\llplugin.wxs
candle -dPROGRAM_FILE=%PROGRAM_FILE% -dMAJOR=%MAJOR% -dMINOR=%MINOR% -dHGCHANGE=%HGCHANGE% -dBUILDDIR=%VIEWER_BUILDDIR%\ -dWIX_SOURCEDIR=%~dp0 %~dp0\firestorm.wxs
candle -dPROGRAM_FILE=%PROGRAM_FILE% -dPROGRAM_VERSION=%PROGRAM_VERSION% -dCHANNEL_NAME=%CHANNEL_NAME% -dSETTINGS_FILE=%SETTINGS_FILE% -dPROGRAM_NAME=%PROGRAM_NAME% -dBUILDDIR=%VIEWER_BUILDDIR%\ %~dp0\registry.wxs

light -sval -ext WixUIExtension -cultures:en-us -out %VIEWER_BUILDDIR%\%OUTPUT_FILE% firestorm.wixobj app_settings.wixobj character.wixobj fonts.wixobj fs_resources.wixobj llplugin.wixobj registry.wixobj
