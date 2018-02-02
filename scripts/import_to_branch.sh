#!/bin/sh
read -p "Calling hg revert and purge on indra/. Then importing changes. Continue? " ANSWER

case "$ANSWER" in
	y|Y)echo "Carrying on"
		;;
	*) echo "Exit"
		exit 0
		;;
esac

if [ ! -d indra/ ]
then
	echo "Indra directory not found"
	exit 1
fi

hg revert indra/
hg revert autobuild.xml
hg purge indra/

for i in $(cat exp.txt|sort -n)
do
	echo "Importing change $i"
	hg import --no-commit -f  exp/$i.diff || exit 1
done

read -p "All patches applied without errors. Apply them again and commit? " ANSWER

case "$ANSWER" in
	y|Y)echo "Carrying on"
		;;
	*) echo "Exit"
		exit 0
		;;
esac

hg revert indra/
hg revert autobuild.xml
hg purge indra/

for i in $(cat exp.txt|sort -n)
do
	echo "Importing change $i"
	hg import exp/$i.diff || exit 1
done
