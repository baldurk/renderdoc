#!/bin/sh

if [ $# -ne 1 ]; then
	echo "Usage: $0 /path/to/IronPython/";
	exit;
fi

IRONPYTHON="$1"
LIBS=$(cat libs.txt | awk '{print "Lib/"$1}')
OUTDIR=$PWD

cd $IRONPYTHON
./ipy.exe Tools/Scripts/pyc.py /target:dll /out:pythonlibs $LIBS
mv pythonlibs.dll $OUTDIR/
