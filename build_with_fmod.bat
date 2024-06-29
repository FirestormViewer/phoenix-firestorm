autobuild configure -A 64 -v -c ReleaseFS_open -- --fmodstudio --package -DLL_TESTS:BOOL=FALSE --chan AyaneStorm --avx2
autobuild build -A 64 -c ReleaseFS_open --no-configure
