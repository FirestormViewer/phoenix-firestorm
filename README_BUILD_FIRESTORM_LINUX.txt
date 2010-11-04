First set up your system as described in the snowstorm linux wiki.
- Additionally, make sure gcc-4.3 ang g++-4.3 are installed.
- We do non-standalone builds

Run ./build_firestorm_linux.sh

NOTE: IF you receive build failures related to libUUID, copy your system libUUID library over the download supplied by SL:
	cd /your/firestorm/code/tree
	cp /lib/libuuid.so.1.3.0 libraries/i686-linux/lib_release_client/libuuid.so
	cp /lib/libuuid.so.1.3.0 libraries/i686-linux/lib_release_client/libuuid.so.1
