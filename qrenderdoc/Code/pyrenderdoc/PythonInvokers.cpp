/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2017-2026 Baldur Karlsson
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

#include "Code/pyrenderdoc/PythonContext.h"
#include "Windows/PythonShell.h"

// a forwarder that invokes onto the UI thread wherever necessary.
// Note this does NOT make CaptureContext thread safe. We just invoke for any potentially UI
// operations. All invokes are blocking, so there can't be any times when the UI thread waits
// on the python thread.
template <typename Obj>
struct UIThreadInvoker : Obj
{
  UIThreadInvoker(ICaptureContext &ctx, Obj &o) : m_Ctx(ctx), m_Obj(o) {}
  virtual ~UIThreadInvoker() {}
  ICaptureContext &m_Ctx;
  Obj &m_Obj;

  template <typename F, typename... paramTypes>
  void InvokeVoidFunction(F ptr, paramTypes... params)
  {
    if(!GUIInvoke::onUIThread())
    {
      void *ctx = PythonContext::PausePythonThreading();
      GUIInvoke::blockcall(m_Ctx.GetMainWindow()->Widget(),
                           [this, ptr, params...]() { (m_Obj.*ptr)(params...); });
      PythonContext::ResumePythonThreading(ctx);
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
      void *ctx = PythonContext::PausePythonThreading();
      GUIInvoke::blockcall(m_Ctx.GetMainWindow()->Widget(),
                           [this, &ret, ptr, params...]() { ret = (m_Obj.*ptr)(params...); });
      PythonContext::ResumePythonThreading(ctx);
      return ret;
    }

    return (m_Obj.*ptr)(params...);
  }
};

struct MiniQtInvoker : UIThreadInvoker<IMiniQtHelper>
{
  MiniQtInvoker(ICaptureContext &ctx, IMiniQtHelper &obj) : UIThreadInvoker(ctx, obj) {}
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
  void SetLayoutSpacing(QWidget *layout, int spacing)
  {
    InvokeVoidFunction(&IMiniQtHelper::SetLayoutSpacing, layout, spacing);
  }
  void SetLayoutMargins(QWidget *layout, int horizontal, int vertical)
  {
    InvokeVoidFunction(&IMiniQtHelper::SetLayoutMargins, layout, horizontal, vertical);
  }

  // widget manipulation

  void SetWidgetText(QWidget *widget, const rdcstr &text)
  {
    InvokeVoidFunction(&IMiniQtHelper::SetWidgetText, widget, text);
  }
  void AppendText(QWidget *widget, const rdcstr &text)
  {
    InvokeVoidFunction(&IMiniQtHelper::AppendText, widget, text);
  }
  rdcstr GetWidgetText(QWidget *widget)
  {
    return InvokeRetFunction<rdcstr>(&IMiniQtHelper::GetWidgetText, widget);
  }

  void ScrollToTop(QWidget *widget) { InvokeVoidFunction(&IMiniQtHelper::ScrollToTop, widget); }
  void ScrollToBottom(QWidget *widget)
  {
    InvokeVoidFunction(&IMiniQtHelper::ScrollToBottom, widget);
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

struct ExtensionInvoker : UIThreadInvoker<IExtensionManager>
{
  MiniQtInvoker *m_MiniQt;
  ExtensionInvoker(ICaptureContext &ctx, IExtensionManager &obj) : UIThreadInvoker(ctx, obj)
  {
    m_MiniQt = new MiniQtInvoker(ctx, obj.GetMiniQtHelper());
  }
  virtual ~ExtensionInvoker() { delete m_MiniQt; }
  //
  ///////////////////////////////////////////////////////////////////////
  // pass-through functions that don't need the UI thread
  ///////////////////////////////////////////////////////////////////////
  //
  rdcarray<ExtensionMetadata> GetInstalledExtensions() { return m_Obj.GetInstalledExtensions(); }
  rdcarray<rdcstr> GetLoadedExtensions() { return m_Obj.GetLoadedExtensions(); }
  bool IsExtensionLoaded(rdcstr name) { return m_Obj.IsExtensionLoaded(name); }
  rdcstr LoadExtension(rdcstr name) { return m_Obj.LoadExtension(name); }
  bool IsPythonDebuggerConnected() { return m_Obj.IsPythonDebuggerConnected(); }
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

struct ReplayControllerInvoker : IReplayController
{
  ReplayControllerInvoker(ICaptureContext &ctx) : m_Ctx(ctx) {}
  virtual ~ReplayControllerInvoker() {}
  PythonShell *m_Shell;
  ICaptureContext &m_Ctx;

  template <typename F, typename... paramTypes>
  void InvokeVoidFunction(F ptr, paramTypes... params)
  {
    void *ctx = PythonContext::PausePythonThreading();
    m_Ctx.Replay().BlockInvoke(
        [ptr, params...](IReplayController *replay) { (replay->*ptr)(params...); });
    PythonContext::ResumePythonThreading(ctx);
  }

  template <typename R, typename F, typename... paramTypes>
  R InvokeRetFunction(F ptr, paramTypes... params)
  {
    R ret = R();
    void *ctx = PythonContext::PausePythonThreading();
    m_Ctx.Replay().BlockInvoke(
        [&ret, ptr, params...](IReplayController *replay) { ret = (replay->*ptr)(params...); });
    PythonContext::ResumePythonThreading(ctx);
    return ret;
  }

  template <typename R, typename F, typename... paramTypes>
  R &InvokeRetRefFunction(F ptr, paramTypes... params)
  {
    R *ret = NULL;
    void *ctx = PythonContext::PausePythonThreading();
    m_Ctx.Replay().BlockInvoke(
        [&ret, ptr, params...](IReplayController *replay) { ret = &(replay->*ptr)(params...); });
    PythonContext::ResumePythonThreading(ctx);
    return *ret;
  }

  APIProperties GetAPIProperties()
  {
    return InvokeRetFunction<APIProperties>(&IReplayController::GetAPIProperties);
  }

  rdcarray<WindowingSystem> GetSupportedWindowSystems()
  {
    return InvokeRetFunction<rdcarray<WindowingSystem>>(&IReplayController::GetSupportedWindowSystems);
  }

  IReplayOutput *CreateOutput(WindowingData window, ReplayOutputType type)
  {
    return InvokeRetFunction<IReplayOutput *>(&IReplayController::CreateOutput, window, type);
  }

  void Shutdown()
  {
    if(!GUIInvoke::onUIThread())
    {
      void *ctx = PythonContext::PausePythonThreading();
      GUIInvoke::blockcall(m_Ctx.GetMainWindow()->Widget(), [this]() { m_Ctx.CloseCapture(); });
      PythonContext::ResumePythonThreading(ctx);
      return;
    }
    m_Ctx.CloseCapture();
  }

  void ReplayLoop(WindowingData window, ResourceId texid)
  {
    return InvokeVoidFunction(&IReplayController::ReplayLoop, window, texid);
  }

  rdcstr CreateRGPProfile(WindowingData window)
  {
    return InvokeRetFunction<rdcstr>(&IReplayController::CreateRGPProfile, window);
  }

  void CancelReplayLoop() { return InvokeVoidFunction(&IReplayController::CancelReplayLoop); }

  void FileChanged() { return InvokeVoidFunction(&IReplayController::FileChanged); }

  void SetFrameEvent(uint32_t eventId, bool force)
  {
    // go through the context so the UI stays up to date
    if(!GUIInvoke::onUIThread())
    {
      void *ctx = PythonContext::PausePythonThreading();
      GUIInvoke::blockcall(m_Ctx.GetMainWindow()->Widget(), [this, eventId, force]() {
        m_Ctx.SetEventID({}, eventId, eventId, force);
      });
      PythonContext::ResumePythonThreading(ctx);
      return;
    }

    m_Ctx.SetEventID({}, eventId, eventId, force);
  }

  const D3D11Pipe::State *GetD3D11PipelineState()
  {
    return InvokeRetFunction<const D3D11Pipe::State *>(&IReplayController::GetD3D11PipelineState);
  }

  const D3D12Pipe::State *GetD3D12PipelineState()
  {
    return InvokeRetFunction<const D3D12Pipe::State *>(&IReplayController::GetD3D12PipelineState);
  }

  const GLPipe::State *GetGLPipelineState()
  {
    return InvokeRetFunction<const GLPipe::State *>(&IReplayController::GetGLPipelineState);
  }

  const VKPipe::State *GetVulkanPipelineState()
  {
    return InvokeRetFunction<const VKPipe::State *>(&IReplayController::GetVulkanPipelineState);
  }

  const PipeState &GetPipelineState()
  {
    return InvokeRetRefFunction<const PipeState>(&IReplayController::GetPipelineState);
  }

  rdcarray<Descriptor> GetDescriptors(ResourceId descriptorStore,
                                      const rdcarray<DescriptorRange> &ranges)
  {
    return InvokeRetFunction<rdcarray<Descriptor>>(&IReplayController::GetDescriptors,
                                                   descriptorStore, ranges);
  }

  rdcarray<SamplerDescriptor> GetSamplerDescriptors(ResourceId descriptorStore,
                                                    const rdcarray<DescriptorRange> &ranges)
  {
    return InvokeRetFunction<rdcarray<SamplerDescriptor>>(&IReplayController::GetSamplerDescriptors,
                                                          descriptorStore, ranges);
  }

  const rdcarray<DescriptorAccess> &GetDescriptorAccess()
  {
    return InvokeRetRefFunction<const rdcarray<DescriptorAccess>>(
        &IReplayController::GetDescriptorAccess);
  }

  rdcarray<DescriptorLogicalLocation> GetDescriptorLocations(ResourceId descriptorStore,
                                                             const rdcarray<DescriptorRange> &ranges)
  {
    return InvokeRetFunction<rdcarray<DescriptorLogicalLocation>>(
        &IReplayController::GetDescriptorLocations, descriptorStore, ranges);
  }

  rdcarray<rdcstr> GetDisassemblyTargets(bool withPipeline)
  {
    return InvokeRetFunction<rdcarray<rdcstr>>(&IReplayController::GetDisassemblyTargets,
                                               withPipeline);
  }

  rdcstr DisassembleShader(ResourceId pipeline, const ShaderReflection *refl, const rdcstr &target)
  {
    return InvokeRetFunction<rdcstr>(&IReplayController::DisassembleShader, pipeline, refl, target);
  }

  void SetCustomShaderIncludes(const rdcarray<rdcstr> &directories)
  {
    return InvokeVoidFunction(&IReplayController::SetCustomShaderIncludes, directories);
  }

  rdcpair<ResourceId, rdcstr> BuildCustomShader(const rdcstr &entry, ShaderEncoding sourceEncoding,
                                                bytebuf source,
                                                const ShaderCompileFlags &compileFlags,
                                                ShaderStage type)
  {
    return InvokeRetFunction<rdcpair<ResourceId, rdcstr>>(
        &IReplayController::BuildCustomShader, entry, sourceEncoding, source, compileFlags, type);
  }

  void FreeCustomShader(ResourceId id)
  {
    return InvokeVoidFunction(&IReplayController::FreeCustomShader, id);
  }

  rdcpair<ResourceId, rdcstr> BuildTargetShader(const rdcstr &entry, ShaderEncoding sourceEncoding,
                                                bytebuf source,
                                                const ShaderCompileFlags &compileFlags,
                                                ShaderStage type)
  {
    return InvokeRetFunction<rdcpair<ResourceId, rdcstr>>(
        &IReplayController::BuildTargetShader, entry, sourceEncoding, source, compileFlags, type);
  }

  rdcarray<ShaderEncoding> GetTargetShaderEncodings()
  {
    return InvokeRetFunction<rdcarray<ShaderEncoding>>(&IReplayController::GetTargetShaderEncodings);
  }

  rdcarray<ShaderEncoding> GetCustomShaderEncodings()
  {
    return InvokeRetFunction<rdcarray<ShaderEncoding>>(&IReplayController::GetCustomShaderEncodings);
  }

  rdcarray<ShaderSourcePrefix> GetCustomShaderSourcePrefixes()
  {
    return InvokeRetFunction<rdcarray<ShaderSourcePrefix>>(
        &IReplayController::GetCustomShaderSourcePrefixes);
  }

  void ReplaceResource(ResourceId original, ResourceId replacement)
  {
    return InvokeVoidFunction(&IReplayController::ReplaceResource, original, replacement);
  }

  void ClearReplayCache() { return InvokeVoidFunction(&IReplayController::ClearReplayCache); }

  void ReloadShaderDebugInformation()
  {
    return InvokeVoidFunction(&IReplayController::ReloadShaderDebugInformation);
  }

  void RemoveReplacement(ResourceId id)
  {
    return InvokeVoidFunction(&IReplayController::RemoveReplacement, id);
  }

  void FreeTargetResource(ResourceId id)
  {
    return InvokeVoidFunction(&IReplayController::FreeTargetResource, id);
  }

  FrameDescription GetFrameInfo()
  {
    return InvokeRetFunction<FrameDescription>(&IReplayController::GetFrameInfo);
  }

  const SDFile &GetStructuredFile()
  {
    return InvokeRetRefFunction<const SDFile>(&IReplayController::GetStructuredFile);
  }

  void AddFakeMarkers() { return InvokeVoidFunction(&IReplayController::AddFakeMarkers); }

  const rdcarray<ActionDescription> &GetRootActions()
  {
    return InvokeRetRefFunction<const rdcarray<ActionDescription>>(&IReplayController::GetRootActions);
  }

  rdcarray<CounterResult> FetchCounters(const rdcarray<GPUCounter> &counters)
  {
    return InvokeRetFunction<rdcarray<CounterResult>>(&IReplayController::FetchCounters, counters);
  }

  rdcarray<GPUCounter> EnumerateCounters()
  {
    return InvokeRetFunction<rdcarray<GPUCounter>>(&IReplayController::EnumerateCounters);
  }

  CounterDescription DescribeCounter(GPUCounter counter)
  {
    return InvokeRetFunction<CounterDescription>(&IReplayController::DescribeCounter, counter);
  }

  const rdcarray<ResourceDescription> &GetResources()
  {
    return InvokeRetRefFunction<const rdcarray<ResourceDescription>>(&IReplayController::GetResources);
  }

  const rdcarray<TextureDescription> &GetTextures()
  {
    return InvokeRetRefFunction<const rdcarray<TextureDescription>>(&IReplayController::GetTextures);
  }

  const rdcarray<BufferDescription> &GetBuffers()
  {
    return InvokeRetRefFunction<const rdcarray<BufferDescription>>(&IReplayController::GetBuffers);
  }

  const rdcarray<DescriptorStoreDescription> &GetDescriptorStores()
  {
    return InvokeRetRefFunction<const rdcarray<DescriptorStoreDescription>>(
        &IReplayController::GetDescriptorStores);
  }

  rdcarray<DebugMessage> GetDebugMessages()
  {
    return InvokeRetFunction<rdcarray<DebugMessage>>(&IReplayController::GetDebugMessages);
  }

  ResultDetails GetFatalErrorStatus()
  {
    return InvokeRetFunction<ResultDetails>(&IReplayController::GetFatalErrorStatus);
  }

  rdcarray<ShaderEntryPoint> GetShaderEntryPoints(ResourceId shader)
  {
    return InvokeRetFunction<rdcarray<ShaderEntryPoint>>(&IReplayController::GetShaderEntryPoints,
                                                         shader);
  }

  const ShaderReflection *GetShader(ResourceId pipeline, ResourceId shader, ShaderEntryPoint entry)
  {
    return InvokeRetFunction<const ShaderReflection *>(&IReplayController::GetShader, pipeline,
                                                       shader, entry);
  }

  PixelValue PickPixel(ResourceId textureId, uint32_t x, uint32_t y, const Subresource &sub,
                       CompType typeCast)
  {
    return InvokeRetFunction<PixelValue>(&IReplayController::PickPixel, textureId, x, y, sub,
                                         typeCast);
  }

  rdcpair<PixelValue, PixelValue> GetMinMax(ResourceId textureId, const Subresource &sub,
                                            CompType typeCast)
  {
    return InvokeRetFunction<rdcpair<PixelValue, PixelValue>>(&IReplayController::GetMinMax,
                                                              textureId, sub, typeCast);
  }

  rdcarray<uint32_t> GetHistogram(ResourceId textureId, const Subresource &sub, CompType typeCast,
                                  float minval, float maxval, const rdcfixedarray<bool, 4> &channels)
  {
    return InvokeRetFunction<rdcarray<uint32_t>>(&IReplayController::GetHistogram, textureId, sub,
                                                 typeCast, minval, maxval, channels);
  }

  rdcarray<PixelModification> PixelHistory(ResourceId texture, uint32_t x, uint32_t y,
                                           const Subresource &sub, CompType typeCast)
  {
    return InvokeRetFunction<rdcarray<PixelModification>>(&IReplayController::PixelHistory, texture,
                                                          x, y, sub, typeCast);
  }

  ShaderDebugTrace *DebugVertex(uint32_t vertid, uint32_t instid, uint32_t idx, uint32_t view)
  {
    return InvokeRetFunction<ShaderDebugTrace *>(&IReplayController::DebugVertex, vertid, instid,
                                                 idx, view);
  }

  ShaderDebugTrace *DebugPixel(uint32_t x, uint32_t y, const DebugPixelInputs &inputs)
  {
    return InvokeRetFunction<ShaderDebugTrace *>(&IReplayController::DebugPixel, x, y, inputs);
  }

  ShaderDebugTrace *DebugThread(const rdcfixedarray<uint32_t, 3> &groupid,
                                const rdcfixedarray<uint32_t, 3> &threadid)
  {
    return InvokeRetFunction<ShaderDebugTrace *>(&IReplayController::DebugThread, groupid, threadid);
  }

  ShaderDebugTrace *DebugMeshThread(const rdcfixedarray<uint32_t, 3> &groupid,
                                    const rdcfixedarray<uint32_t, 3> &threadid)
  {
    return InvokeRetFunction<ShaderDebugTrace *>(&IReplayController::DebugMeshThread, groupid,
                                                 threadid);
  }

  rdcarray<ShaderDebugState> ContinueDebug(ShaderDebugger *debugger)
  {
    return InvokeRetFunction<rdcarray<ShaderDebugState>>(&IReplayController::ContinueDebug, debugger);
  }

  void FreeTrace(ShaderDebugTrace *trace)
  {
    return InvokeVoidFunction(&IReplayController::FreeTrace, trace);
  }

  rdcarray<EventUsage> GetUsage(ResourceId id)
  {
    return InvokeRetFunction<rdcarray<EventUsage>>(&IReplayController::GetUsage, id);
  }

  rdcarray<ShaderVariable> GetCBufferVariableContents(ResourceId pipeline, ResourceId shader,
                                                      ShaderStage stage, const rdcstr &entryPoint,
                                                      uint32_t cbufslot, ResourceId buffer,
                                                      uint64_t offset, uint64_t length)
  {
    return InvokeRetFunction<rdcarray<ShaderVariable>>(
        &IReplayController::GetCBufferVariableContents, pipeline, shader, stage, entryPoint,
        cbufslot, buffer, offset, length);
  }

  ResultDetails SaveTexture(const TextureSave &saveData, const rdcstr &path)
  {
    return InvokeRetFunction<ResultDetails>(&IReplayController::SaveTexture, saveData, path);
  }

  MeshFormat GetPostVSData(uint32_t instance, uint32_t view, MeshDataStage stage)
  {
    return InvokeRetFunction<MeshFormat>(&IReplayController::GetPostVSData, instance, view, stage);
  }

  bytebuf GetBufferData(ResourceId buff, uint64_t offset, uint64_t len)
  {
    return InvokeRetFunction<bytebuf>(&IReplayController::GetBufferData, buff, offset, len);
  }

  bytebuf GetTextureData(ResourceId tex, const Subresource &sub)
  {
    return InvokeRetFunction<bytebuf>(&IReplayController::GetTextureData, tex, sub);
  }
};

struct IMainWindowInvoker : UIThreadInvoker<IMainWindow>
{
  IMainWindowInvoker(ICaptureContext &ctx, IMainWindow &obj) : UIThreadInvoker(ctx, obj) {}
  virtual ~IMainWindowInvoker() {}

  QWidget *Widget() { return m_Obj.Widget(); }
  void RegisterShortcut(const rdcstr &shortcut, QWidget *widget, ShortcutCallback callback)
  {
    return InvokeVoidFunction(&IMainWindow::RegisterShortcut, shortcut, widget, callback);
  }
  void UnregisterShortcut(const rdcstr &shortcut, QWidget *widget)
  {
    return InvokeVoidFunction(&IMainWindow::UnregisterShortcut, shortcut, widget);
  }
  void BringToFront() { return InvokeVoidFunction(&IMainWindow::BringToFront); }
  bool PromptCloseCapture() { return InvokeRetFunction<bool>(&IMainWindow::PromptCloseCapture); }
};

struct IEventBrowserInvoker : UIThreadInvoker<IEventBrowser>
{
  IEventBrowserInvoker(ICaptureContext &ctx, IEventBrowser &obj) : UIThreadInvoker(ctx, obj) {}
  virtual ~IEventBrowserInvoker() {}

  QWidget *Widget() { return m_Obj.Widget(); }
  void UpdateDurationColumn() { return InvokeVoidFunction(&IEventBrowser::UpdateDurationColumn); }
  APIEvent GetAPIEventForEID(uint32_t eventId)
  {
    return InvokeRetFunction<APIEvent>(&IEventBrowser::GetAPIEventForEID, eventId);
  }
  const ActionDescription *GetActionForEID(uint32_t eventId)
  {
    return InvokeRetFunction<const ActionDescription *>(&IEventBrowser::GetActionForEID, eventId);
  }
  rdcstr GetEventName(uint32_t eventId)
  {
    return InvokeRetFunction<rdcstr>(&IEventBrowser::GetEventName, eventId);
  }
  bool IsAPIEventVisible(uint32_t eventId)
  {
    return InvokeRetFunction<bool>(&IEventBrowser::IsAPIEventVisible, eventId);
  }
  bool RegisterEventFilterFunction(const rdcstr &name, const rdcstr &description,
                                   EventFilterCallback filter, FilterParseCallback parser,
                                   AutoCompleteCallback completer)
  {
    return InvokeRetFunction<bool>(&IEventBrowser::RegisterEventFilterFunction, name, description,
                                   filter, parser, completer);
  }
  bool UnregisterEventFilterFunction(const rdcstr &name)
  {
    return InvokeRetFunction<bool>(&IEventBrowser::UnregisterEventFilterFunction, name);
  }
  void SetCurrentFilterText(const rdcstr &text)
  {
    return InvokeVoidFunction(&IEventBrowser::SetCurrentFilterText, text);
  };
  rdcstr GetCurrentFilterText()
  {
    return InvokeRetFunction<rdcstr>(&IEventBrowser::GetCurrentFilterText);
  }
  void SetUseCustomActionNames(bool use)
  {
    return InvokeVoidFunction(&IEventBrowser::SetUseCustomActionNames, use);
  }
  void SetShowParameterNames(bool show)
  {
    return InvokeVoidFunction(&IEventBrowser::SetShowParameterNames, show);
  }
  void SetShowAllParameters(bool show)
  {
    return InvokeVoidFunction(&IEventBrowser::SetShowAllParameters, show);
  }
  void SetEmptyRegionsVisible(bool show)
  {
    return InvokeVoidFunction(&IEventBrowser::SetEmptyRegionsVisible, show);
  }
  void SetHighlightedAnnotation(const rdcstr &annotationPath)
  {
    return InvokeVoidFunction(&IEventBrowser::SetHighlightedAnnotation, annotationPath);
  }
  rdcstr GetHighlightedAnnotation()
  {
    return InvokeRetFunction<rdcstr>(&IEventBrowser::GetHighlightedAnnotation);
  }
  void SetDurationColumnVisible(bool show)
  {
    return InvokeVoidFunction(&IEventBrowser::SetDurationColumnVisible, show);
  }
  void SetAnnotationColumnVisible(bool show)
  {
    return InvokeVoidFunction(&IEventBrowser::SetAnnotationColumnVisible, show);
  }
};

struct IAPIInspectorInvoker : UIThreadInvoker<IAPIInspector>
{
  IAPIInspectorInvoker(ICaptureContext &ctx, IAPIInspector &obj) : UIThreadInvoker(ctx, obj) {}
  virtual ~IAPIInspectorInvoker() {}

  QWidget *Widget() { return m_Obj.Widget(); }
  virtual void Refresh() { return InvokeVoidFunction(&IAPIInspector::Refresh); }
  virtual void RevealParameter(SDObject *param)
  {
    return InvokeVoidFunction(&IAPIInspector::RevealParameter, param);
  }
};

struct IAnnotationViewerInvoker : UIThreadInvoker<IAnnotationViewer>
{
  IAnnotationViewerInvoker(ICaptureContext &ctx, IAnnotationViewer &obj) : UIThreadInvoker(ctx, obj)
  {
  }
  virtual ~IAnnotationViewerInvoker() {}

  QWidget *Widget() { return m_Obj.Widget(); }

  void RevealAnnotation(const rdcstr &keyPath)
  {
    return InvokeVoidFunction(&IAnnotationViewer::RevealAnnotation, keyPath);
  }
};

struct ITextureViewerInvoker : UIThreadInvoker<ITextureViewer>
{
  ITextureViewerInvoker(ICaptureContext &ctx, ITextureViewer &obj) : UIThreadInvoker(ctx, obj) {}
  virtual ~ITextureViewerInvoker() {}

  QWidget *Widget() { return m_Obj.Widget(); }
  void ViewTexture(ResourceId resourceId, CompType typeCast, bool focus)
  {
    return InvokeVoidFunction(&ITextureViewer::ViewTexture, resourceId, typeCast, focus);
  }
  void ViewFollowedResource(FollowType followType, ShaderStage stage, int32_t index,
                            int32_t arrayElement)
  {
    return InvokeVoidFunction(&ITextureViewer::ViewFollowedResource, followType, stage, index,
                              arrayElement);
  }
  ResourceId GetCurrentResource()
  {
    return InvokeRetFunction<ResourceId>(&ITextureViewer::GetCurrentResource);
  }
  Subresource GetSelectedSubresource()
  {
    return InvokeRetFunction<Subresource>(&ITextureViewer::GetSelectedSubresource);
  }
  void SetSelectedSubresource(Subresource sub)
  {
    return InvokeVoidFunction(&ITextureViewer::SetSelectedSubresource, sub);
  }
  void GotoLocation(uint32_t x, uint32_t y)
  {
    return InvokeVoidFunction(&ITextureViewer::GotoLocation, x, y);
  }
  rdcpair<int32_t, int32_t> GetPickedLocation()
  {
    return InvokeRetFunction<rdcpair<int32_t, int32_t>>(&ITextureViewer::GetPickedLocation);
  }
  DebugOverlay GetTextureOverlay()
  {
    return InvokeRetFunction<DebugOverlay>(&ITextureViewer::GetTextureOverlay);
  }
  void SetTextureOverlay(DebugOverlay overlay)
  {
    return InvokeVoidFunction(&ITextureViewer::SetTextureOverlay, overlay);
  }
  bool IsZoomAutoFit() { return InvokeRetFunction<bool>(&ITextureViewer::IsZoomAutoFit); }
  TextureDisplay GetTextureDisplay()
  {
    return InvokeRetFunction<TextureDisplay>(&ITextureViewer::GetTextureDisplay);
  }
  float GetZoomLevel() { return InvokeRetFunction<float>(&ITextureViewer::GetZoomLevel); }
  void SetZoomLevel(bool autofit, float zoom)
  {
    return InvokeVoidFunction(&ITextureViewer::SetZoomLevel, autofit, zoom);
  }
  rdcpair<float, float> GetHistogramRange()
  {
    return InvokeRetFunction<rdcpair<float, float>>(&ITextureViewer::GetHistogramRange);
  }
  void SetHistogramRange(float blackpoint, float whitepoint)
  {
    return InvokeVoidFunction(&ITextureViewer::SetHistogramRange, blackpoint, whitepoint);
  }
  uint32_t GetChannelVisibilityBits()
  {
    return InvokeRetFunction<uint32_t>(&ITextureViewer::GetChannelVisibilityBits);
  }
  rdcfixedarray<bool, 4> GetChannelVisibility()
  {
    return InvokeRetFunction<rdcfixedarray<bool, 4>>(&ITextureViewer::GetChannelVisibility);
  }
  void SetChannelVisibility(bool red, bool green, bool blue, bool alpha)
  {
    return InvokeVoidFunction(&ITextureViewer::SetChannelVisibility, red, green, blue, alpha);
  }
};

struct IBufferViewerInvoker : UIThreadInvoker<IBufferViewer>
{
  IBufferViewerInvoker(ICaptureContext &ctx, IBufferViewer &obj) : UIThreadInvoker(ctx, obj) {}
  virtual ~IBufferViewerInvoker() {}

  QWidget *Widget() { return m_Obj.Widget(); }
  void ScrollToRow(int32_t row, MeshDataStage stage)
  {
    return InvokeVoidFunction(&IBufferViewer::ScrollToRow, row, stage);
  }
  void ScrollToColumn(int32_t column, MeshDataStage stage)
  {
    return InvokeVoidFunction(&IBufferViewer::ScrollToColumn, column, stage);
  }
  void ShowMeshData(MeshDataStage stage)
  {
    return InvokeVoidFunction(&IBufferViewer::ShowMeshData, stage);
  }
  void SetCurrentInstance(int32_t instance)
  {
    return InvokeVoidFunction(&IBufferViewer::SetCurrentInstance, instance);
  }
  void SetCurrentView(int32_t view)
  {
    return InvokeVoidFunction(&IBufferViewer::SetCurrentView, view);
  }
  void SetPreviewStage(MeshDataStage stage)
  {
    return InvokeVoidFunction(&IBufferViewer::SetPreviewStage, stage);
  }
};

struct IPipelineStateViewerInvoker : UIThreadInvoker<IPipelineStateViewer>
{
  IPipelineStateViewerInvoker(ICaptureContext &ctx, IPipelineStateViewer &obj)
      : UIThreadInvoker(ctx, obj)
  {
  }
  virtual ~IPipelineStateViewerInvoker() {}

  QWidget *Widget() { return m_Obj.Widget(); }
  virtual bool SaveShaderFile(const ShaderReflection *shader)
  {
    return InvokeRetFunction<bool>(&IPipelineStateViewer::SaveShaderFile, shader);
  }
  virtual void SelectPipelineStage(PipelineStage stage)
  {
    return InvokeVoidFunction(&IPipelineStateViewer::SelectPipelineStage, stage);
  }
};

struct ICaptureConnectionInvoker : public UIThreadInvoker<ICaptureConnection>
{
  ICaptureConnectionInvoker(ICaptureContext &ctx, ICaptureConnection &obj)
      : UIThreadInvoker(ctx, obj)
  {
    // delete ourself when the connection dies
    obj.RegisterClosedCallback([this](ICaptureConnection *) { delete this; });
  }
  virtual ~ICaptureConnectionInvoker() {}

  QWidget *Widget() { return m_Obj.Widget(); }
  void RegisterClosedCallback(ClosedCallback method)
  {
    return InvokeVoidFunction(&ICaptureConnection::RegisterClosedCallback, method);
  }
  bool IsConnected() { return InvokeRetFunction<bool>(&ICaptureConnection::IsConnected); }
  void PreventAutoClose() { return InvokeVoidFunction(&ICaptureConnection::PreventAutoClose); }
  rdcarray<rdcstr> GetAPIs()
  {
    return InvokeRetFunction<rdcarray<rdcstr>>(&ICaptureConnection::GetAPIs);
  }
  void QueueCapture(int frameNumber, int numFrames)
  {
    return InvokeVoidFunction(&ICaptureConnection::QueueCapture, frameNumber, numFrames);
  }
  void TimedCapture(float secondsDelay, int numFrames)
  {
    return InvokeVoidFunction(&ICaptureConnection::TimedCapture, secondsDelay, numFrames);
  }
  void CycleActiveWindow() { return InvokeVoidFunction(&ICaptureConnection::CycleActiveWindow); }
  rdcstr Target() { return InvokeRetFunction<rdcstr>(&ICaptureConnection::Target); }
  rdcstr Hostname() { return InvokeRetFunction<rdcstr>(&ICaptureConnection::Hostname); }
  rdcstr FriendlyHostname()
  {
    return InvokeRetFunction<rdcstr>(&ICaptureConnection::FriendlyHostname);
  }
  void Close(bool discardUnsaved)
  {
    return InvokeVoidFunction(&ICaptureConnection::Close, discardUnsaved);
  }
  rdcarray<ConnectedTempCapture> GetCaptures()
  {
    return InvokeRetFunction<rdcarray<ConnectedTempCapture>>(&ICaptureConnection::GetCaptures);
  }
  void OpenCapture(uint32_t ID) { return InvokeVoidFunction(&ICaptureConnection::OpenCapture, ID); }
  void DeleteCapture(uint32_t ID, bool promptForSave)
  {
    return InvokeVoidFunction(&ICaptureConnection::DeleteCapture, ID, promptForSave);
  }
  void SaveCapture(uint32_t ID, rdcstr filename)
  {
    return InvokeVoidFunction(&ICaptureConnection::SaveCapture, ID, filename);
  }
  rdcarray<uint32_t> GetChildProcesses()
  {
    return InvokeRetFunction<rdcarray<uint32_t>>(&ICaptureConnection::GetChildProcesses);
  }
  ICaptureConnection *ConnectToChild(uint32_t pid)
  {
    ICaptureConnection *ret =
        InvokeRetFunction<ICaptureConnection *>(&ICaptureConnection::ConnectToChild, pid);

    if(!ret)
      return ret;

    return new ICaptureConnectionInvoker(m_Ctx, *ret);
  }
};

struct ICaptureDialogInvoker : UIThreadInvoker<ICaptureDialog>
{
  ICaptureDialogInvoker(ICaptureContext &ctx, ICaptureDialog &obj) : UIThreadInvoker(ctx, obj) {}
  virtual ~ICaptureDialogInvoker() {}

  QWidget *Widget() { return m_Obj.Widget(); }
  bool IsInjectMode() { return InvokeRetFunction<bool>(&ICaptureDialog::IsInjectMode); }
  void SetInjectMode(bool inject)
  {
    return InvokeVoidFunction(&ICaptureDialog::SetInjectMode, inject);
  }
  void SetExecutableFilename(const rdcstr &filename)
  {
    return InvokeVoidFunction(&ICaptureDialog::SetExecutableFilename, filename);
  }
  void SetWorkingDirectory(const rdcstr &dir)
  {
    return InvokeVoidFunction(&ICaptureDialog::SetWorkingDirectory, dir);
  }
  void SetCommandLine(const rdcstr &cmd)
  {
    return InvokeVoidFunction(&ICaptureDialog::SetCommandLine, cmd);
  }
  void SetEnvironmentModifications(const rdcarray<EnvironmentModification> &modifications)
  {
    return InvokeVoidFunction(&ICaptureDialog::SetEnvironmentModifications, modifications);
  }
  void SetSettings(CaptureSettings settings)
  {
    return InvokeVoidFunction(&ICaptureDialog::SetSettings, settings);
  }
  CaptureSettings Settings()
  {
    return InvokeRetFunction<CaptureSettings>(&ICaptureDialog::Settings);
  }
  ICaptureConnection *Launch()
  {
    ICaptureConnection *ret = InvokeRetFunction<ICaptureConnection *>(&ICaptureDialog::Launch);

    if(!ret)
      return ret;

    return new ICaptureConnectionInvoker(m_Ctx, *ret);
  }
  void LoadSettings(const rdcstr &filename)
  {
    return InvokeVoidFunction(&ICaptureDialog::LoadSettings, filename);
  }
  void SaveSettings(const rdcstr &filename)
  {
    return InvokeVoidFunction(&ICaptureDialog::SaveSettings, filename);
  }
  void UpdateGlobalHook() { return InvokeVoidFunction(&ICaptureDialog::UpdateGlobalHook); }
  void UpdateRemoteHost() { return InvokeVoidFunction(&ICaptureDialog::UpdateRemoteHost); }
};

struct IDebugMessageViewInvoker : UIThreadInvoker<IDebugMessageView>
{
  IDebugMessageViewInvoker(ICaptureContext &ctx, IDebugMessageView &obj) : UIThreadInvoker(ctx, obj)
  {
  }
  virtual ~IDebugMessageViewInvoker() {}

  QWidget *Widget() { return m_Obj.Widget(); }
};

struct IDiagnosticLogViewInvoker : UIThreadInvoker<IDiagnosticLogView>
{
  IDiagnosticLogViewInvoker(ICaptureContext &ctx, IDiagnosticLogView &obj)
      : UIThreadInvoker(ctx, obj)
  {
  }
  virtual ~IDiagnosticLogViewInvoker() {}

  QWidget *Widget() { return m_Obj.Widget(); }
};

struct ICommentViewInvoker : UIThreadInvoker<ICommentView>
{
  ICommentViewInvoker(ICaptureContext &ctx, ICommentView &obj) : UIThreadInvoker(ctx, obj) {}
  virtual ~ICommentViewInvoker() {}

  QWidget *Widget() { return m_Obj.Widget(); }
  virtual void SetComments(const rdcstr &text)
  {
    return InvokeVoidFunction(&ICommentView::SetComments, text);
  }
  virtual rdcstr GetComments() { return InvokeRetFunction<rdcstr>(&ICommentView::GetComments); }
};

struct IPerformanceCounterViewerInvoker : UIThreadInvoker<IPerformanceCounterViewer>
{
  IPerformanceCounterViewerInvoker(ICaptureContext &ctx, IPerformanceCounterViewer &obj)
      : UIThreadInvoker(ctx, obj)
  {
  }
  virtual ~IPerformanceCounterViewerInvoker() {}

  QWidget *Widget() { return m_Obj.Widget(); }
  void UpdateDurationColumn()
  {
    return InvokeVoidFunction(&IPerformanceCounterViewer::UpdateDurationColumn);
  }
};

struct IStatisticsViewerInvoker : UIThreadInvoker<IStatisticsViewer>
{
  IStatisticsViewerInvoker(ICaptureContext &ctx, IStatisticsViewer &obj) : UIThreadInvoker(ctx, obj)
  {
  }
  virtual ~IStatisticsViewerInvoker() {}

  QWidget *Widget() { return m_Obj.Widget(); }
};

struct ITimelineBarInvoker : UIThreadInvoker<ITimelineBar>
{
  ITimelineBarInvoker(ICaptureContext &ctx, ITimelineBar &obj) : UIThreadInvoker(ctx, obj) {}
  virtual ~ITimelineBarInvoker() {}

  QWidget *Widget() { return m_Obj.Widget(); }
  void HighlightResourceUsage(ResourceId id)
  {
    return InvokeVoidFunction(&ITimelineBar::HighlightResourceUsage, id);
  }
  void HighlightHistory(ResourceId id, const rdcarray<PixelModification> &history)
  {
    return InvokeVoidFunction(&ITimelineBar::HighlightHistory, id, history);
  }
};

struct IPythonShellInvoker : UIThreadInvoker<IPythonShell>
{
  IPythonShellInvoker(ICaptureContext &ctx, IPythonShell &obj) : UIThreadInvoker(ctx, obj) {}
  virtual ~IPythonShellInvoker() {}

  QWidget *Widget() { return m_Obj.Widget(); }
  bool CheckUnsavedChanges() { return InvokeRetFunction<bool>(&IPythonShell::CheckUnsavedChanges); }
  bool LoadScriptFromFilename(rdcstr filename)
  {
    return InvokeRetFunction<bool>(&IPythonShell::LoadScriptFromFilename, filename);
  }
  void CreateNewScriptEditor(rdcstr name, rdcstr text)
  {
    return InvokeVoidFunction(&IPythonShell::CreateNewScriptEditor, name, text);
  }
  rdcstr GetScriptText() { return InvokeRetFunction<rdcstr>(&IPythonShell::GetScriptText); }
  void RunScript() { return InvokeVoidFunction(&IPythonShell::RunScript); }
  void AttachDebugger(const rdcstr &extensionName)
  {
    return InvokeVoidFunction(&IPythonShell::AttachDebugger, extensionName);
  }
  void SetExtensionOutputFilter(const rdcstr &extensionName)
  {
    return InvokeVoidFunction(&IPythonShell::SetExtensionOutputFilter, extensionName);
  }
  void SetScriptOutputFilter() { return InvokeVoidFunction(&IPythonShell::SetScriptOutputFilter); }
  void RemoveOutputFilter() { return InvokeVoidFunction(&IPythonShell::RemoveOutputFilter); }
  void ShowOutput() { return InvokeVoidFunction(&IPythonShell::ShowOutput); }
  void ShowREPL() { return InvokeVoidFunction(&IPythonShell::ShowREPL); }
  void ShowHelp() { return InvokeVoidFunction(&IPythonShell::ShowHelp); }
};

struct IResourceInspectorInvoker : UIThreadInvoker<IResourceInspector>
{
  IResourceInspectorInvoker(ICaptureContext &ctx, IResourceInspector &obj)
      : UIThreadInvoker(ctx, obj)
  {
  }
  virtual ~IResourceInspectorInvoker() {}

  QWidget *Widget() { return m_Obj.Widget(); }
  void Inspect(ResourceId id) { return InvokeVoidFunction(&IResourceInspector::Inspect, id); }
  ResourceId CurrentResource()
  {
    return InvokeRetFunction<ResourceId>(&IResourceInspector::CurrentResource);
  }
  void RevealParameter(SDObject *param)
  {
    return InvokeVoidFunction(&IResourceInspector::RevealParameter, param);
  }
};

struct CaptureContextInvoker : public UIThreadInvoker<ICaptureContext>
{
  ExtensionInvoker *m_Ext;
  ReplayControllerInvoker m_ReplayController;

#define WINDOW_INVOKER(iface)                              \
  iface##Invoker *m_invoker##iface = NULL;                 \
  iface *WrapInvoker(iface *i)                             \
  {                                                        \
    if(!m_invoker##iface || &m_invoker##iface->m_Obj != i) \
    {                                                      \
      delete m_invoker##iface;                             \
      m_invoker##iface = new iface##Invoker(m_Ctx, *i);    \
    }                                                      \
    return m_invoker##iface;                               \
  }

  WINDOW_INVOKER(IMainWindow);
  WINDOW_INVOKER(IEventBrowser);
  WINDOW_INVOKER(IAPIInspector);
  WINDOW_INVOKER(IAnnotationViewer);
  WINDOW_INVOKER(ITextureViewer);
  WINDOW_INVOKER(IBufferViewer);
  WINDOW_INVOKER(IPipelineStateViewer);
  WINDOW_INVOKER(ICaptureDialog);
  WINDOW_INVOKER(IDebugMessageView);
  WINDOW_INVOKER(IDiagnosticLogView);
  WINDOW_INVOKER(ICommentView);
  WINDOW_INVOKER(IPerformanceCounterViewer);
  WINDOW_INVOKER(IStatisticsViewer);
  WINDOW_INVOKER(ITimelineBar);
  WINDOW_INVOKER(IPythonShell);
  WINDOW_INVOKER(IResourceInspector);

  CaptureContextInvoker(ICaptureContext &ctx) : UIThreadInvoker(ctx, ctx), m_ReplayController(ctx)
  {
    m_Ext = new ExtensionInvoker(ctx, ctx.Extensions());
  }
  virtual ~CaptureContextInvoker() { delete m_Ext; }
  //
  ///////////////////////////////////////////////////////////////////////
  // pass-through functions that don't need the UI thread
  ///////////////////////////////////////////////////////////////////////
  //
  virtual void InvokeOntoUIThread(std::function<void()> callback) override
  {
    return m_Obj.InvokeOntoUIThread(callback);
  }
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
  virtual FrameDescription FrameInfo() override { return m_Obj.FrameInfo(); }
  virtual APIProperties APIProps() override { return m_Obj.APIProps(); }
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
  virtual void ClearReplayCache() override { return m_Obj.ClearReplayCache(); }
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
  virtual PersistentConfig &Config() override { return m_Obj.Config(); }
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
  virtual IReplayController *GetBlockingController() override
  {
    if(!m_Obj.IsCaptureLoaded())
      return NULL;
    return &m_ReplayController;
  }
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
  virtual void ClearMessages() override { InvokeVoidFunction(&ICaptureContext::ClearMessages); }
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
  virtual void EmbedDependentFiles() override
  {
    InvokeVoidFunction(&ICaptureContext::EmbedDependentFiles);
  }
  virtual void RemoveDependentFiles() override
  {
    InvokeVoidFunction(&ICaptureContext::RemoveDependentFiles);
  }
  virtual void DelayedCallback(uint32_t milliseconds, std::function<void()> callback) override
  {
    if(!GUIInvoke::onUIThread())
    {
      IPythonShell *shell = m_Ctx.GetPythonShell();
      void *ctx = PythonContext::PausePythonThreading();
      GUIInvoke::call(m_Ctx.GetMainWindow()->Widget(), [this, milliseconds, callback]() {
        m_Obj.DelayedCallback(milliseconds, callback);
      });
      PythonContext::ResumePythonThreading(ctx);
      return;
    }

    m_Obj.DelayedCallback(milliseconds, callback);
  }
  virtual IMainWindow *GetMainWindow() override
  {
    return WrapInvoker(InvokeRetFunction<IMainWindow *>(&ICaptureContext::GetMainWindow));
  }
  virtual IEventBrowser *GetEventBrowser() override
  {
    return WrapInvoker(InvokeRetFunction<IEventBrowser *>(&ICaptureContext::GetEventBrowser));
  }
  virtual IAPIInspector *GetAPIInspector() override
  {
    return WrapInvoker(InvokeRetFunction<IAPIInspector *>(&ICaptureContext::GetAPIInspector));
  }
  virtual IAnnotationViewer *GetAnnotationViewer() override
  {
    return WrapInvoker(InvokeRetFunction<IAnnotationViewer *>(&ICaptureContext::GetAnnotationViewer));
  }
  virtual ITextureViewer *GetTextureViewer() override
  {
    return WrapInvoker(InvokeRetFunction<ITextureViewer *>(&ICaptureContext::GetTextureViewer));
  }
  virtual IBufferViewer *GetMeshPreview() override
  {
    return WrapInvoker(InvokeRetFunction<IBufferViewer *>(&ICaptureContext::GetMeshPreview));
  }
  virtual IPipelineStateViewer *GetPipelineViewer() override
  {
    return WrapInvoker(InvokeRetFunction<IPipelineStateViewer *>(&ICaptureContext::GetPipelineViewer));
  }
  virtual ICaptureDialog *GetCaptureDialog() override
  {
    return WrapInvoker(InvokeRetFunction<ICaptureDialog *>(&ICaptureContext::GetCaptureDialog));
  }
  virtual IDebugMessageView *GetDebugMessageView() override
  {
    return WrapInvoker(InvokeRetFunction<IDebugMessageView *>(&ICaptureContext::GetDebugMessageView));
  }
  virtual IDiagnosticLogView *GetDiagnosticLogView() override
  {
    return WrapInvoker(
        InvokeRetFunction<IDiagnosticLogView *>(&ICaptureContext::GetDiagnosticLogView));
  }
  virtual ICommentView *GetCommentView() override
  {
    return WrapInvoker(InvokeRetFunction<ICommentView *>(&ICaptureContext::GetCommentView));
  }
  virtual IPerformanceCounterViewer *GetPerformanceCounterViewer() override
  {
    return WrapInvoker(InvokeRetFunction<IPerformanceCounterViewer *>(
        &ICaptureContext::GetPerformanceCounterViewer));
  }
  virtual IStatisticsViewer *GetStatisticsViewer() override
  {
    return WrapInvoker(InvokeRetFunction<IStatisticsViewer *>(&ICaptureContext::GetStatisticsViewer));
  }
  virtual ITimelineBar *GetTimelineBar() override
  {
    return WrapInvoker(InvokeRetFunction<ITimelineBar *>(&ICaptureContext::GetTimelineBar));
  }
  virtual IPythonShell *GetPythonShell() override
  {
    return WrapInvoker(InvokeRetFunction<IPythonShell *>(&ICaptureContext::GetPythonShell));
  }
  virtual IResourceInspector *GetResourceInspector() override
  {
    return WrapInvoker(
        InvokeRetFunction<IResourceInspector *>(&ICaptureContext::GetResourceInspector));
  }
  virtual bool HasEventBrowser() override
  {
    return InvokeRetFunction<bool>(&ICaptureContext::HasEventBrowser);
  }
  virtual bool HasAPIInspector() override
  {
    return InvokeRetFunction<bool>(&ICaptureContext::HasAPIInspector);
  }
  virtual bool HasAnnotationViewer() override
  {
    return InvokeRetFunction<bool>(&ICaptureContext::HasAnnotationViewer);
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
  virtual void ShowAnnotationViewer() override
  {
    InvokeVoidFunction(&ICaptureContext::ShowAnnotationViewer);
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

ICaptureContext *MakeCaptureContextInvoker(ICaptureContext &ctx)
{
  return new CaptureContextInvoker(ctx);
}

void FreeCaptureContextInvoker(ICaptureContext *ctx)
{
  CaptureContextInvoker *invoker = (CaptureContextInvoker *)ctx;
  delete invoker;
}
