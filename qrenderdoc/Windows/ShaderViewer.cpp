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
#include <QComboBox>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QListWidget>
#include <QMenu>
#include <QMouseEvent>
#include <QShortcut>
#include <QToolTip>
#include "3rdparty/scintilla/include/SciLexer.h"
#include "3rdparty/scintilla/include/qt/ScintillaEdit.h"
#include "3rdparty/toolwindowmanager/ToolWindowManager.h"
#include "3rdparty/toolwindowmanager/ToolWindowManagerArea.h"
#include "Code/ScintillaSyntax.h"
#include "Widgets/FindReplace.h"
#include "ui_ShaderViewer.h"

namespace
{
struct VariableTag
{
  VariableTag() {}
  VariableTag(VariableCategory c, int i, int a = 0) : cat(c), idx(i), arrayIdx(a) {}
  VariableCategory cat = VariableCategory::Unknown;
  int idx = 0;
  int arrayIdx = 0;

  bool operator==(const VariableTag &o)
  {
    return cat == o.cat && idx == o.idx && arrayIdx == o.arrayIdx;
  }
};
};

Q_DECLARE_METATYPE(VariableTag);

ShaderViewer::ShaderViewer(ICaptureContext &ctx, QWidget *parent)
    : QFrame(parent), ui(new Ui::ShaderViewer), m_Ctx(ctx)
{
  ui->setupUi(this);

  ui->constants->setFont(Formatter::PreferredFont());
  ui->variables->setFont(Formatter::PreferredFont());
  ui->watch->setFont(Formatter::PreferredFont());
  ui->inputSig->setFont(Formatter::PreferredFont());
  ui->outputSig->setFont(Formatter::PreferredFont());

  // we create this up front so its state stays persistent as much as possible.
  m_FindReplace = new FindReplace(this);

  m_FindResults = MakeEditor(lit("findresults"), QString(), SCLEX_NULL);
  m_FindResults->setReadOnly(true);
  m_FindResults->setWindowTitle(lit("Find Results"));

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
        MakeEditor(lit("scintillaDisassem"), QString(),
                   m_Ctx.APIProps().pipelineType == GraphicsAPI::Vulkan ? SCLEX_GLSL : SCLEX_HLSL);
    m_DisassemblyView->setReadOnly(true);

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

    m_DisassemblyFrame = new QWidget(this);
    m_DisassemblyFrame->setWindowTitle(tr("Disassembly"));

    QFrame *disasmToolbar = new QFrame(this);
    disasmToolbar->setFrameShape(QFrame::Panel);
    disasmToolbar->setFrameShadow(QFrame::Raised);

    QHBoxLayout *toolbarlayout = new QHBoxLayout(disasmToolbar);
    toolbarlayout->setSpacing(2);
    toolbarlayout->setContentsMargins(2, 2, 2, 2);

    m_DisassemblyType = new QComboBox(disasmToolbar);
    m_DisassemblyType->setMaxVisibleItems(12);
    m_DisassemblyType->setSizeAdjustPolicy(QComboBox::AdjustToContents);

    toolbarlayout->addWidget(new QLabel(tr("Disassembly type:"), disasmToolbar));
    toolbarlayout->addWidget(m_DisassemblyType);
    toolbarlayout->addItem(new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum));

    QVBoxLayout *framelayout = new QVBoxLayout(m_DisassemblyFrame);
    framelayout->setSpacing(0);
    framelayout->setMargin(0);
    framelayout->addWidget(disasmToolbar);
    framelayout->addWidget(m_DisassemblyView);

    ui->docking->addToolWindow(m_DisassemblyFrame, ToolWindowManager::EmptySpace);
    ui->docking->setToolWindowProperties(m_DisassemblyFrame,
                                         ToolWindowManager::HideCloseButton |
                                             ToolWindowManager::DisallowFloatWindow |
                                             ToolWindowManager::AlwaysDisplayFullTabs);
  }

  ui->docking->setAllowFloatingWindow(false);

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
  ui->docking->removeToolWindow(m_DisassemblyFrame);

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

    m_Ctx.GetMainWindow()->RegisterShortcut(QKeySequence(QKeySequence::Save).toString(), this,
                                            [this]() { on_save_clicked(); });

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

  m_Errors = MakeEditor(lit("errors"), QString(), SCLEX_NULL);
  m_Errors->setReadOnly(true);
  m_Errors->setWindowTitle(lit("Errors"));

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
    setWindowTitle(QFormatStr("Debugging %1 - %2")
                       .arg(m_Ctx.CurPipelineState().GetShaderName(stage))
                       .arg(debugContext));
  else
    setWindowTitle(m_Ctx.CurPipelineState().GetShaderName(stage));

  if(shader)
  {
    m_Ctx.Replay().AsyncInvoke([this](IReplayController *r) {
      rdctype::array<rdctype::str> targets = r->GetDisassemblyTargets();

      rdctype::str disasm = r->DisassembleShader(m_ShaderDetails, "");

      GUIInvoke::call([this, targets, disasm]() {
        QStringList targetNames;
        for(const rdctype::str &t : targets)
          targetNames << ToQStr(t);

        m_DisassemblyType->addItems(targetNames);
        m_DisassemblyType->setCurrentIndex(0);
        QObject::connect(m_DisassemblyType, OverloadedSlot<int>::of(&QComboBox::currentIndexChanged),
                         this, &ShaderViewer::disassemble_typeChanged);

        // read-only applies to us too!
        m_DisassemblyView->setReadOnly(false);
        m_DisassemblyView->setText(disasm.c_str());
        m_DisassemblyView->setReadOnly(true);
      });
    });
  }

  // we always want to highlight words/registers
  QObject::connect(m_DisassemblyView, &ScintillaEdit::buttonReleased, this,
                   &ShaderViewer::disassembly_buttonReleased);

  // suppress the built-in context menu and hook up our own
  if(trace)
  {
    m_DisassemblyView->usePopUp(SC_POPUP_NEVER);

    m_DisassemblyView->setContextMenuPolicy(Qt::CustomContextMenu);
    QObject::connect(m_DisassemblyView, &ScintillaEdit::customContextMenuRequested, this,
                     &ShaderViewer::disassembly_contextMenu);

    m_DisassemblyView->setMouseDwellTime(500);

    QObject::connect(m_DisassemblyView, &ScintillaEdit::dwellStart, this,
                     &ShaderViewer::disasm_tooltipShow);
    QObject::connect(m_DisassemblyView, &ScintillaEdit::dwellEnd, this,
                     &ShaderViewer::disasm_tooltipHide);
  }

  if(shader && shader->DebugInfo.files.count > 0)
  {
    if(trace)
      setWindowTitle(QFormatStr("Debug %1() - %2").arg(ToQStr(shader->EntryPoint)).arg(debugContext));
    else
      setWindowTitle(ToQStr(shader->EntryPoint));

    int fileIdx = 0;

    QWidget *sel = NULL;
    for(auto &f : shader->DebugInfo.files)
    {
      QString name = QFileInfo(ToQStr(f.first)).fileName();
      QString text = ToQStr(f.second);

      ScintillaEdit *scintilla = AddFileScintilla(name, text);

      if(sel == NULL)
        sel = scintilla;

      fileIdx++;
    }

    if(trace || sel == NULL)
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
                                                    ui->docking->areaOf(m_DisassemblyFrame), 0.25f));
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
    QObject::connect(ui->runBack, &QToolButton::clicked, this, &ShaderViewer::runBack);
    QObject::connect(ui->run, &QToolButton::clicked, this, &ShaderViewer::run);
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

    // event filter to pick up tooltip events
    ui->constants->installEventFilter(this);
    ui->variables->installEventFilter(this);
    ui->watch->installEventFilter(this);

    SetCurrentStep(0);

    QObject::connect(ui->watch, &RDTableWidget::keyPress, this, &ShaderViewer::watch_keyPress);

    ui->watch->insertRow(0);

    for(int i = 0; i < ui->watch->columnCount(); i++)
    {
      QTableWidgetItem *item = new QTableWidgetItem();
      if(i > 0)
        item->setFlags(item->flags() & ~Qt::ItemIsEditable);
      ui->watch->setItem(0, i, item);
    }

    ui->watch->resizeRowsToContents();
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
                           : QFormatStr("%1 (%2)").arg(ToQStr(s.varName)).arg(ToQStr(s.semanticName));
        if(s.semanticName.count == 0)
          name = ToQStr(s.varName);

        QString semIdx = s.needSemanticIndex ? QString::number(s.semanticIndex) : QString();

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
                           : QFormatStr("%1 (%2)").arg(ToQStr(s.varName)).arg(ToQStr(s.semanticName));
        if(s.semanticName.count == 0)
          name = ToQStr(s.varName);

        if(multipleStreams)
          name = QFormatStr("Stream %1 : %2").arg(s.stream).arg(name);

        QString semIdx = s.needSemanticIndex ? QString::number(s.semanticIndex) : QString();

        ui->outputSig->addTopLevelItem(new RDTreeWidgetItem(
            {name, semIdx, s.regIndex, TypeString(s), ToQStr(s.systemValue),
             GetComponentString(s.regChannelMask), GetComponentString(s.channelUsedMask)}));
      }
    }

    ui->inputSig->setWindowTitle(tr("Input Signature"));
    ui->docking->addToolWindow(ui->inputSig, ToolWindowManager::AreaReference(
                                                 ToolWindowManager::BottomOf,
                                                 ui->docking->areaOf(m_DisassemblyFrame), 0.2f));
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
  // don't want to async invoke while using 'this', so save the trace separately
  ShaderDebugTrace *trace = m_Trace;

  m_Ctx.Replay().AsyncInvoke([trace](IReplayController *r) { r->FreeTrace(trace); });

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
  ScintillaEdit *scintilla = MakeEditor(
      lit("scintilla") + name, text, IsD3D(m_Ctx.APIProps().localRenderer) ? SCLEX_HLSL : SCLEX_GLSL);
  scintilla->setReadOnly(true);
  scintilla->setWindowTitle(name);
  ((QWidget *)scintilla)->setProperty("name", name);

  QObject::connect(scintilla, &ScintillaEdit::keyPressed, this, &ShaderViewer::readonly_keyPressed);

  ToolWindowManager::AreaReference ref(ToolWindowManager::EmptySpace);

  if(!m_Scintillas.empty())
    ref = ToolWindowManager::AreaReference(ToolWindowManager::AddTo,
                                           ui->docking->areaOf(m_Scintillas[0]));

  ui->docking->addToolWindow(scintilla, ref);
  ui->docking->setToolWindowProperties(scintilla, ToolWindowManager::HideCloseButton |
                                                      ToolWindowManager::DisallowFloatWindow |
                                                      ToolWindowManager::AlwaysDisplayFullTabs);

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

  ret->styleSetFont(STYLE_DEFAULT,
                    QFontDatabase::systemFont(QFontDatabase::FixedFont).family().toUtf8().data());

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

void ShaderViewer::disassembly_contextMenu(const QPoint &pos)
{
  int scintillaPos = m_DisassemblyView->positionFromPoint(pos.x(), pos.y());

  QMenu contextMenu(this);

  QAction intDisplay(tr("Integer register display"), this);
  QAction floatDisplay(tr("Float register display"), this);

  intDisplay.setCheckable(true);
  floatDisplay.setCheckable(true);

  intDisplay.setChecked(ui->intView->isChecked());
  floatDisplay.setChecked(ui->floatView->isChecked());

  QObject::connect(&intDisplay, &QAction::triggered, this, &ShaderViewer::on_intView_clicked);
  QObject::connect(&floatDisplay, &QAction::triggered, this, &ShaderViewer::on_floatView_clicked);

  contextMenu.addAction(&intDisplay);
  contextMenu.addAction(&floatDisplay);
  contextMenu.addSeparator();

  QAction addBreakpoint(tr("Toggle breakpoint here"), this);
  QAction runCursor(tr("Run to Cursor"), this);

  QObject::connect(&addBreakpoint, &QAction::triggered, [this, scintillaPos] {
    m_DisassemblyView->setSelection(scintillaPos, scintillaPos);
    ToggleBreakpoint();
  });
  QObject::connect(&runCursor, &QAction::triggered, [this, scintillaPos] {
    m_DisassemblyView->setSelection(scintillaPos, scintillaPos);
    runToCursor();
  });

  contextMenu.addAction(&addBreakpoint);
  contextMenu.addAction(&runCursor);
  contextMenu.addSeparator();

  QAction copyText(tr("Copy"), this);
  QAction selectAll(tr("Select All"), this);

  copyText.setEnabled(!m_DisassemblyView->selectionEmpty());

  QObject::connect(&copyText, &QAction::triggered, [this] {
    m_DisassemblyView->copyRange(m_DisassemblyView->selectionStart(),
                                 m_DisassemblyView->selectionEnd());
  });
  QObject::connect(&selectAll, &QAction::triggered, [this] { m_DisassemblyView->selectAll(); });

  contextMenu.addAction(&copyText);
  contextMenu.addAction(&selectAll);
  contextMenu.addSeparator();

  RDDialog::show(&contextMenu, m_DisassemblyView->viewport()->mapToGlobal(pos));
}

void ShaderViewer::disassembly_buttonReleased(QMouseEvent *event)
{
  if(event->button() == Qt::LeftButton)
  {
    sptr_t scintillaPos = m_DisassemblyView->positionFromPoint(event->x(), event->y());

    sptr_t start = m_DisassemblyView->wordStartPosition(scintillaPos, true);
    sptr_t end = m_DisassemblyView->wordEndPosition(scintillaPos, true);

    QString text = QString::fromUtf8(m_DisassemblyView->textRange(start, end));

    if(!text.isEmpty())
    {
      VariableTag tag;
      getRegisterFromWord(text, tag.cat, tag.idx, tag.arrayIdx);

      // for now since we don't have friendly naming, only highlight registers
      if(tag.cat != VariableCategory::Unknown)
      {
        start = 0;
        end = m_DisassemblyView->length();

        for(int i = 0; i < ui->variables->topLevelItemCount(); i++)
        {
          RDTreeWidgetItem *item = ui->variables->topLevelItem(i);
          if(item->tag().value<VariableTag>() == tag)
            item->setBackgroundColor(QColor::fromHslF(
                0.333f, 1.0f, qBound(0.25, palette().color(QPalette::Base).lightnessF(), 0.85)));
          else
            item->setBackground(QBrush());
        }

        for(int i = 0; i < ui->constants->topLevelItemCount(); i++)
        {
          RDTreeWidgetItem *item = ui->constants->topLevelItem(i);
          if(item->tag().value<VariableTag>() == tag)
            item->setBackgroundColor(QColor::fromHslF(
                0.333f, 1.0f, qBound(0.25, palette().color(QPalette::Base).lightnessF(), 0.85)));
          else
            item->setBackground(QBrush());
        }

        m_DisassemblyView->setIndicatorCurrent(INDICATOR_REGHIGHLIGHT);
        m_DisassemblyView->indicatorClearRange(start, end);

        sptr_t flags = SCFIND_MATCHCASE | SCFIND_WHOLEWORD;

        if(tag.cat != VariableCategory::Unknown)
        {
          flags |= SCFIND_REGEXP | SCFIND_POSIX;
          text += lit("\\.[xyzwrgba]+");
        }

        QByteArray findUtf8 = text.toUtf8();

        QPair<int, int> result;

        do
        {
          result = m_DisassemblyView->findText(flags, findUtf8.data(), start, end);

          if(result.first >= 0)
            m_DisassemblyView->indicatorFillRange(result.first, result.second - result.first);

          start = result.second;

        } while(result.first >= 0);
      }
    }
  }
}

void ShaderViewer::disassemble_typeChanged(int index)
{
  if(m_ShaderDetails == NULL)
    return;

  QByteArray target = m_DisassemblyType->currentText().toUtf8();

  m_Ctx.Replay().AsyncInvoke([this, target](IReplayController *r) {
    rdctype::str disasm = r->DisassembleShader(m_ShaderDetails, target.data());

    GUIInvoke::call([this, disasm]() {
      m_DisassemblyView->setReadOnly(false);
      m_DisassemblyView->setText(disasm.c_str());
      m_DisassemblyView->setReadOnly(true);
      m_DisassemblyView->emptyUndoBuffer();
    });
  });
}

void ShaderViewer::watch_keyPress(QKeyEvent *event)
{
  if(event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace)
  {
    QList<QTableWidgetItem *> items = ui->watch->selectedItems();
    if(!items.isEmpty() && items.back()->row() < ui->watch->rowCount() - 1)
      ui->watch->removeRow(items.back()->row());
  }
}

void ShaderViewer::on_watch_itemChanged(QTableWidgetItem *item)
{
  // ignore changes to the type/value columns. Only look at name changes, which must be by the user
  if(item->column() != 0)
    return;

  static bool recurse = false;

  if(recurse)
    return;

  recurse = true;

  // if the item is now empty, remove it
  if(item->text().isEmpty())
    ui->watch->removeRow(item->row());

  // ensure we have a trailing row for adding new watch items.

  if(ui->watch->rowCount() == 0 || ui->watch->item(ui->watch->rowCount() - 1, 0) == NULL ||
     !ui->watch->item(ui->watch->rowCount() - 1, 0)->text().isEmpty())
  {
    // add a new row if needed
    if(ui->watch->rowCount() == 0 || ui->watch->item(ui->watch->rowCount() - 1, 0) != NULL)
      ui->watch->insertRow(ui->watch->rowCount());

    for(int i = 0; i < ui->watch->columnCount(); i++)
    {
      QTableWidgetItem *newItem = new QTableWidgetItem();
      if(i > 0)
        newItem->setFlags(newItem->flags() & ~Qt::ItemIsEditable);
      ui->watch->setItem(ui->watch->rowCount() - 1, i, newItem);
    }
  }

  ui->watch->resizeRowsToContents();

  recurse = false;

  updateDebugging();
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
  QString trimmed = QString::fromUtf8(m_DisassemblyView->getLine(line).trimmed());

  int colon = trimmed.indexOf(QLatin1Char(':'));

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
  QString name = QFormatStr(" (%1)").arg(ToQStr(res.name));

  const TextureDescription *tex = m_Ctx.GetTexture(bound.Id);
  const BufferDescription *buf = m_Ctx.GetBuffer(bound.Id);

  if(res.IsSampler)
    return NULL;

  QChar regChar(QLatin1Char('u'));

  if(res.IsReadOnly)
    regChar = QLatin1Char('t');

  QString regname;

  if(m_Ctx.APIProps().pipelineType == GraphicsAPI::D3D12)
  {
    if(bind.arraySize == 1)
      regname = QFormatStr("%1%2:%3").arg(regChar).arg(bind.bindset).arg(bind.bind);
    else
      regname = QFormatStr("%1%2:%3[%4]").arg(regChar).arg(bind.bindset).arg(bind.bind).arg(idx);
  }
  else
  {
    regname = QFormatStr("%1%2").arg(regChar).arg(bind.bind);
  }

  if(tex)
  {
    QString type = QFormatStr("%1x%2x%3[%4] @ %5 - %6")
                       .arg(tex->width)
                       .arg(tex->height)
                       .arg(tex->depth > 1 ? tex->depth : tex->arraysize)
                       .arg(tex->mips)
                       .arg(ToQStr(tex->format.strname))
                       .arg(ToQStr(tex->name));

    return new RDTreeWidgetItem({regname + name, lit("Texture"), type});
  }
  else if(buf)
  {
    QString type = QFormatStr("%1 - %2").arg(buf->length).arg(ToQStr(buf->name));

    return new RDTreeWidgetItem({regname + name, lit("Buffer"), type});
  }
  else
  {
    return new RDTreeWidgetItem({regname + name, lit("Resource"), lit("unknown")});
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
           .startsWith(QFormatStr("%1:").arg(nextInst)))
    {
      m_DisassemblyView->markerAdd(i, done ? FINISHED_MARKER : CURRENT_MARKER);
      m_DisassemblyView->markerAdd(i, done ? FINISHED_MARKER + 1 : CURRENT_MARKER + 1);

      int pos = m_DisassemblyView->positionFromLine(i);
      m_DisassemblyView->setSelection(pos, pos);

      ensureLineScrolled(m_DisassemblyView, i);
      break;
    }
  }

  if(ui->constants->topLevelItemCount() == 0)
  {
    for(int i = 0; i < m_Trace->cbuffers.count; i++)
    {
      for(int j = 0; j < m_Trace->cbuffers[i].count; j++)
      {
        if(m_Trace->cbuffers[i][j].rows > 0 || m_Trace->cbuffers[i][j].columns > 0)
        {
          RDTreeWidgetItem *node =
              new RDTreeWidgetItem({ToQStr(m_Trace->cbuffers[i][j].name), lit("cbuffer"),
                                    stringRep(m_Trace->cbuffers[i][j], false)});
          node->setTag(QVariant::fromValue(VariableTag(VariableCategory::Constants, j, i)));

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
            {ToQStr(input.name), ToQStr(input.type) + lit(" input"), stringRep(input, true)});
        node->setTag(QVariant::fromValue(VariableTag(VariableCategory::Inputs, i)));

        ui->constants->addTopLevelItem(node);
      }
    }

    QMap<BindpointMap, QVector<BoundResource>> rw =
        m_Ctx.CurPipelineState().GetReadWriteResources(m_Stage);
    QMap<BindpointMap, QVector<BoundResource>> ro =
        m_Ctx.CurPipelineState().GetReadOnlyResources(m_Stage);

    bool tree = false;

    for(int i = 0;
        i < m_Mapping->ReadWriteResources.count && i < m_ShaderDetails->ReadWriteResources.count; i++)
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
                                  QFormatStr("[%1]").arg(bind.arraySize), QString()});

        for(uint32_t a = 0; a < bind.arraySize; a++)
          node->addChild(
              makeResourceRegister(bind, a, rw[bind][a], m_ShaderDetails->ReadWriteResources[i]));

        tree = true;

        ui->constants->addTopLevelItem(node);
      }
    }

    for(int i = 0;
        i < m_Mapping->ReadOnlyResources.count && i < m_ShaderDetails->ReadOnlyResources.count; i++)
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
                                  QFormatStr("[%1]").arg(bind.arraySize), QString()});

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
          new RDTreeWidgetItem({ToQStr(state.registers[i].name), lit("temporary"), QString()}));

    for(int i = 0; i < state.indexableTemps.count; i++)
    {
      RDTreeWidgetItem *node =
          new RDTreeWidgetItem({QFormatStr("x%1").arg(i), lit("indexable"), QString()});
      for(int t = 0; t < state.indexableTemps[i].count; t++)
        node->addChild(new RDTreeWidgetItem(
            {ToQStr(state.indexableTemps[i][t].name), lit("indexable"), QString()}));
      ui->variables->addTopLevelItem(node);
    }

    for(int i = 0; i < state.outputs.count; i++)
      ui->variables->addTopLevelItem(
          new RDTreeWidgetItem({ToQStr(state.outputs[i].name), lit("output"), QString()}));
  }

  ui->variables->setUpdatesEnabled(false);

  int v = 0;

  for(int i = 0; i < state.registers.count; i++)
  {
    RDTreeWidgetItem *node = ui->variables->topLevelItem(v++);

    node->setText(2, stringRep(state.registers[i], false));
    node->setTag(QVariant::fromValue(VariableTag(VariableCategory::Temporaries, i)));
  }

  for(int i = 0; i < state.indexableTemps.count; i++)
  {
    RDTreeWidgetItem *node = ui->variables->topLevelItem(v++);

    for(int t = 0; t < state.indexableTemps[i].count; t++)
    {
      RDTreeWidgetItem *child = node->child(t);

      child->setText(2, stringRep(state.indexableTemps[i][t], false));
      child->setTag(QVariant::fromValue(VariableTag(VariableCategory::IndexTemporaries, t, i)));
    }
  }

  for(int i = 0; i < state.outputs.count; i++)
  {
    RDTreeWidgetItem *node = ui->variables->topLevelItem(v++);

    node->setText(2, stringRep(state.outputs[i], false));
    node->setTag(QVariant::fromValue(VariableTag(VariableCategory::Outputs, i)));
  }

  ui->variables->setUpdatesEnabled(true);

  ui->watch->setUpdatesEnabled(false);

  for(int i = 0; i < ui->watch->rowCount() - 1; i++)
  {
    QTableWidgetItem *item = ui->watch->item(i, 0);
    ui->watch->setItem(i, 1, new QTableWidgetItem(tr("register", "watch type")));

    QString reg = item->text().trimmed();

    QRegularExpression regexp(lit("^([rvo])([0-9]+)(\\.[xyzwrgba]+)?(,[xfiudb])?$"));

    QRegularExpressionMatch match = regexp.match(reg);

    // try indexable temps
    if(!match.hasMatch())
    {
      regexp = QRegularExpression(lit("^(x[0-9]+)\\[([0-9]+)\\](\\.[xyzwrgba]+)?(,[xfiudb])?$"));

      match = regexp.match(reg);
    }

    if(match.hasMatch())
    {
      QString regtype = match.captured(1);
      QString regidx = match.captured(2);
      QString swizzle = match.captured(3).replace(QLatin1Char('.'), QString());
      QString regcast = match.captured(4).replace(QLatin1Char(','), QString());

      if(regcast.isEmpty())
      {
        if(ui->intView->isChecked())
          regcast = lit("i");
        else
          regcast = lit("f");
      }

      VariableCategory varCat = VariableCategory::Unknown;
      int arrIndex = -1;

      bool ok = false;

      if(regtype == lit("r"))
      {
        varCat = VariableCategory::Temporaries;
      }
      else if(regtype == lit("v"))
      {
        varCat = VariableCategory::Inputs;
      }
      else if(regtype == lit("o"))
      {
        varCat = VariableCategory::Outputs;
      }
      else if(regtype[0] == QLatin1Char('x'))
      {
        varCat = VariableCategory::IndexTemporaries;
        QString tempArrayIndexStr = regtype.mid(1);
        arrIndex = tempArrayIndexStr.toInt(&ok);

        if(!ok)
          arrIndex = -1;
      }

      const rdctype::array<ShaderVariable> *vars = GetVariableList(varCat, arrIndex);

      ok = false;
      int regindex = regidx.toInt(&ok);

      if(vars && ok && regindex >= 0 && regindex < vars->count)
      {
        const ShaderVariable &vr = vars->elems[regindex];

        if(swizzle.isEmpty())
        {
          swizzle = lit("xyzw").left((int)vr.columns);

          if(regcast == lit("d") && swizzle.count() > 2)
            swizzle = lit("xy");
        }

        QString val;

        for(int s = 0; s < swizzle.count(); s++)
        {
          QChar swiz = swizzle[s];

          int elindex = 0;
          if(swiz == QLatin1Char('x') || swiz == QLatin1Char('r'))
            elindex = 0;
          if(swiz == QLatin1Char('y') || swiz == QLatin1Char('g'))
            elindex = 1;
          if(swiz == QLatin1Char('z') || swiz == QLatin1Char('b'))
            elindex = 2;
          if(swiz == QLatin1Char('w') || swiz == QLatin1Char('a'))
            elindex = 3;

          if(regcast == lit("i"))
          {
            val += Formatter::Format(vr.value.iv[elindex]);
          }
          else if(regcast == lit("f"))
          {
            val += Formatter::Format(vr.value.fv[elindex]);
          }
          else if(regcast == lit("u"))
          {
            val += Formatter::Format(vr.value.uv[elindex]);
          }
          else if(regcast == lit("x"))
          {
            val += Formatter::Format(vr.value.uv[elindex], true);
          }
          else if(regcast == lit("b"))
          {
            val += QFormatStr("%1").arg(vr.value.uv[elindex], 32, 2, QLatin1Char('0'));
          }
          else if(regcast == lit("d"))
          {
            if(elindex < 2)
              val += Formatter::Format(vr.value.dv[elindex]);
            else
              val += lit("-");
          }

          if(s < swizzle.count() - 1)
            val += lit(", ");
        }

        item = new QTableWidgetItem(val);
        item->setData(Qt::UserRole, QVariant::fromValue(VariableTag(varCat, regindex, arrIndex)));

        ui->watch->setItem(i, 2, item);

        continue;
      }
    }

    ui->watch->setItem(i, 2, new QTableWidgetItem(tr("Error evaluating expression")));
  }

  ui->watch->setUpdatesEnabled(true);

  updateVariableTooltip();
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

bool ShaderViewer::eventFilter(QObject *watched, QEvent *event)
{
  if(event->type() == QEvent::ToolTip)
  {
    QHelpEvent *he = (QHelpEvent *)event;

    RDTreeWidget *tree = qobject_cast<RDTreeWidget *>(watched);
    if(tree)
    {
      RDTreeWidgetItem *item = tree->itemAt(tree->viewport()->mapFromGlobal(QCursor::pos()));
      if(item)
      {
        VariableTag tag = item->tag().value<VariableTag>();
        showVariableTooltip(tag.cat, tag.idx, tag.arrayIdx);
      }
    }

    RDTableWidget *table = qobject_cast<RDTableWidget *>(watched);
    if(table)
    {
      QTableWidgetItem *item = table->itemAt(table->viewport()->mapFromGlobal(QCursor::pos()));
      if(item)
      {
        item = table->item(item->row(), 2);
        VariableTag tag = item->data(Qt::UserRole).value<VariableTag>();
        showVariableTooltip(tag.cat, tag.idx, tag.arrayIdx);
      }
    }
  }
  if(event->type() == QEvent::MouseMove || event->type() == QEvent::Leave)
  {
    hideVariableTooltip();
  }

  return QFrame::eventFilter(watched, event);
}

void ShaderViewer::disasm_tooltipShow(int x, int y)
{
  // do nothing if there's no trace
  if(!m_Trace || m_CurrentStep < 0 || m_CurrentStep >= m_Trace->states.count)
    return;

  // ignore any messages if we're already outside the viewport
  if(!m_DisassemblyView->rect().contains(m_DisassemblyView->mapFromGlobal(QCursor::pos())))
    return;

  if(m_DisassemblyView->isVisible())
  {
    sptr_t scintillaPos = m_DisassemblyView->positionFromPoint(x, y);

    sptr_t start = m_DisassemblyView->wordStartPosition(scintillaPos, true);
    sptr_t end = m_DisassemblyView->wordEndPosition(scintillaPos, true);

    QString text = QString::fromUtf8(m_DisassemblyView->textRange(start, end));

    if(!text.isEmpty())
    {
      VariableTag tag;
      getRegisterFromWord(text, tag.cat, tag.idx, tag.arrayIdx);

      if(tag.cat != VariableCategory::Unknown && tag.idx >= 0 && tag.arrayIdx >= 0)
        showVariableTooltip(tag.cat, tag.idx, tag.arrayIdx);
    }
  }
}

void ShaderViewer::disasm_tooltipHide(int x, int y)
{
  hideVariableTooltip();
}

void ShaderViewer::showVariableTooltip(VariableCategory varCat, int varIdx, int arrayIdx)
{
  const rdctype::array<ShaderVariable> *vars = GetVariableList(varCat, arrayIdx);

  if(!vars || varIdx < 0 || varIdx >= vars->count)
  {
    m_TooltipVarIdx = -1;
    return;
  }

  m_TooltipVarCat = varCat;
  m_TooltipVarIdx = varIdx;
  m_TooltipArrayIdx = arrayIdx;
  m_TooltipPos = QCursor::pos();

  updateVariableTooltip();
}

const rdctype::array<ShaderVariable> *ShaderViewer::GetVariableList(VariableCategory varCat,
                                                                    int arrayIdx)
{
  const rdctype::array<ShaderVariable> *vars = NULL;

  if(!m_Trace || m_CurrentStep < 0 || m_CurrentStep >= m_Trace->states.count)
    return vars;

  const ShaderDebugState &state = m_Trace->states[m_CurrentStep];

  arrayIdx = qMax(0, arrayIdx);

  switch(varCat)
  {
    case VariableCategory::Unknown: vars = NULL; break;
    case VariableCategory::Temporaries: vars = &state.registers; break;
    case VariableCategory::IndexTemporaries:
      vars = arrayIdx < state.indexableTemps.count ? &state.indexableTemps[arrayIdx] : NULL;
      break;
    case VariableCategory::Inputs: vars = &m_Trace->inputs; break;
    case VariableCategory::Constants:
      vars = arrayIdx < m_Trace->cbuffers.count ? &m_Trace->cbuffers[arrayIdx] : NULL;
      break;
    case VariableCategory::Outputs: vars = &state.outputs; break;
  }

  return vars;
}

void ShaderViewer::getRegisterFromWord(const QString &text, VariableCategory &varCat, int &varIdx,
                                       int &arrayIdx)
{
  QChar regtype = text[0];
  QString regidx = text.mid(1);

  varCat = VariableCategory::Unknown;
  varIdx = -1;
  arrayIdx = 0;

  if(regtype == QLatin1Char('r'))
    varCat = VariableCategory::Temporaries;
  else if(regtype == QLatin1Char('v'))
    varCat = VariableCategory::Inputs;
  else if(regtype == QLatin1Char('o'))
    varCat = VariableCategory::Outputs;
  else
    return;

  bool ok = false;
  varIdx = regidx.toInt(&ok);

  // if we have a list of registers and the index is in range, and we matched the whole word
  // (i.e. v0foo is not the same as v0), then show the tooltip
  if(QFormatStr("%1%2").arg(regtype).arg(varIdx) != text)
  {
    varCat = VariableCategory::Unknown;
    varIdx = -1;
  }
}

void ShaderViewer::updateVariableTooltip()
{
  if(m_TooltipVarIdx < 0)
    return;

  const rdctype::array<ShaderVariable> *vars = GetVariableList(m_TooltipVarCat, m_TooltipArrayIdx);
  const ShaderVariable &var = vars->elems[m_TooltipVarIdx];

  QString text = QFormatStr("<pre>%1\n").arg(ToQStr(var.name));
  text +=
      lit("                 X          Y          Z          W \n"
          "----------------------------------------------------\n");

  text += QFormatStr("float | %1 %2 %3 %4\n")
              .arg(Formatter::Format(var.value.fv[0]), 10)
              .arg(Formatter::Format(var.value.fv[1]), 10)
              .arg(Formatter::Format(var.value.fv[2]), 10)
              .arg(Formatter::Format(var.value.fv[3]), 10);
  text += QFormatStr("uint  | %1 %2 %3 %4\n")
              .arg(var.value.uv[0], 10, 10, QLatin1Char(' '))
              .arg(var.value.uv[1], 10, 10, QLatin1Char(' '))
              .arg(var.value.uv[2], 10, 10, QLatin1Char(' '))
              .arg(var.value.uv[3], 10, 10, QLatin1Char(' '));
  text += QFormatStr("int   | %1 %2 %3 %4\n")
              .arg(var.value.iv[0], 10, 10, QLatin1Char(' '))
              .arg(var.value.iv[1], 10, 10, QLatin1Char(' '))
              .arg(var.value.iv[2], 10, 10, QLatin1Char(' '))
              .arg(var.value.iv[3], 10, 10, QLatin1Char(' '));
  text += QFormatStr("hex   |   %1   %2   %3   %4")
              .arg(Formatter::HexFormat(var.value.uv[0], 4))
              .arg(Formatter::HexFormat(var.value.uv[1], 4))
              .arg(Formatter::HexFormat(var.value.uv[2], 4))
              .arg(Formatter::HexFormat(var.value.uv[3], 4));

  text += lit("</pre>");

  QToolTip::showText(m_TooltipPos, text);
}

void ShaderViewer::hideVariableTooltip()
{
  QToolTip::hideText();
  m_TooltipVarIdx = -1;
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

  QString findHash = QFormatStr("%1%2%3").arg(find).arg(flags).arg((int)context);

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

        results += QFormatStr("  %1(%2): ").arg(s->windowTitle()).arg(line, 4);
        int startPos = results.length();

        results += lineText;
        results += lit("\n");

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

  QString findHash = QFormatStr("%1%2%3").arg(find).arg(flags).arg((int)context);

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
