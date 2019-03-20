Features
========

This page documents the current feature set of RenderDoc. This gives an overview of what RenderDoc is capable of, and where it is in its development. You might also be interested in the :doc:`../behind_scenes/planned_features`.

Currently RenderDoc supports Vulkan, D3D11, D3D12, OpenGL, and OpenGL ES on Windows, Linux, Android, Stadia, and Nintendo Switch :sup:`TM`. The UI runs in Qt and will work on any desktop platform.

RenderDoc can also double as an image viewer in a simplistic fashion, separate to its functionality as a debugger. Drag in or open any of a variety of image file formats and RenderDoc will display them as if they were the only texture in a capture. This way it can be used as a simple e.g. dds viewer, with support for all sorts of formats, encodings and things typical image viewers don't tend to handle like mips, cubemaps and arrays.

Current Common Feature set
--------------------------

* Trimming capture - capture file only contains data necessary for replaying the frame in question, not all textures & buffers ever created in the lifetime of the app.
* Optional network support. The main way RenderDoc is used is capture & replay on the same machine, but you can also attach over the network, and replay on a remote host.
* Multiple frame capture with ability to open side-by-side to compare.
* Event browsing, with standard perfmarker style tree.
* Full graphics pipeline state display.

    * Resources bound to the pipeline are trimmed to what is actually in use, e.g. if a shader only references a texture in the first binding slot, textures in other binding slots will not be displayed by default.
    * Where available through the API, friendly debug names are displayed along with reflection data with the shader to clarify usage.
    * Export of the pipeline to HTML file.

* Shader source display (where possible - i.e. debug info available) and otherwise disassembly where appropriate (when the API has a concept of a compiled binary representation).
* Timeline bar of the scene hierarchy.

    * Displays scene left-to-right in time, event hierarchy top-to-bottom.
    * *Not* scaled based on time of each drawcall
    * Individual draw events are shown as dots when the tree is full expanded.
    * The currently selected resource in the texture viewer is highlighted below individual drawcalls visible that use it - e.g. red for 'used for read', green for 'used for write'

* Flexible resource inspector.

    * Anywhere in the UI that a resource is mentioned by name or handle, it is linked back to the resource inspector.
    * Contains full list of all resources and API objects.
    * Each resource is linked to any parent or child object, visualising construction dependencies.
    * The API calls used to create the object before its use in the frame are displayed.
    * Any object can be renamed, and its name automatically updates everywhere in the UI.

* For each drawcall, a list of all API calls (state/resource setting) is available, with each call optionally having a complete callstack to locate where it came from in-app.
* Mesh buffer inspection and visualisation before/after vertex shader and at the end of the geometry pipeline (after GS or DS, whichever is later). All views have arcball and flycam controls, Projected data is not limited to the 2D viewport, RenderDoc attempts to unproject to allow viewing in world-space.
* More advanced mesh visualisation such as viewing other components as position (e.g. to render a mesh in UV space), and visual mesh picking from both input and output panes.
* 'Raw' buffer inspection for buffers. Custom format can be set with HLSL-lite or GLSL-lite syntax.
* Buffer export to CSV or raw binary blob and texture saving to DDS.
* Texture/render target viewer.

    * List of textures/RTs in the frame, and filtering system.
    * Standard visualisation controls - zoom/fit to window, mip/face/slice selection.
    * RGBA channels or depth/stencil selection as appropriate for the type of resource.
    * Flexible 'range' selection tool, to set custom black/white points (default to 0 and 1).
    * Currently set RT/textures thumbnail strip - updates as you move through the frame. Follows the currently selected pipeline slot as it changes, rather than remaining on the given texture.
    * Tabbed view for locking a view of a particular resource over time.
    * Pixel value picking.
    * Save (in theory) any type of texture and format to various formats, dds as well as regular png/jpg.
    * Several debug overlays for render targets - Wireframe, Depth pass/fail, Stencil pass/fail, Clipping (below black/above white points), NaN/-ve/INF highlight, quad overdraw, triangle size.

* Custom visualisation shader support - e.g. decode custom packed formats or gbuffers.
* Hot shader editing and replacement.
* Auto-range fitting to min/max values in texture data, and histogram display.
* Simple per-drawcall timings and tabular view of GPU counter data.
* Python scripting console with full documented API, giving complete access to RenderDoc internals, core data structures, and the Qt UI itself.
* Import and Export of captures

    * Captures have an internal in-memory representation containing the full serialised data of all function calls, as well as associated metadata.
    * This capture representation can be used to export an ``.rdc`` file to another form that's easier for external tools to work with such as ``.xml``.
    * If the external format contains full expression of data, it can then be imported again back into an ``.rdc`` after modification.
    * This also allows generation of RenderDoc captures from arbitrary data into a standard format.

Most of these should be intuitive if you've used a graphics debugger before.

D3D11
-----

* Support for D3D11 up to D3D11.4, Windows Vista and above. Where hardware support isn't available for feature level 11, WARP will be used.
* Debug marker support comes from any available D3D interface (ID3DUserDefinedAnnotation, D3DPERF\_ functions, etc)
* Pixel history view.
* Vertex, Pixel and Compute shader debugging.
* Detailed statistics on API call usage throughout the frame.

D3D12
-----

* Support for D3D12 up to D3D12.3, Windows 10 only.
* Debug marker uses the PIXSetMarker macros that go through SetMarker/BeginEvent/EndEvent on the command list

Vulkan
------

* Support for Vulkan 1.1 on Windows, Linux, Android, and Stadia.
* Event markers and object naming both come from ``VK_EXT_debug_marker``.

OpenGL & OpenGL ES
------------------

* Support for OpenGL Core profile 3.2 - 4.6 on Windows and Linux.
* Support for OpenGL ES 2.0 - 3.2 on Linux, Windows, and Android.
* Tree hierarchy of events defined by any of the standard or vendor-specific extensions, and ``KHR_debug`` object labels used for object naming.

Captures have a very limited amount of portability between machines. Many hardware-specific feature uses are baked into captures, and portability depends on how similar the capture and replay hardware are, whether these feature uses can map the same in both cases. Captures are however completely portable between different OSes with sufficiently comparable hardware.

See Also
--------

* :doc:`../behind_scenes/planned_features`
