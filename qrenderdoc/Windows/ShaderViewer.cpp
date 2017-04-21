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
#include <QMenu>
#include <QShortcut>
#include "3rdparty/scintilla/include/SciLexer.h"
#include "3rdparty/scintilla/include/qt/ScintillaEdit.h"
#include "3rdparty/toolwindowmanager/ToolWindowManager.h"
#include "3rdparty/toolwindowmanager/ToolWindowManagerArea.h"
#include "Code/ScintillaSyntax.h"
#include "Widgets/FindReplace.h"
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

ShaderViewer::ShaderViewer(ICaptureContext &ctx, QWidget *parent)
    : QFrame(parent), ui(new Ui::ShaderViewer), m_Ctx(ctx)
{
  ui->setupUi(this);

  // we create this up front so its state stays persistent as much as possible.
  m_FindReplace = new FindReplace(this);

  m_FindResults = MakeEditor("findresults", "", SCLEX_NULL);
  m_FindResults->setReadOnly(true);
  m_FindResults->setWindowTitle("Find Results");

  // remove margins
  m_FindResults->setMarginWidthN(0, 0);
  m_FindResults->setMarginWidthN(1, 0);
  m_FindResults->setMarginWidthN(2, 0);

  QObject::connect(m_FindReplace, &FindReplace::performFind, this, &ShaderViewer::performFind);
  QObject::connect(m_FindReplace, &FindReplace::performFindAll, this, &ShaderViewer::performFindAll);
  QObject::connect(m_FindReplace, &FindReplace::performReplace, this, &ShaderViewer::performReplace);
  QObject::connect(m_FindReplace, &FindReplace::performReplaceAll, this,
                   &ShaderViewer::performReplaceAll);

  ui->docking->addToolWindow(m_FindReplace, ToolWindowManager::NoArea);
  ui->docking->setToolWindowProperties(m_FindReplace, ToolWindowManager::HideOnClose);

  ui->docking->addToolWindow(m_FindResults, ToolWindowManager::NoArea);
  ui->docking->setToolWindowProperties(m_FindResults, ToolWindowManager::HideOnClose);

  {
    m_DisassemblyView =
        MakeEditor("scintillaDisassem", "",
                   m_Ctx.APIProps().pipelineType == GraphicsAPI::Vulkan ? SCLEX_GLSL : SCLEX_HLSL);
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
    ui->docking->setToolWindowProperties(
        m_DisassemblyView,
        ToolWindowManager::HideCloseButton | ToolWindowManager::DisallowFloatWindow);
  }

  ui->docking->setRubberBandLineWidth(50);

  {
    QMenu *snippetsMenu = new QMenu(this);

    QAction *dim = new QAction(tr("Texture Dimensions Global"), this);
    QAction *mip = new QAction(tr("Selected Mip Global"), this);
    QAction *slice = new QAction(tr("Seleted Array Slice / Cubemap Face Global"), this);
    QAction *sample = new QAction(tr("Selected Sample Global"), this);
    QAction *type = new QAction(tr("Texture Type Global"), this);
    QAction *samplers = new QAction(tr("Point && Linear Samplers"), this);
    QAction *resources = new QAction(tr("Texture Resources"), this);

    snippetsMenu->addAction(dim);
    snippetsMenu->addAction(mip);
    snippetsMenu->addAction(slice);
    snippetsMenu->addAction(sample);
    snippetsMenu->addAction(type);
    snippetsMenu->addSeparator();
    snippetsMenu->addAction(samplers);
    snippetsMenu->addAction(resources);

    QObject::connect(dim, &QAction::triggered, this, &ShaderViewer::snippet_textureDimensions);
    QObject::connect(mip, &QAction::triggered, this, &ShaderViewer::snippet_selectedMip);
    QObject::connect(slice, &QAction::triggered, this, &ShaderViewer::snippet_selectedSlice);
    QObject::connect(sample, &QAction::triggered, this, &ShaderViewer::snippet_selectedSample);
    QObject::connect(type, &QAction::triggered, this, &ShaderViewer::snippet_selectedType);
    QObject::connect(samplers, &QAction::triggered, this, &ShaderViewer::snippet_samplers);
    QObject::connect(resources, &QAction::triggered, this, &ShaderViewer::snippet_resources);

    ui->snippets->setMenu(snippetsMenu);
  }

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

  ui->snippets->setVisible(customShader);

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
    QObject::connect(scintilla, &ScintillaEdit::keyPressed, this, &ShaderViewer::editable_keyPressed);

    QObject::connect(scintilla, &ScintillaEdit::modified, [this](int type, int, int, int,
                                                                 const QByteArray &, int, int, int) {
      if(type & (SC_MOD_INSERTTEXT | SC_MOD_DELETETEXT | SC_MOD_BEFOREINSERT | SC_MOD_BEFOREDELETE))
        m_FindState = FindState();
    });

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

  m_Errors = MakeEditor("errors", "", SCLEX_NULL);
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
  ui->docking->setToolWindowProperties(
      m_Errors, ToolWindowManager::HideCloseButton | ToolWindowManager::DisallowFloatWindow);
}

void ShaderViewer::debugShader(const ShaderBindpointMapping *bind, const ShaderReflection *shader,
                               ShaderStage stage, ShaderDebugTrace *trace,
                               const QString &debugContext)
{
  m_Mapping = bind;
  m_ShaderDetails = shader;
  m_Trace = trace;
  m_Stage = stage;

  // no replacing allowed, stay in find mode
  m_FindReplace->allowUserModeChange(false);

  if(!shader || !bind)
    m_Trace = NULL;

  if(trace)
    setWindowTitle(QString("Debugging %1 - %2")
                       .arg(m_Ctx.CurPipelineState().GetShaderName(stage))
                       .arg(debugContext));
  else
    setWindowTitle(m_Ctx.CurPipelineState().GetShaderName(stage));

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

    ui->variables->setColumns({tr("Name"), tr("Type"), tr("Value")});
    ui->variables->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    ui->variables->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    ui->variables->header()->setSectionResizeMode(2, QHeaderView::Stretch);

    ui->constants->setColumns({tr("Name"), tr("Type"), tr("Value")});
    ui->constants->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    ui->constants->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    ui->constants->header()->setSectionResizeMode(2, QHeaderView::Stretch);

    ui->watch->setWindowTitle(tr("Watch"));
    ui->docking->addToolWindow(
        ui->watch, ToolWindowManager::AreaReference(ToolWindowManager::BottomOf,
                                                    ui->docking->areaOf(m_DisassemblyView), 0.25f));
    ui->docking->setToolWindowProperties(
        ui->watch, ToolWindowManager::HideCloseButton | ToolWindowManager::DisallowFloatWindow);

    ui->variables->setWindowTitle(tr("Variables"));
    ui->docking->addToolWindow(
        ui->variables,
        ToolWindowManager::AreaReference(ToolWindowManager::AddTo, ui->docking->areaOf(ui->watch)));
    ui->docking->setToolWindowProperties(
        ui->variables, ToolWindowManager::HideCloseButton | ToolWindowManager::DisallowFloatWindow);

    ui->constants->setWindowTitle(tr("Constants && Resources"));
    ui->docking->addToolWindow(
        ui->constants, ToolWindowManager::AreaReference(ToolWindowManager::LeftOf,
                                                        ui->docking->areaOf(ui->variables), 0.5f));
    ui->docking->setToolWindowProperties(
        ui->constants, ToolWindowManager::HideCloseButton | ToolWindowManager::DisallowFloatWindow);

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
                     &QShortcut::activated, [this]() { ToggleBreakpoint(); });

    SetCurrentStep(0);
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
    ui->inputSig->setColumns(
        {tr("Name"), tr("Index"), tr("Reg"), tr("Type"), tr("SysValue"), tr("Mask"), tr("Used")});
    for(int i = 0; i < ui->inputSig->header()->count(); i++)
      ui->inputSig->header()->setSectionResizeMode(i, QHeaderView::ResizeToContents);

    ui->outputSig->setColumns(
        {tr("Name"), tr("Index"), tr("Reg"), tr("Type"), tr("SysValue"), tr("Mask"), tr("Used")});
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

        ui->inputSig->addTopLevelItem(new RDTreeWidgetItem(
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

        ui->outputSig->addTopLevelItem(new RDTreeWidgetItem(
            {name, semIdx, s.regIndex, TypeString(s), ToQStr(s.systemValue),
             GetComponentString(s.regChannelMask), GetComponentString(s.channelUsedMask)}));
      }
    }

    ui->inputSig->setWindowTitle(tr("Input Signature"));
    ui->docking->addToolWindow(ui->inputSig, ToolWindowManager::AreaReference(
                                                 ToolWindowManager::BottomOf,
                                                 ui->docking->areaOf(m_DisassemblyView), 0.2f));
    ui->docking->setToolWindowProperties(
        ui->inputSig, ToolWindowManager::HideCloseButton | ToolWindowManager::DisallowFloatWindow);

    ui->outputSig->setWindowTitle(tr("Output Signature"));
    ui->docking->addToolWindow(
        ui->outputSig, ToolWindowManager::AreaReference(ToolWindowManager::RightOf,
                                                        ui->docking->areaOf(ui->inputSig), 0.5f));
    ui->docking->setToolWindowProperties(
        ui->outputSig, ToolWindowManager::HideCloseButton | ToolWindowManager::DisallowFloatWindow);
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
  ScintillaEdit *scintilla =
      MakeEditor("scintilla" + name, text,
                 m_Ctx.APIProps().localRenderer == GraphicsAPI::OpenGL ? SCLEX_GLSL : SCLEX_HLSL);
  scintilla->setReadOnly(true);
  scintilla->setWindowTitle(name);
  ((QWidget *)scintilla)->setProperty("name", name);

  QObject::connect(scintilla, &ScintillaEdit::keyPressed, this, &ShaderViewer::readonly_keyPressed);

  ToolWindowManager::AreaReference ref(ToolWindowManager::EmptySpace);

  if(!m_Scintillas.empty())
    ref = ToolWindowManager::AreaReference(ToolWindowManager::AddTo,
                                           ui->docking->areaOf(m_Scintillas[0]));

  ui->docking->addToolWindow(scintilla, ref);
  ui->docking->setToolWindowProperties(
      scintilla, ToolWindowManager::HideCloseButton | ToolWindowManager::DisallowFloatWindow);

  m_Scintillas.push_back(scintilla);

  return scintilla;
}

ScintillaEdit *ShaderViewer::MakeEditor(const QString &name, const QString &text, int lang)
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
  ret->indicSetFore(INDICATOR_REGHIGHLIGHT, SCINTILLA_COLOUR(0, 100, 0));
  ret->indicSetStyle(INDICATOR_REGHIGHLIGHT, INDIC_ROUNDBOX);

  // set up find result highlight style
  ret->indicSetFore(INDICATOR_FINDRESULT, SCINTILLA_COLOUR(200, 200, 127));
  ret->indicSetStyle(INDICATOR_FINDRESULT, INDIC_FULLBOX);
  ret->indicSetAlpha(INDICATOR_FINDRESULT, 50);
  ret->indicSetOutlineAlpha(INDICATOR_FINDRESULT, 80);

  ConfigureSyntax(ret, lang);

  ret->setTabWidth(4);

  ret->setScrollWidth(1);
  ret->setScrollWidthTracking(true);

  ret->colourise(0, -1);

  ret->emptyUndoBuffer();

  return ret;
}

void ShaderViewer::readonly_keyPressed(QKeyEvent *event)
{
  if(event->key() == Qt::Key_F && (event->modifiers() & Qt::ControlModifier))
  {
    m_FindReplace->setReplaceMode(false);
    on_findReplace_clicked();
  }

  if(event->key() == Qt::Key_F3)
  {
    find((event->modifiers() & Qt::ShiftModifier) == 0);
  }
}

void ShaderViewer::editable_keyPressed(QKeyEvent *event)
{
  if(event->key() == Qt::Key_H && (event->modifiers() & Qt::ControlModifier))
  {
    m_FindReplace->setReplaceMode(true);
    on_findReplace_clicked();
  }
}

void ShaderViewer::disassembly_buttonReleased(QMouseEvent *event)
{
  // TODO context menu
}

bool ShaderViewer::stepBack()
{
  if(!m_Trace)
    return false;

  if(CurrentStep() == 0)
    return false;

  SetCurrentStep(CurrentStep() - 1);

  return true;
}

bool ShaderViewer::stepNext()
{
  if(!m_Trace)
    return false;

  if(CurrentStep() + 1 >= m_Trace->states.count)
    return false;

  SetCurrentStep(CurrentStep() + 1);

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
  runTo(-1, true, ShaderEvents::SampleLoadGather);
}

void ShaderViewer::runToNanOrInf()
{
  runTo(-1, true, ShaderEvents::GeneratedNanOrInf);
}

void ShaderViewer::runBack()
{
  runTo(-1, false);
}

void ShaderViewer::run()
{
  runTo(-1, true);
}

void ShaderViewer::runTo(int runToInstruction, bool forward, ShaderEvents condition)
{
  if(!m_Trace)
    return;

  int step = CurrentStep();

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

  SetCurrentStep(step);
}

QString ShaderViewer::stringRep(const ShaderVariable &var, bool useType)
{
  if(ui->intView->isChecked() || (useType && var.type == VarType::Int))
    return RowString(var, 0, VarType::Int);

  if(useType && var.type == VarType::UInt)
    return RowString(var, 0, VarType::UInt);

  return RowString(var, 0, VarType::Float);
}

RDTreeWidgetItem *ShaderViewer::makeResourceRegister(const BindpointMap &bind, uint32_t idx,
                                                     const BoundResource &bound,
                                                     const ShaderResource &res)
{
  QString name = QString(" (%1)").arg(ToQStr(res.name));

  const TextureDescription *tex = m_Ctx.GetTexture(bound.Id);
  const BufferDescription *buf = m_Ctx.GetBuffer(bound.Id);

  if(res.IsSampler)
    return NULL;

  QChar regChar('u');

  if(res.IsReadOnly)
    regChar = QChar('t');

  // %1 = reg prefix (t or u for D3D11)
  // %2 = bind slot
  // %3 = bind set
  // %4 = array index

  const char *fmt = "%1%2";

  if(m_Ctx.APIProps().pipelineType == GraphicsAPI::D3D12)
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

    return new RDTreeWidgetItem({regname + name, "Texture", type});
  }
  else if(buf)
  {
    QString type = QString("%1 - %2").arg(buf->length).arg(ToQStr(buf->name));

    return new RDTreeWidgetItem({regname + name, "Buffer", type});
  }
  else
  {
    return new RDTreeWidgetItem({regname + name, "Resource", "unknown"});
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
  ui->docking->setToolWindowProperties(
      list, ToolWindowManager::HideCloseButton | ToolWindowManager::DisallowFloatWindow);
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

      ensureLineScrolled(m_DisassemblyView, i);
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
          RDTreeWidgetItem *node =
              new RDTreeWidgetItem({ToQStr(m_Trace->cbuffers[i][j].name), "cbuffer",
                                    stringRep(m_Trace->cbuffers[i][j], false)});
          node->setTag(QVariant::fromValue(CBufferTag(i, j)));

          ui->constants->addTopLevelItem(node);
        }
      }
    }

    for(int i = 0; i < m_Trace->inputs.count; i++)
    {
      const ShaderVariable &input = m_Trace->inputs[i];

      if(input.rows > 0 || input.columns > 0)
      {
        RDTreeWidgetItem *node = new RDTreeWidgetItem(
            {ToQStr(input.name), ToQStr(input.type) + " input", stringRep(input, true)});
        node->setTag(QVariant::fromValue(ResourceTag(i)));

        ui->constants->addTopLevelItem(node);
      }
    }

    QMap<BindpointMap, QVector<BoundResource>> rw =
        m_Ctx.CurPipelineState().GetReadWriteResources(m_Stage);
    QMap<BindpointMap, QVector<BoundResource>> ro =
        m_Ctx.CurPipelineState().GetReadOnlyResources(m_Stage);

    bool tree = false;

    for(int i = 0;
        i < m_Mapping->ReadWriteResources.count && m_ShaderDetails->ReadWriteResources.count; i++)
    {
      BindpointMap bind = m_Mapping->ReadWriteResources[i];

      if(!bind.used)
        continue;

      if(bind.arraySize == 1)
      {
        RDTreeWidgetItem *node =
            makeResourceRegister(bind, 0, rw[bind][0], m_ShaderDetails->ReadWriteResources[i]);
        if(node)
          ui->constants->addTopLevelItem(node);
      }
      else
      {
        RDTreeWidgetItem *node =
            new RDTreeWidgetItem({ToQStr(m_ShaderDetails->ReadWriteResources[i].name),
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
        RDTreeWidgetItem *node =
            makeResourceRegister(bind, 0, ro[bind][0], m_ShaderDetails->ReadOnlyResources[i]);
        if(node)
          ui->constants->addTopLevelItem(node);
      }
      else
      {
        RDTreeWidgetItem *node =
            new RDTreeWidgetItem({ToQStr(m_ShaderDetails->ReadOnlyResources[i].name),
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
          new RDTreeWidgetItem({ToQStr(state.registers[i].name), "temporary", ""}));

    for(int i = 0; i < state.indexableTemps.count; i++)
    {
      RDTreeWidgetItem *node = new RDTreeWidgetItem({QString("x%1").arg(i), "indexable", ""});
      for(int t = 0; t < state.indexableTemps[i].count; t++)
        node->addChild(
            new RDTreeWidgetItem({ToQStr(state.indexableTemps[i][t].name), "indexable", ""}));
      ui->variables->addTopLevelItem(node);
    }

    for(int i = 0; i < state.outputs.count; i++)
      ui->variables->addTopLevelItem(
          new RDTreeWidgetItem({ToQStr(state.outputs[i].name), "output", ""}));
  }

  ui->variables->setUpdatesEnabled(false);

  int v = 0;

  for(int i = 0; i < state.registers.count; i++)
  {
    RDTreeWidgetItem *node = ui->variables->topLevelItem(v++);

    node->setText(2, stringRep(state.registers[i], false));
    node->setTag(QVariant::fromValue(state.registers[i]));
  }

  for(int i = 0; i < state.indexableTemps.count; i++)
  {
    RDTreeWidgetItem *node = ui->variables->topLevelItem(v++);

    for(int t = 0; t < state.indexableTemps[i].count; t++)
    {
      RDTreeWidgetItem *child = node->child(t);

      child->setText(2, stringRep(state.indexableTemps[i][t], false));
      child->setTag(QVariant::fromValue(state.indexableTemps[i][t]));
    }
  }

  for(int i = 0; i < state.outputs.count; i++)
  {
    RDTreeWidgetItem *node = ui->variables->topLevelItem(v++);

    node->setText(2, stringRep(state.outputs[i], false));
    node->setTag(QVariant::fromValue(state.outputs[i]));
  }

  ui->variables->setUpdatesEnabled(true);

  // TODO watch registers
}

void ShaderViewer::ensureLineScrolled(ScintillaEdit *s, int line)
{
  int firstLine = s->firstVisibleLine();
  int linesVisible = s->linesOnScreen();

  if(s->isVisible() && (line < firstLine || line > (firstLine + linesVisible)))
    s->scrollCaret();
}

int ShaderViewer::CurrentStep()
{
  return m_CurrentStep;
}

void ShaderViewer::SetCurrentStep(int step)
{
  if(m_Trace && !m_Trace->states.empty())
    m_CurrentStep = qBound(0, step, m_Trace->states.count - 1);
  else
    m_CurrentStep = 0;

  updateDebugging();
}

void ShaderViewer::ToggleBreakpoint(int instruction)
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

void ShaderViewer::ShowErrors(const QString &errors)
{
  if(m_Errors)
  {
    m_Errors->setReadOnly(false);
    m_Errors->setText(errors.toUtf8().data());
    m_Errors->setReadOnly(true);
  }
}

int ShaderViewer::snippetPos()
{
  if(IsD3D(m_Ctx.APIProps().pipelineType))
    return 0;

  if(m_Scintillas.isEmpty())
    return 0;

  QPair<int, int> ver =
      m_Scintillas[0]->findText(SCFIND_REGEXP, "#version.*", 0, m_Scintillas[0]->length());

  if(ver.first < 0)
    return 0;

  return ver.second + 1;
}

void ShaderViewer::insertVulkanUBO()
{
  if(m_Scintillas.isEmpty())
    return;

  m_Scintillas[0]->insertText(snippetPos(),
                              "layout(binding = 0, std140) uniform RENDERDOC_Uniforms\n"
                              "{\n"
                              "    uvec4 TexDim;\n"
                              "    uint SelectedMip;\n"
                              "    int TextureType;\n"
                              "    uint SelectedSliceFace;\n"
                              "    int SelectedSample;\n"
                              "} RENDERDOC;\n\n");
}

void ShaderViewer::snippet_textureDimensions()
{
  if(m_Scintillas.isEmpty())
    return;

  GraphicsAPI api = m_Ctx.APIProps().pipelineType;

  if(IsD3D(api))
  {
    m_Scintillas[0]->insertText(snippetPos(),
                                "// xyz == width, height, depth. w == # mips\n"
                                "uint4 RENDERDOC_TexDim; \n\n");
  }
  else if(api == GraphicsAPI::OpenGL)
  {
    m_Scintillas[0]->insertText(snippetPos(),
                                "// xyz == width, height, depth. w == # mips\n"
                                "uniform uvec4 RENDERDOC_TexDim;\n\n");
  }
  else if(api == GraphicsAPI::Vulkan)
  {
    insertVulkanUBO();
  }

  m_Scintillas[0]->setSelection(0, 0);
}

void ShaderViewer::snippet_selectedMip()
{
  if(m_Scintillas.isEmpty())
    return;

  GraphicsAPI api = m_Ctx.APIProps().pipelineType;

  if(IsD3D(api))
  {
    m_Scintillas[0]->insertText(snippetPos(),
                                "// selected mip in UI\n"
                                "uint RENDERDOC_SelectedMip;\n\n");
  }
  else if(api == GraphicsAPI::OpenGL)
  {
    m_Scintillas[0]->insertText(snippetPos(),
                                "// selected mip in UI\n"
                                "uniform uint RENDERDOC_SelectedMip;\n\n");
  }
  else if(api == GraphicsAPI::Vulkan)
  {
    insertVulkanUBO();
  }

  m_Scintillas[0]->setSelection(0, 0);
}

void ShaderViewer::snippet_selectedSlice()
{
  if(m_Scintillas.isEmpty())
    return;

  GraphicsAPI api = m_Ctx.APIProps().pipelineType;

  if(IsD3D(api))
  {
    m_Scintillas[0]->insertText(snippetPos(),
                                "// selected array slice or cubemap face in UI\n"
                                "uint RENDERDOC_SelectedSliceFace;\n\n");
  }
  else if(api == GraphicsAPI::OpenGL)
  {
    m_Scintillas[0]->insertText(snippetPos(),
                                "// selected array slice or cubemap face in UI\n"
                                "uniform uint RENDERDOC_SelectedSliceFace;\n\n");
  }
  else if(api == GraphicsAPI::Vulkan)
  {
    insertVulkanUBO();
  }

  m_Scintillas[0]->setSelection(0, 0);
}

void ShaderViewer::snippet_selectedSample()
{
  if(m_Scintillas.isEmpty())
    return;

  GraphicsAPI api = m_Ctx.APIProps().pipelineType;

  if(IsD3D(api))
  {
    m_Scintillas[0]->insertText(snippetPos(),
                                "// selected MSAA sample or -numSamples for resolve. See docs\n"
                                "int RENDERDOC_SelectedSample;\n\n");
  }
  else if(api == GraphicsAPI::OpenGL)
  {
    m_Scintillas[0]->insertText(snippetPos(),
                                "// selected MSAA sample or -numSamples for resolve. See docs\n"
                                "uniform int RENDERDOC_SelectedSample;\n\n");
  }
  else if(api == GraphicsAPI::Vulkan)
  {
    insertVulkanUBO();
  }

  m_Scintillas[0]->setSelection(0, 0);
}

void ShaderViewer::snippet_selectedType()
{
  if(m_Scintillas.isEmpty())
    return;

  GraphicsAPI api = m_Ctx.APIProps().pipelineType;

  if(IsD3D(api))
  {
    m_Scintillas[0]->insertText(snippetPos(),
                                "// 1 = 1D, 2 = 2D, 3 = 3D, 4 = Depth, 5 = Depth + Stencil\n"
                                "// 6 = Depth (MS), 7 = Depth + Stencil (MS)\n"
                                "uint RENDERDOC_TextureType;\n\n");
  }
  else if(api == GraphicsAPI::OpenGL)
  {
    m_Scintillas[0]->insertText(snippetPos(),
                                "// 1 = 1D, 2 = 2D, 3 = 3D, 4 = Cube\n"
                                "// 5 = 1DArray, 6 = 2DArray, 7 = CubeArray\n"
                                "// 8 = Rect, 9 = Buffer, 10 = 2DMS\n"
                                "uniform uint RENDERDOC_TextureType;\n\n");
  }
  else if(api == GraphicsAPI::Vulkan)
  {
    insertVulkanUBO();
  }

  m_Scintillas[0]->setSelection(0, 0);
}

void ShaderViewer::snippet_samplers()
{
  if(m_Scintillas.isEmpty())
    return;

  GraphicsAPI api = m_Ctx.APIProps().pipelineType;

  if(IsD3D(api))
  {
    m_Scintillas[0]->insertText(snippetPos(),
                                "// Samplers\n"
                                "SamplerState pointSampler : register(s0);\n"
                                "SamplerState linearSampler : register(s1);\n"
                                "// End Samplers\n\n");

    m_Scintillas[0]->setSelection(0, 0);
  }
}

void ShaderViewer::snippet_resources()
{
  if(m_Scintillas.isEmpty())
    return;

  GraphicsAPI api = m_Ctx.APIProps().pipelineType;

  if(IsD3D(api))
  {
    m_Scintillas[0]->insertText(
        snippetPos(),
        "// Textures\n"
        "Texture1DArray<float4> texDisplayTex1DArray : register(t1);\n"
        "Texture2DArray<float4> texDisplayTex2DArray : register(t2);\n"
        "Texture3D<float4> texDisplayTex3D : register(t3);\n"
        "Texture2DArray<float2> texDisplayTexDepthArray : register(t4);\n"
        "Texture2DArray<uint2> texDisplayTexStencilArray : register(t5);\n"
        "Texture2DMSArray<float2> texDisplayTexDepthMSArray : register(t6);\n"
        "Texture2DMSArray<uint2> texDisplayTexStencilMSArray : register(t7);\n"
        "Texture2DMSArray<float4> texDisplayTex2DMSArray : register(t9);\n"
        "\n"
        "Texture1DArray<uint4> texDisplayUIntTex1DArray : register(t11);\n"
        "Texture2DArray<uint4> texDisplayUIntTex2DArray : register(t12);\n"
        "Texture3D<uint4> texDisplayUIntTex3D : register(t13);\n"
        "Texture2DMSArray<uint4> texDisplayUIntTex2DMSArray : register(t19);\n"
        "\n"
        "Texture1DArray<int4> texDisplayIntTex1DArray : register(t21);\n"
        "Texture2DArray<int4> texDisplayIntTex2DArray : register(t22);\n"
        "Texture3D<int4> texDisplayIntTex3D : register(t23);\n"
        "Texture2DMSArray<int4> texDisplayIntTex2DMSArray : register(t29);\n"
        "// End Textures\n\n\n");
  }
  else if(api == GraphicsAPI::OpenGL)
  {
    m_Scintillas[0]->insertText(snippetPos(),
                                "// Textures\n"
                                "// Unsigned int samplers\n"
                                "layout (binding = 1) uniform usampler1D texUInt1D;\n"
                                "layout (binding = 2) uniform usampler2D texUInt2D;\n"
                                "layout (binding = 3) uniform usampler3D texUInt3D;\n"
                                "// cube = 4\n"
                                "layout (binding = 5) uniform usampler1DArray texUInt1DArray;\n"
                                "layout (binding = 6) uniform usampler2DArray texUInt2DArray;\n"
                                "// cube array = 7\n"
                                "layout (binding = 8) uniform usampler2DRect texUInt2DRect;\n"
                                "layout (binding = 9) uniform usamplerBuffer texUIntBuffer;\n"
                                "layout (binding = 10) uniform usampler2DMS texUInt2DMS;\n"
                                "\n"
                                "// Int samplers\n"
                                "layout (binding = 1) uniform isampler1D texSInt1D;\n"
                                "layout (binding = 2) uniform isampler2D texSInt2D;\n"
                                "layout (binding = 3) uniform isampler3D texSInt3D;\n"
                                "// cube = 4\n"
                                "layout (binding = 5) uniform isampler1DArray texSInt1DArray;\n"
                                "layout (binding = 6) uniform isampler2DArray texSInt2DArray;\n"
                                "// cube array = 7\n"
                                "layout (binding = 8) uniform isampler2DRect texSInt2DRect;\n"
                                "layout (binding = 9) uniform isamplerBuffer texSIntBuffer;\n"
                                "layout (binding = 10) uniform isampler2DMS texSInt2DMS;\n"
                                "\n"
                                "// Floating point samplers\n"
                                "layout (binding = 1) uniform sampler1D tex1D;\n"
                                "layout (binding = 2) uniform sampler2D tex2D;\n"
                                "layout (binding = 3) uniform sampler3D tex3D;\n"
                                "layout (binding = 4) uniform samplerCube texCube;\n"
                                "layout (binding = 5) uniform sampler1DArray tex1DArray;\n"
                                "layout (binding = 6) uniform sampler2DArray tex2DArray;\n"
                                "layout (binding = 7) uniform samplerCubeArray texCubeArray;\n"
                                "layout (binding = 8) uniform sampler2DRect tex2DRect;\n"
                                "layout (binding = 9) uniform samplerBuffer texBuffer;\n"
                                "layout (binding = 10) uniform sampler2DMS tex2DMS;\n"
                                "// End Textures\n\n\n");
  }
  else if(api == GraphicsAPI::Vulkan)
  {
    m_Scintillas[0]->insertText(snippetPos(),
                                "// Textures\n"
                                "// Floating point samplers\n"
                                "layout(binding = 6) uniform sampler1DArray tex1DArray;\n"
                                "layout(binding = 7) uniform sampler2DArray tex2DArray;\n"
                                "layout(binding = 8) uniform sampler3D tex3D;\n"
                                "layout(binding = 9) uniform sampler2DMS tex2DMS;\n"
                                "\n"
                                "// Unsigned int samplers\n"
                                "layout(binding = 11) uniform usampler1DArray texUInt1DArray;\n"
                                "layout(binding = 12) uniform usampler2DArray texUInt2DArray;\n"
                                "layout(binding = 13) uniform usampler3D texUInt3D;\n"
                                "layout(binding = 14) uniform usampler2DMS texUInt2DMS;\n"
                                "\n"
                                "// Int samplers\n"
                                "layout(binding = 16) uniform isampler1DArray texSInt1DArray;\n"
                                "layout(binding = 17) uniform isampler2DArray texSInt2DArray;\n"
                                "layout(binding = 18) uniform isampler3D texSInt3D;\n"
                                "layout(binding = 19) uniform isampler2DMS texSInt2DMS;\n"
                                "// End Textures\n\n\n");
  }

  m_Scintillas[0]->setSelection(0, 0);
}

void ShaderViewer::on_findReplace_clicked()
{
  if(m_FindReplace->isVisible())
  {
    ToolWindowManager::raiseToolWindow(m_FindReplace);
  }
  else
  {
    ui->docking->moveToolWindow(
        m_FindReplace, ToolWindowManager::AreaReference(ToolWindowManager::NewFloatingArea));
    ui->docking->setToolWindowProperties(m_FindReplace, ToolWindowManager::HideOnClose);
  }
  ui->docking->areaOf(m_FindReplace)->parentWidget()->activateWindow();
  m_FindReplace->takeFocus();
}

void ShaderViewer::on_save_clicked()
{
  if(m_Trace)
  {
    m_Ctx.GetPipelineViewer()->SaveShaderFile(m_ShaderDetails);
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

ScintillaEdit *ShaderViewer::currentScintilla()
{
  ScintillaEdit *cur = qobject_cast<ScintillaEdit *>(QApplication::focusWidget());

  if(cur == NULL)
  {
    for(ScintillaEdit *s : m_Scintillas)
    {
      if(s->isVisible())
      {
        cur = s;
        break;
      }
    }
  }

  return cur;
}

ScintillaEdit *ShaderViewer::nextScintilla(ScintillaEdit *cur)
{
  for(int i = 0; i < m_Scintillas.count(); i++)
  {
    if(m_Scintillas[i] == cur)
    {
      if(i + 1 < m_Scintillas.count())
        return m_Scintillas[i + 1];

      return m_Scintillas[0];
    }
  }

  if(!m_Scintillas.isEmpty())
    return m_Scintillas[0];

  return NULL;
}

void ShaderViewer::find(bool down)
{
  ScintillaEdit *cur = currentScintilla();

  if(!cur)
    return;

  QString find = m_FindReplace->findText();

  sptr_t flags = 0;

  if(m_FindReplace->matchCase())
    flags |= SCFIND_MATCHCASE;
  if(m_FindReplace->matchWord())
    flags |= SCFIND_WHOLEWORD;
  if(m_FindReplace->regexp())
    flags |= SCFIND_REGEXP | SCFIND_POSIX;

  FindReplace::SearchContext context = m_FindReplace->context();

  QString findHash = QString("%1%2%3").arg(find).arg(flags).arg((int)context);

  if(findHash != m_FindState.hash)
  {
    m_FindState.hash = findHash;
    m_FindState.start = 0;
    m_FindState.end = cur->length();
    m_FindState.offset = cur->currentPos();
  }

  int start = m_FindState.start + m_FindState.offset;
  int end = m_FindState.end;

  if(!down)
    end = m_FindState.start;

  QPair<int, int> result = cur->findText(flags, find.toUtf8().data(), start, end);

  m_FindState.prevResult = result;

  if(result.first == -1)
  {
    sptr_t maxOffset = down ? 0 : m_FindState.end;

    // if we're at offset 0 searching down, there are no results. Same for offset max and searching
    // up
    if(m_FindState.offset == maxOffset)
      return;

    // otherwise, we can wrap the search around

    if(context == FindReplace::AllFiles)
    {
      cur = nextScintilla(cur);
      ToolWindowManager::raiseToolWindow(cur);
      cur->activateWindow();
      cur->QWidget::setFocus();
    }

    m_FindState.offset = maxOffset;

    start = m_FindState.start + m_FindState.offset;
    end = m_FindState.end;

    if(!down)
      end = m_FindState.start;

    result = cur->findText(flags, find.toUtf8().data(), start, end);

    m_FindState.prevResult = result;

    if(result.first == -1)
      return;
  }

  cur->setSelection(result.first, result.second);

  ensureLineScrolled(cur, cur->lineFromPosition(result.first));

  if(down)
    m_FindState.offset = result.second - m_FindState.start;
  else
    m_FindState.offset = result.first - m_FindState.start;
}

void ShaderViewer::performFind()
{
  find(m_FindReplace->direction() == FindReplace::Down);
}

void ShaderViewer::performFindAll()
{
  ScintillaEdit *cur = currentScintilla();

  if(!cur)
    return;

  QString find = m_FindReplace->findText();

  sptr_t flags = 0;

  QString results = tr("Find all \"%1\"").arg(find);

  if(m_FindReplace->matchCase())
  {
    flags |= SCFIND_MATCHCASE;
    results += tr(", Match case");
  }

  if(m_FindReplace->matchWord())
  {
    flags |= SCFIND_WHOLEWORD;
    results += tr(", Match whole word");
  }

  if(m_FindReplace->regexp())
  {
    flags |= SCFIND_REGEXP | SCFIND_POSIX;
    results += tr(", with Regular Expressions");
  }

  FindReplace::SearchContext context = m_FindReplace->context();

  if(context == FindReplace::File)
    results += tr(", in current file\n");
  else
    results += tr(", in all files\n");

  // trash the find state for any incremental finds
  m_FindState = FindState();

  QList<ScintillaEdit *> scintillas = m_Scintillas;

  if(context == FindReplace::File)
    scintillas = {cur};

  QList<QPair<int, int>> resultList;

  QByteArray findUtf8 = find.toUtf8();

  for(ScintillaEdit *s : scintillas)
  {
    sptr_t start = 0;
    sptr_t end = s->length();

    s->setIndicatorCurrent(INDICATOR_FINDRESULT);
    s->indicatorClearRange(start, end);

    if(findUtf8.isEmpty())
      continue;

    QPair<int, int> result;

    do
    {
      result = s->findText(flags, findUtf8.data(), start, end);

      if(result.first >= 0)
      {
        int line = s->lineFromPosition(result.first);
        sptr_t lineStart = s->positionFromLine(line);
        sptr_t lineEnd = s->lineEndPosition(line);

        s->indicatorFillRange(result.first, result.second - result.first);

        QString lineText = QString::fromUtf8(s->textRange(lineStart, lineEnd));

        results += QString("  %1(%2): ").arg(s->windowTitle()).arg(line, 4);
        int startPos = results.length();

        results += lineText;
        results += "\n";

        resultList.push_back(
            qMakePair(result.first - lineStart + startPos, result.second - lineStart + startPos));
      }

      start = result.second;

    } while(result.first >= 0);
  }

  if(findUtf8.isEmpty())
    return;

  results += tr("Matching lines: %1").arg(resultList.count());

  m_FindResults->setReadOnly(false);
  m_FindResults->setText(results.toUtf8().data());

  m_FindResults->setIndicatorCurrent(INDICATOR_FINDRESULT);

  for(QPair<int, int> r : resultList)
    m_FindResults->indicatorFillRange(r.first, r.second - r.first);

  m_FindResults->setReadOnly(true);

  if(m_FindResults->isVisible())
  {
    ToolWindowManager::raiseToolWindow(m_FindResults);
  }
  else
  {
    ui->docking->moveToolWindow(m_FindResults,
                                ToolWindowManager::AreaReference(ToolWindowManager::BottomOf,
                                                                 ui->docking->areaOf(cur), 0.2f));
    ui->docking->setToolWindowProperties(m_FindResults, ToolWindowManager::HideOnClose);
  }
}

void ShaderViewer::performReplace()
{
  ScintillaEdit *cur = currentScintilla();

  if(!cur)
    return;

  QString find = m_FindReplace->findText();

  if(find.isEmpty())
    return;

  sptr_t flags = 0;

  if(m_FindReplace->matchCase())
    flags |= SCFIND_MATCHCASE;
  if(m_FindReplace->matchWord())
    flags |= SCFIND_WHOLEWORD;
  if(m_FindReplace->regexp())
    flags |= SCFIND_REGEXP | SCFIND_POSIX;

  FindReplace::SearchContext context = m_FindReplace->context();

  QString findHash = QString("%1%2%3").arg(find).arg(flags).arg((int)context);

  // if we didn't have a valid previous find, just do a find and bail
  if(findHash != m_FindState.hash)
  {
    performFind();
    return;
  }

  if(m_FindState.prevResult.first == -1)
    return;

  cur->setTargetRange(m_FindState.prevResult.first, m_FindState.prevResult.second);

  FindState save = m_FindState;

  QString replaceText = m_FindReplace->replaceText();

  // otherwise we have a valid previous find. Do the replace now
  // note this will invalidate the find state (as most user operations would), so we save/restore
  // the state
  if(m_FindReplace->regexp())
    cur->replaceTargetRE(-1, replaceText.toUtf8().data());
  else
    cur->replaceTarget(-1, replaceText.toUtf8().data());

  m_FindState = save;

  // adjust the offset if we replaced text and it went up or down in size
  m_FindState.offset += (replaceText.count() - find.count());

  // move to the next result
  performFind();
}

void ShaderViewer::performReplaceAll()
{
  ScintillaEdit *cur = currentScintilla();

  if(!cur)
    return;

  QString find = m_FindReplace->findText();
  QString replace = m_FindReplace->replaceText();

  if(find.isEmpty())
    return;

  sptr_t flags = 0;

  if(m_FindReplace->matchCase())
    flags |= SCFIND_MATCHCASE;
  if(m_FindReplace->matchWord())
    flags |= SCFIND_WHOLEWORD;
  if(m_FindReplace->regexp())
    flags |= SCFIND_REGEXP | SCFIND_POSIX;

  FindReplace::SearchContext context = m_FindReplace->context();

  (void)context;

  // trash the find state for any incremental finds
  m_FindState = FindState();

  QList<ScintillaEdit *> scintillas = m_Scintillas;

  if(context == FindReplace::File)
    scintillas = {cur};

  int numReplacements = 1;

  for(ScintillaEdit *s : scintillas)
  {
    sptr_t start = 0;
    sptr_t end = s->length();

    QPair<int, int> result;

    QByteArray findUtf8 = find.toUtf8();
    QByteArray replaceUtf8 = replace.toUtf8();

    do
    {
      result = s->findText(flags, findUtf8.data(), start, end);

      if(result.first >= 0)
      {
        s->setTargetRange(result.first, result.second);

        if(m_FindReplace->regexp())
          s->replaceTargetRE(-1, replaceUtf8.data());
        else
          s->replaceTarget(-1, replaceUtf8.data());

        numReplacements++;
      }

      start = result.second + (replaceUtf8.count() - findUtf8.count());

    } while(result.first >= 0);
  }

  RDDialog::information(
      this, tr("Replace all"),
      tr("%1 replacements made in %2 files").arg(numReplacements).arg(scintillas.count()));
}
