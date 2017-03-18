#!/bin/bash
curl --user-agent "Microsoft-Symbol-Server" http://msdl.microsoft.com/download/symbols/$1/$2/${1%.*}.pd_ --output ${1%.*}.pd_
expand ${1%.*}.pd_ ${1%.*}.pdb
./makesym.sh ${1%.*}.pdb
rm ${1%.*}.pd_ ${1%.*}.pdb
