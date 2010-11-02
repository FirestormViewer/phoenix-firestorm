Follow the snowstorm instructions for setting up a windows build environment
http://wiki.secondlife.com/wiki/Viewer_2_Microsoft_Windows_Builds

Make sure to copy fmod.dll into both your indra/ folder and also libraries/i686-win32/Release and libraries/RelWithDebInfo

Open up cygwin and navigate to your code directory. Example:
	cd /cygdrive/c/my/path/to/phoenix-firestorm

Execute the command to build firestorm in the cygwin window:

	./build_firestorm_win32.sh

This will do a clean compile. Rebuilds should be possible by specifying --rebuild. 

NOTE: It is normal to see errors about ambiguous include/library paths at this time. It will not cause the build to fail

