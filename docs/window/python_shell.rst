Python Shell
============

The python shell allows a limited form of scripting for RenderDoc, including the ability to run simple scripts over the whole dataset in ways not easy to do in the UI.

Overview
--------

You can open the python shell from the window menu. It offers both an interactive shell and a window that can open and run scripts and display the output.

Currently the support is fairly bare bones, and typically you will need some understanding of the code to use the scripting support well. The 'renderdoc' object corresponds to the Core object type used throughout the UI, and it provides a jumping off point for most operations from there.

Examples are:

* :code:`renderdoc.CurTextures` is an array of FetchTexture with all textures in the current log.
* :code:`renderdoc.GetTextureViewer()` gives a handle to the texture viewer, allowing you to view a particular texture.
* :code:`renderdoc.AppWindow.LoadLogfile("filename", False)` opens a given log.
* :code:`renderdoc.GetDrawcall(0, 151)` will return the drawcall of EID 151. Drawcalls have next/previous/parent/children properties to allow you to step through the frame.
* :code:`renderdoc.SetEventID(None, 0, 151)` browses to EID 151.


So while not particularly friendly to discovery, you can use this to perform an automated iteration over e.g. all events or all textures for something of interest.

.. figure:: ../imgs/Screenshots/pythonshell.png

	Python Shell: A simple script that looks at textures and their use in a log.
