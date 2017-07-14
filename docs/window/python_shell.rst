Python Shell
============

The python shell allows a limited form of scripting for RenderDoc, including the ability to run simple scripts over the whole dataset in ways not easy to do in the UI.

Overview
--------

You can open the python shell from the window menu. It offers both an interactive shell and a window that can open and run scripts and display the output.

Currently the support is fairly bare bones, and typically you will need some understanding of the code to use the scripting support well. The :code:`pyrenderdoc` object corresponds to the :code:`Core` object type used throughout the UI, and the :code:`renderdoc` python module corresponds to the :code:`renderdoc` namespace in C# and it provides a jumping off point for most operations from there.

So while not particularly friendly to discovery, you can use this to perform an automated iteration over e.g. all events or all textures for something of interest.

