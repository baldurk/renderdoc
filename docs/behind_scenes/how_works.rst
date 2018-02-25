How RenderDoc works
===================

RenderDoc works on very simple operating principles. This page outlines the basic idea behind its functioning to give people a better idea of what's going on.

Capturing Frames
----------------

Leaving aside the relatively uninteresting matter of injecting the RenderDoc DLL and calling functions to configure it in the target process, we begin by looking at how RenderDoc captures a capture file.

We will use D3D11 as an example of a driver for RenderDoc - the driver layer is responsible both for faithfully capturing the application's API usage, as well as then replaying and analysing it later. Essentially anything built on top of a driver layer can be used agnostically of the API the application in question is using.

When the driver initialises it will hook into every entry point into the API such that when application uses the API it passes through the driver wrapper. In the case of D3D11 this is the ``D3D11CreateDevice`` and ``CreateDXGIFactory`` functions.

After this point all accesses to the API remain wrapped and the driver essentially sets itself up as a "man-in-the-middle" between the application and the real API.

The driver initialises in a background capture state. In this state it's up to the specific implementation about what it serialises. As a general rule, creation and deletion type actions are always serialised, and data-upload calls can sometimes be serialised. In some cases the driver might choose to optimise out some of the data-upload calls and lazy initialise the contents of some resources to save on background overhead.

This serialised data is stored in-memory in a chunk-based representation. Although it's up to the driver implementation it is generally refcounted such that resources which end up becoming unbound and destroyed will have their memory overhead deleted.

When the capture button is hit the driver will enter active capturing upon the beginning of the next frame. In this state every API call is serialised out in order and any initial contents and states are saved.

Once the frame completes, this frame capture is serialised to disk along with the in-memory data for any resources that are referenced - by default resources which are not referenced are not included in the capture.

Replaying & Analysing Captures
------------------------------

The replay process is ostensibly simple, but as with the capturing the devil is in the details.

When replaying, the initial section of the capture (up to the beginning of the frame) is read and executed verbatim. Each resource created is mapped to the live version and vice versa so later parts of the capture can obtain the replayed representation of the original resource.

RenderDoc then does an initial pass over the captured frame. This allows us to build up a list of all the 'drawcall' events, analyse dependencies and check which resources are used at each drawcall for read, write, and so on. An internal tree is built up similar to what you see in the Event Browser & API Inspector, as well as a linked list with the linear sequence of drawcalls, since both representations are useful for iterating over the frame.

After this point most work is done in response to user actions. The basic building block is replaying a partial frame. Most analysis tools are built out of either replaying up to the current event, replaying up to the event - not including the current drawcall - and replaying *only* the current drawcall.

Care is taken to minimise this as much as possible as this tends to be the slowest operation given the overheads of serialisation and decoding the command stream.

When replaying from the beginning of a frame (and not a partial subset of the frame) the initial states of all resources are applied, and the initial pipeline state is restored. Resources which did not have a serialised initial state (e.g. gbuffer textures) have an initial state saved before the first replay of the frame, and this is restored. That way you don't get effects 'leaking' from later in a frame into an earlier point.

For example, let's assume the user has the 'depth test' overlay enabled, and selects a new event. This is the order of events that occur for the Texture Viewer - other viewers follow similar patterns, with a certain degree of sharing to reduce redundant replays:

#. The capture is replayed up to, but not including, the selected drawcall. After doing this the current pipeline state and contents of all resources exactly match the state at the point of this drawcall.
#. We then save a copy of the pristine depth buffer, save the current pipeline state, and set the reversed depth test. Replacing the pixel shader with one that just writes red, we repeat the drawcall to draw all the areas that fail the depth test.
#. Restoring the depth buffer and repeating this with a pixel shader which writes green, we fill in the overlay. Both of these renders happen to an off-screen buffer.
#. After restoring the pipeline state we finally replay the original drawcall to get the final image.
#. When we want to re-paint the viewed texture (either regular painting, or if the user changed a visualisation option which is just a constant buffer value) we bind the current render target as a resource and render it to the texture viewer control, then render the overlay texture on top of that.
