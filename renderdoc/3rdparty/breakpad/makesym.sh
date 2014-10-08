#!/bin/bash
./dump_syms.exe $1 > ${1%.*}.sym
