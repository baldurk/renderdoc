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
#include <QXmlStreamWriter>
#include "3rdparty/toolwindowmanager/ToolWindowManager.h"
#include "Code/Resources.h"
#include "Widgets/Extended/RDHeaderView.h"
#include "PipelineStateViewer.h"
#include "ui_GLPipelineStateViewer.h"

Q_DECLARE_METATYPE(ResourceId);

struct GLVBIBTag
{
  GLVBIBTag() { offset = 0; }
  GLVBIBTag(ResourceId i, uint64_t offs)
  {
    id = i;
    offset = offs;
  }

  ResourceId id;
  uint64_t offset;
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
  {
    QObject::connect(b, &RDLabel::clicked, this, &GLPipelineStateViewer::shaderLabel_clicked);
    b->setAutoFillBackground(true);
    b->setBackgroundRole(QPalette::ToolTipBase);
    b->setForegroundRole(QPalette::ToolTipText);
  }

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

    samp->setColumns({tr("Slot"), tr("Addressing"), tr("Filter"), tr("LOD Clamp"), tr("LOD Bias")});
    header->setColumnStretchHints({1, 2, 2, 2, 2});

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
          name = tr("Empty");
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

        tag = QVariant::fromValue(GLReadWriteTag(i, id, offset, length));
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

  const QPixmap &tick = Pixmaps::tick(this);
  const QPixmap &cross = Pixmaps::cross(this);

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

  m_Common.setTopologyDiagram(ui->topologyDiagram, topo);

  bool ibufferUsed = draw && (draw->flags & DrawFlags::UseIBuffer);

  if(ibufferUsed)
  {
    ui->primRestart->setVisible(true);
    if(state.m_VtxIn.primitiveRestart)
      ui->primRestart->setText(
          tr("Restart Idx: 0x%1").arg(Formatter::Format(state.m_VtxIn.restartIndex, true)));
    else
      ui->primRestart->setText(tr("Restart Idx: Disabled"));
  }
  else
  {
    ui->primRestart->setVisible(false);
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

      node->setTag(
          QVariant::fromValue(GLVBIBTag(state.m_VtxIn.ibuffer, draw ? draw->indexOffset : 0)));

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

      node->setTag(
          QVariant::fromValue(GLVBIBTag(state.m_VtxIn.ibuffer, draw ? draw->indexOffset : 0)));

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

      node->setTag(QVariant::fromValue(GLVBIBTag(v.Buffer, v.Offset)));

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
    ui->sampleMask->setText(Formatter::Format(state.m_Rasterizer.m_State.SampleMaskValue, true));
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
    ui->stencils->addTopLevelItem(
        new RDTreeWidgetItem({tr("Front"), ToQStr(state.m_StencilState.m_FrontFace.Func),
                              ToQStr(state.m_StencilState.m_FrontFace.FailOp),
                              ToQStr(state.m_StencilState.m_FrontFace.DepthFailOp),
                              ToQStr(state.m_StencilState.m_FrontFace.PassOp),
                              Formatter::Format(state.m_StencilState.m_FrontFace.WriteMask, true),
                              Formatter::Format(state.m_StencilState.m_FrontFace.ValueMask, true),
                              Formatter::Format(state.m_StencilState.m_FrontFace.Ref, true)}));

    ui->stencils->addTopLevelItem(
        new RDTreeWidgetItem({tr("Back"), ToQStr(state.m_StencilState.m_BackFace.Func),
                              ToQStr(state.m_StencilState.m_BackFace.FailOp),
                              ToQStr(state.m_StencilState.m_BackFace.DepthFailOp),
                              ToQStr(state.m_StencilState.m_BackFace.PassOp),
                              Formatter::Format(state.m_StencilState.m_BackFace.WriteMask, true),
                              Formatter::Format(state.m_StencilState.m_BackFace.ValueMask, true),
                              Formatter::Format(state.m_StencilState.m_BackFace.Ref, true)}));
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
  else if(tag.canConvert<GLReadWriteTag>())
  {
    GLReadWriteTag buf = tag.value<GLReadWriteTag>();

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

  if(tag.canConvert<GLVBIBTag>())
  {
    GLVBIBTag buf = tag.value<GLVBIBTag>();

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

  QColor col = QColor::fromHslF(float(idx) / 32.0f, 1.0f,
                                qBound(0.05, palette().color(QPalette::Base).lightnessF(), 0.95));

  ui->viAttrs->beginUpdate();
  ui->viBuffers->beginUpdate();

  if(slot < m_VBNodes.count())
  {
    m_VBNodes[slot]->setBackgroundColor(col);
    m_VBNodes[slot]->setForegroundColor(contrastingColor(col, QColor(0, 0, 0)));
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
      item->setForegroundColor(contrastingColor(col, QColor(0, 0, 0)));
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
    // this would only happen if the GL program is uploading SPIR-V instead of GLSL.
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

void GLPipelineStateViewer::exportHTML(QXmlStreamWriter &xml, GLPipe::VertexInput &vtx)
{
  const GLPipe::State &pipe = m_Ctx.CurGLPipelineState();
  {
    xml.writeStartElement(tr("h3"));
    xml.writeCharacters(tr("Vertex Attributes"));
    xml.writeEndElement();

    QList<QVariantList> rows;

    int i = 0;
    for(const GLPipe::VertexAttribute &a : vtx.attributes)
    {
      QString generic;
      if(!a.Enabled)
        generic = MakeGenericValueString(a.Format.compCount, a.Format.compType, a);
      rows.push_back(
          {i, (bool)a.Enabled, a.BufferSlot, ToQStr(a.Format.strname), a.RelativeOffset, generic});

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
    for(const GLPipe::VB &vb : vtx.vbuffers)
    {
      QString name = tr("Buffer %1").arg(ToQStr(vb.Buffer));
      uint64_t length = 0;

      if(vb.Buffer == ResourceId())
      {
        continue;
      }
      else
      {
        BufferDescription *buf = m_Ctx.GetBuffer(vb.Buffer);
        if(buf)
        {
          name = ToQStr(buf->name);
          length = buf->length;
        }
      }

      rows.push_back({i, name, vb.Stride, vb.Offset, vb.Divisor, (qulonglong)length});

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

    QString name = tr("Buffer %1").arg(ToQStr(vtx.ibuffer));
    uint64_t length = 0;

    if(vtx.ibuffer == ResourceId())
    {
      name = tr("Empty");
    }
    else
    {
      BufferDescription *buf = m_Ctx.GetBuffer(vtx.ibuffer);
      if(buf)
      {
        name = ToQStr(buf->name);
        length = buf->length;
      }
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
        {pipe.m_VtxProcess.discard ? tr("Yes") : tr("No"),
         pipe.m_VtxProcess.clipOriginLowerLeft ? tr("Yes") : tr("No"),
         pipe.m_VtxProcess.clipNegativeOneToOne ? tr("-1 to 1") : tr("0 to 1")});

    xml.writeStartElement(tr("p"));
    xml.writeEndElement();

    QList<QVariantList> clipPlaneRows;

    for(int i = 0; i < 8; i++)
      clipPlaneRows.push_back({i, pipe.m_VtxProcess.clipPlanes[i] ? tr("Yes") : tr("No")});

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
                .arg(pipe.m_VtxProcess.defaultInnerLevel[0])
                .arg(pipe.m_VtxProcess.defaultInnerLevel[1]),

            QFormatStr("%1, %2, %3, %4")
                .arg(pipe.m_VtxProcess.defaultOuterLevel[0])
                .arg(pipe.m_VtxProcess.defaultOuterLevel[1])
                .arg(pipe.m_VtxProcess.defaultOuterLevel[2])
                .arg(pipe.m_VtxProcess.defaultOuterLevel[3]),
        });
  }
}

void GLPipelineStateViewer::exportHTML(QXmlStreamWriter &xml, GLPipe::Shader &sh)
{
  const GLPipe::State &pipe = m_Ctx.CurGLPipelineState();
  ShaderReflection *shaderDetails = sh.ShaderDetails;
  const ShaderBindpointMapping &mapping = sh.BindpointMapping;

  {
    xml.writeStartElement(tr("h3"));
    xml.writeCharacters(tr("Shader"));
    xml.writeEndElement();

    QString shadername = tr("Unknown");

    if(sh.Object == ResourceId())
      shadername = tr("Unbound");
    else
      shadername = ToQStr(sh.ShaderName);

    if(sh.Object == ResourceId())
    {
      shadername = tr("Unbound");
    }
    else
    {
      QString shname = tr("%1 Shader").arg(ToQStr(sh.stage, GraphicsAPI::OpenGL));

      if(!sh.customShaderName && !sh.customProgramName && !sh.customPipelineName)
      {
        shadername = QFormatStr("%1 %2").arg(shname).arg(ToQStr(sh.Object));
      }
      else
      {
        if(sh.customShaderName)
          shname = ToQStr(sh.ShaderName);

        if(sh.customProgramName)
          shname = QFormatStr("%1 - %2").arg(ToQStr(sh.ProgramName)).arg(shname);

        if(sh.customPipelineName && sh.PipelineActive)
          shname = QFormatStr("%1 - %2").arg(ToQStr(sh.PipelineName)).arg(shname);

        shadername = shname;
      }
    }

    xml.writeStartElement(tr("p"));
    xml.writeCharacters(shadername);
    xml.writeEndElement();

    if(sh.Object == ResourceId())
      return;
  }

  QList<QVariantList> textureRows;
  QList<QVariantList> samplerRows;
  QList<QVariantList> cbufferRows;
  QList<QVariantList> readwriteRows;
  QList<QVariantList> subRows;

  for(int i = 0; i < pipe.Textures.count; i++)
  {
    const GLPipe::Texture &r = pipe.Textures[i];
    const GLPipe::Sampler &s = pipe.Samplers[i];

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
    bool usedSlot = (shaderInput && map->used);

    if(shaderInput)
    {
      // do texture
      {
        QString slotname = QString::number(i);

        if(shaderInput && shaderInput->name.count > 0)
          slotname += QFormatStr(": %1").arg(ToQStr(shaderInput->name));

        uint32_t w = 1, h = 1, d = 1;
        uint32_t a = 1;
        QString format = tr("Unknown");
        QString name = tr("Shader Resource %1").arg(ToQStr(r.Resource));
        QString typeName = tr("Unknown");

        if(!filledSlot)
        {
          name = tr("Empty");
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
              format += tr(" Depth-Repipead");
            else if(r.DepthReadChannel == 1)
              format += tr(" Stencil-Read");
          }
          else if(r.Swizzle[0] != TextureSwizzle::Red || r.Swizzle[1] != TextureSwizzle::Green ||
                  r.Swizzle[2] != TextureSwizzle::Blue || r.Swizzle[3] != TextureSwizzle::Alpha)
          {
            format += QFormatStr(" swizzle[%1%2%3%4]")
                          .arg(ToQStr(r.Swizzle[0]))
                          .arg(ToQStr(r.Swizzle[1]))
                          .arg(ToQStr(r.Swizzle[2]))
                          .arg(ToQStr(r.Swizzle[3]));
          }
        }

        textureRows.push_back({slotname, name, typeName, w, h, d, a, format});
      }

      // do sampler
      {
        QString slotname = QString::number(i);

        if(shaderInput && shaderInput->name.count > 0)
          slotname += QFormatStr(": %1").arg(ToQStr(shaderInput->name));

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
          filter += tr(" Aniso%1x").arg(s.MaxAniso);

        if(s.Filter.func == FilterFunc::Comparison)
          filter += QFormatStr(" %1").arg(ToQStr(s.Comparison));
        else if(s.Filter.func != FilterFunc::Normal)
          filter += QFormatStr(" (%1)").arg(ToQStr(s.Filter.func));

        samplerRows.push_back(
            {slotname, addressing, filter,
             QFormatStr("%1 - %2")
                 .arg(s.MinLOD == -FLT_MAX ? lit("0") : QString::number(s.MinLOD))
                 .arg(s.MaxLOD == FLT_MAX ? lit("FLT_MAX") : QString::number(s.MaxLOD)),
             s.MipLODBias});
      }
    }
  }

  if(shaderDetails)
  {
    uint32_t i = 0;
    for(const ConstantBlock &shaderCBuf : shaderDetails->ConstantBlocks)
    {
      int bindPoint = mapping.ConstantBlocks[i].bind;

      const GLPipe::Buffer *b = NULL;

      if(bindPoint >= 0 && bindPoint < pipe.UniformBuffers.count)
        b = &pipe.UniformBuffers[bindPoint];

      bool filledSlot = !shaderCBuf.bufferBacked || (b && b->Resource != ResourceId());
      bool usedSlot = mapping.ConstantBlocks[i].used;

      // show if
      {
        uint64_t offset = 0;
        uint64_t length = 0;
        int numvars = shaderCBuf.variables.count;
        uint64_t byteSize = shaderCBuf.byteSize;

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
          name = tr("UBO %1").arg(ToQStr(b->Resource));
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

          byterange = QFormatStr("%1 - %2").arg(offset).arg(offset + length);
        }

        cbufferRows.push_back({slotname, name, byterange, sizestr});
      }
      i++;
    }
  }

  {
    uint32_t i = 0;
    for(uint32_t subval : sh.Subroutines)
    {
      subRows.push_back({i, subval});

      i++;
    }
  }

  if(shaderDetails)
  {
    uint32_t i = 0;
    for(const ShaderResource &res : shaderDetails->ReadWriteResources)
    {
      int bindPoint = mapping.ReadWriteResources[i].bind;

      GLReadWriteType readWriteType = GetGLReadWriteType(res);

      const GLPipe::Buffer *bf = NULL;
      const GLPipe::ImageLoadStore *im = NULL;
      ResourceId id;

      if(readWriteType == GLReadWriteType::Image && bindPoint >= 0 && bindPoint < pipe.Images.count)
      {
        im = &pipe.Images[bindPoint];
        id = pipe.Images[bindPoint].Resource;
      }

      if(readWriteType == GLReadWriteType::Atomic && bindPoint >= 0 &&
         bindPoint < pipe.AtomicBuffers.count)
      {
        bf = &pipe.AtomicBuffers[bindPoint];
        id = pipe.AtomicBuffers[bindPoint].Resource;
      }

      if(readWriteType == GLReadWriteType::SSBO && bindPoint >= 0 &&
         bindPoint < pipe.ShaderStorageBuffers.count)
      {
        bf = &pipe.ShaderStorageBuffers[bindPoint];
        id = pipe.ShaderStorageBuffers[bindPoint].Resource;
      }

      bool filledSlot = id != ResourceId();
      bool usedSlot = mapping.ReadWriteResources[i].used;

      // show if
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

          name = ToQStr(tex->name);
        }

        // if not a texture, it must be a buffer
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

    m_Common.exportHTMLTable(xml, {tr("Slot"), tr("Name"), tr("Type"), tr("Width"), tr("Height"),
                                   tr("Depth"), tr("Array Size"), tr("Format")},
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

void GLPipelineStateViewer::exportHTML(QXmlStreamWriter &xml, GLPipe::Feedback &xfb)
{
  const GLPipe::State &pipe = m_Ctx.CurGLPipelineState();
  {
    xml.writeStartElement(tr("h3"));
    xml.writeCharacters(tr("States"));
    xml.writeEndElement();

    m_Common.exportHTMLTable(
        xml, {tr("Active"), tr("Paused")},
        {xfb.Active ? tr("Yes") : tr("No"), xfb.Paused ? tr("Yes") : tr("No")});
  }

  {
    xml.writeStartElement(tr("h3"));
    xml.writeCharacters(tr("Transform Feedback Targets"));
    xml.writeEndElement();

    QList<QVariantList> rows;

    for(size_t i = 0; i < ARRAY_COUNT(xfb.BufferBinding); i++)
    {
      QString name = tr("Buffer %1").arg(ToQStr(xfb.BufferBinding[i]));
      uint64_t length = 0;

      if(xfb.BufferBinding[i] == ResourceId())
      {
        name = tr("Empty");
      }
      else
      {
        BufferDescription *buf = m_Ctx.GetBuffer(xfb.BufferBinding[i]);
        if(buf)
        {
          name = ToQStr(buf->name);
          length = buf->length;
        }
      }

      rows.push_back(
          {(int)i, name, (qulonglong)xfb.Offset[i], (qulonglong)xfb.Size[i], (qulonglong)length});
    }

    m_Common.exportHTMLTable(
        xml, {tr("Slot"), tr("Buffer"), tr("Offset"), tr("Binding size"), tr("Buffer byte Length")},
        rows);
  }
}

void GLPipelineStateViewer::exportHTML(QXmlStreamWriter &xml, GLPipe::Rasterizer &rs)
{
  const GLPipe::State &pipe = m_Ctx.CurGLPipelineState();
  xml.writeStartElement(tr("h3"));
  xml.writeCharacters(tr("Rasterizer"));
  xml.writeEndElement();

  {
    xml.writeStartElement(tr("h3"));
    xml.writeCharacters(tr("States"));
    xml.writeEndElement();

    m_Common.exportHTMLTable(xml, {tr("Fill Mode"), tr("Cull Mode"), tr("Front CCW")},
                             {ToQStr(rs.m_State.fillMode), ToQStr(rs.m_State.cullMode),
                              rs.m_State.FrontCCW ? tr("Yes") : tr("No")});

    xml.writeStartElement(tr("p"));
    xml.writeEndElement();

    m_Common.exportHTMLTable(
        xml, {tr("Multisample Enable"), tr("Sample Shading"), tr("Sample Mask"),
              tr("Sample Coverage"), tr("Sample Coverage Invert"), tr("Alpha to Coverage"),
              tr("Alpha to One"), tr("Min Sample Shading Rate")},
        {
            rs.m_State.MultisampleEnable ? tr("Yes") : tr("No"),
            rs.m_State.SampleShading ? tr("Yes") : tr("No"),
            rs.m_State.SampleMask ? Formatter::Format(rs.m_State.SampleMaskValue, true) : tr("No"),
            rs.m_State.SampleCoverage ? QString::number(rs.m_State.SampleCoverageValue) : tr("No"),
            rs.m_State.SampleCoverageInvert ? tr("Yes") : tr("No"),
            rs.m_State.SampleAlphaToCoverage ? tr("Yes") : tr("No"),
            rs.m_State.SampleAlphaToOne ? tr("Yes") : tr("No"),
            Formatter::Format(rs.m_State.MinSampleShadingRate),
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
            rs.m_State.ProgrammablePointSize ? tr("Yes") : tr("No"),
            Formatter::Format(rs.m_State.PointSize), Formatter::Format(rs.m_State.LineWidth),
            Formatter::Format(rs.m_State.PointFadeThreshold),
            rs.m_State.PointOriginUpperLeft ? tr("Yes") : tr("No"),
        });

    xml.writeStartElement(tr("p"));
    xml.writeEndElement();

    m_Common.exportHTMLTable(
        xml, {tr("Depth Clamp"), tr("Depth Bias"), tr("Offset Clamp"), tr("Slope Scaled Bias")},
        {rs.m_State.DepthClamp ? tr("Yes") : tr("No"), rs.m_State.DepthBias,
         Formatter::Format(rs.m_State.OffsetClamp),
         Formatter::Format(rs.m_State.SlopeScaledDepthBias)});
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
            ToQStr(pipe.m_Hints.Derivatives),
            pipe.m_Hints.LineSmoothEnabled ? ToQStr(pipe.m_Hints.LineSmooth) : tr("Disabled"),
            pipe.m_Hints.PolySmoothEnabled ? ToQStr(pipe.m_Hints.PolySmooth) : tr("Disabled"),
            ToQStr(pipe.m_Hints.TexCompression),
        });
  }

  {
    xml.writeStartElement(tr("h3"));
    xml.writeCharacters(tr("Viewports"));
    xml.writeEndElement();

    QList<QVariantList> rows;

    int i = 0;
    for(const GLPipe::Viewport &v : rs.Viewports)
    {
      rows.push_back({i, v.Left, v.Bottom, v.Width, v.Height, v.MinDepth, v.MaxDepth});

      i++;
    }

    m_Common.exportHTMLTable(xml, {tr("Slot"), tr("Left"), tr("Bottom"), tr("Width"), tr("Height"),
                                   tr("Min Depth"), tr("Max Depth")},
                             rows);
  }

  {
    xml.writeStartElement(tr("h3"));
    xml.writeCharacters(tr("Scissors"));
    xml.writeEndElement();

    QList<QVariantList> rows;

    int i = 0;
    for(const GLPipe::Scissor &s : rs.Scissors)
    {
      rows.push_back({i, (bool)s.Enabled, s.Left, s.Bottom, s.Width, s.Height});

      i++;
    }

    m_Common.exportHTMLTable(
        xml, {tr("Slot"), tr("Enabled"), tr("Left"), tr("Bottom"), tr("Width"), tr("Height")}, rows);
  }
}

void GLPipelineStateViewer::exportHTML(QXmlStreamWriter &xml, GLPipe::FrameBuffer &fb)
{
  const GLPipe::State &pipe = m_Ctx.CurGLPipelineState();
  {
    xml.writeStartElement(tr("h3"));
    xml.writeCharacters(tr("Blend State"));
    xml.writeEndElement();

    QString blendFactor = QFormatStr("%1, %2, %3, %4")
                              .arg(fb.m_Blending.BlendFactor[0], 0, 'f', 2)
                              .arg(fb.m_Blending.BlendFactor[1], 0, 'f', 2)
                              .arg(fb.m_Blending.BlendFactor[2], 0, 'f', 2)
                              .arg(fb.m_Blending.BlendFactor[3], 0, 'f', 2);

    m_Common.exportHTMLTable(xml, {tr("Framebuffer SRGB"), tr("Blend Factor")},
                             {
                                 fb.FramebufferSRGB ? tr("Yes") : tr("No"), blendFactor,
                             });

    xml.writeStartElement(tr("h3"));
    xml.writeCharacters(tr("Target Blends"));
    xml.writeEndElement();

    QList<QVariantList> rows;

    int i = 0;
    for(const GLPipe::Blend &b : fb.m_Blending.Blends)
    {
      if(i >= fb.m_DrawFBO.Color.count)
        continue;

      rows.push_back({i, b.Enabled ? tr("Yes") : tr("No"), ToQStr(b.m_Blend.Source),
                      ToQStr(b.m_Blend.Destination), ToQStr(b.m_Blend.Operation),
                      ToQStr(b.m_AlphaBlend.Source), ToQStr(b.m_AlphaBlend.Destination),
                      ToQStr(b.m_AlphaBlend.Operation), ToQStr(b.Logic),
                      ((b.WriteMask & 0x1) == 0 ? tr("_") : tr("R")) +
                          ((b.WriteMask & 0x2) == 0 ? tr("_") : tr("G")) +
                          ((b.WriteMask & 0x4) == 0 ? tr("_") : tr("B")) +
                          ((b.WriteMask & 0x8) == 0 ? tr("_") : tr("A"))});

      i++;
    }

    m_Common.exportHTMLTable(
        xml,
        {
            tr("Slot"), tr("Blend Enable"), tr("Blend Source"), tr("Blend Destination"),
            tr("Blend Operation"), tr("Alpha Blend Source"), tr("Alpha Blend Destination"),
            tr("Alpha Blend Operation"), tr("Logic Operation"), tr("Write Mask"),
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
                                 pipe.m_DepthState.DepthEnable ? tr("Yes") : tr("No"),
                                 pipe.m_DepthState.DepthWrites ? tr("Yes") : tr("No"),
                                 ToQStr(pipe.m_DepthState.DepthFunc),
                                 pipe.m_DepthState.DepthEnable
                                     ? QFormatStr("%1 - %2")
                                           .arg(Formatter::Format(pipe.m_DepthState.NearBound))
                                           .arg(Formatter::Format(pipe.m_DepthState.FarBound))
                                     : tr("Disabled"),
                             });
  }

  {
    xml.writeStartElement(tr("h3"));
    xml.writeCharacters(tr("Stencil State"));
    xml.writeEndElement();

    m_Common.exportHTMLTable(xml, {tr("Stencil Test Enable")},
                             {pipe.m_StencilState.StencilEnable ? tr("Yes") : tr("No")});

    xml.writeStartElement(tr("p"));
    xml.writeEndElement();

    m_Common.exportHTMLTable(
        xml, {tr("Face"), tr("Reference"), tr("Value Mask"), tr("Write Mask"), tr("Function"),
              tr("Pass Operation"), tr("Fail Operation"), tr("Depth Fail Operation")},
        {
            {tr("Front"), Formatter::Format(pipe.m_StencilState.m_FrontFace.Ref, true),
             Formatter::Format(pipe.m_StencilState.m_FrontFace.ValueMask, true),
             Formatter::Format(pipe.m_StencilState.m_FrontFace.WriteMask, true),
             ToQStr(pipe.m_StencilState.m_FrontFace.Func),
             ToQStr(pipe.m_StencilState.m_FrontFace.PassOp),
             ToQStr(pipe.m_StencilState.m_FrontFace.FailOp),
             ToQStr(pipe.m_StencilState.m_FrontFace.DepthFailOp)},

            {tr("Back"), Formatter::Format(pipe.m_StencilState.m_BackFace.Ref, true),
             Formatter::Format(pipe.m_StencilState.m_BackFace.ValueMask, true),
             Formatter::Format(pipe.m_StencilState.m_BackFace.WriteMask, true),
             ToQStr(pipe.m_StencilState.m_BackFace.Func),
             ToQStr(pipe.m_StencilState.m_BackFace.PassOp),
             ToQStr(pipe.m_StencilState.m_BackFace.FailOp),
             ToQStr(pipe.m_StencilState.m_BackFace.DepthFailOp)},
        });
  }

  {
    xml.writeStartElement(tr("h3"));
    xml.writeCharacters(tr("Draw FBO Attachments"));
    xml.writeEndElement();

    QList<QVariantList> rows;

    QList<const GLPipe::Attachment *> atts;
    for(const GLPipe::Attachment &att : fb.m_DrawFBO.Color)
      atts.push_back(&att);
    atts.push_back(&fb.m_DrawFBO.Depth);
    atts.push_back(&fb.m_DrawFBO.Stencil);

    int i = 0;
    for(const GLPipe::Attachment *att : atts)
    {
      const GLPipe::Attachment &a = *att;

      TextureDescription *tex = m_Ctx.GetTexture(a.Obj);

      QString name = tr("Image %1").arg(ToQStr(a.Obj));

      if(tex)
        name = ToQStr(tex->name);
      if(a.Obj == ResourceId())
        name = tr("Empty");

      QString slotname = QString::number(i);

      if(i == atts.count() - 2)
        slotname = tr("Depth");
      else if(i == atts.count() - 1)
        slotname = tr("Stencil");

      rows.push_back({slotname, name, a.Mip, a.Layer});

      i++;
    }

    m_Common.exportHTMLTable(xml,
                             {
                                 tr("Slot"), tr("Image"), tr("First mip"), tr("First array slice"),
                             },
                             rows);

    QList<QVariantList> drawbuffers;

    for(i = 0; i < fb.m_DrawFBO.DrawBuffers.count; i++)
      drawbuffers.push_back({fb.m_DrawFBO.DrawBuffers[i]});

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
    for(const GLPipe::Attachment &att : fb.m_ReadFBO.Color)
      atts.push_back(&att);
    atts.push_back(&fb.m_ReadFBO.Depth);
    atts.push_back(&fb.m_ReadFBO.Stencil);

    int i = 0;
    for(const GLPipe::Attachment *att : atts)
    {
      const GLPipe::Attachment &a = *att;

      TextureDescription *tex = m_Ctx.GetTexture(a.Obj);

      QString name = tr("Image %1").arg(ToQStr(a.Obj));

      if(tex)
        name = ToQStr(tex->name);
      if(a.Obj == ResourceId())
        name = tr("Empty");

      QString slotname = QString::number(i);

      if(i == atts.count() - 2)
        slotname = tr("Depth");
      else if(i == atts.count() - 1)
        slotname = tr("Stencil");

      rows.push_back({slotname, name, a.Mip, a.Layer});

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
                             {fb.m_ReadFBO.ReadBuffer});
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
        case 0: exportHTML(xml, m_Ctx.CurGLPipelineState().m_VtxIn); break;
        case 1: exportHTML(xml, m_Ctx.CurGLPipelineState().m_VS); break;
        case 2: exportHTML(xml, m_Ctx.CurGLPipelineState().m_TCS); break;
        case 3: exportHTML(xml, m_Ctx.CurGLPipelineState().m_TES); break;
        case 4:
          exportHTML(xml, m_Ctx.CurGLPipelineState().m_GS);
          exportHTML(xml, m_Ctx.CurGLPipelineState().m_Feedback);
          break;
        case 5: exportHTML(xml, m_Ctx.CurGLPipelineState().m_Rasterizer); break;
        case 6: exportHTML(xml, m_Ctx.CurGLPipelineState().m_FS); break;
        case 7: exportHTML(xml, m_Ctx.CurGLPipelineState().m_FB); break;
        case 8: exportHTML(xml, m_Ctx.CurGLPipelineState().m_CS); break;
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
