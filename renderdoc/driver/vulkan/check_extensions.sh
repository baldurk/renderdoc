#!/bin/bash

grep '<extension ' vk.xml |
	 grep -v 'supported="disabled"' | grep -v 'supported="vulkansc"' |
   sed -e '{s#.*name="\([^"]*\)".*number="\([0-9]*\)".*#\2    \1#g}' |
   sed -e '{s#\<[0-9]\>#00&#g}' | sed -e '{s#\<[0-9][0-9]\>#0&#g}' |
   sort > all_exts.txt
unix2dos -q all_exts.txt

export IFS="
"
for I in $(cat all_exts.txt | awk '{print $2}'); do
	if ! grep -q "$I" extension_support.md; then
		echo "$I isn't in extension_support.md";
	fi
done
