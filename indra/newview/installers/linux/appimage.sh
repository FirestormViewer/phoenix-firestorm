#!/bin/bash

SCRIPT_PATH=`readlink -f $0`
SCRIPT_PATH=`dirname $SCRIPT_PATH`

echo "Trying to build AppImage in directory $1 into file $3"

# All hope is lost if there is no lsb_release command
command -v lsb_release >/dev/null 2>/dev/null || exit 0

if [ `lsb_release -is` != "Ubuntu" ]
then
	echo "Distribution is not Ubuntu, skipping AppImage creation"
	exit 0
fi

set -e

cd $1
pushd packaged

wget -q https://github.com/AppImage/AppImages/raw/master/functions.sh -O ./functions.sh
. ./functions.sh
rm functions.sh

cp firestorm AppRun
cp ${SCRIPT_PATH}/firestorm.desktop firestorm.desktop

copy_deps
copy_deps
copy_deps
delete_blacklisted

# Now copy everything to ./lib. The viewer binaries got build with a rpath pointing to ./lib so all so will be magically found there.
#find ./usr/lib/ -type f -print0 | xargs -0 -Ifile cp file ./lib/
#find ./lib/x86_64-linux-gnu/ -type f -print0 | xargs -0 -Ifile cp file ./lib/

#rm -rf ./usr/lib/
#rm -rf ./lib/x86_64-linux-gnu/

find . -empty -type d -delete

popd

wget "https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-x86_64.AppImage"
chmod a+x appimagetool-x86_64.AppImage
./appimagetool-x86_64.AppImage --appimage-extract
rm appimagetool-x86_64.AppImage
ARCH=x86_64 squashfs-root/AppRun packaged

if [ -f $2 ]
then
	mv $2 $3
fi
