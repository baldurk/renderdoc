ToolWindowManager
=================

ToolWindowManager is a Qt based tool window manager. This allows Qt projects to use docking functionality similar to QDockWidget, but since it's a separate component it's easier to customise and extend and so has a number of improvements over the built-in docking system.

This is a fork from https://github.com/riateche/toolwindowmanager, specifically from [5244be3f9ac680ac568a6eff8156a520ce08ecf1](https://github.com/baldurk/toolwindowmanager/tree/original_impl) where the license is clearly MIT. After that point there was a re-implementation that may have relicensed under LGPL and the author has not clarified.

Also this fork contains a number of changes and improvements to make it useful for RenderDoc and perhaps other projects. Notable highlights:

* Additional customisability like arbitrary data tagged with saved states, callbacks to check before closing, and allowing/disallowing tab reorder or float windows
* Fixes for having multiple nested TWMs
* Render a preview overlay for drop locations
* Use hotspots icons and specific locations to determine drop sites, not the old 'cycle through suggestions' method
* Allow dragging/dropping whole floating windows together

The original README.md [can be found here](https://github.com/baldurk/toolwindowmanager/blob/original_impl/README.md)

Screenshots
===========

Windows:

![Windows](docs/windows.png)


Linux (Ubuntu 17.04 Unity);

![Ubuntu 17.04 Unity](docs/unity.png)

Linux (Ubuntu 17.04 KDE Plasma):

![Ubuntu 17.04 KDE Plasma](docs/plasma.png)

OS X:

![OS X](docs/osx.png)
