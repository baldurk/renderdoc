/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2023 Baldur Karlsson
 * Copyright (c) 2014 Crytek
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "apidefs.h"

// this #define can be used to mark a program as a 'replay' program which should not be captured.
// Any program used for such purpose must define and export this symbol in the main exe or one dll
// that will be loaded before renderdoc.dll is loaded.
#define REPLAY_PROGRAM_MARKER() \
  extern "C" RENDERDOC_EXPORT_API void RENDERDOC_CC renderdoc__replay__marker() {}
// declare ResourceId extremely early so that it can be referenced in structured_data.h

DOCUMENT("");
typedef uint8_t byte;

#if !defined(SWIG)
// needs to be declared up here for reference in rdcarray/rdcstr
extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_FreeArrayMem(void *mem);
typedef void(RENDERDOC_CC *pRENDERDOC_FreeArrayMem)(void *mem);

extern "C" RENDERDOC_API void *RENDERDOC_CC RENDERDOC_AllocArrayMem(uint64_t sz);
typedef void *(RENDERDOC_CC *pRENDERDOC_AllocArrayMem)(uint64_t sz);
#endif

// declare base types and stringise interface
#include "rdcarray.h"
#include "rdcdatetime.h"
#include "rdcpair.h"
#include "rdcstr.h"
#include "resourceid.h"
#include "stringise.h"

// include all API types now
#include "capture_options.h"
#include "control_types.h"
#include "data_types.h"
#include "pipestate.h"
#include "replay_enums.h"
#include "shader_types.h"
#include "structured_data.h"

DOCUMENT(R"(Create a :class:`WindowingData` for no backing window, it will be headless.

:param int width: The initial width for this virtual window.
:param int height: The initial height for this virtual window.

:return: A :class:`WindowingData` corresponding to an 'empty' backing window.
:rtype: WindowingData
)");
inline const WindowingData CreateHeadlessWindowingData(int32_t width, int32_t height)
{
  WindowingData ret = {};

  ret.system = WindowingSystem::Headless;

  ret.headless.width = width > 0 ? width : 1;
  ret.headless.height = height > 0 ? height : 1;

  return ret;
}

DOCUMENT(R"(Create a :class:`WindowingData` for a Win32 ``HWND`` handle.

:param HWND window: The native ``HWND`` handle for this window.
:return: A :class:`WindowingData` corresponding to the given window.
:rtype: WindowingData
)");
inline const WindowingData CreateWin32WindowingData(HWND window)
{
  WindowingData ret = {};

  ret.system = WindowingSystem::Win32;
  ret.win32.window = window;

  return ret;
}

DOCUMENT(R"(Create a :class:`WindowingData` for an Xlib ``Drawable`` handle.

:param Display display: The ``Display`` connection used for this window.
:param Drawable window: The native ``Drawable`` handle for this window.
:return: A :class:`WindowingData` corresponding to the given window.
:rtype: WindowingData
)");
inline const WindowingData CreateXlibWindowingData(Display *display, Drawable window)
{
  WindowingData ret = {};

  ret.system = WindowingSystem::Xlib;
  ret.xlib.display = display;
  ret.xlib.window = window;

  return ret;
}

DOCUMENT(R"(Create a :class:`WindowingData` for an XCB ``xcb_window_t`` handle.

:param xcb_connection_t connection: The ``xcb_connection_t`` connection used for this window.
:param xcb_window_t window: The native ``xcb_window_t`` handle for this window.
:return: A :class:`WindowingData` corresponding to the given window.
:rtype: WindowingData
)");
inline const WindowingData CreateXCBWindowingData(xcb_connection_t *connection, xcb_window_t window)
{
  WindowingData ret = {};

  ret.system = WindowingSystem::XCB;
  ret.xcb.connection = connection;
  ret.xcb.window = window;

  return ret;
}

DOCUMENT(R"(Create a :class:`WindowingData` for an Wayland ``wl_surface`` handle.

:param wl_display display: The ``wl_display`` connection used for this window.
:param wl_surface window: The native ``wl_surface`` handle for this window.
:return: A :class:`WindowingData` corresponding to the given window.
:rtype: WindowingData
)");
inline const WindowingData CreateWaylandWindowingData(wl_display *display, wl_surface *window)
{
  WindowingData ret = {};

  ret.system = WindowingSystem::Wayland;
  ret.wayland.display = display;
  ret.wayland.window = window;

  return ret;
}

DOCUMENT(R"(Create a :class:`WindowingData` for a GGP application.

:return: A :class:`WindowingData` corresponding to the given system.
:rtype: WindowingData
)");
inline const WindowingData CreateGgpWindowingData()
{
  WindowingData ret = {};

  ret.system = WindowingSystem::GGP;

  return ret;
}

DOCUMENT(R"(Create a :class:`WindowingData` for an Android ``ANativeWindow`` handle.

:param ANativeWindow window: The native ``ANativeWindow`` handle for this window.
:return: A :class:`WindowingData` corresponding to the given window.
:rtype: WindowingData
)");
inline const WindowingData CreateAndroidWindowingData(ANativeWindow *window)
{
  WindowingData ret = {};

  ret.system = WindowingSystem::Android;
  ret.android.window = window;

  return ret;
}

typedef void *NSView;
typedef void *CALayer;

DOCUMENT(R"(Create a :class:`WindowingData` for an metal/opengl-compatible macOS ``CALayer`` handle
and ``NSView`` handle (as void pointers).

:param NSView view: The native ``NSView`` handle for this window.
:param CALayer layer: The native ``CALayer`` handle for this window.
:return: A :class:`WindowingData` corresponding to the given window.
:rtype: WindowingData
)");
inline const WindowingData CreateMacOSWindowingData(NSView view, CALayer layer)
{
  WindowingData ret = {};

  ret.system = WindowingSystem::MacOS;
  ret.macOS.view = view;
  ret.macOS.layer = layer;

  return ret;
}

DOCUMENT(R"(A stateful output handle that contains the current configuration for one particular view
of the capture. This allows multiple outputs to run independently without interfering with each
other.

The different types are enumerated in :class:`ReplayOutputType`.

.. data:: NoResult

  No result was found in e.g. :meth:`PickVertex`.
)");
struct IReplayOutput
{
  DOCUMENT(R"(Shutdown this output.

It's optional to call this, as calling :meth:`ReplayController.Shutdown` will shut down all of its
outputs.
)");
  virtual void Shutdown() = 0;

  DOCUMENT(R"(Sets the configuration for a texture output.

:param TextureDisplay config: The configuration.
)");
  virtual void SetTextureDisplay(const TextureDisplay &config) = 0;

  DOCUMENT(R"(Sets the configuration for a mesh output.

:param MeshDisplay config: The configuration.
)");
  virtual void SetMeshDisplay(const MeshDisplay &config) = 0;

  DOCUMENT(R"(Sets the dimensions of the output, useful only for headless outputs that don't have a
backing window which don't have any implicit dimensions. This allows configuring a virtual viewport
which is useful for operations like picking vertices that depends on the output dimensions.

.. note:: For outputs with backing windows, this will be ignored.

:param int width: The width to use.
:param int height: The height to use.
)");
  virtual void SetDimensions(int32_t width, int32_t height) = 0;

  DOCUMENT(R"(Read the output texture back as byte data. Primarily useful for headless outputs where
the output data is not displayed anywhere natively.

:return: The output texture data as tightly packed RGB 3-byte data.
:rtype: bytes
)");
  virtual bytebuf ReadbackOutputTexture() = 0;

  DOCUMENT(R"(Retrieve the current dimensions of the output.

:return: The current width and height of the output.
:rtype: Tuple[int,int]
)");
  virtual rdcpair<int32_t, int32_t> GetDimensions() = 0;

  DOCUMENT(
      "Clear and release all thumbnails associated with this output. See :meth:`AddThumbnail`.");
  virtual void ClearThumbnails() = 0;

  DOCUMENT(R"(Sets up a thumbnail for displaying a particular texture with sensible defaults.

The window handle specified will be filled (in an aspect-ratio preserving way) with the texture.

If the window specified has been used for a thumbnail before, then the texture will be updated but
otherwise nothing will be created and the existing internal data will be reused. This means that
you can call this function multiple times to just change the texture.

Should only be called for texture outputs.

:param WindowingData window: A :class:`WindowingData` describing the native window.
:param ResourceId textureId: The texture ID to display in the thumbnail preview.
:param Subresource sub: The subresource within this texture to use.
:param CompType typeCast: If possible interpret the texture with this type instead of its normal
  type. If set to :data:`CompType.Typeless` then no cast is applied, otherwise where allowed the
  texture data will be reinterpreted - e.g. from unsigned integers to floats, or to unsigned
  normalised values.
:return: A result indicating if the thumbnail was successfully created.
:rtype: ResultDetails
)");
  virtual ResultDetails AddThumbnail(WindowingData window, ResourceId textureId,
                                     const Subresource &sub, CompType typeCast) = 0;

  DOCUMENT(R"(Draws a thumbnail for a particular texture with sensible defaults and returns an RGBA8
byte buffer for display. This does not render to a window but internally to a texture which is read
back from the GPU.

Should only be called for texture outputs.

:param int width: The width of the desired thumbnail.
:param int height: The height of the desired thumbnail.
:param ResourceId textureId: The texture ID to display in the thumbnail preview.
:param Subresource sub: The subresource within this texture to use.
:param CompType typeCast: If possible interpret the texture with this type instead of its normal
  type. If set to :data:`CompType.Typeless` then no cast is applied, otherwise where allowed the
  texture data will be reinterpreted - e.g. from unsigned integers to floats, or to unsigned
  normalised values.
:return: A buffer with the thumbnail RGBA8 data if successful, or empty if something went wrong.
:rtype: bytes
)");
  virtual bytebuf DrawThumbnail(int32_t width, int32_t height, ResourceId textureId,
                                const Subresource &sub, CompType typeCast) = 0;

  DOCUMENT(R"(Render to the window handle specified when the output was created.

This will also render any thumbnails and the pixel context, if enabled.
)");
  virtual void Display() = 0;

  DOCUMENT(R"(Sets up a zoomed in pixel context view around a particular pixel selection.

The texture rendering uses the configuration specified in :meth:`SetTextureDisplay` except with a
fixed high zoom value and a fixed position, see :meth:`SetPixelContextLocation`.

Should only be called for texture outputs.

:param WindowingData window: A :class:`WindowingData` describing the native window.
:return: A result indicating if the pixel context was successfully configured.
:rtype: ResultDetails
)");
  virtual ResultDetails SetPixelContext(WindowingData window) = 0;

  DOCUMENT(R"(Sets the pixel that the pixel context should be centred on.

Should only be called for texture outputs.

:param int x: The X co-ordinate to highlight.
:param int y: The Y co-ordinate to highlight.
)");
  virtual void SetPixelContextLocation(uint32_t x, uint32_t y) = 0;

  DOCUMENT("Disable the pixel context view from rendering.");
  virtual void DisablePixelContext() = 0;

  DOCUMENT(R"(Retrieves the :class:`ResourceId` containing the contents of the texture after being
passed through a custom shader pass.

Should only be called for texture outputs.

:return: The :class:`ResourceId` assigned to the texture with the results of the custom shader.
:rtype: ResourceId
)");
  virtual ResourceId GetCustomShaderTexID() = 0;

  DOCUMENT(R"(Retrieves the :class:`ResourceId` containing the contents of the debug overlay
rendering (if enabled).

Should only be called for texture outputs.

:return: The :class:`ResourceId` assigned to the texture with the debug overlay.
:rtype: ResourceId
)");
  virtual ResourceId GetDebugOverlayTexID() = 0;

  DOCUMENT(R"(Retrieves the vertex and instance that is under the cursor location, when viewed
relative to the current window with the current mesh display configuration.

.. note::
  X and Y co-ordinates are always considered to be top-left, even on GL, for consistency between
  APIs and preventing the need for API-specific code in most cases. This means if co-ordinates are
  fetched from e.g. viewport or scissor data or other GL pipeline state which is perhaps in
  bottom-left co-ordinates, care must be taken to translate them.

Should only be called for mesh outputs.

:param int x: The x co-ordinate to pick from.
:param int y: The y co-ordinate to pick from.
:return: A tuple with the first value being the vertex index in the mesh, and the second value being
  the instance index. The values are set to :data:`NoResult` if no vertex was found, 
:rtype: Tuple[int,int]
)");
  virtual rdcpair<uint32_t, uint32_t> PickVertex(uint32_t x, uint32_t y) = 0;

  static const uint32_t NoResult = ~0U;

protected:
  IReplayOutput() = default;
  ~IReplayOutput() = default;
};

DOCUMENT(R"(The primary interface to access the information in a capture and the current state, as
well as control the replay and analysis functionality available.

.. function:: KillCallback()

  Not an actual member function - the signature for any ``KillCallback`` callbacks.

  Called whenever some on-going blocking process needs to determine if it should close.

  :return: Whether or not the process should be killed.
  :rtype: bool

.. function:: ProgressCallback()

  Not an actual member function - the signature for any ``ProgressCallback`` callbacks.

  Called by an on-going blocking process to update a progress bar or similar user feedback.

  The progress value will go from 0.0 to 1.0 as the process completes. Any other value will indicate
  that the process has completed

  :param float progress: The latest progress amount.

.. function:: PreviewWindowCallback()

  Not an actual member function - the signature for any ``PreviewWindowCallback`` callbacks.

  Called when a preview window could optionally be opened to display some information. It will be
  called repeatedly with :paramref:`active` set to ``True`` to allow any platform-specific message
  pumping.

  :param bool active: ``True`` if a preview window is active/opened, ``False`` if it has closed.
  :return: The windowing data for a preview window, or empty/default values if no window should be
    created.
  :rtype: WindowingData

.. data:: NoPreference

  No preference for a particular value, see :meth:`ReplayController.DebugPixel`.
)");
struct IReplayController
{
  DOCUMENT(R"(Retrieve a :class:`APIProperties` object describing the current capture.

:return: The properties of the current capture.
:rtype: APIProperties
)");
  virtual APIProperties GetAPIProperties() = 0;

  DOCUMENT(R"(Retrieves the supported :class:`WindowingSystem` systems by the local system.

:return: The list of supported systems.
:rtype: List[WindowingSystem]
)");
  virtual rdcarray<WindowingSystem> GetSupportedWindowSystems() = 0;

  DOCUMENT(R"(Creates a replay output of the given type to the given native window

:param WindowingData window: A :class:`WindowingData` describing the native window.
:param ReplayOutputType type: What type of output to create
:return: A handle to the created output, or ``None`` on failure
:rtype: ReplayOutput
)");
  virtual IReplayOutput *CreateOutput(WindowingData window, ReplayOutputType type) = 0;

  DOCUMENT("Shutdown and destroy the current interface and all outputs that have been created.");
  virtual void Shutdown() = 0;

  DOCUMENT(R"(Goes into a blocking loop, repeatedly replaying the open capture as fast as possible,
displaying the selected texture in a default unscaled manner to the given output window.

The function won't return until :meth:`CancelReplayLoop` is called. Since this function is blocking, that
function must be called from another thread.

:param WindowingData window: A :class:`WindowingData` describing the native window.
:param ResourceId texid: The id of the texture to display.
)");
  virtual void ReplayLoop(WindowingData window, ResourceId texid) = 0;

  DOCUMENT(R"(Uses the given output window to create an RGP Profile.

:param WindowingData window: A :class:`WindowingData` describing the native window.
:return: The path to the created RGP profile, or empty on failure
:rtype: str
)");
  virtual rdcstr CreateRGPProfile(WindowingData window) = 0;

  DOCUMENT("Cancels a replay loop begun in :meth:`ReplayLoop`. Does nothing if no loop is active.");
  virtual void CancelReplayLoop() = 0;

  DOCUMENT("Notify the interface that the file it has open has been changed on disk.");
  virtual void FileChanged() = 0;

  DOCUMENT(R"(Move the replay to reflect the state immediately *after* the given
:data:`eventId <APIEvent.eventId>`.

:param int eventId: The :data:`eventId <APIEvent.eventId>` to move to.
:param bool force: ``True`` if the internal replay should refresh even if the ``eventId`` is
  already current. This can be useful if external factors might cause the replay to vary.
)");
  virtual void SetFrameEvent(uint32_t eventId, bool force) = 0;

  DOCUMENT(R"(Retrieve the current :class:`D3D11State` pipeline state.

The return value will be ``None`` if the capture is not using the D3D11 API.
You should use :meth:`GetAPIProperties` to determine the API of the capture.

See also :meth:`GetPipelineState`.

:return: The current D3D11 pipeline state.
:rtype: D3D11State
)");
  virtual const D3D11Pipe::State *GetD3D11PipelineState() = 0;

  DOCUMENT(R"(Retrieve the current :class:`D3D12State` pipeline state.

The return value will be ``None`` if the capture is not using the D3D12 API.
You should use :meth:`GetAPIProperties` to determine the API of the capture.

See also :meth:`GetPipelineState`.

:return: The current D3D12 pipeline state.
:rtype: D3D12State
)");
  virtual const D3D12Pipe::State *GetD3D12PipelineState() = 0;

  DOCUMENT(R"(Retrieve the current :class:`GLState` pipeline state.

The return value will be ``None`` if the capture is not using the OpenGL API.
You should use :meth:`GetAPIProperties` to determine the API of the capture.

See also :meth:`GetPipelineState`.

:return: The current OpenGL pipeline state.
:rtype: GLState
)");
  virtual const GLPipe::State *GetGLPipelineState() = 0;

  DOCUMENT(R"(Retrieve the current :class:`VKState` pipeline state.

The return value will be ``None`` if the capture is not using the Vulkan API.
You should use :meth:`GetAPIProperties` to determine the API of the capture.

See also :meth:`GetPipelineState`.

:return: The current Vulkan pipeline state.
:rtype: VKState
)");
  virtual const VKPipe::State *GetVulkanPipelineState() = 0;

  DOCUMENT(R"(Retrieve the current :class:`PipeState` pipeline state abstraction.

This pipeline state will always be valid, and allows queries that will work regardless of the
capture's API.

:return: The current pipeline state abstraction.
:rtype: PipeState
)");
  virtual const PipeState &GetPipelineState() = 0;

  DOCUMENT(R"(Retrieve the list of possible disassembly targets for :meth:`DisassembleShader`. The
values are implementation dependent but will always include a default target first which is the
native disassembly of the shader. Further options may be available for additional diassembly views
or hardware-specific ISA formats.

:param bool withPipeline: More disassembly may be available when a pipeline is specified.
:return: The list of disassembly targets available.
:rtype: List[str]
)");
  virtual rdcarray<rdcstr> GetDisassemblyTargets(bool withPipeline) = 0;

  DOCUMENT(R"(Retrieve the disassembly for a given shader, for the given disassembly target.

:param ResourceId pipeline: The pipeline state object, if applicable, that this shader is bound to.
:param ShaderReflection refl: The shader reflection details of the shader to disassemble
:param str target: The name of the disassembly target to generate for. Must be one of the values
  returned by :meth:`GetDisassemblyTargets`, or empty to use the default generation.
:return: The disassembly text, or an error message if something went wrong.
:rtype: str
)");
  virtual rdcstr DisassembleShader(ResourceId pipeline, const ShaderReflection *refl,
                                   const rdcstr &target) = 0;

  DOCUMENT(R"(Sets a list of directories to search for include files when compiling custom shaders
with the internal shader compiler.

.. note::
  This is currently only supported for D3D11 and D3D12. For Vulkan includes can be supported via
  configuring an external compiler to SPIR-V which is ingested.

:param List[str] directories: The absolute paths of the directories.
)");
  virtual void SetCustomShaderIncludes(const rdcarray<rdcstr> &directories) = 0;

  DOCUMENT(R"(Builds a shader suitable for running on the local replay instance as a custom shader.

System-level include directories can be set up via SetCustomShaderIncludes.

See :data:`TextureDisplay.customShaderId`.

:param str entry: The entry point to use when compiling.
:param ShaderEncoding sourceEncoding: The encoding of the source data.
:param bytes source: The source data itself.
:param ShaderCompileFlags compileFlags: API-specific compilation flags.
:param ShaderStage type: The stage that this shader will be executed at.
:return: A ``tuple`` with the id of the new shader if compilation was successful,
  :meth:`ResourceId.Null` otherwise, and a ``str`` with any warnings/errors from compilation.
:rtype: Tuple[ResourceId,str]
)");
  virtual rdcpair<ResourceId, rdcstr> BuildCustomShader(const rdcstr &entry,
                                                        ShaderEncoding sourceEncoding, bytebuf source,
                                                        const ShaderCompileFlags &compileFlags,
                                                        ShaderStage type) = 0;

  DOCUMENT(R"(Free a previously created custom shader.

See :meth:`BuildCustomShader`.

:param ResourceId id: The id of the custom shader to free.
)");
  virtual void FreeCustomShader(ResourceId id) = 0;

  DOCUMENT(R"(Builds a shader suitable for running in the capture's API as a replacement shader.

:param str entry: The entry point to use when compiling.
:param ShaderEncoding sourceEncoding: The encoding of the source data.
:param bytes source: The source data itself.
:param ShaderCompileFlags compileFlags: API-specific compilation flags.
:param ShaderStage type: The stage that this shader will be executed at.
:return: A ``tuple`` with the id of the new shader if compilation was successful,
  :meth:`ResourceId.Null` otherwise, and a ``str`` with any warnings/errors from compilation.
:rtype: Tuple[ResourceId,str]
)");
  virtual rdcpair<ResourceId, rdcstr> BuildTargetShader(const rdcstr &entry,
                                                        ShaderEncoding sourceEncoding, bytebuf source,
                                                        const ShaderCompileFlags &compileFlags,
                                                        ShaderStage type) = 0;

  DOCUMENT(R"(Retrieve the list of supported :class:`ShaderEncoding` which can be build using
:meth:`BuildTargetShader`.

The list is sorted in priority order, so if the caller has a shader in a form but could
compile/translate it to another, prefer to satisfy the first encoding before later encodings.

This typically means the 'native' encoding is listed first, and then subsequent encodings are
compiled internally - so compiling externally could be preferable as it allows better customisation
of the compile process or using alternate/updated tools.

:return: The list of target shader encodings available.
:rtype: List[ShaderEncoding]
)");
  virtual rdcarray<ShaderEncoding> GetTargetShaderEncodings() = 0;

  DOCUMENT(R"(Retrieve the list of supported :class:`ShaderEncoding` which can be build using
:meth:`BuildCustomShader`.

The list is sorted in priority order, so if the caller has a shader in a form but could
compile/translate it to another, prefer to satisfy the first encoding before later encodings.

This typically means the 'native' encoding is listed first, and then subsequent encodings are
compiled internally - so compiling externally could be preferable as it allows better customisation
of the compile process or using alternate/updated tools.

:return: The list of target shader encodings available.
:rtype: List[ShaderEncoding]
)");
  virtual rdcarray<ShaderEncoding> GetCustomShaderEncodings() = 0;

  DOCUMENT(R"(Retrieve a list of source prefixes that should be applied to custom shaders of each
:class:`ShaderEncoding` before custom compilation prior to calling :meth:`BuildCustomShader`.

This list provides source code prefixes which should be applied to a given custom shader in a
:class:`ShaderEncoding` *if and only if* that shader is being compiled in a custom step to a
different encoding, prior to being passed to :meth:`BuildCustomShader`. This allows source
compatibility even when doing custom compilation.

For example a shader written in :data:`ShaderEncoding.HLSL` may be custom compiled to
:data:`ShaderEncoding.SPIRV` before being passed to :meth:`BuildCustomShader`. In this case any
prefix for :data:`ShaderEncoding.HLSL` should be prepended to the source before custom compilation,
to allow for defines and other helpers to be made available, since otherwise the shader may not
compile.

If a shader encoding is not in the list, no prefix is required. This may be possible even for a
high level language such as :data:`ShaderEncoding.GLSL`.

:return: A list of pairs, listing a prefix for each shader encoding referenced.
:rtype: List[ShaderSourcePrefix]
)");
  virtual rdcarray<ShaderSourcePrefix> GetCustomShaderSourcePrefixes() = 0;

  DOCUMENT(R"(Replace one resource with another for subsequent replay and analysis work.

This is commonly used for modifying the capture by selectively replacing resources with newly
created resources.

See :meth:`BuildTargetShader`, :meth:`RemoveReplacement`.

:param ResourceId original: The id of the original resource that should be substituted.
:param ResourceId replacement: The id of the new resource that should be used instead.
)");
  virtual void ReplaceResource(ResourceId original, ResourceId replacement) = 0;

  DOCUMENT(R"(Remove any previously specified replacement for an object.

See :meth:`ReplaceResource`.

:param ResourceId id: The id of the original resource that was previously being substituted.
)");
  virtual void RemoveReplacement(ResourceId id) = 0;

  DOCUMENT(R"(Free a previously created target shader.

See :meth:`BuildTargetShader`.

:param ResourceId id: The id of the target shader to free.
)");
  virtual void FreeTargetResource(ResourceId id) = 0;

  DOCUMENT(R"(Retrieve the information about the frame contained in the capture.

:return: The frame information.
:rtype: FrameDescription
)");
  virtual FrameDescription GetFrameInfo() = 0;

  DOCUMENT(R"(Fetch the structured data representation of the capture loaded.

:return: The structured file.
:rtype: SDFile
)");
  virtual const SDFile &GetStructuredFile() = 0;

  DOCUMENT(R"(Add fake marker regions to the list of actions in the capture, based on which
textures are bound as outputs. Will not do anything if the capture already contains user marker
regions.

.. warning::
  This must be called *immediately* after capture load, calling it at a later time will cause
  corruption. No other functions should be called between load and this one.

.. note::
  The event IDs for fake marker pushes and pops will not be contiguous with the surrounding actions
  and will be set to values above the last real event in the capture. This also means they break the
  typical rules that event IDs always increase. It's recommended that these events are not
  referenced directly in other calls such as SetFrameEvent, and fake markers should be used 
  sparingly at all compared to proper application-provided markers.
)");
  virtual void AddFakeMarkers() = 0;

  DOCUMENT(R"(Retrieve the list of root-level actions in the capture.

:return: The list of root-level actions in the capture.
:rtype: List[ActionDescription]
)");
  virtual const rdcarray<ActionDescription> &GetRootActions() = 0;

  DOCUMENT(R"(Retrieve the values of a specified set of counters.

:param List[GPUCounter] counters: The list of counters to fetch results for.
:return: The list of counter results generated.
:rtype: List[CounterResult]
)");
  virtual rdcarray<CounterResult> FetchCounters(const rdcarray<GPUCounter> &counters) = 0;

  DOCUMENT(R"(Retrieve a list of which counters are available in the current capture analysis
implementation.

:return: The list of counters available.
:rtype: List[GPUCounter]
)");
  virtual rdcarray<GPUCounter> EnumerateCounters() = 0;

  DOCUMENT(R"(Get information about what a counter actually represents, in terms of a human-readable
understanding as well as the type and unit of the resulting information.

:param GPUCounter counter: The counter to query about.
:return: The description of the counter.
:rtype: CounterDescription
)");
  virtual CounterDescription DescribeCounter(GPUCounter counter) = 0;

  DOCUMENT(R"(Retrieve the list of all resources in the capture.

This includes any object allocated a :class:`ResourceId`, that don't have any other state or
are only used as intermediary elements.

:return: The list of resources in the capture.
:rtype: List[ResourceDescription]
)");
  virtual const rdcarray<ResourceDescription> &GetResources() = 0;

  DOCUMENT(R"(Retrieve the list of textures alive in the capture.

:return: The list of textures in the capture.
:rtype: List[TextureDescription]
)");
  virtual const rdcarray<TextureDescription> &GetTextures() = 0;

  DOCUMENT(R"(Retrieve the list of buffers alive in the capture.

:return: The list of buffers in the capture.
:rtype: List[BufferDescription]
)");
  virtual const rdcarray<BufferDescription> &GetBuffers() = 0;

  DOCUMENT(R"(Retrieve a list of any newly generated diagnostic messages.

Every time this function is called, any debug messages returned will not be returned again. Only
newly generated messages will be returned after that.

:return: The list of the :class:`DebugMessage` messages.
:rtype: List[DebugMessage]
)");
  virtual rdcarray<DebugMessage> GetDebugMessages() = 0;

  DOCUMENT(R"(Poll for the current status of the replay.

This function can be used to monitor to see if a fatal error has been encountered and react
appropriately, such as by displaying a message to the user. The replay controller interface should
remain stable and return null/empty data for the most part, but it's recommended for maximum
stability to stop using the controller when a fatal error is encountered.

If there has been no error, this will return :data:`ResultCode.Succeeded`. If there has been an
error this will return the error code every time, it will not be 'consumed' so it's safe to have
multiple things checking it.

:return: The current fatal error status.
:rtype: ResultDetails
)");
  virtual ResultDetails GetFatalErrorStatus() = 0;

  DOCUMENT(R"(Retrieve a list of entry points for a shader.

If the given ID doesn't specify a shader, an empty list will be return. On some APIs, the list will
only ever have one result (only one entry point per shader).

:param ResourceId shader: The shader to look up entry points for.
:return: The list of the :class:`ShaderEntryPoint` messages.
:rtype: List[ShaderEntryPoint]
)");
  virtual rdcarray<ShaderEntryPoint> GetShaderEntryPoints(ResourceId shader) = 0;

  DOCUMENT(R"(Retrieve the information about the frame contained in the capture.

:param ResourceId pipeline: The pipeline state object, if applicable, that this shader is bound to.
:param ResourceId shader: The shader to get reflection data for.
:param ShaderEntryPoint entry: The entry point within the shader to reflect. May be ignored on some
  APIs
:return: The frame information.
:rtype: ShaderReflection
)");
  virtual const ShaderReflection *GetShader(ResourceId pipeline, ResourceId shader,
                                            ShaderEntryPoint entry) = 0;

  DOCUMENT(R"(Retrieve the contents of a particular pixel in a texture.

.. note::
  X and Y co-ordinates are always considered to be top-left, even on GL, for consistency between
  APIs and preventing the need for API-specific code in most cases. This means if co-ordinates are
  fetched from e.g. viewport or scissor data or other GL pipeline state which is perhaps in
  bottom-left co-ordinates, care must be taken to translate them.

:param ResourceId textureId: The texture to pick the pixel from.
:param int x: The x co-ordinate to pick from.
:param int y: The y co-ordinate to pick from.
:param Subresource sub: The subresource within this texture to use.
:param CompType typeCast: If possible interpret the texture with this type instead of its normal
  type. If set to :data:`CompType.Typeless` then no cast is applied, otherwise where allowed the
  texture data will be reinterpreted - e.g. from unsigned integers to floats, or to unsigned
  normalised values.
:return: The contents of the pixel.
:rtype: PixelValue
)");
  virtual PixelValue PickPixel(ResourceId textureId, uint32_t x, uint32_t y, const Subresource &sub,
                               CompType typeCast) = 0;

  DOCUMENT(R"(Retrieves the minimum and maximum values in the specified texture.

:param ResourceId textureId: The texture to get the values from.
:param Subresource sub: The subresource within this texture to use.
:param CompType typeCast: If possible interpret the texture with this type instead of its normal
  type. If set to :data:`CompType.Typeless` then no cast is applied, otherwise where allowed the
  texture data will be reinterpreted - e.g. from unsigned integers to floats, or to unsigned
  normalised values.
:return: A tuple with the minimum and maximum pixel values respectively.
:rtype: Tuple[PixelValue,PixelValue]
)");
  virtual rdcpair<PixelValue, PixelValue> GetMinMax(ResourceId textureId, const Subresource &sub,
                                                    CompType typeCast) = 0;

  DOCUMENT(R"(Retrieve a list of values that can be used to show a histogram of values for the
specified texture.

The output list contains N buckets, and each bucket has the number of pixels that falls in each
bucket when the pixel values are divided between ``minval`` and ``maxval``.

:param ResourceId textureId: The texture to get the histogram from.
:param Subresource sub: The subresource within this texture to use.
:param CompType typeCast: If possible interpret the texture with this type instead of its normal
  type. If set to :data:`CompType.Typeless` then no cast is applied, otherwise where allowed the
  texture data will be reinterpreted - e.g. from unsigned integers to floats, or to unsigned
  normalised values.
:param float minval: The lower end of the smallest bucket. If any values are below this, they are
  not added to any bucket.
:param float maxval: The upper end of the largest bucket. If any values are above this, they are
  not added to any bucket.
:param Tuple[bool,bool,bool,bool] channels: A set of four flags indicating whether each of RGBA
  respectively should be included in the count.
:return: A list of the unnormalised bucket values.
:rtype: List[int]
)");
  virtual rdcarray<uint32_t> GetHistogram(ResourceId textureId, const Subresource &sub,
                                          CompType typeCast, float minval, float maxval,
                                          const rdcfixedarray<bool, 4> &channels) = 0;

  DOCUMENT(R"(Retrieve the history of modifications to the selected pixel on the selected texture.

.. note::
  X and Y co-ordinates are always considered to be top-left, even on GL, for consistency between
  APIs and preventing the need for API-specific code in most cases. This means if co-ordinates are
  fetched from e.g. viewport or scissor data or other GL pipeline state which is perhaps in
  bottom-left co-ordinates, care must be taken to translate them.

:param ResourceId texture: The texture to search for modifications.
:param int x: The x co-ordinate.
:param int y: The y co-ordinate.
:param Subresource sub: The subresource within this texture to use.
:param CompType typeCast: If possible interpret the texture with this type instead of its normal
  type. If set to :data:`CompType.Typeless` then no cast is applied, otherwise where allowed the
  texture data will be reinterpreted - e.g. from unsigned integers to floats, or to unsigned
  normalised values.
:return: The list of pixel history events.
:rtype: List[PixelModification]
)");
  virtual rdcarray<PixelModification> PixelHistory(ResourceId texture, uint32_t x, uint32_t y,
                                                   const Subresource &sub, CompType typeCast) = 0;

  DOCUMENT(R"(Retrieve a debugging trace from running a vertex shader.

:param int vertid: The vertex ID as a 0-based index up to the number of vertices in the draw.
:param int instid: The instance ID as a 0-based index up to the number of instances in the draw.
:param int idx: The actual index used to look up vertex inputs, either from the vertex ID for non-
  indexed draws or drawn from the index buffer. This must have all drawcall offsets applied.
:param int view: The index of the multiview viewport to use, or 0 if multiview is not in use.
:return: The resulting trace resulting from debugging. Destroy with
  :meth:`FreeTrace`.
:rtype: ShaderDebugTrace
)");
  virtual ShaderDebugTrace *DebugVertex(uint32_t vertid, uint32_t instid, uint32_t idx,
                                        uint32_t view) = 0;

  DOCUMENT(R"(Retrieve a debugging trace from running a pixel shader.

.. note::
  X and Y co-ordinates are always considered to be top-left, even on GL, for consistency between
  APIs and preventing the need for API-specific code in most cases. This means if co-ordinates are
  fetched from e.g. viewport or scissor data or other GL pipeline state which is perhaps in
  bottom-left co-ordinates, care must be taken to translate them.

:param int x: The x co-ordinate.
:param int y: The y co-ordinate.
:param int sample: The multi-sampled sample. Ignored if non-multisampled texture.
:param int primitive: Debug the pixel from this primitive if there's ambiguity. If set to
  :data:`NoPreference` then a random fragment writing to the given co-ordinate is debugged.
:return: The resulting trace resulting from debugging. Destroy with
  :meth:`FreeTrace`.
:rtype: ShaderDebugTrace
)");
  virtual ShaderDebugTrace *DebugPixel(uint32_t x, uint32_t y, uint32_t sample,
                                       uint32_t primitive) = 0;

  DOCUMENT(R"(Retrieve a debugging trace from running a compute thread.

:param Tuple[int,int,int] groupid: A list containing the 3D workgroup index.
:param Tuple[int,int,int] threadid: A list containing the 3D thread index within the workgroup.
:return: The resulting trace resulting from debugging. Destroy with
  :meth:`FreeTrace`.
:rtype: ShaderDebugTrace
)");
  virtual ShaderDebugTrace *DebugThread(const rdcfixedarray<uint32_t, 3> &groupid,
                                        const rdcfixedarray<uint32_t, 3> &threadid) = 0;

  DOCUMENT(R"(Continue a shader's debugging with a given shader debugger instance. This will run an
implementation defined number of steps and then return those steps in a list. This may be a fixed
number of steps or it may run for a fixed length of time and return as many steps as can be
calculated in that time.

This will always perform at least one step. If the list is empty, the debugging process has
completed, further calls will return an empty list.

:param ShaderDebugger debugger: The shader debugger to continue running.
:return: A number of subsequent states.
:rtype: List[ShaderDebugState]
)");
  virtual rdcarray<ShaderDebugState> ContinueDebug(ShaderDebugger *debugger) = 0;

  DOCUMENT(R"(Free a debugging trace from running a shader invocation debug.

:param ShaderDebugTrace trace: The shader debugging trace to free.
)");
  virtual void FreeTrace(ShaderDebugTrace *trace) = 0;

  DOCUMENT(R"(Retrieve a list of ways a given resource is used.

:param ResourceId id: The id of the texture or buffer resource to be queried.
:return: The list of usages of the resource.
:rtype: List[EventUsage]
)");
  virtual rdcarray<EventUsage> GetUsage(ResourceId id) = 0;

  DOCUMENT(R"(Retrieve the contents of a constant block by reading from memory or their source
otherwise.

:param ResourceId pipeline: The pipeline state object, if applicable, that this shader is bound to.
:param ResourceId shader: The id of the shader to use for metadata.
:param ShaderStage stage: The shader stage to fetch variables from.
:param str entryPoint: The entry point of the shader being used. In some APIs, this is ignored.
:param int cbufslot: The index in the :data:`ShaderReflection.constantBlocks` list to look up.
:param ResourceId buffer: The id of the buffer to use for data. If
  :data:`ConstantBlock.bufferBacked` is ``False`` this is ignored.
:param int offset: Retrieve buffer contents starting at this byte offset.
:param int length: Retrieve this many bytes after :paramref:`offset`. May be 0 to fetch the rest of the
  buffer.
:return: The shader variables with their contents.
:rtype: List[ShaderVariable]
)");
  virtual rdcarray<ShaderVariable> GetCBufferVariableContents(ResourceId pipeline,
                                                              ResourceId shader, ShaderStage stage,
                                                              const rdcstr &entryPoint,
                                                              uint32_t cbufslot, ResourceId buffer,
                                                              uint64_t offset, uint64_t length) = 0;

  DOCUMENT(R"(Save a texture to a file on disk, with possible transformation to map a complex
texture to something compatible with the target file format.

:param TextureSave saveData: The configuration settings of which texture to save, and how
:param str path: The path to save to on disk.
:return: The result of the operation.
:rtype: ResultDetails
)");
  virtual ResultDetails SaveTexture(const TextureSave &saveData, const rdcstr &path) = 0;

  DOCUMENT(R"(Retrieve the generated data from one of the geometry processing shader stages.

:param int instance: The index of the instance to retrieve data for, or 0 for non-instanced draws.
:param int view: The index of the multiview view to retrieve data for, or 0 if multiview is disabled.
:param MeshDataStage stage: The stage of the geometry processing pipeline to retrieve data from.
:return: The information describing where the post-transform data is stored.
:rtype: MeshFormat
)");
  virtual MeshFormat GetPostVSData(uint32_t instance, uint32_t view, MeshDataStage stage) = 0;

  DOCUMENT(R"(Retrieve the contents of a range of a buffer as a ``bytes``.

:param ResourceId buff: The id of the buffer to retrieve data from.
:param int offset: The byte offset to the start of the range.
:param int len: The length of the range, or 0 to retrieve the rest of the bytes in the buffer.
:return: The requested buffer contents.
:rtype: bytes
)");
  virtual bytebuf GetBufferData(ResourceId buff, uint64_t offset, uint64_t len) = 0;

  DOCUMENT(R"(Retrieve the contents of one subresource of a texture as a ``bytes``.

:param ResourceId tex: The id of the texture to retrieve data from.
:param Subresource sub: The subresource within this texture to use.
:return: The requested texture contents.
:rtype: bytes
)");
  virtual bytebuf GetTextureData(ResourceId tex, const Subresource &sub) = 0;

  static const uint32_t NoPreference = ~0U;

protected:
  IReplayController() = default;
  ~IReplayController() = default;
};

DECLARE_REFLECTION_STRUCT(IReplayController);

DOCUMENT(R"(A connection to a running application with RenderDoc injected, which allows limited
control over the capture process as well as querying the current status.
)");
struct ITargetControl
{
  DOCUMENT("Closes the connection without affecting the running application.");
  virtual void Shutdown() = 0;

  DOCUMENT(R"(Determines if the connection is still alive.

:return: ``True`` if the connection still appears to be working, ``False`` if it has been closed.
:rtype: bool
)");
  virtual bool Connected() = 0;

  DOCUMENT(R"(Retrieves the target's name or identifier - typically the name of the executable.

:return: The target name.
:rtype: str
)");
  virtual rdcstr GetTarget() = 0;

  DOCUMENT(R"(Retrieves the API currently in use by the target.

:return: The API name, or empty if no API is initialised yet.
:rtype: str
)");
  virtual rdcstr GetAPI() = 0;

  DOCUMENT(R"(Retrieves the Process ID (PID) of the target on its local system.

:return: The Process ID, or 0 if that's not applicable on the target platform.
:rtype: int
)");
  virtual uint32_t GetPID() = 0;

  DOCUMENT(R"(If a busy message was received, determine the client keeping the target busy.

:return: The name of the client currently connected to the target.
:rtype: str
)");
  virtual rdcstr GetBusyClient() = 0;

  DOCUMENT(R"(Trigger a capture on the target, with the same semantics as if the capture key had
been pressed - from the next presentation call after this message is processed on the target to the
next after that.

:param int numFrames: How many frames to capture. These will be captured sequentially and
  independently to separate files.
)");
  virtual void TriggerCapture(uint32_t numFrames) = 0;

  DOCUMENT(R"(Queue up a capture to happen on a particular frame number. When this frame is about to
begin a capture is begun, and it ends when this frame number ends.

.. note:: Frame 0 is defined as starting when the device is created, up to the first swapchain
  present defined frame boundary.

:param int frameNumber: The number of the frame to capture on.
:param int numFrames: How many frames to capture. These will be captured sequentially and
  independently to separate files.
)");
  virtual void QueueCapture(uint32_t frameNumber, uint32_t numFrames) = 0;

  DOCUMENT(R"(Begin copying a given capture stored on a remote machine to the local machine over the
target control connection.

:param int captureId: The identifier of the remote capture.
:param str localpath: The absolute path on the local system where the file should be saved.
)");
  virtual void CopyCapture(uint32_t captureId, const rdcstr &localpath) = 0;

  DOCUMENT(R"(Delete a capture from the remote machine.

:param int captureId: The identifier of the remote capture.
)");
  virtual void DeleteCapture(uint32_t captureId) = 0;

  DOCUMENT(R"(Query to see if a message has been received from the remote system.

The details of the types of messages that can be received are listed under
:class:`TargetControlMessage`.

.. note:: If no message has been received, this function will pump the connection. You are expected
  to continually call this function and process any messages to kee pthe connection alive.

  This function will block but only to a limited degree. If no message is waiting after a small time
  it will return with a No-op message to allow further processing.

:param ProgressCallback progress: A callback that will be repeatedly called with an updated progress
  value when a long blocking message is coming through, e.g. a capture copy. Can be ``None`` if no
  progress is desired.
:return: The message that was received.
:rtype: TargetControlMessage
)");
  virtual TargetControlMessage ReceiveMessage(RENDERDOC_ProgressCallback progress) = 0;

  DOCUMENT("Cycle the currently active window if there are more windows to capture.");
  virtual void CycleActiveWindow() = 0;

protected:
  ITargetControl() = default;
  ~ITargetControl() = default;
};

DOCUMENT(R"(An interface for accessing a capture, possibly over a network connection. This is a
subset of the functionality provided in :class:`CaptureFile` which only supports import/export
and construction of files.
)");
struct ICaptureAccess
{
  DOCUMENT(R"(Returns the list of available GPUs, that can be used in combination with
:class:`ReplayOptions` to force replay on a particular GPU.

:return: The list of GPUs available.
:rtype: List[GPUDevice]
)");
  virtual rdcarray<GPUDevice> GetAvailableGPUs() = 0;

  DOCUMENT(R"(Retrieve the total number of available sections.

:return: The number of sections in the capture
:rtype: int
)");
  virtual int32_t GetSectionCount() = 0;

  DOCUMENT(R"(Locate the index of a section by its name. Returns ``-1`` if the section is not found.

This index should not be cached, as writing sections could re-order the indices.

:param str name: The name of the section to search for.
:return: The index of the section, or ``-1`` if not found.
:rtype: int
)");
  virtual int32_t FindSectionByName(const rdcstr &name) = 0;

  DOCUMENT(R"(Locate the index of a section by its type. Returns ``-1`` if the section is not found.

This index should not be cached, as writing sections could re-order the indices.

:param SectionType type: The type of the section to search for.
:return: The index of the section, or ``-1`` if not found.
:rtype: int
)");
  virtual int32_t FindSectionByType(SectionType type) = 0;

  DOCUMENT(R"(Get the describing properties of the specified section.

:param int index: The index of the section.
:return: The properties of the section, if the index is valid.
:rtype: SectionProperties
)");
  virtual SectionProperties GetSectionProperties(int32_t index) = 0;

  DOCUMENT(R"(Get the raw byte contents of the specified section.

:param int index: The index of the section.
:return: The raw contents of the section, if the index is valid.
:rtype: bytes
)");
  virtual bytebuf GetSectionContents(int32_t index) = 0;

  DOCUMENT(R"(Writes a new section with specified properties and contents. If an existing section
already has the same type or name, it will be overwritten (two sections cannot share the same type
or name).

:param SectionProperties props: The properties of the section to be written.
:param bytes contents: The raw contents of the section.
:return: The result of the operation.
:rtype: ResultDetails
)");
  virtual ResultDetails WriteSection(const SectionProperties &props, const bytebuf &contents) = 0;

  DOCUMENT(R"(Query if callstacks are available.

:return: ``True`` if any callstacks are available, ``False`` otherwise.
:rtype: bool
)");
  virtual bool HasCallstacks() = 0;

  DOCUMENT(R"(Begin initialising a callstack resolver, looking up symbol files and caching as
necessary.

This function blocks while trying to initialise callstack resolving, so it should be called on a
separate thread.

:param bool interactive: ``True`` if missing symbols or other prompts should be resolved interactively.
  If this is ``False``, the function will not interact or block forever on user interaction and will
  always assume the input is effectively 'cancel' or empty. This may cause the symbol resolution to
  fail.
:param ProgressCallback progress: A callback that will be repeatedly called with an updated progress
  value for the resolver process. Can be ``None`` if no progress is desired.
:return: The result of the operation.
:rtype: ResultDetails
)");
  virtual ResultDetails InitResolver(bool interactive, RENDERDOC_ProgressCallback progress) = 0;

  DOCUMENT(R"(Retrieve the details of each stackframe in the provided callstack.

Must only be called after :meth:`InitResolver` has returned ``True``.

:param List[int] callstack: The integer addresses in the original callstack.
:return: The list of resolved callstack entries as strings.
:rtype: List[str]
)");
  virtual rdcarray<rdcstr> GetResolve(const rdcarray<uint64_t> &callstack) = 0;

  DOCUMENT(R"(Retrieves the name of the driver that was used to create this capture.

:return: A simple string identifying the driver used to make the capture.
:rtype: str
)");
  virtual rdcstr DriverName() = 0;

protected:
  ICaptureAccess() = default;
  ~ICaptureAccess() = default;
};

DOCUMENT(R"(A connection to a running remote RenderDoc server on another machine. This allows the
transfer of captures to and from the local machine, as well as remotely replaying a capture with a
local proxy renderer, so that captures that are not supported locally can still be debugged with as
much work as possible happening on the local machine.

.. data:: NoPreference

  No preference for a particular value, see :meth:`ReplayController.DebugPixel`.
)");
struct IRemoteServer : public ICaptureAccess
{
  DOCUMENT("Closes the connection without affecting the running server.");
  virtual void ShutdownConnection() = 0;

  DOCUMENT("Closes the connection and also tells the running server to close.");
  virtual void ShutdownServerAndConnection() = 0;

  DOCUMENT(R"(Pings the remote server to ensure the connection is still alive.

:return: The result of the operation - if a failure occurred the connection is no longer alive.
:rtype: ResultDetails
)");
  virtual ResultDetails Ping() = 0;

  DOCUMENT(R"(Retrieve a list of renderers available for local proxying.

These will be strings like "D3D11" or "OpenGL".

:return: A list of names of the local proxies.
:rtype: List[str]
)");
  virtual rdcarray<rdcstr> LocalProxies() = 0;

  DOCUMENT(R"(Retrieve a list of renderers supported by the remote server.

These will be strings like "D3D11" or "OpenGL".

:return: A list of names of the remote renderers.
:rtype: List[str]
)");
  virtual rdcarray<rdcstr> RemoteSupportedReplays() = 0;

  DOCUMENT(R"(Retrieve the path on the remote system where browsing can begin.

:return: The 'home' path where browsing for files or folders can begin.
:rtype: str
)");
  virtual rdcstr GetHomeFolder() = 0;

  DOCUMENT(R"(Retrieve the contents of a folder path on the remote system.

If an error occurs, a single :class:`PathEntry` will be returned with appropriate error flags.

:param str path: The remote path to list.
:return: The contents of the specified folder.
:rtype: List[PathEntry]
)");
  virtual rdcarray<PathEntry> ListFolder(const rdcstr &path) = 0;

  DOCUMENT(R"(Launch an application and inject into it to allow capturing.

This happens on the remote system, so all paths are relative to the remote filesystem.

:param str app: The path to the application to run.
:param str workingDir: The working directory to use when running the application. If blank, the
  directory containing the application is used.
:param str cmdLine: The command line to use when running the application, it will be processed in a
  platform specific way to generate arguments.
:param List[EnvironmentModification] env: Any environment changes that should be made when running
  the program.
:param CaptureOptions opts: The capture options to use when injecting into the program.
:return: The :class:`ExecuteResult` indicating both the status of the operation (success or failure)
  and any reason for failure, or else the ident where the new application is listening for target
  control if everything succeeded.
:rtype: ExecuteResult
)");
  virtual ExecuteResult ExecuteAndInject(const rdcstr &app, const rdcstr &workingDir,
                                         const rdcstr &cmdLine,
                                         const rdcarray<EnvironmentModification> &env,
                                         const CaptureOptions &opts) = 0;

  DOCUMENT(R"(Take ownership over a capture file.

Initially when a capture is made, it is owned by the injected library in the application. It passes
ownership to any program that is connected via target control that is notified about the capture,
which is then responsible for either saving the file or deleting it if it's unwanted.

Passing ownership of a file to the remote server means that it will be kept around for future use
until the server closes, at which point it will delete any files it owns.

:param str filename: The remote path to take ownership of.
)");
  virtual void TakeOwnershipCapture(const rdcstr &filename) = 0;

  DOCUMENT(R"(Copy a capture file that is stored on the local system to the remote system.

This function will block until the copy is fully complete, or an error has occurred.

This is primarily useful for when a capture is only stored locally and must be replayed remotely, as
the capture must be available on the machine where the replay happens.

:param str filename: The path to the file on the local system.
:param ProgressCallback progress: A callback that will be repeatedly called with an updated progress
  value for the copy. Can be ``None`` if no progress is desired.
:return: The path on the remote system where the capture was saved temporarily.
:rtype: str
)");
  virtual rdcstr CopyCaptureToRemote(const rdcstr &filename, RENDERDOC_ProgressCallback progress) = 0;

  DOCUMENT(R"(Copy a capture file that is stored on the remote system to the local system.

This function will block until the copy is fully complete, or an error has occurred.

:param str remotepath: The remote path where the file should be copied from.
:param str localpath: The local path where the file should be saved.
:param ProgressCallback progress: A callback that will be repeatedly called with an updated progress
  value for the copy. Can be ``None`` if no progress is desired.
)");
  virtual void CopyCaptureFromRemote(const rdcstr &remotepath, const rdcstr &localpath,
                                     RENDERDOC_ProgressCallback progress) = 0;

  DOCUMENT(R"(Open a capture file for remote capture and replay. The capture will be opened and
replayed on the remote system, and proxied to the local system with a given renderer. As much work
as possible will happen locally to save on bandwidth, processing and latency.

This function will block until the capture is fully opened on the remote system and ready for use,
or an error has occurred.

.. note:: You *must* close the resulting :class:`ReplayController` with the :meth:`CloseCapture`
  function to ensure the local proxy is correctly tidied up, instead of using
  :meth:`ReplayController.Shutdown`.

:param int proxyid: The index in the array returned by :meth:`LocalProxies` to use as a local proxy,
  or :data:`NoPreference` to indicate no preference for any proxy.
:param str logfile: The path on the remote system where the file is. If the file is only available
  locally you can use :meth:`CopyCaptureToRemote` to transfer it over the remote connection.
:param ReplayOptions opts: The options controlling how the capture should be replayed.
:param ProgressCallback progress: A callback that will be repeatedly called with an updated progress
  value for the opening. Can be ``None`` if no progress is desired.
:return: A tuple containing the status of opening the capture, whether success or failure, and the
  resulting :class:`ReplayController` handle if successful.
:rtype: Tuple[ResultDetails,ReplayController]
)");
  virtual rdcpair<ResultDetails, IReplayController *> OpenCapture(
      uint32_t proxyid, const rdcstr &logfile, const ReplayOptions &opts,
      RENDERDOC_ProgressCallback progress) = 0;

  DOCUMENT(R"(Close a capture analysis handle previously opened by :meth:`OpenCapture`.

:param ReplayController rend: The ReplayController that is to be closed.
)");
  virtual void CloseCapture(IReplayController *rend) = 0;

  static const uint32_t NoPreference = ~0U;

protected:
  IRemoteServer() = default;
  ~IRemoteServer() = default;
};

DOCUMENT(R"(A handle to a capture file. Used for simple cheap processing and meta-data fetching
without opening the capture for analysis.
)")
struct ICaptureFile : public ICaptureAccess
{
  DOCUMENT("Closes the file handle.");
  virtual void Shutdown() = 0;

  DOCUMENT(R"(Initialises the capture handle from a file.

This method supports converting from non-native representations via structured data, by specifying
the input format in the :paramref:`OpenFile.filetype` parameter. The list of supported formats can be retrieved
by calling :meth:`GetCaptureFileFormats`.

``rdc`` is guaranteed to always be a supported filetype, and will be assumed if the filetype is
empty or unrecognised.

:param str filename: The filename of the file to open.
:param str filetype: The format of the given file.
:param ProgressCallback progress: A callback that will be repeatedly called with an updated progress
  value if an import step occurs. Can be ``None`` if no progress is desired.
:return: The result of the operation.
:rtype: ResultDetails
)");
  virtual ResultDetails OpenFile(const rdcstr &filename, const rdcstr &filetype,
                                 RENDERDOC_ProgressCallback progress) = 0;

  DOCUMENT(R"(Initialises the file handle from a raw memory buffer.

This may be useful if you don't want to parse the whole file or already have the file in memory.

For the :paramref:`OpenBuffer.filetype` parameter, see :meth:`OpenFile`.

:param bytes buffer: The buffer containing the data to process.
:param str filetype: The format of the given file.
:param ProgressCallback progress: A callback that will be repeatedly called with an updated progress
  value if an import step occurs. Can be ``None`` if no progress is desired.
:return: The result of the operation.
:rtype: ResultDetails
)");
  virtual ResultDetails OpenBuffer(const bytebuf &buffer, const rdcstr &filetype,
                                   RENDERDOC_ProgressCallback progress) = 0;

  DOCUMENT(R"(When a capture file is opened, an exclusive lock is held on the file on disk. This
makes it impossible to copy the file to another location at the user's request. Calling this
function will copy the file on disk to a new location but otherwise won't affect the capture handle.
The new file will be locked, the old file will be unlocked - to allow deleting if necessary.

It is invalid to call this function if :meth:`OpenFile` has not previously been called to open the
file.

:param str filename: The filename to copy to.
:return: The result of the file copy operation.
:rtype: ResultDetails
)");
  virtual ResultDetails CopyFileTo(const rdcstr &filename) = 0;

  DOCUMENT(R"(Converts the currently loaded file to a given format and saves it to disk.

This allows converting a native RDC to another representation, or vice-versa converting another
representation back to native RDC.

:param str filename: The filename to save to.
:param str filetype: The format to convert to.
:param SDFile file: An optional :class:`SDFile` with the structured data to source from. This is
  useful in case the format specifies that it doesn't need buffers, and you already have a
  :class:`ReplayController` open with the structured data. This saves the need to load the file
  again. If ``None`` then structured data will be fetched if not already present and used.
:param ProgressCallback progress: A callback that will be repeatedly called with an updated progress
  value for the conversion. Can be ``None`` if no progress is desired.
:return: The result of the operation.
:rtype: ResultDetails
)");
  virtual ResultDetails Convert(const rdcstr &filename, const rdcstr &filetype, const SDFile *file,
                                RENDERDOC_ProgressCallback progress) = 0;

  DOCUMENT(R"(Returns the list of capture file formats.

:return: The list of capture file formats available.
:rtype: List[CaptureFileFormat]
)");
  virtual rdcarray<CaptureFileFormat> GetCaptureFileFormats() = 0;

  DOCUMENT(R"(Queries for how well a particular capture is supported on the local machine.

If the file was opened with a format other than native ``rdc`` this will always return no
replay support.

:return: How much support for replay exists locally.
:rtype: ReplaySupport
)");
  virtual ReplaySupport LocalReplaySupport() = 0;

  DOCUMENT(R"(Retrieves the identifying string describing what type of machine created this capture.

:return: A string identifying the machine ident used to make the capture.
:rtype: str
)");
  virtual rdcstr RecordedMachineIdent() = 0;

  DOCUMENT(R"(Retrieves the timestamp basis that all timestamps in the capture are relative to. May
be 0 if all timestamps are already absolute.

:return: The timestamp base value
:rtype: int
)");
  virtual uint64_t TimestampBase() = 0;

  DOCUMENT(R"(Retrieves frequency for timestamps and durations to be divided by to convert to
microseconds. May be 1.0 if all timestamps and durations are already in microseconds.

:return: The timestamp frequency
:rtype: float
)");
  virtual double TimestampFrequency() = 0;

  DOCUMENT(R"(Sets the matadata for this capture handle.

This function may only be called if the handle is 'empty' - i.e. no file has been opened with
:meth:`OpenFile` or :meth:`OpenBuffer`.

.. note:: The only supported values for :paramref:`SetMetadata.thumbType` are :attr:`FileType.JPG`,
  :attr:`FileType.PNG`, :attr:`FileType.TGA`, and :attr:`FileType.BMP`.

:param str driverName: The name of the driver. Must be a recognised driver name (even if replay
  support for that driver is not compiled in locally.
:param int machineIdent: The encoded machine identity value. Optional value and can be left to 0, as
  the bits to set are internally defined, so only generally useful if copying a machine ident from
  an existing capture.
:param FileType thumbType: The file type of the thumbnail. Ignored if
  :paramref:`SetMetadata.thumbData` is empty.
:param int thumbWidth: The width of the thumbnail. Ignored if :paramref:`SetMetadata.thumbData` is
  empty.
:param int thumbHeight: The height of the thumbnail. Ignored if :paramref:`SetMetadata.thumbData` is
  empty.
:param bytes thumbData: The raw data of the thumbnail. If empty, no thumbnail is set.
:param int timeBase: The base value for timestamps in the capture. Can be set to 0 to indicate that
  timestamps are already capture relative.
:param float timeFreq: The frequency for timestamps and durations to be divided by to convert to
  microseconds. Can be set to 1.0 to indicate that timestamps and durations are already in
  microseconds.
)");
  virtual void SetMetadata(const rdcstr &driverName, uint64_t machineIdent, FileType thumbType,
                           uint32_t thumbWidth, uint32_t thumbHeight, const bytebuf &thumbData,
                           uint64_t timeBase, double timeFreq) = 0;

  DOCUMENT(R"(Opens a capture for replay locally and returns a handle to the capture. Only supported
for handles opened with a native ``rdc`` capture, otherwise this will fail.

This function will block until the capture is fully loaded and ready.

Once the replay is created, this :class:`CaptureFile` can be shut down, there is no dependency on it
by the :class:`ReplayController`.

:param ReplayOptions opts: The options controlling how the capture should be replayed.
:param ProgressCallback progress: A callback that will be repeatedly called with an updated progress
  value for the opening. Can be ``None`` if no progress is desired.
:return: A tuple containing the status of opening the capture, whether success or failure, and the
  resulting :class:`ReplayController` handle if successful.
:rtype: Tuple[ResultDetails,ReplayController]
)");
  virtual rdcpair<ResultDetails, IReplayController *> OpenCapture(
      const ReplayOptions &opts, RENDERDOC_ProgressCallback progress) = 0;

  DOCUMENT(R"(Returns the structured data for this capture.

The lifetime of this data is scoped to the lifetime of the capture handle, so it cannot be used
after the handle is destroyed.

:return: The structured data representing the file.
:rtype: SDFile
)");
  virtual const SDFile &GetStructuredData() = 0;

  DOCUMENT(R"(Sets the structured data for this capture.

This allows calling code to populate a capture out of generated structured data. In combination with
:meth:`SetMetadata` this allows a purely in-memory creation of a file to be saved out with
:meth:`Convert`.

The data is copied internally so it can be destroyed after calling this function.

:param SDFile file: The structured data representing the file.
)");
  virtual void SetStructuredData(const SDFile &file) = 0;

  DOCUMENT(R"(Retrieves the embedded thumbnail from the capture.

.. note:: The only supported values for :paramref:`GetThumbnail.type` are :attr:`FileType.JPG`,
  :attr:`FileType.PNG`, :attr:`FileType.TGA`, and :attr:`FileType.BMP`.

:param FileType type: The image format to convert the thumbnail to.
:param int maxsize: The largest width or height allowed. If the thumbnail is larger, it's resized.
:return: The raw contents of the thumbnail, converted to the desired type at the desired max
  resolution.
:rtype: Thumbnail
  )");
  virtual Thumbnail GetThumbnail(FileType type, uint32_t maxsize) = 0;

protected:
  ICaptureFile() = default;
  ~ICaptureFile() = default;
};

//////////////////////////////////////////////////////////////////////////
// camera
//////////////////////////////////////////////////////////////////////////

DOCUMENT(R"(A handle to a camera controller, used for user interaction and controlling a view of a
3D scene.
)")
struct ICamera
{
  DOCUMENT("Closes the camera handle.");
  virtual void Shutdown() = 0;

  DOCUMENT(R"(Sets the position for the camera, either arcball or FPS.

For arcball cameras, this sets the lookat position at the centre of the arcball.

For FPS look cameras, this sets the position of the camera in space.

:param float x: The X co-ordinate of the position.
:param float y: The Y co-ordinate of the position.
:param float z: The Z co-ordinate of the position.
)");
  virtual void SetPosition(float x, float y, float z) = 0;

  DOCUMENT(R"(Sets the rotation for an FPS camera.

It is invalid to call this function for arcball cameras.

:param float x: The rotation around the X axis (pitch).
:param float y: The rotation around the Y axis (yaw).
:param float z: The rotation around the Z axis (roll).
)");
  virtual void SetFPSRotation(float x, float y, float z) = 0;

  DOCUMENT(R"(Sets the distance in units the arcball camera sits away from the lookat position.

:param float dist: The distance of the camera from the lookat position.
)");
  virtual void SetArcballDistance(float dist) = 0;

  DOCUMENT("Reset the arcball to defaults.");
  virtual void ResetArcball() = 0;

  DOCUMENT(R"(Rotates the arcball based on relative window co-ordinates.

The co-ordinates are in pixels and represent the old/new co-ordinates of the mouse cursor over the
drag.

:param float ax: The X co-ordinate of the previous mouse position.
:param float ay: The Y co-ordinate of the previous mouse position.
:param float bx: The X co-ordinate of the new mouse position.
:param float by: The Y co-ordinate of the new mouse position.
)");
  virtual void RotateArcball(float ax, float ay, float bx, float by) = 0;

  DOCUMENT(R"(Retrieves the position of the camera

:return: The position vector of the camera. W is set to 1
:rtype: FloatVector
)");
  virtual FloatVector GetPosition() = 0;

  DOCUMENT(R"(Retrieves the forward vector of the camera, in the positive Z direction.

:return: The forward vector of the camera. W is set to 1
:rtype: FloatVector
)");
  virtual FloatVector GetForward() = 0;

  DOCUMENT(R"(Retrieves the right vector of the camera, in the positive X direction.

:return: The right vector of the camera. W is set to 1
:rtype: FloatVector
)");
  virtual FloatVector GetRight() = 0;

  DOCUMENT(R"(Retrieves the up vector of the camera, in the positive Y direction.

:return: The up vector of the camera. W is set to 1
:rtype: FloatVector
)");
  virtual FloatVector GetUp() = 0;

protected:
  ICamera() = default;
  ~ICamera() = default;
};

DOCUMENT(R"(Create a new camera of a given type.

:param CameraType type: The type of controls to use
:return: The handle to the new camera.
:rtype: Camera
)");
extern "C" RENDERDOC_API ICamera *RENDERDOC_CC RENDERDOC_InitCamera(CameraType type);

//////////////////////////////////////////////////////////////////////////
// Maths/format/misc related exports
//////////////////////////////////////////////////////////////////////////

DOCUMENT(R"(A utility function that converts a half (stored in a 16-bit unsigned integer) to a
float.

:param int half: The half stored as an int.
:return: The floating point equivalent.
:rtype: float
)");
extern "C" RENDERDOC_API float RENDERDOC_CC RENDERDOC_HalfToFloat(uint16_t half);

DOCUMENT(R"(A utility function that converts a float to a half (stored in a 16-bit unsigned
integer).

:param float flt: The floating point value.
:return: The nearest half-float equivalent stored as an int.
:rtype: int
)");
extern "C" RENDERDOC_API uint16_t RENDERDOC_CC RENDERDOC_FloatToHalf(float flt);

DOCUMENT(R"(A utility function that returns the number of vertices in a primitive of a given
topology.

.. note:: In strip topologies vertices are re-used.

:param Topology topology: The topology to query about.
:return: The number of vertices in a single primitive.
:rtype: int
)");
extern "C" RENDERDOC_API uint32_t RENDERDOC_CC RENDERDOC_NumVerticesPerPrimitive(Topology topology);

DOCUMENT(R"(A utility function that returns the offset in the list of vertices of the first vertex
in a particular primitive of a given topology. This calculation is simple but not trivial for the
case of strip topologies.

:param Topology topology: The topology to query about.
:param int primitive: The primitive to query about.
:return: The vertex offset where the primitive starts.
:rtype: int
)");
extern "C" RENDERDOC_API uint32_t RENDERDOC_CC RENDERDOC_VertexOffset(Topology topology,
                                                                      uint32_t primitive);

//////////////////////////////////////////////////////////////////////////
// Create a capture file handle.
//////////////////////////////////////////////////////////////////////////

DOCUMENT(R"(Create a handle for a capture file.

This function returns a new handle to a capture file. Once initialised by opening a file the handle
can only be shut-down, it is not re-usable.

:return: A handle to the specified path.
:rtype: CaptureFile
)");
extern "C" RENDERDOC_API ICaptureFile *RENDERDOC_CC RENDERDOC_OpenCaptureFile();

//////////////////////////////////////////////////////////////////////////
// Target Control
//////////////////////////////////////////////////////////////////////////

DOCUMENT(R"(Creates a :class:`TargetControl` connection to a given hostname and ident.

This function will block until the control connection is ready, or an error occurs.

:param str URL: The URL to connect to. If blank, the local machine is used. If no protocol is
  specified then default TCP enumeration happens.
:param int ident: The ident for the particular target to connect to on that machine.
:param str clientName: The client name to use when connecting. See
  :meth:`TargetControl.GetBusyClient`.
:param bool forceConnection: Force the connection and kick off any existing client that is currently
  connected.
:return: A handle to the target control connection, or ``None`` if something went wrong.
:rtype: TargetControl
)");
extern "C" RENDERDOC_API ITargetControl *RENDERDOC_CC RENDERDOC_CreateTargetControl(
    const rdcstr &URL, uint32_t ident, const rdcstr &clientName, bool forceConnection);

DOCUMENT(R"(Repeatedly query to enumerate which targets are active on a given machine and their
idents.

Initially this should be called with ``nextIdent`` being 0, to retrieve the first target
active. After that it can be called again and again with the previous return value to enumerate
more targets.

This function will block for a variable timeout depending on how many targets are scanned.

:param str URL: The URL to connect to. If blank, the local machine is used. If no protocol is
  specified then default TCP enumeration happens.
:param int nextIdent: The next ident to scan.
:return: The ident of the next active target, or ``0`` if no other targets exist.
:rtype: int
)");
extern "C" RENDERDOC_API uint32_t RENDERDOC_CC RENDERDOC_EnumerateRemoteTargets(const rdcstr &URL,
                                                                                uint32_t nextIdent);

//////////////////////////////////////////////////////////////////////////
// Remote server
//////////////////////////////////////////////////////////////////////////

DOCUMENT(R"(Create a connection to a remote server running on given hostname.

:param str URL: The hostname to connect to, if blank then localhost is used. If no protocol is
  specified then default TCP enumeration happens.
:return: The status of opening the connection, whether success or failure, and a :class:`RemoteServer`
  instance if it were successful
:rtype: Tuple[ResultDetails,RemoteServer]
)");
extern "C" RENDERDOC_API ResultDetails RENDERDOC_CC
RENDERDOC_CreateRemoteServerConnection(const rdcstr &URL, IRemoteServer **rend);

DOCUMENT(R"(Check the connection to a remote server running on given hostname.

This should be preferred to :func:`CreateRemoteServerConnection` when no connection is desired, as
the status can be checked without interfering with making connections.

:param str URL: The hostname to connect to, if blank then localhost is used. If no protocol is
  specified then default TCP enumeration happens.
:return: The status of the server.
:rtype: ResultDetails
)");
extern "C" RENDERDOC_API ResultDetails RENDERDOC_CC
RENDERDOC_CheckRemoteServerConnection(const rdcstr &URL);

DOCUMENT(R"(This launches a remote server which will continually run in a loop to server requests
from external sources.

This function will block until a remote connection tells the server to shut down, or the
``killReplay`` callback returns ``True``.

:param str listenhost: The name of the interface to listen on.
:param int port: The port to listen on, or ``0`` to listen on the default port.
:param KillCallback killReplay: A callback that returns a ``bool`` indicating if the server should
  be shut down or not.
:param PreviewWindowCallback previewWindow: A callback that returns information for a preview window
  when the server wants to display some preview of the ongoing replay.
)");
extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_BecomeRemoteServer(
    const rdcstr &listenhost, uint16_t port, RENDERDOC_KillCallback killReplay,
    RENDERDOC_PreviewWindowCallback previewWindow);

//////////////////////////////////////////////////////////////////////////
// Injection/execution capture functions.
//////////////////////////////////////////////////////////////////////////

DOCUMENT(R"(Retrieve the default and recommended set of capture options.

:return: The default capture options.
:rtype: CaptureOptions
)");
extern "C" RENDERDOC_API void RENDERDOC_CC
RENDERDOC_GetDefaultCaptureOptions(CaptureOptions *defaultOpts);

DOCUMENT(R"(Begin injecting speculatively into all new processes started on the system. Where
supported by platform, configuration, and setup begin injecting speculatively into all new processes
started on the system.

This function can only be called if global hooking is supported (see :func:`CanGlobalHook`) and if
global hooking is not active (see :func:`IsGlobalHookActive`).

The hook must be closed with :func:`StopGlobalHook` before the application is closed.

This function must be called when the process is running with administrator/superuser permissions.

:param str pathmatch: A string to match against each new process's executable path to determine
  which corresponds to the program we actually want to capture.
:param str logfile: Where to store any captures.
:param CaptureOptions opts: The capture options to use when injecting into the program.
:return: The result of the operation, if the result succeeded the hook is now active.
:rtype: ResultDetails
)");
extern "C" RENDERDOC_API ResultDetails RENDERDOC_CC RENDERDOC_StartGlobalHook(
    const rdcstr &pathmatch, const rdcstr &logfile, const CaptureOptions &opts);

DOCUMENT(R"(Stop the global hook that was activated by :func:`StartGlobalHook`.

This function can only be called if global hooking is supported (see :func:`CanGlobalHook`) and if
global hooking is active (see :func:`IsGlobalHookActive`).
)");
extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_StopGlobalHook();

DOCUMENT(R"(Determines if the global hook is active or not.

This function can only be called if global hooking is supported (see :func:`CanGlobalHook`).

:return: ``True`` if the hook is active, or ``False`` if the hook is inactive.
:rtype: bool
)");
extern "C" RENDERDOC_API bool RENDERDOC_CC RENDERDOC_IsGlobalHookActive();

DOCUMENT(R"(Determines if the global hook is supported on the current platform and configuration.

:return: ``True`` if global hooking can be used on the platform, ``False`` if not.
:rtype: bool
)");
extern "C" RENDERDOC_API bool RENDERDOC_CC RENDERDOC_CanGlobalHook();

DOCUMENT(R"(Launch an application and inject into it to allow capturing.

:param str app: The path to the application to run.
:param str workingDir: The working directory to use when running the application. If blank, the
  directory containing the application is used.
:param str cmdLine: The command line to use when running the application, it will be processed in a
  platform specific way to generate arguments.
:param List[EnvironmentModification] env: Any environment changes that should be made when running
  the program.
:param str capturefile: The capture file path template, or blank to use a default location.
:param CaptureOptions opts: The capture options to use when injecting into the program.
:param bool waitForExit: If ``True`` this function will block until the process exits.
:return: The :class:`ExecuteResult` indicating both the status of the operation (success or failure)
  and any reason for failure, or else the ident where the new application is listening for target
  control if everything succeeded.
:rtype: ExecuteResult
)");
extern "C" RENDERDOC_API ExecuteResult RENDERDOC_CC
RENDERDOC_ExecuteAndInject(const rdcstr &app, const rdcstr &workingDir, const rdcstr &cmdLine,
                           const rdcarray<EnvironmentModification> &env, const rdcstr &capturefile,
                           const CaptureOptions &opts, bool waitForExit);

DOCUMENT(R"(Where supported by operating system and permissions, inject into a running process.

:param int pid: The Process ID (PID) to inject into.
:param List[EnvironmentModification] env: Any environment changes that should be made when running
  the program.
:param str capturefile: The capture file path template, or blank to use a default location.
:param CaptureOptions opts: The capture options to use when injecting into the program.
:param bool waitForExit: If ``True`` this function will block until the process exits.
:return: The :class:`ExecuteResult` indicating both the status of the operation (success or failure)
  and any reason for failure, or else the ident where the new application is listening for target
  control if everything succeeded.
:rtype: ExecuteResult
)");
extern "C" RENDERDOC_API ExecuteResult RENDERDOC_CC
RENDERDOC_InjectIntoProcess(uint32_t pid, const rdcarray<EnvironmentModification> &env,
                            const rdcstr &capturefile, const CaptureOptions &opts, bool waitForExit);

DOCUMENT(R"(When debugging RenderDoc it can be useful to capture itself by doing a side-build with a
temporary name. This function wraps up the use of the in-application API to start a capture.

:param str dllname: The name of the self-hosted capture module.
)");
extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_StartSelfHostCapture(const rdcstr &dllname);

DOCUMENT(R"(When debugging RenderDoc it can be useful to capture itself by doing a side-build with a
temporary name. This function wraps up the use of the in-application API to end a capture.

:param str dllname: The name of the self-hosted capture module.
)");
extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_EndSelfHostCapture(const rdcstr &dllname);

//////////////////////////////////////////////////////////////////////////
// Vulkan layer handling
//////////////////////////////////////////////////////////////////////////

DOCUMENT("INTERNAL: Information about vulkan layer registration");
struct VulkanLayerRegistrationInfo
{
  DOCUMENT(":class:`VulkanLayerFlags` detailing the current registration.");
  VulkanLayerFlags flags;

  DOCUMENT("A list of jsons that should be registered");
  rdcarray<rdcstr> myJSONs;

  DOCUMENT("A list of jsons that should be unregistered / updated");
  rdcarray<rdcstr> otherJSONs;
};

DOCUMENT("INTERNAL: Determine vulkan layer registration status.");
extern "C" RENDERDOC_API bool RENDERDOC_CC
RENDERDOC_NeedVulkanLayerRegistration(VulkanLayerRegistrationInfo *info);

DOCUMENT("INTERNAL: Update vulkan layer registration.");
extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_UpdateVulkanLayerRegistration(bool systemLevel);

//////////////////////////////////////////////////////////////////////////
// Miscellaneous!
//////////////////////////////////////////////////////////////////////////

#if !defined(SWIG)
DOCUMENT("INTERNAL: Update installed version number in windows registry.");
extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_UpdateInstalledVersionNumber();
#endif

DOCUMENT(R"(Initialises RenderDoc for replay. Replay API functions should not be called before this
has been called. It should be called exactly once, and before shutdown you must call
:func:`ShutdownReplay`.

:param GlobalEnvironment globalEnv: The path to the new log file.
:param List[str] args: Any extra command-line arguments.
)");
extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_InitialiseReplay(GlobalEnvironment globalEnv,
                                                                      const rdcarray<rdcstr> &args);

DOCUMENT(R"(Shutdown RenderDoc for replay. Replay API functions should not be called after this
has been called. It is not safe to re-initialise replay after this function has been called so it
should only be called at program shutdown. This function must only be called if
:func:`InitialiseReplay` was previously called.
)");
extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_ShutdownReplay();

#if !defined(SWIG)
DOCUMENT("INTERNAL: Create a bug report zip.");
extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_CreateBugReport(const rdcstr &logfile,
                                                                     const rdcstr &dumpfile,
                                                                     rdcstr &report);

DOCUMENT("INTERNAL: Register a memory region to be saved with crash dumps.");
extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_RegisterMemoryRegion(void *base, size_t size);

DOCUMENT("INTERNAL: Unregister a memory region to be saved with crash dumps.");
extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_UnregisterMemoryRegion(void *base);
#endif

DOCUMENT(R"(Sets the location for the diagnostic log output, shared by captured programs and the
analysis program.

:param str filename: The path to the new log file.
)");
extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_SetDebugLogFile(const rdcstr &filename);

DOCUMENT(R"(Gets the location for the diagnostic log output, shared by captured programs and the
analysis program.

:return: The path to the current log file.
:rtype: str
)");
extern "C" RENDERDOC_API const char *RENDERDOC_CC RENDERDOC_GetLogFile();

#if !defined(SWIG)
DOCUMENT("INTERNAL: Atomically fetch the contents of the log");
extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_GetLogFileContents(uint64_t offset,
                                                                        rdcstr &logfile);
#endif

DOCUMENT(R"(Add a message to RenderDoc's logfile.

:param LogType type: The type of the log message. Error messages will trigger a debugger breakpoint
  if a debugger is attached, and fatal errors will kill the process after logging.
:param str project: A short project tag, which should be uppercase and either 3 or 4 characters.
:param str file: The file where this log message came from.
:param int line: The line number in :paramref:`file` where this log message came from.
:param str text: The text of the message.
)");
extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_LogMessage(LogType type, const rdcstr &project,
                                                                const rdcstr &file, uint32_t line,
                                                                const rdcstr &text);

DOCUMENT(R"(Retrieves the version string.

This will be in the form "MAJOR.MINOR"

:return: The version string.
:rtype: str
)");
extern "C" RENDERDOC_API const char *RENDERDOC_CC RENDERDOC_GetVersionString();

DOCUMENT(R"(Determines if this is a release build of RenderDoc or not.

:return: ``True`` if the replay is running on a release build.
:rtype: bool
)");
extern "C" RENDERDOC_API bool RENDERDOC_CC RENDERDOC_IsReleaseBuild();

DOCUMENT(R"(Retrieves the commit hash used to build.

This will be in the form "0123456789abcdef0123456789abcdef01234567"

:return: The commit hash.
:rtype: str
)");
extern "C" RENDERDOC_API const char *RENDERDOC_CC RENDERDOC_GetCommitHash();

DOCUMENT(R"(Retrieves the driver information (if available) for a given graphics API.

:param GraphicsAPI api: The API to get driver information for.
:return: A :class:`DriverInformation` containing the driver information.
:rtype: DriverInformation
)");
extern "C" RENDERDOC_API DriverInformation RENDERDOC_CC RENDERDOC_GetDriverInformation(GraphicsAPI api);

DOCUMENT(R"(Returns the current process's memory usage in bytes

:return: The current memory usage in bytes.
:rtype: int
)");
extern "C" RENDERDOC_API uint64_t RENDERDOC_CC RENDERDOC_GetCurrentProcessMemoryUsage();

DOCUMENT(R"(Return a read-only handle to the :class:`SDObject` corresponding to a given setting's
value object.

If an empty string is passed, the root object is returned containing all settings and setting
categories. Categories contain other categories and settings, settings contain children that include
the setting's value, description, etc.

If no such setting exists, `None` is returned.

:param str name: The name of the setting.
:return: The specified setting.
:rtype: SDObject
)");
extern "C" RENDERDOC_API const SDObject *RENDERDOC_CC RENDERDOC_GetConfigSetting(const rdcstr &name);

DOCUMENT(R"(Return a mutable handle to the :class:`SDObject` corresponding to a given setting's
value object.

If no such setting exists, `None` is returned.

:param str name: The name of the setting.
:return: The specified setting.
:rtype: SDObject
)");
extern "C" RENDERDOC_API SDObject *RENDERDOC_CC RENDERDOC_SetConfigSetting(const rdcstr &name);

DOCUMENT(R"(Flush the current config settings as they are in memory to the config file on disk.

Without calling this function, settings changes will only be temporary. The settings are **not**
saved to disk on exit implicitly.
)");
extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_SaveConfigSettings();

DOCUMENT(R"(Configure the default colours used for checkerboards, this can broadly speaking help
match the replay rendering to the overall theme of the replay application.

:param FloatVector darkChecker: The color of dark squares in checkerboard patterns.
:param FloatVector lightChecker: The color of light squares in checkerboard patterns.
:param bool darkTheme: ``True`` if the theme is a 'dark' theme, used to pick different contrasting
  colors. ``False`` if the theme is 'light' and normal colors are used.
)");
extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_SetColors(FloatVector darkChecker,
                                                               FloatVector lightChecker,
                                                               bool darkTheme);

DOCUMENT("INTERNAL: Check remote Android package for requirements");
extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_CheckAndroidPackage(
    const rdcstr &URL, const rdcstr &packageAndActivity, AndroidFlags *flags);

DOCUMENT("An interface for enumerating and controlling remote devices.");
struct IDeviceProtocolController
{
  DOCUMENT(R"(Retrieves the name of this protocol as passed to :func:`GetDeviceProtocolController`.

:return: A string identifying the protocol.
:rtype: str
)");
  virtual rdcstr GetProtocolName() = 0;

  DOCUMENT(R"(Returns a list of devices currently available through the given protocol.

Until a device is enumerated through this function it may not be available for connection through
other methods such as target control or remote server access, even if the device is physically
connected, due to initialisation happening only when enumerated.

The returned string is the hostname of the device, which can be connected via
``protocol://hostname`` with interfaces that take a hostname.

:return: A list of the devices currently available.
:rtype: List[str]
)");
  virtual rdcarray<rdcstr> GetDevices() = 0;

  DOCUMENT(R"(Retrieves the user friendly name of the given device. This may be easier for a user to
correlate to a device than the hostname which may be only a programmatic identifier.

:param str URL: The URL of the device in the form ``protocol://host``, with protocol as returned by
  :func:`GetProtocolName` and host as returned by :func:`GetDevices`.
:return: A string identifying the device.
:rtype: str
)");
  virtual rdcstr GetFriendlyName(const rdcstr &URL) = 0;

  DOCUMENT(R"(Query if the device supports multiple programs running and being captured. If not, the
user can be prompted to close an existing program before a new one is launched.

:param str URL: The URL of the device in the form ``protocol://host``, with protocol as returned by
  :func:`GetProtocolName` and host as returned by :func:`GetDevices`.
:return: ``True`` if the device supports multiple programs, ``False`` otherwise.
:rtype: bool
)");
  virtual bool SupportsMultiplePrograms(const rdcstr &URL) = 0;

  DOCUMENT(R"(Query if the device supports RenderDoc capture and replay.

:param str URL: The URL of the device in the form ``protocol://host``, with protocol as returned by
  :func:`GetProtocolName` and host as returned by :func:`GetDevices`.
:return: ``True`` if any the device is supported, ``False`` otherwise.
:rtype: bool
)");
  virtual bool IsSupported(const rdcstr &URL) = 0;

  DOCUMENT(R"(Start the remote server running on the given device.

:param str URL: The URL of the device in the form ``protocol://host``, with protocol as returned by
  :func:`GetProtocolName` and host as returned by :func:`GetDevices`.
:return: The status of starting the server, whether success or failure.
:rtype: ResultDetails
)");
  virtual ResultDetails StartRemoteServer(const rdcstr &URL) = 0;

protected:
  IDeviceProtocolController() = default;
  ~IDeviceProtocolController() = default;
};

DOCUMENT(R"(Retrieve the set of device protocols supported (see :func:`GetDeviceProtocolController`).

:return: The supported device protocols.
:rtype: List[str]
)");
extern "C" RENDERDOC_API void RENDERDOC_CC
RENDERDOC_GetSupportedDeviceProtocols(rdcarray<rdcstr> *supportedProtocols);

DOCUMENT(R"(Creates a :class:`DeviceProtocolController` that provides device-specific controls.

This interface is intended to allow closer integration with remote devices.

.. note::
  Note that the use of scripting with Android is explicitly **not supported** due to the inherent
  fragility and unreliability of the Android platform. This interface is designed primarily for
  internal use and no support will be provided for Android-specific problems encountered using this.

This function will not block, however the protocol may still be initialising when it is returned so
immediate use of it may block.

:param str protocol: The protocol to fetch a controller for.
:return: A handle to the protocol controller, or ``None`` if something went wrong such as an
  unsupported protocol being specified.
:rtype: DeviceProtocolController
)");
extern "C" RENDERDOC_API IDeviceProtocolController *RENDERDOC_CC
RENDERDOC_GetDeviceProtocolController(const rdcstr &protocol);

#if !defined(SWIG)
DOCUMENT("INTERNAL: Run unit tests.");
extern "C" RENDERDOC_API int RENDERDOC_CC RENDERDOC_RunUnitTests(const rdcstr &command,
                                                                 const rdcarray<rdcstr> &args);

DOCUMENT("INTERNAL: Run functional tests.");
extern "C" RENDERDOC_API int RENDERDOC_CC RENDERDOC_RunFunctionalTests(int pythonMinorVersion,
                                                                       const rdcarray<rdcstr> &args);
#endif

#if !defined(SWIG)
#include "version.h"

DOCUMENT("INTERNAL: Begin a profile region.");
extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_BeginProfileRegion(const rdcstr &name);

DOCUMENT("INTERNAL: End a profile region.");
extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_EndProfileRegion();

// don't define profile regions in stable builds
#if RENDERDOC_STABLE_BUILD

#define RENDERDOC_PROFILEREGION(name)

#else

struct RENDERDOC_ProfileRegion
{
  RENDERDOC_ProfileRegion(const rdcstr &name) { RENDERDOC_BeginProfileRegion(name); }
  ~RENDERDOC_ProfileRegion() { RENDERDOC_EndProfileRegion(); }
};

#define RENDERDOC_PROFILEREGION(name) RENDERDOC_ProfileRegion profile##__LINE__(name);

#endif

#if defined(RENDERDOC_PLATFORM_WIN32)
#define RENDERDOC_PROFILEFUNCTION() RENDERDOC_PROFILEREGION(__FUNCSIG__);
#else
#define RENDERDOC_PROFILEFUNCTION() RENDERDOC_PROFILEREGION(__PRETTY_FUNCTION__);
#endif

#endif
