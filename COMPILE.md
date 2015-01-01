Windows
--------------

The main [renderdoc.sln](renderdoc.sln) is a VS2010 solution. To build on later VS versions, simply open & upgrade, I've tested building on VS2012 and VS2013 without issues. It compiles fine in the [free VS2013 version](http://www.visualstudio.com/en-us/news/vs2013-community-vs.aspx).

The only external dependency should be the [Windows 8.1 SDK](http://msdn.microsoft.com/en-us/windows/desktop/bg162891.aspx). The 8.0 SDK should also work fine, but [the vcxproj](renderdoc/renderdoc.vcxproj) is set up to look in `$(ProgramFiles)\Windows Kits\8.1\` for the necessary paths. If your SDK is installed elsewhere you'll also need to change these locally. You can also compile only against the June 2010 DirectX SDK if you undefine `INCLUDE_D3D_11_1` in `d3d11_common.h`.

Profile configuration is recommended for day-to-day dev. It's debuggable but not too slow. Release configuration is then obviously what you should build for any builds you'll send out to people or if you want to evaluate performance.

### Visual Studio Visualisers ###

You might find these visualisers useful, going under your [Visualizer] section in autoexp.dat:

    rdctype::str { preview([$e.elems,s]) stringview([$e.elems,s]) }

    rdctype::array<*> {
        preview ( #( "[",$e.count,"] {", #array(expr: $e.elems[$i], size: $e.count), "}") )
        children ( #( #([size] : $e.count), #array(expr: $e.elems[$i], size: $e.count) ) )
    }

Linux
--------------

Just 'make' in the root should do the trick. This build system is work in progress as the linux port is very early, so it may change!

There's no configuration or cmake setup, it assumes gcc/g++ (this can be overwridden via variables CC and CPP, or just in the makefiles), and links against -lX11 and -lGL.

Contributing & Development
--------------

There are always plenty of things to do, if you'd like to chip in! Check out the [Roadmap](https://github.com/baldurk/renderdoc/wiki/Roadmap) page in the wiki for future tasks to tackle, or have a look at the [issues](https://github.com/baldurk/renderdoc/issues) for outstanding bugs. I'll try and tag things that seem like small changes that would be a good way for someone to get started with.

If you have a change you'd like to see make it into mainline, just open a pull request on github. We can discuss changes if there need to be any, then merge it in. Please make sure your changes are fully rebased against master when you create the pull request.

If you're tackling anything large then please contact me and post an issue so that everyone knows you're working on it and there's not duplicated effort. *Specifically* if you want to extend RenderDoc to a platform or API that it doesn't already support please get in touch, as there might already be code that isn't committed yet. Particularly if this is not a public API that anyone can write against.

Official releases will get a github release made for them, nightly builds are just marked with the hash of the commit they were built from.

Please don't distribute releases marked with a version number and commit hash as it will confuse me with auto-submitted crashes since I won't have the symbols for them. If you distribute releases leave the version information as is (master is always marked as an unofficial non-versioned build).

Code Explanation
--------------

There are [several pages](https://github.com/baldurk/renderdoc/wiki/Code-Dives) on the wiki explaining different aspects of how the code fits together - like how the capture-side works vs replay-side, how shader debugging works, etc.

    renderdoc/ 
        dist.sh                         ; a little script that will build into dist/ with everything necessary
                                        ; to distribute a build - assumes that exes etc are already built
        renderdoc/
            3rdparty/                   ; third party utilities & libraries included
            ...                         ; everything else in here consists of the core renderdoc runtime
        renderdoccmd/                   ; A small C++ utility program that runs to do various little tasks
        renderdocui/                    ; The .NET UI layer built on top of renderdoc/
        pdblocate/                      ; a simple stub program to invoke DIA to look up symbols/pdbs
                                        ; for callstack resolution on windows
        docs/                           ; source documentation for the .chm file or http://docs.renderdoc.org/
                                        ; in the Sandcastle help file builder
        installer/                      ; installer scripts for WiX Toolset
        ScintillaNET/                   ; .NET component for using Scintilla in the shader viewers
        WinFormsUI/                     ; dockpanelsuite for docking UI
        breakpad/                       ; parts of google breakpad necessary for crash reporting system

