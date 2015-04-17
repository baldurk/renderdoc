This is a distribution of [IronPython](http://ironpython.net/) 2.7.4, license is available in LICENSE.md.

In this folder is compilelibs.sh which will build a selection of the Libs/ python standard library into a single dll for distribution. The dll isn't included since it ends up being ~10MB so too large to be nice to commit to git.

Run compilelibs.sh and point it at an IronPython checkout and it will copy pythonlibs.dll to this folder, which will be optionally loaded in code (and expected by the packaging scripts) to provide the python standard library in-program.
