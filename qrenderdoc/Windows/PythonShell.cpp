/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2017 Baldur Karlsson
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
#include <QScrollBar>
#include "3rdparty/scintilla/include/SciLexer.h"
#include "3rdparty/scintilla/include/qt/ScintillaEdit.h"
#include "Code/ScintillaSyntax.h"
#include "Code/pyrenderdoc/PythonContext.h"
#include "ui_PythonShell.h"

// a forwarder that invokes onto the UI thread wherever necessary.
// Note this does NOT make CaptureContext thread safe. We just invoke for any potentially UI
// operations. All invokes are blocking, so there can't be any times when the UI thread waits
// on the python thread.
struct CaptureContextInvoker : ICaptureContext
{
  ICaptureContext &m_Ctx;
  CaptureContextInvoker(ICaptureContext &ctx) : m_Ctx(ctx) {}
  virtual ~CaptureContextInvoker() {}
  //
  ///////////////////////////////////////////////////////////////////////
  // pass-through functions that don't need the UI thread
  ///////////////////////////////////////////////////////////////////////
  //
  virtual QString ConfigFilePath(const QString &filename) override
  {
    return m_Ctx.ConfigFilePath(filename);
  }
  virtual QString TempLogFilename(QString appname) override
  {
    return m_Ctx.TempLogFilename(appname);
  }
  virtual IReplayManager &Replay() override { return m_Ctx.Replay(); }
  virtual bool LogLoaded() override { return m_Ctx.LogLoaded(); }
  virtual bool IsLogLocal() override { return m_Ctx.IsLogLocal(); }
  virtual bool LogLoading() override { return m_Ctx.LogLoading(); }
  virtual QString LogFilename() override { return m_Ctx.LogFilename(); }
  virtual const FrameDescription &FrameInfo() override { return m_Ctx.FrameInfo(); }
  virtual const APIProperties &APIProps() override { return m_Ctx.APIProps(); }
  virtual uint32_t CurSelectedEvent() override { return m_Ctx.CurSelectedEvent(); }
  virtual uint32_t CurEvent() override { return m_Ctx.CurEvent(); }
  virtual const DrawcallDescription *CurSelectedDrawcall() override
  {
    return m_Ctx.CurSelectedDrawcall();
  }
  virtual const DrawcallDescription *CurDrawcall() override { return m_Ctx.CurDrawcall(); }
  virtual const DrawcallDescription *GetFirstDrawcall() override
  {
    return m_Ctx.GetFirstDrawcall();
  }
  virtual const DrawcallDescription *GetLastDrawcall() override { return m_Ctx.GetLastDrawcall(); }
  virtual const rdctype::array<DrawcallDescription> &CurDrawcalls() override
  {
    return m_Ctx.CurDrawcalls();
  }
  virtual TextureDescription *GetTexture(ResourceId id) override { return m_Ctx.GetTexture(id); }
  virtual const rdctype::array<TextureDescription> &GetTextures() override
  {
    return m_Ctx.GetTextures();
  }
  virtual BufferDescription *GetBuffer(ResourceId id) override { return m_Ctx.GetBuffer(id); }
  virtual const rdctype::array<BufferDescription> &GetBuffers() override
  {
    return m_Ctx.GetBuffers();
  }
  virtual const DrawcallDescription *GetDrawcall(uint32_t eventID) override
  {
    return m_Ctx.GetDrawcall(eventID);
  }
  virtual WindowingSystem CurWindowingSystem() override { return m_Ctx.CurWindowingSystem(); }
  virtual void *FillWindowingData(uintptr_t winId) override
  {
    return m_Ctx.FillWindowingData(winId);
  }
  virtual const QVector<DebugMessage> &DebugMessages() override { return m_Ctx.DebugMessages(); }
  virtual int UnreadMessageCount() override { return m_Ctx.UnreadMessageCount(); }
  virtual void MarkMessagesRead() override { return m_Ctx.MarkMessagesRead(); }
  virtual D3D11Pipe::State &CurD3D11PipelineState() override
  {
    return m_Ctx.CurD3D11PipelineState();
  }
  virtual D3D12Pipe::State &CurD3D12PipelineState() override
  {
    return m_Ctx.CurD3D12PipelineState();
  }
  virtual GLPipe::State &CurGLPipelineState() override { return m_Ctx.CurGLPipelineState(); }
  virtual VKPipe::State &CurVulkanPipelineState() override
  {
    return m_Ctx.CurVulkanPipelineState();
  }
  virtual CommonPipelineState &CurPipelineState() override { return m_Ctx.CurPipelineState(); }
  virtual PersistantConfig &Config() override { return m_Ctx.Config(); }
  //
  ///////////////////////////////////////////////////////////////////////
  // functions that invoke onto the UI thread
  ///////////////////////////////////////////////////////////////////////
  //
  template <typename F, typename... paramTypes>
  void InvokeVoidFunction(F ptr, paramTypes... params)
  {
    if(!GUIInvoke::onUIThread())
    {
      GUIInvoke::blockcall([this, ptr, params...]() { (m_Ctx.*ptr)(params...); });

      return;
    }

    (m_Ctx.*ptr)(params...);
  }

  template <typename R, typename F, typename... paramTypes>
  R InvokeRetFunction(F ptr, paramTypes... params)
  {
    if(!GUIInvoke::onUIThread())
    {
      R ret;
      GUIInvoke::blockcall([this, &ret, ptr, params...]() { ret = (m_Ctx.*ptr)(params...); });

      return ret;
    }

    return (m_Ctx.*ptr)(params...);
  }

  virtual void LoadLogfile(const QString &logFile, const QString &origFilename, bool temporary,
                           bool local) override
  {
    InvokeVoidFunction(&ICaptureContext::LoadLogfile, logFile, origFilename, temporary, local);
  }
  virtual void CloseLogfile() override { InvokeVoidFunction(&ICaptureContext::CloseLogfile); }
  virtual void SetEventID(const QVector<ILogViewer *> &exclude, uint32_t selectedEventID,
                          uint32_t eventID, bool force = false) override
  {
    InvokeVoidFunction(&ICaptureContext::SetEventID, exclude, selectedEventID, eventID, force);
  }
  virtual void RefreshStatus() override { InvokeVoidFunction(&ICaptureContext::RefreshStatus); }
  virtual void AddLogViewer(ILogViewer *viewer) override
  {
    InvokeVoidFunction(&ICaptureContext::AddLogViewer, viewer);
  }
  virtual void RemoveLogViewer(ILogViewer *viewer) override
  {
    InvokeVoidFunction(&ICaptureContext::RemoveLogViewer, viewer);
  }
  virtual void AddMessages(const rdctype::array<DebugMessage> &msgs) override
  {
    InvokeVoidFunction(&ICaptureContext::AddMessages, msgs);
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
  virtual void ShowStatisticsViewer() override
  {
    InvokeVoidFunction(&ICaptureContext::ShowStatisticsViewer);
  }
  virtual void ShowTimelineBar() override { InvokeVoidFunction(&ICaptureContext::ShowTimelineBar); }
  virtual void ShowPythonShell() override { InvokeVoidFunction(&ICaptureContext::ShowPythonShell); }
  virtual IShaderViewer *EditShader(bool customShader, const QString &entryPoint,
                                    const QStringMap &files, IShaderViewer::SaveCallback saveCallback,
                                    IShaderViewer::CloseCallback closeCallback) override
  {
    return InvokeRetFunction<IShaderViewer *>(&ICaptureContext::EditShader, customShader,
                                              entryPoint, files, saveCallback, closeCallback);
  }

  virtual IShaderViewer *DebugShader(const ShaderBindpointMapping *bind,
                                     const ShaderReflection *shader, ShaderStage stage,
                                     ShaderDebugTrace *trace, const QString &debugContext) override
  {
    return InvokeRetFunction<IShaderViewer *>(&ICaptureContext::DebugShader, bind, shader, stage,
                                              trace, debugContext);
  }

  virtual IShaderViewer *ViewShader(const ShaderBindpointMapping *bind,
                                    const ShaderReflection *shader, ShaderStage stage) override
  {
    return InvokeRetFunction<IShaderViewer *>(&ICaptureContext::ViewShader, bind, shader, stage);
  }

  virtual IBufferViewer *ViewBuffer(uint64_t byteOffset, uint64_t byteSize, ResourceId id,
                                    const QString &format = QString()) override
  {
    return InvokeRetFunction<IBufferViewer *>(&ICaptureContext::ViewBuffer, byteOffset, byteSize,
                                              id, format);
  }

  virtual IBufferViewer *ViewTextureAsBuffer(uint32_t arrayIdx, uint32_t mip, ResourceId id,
                                             const QString &format = QString()) override
  {
    return InvokeRetFunction<IBufferViewer *>(&ICaptureContext::ViewTextureAsBuffer, arrayIdx, mip,
                                              id, format);
  }

  virtual IConstantBufferPreviewer *ViewConstantBuffer(ShaderStage stage, uint32_t slot,
                                                       uint32_t idx) override
  {
    return InvokeRetFunction<IConstantBufferPreviewer *>(&ICaptureContext::ViewConstantBuffer,
                                                         stage, slot, idx);
  }

  virtual IPixelHistoryView *ViewPixelHistory(ResourceId texID, int x, int y,
                                              const TextureDisplay &display) override
  {
    return InvokeRetFunction<IPixelHistoryView *>(&ICaptureContext::ViewPixelHistory, texID, x, y,
                                                  display);
  }

  virtual QWidget *CreateBuiltinWindow(const QString &objectName) override
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

  m_ThreadCtx = new CaptureContextInvoker(m_Ctx);

  QObject::connect(ui->lineInput, &RDLineEdit::keyPress, this, &PythonShell::interactive_keypress);

  ui->lineInput->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
  ui->interactiveOutput->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
  ui->scriptOutput->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));

  scriptEditor = new ScintillaEdit(this);

  scriptEditor->styleSetFont(
      STYLE_DEFAULT, QFontDatabase::systemFont(QFontDatabase::FixedFont).family().toUtf8().data());

  scriptEditor->setMarginLeft(4);
  scriptEditor->setMarginWidthN(0, 32);
  scriptEditor->setMarginWidthN(1, 0);
  scriptEditor->setMarginWidthN(2, 16);
  scriptEditor->setObjectName(lit("scriptEditor"));

  scriptEditor->markerSetBack(CURRENT_MARKER, SCINTILLA_COLOUR(240, 128, 128));
  scriptEditor->markerSetBack(CURRENT_MARKER + 1, SCINTILLA_COLOUR(240, 128, 128));
  scriptEditor->markerDefine(CURRENT_MARKER, SC_MARK_SHORTARROW);
  scriptEditor->markerDefine(CURRENT_MARKER + 1, SC_MARK_BACKGROUND);

  ConfigureSyntax(scriptEditor, SCLEX_PYTHON);

  scriptEditor->setTabWidth(4);

  scriptEditor->setScrollWidth(1);
  scriptEditor->setScrollWidthTracking(true);

  scriptEditor->colourise(0, -1);

  QObject::connect(scriptEditor, &ScintillaEdit::modified, [this](int type, int, int, int,
                                                                  const QByteArray &, int, int, int) {
    if(type & (SC_MOD_INSERTTEXT | SC_MOD_DELETETEXT | SC_MOD_BEFOREINSERT | SC_MOD_BEFOREDELETE))
    {
      scriptEditor->markerDeleteAll(CURRENT_MARKER);
      scriptEditor->markerDeleteAll(CURRENT_MARKER + 1);
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

void PythonShell::on_execute_clicked()
{
  QString command = ui->lineInput->text();

  appendText(ui->interactiveOutput, command + lit("\n"));

  if(command.trimmed().length() > 0)
    interactiveContext->executeString(command, true);

  history.push_front(command);
  historyidx = -1;

  ui->lineInput->clear();

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

  if(!filename.isEmpty())
  {
    QFile f(filename);
    if(f.open(QIODevice::ReadOnly | QIODevice::Text))
    {
      scriptEditor->setText(f.readAll().data());
    }
    else
    {
      RDDialog::critical(this, tr("Error loading script"),
                         tr("Couldn't open path %1.").arg(filename));
    }
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
      if(f.open(QIODevice::WriteOnly | QIODevice::Truncate))
      {
        f.write(scriptEditor->getText(scriptEditor->textLength() + 1));
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
  PythonContext *context = newContext();

  ui->scriptOutput->clear();

  QString script = QString::fromUtf8(scriptEditor->getText(scriptEditor->textLength() + 1));

  enableButtons(false);

  LambdaThread *thread = new LambdaThread([this, script, context]() {

    scriptContext = context;
    context->executeString(lit("script.py"), script);
    scriptContext = NULL;

    GUIInvoke::call([this, context]() {
      context->Finish();
      enableButtons(true);
    });
  });

  thread->selfDelete(true);
  thread->start();
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

void PythonShell::exception(const QString &type, const QString &value, QList<QString> frames)
{
  QTextEdit *out = ui->scriptOutput;
  if(QObject::sender() == (QObject *)interactiveContext)
    out = ui->interactiveOutput;

  QString exString;

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

void PythonShell::interactive_keypress(QKeyEvent *event)
{
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

PythonContext *PythonShell::newContext()
{
  PythonContext *ret = new PythonContext();

  QObject::connect(ret, &PythonContext::traceLine, this, &PythonShell::traceLine);
  QObject::connect(ret, &PythonContext::exception, this, &PythonShell::exception);
  QObject::connect(ret, &PythonContext::textOutput, this, &PythonShell::textOutput);

  ret->setGlobal("pyrenderdoc", (ICaptureContext *)m_ThreadCtx);

  return ret;
}
