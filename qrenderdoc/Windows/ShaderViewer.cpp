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
#include "3rdparty/toolwindowmanager/ToolWindowManager.h"
#include "Code/ScintillaSyntax.h"
#include "SciLexer.h"
#include "ScintillaEdit.h"
#include "ui_ShaderViewer.h"

ShaderViewer::ShaderViewer(CaptureContext *ctx, ShaderReflection *shader, ShaderStageType stage,
                           ShaderDebugTrace *trace, const QString &debugContext, QWidget *parent)
    : QFrame(parent), ui(new Ui::ShaderViewer), m_Ctx(ctx)
{
  ui->setupUi(this);

  m_ShaderDetails = shader;
  m_Trace = trace;

  if(trace != NULL)
    setWindowTitle(
        QString("Debugging %1 - %2").arg(m_Ctx->CurPipelineState.GetShaderName(stage)).arg(debugContext));
  else
    setWindowTitle(m_Ctx->CurPipelineState.GetShaderName(stage));

  QString disasm = shader != NULL ? ToQStr(shader->Disassembly) : "";

  {
    m_DisassemblyView = MakeEditor("scintillaDisassem", disasm,
                                   m_Ctx->APIProps().pipelineType == eGraphicsAPI_Vulkan);
    m_DisassemblyView->setReadOnly(true);
    m_DisassemblyView->setWindowTitle(tr("Disassembly"));

    QObject::connect(m_DisassemblyView, &ScintillaEdit::keyPressed, this,
                     &ShaderViewer::disassembly_keyPressed);
    QObject::connect(m_DisassemblyView, &ScintillaEdit::keyPressed, this,
                     &ShaderViewer::readonly_keyPressed);

    // C# LightCoral
    m_DisassemblyView->markerSetBack(CURRENT_MARKER, SCINTILLA_COLOUR(240, 128, 128));
    m_DisassemblyView->markerDefine(CURRENT_MARKER, SC_MARK_SHORTARROW);

    // C# LightSlateGray
    m_DisassemblyView->markerSetBack(FINISHED_MARKER, SCINTILLA_COLOUR(119, 136, 153));
    m_DisassemblyView->markerDefine(FINISHED_MARKER, SC_MARK_ROUNDRECT);

    // C# Red
    m_DisassemblyView->markerSetBack(BREAKPOINT_MARKER, SCINTILLA_COLOUR(255, 0, 0));
    m_DisassemblyView->markerDefine(BREAKPOINT_MARKER, SC_MARK_CIRCLE);

    m_Scintillas.push_back(m_DisassemblyView);

    ui->docking->addToolWindow(m_DisassemblyView, ToolWindowManager::EmptySpace);
    ui->docking->setToolWindowProperties(m_DisassemblyView, ToolWindowManager::HideCloseButton);
  }

  if(shader != NULL && shader->DebugInfo.entryFunc.count > 0 && shader->DebugInfo.files.count > 0)
  {
    if(trace != NULL)
      setWindowTitle(
          QString("Debug %1() - %2").arg(ToQStr(shader->DebugInfo.entryFunc)).arg(debugContext));
    else
      setWindowTitle(ToQStr(shader->DebugInfo.entryFunc));

    int fileIdx = 0;

    QWidget *sel = m_DisassemblyView;
    for(auto &f : shader->DebugInfo.files)
    {
      QString name = ToQStr(f.first);
      QString text = ToQStr(f.second);

      ScintillaEdit *scintilla = MakeEditor("scintilla" + name, text, true);
      scintilla->setReadOnly(true);
      scintilla->setWindowTitle(name);
      ((QWidget *)scintilla)->setProperty("name", name);

      QObject::connect(scintilla, &ScintillaEdit::keyPressed, this,
                       &ShaderViewer::readonly_keyPressed);

      ui->docking->addToolWindow(
          scintilla, ToolWindowManager::AreaReference(ToolWindowManager::AddTo,
                                                      ui->docking->areaOf(m_DisassemblyView)));
      ui->docking->setToolWindowProperties(scintilla, ToolWindowManager::HideCloseButton);

      m_Scintillas.push_back(scintilla);

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

    // if(shader->DebugInfo.files.count > 2)
    // AddFileList();

    ToolWindowManager::raiseToolWindow(sel);
  }

  for(int i = 0; i < ui->inputSig->header()->count(); i++)
    ui->inputSig->header()->setSectionResizeMode(i, QHeaderView::ResizeToContents);

  for(int i = 0; i < ui->outputSig->header()->count(); i++)
    ui->outputSig->header()->setSectionResizeMode(i, QHeaderView::ResizeToContents);

  if(m_ShaderDetails != NULL)
  {
    for(SigParameter &s : m_ShaderDetails->InputSig)
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
    for(const SigParameter &s : m_ShaderDetails->OutputSig)
    {
      if(s.stream > 0)
      {
        multipleStreams = true;
        break;
      }
    }

    for(const SigParameter &s : m_ShaderDetails->OutputSig)
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
  ui->docking->addToolWindow(
      ui->inputSig, ToolWindowManager::AreaReference(ToolWindowManager::BottomOf,
                                                     ui->docking->areaOf(m_DisassemblyView), 0.2f));
  ui->docking->setToolWindowProperties(ui->inputSig, ToolWindowManager::HideCloseButton);

  ui->outputSig->setWindowTitle(tr("Output Signature"));
  ui->docking->addToolWindow(
      ui->outputSig, ToolWindowManager::AreaReference(ToolWindowManager::RightOf,
                                                      ui->docking->areaOf(ui->inputSig), 0.5f));
  ui->docking->setToolWindowProperties(ui->outputSig, ToolWindowManager::HideCloseButton);

  ui->docking->setAllowFloatingWindow(false);
  ui->docking->setRubberBandLineWidth(50);

  QHBoxLayout *layout = new QHBoxLayout(this);
  layout->addWidget(ui->docking);

  m_Ctx->AddLogViewer(this);
}

ShaderViewer::~ShaderViewer()
{
  m_Ctx->RemoveLogViewer(this);
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

void ShaderViewer::disassembly_keyPressed(QKeyEvent *event)
{
}

void ShaderViewer::readonly_keyPressed(QKeyEvent *event)
{
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
                  m_Ctx->APIProps().localRenderer == eGraphicsAPI_OpenGL ? SCLEX_GLSL : SCLEX_HLSL);

  ret->setTabWidth(4);

  ret->setScrollWidth(1);
  ret->setScrollWidthTracking(true);

  ret->colourise(0, -1);

  ret->emptyUndoBuffer();

  return ret;
}
