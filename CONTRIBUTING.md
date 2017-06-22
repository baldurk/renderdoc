# Contributing to RenderDoc
----

If you're interested in contributing to RenderDoc, this is the place!

For small changes like one-line fixes or minor tweaks then don't worry about reading this end-to-end. The point of this isn't to be anal about rules, so I'm happy to help to be accommodating and go back and forth to get any change ready to commit, or fix up minor issues by hand. It's a good idea to check the [commit message](#commit-messages) and [code formatting](#code-formatting) sections though.

On the other hand regular contributors or if you have a larger amount of code that's changing, please read on as it will make life easier for everyone if you to follow along with these guidelines.

1. [Compiling](#compiling)
2. [Code of Conduct](#code-of-conduct)
3. [Preparing commits](#preparing-commits)
4. [Developing a change](#developing-a-change)
5. [Copyright / Contributor License Agreement](#copyright--contributor-license-agreement)
6. [Dependencies](#dependencies)
7. [Where to Start](#where-to-start)
8. [Code Explanation](#code-explanation)
9. [Testing](#testing)

# Compiling

### Windows

The main [renderdoc.sln](renderdoc.sln) is a VS2015 solution, as of March 2017. It should also compile in VS2017, just select to update the compilers if you don't have the 2015 compilers available.

There are no external dependencies, all libraries/headers needed to build are included in the git checkout. On windows, the `Development` configuration is recommended for day-to-day dev. It's debuggable but not too slow. The `Release` configuration is then obviously what you should compile for any builds you'll send out to people or if you want to evaluate performance.

### Linux

Currently linux should work with gcc 5+ and clang 3.4+ as it requires C++14 compiler support. The Travis CI builds with gcc-6.0 and clang-3.5. Within reason other compilers will be supported if the required patches are minimal. Distribution packages should be built with the `Release` CMake build type so that warnings do not trigger errors. To build just run:

```
cmake -DCMAKE_BUILD_TYPE=Debug -Bbuild -H.
make -C build
```

Configuration is available for cmake, [documented elsewhere](https://cmake.org/documentation/). You can override the compiler with environment variables `CC` and `CXX`, and there are some options you can toggle in the root CMakeLists files such as `cmake -DENABLE_GL=OFF`.

### Mac

Mac support is pretty early and while it will compile, it's not usable for debugging yet. Builds happen with cmake the same way as Linux.

# Code of Conduct

I want to ensure that anyone can contribute to RenderDoc with only the next bug to worry about. For that reason the project has adopted the [contributor covenent](CODE_OF_CONDUCT.md) as a code of conduct to be enforced for anyone taking part in RenderDoc development. If you have any queries in this regard you can get in touch with me [directly over email](mailto:baldurk@baldurk.org).

# Preparing commits

### Commit Messages

Ensure commit messages have a single title of at most 72 characters, followed by a body if necessary:

```
Short summary text, maximum of 72 characters. That's out to here ---->X

Longer paragraph that details the change and any logic behind needing
to make the change, other impacts, future work etc. Keeping the title
to 72 characters means it will display fully in git log and github logs.
```

Merge commits should not be included in pull requests as they just muddy the history, please rebase when bringing code up to date against latest master. Likewise commits for code formatting or compile fixes should be squashed into the relevant commits that they update, rather than left in the history.

### Code formatting

To make things easier for everyone, I've adopted clang-format for keeping code consistently formatted. Since clang-format can change its output depending on version number even with the same configuration options, I have fixed the version used for RenderDoc at [clang-format-3.8](http://llvm.org/releases/3.8.0/tools/clang/docs/ClangFormatStyleOptions.html). This formatting is enforced by CI checks that run on PRs, so if you aren't running the same version locally it will show up there.

There are instructions on how to set up git hooks or IDE integration [on the wiki](https://github.com/baldurk/renderdoc/wiki/Code-formatting-(using-clang-format)).

Do not make any intermediate commits which don't follow the formatting conventions. Having several intermediate commits with mismatched formatting then a single 'reformatted code' commit at the end makes history and blames harder to read, which is an important tool for others to understand your code. It is much easier to enforce proper formatting on each commit as you go along, than to try and rebase and merge formatting changes in after the fact.

Since it's not covered by a pure formatting check, don't use overly modern C++ unnecessarily. Although the minimum compiler spec is now higher than it was in the past (as of March 2017) and modern features may be supported, some modern C++ constructs do not fit with the style of the rest of the code.

### Branch history

Different people have different preferences on how history should be organised. Some people prefer to commit often and then squash down changes into a single commit after they've finished working. Others prefer to combine commits intermittently when they reach a logical boundary and or next step. Some like to avoid committing until they have a 'final' commit that they are ready to make.

I'm flexible about whether branches are squashed or expanded when they are put up for PR. The history should be reasonable though; if you have a change that modifies a dozen files and changes hundreds of lines of code then it should not be squashed into a single commit. Likewise, intermittent changes where something was tried and then reverted, or minor fixes and tweaks as you go should be rebased and squashed together to form a more coherent whole. As a rule of thumb, try to keep your commit messages describing what they do roughly, even if it is a minor change. Commit messages like "fix stuff" or "compile fix" or any message with "WIP", "amend", "temp", etc should probably not remain in the final PR.

With fixes from a code review it's up to you whether you keep changes as a separate commit or squash them in. With the exception of formatting and compile fix commits, which as noted before should be squashed into the relevant commits.

For overall scope of a change/branch read the [developing a change](#developing-a-change) section.

# Developing a change

If you're making a tiny bugfix that only changes a few lines, then you don't really have to worry about how you structure your development or organise branches. You can just make your fix, open up a pull request, and everything can be handled from there on the fly.

When making a larger change though, there are more things to take into consideration. Your changes need to be compatible with the project on a larger scale, as well as making sure your development process can merge into the mainline development with other contributors and the project maintainer.

There are a few guidelines to follow to make sure that everyone can work together with as little friction as possible:

Be proactive about communication. You can always [email me](mailto:baldurk@baldurk.org) about anything RenderDoc related including work you are planning, or currently doing. You can also open an issue to discuss a change. Staying in communication particularly with me can head off problems at a much earlier stage - perhaps a design you were planning would conflict with the direction of the project or with a better idea of the whole picture I can suggest something that would be more appropriate. It's much better to have a conversation and avoid spending time doing work that will be rejected or require rewrites at PR stage.

Aim to merge your work to the main line in reasonably sized chunks. How big is a 'reasonably sized' chunk is debateable, but bear in mind that your code must be able to be reviewed. If in doubt you can always split the work into a smaller standalone chunk, but keeping any one PR under 1000 lines changed at the very most is a good mental limit. Keeping a large change on a branch means that you have to do more merges from the mainline to keep up to date, and increases the chance that your changes will diverge away from the project. The [LLVM developer policy](http://llvm.org/docs/DeveloperPolicy.html#incremental-development) describes this kind of workflow and its benefits much better than I can.

Have a clear idea of what your change is to do. This goes hand in hand with the above, but if your change involves a lot of work then it's better to split it up into smaller components that can be developed and merged individually, towards the larger goal. Doing this makes it more easily digestible for the rest of the people on the project as well as making it easier to review the changes when they land.

It's fine to land features one-by-one in different drivers. Historically there have been features that only worked on certain APIs so don't feel that you must implement any new feature on all APIs. At the same time, feature parity is a goal of the project so you should aim to implement features that can be later ported to other APIs where possible either by yourself or by others.

# Copyright / Contributor License Agreement

Any code you submit will become part of the repository and be distributed under the [RenderDoc license](LICENSE.md). By submitting code to the project you agree that the code is your own work and that you have the ability to give it to the project.

You also agree by submitting your code that you grant all transferrable rights to the code to the project maintainer, including for example re-licensing the code, modifying the code, distributing in source or binary forms. Specifically this includes a requirement that you assign copyright to the project maintainer (Baldur Karlsson). For this reason, do not modify any copyright statements in files in any PRs.

# Dependencies

### Windows

On Windows there are no dependencies - you can always compile the latest version just by downloading the code and compiling the solution in Visual Studio. If you want to modify the Qt UI you will need a version of Qt installed - at least version 5.6.

### Linux

Requirements for the core library and renderdoccmd are libx11, libxcb, libxcb-keysyms and libGL. The exact are packages for these vary by distribution.

For qrenderdoc you need Qt5 >= 5.6 along with the 'svg' and 'x11extras' packages. You also need python3-dev for the python integration, and bison, autoconf, automake and libpcre3-dev for building the custom SWIG tool for generating bindings.

This is the apt-get line you'd need to install the requirements bar Qt on Ubuntu 14.04 or above:

```
sudo apt-get install libx11-dev libx11-xcb-dev mesa-common-dev libgl1-mesa-dev libxcb-keysyms1-dev cmake python3-dev bison autoconf automake libpcre3-dev
```

Your version of Ubuntu might not include a recent enough Qt version, so you can use [Stephan Binner's ppas](https://launchpad.net/~beineri) to install a more recent version of Qt. At least 5.6.2 is required.

For Archlinux (as of 2017.04.18) you'll need:

```
sudo pacman -S libx11 libxcb xcb-util-keysyms mesa libgl qt5-base qt5-svg qt5-x11extras cmake python3 bison autoconf automake pcre
```

For Gentoo (as of 2017.04.18), you'll need:

```
sudo emerge --ask x11-libs/libX11 x11-libs/libxcb x11-libs/xcb-util-keysyms dev-util/cmake dev-qt/qtcore dev-qt/qtgui dev-qt/qtwidgets dev-qt/qtsvg dev-qt/qtx11extras sys-devel/bison sys-devel/autoconf sys-devel/automake dev-lang/python dev-libs/libpcre
```

Checking that at least Qt 5.6 installs.

On any distribution if you find qmake isn't available under its default name, or if `qmake -v` lists a Qt4 version, make sure you have qtchooser installed in your package manager and use it to select Qt5. This might be done by exporting `QT_SELECT=qt5`, but check with your distribution for details.

If you know the required packages for another distribution, please share (or pull request this file!)

### Mac

Mac requires a recent version of CMake, and the same Qt version as the other platforms (at least 5.6.2). If you're using [homebrew](http://brew.sh) then this will do the trick:

```
brew install cmake qt5
brew link qt5 --force
```

# Where to Start

There are always plenty of things to do, if you'd like to chip in! Check out the [Roadmap](https://github.com/baldurk/renderdoc/wiki/Roadmap) page in the wiki for future tasks to tackle, or have a look at the [issues](https://github.com/baldurk/renderdoc/issues) for outstanding bugs. I'll try and tag things that seem like small changes that would be a good way for someone to get started with.

If you have a change you'd like to see make it into mainline, create a fork of renderdoc, make your changes to a branch, and open a pull request on github. You can look around for instructions on that - it's pretty simple.

# Code Explanation

There are [several pages](https://github.com/baldurk/renderdoc/wiki/Code-Dives) on the wiki explaining different aspects of how the code fits together - like how the capture-side works vs replay-side, how shader debugging works, etc.

    renderdoc/ 
        CMakeLists.txt                  ; The cmake file, will recurse into subdirectories to build them
        renderdoc.sln                   ; VS2015 solution for windows building
        renderdoc/
            3rdparty/                   ; third party utilities & libraries included
            drivers/                    ; API-specific back-ends, can be individually skipped/removed
            ...                         ; everything else in here consists of the core renderdoc runtime
        renderdoccmd/                   ; A small C++ utility program that runs to do various little tasks
        renderdocshim/                  ; A tiny C DLL using only kernel32.dll that is used for global hooking
        renderdocui/                    ; The .NET UI layer built on top of renderdoc/
            3rdparty/                   ; third party utilities & libraries included
        qrenderdoc/                     ; The Qt UI layer built on top of renderdoc/
        docs/                           ; source documentation for the .chm file or http://docs.renderdoc.org/
                                        ; in the Sandcastle help file builder
        scripts/                        ; folder for small scripts - e.g. for CI, installers, distribution

# Testing

At the moment the testing of any features and changes is pretty much ad-hoc. I've been working on a proper test suite that will test both API capture/replay support as well as the analysis features.

Until then, test any changes you make around the area that you've tested - if I have any particular suggestions on testing I will probably bring it up in the pull request.
