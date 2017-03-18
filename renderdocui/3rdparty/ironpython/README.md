This is a distribution of [IronPython](http://ironpython.net/) 2.7.4, license is available in LICENSE.md.

In this folder is compilelibs.sh which will rompress the Libs/ python standard library into a zip for distribution.

Run compilelibs.sh and point it at an IronPython checkout and it will copy pythonlibs.zip to this folder, which will be copied by the packaging scripts next to renderdocui.exe to provide the python standard library in-program.
