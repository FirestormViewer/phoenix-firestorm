![Firestorm Viewer Logo](doc/firestorm_256.png)

# AyaneStorm

Quick build instructions (for Windows):

First, make sure you installed all the prerequisites software. See [Windows](doc/building_windows.md)

Build without FMOD (the simplest):
```
git clone https://github.com/AyaneStorm/phoenix-firestorm.git
cd phoenix-firestorm
prepare_no_fmod.bat (only the first time)
build_no_fmod.bat
```

Build with FMOD (more complicated):
First, follow Alchemy Viewer instructions (you'll need Cygwin64).
Then update the set_fmod_vars.bat file with correct values.

- FMOD_HASH is the MD5 hash of the FMOD file
- FMOD_URL is the locaiton of the FMOD file (starts with file:///)

Then type:

```
git clone https://github.com/AyaneStorm/phoenix-firestorm.git
cd phoenix-firestorm
prepare_with_fmod.bat  (only the first time)
build_with_fmod.bat
```

If you already prepared the build and want to rebuild later:
- Open a CMD prompt
- Depending on you want to build with FMOD or without, type `rebuild_with_fmod.bat` or `rebuild_no_fmod.bat`
- Then type `build_with_fmod.bat` or `build_without_fmod.bat`

**[Firestorm](https://www.firestormviewer.org/) is a free client for 3D virtual worlds such as Second Life and various OpenSim worlds where users can create, connect and chat with others from around the world.** This repository contains the official source code for the Firestorm viewer.

## Open Source

Firestorm is a third party viewer derived from the official [Second Life](https://github.com/secondlife/viewer) client. The client codebase has been open source since 2007 and is available under the LGPL license.

## Download

Pre-built versions of the viewer releases for Windows, Mac and Linux can be downloaded from the [official website](https://www.firestormviewer.org/choose-your-platform/).

## Build Instructions

Build instructions for each operating system can be found using the links below and in the official [wiki](https://wiki.firestormviewer.org/).

- [Windows](doc/building_windows.md)
- [Mac](doc/building_macos.md)
- [Linux](doc/building_linux.md)

> [!NOTE]
> We do not provide support for compiling the viewer or issues resulting from using a self-compiled viewer. However, there is a self-compilers group within Second Life that can be joined to ask questions related to compiling the viewer: [Firestorm Self Compilers](https://tinyurl.com/firestorm-self-compilers)

## Contribute

Help make Firestorm better! You can get involved with improvements by filing bugs and suggesting enhancements via [JIRA](https://jira.firestormviewer.org) or creating pull requests.
