/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016-2017 Baldur Karlsson
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

#include "GLPipelineStateViewer.h"
#include <float.h>
#include <QMouseEvent>
#include <QScrollBar>
#include "3rdparty/toolwindowmanager/ToolWindowManager.h"
#include "Code/Resources.h"
#include "PipelineStateViewer.h"
#include "ui_GLPipelineStateViewer.h"

Q_DECLARE_METATYPE(ResourceId);

struct VBIBTag
{
  VBIBTag() { offset = 0; }
  VBIBTag(ResourceId i, uint64_t offs)
  {
    id = i;
    offset = offs;
  }

  ResourceId id;
  uint64_t offset;
};

Q_DECLARE_METATYPE(VBIBTag);

struct ReadWriteTag
{
  ReadWriteTag()
  {
    bindPoint = 0;
    offset = size = 0;
  }
  ReadWriteTag(uint32_t b, ResourceId id, uint64_t offs, uint64_t sz)
  {
    bindPoint = b;
    ID = id;
    offset = offs;
    size = sz;
  }
  uint32_t bindPoint;
  ResourceId ID;
  uint64_t offset;
  uint64_t size;
};

Q_DECLARE_METATYPE(ReadWriteTag);

GLPipelineStateViewer::GLPipelineStateViewer(ICaptureContext &ctx, PipelineStateViewer &common,
                                             QWidget *parent)
    : QFrame(parent), ui(new Ui::GLPipelineStateViewer), m_Ctx(ctx), m_Common(common)
{
  ui->setupUi(this);

  const QIcon &action = Icons::action();
  const QIcon &action_hover = Icons::action_hover();

  RDLabel *shaderLabels[] = {
      ui->vsShader, ui->tcsShader, ui->tesShader, ui->gsShader, ui->fsShader, ui->csShader,
  };

  QToolButton *viewButtons[] = {
      ui->vsShaderViewButton, ui->tcsShaderViewButton, ui->tesShaderViewButton,
      ui->gsShaderViewButton, ui->fsShaderViewButton,  ui->csShaderViewButton,
  };

  QToolButton *editButtons[] = {
      ui->vsShaderEditButton, ui->tcsShaderEditButton, ui->tesShaderEditButton,
      ui->gsShaderEditButton, ui->fsShaderEditButton,  ui->csShaderEditButton,
  };

  QToolButton *saveButtons[] = {
      ui->vsShaderSaveButton, ui->tcsShaderSaveButton, ui->tesShaderSaveButton,
      ui->gsShaderSaveButton, ui->fsShaderSaveButton,  ui->csShaderSaveButton,
  };

  RDTreeWidget *textures[] = {
      ui->vsTextures, ui->tcsTextures, ui->tesTextures,
      ui->gsTextures, ui->fsTextures,  ui->csTextures,
  };

  RDTreeWidget *samplers[] = {
      ui->vsSamplers, ui->tcsSamplers, ui->tesSamplers,
      ui->gsSamplers, ui->fsSamplers,  ui->csSamplers,
  };

  RDTreeWidget *ubos[] = {
      ui->vsUBOs, ui->tcsUBOs, ui->tesUBOs, ui->gsUBOs, ui->fsUBOs, ui->csUBOs,
  };

  RDTreeWidget *subroutines[] = {
      ui->vsSubroutines, ui->tcsSubroutines, ui->tesSubroutines,
      ui->gsSubroutines, ui->fsSubroutines,  ui->csSubroutines,
  };

  RDTreeWidget *readwrites[] = {
      ui->vsReadWrite, ui->tcsReadWrite, ui->tesReadWrite,
      ui->gsReadWrite, ui->fsReadWrite,  ui->csReadWrite,
  };

  for(QToolButton *b : viewButtons)
    QObject::connect(b, &QToolButton::clicked, this, &GLPipelineStateViewer::shaderView_clicked);

  for(RDLabel *b : shaderLabels)
    QObject::connect(b, &RDLabel::clicked, this, &GLPipelineStateViewer::shaderLabel_clicked);

  for(QToolButton *b : editButtons)
    QObject::connect(b, &QToolButton::clicked, this, &GLPipelineStateViewer::shaderEdit_clicked);

  for(QToolButton *b : saveButtons)
    QObject::connect(b, &QToolButton::clicked, this, &GLPipelineStateViewer::shaderSave_clicked);

  QObject::connect(ui->viAttrs, &RDTreeWidget::leave, this, &GLPipelineStateViewer::vertex_leave);
  QObject::connect(ui->viBuffers, &RDTreeWidget::leave, this, &GLPipelineStateViewer::vertex_leave);

  QObject::connect(ui->framebuffer, &RDTreeWidget::itemActivated, this,
                   &GLPipelineStateViewer::resource_itemActivated);

  for(RDTreeWidget *res : textures)
    QObject::connect(res, &RDTreeWidget::itemActivated, this,
                     &GLPipelineStateViewer::resource_itemActivated);

  for(RDTreeWidget *ubo : ubos)
    QObject::connect(ubo, &RDTreeWidget::itemActivated, this,
                     &GLPipelineStateViewer::ubo_itemActivated);

  for(RDTreeWidget *res : readwrites)
    QObject::connect(res, &RDTreeWidget::itemActivated, this,
                     &GLPipelineStateViewer::resource_itemActivated);

  addGridLines(ui->rasterizerGridLayout);
  addGridLines(ui->MSAAGridLayout);
  addGridLines(ui->blendStateGridLayout);
  addGridLines(ui->depthStateGridLayout);

  {
    ui->viAttrs->setColumns({tr("Index"), tr("Enabled"), tr("Name"), tr("Format/Generic Value"),
                             tr("Buffer Slot"), tr("Relative Offset"), tr("Go")});
    ui->viAttrs->header()->resizeSection(0, 75);
    ui->viAttrs->header()->setSectionResizeMode(0, QHeaderView::Interactive);
    ui->viAttrs->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    ui->viAttrs->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    ui->viAttrs->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    ui->viAttrs->header()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    ui->viAttrs->header()->setSectionResizeMode(5, QHeaderView::Stretch);
    ui->viAttrs->header()->setSectionResizeMode(6, QHeaderView::ResizeToContents);

    ui->viAttrs->setClearSelectionOnFocusLoss(true);
    ui->viAttrs->setHoverIconColumn(6, action, action_hover);
  }

  {
    ui->viBuffers->setColumns({tr("Slot"), tr("Buffer"), tr("Stride"), tr("Offset"), tr("Divisor"),
                               tr("Byte Length"), tr("Go")});
    ui->viBuffers->header()->resizeSection(0, 75);
    ui->viBuffers->header()->setSectionResizeMode(0, QHeaderView::Interactive);
    ui->viBuffers->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    ui->viBuffers->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    ui->viBuffers->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    ui->viBuffers->header()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    ui->viBuffers->header()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    ui->viBuffers->header()->setSectionResizeMode(6, QHeaderView::ResizeToContents);

    ui->viBuffers->setClearSelectionOnFocusLoss(true);
    ui->viBuffers->setHoverIconColumn(6, action, action_hover);
  }

  for(RDTreeWidget *tex : textures)
  {
    tex->setColumns({tr("Slot"), tr("Resource"), tr("Type"), tr("Width"), tr("Height"), tr("Depth"),
                     tr("Array Size"), tr("Format"), tr("Go")});
    tex->header()->resizeSection(0, 120);
    tex->header()->setSectionResizeMode(0, QHeaderView::Interactive);
    tex->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    tex->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    tex->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    tex->header()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    tex->header()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    tex->header()->setSectionResizeMode(6, QHeaderView::ResizeToContents);
    tex->header()->setSectionResizeMode(7, QHeaderView::ResizeToContents);
    tex->header()->setSectionResizeMode(8, QHeaderView::ResizeToContents);

    tex->setHoverIconColumn(8, action, action_hover);
    tex->setClearSelectionOnFocusLoss(true);
  }

  for(RDTreeWidget *samp : samplers)
  {
    samp->setColumns({tr("Slot"), tr("Addressing"), tr("Filter"), tr("LOD Clamp"), tr("LOD Bias")});
    samp->header()->resizeSection(0, 120);
    samp->header()->setSectionResizeMode(0, QHeaderView::Interactive);
    samp->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    samp->header()->setSectionResizeMode(2, QHeaderView::Stretch);
    samp->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    samp->header()->setSectionResizeMode(4, QHeaderView::ResizeToContents);

    samp->setClearSelectionOnFocusLoss(true);
  }

  for(RDTreeWidget *ubo : ubos)
  {
    ubo->setColumns({tr("Slot"), tr("Buffer"), tr("Byte Range"), tr("Size"), tr("Go")});
    ubo->header()->resizeSection(0, 120);
    ubo->header()->setSectionResizeMode(0, QHeaderView::Interactive);
    ubo->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    ubo->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    ubo->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    ubo->header()->setSectionResizeMode(4, QHeaderView::ResizeToContents);

    ubo->setHoverIconColumn(4, action, action_hover);
    ubo->setClearSelectionOnFocusLoss(true);
  }

  for(RDTreeWidget *sub : subroutines)
  {
    sub->setColumns({tr("Uniform"), tr("Value")});
    sub->header()->resizeSection(0, 120);
    sub->header()->setSectionResizeMode(0, QHeaderView::Interactive);
    sub->header()->setSectionResizeMode(1, QHeaderView::Stretch);

    sub->setClearSelectionOnFocusLoss(true);
  }

  for(RDTreeWidget *ubo : readwrites)
  {
    ubo->setColumns({tr("Binding"), tr("Slot"), tr("Resource"), tr("Dimensions"), tr("Format"),
                     tr("Access"), tr("Go")});
    ubo->header()->resizeSection(1, 120);
    ubo->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    ubo->header()->setSectionResizeMode(1, QHeaderView::Interactive);
    ubo->header()->setSectionResizeMode(2, QHeaderView::Stretch);
    ubo->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    ubo->header()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    ubo->header()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    ubo->header()->setSectionResizeMode(6, QHeaderView::ResizeToContents);

    ubo->setHoverIconColumn(6, action, action_hover);
    ubo->setClearSelectionOnFocusLoss(true);
  }

  {
    ui->viewports->setColumns(
        {tr("Slot"), tr("X"), tr("Y"), tr("Width"), tr("Height"), tr("MinDepth"), tr("MaxDepth")});
    ui->viewports->header()->resizeSection(0, 75);
    ui->viewports->header()->setSectionResizeMode(0, QHeaderView::Interactive);
    ui->viewports->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    ui->viewports->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    ui->viewports->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    ui->viewports->header()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    ui->viewports->header()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    ui->viewports->header()->setSectionResizeMode(6, QHeaderView::ResizeToContents);

    ui->viewports->setClearSelectionOnFocusLoss(true);
  }

  {
    ui->scissors->setColumns(
        {tr("Slot"), tr("X"), tr("Y"), tr("Width"), tr("Height"), tr("Enabled")});
    ui->scissors->header()->resizeSection(0, 100);
    ui->scissors->header()->setSectionResizeMode(0, QHeaderView::Interactive);
    ui->scissors->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    ui->scissors->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    ui->scissors->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    ui->scissors->header()->setSectionResizeMode(4, QHeaderView::Stretch);
    ui->scissors->header()->setSectionResizeMode(5, QHeaderView::ResizeToContents);

    ui->scissors->setClearSelectionOnFocusLoss(true);
  }

  {
    ui->framebuffer->setColumns({tr("Slot"), tr("Resource"), tr("Type"), tr("Width"), tr("Height"),
                                 tr("Depth"), tr("Array Size"), tr("Format"), tr("Go")});
    ui->framebuffer->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    ui->framebuffer->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    ui->framebuffer->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    ui->framebuffer->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    ui->framebuffer->header()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    ui->framebuffer->header()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    ui->framebuffer->header()->setSectionResizeMode(6, QHeaderView::ResizeToContents);
    ui->framebuffer->header()->setSectionResizeMode(7, QHeaderView::ResizeToContents);
    ui->framebuffer->header()->setSectionResizeMode(8, QHeaderView::ResizeToContents);

    ui->framebuffer->setHoverIconColumn(8, action, action_hover);
    ui->framebuffer->setClearSelectionOnFocusLoss(true);
  }

  {
    ui->blends->setColumns({tr("Slot"), tr("Enabled"), tr("Col Src"), tr("Col Dst"), tr("Col Op"),
                            tr("Alpha Src"), tr("Alpha Dst"), tr("Alpha Op"), tr("Write Mask")});
    ui->blends->header()->resizeSection(0, 75);
    ui->blends->header()->setSectionResizeMode(0, QHeaderView::Interactive);
    ui->blends->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    ui->blends->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    ui->blends->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    ui->blends->header()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    ui->blends->header()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    ui->blends->header()->setSectionResizeMode(6, QHeaderView::ResizeToContents);
    ui->blends->header()->setSectionResizeMode(7, QHeaderView::ResizeToContents);
    ui->blends->header()->setSectionResizeMode(8, QHeaderView::ResizeToContents);

    ui->blends->setClearSelectionOnFocusLoss(true);
  }

  {
    ui->stencils->setColumns({tr("Face"), tr("Func"), tr("Fail Op"), tr("Depth Fail Op"),
                              tr("Pass Op"), tr("Write Mask"), tr("Comp Mask"), tr("Ref")});
    ui->stencils->header()->resizeSection(0, 50);
    ui->stencils->header()->setSectionResizeMode(0, QHeaderView::Interactive);
    ui->stencils->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    ui->stencils->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    ui->stencils->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    ui->stencils->header()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    ui->stencils->header()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    ui->stencils->header()->setSectionResizeMode(6, QHeaderView::ResizeToContents);
    ui->stencils->header()->setSectionResizeMode(7, QHeaderView::Stretch);

    ui->stencils->setClearSelectionOnFocusLoss(true);
  }

  // this is often changed just because we're changing some tab in the designer.
  ui->stagesTabs->setCurrentIndex(0);

  ui->stagesTabs->tabBar()->setVisible(false);

  ui->pipeFlow->setStages(
      {
          lit("VTX"), lit("VS"), lit("TCS"), lit("TES"), lit("GS"), lit("RS"), lit("FS"), lit("FB"),
          lit("CS"),
      },
      {
          tr("Vertex Input"), tr("Vertex Shader"), tr("Tess. Control Shader"),
          tr("Tess. Eval. Shader"), tr("Geometry Shader"), tr("Rasterizer"), tr("Fragment Shader"),
          tr("Framebuffer Output"), tr("Compute Shader"),
      });

  ui->pipeFlow->setIsolatedStage(8);    // compute shader isolated

  ui->pipeFlow->setStagesEnabled({true, true, true, true, true, true, true, true, true});

  ui->viAttrs->setFont(Formatter::PreferredFont());
  ui->viBuffers->setFont(Formatter::PreferredFont());
  ui->gsFeedback->setFont(Formatter::PreferredFont());
  ui->vsShader->setFont(Formatter::PreferredFont());
  ui->vsTextures->setFont(Formatter::PreferredFont());
  ui->vsSamplers->setFont(Formatter::PreferredFont());
  ui->vsUBOs->setFont(Formatter::PreferredFont());
  ui->vsSubroutines->setFont(Formatter::PreferredFont());
  ui->vsReadWrite->setFont(Formatter::PreferredFont());
  ui->gsShader->setFont(Formatter::PreferredFont());
  ui->gsTextures->setFont(Formatter::PreferredFont());
  ui->gsSamplers->setFont(Formatter::PreferredFont());
  ui->gsUBOs->setFont(Formatter::PreferredFont());
  ui->gsSubroutines->setFont(Formatter::PreferredFont());
  ui->gsReadWrite->setFont(Formatter::PreferredFont());
  ui->tcsShader->setFont(Formatter::PreferredFont());
  ui->tcsTextures->setFont(Formatter::PreferredFont());
  ui->tcsSamplers->setFont(Formatter::PreferredFont());
  ui->tcsUBOs->setFont(Formatter::PreferredFont());
  ui->tcsSubroutines->setFont(Formatter::PreferredFont());
  ui->tcsReadWrite->setFont(Formatter::PreferredFont());
  ui->tesShader->setFont(Formatter::PreferredFont());
  ui->tesTextures->setFont(Formatter::PreferredFont());
  ui->tesSamplers->setFont(Formatter::PreferredFont());
  ui->tesUBOs->setFont(Formatter::PreferredFont());
  ui->tesSubroutines->setFont(Formatter::PreferredFont());
  ui->tesReadWrite->setFont(Formatter::PreferredFont());
  ui->fsShader->setFont(Formatter::PreferredFont());
  ui->fsTextures->setFont(Formatter::PreferredFont());
  ui->fsSamplers->setFont(Formatter::PreferredFont());
  ui->fsUBOs->setFont(Formatter::PreferredFont());
  ui->fsSubroutines->setFont(Formatter::PreferredFont());
  ui->fsReadWrite->setFont(Formatter::PreferredFont());
  ui->csShader->setFont(Formatter::PreferredFont());
  ui->csTextures->setFont(Formatter::PreferredFont());
  ui->csSamplers->setFont(Formatter::PreferredFont());
  ui->csUBOs->setFont(Formatter::PreferredFont());
  ui->csSubroutines->setFont(Formatter::PreferredFont());
  ui->csReadWrite->setFont(Formatter::PreferredFont());
  ui->viewports->setFont(Formatter::PreferredFont());
  ui->scissors->setFont(Formatter::PreferredFont());
  ui->framebuffer->setFont(Formatter::PreferredFont());
  ui->blends->setFont(Formatter::PreferredFont());

  // reset everything back to defaults
  clearState();
}

GLPipelineStateViewer::~GLPipelineStateViewer()
{
  delete ui;
}

void GLPipelineStateViewer::OnLogfileLoaded()
{
  OnEventChanged(m_Ctx.CurEvent());
}

void GLPipelineStateViewer::OnLogfileClosed()
{
  ui->pipeFlow->setStagesEnabled({true, true, true, true, true, true, true, true, true});

  clearState();
}

void GLPipelineStateViewer::OnEventChanged(uint32_t eventID)
{
  setState();
}

void GLPipelineStateViewer::on_showDisabled_toggled(bool checked)
{
  setState();
}

void GLPipelineStateViewer::on_showEmpty_toggled(bool checked)
{
  setState();
}

void GLPipelineStateViewer::setInactiveRow(RDTreeWidgetItem *node)
{
  node->setItalic(true);
}

void GLPipelineStateViewer::setEmptyRow(RDTreeWidgetItem *node)
{
  node->setBackgroundColor(QColor(255, 70, 70));
  node->setForegroundColor(QColor(0, 0, 0));
}

bool GLPipelineStateViewer::showNode(bool usedSlot, bool filledSlot)
{
  const bool showDisabled = ui->showDisabled->isChecked();
  const bool showEmpty = ui->showEmpty->isChecked();

  // show if it's referenced by the shader - regardless of empty or not
  if(usedSlot)
    return true;

  // it's bound, but not referenced, and we have "show disabled"
  if(showDisabled && !usedSlot && filledSlot)
    return true;

  // it's empty, and we have "show empty"
  if(showEmpty && !filledSlot)
    return true;

  return false;
}

const GLPipe::Shader *GLPipelineStateViewer::stageForSender(QWidget *widget)
{
  if(!m_Ctx.LogLoaded())
    return NULL;

  while(widget)
  {
    if(widget == ui->stagesTabs->widget(0))
      return &m_Ctx.CurGLPipelineState().m_VS;
    if(widget == ui->stagesTabs->widget(1))
      return &m_Ctx.CurGLPipelineState().m_VS;
    if(widget == ui->stagesTabs->widget(2))
      return &m_Ctx.CurGLPipelineState().m_TCS;
    if(widget == ui->stagesTabs->widget(3))
      return &m_Ctx.CurGLPipelineState().m_TES;
    if(widget == ui->stagesTabs->widget(4))
      return &m_Ctx.CurGLPipelineState().m_GS;
    if(widget == ui->stagesTabs->widget(5))
      return &m_Ctx.CurGLPipelineState().m_FS;
    if(widget == ui->stagesTabs->widget(6))
      return &m_Ctx.CurGLPipelineState().m_FS;
    if(widget == ui->stagesTabs->widget(7))
      return &m_Ctx.CurGLPipelineState().m_FS;
    if(widget == ui->stagesTabs->widget(8))
      return &m_Ctx.CurGLPipelineState().m_CS;

    widget = widget->parentWidget();
  }

  qCritical() << "Unrecognised control calling event handler";

  return NULL;
}

void GLPipelineStateViewer::clearShaderState(QLabel *shader, RDTreeWidget *tex, RDTreeWidget *samp,
                                             RDTreeWidget *ubo, RDTreeWidget *sub, RDTreeWidget *rw)
{
  shader->setText(tr("Unbound Shader"));
  tex->clear();
  samp->clear();
  sub->clear();
  ubo->clear();
  rw->clear();
}

void GLPipelineStateViewer::clearState()
{
  m_VBNodes.clear();

  ui->viAttrs->clear();
  ui->viBuffers->clear();
  ui->topology->setText(QString());
  ui->primRestart->setVisible(false);
  ui->topologyDiagram->setPixmap(QPixmap());

  clearShaderState(ui->vsShader, ui->vsTextures, ui->vsSamplers, ui->vsUBOs, ui->vsSubroutines,
                   ui->vsReadWrite);
  clearShaderState(ui->gsShader, ui->gsTextures, ui->gsSamplers, ui->gsUBOs, ui->gsSubroutines,
                   ui->gsReadWrite);
  clearShaderState(ui->tcsShader, ui->tcsTextures, ui->tcsSamplers, ui->tcsUBOs, ui->tcsSubroutines,
                   ui->tcsReadWrite);
  clearShaderState(ui->tesShader, ui->tesTextures, ui->tesSamplers, ui->tesUBOs, ui->tesSubroutines,
                   ui->tesReadWrite);
  clearShaderState(ui->fsShader, ui->fsTextures, ui->fsSamplers, ui->fsUBOs, ui->fsSubroutines,
                   ui->fsReadWrite);
  clearShaderState(ui->csShader, ui->csTextures, ui->csSamplers, ui->csUBOs, ui->csSubroutines,
                   ui->csReadWrite);

  const QPixmap &tick = Pixmaps::tick();
  const QPixmap &cross = Pixmaps::cross();

  ui->fillMode->setText(tr("Solid", "Fill Mode"));
  ui->cullMode->setText(tr("Front", "Cull Mode"));
  ui->frontCCW->setPixmap(tick);

  ui->scissorEnabled->setPixmap(tick);
  ui->provoking->setText(tr("Last"));
  ui->rasterizerDiscard->setPixmap(cross);

  ui->pointSize->setText(lit("1.0"));
  ui->lineWidth->setText(lit("1.0"));

  ui->clipSetup->setText(tr("0,0 Lower Left") + lit(", Z= -1 to 1"));
  ui->clipDistance->setText(lit("-"));

  ui->depthClamp->setPixmap(tick);
  ui->depthBias->setText(lit("0.0"));
  ui->slopeScaledBias->setText(lit("0.0"));
  ui->offsetClamp->setText(QString());
  ui->offsetClamp->setPixmap(cross);

  ui->multisample->setPixmap(tick);
  ui->sampleShading->setPixmap(tick);
  ui->minSampleShading->setText(lit("0.0"));
  ui->alphaToOne->setPixmap(tick);
  ui->alphaToCoverage->setPixmap(tick);

  ui->sampleCoverage->setText(QString());
  ui->sampleCoverage->setPixmap(cross);
  ui->sampleMask->setText(QString());
  ui->sampleMask->setPixmap(cross);

  ui->viewports->clear();
  ui->scissors->clear();

  ui->framebuffer->clear();
  ui->blends->clear();

  ui->blendFactor->setText(lit("0.00, 0.00, 0.00, 0.00"));

  ui->depthEnabled->setPixmap(tick);
  ui->depthFunc->setText(lit("GREATER_EQUAL"));
  ui->depthWrite->setPixmap(tick);

  ui->depthBounds->setText(lit("0.0-1.0"));
  ui->depthBounds->setPixmap(QPixmap());

  ui->stencils->clear();
}

void GLPipelineStateViewer::setShaderState(const GLPipe::Shader &stage, QLabel *shader,
                                           RDTreeWidget *textures, RDTreeWidget *samplers,
                                           RDTreeWidget *ubos, RDTreeWidget *subs,
                                           RDTreeWidget *readwrites)
{
  ShaderReflection *shaderDetails = stage.ShaderDetails;
  const ShaderBindpointMapping &mapping = stage.BindpointMapping;
  const GLPipe::State &state = m_Ctx.CurGLPipelineState();

  if(stage.Object == ResourceId())
  {
    shader->setText(tr("Unbound Shader"));
  }
  else
  {
    QString shaderName = ToQStr(stage.stage, GraphicsAPI::OpenGL) + lit(" Shader");

    if(!stage.customShaderName && !stage.customProgramName && !stage.customPipelineName)
    {
      shader->setText(shaderName + lit(" ") + ToQStr(stage.Object));
    }
    else
    {
      if(stage.customShaderName)
        shaderName = ToQStr(stage.ShaderName);

      if(stage.customProgramName)
        shaderName = ToQStr(stage.ProgramName) + lit(" - ") + shaderName;

      if(stage.customPipelineName && stage.PipelineActive)
        shaderName = ToQStr(stage.PipelineName) + lit(" - ") + shaderName;

      shader->setText(shaderName);
    }
  }

  int vs = 0;
  int vs2 = 0;

  // simultaneous update of resources and samplers
  vs = textures->verticalScrollBar()->value();
  textures->setUpdatesEnabled(false);
  textures->clear();
  vs2 = samplers->verticalScrollBar()->value();
  samplers->setUpdatesEnabled(false);
  samplers->clear();

  for(int i = 0; i < state.Textures.count; i++)
  {
    const GLPipe::Texture &r = state.Textures[i];
    const GLPipe::Sampler &s = state.Samplers[i];

    const ShaderResource *shaderInput = NULL;
    const BindpointMap *map = NULL;

    if(shaderDetails)
    {
      for(const ShaderResource &bind : shaderDetails->ReadOnlyResources)
      {
        if(bind.IsReadOnly && mapping.ReadOnlyResources[bind.bindPoint].bind == i)
        {
          shaderInput = &bind;
          map = &mapping.ReadOnlyResources[bind.bindPoint];
        }
      }
    }

    bool filledSlot = (r.Resource != ResourceId());
    bool usedSlot = (shaderInput && map && map->used);

    if(showNode(usedSlot, filledSlot))
    {
      // do texture
      {
        QString slotname = QString::number(i);

        if(shaderInput && !shaderInput->name.empty())
          slotname += lit(": ") + ToQStr(shaderInput->name);

        uint32_t w = 1, h = 1, d = 1;
        uint32_t a = 1;
        QString format = lit("Unknown");
        QString name = tr("Shader Resource %1").arg(ToQStr(r.Resource));
        QString typeName = lit("Unknown");

        if(!filledSlot)
        {
          name = lit("Empty");
          format = lit("-");
          typeName = lit("-");
          w = h = d = a = 0;
        }

        TextureDescription *tex = m_Ctx.GetTexture(r.Resource);

        if(tex)
        {
          w = tex->width;
          h = tex->height;
          d = tex->depth;
          a = tex->arraysize;
          format = ToQStr(tex->format.strname);
          name = ToQStr(tex->name);
          typeName = ToQStr(tex->resType);

          if(tex->format.special && (tex->format.specialFormat == SpecialFormat::D16S8 ||
                                     tex->format.specialFormat == SpecialFormat::D24S8 ||
                                     tex->format.specialFormat == SpecialFormat::D32S8))
          {
            if(r.DepthReadChannel == 0)
              format += tr(" Depth-Read");
            else if(r.DepthReadChannel == 1)
              format += tr(" Stencil-Read");
          }
          else if(r.Swizzle[0] != TextureSwizzle::Red || r.Swizzle[1] != TextureSwizzle::Green ||
                  r.Swizzle[2] != TextureSwizzle::Blue || r.Swizzle[3] != TextureSwizzle::Alpha)
          {
            format += tr(" swizzle[%1%2%3%4]")
                          .arg(ToQStr(r.Swizzle[0]))
                          .arg(ToQStr(r.Swizzle[1]))
                          .arg(ToQStr(r.Swizzle[2]))
                          .arg(ToQStr(r.Swizzle[3]));
          }
        }

        RDTreeWidgetItem *node =
            new RDTreeWidgetItem({slotname, name, typeName, w, h, d, a, format, QString()});

        node->setTag(QVariant::fromValue(r.Resource));

        if(!filledSlot)
          setEmptyRow(node);

        if(!usedSlot)
          setInactiveRow(node);

        textures->addTopLevelItem(node);
      }

      // do sampler
      {
        QString slotname = QString::number(i);

        if(shaderInput && !shaderInput->name.empty())
          slotname += lit(": ") + ToQStr(shaderInput->name);

        QString borderColor = QFormatStr("%1, %2, %3, %4")
                                  .arg(s.BorderColor[0])
                                  .arg(s.BorderColor[1])
                                  .arg(s.BorderColor[2])
                                  .arg(s.BorderColor[3]);

        QString addressing;

        QString addPrefix;
        QString addVal;

        QString addr[] = {ToQStr(s.AddressS), ToQStr(s.AddressT), ToQStr(s.AddressR)};

        // arrange like either STR: WRAP or ST: WRAP, R: CLAMP
        for(int a = 0; a < 3; a++)
        {
          const QString str[] = {lit("S"), lit("T"), lit("R")};
          QString prefix = str[a];

          if(a == 0 || addr[a] == addr[a - 1])
          {
            addPrefix += prefix;
          }
          else
          {
            addressing += QFormatStr("%1: %2, ").arg(addPrefix).arg(addVal);

            addPrefix = prefix;
          }
          addVal = addr[a];
        }

        addressing += addPrefix + lit(": ") + addVal;

        if(s.UseBorder())
          addressing += QFormatStr("<%1>").arg(borderColor);

        if(r.ResType == TextureDim::TextureCube || r.ResType == TextureDim::TextureCubeArray)
        {
          addressing += s.SeamlessCube ? tr(" Seamless") : tr(" Non-Seamless");
        }

        QString filter = ToQStr(s.Filter);

        if(s.MaxAniso > 1)
          filter += lit(" Aniso%1x").arg(s.MaxAniso);

        if(s.Filter.func == FilterFunc::Comparison)
          filter += QFormatStr(" (%1)").arg(ToQStr(s.Comparison));
        else if(s.Filter.func != FilterFunc::Normal)
          filter += QFormatStr(" (%1)").arg(ToQStr(s.Filter.func));

        RDTreeWidgetItem *node = new RDTreeWidgetItem(
            {slotname, addressing, filter,
             QFormatStr("%1 - %2")
                 .arg(s.MinLOD == -FLT_MAX ? lit("0") : QString::number(s.MinLOD))
                 .arg(s.MaxLOD == FLT_MAX ? lit("FLT_MAX") : QString::number(s.MaxLOD)),
             s.MipLODBias});

        if(!filledSlot)
          setEmptyRow(node);

        if(!usedSlot)
          setInactiveRow(node);

        samplers->addTopLevelItem(node);
      }
    }
  }

  samplers->clearSelection();
  samplers->setUpdatesEnabled(true);
  samplers->verticalScrollBar()->setValue(vs2);
  textures->clearSelection();
  textures->setUpdatesEnabled(true);
  textures->verticalScrollBar()->setValue(vs);

  vs = ubos->verticalScrollBar()->value();
  ubos->setUpdatesEnabled(false);
  ubos->clear();
  for(int i = 0; shaderDetails && i < shaderDetails->ConstantBlocks.count; i++)
  {
    const ConstantBlock &shaderCBuf = shaderDetails->ConstantBlocks[i];
    int bindPoint = stage.BindpointMapping.ConstantBlocks[i].bind;

    const GLPipe::Buffer *b = NULL;

    if(bindPoint >= 0 && bindPoint < state.UniformBuffers.count)
      b = &state.UniformBuffers[bindPoint];

    bool filledSlot = !shaderCBuf.bufferBacked || (b && b->Resource != ResourceId());
    bool usedSlot = stage.BindpointMapping.ConstantBlocks[i].used;

    if(showNode(usedSlot, filledSlot))
    {
      ulong offset = 0;
      ulong length = 0;
      int numvars = shaderCBuf.variables.count;
      ulong byteSize = (ulong)shaderCBuf.byteSize;

      QString slotname = tr("Uniforms");
      QString name;
      QString sizestr = tr("%1 Variables").arg(numvars);
      QString byterange;

      if(!filledSlot)
      {
        name = tr("Empty");
        length = 0;
      }

      if(b)
      {
        slotname = QFormatStr("%1: %2").arg(bindPoint).arg(ToQStr(shaderCBuf.name));
        name = lit("UBO ") + ToQStr(b->Resource);
        offset = b->Offset;
        length = b->Size;

        BufferDescription *buf = m_Ctx.GetBuffer(b->Resource);
        if(buf)
        {
          name = ToQStr(buf->name);
          if(length == 0)
            length = buf->length;
        }

        if(length == byteSize)
          sizestr = tr("%1 Variables, %2 bytes").arg(numvars).arg(length);
        else
          sizestr =
              tr("%1 Variables, %2 bytes needed, %3 provided").arg(numvars).arg(byteSize).arg(length);

        if(length < byteSize)
          filledSlot = false;

        byterange = QFormatStr("%1 - %2").arg(offset).arg(offset + length);
      }

      RDTreeWidgetItem *node = new RDTreeWidgetItem({slotname, name, byterange, sizestr, QString()});

      node->setTag(QVariant::fromValue(i));

      if(!filledSlot)
        setEmptyRow(node);

      if(!usedSlot)
        setInactiveRow(node);

      ubos->addTopLevelItem(node);
    }
  }
  ubos->clearSelection();
  ubos->setUpdatesEnabled(true);
  ubos->verticalScrollBar()->setValue(vs);

  vs = subs->verticalScrollBar()->value();
  subs->setUpdatesEnabled(false);
  subs->clear();
  for(int i = 0; i < stage.Subroutines.count; i++)
    subs->addTopLevelItem(new RDTreeWidgetItem({i, stage.Subroutines[i]}));
  subs->clearSelection();
  subs->setUpdatesEnabled(true);
  subs->verticalScrollBar()->setValue(vs);

  subs->parentWidget()->setVisible(!stage.Subroutines.empty());

  vs = readwrites->verticalScrollBar()->value();
  readwrites->setUpdatesEnabled(false);
  readwrites->clear();
  for(int i = 0; shaderDetails && i < shaderDetails->ReadWriteResources.count; i++)
  {
    const ShaderResource &res = shaderDetails->ReadWriteResources[i];
    int bindPoint = stage.BindpointMapping.ReadWriteResources[i].bind;

    GLReadWriteType readWriteType = GetGLReadWriteType(res);

    const GLPipe::Buffer *bf = NULL;
    const GLPipe::ImageLoadStore *im = NULL;
    ResourceId id;

    if(readWriteType == GLReadWriteType::Image && bindPoint >= 0 && bindPoint < state.Images.count)
    {
      im = &state.Images[bindPoint];
      id = state.Images[bindPoint].Resource;
    }

    if(readWriteType == GLReadWriteType::Atomic && bindPoint >= 0 &&
       bindPoint < state.AtomicBuffers.count)
    {
      bf = &state.AtomicBuffers[bindPoint];
      id = state.AtomicBuffers[bindPoint].Resource;
    }

    if(readWriteType == GLReadWriteType::SSBO && bindPoint >= 0 &&
       bindPoint < state.ShaderStorageBuffers.count)
    {
      bf = &state.ShaderStorageBuffers[bindPoint];
      id = state.ShaderStorageBuffers[bindPoint].Resource;
    }

    bool filledSlot = id != ResourceId();
    bool usedSlot = stage.BindpointMapping.ReadWriteResources[i].used;

    if(showNode(usedSlot, filledSlot))
    {
      QString binding =
          readWriteType == GLReadWriteType::Image
              ? tr("Image")
              : readWriteType == GLReadWriteType::Atomic
                    ? tr("Atomic")
                    : readWriteType == GLReadWriteType::SSBO ? tr("SSBO") : tr("Unknown");

      QString slotname = QFormatStr("%1: %2").arg(bindPoint).arg(ToQStr(res.name));
      QString name;
      QString dimensions;
      QString format = lit("-");
      QString access = tr("Read/Write");
      if(im)
      {
        if(im->readAllowed && !im->writeAllowed)
          access = tr("Read-Only");
        if(!im->readAllowed && im->writeAllowed)
          access = tr("Write-Only");
        format = ToQStr(im->Format.strname);
      }

      QVariant tag;

      TextureDescription *tex = m_Ctx.GetTexture(id);

      if(tex)
      {
        if(tex->dimension == 1)
        {
          if(tex->arraysize > 1)
            dimensions = QFormatStr("%1[%2]").arg(tex->width).arg(tex->arraysize);
          else
            dimensions = QFormatStr("%1").arg(tex->width);
        }
        else if(tex->dimension == 2)
        {
          if(tex->arraysize > 1)
            dimensions = QFormatStr("%1x%2[%3]").arg(tex->width).arg(tex->height).arg(tex->arraysize);
          else
            dimensions = QFormatStr("%1x%2").arg(tex->width).arg(tex->height);
        }
        else if(tex->dimension == 3)
        {
          dimensions = QFormatStr("%1x%2x%3").arg(tex->width).arg(tex->height).arg(tex->depth);
        }

        name = ToQStr(tex->name);

        tag = QVariant::fromValue(id);
      }

      BufferDescription *buf = m_Ctx.GetBuffer(id);

      if(buf)
      {
        uint64_t offset = 0;
        uint64_t length = buf->length;
        if(bf && bf->Size > 0)
        {
          offset = bf->Offset;
          length = bf->Size;
        }

        if(offset > 0)
          dimensions = tr("%1 bytes at offset %2 bytes").arg(length).arg(offset);
        else
          dimensions = tr("%1 bytes").arg(length);

        name = ToQStr(buf->name);

        tag = QVariant::fromValue(ReadWriteTag(i, id, offset, length));
      }

      if(!filledSlot)
      {
        name = tr("Empty");
        dimensions = lit("-");
        access = lit("-");
      }

      RDTreeWidgetItem *node =
          new RDTreeWidgetItem({binding, slotname, name, dimensions, format, access, QString()});

      node->setTag(tag);

      if(!filledSlot)
        setEmptyRow(node);

      if(!usedSlot)
        setInactiveRow(node);

      readwrites->addTopLevelItem(node);
    }
  }
  readwrites->clearSelection();
  readwrites->setUpdatesEnabled(true);
  readwrites->verticalScrollBar()->setValue(vs);

  readwrites->parentWidget()->setVisible(readwrites->invisibleRootItem()->childCount() > 0);
}

QString GLPipelineStateViewer::MakeGenericValueString(uint32_t compCount, CompType compType,
                                                      const GLPipe::VertexAttribute &val)
{
  QString ret;
  if(compCount == 1)
    ret = QFormatStr("<%1>");
  else if(compCount == 2)
    ret = QFormatStr("<%1, %2>");
  else if(compCount == 3)
    ret = QFormatStr("<%1, %2, %3>");
  else if(compCount == 4)
    ret = QFormatStr("<%1, %2, %3, %4>");

  if(compType == CompType::UInt)
  {
    for(uint32_t i = 0; i < compCount; i++)
      ret = ret.arg(val.GenericValue.value_u[i]);

    return ret;
  }
  else if(compType == CompType::SInt)
  {
    for(uint32_t i = 0; i < compCount; i++)
      ret = ret.arg(val.GenericValue.value_i[i]);

    return ret;
  }
  else
  {
    for(uint32_t i = 0; i < compCount; i++)
      ret = ret.arg(val.GenericValue.value_f[i]);

    return ret;
  }
}

GLPipelineStateViewer::GLReadWriteType GLPipelineStateViewer::GetGLReadWriteType(ShaderResource res)
{
  GLReadWriteType ret = GLReadWriteType::Image;

  if(res.IsTexture)
  {
    ret = GLReadWriteType::Image;
  }
  else
  {
    if(res.variableType.descriptor.rows == 1 && res.variableType.descriptor.cols == 1 &&
       res.variableType.descriptor.type == VarType::UInt)
    {
      ret = GLReadWriteType::Atomic;
    }
    else
    {
      ret = GLReadWriteType::SSBO;
    }
  }

  return ret;
}

void GLPipelineStateViewer::setState()
{
  if(!m_Ctx.LogLoaded())
  {
    clearState();
    return;
  }

  const GLPipe::State &state = m_Ctx.CurGLPipelineState();
  const DrawcallDescription *draw = m_Ctx.CurDrawcall();

  bool showDisabled = ui->showDisabled->isChecked();
  bool showEmpty = ui->showEmpty->isChecked();

  const QPixmap &tick = Pixmaps::tick();
  const QPixmap &cross = Pixmaps::cross();

  bool usedBindings[128] = {};

  ////////////////////////////////////////////////
  // Vertex Input

  int vs = 0;

  vs = ui->viAttrs->verticalScrollBar()->value();
  ui->viAttrs->setUpdatesEnabled(false);
  ui->viAttrs->clear();
  {
    int i = 0;
    for(const GLPipe::VertexAttribute &a : state.m_VtxIn.attributes)
    {
      bool filledSlot = true;
      bool usedSlot = false;

      QString name = tr("Attribute %1").arg(i);

      uint32_t compCount = 4;
      CompType compType = CompType::Float;

      if(state.m_VS.Object != ResourceId())
      {
        int attrib = -1;
        if(i < state.m_VS.BindpointMapping.InputAttributes.count)
          attrib = state.m_VS.BindpointMapping.InputAttributes[i];

        if(attrib >= 0 && attrib < state.m_VS.ShaderDetails->InputSig.count)
        {
          name = ToQStr(state.m_VS.ShaderDetails->InputSig[attrib].varName);
          compCount = state.m_VS.ShaderDetails->InputSig[attrib].compCount;
          compType = state.m_VS.ShaderDetails->InputSig[attrib].compType;
          usedSlot = true;
        }
      }

      if(showNode(usedSlot, filledSlot))
      {
        QString genericVal = tr("Generic=") + MakeGenericValueString(compCount, compType, a);

        RDTreeWidgetItem *node = new RDTreeWidgetItem(
            {i, a.Enabled ? tr("Enabled") : tr("Disabled"), name,
             a.Enabled ? ToQStr(a.Format.strname) : genericVal, a.BufferSlot, a.RelativeOffset});

        if(a.Enabled)
          usedBindings[a.BufferSlot] = true;

        if(!usedSlot)
          setInactiveRow(node);

        ui->viAttrs->addTopLevelItem(node);
      }

      i++;
    }
  }
  ui->viAttrs->clearSelection();
  ui->viAttrs->setUpdatesEnabled(true);
  ui->viAttrs->verticalScrollBar()->setValue(vs);

  Topology topo = draw ? draw->topology : Topology::Unknown;

  int numCPs = PatchList_Count(topo);
  if(numCPs > 0)
  {
    ui->topology->setText(tr("PatchList (%1 Control Points)").arg(numCPs));
  }
  else
  {
    ui->topology->setText(ToQStr(topo));
  }

  bool ibufferUsed = draw && (draw->flags & DrawFlags::UseIBuffer);

  if(ibufferUsed)
  {
    ui->primRestart->setVisible(true);
    if(state.m_VtxIn.primitiveRestart)
      ui->primRestart->setText(
          tr("Restart Idx: 0x%1").arg(state.m_VtxIn.restartIndex, 8, 16, QLatin1Char('0')).toUpper());
    else
      ui->primRestart->setText(tr("Restart Idx: Disabled"));
  }
  else
  {
    ui->primRestart->setVisible(false);
  }

  switch(topo)
  {
    case Topology::PointList: ui->topologyDiagram->setPixmap(Pixmaps::topo_pointlist()); break;
    case Topology::LineList: ui->topologyDiagram->setPixmap(Pixmaps::topo_linelist()); break;
    case Topology::LineStrip: ui->topologyDiagram->setPixmap(Pixmaps::topo_linestrip()); break;
    case Topology::TriangleList: ui->topologyDiagram->setPixmap(Pixmaps::topo_trilist()); break;
    case Topology::TriangleStrip: ui->topologyDiagram->setPixmap(Pixmaps::topo_tristrip()); break;
    case Topology::LineList_Adj:
      ui->topologyDiagram->setPixmap(Pixmaps::topo_linelist_adj());
      break;
    case Topology::LineStrip_Adj:
      ui->topologyDiagram->setPixmap(Pixmaps::topo_linestrip_adj());
      break;
    case Topology::TriangleList_Adj:
      ui->topologyDiagram->setPixmap(Pixmaps::topo_trilist_adj());
      break;
    case Topology::TriangleStrip_Adj:
      ui->topologyDiagram->setPixmap(Pixmaps::topo_tristrip_adj());
      break;
    default: ui->topologyDiagram->setPixmap(Pixmaps::topo_patch()); break;
  }

  vs = ui->viBuffers->verticalScrollBar()->value();
  ui->viBuffers->setUpdatesEnabled(false);
  ui->viBuffers->clear();

  if(state.m_VtxIn.ibuffer != ResourceId())
  {
    if(ibufferUsed || showDisabled)
    {
      QString name = tr("Buffer ") + ToQStr(state.m_VtxIn.ibuffer);
      uint64_t length = 1;

      if(!ibufferUsed)
        length = 0;

      BufferDescription *buf = m_Ctx.GetBuffer(state.m_VtxIn.ibuffer);

      if(buf)
      {
        name = ToQStr(buf->name);
        length = buf->length;
      }

      RDTreeWidgetItem *node =
          new RDTreeWidgetItem({tr("Element"), name, draw ? draw->indexByteWidth : 0, 0, 0,
                                (qulonglong)length, QString()});

      node->setTag(QVariant::fromValue(VBIBTag(state.m_VtxIn.ibuffer, draw ? draw->indexOffset : 0)));

      if(!ibufferUsed)
        setInactiveRow(node);

      if(state.m_VtxIn.ibuffer == ResourceId())
        setEmptyRow(node);

      ui->viBuffers->addTopLevelItem(node);
    }
  }
  else
  {
    if(ibufferUsed || showEmpty)
    {
      RDTreeWidgetItem *node = new RDTreeWidgetItem(
          {tr("Element"), tr("No Buffer Set"), lit("-"), lit("-"), lit("-"), lit("-"), QString()});

      node->setTag(QVariant::fromValue(VBIBTag(state.m_VtxIn.ibuffer, draw ? draw->indexOffset : 0)));

      setEmptyRow(node);

      if(!ibufferUsed)
        setInactiveRow(node);

      ui->viBuffers->addTopLevelItem(node);
    }
  }

  m_VBNodes.clear();

  for(int i = 0; i < state.m_VtxIn.vbuffers.count; i++)
  {
    const GLPipe::VB &v = state.m_VtxIn.vbuffers[i];

    bool filledSlot = (v.Buffer != ResourceId());
    bool usedSlot = (usedBindings[i]);

    if(showNode(usedSlot, filledSlot))
    {
      QString name = tr("Buffer ") + ToQStr(v.Buffer);
      uint64_t length = 1;
      uint64_t offset = v.Offset;

      if(!filledSlot)
      {
        name = tr("Empty");
        length = 0;
      }

      BufferDescription *buf = m_Ctx.GetBuffer(v.Buffer);
      if(buf)
      {
        name = ToQStr(buf->name);
        length = buf->length;
      }

      RDTreeWidgetItem *node = new RDTreeWidgetItem(
          {i, name, v.Stride, (qulonglong)offset, v.Divisor, (qulonglong)length, QString()});

      node->setTag(QVariant::fromValue(VBIBTag(v.Buffer, v.Offset)));

      if(!filledSlot)
        setEmptyRow(node);

      if(!usedSlot)
        setInactiveRow(node);

      m_VBNodes.push_back(node);

      ui->viBuffers->addTopLevelItem(node);
    }
  }
  ui->viBuffers->clearSelection();
  ui->viBuffers->setUpdatesEnabled(true);
  ui->viBuffers->verticalScrollBar()->setValue(vs);

  setShaderState(state.m_VS, ui->vsShader, ui->vsTextures, ui->vsSamplers, ui->vsUBOs,
                 ui->vsSubroutines, ui->vsReadWrite);
  setShaderState(state.m_GS, ui->gsShader, ui->gsTextures, ui->gsSamplers, ui->gsUBOs,
                 ui->gsSubroutines, ui->gsReadWrite);
  setShaderState(state.m_TCS, ui->tcsShader, ui->tcsTextures, ui->tcsSamplers, ui->tcsUBOs,
                 ui->tcsSubroutines, ui->tcsReadWrite);
  setShaderState(state.m_TES, ui->tesShader, ui->tesTextures, ui->tesSamplers, ui->tesUBOs,
                 ui->tesSubroutines, ui->tesReadWrite);
  setShaderState(state.m_FS, ui->fsShader, ui->fsTextures, ui->fsSamplers, ui->fsUBOs,
                 ui->fsSubroutines, ui->fsReadWrite);
  setShaderState(state.m_CS, ui->csShader, ui->csTextures, ui->csSamplers, ui->csUBOs,
                 ui->csSubroutines, ui->csReadWrite);

  vs = ui->gsFeedback->verticalScrollBar()->value();
  ui->gsFeedback->setUpdatesEnabled(false);
  ui->gsFeedback->clear();
  if(state.m_Feedback.Active)
  {
    ui->xfbPaused->setPixmap(state.m_Feedback.Paused ? tick : cross);
    for(int i = 0; i < (int)ARRAY_COUNT(state.m_Feedback.BufferBinding); i++)
    {
      bool filledSlot = (state.m_Feedback.BufferBinding[i] != ResourceId());
      bool usedSlot = (filledSlot);

      if(showNode(usedSlot, filledSlot))
      {
        QString name = tr("Buffer ") + ToQStr(state.m_Feedback.BufferBinding[i]);
        qulonglong length = state.m_Feedback.Size[i];

        if(!filledSlot)
        {
          name = tr("Empty");
        }

        BufferDescription *buf = m_Ctx.GetBuffer(state.m_Feedback.BufferBinding[i]);

        if(buf)
        {
          name = ToQStr(buf->name);
          if(length == 0)
            length = buf->length;
        }

        RDTreeWidgetItem *node = new RDTreeWidgetItem(
            {i, name, length, (qulonglong)state.m_Feedback.Offset[i], QString()});

        node->setTag(QVariant::fromValue(state.m_Feedback.BufferBinding[i]));

        if(!filledSlot)
          setEmptyRow(node);

        if(!usedSlot)
          setInactiveRow(node);

        ui->gsFeedback->addTopLevelItem(node);
      }
    }
  }
  ui->gsFeedback->verticalScrollBar()->setValue(vs);
  ui->gsFeedback->clearSelection();
  ui->gsFeedback->setUpdatesEnabled(true);

  ui->gsFeedback->setVisible(state.m_Feedback.Active);
  ui->xfbGroup->setVisible(state.m_Feedback.Active);

  ////////////////////////////////////////////////
  // Rasterizer

  vs = ui->viewports->verticalScrollBar()->value();
  ui->viewports->setUpdatesEnabled(false);
  ui->viewports->clear();

  {
    // accumulate identical viewports to save on visual repetition
    int prev = 0;
    for(int i = 0; i < state.m_Rasterizer.Viewports.count; i++)
    {
      const GLPipe::Viewport &v1 = state.m_Rasterizer.Viewports[prev];
      const GLPipe::Viewport &v2 = state.m_Rasterizer.Viewports[i];

      if(v1.Width != v2.Width || v1.Height != v2.Height || v1.Left != v2.Left ||
         v1.Bottom != v2.Bottom || v1.MinDepth != v2.MinDepth || v1.MaxDepth != v2.MaxDepth)
      {
        if(v1.Width != v1.Height || v1.Width != 0 || v1.Height != 0 || v1.MinDepth != v1.MaxDepth ||
           ui->showEmpty->isChecked())
        {
          QString indexstring;
          if(prev < i - 1)
            indexstring = QFormatStr("%1-%2").arg(prev).arg(i - 1);
          else
            indexstring = QString::number(prev);

          RDTreeWidgetItem *node = new RDTreeWidgetItem(
              {indexstring, v1.Left, v1.Bottom, v1.Width, v1.Height, v1.MinDepth, v1.MaxDepth});

          if(v1.Width == 0 || v1.Height == 0 || v1.MinDepth == v1.MaxDepth)
            setEmptyRow(node);

          ui->viewports->addTopLevelItem(node);
        }

        prev = i;
      }
    }

    // handle the last batch (the loop above leaves the last batch un-added)
    if(prev < state.m_Rasterizer.Viewports.count)
    {
      const GLPipe::Viewport &v1 = state.m_Rasterizer.Viewports[prev];

      // must display at least one viewport - otherwise if they are
      // all empty we get an empty list - we want a nice obvious
      // 'invalid viewport' entry. So check if last is 0

      if(v1.Width != v1.Height || v1.Width != 0 || v1.Height != 0 || v1.MinDepth != v1.MaxDepth ||
         ui->showEmpty->isChecked() || prev == 0)
      {
        QString indexstring;
        if(prev < state.m_Rasterizer.Viewports.count - 1)
          indexstring = QFormatStr("%1-%2").arg(prev).arg(state.m_Rasterizer.Viewports.count - 1);
        else
          indexstring = QString::number(prev);

        RDTreeWidgetItem *node = new RDTreeWidgetItem(
            {indexstring, v1.Left, v1.Bottom, v1.Width, v1.Height, v1.MinDepth, v1.MaxDepth});

        if(v1.Width == 0 || v1.Height == 0 || v1.MinDepth == v1.MaxDepth)
          setEmptyRow(node);

        ui->viewports->addTopLevelItem(node);
      }
    }
  }
  ui->viewports->verticalScrollBar()->setValue(vs);
  ui->viewports->clearSelection();
  ui->viewports->setUpdatesEnabled(true);

  bool anyScissorEnable = false;

  vs = ui->scissors->verticalScrollBar()->value();
  ui->scissors->setUpdatesEnabled(false);
  ui->scissors->clear();
  {
    // accumulate identical scissors to save on visual repetition
    int prev = 0;
    for(int i = 0; i < state.m_Rasterizer.Scissors.count; i++)
    {
      const GLPipe::Scissor &s1 = state.m_Rasterizer.Scissors[prev];
      const GLPipe::Scissor &s2 = state.m_Rasterizer.Scissors[i];

      if(s1.Width != s2.Width || s1.Height != s2.Height || s1.Left != s2.Left ||
         s1.Bottom != s2.Bottom || s1.Enabled != s2.Enabled)
      {
        if(s1.Enabled || ui->showEmpty->isChecked())
        {
          QString indexstring;
          if(prev < i - 1)
            indexstring = QFormatStr("%1-%2").arg(prev).arg(i - 1);
          else
            indexstring = QString::number(prev);

          RDTreeWidgetItem *node =
              new RDTreeWidgetItem({indexstring, s1.Left, s1.Bottom, s1.Width, s1.Height,
                                    s1.Enabled ? tr("True") : tr("False")});

          if(s1.Width == 0 || s1.Height == 0)
            setEmptyRow(node);

          if(!s1.Enabled)
            setInactiveRow(node);

          anyScissorEnable = anyScissorEnable || s1.Enabled;

          ui->scissors->addTopLevelItem(node);
        }

        prev = i;
      }
    }

    // handle the last batch (the loop above leaves the last batch un-added)
    if(prev < state.m_Rasterizer.Scissors.count)
    {
      const GLPipe::Scissor &s1 = state.m_Rasterizer.Scissors[prev];

      if(s1.Enabled || ui->showEmpty->isChecked())
      {
        QString indexstring;
        if(prev < state.m_Rasterizer.Scissors.count - 1)
          indexstring = QFormatStr("%1-%2").arg(prev).arg(state.m_Rasterizer.Scissors.count - 1);
        else
          indexstring = QString::number(prev);

        RDTreeWidgetItem *node =
            new RDTreeWidgetItem({indexstring, s1.Left, s1.Bottom, s1.Width, s1.Height,
                                  s1.Enabled ? tr("True") : tr("False")});

        if(s1.Width == 0 || s1.Height == 0)
          setEmptyRow(node);

        if(!s1.Enabled)
          setInactiveRow(node);

        anyScissorEnable = anyScissorEnable || s1.Enabled;

        ui->scissors->addTopLevelItem(node);
      }
    }
  }
  ui->scissors->clearSelection();
  ui->scissors->verticalScrollBar()->setValue(vs);
  ui->scissors->setUpdatesEnabled(true);

  ui->fillMode->setText(ToQStr(state.m_Rasterizer.m_State.fillMode));
  ui->cullMode->setText(ToQStr(state.m_Rasterizer.m_State.cullMode));
  ui->frontCCW->setPixmap(state.m_Rasterizer.m_State.FrontCCW ? tick : cross);

  ui->scissorEnabled->setPixmap(anyScissorEnable ? tick : cross);
  ui->provoking->setText(state.m_VtxIn.provokingVertexLast ? tr("Last") : tr("First"));

  ui->rasterizerDiscard->setPixmap(state.m_VtxProcess.discard ? tick : cross);

  if(state.m_Rasterizer.m_State.ProgrammablePointSize)
    ui->pointSize->setText(tr("Program", "ProgrammablePointSize"));
  else
    ui->pointSize->setText(Formatter::Format(state.m_Rasterizer.m_State.PointSize));
  ui->lineWidth->setText(Formatter::Format(state.m_Rasterizer.m_State.LineWidth));

  QString clipSetup;
  if(state.m_VtxProcess.clipOriginLowerLeft)
    clipSetup += tr("0,0 Lower Left");
  else
    clipSetup += tr("0,0 Upper Left");
  clipSetup += lit(", ");
  if(state.m_VtxProcess.clipNegativeOneToOne)
    clipSetup += lit("Z= -1 to 1");
  else
    clipSetup += lit("Z= 0 to 1");

  ui->clipSetup->setText(clipSetup);

  QString clipDistances;

  int numDist = 0;
  for(int i = 0; i < (int)ARRAY_COUNT(state.m_VtxProcess.clipPlanes); i++)
  {
    if(state.m_VtxProcess.clipPlanes[i])
    {
      if(numDist > 0)
        clipDistances += lit(", ");
      clipDistances += QString::number(i);

      numDist++;
    }
  }

  if(numDist == 0)
    clipDistances = lit("-");
  else
    clipDistances += tr(" enabled");

  ui->clipDistance->setText(clipDistances);

  ui->depthClamp->setPixmap(state.m_Rasterizer.m_State.DepthClamp ? tick : cross);
  ui->depthBias->setText(Formatter::Format(state.m_Rasterizer.m_State.DepthBias));
  ui->slopeScaledBias->setText(Formatter::Format(state.m_Rasterizer.m_State.SlopeScaledDepthBias));

  if(state.m_Rasterizer.m_State.OffsetClamp == 0.0f || qIsNaN(state.m_Rasterizer.m_State.OffsetClamp))
  {
    ui->offsetClamp->setText(QString());
    ui->offsetClamp->setPixmap(cross);
  }
  else
  {
    ui->offsetClamp->setText(Formatter::Format(state.m_Rasterizer.m_State.OffsetClamp));
    ui->offsetClamp->setPixmap(QPixmap());
  }

  ui->multisample->setPixmap(state.m_Rasterizer.m_State.MultisampleEnable ? tick : cross);
  ui->sampleShading->setPixmap(state.m_Rasterizer.m_State.SampleShading ? tick : cross);
  ui->minSampleShading->setText(Formatter::Format(state.m_Rasterizer.m_State.MinSampleShadingRate));
  ui->alphaToCoverage->setPixmap(state.m_Rasterizer.m_State.SampleAlphaToCoverage ? tick : cross);
  ui->alphaToOne->setPixmap(state.m_Rasterizer.m_State.SampleAlphaToOne ? tick : cross);
  if(state.m_Rasterizer.m_State.SampleCoverage)
  {
    QString sampleCoverage = Formatter::Format(state.m_Rasterizer.m_State.SampleCoverageValue);
    if(state.m_Rasterizer.m_State.SampleCoverageInvert)
      sampleCoverage += tr(" inverted");
    ui->sampleCoverage->setText(sampleCoverage);
    ui->sampleCoverage->setPixmap(QPixmap());
  }
  else
  {
    ui->sampleCoverage->setText(QString());
    ui->sampleCoverage->setPixmap(cross);
  }

  if(state.m_Rasterizer.m_State.SampleMask)
  {
    ui->sampleMask->setText(
        QFormatStr("%1")
            .arg(state.m_Rasterizer.m_State.SampleMaskValue, 8, 16, QLatin1Char('0'))
            .toUpper());
    ui->sampleMask->setPixmap(QPixmap());
  }
  else
  {
    ui->sampleMask->setText(QString());
    ui->sampleMask->setPixmap(cross);
  }

  ////////////////////////////////////////////////
  // Output Merger

  bool targets[32] = {};

  vs = ui->framebuffer->verticalScrollBar()->value();
  ui->framebuffer->setUpdatesEnabled(false);
  ui->framebuffer->clear();
  {
    int i = 0;
    for(int db : state.m_FB.m_DrawFBO.DrawBuffers)
    {
      ResourceId p;
      const GLPipe::Attachment *r = NULL;

      if(db >= 0 && db < state.m_FB.m_DrawFBO.Color.count)
      {
        p = state.m_FB.m_DrawFBO.Color[db].Obj;
        r = &state.m_FB.m_DrawFBO.Color[db];
      }

      bool filledSlot = (p != ResourceId());
      bool usedSlot = db >= 0;

      if(showNode(usedSlot, filledSlot))
      {
        uint32_t w = 1, h = 1, d = 1;
        uint32_t a = 1;
        QString format = tr("Unknown");
        QString name = tr("Texture ") + ToQStr(p);
        QString typeName = tr("Unknown");

        if(p == ResourceId())
        {
          name = tr("Empty");
          format = lit("-");
          typeName = lit("-");
          w = h = d = a = 0;
        }

        TextureDescription *tex = m_Ctx.GetTexture(p);
        if(tex)
        {
          w = tex->width;
          h = tex->height;
          d = tex->depth;
          a = tex->arraysize;
          format = ToQStr(tex->format.strname);
          name = ToQStr(tex->name);
          typeName = ToQStr(tex->resType);

          if(tex->format.srgbCorrected && !state.m_FB.FramebufferSRGB)
            name += lit(" (GL_FRAMEBUFFER_SRGB = 0)");

          if(!tex->customName && state.m_FS.ShaderDetails)
          {
            for(int s = 0; s < state.m_FS.ShaderDetails->OutputSig.count; s++)
            {
              if(state.m_FS.ShaderDetails->OutputSig[s].regIndex == (uint32_t)db &&
                 (state.m_FS.ShaderDetails->OutputSig[s].systemValue == ShaderBuiltin::Undefined ||
                  state.m_FS.ShaderDetails->OutputSig[s].systemValue == ShaderBuiltin::ColorOutput))
              {
                name = QFormatStr("<%1>").arg(ToQStr(state.m_FS.ShaderDetails->OutputSig[s].varName));
              }
            }
          }
        }

        if(r && (r->Swizzle[0] != TextureSwizzle::Red || r->Swizzle[1] != TextureSwizzle::Green ||
                 r->Swizzle[2] != TextureSwizzle::Blue || r->Swizzle[3] != TextureSwizzle::Alpha))
        {
          format += tr(" swizzle[%1%2%3%4]")
                        .arg(ToQStr(r->Swizzle[0]))
                        .arg(ToQStr(r->Swizzle[1]))
                        .arg(ToQStr(r->Swizzle[2]))
                        .arg(ToQStr(r->Swizzle[3]));
        }

        RDTreeWidgetItem *node =
            new RDTreeWidgetItem({i, name, typeName, w, h, d, a, format, QString()});

        if(tex)
          node->setTag(QVariant::fromValue(p));

        if(p == ResourceId())
        {
          setEmptyRow(node);
        }
        else
        {
          targets[i] = true;
        }

        ui->framebuffer->addTopLevelItem(node);
      }

      i++;
    }

    ResourceId dsObjects[] = {
        state.m_FB.m_DrawFBO.Depth.Obj, state.m_FB.m_DrawFBO.Stencil.Obj,
    };

    for(int dsIdx = 0; dsIdx < 2; dsIdx++)
    {
      ResourceId ds = dsObjects[dsIdx];

      bool filledSlot = (ds != ResourceId());
      bool usedSlot = filledSlot;
      if(showNode(usedSlot, filledSlot))
      {
        uint32_t w = 1, h = 1, d = 1;
        uint32_t a = 1;
        QString format = tr("Unknown");
        QString name = tr("Texture ") + ToQStr(ds);
        QString typeName = tr("Unknown");

        if(ds == ResourceId())
        {
          name = tr("Empty");
          format = lit("-");
          typeName = lit("-");
          w = h = d = a = 0;
        }

        TextureDescription *tex = m_Ctx.GetTexture(ds);
        if(tex)
        {
          w = tex->width;
          h = tex->height;
          d = tex->depth;
          a = tex->arraysize;
          format = ToQStr(tex->format.strname);
          name = ToQStr(tex->name);
          typeName = ToQStr(tex->resType);
        }

        QString slot = tr("Depth");
        if(i == 1)
          slot = tr("Stencil");

        bool depthstencil = false;

        if(state.m_FB.m_DrawFBO.Depth.Obj == state.m_FB.m_DrawFBO.Stencil.Obj &&
           state.m_FB.m_DrawFBO.Depth.Obj != ResourceId())
        {
          depthstencil = true;
          slot = tr("Depth-Stencil");
        }

        RDTreeWidgetItem *node =
            new RDTreeWidgetItem({slot, name, typeName, w, h, d, a, format, QString()});

        if(tex)
          node->setTag(QVariant::fromValue(ds));

        if(ds == ResourceId())
          setEmptyRow(node);

        ui->framebuffer->addTopLevelItem(node);

        // if we added a combined depth-stencil row, break now
        if(depthstencil)
          break;
      }
    }
  }

  ui->framebuffer->clearSelection();
  ui->framebuffer->setUpdatesEnabled(true);
  ui->framebuffer->verticalScrollBar()->setValue(vs);

  vs = ui->blends->verticalScrollBar()->value();
  ui->blends->setUpdatesEnabled(false);
  ui->blends->clear();
  {
    bool logic = state.m_FB.m_Blending.Blends[0].Logic != LogicOp::NoOp;

    int i = 0;
    for(const GLPipe::Blend &blend : state.m_FB.m_Blending.Blends)
    {
      bool filledSlot = (blend.Enabled || targets[i]);
      bool usedSlot = (targets[i]);

      // if logic operation is enabled, blending is disabled
      if(logic)
        filledSlot = (i == 0);

      if(showNode(usedSlot, filledSlot))
      {
        RDTreeWidgetItem *node = NULL;

        if(i == 0 && logic)
        {
          node = new RDTreeWidgetItem({i, tr("True"),

                                       lit("-"), lit("-"), ToQStr(blend.Logic),

                                       lit("-"), lit("-"), lit("-"),

                                       QFormatStr("%1%2%3%4")
                                           .arg((blend.WriteMask & 0x1) == 0 ? lit("_") : lit("R"))
                                           .arg((blend.WriteMask & 0x2) == 0 ? lit("_") : lit("G"))
                                           .arg((blend.WriteMask & 0x4) == 0 ? lit("_") : lit("B"))
                                           .arg((blend.WriteMask & 0x8) == 0 ? lit("_") : lit("A"))});
        }
        else
        {
          node = new RDTreeWidgetItem(
              {i, blend.Enabled ? tr("True") : tr("False"),

               ToQStr(blend.m_Blend.Source), ToQStr(blend.m_Blend.Destination),
               ToQStr(blend.m_Blend.Operation),

               ToQStr(blend.m_AlphaBlend.Source), ToQStr(blend.m_AlphaBlend.Destination),
               ToQStr(blend.m_AlphaBlend.Operation),

               QFormatStr("%1%2%3%4")
                   .arg((blend.WriteMask & 0x1) == 0 ? lit("_") : lit("R"))
                   .arg((blend.WriteMask & 0x2) == 0 ? lit("_") : lit("G"))
                   .arg((blend.WriteMask & 0x4) == 0 ? lit("_") : lit("B"))
                   .arg((blend.WriteMask & 0x8) == 0 ? lit("_") : lit("A"))});
        }

        if(!filledSlot)
          setEmptyRow(node);

        if(!usedSlot)
          setInactiveRow(node);

        ui->blends->addTopLevelItem(node);
      }

      i++;
    }
  }
  ui->blends->clearSelection();
  ui->blends->setUpdatesEnabled(true);
  ui->blends->verticalScrollBar()->setValue(vs);

  ui->blendFactor->setText(QFormatStr("%1, %2, %3, %4")
                               .arg(state.m_FB.m_Blending.BlendFactor[0], 0, 'f', 2)
                               .arg(state.m_FB.m_Blending.BlendFactor[1], 0, 'f', 2)
                               .arg(state.m_FB.m_Blending.BlendFactor[2], 0, 'f', 2)
                               .arg(state.m_FB.m_Blending.BlendFactor[3], 0, 'f', 2));

  ui->depthEnabled->setPixmap(state.m_DepthState.DepthEnable ? tick : cross);
  ui->depthFunc->setText(ToQStr(state.m_DepthState.DepthFunc));
  ui->depthWrite->setPixmap(state.m_DepthState.DepthWrites ? tick : cross);

  if(state.m_DepthState.DepthBounds)
  {
    ui->depthBounds->setText(Formatter::Format(state.m_DepthState.NearBound) + lit("-") +
                             Formatter::Format(state.m_DepthState.FarBound));
    ui->depthBounds->setPixmap(QPixmap());
  }
  else
  {
    ui->depthBounds->setText(QString());
    ui->depthBounds->setPixmap(cross);
  }

  ui->stencils->setUpdatesEnabled(false);
  ui->stencils->clear();
  if(state.m_StencilState.StencilEnable)
  {
    ui->stencils->addTopLevelItem(new RDTreeWidgetItem(
        {tr("Front"), ToQStr(state.m_StencilState.m_FrontFace.Func),
         ToQStr(state.m_StencilState.m_FrontFace.FailOp),
         ToQStr(state.m_StencilState.m_FrontFace.DepthFailOp),
         ToQStr(state.m_StencilState.m_FrontFace.PassOp),
         QFormatStr("%1").arg(state.m_StencilState.m_FrontFace.WriteMask, 2, 16, QLatin1Char('0')).toUpper(),
         QFormatStr("%1").arg(state.m_StencilState.m_FrontFace.ValueMask, 2, 16, QLatin1Char('0')).toUpper(),
         QFormatStr("%1").arg(state.m_StencilState.m_FrontFace.Ref, 2, 16, QLatin1Char('0')).toUpper()}));

    ui->stencils->addTopLevelItem(new RDTreeWidgetItem(
        {tr("Back"), ToQStr(state.m_StencilState.m_BackFace.Func),
         ToQStr(state.m_StencilState.m_BackFace.FailOp),
         ToQStr(state.m_StencilState.m_BackFace.DepthFailOp),
         ToQStr(state.m_StencilState.m_BackFace.PassOp),
         QFormatStr("%1").arg(state.m_StencilState.m_BackFace.WriteMask, 2, 16, QLatin1Char('0')).toUpper(),
         QFormatStr("%1").arg(state.m_StencilState.m_BackFace.ValueMask, 2, 16, QLatin1Char('0')).toUpper(),
         QFormatStr("%1").arg(state.m_StencilState.m_BackFace.Ref, 2, 16, QLatin1Char('0')).toUpper()}));
  }
  else
  {
    ui->stencils->addTopLevelItem(new RDTreeWidgetItem(
        {tr("Front"), lit("-"), lit("-"), lit("-"), lit("-"), lit("-"), lit("-"), lit("-")}));
    ui->stencils->addTopLevelItem(new RDTreeWidgetItem(
        {tr("Back"), lit("-"), lit("-"), lit("-"), lit("-"), lit("-"), lit("-"), lit("-")}));
  }
  ui->stencils->clearSelection();
  ui->stencils->setUpdatesEnabled(true);

  // highlight the appropriate stages in the flowchart
  if(draw == NULL)
  {
    ui->pipeFlow->setStagesEnabled({true, true, true, true, true, true, true, true, true});
  }
  else if(draw->flags & DrawFlags::Dispatch)
  {
    ui->pipeFlow->setStagesEnabled({false, false, false, false, false, false, false, false, true});
  }
  else
  {
    ui->pipeFlow->setStagesEnabled(
        {true, true, state.m_TCS.Object != ResourceId(), state.m_TES.Object != ResourceId(),
         state.m_GS.Object != ResourceId(), true, state.m_FS.Object != ResourceId(), true, false});
  }
}

QString GLPipelineStateViewer::formatMembers(int indent, const QString &nameprefix,
                                             const rdctype::array<ShaderConstant> &vars)
{
  QString indentstr(indent * 4, QLatin1Char(' '));

  QString ret;

  int i = 0;

  for(const ShaderConstant &v : vars)
  {
    if(!v.type.members.empty())
    {
      if(i > 0)
        ret += lit("\n");
      ret += indentstr + QFormatStr("// struct %1\n").arg(ToQStr(v.type.descriptor.name));
      ret += indentstr + lit("{\n") +
             formatMembers(indent + 1, ToQStr(v.name) + lit("_"), v.type.members) + indentstr +
             lit("}\n");
      if(i < vars.count - 1)
        ret += lit("\n");
    }
    else
    {
      QString arr;
      if(v.type.descriptor.elements > 1)
        arr = QFormatStr("[%1]").arg(v.type.descriptor.elements);
      ret += QFormatStr("%1%2 %3%4%5;\n")
                 .arg(indentstr)
                 .arg(ToQStr(v.type.descriptor.name))
                 .arg(nameprefix)
                 .arg(ToQStr(v.name))
                 .arg(arr);
    }

    i++;
  }

  return ret;
}

void GLPipelineStateViewer::resource_itemActivated(RDTreeWidgetItem *item, int column)
{
  const GLPipe::Shader *stage = stageForSender(item->treeWidget());

  if(stage == NULL)
    return;

  QVariant tag = item->tag();

  if(tag.canConvert<ResourceId>())
  {
    TextureDescription *tex = m_Ctx.GetTexture(tag.value<ResourceId>());

    if(tex)
    {
      if(tex->resType == TextureDim::Buffer)
      {
        IBufferViewer *viewer = m_Ctx.ViewTextureAsBuffer(0, 0, tex->ID);

        m_Ctx.AddDockWindow(viewer->Widget(), DockReference::AddTo, this);
      }
      else
      {
        if(!m_Ctx.HasTextureViewer())
          m_Ctx.ShowTextureViewer();
        ITextureViewer *viewer = m_Ctx.GetTextureViewer();
        viewer->ViewTexture(tex->ID, true);
      }

      return;
    }
  }
  else if(tag.canConvert<ReadWriteTag>())
  {
    ReadWriteTag buf = tag.value<ReadWriteTag>();

    const ShaderResource &shaderRes = stage->ShaderDetails->ReadWriteResources[buf.bindPoint];

    QString format = lit("// struct %1\n").arg(ToQStr(shaderRes.variableType.descriptor.name));

    if(shaderRes.variableType.members.count > 1)
    {
      format += tr("// members skipped as they are fixed size:\n");
      for(int i = 0; i < shaderRes.variableType.members.count - 1; i++)
        format += QFormatStr("%1 %2;\n")
                      .arg(ToQStr(shaderRes.variableType.members[i].type.descriptor.name))
                      .arg(ToQStr(shaderRes.variableType.members[i].name));
    }

    if(!shaderRes.variableType.members.empty())
    {
      format += lit("{\n") +
                formatMembers(1, QString(), shaderRes.variableType.members.back().type.members) +
                lit("}");
    }
    else
    {
      const auto &desc = shaderRes.variableType.descriptor;

      format = QString();
      if(desc.rowMajorStorage)
        format += lit("row_major ");

      format += ToQStr(desc.type);
      if(desc.rows > 1 && desc.cols > 1)
        format += QFormatStr("%1x%2").arg(desc.rows).arg(desc.cols);
      else if(desc.cols > 1)
        format += desc.cols;

      if(!desc.name.empty())
        format += lit(" ") + ToQStr(desc.name);

      if(desc.elements > 1)
        format += QFormatStr("[%1]").arg(desc.elements);
    }

    if(buf.ID != ResourceId())
    {
      IBufferViewer *viewer = m_Ctx.ViewBuffer(buf.offset, buf.size, buf.ID, format);

      m_Ctx.AddDockWindow(viewer->Widget(), DockReference::AddTo, this);
    }
  }
}

void GLPipelineStateViewer::ubo_itemActivated(RDTreeWidgetItem *item, int column)
{
  const GLPipe::Shader *stage = stageForSender(item->treeWidget());

  if(stage == NULL)
    return;

  QVariant tag = item->tag();

  if(!tag.canConvert<int>())
    return;

  int cb = tag.value<int>();

  IConstantBufferPreviewer *prev = m_Ctx.ViewConstantBuffer(stage->stage, cb, 0);

  m_Ctx.AddDockWindow(prev->Widget(), DockReference::RightOf, this, 0.3f);
}

void GLPipelineStateViewer::on_viAttrs_itemActivated(RDTreeWidgetItem *item, int column)
{
  on_meshView_clicked();
}

void GLPipelineStateViewer::on_viBuffers_itemActivated(RDTreeWidgetItem *item, int column)
{
  QVariant tag = item->tag();

  if(tag.canConvert<VBIBTag>())
  {
    VBIBTag buf = tag.value<VBIBTag>();

    if(buf.id != ResourceId())
    {
      IBufferViewer *viewer = m_Ctx.ViewBuffer(buf.offset, UINT64_MAX, buf.id);

      m_Ctx.AddDockWindow(viewer->Widget(), DockReference::AddTo, this);
    }
  }
}

void GLPipelineStateViewer::highlightIABind(int slot)
{
  int idx = ((slot + 1) * 21) % 32;    // space neighbouring colours reasonably distinctly

  const GLPipe::VertexInput &VI = m_Ctx.CurGLPipelineState().m_VtxIn;

  QColor col = QColor::fromHslF(float(idx) / 32.0f, 1.0f, 0.95f);

  ui->viAttrs->beginUpdate();
  ui->viBuffers->beginUpdate();

  if(slot < m_VBNodes.count())
  {
    m_VBNodes[slot]->setBackgroundColor(col);
    m_VBNodes[slot]->setForegroundColor(QColor(0, 0, 0));
  }

  for(int i = 0; i < ui->viAttrs->topLevelItemCount(); i++)
  {
    RDTreeWidgetItem *item = ui->viAttrs->topLevelItem(i);

    if((int)VI.attributes[i].BufferSlot != slot)
    {
      item->setBackground(QBrush());
      item->setForeground(QBrush());
    }
    else
    {
      item->setBackgroundColor(col);
      item->setForegroundColor(QColor(0, 0, 0));
    }
  }

  ui->viAttrs->endUpdate();
  ui->viBuffers->endUpdate();
}

void GLPipelineStateViewer::on_viAttrs_mouseMove(QMouseEvent *e)
{
  if(!m_Ctx.LogLoaded())
    return;

  QModelIndex idx = ui->viAttrs->indexAt(e->pos());

  vertex_leave(NULL);

  const GLPipe::VertexInput &VI = m_Ctx.CurGLPipelineState().m_VtxIn;

  if(idx.isValid())
  {
    if(idx.row() >= 0 && idx.row() < VI.attributes.count)
    {
      uint32_t buffer = VI.attributes[idx.row()].BufferSlot;

      highlightIABind((int)buffer);
    }
  }
}

void GLPipelineStateViewer::on_viBuffers_mouseMove(QMouseEvent *e)
{
  if(!m_Ctx.LogLoaded())
    return;

  RDTreeWidgetItem *item = ui->viBuffers->itemAt(e->pos());

  vertex_leave(NULL);

  if(item)
  {
    int idx = m_VBNodes.indexOf(item);
    if(idx >= 0)
    {
      highlightIABind(idx);
    }
    else
    {
      item->setBackground(ui->viBuffers->palette().brush(QPalette::Window));
      item->setForeground(ui->viBuffers->palette().brush(QPalette::WindowText));
    }
  }
}

void GLPipelineStateViewer::vertex_leave(QEvent *e)
{
  ui->viAttrs->beginUpdate();
  ui->viBuffers->beginUpdate();

  for(int i = 0; i < ui->viAttrs->topLevelItemCount(); i++)
  {
    ui->viAttrs->topLevelItem(i)->setBackground(QBrush());
    ui->viAttrs->topLevelItem(i)->setForeground(QBrush());
  }

  for(int i = 0; i < ui->viBuffers->topLevelItemCount(); i++)
  {
    ui->viBuffers->topLevelItem(i)->setBackground(QBrush());
    ui->viBuffers->topLevelItem(i)->setForeground(QBrush());
  }

  ui->viAttrs->endUpdate();
  ui->viBuffers->endUpdate();
}

void GLPipelineStateViewer::on_pipeFlow_stageSelected(int index)
{
  ui->stagesTabs->setCurrentIndex(index);
}

void GLPipelineStateViewer::shaderView_clicked()
{
  const GLPipe::Shader *stage = stageForSender(qobject_cast<QWidget *>(QObject::sender()));

  if(stage == NULL || stage->Object == ResourceId())
    return;

  ShaderReflection *shaderDetails = stage->ShaderDetails;

  IShaderViewer *shad = m_Ctx.ViewShader(&stage->BindpointMapping, shaderDetails, stage->stage);

  m_Ctx.AddDockWindow(shad->Widget(), DockReference::AddTo, this);
}

void GLPipelineStateViewer::shaderLabel_clicked(QMouseEvent *event)
{
  // forward to shaderView_clicked, we only need this to handle the different parameter, and we
  // can't use a lambda because then QObject::sender() is NULL
  shaderView_clicked();
}

void GLPipelineStateViewer::shaderEdit_clicked()
{
  QWidget *sender = qobject_cast<QWidget *>(QObject::sender());
  const GLPipe::Shader *stage = stageForSender(sender);

  if(!stage || stage->Object == ResourceId())
    return;

  const ShaderReflection *shaderDetails = stage->ShaderDetails;

  if(!shaderDetails)
    return;

  QString entryFunc = lit("EditedShader%1S").arg(ToQStr(stage->stage, GraphicsAPI::OpenGL)[0]);

  QString mainfile;

  QStringMap files;

  bool hasOrigSource = m_Common.PrepareShaderEditing(shaderDetails, entryFunc, files, mainfile);

  if(!hasOrigSource)
  {
    QString glsl = lit("// TODO - disassemble SPIR-V");

    mainfile = lit("generated.glsl");

    files[mainfile] = glsl;
  }

  if(files.empty())
    return;

  m_Common.EditShader(stage->stage, stage->Object, shaderDetails, entryFunc, files, mainfile);
}

void GLPipelineStateViewer::shaderSave_clicked()
{
  const GLPipe::Shader *stage = stageForSender(qobject_cast<QWidget *>(QObject::sender()));

  if(stage == NULL)
    return;

  ShaderReflection *shaderDetails = stage->ShaderDetails;

  if(stage->Object == ResourceId())
    return;

  m_Common.SaveShaderFile(shaderDetails);
}

void GLPipelineStateViewer::on_exportHTML_clicked()
{
}

void GLPipelineStateViewer::on_meshView_clicked()
{
  if(!m_Ctx.HasMeshPreview())
    m_Ctx.ShowMeshPreview();
  ToolWindowManager::raiseToolWindow(m_Ctx.GetMeshPreview()->Widget());
}
