Make sure Xcode is installed, it's a (sometimes) free download from Apple.
Make sure cmake is installed, use at least a 2.8.x version.

If you are using OSX 10.5 or 10.6, Xcode 3 will be used. Output will work on
all 10.5 systems and higher.

If you are using OSX 10.7 (ie, Lion), the build will use Xcode 4 and your
output will only work on OSX 10.6 and higher unless you install the OS X
10.5 SDK. You can copy that from an Xcode 3 installation; you need the
directory /Developer/SDKs/MacOSX10.5.sdk and its contents. If you do this,
then the output will work on 10.5.

Insure you can build a stock viewer-development tree as described in the SL
wiki.  Before asking for any help compiling Firestorm, make sure you can
build viewer-development first.  If you try and skip this step, you may
receive much less help. 
http://wiki.secondlife.com/wiki/Compiling_the_viewer_(Mac_OS_X)

If you want to use licensed FMOD or KDU build libraries (they are optional)
you have to provision these yourself.  If you're licensing these with
Phoenix/Firestorm, ask for the libraries for fmod and kdu.  Put them into:
        /opt/firestorm

If you're a community builder, you'll need to build these libraries
yourself, then change your autobuild.xml file to point to your own versions,
or create a different autobuild.xml with your customizations, and use this
with autobuild instead of our default autobuild.xml There are some examples
of how to build FMOD on the LL Wiki and opensource-dev mailing list.  We've
created a non-KDU build target to make this easier.  Everywhere you see
"ReleaseFS" below, use "ReleaseFS_open" instead.  This will perform the same
build, using openjpeg instead of KDU.

Available premade firestorm-specific build targets:

	ReleaseFS		(includes KDU, FMOD)
	ReleaseFS_open		(no KDU, no FMOD)
	RelWithDebInfo_open	(no KDU, no FMOD)

To build firestorm:

        autobuild build -c ReleaseFS                        

Other examples:

        autobuild configure -c ReleaseFS                    # basic configuration step, don't build, just configure
        autobuild configure -c ReleaseFS -- --clean         # clean the output area first, then configure
        autobuild configure -c ReleaseFS -- --chan Private-Yourname   # configure with a custom channel

        autobuild build -c ReleaseFS --no-configure              # default quick rebuild
        autobuild build -c ReleaseFS --no-configure -- --clean   # Clean rebuild

Any of the configure options can also be used (and do the same thing) with
the build options.  Typical LL autobuild configure options should also work,
as long as they don't duplicate configuration we are already doing.


Logs:

        Look for logs in build-darwin-i386/logs

Output:

        Look for output in build-darwin-i386/newview/Release
