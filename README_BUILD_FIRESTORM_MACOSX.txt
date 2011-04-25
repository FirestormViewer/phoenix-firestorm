Make sure xcode is installed, it's a free download from apple.

Insure you use Xcode version 3, and not version 4. You make need to use xcode-select to change the version number.

Make sure cmake is installed, use at least a 2.8.x version.
- Additionally, patch your source directory with fmodmacapi per the older snowglobe instructions.
- run ./build_firestorm_macosx.sh

By default your build will be set to use channel private-(your build machine). If you want to change this, 
you can use pass the option "--chan private-SomeNameYouPrefer" to the build command above.

