RenderDoc
==============

Welcome to RenderDoc - a graphics debugger, currently available for D3D11 development on windows. 

Quick Links:

* **Homepage**: http://cryengine.com/renderdoc
* **Forums**: http://crydev.net/renderdoc
* **Documentation**: renderdoc.chm in the build, or http://docs.renderdoc.org/
* **Tutorials**: There are some video tutorials on Youtube: http://www.youtube.com/user/baldurkarlsson/
* **Email contact**: renderdoc@crytek.com
* **IRC channel**: #renderdoc on freenode
* **Roadmap/future development**: Roadmap on the wiki

Downloads
--------------

There are binary releases available, built from the release targets. If you just want to use the program and you ended up here, this is what you want :).

* http://cryengine.com/renderdoc - Official stable installers
* http://renderdoc.org/autobuild - Automated builds from the master branch, as just zipped folders

It's recommended that if you're new you start with the latest official stable installer, and only use the automated builds if you want the bleeding edge, or you want a specific bugfix.

Documentation & How to use
--------------

The documentation is included in the builds, and also available here: http://docs.renderdoc.org/. There are some video tutorials on Youtube: http://www.youtube.com/user/baldurkarlsson/

License
--------------

RenderDoc is released under the MIT license, see LICENSE.md for full text as well as 3rd party library acknowledgements.

Contributing & Development
--------------

There are always plenty of things to do, if you'd like to chip in! Check out the roadmap page in the wiki for future tasks to tackle, or have a look at the issues for outstanding bugs. I'll try and tag things that seem like small changes that would be a good way for someone to get started with.

The **master** branch is kept relatively stable - ie. it should always build and run but might contain new bugs compared to a stable release. There are auto-builds that come off this branch. If you're working on any changes you should branch from **master** and submit pull requests against **master**.

The **dev** branch is where I work and it is liable to not compile, be horribly broken, have its history rewritten as I rebase against **master**, etc. The reason for having this branch is mostly so that what I'm doing is visible if people are interested and so I have somewhere to push to, to sync between work and home etc.

If you're tackling anything large then please contact me and post an issue so that everyone knows you're working on it and there's not duplicated effort. *Specifically* if you want to extend RenderDoc to a platform or API that it doesn't already support please get in touch, as there might already be code that isn't committed yet. Particularly if this is not a public API that anyone can write against.

Official releases will get a branch made from **master** for them, autobuilds are just marked with the hash of the commit they were built from.

Please don't distribute releases marked with a version number as it will confuse me with auto-submitted crashes since I won't have the symbols for them. If you distribute releases leave the version information as is (master is always marked as an unofficial non-versioned build).

Building
--------------

Building RenderDoc is fairly straight forward.

### Windows ###

The main solution is in the root folder. renderdoc.sln is a VS2010 solution, there may be other versions as side .sln files next to it.

The only external dependency should be the Windows 8.1 SDK. The 8.0 SDK should also work fine, but the vcxproj is set up to look in $(ProgramFiles)\Windows Kits\8.1\ for the necessary paths. If your SDK is installed elsewhere you'll also need to change these locally.

Profile is recommended for day-to-day dev. It's debuggable but not too slow. Release is obviously what you should build for any builds you'll send out to people or if you want to evaluate performance.

### Linux ###

Just 'make' in the root should do the trick. This build system is work in progress as the linux port is very early, so it may change!

Code Explanation
--------------

There are several pages on the wiki explaining different aspects of how the code fits together - like how the capture-side works vs replay-side, how shader debugging works, etc.

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

Tips
--------------

### Visual Studio Visualisers ###

You might find these visualisers useful, going under your [Visualizer] section in autoexp.dat:

    rdctype::wstr { preview([$e.elems,su]) stringview([$e.elems,su]) }
    rdctype::str { preview([$e.elems,s]) stringview([$e.elems,s]) }

    rdctype::array<*> {
        preview ( #( "[",$e.count,"] {", #array(expr: $e.elems[$i], size: $e.count), "}") )
        children ( #( #([size] : $e.count), #array(expr: $e.elems[$i], size: $e.count) ) )
    }

