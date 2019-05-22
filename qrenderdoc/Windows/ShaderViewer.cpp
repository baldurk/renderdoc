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
  ui->registers->setFont(Formatter::PreferredFont());
  ui->locals->setFont(Formatter::PreferredFont());
  ui->watch->setFont(Formatter::PreferredFont());
  ui->inputSig->setFont(Formatter::PreferredFont());
  ui->outputSig->setFont(Formatter::PreferredFont());
  ui->callstack->setFont(Formatter::PreferredFont());

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

    m_Scintillas.push_back(m_DisassemblyView);

    m_DisassemblyFrame = new QWidget(this);
    m_DisassemblyFrame->setWindowTitle(tr("Disassembly"));

    m_DisassemblyToolbar = new QFrame(this);
    m_DisassemblyToolbar->setFrameShape(QFrame::Panel);
    m_DisassemblyToolbar->setFrameShadow(QFrame::Raised);

    QHBoxLayout *toolbarlayout = new QHBoxLayout(m_DisassemblyToolbar);
    toolbarlayout->setSpacing(2);
    toolbarlayout->setContentsMargins(3, 3, 3, 3);

    m_DisassemblyType = new QComboBox(m_DisassemblyToolbar);
    m_DisassemblyType->setMaxVisibleItems(12);
    m_DisassemblyType->setSizeAdjustPolicy(QComboBox::AdjustToContents);

    toolbarlayout->addWidget(new QLabel(tr("Disassembly type:"), m_DisassemblyToolbar));
    toolbarlayout->addWidget(m_DisassemblyType);
    toolbarlayout->addItem(new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum));

    QVBoxLayout *framelayout = new QVBoxLayout(m_DisassemblyFrame);
    framelayout->setSpacing(0);
    framelayout->setMargin(0);
    framelayout->addWidget(m_DisassemblyToolbar);
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
  layout->setSpacing(3);
  layout->setContentsMargins(3, 3, 3, 3);
  layout->addWidget(ui->toolbar);
  layout->addWidget(ui->docking);

  m_Ctx.AddCaptureViewer(this);
}

void ShaderViewer::editShader(bool customShader, ShaderStage stage, const QString &entryPoint,
                              const rdcstrpairs &files, ShaderEncoding shaderEncoding,
                              ShaderCompileFlags flags)
{
  m_Scintillas.removeOne(m_DisassemblyView);
  ui->docking->removeToolWindow(m_DisassemblyFrame);

  m_DisassemblyView = NULL;

  m_Stage = stage;
  m_Flags = flags;

  m_CustomShader = customShader;

  // set up compilation parameters
  for(ShaderEncoding i : values<ShaderEncoding>())
    if(IsTextRepresentation(i) || shaderEncoding == i)
      m_Encodings << i;

  QStringList strs;
  strs.clear();
  for(ShaderEncoding i : m_Encodings)
    strs << ToQStr(i);

  ui->encoding->addItems(strs);
  ui->encoding->setCurrentIndex(m_Encodings.indexOf(shaderEncoding));
  ui->entryFunc->setText(entryPoint);

  PopulateCompileTools();

  QObject::connect(ui->encoding, OverloadedSlot<int>::of(&QComboBox::currentIndexChanged),
                   [this](int) { PopulateCompileTools(); });
  QObject::connect(ui->compileTool, OverloadedSlot<int>::of(&QComboBox::currentIndexChanged),
                   [this](int) { PopulateCompileToolParameters(); });

  // if it's a custom shader, hide the group entirely (don't allow customisation of compile
  // parameters). We can still use it to store the parameters passed in. When visible we collapse it
  // by default.
  if(customShader)
    ui->compilationGroup->hide();

  // hide debugging windows
  ui->watch->hide();
  ui->registers->hide();
  ui->constants->hide();
  ui->callstack->hide();
  ui->locals->hide();

  ui->snippets->setVisible(customShader);

  // hide debugging toolbar buttons
  ui->debugSep->hide();
  ui->runBack->hide();
  ui->run->hide();
  ui->stepBack->hide();
  ui->stepNext->hide();
  ui->runToCursor->hide();
  ui->runToSample->hide();
  ui->runToNaNOrInf->hide();
  ui->regFormatSep->hide();
  ui->intView->hide();
  ui->floatView->hide();
  ui->debugToggleSep->hide();
  ui->debugToggle->hide();

  // hide signatures
  ui->inputSig->hide();
  ui->outputSig->hide();

  QString title;

  QWidget *sel = NULL;
  for(const rdcstrpair &kv : files)
  {
    QString name = QFileInfo(kv.first).fileName();
    QString text = kv.second;

    ScintillaEdit *scintilla = AddFileScintilla(name, text, shaderEncoding);

    scintilla->setReadOnly(false);
    QObject::connect(scintilla, &ScintillaEdit::keyPressed, this, &ShaderViewer::editable_keyPressed);

    QObject::connect(scintilla, &ScintillaEdit::modified, [this](int type, int, int, int,
                                                                 const QByteArray &, int, int, int) {
      if(type & (SC_MOD_INSERTTEXT | SC_MOD_DELETETEXT | SC_MOD_BEFOREINSERT | SC_MOD_BEFOREDELETE))
        m_FindState = FindState();
    });

    m_Ctx.GetMainWindow()->RegisterShortcut(QKeySequence(QKeySequence::Refresh).toString(), this,
                                            [this](QWidget *) { on_refresh_clicked(); });
    ui->refresh->setToolTip(ui->refresh->toolTip() +
                            lit(" (%1)").arg(QKeySequence(QKeySequence::Refresh).toString()));

    QWidget *w = (QWidget *)scintilla;
    w->setProperty("filename", kv.first);

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

  if(!customShader)
  {
    ui->compilationGroup->setWindowTitle(tr("Compilation Settings"));
    ui->docking->addToolWindow(ui->compilationGroup,
                               ToolWindowManager::AreaReference(
                                   ToolWindowManager::LeftOf, ui->docking->areaOf(m_Errors), 0.5f));
    ui->docking->setToolWindowProperties(
        ui->compilationGroup,
        ToolWindowManager::HideCloseButton | ToolWindowManager::DisallowFloatWindow);
  }
}

void ShaderViewer::debugShader(const ShaderBindpointMapping *bind, const ShaderReflection *shader,
                               ResourceId pipeline, ShaderDebugTrace *trace,
                               const QString &debugContext)
{
  m_Mapping = bind;
  m_ShaderDetails = shader;
  m_Pipeline = pipeline;
  m_Trace = trace;
  m_Stage = ShaderStage::Vertex;
  m_DebugContext = debugContext;

  // no recompilation happening, hide that group
  ui->compilationGroup->hide();

  // no replacing allowed, stay in find mode
  m_FindReplace->allowUserModeChange(false);

  if(!m_ShaderDetails || !m_Mapping)
    m_Trace = NULL;

  if(m_ShaderDetails)
  {
    m_Stage = m_ShaderDetails->stage;

    m_Ctx.Replay().AsyncInvoke([this](IReplayController *r) {
      rdcarray<rdcstr> targets = r->GetDisassemblyTargets();

      rdcstr disasm = r->DisassembleShader(m_Pipeline, m_ShaderDetails, "");

      GUIInvoke::call(this, [this, targets, disasm]() {
        QStringList targetNames;
        for(int i = 0; i < targets.count(); i++)
        {
          QString target = targets[i];
          targetNames << QString(targets[i]);

          if(i == 0)
          {
            // add any custom decompiling tools we have after the first one
            for(const ShaderProcessingTool &d : m_Ctx.Config().ShaderProcessors)
            {
              if(d.input == m_ShaderDetails->encoding)
                targetNames << targetName(d);
            }
          }
        }

        m_DisassemblyType->clear();
        m_DisassemblyType->addItems(targetNames);
        m_DisassemblyType->setCurrentIndex(0);
        QObject::connect(m_DisassemblyType, OverloadedSlot<int>::of(&QComboBox::currentIndexChanged),
                         this, &ShaderViewer::disassemble_typeChanged);

        // read-only applies to us too!
        m_DisassemblyView->setReadOnly(false);
        m_DisassemblyView->setText(disasm.c_str());
        m_DisassemblyView->setReadOnly(true);

        bool preferSourceDebug = false;

        for(const ShaderCompileFlag &flag : m_ShaderDetails->debugInfo.compileFlags.flags)
        {
          if(flag.name == "preferSourceDebug")
          {
            preferSourceDebug = true;
            break;
          }
        }

        updateDebugging();

        // we do updateDebugging() again because the first call finds the scintilla for the current
        // source file, the second time jumps to it.
        if(preferSourceDebug)
        {
          gotoSourceDebugging();
          updateDebugging();
        }
      });
    });
  }

  updateWindowTitle();

  // we always want to highlight words/registers
  QObject::connect(m_DisassemblyView, &ScintillaEdit::buttonReleased, this,
                   &ShaderViewer::disassembly_buttonReleased);

  if(m_Trace)
  {
    if(m_Stage == ShaderStage::Vertex)
    {
      ANALYTIC_SET(ShaderDebug.Vertex, true);
    }
    else if(m_Stage == ShaderStage::Pixel)
    {
      ANALYTIC_SET(ShaderDebug.Pixel, true);
    }
    else if(m_Stage == ShaderStage::Compute)
    {
      ANALYTIC_SET(ShaderDebug.Compute, true);
    }

    m_DisassemblyFrame->layout()->removeWidget(m_DisassemblyToolbar);
  }

  if(m_ShaderDetails && !m_ShaderDetails->debugInfo.files.isEmpty())
  {
    if(m_Trace)
      setWindowTitle(QFormatStr("Debug %1() - %2").arg(m_ShaderDetails->entryPoint).arg(debugContext));
    else
      setWindowTitle(m_ShaderDetails->entryPoint);

    // add all the files, skipping any that have empty contents. We push a NULL in that case so the
    // indices still match up with what the debug info expects. Debug info *shouldn't* point us at
    // an empty file, but if it does we'll just bail out when we see NULL
    m_FileScintillas.reserve(m_ShaderDetails->debugInfo.files.count());

    QWidget *sel = NULL;
    for(const ShaderSourceFile &f : m_ShaderDetails->debugInfo.files)
    {
      if(f.contents.isEmpty())
      {
        m_FileScintillas.push_back(NULL);
        continue;
      }

      QString name = QFileInfo(f.filename).fileName();
      QString text = f.contents;

      ScintillaEdit *scintilla = AddFileScintilla(name, text, m_ShaderDetails->debugInfo.encoding);

      if(sel == NULL)
        sel = scintilla;

      m_FileScintillas.push_back(scintilla);
    }

    if(m_Trace || sel == NULL)
      sel = m_DisassemblyFrame;

    if(m_ShaderDetails->debugInfo.files.size() > 2)
      addFileList();

    ToolWindowManager::raiseToolWindow(sel);
  }

  // hide edit buttons
  ui->editSep->hide();
  ui->refresh->hide();
  ui->snippets->hide();

  if(m_Trace)
  {
    // hide signatures
    ui->inputSig->hide();
    ui->outputSig->hide();

    if(m_ShaderDetails->debugInfo.files.isEmpty())
    {
      ui->debugToggle->setEnabled(false);
      ui->debugToggle->setText(tr("HLSL Unavailable"));
    }

    ui->registers->setColumns({tr("Name"), tr("Type"), tr("Value")});
    ui->registers->header()->setSectionResizeMode(0, QHeaderView::Interactive);
    ui->registers->header()->setSectionResizeMode(1, QHeaderView::Interactive);
    ui->registers->header()->setSectionResizeMode(2, QHeaderView::Stretch);

    ui->locals->setColumns({tr("Name"), tr("Register(s)"), tr("Type"), tr("Value")});
    ui->locals->header()->setSectionResizeMode(0, QHeaderView::Interactive);
    ui->locals->header()->setSectionResizeMode(1, QHeaderView::Interactive);
    ui->locals->header()->setSectionResizeMode(2, QHeaderView::Interactive);
    ui->locals->header()->setSectionResizeMode(3, QHeaderView::Stretch);

    ui->constants->setColumns({tr("Name"), tr("Type"), tr("Value")});
    ui->constants->header()->setSectionResizeMode(0, QHeaderView::Interactive);
    ui->constants->header()->setSectionResizeMode(1, QHeaderView::Interactive);
    ui->constants->header()->setSectionResizeMode(2, QHeaderView::Stretch);

    ui->registers->setTooltipElidedItems(false);
    ui->constants->setTooltipElidedItems(false);

    ui->watch->setWindowTitle(tr("Watch"));
    ui->docking->addToolWindow(
        ui->watch, ToolWindowManager::AreaReference(ToolWindowManager::BottomOf,
                                                    ui->docking->areaOf(m_DisassemblyFrame), 0.25f));
    ui->docking->setToolWindowProperties(
        ui->watch, ToolWindowManager::HideCloseButton | ToolWindowManager::DisallowFloatWindow);

    ui->registers->setWindowTitle(tr("Registers"));
    ui->docking->addToolWindow(
        ui->registers,
        ToolWindowManager::AreaReference(ToolWindowManager::AddTo, ui->docking->areaOf(ui->watch)));
    ui->docking->setToolWindowProperties(
        ui->registers, ToolWindowManager::HideCloseButton | ToolWindowManager::DisallowFloatWindow);

    ui->constants->setWindowTitle(tr("Constants && Resources"));
    ui->docking->addToolWindow(
        ui->constants, ToolWindowManager::AreaReference(ToolWindowManager::LeftOf,
                                                        ui->docking->areaOf(ui->registers), 0.5f));
    ui->docking->setToolWindowProperties(
        ui->constants, ToolWindowManager::HideCloseButton | ToolWindowManager::DisallowFloatWindow);

    ui->callstack->setWindowTitle(tr("Callstack"));
    ui->docking->addToolWindow(
        ui->callstack, ToolWindowManager::AreaReference(ToolWindowManager::RightOf,
                                                        ui->docking->areaOf(ui->registers), 0.2f));
    ui->docking->setToolWindowProperties(
        ui->callstack, ToolWindowManager::HideCloseButton | ToolWindowManager::DisallowFloatWindow);

    if(m_Trace->hasLocals)
    {
      ui->locals->setWindowTitle(tr("Local Variables"));
      ui->docking->addToolWindow(
          ui->locals, ToolWindowManager::AreaReference(ToolWindowManager::AddTo,
                                                       ui->docking->areaOf(ui->registers)));
      ui->docking->setToolWindowProperties(
          ui->locals, ToolWindowManager::HideCloseButton | ToolWindowManager::DisallowFloatWindow);
    }
    else
    {
      ui->locals->hide();
    }

    m_Line2Inst.resize(m_ShaderDetails->debugInfo.files.count());

    for(size_t inst = 0; inst < m_Trace->lineInfo.size(); inst++)
    {
      const LineColumnInfo &line = m_Trace->lineInfo[inst];

      if(line.fileIndex < 0 || line.fileIndex >= m_Line2Inst.count())
        continue;

      for(uint32_t lineNum = line.lineStart; lineNum <= line.lineEnd; lineNum++)
        m_Line2Inst[line.fileIndex][lineNum] = inst;
    }

    QObject::connect(ui->stepBack, &QToolButton::clicked, this, &ShaderViewer::stepBack);
    QObject::connect(ui->stepNext, &QToolButton::clicked, this, &ShaderViewer::stepNext);
    QObject::connect(ui->runBack, &QToolButton::clicked, this, &ShaderViewer::runBack);
    QObject::connect(ui->run, &QToolButton::clicked, this, &ShaderViewer::run);
    QObject::connect(ui->runToCursor, &QToolButton::clicked, this, &ShaderViewer::runToCursor);
    QObject::connect(ui->runToSample, &QToolButton::clicked, this, &ShaderViewer::runToSample);
    QObject::connect(ui->runToNaNOrInf, &QToolButton::clicked, this, &ShaderViewer::runToNanOrInf);

    for(ScintillaEdit *edit : m_Scintillas)
    {
      edit->setMarginWidthN(1, 20.0 * devicePixelRatioF());

      // display current line in margin 2, distinct from breakpoint in margin 1
      sptr_t markMask = (1 << CURRENT_MARKER) | (1 << FINISHED_MARKER);

      edit->setMarginMaskN(1, edit->marginMaskN(1) & ~markMask);
      edit->setMarginMaskN(2, edit->marginMaskN(2) | markMask);

      // suppress the built-in context menu and hook up our own
      edit->usePopUp(SC_POPUP_NEVER);

      edit->setContextMenuPolicy(Qt::CustomContextMenu);
      QObject::connect(edit, &ScintillaEdit::customContextMenuRequested, this,
                       &ShaderViewer::debug_contextMenu);

      edit->setMouseDwellTime(500);

      QObject::connect(edit, &ScintillaEdit::dwellStart, this, &ShaderViewer::disasm_tooltipShow);
      QObject::connect(edit, &ScintillaEdit::dwellEnd, this, &ShaderViewer::disasm_tooltipHide);
    }

    // register the shortcuts via MainWindow so that it works regardless of the active scintilla but
    // still handles multiple shader viewers being present (the one with focus will get the input)
    m_Ctx.GetMainWindow()->RegisterShortcut(QKeySequence(Qt::Key_F10).toString(), this,
                                            [this](QWidget *) { stepNext(); });
    m_Ctx.GetMainWindow()->RegisterShortcut(QKeySequence(Qt::Key_F10 | Qt::ShiftModifier).toString(),
                                            this, [this](QWidget *) { stepBack(); });
    m_Ctx.GetMainWindow()->RegisterShortcut(
        QKeySequence(Qt::Key_F10 | Qt::ControlModifier).toString(), this,
        [this](QWidget *) { runToCursor(); });
    m_Ctx.GetMainWindow()->RegisterShortcut(QKeySequence(Qt::Key_F5).toString(), this,
                                            [this](QWidget *) { run(); });
    m_Ctx.GetMainWindow()->RegisterShortcut(QKeySequence(Qt::Key_F5 | Qt::ShiftModifier).toString(),
                                            this, [this](QWidget *) { runBack(); });
    m_Ctx.GetMainWindow()->RegisterShortcut(QKeySequence(Qt::Key_F9).toString(), this,
                                            [this](QWidget *) { ToggleBreakpoint(); });

    // event filter to pick up tooltip events
    ui->constants->installEventFilter(this);
    ui->registers->installEventFilter(this);
    ui->watch->installEventFilter(this);

    SetCurrentStep(0);

    QObject::connect(ui->watch, &RDTableWidget::keyPress, this, &ShaderViewer::watch_keyPress);

    ui->watch->setContextMenuPolicy(Qt::CustomContextMenu);
    QObject::connect(ui->watch, &RDTableWidget::customContextMenuRequested, this,
                     &ShaderViewer::variables_contextMenu);
    ui->registers->setContextMenuPolicy(Qt::CustomContextMenu);
    QObject::connect(ui->registers, &RDTreeWidget::customContextMenuRequested, this,
                     &ShaderViewer::variables_contextMenu);
    ui->locals->setContextMenuPolicy(Qt::CustomContextMenu);
    QObject::connect(ui->locals, &RDTreeWidget::customContextMenuRequested, this,
                     &ShaderViewer::variables_contextMenu);

    ui->watch->insertRow(0);

    for(int i = 0; i < ui->watch->columnCount(); i++)
    {
      QTableWidgetItem *item = new QTableWidgetItem();
      if(i > 0)
        item->setFlags(item->flags() & ~Qt::ItemIsEditable);
      ui->watch->setItem(0, i, item);
    }

    ui->watch->resizeRowsToContents();

    ToolWindowManager::raiseToolWindow(m_DisassemblyFrame);
  }
  else
  {
    // hide watch, constants, variables
    ui->watch->hide();
    ui->registers->hide();
    ui->constants->hide();
    ui->locals->hide();
    ui->callstack->hide();

    // hide debugging toolbar buttons
    ui->debugSep->hide();
    ui->runBack->hide();
    ui->run->hide();
    ui->stepBack->hide();
    ui->stepNext->hide();
    ui->runToCursor->hide();
    ui->runToSample->hide();
    ui->runToNaNOrInf->hide();
    ui->regFormatSep->hide();
    ui->intView->hide();
    ui->floatView->hide();
    ui->debugToggleSep->hide();
    ui->debugToggle->hide();

    // show input and output signatures
    ui->inputSig->setColumns(
        {tr("Name"), tr("Index"), tr("Reg"), tr("Type"), tr("SysValue"), tr("Mask"), tr("Used")});
    for(int i = 0; i < ui->inputSig->header()->count(); i++)
      ui->inputSig->header()->setSectionResizeMode(i, QHeaderView::ResizeToContents);

    ui->outputSig->setColumns(
        {tr("Name"), tr("Index"), tr("Reg"), tr("Type"), tr("SysValue"), tr("Mask"), tr("Used")});
    for(int i = 0; i < ui->outputSig->header()->count(); i++)
      ui->outputSig->header()->setSectionResizeMode(i, QHeaderView::ResizeToContents);

    if(m_ShaderDetails)
    {
      for(const SigParameter &s : m_ShaderDetails->inputSignature)
      {
        QString name = s.varName.isEmpty()
                           ? QString(s.semanticName)
                           : QFormatStr("%1 (%2)").arg(s.varName).arg(s.semanticName);
        if(s.semanticName.isEmpty())
          name = s.varName;

        QString semIdx = s.needSemanticIndex ? QString::number(s.semanticIndex) : QString();

        QString regIdx =
            s.systemValue == ShaderBuiltin::Undefined ? QString::number(s.regIndex) : lit("-");

        ui->inputSig->addTopLevelItem(new RDTreeWidgetItem(
            {name, semIdx, regIdx, TypeString(s), ToQStr(s.systemValue),
             GetComponentString(s.regChannelMask), GetComponentString(s.channelUsedMask)}));
      }

      bool multipleStreams = false;
      for(const SigParameter &s : m_ShaderDetails->outputSignature)
      {
        if(s.stream > 0)
        {
          multipleStreams = true;
          break;
        }
      }

      for(const SigParameter &s : m_ShaderDetails->outputSignature)
      {
        QString name = s.varName.isEmpty()
                           ? QString(s.semanticName)
                           : QFormatStr("%1 (%2)").arg(s.varName).arg(s.semanticName);
        if(s.semanticName.isEmpty())
          name = s.varName;

        if(multipleStreams)
          name = QFormatStr("Stream %1 : %2").arg(s.stream).arg(name);

        QString semIdx = s.needSemanticIndex ? QString::number(s.semanticIndex) : QString();

        QString regIdx =
            s.systemValue == ShaderBuiltin::Undefined ? QString::number(s.regIndex) : lit("-");

        ui->outputSig->addTopLevelItem(new RDTreeWidgetItem(
            {name, semIdx, regIdx, TypeString(s), ToQStr(s.systemValue),
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

  for(ScintillaEdit *edit : m_Scintillas)
  {
    // C# LightCoral
    edit->markerSetBack(CURRENT_MARKER, SCINTILLA_COLOUR(240, 128, 128));
    edit->markerSetBack(CURRENT_MARKER + 1, SCINTILLA_COLOUR(240, 128, 128));
    edit->markerDefine(CURRENT_MARKER, SC_MARK_SHORTARROW);
    edit->markerDefine(CURRENT_MARKER + 1, SC_MARK_BACKGROUND);
    edit->indicSetFore(CURRENT_INDICATOR, SCINTILLA_COLOUR(240, 128, 128));
    edit->indicSetAlpha(CURRENT_INDICATOR, 220);
    edit->indicSetOutlineAlpha(CURRENT_INDICATOR, 255);
    edit->indicSetUnder(CURRENT_INDICATOR, true);
    edit->indicSetStyle(CURRENT_INDICATOR, INDIC_STRAIGHTBOX);
    edit->indicSetHoverFore(CURRENT_INDICATOR, SCINTILLA_COLOUR(240, 128, 128));
    edit->indicSetHoverStyle(CURRENT_INDICATOR, INDIC_STRAIGHTBOX);

    // C# LightSlateGray
    edit->markerSetBack(FINISHED_MARKER, SCINTILLA_COLOUR(119, 136, 153));
    edit->markerSetBack(FINISHED_MARKER + 1, SCINTILLA_COLOUR(119, 136, 153));
    edit->markerDefine(FINISHED_MARKER, SC_MARK_ROUNDRECT);
    edit->markerDefine(FINISHED_MARKER + 1, SC_MARK_BACKGROUND);
    edit->indicSetFore(FINISHED_INDICATOR, SCINTILLA_COLOUR(119, 136, 153));
    edit->indicSetAlpha(FINISHED_INDICATOR, 220);
    edit->indicSetOutlineAlpha(FINISHED_INDICATOR, 255);
    edit->indicSetUnder(FINISHED_INDICATOR, true);
    edit->indicSetStyle(FINISHED_INDICATOR, INDIC_STRAIGHTBOX);
    edit->indicSetHoverFore(FINISHED_INDICATOR, SCINTILLA_COLOUR(119, 136, 153));
    edit->indicSetHoverStyle(FINISHED_INDICATOR, INDIC_STRAIGHTBOX);

    // C# Red
    edit->markerSetBack(BREAKPOINT_MARKER, SCINTILLA_COLOUR(255, 0, 0));
    edit->markerSetBack(BREAKPOINT_MARKER + 1, SCINTILLA_COLOUR(255, 0, 0));
    edit->markerDefine(BREAKPOINT_MARKER, SC_MARK_CIRCLE);
    edit->markerDefine(BREAKPOINT_MARKER + 1, SC_MARK_BACKGROUND);
  }
}

void ShaderViewer::updateWindowTitle()
{
  if(m_ShaderDetails)
  {
    QString shaderName = m_Ctx.GetResourceName(m_ShaderDetails->resourceId);

    // On D3D12, get the shader name from the pipeline rather than the shader itself
    // for the benefit of D3D12 which doesn't have separate shader objects
    if(m_Ctx.CurPipelineState().IsCaptureD3D12())
      shaderName = QFormatStr("%1 %2")
                       .arg(m_Ctx.GetResourceName(m_Pipeline))
                       .arg(m_Ctx.CurPipelineState().Abbrev(m_ShaderDetails->stage));

    if(m_Trace)
      setWindowTitle(QFormatStr("Debugging %1 - %2").arg(shaderName).arg(m_DebugContext));
    else
      setWindowTitle(shaderName);
  }
}

void ShaderViewer::gotoSourceDebugging()
{
  if(m_CurInstructionScintilla)
  {
    ToolWindowManager::raiseToolWindow(m_CurInstructionScintilla);
    m_CurInstructionScintilla->setFocus(Qt::MouseFocusReason);
  }
}

void ShaderViewer::gotoDisassemblyDebugging()
{
  ToolWindowManager::raiseToolWindow(m_DisassemblyFrame);
  m_DisassemblyFrame->setFocus(Qt::MouseFocusReason);
}

ShaderViewer::~ShaderViewer()
{
  delete m_FindResults;
  m_FindResults = NULL;

  // don't want to async invoke while using 'this', so save the trace separately
  ShaderDebugTrace *trace = m_Trace;

  // unregister any shortcuts on this window
  m_Ctx.GetMainWindow()->UnregisterShortcut(QString(), this);

  m_Ctx.Replay().AsyncInvoke([trace](IReplayController *r) { r->FreeTrace(trace); });

  if(m_CloseCallback)
    m_CloseCallback(&m_Ctx);

  m_Ctx.RemoveCaptureViewer(this);
  delete ui;
}

void ShaderViewer::OnCaptureLoaded()
{
}

void ShaderViewer::OnCaptureClosed()
{
  ToolWindowManager::closeToolWindow(this);
}

void ShaderViewer::OnEventChanged(uint32_t eventId)
{
  updateDebugging();
  updateWindowTitle();
}

ScintillaEdit *ShaderViewer::AddFileScintilla(const QString &name, const QString &text,
                                              ShaderEncoding encoding)
{
  ScintillaEdit *scintilla = MakeEditor(lit("scintilla") + name, text,
                                        encoding == ShaderEncoding::HLSL ? SCLEX_HLSL : SCLEX_GLSL);
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

  margin0width = int(margin0width * devicePixelRatioF());

  ret->setMarginLeft(4.0 * devicePixelRatioF());
  ret->setMarginWidthN(0, margin0width);
  ret->setMarginWidthN(1, 0);
  ret->setMarginWidthN(2, 16.0 * devicePixelRatioF());
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

void ShaderViewer::debug_contextMenu(const QPoint &pos)
{
  ScintillaEdit *edit = qobject_cast<ScintillaEdit *>(QObject::sender());

  bool isDisasm = (edit == m_DisassemblyView);

  int scintillaPos = edit->positionFromPoint(pos.x(), pos.y());

  QMenu contextMenu(this);

  QAction gotoOther(isDisasm ? tr("Go to Source") : tr("Go to Disassembly"), this);

  QObject::connect(&gotoOther, &QAction::triggered, [this, isDisasm]() {
    if(isDisasm)
      gotoSourceDebugging();
    else
      gotoDisassemblyDebugging();

    updateDebugging();
  });

  QAction intDisplay(tr("Integer register display"), this);
  QAction floatDisplay(tr("Float register display"), this);

  intDisplay.setCheckable(true);
  floatDisplay.setCheckable(true);

  intDisplay.setChecked(ui->intView->isChecked());
  floatDisplay.setChecked(ui->floatView->isChecked());

  QObject::connect(&intDisplay, &QAction::triggered, this, &ShaderViewer::on_intView_clicked);
  QObject::connect(&floatDisplay, &QAction::triggered, this, &ShaderViewer::on_floatView_clicked);

  if(isDisasm && m_CurInstructionScintilla == NULL)
    gotoOther.setEnabled(false);

  contextMenu.addAction(&gotoOther);
  contextMenu.addSeparator();

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

  copyText.setEnabled(!edit->selectionEmpty());

  QObject::connect(&copyText, &QAction::triggered,
                   [edit] { edit->copyRange(edit->selectionStart(), edit->selectionEnd()); });
  QObject::connect(&selectAll, &QAction::triggered, [edit] { edit->selectAll(); });

  contextMenu.addAction(&copyText);
  contextMenu.addAction(&selectAll);
  contextMenu.addSeparator();

  RDDialog::show(&contextMenu, edit->viewport()->mapToGlobal(pos));
}

void ShaderViewer::variables_contextMenu(const QPoint &pos)
{
  QAbstractItemView *w = qobject_cast<QAbstractItemView *>(QObject::sender());

  QMenu contextMenu(this);

  QAction copyValue(tr("Copy"), this);
  QAction addWatch(tr("Add Watch"), this);
  QAction deleteWatch(tr("Delete Watch"), this);
  QAction clearAll(tr("Clear All"), this);

  contextMenu.addAction(&copyValue);
  contextMenu.addSeparator();
  contextMenu.addAction(&addWatch);

  if(QObject::sender() == ui->watch)
  {
    QObject::connect(&copyValue, &QAction::triggered, [this] { ui->watch->copySelection(); });

    contextMenu.addAction(&deleteWatch);
    contextMenu.addSeparator();
    contextMenu.addAction(&clearAll);

    // start with no row selected
    int selRow = -1;

    QList<QTableWidgetItem *> items = ui->watch->selectedItems();
    for(QTableWidgetItem *item : items)
    {
      // if no row is selected, or the same as this item, set selected row to this item's
      if(selRow == -1 || selRow == item->row())
      {
        selRow = item->row();
      }
      else
      {
        // we only get here if we see an item on a different row selected - that means too many rows
        // so bail out
        selRow = -1;
        break;
      }
    }

    // if we have a selected row that isn't the last one, we can add/delete this item
    deleteWatch.setEnabled(selRow >= 0 && selRow < ui->watch->rowCount() - 1);
    addWatch.setEnabled(selRow >= 0 && selRow < ui->watch->rowCount() - 1);

    QObject::connect(&addWatch, &QAction::triggered, [this, selRow] {
      QTableWidgetItem *item = ui->watch->item(selRow, 0);

      if(item)
        AddWatch(item->text());
    });

    QObject::connect(&deleteWatch, &QAction::triggered,
                     [this, selRow] { ui->watch->removeRow(selRow); });

    QObject::connect(&clearAll, &QAction::triggered, [this] {
      while(ui->watch->rowCount() > 1)
        ui->watch->removeRow(0);
    });
  }
  else
  {
    RDTreeWidget *tree = qobject_cast<RDTreeWidget *>(w);

    QObject::connect(&copyValue, &QAction::triggered, [tree] { tree->copySelection(); });

    addWatch.setEnabled(tree->selectedItem() != NULL);

    QObject::connect(&addWatch, &QAction::triggered, [this, tree] {
      if(tree == ui->locals)
        AddWatch(tree->selectedItem()->tag().toString());
      else
        AddWatch(tree->selectedItem()->text(0));
    });
  }

  RDDialog::show(&contextMenu, w->viewport()->mapToGlobal(pos));
}

void ShaderViewer::disassembly_buttonReleased(QMouseEvent *event)
{
  if(event->button() == Qt::LeftButton)
  {
    sptr_t scintillaPos = m_DisassemblyView->positionFromPoint(event->x(), event->y());

    sptr_t start = m_DisassemblyView->wordStartPosition(scintillaPos, true);
    sptr_t end = m_DisassemblyView->wordEndPosition(scintillaPos, true);

    QString text = QString::fromUtf8(m_DisassemblyView->textRange(start, end));

    QRegularExpression regexp(lit("^[xyzwrgba]+$"));

    // if we match a swizzle look before that for the register
    if(regexp.match(text).hasMatch())
    {
      start--;
      while(isspace(m_DisassemblyView->charAt(start)))
        start--;

      if(m_DisassemblyView->charAt(start) == '.')
      {
        end = m_DisassemblyView->wordEndPosition(start - 1, true);
        start = m_DisassemblyView->wordStartPosition(start - 1, true);

        text = QString::fromUtf8(m_DisassemblyView->textRange(start, end));
      }
    }

    if(!text.isEmpty())
    {
      VariableTag tag;
      getRegisterFromWord(text, tag.cat, tag.idx, tag.arrayIdx);

      // for now since we don't have friendly naming, only highlight registers
      if(tag.cat != VariableCategory::Unknown)
      {
        start = 0;
        end = m_DisassemblyView->length();

        for(int i = 0; i < ui->registers->topLevelItemCount(); i++)
        {
          RDTreeWidgetItem *item = ui->registers->topLevelItem(i);
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

  QString targetStr = m_DisassemblyType->currentText();
  QByteArray target = targetStr.toUtf8();

  for(const ShaderProcessingTool &disasm : m_Ctx.Config().ShaderProcessors)
  {
    if(targetStr == targetName(disasm))
    {
      ShaderToolOutput out = disasm.DisassembleShader(this, m_ShaderDetails, "");

      rdcstr text;

      if(out.result.isEmpty())
        text = out.log;
      else
        text.assign((const char *)out.result.data(), out.result.size());

      m_DisassemblyView->setReadOnly(false);
      m_DisassemblyView->setText(text.c_str());
      m_DisassemblyView->setReadOnly(true);
      m_DisassemblyView->emptyUndoBuffer();
      return;
    }
  }

  m_Ctx.Replay().AsyncInvoke([this, target](IReplayController *r) {
    rdcstr disasm = r->DisassembleShader(m_Pipeline, m_ShaderDetails, target.data());

    GUIInvoke::call(this, [this, disasm]() {
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

  if(isSourceDebugging())
  {
    const ShaderDebugState &oldstate = m_Trace->states[CurrentStep()];

    LineColumnInfo oldLine =
        m_Trace->lineInfo[qMin(m_Trace->lineInfo.size() - 1, (size_t)oldstate.nextInstruction)];

    while(CurrentStep() < m_Trace->states.count())
    {
      m_CurrentStep--;

      const ShaderDebugState &state = m_Trace->states[m_CurrentStep];

      if(m_Breakpoints.contains((int)state.nextInstruction))
        break;

      if(m_CurrentStep == 0)
        break;

      if(m_Trace->lineInfo[state.nextInstruction] == oldLine)
        continue;

      break;
    }

    SetCurrentStep(CurrentStep());
  }
  else
  {
    SetCurrentStep(CurrentStep() - 1);
  }

  return true;
}

bool ShaderViewer::stepNext()
{
  if(!m_Trace)
    return false;

  if(CurrentStep() + 1 >= m_Trace->states.count())
    return false;

  if(isSourceDebugging())
  {
    const ShaderDebugState &oldstate = m_Trace->states[CurrentStep()];

    LineColumnInfo oldLine = m_Trace->lineInfo[oldstate.nextInstruction];

    while(CurrentStep() < m_Trace->states.count())
    {
      m_CurrentStep++;

      const ShaderDebugState &state = m_Trace->states[m_CurrentStep];

      if(m_Breakpoints.contains((int)state.nextInstruction))
        break;

      if(m_CurrentStep + 1 >= m_Trace->states.count())
        break;

      if(m_Trace->lineInfo[state.nextInstruction] == oldLine)
        continue;

      break;
    }

    SetCurrentStep(CurrentStep());
  }
  else
  {
    SetCurrentStep(CurrentStep() + 1);
  }

  return true;
}

void ShaderViewer::runToCursor()
{
  if(!m_Trace)
    return;

  ScintillaEdit *cur = currentScintilla();

  if(cur != m_DisassemblyView)
  {
    int scintillaIndex = m_FileScintillas.indexOf(cur);

    if(scintillaIndex < 0)
      return;

    sptr_t i = cur->lineFromPosition(cur->currentPos()) + 1;

    QMap<int32_t, size_t> &fileMap = m_Line2Inst[scintillaIndex];

    // find the next line that maps to an instruction
    for(; i < cur->lineCount(); i++)
    {
      if(fileMap.contains(i))
      {
        runTo((int)fileMap[i], true);
        return;
      }
    }

    // if we didn't find one, just run
    run();
  }
  else
  {
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

  while(step < m_Trace->states.count())
  {
    if(runToInstruction >= 0 && m_Trace->states[step].nextInstruction == (uint32_t)runToInstruction)
      break;

    if(!firstStep && (step + inc >= 0) && (step + inc < m_Trace->states.count()) &&
       (m_Trace->states[step + inc].flags & condition))
      break;

    if(!firstStep && m_Breakpoints.contains((int)m_Trace->states[step].nextInstruction))
      break;

    firstStep = false;

    if(step + inc < 0 || step + inc >= m_Trace->states.count())
      break;

    step += inc;
  }

  SetCurrentStep(step);
}

QString ShaderViewer::stringRep(const ShaderVariable &var, bool useType)
{
  if(ui->intView->isChecked() || (useType && var.type == VarType::SInt))
    return RowString(var, 0, VarType::SInt);

  if(useType && var.type == VarType::UInt)
    return RowString(var, 0, VarType::UInt);

  return RowString(var, 0, VarType::Float);
}

RDTreeWidgetItem *ShaderViewer::makeResourceRegister(const Bindpoint &bind, uint32_t idx,
                                                     const BoundResource &bound,
                                                     const ShaderResource &res)
{
  QString name = QFormatStr(" (%1)").arg(res.name);

  const TextureDescription *tex = m_Ctx.GetTexture(bound.resourceId);
  const BufferDescription *buf = m_Ctx.GetBuffer(bound.resourceId);

  QChar regChar(QLatin1Char('u'));

  if(res.isReadOnly)
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
                       .arg(tex->format.Name())
                       .arg(m_Ctx.GetResourceName(bound.resourceId));

    return new RDTreeWidgetItem({regname + name, lit("Texture"), type});
  }
  else if(buf)
  {
    QString type =
        QFormatStr("%1 - %2").arg(buf->length).arg(m_Ctx.GetResourceName(bound.resourceId));

    return new RDTreeWidgetItem({regname + name, lit("Buffer"), type});
  }
  else
  {
    return new RDTreeWidgetItem(
        {regname + name, lit("Resource"), m_Ctx.GetResourceName(bound.resourceId)});
  }
}

QString ShaderViewer::targetName(const ShaderProcessingTool &disasm)
{
  return lit("%1 (%2)").arg(ToQStr(disasm.output)).arg(disasm.name);
}

void ShaderViewer::addFileList()
{
  QListWidget *list = new QListWidget(this);
  list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  list->setSelectionMode(QAbstractItemView::SingleSelection);
  QObject::connect(list, &QListWidget::currentRowChanged, [this](int idx) {
    QWidget *raiseWidget = m_Scintillas[idx];
    if(m_Scintillas[idx] == m_DisassemblyView)
      raiseWidget = m_DisassemblyFrame;
    ToolWindowManager::raiseToolWindow(raiseWidget);
  });
  list->setWindowTitle(tr("File List"));

  for(ScintillaEdit *s : m_Scintillas)
  {
    if(s == m_DisassemblyView)
      list->addItem(m_DisassemblyFrame->windowTitle());
    else
      list->addItem(s->windowTitle());
  }

  ui->docking->addToolWindow(
      list, ToolWindowManager::AreaReference(ToolWindowManager::LeftOf,
                                             ui->docking->areaOf(m_Scintillas.front()), 0.2f));
  ui->docking->setToolWindowProperties(
      list, ToolWindowManager::HideCloseButton | ToolWindowManager::DisallowFloatWindow);
}

void ShaderViewer::combineStructures(RDTreeWidgetItem *root)
{
  RDTreeWidgetItem temp;

  // we perform a filter moving from root to temp. At each point we check the node:
  // * if the node has no struct or array prefix, it gets moved
  // * if the node does have a prefix, we sweep finding all matching elements with the same prefix,
  //   strip the prefix off them and make a combined node, then recurse to combine anything
  //   underneath. We aren't greedy in picking prefixes so this should generate a struct/array tree.
  // * in the event that a node has no matching elements we move it across as if it had no prefix.
  // * we iterate from last to first, because when combining elements that may be spread out in the
  //   list of children, we want to combine up to the position of the last item, not the position of
  //   the first.

  for(int c = root->childCount() - 1; c >= 0;)
  {
    RDTreeWidgetItem *child = root->takeChild(c);
    c--;

    QString name = child->text(0);

    int dotIndex = name.indexOf(QLatin1Char('.'));
    int arrIndex = name.indexOf(QLatin1Char('['));

    // if this node doesn't have any segments, just move it across.
    if(dotIndex < 0 && arrIndex < 0)
    {
      temp.insertChild(0, child);
      continue;
    }

    // store the index of the first separator
    int sepIndex = dotIndex;
    bool isArray = false;
    if(sepIndex == -1 || (arrIndex > 0 && arrIndex < sepIndex))
    {
      sepIndex = arrIndex;
      isArray = true;
    }

    // we have a valid node to match against, record the prefix (including separator character)
    QString prefix = name.mid(0, sepIndex + 1);

    QVector<RDTreeWidgetItem *> matches = {child};

    // iterate down from the next item
    for(int n = c; n >= 0; n--)
    {
      RDTreeWidgetItem *testNode = root->child(n);

      QString testName = testNode->text(0);

      QString testprefix = testName.mid(0, sepIndex + 1);

      // no match - continue
      if(testprefix != prefix)
        continue;

      // match, take this child
      matches.push_back(root->takeChild(n));

      // also decrement c since we're taking a child ahead of where that loop will go.
      c--;
    }

    // no other matches with the same prefix, just move across
    if(matches.count() == 1)
    {
      temp.insertChild(0, child);
      continue;
    }

    // sort the children by name
    std::sort(matches.begin(), matches.end(),
              [](const RDTreeWidgetItem *a, const RDTreeWidgetItem *b) {
                return a->text(0) < b->text(0);
              });

    // create a new parent with just the prefix
    QVariantList values = {name.mid(0, sepIndex)};
    for(int i = 1; i < child->dataCount(); i++)
      values.push_back(QVariant());
    RDTreeWidgetItem *parent = new RDTreeWidgetItem(values);

    // add all the children (stripping the prefix from their name)
    for(RDTreeWidgetItem *item : matches)
    {
      if(!isArray)
        item->setText(0, item->text(0).mid(sepIndex + 1));
      parent->addChild(item);

      if(item->background().color().isValid())
        parent->setBackground(item->background());
      if(item->foreground().color().isValid())
        parent->setForeground(item->foreground());
    }

    // recurse and combine members of this object if a struct
    if(!isArray)
      combineStructures(parent);

    // now add to the list
    temp.insertChild(0, parent);
  }

  if(root->childCount() > 0)
    qCritical() << "Some objects left on root!";

  // move all the children back from the temp object into the parameter
  while(temp.childCount() > 0)
    root->addChild(temp.takeChild(0));
}

RDTreeWidgetItem *ShaderViewer::findLocal(RDTreeWidgetItem *root, QString name)
{
  if(root->tag().toString() == name)
    return root;

  for(int i = 0; i < root->childCount(); i++)
  {
    RDTreeWidgetItem *ret = findLocal(root->child(i), name);
    if(ret)
      return ret;
  }

  return NULL;
}

void ShaderViewer::updateDebugging()
{
  if(!m_Trace || m_CurrentStep < 0 || m_CurrentStep >= m_Trace->states.count())
    return;

  if(ui->debugToggle->isEnabled())
  {
    if(isSourceDebugging())
      ui->debugToggle->setText(tr("Debug in Assembly"));
    else
      ui->debugToggle->setText(tr("Debug in HLSL"));
  }

  const ShaderDebugState &state = m_Trace->states[m_CurrentStep];

  uint32_t nextInst = state.nextInstruction;
  bool done = false;

  if(m_CurrentStep == m_Trace->states.count() - 1)
  {
    nextInst--;
    done = true;
  }

  // add current instruction marker
  m_DisassemblyView->markerDeleteAll(CURRENT_MARKER);
  m_DisassemblyView->markerDeleteAll(CURRENT_MARKER + 1);
  m_DisassemblyView->markerDeleteAll(FINISHED_MARKER);
  m_DisassemblyView->markerDeleteAll(FINISHED_MARKER + 1);

  if(m_CurInstructionScintilla)
  {
    m_CurInstructionScintilla->markerDeleteAll(CURRENT_MARKER);
    m_CurInstructionScintilla->markerDeleteAll(CURRENT_MARKER + 1);
    m_CurInstructionScintilla->markerDeleteAll(FINISHED_MARKER);
    m_CurInstructionScintilla->markerDeleteAll(FINISHED_MARKER + 1);

    m_CurInstructionScintilla->indicatorClearRange(0, m_CurInstructionScintilla->length());

    m_CurInstructionScintilla = NULL;
  }

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

  ui->callstack->clear();

  if(state.nextInstruction < m_Trace->lineInfo.size())
  {
    LineColumnInfo &lineInfo = m_Trace->lineInfo[state.nextInstruction];

    for(const rdcstr &s : lineInfo.callstack)
      ui->callstack->insertItem(0, s);

    if(lineInfo.fileIndex >= 0 && lineInfo.fileIndex < m_FileScintillas.count())
    {
      m_CurInstructionScintilla = m_FileScintillas[lineInfo.fileIndex];

      if(m_CurInstructionScintilla)
      {
        for(sptr_t line = lineInfo.lineStart; line <= (sptr_t)lineInfo.lineEnd; line++)
        {
          if(line == (sptr_t)lineInfo.lineEnd)
            m_CurInstructionScintilla->markerAdd(line - 1, done ? FINISHED_MARKER : CURRENT_MARKER);

          if(lineInfo.colStart == 0)
          {
            // with no column info, add a marker on the whole line
            m_CurInstructionScintilla->markerAdd(line - 1,
                                                 done ? FINISHED_MARKER + 1 : CURRENT_MARKER + 1);
          }
          else
          {
            // otherwise add an indicator on the column range.

            // Start from the full position/length for this line
            sptr_t pos = m_CurInstructionScintilla->positionFromLine(line - 1);
            sptr_t len = m_CurInstructionScintilla->lineEndPosition(line - 1) - pos;

            // if we're on the last line of the range, restrict the length to end on the last column
            if(line == (sptr_t)lineInfo.lineEnd && lineInfo.colEnd != 0)
              len = lineInfo.colEnd;

            // if we're on the start of the range (which may also be the last line above too), shift
            // inwards towards the first column
            if(line == (sptr_t)lineInfo.lineStart)
            {
              pos += lineInfo.colStart - 1;
              len -= lineInfo.colStart - 1;
            }

            m_CurInstructionScintilla->setIndicatorCurrent(done ? FINISHED_INDICATOR
                                                                : CURRENT_INDICATOR);
            m_CurInstructionScintilla->indicatorFillRange(pos, len);
          }
        }

        if(isSourceDebugging() ||
           ui->docking->areaOf(m_CurInstructionScintilla) != ui->docking->areaOf(m_DisassemblyFrame))
          ToolWindowManager::raiseToolWindow(m_CurInstructionScintilla);

        int pos = m_CurInstructionScintilla->positionFromLine(lineInfo.lineStart - 1);
        m_CurInstructionScintilla->setSelection(pos, pos);

        ensureLineScrolled(m_CurInstructionScintilla, lineInfo.lineStart - 1);
      }
    }
  }

  if(ui->constants->topLevelItemCount() == 0)
  {
    for(int i = 0; i < m_Trace->constantBlocks.count(); i++)
    {
      for(int j = 0; j < m_Trace->constantBlocks[i].members.count(); j++)
      {
        if(m_Trace->constantBlocks[i].members[j].rows > 0 ||
           m_Trace->constantBlocks[i].members[j].columns > 0)
        {
          RDTreeWidgetItem *node =
              new RDTreeWidgetItem({m_Trace->constantBlocks[i].members[j].name, lit("cbuffer"),
                                    stringRep(m_Trace->constantBlocks[i].members[j], false)});
          node->setTag(QVariant::fromValue(VariableTag(VariableCategory::Constants, j, i)));

          ui->constants->addTopLevelItem(node);
        }
      }
    }

    for(int i = 0; i < m_Trace->inputs.count(); i++)
    {
      const ShaderVariable &input = m_Trace->inputs[i];

      if(input.rows > 0 || input.columns > 0)
      {
        RDTreeWidgetItem *node = new RDTreeWidgetItem(
            {input.name, ToQStr(input.type) + lit(" input"), stringRep(input, true)});
        node->setTag(QVariant::fromValue(VariableTag(VariableCategory::Inputs, i)));

        ui->constants->addTopLevelItem(node);
      }
    }

    rdcarray<BoundResourceArray> rw = m_Ctx.CurPipelineState().GetReadWriteResources(m_Stage);
    rdcarray<BoundResourceArray> ro = m_Ctx.CurPipelineState().GetReadOnlyResources(m_Stage);

    bool tree = false;

    for(int i = 0;
        i < m_Mapping->readWriteResources.count() && i < m_ShaderDetails->readWriteResources.count();
        i++)
    {
      Bindpoint bind = m_Mapping->readWriteResources[i];

      if(!bind.used)
        continue;

      int idx = rw.indexOf(bind);

      if(idx < 0 || rw[idx].resources.isEmpty())
        continue;

      if(bind.arraySize == 1)
      {
        RDTreeWidgetItem *node = makeResourceRegister(bind, 0, rw[idx].resources[0],
                                                      m_ShaderDetails->readWriteResources[i]);
        if(node)
          ui->constants->addTopLevelItem(node);
      }
      else
      {
        RDTreeWidgetItem *node =
            new RDTreeWidgetItem({m_ShaderDetails->readWriteResources[i].name,
                                  QFormatStr("[%1]").arg(bind.arraySize), QString()});

        for(uint32_t a = 0; a < bind.arraySize; a++)
          node->addChild(makeResourceRegister(bind, a, rw[idx].resources[a],
                                              m_ShaderDetails->readWriteResources[i]));

        tree = true;

        ui->constants->addTopLevelItem(node);
      }
    }

    for(int i = 0;
        i < m_Mapping->readOnlyResources.count() && i < m_ShaderDetails->readOnlyResources.count();
        i++)
    {
      Bindpoint bind = m_Mapping->readOnlyResources[i];

      if(!bind.used)
        continue;

      int idx = ro.indexOf(bind);

      if(idx < 0 || ro[idx].resources.isEmpty())
        continue;

      if(bind.arraySize == 1)
      {
        RDTreeWidgetItem *node = makeResourceRegister(bind, 0, ro[idx].resources[0],
                                                      m_ShaderDetails->readOnlyResources[i]);
        if(node)
          ui->constants->addTopLevelItem(node);
      }
      else
      {
        RDTreeWidgetItem *node =
            new RDTreeWidgetItem({m_ShaderDetails->readOnlyResources[i].name,
                                  QFormatStr("[%1]").arg(bind.arraySize), QString()});

        for(uint32_t a = 0; a < bind.arraySize; a++)
          node->addChild(makeResourceRegister(bind, a, ro[idx].resources[a],
                                              m_ShaderDetails->readOnlyResources[i]));

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

  if(m_Trace->hasLocals)
  {
    RDTreeViewExpansionState expansion;
    ui->locals->saveExpansion(expansion, 0);

    ui->locals->clear();

    const QString xyzw = lit("xyzw");

    RDTreeWidgetItem fakeroot;

    for(size_t lidx = 0; lidx < state.locals.size(); lidx++)
    {
      // iterate in reverse order, so newest locals tend to end up on top
      const LocalVariableMapping &l = state.locals[state.locals.size() - 1 - lidx];

      QString localName = l.localName;
      QString regNames, typeName;
      QString value;

      bool modified = false;

      if(l.type == VarType::UInt)
        typeName = lit("uint");
      else if(l.type == VarType::SInt)
        typeName = lit("int");
      else if(l.type == VarType::Float)
        typeName = lit("float");
      else if(l.type == VarType::Double)
        typeName = lit("double");

      if(l.registers[0].type == RegisterType::IndexedTemporary)
      {
        typeName += lit("[]");

        regNames = QFormatStr("x%1").arg(l.registers[0].index);

        for(const RegisterRange &mr : state.modified)
        {
          if(mr.type == RegisterType::IndexedTemporary && mr.index == l.registers[0].index)
          {
            modified = true;
            break;
          }
        }
      }
      else
      {
        if(l.rows > 1)
          typeName += QFormatStr("%1x%2").arg(l.rows).arg(l.columns);
        else
          typeName += QString::number(l.columns);

        for(uint32_t i = 0; i < l.regCount; i++)
        {
          const RegisterRange &r = l.registers[i];

          for(const RegisterRange &mr : state.modified)
          {
            if(mr.type == r.type && mr.index == r.index && mr.component == r.component)
            {
              modified = true;
              break;
            }
          }

          if(!value.isEmpty())
            value += lit(", ");
          if(!regNames.isEmpty())
            regNames += lit(", ");

          if(r.type == RegisterType::Undefined)
          {
            regNames += lit("-");
            value += lit("?");
            continue;
          }

          const ShaderVariable *var = GetRegisterVariable(r);

          if(var)
          {
            // if the previous register was the same, just append our component
            if(i > 0 && r.type == l.registers[i - 1].type && r.index == l.registers[i - 1].index)
            {
              // remove the auto-appended ", " - there must be one because this isn't the first
              // register
              regNames.chop(2);
              regNames += xyzw[r.component];
            }
            else
            {
              regNames += QFormatStr("%1.%2").arg(var->name).arg(xyzw[r.component]);
            }

            if(l.type == VarType::UInt)
              value += Formatter::Format(var->value.uv[r.component]);
            else if(l.type == VarType::SInt)
              value += Formatter::Format(var->value.iv[r.component]);
            else if(l.type == VarType::Float)
              value += Formatter::Format(var->value.fv[r.component]);
            else if(l.type == VarType::Double)
              value += Formatter::Format(var->value.dv[r.component]);
          }
          else
          {
            regNames += lit("<error>");
            value += lit("<error>");
          }
        }
      }

      RDTreeWidgetItem *node = new RDTreeWidgetItem({localName, regNames, typeName, value});

      node->setTag(localName);

      if(modified)
        node->setForegroundColor(QColor(Qt::red));

      if(l.registers[0].type == RegisterType::IndexedTemporary)
      {
        const ShaderVariable *var = NULL;

        if(l.registers[0].index < state.indexableTemps.size())
          var = &state.indexableTemps[l.registers[0].index];

        for(int t = 0; var && t < var->members.count(); t++)
        {
          node->addChild(new RDTreeWidgetItem({
              QFormatStr("%1[%2]").arg(localName).arg(t), QFormatStr("%1[%2]").arg(regNames).arg(t),
              typeName, RowString(var->members[t], 0, l.type),
          }));
        }
      }

      fakeroot.addChild(node);
    }

    // recursively combine nodes with the same prefix together
    combineStructures(&fakeroot);

    while(fakeroot.childCount() > 0)
      ui->locals->addTopLevelItem(fakeroot.takeChild(0));

    ui->locals->applyExpansion(expansion, 0);
  }

  if(ui->registers->topLevelItemCount() == 0)
  {
    for(int i = 0; i < state.registers.count(); i++)
      ui->registers->addTopLevelItem(
          new RDTreeWidgetItem({state.registers[i].name, lit("temporary"), QString()}));

    for(int i = 0; i < state.indexableTemps.count(); i++)
    {
      RDTreeWidgetItem *node =
          new RDTreeWidgetItem({QFormatStr("x%1").arg(i), lit("indexable"), QString()});
      for(int t = 0; t < state.indexableTemps[i].members.count(); t++)
        node->addChild(new RDTreeWidgetItem(
            {state.indexableTemps[i].members[t].name, lit("indexable"), QString()}));
      ui->registers->addTopLevelItem(node);
    }

    for(int i = 0; i < state.outputs.count(); i++)
      ui->registers->addTopLevelItem(
          new RDTreeWidgetItem({state.outputs[i].name, lit("output"), QString()}));
  }

  ui->registers->beginUpdate();

  int v = 0;

  for(int i = 0; i < state.registers.count(); i++)
  {
    RDTreeWidgetItem *node = ui->registers->topLevelItem(v++);

    node->setText(2, stringRep(state.registers[i], false));
    node->setTag(QVariant::fromValue(VariableTag(VariableCategory::Temporaries, i)));

    bool modified = false;

    for(const RegisterRange &mr : state.modified)
    {
      if(mr.type == RegisterType::Temporary && mr.index == i)
      {
        modified = true;
        break;
      }
    }

    if(modified)
      node->setForegroundColor(QColor(Qt::red));
    else
      node->setForeground(QBrush());
  }

  for(int i = 0; i < state.indexableTemps.count(); i++)
  {
    RDTreeWidgetItem *node = ui->registers->topLevelItem(v++);

    bool modified = false;

    for(const RegisterRange &mr : state.modified)
    {
      if(mr.type == RegisterType::IndexedTemporary && mr.index == i)
      {
        modified = true;
        break;
      }
    }

    if(modified)
      node->setForegroundColor(QColor(Qt::red));
    else
      node->setForeground(QBrush());

    for(int t = 0; t < state.indexableTemps[i].members.count(); t++)
    {
      RDTreeWidgetItem *child = node->child(t);

      if(modified)
        child->setForegroundColor(QColor(Qt::red));
      else
        child->setForeground(QBrush());

      child->setText(2, stringRep(state.indexableTemps[i].members[t], false));
      child->setTag(QVariant::fromValue(VariableTag(VariableCategory::IndexTemporaries, t, i)));
    }
  }

  for(int i = 0; i < state.outputs.count(); i++)
  {
    RDTreeWidgetItem *node = ui->registers->topLevelItem(v++);

    node->setText(2, stringRep(state.outputs[i], false));
    node->setTag(QVariant::fromValue(VariableTag(VariableCategory::Outputs, i)));

    bool modified = false;

    for(const RegisterRange &mr : state.modified)
    {
      if(mr.type == RegisterType::Output && mr.index == i)
      {
        modified = true;
        break;
      }
    }

    if(modified)
      node->setForegroundColor(QColor(Qt::red));
    else
      node->setForeground(QBrush());
  }

  ui->registers->endUpdate();

  ui->watch->setUpdatesEnabled(false);

  for(int i = 0; i < ui->watch->rowCount() - 1; i++)
  {
    QTableWidgetItem *item = ui->watch->item(i, 0);

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
      item = new QTableWidgetItem(tr("register", "watch type"));
      item->setFlags(item->flags() & ~Qt::ItemIsEditable);
      ui->watch->setItem(i, 2, item);

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

      const rdcarray<ShaderVariable> *vars = GetVariableList(varCat, arrIndex);

      ok = false;
      int regindex = regidx.toInt(&ok);

      if(vars && ok && regindex >= 0 && regindex < vars->count())
      {
        const ShaderVariable &vr = (*vars)[regindex];

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

        item = new QTableWidgetItem(vr.name);
        item->setFlags(item->flags() & ~Qt::ItemIsEditable);
        ui->watch->setItem(i, 1, item);

        item = new QTableWidgetItem(val);
        item->setData(Qt::UserRole, QVariant::fromValue(VariableTag(varCat, regindex, arrIndex)));
        item->setFlags(item->flags() & ~Qt::ItemIsEditable);

        ui->watch->setItem(i, 3, item);

        continue;
      }
    }
    else
    {
      regexp = QRegularExpression(lit("^(.+)(\\.[xyzwrgba]+)?(,[xfiudb])?$"));

      match = regexp.match(reg);

      if(match.hasMatch())
      {
        QString variablename = match.captured(1);

        RDTreeWidgetItem *local = findLocal(ui->locals->invisibleRootItem(), match.captured(1));

        if(local)
        {
          // TODO apply swizzle/typecast ?

          item = new QTableWidgetItem(local->text(1));
          item->setFlags(item->flags() & ~Qt::ItemIsEditable);
          ui->watch->setItem(i, 1, item);

          item = new QTableWidgetItem(local->text(2));
          item->setFlags(item->flags() & ~Qt::ItemIsEditable);
          ui->watch->setItem(i, 2, item);

          if(local->childCount() > 0)
          {
            // can't display structs
            item = new QTableWidgetItem(lit("{...}"));
            item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            ui->watch->setItem(i, 3, item);
          }
          else
          {
            item = new QTableWidgetItem(local->text(3));
            item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            ui->watch->setItem(i, 3, item);
          }

          continue;
        }
      }
    }

    item = new QTableWidgetItem();
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    ui->watch->setItem(i, 2, item);

    item = new QTableWidgetItem(tr("Error evaluating expression"));
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    ui->watch->setItem(i, 3, item);
  }

  ui->watch->setUpdatesEnabled(true);

  ui->constants->resizeColumnToContents(0);
  ui->registers->resizeColumnToContents(0);
  ui->constants->resizeColumnToContents(1);
  ui->registers->resizeColumnToContents(1);

  updateVariableTooltip();
}

const ShaderVariable *ShaderViewer::GetRegisterVariable(const RegisterRange &r)
{
  const ShaderDebugState &state = m_Trace->states[m_CurrentStep];

  const ShaderVariable *var = NULL;
  switch(r.type)
  {
    case RegisterType::Undefined: break;
    case RegisterType::Input:
      if(r.index < m_Trace->inputs.size())
        var = &m_Trace->inputs[r.index];
      break;
    case RegisterType::Temporary:
      if(r.index < state.registers.size())
        var = &state.registers[r.index];
      break;
    case RegisterType::IndexedTemporary:
      if(r.index < state.indexableTemps.size())
        var = &state.indexableTemps[r.index];
      break;
    case RegisterType::Output:
      if(r.index < state.outputs.size())
        var = &state.outputs[r.index];
      break;
  }

  return var;
}

void ShaderViewer::ensureLineScrolled(ScintillaEdit *s, int line)
{
  int firstLine = s->firstVisibleLine();
  int linesVisible = s->linesOnScreen();

  if(s->isVisible() && (line < firstLine || line > (firstLine + linesVisible)))
    s->setFirstVisibleLine(qMax(0, line - linesVisible / 2));
}

int ShaderViewer::CurrentStep()
{
  return m_CurrentStep;
}

void ShaderViewer::SetCurrentStep(int step)
{
  if(m_Trace && !m_Trace->states.empty())
    m_CurrentStep = qBound(0, step, m_Trace->states.count() - 1);
  else
    m_CurrentStep = 0;

  updateDebugging();
}

void ShaderViewer::ToggleBreakpoint(int instruction)
{
  sptr_t instLine = -1;

  if(instruction == -1)
  {
    ScintillaEdit *cur = currentScintilla();

    // search forward for an instruction
    if(cur != m_DisassemblyView)
    {
      int scintillaIndex = m_FileScintillas.indexOf(cur);

      if(scintillaIndex < 0)
        return;

      // add one to go from scintilla line numbers (0-based) to ours (1-based)
      sptr_t i = cur->lineFromPosition(cur->currentPos()) + 1;

      QMap<int32_t, size_t> &fileMap = m_Line2Inst[scintillaIndex];

      // find the next line that maps to an instruction
      for(; i < cur->lineCount(); i++)
      {
        if(fileMap.contains(i))
        {
          instruction = (int)fileMap[i];
          break;
        }
      }
    }
    else
    {
      instLine = m_DisassemblyView->lineFromPosition(m_DisassemblyView->currentPos());

      for(; instLine < m_DisassemblyView->lineCount(); instLine++)
      {
        instruction = instructionForLine(instLine);

        if(instruction >= 0)
          break;
      }
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

      const LineColumnInfo &lineInfo = m_Trace->lineInfo[instruction];

      if(lineInfo.fileIndex >= 0 && lineInfo.fileIndex < m_FileScintillas.count())
      {
        for(sptr_t line = lineInfo.lineStart; line <= (sptr_t)lineInfo.lineEnd; line++)
        {
          ScintillaEdit *s = m_FileScintillas[lineInfo.fileIndex];
          if(s)
          {
            m_FileScintillas[lineInfo.fileIndex]->markerDelete(line - 1, BREAKPOINT_MARKER);
            m_FileScintillas[lineInfo.fileIndex]->markerDelete(line - 1, BREAKPOINT_MARKER + 1);
          }
        }
      }
    }
    m_Breakpoints.removeOne(instruction);
  }
  else
  {
    if(instLine >= 0)
    {
      m_DisassemblyView->markerAdd(instLine, BREAKPOINT_MARKER);
      m_DisassemblyView->markerAdd(instLine, BREAKPOINT_MARKER + 1);

      const LineColumnInfo &lineInfo = m_Trace->lineInfo[instruction];

      if(lineInfo.fileIndex >= 0 && lineInfo.fileIndex < m_FileScintillas.count())
      {
        for(sptr_t line = lineInfo.lineStart; line <= (sptr_t)lineInfo.lineEnd; line++)
        {
          ScintillaEdit *s = m_FileScintillas[lineInfo.fileIndex];
          if(s)
          {
            m_FileScintillas[lineInfo.fileIndex]->markerAdd(line - 1, BREAKPOINT_MARKER);
            m_FileScintillas[lineInfo.fileIndex]->markerAdd(line - 1, BREAKPOINT_MARKER + 1);
          }
        }
      }
    }
    m_Breakpoints.push_back(instruction);
  }
}

void ShaderViewer::ShowErrors(const rdcstr &errors)
{
  if(m_Errors)
  {
    m_Errors->setReadOnly(false);
    m_Errors->setText(errors.c_str());
    m_Errors->setReadOnly(true);

    if(!errors.isEmpty())
      ToolWindowManager::raiseToolWindow(m_Errors);
  }
}

void ShaderViewer::AddWatch(const rdcstr &variable)
{
  int newRow = ui->watch->rowCount() - 1;
  ui->watch->insertRow(ui->watch->rowCount() - 1);

  ui->watch->setItem(newRow, 0, new QTableWidgetItem(variable));

  ToolWindowManager::raiseToolWindow(ui->watch);
  ui->watch->activateWindow();
  ui->watch->QWidget::setFocus();
}

int ShaderViewer::snippetPos()
{
  ShaderEncoding encoding = currentEncoding();

  if(encoding != ShaderEncoding::GLSL)
    return 0;

  if(m_Scintillas.isEmpty())
    return 0;

  QPair<int, int> ver =
      m_Scintillas[0]->findText(SCFIND_REGEXP, "#version.*", 0, m_Scintillas[0]->length());

  if(ver.first < 0)
    return 0;

  return ver.second + 1;
}

void ShaderViewer::insertSnippet(const QString &text)
{
  if(text.isEmpty())
    return;

  if(m_Scintillas.isEmpty())
    return;

  m_Scintillas[0]->insertText(snippetPos(), text.toUtf8().data());

  m_Scintillas[0]->setSelection(0, 0);
}

QString ShaderViewer::vulkanUBO()
{
  ShaderEncoding encoding = currentEncoding();

  if(encoding == ShaderEncoding::GLSL)
  {
    return lit(R"(
layout(binding = 0, std140) uniform RENDERDOC_Uniforms
{
    uvec4 TexDim;
    uint SelectedMip;
    int TextureType;
    uint SelectedSliceFace;
    int SelectedSample;
    uvec4 YUVDownsampleRate;
    uvec4 YUVAChannels;
} RENDERDOC;

)");
  }
  else if(encoding == ShaderEncoding::HLSL)
  {
    return lit(R"(
cbuffer RENDERDOC_Constants : register(b0)
{
    uint4 RENDERDOC_TexDim;
    uint RENDERDOC_SelectedMip;
    int RENDERDOC_TextureType;
    uint RENDERDOC_SelectedSliceFace;
    int RENDERDOC_SelectedSample;
    uint4 RENDERDOC_YUVDownsampleRate;
    uint4 RENDERDOC_YUVAChannels;
};

)");
  }
  else if(encoding == ShaderEncoding::SPIRVAsm)
  {
    return lit("; Can't insert snippets for SPIR-V ASM");
  }

  return QString();
}

void ShaderViewer::snippet_textureDimensions()
{
  ShaderEncoding encoding = currentEncoding();
  GraphicsAPI api = m_Ctx.APIProps().localRenderer;

  QString text;

  if(api == GraphicsAPI::Vulkan)
  {
    text = vulkanUBO();
  }
  else if(encoding == ShaderEncoding::HLSL)
  {
    text = lit(R"(
// xyz == width, height, depth. w == # mips
uint4 RENDERDOC_TexDim;
uint4 RENDERDOC_YUVDownsampleRate;
uint4 RENDERDOC_YUVAChannels;

)");
  }
  else if(encoding == ShaderEncoding::GLSL)
  {
    text = lit(R"(
// xyz == width, height, depth. w == # mips
uniform uvec4 RENDERDOC_TexDim;

)");
  }
  else if(encoding == ShaderEncoding::SPIRVAsm)
  {
    text = lit("; Can't insert snippets for SPIR-V ASM");
  }

  insertSnippet(text);
}

void ShaderViewer::snippet_selectedMip()
{
  ShaderEncoding encoding = currentEncoding();
  GraphicsAPI api = m_Ctx.APIProps().localRenderer;

  QString text;

  if(api == GraphicsAPI::Vulkan)
  {
    text = vulkanUBO();
  }
  else if(encoding == ShaderEncoding::HLSL)
  {
    text = lit(R"(
// selected mip in UI
uint RENDERDOC_SelectedMip;

)");
  }
  else if(encoding == ShaderEncoding::GLSL)
  {
    text = lit(R"(
// selected mip in UI
uniform uint RENDERDOC_SelectedMip;

)");
  }
  else if(encoding == ShaderEncoding::SPIRVAsm)
  {
    text = lit("; Can't insert snippets for SPIR-V ASM");
  }

  insertSnippet(text);
}

void ShaderViewer::snippet_selectedSlice()
{
  ShaderEncoding encoding = currentEncoding();
  GraphicsAPI api = m_Ctx.APIProps().localRenderer;

  QString text;

  if(api == GraphicsAPI::Vulkan)
  {
    text = vulkanUBO();
  }
  else if(encoding == ShaderEncoding::HLSL)
  {
    text = lit(R"(
// selected array slice or cubemap face in UI
uint RENDERDOC_SelectedSliceFace;

)");
  }
  else if(encoding == ShaderEncoding::GLSL)
  {
    text = lit(R"(
// selected array slice or cubemap face in UI
uniform uint RENDERDOC_SelectedSliceFace;

)");
  }
  else if(encoding == ShaderEncoding::SPIRVAsm)
  {
    text = lit("; Can't insert snippets for SPIR-V ASM");
  }

  insertSnippet(text);
}

void ShaderViewer::snippet_selectedSample()
{
  ShaderEncoding encoding = currentEncoding();
  GraphicsAPI api = m_Ctx.APIProps().localRenderer;

  QString text;

  if(api == GraphicsAPI::Vulkan)
  {
    text = vulkanUBO();
  }
  else if(encoding == ShaderEncoding::HLSL)
  {
    text = lit(R"(
// selected MSAA sample or -numSamples for resolve. See docs
int RENDERDOC_SelectedSample;

)");
  }
  else if(encoding == ShaderEncoding::GLSL)
  {
    text = lit(R"(
// selected MSAA sample or -numSamples for resolve. See docs
uniform int RENDERDOC_SelectedSample;

)");
  }
  else if(encoding == ShaderEncoding::SPIRVAsm)
  {
    text = lit("; Can't insert snippets for SPIR-V ASM");
  }

  insertSnippet(text);
}

void ShaderViewer::snippet_selectedType()
{
  ShaderEncoding encoding = currentEncoding();
  GraphicsAPI api = m_Ctx.APIProps().localRenderer;

  QString text;

  if(api == GraphicsAPI::Vulkan)
  {
    text = vulkanUBO();
  }
  else if(encoding == ShaderEncoding::HLSL)
  {
    text = lit(R"(
// 1 = 1D, 2 = 2D, 3 = 3D, 4 = Depth, 5 = Depth + Stencil
// 6 = Depth (MS), 7 = Depth + Stencil (MS)
uint RENDERDOC_TextureType;

)");
  }
  else if(encoding == ShaderEncoding::GLSL)
  {
    text = lit(R"(
// 1 = 1D, 2 = 2D, 3 = 3D, 4 = Cube
// 5 = 1DArray, 6 = 2DArray, 7 = CubeArray
// 8 = Rect, 9 = Buffer, 10 = 2DMS
uniform uint RENDERDOC_TextureType;

)");
  }
  else if(encoding == ShaderEncoding::SPIRVAsm)
  {
    text = lit("; Can't insert snippets for SPIR-V ASM");
  }

  insertSnippet(text);
}

void ShaderViewer::snippet_samplers()
{
  ShaderEncoding encoding = currentEncoding();
  GraphicsAPI api = m_Ctx.APIProps().localRenderer;

  if(encoding == ShaderEncoding::HLSL)
  {
    if(api == GraphicsAPI::Vulkan)
    {
      insertSnippet(lit(R"(
// Samplers
SamplerState pointSampler : register(s50);
SamplerState linearSampler : register(s51);
// End Samplers
)"));
    }
    else
    {
      insertSnippet(lit(R"(
// Samplers
SamplerState pointSampler : register(s0);
SamplerState linearSampler : register(s1);
// End Samplers
)"));
    }
  }
}

void ShaderViewer::snippet_resources()
{
  ShaderEncoding encoding = currentEncoding();
  GraphicsAPI api = m_Ctx.APIProps().localRenderer;

  if(encoding == ShaderEncoding::HLSL)
  {
    if(api == GraphicsAPI::Vulkan)
    {
      insertSnippet(lit(R"(
// Textures
// Floating point
Texture1DArray<float4> texDisplayTex1DArray : register(t6);
Texture2DArray<float4> texDisplayTex2DArray : register(t7);
Texture3D<float4> texDisplayTex3D : register(t8);
Texture2DMSArray<float4> texDisplayTex2DMSArray : register(t9);
Texture2DArray<float4> texDisplayYUVArray : register(t10);

// Unsigned int samplers
Texture1DArray<uint4> texDisplayUIntTex1DArray : register(t11);
Texture2DArray<uint4> texDisplayUIntTex2DArray : register(t12);
Texture3D<uint4> texDisplayUIntTex3D : register(t13);
Texture2DMSArray<uint4> texDisplayUIntTex2DMSArray : register(t14);

// Int samplers
Texture1DArray<int4> texDisplayIntTex1DArray : register(t16);
Texture2DArray<int4> texDisplayIntTex2DArray : register(t17);
Texture3D<int4> texDisplayIntTex3D : register(t18);
Texture2DMSArray<int4> texDisplayIntTex2DMSArray : register(t19);
// End Textures
)"));
    }
    else
    {
      insertSnippet(lit(R"(
// Textures
Texture1DArray<float4> texDisplayTex1DArray : register(t1);
Texture2DArray<float4> texDisplayTex2DArray : register(t2);
Texture3D<float4> texDisplayTex3D : register(t3);
Texture2DArray<float2> texDisplayTexDepthArray : register(t4);
Texture2DArray<uint2> texDisplayTexStencilArray : register(t5);
Texture2DMSArray<float2> texDisplayTexDepthMSArray : register(t6);
Texture2DMSArray<uint2> texDisplayTexStencilMSArray : register(t7);
Texture2DMSArray<float4> texDisplayTex2DMSArray : register(t9);
Texture2DArray<float4> texDisplayYUVArray : register(t10);

// Unsigned int samplers
Texture1DArray<uint4> texDisplayUIntTex1DArray : register(t11);
Texture2DArray<uint4> texDisplayUIntTex2DArray : register(t12);
Texture3D<uint4> texDisplayUIntTex3D : register(t13);
Texture2DMSArray<uint4> texDisplayUIntTex2DMSArray : register(t19);

// Int samplers
Texture1DArray<int4> texDisplayIntTex1DArray : register(t21);
Texture2DArray<int4> texDisplayIntTex2DArray : register(t22);
Texture3D<int4> texDisplayIntTex3D : register(t23);
Texture2DMSArray<int4> texDisplayIntTex2DMSArray : register(t29);
// End Textures
)"));
    }
  }
  else if(encoding == ShaderEncoding::GLSL)
  {
    if(api == GraphicsAPI::Vulkan)
    {
      insertSnippet(lit(R"(
// Textures
// Floating point samplers
layout(binding = 6) uniform sampler1DArray tex1DArray;
layout(binding = 7) uniform sampler2DArray tex2DArray;
layout(binding = 8) uniform sampler3D tex3D;
layout(binding = 9) uniform sampler2DMS tex2DMS;
layout(binding = 10) uniform sampler2DArray texYUVArray[2];

// Unsigned int samplers
layout(binding = 11) uniform usampler1DArray texUInt1DArray;
layout(binding = 12) uniform usampler2DArray texUInt2DArray;
layout(binding = 13) uniform usampler3D texUInt3D;
layout(binding = 14) uniform usampler2DMS texUInt2DMS;

// Int samplers
layout(binding = 16) uniform isampler1DArray texSInt1DArray;
layout(binding = 17) uniform isampler2DArray texSInt2DArray;
layout(binding = 18) uniform isampler3D texSInt3D;
layout(binding = 19) uniform isampler2DMS texSInt2DMS;
// End Textures
)"));
    }
    else
    {
      insertSnippet(lit(R"(
// Textures
// Unsigned int samplers
layout (binding = 1) uniform usampler1D texUInt1D;
layout (binding = 2) uniform usampler2D texUInt2D;
layout (binding = 3) uniform usampler3D texUInt3D;
// cube = 4
layout (binding = 5) uniform usampler1DArray texUInt1DArray;
layout (binding = 6) uniform usampler2DArray texUInt2DArray;
// cube array = 7
layout (binding = 8) uniform usampler2DRect texUInt2DRect;
layout (binding = 9) uniform usamplerBuffer texUIntBuffer;
layout (binding = 10) uniform usampler2DMS texUInt2DMS;

// Int samplers
layout (binding = 1) uniform isampler1D texSInt1D;
layout (binding = 2) uniform isampler2D texSInt2D;
layout (binding = 3) uniform isampler3D texSInt3D;
// cube = 4
layout (binding = 5) uniform isampler1DArray texSInt1DArray;
layout (binding = 6) uniform isampler2DArray texSInt2DArray;
// cube array = 7
layout (binding = 8) uniform isampler2DRect texSInt2DRect;
layout (binding = 9) uniform isamplerBuffer texSIntBuffer;
layout (binding = 10) uniform isampler2DMS texSInt2DMS;

// Floating point samplers
layout (binding = 1) uniform sampler1D tex1D;
layout (binding = 2) uniform sampler2D tex2D;
layout (binding = 3) uniform sampler3D tex3D;
layout (binding = 4) uniform samplerCube texCube;
layout (binding = 5) uniform sampler1DArray tex1DArray;
layout (binding = 6) uniform sampler2DArray tex2DArray;
layout (binding = 7) uniform samplerCubeArray texCubeArray;
layout (binding = 8) uniform sampler2DRect tex2DRect;
layout (binding = 9) uniform samplerBuffer texBuffer;
layout (binding = 10) uniform sampler2DMS tex2DMS;
// End Textures
)"));
    }
  }
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
  if(!m_Trace || m_CurrentStep < 0 || m_CurrentStep >= m_Trace->states.count())
    return;

  ScintillaEdit *sc = qobject_cast<ScintillaEdit *>(QObject::sender());

  if(!sc)
    return;

  // ignore any messages if we're already outside the viewport
  if(!sc->rect().contains(sc->mapFromGlobal(QCursor::pos())))
    return;

  if(sc->isVisible())
  {
    sptr_t scintillaPos = sc->positionFromPoint(x, y);

    sptr_t start = sc->wordStartPosition(scintillaPos, true);
    sptr_t end = sc->wordEndPosition(scintillaPos, true);

    do
    {
      // expand leftwards through simple struct . access
      // TODO handle arrays
      while(isspace(sc->charAt(start - 1)))
        start--;

      if(sc->charAt(start - 1) == '.')
        start = sc->wordStartPosition(start - 2, true);
      else
        break;
    } while(true);

    QString text = QString::fromUtf8(sc->textRange(start, end)).trimmed();

    if(!text.isEmpty())
      showVariableTooltip(text);
  }
}

void ShaderViewer::disasm_tooltipHide(int x, int y)
{
  hideVariableTooltip();
}

void ShaderViewer::showVariableTooltip(VariableCategory varCat, int varIdx, int arrayIdx)
{
  const rdcarray<ShaderVariable> *vars = GetVariableList(varCat, arrayIdx);

  if(!vars || varIdx < 0 || varIdx >= vars->count())
  {
    m_TooltipVarIdx = -1;
    return;
  }

  m_TooltipVarCat = varCat;
  m_TooltipName = QString();
  m_TooltipVarIdx = varIdx;
  m_TooltipArrayIdx = arrayIdx;
  m_TooltipPos = QCursor::pos();

  updateVariableTooltip();
}

void ShaderViewer::showVariableTooltip(QString name)
{
  VariableCategory varCat;
  int varIdx;
  int arrayIdx;
  getRegisterFromWord(name, varCat, varIdx, arrayIdx);

  if(varCat != VariableCategory::Unknown)
    showVariableTooltip(varCat, varIdx, arrayIdx);

  m_TooltipVarCat = VariableCategory::ByString;
  m_TooltipName = name;
  m_TooltipPos = QCursor::pos();

  updateVariableTooltip();
}

const rdcarray<ShaderVariable> *ShaderViewer::GetVariableList(VariableCategory varCat, int arrayIdx)
{
  const rdcarray<ShaderVariable> *vars = NULL;

  if(!m_Trace || m_CurrentStep < 0 || m_CurrentStep >= m_Trace->states.count())
    return vars;

  const ShaderDebugState &state = m_Trace->states[m_CurrentStep];

  arrayIdx = qMax(0, arrayIdx);

  switch(varCat)
  {
    case VariableCategory::ByString:
    case VariableCategory::Unknown: vars = NULL; break;
    case VariableCategory::Temporaries: vars = &state.registers; break;
    case VariableCategory::IndexTemporaries:
      vars = arrayIdx < state.indexableTemps.count() ? &state.indexableTemps[arrayIdx].members : NULL;
      break;
    case VariableCategory::Inputs: vars = &m_Trace->inputs; break;
    case VariableCategory::Constants:
      vars = arrayIdx < m_Trace->constantBlocks.count() ? &m_Trace->constantBlocks[arrayIdx].members
                                                        : NULL;
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
  if(!m_Trace || m_CurrentStep < 0 || m_CurrentStep >= m_Trace->states.count())
    return;

  const ShaderDebugState &state = m_Trace->states[m_CurrentStep];

  if(m_TooltipVarCat == VariableCategory::ByString)
  {
    if(m_TooltipName.isEmpty())
      return;

    // first check the constants
    QString bracketedName = lit("(") + m_TooltipName;
    QString commaName = lit(", ") + m_TooltipName;
    QList<ShaderVariable> constants;
    for(ShaderVariable &block : m_Trace->constantBlocks)
    {
      for(ShaderVariable &c : block.members)
      {
        QString cname = c.name;

        // this is a hack for now :(.
        if(cname == m_TooltipName)
          constants.push_back(c);

        int idx = cname.indexOf(bracketedName);
        if(idx >= 0 && (cname.at(idx + bracketedName.length()) == QLatin1Char(',') ||
                        cname.at(idx + bracketedName.length()) == QLatin1Char(')')))
          constants.push_back(c);

        idx = cname.indexOf(commaName);
        if(idx >= 0 && (cname.at(idx + commaName.length()) == QLatin1Char(',') ||
                        cname.at(idx + commaName.length()) == QLatin1Char(')')))
          constants.push_back(c);
      }
    }

    if(constants.count() == 1)
    {
      QToolTip::showText(
          m_TooltipPos,
          QFormatStr("<pre>%1: %2</pre>").arg(constants[0].name).arg(RowString(constants[0], 0)));
      return;
    }
    else if(constants.count() > 1)
    {
      QString tooltip =
          QFormatStr("<pre>%1: %2\n").arg(m_TooltipName).arg(RowString(constants[0], 0));
      QString spacing = QString(m_TooltipName.length(), QLatin1Char(' '));
      for(int i = 1; i < constants.count(); i++)
        tooltip += QFormatStr("%1  %2\n").arg(spacing).arg(RowString(constants[i], 0));
      tooltip += lit("</pre>");
      QToolTip::showText(m_TooltipPos, tooltip);
      return;
    }

    // now check locals (if there are any)
    for(const LocalVariableMapping &l : state.locals)
    {
      if(QString(l.localName) == m_TooltipName)
      {
        QString tooltip = QFormatStr("<pre>%1: ").arg(m_TooltipName);

        for(uint32_t i = 0; i < l.regCount; i++)
        {
          const RegisterRange &r = l.registers[i];

          if(i > 0)
            tooltip += lit(", ");

          if(r.type == RegisterType::Undefined)
          {
            tooltip += lit("?");
            continue;
          }

          const ShaderVariable *var = GetRegisterVariable(r);

          if(var)
          {
            if(l.type == VarType::UInt)
              tooltip += Formatter::Format(var->value.uv[r.component]);
            else if(l.type == VarType::SInt)
              tooltip += Formatter::Format(var->value.iv[r.component]);
            else if(l.type == VarType::Float)
              tooltip += Formatter::Format(var->value.fv[r.component]);
            else if(l.type == VarType::Double)
              tooltip += Formatter::Format(var->value.dv[r.component]);
          }
          else
          {
            tooltip += lit("&lt;error&gt;");
          }
        }

        tooltip += lit("</pre>");

        QToolTip::showText(m_TooltipPos, tooltip);
        return;
      }
    }

    return;
  }

  if(m_TooltipVarIdx < 0)
    return;

  const rdcarray<ShaderVariable> *vars = GetVariableList(m_TooltipVarCat, m_TooltipArrayIdx);
  const ShaderVariable &var = (*vars)[m_TooltipVarIdx];

  QString text = QFormatStr("<pre>%1\n").arg(var.name);
  text +=
      lit("                  X           Y           Z           W \n"
          "--------------------------------------------------------\n");

  text += QFormatStr("float | %1 %2 %3 %4\n")
              .arg(Formatter::Format(var.value.fv[0]), 11)
              .arg(Formatter::Format(var.value.fv[1]), 11)
              .arg(Formatter::Format(var.value.fv[2]), 11)
              .arg(Formatter::Format(var.value.fv[3]), 11);
  text += QFormatStr("uint  | %1 %2 %3 %4\n")
              .arg(var.value.uv[0], 11, 10, QLatin1Char(' '))
              .arg(var.value.uv[1], 11, 10, QLatin1Char(' '))
              .arg(var.value.uv[2], 11, 10, QLatin1Char(' '))
              .arg(var.value.uv[3], 11, 10, QLatin1Char(' '));
  text += QFormatStr("int   | %1 %2 %3 %4\n")
              .arg(var.value.iv[0], 11, 10, QLatin1Char(' '))
              .arg(var.value.iv[1], 11, 10, QLatin1Char(' '))
              .arg(var.value.iv[2], 11, 10, QLatin1Char(' '))
              .arg(var.value.iv[3], 11, 10, QLatin1Char(' '));
  text += QFormatStr("hex   |    %1    %2    %3    %4")
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
  m_TooltipName = QString();
}

bool ShaderViewer::isSourceDebugging()
{
  return !m_DisassemblyFrame->isVisible();
}

ShaderEncoding ShaderViewer::currentEncoding()
{
  int idx = ui->encoding->currentIndex();
  if(idx >= 0 && idx < m_Encodings.count())
    return m_Encodings[idx];

  return ShaderEncoding::Unknown;
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

void ShaderViewer::PopulateCompileTools()
{
  ShaderEncoding encoding = currentEncoding();
  rdcarray<ShaderEncoding> accepted = m_Ctx.TargetShaderEncodings();

  QStringList strs;
  strs.clear();
  for(const ShaderProcessingTool &tool : m_Ctx.Config().ShaderProcessors)
  {
    // skip tools that can't accept our inputs, or doesn't produce a supported output
    if(tool.input != encoding || accepted.indexOf(tool.output) < 0)
      continue;

    strs << tool.name;
  }

  // if we can pass in the shader source as-is, add a built-in option
  if(accepted.indexOf(encoding) >= 0)
    strs << tr("Builtin");

  ui->compileTool->clear();
  ui->compileTool->addItems(strs);

  // pick the first option as highest priority
  ui->compileTool->setCurrentIndex(0);

  // fill out parameters
  PopulateCompileToolParameters();

  if(strs.isEmpty())
  {
    ShowErrors(tr("No compilation tool found that takes %1 as input and produces compatible output")
                   .arg(ToQStr(encoding)));
  }
}

void ShaderViewer::PopulateCompileToolParameters()
{
  ShaderEncoding encoding = currentEncoding();
  rdcarray<ShaderEncoding> accepted = m_Ctx.TargetShaderEncodings();

  ui->toolCommandLine->clear();

  if(accepted.indexOf(encoding) >= 0 &&
     ui->compileTool->currentIndex() == ui->compileTool->count() - 1)
  {
    // if we're using the last Builtin tool, there are no default parameters
  }
  else
  {
    for(const ShaderProcessingTool &tool : m_Ctx.Config().ShaderProcessors)
    {
      if(QString(tool.name) == ui->compileTool->currentText())
      {
        ui->toolCommandLine->setPlainText(tool.DefaultArguments());
        ui->toolCommandLine->setEnabled(true);
        break;
      }
    }
  }

  for(int i = 0; i < m_Flags.flags.count(); i++)
  {
    ShaderCompileFlag &flag = m_Flags.flags[i];
    if(flag.name == "@cmdline")
    {
      // append command line from saved flags
      ui->toolCommandLine->setPlainText(ui->toolCommandLine->toPlainText() +
                                        lit(" %1").arg(flag.value));
      break;
    }
  }
}

bool ShaderViewer::ProcessIncludeDirectives(QString &source, const rdcstrpairs &files)
{
  // try and match up #includes against the files that we have. This isn't always
  // possible as fxc only seems to include the source for files if something in
  // that file was included in the compiled output. So you might end up with
  // dangling #includes - we just have to ignore them
  int offs = source.indexOf(lit("#include"));

  while(offs >= 0)
  {
    // search back to ensure this is a valid #include (ie. not in a comment).
    // Must only see whitespace before, then a newline.
    int ws = qMax(0, offs - 1);
    while(ws >= 0 && (source[ws] == QLatin1Char(' ') || source[ws] == QLatin1Char('\t')))
      ws--;

    // not valid? jump to next.
    if(ws > 0 && source[ws] != QLatin1Char('\n'))
    {
      offs = source.indexOf(lit("#include"), offs + 1);
      continue;
    }

    int start = ws + 1;

    bool tail = true;

    int lineEnd = source.indexOf(QLatin1Char('\n'), start + 1);
    if(lineEnd == -1)
    {
      lineEnd = source.length();
      tail = false;
    }

    ws = offs + sizeof("#include") - 1;
    while(source[ws] == QLatin1Char(' ') || source[ws] == QLatin1Char('\t'))
      ws++;

    QString line = source.mid(offs, lineEnd - offs + 1);

    if(source[ws] != QLatin1Char('<') && source[ws] != QLatin1Char('"'))
    {
      ShowErrors(tr("Invalid #include directive found:\r\n") + line);
      return false;
    }

    // find matching char, either <> or "";
    int end =
        source.indexOf(source[ws] == QLatin1Char('"') ? QLatin1Char('"') : QLatin1Char('>'), ws + 1);

    if(end == -1)
    {
      ShowErrors(tr("Invalid #include directive found:\r\n") + line);
      return false;
    }

    QString fname = source.mid(ws + 1, end - ws - 1);

    QString fileText;

    // look for exact match first
    for(int i = 0; i < files.count(); i++)
    {
      if(QString(files[i].first) == fname)
      {
        fileText = files[i].second;
        break;
      }
    }

    if(fileText.isEmpty())
    {
      QString search = QFileInfo(fname).fileName();

      // if not, try and find the same filename (this is not proper include handling!)
      for(const rdcstrpair &kv : files)
      {
        if(QFileInfo(kv.first).fileName().compare(search, Qt::CaseInsensitive) == 0)
        {
          fileText = kv.second;
          break;
        }
      }

      if(fileText.isEmpty())
        fileText = QFormatStr("// Can't find file %1\n").arg(fname);
    }

    source = source.left(offs) + lit("\n\n") + fileText + lit("\n\n") +
             (tail ? source.mid(lineEnd + 1) : QString());

    // need to start searching from the beginning - wasteful but allows nested includes to
    // work
    offs = source.indexOf(lit("#include"));
  }

  for(const rdcstrpair &kv : files)
  {
    if(kv.first == "@cmdline")
      source = QString(kv.second) + lit("\n\n") + source;
  }

  return true;
}

void ShaderViewer::on_refresh_clicked()
{
  if(m_Trace)
  {
    m_Ctx.GetPipelineViewer()->SaveShaderFile(m_ShaderDetails);
    return;
  }

  ShaderEncoding encoding = currentEncoding();

  // if we don't have any compile tools - even the 'builtin' one, this compilation is not going to
  // succeed.
  if(ui->compileTool->count() == 0 && !m_CustomShader)
  {
    ShowErrors(tr("No compilation tool found that takes %1 as input and produces compatible output")
                   .arg(ToQStr(encoding)));
  }
  else if(m_SaveCallback)
  {
    rdcstrpairs files;
    for(ScintillaEdit *s : m_Scintillas)
    {
      QWidget *w = (QWidget *)s;
      files.push_back(
          {w->property("filename").toString(), QString::fromUtf8(s->getText(s->textLength() + 1))});
    }

    if(files.isEmpty())
      return;

    QString source = files[0].second;

    if(encoding == ShaderEncoding::HLSL || encoding == ShaderEncoding::GLSL)
    {
      bool success = ProcessIncludeDirectives(source, files);
      if(!success)
        return;
    }

    bytebuf shaderBytes(source.toUtf8());

    rdcarray<ShaderEncoding> accepted = m_Ctx.TargetShaderEncodings();

    if(m_CustomShader || (accepted.indexOf(encoding) >= 0 &&
                          ui->compileTool->currentIndex() == ui->compileTool->count() - 1))
    {
      // if using the builtin compiler, just pass through
    }
    else
    {
      for(const ShaderProcessingTool &tool : m_Ctx.Config().ShaderProcessors)
      {
        if(QString(tool.name) == ui->compileTool->currentText())
        {
          ShaderToolOutput out = tool.CompileShader(this, source, ui->entryFunc->text(), m_Stage,
                                                    ui->toolCommandLine->toPlainText());

          ShowErrors(out.log);

          if(out.result.isEmpty())
            return;

          encoding = tool.output;
          shaderBytes = out.result;
          break;
        }
      }
    }

    ShaderCompileFlags flags = m_Flags;

    bool found = false;
    for(ShaderCompileFlag &f : flags.flags)
    {
      if(f.name == "@cmdline")
      {
        f.value = ui->toolCommandLine->toPlainText();
        found = true;
        break;
      }
    }

    if(!found)
      flags.flags.push_back({"@cmdline", ui->toolCommandLine->toPlainText()});

    m_SaveCallback(&m_Ctx, this, encoding, flags, ui->entryFunc->text(), shaderBytes);
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

void ShaderViewer::on_debugToggle_clicked()
{
  if(isSourceDebugging())
    gotoDisassemblyDebugging();
  else
    gotoSourceDebugging();

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
