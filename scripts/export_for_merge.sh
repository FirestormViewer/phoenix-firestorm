if [ -d exp/ ]
then
	rm exp/*.diff
else
	mkdir exp
fi

for i in $(cat exp.txt)
do
	hg export -o exp/$i.diff -r $i
 done
