Display texture in window
=========================

In this example we will open a window and iterate through the capture on a loop, displaying the first colour output target on it.

.. note::

    This is intended for use with the python module directly, as the UI already has a texture viewer panel to do this with much more control. The principle is the same though and it can be useful reference of how to iterate over a capture.

To create a window we use tkinter, since it is provided with the Python distribution.

.. highlight:: python
.. code:: python

    # Use tkinter to create windows
    import tkinter

    # Create a simple window
    window = tkinter.Tk()
    window.geometry("1280x720")

Next we need to determine which windowing systems the RenderDoc implementation supports, and create a :py:class:`~renderdoc.WindowingData` object for the window we want to render to. For the purposes of this example we will look for Win32 since it's the simplest to set up - needing only a window handle that we can get from tkinter easily. XCB/XLib require a display connection, which would be possible to get from another library such as Qt.

Once we have the :py:class:`~renderdoc.WindowingData`, we can create a :py:class:`~renderdoc.ReplayOutput` using :py:meth:`~renderdoc.ReplayController.CreateOutput`.

.. highlight:: python
.. code:: python

    # Create renderdoc windowing data.
    winsystems = [rd.WindowingSystem(i) for i in controller.GetSupportedWindowSystems()]

    # Pass window system specific data here, See:
    # - renderdoc.CreateWin32WindowingData
    # - renderdoc.CreateXlibWindowingData
    # - renderdoc.CreateXCBWindowingData

    # This example code works on windows as that's simple to integrate with tkinter
    if not rd.WindowingSystem.Win32 in winsystems:
        raise RuntimeError("Example requires Win32 windowing system: " + str(winsystems))

    windata = rd.CreateWin32WindowingData(int(window.frame(), 16))

    # Create a texture output on the window
    out = controller.CreateOutput(windata, rd.ReplayOutputType.Texture)

In order to iterate over all drawcalls we need some global state first from :py:meth:`~renderdoc.ReplayController.GetTextures` and :py:meth:`~renderdoc.ReplayController.GetDrawcalls`, and we'll also define a helper function to fetch a particular texture by resourceId, so that we can easily look up the details for a texture.

.. highlight:: python
.. code:: python

    # Fetch the list of textures
    textures = controller.GetTextures()

    # Fetch the list of drawcalls
    draws = controller.GetDrawcalls()

    # Function to look up the texture descriptor for a given resourceId
    def getTexture(texid):
        global textures
        for tex in textures:
            if tex.resourceId == texid:
                return tex
        return None

We now define two callback functions - ``paint`` and ``advance``. ``paint`` will be called every 33ms, it will display the output with the latest state using :py:meth:`~renderdoc.ReplayOutput.Display`. ``advance`` changes the current state to reflect a new drawcall.

.. highlight:: python
.. code:: python

    # Our paint function will be called ever 33ms, to display the output
    def paint():
        global out, window
        out.Display()
        window.after(33, paint)

Within ``advance`` we do a few things. First we move the current event to the current drawcall's ``eventId``, using :py:meth:`~renderdoc.ReplayController.SetFrameEvent`. Then we set up the texture display configuration with :py:meth:`~renderdoc.ReplayOutput.SetTextureDisplay`, to point to the first colour output at that drawcall.

When we update to a new texture, we fetch its details using our earlier ``getTexture`` and calculate a scale that keeps the texture fully visible on screen.

Finally we move to the next drawcall in the list for the next time ``advance`` is called.

.. highlight:: python
.. code:: python

    # Start on the first drawcall
    curdraw = 0

    # The advance function will be called every 150ms, to move to the next draw
    def advance():
        global out, window, curdraw

        # Move to the current drawcall
        controller.SetFrameEvent(draws[curdraw].eventId, False)

        # Initialise a default TextureDisplay object
        disp = rd.TextureDisplay()

        # Set the first colour output as the texture to display
        disp.resourceId = draws[curdraw].outputs[0]

        # Get the details of this texture
        texDetails = getTexture(disp.resourceId)

        # Calculate the scale required in width and height
        widthScale = window.winfo_width() / texDetails.width
        heightScale = window.winfo_height() / texDetails.height

        # Use the lower scale to fit the texture on the window
        disp.scale = min(widthScale, heightScale)

        # Update the texture display
        out.SetTextureDisplay(disp)

        # Set the next drawcall
        curdraw = (curdraw + 1) % len(draws)

        window.after(150, advance)

Once we have the callbacks defined, we call them once to initialise the display and set up the repeated callbacks, and start the tkinter main window loop.

.. highlight:: python
.. code:: python

    # Start the callbacks
    advance()
    paint()

    # Start the main window loop
    window.mainloop()

Example Source
--------------

.. only:: html and not htmlhelp

    :download:`Download the example script <display_window.py>`.

.. literalinclude:: display_window.py