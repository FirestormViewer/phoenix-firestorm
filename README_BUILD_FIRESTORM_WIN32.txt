Before you start configuring your Windows build system, be aware of our tested configurations:
	Memory: You will need at least 2GB RAM
	CPU: Multiple CPUs are strongly recommended. 
	  A build can take over an hour.
	Tested Build Environments:
	  WinXPSP3, 32bit, 2GB RAM, Visual Studio Pro 2005 SP1, 
	  latest VC++ runtime installed

If you are not using something that closely matches a tested configuration, you
may run into trouble, particularly with different versions of Visual Studio. If you are running VS2008, you may be able to build the viewer for your local machine, but not package it into an installer.

A free download of VS2005Express can be used to compile firestorm. You can download this at http://download.microsoft.com/download/8/3/a/83aad8f9-38ba-4503-b3cd-ba28c360c27b/ENU/vcsetup.exe

To get started, follow the snowstorm instructions for setting up a windows build environment at this page: http://wiki.secondlife.com/wiki/Viewer_2_Microsoft_Windows_Builds


GET THE PHOENIX SOURCE
======================

Open up cygwin and run the following commands one at a time
	mkdir /cygdrive/c/code
	cd /cygdrive/c/code
	hg clone http://hg.phoenixviewer.com/phoenix-firestorm-lgpl/

Make sure to copy fmod.dll into both your indra/ folder and also libraries/i686-win32/Release and libraries/RelWithDebInfo

COMMAND LINE BUILDS
===================

Open up cygwin and navigate to your code directory. Example:
        cd /cygdrive/c/my/path/to/phoenix-firestorm-lgpl

Execute the command to build firestorm in the cygwin window:

	./build_firestorm_win32.sh

This will do a clean compile. Rebuilds should be possible by specifying --rebuild. 

By default your build will be set to use channel private-(your build machine). If you want to change this, 
you can use pass the option "--chan private-SomeNameYouPrefer" to the build command above.

NOTE: It is normal to see errors about ambiguous include/library paths at this time. It will not cause the build to fail

A log for the build will be placed in logs/build_firestorm_windows.log

When the build completes, your output installer will be in indra/VC80/newview/Release, look for a <product-build>-Setup.exe file


VISUAL STUDIO BUILDS
====================

0. Open up a regular CMD.exe command window. Navigate to your downloaded source code.
1. Run the command "develop.py -G vc80 -t Release configure -DLL_TESTS:BOOL=OFF" Change vc80 to vc90 for VS2008 or to VC100 for VS2010
	(*as of Jan 1st 2011, VS2008 and VS2010 do not work, however, LL is working on supporting VS2010 in the future.) 
1. Launch Visual Studio and open up <your downloaded phoenix code>\indra\build-vc80\Secondlife.sln
2. Set the build type to Release
3. Select the "firestorm-bin" target
4. Build.
5. Your output installer will be in indra/VC80/newview/Release, look for a <product-build>-Setup.exe file


BUILD ERRORS
============

1. Google Breakpad

If your build fails because of an error in 'dump_syms.exe' download a new version of this executable that is statically linked. 
http://google-breakpad.googlecode.com/svn-history/r595/trunk/src/tools/windows/binaries/dump_syms.exe

Place this file under libraries/i686-win32/bin/dump_syms.exe

2. "Manifest multiple bindings error"

If your build fails to package with an error like the above, it is because you do not have an up to date C++ runtime library installed. You should enable windows auto-updates and install ALL required updates for your platform to resolve this error.


COMMITING CHANGES
=================

When commiting changes back to the phoenix-firestorm-lgpl repository, you must include the string "lgpl" or "LGPL" somewhere in your most recent commit message. Also, insure all code you commit to this repository is LGPL licensed!
