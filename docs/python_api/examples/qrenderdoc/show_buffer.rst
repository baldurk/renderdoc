Display buffer with format
==========================

This example shows an easy way to automate a repro case. It could be run as a command line argument when starting the UI, to avoid repetitive steps.

First the code opens a specified file, although this step could be omitted if the desired capture is already open.

.. highlight:: python
.. code:: python

    filename = "test.rdc"

    pyrenderdoc.LoadCapture(filename, filename, False, True)

Next we iterate through the list of buffers to find the one we want. The selection criteria are up to you, in this case we look at the name provided and identify the buffer by that, however it could also be a particular size, or the buffer bound at a given event.

.. highlight:: python
.. code:: python

    mybuf = renderdoc.ResourceId.Null()

    for buf in pyrenderdoc.GetBuffers():
        print("buf %s is %s" % (buf.resourceId, pyrenderdoc.GetResourceName(buf.resourceId)))

        # here put your actual selection criteria - i.e. look for a particular name
        if pyrenderdoc.GetResourceName(buf.resourceId) == "dataBuffer":
            mybuf = buf.resourceId
            break

    print("selected %s" % pyrenderdoc.GetResourceName(mybuf))

Once we've identified the buffer we want to view, we create a buffer viewer and display it on the main tool area.

.. highlight:: python
.. code:: python

    # Open a new buffer viewer for this buffer, with the given format
    bufview = pyrenderdoc.ViewBuffer(0, 0, mybuf, formatter)

    # Show the buffer viewer on the main tool area
    pyrenderdoc.AddDockWindow(bufview.Widget(), qrenderdoc.DockReference.MainToolArea, None)

Example Source
--------------

.. only:: html and not htmlhelp

    :download:`Download the example script <show_buffer.py>`.

.. literalinclude:: show_buffer.py