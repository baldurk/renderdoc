Python development environment
==============================

This document outlines how to set up a development environment with suitable IDE type information for the RenderDoc modules. It's entirely optional, and you can write your python code in whichever way makes most sense for you. We will be configuring the PyCharm IDE to have autocomplete for all RenderDoc types.

Building RenderDoc
------------------

The first step is to build RenderDoc from source to ensure you have a matching python module and RenderDoc build. Due to binary incompatibilities the python module can't be widely distributed since it is linked against a specific python minor version.

Build instructions for your platform are available `on github <https://github.com/baldurk/renderdoc>`_. Take note of the python version that you build against, since this will need to be used later.

.. note::
  On windows by default RenderDoc builds against python 3.6 which is what it's distributed with.
  
  This can be overridden by setting an overridden path under the ``Python Configuration`` section in the properties of the ``qrenderdoc`` project and ``pyrenderdoc_module`` project. It must point to a python installation.
  
  RenderDoc requires pythonXY.lib, include files such as include/Python.h, as well as a .zip of the standard library. If you installed python with an installer you have the first two, and can generate the standard library zip by zipping the contents of the Lib folder. If you downloaded the embeddable zip distribution you will only have the standard library zip, you need to obtain the include files and ``.lib`` file separately.

Once you have compiled RenderDoc copy the python module into the same folder as the main renderdoc library. On windows this means copying out of the ``pymodules`` subdirectory, on linux this will likely be the case already. We do this to keep things simple, so the python module can load the library without needing to change ``PATH``.

Python Setup for VS Code
------------------------
Using the same python version as the RenderDoc build used (Python 3.6 by default).
From inside the `docs` folder run the following command:
`python3 regenerate_stubs.py <path_to_folder_to_store_render_python_symbols>``
After running the command the output folder `<path_to_folder_to_store_render_python_symbols>` should contain `renderdoc` and `qrenderdoc` folders.

In `VS Code` change the setting `python.analysis.extraPaths`` and add the output path used in the previous command (`<path_to_folder_to_store_render_python_symbols>`).

Now when viewing a python script in `VS Code`, you should see the `RenderDoc` types being correctly resolved and autocompleted.

If you get a warning about failure to import module `renderdoc` then something is not correct in the setup, double check the `VS Code` setting `python.analysis.extraPaths` includes the `RenderDoc` python API parsed output folder (which should contain two folders `renderdoc` and `qrenderdoc`).

Configuring python module for PyCharm
-------------------------------------

Python Setup for PyCharm
------------------------

Now install PyCharm, for this document we will install 2020.3.2. Any version is fine, though newer versions may require modification to work properly.

Before you run PyCharm, we will replace one file in it to generate better type information for RenderDoc. In the RenderDoc repository there's a `pycharm_helpers folder <https://github.com/baldurk/renderdoc/tree/v1.x/docs/pycharm_helpers>`_. Copying the content of the plugins folder over the folder in your PyCharm installation will update the file that is customised. You can back it up beforehand at this path: ``plugins/python-ce/helpers/generator3/module_redeclarator.py``.

If you're using a different version of PyCharm you can try to apply the patch also available in that folder.

Configuring python module for PyCharm
-------------------------------------

You can now launch PyCharm and open or create the python project where you'll be writing code. Now we'll configure the python interpreter. This must match the python version that you built against above - the same major and minor version, and the same bitness (32-bit to 32-bit or 64-bit to 64-bit).

Go into :guilabel:`File` enter :guilabel:`Settings`. On the left you can go into the project and to :guilabel:`Python Interpreter`.

In the first entry click the gear next to :guilabel:`Python Interpreter` and choose :guilabel:`Add` if the interpreter you want isn't available. You can now configure a System Interpreter with the correct version as above.

Once you've chosen the correct interpreter we'll also tell it where to find the RenderDoc libraries since they won't be in the default python path. Go back to the gear wheel but this time select :guilabel:`Show All`. With your chosen interpreter selected click on the tree icon at the bottom labeled ``Show paths for the selected interpreter`` and add the directory where you have the ``renderdoc`` and ``qrenderdoc`` modules, as well as the ``renderdoc`` library.

If everything went well, PyCharm should load that interpreter for the project and discover the renderdoc python modules. It will then generate stubs for them with correct typing information so you can benefit from proper autocomplete while writing python code.

Troubleshooting PyCharm
-----------------------

If you get an error about "No module named 'renderdoc'" then something has gone wrong with how the interpreter finds and loads the python module. Ensure you have the right path specified and that the interpreter is the correctly matching version for the python module you compiled.

To regenerate the generated python stubs delete your ``python_stubs`` folder in the JetBrains local cache. On windows this is in ``%LOCALAPPDATA%/JetBrains``.
