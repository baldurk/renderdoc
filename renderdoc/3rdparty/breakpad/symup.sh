#!/bin/bash
curl --form "key=$1" --form "sym=@$2" http://renderdoc.org/symupload
