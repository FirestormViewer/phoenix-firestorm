rem @echo off
setlocal

del /F /Q build_MSI\*
rmdir build_MSI
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

set WIX_SOURCE_DIR=%~dp0

shift
shift
shift
shift
shift
shift
shift
shift
shift

set UPGRADECODE=%1
set UPGRADECODE2=%2
set CHANSUFFIX=%3

if exist %VIEWER_BUILDDIR%\Leap.dll (
  set PACKAGE_LEAP=PACKAGE_LEAP=1
  echo Packaging Leap.dll
) else (
  set PACKAGE_LEAP=PACKAGE_LEAP=0
  echo Not Packaging Leap.dll
)

heat dir %VIEWER_BUILDDIR%\character -gg -cg fs_character -var var.BUILDDIR -dr INSTALLDIR -out character.wxs
heat dir %VIEWER_BUILDDIR%\fonts -gg -cg fs_fonts -var var.BUILDDIR -dr INSTALLDIR -out fonts.wxs
heat dir %VIEWER_BUILDDIR%\fs_resources -gg -cg fs_fsres -var var.BUILDDIR -dr INSTALLDIR -out fs_resources.wxs

python %WIX_SOURCE_DIR%\compress_assets.py %VIEWER_BUILDDIR%\skins %VIEWER_BUILDDIR%\skins
python %WIX_SOURCE_DIR%\compress_assets.py %VIEWER_BUILDDIR%\app_settings %VIEWER_BUILDDIR%\app_settings

candle -dBUILDDIR=%VIEWER_BUILDDIR%\character character.wxs
candle -dBUILDDIR=%VIEWER_BUILDDIR%\fonts fonts.wxs
candle -dBUILDDIR=%VIEWER_BUILDDIR%\fs_resources fs_resources.wxs

candle -dPLUGIN_SOURCEDIR=%PLUGIN_SOURCEDIR% %WIX_SOURCE_DIR%\llplugin.wxs
candle -dPROGRAM_FILE=%PROGRAM_FILE% -dMAJOR=%MAJOR% -dMINOR=%MINOR% -dHGCHANGE=%HGCHANGE% -dBUILDDIR=%VIEWER_BUILDDIR%\ -dWIX_SOURCEDIR=%WIX_SOURCE_DIR% -d%PACKAGE_LEAP% -dUPGRADECODE=%UPGRADECODE% -dCHANNEL_SUFFIX=%CHANSUFFIX% %WIX_SOURCE_DIR%\firestorm.wxs
candle -dPROGRAM_FILE=%PROGRAM_FILE% -dPROGRAM_VERSION=%PROGRAM_VERSION% -dCHANNEL_NAME=%CHANNEL_NAME% -dSETTINGS_FILE=%SETTINGS_FILE% -dPROGRAM_NAME=%PROGRAM_NAME% -dBUILDDIR=%VIEWER_BUILDDIR%\ -dCHANNEL_SUFFIX=%CHANSUFFIX% %WIX_SOURCE_DIR%\registry.wxs

light -sval -ext WixUIExtension -cultures:en-us -out %VIEWER_BUILDDIR%\%OUTPUT_FILE%.msi firestorm.wixobj character.wixobj fonts.wixobj fs_resources.wixobj llplugin.wixobj registry.wixobj

signtool.exe sign /n Phoenix /d Firestorm /du http://www.phoenixviewer.com /t http://timestamp.verisign.com/scripts/timstamp.dll %VIEWER_BUILDDIR%\%OUTPUT_FILE%.msi

candle -dMAJOR=%MAJOR% -dMINOR=%MINOR% -dHGCHANGE=%HGCHANGE% -dWIX_SOURCEDIR=%WIX_SOURCE_DIR% -dFS_MSI_FILE=%VIEWER_BUILDDIR%\%OUTPUT_FILE%.msi  -dUPGRADECODE=%UPGRADECODE2% -dCHANNEL_SUFFIX=%CHANSUFFIX% -ext WixBalExtension %WIX_SOURCE_DIR%\installer.wxs
light -sval -ext WixBalExtension -out %VIEWER_BUILDDIR%\%OUTPUT_FILE%.exe installer.wixobj

insignia -ib %VIEWER_BUILDDIR%\%OUTPUT_FILE%.exe -o engine.exe
signtool.exe sign /n Phoenix /d Firestorm /du http://www.phoenixviewer.com /t http://timestamp.verisign.com/scripts/timstamp.dll engine.exe
insignia -ab engine.exe %VIEWER_BUILDDIR%\%OUTPUT_FILE%.exe -o %VIEWER_BUILDDIR%\%OUTPUT_FILE%.exe

signtool.exe sign /n Phoenix /d Firestorm /du http://www.phoenixviewer.com /t http://timestamp.verisign.com/scripts/timstamp.dll %VIEWER_BUILDDIR%\%OUTPUT_FILE%.exe
echo "Done"