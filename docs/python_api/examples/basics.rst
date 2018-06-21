Basic Interfaces
================

This document explains some common interfaces and their relationship, which can be useful as a primer to understand where to get started, as well as for reference to look back on from later examples.

Replay Basics
-------------

ReplayController
^^^^^^^^^^^^^^^^

The primary interface for accessing the low level of RenderDoc's replay analysis is :py:class:`~renderdoc.ReplayController`.

From this interface, information about the capture can be gathered, using e.g. :py:meth:`~renderdoc.ReplayController.GetDrawcalls` to return the list of root-level drawcalls in the frame, or :py:meth:`~renderdoc.ReplayController.GetResources` to return a list of all resources in the capture.

Some methods like the two above return information which is global and does not vary across the frame. Most functions however return information relative to the current point in the frame.

During RenderDoc's replay, you can imagine a cursor that moves back and forth between the start and end of the frame. All requests for information that varies - such as texture and buffer contents, pipeline state, and other information will be relative to the current event.

Every function call within a frame is assigned an ascending ``eventId``, from ``1`` up to as many events as are in the frame. Within the drawcall list returned by :py:meth:`~renderdoc.ReplayController.GetDrawcalls`, each drawcall contains a list of events in :py:attr:`~renderdoc.DrawcallDescription.events`. These contain all of the ``eventId`` that immediately preceeded the draw. The details of the function call can be found by using :py:attr:`~renderdoc.APIEvent.chunkIndex` as an index into the structured data returned from :py:meth:`~renderdoc.GetStructuredFile`. The structured data contains the function name and the complete set of parameters passed to it, with their values.

To change the current active event and move the cursor, you can call :py:meth:`~renderdoc.ReplayController.SetFrameEvent`. This will move the replay to represent the current state immediately after the given event has executed.

At this point you can use :py:meth:`~renderdoc.ReplayController.GetBufferData` and :py:meth:`~renderdoc.ReplayController.GetTextureData` to obtain the contents of a buffer or texture respectively. The pipeline state can be accessed via ``Get*PipelineState`` for each API - to determine the current capture's pipeline type you can fetch the API properties from :py:meth:`~renderdoc.ReplayController.GetAPIProperties`.

There is also an API-agnostic pipeline abstraction to return information that is the same across APIs. Using :py:meth:`~renderdoc.GetPipelineState` returns a :py:class:`~renderdoc.PipeState` which has accessors for fetching the current vertex buffers, shaders, and colour outputs. This allows you to write generic code that will work on any API that RenderDoc supports. The API-specific pipelines are still available through ``Get*PipelineState``.

For more examples of how to fetch data, see the concrete examples below.

ReplayOutput
^^^^^^^^^^^^

While :py:class:`~renderdoc.ReplayController` provides methods for obtaining data directly, it doesn't provide any functionality for displaying to a window. For this the :py:class:`~renderdoc.ReplayOutput` class allows you to bind to a window and configure the output.

First you need to gather the platform-specific windowing information in a :py:class:`~renderdoc.WindowingData`. This class is opaque to python, but you can create it using helper functions such as :py:func:`~renderdoc.CreateWin32WindowingData` and :py:func:`~renderdoc.CreateXlibWindowingData`. The parameters to these are platform specific, and are typically accepted as integers where they refer to a windowing handle.

To then create an output for a window, :py:meth:`~renderdoc.ReplayController.CreateOutput` can create different types of outputs.

Once created, you can configure the output with :py:meth:`~renderdoc.ReplayOutput.SetMeshDisplay` and :py:meth:`~renderdoc.ReplayOutput.SetTextureDisplay` to update the configuration, and then call :py:meth:`~renderdoc.ReplayOutput.Display` to display on screen.

.. _qrenderdoc-python-basics:

RenderDoc UI Basics
-------------------

The RenderDoc UI provides a number of useful abstractions over the lower level API, which can be convenient when developing scripts. In addition it gives access to the different panels to allow limited control over them. The ``pyrenderdoc`` global is available to all scripts running within the RenderDoc UI, and it provides access to all of these things.

Each single-instance panel such as the :py:class:`~qrenderdoc.TextureViewer` or :py:class:`~qrenderdoc.PipelineStateViewer` has accessors within the :py:class:`~qrenderdoc.CaptureContext`.

Functions such as :py:meth:`~qrenderdoc.CaptureContext.GetTextureViewer` will return a valid handle to the texture viewer, but if the texture viewer was closed then although it will be created it will *not* be immediately visible. You need to call :py:meth:`~qrenderdoc.CaptureContext.ShowTextureViewer` first which will bring the texture viewer to the front and make sure it is visible and docked if it wasn't already.

You can also create new instances of windows such as buffer or shader viewers using :py:meth:`~qrenderdoc.CaptureContext.ViewBuffer` or :py:meth:`~qrenderdoc.CaptureContext.ViewShader`.

The :py:class:`~qrenderdoc.CaptureContext` interface also provides useful utility functions such as :py:meth:`~qrenderdoc.CaptureContext.GetTexture` or :py:meth:`~qrenderdoc.CaptureContext.GetDrawcall` to look up objects by id instead of needing your own caching and lookup from the lists returned by the lower level interface.
