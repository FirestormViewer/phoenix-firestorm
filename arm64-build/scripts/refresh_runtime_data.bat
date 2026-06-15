@echo off
setlocal
REM ==========================================================================
REM  Refresh the viewer's runtime data files in the build output.
REM
REM  Dev builds only stage app_settings/skins into Release\ during the PACKAGING
REM  manifest action, not normal builds -- so after a source update (e.g. the
REM  7.2.4 rebase) the deployed data goes stale and the viewer crashes at startup
REM  (e.g. "couldn't access some of the files" / missing setting DebugQualityPerformance).
REM
REM  Run this after building when you launch firestorm-bin.exe directly from
REM  Release\. (Alternatively, run the viewer with working directory set to
REM  indra\newview, which always uses the live source data.)
REM ==========================================================================

set "SRC=C:\Work\Research\Firestorm\phoenix-firestorm\indra\newview"
set "REL=C:\Work\Research\Firestorm\phoenix-firestorm\build-vc170-64\newview\Release"

if not exist "%REL%" ( echo ERROR: Release dir not found: "%REL%" & exit /b 1 )

echo === Refreshing app_settings ===
xcopy /E /Y /I /Q "%SRC%\app_settings" "%REL%\app_settings" >nul

echo === Refreshing skins (large) ===
xcopy /E /Y /I /Q "%SRC%\skins" "%REL%\skins" >nul

echo === Runtime data refreshed ===
