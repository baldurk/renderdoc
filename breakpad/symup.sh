#!/bin/bash
curl --form "key=PolyRADSymbolUpload" --form "sym=@$2" $1/symupload
