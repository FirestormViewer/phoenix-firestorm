Before you start configuring your Windows build system, be aware of our tested configurations:
    Memory: You will need at least 2GB RAM, 4GB strongly recommended.
    CPU: Multiple CPUs are strongly recommended. 
      A build can take over an hour.
    Visual Studio 2013 Community Edition.

Ensure you can build a stock viewer-development try as described in the SL wiki. Before asking for any help
compiling Firestorm, make sure you can build viewer-development first. If you try and skip this step, you may
receive much less help. http://wiki.secondlife.com/wiki/Visual_Studio_2013_Viewer_Builds

If you want to use licensed FMOD or KDU build libraries (they are optional) you have to provision these yourself.
If you're licensing these with Phoenix/Firestorm, ask for the libraries for fmod and kdu. Put them into:

        c:\cygwin\opt\firestorm

If you're a community builder, you'll need to build these libraries yourself, then change your autobuild.xml file to
point to your own versions, or create a different autobuild.xml with your customizations, and use this with autobuild
instead of our default autobuild.xml There are some examples of how to build FMOD on the LL Wiki and opensource-dev
mailing list. We've created a non-KDU build target to make this easier. Everywhere you see "ReleaseFS" below, use 
"ReleaseFS_open" instead. This will perform the same build, using OpenJpeg instead of KDU.


To build Firestorm:

Open a CMD shell and navigating to your firestorm code repo:

        autobuild build -c ReleaseFS

Other build targets you may use are:

    ReleaseFS (includes KDU, FMOD)
    ReleaseFS_open (no KDU, no FMOD)
    RelWithDebInfoFS_open (no KDU, no FMOD)

Other examples:
        autobuild configure -c ReleaseFS                    # basic configuration step, don't build, just configure
        autobuild configure -c ReleaseFS -- --clean         # clean the output area first, then configure
        autobuild configure -c ReleaseFS -- --chan Private-Yourname   # configure with a custom channel

        autobuild build -c ReleaseFS --no-configure              # default quick rebuild

If you want to set custom configuration, do this in the configure step separately from build, then run "autobuild
build -c ReleaseFS --no-configure" as a secondary step.

If you want to build the 64bit version, add the parameter -A 64 to the autobuild commands, e.g.:
        autobuild configure -A 64 -c ReleaseFS
        autobuild build -A 64 -c ReleaseFS --no-configure


Logs:
    Look for logs in build-vc120-32/logs for 32bit builds and build-vc120-64/logs for 64bit

Output:
    Look for output in build-vc120-32/newview/Release for 32bit builds and build-vc120-64/newview/Release for 64bit
