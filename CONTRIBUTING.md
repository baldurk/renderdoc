Contributing & Development
--------------

There are always plenty of things to do, if you'd like to chip in! Check out the [Roadmap](https://github.com/baldurk/renderdoc/wiki/Roadmap) page in the wiki for future tasks to tackle, or have a look at the [issues](https://github.com/baldurk/renderdoc/issues) for outstanding bugs. I'll try and tag things that seem like small changes that would be a good way for someone to get started with.

If you have a change you'd like to see make it into mainline, create a fork of renderdoc, make your changes to a branch, and open a pull request on github. You can look around for instructions on that - it's pretty simple.

We can discuss changes if there need to be any, then merge it in. Please make sure your changes are fully rebased against master when you create the pull request.

If you're tackling anything large then please contact me and post an issue so that everyone knows you're working on it and there's not duplicated effort. *Specifically* if you want to extend RenderDoc to a platform or API that it doesn't already support please get in touch, as there might already be code that isn't committed yet. Particularly if this is not a public API that anyone can write against.

To get started compiling there are instructions in [COMPILE.md](COMPILE.md)

Commit Messages
--------------

Ideally keep your commit messages to the form:

```
Short summary text, maximum 72 characters

Longer paragraph that details the change and any logic behind needing
to make the change, other impacts, future work etc.
```

I like to make the second paragraph bulletted with *s as you can probably see if you look over the commit history, but this is just personal preference and you can format it any way you like. It's important to keep the first line under 72 characters as this way it will display fully in git log output and in all of github's log previews.

Copyright / Contributor License Agreement
--------------

Any code you submit will become part of the repository and be distributed under the [RenderDoc license](LICENSE.md). By submitting code to the project you agree that the code is your own work and that you have the ability to give it to the project. You also agree by submitting your code that you grant all transferrable rights to the code to the project maintainer, including for example re-licensing the code, modifying the code, distributing in source or binary forms.

Code Explanation
--------------

There are [several pages](https://github.com/baldurk/renderdoc/wiki/Code-Dives) on the wiki explaining different aspects of how the code fits together - like how the capture-side works vs replay-side, how shader debugging works, etc.

    renderdoc/ 
        Makefile                        ; The linux make file, will recurse into subdirectories to build them
        renderdoc.sln                   ; VS2010 solution for windows building
        renderdoc/
            3rdparty/                   ; third party utilities & libraries included
            drivers/                    ; API-specific back-ends, can be individually skipped/removed
            ...                         ; everything else in here consists of the core renderdoc runtime
        renderdoccmd/                   ; A small C++ utility program that runs to do various little tasks
        renderdocshim/                  ; A tiny C DLL using only kernel32.dll that is used for global hooking
        renderdocui/                    ; The .NET UI layer built on top of renderdoc/
            3rdparty/                   ; third party utilities & libraries included
        qrenderdoc/                     ; The Qt UI layer built on top of renderdoc/
        pdblocate/                      ; a simple stub program to invoke DIA to look up symbols/pdbs
                                        ; for callstack resolution on windows
        docs/                           ; source documentation for the .chm file or http://docs.renderdoc.org/
                                        ; in the Sandcastle help file builder
        installer/                      ; installer scripts for WiX Toolset
        dist.sh                         ; a little script that will build into dist/ with everything necessary
                                        ; to distribute a build - assumes that exes etc are already built

Testing
--------------

At the moment the testing of any features and changes is pretty much ad-hoc. I've been working on a proper test suite that will test both API capture/replay support as well as the analysis features.

Until then, test any changes you make around the area that you've tested - if I have any particular suggestions on testing I will probably bring it up in the pull request.
