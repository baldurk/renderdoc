/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2019 Baldur Karlsson
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
#include <functional>

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

// There's a bug in visual assist that stops highlighting if a raw string is too long. It looks like
// it happens when it reaches 128 lines long or ~5000 bytes which is quite suspicious.
// Anyway since this doesn't come up that often, we split the following docstring in two part-way
// through. Don't be alarmed, just move along
#ifndef DOCUMENT2
#define DOCUMENT2(text1, text2)
#endif

#ifndef DOCUMENT3
#define DOCUMENT3(text1, text2, text3)
#endif

#ifndef DOCUMENT4
#define DOCUMENT4(text1, text2, text3, text4)
#endif

#if defined(RENDERDOC_PLATFORM_WIN32)

#define RENDERDOC_EXPORT_API __declspec(dllexport)
#define RENDERDOC_IMPORT_API __declspec(dllimport)
#define RENDERDOC_CC __cdecl

#elif defined(RENDERDOC_PLATFORM_LINUX) || defined(RENDERDOC_PLATFORM_APPLE) || \
    defined(RENDERDOC_PLATFORM_ANDROID) || defined(RENDERDOC_PLATFORM_GGP)

#define RENDERDOC_EXPORT_API __attribute__((visibility("default")))
#define RENDERDOC_IMPORT_API

#define RENDERDOC_CC

#else

#error "Unknown platform"

#endif

// this #define can be used to mark a program as a 'replay' program which should not be captured.
// Any program used for such purpose must define and export this symbol in the main exe or one dll
// that will be loaded before renderdoc.dll is loaded.
#define REPLAY_PROGRAM_MARKER() \
  extern "C" RENDERDOC_EXPORT_API void RENDERDOC_CC renderdoc__replay__marker() {}
// define the API visibility depending on whether we're exporting
#ifdef RENDERDOC_EXPORTS
#define RENDERDOC_API RENDERDOC_EXPORT_API
#else
#define RENDERDOC_API RENDERDOC_IMPORT_API
#endif

// needs to be declared up here for reference in basic_types

extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_FreeArrayMem(const void *mem);
typedef void(RENDERDOC_CC *pRENDERDOC_FreeArrayMem)(const void *mem);

extern "C" RENDERDOC_API void *RENDERDOC_CC RENDERDOC_AllocArrayMem(uint64_t sz);
typedef void *(RENDERDOC_CC *pRENDERDOC_AllocArrayMem)(uint64_t sz);

#ifdef NO_ENUM_CLASS_OPERATORS

#define BITMASK_OPERATORS(a)
#define ITERABLE_OPERATORS(a)

#else

#include <type_traits>

// helper template that allows the result of & to be cast back to the enum or explicitly cast to
// bool for use in if() or ?: or so on or compared against 0.
//
// If you get an error about missing operator then you're probably doing something like
// (bitfield & value) == 0 or (bitfield & value) != 0 or similar. Instead prefer:
// !(bitfield & value)     or (bitfield & value) to make use of the bool cast directly
template <typename enum_name>
struct EnumCastHelper
{
public:
  constexpr EnumCastHelper(enum_name v) : val(v) {}
  constexpr operator enum_name() const { return val; }
  constexpr explicit operator bool() const
  {
    typedef typename std::underlying_type<enum_name>::type etype;
    return etype(val) != 0;
  }

private:
  const enum_name val;
};

// helper templates for iterating over all values in an enum that has sequential values and is
// to be used for array indices or something like that.
template <typename enum_name>
struct ValueIterContainer
{
  struct ValueIter
  {
    ValueIter(enum_name v) : val(v) {}
    enum_name val;
    enum_name operator*() const { return val; }
    bool operator!=(const ValueIter &it) const { return !(val == *it); }
    const inline enum_name operator++()
    {
      ++val;
      return val;
    }
  };

  ValueIter begin() { return ValueIter(enum_name::First); }
  ValueIter end() { return ValueIter(enum_name::Count); }
};

template <typename enum_name>
struct IndexIterContainer
{
  typedef typename std::underlying_type<enum_name>::type etype;

  struct IndexIter
  {
    IndexIter(enum_name v) : val(v) {}
    enum_name val;
    etype operator*() const { return etype(val); }
    bool operator!=(const IndexIter &it) const { return !(val == it.val); }
    const inline enum_name operator++()
    {
      ++val;
      return val;
    }
  };

  IndexIter begin() { return IndexIter(enum_name::First); }
  IndexIter end() { return IndexIter(enum_name::Count); }
};

template <typename enum_name>
constexpr inline ValueIterContainer<enum_name> values()
{
  return ValueIterContainer<enum_name>();
};

template <typename enum_name>
constexpr inline IndexIterContainer<enum_name> indices()
{
  return IndexIterContainer<enum_name>();
};

template <typename enum_name>
constexpr inline size_t arraydim()
{
  typedef typename std::underlying_type<enum_name>::type etype;
  return (size_t)etype(enum_name::Count);
};

// clang-format makes a even more of a mess of this multi-line macro than it usually does, for some
// reason. So we just disable it since it's still readable and this isn't really the intended case
// we are using clang-format for.

// clang-format off
#define BITMASK_OPERATORS(enum_name)                                           \
                                                                               \
constexpr inline enum_name operator|(enum_name a, enum_name b)                 \
{                                                                              \
  typedef typename std::underlying_type<enum_name>::type etype;                \
  return enum_name(etype(a) | etype(b));                                       \
}                                                                              \
                                                                               \
constexpr inline EnumCastHelper<enum_name> operator&(enum_name a, enum_name b) \
{                                                                              \
  typedef typename std::underlying_type<enum_name>::type etype;                \
  return EnumCastHelper<enum_name>(enum_name(etype(a) & etype(b)));            \
}                                                                              \
                                                                               \
constexpr inline enum_name operator~(enum_name a)                              \
{                                                                              \
  typedef typename std::underlying_type<enum_name>::type etype;                \
  return enum_name(~etype(a));                                                 \
}                                                                              \
                                                                               \
inline enum_name &operator|=(enum_name &a, enum_name b)                        \
{ return a = a | b; }                                                          \
                                                                               \
inline enum_name &operator&=(enum_name &a, enum_name b)                        \
{ return a = a & b; }

#define ITERABLE_OPERATORS(enum_name)                                          \
                                                                               \
inline enum_name operator++(enum_name &a)                                      \
{                                                                              \
  typedef typename std::underlying_type<enum_name>::type etype;                \
  return a = enum_name(etype(a)+1);                                            \
}
// clang-format on

#endif

#define ENUM_ARRAY_SIZE(enum_name) size_t(enum_name::Count)

// declare ResourceId extremely early so that it can be referenced in structured_data.h

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
#if defined(SWIG)
  ResourceId(const ResourceId &other) : id(other.id) {}
#endif
  DOCUMENT("A helper function that explicitly creates an empty/invalid/null :class:`ResourceId`.");
  inline static ResourceId Null() { return ResourceId(); }
  DOCUMENT("Compares two ``ResourceId`` objects for equality.");
  bool operator==(const ResourceId u) const { return id == u.id; }
  DOCUMENT("Compares two ``ResourceId`` objects for inequality.");
  bool operator!=(const ResourceId u) const { return id != u.id; }
  DOCUMENT("Compares two ``ResourceId`` objects for less-than.");
  bool operator<(const ResourceId u) const { return id < u.id; }
#if defined(RENDERDOC_QT_COMPAT)
  operator QVariant() const { return QVariant::fromValue(*this); }
#endif

private:
  uint64_t id;

#ifdef RENDERDOC_EXPORTS
  friend ResourceId ResourceIDGen::GetNewUniqueID();
#endif
};

#include "basic_types.h"
#include "stringise.h"
#include "structured_data.h"

DOCUMENT(R"(Specifies a windowing system to use for creating an output window.

.. data:: Unknown

  Unknown window type, no windowing data is passed and no native window is described.

.. data:: Headless

  The windowing data doesn't describe a real window but a virtual area, allowing all normal output
  rendering to happen off-screen.
  See :func:`CreateHeadlessWindowingData`.

.. data:: Win32

  The windowing data refers to a Win32 window. See :func:`CreateWin32WindowingData`.

.. data:: Xlib

  The windowing data refers to an Xlib window. See :func:`CreateXLibWindowingData`.

.. data:: XCB

  The windowing data refers to an XCB window. See :func:`CreateXCBWindowingData`.

.. data:: Android

  The windowing data refers to an Android window. See :func:`CreateAndroidWindowingData`.

.. data:: MacOS

  The windowing data refers to a MacOS / OS X NSView & CALayer that is Metal/GL compatible.
  See :func:`CreateMacOSWindowingData`.
)");
enum class WindowingSystem : uint32_t
{
  Unknown,
  Headless,
  Win32,
  Xlib,
  XCB,
  Android,
  MacOS,
  GGP,
};

DECLARE_REFLECTION_ENUM(WindowingSystem);

// typedef the window data structs so this will compile on all platforms without system headers. We
// only actually need the real definitions when we're using the data, otherwise it's mostly opaque
// pointers or integers.

// Win32
typedef struct HWND__ *HWND;

// xlib
typedef struct _XDisplay Display;
typedef unsigned long Drawable;

// xcb
struct xcb_connection_t;
typedef uint32_t xcb_window_t;

// android
struct ANativeWindow;

// for swig bindings treat the windowing data struct as completely opaque
#if defined(SWIG)

DOCUMENT("An opaque structure created to hold windowing setup data");
struct WindowingData
{
};

#else

struct WindowingData
{
  WindowingSystem system;

  union
  {
    struct
    {
      int32_t width, height;
    } headless;

    struct
    {
      HWND window;
    } win32;

    struct
    {
      Display *display;
      Drawable window;
    } xlib;

    struct
    {
      xcb_connection_t *connection;
      xcb_window_t window;
    } xcb;

    struct
    {
      ANativeWindow *window;
    } android;

    struct
    {
      void *view;
      void *layer;
    } macOS;
  };
};

DECLARE_REFLECTION_ENUM(WindowingData);

#endif

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

DOCUMENT(R"(Create a :class:`WindowingData` for an metal/opengl-compatible macOS ``CALayer`` handle
and ``NSView`` handle (as void pointers).

:param NSView view: The native ``NSView`` handle for this window.
:param CALayer layer: The native ``CALayer`` handle for this window.
:return: A :class:`WindowingData` corresponding to the given window.
:rtype: WindowingData
)");
inline const WindowingData CreateMacOSWindowingData(void *view, void *layer)
{
  WindowingData ret = {};

  ret.system = WindowingSystem::MacOS;
  ret.macOS.view = view;
  ret.macOS.layer = layer;

  return ret;
}

// declare metatype/reflection for ResourceId here as the struct itself is declared before including
// all relevant headers above
#if defined(RENDERDOC_QT_COMPAT)
Q_DECLARE_METATYPE(ResourceId);
#endif

DECLARE_REFLECTION_STRUCT(ResourceId);

#include "capture_options.h"
#include "control_types.h"
#include "data_types.h"
#include "replay_enums.h"
#include "shader_types.h"
#include "pipestate.h"

DOCUMENT(R"(Internal structure used for initialising environment in a replay application.)");
struct GlobalEnvironment
{
  DOCUMENT("");
  GlobalEnvironment() = default;
  GlobalEnvironment(const GlobalEnvironment &) = default;

  DOCUMENT("The handle to the X display to use internally. If left ``NULL``, one will be opened.");
  Display *xlibDisplay = NULL;
};

DOCUMENT("The result of executing or injecting into a program.")
struct ExecuteResult
{
  DOCUMENT("");
  ExecuteResult() = default;
  ExecuteResult(const ExecuteResult &) = default;

  DOCUMENT(
      "The :class:`ReplayStatus` resulting from the operation, indicating success or failure.");
  ReplayStatus status;
  DOCUMENT(R"(The ident where the new application is listening for target control, or 0 if something
went wrong.
)");
  uint32_t ident;
};

DECLARE_REFLECTION_STRUCT(ExecuteResult);

// there's not a good way to document a callback, so for lack of a better place we declare these
// here and document them immediately below. They can be linked to from anywhere by name.
typedef std::function<bool()> RENDERDOC_KillCallback;
typedef std::function<void(float)> RENDERDOC_ProgressCallback;
typedef std::function<WindowingData(bool, const rdcarray<WindowingSystem> &)> RENDERDOC_PreviewWindowCallback;

DOCUMENT(R"(A stateful output handle that contains the current configuration for one particular view
of the capture. This allows multiple outputs to run independently without interfering with each
other.

The different types are enumerated in :class:`ReplayOutputType`.

.. function:: KillCallback()

  Not an actual member function - the signature for any ``KillCallback`` callbacks.

  Called whenever some on-going blocking process needs to determine if it should close.

  :return: Whether or not the process should be killed.
  :rtype: ``bool``

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

  DOCUMENT("Sets the :class:`TextureDisplay` configuration for a texture output.");
  virtual void SetTextureDisplay(const TextureDisplay &o) = 0;

  DOCUMENT("Sets the :class:`MeshDisplay` configuration for a mesh output.");
  virtual void SetMeshDisplay(const MeshDisplay &o) = 0;

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
:rtype: ``bytes``
)");
  virtual bytebuf ReadbackOutputTexture() = 0;

  DOCUMENT(R"(Retrieve the current dimensions of the output.

:return: The current width and height of the output.
:rtype: ``pair`` of two ``int``
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
:param int slice: The slice of the texture to display.
:param int mip: The mip of the texture to display.
:return: A boolean indicating if the thumbnail was successfully created.
:rtype: ``bool``
)");
  virtual bool AddThumbnail(WindowingData window, ResourceId textureId, CompType typeHint,
                            uint32_t mip, uint32_t slice) = 0;

  DOCUMENT(R"(Render to the window handle specified when the output was created.

This will also render any thumbnails and the pixel context, if enabled.
)");
  virtual void Display() = 0;

  DOCUMENT(R"(Sets up a zoomed in pixel context view around a particular pixel selection.

The texture rendering uses the configuration specified in :meth:`SetTextureDisplay` except with a
fixed high zoom value and a fixed position, see :meth:`SetPixelContextLocation`.

Should only be called for texture outputs.

:param WindowingData window: A :class:`WindowingData` describing the native window.
:return: A boolean indicating if the pixel context was successfully configured.
:rtype: ``bool``
)");
  virtual bool SetPixelContext(WindowingData window) = 0;

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
  virtual rdcpair<PixelValue, PixelValue> GetMinMax() = 0;

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
  virtual rdcarray<uint32_t> GetHistogram(float minval, float maxval, bool channels[4]) = 0;

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

.. note::
  X and Y co-ordinates are always considered to be top-left, even on GL, for consistency between
  APIs and preventing the need for API-specific code in most cases. This means if co-ordinates are
  fetched from e.g. viewport or scissor data or other GL pipeline state which is perhaps in
  bottom-left co-ordinates, care must be taken to translate them.

:param ResourceId textureId: The texture to pick the pixel from.
:param bool customShader: Whether to apply the configured custom shader.
:param int x: The x co-ordinate to pick from.
:param int y: The y co-ordinate to pick from.
:param int sliceFace: The slice of an array or 3D texture, or face of a cubemap texture.
:param int mip: The mip level to pick from.
:param int sample: The multisample sample to pick from.
:return: The contents of the pixel.
:rtype: PixelValue
)");
  virtual PixelValue PickPixel(ResourceId textureId, bool customShader, uint32_t x, uint32_t y,
                               uint32_t sliceFace, uint32_t mip, uint32_t sample) = 0;

  DOCUMENT(R"(Retrieves the vertex and instance that is under the cursor location, when viewed
relative to the current window with the current mesh display configuration.

.. note::
  X and Y co-ordinates are always considered to be top-left, even on GL, for consistency between
  APIs and preventing the need for API-specific code in most cases. This means if co-ordinates are
  fetched from e.g. viewport or scissor data or other GL pipeline state which is perhaps in
  bottom-left co-ordinates, care must be taken to translate them.

Should only be called for mesh outputs.

:param int eventId: The event ID to pick at.
:param int x: The x co-ordinate to pick from.
:param int y: The y co-ordinate to pick from.
:return: A tuple with the first value being the vertex index in the mesh, and the second value being
  the instance index. The values are set to :data:`NoResult` if no vertex was found, 
:rtype: ``tuple`` of ``int`` and ``int``
)");
  virtual rdcpair<uint32_t, uint32_t> PickVertex(uint32_t eventId, uint32_t x, uint32_t y) = 0;

  static const uint32_t NoResult = ~0U;

protected:
  IReplayOutput() = default;
  ~IReplayOutput() = default;
};

DOCUMENT(R"(The primary interface to access the information in a capture and the current state, as
well as control the replay and analysis functionality available.

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
:rtype: ``list`` of :class:`WindowingSystem`
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

:return: The list of disassembly targets available.
:rtype: ``list`` of ``str``
)");
  virtual rdcarray<rdcstr> GetDisassemblyTargets() = 0;

  DOCUMENT(R"(Retrieve the disassembly for a given shader, for the given disassembly target.

:param ResourceId pipeline: The pipeline state object, if applicable, that this shader is bound to.
:param ShaderReflection refl: The shader reflection details of the shader to disassemble
:param str target: The name of the disassembly target to generate for. Must be one of the values
  returned by :meth:`GetDisassemblyTargets`, or empty to use the default generation.
:return: The disassembly text, or an error message if something went wrong.
:rtype: ``str``
)");
  virtual rdcstr DisassembleShader(ResourceId pipeline, const ShaderReflection *refl,
                                   const char *target) = 0;

  DOCUMENT(R"(Builds a shader suitable for running on the local replay instance as a custom shader.

See :data:`TextureDisplay.customShaderId`.

:param str entry: The entry point to use when compiling.
:param ShaderEncoding sourceEncoding: The encoding of the source data.
:param bytes source: The source data itself.
:param int compileFlags: API-specific compilation flags.
:param ShaderStage type: The stage that this shader will be executed at.
:return: A ``tuple`` with the id of the new shader if compilation was successful,
  :meth:`ResourceId.Null` otherwise, and a ``str`` with any warnings/errors from compilation.
:rtype: ``tuple`` of :class:`ResourceId` and ``str``.
)");
  virtual rdcpair<ResourceId, rdcstr> BuildCustomShader(const char *entry,
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
:rtype: ``tuple`` of :class:`ResourceId` and ``str``.
)");
  virtual rdcpair<ResourceId, rdcstr> BuildTargetShader(const char *entry,
                                                        ShaderEncoding sourceEncoding, bytebuf source,
                                                        const ShaderCompileFlags &flags,
                                                        ShaderStage type) = 0;

  DOCUMENT(R"(Retrieve the list of supported :class:`ShaderEncoding` which can be build using
:meth:`BuildTargetShader`.

The list is sorted in priority order, so if the caller has a shader in a form but could
compile/translate it to another, prefer to satisfy the first encoding before later encodings.

This typically means the 'native' encoding is listed first, and then subsequent encodings are
compiled internally - so compiling externally could be preferable as it allows better customisation
of the compile process or using alternate/updated tools.

:return: The list of target shader encodings available.
:rtype: ``list`` of :class:`ShaderEncoding`
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
:rtype: ``list`` of :class:`ShaderEncoding`
)");
  virtual rdcarray<ShaderEncoding> GetCustomShaderEncodings() = 0;

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

  DOCUMENT(R"(Add fake marker regions to the list of drawcalls in the capture, based on which
textures are bound as outputs.
)");
  virtual void AddFakeMarkers() = 0;

  DOCUMENT(R"(Retrieve the list of root-level drawcalls in the capture.

:return: The list of root-level drawcalls in the capture.
:rtype: ``list`` of :class:`DrawcallDescription`
)");
  virtual const rdcarray<DrawcallDescription> &GetDrawcalls() = 0;

  DOCUMENT(R"(Retrieve the values of a specified set of counters.

:param list counters: The list of :class:`GPUCounter` to fetch results for.
:return: The list of counter results generated.
:rtype: ``list`` of :class:`CounterResult`
)");
  virtual rdcarray<CounterResult> FetchCounters(const rdcarray<GPUCounter> &counters) = 0;

  DOCUMENT(R"(Retrieve a list of which counters are available in the current capture analysis
implementation.

:return: The list of counters available.
:rtype: ``list`` of :class:`GPUCounter`
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
:rtype: ``list`` of :class:`ResourceDescription`
)");
  virtual const rdcarray<ResourceDescription> &GetResources() = 0;

  DOCUMENT(R"(Retrieve the list of textures alive in the capture.

:return: The list of textures in the capture.
:rtype: ``list`` of :class:`TextureDescription`
)");
  virtual const rdcarray<TextureDescription> &GetTextures() = 0;

  DOCUMENT(R"(Retrieve the list of buffers alive in the capture.

:return: The list of buffers in the capture.
:rtype: ``list`` of :class:`BufferDescription`
)");
  virtual const rdcarray<BufferDescription> &GetBuffers() = 0;

  DOCUMENT(R"(Retrieve a list of any newly generated diagnostic messages.

Every time this function is called, any debug messages returned will not be returned again. Only
newly generated messages will be returned after that.

:return: The list of the :class:`DebugMessage` messages.
:rtype: ``list`` of :class:`DebugMessage`
)");
  virtual rdcarray<DebugMessage> GetDebugMessages() = 0;

  DOCUMENT(R"(Retrieve a list of entry points for a shader.

If the given ID doesn't specify a shader, an empty list will be return. On some APIs, the list will
only ever have one result (only one entry point per shader).

:param ResourceId shader: The shader to look up entry points for.
:return: The list of the :class:`ShaderEntryPoint` messages.
:rtype: ``list`` of :class:`ShaderEntryPoint`
)");
  virtual rdcarray<ShaderEntryPoint> GetShaderEntryPoints(ResourceId shader) = 0;

  DOCUMENT(R"(Retrieve the information about the frame contained in the capture.

:param ResourceId shader: The shader to get reflection data for.
:param ShaderEntryPoint entry: The entry point within the shader to reflect. May be ignored on some
  APIs
:return: The frame information.
:rtype: ShaderReflection
)");
  virtual ShaderReflection *GetShader(ResourceId shader, ShaderEntryPoint entry) = 0;

  DOCUMENT(R"(Retrieve the history of modifications to the selected pixel on the selected texture.

.. note::
  X and Y co-ordinates are always considered to be top-left, even on GL, for consistency between
  APIs and preventing the need for API-specific code in most cases. This means if co-ordinates are
  fetched from e.g. viewport or scissor data or other GL pipeline state which is perhaps in
  bottom-left co-ordinates, care must be taken to translate them.

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
  virtual rdcarray<PixelModification> PixelHistory(ResourceId texture, uint32_t x, uint32_t y,
                                                   uint32_t slice, uint32_t mip, uint32_t sampleIdx,
                                                   CompType typeHint) = 0;

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
  virtual rdcarray<EventUsage> GetUsage(ResourceId id) = 0;

  DOCUMENT(R"(Retrieve the contents of a constant block by reading from memory or their source
otherwise.

:param ResourceId shader: The id of the shader to use for metadata.
:param str entryPoint: The entry point of the shader being used. In some APIs, this is ignored.
:param int cbufslot: The index in the :data:`ShaderReflection.constantBlocks` list to look up.
:param ResourceId buffer: The id of the buffer to use for data. If
  :data:`ConstantBlock.bufferBacked` is ``False`` this is ignored.
:param int offs: Retrieve buffer contents starting at this byte offset.
:return: The shader variables with their contents.
:rtype: ``list`` of :class:`ShaderVariable`
)");
  virtual rdcarray<ShaderVariable> GetCBufferVariableContents(ResourceId shader,
                                                              const char *entryPoint,
                                                              uint32_t cbufslot, ResourceId buffer,
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
:rtype: ``bytes``
)");
  virtual bytebuf GetBufferData(ResourceId buff, uint64_t offset, uint64_t len) = 0;

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
  virtual bytebuf GetTextureData(ResourceId tex, uint32_t arrayIdx, uint32_t mip) = 0;

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
  virtual void CopyCapture(uint32_t captureId, const char *localpath) = 0;

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
  DOCUMENT(R"(Retrieve the total number of available sections.

:return: The number of sections in the capture
:rtype: ``int``.
)");
  virtual int GetSectionCount() = 0;

  DOCUMENT(R"(Locate the index of a section by its name. Returns ``-1`` if the section is not found.

This index should not be cached, as writing sections could re-order the indices.

:param str name: The name of the section to search for.
:return: The index of the section, or ``-1`` if not found.
:rtype: ``int``.
)");
  virtual int FindSectionByName(const char *name) = 0;

  DOCUMENT(R"(Locate the index of a section by its type. Returns ``-1`` if the section is not found.

This index should not be cached, as writing sections could re-order the indices.

:param SectionType type: The type of the section to search for.
:return: The index of the section, or ``-1`` if not found.
:rtype: ``int``.
)");
  virtual int FindSectionByType(SectionType type) = 0;

  DOCUMENT(R"(Get the describing properties of the specified section.

:param int index: The index of the section.
:return: The properties of the section, if the index is valid.
:rtype: SectionProperties
)");
  virtual SectionProperties GetSectionProperties(int index) = 0;

  DOCUMENT(R"(Get the raw byte contents of the specified section.

:param int index: The index of the section.
:return: The raw contents of the section, if the index is valid.
:rtype: ``bytes``.
)");
  virtual bytebuf GetSectionContents(int index) = 0;

  DOCUMENT(R"(Writes a new section with specified properties and contents. If an existing section
already has the same type or name, it will be overwritten (two sections cannot share the same type
or name).

:param SectionProperties props: The properties of the section to be written.
:param bytes contents: The raw contents of the section.
:return: ``True`` if the section was written successfully, ``False`` otherwise.
:rtype: ``bool``
)");
  virtual bool WriteSection(const SectionProperties &props, const bytebuf &contents) = 0;

  DOCUMENT(R"(Query if callstacks are available.

:return: ``True`` if any callstacks are available, ``False`` otherwise.
:rtype: ``bool``
)");
  virtual bool HasCallstacks() = 0;

  DOCUMENT(R"(Begin initialising a callstack resolver, looking up symbol files and caching as
necessary.

This function blocks while trying to initialise callstack resolving, so it should be called on a
separate thread.

:param ProgressCallback progress: A callback that will be repeatedly called with an updated progress
  value for the resolver process. Can be ``None`` if no progress is desired.
:return: ``True`` if the resolver successfully initialised, ``False`` if something went wrong.
:rtype: ``bool``
)");
  virtual bool InitResolver(RENDERDOC_ProgressCallback progress) = 0;

  DOCUMENT(R"(Retrieve the details of each stackframe in the provided callstack.

Must only be called after :meth:`InitResolver` has returned ``True``.

:param list callstack: The integer addresses in the original callstack.
:return: The list of resolved callstack entries as strings.
:rtype: ``list`` of ``str``
)");
  virtual rdcarray<rdcstr> GetResolve(const rdcarray<uint64_t> &callstack) = 0;

  DOCUMENT(R"(Retrieves the name of the driver that was used to create this capture.

:return: A simple string identifying the driver used to make the capture.
:rtype: ``str``
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
  virtual rdcarray<rdcstr> LocalProxies() = 0;

  DOCUMENT(R"(Retrieve a list of renderers supported by the remote server.

These will be strings like "D3D11" or "OpenGL".

:return: A list of names of the remote renderers.
:rtype: ``list`` of ``str``
)");
  virtual rdcarray<rdcstr> RemoteSupportedReplays() = 0;

  DOCUMENT(R"(Retrieve the path on the remote system where browsing can begin.

:return: The 'home' path where browsing for files or folders can begin.
:rtype: ``str``
)");
  virtual rdcstr GetHomeFolder() = 0;

  DOCUMENT(R"(Retrieve the contents of a folder path on the remote system.

If an error occurs, a single :class:`PathEntry` will be returned with appropriate error flags.

:param str path: The remote path to list.
:return: The contents of the specified folder.
:rtype: ``list`` of :class:`PathEntry`
)");
  virtual rdcarray<PathEntry> ListFolder(const char *path) = 0;

  DOCUMENT(R"(Launch an application and inject into it to allow capturing.

This happens on the remote system, so all paths are relative to the remote filesystem.

:param str app: The path to the application to run.
:param str workingDir: The working directory to use when running the application. If blank, the
  directory containing the application is used.
:param str cmdLine: The command line to use when running the application, it will be processed in a
  platform specific way to generate arguments.
:param list env: Any :class:`EnvironmentModification` that should be made when running the program.
:param CaptureOptions opts: The capture options to use when injecting into the program.
:return: The :class:`ExecuteResult` indicating both the status of the operation (success or failure)
  and any reason for failure, or else the ident where the new application is listening for target
  control if everything succeeded.
:rtype: ExecuteResult
)");
  virtual ExecuteResult ExecuteAndInject(const char *app, const char *workingDir, const char *cmdLine,
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
  virtual void TakeOwnershipCapture(const char *filename) = 0;

  DOCUMENT(R"(Copy a capture file that is stored on the local system to the remote system.

This function will block until the copy is fully complete, or an error has occurred.

This is primarily useful for when a capture is only stored locally and must be replayed remotely, as
the capture must be available on the machine where the replay happens.

:param str filename: The path to the file on the local system.
:param ProgressCallback progress: A callback that will be repeatedly called with an updated progress
  value for the copy. Can be ``None`` if no progress is desired.
:return: The path on the remote system where the capture was saved temporarily.
:rtype: ``str``
)");
  virtual rdcstr CopyCaptureToRemote(const char *filename, RENDERDOC_ProgressCallback progress) = 0;

  DOCUMENT(R"(Copy a capture file that is stored on the remote system to the local system.

This function will block until the copy is fully complete, or an error has occurred.

:param str remotepath: The remote path where the file should be copied from.
:param str localpath: The local path where the file should be saved.
:param ProgressCallback progress: A callback that will be repeatedly called with an updated progress
  value for the copy. Can be ``None`` if no progress is desired.
)");
  virtual void CopyCaptureFromRemote(const char *remotepath, const char *localpath,
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
:param ProgressCallback progress: A callback that will be repeatedly called with an updated progress
  value for the opening. Can be ``None`` if no progress is desired.
:return: A tuple containing the status of opening the capture, whether success or failure, and the
  resulting :class:`ReplayController` handle if successful.
:rtype: ``tuple`` of :class:`ReplayStatus` and :class:`ReplayController`
)");
  virtual rdcpair<ReplayStatus, IReplayController *> OpenCapture(
      uint32_t proxyid, const char *logfile, RENDERDOC_ProgressCallback progress) = 0;

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
:return: The status of the open operation, whether it succeeded or failed (and how it failed).
:rtype: ReplayStatus
)");
  virtual ReplayStatus OpenFile(const char *filename, const char *filetype,
                                RENDERDOC_ProgressCallback progress) = 0;

  DOCUMENT(R"(Initialises the file handle from a raw memory buffer.

This may be useful if you don't want to parse the whole file or already have the file in memory.

For the :paramref:`OpenBuffer.filetype` parameter, see :meth:`OpenFile`.

:param bytes buffer: The buffer containing the data to process.
:param str filetype: The format of the given file.
:param ProgressCallback progress: A callback that will be repeatedly called with an updated progress
  value if an import step occurs. Can be ``None`` if no progress is desired.
:return: The status of the open operation, whether it succeeded or failed (and how it failed).
:rtype: ReplayStatus
)");
  virtual ReplayStatus OpenBuffer(const bytebuf &buffer, const char *filetype,
                                  RENDERDOC_ProgressCallback progress) = 0;

  DOCUMENT(R"(When a capture file is opened, an exclusive lock is held on the file on disk. This
makes it impossible to copy the file to another location at the user's request. Calling this
function will copy the file on disk to a new location but otherwise won't affect the capture handle.
The new file will be locked, the old file will be unlocked - to allow deleting if necessary.

It is invalid to call this function if :meth:`OpenFile` has not previously been called to open the
file.

:param str filename: The filename to copy to.
:return: ``True`` if the operation succeeded.
:rtype: ``bool``
)");
  virtual bool CopyFileTo(const char *filename) = 0;

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
:return: The status of the conversion operation, whether it succeeded or failed (and how it failed).
:rtype: ReplayStatus
)");
  virtual ReplayStatus Convert(const char *filename, const char *filetype, const SDFile *file,
                               RENDERDOC_ProgressCallback progress) = 0;

  DOCUMENT(R"(Returns the human-readable error string for the last error received.

The error string is not reset by calling this function so it's safe to call multiple times. However
any other function call may reset the error string to empty.

:return: The error string, if one exists, or an empty string.
:rtype: ``str``
)");
  virtual rdcstr ErrorString() = 0;

  DOCUMENT(R"(Returns the list of capture file formats.

:return: The list of capture file formats available.
:rtype: ``list`` of :class:`CaptureFileFormat`
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
:rtype: ``str``
)");
  virtual const char *RecordedMachineIdent() = 0;

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
)");
  virtual void SetMetadata(const char *driverName, uint64_t machineIdent, FileType thumbType,
                           uint32_t thumbWidth, uint32_t thumbHeight, const bytebuf &thumbData) = 0;

  DOCUMENT(R"(Opens a capture for replay locally and returns a handle to the capture. Only supported
for handles opened with a native ``rdc`` capture, otherwise this will fail.

This function will block until the capture is fully loaded and ready.

Once the replay is created, this :class:`CaptureFile` can be shut down, there is no dependency on it
by the :class:`ReplayController`.

:param ProgressCallback progress: A callback that will be repeatedly called with an updated progress
  value for the opening. Can be ``None`` if no progress is desired.
:return: A tuple containing the status of opening the capture, whether success or failure, and the
  resulting :class:`ReplayController` handle if successful.
:rtype: ``tuple`` of :class:`ReplayStatus` and :class:`ReplayController`.
)");
  virtual rdcpair<ReplayStatus, IReplayController *> OpenCapture(RENDERDOC_ProgressCallback progress) = 0;

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
    const char *host, uint32_t ident, const char *clientName, bool forceConnection);

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
``killReplay`` callback returns ``True``.

:param str host: The name of the interface to listen on.
:param int port: The port to listen on, or the default port if 0.
:param KillCallback killReplay: A callback that returns a ``bool`` indicating if the server should
  be shut down or not.
:param PreviewWindowCallback previewWindow: A callback that returns information for a preview window
  when the server wants to display some preview of the ongoing replay.
)");
extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_BecomeRemoteServer(
    const char *listenhost, uint32_t port, RENDERDOC_KillCallback killReplay,
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

This function must be called when the process is running with administrator/superuser permissions.

:param str pathmatch: A string to match against each new process's executable path to determine
  which corresponds to the program we actually want to capture.
:param str logfile: Where to store any captures.
:param CaptureOptions opts: The capture options to use when injecting into the program.
:return: ``True`` if the hook is active, ``False`` if something went wrong. The hook must be closed
  with :func:`StopGlobalHook` before the application is closed.
:rtype: ``bool``
)");
extern "C" RENDERDOC_API bool RENDERDOC_CC RENDERDOC_StartGlobalHook(const char *pathmatch,
                                                                     const char *logfile,
                                                                     const CaptureOptions &opts);

DOCUMENT(R"(Stop the global hook that was activated by :func:`StartGlobalHook`.

This function can only be called if global hooking is supported (see :func:`CanGlobalHook`) and if
global hooking is active (see :func:`IsGlobalHookActive`).
)");
extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_StopGlobalHook();

DOCUMENT(R"(Determines if the global hook is active or not.

This function can only be called if global hooking is supported (see :func:`CanGlobalHook`).

:return: ``True`` if the hook is active, or ``False`` if the hook is inactive.
:rtype: ``bool``
)");
extern "C" RENDERDOC_API bool RENDERDOC_CC RENDERDOC_IsGlobalHookActive();

DOCUMENT(R"(Determines if the global hook is supported on the current platform and configuration.

:return: ``True`` if global hooking can be used on the platform, ``False`` if not.
:rtype: ``bool``
)");
extern "C" RENDERDOC_API bool RENDERDOC_CC RENDERDOC_CanGlobalHook();

DOCUMENT(R"(Launch an application and inject into it to allow capturing.

:param str app: The path to the application to run.
:param str workingDir: The working directory to use when running the application. If blank, the
  directory containing the application is used.
:param str cmdLine: The command line to use when running the application, it will be processed in a
  platform specific way to generate arguments.
:param list env: Any :class:`EnvironmentModification` that should be made when running the program.
:param str capturefile: The capture file path template, or blank to use a default location.
:param CaptureOptions opts: The capture options to use when injecting into the program.
:param bool waitForExit: If ``True`` this function will block until the process exits.
:return: The :class:`ExecuteResult` indicating both the status of the operation (success or failure)
  and any reason for failure, or else the ident where the new application is listening for target
  control if everything succeeded.
:rtype: ExecuteResult
)");
extern "C" RENDERDOC_API ExecuteResult RENDERDOC_CC
RENDERDOC_ExecuteAndInject(const char *app, const char *workingDir, const char *cmdLine,
                           const rdcarray<EnvironmentModification> &env, const char *capturefile,
                           const CaptureOptions &opts, bool waitForExit);

DOCUMENT(R"(Where supported by operating system and permissions, inject into a running process.

:param int pid: The Process ID (PID) to inject into.
:param list env: Any :class:`EnvironmentModification` that should be made when running the program.
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
                            const char *capturefile, const CaptureOptions &opts, bool waitForExit);

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

DOCUMENT("Structure containing all the information about vulkan layer registration");
struct VulkanLayerRegistrationInfo
{
  DOCUMENT(":class:`VulkanLayerFlags` detailing the current registration.");
  VulkanLayerFlags flags;

  DOCUMENT("A list of jsons that should be registered");
  rdcarray<rdcstr> myJSONs;

  DOCUMENT("A list of jsons that should be unregistered / updated");
  rdcarray<rdcstr> otherJSONs;
};

DOCUMENT("Internal function for determining vulkan layer registration status.");
extern "C" RENDERDOC_API bool RENDERDOC_CC
RENDERDOC_NeedVulkanLayerRegistration(VulkanLayerRegistrationInfo *info);

DOCUMENT("Internal function for updating vulkan layer registration.");
extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_UpdateVulkanLayerRegistration(bool systemLevel);

//////////////////////////////////////////////////////////////////////////
// Miscellaneous!
//////////////////////////////////////////////////////////////////////////

DOCUMENT("Internal function for updating installed version number in windows registry.");
extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_UpdateInstalledVersionNumber();

DOCUMENT("Internal function for initialising global process environment in a replay program.");
extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_InitGlobalEnv(GlobalEnvironment env,
                                                                   const rdcarray<rdcstr> &args);

DOCUMENT("Internal function for creating a bug report zip.");
extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_CreateBugReport(const char *logfile,
                                                                     const char *dumpfile,
                                                                     rdcstr &report);

DOCUMENT("Internal function for registering a memory region to be saved with crash dumps.");
extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_RegisterMemoryRegion(void *base, size_t size);

DOCUMENT("Internal function for unregistering a memory region to be saved with crash dumps.");
extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_UnregisterMemoryRegion(void *base);

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

DOCUMENT(R"(Retrieves the commit hash used to build.

This will be in the form "0123456789abcdef0123456789abcdef01234567"

:return: The commit hash.
:rtype: ``str``
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
:rtype: ``int``
)");
extern "C" RENDERDOC_API uint64_t RENDERDOC_CC RENDERDOC_GetCurrentProcessMemoryUsage();

DOCUMENT("Internal function for retrieving a config setting.");
extern "C" RENDERDOC_API const char *RENDERDOC_CC RENDERDOC_GetConfigSetting(const char *name);

DOCUMENT("Internal function for setting a config setting.");
extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_SetConfigSetting(const char *name,
                                                                      const char *value);

DOCUMENT("Internal function for setting UI theme colors.");
extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_SetColors(FloatVector darkChecker,
                                                               FloatVector lightChecker,
                                                               bool darkTheme);

DOCUMENT("Internal function for fetching friendly android names.");
extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_GetAndroidFriendlyName(const rdcstr &device,
                                                                            rdcstr &friendly);

DOCUMENT("Internal function for enumerating android devices.");
extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_EnumerateAndroidDevices(rdcstr *deviceList);

DOCUMENT("Internal function for initialising android use.");
extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_AndroidInitialise();

DOCUMENT("Internal function for shutting down android use.");
extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_AndroidShutdown();

DOCUMENT("Internal function for checking android support.");
extern "C" RENDERDOC_API bool RENDERDOC_CC RENDERDOC_IsAndroidSupported(const char *device);

DOCUMENT("Internal function for starting an android remote server.");
extern "C" RENDERDOC_API ReplayStatus RENDERDOC_CC
RENDERDOC_StartAndroidRemoteServer(const char *device);

DOCUMENT("Internal function for checking remote Android package for requirements");
extern "C" RENDERDOC_API void RENDERDOC_CC RENDERDOC_CheckAndroidPackage(
    const char *hostname, const char *packageAndActivity, AndroidFlags *flags);

DOCUMENT("Internal function that attempts to modify APK contents, adding debuggable flag.");
extern "C" RENDERDOC_API AndroidFlags RENDERDOC_CC RENDERDOC_MakeDebuggablePackage(
    const char *hostname, const char *packageAndActivity, RENDERDOC_ProgressCallback progress);

DOCUMENT("Internal function that runs unit tests.");
extern "C" RENDERDOC_API int RENDERDOC_CC RENDERDOC_RunUnitTests(const rdcstr &command,
                                                                 const rdcarray<rdcstr> &args);

DOCUMENT("Internal function that runs functional tests.");
extern "C" RENDERDOC_API int RENDERDOC_CC RENDERDOC_RunFunctionalTests(int pythonMinorVersion,
                                                                       const rdcarray<rdcstr> &args);
