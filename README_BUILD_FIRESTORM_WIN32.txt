Before you start configuring your Windows build system, be aware of our tested configurations:
	Memory: You will need at least 2GB RAM
	CPU: Multiple CPUs are strongly recommended. A build can take over an hour.
	Tested Build Environments:
		WinXPSP3, 32bit, 2GB RAM, Visual Studio Pro 2005 SP1, latest VC++ runtime installed

If you are not using something that closely matches a tested configuration, you may run into trouble, 
particularly with different versions of Visual Studio.

To get started, follow the snowstorm instructions for setting up a windows build environment at this page:
http://wiki.secondlife.com/wiki/Viewer_2_Microsoft_Windows_Builds

GET THE PHOENIX SOURCE
======================

Open up cygwin and run the following commands one at a time
	mkdir /cygdrive/c/code
	cd /cygdrive/c/code
	hg clone http://hg.phoenixviewer.com/phoenix-firestorm-lpgl
	cd phoenix-firestorm-lgpl

Make sure to copy fmod.dll into both your indra/ folder and also libraries/i686-win32/Release and libraries/RelWithDebInfo

Open up cygwin and navigate to your code directory. Example:
	cd /cygdrive/c/my/path/to/phoenix-firestorm

COMMAND LINE BUILDS
===================

Execute the command to build firestorm in the cygwin window:

	./build_firestorm_win32.sh

This will do a clean compile. Rebuilds should be possible by specifying --rebuild. 

NOTE: It is normal to see errors about ambiguous include/library paths at this time. It will not cause the build to fail

A log for the build will be placed in logs/build_firestorm_windows.log


VISUAL STUDIO BUILDS
====================


1. Launch Visual Studio and open up <your downloaded phoenix code>\indra\build-vc80\Secondlife.sln
2. Set the build type to Release
3. Select the "firestorm-bin" target
4. Build.
