First set up your system as described in the snowstorm linux wiki.
Note: You must manually install fmod as described there.

- Additionally, make sure gcc-4.4 ang g++-4.4 are installed.
- You should do non-standalone builds. If you try standalone, you will most likely run into trouble.

Run ./build_firestorm_linux.sh

By default your build will be set to use channel private-(your build machine). If you want to change this, 
you can use pass the option "--chan private-SomeNameYouPrefer" to the build command above.

NOTE: IF you receive build failures related to libUUID, copy your system libUUID library over the download supplied by SL:
	cd /your/firestorm/code/tree
	cp /lib/libuuid.so.1.3.0 libraries/i686-linux/lib_release_client/libuuid.so
	cp /lib/libuuid.so.1.3.0 libraries/i686-linux/lib_release_client/libuuid.so.1
