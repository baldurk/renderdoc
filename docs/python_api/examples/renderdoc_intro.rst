Getting Started (python)
========================

.. note::

  This document is aimed at users getting started with loading a capture and getting access from the renderdoc module, and is generally not relevant when running within the RenderDoc UI.

  The same APIs are available in the UI, so you can follow these steps. Be aware that loading captures while purely from script may interfere with a capture that is loaded in the UI itself, so this is not recommended.

Loading the Module
------------------

For this section we assume you have built a copy of RenderDoc and have the module (``renderdoc.pyd`` or ``renderdoc.so`` depending on your platform). For information on how to build see the `GitHub repository <https://github.com/baldurk/renderdoc>`_.

.. note::

  You must use exactly the same version of python to load the module as was used to build it.

  On windows by default RenderDoc builds against python 3.6 which is what it's distributed with.
  
  This can be overridden by setting an overridden path under the ``Python Configuration`` section in the properties of the ``qrenderdoc`` project and ``pyrenderdoc_module`` project. It must point to a python installation.
  
  RenderDoc requires pythonXY.lib, include files such as include/Python.h, as well as a .zip of the standard library. If you installed python with an installer you have the first two, and can generate the standard library zip by zipping the contents of the Lib folder. If you downloaded the embeddable zip distribution you will only have the standard library zip, you need to obtain the include files and ``.lib`` file separately.

Once you have the module, either place the module within your python's default library search path, or else insert the location of the python module into the path in your script. You can either set the ``PYTHONPATH`` environment variable or do it at the start of your script:

.. highlight:: python
.. code:: python

    import sys

    sys.path.append('/path/to/renderdoc/module')

Additionally, the renderdoc python module needs to be able to load the main renderdoc library - the module library it self just contains stubs and python wrappers for the C++ interfaces. You can either place the renderdoc library in the system library paths, or solve it in a platform specific way. For example on windows you can either place ``renderdoc.dll`` in the same directory as the python module, or append to ``PATH``. On Python 3.8 and above ``PATH`` is no longer searched by default so you need to explicitly add the DLL folder:

.. highlight:: python
.. code:: python

    import os, sys

    os.environ["PATH"] += os.pathsep + os.path.abspath('/path/to/renderdoc/native/library')
    if sys.platform == 'win32' and sys.version_info[1] >= 8:
        os.add_dll_directory("/path/to/renderdoc/native/library")

On linux you'd perform a similar modification to ``LD_LIBRARY_PATH``.

Assuming all has gone well, you should now be able to import the renderdoc module:

.. highlight:: python
.. code:: python

    import renderdoc as rd

    # Prints 'CullMode.FrontAndBack'
    print(rd.CullMode.FrontAndBack)

Loading a Capture
-----------------

Given a capture file ``test.rdc`` we want to load it, begin the replay and get ready to perform analysis on it.

Before doing anything, we must initialise the replay API. We do that by calling :py:meth:`~renderdoc.InitialiseReplay`. Generally no special configuration is needed so passing a default :py:class:`~renderdoc.GlobalEnvironment` and an empty list of arguments is fine.


.. highlight:: python
.. code:: python

    rd.InitialiseReplay(rd.GlobalEnvironment(), [])

To begin with, we use :py:meth:`~renderdoc.OpenCaptureFile` to obtain a :py:class:`~renderdoc.CaptureFile` instance. This gives us access to control over a capture file at a meta level. For more information see the :py:class:`CaptureFile` reference - the interface can also be used to create.

To open a file, use :py:meth:`~renderdoc.CaptureFile.OpenFile` on the :py:class:`~renderdoc.CaptureFile` instance. This function allows conversion from other formats via an importer, but here we'll use it just for opening a regular ``rdc`` file. It returns a :py:class:`~renderdoc.ResultDetails` which can be used to determine what went wrong in the event that there was a problem. We then check that the capture uses an API which can be replayed locally - for example not every platform supports ``D3D11``, so on linux this would return no local replay support.

.. highlight:: python
.. code:: python

    # Open a capture file handle
    cap = rd.OpenCaptureFile()

    # Open a particular file - see also OpenBuffer to load from memory
    result = cap.OpenFile('test.rdc', '', None)

    # Make sure the file opened successfully
    if result != rd.ResultCode.Succeeded:
        raise RuntimeError("Couldn't open file: " + str(result))

    # Make sure we can replay
    if not cap.LocalReplaySupport():
        raise RuntimeError("Capture cannot be replayed")

Accessing Capture Analysis
--------------------------

Once the capture has been loaded, we can now begin the replay analysis. To do that we use :py:meth:`~renderdoc.CaptureFile.OpenCapture` which returns a tuple of :py:class:`~renderdoc.ResultDetails` and :py:class:`~renderdoc.ReplayController`.

This function call will open the capture and begin to replay it, and initialise the analysis. The :py:class:`~renderdoc.ReplayController` returned is the interface to the majority of RenderDoc's replaying functionality.

.. highlight:: python
.. code:: python

    # Initialise the replay
    result,controller = cap.OpenCapture(rd.ReplayOptions(), None)

    if result != rd.ResultCode.Succeeded:
        raise RuntimeError("Couldn't initialise replay: " + str(result))

    # Now we can use the controller!
    print("%d top-level actions" % len(controller.GetRootActions()))

Once we're done with the interfaces, we should call the ``Shutdown`` function on each, this allows the C++ interface to release the resources allocated.

Once all work is done we can shutdown the replay API.

.. highlight:: python
.. code:: python

    # Shutdown the controller first, then the capture file
    controller.Shutdown()

    cap.Shutdown()

    rd.ShutdownReplay()

Example Source
--------------

The full source for this example is available below:

.. only:: html and not htmlhelp

    :download:`Download the example script <renderdoc_intro.py>`.

.. literalinclude:: renderdoc_intro.py
