<img align="left" width="100" height="100" src="doc/ayanestorm_256.png" alt="Logo of AyaneStorm viewer"/>

**[Firestorm](https://www.firestormviewer.org) is a free client for 3D virtual worlds such as Second Life and various OpenSim worlds where users can create, connect and chat with others from around the world.**

# AyaneStorm - A fork of Firestorm for photographers

## Features (so far ; some features might get merged in official Firestorm)
- Added poser from BlackDragon
- Added camera roll/tilt buttons from Blackdragon (+ key bindings) (merged in Firestorm)
- Snapshots up to 11500x11500 resolution
- AVX2 optimized build (now also in Firestorm)
- Made the Animation Speed menu easily accessible: it is no longer in Developer > Avatar > Animation Speed, it is now directly accessible in AyaneStorm > animation
- Added a "All Animations Slowed Down" menu that directly sets animations speed to the minimum (10%)
- Added a "Freeze Animations" menu in World > Animation Speed, effectively freezing all animations. Very useful for photographers!
- No longer shows the stupid ":non-potable_water:" and "fleur-de-lys" emojis when typing ":-P" or ":-D" (use package located in https://github.com/AyaneStorm/3p-emoji-shortcodes)
- Increased resolution limit to 2048x2048 for snapshot uploaded to inventory (merged in Second-Life)
- Updated OpenJPEG to 2.5.2 (for Jpeg2000 format) (Firestorm also updated to this version now)
- Fixed texture and snapshots poor upload quality with OpenJPEG (merged in Firestorm)
- Enabled "lossless" texture uploads
- New icons
- Custom login page
- Added support for WebP format, like in Alchemy
- Bigger camera controls
- Can double-click on an animation in the AO to play it (merged in Firestorm)
- Can preview profile, picks and 1st life as others would see it (merged in Firestorm)
- Can see oneself in Nearby people list, so that the people count is correct, and you can double-click on yourself to focus camera on you
- Bigger profile pictures!
- Changed location of "Drop inventory item here" in profile to a more logical one
- Additional advanced phototools by Kiss Spicoli
- Added a slider on top to change the render distance, just like in Starlight UI
- Includes latest updates from Firestorm
- Windows, macOS and Linux builds
- All builds with FMOD for audio and Kakadu for JPG2000 encoder/decoder

## Quick build instructions (for Windows):

First, make sure you installed all the prerequisites software. See [Windows](doc/building_windows.md)

Build without FMOD (the simplest):
```
git clone https://github.com/AyaneStorm/phoenix-firestorm.git
cd phoenix-firestorm
prepare_no_fmod.bat (only the first time)
build_no_fmod.bat
```

Build with FMOD (more complicated):
First, follow Alchemy Viewer instructions to generate the FMOD library file (you'll need Cygwin64).
Then update the set_fmod_vars.bat file with correct values.

- FMOD_HASH is the MD5 hash of the FMOD file
- FMOD_URL is the location of the FMOD file (it must start with file:///)

Then type:

```
git clone https://github.com/AyaneStorm/phoenix-firestorm.git
cd phoenix-firestorm
prepare_with_fmod.bat  (only the first time)
build_with_fmod.bat
```

If you already prepared the build and want to rebuild later with another CMD window:
- Open a new CMD window
- Depending on whether you want to build with FMOD or without, type `rebuild_with_fmod.bat` or `rebuild_no_fmod.bat` so that the required variables are set again
- Then type `build_with_fmod.bat` or `build_without_fmod.bat` for each subsequent build

### Kakadu build for Windows
There is absolutely no instructions anywhere for building SecondLife / Firestorm / Alchemy with Kakadu, but I managed to do it. If you're interested, do as follow:
- Acquire a Kakadu license (can't help you with this, and won't share mine, it's private, please don't ask)
- Install the Visual Studio 2019 Build Tools and optionnaly Visual Studio itself (won't work with VS2022 build tools with current Kakadu version 8.4.1)
- Install Mingw64, make sure that you can create .tar.bz2 files.
- Extract Kakadu library somewhere
- In coresys directory you'll find a VS2019 solution and project
- Set coresys VS2019 project configuration to `<ConfigurationType>StaticLibrary</ConfigurationType>` instead of `<ConfigurationType>DynamicLibrary</ConfigurationType>`
- Set linker additional dependencies to:
    `..\srclib_ht\Win-x86-64\kdu_ht2019R.lib;%(AdditionalDependencies) for Release`
    `..\srclib_ht\Win-x86-64\kdu_ht2019D.lib;%(AdditionalDependencies) for Debug`
  (It is simpler to edit that in the project properties in Visual Studio, but that's optional)
- Prepare Kakadu sources for high speed Kakadu if your license allows it (see Enabling_HT.txt in Kakadu library)
- In VS 2019 build tools, cd to Kakadu coresys folder and type:
    `msbuild.exe coresys_2019.sln -t:coresys -p:Configuration="Release" -p:Platform="x64"` for Release, or
    `msbuild.exe coresys_2019.sln -t:coresys -p:Configuration="Debug" -p:Platform="x64"` for Debug
- You'll obtain a kdu_84R.lib or a kdu_84D.lib in coresys/../../bin_x64/
- Git clone this repository: https://github.com/AyaneStorm/kdu_installable_template
- In kdu-8.4.1-windows-233220159-template directory:
  - Copy and rename kdu_84R.lib to lib/Release/kdu_x64.lib
  - Copy and rename kdu_84D.lib to lib/Debug/kdu_x64.lib
  - Copy all the .h files of Kakadu into include/ (all, not only coresys headers)
  - Copy the Kakadu license into LICENSES/
- In Mingw64, type:
    `cd kdu-8.4.1-windows-233220159-template/ && tar -cvjSf kdu-8.4.1-windows-233220159.tar.bz2 *`
    `mv kdu-8.4.1-windows-233220159.tar.bz2 ../ && cd .. && md5sum kdu-8.4.1-windows-233220159.tar.bz2`
- You should obtain a `kdu-8.4.1-windows-233220159.tar.bz2` file and its MD5 checksum (keep that checksum for later)
- Go to you my_autobuild.xml file, and look for the `kdu` key.
- The Windows related part should look like this (change paths where needed):
    ```
    <key>windows64</key>
    <map>
    <key>archive</key>
    <map>
        <key>hash</key>
        <string>the MD5 hash you obtained previously</string>
        <key>hash_algorithm</key>
        <string>md5</string>
        <key>url</key>
        <string>file:///C:/path/to/kakadu/kdu-8.4.1-windows-233220159.tar.bz2</string>
    </map>
    <key>name</key>
    <string>windows64</string>
    </map>
    ```
- Just reconfigure with --kdu and recompile.

## Open Source

Firestorm is a third party viewer derived from the official [Second Life](https://github.com/secondlife/viewer) client. The client codebase has been open source since 2007 and is available under the LGPL license.

## Download

Pre-built versions of the viewer releases for Windows, Mac and Linux can be downloaded from the [official website](https://www.firestormviewer.org/choose-your-platform/).

## Build Instructions

Build instructions for each operating system can be found using the links below and in the official [wiki](https://wiki.firestormviewer.org).

- [Windows](doc/building_windows.md)
- [Mac](doc/building_macos.md)
- [Linux](doc/building_linux.md)

> [!NOTE]
> We do not provide support for compiling the viewer or issues resulting from using a self-compiled viewer. However, there is a self-compilers group within Second Life that can be joined to ask questions related to compiling the viewer: [Firestorm Self Compilers](https://tinyurl.com/firestorm-self-compilers)

## Contribute

Help make Firestorm better! You can get involved with improvements by filing bugs and suggesting enhancements via [JIRA](https://jira.firestormviewer.org) or [creating pull requests](doc/FS_PR_GUIDELINES.md).
