Windows
--------------

The main [renderdoc.sln](renderdoc.sln) is a VS2010 solution. To build on later VS versions, simply open & upgrade, I've tested building on VS2012, VS2013 and VS2015 without issues. It compiles fine in the [free VS2015 version](https://www.visualstudio.com/en-us/downloads/download-visual-studio-vs.aspx), just select to update the compilers.

There should be no external dependencies, all libraries/headers needed to build are included in the git checkout.

Development configuration is recommended for day-to-day dev. It's debuggable but not too slow. Release configuration is then obviously what you should build for any builds you'll send out to people or if you want to evaluate performance.

### Visual Studio Visualisers ###

You might find these visualisers useful, going under your [Visualizer] section in autoexp.dat:

    rdctype::str { preview([$e.elems,s]) stringview([$e.elems,s]) }

    rdctype::array<*> {
        preview ( #( "[",$e.count,"] {", #array(expr: $e.elems[$i], size: $e.count), "}") )
        children ( #( #([size] : $e.count), #array(expr: $e.elems[$i], size: $e.count) ) )
    }

Linux
--------------

Currently linux supports gcc-4.8 and clang-3.5, as these are the compilers used in CI builds. Once the linux port is more mature, more compilers can be supported.

Just 'make' in the root should do the trick. This build system is work in progress as the linux port is very early, so it may change! Currently it uses cmake, running the default Makefile will run cmake inside a 'build' folder then build from there.

Configuration is as usual for cmake, you can override the compiler with environment variables CC and CXX, and there are some options you can toggle in the root CMakeLists files.

Requirements are linking against -lX11 and -lGL. For qrenderdoc you need qt5 along with the 'x11extras' package.

This is the apt-get line you'd need to install the requirements on Ubuntu 14.04:

```
sudo apt-get install libx11-dev libx11-xcb-dev mesa-common-dev libgl1-mesa-dev qt5-default libqt5x11extras5-dev libxcb-keysyms1-dev cmake
```

For Archlinux (as of 2016.08.04) you'll need:

```
sudo pacman -S libx11 libx11-xcb xcb-util-keysyms mesa mesa-libgl qt5-base qt5-x11extras cmake
```

If you know the required packages for another distribution, please share (or pull request this file!)

