#pragma once

// take care before adding any more headers here, as they must be converted to python types. Any
// types in the RenderDoc core interface are already wrapped, and Qt types must either be manually
// converted directly to python, or interfaced with PySide, otherwise we get into the situation
// where pyside and SWIG have independent incompatible wrappers of Qt types
#include <QDateTime>
#include <QList>
#include <QMap>
#include <QPair>
#include <QString>
#include <QVector>

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

// we depend on the internal RenderDoc API, but the bindings for that are imported entirely
#include "renderdoc_replay.h"

// this is pre-declared as an opaque type as we only support converting to QWidget* via PySide
class QWidget;

// we only support QVariant as an 'internal' interface, it's not exposed to python. However we need
// to use it in constructors/operators so conditionally compile it rather than split small structs
// into interface/implementations
#if defined(SWIG)

#define VARIANT_CAST(classname)

#else

#include <QVariant>

// conversion to/from QVariant
#define VARIANT_CAST(classname)   \
  classname(const QVariant &var); \
  operator QVariant() const;

#endif

#include "CommonPipelineState.h"
#include "PersistantConfig.h"
#include "RemoteHost.h"

struct ICaptureContext;

DOCUMENT("Contains all of the settings that control how to capture an executable.");
struct CaptureSettings
{
  CaptureSettings();

  VARIANT_CAST(CaptureSettings);

  DOCUMENT("The :class:`~renderdoc.CaptureOptions` with fine-tuned settings for the capture.");
  CaptureOptions Options;
  DOCUMENT(
      "``True`` if the described capture is an inject-into-process instead of a launched "
      "executable.");
  bool Inject;
  DOCUMENT("``True`` if this capture settings object should be immediately executed upon load.");
  bool AutoStart;
  DOCUMENT("The path to the executable to run.");
  QString Executable;
  DOCUMENT("The path to the working directory to run in, or blank for the executable's directory.");
  QString WorkingDir;
  DOCUMENT("The command line to pass when running :data:`Exectuable`.");
  QString CmdLine;
  DOCUMENT(
      "A ``list`` of :class:`~renderdoc.EnvironmentModification` with environment changes to "
      "apply.");
  QList<EnvironmentModification> Environment;
};

DECLARE_REFLECTION_STRUCT(CaptureSettings);

DOCUMENT(R"(The main parent window of the application.

.. function:: ShortcutCallback()

  Not a member function - the signature for any ``ShortcutCallback`` callbacks.
)");
struct IMainWindow
{
  typedef std::function<void()> ShortcutCallback;

  DOCUMENT(
      "Retrieves the QWidget for this :class:`MainWindow` if PySide2 is available, or otherwise a "
      "unique opaque pointer that can be passed to RenderDoc functions expecting a QWidget.");
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
:rtype: ``str``
)");
  virtual void RegisterShortcut(const QString &shortcut, QWidget *widget,
                                ShortcutCallback callback) = 0;

protected:
  IMainWindow() = default;
  ~IMainWindow() = default;
};

DECLARE_REFLECTION_STRUCT(IMainWindow);

DOCUMENT("The event browser window.");
struct IEventBrowser
{
  DOCUMENT(
      "Retrieves the QWidget for this :class:`EventBrowser` if PySide2 is available, or otherwise "
      "unique opaque pointer that can be passed to RenderDoc functions expecting a QWidget.");
  virtual QWidget *Widget() = 0;

  DOCUMENT("Updates the duration column if the selected time unit changes.");
  virtual void UpdateDurationColumn() = 0;

protected:
  IEventBrowser() = default;
  ~IEventBrowser() = default;
};

DECLARE_REFLECTION_STRUCT(IEventBrowser);

DOCUMENT("The API inspector window.");
struct IAPIInspector
{
  DOCUMENT(
      "Retrieves the QWidget for this :class:`APIInspector` if PySide2 is available, or otherwise "
      "unique opaque pointer that can be passed to RenderDoc functions expecting a QWidget.");
  virtual QWidget *Widget() = 0;

  DOCUMENT("Refresh the current API view - useful if callstacks are now available.");
  virtual void Refresh() = 0;

protected:
  IAPIInspector() = default;
  ~IAPIInspector() = default;
};

DECLARE_REFLECTION_STRUCT(IAPIInspector);

DOCUMENT("The pipeline state viewer window.");
struct IPipelineStateViewer
{
  DOCUMENT(
      "Retrieves the QWidget for this :class:`PipelineStateViewer` if PySide2 is available, or "
      "otherwise unique opaque pointer that can be passed to RenderDoc functions expecting a "
      "QWidget.");
  virtual QWidget *Widget() = 0;

  DOCUMENT(R"(Prompt the user to save the binary form of the given shader to disk.

:param ~renderdoc.ShaderReflection shader: The shader reflection data to save.
)");
  virtual bool SaveShaderFile(const ShaderReflection *shader) = 0;

protected:
  IPipelineStateViewer() = default;
  ~IPipelineStateViewer() = default;
};

DECLARE_REFLECTION_STRUCT(IPipelineStateViewer);

DOCUMENT("The texture viewer window.");
struct ITextureViewer
{
  DOCUMENT(
      "Retrieves the QWidget for this :class:`TextureViewer` if PySide2 is available, or "
      "otherwise unique opaque pointer that can be passed to RenderDoc functions expecting a "
      "QWidget.");
  virtual QWidget *Widget() = 0;

  DOCUMENT(R"(Open a texture view, optionally raising this window to the foreground.

:param ~renderdoc.ResourceId ID: The ID of the texture to view.
:param bool focus: ``True`` if the :class:`TextureViewer` should be raised.
)");
  virtual void ViewTexture(ResourceId ID, bool focus) = 0;
  DOCUMENT(R"(Highlights the given pixel location in the current texture.

:param int x: The X co-ordinate.
:param int y: The Y co-ordinate.
)");
  virtual void GotoLocation(int x, int y) = 0;

protected:
  ITextureViewer() = default;
  ~ITextureViewer() = default;
};

DECLARE_REFLECTION_STRUCT(ITextureViewer);

DOCUMENT("The buffer viewer window, either a raw buffer or the geometry pipeline.");
struct IBufferViewer
{
  DOCUMENT(
      "Retrieves the QWidget for this :class:`BufferViewer` if PySide2 is available, or otherwise "
      "unique opaque pointer that can be passed to RenderDoc functions expecting a QWidget.");
  virtual QWidget *Widget() = 0;

  DOCUMENT(R"(Scroll to the given row in the given stage's data.

:param int row: the row to scroll to.
:param ~renderdoc.MeshDataStage stage: The stage of the geometry pipeline to scroll within.
)");
  virtual void ScrollToRow(int row, MeshDataStage stage = MeshDataStage::VSIn) = 0;

  DOCUMENT(R"(In a raw buffer viewer, load the contents from a particular buffer resource.

:param int byteOffset: The offset in bytes to the start of the data.
:param int byteSize: The number of bytes to read out.
:param ~renderdoc.ResourceId id: The ID of the buffer itself.
:param str format: Optionally a HLSL/GLSL style formatting string.
)");
  virtual void ViewBuffer(uint64_t byteOffset, uint64_t byteSize, ResourceId id,
                          const QString &format = QString()) = 0;
  DOCUMENT(R"(In a raw buffer viewer, load the contents from a particular texture resource.

:param int arrayIdx: The array slice to load from.
:param int mip: The mip level to load from.
:param ~renderdoc.ResourceId id: The ID of the texture itself.
:param str format: Optionally a HLSL/GLSL style formatting string.
)");
  virtual void ViewTexture(uint32_t arrayIdx, uint32_t mip, ResourceId id,
                           const QString &format = QString()) = 0;

protected:
  IBufferViewer() = default;
  ~IBufferViewer() = default;
};

DECLARE_REFLECTION_STRUCT(IBufferViewer);

DOCUMENT("The executable capture window.");
struct ICaptureDialog
{
  DOCUMENT(
      "Retrieves the QWidget for this :class:`CaptureDialog` if PySide2 is available, or "
      "otherwise unique opaque pointer that can be passed to RenderDoc functions expecting a "
      "QWidget.");
  virtual QWidget *Widget() = 0;

  DOCUMENT(R"(Determines if the window is in inject or launch mode.

:return: ``True`` if the window is set up for injecting.
:rtype: ``bool``
)");
  virtual bool IsInjectMode() = 0;
  DOCUMENT(R"(Switches the window to or from inject mode.

:param bool inject: ``True`` if the window should configure for injecting into processes.
)");
  virtual void SetInjectMode(bool inject) = 0;

  DOCUMENT(R"(Sets the executable filename to capture.

:param str filename: The filename to execute.
)");
  virtual void SetExecutableFilename(const QString &filename) = 0;

  DOCUMENT(R"(Sets the working directory for capture.

:param str dir: The directory to use.
)");
  virtual void SetWorkingDirectory(const QString &dir) = 0;

  DOCUMENT(R"(Sets the command line string to use when launching an executable.

:param str cmd: The command line to use.
)");
  virtual void SetCommandLine(const QString &cmd) = 0;

  DOCUMENT(R"(Sets the list of environment modifications to apply when launching.

:param list modifications: The list of :class:`~renderdoc.EnvironmentModification` to apply.
)");
  virtual void SetEnvironmentModifications(const QList<EnvironmentModification> &modifications) = 0;

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
  virtual void LoadSettings(QString filename) = 0;

  DOCUMENT(R"(Saves the current settings to a file. See :meth:`Settings`.

:param str filename: The filename to save the settings to.
)");
  virtual void SaveSettings(QString filename) = 0;

  DOCUMENT("Update the current state of the global hook, e.g. if it has been enabled.");
  virtual void UpdateGlobalHook() = 0;

protected:
  ICaptureDialog() = default;
  ~ICaptureDialog() = default;
};

DECLARE_REFLECTION_STRUCT(ICaptureDialog);

DOCUMENT("The debug warnings and errors window.");
struct IDebugMessageView
{
  DOCUMENT(
      "Retrieves the QWidget for this :class:`DebugMessageView` if PySide2 is available, or "
      "otherwise unique opaque pointer that can be passed to RenderDoc functions expecting a "
      "QWidget.");
  virtual QWidget *Widget() = 0;

protected:
  IDebugMessageView() = default;
  ~IDebugMessageView() = default;
};

DECLARE_REFLECTION_STRUCT(IDebugMessageView);

DOCUMENT("The statistics window.");
struct IStatisticsViewer
{
  DOCUMENT(
      "Retrieves the QWidget for this :class:`StatisticsViewer` if PySide2 is available, or "
      "otherwise unique opaque pointer that can be passed to RenderDoc functions expecting a "
      "QWidget.");
  virtual QWidget *Widget() = 0;

protected:
  IStatisticsViewer() = default;
  ~IStatisticsViewer() = default;
};

DOCUMENT("The timeline bar.");
struct ITimelineBar
{
  DOCUMENT(
      "Retrieves the QWidget for this :class:`TimelineBar` if PySide2 is available, or otherwise "
      "unique opaque pointer that can be passed to RenderDoc functions expecting a QWidget.");
  virtual QWidget *Widget() = 0;

  DOCUMENT(R"(Highlights the frame usage of the specified resource.

:param ~renderdoc.ResourceId id: The ID of the resource to highlight.
)");
  virtual void HighlightResourceUsage(ResourceId id) = 0;

  DOCUMENT(R"(Highlights the modifications in a frame of a given resource.

:param ~renderdoc.ResourceId id: The ID of the resource that is being modified.
:param list history: A list of :class:`~renderdoc.PixelModification` events to display.
)");
  virtual void HighlightHistory(ResourceId id, const QList<PixelModification> &history) = 0;

protected:
  ITimelineBar() = default;
  ~ITimelineBar() = default;
};

DECLARE_REFLECTION_STRUCT(IStatisticsViewer);

DOCUMENT("The interactive python shell.");
struct IPythonShell
{
  DOCUMENT(
      "Retrieves the QWidget for this :class:`PythonShell` if PySide2 is available, or otherwise "
      "unique opaque pointer that can be passed to RenderDoc functions expecting a QWidget.");
  virtual QWidget *Widget() = 0;

protected:
  IPythonShell() = default;
  ~IPythonShell() = default;
};

DECLARE_REFLECTION_STRUCT(IPythonShell);

DOCUMENT(R"(A shader window used for viewing, editing, or debugging.

.. function:: SaveCallback(context, viewer, files)

  Not a member function - the signature for any ``SaveCallback`` callbacks.

  Called whenever a shader viewer that was open for editing triggers a save/update.

  :param CaptureContext context: The current capture context.
  :param ShaderViewer viewer: The open shader viewer.
  :param dict files: A dictionary with ``str`` filename keys and ``str`` file contents values.

.. function:: CloseCallback(context)

  Not a member function - the signature for any ``CloseCallback`` callbacks.

  Called whenever a shader viewer that was open for editing is closed.

  :param CaptureContext context: The current capture context.
)");
struct IShaderViewer
{
  typedef std::function<void(ICaptureContext *ctx, IShaderViewer *, const QStringMap &)> SaveCallback;
  typedef std::function<void(ICaptureContext *ctx)> CloseCallback;

  DOCUMENT(
      "Retrieves the QWidget for this :class:`ShaderViewer` if PySide2 is available, or otherwise "
      "unique opaque pointer that can be passed to RenderDoc functions expecting a QWidget.");
  virtual QWidget *Widget() = 0;

  DOCUMENT(R"(Retrieves the current step in the debugging.

:return: The current step.
:rtype: ``int``
)");
  virtual int CurrentStep() = 0;

  DOCUMENT(R"(Sets the current step in the debugging.

:param int step: The current step to jump to.
)");
  virtual void SetCurrentStep(int step) = 0;

  DOCUMENT(R"(Toggles a breakpoint at a given instruction.

:param int instruction: The instruction to toggle breakpoint at. If this is ``-1`` the nearest
  instruction after the current caret position is used.
)");
  virtual void ToggleBreakpoint(int instruction = -1) = 0;

  DOCUMENT(R"(Show a list of shader compilation errors or warnings.

:param str errors: The string of errors or warnings to display.
)");
  virtual void ShowErrors(const QString &errors) = 0;

protected:
  IShaderViewer() = default;
  ~IShaderViewer() = default;
};

DECLARE_REFLECTION_STRUCT(IShaderViewer);

DOCUMENT("A constant buffer preview window.");
struct IConstantBufferPreviewer
{
  DOCUMENT(
      "Retrieves the QWidget for this :class:`ConstantBufferPreviewer` if PySide2 is available, or "
      "otherwise unique opaque pointer that can be passed to RenderDoc functions expecting a "
      "QWidget.");
  virtual QWidget *Widget() = 0;

protected:
  IConstantBufferPreviewer() = default;
  ~IConstantBufferPreviewer() = default;
};

DECLARE_REFLECTION_STRUCT(IConstantBufferPreviewer);

DOCUMENT("A pixel history window.");
struct IPixelHistoryView
{
  DOCUMENT(
      "Retrieves the QWidget for this :class:`PixelHistoryView` if PySide2 is available, or "
      "otherwise unique opaque pointer that can be passed to RenderDoc functions expecting a "
      "QWidget.");
  virtual QWidget *Widget() = 0;

  DOCUMENT(R"(Set the history displayed in this window.

:param list history: A list of :class:`~renderdoc.PixelModification` events to display.
)");
  virtual void SetHistory(const rdctype::array<PixelModification> &history) = 0;

protected:
  IPixelHistoryView() = default;
  ~IPixelHistoryView() = default;
};

DECLARE_REFLECTION_STRUCT(IPixelHistoryView);

DOCUMENT("An interface implemented by any object wanting to be notified of capture events.");
struct ILogViewer
{
  DOCUMENT("Called whenever a capture is opened.");
  virtual void OnLogfileLoaded() = 0;

  DOCUMENT("Called whenever a capture is closed.");
  virtual void OnLogfileClosed() = 0;

  DOCUMENT(R"(Called whenever the current selected event changes. This is distinct from the actual
effective current event, since for example selecting a marker region will change the current event
to be the last event inside that region, to be consistent with selecting an item reflecting the
current state after that item.

The selected event shows the :data:`EID <renderdoc.APIEvent.eventID>` that was actually selected,
which will usually but not always be the same as the current effective
:data:`EID <renderdoc.APIEvent.eventID>`.

The distinction for this callback is not normally desired, instead use :meth:`OnEventChanged` to
be notified whenever the current event changes. The API inspector uses this to display API events up
to a marker region.

:param int eventID: The new :data:`EID <renderdoc.APIEvent.eventID>`.
)");
  virtual void OnSelectedEventChanged(uint32_t eventID) = 0;

  DOCUMENT(R"(Called whenever the effective current event changes.

:param int eventID: The new :data:`EID <renderdoc.APIEvent.eventID>`.
)");
  virtual void OnEventChanged(uint32_t eventID) = 0;

protected:
  ILogViewer() = default;
  ~ILogViewer() = default;
};

DECLARE_REFLECTION_STRUCT(ILogViewer);

DOCUMENT(R"(A manager for accessing the underlying replay information that isn't already abstracted
in UI side structures. This manager controls and serialises access to the underlying
:class:`~renderdoc.ReplayController`, as well as handling remote server connections.

.. function:: InvokeCallback(controller)

  Not a member function - the signature for any ``InvokeCallback`` callbacks.

  :param ~renderdoc.ReplayController controller: The controller to access. Must not be cached or
    used after the callback returns.

.. function:: DirectoryBrowseCallback(path, entries)

  Not a member function - the signature for any ``DirectoryBrowseCallback`` callbacks.

  :param str path: The path that was queried for.
  :param list entries: The :class:`~renderdoc.PathEntry` entries underneath the path, as relevant.
)");
struct IReplayManager
{
  typedef std::function<void(IReplayController *)> InvokeCallback;
  typedef std::function<void(const rdctype::str &, const rdctype::array<PathEntry> &)> DirectoryBrowseCallback;

  DOCUMENT(R"(Delete a capture file, whether local or remote.

:param str logfile: The path to the file.
:param bool local: ``True`` if the file is on the local machine.
)");
  virtual void DeleteCapture(const QString &logfile, bool local) = 0;

  DOCUMENT(R"(Connect to a remote server.

:param RemoteHost host: The host to connect to.
:return: Whether or not the connection was successful.
:rtype: ~renderdoc.ReplayStatus
)");
  virtual ReplayStatus ConnectToRemoteServer(RemoteHost *host) = 0;

  DOCUMENT("Disconnect from the server the manager is currently connected to.");
  virtual void DisconnectFromRemoteServer() = 0;

  DOCUMENT("Shutdown the server the manager is currently connected to.");
  virtual void ShutdownServer() = 0;

  DOCUMENT("Ping the remote server to ensure the connection is still alive.");
  virtual void PingRemote() = 0;

  DOCUMENT(R"(Retrieves the host that the manager is currently connected to.

:return: The host connected to, or ``None`` if no connection is active.
:rtype: RemoteHost
)");
  virtual const RemoteHost *CurrentRemote() = 0;

  DOCUMENT(R"(Launch an application and inject into it to allow capturing.

This happens either locally, or on the remote server, depending on whether a connection is active.

:param str exe: The path to the executable to run.
:param str workingDir: The working directory to use when running the application. If blank, the
  directory containing the executable is used.
:param str cmdLine: The command line to use when running the executable, it will be processed in a
  platform specific way to generate arguments.
:param list env: Any :class:`EnvironmentModification` that should be made when running the program.
:param str logfile: The location to save any captures, if running locally.
:param CaptureOptions opts: The capture options to use when injecting into the program.
:return: The ident where the new application is listening for target control, or 0 if something went
  wrong.
:rtype: ``int``
)");
  virtual uint32_t ExecuteAndInject(const QString &exe, const QString &workingDir,
                                    const QString &cmdLine, const QList<EnvironmentModification> &env,
                                    const QString &logfile, CaptureOptions opts) = 0;

  DOCUMENT(R"(Retrieve a list of drivers that the current remote server supports.

:return: The list of supported replay drivers.
:rtype: ``list`` of ``str``.
)");
  virtual QStringList GetRemoteSupport() = 0;

  DOCUMENT(R"(Query the remote host for its home directory.

If a capture is open, the callback will happen on the replay thread, otherwise it will happen in a
blocking fashion on the current thread.

:param bool synchronous: If a capture is open, then ``True`` will use :meth:`BlockInvoke` to call
  the callback. Otherwise if ``False`` then :meth:`AsyncInvoke` will be used.
:param DirectoryBrowseMethod method: The function to callback on the replay thread.
)");
  virtual void GetHomeFolder(bool synchronous, DirectoryBrowseCallback cb) = 0;

  DOCUMENT(R"(Query the remote host for the contents of a path.

If a capture is open, the callback will happen on the replay thread, otherwise it will happen in a
blocking fashion on the current thread.

:param str path: The path to query the contents of.
:param bool synchronous: If a capture is open, then ``True`` will use :meth:`BlockInvoke` to call
  the callback. Otherwise if ``False`` then :meth:`AsyncInvoke` will be used.
:param DirectoryBrowseMethod method: The function to callback on the replay thread.
)");
  virtual void ListFolder(QString path, bool synchronous, DirectoryBrowseCallback cb) = 0;

  DOCUMENT(R"(Copy a capture from the local machine to the remote host.

:param str localpath: The path on the local machine to copy from.
:return: The path on the local machine where the file was saved, or empty if something went wrong.
:param QWidget window: A handle to the window to use when showing a progress bar.
:rtype: ``str``
)");
  virtual QString CopyCaptureToRemote(const QString &localpath, QWidget *window) = 0;

  DOCUMENT(R"(Copy a capture from the remote host to the local machine.

:param str remotepath: The path on the remote server to copy from.
:param str localpath: The path on the local machine to copy to.
:param QWidget window: A handle to the window to use when showing a progress bar.
)");
  virtual void CopyCaptureFromRemote(const QString &remotepath, const QString &localpath,
                                     QWidget *window) = 0;

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
  virtual void AsyncInvoke(const QString &tag, InvokeCallback method) = 0;

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

.. data:: ConstantBufferArea

  The new dock window is docked with other constant buffer views, if they exist, or to the right
  of the existing window if there are none open.
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
  ConstantBufferArea,
};

DOCUMENT("The capture context that the python script is running in.")
struct ICaptureContext
{
  DOCUMENT(R"(Retrieve the absolute path where a given file can be stored with other application
data.

:param str filename: The base filename.
:return: The absolute path.
:rtype: ``str``
)");
  virtual QString ConfigFilePath(const QString &filename) = 0;

  DOCUMENT(R"(Retrieve the absolute path where a given temporary capture should be stored.
data.

:param str appname: The name of the application to use as part of the template.
:return: The absolute path.
:rtype: ``str``
)");
  virtual QString TempLogFilename(QString appname) = 0;

  DOCUMENT(R"(Open a capture file for replay.

:param str logFile: The actual path to the capture file.
:param str origFilename: The original filename, if the capture was copied remotely for replay.
:param bool temporary: ``True`` if this is a temporary capture which should prompt the user for
  either save or delete on close.
:param bool local: ``True`` if ``logFile`` refers to a file on the local machine.
)");
  virtual void LoadLogfile(const QString &logFile, const QString &origFilename, bool temporary,
                           bool local) = 0;

  DOCUMENT("Close the currently open capture file.");
  virtual void CloseLogfile() = 0;

  DOCUMENT(R"(Move the current replay to a new event in the capture.

:param list exclude: A list of :class:`LogViewer` to exclude from being notified of this, to stop
  infinite recursion.
:param int selectedEventID: The selected :data:`EID <renderdoc.APIEvent.eventID>`. See
  :meth:`LogViewer.OnSelectedEventChanged` for more information.
:param int eventID: The new current :data:`EID <renderdoc.APIEvent.eventID>`. See
  :meth:`LogViewer.OnEventChanged` for more information.
:param bool force: Optional parameter, if ``True`` then the replay will 'move' even if it is moving
  to the same :data:`EID <renderdoc.APIEvent.eventID>` as it's currently on.
)");
  virtual void SetEventID(const QVector<ILogViewer *> &exclude, uint32_t selectedEventID,
                          uint32_t eventID, bool force = false) = 0;
  DOCUMENT(R"(Replay the capture to the current event again, to pick up any changes that might have
been made.
)");
  virtual void RefreshStatus() = 0;

  DOCUMENT(R"(Register a new instance of :class:`LogViewer` to receive capture event notifications.

:param LogViewer viewer: The viewer to register.
)");
  virtual void AddLogViewer(ILogViewer *viewer) = 0;

  DOCUMENT(R"(Unregister an instance of :class:`LogViewer` from receiving notifications.

:param LogViewer viewer: The viewer to unregister.
)");
  virtual void RemoveLogViewer(ILogViewer *viewer) = 0;

  //////////////////////////////////////////////////////////////////////////////
  // Accessors

  DOCUMENT(R"(Retrieve the replay manager for access to the internal RenderDoc replay controller.

:return: The current replay manager.
:rtype: ReplayManager
)");
  virtual IReplayManager &Replay() = 0;

  DOCUMENT(R"(Check whether or not a capture is currently loaded.

:return: ``True`` if a capture is loaded.
:rtype: ``bool``
)");
  virtual bool LogLoaded() = 0;

  DOCUMENT(R"(Check whether or not the current capture is stored locally, or on a remote host.

:return: ``True`` if a capture is local.
:rtype: ``bool``
)");
  virtual bool IsLogLocal() = 0;

  DOCUMENT(R"(Check whether or not a capture is currently loading in-progress.

:return: ``True`` if a capture is currently loading.
:rtype: ``bool``
)");
  virtual bool LogLoading() = 0;

  DOCUMENT(R"(Retrieve the filename for the currently loaded capture.

:return: The filename of the current capture.
:rtype: ``str``
)");
  virtual QString LogFilename() = 0;

  DOCUMENT(R"(Retrieve the :class:`~renderdoc.FrameDescription` for the currently loaded capture.

:return: The frame information.
:rtype: ~renderdoc.FrameDescription
)");
  virtual const FrameDescription &FrameInfo() = 0;

  DOCUMENT(R"(Retrieve the :class:`~renderdoc.APIProperties` for the currently loaded capture.

:return: The API properties.
:rtype: ~renderdoc.APIProperties
)");
  virtual const APIProperties &APIProps() = 0;

  DOCUMENT(R"(Retrieve the currently selected :data:`EID <renderdoc.APIEvent.eventID>`.

In most cases, prefer using :meth:`CurEvent`. See :meth:`LogViewer.OnSelectedEventChanged` for more
information for how this differs.

:return: The current selected event.
:rtype: ``int``
)");
  virtual uint32_t CurSelectedEvent() = 0;

  DOCUMENT(R"(Retrieve the current :data:`EID <renderdoc.APIEvent.eventID>`.

:return: The current event.
:rtype: ``int``
)");
  virtual uint32_t CurEvent() = 0;

  DOCUMENT(R"(Retrieve the currently selected drawcall.

In most cases, prefer using :meth:`CurDrawcall`. See :meth:`LogViewer.OnSelectedEventChanged` for
more information for how this differs.

:return: The currently selected drawcall.
:rtype: ~renderdoc.DrawcallDescription
)");
  virtual const DrawcallDescription *CurSelectedDrawcall() = 0;

  DOCUMENT(R"(Retrieve the current drawcall.

:return: The current drawcall, or ``None`` if no drawcall is selected.
:rtype: ~renderdoc.DrawcallDescription
)");
  virtual const DrawcallDescription *CurDrawcall() = 0;

  DOCUMENT(R"(Retrieve the first drawcall in the capture.

:return: The first drawcall.
:rtype: ~renderdoc.DrawcallDescription
)");
  virtual const DrawcallDescription *GetFirstDrawcall() = 0;

  DOCUMENT(R"(Retrieve the last drawcall in the capture.

:return: The last drawcall.
:rtype: ~renderdoc.DrawcallDescription
)");
  virtual const DrawcallDescription *GetLastDrawcall() = 0;

  DOCUMENT(R"(Retrieve the root list of drawcalls in the current capture.

:return: The root drawcalls.
:rtype: ``list`` of :class:`~renderdoc.DrawcallDescription`
)");
  virtual const rdctype::array<DrawcallDescription> &CurDrawcalls() = 0;

  DOCUMENT(R"(Retrieve the information about a particular texture.

:param ~renderdoc.ResourceId id: The ID of the texture to query about.
:return: The information about a texture, or ``None`` if the ID does not correspond to a texture.
:rtype: ~renderdoc.TextureDescription
)");
  virtual TextureDescription *GetTexture(ResourceId id) = 0;

  DOCUMENT(R"(Retrieve the list of textures in the current capture.

:return: The list of textures.
:rtype: ``list`` of :class:`~renderdoc.TextureDescription`
)");
  virtual const rdctype::array<TextureDescription> &GetTextures() = 0;

  DOCUMENT(R"(Retrieve the information about a particular buffer.

:param ~renderdoc.ResourceId id: The ID of the buffer to query about.
:return: The information about a buffer, or ``None`` if the ID does not correspond to a buffer.
:rtype: ~renderdoc.BufferDescription
)");
  virtual BufferDescription *GetBuffer(ResourceId id) = 0;

  DOCUMENT(R"(Retrieve the list of buffers in the current capture.

:return: The list of buffers.
:rtype: ``list`` of :class:`~renderdoc.BufferDescription`
)");
  virtual const rdctype::array<BufferDescription> &GetBuffers() = 0;

  DOCUMENT(R"(Retrieve the information about a drawcall at a given
:data:`EID <renderdoc.APIEvent.eventID>`.

:param int id: The :data:`EID <renderdoc.APIEvent.eventID>` to query for.
:return: The information about the drawcall, or ``None`` if the
  :data:`EID <renderdoc.APIEvent.eventID>` doesn't correspond to a drawcall.
:rtype: ~renderdoc.BufferDescription
)");
  virtual const DrawcallDescription *GetDrawcall(uint32_t eventID) = 0;

  DOCUMENT(R"(Retrieve the current windowing system in use.

:return: The active windowing system.
:rtype: ~renderdoc.WindowingSystem
)");
  virtual WindowingSystem CurWindowingSystem() = 0;

  DOCUMENT(R"(Create an opaque pointer suitable for passing to
:meth:`~ReplayController.CreateOutput` or other functions that expect windowing data.

.. note::

  This data only stays valid until the next call to FillWindowingData. You should pass it to the
  consuming function immediately.

:param int winId: The window ID as returned from ``QWidget.winId()``.
:return: The windowing data.
:rtype: opaque void * pointer.
)");
  virtual void *FillWindowingData(uintptr_t winId) = 0;

  DOCUMENT(R"(Retrieve the current list of debug messages. This includes messages from the capture
as well as messages generated during replay and analysis.

:return: The debug messages generated to date.
:rtype: ``list`` of :class:`~renderdoc.DebugMessage`
)");
  virtual const QVector<DebugMessage> &DebugMessages() = 0;

  DOCUMENT(R"(Retrieve how many messages in :meth:`DebugMessages` are currently unread.

:return: The number of unread messages.
:rtype: ``int``
)");
  virtual int UnreadMessageCount() = 0;

  DOCUMENT("Mark all messages as read, resets :meth:`UnreadMessageCount` to 0.");
  virtual void MarkMessagesRead() = 0;

  DOCUMENT(R"(Add messages into the list returned by :meth:`DebugMessages`. Initially set to unread.

:param list msgs: A list of :class:`~renderdoc.DebugMessage` to add.
)");
  virtual void AddMessages(const rdctype::array<DebugMessage> &msgs) = 0;

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

  DOCUMENT(R"(Retrieve the current singleton :class:`BufferViewer`.

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

  DOCUMENT(R"(Check if there is a current :class:`EventBrowser` open.

:return: ``True`` if there is a window open.
:rtype: ``bool``
)");
  virtual bool HasEventBrowser() = 0;

  DOCUMENT(R"(Check if there is a current :class:`APIInspector` open.

:return: ``True`` if there is a window open.
:rtype: ``bool``
)");
  virtual bool HasAPIInspector() = 0;

  DOCUMENT(R"(Check if there is a current :class:`TextureViewer` open.

:return: ``True`` if there is a window open.
:rtype: ``bool``
)");
  virtual bool HasTextureViewer() = 0;

  DOCUMENT(R"(Check if there is a current :class:`PipelineViewer` open.

:return: ``True`` if there is a window open.
:rtype: ``bool``
)");
  virtual bool HasPipelineViewer() = 0;

  DOCUMENT(R"(Check if there is a current :class:`MeshPreview` open.

:return: ``True`` if there is a window open.
:rtype: ``bool``
)");
  virtual bool HasMeshPreview() = 0;

  DOCUMENT(R"(Check if there is a current :class:`CaptureDialog` open.

:return: ``True`` if there is a window open.
:rtype: ``bool``
)");
  virtual bool HasCaptureDialog() = 0;

  DOCUMENT(R"(Check if there is a current :class:`DebugMessageView` open.

:return: ``True`` if there is a window open.
:rtype: ``bool``
)");
  virtual bool HasDebugMessageView() = 0;

  DOCUMENT(R"(Check if there is a current :class:`StatisticsViewer` open.

:return: ``True`` if there is a window open.
:rtype: ``bool``
)");
  virtual bool HasStatisticsViewer() = 0;

  DOCUMENT(R"(Check if there is a current :class:`TimelineBar` open.

:return: ``True`` if there is a window open.
:rtype: ``bool``
)");
  virtual bool HasTimelineBar() = 0;

  DOCUMENT(R"(Check if there is a current :class:`PythonShell` open.

:return: ``True`` if there is a window open.
:rtype: ``bool``
)");
  virtual bool HasPythonShell() = 0;

  DOCUMENT("Raise the current :class:`EventBrowser`, showing it in the default place if needed.");
  virtual void ShowEventBrowser() = 0;
  DOCUMENT("Raise the current :class:`APIInspector`, showing it in the default place if needed.");
  virtual void ShowAPIInspector() = 0;
  DOCUMENT("Raise the current :class:`TextureViewer`, showing it in the default place if needed.");
  virtual void ShowTextureViewer() = 0;
  DOCUMENT("Raise the current :class:`MeshPreview`, showing it in the default place if needed.");
  virtual void ShowMeshPreview() = 0;
  DOCUMENT("Raise the current :class:`PipelineViewer`, showing it in the default place if needed.");
  virtual void ShowPipelineViewer() = 0;
  DOCUMENT("Raise the current :class:`CaptureDialog`, showing it in the default place if needed.");
  virtual void ShowCaptureDialog() = 0;
  DOCUMENT(
      "Raise the current :class:`DebugMessageView`, showing it in the default place if needed.");
  virtual void ShowDebugMessageView() = 0;
  DOCUMENT(
      "Raise the current :class:`StatisticsViewer`, showing it in the default place if needed.");
  virtual void ShowStatisticsViewer() = 0;
  DOCUMENT("Raise the current :class:`TimelineBar`, showing it in the default place if needed.");
  virtual void ShowTimelineBar() = 0;
  DOCUMENT("Raise the current :class:`PythonShell`, showing it in the default place if needed.");
  virtual void ShowPythonShell() = 0;

  DOCUMENT(R"(Show a new :class:`ShaderViewer` window, showing an editable view of a given shader.

:param bool customShader: ``True`` if the shader being edited is a custom display shader.
:param str entryPoint: The entry point to be used when compiling the edited shader.
:param dict files: The files stored in a ``dict`` with ``str`` keys as filenames and ``str`` values
  with the file contents.
:param ShaderViewer.SaveCallback saveCallback: The callback function to call when a save/update is
  triggered.
:param ShaderViewer.CloseCallback closeCallback: The callback function to call when the shader
  viewer is closed.
:return: The new :class:`ShaderViewer` window opened but not shown for editing.
:rtype: ShaderViewer
)");
  virtual IShaderViewer *EditShader(bool customShader, const QString &entryPoint,
                                    const QStringMap &files, IShaderViewer::SaveCallback saveCallback,
                                    IShaderViewer::CloseCallback closeCallback) = 0;

  DOCUMENT(R"(Show a new :class:`ShaderViewer` window, showing a read-only view of a debug trace
through the execution of a given shader.

:param ~renderdoc.ShaderBindpointMapping bind: The bindpoint mapping for the shader to view.
:param ~renderdoc.ShaderReflection shader: The reflection data for the shader to view.
:param ~renderdoc.ShaderStage stage: The stage that the shader is bound to.
:param ~renderdoc.ShaderDebugTrace trace: The execution trace of the debugged shader.
:param str debugContext: A human-readable context string describing which invocation of this shader
  was debugged. For example 'Pixel 12,34 at EID 678'.
:return: The new :class:`ShaderViewer` window opened, but not shown.
:rtype: ShaderViewer
)");
  virtual IShaderViewer *DebugShader(const ShaderBindpointMapping *bind,
                                     const ShaderReflection *shader, ShaderStage stage,
                                     ShaderDebugTrace *trace, const QString &debugContext) = 0;

  DOCUMENT(R"(Show a new :class:`ShaderViewer` window, showing a read-only view of a given shader.

:param ~renderdoc.ShaderBindpointMapping bind: The bindpoint mapping for the shader to view.
:param ~renderdoc.ShaderReflection shader: The reflection data for the shader to view.
:param ~renderdoc.ShaderStage stage: The stage that the shader is bound to.
:return: The new :class:`ShaderViewer` window opened, but not shown.
:rtype: ShaderViewer
)");
  virtual IShaderViewer *ViewShader(const ShaderBindpointMapping *bind,
                                    const ShaderReflection *shader, ShaderStage stage) = 0;

  DOCUMENT(R"(Show a new :class:`BufferViewer` window, showing a read-only view of buffer data.

:param int byteOffset: The offset in bytes to the start of the buffer data to show.
:param int byteSize: The number of bytes in the buffer to show.
:param ~renderdoc.ResourceId id: The ID of the buffer to fetch data from.
:param str format: Optionally a HLSL/GLSL style formatting string.
:return: The new :class:`BufferViewer` window opened, but not shown.
:rtype: BufferViewer
)");
  virtual IBufferViewer *ViewBuffer(uint64_t byteOffset, uint64_t byteSize, ResourceId id,
                                    const QString &format = QString()) = 0;

  DOCUMENT(R"(Show a new :class:`BufferViewer` window, showing a read-only view of a texture's raw
bytes.

:param int arrayIdx: The array slice to load from.
:param int mip: The mip level to load from.
:param ~renderdoc.ResourceId id: The ID of the texture itself.
:param str format: Optionally a HLSL/GLSL style formatting string.
:return: The new :class:`BufferViewer` window opened, but not shown.
:rtype: BufferViewer
)");
  virtual IBufferViewer *ViewTextureAsBuffer(uint32_t arrayIdx, uint32_t mip, ResourceId id,
                                             const QString &format = QString()) = 0;

  DOCUMENT(R"(Show a new :class:`ConstantBufferPreviewer` window, showing a read-only view of a the
variables in a constant buffer with their values.

:param ~renderdoc.ShaderStage stage: The stage that the constant buffer is bound to.
:param int slot: The index in the shader's constant buffer list to look up.
:param int idx: For APIs that support arrayed resource binds, the index in the constant buffer
  array.
:return: The new :class:`ConstantBufferPreviewer` window opened, but not shown.
:rtype: ConstantBufferPreviewer
)");
  virtual IConstantBufferPreviewer *ViewConstantBuffer(ShaderStage stage, uint32_t slot,
                                                       uint32_t idx) = 0;

  DOCUMENT(R"(Show a new :class:`PixelHistoryView` window, showing the results from a pixel history
operation.

:param ~renderdoc.ResourceId id: The ID of the texture to show the history of.
:param int X: The x co-ordinate of the pixel to search for.
:param int Y: The y co-ordinate of the pixel to search for.
:param ~renderdoc.TextureDisplay display: The texture display configuration to use when looking up
  the history.
:return: The new :class:`PixelHistoryView` window opened, but not shown.
:rtype: PixelHistoryView
)");
  virtual IPixelHistoryView *ViewPixelHistory(ResourceId texID, int x, int y,
                                              const TextureDisplay &display) = 0;

  DOCUMENT(R"(Creates and returns a built-in window.

This function is intended for internal use for restoring layouts, and generally should not be used
by user code.

:param str objectName: The built-in name of a singleton window.
:return: The handle to the existing or newly created window of this type, or ``None`` if PySide2 is
  not available.
:rtype: ``QWidget``
)");
  virtual QWidget *CreateBuiltinWindow(const QString &objectName) = 0;

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

:param QWidget dockWindow: The new window to add.
:param DockReference ref: The location to add the new window, possibly relative to ``refWindow``.
:param QWidget refWindow: The window to refer to if the new window is being added relative, or can
  be ``None`` if the new location is absolute.
:param float percentage: Optionally the percentage to split the area. If omitted, a 50% split is
  used.
)");
  virtual void AddDockWindow(QWidget *newWindow, DockReference ref, QWidget *refWindow,
                             float percentage = 0.5f) = 0;

  DOCUMENT(R"(Retrieve the current :class:`~renderdoc.D3D11_State` pipeline state.

:return: The current D3D11 pipeline state.
:rtype: ~renderdoc.D3D11_State
)");
  virtual D3D11Pipe::State &CurD3D11PipelineState() = 0;

  DOCUMENT(R"(Retrieve the current :class:`~renderdoc.D3D12_State` pipeline state.

:return: The current D3D12 pipeline state.
:rtype: ~renderdoc.D3D12_State
)");
  virtual D3D12Pipe::State &CurD3D12PipelineState() = 0;

  DOCUMENT(R"(Retrieve the current :class:`~renderdoc.GL_State` pipeline state.

:return: The current OpenGL pipeline state.
:rtype: ~renderdoc.GL_State
)");
  virtual GLPipe::State &CurGLPipelineState() = 0;

  DOCUMENT(R"(Retrieve the current :class:`~renderdoc.VK_State` pipeline state.

:return: The current Vulkan pipeline state.
:rtype: ~renderdoc.VK_State
)");
  virtual VKPipe::State &CurVulkanPipelineState() = 0;

  DOCUMENT(R"(Retrieve the current :class:`CommonPipelineState` abstracted pipeline state.

:return: The current API-agnostic abstracted pipeline state.
:rtype: CommonPipelineState
)");
  virtual CommonPipelineState &CurPipelineState() = 0;

  DOCUMENT(R"(Retrieve the current persistant config.

:return: The current persistant config manager.
:rtype: PersistantConfig
)");
  virtual PersistantConfig &Config() = 0;

protected:
  ICaptureContext() = default;
  ~ICaptureContext() = default;
};

DECLARE_REFLECTION_STRUCT(ICaptureContext);
