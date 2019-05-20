/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016-2019 Baldur Karlsson
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
#include <QMenu>
#include <QMouseEvent>
#include <QScrollBar>
#include <QXmlStreamWriter>
#include "3rdparty/toolwindowmanager/ToolWindowManager.h"
#include "Code/Resources.h"
#include "Widgets/Extended/RDHeaderView.h"
#include "PipelineStateViewer.h"
#include "ui_GLPipelineStateViewer.h"

struct GLVBIBTag
{
  GLVBIBTag() { offset = 0; }
  GLVBIBTag(ResourceId i, uint64_t offs, QString f = QString())
  {
    id = i;
    offset = offs;
    format = f;
  }

  ResourceId id;
  uint64_t offset;
  QString format;
};

Q_DECLARE_METATYPE(GLVBIBTag);

struct GLReadWriteTag
{
  GLReadWriteTag()
  {
    bindPoint = 0;
    offset = size = 0;
  }
  GLReadWriteTag(uint32_t b, ResourceId id, uint64_t offs, uint64_t sz)
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

Q_DECLARE_METATYPE(GLReadWriteTag);

GLPipelineStateViewer::GLPipelineStateViewer(ICaptureContext &ctx, PipelineStateViewer &common,
                                             QWidget *parent)
    : QFrame(parent), ui(new Ui::GLPipelineStateViewer), m_Ctx(ctx), m_Common(common)
{
  ui->setupUi(this);

  const QIcon &action = Icons::action();
  const QIcon &action_hover = Icons::action_hover();

  RDLabel *shaderLabels[] = {
      ui->vaoLabel, ui->vsShader, ui->tcsShader, ui->tesShader,
      ui->gsShader, ui->fsShader, ui->csShader,
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
  {
    b->setAutoFillBackground(true);
    b->setBackgroundRole(QPalette::ToolTipBase);
    b->setForegroundRole(QPalette::ToolTipText);
    b->setMinimumSizeHint(QSize(250, 0));
  }

  for(RDLabel *b : {ui->xfbObj, ui->readFBO, ui->drawFBO})
  {
    b->setAutoFillBackground(true);
    b->setBackgroundRole(QPalette::ToolTipBase);
    b->setForegroundRole(QPalette::ToolTipText);
    b->setMinimumSizeHint(QSize(100, 0));
  }

  for(QToolButton *b : editButtons)
    QObject::connect(b, &QToolButton::clicked, &m_Common, &PipelineStateViewer::shaderEdit_clicked);

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

  {
    QMenu *extensionsMenu = new QMenu(this);

    ui->extensions->setMenu(extensionsMenu);
    ui->extensions->setPopupMode(QToolButton::InstantPopup);

    QObject::connect(extensionsMenu, &QMenu::aboutToShow, [this, extensionsMenu]() {
      extensionsMenu->clear();
      m_Ctx.Extensions().MenuDisplaying(PanelMenu::PipelineStateViewer, extensionsMenu,
                                        ui->extensions, {});
    });
  }

  addGridLines(ui->rasterizerGridLayout, palette().color(QPalette::WindowText));
  addGridLines(ui->MSAAGridLayout, palette().color(QPalette::WindowText));
  addGridLines(ui->blendStateGridLayout, palette().color(QPalette::WindowText));
  addGridLines(ui->depthStateGridLayout, palette().color(QPalette::WindowText));

  {
    RDHeaderView *header = new RDHeaderView(Qt::Horizontal, this);
    ui->viAttrs->setHeader(header);

    ui->viAttrs->setColumns({tr("Index"), tr("Enabled"), tr("Name"), tr("Format/Generic Value"),
                             tr("Buffer Slot"), tr("Relative Offset"), tr("Go")});
    header->setColumnStretchHints({1, 1, 4, 3, 2, 2, -1});

    ui->viAttrs->setClearSelectionOnFocusLoss(true);
    ui->viAttrs->setInstantTooltips(true);
    ui->viAttrs->setHoverIconColumn(6, action, action_hover);
  }

  {
    RDHeaderView *header = new RDHeaderView(Qt::Horizontal, this);
    ui->viBuffers->setHeader(header);

    ui->viBuffers->setColumns({tr("Slot"), tr("Buffer"), tr("Stride"), tr("Offset"), tr("Divisor"),
                               tr("Byte Length"), tr("Go")});
    header->setColumnStretchHints({1, 4, 2, 2, 2, 3, -1});

    ui->viBuffers->setClearSelectionOnFocusLoss(true);
    ui->viBuffers->setInstantTooltips(true);
    ui->viBuffers->setHoverIconColumn(6, action, action_hover);
  }

  for(RDTreeWidget *tex : textures)
  {
    RDHeaderView *header = new RDHeaderView(Qt::Horizontal, this);
    tex->setHeader(header);

    tex->setColumns({tr("Slot"), tr("Resource"), tr("Type"), tr("Width"), tr("Height"), tr("Depth"),
                     tr("Array Size"), tr("Format"), tr("Go")});
    header->setColumnStretchHints({2, 4, 2, 1, 1, 1, 1, 3, -1});

    tex->setHoverIconColumn(8, action, action_hover);
    tex->setClearSelectionOnFocusLoss(true);
    tex->setInstantTooltips(true);
  }

  for(RDTreeWidget *samp : samplers)
  {
    RDHeaderView *header = new RDHeaderView(Qt::Horizontal, this);
    samp->setHeader(header);

    samp->setColumns(
        {tr("Slot"), tr("Object"), tr("Wrap Mode"), tr("Filter"), tr("LOD Clamp"), tr("LOD Bias")});
    header->setColumnStretchHints({1, 2, 2, 2, 2, 2});

    samp->setClearSelectionOnFocusLoss(true);
    samp->setInstantTooltips(true);
  }

  for(RDTreeWidget *ubo : ubos)
  {
    RDHeaderView *header = new RDHeaderView(Qt::Horizontal, this);
    ubo->setHeader(header);

    ubo->setColumns({tr("Slot"), tr("Buffer"), tr("Byte Range"), tr("Size"), tr("Go")});
    header->setColumnStretchHints({1, 2, 3, 3, -1});

    ubo->setHoverIconColumn(4, action, action_hover);
    ubo->setClearSelectionOnFocusLoss(true);
    ubo->setInstantTooltips(true);
  }

  for(RDTreeWidget *sub : subroutines)
  {
    RDHeaderView *header = new RDHeaderView(Qt::Horizontal, this);
    sub->setHeader(header);

    sub->setColumns({tr("Uniform"), tr("Value")});
    header->setColumnStretchHints({1, 1});

    sub->setClearSelectionOnFocusLoss(true);
    sub->setInstantTooltips(true);
  }

  for(RDTreeWidget *ubo : readwrites)
  {
    RDHeaderView *header = new RDHeaderView(Qt::Horizontal, this);
    ubo->setHeader(header);

    ubo->setColumns({tr("Binding"), tr("Slot"), tr("Resource"), tr("Dimensions"), tr("Format"),
                     tr("Access"), tr("Go")});
    header->setColumnStretchHints({1, 1, 2, 3, 3, 1, -1});

    ubo->setHoverIconColumn(6, action, action_hover);
    ubo->setClearSelectionOnFocusLoss(true);
    ubo->setInstantTooltips(true);
  }

  {
    RDHeaderView *header = new RDHeaderView(Qt::Horizontal, this);
    ui->xfbBuffers->setHeader(header);

    ui->xfbBuffers->setColumns(
        {tr("Slot"), tr("Buffer"), tr("Byte Length"), tr("Offset"), tr("Go")});
    header->setColumnStretchHints({1, 4, 3, 2, -1});

    header->setMinimumSectionSize(40);

    ui->xfbBuffers->setClearSelectionOnFocusLoss(true);
    ui->xfbBuffers->setInstantTooltips(true);
    ui->xfbBuffers->setHoverIconColumn(4, action, action_hover);
  }

  {
    RDHeaderView *header = new RDHeaderView(Qt::Horizontal, this);
    ui->viewports->setHeader(header);

    ui->viewports->setColumns(
        {tr("Slot"), tr("X"), tr("Y"), tr("Width"), tr("Height"), tr("MinDepth"), tr("MaxDepth")});
    header->setColumnStretchHints({-1, -1, -1, -1, -1, -1, 1});
    header->setMinimumSectionSize(40);

    ui->viewports->setClearSelectionOnFocusLoss(true);
    ui->viewports->setInstantTooltips(true);
  }

  {
    RDHeaderView *header = new RDHeaderView(Qt::Horizontal, this);
    ui->scissors->setHeader(header);

    ui->scissors->setColumns(
        {tr("Slot"), tr("X"), tr("Y"), tr("Width"), tr("Height"), tr("Enabled")});
    header->setColumnStretchHints({-1, -1, -1, -1, -1, 1});
    header->setMinimumSectionSize(40);

    ui->scissors->setClearSelectionOnFocusLoss(true);
    ui->scissors->setInstantTooltips(true);
  }

  {
    RDHeaderView *header = new RDHeaderView(Qt::Horizontal, this);
    ui->framebuffer->setHeader(header);

    ui->framebuffer->setColumns({tr("Slot"), tr("Resource"), tr("Type"), tr("Width"), tr("Height"),
                                 tr("Depth"), tr("Array Size"), tr("Format"), tr("Go")});
    header->setColumnStretchHints({2, 4, 2, 1, 1, 1, 1, 3, -1});

    ui->framebuffer->setHoverIconColumn(8, action, action_hover);
    ui->framebuffer->setClearSelectionOnFocusLoss(true);
    ui->framebuffer->setInstantTooltips(true);
  }

  {
    RDHeaderView *header = new RDHeaderView(Qt::Horizontal, this);
    ui->blends->setHeader(header);

    ui->blends->setColumns({tr("Slot"), tr("Enabled"), tr("Col Src"), tr("Col Dst"), tr("Col Op"),
                            tr("Alpha Src"), tr("Alpha Dst"), tr("Alpha Op"), tr("Write Mask")});
    header->setColumnStretchHints({-1, 1, 2, 2, 2, 2, 2, 2, 1});

    ui->blends->setClearSelectionOnFocusLoss(true);
    ui->blends->setInstantTooltips(true);
  }

  {
    RDHeaderView *header = new RDHeaderView(Qt::Horizontal, this);
    ui->stencils->setHeader(header);

    ui->stencils->setColumns({tr("Face"), tr("Func"), tr("Fail Op"), tr("Depth Fail Op"),
                              tr("Pass Op"), tr("Write Mask"), tr("Comp Mask"), tr("Ref")});
    header->setColumnStretchHints({1, 2, 2, 2, 2, 1, 1, 1});

    ui->stencils->setClearSelectionOnFocusLoss(true);
    ui->stencils->setInstantTooltips(true);
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

  m_Common.setMeshViewPixmap(ui->meshView);

  ui->vaoLabel->setFont(Formatter::PreferredFont());
  ui->viAttrs->setFont(Formatter::PreferredFont());
  ui->viBuffers->setFont(Formatter::PreferredFont());
  ui->xfbBuffers->setFont(Formatter::PreferredFont());
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

void GLPipelineStateViewer::OnCaptureLoaded()
{
  OnEventChanged(m_Ctx.CurEvent());
}

void GLPipelineStateViewer::OnCaptureClosed()
{
  ui->pipeFlow->setStagesEnabled({true, true, true, true, true, true, true, true, true});

  clearState();
}

void GLPipelineStateViewer::OnEventChanged(uint32_t eventId)
{
  setState();
}

void GLPipelineStateViewer::on_showUnused_toggled(bool checked)
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

void GLPipelineStateViewer::setViewDetails(RDTreeWidgetItem *node, TextureDescription *tex,
                                           uint32_t firstMip, uint32_t numMips)
{
  if((tex->mips > 1 && firstMip > 0) || numMips < tex->mips)
  {
    QString text;

    if(numMips == 1)
      text += tr("The texture has %1 mips, the view covers mip %2.").arg(tex->mips).arg(firstMip);
    else
      text += tr("The texture has %1 mips, the view covers mips %2-%3.")
                  .arg(tex->mips)
                  .arg(firstMip)
                  .arg(firstMip + numMips - 1);

    node->setToolTip(text);

    node->setBackgroundColor(QColor(127, 255, 212));
    node->setForegroundColor(QColor(0, 0, 0));
  }
}

bool GLPipelineStateViewer::showNode(bool usedSlot, bool filledSlot)
{
  const bool showUnused = ui->showUnused->isChecked();
  const bool showEmpty = ui->showEmpty->isChecked();

  // show if it's referenced by the shader - regardless of empty or not
  if(usedSlot)
    return true;

  // it's bound, but not referenced, and we have "show unused"
  if(showUnused && !usedSlot && filledSlot)
    return true;

  // it's empty, and we have "show empty"
  if(showEmpty && !filledSlot)
    return true;

  return false;
}

const GLPipe::Shader *GLPipelineStateViewer::stageForSender(QWidget *widget)
{
  if(!m_Ctx.IsCaptureLoaded())
    return NULL;

  while(widget)
  {
    if(widget == ui->stagesTabs->widget(0))
      return &m_Ctx.CurGLPipelineState()->vertexShader;
    if(widget == ui->stagesTabs->widget(1))
      return &m_Ctx.CurGLPipelineState()->vertexShader;
    if(widget == ui->stagesTabs->widget(2))
      return &m_Ctx.CurGLPipelineState()->tessControlShader;
    if(widget == ui->stagesTabs->widget(3))
      return &m_Ctx.CurGLPipelineState()->tessEvalShader;
    if(widget == ui->stagesTabs->widget(4))
      return &m_Ctx.CurGLPipelineState()->geometryShader;
    if(widget == ui->stagesTabs->widget(5))
      return &m_Ctx.CurGLPipelineState()->fragmentShader;
    if(widget == ui->stagesTabs->widget(6))
      return &m_Ctx.CurGLPipelineState()->fragmentShader;
    if(widget == ui->stagesTabs->widget(7))
      return &m_Ctx.CurGLPipelineState()->fragmentShader;
    if(widget == ui->stagesTabs->widget(8))
      return &m_Ctx.CurGLPipelineState()->computeShader;

    widget = widget->parentWidget();
  }

  qCritical() << "Unrecognised control calling event handler";

  return NULL;
}

void GLPipelineStateViewer::clearShaderState(RDLabel *shader, RDTreeWidget *tex, RDTreeWidget *samp,
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
  m_EmptyNodes.clear();

  ui->vaoLabel->setText(QString());

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

  ui->xfbBuffers->clear();

  QToolButton *shaderButtons[] = {
      ui->vsShaderViewButton, ui->tcsShaderViewButton, ui->tesShaderViewButton,
      ui->gsShaderViewButton, ui->fsShaderViewButton,  ui->csShaderViewButton,
      ui->vsShaderEditButton, ui->tcsShaderEditButton, ui->tesShaderEditButton,
      ui->gsShaderEditButton, ui->fsShaderEditButton,  ui->csShaderEditButton,
      ui->vsShaderSaveButton, ui->tcsShaderSaveButton, ui->tesShaderSaveButton,
      ui->gsShaderSaveButton, ui->fsShaderSaveButton,  ui->csShaderSaveButton,
  };

  for(QToolButton *b : shaderButtons)
    b->setEnabled(false);

  const QPixmap &tick = Pixmaps::tick(this);
  const QPixmap &cross = Pixmaps::cross(this);

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

  ui->depthBounds->setPixmap(QPixmap());
  ui->depthBounds->setText(lit("0.0-1.0"));

  ui->stencils->clear();
}

void GLPipelineStateViewer::setShaderState(const GLPipe::Shader &stage, RDLabel *shader,
                                           RDTreeWidget *textures, RDTreeWidget *samplers,
                                           RDTreeWidget *ubos, RDTreeWidget *subs,
                                           RDTreeWidget *readwrites)
{
  ShaderReflection *shaderDetails = stage.reflection;
  const ShaderBindpointMapping &mapping = stage.bindpointMapping;
  const GLPipe::State &state = *m_Ctx.CurGLPipelineState();

  if(stage.shaderResourceId == ResourceId())
  {
    shader->setText(ToQStr(stage.shaderResourceId));
  }
  else
  {
    QString shText = ToQStr(stage.shaderResourceId);

    shText = ToQStr(stage.programResourceId) + lit(" > ") + shText;

    if(state.pipelineResourceId != ResourceId())
      shText = ToQStr(state.pipelineResourceId) + lit(" > ") + shText;

    shader->setText(shText);
  }

  int vs = 0;
  int vs2 = 0;

  // simultaneous update of resources and samplers
  vs = textures->verticalScrollBar()->value();
  textures->beginUpdate();
  textures->clear();
  vs2 = samplers->verticalScrollBar()->value();
  samplers->beginUpdate();
  samplers->clear();

  for(int i = 0; i < state.textures.count(); i++)
  {
    const GLPipe::Texture &r = state.textures[i];
    const GLPipe::Sampler &s = state.samplers[i];

    const ShaderResource *shaderInput = NULL;
    const Bindpoint *map = NULL;

    if(shaderDetails)
    {
      for(const ShaderResource &bind : shaderDetails->readOnlyResources)
      {
        if(bind.isReadOnly && mapping.readOnlyResources[bind.bindPoint].bind == i)
        {
          shaderInput = &bind;
          map = &mapping.readOnlyResources[bind.bindPoint];
        }
      }
    }

    bool filledSlot = (r.resourceId != ResourceId());
    bool usedSlot = (shaderInput && map && map->used);

    if(showNode(usedSlot, filledSlot))
    {
      // do texture
      {
        QString slotname = QString::number(i);

        if(shaderInput && !shaderInput->name.empty())
          slotname += lit(": ") + shaderInput->name;

        uint32_t w = 1, h = 1, d = 1;
        uint32_t a = 1;
        QString format = lit("Unknown");
        QString typeName = lit("Unknown");

        if(!filledSlot)
        {
          format = lit("-");
          typeName = lit("-");
          w = h = d = a = 0;
        }

        TextureDescription *tex = m_Ctx.GetTexture(r.resourceId);

        if(tex)
        {
          w = tex->width;
          h = tex->height;
          d = tex->depth;
          a = tex->arraysize;
          format = tex->format.Name();
          typeName = ToQStr(tex->type);

          if(tex->format.type == ResourceFormatType::D16S8 ||
             tex->format.type == ResourceFormatType::D24S8 ||
             tex->format.type == ResourceFormatType::D32S8)
          {
            if(r.depthReadChannel == 0)
              format += tr(" Depth-Read");
            else if(r.depthReadChannel == 1)
              format += tr(" Stencil-Read");
          }
          else if(r.swizzle[0] != TextureSwizzle::Red || r.swizzle[1] != TextureSwizzle::Green ||
                  r.swizzle[2] != TextureSwizzle::Blue || r.swizzle[3] != TextureSwizzle::Alpha)
          {
            format += tr(" swizzle[%1%2%3%4]")
                          .arg(ToQStr(r.swizzle[0]))
                          .arg(ToQStr(r.swizzle[1]))
                          .arg(ToQStr(r.swizzle[2]))
                          .arg(ToQStr(r.swizzle[3]));
          }
        }

        RDTreeWidgetItem *node =
            new RDTreeWidgetItem({slotname, r.resourceId, typeName, w, h, d, a, format, QString()});

        node->setTag(QVariant::fromValue(r.resourceId));

        if(tex)
          setViewDetails(node, tex, r.firstMip, r.numMips);

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
          slotname += lit(": ") + shaderInput->name;

        QString borderColor = QFormatStr("%1, %2, %3, %4")
                                  .arg(s.borderColor[0])
                                  .arg(s.borderColor[1])
                                  .arg(s.borderColor[2])
                                  .arg(s.borderColor[3]);

        QString addressing;

        QString addPrefix;
        QString addVal;

        QString addr[] = {ToQStr(s.addressS, GraphicsAPI::OpenGL),
                          ToQStr(s.addressT, GraphicsAPI::OpenGL),
                          ToQStr(s.addressR, GraphicsAPI::OpenGL)};

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

        if(r.type == TextureType::TextureCube || r.type == TextureType::TextureCubeArray)
        {
          addressing += s.seamlessCubeMap ? tr(" Seamless") : tr(" Non-Seamless");
        }

        QString filter = ToQStr(s.filter);

        if(s.maxAnisotropy > 1)
          filter += lit(" Aniso%1x").arg(s.maxAnisotropy);

        if(s.filter.filter == FilterFunction::Comparison)
          filter += QFormatStr(" (%1)").arg(ToQStr(s.compareFunction));
        else if(s.filter.filter != FilterFunction::Normal)
          filter += QFormatStr(" (%1)").arg(ToQStr(s.filter.filter));

        RDTreeWidgetItem *node = new RDTreeWidgetItem({
            slotname, s.resourceId != ResourceId() ? s.resourceId : r.resourceId, addressing,
            filter, QFormatStr("%1 - %2")
                        .arg(s.minLOD == -FLT_MAX ? lit("0") : QString::number(s.minLOD))
                        .arg(s.maxLOD == FLT_MAX ? lit("FLT_MAX") : QString::number(s.maxLOD)),
            s.mipLODBias,
        });

        if(!filledSlot)
          setEmptyRow(node);

        if(!usedSlot)
          setInactiveRow(node);

        samplers->addTopLevelItem(node);
      }
    }
  }

  samplers->clearSelection();
  samplers->endUpdate();
  samplers->verticalScrollBar()->setValue(vs2);
  textures->clearSelection();
  textures->endUpdate();
  textures->verticalScrollBar()->setValue(vs);

  vs = ubos->verticalScrollBar()->value();
  ubos->beginUpdate();
  ubos->clear();

  // see if there's a global UBO, if so display it first
  if(shaderDetails)
  {
    const ConstantBlock *shaderCBuf = NULL;
    const Bindpoint *map = NULL;

    int idx = 0;

    for(const ConstantBlock &bind : shaderDetails->constantBlocks)
    {
      if(!bind.bufferBacked)
      {
        shaderCBuf = &bind;
        map = &mapping.constantBlocks[bind.bindPoint];
        break;
      }

      idx++;
    }

    if(shaderCBuf && map)
    {
      bool filledSlot = true;
      bool usedSlot = map->used;

      if(showNode(usedSlot, filledSlot))
      {
        QString sizestr = tr("%1 Variables").arg(shaderCBuf->variables.count());

        RDTreeWidgetItem *node =
            new RDTreeWidgetItem({tr("Uniforms"), QString(), QString(), sizestr, QString()});

        node->setTag(QVariant::fromValue(idx));

        if(!usedSlot)
          setInactiveRow(node);

        ubos->addTopLevelItem(node);
      }
    }
  }

  for(int i = 0; i < state.uniformBuffers.count(); i++)
  {
    const GLPipe::Buffer &b = state.uniformBuffers[i];

    const ConstantBlock *shaderCBuf = NULL;
    const Bindpoint *map = NULL;

    int idx = 0;

    if(shaderDetails)
    {
      for(const ConstantBlock &bind : shaderDetails->constantBlocks)
      {
        if(bind.bufferBacked && mapping.constantBlocks[bind.bindPoint].bind == i)
        {
          shaderCBuf = &bind;
          map = &mapping.constantBlocks[bind.bindPoint];

          // if this one is used, break immediately. Otherwise keep going to see if we find one that
          // is used
          if(map->used)
            break;
        }

        idx++;
      }
    }

    bool filledSlot = ((shaderCBuf && !shaderCBuf->bufferBacked) || b.resourceId != ResourceId());
    bool usedSlot = (shaderCBuf && map && map->used);

    if(showNode(usedSlot, filledSlot))
    {
      ulong offset = 0;
      ulong length = 0;
      int numvars = shaderCBuf ? shaderCBuf->variables.count() : 0;
      ulong byteSize = shaderCBuf ? (ulong)shaderCBuf->byteSize : 0;

      QString name;
      QString sizestr = tr("%1 Variables").arg(numvars);
      QString byterange;

      if(!filledSlot)
      {
        name = tr("Empty");
        length = 0;
      }

      QString slotname = QString::number(i);

      if(shaderCBuf && !shaderCBuf->name.empty())
        slotname += lit(": ") + shaderCBuf->name;

      offset = b.byteOffset;
      length = b.byteSize;

      BufferDescription *buf = m_Ctx.GetBuffer(b.resourceId);
      if(buf)
      {
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

      RDTreeWidgetItem *node =
          new RDTreeWidgetItem({slotname, b.resourceId, byterange, sizestr, QString()});

      node->setTag(QVariant::fromValue(idx));

      if(!filledSlot)
        setEmptyRow(node);

      if(!usedSlot)
        setInactiveRow(node);

      ubos->addTopLevelItem(node);
    }
  }
  ubos->clearSelection();
  ubos->endUpdate();
  ubos->verticalScrollBar()->setValue(vs);

  vs = subs->verticalScrollBar()->value();
  subs->beginUpdate();
  subs->clear();
  for(int i = 0; i < stage.subroutines.count(); i++)
    subs->addTopLevelItem(new RDTreeWidgetItem({i, stage.subroutines[i]}));
  subs->clearSelection();
  subs->endUpdate();
  subs->verticalScrollBar()->setValue(vs);

  subs->parentWidget()->setVisible(!stage.subroutines.empty());

  vs = readwrites->verticalScrollBar()->value();
  readwrites->beginUpdate();
  readwrites->clear();
  for(int i = 0; shaderDetails && i < shaderDetails->readWriteResources.count(); i++)
  {
    const ShaderResource &res = shaderDetails->readWriteResources[i];
    int bindPoint = stage.bindpointMapping.readWriteResources[res.bindPoint].bind;

    GLReadWriteType readWriteType = GetGLReadWriteType(res);

    const GLPipe::Buffer *bf = NULL;
    const GLPipe::ImageLoadStore *im = NULL;
    ResourceId id;

    if(readWriteType == GLReadWriteType::Image && bindPoint >= 0 && bindPoint < state.images.count())
    {
      im = &state.images[bindPoint];
      id = state.images[bindPoint].resourceId;
    }

    if(readWriteType == GLReadWriteType::Atomic && bindPoint >= 0 &&
       bindPoint < state.atomicBuffers.count())
    {
      bf = &state.atomicBuffers[bindPoint];
      id = state.atomicBuffers[bindPoint].resourceId;
    }

    if(readWriteType == GLReadWriteType::SSBO && bindPoint >= 0 &&
       bindPoint < state.shaderStorageBuffers.count())
    {
      bf = &state.shaderStorageBuffers[bindPoint];
      id = state.shaderStorageBuffers[bindPoint].resourceId;
    }

    bool filledSlot = id != ResourceId();
    bool usedSlot = stage.bindpointMapping.readWriteResources[i].used;

    if(showNode(usedSlot, filledSlot))
    {
      QString binding =
          readWriteType == GLReadWriteType::Image
              ? tr("Image")
              : readWriteType == GLReadWriteType::Atomic
                    ? tr("Atomic")
                    : readWriteType == GLReadWriteType::SSBO ? tr("SSBO") : tr("Unknown");

      QString slotname = QFormatStr("%1: %2").arg(bindPoint).arg(res.name);
      QString dimensions;
      QString format = lit("-");
      QString access = tr("Read/Write");
      if(im)
      {
        if(im->readAllowed && !im->writeAllowed)
          access = tr("Read-Only");
        if(!im->readAllowed && im->writeAllowed)
          access = tr("Write-Only");
        format = im->imageFormat.Name();
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

        tag = QVariant::fromValue(id);
      }

      BufferDescription *buf = m_Ctx.GetBuffer(id);

      if(buf)
      {
        uint64_t offset = 0;
        uint64_t length = buf->length;
        if(bf && bf->byteSize > 0)
        {
          offset = bf->byteOffset;
          length = bf->byteSize;
        }

        if(offset > 0)
          dimensions = tr("%1 bytes at offset %2 bytes").arg(length).arg(offset);
        else
          dimensions = tr("%1 bytes").arg(length);

        tag = QVariant::fromValue(GLReadWriteTag(i, id, offset, length));
      }

      if(!filledSlot)
      {
        dimensions = lit("-");
        access = lit("-");
      }

      RDTreeWidgetItem *node =
          new RDTreeWidgetItem({binding, slotname, id, dimensions, format, access, QString()});

      node->setTag(tag);

      if(im && tex)
        setViewDetails(node, tex, im->mipLevel, 1);

      if(!filledSlot)
        setEmptyRow(node);

      if(!usedSlot)
        setInactiveRow(node);

      readwrites->addTopLevelItem(node);
    }
  }
  readwrites->clearSelection();
  readwrites->endUpdate();
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
      ret = ret.arg(val.genericValue.uintValue[i]);

    return ret;
  }
  else if(compType == CompType::SInt)
  {
    for(uint32_t i = 0; i < compCount; i++)
      ret = ret.arg(val.genericValue.intValue[i]);

    return ret;
  }
  else
  {
    for(uint32_t i = 0; i < compCount; i++)
      ret = ret.arg(val.genericValue.floatValue[i]);

    return ret;
  }
}

GLPipelineStateViewer::GLReadWriteType GLPipelineStateViewer::GetGLReadWriteType(ShaderResource res)
{
  GLReadWriteType ret = GLReadWriteType::Image;

  if(res.isTexture)
  {
    ret = GLReadWriteType::Image;
  }
  else
  {
    if(res.variableType.descriptor.rows == 1 && res.variableType.descriptor.columns == 1 &&
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
  if(!m_Ctx.IsCaptureLoaded())
  {
    clearState();
    return;
  }

  const GLPipe::State &state = *m_Ctx.CurGLPipelineState();
  const DrawcallDescription *draw = m_Ctx.CurDrawcall();

  bool showUnused = ui->showUnused->isChecked();
  bool showEmpty = ui->showEmpty->isChecked();

  const QPixmap &tick = Pixmaps::tick(this);
  const QPixmap &cross = Pixmaps::cross(this);

  bool usedBindings[128] = {};

  ////////////////////////////////////////////////
  // Vertex Input

  int vs = 0;

  vs = ui->viAttrs->verticalScrollBar()->value();
  ui->viAttrs->beginUpdate();
  ui->viAttrs->clear();
  {
    int i = 0;
    for(const GLPipe::VertexAttribute &a : state.vertexInput.attributes)
    {
      bool filledSlot = true;
      bool usedSlot = false;

      QString name = tr("Attribute %1").arg(i);

      uint32_t compCount = 4;
      CompType compType = CompType::Float;

      if(state.vertexShader.shaderResourceId != ResourceId())
      {
        int attrib = -1;
        if(i < state.vertexShader.bindpointMapping.inputAttributes.count())
          attrib = state.vertexShader.bindpointMapping.inputAttributes[i];

        if(attrib >= 0 && attrib < state.vertexShader.reflection->inputSignature.count())
        {
          name = state.vertexShader.reflection->inputSignature[attrib].varName;
          compCount = state.vertexShader.reflection->inputSignature[attrib].compCount;
          compType = state.vertexShader.reflection->inputSignature[attrib].compType;
          usedSlot = true;
        }
      }

      if(showNode(usedSlot, filledSlot))
      {
        QString genericVal = tr("Generic=") + MakeGenericValueString(compCount, compType, a);

        RDTreeWidgetItem *node =
            new RDTreeWidgetItem({i, a.enabled ? tr("Enabled") : tr("Disabled"), name,
                                  a.enabled ? QString(a.format.Name()) : genericVal,
                                  a.vertexBufferSlot, a.byteOffset, QString()});

        node->setTag(i);

        if(a.enabled)
          usedBindings[a.vertexBufferSlot] = true;

        if(!usedSlot)
          setInactiveRow(node);

        ui->viAttrs->addTopLevelItem(node);
      }

      i++;
    }
  }
  ui->viAttrs->clearSelection();
  ui->viAttrs->endUpdate();
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

  m_Common.setTopologyDiagram(ui->topologyDiagram, topo);

  bool ibufferUsed = draw && (draw->flags & DrawFlags::Indexed);

  if(ibufferUsed)
  {
    ui->primRestart->setVisible(true);
    if(state.vertexInput.primitiveRestart)
      ui->primRestart->setText(
          tr("Restart Idx: 0x%1").arg(Formatter::Format(state.vertexInput.restartIndex, true)));
    else
      ui->primRestart->setText(tr("Restart Idx: Disabled"));
  }
  else
  {
    ui->primRestart->setVisible(false);
  }

  m_VBNodes.clear();
  m_EmptyNodes.clear();

  ui->vaoLabel->setText(ToQStr(state.vertexInput.vertexArrayObject));

  vs = ui->viBuffers->verticalScrollBar()->value();
  ui->viBuffers->beginUpdate();
  ui->viBuffers->clear();

  if(state.vertexInput.indexBuffer != ResourceId())
  {
    if(ibufferUsed || showUnused)
    {
      uint64_t length = 1;

      if(!ibufferUsed)
        length = 0;

      BufferDescription *buf = m_Ctx.GetBuffer(state.vertexInput.indexBuffer);

      if(buf)
        length = buf->length;

      RDTreeWidgetItem *node = new RDTreeWidgetItem({tr("Element"), state.vertexInput.indexBuffer,
                                                     draw ? draw->indexByteWidth : 0, 0, 0,
                                                     (qulonglong)length, QString()});

      QString iformat;
      if(draw)
      {
        if(draw->indexByteWidth == 1)
          iformat = lit("ubyte");
        else if(draw->indexByteWidth == 2)
          iformat = lit("ushort");
        else if(draw->indexByteWidth == 4)
          iformat = lit("uint");

        iformat += lit(" indices[%1]").arg(RENDERDOC_NumVerticesPerPrimitive(draw->topology));
      }

      node->setTag(QVariant::fromValue(GLVBIBTag(state.vertexInput.indexBuffer,
                                                 draw ? draw->indexOffset * draw->indexByteWidth : 0,
                                                 iformat)));

      if(!ibufferUsed)
        setInactiveRow(node);

      if(state.vertexInput.indexBuffer == ResourceId())
      {
        setEmptyRow(node);
        m_EmptyNodes.push_back(node);
      }

      ui->viBuffers->addTopLevelItem(node);
    }
  }
  else
  {
    if(ibufferUsed || showEmpty)
    {
      RDTreeWidgetItem *node = new RDTreeWidgetItem(
          {tr("Element"), tr("No Buffer Set"), lit("-"), lit("-"), lit("-"), lit("-"), QString()});

      QString iformat;
      if(draw)
      {
        if(draw->indexByteWidth == 1)
          iformat = lit("ubyte");
        else if(draw->indexByteWidth == 2)
          iformat = lit("ushort");
        else if(draw->indexByteWidth == 4)
          iformat = lit("uint");

        iformat += lit(" indices[%1]").arg(RENDERDOC_NumVerticesPerPrimitive(draw->topology));
      }

      node->setTag(QVariant::fromValue(GLVBIBTag(state.vertexInput.indexBuffer,
                                                 draw ? draw->indexOffset * draw->indexByteWidth : 0,
                                                 iformat)));

      setEmptyRow(node);
      m_EmptyNodes.push_back(node);

      if(!ibufferUsed)
        setInactiveRow(node);

      ui->viBuffers->addTopLevelItem(node);
    }
  }

  for(int i = 0; i < state.vertexInput.vertexBuffers.count(); i++)
  {
    const GLPipe::VertexBuffer &v = state.vertexInput.vertexBuffers[i];

    bool filledSlot = (v.resourceId != ResourceId());
    bool usedSlot = (usedBindings[i]);

    if(showNode(usedSlot, filledSlot))
    {
      uint64_t length = 0;
      uint64_t offset = v.byteOffset;

      BufferDescription *buf = m_Ctx.GetBuffer(v.resourceId);
      if(buf)
        length = buf->length;

      RDTreeWidgetItem *node =
          new RDTreeWidgetItem({i, v.resourceId, v.byteStride, (qulonglong)offset,
                                v.instanceDivisor, (qulonglong)length, QString()});

      node->setTag(QVariant::fromValue(
          GLVBIBTag(v.resourceId, v.byteOffset, m_Common.GetVBufferFormatString(i))));

      if(!filledSlot)
      {
        setEmptyRow(node);
        m_EmptyNodes.push_back(node);
      }

      if(!usedSlot)
        setInactiveRow(node);

      m_VBNodes.push_back(node);

      ui->viBuffers->addTopLevelItem(node);
    }
    else
    {
      m_VBNodes.push_back(NULL);
    }
  }
  ui->viBuffers->clearSelection();
  ui->viBuffers->endUpdate();
  ui->viBuffers->verticalScrollBar()->setValue(vs);

  setShaderState(state.vertexShader, ui->vsShader, ui->vsTextures, ui->vsSamplers, ui->vsUBOs,
                 ui->vsSubroutines, ui->vsReadWrite);
  setShaderState(state.geometryShader, ui->gsShader, ui->gsTextures, ui->gsSamplers, ui->gsUBOs,
                 ui->gsSubroutines, ui->gsReadWrite);
  setShaderState(state.tessControlShader, ui->tcsShader, ui->tcsTextures, ui->tcsSamplers,
                 ui->tcsUBOs, ui->tcsSubroutines, ui->tcsReadWrite);
  setShaderState(state.tessEvalShader, ui->tesShader, ui->tesTextures, ui->tesSamplers, ui->tesUBOs,
                 ui->tesSubroutines, ui->tesReadWrite);
  setShaderState(state.fragmentShader, ui->fsShader, ui->fsTextures, ui->fsSamplers, ui->fsUBOs,
                 ui->fsSubroutines, ui->fsReadWrite);
  setShaderState(state.computeShader, ui->csShader, ui->csTextures, ui->csSamplers, ui->csUBOs,
                 ui->csSubroutines, ui->csReadWrite);

  QToolButton *shaderButtons[] = {
      ui->vsShaderViewButton, ui->tcsShaderViewButton, ui->tesShaderViewButton,
      ui->gsShaderViewButton, ui->fsShaderViewButton,  ui->csShaderViewButton,
      ui->vsShaderEditButton, ui->tcsShaderEditButton, ui->tesShaderEditButton,
      ui->gsShaderEditButton, ui->fsShaderEditButton,  ui->csShaderEditButton,
      ui->vsShaderSaveButton, ui->tcsShaderSaveButton, ui->tesShaderSaveButton,
      ui->gsShaderSaveButton, ui->fsShaderSaveButton,  ui->csShaderSaveButton,
  };

  for(QToolButton *b : shaderButtons)
  {
    const GLPipe::Shader *stage = stageForSender(b);

    if(stage == NULL || stage->shaderResourceId == ResourceId())
      continue;

    ShaderReflection *shaderDetails = stage->reflection;

    b->setEnabled(shaderDetails != NULL);

    m_Common.SetupShaderEditButton(b, ResourceId(), stage->shaderResourceId, shaderDetails);
  }

  vs = ui->xfbBuffers->verticalScrollBar()->value();
  ui->xfbBuffers->beginUpdate();
  ui->xfbBuffers->clear();
  ui->xfbObj->setText(ToQStr(state.transformFeedback.feedbackResourceId));
  if(state.transformFeedback.active)
  {
    ui->xfbPaused->setPixmap(state.transformFeedback.paused ? tick : cross);
    for(int i = 0; i < (int)ARRAY_COUNT(state.transformFeedback.bufferResourceId); i++)
    {
      bool filledSlot = (state.transformFeedback.bufferResourceId[i] != ResourceId());
      bool usedSlot = (filledSlot);

      if(showNode(usedSlot, filledSlot))
      {
        qulonglong length = state.transformFeedback.byteSize[i];

        BufferDescription *buf = m_Ctx.GetBuffer(state.transformFeedback.bufferResourceId[i]);

        if(buf)
          length = buf->length;

        RDTreeWidgetItem *node =
            new RDTreeWidgetItem({i, state.transformFeedback.bufferResourceId[i], length,
                                  (qulonglong)state.transformFeedback.byteOffset[i], QString()});

        node->setTag(QVariant::fromValue(state.transformFeedback.bufferResourceId[i]));

        if(!filledSlot)
          setEmptyRow(node);

        if(!usedSlot)
          setInactiveRow(node);

        ui->xfbBuffers->addTopLevelItem(node);
      }
    }
  }
  ui->xfbBuffers->verticalScrollBar()->setValue(vs);
  ui->xfbBuffers->clearSelection();
  ui->xfbBuffers->endUpdate();

  ui->xfbGroup->setVisible(state.transformFeedback.active);

  ////////////////////////////////////////////////
  // Rasterizer

  vs = ui->viewports->verticalScrollBar()->value();
  ui->viewports->beginUpdate();
  ui->viewports->clear();

  {
    // accumulate identical viewports to save on visual repetition
    int prev = 0;
    for(int i = 0; i < state.rasterizer.viewports.count(); i++)
    {
      const Viewport &v1 = state.rasterizer.viewports[prev];
      const Viewport &v2 = state.rasterizer.viewports[i];

      if(v1.width != v2.width || v1.height != v2.height || v1.x != v2.x || v1.y != v2.y ||
         v1.minDepth != v2.minDepth || v1.maxDepth != v2.maxDepth)
      {
        if(v1.width != v1.height || v1.width != 0 || v1.height != 0 || v1.minDepth != v1.maxDepth ||
           ui->showEmpty->isChecked())
        {
          QString indexstring;
          if(prev < i - 1)
            indexstring = QFormatStr("%1-%2").arg(prev).arg(i - 1);
          else
            indexstring = QString::number(prev);

          RDTreeWidgetItem *node = new RDTreeWidgetItem(
              {indexstring, v1.x, v1.y, v1.width, v1.height, v1.minDepth, v1.maxDepth});

          if(v1.width == 0 || v1.height == 0 || v1.minDepth == v1.maxDepth)
            setEmptyRow(node);

          ui->viewports->addTopLevelItem(node);
        }

        prev = i;
      }
    }

    // handle the last batch (the loop above leaves the last batch un-added)
    if(prev < state.rasterizer.viewports.count())
    {
      const Viewport &v1 = state.rasterizer.viewports[prev];

      // must display at least one viewport - otherwise if they are
      // all empty we get an empty list - we want a nice obvious
      // 'invalid viewport' entry. So check if last is 0

      if(v1.width != v1.height || v1.width != 0 || v1.height != 0 || v1.minDepth != v1.maxDepth ||
         ui->showEmpty->isChecked() || prev == 0)
      {
        QString indexstring;
        if(prev < state.rasterizer.viewports.count() - 1)
          indexstring = QFormatStr("%1-%2").arg(prev).arg(state.rasterizer.viewports.count() - 1);
        else
          indexstring = QString::number(prev);

        RDTreeWidgetItem *node = new RDTreeWidgetItem(
            {indexstring, v1.x, v1.y, v1.width, v1.height, v1.minDepth, v1.maxDepth});

        if(v1.width == 0 || v1.height == 0 || v1.minDepth == v1.maxDepth)
          setEmptyRow(node);

        ui->viewports->addTopLevelItem(node);
      }
    }
  }
  ui->viewports->verticalScrollBar()->setValue(vs);
  ui->viewports->clearSelection();
  ui->viewports->endUpdate();

  bool anyScissorEnable = false;

  vs = ui->scissors->verticalScrollBar()->value();
  ui->scissors->beginUpdate();
  ui->scissors->clear();
  {
    // accumulate identical scissors to save on visual repetition
    int prev = 0;
    for(int i = 0; i < state.rasterizer.scissors.count(); i++)
    {
      const Scissor &s1 = state.rasterizer.scissors[prev];
      const Scissor &s2 = state.rasterizer.scissors[i];

      if(s1.width != s2.width || s1.height != s2.height || s1.x != s2.x || s1.y != s2.y ||
         s1.enabled != s2.enabled)
      {
        if(s1.enabled || ui->showEmpty->isChecked())
        {
          QString indexstring;
          if(prev < i - 1)
            indexstring = QFormatStr("%1-%2").arg(prev).arg(i - 1);
          else
            indexstring = QString::number(prev);

          RDTreeWidgetItem *node = new RDTreeWidgetItem({indexstring, s1.x, s1.y, s1.width, s1.height,
                                                         s1.enabled ? tr("True") : tr("False")});

          if(s1.width == 0 || s1.height == 0)
            setEmptyRow(node);

          if(!s1.enabled)
            setInactiveRow(node);

          anyScissorEnable = anyScissorEnable || s1.enabled;

          ui->scissors->addTopLevelItem(node);
        }

        prev = i;
      }
    }

    // handle the last batch (the loop above leaves the last batch un-added)
    if(prev < state.rasterizer.scissors.count())
    {
      const Scissor &s1 = state.rasterizer.scissors[prev];

      if(s1.enabled || ui->showEmpty->isChecked())
      {
        QString indexstring;
        if(prev < state.rasterizer.scissors.count() - 1)
          indexstring = QFormatStr("%1-%2").arg(prev).arg(state.rasterizer.scissors.count() - 1);
        else
          indexstring = QString::number(prev);

        RDTreeWidgetItem *node = new RDTreeWidgetItem(
            {indexstring, s1.x, s1.y, s1.width, s1.height, s1.enabled ? tr("True") : tr("False")});

        if(s1.width == 0 || s1.height == 0)
          setEmptyRow(node);

        if(!s1.enabled)
          setInactiveRow(node);

        anyScissorEnable = anyScissorEnable || s1.enabled;

        ui->scissors->addTopLevelItem(node);
      }
    }
  }
  ui->scissors->clearSelection();
  ui->scissors->verticalScrollBar()->setValue(vs);
  ui->scissors->endUpdate();

  ui->fillMode->setText(ToQStr(state.rasterizer.state.fillMode));
  ui->cullMode->setText(ToQStr(state.rasterizer.state.cullMode));
  ui->frontCCW->setPixmap(state.rasterizer.state.frontCCW ? tick : cross);

  ui->scissorEnabled->setPixmap(anyScissorEnable ? tick : cross);
  ui->provoking->setText(state.vertexInput.provokingVertexLast ? tr("Last") : tr("First"));

  ui->rasterizerDiscard->setPixmap(state.vertexProcessing.discard ? tick : cross);

  if(state.rasterizer.state.programmablePointSize)
    ui->pointSize->setText(tr("Program", "ProgrammablePointSize"));
  else
    ui->pointSize->setText(Formatter::Format(state.rasterizer.state.pointSize));
  ui->lineWidth->setText(Formatter::Format(state.rasterizer.state.lineWidth));

  QString clipSetup;
  if(state.vertexProcessing.clipOriginLowerLeft)
    clipSetup += tr("0,0 Lower Left");
  else
    clipSetup += tr("0,0 Upper Left");
  clipSetup += lit(", ");
  if(state.vertexProcessing.clipNegativeOneToOne)
    clipSetup += lit("Z= -1 to 1");
  else
    clipSetup += lit("Z= 0 to 1");

  ui->clipSetup->setText(clipSetup);

  QString clipDistances;

  int numDist = 0;
  for(int i = 0; i < (int)ARRAY_COUNT(state.vertexProcessing.clipPlanes); i++)
  {
    if(state.vertexProcessing.clipPlanes[i])
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

  ui->depthClamp->setPixmap(state.rasterizer.state.depthClamp ? tick : cross);
  ui->depthBias->setText(Formatter::Format(state.rasterizer.state.depthBias));
  ui->slopeScaledBias->setText(Formatter::Format(state.rasterizer.state.slopeScaledDepthBias));

  if(state.rasterizer.state.offsetClamp == 0.0f || qIsNaN(state.rasterizer.state.offsetClamp))
  {
    ui->offsetClamp->setText(QString());
    ui->offsetClamp->setPixmap(cross);
  }
  else
  {
    ui->offsetClamp->setPixmap(QPixmap());
    ui->offsetClamp->setText(Formatter::Format(state.rasterizer.state.offsetClamp));
  }

  ui->multisample->setPixmap(state.rasterizer.state.multisampleEnable ? tick : cross);
  ui->sampleShading->setPixmap(state.rasterizer.state.sampleShading ? tick : cross);
  ui->minSampleShading->setText(Formatter::Format(state.rasterizer.state.minSampleShadingRate));
  ui->alphaToCoverage->setPixmap(state.rasterizer.state.alphaToCoverage ? tick : cross);
  ui->alphaToOne->setPixmap(state.rasterizer.state.alphaToOne ? tick : cross);
  if(state.rasterizer.state.sampleCoverage)
  {
    QString sampleCoverage = Formatter::Format(state.rasterizer.state.sampleCoverageValue);
    if(state.rasterizer.state.sampleCoverageInvert)
      sampleCoverage += tr(" inverted");
    ui->sampleCoverage->setPixmap(QPixmap());
    ui->sampleCoverage->setText(sampleCoverage);
  }
  else
  {
    ui->sampleCoverage->setText(QString());
    ui->sampleCoverage->setPixmap(cross);
  }

  if(state.rasterizer.state.sampleMask)
  {
    ui->sampleMask->setPixmap(QPixmap());
    ui->sampleMask->setText(Formatter::Format(state.rasterizer.state.sampleMaskValue, true));
  }
  else
  {
    ui->sampleMask->setText(QString());
    ui->sampleMask->setPixmap(cross);
  }

  ////////////////////////////////////////////////
  // Output Merger

  bool targets[32] = {};

  ui->drawFBO->setText(QFormatStr("Draw FBO: %1").arg(ToQStr(state.framebuffer.drawFBO.resourceId)));
  ui->readFBO->setText(QFormatStr("Read FBO: %1").arg(ToQStr(state.framebuffer.readFBO.resourceId)));

  vs = ui->framebuffer->verticalScrollBar()->value();
  ui->framebuffer->beginUpdate();
  ui->framebuffer->clear();
  {
    int i = 0;
    for(int db : state.framebuffer.drawFBO.drawBuffers)
    {
      ResourceId p;
      const GLPipe::Attachment *r = NULL;

      if(db >= 0 && db < state.framebuffer.drawFBO.colorAttachments.count())
      {
        p = state.framebuffer.drawFBO.colorAttachments[db].resourceId;
        r = &state.framebuffer.drawFBO.colorAttachments[db];
      }

      bool filledSlot = (p != ResourceId());
      bool usedSlot = db >= 0;

      if(showNode(usedSlot, filledSlot))
      {
        uint32_t w = 1, h = 1, d = 1;
        uint32_t a = 1;
        QString format = tr("Unknown");
        QString typeName = tr("Unknown");

        if(p == ResourceId())
        {
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
          format = tex->format.Name();
          typeName = ToQStr(tex->type);

          if(tex->format.SRGBCorrected() && !state.framebuffer.framebufferSRGB)
            format += lit(" (GL_FRAMEBUFFER_SRGB = 0)");
        }

        if(r && (r->swizzle[0] != TextureSwizzle::Red || r->swizzle[1] != TextureSwizzle::Green ||
                 r->swizzle[2] != TextureSwizzle::Blue || r->swizzle[3] != TextureSwizzle::Alpha))
        {
          format += tr(" swizzle[%1%2%3%4]")
                        .arg(ToQStr(r->swizzle[0]))
                        .arg(ToQStr(r->swizzle[1]))
                        .arg(ToQStr(r->swizzle[2]))
                        .arg(ToQStr(r->swizzle[3]));
        }

        QString slotname = QString::number(i);

        if(state.fragmentShader.reflection)
        {
          for(int s = 0; s < state.fragmentShader.reflection->outputSignature.count(); s++)
          {
            if(state.fragmentShader.reflection->outputSignature[s].regIndex == (uint32_t)db &&
               (state.fragmentShader.reflection->outputSignature[s].systemValue ==
                    ShaderBuiltin::Undefined ||
                state.fragmentShader.reflection->outputSignature[s].systemValue ==
                    ShaderBuiltin::ColorOutput))
            {
              slotname +=
                  QFormatStr(": %1").arg(state.fragmentShader.reflection->outputSignature[s].varName);
            }
          }
        }

        RDTreeWidgetItem *node =
            new RDTreeWidgetItem({i, p, typeName, w, h, d, a, format, QString()});

        if(tex)
        {
          if(r)
            setViewDetails(node, tex, r->mipLevel, 1);
          node->setTag(QVariant::fromValue(p));
        }

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
        state.framebuffer.drawFBO.depthAttachment.resourceId,
        state.framebuffer.drawFBO.stencilAttachment.resourceId,
    };

    uint32_t dsMips[] = {
        state.framebuffer.drawFBO.depthAttachment.mipLevel,
        state.framebuffer.drawFBO.stencilAttachment.mipLevel,
    };

    for(int dsIdx = 0; dsIdx < 2; dsIdx++)
    {
      ResourceId ds = dsObjects[dsIdx];
      uint32_t mip = dsMips[dsIdx];

      bool filledSlot = (ds != ResourceId());
      bool usedSlot = filledSlot;
      if(showNode(usedSlot, filledSlot))
      {
        uint32_t w = 1, h = 1, d = 1;
        uint32_t a = 1;
        QString format = tr("Unknown");
        QString typeName = tr("Unknown");

        if(ds == ResourceId())
        {
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
          format = tex->format.Name();
          typeName = ToQStr(tex->type);
        }

        QString slot = tr("Depth Only");
        if(i == 1)
          slot = tr("Stencil Only");

        bool depthstencil = false;

        if(state.framebuffer.drawFBO.depthAttachment.resourceId ==
               state.framebuffer.drawFBO.stencilAttachment.resourceId &&
           state.framebuffer.drawFBO.depthAttachment.resourceId != ResourceId())
        {
          depthstencil = true;
          slot = tr("Depth-Stencil");
        }

        RDTreeWidgetItem *node =
            new RDTreeWidgetItem({slot, ds, typeName, w, h, d, a, format, QString()});

        if(tex)
        {
          setViewDetails(node, tex, mip, 1);
          node->setTag(QVariant::fromValue(ds));
        }

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
  ui->framebuffer->endUpdate();
  ui->framebuffer->verticalScrollBar()->setValue(vs);

  vs = ui->blends->verticalScrollBar()->value();
  ui->blends->beginUpdate();
  ui->blends->clear();
  {
    bool logic = state.framebuffer.blendState.blends[0].logicOperationEnabled &&
                 state.framebuffer.blendState.blends[0].logicOperation != LogicOperation::NoOp;

    int i = 0;
    for(const ColorBlend &blend : state.framebuffer.blendState.blends)
    {
      bool filledSlot = (blend.enabled || targets[i]);
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

                                       lit("-"), lit("-"), ToQStr(blend.logicOperation),

                                       lit("-"), lit("-"), lit("-"),

                                       QFormatStr("%1%2%3%4")
                                           .arg((blend.writeMask & 0x1) == 0 ? lit("_") : lit("R"))
                                           .arg((blend.writeMask & 0x2) == 0 ? lit("_") : lit("G"))
                                           .arg((blend.writeMask & 0x4) == 0 ? lit("_") : lit("B"))
                                           .arg((blend.writeMask & 0x8) == 0 ? lit("_") : lit("A"))});
        }
        else
        {
          node = new RDTreeWidgetItem(
              {i, blend.enabled ? tr("True") : tr("False"),

               ToQStr(blend.colorBlend.source), ToQStr(blend.colorBlend.destination),
               ToQStr(blend.colorBlend.operation),

               ToQStr(blend.alphaBlend.source), ToQStr(blend.alphaBlend.destination),
               ToQStr(blend.alphaBlend.operation),

               QFormatStr("%1%2%3%4")
                   .arg((blend.writeMask & 0x1) == 0 ? lit("_") : lit("R"))
                   .arg((blend.writeMask & 0x2) == 0 ? lit("_") : lit("G"))
                   .arg((blend.writeMask & 0x4) == 0 ? lit("_") : lit("B"))
                   .arg((blend.writeMask & 0x8) == 0 ? lit("_") : lit("A"))});
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
  ui->blends->endUpdate();
  ui->blends->verticalScrollBar()->setValue(vs);

  ui->blendFactor->setText(QFormatStr("%1, %2, %3, %4")
                               .arg(state.framebuffer.blendState.blendFactor[0], 0, 'f', 2)
                               .arg(state.framebuffer.blendState.blendFactor[1], 0, 'f', 2)
                               .arg(state.framebuffer.blendState.blendFactor[2], 0, 'f', 2)
                               .arg(state.framebuffer.blendState.blendFactor[3], 0, 'f', 2));

  ui->depthEnabled->setPixmap(state.depthState.depthEnable ? tick : cross);
  ui->depthFunc->setText(ToQStr(state.depthState.depthFunction));
  ui->depthWrite->setPixmap(state.depthState.depthWrites ? tick : cross);

  if(state.depthState.depthBounds)
  {
    ui->depthBounds->setPixmap(QPixmap());
    ui->depthBounds->setText(Formatter::Format(state.depthState.nearBound) + lit("-") +
                             Formatter::Format(state.depthState.farBound));
  }
  else
  {
    ui->depthBounds->setText(QString());
    ui->depthBounds->setPixmap(cross);
  }

  ui->stencils->beginUpdate();
  ui->stencils->clear();
  if(state.stencilState.stencilEnable)
  {
    ui->stencils->addTopLevelItem(new RDTreeWidgetItem(
        {tr("Front"), ToQStr(state.stencilState.frontFace.function),
         ToQStr(state.stencilState.frontFace.failOperation),
         ToQStr(state.stencilState.frontFace.depthFailOperation),
         ToQStr(state.stencilState.frontFace.passOperation),
         Formatter::Format((uint8_t)state.stencilState.frontFace.writeMask, true),
         Formatter::Format((uint8_t)state.stencilState.frontFace.compareMask, true),
         Formatter::Format((uint8_t)state.stencilState.frontFace.reference, true)}));

    ui->stencils->addTopLevelItem(new RDTreeWidgetItem(
        {tr("Back"), ToQStr(state.stencilState.backFace.function),
         ToQStr(state.stencilState.backFace.failOperation),
         ToQStr(state.stencilState.backFace.depthFailOperation),
         ToQStr(state.stencilState.backFace.passOperation),
         Formatter::Format((uint8_t)state.stencilState.backFace.writeMask, true),
         Formatter::Format((uint8_t)state.stencilState.backFace.compareMask, true),
         Formatter::Format((uint8_t)state.stencilState.backFace.reference, true)}));
  }
  else
  {
    ui->stencils->addTopLevelItem(new RDTreeWidgetItem(
        {tr("Front"), lit("-"), lit("-"), lit("-"), lit("-"), lit("-"), lit("-"), lit("-")}));
    ui->stencils->addTopLevelItem(new RDTreeWidgetItem(
        {tr("Back"), lit("-"), lit("-"), lit("-"), lit("-"), lit("-"), lit("-"), lit("-")}));
  }
  ui->stencils->clearSelection();
  ui->stencils->endUpdate();

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
    bool raster = true;
    bool fbo = true;

    if(state.vertexProcessing.discard)
    {
      raster = fbo = false;
    }

    if(state.geometryShader.shaderResourceId == ResourceId() && state.transformFeedback.active)
    {
      ui->pipeFlow->setStageName(4, lit("XFB"), tr("Transform Feedback"));
    }
    else
    {
      ui->pipeFlow->setStageName(4, lit("GS"), tr("Geometry Shader"));
    }

    ui->pipeFlow->setStagesEnabled(
        {true, true, state.tessControlShader.shaderResourceId != ResourceId(),
         state.tessEvalShader.shaderResourceId != ResourceId(),
         state.geometryShader.shaderResourceId != ResourceId() || state.transformFeedback.active,
         raster,
         !state.vertexProcessing.discard && state.fragmentShader.shaderResourceId != ResourceId(),
         fbo, false});
  }
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
      if(tex->type == TextureType::Buffer)
      {
        IBufferViewer *viewer = m_Ctx.ViewTextureAsBuffer(
            0, 0, tex->resourceId, FormatElement::GenerateTextureBufferFormat(*tex));

        m_Ctx.AddDockWindow(viewer->Widget(), DockReference::AddTo, this);
      }
      else
      {
        if(!m_Ctx.HasTextureViewer())
          m_Ctx.ShowTextureViewer();
        ITextureViewer *viewer = m_Ctx.GetTextureViewer();
        viewer->ViewTexture(tex->resourceId, true);
      }

      return;
    }
  }
  else if(tag.canConvert<GLReadWriteTag>())
  {
    GLReadWriteTag buf = tag.value<GLReadWriteTag>();

    const ShaderResource &shaderRes = stage->reflection->readWriteResources[buf.bindPoint];

    QString format = m_Common.GenerateBufferFormatter(shaderRes, ResourceFormat(), buf.offset);

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

  m_Ctx.AddDockWindow(prev->Widget(), DockReference::TransientPopupArea, this, 0.3f);
}

void GLPipelineStateViewer::on_viAttrs_itemActivated(RDTreeWidgetItem *item, int column)
{
  on_meshView_clicked();
}

void GLPipelineStateViewer::on_viBuffers_itemActivated(RDTreeWidgetItem *item, int column)
{
  QVariant tag = item->tag();

  if(tag.canConvert<GLVBIBTag>())
  {
    GLVBIBTag buf = tag.value<GLVBIBTag>();

    if(buf.id != ResourceId())
    {
      IBufferViewer *viewer = m_Ctx.ViewBuffer(buf.offset, UINT64_MAX, buf.id, buf.format);

      m_Ctx.AddDockWindow(viewer->Widget(), DockReference::AddTo, this);
    }
  }
}

void GLPipelineStateViewer::highlightIABind(int slot)
{
  int idx = ((slot + 1) * 21) % 32;    // space neighbouring colours reasonably distinctly

  const GLPipe::VertexInput &VI = m_Ctx.CurGLPipelineState()->vertexInput;

  QColor col = QColor::fromHslF(float(idx) / 32.0f, 1.0f,
                                qBound(0.05, palette().color(QPalette::Base).lightnessF(), 0.95));

  ui->viAttrs->beginUpdate();
  ui->viBuffers->beginUpdate();

  if(slot < m_VBNodes.count())
  {
    if(m_VBNodes[slot] && !m_EmptyNodes.contains(m_VBNodes[slot]))
    {
      m_VBNodes[slot]->setBackgroundColor(col);
      m_VBNodes[slot]->setForegroundColor(contrastingColor(col, QColor(0, 0, 0)));
    }
  }

  for(int i = 0; i < ui->viAttrs->topLevelItemCount(); i++)
  {
    RDTreeWidgetItem *item = ui->viAttrs->topLevelItem(i);

    if((int)VI.attributes[item->tag().toUInt()].vertexBufferSlot != slot)
    {
      item->setBackground(QBrush());
      item->setForeground(QBrush());
    }
    else
    {
      item->setBackgroundColor(col);
      item->setForegroundColor(contrastingColor(col, QColor(0, 0, 0)));
    }
  }

  ui->viAttrs->endUpdate();
  ui->viBuffers->endUpdate();
}

void GLPipelineStateViewer::on_viAttrs_mouseMove(QMouseEvent *e)
{
  if(!m_Ctx.IsCaptureLoaded())
    return;

  RDTreeWidgetItem *item = ui->viAttrs->itemAt(e->pos());

  vertex_leave(NULL);

  const GLPipe::VertexInput &VI = m_Ctx.CurGLPipelineState()->vertexInput;

  if(item)
  {
    uint32_t buffer = VI.attributes[item->tag().toUInt()].vertexBufferSlot;

    highlightIABind((int)buffer);
  }
}

void GLPipelineStateViewer::on_viBuffers_mouseMove(QMouseEvent *e)
{
  if(!m_Ctx.IsCaptureLoaded())
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
      if(!m_EmptyNodes.contains(item))
      {
        item->setBackground(ui->viBuffers->palette().brush(QPalette::Window));
        item->setForeground(ui->viBuffers->palette().brush(QPalette::WindowText));
      }
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
    RDTreeWidgetItem *item = ui->viBuffers->topLevelItem(i);

    if(m_EmptyNodes.contains(item))
      continue;

    item->setBackground(QBrush());
    item->setForeground(QBrush());
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

  if(stage == NULL || stage->shaderResourceId == ResourceId())
    return;

  ShaderReflection *shaderDetails = stage->reflection;

  if(!shaderDetails)
    return;

  IShaderViewer *shad = m_Ctx.ViewShader(shaderDetails, ResourceId());

  m_Ctx.AddDockWindow(shad->Widget(), DockReference::AddTo, this);
}

void GLPipelineStateViewer::shaderSave_clicked()
{
  const GLPipe::Shader *stage = stageForSender(qobject_cast<QWidget *>(QObject::sender()));

  if(stage == NULL)
    return;

  ShaderReflection *shaderDetails = stage->reflection;

  if(stage->shaderResourceId == ResourceId())
    return;

  m_Common.SaveShaderFile(shaderDetails);
}

void GLPipelineStateViewer::exportHTML(QXmlStreamWriter &xml, const GLPipe::VertexInput &vtx)
{
  const GLPipe::State &pipe = *m_Ctx.CurGLPipelineState();
  {
    xml.writeStartElement(tr("h3"));
    xml.writeCharacters(tr("Vertex Attributes"));
    xml.writeEndElement();

    QList<QVariantList> rows;

    int i = 0;
    for(const GLPipe::VertexAttribute &a : vtx.attributes)
    {
      QString generic;
      if(!a.enabled)
        generic = MakeGenericValueString(a.format.compCount, a.format.compType, a);
      rows.push_back({i, (bool)a.enabled, a.vertexBufferSlot, a.format.Name(), a.byteOffset, generic});

      i++;
    }

    m_Common.exportHTMLTable(xml, {tr("Slot"), tr("Enabled"), tr("Vertex Buffer Slot"),
                                   tr("Format"), tr("Relative Offset"), tr("Generic Value")},
                             rows);
  }

  {
    xml.writeStartElement(tr("h3"));
    xml.writeCharacters(tr("Vertex Buffers"));
    xml.writeEndElement();

    QList<QVariantList> rows;

    int i = 0;
    for(const GLPipe::VertexBuffer &vb : vtx.vertexBuffers)
    {
      QString name = m_Ctx.GetResourceName(vb.resourceId);
      uint64_t length = 0;

      if(vb.resourceId == ResourceId())
      {
        continue;
      }
      else
      {
        BufferDescription *buf = m_Ctx.GetBuffer(vb.resourceId);
        if(buf)
          length = buf->length;
      }

      rows.push_back({i, name, vb.byteStride, vb.byteOffset, vb.instanceDivisor, (qulonglong)length});

      i++;
    }

    m_Common.exportHTMLTable(xml, {tr("Slot"), tr("Buffer"), tr("Stride"), tr("Offset"),
                                   tr("Instance Divisor"), tr("Byte Length")},
                             rows);
  }

  {
    xml.writeStartElement(tr("h3"));
    xml.writeCharacters(tr("Index Buffer"));
    xml.writeEndElement();

    QString name = m_Ctx.GetResourceName(vtx.indexBuffer);
    uint64_t length = 0;

    if(vtx.indexBuffer == ResourceId())
    {
      name = tr("Empty");
    }
    else
    {
      BufferDescription *buf = m_Ctx.GetBuffer(vtx.indexBuffer);
      if(buf)
        length = buf->length;
    }

    QString ifmt = lit("UNKNOWN");
    if(m_Ctx.CurDrawcall()->indexByteWidth == 2)
      ifmt = lit("R16_UINT");
    if(m_Ctx.CurDrawcall()->indexByteWidth == 4)
      ifmt = lit("R32_UINT");

    m_Common.exportHTMLTable(xml, {tr("Buffer"), tr("Format"), tr("Byte Length")},
                             {name, ifmt, (qulonglong)length});
  }

  xml.writeStartElement(tr("p"));
  xml.writeEndElement();

  m_Common.exportHTMLTable(xml, {tr("Primitive Topology")}, {ToQStr(m_Ctx.CurDrawcall()->topology)});

  {
    xml.writeStartElement(tr("h3"));
    xml.writeCharacters(tr("States"));
    xml.writeEndElement();

    m_Common.exportHTMLTable(
        xml, {tr("Primitive Restart"), tr("Restart Index"), tr("Provoking Vertex Last")},
        {(bool)vtx.primitiveRestart, vtx.restartIndex,
         vtx.provokingVertexLast ? tr("Yes") : tr("No")});

    xml.writeStartElement(tr("p"));
    xml.writeEndElement();

    m_Common.exportHTMLTable(
        xml, {tr("Rasterizer Discard"), tr("Clip Origin Lower Left"), tr("Clip Space Z")},
        {pipe.vertexProcessing.discard ? tr("Yes") : tr("No"),
         pipe.vertexProcessing.clipOriginLowerLeft ? tr("Yes") : tr("No"),
         pipe.vertexProcessing.clipNegativeOneToOne ? tr("-1 to 1") : tr("0 to 1")});

    xml.writeStartElement(tr("p"));
    xml.writeEndElement();

    QList<QVariantList> clipPlaneRows;

    for(int i = 0; i < 8; i++)
      clipPlaneRows.push_back({i, pipe.vertexProcessing.clipPlanes[i] ? tr("Yes") : tr("No")});

    m_Common.exportHTMLTable(xml,
                             {
                                 tr("User Clip Plane"), tr("Enabled"),
                             },
                             clipPlaneRows);

    xml.writeStartElement(tr("p"));
    xml.writeEndElement();

    m_Common.exportHTMLTable(
        xml,
        {
            tr("Default Inner Tessellation Level"), tr("Default Outer Tessellation level"),
        },
        {
            QFormatStr("%1, %2")
                .arg(pipe.vertexProcessing.defaultInnerLevel[0])
                .arg(pipe.vertexProcessing.defaultInnerLevel[1]),

            QFormatStr("%1, %2, %3, %4")
                .arg(pipe.vertexProcessing.defaultOuterLevel[0])
                .arg(pipe.vertexProcessing.defaultOuterLevel[1])
                .arg(pipe.vertexProcessing.defaultOuterLevel[2])
                .arg(pipe.vertexProcessing.defaultOuterLevel[3]),
        });
  }
}

void GLPipelineStateViewer::exportHTML(QXmlStreamWriter &xml, const GLPipe::Shader &sh)
{
  const GLPipe::State &pipe = *m_Ctx.CurGLPipelineState();
  ShaderReflection *shaderDetails = sh.reflection;
  const ShaderBindpointMapping &mapping = sh.bindpointMapping;

  {
    xml.writeStartElement(tr("h3"));
    xml.writeCharacters(tr("Shader"));
    xml.writeEndElement();

    QString shadername = tr("Unknown");

    if(sh.shaderResourceId == ResourceId())
      shadername = tr("Unbound");
    else
      shadername = m_Ctx.GetResourceName(sh.shaderResourceId);

    if(sh.shaderResourceId == ResourceId())
    {
      shadername = tr("Unbound");
    }
    else
    {
      QString shname = tr("%1 Shader").arg(ToQStr(sh.stage, GraphicsAPI::OpenGL));

      if(m_Ctx.IsAutogeneratedName(sh.shaderResourceId) &&
         m_Ctx.IsAutogeneratedName(sh.programResourceId) &&
         m_Ctx.IsAutogeneratedName(pipe.pipelineResourceId))
      {
        shadername = QFormatStr("%1 %2").arg(shname).arg(ToQStr(sh.shaderResourceId));
      }
      else
      {
        if(!m_Ctx.IsAutogeneratedName(sh.shaderResourceId))
          shname = m_Ctx.GetResourceName(sh.shaderResourceId);

        if(!m_Ctx.IsAutogeneratedName(sh.programResourceId))
          shname = QFormatStr("%1 - %2").arg(m_Ctx.GetResourceName(sh.programResourceId)).arg(shname);

        if(!m_Ctx.IsAutogeneratedName(pipe.pipelineResourceId))
          shname =
              QFormatStr("%1 - %2").arg(m_Ctx.GetResourceName(pipe.pipelineResourceId)).arg(shname);

        shadername = shname;
      }
    }

    xml.writeStartElement(tr("p"));
    xml.writeCharacters(shadername);
    xml.writeEndElement();

    if(sh.shaderResourceId == ResourceId())
      return;
  }

  QList<QVariantList> textureRows;
  QList<QVariantList> samplerRows;
  QList<QVariantList> cbufferRows;
  QList<QVariantList> readwriteRows;
  QList<QVariantList> subRows;

  for(int i = 0; i < pipe.textures.count(); i++)
  {
    const GLPipe::Texture &r = pipe.textures[i];
    const GLPipe::Sampler &s = pipe.samplers[i];

    const ShaderResource *shaderInput = NULL;
    const Bindpoint *map = NULL;

    if(shaderDetails)
    {
      for(const ShaderResource &bind : shaderDetails->readOnlyResources)
      {
        if(bind.isReadOnly && mapping.readOnlyResources[bind.bindPoint].bind == i)
        {
          shaderInput = &bind;
          map = &mapping.readOnlyResources[bind.bindPoint];
        }
      }
    }

    bool filledSlot = (r.resourceId != ResourceId());
    bool usedSlot = (shaderInput && map->used);

    if(shaderInput)
    {
      // do texture
      {
        QString slotname = QString::number(i);

        if(shaderInput && !shaderInput->name.isEmpty())
          slotname += QFormatStr(": %1").arg(shaderInput->name);

        uint32_t w = 1, h = 1, d = 1;
        uint32_t a = 1;
        QString format = tr("Unknown");
        QString name = m_Ctx.GetResourceName(r.resourceId);
        QString typeName = tr("Unknown");

        if(!filledSlot)
        {
          name = tr("Empty");
          format = lit("-");
          typeName = lit("-");
          w = h = d = a = 0;
        }

        TextureDescription *tex = m_Ctx.GetTexture(r.resourceId);
        if(tex)
        {
          w = tex->width;
          h = tex->height;
          d = tex->depth;
          a = tex->arraysize;
          format = tex->format.Name();
          typeName = ToQStr(tex->type);

          if(tex->format.type == ResourceFormatType::D16S8 ||
             tex->format.type == ResourceFormatType::D24S8 ||
             tex->format.type == ResourceFormatType::D32S8)
          {
            if(r.depthReadChannel == 0)
              format += tr(" Depth-Repipead");
            else if(r.depthReadChannel == 1)
              format += tr(" Stencil-Read");
          }
          else if(r.swizzle[0] != TextureSwizzle::Red || r.swizzle[1] != TextureSwizzle::Green ||
                  r.swizzle[2] != TextureSwizzle::Blue || r.swizzle[3] != TextureSwizzle::Alpha)
          {
            format += QFormatStr(" swizzle[%1%2%3%4]")
                          .arg(ToQStr(r.swizzle[0]))
                          .arg(ToQStr(r.swizzle[1]))
                          .arg(ToQStr(r.swizzle[2]))
                          .arg(ToQStr(r.swizzle[3]));
          }
        }

        textureRows.push_back({slotname, name, typeName, w, h, d, a, format, r.firstMip, r.numMips});
      }

      // do sampler
      {
        QString slotname = QString::number(i);

        if(shaderInput && !shaderInput->name.isEmpty())
          slotname += QFormatStr(": %1").arg(shaderInput->name);

        QString borderColor = QFormatStr("%1, %2, %3, %4")
                                  .arg(s.borderColor[0])
                                  .arg(s.borderColor[1])
                                  .arg(s.borderColor[2])
                                  .arg(s.borderColor[3]);

        QString addressing;

        QString addPrefix;
        QString addVal;

        QString addr[] = {ToQStr(s.addressS, GraphicsAPI::OpenGL),
                          ToQStr(s.addressT, GraphicsAPI::OpenGL),
                          ToQStr(s.addressR, GraphicsAPI::OpenGL)};

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

        if(r.type == TextureType::TextureCube || r.type == TextureType::TextureCubeArray)
        {
          addressing += s.seamlessCubeMap ? tr(" Seamless") : tr(" Non-Seamless");
        }

        QString filter = ToQStr(s.filter);

        if(s.maxAnisotropy > 1)
          filter += tr(" Aniso%1x").arg(s.maxAnisotropy);

        if(s.filter.filter == FilterFunction::Comparison)
          filter += QFormatStr(" %1").arg(ToQStr(s.compareFunction));
        else if(s.filter.filter != FilterFunction::Normal)
          filter += QFormatStr(" (%1)").arg(ToQStr(s.filter.filter));

        samplerRows.push_back(
            {slotname, addressing, filter,
             QFormatStr("%1 - %2")
                 .arg(s.minLOD == -FLT_MAX ? lit("0") : QString::number(s.minLOD))
                 .arg(s.maxLOD == FLT_MAX ? lit("FLT_MAX") : QString::number(s.maxLOD)),
             s.mipLODBias});
      }
    }
  }

  if(shaderDetails)
  {
    uint32_t i = 0;
    for(const ConstantBlock &shaderCBuf : shaderDetails->constantBlocks)
    {
      int bindPoint = mapping.constantBlocks[i].bind;

      const GLPipe::Buffer *b = NULL;

      if(bindPoint >= 0 && bindPoint < pipe.uniformBuffers.count())
        b = &pipe.uniformBuffers[bindPoint];

      bool filledSlot = !shaderCBuf.bufferBacked || (b && b->resourceId != ResourceId());
      bool usedSlot = mapping.constantBlocks[i].used;

      // show if
      {
        uint64_t offset = 0;
        uint64_t length = 0;
        int numvars = shaderCBuf.variables.count();
        uint64_t byteSize = shaderCBuf.byteSize;

        QString slotname = tr("Uniforms");
        QString name = tr("Empty");
        QString sizestr = tr("%1 Variables").arg(numvars);
        QString byterange;

        if(!filledSlot)
          length = 0;

        if(b)
        {
          slotname = QFormatStr("%1: %2").arg(bindPoint).arg(shaderCBuf.name);
          offset = b->byteOffset;
          length = b->byteSize;

          name = m_Ctx.GetResourceName(b->resourceId);

          BufferDescription *buf = m_Ctx.GetBuffer(b->resourceId);
          if(buf && length == 0)
            length = buf->length;

          if(length == byteSize)
            sizestr = tr("%1 Variables, %2 bytes").arg(numvars).arg(length);
          else
            sizestr =
                tr("%1 Variables, %2 bytes needed, %3 provided").arg(numvars).arg(byteSize).arg(length);

          byterange = QFormatStr("%1 - %2").arg(offset).arg(offset + length);
        }

        cbufferRows.push_back({slotname, name, byterange, sizestr});
      }
      i++;
    }
  }

  {
    uint32_t i = 0;
    for(uint32_t subval : sh.subroutines)
    {
      subRows.push_back({i, subval});

      i++;
    }
  }

  if(shaderDetails)
  {
    uint32_t i = 0;
    for(const ShaderResource &res : shaderDetails->readWriteResources)
    {
      int bindPoint = mapping.readWriteResources[i].bind;

      GLReadWriteType readWriteType = GetGLReadWriteType(res);

      const GLPipe::Buffer *bf = NULL;
      const GLPipe::ImageLoadStore *im = NULL;
      ResourceId id;

      if(readWriteType == GLReadWriteType::Image && bindPoint >= 0 && bindPoint < pipe.images.count())
      {
        im = &pipe.images[bindPoint];
        id = pipe.images[bindPoint].resourceId;
      }

      if(readWriteType == GLReadWriteType::Atomic && bindPoint >= 0 &&
         bindPoint < pipe.atomicBuffers.count())
      {
        bf = &pipe.atomicBuffers[bindPoint];
        id = pipe.atomicBuffers[bindPoint].resourceId;
      }

      if(readWriteType == GLReadWriteType::SSBO && bindPoint >= 0 &&
         bindPoint < pipe.shaderStorageBuffers.count())
      {
        bf = &pipe.shaderStorageBuffers[bindPoint];
        id = pipe.shaderStorageBuffers[bindPoint].resourceId;
      }

      bool filledSlot = id != ResourceId();
      bool usedSlot = mapping.readWriteResources[i].used;

      // show if
      {
        QString binding =
            readWriteType == GLReadWriteType::Image
                ? tr("Image")
                : readWriteType == GLReadWriteType::Atomic
                      ? tr("Atomic")
                      : readWriteType == GLReadWriteType::SSBO ? tr("SSBO") : tr("Unknown");

        QString slotname = QFormatStr("%1: %2").arg(bindPoint).arg(res.name);
        QString name = m_Ctx.GetResourceName(id);
        QString dimensions;
        QString format = lit("-");
        QString access = tr("Read/Write");
        if(im)
        {
          if(im->readAllowed && !im->writeAllowed)
            access = tr("Read-Only");
          if(!im->readAllowed && im->writeAllowed)
            access = tr("Write-Only");
          format = im->imageFormat.Name();
        }

        // check to see if it's a texture
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
              dimensions =
                  QFormatStr("%1x%2[%3]").arg(tex->width).arg(tex->height).arg(tex->arraysize);
            else
              dimensions = QFormatStr("%1x%2").arg(tex->width).arg(tex->height);
          }
          else if(tex->dimension == 3)
          {
            dimensions = QFormatStr("%1x%2x%3").arg(tex->width).arg(tex->height).arg(tex->depth);
          }
        }

        // if not a texture, it must be a buffer
        BufferDescription *buf = m_Ctx.GetBuffer(id);
        if(buf)
        {
          uint64_t offset = 0;
          uint64_t length = buf->length;
          if(bf && bf->byteSize > 0)
          {
            offset = bf->byteOffset;
            length = bf->byteSize;
          }

          if(offset > 0)
            dimensions = tr("%1 bytes at offset %2 bytes").arg(length).arg(offset);
          else
            dimensions = tr("%1 bytes").arg(length);
        }

        if(!filledSlot)
        {
          name = tr("Empty");
          dimensions = tr("-");
          access = tr("-");
        }

        readwriteRows.push_back({binding, slotname, name, dimensions, format, access});
      }
      i++;
    }
  }

  {
    xml.writeStartElement(tr("h3"));
    xml.writeCharacters(tr("Textures"));
    xml.writeEndElement();

    m_Common.exportHTMLTable(
        xml, {tr("Slot"), tr("Name"), tr("Type"), tr("Width"), tr("Height"), tr("Depth"),
              tr("Array Size"), tr("Format"), tr("First Mip"), tr("Num Mips")},
        textureRows);
  }

  {
    xml.writeStartElement(tr("h3"));
    xml.writeCharacters(tr("Samplers"));
    xml.writeEndElement();

    m_Common.exportHTMLTable(
        xml, {tr("Slot"), tr("Addressing"), tr("Filtering"), tr("LOD Clamping"), tr("LOD Bias")},
        samplerRows);
  }

  {
    xml.writeStartElement(tr("h3"));
    xml.writeCharacters(tr("Uniform Buffers"));
    xml.writeEndElement();

    m_Common.exportHTMLTable(xml, {tr("Slot"), tr("Name"), tr("Byte Range"), tr("Size")},
                             cbufferRows);
  }

  {
    xml.writeStartElement(tr("h3"));
    xml.writeCharacters(tr("Subroutines"));
    xml.writeEndElement();

    m_Common.exportHTMLTable(xml, {tr("Index"), tr("Value")}, subRows);
  }

  {
    xml.writeStartElement(tr("h3"));
    xml.writeCharacters(tr("Read-write resources"));
    xml.writeEndElement();

    m_Common.exportHTMLTable(
        xml,
        {
            tr("Binding"), tr("Resource"), tr("Name"), tr("Dimensions"), tr("Format"), tr("Access"),
        },
        readwriteRows);
  }
}

void GLPipelineStateViewer::exportHTML(QXmlStreamWriter &xml, const GLPipe::Feedback &xfb)
{
  const GLPipe::State &pipe = *m_Ctx.CurGLPipelineState();
  {
    xml.writeStartElement(tr("h3"));
    xml.writeCharacters(tr("States"));
    xml.writeEndElement();

    m_Common.exportHTMLTable(
        xml, {tr("Active"), tr("Paused")},
        {xfb.active ? tr("Yes") : tr("No"), xfb.paused ? tr("Yes") : tr("No")});
  }

  {
    xml.writeStartElement(tr("h3"));
    xml.writeCharacters(tr("Transform Feedback Targets"));
    xml.writeEndElement();

    QList<QVariantList> rows;

    for(size_t i = 0; i < ARRAY_COUNT(xfb.bufferResourceId); i++)
    {
      QString name = m_Ctx.GetResourceName(xfb.bufferResourceId[i]);
      uint64_t length = 0;

      if(xfb.bufferResourceId[i] == ResourceId())
      {
        name = tr("Empty");
      }
      else
      {
        BufferDescription *buf = m_Ctx.GetBuffer(xfb.bufferResourceId[i]);
        if(buf)
          length = buf->length;
      }

      rows.push_back({(int)i, name, (qulonglong)xfb.byteOffset[i], (qulonglong)xfb.byteSize[i],
                      (qulonglong)length});
    }

    m_Common.exportHTMLTable(
        xml, {tr("Slot"), tr("Buffer"), tr("Offset"), tr("Binding size"), tr("Buffer byte Length")},
        rows);
  }
}

void GLPipelineStateViewer::exportHTML(QXmlStreamWriter &xml, const GLPipe::Rasterizer &rs)
{
  const GLPipe::State &pipe = *m_Ctx.CurGLPipelineState();
  xml.writeStartElement(tr("h3"));
  xml.writeCharacters(tr("Rasterizer"));
  xml.writeEndElement();

  {
    xml.writeStartElement(tr("h3"));
    xml.writeCharacters(tr("States"));
    xml.writeEndElement();

    m_Common.exportHTMLTable(xml, {tr("Fill Mode"), tr("Cull Mode"), tr("Front CCW")},
                             {ToQStr(rs.state.fillMode), ToQStr(rs.state.cullMode),
                              rs.state.frontCCW ? tr("Yes") : tr("No")});

    xml.writeStartElement(tr("p"));
    xml.writeEndElement();

    m_Common.exportHTMLTable(
        xml, {tr("Multisample Enable"), tr("Sample Shading"), tr("Sample Mask"),
              tr("Sample Coverage"), tr("Sample Coverage Invert"), tr("Alpha to Coverage"),
              tr("Alpha to One"), tr("Min Sample Shading Rate")},
        {
            rs.state.multisampleEnable ? tr("Yes") : tr("No"),
            rs.state.sampleShading ? tr("Yes") : tr("No"),
            rs.state.sampleMask ? Formatter::Format(rs.state.sampleMaskValue, true) : tr("No"),
            rs.state.sampleCoverage ? QString::number(rs.state.sampleCoverageValue) : tr("No"),
            rs.state.sampleCoverageInvert ? tr("Yes") : tr("No"),
            rs.state.alphaToCoverage ? tr("Yes") : tr("No"),
            rs.state.alphaToOne ? tr("Yes") : tr("No"),
            Formatter::Format(rs.state.minSampleShadingRate),
        });

    xml.writeStartElement(tr("p"));
    xml.writeEndElement();

    m_Common.exportHTMLTable(
        xml,
        {
            tr("Programmable Point Size"), tr("Fixed Point Size"), tr("Line Width"),
            tr("Point Fade Threshold"), tr("Point Origin Upper Left"),
        },
        {
            rs.state.programmablePointSize ? tr("Yes") : tr("No"),
            Formatter::Format(rs.state.pointSize), Formatter::Format(rs.state.lineWidth),
            Formatter::Format(rs.state.pointFadeThreshold),
            rs.state.pointOriginUpperLeft ? tr("Yes") : tr("No"),
        });

    xml.writeStartElement(tr("p"));
    xml.writeEndElement();

    m_Common.exportHTMLTable(
        xml, {tr("Depth Clamp"), tr("Depth Bias"), tr("Offset Clamp"), tr("Slope Scaled Bias")},
        {rs.state.depthClamp ? tr("Yes") : tr("No"), rs.state.depthBias,
         Formatter::Format(rs.state.offsetClamp), Formatter::Format(rs.state.slopeScaledDepthBias)});
  }

  {
    xml.writeStartElement(tr("h3"));
    xml.writeCharacters(tr("Hints"));
    xml.writeEndElement();

    m_Common.exportHTMLTable(
        xml,
        {
            tr("Derivatives"), tr("Line Smooth"), tr("Poly Smooth"), tr("Tex Compression"),
        },
        {
            ToQStr(pipe.hints.derivatives),
            pipe.hints.lineSmoothingEnabled ? ToQStr(pipe.hints.lineSmoothing) : tr("Disabled"),
            pipe.hints.polySmoothingEnabled ? ToQStr(pipe.hints.polySmoothing) : tr("Disabled"),
            ToQStr(pipe.hints.textureCompression),
        });
  }

  {
    xml.writeStartElement(tr("h3"));
    xml.writeCharacters(tr("Viewports"));
    xml.writeEndElement();

    QList<QVariantList> rows;

    int i = 0;
    for(const Viewport &v : rs.viewports)
    {
      rows.push_back({i, v.x, v.y, v.width, v.height, v.minDepth, v.maxDepth});

      i++;
    }

    m_Common.exportHTMLTable(xml, {tr("Slot"), tr("X"), tr("Y"), tr("Width"), tr("Height"),
                                   tr("Min Depth"), tr("Max Depth")},
                             rows);
  }

  {
    xml.writeStartElement(tr("h3"));
    xml.writeCharacters(tr("Scissors"));
    xml.writeEndElement();

    QList<QVariantList> rows;

    int i = 0;
    for(const Scissor &s : rs.scissors)
    {
      rows.push_back({i, (bool)s.enabled, s.x, s.y, s.width, s.height});

      i++;
    }

    m_Common.exportHTMLTable(
        xml, {tr("Slot"), tr("Enabled"), tr("X"), tr("Y"), tr("Width"), tr("Height")}, rows);
  }
}

void GLPipelineStateViewer::exportHTML(QXmlStreamWriter &xml, const GLPipe::FrameBuffer &fb)
{
  const GLPipe::State &pipe = *m_Ctx.CurGLPipelineState();
  {
    xml.writeStartElement(tr("h3"));
    xml.writeCharacters(tr("Blend State"));
    xml.writeEndElement();

    QString blendFactor = QFormatStr("%1, %2, %3, %4")
                              .arg(fb.blendState.blendFactor[0], 0, 'f', 2)
                              .arg(fb.blendState.blendFactor[1], 0, 'f', 2)
                              .arg(fb.blendState.blendFactor[2], 0, 'f', 2)
                              .arg(fb.blendState.blendFactor[3], 0, 'f', 2);

    m_Common.exportHTMLTable(xml, {tr("Framebuffer SRGB"), tr("Blend Factor")},
                             {
                                 fb.framebufferSRGB ? tr("Yes") : tr("No"), blendFactor,
                             });

    xml.writeStartElement(tr("h3"));
    xml.writeCharacters(tr("Target Blends"));
    xml.writeEndElement();

    QList<QVariantList> rows;

    int i = 0;
    for(const ColorBlend &b : fb.blendState.blends)
    {
      if(i >= fb.drawFBO.colorAttachments.count())
        continue;

      rows.push_back({i, b.enabled ? tr("Yes") : tr("No"), ToQStr(b.colorBlend.source),
                      ToQStr(b.colorBlend.destination), ToQStr(b.colorBlend.operation),
                      ToQStr(b.alphaBlend.source), ToQStr(b.alphaBlend.destination),
                      ToQStr(b.alphaBlend.operation),
                      b.logicOperationEnabled ? tr("Yes") : tr("No"), ToQStr(b.logicOperation),
                      ((b.writeMask & 0x1) == 0 ? tr("_") : tr("R")) +
                          ((b.writeMask & 0x2) == 0 ? tr("_") : tr("G")) +
                          ((b.writeMask & 0x4) == 0 ? tr("_") : tr("B")) +
                          ((b.writeMask & 0x8) == 0 ? tr("_") : tr("A"))});

      i++;
    }

    m_Common.exportHTMLTable(
        xml,
        {
            tr("Slot"), tr("Blend Enable"), tr("Blend Source"), tr("Blend Destination"),
            tr("Blend Operation"), tr("Alpha Blend Source"), tr("Alpha Blend Destination"),
            tr("Alpha Blend Operation"), tr("Logic Operation Enabled"), tr("Logic Operation"),
            tr("Write Mask"),
        },
        rows);
  }

  {
    xml.writeStartElement(tr("h3"));
    xml.writeCharacters(tr("Depth State"));
    xml.writeEndElement();

    m_Common.exportHTMLTable(xml, {tr("Depth Test Enable"), tr("Depth Writes Enable"),
                                   tr("Depth Function"), tr("Depth Bounds")},
                             {
                                 pipe.depthState.depthEnable ? tr("Yes") : tr("No"),
                                 pipe.depthState.depthWrites ? tr("Yes") : tr("No"),
                                 ToQStr(pipe.depthState.depthFunction),
                                 pipe.depthState.depthEnable
                                     ? QFormatStr("%1 - %2")
                                           .arg(Formatter::Format(pipe.depthState.nearBound))
                                           .arg(Formatter::Format(pipe.depthState.farBound))
                                     : tr("Disabled"),
                             });
  }

  {
    xml.writeStartElement(tr("h3"));
    xml.writeCharacters(tr("Stencil State"));
    xml.writeEndElement();

    m_Common.exportHTMLTable(xml, {tr("Stencil Test Enable")},
                             {pipe.stencilState.stencilEnable ? tr("Yes") : tr("No")});

    xml.writeStartElement(tr("p"));
    xml.writeEndElement();

    m_Common.exportHTMLTable(
        xml, {tr("Face"), tr("Reference"), tr("Value Mask"), tr("Write Mask"), tr("Function"),
              tr("Pass Operation"), tr("Fail Operation"), tr("Depth Fail Operation")},
        {
            {tr("Front"), Formatter::Format(pipe.stencilState.frontFace.reference, true),
             Formatter::Format(pipe.stencilState.frontFace.compareMask, true),
             Formatter::Format(pipe.stencilState.frontFace.writeMask, true),
             ToQStr(pipe.stencilState.frontFace.function),
             ToQStr(pipe.stencilState.frontFace.passOperation),
             ToQStr(pipe.stencilState.frontFace.failOperation),
             ToQStr(pipe.stencilState.frontFace.depthFailOperation)},

            {tr("Back"), Formatter::Format(pipe.stencilState.backFace.reference, true),
             Formatter::Format(pipe.stencilState.backFace.compareMask, true),
             Formatter::Format(pipe.stencilState.backFace.writeMask, true),
             ToQStr(pipe.stencilState.backFace.function),
             ToQStr(pipe.stencilState.backFace.passOperation),
             ToQStr(pipe.stencilState.backFace.failOperation),
             ToQStr(pipe.stencilState.backFace.depthFailOperation)},
        });
  }

  {
    xml.writeStartElement(tr("h3"));
    xml.writeCharacters(tr("Draw FBO Attachments"));
    xml.writeEndElement();

    QList<QVariantList> rows;

    QList<const GLPipe::Attachment *> atts;
    for(const GLPipe::Attachment &att : fb.drawFBO.colorAttachments)
      atts.push_back(&att);
    atts.push_back(&fb.drawFBO.depthAttachment);
    atts.push_back(&fb.drawFBO.stencilAttachment);

    int i = 0;
    for(const GLPipe::Attachment *att : atts)
    {
      const GLPipe::Attachment &a = *att;

      TextureDescription *tex = m_Ctx.GetTexture(a.resourceId);

      QString name = m_Ctx.GetResourceName(a.resourceId);

      if(a.resourceId == ResourceId())
        name = tr("Empty");

      QString slotname = QString::number(i);

      if(i == atts.count() - 2)
        slotname = tr("Depth");
      else if(i == atts.count() - 1)
        slotname = tr("Stencil");

      rows.push_back({slotname, name, a.mipLevel, a.slice});

      i++;
    }

    m_Common.exportHTMLTable(xml,
                             {
                                 tr("Slot"), tr("Image"), tr("First mip"), tr("First array slice"),
                             },
                             rows);

    QList<QVariantList> drawbuffers;

    for(i = 0; i < fb.drawFBO.drawBuffers.count(); i++)
      drawbuffers.push_back({fb.drawFBO.drawBuffers[i]});

    xml.writeStartElement(tr("p"));
    xml.writeEndElement();

    m_Common.exportHTMLTable(xml,
                             {
                                 tr("Draw Buffers"),
                             },
                             drawbuffers);
  }

  {
    xml.writeStartElement(tr("h3"));
    xml.writeCharacters(tr("Read FBO Attachments"));
    xml.writeEndElement();

    QList<QVariantList> rows;

    QList<const GLPipe::Attachment *> atts;
    for(const GLPipe::Attachment &att : fb.readFBO.colorAttachments)
      atts.push_back(&att);
    atts.push_back(&fb.readFBO.depthAttachment);
    atts.push_back(&fb.readFBO.stencilAttachment);

    int i = 0;
    for(const GLPipe::Attachment *att : atts)
    {
      const GLPipe::Attachment &a = *att;

      TextureDescription *tex = m_Ctx.GetTexture(a.resourceId);

      QString name = m_Ctx.GetResourceName(a.resourceId);

      if(a.resourceId == ResourceId())
        name = tr("Empty");

      QString slotname = QString::number(i);

      if(i == atts.count() - 2)
        slotname = tr("Depth");
      else if(i == atts.count() - 1)
        slotname = tr("Stencil");

      rows.push_back({slotname, name, a.mipLevel, a.slice});

      i++;
    }

    m_Common.exportHTMLTable(xml,
                             {
                                 tr("Slot"), tr("Image"), tr("First mip"), tr("First array slice"),
                             },
                             rows);

    xml.writeStartElement(tr("p"));
    xml.writeEndElement();

    m_Common.exportHTMLTable(xml,
                             {
                                 tr("Read Buffer"),
                             },
                             {fb.readFBO.readBuffer});
  }
}

void GLPipelineStateViewer::on_exportHTML_clicked()
{
  QXmlStreamWriter *xmlptr = m_Common.beginHTMLExport();

  if(xmlptr)
  {
    QXmlStreamWriter &xml = *xmlptr;

    const QStringList &stageNames = ui->pipeFlow->stageNames();
    const QStringList &stageAbbrevs = ui->pipeFlow->stageAbbreviations();

    int stage = 0;
    for(const QString &sn : stageNames)
    {
      xml.writeStartElement(lit("div"));
      xml.writeStartElement(lit("a"));
      xml.writeAttribute(lit("name"), stageAbbrevs[stage]);
      xml.writeEndElement();
      xml.writeEndElement();

      xml.writeStartElement(lit("div"));
      xml.writeAttribute(lit("class"), lit("stage"));

      xml.writeStartElement(lit("h1"));
      xml.writeCharacters(sn);
      xml.writeEndElement();

      switch(stage)
      {
        case 0: exportHTML(xml, m_Ctx.CurGLPipelineState()->vertexInput); break;
        case 1: exportHTML(xml, m_Ctx.CurGLPipelineState()->vertexShader); break;
        case 2: exportHTML(xml, m_Ctx.CurGLPipelineState()->tessControlShader); break;
        case 3: exportHTML(xml, m_Ctx.CurGLPipelineState()->tessEvalShader); break;
        case 4:
          exportHTML(xml, m_Ctx.CurGLPipelineState()->geometryShader);
          exportHTML(xml, m_Ctx.CurGLPipelineState()->transformFeedback);
          break;
        case 5: exportHTML(xml, m_Ctx.CurGLPipelineState()->rasterizer); break;
        case 6: exportHTML(xml, m_Ctx.CurGLPipelineState()->fragmentShader); break;
        case 7: exportHTML(xml, m_Ctx.CurGLPipelineState()->framebuffer); break;
        case 8: exportHTML(xml, m_Ctx.CurGLPipelineState()->computeShader); break;
      }

      xml.writeEndElement();

      stage++;
    }

    m_Common.endHTMLExport(xmlptr);
  }
}

void GLPipelineStateViewer::on_meshView_clicked()
{
  if(!m_Ctx.HasMeshPreview())
    m_Ctx.ShowMeshPreview();
  ToolWindowManager::raiseToolWindow(m_Ctx.GetMeshPreview()->Widget());
}
