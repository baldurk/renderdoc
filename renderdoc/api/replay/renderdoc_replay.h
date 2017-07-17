/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2017 Baldur Karlsson
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

template <typename T>
inline const char *TypeName();

#define DECLARE_REFLECTION_STRUCT(type) \
  template <>                           \
  inline const char *TypeName<type>()   \
  {                                     \
    return #type;                       \
  }

typedef uint8_t byte;
typedef uint32_t bool32;

// Guidelines for documentation:
//
// * If you only need a short string, use DOCUMENT("Here is my string");
// * If your string is only just over the limit by clang-format, allow it to be reformatted and
//   moved to a new line as necessary.
// * If your string is a couple of lines long or a paragraph or more, use raw C++11 string literals
//   like so:
//   R"(Here is my string. It is fairly long so I am going to break the first line over a paragraph
//   boundary like so, so that I have enough room to continue it.
//
//   A second paragraph can be used like so. Note that the first line is right after the opening
//   quotation mark, but the terminating bracket and quote should be on a new line.
//   )"
// * Use :class:`ClassName` to refer to classes, :data:`ClassName.constant` to refer to constants or
//   member variables, and :meth:`ClassName.method` to refer to member functions. You can also link
//   to the external documentation with :ref:`external-ref-name`.
// * For constants like ``None`` or ``True`` use the python term (i.e. ``None`` not ``NULL``) and
//   surround with double backticks ``.
// * Likewise use python types to refer to basic types - ``str``, ``int``, ``float``, etc.
// * All values for enums should be documented in the docstring for the enum itself, you can't
//   document the values. See the examples in replay_enums.h for the syntax
// * Take care not to go too far over 100 columns, if you're using raw C++11 string literals then
//   clang-format won't reformat them into the column limit.
//
#ifndef DOCUMENT
#define DOCUMENT(text)
#endif

#if defined(RENDERDOC_PLATFORM_WIN32)

#ifdef RENDERDOC_EXPORTS
#define RENDERDOC_API __declspec(dllexport)
#else
#define RENDERDOC_API __declspec(dllimport)
#endif
#define RENDERDOC_CC __cdecl

#elif defined(RENDERDOC_PLATFORM_LINUX) || defined(RENDERDOC_PLATFORM_APPLE) || \
    defined(RENDERDOC_PLATFORM_ANDROID)

#ifdef RENDERDOC_EXPORTS
#define RENDERDOC_API __attribute__((visibility("default")))
#else
#define RENDERDOC_API
#endif

#define RENDERDOC_CC

#else

#error "Unknown platform"

#endif

// windowing structures

#if defined(RENDERDOC_PLATFORM_WIN32)

// Win32 uses HWND

#endif

#if defined(RENDERDOC_WINDOWING_XLIB)

// can't include xlib.h here as it defines a ton of crap like None
// and Bool etc which can interfere with other headers
typedef struct _XDisplay Display;
typedef unsigned long Drawable;

struct XlibWindowData
{
  Display *display;
  Drawable window;
};

#else

typedef void *Display;

#endif

#if defined(RENDERDOC_WINDOWING_XCB)

struct xcb_connection_t;
typedef uint32_t xcb_window_t;

struct XCBWindowData
{
  xcb_connection_t *connection;
  xcb_window_t window;
};

#endif

#if defined(RENDERDOC_PLATFORM_ANDROID)

// android uses ANativeWindow*

#endif

DOCUMENT(R"(Specifies a windowing system to use for creating an output window.

.. data:: Unknown

  No windowing data is passed and no native window will be output to.

.. data:: Win32

  The windowing data refers to a Win32 ``HWND`` handle.

.. data:: Xlib

  The windowing data refers to an Xlib pair of ``Display *`` and ``Drawable``.

.. data:: XCB

  The windowing data refers to an XCB pair of ``xcb_connection_t *`` and ``xcb_window_t``.

.. data:: Android

  The windowing data refers to an Android ``ANativeWindow *``.
)");
enum class WindowingSystem : uint32_t
{
  Unknown,
  Win32,
  Xlib,
  XCB,
  Android,
};

DOCUMENT(R"(Internal structure used for initialising environment in a replay application.)");
struct GlobalEnvironment
{
  DOCUMENT("The handle to the X display to use internally. If left ``NULL``, one will be opened.");
  Display *xlibDisplay = NULL;
};

// needs to be declared up here for reference in basic_types

extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_FreeArrayMem(const void *mem);
typedef void(RENDERDOC_CC *pRENDERDOC_FreeArrayMem)(const void *mem);

extern "C" RENDERDOC_API void *RENDERDOC_CC RENDERDOC_AllocArrayMem(uint64_t sz);
typedef void *(RENDERDOC_CC *pRENDERDOC_AllocArrayMem)(uint64_t sz);

#include "basic_types.h"

#ifdef RENDERDOC_EXPORTS
struct ResourceId;

namespace ResourceIDGen
{
// the only function allowed access to ResourceId internals, for allocating a new ID
ResourceId GetNewUniqueID();
};
#endif

// We give every resource a globally unique ID so that we can differentiate
// between two textures allocated in the same memory (after the first is freed)
//
// it's a struct around a uint64_t to aid in template selection
DOCUMENT(R"(This is an opaque identifier that uniquely locates a resource.

.. note::
  These IDs do not overlap ever - textures, buffers, shaders and samplers will all have unique IDs
  and do not reuse the namespace. Likewise the IDs assigned for resources during capture  are not
  re-used on replay - the corresponding resources created on replay to stand-in for capture-time
  resources are given unique IDs and a mapping is stored to between the capture-time resource and
  the replay-time one.
)");
struct ResourceId
{
  ResourceId() : id() {}
  DOCUMENT("A helper function that explicitly creates an empty/invalid/null :class:`ResourceId`.");
  inline static ResourceId Null() { return ResourceId(); }
  DOCUMENT("Compares two ``ResourceId`` objects for equality.");
  bool operator==(const ResourceId u) const { return id == u.id; }
  DOCUMENT("Compares two ``ResourceId`` objects for inequality.");
  bool operator!=(const ResourceId u) const { return id != u.id; }
  DOCUMENT("Compares two ``ResourceId`` objects for less-than.");
  bool operator<(const ResourceId u) const { return id < u.id; }
private:
  uint64_t id;

#ifdef RENDERDOC_EXPORTS
  friend ResourceId ResourceIDGen::GetNewUniqueID();
#endif
};

DECLARE_REFLECTION_STRUCT(ResourceId);

#include "capture_options.h"
#include "control_types.h"
#include "d3d11_pipestate.h"
#include "d3d12_pipestate.h"
#include "data_types.h"
#include "gl_pipestate.h"
#include "replay_enums.h"
#include "shader_types.h"
#include "vk_pipestate.h"

DOCUMENT(R"(A stateful output handle that contains the current configuration for one particular view
of the capture. This allows multiple outputs to run independently without interfering with each
other.

The different types are enumerated in :class:`ReplayOutputType`.

.. data:: NoResult

  No result was found in e.g. :meth:`PickVertex`.
)");
struct IReplayOutput
{
  DOCUMENT("Sets the :class:`TextureDisplay` configuration for a texture output.");
  virtual void SetTextureDisplay(const TextureDisplay &o) = 0;

  DOCUMENT("Sets the :class:`MeshDisplay` configuration for a mesh output.");
  virtual void SetMeshDisplay(const MeshDisplay &o) = 0;

  DOCUMENT(
      "Clear and release all thumbnails associated with this output. See :meth:`AddThumbnail`.");
  virtual void ClearThumbnails() = 0;

  DOCUMENT(R"(Sets up a thumbnail for displaying a particular texture with sensible defaults.

The window handle specified will be filled (in an aspect-ratio preserving way) with the texture.

If the window specified has been used for a thumbnail before, then the texture will be updated but
otherwise nothing will be created and the existing internal data will be reused. This means that
you can call this function multiple times to just change the texture.

Should only be called for texture outputs.

:param WindowingSystem system: The type of native window handle data being provided.
:param data: The native window data, in a format defined by the system.
:type data: opaque void * pointer.
:param ResourceId texID: The texture ID to display in the thumbnail preview.
:return: A boolean indicating if the thumbnail was successfully created.
:rtype: ``bool``
)");
  virtual bool AddThumbnail(WindowingSystem system, void *data, ResourceId texID,
                            CompType typeHint) = 0;

  DOCUMENT(R"(Render to the window handle specified when the output was created.

This will also render any thumbnails and the pixel context, if enabled.
)");
  virtual void Display() = 0;

  DOCUMENT(R"(Sets up a zoomed in pixel context view around a particular pixel selection.

The texture rendering uses the configuration specified in :meth:`SetTextureDisplay` except with a
fixed high zoom value and a fixed position, see :meth:`SetPixelContextLocation`.

Should only be called for texture outputs.

:param WindowingSystem system: The type of native window handle data being provided.
:param data: The native window data, in a format defined by the system.
:type data: opaque void * pointer.
:return: A boolean indicating if the pixel context was successfully configured.
:rtype: ``bool``
)");
  virtual bool SetPixelContext(WindowingSystem system, void *data) = 0;

  DOCUMENT(R"(Sets the pixel that the pixel context should be centred on.

Should only be called for texture outputs.
)");
  virtual void SetPixelContextLocation(uint32_t x, uint32_t y) = 0;

  DOCUMENT("Disable the pixel context view from rendering.");
  virtual void DisablePixelContext() = 0;

  DOCUMENT(R"(Retrieves the minimum and maximum values in the current texture.

Should only be called for texture outputs.

:return: A tuple with the minimum and maximum pixel values respectively.
:rtype: ``tuple`` of PixelValue and PixelValue
)");
  virtual rdctype::pair<PixelValue, PixelValue> GetMinMax() = 0;

  DOCUMENT(R"(Retrieve a list of values that can be used to show a histogram of values for the
current texture.

The output list contains N buckets, and each bucket has the number of pixels that falls in each
bucket when the pixel values are divided between ``minval`` and ``maxval``.

Should only be called for texture outputs.

:param float minval: The lower end of the smallest bucket. If any values are below this, they are
  not added to any bucket.
:param float maxval: The upper end of the largest bucket. If any values are above this, they are
  not added to any bucket.
:param list channels: A list of four ``bool`` values indicating whether each of RGBA should be
  included in the count.
:return: A list of the unnormalised bucket values.
:rtype: ``list`` of ``int``
)");
  virtual rdctype::array<uint32_t> GetHistogram(float minval, float maxval, bool channels[4]) = 0;

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

  DOCUMENT(R"(Retrieve the contents of a particular pixel in a texture.

Should only be called for texture outputs.

:param ResourceId texID: The texture to pick the pixel from.
:param bool customShader: Whether to apply the configured custom shader.
:param int x: The x co-ordinate to pick from.
:param int y: The y co-ordinate to pick from.
:param int sliceFace: The slice of an array or 3D texture, or face of a cubemap texture.
:param int mip: The mip level to pick from.
:param int sample: The multisample sample to pick from.
:return: The contents of the pixel.
:rtype: PixelValue
)");
  virtual PixelValue PickPixel(ResourceId texID, bool customShader, uint32_t x, uint32_t y,
                               uint32_t sliceFace, uint32_t mip, uint32_t sample) = 0;

  DOCUMENT(R"(Retrieves the vertex and instance that is under the cursor location, when viewed
relative to the current window with the current mesh display configuration.

Should only be called for mesh outputs.

:param int eventID: The event ID to pick at.
:param int x: The x co-ordinate to pick from.
:param int y: The y co-ordinate to pick from.
:return: A tuple with the first value being the vertex index in the mesh, and the second value being
  the instance index. The values are set to :data:`NoResult` if no vertex was found, 
:rtype: ``tuple`` of ``int`` and ``int``
)");
  virtual rdctype::pair<uint32_t, uint32_t> PickVertex(uint32_t eventID, uint32_t x, uint32_t y) = 0;

  static const uint32_t NoResult = ~0U;

protected:
  IReplayOutput() = default;
  ~IReplayOutput() = default;
};

DOCUMENT(R"(The primary interface to access the information in a capture and the current state, as
well as control the replay and analysis functionality available.

.. data:: NoPreference

  No preference for a particular value, see :meth:`DebugPixel`.
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
:rtype: ``list`` of :class:`WindowingSystem`
)");
  virtual rdctype::array<WindowingSystem> GetSupportedWindowSystems() = 0;

  DOCUMENT(R"(Creates a replay output of the given type to the given native window

:param WindowingSystem system: The type of native window handle data being provided
:param data: The native window data, in a format defined by the system
:type data: opaque void * pointer
:param ReplayOutputType type: What type of output to create
:return: A handle to the created output, or ``None`` on failure
:rtype: ReplayOutput
)");
  virtual IReplayOutput *CreateOutput(WindowingSystem system, void *data, ReplayOutputType type) = 0;

  DOCUMENT("Shutdown and destroy the current interface and all outputs that have been created.");
  virtual void Shutdown() = 0;

  DOCUMENT(R"(Shutdown a particular output.

:param ReplayOutput output: The output to shut down.
)");
  virtual void ShutdownOutput(IReplayOutput *output) = 0;

  DOCUMENT("Notify the interface that the file it has open has been changed on disk.");
  virtual void FileChanged() = 0;

  DOCUMENT(R"(Query if per-event or per-draw callstacks are available in this capture.

:return: ``True`` if any callstacks are available, ``False`` otherwise.
:rtype: ``bool``
)");
  virtual bool HasCallstacks() = 0;

  DOCUMENT(R"(Begin initialising a callstack resolver, looking up symbol files and caching as
necessary.

This function will eventually return true if either the resolver successfully initialises, or if it
comes to a point where a problem is encountered that the user cannot solve. That means this can be
used to present a progress dialog and repeatedly queried to see when to allow the user to continue.

:return: ``True`` if any callstacks are available, ``False`` otherwise.
:rtype: ``bool``
)");
  virtual bool InitResolver() = 0;

  DOCUMENT(R"(Move the replay to reflect the state immediately *after* the given
:data:`EID <APIEvent.eventID>`.

:param int eventID: The :data:`EID <APIEvent.eventID>` to move to.
:param bool force: ``True`` if the internal replay should refresh even if the ``eventID`` is
  already current. This can be useful if external factors might cause the replay to vary.
)");
  virtual void SetFrameEvent(uint32_t eventID, bool force) = 0;

  DOCUMENT(R"(Retrieve the current :class:`D3D11_State` pipeline state.

This pipeline state will be filled with default values if the capture is not using the D3D11 API.
You should use :meth:`GetAPIProperties` to determine the API of the capture.

:return: The current D3D11 pipeline state.
:rtype: D3D11_State
)");
  virtual D3D11Pipe::State GetD3D11PipelineState() = 0;

  DOCUMENT(R"(Retrieve the current :class:`D3D12_State` pipeline state.

This pipeline state will be filled with default values if the capture is not using the D3D12 API.
You should use :meth:`GetAPIProperties` to determine the API of the capture.

:return: The current D3D12 pipeline state.
:rtype: D3D12_State
)");
  virtual D3D12Pipe::State GetD3D12PipelineState() = 0;

  DOCUMENT(R"(Retrieve the current :class:`GL_State` pipeline state.

This pipeline state will be filled with default values if the capture is not using the OpenGL API.
You should use :meth:`GetAPIProperties` to determine the API of the capture.

:return: The current OpenGL pipeline state.
:rtype: GL_State
)");
  virtual GLPipe::State GetGLPipelineState() = 0;

  DOCUMENT(R"(Retrieve the current :class:`VK_State` pipeline state.

This pipeline state will be filled with default values if the capture is not using the Vulkan API.
You should use :meth:`GetAPIProperties` to determine the API of the capture.

:return: The current Vulkan pipeline state.
:rtype: VK_State
)");
  virtual VKPipe::State GetVulkanPipelineState() = 0;

  DOCUMENT(R"(Retrieve the list of possible disassembly targets for :meth:`DisassembleShader`. The
values are implementation dependent but will always include a default target first which is the
native disassembly of the shader. Further options may be available for additional diassembly views
or hardware-specific ISA formats.

:return: The list of disassembly targets available.
:rtype: ``list`` of ``str``
)");
  virtual rdctype::array<rdctype::str> GetDisassemblyTargets() = 0;

  DOCUMENT(R"(Retrieve the disassembly for a given shader, for the given disassembly target.

:param ShaderReflection refl: The shader reflection details of the shader to disassemble
:param str target: The name of the disassembly target to generate for. Must be one of the values
  returned by :meth:`GetDisassemblyTargets`, or empty to use the default generation.
:return: The disassembly text, or an error message if something went wrong.
:rtype: str
)");
  virtual rdctype::str DisassembleShader(const ShaderReflection *refl, const char *target) = 0;

  DOCUMENT(R"(Builds a shader suitable for running on the local replay instance as a custom shader.

The language used is native to the local renderer - HLSL for D3D based renderers, GLSL otherwise.

See :data:`TextureDisplay.CustomShader`.

:param str entry: The entry point to use when compiling.
:param str source: The source file.
:param int compileFlags: API-specific compilation flags.
:param ShaderStage type: The stage that this shader will be executed at.
:return: A ``tuple`` with the id of the new shader if compilation was successful,
  :meth:`ResourceId.Null` otherwise, and a ``str`` with any warnings/errors from compilation.
:rtype: ``tuple`` of :class:`ResourceId` and ``str``.
)");
  virtual rdctype::pair<ResourceId, rdctype::str> BuildCustomShader(const char *entry,
                                                                    const char *source,
                                                                    const uint32_t compileFlags,
                                                                    ShaderStage type) = 0;

  DOCUMENT(R"(Free a previously created custom shader.

See :meth:`BuildCustomShader`.

:param ResourceId id: The id of the custom shader to free.
)");
  virtual void FreeCustomShader(ResourceId id) = 0;

  DOCUMENT(R"(Builds a shader suitable for running in the capture's API as a replacement shader.

The language used is native to the API's renderer - HLSL for D3D based renderers, GLSL otherwise.

:param str entry: The entry point to use when compiling.
:param str source: The source file.
:param int compileFlags: API-specific compilation flags.
:param ShaderStage type: The stage that this shader will be executed at.
:return: A ``tuple`` with the id of the new shader if compilation was successful,
  :meth:`ResourceId.Null` otherwise, and a ``str`` with any warnings/errors from compilation.
:rtype: ``tuple`` of :class:`ResourceId` and ``str``.
)");
  virtual rdctype::pair<ResourceId, rdctype::str> BuildTargetShader(const char *entry,
                                                                    const char *source,
                                                                    const uint32_t compileFlags,
                                                                    ShaderStage type) = 0;

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

  DOCUMENT(R"(Retrieve the list of root-level drawcalls in the capture.

:return: The list of root-level drawcalls in the capture.
:rtype: ``list`` of :class:`DrawcallDescription`
)");
  virtual rdctype::array<DrawcallDescription> GetDrawcalls() = 0;

  DOCUMENT(R"(Retrieve the values of a specified set of counters.

:param list counters: The list of :class:`GPUCounter` to fetch results for.
:return: The list of counter results generated.
:rtype: ``list`` of :class:`CounterResult`
)");
  virtual rdctype::array<CounterResult> FetchCounters(const rdctype::array<GPUCounter> &counters) = 0;

  DOCUMENT(R"(Retrieve a list of which counters are available in the current capture analysis
implementation.

:return: The list of counters available.
:rtype: ``list`` of :class:`GPUCounter`
)");
  virtual rdctype::array<GPUCounter> EnumerateCounters() = 0;

  DOCUMENT(R"(Get information about what a counter actually represents, in terms of a human-readable
understanding as well as the type and unit of the resulting information.

:param GPUCounter counterID: The counter to query about.
:return: The description of the counter.
:rtype: CounterDescription
)");
  virtual CounterDescription DescribeCounter(GPUCounter counterID) = 0;

  DOCUMENT(R"(Retrieve the list of textures alive in the capture.

:return: The list of textures in the capture.
:rtype: ``list`` of :class:`TextureDescription`
)");
  virtual rdctype::array<TextureDescription> GetTextures() = 0;

  DOCUMENT(R"(Retrieve the list of buffers alive in the capture.

:return: The list of buffers in the capture.
:rtype: ``list`` of :class:`BufferDescription`
)");
  virtual rdctype::array<BufferDescription> GetBuffers() = 0;

  DOCUMENT(R"(Retrieve the list of buffers alive in the capture.

Must only be called after :meth:`InitResolver` has returned ``True``.

:param list callstack: The integer addresses in the original callstack.
:return: The list of resolved callstack entries as strings.
:rtype: ``list`` of ``str``
)");
  virtual rdctype::array<rdctype::str> GetResolve(const rdctype::array<uint64_t> &callstack) = 0;

  DOCUMENT(R"(Retrieve a list of any newly generated diagnostic messages.

Every time this function is called, any debug messages returned will not be returned again. Only
newly generated messages will be returned after that.

:return: The list of the :class:`DebugMessage` messages.
:rtype: ``list`` of :class:`DebugMessage`
)");
  virtual rdctype::array<DebugMessage> GetDebugMessages() = 0;

  DOCUMENT(R"(Retrieve the history of modifications to the selected pixel on the selected texture.

:param ResourceId texture: The texture to search for modifications.
:param int x: The x co-ordinate.
:param int y: The y co-ordinate.
:param int slice: The slice of an array or 3D texture, or face of a cubemap texture.
:param int mip: The mip level to pick from.
:param int sampleIdx: The multi-sampled sample. Ignored if non-multisampled texture.
:param CompType typeHint: A hint on how to interpret textures that are typeless.
:return: The list of pixel history events.
:rtype: ``list`` of :class:`PixelModification`
)");
  virtual rdctype::array<PixelModification> PixelHistory(ResourceId texture, uint32_t x, uint32_t y,
                                                         uint32_t slice, uint32_t mip,
                                                         uint32_t sampleIdx, CompType typeHint) = 0;

  DOCUMENT(R"(Retrieve a debugging trace from running a vertex shader.

:param int vertid: The vertex ID as a 0-based index up to the number of vertices in the draw.
:param int instid: The instance ID as a 0-based index up to the number of instances in the draw.
:param int idx: The actual index used to look up vertex inputs, either from the vertex ID for non-
  indexed draws or drawn from the index buffer. This must have all drawcall offsets applied.
:param int instOffset: The value from :data:`DrawcallDescription.instanceOffset`.
:param int vertOffset: The value from :data:`DrawcallDescription.vertexOffset`.
:return: The resulting trace resulting from debugging. Destroy with
  :meth:`FreeTrace`.
:rtype: ShaderDebugTrace
)");
  virtual ShaderDebugTrace *DebugVertex(uint32_t vertid, uint32_t instid, uint32_t idx,
                                        uint32_t instOffset, uint32_t vertOffset) = 0;

  DOCUMENT(R"(Retrieve a debugging trace from running a pixel shader.

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

:param groupid: A list containing the 3D workgroup index.
:param threadid: A list containing the 3D thread index within the above workgroup.
:return: The resulting trace resulting from debugging. Destroy with
  :meth:`FreeTrace`.
:rtype: ShaderDebugTrace
)");
  virtual ShaderDebugTrace *DebugThread(const uint32_t groupid[3], const uint32_t threadid[3]) = 0;

  DOCUMENT(R"(Free a debugging trace from running a shader invocation debug.

:param ShaderDebugTrace trace: The shader debugging trace to free.
)");
  virtual void FreeTrace(ShaderDebugTrace *trace) = 0;

  DOCUMENT(R"(Retrieve a list of ways a given resource is used.

:param ResourceId id: The id of the texture or buffer resource to be queried.
:return: The list of usages of the resource.
:rtype: ``list`` of :class:`EventUsage`
)");
  virtual rdctype::array<EventUsage> GetUsage(ResourceId id) = 0;

  DOCUMENT(R"(Retrieve the contents of a constant block by reading from memory or their source
otherwise.

:param ResourceId shader: The id of the shader to use for metadata.
:param str entryPoint: The entry point of the shader being used. In some APIs, this is ignored.
:param int cbufslot: The index in the :data:`ShaderReflection.ConstantBlocks` list to look up.
:param ResourceId buffer: The id of the buffer to use for data. If
  :data:`ConstantBlock.bufferBacked` is ``False`` this is ignored.
:param int offs: Retrieve buffer contents starting at this byte offset.
:return: The shader variables with their contents.
:rtype: ``list`` of :class:`ShaderVariable`
)");
  virtual rdctype::array<ShaderVariable> GetCBufferVariableContents(ResourceId shader,
                                                                    const char *entryPoint,
                                                                    uint32_t cbufslot,
                                                                    ResourceId buffer,
                                                                    uint64_t offs) = 0;

  DOCUMENT(R"(Save a texture to a file on disk, with possible transformation to map a complex
texture to something compatible with the target file format.

:param TextureSave saveData: The configuration settings of which texture to save, and how
:param str path: The path to save to on disk.
:return: ``True`` if the texture was saved successfully, ``False`` otherwise.
:rtype: ``bool``
)");
  virtual bool SaveTexture(const TextureSave &saveData, const char *path) = 0;

  DOCUMENT(R"(Retrieve the generated data from one of the geometry processing shader stages.

:param int instID: The index of the instance to retrieve data for.
:param MeshDataStage stage: The stage of the geometry processing pipeline to retrieve data from.
:return: The information describing where the post-transform data is stored.
:rtype: MeshFormat
)");
  virtual MeshFormat GetPostVSData(uint32_t instID, MeshDataStage stage) = 0;

  DOCUMENT(R"(Retrieve the contents of a range of a buffer as a ``bytes``.

:param ResourceId buff: The id of the buffer to retrieve data from.
:param int offset: The byte offset to the start of the range.
:param int len: The length of the range, or 0 to retrieve the rest of the bytes in the buffer.
:return: The requested buffer contents.
:rtype: ``bytes``
)");
  virtual rdctype::array<byte> GetBufferData(ResourceId buff, uint64_t offset, uint64_t len) = 0;

  DOCUMENT(R"(Retrieve the contents of one subresource of a texture as a ``bytes``.

For multi-sampled images, they are treated as if they are an array that is Nx longer, with each
array slice being expanded in-place so it would be slice 0: sample 0, slice 0: sample 1, slice 1:
sample 0, etc.

:param ResourceId tex: The id of the texture to retrieve data from.
:param int arrayIdx: The slice of an array or 3D texture, or face of a cubemap texture.
:param int mip: The mip level to pick from.
:return: The requested texture contents.
:rtype: ``bytes``
)");
  virtual rdctype::array<byte> GetTextureData(ResourceId tex, uint32_t arrayIdx, uint32_t mip) = 0;

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
:rtype: ``bool``
)");
  virtual bool Connected() = 0;

  DOCUMENT(R"(Retrieves the target's name or identifier - typically the name of the executable.

:return: The target name.
:rtype: ``str``
)");
  virtual const char *GetTarget() = 0;

  DOCUMENT(R"(Retrieves the API currently in use by the target.

:return: The API name, or empty if no API is initialised yet.
:rtype: ``str``
)");
  virtual const char *GetAPI() = 0;

  DOCUMENT(R"(Retrieves the Process ID (PID) of the target on its local system.

:return: The Process ID, or 0 if that's not applicable on the target platform.
:rtype: ``int``
)");
  virtual uint32_t GetPID() = 0;

  DOCUMENT(R"(If a busy message was received, determine the client keeping the target busy.

:return: The name of the client currently connected to the target.
:rtype: ``str``
)");
  virtual const char *GetBusyClient() = 0;

  DOCUMENT(R"(Trigger a capture on the target, with the same semantics as if the capture key had
been pressed - from the next presentation call after this message is processed on the target to the
next after that.

:param int numFrames: How many frames to capture. These will be captured sequentially and
  independently to separate files.
)");
  virtual void TriggerCapture(uint32_t numFrames) = 0;

  DOCUMENT(R"(Queue up a capture to happen on a particular frame number. When this frame is about to
begin a capture is begun, and it ends when this frame number ends.

:param int frameNumber: The number of the frame to capture on.
)");
  virtual void QueueCapture(uint32_t frameNumber) = 0;

  DOCUMENT(R"(Begin copying a given capture stored on a remote machine to the local machine over the
target control connection.

:param int remoteID: The identifier of the remote capture.
:param str localpath: The absolute path on the local system where the file should be saved.
)");
  virtual void CopyCapture(uint32_t remoteID, const char *localpath) = 0;

  DOCUMENT(R"(Delete a capture from the remote machine.

:param int remoteID: The identifier of the remote capture.
)");
  virtual void DeleteCapture(uint32_t remoteID) = 0;

  DOCUMENT(R"(Query to see if a message has been received from the remote system.

The details of the types of messages that can be received are listed under
:class:`TargetControlMessage`.

.. note:: If no message has been received, this function will pump the connection. You are expected
  to continually call this function and process any messages to kee pthe connection alive.

  This function will block but only to a limited degree. If no message is waiting after a small time
  it will return with a No-op message to allow further processing.

:return: The message that was received.
:rtype: TargetControlMessage
)");
  virtual TargetControlMessage ReceiveMessage() = 0;

protected:
  ITargetControl() = default;
  ~ITargetControl() = default;
};

DOCUMENT(R"(A connection to a running remote RenderDoc server on another machine. This allows the
transfer of captures to and from the local machine, as well as remotely replaying a capture with a
local proxy renderer, so that captures that are not supported locally can still be debugged with as
much work as possible happening on the local machine.

.. data:: NoPreference

  No preference for a particular value, see :meth:`DebugPixel`.
)");
struct IRemoteServer
{
  DOCUMENT("Closes the connection without affecting the running server.");
  virtual void ShutdownConnection() = 0;

  DOCUMENT("Closes the connection and also tells the running server to close.");
  virtual void ShutdownServerAndConnection() = 0;

  DOCUMENT(R"(Pings the remote server to ensure the connection is still alive.

:return: ``True`` if the ping was sent and received successfully, ``False`` if something went wrong
  and the connection is no longer alive.
:rtype: ``bool``
)");
  virtual bool Ping() = 0;

  DOCUMENT(R"(Retrieve a list of renderers available for local proxying.

These will be strings like "D3D11" or "OpenGL".

:return: A list of names of the local proxies.
:rtype: ``list`` of ``str``
)");
  virtual rdctype::array<rdctype::str> LocalProxies() = 0;

  DOCUMENT(R"(Retrieve a list of renderers supported by the remote server.

These will be strings like "D3D11" or "OpenGL".

:return: A list of names of the remote renderers.
:rtype: ``list`` of ``str``
)");
  virtual rdctype::array<rdctype::str> RemoteSupportedReplays() = 0;

  DOCUMENT(R"(Retrieve the path on the remote system where browsing can begin.

:return: The 'home' path where browsing for files or folders can begin.
:rtype: ``str``
)");
  virtual rdctype::str GetHomeFolder() = 0;

  DOCUMENT(R"(Retrieve the contents of a folder path on the remote system.

If an error occurs, a single :class:`PathEntry` will be returned with appropriate error flags.

:param str path: The remote path to list.
:return: The contents of the specified folder.
:rtype: ``list`` of :class:`PathEntry`
)");
  virtual rdctype::array<PathEntry> ListFolder(const char *path) = 0;

  DOCUMENT(R"(Launch an application and inject into it to allow capturing.

This happens on the remote system, so all paths are relative to the remote filesystem.

:param str app: The path to the application to run.
:param str workingDir: The working directory to use when running the application. If blank, the
  directory containing the application is used.
:param str cmdLine: The command line to use when running the application, it will be processed in a
  platform specific way to generate arguments.
:param list env: Any :class:`EnvironmentModification` that should be made when running the program.
:param CaptureOptions opts: The capture options to use when injecting into the program.
:return: The ident where the new application is listening for target control, or 0 if something went
  wrong.
:rtype: ``int``
)");
  virtual uint32_t ExecuteAndInject(const char *app, const char *workingDir, const char *cmdLine,
                                    const rdctype::array<EnvironmentModification> &env,
                                    const CaptureOptions &opts) = 0;

  DOCUMENT(R"(Take ownership over a capture file.

Initially when a capture is made, it is owned by the injected library in the application. It passes
ownership to any program that is connected via target control that is notified about the capture,
which is then responsible for either saving the file or deleting it if it's unwanted.

Passing ownership of a file to the remote server means that it will be kept around for future use
until the server closes, at which point it will delete any files it owns.

:param str filename: The remote path to take ownership of.
)");
  virtual void TakeOwnershipCapture(const char *filename) = 0;

  DOCUMENT(R"(Copy a capture file that is stored on the local system to the remote system.

This function will block until the copy is fully complete, or an error has occurred.

This is primarily useful for when a capture is only stored locally and must be replayed remotely, as
the capture must be available on the machine where the replay happens.

:param str filename: The path to the file on the local system.
:param float progress: A reference to a float value that will be updated as the copy happens from
  ``0.0`` to ``1.0``. The parameter can be ``None`` if no progress update is desired.
:return: The path on the remote system where the capture was saved temporarily.
:rtype: ``str``
)");
  virtual rdctype::str CopyCaptureToRemote(const char *filename, float *progress) = 0;

  DOCUMENT(R"(Copy a capture file that is stored on the remote system to the local system.

This function will block until the copy is fully complete, or an error has occurred.

:param str remotepath: The remote path where the file should be copied from.
:param str localpath: The local path where the file should be saved.
:param float progress: A reference to a ``float`` value that will be updated as the copy happens
  from ``0.0`` to ``1.0``. The parameter can be ``None`` if no progress update is desired.
)");
  virtual void CopyCaptureFromRemote(const char *remotepath, const char *localpath,
                                     float *progress) = 0;

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
:param float progress: A reference to a ``float`` value that will be updated as the copy happens
  from ``0.0`` to ``1.0``. The parameter can be ``None`` if no progress update is desired.
:return: A tuple containing the status of opening the capture, whether success or failure, and the
  resulting :class:`ReplayController` handle if successful.
:rtype: ``tuple`` of :class:`ReplayStatus` and :class:`ReplayController`
)");
  virtual rdctype::pair<ReplayStatus, IReplayController *> OpenCapture(uint32_t proxyid,
                                                                       const char *logfile,
                                                                       float *progress) = 0;

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
struct ICaptureFile
{
  DOCUMENT("Closes the file handle.");
  virtual void Shutdown() = 0;

  DOCUMENT(R"(Retrieves the status of the handle.

This returns an error if the capture file used to create the handle wasn't found, or was corrupted,
or something else went wrong while processing it.

:return: The status of the handle to the file.
:rtype: ReplayStatus
)");
  virtual ReplayStatus OpenStatus() = 0;

  DOCUMENT(R"(Retrieves the filename used to open this handle.

This filename is exactly as specified without any modificaton to make it an absolute path.

:return: The filename used to create this handle.
:rtype: ``str``
)");
  virtual const char *Filename() = 0;

  DOCUMENT(R"(Queries for how well a particular capture is supported on the local machine.

:return: How much support for replay exists locally.
:rtype: ReplaySupport
)");
  virtual ReplaySupport LocalReplaySupport() = 0;

  DOCUMENT(R"(Retrieves the name of the driver that was used to create this capture.

:return: A simple string identifying the driver used to make the capture.
:rtype: ``str``
)");
  virtual const char *DriverName() = 0;

  DOCUMENT(R"(Retrieves the identifying string describing what type of machine created this capture.

:return: A string identifying the machine ident used to make the capture.
:rtype: ``str``
)");
  virtual const char *RecordedMachineIdent() = 0;

  DOCUMENT(R"(Opens a capture for replay locally and returns a handle to the capture.

This function will block until the capture is fully loaded and ready.

Once the replay is created, this :class:`CaptureFile` can be shut down, there is no dependency on it
by the :class:`ReplayController`.

:param float progress: A reference to a ``float`` value that will be updated as the copy happens
  from ``0.0`` to ``1.0``. The parameter can be ``None`` if no progress update is desired.
:return: A tuple containing the status of opening the capture, whether success or failure, and the
  resulting :class:`ReplayController` handle if successful.
:rtype: ``tuple`` of :class:`ReplayStatus` and :class:`ReplayController`.
)");
  virtual rdctype::pair<ReplayStatus, IReplayController *> OpenCapture(float *progress) = 0;

  DOCUMENT(R"(Retrieves the embedded thumbnail from the capture.

:param FileType type: The image format to convert the thumbnail to.
:param int maxsize: The largest width or height allowed. If the thumbnail is larger, it's resized.
:return: The raw contents of the thumbnail, converted to the desired type at the desired max
  resolution.
:rtype: ``bytes``.
  )");
  virtual rdctype::array<byte> GetThumbnail(FileType type, uint32_t maxsize) = 0;

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
:rtype: ``float``
)");
extern "C" RENDERDOC_API float RENDERDOC_CC RENDERDOC_HalfToFloat(uint16_t half);

DOCUMENT(R"(A utility function that converts a float to a half (stored in a 16-bit unsigned
integer).

:param float f: The floating point value.
:return: The nearest half-float equivalent stored as an int.
:rtype: ``int``
)");
extern "C" RENDERDOC_API uint16_t RENDERDOC_CC RENDERDOC_FloatToHalf(float flt);

DOCUMENT(R"(A utility function that returns the number of vertices in a primitive of a given
topology.

.. note:: In strip topologies vertices are re-used.

:param Topology topology: The topology to query about.
:return: The number of vertices in a single primitive.
:rtype: ``int``
)");
extern "C" RENDERDOC_API uint32_t RENDERDOC_CC RENDERDOC_NumVerticesPerPrimitive(Topology topology);

DOCUMENT(R"(A utility function that returns the offset in the list of vertices of the first vertex
in a particular primitive of a given topology. This calculation is simple but not trivial for the
case of strip topologies.

:param Topology topology: The topology to query about.
:param int primitive: The primitive to query about.
:return: The vertex offset where the primitive starts.
:rtype: ``int``
)");
extern "C" RENDERDOC_API uint32_t RENDERDOC_CC RENDERDOC_VertexOffset(Topology topology,
                                                                      uint32_t primitive);

//////////////////////////////////////////////////////////////////////////
// Create a capture handle.
//
// Takes the filename of the log. Always returns a valid handle that must be shut down.
// If any errors happen this can be queried with :meth:`CaptureFile.Status`.
//////////////////////////////////////////////////////////////////////////

DOCUMENT(R"(Create a capture handle.

Takes the filename of the log. This function *always* returns a valid handle that must be shut down.
If any errors happen this can be queried with :meth:`CaptureFile.Status`.

:return: A handle to the specified path.
:rtype: ReplaySupport
)");
extern "C" RENDERDOC_API ICaptureFile *RENDERDOC_CC RENDERDOC_OpenCaptureFile(const char *logfile);

//////////////////////////////////////////////////////////////////////////
// Target Control
//////////////////////////////////////////////////////////////////////////

DOCUMENT(R"(Creates a :class:`TargetControl` connection to a given hostname and ident.

This function will block until the control connection is ready, or an error occurs.

:param str host: The hostname to connect to. If blank, the local machine is used.
:param int ident: The ident for the particular target to connect to on that machine.
:param str clientName: The client name to use when connecting. See
  :meth:`TargetControl.GetBusyClient`.
:param bool forceConnection: Force the connection and kick off any existing client that is currently
  connected.
:return: A handle to the target control connection, or ``None`` if something went wrong.
:rtype: TargetControl
)");
extern "C" RENDERDOC_API ITargetControl *RENDERDOC_CC RENDERDOC_CreateTargetControl(
    const char *host, uint32_t ident, const char *clientName, bool32 forceConnection);

DOCUMENT(R"(Repeatedly query to enumerate which targets are active on a given machine and their
idents.

Initially this should be called with ``nextIdent`` being 0, to retrieve the first target
active. After that it can be called again and again with the previous return value to enumerate
more targets.

This function will block for a variable timeout depending on how many targets are scanned.

:param str host: The hostname to connect to. If blank, the local machine is used.
:param int nextIdent: The next ident to scan.
:return: The ident of the next active target, or ``0`` if no other targets exist.
:rtype: ``int``
)");
extern "C" RENDERDOC_API uint32_t RENDERDOC_CC RENDERDOC_EnumerateRemoteTargets(const char *host,
                                                                                uint32_t nextIdent);

//////////////////////////////////////////////////////////////////////////
// Remote server
//////////////////////////////////////////////////////////////////////////

DOCUMENT(R"(Retrieves the default ports where remote servers listen.

:return: The port where remote servers listen by default.
:rtype: ``int``
)");
extern "C" RENDERDOC_API uint32_t RENDERDOC_CC RENDERDOC_GetDefaultRemoteServerPort();

DOCUMENT(R"(Create a connection to a remote server running on given hostname and port.

This function will block until the capture is fully loaded and ready.

:param str host: The hostname to connect to, if blank then localhost is used.
:param int port: The port to connect to, or the default port if 0.
:param RemoteServer rend: A reference to a :class:`RemoteServer` where the connection handle will be
  stored.
:return: The status of opening the capture, whether success or failure.
:rtype: ReplayStatus
)");
extern "C" RENDERDOC_API ReplayStatus RENDERDOC_CC
RENDERDOC_CreateRemoteServerConnection(const char *host, uint32_t port, IRemoteServer **rend);

DOCUMENT(R"(This launches a remote server which will continually run in a loop to server requests
from external sources.

This function will block until a remote connection tells the server to shut down, or the
``killReplay`` value becomes ``True``.

:param str host: The name of the interface to listen on.
:param int port: The port to listen on, or the default port if 0.
:param bool killReplay: A reference to a ``bool`` that can be set to ``True`` to shut down the
  server.
)");
extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_BecomeRemoteServer(const char *listenhost,
                                                                        uint32_t port,
                                                                        volatile bool32 *killReplay);

//////////////////////////////////////////////////////////////////////////
// Injection/execution capture functions.
//////////////////////////////////////////////////////////////////////////

DOCUMENT(R"(Retrieve the default and recommended set of capture options.

:param CaptureOptions opts: A reference to a :class:`CaptureOptions` where the options will be
  stored.
)");
extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_GetDefaultCaptureOptions(CaptureOptions *opts);

DOCUMENT(R"(Begin injecting speculatively into all new processes started on the system. Where
supported by platform, configuration, and setup begin injecting speculatively into all new processes
started on the system.

This function can only be called if global hooking is supported (see :ref:`CanGlobalHook`) and if
global hooking is not active (see :ref:`IsGlobalHookActive`).

This function must be called when the process is running with administrator/superuser permissions.

:param str pathmatch: A string to match against each new process's executable path to determine
  which corresponds to the program we actually want to capture.
:param str logfile: Where to store any captures.
:param CaptureOptions opts: The capture options to use when injecting into the program.
:return: ``True`` if the hook is active, ``False`` if something went wrong. The hook must be closed
  with :ref:`StopGlobalHook` before the application is closed.
:rtype: ``bool``
)");
extern "C" RENDERDOC_API bool32 RENDERDOC_CC RENDERDOC_StartGlobalHook(const char *pathmatch,
                                                                       const char *logfile,
                                                                       const CaptureOptions &opts);

DOCUMENT(R"(Stop the global hook that was activated by :ref:`StartGlobalHook`.

This function can only be called if global hooking is supported (see :ref:`CanGlobalHook`) and if
global hooking is active (see :ref:`IsGlobalHookActive`).
)");
extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_StopGlobalHook();

DOCUMENT(R"(Determines if the global hook is active or not.

This function can only be called if global hooking is supported (see :ref:`CanGlobalHook`).

:return: ``True`` if the hook is active, or ``False`` if the hook is inactive.
:rtype: ``bool``
)");
extern "C" RENDERDOC_API bool32 RENDERDOC_CC RENDERDOC_IsGlobalHookActive();

DOCUMENT(R"(Determines if the global hook is supported on the current platform and configuration.

:return: ``True`` if global hooking can be used on the platform, ``False`` if not.
:rtype: ``bool``
)");
extern "C" RENDERDOC_API bool32 RENDERDOC_CC RENDERDOC_CanGlobalHook();

DOCUMENT(R"(Launch an application and inject into it to allow capturing.

:param str app: The path to the application to run.
:param str workingDir: The working directory to use when running the application. If blank, the
  directory containing the application is used.
:param str cmdLine: The command line to use when running the application, it will be processed in a
  platform specific way to generate arguments.
:param list env: Any :class:`EnvironmentModification` that should be made when running the program.
:param CaptureOptions opts: The capture options to use when injecting into the program.
:param bool waitForExit: If ``True`` this function will block until the process exits.
:return: The ident where the new application is listening for target control, or 0 if something went
  wrong.
:rtype: ``int``
)");
extern "C" RENDERDOC_API uint32_t RENDERDOC_CC
RENDERDOC_ExecuteAndInject(const char *app, const char *workingDir, const char *cmdLine,
                           const rdctype::array<EnvironmentModification> &env, const char *logfile,
                           const CaptureOptions &opts, bool32 waitForExit);

DOCUMENT(R"(Where supported by operating system and permissions, inject into a running process.

:param int pid: The Process ID (PID) to inject into.
:param list env: Any :class:`EnvironmentModification` that should be made when running the program.
:param CaptureOptions opts: The capture options to use when injecting into the program.
:param bool waitForExit: If ``True`` this function will block until the process exits.
:return: The ident where the new application is listening for target control, or 0 if something went
  wrong.
:rtype: ``int``
)");
extern "C" RENDERDOC_API uint32_t RENDERDOC_CC
RENDERDOC_InjectIntoProcess(uint32_t pid, const rdctype::array<EnvironmentModification> &env,
                            const char *logfile, const CaptureOptions &opts, bool32 waitForExit);

DOCUMENT(R"(When debugging RenderDoc it can be useful to capture itself by doing a side-build with a
temporary name. This function wraps up the use of the in-application API to start a capture.

:param str dllname: The name of the self-hosted capture module.
)");
extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_StartSelfHostCapture(const char *dllname);

DOCUMENT(R"(When debugging RenderDoc it can be useful to capture itself by doing a side-build with a
temporary name. This function wraps up the use of the in-application API to end a capture.

:param str dllname: The name of the self-hosted capture module.
)");
extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_EndSelfHostCapture(const char *dllname);

//////////////////////////////////////////////////////////////////////////
// Vulkan layer handling
//////////////////////////////////////////////////////////////////////////

DOCUMENT("Internal function for determining vulkan layer registration status.");
extern "C" RENDERDOC_API bool RENDERDOC_CC
RENDERDOC_NeedVulkanLayerRegistration(VulkanLayerFlags *flags, rdctype::array<rdctype::str> *myJSONs,
                                      rdctype::array<rdctype::str> *otherJSONs);

DOCUMENT("Internal function for updating vulkan layer registration.");
extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_UpdateVulkanLayerRegistration(bool systemLevel);

//////////////////////////////////////////////////////////////////////////
// Miscellaneous!
//////////////////////////////////////////////////////////////////////////

DOCUMENT("Internal function for initialising global process environment in a replay program.");
extern "C" RENDERDOC_API void RENDERDOC_CC
RENDERDOC_InitGlobalEnv(GlobalEnvironment env, const rdctype::array<rdctype::str> &args);

DOCUMENT("Internal function for triggering exception handler.");
extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_TriggerExceptionHandler(void *exceptionPtrs,
                                                                             bool32 crashed);

DOCUMENT(R"(Sets the location for the diagnostic log output, shared by captured programs and the
analysis program.

:param str filename: The path to the new log file.
)");
extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_SetDebugLogFile(const char *filename);

DOCUMENT(R"(Gets the location for the diagnostic log output, shared by captured programs and the
analysis program.

:return: The path to the current log file.
:rtype: ``str``
)");
extern "C" RENDERDOC_API const char *RENDERDOC_CC RENDERDOC_GetLogFile();

DOCUMENT("Internal function for logging text simply.");
extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_LogText(const char *text);

DOCUMENT("Internal function for logging messages in detail.");
extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_LogMessage(LogType type, const char *project,
                                                                const char *file, unsigned int line,
                                                                const char *text);

DOCUMENT(R"(Retrieves the version string.

This will be in the form "MAJOR.MINOR"

:return: The version string.
:rtype: ``str``
)");
extern "C" RENDERDOC_API const char *RENDERDOC_CC RENDERDOC_GetVersionString();

DOCUMENT("Internal function for retrieving a config setting.");
extern "C" RENDERDOC_API const char *RENDERDOC_CC RENDERDOC_GetConfigSetting(const char *name);

DOCUMENT("Internal function for setting a config setting.");
extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_SetConfigSetting(const char *name,
                                                                      const char *value);

DOCUMENT("Internal function for fetching friendly android names.");
extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_GetAndroidFriendlyName(const rdctype::str &device,
                                                                            rdctype::str &friendly);

DOCUMENT("Internal function for enumerating android devices.");
extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_EnumerateAndroidDevices(rdctype::str *deviceList);

DOCUMENT("Internal function for starting an android remote server.");
extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_StartAndroidRemoteServer(const char *device);
