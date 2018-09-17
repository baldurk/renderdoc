# Code Explanation

There are [several pages](https://github.com/baldurk/renderdoc/wiki/Code-Dives) on the wiki explaining different aspects of how the code fits together - like how the capture-side works vs replay-side, how shader debugging works, etc.

    renderdoc/ 
        CMakeLists.txt           ; The cmake file, will recurse into subdirectories to build them
        renderdoc.sln            ; VS2015 solution for windows building
        renderdoc/
            3rdparty/            ; third party utilities & libraries included
            drivers/             ; API-specific back-ends, can be individually skipped/removed
            ...                  ; everything else in here consists of the core renderdoc runtime
        renderdoccmd/            ; A small C++ utility program that runs to do various little tasks
        renderdocshim/           ; A tiny C DLL using only kernel32.dll that is used for global hooking
        qrenderdoc/              ; The Qt UI layer built on top of renderdoc/
        docs/                    ; source documentation for the .chm file or http://docs.renderdoc.org/
        util/                    ; folder for utility/support files - e.g. build scripts, installers, CI config
