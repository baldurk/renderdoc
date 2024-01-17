/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2024 Baldur Karlsson
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

// don't add any Qt headers visible to SWIG, as we don't want a Qt dependency for the SWIG-generated
// qrenderdoc module. Instead we should use public RDC types for any public QRenderDoc headers, and
// define conversions to/from Qt types. See rdcstr / QString, rdcpair / QPair, and
// rdcdatetime / QDateTime.
#include <functional>

// For string literals - use either tr() for translated strings, lit() for untranslated strings, or
// QFormatStr for the special case of literals without text used to format text with .arg().
//
// A default constructed QString() should be preferred to "".
//
// Instead of comparisons to "", use .isEmpty() - either !foo.isEmpty() for foo != "" or
// foo.isEmpty() for foo == "".

// this macro is fairly small/non-namespaced which is generally not good, but it's intended to
// be correspond to the tr() function in QObject, but for string literals. It makes the code a
// little bit more readable.
//
// If you have some text which should not be translated, then it should use lit().
#define lit(a) QStringLiteral(a)

// Same as lit() above, but only for formatting strings like QFormatStr("%1: %2[%3]").
// Note that tr() and lit() can format as well, so if there's text in the format string like
// QFormatStr("Sprocket thingy: %1.%2") then it should use either tr() or lit() depending on whether
// or not it should be translated
#define QFormatStr(fmt) QStringLiteral(fmt)

// this is pre-declared as an opaque type as we only support converting to QWidget* via PySide
class QWidget;
class QMenu;

// we only support QVariant as an 'internal' interface, it's not exposed to python. However we need
// to use it in constructors/operators so conditionally compile it rather than split small structs
// into interface/implementations
#if defined(SWIG) || defined(SWIG_GENERATED)

#define VARIANT_CAST(classname)

#else

#include <QVariant>

// conversion to/from QVariant
#define VARIANT_CAST(classname)   \
  classname(const QVariant &var); \
  operator QVariant() const;

// we also add some headers here that are only needed for Qt helpers in the replay interface, which
// is not exposed to swig
#define RENDERDOC_QT_COMPAT
#include <QColor>
#include <QDateTime>
#include <QList>
#include <QString>
#include <QVector>

#endif

// we depend on the internal RenderDoc API, but the bindings for that are imported entirely
#include "renderdoc_replay.h"

typedef rdcpair<rdcstr, rdcstr> rdcstrpair;
typedef rdcarray<rdcstrpair> rdcstrpairs;

struct ICaptureContext;

#include "Analytics.h"
#include "Extensions.h"
#include "PersistantConfig.h"
#include "RemoteHost.h"

DOCUMENT("Contains all of the settings that control how to capture an executable.");
struct CaptureSettings
{
  CaptureSettings();

  VARIANT_CAST(CaptureSettings);

  DOCUMENT(R"(The settings for the capture.
  
:type: renderdoc.CaptureOptions
)");
  CaptureOptions options;
  DOCUMENT(
      "``True`` if the described capture is an inject-into-process instead of a launched "
      "executable.");
  bool inject;
  DOCUMENT("``True`` if this capture settings object should be immediately executed upon load.");
  bool autoStart;
  DOCUMENT("The path to the executable to run.");
  rdcstr executable;
  DOCUMENT("The path to the working directory to run in, or blank for the executable's directory.");
  rdcstr workingDir;
  DOCUMENT("The command line to pass when running :data:`executable`.");
  rdcstr commandLine;
  DOCUMENT(R"(The environment changes to apply.

:type: List[renderdoc.EnvironmentModification]
)");
  rdcarray<EnvironmentModification> environment;
  DOCUMENT("The number of queued frames to capture, or 0 if no frames are queued to be captured.");
  uint32_t numQueuedFrames;
  DOCUMENT("The first queued frame to capture. Ignored if :data:`numQueuedFrames` is 0.");
  uint32_t queuedFrameCap;
};

DECLARE_REFLECTION_STRUCT(CaptureSettings);

DOCUMENT(R"(The main parent window of the application.

.. function:: ShortcutCallback(QWidget focusWidget)

  Not a member function - the signature for any ``ShortcutCallback`` callbacks.

  :param QWidget focusWidget: The widget with focus at the time this shortcut was detected. May be
     ``None``.
)");
struct IMainWindow
{
  typedef std::function<void(QWidget *focusWidget)> ShortcutCallback;

  DOCUMENT(R"(Retrieves the PySide2 QWidget for this :class:`MainWindow` if PySide2 is available, or otherwise
returns a unique opaque pointer that can be passed back to any RenderDoc functions expecting a
QWidget.

:return: Return the widget handle, either a PySide2 handle or an opaque handle.
:rtype: QWidget
)");
  virtual QWidget *Widget() = 0;

  DOCUMENT(R"(Register a callback for a particular key shortcut.

This creates a managed shortcut. Qt's shortcut system doesn't allow specialisation/duplication, so
you can't use ``Ctrl+S`` for a shortcut in a window to update some changes if there's also a global
``Ctrl+S`` shortcut on the window. In the end, neither shortcut will be called.

Instead this function allows the main window to manage shortcuts internally, and it will pick the
closest shortcut to a given action. The search goes from the widget with the focus currently up the
chain of parents, with the first match being used. If no matches are found, then a 'global' default
will be invoked, if it exists.

:param str shortcut: The text string representing the shortcut, e.g. 'Ctrl+S'.
:param QWidget widget: A handle to the widget to use as the context for this shortcut, or ``None``
  for a global shortcut. Note that if an existing global shortcut exists the new one will not be
  registered.
:param ShortcutCallback callback: The function to callback when the shortcut is hit.
)");
  virtual void RegisterShortcut(const rdcstr &shortcut, QWidget *widget,
                                ShortcutCallback callback) = 0;

  DOCUMENT(R"(Unregister a callback for a particular key shortcut, made in a previous call to
:meth:`RegisterShortcut`.

See the documentation for :meth:`RegisterShortcut` for what these shortcuts are for.

:param str shortcut: The text string representing the shortcut, e.g. 'Ctrl+S'. To unregister all
  shortcuts for a particular widget, you can pass an empty string here. In this case,
  :paramref:`UnregisterShortcut.widget` must not be ``None``.
:param QWidget widget: A handle to the widget used as the context for the shortcut, or ``None``
  if referring to a global shortcut.
)");
  virtual void UnregisterShortcut(const rdcstr &shortcut, QWidget *widget) = 0;

  DOCUMENT(R"(Attempts to bring the main window to the front to the user's focus.

This may not be possible on all OSs, so the function is not guaranteed to succeed.
)");
  virtual void BringToFront() = 0;

protected:
  IMainWindow() = default;
  ~IMainWindow() = default;
};

DECLARE_REFLECTION_STRUCT(IMainWindow);

DOCUMENT(R"(The event browser window.

.. function:: EventFilterCallback(context, filter, params, eventId, chunk, action, name)

  Not a member function - the signature for any ``EventFilterCallback`` callbacks.

  Called for each event in a capture when performing filtering in the Event Browser. The associated
  ``FilterParseCallback`` will be called first to parse the parameters, and is available for caching
  or syntax checking. The same filter name and params string will be passed to this function.

  :param CaptureContext context: The current capture context.
  :param str filter: The name of the filter function.
  :param str params: The parameters to the filter function.
  :param int eventId: The event's :data:`eventId <renderdoc.APIEvent.eventId>`.
  :param renderdoc.SDChunk chunk: The structured data chunk for this event.
  :param renderdoc.ActionDescription action: The action that contains this event. If the event is
    the action itself then the event ID will be equal.
  :param str name: The name of the event as shown in the event browser, for string-based filtering.
  :return: Whether or not this event matches the filter
  :rtype: bool

.. function:: FilterParseCallback(context, filter, params)

  Not a member function - the signature for any ``FilterParseCallback`` callbacks.

  Called once when the filter changes, to allow parsing any any data caching, as well as reporting
  of errors in the filter usage.

  :param CaptureContext context: The current capture context.
  :param str filter: The name of the filter function.
  :param str params: The parameters to the filter function.
  :return: An empty string if the parse succeeded, otherwise any error messages to be displayed to
    the user, such as syntax or other errors.
  :rtype: str

.. function:: AutoCompleteCallback(context, filter, params)

  Not a member function - the signature for any ``AutoCompleteCallback`` callbacks.

  Called when autocompletion is triggered inside a filter. The params passed are any previous
  text inside the filter's parameter list up to where the cursor is. The callback should return a
  list of identifiers used for auto-completion.

  The list does not have to be pre-filtered for matches to the :paramref:`params`, that is provided
  to allow different autocompletion at different stages (e.g. if there are no parameters, you can
  autocomplete a property, if a property is already present you can autocomplete valid values for
  it)

  :param CaptureContext context: The current capture context.
  :param str filter: The name of the filter function.
  :param str params: The previous parameter text to the filter function.
  :return: A list of strings giving identifiers to autocomplete, or an empty list of there are no
    such identifiers to prompt.
  :rtype: List[str]
)");
struct IEventBrowser
{
  typedef std::function<bool(ICaptureContext *, const rdcstr &, const rdcstr &, uint32_t,
                             const SDChunk *, const ActionDescription *, const rdcstr &)>
      EventFilterCallback;

  typedef std::function<rdcstr(ICaptureContext *, const rdcstr &, const rdcstr &)> FilterParseCallback;

  typedef std::function<rdcarray<rdcstr>(ICaptureContext *, const rdcstr &, const rdcstr &)>
      AutoCompleteCallback;

  DOCUMENT(R"(Retrieves the PySide2 QWidget for this :class:`EventBrowser` if PySide2 is available, or otherwise
returns a unique opaque pointer that can be passed back to any RenderDoc functions expecting a
QWidget.

:return: Return the widget handle, either a PySide2 handle or an opaque handle.
:rtype: QWidget
)");
  virtual QWidget *Widget() = 0;

  DOCUMENT("Updates the duration column if the selected time unit changes.");
  virtual void UpdateDurationColumn() = 0;

  DOCUMENT(R"(Uses the existing caching in the event browser to return a :class:`~renderdoc.APIEvent`
for a specified EID.

If no capture is loaded or the EID doesn't correspond to a known event, an empty struct will be
returned.

:param int eventId: The EID to look up.
:return: The event corresponding to the EID, or an empty struct if no such EID exists.
:rtype: renderdoc.APIEvent
)");
  virtual APIEvent GetAPIEventForEID(uint32_t eventId) = 0;

  DOCUMENT(R"(Uses the existing caching in the event browser to return a
:class:`~renderdoc.ActionDescription` for a specified EID. This action may not be the exact EID
specified, but it will be the action that the EID is associated with. I.e. if you specify the EID for
a state setting event the next action will be returned.

If no capture is loaded or the EID doesn't correspond to a known event, ``None`` will be returned.

:param int eventId: The EID to look up.
:return: The action containing the EID, or ``None`` if no such EID exists.
:rtype: renderdoc.ActionDescription
)");
  virtual const ActionDescription *GetActionForEID(uint32_t eventId) = 0;

  DOCUMENT(R"(Returns the formatted name of an event according to the current settings, whether
that be a custom name or an auto-generated name with/without parameter names.

If no capture is loaded or the EID doesn't correspond to a known event, an empty string will be
returned.

:param int eventId: The EID to look up.
:return: The formatted name of the specified event, or ``None`` if no such EID exists.
:rtype: str
)");
  virtual rdcstr GetEventName(uint32_t eventId) = 0;

  DOCUMENT(R"(Determines if a given EID is visible with the current filters applied to the event
browser.

If no capture is loaded or the EID doesn't correspond to a known event, ``False`` will be returned.

:param int eventId: The EID to look up.
:return: Whether or not the event is currently visible (passing the filters).
:rtype: bool
)");
  virtual bool IsAPIEventVisible(uint32_t eventId) = 0;

  DOCUMENT(R"(Registers a new event browser filter function.

Filter functions are available as $name() so long as they don't shadow an existing function. The
filter callback will be called for each event to filter.

The parser callback will be called once when a filter is first specified or the parameters change.
Note that a filter can be used multiple times in a filter expression! For this reason the parser
may be called multiple times and the filter callback takes the parameters string. If any expensive
work is done then the parameters can be used as a cache key to cache any data once per filter
expression.

:param str name: The name of the filter function.
:param str description: The description of the filter function. This should explain the available
  parameters (if applicable) and what the filter does. It will be used for documenting to users
  what each filter means.
:param EventFilterCallback filter: The callback to call for each candidate event to perform
  filtering.
:param FilterParseCallback parser: The callback to call when the parsing the parameters and checking
  for any errors. This can be ``None`` if no pre-parsing is required.
:param AutoCompleteCallback completer: The callback to call when trying to provide autocomplete
  suggestions. This can be ``None`` if no completion is desired/applicable.
:return: Whether or not the registration was successful.
:rtype: bool
)");
  virtual bool RegisterEventFilterFunction(const rdcstr &name, const rdcstr &description,
                                           EventFilterCallback filter, FilterParseCallback parser,
                                           AutoCompleteCallback completer) = 0;

  DOCUMENT(R"(Unregisters an event browser filter function that was previously registered.

:param str name: The name of the filter function.

:return: Whether or not the unregistration was successful.
:rtype: bool
)");
  virtual bool UnregisterEventFilterFunction(const rdcstr &name) = 0;

  DOCUMENT(R"(Sets the current filter text. This will not modify any saved filter but will modify
the scratch filter. The filter is applied immediately.

:param str text: The filter text.
)");
  virtual void SetCurrentFilterText(const rdcstr &text) = 0;

  DOCUMENT(R"(Returns the current filter text, whether temporary or a saved filter.

:return: The current filter text.
:rtype: str
)");
  virtual rdcstr GetCurrentFilterText() = 0;

  DOCUMENT(R"(Sets whether or not custom action names are used. Certain actions such as indirect
actions it is useful to show a custom action name which contains the actual indirect parameters
instead of the 'raw' parameters.

:param bool use: Whether or not custom action names will be used.
)");
  virtual void SetUseCustomActionNames(bool use) = 0;

  DOCUMENT(R"(Sets whether or not parameter names are shown in the events. If disabled, only the
value is shown and the parameter is implicit.

.. note::
  If custom action names are used this will not have an effect for any such actions. See
  :meth:`SetUseCustomActionNames`.

:param bool show: Whether or not parameter names will be shown.
)");
  virtual void SetShowParameterNames(bool show) = 0;

  DOCUMENT(R"(Sets whether or not all parameters are shown in the events. By default only
the most significant parameters are shown.

.. note::
  If custom action names are used this will not have an effect for any such actions. See
  :meth:`SetUseCustomActionNames`.

:param bool show: Whether or not parameter names will be shown.
)");
  virtual void SetShowAllParameters(bool show) = 0;

  DOCUMENT(R"(Sets whether or not marker regions which have no visible actions.

:param bool show: Whether or not empty regions after filtering will be shown.
)");
  virtual void SetEmptyRegionsVisible(bool show) = 0;

protected:
  IEventBrowser() = default;
  ~IEventBrowser() = default;
};

DECLARE_REFLECTION_STRUCT(IEventBrowser);

DOCUMENT("The API inspector window.");
struct IAPIInspector
{
  DOCUMENT(R"(Retrieves the PySide2 QWidget for this :class:`APIInspector` if PySide2 is available, or otherwise
returns a unique opaque pointer that can be passed back to any RenderDoc functions expecting a
QWidget.

:return: Return the widget handle, either a PySide2 handle or an opaque handle.
:rtype: QWidget
)");
  virtual QWidget *Widget() = 0;

  DOCUMENT("Refresh the current API view - useful if callstacks are now available.");
  virtual void Refresh() = 0;

  DOCUMENT(R"(Expand the API view to reveal a given parameter and select it.

:param renderdoc.SDObject param: The parameter to reveal and select.
)");
  virtual void RevealParameter(SDObject *param) = 0;

protected:
  IAPIInspector() = default;
  ~IAPIInspector() = default;
};

DECLARE_REFLECTION_STRUCT(IAPIInspector);

DOCUMENT(R"(Specifies a pipeline stage for the :class:`PipelineStateViewer`.

.. data:: VertexInput

  The fixed function vertex input stage.

.. data:: VertexShader

  The vertex shader.

.. data:: HullShader

  The vertex shader.

.. data:: TessControlShader

  The tessellation control shader.

.. data:: DomainShader

  The domain shader.

.. data:: TessEvalShader

  The tessellation evaluation shader.

.. data:: GeometryShader

  The geometry shader, including stream-out/transform feedback.

.. data:: Rasterizer

  The fixed function rasterizer stage.

.. data:: ViewportsScissors

  The viewports and scissors. Helper alias for :data:`Rasterizer`.

.. data:: PixelShader

  The pixel shader.

.. data:: FragmentShader

  The fragment shader.

.. data:: ColorDepthOutput

  The fixed function color and depth output stage, including color blending and depth/stencil
  testing state.

.. data:: Blending

  The color blending state. Helper alias for :data:`ColorDepthOutput`.

.. data:: DepthTest

  The depth test state. Helper alias for :data:`ColorDepthOutput`.

.. data:: StencilTest

  The stencil test state. Helper alias for :data:`ColorDepthOutput`.

.. data:: ComputeShader

  The compute shader.

.. data:: SampleMask

  The sample mask.
)");
enum class PipelineStage : int
{
  VertexInput = 0,

  VertexShader,
  HullShader,
  TessControlShader = HullShader,
  DomainShader,
  TessEvalShader = DomainShader,
  GeometryShader,
  Rasterizer,
  ViewportsScissors = Rasterizer,
  PixelShader,
  FragmentShader = PixelShader,
  ColorDepthOutput,
  Blending = ColorDepthOutput,
  DepthTest = ColorDepthOutput,
  StencilTest = ColorDepthOutput,
  ComputeShader,

  // these vary by API
  SampleMask,
};

DOCUMENT("The pipeline state viewer window.");
struct IPipelineStateViewer
{
  DOCUMENT(R"(Retrieves the PySide2 QWidget for this :class:`PipelineStateViewer` if PySide2 is available, or otherwise
returns a unique opaque pointer that can be passed back to any RenderDoc functions expecting a
QWidget.

:return: Return the widget handle, either a PySide2 handle or an opaque handle.
:rtype: QWidget
)");
  virtual QWidget *Widget() = 0;

  DOCUMENT(R"(Prompt the user to save the binary form of the given shader to disk.

:param renderdoc.ShaderReflection shader: The shader reflection data to save.
:return: ``True`` if the shader was saved successfully, ``False`` if an error occurred.
:rtype: bool
)");
  virtual bool SaveShaderFile(const ShaderReflection *shader) = 0;

  DOCUMENT(R"(Select a given pipeline stage in the viewer.

:param PipelineStage stage: The stage to select.
)");
  virtual void SelectPipelineStage(PipelineStage stage) = 0;

protected:
  IPipelineStateViewer() = default;
  ~IPipelineStateViewer() = default;
};

DECLARE_REFLECTION_STRUCT(IPipelineStateViewer);

DOCUMENT(R"(Specifies a type of followed resource for the :class:`TextureViewer`.

.. data:: OutputColor

  The index specifies which output color target to select. Shader stage and array index are ignored.

.. data:: OutputDepth

  The resource followed is the depth/stencil output target. All other parameters are ignored.

.. data:: ReadWrite

  The index specifies a resource within the given shader's
  :data:`read-write resources <~renderdoc.ShaderReflection.readWriteResources>`. The array element
  then specifies the index within that resource's array, if applicable.

.. data:: ReadOnly

  The index specifies a resource within the given shader's
  :data:`read-only resources <~renderdoc.ShaderReflection.readOnlyResources>`. The array element
  then specifies the index within that resource's array, if applicable.

.. data:: OutputDepthResolve

  The resource followed is the depth/stencil resolve output target. All other parameters are ignored.
)");
enum class FollowType : int
{
  OutputColor,
  OutputDepth,
  ReadWrite,
  ReadOnly,
  OutputDepthResolve
};

DOCUMENT("The texture viewer window.");
struct ITextureViewer
{
  DOCUMENT(R"(Retrieves the PySide2 QWidget for this :class:`TextureViewer` if PySide2 is available, or otherwise
returns a unique opaque pointer that can be passed back to any RenderDoc functions expecting a
QWidget.

:return: Return the widget handle, either a PySide2 handle or an opaque handle.
:rtype: QWidget
)");
  virtual QWidget *Widget() = 0;

  DOCUMENT(R"(Open a texture view, optionally raising this window to the foreground.

:param renderdoc.ResourceId resourceId: The ID of the texture to view.
:param renderdoc.CompType typeCast: If possible interpret the texture with this type instead of its
  normal type. If set to :data:`~renderdoc.CompType.Typeless` then no cast is applied, otherwise
  where allowed the texture data will be reinterpreted - e.g. from unsigned integers to floats, or
  to unsigned normalised values.
:param bool focus: ``True`` if the :class:`TextureViewer` should be raised.
)");
  virtual void ViewTexture(ResourceId resourceId, CompType typeCast, bool focus) = 0;

  DOCUMENT(R"(Select the 'following' view and choose which resource slot to follow.

:param FollowType followType: The type of followed resource.
:param renderdoc.ShaderStage stage: The shader stage of the shader reflection data to look up.
:param int index: The index within the given resource list (if applicable) to follow.
:param int arrayElement: The index within the given resource array (if applicable) to follow.
)");
  virtual void ViewFollowedResource(FollowType followType, ShaderStage stage, int32_t index,
                                    int32_t arrayElement) = 0;

  DOCUMENT(R"(Return which resource is currently being displayed in the active tab.

:return: The ID of the resource being displayed.
:rtype: renderdoc.ResourceId
)");
  virtual ResourceId GetCurrentResource() = 0;

  DOCUMENT(R"(Return which subresource is currently selected for viewing.

:return: The subresource currently selected.
:rtype: renderdoc.Subresource
)");
  virtual Subresource GetSelectedSubresource() = 0;

  DOCUMENT(R"(Select a particular subresource within the currently selected texture. Any out of
bounds parameters will be clamped to the available subresources.

:param renderdoc.Subresource sub: The subresource to select.
)");
  virtual void SetSelectedSubresource(Subresource sub) = 0;

  DOCUMENT(R"(Highlights the given pixel location in the current texture.

:param int x: The X co-ordinate.
:param int y: The Y co-ordinate.
)");
  virtual void GotoLocation(uint32_t x, uint32_t y) = 0;

  DOCUMENT(R"(Returns the currently selected texel location in the current texture.

If no location is currently selected or there is no current texture, this will return ``(-1, -1)``.

:return: The currently picked pixel location.
:rtype: Tuple[int,int]
)");
  virtual rdcpair<int32_t, int32_t> GetPickedLocation() = 0;

  DOCUMENT(R"(Return the currently selected texture overlay.

:return: The currently selected texture overlay.
:rtype: renderdoc.DebugOverlay
)");
  virtual DebugOverlay GetTextureOverlay() = 0;

  DOCUMENT(R"(Changes the currently selected overlay the given pixel location in the current texture.

:param renderdoc.DebugOverlay overlay: The overlay to enable.
)");
  virtual void SetTextureOverlay(DebugOverlay overlay) = 0;

  DOCUMENT(R"(Return whether or not the texture viewer is currently auto-fitting the zoom level.

:return: ``True`` if the zoom level is currently auto-fitting.
:rtype: bool
)");
  virtual bool IsZoomAutoFit() = 0;

  DOCUMENT(R"(Return the current zoom level, whether manually set or auto-calculated.

:return: The current zoom level, with 100% being represented as 1.0.
:rtype: float
)");
  virtual float GetZoomLevel() = 0;

  DOCUMENT(R"(Set the zoom level for displaying textures.

:param bool autofit: ``True`` if the zoom level should be auto-calculated continuously to
  automatically fit the texture completely in view.
:param float zoom: The zoom level as a percentage, with 100% being 1.0. Ignored if
  :paramref:`autofit` is ``True``.
)");
  virtual void SetZoomLevel(bool autofit, float zoom) = 0;

  DOCUMENT(R"(Return the current histogram blackpoint to whitepoint range.

:return: The current histogram range.
:rtype: Tuple[float,float]
)");
  virtual rdcpair<float, float> GetHistogramRange() = 0;

  DOCUMENT(R"(Set the current histogram blackpoint to whitepoint range.

:param float blackpoint: The value that should be mapped to black, component-wise.
:param float whitepoint: The value that should be mapped to white, component-wise.
)");
  virtual void SetHistogramRange(float blackpoint, float whitepoint) = 0;

  DOCUMENT(R"(Return which channels are currently displayed, as a bitmask.

If red is visible ``0x1`` will be set in the returned value, if blue is visible ``0x2`` will be set,
etc.

:return: The current bitmask showing channel visibility.
:rtype: int
)");
  virtual uint32_t GetChannelVisibilityBits() = 0;

  DOCUMENT(R"(Set the visibility of each channel.

:param bool red: Whether the red channel should be visible.
:param bool green: Whether the green channel should be visible.
:param bool blue: Whether the blue channel should be visible.
:param bool alpha: Whether the alpha channel should be visible.
)");
  virtual void SetChannelVisibility(bool red, bool green, bool blue, bool alpha) = 0;

protected:
  ITextureViewer() = default;
  ~ITextureViewer() = default;
};

DECLARE_REFLECTION_STRUCT(ITextureViewer);

DOCUMENT("The buffer viewer window, either a raw buffer or the geometry pipeline.");
struct IBufferViewer
{
  DOCUMENT(R"(Retrieves the PySide2 QWidget for this :class:`BufferViewer` if PySide2 is available, or otherwise
returns a unique opaque pointer that can be passed back to any RenderDoc functions expecting a
QWidget.

:return: Return the widget handle, either a PySide2 handle or an opaque handle.
:rtype: QWidget
)");
  virtual QWidget *Widget() = 0;

  DOCUMENT(R"(Scroll to the given row in the given stage's data.

:param int row: the row to scroll to.
:param renderdoc.MeshDataStage stage: The stage of the geometry pipeline to scroll within.
)");
  virtual void ScrollToRow(int32_t row, MeshDataStage stage = MeshDataStage::VSIn) = 0;

  DOCUMENT(R"(Scroll to the given column in the given stage's data.

:param int column: the column to scroll to.
:param renderdoc.MeshDataStage stage: The stage of the geometry pipeline to scroll within.
)");
  virtual void ScrollToColumn(int32_t column, MeshDataStage stage = MeshDataStage::VSIn) = 0;

  DOCUMENT(R"(Ensure the given stage's data is visible and raised, if it wasn't before.

:param renderdoc.MeshDataStage stage: The stage of the geometry pipeline to show data for.
)");
  virtual void ShowMeshData(MeshDataStage stage) = 0;

  DOCUMENT(R"(For a mesh view, set the current instance. This is ignored when called on a raw buffer
view.

:param int instance: The instance to select, will be clamped to the range [0, numInstances-1]
)");
  virtual void SetCurrentInstance(int32_t instance) = 0;

  DOCUMENT(R"(For a mesh view, set the current multiview view. This is ignored when called on a raw
buffer view.

:param int view: The view to select, will be clamped to the range [0, numViews-1]
)");
  virtual void SetCurrentView(int32_t view) = 0;

  DOCUMENT(R"(For a mesh view, set the current preview stage. This is ignored when called on a raw
buffer view.

:param renderdoc.MeshDataStage stage: The stage to show
)");
  virtual void SetPreviewStage(MeshDataStage stage) = 0;

protected:
  IBufferViewer() = default;
  ~IBufferViewer() = default;
};

DECLARE_REFLECTION_STRUCT(IBufferViewer);

DOCUMENT("The Resource inspector window.");
struct IResourceInspector
{
  DOCUMENT(R"(Retrieves the PySide2 QWidget for this :class:`ResourceInspector` if PySide2 is available, or otherwise
returns a unique opaque pointer that can be passed back to any RenderDoc functions expecting a
QWidget.

:return: Return the widget handle, either a PySide2 handle or an opaque handle.
:rtype: QWidget
)");
  virtual QWidget *Widget() = 0;

  DOCUMENT(R"(Change the current resource being inspected.

:param renderdoc.ResourceId id: The ID of the resource to inspect.
)");
  virtual void Inspect(ResourceId id) = 0;

  DOCUMENT(R"(Return which resource is currently being inspected.

:return: The ID of the resource being inspected.
:rtype: renderdoc.ResourceId
)");
  virtual ResourceId CurrentResource() = 0;

  DOCUMENT(R"(Expand the resource initialisation chunks to reveal and select a given parameter.

:param renderdoc.SDObject param: The parameter to reveal and select.
)");
  virtual void RevealParameter(SDObject *param) = 0;

protected:
  IResourceInspector() = default;
  ~IResourceInspector() = default;
};

DECLARE_REFLECTION_STRUCT(IResourceInspector);

DOCUMENT("The executable capture window.");
struct ICaptureDialog
{
  DOCUMENT(R"(Retrieves the PySide2 QWidget for this :class:`CaptureDialog` if PySide2 is available, or otherwise
returns a unique opaque pointer that can be passed back to any RenderDoc functions expecting a
QWidget.

:return: Return the widget handle, either a PySide2 handle or an opaque handle.
:rtype: QWidget
)");
  virtual QWidget *Widget() = 0;

  DOCUMENT(R"(Determines if the window is in inject or launch mode.

:return: ``True`` if the window is set up for injecting.
:rtype: bool
)");
  virtual bool IsInjectMode() = 0;
  DOCUMENT(R"(Switches the window to or from inject mode.

:param bool inject: ``True`` if the window should configure for injecting into processes.
)");
  virtual void SetInjectMode(bool inject) = 0;

  DOCUMENT(R"(Sets the executable filename to capture.

:param str filename: The filename to execute.
)");
  virtual void SetExecutableFilename(const rdcstr &filename) = 0;

  DOCUMENT(R"(Sets the working directory for capture.

:param str dir: The directory to use.
)");
  virtual void SetWorkingDirectory(const rdcstr &dir) = 0;

  DOCUMENT(R"(Sets the command line string to use when launching an executable.

:param str cmd: The command line to use.
)");
  virtual void SetCommandLine(const rdcstr &cmd) = 0;

  DOCUMENT(R"(Sets the list of environment modifications to apply when launching.

:param List[renderdoc.EnvironmentModification] modifications: The list of modifications to apply.
)");
  virtual void SetEnvironmentModifications(const rdcarray<EnvironmentModification> &modifications) = 0;

  DOCUMENT(R"(Configures the window based on a bulk structure of settings.

:param CaptureSettings settings: The settings to apply.
)");
  virtual void SetSettings(CaptureSettings settings) = 0;

  DOCUMENT(R"(Retrieves the current state of the window as a structure of settings.

:return: The settings describing the current window state.
:rtype: CaptureSettings
)");
  virtual CaptureSettings Settings() = 0;

  DOCUMENT("Launches a capture of the current executable.");
  virtual void TriggerCapture() = 0;

  DOCUMENT(R"(Loads settings from a file and applies them. See :meth:`SetSettings`.

:param str filename: The filename to load the settings from.
)");
  virtual void LoadSettings(const rdcstr &filename) = 0;

  DOCUMENT(R"(Saves the current settings to a file. See :meth:`Settings`.

:param str filename: The filename to save the settings to.
)");
  virtual void SaveSettings(const rdcstr &filename) = 0;

  DOCUMENT("Update the current state of the global hook, e.g. if it has been enabled.");
  virtual void UpdateGlobalHook() = 0;

  DOCUMENT("Update the current state based on the current remote host, when that changes.");
  virtual void UpdateRemoteHost() = 0;

protected:
  ICaptureDialog() = default;
  ~ICaptureDialog() = default;
};

DECLARE_REFLECTION_STRUCT(ICaptureDialog);

DOCUMENT("The debug warnings and errors window.");
struct IDebugMessageView
{
  DOCUMENT(R"(Retrieves the PySide2 QWidget for this :class:`DebugMessageView` if PySide2 is available, or otherwise
returns a unique opaque pointer that can be passed back to any RenderDoc functions expecting a
QWidget.

:return: Return the widget handle, either a PySide2 handle or an opaque handle.
:rtype: QWidget
)");
  virtual QWidget *Widget() = 0;

protected:
  IDebugMessageView() = default;
  ~IDebugMessageView() = default;
};

DECLARE_REFLECTION_STRUCT(IDebugMessageView);

DOCUMENT("The diagnostic log viewing window.");
struct IDiagnosticLogView
{
  DOCUMENT(R"(Retrieves the PySide2 QWidget for this :class:`DiagnosticLogView` if PySide2 is available, or otherwise
returns a unique opaque pointer that can be passed back to any RenderDoc functions expecting a
QWidget.

:return: Return the widget handle, either a PySide2 handle or an opaque handle.
:rtype: QWidget
)");
  virtual QWidget *Widget() = 0;

protected:
  IDiagnosticLogView() = default;
  ~IDiagnosticLogView() = default;
};

DECLARE_REFLECTION_STRUCT(IDiagnosticLogView);

DOCUMENT("The capture comments window.");
struct ICommentView
{
  DOCUMENT(R"(Retrieves the PySide2 QWidget for this :class:`CommentView` if PySide2 is available, or otherwise
returns a unique opaque pointer that can be passed back to any RenderDoc functions expecting a
QWidget.

:return: Return the widget handle, either a PySide2 handle or an opaque handle.
:rtype: QWidget
)");
  virtual QWidget *Widget() = 0;

  DOCUMENT(R"(Sets the current comments text.

:param str text: The new comments text.
)");
  virtual void SetComments(const rdcstr &text) = 0;

  DOCUMENT(R"(Gets the current comments text.

:return: The current comments text.
:rtype: str
)");
  virtual rdcstr GetComments() = 0;

protected:
  ICommentView() = default;
  ~ICommentView() = default;
};

DECLARE_REFLECTION_STRUCT(ICommentView);

DOCUMENT("The statistics window.");
struct IStatisticsViewer
{
  DOCUMENT(R"(Retrieves the PySide2 QWidget for this :class:`StatisticsViewer` if PySide2 is available, or otherwise
returns a unique opaque pointer that can be passed back to any RenderDoc functions expecting a
QWidget.

:return: Return the widget handle, either a PySide2 handle or an opaque handle.
:rtype: QWidget
)");
  virtual QWidget *Widget() = 0;

protected:
  IStatisticsViewer() = default;
  ~IStatisticsViewer() = default;
};

DOCUMENT("The timeline bar.");
struct ITimelineBar
{
  DOCUMENT(R"(Retrieves the PySide2 QWidget for this :class:`TimelineBar` if PySide2 is available, or otherwise
returns a unique opaque pointer that can be passed back to any RenderDoc functions expecting a
QWidget.

:return: Return the widget handle, either a PySide2 handle or an opaque handle.
:rtype: QWidget
)");
  virtual QWidget *Widget() = 0;

  DOCUMENT(R"(Highlights the frame usage of the specified resource.

:param renderdoc.ResourceId id: The ID of the resource to highlight.
)");
  virtual void HighlightResourceUsage(ResourceId id) = 0;

  DOCUMENT(R"(Highlights the modifications in a frame of a given resource.

:param renderdoc.ResourceId id: The ID of the resource that is being modified.
:param List[renderdoc.PixelModification] history: A list of pixel events to display.
)");
  virtual void HighlightHistory(ResourceId id, const rdcarray<PixelModification> &history) = 0;

protected:
  ITimelineBar() = default;
  ~ITimelineBar() = default;
};

DECLARE_REFLECTION_STRUCT(IStatisticsViewer);

DOCUMENT("The performance counter view window.");
struct IPerformanceCounterViewer
{
  DOCUMENT(R"(Retrieves the PySide2 QWidget for this :class:`PerformanceCounterViewer` if PySide2 is available, or otherwise
returns a unique opaque pointer that can be passed back to any RenderDoc functions expecting a
QWidget.

:return: Return the widget handle, either a PySide2 handle or an opaque handle.
:rtype: QWidget
)");
  virtual QWidget *Widget() = 0;

  DOCUMENT("Updates duration columns if the selected time unit changes.");
  virtual void UpdateDurationColumn() = 0;

protected:
  IPerformanceCounterViewer() = default;
  ~IPerformanceCounterViewer() = default;
};

DECLARE_REFLECTION_STRUCT(IPerformanceCounterViewer);

DOCUMENT("The interactive python shell.");
struct IPythonShell
{
  DOCUMENT(R"(Retrieves the PySide2 QWidget for this :class:`PythonShell` if PySide2 is available, or otherwise
returns a unique opaque pointer that can be passed back to any RenderDoc functions expecting a
QWidget.

:return: Return the widget handle, either a PySide2 handle or an opaque handle.
:rtype: QWidget
)");
  virtual QWidget *Widget() = 0;

  DOCUMENT(R"(Sets the current script in the python shell to the given string.

:param str script: The text of the script to set.
)");
  virtual void SetScriptText(rdcstr script) = 0;

  DOCUMENT(R"(Sets the current script in the python shell to the contents of the given file.

:param str filename: The filename of the script to load.
:return: Whether or not the script was successfully loaded.
:rtype: bool
)");
  virtual bool LoadScriptFromFilename(rdcstr filename) = 0;

  DOCUMENT(R"(Returns the current script text.

:return: The current script text.
:rtype: str
)");
  virtual rdcstr GetScriptText() = 0;

  DOCUMENT(R"(Runs the current script in the python shell.
)");
  virtual void RunScript() = 0;

protected:
  IPythonShell() = default;
  ~IPythonShell() = default;
};

DECLARE_REFLECTION_STRUCT(IPythonShell);

DOCUMENT(R"(A shader window used for viewing, editing, or debugging.

.. function:: SaveCallback(context, viewer, encoding, flags, entry, compiled)

  Not a member function - the signature for any ``SaveCallback`` callbacks.

  Called whenever a shader viewer that was open for editing triggers a save/update.

  :param CaptureContext context: The current capture context.
  :param ShaderViewer viewer: The open shader viewer.
  :param renderdoc.ResourceId id: The id of the shader being replaced.
  :param renderdoc.ShaderStage stage: The shader stage of the shader being replaced.
  :param renderdoc.ShaderEncoding encoding: The encoding of the files being passed.
  :param renderdoc.ShaderCompileFlags flags: The flags to use during compilation.
  :param str entryFunc: The name of the entry point.
  :param bytes source: The byte buffer containing the source - may just be text depending on the
    encoding.

.. function:: RevertCallback(context)

  Not a member function - the signature for any ``RevertCallback`` callbacks.

  Called whenever a shader viewer that was open for editing is closed.

  :param CaptureContext context: The current capture context.
  :param ShaderViewer viewer: The open shader viewer.
  :param renderdoc.ResourceId id: The id of the shader being replaced.
)");
struct IShaderViewer
{
  typedef std::function<void(ICaptureContext *, IShaderViewer *, ResourceId, ShaderStage,
                             ShaderEncoding, ShaderCompileFlags, rdcstr, bytebuf)>
      SaveCallback;
  typedef std::function<void(ICaptureContext *, IShaderViewer *, ResourceId)> RevertCallback;

  DOCUMENT(R"(Retrieves the PySide2 QWidget for this :class:`ShaderViewer` if PySide2 is available, or otherwise
returns a unique opaque pointer that can be passed back to any RenderDoc functions expecting a
QWidget.

:return: Return the widget handle, either a PySide2 handle or an opaque handle.
:rtype: QWidget
)");
  virtual QWidget *Widget() = 0;

  DOCUMENT(R"(Retrieves the current step in the debugging.

:return: The current step.
:rtype: int
)");
  virtual uint32_t CurrentStep() = 0;

  DOCUMENT(R"(Sets the current step in the debugging.

:param int step: The current step to jump to.
)");
  virtual void SetCurrentStep(uint32_t step) = 0;

  DOCUMENT(R"(Toggles a breakpoint at a given instruction.

:param int instruction: The instruction to toggle breakpoint at. If this is ``-1`` the nearest
  instruction after the current caret position is used.
)");
  virtual void ToggleBreakpointOnInstruction(int32_t instruction = -1) = 0;

  DOCUMENT(R"(Toggles a breakpoint at a given disassembly line (starting from 1).

:param int disassemblyLine: The line of the disassembly to toggle a breakpoint on.
)");
  virtual void ToggleBreakpointOnDisassemblyLine(int32_t disassemblyLine) = 0;

  DOCUMENT(R"(Runs execution forward to the next breakpoint, or the end of the trace.
)");
  virtual void RunForward() = 0;

  DOCUMENT(R"(Show a list of shader compilation errors or warnings.

:param str errors: The string of errors or warnings to display.
)");
  virtual void ShowErrors(const rdcstr &errors) = 0;

  DOCUMENT(R"(Add an expression to the watch panel.

:param str expression: The name of the expression to watch.
)");
  virtual void AddWatch(const rdcstr &expression) = 0;

  DOCUMENT(R"(Return the current text of source files within the viewer. Primarily useful for
returning any edits applied when editing a shader.

:return: The current file contents as a list of (filename, contents) pairs.
:rtype: List[Tuple[str,str]]
)");
  virtual rdcstrpairs GetCurrentFileContents() = 0;

protected:
  IShaderViewer() = default;
  ~IShaderViewer() = default;
};

DECLARE_REFLECTION_STRUCT(IShaderViewer);

DOCUMENT("A shader message list window.");
struct IShaderMessageViewer
{
  DOCUMENT(R"(Retrieves the PySide2 QWidget for this :class:`ShaderMessageViewer` if PySide2 is available, or otherwise
returns a unique opaque pointer that can be passed back to any RenderDoc functions expecting a
QWidget.

:return: Return the widget handle, either a PySide2 handle or an opaque handle.
:rtype: QWidget
)");
  virtual QWidget *Widget() = 0;

  DOCUMENT(R"(Return the EID that this viewer is displaying messages from.

:return: The EID.
:rtype: int
)");
  virtual uint32_t GetEvent() = 0;

  DOCUMENT(R"(Return the shader messages displayed in this viewer.

:return: The shader messages.
:rtype: List[renderdoc.ShaderMessage]
)");
  virtual rdcarray<ShaderMessage> GetShaderMessages() = 0;

  DOCUMENT(R"(Returns whether or not this viewer is out of date - if the shaders have been edited
since the messages were fetched.

:return: ``True`` if the viewer is out of date.
:rtype: bool
)");
  virtual bool IsOutOfDate() = 0;

protected:
  IShaderMessageViewer() = default;
  ~IShaderMessageViewer() = default;
};

DECLARE_REFLECTION_STRUCT(IShaderMessageViewer);

DOCUMENT("A pixel history window.");
struct IPixelHistoryView
{
  DOCUMENT(R"(Retrieves the PySide2 QWidget for this :class:`PixelHistoryView` if PySide2 is available, or otherwise
returns a unique opaque pointer that can be passed back to any RenderDoc functions expecting a
QWidget.

:return: Return the widget handle, either a PySide2 handle or an opaque handle.
:rtype: QWidget
)");
  virtual QWidget *Widget() = 0;

  DOCUMENT(R"(Set the history displayed in this window.

:param List[renderdoc.PixelModification] history: A list of pixel events to display.
)");
  virtual void SetHistory(const rdcarray<PixelModification> &history) = 0;

protected:
  IPixelHistoryView() = default;
  ~IPixelHistoryView() = default;
};

DECLARE_REFLECTION_STRUCT(IPixelHistoryView);

DOCUMENT("An interface implemented by any object wanting to be notified of capture events.");
struct ICaptureViewer
{
  DOCUMENT("Called whenever a capture is opened.");
  virtual void OnCaptureLoaded() = 0;

  DOCUMENT("Called whenever a capture is closed.");
  virtual void OnCaptureClosed() = 0;

  DOCUMENT(R"(Called whenever the current selected event changes. This is distinct from the actual
effective current event, since for example selecting a marker region will change the current event
to be the last event inside that region, to be consistent with selecting an item reflecting the
current state after that item.

The selected event shows the :data:`eventId <renderdoc.APIEvent.eventId>` that was actually selected,
which will usually but not always be the same as the current effective
:data:`eventId <renderdoc.APIEvent.eventId>`.

The distinction for this callback is not normally desired, instead use :meth:`OnEventChanged` to
be notified whenever the current event changes. The API inspector uses this to display API events up
to a marker region.

:param int eventId: The new :data:`eventId <renderdoc.APIEvent.eventId>`.
)");
  virtual void OnSelectedEventChanged(uint32_t eventId) = 0;

  DOCUMENT(R"(Called whenever the effective current event changes.

:param int eventId: The new :data:`eventId <renderdoc.APIEvent.eventId>`.
)");
  virtual void OnEventChanged(uint32_t eventId) = 0;

protected:
  ICaptureViewer() = default;
  virtual ~ICaptureViewer() = default;
};

DECLARE_REFLECTION_STRUCT(ICaptureViewer);
DECLARE_REFLECTION_STRUCT(ICaptureViewer *);

DOCUMENT(R"(A manager for accessing the underlying replay information that isn't already abstracted
in UI side structures. This manager controls and serialises access to the underlying
:class:`~renderdoc.ReplayController`, as well as handling remote server connections.

.. function:: InvokeCallback(controller)

  Not a member function - the signature for any ``InvokeCallback`` callbacks.

  :param renderdoc.ReplayController controller: The controller to access. Must not be cached or
    used after the callback returns.

.. function:: DirectoryBrowseCallback(path, entries)

  Not a member function - the signature for any ``DirectoryBrowseCallback`` callbacks.

  :param str path: The path that was queried for.
  :param List[renderdoc.PathEntry] entries: The entries underneath the path, as relevant.
)");
struct IReplayManager
{
  typedef std::function<void(IReplayController *)> InvokeCallback;
  typedef std::function<void(const rdcstr &, const rdcarray<PathEntry> &)> DirectoryBrowseCallback;

  DOCUMENT(R"(Delete a capture file, whether local or remote.

:param str capturefile: The path to the file.
:param bool local: ``True`` if the file is on the local machine.
)");
  virtual void DeleteCapture(const rdcstr &capturefile, bool local) = 0;

  DOCUMENT(R"(Connect to a remote server.

:param RemoteHost host: The host to connect to.
:return: Whether or not the connection was successful.
:rtype: renderdoc.ResultDetails
)");
  virtual ResultDetails ConnectToRemoteServer(RemoteHost host) = 0;

  DOCUMENT("Disconnect from the server the manager is currently connected to.");
  virtual void DisconnectFromRemoteServer() = 0;

  DOCUMENT("Shutdown the server the manager is currently connected to.");
  virtual void ShutdownServer() = 0;

  DOCUMENT("Ping the remote server to ensure the connection is still alive.");
  virtual void PingRemote() = 0;

  DOCUMENT("Cancels the active replay loop. See :meth:`~renderdoc.ReplayController.ReplayLoop`.");
  virtual void CancelReplayLoop() = 0;

  DOCUMENT(R"(Retrieves the host that the manager is currently connected to.

:return: The host connected to, or an invalid RemoteHost if no connection is active.
:rtype: RemoteHost
)");
  virtual RemoteHost CurrentRemote() = 0;

  DOCUMENT(R"(Retrieves the capture access handle for the currently open file.

:return: The file handle active, or ``None`` if no capture is open.
:rtype: renderdoc.CaptureAccess
)");
  virtual ICaptureAccess *GetCaptureAccess() = 0;

  DOCUMENT(R"(Retrieves the capture file handle for the currently open file, if it is available.

If the capture is not open locally this will not be available, and only :meth:`GetCaptureAccess`
will be usable.

:return: The file handle active, or ``None`` if no capture is open or the capture is only available
  remotely.
:rtype: renderdoc.CaptureFile
)");
  virtual ICaptureFile *GetCaptureFile() = 0;

  DOCUMENT(R"(Launch an application and inject into it to allow capturing.

This happens either locally, or on the remote server, depending on whether a connection is active.

:param str exe: The path to the executable to run.
:param str workingDir: The working directory to use when running the application. If blank, the
  directory containing the executable is used.
:param str cmdLine: The command line to use when running the executable, it will be processed in a
  platform specific way to generate arguments.
:param List[renderdoc.EnvironmentModification] env: Any environment changes that should be made when
  running the program.
:param str capturefile: The location to save any captures, if running locally.
:param renderdoc.CaptureOptions opts: The capture options to use when injecting into the program.
:return: The :class:`~renderdoc.ExecuteResult` indicating both the status of the operation (success
  or failure) and any reason for failure, or else the ident where the new application is listening
  for target control if everything succeeded.
:rtype: renderdoc.ExecuteResult
)");
  virtual ExecuteResult ExecuteAndInject(const rdcstr &exe, const rdcstr &workingDir,
                                         const rdcstr &cmdLine,
                                         const rdcarray<EnvironmentModification> &env,
                                         const rdcstr &capturefile, CaptureOptions opts) = 0;

  DOCUMENT(R"(Retrieve a list of drivers that the current remote server supports.

:return: The list of supported replay drivers.
:rtype: List[str]
)");
  virtual rdcarray<rdcstr> GetRemoteSupport() = 0;

  DOCUMENT(R"(Query the remote host for its home directory.

If a capture is open, the callback will happen on the replay thread, otherwise it will happen in a
blocking fashion on the current thread.

:param bool synchronous: If a capture is open, then ``True`` will use :meth:`BlockInvoke` to call
  the callback. Otherwise if ``False`` then :meth:`AsyncInvoke` will be used.
:param DirectoryBrowseCallback callback: The function to callback on the replay thread.
)");
  virtual void GetHomeFolder(bool synchronous, DirectoryBrowseCallback callback) = 0;

  DOCUMENT(R"(Query the remote host for the contents of a path.

If a capture is open, the callback will happen on the replay thread, otherwise it will happen in a
blocking fashion on the current thread.

:param str path: The path to query the contents of.
:param bool synchronous: If a capture is open, then ``True`` will use :meth:`BlockInvoke` to call
  the callback. Otherwise if ``False`` then :meth:`AsyncInvoke` will be used.
:param DirectoryBrowseCallback callback: The function to callback on the replay thread.
)");
  virtual void ListFolder(const rdcstr &path, bool synchronous, DirectoryBrowseCallback callback) = 0;

  DOCUMENT(R"(Copy a capture from the local machine to the remote host.

:param str localpath: The path on the local machine to copy from.
:param QWidget window: A handle to the window to use when showing a progress bar.
:return: The path on the local machine where the file was saved, or empty if something went wrong.
:rtype: str
)");
  virtual rdcstr CopyCaptureToRemote(const rdcstr &localpath, QWidget *window) = 0;

  DOCUMENT(R"(Copy a capture from the remote host to the local machine.

:param str remotepath: The path on the remote server to copy from.
:param str localpath: The path on the local machine to copy to.
:param QWidget window: A handle to the window to use when showing a progress bar.
)");
  virtual void CopyCaptureFromRemote(const rdcstr &remotepath, const rdcstr &localpath,
                                     QWidget *window) = 0;

  DOCUMENT(R"(Return the amount of time that the currently active command on the replay thread has
been executing for.

This can be used to identify if a command is long-running to display a progress bar or notification.

:return: The time in seconds that the current command has been executing for, or 0.0 if no command
  is executing.
:rtype: float
)");
  virtual float GetCurrentProcessingTime() = 0;

  DOCUMENT(R"(Make a tagged non-blocking invoke call onto the replay thread.

This tagged function is for cases when we might send a request - e.g. to pick a vertex or pixel -
and want to pre-empt it with a new request before the first has returned. Either because some
other work is taking a while or because we're sending requests faster than they can be
processed.

The manager processes only the request on the top of the queue, so when a new tagged invoke
comes in, we remove any other requests in the queue before it that have the same tag.

:param str tag: The tag to identify this callback.
:param InvokeCallback method: The function to callback on the replay thread.
)");
  virtual void AsyncInvoke(const rdcstr &tag, InvokeCallback method) = 0;

  DOCUMENT(R"(Make a non-blocking invoke call onto the replay thread.

:param InvokeCallback method: The function to callback on the replay thread.
)");
  virtual void AsyncInvoke(InvokeCallback method) = 0;

  // This is an ugly hack, but we leave BlockInvoke as the last method, so that when the class is
  // extended and the wrapper around BlockInvoke to release the python GIL happens, it picks up the
  // same docstring.
  DOCUMENT(R"(Make a blocking invoke call onto the replay thread.

:param InvokeCallback method: The function to callback on the replay thread.
)");
  virtual void BlockInvoke(InvokeCallback method) = 0;

protected:
  IReplayManager() = default;
  ~IReplayManager() = default;
};

DECLARE_REFLECTION_STRUCT(IReplayManager);

// should match ToolWindowManager::AreaReferenceType
DOCUMENT(R"(Specifies the relationship between the existing dock window and the new one when adding
a new dock window or moving an existing dock window.

.. data:: LastUsedArea

  The existing dock window is not used, the new dock window is placed wherever the last dock window
  was placed.

.. data:: NewFloatingArea

  The existing dock window is not used, the new dock window is placed in a new floating area.

.. data:: EmptySpace

  The existing dock window is not used, the new dock window is placed in empty area in the dockarea.

.. data:: NoArea

  The existing dock window is not used, the new window is hidden.

.. data:: AddTo

  The new dock window is placed in a tab set with the existing dock window.

.. data:: LeftOf

  The new dock window is placed to the left of the existing dock window, at a specified proportion.

.. data:: RightOf

  The new dock window is placed to the right of the existing dock window, at a specified proportion.

.. data:: TopOf

  The new dock window is placed above the existing dock window, at a specified proportion.

.. data:: BottomOf

  The new dock window is placed below the existing dock window, at a specified proportion.

.. data:: LeftWindowSide

  The new dock window is placed left of *all* docks in the window, at a specified proportion.

.. data:: RightWindowSide

  The new dock window is placed right of *all* docks in the window, at a specified proportion.

.. data:: TopWindowSide

  The new dock window is placed above *all* docks in the window, at a specified proportion.

.. data:: BottomWindowSide

  The new dock window is placed below *all* docks in the window, at a specified proportion.

.. data:: MainToolArea

  The new dock window is placed in the 'main' tool area as defined by finding an existing known
  window and using that as the main area. In the default layout this is where most windows are
  placed.

.. data:: LeftToolArea

  The new dock window is placed in the 'left' tool area as defined by finding an existing known
  window and using that as the main area, then adding to the left of that. In the default layout
  this is where the event browser is placed.

.. data:: TransientPopupArea

  The new dock window is docked with other similar transient views like constant buffer or pixel
  history windows, if they exist, or else docked to the right of the main window.
)");
enum class DockReference : int
{
  LastUsedArea,
  NewFloatingArea,
  EmptySpace,
  NoArea,
  AddTo,
  LeftOf,
  RightOf,
  TopOf,
  BottomOf,
  LeftWindowSide,
  RightWindowSide,
  TopWindowSide,
  BottomWindowSide,

  // extra values here
  MainToolArea,
  LeftToolArea,
  TransientPopupArea,
};

DOCUMENT(R"(Details any changes that have been made to a capture in the UI which can be saved to
disk but currently aren't. Note that detection is conservative - e.g. if a change is made, then
cancelled out by reversing the change, this will still count as 'modified' even if the end result is
the same data. In that sense it's analogous to adding and then deleting some characters in a text
editor, since there is no actual undo system.

This is a bitmask, so several values can be present at once.

.. data:: NoModifications

  Fixed value of 0 indicating no modifications have been made.

.. data:: Renames

  One or more resources have been given a custom name which hasn't been saved.

.. data:: Bookmarks

  Event bookmarks have been added or removed.

.. data:: Notes

  The general notes field has been changed.

.. data:: EditedShaders

  There are shader editing changes (new edits or reverts).

.. data:: All

  Fixed value with all bits set, indication all modifications have been made.
)");
enum class CaptureModifications : uint32_t
{
  NoModifications = 0x0000,
  Renames = 0x0001,
  Bookmarks = 0x0002,
  Notes = 0x0004,
  EditedShaders = 0x0008,
  All = 0xffffffff,
};

BITMASK_OPERATORS(CaptureModifications);

DOCUMENT("A description of a bookmark on an event");
struct EventBookmark
{
  DOCUMENT("The :data:`eventId <renderdoc.APIEvent.eventId>` at which this bookmark is placed.");
  uint32_t eventId = 0;

  DOCUMENT("The text associated with this bookmark - could be empty");
  rdcstr text;

  DOCUMENT("");
  EventBookmark() = default;
  EventBookmark(uint32_t e) : eventId(e) {}
  bool operator==(const EventBookmark &o) const { return eventId == o.eventId; }
  bool operator!=(const EventBookmark &o) const { return eventId != o.eventId; }
  bool operator<(const EventBookmark &o) const { return eventId < o.eventId; }
};

DECLARE_REFLECTION_STRUCT(EventBookmark);

DOCUMENT("Controlling interface for interop with RGP tool.");
struct IRGPInterop
{
  DOCUMENT(R"(Return true if the given :data:`eventId <renderdoc.APIEvent.eventId>` has and
equivalent in RGP.

:param int eventId: The :data:`eventId <renderdoc.APIEvent.eventId>` to query for.
:return: ``True`` if there is an equivalent. This only confirms the equivalent exists, not that it
  will be selectable in all cases.
:rtype: bool
)");
  virtual bool HasRGPEvent(uint32_t eventId) = 0;

  DOCUMENT(R"(Select the given :data:`eventId <renderdoc.APIEvent.eventId>` equivalent in RGP.

:param int eventId: The :data:`eventId <renderdoc.APIEvent.eventId>` to query for.
:return: ``True`` if the selection request succeeded. This only confirms the request was sent, not
  that the event was selected in RGP.
:rtype: bool
)");
  virtual bool SelectRGPEvent(uint32_t eventId) = 0;

  DOCUMENT("");
  virtual ~IRGPInterop() = default;

protected:
  IRGPInterop() = default;
};

DECLARE_REFLECTION_STRUCT(IRGPInterop);

DOCUMENT("The capture context that the python script is running in.")
struct ICaptureContext
{
  DOCUMENT(R"(Retrieve the absolute path where a given temporary capture should be stored.
data.

:param str appname: The name of the application to use as part of the template.
:return: The absolute path.
:rtype: str
)");
  virtual rdcstr TempCaptureFilename(const rdcstr &appname) = 0;

  DOCUMENT(R"(Open a capture file for replay.

:param str captureFile: The actual path to the capture file.
:param renderdoc.ReplayOptions opts: The options controlling how the capture should be replayed.
:param str origFilename: The original filename, if the capture was copied remotely for replay.
:param bool temporary: ``True`` if this is a temporary capture which should prompt the user for
  either save or delete on close.
:param bool local: ``True`` if ``captureFile`` refers to a file on the local machine.
)");
  virtual void LoadCapture(const rdcstr &captureFile, const ReplayOptions &opts,
                           const rdcstr &origFilename, bool temporary, bool local) = 0;

  DOCUMENT(R"(Saves the current capture file to a given path.

If the capture was temporary, this save action means it is no longer temporary and will be treated
like any other capture.

Any modifications to the capture (see :meth:`GetCaptureModifications`) will be applied at the same
time.

:param str captureFile: The path to save the capture file to.
:return: ``True`` if the save operation was successful.
:rtype: bool
)");
  virtual bool SaveCaptureTo(const rdcstr &captureFile) = 0;

  DOCUMENT("Recompress the current capture as much as possible.");
  virtual void RecompressCapture() = 0;

  DOCUMENT("Close the currently open capture file.");
  virtual void CloseCapture() = 0;

  DOCUMENT(R"(Imports a capture file from a non-native format, via conversion to temporary rdc.

This converts the file to a specified temporary .rdc and loads it, closing any existing capture.

The capture must be available locally, if it's not this function will fail.

:param renderdoc.CaptureFileFormat fmt: The capture file format to import from.
:param str importfile: The path to import from.
:param str rdcfile: The temporary path to save the rdc file to.
:return: ``True`` if the import operation was successful and the capture was loaded.
:rtype: bool
)");
  virtual bool ImportCapture(const CaptureFileFormat &fmt, const rdcstr &importfile,
                             const rdcstr &rdcfile) = 0;

  DOCUMENT(R"(Exports the current capture file to a given path with a specified capture file format.

The capture must be available locally, if it's not this function will fail.

:param renderdoc.CaptureFileFormat fmt: The capture file format to export to.
:param str exportfile: The path to export the capture file to.
)");
  virtual void ExportCapture(const CaptureFileFormat &fmt, const rdcstr &exportfile) = 0;

  DOCUMENT(R"(Move the current replay to a new event in the capture.

:param List[CaptureViewer] exclude: A list of viewers to exclude from being notified of this change,
  to stop infinite recursion.
:param int selectedEventId: The selected :data:`eventId <renderdoc.APIEvent.eventId>`. See
  :meth:`CaptureViewer.OnSelectedEventChanged` for more information.
:param int eventId: The new current :data:`eventId <renderdoc.APIEvent.eventId>`. See
  :meth:`CaptureViewer.OnEventChanged` for more information.
:param bool force: Optional parameter, if ``True`` then the replay will 'move' even if it is moving
  to the same :data:`eventId <renderdoc.APIEvent.eventId>` as it's currently on.
)");
  virtual void SetEventID(const rdcarray<ICaptureViewer *> &exclude, uint32_t selectedEventId,
                          uint32_t eventId, bool force = false) = 0;
  DOCUMENT(R"(Replay the capture to the current event again, to pick up any changes that might have
been made.
)");
  virtual void RefreshStatus() = 0;

  DOCUMENT(R"(Determine if a resource has been replaced. See :meth:`RegisterReplacement`.

:param renderdoc.ResourceId id: The id of the resource to check.
:return: ``True`` if the resource has been replaced.
:rtype: bool
)");
  virtual bool IsResourceReplaced(ResourceId id) = 0;

  DOCUMENT(R"(Return the id of a replacement for the given resource. See
:meth:`RegisterReplacement` and :meth:`IsResourceReplaced`.

:param renderdoc.ResourceId id: The id of the resource to check.
:return: The replacement id, or a null id if the resource hasn't been replaced
:rtype: renderdoc.ResourceId
)");
  virtual ResourceId GetResourceReplacement(ResourceId id) = 0;

  DOCUMENT(R"(Register that a resource has replaced, so that the UI can be updated to reflect the
change.

This should be called at the same time as :meth:`ReplayController.ReplaceResource`.

:param renderdoc.ResourceId from: The id of the resource being replaced.
:param renderdoc.ResourceId to: The id of the resource replacing it.
)");
  virtual void RegisterReplacement(ResourceId from, ResourceId to) = 0;

  DOCUMENT(R"(Register that a replacement has been removed, so that the UI can be updated to reflect
the change.

This should be called at the same time as :meth:`ReplayController.RemoveReplacement`.

See :meth:`ReplaceResource`.

:param renderdoc.ResourceId id: The id of the original resource that was previously replaced.
)");
  virtual void UnregisterReplacement(ResourceId id) = 0;

  DOCUMENT(R"(Register a new instance of :class:`CaptureViewer` to receive capture event notifications.

:param CaptureViewer viewer: The viewer to register.
)");
  virtual void AddCaptureViewer(ICaptureViewer *viewer) = 0;

  DOCUMENT(R"(Unregister an instance of :class:`CaptureViewer` from receiving notifications.

:param CaptureViewer viewer: The viewer to unregister.
)");
  virtual void RemoveCaptureViewer(ICaptureViewer *viewer) = 0;

  //////////////////////////////////////////////////////////////////////////////
  // Accessors

  DOCUMENT(R"(Retrieve the replay manager for access to the internal RenderDoc replay controller.

:return: The current replay manager.
:rtype: ReplayManager
)");
  virtual IReplayManager &Replay() = 0;

  DOCUMENT(R"(Connect to a remote server.

:param RemoteHost host: The host to connect to.
)");
  virtual void ConnectToRemoteServer(RemoteHost host) = 0;

  DOCUMENT(R"(Check whether or not a capture is currently loaded.

:return: ``True`` if a capture is loaded.
:rtype: bool
)");
  virtual bool IsCaptureLoaded() = 0;

  DOCUMENT(R"(Check whether or not the current capture is stored locally, or on a remote host.

:return: ``True`` if a capture is local.
:rtype: bool
)");
  virtual bool IsCaptureLocal() = 0;

  DOCUMENT(R"(Check whether or not the current capture is considered temporary. Captures that were
made by an application and then have not been explicitly saved anywhere are temporary and will be
cleaned up on close (with a final prompt to save). Once they are save to disk, they are no longer
temporary and treated like any other capture.

:return: ``True`` if a capture is temporary.
:rtype: bool
)");
  virtual bool IsCaptureTemporary() = 0;

  DOCUMENT(R"(Check whether or not a capture is currently loading in-progress.

:return: ``True`` if a capture is currently loading.
:rtype: bool
)");
  virtual bool IsCaptureLoading() = 0;

  DOCUMENT(R"(If a capture is loaded, return the current fatal error status.

:return: If a capture is currently loaded, return the fatal error status.
:rtype: renderdoc.ResultDetails
)");
  virtual ResultDetails GetFatalError() = 0;

  DOCUMENT(R"(Retrieve the filename for the currently loaded capture.

:return: The filename of the current capture.
:rtype: str
)");
  virtual rdcstr GetCaptureFilename() = 0;

  DOCUMENT(R"(Get a bitmask indicating which modifications (if any) have been made to the capture in
the UI which aren't reflected in the capture file on disk.

:return: The modifications (if any) that have been made to the capture.
:rtype: CaptureModifications
)");
  virtual CaptureModifications GetCaptureModifications() = 0;

  DOCUMENT(R"(Retrieve the :class:`~renderdoc.FrameDescription` for the currently loaded capture.

:return: The frame information.
:rtype: renderdoc.FrameDescription
)");
  virtual const FrameDescription &FrameInfo() = 0;

  DOCUMENT(R"(Retrieve the :class:`~renderdoc.APIProperties` for the currently loaded capture.

:return: The API properties.
:rtype: renderdoc.APIProperties
)");
  virtual const APIProperties &APIProps() = 0;

  DOCUMENT(R"(Retrieve the list of :class:`~renderdoc.ShaderEncoding` that are available for
building target shaders for the currently loaded capture. See
:meth:`~renderdoc.ReplayController.BuildTargetShader`.

:return: The available encodings.
:rtype: List[renderdoc.ShaderEncoding]
)");
  virtual rdcarray<ShaderEncoding> TargetShaderEncodings() = 0;

  DOCUMENT(R"(Retrieve the list of :class:`~renderdoc.ShaderEncoding` that are available for
building custom shaders for the currently loaded capture. See
:meth:`~renderdoc.ReplayController.BuildCustomShader`.

:return: The available encodings.
:rtype: List[renderdoc.ShaderEncoding]
)");
  virtual rdcarray<ShaderEncoding> CustomShaderEncodings() = 0;

  DOCUMENT(R"(Retrieve the list of prefixes for each :class:`~renderdoc.ShaderEncoding` that should
be added to custom compiled shaders. See
:meth:`~renderdoc.ReplayController.GetCustomShaderSourcePrefixes`.

:return: A list of pairs, listing a prefix for each shader encoding referenced.
:rtype: List[renderdoc.ShaderSourcePrefix]
)");
  virtual rdcarray<ShaderSourcePrefix> CustomShaderSourcePrefixes() = 0;

  DOCUMENT(R"(Retrieve the currently selected :data:`eventId <renderdoc.APIEvent.eventId>`.

In most cases, prefer using :meth:`CurEvent`. See :meth:`CaptureViewer.OnSelectedEventChanged` for more
information for how this differs.

:return: The current selected event.
:rtype: int
)");
  virtual uint32_t CurSelectedEvent() = 0;

  DOCUMENT(R"(Retrieve the current :data:`eventId <renderdoc.APIEvent.eventId>`.

:return: The current event.
:rtype: int
)");
  virtual uint32_t CurEvent() = 0;

  DOCUMENT(R"(Retrieve the currently selected action.

In most cases, prefer using :meth:`CurAction`. See :meth:`CaptureViewer.OnSelectedEventChanged` for
more information for how this differs.

:return: The currently selected action.
:rtype: renderdoc.ActionDescription
)");
  virtual const ActionDescription *CurSelectedAction() = 0;

  DOCUMENT(R"(Retrieve the current action.

:return: The current action, or ``None`` if no action is selected.
:rtype: renderdoc.ActionDescription
)");
  virtual const ActionDescription *CurAction() = 0;

  DOCUMENT(R"(Retrieve the first action in the capture.

:return: The first action.
:rtype: renderdoc.ActionDescription
)");
  virtual const ActionDescription *GetFirstAction() = 0;

  DOCUMENT(R"(Retrieve the last action in the capture.

:return: The last action.
:rtype: renderdoc.ActionDescription
)");
  virtual const ActionDescription *GetLastAction() = 0;

  DOCUMENT(R"(Retrieve the root list of actions in the current capture.

:return: The root actions.
:rtype: List[renderdoc.ActionDescription]
)");
  virtual const rdcarray<ActionDescription> &CurRootActions() = 0;

  DOCUMENT(R"(Retrieve the information about a particular resource.

:param renderdoc.ResourceId id: The ID of the resource to query about.
:return: The information about a resource, or ``None`` if the ID does not correspond to a resource.
:rtype: renderdoc.ResourceDescription
)");
  virtual const ResourceDescription *GetResource(ResourceId id) const = 0;

  DOCUMENT(R"(Retrieve the list of resources in the current capture.

:return: The list of resources.
:rtype: List[renderdoc.ResourceDescription]
)");
  virtual const rdcarray<ResourceDescription> &GetResources() = 0;

  DOCUMENT(R"(Retrieve the human-readable name for the resource to display.

This will first check to see if a custom name has been set for the resource, and if so use that. See
:meth:`SetResourceCustomName`. If no custom name has been set, it will use the resource name found
in the capture, either a name set via API-specific debug methods, or an auto-generated name based on
the resource type.

:param renderdoc.ResourceId id: The ID of the resource to query.
:return: The current name of the resource.
:rtype: str
)");
  virtual rdcstr GetResourceName(ResourceId id) const = 0;

  DOCUMENT(R"(Returns the same name as :meth:`GetResourceName` but without any added suffix, e.g. to
indicate the resource's status such as (Edited).

:param renderdoc.ResourceId id: The ID of the resource to query.
:return: The unsuffixed resource name.
:rtype: str
)");
  virtual rdcstr GetResourceNameUnsuffixed(ResourceId id) const = 0;

  DOCUMENT(R"(Determines whether the name for the given resource has been customised at all, either
during capture time or with :meth:`SetResourceCustomName`.

If not, the name is just auto-generated based on the ID and resource type, so depending on
circumstance it may be preferable to omit the name.

:param renderdoc.ResourceId id: The ID of the resource to query.
:return: Whether the name for the resource has just been auto-generated.
:rtype: bool
)");
  virtual bool IsAutogeneratedName(ResourceId id) = 0;

  DOCUMENT(R"(Checks whether a runtime custom name has been set with :meth:`SetResourceCustomName`.

In general, :meth:`IsAutogeneratedName` should be preferred to check if the resource name is default
generated just from the ID, or if it has been set to some human readable name. This function will
only check if a name has been set in the UI itself, a resource could still have a custom name that
was set programmatically during capture time.

:param renderdoc.ResourceId id: The ID of the resource to query.
:return: Whether the name for the resource has been customised with :meth:`SetResourceCustomName`.
:rtype: bool
)");
  virtual bool HasResourceCustomName(ResourceId id) = 0;

  DOCUMENT(R"(Set a custom name for a resource.

This allows an override to the name returned by :meth:`GetResourceName`, most useful when there are
no pre-existing debug names specified in the capture.

To remove a custom name that has been set previously, specify the empty string as the name. Then the
custom name will be removed, and instead :meth:`GetResourceName` will fall back to returning any
name fetched from the capture.

:param renderdoc.ResourceId id: The ID of the resource to name.
:param str name: The name to provide, or an empty string to remove any previous custom name.
)");
  virtual void SetResourceCustomName(ResourceId id, const rdcstr &name) = 0;

  DOCUMENT(R"(Returns an index that can be used to cache the results of resource naming.

In some cases (e.g. formatting in widgets) there might be high frequency fetches to names without an
easy way to force a refresh on a rename. Instead, the index here can be cached and compared each
time to see if any names have changed.

The index starts at 1, so initialising an internal cache to 0 will cause the first check to be
considered out of date

:return: An incrementing index that can be used as a quick check if any names have changed.
:rtype: int
)");
  virtual int32_t ResourceNameCacheID() const = 0;

  DOCUMENT(R"(Retrieve the information about a particular texture.

:param renderdoc.ResourceId id: The ID of the texture to query about.
:return: The information about a texture, or ``None`` if the ID does not correspond to a texture.
:rtype: renderdoc.TextureDescription
)");
  virtual TextureDescription *GetTexture(ResourceId id) = 0;

  DOCUMENT(R"(Retrieve the list of textures in the current capture.

:return: The list of textures.
:rtype: List[renderdoc.TextureDescription]
)");
  virtual const rdcarray<TextureDescription> &GetTextures() = 0;

  DOCUMENT(R"(Retrieve the information about a particular buffer.

:param renderdoc.ResourceId id: The ID of the buffer to query about.
:return: The information about a buffer, or ``None`` if the ID does not correspond to a buffer.
:rtype: renderdoc.BufferDescription
)");
  virtual BufferDescription *GetBuffer(ResourceId id) = 0;

  DOCUMENT(R"(Retrieve the list of buffers in the current capture.

:return: The list of buffers.
:rtype: List[renderdoc.BufferDescription]
)");
  virtual const rdcarray<BufferDescription> &GetBuffers() const = 0;

  DOCUMENT(R"(Retrieve the information about an action at a given
:data:`eventId <renderdoc.APIEvent.eventId>`.

:param int eventId: The :data:`eventId <renderdoc.APIEvent.eventId>` to query for.
:return: The information about the action, or ``None`` if the
  :data:`eventId <renderdoc.APIEvent.eventId>` doesn't correspond to an action.
:rtype: renderdoc.ActionDescription
)");
  virtual const ActionDescription *GetAction(uint32_t eventId) = 0;

  DOCUMENT(R"(Sets the path to the RGP profile to use with :meth:`GetRGPInterop`, launches RGP and
opens an interop connection. This function will block (with a progress dialog) until either an
error is encountered or else the connection is successfully established.

This could be newly created, or extracted from an embedded section in the RDC.

The connection is automatically closed when the capture is closed. If OpenRGPProfile is called
again, any previous connection will be closed.

:param str filename: The filename of the extracted temporary RGP capture on disk.
:return: Whether RGP launched successfully.
:rtype: bool
)");
  virtual bool OpenRGPProfile(const rdcstr &filename) = 0;

  DOCUMENT(R"(Returns the current interop handle for RGP.

This may return ``None`` in several cases:

- if there is no capture loaded
- if no RGP profile has been associated with the current capture yet (See :meth:`OpenRGPProfile`)
- if RGP failed to launch or connect.

The handle returned is invalidated when the capture is closed, or if :meth:`OpenRGPProfile` is
called.

:return: The RGP interop connection handle.
:rtype: RGPInterop
)");
  virtual IRGPInterop *GetRGPInterop() = 0;

  DOCUMENT(R"(Retrieve the :class:`~renderdoc.SDFile` for the currently open capture.

:return: The structured file.
:rtype: renderdoc.SDFile
)");
  virtual const SDFile &GetStructuredFile() = 0;

  DOCUMENT(R"(Retrieve the current windowing system in use.

:return: The active windowing system.
:rtype: renderdoc.WindowingSystem
)");
  virtual WindowingSystem CurWindowingSystem() = 0;

  DOCUMENT(R"(Create an opaque pointer suitable for passing to
:meth:`~renderdoc.ReplayController.CreateOutput` or other functions that expect windowing data.

.. note::
  This function must be called on the main UI thread.

:param QWidget window: The window to create windowing data for.
:return: The windowing data.
:rtype: renderdoc.WindowingData
)");
  virtual WindowingData CreateWindowingData(QWidget *window) = 0;

  DOCUMENT(R"(Retrieve the current list of debug messages. This includes messages from the capture
as well as messages generated during replay and analysis.

:return: The debug messages generated to date.
:rtype: List[renderdoc.DebugMessage]
)");
  virtual const rdcarray<DebugMessage> &DebugMessages() = 0;

  DOCUMENT(R"(Retrieve how many messages in :meth:`DebugMessages` are currently unread.

:return: The number of unread messages.
:rtype: int
)");
  virtual int32_t UnreadMessageCount() = 0;

  DOCUMENT("Mark all messages as read, resets :meth:`UnreadMessageCount` to 0.");
  virtual void MarkMessagesRead() = 0;

  DOCUMENT(R"(Add messages into the list returned by :meth:`DebugMessages`. Initially set to unread.

:param List[renderdoc.DebugMessage] msgs: A list of debug messages to add.
)");
  virtual void AddMessages(const rdcarray<DebugMessage> &msgs) = 0;

  DOCUMENT(R"(Retrieve the contents for a given notes field.

Examples of fields are:

* 'comments' for generic comments to be displayed in a text field
* 'hwinfo' for a plaintext summary of the hardware and driver configuration of the system.

:param str key: The name of the notes field to retrieve.
:return: The contents, or an empty string if the field doesn't exist.
:rtype: str
)");
  virtual rdcstr GetNotes(const rdcstr &key) = 0;

  DOCUMENT(R"(Set the contents for a given notes field.

See :meth:`GetNotes` for a list of possible common field keys.

:param str key: The name of the notes field to set.
:param str contents: The new contents to assign to that field.
)");
  virtual void SetNotes(const rdcstr &key, const rdcstr &contents) = 0;

  DOCUMENT(R"(Get the current list of bookmarks in the capture. Each bookmark is associated with an
eventId and has some text attached. There will only be at most one bookmark for any given eventId.

The list of bookmarks is not necessarily sorted by eventId. Thus, bookmark 1 is always bookmark 1
until it is removed, the indices do not shift as new bookmarks are added or removed.

:return: The currently set bookmarks.
:rtype: List[EventBookmark]
)");
  virtual rdcarray<EventBookmark> GetBookmarks() = 0;

  DOCUMENT(R"(Set or update a bookmark.

A bookmark will be added at the specified eventId, or if one already exists then the attached text
will be replaced.

:param EventBookmark mark: The bookmark to add.
)");
  virtual void SetBookmark(const EventBookmark &mark) = 0;

  DOCUMENT(R"(Remove a bookmark at a given eventId.

If no bookmark exists, this function will do nothing.

:param int eventId: The eventId of the bookmark to remove.
)");
  virtual void RemoveBookmark(uint32_t eventId) = 0;

  DOCUMENT(R"(Retrieve the current singleton :class:`MainWindow`.

:return: The current window.
:rtype: MainWindow
)");
  virtual IMainWindow *GetMainWindow() = 0;

  DOCUMENT(R"(Retrieve the current singleton :class:`EventBrowser`.

:return: The current window, which is created (but not shown) it there wasn't one open.
:rtype: EventBrowser
)");
  virtual IEventBrowser *GetEventBrowser() = 0;

  DOCUMENT(R"(Retrieve the current singleton :class:`APIInspector`.

:return: The current window, which is created (but not shown) it there wasn't one open.
:rtype: APIInspector
)");
  virtual IAPIInspector *GetAPIInspector() = 0;

  DOCUMENT(R"(Retrieve the current singleton :class:`TextureViewer`.

:return: The current window, which is created (but not shown) it there wasn't one open.
:rtype: TextureViewer
)");
  virtual ITextureViewer *GetTextureViewer() = 0;

  DOCUMENT(R"(Retrieve the current singleton :class:`BufferViewer` configured for mesh viewing.

:return: The current window, which is created (but not shown) it there wasn't one open.
:rtype: BufferViewer
)");
  virtual IBufferViewer *GetMeshPreview() = 0;

  DOCUMENT(R"(Retrieve the current singleton :class:`PipelineStateViewer`.

:return: The current window, which is created (but not shown) it there wasn't one open.
:rtype: PipelineStateViewer
)");
  virtual IPipelineStateViewer *GetPipelineViewer() = 0;

  DOCUMENT(R"(Retrieve the current singleton :class:`CaptureDialog`.

:return: The current window, which is created (but not shown) it there wasn't one open.
:rtype: CaptureDialog
)");
  virtual ICaptureDialog *GetCaptureDialog() = 0;

  DOCUMENT(R"(Retrieve the current singleton :class:`DebugMessageView`.

:return: The current window, which is created (but not shown) it there wasn't one open.
:rtype: DebugMessageView
)");
  virtual IDebugMessageView *GetDebugMessageView() = 0;

  DOCUMENT(R"(Retrieve the current singleton :class:`LogView`.

:return: The current window, which is created (but not shown) it there wasn't one open.
:rtype: DiagnosticLogView
)");
  virtual IDiagnosticLogView *GetDiagnosticLogView() = 0;

  DOCUMENT(R"(Retrieve the current singleton :class:`CommentView`.

:return: The current window, which is created (but not shown) it there wasn't one open.
:rtype: CommentView
)");
  virtual ICommentView *GetCommentView() = 0;

  DOCUMENT(R"(Retrieve the current singleton :class:`PerformanceCounterViewer`.

:return: The current window, which is created (but not shown) it there wasn't one open.
:rtype: PerformanceCounterViewer
)");
  virtual IPerformanceCounterViewer *GetPerformanceCounterViewer() = 0;

  DOCUMENT(R"(Retrieve the current singleton :class:`StatisticsViewer`.

:return: The current window, which is created (but not shown) it there wasn't one open.
:rtype: StatisticsViewer
)");
  virtual IStatisticsViewer *GetStatisticsViewer() = 0;

  DOCUMENT(R"(Retrieve the current singleton :class:`TimelineBar`.

:return: The current window, which is created (but not shown) it there wasn't one open.
:rtype: TimelineBar
)");
  virtual ITimelineBar *GetTimelineBar() = 0;

  DOCUMENT(R"(Retrieve the current singleton :class:`PythonShell`.

:return: The current window, which is created (but not shown) it there wasn't one open.
:rtype: PythonShell
)");
  virtual IPythonShell *GetPythonShell() = 0;

  DOCUMENT(R"(Retrieve the current singleton :class:`ResourceInspector`.

:return: The current window, which is created (but not shown) it there wasn't one open.
:rtype: ResourceInspector
)");
  virtual IResourceInspector *GetResourceInspector() = 0;

  DOCUMENT(R"(Check if there is a current :class:`EventBrowser` open.

:return: ``True`` if there is a window open.
:rtype: bool
)");
  virtual bool HasEventBrowser() = 0;

  DOCUMENT(R"(Check if there is a current :class:`APIInspector` open.

:return: ``True`` if there is a window open.
:rtype: bool
)");
  virtual bool HasAPIInspector() = 0;

  DOCUMENT(R"(Check if there is a current :class:`TextureViewer` open.

:return: ``True`` if there is a window open.
:rtype: bool
)");
  virtual bool HasTextureViewer() = 0;

  DOCUMENT(R"(Check if there is a current :class:`PipelineStateViewer` open.

:return: ``True`` if there is a window open.
:rtype: bool
)");
  virtual bool HasPipelineViewer() = 0;

  DOCUMENT(R"(Check if there is a current mesh previewing :class:`BufferViewer` open.

:return: ``True`` if there is a window open.
:rtype: bool
)");
  virtual bool HasMeshPreview() = 0;

  DOCUMENT(R"(Check if there is a current :class:`CaptureDialog` open.

:return: ``True`` if there is a window open.
:rtype: bool
)");
  virtual bool HasCaptureDialog() = 0;

  DOCUMENT(R"(Check if there is a current :class:`DebugMessageView` open.

:return: ``True`` if there is a window open.
:rtype: bool
)");
  virtual bool HasDebugMessageView() = 0;

  DOCUMENT(R"(Check if there is a current :class:`DiagnosticLogView` open.

:return: ``True`` if there is a window open.
:rtype: bool
)");
  virtual bool HasDiagnosticLogView() = 0;

  DOCUMENT(R"(Check if there is a current :class:`CommentView` open.

:return: ``True`` if there is a window open.
:rtype: bool
)");
  virtual bool HasCommentView() = 0;

  DOCUMENT(R"(Check if there is a current :class:`PerformanceCounterViewer` open.

:return: ``True`` if there is a window open.
:rtype: bool
)");
  virtual bool HasPerformanceCounterViewer() = 0;

  DOCUMENT(R"(Check if there is a current :class:`StatisticsViewer` open.

:return: ``True`` if there is a window open.
:rtype: bool
)");
  virtual bool HasStatisticsViewer() = 0;

  DOCUMENT(R"(Check if there is a current :class:`TimelineBar` open.

:return: ``True`` if there is a window open.
:rtype: bool
)");
  virtual bool HasTimelineBar() = 0;

  DOCUMENT(R"(Check if there is a current :class:`PythonShell` open.

:return: ``True`` if there is a window open.
:rtype: bool
)");
  virtual bool HasPythonShell() = 0;

  DOCUMENT(R"(Check if there is a current :class:`ResourceInspector` open.

:return: ``True`` if there is a window open.
:rtype: bool
)");
  virtual bool HasResourceInspector() = 0;

  DOCUMENT("Raise the current :class:`EventBrowser`, showing it in the default place if needed.");
  virtual void ShowEventBrowser() = 0;
  DOCUMENT("Raise the current :class:`APIInspector`, showing it in the default place if needed.");
  virtual void ShowAPIInspector() = 0;
  DOCUMENT("Raise the current :class:`TextureViewer`, showing it in the default place if needed.");
  virtual void ShowTextureViewer() = 0;
  DOCUMENT(R"(Raise the current mesh previewing :class:`BufferViewer`, showing it in the default
place if needed.
)");
  virtual void ShowMeshPreview() = 0;
  DOCUMENT(
      "Raise the current :class:`PipelineStateViewer`, showing it in the default place if needed.");
  virtual void ShowPipelineViewer() = 0;
  DOCUMENT("Raise the current :class:`CaptureDialog`, showing it in the default place if needed.");
  virtual void ShowCaptureDialog() = 0;
  DOCUMENT(
      "Raise the current :class:`DebugMessageView`, showing it in the default place if needed.");
  virtual void ShowDebugMessageView() = 0;
  DOCUMENT(
      "Raise the current :class:`DiagnosticLogView`, showing it in the default place if needed.");
  virtual void ShowDiagnosticLogView() = 0;
  DOCUMENT("Raise the current :class:`CommentView`, showing it in the default place if needed.");
  virtual void ShowCommentView() = 0;
  DOCUMENT(
      "Raise the current :class:`PerformanceCounterViewer`, showing it in the default place if "
      "needed.");
  virtual void ShowPerformanceCounterViewer() = 0;
  DOCUMENT(
      "Raise the current :class:`StatisticsViewer`, showing it in the default place if needed.");
  virtual void ShowStatisticsViewer() = 0;
  DOCUMENT("Raise the current :class:`TimelineBar`, showing it in the default place if needed.");
  virtual void ShowTimelineBar() = 0;
  DOCUMENT("Raise the current :class:`PythonShell`, showing it in the default place if needed.");
  virtual void ShowPythonShell() = 0;
  DOCUMENT(
      "Raise the current :class:`ResourceInspector`, showing it in the default place if needed.");
  virtual void ShowResourceInspector() = 0;

  DOCUMENT(R"(Show a new :class:`ShaderViewer` window, showing an editable view of a given shader.

:param renderdoc.ResourceId id: The shader object, if applicable, that's being edited. If this edit
  corresponds to no shader object (such as if it's a custom shader) this can be a null ID.
:param renderdoc.ShaderStage stage: The shader stage for this shader.
:param str entryPoint: The entry point to be used when compiling the edited shader.
:param List[Tuple[str,str]] files: The source files, with each tuple being a pair of the filename
  and the file contents.
:param renderdoc.KnownShaderTool knownTool: The preferred tool to use to compile, if known.
:param renderdoc.ShaderEncoding shaderEncoding: The encoding of the input files.
:param renderdoc.ShaderCompileFlags flags: The flags originally used to compile the shader.
:param ShaderViewer.SaveCallback saveCallback: The callback function to call when a save/update is
  triggered.
:param ShaderViewer.RevertCallback revertCallback: The callback function to call when the shader
  is to be reverted - either by user request or because the shader viewer was closed.
:return: The new :class:`ShaderViewer` window opened but not shown for editing.
:rtype: ShaderViewer
)");
  virtual IShaderViewer *EditShader(ResourceId id, ShaderStage stage, const rdcstr &entryPoint,
                                    const rdcstrpairs &files, KnownShaderTool knownTool,
                                    ShaderEncoding shaderEncoding, ShaderCompileFlags flags,
                                    IShaderViewer::SaveCallback saveCallback,
                                    IShaderViewer::RevertCallback revertCallback) = 0;

  DOCUMENT(R"(Show a new :class:`ShaderViewer` window, showing a read-only view of a debug trace
through the execution of a given shader.

:param renderdoc.ShaderBindpointMapping bind: The bindpoint mapping for the shader to view.
:param renderdoc.ShaderReflection shader: The reflection data for the shader to view.
:param renderdoc.ResourceId pipeline: The pipeline state object, if applicable, that this shader is
  bound to.
:param renderdoc.ShaderDebugTrace trace: The execution trace of the debugged shader.
:param str debugContext: A human-readable context string describing which invocation of this shader
  was debugged. For example 'Pixel 12,34 at eventId 678'.
:return: The new :class:`ShaderViewer` window opened, but not shown.
:rtype: ShaderViewer
)");
  virtual IShaderViewer *DebugShader(const ShaderBindpointMapping *bind,
                                     const ShaderReflection *shader, ResourceId pipeline,
                                     ShaderDebugTrace *trace, const rdcstr &debugContext) = 0;

  DOCUMENT(R"(Show a new :class:`ShaderViewer` window, showing a read-only view of a given shader.

:param renderdoc.ShaderReflection shader: The reflection data for the shader to view.
:param renderdoc.ResourceId pipeline: The pipeline state object, if applicable, that this shader is
  bound to.
:return: The new :class:`ShaderViewer` window opened, but not shown.
:rtype: ShaderViewer
)");
  virtual IShaderViewer *ViewShader(const ShaderReflection *shader, ResourceId pipeline) = 0;

  DOCUMENT(R"(Show a new :class:`ShaderMessageViewer` window, showing the current event's messages.

:param renderdoc.ShaderStageMask stages: The initial stages being viewed.
:return: The new :class:`ShaderMessageViewer` window opened, but not shown.
:rtype: ShaderMessageViewer
)");
  virtual IShaderMessageViewer *ViewShaderMessages(ShaderStageMask stages) = 0;

  DOCUMENT(R"(Show a new :class:`BufferViewer` window, showing a read-only view of buffer data.

:param int byteOffset: The offset in bytes to the start of the buffer data to show.
:param int byteSize: The number of bytes in the buffer to show.
:param renderdoc.ResourceId id: The ID of the buffer to fetch data from.
:param str format: Optionally a HLSL/GLSL style formatting string.
:return: The new :class:`BufferViewer` window opened, but not shown.
:rtype: BufferViewer
)");
  virtual IBufferViewer *ViewBuffer(uint64_t byteOffset, uint64_t byteSize, ResourceId id,
                                    const rdcstr &format = "") = 0;

  DOCUMENT(R"(Show a new :class:`BufferViewer` window, showing a read-only view of a texture's raw
bytes.

:param renderdoc.ResourceId id: The ID of the texture itself.
:param renderdoc.Subresource sub: The subresource within this texture to use.
:param str format: Optionally a HLSL/GLSL style formatting string.
:return: The new :class:`BufferViewer` window opened, but not shown.
:rtype: BufferViewer
)");
  virtual IBufferViewer *ViewTextureAsBuffer(ResourceId id, const Subresource &sub,
                                             const rdcstr &format = "") = 0;

  DOCUMENT(R"(Show a new :class:`BufferViewer` window, showing a read-only view of a the
variables in a constant buffer with their values.

:param renderdoc.ShaderStage stage: The stage that the constant buffer is bound to.
:param int slot: The index in the shader's constant buffer list to look up.
:param int idx: For APIs that support arrayed resource binds, the index in the constant buffer
  array.
:return: The new :class:`BufferViewer` window opened, but not shown.
:rtype: BufferViewer
)");
  virtual IBufferViewer *ViewConstantBuffer(ShaderStage stage, uint32_t slot, uint32_t idx) = 0;

  DOCUMENT(R"(Show a new :class:`PixelHistoryView` window, showing the results from a pixel history
operation.

:param renderdoc.ResourceId id: The ID of the texture to show the history of.
:param int x: The x co-ordinate of the pixel to search for.
:param int y: The y co-ordinate of the pixel to search for.
:param int view: The layered or multiview rendering view index of the pixel to search for.
:param renderdoc.TextureDisplay display: The texture display configuration to use when looking up
  the history.
:return: The new :class:`PixelHistoryView` window opened, but not shown.
:rtype: PixelHistoryView
)");
  virtual IPixelHistoryView *ViewPixelHistory(ResourceId id, uint32_t x, uint32_t y, uint32_t view,
                                              const TextureDisplay &display) = 0;

  DOCUMENT(R"(Creates and returns a built-in window.

This function is intended for internal use for restoring layouts, and generally should not be used
by user code.

:param str objectName: The built-in name of a singleton window.
:return: The handle to the existing or newly created window of this type.
:rtype: QWidget
)");
  virtual QWidget *CreateBuiltinWindow(const rdcstr &objectName) = 0;

  DOCUMENT(R"(Marks a built-in window as closed.

This function is intended for internal use by the built-in windows for singleton management, and
should not be called by user code.

:param QWidget window: The built-in window that closed.
)");
  virtual void BuiltinWindowClosed(QWidget *window) = 0;

  DOCUMENT(R"(Raises a window within its docking manager so it becomes the focus of wherever it is
currently docked.

:param QWidget dockWindow: The window to raise.
)");
  virtual void RaiseDockWindow(QWidget *dockWindow) = 0;

  DOCUMENT(R"(Adds a new window within the docking system.

:param QWidget newWindow: The new window to add.
:param DockReference ref: The location to add the new window, possibly relative to ``refWindow``.
:param QWidget refWindow: The window to refer to if the new window is being added relative, or can
  be ``None`` if the new location is absolute.
:param float percentage: Optionally the percentage to split the area. If omitted, a 50% split is
  used.
)");
  virtual void AddDockWindow(QWidget *newWindow, DockReference ref, QWidget *refWindow,
                             float percentage = 0.5f) = 0;

  DOCUMENT(R"(Retrieve the current :class:`~renderdoc.D3D11State` pipeline state.

The return value will be ``None`` if the capture is not using the D3D11 API.
You should determine the API of the capture first before fetching it.

:return: The current D3D11 pipeline state.
:rtype: renderdoc.D3D11State
)");
  virtual const D3D11Pipe::State *CurD3D11PipelineState() = 0;

  DOCUMENT(R"(Retrieve the current :class:`~renderdoc.D3D12State` pipeline state.

The return value will be ``None`` if the capture is not using the D3D12 API.
You should determine the API of the capture first before fetching it.

:return: The current D3D12 pipeline state.
:rtype: renderdoc.D3D12State
)");
  virtual const D3D12Pipe::State *CurD3D12PipelineState() = 0;

  DOCUMENT(R"(Retrieve the current :class:`~renderdoc.GLState` pipeline state.

The return value will be ``None`` if the capture is not using the OpenGL API.
You should determine the API of the capture first before fetching it.

:return: The current OpenGL pipeline state.
:rtype: renderdoc.GLState
)");
  virtual const GLPipe::State *CurGLPipelineState() = 0;

  DOCUMENT(R"(Retrieve the current :class:`~renderdoc.VKState` pipeline state.

The return value will be ``None`` if the capture is not using the Vulkan API.
You should determine the API of the capture first before fetching it.

:return: The current Vulkan pipeline state.
:rtype: renderdoc.VKState
)");
  virtual const VKPipe::State *CurVulkanPipelineState() = 0;

  DOCUMENT(R"(Retrieve the current :class:`~renderdoc.PipeState` abstracted pipeline state.

This pipeline state will always be valid, and allows queries that will work regardless of the
capture's API.

:return: The current API-agnostic abstracted pipeline state.
:rtype: renderdoc.PipeState
)");
  virtual const PipeState &CurPipelineState() = 0;

  DOCUMENT(R"(Retrieve the current persistant config.

:return: The current persistant config manager.
:rtype: PersistantConfig
)");
  virtual PersistantConfig &Config() = 0;

  DOCUMENT(R"(Retrieve the manager for extensions.

:return: The current extension manager.
:rtype: ExtensionManager
)");
  virtual IExtensionManager &Extensions() = 0;

protected:
  ICaptureContext() = default;
  ~ICaptureContext() = default;
};

DECLARE_REFLECTION_STRUCT(ICaptureContext);

DOCUMENT(R"(Retrieve the absolute path where a given file can be stored with other application
data.

:param str filename: The base filename.
:return: The absolute path.
:rtype: str
)");
rdcstr ConfigFilePath(const rdcstr &filename);

// simple helper for the common case of 'we just need to run this on the replay thread'
#define INVOKE_MEMFN(function)                                          \
  {                                                                     \
    QPointer<std::remove_reference<decltype(*this)>::type> meptr(this); \
    m_Ctx.Replay().AsyncInvoke([meptr](IReplayController *r) {          \
      if(meptr)                                                         \
        meptr->function(r);                                             \
    });                                                                 \
  }
