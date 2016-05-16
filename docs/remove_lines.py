#!/usr/bin/env python3
# -*- coding: utf-8 -*-
#
# Script taken from http://stackoverflow.com/a/17579949/4070143 by inspectorG4dget
# to remove lines above and below certain string match.
# Minor modifications to read and write from stdin/stdout respectively
# and to remove some extra newlines getting added

import sys

def remLines(delim, above, below):
    buff = []
    line = sys.stdin.readline()
    while line:
        if delim in line.strip():
            buff = []
            for _ in range(below):
                sys.stdin.readline()
        else:
            if len(buff) == above:
                print(buff[0].replace('\r', '').replace('\n', ''))
                buff = buff[1:]
            buff.append(line)
        line = sys.stdin.readline()
    print(''.join(buff).strip())

if __name__ == "__main__":
	if len(sys.argv) < 2:
		print("Error: no search pattern specified")
	else:
		remLines(sys.argv[1], 2, 1)
