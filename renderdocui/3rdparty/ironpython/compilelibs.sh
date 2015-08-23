#!/bin/sh

if [ $# -ne 1 ]; then
	echo "Usage: $0 /path/to/IronPython/";
	exit;
fi

IRONPYTHON="$1"
OUTDIR=$PWD

cd $IRONPYTHON/Lib
zip -r $OUTDIR/pythonlibs.zip *
