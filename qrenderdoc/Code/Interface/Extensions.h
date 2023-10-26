/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2023 Baldur Karlsson
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

#ifdef RENDERDOC_QT_COMPAT

typedef rdcarray<rdcpair<rdcstr, QVariant> > ExtensionCallbackData;
#define make_pyarg rdcpair<rdcstr, QVariant>

#endif

DOCUMENT(R"(Specifies the base menu to add a menu item into.

.. data:: Unknown

  Unknown/invalid window.

.. data:: File

  The menu item will be in a section between Open/Save/Close captures and Import/Export.

.. data:: Window

  The menu item will be in a new section at the end of the menu.

.. data:: Tools

  The menu item will be added to a new section above Settings.

.. data:: NewMenu

  The menu item will be a root menu, placed between Tools and Help.

.. data:: Help

  The menu item will be added after the error reporting item.
)");
enum class WindowMenu
{
  Unknown,
  File,
  Window,
  Tools,
  NewMenu,
  Help,
};

DECLARE_REFLECTION_ENUM(WindowMenu);

DOCUMENT(R"(Specifies the panel to add a menu item into.

.. data:: Unknown

  Unknown/invalid panel.

.. data:: EventBrowser

  The :class:`EventBrowser`.

.. data:: PipelineStateViewer

  The :class:`PipelineStateViewer`.

.. data:: MeshPreview

  The mesh previewing :class:`BufferViewer`.

.. data:: TextureViewer

  The :class:`TextureViewer`.

.. data:: BufferViewer

  Any non-mesh previewing :class:`BufferViewer`.
)");
enum class PanelMenu
{
  Unknown,
  EventBrowser,
  PipelineStateViewer,
  MeshPreview,
  TextureViewer,
  BufferViewer,
};

DECLARE_REFLECTION_ENUM(PanelMenu);

DOCUMENT(R"(Specifies the panel to add a menu item into.

.. data:: Unknown

  Unknown/invalid context menu.

.. data:: EventBrowser_Event

  Adds the item to the context menu for events in the :class:`EventBrowser`.

.. data:: MeshPreview_Vertex

  Adds the item to the context menu for all vertices in the mesh previewing :class:`BufferViewer`.

.. data:: MeshPreview_VSInVertex

  Adds the item to the context menu for vertex inputs in the mesh previewing :class:`BufferViewer`.

.. data:: MeshPreview_VSOutVertex

  Adds the item to the context menu for VS output in the mesh previewing :class:`BufferViewer`.

.. data:: MeshPreview_GSOutVertex

  Adds the item to the context menu for GS/Tess output in the mesh previewing :class:`BufferViewer`.

.. data:: MeshPreview_TaskOutVertex

  Adds the item to the context menu for task shader output in the mesh previewing :class:`BufferViewer`.

.. data:: MeshPreview_MeshOutVertex

  Adds the item to the context menu for mesh shader output in the mesh previewing :class:`BufferViewer`.

.. data:: TextureViewer_Thumbnail

  Adds the item to the context menu for all thumbnails in the :class:`TextureViewer`.

.. data:: TextureViewer_InputThumbnail

  Adds the item to the context menu for input thumbnails in the :class:`TextureViewer`.

.. data:: TextureViewer_OutputThumbnail

  Adds the item to the context menu for output thumbnails in the :class:`TextureViewer`.
)");
enum class ContextMenu
{
  Unknown,
  EventBrowser_Event,
  MeshPreview_Vertex,
  MeshPreview_VSInVertex,
  MeshPreview_VSOutVertex,
  MeshPreview_GSOutVertex,
  MeshPreview_TaskOutVertex,
  MeshPreview_MeshOutVertex,
  TextureViewer_Thumbnail,
  TextureViewer_InputThumbnail,
  TextureViewer_OutputThumbnail,
};

DECLARE_REFLECTION_ENUM(ContextMenu);

#if defined(ENABLE_PYTHON_FLAG_ENUMS)
ENABLE_PYTHON_FLAG_ENUMS;
#endif

DOCUMENT(R"(A button for a dialog prompt.

.. data:: OK

  An OK button

.. data:: Save

  A Save button

.. data:: SaveAll

  A Save All button

.. data:: Open

  An Open button

.. data:: Yes

  A Yes button

.. data:: YesToAll

  A Yes To All button

.. data:: No

  A No button

.. data:: NoToAll

  A No To All button

.. data:: Abort

  An Abort button

.. data:: Retry

  A Retry button

.. data:: Ignore

  An Ignore button

.. data:: Close

  A Close button

.. data:: Cancel

  A Cancel button

.. data:: Discard

  A Discard button

.. data:: Help

  A Help button

.. data:: Apply

  An Apply button

.. data:: Reset

  A Reset button

.. data:: RestoreDefaults

  A Restore Defaults button

)");
enum class DialogButton
{
  // keep this in sync with QMessageBox::StandardButton
  OK = 0x00000400,
  Save = 0x00000800,
  SaveAll = 0x00001000,
  Open = 0x00002000,
  Yes = 0x00004000,
  YesToAll = 0x00008000,
  No = 0x00010000,
  NoToAll = 0x00020000,
  Abort = 0x00040000,
  Retry = 0x00080000,
  Ignore = 0x00100000,
  Close = 0x00200000,
  Cancel = 0x00400000,
  Discard = 0x00800000,
  Help = 0x01000000,
  Apply = 0x02000000,
  Reset = 0x04000000,
  RestoreDefaults = 0x08000000,
};

DECLARE_REFLECTION_ENUM(DialogButton);
BITMASK_OPERATORS(DialogButton);

#if defined(DISABLE_PYTHON_FLAG_ENUMS)
DISABLE_PYTHON_FLAG_ENUMS;
#endif

DOCUMENT("The metadata for an extension.");
struct ExtensionMetadata
{
  DOCUMENT("");
  bool operator==(const ExtensionMetadata &o) const
  {
    return extensionAPI == o.extensionAPI && filePath == o.filePath && package == o.package &&
           name == o.name && version == o.version && author == o.author &&
           extensionURL == o.extensionURL && description == o.description;
  }
  bool operator<(const ExtensionMetadata &o) const
  {
    if(!(extensionAPI == o.extensionAPI))
      return extensionAPI < o.extensionAPI;
    if(!(filePath == o.filePath))
      return filePath < o.filePath;
    if(!(package == o.package))
      return package < o.package;
    if(!(name == o.name))
      return name < o.name;
    if(!(version == o.version))
      return version < o.version;
    if(!(author == o.author))
      return author < o.author;
    if(!(extensionURL == o.extensionURL))
      return extensionURL < o.extensionURL;
    if(!(description == o.description))
      return description < o.description;
    return false;
  }

  DOCUMENT("The version of the extension API that this extension is written against");
  int extensionAPI;

  DOCUMENT("The location of this package on disk");
  rdcstr filePath;

  DOCUMENT("The python package for this extension, e.g. foo.bar");
  rdcstr package;

  DOCUMENT("The short friendly name for the extension");
  rdcstr name;

  DOCUMENT("The version of the extension");
  rdcstr version;

  DOCUMENT("The author of the extension, optionally with an email contact");
  rdcstr author;

  DOCUMENT("The URL for where the extension is fetched from");
  rdcstr extensionURL;

  DOCUMENT("A longer description of what the extension does");
  rdcstr description;
};

DECLARE_REFLECTION_STRUCT(ExtensionMetadata);

typedef struct _object PyObject;

DOCUMENT(R"(Python can have direct access to Qt via PySide2, but this is not always available in
all RenderDoc builds. To aid extensions to manipulate widgets in a simple but portable fashion this
helper exposes a small subset of Qt via RenderDoc's python bindings.

The intention is not to allow fully flexible building of Qt panels, but to allow access to some
basic UI building tools for simple data input and display which can be used on any RenderDoc build.

.. note::
  The widget handles returned are PySide2 widgets where that is available, so this can be used to
  make a basic UI and optionally customise it further with PySide2 when possible.

.. function:: WidgetCallback(context, widget, text)

  Not a member function - the signature for any ``WidgetCallback`` callbacks.

  Callback for widgets can be registered at creation time, the text field is optional and may be
  blank depending on the event, but the context and widget are always valid.

  :param CaptureContext context: The current capture context.
  :param QWidget widget: The widget sending the callback.
  :param str text: Additional data for the call, such as the current or selected text.

.. function:: InvokeCallback(context, widget, text)

  Not a member function - the signature for any ``InvokeCallback`` callbacks.

  Callback for invoking onto the UI thread from another thread (in particular the replay thread).
  Takes no parameters as the callback is expected to store its own state.
)");
struct IMiniQtHelper
{
  typedef std::function<void(ICaptureContext *, QWidget *, rdcstr)> WidgetCallback;
  typedef std::function<void()> InvokeCallback;

  DOCUMENT(R"(Invoke a callback on the UI thread. All widget accesses must come from the UI thread,
so if work has been done on the render thread then this function can be used to asynchronously and
safely go back to the UI thread.

This function is safe to call on the UI thread, but it will synchronously call the callback
immediately before returning.

.. note::
  No parameters are provided to the callback, it is assumed that the callback will maintain its own
  context as needed.

:param InvokeCallback callback: The callback to invoke on the UI thread.
)");
  virtual void InvokeOntoUIThread(InvokeCallback callback) = 0;

  // top level widgets

  DOCUMENT(R"(Creates and returns a top-level widget for creating layouts.

The widget is not immediately visible. It should be shown either with :meth:`ShowWidgetAsDialog` or
with :meth:`CaptureContext.AddDockWindow` once it's ready.

This widget can have children added, but it is recommended to immediately add only one child which
is a layout type widget, to allow customising how children are added. By default the children are
added in a vertical layout.

:param str windowTitle: The title of any window with this widget as its root.
:param WidgetCallback closed: A callback that will be called when the widget is closed by the user.
  This implicitly deletes the widget and all its children, which will no longer be valid even if a
  handle to them exists.
:return: The handle to the newly created widget.
:rtype: QWidget
)");
  virtual QWidget *CreateToplevelWidget(const rdcstr &windowTitle, WidgetCallback closed) = 0;

  DOCUMENT(R"(Closes a top-level widget as if the user had clicked to close.

This function is undefined if used on a non top-level widget. It will invoke the closed widget
callback.

:param QWidget widget: The top-level widget to close.
)");
  virtual void CloseToplevelWidget(QWidget *widget) = 0;

  // widget hierarchy

  DOCUMENT(R"(Set the internal name of a widget. This is not displayed anywhere but can be used by
:meth:`FindChildByName` to locate a widget within a hierarchy.

.. note::
  Names are optional and only for your use. Nothing prevents from you from setting duplicate names,
  but this makes searches by name ambiguous.

:param QWidget widget: The widget to set an internal name for.
:param str name: The internal name to set for the widget.
)");
  virtual void SetWidgetName(QWidget *widget, const rdcstr &name) = 0;

  DOCUMENT(R"(Return the internal name of a widget, as set my :meth:`SetWidgetName`.

:param QWidget widget: The widget to query.
:return: The widget's internal name, which may be an empty string if no name has been set.
:rtype: str
)");
  virtual rdcstr GetWidgetName(QWidget *widget) = 0;

  DOCUMENT(R"(Return the type of the widget as a string. This type is the Qt type name so this
should only be used for debugging as the name may change even if for the same type of widget.

:param QWidget widget: The widget to query.
:return: The widget's type name.
:rtype: str
)");
  virtual rdcstr GetWidgetType(QWidget *widget) = 0;

  DOCUMENT(R"(Find a child widget of a parent by internal name.

:param QWidget parent: The widget to start the search from.
:param str name: The internal name to search for.
:return: The handle to the first widget with a matching name, or ``None`` if no widget is found.
:rtype: QWidget
)");
  virtual QWidget *FindChildByName(QWidget *parent, const rdcstr &name) = 0;

  DOCUMENT(R"(Return the parent of a widget in the widget hierarchy.

.. note::
  The widget returned may not be a widget created through this helper interface if the specified
  widget has been docked somewhere. Beware making changes to any widgets returned as you may modify
  the RenderDoc UI itself.

:param QWidget widget: The widget to query.
:return: The handle to the parent widget with a matching name, or ``None`` if this widget is either
  not yet parented or is a top-level window.
:rtype: QWidget
)");
  virtual QWidget *GetParent(QWidget *widget) = 0;

  DOCUMENT(R"(Return the number of children this widget has. This is generally only useful for
layout type widgets.

:param QWidget widget: The widget to query.
:return: The number of child widgets this widget has.
:rtype: int
)");
  virtual int32_t GetNumChildren(QWidget *widget) = 0;

  DOCUMENT(R"(Return a child widget for a parent.

:param QWidget parent: The parent widget to look up.
:param int index: The child index to return.
:return: The specified child of the parent, or ``None`` if the index is out of bounds.
:rtype: QWidget
)");
  virtual QWidget *GetChild(QWidget *parent, int32_t index) = 0;

  DOCUMENT(R"(Destroy a widget. Widgets stay alive unless explicitly destroyed here, OR in one other
case when they are in a widget hiearchy under a top-level window which the user closes, which can
be detected with the callback parameter in :meth:`CreateToplevelWidget`.

If the widget being destroyed is a top-level window, it will be closed. Otherwise if it is part of a
widget hierarchy it will be removed from its parent automatically. You can remove a widget and then
destroy it if you wish, but you must not destroy a widget then attempt to remove it from its parent,
as after the call to this function the widget is no longer valid to use.

All children under this widget will be destroyed recursively as well, which will be made invalid
even if a handle to them exists.

:param QWidget widget: The widget to destroy.
)");
  virtual void DestroyWidget(QWidget *widget) = 0;

  // dialogs

  DOCUMENT(R"(Show a top-level widget as a blocking modal dialog. This is most useful to prompt the
user for some specific information.

The dialog is only closed when the user closes the window explicitly or if you call
:meth:`CloseCurrentDialog` in a widget callback, e.g. upon a button press.

:param QWidget widget: The top-level widget to show as a dialog.
:return: Whether the dialog was closed successfully, via :meth:`CloseCurrentDialog`.
:rtype: bool
)");
  virtual bool ShowWidgetAsDialog(QWidget *widget) = 0;

  DOCUMENT(R"(Close the active modal dialog. This does nothing if no dialog is being shown.

.. note::
  Closing a dialog 'sucessfully' does nothing except modify the return value of
  :meth:`CloseCurrentDialog`. It allows quick distinguishing between OK and Cancel actions without
  having to carry that information separately in a global or other state.

:param bool success: ``True`` if the dialog was successful (the user clicked an OK/Accept type
  button).
)");
  virtual void CloseCurrentDialog(bool success) = 0;

  // layout functions

  DOCUMENT(R"(Creates and returns a horizontal layout widget.

The widget needs to be added to a parent to become part of a panel or window.

Children added to this layout widget are listed horizontally. Widget sizing follows default logic,
which typically has some widgets be only large enough for their content and others which are
'greedy' evenly divide any remaining free space.

:return: The handle to the newly created widget.
:rtype: QWidget
)");
  virtual QWidget *CreateHorizontalContainer() = 0;

  DOCUMENT(R"(Creates and returns a vertical layout widget.

The widget needs to be added to a parent to become part of a panel or window.

Children added to this layout widget are listed vertically. Widget sizing follows default logic,
which typically has some widgets be only large enough for their content and others which are
'greedy' evenly divide any remaining free space.

:return: The handle to the newly created widget.
:rtype: QWidget
)");
  virtual QWidget *CreateVerticalContainer() = 0;

  DOCUMENT(R"(Creates and returns a grid layout widget.

The widget needs to be added to a parent to become part of a panel or window.

Children added to this layout widget are arranged in a grid. Widget sizing follows default logic,
which typically has some widgets be only large enough for their content and others which are
'greedy' evenly divide any remaining free space. This will not violate the grid constraint though.

:return: The handle to the newly created widget.
:rtype: QWidget
)");
  virtual QWidget *CreateGridContainer() = 0;

  DOCUMENT(R"(Creates and returns a spacer widget.

This widget is completely empty but consumes empty space, meaning all other non-greedy widgets in
the same container will be minimally sized. This can be useful for simple layouts.

:param bool horizontal: ``True`` if this spacer should consume horizontal space, ``False`` if this
  spacer should consume vertical space. Typically this matches the direction of the layout it is
  in.
:return: The handle to the newly created widget.
:rtype: QWidget
)");
  virtual QWidget *CreateSpacer(bool horizontal) = 0;

  DOCUMENT(R"(Removes all child widgets from a parent and makes them invisible.

These widgets remain valid and can be re-added to another parent or the same parent.

:param QWidget parent: The parent widget to clear of  children.
)");
  virtual void ClearContainedWidgets(QWidget *parent) = 0;

  DOCUMENT(R"(Adds a child widget to a grid layout. If the parent is not a grid layout nothing will
happen and the widget will not be added anywhere.

:param QWidget parent: The parent grid layout widget.
:param int row: The row at which to add the child widget.
:param int column: The column at which to add the child widget.
:param QWidget child: The child widget to add.
:param int rowSpan: How many rows should this child span over.
:param int columnSpan: How many columns should this child span over.
)");
  virtual void AddGridWidget(QWidget *parent, int32_t row, int32_t column, QWidget *child,
                             int32_t rowSpan, int32_t columnSpan) = 0;

  DOCUMENT(R"(Adds a child widget to the end of an ordered layout (either horizontal or vertical).
If the parent is not an ordered layout nothing will happen and the widget will not be added anywhere.

:param QWidget parent: The parent grid layout widget.
:param QWidget child: The child widget to add.
)");
  virtual void AddWidget(QWidget *parent, QWidget *child) = 0;

  DOCUMENT(R"(Insert a child widget at the specified index in an ordered layout (either horizontal
or vertical). If the parent is not an ordered layout nothing will happen and the widget will not be
added anywhere.

:param QWidget parent: The parent grid layout widget.
:param int index: The index to insert the widget at. If this index is out of bounds it will be
  clamped, so that negative indices will be equivalent to index 0 and all indices above the number
  of children will append the widget
:param QWidget child: The child widget to add.
)");
  virtual void InsertWidget(QWidget *parent, int32_t index, QWidget *child) = 0;

  // widget manipulation

  DOCUMENT(R"(Set the 'text' of a widget. How this manifests depends on the type of the widget, for
example a text-box or label will set the text directly. For a checkbox or radio button this will
add text next to it.

:param QWidget widget: The widget to set text for.
:param str text: The text to set for the widget.
)");
  virtual void SetWidgetText(QWidget *widget, const rdcstr &text) = 0;

  DOCUMENT(R"(Return the current text of a widget. See :meth:`SetWidgetText`.

:param QWidget widget: The widget to query.
:return: The widget's current text, which may be an empty string if no valid text is available.
:rtype: str
)");
  virtual rdcstr GetWidgetText(QWidget *widget) = 0;

  DOCUMENT(R"(Change the font properties of a widget.

:param QWidget widget: The widget to change font of.
:param str font: The new font family to use, or an empty string to leave the font family the same.
:param int fontSize: The new font point size to use, or 0 to leave the size the same.
:param bool bold: ``True`` if the font should be bold.
:param bool italic: ``True`` if the font should be italic.
)");
  virtual void SetWidgetFont(QWidget *widget, const rdcstr &font, int32_t fontSize, bool bold,
                             bool italic) = 0;

  DOCUMENT(R"(Set whether the widget is enabled or not. This generally only affects interactive
widgets and not fixed widgets, interactive widgets become read-only while still displaying the same
data.

.. note::
  Disabled widgets can still be modified programmatically, they are only disabled for the user.

:param QWidget widget: The widget to enable or disable.
:param bool enabled: ``True`` if the widget should be enabled.
)");
  virtual void SetWidgetEnabled(QWidget *widget, bool enabled) = 0;

  DOCUMENT(R"(Return the current enabled-state of a widget. See :meth:`SetWidgetEnabled`.

:param QWidget widget: The widget to query.
:return: ``True`` if the widget is currently enabled.
:rtype: bool
)");
  virtual bool IsWidgetEnabled(QWidget *widget) = 0;

  DOCUMENT(R"(Set whether the widget is visible or not. An invisible widget maintains its position
in the hierarchy but is not visible and cannot be interacted with in any way.

:param QWidget widget: The widget to show or hide.
:param bool visible: ``True`` if the widget should be made visible (shown).
)");
  virtual void SetWidgetVisible(QWidget *widget, bool visible) = 0;

  DOCUMENT(R"(Return the current visibility of a widget. See :meth:`SetWidgetVisible`.

This query is recursive - a widget could be individually visible, but if it is under a parent which
is invisible then this widget will be returned as invisible.

:param QWidget widget: The widget to query.
:return: ``True`` if the widget is currently visible.
:rtype: bool
)");
  virtual bool IsWidgetVisible(QWidget *widget) = 0;

  // specific widgets

  DOCUMENT(R"(Create a groupbox widget which can optionally allow collapsing.

This widget can have children added, but it is recommended to immediately add only one child which
is a layout type widget, to allow customising how children are added. By default the children are
added in a vertical layout.

The widget needs to be added to a parent to become part of a panel or window.

:param bool collapsible: ``True`` if the groupbox should have a toggle in its header to allow
  collapsing its contents down vertically.
:return: The handle to the newly created widget.
:rtype: QWidget
)");
  virtual QWidget *CreateGroupBox(bool collapsible) = 0;

  DOCUMENT(R"(Create a normal button widget.

:param WidgetCallback pressed: Callback to be called when the button is pressed.
:return: The handle to the newly created widget.
:rtype: QWidget
)");
  virtual QWidget *CreateButton(WidgetCallback pressed) = 0;

  DOCUMENT(R"(Create a read-only label widget.

.. note::
  This widget will be blank by default, you can set the text with :meth:`SetWidgetText`.

:return: The handle to the newly created widget.
:rtype: QWidget
)");
  virtual QWidget *CreateLabel() = 0;

  DOCUMENT(R"(Set an image for a label widget. If the widget isn't a label, this call has no effect.

The label will be resized to a fixed size to display the image at 100% scale. Any text in the label
will not be displayed, but passing an empty image will revert the label back to being text-based.

The data must be in RGB(A) format with the first byte of each texel being R.

:param QWidget widget: The widget to set the picture for.
:param bytes data: The image data itself, tightly packed.
:param int width: The width of the image in pixels.
:param int height: The height of the image in pixels.
:param bool alpha: ``True`` if the image data contains an alpha channel.
)");
  virtual void SetLabelImage(QWidget *widget, const bytebuf &data, int32_t width, int32_t height,
                             bool alpha) = 0;

  DOCUMENT(R"(Create a widget suitable for rendering to with a :class:`renderdoc.ReplayOutput`. This
widget takes care of painting on demand and recreating the internal display widget when necessary,
however this means you must use :meth:`GetWidgetWindowingData` to retrieve the windowing data for
creating the output as well as call :meth:`SetWidgetReplayOutput` to notify the widget of the
current output.

:return: The handle to the newly created widget.
:rtype: QWidget
)");
  virtual QWidget *CreateOutputRenderingWidget() = 0;

  DOCUMENT(R"(Return the opaque pointer of windowing data suitable for passing to
:meth:`~renderdoc.ReplayController.CreateOutput` or other functions that expect windowing data.

If the widget is not a output rendering widget created with :meth:`CreateOutputRenderingWidget` this
function will fail and return an invalid set of windowing data.

It's important to note that the windowing data is not valid forever, so this function should be
called as close to where you call :meth:`~renderdoc.ReplayController.CreateOutput` as possible.
Also don't fetch windowing data unless you are going to create an output, because this function will
cause the widget to go into an undefined state unless an output is created to render onto it.

.. note::
  This function must be called on the main UI thread.

:param QWidget widget: The widget to create windowing data for.
:return: The windowing data.
:rtype: renderdoc.WindowingData
)");
  virtual WindowingData GetWidgetWindowingData(QWidget *widget) = 0;

  DOCUMENT(R"(Set the current output for a widget. This only affects output rendering widgets. If
another type of widget is passed nothing will happen.

Passing ``None`` as the output will reset the widget and make it display the default background
until another output is set.

When a capture is closed and all outputs are destroyed, the widget will automatically unset the
output so there is no need to do that manually.

:param QWidget widget: The widget to set the output for.
:param renderdoc.ReplayOutput output: The new output to set, or ``None`` to unset any previous
  output.
)");
  virtual void SetWidgetReplayOutput(QWidget *widget, IReplayOutput *output) = 0;

  DOCUMENT(R"(Set the default backkground color for a rendering widget. This background color is
used when no output is currently configured, e.g. when a capture is closed.

For all other widget types this has no effect.

To disable the background color pass negative values for the components, this will cause a default
checkerboard to be rendered instead. This is the default behaviour when a widget is created.

:param QWidget widget: The widget to set the background color of.
:param float red: The red component of the color, in the range ``0.0 - 1.0``.
:param float green: The green component of the color, in the range ``0.0 - 1.0``.
:param float blue: The blue component of the color, in the range ``0.0 - 1.0``.
)");
  virtual void SetWidgetBackgroundColor(QWidget *widget, float red, float green, float blue) = 0;

  DOCUMENT(R"(Create a checkbox widget which can be toggled between unchecked and checked. When
created the checkbox is unchecked.

:param WidgetCallback changed: Callback to be called when the widget is toggled.
:return: The handle to the newly created widget.
:rtype: QWidget
)");
  virtual QWidget *CreateCheckbox(WidgetCallback changed) = 0;

  DOCUMENT(R"(Create a radio box widget which can be toggled between unchecked and checked but with
at most one radio box in any group of sibling radio boxes being checked.

Upon creation the radio box is unchecked, even in a group of other radio boxes that are unchecked.
If you want a default radio box to be checked, you should use :meth:`SetWidgetChecked`.

:param WidgetCallback changed: Callback to be called when the widget is toggled.
:return: The handle to the newly created widget.
:rtype: QWidget
)");
  virtual QWidget *CreateRadiobox(WidgetCallback changed) = 0;

  DOCUMENT(R"(Set whether the widget is checked or not. This only affects checkboxes and radio
boxes and group box. If another type of widget is passed nothing will happen.

:param QWidget checkableWidget: The widget to check or uncheck.
:param bool checked: ``True`` if the widget should be checked.
)");
  virtual void SetWidgetChecked(QWidget *checkableWidget, bool checked) = 0;

  DOCUMENT(R"(Return the current checked-state of a widget. See :meth:`SetWidgetChecked`. If another
type of widget is passed other than a checkbox or radio box or group box ``False`` will be returned.

:param QWidget checkableWidget: The widget to query.
:return: ``True`` if the widget is currently checked.
:rtype: bool
)");
  virtual bool IsWidgetChecked(QWidget *checkableWidget) = 0;

  DOCUMENT(R"(Create a spinbox widget with a numerical value and up/down buttons to change it.

The number of decimal places can be set to 0 for an integer spinbox, and in that case the step
should be set to 1.0.

By default the spinbox has minimum and maximum values of 0.0 and 100.0, these can be changed with
:meth:`SetSpinboxBounds`.

:param int decimalPlaces: The number of decimal places to display when showing the number.
:param float step: The step value to apply in each direction when clicking up or down.
:return: The handle to the newly created widget.
:rtype: QWidget
)");
  virtual QWidget *CreateSpinbox(int32_t decimalPlaces, double step) = 0;

  DOCUMENT(R"(Set the minimum and maximum values allowed in the spinbox. If another type of widget
is passed nothing will happen.

:param QWidget spinbox: The spinbox.
:param float minVal: The minimum value allowed for the spinbox to reach. Lower values entered will
  be clamped to this.
:param float maxVal: The maximum value allowed for the spinbox to reach. Higher values entered will
  be clamped to this.
)");
  virtual void SetSpinboxBounds(QWidget *spinbox, double minVal, double maxVal) = 0;

  DOCUMENT(R"(Set the value contained in a spinbox. If another type of widget is passed nothing will
happen.

:param QWidget spinbox: The spinbox.
:param float value: The value for the spinbox, which will be clamped by the current bounds.
)");
  virtual void SetSpinboxValue(QWidget *spinbox, double value) = 0;

  DOCUMENT(R"(Return the current value of a spinbox widget. If another type of widget is passed
``0.0`` will be returned.

:param QWidget spinbox: The widget to query.
:return: The current  value of the spinbox.
:rtype: float
)");
  virtual double GetSpinboxValue(QWidget *spinbox) = 0;

  DOCUMENT(R"(Create a text box widget for the user to enter text into.

:param bool singleLine: ``True`` if the widget should be a single-line entry, otherwise it is a
  multi-line text box.
:param WidgetCallback changed: Callback to be called when the text in the textbox is changed.
:return: The handle to the newly created widget.
:rtype: QWidget
)");
  virtual QWidget *CreateTextBox(bool singleLine, WidgetCallback changed) = 0;

  DOCUMENT(R"(Create a drop-down combo box widget.

When created there are no pre-defined entries in the drop-down section. This can be changed with
:meth:`SetComboOptions`.

:param bool editable: ``True`` if the widget should allow the user to enter any text they wish as
  well as being able to select a pre-defined entry.
:param WidgetCallback changed: Callback to be called when the text in the combobox is changed. This
  will be called both when a new option is selected or when the user edits the text.
:return: The handle to the newly created widget.
:rtype: QWidget
)");
  virtual QWidget *CreateComboBox(bool editable, WidgetCallback changed) = 0;

  DOCUMENT(R"(Set the pre-defined options in a drop-down combo box. If another type of widget is
passed nothing will happen.

:param QWidget combo: The combo box.
:param List[str] options: The new options for the combo box.
)");
  virtual void SetComboOptions(QWidget *combo, const rdcarray<rdcstr> &options) = 0;

  DOCUMENT(R"(Get the number of options in a drop-down combo box. If another type of widget is
passed ``0`` will be returned.

:param QWidget combo: The combo box.
:return: The current number of options.
:rtype: int
)")
  virtual size_t GetComboCount(QWidget *combo) = 0;

  DOCUMENT(R"(Select the current option in a drop-down combo box. If another type of widget or
an unknown option is passed, nothing will happen.

:param QWidget combo: The combo box.
:param str option: The option to select.
)")
  virtual void SelectComboOption(QWidget *combo, const rdcstr &option) = 0;

  DOCUMENT(R"(Create a progress bar widget.

By default the progress bar has minimum and maximum values of 0 and 100. These can be changed with
:meth:`SetProgressBarRange`.

:param bool horizontal: the progress bar orientation, true for horizontal otherwise vertical.
:return: The handle to the newly created widget.
:rtype: QWidget

)")
  virtual QWidget *CreateProgressBar(bool horizontal) = 0;

  DOCUMENT(R"(Reset a progress bar widget.

Rewinds the progress bar's indicator and hides the indicator's label (theme dependent). If you want
to keep the label visible, call :meth:`SetProgressBarValue(0)` instead. The minimum and maximum values
are not changed.

:param QWidget pbar: the progress bar.
)")
  virtual void ResetProgressBar(QWidget *pbar) = 0;

  DOCUMENT(R"(Set the progress bar's current value.

Attempting to change the current value outside the minimum and maximum range does not affect
the current value.

:param QWidget pbar: the progress bar.
:param int value: the new current value.
)")
  virtual void SetProgressBarValue(QWidget *pbar, int32_t value) = 0;

  DOCUMENT(R"(Set the progress bar's current value relative to the existing value.

:param QWidget pbar: the progress bar.
:param int delta: the relative value to update the current value.
)")
  virtual void UpdateProgressBarValue(QWidget *pbar, int32_t delta) = 0;

  DOCUMENT(R"(Get the progress bar's current value.

:param QWidget pbar: the progress bar.
:return: The current value of the progress bar.
:rtype: int
)")
  virtual int32_t GetProgressBarValue(QWidget *pbar) = 0;

  DOCUMENT(R"(Set a progress bar's minimum and maximum values.

If maximum is smaller than minimum, minimum is set as the maximum, too. If the current value falls
outside the new range, the progress bar is reset. Use range (0, 0) to set the progress bar to
indeterminated state (the progress cannot be estimated or is not being calculated).

:param QWidget pbar: the progress bar.
:param int minimum: the minimum value.
:param int maximum: the maximum value.
)")
  virtual void SetProgressBarRange(QWidget *pbar, int32_t minimum, int32_t maximum) = 0;

  DOCUMENT(R"(Get the minimum value of the progress bar's range.

:param QWidget pbar: the progress bar.
:return: The minimum value of the range.
:rtype: int
)")
  virtual int32_t GetProgressBarMinimum(QWidget *pbar) = 0;

  DOCUMENT(R"(Get the maximum value of the progress bar's range.

:param QWidget pbar: the progress bar.
:return: The maximum value of the range.
:rtype: int
)")
  virtual int32_t GetProgressBarMaximum(QWidget *pbar) = 0;

protected:
  DOCUMENT("");
  IMiniQtHelper() = default;
  ~IMiniQtHelper() = default;
};

DECLARE_REFLECTION_STRUCT(IMiniQtHelper);

DOCUMENT(R"(A manager for listing available and active extensions, as well as the interface for
extensions to register hooks and additional functionality.

.. function:: ExtensionCallback(context, data)

  Not a member function - the signature for any ``ExtensionCallback`` callbacks.

  Callback for extensions to register entry points with, used in many situations depending on how it
  was registered.

  :param CaptureContext context: The current capture context.
  :param dict data: Additional data for the call, as a dictionary with string keys.
    Context-dependent based on what generated the callback
)");
struct IExtensionManager
{
  typedef std::function<void(ICaptureContext *ctx, const rdcarray<rdcpair<rdcstr, PyObject *> > &data)>
      ExtensionCallback;

  //////////////////////////////////////////////////////////////////////////
  // Extension management

  DOCUMENT(R"(Retrieve a list of installed extensions.

:return: The list of installed extensions.
:rtype: List[ExtensionMetadata]
)");
  virtual rdcarray<ExtensionMetadata> GetInstalledExtensions() = 0;

  DOCUMENT(R"(Check if an installed extension is enabled.

:param str name: The qualified name of the extension, e.g. ``foo.bar``
:return: If the extension is enabled or not.
:rtype: bool
)");
  virtual bool IsExtensionLoaded(rdcstr name) = 0;

  DOCUMENT(R"(Enable an extension by name. If the extension is already enabled, this will reload it.

:param str name: The qualified name of the extension, e.g. ``foo.bar``
:return: If the extension loaded successfully, an empty string, otherwise the errors encountered
  while loading it.
:rtype: str
)");
  virtual rdcstr LoadExtension(rdcstr name) = 0;

  //////////////////////////////////////////////////////////////////////////
  // UI hook/callback registration

  DOCUMENT(R"(Register a new menu item in the main window's menus for an extension.

.. note:
  The intermediate submenu items will be created as needed, there's no need to register each item
  in the hierarchy. Registering a menu item and then registering a child is undefined, and the item
  with a child may not receive callbacks at the correct times.

:param WindowMenu base: The base menu to add the item to.
:param List[str] submenus: A list of strings containing the submenus to add before the item. The
  last string will be the name of the menu item itself. Must contain at least one entry, or two
  entries if ``base`` is :data:`WindowMenu.NewMenu`.
:param ExtensionCallback callback: The function to callback when the menu item is selected.
)");
  virtual void RegisterWindowMenu(WindowMenu base, const rdcarray<rdcstr> &submenus,
                                  ExtensionCallback callback) = 0;

  DOCUMENT(R"(Register a menu item in a panel for an extension.

.. note:
  The intermediate submenu items will be created as needed, there's no need to register each item
  in the hierarchy. Registering a menu item and then registering a child is undefined, and the item
  with a child may not receive callbacks at the correct times.

:param PanelMenu base: The panel to add the item to.
:param List[str] submenus: A list of strings containing the submenus to add before the item. The
  last string will be the name of the menu item itself. Must contain at least one entry.
:param ExtensionCallback callback: The function to callback when the menu item is selected.
)");
  virtual void RegisterPanelMenu(PanelMenu base, const rdcarray<rdcstr> &submenus,
                                 ExtensionCallback callback) = 0;

  DOCUMENT(R"(Register a context menu item in a panel for an extension.

.. note:
  The intermediate submenu items will be created as needed, there's no need to register each item
  in the hierarchy. Registering a menu item and then registering a child is undefined, and the item
  with a child may not receive callbacks at the correct times.

:param ContextMenu base: The panel to add the item to.
:param List[str] submenus: A list of strings containing the submenus to add before the item. The
  last string will be the name of the menu item itself. Must contain at least one entry.
:param ExtensionCallback callback: The function to callback when the menu item is selected.
)");
  virtual void RegisterContextMenu(ContextMenu base, const rdcarray<rdcstr> &submenus,
                                   ExtensionCallback callback) = 0;

  //////////////////////////////////////////////////////////////////////////
  // Utility UI functions

  DOCUMENT(R"(Returns a handle to the mini Qt helper. See :class:`MiniQtHelper`.

:return: The helper interface.
:rtype: MiniQtHelper
)");
  virtual IMiniQtHelper &GetMiniQtHelper() = 0;

  DOCUMENT(R"(Display a simple informational message dialog.

:param str text: The text of the dialog itself, required.
:param str title: The dialog title, optional.
)");
  virtual void MessageDialog(const rdcstr &text,
                             const rdcstr &title = "Python Extension Message") = 0;

  DOCUMENT(R"(Display an error message dialog.

:param str text: The text of the dialog itself, required.
:param str title: The dialog title, optional.
)");
  virtual void ErrorDialog(const rdcstr &text, const rdcstr &title = "Python Extension Error") = 0;

  DOCUMENT(R"(Display an error message dialog.

:param str text: The text of the dialog itself, required.
:param List[DialogButton] options: The buttons to display on the dialog.
:param str title: The dialog title, optional.
:return: The button that was clicked on.
:rtype: DialogButton
)");
  virtual DialogButton QuestionDialog(const rdcstr &text, const rdcarray<DialogButton> &options,
                                      const rdcstr &title = "Python Extension Prompt") = 0;

  DOCUMENT(R"(Browse for a filename to open.

:param str caption: The dialog title, optional.
:param str dir: The starting directory for browsing, optional.
:param str filter: The filter to apply for filenames, optional.
:return: The filename selected, or an empty string if nothing was selected.
:rtype: str
)");
  virtual rdcstr OpenFileName(const rdcstr &caption = "Open a file", const rdcstr &dir = rdcstr(),
                              const rdcstr &filter = rdcstr()) = 0;

  DOCUMENT(R"(Browse for a directory to open.

:param str caption: The dialog title, optional.
:param str dir: The starting directory for browsing, optional.
:return: The directory selected, or an empty string if nothing was selected.
:rtype: str
)");
  virtual rdcstr OpenDirectoryName(const rdcstr &caption = "Open a directory",
                                   const rdcstr &dir = rdcstr()) = 0;

  DOCUMENT(R"(Browse for a filename to save to.

:param str caption: The dialog title, optional.
:param str dir: The starting directory for browsing, optional.
:param str filter: The filter to apply for filenames, optional.
:return: The filename selected, or an empty string if nothing was selected.
:rtype: str
)");
  virtual rdcstr SaveFileName(const rdcstr &caption = "Save a file", const rdcstr &dir = rdcstr(),
                              const rdcstr &filter = rdcstr()) = 0;

#if !defined(SWIG) && !defined(SWIG_GENERATED)
  // not exposed to SWIG, only used internally. For when a menu is displayed dynamically in a panel,
  // this function is called to add any relevant menu items. Doing this almost immediate-mode GUI
  // style avoids complex retained state that has to be refreshed each time a panel is created.
  //
  // THIS MUST BE AT THE END OF THE CLASS, otherwise the vtable is inconsistent between
  // SWIG-generated code and normal code
  virtual void MenuDisplaying(ContextMenu contextMenu, QMenu *menu,
                              const ExtensionCallbackData &data) = 0;
  virtual void MenuDisplaying(PanelMenu panelMenu, QMenu *menu, QWidget *extensionButton,
                              const ExtensionCallbackData &data) = 0;
#endif

protected:
  DOCUMENT("");
  IExtensionManager() = default;
  ~IExtensionManager() = default;
};

DECLARE_REFLECTION_STRUCT(IExtensionManager);
