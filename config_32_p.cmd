@ECHO OFF
REM call c:\python27\autobuild11\scripts\activate.bat
autobuild --version
autobuild configure -c ReleaseFS -v -- --chan private
REM autobuild build --no-configure -c ReleaseFS
pause