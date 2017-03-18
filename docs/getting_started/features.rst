Features
========

This page documents the current feature set of RenderDoc. This gives an overview of what RenderDoc is capable of, and where it is in its development. You might also be interested in the :doc:`../behind_scenes/planned_features`.

Currently RenderDoc supports Vulkan, D3D11, D3D12, and OpenGL on Windows and Linux. The primary UI only runs on Windows currently, a Qt UI is in progress to replace it as a cross-platform UI.

RenderDoc can also double as an image viewer in a simplistic fashion, separate to its functionality as a debugger. Drag in or open any of a variety of image file formats and RenderDoc will display them as a texture in a log. This way it can be used as a simple e.g. dds viewer, with support for all sorts of formats, encodings and things typical image viewers don't tend to handle like mips, cubemaps and arrays.

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
    * The currently selected resource in the texture viewer is highlighted below individual drawcalls visible that use it - e.g. purple for 'used for write', green for 'used for read'

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
* Simple per-drawcall timings.
* Python scripting console, giving access to some of the RenderDoc internals and core data structures.

Most of these should be intuitive if you've used a graphics debugger before.

D3D11
-----

* Support for D3D11 and D3D11.x, Windows Vista and above. Where hardware support isn't available for feature level 11, WARP will be used.
* Debug marker support comes from any available D3D interface (ID3DUserDefinedAnnotation, D3DPERF\_ functions, etc)
* Pixel history view.
* Vertex, Pixel and Compute shader debugging.
* Detailed statistics on API call usage throughout the frame.

D3D12
-----

* Support for D3D12, Windows 10 only.
* Debug marker uses the PIXSetMarker macros that go through SetMarker/BeginEvent/EndEvent on the command list

OpenGL
------

* Support for OpenGL Core profile 3.2+ on Windows and Linux.
* Tree heirarchy of events defined by any of the standard or vendor-specific extensions, and ``KHR_debug`` object labels used for object naming.

Capturing on Linux is possible, although there is no native UI. The renderdoccmd program allows capturing on the command line, as well as opening a 'preview' window of the final frame of the framebuffer. For most work though, you have to transfer the .rdc capture file (by default placed in /tmp) to windows and open it in the UI there - logs are completely interchangeable between windows and linux.

Vulkan
------

* Support for Vulkan 1.0 on Windows and Linux.
* Event markers and object naming both come from ``VK_EXT_debug_marker``.

Logs have a very limited amount of portability between machines. Many hardware-specific feature uses are baked into logs, and portability depends on how similar the captuer and replay hardware are, whether these feature uses can map the same in both cases. Logs are however completely portable between different OSes with sufficiently comparable hardware.

See Also
--------

* :doc:`../behind_scenes/planned_features`
