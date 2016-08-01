First, make sure gcc-4.7 and g++-4.7 are installed.

32bit build platforms are better tested at this point and strongly recommended.

Ensure you can build a stock viewer-development try as described in the SL wiki. Before asking for any help 
compiling Firestorm, make sure you can build viewer-development first. If you try and skip this step, you may
receive  much less help. http://wiki.secondlife.com/wiki/Compiling_the_viewer_(Linux)

If you want to use licensed FMOD or KDU build libraries (they are optional) you have to provision these yourself.
If you're licensing these with Phoenix/Firestorm, ask for the libraries for fmod and kdu. Put them into:
	/opt/firestorm

If you're a community builder, you'll need to build these libraries yourself, then change your autobuild.xml file to
point to your own versions, or create a different autobuild.xml with your customizations, and use this with autobuild
instead of our default autobuild.xml There are some examples of how to build FMOD on the LL Wiki and opensource-dev
mailing list. We've created a non-KDU build target to make this easier. Everywhere you see "ReleaseFS" below, use 
"ReleaseFS_open" instead.  This will perform the same build, using openjpeg instead of KDU.

Available premade firestorm-specific build targets:
	ReleaseFS (includes KDU, FMOD)
	ReleaseFS_open (no KDU, no FMOD)
	RelWithDebInfoFS_open (no KDU, no FMOD)

To build firestorm:
	autobuild build -c ReleaseFS                        

Other examples:
	autobuild configure -c ReleaseFS  		    # basic configuration step, don't build, just configure
	autobuild configure -c ReleaseFS -- --clean	    # clean the output area first, then configure
	autobuild configure -c ReleaseFS -- --chan Private-Yourname   # configure with a custom channel

	autobuild build -c ReleaseFS --no-configure		# default quick rebuild
	autobuild build -c ReleaseFS --no-configure -- --clean	# Clean rebuild
	autobuild build -c ReleaseFS -- --package		# Complete a build and package it into a tarball

Any of the configure options can also be used (and do the same thing) with the build options.
Typical LL autobuild configure options should also work, as long as they don't duplicate configuration we are
already doing.


Logs:
        Look for logs in build-linux-i686/logs

Output:
        Look for output in build-linux-i686/newview/Release


