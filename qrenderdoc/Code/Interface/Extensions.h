/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2017-2019 Baldur Karlsson
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

.. data:: EventBrowser

  The :class:`EventBrowser`.

.. data:: PipelineStateViewer

  The :class:`PipelineStateViewer`.

.. data:: MeshPreview

  The mesh previewing :class:`BufferViewer`.

.. data:: TextureViewer

  The :class:`TextureViewer`.
)");
enum class PanelMenu
{
  Unknown,
  EventBrowser,
  PipelineStateViewer,
  MeshPreview,
  TextureViewer,
};

DECLARE_REFLECTION_ENUM(PanelMenu);

DOCUMENT(R"(Specifies the panel to add a menu item into.

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
:rtype: ``list`` of :class:`ExtensionMetadata`.
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
:return: If the extension loaded successfully or not.
:rtype: bool
)");
  virtual bool LoadExtension(rdcstr name) = 0;

  //////////////////////////////////////////////////////////////////////////
  // UI hook/callback registration

  DOCUMENT(R"(Register a new menu item in the main window's menus for an extension.

.. note:
  The intermediate submenu items will be created as needed, there's no need to register each item
  in the hierarchy. Registering a menu item and then registering a child is undefined, and the item
  with a child may not receive callbacks at the correct times.

:param WindowMenu base: The base menu to add the item to.
:param list submenus: A list of strings containing the submenus to add before the item. The last
  string will be the name of the menu item itself. Must contain at least one entry, or two entries
  if ``base`` is :data:`WindowMenu.NewMenu`.
:param callback: The function to callback when the menu item is selected.
:type method: :func:`ExtensionCallback`
)");
  virtual void RegisterWindowMenu(WindowMenu base, const rdcarray<rdcstr> &submenus,
                                  ExtensionCallback callback) = 0;

  DOCUMENT(R"(Register a menu item in a panel for an extension.

.. note:
  The intermediate submenu items will be created as needed, there's no need to register each item
  in the hierarchy. Registering a menu item and then registering a child is undefined, and the item
  with a child may not receive callbacks at the correct times.

:param PanelMenu base: The panel to add the item to.
:param list submenus: A list of strings containing the submenus to add before the item. The last
  string will be the name of the menu item itself. Must contain at least one entry.
:param callback: The function to callback when the menu item is selected.
:type method: :func:`ExtensionCallback`
)");
  virtual void RegisterPanelMenu(PanelMenu base, const rdcarray<rdcstr> &submenus,
                                 ExtensionCallback callback) = 0;

  DOCUMENT(R"(Register a context menu item in a panel for an extension.

.. note:
  The intermediate submenu items will be created as needed, there's no need to register each item
  in the hierarchy. Registering a menu item and then registering a child is undefined, and the item
  with a child may not receive callbacks at the correct times.

:param ContextMenu base: The panel to add the item to.
:param list submenus: A list of strings containing the submenus to add before the item. The last
  string will be the name of the menu item itself. Must contain at least one entry.
:param callback: The function to callback when the menu item is selected.
:type method: :func:`ExtensionCallback`
)");
  virtual void RegisterContextMenu(ContextMenu base, const rdcarray<rdcstr> &submenus,
                                   ExtensionCallback callback) = 0;

  //////////////////////////////////////////////////////////////////////////
  // Utility UI functions

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
:param list options: The buttons to display on the dialog.
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
