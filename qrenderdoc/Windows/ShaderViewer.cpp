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

#include "ShaderViewer.h"
#include <QHBoxLayout>
#include <QListWidget>
#include <QShortcut>
#include "3rdparty/scintilla/include/SciLexer.h"
#include "3rdparty/scintilla/include/qt/ScintillaEdit.h"
#include "3rdparty/toolwindowmanager/ToolWindowManager.h"
#include "Code/ScintillaSyntax.h"
#include "Windows/PipelineState/PipelineStateViewer.h"
#include "ui_ShaderViewer.h"

struct CBufferTag
{
  CBufferTag() {}
  CBufferTag(uint32_t c, uint32_t r) : cb(c), reg(r) {}
  uint32_t cb = 0;
  uint32_t reg = 0;
};

struct ResourceTag
{
  ResourceTag() {}
  ResourceTag(uint32_t r) : reg(r) {}
  uint32_t reg = 0;
};

Q_DECLARE_METATYPE(CBufferTag);
Q_DECLARE_METATYPE(ResourceTag);
Q_DECLARE_METATYPE(ShaderVariable);

ShaderViewer::ShaderViewer(CaptureContext &ctx, QWidget *parent)
    : QFrame(parent), ui(new Ui::ShaderViewer), m_Ctx(ctx)
{
  ui->setupUi(this);

  {
    m_DisassemblyView =
        MakeEditor("scintillaDisassem", "", m_Ctx.APIProps().pipelineType == eGraphicsAPI_Vulkan);
    m_DisassemblyView->setReadOnly(true);
    m_DisassemblyView->setWindowTitle(tr("Disassembly"));

    m_DisassemblyView->usePopUp(SC_POPUP_NEVER);

    QObject::connect(m_DisassemblyView, &ScintillaEdit::keyPressed, this,
                     &ShaderViewer::readonly_keyPressed);

    // C# LightCoral
    m_DisassemblyView->markerSetBack(CURRENT_MARKER, SCINTILLA_COLOUR(240, 128, 128));
    m_DisassemblyView->markerSetBack(CURRENT_MARKER + 1, SCINTILLA_COLOUR(240, 128, 128));
    m_DisassemblyView->markerDefine(CURRENT_MARKER, SC_MARK_SHORTARROW);
    m_DisassemblyView->markerDefine(CURRENT_MARKER + 1, SC_MARK_BACKGROUND);

    // C# LightSlateGray
    m_DisassemblyView->markerSetBack(FINISHED_MARKER, SCINTILLA_COLOUR(119, 136, 153));
    m_DisassemblyView->markerSetBack(FINISHED_MARKER + 1, SCINTILLA_COLOUR(119, 136, 153));
    m_DisassemblyView->markerDefine(FINISHED_MARKER, SC_MARK_ROUNDRECT);
    m_DisassemblyView->markerDefine(FINISHED_MARKER + 1, SC_MARK_BACKGROUND);

    // C# Red
    m_DisassemblyView->markerSetBack(BREAKPOINT_MARKER, SCINTILLA_COLOUR(255, 0, 0));
    m_DisassemblyView->markerSetBack(BREAKPOINT_MARKER + 1, SCINTILLA_COLOUR(255, 0, 0));
    m_DisassemblyView->markerDefine(BREAKPOINT_MARKER, SC_MARK_CIRCLE);
    m_DisassemblyView->markerDefine(BREAKPOINT_MARKER + 1, SC_MARK_BACKGROUND);

    m_Scintillas.push_back(m_DisassemblyView);

    ui->docking->addToolWindow(m_DisassemblyView, ToolWindowManager::EmptySpace);
    ui->docking->setToolWindowProperties(m_DisassemblyView, ToolWindowManager::HideCloseButton);
  }

  ui->docking->setAllowFloatingWindow(false);
  ui->docking->setRubberBandLineWidth(50);

  QVBoxLayout *layout = new QVBoxLayout(this);
  layout->setSpacing(0);
  layout->setMargin(0);
  layout->addWidget(ui->toolbar);
  layout->addWidget(ui->docking);

  m_Ctx.AddLogViewer(this);
}

void ShaderViewer::editShader(bool customShader, const QString &entryPoint, const QStringMap &files)
{
  m_Scintillas.removeOne(m_DisassemblyView);
  ui->docking->removeToolWindow(m_DisassemblyView);

  // hide watch, constants, variables
  ui->watch->hide();
  ui->variables->hide();
  ui->constants->hide();

  // hide debugging toolbar buttons
  ui->stepBack->hide();
  ui->stepNext->hide();
  ui->runToCursor->hide();
  ui->runToSample->hide();
  ui->runToNaNOrInf->hide();
  ui->regFormatSep->hide();
  ui->intView->hide();
  ui->floatView->hide();

  // hide signatures
  ui->inputSig->hide();
  ui->outputSig->hide();

  QString title;

  QWidget *sel = NULL;
  for(const QString &f : files.keys())
  {
    QString name = QFileInfo(f).fileName();
    QString text = files[f];

    ScintillaEdit *scintilla = AddFileScintilla(name, text);

    scintilla->setReadOnly(false);
    QObject::connect(m_DisassemblyView, &ScintillaEdit::keyPressed, this,
                     &ShaderViewer::editable_keyPressed);

    // TODO - shortcuts
    QObject::connect(new QShortcut(QKeySequence(Qt::Key_S | Qt::ControlModifier), scintilla),
                     &QShortcut::activated, this, &ShaderViewer::on_save_clicked);

    QWidget *w = (QWidget *)scintilla;
    w->setProperty("filename", f);

    if(text.contains(entryPoint))
      sel = scintilla;

    if(sel == scintilla || title.isEmpty())
      title = tr("%1 - Edit (%2)").arg(entryPoint).arg(name);
  }

  if(sel != NULL)
    ToolWindowManager::raiseToolWindow(sel);

  setWindowTitle(title);

  if(files.count() > 2)
    addFileList();

  m_Errors = MakeEditor("errors", "", false);
  m_Errors->setReadOnly(true);
  m_Errors->setWindowTitle("Errors");

  // remove margins
  m_Errors->setMarginWidthN(0, 0);
  m_Errors->setMarginWidthN(1, 0);
  m_Errors->setMarginWidthN(2, 0);

  QObject::connect(m_Errors, &ScintillaEdit::keyPressed, this, &ShaderViewer::readonly_keyPressed);

  ui->docking->addToolWindow(
      m_Errors, ToolWindowManager::AreaReference(ToolWindowManager::BottomOf,
                                                 ui->docking->areaOf(m_Scintillas.front()), 0.2f));
  ui->docking->setToolWindowProperties(m_Errors, ToolWindowManager::HideCloseButton);
}

void ShaderViewer::debugShader(const ShaderBindpointMapping *bind, const ShaderReflection *shader,
                               ShaderStageType stage, ShaderDebugTrace *trace,
                               const QString &debugContext)
{
  m_Mapping = bind;
  m_ShaderDetails = shader;
  m_Trace = trace;
  m_Stage = stage;

  if(!shader || !bind)
    m_Trace = NULL;

  if(trace)
    setWindowTitle(
        QString("Debugging %1 - %2").arg(m_Ctx.CurPipelineState.GetShaderName(stage)).arg(debugContext));
  else
    setWindowTitle(m_Ctx.CurPipelineState.GetShaderName(stage));

  if(shader)
  {
    // read-only applies to us too!
    m_DisassemblyView->setReadOnly(false);
    m_DisassemblyView->setText(shader->Disassembly.c_str());
    m_DisassemblyView->setReadOnly(true);
  }

  if(trace)
    QObject::connect(m_DisassemblyView, &ScintillaEdit::buttonReleased, this,
                     &ShaderViewer::disassembly_buttonReleased);

  if(shader && shader->DebugInfo.entryFunc.count > 0 && shader->DebugInfo.files.count > 0)
  {
    if(trace)
      setWindowTitle(
          QString("Debug %1() - %2").arg(ToQStr(shader->DebugInfo.entryFunc)).arg(debugContext));
    else
      setWindowTitle(ToQStr(shader->DebugInfo.entryFunc));

    int fileIdx = 0;

    QWidget *sel = m_DisassemblyView;
    for(auto &f : shader->DebugInfo.files)
    {
      QString name = QFileInfo(ToQStr(f.first)).fileName();
      QString text = ToQStr(f.second);

      ScintillaEdit *scintilla = AddFileScintilla(name, text);

      if(shader->DebugInfo.entryFile >= 0 &&
         shader->DebugInfo.entryFile < shader->DebugInfo.files.count)
      {
        if(fileIdx == shader->DebugInfo.entryFile)
          sel = scintilla;
      }
      else if(text.contains(ToQStr(shader->DebugInfo.entryFunc)))
      {
        sel = scintilla;
      }

      fileIdx++;
    }

    if(trace)
      sel = m_DisassemblyView;

    if(shader->DebugInfo.files.count > 2)
      addFileList();

    ToolWindowManager::raiseToolWindow(sel);
  }

  ui->snippets->hide();

  if(trace)
  {
    // hide signatures
    ui->inputSig->hide();
    ui->outputSig->hide();

    ui->variables->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    ui->variables->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    ui->variables->header()->setSectionResizeMode(2, QHeaderView::Stretch);

    ui->constants->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    ui->constants->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    ui->constants->header()->setSectionResizeMode(2, QHeaderView::Stretch);

    ui->watch->setWindowTitle(tr("Watch"));
    ui->docking->addToolWindow(
        ui->watch, ToolWindowManager::AreaReference(ToolWindowManager::BottomOf,
                                                    ui->docking->areaOf(m_DisassemblyView), 0.25f));
    ui->docking->setToolWindowProperties(ui->watch, ToolWindowManager::HideCloseButton);

    ui->variables->setWindowTitle(tr("Variables"));
    ui->docking->addToolWindow(
        ui->variables,
        ToolWindowManager::AreaReference(ToolWindowManager::AddTo, ui->docking->areaOf(ui->watch)));
    ui->docking->setToolWindowProperties(ui->variables, ToolWindowManager::HideCloseButton);

    ui->constants->setWindowTitle(tr("Constants && Resources"));
    ui->docking->addToolWindow(
        ui->constants, ToolWindowManager::AreaReference(ToolWindowManager::LeftOf,
                                                        ui->docking->areaOf(ui->variables), 0.5f));
    ui->docking->setToolWindowProperties(ui->constants, ToolWindowManager::HideCloseButton);

    m_DisassemblyView->setMarginWidthN(1, 20);

    // display current line in margin 2, distinct from breakpoint in margin 1
    sptr_t markMask = (1 << CURRENT_MARKER) | (1 << FINISHED_MARKER);

    m_DisassemblyView->setMarginMaskN(1, m_DisassemblyView->marginMaskN(1) & ~markMask);
    m_DisassemblyView->setMarginMaskN(2, m_DisassemblyView->marginMaskN(2) | markMask);

    QObject::connect(ui->stepBack, &QToolButton::clicked, this, &ShaderViewer::stepBack);
    QObject::connect(ui->stepNext, &QToolButton::clicked, this, &ShaderViewer::stepNext);
    QObject::connect(ui->runToCursor, &QToolButton::clicked, this, &ShaderViewer::runToCursor);
    QObject::connect(ui->runToSample, &QToolButton::clicked, this, &ShaderViewer::runToSample);
    QObject::connect(ui->runToNaNOrInf, &QToolButton::clicked, this, &ShaderViewer::runToNanOrInf);

    QObject::connect(new QShortcut(QKeySequence(Qt::Key_F10), m_DisassemblyView),
                     &QShortcut::activated, this, &ShaderViewer::stepNext);
    QObject::connect(new QShortcut(QKeySequence(Qt::Key_F10 | Qt::ShiftModifier), m_DisassemblyView),
                     &QShortcut::activated, this, &ShaderViewer::stepBack);
    QObject::connect(
        new QShortcut(QKeySequence(Qt::Key_F10 | Qt::ControlModifier), m_DisassemblyView),
        &QShortcut::activated, this, &ShaderViewer::runToCursor);
    QObject::connect(new QShortcut(QKeySequence(Qt::Key_F5), m_DisassemblyView),
                     &QShortcut::activated, this, &ShaderViewer::run);
    QObject::connect(new QShortcut(QKeySequence(Qt::Key_F5 | Qt::ShiftModifier), m_DisassemblyView),
                     &QShortcut::activated, this, &ShaderViewer::runBack);
    QObject::connect(new QShortcut(QKeySequence(Qt::Key_F9), m_DisassemblyView),
                     &QShortcut::activated, [this]() { toggleBreakpoint(); });

    setCurrentStep(0);
  }
  else
  {
    // hide watch, constants, variables
    ui->watch->hide();
    ui->variables->hide();
    ui->constants->hide();

    // hide debugging toolbar buttons
    ui->stepBack->hide();
    ui->stepNext->hide();
    ui->runToCursor->hide();
    ui->runToSample->hide();
    ui->runToNaNOrInf->hide();
    ui->regFormatSep->hide();
    ui->intView->hide();
    ui->floatView->hide();

    // show input and output signatures
    for(int i = 0; i < ui->inputSig->header()->count(); i++)
      ui->inputSig->header()->setSectionResizeMode(i, QHeaderView::ResizeToContents);

    for(int i = 0; i < ui->outputSig->header()->count(); i++)
      ui->outputSig->header()->setSectionResizeMode(i, QHeaderView::ResizeToContents);

    if(shader)
    {
      for(const SigParameter &s : shader->InputSig)
      {
        QString name = s.varName.count == 0
                           ? ToQStr(s.semanticName)
                           : QString("%1 (%2)").arg(ToQStr(s.varName)).arg(ToQStr(s.semanticName));
        if(s.semanticName.count == 0)
          name = s.varName;

        QString semIdx = s.needSemanticIndex ? QString::number(s.semanticIndex) : "";

        ui->inputSig->addTopLevelItem(makeTreeNode(
            {name, semIdx, s.regIndex, TypeString(s), ToQStr(s.systemValue),
             GetComponentString(s.regChannelMask), GetComponentString(s.channelUsedMask)}));
      }

      bool multipleStreams = false;
      for(const SigParameter &s : shader->OutputSig)
      {
        if(s.stream > 0)
        {
          multipleStreams = true;
          break;
        }
      }

      for(const SigParameter &s : shader->OutputSig)
      {
        QString name = s.varName.count == 0
                           ? ToQStr(s.semanticName)
                           : QString("%1 (%2)").arg(ToQStr(s.varName)).arg(ToQStr(s.semanticName));
        if(s.semanticName.count == 0)
          name = s.varName;

        if(multipleStreams)
          name = QString("Stream %1 : %2").arg(s.stream).arg(name);

        QString semIdx = s.needSemanticIndex ? QString::number(s.semanticIndex) : "";

        ui->outputSig->addTopLevelItem(makeTreeNode(
            {name, semIdx, s.regIndex, TypeString(s), ToQStr(s.systemValue),
             GetComponentString(s.regChannelMask), GetComponentString(s.channelUsedMask)}));
      }
    }

    ui->inputSig->setWindowTitle(tr("Input Signature"));
    ui->docking->addToolWindow(ui->inputSig, ToolWindowManager::AreaReference(
                                                 ToolWindowManager::BottomOf,
                                                 ui->docking->areaOf(m_DisassemblyView), 0.2f));
    ui->docking->setToolWindowProperties(ui->inputSig, ToolWindowManager::HideCloseButton);

    ui->outputSig->setWindowTitle(tr("Output Signature"));
    ui->docking->addToolWindow(
        ui->outputSig, ToolWindowManager::AreaReference(ToolWindowManager::RightOf,
                                                        ui->docking->areaOf(ui->inputSig), 0.5f));
    ui->docking->setToolWindowProperties(ui->outputSig, ToolWindowManager::HideCloseButton);
  }
}

ShaderViewer::~ShaderViewer()
{
  delete m_Trace;

  if(m_CloseCallback)
    m_CloseCallback(&m_Ctx);

  m_Ctx.RemoveLogViewer(this);
  delete ui;
}

void ShaderViewer::OnLogfileLoaded()
{
}

void ShaderViewer::OnLogfileClosed()
{
  ToolWindowManager::closeToolWindow(this);
}

void ShaderViewer::OnEventChanged(uint32_t eventID)
{
}

ScintillaEdit *ShaderViewer::AddFileScintilla(const QString &name, const QString &text)
{
  ScintillaEdit *scintilla = MakeEditor("scintilla" + name, text, true);
  scintilla->setReadOnly(true);
  scintilla->setWindowTitle(name);
  ((QWidget *)scintilla)->setProperty("name", name);

  QObject::connect(scintilla, &ScintillaEdit::keyPressed, this, &ShaderViewer::readonly_keyPressed);

  ToolWindowManager::AreaReference ref(ToolWindowManager::EmptySpace);

  if(!m_Scintillas.empty())
    ref = ToolWindowManager::AreaReference(ToolWindowManager::AddTo,
                                           ui->docking->areaOf(m_Scintillas[0]));

  ui->docking->addToolWindow(scintilla, ref);
  ui->docking->setToolWindowProperties(scintilla, ToolWindowManager::HideCloseButton);

  m_Scintillas.push_back(scintilla);

  return scintilla;
}

ScintillaEdit *ShaderViewer::MakeEditor(const QString &name, const QString &text, bool src)
{
  ScintillaEdit *ret = new ScintillaEdit(this);

  ret->setText(text.toUtf8().data());

  sptr_t numlines = ret->lineCount();

  int margin0width = 30;
  if(numlines > 1000)
    margin0width += 6;
  if(numlines > 10000)
    margin0width += 6;

  ret->setMarginLeft(4);
  ret->setMarginWidthN(0, margin0width);
  ret->setMarginWidthN(1, 0);
  ret->setMarginWidthN(2, 16);
  ret->setObjectName(name);

  // C# DarkGreen
  ret->indicSetFore(4, SCINTILLA_COLOUR(0, 100, 0));
  ret->indicSetStyle(4, INDIC_ROUNDBOX);

  ConfigureSyntax(ret,
                  m_Ctx.APIProps().localRenderer == eGraphicsAPI_OpenGL ? SCLEX_GLSL : SCLEX_HLSL);

  ret->setTabWidth(4);

  ret->setScrollWidth(1);
  ret->setScrollWidthTracking(true);

  ret->colourise(0, -1);

  ret->emptyUndoBuffer();

  return ret;
}

void ShaderViewer::readonly_keyPressed(QKeyEvent *event)
{
  // TODO find
}

void ShaderViewer::editable_keyPressed(QKeyEvent *event)
{
  // TODO replace
}

void ShaderViewer::disassembly_buttonReleased(QMouseEvent *event)
{
  // TODO context menu
}

bool ShaderViewer::stepBack()
{
  if(!m_Trace)
    return false;

  if(currentStep() == 0)
    return false;

  setCurrentStep(currentStep() - 1);

  return true;
}

bool ShaderViewer::stepNext()
{
  if(!m_Trace)
    return false;

  if(currentStep() + 1 >= m_Trace->states.count)
    return false;

  setCurrentStep(currentStep() + 1);

  return true;
}

void ShaderViewer::runToCursor()
{
  if(!m_Trace)
    return;

  sptr_t i = m_DisassemblyView->lineFromPosition(m_DisassemblyView->currentPos());

  for(; i < m_DisassemblyView->lineCount(); i++)
  {
    int line = instructionForLine(i);
    if(line >= 0)
    {
      runTo(line, true);
      break;
    }
  }
}

int ShaderViewer::instructionForLine(sptr_t line)
{
  QString trimmed = m_DisassemblyView->getLine(line).trimmed();

  int colon = trimmed.indexOf(QChar(':'));

  if(colon > 0)
  {
    trimmed.truncate(colon);

    bool ok = false;
    int instruction = trimmed.toInt(&ok);

    if(ok && instruction >= 0)
      return instruction;
  }

  return -1;
}

void ShaderViewer::runToSample()
{
  runTo(-1, true, eShaderDbg_SampleLoadGather);
}

void ShaderViewer::runToNanOrInf()
{
  runTo(-1, true, eShaderDbg_GeneratedNanOrInf);
}

void ShaderViewer::runBack()
{
  runTo(-1, false);
}

void ShaderViewer::run()
{
  runTo(-1, true);
}

void ShaderViewer::runTo(int runToInstruction, bool forward, ShaderDebugStateFlags condition)
{
  if(!m_Trace)
    return;

  int step = currentStep();

  int inc = forward ? 1 : -1;

  bool firstStep = true;

  while(step < m_Trace->states.count)
  {
    if(runToInstruction >= 0 && m_Trace->states[step].nextInstruction == (uint32_t)runToInstruction)
      break;

    if(!firstStep && (m_Trace->states[step + inc].flags & condition))
      break;

    if(!firstStep && m_Breakpoints.contains((int)m_Trace->states[step].nextInstruction))
      break;

    firstStep = false;

    if(step + inc < 0 || step + inc >= m_Trace->states.count)
      break;

    step += inc;
  }

  setCurrentStep(step);
}

QString ShaderViewer::stringRep(const ShaderVariable &var, bool useType)
{
  if(ui->intView->isChecked() || (useType && var.type == eVar_Int))
    return RowString(var, 0, eVar_Int);

  if(useType && var.type == eVar_UInt)
    return RowString(var, 0, eVar_UInt);

  return RowString(var, 0, eVar_Float);
}

QTreeWidgetItem *ShaderViewer::makeResourceRegister(const BindpointMap &bind, uint32_t idx,
                                                    const BoundResource &bound,
                                                    const ShaderResource &res)
{
  QString name = QString(" (%1)").arg(ToQStr(res.name));

  const FetchTexture *tex = m_Ctx.GetTexture(bound.Id);
  const FetchBuffer *buf = m_Ctx.GetBuffer(bound.Id);

  if(res.IsSampler)
    return NULL;

  QChar regChar('u');

  if(res.IsSRV)
    regChar = QChar('t');

  // %1 = reg prefix (t or u for D3D11)
  // %2 = bind slot
  // %3 = bind set
  // %4 = array index

  const char *fmt = "%1%2";

  if(m_Ctx.APIProps().pipelineType == eGraphicsAPI_D3D12)
    fmt = bind.arraySize == 1 ? "t%3:%2" : "t%3:%2[%4]";

  QString regname = QString(fmt).arg(regChar).arg(bind.bind).arg(bind.bindset).arg(idx);

  if(tex)
  {
    QString type = QString("%1x%2x%3[%4] @ %5 - %6")
                       .arg(tex->width)
                       .arg(tex->height)
                       .arg(tex->depth > 1 ? tex->depth : tex->arraysize)
                       .arg(tex->mips)
                       .arg(ToQStr(tex->format.strname))
                       .arg(ToQStr(tex->name));

    return makeTreeNode({regname + name, "Texture", type});
  }
  else if(buf)
  {
    QString type = QString("%1 - %2").arg(buf->length).arg(ToQStr(buf->name));

    return makeTreeNode({regname + name, "Buffer", type});
  }
  else
  {
    return makeTreeNode({regname + name, "Resource", "unknown"});
  }
}

void ShaderViewer::addFileList()
{
  QListWidget *list = new QListWidget(this);
  list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  list->setSelectionMode(QAbstractItemView::SingleSelection);
  QObject::connect(list, &QListWidget::currentRowChanged,
                   [this](int idx) { ToolWindowManager::raiseToolWindow(m_Scintillas[idx]); });
  list->setWindowTitle(tr("File List"));

  for(ScintillaEdit *s : m_Scintillas)
    list->addItem(s->windowTitle());

  ui->docking->addToolWindow(
      list, ToolWindowManager::AreaReference(ToolWindowManager::LeftOf,
                                             ui->docking->areaOf(m_Scintillas.front()), 0.2f));
  ui->docking->setToolWindowProperties(list, ToolWindowManager::HideCloseButton);
}

void ShaderViewer::updateDebugging()
{
  if(!m_Trace || m_CurrentStep < 0 || m_CurrentStep >= m_Trace->states.count)
    return;

  const ShaderDebugState &state = m_Trace->states[m_CurrentStep];

  uint32_t nextInst = state.nextInstruction;
  bool done = false;

  if(m_CurrentStep == m_Trace->states.count - 1)
  {
    nextInst--;
    done = true;
  }

  // add current instruction marker
  m_DisassemblyView->markerDeleteAll(CURRENT_MARKER);
  m_DisassemblyView->markerDeleteAll(CURRENT_MARKER + 1);
  m_DisassemblyView->markerDeleteAll(FINISHED_MARKER);
  m_DisassemblyView->markerDeleteAll(FINISHED_MARKER + 1);

  for(sptr_t i = 0; i < m_DisassemblyView->lineCount(); i++)
  {
    if(QString::fromUtf8(m_DisassemblyView->getLine(i).trimmed())
           .startsWith(QString("%1:").arg(nextInst)))
    {
      m_DisassemblyView->markerAdd(i, done ? FINISHED_MARKER : CURRENT_MARKER);
      m_DisassemblyView->markerAdd(i, done ? FINISHED_MARKER + 1 : CURRENT_MARKER + 1);

      int pos = m_DisassemblyView->positionFromLine(i);
      m_DisassemblyView->setSelection(pos, pos);

      int firstLine = m_DisassemblyView->firstVisibleLine();
      int linesVisible = m_DisassemblyView->linesOnScreen();

      if(m_DisassemblyView->isVisible() && (i < firstLine || i > (firstLine + linesVisible)))
        m_DisassemblyView->scrollCaret();

      break;
    }
  }

  // TODO tooltips
  // hoverTimer_Tick(hoverTimer, new EventArgs());

  if(ui->constants->topLevelItemCount() == 0)
  {
    for(int i = 0; i < m_Trace->cbuffers.count; i++)
    {
      for(int j = 0; j < m_Trace->cbuffers[i].count; j++)
      {
        if(m_Trace->cbuffers[i][j].rows > 0 || m_Trace->cbuffers[i][j].columns > 0)
        {
          QTreeWidgetItem *node = makeTreeNode({ToQStr(m_Trace->cbuffers[i][j].name), "cbuffer",
                                                stringRep(m_Trace->cbuffers[i][j], false)});
          node->setData(0, Qt::UserRole, QVariant::fromValue(CBufferTag(i, j)));

          ui->constants->addTopLevelItem(node);
        }
      }
    }

    for(int i = 0; i < m_Trace->inputs.count; i++)
    {
      const ShaderVariable &input = m_Trace->inputs[i];

      if(input.rows > 0 || input.columns > 0)
      {
        QTreeWidgetItem *node =
            makeTreeNode({ToQStr(input.name), ToQStr(input.type) + " input", stringRep(input, true)});
        node->setData(0, Qt::UserRole, QVariant::fromValue(ResourceTag(i)));

        ui->constants->addTopLevelItem(node);
      }
    }

    QMap<BindpointMap, QVector<BoundResource>> rw =
        m_Ctx.CurPipelineState.GetReadWriteResources(m_Stage);
    QMap<BindpointMap, QVector<BoundResource>> ro =
        m_Ctx.CurPipelineState.GetReadOnlyResources(m_Stage);

    bool tree = false;

    for(int i = 0;
        i < m_Mapping->ReadWriteResources.count && m_ShaderDetails->ReadWriteResources.count; i++)
    {
      BindpointMap bind = m_Mapping->ReadWriteResources[i];

      if(!bind.used)
        continue;

      if(bind.arraySize == 1)
      {
        QTreeWidgetItem *node =
            makeResourceRegister(bind, 0, rw[bind][0], m_ShaderDetails->ReadWriteResources[i]);
        if(node)
          ui->constants->addTopLevelItem(node);
      }
      else
      {
        QTreeWidgetItem *node = makeTreeNode({ToQStr(m_ShaderDetails->ReadWriteResources[i].name),
                                              QString("[%1]").arg(bind.arraySize), ""});

        for(uint32_t a = 0; a < bind.arraySize; a++)
          node->addChild(
              makeResourceRegister(bind, a, rw[bind][a], m_ShaderDetails->ReadWriteResources[i]));

        tree = true;

        ui->constants->addTopLevelItem(node);
      }
    }

    for(int i = 0;
        i < m_Mapping->ReadOnlyResources.count && m_ShaderDetails->ReadOnlyResources.count; i++)
    {
      BindpointMap bind = m_Mapping->ReadOnlyResources[i];

      if(!bind.used)
        continue;

      if(bind.arraySize == 1)
      {
        QTreeWidgetItem *node =
            makeResourceRegister(bind, 0, ro[bind][0], m_ShaderDetails->ReadOnlyResources[i]);
        if(node)
          ui->constants->addTopLevelItem(node);
      }
      else
      {
        QTreeWidgetItem *node = makeTreeNode({ToQStr(m_ShaderDetails->ReadOnlyResources[i].name),
                                              QString("[%1]").arg(bind.arraySize), ""});

        for(uint32_t a = 0; a < bind.arraySize; a++)
          node->addChild(
              makeResourceRegister(bind, a, ro[bind][a], m_ShaderDetails->ReadOnlyResources[i]));

        tree = true;

        ui->constants->addTopLevelItem(node);
      }
    }

    if(tree)
    {
      ui->constants->setIndentation(20);
      ui->constants->setRootIsDecorated(true);
    }
  }

  if(ui->variables->topLevelItemCount() == 0)
  {
    for(int i = 0; i < state.registers.count; i++)
      ui->variables->addTopLevelItem(
          makeTreeNode({ToQStr(state.registers[i].name), "temporary", ""}));

    for(int i = 0; i < state.indexableTemps.count; i++)
    {
      QTreeWidgetItem *node = makeTreeNode({QString("x%1").arg(i), "indexable", ""});
      for(int t = 0; t < state.indexableTemps[i].count; t++)
        node->addChild(makeTreeNode({ToQStr(state.indexableTemps[i][t].name), "indexable", ""}));
      ui->variables->addTopLevelItem(node);
    }

    for(int i = 0; i < state.outputs.count; i++)
      ui->variables->addTopLevelItem(makeTreeNode({ToQStr(state.outputs[i].name), "output", ""}));
  }

  ui->variables->setUpdatesEnabled(false);

  int v = 0;

  for(int i = 0; i < state.registers.count; i++)
  {
    QTreeWidgetItem *node = ui->variables->topLevelItem(v++);

    node->setText(2, stringRep(state.registers[i], false));
    node->setData(0, Qt::UserRole, QVariant::fromValue(state.registers[i]));
  }

  for(int i = 0; i < state.indexableTemps.count; i++)
  {
    QTreeWidgetItem *node = ui->variables->topLevelItem(v++);

    for(int t = 0; t < state.indexableTemps[i].count; t++)
    {
      QTreeWidgetItem *child = node->child(t);

      child->setText(2, stringRep(state.indexableTemps[i][t], false));
      child->setData(0, Qt::UserRole, QVariant::fromValue(state.indexableTemps[i][t]));
    }
  }

  for(int i = 0; i < state.outputs.count; i++)
  {
    QTreeWidgetItem *node = ui->variables->topLevelItem(v++);

    node->setText(2, stringRep(state.outputs[i], false));
    node->setData(0, Qt::UserRole, QVariant::fromValue(state.outputs[i]));
  }

  ui->variables->setUpdatesEnabled(true);

  // TODO watch registers
}

int ShaderViewer::currentStep()
{
  return m_CurrentStep;
}

void ShaderViewer::setCurrentStep(int step)
{
  if(m_Trace && !m_Trace->states.empty())
    m_CurrentStep = qBound(0, step, m_Trace->states.count - 1);
  else
    m_CurrentStep = 0;

  updateDebugging();
}

void ShaderViewer::toggleBreakpoint(int instruction)
{
  sptr_t instLine = -1;

  if(instruction == -1)
  {
    // search forward for an instruction
    instLine = m_DisassemblyView->lineFromPosition(m_DisassemblyView->currentPos());

    for(; instLine < m_DisassemblyView->lineCount(); instLine++)
    {
      instruction = instructionForLine(instLine);

      if(instruction >= 0)
        break;
    }
  }

  if(instruction < 0 || instruction >= m_DisassemblyView->lineCount())
    return;

  if(instLine == -1)
  {
    // find line for this instruction
    for(instLine = 0; instLine < m_DisassemblyView->lineCount(); instLine++)
    {
      int inst = instructionForLine(instLine);

      if(instruction == inst)
        break;
    }

    if(instLine >= m_DisassemblyView->lineCount())
      instLine = -1;
  }

  if(m_Breakpoints.contains(instruction))
  {
    if(instLine >= 0)
    {
      m_DisassemblyView->markerDelete(instLine, BREAKPOINT_MARKER);
      m_DisassemblyView->markerDelete(instLine, BREAKPOINT_MARKER + 1);
    }
    m_Breakpoints.removeOne(instruction);
  }
  else
  {
    if(instLine >= 0)
    {
      m_DisassemblyView->markerAdd(instLine, BREAKPOINT_MARKER);
      m_DisassemblyView->markerAdd(instLine, BREAKPOINT_MARKER + 1);
    }
    m_Breakpoints.push_back(instruction);
  }
}

void ShaderViewer::showErrors(const QString &errors)
{
  if(m_Errors)
  {
    m_Errors->setReadOnly(false);
    m_Errors->setText(errors.toUtf8().data());
    m_Errors->setReadOnly(true);
  }
}

void ShaderViewer::on_findReplace_clicked()
{
}

void ShaderViewer::on_save_clicked()
{
  if(m_Trace)
  {
    m_Ctx.pipelineViewer()->SaveShaderFile(m_ShaderDetails);
    return;
  }

  if(m_SaveCallback)
  {
    QMap<QString, QString> files;
    for(ScintillaEdit *s : m_Scintillas)
    {
      QWidget *w = (QWidget *)s;
      files[w->property("filename").toString()] = QString::fromUtf8(s->getText(s->textLength() + 1));
    }
    m_SaveCallback(&m_Ctx, this, files);
  }
}

void ShaderViewer::on_intView_clicked()
{
  ui->intView->setChecked(true);
  ui->floatView->setChecked(false);

  updateDebugging();
}

void ShaderViewer::on_floatView_clicked()
{
  ui->floatView->setChecked(true);
  ui->intView->setChecked(false);

  updateDebugging();
}
