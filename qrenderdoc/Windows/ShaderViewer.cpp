/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2020 Baldur Karlsson
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
#include "Code/ScintillaSyntax.h"
#include "Widgets/FindReplace.h"
#include "scintilla/include/SciLexer.h"
#include "scintilla/include/qt/ScintillaEdit.h"
#include "toolwindowmanager/ToolWindowManager.h"
#include "toolwindowmanager/ToolWindowManagerArea.h"
#include "ui_ShaderViewer.h"

namespace
{
struct VariableTag
{
  VariableTag() : offset(0), globalSourceVar(-1), localSourceVar(-1) {}
  VariableTag(rdcstr name, uint32_t offs, int32_t globalVar, int32_t localVar)
      : offset(offs), globalSourceVar(globalVar), localSourceVar(localVar)
  {
    debugVar.name = name;
  }
  VariableTag(DebugVariableReference var) : offset(0), debugVar(var) {}
  uint32_t offset;
  int32_t globalSourceVar;
  int32_t localSourceVar;

  DebugVariableReference debugVar;
};

struct AccessedResourceTag
{
  AccessedResourceTag() : type(VarType::Unknown), step(0) { bind.bind = -1; }
  AccessedResourceTag(uint32_t s) : type(VarType::Unknown), step(s) { bind.bind = -1; }
  AccessedResourceTag(BindpointIndex bp, VarType t) : bind(bp), type(t), step(0) {}
  AccessedResourceTag(ShaderVariable var) : step(0), type(var.type)
  {
    if(var.type == VarType::ReadOnlyResource || var.type == VarType::ReadWriteResource)
      bind = var.GetBinding();
    else
      bind.bind = -1;
  }
  BindpointIndex bind;
  VarType type;
  uint32_t step;
};
};

Q_DECLARE_METATYPE(VariableTag);
Q_DECLARE_METATYPE(AccessedResourceTag);

ShaderViewer::ShaderViewer(ICaptureContext &ctx, QWidget *parent)
    : QFrame(parent), ui(new Ui::ShaderViewer), m_Ctx(ctx)
{
  ui->setupUi(this);

  ui->constants->setFont(Formatter::PreferredFont());
  ui->accessedResources->setFont(Formatter::PreferredFont());
  ui->debugVars->setFont(Formatter::PreferredFont());
  ui->sourceVars->setFont(Formatter::PreferredFont());
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

void ShaderViewer::editShader(ResourceId id, ShaderStage stage, const QString &entryPoint,
                              const rdcstrpairs &files, ShaderEncoding shaderEncoding,
                              ShaderCompileFlags flags)
{
  m_Scintillas.removeOne(m_DisassemblyView);
  ui->docking->removeToolWindow(m_DisassemblyFrame);

  m_DisassemblyView = NULL;

  m_Stage = stage;
  m_Flags = flags;

  m_CustomShader = (id == ResourceId());

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
  if(m_CustomShader)
    ui->compilationGroup->hide();

  // hide debugging windows
  ui->watch->hide();
  ui->debugVars->hide();
  ui->constants->hide();
  ui->resourcesPanel->hide();
  ui->callstack->hide();
  ui->sourceVars->hide();

  ui->snippets->setVisible(m_CustomShader);

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
      title = tr(" - %1 - %2()").arg(name).arg(entryPoint);
  }

  if(sel != NULL)
    ToolWindowManager::raiseToolWindow(sel);

  if(m_CustomShader)
    title.prepend(tr("Editing %1 Shader").arg(ToQStr(stage, m_Ctx.APIProps().pipelineType)));
  else
    title.prepend(tr("Editing %1").arg(m_Ctx.GetResourceNameUnsuffixed(id)));

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

  if(!m_CustomShader)
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

void ShaderViewer::cacheResources()
{
  m_ReadOnlyResources = m_Ctx.CurPipelineState().GetReadOnlyResources(m_Stage, false);
  m_ReadWriteResources = m_Ctx.CurPipelineState().GetReadWriteResources(m_Stage, false);
}

void ShaderViewer::debugShader(const ShaderBindpointMapping *bind, const ShaderReflection *shader,
                               ResourceId pipeline, ShaderDebugTrace *trace,
                               const QString &debugContext)
{
  if(bind)
    m_Mapping = *bind;
  m_ShaderDetails = shader;
  m_Pipeline = pipeline;
  m_Trace = trace;
  m_Stage = ShaderStage::Vertex;
  m_DebugContext = debugContext;

  // no recompilation happening, hide that group
  ui->compilationGroup->hide();

  // no replacing allowed, stay in find mode
  m_FindReplace->allowUserModeChange(false);

  if(!bind || !m_ShaderDetails)
    m_Trace = NULL;

  if(m_ShaderDetails)
  {
    m_Stage = m_ShaderDetails->stage;

    QPointer<ShaderViewer> me(this);

    m_Ctx.Replay().AsyncInvoke([me, this](IReplayController *r) {
      if(!me)
        return;

      rdcarray<rdcstr> targets = r->GetDisassemblyTargets(m_Pipeline != ResourceId());

      if(m_Pipeline == ResourceId())
      {
        rdcarray<rdcstr> pipelineTargets = r->GetDisassemblyTargets(true);

        if(pipelineTargets.size() > targets.size())
        {
          m_PipelineTargets = pipelineTargets;
          m_PipelineTargets.removeIf([&targets](const rdcstr &t) { return targets.contains(t); });
        }
      }

      rdcstr disasm = r->DisassembleShader(m_Pipeline, m_ShaderDetails, "");

      if(!me)
        return;

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

        if(!m_PipelineTargets.empty())
          targetNames << tr("More disassembly formats...");

        m_DisassemblyType->clear();
        m_DisassemblyType->addItems(targetNames);
        m_DisassemblyType->setCurrentIndex(0);
        QObject::connect(m_DisassemblyType, OverloadedSlot<int>::of(&QComboBox::currentIndexChanged),
                         this, &ShaderViewer::disassemble_typeChanged);

        // read-only applies to us too!
        m_DisassemblyView->setReadOnly(false);
        SetTextAndUpdateMargin0(m_DisassemblyView, disasm);
        m_DisassemblyView->setReadOnly(true);
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
      ui->debugToggle->setText(tr("Source Unavailable"));
    }

    ui->debugVars->setColumns({tr("Name"), tr("Value")});
    ui->debugVars->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    ui->debugVars->header()->setSectionResizeMode(1, QHeaderView::Interactive);

    ui->sourceVars->setColumns({tr("Name"), tr("Register(s)"), tr("Type"), tr("Value")});
    ui->sourceVars->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    ui->sourceVars->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    ui->sourceVars->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    ui->sourceVars->header()->setSectionResizeMode(3, QHeaderView::Interactive);

    ui->constants->setColumns({tr("Name"), tr("Register(s)"), tr("Type"), tr("Value")});
    ui->constants->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    ui->constants->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    ui->constants->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    ui->constants->header()->setSectionResizeMode(3, QHeaderView::Interactive);

    ui->constants->header()->resizeSection(0, 80);

    ui->accessedResources->setColumns({tr("Location"), tr("Type"), tr("Info")});
    ui->accessedResources->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    ui->accessedResources->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    ui->accessedResources->header()->setSectionResizeMode(2, QHeaderView::Interactive);

    ui->accessedResources->header()->resizeSection(0, 80);

    ui->debugVars->setTooltipElidedItems(false);
    ui->constants->setTooltipElidedItems(false);
    ui->accessedResources->setTooltipElidedItems(false);

    ToolWindowManager::ToolWindowProperty windowProps =
        ToolWindowManager::HideCloseButton | ToolWindowManager::DisallowFloatWindow;
    ui->watch->setWindowTitle(tr("Watch"));
    ui->docking->addToolWindow(
        ui->watch, ToolWindowManager::AreaReference(ToolWindowManager::BottomOf,
                                                    ui->docking->areaOf(m_DisassemblyFrame), 0.25f));
    ui->docking->setToolWindowProperties(ui->watch, windowProps);

    ui->debugVars->setWindowTitle(tr("Variable Values"));
    ui->docking->addToolWindow(
        ui->debugVars,
        ToolWindowManager::AreaReference(ToolWindowManager::AddTo, ui->docking->areaOf(ui->watch)));
    ui->docking->setToolWindowProperties(ui->debugVars, windowProps);

    ui->constants->setWindowTitle(tr("Constants && Resources"));
    ui->docking->addToolWindow(
        ui->constants, ToolWindowManager::AreaReference(ToolWindowManager::LeftOf,
                                                        ui->docking->areaOf(ui->debugVars), 0.5f));
    ui->docking->setToolWindowProperties(ui->constants, windowProps);

    ui->resourcesPanel->setWindowTitle(tr("Accessed Resources"));
    ui->docking->addToolWindow(
        ui->resourcesPanel, ToolWindowManager::AreaReference(ToolWindowManager::AddTo,
                                                             ui->docking->areaOf(ui->constants)));
    ui->docking->setToolWindowProperties(ui->resourcesPanel, windowProps);
    ui->docking->raiseToolWindow(ui->constants);

    ui->callstack->setWindowTitle(tr("Callstack"));
    ui->docking->addToolWindow(
        ui->callstack, ToolWindowManager::AreaReference(ToolWindowManager::RightOf,
                                                        ui->docking->areaOf(ui->debugVars), 0.2f));
    ui->docking->setToolWindowProperties(ui->callstack, windowProps);

    ui->sourceVars->setWindowTitle(tr("High-level Variables"));
    ui->docking->addToolWindow(
        ui->sourceVars, ToolWindowManager::AreaReference(ToolWindowManager::AddTo,
                                                         ui->docking->areaOf(ui->debugVars)));
    ui->docking->setToolWindowProperties(ui->sourceVars, windowProps);

    m_Line2Insts.resize(m_ShaderDetails->debugInfo.files.count());

    bool hasLineInfo = false;

    for(size_t inst = 0; inst < m_Trace->lineInfo.size(); inst++)
    {
      const LineColumnInfo &line = m_Trace->lineInfo[inst];

      int disasmLine = (int)line.disassemblyLine;
      if(disasmLine > 0 && disasmLine >= m_AsmLine2Inst.size())
      {
        int oldSize = m_AsmLine2Inst.size();
        m_AsmLine2Inst.resize(disasmLine + 1);
        for(int i = oldSize; i < disasmLine; i++)
          m_AsmLine2Inst[i] = -1;
      }

      if(disasmLine > 0)
        m_AsmLine2Inst[disasmLine] = (int)inst;

      if(line.fileIndex < 0 || line.fileIndex >= m_Line2Insts.count())
        continue;

      hasLineInfo = true;

      for(uint32_t lineNum = line.lineStart; lineNum <= line.lineEnd; lineNum++)
        m_Line2Insts[line.fileIndex][lineNum].push_back(inst);
    }

    // if we don't have line mapping info, assume we also don't have useful high-level variable
    // info. Show the debug variables first rather than a potentially empty source variables panel.
    if(!hasLineInfo)
      ui->docking->raiseToolWindow(ui->debugVars);

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
    ui->accessedResources->installEventFilter(this);
    ui->debugVars->installEventFilter(this);
    ui->watch->installEventFilter(this);

    cacheResources();

    m_BackgroundRunning.release();

    QPointer<ShaderViewer> me(this);

    m_Ctx.Replay().AsyncInvoke([this, me](IReplayController *r) {
      if(!me)
        return;

      rdcarray<ShaderDebugState> states = r->ContinueDebug(m_Trace->debugger);

      bool finished = false;
      do
      {
        if(!me)
          return;

        rdcarray<ShaderDebugState> nextStates = r->ContinueDebug(m_Trace->debugger);

        if(!me)
          return;

        states.append(nextStates);
        finished = nextStates.empty();
      } while(!finished && m_BackgroundRunning.available() == 1);

      if(!me || m_BackgroundRunning.available() != 1)
        return;

      m_BackgroundRunning.tryAcquire(1);

      r->SetFrameEvent(m_Ctx.CurEvent(), true);

      if(!me)
        return;

      GUIInvoke::call(this, [this, states]() {
        m_States = states;

        if(!m_States.empty())
        {
          for(const ShaderVariableChange &c : GetCurrentState().changes)
            m_Variables.push_back(c.after);
        }

        bool preferSourceDebug = false;

        for(const ShaderCompileFlag &flag : m_ShaderDetails->debugInfo.compileFlags.flags)
        {
          if(flag.name == "preferSourceDebug")
          {
            preferSourceDebug = true;
            break;
          }
        }

        updateDebugState();

        // we do updateDebugging() again because the first call finds the scintilla for the current
        // source file, the second time jumps to it.
        if(preferSourceDebug)
        {
          gotoSourceDebugging();
          updateDebugState();
        }
      });
    });

    GUIInvoke::defer(this, [this, debugContext]() {
      // wait a short while before displaying the progress dialog (which won't show if we're already
      // done by the time we reach it)
      for(int i = 0; m_BackgroundRunning.available() == 1 && i < 100; i++)
        QThread::msleep(5);

      ShowProgressDialog(this, tr("Debugging %1").arg(debugContext),
                         [this]() { return m_BackgroundRunning.available() == 0; }, NULL,
                         [this]() { m_BackgroundRunning.acquire(); });

    });

    m_CurrentStateIdx = 0;

    QObject::connect(ui->watch, &RDTableWidget::keyPress, this, &ShaderViewer::watch_keyPress);

    ui->watch->setContextMenuPolicy(Qt::CustomContextMenu);
    QObject::connect(ui->watch, &RDTableWidget::customContextMenuRequested, this,
                     &ShaderViewer::variables_contextMenu);
    ui->debugVars->setContextMenuPolicy(Qt::CustomContextMenu);
    QObject::connect(ui->debugVars, &RDTreeWidget::customContextMenuRequested, this,
                     &ShaderViewer::variables_contextMenu);
    ui->sourceVars->setContextMenuPolicy(Qt::CustomContextMenu);
    QObject::connect(ui->sourceVars, &RDTreeWidget::customContextMenuRequested, this,
                     &ShaderViewer::variables_contextMenu);
    ui->accessedResources->setContextMenuPolicy(Qt::CustomContextMenu);
    QObject::connect(ui->accessedResources, &RDTreeWidget::customContextMenuRequested, this,
                     &ShaderViewer::accessedResources_contextMenu);

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
    ui->debugVars->hide();
    ui->constants->hide();
    ui->resourcesPanel->hide();
    ui->sourceVars->hide();
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
  updateDebugState();
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

  SetTextAndUpdateMargin0(ret, text);

  ret->setMarginLeft(4.0 * devicePixelRatioF());
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

void ShaderViewer::SetTextAndUpdateMargin0(ScintillaEdit *sc, const QString &text)
{
  sc->setText(text.toUtf8().data());

  sptr_t numlines = sc->lineCount();

  int margin0width = 30;
  if(numlines > 1000)
    margin0width += 6;
  if(numlines > 10000)
    margin0width += 6;

  margin0width = int(margin0width * devicePixelRatioF());

  sc->setMarginWidthN(0, margin0width);
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

    updateDebugState();
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
      if(tree == ui->sourceVars)
        AddWatch(tree->selectedItem()->tag().value<VariableTag>().debugVar.name);
      else
        AddWatch(tree->selectedItem()->text(0));
    });
  }

  RDDialog::show(&contextMenu, w->viewport()->mapToGlobal(pos));
}

void ShaderViewer::accessedResources_contextMenu(const QPoint &pos)
{
  QAbstractItemView *w = qobject_cast<QAbstractItemView *>(QObject::sender());
  RDTreeWidget *tree = qobject_cast<RDTreeWidget *>(w);
  if(tree->selectedItem() == NULL)
    return;

  const AccessedResourceTag &tag = tree->selectedItem()->tag().value<AccessedResourceTag>();
  if(tag.type == VarType::Unknown)
  {
    // Right clicked on an instruction row
    QMenu contextMenu(this);

    QAction gotoInstr(tr("Go to Step"), this);

    contextMenu.addAction(&gotoInstr);

    QObject::connect(&gotoInstr, &QAction::triggered, [this, tag] {
      bool forward = (tag.step >= m_CurrentStateIdx);
      runTo({tag.step}, forward);
    });

    RDDialog::show(&contextMenu, w->viewport()->mapToGlobal(pos));
  }
  else
  {
    // Right clicked on a resource row
    QMenu contextMenu(this);

    QAction prevAccess(tr("Run to Previous Access"), this);
    QAction nextAccess(tr("Run to Next Access"), this);

    contextMenu.addAction(&prevAccess);
    contextMenu.addAction(&nextAccess);

    QObject::connect(&prevAccess, &QAction::triggered,
                     [this, tag] { runToResourceAccess(false, tag.type, tag.bind); });
    QObject::connect(&nextAccess, &QAction::triggered,
                     [this, tag] { runToResourceAccess(true, tag.type, tag.bind); });

    RDDialog::show(&contextMenu, w->viewport()->mapToGlobal(pos));
  }
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

    // if we match a swizzle look before that for the variable
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
      if(findVar(text))
      {
        start = 0;
        end = m_DisassemblyView->length();

        QColor highlightColor = QColor::fromHslF(
            0.333f, 1.0f, qBound(0.25, palette().color(QPalette::Base).lightnessF(), 0.85));

        highlightMatchingVars(ui->debugVars->invisibleRootItem(), text, highlightColor);
        highlightMatchingVars(ui->constants->invisibleRootItem(), text, highlightColor);
        highlightMatchingVars(ui->accessedResources->invisibleRootItem(), text, highlightColor);
        highlightMatchingVars(ui->sourceVars->invisibleRootItem(), text, highlightColor);

        m_DisassemblyView->setIndicatorCurrent(INDICATOR_REGHIGHLIGHT);
        m_DisassemblyView->indicatorClearRange(start, end);

        sptr_t flags = SCFIND_MATCHCASE | SCFIND_WHOLEWORD | SCFIND_REGEXP | SCFIND_POSIX;
        text += lit("\\.[xyzwrgba]+");

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
      SetTextAndUpdateMargin0(m_DisassemblyView, text);
      m_DisassemblyView->setReadOnly(true);
      m_DisassemblyView->emptyUndoBuffer();
      return;
    }
  }

  if(targetStr == tr("More disassembly formats..."))
  {
    QString text;

    text =
        tr("; More disassembly formats are available with a pipeline. This shader view is not\n"
           "; associated with any specific pipeline and shows only the shader itself.\n\n"
           "; Viewing the shader from the pipeline state view with a pipeline bound will expose\n"
           "; these other formats:\n\n");

    for(const rdcstr &t : m_PipelineTargets)
      text += QFormatStr("%1\n").arg(QString(t));

    m_DisassemblyView->setReadOnly(false);
    SetTextAndUpdateMargin0(m_DisassemblyView, text);
    m_DisassemblyView->setReadOnly(true);
    m_DisassemblyView->emptyUndoBuffer();
    return;
  }

  QPointer<ShaderViewer> me(this);

  m_Ctx.Replay().AsyncInvoke([me, this, target](IReplayController *r) {
    if(!me)
      return;

    rdcstr disasm = r->DisassembleShader(m_Pipeline, m_ShaderDetails, target.data());

    if(!me)
      return;

    GUIInvoke::call(this, [this, disasm]() {
      m_DisassemblyView->setReadOnly(false);
      SetTextAndUpdateMargin0(m_DisassemblyView, disasm);
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

  updateDebugState();
}

bool ShaderViewer::stepBack()
{
  if(!m_Trace || m_States.empty())
    return false;

  if(IsFirstState())
    return false;

  if(isSourceDebugging())
  {
    LineColumnInfo oldLine = m_Trace->lineInfo[GetCurrentState().nextInstruction];

    // first step to the next instruction in a backwards direction that's on a different line from
    // the current one
    do
    {
      applyBackwardsChange();

      if(m_Breakpoints.contains((int)GetCurrentState().nextInstruction))
        break;

      if(IsFirstState())
        break;

      if(m_Trace->lineInfo[GetCurrentState().nextInstruction].SourceEqual(oldLine))
        continue;

      break;
    } while(true);

    oldLine = m_Trace->lineInfo[GetCurrentState().nextInstruction];

    // now since a line can have multiple instructions, keep stepping (looking forward) until we
    // reach the first instruction with an identical line info
    while(!IsFirstState() &&
          m_Trace->lineInfo[GetPreviousState().nextInstruction].SourceEqual(oldLine))
    {
      applyBackwardsChange();

      if(m_Breakpoints.contains((int)GetCurrentState().nextInstruction))
        break;
    }

    updateDebugState();
  }
  else
  {
    applyBackwardsChange();
    updateDebugState();
  }

  return true;
}

bool ShaderViewer::stepNext()
{
  if(!m_Trace || m_States.empty())
    return false;

  if(IsLastState())
    return false;

  if(isSourceDebugging())
  {
    LineColumnInfo oldLine = m_Trace->lineInfo[GetCurrentState().nextInstruction];

    do
    {
      applyForwardsChange();

      if(m_Breakpoints.contains((int)GetCurrentState().nextInstruction))
        break;

      if(IsLastState())
        break;

      if(m_Trace->lineInfo[GetCurrentState().nextInstruction].SourceEqual(oldLine))
        continue;

      break;
    } while(true);

    updateDebugState();
  }
  else
  {
    applyForwardsChange();
    updateDebugState();
  }

  return true;
}

void ShaderViewer::runToCursor()
{
  if(!m_Trace || m_States.empty())
    return;

  ScintillaEdit *cur = currentScintilla();

  if(cur != m_DisassemblyView)
  {
    int scintillaIndex = m_FileScintillas.indexOf(cur);

    if(scintillaIndex < 0)
      return;

    sptr_t i = cur->lineFromPosition(cur->currentPos()) + 1;

    QMap<int32_t, QVector<size_t>> &fileMap = m_Line2Insts[scintillaIndex];

    // find the next line that maps to an instruction
    for(; i < cur->lineCount(); i++)
    {
      if(fileMap.contains(i))
      {
        runTo(fileMap[i], true);
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
      int line = instructionForDisassemblyLine(i);
      if(line >= 0)
      {
        runTo({(size_t)line}, true);
        break;
      }
    }
  }
}

int ShaderViewer::instructionForDisassemblyLine(sptr_t line)
{
  // go from scintilla's lines (0-based) to ours (1-based)
  line++;

  if(line < m_AsmLine2Inst.size())
    return m_AsmLine2Inst[line];

  return -1;
}

bool ShaderViewer::IsFirstState() const
{
  return m_CurrentStateIdx == 0;
}

bool ShaderViewer::IsLastState() const
{
  return m_CurrentStateIdx == m_States.size() - 1;
}

const ShaderDebugState &ShaderViewer::GetPreviousState() const
{
  if(m_CurrentStateIdx > 0)
    return m_States[m_CurrentStateIdx - 1];

  return m_States.front();
}

const ShaderDebugState &ShaderViewer::GetCurrentState() const
{
  if(m_CurrentStateIdx < m_States.size())
    return m_States[m_CurrentStateIdx];

  return m_States.back();
}

const ShaderDebugState &ShaderViewer::GetNextState() const
{
  if(m_CurrentStateIdx + 1 < m_States.size())
    return m_States[m_CurrentStateIdx + 1];

  return m_States.back();
}

void ShaderViewer::runToSample()
{
  runTo({}, true, ShaderEvents::SampleLoadGather);
}

void ShaderViewer::runToNanOrInf()
{
  runTo({}, true, ShaderEvents::GeneratedNanOrInf);
}

void ShaderViewer::runBack()
{
  runTo({}, false);
}

void ShaderViewer::run()
{
  runTo({}, true);
}

void ShaderViewer::runTo(QVector<size_t> runToInstruction, bool forward, ShaderEvents condition)
{
  if(!m_Trace || m_States.empty())
    return;

  bool firstStep = true;

  // this is effectively infinite as we break out before moving to next/previous state if that would
  // be first/last
  while((forward && !IsLastState()) || (!forward && !IsFirstState()))
  {
    // break immediately even on the very first step if it's the one we want to go to
    if(runToInstruction.contains(GetCurrentState().nextInstruction))
      break;

    // after the first step, break on condition
    if(!firstStep && (GetCurrentState().flags & condition))
      break;

    // or breakpoint
    if(!firstStep && m_Breakpoints.contains((int)GetCurrentState().nextInstruction))
      break;

    firstStep = false;

    if(forward)
    {
      if(IsLastState())
        break;
      applyForwardsChange();
    }
    else
    {
      if(IsFirstState())
        break;
      applyBackwardsChange();
    }
  }

  updateDebugState();
}

void ShaderViewer::runToResourceAccess(bool forward, VarType type, const BindpointIndex &resource)
{
  if(!m_Trace || m_States.empty())
    return;

  // this is effectively infinite as we break out before moving to next/previous state if that would
  // be first/last
  while((forward && !IsLastState()) || (!forward && !IsFirstState()))
  {
    if(forward)
    {
      if(IsLastState())
        break;
      applyForwardsChange();
    }
    else
    {
      if(IsFirstState())
        break;
      applyBackwardsChange();
    }

    // Break if the current state references the specific resource requested
    bool foundResource = false;
    for(const ShaderVariableChange &c : GetCurrentState().changes)
    {
      if(c.after.type == type && c.after.GetBinding() == resource)
      {
        foundResource = true;
        break;
      }
    }

    if(foundResource)
      break;

    // or breakpoint
    if(m_Breakpoints.contains((int)GetCurrentState().nextInstruction))
      break;
  }

  updateDebugState();
}

void ShaderViewer::applyBackwardsChange()
{
  if(!IsFirstState())
  {
    rdcarray<ShaderVariable> newVariables;

    for(const ShaderVariableChange &c : GetCurrentState().changes)
    {
      // if the before name is empty, this is a variable that came into scope/was created
      if(c.before.name.empty())
      {
        // delete the matching variable (should only be one)
        for(size_t i = 0; i < m_Variables.size(); i++)
        {
          if(c.after.name == m_Variables[i].name)
          {
            m_Variables.erase(i);
            break;
          }
        }
      }
      else
      {
        ShaderVariable *v = NULL;
        for(size_t i = 0; i < m_Variables.size(); i++)
        {
          if(c.before.name == m_Variables[i].name)
          {
            v = &m_Variables[i];
            break;
          }
        }

        if(v)
          *v = c.before;
        else
          newVariables.push_back(c.before);
      }
    }

    m_Variables.insert(0, newVariables);

    m_CurrentStateIdx--;
  }
}

void ShaderViewer::applyForwardsChange()
{
  if(!IsLastState())
  {
    m_CurrentStateIdx++;

    rdcarray<ShaderVariable> newVariables;
    rdcarray<AccessedResourceData> newAccessedResources;

    for(const ShaderVariableChange &c : GetCurrentState().changes)
    {
      // if the after name is empty, this is a variable going out of scope/being deleted
      if(c.after.name.empty())
      {
        // delete the matching variable (should only be one)
        for(size_t i = 0; i < m_Variables.size(); i++)
        {
          if(c.before.name == m_Variables[i].name)
          {
            m_Variables.erase(i);
            break;
          }
        }
      }
      else
      {
        ShaderVariable *v = NULL;
        for(size_t i = 0; i < m_Variables.size(); i++)
        {
          if(c.after.name == m_Variables[i].name)
          {
            v = &m_Variables[i];
            break;
          }
        }

        if(v)
          *v = c.after;
        else
          newVariables.push_back(c.after);

        if(c.after.type == VarType::ReadOnlyResource || c.after.type == VarType::ReadWriteResource)
        {
          bool found = false;
          for(size_t i = 0; i < m_AccessedResources.size(); i++)
          {
            if(c.after.GetBinding() == m_AccessedResources[i].resource.GetBinding())
            {
              found = true;
              if(m_AccessedResources[i].steps.indexOf(m_CurrentStateIdx) < 0)
                m_AccessedResources[i].steps.push_back(m_CurrentStateIdx);
              break;
            }
          }

          if(!found)
            newAccessedResources.push_back({c.after, {m_CurrentStateIdx}});
        }
      }
    }

    m_Variables.insert(0, newVariables);
    m_AccessedResources.insert(0, newAccessedResources);
  }
}

QString ShaderViewer::stringRep(const ShaderVariable &var, uint32_t row)
{
  VarType type = var.type;

  if(type == VarType::Unknown)
    type = ui->intView->isChecked() ? VarType::SInt : VarType::Float;

  if(type == VarType::ReadOnlyResource || type == VarType::ReadWriteResource ||
     type == VarType::Sampler)
  {
    BindpointIndex varBind = var.GetBinding();

    rdcarray<BoundResourceArray> resList;

    if(type == VarType::ReadOnlyResource)
      resList = m_ReadOnlyResources;
    else if(type == VarType::ReadWriteResource)
      resList = m_ReadWriteResources;
    else if(type == VarType::Sampler)
      resList = m_Ctx.CurPipelineState().GetSamplers(m_Stage);

    int32_t bindIdx = resList.indexOf(Bindpoint(varBind));

    if(bindIdx < 0)
      return QString();

    BoundResourceArray res = resList[bindIdx];

    if(varBind.arrayIndex >= res.resources.size())
      return QString();

    if(type == VarType::Sampler)
      return samplerRep(varBind, varBind.arrayIndex, res.resources[varBind.arrayIndex].resourceId);
    return ToQStr(res.resources[varBind.arrayIndex].resourceId);
  }

  return RowString(var, row, type);
}

QString ShaderViewer::samplerRep(Bindpoint bind, uint32_t arrayIndex, ResourceId id)
{
  if(id == ResourceId())
  {
    QString contents;
    if(bind.bindset > 0)
    {
      // a bit ugly to do an API-specific switch here but we don't have a better way to refer
      // by binding
      contents = IsD3D(m_Ctx.APIProps().pipelineType) ? tr("space%1, ") : tr("Set %1, ");
      contents = contents.arg(bind.bindset);
    }

    if(arrayIndex == ~0U)
      contents += QString::number(bind.bind);
    else
      contents += QFormatStr("%1[%2]").arg(bind.bind).arg(arrayIndex);

    return contents;
  }
  else
  {
    return ToQStr(id);
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

void ShaderViewer::combineStructures(RDTreeWidgetItem *root, int skipPrefixLength)
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

    int dotIndex = name.indexOf(QLatin1Char('.'), skipPrefixLength);
    int arrIndex = name.indexOf(QLatin1Char('['), skipPrefixLength);

    // if this node doesn't have any segments, just move it across.
    if(dotIndex < 0 && arrIndex < 0)
    {
      temp.insertChild(0, child);
      continue;
    }

    // store the index of the first separator
    int sepIndex = dotIndex;
    bool isLeafArray = (sepIndex == -1);
    if(sepIndex == -1 || (arrIndex > 0 && arrIndex < sepIndex))
      sepIndex = arrIndex;

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

    // Sort the children by offset, then global source var index, then by text.
    // Using the global source var index allows resource arrays to be presented in index order
    // rather than by name, so for example arr[2] comes before arr[10]
    std::sort(matches.begin(), matches.end(),
              [](const RDTreeWidgetItem *a, const RDTreeWidgetItem *b) {
                VariableTag at = a->tag().value<VariableTag>();
                VariableTag bt = b->tag().value<VariableTag>();
                if(at.offset != bt.offset)
                  return at.offset < bt.offset;
                if(at.globalSourceVar != bt.globalSourceVar)
                  return at.globalSourceVar < bt.globalSourceVar;
                return a->text(0) < b->text(0);
              });

    // create a new parent with just the prefix
    prefix.chop(1);
    QVariantList values = {prefix};
    for(int i = 1; i < child->dataCount(); i++)
      values.push_back(QVariant());
    RDTreeWidgetItem *parent = new RDTreeWidgetItem(values);

    // add all the children (stripping the prefix from their name)
    for(RDTreeWidgetItem *item : matches)
    {
      if(sepIndex == dotIndex)
        item->setText(0, item->text(0).mid(sepIndex + 1));
      parent->addChild(item);

      if(item->background().color().isValid())
        parent->setBackground(item->background());
      if(item->foreground().color().isValid())
        parent->setForeground(item->foreground());
    }

    // recurse and combine members of this object if a struct
    if(!isLeafArray)
    {
      if(sepIndex != dotIndex)
        combineStructures(parent, sepIndex + 1);
      else
        combineStructures(parent);
    }

    // now add to the list
    temp.insertChild(0, parent);
  }

  if(root->childCount() > 0)
    qCritical() << "Some objects left on root!";

  // move all the children back from the temp object into the parameter
  while(temp.childCount() > 0)
    root->addChild(temp.takeChild(0));
}

RDTreeWidgetItem *ShaderViewer::findVarInTree(RDTreeWidgetItem *root, QString name, bool fullmatch,
                                              int maxDepth)
{
  if(fullmatch)
  {
    if(root->tag().value<VariableTag>().debugVar.name == rdcstr(name))
      return root;
  }
  else
  {
    if(root->dataCount() > 0 && root->text(0) == name)
      return root;

    for(int i = 0; i < root->childCount(); i++)
    {
      RDTreeWidgetItem *child = root->child(i);
      if(child->dataCount() > 0 && child->text(0) == name)
        return child;
    }

    maxDepth--;
    if(maxDepth <= 0)
      return NULL;
  }

  for(int i = 0; i < root->childCount(); i++)
  {
    RDTreeWidgetItem *ret = findVarInTree(root->child(i), name, fullmatch, maxDepth);
    if(ret)
      return ret;
  }

  return NULL;
}

bool ShaderViewer::findVar(QString name, ShaderVariable *var)
{
  if(!m_Trace || m_States.empty())
    return false;

  // try source mapped variables first, as if we have ambiguity (a source variable the same as a
  // debug variable) we'll pick the source variable as 'more desirable'
  RDTreeWidgetItem *item = findVarInTree(ui->sourceVars->invisibleRootItem(), name, true, -1);

  // next try constants, which also contains some source mapping
  if(!item)
    item = findVarInTree(ui->constants->invisibleRootItem(), name, true, -1);

  // finally try debug variables
  if(!item)
    item = findVarInTree(ui->debugVars->invisibleRootItem(), name, true, -1);

  // if we didn't  find anything, try a non-full match. This will search just on the member name
  // and might pick up some false positives. We search top-level items (which is equivalent to the
  // match above so redundant) and their children only, not any further. The idea is to catch
  // anything under implicit global scopes which don't match the source, e.g. constant buffer names
  // or struct names which are implicit.
  if(!item)
    item = findVarInTree(ui->sourceVars->invisibleRootItem(), name, false, 2);
  if(!item)
    item = findVarInTree(ui->constants->invisibleRootItem(), name, false, 2);
  if(!item)
    item = findVarInTree(ui->debugVars->invisibleRootItem(), name, false, 2);

  if(!item)
    return false;

  return getVar(item, var, NULL);
}

bool ShaderViewer::getVar(RDTreeWidgetItem *item, ShaderVariable *var, QString *regNames)
{
  VariableTag tag = item->tag().value<VariableTag>();

  // if the tag is invalid, it's not a proper match
  if(tag.globalSourceVar < 0 && tag.localSourceVar < 0 &&
     tag.debugVar.type == DebugVariableType::Undefined)
    return false;

  // don't find resource variables
  if(tag.debugVar.type == DebugVariableType::Sampler ||
     tag.debugVar.type == DebugVariableType::ReadOnlyResource ||
     tag.debugVar.type == DebugVariableType::ReadWriteResource)
    return false;

  // if we have a debug var tag then it's easy-mode
  if(tag.debugVar.type != DebugVariableType::Undefined)
  {
    // found a match. If we don't want the variable contents, just return true now
    if(!var)
      return true;

    const ShaderVariable *reg = GetDebugVariable(tag.debugVar);

    if(reg)
    {
      *var = *reg;
      var->name = tag.debugVar.name;

      if(regNames)
        *regNames = reg->name;
    }
    else
    {
      qCritical() << "Couldn't find expected debug variable!" << ToQStr(tag.debugVar.type)
                  << QString(tag.debugVar.name) << tag.debugVar.component;
      return false;
    }

    return true;
  }
  else
  {
    SourceVariableMapping mapping;

    if(tag.globalSourceVar >= 0 && tag.globalSourceVar < m_Trace->sourceVars.count())
      mapping = m_Trace->sourceVars[tag.globalSourceVar];
    else if(tag.localSourceVar >= 0 && tag.localSourceVar < GetCurrentState().sourceVars.count())
      mapping = GetCurrentState().sourceVars[tag.localSourceVar];
    else
      qCritical() << "Couldn't find expected source variable!" << tag.globalSourceVar
                  << tag.localSourceVar;

    if(mapping.variables.empty())
      return false;

    {
      // don't find resource variables
      if(mapping.variables[0].type == DebugVariableType::Sampler ||
         mapping.variables[0].type == DebugVariableType::ReadOnlyResource ||
         mapping.variables[0].type == DebugVariableType::ReadWriteResource)
        return false;

      // found a match. If we don't want the variable contents, just return true now
      if(!var)
        return true;

      ShaderVariable &ret = *var;
      ret.name = tag.debugVar.name;
      ret.rowMajor = true;
      ret.rows = mapping.rows;
      ret.columns = mapping.columns;
      ret.type = mapping.type;

      const QString xyzw = lit("xyzw");

      for(uint32_t i = 0; i < mapping.variables.size(); i++)
      {
        const DebugVariableReference &r = mapping.variables[i];

        const ShaderVariable *reg = GetDebugVariable(r);

        if(regNames && !regNames->isEmpty())
          *regNames += lit(", ");

        if(reg)
        {
          if(regNames)
          {
            // if the previous register was the same, just append our component
            if(i > 0 && r.type == mapping.variables[i - 1].type &&
               r.name == mapping.variables[i - 1].name &&
               (r.component / reg->columns) == (mapping.variables[i - 1].component / reg->columns))
            {
              // remove the auto-appended ", " - there must be one because this isn't the first
              // register
              regNames->chop(2);
              *regNames += xyzw[r.component % 4];
            }
            else
            {
              if(reg->rows > 1)
                *regNames += QFormatStr("%1.row%2.%3")
                                 .arg(reg->name)
                                 .arg(r.component / 4)
                                 .arg(xyzw[r.component % 4]);
              else
                *regNames += QFormatStr("%1.%2").arg(reg->name).arg(xyzw[r.component % 4]);
            }
          }

          if(mapping.type == VarType::Double || mapping.type == VarType::ULong)
            ret.value.u64v[i] = reg->value.u64v[r.component];
          else
            ret.value.uv[i] = reg->value.uv[r.component];
        }
        else
        {
          if(regNames)
            *regNames += lit("-");
        }
      }

      return true;
    }

    return false;
  }
}

void ShaderViewer::highlightMatchingVars(RDTreeWidgetItem *root, const QString varName,
                                         const QColor highlightColor)
{
  for(int i = 0; i < root->childCount(); i++)
  {
    RDTreeWidgetItem *item = root->child(i);
    if(item->tag().value<VariableTag>().debugVar.name == rdcstr(varName))
      item->setBackgroundColor(highlightColor);
    else
      item->setBackground(QBrush());

    highlightMatchingVars(item, varName, highlightColor);
  }
}

void ShaderViewer::updateAccessedResources()
{
  RDTreeViewExpansionState expansion;
  ui->accessedResources->saveExpansion(expansion, 0);

  ui->accessedResources->beginUpdate();

  ui->accessedResources->clear();

  switch(m_AccessedResourceView)
  {
    case AccessedResourceView::SortByResource:
    {
      for(size_t i = 0; i < m_AccessedResources.size(); ++i)
      {
        // Check if the resource was accessed prior to this step
        bool accessed = false;
        for(size_t j = 0; j < m_AccessedResources[i].steps.size(); ++j)
        {
          if(m_AccessedResources[i].steps[j] <= m_CurrentStateIdx)
          {
            accessed = true;
            break;
          }
        }
        if(!accessed)
          continue;

        bool modified = false;
        for(const ShaderVariableChange &c : GetCurrentState().changes)
        {
          if(c.before.name == m_AccessedResources[i].resource.name ||
             c.after.name == m_AccessedResources[i].resource.name)
          {
            modified = true;
            break;
          }
        }

        RDTreeWidgetItem *resourceNode =
            makeAccessedResourceNode(m_AccessedResources[i].resource, modified);
        if(resourceNode)
        {
          // Add a child for each step that it was accessed
          for(size_t j = 0; j < m_AccessedResources[i].steps.size(); ++j)
          {
            accessed = m_AccessedResources[i].steps[j] <= m_CurrentStateIdx;
            if(accessed)
            {
              RDTreeWidgetItem *stepNode = new RDTreeWidgetItem(
                  {tr("Step %1").arg(m_AccessedResources[i].steps[j]), lit("Access"), lit("")});
              stepNode->setTag(QVariant::fromValue(
                  AccessedResourceTag((uint32_t)m_AccessedResources[i].steps[j])));
              if(m_CurrentStateIdx == m_AccessedResources[i].steps[j])
                stepNode->setForegroundColor(QColor(Qt::red));
              resourceNode->addChild(stepNode);
            }
          }

          ui->accessedResources->addTopLevelItem(resourceNode);
        }
      }

      break;
    }
    case AccessedResourceView::SortByStep:
    {
      rdcarray<rdcpair<size_t, RDTreeWidgetItem *>> stepNodes;
      for(size_t i = 0; i < m_AccessedResources.size(); ++i)
      {
        bool modified = false;
        for(const ShaderVariableChange &c : GetCurrentState().changes)
        {
          if(c.before.name == m_AccessedResources[i].resource.name ||
             c.after.name == m_AccessedResources[i].resource.name)
          {
            modified = true;
            break;
          }
        }

        // Add a root node for each instruction, and place the resource node as a child
        for(size_t j = 0; j < m_AccessedResources[i].steps.size(); ++j)
        {
          bool accessed = m_AccessedResources[i].steps[j] <= m_CurrentStateIdx;
          if(accessed)
          {
            int32_t nodeIdx = -1;
            for(int32_t k = 0; k < stepNodes.count(); ++k)
            {
              if(stepNodes[k].first == m_AccessedResources[i].steps[j])
              {
                nodeIdx = k;
                break;
              }
            }

            RDTreeWidgetItem *resourceNode =
                makeAccessedResourceNode(m_AccessedResources[i].resource, modified);

            if(nodeIdx == -1)
            {
              RDTreeWidgetItem *stepNode = new RDTreeWidgetItem(
                  {tr("Step %1").arg(m_AccessedResources[i].steps[j]), lit("Access"), lit("")});
              stepNode->setTag(QVariant::fromValue(
                  AccessedResourceTag((uint32_t)m_AccessedResources[i].steps[j])));
              if(m_CurrentStateIdx == m_AccessedResources[i].steps[j])
                stepNode->setForegroundColor(QColor(Qt::red));
              stepNode->addChild(resourceNode);
              stepNodes.push_back({m_AccessedResources[i].steps[j], stepNode});
            }
            else
            {
              stepNodes[nodeIdx].second->addChild(resourceNode);
            }
          }
        }
      }

      std::sort(stepNodes.begin(), stepNodes.end(),
                [](const rdcpair<size_t, RDTreeWidgetItem *> &a,
                   const rdcpair<size_t, RDTreeWidgetItem *> &b) { return a.first < b.first; });

      for(size_t i = 0; i < stepNodes.size(); ++i)
        ui->accessedResources->addTopLevelItem(stepNodes[i].second);

      break;
    }
  }

  ui->accessedResources->endUpdate();

  ui->accessedResources->applyExpansion(expansion, 0);
}

void ShaderViewer::updateDebugState()
{
  if(!m_Trace || m_States.empty())
    return;

  if(ui->debugToggle->isEnabled())
  {
    if(isSourceDebugging())
      ui->debugToggle->setText(tr("Debug in Assembly"));
    else
      ui->debugToggle->setText(tr("Debug in Source"));
  }

  const ShaderDebugState &state = GetCurrentState();

  const bool done = IsLastState();

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

  ui->callstack->clear();

  for(const rdcstr &s : state.callstack)
    ui->callstack->insertItem(0, s);

  if(state.nextInstruction < m_Trace->lineInfo.size())
  {
    LineColumnInfo &lineInfo = m_Trace->lineInfo[state.nextInstruction];

    // highlight the current line
    {
      m_DisassemblyView->markerAdd(lineInfo.disassemblyLine - 1,
                                   done ? FINISHED_MARKER : CURRENT_MARKER);
      m_DisassemblyView->markerAdd(lineInfo.disassemblyLine - 1,
                                   done ? FINISHED_MARKER + 1 : CURRENT_MARKER + 1);

      int pos = m_DisassemblyView->positionFromLine(lineInfo.disassemblyLine - 1);
      m_DisassemblyView->setSelection(pos, pos);

      ensureLineScrolled(m_DisassemblyView, lineInfo.disassemblyLine - 1);
    }

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
    // track all debug variables that have been mapped by source vars
    QSet<QString> varsMapped;

    RDTreeWidgetItem fakeroot;

    for(int globalVarIdx = 0; globalVarIdx < m_Trace->sourceVars.count(); globalVarIdx++)
    {
      const SourceVariableMapping &sourceVar = m_Trace->sourceVars[globalVarIdx];

      if(!sourceVar.variables.empty() && sourceVar.variables[0].type == DebugVariableType::Variable)
        continue;

      for(const DebugVariableReference &r : sourceVar.variables)
        varsMapped.insert(r.name);

      if(sourceVar.rows == 0 || sourceVar.columns == 0)
        continue;

      fakeroot.addChild(makeSourceVariableNode(sourceVar, globalVarIdx, -1, false));
    }

    // recursively combine nodes with the same prefix together
    combineStructures(&fakeroot);

    while(fakeroot.childCount() > 0)
      ui->constants->addTopLevelItem(fakeroot.takeChild(0));

    // add any raw registers that weren't mapped with something better. We assume for inputs that
    // everything has a source mapping, even if it's faked from reflection info, but just to be sure
    // we add any remainders here. Constants might be un-touched by reflection info
    for(int i = 0; i < m_Trace->constantBlocks.count(); i++)
    {
      rdcstr name = m_Trace->constantBlocks[i].name;
      if(varsMapped.contains(name))
        continue;

      RDTreeWidgetItem *node = new RDTreeWidgetItem({name, name, lit("Constant"), QString()});
      node->setTag(QVariant::fromValue(
          VariableTag(DebugVariableReference(DebugVariableType::Constant, name))));

      for(int j = 0; j < m_Trace->constantBlocks[i].members.count(); j++)
      {
        if(m_Trace->constantBlocks[i].members[j].rows > 0 ||
           m_Trace->constantBlocks[i].members[j].columns > 0)
        {
          name = m_Trace->constantBlocks[i].members[j].name;
          if(!varsMapped.contains(name))
          {
            RDTreeWidgetItem *child = new RDTreeWidgetItem(
                {name, name, lit("Constant"), stringRep(m_Trace->constantBlocks[i].members[j])});
            child->setTag(QVariant::fromValue(
                VariableTag(DebugVariableReference(DebugVariableType::Constant, name))));
            node->addChild(child);
          }
        }
        else
        {
          // Check if this is a constant buffer array
          int arrayCount = m_Trace->constantBlocks[i].members[j].members.count();
          for(int k = 0; k < arrayCount; k++)
          {
            if(m_Trace->constantBlocks[i].members[j].members[k].rows > 0 ||
               m_Trace->constantBlocks[i].members[j].members[k].columns > 0)
            {
              name = m_Trace->constantBlocks[i].members[j].members[k].name;
              RDTreeWidgetItem *child =
                  new RDTreeWidgetItem({name, name, lit("Constant"),
                                        stringRep(m_Trace->constantBlocks[i].members[j].members[k])});
              node->setTag(QVariant::fromValue(
                  VariableTag(DebugVariableReference(DebugVariableType::Constant, name))));

              node->addChild(child);
            }
          }
        }
      }

      if(node->childCount() == 0)
      {
        delete node;
        continue;
      }

      ui->constants->addTopLevelItem(node);
    }

    for(int i = 0; i < m_Trace->inputs.count(); i++)
    {
      const ShaderVariable &input = m_Trace->inputs[i];

      if(varsMapped.contains(input.name))
        continue;

      if(input.rows > 0 || input.columns > 0)
      {
        RDTreeWidgetItem *node =
            new RDTreeWidgetItem({input.name, input.name, lit("Input"), stringRep(input)});
        node->setTag(QVariant::fromValue(
            VariableTag(DebugVariableReference(DebugVariableType::Input, input.name))));

        ui->constants->addTopLevelItem(node);
      }
    }

    rdcarray<BoundResourceArray> &roBinds = m_ReadOnlyResources;

    for(int i = 0; i < m_Trace->readOnlyResources.count(); i++)
    {
      const ShaderVariable &ro = m_Trace->readOnlyResources[i];

      if(varsMapped.contains(ro.name))
        continue;

      int32_t idx = m_Mapping.readOnlyResources.indexOf(Bindpoint(ro.GetBinding()));

      if(idx < 0)
        continue;

      Bindpoint bind = m_Mapping.readOnlyResources[idx];

      if(!bind.used)
        continue;

      int32_t bindIdx = roBinds.indexOf(bind);

      if(bindIdx < 0)
        continue;

      BoundResourceArray &roBind = roBinds[bindIdx];

      if(bind.arraySize == 1)
      {
        if(!roBind.resources.empty())
        {
          RDTreeWidgetItem *node =
              new RDTreeWidgetItem({m_ShaderDetails->readOnlyResources[i].name, ro.name,
                                    lit("Resource"), ToQStr(roBind.resources[0].resourceId)});
          node->setTag(QVariant::fromValue(
              VariableTag(DebugVariableReference(DebugVariableType::ReadOnlyResource, ro.name))));
          ui->constants->addTopLevelItem(node);
        }
      }
      else if(bind.arraySize == ~0U)
      {
        RDTreeWidgetItem *node = new RDTreeWidgetItem(
            {m_ShaderDetails->readOnlyResources[i].name, ro.name, lit("[unbounded]"), QString()});
        node->setTag(QVariant::fromValue(
            VariableTag(DebugVariableReference(DebugVariableType::ReadOnlyResource, ro.name))));
        ui->constants->addTopLevelItem(node);
      }
      else
      {
        RDTreeWidgetItem *node =
            new RDTreeWidgetItem({m_ShaderDetails->readOnlyResources[i].name, ro.name,
                                  QFormatStr("[%1]").arg(bind.arraySize), QString()});
        node->setTag(QVariant::fromValue(
            VariableTag(DebugVariableReference(DebugVariableType::ReadOnlyResource, ro.name))));

        uint32_t count = qMin(bind.arraySize, (uint32_t)roBind.resources.size());
        for(uint32_t a = 0; a < count; a++)
        {
          QString childName = QFormatStr("%1[%2]").arg(ro.name).arg(a);
          RDTreeWidgetItem *child = new RDTreeWidgetItem({
              QFormatStr("%1[%2]").arg(m_ShaderDetails->readOnlyResources[i].name).arg(a),
              childName, lit("Resource"), ToQStr(roBind.resources[a].resourceId),
          });
          child->setTag(QVariant::fromValue(
              VariableTag(DebugVariableReference(DebugVariableType::ReadOnlyResource, childName))));
          node->addChild(child);
        }

        ui->constants->addTopLevelItem(node);
      }
    }

    rdcarray<BoundResourceArray> &rwBinds = m_ReadWriteResources;

    for(int i = 0; i < m_Trace->readWriteResources.count(); i++)
    {
      const ShaderVariable &rw = m_Trace->readWriteResources[i];

      if(varsMapped.contains(rw.name))
        continue;

      int32_t idx = m_Mapping.readWriteResources.indexOf(Bindpoint(rw.GetBinding()));

      if(idx < 0)
        continue;

      Bindpoint bind = m_Mapping.readWriteResources[idx];

      if(!bind.used)
        continue;

      int32_t bindIdx = rwBinds.indexOf(bind);

      if(bindIdx < 0)
        continue;

      BoundResourceArray &rwBind = rwBinds[bindIdx];

      if(bind.arraySize == 1)
      {
        if(!rwBind.resources.empty())
        {
          RDTreeWidgetItem *node =
              new RDTreeWidgetItem({m_ShaderDetails->readWriteResources[i].name, rw.name,
                                    lit("Resource"), ToQStr(rwBind.resources[0].resourceId)});
          node->setTag(QVariant::fromValue(
              VariableTag(DebugVariableReference(DebugVariableType::ReadWriteResource, rw.name))));
          ui->constants->addTopLevelItem(node);
        }
      }
      else if(bind.arraySize == ~0U)
      {
        RDTreeWidgetItem *node = new RDTreeWidgetItem(
            {m_ShaderDetails->readWriteResources[i].name, rw.name, lit("[unbounded]"), QString()});
        node->setTag(QVariant::fromValue(
            VariableTag(DebugVariableReference(DebugVariableType::ReadWriteResource, rw.name))));
        ui->constants->addTopLevelItem(node);
      }
      else
      {
        RDTreeWidgetItem *node =
            new RDTreeWidgetItem({m_ShaderDetails->readWriteResources[i].name, rw.name,
                                  QFormatStr("[%1]").arg(bind.arraySize), QString()});
        node->setTag(QVariant::fromValue(
            VariableTag(DebugVariableReference(DebugVariableType::ReadWriteResource, rw.name))));

        uint32_t count = qMin(bind.arraySize, (uint32_t)rwBind.resources.size());
        for(uint32_t a = 0; a < count; a++)
        {
          QString childName = QFormatStr("%1[%2]").arg(rw.name).arg(a);
          RDTreeWidgetItem *child = new RDTreeWidgetItem({
              QFormatStr("%1[%2]").arg(m_ShaderDetails->readWriteResources[i].name).arg(a),
              childName, lit("RW Resource"), ToQStr(rwBind.resources[a].resourceId),
          });
          child->setTag(QVariant::fromValue(
              VariableTag(DebugVariableReference(DebugVariableType::ReadWriteResource, childName))));
          node->addChild(child);
        }

        ui->constants->addTopLevelItem(node);
      }
    }

    rdcarray<BoundResourceArray> samplers = m_Ctx.CurPipelineState().GetSamplers(m_Stage);

    for(int i = 0; i < m_Trace->samplers.count(); i++)
    {
      const ShaderVariable &s = m_Trace->samplers[i];

      if(varsMapped.contains(s.name))
        continue;

      int32_t idx = m_Mapping.samplers.indexOf(Bindpoint(s.GetBinding()));

      if(idx < 0)
        continue;

      Bindpoint bind = m_Mapping.samplers[idx];

      if(!bind.used)
        continue;

      int32_t bindIdx = samplers.indexOf(bind);

      if(bindIdx < 0)
        continue;

      BoundResourceArray sampBind = samplers[bindIdx];

      if(bind.arraySize == 1)
      {
        if(!sampBind.resources.empty())
        {
          RDTreeWidgetItem *node =
              new RDTreeWidgetItem({m_ShaderDetails->samplers[i].name, s.name, lit("Sampler"),
                                    samplerRep(bind, ~0U, sampBind.resources[0].resourceId)});
          node->setTag(QVariant::fromValue(
              VariableTag(DebugVariableReference(DebugVariableType::Sampler, s.name))));
          ui->constants->addTopLevelItem(node);
        }
      }
      else if(bind.arraySize == ~0U)
      {
        RDTreeWidgetItem *node = new RDTreeWidgetItem(
            {m_ShaderDetails->samplers[i].name, s.name, lit("[unbounded]"), QString()});
        node->setTag(QVariant::fromValue(
            VariableTag(DebugVariableReference(DebugVariableType::Sampler, s.name))));
        ui->constants->addTopLevelItem(node);
      }
      else
      {
        RDTreeWidgetItem *node =
            new RDTreeWidgetItem({m_ShaderDetails->samplers[i].name, s.name,
                                  QFormatStr("[%1]").arg(bind.arraySize), QString()});
        node->setTag(QVariant::fromValue(
            VariableTag(DebugVariableReference(DebugVariableType::Sampler, s.name))));

        for(uint32_t a = 0; a < bind.arraySize; a++)
        {
          QString childName = QFormatStr("%1[%2]").arg(s.name).arg(a);
          RDTreeWidgetItem *child = new RDTreeWidgetItem({
              QFormatStr("%1[%2]").arg(m_ShaderDetails->samplers[i].name).arg(a), childName,
              lit("Sampler"), samplerRep(bind, a, sampBind.resources[a].resourceId),
          });
          child->setTag(QVariant::fromValue(
              VariableTag(DebugVariableReference(DebugVariableType::Sampler, childName))));
          node->addChild(child);
        }

        ui->constants->addTopLevelItem(node);
      }
    }
  }

  {
    RDTreeViewExpansionState expansion;
    ui->sourceVars->saveExpansion(expansion, 0);

    ui->sourceVars->beginUpdate();

    ui->sourceVars->clear();

    RDTreeWidgetItem fakeroot;

    const rdcarray<SourceVariableMapping> &sourceVars = state.sourceVars;

    for(size_t lidx = 0; lidx < sourceVars.size(); lidx++)
    {
      int32_t localVarIdx = int32_t(sourceVars.size() - 1 - lidx);

      // iterate in reverse order, so newest locals tend to end up on top
      const SourceVariableMapping &l = sourceVars[localVarIdx];

      bool modified = false;

      // don't list any modified variables on the first step when they all come into existance
      if(l.variables[0].type == DebugVariableType::Variable && !IsFirstState())
      {
        for(const DebugVariableReference &v : l.variables)
        {
          rdcstr base = v.name;
          int offs = base.find_first_of("[.");
          if(offs > 0)
            base = v.name.substr(0, offs);

          for(const ShaderVariableChange &c : GetCurrentState().changes)
          {
            if(c.before.name == v.name || c.after.name == v.name || c.before.name == base ||
               c.after.name == base)
            {
              modified = true;
              break;
            }
          }

          if(modified)
            break;
        }
      }

      RDTreeWidgetItem *node = makeSourceVariableNode(l, -1, localVarIdx, modified);

      fakeroot.addChild(node);
    }

    // recursively combine nodes with the same prefix together
    combineStructures(&fakeroot);

    while(fakeroot.childCount() > 0)
      ui->sourceVars->addTopLevelItem(fakeroot.takeChild(0));

    ui->sourceVars->endUpdate();

    ui->sourceVars->applyExpansion(expansion, 0);
  }

  {
    RDTreeViewExpansionState expansion;
    ui->debugVars->saveExpansion(expansion, 0);

    ui->debugVars->beginUpdate();

    ui->debugVars->clear();

    for(int i = 0; i < m_Variables.count(); i++)
    {
      bool modified = false;

      for(const ShaderVariableChange &c : GetCurrentState().changes)
      {
        if(c.before.name == m_Variables[i].name || c.after.name == m_Variables[i].name)
        {
          modified = true;
          break;
        }
      }

      ui->debugVars->addTopLevelItem(makeDebugVariableNode(m_Variables[i], "", modified));
    }

    ui->debugVars->endUpdate();

    ui->debugVars->applyExpansion(expansion, 0);
  }

  updateAccessedResources();

  updateWatchVariables();

  ui->debugVars->resizeColumnToContents(0);
  ui->debugVars->resizeColumnToContents(1);

  updateVariableTooltip();
}

void ShaderViewer::updateWatchVariables()
{
  ui->watch->setUpdatesEnabled(false);

  for(int i = 0; i < ui->watch->rowCount() - 1; i++)
  {
    QTableWidgetItem *item = ui->watch->item(i, 0);

    QString expr = item->text().trimmed();

    QRegularExpression exprRE(
        lit("^"                         // beginning of the line
            "(("                        // chained identifiers, captured together
            "[a-zA-Z_][a-zA-Z_0-9]*"    // a named identifier
            "(\\[[0-9]+\\])?"           // a literal-indexed array expression
            "\\.?"                      // optional struct dot
            ")+)"                       // 1 or more chained identifiers
            "(,[xfiudb])?"              // optional typecast
            "$"));                      // end of the line

    QRegularExpression identifierSliceRE(
        lit("^"                          // beginning of the line
            "\\.?"                       // possible struct dot
            "("                          // begin capture
            "[a-zA-Z_][a-zA-Z_0-9]*|"    // a named identifier
            "(\\[[0-9]+\\])"             // or a literal-indexed array expression
            ")"));                       // end capture

    QRegularExpression swizzleRE(lit("^\\.[xyzwrgba]+$"));

    QRegularExpressionMatch match = exprRE.match(expr);

    QString error = tr("Error evaluating expression");

    if(match.hasMatch())
    {
      QString identifiers = match.captured(1);
      QChar regcast = QLatin1Char(' ');
      if(!match.captured(4).isEmpty())
        regcast = match.captured(4)[1];

      expr = identifiers;

      match = identifierSliceRE.match(identifiers);

      if(match.hasMatch())
      {
        QString base = match.captured(1);
        identifiers = identifiers.mid(base.length());

        RDTreeWidgetItem *node = findVarInTree(ui->sourceVars->invisibleRootItem(), base, false, 2);
        if(!node)
          node = findVarInTree(ui->constants->invisibleRootItem(), base, false, 2);
        if(!node)
          node = findVarInTree(ui->debugVars->invisibleRootItem(), base, false, 2);

        if(!node)
          error = tr("Couldn't find variable '%1'").arg(base);

        QString swizzle;

        // now we have the node, continue while there are still identifiers to resolve
        while(node && identifiers.length() > 0)
        {
          // get the next identifier
          match = identifierSliceRE.match(identifiers);

          if(!match.hasMatch())
          {
            error = tr("Parse error at '%1'").arg(identifiers);
            node = NULL;
            break;
          }

          QString identifier = match.captured(1);
          identifiers = identifiers.mid(match.capturedEnd(1));

          RDTreeWidgetItem *child = NULL;

          // handle arrays specially, as the child might be foo[0] or [0]
          if(identifier[0] == QLatin1Char('['))
          {
            child = findVarInTree(node, identifier, false, 1);

            if(!child)
              child = findVarInTree(node, node->text(0) + identifier, false, 1);
          }
          else
          {
            child = findVarInTree(node, identifier, false, 1);
          }

          // didn't find a match!
          if(!child)
          {
            // This is OK if this is the final identifier and it's a swizzle, that just looks like a
            // member
            if(swizzleRE.match(identifier).hasMatch() && identifiers.isEmpty())
            {
              swizzle = identifier.mid(1);
              break;
            }

            error = tr("Couldn't find '%1' in '%2'").arg(identifier).arg(node->text(0));

            // otherwise we've failed to resolve the expression
            node = NULL;
            break;
          }

          // recurse
          node = child;
        }

        if(node)
        {
          ShaderVariable var;
          QString regNames;

          if(getVar(node, &var, &regNames))
          {
            if(swizzle.isEmpty())
              swizzle = lit("xyzw").left((int)var.columns);

            if(regcast == QLatin1Char(' '))
            {
              if(var.type == VarType::Double)
                regcast = QLatin1Char('d');
              else if(var.type == VarType::Float || var.type == VarType::Half)
                regcast = QLatin1Char('f');
              else if(var.type == VarType::ULong || var.type == VarType::UInt ||
                      var.type == VarType::UShort || var.type == VarType::UByte)
                regcast = QLatin1Char('u');
              else if(var.type == VarType::SLong || var.type == VarType::SInt ||
                      var.type == VarType::SShort || var.type == VarType::SByte ||
                      var.type == VarType::Bool)
                regcast = QLatin1Char('i');
              else if(var.type == VarType::Unknown)
                regcast = ui->intView->isChecked() ? QLatin1Char('i') : QLatin1Char('f');
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

              if(regcast == QLatin1Char('i'))
              {
                val += Formatter::Format(var.value.iv[elindex]);
              }
              else if(regcast == QLatin1Char('f'))
              {
                val += Formatter::Format(var.value.fv[elindex]);
              }
              else if(regcast == QLatin1Char('u'))
              {
                val += Formatter::Format(var.value.uv[elindex]);
              }
              else if(regcast == QLatin1Char('x'))
              {
                val += Formatter::Format(var.value.uv[elindex], true);
              }
              else if(regcast == QLatin1Char('b'))
              {
                val += QFormatStr("%1").arg(var.value.uv[elindex], 32, 2, QLatin1Char('0'));
              }
              else if(regcast == QLatin1Char('d'))
              {
                val += Formatter::Format(var.value.dv[elindex]);
              }

              if(s < swizzle.count() - 1)
                val += lit(", ");
            }

            item = new QTableWidgetItem(regNames);
            item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            ui->watch->setItem(i, 1, item);

            item = new QTableWidgetItem(TypeString(var));
            item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            ui->watch->setItem(i, 2, item);

            item = new QTableWidgetItem(val);
            item->setData(Qt::UserRole, node->tag());
            item->setFlags(item->flags() & ~Qt::ItemIsEditable);

            ui->watch->setItem(i, 3, item);

            // success! continue
            continue;
          }
          else
          {
            error = tr("'%1' not a watchable variable").arg(expr);
          }
        }
      }
    }

    // if we got here, something went wrong.
    item = new QTableWidgetItem();
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    ui->watch->setItem(i, 1, item);

    item = new QTableWidgetItem();
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    ui->watch->setItem(i, 2, item);

    item = new QTableWidgetItem(error);
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    ui->watch->setItem(i, 3, item);
  }

  ui->watch->setUpdatesEnabled(true);
}

RDTreeWidgetItem *ShaderViewer::makeSourceVariableNode(const ShaderVariable &var,
                                                       const rdcstr &sourcePath,
                                                       const rdcstr &debugVarPath, bool modified)
{
  QString typeName;

  if(var.type == VarType::UInt)
    typeName = lit("uint");
  else if(var.type == VarType::SInt)
    typeName = lit("int");
  else if(var.type == VarType::Float)
    typeName = lit("float");
  else if(var.type == VarType::Double)
    typeName = lit("double");
  else if(var.type == VarType::Bool)
    typeName = lit("bool");

  QString rowTypeName = typeName;

  if(var.rows > 1)
  {
    typeName += QFormatStr("%1x%2").arg(var.rows).arg(var.columns);
    if(var.columns > 1)
      rowTypeName += QString::number(var.columns);
  }
  else if(var.columns > 1)
  {
    typeName += QString::number(var.columns);
  }

  QString value = var.rows == 1 && var.members.empty() ? stringRep(var) : QString();

  rdcstr sep = var.name[0] == '[' ? "" : ".";

  rdcstr sourceName = sourcePath + sep + var.name;
  rdcstr debugName = debugVarPath + sep + var.name;

  RDTreeWidgetItem *node = new RDTreeWidgetItem({sourceName, debugName, typeName, value});

  for(const ShaderVariable &child : var.members)
    node->addChild(makeSourceVariableNode(child, sourceName, debugName, modified));

  // if this is a matrix, even if it has no explicit row members add the rows as children
  if(var.members.empty() && var.rows > 1)
  {
    for(uint32_t row = 0; row < var.rows; row++)
    {
      rdcstr rowsuffix = ".row" + ToStr(row);
      node->addChild(new RDTreeWidgetItem(
          {sourceName + rowsuffix, debugName + rowsuffix, rowTypeName, stringRep(var, row)}));
    }
  }

  if(modified)
    node->setForegroundColor(QColor(Qt::red));

  return node;
}

RDTreeWidgetItem *ShaderViewer::makeSourceVariableNode(const SourceVariableMapping &l,
                                                       int globalVarIdx, int localVarIdx,
                                                       bool modified)
{
  const QString xyzw = lit("xyzw");

  QString localName = l.name;
  QString regNames, typeName;
  QString value;

  if(l.type == VarType::UInt)
    typeName = lit("uint");
  else if(l.type == VarType::SInt)
    typeName = lit("int");
  else if(l.type == VarType::Float)
    typeName = lit("float");
  else if(l.type == VarType::Double)
    typeName = lit("double");
  else if(l.type == VarType::Bool)
    typeName = lit("bool");

  QList<RDTreeWidgetItem *> children;

  {
    if(l.rows > 1)
      typeName += QFormatStr("%1x%2").arg(l.rows).arg(l.columns);
    else if(l.columns > 1)
      typeName += QString::number(l.columns);

    for(size_t i = 0; i < l.variables.size(); i++)
    {
      const DebugVariableReference &r = l.variables[i];

      if(!value.isEmpty())
        value += lit(", ");
      if(!regNames.isEmpty())
        regNames += lit(", ");

      if(r.name.empty())
      {
        regNames += lit("-");
        value += lit("?");
      }
      else if(r.type == DebugVariableType::Sampler)
      {
        const ShaderVariable *reg = GetDebugVariable(r);

        if(reg == NULL)
          continue;

        regNames = r.name;
        typeName = lit("Sampler");

        rdcarray<BoundResourceArray> samplers = m_Ctx.CurPipelineState().GetSamplers(m_Stage);

        int32_t idx = m_Mapping.samplers.indexOf(Bindpoint(reg->GetBinding()));

        if(idx < 0)
          continue;

        Bindpoint bind = m_Mapping.samplers[idx];

        int32_t bindIdx = samplers.indexOf(bind);

        if(bindIdx < 0)
          continue;

        BoundResourceArray &res = samplers[bindIdx];

        if(bind.arraySize == 1)
        {
          if(!res.resources.empty())
            value = samplerRep(bind, ~0U, res.resources[0].resourceId);
        }
        else if(bind.arraySize == ~0U)
        {
          regNames = QString();
          typeName = lit("[unbounded]");
          value = QString();
        }
        else
        {
          for(uint32_t a = 0; a < bind.arraySize; a++)
            children.push_back(new RDTreeWidgetItem({
                QFormatStr("%1[%2]").arg(localName).arg(a), QFormatStr("%1[%2]").arg(regNames).arg(a),
                typeName, samplerRep(bind, a, res.resources[a].resourceId),
            }));

          regNames = QString();
          typeName = QFormatStr("[%1]").arg(bind.arraySize);
          value = QString();
        }
      }
      else if(r.type == DebugVariableType::ReadOnlyResource ||
              r.type == DebugVariableType::ReadWriteResource)
      {
        const bool isReadOnlyResource = r.type == DebugVariableType::ReadOnlyResource;

        const ShaderVariable *reg = GetDebugVariable(r);

        if(reg == NULL)
          continue;

        regNames = r.name;
        typeName = isReadOnlyResource ? lit("Resource") : lit("RW Resource");

        rdcarray<BoundResourceArray> &resList =
            isReadOnlyResource ? m_ReadOnlyResources : m_ReadWriteResources;

        int32_t idx =
            (isReadOnlyResource ? m_Mapping.readOnlyResources : m_Mapping.readWriteResources)
                .indexOf(Bindpoint(reg->GetBinding()));

        if(idx < 0)
          continue;

        Bindpoint bind = isReadOnlyResource ? m_Mapping.readOnlyResources[idx]
                                            : m_Mapping.readWriteResources[idx];

        int32_t bindIdx = resList.indexOf(bind);

        if(bindIdx < 0)
          continue;

        BoundResourceArray &res = resList[bindIdx];
        if(bind.arraySize == 1)
        {
          if(!res.resources.empty())
            value = ToQStr(res.resources[0].resourceId);
        }
        else if(bind.arraySize == ~0U)
        {
          regNames = QString();
          typeName = lit("[unbounded]");
          value = QString();
        }
        else
        {
          uint32_t count = qMin(bind.arraySize, (uint32_t)res.resources.size());
          for(uint32_t a = 0; a < count; a++)
          {
            children.push_back(new RDTreeWidgetItem({
                QFormatStr("%1[%2]").arg(localName).arg(a), QFormatStr("%1[%2]").arg(regNames).arg(a),
                typeName, ToQStr(res.resources[a].resourceId),
            }));
          }

          regNames = QString();
          typeName = QFormatStr("[%1]").arg(bind.arraySize);
          value = QString();
        }
      }
      else
      {
        const ShaderVariable *reg = GetDebugVariable(r);

        if(reg)
        {
          if(!reg->members.empty())
          {
            // if the register we were pointed at is a complex type (struct/array/etc), embed it as
            // a child
            typeName = QString();
            value = QString();

            for(const ShaderVariable &child : reg->members)
              children.push_back(makeSourceVariableNode(child, localName, reg->name, modified));
            break;
          }
          else if(i > 0 && r.name == l.variables[i - 1].name &&
                  (r.component / reg->columns) == (l.variables[i - 1].component / reg->columns))
          {
            // if the previous register was the same, just append our component
            // remove the auto-appended ", " - there must be one because this isn't the first
            // register
            regNames.chop(2);
            regNames += xyzw[r.component % 4];
          }
          else
          {
            if(reg->rows > 1)
              regNames += QFormatStr("%1.row%2.%3")
                              .arg(reg->name)
                              .arg(r.component / reg->columns)
                              .arg(xyzw[r.component % 4]);
            else
              regNames += QFormatStr("%1.%2").arg(r.name).arg(xyzw[r.component % 4]);
          }

          if(l.type == VarType::UInt)
            value += Formatter::Format(reg->value.uv[r.component]);
          else if(l.type == VarType::SInt)
            value += Formatter::Format(reg->value.iv[r.component]);
          else if(l.type == VarType::Bool)
            value += Formatter::Format(reg->value.uv[r.component] ? true : false);
          else if(l.type == VarType::Float)
            value += Formatter::Format(reg->value.fv[r.component]);
          else if(l.type == VarType::Double)
            value += Formatter::Format(reg->value.dv[r.component]);
        }
        else
        {
          regNames += lit("<error>");
          value += lit("<error>");
        }
      }

      if(l.rows > 1 && l.variables.size() > l.columns)
      {
        if(((i + 1) % l.columns) == 0)
        {
          QString localBaseName = localName;
          int dot = localBaseName.lastIndexOf(QLatin1Char('.'));
          if(dot >= 0)
            localBaseName = localBaseName.mid(dot + 1);

          uint32_t row = (uint32_t)i / l.columns;
          children.push_back(new RDTreeWidgetItem(
              {QFormatStr("%1.row%2").arg(localBaseName).arg(row), regNames, typeName, value}));
          regNames = QString();
          value = QString();
        }
      }
    }
  }

  RDTreeWidgetItem *node = new RDTreeWidgetItem({localName, regNames, typeName, value});

  for(RDTreeWidgetItem *c : children)
    node->addChild(c);

  if(modified)
    node->setForegroundColor(QColor(Qt::red));

  node->setTag(QVariant::fromValue(VariableTag(localName, l.offset, globalVarIdx, localVarIdx)));

  return node;
}

RDTreeWidgetItem *ShaderViewer::makeDebugVariableNode(const ShaderVariable &v, rdcstr prefix,
                                                      bool modified)
{
  rdcstr basename = prefix + v.name;
  RDTreeWidgetItem *node =
      new RDTreeWidgetItem({v.name, v.rows == 1 && v.members.empty() ? stringRep(v) : QString()});
  node->setTag(QVariant::fromValue(
      VariableTag(DebugVariableReference(DebugVariableType::Variable, basename))));
  for(const ShaderVariable &m : v.members)
  {
    rdcstr childprefix = basename + ".";
    if(m.name.beginsWith(basename + "["))
      childprefix = basename;
    node->addChild(makeDebugVariableNode(m, childprefix, modified));
  }

  // if this is a matrix, even if it has no explicit row members add the rows as children
  if(v.members.empty() && v.rows > 1)
  {
    for(uint32_t row = 0; row < v.rows; row++)
    {
      rdcstr rowsuffix = ".row" + ToStr(row);
      RDTreeWidgetItem *child = new RDTreeWidgetItem({v.name + rowsuffix, stringRep(v, row)});
      child->setTag(QVariant::fromValue(
          VariableTag(DebugVariableReference(DebugVariableType::Variable, basename + rowsuffix))));
      node->addChild(child);
    }
  }

  if(modified)
    node->setForegroundColor(QColor(Qt::red));

  return node;
}

RDTreeWidgetItem *ShaderViewer::makeAccessedResourceNode(const ShaderVariable &v, bool modified)
{
  BindpointIndex bp = v.GetBinding();
  ResourceId resId;
  QString typeName;
  if(v.type == VarType::ReadOnlyResource)
  {
    typeName = lit("Resource");
    int32_t idx = m_Mapping.readOnlyResources.indexOf(Bindpoint(bp));
    if(idx >= 0)
    {
      Bindpoint bind = m_Mapping.readOnlyResources[idx];
      if(bind.used)
      {
        int32_t bindIdx = m_ReadOnlyResources.indexOf(bind);
        if(bindIdx >= 0)
        {
          BoundResourceArray &roBind = m_ReadOnlyResources[bindIdx];
          if(bp.arrayIndex < roBind.resources.size())
            resId = roBind.resources[bp.arrayIndex].resourceId;
        }
      }
    }
  }
  else if(v.type == VarType::ReadWriteResource)
  {
    typeName = lit("RW Resource");
    int32_t idx = m_Mapping.readWriteResources.indexOf(Bindpoint(bp));
    if(idx >= 0)
    {
      Bindpoint bind = m_Mapping.readWriteResources[idx];
      if(bind.used)
      {
        int32_t bindIdx = m_ReadWriteResources.indexOf(bind);
        if(bindIdx >= 0)
        {
          BoundResourceArray &rwBind = m_ReadWriteResources[bindIdx];
          if(bp.arrayIndex < rwBind.resources.size())
            resId = rwBind.resources[bp.arrayIndex].resourceId;
        }
      }
    }
  }

  RDTreeWidgetItem *node = new RDTreeWidgetItem({v.name, typeName, ToQStr(resId)});
  if(resId != ResourceId())
    node->setTag(QVariant::fromValue(AccessedResourceTag(bp, v.type)));
  if(modified)
    node->setForegroundColor(QColor(Qt::red));

  return node;
}

const ShaderVariable *ShaderViewer::GetDebugVariable(const DebugVariableReference &r)
{
  if(r.type == DebugVariableType::ReadOnlyResource)
  {
    for(int i = 0; i < m_Trace->readOnlyResources.count(); i++)
      if(m_Trace->readOnlyResources[i].name == r.name)
        return &m_Trace->readOnlyResources[i];

    return NULL;
  }
  else if(r.type == DebugVariableType::ReadWriteResource)
  {
    for(int i = 0; i < m_Trace->readWriteResources.count(); i++)
      if(m_Trace->readWriteResources[i].name == r.name)
        return &m_Trace->readWriteResources[i];

    return NULL;
  }
  else if(r.type == DebugVariableType::Sampler)
  {
    for(int i = 0; i < m_Trace->samplers.count(); i++)
      if(m_Trace->samplers[i].name == r.name)
        return &m_Trace->samplers[i];

    return NULL;
  }
  else if(r.type == DebugVariableType::Input || r.type == DebugVariableType::Constant ||
          r.type == DebugVariableType::Variable)
  {
    rdcarray<ShaderVariable> *vars =
        r.type == DebugVariableType::Input
            ? &m_Trace->inputs
            : (r.type == DebugVariableType::Constant ? &m_Trace->constantBlocks : &m_Variables);

    rdcstr path = r.name;

    while(!path.empty())
    {
      rdcstr elem;

      // pick out the next element in the path
      // if this is an array index, grab that
      if(path[0] == '[')
      {
        int idx = path.indexOf(']');
        if(idx < 0)
          break;
        elem = path.substr(0, idx + 1);

        // skip past any .s
        if(path[idx + 1] == '.')
          idx++;

        path = path.substr(idx + 1);
      }
      else
      {
        // otherwise look for the next identifier
        int idx = path.find_first_of("[.");
        if(idx < 0)
        {
          // no results means that all that's left of the path is an identifier
          elem.swap(path);
        }
        else
        {
          elem = path.substr(0, idx);

          // skip past any .s
          if(path[idx] == '.')
            idx++;

          path = path.substr(idx);
        }
      }

      // look in our current set of vars for a matching variable
      bool found = false;
      for(size_t i = 0; i < vars->size(); i++)
      {
        if(vars->at(i).name == elem)
        {
          // found the match.
          found = true;

          // If there's no more path, we've found the exact match, otherwise continue
          if(path.empty())
            return &vars->at(i);

          vars = &vars->at(i).members;

          break;
        }
      }

      if(!found)
        break;
    }

    return NULL;
  }

  return NULL;
}

void ShaderViewer::ensureLineScrolled(ScintillaEdit *s, int line)
{
  int firstLine = s->firstVisibleLine();
  int linesVisible = s->linesOnScreen();

  if(s->isVisible() && (line < firstLine || line > (firstLine + linesVisible - 1)))
    s->setFirstVisibleLine(qMax(0, line - linesVisible / 2));
}

uint32_t ShaderViewer::CurrentStep()
{
  return (uint32_t)m_CurrentStateIdx;
}

void ShaderViewer::SetCurrentStep(uint32_t step)
{
  if(!m_Trace || m_States.empty())
    return;

  while(GetCurrentState().stepIndex != step)
  {
    if(GetCurrentState().stepIndex < step)
    {
      // move forward if possible
      if(!IsLastState())
        applyForwardsChange();
      else
        break;
    }
    else
    {
      // move backward if possible
      if(!IsFirstState())
        applyBackwardsChange();
      else
        break;
    }
  }

  updateDebugState();
}

void ShaderViewer::ToggleBreakpoint(int instruction)
{
  if(!m_Trace || m_States.empty())
    return;

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

      QMap<int32_t, QVector<size_t>> &fileMap = m_Line2Insts[scintillaIndex];

      // find the next line that maps to an instruction
      for(; i < cur->lineCount(); i++)
      {
        if(fileMap.contains(i))
        {
          for(size_t inst : fileMap[i])
            ToggleBreakpoint((int)inst);

          return;
        }
      }
    }
    else
    {
      instLine = m_DisassemblyView->lineFromPosition(m_DisassemblyView->currentPos());

      for(; instLine < m_DisassemblyView->lineCount(); instLine++)
      {
        instruction = instructionForDisassemblyLine(instLine);

        if(instruction >= 0)
          break;
      }
    }
  }

  if(instruction < 0 || instruction >= m_Trace->lineInfo.count())
    return;

  if(instLine == -1)
  {
    if(instruction < m_Trace->lineInfo.count())
      instLine = m_Trace->lineInfo[instruction].disassemblyLine - 1;
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
      m_Breakpoints.push_back(instruction);
    }
  }
}

void ShaderViewer::ShowErrors(const rdcstr &errors)
{
  if(m_Errors)
  {
    m_Errors->setReadOnly(false);
    SetTextAndUpdateMargin0(m_Errors, errors);
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
        showVariableTooltip(tag.debugVar.name);
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
        showVariableTooltip(tag.debugVar.name);
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
  if(!m_Trace || m_States.empty())
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

void ShaderViewer::showVariableTooltip(QString name)
{
  m_TooltipName = name;
  m_TooltipPos = QCursor::pos();

  updateVariableTooltip();
}

void ShaderViewer::updateVariableTooltip()
{
  if(!m_Trace || m_States.empty())
    return;

  ShaderVariable var;

  if(!findVar(m_TooltipName, &var))
    return;

  if(var.type != VarType::Unknown)
  {
    QString tooltip;

    if(var.type == VarType::ReadOnlyResource || var.type == VarType::ReadWriteResource)
    {
      tooltip = RichResourceTextFormat(m_Ctx, stringRep(var, 0));
    }
    else
    {
      tooltip = QFormatStr("<pre>%1: %2\n").arg(var.name).arg(RowString(var, 0));
      QString spacing = QString(var.name.count(), QLatin1Char(' '));
      for(int i = 1; i < var.rows; i++)
        tooltip += QFormatStr("%1  %2\n").arg(spacing).arg(RowString(var, i));
      tooltip += lit("</pre>");
    }

    QToolTip::showText(m_TooltipPos, tooltip);
    return;
  }

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
  m_TooltipVarIndex = -1;
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

  updateDebugState();
}

void ShaderViewer::on_floatView_clicked()
{
  ui->floatView->setChecked(true);
  ui->intView->setChecked(false);

  updateDebugState();
}

void ShaderViewer::on_debugToggle_clicked()
{
  if(isSourceDebugging())
    gotoDisassemblyDebugging();
  else
    gotoSourceDebugging();

  updateDebugState();
}

void ShaderViewer::on_resources_sortByStep_clicked()
{
  m_AccessedResourceView = AccessedResourceView::SortByStep;
  ui->resources_sortByStep->setChecked(true);
  ui->resources_sortByResource->setChecked(false);
  updateAccessedResources();
}

void ShaderViewer::on_resources_sortByResource_clicked()
{
  m_AccessedResourceView = AccessedResourceView::SortByResource;
  ui->resources_sortByResource->setChecked(true);
  ui->resources_sortByStep->setChecked(false);
  updateAccessedResources();
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

  QString findHash = QFormatStr("%1%2%3%4").arg(find).arg(flags).arg((int)context).arg(down);

  if(findHash != m_FindState.hash)
  {
    m_FindState.hash = findHash;
    m_FindState.start = 0;
    m_FindState.end = cur->length();
    m_FindState.offset = cur->currentPos();
    if(down && cur->selectionStart() == m_FindState.offset &&
       cur->selectionEnd() - m_FindState.offset == find.length())
      m_FindState.offset += find.length();
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
