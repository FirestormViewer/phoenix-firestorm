This is WIP. Please change/expand when seeing fit.

1.1 Visual Studio 2013 Community Edition or better with installed 64 bit compiler.
1.2 Or Visual Studio Express and 'Microsoft Windows SDK for Windows 7 and .NET Framework 4'. Make sure to install at least the header and compiler with all the latest service packs.
2. autobuild from https://bitbucket.org/NickyD/autobuild-1.0
3. FMOD, if you want sound, please see https://bitbucket.org/NickyD/3p-fmodex

You will find the urls to all 64 bit prebuild packages in <viewer_source_dir>/package_override_vc12.ini

Make sure you're not building from a Visual Studio command prompt, or parts of the build
chain might accidentally pick up a 32 bit compiler, resulting in x86<>x64 mismatch.

Configure/build is nearly the same as building a 32 bit version.
autobuild -m64 configure -c ReleaseFS -- --chan <channel> --package
autobuild -m64 build -c ReleaseFS --no-configure

The resulting installer/exe gets created in build-vc120_x64/newview/Release
