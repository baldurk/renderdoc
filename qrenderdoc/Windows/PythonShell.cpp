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

#include "PythonShell.h"
#include <QFontDatabase>
#include <QKeyEvent>
#include <QMenu>
#include <QScrollBar>
#include "Code/QRDUtils.h"
#include "Code/ScintillaSyntax.h"
#include "Code/pyrenderdoc/PythonContext.h"
#include "scintilla/include/SciLexer.h"
#include "scintilla/include/qt/ScintillaEdit.h"
#include "ui_PythonShell.h"

// a forwarder that invokes onto the UI thread wherever necessary.
// Note this does NOT make CaptureContext thread safe. We just invoke for any potentially UI
// operations. All invokes are blocking, so there can't be any times when the UI thread waits
// on the python thread.
template <typename Obj>
struct ObjectForwarder : Obj
{
  ObjectForwarder(PythonShell *sh, Obj &o) : m_Shell(sh), m_Obj(o) {}
  PythonShell *m_Shell;
  Obj &m_Obj;

  template <typename F, typename... paramTypes>
  void InvokeVoidFunction(F ptr, paramTypes... params)
  {
    if(!GUIInvoke::onUIThread())
    {
      PythonContext *scriptContext = m_Shell->GetScriptContext();
      if(scriptContext)
        scriptContext->PausePythonThreading();
      GUIInvoke::blockcall(m_Shell, [this, ptr, params...]() { (m_Obj.*ptr)(params...); });
      if(scriptContext)
        scriptContext->ResumePythonThreading();
      return;
    }

    (m_Obj.*ptr)(params...);
  }

  template <typename R, typename F, typename... paramTypes>
  R InvokeRetFunction(F ptr, paramTypes... params)
  {
    if(!GUIInvoke::onUIThread())
    {
      R ret;
      PythonContext *scriptContext = m_Shell->GetScriptContext();
      if(scriptContext)
        scriptContext->PausePythonThreading();
      GUIInvoke::blockcall(m_Shell,
                           [this, &ret, ptr, params...]() { ret = (m_Obj.*ptr)(params...); });
      if(scriptContext)
        scriptContext->ResumePythonThreading();
      return ret;
    }

    return (m_Obj.*ptr)(params...);
  }
};

struct MiniQtInvoker : ObjectForwarder<IMiniQtHelper>
{
  MiniQtInvoker(PythonShell *shell, IMiniQtHelper &obj) : ObjectForwarder(shell, obj) {}
  virtual ~MiniQtInvoker() {}
  void InvokeOntoUIThread(std::function<void()> callback)
  {
    // this function is already thread safe since it's invoking, so just call it directly
    m_Obj.InvokeOntoUIThread(callback);
  }

  ///////////////////////////////////////////////////////////////////////
  // all functions invoke onto the UI thread since they deal with widgets!
  ///////////////////////////////////////////////////////////////////////

  QWidget *CreateToplevelWidget(const rdcstr &windowTitle, WidgetCallback closed)
  {
    return InvokeRetFunction<QWidget *>(&IMiniQtHelper::CreateToplevelWidget, windowTitle, closed);
  }
  void CloseToplevelWidget(QWidget *widget)
  {
    InvokeVoidFunction(&IMiniQtHelper::CloseToplevelWidget, widget);
  }

  // widget hierarchy

  void SetWidgetName(QWidget *widget, const rdcstr &name)
  {
    InvokeVoidFunction(&IMiniQtHelper::SetWidgetName, widget, name);
  }
  rdcstr GetWidgetName(QWidget *widget)
  {
    return InvokeRetFunction<rdcstr>(&IMiniQtHelper::GetWidgetName, widget);
  }
  rdcstr GetWidgetType(QWidget *widget)
  {
    return InvokeRetFunction<rdcstr>(&IMiniQtHelper::GetWidgetType, widget);
  }
  QWidget *FindChildByName(QWidget *parent, const rdcstr &name)
  {
    return InvokeRetFunction<QWidget *>(&IMiniQtHelper::FindChildByName, parent, name);
  }
  QWidget *GetParent(QWidget *widget)
  {
    return InvokeRetFunction<QWidget *>(&IMiniQtHelper::GetParent, widget);
  }
  int32_t GetNumChildren(QWidget *widget)
  {
    return InvokeRetFunction<int32_t>(&IMiniQtHelper::GetNumChildren, widget);
  }
  QWidget *GetChild(QWidget *parent, int32_t index)
  {
    return InvokeRetFunction<QWidget *>(&IMiniQtHelper::GetChild, parent, index);
  }
  void DestroyWidget(QWidget *widget) { InvokeVoidFunction(&IMiniQtHelper::DestroyWidget, widget); }
  // dialogs

  bool ShowWidgetAsDialog(QWidget *widget)
  {
    return InvokeRetFunction<bool>(&IMiniQtHelper::ShowWidgetAsDialog, widget);
  }
  void CloseCurrentDialog(bool success)
  {
    InvokeVoidFunction(&IMiniQtHelper::CloseCurrentDialog, success);
  }

  // layout functions

  QWidget *CreateHorizontalContainer()
  {
    return InvokeRetFunction<QWidget *>(&IMiniQtHelper::CreateHorizontalContainer);
  }
  QWidget *CreateVerticalContainer()
  {
    return InvokeRetFunction<QWidget *>(&IMiniQtHelper::CreateVerticalContainer);
  }
  QWidget *CreateGridContainer()
  {
    return InvokeRetFunction<QWidget *>(&IMiniQtHelper::CreateGridContainer);
  }
  QWidget *CreateSpacer(bool horizontal)
  {
    return InvokeRetFunction<QWidget *>(&IMiniQtHelper::CreateSpacer, horizontal);
  }
  void ClearContainedWidgets(QWidget *parent)
  {
    InvokeVoidFunction(&IMiniQtHelper::ClearContainedWidgets, parent);
  }
  void AddGridWidget(QWidget *parent, int32_t row, int32_t column, QWidget *child, int32_t rowSpan,
                     int32_t columnSpan)
  {
    InvokeVoidFunction(&IMiniQtHelper::AddGridWidget, parent, row, column, child, rowSpan,
                       columnSpan);
  }
  void AddWidget(QWidget *parent, QWidget *child)
  {
    InvokeVoidFunction(&IMiniQtHelper::AddWidget, parent, child);
  }
  void InsertWidget(QWidget *parent, int32_t index, QWidget *child)
  {
    InvokeVoidFunction(&IMiniQtHelper::InsertWidget, parent, index, child);
  }

  // widget manipulation

  void SetWidgetText(QWidget *widget, const rdcstr &text)
  {
    InvokeVoidFunction(&IMiniQtHelper::SetWidgetText, widget, text);
  }
  rdcstr GetWidgetText(QWidget *widget)
  {
    return InvokeRetFunction<rdcstr>(&IMiniQtHelper::GetWidgetText, widget);
  }

  void SetWidgetFont(QWidget *widget, const rdcstr &font, int32_t fontSize, bool bold, bool italic)
  {
    InvokeVoidFunction(&IMiniQtHelper::SetWidgetFont, widget, font, fontSize, bold, italic);
  }

  void SetWidgetEnabled(QWidget *widget, bool enabled)
  {
    InvokeVoidFunction(&IMiniQtHelper::SetWidgetEnabled, widget, enabled);
  }
  bool IsWidgetEnabled(QWidget *widget)
  {
    return InvokeRetFunction<bool>(&IMiniQtHelper::IsWidgetEnabled, widget);
  }
  void SetWidgetVisible(QWidget *widget, bool visible)
  {
    InvokeVoidFunction(&IMiniQtHelper::SetWidgetVisible, widget, visible);
  }
  bool IsWidgetVisible(QWidget *widget)
  {
    return InvokeRetFunction<bool>(&IMiniQtHelper::IsWidgetVisible, widget);
  }

  // specific widgets

  QWidget *CreateGroupBox(bool collapsible)
  {
    return InvokeRetFunction<QWidget *>(&IMiniQtHelper::CreateGroupBox, collapsible);
  }

  QWidget *CreateButton(WidgetCallback pressed)
  {
    return InvokeRetFunction<QWidget *>(&IMiniQtHelper::CreateButton, pressed);
  }

  QWidget *CreateLabel() { return InvokeRetFunction<QWidget *>(&IMiniQtHelper::CreateLabel); }
  void SetLabelImage(QWidget *widget, const bytebuf &data, int32_t width, int32_t height, bool alpha)
  {
    InvokeVoidFunction(&IMiniQtHelper::SetLabelImage, widget, data, width, height, alpha);
  }
  QWidget *CreateOutputRenderingWidget()
  {
    return InvokeRetFunction<QWidget *>(&IMiniQtHelper::CreateOutputRenderingWidget);
  }
  WindowingData GetWidgetWindowingData(QWidget *widget)
  {
    return InvokeRetFunction<WindowingData>(&IMiniQtHelper::GetWidgetWindowingData, widget);
  }

  void SetWidgetReplayOutput(QWidget *widget, IReplayOutput *output)
  {
    InvokeVoidFunction(&IMiniQtHelper::SetWidgetReplayOutput, widget, output);
  }

  void SetWidgetBackgroundColor(QWidget *widget, float red, float green, float blue)
  {
    InvokeVoidFunction(&IMiniQtHelper::SetWidgetBackgroundColor, widget, red, green, blue);
  }
  QWidget *CreateCheckbox(WidgetCallback changed)
  {
    return InvokeRetFunction<QWidget *>(&IMiniQtHelper::CreateCheckbox, changed);
  }
  QWidget *CreateRadiobox(WidgetCallback changed)
  {
    return InvokeRetFunction<QWidget *>(&IMiniQtHelper::CreateRadiobox, changed);
  }

  void SetWidgetChecked(QWidget *checkableWidget, bool checked)
  {
    InvokeVoidFunction(&IMiniQtHelper::SetWidgetChecked, checkableWidget, checked);
  }
  bool IsWidgetChecked(QWidget *checkableWidget)
  {
    return InvokeRetFunction<bool>(&IMiniQtHelper::IsWidgetChecked, checkableWidget);
  }

  QWidget *CreateSpinbox(int32_t decimalPlaces, double step)
  {
    return InvokeRetFunction<QWidget *>(&IMiniQtHelper::CreateSpinbox, decimalPlaces, step);
  }

  void SetSpinboxBounds(QWidget *spinbox, double minVal, double maxVal)
  {
    InvokeVoidFunction(&IMiniQtHelper::SetSpinboxBounds, spinbox, minVal, maxVal);
  }
  void SetSpinboxValue(QWidget *spinbox, double value)
  {
    InvokeVoidFunction(&IMiniQtHelper::SetSpinboxValue, spinbox, value);
  }
  double GetSpinboxValue(QWidget *spinbox)
  {
    return InvokeRetFunction<double>(&IMiniQtHelper::GetSpinboxValue, spinbox);
  }

  QWidget *CreateTextBox(bool singleLine, WidgetCallback changed)
  {
    return InvokeRetFunction<QWidget *>(&IMiniQtHelper::CreateTextBox, singleLine, changed);
  }

  QWidget *CreateComboBox(bool editable, WidgetCallback changed)
  {
    return InvokeRetFunction<QWidget *>(&IMiniQtHelper::CreateComboBox, editable, changed);
  }

  void SetComboOptions(QWidget *combo, const rdcarray<rdcstr> &options)
  {
    InvokeVoidFunction(&IMiniQtHelper::SetComboOptions, combo, options);
  }

  size_t GetComboCount(QWidget *combo)
  {
    return InvokeRetFunction<size_t>(&IMiniQtHelper::GetComboCount, combo);
  }

  void SelectComboOption(QWidget *combo, const rdcstr &option)
  {
    InvokeVoidFunction(&IMiniQtHelper::SelectComboOption, combo, option);
  }

  QWidget *CreateProgressBar(bool horizontal)
  {
    return InvokeRetFunction<QWidget *>(&IMiniQtHelper::CreateProgressBar, horizontal);
  }

  void ResetProgressBar(QWidget *pbar)
  {
    InvokeVoidFunction(&IMiniQtHelper::ResetProgressBar, pbar);
  }

  void SetProgressBarValue(QWidget *pbar, int32_t value)
  {
    InvokeVoidFunction(&IMiniQtHelper::SetProgressBarValue, pbar, value);
  }

  void UpdateProgressBarValue(QWidget *pbar, int32_t delta)
  {
    InvokeVoidFunction(&IMiniQtHelper::UpdateProgressBarValue, pbar, delta);
  }

  int32_t GetProgressBarValue(QWidget *pbar)
  {
    return InvokeRetFunction<int>(&IMiniQtHelper::GetProgressBarValue, pbar);
  }

  void SetProgressBarRange(QWidget *pbar, int32_t minimum, int32_t maximum)
  {
    InvokeVoidFunction(&IMiniQtHelper::SetProgressBarRange, pbar, minimum, maximum);
  }

  int32_t GetProgressBarMinimum(QWidget *pbar)
  {
    return InvokeRetFunction<int>(&IMiniQtHelper::GetProgressBarMinimum, pbar);
  }

  int32_t GetProgressBarMaximum(QWidget *pbar)
  {
    return InvokeRetFunction<int>(&IMiniQtHelper::GetProgressBarMaximum, pbar);
  }
};

struct ExtensionInvoker : ObjectForwarder<IExtensionManager>
{
  MiniQtInvoker *m_MiniQt;
  ExtensionInvoker(PythonShell *shell, IExtensionManager &obj) : ObjectForwarder(shell, obj)
  {
    m_MiniQt = new MiniQtInvoker(shell, obj.GetMiniQtHelper());
  }
  virtual ~ExtensionInvoker() { delete m_MiniQt; }
  //
  ///////////////////////////////////////////////////////////////////////
  // pass-through functions that don't need the UI thread
  ///////////////////////////////////////////////////////////////////////
  //
  rdcarray<ExtensionMetadata> GetInstalledExtensions() { return m_Obj.GetInstalledExtensions(); }
  bool IsExtensionLoaded(rdcstr name) { return m_Obj.IsExtensionLoaded(name); }
  rdcstr LoadExtension(rdcstr name) { return m_Obj.LoadExtension(name); }
  IMiniQtHelper &GetMiniQtHelper() { return *m_MiniQt; }
  //
  ///////////////////////////////////////////////////////////////////////
  // functions that invoke onto the UI thread
  ///////////////////////////////////////////////////////////////////////
  //
  void RegisterWindowMenu(WindowMenu base, const rdcarray<rdcstr> &submenus,
                          ExtensionCallback callback)
  {
    InvokeVoidFunction(&IExtensionManager::RegisterWindowMenu, base, submenus, callback);
  }

  void RegisterPanelMenu(PanelMenu base, const rdcarray<rdcstr> &submenus, ExtensionCallback callback)
  {
    InvokeVoidFunction(&IExtensionManager::RegisterPanelMenu, base, submenus, callback);
  }

  void RegisterContextMenu(ContextMenu base, const rdcarray<rdcstr> &submenus,
                           ExtensionCallback callback)
  {
    InvokeVoidFunction(&IExtensionManager::RegisterContextMenu, base, submenus, callback);
  }

  void MessageDialog(const rdcstr &text, const rdcstr &title)
  {
    InvokeVoidFunction(&IExtensionManager::MessageDialog, text, title);
  }

  void ErrorDialog(const rdcstr &text, const rdcstr &title)
  {
    InvokeVoidFunction(&IExtensionManager::ErrorDialog, text, title);
  }

  DialogButton QuestionDialog(const rdcstr &text, const rdcarray<DialogButton> &options,
                              const rdcstr &title)
  {
    return InvokeRetFunction<DialogButton>(&IExtensionManager::QuestionDialog, text, options, title);
  }

  rdcstr OpenFileName(const rdcstr &caption, const rdcstr &dir, const rdcstr &filter)
  {
    return InvokeRetFunction<rdcstr>(&IExtensionManager::OpenFileName, caption, dir, filter);
  }

  rdcstr OpenDirectoryName(const rdcstr &caption, const rdcstr &dir)
  {
    return InvokeRetFunction<rdcstr>(&IExtensionManager::OpenDirectoryName, caption, dir);
  }

  rdcstr SaveFileName(const rdcstr &caption, const rdcstr &dir, const rdcstr &filter)
  {
    return InvokeRetFunction<rdcstr>(&IExtensionManager::SaveFileName, caption, dir, filter);
  }

  void MenuDisplaying(ContextMenu contextMenu, QMenu *menu, const ExtensionCallbackData &data)
  {
    InvokeVoidFunction(
        (void(IExtensionManager::*)(ContextMenu, QMenu *, const ExtensionCallbackData &)) &
            IExtensionManager::MenuDisplaying,
        contextMenu, menu, data);
  }
  void MenuDisplaying(PanelMenu panelMenu, QMenu *menu, QWidget *extensionButton,
                      const ExtensionCallbackData &data)
  {
    InvokeVoidFunction(
        (void(IExtensionManager::*)(PanelMenu, QMenu *, QWidget *, const ExtensionCallbackData &)) &
            IExtensionManager::MenuDisplaying,
        panelMenu, menu, extensionButton, data);
  }
};

struct CaptureContextInvoker : ObjectForwarder<ICaptureContext>
{
  ExtensionInvoker *m_Ext;
  CaptureContextInvoker(PythonShell *shell, ICaptureContext &obj) : ObjectForwarder(shell, obj)
  {
    m_Ext = new ExtensionInvoker(shell, obj.Extensions());
  }
  virtual ~CaptureContextInvoker() { delete m_Ext; }
  //
  ///////////////////////////////////////////////////////////////////////
  // pass-through functions that don't need the UI thread
  ///////////////////////////////////////////////////////////////////////
  //
  virtual rdcstr TempCaptureFilename(const rdcstr &appname) override
  {
    return m_Obj.TempCaptureFilename(appname);
  }
  virtual IExtensionManager &Extensions() override { return *m_Ext; }
  virtual IReplayManager &Replay() override { return m_Obj.Replay(); }
  virtual bool IsCaptureLoaded() override { return m_Obj.IsCaptureLoaded(); }
  virtual bool IsCaptureLocal() override { return m_Obj.IsCaptureLocal(); }
  virtual bool IsCaptureTemporary() override { return m_Obj.IsCaptureTemporary(); }
  virtual bool IsCaptureLoading() override { return m_Obj.IsCaptureLoading(); }
  virtual ResultDetails GetFatalError() override { return m_Obj.GetFatalError(); }
  virtual rdcstr GetCaptureFilename() override { return m_Obj.GetCaptureFilename(); }
  virtual CaptureModifications GetCaptureModifications() override
  {
    return m_Obj.GetCaptureModifications();
  }
  virtual const FrameDescription &FrameInfo() override { return m_Obj.FrameInfo(); }
  virtual const APIProperties &APIProps() override { return m_Obj.APIProps(); }
  virtual rdcarray<ShaderEncoding> TargetShaderEncodings() override
  {
    return m_Obj.TargetShaderEncodings();
  }
  virtual rdcarray<ShaderEncoding> CustomShaderEncodings() override
  {
    return m_Obj.CustomShaderEncodings();
  }
  virtual rdcarray<ShaderSourcePrefix> CustomShaderSourcePrefixes() override
  {
    return m_Obj.CustomShaderSourcePrefixes();
  }
  virtual uint32_t CurSelectedEvent() override { return m_Obj.CurSelectedEvent(); }
  virtual uint32_t CurEvent() override { return m_Obj.CurEvent(); }
  virtual const ActionDescription *CurSelectedAction() override
  {
    return m_Obj.CurSelectedAction();
  }
  virtual const ActionDescription *CurAction() override { return m_Obj.CurAction(); }
  virtual const ActionDescription *GetFirstAction() override { return m_Obj.GetFirstAction(); }
  virtual const ActionDescription *GetLastAction() override { return m_Obj.GetLastAction(); }
  virtual const rdcarray<ActionDescription> &CurRootActions() override
  {
    return m_Obj.CurRootActions();
  }
  virtual const ResourceDescription *GetResource(ResourceId id) const override
  {
    return m_Obj.GetResource(id);
  }
  virtual const rdcarray<ResourceDescription> &GetResources() override
  {
    return m_Obj.GetResources();
  }
  virtual rdcstr GetResourceName(ResourceId id) const override { return m_Obj.GetResourceName(id); }
  virtual rdcstr GetResourceNameUnsuffixed(ResourceId id) const override
  {
    return m_Obj.GetResourceNameUnsuffixed(id);
  }
  virtual bool IsAutogeneratedName(ResourceId id) override { return m_Obj.IsAutogeneratedName(id); }
  virtual bool HasResourceCustomName(ResourceId id) override
  {
    return m_Obj.HasResourceCustomName(id);
  }
  virtual int32_t ResourceNameCacheID() const override { return m_Obj.ResourceNameCacheID(); }
  virtual TextureDescription *GetTexture(ResourceId id) override { return m_Obj.GetTexture(id); }
  virtual const rdcarray<TextureDescription> &GetTextures() override { return m_Obj.GetTextures(); }
  virtual BufferDescription *GetBuffer(ResourceId id) override { return m_Obj.GetBuffer(id); }
  virtual DescriptorStoreDescription *GetDescriptorStore(ResourceId id) override
  {
    return m_Obj.GetDescriptorStore(id);
  }
  virtual const rdcarray<BufferDescription> &GetBuffers() const override
  {
    return m_Obj.GetBuffers();
  }
  virtual const ActionDescription *GetAction(uint32_t eventId) override
  {
    return m_Obj.GetAction(eventId);
  }
  virtual bool OpenRGPProfile(const rdcstr &filename) override
  {
    return m_Obj.OpenRGPProfile(filename);
  }
  virtual IRGPInterop *GetRGPInterop() override { return m_Obj.GetRGPInterop(); }
  virtual const SDFile &GetStructuredFile() override { return m_Obj.GetStructuredFile(); }
  virtual WindowingSystem CurWindowingSystem() override { return m_Obj.CurWindowingSystem(); }
  virtual const rdcarray<DebugMessage> &DebugMessages() override { return m_Obj.DebugMessages(); }
  virtual int32_t UnreadMessageCount() override { return m_Obj.UnreadMessageCount(); }
  virtual void MarkMessagesRead() override { return m_Obj.MarkMessagesRead(); }
  virtual rdcstr GetNotes(const rdcstr &key) override { return m_Obj.GetNotes(key); }
  virtual rdcarray<EventBookmark> GetBookmarks() override { return m_Obj.GetBookmarks(); }
  virtual const D3D11Pipe::State *CurD3D11PipelineState() override
  {
    return m_Obj.CurD3D11PipelineState();
  }
  virtual const D3D12Pipe::State *CurD3D12PipelineState() override
  {
    return m_Obj.CurD3D12PipelineState();
  }
  virtual const GLPipe::State *CurGLPipelineState() override { return m_Obj.CurGLPipelineState(); }
  virtual const VKPipe::State *CurVulkanPipelineState() override
  {
    return m_Obj.CurVulkanPipelineState();
  }
  virtual const PipeState &CurPipelineState() override { return m_Obj.CurPipelineState(); }
  virtual PersistantConfig &Config() override { return m_Obj.Config(); }
  //
  ///////////////////////////////////////////////////////////////////////
  // functions that invoke onto the UI thread
  ///////////////////////////////////////////////////////////////////////
  //
  virtual void ConnectToRemoteServer(RemoteHost host) override
  {
    InvokeVoidFunction(&ICaptureContext::ConnectToRemoteServer, host);
  }
  virtual WindowingData CreateWindowingData(QWidget *window) override
  {
    return InvokeRetFunction<WindowingData>(&ICaptureContext::CreateWindowingData, window);
  }
  virtual void LoadCapture(const rdcstr &capture, const ReplayOptions &opts,
                           const rdcstr &origFilename, bool temporary, bool local) override
  {
    InvokeVoidFunction(&ICaptureContext::LoadCapture, capture, opts, origFilename, temporary, local);
  }
  virtual bool SaveCaptureTo(const rdcstr &capture) override
  {
    return InvokeRetFunction<bool>(&ICaptureContext::SaveCaptureTo, capture);
  }
  virtual void RecompressCapture() override
  {
    InvokeVoidFunction(&ICaptureContext::RecompressCapture);
  }
  virtual void CloseCapture() override { InvokeVoidFunction(&ICaptureContext::CloseCapture); }
  virtual bool ImportCapture(const CaptureFileFormat &fmt, const rdcstr &importfile,
                             const rdcstr &rdcfile) override
  {
    return InvokeRetFunction<bool>(&ICaptureContext::ImportCapture, fmt, importfile, rdcfile);
  }
  virtual void ExportCapture(const CaptureFileFormat &fmt, const rdcstr &exportfile) override
  {
    InvokeVoidFunction(&ICaptureContext::ExportCapture, fmt, exportfile);
  }
  virtual void SetEventID(const rdcarray<ICaptureViewer *> &exclude, uint32_t selectedEventID,
                          uint32_t eventId, bool force = false) override
  {
    InvokeVoidFunction(&ICaptureContext::SetEventID, exclude, selectedEventID, eventId, force);
  }
  virtual void RefreshStatus() override { InvokeVoidFunction(&ICaptureContext::RefreshStatus); }
  virtual bool IsResourceReplaced(ResourceId id) override
  {
    return InvokeRetFunction<bool>(&ICaptureContext::IsResourceReplaced, id);
  }
  virtual ResourceId GetResourceReplacement(ResourceId id) override
  {
    return InvokeRetFunction<ResourceId>(&ICaptureContext::GetResourceReplacement, id);
  }
  virtual void RegisterReplacement(ResourceId from, ResourceId to) override
  {
    InvokeVoidFunction(&ICaptureContext::RegisterReplacement, from, to);
  }
  virtual void UnregisterReplacement(ResourceId id) override
  {
    InvokeVoidFunction(&ICaptureContext::UnregisterReplacement, id);
  }
  virtual void AddCaptureViewer(ICaptureViewer *viewer) override
  {
    InvokeVoidFunction(&ICaptureContext::AddCaptureViewer, viewer);
  }
  virtual void RemoveCaptureViewer(ICaptureViewer *viewer) override
  {
    InvokeVoidFunction(&ICaptureContext::RemoveCaptureViewer, viewer);
  }
  virtual void AddMessages(const rdcarray<DebugMessage> &msgs) override
  {
    InvokeVoidFunction(&ICaptureContext::AddMessages, msgs);
  }
  virtual void SetResourceCustomName(ResourceId id, const rdcstr &name) override
  {
    InvokeVoidFunction(&ICaptureContext::SetResourceCustomName, id, name);
  }
  virtual void SetNotes(const rdcstr &key, const rdcstr &contents) override
  {
    InvokeVoidFunction(&ICaptureContext::SetNotes, key, contents);
  }

  virtual void SetBookmark(const EventBookmark &mark) override
  {
    InvokeVoidFunction(&ICaptureContext::SetBookmark, mark);
  }
  virtual void RemoveBookmark(uint32_t EID) override
  {
    InvokeVoidFunction(&ICaptureContext::RemoveBookmark, EID);
  }
  virtual IMainWindow *GetMainWindow() override
  {
    return InvokeRetFunction<IMainWindow *>(&ICaptureContext::GetMainWindow);
  }
  virtual IEventBrowser *GetEventBrowser() override
  {
    return InvokeRetFunction<IEventBrowser *>(&ICaptureContext::GetEventBrowser);
  }
  virtual IAPIInspector *GetAPIInspector() override
  {
    return InvokeRetFunction<IAPIInspector *>(&ICaptureContext::GetAPIInspector);
  }
  virtual ITextureViewer *GetTextureViewer() override
  {
    return InvokeRetFunction<ITextureViewer *>(&ICaptureContext::GetTextureViewer);
  }
  virtual IBufferViewer *GetMeshPreview() override
  {
    return InvokeRetFunction<IBufferViewer *>(&ICaptureContext::GetMeshPreview);
  }
  virtual IPipelineStateViewer *GetPipelineViewer() override
  {
    return InvokeRetFunction<IPipelineStateViewer *>(&ICaptureContext::GetPipelineViewer);
  }
  virtual ICaptureDialog *GetCaptureDialog() override
  {
    return InvokeRetFunction<ICaptureDialog *>(&ICaptureContext::GetCaptureDialog);
  }
  virtual IDebugMessageView *GetDebugMessageView() override
  {
    return InvokeRetFunction<IDebugMessageView *>(&ICaptureContext::GetDebugMessageView);
  }
  virtual IDiagnosticLogView *GetDiagnosticLogView() override
  {
    return InvokeRetFunction<IDiagnosticLogView *>(&ICaptureContext::GetDiagnosticLogView);
  }
  virtual ICommentView *GetCommentView() override
  {
    return InvokeRetFunction<ICommentView *>(&ICaptureContext::GetCommentView);
  }
  virtual IPerformanceCounterViewer *GetPerformanceCounterViewer() override
  {
    return InvokeRetFunction<IPerformanceCounterViewer *>(
        &ICaptureContext::GetPerformanceCounterViewer);
  }
  virtual IStatisticsViewer *GetStatisticsViewer() override
  {
    return InvokeRetFunction<IStatisticsViewer *>(&ICaptureContext::GetStatisticsViewer);
  }
  virtual ITimelineBar *GetTimelineBar() override
  {
    return InvokeRetFunction<ITimelineBar *>(&ICaptureContext::GetTimelineBar);
  }
  virtual IPythonShell *GetPythonShell() override
  {
    return InvokeRetFunction<IPythonShell *>(&ICaptureContext::GetPythonShell);
  }
  virtual IResourceInspector *GetResourceInspector() override
  {
    return InvokeRetFunction<IResourceInspector *>(&ICaptureContext::GetResourceInspector);
  }
  virtual bool HasEventBrowser() override
  {
    return InvokeRetFunction<bool>(&ICaptureContext::HasEventBrowser);
  }
  virtual bool HasAPIInspector() override
  {
    return InvokeRetFunction<bool>(&ICaptureContext::HasAPIInspector);
  }
  virtual bool HasTextureViewer() override
  {
    return InvokeRetFunction<bool>(&ICaptureContext::HasTextureViewer);
  }
  virtual bool HasPipelineViewer() override
  {
    return InvokeRetFunction<bool>(&ICaptureContext::HasPipelineViewer);
  }
  virtual bool HasMeshPreview() override
  {
    return InvokeRetFunction<bool>(&ICaptureContext::HasMeshPreview);
  }
  virtual bool HasCaptureDialog() override
  {
    return InvokeRetFunction<bool>(&ICaptureContext::HasCaptureDialog);
  }
  virtual bool HasDebugMessageView() override
  {
    return InvokeRetFunction<bool>(&ICaptureContext::HasDebugMessageView);
  }
  virtual bool HasDiagnosticLogView() override
  {
    return InvokeRetFunction<bool>(&ICaptureContext::HasDiagnosticLogView);
  }
  virtual bool HasCommentView() override
  {
    return InvokeRetFunction<bool>(&ICaptureContext::HasCommentView);
  }
  virtual bool HasPerformanceCounterViewer() override
  {
    return InvokeRetFunction<bool>(&ICaptureContext::HasPerformanceCounterViewer);
  }
  virtual bool HasStatisticsViewer() override
  {
    return InvokeRetFunction<bool>(&ICaptureContext::HasStatisticsViewer);
  }
  virtual bool HasTimelineBar() override
  {
    return InvokeRetFunction<bool>(&ICaptureContext::HasTimelineBar);
  }
  virtual bool HasPythonShell() override
  {
    return InvokeRetFunction<bool>(&ICaptureContext::HasPythonShell);
  }
  virtual bool HasResourceInspector() override
  {
    return InvokeRetFunction<bool>(&ICaptureContext::HasResourceInspector);
  }

  virtual void ShowEventBrowser() override
  {
    InvokeVoidFunction(&ICaptureContext::ShowEventBrowser);
  }
  virtual void ShowAPIInspector() override
  {
    InvokeVoidFunction(&ICaptureContext::ShowAPIInspector);
  }
  virtual void ShowTextureViewer() override
  {
    InvokeVoidFunction(&ICaptureContext::ShowTextureViewer);
  }
  virtual void ShowMeshPreview() override { InvokeVoidFunction(&ICaptureContext::ShowMeshPreview); }
  virtual void ShowPipelineViewer() override
  {
    InvokeVoidFunction(&ICaptureContext::ShowPipelineViewer);
  }
  virtual void ShowCaptureDialog() override
  {
    InvokeVoidFunction(&ICaptureContext::ShowCaptureDialog);
  }
  virtual void ShowDebugMessageView() override
  {
    InvokeVoidFunction(&ICaptureContext::ShowDebugMessageView);
  }
  virtual void ShowDiagnosticLogView() override
  {
    InvokeVoidFunction(&ICaptureContext::ShowDiagnosticLogView);
  }
  virtual void ShowCommentView() override { InvokeVoidFunction(&ICaptureContext::ShowCommentView); }
  virtual void ShowPerformanceCounterViewer() override
  {
    InvokeVoidFunction(&ICaptureContext::ShowPerformanceCounterViewer);
  }
  virtual void ShowStatisticsViewer() override
  {
    InvokeVoidFunction(&ICaptureContext::ShowStatisticsViewer);
  }
  virtual void ShowTimelineBar() override { InvokeVoidFunction(&ICaptureContext::ShowTimelineBar); }
  virtual void ShowPythonShell() override { InvokeVoidFunction(&ICaptureContext::ShowPythonShell); }
  virtual void ShowResourceInspector() override
  {
    InvokeVoidFunction(&ICaptureContext::ShowResourceInspector);
  }
  virtual IShaderViewer *EditShader(ResourceId id, ShaderStage stage, const rdcstr &entryPoint,
                                    const rdcstrpairs &files, KnownShaderTool knownTool,
                                    ShaderEncoding shaderEncoding, ShaderCompileFlags flags,
                                    IShaderViewer::SaveCallback saveCallback,
                                    IShaderViewer::RevertCallback revertCallback) override
  {
    return InvokeRetFunction<IShaderViewer *>(&ICaptureContext::EditShader, id, stage, entryPoint,
                                              files, knownTool, shaderEncoding, flags, saveCallback,
                                              revertCallback);
  }

  virtual IShaderViewer *DebugShader(const ShaderReflection *shader, ResourceId pipeline,
                                     ShaderDebugTrace *trace, const rdcstr &debugContext) override
  {
    return InvokeRetFunction<IShaderViewer *>(&ICaptureContext::DebugShader, shader, pipeline,
                                              trace, debugContext);
  }

  virtual IShaderViewer *ViewShader(const ShaderReflection *shader, ResourceId pipeline) override
  {
    return InvokeRetFunction<IShaderViewer *>(&ICaptureContext::ViewShader, shader, pipeline);
  }

  virtual IShaderMessageViewer *ViewShaderMessages(ShaderStageMask stages) override
  {
    return InvokeRetFunction<IShaderMessageViewer *>(&ICaptureContext::ViewShaderMessages, stages);
  }

  virtual IDescriptorViewer *ViewDescriptorStore(ResourceId id) override
  {
    return InvokeRetFunction<IDescriptorViewer *>(&ICaptureContext::ViewDescriptorStore, id);
  }
  virtual IDescriptorViewer *ViewDescriptors(const rdcarray<Descriptor> &descriptors,
                                             const rdcarray<SamplerDescriptor> &samplerDescriptors) override
  {
    return InvokeRetFunction<IDescriptorViewer *>(&ICaptureContext::ViewDescriptors, descriptors,
                                                  samplerDescriptors);
  }

  virtual IBufferViewer *ViewBuffer(uint64_t byteOffset, uint64_t byteSize, ResourceId id,
                                    const rdcstr &format = "") override
  {
    return InvokeRetFunction<IBufferViewer *>(&ICaptureContext::ViewBuffer, byteOffset, byteSize,
                                              id, format);
  }

  virtual IBufferViewer *ViewTextureAsBuffer(ResourceId id, const Subresource &sub,
                                             const rdcstr &format = "") override
  {
    return InvokeRetFunction<IBufferViewer *>(&ICaptureContext::ViewTextureAsBuffer, id, sub, format);
  }

  virtual IBufferViewer *ViewConstantBuffer(ShaderStage stage, uint32_t slot, uint32_t idx) override
  {
    return InvokeRetFunction<IBufferViewer *>(&ICaptureContext::ViewConstantBuffer, stage, slot, idx);
  }

  virtual IPixelHistoryView *ViewPixelHistory(ResourceId texID, uint32_t x, uint32_t y,
                                              uint32_t view, const TextureDisplay &display) override
  {
    return InvokeRetFunction<IPixelHistoryView *>(&ICaptureContext::ViewPixelHistory, texID, x, y,
                                                  view, display);
  }

  virtual QWidget *CreateBuiltinWindow(const rdcstr &objectName) override
  {
    return InvokeRetFunction<QWidget *>(&ICaptureContext::CreateBuiltinWindow, objectName);
  }

  virtual void BuiltinWindowClosed(QWidget *window) override
  {
    InvokeVoidFunction(&ICaptureContext::BuiltinWindowClosed, window);
  }

  virtual void RaiseDockWindow(QWidget *dockWindow) override
  {
    InvokeVoidFunction(&ICaptureContext::RaiseDockWindow, dockWindow);
  }

  virtual void AddDockWindow(QWidget *newWindow, DockReference ref, QWidget *refWindow,
                             float percentage = 0.5f) override
  {
    InvokeVoidFunction(&ICaptureContext::AddDockWindow, newWindow, ref, refWindow, percentage);
  }
};

PythonShell::PythonShell(ICaptureContext &ctx, QWidget *parent)
    : QFrame(parent), ui(new Ui::PythonShell), m_Ctx(ctx)
{
  ui->setupUi(this);

  m_ThreadCtx = new CaptureContextInvoker(this, m_Ctx);

  QObject::connect(ui->lineInput, &RDLineEdit::keyPress, this, &PythonShell::interactive_keypress);
  QObject::connect(ui->helpSearch, &RDLineEdit::keyPress, this, &PythonShell::helpSearch_keypress);

  ui->lineInput->setFont(Formatter::FixedFont());
  ui->interactiveOutput->setFont(Formatter::FixedFont());
  ui->scriptOutput->setFont(Formatter::FixedFont());
  ui->helpText->setFont(Formatter::FixedFont());

  ui->lineInput->setAcceptTabCharacters(true);

  scriptEditor = new ScintillaEdit(this);

  scriptEditor->styleSetFont(STYLE_DEFAULT, Formatter::FixedFont().family().toUtf8().data());

  scriptEditor->setMarginLeft(4.0);
  scriptEditor->setMarginWidthN(0, 32.0);
  scriptEditor->setMarginWidthN(1, 0.0);
  scriptEditor->setMarginWidthN(2, 16.0);
  scriptEditor->setObjectName(lit("scriptEditor"));

  scriptEditor->markerSetBack(CURRENT_MARKER, SCINTILLA_COLOUR(240, 128, 128));
  scriptEditor->markerSetBack(CURRENT_MARKER + 1, SCINTILLA_COLOUR(240, 128, 128));
  scriptEditor->markerDefine(CURRENT_MARKER, SC_MARK_SHORTARROW);
  scriptEditor->markerDefine(CURRENT_MARKER + 1, SC_MARK_BACKGROUND);

  scriptEditor->autoCSetMaxHeight(10);

  scriptEditor->usePopUp(SC_POPUP_NEVER);

  scriptEditor->setContextMenuPolicy(Qt::CustomContextMenu);
  QObject::connect(scriptEditor, &ScintillaEdit::customContextMenuRequested, this,
                   &PythonShell::editor_contextMenu);

  ConfigureSyntax(scriptEditor, SCLEX_PYTHON);

  scriptEditor->setTabWidth(4);

  scriptEditor->setScrollWidth(1);
  scriptEditor->setScrollWidthTracking(true);

  scriptEditor->colourise(0, -1);

  QObject::connect(scriptEditor, &ScintillaEdit::modified,
                   [this](int type, int, int, int, const QByteArray &, int, int, int) {
                     if(type & (SC_MOD_INSERTTEXT | SC_MOD_DELETETEXT | SC_MOD_BEFOREINSERT |
                                SC_MOD_BEFOREDELETE))
                     {
                       scriptEditor->markerDeleteAll(CURRENT_MARKER);
                       scriptEditor->markerDeleteAll(CURRENT_MARKER + 1);
                     }
                   });

  QObject::connect(scriptEditor, &ScintillaEdit::charAdded, [this](int ch) {
    if(ch == '.')
    {
      startAutocomplete();
    }
  });

  QObject::connect(scriptEditor, &ScintillaEdit::keyPressed, [this](QKeyEvent *ev) {
    if(ev->key() == Qt::Key_Space && (ev->modifiers() & Qt::ControlModifier))
    {
      startAutocomplete();
    }

    if(ev->key() == Qt::Key_F1)
    {
      QString curWord = getDottedWordAtPoint(scriptEditor->currentPos());

      if(!curWord.isEmpty())
      {
        ui->helpSearch->setText(curWord);
        refreshCurrentHelp();
      }
    }
  });

  ui->scriptSplitter->insertWidget(0, scriptEditor);
  int w = ui->scriptSplitter->rect().width();
  ui->scriptSplitter->setSizes({w * 2 / 3, w / 3});

  ui->tabWidget->setCurrentIndex(0);

  interactiveContext = NULL;

  enableButtons(true);

  // reset output to default
  on_clear_clicked();
  on_newScript_clicked();
}

PythonShell::~PythonShell()
{
  m_Ctx.BuiltinWindowClosed(this);

  interactiveContext->Finish();

  delete m_ThreadCtx;

  delete ui;
}

PythonContext *PythonShell::GetScriptContext()
{
  return scriptContext;
}

void PythonShell::SetScriptText(rdcstr script)
{
  scriptEditor->setText(script.c_str());
}

bool PythonShell::LoadScriptFromFilename(rdcstr filename)
{
  if(!filename.isEmpty())
  {
    QFile f(filename);
    if(f.open(QIODevice::ReadOnly | QIODevice::Text))
    {
      scriptEditor->setText(f.readAll().data());
      return true;
    }
  }

  return false;
}

rdcstr PythonShell::GetScriptText()
{
  return scriptEditor->getText(scriptEditor->textLength() + 1).data();
}

void PythonShell::RunScript()
{
  PythonContext *context = newContext();

  ANALYTIC_SET(UIFeatures.PythonInterop, true);

  ui->outputHelpTabs->setCurrentIndex(0);

  ui->scriptOutput->clear();

  QString script = QString::fromUtf8(scriptEditor->getText(scriptEditor->textLength() + 1));

  enableButtons(false);

  LambdaThread *thread = new LambdaThread([this, script, context]() {
    scriptContext = context;
    context->executeString(lit("script.py"), script);
    scriptContext = NULL;

    GUIInvoke::call(this, [this, context]() {
      context->Finish();
      enableButtons(true);
    });
  });

  thread->setName(lit("Python script"));
  thread->selfDelete(true);
  thread->start();
}

void PythonShell::on_execute_clicked()
{
  QString command = ui->lineInput->text();

  ANALYTIC_SET(UIFeatures.PythonInterop, true);

  appendText(ui->interactiveOutput, command + lit("\n"));

  history.push_front(command);
  historyidx = -1;

  ui->lineInput->clear();

  // assume a trailing colon means there will be continuation. Store the command and add a continue
  // prompt. If we're already continuing, then wait until we get a blank line before executing.
  if(command.trimmed().right(1) == lit(":") || (!m_storedLines.isEmpty() && !command.isEmpty()))
  {
    appendText(ui->interactiveOutput, lit(".. "));
    m_storedLines += command + lit("\n");
    return;
  }

  // concatenate any previous lines if we are doing a multi-line command.
  command = m_storedLines + command;
  m_storedLines = QString();

  if(command.trimmed().length() > 0)
    interactiveContext->executeString(command);

  appendText(ui->interactiveOutput, lit(">> "));
}

void PythonShell::on_clear_clicked()
{
  QString minidocHeader = scriptHeader();

  minidocHeader += lit("\n\n>> ");

  ui->interactiveOutput->setText(minidocHeader);

  if(interactiveContext)
    interactiveContext->Finish();

  interactiveContext = newContext();
}

void PythonShell::on_newScript_clicked()
{
  QString minidocHeader = scriptHeader();

  minidocHeader.replace(QLatin1Char('\n'), lit("\n# "));

  minidocHeader = QFormatStr("# %1\n\n").arg(minidocHeader);

  scriptEditor->setText(minidocHeader.toUtf8().data());

  scriptEditor->emptyUndoBuffer();
}

void PythonShell::on_openScript_clicked()
{
  QString filename = RDDialog::getOpenFileName(this, tr("Open Python Script"), QString(),
                                               tr("Python scripts (*.py)"));

  if(!LoadScriptFromFilename(filename))
  {
    RDDialog::critical(this, tr("Error loading script"), tr("Couldn't open path %1.").arg(filename));
  }
}

void PythonShell::on_saveScript_clicked()
{
  QString filename = RDDialog::getSaveFileName(this, tr("Save Python Script"), QString(),
                                               tr("Python scripts (*.py)"));

  if(!filename.isEmpty())
  {
    QDir dirinfo = QFileInfo(filename).dir();
    if(dirinfo.exists())
    {
      QFile f(filename);
      if(f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
      {
        QString text = QString::fromUtf8(scriptEditor->getText(scriptEditor->textLength() + 1));
        text.remove(QLatin1Char('\r'));
        f.write(text.toUtf8());
      }
      else
      {
        RDDialog::critical(
            this, tr("Error saving script"),
            tr("Couldn't open path %1 for write.\n%2").arg(filename).arg(f.errorString()));
      }
    }
    else
    {
      RDDialog::critical(this, tr("Invalid directory"),
                         tr("Cannot find target directory to save to"));
    }
  }
}

void PythonShell::on_runScript_clicked()
{
  RunScript();
}

void PythonShell::on_abortRun_clicked()
{
  if(scriptContext)
    scriptContext->abort();
}

void PythonShell::traceLine(const QString &file, int line)
{
  if(QObject::sender() == (QObject *)interactiveContext)
    return;

  scriptEditor->markerDeleteAll(CURRENT_MARKER);
  scriptEditor->markerDeleteAll(CURRENT_MARKER + 1);

  scriptEditor->markerAdd(line > 0 ? line - 1 : 0, CURRENT_MARKER);
  scriptEditor->markerAdd(line > 0 ? line - 1 : 0, CURRENT_MARKER + 1);
}

void PythonShell::exception(const QString &type, const QString &value, int finalLine,
                            QList<QString> frames)
{
  QTextEdit *out = ui->scriptOutput;
  if(QObject::sender() == (QObject *)interactiveContext)
    out = ui->interactiveOutput;

  QString exString;

  if(finalLine >= 0)
    traceLine(QString(), finalLine);

  if(!out->toPlainText().endsWith(QLatin1Char('\n')))
    exString = lit("\n");
  if(!frames.isEmpty())
  {
    exString += tr("Traceback (most recent call last):\n");
    for(const QString &f : frames)
      exString += QFormatStr("  %1\n").arg(f);
  }
  exString += QFormatStr("%1: %2\n").arg(type).arg(value);

  appendText(out, exString);
}

void PythonShell::textOutput(bool isStdError, const QString &output)
{
  QTextEdit *out = ui->scriptOutput;
  if(QObject::sender() == (QObject *)interactiveContext)
    out = ui->interactiveOutput;

  appendText(out, output);
}

void PythonShell::editor_contextMenu(const QPoint &pos)
{
  int scintillaPos = scriptEditor->positionFromPoint(pos.x(), pos.y());

  QMenu contextMenu(this);

  QString curWord = getDottedWordAtPoint(scintillaPos);

  bool valid = !curWord.isEmpty();

  QAction help(valid ? tr("Help for '%1'").arg(curWord) : tr("Help"), this);

  QObject::connect(&help, &QAction::triggered, [this, curWord] { selectedHelp(curWord); });

  help.setEnabled(valid);

  contextMenu.addAction(&help);
  contextMenu.addSeparator();

  QAction undo(tr("Undo"), this);
  QAction redo(tr("Redo"), this);

  QObject::connect(&undo, &QAction::triggered, [this] { scriptEditor->undo(); });
  QObject::connect(&redo, &QAction::triggered, [this] { scriptEditor->redo(); });

  undo.setEnabled(scriptEditor->canUndo());
  redo.setEnabled(scriptEditor->canRedo());

  contextMenu.addAction(&undo);
  contextMenu.addAction(&redo);
  contextMenu.addSeparator();

  QAction cutText(tr("Cut"), this);
  QAction copyText(tr("Copy"), this);
  QAction pasteText(tr("Paste"), this);
  QAction deleteText(tr("Delete"), this);

  QObject::connect(&cutText, &QAction::triggered, [this] { scriptEditor->cut(); });

  QObject::connect(&copyText, &QAction::triggered, [this] {
    scriptEditor->copyRange(scriptEditor->selectionStart(), scriptEditor->selectionEnd());
  });

  QObject::connect(&pasteText, &QAction::triggered, [this] { scriptEditor->paste(); });

  QObject::connect(&deleteText, &QAction::triggered, [this] {
    scriptEditor->deleteRange(scriptEditor->selectionStart(), scriptEditor->selectionEnd());
  });

  contextMenu.addAction(&cutText);
  contextMenu.addAction(&copyText);
  contextMenu.addAction(&pasteText);
  contextMenu.addAction(&deleteText);
  contextMenu.addSeparator();

  if(scriptEditor->selectionEmpty())
  {
    cutText.setEnabled(false);
    copyText.setEnabled(false);
    deleteText.setEnabled(false);
  }

  pasteText.setEnabled(scriptEditor->canPaste());

  QAction selectAll(tr("Select All"), this);
  QObject::connect(&selectAll, &QAction::triggered, [this] { scriptEditor->selectAll(); });
  contextMenu.addAction(&selectAll);

  RDDialog::show(&contextMenu, scriptEditor->viewport()->mapToGlobal(pos));
}

QString PythonShell::getDottedWordAtPoint(int scintillaPos)
{
  QByteArray wordChars = scriptEditor->wordChars();

  QByteArray wordCharsAndDot = wordChars;
  if(wordCharsAndDot.indexOf('.') < 0)
    wordCharsAndDot.append('.');

  scriptEditor->setWordChars(wordCharsAndDot.data());

  sptr_t start = scriptEditor->wordStartPosition(scintillaPos, true);
  sptr_t end = scriptEditor->wordEndPosition(scintillaPos, true);

  scriptEditor->setWordChars(wordChars.data());

  QString curWord = QString::fromUtf8(scriptEditor->textRange(start, end));

  bool valid = true;

  if(curWord.isEmpty() || (!curWord[0].isLetterOrNumber() && curWord[0] != QLatin1Char('_')))
    valid = false;

  for(QChar c : curWord)
  {
    if(!c.isLetterOrNumber() && c != QLatin1Char('_') && c != QLatin1Char('.'))
      valid = false;
  }

  return valid ? curWord : QString();
}

void PythonShell::selectedHelp(QString word)
{
  ui->helpSearch->setText(word);

  refreshCurrentHelp();
}

void PythonShell::refreshCurrentHelp()
{
  PythonContext *context = newImportedDummyContext();

  ui->helpText->clear();

  QObject::connect(
      context, &PythonContext::textOutput,
      [this](bool isStdError, const QString &output) { appendText(ui->helpText, output); });

  context->executeString(lit(R"(
try:
  import keyword
  if keyword.iskeyword("%1"):
    help("%1")
  else:
    help(%1)
except ImportError:
  help(%1)
)")
                             .arg(ui->helpSearch->text()));

  context->Finish();
}

void PythonShell::interactive_keypress(QKeyEvent *event)
{
  if(event->key() == Qt::Key_Tab)
  {
    QString base = ui->lineInput->text();
    if(!base.isEmpty() && !base.rbegin()->isSpace())
    {
      // search backwards from the end for the first non dotted identifier first, and extract that
      // substring. This is just ASCII, not unicode
      for(int i = base.count() - 1; i >= 0; i--)
      {
        if(!base[i].isLetterOrNumber() && base[i] != QLatin1Char('.') && base[i] != QLatin1Char('_'))
        {
          base = base.right(base.count() - 1 - i);
          break;
        }
      }

      // skip any initial digits that got included in the coarse search above
      while(!base.isEmpty() && base[0].isDigit())
        base.remove(0, 1);

      QStringList options = interactiveContext->completionOptions(base);

      QString line = ui->lineInput->text();

      if(!options.isEmpty())
      {
        QString commonSubstring = options[0];

        for(int i = 1; i < options.count(); i++)
        {
          const QString &opt = options[i];
          if(opt.count() < commonSubstring.count())
            commonSubstring.truncate(opt.count());

          for(int j = 0; j < commonSubstring.count(); j++)
          {
            if(commonSubstring[j] != opt[j])
            {
              commonSubstring.truncate(j);
              break;
            }
          }
        }

        if(commonSubstring.length() > base.length())
        {
          line.chop(base.length());
          line += commonSubstring;
          ui->lineInput->setText(line);
        }

        if(options.count() > 1)
        {
          QString text;
          text += line;
          text += lit("\n");
          for(const QString &opt : options)
          {
            text += opt;
            text += lit("\n");
          }
          text += m_storedLines.isEmpty() ? lit(">> ") : lit(".. ");
          appendText(ui->interactiveOutput, text);
        }
      }

      return;
    }

    ui->lineInput->insert(lit("\t"));
    return;
  }

  if(event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
    on_execute_clicked();

  bool moved = false;

  if(event->key() == Qt::Key_Down && historyidx > -1)
  {
    historyidx--;

    moved = true;
  }

  QString workingtext;

  if(event->key() == Qt::Key_Up && historyidx + 1 < history.count())
  {
    if(historyidx == -1)
      workingtext = ui->lineInput->text();

    historyidx++;

    moved = true;
  }

  if(moved)
  {
    if(historyidx == -1)
      ui->lineInput->setText(workingtext);
    else
      ui->lineInput->setText(history[historyidx]);

    ui->lineInput->deselect();
  }
}

void PythonShell::helpSearch_keypress(QKeyEvent *e)
{
  if(e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter)
    refreshCurrentHelp();
}

QString PythonShell::scriptHeader()
{
  return tr(R"(RenderDoc Python console, powered by python %1.
The 'pyrenderdoc' object is the current CaptureContext instance.
The 'renderdoc' and 'qrenderdoc' modules are available.
Documentation is available: https://renderdoc.org/docs/python_api/index.html)")
      .arg(interactiveContext->versionString());
}

void PythonShell::appendText(QTextEdit *output, const QString &text)
{
  output->moveCursor(QTextCursor::End);
  output->insertPlainText(text);

  // scroll to the bottom
  QScrollBar *vscroll = output->verticalScrollBar();
  vscroll->setValue(vscroll->maximum());
}

void PythonShell::enableButtons(bool enable)
{
  ui->newScript->setEnabled(enable);
  ui->openScript->setEnabled(enable);
  ui->saveScript->setEnabled(enable);
  ui->runScript->setEnabled(enable);
  ui->abortRun->setEnabled(!enable);
}

void PythonShell::startAutocomplete()
{
  sptr_t pos = scriptEditor->currentPos();
  sptr_t line = scriptEditor->lineFromPosition(pos);
  sptr_t lineStart = scriptEditor->positionFromLine(line);
  QByteArray lineText = scriptEditor->getLine(line);

  sptr_t end = pos - lineStart - 1;
  sptr_t start;
  for(start = end; start >= 0; start--)
  {
    char c = lineText[(int)start];
    if(QChar::fromLatin1(c).isLetterOrNumber() || c == '.' || c == '_')
      continue;

    start++;
    break;
  }

  QString comp = QString::fromUtf8(lineText.mid(start, end - start + 1));

  PythonContext *context = newImportedDummyContext();

  QStringList completions = context->completionOptions(comp);

  context->Finish();

  scriptEditor->autoCShow(comp.count(), completions.join(QLatin1Char(' ')).toUtf8().data());
}

PythonContext *PythonShell::newImportedDummyContext()
{
  sptr_t pos = scriptEditor->currentPos();

  PythonContext *context = new PythonContext();

  setGlobals(context);

  // super hack. Try to import any modules to get completion suggestions from them.
  // we only process imports with no indentation since they should be unconditional. We ignore
  // imports that fail.
  QByteArray text = scriptEditor->getText(pos + 1);

  for(int offs = 0; offs < text.length();)
  {
    // find the next newline (may be NULL if we're at the end)
    int newline = text.indexOf('\n', offs);

    // execute the import if there is one
    const char *c = text.data() + offs;
    if(!strncmp(c, "import ", 7))
    {
      context->executeString(newline >= 0 ? QString::fromUtf8(c, newline - offs + 1)
                                          : QString::fromUtf8(c));
    }

    if(newline < 0)
      break;

    // move to the next line
    offs = newline + 1;
  }

  return context;
}

PythonContext *PythonShell::newContext()
{
  PythonContext *ret = new PythonContext();

  QObject::connect(ret, &PythonContext::traceLine, this, &PythonShell::traceLine);
  QObject::connect(ret, &PythonContext::exception, this, &PythonShell::exception);
  QObject::connect(ret, &PythonContext::textOutput, this, &PythonShell::textOutput);

  setGlobals(ret);

  return ret;
}

void PythonShell::setGlobals(PythonContext *ret)
{
  ret->setGlobal("pyrenderdoc", (ICaptureContext *)m_ThreadCtx);
}
