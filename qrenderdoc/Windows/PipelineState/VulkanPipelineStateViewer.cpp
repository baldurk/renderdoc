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

#include "VulkanPipelineStateViewer.h"
#include <float.h>
#include <QMouseEvent>
#include <QScrollBar>
#include <QXmlStreamWriter>
#include "3rdparty/toolwindowmanager/ToolWindowManager.h"
#include "Code/Resources.h"
#include "Widgets/Extended/RDHeaderView.h"
#include "PipelineStateViewer.h"
#include "ui_VulkanPipelineStateViewer.h"

Q_DECLARE_METATYPE(ResourceId);
Q_DECLARE_METATYPE(SamplerData);

struct VulkanVBIBTag
{
  VulkanVBIBTag() { offset = 0; }
  VulkanVBIBTag(ResourceId i, uint64_t offs)
  {
    id = i;
    offset = offs;
  }

  ResourceId id;
  uint64_t offset;
};

Q_DECLARE_METATYPE(VulkanVBIBTag);

struct VulkanCBufferTag
{
  VulkanCBufferTag() { slotIdx = arrayIdx = 0; }
  VulkanCBufferTag(uint32_t s, uint32_t i)
  {
    slotIdx = s;
    arrayIdx = i;
  }
  uint32_t slotIdx;
  uint32_t arrayIdx;
};

Q_DECLARE_METATYPE(VulkanCBufferTag);

struct VulkanBufferTag
{
  VulkanBufferTag()
  {
    rwRes = false;
    bindPoint = 0;
    offset = size = 0;
  }
  VulkanBufferTag(bool rw, uint32_t b, ResourceId id, uint64_t offs, uint64_t sz)
  {
    rwRes = rw;
    bindPoint = b;
    ID = id;
    offset = offs;
    size = sz;
  }
  bool rwRes;
  uint32_t bindPoint;
  ResourceId ID;
  uint64_t offset;
  uint64_t size;
};

Q_DECLARE_METATYPE(VulkanBufferTag);

VulkanPipelineStateViewer::VulkanPipelineStateViewer(ICaptureContext &ctx,
                                                     PipelineStateViewer &common, QWidget *parent)
    : QFrame(parent), ui(new Ui::VulkanPipelineStateViewer), m_Ctx(ctx), m_Common(common)
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

  RDTreeWidget *resources[] = {
      ui->vsResources, ui->tcsResources, ui->tesResources,
      ui->gsResources, ui->fsResources,  ui->csResources,
  };

  RDTreeWidget *ubos[] = {
      ui->vsUBOs, ui->tcsUBOs, ui->tesUBOs, ui->gsUBOs, ui->fsUBOs, ui->csUBOs,
  };

  for(QToolButton *b : viewButtons)
    QObject::connect(b, &QToolButton::clicked, this, &VulkanPipelineStateViewer::shaderView_clicked);

  for(RDLabel *b : shaderLabels)
  {
    QObject::connect(b, &RDLabel::clicked, this, &VulkanPipelineStateViewer::shaderLabel_clicked);
    b->setAutoFillBackground(true);
    b->setBackgroundRole(QPalette::ToolTipBase);
    b->setForegroundRole(QPalette::ToolTipText);
  }

  for(QToolButton *b : editButtons)
    QObject::connect(b, &QToolButton::clicked, this, &VulkanPipelineStateViewer::shaderEdit_clicked);

  for(QToolButton *b : saveButtons)
    QObject::connect(b, &QToolButton::clicked, this, &VulkanPipelineStateViewer::shaderSave_clicked);

  QObject::connect(ui->viAttrs, &RDTreeWidget::leave, this, &VulkanPipelineStateViewer::vertex_leave);
  QObject::connect(ui->viBuffers, &RDTreeWidget::leave, this,
                   &VulkanPipelineStateViewer::vertex_leave);

  QObject::connect(ui->framebuffer, &RDTreeWidget::itemActivated, this,
                   &VulkanPipelineStateViewer::resource_itemActivated);

  for(RDTreeWidget *res : resources)
    QObject::connect(res, &RDTreeWidget::itemActivated, this,
                     &VulkanPipelineStateViewer::resource_itemActivated);

  for(RDTreeWidget *ubo : ubos)
    QObject::connect(ubo, &RDTreeWidget::itemActivated, this,
                     &VulkanPipelineStateViewer::ubo_itemActivated);

  addGridLines(ui->rasterizerGridLayout, palette().color(QPalette::WindowText));
  addGridLines(ui->MSAAGridLayout, palette().color(QPalette::WindowText));
  addGridLines(ui->blendStateGridLayout, palette().color(QPalette::WindowText));
  addGridLines(ui->depthStateGridLayout, palette().color(QPalette::WindowText));

  {
    RDHeaderView *header = new RDHeaderView(Qt::Horizontal, this);
    ui->viAttrs->setHeader(header);

    ui->viAttrs->setColumns({tr("Index"), tr("Name"), tr("Location"), tr("Binding"), tr("Format"),
                             tr("Offset"), tr("Go")});
    header->setColumnStretchHints({1, 4, 1, 2, 3, 2, -1});

    ui->viAttrs->setHoverIconColumn(6, action, action_hover);
    ui->viAttrs->setClearSelectionOnFocusLoss(true);
    ui->viAttrs->setInstantTooltips(true);
  }

  {
    RDHeaderView *header = new RDHeaderView(Qt::Horizontal, this);
    ui->viBuffers->setHeader(header);

    ui->viBuffers->setColumns({tr("Slot"), tr("Buffer"), tr("Rate"), tr("Offset"), tr("Stride"),
                               tr("Byte Length"), tr("Go")});
    header->setColumnStretchHints({1, 4, 2, 2, 2, 3, -1});

    ui->viBuffers->setHoverIconColumn(6, action, action_hover);
    ui->viBuffers->setClearSelectionOnFocusLoss(true);
    ui->viBuffers->setInstantTooltips(true);
  }

  for(RDTreeWidget *res : resources)
  {
    RDHeaderView *header = new RDHeaderView(Qt::Horizontal, this);
    res->setHeader(header);

    res->setColumns({QString(), tr("Set"), tr("Binding"), tr("Type"), tr("Resource"),
                     tr("Contents"), tr("cont.d"), tr("Go")});
    header->setColumnStretchHints({-1, -1, 2, 2, 2, 4, 4, -1});

    res->setHoverIconColumn(7, action, action_hover);
    res->setClearSelectionOnFocusLoss(true);
    res->setInstantTooltips(true);
  }

  for(RDTreeWidget *ubo : ubos)
  {
    RDHeaderView *header = new RDHeaderView(Qt::Horizontal, this);
    ubo->setHeader(header);

    ubo->setColumns({QString(), tr("Set"), tr("Binding"), tr("Buffer"), tr("Byte Range"),
                     tr("Size"), tr("Go")});
    header->setColumnStretchHints({-1, -1, 2, 4, 3, 3, -1});

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

    ui->scissors->setColumns({tr("Slot"), tr("X"), tr("Y"), tr("Width"), tr("Height")});
    header->setColumnStretchHints({-1, -1, -1, -1, 1});
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
  ui->vsShader->setFont(Formatter::PreferredFont());
  ui->vsResources->setFont(Formatter::PreferredFont());
  ui->vsUBOs->setFont(Formatter::PreferredFont());
  ui->gsShader->setFont(Formatter::PreferredFont());
  ui->gsResources->setFont(Formatter::PreferredFont());
  ui->gsUBOs->setFont(Formatter::PreferredFont());
  ui->tcsShader->setFont(Formatter::PreferredFont());
  ui->tcsResources->setFont(Formatter::PreferredFont());
  ui->tcsUBOs->setFont(Formatter::PreferredFont());
  ui->tesShader->setFont(Formatter::PreferredFont());
  ui->tesResources->setFont(Formatter::PreferredFont());
  ui->tesUBOs->setFont(Formatter::PreferredFont());
  ui->fsShader->setFont(Formatter::PreferredFont());
  ui->fsResources->setFont(Formatter::PreferredFont());
  ui->fsUBOs->setFont(Formatter::PreferredFont());
  ui->csShader->setFont(Formatter::PreferredFont());
  ui->csResources->setFont(Formatter::PreferredFont());
  ui->csUBOs->setFont(Formatter::PreferredFont());
  ui->viewports->setFont(Formatter::PreferredFont());
  ui->scissors->setFont(Formatter::PreferredFont());
  ui->framebuffer->setFont(Formatter::PreferredFont());
  ui->blends->setFont(Formatter::PreferredFont());

  // reset everything back to defaults
  clearState();
}

VulkanPipelineStateViewer::~VulkanPipelineStateViewer()
{
  delete ui;
}

void VulkanPipelineStateViewer::OnLogfileLoaded()
{
  OnEventChanged(m_Ctx.CurEvent());
}

void VulkanPipelineStateViewer::OnLogfileClosed()
{
  ui->pipeFlow->setStagesEnabled({true, true, true, true, true, true, true, true, true});

  clearState();
}

void VulkanPipelineStateViewer::OnEventChanged(uint32_t eventID)
{
  setState();
}

void VulkanPipelineStateViewer::on_showDisabled_toggled(bool checked)
{
  setState();
}

void VulkanPipelineStateViewer::on_showEmpty_toggled(bool checked)
{
  setState();
}

void VulkanPipelineStateViewer::setInactiveRow(RDTreeWidgetItem *node)
{
  node->setItalic(true);
}

void VulkanPipelineStateViewer::setEmptyRow(RDTreeWidgetItem *node)
{
  node->setBackgroundColor(QColor(255, 70, 70));
  node->setForegroundColor(QColor(0, 0, 0));
}

template <typename bindType>
void VulkanPipelineStateViewer::setViewDetails(RDTreeWidgetItem *node, const bindType &view,
                                               TextureDescription *tex)
{
  if(tex == NULL)
    return;

  QString text;

  bool viewdetails = false;

  {
    for(const VKPipe::ImageData &im : m_Ctx.CurVulkanPipelineState().images)
    {
      if(im.image == tex->ID)
      {
        text += tr("Texture is in the '%1' layout\n\n").arg(ToQStr(im.layouts[0].name));
        break;
      }
    }

    if(view.viewfmt != tex->format)
    {
      text += tr("The texture is format %1, the view treats it as %2.\n")
                  .arg(ToQStr(tex->format.strname))
                  .arg(ToQStr(view.viewfmt.strname));

      viewdetails = true;
    }

    if(tex->mips > 1 && (tex->mips != view.numMip || view.baseMip > 0))
    {
      if(view.numMip == 1)
        text +=
            tr("The texture has %1 mips, the view covers mip %2.\n").arg(tex->mips).arg(view.baseMip);
      else
        text += tr("The texture has %1 mips, the view covers mips %2-%3.\n")
                    .arg(tex->mips)
                    .arg(view.baseMip)
                    .arg(view.baseMip + view.numMip - 1);

      viewdetails = true;
    }

    if(tex->arraysize > 1 && (tex->arraysize != view.numLayer || view.baseLayer > 0))
    {
      if(view.numLayer == 1)
        text += tr("The texture has %1 array slices, the view covers slice %2.\n")
                    .arg(tex->arraysize)
                    .arg(view.baseLayer);
      else
        text += tr("The texture has %1 array slices, the view covers slices %2-%3.\n")
                    .arg(tex->arraysize)
                    .arg(view.baseLayer)
                    .arg(view.baseLayer + view.numLayer);

      viewdetails = true;
    }
  }

  text = text.trimmed();

  node->setToolTip(text);

  if(viewdetails)
  {
    node->setBackgroundColor(QColor(127, 255, 212));
    node->setForegroundColor(QColor(0, 0, 0));
  }
}

template <typename bindType>
void VulkanPipelineStateViewer::setViewDetails(RDTreeWidgetItem *node, const bindType &view,
                                               BufferDescription *buf)
{
  if(buf == NULL)
    return;

  QString text;

  if(view.offset > 0 || view.size < buf->length)
  {
    text += tr("The view covers bytes %1-%2.\nThe buffer is %3 bytes in length.")
                .arg(view.offset)
                .arg(view.offset + view.size)
                .arg(buf->length);
  }
  else
  {
    return;
  }

  node->setToolTip(text);
  node->setBackgroundColor(QColor(127, 255, 212));
  node->setForegroundColor(QColor(0, 0, 0));
}

bool VulkanPipelineStateViewer::showNode(bool usedSlot, bool filledSlot)
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

const VKPipe::Shader *VulkanPipelineStateViewer::stageForSender(QWidget *widget)
{
  if(!m_Ctx.LogLoaded())
    return NULL;

  while(widget)
  {
    if(widget == ui->stagesTabs->widget(0))
      return &m_Ctx.CurVulkanPipelineState().m_VS;
    if(widget == ui->stagesTabs->widget(1))
      return &m_Ctx.CurVulkanPipelineState().m_VS;
    if(widget == ui->stagesTabs->widget(2))
      return &m_Ctx.CurVulkanPipelineState().m_TCS;
    if(widget == ui->stagesTabs->widget(3))
      return &m_Ctx.CurVulkanPipelineState().m_TES;
    if(widget == ui->stagesTabs->widget(4))
      return &m_Ctx.CurVulkanPipelineState().m_GS;
    if(widget == ui->stagesTabs->widget(5))
      return &m_Ctx.CurVulkanPipelineState().m_FS;
    if(widget == ui->stagesTabs->widget(6))
      return &m_Ctx.CurVulkanPipelineState().m_FS;
    if(widget == ui->stagesTabs->widget(7))
      return &m_Ctx.CurVulkanPipelineState().m_FS;
    if(widget == ui->stagesTabs->widget(8))
      return &m_Ctx.CurVulkanPipelineState().m_CS;

    widget = widget->parentWidget();
  }

  qCritical() << "Unrecognised control calling event handler";

  return NULL;
}

void VulkanPipelineStateViewer::clearShaderState(QLabel *shader, RDTreeWidget *resources,
                                                 RDTreeWidget *cbuffers)
{
  shader->setText(tr("Unbound Shader"));
  resources->clear();
  cbuffers->clear();
}

void VulkanPipelineStateViewer::clearState()
{
  m_VBNodes.clear();
  m_BindNodes.clear();

  ui->viAttrs->clear();
  ui->viBuffers->clear();
  ui->topology->setText(QString());
  ui->primRestart->setVisible(false);
  ui->topologyDiagram->setPixmap(QPixmap());

  clearShaderState(ui->vsShader, ui->vsResources, ui->vsUBOs);
  clearShaderState(ui->tcsShader, ui->tcsResources, ui->tcsUBOs);
  clearShaderState(ui->tesShader, ui->tesResources, ui->tesUBOs);
  clearShaderState(ui->gsShader, ui->gsResources, ui->gsUBOs);
  clearShaderState(ui->fsShader, ui->fsResources, ui->fsUBOs);
  clearShaderState(ui->csShader, ui->csResources, ui->csUBOs);

  const QPixmap &tick = Pixmaps::tick(this);

  ui->fillMode->setText(tr("Solid", "Fill Mode"));
  ui->cullMode->setText(tr("Front", "Cull Mode"));
  ui->frontCCW->setPixmap(tick);

  ui->depthBias->setText(lit("0.0"));
  ui->depthBiasClamp->setText(lit("0.0"));
  ui->slopeScaledBias->setText(lit("0.0"));

  ui->depthClamp->setPixmap(tick);
  ui->rasterizerDiscard->setPixmap(tick);
  ui->lineWidth->setText(lit("1.0"));

  ui->sampleCount->setText(lit("1"));
  ui->sampleShading->setPixmap(tick);
  ui->minSampleShading->setText(lit("0.0"));
  ui->sampleMask->setText(lit("FFFFFFFF"));

  ui->viewports->clear();
  ui->scissors->clear();

  ui->framebuffer->clear();
  ui->blends->clear();

  ui->blendFactor->setText(lit("0.00, 0.00, 0.00, 0.00"));
  ui->logicOp->setText(lit("-"));
  ui->alphaToOne->setPixmap(tick);

  ui->depthEnabled->setPixmap(tick);
  ui->depthFunc->setText(lit("GREATER_EQUAL"));
  ui->depthWrite->setPixmap(tick);

  ui->depthBounds->setText(lit("0.0-1.0"));
  ui->depthBounds->setPixmap(QPixmap());

  ui->stencils->clear();
}

QVariantList VulkanPipelineStateViewer::makeSampler(const QString &bindset, const QString &slotname,
                                                    const VKPipe::BindingElement &descriptor)
{
  QString addressing;
  QString addPrefix;
  QString addVal;

  QString filter;

  QString addr[] = {ToQStr(descriptor.AddressU), ToQStr(descriptor.AddressV),
                    ToQStr(descriptor.AddressW)};

  // arrange like either UVW: WRAP or UV: WRAP, W: CLAMP
  for(int a = 0; a < 3; a++)
  {
    QString prefix = QString(QLatin1Char("UVW"[a]));

    if(a == 0 || addr[a] == addr[a - 1])
    {
      addPrefix += prefix;
    }
    else
    {
      addressing += addPrefix + lit(": ") + addVal + lit(", ");

      addPrefix = prefix;
    }
    addVal = addr[a];
  }

  addressing += addPrefix + lit(": ") + addVal;

  if(descriptor.UseBorder())
    addressing += QFormatStr(" <%1, %2, %3, %4>")
                      .arg(descriptor.BorderColor[0])
                      .arg(descriptor.BorderColor[1])
                      .arg(descriptor.BorderColor[2])
                      .arg(descriptor.BorderColor[3]);

  if(descriptor.unnormalized)
    addressing += lit(" (Un-norm)");

  filter = ToQStr(descriptor.Filter);

  if(descriptor.maxAniso > 1.0f)
    filter += lit(" Aniso %1x").arg(descriptor.maxAniso);

  if(descriptor.Filter.func == FilterFunc::Comparison)
    filter += QFormatStr(" (%1)").arg(ToQStr(descriptor.comparison));
  else if(descriptor.Filter.func != FilterFunc::Normal)
    filter += QFormatStr(" (%1)").arg(ToQStr(descriptor.Filter.func));

  QString lod =
      lit("LODs: %1 - %2")
          .arg((descriptor.minlod == -FLT_MAX ? lit("0") : QString::number(descriptor.minlod)))
          .arg((descriptor.maxlod == FLT_MAX ? lit("FLT_MAX") : QString::number(descriptor.maxlod)));

  if(descriptor.mipBias != 0.0f)
    lod += lit(" Bias %1").arg(descriptor.mipBias);

  return {QString(),
          bindset,
          slotname,
          descriptor.immutableSampler ? tr("Immutable Sampler") : tr("Sampler"),
          ToQStr(descriptor.name),
          addressing,
          filter + lit(", ") + lod};
}

void VulkanPipelineStateViewer::addResourceRow(ShaderReflection *shaderDetails,
                                               const VKPipe::Shader &stage, int bindset, int bind,
                                               const VKPipe::Pipeline &pipe, RDTreeWidget *resources,
                                               QMap<ResourceId, SamplerData> &samplers)
{
  const ShaderResource *shaderRes = NULL;
  const BindpointMap *bindMap = NULL;

  bool isrw = false;
  uint bindPoint = 0;

  if(shaderDetails != NULL)
  {
    for(int i = 0; i < shaderDetails->ReadOnlyResources.count; i++)
    {
      const ShaderResource &ro = shaderDetails->ReadOnlyResources[i];

      if(stage.BindpointMapping.ReadOnlyResources[ro.bindPoint].bindset == bindset &&
         stage.BindpointMapping.ReadOnlyResources[ro.bindPoint].bind == bind)
      {
        bindPoint = (uint)i;
        shaderRes = &ro;
        bindMap = &stage.BindpointMapping.ReadOnlyResources[ro.bindPoint];
      }
    }

    for(int i = 0; i < shaderDetails->ReadWriteResources.count; i++)
    {
      const ShaderResource &rw = shaderDetails->ReadWriteResources[i];

      if(stage.BindpointMapping.ReadWriteResources[rw.bindPoint].bindset == bindset &&
         stage.BindpointMapping.ReadWriteResources[rw.bindPoint].bind == bind)
      {
        bindPoint = (uint)i;
        isrw = true;
        shaderRes = &rw;
        bindMap = &stage.BindpointMapping.ReadWriteResources[rw.bindPoint];
      }
    }
  }

  const rdctype::array<VKPipe::BindingElement> *slotBinds = NULL;
  BindType bindType = BindType::Unknown;
  ShaderStageMask stageBits = ShaderStageMask::Unknown;

  if(bindset < pipe.DescSets.count && bind < pipe.DescSets[bindset].bindings.count)
  {
    slotBinds = &pipe.DescSets[bindset].bindings[bind].binds;
    bindType = pipe.DescSets[bindset].bindings[bind].type;
    stageBits = pipe.DescSets[bindset].bindings[bind].stageFlags;
  }
  else
  {
    if(shaderRes->IsSampler)
      bindType = BindType::Sampler;
    else if(shaderRes->IsSampler && shaderRes->IsTexture)
      bindType = BindType::ImageSampler;
    else if(shaderRes->resType == TextureDim::Buffer)
      bindType = BindType::ReadOnlyTBuffer;
    else
      bindType = BindType::ReadOnlyImage;
  }

  bool usedSlot = bindMap != NULL && bindMap->used;
  bool stageBitsIncluded = bool(stageBits & MaskForStage(stage.stage));

  // skip descriptors that aren't for this shader stage
  if(!usedSlot && !stageBitsIncluded)
    return;

  if(bindType == BindType::ConstantBuffer)
    return;

  // TODO - check compatibility between bindType and shaderRes.resType ?

  // consider it filled if any array element is filled
  bool filledSlot = false;
  for(int idx = 0; slotBinds != NULL && idx < slotBinds->count; idx++)
  {
    filledSlot |= (*slotBinds)[idx].res != ResourceId();
    if(bindType == BindType::Sampler || bindType == BindType::ImageSampler)
      filledSlot |= (*slotBinds)[idx].sampler != ResourceId();
  }

  // if it's masked out by stage bits, act as if it's not filled, so it's marked in red
  if(!stageBitsIncluded)
    filledSlot = false;

  if(showNode(usedSlot, filledSlot))
  {
    RDTreeWidgetItem *parentNode = resources->invisibleRootItem();

    QString setname = QString::number(bindset);

    QString slotname = QString::number(bind);
    if(shaderRes != NULL && shaderRes->name.count > 0)
      slotname += lit(": ") + ToQStr(shaderRes->name);

    int arrayLength = 0;
    if(slotBinds != NULL)
      arrayLength = slotBinds->count;
    else
      arrayLength = (int)bindMap->arraySize;

    // for arrays, add a parent element that we add the real cbuffers below
    if(arrayLength > 1)
    {
      RDTreeWidgetItem *node =
          new RDTreeWidgetItem({QString(), setname, slotname, tr("Array[%1]").arg(arrayLength),
                                QString(), QString(), QString(), QString()});

      if(!filledSlot)
        setEmptyRow(node);

      if(!usedSlot)
        setInactiveRow(node);

      resources->addTopLevelItem(node);

      // show the tree column
      resources->showColumn(0);
      parentNode = node;
    }

    for(int idx = 0; idx < arrayLength; idx++)
    {
      const VKPipe::BindingElement *descriptorBind = NULL;
      if(slotBinds != NULL)
        descriptorBind = &(*slotBinds)[idx];

      if(arrayLength > 1)
      {
        if(shaderRes != NULL && shaderRes->name.count > 0)
          slotname = QFormatStr("%1[%2]: %3").arg(bind).arg(idx).arg(ToQStr(shaderRes->name));
        else
          slotname = QFormatStr("%1[%2]").arg(bind).arg(idx);
      }

      bool isbuf = false;
      uint32_t w = 1, h = 1, d = 1;
      uint32_t a = 1;
      uint32_t samples = 1;
      uint64_t len = 0;
      QString format = tr("Unknown");
      QString name = tr("Empty");
      TextureDim restype = TextureDim::Unknown;
      QVariant tag;

      TextureDescription *tex = NULL;
      BufferDescription *buf = NULL;

      uint64_t descriptorLen = descriptorBind ? descriptorBind->size : 0;

      if(filledSlot && descriptorBind != NULL)
      {
        name = tr("Object %1").arg(ToQStr(descriptorBind->res));

        format = ToQStr(descriptorBind->viewfmt.strname);

        // check to see if it's a texture
        tex = m_Ctx.GetTexture(descriptorBind->res);
        if(tex)
        {
          w = tex->width;
          h = tex->height;
          d = tex->depth;
          a = tex->arraysize;
          name = ToQStr(tex->name);
          restype = tex->resType;
          samples = tex->msSamp;

          tag = QVariant::fromValue(descriptorBind->res);
        }

        // if not a texture, it must be a buffer
        buf = m_Ctx.GetBuffer(descriptorBind->res);
        if(buf)
        {
          len = buf->length;
          w = 0;
          h = 0;
          d = 0;
          a = 0;
          name = ToQStr(buf->name);
          restype = TextureDim::Buffer;

          if(descriptorLen == UINT64_MAX)
            descriptorLen = len - descriptorBind->offset;

          tag = QVariant::fromValue(
              VulkanBufferTag(isrw, bindPoint, buf->ID, descriptorBind->offset, descriptorLen));

          isbuf = true;
        }
      }
      else
      {
        name = tr("Empty");
        format = lit("-");
        w = h = d = a = 0;
      }

      RDTreeWidgetItem *node = NULL;
      RDTreeWidgetItem *samplerNode = NULL;

      if(bindType == BindType::ReadWriteBuffer || bindType == BindType::ReadOnlyTBuffer ||
         bindType == BindType::ReadWriteTBuffer)
      {
        if(!isbuf)
        {
          node = new RDTreeWidgetItem({
              QString(), bindset, slotname, ToQStr(bindType), lit("-"), lit("-"), QString(),
          });

          setEmptyRow(node);
        }
        else
        {
          QString range = lit("-");
          if(descriptorBind != NULL)
            range = QFormatStr("%1 - %2").arg(descriptorBind->offset).arg(descriptorLen);

          node = new RDTreeWidgetItem({
              QString(), bindset, slotname, ToQStr(bindType), name, tr("%1 bytes").arg(len), range,
          });

          node->setTag(tag);

          if(!filledSlot)
            setEmptyRow(node);

          if(!usedSlot)
            setInactiveRow(node);
        }
      }
      else if(bindType == BindType::Sampler)
      {
        if(descriptorBind == NULL || descriptorBind->sampler == ResourceId())
        {
          node = new RDTreeWidgetItem({
              QString(), bindset, slotname, ToQStr(bindType), lit("-"), lit("-"), QString(),
          });

          setEmptyRow(node);
        }
        else
        {
          node =
              new RDTreeWidgetItem(makeSampler(QString::number(bindset), slotname, *descriptorBind));

          if(!filledSlot)
            setEmptyRow(node);

          if(!usedSlot)
            setInactiveRow(node);

          SamplerData sampData;
          sampData.node = node;
          node->setTag(QVariant::fromValue(sampData));

          if(!samplers.contains(descriptorBind->sampler))
            samplers.insert(descriptorBind->sampler, sampData);
        }
      }
      else
      {
        if(descriptorBind == NULL || descriptorBind->res == ResourceId())
        {
          node = new RDTreeWidgetItem({
              QString(), bindset, slotname, ToQStr(bindType), lit("-"), lit("-"), QString(),
          });

          setEmptyRow(node);
        }
        else
        {
          QString typeName = ToQStr(restype) + lit(" ") + ToQStr(bindType);

          QString dim;

          if(restype == TextureDim::Texture3D)
            dim = QFormatStr("%1x%2x%3").arg(w).arg(h).arg(d);
          else if(restype == TextureDim::Texture1D || restype == TextureDim::Texture1DArray)
            dim = QString::number(w);
          else
            dim = QFormatStr("%1x%2").arg(w).arg(h);

          if(descriptorBind->swizzle[0] != TextureSwizzle::Red ||
             descriptorBind->swizzle[1] != TextureSwizzle::Green ||
             descriptorBind->swizzle[2] != TextureSwizzle::Blue ||
             descriptorBind->swizzle[3] != TextureSwizzle::Alpha)
          {
            format += tr(" swizzle[%1%2%3%4]")
                          .arg(ToQStr(descriptorBind->swizzle[0]))
                          .arg(ToQStr(descriptorBind->swizzle[1]))
                          .arg(ToQStr(descriptorBind->swizzle[2]))
                          .arg(ToQStr(descriptorBind->swizzle[3]));
          }

          if(restype == TextureDim::Texture1DArray || restype == TextureDim::Texture2DArray ||
             restype == TextureDim::Texture2DMSArray || restype == TextureDim::TextureCubeArray)
          {
            dim += QFormatStr(" %1[%2]").arg(ToQStr(restype)).arg(a);
          }

          if(restype == TextureDim::Texture2DMS || restype == TextureDim::Texture2DMSArray)
            dim += QFormatStr(", %1x MSAA").arg(samples);

          node = new RDTreeWidgetItem({
              QString(), bindset, slotname, typeName, name, dim, format,
          });

          node->setTag(tag);

          if(!filledSlot)
            setEmptyRow(node);

          if(!usedSlot)
            setInactiveRow(node);
        }

        if(bindType == BindType::ImageSampler)
        {
          if(descriptorBind == NULL || descriptorBind->sampler == ResourceId())
          {
            samplerNode = new RDTreeWidgetItem({
                QString(), bindset, slotname, ToQStr(bindType), lit("-"), lit("-"), QString(),
            });

            setEmptyRow(node);
          }
          else
          {
            if(!samplers.contains(descriptorBind->sampler))
            {
              samplerNode = new RDTreeWidgetItem(makeSampler(QString(), QString(), *descriptorBind));

              if(!filledSlot)
                setEmptyRow(samplerNode);

              if(!usedSlot)
                setInactiveRow(samplerNode);

              SamplerData sampData;
              sampData.node = samplerNode;
              samplerNode->setTag(QVariant::fromValue(sampData));

              samplers.insert(descriptorBind->sampler, sampData);
            }

            if(node != NULL)
            {
              m_CombinedImageSamplers[node] = samplers[descriptorBind->sampler].node;
              samplers[descriptorBind->sampler].images.push_back(node);
            }
          }
        }
      }

      if(descriptorBind && tex)
        setViewDetails(node, *descriptorBind, tex);
      else if(descriptorBind && buf)
        setViewDetails(node, *descriptorBind, buf);

      parentNode->addChild(node);

      if(samplerNode)
        parentNode->addChild(samplerNode);
    }
  }
}

void VulkanPipelineStateViewer::addConstantBlockRow(ShaderReflection *shaderDetails,
                                                    const VKPipe::Shader &stage, int bindset,
                                                    int bind, const VKPipe::Pipeline &pipe,
                                                    RDTreeWidget *ubos)
{
  const ConstantBlock *cblock = NULL;
  const BindpointMap *bindMap = NULL;

  uint32_t slot = ~0U;
  if(shaderDetails != NULL)
  {
    for(slot = 0; slot < (uint)shaderDetails->ConstantBlocks.count; slot++)
    {
      ConstantBlock cb = shaderDetails->ConstantBlocks[slot];
      if(stage.BindpointMapping.ConstantBlocks[cb.bindPoint].bindset == bindset &&
         stage.BindpointMapping.ConstantBlocks[cb.bindPoint].bind == bind)
      {
        cblock = &cb;
        bindMap = &stage.BindpointMapping.ConstantBlocks[cb.bindPoint];
        break;
      }
    }

    if(slot >= (uint)shaderDetails->ConstantBlocks.count)
      slot = ~0U;
  }

  const rdctype::array<VKPipe::BindingElement> *slotBinds = NULL;
  BindType bindType = BindType::ConstantBuffer;
  ShaderStageMask stageBits = ShaderStageMask::Unknown;

  if(bindset < pipe.DescSets.count && bind < pipe.DescSets[bindset].bindings.count)
  {
    slotBinds = &pipe.DescSets[bindset].bindings[bind].binds;
    bindType = pipe.DescSets[bindset].bindings[bind].type;
    stageBits = pipe.DescSets[bindset].bindings[bind].stageFlags;
  }

  bool usedSlot = bindMap != NULL && bindMap->used;
  bool stageBitsIncluded = bool(stageBits & MaskForStage(stage.stage));

  // skip descriptors that aren't for this shader stage
  if(!usedSlot && !stageBitsIncluded)
    return;

  if(bindType != BindType::ConstantBuffer)
    return;

  // consider it filled if any array element is filled (or it's push constants)
  bool filledSlot = cblock != NULL && !cblock->bufferBacked;
  for(int idx = 0; slotBinds != NULL && idx < slotBinds->count; idx++)
    filledSlot |= (*slotBinds)[idx].res != ResourceId();

  // if it's masked out by stage bits, act as if it's not filled, so it's marked in red
  if(!stageBitsIncluded)
    filledSlot = false;

  if(showNode(usedSlot, filledSlot))
  {
    RDTreeWidgetItem *parentNode = ubos->invisibleRootItem();

    QString setname = QString::number(bindset);

    QString slotname = QString::number(bind);
    if(cblock != NULL && cblock->name.count > 0)
      slotname += lit(": ") + ToQStr(cblock->name);

    int arrayLength = 0;
    if(slotBinds != NULL)
      arrayLength = slotBinds->count;
    else
      arrayLength = (int)bindMap->arraySize;

    // for arrays, add a parent element that we add the real cbuffers below
    if(arrayLength > 1)
    {
      RDTreeWidgetItem *node = new RDTreeWidgetItem(
          {QString(), setname, slotname, tr("Array[%1]").arg(arrayLength), QString(), QString()});

      if(!filledSlot)
        setEmptyRow(node);

      if(!usedSlot)
        setInactiveRow(node);

      parentNode = node;

      ubos->showColumn(0);
    }

    for(int idx = 0; idx < arrayLength; idx++)
    {
      const VKPipe::BindingElement *descriptorBind = NULL;
      if(slotBinds != NULL)
        descriptorBind = &(*slotBinds)[idx];

      if(arrayLength > 1)
      {
        if(cblock != NULL && cblock->name.count > 0)
          slotname = QFormatStr("%1[%2]: %3").arg(bind).arg(idx).arg(ToQStr(cblock->name));
        else
          slotname = QFormatStr("%1[%2]").arg(bind).arg(idx);
      }

      QString name = tr("Empty");
      uint64_t length = 0;
      int numvars = cblock != NULL ? cblock->variables.count : 0;
      uint64_t byteSize = cblock != NULL ? cblock->byteSize : 0;

      QString vecrange = lit("-");

      if(filledSlot && descriptorBind != NULL)
      {
        name = QString();
        length = descriptorBind->size;

        BufferDescription *buf = m_Ctx.GetBuffer(descriptorBind->res);
        if(buf)
        {
          name = ToQStr(buf->name);
          if(length == UINT64_MAX)
            length = buf->length - descriptorBind->offset;
        }

        if(name == QString())
          name = lit("UBO ") + ToQStr(descriptorBind->res);

        vecrange =
            QFormatStr("%1 - %2").arg(descriptorBind->offset).arg(descriptorBind->offset + length);
      }

      QString sizestr;

      // push constants or specialization constants
      if(cblock != NULL && !cblock->bufferBacked)
      {
        setname = QString();
        slotname = ToQStr(cblock->name);
        name = tr("Push constants");
        vecrange = QString();
        sizestr = tr("%1 Variables").arg(numvars);

        // could maybe get range from ShaderVariable.reg if it's filled out
        // from SPIR-V side.
      }
      else
      {
        if(length == byteSize)
          sizestr = tr("%1 Variables, %2 bytes").arg(numvars).arg(length);
        else
          sizestr =
              tr("%1 Variables, %2 bytes needed, %3 provided").arg(numvars).arg(byteSize).arg(length);

        if(length < byteSize)
          filledSlot = false;
      }

      RDTreeWidgetItem *node =
          new RDTreeWidgetItem({QString(), setname, slotname, name, vecrange, sizestr});

      node->setTag(QVariant::fromValue(VulkanCBufferTag(slot, (uint)idx)));

      if(!filledSlot)
        setEmptyRow(node);

      if(!usedSlot)
        setInactiveRow(node);

      parentNode->addChild(node);
    }
  }
}

void VulkanPipelineStateViewer::setShaderState(const VKPipe::Shader &stage,
                                               const VKPipe::Pipeline &pipe, QLabel *shader,
                                               RDTreeWidget *resources, RDTreeWidget *ubos)
{
  ShaderReflection *shaderDetails = stage.ShaderDetails;

  if(stage.Object == ResourceId())
    shader->setText(tr("Unbound Shader"));
  else
    shader->setText(ToQStr(stage.name));

  if(shaderDetails != NULL)
  {
    QString entryFunc = ToQStr(shaderDetails->EntryPoint);
    if(shaderDetails->DebugInfo.files.count > 0 || entryFunc != lit("main"))
      shader->setText(entryFunc + lit("()"));

    if(shaderDetails->DebugInfo.files.count > 0)
      shader->setText(entryFunc + lit("() - ") +
                      QFileInfo(ToQStr(shaderDetails->DebugInfo.files[0].first)).fileName());
  }

  int vs = 0;

  // hide the tree columns. The functions below will add it
  // if any array bindings are present
  resources->hideColumn(0);
  ubos->hideColumn(0);

  vs = resources->verticalScrollBar()->value();
  resources->setUpdatesEnabled(false);
  resources->clear();

  QMap<ResourceId, SamplerData> samplers;

  for(int bindset = 0; bindset < pipe.DescSets.count; bindset++)
  {
    for(int bind = 0; bind < pipe.DescSets[bindset].bindings.count; bind++)
    {
      addResourceRow(shaderDetails, stage, bindset, bind, pipe, resources, samplers);
    }

    // if we have a shader bound, go through and add rows for any resources it wants for binds that
    // aren't
    // in this descriptor set (e.g. if layout mismatches)
    if(shaderDetails != NULL)
    {
      for(int i = 0; i < shaderDetails->ReadOnlyResources.count; i++)
      {
        const ShaderResource &ro = shaderDetails->ReadOnlyResources[i];

        if(stage.BindpointMapping.ReadOnlyResources[ro.bindPoint].bindset == bindset &&
           stage.BindpointMapping.ReadOnlyResources[ro.bindPoint].bind >=
               pipe.DescSets[bindset].bindings.count)
        {
          addResourceRow(shaderDetails, stage, bindset,
                         stage.BindpointMapping.ReadOnlyResources[ro.bindPoint].bind, pipe,
                         resources, samplers);
        }
      }

      for(int i = 0; i < shaderDetails->ReadWriteResources.count; i++)
      {
        const ShaderResource &rw = shaderDetails->ReadWriteResources[i];

        if(stage.BindpointMapping.ReadWriteResources[rw.bindPoint].bindset == bindset &&
           stage.BindpointMapping.ReadWriteResources[rw.bindPoint].bind >=
               pipe.DescSets[bindset].bindings.count)
        {
          addResourceRow(shaderDetails, stage, bindset,
                         stage.BindpointMapping.ReadWriteResources[rw.bindPoint].bind, pipe,
                         resources, samplers);
        }
      }
    }
  }

  // if we have a shader bound, go through and add rows for any resources it wants for descriptor
  // sets that aren't
  // bound at all
  if(shaderDetails != NULL)
  {
    for(int i = 0; i < shaderDetails->ReadOnlyResources.count; i++)
    {
      const ShaderResource &ro = shaderDetails->ReadOnlyResources[i];

      if(stage.BindpointMapping.ReadOnlyResources[ro.bindPoint].bindset >= pipe.DescSets.count)
      {
        addResourceRow(
            shaderDetails, stage, stage.BindpointMapping.ReadOnlyResources[ro.bindPoint].bindset,
            stage.BindpointMapping.ReadOnlyResources[ro.bindPoint].bind, pipe, resources, samplers);
      }
    }

    for(int i = 0; i < shaderDetails->ReadWriteResources.count; i++)
    {
      const ShaderResource &rw = shaderDetails->ReadWriteResources[i];

      if(stage.BindpointMapping.ReadWriteResources[rw.bindPoint].bindset >= pipe.DescSets.count)
      {
        addResourceRow(
            shaderDetails, stage, stage.BindpointMapping.ReadWriteResources[rw.bindPoint].bindset,
            stage.BindpointMapping.ReadWriteResources[rw.bindPoint].bind, pipe, resources, samplers);
      }
    }
  }

  resources->clearSelection();
  resources->setUpdatesEnabled(true);
  resources->verticalScrollBar()->setValue(vs);

  vs = ubos->verticalScrollBar()->value();
  ubos->setUpdatesEnabled(false);
  ubos->clear();
  for(int bindset = 0; bindset < pipe.DescSets.count; bindset++)
  {
    for(int bind = 0; bind < pipe.DescSets[bindset].bindings.count; bind++)
    {
      addConstantBlockRow(shaderDetails, stage, bindset, bind, pipe, ubos);
    }

    // if we have a shader bound, go through and add rows for any cblocks it wants for binds that
    // aren't
    // in this descriptor set (e.g. if layout mismatches)
    if(shaderDetails != NULL)
    {
      for(int i = 0; i < shaderDetails->ConstantBlocks.count; i++)
      {
        ConstantBlock &cb = shaderDetails->ConstantBlocks[i];

        if(stage.BindpointMapping.ConstantBlocks[cb.bindPoint].bindset == bindset &&
           stage.BindpointMapping.ConstantBlocks[cb.bindPoint].bind >=
               pipe.DescSets[bindset].bindings.count)
        {
          addConstantBlockRow(shaderDetails, stage, bindset,
                              stage.BindpointMapping.ConstantBlocks[cb.bindPoint].bind, pipe, ubos);
        }
      }
    }
  }

  // if we have a shader bound, go through and add rows for any resources it wants for descriptor
  // sets that aren't
  // bound at all
  if(shaderDetails != NULL)
  {
    for(int i = 0; i < shaderDetails->ConstantBlocks.count; i++)
    {
      ConstantBlock &cb = shaderDetails->ConstantBlocks[i];

      if(stage.BindpointMapping.ConstantBlocks[cb.bindPoint].bindset >= pipe.DescSets.count &&
         cb.bufferBacked)
      {
        addConstantBlockRow(shaderDetails, stage,
                            stage.BindpointMapping.ConstantBlocks[cb.bindPoint].bindset,
                            stage.BindpointMapping.ConstantBlocks[cb.bindPoint].bind, pipe, ubos);
      }
    }
  }

  // search for push constants and add them last
  if(shaderDetails != NULL)
  {
    for(int cb = 0; cb < shaderDetails->ConstantBlocks.count; cb++)
    {
      ConstantBlock &cblock = shaderDetails->ConstantBlocks[cb];
      if(cblock.bufferBacked == false)
      {
        // could maybe get range from ShaderVariable.reg if it's filled out
        // from SPIR-V side.

        RDTreeWidgetItem *node =
            new RDTreeWidgetItem({QString(), QString(), ToQStr(cblock.name), tr("Push constants"),
                                  QString(), tr("%1 Variable(s)", "", cblock.variables.count)});

        node->setTag(QVariant::fromValue(VulkanCBufferTag(cb, 0)));

        ubos->addTopLevelItem(node);
      }
    }
  }
  ubos->clearSelection();
  ubos->setUpdatesEnabled(true);
  ubos->verticalScrollBar()->setValue(vs);
}

void VulkanPipelineStateViewer::setState()
{
  if(!m_Ctx.LogLoaded())
  {
    clearState();
    return;
  }

  m_CombinedImageSamplers.clear();

  const VKPipe::State &state = m_Ctx.CurVulkanPipelineState();
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
    for(const VKPipe::VertexAttribute &a : state.VI.attrs)
    {
      bool filledSlot = true;
      bool usedSlot = false;

      QString name = tr("Attribute %1").arg(i);

      if(state.m_VS.Object != ResourceId())
      {
        int attrib = -1;
        if((int32_t)a.location < state.m_VS.BindpointMapping.InputAttributes.count)
          attrib = state.m_VS.BindpointMapping.InputAttributes[a.location];

        if(attrib >= 0 && attrib < state.m_VS.ShaderDetails->InputSig.count)
        {
          name = ToQStr(state.m_VS.ShaderDetails->InputSig[attrib].varName);
          usedSlot = true;
        }
      }

      if(showNode(usedSlot, filledSlot))
      {
        RDTreeWidgetItem *node = new RDTreeWidgetItem(
            {i, name, a.location, a.binding, ToQStr(a.format.strname), a.byteoffset});

        usedBindings[a.binding] = true;

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

  m_BindNodes.clear();

  Topology topo = draw != NULL ? draw->topology : Topology::Unknown;

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

  ui->primRestart->setVisible(state.IA.primitiveRestartEnable);

  vs = ui->viBuffers->verticalScrollBar()->value();
  ui->viBuffers->setUpdatesEnabled(false);
  ui->viBuffers->clear();

  bool ibufferUsed = draw != NULL && (draw->flags & DrawFlags::UseIBuffer);

  if(state.IA.ibuffer.buf != ResourceId())
  {
    if(ibufferUsed || showDisabled)
    {
      QString name = tr("Buffer ") + ToQStr(state.IA.ibuffer.buf);
      uint64_t length = 1;

      if(!ibufferUsed)
        length = 0;

      BufferDescription *buf = m_Ctx.GetBuffer(state.IA.ibuffer.buf);

      if(buf)
      {
        name = ToQStr(buf->name);
        length = buf->length;
      }

      RDTreeWidgetItem *node = new RDTreeWidgetItem(
          {tr("Index"), name, tr("Index"), (qulonglong)state.IA.ibuffer.offs,
           draw != NULL ? draw->indexByteWidth : 0, (qulonglong)length, QString()});

      node->setTag(QVariant::fromValue(
          VulkanVBIBTag(state.IA.ibuffer.buf, draw != NULL ? draw->indexOffset : 0)));

      if(!ibufferUsed)
        setInactiveRow(node);

      if(state.IA.ibuffer.buf == ResourceId())
        setEmptyRow(node);

      ui->viBuffers->addTopLevelItem(node);
    }
  }
  else
  {
    if(ibufferUsed || showEmpty)
    {
      RDTreeWidgetItem *node = new RDTreeWidgetItem(
          {tr("Index"), tr("No Buffer Set"), tr("Index"), lit("-"), lit("-"), lit("-"), QString()});

      node->setTag(QVariant::fromValue(
          VulkanVBIBTag(state.IA.ibuffer.buf, draw != NULL ? draw->indexOffset : 0)));

      setEmptyRow(node);

      if(!ibufferUsed)
        setInactiveRow(node);

      ui->viBuffers->addTopLevelItem(node);
    }
  }

  m_VBNodes.clear();

  {
    int i = 0;
    for(; i < qMax(state.VI.vbuffers.count, state.VI.binds.count); i++)
    {
      const VKPipe::VB *vbuff = (i < state.VI.vbuffers.count ? &state.VI.vbuffers[i] : NULL);
      const VKPipe::VertexBinding *bind = NULL;

      for(int b = 0; b < state.VI.binds.count; b++)
      {
        if(state.VI.binds[b].vbufferBinding == (uint32_t)i)
          bind = &state.VI.binds[b];
      }

      bool filledSlot = ((vbuff != NULL && vbuff->buffer != ResourceId()) || bind != NULL);
      bool usedSlot = (usedBindings[i]);

      if(showNode(usedSlot, filledSlot))
      {
        QString name = tr("No Buffer");
        QString rate = lit("-");
        uint64_t length = 1;
        uint64_t offset = 0;
        uint32_t stride = 0;

        if(vbuff != NULL)
        {
          name = tr("Buffer ") + ToQStr(vbuff->buffer);
          offset = vbuff->offset;

          BufferDescription *buf = m_Ctx.GetBuffer(vbuff->buffer);
          if(buf)
          {
            name = ToQStr(buf->name);
            length = buf->length;
          }
        }

        if(bind != NULL)
        {
          stride = bind->bytestride;
          rate = bind->perInstance ? tr("Instance") : tr("Vertex");
        }
        else
        {
          name += tr(", No Binding");
        }

        RDTreeWidgetItem *node = NULL;

        if(filledSlot)
          node = new RDTreeWidgetItem(
              {i, name, rate, (qulonglong)offset, stride, (qulonglong)length, QString()});
        else
          node = new RDTreeWidgetItem(
              {i, tr("No Binding"), lit("-"), lit("-"), lit("-"), lit("-"), QString()});

        node->setTag(QVariant::fromValue(VulkanVBIBTag(vbuff != NULL ? vbuff->buffer : ResourceId(),
                                                       vbuff != NULL ? vbuff->offset : 0)));

        if(!filledSlot || bind == NULL || vbuff == NULL)
          setEmptyRow(node);

        if(!usedSlot)
          setInactiveRow(node);

        m_VBNodes.push_back(node);

        ui->viBuffers->addTopLevelItem(node);
      }
    }

    for(; i < (int)ARRAY_COUNT(usedBindings); i++)
    {
      if(usedBindings[i])
      {
        RDTreeWidgetItem *node = new RDTreeWidgetItem(
            {i, tr("No Binding"), lit("-"), lit("-"), lit("-"), lit("-"), QString()});

        node->setTag(QVariant::fromValue(VulkanVBIBTag(ResourceId(), 0)));

        setEmptyRow(node);

        setInactiveRow(node);

        ui->viBuffers->addTopLevelItem(node);

        m_VBNodes.push_back(node);
      }
    }
  }
  ui->viBuffers->clearSelection();
  ui->viBuffers->setUpdatesEnabled(true);
  ui->viBuffers->verticalScrollBar()->setValue(vs);

  setShaderState(state.m_VS, state.graphics, ui->vsShader, ui->vsResources, ui->vsUBOs);
  setShaderState(state.m_GS, state.graphics, ui->gsShader, ui->gsResources, ui->gsUBOs);
  setShaderState(state.m_TCS, state.graphics, ui->tcsShader, ui->tcsResources, ui->tcsUBOs);
  setShaderState(state.m_TES, state.graphics, ui->tesShader, ui->tesResources, ui->tesUBOs);
  setShaderState(state.m_FS, state.graphics, ui->fsShader, ui->fsResources, ui->fsUBOs);
  setShaderState(state.m_CS, state.compute, ui->csShader, ui->csResources, ui->csUBOs);

  ////////////////////////////////////////////////
  // Rasterizer

  vs = ui->viewports->verticalScrollBar()->value();
  ui->viewports->setUpdatesEnabled(false);
  ui->viewports->clear();

  int vs2 = ui->scissors->verticalScrollBar()->value();
  ui->scissors->setUpdatesEnabled(false);
  ui->scissors->clear();

  if(state.Pass.renderpass.obj != ResourceId())
  {
    ui->scissors->addTopLevelItem(
        new RDTreeWidgetItem({tr("Render Area"), state.Pass.renderArea.x, state.Pass.renderArea.y,
                              state.Pass.renderArea.width, state.Pass.renderArea.height}));
  }

  {
    int i = 0;
    for(const VKPipe::ViewportScissor &v : state.VP.viewportScissors)
    {
      RDTreeWidgetItem *node = new RDTreeWidgetItem(
          {i, v.vp.x, v.vp.y, v.vp.width, v.vp.height, v.vp.minDepth, v.vp.maxDepth});
      ui->viewports->addTopLevelItem(node);

      if(v.vp.width == 0 || v.vp.height == 0)
        setEmptyRow(node);

      node = new RDTreeWidgetItem({i, v.scissor.x, v.scissor.y, v.scissor.width, v.scissor.height});
      ui->scissors->addTopLevelItem(node);

      if(v.scissor.width == 0 || v.scissor.height == 0)
        setEmptyRow(node);

      i++;
    }
  }

  ui->viewports->verticalScrollBar()->setValue(vs);
  ui->viewports->clearSelection();
  ui->scissors->clearSelection();
  ui->scissors->verticalScrollBar()->setValue(vs2);

  ui->viewports->setUpdatesEnabled(true);
  ui->scissors->setUpdatesEnabled(true);

  ui->fillMode->setText(ToQStr(state.RS.fillMode));
  ui->cullMode->setText(ToQStr(state.RS.cullMode));
  ui->frontCCW->setPixmap(state.RS.FrontCCW ? tick : cross);

  ui->depthBias->setText(Formatter::Format(state.RS.depthBias));
  ui->depthBiasClamp->setText(Formatter::Format(state.RS.depthBiasClamp));
  ui->slopeScaledBias->setText(Formatter::Format(state.RS.slopeScaledDepthBias));

  ui->depthClamp->setPixmap(state.RS.depthClampEnable ? tick : cross);
  ui->rasterizerDiscard->setPixmap(state.RS.rasterizerDiscardEnable ? tick : cross);
  ui->lineWidth->setText(Formatter::Format(state.RS.lineWidth));

  ui->sampleCount->setText(QString::number(state.MSAA.rasterSamples));
  ui->sampleShading->setPixmap(state.MSAA.sampleShadingEnable ? tick : cross);
  ui->minSampleShading->setText(Formatter::Format(state.MSAA.minSampleShading));
  ui->sampleMask->setText(Formatter::Format(state.MSAA.sampleMask, true));

  ////////////////////////////////////////////////
  // Output Merger

  bool targets[32] = {};

  vs = ui->framebuffer->verticalScrollBar()->value();
  ui->framebuffer->setUpdatesEnabled(false);
  ui->framebuffer->clear();
  {
    int i = 0;
    for(const VKPipe::Attachment &p : state.Pass.framebuffer.attachments)
    {
      int colIdx = -1;
      for(int c = 0; c < state.Pass.renderpass.colorAttachments.count; c++)
      {
        if(state.Pass.renderpass.colorAttachments[c] == (uint)i)
        {
          colIdx = c;
          break;
        }
      }
      int resIdx = -1;
      for(int c = 0; c < state.Pass.renderpass.resolveAttachments.count; c++)
      {
        if(state.Pass.renderpass.resolveAttachments[c] == (uint)i)
        {
          resIdx = c;
          break;
        }
      }

      bool filledSlot = (p.img != ResourceId());
      bool usedSlot =
          (colIdx >= 0 || resIdx >= 0 || state.Pass.renderpass.depthstencilAttachment == i);

      if(showNode(usedSlot, filledSlot))
      {
        uint32_t w = 1, h = 1, d = 1;
        uint32_t a = 1;
        QString format = ToQStr(p.viewfmt.strname);
        QString name = tr("Texture ") + ToQStr(p.img);
        QString typeName = tr("Unknown");

        if(p.img == ResourceId())
        {
          name = tr("Empty");
          format = lit("-");
          typeName = lit("-");
          w = h = d = a = 0;
        }

        TextureDescription *tex = m_Ctx.GetTexture(p.img);
        if(tex)
        {
          w = tex->width;
          h = tex->height;
          d = tex->depth;
          a = tex->arraysize;
          name = ToQStr(tex->name);
          typeName = ToQStr(tex->resType);

          if(!tex->customName && state.m_FS.ShaderDetails != NULL)
          {
            for(int s = 0; s < state.m_FS.ShaderDetails->OutputSig.count; s++)
            {
              if(state.m_FS.ShaderDetails->OutputSig[s].regIndex == (uint32_t)colIdx &&
                 (state.m_FS.ShaderDetails->OutputSig[s].systemValue == ShaderBuiltin::Undefined ||
                  state.m_FS.ShaderDetails->OutputSig[s].systemValue == ShaderBuiltin::ColorOutput))
              {
                name = QFormatStr("<%1>").arg(ToQStr(state.m_FS.ShaderDetails->OutputSig[s].varName));
              }
            }
          }
        }

        if(p.swizzle[0] != TextureSwizzle::Red || p.swizzle[1] != TextureSwizzle::Green ||
           p.swizzle[2] != TextureSwizzle::Blue || p.swizzle[3] != TextureSwizzle::Alpha)
        {
          format += tr(" swizzle[%1%2%3%4]")
                        .arg(ToQStr(p.swizzle[0]))
                        .arg(ToQStr(p.swizzle[1]))
                        .arg(ToQStr(p.swizzle[2]))
                        .arg(ToQStr(p.swizzle[3]));
        }

        QString slotname;

        if(colIdx >= 0)
          slotname = QFormatStr("Color %1").arg(i);
        else if(resIdx >= 0)
          slotname = QFormatStr("Resolve %1").arg(i);
        else
          slotname = lit("Depth");

        RDTreeWidgetItem *node =
            new RDTreeWidgetItem({slotname, name, typeName, w, h, d, a, format, QString()});

        if(tex)
          node->setTag(QVariant::fromValue(p.img));

        if(p.img == ResourceId())
        {
          setEmptyRow(node);
        }
        else if(!usedSlot)
        {
          setInactiveRow(node);
        }
        else
        {
          targets[i] = true;
        }

        setViewDetails(node, p, tex);

        ui->framebuffer->addTopLevelItem(node);
      }

      i++;
    }
  }

  ui->framebuffer->clearSelection();
  ui->framebuffer->setUpdatesEnabled(true);
  ui->framebuffer->verticalScrollBar()->setValue(vs);

  vs = ui->blends->verticalScrollBar()->value();
  ui->blends->setUpdatesEnabled(false);
  ui->blends->clear();
  {
    int i = 0;
    for(const VKPipe::Blend &blend : state.CB.attachments)
    {
      bool filledSlot = true;
      bool usedSlot = (targets[i]);

      if(showNode(usedSlot, filledSlot))
      {
        RDTreeWidgetItem *node = new RDTreeWidgetItem(
            {i, blend.blendEnable ? tr("True") : tr("False"),

             ToQStr(blend.blend.Source), ToQStr(blend.blend.Destination),
             ToQStr(blend.blend.Operation),

             ToQStr(blend.alphaBlend.Source), ToQStr(blend.alphaBlend.Destination),
             ToQStr(blend.alphaBlend.Operation),

             QFormatStr("%1%2%3%4")
                 .arg((blend.writeMask & 0x1) == 0 ? lit("_") : lit("R"))
                 .arg((blend.writeMask & 0x2) == 0 ? lit("_") : lit("G"))
                 .arg((blend.writeMask & 0x4) == 0 ? lit("_") : lit("B"))
                 .arg((blend.writeMask & 0x8) == 0 ? lit("_") : lit("A"))});

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
                               .arg(state.CB.blendConst[0], 0, 'f', 2)
                               .arg(state.CB.blendConst[1], 0, 'f', 2)
                               .arg(state.CB.blendConst[2], 0, 'f', 2)
                               .arg(state.CB.blendConst[3], 0, 'f', 2));
  ui->logicOp->setText(state.CB.logicOpEnable ? ToQStr(state.CB.logic) : lit("-"));
  ui->alphaToOne->setPixmap(state.CB.alphaToOneEnable ? tick : cross);

  ui->depthEnabled->setPixmap(state.DS.depthTestEnable ? tick : cross);
  ui->depthFunc->setText(ToQStr(state.DS.depthCompareOp));
  ui->depthWrite->setPixmap(state.DS.depthWriteEnable ? tick : cross);

  if(state.DS.depthBoundsEnable)
  {
    ui->depthBounds->setText(Formatter::Format(state.DS.minDepthBounds) + lit("-") +
                             Formatter::Format(state.DS.maxDepthBounds));
    ui->depthBounds->setPixmap(QPixmap());
  }
  else
  {
    ui->depthBounds->setText(QString());
    ui->depthBounds->setPixmap(cross);
  }

  ui->stencils->setUpdatesEnabled(false);
  ui->stencils->clear();
  if(state.DS.stencilTestEnable)
  {
    ui->stencils->addTopLevelItem(new RDTreeWidgetItem(
        {tr("Front"), ToQStr(state.DS.front.Func), ToQStr(state.DS.front.FailOp),
         ToQStr(state.DS.front.DepthFailOp), ToQStr(state.DS.front.PassOp),
         Formatter::Format(state.DS.front.writeMask, true),
         Formatter::Format(state.DS.front.compareMask, true),
         Formatter::Format(state.DS.front.ref, true)}));
    ui->stencils->addTopLevelItem(
        new RDTreeWidgetItem({tr("Back"), ToQStr(state.DS.back.Func), ToQStr(state.DS.back.FailOp),
                              ToQStr(state.DS.back.DepthFailOp), ToQStr(state.DS.back.PassOp),
                              Formatter::Format(state.DS.back.writeMask, true),
                              Formatter::Format(state.DS.back.compareMask, true),
                              Formatter::Format(state.DS.back.ref, true)}));
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

QString VulkanPipelineStateViewer::formatMembers(int indent, const QString &nameprefix,
                                                 const rdctype::array<ShaderConstant> &vars)
{
  QString indentstr(indent * 4, QLatin1Char(' '));

  QString ret = QString();

  int i = 0;

  for(const ShaderConstant &v : vars)
  {
    if(v.type.members.count > 0)
    {
      if(i > 0)
        ret += lit("\n");
      ret += indentstr + lit("// struct %1\n").arg(ToQStr(v.type.descriptor.name));
      ret += indentstr + lit("{\n") +
             formatMembers(indent + 1, ToQStr(v.name) + lit("_"), v.type.members) + indentstr +
             lit("}\n");
      if(i < vars.count - 1)
        ret += lit("\n");
    }
    else
    {
      QString arr = QString();
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

void VulkanPipelineStateViewer::resource_itemActivated(RDTreeWidgetItem *item, int column)
{
  const VKPipe::Shader *stage = stageForSender(item->treeWidget());

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
  else if(tag.canConvert<VulkanBufferTag>())
  {
    VulkanBufferTag buf = tag.value<VulkanBufferTag>();

    const ShaderResource &shaderRes = buf.rwRes
                                          ? stage->ShaderDetails->ReadWriteResources[buf.bindPoint]
                                          : stage->ShaderDetails->ReadOnlyResources[buf.bindPoint];

    QString format = lit("// struct %1\n").arg(ToQStr(shaderRes.variableType.descriptor.name));

    if(shaderRes.variableType.members.count > 1)
    {
      format += lit("// members skipped as they are fixed size:\n");
      for(int i = 0; i < shaderRes.variableType.members.count - 1; i++)
        format += QFormatStr("%1 %2;\n")
                      .arg(ToQStr(shaderRes.variableType.members[i].type.descriptor.name))
                      .arg(ToQStr(shaderRes.variableType.members[i].name));
    }

    if(shaderRes.variableType.members.count > 0)
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

      if(desc.name.count > 0)
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

void VulkanPipelineStateViewer::ubo_itemActivated(RDTreeWidgetItem *item, int column)
{
  const VKPipe::Shader *stage = stageForSender(item->treeWidget());

  if(stage == NULL)
    return;

  QVariant tag = item->tag();

  if(!tag.canConvert<VulkanCBufferTag>())
    return;

  VulkanCBufferTag cb = tag.value<VulkanCBufferTag>();

  IConstantBufferPreviewer *prev = m_Ctx.ViewConstantBuffer(stage->stage, cb.slotIdx, cb.arrayIdx);

  m_Ctx.AddDockWindow(prev->Widget(), DockReference::RightOf, this, 0.3f);
}

void VulkanPipelineStateViewer::on_viAttrs_itemActivated(RDTreeWidgetItem *item, int column)
{
  on_meshView_clicked();
}

void VulkanPipelineStateViewer::on_viBuffers_itemActivated(RDTreeWidgetItem *item, int column)
{
  QVariant tag = item->tag();

  if(tag.canConvert<VulkanVBIBTag>())
  {
    VulkanVBIBTag buf = tag.value<VulkanVBIBTag>();

    if(buf.id != ResourceId())
    {
      IBufferViewer *viewer = m_Ctx.ViewBuffer(buf.offset, UINT64_MAX, buf.id);

      m_Ctx.AddDockWindow(viewer->Widget(), DockReference::AddTo, this);
    }
  }
}

void VulkanPipelineStateViewer::highlightIABind(int slot)
{
  int idx = ((slot + 1) * 21) % 32;    // space neighbouring colours reasonably distinctly

  const VKPipe::VertexInput &VI = m_Ctx.CurVulkanPipelineState().VI;

  QColor col = QColor::fromHslF(float(idx) / 32.0f, 1.0f,
                                qBound(0.05, palette().color(QPalette::Base).lightnessF(), 0.95));

  ui->viAttrs->beginUpdate();
  ui->viBuffers->beginUpdate();

  if(slot < m_VBNodes.count())
  {
    m_VBNodes[slot]->setBackgroundColor(col);
    m_VBNodes[slot]->setForegroundColor(contrastingColor(col, QColor(0, 0, 0)));
  }

  if(slot < m_BindNodes.count())
  {
    m_BindNodes[slot]->setBackgroundColor(col);
    m_BindNodes[slot]->setForegroundColor(contrastingColor(col, QColor(0, 0, 0)));
  }

  for(int i = 0; i < ui->viAttrs->topLevelItemCount(); i++)
  {
    RDTreeWidgetItem *item = ui->viAttrs->topLevelItem(i);

    if((int)VI.attrs[i].binding != slot)
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

void VulkanPipelineStateViewer::on_viAttrs_mouseMove(QMouseEvent *e)
{
  if(!m_Ctx.LogLoaded())
    return;

  QModelIndex idx = ui->viAttrs->indexAt(e->pos());

  vertex_leave(NULL);

  const VKPipe::VertexInput &VI = m_Ctx.CurVulkanPipelineState().VI;

  if(idx.isValid())
  {
    if(idx.row() >= 0 && idx.row() < VI.attrs.count)
    {
      uint32_t binding = VI.attrs[idx.row()].binding;

      highlightIABind((int)binding);
    }
  }
}

void VulkanPipelineStateViewer::on_viBuffers_mouseMove(QMouseEvent *e)
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

void VulkanPipelineStateViewer::vertex_leave(QEvent *e)
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

void VulkanPipelineStateViewer::on_pipeFlow_stageSelected(int index)
{
  ui->stagesTabs->setCurrentIndex(index);
}

void VulkanPipelineStateViewer::shaderView_clicked()
{
  const VKPipe::Shader *stage = stageForSender(qobject_cast<QWidget *>(QObject::sender()));

  if(stage == NULL || stage->Object == ResourceId())
    return;

  ShaderReflection *shaderDetails = stage->ShaderDetails;

  IShaderViewer *shad = m_Ctx.ViewShader(&stage->BindpointMapping, shaderDetails, stage->stage);

  m_Ctx.AddDockWindow(shad->Widget(), DockReference::AddTo, this);
}

void VulkanPipelineStateViewer::shaderLabel_clicked(QMouseEvent *event)
{
  // forward to shaderView_clicked, we only need this to handle the different parameter, and we
  // can't use a lambda because then QObject::sender() is NULL
  shaderView_clicked();
}

void VulkanPipelineStateViewer::shaderEdit_clicked()
{
  QWidget *sender = qobject_cast<QWidget *>(QObject::sender());
  const VKPipe::Shader *stage = stageForSender(sender);

  if(!stage || stage->Object == ResourceId())
    return;

  const ShaderReflection *shaderDetails = stage->ShaderDetails;

  if(!shaderDetails)
    return;

  QString entryFunc = lit("EditedShader%1S").arg(ToQStr(stage->stage, GraphicsAPI::Vulkan)[0]);

  QString mainfile;

  QStringMap files;

  bool hasOrigSource = m_Common.PrepareShaderEditing(shaderDetails, entryFunc, files, mainfile);

  if(hasOrigSource)
  {
    if(files.empty())
      return;
  }
  else
  {
    QString glsl;

    if(!m_Ctx.Config().SPIRVDisassemblers.isEmpty())
      glsl = disassembleSPIRV(shaderDetails);

    mainfile = lit("generated.glsl");

    files[mainfile] = glsl;

    if(glsl.isEmpty())
    {
      m_Ctx.Replay().AsyncInvoke(
          [this, stage, shaderDetails, entryFunc, mainfile](IReplayController *r) {
            rdctype::str disasm = r->DisassembleShader(shaderDetails, "");

            GUIInvoke::call([this, stage, shaderDetails, entryFunc, mainfile, disasm]() {
              QStringMap fileMap;
              fileMap[mainfile] = ToQStr(disasm);
              m_Common.EditShader(stage->stage, stage->Object, shaderDetails, entryFunc, fileMap,
                                  mainfile);
            });
          });
      return;
    }
  }

  m_Common.EditShader(stage->stage, stage->Object, shaderDetails, entryFunc, files, mainfile);
}

QString VulkanPipelineStateViewer::disassembleSPIRV(const ShaderReflection *shaderDetails)
{
  QString glsl;

  const SPIRVDisassembler &disasm = m_Ctx.Config().SPIRVDisassemblers[0];

  if(disasm.executable.isEmpty())
    return QString();

  QString spv_bin_file = QDir(QDir::tempPath()).absoluteFilePath(lit("spv_bin.spv"));

  QFile binHandle(spv_bin_file);
  if(binHandle.open(QFile::WriteOnly | QIODevice::Truncate))
  {
    binHandle.write(
        QByteArray((const char *)shaderDetails->RawBytes.elems, shaderDetails->RawBytes.count));
    binHandle.close();
  }
  else
  {
    RDDialog::critical(this, tr("Error writing temp file"),
                       tr("Couldn't write temporary SPIR-V file %1.").arg(spv_bin_file));
    return QString();
  }

  if(!disasm.args.contains(lit("{spv_bin}")))
  {
    RDDialog::critical(
        this, tr("Wrongly configured disassembler"),
        tr("Please use {spv_bin} in the disassembler arguments to specify the input file."));
    return QString();
  }

  LambdaThread *thread = new LambdaThread([this, &glsl, &disasm, spv_bin_file]() {
    QString spv_disas_file = QDir(QDir::tempPath()).absoluteFilePath(lit("spv_disas.txt"));

    QString args = disasm.args;

    bool writesToFile = disasm.args.contains(lit("{spv_disas}"));

    args.replace(lit("{spv_bin}"), spv_bin_file);
    args.replace(lit("{spv_disas}"), spv_disas_file);

    QStringList argList = ParseArgsList(args);

    QProcess process;
    process.start(disasm.executable, argList);
    process.waitForFinished();

    if(process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0)
    {
      GUIInvoke::call([this]() {
        RDDialog::critical(this, tr("Error running disassembler"),
                           tr("There was an error invoking the external SPIR-V disassembler."));
      });
    }

    if(writesToFile)
    {
      QFile outputHandle(spv_disas_file);
      if(outputHandle.open(QFile::ReadOnly | QIODevice::Text))
      {
        glsl = QString::fromUtf8(outputHandle.readAll());
        outputHandle.close();
      }
    }
    else
    {
      glsl = QString::fromUtf8(process.readAll());
    }

    QFile::remove(spv_bin_file);
    QFile::remove(spv_disas_file);
  });
  thread->start();

  ShowProgressDialog(this, tr("Please wait - running external disassembler"),
                     [thread]() { return !thread->isRunning(); });

  thread->deleteLater();

  return glsl;
}

void VulkanPipelineStateViewer::shaderSave_clicked()
{
  const VKPipe::Shader *stage = stageForSender(qobject_cast<QWidget *>(QObject::sender()));

  if(stage == NULL)
    return;

  ShaderReflection *shaderDetails = stage->ShaderDetails;

  if(stage->Object == ResourceId())
    return;

  m_Common.SaveShaderFile(shaderDetails);
}

void VulkanPipelineStateViewer::exportHTML(QXmlStreamWriter &xml, VKPipe::VertexInput &vi)
{
  {
    xml.writeStartElement(lit("h3"));
    xml.writeCharacters(tr("Attributes"));
    xml.writeEndElement();

    QList<QVariantList> rows;

    for(const VKPipe::VertexAttribute &attr : vi.attrs)
      rows.push_back({attr.location, attr.binding, ToQStr(attr.format.strname), attr.byteoffset});

    m_Common.exportHTMLTable(xml, {tr("Location"), tr("Binding"), tr("Format"), tr("Offset")}, rows);
  }

  {
    xml.writeStartElement(lit("h3"));
    xml.writeCharacters(tr("Bindings"));
    xml.writeEndElement();

    QList<QVariantList> rows;

    for(const VKPipe::VertexBinding &attr : vi.binds)
      rows.push_back({attr.vbufferBinding, attr.bytestride,
                      attr.perInstance ? tr("PER_INSTANCE") : tr("PER_VERTEX")});

    m_Common.exportHTMLTable(xml, {tr("Binding"), tr("Byte Stride"), tr("Step Rate")}, rows);
  }

  {
    xml.writeStartElement(lit("h3"));
    xml.writeCharacters(tr("Vertex Buffers"));
    xml.writeEndElement();

    QList<QVariantList> rows;

    int i = 0;
    for(const VKPipe::VB &vb : vi.vbuffers)
    {
      QString name = tr("Buffer %1").arg(ToQStr(vb.buffer));
      uint64_t length = 0;

      if(vb.buffer == ResourceId())
      {
        continue;
      }
      else
      {
        BufferDescription *buf = m_Ctx.GetBuffer(vb.buffer);
        if(buf)
        {
          name = ToQStr(buf->name);
          length = buf->length;
        }
      }

      rows.push_back({i, name, (qulonglong)vb.offset, (qulonglong)length});

      i++;
    }

    m_Common.exportHTMLTable(xml, {tr("Binding"), tr("Buffer"), tr("Offset"), tr("Byte Length")},
                             rows);
  }
}

void VulkanPipelineStateViewer::exportHTML(QXmlStreamWriter &xml, VKPipe::InputAssembly &ia)
{
  {
    xml.writeStartElement(lit("h3"));
    xml.writeCharacters(tr("Index Buffer"));
    xml.writeEndElement();

    BufferDescription *ib = m_Ctx.GetBuffer(ia.ibuffer.buf);

    QString name = tr("Empty");
    uint64_t length = 0;

    if(ib)
    {
      name = ToQStr(ib->name);
      length = ib->length;
    }

    QString ifmt = lit("UNKNOWN");
    if(m_Ctx.CurDrawcall()->indexByteWidth == 2)
      ifmt = lit("UINT16");
    if(m_Ctx.CurDrawcall()->indexByteWidth == 4)
      ifmt = lit("UINT32");

    m_Common.exportHTMLTable(
        xml, {tr("Buffer"), tr("Format"), tr("Offset"), tr("Byte Length"), tr("Primitive Restart")},
        {name, ifmt, (qulonglong)ia.ibuffer.offs, (qulonglong)length,
         ia.primitiveRestartEnable ? tr("Yes") : tr("No")});
  }

  xml.writeStartElement(lit("p"));
  xml.writeEndElement();

  m_Common.exportHTMLTable(
      xml, {tr("Primitive Topology"), tr("Tessellation Control Points")},
      {ToQStr(m_Ctx.CurDrawcall()->topology), m_Ctx.CurVulkanPipelineState().Tess.numControlPoints});
}

void VulkanPipelineStateViewer::exportHTML(QXmlStreamWriter &xml, VKPipe::Shader &sh)
{
  ShaderReflection *shaderDetails = sh.ShaderDetails;

  {
    xml.writeStartElement(lit("h3"));
    xml.writeCharacters(tr("Shader"));
    xml.writeEndElement();

    QString shadername = tr("Unknown");

    if(sh.Object == ResourceId())
      shadername = tr("Unbound");
    else
      shadername = ToQStr(sh.name);

    if(shaderDetails)
    {
      QString entryFunc = ToQStr(shaderDetails->EntryPoint);
      if(entryFunc != lit("main"))
        shadername = QFormatStr("%1()").arg(entryFunc);
      else if(shaderDetails->DebugInfo.files.count > 0)
        shadername = QFormatStr("%1() - %2")
                         .arg(entryFunc)
                         .arg(QFileInfo(ToQStr(shaderDetails->DebugInfo.files[0].first)).fileName());
    }

    xml.writeStartElement(lit("p"));
    xml.writeCharacters(shadername);
    xml.writeEndElement();

    if(sh.Object == ResourceId())
      return;
  }

  const VKPipe::Pipeline &pipeline =
      (sh.stage == ShaderStage::Compute ? m_Ctx.CurVulkanPipelineState().compute
                                        : m_Ctx.CurVulkanPipelineState().graphics);

  if(shaderDetails && shaderDetails->ConstantBlocks.count > 0)
  {
    xml.writeStartElement(lit("h3"));
    xml.writeCharacters(tr("UBOs"));
    xml.writeEndElement();

    QList<QVariantList> rows;

    for(int i = 0; i < shaderDetails->ConstantBlocks.count; i++)
    {
      const ConstantBlock &b = shaderDetails->ConstantBlocks[i];
      const BindpointMap &bindMap = sh.BindpointMapping.ConstantBlocks[i];

      if(!bindMap.used)
        continue;

      const VKPipe::DescriptorSet &set =
          pipeline.DescSets[sh.BindpointMapping.ConstantBlocks[i].bindset];
      const VKPipe::DescriptorBinding &bind =
          set.bindings[sh.BindpointMapping.ConstantBlocks[i].bind];

      QString setname = QString::number(bindMap.bindset);

      QString slotname = QFormatStr("%1: %2").arg(bindMap.bind).arg(ToQStr(b.name));

      for(uint32_t a = 0; a < bind.descriptorCount; a++)
      {
        const VKPipe::BindingElement &descriptorBind = bind.binds[a];

        ResourceId id = bind.binds[a].res;

        if(bindMap.arraySize > 1)
          slotname = QFormatStr("%1: %2[%3]").arg(bindMap.bind).arg(ToQStr(b.name)).arg(a);

        QString name;
        uint64_t byteOffset = descriptorBind.offset;
        uint64_t length = descriptorBind.size;
        int numvars = b.variables.count;

        if(descriptorBind.res == ResourceId())
        {
          name = tr("Empty");
          length = 0;
        }

        BufferDescription *buf = m_Ctx.GetBuffer(id);
        if(buf)
        {
          name = ToQStr(buf->name);

          if(length == UINT64_MAX)
            length = buf->length - byteOffset;
        }

        if(name.isEmpty())
          name = tr("UBO %1").arg(ToQStr(descriptorBind.res));

        // push constants
        if(!b.bufferBacked)
        {
          setname = QString();
          slotname = ToQStr(b.name);
          name = tr("Push constants");
          byteOffset = 0;
          length = 0;

          // could maybe get range/size from ShaderVariable.reg if it's filled out
          // from SPIR-V side.
        }

        rows.push_back({setname, slotname, name, (qulonglong)byteOffset, (qulonglong)length,
                        numvars, b.byteSize});
      }
    }

    m_Common.exportHTMLTable(xml, {tr("Set"), tr("Bind"), tr("Buffer"), tr("Byte Offset"),
                                   tr("Byte Size"), tr("Number of Variables"), tr("Bytes Needed")},
                             rows);
  }

  if(shaderDetails->ReadOnlyResources.count > 0)
  {
    xml.writeStartElement(lit("h3"));
    xml.writeCharacters(tr("Read-only Resources"));
    xml.writeEndElement();

    QList<QVariantList> rows;

    for(int i = 0; i < shaderDetails->ReadOnlyResources.count; i++)
    {
      const ShaderResource &b = shaderDetails->ReadOnlyResources[i];
      const BindpointMap &bindMap = sh.BindpointMapping.ReadOnlyResources[i];

      if(!bindMap.used)
        continue;

      const VKPipe::DescriptorSet &set =
          pipeline.DescSets[sh.BindpointMapping.ReadOnlyResources[i].bindset];
      const VKPipe::DescriptorBinding &bind =
          set.bindings[sh.BindpointMapping.ReadOnlyResources[i].bind];

      QString setname = QString::number(bindMap.bindset);

      QString slotname = QFormatStr("%1: %2").arg(bindMap.bind).arg(ToQStr(b.name));

      for(uint32_t a = 0; a < bind.descriptorCount; a++)
      {
        const VKPipe::BindingElement &descriptorBind = bind.binds[a];

        ResourceId id = bind.binds[a].res;

        if(bindMap.arraySize > 1)
          slotname = QFormatStr("%1: %2[%3]").arg(bindMap.bind).arg(ToQStr(b.name)).arg(a);

        QString name;

        if(descriptorBind.res == ResourceId())
          name = tr("Empty");

        BufferDescription *buf = m_Ctx.GetBuffer(id);
        if(buf)
          name = ToQStr(buf->name);

        TextureDescription *tex = m_Ctx.GetTexture(id);
        if(tex)
          name = ToQStr(tex->name);

        if(name.isEmpty())
          name = tr("Resource %1").arg(ToQStr(descriptorBind.res));

        uint64_t w = 1;
        uint32_t h = 1, d = 1;
        uint32_t arr = 0;
        QString format = tr("Unknown");
        QString viewParams;

        if(tex)
        {
          w = tex->width;
          h = tex->height;
          d = tex->depth;
          arr = tex->arraysize;
          format = ToQStr(tex->format.strname);
          name = ToQStr(tex->name);

          if(tex->mips > 1)
          {
            viewParams = tr("Mips: %1-%2")
                             .arg(descriptorBind.baseMip)
                             .arg(descriptorBind.baseMip + descriptorBind.numMip - 1);
          }

          if(tex->arraysize > 1)
          {
            if(!viewParams.isEmpty())
              viewParams += lit(", ");
            viewParams += tr("Layers: %1-%2")
                              .arg(descriptorBind.baseLayer)
                              .arg(descriptorBind.baseLayer + descriptorBind.numLayer - 1);
          }
        }

        if(buf)
        {
          w = buf->length;
          h = 0;
          d = 0;
          a = 0;
          format = lit("-");
          name = ToQStr(buf->name);

          uint64_t length = descriptorBind.size;

          if(length == UINT64_MAX)
            length = buf->length - descriptorBind.offset;

          viewParams =
              tr("Byte Range: %1 - %2").arg(descriptorBind.offset).arg(descriptorBind.offset + length);
        }

        if(bind.type != BindType::Sampler)
          rows.push_back({setname, slotname, name, ToQStr(bind.type), (qulonglong)w, h, d, arr,
                          format, viewParams});

        if(bind.type == BindType::ImageSampler || bind.type == BindType::Sampler)
        {
          name = tr("Sampler %1").arg(ToQStr(descriptorBind.sampler));

          if(bind.type == BindType::ImageSampler)
            setname = slotname = QString();

          QVariantList sampDetails = makeSampler(QString(), QString(), descriptorBind);
          rows.push_back({setname, slotname, name, ToQStr(bind.type), QString(), QString(),
                          QString(), QString(), sampDetails[5], sampDetails[6]});
        }
      }
    }

    m_Common.exportHTMLTable(
        xml, {tr("Set"), tr("Bind"), tr("Buffer"), tr("Resource Type"), tr("Width"), tr("Height"),
              tr("Depth"), tr("Array Size"), tr("Resource Format"), tr("View Parameters")},
        rows);
  }

  if(shaderDetails->ReadWriteResources.count > 0)
  {
    xml.writeStartElement(lit("h3"));
    xml.writeCharacters(tr("Read-write Resources"));
    xml.writeEndElement();

    QList<QVariantList> rows;

    for(int i = 0; i < shaderDetails->ReadWriteResources.count; i++)
    {
      const ShaderResource &b = shaderDetails->ReadWriteResources[i];
      const BindpointMap &bindMap = sh.BindpointMapping.ReadWriteResources[i];

      if(!bindMap.used)
        continue;

      const VKPipe::DescriptorSet &set =
          pipeline.DescSets[sh.BindpointMapping.ReadWriteResources[i].bindset];
      const VKPipe::DescriptorBinding &bind =
          set.bindings[sh.BindpointMapping.ReadWriteResources[i].bind];

      QString setname = QString::number(bindMap.bindset);

      QString slotname = QFormatStr("%1: %2").arg(bindMap.bind).arg(ToQStr(b.name));

      for(uint32_t a = 0; a < bind.descriptorCount; a++)
      {
        const VKPipe::BindingElement &descriptorBind = bind.binds[a];

        ResourceId id = bind.binds[a].res;

        if(bindMap.arraySize > 1)
          slotname = QFormatStr("%1: %2[%3]").arg(bindMap.bind).arg(ToQStr(b.name)).arg(a);

        QString name;

        if(descriptorBind.res == ResourceId())
          name = tr("Empty");

        BufferDescription *buf = m_Ctx.GetBuffer(id);
        if(buf)
          name = ToQStr(buf->name);

        TextureDescription *tex = m_Ctx.GetTexture(id);
        if(tex)
          name = ToQStr(tex->name);

        if(name.isEmpty())
          name = tr("Resource %1").arg(ToQStr(descriptorBind.res));

        uint64_t w = 1;
        uint32_t h = 1, d = 1;
        uint32_t arr = 0;
        QString format = tr("Unknown");
        QString viewParams;

        if(tex)
        {
          w = tex->width;
          h = tex->height;
          d = tex->depth;
          arr = tex->arraysize;
          format = ToQStr(tex->format.strname);
          name = ToQStr(tex->name);

          if(tex->mips > 1)
          {
            viewParams = tr("Mips: %1-%2")
                             .arg(descriptorBind.baseMip)
                             .arg(descriptorBind.baseMip + descriptorBind.numMip - 1);
          }

          if(tex->arraysize > 1)
          {
            if(!viewParams.isEmpty())
              viewParams += lit(", ");
            viewParams += tr("Layers: %1-%2")
                              .arg(descriptorBind.baseLayer)
                              .arg(descriptorBind.baseLayer + descriptorBind.numLayer - 1);
          }
        }

        if(buf)
        {
          w = buf->length;
          h = 0;
          d = 0;
          a = 0;
          format = lit("-");
          name = ToQStr(buf->name);

          uint64_t length = descriptorBind.size;

          if(length == UINT64_MAX)
            length = buf->length - descriptorBind.offset;

          viewParams =
              tr("Byte Range: %1 - %2").arg(descriptorBind.offset).arg(descriptorBind.offset + length);
        }

        rows.push_back({setname, slotname, name, ToQStr(bind.type), (qulonglong)w, h, d, arr,
                        format, viewParams});
      }
    }

    m_Common.exportHTMLTable(
        xml, {tr("Set"), tr("Bind"), tr("Buffer"), tr("Resource Type"), tr("Width"), tr("Height"),
              tr("Depth"), tr("Array Size"), tr("Resource Format"), tr("View Parameters")},
        rows);
  }
}

void VulkanPipelineStateViewer::exportHTML(QXmlStreamWriter &xml, VKPipe::Raster &rs)
{
  {
    xml.writeStartElement(lit("h3"));
    xml.writeCharacters(tr("Raster State"));
    xml.writeEndElement();

    m_Common.exportHTMLTable(
        xml, {tr("Fill Mode"), tr("Cull Mode"), tr("Front CCW")},
        {ToQStr(rs.fillMode), ToQStr(rs.cullMode), rs.FrontCCW ? tr("Yes") : tr("No")});

    xml.writeStartElement(lit("p"));
    xml.writeEndElement();

    m_Common.exportHTMLTable(xml, {tr("Depth Clip Enable"), tr("Rasterizer Discard Enable")},
                             {rs.depthClampEnable ? tr("Yes") : tr("No"),
                              rs.rasterizerDiscardEnable ? tr("Yes") : tr("No")});

    xml.writeStartElement(lit("p"));
    xml.writeEndElement();

    m_Common.exportHTMLTable(
        xml, {tr("Depth Bias"), tr("Depth Bias Clamp"), tr("Slope Scaled Bias"), tr("Line Width")},
        {Formatter::Format(rs.depthBias), Formatter::Format(rs.depthBiasClamp),
         Formatter::Format(rs.slopeScaledDepthBias), Formatter::Format(rs.lineWidth)});
  }

  VKPipe::MultiSample &msaa = m_Ctx.CurVulkanPipelineState().MSAA;

  {
    xml.writeStartElement(lit("h3"));
    xml.writeCharacters(tr("Multisampling State"));
    xml.writeEndElement();

    m_Common.exportHTMLTable(
        xml, {tr("Raster Samples"), tr("Sample-rate shading"), tr("Min Sample Shading Rate"),
              tr("Sample Mask")},
        {msaa.rasterSamples, msaa.sampleShadingEnable ? tr("Yes") : tr("No"),
         Formatter::Format(msaa.minSampleShading), Formatter::Format(msaa.sampleMask, true)});
  }

  VKPipe::ViewState &vp = m_Ctx.CurVulkanPipelineState().VP;

  {
    xml.writeStartElement(lit("h3"));
    xml.writeCharacters(tr("Viewports"));
    xml.writeEndElement();

    QList<QVariantList> rows;

    int i = 0;
    for(const VKPipe::ViewportScissor &vs : vp.viewportScissors)
    {
      const VKPipe::Viewport &v = vs.vp;

      rows.push_back({i, v.x, v.y, v.width, v.height, v.minDepth, v.maxDepth});

      i++;
    }

    m_Common.exportHTMLTable(xml, {tr("Slot"), tr("X"), tr("Y"), tr("Width"), tr("Height"),
                                   tr("Min Depth"), tr("Max Depth")},
                             rows);
  }

  {
    xml.writeStartElement(lit("h3"));
    xml.writeCharacters(tr("Scissors"));
    xml.writeEndElement();

    QList<QVariantList> rows;

    int i = 0;
    for(const VKPipe::ViewportScissor &vs : vp.viewportScissors)
    {
      const VKPipe::Scissor &s = vs.scissor;

      rows.push_back({i, s.x, s.y, s.width, s.height});

      i++;
    }

    m_Common.exportHTMLTable(xml, {tr("Slot"), tr("X"), tr("Y"), tr("Width"), tr("Height")}, rows);
  }
}

void VulkanPipelineStateViewer::exportHTML(QXmlStreamWriter &xml, VKPipe::ColorBlend &cb)
{
  xml.writeStartElement(lit("h3"));
  xml.writeCharacters(tr("Color Blend State"));
  xml.writeEndElement();

  QString blendConst = QFormatStr("%1, %2, %3, %4")
                           .arg(cb.blendConst[0], 0, 'f', 2)
                           .arg(cb.blendConst[1], 0, 'f', 2)
                           .arg(cb.blendConst[2], 0, 'f', 2)
                           .arg(cb.blendConst[3], 0, 'f', 2);

  m_Common.exportHTMLTable(
      xml, {tr("Alpha to Coverage"), tr("Alpha to One"), tr("Logic Op"), tr("Blend Constant")},
      {
          cb.alphaToCoverageEnable ? tr("Yes") : tr("No"),
          cb.alphaToOneEnable ? tr("Yes") : tr("No"),
          cb.logicOpEnable ? ToQStr(cb.logic) : tr("Disabled"), blendConst,
      });

  xml.writeStartElement(lit("h3"));
  xml.writeCharacters(tr("Attachment Blends"));
  xml.writeEndElement();

  QList<QVariantList> rows;

  int i = 0;
  for(const VKPipe::Blend &b : cb.attachments)
  {
    rows.push_back(
        {i, b.blendEnable ? tr("Yes") : tr("No"), ToQStr(b.blend.Source), ToQStr(b.blend.Destination),
         ToQStr(b.blend.Operation), ToQStr(b.alphaBlend.Source), ToQStr(b.alphaBlend.Destination),
         ToQStr(b.alphaBlend.Operation), ((b.writeMask & 0x1) == 0 ? lit("_") : lit("R")) +
                                             ((b.writeMask & 0x2) == 0 ? lit("_") : lit("G")) +
                                             ((b.writeMask & 0x4) == 0 ? lit("_") : lit("B")) +
                                             ((b.writeMask & 0x8) == 0 ? lit("_") : lit("A"))});

    i++;
  }

  m_Common.exportHTMLTable(
      xml,
      {
          tr("Slot"), tr("Blend Enable"), tr("Blend Source"), tr("Blend Destination"),
          tr("Blend Operation"), tr("Alpha Blend Source"), tr("Alpha Blend Destination"),
          tr("Alpha Blend Operation"), tr("Write Mask"),
      },
      rows);
}

void VulkanPipelineStateViewer::exportHTML(QXmlStreamWriter &xml, VKPipe::DepthStencil &ds)
{
  {
    xml.writeStartElement(lit("h3"));
    xml.writeCharacters(tr("Depth State"));
    xml.writeEndElement();

    m_Common.exportHTMLTable(
        xml, {tr("Depth Test Enable"), tr("Depth Writes Enable"), tr("Depth Function"),
              tr("Depth Bounds")},
        {
            ds.depthTestEnable ? tr("Yes") : tr("No"), ds.depthWriteEnable ? tr("Yes") : tr("No"),
            ToQStr(ds.depthCompareOp), ds.depthBoundsEnable
                                           ? QFormatStr("%1 - %2")
                                                 .arg(Formatter::Format(ds.minDepthBounds))
                                                 .arg(Formatter::Format(ds.maxDepthBounds))
                                           : tr("Disabled"),
        });
  }

  {
    xml.writeStartElement(lit("h3"));
    xml.writeCharacters(tr("Stencil State"));
    xml.writeEndElement();

    if(ds.stencilTestEnable)
    {
      QList<QVariantList> rows;

      rows.push_back({
          tr("Front"), Formatter::Format(ds.front.ref, true),
          Formatter::Format(ds.front.compareMask, true),
          Formatter::Format(ds.front.writeMask, true), ToQStr(ds.front.Func),
          ToQStr(ds.front.PassOp), ToQStr(ds.front.FailOp), ToQStr(ds.front.DepthFailOp),
      });

      rows.push_back({
          tr("back"), Formatter::Format(ds.back.ref, true),
          Formatter::Format(ds.back.compareMask, true), Formatter::Format(ds.back.writeMask, true),
          ToQStr(ds.back.Func), ToQStr(ds.back.PassOp), ToQStr(ds.back.FailOp),
          ToQStr(ds.back.DepthFailOp),
      });

      m_Common.exportHTMLTable(xml,
                               {tr("Face"), tr("Ref"), tr("Compare Mask"), tr("Write Mask"),
                                tr("Function"), tr("Pass Op"), tr("Fail Op"), tr("Depth Fail Op")},
                               rows);
    }
    else
    {
      xml.writeStartElement(lit("p"));
      xml.writeCharacters(tr("Disabled"));
      xml.writeEndElement();
    }
  }
}

void VulkanPipelineStateViewer::exportHTML(QXmlStreamWriter &xml, VKPipe::CurrentPass &pass)
{
  {
    xml.writeStartElement(lit("h3"));
    xml.writeCharacters(tr("Framebuffer"));
    xml.writeEndElement();

    m_Common.exportHTMLTable(
        xml, {tr("Width"), tr("Height"), tr("Layers")},
        {pass.framebuffer.width, pass.framebuffer.height, pass.framebuffer.layers});

    QList<QVariantList> rows;

    int i = 0;
    for(const VKPipe::Attachment &a : pass.framebuffer.attachments)
    {
      TextureDescription *tex = m_Ctx.GetTexture(a.img);

      QString name = tr("Image %1").arg(ToQStr(a.img));

      if(tex)
        name = ToQStr(tex->name);

      rows.push_back({i, name, a.baseMip, a.numMip, a.baseLayer, a.numLayer});

      i++;
    }

    m_Common.exportHTMLTable(xml,
                             {
                                 tr("Slot"), tr("Image"), tr("First mip"), tr("Number of mips"),
                                 tr("First array layer"), tr("Number of layers"),
                             },
                             rows);
  }

  {
    xml.writeStartElement(lit("h3"));
    xml.writeCharacters(tr("Render Pass"));
    xml.writeEndElement();

    if(pass.renderpass.inputAttachments.count > 0)
    {
      QList<QVariantList> inputs;

      for(int i = 0; i < pass.renderpass.inputAttachments.count; i++)
        inputs.push_back({pass.renderpass.inputAttachments[i]});

      m_Common.exportHTMLTable(xml,
                               {
                                   tr("Input Attachment"),
                               },
                               inputs);

      xml.writeStartElement(lit("p"));
      xml.writeEndElement();
    }

    if(pass.renderpass.colorAttachments.count > 0)
    {
      QList<QVariantList> colors;

      for(int i = 0; i < pass.renderpass.colorAttachments.count; i++)
        colors.push_back({pass.renderpass.colorAttachments[i]});

      m_Common.exportHTMLTable(xml,
                               {
                                   tr("Color Attachment"),
                               },
                               colors);

      xml.writeStartElement(lit("p"));
      xml.writeEndElement();
    }

    if(pass.renderpass.depthstencilAttachment >= 0)
    {
      xml.writeStartElement(lit("p"));
      xml.writeCharacters(
          tr("Depth-stencil Attachment: %1").arg(pass.renderpass.depthstencilAttachment));
      xml.writeEndElement();
    }
  }

  {
    xml.writeStartElement(lit("h3"));
    xml.writeCharacters(tr("Render Area"));
    xml.writeEndElement();

    m_Common.exportHTMLTable(
        xml, {tr("X"), tr("Y"), tr("Width"), tr("Height")},
        {pass.renderArea.x, pass.renderArea.y, pass.renderArea.width, pass.renderArea.height});
  }
}

void VulkanPipelineStateViewer::on_exportHTML_clicked()
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
        case 0:
          // VTX
          xml.writeStartElement(lit("h2"));
          xml.writeCharacters(tr("Input Assembly"));
          xml.writeEndElement();
          exportHTML(xml, m_Ctx.CurVulkanPipelineState().IA);

          xml.writeStartElement(lit("h2"));
          xml.writeCharacters(tr("Vertex Input"));
          xml.writeEndElement();
          exportHTML(xml, m_Ctx.CurVulkanPipelineState().VI);
          break;
        case 1: exportHTML(xml, m_Ctx.CurVulkanPipelineState().m_VS); break;
        case 2: exportHTML(xml, m_Ctx.CurVulkanPipelineState().m_TCS); break;
        case 3: exportHTML(xml, m_Ctx.CurVulkanPipelineState().m_TES); break;
        case 4: exportHTML(xml, m_Ctx.CurVulkanPipelineState().m_GS); break;
        case 5: exportHTML(xml, m_Ctx.CurVulkanPipelineState().RS); break;
        case 6: exportHTML(xml, m_Ctx.CurVulkanPipelineState().m_FS); break;
        case 7:
          // FB
          xml.writeStartElement(lit("h2"));
          xml.writeCharacters(tr("Color Blend"));
          xml.writeEndElement();
          exportHTML(xml, m_Ctx.CurVulkanPipelineState().CB);

          xml.writeStartElement(lit("h2"));
          xml.writeCharacters(tr("Depth Stencil"));
          xml.writeEndElement();
          exportHTML(xml, m_Ctx.CurVulkanPipelineState().DS);

          xml.writeStartElement(lit("h2"));
          xml.writeCharacters(tr("Current Pass"));
          xml.writeEndElement();
          exportHTML(xml, m_Ctx.CurVulkanPipelineState().Pass);
          break;
        case 8: exportHTML(xml, m_Ctx.CurVulkanPipelineState().m_CS); break;
      }

      xml.writeEndElement();

      stage++;
    }

    m_Common.endHTMLExport(xmlptr);
  }
}

void VulkanPipelineStateViewer::on_meshView_clicked()
{
  if(!m_Ctx.HasMeshPreview())
    m_Ctx.ShowMeshPreview();
  ToolWindowManager::raiseToolWindow(m_Ctx.GetMeshPreview()->Widget());
}
