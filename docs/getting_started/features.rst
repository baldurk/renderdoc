Features
========

This page documents the current feature set of RenderDoc. This gives an overview of what RenderDoc is capable of, and where it is in its development. You might also be interested in the :doc:`../behind_scenes/planned_features`.

Currently RenderDoc supports D3D11, OpenGL, and Vulkan on Windows and Linux, although the UI only runs on Windows currently. API support for other APIs such as D3D12 is planned. A Qt UI will be written to fully support Linux and in future OS X, but currently this is just at the drawing board stage.

RenderDoc can also double as an image viewer in a simplistic fashion, separate to its functionality as a debugger. Drag in or open any of a variety of image file formats and RenderDoc will display them as a texture in a log. This way it can be used as a simple e.g. dds viewer, with support for all sorts of formats, encodings and things typical image viewers don't tend to handle like mips, cubemaps and arrays.

Current Windows+D3D11 Feature set
---------------------------------

* Support for D3D11 and D3D11.1, Windows Vista and above.
* Trimming capture - capture file only contains data necessary for replaying the frame in question, not all textures &amp; buffers ever created in the lifetime of the app.
* Optional network support. The main use case is capture &amp; replay on the same machine, but you can also attach over the network, and replay on a remote host.
* Multiple frame capture with ability to open side-by-side to compare.
* Event browsing, with standard perfmarker style tree.
* Full D3D11 Pipeline display.

    * Resources bound to the pipeline are trimmed to what is actually in use, e.g. if a shader only references SRV slot 0, only SRV slot 0 will be displayed, even if something is bound to slot 1.
    * Where available D3D11 debug names are displayed, along with reflection data with the shader to clarify usage.
    * Structured buffers have their total element count displayed, and UAVs also show the current structure count.
    * Export of the pipeline to HTML file.

* Shader source display (where possible - i.e. debug info available).
* Timeline bar of the scene hierarchy.

    * Displays scene left-to-right in time, event hierarchy top-to-bottom.
    * *Not* scaled based on time of each drawcall
    * Individual draw events are shown as dots when the tree is full expanded.
    * The currently selected resource in the texture viewer is highlighted below individual drawcalls visible that use it - e.g. purple for 'used for write', green for 'used for read'

* For each drawcall, a list of all API calls (state/resource setting) is available, with each call optionally having a complete callstack to locate where it came from in-app.
* Mesh buffer inspection and visualisation before/after vertex shader and at the end of the geometry pipeline (after GS or DS, whichever is later). All views have arcball and flycam controls, Projected data is not limited to the 2D viewport, RenderDoc attempts to unproject to allow viewing in world-space.
* 'Raw' buffer inspection, e.g. for UAVs, VBs or other buffers. Custom format can be set with HLSL-lite syntax.
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
    * Several debug overlays for render targets - Wireframe, Depth pass/fail, Stencil pass/fail, Clipping (below black/above white points), NaN/-ve/INF highlight, quad overdraw.

* Pixel history view.
* Custom visualisation shader support - e.g. decode custom packed formats or gbuffers.
* Vertex, Pixel and Compute shader debugging.
* Hot shader editing and replacement.
* Auto-range fitting to min/max values in texture data, and histogram display.
* Simple per-drawcall timings.
* Python scripting console, giving access to some of the RenderDoc internals and core data structures.

Most of these should be intuitive if you've used a graphics debugger before.

Current OpenGL Feature set
--------------------------

Most features are present and supported for both D3D11 and OpenGL, but some are still in development. Feature parity is an important goal, but for now we list them separately just to be clear.

* Support for OpenGL Core profile 3.2+ on Windows and Linux.
* Optional network support. The main use case is capture &amp; replay on the same machine, but you can also attach over the network, and replay on a remote host.
* Event browsing, with standard ARB extension based tree.
* Full OpenGL Pipeline display.

    * Resources are shown on the stage that references them, so each shader stage shows a list of the resources, image read/writes, uniform buffers etc that it uses.
    * Where available ``KHR_debug`` glObjectLabel names are displayed, along with reflection data with the shader to clarify usage.

* Shader source display.
* Timeline bar of the scene hierarchy.

    * Displays scene left-to-right in time, event hierarchy top-to-bottom.
    * *Not* scaled based on time of each drawcall
    * Individual draw events are shown as dots when the tree is full expanded.
    * The currently selected resource in the texture viewer is highlighted below individual drawcalls visible that use it - e.g. purple for 'used for write', green for 'used for read'

* For each drawcall, a list of all API calls (state/resource setting) is available, with each call optionally having a complete callstack to locate where it came from in-app.
* Mesh buffer inspection and visualisation before/after vertex shader and at the end of the geometry pipeline (after GS or TES, whichever is later). All views have arcball and flycam controls, Projected data is not limited to the 2D viewport, RenderDoc attempts to unproject to allow viewing in world-space.
* 'Raw' buffer inspection, e.g. for SSBOs, VBs or other buffers. Custom format can be set with GLSL-lite syntax.
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
    * Several debug overlays for render targets - Wireframe, Depth pass/fail, Stencil pass/fail, Clipping (below black/above white points), NaN/-ve/INF highlight, quad overdraw.

* Custom visualisation shader support - e.g. decode custom packed formats or gbuffers.
* Hot shader editing and replacement.
* Auto-range fitting to min/max values in texture data, and histogram display.
* Simple per-drawcall timings.
* Python scripting console, giving access to some of the RenderDoc internals and core data structures.

Capturing on Linux is possible, although there is no native UI. The renderdoccmd program allows capturing on the command line, as well as opening a 'preview' window of the final frame of the framebuffer. For most work though, you have to transfer the .rdc capture file (by default placed in /tmp) to windows and open it in the UI there - logs are completely interchangeable between windows and linux.

Current Vulkan Feature set
--------------------------

As Vulkan is still in early support, some features are not yet supported. Feature parity is an important goal, but for now we list them separately just to be clear.

* Support for Vulkan 1.0 on Windows and Linux.
* Optional network support. The main use case is capture &amp; replay on the same machine, but you can also attach over the network, and replay on a remote host.
* Event browsing, with ``VK_EXT_debug_marker`` extension based tree.
* Full Vulkan Pipeline display.

    * Resources are shown on the stage that references them, so each shader stage shows a list of the resources that it uses. Uniform buffers are separated out for clarity, but all other resources are listed together along with their descriptor set and binding point.
    * Where available ``VK_EXT_debug_marker`` labelled names are displayed, along with reflection data from the SPIR-V to clarify usage.

* SPIR-V disassembly display.
* Timeline bar of the scene hierarchy.

    * Displays scene left-to-right in time, event hierarchy top-to-bottom.
    * *Not* scaled based on time of each drawcall
    * Individual draw events are shown as dots when the tree is full expanded.
    * The currently selected resource in the texture viewer is highlighted below individual drawcalls visible that use it - e.g. purple for 'used for write', green for 'used for read'

* For each drawcall, a list of all API calls (state/resource setting) is available, with each call optionally having a complete callstack to locate where it came from in-app.
* Mesh buffer inspection and visualisation before/after vertex shader. All views have arcball and flycam controls, Projected data is not limited to the 2D viewport, RenderDoc attempts to unproject to allow viewing in world-space.
* 'Raw' buffer inspection, e.g. for SSBOs, VBs or other buffers. Custom format can be set with HLSL or GLSL-lite syntax.
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
    * Several debug overlays for render targets - Wireframe, Depth pass/fail, Stencil pass/fail, Clipping (below black/above white points), NaN/-ve/INF highlight, quad overdraw.

* Auto-range fitting to min/max values in texture data, and histogram display.
* Simple per-drawcall timings.
* Python scripting console, giving access to some of the RenderDoc internals and core data structures.

Capturing on Linux is possible, although there is no native UI. The ``renderdoccmd`` program allows capturing on the command line, as well as opening a 'preview' window of the final frame of the framebuffer.

See Also
--------

* :doc:`../behind_scenes/planned_features`
