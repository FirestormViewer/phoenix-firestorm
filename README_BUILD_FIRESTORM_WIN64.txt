This is WIP. Please change/expand when seeing fit.

- Visual Studio 2010 with installed 64 bit compiler (Express ist not supported).
- autobuild from https://bitbucket.org/NickyD/autobuild
- M2Crypto from http://chandlerproject.org/Projects/MeTooCrypto You need it for the autobuild above
- FMOD if you want sound, please see https://bitbucket.org/NickyD/3p-fmodex

You will find the urls to all 64 bit prebuild packages in <viewer_source_dir>/package_override.ini

Make sure you're not building from a Visual Studio command prompt, or parts of the build
chain might accidentally pick up a 32 bit compiler, resulting in x86<>x64 mismatch.
If you're getting problems with missing msbuild.exe during autobuild build, you can
set the environment variables for a 64 bit tool chain by running vcvarsx86_amd64.bat.
(Located in "c:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\bin\x86_amd64" on my system).

Configure/build is nearly the same as building a 32 bit version.
autobuild -m64 configure -c ReleaseFS -- --package
autobuild -m64 build -c ReleaseFS --no_configure

The resulting installer/exe gets created in build-vc100_x64/newview/Release

