Getting Started (python)
========================

.. note::

  This document is aimed at users getting started with loading a capture and getting access from the renderdoc module, and is generally not relevant when running within the RenderDoc UI.

  The same APIs are available in the UI, so you can follow these steps. Be aware that loading captures while purely from script may interfere with a capture that is loaded in the UI itself, so this is not recommended.

Loading the Module
------------------

For this section we assume you have built a copy of RenderDoc and have the module (``renderdoc.pyd`` or ``renderdoc.so`` depending on your platform). For information on how to build see the `GitHub repository <https://github.com/baldurk/renderdoc>`_.

Once you have the module, either place the module within your python's default library search path, or else insert the location of the python module into the path in your script. You can either set the ``PYTHONPATH`` environment variable or do it at the start of your script:

.. highlight:: python
.. code:: python

    import sys

    sys.path.append('/path/to/renderdoc/module')

Additionally, the renderdoc python module needs to be able to load the main renderdoc library - the module library it self just contains stubs and python wrappers for the C++ interfaces. You can either place the renderdoc library in the system library paths, or solve it in a platform specific way. For example on windows you can either place ``renderdoc.dll`` in the same directory as the python module, or append to ``PATH``:

.. highlight:: python
.. code:: python

    import os

    os.environ["PATH"] += os.pathsep + os.path.abspath('/path/to/renderdoc/native/library')

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

To begin with, we use :py:meth:`~renderdoc.OpenCaptureFile` to obtain a :py:class:`~renderdoc.CaptureFile` instance. This gives us access to control over a capture file at a meta level. For more information see the :py:class:`CaptureFile` reference - the interface can also be used to create.

To open a file, use :py:meth:`~renderdoc.CaptureFile.OpenFile` on the :py:class:`~renderdoc.CaptureFile` instance. This function allows conversion from other formats via an importer, but here we'll use it just for opening a regular ``rdc`` file. It returns a :py:class:`~renderdoc.ReplayStatus` which can be used to determine what went wrong in the event that there was a problem. We then check that the capture uses an API which can be replayed locally - for example not every platform supports ``D3D11``, so on linux this would return no local replay support.

.. highlight:: python
.. code:: python

    # Open a capture file handle
    cap = rd.OpenCaptureFile()

    # Open a particular file - see also OpenBuffer to load from memory
    status = cap.OpenFile('test.rdc', '', None)

    # Make sure the file opened successfully
    if status != rd.ReplayStatus.Succeeded:
        raise RuntimeError("Couldn't open file: " + str(status))

    # Make sure we can replay
    if not cap.LocalReplaySupport():
        raise RuntimeError("Capture cannot be replayed")

Accessing Capture Analysis
--------------------------

Once the capture has been loaded, we can now begin the replay analysis. To do that we use :py:meth:`~renderdoc.CaptureFile.OpenCapture` which returns a tuple of :py:class:`~renderdoc.ReplayStatus` and :py:class:`~renderdoc.ReplayController`.

This function call will open the capture and begin to replay it, and initialise the analysis. The :py:class:`~renderdoc.ReplayController` returned is the interface to the majority of RenderDoc's replaying functionality.

.. highlight:: python
.. code:: python

    # Initialise the replay
    status,controller = cap.OpenCapture(None)

    if status != rd.ReplayStatus.Succeeded:
        raise RuntimeError("Couldn't initialise replay: " + str(status))

    # Now we can use the controller!
    print("%d top-level drawcalls" % len(controller.GetDrawcalls()))

Once we're done with the interfaces, we should call the ``Shutdown`` function on each, this allows the C++ interface to release the resources allocated.

.. highlight:: python
.. code:: python

    # Shutdown the controller first, then the capture file
    controller.Shutdown()

    cap.Shutdown()

Example Source
--------------

The full source for this example is available below:

.. only:: html and not htmlhelp

    :download:`Download the example script <renderdoc_intro.py>`.

.. literalinclude:: renderdoc_intro.py