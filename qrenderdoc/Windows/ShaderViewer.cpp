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

#include "ShaderViewer.h"
#include <QComboBox>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QListWidget>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QSet>
#include <QShortcut>
#include <QToolTip>
#include "Code/Resources.h"
#include "Code/ScintillaSyntax.h"
#include "Widgets/Extended/RDLabel.h"
#include "Widgets/FindReplace.h"
#include "scintilla/include/SciLexer.h"
#include "scintilla/include/qt/ScintillaEdit.h"
#include "toolwindowmanager/ToolWindowManager.h"
#include "toolwindowmanager/ToolWindowManagerArea.h"
#include "ui_ShaderViewer.h"

struct AccessedResourceTag
{
  AccessedResourceTag() : type(VarType::Unknown), step(0)
  {
    bind.category = DescriptorCategory::Unknown;
  }
  AccessedResourceTag(uint32_t s) : type(VarType::Unknown), step(s)
  {
    bind.category = DescriptorCategory::Unknown;
  }
  AccessedResourceTag(ShaderBindIndex bp, VarType t) : bind(bp), type(t), step(0) {}
  AccessedResourceTag(ShaderVariable var) : step(0), type(var.type)
  {
    if(var.type == VarType::ReadOnlyResource || var.type == VarType::ReadWriteResource)
      bind = var.GetBindIndex();
    else
      bind.category = DescriptorCategory::Unknown;
  }
  ShaderBindIndex bind;
  VarType type;
  uint32_t step;
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
  QObject::connect(m_FindReplace, &FindReplace::keyPress, [this](QKeyEvent *e) {
    if(e->key() == Qt::Key_Escape)
    {
      // the find replace dialog is the only thing allowed to float. If it's in a floating area,
      // hide it on escape
      ToolWindowManagerArea *area = ui->docking->areaOf(m_FindReplace);

      if(area && ui->docking->isFloating(m_FindReplace))
      {
        ui->docking->hideToolWindow(m_FindReplace);
      }
    }
  });

  ui->docking->addToolWindow(m_FindReplace, ToolWindowManager::NoArea);
  ui->docking->setToolWindowProperties(m_FindReplace, ToolWindowManager::HideOnClose);

  ui->docking->addToolWindow(m_FindResults, ToolWindowManager::NoArea);
  ui->docking->setToolWindowProperties(
      m_FindResults, ToolWindowManager::HideOnClose | ToolWindowManager::DisallowFloatWindow);

  QObject::connect(m_FindResults, &ScintillaEdit::doubleClick, this,
                   &ShaderViewer::resultsDoubleClick);

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

  {
    QMenu *snippetsMenu = new QMenu(this);

    QAction *constants = new QAction(tr("Per-texture constants"), this);
    QAction *samplers = new QAction(tr("Point && Linear Samplers"), this);
    QAction *resources = new QAction(tr("Texture Resources"), this);

    snippetsMenu->addAction(constants);
    snippetsMenu->addAction(samplers);
    snippetsMenu->addAction(resources);

    QObject::connect(constants, &QAction::triggered, this, &ShaderViewer::snippet_constants);
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
                              const rdcstrpairs &files, KnownShaderTool knownTool,
                              ShaderEncoding shaderEncoding, ShaderCompileFlags flags)
{
  m_Scintillas.removeOne(m_DisassemblyView);
  ui->docking->removeToolWindow(m_DisassemblyFrame);

  m_DisassemblyView = NULL;

  m_Stage = stage;
  m_Flags = flags;

  m_CustomShader = (id == ResourceId());
  m_EditingShader = id;

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

  QObject::connect(ui->entryFunc, &QLineEdit::textChanged,
                   [this](const QString &) { MarkModification(); });
  QObject::connect(ui->toolCommandLine, &QTextEdit::textChanged, [this]() { MarkModification(); });

  PopulateCompileTools();

  QObject::connect(ui->encoding, OverloadedSlot<int>::of(&QComboBox::currentIndexChanged),
                   [this](int) {
                     PopulateCompileTools();
                     MarkModification();
                   });
  QObject::connect(ui->compileTool, OverloadedSlot<int>::of(&QComboBox::currentIndexChanged),
                   [this](int) {
                     PopulateCompileToolParameters();
                     MarkModification();
                   });

  // if we know the shader was originally compiled with a specific tool, pick the first matching
  // tool we have configured.
  if(knownTool != KnownShaderTool::Unknown)
  {
    for(const ShaderProcessingTool &tool : m_Ctx.Config().ShaderProcessors)
    {
      // skip tools that can't accept our inputs, or doesn't produce a supported output
      if(tool.tool == knownTool)
      {
        for(int i = 0; i < ui->compileTool->count(); i++)
        {
          if(ui->compileTool->itemText(i) == tool.name)
          {
            ui->compileTool->setCurrentIndex(i);
            break;
          }
        }
        break;
      }
    }
  }

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

  if(m_CustomShader)
  {
    ui->snippets->show();
    ui->refresh->setText(tr("Refresh"));
    ui->resetEdits->hide();
    ui->unrefresh->hide();
    ui->editStatusLabel->hide();
  }
  else
  {
    ui->snippets->hide();
  }

  // hide debugging toolbar buttons
  ui->editSep->hide();
  ui->execBackwards->hide();
  ui->execForwards->hide();
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

    QObject::connect(scintilla, &ScintillaEdit::modified,
                     [this](int type, int, int, int, const QByteArray &, int, int, int) {
                       if(type & (SC_MOD_INSERTTEXT | SC_MOD_DELETETEXT | SC_MOD_BEFOREINSERT |
                                  SC_MOD_BEFOREDELETE))
                         m_FindState = FindState();

                       MarkModification();
                     });
    QWidget *w = (QWidget *)scintilla;
    w->setProperty("filename", kv.first);
    w->setProperty("origText", kv.second);

    if(sel == NULL)
    {
      sel = scintilla;

      title = tr(" - %1 - %2()").arg(name).arg(entryPoint);
    }
  }

  m_Ctx.GetMainWindow()->RegisterShortcut(QKeySequence(QKeySequence::Refresh).toString(), this,
                                          [this](QWidget *) { on_refresh_clicked(); });
  ui->refresh->setToolTip(ui->refresh->toolTip() +
                          lit(" (%1)").arg(QKeySequence(QKeySequence::Refresh).toString()));

  if(sel != NULL)
    ToolWindowManager::raiseToolWindow(sel);

  if(m_CustomShader)
    title.prepend(tr("Editing %1 Shader").arg(ToQStr(stage, m_Ctx.APIProps().pipelineType)));
  else
    title.prepend(tr("Editing %1").arg(m_Ctx.GetResourceNameUnsuffixed(id)));

  setWindowTitle(title);

  if(files.count() > 2)
    addFileList();

  ConfigureBookmarkMenu();

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

void ShaderViewer::debugShader(const ShaderReflection *shader, ResourceId pipeline,
                               ShaderDebugTrace *trace, const QString &debugContext)
{
  m_ShaderDetails = shader;
  m_Pipeline = pipeline;
  m_Trace = trace;
  m_Stage = ShaderStage::Vertex;
  m_DebugContext = debugContext;

  // no recompilation happening, hide that group
  ui->compilationGroup->hide();

  // no replacing allowed, stay in find mode
  m_FindReplace->allowUserModeChange(false);

  if(!m_ShaderDetails)
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
              if(d.input == m_ShaderDetails->encoding && IsTextRepresentation(d.output))
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
    updateWindowTitle();

    // add all the files, skipping any that have empty contents. We push a NULL in that case so the
    // indices still match up with what the debug info expects. Debug info *shouldn't* point us at
    // an empty file, but if it does we'll just bail out when we see NULL
    m_FileScintillas.reserve(m_ShaderDetails->debugInfo.files.count());

    QWidget *sel = NULL;
    int32_t entryFile = m_ShaderDetails->debugInfo.entryLocation.fileIndex;
    int32_t i = -1;
    for(const ShaderSourceFile &f : m_ShaderDetails->debugInfo.files)
    {
      i++;
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

      if(i == entryFile)
      {
        sel = scintilla;

        if(m_ShaderDetails->debugInfo.entryLocation.lineStart > 0)
        {
          GUIInvoke::defer(scintilla, [scintilla, this]() {
            ensureLineScrolled(scintilla, m_ShaderDetails->debugInfo.entryLocation.lineStart);
          });
        }
      }

      m_FileScintillas.push_back(scintilla);
    }

    if(m_Trace || sel == NULL)
      sel = m_DisassemblyFrame;

    if(m_ShaderDetails->debugInfo.files.size() > 2)
      addFileList();

    ToolWindowManager::raiseToolWindow(sel);
  }

  // hide edit buttons
  ui->refresh->hide();
  ui->unrefresh->hide();
  ui->resetEdits->hide();
  ui->editStatusLabel->hide();
  ui->snippets->hide();
  ui->editSep->hide();

  ConfigureBookmarkMenu();

  if(m_Trace)
  {
    // hide signatures
    ui->inputSig->hide();
    ui->outputSig->hide();

    // hide int/float toggles except on DXBC, other encodings are strongly typed
    if(m_ShaderDetails->encoding != ShaderEncoding::DXBC)
    {
      ui->intView->hide();
      ui->floatView->hide();
    }

    if(!m_ShaderDetails->debugInfo.sourceDebugInformation)
    {
      ui->debugToggle->setEnabled(false);
      ui->debugToggle->setText(tr("Source debugging Unavailable"));
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

    ui->watch->setColumns({tr("Name"), tr("Register(s)"), tr("Type"), tr("Value")});
    ui->watch->header()->setSectionResizeMode(0, QHeaderView::Interactive);
    ui->watch->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    ui->watch->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    ui->watch->header()->setSectionResizeMode(3, QHeaderView::Interactive);

    ui->watch->header()->resizeSection(0, 80);

    ui->watch->setItemDelegate(new FullEditorDelegate(ui->watch));

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

    bool hasLineInfo = false;
    LineColumnInfo prevLine;

    for(uint32_t inst = 0; inst < m_Trace->instInfo.size(); inst++)
    {
      const InstructionSourceInfo &instInfo = m_Trace->instInfo[inst];

      LineColumnInfo line = instInfo.lineInfo;

      int disasmLine = (int)line.disassemblyLine;
      if(disasmLine > 0 && disasmLine >= m_AsmLine2Inst.size())
      {
        int oldSize = m_AsmLine2Inst.size();
        m_AsmLine2Inst.resize(disasmLine + 1);
        for(int i = oldSize; i < disasmLine; i++)
          m_AsmLine2Inst[i] = -1;
      }

      if(disasmLine > 0)
        m_AsmLine2Inst[disasmLine] = (int)instInfo.instruction;

      if(line.fileIndex < 0)
        continue;

      hasLineInfo = true;

      // store the source location *without* any disassembly line
      line.disassemblyLine = 0;

      // skip any instructions with the same mapping as the last one, this is a contiguous block
      if(line.SourceEqual(prevLine))
        continue;

      prevLine = line;

      m_Location2Inst[line].push_back(instInfo.instruction);
    }

    // if we don't have line mapping info, assume we also don't have useful high-level variable
    // info. Show the debug variables first rather than a potentially empty source variables panel.
    if(!hasLineInfo)
      ui->docking->raiseToolWindow(ui->debugVars);

    // set up stepping/running actions

    // we register the shortcuts via MainWindow so that it works regardless of the active scintilla
    // but still handles multiple shader viewers being present (the one with focus will get the
    // input)

    // all shortcuts have a reverse version with shift. This means step out is Ctrl-F11 instead of
    // Shift-F11, but otherwise the shortcuts behave the same as visual studio

    {
      QMenu *backwardsMenu = new QMenu(this);
      backwardsMenu->setToolTipsVisible(true);
      QAction *act;

      act = MakeExecuteAction(tr("&Run backwards"), Icons::control_start_blue(),
                              tr("Run backwards to the start of the shader"),
                              QKeySequence(Qt::Key_F5 | Qt::ShiftModifier));

      QObject::connect(act, &QAction::triggered, [this]() { runTo(~0U, false); });
      backwardsMenu->addAction(act);

      act = MakeExecuteAction(
          tr("Run backwards to &Cursor"), Icons::control_reverse_cursor_blue(),
          tr("Run backwards until execution reaches the cursor, or the start of the shader"),
          QKeySequence(Qt::Key_F10 | Qt::ControlModifier | Qt::ShiftModifier));

      QObject::connect(act, &QAction::triggered, [this]() { runToCursor(false); });
      backwardsMenu->addAction(act);

      act = MakeExecuteAction(tr("Run backwards to &Sample"), Icons::control_reverse_sample_blue(),
                              tr("Run backwards until execution reads from a resource, or the "
                                 "start of the shader is reached"),
                              QKeySequence());

      QObject::connect(act, &QAction::triggered,
                       [this]() { runTo(~0U, false, ShaderEvents::SampleLoadGather); });
      backwardsMenu->addAction(act);

      act = MakeExecuteAction(
          tr("Run backwards to &NaN/Inf"), Icons::control_reverse_nan_blue(),
          tr("Run backwards until a floating point instruction generates a NaN "
             "or Inf, an integer instruction divides by 0, or the start of the shader is reached"),
          QKeySequence());

      QObject::connect(act, &QAction::triggered,
                       [this]() { runTo(~0U, false, ShaderEvents::GeneratedNanOrInf); });
      backwardsMenu->addAction(act);

      backwardsMenu->addSeparator();

      act = MakeExecuteAction(tr("Step backwards &Over"), Icons::control_reverse_blue(),
                              tr("Step backwards, and don't enter functions when source debugging"),
                              QKeySequence(Qt::Key_F10 | Qt::ShiftModifier));

      QObject::connect(act, &QAction::triggered, [this]() { step(false, StepOver); });
      backwardsMenu->addAction(act);

      act = MakeExecuteAction(tr("Step backwards &Into"), Icons::control_reverse_blue(),
                              tr("Step backwards, entering functions when source debugging"),
                              QKeySequence(Qt::Key_F11 | Qt::ShiftModifier));

      QObject::connect(act, &QAction::triggered, [this]() { step(false, StepInto); });
      backwardsMenu->addAction(act);

      act =
          MakeExecuteAction(tr("Step backwards Ou&t"), Icons::control_reverse_blue(),
                            tr("Step backwards, out of the current function when source debugging"),
                            QKeySequence(Qt::Key_F11 | Qt::ControlModifier | Qt::ShiftModifier));

      QObject::connect(act, &QAction::triggered, [this]() { step(false, StepOut); });
      backwardsMenu->addAction(act);

      ui->execBackwards->setMenu(backwardsMenu);
    }

    {
      QMenu *forwardsMenu = new QMenu(this);
      forwardsMenu->setToolTipsVisible(true);
      QAction *act;

      act = MakeExecuteAction(tr("&Run forwards"), Icons::control_end_blue(),
                              tr("Run forwards to the start of the shader"),
                              QKeySequence(Qt::Key_F5));

      QObject::connect(act, &QAction::triggered, [this]() { runTo(~0U, true); });
      forwardsMenu->addAction(act);

      act = MakeExecuteAction(
          tr("Run forwards to &Cursor"), Icons::control_cursor_blue(),
          tr("Run forwards until execution reaches the cursor, or the end of the shader"),
          QKeySequence(Qt::Key_F10 | Qt::ControlModifier));

      QObject::connect(act, &QAction::triggered, [this]() { runToCursor(true); });
      forwardsMenu->addAction(act);

      act = MakeExecuteAction(tr("Run forwards to &Sample"), Icons::control_sample_blue(),
                              tr("Run forwards until execution reads from a resource, or the "
                                 "end of the shader is reached"),
                              QKeySequence());

      QObject::connect(act, &QAction::triggered,
                       [this]() { runTo(~0U, true, ShaderEvents::SampleLoadGather); });
      forwardsMenu->addAction(act);

      act = MakeExecuteAction(
          tr("Run forwards to &NaN/Inf"), Icons::control_nan_blue(),
          tr("Run forwards until a floating point instruction generates a NaN "
             "or Inf, an integer instruction divides by 0, or the end of the shader is reached"),
          QKeySequence());

      QObject::connect(act, &QAction::triggered,
                       [this]() { runTo(~0U, true, ShaderEvents::GeneratedNanOrInf); });
      forwardsMenu->addAction(act);

      forwardsMenu->addSeparator();

      act = MakeExecuteAction(tr("Step forwards &Over"), Icons::control_play_blue(),
                              tr("Step forwards, and don't enter functions when source debugging"),
                              QKeySequence(Qt::Key_F10));

      QObject::connect(act, &QAction::triggered, [this]() { step(true, StepOver); });
      forwardsMenu->addAction(act);

      act = MakeExecuteAction(tr("Step forwards &Into"), Icons::control_play_blue(),
                              tr("Step forwards, entering functions when source debugging"),
                              QKeySequence(Qt::Key_F11));

      QObject::connect(act, &QAction::triggered, [this]() { step(true, StepInto); });
      forwardsMenu->addAction(act);

      act =
          MakeExecuteAction(tr("Step forwards Ou&t"), Icons::control_play_blue(),
                            tr("Step forwards, out of the current function when source debugging"),
                            QKeySequence(Qt::Key_F11 | Qt::ControlModifier));

      QObject::connect(act, &QAction::triggered, [this]() { step(true, StepOut); });
      forwardsMenu->addAction(act);

      ui->execForwards->setMenu(forwardsMenu);
    }

    for(ScintillaEdit *edit : m_Scintillas)
    {
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

    // toggle breakpoint - F9
    m_Ctx.GetMainWindow()->RegisterShortcut(QKeySequence(Qt::Key_F9).toString(), this,
                                            [this](QWidget *) { ToggleBreakpointOnInstruction(); });

    // toggle bookmark - Ctrl-F2
    m_Ctx.GetMainWindow()->RegisterShortcut(QKeySequence(Qt::Key_F2 | Qt::ControlModifier).toString(),
                                            this, [this](QWidget *) { ToggleBookmark(); });
    // next bookmark - F2
    m_Ctx.GetMainWindow()->RegisterShortcut(QKeySequence(Qt::Key_F2).toString(), this,
                                            [this](QWidget *) { NextBookmark(); });
    // previous bookmark - Shift-F2
    m_Ctx.GetMainWindow()->RegisterShortcut(QKeySequence(Qt::Key_F2 | Qt::ShiftModifier).toString(),
                                            this, [this](QWidget *) { PreviousBookmark(); });

    // event filter to pick up tooltip events
    ui->constants->installEventFilter(this);
    ui->accessedResources->installEventFilter(this);
    ui->debugVars->installEventFilter(this);
    ui->watch->installEventFilter(this);

    cacheResources();

    m_BackgroundRunning.release();

    QPointer<ShaderViewer> me(this);

    m_DeferredInit = true;

    m_Ctx.Replay().AsyncInvoke([this, me](IReplayController *r) {
      if(!me)
        return;

      rdcarray<ShaderDebugState> *states = new rdcarray<ShaderDebugState>();

      states->append(r->ContinueDebug(m_Trace->debugger));

      rdcarray<ShaderDebugState> nextStates;

      bool finished = false;
      do
      {
        if(!me)
        {
          delete states;
          return;
        }

        nextStates = r->ContinueDebug(m_Trace->debugger);

        if(!me)
        {
          delete states;
          return;
        }

        finished = nextStates.empty();
        states->append(std::move(nextStates));
      } while(!finished && m_BackgroundRunning.available() == 1);

      if(!me)
      {
        delete states;
        return;
      }

      m_BackgroundRunning.tryAcquire(1);

      r->SetFrameEvent(m_Ctx.CurEvent(), true);

      if(!me)
      {
        delete states;
        return;
      }

      GUIInvoke::call(this, [this, states]() {
        m_States.swap(*states);
        delete states;

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
          // if we're not on a source line, move forward to the first source line
          while(!m_CurInstructionScintilla)
          {
            do
            {
              applyForwardsChange();

              if(GetCurrentInstInfo().lineInfo.fileIndex >= 0)
                break;

              if(IsLastState())
                break;

            } while(true);

            updateDebugState();

            if(IsLastState())
              break;
          }

          // if we got to the last state something's wrong - we're preferring source debug but we
          // didn't ever reach an instruction mapped to source lines? just reverse course and don't
          // switch to source debugging
          if(IsLastState())
          {
            m_FirstSourceStateIdx = ~0U;
            runTo(~0U, false);
          }
          else
          {
            m_FirstSourceStateIdx = m_CurrentStateIdx;
            gotoSourceDebugging();
            updateDebugState();
          }
        }

        m_DeferredInit = false;
        for(std::function<void(ShaderViewer *)> &f : m_DeferredCommands)
          f(this);
        m_DeferredCommands.clear();
      });
    });

    GUIInvoke::defer(this, [this, debugContext]() {
      // wait a short while before displaying the progress dialog (which won't show if we're already
      // done by the time we reach it)
      for(int i = 0; m_BackgroundRunning.available() == 1 && i < 100; i++)
        QThread::msleep(5);

      ShowProgressDialog(
          this, tr("Debugging %1").arg(debugContext),
          [this]() { return m_BackgroundRunning.available() == 0; }, NULL,
          [this]() { m_BackgroundRunning.acquire(); });
    });

    m_CurrentStateIdx = 0;

    QObject::connect(ui->watch, &RDTreeWidget::keyPress, this, &ShaderViewer::watch_keyPress);

    ui->watch->setContextMenuPolicy(Qt::CustomContextMenu);
    QObject::connect(ui->watch, &RDTreeWidget::customContextMenuRequested, this,
                     &ShaderViewer::variables_contextMenu);
    ui->constants->setContextMenuPolicy(Qt::CustomContextMenu);
    QObject::connect(ui->constants, &RDTreeWidget::customContextMenuRequested, this,
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

    RDTreeWidgetItem *item = new RDTreeWidgetItem({
        QVariant(),
        QVariant(),
        QVariant(),
        QVariant(),
    });
    item->setEditable(0, true);
    ui->watch->addTopLevelItem(item);

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
    ui->editSep->hide();
    ui->execBackwards->hide();
    ui->execForwards->hide();
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
        if(s.perPrimitiveRate)
          name += tr(" (Per-Prim)");

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

void ShaderViewer::ConfigureBookmarkMenu()
{
  QMenu *bookmarkMenu = new QMenu(this);
  bookmarkMenu->setToolTipsVisible(true);

  QAction *toggleAction = MakeExecuteAction(tr("Toggle Bookmark"), Icons::bookmark_blue(),
                                            tr("Toggle bookmark on current line"),
                                            QKeySequence(Qt::Key_F2 | Qt::ControlModifier));
  QObject::connect(toggleAction, &QAction::triggered, [this]() { ToggleBookmark(); });
  bookmarkMenu->addAction(toggleAction);

  QAction *nextAction =
      MakeExecuteAction(tr("Next Bookmark"), Icons::arrow_right(),
                        tr("Go to next bookmark in the current file"), QKeySequence(Qt::Key_F2));
  QObject::connect(nextAction, &QAction::triggered, [this]() { NextBookmark(); });
  bookmarkMenu->addAction(nextAction);

  QAction *prevAction = MakeExecuteAction(tr("Previous Bookmark"), Icons::arrow_left(),
                                          tr("Go to previous bookmark in the current file"),
                                          QKeySequence(Qt::Key_F2 | Qt::ShiftModifier));
  QObject::connect(prevAction, &QAction::triggered, [this]() { PreviousBookmark(); });
  bookmarkMenu->addAction(prevAction);

  QAction *clearAction =
      MakeExecuteAction(tr("Clear All Bookmarks"), Icons::cross(),
                        tr("Clear all bookmarks in the current file"), QKeySequence());
  QObject::connect(clearAction, &QAction::triggered, [this]() { ClearAllBookmarks(); });
  bookmarkMenu->addAction(clearAction);

  QObject::connect(bookmarkMenu, &QMenu::aboutToShow,
                   [this, bookmarkMenu, nextAction, prevAction, clearAction]() {
                     UpdateBookmarkMenu(bookmarkMenu, nextAction, prevAction, clearAction);
                   });

  bookmarkMenu->addSeparator();
  ui->bookmark->setMenu(bookmarkMenu);
}

void ShaderViewer::UpdateBookmarkMenu(QMenu *menu, QAction *nextAction, QAction *prevAction,
                                      QAction *clearAction)
{
  // setup permanent actions
  const bool hasBookmarks = HasBookmarks();
  nextAction->setEnabled(hasBookmarks);
  prevAction->setEnabled(hasBookmarks);
  clearAction->setEnabled(hasBookmarks);

  // remove current bookmark list
  QList<QAction *> actions = menu->actions();
  for(auto itAction = actions.rbegin(); itAction != actions.rend() && !(*itAction)->isSeparator();
      ++itAction)
    menu->removeAction(*itAction);

  // populate bookmark list
  ScintillaEdit *cur = currentScintilla();
  if(!cur)
    return;

  auto itBookmarks = m_Bookmarks.find(cur);
  if(itBookmarks == m_Bookmarks.end())
    return;

  QString filename;
  if(cur == m_DisassemblyView)
    filename = m_DisassemblyFrame->windowTitle();
  else
    filename = cur->windowTitle();

  int numAddedBookmarks = 0;
  for(sptr_t lineNumber : *itBookmarks)
  {
    QString textLine = QString::fromUtf8(cur->getLine(lineNumber)).simplified();
    if(textLine.size() > BOOKMARK_MAX_MENU_ENTRY_LENGTH)
    {
      textLine.chop(textLine.size() - BOOKMARK_MAX_MENU_ENTRY_LENGTH);
      textLine.append(lit("..."));
    }
    else if(textLine.isEmpty())
    {
      textLine = lit("(empty)");
    }

    QString name = QFormatStr("%1:%2 - %3").arg(filename).arg(lineNumber + 1).arg(textLine);
    QAction *action = new QAction(name, menu);
    QObject::connect(action, &QAction::triggered, [cur, lineNumber]() { cur->gotoLine(lineNumber); });
    menu->addAction(action);

    if(++numAddedBookmarks >= BOOKMARK_MAX_MENU_ENTRY_COUNT)
      break;
  }
}

QAction *ShaderViewer::MakeExecuteAction(QString name, const QIcon &icon, QString tooltip,
                                         QKeySequence shortcut)
{
  QAction *act = new QAction(name, this);
  // set the shortcut context to something that shouldn't fire, since we want to handle this
  // ourselves - we just want Qt to *display* the shortcut
  act->setShortcutContext(Qt::WidgetShortcut);
  act->setToolTip(tooltip);
  act->setIcon(icon);
  if(!shortcut.isEmpty())
  {
    act->setShortcut(shortcut);
    m_Ctx.GetMainWindow()->RegisterShortcut(act->shortcut().toString(), this,
                                            [act](QWidget *) { act->activate(QAction::Trigger); });
  }
  return act;
}

void ShaderViewer::updateWindowTitle()
{
  if(m_ShaderDetails)
  {
    QString shaderName = m_Ctx.GetResourceNameUnsuffixed(m_ShaderDetails->resourceId);

    // On D3D12, get the shader name from the pipeline rather than the shader itself
    // for the benefit of D3D12 which doesn't have separate shader objects
    if(m_Ctx.CurPipelineState().IsCaptureD3D12())
      shaderName = QFormatStr("%1 %2")
                       .arg(m_Ctx.GetResourceNameUnsuffixed(m_Pipeline))
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

ShaderViewer *ShaderViewer::LoadEditor(ICaptureContext &ctx, QVariantMap data,
                                       IShaderViewer::SaveCallback saveCallback,
                                       IShaderViewer::RevertCallback revertCallback,
                                       ModifyCallback modifyCallback, QWidget *parent)
{
  if(data.isEmpty())
    return NULL;

  ResourceId id;

  {
    QVariant v = data[lit("id")];
    RichResourceTextInitialise(v, &ctx);
    id = v.value<ResourceId>();
  }
  ShaderStage stage = (ShaderStage)data[lit("stage")].toUInt();
  rdcstr entryPoint = data[lit("entryPoint")].toString();
  ShaderEncoding encoding = (ShaderEncoding)data[lit("encoding")].toUInt();
  QString commandLine = data[lit("commandLine")].toString();
  QString toolName = data[lit("tool")].toString();

  ShaderCompileFlags flags;

  {
    QVariantMap v = data[lit("flags")].toMap();

    for(const QString &str : v.keys())
      flags.flags.push_back({(rdcstr)str, (rdcstr)v[str].toString()});
  }

  rdcstrpairs files;

  {
    QVariantList v = data[lit("files")].toList();

    for(QVariant f : v)
    {
      QVariantMap file = f.toMap();
      files.push_back(
          {(rdcstr)file[lit("name")].toString(), (rdcstr)file[lit("contents")].toString()});
    }
  }

  rdcstr errors;

  if((uint32_t)encoding >= (uint32_t)ShaderEncoding::Count)
  {
    errors += tr("Unrecognised shader encoding '%1'").arg(ToStr(encoding));
    // with no other information let's guess HLSL as the most likely
    encoding = ShaderEncoding::HLSL;
  }

  ShaderViewer *view =
      EditShader(ctx, id, stage, entryPoint, files, KnownShaderTool::Unknown, encoding, flags,
                 saveCallback, revertCallback, modifyCallback, parent);

  int toolIndex = -1;

  for(int i = 0; i < view->ui->compileTool->count(); i++)
  {
    QString tool = view->ui->compileTool->itemText(i);
    if(tool == toolName)
    {
      toolIndex = i;
      break;
    }
  }

  if(toolIndex == -1)
  {
    errors += tr("Unknown shader tool '%1'").arg(toolName);
    // let's pick the highest priority tool for the current encoding
    toolIndex = 0;
  }

  view->ui->encoding->setCurrentIndex(view->m_Encodings.indexOf(encoding));
  view->ui->compileTool->setCurrentIndex(toolIndex);
  view->ui->entryFunc->setText(entryPoint);
  view->ui->toolCommandLine->setText(commandLine);

  return view;
}

QVariantMap ShaderViewer::SaveEditor()
{
  QVariantMap ret;

  if(m_EditingShader != ResourceId())
  {
    ret[lit("id")] = m_EditingShader;
    ret[lit("stage")] = (uint32_t)m_Stage;
    ret[lit("entryPoint")] = ui->entryFunc->text();
    ret[lit("encoding")] = (uint32_t)currentEncoding();
    ret[lit("commandLine")] = ui->toolCommandLine->toPlainText();
    ret[lit("tool")] = ui->compileTool->currentText();

    {
      QVariantMap v;

      for(const ShaderCompileFlag &flag : m_Flags.flags)
        v[flag.name] = QString(flag.value);

      ret[lit("flags")] = v;
    }

    {
      QVariantList v;

      for(ScintillaEdit *edit : m_Scintillas)
      {
        QWidget *w = (QWidget *)edit;

        QVariantMap f;
        f[lit("name")] = w->property("filename").toString();
        f[lit("contents")] = QString::fromUtf8(edit->getText(edit->textLength() + 1));

        v.push_back(f);
      }

      ret[lit("files")] = v;
    }
  }

  return ret;
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

  if(m_RevertCallback)
    m_RevertCallback(&m_Ctx, this, m_EditingShader);

  if(m_ModifyCallback)
    m_ModifyCallback(this, true);

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
  ScintillaEdit *scintilla =
      MakeEditor(lit("scintilla") + name, text,
                 encoding == ShaderEncoding::HLSL || encoding == ShaderEncoding::Slang ? SCLEX_HLSL
                                                                                       : SCLEX_GLSL);
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

  ret->setMarginLeft(4.0);
  ret->setMarginWidthN(1, 20.0);
  ret->setMarginWidthN(2, 16.0);
  ret->setObjectName(name);

  ret->styleSetFont(STYLE_DEFAULT, Formatter::FixedFont().family().toUtf8().data());
  ret->styleSetSize(STYLE_DEFAULT, Formatter::FixedFont().pointSize());

  // C# DarkGreen
  ret->indicSetFore(INDICATOR_REGHIGHLIGHT, SCINTILLA_COLOUR(0, 100, 0));
  ret->indicSetStyle(INDICATOR_REGHIGHLIGHT, INDIC_ROUNDBOX);

  // set up find result highlight style
  ret->indicSetFore(INDICATOR_FINDRESULT, SCINTILLA_COLOUR(200, 200, 64));
  ret->indicSetStyle(INDICATOR_FINDRESULT, INDIC_FULLBOX);
  ret->indicSetAlpha(INDICATOR_FINDRESULT, 50);
  ret->indicSetOutlineAlpha(INDICATOR_FINDRESULT, 80);

  QColor highlightColor = palette().color(QPalette::Highlight).toRgb();

  ret->indicSetFore(
      INDICATOR_FINDALLHIGHLIGHT,
      SCINTILLA_COLOUR(highlightColor.red(), highlightColor.green(), highlightColor.blue()));
  ret->indicSetStyle(INDICATOR_FINDALLHIGHLIGHT, INDIC_FULLBOX);
  ret->indicSetAlpha(INDICATOR_FINDALLHIGHLIGHT, 120);
  ret->indicSetOutlineAlpha(INDICATOR_FINDALLHIGHLIGHT, 180);

  // C# Highlight for bookmarks
  ret->markerSetBack(BOOKMARK_MARKER, SCINTILLA_COLOUR(51, 153, 255));
  ret->markerDefine(BOOKMARK_MARKER, SC_MARK_BOOKMARK);

  ConfigureSyntax(ret, lang);

  ret->setTabWidth(4);

  ret->setScrollWidth(1);
  ret->setScrollWidthTracking(true);

  ret->colourise(0, -1);

  SetTextAndUpdateMargin0(ret, text);

  ret->emptyUndoBuffer();

  return ret;
}

void ShaderViewer::SetTextAndUpdateMargin0(ScintillaEdit *sc, const QString &text)
{
  sc->setText(text.toUtf8().data());

  int numLines = sc->lineCount();

  // don't make the margin too narrow, it looks strange even if there are only 5 lines in a file
  // we also add on an extra character for padding (with the *10)
  numLines = qMax(1000, numLines * 10);

  sptr_t width = sc->textWidth(SC_MARGIN_RTEXT, QString::number(numLines).toUtf8().data());

  sc->setMarginWidthN(0, int(width));
}

void ShaderViewer::readonly_keyPressed(QKeyEvent *event)
{
  if(event->key() == Qt::Key_F && (event->modifiers() & Qt::ControlModifier))
  {
    m_FindReplace->setReplaceMode(false);
    SetFindTextFromCurrentWord();
    on_findReplace_clicked();
  }

  if(event->key() == Qt::Key_F3)
  {
    if(event->modifiers() & Qt::ControlModifier)
      SetFindTextFromCurrentWord();
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
  m_ContextActive = true;
  hideVariableTooltip();

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
  QAction runForwardCursor(tr("Run forwards to Cursor"), this);
  QAction runBackwardCursor(tr("Run backwards to Cursor"), this);

  addBreakpoint.setShortcut(QKeySequence(Qt::Key_F9));
  runForwardCursor.setShortcut(QKeySequence(Qt::Key_F10 | Qt::ControlModifier));
  runBackwardCursor.setShortcut(QKeySequence(Qt::Key_F10 | Qt::ControlModifier | Qt::ShiftModifier));

  QObject::connect(&addBreakpoint, &QAction::triggered, [this, scintillaPos] {
    m_DisassemblyView->setSelection(scintillaPos, scintillaPos);
    ToggleBreakpointOnInstruction();
  });
  QObject::connect(&runForwardCursor, &QAction::triggered, [this, scintillaPos] {
    m_DisassemblyView->setSelection(scintillaPos, scintillaPos);
    runToCursor(true);
  });
  QObject::connect(&runBackwardCursor, &QAction::triggered, [this, scintillaPos] {
    m_DisassemblyView->setSelection(scintillaPos, scintillaPos);
    runToCursor(false);
  });

  contextMenu.addAction(&addBreakpoint);
  contextMenu.addAction(&runBackwardCursor);
  contextMenu.addAction(&runForwardCursor);
  contextMenu.addSeparator();

  QAction toggleBookmark(tr("Toggle bookmark here"), this);
  QAction nextBookmark(tr("Go to next Bookmark"), this);
  QAction prevBookmark(tr("Go to previous Bookmark"), this);
  QAction clearBookmarks(tr("Clear all Bookmarks"), this);

  const bool hasBookmarks = HasBookmarks();
  nextBookmark.setEnabled(hasBookmarks);
  prevBookmark.setEnabled(hasBookmarks);
  clearBookmarks.setEnabled(hasBookmarks);

  toggleBookmark.setShortcut(QKeySequence(Qt::Key_F2 | Qt::ControlModifier));
  nextBookmark.setShortcut(QKeySequence(Qt::Key_F2));
  prevBookmark.setShortcut(QKeySequence(Qt::Key_F2 | Qt::ShiftModifier));

  QObject::connect(&toggleBookmark, &QAction::triggered, [this] { ToggleBookmark(); });
  QObject::connect(&nextBookmark, &QAction::triggered, [this] { NextBookmark(); });
  QObject::connect(&prevBookmark, &QAction::triggered, [this] { PreviousBookmark(); });
  QObject::connect(&clearBookmarks, &QAction::triggered, [this] { ClearAllBookmarks(); });

  contextMenu.addAction(&toggleBookmark);
  contextMenu.addAction(&nextBookmark);
  contextMenu.addAction(&prevBookmark);
  contextMenu.addAction(&clearBookmarks);
  contextMenu.addSeparator();

  QAction watchExpr(tr("Add Watch Expression"), this);
  watchExpr.setEnabled(!edit->selectionEmpty());

  QObject::connect(&watchExpr, &QAction::triggered, [this, edit] {
    QString expr =
        QString::fromUtf8(edit->get_text_range(edit->selectionStart(), edit->selectionEnd()));

    for(QString e : expr.split(QLatin1Char(' ')))
      AddWatch(e);
  });

  contextMenu.addAction(&watchExpr);
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

  m_ContextActive = false;
}

void ShaderViewer::variables_contextMenu(const QPoint &pos)
{
  QAbstractItemView *w = qobject_cast<QAbstractItemView *>(QObject::sender());

  QMenu contextMenu(this);

  QAction copyValue(tr("Copy"), this);
  QAction addWatch(tr("Add Watch"), this);
  QAction deleteWatch(tr("Delete Watch"), this);
  QMenu interpretMenu(tr("Interpret As..."), this);
  QAction clearAll(tr("Clear All"), this);

  QAction interpDefault(tr("Default"), this);
  QAction interpFloat(tr("Floating point"), this);
  QAction interpSInt(tr("Signed decimal"), this);
  QAction interpUInt(tr("Unsigned decimal"), this);
  QAction interpHex(tr("Unsigned hexadecimal"), this);
  QAction interpOctal(tr("Unsigned octal"), this);
  QAction interpBinary(tr("Unsigned binary"), this);
  QAction interpColor(tr("Float RGB color"), this);

  interpDefault.setCheckable(true);
  interpFloat.setCheckable(true);
  interpSInt.setCheckable(true);
  interpUInt.setCheckable(true);
  interpHex.setCheckable(true);
  interpOctal.setCheckable(true);
  interpBinary.setCheckable(true);
  interpColor.setCheckable(true);

  interpretMenu.addAction(&interpDefault);
  interpretMenu.addAction(&interpFloat);
  interpretMenu.addAction(&interpSInt);
  interpretMenu.addAction(&interpUInt);
  interpretMenu.addAction(&interpHex);
  interpretMenu.addAction(&interpOctal);
  interpretMenu.addAction(&interpBinary);
  interpretMenu.addAction(&interpColor);

  contextMenu.addAction(&copyValue);
  contextMenu.addSeparator();
  contextMenu.addAction(&addWatch);

  if(QObject::sender() == ui->watch)
  {
    QObject::connect(&copyValue, &QAction::triggered, [this] { ui->watch->copySelection(); });

    contextMenu.addMenu(&interpretMenu);
    contextMenu.addAction(&deleteWatch);
    contextMenu.addSeparator();
    contextMenu.addAction(&clearAll);

    // start with no row selected
    RDTreeWidgetItem *item = ui->watch->selectedItem();

    int topLevelIdx = ui->watch->indexOfTopLevelItem(item);

    // last top level item is the empty entry for adding new values, it's not valid
    if(topLevelIdx == ui->watch->topLevelItemCount() - 1)
      topLevelIdx = -1;

    // if we have a top-level item selected, we can re-interpret or delete
    interpretMenu.setEnabled(topLevelIdx >= 0);
    deleteWatch.setEnabled(topLevelIdx >= 0);

    VariableTag tag;
    if(item)
      tag = item->tag().value<VariableTag>();

    // we can add any selected item as a watch that has a path to reference
    addWatch.setEnabled(item != NULL && !tag.absoluteRefPath.empty());

    if(topLevelIdx >= 0)
    {
      if(tag.state == WatchVarState::Valid)
      {
        QString baseUninterpText = item->text(0);
        int comma = baseUninterpText.lastIndexOf(QLatin1Char(','));

        if(comma < 0)
        {
          interpDefault.setChecked(true);
        }
        else
        {
          QChar interp = baseUninterpText[comma + 1];
          baseUninterpText = baseUninterpText.left(comma);

          if(interp == QLatin1Char('f'))
            interpFloat.setChecked(true);
          else if(interp == QLatin1Char('c'))
            interpColor.setChecked(true);
          else if(interp == QLatin1Char('d') || interp == QLatin1Char('i'))
            interpSInt.setChecked(true);
          else if(interp == QLatin1Char('u'))
            interpUInt.setChecked(true);
          else if(interp == QLatin1Char('x'))
            interpHex.setChecked(true);
          else if(interp == QLatin1Char('o'))
            interpOctal.setChecked(true);
          else if(interp == QLatin1Char('b'))
            interpBinary.setChecked(true);
        }

        QObject::connect(&interpFloat, &QAction::triggered, [item, baseUninterpText] {
          item->setText(0, baseUninterpText + lit(",f"));
        });
        QObject::connect(&interpColor, &QAction::triggered, [item, baseUninterpText] {
          item->setText(0, baseUninterpText + lit(",c"));
        });
        QObject::connect(&interpSInt, &QAction::triggered, [item, baseUninterpText] {
          item->setText(0, baseUninterpText + lit(",i"));
        });
        QObject::connect(&interpUInt, &QAction::triggered, [item, baseUninterpText] {
          item->setText(0, baseUninterpText + lit(",u"));
        });
        QObject::connect(&interpHex, &QAction::triggered, [item, baseUninterpText] {
          item->setText(0, baseUninterpText + lit(",x"));
        });
        QObject::connect(&interpOctal, &QAction::triggered, [item, baseUninterpText] {
          item->setText(0, baseUninterpText + lit(",o"));
        });
        QObject::connect(&interpBinary, &QAction::triggered, [item, baseUninterpText] {
          item->setText(0, baseUninterpText + lit(",b"));
        });
      }
      else
      {
        interpretMenu.setEnabled(false);
      }
    }

    QObject::connect(&addWatch, &QAction::triggered,
                     [this, item] { AddWatch(item->tag().value<VariableTag>().absoluteRefPath); });

    QObject::connect(&deleteWatch, &QAction::triggered, [this, topLevelIdx] {
      RDTreeViewExpansionState expansion;
      ui->watch->saveExpansion(expansion, 0);

      ui->watch->beginUpdate();

      delete ui->watch->takeTopLevelItem(topLevelIdx);

      ui->watch->endUpdate();

      ui->watch->applyExpansion(expansion, 0);
    });

    QObject::connect(&clearAll, &QAction::triggered, [this] {
      ui->watch->clear();
      RDTreeWidgetItem *item = new RDTreeWidgetItem({
          QVariant(),
          QVariant(),
          QVariant(),
          QVariant(),
      });
      item->setEditable(0, true);
      ui->watch->addTopLevelItem(item);
    });
  }
  else
  {
    RDTreeWidget *tree = qobject_cast<RDTreeWidget *>(w);

    QObject::connect(&copyValue, &QAction::triggered, [tree] { tree->copySelection(); });

    RDTreeWidgetItem *item = tree->selectedItem();

    VariableTag tag;
    if(item)
      tag = item->tag().value<VariableTag>();

    // we can add any selected item as a watch that has a path to reference
    addWatch.setEnabled(item != NULL && !tag.absoluteRefPath.empty());

    QObject::connect(&addWatch, &QAction::triggered, [this, tree] {
      RDTreeWidgetItem *item = tree->selectedItem();
      VariableTag tag = item->tag().value<VariableTag>();
      if(!tag.absoluteRefPath.empty())
        AddWatch(tag.absoluteRefPath);
      else
        AddWatch(item->text(0));
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
      runTo(m_States[tag.step].nextInstruction, forward);
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
      if(getVarFromPath(text))
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

  m_Ctx.Replay().AsyncInvoke([me, this, targetStr](IReplayController *r) {
    if(!me)
      return;

    rdcstr disasm = r->DisassembleShader(m_Pipeline, m_ShaderDetails, targetStr);

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
    int idx = ui->watch->indexOfTopLevelItem(ui->watch->selectedItem());

    if(idx >= 0 && idx < ui->watch->topLevelItemCount() - 1)
    {
      RDTreeViewExpansionState expansion;
      ui->watch->saveExpansion(expansion, 0);

      ui->watch->beginUpdate();

      delete ui->watch->takeTopLevelItem(idx);

      ui->watch->endUpdate();

      ui->watch->applyExpansion(expansion, 0);
    }
  }
}

void ShaderViewer::on_watch_itemChanged(RDTreeWidgetItem *item, int column)
{
  // ignore changes to the type/value columns. Only look at name changes, which must be by the user
  if(column != 0)
    return;

  QSignalBlocker block(ui->watch);

  VariableTag tag = item->tag().value<VariableTag>();
  tag.state = WatchVarState::Invalid;
  item->setTag(QVariant::fromValue(tag));

  // remove any children
  item->clear();

  // if the item is now empty, remove it. Only top-level items are editable so this must be one
  if(item->text(0).isEmpty())
    delete ui->watch->takeTopLevelItem(ui->watch->indexOfTopLevelItem(item));

  // ensure we have a trailing row for adding new watch items.

  if(ui->watch->topLevelItemCount() == 0 ||
     !ui->watch->topLevelItem(ui->watch->topLevelItemCount() - 1)->text(0).isEmpty())
  {
    RDTreeWidgetItem *blankItem = new RDTreeWidgetItem({
        QVariant(),
        QVariant(),
        QVariant(),
        QVariant(),
    });
    blankItem->setEditable(0, true);
    ui->watch->addTopLevelItem(blankItem);
  }

  updateDebugState();
}

bool ShaderViewer::step(bool forward, StepMode mode)
{
  if(!m_Trace || m_States.empty())
    return false;

  if((forward && IsLastState()) || (!forward && IsFirstState()))
    return false;

  // also stop if we reach the first real source-mapped instruction, while source debugging
  if(!forward && isSourceDebugging() && m_CurrentStateIdx == m_FirstSourceStateIdx)
    return false;

  m_VariablesChanged.clear();

  if(isSourceDebugging())
  {
    LineColumnInfo oldLine = GetCurrentInstInfo().lineInfo;
    rdcarray<rdcstr> oldStack = GetCurrentState().callstack;

    do
    {
      // step once in the right direction
      if(forward)
        applyForwardsChange();
      else
        applyBackwardsChange();

      LineColumnInfo curLine = GetCurrentLineInfo();

      // break out if we hit a breakpoint, no matter what
      if(m_Breakpoints.contains({-1, curLine.disassemblyLine}) ||
         m_Breakpoints.contains({curLine.fileIndex, curLine.lineStart}))
        break;

      // if we've reached the limit, break
      if((forward && IsLastState()) || (!forward && IsFirstState()))
        break;

      // also stop if we reach the first real source-mapped instruction, while source debugging
      if(!forward && isSourceDebugging() && m_CurrentStateIdx == m_FirstSourceStateIdx)
        break;

      // keep going if we're still on the same source line as we started
      if(curLine.SourceEqual(oldLine))
        continue;

      // if we're in an invalid file, skip it as unmapped instructions
      if(curLine.fileIndex == -1)
        continue;

      // we're on a different line so we can probably stop, but that might not be enough for Step
      // Out or Step Over
      rdcarray<rdcstr> curStack = GetCurrentState().callstack;

      // first/last instruction may not have a stack - treat any change from no stack to stack as
      // hitting the step condition
      if(oldStack.empty() || curStack.empty())
        break;

      // if we're stepping into, any line different from the previous is sufficient to stop stepping
      if(mode == StepInto)
        break;

      // if the stack has shrunk we must have exited the function, don't continue stepping
      if(curStack.size() < oldStack.size())
        break;

      // if the stack is identical we haven't left this function (to our knowledge. We can't
      // differentiate between inlining causing us to immediately jump back into this function, a
      // kind of A-B-A problem, but there's not much we can do about that here.
      if(curStack == oldStack)
      {
        // if we're stepping over, we can stop stepping now as we've gotten to a different line in
        // the same function without going into a new one
        if(mode == StepOver)
          break;

        // if we're stepping out keep going - we want to leave this function.
        if(mode == StepOut)
          continue;
      }

      // if the stack common subset is different, we have stepped into a different function due to
      // inlining, so stop stepping. This covers the case where StepOut hasn't seen a stack shrink
      // yet as well as StepOver where the stack has grown but now has a different root from when we
      // started.
      //
      // E.g. A() -> B() StepOver A() -> C() -> D()
      //
      // Or A() -> B() StepOut A() -> C()
      bool different = false;
      for(size_t i = 0; i < qMin(curStack.size(), oldStack.size()); i++)
      {
        if(oldStack[i] != curStack[i])
        {
          different = true;
          break;
        }
      }

      if(different)
        break;

      // otherwise we either went into a bigger callstack and continue stepping over it, or else we
      // haven't yet stepped out of the callstack that we started in

    } while(true);

    oldLine = GetCurrentLineInfo();
    oldStack = GetCurrentState().callstack;

    if(!forward)
    {
      // ignore disassembly line for comparison in map below
      oldLine.disassemblyLine = 0;

      // now since a line can have multiple instructions, we may only be on the last one of several
      // instructions that map to this source location.
      // Keep stepping until we reach the first instruction this line info
      // We only do this when stepping over, otherwise we could miss a backwards step-into when the
      // same source line is mapped before and after a function call. For step-into we go strictly
      // by contiguous sets of instructions with the same source mapping
      if(m_Location2Inst.contains(oldLine) && mode == StepOver)
      {
        const rdcarray<uint32_t> &targetInsts = m_Location2Inst[oldLine];

        // keep going until we hit the closest instruction of any block that maps to this function
        while(!IsFirstState() && !targetInsts.contains(GetCurrentState().nextInstruction))
        {
          const rdcarray<rdcstr> &prevStack = GetPreviousState().callstack;
          if((oldStack == prevStack || (!prevStack.empty() && oldStack.size() > prevStack.size())) &&
             !oldLine.SourceEqual(GetPreviousInstInfo().lineInfo))
          {
            // if we hit this case, it means we jumped to an instruction in the same call which maps
            // to a different line (perhaps through a loop or branch), or we lost a function in the
            // callstack. In either event, the contiguous group of instructions we were trying to
            // step over is not so contiguous so don't move back any further.
            break;
          }

          applyBackwardsChange();

          LineColumnInfo curLine = GetCurrentLineInfo();

          // still need to check for instruction-level breakpoints
          if(m_Breakpoints.contains({-1, curLine.disassemblyLine}))
            break;
        }
      }
      else
      {
        while(!IsFirstState() && GetPreviousInstInfo().lineInfo.SourceEqual(oldLine))
        {
          applyBackwardsChange();

          LineColumnInfo curLine = GetCurrentLineInfo();

          // still need to check for instruction-level breakpoints
          if(m_Breakpoints.contains({-1, curLine.disassemblyLine}))
            break;
        }
      }
    }

    updateDebugState();
  }
  else
  {
    // non-source stepping is easy, we just do one instruction in that direction regardless of step
    // mode

    if(forward)
      applyForwardsChange();
    else
      applyBackwardsChange();
    updateDebugState();
  }

  return true;
}

void ShaderViewer::runToCursor(bool forward)
{
  if(!m_Trace || m_States.empty())
    return;

  // don't update the UI or remove any breakpoints
  m_TempBreakpoint = true;
  QSet<QPair<int, uint32_t>> oldBPs = m_Breakpoints;

  // add a temporary breakpoint on the current instruction
  ToggleBreakpointOnInstruction(-1);

  // run in the direction
  runTo(~0U, forward);

  // turn off temp breakpoint state
  m_TempBreakpoint = false;
  // restore the set of breakpoints to what it was (this handles the case of doing 'run to cursor'
  // on a line that already has a breakpoint)
  m_Breakpoints = oldBPs;
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

const InstructionSourceInfo &ShaderViewer::GetPreviousInstInfo() const
{
  return GetInstInfo(GetPreviousState().nextInstruction);
}

const InstructionSourceInfo &ShaderViewer::GetCurrentInstInfo() const
{
  return GetInstInfo(GetCurrentState().nextInstruction);
}

const InstructionSourceInfo &ShaderViewer::GetNextInstInfo() const
{
  return GetInstInfo(GetNextState().nextInstruction);
}

const InstructionSourceInfo &ShaderViewer::GetInstInfo(uint32_t instruction) const
{
  InstructionSourceInfo search;
  search.instruction = instruction;
  auto it = std::lower_bound(m_Trace->instInfo.begin(), m_Trace->instInfo.end(), search);
  if(it == m_Trace->instInfo.end())
  {
    qCritical() << "Couldn't find instruction info for" << instruction;
    return m_Trace->instInfo[0];
  }

  return *it;
}

const LineColumnInfo &ShaderViewer::GetCurrentLineInfo() const
{
  return GetCurrentInstInfo().lineInfo;
}

void ShaderViewer::runTo(uint32_t runToInstruction, bool forward, ShaderEvents condition)
{
  rdcarray<uint32_t> insts = {runToInstruction};
  runTo(insts, forward, condition);
}

void ShaderViewer::runTo(const rdcarray<uint32_t> &runToInstructions, bool forward,
                         ShaderEvents condition)
{
  if(!m_Trace || m_States.empty())
    return;

  m_VariablesChanged.clear();

  bool firstStep = true;
  LineColumnInfo oldLine = GetCurrentLineInfo();

  // this is effectively infinite as we break out before moving to next/previous state if that would
  // be first/last
  while((forward && !IsLastState()) || (!forward && !IsFirstState()))
  {
    // break immediately even on the very first step if it's the one we want to go to
    if(runToInstructions.contains(GetCurrentState().nextInstruction))
      break;

    // after the first step, break on condition
    if(!firstStep && (GetCurrentState().flags & condition))
      break;

    // or breakpoint
    LineColumnInfo curLine = GetCurrentLineInfo();
    if(!firstStep && (m_Breakpoints.contains({-1, curLine.disassemblyLine}) ||
                      m_Breakpoints.contains({curLine.fileIndex, curLine.lineStart})))
      break;

    if(isSourceDebugging())
    {
      if(!curLine.SourceEqual(oldLine))
        firstStep = false;
    }
    else
    {
      firstStep = false;
    }

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
      // also stop if we reach the first real source-mapped instruction, while source debugging
      if(isSourceDebugging() && m_CurrentStateIdx == m_FirstSourceStateIdx)
        break;
      applyBackwardsChange();
    }
  }

  updateDebugState();
}

void ShaderViewer::runToResourceAccess(bool forward, VarType type, const ShaderBindIndex &resource)
{
  if(!m_Trace || m_States.empty())
    return;

  m_VariablesChanged.clear();

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
      if(c.after.type == type && c.after.GetBindIndex() == resource)
      {
        foundResource = true;
        break;
      }
    }

    if(foundResource)
      break;

    // or breakpoint
    LineColumnInfo curLine = GetCurrentLineInfo();
    if(m_Breakpoints.contains({-1, curLine.disassemblyLine}) ||
       m_Breakpoints.contains({curLine.fileIndex, curLine.lineStart}))
      break;
  }

  updateDebugState();
}

void ShaderViewer::applyBackwardsChange()
{
  if(IsFirstState())
    return;

  for(const ShaderVariableChange &c : GetCurrentState().changes)
  {
    // if the before name is empty, this is a variable that came into scope/was created
    if(c.before.name.empty())
    {
      m_VariablesChanged.push_back(c.after.name);
      m_VariableLastUpdate[c.after.name] = m_UpdateID;

      // delete the matching variable (should only be one)
      for(int i = 0; i < m_Variables.count(); i++)
      {
        if(c.after.name == m_Variables[i].name)
        {
          m_Variables.removeAt(i);
          break;
        }
      }
    }
    else
    {
      m_VariablesChanged.push_back(c.before.name);
      m_VariableLastUpdate[c.before.name] = m_UpdateID;

      ShaderVariable *v = NULL;
      for(int i = 0; i < m_Variables.count(); i++)
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
        m_Variables.insert(0, c.before);
    }
  }

  m_CurrentStateIdx--;

  m_UpdateID++;
}

void ShaderViewer::applyForwardsChange()
{
  if(IsLastState())
    return;

  m_CurrentStateIdx++;

  rdcarray<AccessedResourceData> newAccessedResources;

  for(const ShaderVariableChange &c : GetCurrentState().changes)
  {
    // if the after name is empty, this is a variable going out of scope/being deleted
    if(c.after.name.empty())
    {
      m_VariablesChanged.push_back(c.before.name);
      m_VariableLastUpdate[c.before.name] = m_UpdateID;

      // delete the matching variable (should only be one)
      for(int i = 0; i < m_Variables.count(); i++)
      {
        if(c.before.name == m_Variables[i].name)
        {
          m_Variables.removeAt(i);
          break;
        }
      }
    }
    else
    {
      m_VariablesChanged.push_back(c.after.name);
      m_VariableLastUpdate[c.after.name] = m_UpdateID;

      ShaderVariable *v = NULL;
      for(int i = 0; i < m_Variables.count(); i++)
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
        m_Variables.insert(0, c.after);

      if(c.after.type == VarType::ReadOnlyResource || c.after.type == VarType::ReadWriteResource)
      {
        bool found = false;
        for(size_t i = 0; i < m_AccessedResources.size(); i++)
        {
          if(c.after.GetBindIndex() == m_AccessedResources[i].resource.GetBindIndex())
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

  m_AccessedResources.insert(0, newAccessedResources);

  m_UpdateID++;
}

QString ShaderViewer::stringRep(const ShaderVariable &var, uint32_t row)
{
  VarType type = var.type;

  if(type == VarType::Unknown)
    type = ui->intView->isChecked() ? VarType::SInt : VarType::Float;

  if(type == VarType::ReadOnlyResource || type == VarType::ReadWriteResource ||
     type == VarType::Sampler)
  {
    ShaderBindIndex varBind = var.GetBindIndex();

    rdcarray<UsedDescriptor> resList;

    if(type == VarType::ReadOnlyResource)
      resList = m_ReadOnlyResources;
    else if(type == VarType::ReadWriteResource)
      resList = m_ReadWriteResources;
    else if(type == VarType::Sampler)
      resList = m_Ctx.CurPipelineState().GetSamplers(m_Stage);

    int32_t bindIdx = resList.indexOf(varBind);

    if(bindIdx < 0)
      return QString();

    const UsedDescriptor &res = resList[bindIdx];

    if(type == VarType::Sampler)
      return samplerRep(m_ShaderDetails->samplers[varBind.index], varBind.arrayElement,
                        res.descriptor.resource);
    return ToQStr(res.descriptor.resource);
  }

  return RowString(var, row, type);
}

QString ShaderViewer::samplerRep(const ShaderSampler &samp, uint32_t arrayElement, ResourceId id)
{
  if(id == ResourceId())
  {
    QString contents;
    if(samp.fixedBindSetOrSpace > 0)
    {
      // a bit ugly to do an API-specific switch here but we don't have a better way to refer
      // by binding
      contents = IsD3D(m_Ctx.APIProps().pipelineType) ? tr("space%1, ") : tr("Set %1, ");
      contents = contents.arg(samp.fixedBindSetOrSpace);
    }

    if(arrayElement == ~0U || samp.bindArraySize == 1)
      contents += QString::number(samp.fixedBindNumber);
    else
      contents += QFormatStr("%1[%2]").arg(samp.fixedBindNumber).arg(arrayElement);

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
                return a->text(0) < b->text(0);
              });

    // create a new parent with just the prefix
    prefix.chop(1);
    QVariantList values = {prefix};
    for(int i = 1; i < child->dataCount(); i++)
      values.push_back(QVariant());
    RDTreeWidgetItem *parent = new RDTreeWidgetItem(values);

    VariableTag tag;
    tag.absoluteRefPath = prefix;
    parent->setTag(QVariant::fromValue(tag));

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

QString ShaderViewer::getRegNames(const RDTreeWidgetItem *item, uint32_t swizzle, uint32_t child)
{
  VariableTag tag = item->tag().value<VariableTag>();

  if(tag.absoluteRefPath.empty())
    return QString();

  if(tag.debugVarType != DebugVariableType::Undefined)
  {
    DebugVariableReference debugVar;
    debugVar.type = tag.debugVarType;
    debugVar.name = tag.absoluteRefPath;

    const ShaderVariable *reg = GetDebugVariable(debugVar);

    return reg->name;
  }

  SourceVariableMapping mapping;

  VariableTag itemTag = tag;

  // if this an expanded node (when a single source variable maps to a complex struct/array)
  // search up to the nearest parent which isn't
  const RDTreeWidgetItem *cur = item;
  while(tag.expanded)
  {
    cur = cur->parent();
    tag = cur->tag().value<VariableTag>();
  }

  // check if it's a local or global source var and look up the mapping
  if(tag.globalSourceVar && tag.sourceVarIdx >= 0 && tag.sourceVarIdx < m_Trace->sourceVars.count())
  {
    mapping = m_Trace->sourceVars[tag.sourceVarIdx];
  }
  else if(!tag.globalSourceVar && tag.sourceVarIdx >= 0 &&
          tag.sourceVarIdx < GetCurrentInstInfo().sourceVars.count())
  {
    mapping = GetCurrentInstInfo().sourceVars[tag.sourceVarIdx];
  }
  else
  {
    // if there's no source mapping, we assume this is a root node of a struct, which has no reg
    // names
    return QString();
  }

  QString ret;

  // if we have a 'tail' referencing values inside this mapping, need to evaluate that
  if(cur != item)
  {
    // we don't support combining complex structs from multiple places, we should only have one
    // mapping here
    if(mapping.variables.size() != 1)
      return QString();

    const ShaderVariable *reg = GetDebugVariable(mapping.variables[0]);

    // return the base name with the suffix/tail that we have
    ret = reg->name + itemTag.absoluteRefPath.substr(tag.absoluteRefPath.size());

    if(child < 4)
      ret += lit(".row%1").arg(child);

    return ret;
  }

  if(mapping.type == VarType::Sampler || mapping.type == VarType::ReadOnlyResource ||
     mapping.type == VarType::ReadWriteResource)
  {
    const ShaderVariable *reg = GetDebugVariable(mapping.variables[0]);

    ret = reg->name;

    if(mapping.type == VarType::Sampler)
    {
      ShaderBindIndex bind = reg->GetBindIndex();

      if(bind.category != DescriptorCategory::Sampler)
        return QString();

      if(bind.index >= m_ShaderDetails->samplers.size())
        return QString();

      const ShaderSampler &samp = m_ShaderDetails->samplers[bind.index];

      if(samp.bindArraySize == ~0U)
        return ret + lit("[unbounded]");

      if(samp.bindArraySize > 1 && child != ~0U)
        return QFormatStr("%1[%2]").arg(ret).arg(child);

      return ret;
    }
    else if(mapping.type == VarType::ReadOnlyResource || mapping.type == VarType::ReadWriteResource)
    {
      ShaderBindIndex bind = reg->GetBindIndex();

      if((mapping.type == VarType::ReadOnlyResource &&
          bind.category != DescriptorCategory::ReadOnlyResource) ||
         (mapping.type == VarType::ReadWriteResource &&
          bind.category != DescriptorCategory::ReadWriteResource))
        return QString();

      if((mapping.type == VarType::ReadOnlyResource &&
          bind.index >= m_ShaderDetails->readOnlyResources.size()) ||
         (mapping.type == VarType::ReadWriteResource &&
          bind.index >= m_ShaderDetails->readWriteResources.size()))
        return QString();

      const ShaderResource &res = mapping.type == VarType::ReadOnlyResource
                                      ? m_ShaderDetails->readOnlyResources[bind.index]
                                      : m_ShaderDetails->readWriteResources[bind.index];

      if(res.bindArraySize == ~0U)
        return ret + lit("[unbounded]");

      if(res.bindArraySize > 1 && child != ~0U)
        return QFormatStr("%1[%2]").arg(ret).arg(child);

      return ret;
    }

    return ret;
  }

  uint32_t start = 0;
  uint32_t count = (uint32_t)mapping.variables.size();

  if(mapping.rows > 1 && mapping.variables.size() > mapping.columns)
  {
    count = mapping.columns;
    if(child < mapping.rows)
      start += count * child;

    swizzle = ~0U;
  }

  DebugVariableReference prevRef;

  const QString xyzw = lit("xyzw");

  for(uint32_t i = start; i < start + count; i++)
  {
    uint32_t swiz_i = i;

    if(swizzle != ~0U)
    {
      swiz_i = (swizzle >> (i * 8)) & 0xff;

      if(swiz_i == 0xff)
      {
        // swizzle has finished, truncate
        break;
      }
    }

    const DebugVariableReference &r = mapping.variables[swiz_i];

    if(!ret.isEmpty())
      ret += lit(", ");

    if(r.name.empty())
    {
      ret += lit("-");
    }
    else
    {
      const ShaderVariable *reg = GetDebugVariable(r);

      if(reg)
      {
        if(!reg->members.empty())
          return QString();

        if(i > start && r.name == prevRef.name &&
           (r.component / reg->columns) == (prevRef.component / reg->columns))
        {
          // if the previous register was the same, just append our component
          // remove the auto-appended ", " - there must be one because this isn't the first
          // register
          ret.chop(2);
          ret += xyzw[r.component % 4];
        }
        else
        {
          if(reg->rows > 1)
            ret += QFormatStr("%1.row%2.%3")
                       .arg(reg->name)
                       .arg(r.component / reg->columns)
                       .arg(xyzw[r.component % 4]);
          else
            ret += QFormatStr("%1.%2").arg(r.name).arg(xyzw[r.component % 4]);
        }
      }
      else
      {
        ret += lit("-");
      }
    }

    prevRef = r;
  }

  return ret;
}

const RDTreeWidgetItem *ShaderViewer::evaluateVar(const RDTreeWidgetItem *item, uint32_t swizzle,
                                                  ShaderVariable *var)
{
  VariableTag tag = item->tag().value<VariableTag>();

  // if the tag is invalid, it's not a proper match
  if(tag.absoluteRefPath.empty())
    return NULL;

  // if we have a debug var tag then it's easy-mode
  if(tag.debugVarType != DebugVariableType::Undefined)
  {
    // found a match. If we don't want the variable contents, just return true now
    if(!var)
      return item;

    DebugVariableReference debugVar;
    debugVar.type = tag.debugVarType;
    debugVar.name = tag.absoluteRefPath;

    const ShaderVariable *reg = GetDebugVariable(debugVar);

    if(reg)
    {
      *var = *reg;
      var->name = debugVar.name;

      if(swizzle != ~0U)
      {
        // only support swizzles if the debug variable is a plain vector or scalar
        if(!reg->members.empty() || reg->rows > 1)
          return NULL;

        var->value = ShaderValue();

        size_t compSize = VarTypeByteSize(var->type);
        for(uint32_t i = 0; i < 4; i++)
        {
          uint8_t sw = (swizzle >> (i * 8)) & 0xff;

          if(sw == 0xff)
          {
            // swizzle has finished, truncate
            break;
          }
          else if(sw < reg->columns)
          {
            var->columns = i + 1;

            memcpy(var->value.u8v.data() + compSize * i, reg->value.u8v.data() + compSize * sw,
                   compSize);
          }
          else
          {
            return NULL;
          }
        }
      }
    }
    else
    {
      qCritical() << "Couldn't find expected debug variable!" << ToQStr(debugVar.type)
                  << QString(debugVar.name);
      return NULL;
    }

    return item;
  }
  else
  {
    // found a match. If we don't want the variable contents, just return true now
    if(!var)
      return item;

    SourceVariableMapping mapping;

    VariableTag itemTag = tag;

    // if this an expanded node (when a single source variable maps to a complex struct/array)
    // search up to the nearest parent which isn't
    const RDTreeWidgetItem *cur = item;
    while(tag.expanded)
    {
      cur = cur->parent();
      tag = cur->tag().value<VariableTag>();
    }

    // check if it's a local or global source var and look up the mapping
    if(tag.globalSourceVar && tag.sourceVarIdx >= 0 && tag.sourceVarIdx < m_Trace->sourceVars.count())
    {
      mapping = m_Trace->sourceVars[tag.sourceVarIdx];
    }
    else if(!tag.globalSourceVar && tag.sourceVarIdx >= 0 &&
            tag.sourceVarIdx < GetCurrentInstInfo().sourceVars.count())
    {
      mapping = GetCurrentInstInfo().sourceVars[tag.sourceVarIdx];
    }
    else
    {
      // if there's no source mapping, we assume this is a root node of a struct, so build the
      // ShaderVariable that way. We should not have encountered any expanded nodes, and we should
      // have children
      if(cur != item)
        return NULL;

      if(item->childCount() == 0)
        return NULL;

      ShaderVariable &ret = *var;
      ret.name = item->text(0);

      for(int i = 0; i < item->childCount(); i++)
      {
        ret.members.push_back(ShaderVariable());
        if(!evaluateVar(item->child(i), ~0U, &ret.members.back()))
          return NULL;
      }

      return item;
    }

    if(mapping.variables.empty())
      return NULL;

    // if we have a 'tail' referencing values inside this mapping, need to evaluate that
    if(cur != item)
    {
      // we don't support combining complex structs from multiple places, we should only have one
      // mapping here
      if(mapping.variables.size() != 1)
        return NULL;

      DebugVariableReference ref = mapping.variables[0];

      // append on our suffix
      ref.name += itemTag.absoluteRefPath.substr(tag.absoluteRefPath.size());
      ref.component = 0;

      mapping.variables.clear();

      const ShaderVariable *reg = GetDebugVariable(ref);

      // expect to find the variable
      if(!reg)
        return NULL;

      // update the mapping
      mapping.name = reg->name;
      mapping.rows = reg->rows;
      mapping.columns = reg->columns;
      mapping.type = reg->type;

      // add a mapping for each component in the resulting variable referenced. Swizzles are handled
      // separately
      for(uint8_t c = 0; c < std::max(1, reg->rows * reg->columns); c++)
      {
        ref.component = c;
        mapping.variables.push_back(ref);
      }
    }

    ShaderVariable &ret = *var;
    ret.name = mapping.name;
    ret.flags |= ShaderVariableFlags::RowMajorMatrix;
    ret.rows = mapping.rows;
    ret.columns = mapping.columns;
    ret.type = mapping.type;

    size_t dataSize = VarTypeByteSize(ret.type);
    if(dataSize == 0)
      dataSize = 4;

    if(ret.type == VarType::Sampler || ret.type == VarType::ReadOnlyResource ||
       ret.type == VarType::ReadWriteResource)
      dataSize = 16;

    // only support swizzling on vectors
    if(swizzle != ~0U && (ret.rows > 1 || mapping.variables.size() > 4))
      return NULL;

    DebugVariableReference prevRef;

    for(uint32_t i = 0; i < mapping.variables.size(); i++)
    {
      uint32_t swiz_i = i;

      if(swizzle != ~0U)
      {
        swiz_i = (swizzle >> (i * 8)) & 0xff;

        if(swiz_i == 0xff)
        {
          // swizzle has finished, truncate
          break;
        }
        else if(swiz_i < mapping.variables.size())
        {
          ret.columns = i + 1;
        }
      }

      const DebugVariableReference &r = mapping.variables[swiz_i];

      const ShaderVariable *reg = GetDebugVariable(r);

      if(reg)
      {
        if(!reg->members.empty())
        {
          ret.members = reg->members;
          if(mapping.variables.size() != 1)
            return NULL;
          break;
        }

        if(dataSize == 16)
          ret.value = reg->value;
        else if(dataSize == 8)
          ret.value.u64v[i] = reg->value.u64v[r.component];
        else if(dataSize == 4)
          ret.value.u32v[i] = reg->value.u32v[r.component];
        else if(dataSize == 2)
          ret.value.u16v[i] = reg->value.u16v[r.component];
        else
          ret.value.u8v[i] = reg->value.u8v[r.component];
      }

      prevRef = r;
    }

    return item;
  }
}

const RDTreeWidgetItem *ShaderViewer::getVarFromPath(const rdcstr &path, const RDTreeWidgetItem *root,
                                                     ShaderVariable *var, uint32_t *swizzlePtr)
{
  VariableTag tag = root->tag().value<VariableTag>();

  // if the path is an exact match, return the evaluation directly
  if(tag.absoluteRefPath == path)
  {
    return evaluateVar(root, ~0U, var);
  }

  for(int i = 0; i < root->childCount(); i++)
  {
    RDTreeWidgetItem *child = root->child(i);

    tag = child->tag().value<VariableTag>();

    // if this child has a longer path, it can't be what we're looking for
    if(tag.absoluteRefPath.size() > path.size())
      continue;

    // if the path is an exact match, return the evaluation directly
    if(tag.absoluteRefPath == path)
    {
      return evaluateVar(child, ~0U, var);
    }

    // after the common prefix, if the next value is . or [ then this is the next child, so recurse.
    // it can't be any other child since we don't support multiple members with the same name, so if
    // this recursion fails there is no better option
    // we know it's not an exact match (or the path would have been identical above) but the
    // recursion will handle any trailing swizzle
    rdcstr common = path.substr(0, tag.absoluteRefPath.size());
    if(common == tag.absoluteRefPath &&
       (path[tag.absoluteRefPath.size()] == '.' || path[tag.absoluteRefPath.size()] == '['))
    {
      return getVarFromPath(path, child, var, swizzlePtr);
    }
  }

  if(root->childCount() == 0)
  {
    // if there are no children but we got here instead of earlying out elsewhere, there might be a
    // trailing swizzle
    QRegularExpression swizzleRE(lit("^(.*)\\.([xyzwrgba][xyzwrgba]?[xyzwrgba]?[xyzwrgba]?)$"));

    QRegularExpressionMatch match = swizzleRE.match(path);

    // if we exactly match without the swizzle, we can evaluate this node
    if(match.hasMatch() && QString(tag.absoluteRefPath) == match.captured(1))
    {
      QString swizzle = match.captured(2);
      uint32_t swizzleMask = 0;

      int s = 0;
      for(; s < swizzle.count(); s++)
      {
        switch(swizzle[s].toLatin1())
        {
          case 'x':
          case 'r': swizzleMask |= (0x00U << (s * 8)); break;
          case 'y':
          case 'g': swizzleMask |= (0x01U << (s * 8)); break;
          case 'z':
          case 'b': swizzleMask |= (0x02U << (s * 8)); break;
          case 'w':
          case 'a': swizzleMask |= (0x03U << (s * 8)); break;
          default: return NULL;
        }
      }
      for(; s < 4; s++)
      {
        swizzleMask |= (0xffU << (s * 8));
      }

      if(swizzlePtr)
        *swizzlePtr = swizzleMask;

      return evaluateVar(root, swizzleMask, var);
    }
  }

  return NULL;
}

const RDTreeWidgetItem *ShaderViewer::getVarFromPath(const rdcstr &path, ShaderVariable *var,
                                                     uint32_t *swizzle)
{
  if(!m_Trace || m_States.empty())
    return NULL;

  // prioritise source mapped variables, in the event that source vars have the same name as debug
  // vars we want to prioritise source vars. After that look through constants (some/all of which
  // may be source mapped as well) before searching debug vars
  RDTreeWidget *widgets[] = {ui->sourceVars, ui->constants, ui->debugVars};

  rdcstr root;
  int idx = path.find_first_of("[.");
  if(idx > 0)
    root = path.substr(0, idx);
  else
    root = path;

  // we do a 'breadth first' type search. First look for any direct descendents which match the
  // first part of the path we're looking for. If that doesn't find anything, look for any children
  // of the top level items that match. This fixes issues with e.g. constant buffer names where they
  // aren't actually namespaced
  for(int pass = 0; pass < 2; pass++)
  {
    for(RDTreeWidget *w : widgets)
    {
      for(int i = 0; i < w->topLevelItemCount(); i++)
      {
        RDTreeWidgetItem *item = w->topLevelItem(i);

        if(item->text(0) == root)
        {
          const RDTreeWidgetItem *ret = getVarFromPath(path, item, var, swizzle);
          if(ret)
            return ret;
        }

        if(pass == 1)
        {
          for(int j = 0; j < item->childCount(); j++)
          {
            RDTreeWidgetItem *child = item->child(j);

            if(child->text(0) == root)
            {
              VariableTag tag = item->tag().value<VariableTag>();

              const RDTreeWidgetItem *ret =
                  getVarFromPath(tag.absoluteRefPath + "." + path, child, var, swizzle);
              if(ret)
                return ret;
            }
          }
        }
      }
    }
  }

  return NULL;
}

void ShaderViewer::updateEditState()
{
  if(m_EditingShader != ResourceId())
  {
    QString statusString, statusTooltip;

    // check if the shader is replaced and update the status
    if(m_Ctx.IsResourceReplaced(m_EditingShader))
    {
      statusString = tr("Status: Edited Shader Active");
      statusTooltip = tr("The replay currently has the original shader active");
    }
    else
    {
      statusString = tr("Status: Original Shader Active");
      statusTooltip = tr("The replay currently has an edited version of the shader active");

      // if we expected it to be saved something went wrong
      if(m_Saved)
      {
        // we're still 'modified' as in have unsaved changes
        m_Modified = true;

        statusTooltip.append(tr("\n\nSomething went wrong applying changes."));
      }
    }

    ui->editStatusLabel->setText(statusString);
    ui->editStatusLabel->setToolTip(statusTooltip);

    if(m_Modified)
    {
      QString title = windowTitle();
      if(title[0] != QLatin1Char('*'))
        title.prepend(lit("* "));
      setWindowTitle(title);
    }
    else
    {
      QString title = windowTitle();
      if(title[0] == QLatin1Char('*'))
        title.remove(0, 2);
      setWindowTitle(title);
    }
  }
}

void ShaderViewer::highlightMatchingVars(RDTreeWidgetItem *root, const QString varName,
                                         const QColor highlightColor)
{
  for(int i = 0; i < root->childCount(); i++)
  {
    RDTreeWidgetItem *item = root->child(i);
    if(item->tag().value<VariableTag>().absoluteRefPath == rdcstr(varName))
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

        RDTreeWidgetItem *resourceNode = makeAccessedResourceNode(m_AccessedResources[i].resource);
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
                makeAccessedResourceNode(m_AccessedResources[i].resource);

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

  {
    LineColumnInfo lineInfo = GetInstInfo(state.nextInstruction).lineInfo;

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

    if(IsLastState() && (lineInfo.fileIndex < 0 || lineInfo.fileIndex >= m_FileScintillas.count()))
    {
      // if the last state doesn't have source mapping information, display the line info for the
      // last state which did.
      for(int stateLookbackIdx = (int)m_CurrentStateIdx; stateLookbackIdx > 0; stateLookbackIdx--)
      {
        lineInfo = GetInstInfo(m_States[stateLookbackIdx].nextInstruction).lineInfo;

        if(lineInfo.fileIndex >= 0 && lineInfo.fileIndex < m_FileScintillas.count())
          break;
      }
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

        if(isSourceDebugging() || ui->docking->areaOf(m_CurInstructionScintilla) !=
                                      ui->docking->areaOf(m_DisassemblyFrame))
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

      fakeroot.addChild(makeSourceVariableNode(sourceVar, globalVarIdx, -1));
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
      node->setTag(QVariant::fromValue(VariableTag(DebugVariableType::Constant, name)));

      for(int j = 0; j < m_Trace->constantBlocks[i].members.count(); j++)
      {
        if(m_Trace->constantBlocks[i].members[j].rows > 0 ||
           m_Trace->constantBlocks[i].members[j].columns > 0)
        {
          rdcstr childname = name + "." + m_Trace->constantBlocks[i].members[j].name;
          if(!varsMapped.contains(name))
          {
            RDTreeWidgetItem *child = new RDTreeWidgetItem(
                {name, name, lit("Constant"), stringRep(m_Trace->constantBlocks[i].members[j])});
            child->setTag(QVariant::fromValue(VariableTag(DebugVariableType::Constant, childname)));
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
              rdcstr childname = name + "." + m_Trace->constantBlocks[i].members[j].members[k].name;
              RDTreeWidgetItem *child =
                  new RDTreeWidgetItem({name, name, lit("Constant"),
                                        stringRep(m_Trace->constantBlocks[i].members[j].members[k])});
              node->setTag(QVariant::fromValue(VariableTag(DebugVariableType::Constant, childname)));

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
        node->setTag(QVariant::fromValue(VariableTag(DebugVariableType::Input, input.name)));

        ui->constants->addTopLevelItem(node);
      }
    }

    rdcarray<UsedDescriptor> &roBinds = m_ReadOnlyResources;

    for(int i = 0; i < m_Trace->readOnlyResources.count(); i++)
    {
      const ShaderVariable &ro = m_Trace->readOnlyResources[i];

      if(varsMapped.contains(ro.name))
        continue;

      const rdcarray<UsedDescriptor> &resList = m_ReadOnlyResources;

      // find all descriptors in this bind's array
      ShaderBindIndex bind = ro.GetBindIndex();
      bind.arrayElement = 0;

      rdcarray<UsedDescriptor> descriptors;
      for(const UsedDescriptor &a : resList)
        if(CategoryForDescriptorType(a.access.type) == bind.category && a.access.index == bind.index)
          descriptors.push_back(a);

      if(descriptors.empty())
        continue;

      const ShaderResource &res = m_ShaderDetails->readOnlyResources[bind.index];

      if(res.bindArraySize == 1)
      {
        RDTreeWidgetItem *node = new RDTreeWidgetItem(
            {res.name, ro.name, lit("Resource"), ToQStr(descriptors[0].descriptor.resource)});
        node->setTag(QVariant::fromValue(VariableTag(DebugVariableType::ReadOnlyResource, ro.name)));
        ui->constants->addTopLevelItem(node);
      }
      else if(res.bindArraySize == ~0U)
      {
        RDTreeWidgetItem *node =
            new RDTreeWidgetItem({res.name, ro.name, lit("[unbounded]"), QString()});
        node->setTag(QVariant::fromValue(VariableTag(DebugVariableType::ReadOnlyResource, ro.name)));
        ui->constants->addTopLevelItem(node);
      }
      else
      {
        RDTreeWidgetItem *node = new RDTreeWidgetItem(
            {res.name, ro.name, QFormatStr("[%1]").arg(res.bindArraySize), QString()});
        node->setTag(QVariant::fromValue(VariableTag(DebugVariableType::ReadOnlyResource, ro.name)));

        uint32_t count = qMin(res.bindArraySize, (uint32_t)descriptors.size());
        for(uint32_t a = 0; a < count; a++)
        {
          QString childName = QFormatStr("%1[%2]").arg(ro.name).arg(a);
          RDTreeWidgetItem *child = new RDTreeWidgetItem({
              QFormatStr("%1[%2]").arg(res.name).arg(a),
              childName,
              lit("Resource"),
              ToQStr(descriptors[a].descriptor.resource),
          });
          child->setTag(
              QVariant::fromValue(VariableTag(DebugVariableType::ReadOnlyResource, childName)));
          node->addChild(child);
        }

        ui->constants->addTopLevelItem(node);
      }
    }

    rdcarray<UsedDescriptor> &rwBinds = m_ReadWriteResources;

    for(int i = 0; i < m_Trace->readWriteResources.count(); i++)
    {
      const ShaderVariable &rw = m_Trace->readWriteResources[i];

      if(varsMapped.contains(rw.name))
        continue;

      const rdcarray<UsedDescriptor> &resList = m_ReadWriteResources;

      // find all descriptors in this bind's array
      ShaderBindIndex bind = rw.GetBindIndex();
      bind.arrayElement = 0;

      rdcarray<UsedDescriptor> descriptors;
      for(const UsedDescriptor &a : resList)
        if(CategoryForDescriptorType(a.access.type) == bind.category && a.access.index == bind.index)
          descriptors.push_back(a);

      if(descriptors.empty())
        continue;

      const ShaderResource &res = m_ShaderDetails->readWriteResources[bind.index];

      if(res.bindArraySize == 1)
      {
        RDTreeWidgetItem *node = new RDTreeWidgetItem(
            {res.name, rw.name, lit("Resource"), ToQStr(descriptors[0].descriptor.resource)});
        node->setTag(QVariant::fromValue(VariableTag(DebugVariableType::ReadWriteResource, rw.name)));
        ui->constants->addTopLevelItem(node);
      }
      else if(res.bindArraySize == ~0U)
      {
        RDTreeWidgetItem *node =
            new RDTreeWidgetItem({res.name, rw.name, lit("[unbounded]"), QString()});
        node->setTag(QVariant::fromValue(VariableTag(DebugVariableType::ReadWriteResource, rw.name)));
        ui->constants->addTopLevelItem(node);
      }
      else
      {
        RDTreeWidgetItem *node = new RDTreeWidgetItem(
            {res.name, rw.name, QFormatStr("[%1]").arg(res.bindArraySize), QString()});
        node->setTag(QVariant::fromValue(VariableTag(DebugVariableType::ReadWriteResource, rw.name)));

        uint32_t count = qMin(res.bindArraySize, (uint32_t)descriptors.size());
        for(uint32_t a = 0; a < count; a++)
        {
          QString childName = QFormatStr("%1[%2]").arg(rw.name).arg(a);
          RDTreeWidgetItem *child = new RDTreeWidgetItem({
              QFormatStr("%1[%2]").arg(res.name).arg(a),
              childName,
              lit("RW Resource"),
              ToQStr(descriptors[a].descriptor.resource),
          });
          child->setTag(
              QVariant::fromValue(VariableTag(DebugVariableType::ReadWriteResource, childName)));
          node->addChild(child);
        }

        ui->constants->addTopLevelItem(node);
      }
    }

    rdcarray<UsedDescriptor> samplers = m_Ctx.CurPipelineState().GetSamplers(m_Stage);

    for(int i = 0; i < m_Trace->samplers.count(); i++)
    {
      const ShaderVariable &s = m_Trace->samplers[i];

      if(varsMapped.contains(s.name))
        continue;

      // find all descriptors in this bind's array
      ShaderBindIndex bind = s.GetBindIndex();
      bind.arrayElement = 0;

      rdcarray<UsedDescriptor> descriptors;
      for(const UsedDescriptor &a : samplers)
        if(CategoryForDescriptorType(a.access.type) == bind.category && a.access.index == bind.index)
          descriptors.push_back(a);

      if(descriptors.empty())
        continue;

      const ShaderSampler &samp = m_ShaderDetails->samplers[bind.index];

      if(samp.bindArraySize == 1)
      {
        RDTreeWidgetItem *node =
            new RDTreeWidgetItem({samp.name, s.name, lit("Sampler"),
                                  samplerRep(samp, ~0U, descriptors[0].descriptor.resource)});
        node->setTag(QVariant::fromValue(VariableTag(DebugVariableType::Sampler, s.name)));
        ui->constants->addTopLevelItem(node);
      }
      else if(samp.bindArraySize == ~0U)
      {
        RDTreeWidgetItem *node =
            new RDTreeWidgetItem({samp.name, s.name, lit("[unbounded]"), QString()});
        node->setTag(QVariant::fromValue(VariableTag(DebugVariableType::Sampler, s.name)));
        ui->constants->addTopLevelItem(node);
      }
      else
      {
        RDTreeWidgetItem *node = new RDTreeWidgetItem(
            {samp.name, s.name, QFormatStr("[%1]").arg(samp.bindArraySize), QString()});
        node->setTag(QVariant::fromValue(VariableTag(DebugVariableType::Sampler, s.name)));

        uint32_t count = qMin(samp.bindArraySize, (uint32_t)descriptors.size());
        for(uint32_t a = 0; a < count; a++)
        {
          QString childName = QFormatStr("%1[%2]").arg(s.name).arg(a);
          RDTreeWidgetItem *child = new RDTreeWidgetItem({
              QFormatStr("%1[%2]").arg(m_ShaderDetails->samplers[i].name).arg(a),
              childName,
              lit("Sampler"),
              samplerRep(samp, a, descriptors[a].descriptor.resource),
          });
          child->setTag(QVariant::fromValue(VariableTag(DebugVariableType::Sampler, childName)));
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

    for(int globalVarIdx = 0; globalVarIdx < m_Trace->sourceVars.count(); globalVarIdx++)
    {
      const SourceVariableMapping &sourceVar = m_Trace->sourceVars[globalVarIdx];

      if(!sourceVar.variables.empty() && sourceVar.variables[0].type != DebugVariableType::Variable)
        continue;

      if(sourceVar.rows == 0 || sourceVar.columns == 0)
        continue;

      fakeroot.addChild(makeSourceVariableNode(sourceVar, globalVarIdx, -1));
    }

    const rdcarray<SourceVariableMapping> &sourceVars = GetCurrentInstInfo().sourceVars;

    for(size_t lidx = 0; lidx < sourceVars.size(); lidx++)
    {
      int32_t localVarIdx = int32_t(sourceVars.size() - 1 - lidx);

      // iterate in reverse order, so newest locals tend to end up on top
      const SourceVariableMapping &l = sourceVars[localVarIdx];

      bool hasNonError = false;

      for(const DebugVariableReference &v : l.variables)
        hasNonError |= (GetDebugVariable(v) != NULL);

      // don't display source variables that map to non-existant debug variables. This can happen
      // when flow control means those debug variables were never created, but they would be mapped
      // at this point.
      if(!hasNonError)
        continue;

      RDTreeWidgetItem *node = makeSourceVariableNode(l, -1, localVarIdx);

      fakeroot.addChild(node);
    }

    // recursively combine nodes with the same prefix together
    combineStructures(&fakeroot);

    QList<RDTreeWidgetItem *> nodes;
    while(fakeroot.childCount() > 0)
      nodes.push_back(fakeroot.takeChild(0));

    // sort more recently updated variables to the top
    std::sort(nodes.begin(), nodes.end(), [](const RDTreeWidgetItem *a, const RDTreeWidgetItem *b) {
      VariableTag at = a->tag().value<VariableTag>();
      VariableTag bt = b->tag().value<VariableTag>();
      return at.updateID > bt.updateID;
    });

    for(RDTreeWidgetItem *n : nodes)
      ui->sourceVars->addTopLevelItem(n);

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
      ui->debugVars->addTopLevelItem(makeDebugVariableNode(m_Variables[i], ""));
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

void ShaderViewer::markWatchStale(RDTreeWidgetItem *item)
{
  VariableTag tag = item->tag().value<VariableTag>();
  if(tag.state != WatchVarState::Invalid)
  {
    tag.state = WatchVarState::Stale;
    item->setItalic(true);
    item->setText(1, tr("Unavailable"));
    item->setTag(QVariant::fromValue(tag));
  }

  for(int i = 0; i < item->childCount(); i++)
    markWatchStale(item->child(i));
}

bool ShaderViewer::updateWatchVariable(RDTreeWidgetItem *watchItem, const RDTreeWidgetItem *varItem,
                                       const rdcstr &path, uint32_t swizzle,
                                       const ShaderVariable &var, QChar regcast)
{
  if(!var.members.empty())
  {
    QSet<QString> existing, current;

    // see which members we have already
    for(int i = 0; i < watchItem->childCount(); i++)
      existing.insert(watchItem->child(i)->text(0));

    // see which members are in the variable
    for(int i = 0; i < var.members.count(); i++)
      current.insert(var.members[i].name);

    // if there are no new members in the variable, the existing set will contain the current set
    if(!existing.contains(current))
    {
      // the current variable has some new members in its set - this may be a different structure.
      // Clear the existing watch before updating
      watchItem->clear();
    }

    // iterate over every sub-variable we know about
    QVector<bool> valid;
    valid.resize(watchItem->childCount());
    for(int i = 0; i < var.members.count(); i++)
    {
      int idx = -1;

      QString name = var.members[i].name;

      for(int j = 0; j < watchItem->childCount(); j++)
      {
        if(name == watchItem->child(j)->text(0))
        {
          idx = j;
          break;
        }
      }

      if(idx == -1)
      {
        idx = watchItem->childCount();
        RDTreeWidgetItem *item = new RDTreeWidgetItem({
            name,
            QVariant(),
            QVariant(),
            QVariant(),
        });
        VariableTag tag = VariableTag(DebugVariableType::Variable, path);
        tag.state = WatchVarState::Valid;
        item->setTag(QVariant::fromValue(tag));
        watchItem->addChild(item);
        valid.push_back(false);
      }

      valid[idx] = true;

      rdcstr sep = var.members[i].name[0] == '[' ? "" : ".";

      updateWatchVariable(watchItem->child(idx), varItem->child(i),
                          path + sep + var.members[i].name, ~0U, var.members[i], regcast);
    }

    // any children that weren't marked as valid are now stale
    for(int i = 0; i < watchItem->childCount(); i++)
    {
      if(valid[i])
        continue;
      markWatchStale(watchItem->child(i));
    }

    // resort the watch item members
    QVector<RDTreeWidgetItem *> members;
    while(watchItem->childCount())
      members.push_back(watchItem->takeChild(0));

    std::sort(members.begin(), members.end(),
              [](const RDTreeWidgetItem *a, const RDTreeWidgetItem *b) {
                VariableTag at = a->tag().value<VariableTag>();
                VariableTag bt = b->tag().value<VariableTag>();
                if(at.offset != bt.offset)
                  return at.offset < bt.offset;
                return a->text(0) < b->text(0);
              });

    for(int i = 0; i < members.count(); i++)
      watchItem->addChild(members[i]);

    watchItem->setText(1, QString());
    watchItem->setText(2, QString());
    watchItem->setText(3, QString());

    VariableTag tag = VariableTag(DebugVariableType::Variable, path);
    tag.state = WatchVarState::Valid;
    watchItem->setTag(QVariant::fromValue(tag));

    return true;
  }
  else
  {
    // if the node has no children, clear any stale children we might have had from a previous node
    // (note if a struct disappears entirely, we won't have a varNode here so the node will just be
    // marked stale. We get here if we have a non-struct)
    watchItem->clear();
  }

  size_t dataSize = VarTypeByteSize(var.type);

  if(var.rows > 1 && !var.members.empty())
  {
    watchItem->clear();

    watchItem->setText(1, QString());
    watchItem->setText(2, QString());
    watchItem->setText(3, QString());

    for(uint32_t r = 0; r < var.rows; r++)
    {
      ShaderVariable rowVar = var;
      rowVar.name += ".row" + ToStr(r);
      rowVar.rows = 1;

      if(r > 0)
        memcpy(rowVar.value.u8v.data(), rowVar.value.u8v.data() + dataSize * var.columns * r,
               dataSize * var.columns);

      RDTreeWidgetItem *item = new RDTreeWidgetItem({
          rowVar.name,
          QVariant(),
          QVariant(),
          QVariant(),
      });

      updateWatchVariable(item, varItem->child(r), path + ".row" + ToStr(r), ~0U, rowVar, regcast);
      item->setText(1, getRegNames(varItem, ~0U, r));
      item->setTag(QVariant());
      watchItem->addChild(item);
    }

    VariableTag tag = VariableTag(DebugVariableType::Variable, path);
    tag.state = WatchVarState::Valid;
    watchItem->setTag(QVariant::fromValue(tag));

    return true;
  }

  if(var.type == VarType::Unknown || dataSize == 0)
    dataSize = 4;

  if(regcast == QLatin1Char(' '))
  {
    switch(var.type)
    {
      case VarType::Float:
      case VarType::Double:
      case VarType::Half: regcast = QLatin1Char('f'); break;
      case VarType::Bool:
      case VarType::ULong:
      case VarType::UInt:
      case VarType::UShort:
      case VarType::UByte: regcast = QLatin1Char('u'); break;
      case VarType::SLong:
      case VarType::SInt:
      case VarType::SShort:
      case VarType::SByte: regcast = QLatin1Char('i'); break;
      case VarType::Struct:
      case VarType::Enum:
      case VarType::GPUPointer:
        regcast = QLatin1Char('#');
        dataSize = 8;
        break;
      case VarType::ConstantBlock:
      case VarType::ReadOnlyResource:
      case VarType::ReadWriteResource:
      case VarType::Sampler:
        regcast = QLatin1Char('#');
        dataSize = 4;
        break;
      case VarType::Unknown:
        regcast = ui->intView->isChecked() ? QLatin1Char('i') : QLatin1Char('f');
        dataSize = 4;
        break;
    }
  }

  QString val;
  QColor swatchColor;

  for(uint8_t i = 0; i < var.columns; i++)
  {
    ShaderValue value = {};
    memcpy(&value, var.value.u8v.data() + i * dataSize, dataSize);

    if(regcast == QLatin1Char('#'))
    {
      if(var.type == VarType::GPUPointer)
        val += ToQStr(var.GetPointer());
      else
        val += stringRep(var, 0);
    }
    else if(regcast == QLatin1Char('i') || regcast == QLatin1Char('d'))
    {
      if(dataSize == 8)
        val += Formatter::Format(value.s64v[0]);
      else if(dataSize == 4)
        val += Formatter::Format(value.s32v[0]);
      else if(dataSize == 2)
        val += Formatter::Format(value.s16v[0]);
      else
        val += Formatter::Format(value.s8v[0]);
    }
    else if(regcast == QLatin1Char('f') || regcast == QLatin1Char('c'))
    {
      float f;

      if(dataSize == 8)
      {
        val += Formatter::Format(value.f64v[0]);
        f = (float)value.f64v[0];
      }
      else if(dataSize == 4)
      {
        val += Formatter::Format(value.f32v[0]);
        f = (float)value.f32v[0];
      }
      else
      {
        val += Formatter::Format(value.f16v[0]);
        f = (float)value.f16v[0];
      }

      if(regcast == QLatin1Char('c') && i < 3)
      {
        if(i == 0)
        {
          swatchColor = QColor(0, 0, 0, 255);
          swatchColor.setRedF(f);
        }
        else if(i == 1)
        {
          swatchColor.setGreenF(f);
        }
        else
        {
          swatchColor.setBlueF(f);
        }
      }
    }
    else if(regcast == QLatin1Char('u'))
    {
      val += Formatter::Format(value.u64v[0]);
    }
    else if(regcast == QLatin1Char('x'))
    {
      if(dataSize == 8)
        val += Formatter::Format(value.u64v[0], true);
      else if(dataSize == 4)
        val += Formatter::Format(value.u32v[0], true);
      else if(dataSize == 2)
        val += Formatter::Format(value.u16v[0], true);
      else
        val += Formatter::Format(value.u8v[0], true);
    }
    else if(regcast == QLatin1Char('b'))
    {
      val += QFormatStr("%1").arg(value.u64v[0], (int)dataSize * 8, 2, QLatin1Char('0'));
    }
    else if(regcast == QLatin1Char('o'))
    {
      val += QFormatStr("0%1").arg(value.u64v[0], 0, 8, QLatin1Char('0'));
    }

    if(i < var.columns - 1)
      val += lit(", ");
  }

  watchItem->setText(1, getRegNames(varItem, swizzle));
  watchItem->setText(2, TypeString(var));

  if(!swatchColor.isValid())
  {
    watchItem->setIcon(3, QIcon());
  }
  else
  {
    int h = ui->watch->fontMetrics().height();
    QPixmap pm(1, 1);
    pm.fill(swatchColor);
    pm = pm.scaled(QSize(h, h));

    {
      QPainter painter(&pm);

      QPen pen(ui->watch->palette().foreground(), 1.0);
      painter.setPen(pen);
      painter.drawLine(QPoint(0, 0), QPoint(h - 1, 0));
      painter.drawLine(QPoint(h - 1, 0), QPoint(h - 1, h - 1));
      painter.drawLine(QPoint(h - 1, h - 1), QPoint(0, h - 1));
      painter.drawLine(QPoint(0, h - 1), QPoint(0, 0));
    }

    watchItem->setIcon(3, QIcon(pm));
  }

  watchItem->setText(3, val);
  watchItem->setItalic(false);

  VariableTag tag = VariableTag(DebugVariableType::Variable, path);
  tag.state = WatchVarState::Valid;
  watchItem->setTag(QVariant::fromValue(tag));

  return true;
}

void ShaderViewer::updateWatchVariables()
{
  QSignalBlocker block(ui->watch);

  RDTreeViewExpansionState expansion;
  ui->watch->saveExpansion(expansion, 0);

  ui->watch->beginUpdate();

  for(int i = 0; i < ui->watch->topLevelItemCount() - 1; i++)
  {
    RDTreeWidgetItem *item = ui->watch->topLevelItem(i);

    QString expr = item->text(0).trimmed();

    QRegularExpression exprRE(
        lit("^"                 // beginning of the line
            "([^,]+)"           // variable path
            "(,[iduxbocf])?"    // optional typecast
            "$"));              // end of the line

    QRegularExpressionMatch match = exprRE.match(expr);

    QString error = tr("Error evaluating expression");

    if(match.hasMatch())
    {
      QString path = match.captured(1);
      QChar regcast = QLatin1Char(' ');
      if(!match.captured(2).isEmpty())
        regcast = match.captured(2)[1];

      ShaderVariable var;
      uint32_t swizzle = ~0U;
      const RDTreeWidgetItem *varItem = getVarFromPath(path, &var, &swizzle);
      if(varItem)
      {
        if(updateWatchVariable(item, varItem, path, swizzle, var, regcast))
          continue;

        error = tr("Couldn't evaluate watch for '%1'").arg(expr);
      }
      else if(item->childCount())
      {
        markWatchStale(item);
        continue;
      }
      else
      {
        error = tr("Couldn't find variable for '%1'").arg(path);
      }
    }

    // if we got here, something went wrong.
    VariableTag tag;
    item->setItalic(false);
    item->setText(1, QString());
    item->setText(2, QString());
    item->setText(3, error);
    item->setTag(QVariant::fromValue(tag));
  }

  ui->watch->endUpdate();

  ui->watch->applyExpansion(expansion, 0);
}

RDTreeWidgetItem *ShaderViewer::makeSourceVariableNode(const ShaderVariable &var,
                                                       const rdcstr &debugVarPath,
                                                       VariableTag baseTag)
{
  QString typeName;

  typeName = ToQStr(var.type);

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

  rdcstr debugName = debugVarPath + sep + var.name;

  if(!var.members.empty())
    typeName = QString();

  RDTreeWidgetItem *node = new RDTreeWidgetItem({var.name, debugName, typeName, value});

  VariableTag tag;
  tag.absoluteRefPath = baseTag.absoluteRefPath + sep + var.name;
  tag.expanded = true;
  tag.modified = HasChanged(baseTag.absoluteRefPath + sep + var.name);
  tag.updateID = CalcUpdateID(tag.updateID, baseTag.absoluteRefPath + sep + var.name);

  for(const ShaderVariable &child : var.members)
  {
    RDTreeWidgetItem *item = makeSourceVariableNode(child, debugName, tag);
    tag.modified |= item->tag().value<VariableTag>().modified;
    node->addChild(item);
  }

  // if this is a matrix, even if it has no explicit row members add the rows as children
  if(var.members.empty() && var.rows > 1)
  {
    for(uint32_t row = 0; row < var.rows; row++)
    {
      rdcstr rowsuffix = ".row" + ToStr(row);
      node->addChild(new RDTreeWidgetItem(
          {var.name + rowsuffix, debugName + rowsuffix, rowTypeName, stringRep(var, row)}));
    }
  }
  node->setTag(QVariant::fromValue(tag));

  if(tag.modified)
    node->setForegroundColor(QColor(Qt::red));

  return node;
}

RDTreeWidgetItem *ShaderViewer::makeSourceVariableNode(const SourceVariableMapping &l,
                                                       int globalVarIdx, int localVarIdx)
{
  QString localName = l.name;
  QString typeName;
  QString value;

  typeName = ToQStr(l.type);

  VariableTag baseTag;
  baseTag.absoluteRefPath = localName;
  baseTag.offset = l.offset;
  if(globalVarIdx >= 0)
  {
    baseTag.globalSourceVar = true;
    baseTag.sourceVarIdx = globalVarIdx;
  }
  else
  {
    baseTag.sourceVarIdx = localVarIdx;
  }

  QList<RDTreeWidgetItem *> children;

  uint32_t childCount = 0;

  {
    if(l.rows > 1 && l.variables.size() > l.columns)
      typeName += QFormatStr("%1x%2").arg(l.rows).arg(l.columns);
    else if(l.columns > 1)
      typeName += QString::number(l.columns);

    if(l.rows > 1 && l.variables.size() > l.columns)
      childCount = l.rows;

    for(size_t i = 0; i < l.variables.size(); i++)
    {
      const DebugVariableReference &r = l.variables[i];

      baseTag.modified |= HasChanged(r.name);
      baseTag.updateID = CalcUpdateID(baseTag.updateID, r.name);

      if(!value.isEmpty())
        value += lit(", ");

      if(r.name.empty())
      {
        value += lit("?");
      }
      else if(r.type == DebugVariableType::Sampler)
      {
        const ShaderVariable *reg = GetDebugVariable(r);

        if(reg == NULL)
          continue;

        typeName = lit("Sampler");

        rdcarray<UsedDescriptor> samplers = m_Ctx.CurPipelineState().GetSamplers(m_Stage);

        ShaderBindIndex bind = reg->GetBindIndex();
        int32_t bindIdx = samplers.indexOf(bind);

        if(bindIdx < 0)
          continue;

        Descriptor desc = samplers[bindIdx].descriptor;
        const ShaderSampler &samp = m_ShaderDetails->samplers[bind.index];

        if(samp.bindArraySize == 1)
        {
          value = samplerRep(samp, ~0U, desc.resource);
        }
        else if(samp.bindArraySize == ~0U)
        {
          typeName = lit("[unbounded]");
          value = QString();
        }
        else
        {
          for(uint32_t a = 0; a < samp.bindArraySize; a++)
            children.push_back(new RDTreeWidgetItem({
                QFormatStr("%1[%2]").arg(localName).arg(a),
                QString(),
                typeName,
                samplerRep(samp, a, desc.resource),
            }));

          childCount += samp.bindArraySize;

          typeName = QFormatStr("[%1]").arg(samp.bindArraySize);
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

        typeName = isReadOnlyResource ? lit("Resource") : lit("RW Resource");

        const rdcarray<UsedDescriptor> &resList =
            isReadOnlyResource ? m_ReadOnlyResources : m_ReadWriteResources;

        // find all descriptors in this bind's array
        ShaderBindIndex bind = reg->GetBindIndex();
        bind.arrayElement = 0;

        rdcarray<UsedDescriptor> descriptors;
        for(const UsedDescriptor &a : resList)
          if(CategoryForDescriptorType(a.access.type) == bind.category && a.access.index == bind.index)
            descriptors.push_back(a);

        if(descriptors.empty())
          continue;

        const ShaderResource &res = isReadOnlyResource
                                        ? m_ShaderDetails->readOnlyResources[bind.index]
                                        : m_ShaderDetails->readWriteResources[bind.index];

        if(res.bindArraySize == 1)
        {
          value = ToQStr(descriptors[0].descriptor.resource);
        }
        else if(res.bindArraySize == ~0U)
        {
          typeName = lit("[unbounded]");
          value = QString();
        }
        else
        {
          uint32_t count = qMin(res.bindArraySize, (uint32_t)descriptors.size());
          for(uint32_t a = 0; a < count; a++)
            children.push_back(new RDTreeWidgetItem({
                QFormatStr("%1[%2]").arg(localName).arg(a),
                QString(),
                typeName,
                ToQStr(descriptors[a].descriptor.resource),
            }));

          childCount += res.bindArraySize;

          typeName = QFormatStr("[%1]").arg(res.bindArraySize);
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
              children.push_back(makeSourceVariableNode(child, reg->name, baseTag));
            break;
          }

          switch(l.type)
          {
            case VarType::Float: value += Formatter::Format(reg->value.f32v[r.component]); break;
            case VarType::Double: value += Formatter::Format(reg->value.f64v[r.component]); break;
            case VarType::Half: value += Formatter::Format(reg->value.f16v[r.component]); break;
            case VarType::Bool:
              value += Formatter::Format(reg->value.u32v[r.component] ? true : false);
              break;
            case VarType::ULong: value += Formatter::Format(reg->value.u64v[r.component]); break;
            case VarType::UInt: value += Formatter::Format(reg->value.u32v[r.component]); break;
            case VarType::UShort: value += Formatter::Format(reg->value.u16v[r.component]); break;
            case VarType::UByte: value += Formatter::Format(reg->value.u8v[r.component]); break;
            case VarType::SLong: value += Formatter::Format(reg->value.s64v[r.component]); break;
            case VarType::SInt: value += Formatter::Format(reg->value.s32v[r.component]); break;
            case VarType::SShort: value += Formatter::Format(reg->value.s16v[r.component]); break;
            case VarType::SByte: value += Formatter::Format(reg->value.s8v[r.component]); break;
            case VarType::GPUPointer: value += ToQStr(reg->GetPointer()); break;
            case VarType::ConstantBlock:
            case VarType::ReadOnlyResource:
            case VarType::ReadWriteResource:
            case VarType::Sampler:
            case VarType::Enum:
            case VarType::Struct: value += stringRep(*reg, 0); break;
            case VarType::Unknown:
              qCritical() << "Unexpected unknown variable" << (QString)l.name;
              break;
          }
        }
        else
        {
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
              {QFormatStr("%1.row%2").arg(localBaseName).arg(row), QString(), typeName, value}));
          value = QString();
        }
      }
    }
  }

  RDTreeWidgetItem *node = new RDTreeWidgetItem({localName, QString(), typeName, value});

  for(RDTreeWidgetItem *c : children)
  {
    baseTag.modified |= c->tag().value<VariableTag>().modified;
    node->addChild(c);
  }

  if(baseTag.modified)
    node->setForegroundColor(QColor(Qt::red));

  node->setTag(QVariant::fromValue(baseTag));

  if(childCount > 0)
  {
    for(uint32_t i = 0; i < childCount && i < (uint32_t)node->childCount(); i++)
      node->child(i)->setText(1, getRegNames(node, ~0U, i));
  }
  else
  {
    node->setText(1, getRegNames(node, ~0U));
  }

  return node;
}

RDTreeWidgetItem *ShaderViewer::makeDebugVariableNode(const ShaderVariable &v, rdcstr prefix)
{
  rdcstr basename = prefix + v.name;
  RDTreeWidgetItem *node =
      new RDTreeWidgetItem({v.name, v.rows == 1 && v.members.empty() ? stringRep(v) : QString()});
  VariableTag tag(DebugVariableType::Variable, basename);
  tag.modified = HasChanged(basename);
  tag.updateID = CalcUpdateID(tag.updateID, basename);
  for(const ShaderVariable &m : v.members)
  {
    rdcstr childprefix = basename + ".";
    if(m.name.beginsWith(basename + "["))
      childprefix = basename;
    RDTreeWidgetItem *child = makeDebugVariableNode(m, childprefix);
    tag.modified |= child->tag().value<VariableTag>().modified;
    node->addChild(child);
  }

  // if this is a matrix, even if it has no explicit row members add the rows as children
  if(v.members.empty() && v.rows > 1)
  {
    for(uint32_t row = 0; row < v.rows; row++)
    {
      rdcstr rowsuffix = ".row" + ToStr(row);
      RDTreeWidgetItem *child = new RDTreeWidgetItem({v.name + rowsuffix, stringRep(v, row)});
      child->setTag(
          QVariant::fromValue(VariableTag(DebugVariableType::Variable, basename + rowsuffix)));
      node->addChild(child);
    }
  }

  node->setTag(QVariant::fromValue(tag));

  if(tag.modified)
    node->setForegroundColor(QColor(Qt::red));

  return node;
}

RDTreeWidgetItem *ShaderViewer::makeAccessedResourceNode(const ShaderVariable &v)
{
  ShaderBindIndex bp = v.GetBindIndex();
  ResourceId resId;
  QString typeName;
  if(v.type == VarType::ReadOnlyResource)
  {
    if(bp.category != DescriptorCategory::ReadOnlyResource)
      qCritical() << "Mismatch between variable type and descriptor category";
    typeName = lit("Resource");
    int32_t bindIdx = m_ReadOnlyResources.indexOf(bp);
    if(bindIdx >= 0)
      resId = m_ReadOnlyResources[bindIdx].descriptor.resource;
  }
  else if(v.type == VarType::ReadWriteResource)
  {
    if(bp.category != DescriptorCategory::ReadWriteResource)
      qCritical() << "Mismatch between variable type and descriptor category";
    typeName = lit("RW Resource");
    int32_t bindIdx = m_ReadWriteResources.indexOf(bp);
    if(bindIdx >= 0)
      resId = m_ReadWriteResources[bindIdx].descriptor.resource;
  }

  RDTreeWidgetItem *node = new RDTreeWidgetItem({v.name, typeName, ToQStr(resId)});
  if(resId != ResourceId())
    node->setTag(QVariant::fromValue(AccessedResourceTag(bp, v.type)));
  if(HasChanged(v.name))
    node->setForegroundColor(QColor(Qt::red));

  return node;
}

bool ShaderViewer::HasChanged(rdcstr debugVarName) const
{
  return m_VariablesChanged.contains(debugVarName);
}

uint32_t ShaderViewer::CalcUpdateID(uint32_t prevID, rdcstr debugVarName) const
{
  return qMax(prevID, m_VariableLastUpdate[debugVarName]);
}

// this function is a bit messy, we want a base recursion container of either QList or rdcarray
// but further recursion is always rdcarray because of ShaderVariable::members
template <typename Container>
const ShaderVariable *GetShaderDebugVariable(rdcstr path, const Container &vars)
{
  rdcstr elem;

  // pick out the next element in the path
  // if this is an array index, grab that
  if(path[0] == '[')
  {
    int idx = path.indexOf(']');
    if(idx < 0)
      return NULL;
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
  for(int i = 0; i < vars.count(); i++)
  {
    if(vars[i].name == elem)
    {
      // If there's no more path, we've found the exact match, otherwise continue
      if(path.empty())
        return &vars[i];

      // otherwise recurse
      return GetShaderDebugVariable(path, vars[i].members);
    }
  }

  return NULL;
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
  else if(r.type == DebugVariableType::Input)
  {
    return GetShaderDebugVariable(r.name, m_Trace->inputs);
  }
  else if(r.type == DebugVariableType::Constant)
  {
    return GetShaderDebugVariable(r.name, m_Trace->constantBlocks);
  }
  else if(r.type == DebugVariableType::Variable)
  {
    return GetShaderDebugVariable(r.name, m_Variables);
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

  m_VariablesChanged.clear();

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

void ShaderViewer::ToggleBreakpointOnInstruction(int32_t instruction)
{
  if(m_DeferredInit)
  {
    m_DeferredCommands.push_back(
        [instruction](ShaderViewer *v) { v->ToggleBreakpointOnInstruction(instruction); });
    return;
  }

  if(!m_Trace || m_States.empty())
    return;

  QPair<int, uint32_t> sourceBreakpoint = {-1, 0};
  QList<QPair<int, uint32_t>> disasmBreakpoints;

  if(instruction >= 0)
  {
    const LineColumnInfo &instLine = GetInstInfo(instruction).lineInfo;
    sourceBreakpoint = {instLine.fileIndex, instLine.lineStart};
    disasmBreakpoints.push_back({-1, instLine.disassemblyLine});
  }
  else
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

      LineColumnInfo sourceLine;
      sourceLine.fileIndex = scintillaIndex;
      sourceLine.lineStart = sourceLine.lineEnd = i;

      auto it = m_Location2Inst.lowerBound(sourceLine);

      // find the next source location that has an instruction mapped. If there was an exact match
      // this won't loop, if the lower bound was earlier we'll step at most once to get to the next
      // line past it.
      if(it != m_Location2Inst.end() && (sptr_t)it.key().lineEnd < i)
        it++;

      // set a breakpoint on all instructions that match this line
      if(it != m_Location2Inst.end())
      {
        sourceBreakpoint = {scintillaIndex, (uint32_t)i};
        for(uint32_t inst : it.value())
          disasmBreakpoints.push_back({-1, GetInstInfo(inst).lineInfo.disassemblyLine});
      }
      else
      {
        return;
      }
    }
    else
    {
      int32_t disassemblyLine = m_DisassemblyView->lineFromPosition(m_DisassemblyView->currentPos());

      for(; disassemblyLine < m_DisassemblyView->lineCount(); disassemblyLine++)
      {
        if(instructionForDisassemblyLine(disassemblyLine) >= 0)
          break;
      }

      if(disassemblyLine < m_DisassemblyView->lineCount())
        disasmBreakpoints.push_back({-1, (uint32_t)disassemblyLine + 1});
    }
  }

  // if we have a source breakpoint, treat that as 'canonical' whether or not the corresponding
  // disasm ones are set
  if(sourceBreakpoint.first >= 0)
  {
    if(m_TempBreakpoint)
    {
      m_Breakpoints.insert(sourceBreakpoint);
      for(QPair<int, uint32_t> &d : disasmBreakpoints)
        m_Breakpoints.insert(d);
    }
    else if(m_Breakpoints.contains(sourceBreakpoint))
    {
      m_Breakpoints.remove(sourceBreakpoint);
      m_FileScintillas[sourceBreakpoint.first]->markerDelete(sourceBreakpoint.second - 1,
                                                             BREAKPOINT_MARKER);
      m_FileScintillas[sourceBreakpoint.first]->markerDelete(sourceBreakpoint.second - 1,
                                                             BREAKPOINT_MARKER + 1);

      for(QPair<int, uint32_t> &d : disasmBreakpoints)
      {
        m_Breakpoints.remove(d);
        m_DisassemblyView->markerDelete(d.second - 1, BREAKPOINT_MARKER);
        m_DisassemblyView->markerDelete(d.second - 1, BREAKPOINT_MARKER + 1);
      }
    }
    else
    {
      m_Breakpoints.insert(sourceBreakpoint);
      m_FileScintillas[sourceBreakpoint.first]->markerAdd(sourceBreakpoint.second - 1,
                                                          BREAKPOINT_MARKER);
      m_FileScintillas[sourceBreakpoint.first]->markerAdd(sourceBreakpoint.second - 1,
                                                          BREAKPOINT_MARKER + 1);

      for(QPair<int, uint32_t> &d : disasmBreakpoints)
      {
        m_Breakpoints.insert(d);
        m_DisassemblyView->markerAdd(d.second - 1, BREAKPOINT_MARKER);
        m_DisassemblyView->markerAdd(d.second - 1, BREAKPOINT_MARKER + 1);
      }
    }
  }
  else
  {
    if(disasmBreakpoints.empty())
      return;

    QPair<int, uint32_t> &bp = disasmBreakpoints[0];

    if(m_TempBreakpoint)
    {
      m_Breakpoints.insert(bp);
    }
    else if(m_Breakpoints.contains(bp))
    {
      m_Breakpoints.remove(bp);
      m_DisassemblyView->markerDelete(bp.second - 1, BREAKPOINT_MARKER);
      m_DisassemblyView->markerDelete(bp.second - 1, BREAKPOINT_MARKER + 1);
    }
    else
    {
      m_Breakpoints.insert(bp);
      m_DisassemblyView->markerAdd(bp.second - 1, BREAKPOINT_MARKER);
      m_DisassemblyView->markerAdd(bp.second - 1, BREAKPOINT_MARKER + 1);
    }
  }
}

void ShaderViewer::ToggleBreakpointOnDisassemblyLine(int32_t disassemblyLine)
{
  if(!m_Trace || m_States.empty())
    return;

  // move forward to the next actual mapped line
  for(; disassemblyLine < m_DisassemblyView->lineCount(); disassemblyLine++)
  {
    if(instructionForDisassemblyLine(disassemblyLine - 1) >= 0)
      break;
  }

  if(disassemblyLine >= m_DisassemblyView->lineCount())
    return;

  if(m_TempBreakpoint)
  {
    m_Breakpoints.insert({-1, (uint32_t)disassemblyLine});
  }
  else if(m_Breakpoints.contains({-1, (uint32_t)disassemblyLine}))
  {
    m_Breakpoints.remove({-1, (uint32_t)disassemblyLine});
    m_DisassemblyView->markerDelete(disassemblyLine - 1, BREAKPOINT_MARKER);
    m_DisassemblyView->markerDelete(disassemblyLine - 1, BREAKPOINT_MARKER + 1);
  }
  else
  {
    m_Breakpoints.insert({-1, (uint32_t)disassemblyLine});
    m_DisassemblyView->markerAdd(disassemblyLine - 1, BREAKPOINT_MARKER);
    m_DisassemblyView->markerAdd(disassemblyLine - 1, BREAKPOINT_MARKER + 1);
  }
}

void ShaderViewer::RunForward()
{
  if(m_DeferredInit)
  {
    m_DeferredCommands.push_back([](ShaderViewer *v) { v->RunForward(); });
    return;
  }

  runTo(~0U, true);
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

  updateEditState();
}

void ShaderViewer::AddWatch(const rdcstr &variable)
{
  if(variable.isEmpty())
    return;

  RDTreeWidgetItem *item = new RDTreeWidgetItem({
      QString(variable),
      QVariant(),
      QVariant(),
      QVariant(),
  });
  item->setEditable(0, true);
  ui->watch->insertTopLevelItem(ui->watch->topLevelItemCount() - 1, item);

  ToolWindowManager::raiseToolWindow(ui->watch);
  ui->watch->activateWindow();
  ui->watch->QWidget::setFocus();

  updateWatchVariables();
}

rdcstrpairs ShaderViewer::GetCurrentFileContents()
{
  rdcstrpairs files;
  for(ScintillaEdit *s : m_Scintillas)
  {
    // don't include the disassembly view
    if(m_DisassemblyView == s)
      continue;

    QWidget *w = (QWidget *)s;
    files.push_back(
        {w->property("filename").toString(), QString::fromUtf8(s->getText(s->textLength() + 1))});
  }
  return files;
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

void ShaderViewer::snippet_constants()
{
  ShaderEncoding encoding = currentEncoding();

  QString text;

  if(encoding == ShaderEncoding::GLSL)
  {
    text = lit(R"(
/////////////////////////////////////
//            Constants            //
/////////////////////////////////////

// possible values (these are only return values from this function, NOT texture binding points):
// RD_TextureType_1D
// RD_TextureType_2D
// RD_TextureType_3D
// RD_TextureType_Cube (OpenGL only)
// RD_TextureType_1D_Array (OpenGL only)
// RD_TextureType_2D_Array (OpenGL only)
// RD_TextureType_Cube_Array (OpenGL only)
// RD_TextureType_Rect (OpenGL only)
// RD_TextureType_Buffer (OpenGL only)
// RD_TextureType_2DMS
// RD_TextureType_2DMS_Array (OpenGL only)
uint RD_TextureType();

// selected sample, or -numSamples for resolve
int RD_SelectedSample();

uint RD_SelectedSliceFace();

uint RD_SelectedMip();

// xyz = width, height, depth (or array size). w = # mips
uvec4 RD_TexDim();

// x = horizontal downsample rate (1 full rate, 2 half rate)
// y = vertical downsample rate
// z = number of planes in input texture
// w = number of bits per component (8, 10, 16)
uvec4 RD_YUVDownsampleRate();

// x = where Y channel comes from
// y = where U channel comes from
// z = where V channel comes from
// w = where A channel comes from
// each index will be [0,1,2,3] for xyzw in first plane,
// [4,5,6,7] for xyzw in second plane texture, etc.
// it will be 0xff = 255 if the channel does not exist.
uvec4 RD_YUVAChannels();

// a pair with minimum and maximum selected range values
vec2 RD_SelectedRange();

/////////////////////////////////////

)");
  }
  else if(encoding == ShaderEncoding::HLSL || encoding == ShaderEncoding::Slang)
  {
    text = lit(R"(
/////////////////////////////////////
//            Constants            //
/////////////////////////////////////

// possible values (these are only return values from this function, NOT texture binding points):
// RD_TextureType_1D
// RD_TextureType_2D
// RD_TextureType_3D
// RD_TextureType_Depth
// RD_TextureType_DepthStencil
// RD_TextureType_DepthMS
// RD_TextureType_DepthStencilMS
// RD_TextureType_2DMS
uint RD_TextureType();

// selected sample, or -numSamples for resolve
int RD_SelectedSample();

uint RD_SelectedSliceFace();

uint RD_SelectedMip();

// xyz = width, height, depth. w = # mips
uint4 RD_TexDim();

// x = horizontal downsample rate (1 full rate, 2 half rate)
// y = vertical downsample rate
// z = number of planes in input texture
// w = number of bits per component (8, 10, 16)
uint4 RD_YUVDownsampleRate();

// x = where Y channel comes from
// y = where U channel comes from
// z = where V channel comes from
// w = where A channel comes from
// each index will be [0,1,2,3] for xyzw in first plane,
// [4,5,6,7] for xyzw in second plane texture, etc.
// it will be 0xff = 255 if the channel does not exist.
uint4 RD_YUVAChannels();

// a pair with minimum and maximum selected range values
float2 RD_SelectedRange();

/////////////////////////////////////

)");
  }
  else if(encoding == ShaderEncoding::SPIRVAsm || encoding == ShaderEncoding::OpenGLSPIRVAsm)
  {
    text = lit("; Can't insert snippets for SPIR-V ASM");
  }

  insertSnippet(text);
}

void ShaderViewer::snippet_samplers()
{
  ShaderEncoding encoding = currentEncoding();

  if(encoding == ShaderEncoding::HLSL || encoding == ShaderEncoding::Slang)
  {
    insertSnippet(lit(R"(
/////////////////////////////////////
//            Samplers             //
/////////////////////////////////////

SamplerState pointSampler : register(RD_POINT_SAMPLER_BINDING);
SamplerState linearSampler : register(RD_LINEAR_SAMPLER_BINDING);

/////////////////////////////////////

)"));
  }
  else if(encoding == ShaderEncoding::GLSL)
  {
    insertSnippet(lit(R"(

/////////////////////////////////////
//            Samplers             //
/////////////////////////////////////

#ifdef VULKAN

layout(binding = RD_POINT_SAMPLER_BINDING) uniform sampler pointSampler;
layout(binding = RD_LINEAR_SAMPLER_BINDING) uniform sampler linearSampler;

#endif

/////////////////////////////////////
)"));
  }
  else if(encoding == ShaderEncoding::SPIRVAsm || encoding == ShaderEncoding::OpenGLSPIRVAsm)
  {
    insertSnippet(lit("; Can't insert snippets for SPIR-V ASM"));
  }
}

void ShaderViewer::snippet_resources()
{
  ShaderEncoding encoding = currentEncoding();
  GraphicsAPI api = m_Ctx.APIProps().localRenderer;

  if(encoding == ShaderEncoding::HLSL || encoding == ShaderEncoding::Slang)
  {
    insertSnippet(lit(R"(
/////////////////////////////////////
//           Resources             //
/////////////////////////////////////

// Float Textures
Texture1DArray<float4> texDisplayTex1DArray : register(RD_FLOAT_1D_ARRAY_BINDING);
Texture2DArray<float4> texDisplayTex2DArray : register(RD_FLOAT_2D_ARRAY_BINDING);
Texture3D<float4> texDisplayTex3D : register(RD_FLOAT_3D_BINDING);
Texture2DMSArray<float4> texDisplayTex2DMSArray : register(RD_FLOAT_2DMS_ARRAY_BINDING);
Texture2DArray<float4> texDisplayYUVArray : register(RD_FLOAT_YUV_ARRAY_BINDING);

// only used on D3D
Texture2DArray<float2> texDisplayTexDepthArray : register(RD_FLOAT_DEPTH_ARRAY_BINDING);
Texture2DArray<uint2> texDisplayTexStencilArray : register(RD_FLOAT_STENCIL_ARRAY_BINDING);
Texture2DMSArray<float2> texDisplayTexDepthMSArray : register(RD_FLOAT_DEPTHMS_ARRAY_BINDING);
Texture2DMSArray<uint2> texDisplayTexStencilMSArray : register(RD_FLOAT_STENCILMS_ARRAY_BINDING);

// Int Textures
Texture1DArray<int4> texDisplayIntTex1DArray : register(RD_INT_1D_ARRAY_BINDING);
Texture2DArray<int4> texDisplayIntTex2DArray : register(RD_INT_2D_ARRAY_BINDING);
Texture3D<int4> texDisplayIntTex3D : register(RD_INT_3D_BINDING);
Texture2DMSArray<int4> texDisplayIntTex2DMSArray : register(RD_INT_2DMS_ARRAY_BINDING);

// Unsigned int Textures
Texture1DArray<uint4> texDisplayUIntTex1DArray : register(RD_UINT_1D_ARRAY_BINDING);
Texture2DArray<uint4> texDisplayUIntTex2DArray : register(RD_UINT_2D_ARRAY_BINDING);
Texture3D<uint4> texDisplayUIntTex3D : register(RD_UINT_3D_BINDING);
Texture2DMSArray<uint4> texDisplayUIntTex2DMSArray : register(RD_UINT_2DMS_ARRAY_BINDING);

/////////////////////////////////////

)"));
  }
  else if(encoding == ShaderEncoding::GLSL)
  {
    insertSnippet(lit(R"(
/////////////////////////////////////
//           Resources             //
/////////////////////////////////////

// Float Textures
layout (binding = RD_FLOAT_1D_ARRAY_BINDING) uniform sampler1DArray tex1DArray;
layout (binding = RD_FLOAT_2D_ARRAY_BINDING) uniform sampler2DArray tex2DArray;
layout (binding = RD_FLOAT_3D_BINDING) uniform sampler3D tex3D;
layout (binding = RD_FLOAT_2DMS_ARRAY_BINDING) uniform sampler2DMSArray tex2DMSArray;

// YUV textures only supported on vulkan
#ifdef VULKAN
layout(binding = RD_FLOAT_YUV_ARRAY_BINDING) uniform sampler2DArray texYUVArray[2];
#endif

// OpenGL has more texture types to match
#ifndef VULKAN
layout (binding = RD_FLOAT_1D_BINDING) uniform sampler1D tex1D;
layout (binding = RD_FLOAT_2D_BINDING) uniform sampler2D tex2D;
layout (binding = RD_FLOAT_CUBE_BINDING) uniform samplerCube texCube;
layout (binding = RD_FLOAT_CUBE_ARRAY_BINDING) uniform samplerCubeArray texCubeArray;
layout (binding = RD_FLOAT_RECT_BINDING) uniform sampler2DRect tex2DRect;
layout (binding = RD_FLOAT_BUFFER_BINDING) uniform samplerBuffer texBuffer;
layout (binding = RD_FLOAT_2DMS_BINDING) uniform sampler2DMS tex2DMS;
#endif

// Int Textures
layout (binding = RD_INT_1D_ARRAY_BINDING) uniform isampler1DArray texSInt1DArray;
layout (binding = RD_INT_2D_ARRAY_BINDING) uniform isampler2DArray texSInt2DArray;
layout (binding = RD_INT_3D_BINDING) uniform isampler3D texSInt3D;
layout (binding = RD_INT_2DMS_ARRAY_BINDING) uniform isampler2DMSArray texSInt2DMSArray;

#ifndef VULKAN
layout (binding = RD_INT_1D_BINDING) uniform isampler1D texSInt1D;
layout (binding = RD_INT_2D_BINDING) uniform isampler2D texSInt2D;
layout (binding = RD_INT_RECT_BINDING) uniform isampler2DRect texSInt2DRect;
layout (binding = RD_INT_BUFFER_BINDING) uniform isamplerBuffer texSIntBuffer;
layout (binding = RD_INT_2DMS_BINDING) uniform isampler2DMS texSInt2DMS;
#endif

// Unsigned int Textures
layout (binding = RD_UINT_1D_ARRAY_BINDING) uniform usampler1DArray texUInt1DArray;
layout (binding = RD_UINT_2D_ARRAY_BINDING) uniform usampler2DArray texUInt2DArray;
layout (binding = RD_UINT_3D_BINDING) uniform usampler3D texUInt3D;
layout (binding = RD_UINT_2DMS_ARRAY_BINDING) uniform usampler2DMSArray texUInt2DMSArray;

#ifndef VULKAN
layout (binding = RD_UINT_1D_BINDING) uniform usampler1D texUInt1D;
layout (binding = RD_UINT_2D_BINDING) uniform usampler2D texUInt2D;
layout (binding = RD_UINT_RECT_BINDING) uniform usampler2DRect texUInt2DRect;
layout (binding = RD_UINT_BUFFER_BINDING) uniform usamplerBuffer texUIntBuffer;
layout (binding = RD_UINT_2DMS_BINDING) uniform usampler2DMS texUInt2DMS;
#endif

/////////////////////////////////////
)"));
  }
  else if(encoding == ShaderEncoding::SPIRVAsm || encoding == ShaderEncoding::OpenGLSPIRVAsm)
  {
    insertSnippet(lit("; Can't insert snippets for SPIR-V ASM"));
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
        showVariableTooltip(tag.absoluteRefPath);
      }
    }
  }
  if(event->type() == QEvent::MouseMove || event->type() == QEvent::Leave)
  {
    hideVariableTooltip();
  }

  return QFrame::eventFilter(watched, event);
}

void ShaderViewer::MarkModification()
{
  if(m_ModifyCallback)
    m_ModifyCallback(this, false);

  m_Modified = true;

  updateEditState();
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
  m_TooltipVarPath = name.replace(QRegularExpression(lit("\\s+")), QString());
  m_TooltipPos = QCursor::pos();

  // don't do anything if we're showing a context menu
  if(m_ContextActive)
    return;

  updateVariableTooltip();
}

void ShaderViewer::updateVariableTooltip()
{
  if(!m_Trace || m_States.empty())
    return;

  ShaderVariable var;

  if(!getVarFromPath(m_TooltipVarPath, &var))
    return;

  if(var.type != VarType::Unknown)
  {
    QString tooltip;

    if(var.type == VarType::Sampler || var.type == VarType::ReadOnlyResource ||
       var.type == VarType::ReadWriteResource || var.type == VarType::GPUPointer)
    {
      tooltip = QFormatStr("%1: ").arg(var.name) + RichResourceTextFormat(m_Ctx, stringRep(var, 0));
    }
    else if(var.type == VarType::ConstantBlock)
    {
      tooltip = QFormatStr("<pre>%1: { ... }</pre>").arg(var.name);
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
  else if(var.members.size() == 2 && var.members[0].type == VarType::ReadOnlyResource &&
          var.members[1].type == VarType::Sampler)
  {
    // combined image/sampler structs
    QToolTip::showText(m_TooltipPos,
                       QFormatStr("%1: %2 & %3")
                           .arg(var.name)
                           .arg(RichResourceTextFormat(m_Ctx, stringRep(var.members[0], 0)))
                           .arg(RichResourceTextFormat(m_Ctx, stringRep(var.members[1], 0))));
    return;
  }
  else if(!var.members.empty())
  {
    // other structs
    QToolTip::showText(m_TooltipPos, lit("{ ... }"));
    return;
  }

  QString text = QFormatStr("<pre>%1\n").arg(var.name);
  text +=
      lit("                  X           Y           Z           W \n"
          "--------------------------------------------------------\n");

  text += QFormatStr("float | %1 %2 %3 %4\n")
              .arg(Formatter::Format(var.value.f32v[0]), 11)
              .arg(Formatter::Format(var.value.f32v[1]), 11)
              .arg(Formatter::Format(var.value.f32v[2]), 11)
              .arg(Formatter::Format(var.value.f32v[3]), 11);
  text += QFormatStr("uint  | %1 %2 %3 %4\n")
              .arg(var.value.u32v[0], 11, 10, QLatin1Char(' '))
              .arg(var.value.u32v[1], 11, 10, QLatin1Char(' '))
              .arg(var.value.u32v[2], 11, 10, QLatin1Char(' '))
              .arg(var.value.u32v[3], 11, 10, QLatin1Char(' '));
  text += QFormatStr("int   | %1 %2 %3 %4\n")
              .arg(var.value.s32v[0], 11, 10, QLatin1Char(' '))
              .arg(var.value.s32v[1], 11, 10, QLatin1Char(' '))
              .arg(var.value.s32v[2], 11, 10, QLatin1Char(' '))
              .arg(var.value.s32v[3], 11, 10, QLatin1Char(' '));
  text += QFormatStr("hex   |    %1    %2    %3    %4")
              .arg(Formatter::HexFormat(var.value.u32v[0], 4))
              .arg(Formatter::HexFormat(var.value.u32v[1], 4))
              .arg(Formatter::HexFormat(var.value.u32v[2], 4))
              .arg(Formatter::HexFormat(var.value.u32v[3], 4));
  text += lit("</pre>");

  QToolTip::showText(m_TooltipPos, text);
}

void ShaderViewer::hideVariableTooltip()
{
  QToolTip::hideText();
  m_TooltipVarIndex = -1;
  m_TooltipVarPath = QString();
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
      // append command line from saved flags, so any specified options override the defaults if
      // they're specified twice
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

void ShaderViewer::on_resetEdits_clicked()
{
  QMessageBox::StandardButton res =
      RDDialog::question(this, tr("Are you sure?"),
                         tr("Are you sure you want to reset all edits and restore the "
                            "shader source back to the original?"),
                         QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);

  if(res != QMessageBox::Yes)
    return;

  for(ScintillaEdit *s : m_Scintillas)
  {
    QWidget *w = (QWidget *)s;
    s->selectAll();
    s->replaceSel(w->property("origText").toString().toUtf8().data());
  }

  m_RevertCallback(&m_Ctx, this, m_EditingShader);

  m_Modified = false;
  m_Saved = false;

  updateEditState();
}

void ShaderViewer::on_unrefresh_clicked()
{
  m_RevertCallback(&m_Ctx, this, m_EditingShader);

  m_Modified = true;
  m_Saved = false;

  updateEditState();
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

    if(encoding == ShaderEncoding::HLSL || encoding == ShaderEncoding::Slang ||
       encoding == ShaderEncoding::GLSL)
    {
      bool success = ProcessIncludeDirectives(source, files);
      if(!success)
        return;
    }

    m_Saved = true;

    bytebuf shaderBytes(source.toUtf8());

    rdcarray<ShaderEncoding> accepted = m_Ctx.TargetShaderEncodings();

    rdcstr spirvVer = "spirv1.0";
    for(const ShaderCompileFlag &flag : m_Flags.flags)
      if(flag.name == "@spirver")
        spirvVer = flag.value;

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
                                                    spirvVer, ui->toolCommandLine->toPlainText());

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

    m_Modified = false;

    m_SaveCallback(&m_Ctx, this, m_EditingShader, m_Stage, encoding, flags, ui->entryFunc->text(),
                   shaderBytes);
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

  m_FindAllResults.clear();

  QByteArray findUtf8 = find.toUtf8();

  if(findUtf8.isEmpty())
    return;

  for(ScintillaEdit *s : scintillas)
  {
    sptr_t start = 0;
    sptr_t end = s->length();

    s->setIndicatorCurrent(INDICATOR_FINDRESULT);
    s->indicatorClearRange(start, end);

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

        results += QFormatStr("  %1(%2): ").arg(s->windowTitle()).arg(line + 1, 4);
        int startPos = results.length();

        results += lineText;
        results += lit("\n");

        resultList.push_back(
            qMakePair(result.first - lineStart + startPos, result.second - lineStart + startPos));

        m_FindAllResults.push_back({s, result.first});
      }

      start = result.second;

    } while(result.first >= 0);
  }

  results += tr("Matching lines: %1").arg(resultList.count());

  m_FindResults->setReadOnly(false);
  m_FindResults->setText(results.toUtf8().data());

  m_FindResults->setIndicatorCurrent(INDICATOR_FINDALLHIGHLIGHT);
  m_FindResults->indicatorClearRange(0, m_FindResults->length());

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
    ui->docking->setToolWindowProperties(
        m_FindResults, ToolWindowManager::HideOnClose | ToolWindowManager::DisallowFloatWindow);
  }
}

void ShaderViewer::resultsDoubleClick(int position, int line)
{
  if(line >= 1 && line - 1 < m_FindAllResults.count())
  {
    m_FindResults->setIndicatorCurrent(INDICATOR_FINDALLHIGHLIGHT);
    m_FindResults->indicatorClearRange(0, m_FindResults->length());

    sptr_t start = m_FindResults->positionFromLine(line);
    sptr_t length = m_FindResults->lineLength(line);
    m_FindResults->indicatorFillRange(start, length);

    m_FindResults->setSelection(position, position);

    ScintillaEdit *s = m_FindAllResults[line - 1].first;
    int resultPos = m_FindAllResults[line - 1].second;
    ToolWindowManager::raiseToolWindow(s);
    s->activateWindow();
    s->QWidget::setFocus();
    s->clearSelections();
    s->setSelection(resultPos, resultPos);
    s->scrollCaret();
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

void ShaderViewer::ToggleBookmark()
{
  ScintillaEdit *cur = currentScintilla();
  if(!cur)
    return;

  sptr_t curLine = cur->lineFromPosition(cur->currentPos());

  QList<sptr_t> &bookmarks = m_Bookmarks[cur];
  if(bookmarks.contains(curLine))
  {
    bookmarks.removeOne(curLine);
    cur->markerDelete(curLine, BOOKMARK_MARKER);
  }
  else
  {
    // Insert new bookmark in numerical order
    bookmarks.insert(std::lower_bound(bookmarks.begin(), bookmarks.end(), curLine), curLine);
    cur->markerAdd(curLine, BOOKMARK_MARKER);
  }
}

void ShaderViewer::NextBookmark()
{
  ScintillaEdit *cur = currentScintilla();
  if(!cur)
    return;

  auto itBookmarks = m_Bookmarks.find(cur);
  if(itBookmarks == m_Bookmarks.end() || itBookmarks->empty())
    return;

  sptr_t curLine = cur->lineFromPosition(cur->currentPos());
  auto itNextBookmark = std::upper_bound(itBookmarks->begin(), itBookmarks->end(), curLine);
  if(itNextBookmark == itBookmarks->end())
    itNextBookmark = itBookmarks->begin();

  if(*itNextBookmark != curLine)
    cur->gotoLine(*itNextBookmark);
}

void ShaderViewer::PreviousBookmark()
{
  ScintillaEdit *cur = currentScintilla();
  if(!cur)
    return;

  auto itBookmarks = m_Bookmarks.find(cur);
  if(itBookmarks == m_Bookmarks.end())
    return;

  sptr_t curLine = cur->lineFromPosition(cur->currentPos());
  auto itPrevBookmark = std::lower_bound(itBookmarks->begin(), itBookmarks->end(), curLine);
  if(itPrevBookmark == itBookmarks->begin())
    itPrevBookmark = itBookmarks->end();
  --itPrevBookmark;

  if(*itPrevBookmark != curLine)
    cur->gotoLine(*itPrevBookmark);
}

void ShaderViewer::ClearAllBookmarks()
{
  ScintillaEdit *cur = currentScintilla();
  if(!cur)
    return;

  cur->markerDeleteAll(BOOKMARK_MARKER);
  m_Bookmarks.remove(cur);
}

void ShaderViewer::SetFindTextFromCurrentWord()
{
  ScintillaEdit *edit = qobject_cast<ScintillaEdit *>(QObject::sender());

  if(edit)
  {
    // if there's a selection, fill the find prompt with that
    if(!edit->getSelText().isEmpty())
    {
      m_FindReplace->setFindText(QString::fromUtf8(edit->getSelText()));
    }
    else
    {
      // otherwise pick the word under the cursor, if there is one
      sptr_t scintillaPos = edit->currentPos();

      sptr_t start = edit->wordStartPosition(scintillaPos, true);
      sptr_t end = edit->wordEndPosition(scintillaPos, true);

      QByteArray text = edit->textRange(start, end);

      if(!text.isEmpty())
        m_FindReplace->setFindText(QString::fromUtf8(text));
    }
  }
}

bool ShaderViewer::HasBookmarks()
{
  ScintillaEdit *cur = currentScintilla();
  auto itBookmarks = m_Bookmarks.find(cur);
  return itBookmarks != m_Bookmarks.end() && !itBookmarks->empty();
}
