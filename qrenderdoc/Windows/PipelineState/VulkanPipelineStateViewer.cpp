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
#include <QScrollBar>
#include "3rdparty/toolwindowmanager/ToolWindowManager.h"
#include "Windows/BufferViewer.h"
#include "Windows/ConstantBufferPreviewer.h"
#include "Windows/MainWindow.h"
#include "Windows/ShaderViewer.h"
#include "Windows/TextureViewer.h"
#include "ui_VulkanPipelineStateViewer.h"

Q_DECLARE_METATYPE(ResourceId);
Q_DECLARE_METATYPE(SamplerData);

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

struct CBufferTag
{
  CBufferTag() { slotIdx = arrayIdx = 0; }
  CBufferTag(uint32_t s, uint32_t i)
  {
    slotIdx = s;
    arrayIdx = i;
  }
  uint32_t slotIdx;
  uint32_t arrayIdx;
};

Q_DECLARE_METATYPE(CBufferTag);

struct BufferTag
{
  BufferTag()
  {
    rwRes = false;
    bindPoint = 0;
    offset = size = 0;
  }
  BufferTag(bool rw, uint32_t b, ResourceId id, uint64_t offs, uint64_t sz)
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

Q_DECLARE_METATYPE(BufferTag);

VulkanPipelineStateViewer::VulkanPipelineStateViewer(CaptureContext *ctx, QWidget *parent)
    : QFrame(parent), ui(new Ui::VulkanPipelineStateViewer), m_Ctx(ctx)
{
  ui->setupUi(this);

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
    QObject::connect(b, &RDLabel::clicked, this, &VulkanPipelineStateViewer::shaderView_clicked);

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

  addGridLines(ui->rasterizerGridLayout);
  addGridLines(ui->MSAAGridLayout);
  addGridLines(ui->blendStateGridLayout);
  addGridLines(ui->depthStateGridLayout);

  // no way to set this up in the UI :(
  {
    // Index | Name | Location | Binding | Format | Offset | Go
    ui->viAttrs->header()->resizeSection(0, 75);
    ui->viAttrs->header()->setSectionResizeMode(0, QHeaderView::Interactive);
    ui->viAttrs->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    ui->viAttrs->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    ui->viAttrs->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    ui->viAttrs->header()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    ui->viAttrs->header()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    ui->viAttrs->header()->setSectionResizeMode(6, QHeaderView::ResizeToContents);

    ui->viAttrs->setHoverIconColumn(6);
  }

  {
    // Slot | Buffer | Rate | Offset | Stride | Byte Length | Go
    ui->viBuffers->header()->resizeSection(0, 75);
    ui->viBuffers->header()->setSectionResizeMode(0, QHeaderView::Interactive);
    ui->viBuffers->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    ui->viBuffers->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    ui->viBuffers->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    ui->viBuffers->header()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    ui->viBuffers->header()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    ui->viBuffers->header()->setSectionResizeMode(6, QHeaderView::ResizeToContents);

    ui->viBuffers->setHoverIconColumn(6);
  }

  for(RDTreeWidget *res : resources)
  {
    // <Tree> | Set | Binding | Type | Resource | Contents | cont.d | Go
    res->header()->resizeSection(0, 30);
    res->header()->setSectionResizeMode(0, QHeaderView::Fixed);
    res->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    res->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    res->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    res->header()->setSectionResizeMode(4, QHeaderView::Stretch);
    res->header()->setSectionResizeMode(5, QHeaderView::Stretch);
    res->header()->setSectionResizeMode(6, QHeaderView::Stretch);
    res->header()->setSectionResizeMode(7, QHeaderView::ResizeToContents);

    res->setHoverIconColumn(7);
    res->setDefaultHoverColor(ui->framebuffer->palette().color(QPalette::Window));
  }

  for(RDTreeWidget *ubo : ubos)
  {
    // <Tree> | Set | Binding | Buffer | Byte Range | Size | Go
    ubo->header()->resizeSection(0, 30);
    ubo->header()->setSectionResizeMode(0, QHeaderView::Fixed);
    ubo->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    ubo->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    ubo->header()->setSectionResizeMode(3, QHeaderView::Stretch);
    ubo->header()->setSectionResizeMode(4, QHeaderView::Stretch);
    ubo->header()->setSectionResizeMode(5, QHeaderView::Stretch);
    ubo->header()->setSectionResizeMode(6, QHeaderView::ResizeToContents);

    ubo->setHoverIconColumn(6);
    ubo->setDefaultHoverColor(ui->framebuffer->palette().color(QPalette::Window));
  }

  {
    // Slot | X | Y | Width | Height | MinDepth | MaxDepth
    ui->viewports->header()->resizeSection(0, 75);
    ui->viewports->header()->setSectionResizeMode(0, QHeaderView::Interactive);
    ui->viewports->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    ui->viewports->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    ui->viewports->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    ui->viewports->header()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    ui->viewports->header()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    ui->viewports->header()->setSectionResizeMode(6, QHeaderView::ResizeToContents);
  }

  {
    // Slot | X | Y | Width | Height
    ui->scissors->header()->resizeSection(0, 100);
    ui->scissors->header()->setSectionResizeMode(0, QHeaderView::Interactive);
    ui->scissors->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    ui->scissors->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    ui->scissors->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    ui->scissors->header()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
  }

  {
    // Slot | Resource | Type | Width | Height | Depth | Array Size | Format | Go
    ui->framebuffer->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    ui->framebuffer->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    ui->framebuffer->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    ui->framebuffer->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    ui->framebuffer->header()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    ui->framebuffer->header()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    ui->framebuffer->header()->setSectionResizeMode(6, QHeaderView::ResizeToContents);
    ui->framebuffer->header()->setSectionResizeMode(7, QHeaderView::ResizeToContents);
    ui->framebuffer->header()->setSectionResizeMode(8, QHeaderView::ResizeToContents);

    ui->framebuffer->setHoverIconColumn(8);
    ui->framebuffer->setDefaultHoverColor(ui->framebuffer->palette().color(QPalette::Window));
  }

  {
    // Slot | Enabled | Col Src | Col Dst | Col Op | Alpha Src | Alpha Dst | Alpha Op | Write Mask
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
  }

  {
    // Face | Func | Fail Op | Depth Fail Op | Pass Op | Write Mask | Comp Mask | Ref
    ui->stencils->header()->resizeSection(0, 50);
    ui->stencils->header()->setSectionResizeMode(0, QHeaderView::Interactive);
    ui->stencils->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    ui->stencils->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    ui->stencils->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    ui->stencils->header()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    ui->stencils->header()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    ui->stencils->header()->setSectionResizeMode(6, QHeaderView::ResizeToContents);
    ui->stencils->header()->setSectionResizeMode(7, QHeaderView::Stretch);
  }

  // this is often changed just because we're changing some tab in the designer.
  ui->stagesTabs->setCurrentIndex(0);

  // reset everything back to defaults
  clearState();
}

VulkanPipelineStateViewer::~VulkanPipelineStateViewer()
{
  delete ui;
}

void VulkanPipelineStateViewer::OnLogfileLoaded()
{
  OnEventChanged(m_Ctx->CurEvent());
}

void VulkanPipelineStateViewer::OnLogfileClosed()
{
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

void VulkanPipelineStateViewer::setInactiveRow(QTreeWidgetItem *node)
{
  for(int i = 0; i < node->columnCount(); i++)
  {
    QFont f = node->font(i);
    f.setItalic(true);
    node->setFont(i, f);
  }
}

void VulkanPipelineStateViewer::setEmptyRow(QTreeWidgetItem *node)
{
  for(int i = 0; i < node->columnCount(); i++)
    node->setBackgroundColor(i, QColor(255, 70, 70));
}

template <typename bindType>
void VulkanPipelineStateViewer::setViewDetails(QTreeWidgetItem *node, const bindType &view,
                                               FetchTexture *tex)
{
  if(tex == NULL)
    return;

  QString text;

  bool viewdetails = false;

  {
    for(const VulkanPipelineState::ImageData &im : m_Ctx->CurVulkanPipelineState.images)
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

  for(int i = 0; i < node->columnCount(); i++)
  {
    node->setToolTip(i, text);

    if(viewdetails)
    {
      node->setBackgroundColor(i, QColor(127, 255, 212));
      node->setForeground(i, QBrush(QColor(0, 0, 0)));
    }
  }
}

template <typename bindType>
void VulkanPipelineStateViewer::setViewDetails(QTreeWidgetItem *node, const bindType &view,
                                               FetchBuffer *buf)
{
  if(buf == NULL)
    return;

  QString text;

  if(view.offset > 0 || view.size < buf->length)
  {
    text += tr("The view covers bytes %1-%2.\nThe buffer is %3 bytes in length.")
                .arg(view.offset)
                .arg(view.size)
                .arg(buf->length);
  }
  else
  {
    return;
  }

  for(int i = 0; i < node->columnCount(); i++)
  {
    node->setToolTip(i, text);
    node->setBackgroundColor(i, QColor(127, 255, 212));
    node->setForeground(i, QBrush(QColor(0, 0, 0)));
  }
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

const VulkanPipelineState::ShaderStage *VulkanPipelineStateViewer::stageForSender(QWidget *widget)
{
  if(!m_Ctx->LogLoaded())
    return NULL;

  while(widget)
  {
    if(widget == ui->stagesTabs->widget(0))
      return &m_Ctx->CurVulkanPipelineState.VS;
    if(widget == ui->stagesTabs->widget(1))
      return &m_Ctx->CurVulkanPipelineState.VS;
    if(widget == ui->stagesTabs->widget(2))
      return &m_Ctx->CurVulkanPipelineState.TCS;
    if(widget == ui->stagesTabs->widget(3))
      return &m_Ctx->CurVulkanPipelineState.TES;
    if(widget == ui->stagesTabs->widget(4))
      return &m_Ctx->CurVulkanPipelineState.GS;
    if(widget == ui->stagesTabs->widget(5))
      return &m_Ctx->CurVulkanPipelineState.FS;
    if(widget == ui->stagesTabs->widget(6))
      return &m_Ctx->CurVulkanPipelineState.FS;
    if(widget == ui->stagesTabs->widget(7))
      return &m_Ctx->CurVulkanPipelineState.FS;
    if(widget == ui->stagesTabs->widget(8))
      return &m_Ctx->CurVulkanPipelineState.CS;

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
  ui->topology->setText("");
  ui->primRestart->setVisible(false);
  ui->topologyDiagram->setPixmap(QPixmap());

  clearShaderState(ui->vsShader, ui->vsResources, ui->vsUBOs);
  clearShaderState(ui->tcsShader, ui->tcsResources, ui->tcsUBOs);
  clearShaderState(ui->tesShader, ui->tesResources, ui->tesUBOs);
  clearShaderState(ui->gsShader, ui->gsResources, ui->gsUBOs);
  clearShaderState(ui->fsShader, ui->fsResources, ui->fsUBOs);
  clearShaderState(ui->csShader, ui->csResources, ui->csUBOs);

  QPixmap tick(QString::fromUtf8(":/Resources/tick.png"));

  ui->fillMode->setText(tr("Solid", "Fill Mode"));
  ui->cullMode->setText(tr("Front", "Cull Mode"));
  ui->frontCCW->setPixmap(tick);

  ui->depthBias->setText("0.0");
  ui->depthBiasClamp->setText("0.0");
  ui->slopeScaledBias->setText("0.0");

  ui->depthClamp->setPixmap(tick);
  ui->rasterizerDiscard->setPixmap(tick);
  ui->lineWidth->setText("1.0");

  ui->sampleCount->setText("1");
  ui->sampleShading->setPixmap(tick);
  ui->minSampleShading->setText("0.0");
  ui->sampleMask->setText("FFFFFFFF");

  ui->viewports->clear();
  ui->scissors->clear();

  ui->framebuffer->clear();
  ui->blends->clear();

  ui->blendFactor->setText("0.00, 0.00, 0.00, 0.00");
  ui->logicOp->setText("-");
  ui->alphaToOne->setPixmap(tick);

  ui->depthEnabled->setPixmap(tick);
  ui->depthFunc->setText("GREATER_EQUAL");
  ui->depthWrite->setPixmap(tick);

  ui->depthBounds->setText("0.0-1.0");
  ui->depthBounds->setPixmap(QPixmap());

  ui->stencils->clear();
}

QVariantList VulkanPipelineStateViewer::makeSampler(
    const QString &bindset, const QString &slotname,
    const VulkanPipelineState::Pipeline::DescriptorSet::DescriptorBinding::BindingElement &descriptor)
{
  QString addressing = "";
  QString addPrefix = "";
  QString addVal = "";

  QString filter = "";
  QString filtPrefix = "";
  QString filtVal = "";

  QString addr[] = {ToQStr(descriptor.addrU), ToQStr(descriptor.addrV), ToQStr(descriptor.addrW)};

  // arrange like either UVW: WRAP or UV: WRAP, W: CLAMP
  for(int a = 0; a < 3; a++)
  {
    QString prefix = QChar("UVW"[a]);

    if(a == 0 || addr[a] == addr[a - 1])
    {
      addPrefix += prefix;
    }
    else
    {
      addressing += addPrefix + ": " + addVal + ", ";

      addPrefix = prefix;
    }
    addVal = addr[a];
  }

  addressing += addPrefix + ": " + addVal;

  if(descriptor.borderEnable)
    addressing += " " + ToQStr(descriptor.border);

  if(descriptor.unnormalized)
    addressing += " (Un-norm)";

  QString filters[] = {ToQStr(descriptor.min), ToQStr(descriptor.mag), ToQStr(descriptor.mip)};
  QString filterPrefixes[] = {"Min", "Mag", "Mip"};

  // arrange as addressing above
  for(int a = 0; a < 3; a++)
  {
    if(a == 0 || filters[a] == filters[a - 1])
    {
      if(filtPrefix != "")
        filtPrefix += "/";
      filtPrefix += filterPrefixes[a];
    }
    else
    {
      filter += filtPrefix + ": " + filtVal + ", ";

      filtPrefix = filterPrefixes[a];
    }
    filtVal = filters[a];
  }

  filter += filtPrefix + ": " + filtVal;

  if(descriptor.maxAniso > 1.0f)
    filter += QString(" Aniso %1x").arg(descriptor.maxAniso);

  if(descriptor.compareEnable)
    filter += QString(" (%1)").arg(ToQStr(descriptor.comparison));

  QString lod = "LODs: " +
                (descriptor.minlod == -FLT_MAX ? "0" : QString::number(descriptor.minlod)) + " - " +
                (descriptor.maxlod == FLT_MAX ? "FLT_MAX" : QString::number(descriptor.maxlod));

  if(descriptor.mipBias != 0.0f)
    lod += QString(" Bias %1").arg(descriptor.mipBias);

  return {"",
          bindset,
          slotname,
          descriptor.immutableSampler ? "Immutable Sampler" : "Sampler",
          ToQStr(descriptor.SamplerName),
          addressing,
          filter + ", " + lod};
}

void VulkanPipelineStateViewer::addResourceRow(ShaderReflection *shaderDetails,
                                               const VulkanPipelineState::ShaderStage &stage,
                                               int bindset, int bind,
                                               const VulkanPipelineState::Pipeline &pipe,
                                               RDTreeWidget *resources,
                                               QMap<ResourceId, SamplerData> &samplers)
{
  const ShaderResource *shaderRes = NULL;
  const BindpointMap *bindMap = NULL;

  QIcon action(QPixmap(QString::fromUtf8(":/Resources/action.png")));
  QIcon action_hover(QPixmap(QString::fromUtf8(":/Resources/action_hover.png")));

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

  const rdctype::array<VulkanPipelineState::Pipeline::DescriptorSet::DescriptorBinding::BindingElement>
      *slotBinds = NULL;
  ShaderBindType bindType = eBindType_Unknown;
  ShaderStageBits stageBits = (ShaderStageBits)0;

  if(bindset < pipe.DescSets.count && bind < pipe.DescSets[bindset].bindings.count)
  {
    slotBinds = &pipe.DescSets[bindset].bindings[bind].binds;
    bindType = pipe.DescSets[bindset].bindings[bind].type;
    stageBits = pipe.DescSets[bindset].bindings[bind].stageFlags;
  }
  else
  {
    if(shaderRes->IsSampler)
      bindType = eBindType_Sampler;
    else if(shaderRes->IsSampler && shaderRes->IsTexture)
      bindType = eBindType_ImageSampler;
    else if(shaderRes->resType == eResType_Buffer)
      bindType = eBindType_ReadOnlyTBuffer;
    else
      bindType = eBindType_ReadOnlyImage;
  }

  bool usedSlot = bindMap != NULL && bindMap->used;
  bool stageBitsIncluded = stageBits & ((ShaderStageBits)(1 << (int)stage.stage));

  // skip descriptors that aren't for this shader stage
  if(!usedSlot && !stageBitsIncluded)
    return;

  if(bindType == eBindType_ConstantBuffer)
    return;

  // TODO - check compatibility between bindType and shaderRes.resType ?

  // consider it filled if any array element is filled
  bool filledSlot = false;
  for(int idx = 0; slotBinds != NULL && idx < slotBinds->count; idx++)
  {
    filledSlot |= (*slotBinds)[idx].res != ResourceId();
    if(bindType == eBindType_Sampler || bindType == eBindType_ImageSampler)
      filledSlot |= (*slotBinds)[idx].sampler != ResourceId();
  }

  // if it's masked out by stage bits, act as if it's not filled, so it's marked in red
  if(!stageBitsIncluded)
    filledSlot = false;

  if(showNode(usedSlot, filledSlot))
  {
    QTreeWidgetItem *parentNode = resources->invisibleRootItem();

    QString setname = QString::number(bindset);

    QString slotname = QString::number(bind);
    if(shaderRes != NULL && shaderRes->name.count > 0)
      slotname += ": " + ToQStr(shaderRes->name);

    int arrayLength = 0;
    if(slotBinds != NULL)
      arrayLength = slotBinds->count;
    else
      arrayLength = (int)bindMap->arraySize;

    // for arrays, add a parent element that we add the real cbuffers below
    if(arrayLength > 1)
    {
      QTreeWidgetItem *node =
          makeTreeNode({"", setname, slotname, tr("Array[%1]").arg(arrayLength), "", "", "", ""});

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
      const VulkanPipelineState::Pipeline::DescriptorSet::DescriptorBinding::BindingElement *descriptorBind =
          NULL;
      if(slotBinds != NULL)
        descriptorBind = &(*slotBinds)[idx];

      if(arrayLength > 1)
      {
        if(shaderRes != NULL && shaderRes->name.count > 0)
          slotname = QString("%1[%2]: %3").arg(bind).arg(idx).arg(ToQStr(shaderRes->name));
        else
          slotname = QString("%1[%2]").arg(bind).arg(idx);
      }

      bool isbuf = false;
      uint32_t w = 1, h = 1, d = 1;
      uint32_t a = 1;
      uint32_t samples = 1;
      uint64_t len = 0;
      QString format = "Unknown";
      QString name = "Empty";
      ShaderResourceType restype = eResType_None;
      QVariant tag;

      FetchTexture *tex = NULL;
      FetchBuffer *buf = NULL;

      if(filledSlot && descriptorBind != NULL)
      {
        name = "Object " + ToQStr(descriptorBind->res);

        format = ToQStr(descriptorBind->viewfmt.strname);

        // check to see if it's a texture
        tex = m_Ctx->GetTexture(descriptorBind->res);
        if(tex)
        {
          w = tex->width;
          h = tex->height;
          d = tex->depth;
          a = tex->arraysize;
          name = tex->name;
          restype = tex->resType;
          samples = tex->msSamp;

          tag = QVariant::fromValue(descriptorBind->res);
        }

        // if not a texture, it must be a buffer
        buf = m_Ctx->GetBuffer(descriptorBind->res);
        if(buf)
        {
          len = buf->length;
          w = 0;
          h = 0;
          d = 0;
          a = 0;
          name = buf->name;
          restype = eResType_Buffer;

          ulong descriptorLen = descriptorBind->size;

          if(descriptorLen == 0xFFFFFFFFFFFFFFFFULL)
            descriptorLen = len - descriptorBind->offset;

          tag = QVariant::fromValue(
              BufferTag(isrw, bindPoint, buf->ID, descriptorBind->offset, descriptorLen));

          isbuf = true;
        }
      }
      else
      {
        name = "Empty";
        format = "-";
        w = h = d = a = 0;
      }

      QTreeWidgetItem *node = NULL;
      QTreeWidgetItem *samplerNode = NULL;

      if(bindType == eBindType_ReadWriteBuffer || bindType == eBindType_ReadOnlyTBuffer ||
         bindType == eBindType_ReadWriteTBuffer)
      {
        if(!isbuf)
        {
          node = makeTreeNode({
              "", bindset, slotname, ToQStr(bindType), "-", "-", "",
          });

          setEmptyRow(node);
        }
        else
        {
          QString range = "-";
          if(descriptorBind != NULL)
            range = QString("%1 - %2").arg(descriptorBind->offset).arg(descriptorBind->size);

          node = makeTreeNode({
              "", bindset, slotname, ToQStr(bindType), name, QString("%1 bytes").arg(len), range,
          });

          resources->setHoverIcons(node, action, action_hover);

          node->setData(0, Qt::UserRole, tag);

          if(!filledSlot)
            setEmptyRow(node);

          if(!usedSlot)
            setInactiveRow(node);
        }
      }
      else if(bindType == eBindType_Sampler)
      {
        if(descriptorBind == NULL || descriptorBind->sampler == ResourceId())
        {
          node = makeTreeNode({
              "", bindset, slotname, ToQStr(bindType), "-", "-", "",
          });

          setEmptyRow(node);
        }
        else
        {
          node = makeTreeNode(makeSampler(QString::number(bindset), slotname, *descriptorBind));

          if(!filledSlot)
            setEmptyRow(node);

          if(!usedSlot)
            setInactiveRow(node);

          SamplerData sampData;
          sampData.node = node;
          node->setData(0, Qt::UserRole, QVariant::fromValue(sampData));

          if(!samplers.contains(descriptorBind->sampler))
            samplers.insert(descriptorBind->sampler, sampData);
        }
      }
      else
      {
        if(descriptorBind == NULL || descriptorBind->res == ResourceId())
        {
          node = makeTreeNode({
              "", bindset, slotname, ToQStr(bindType), "-", "-", "",
          });

          setEmptyRow(node);
        }
        else
        {
          QString typeName = ToQStr(restype) + " " + ToQStr(bindType);

          QString dim;

          if(restype == eResType_Texture3D)
            dim = QString("%1x%2x%3").arg(w).arg(h).arg(d);
          else if(restype == eResType_Texture1D || restype == eResType_Texture1DArray)
            dim = QString::number(w);
          else
            dim = QString("%1x%2").arg(w).arg(h);

          if(descriptorBind->swizzle[0] != eSwizzle_Red ||
             descriptorBind->swizzle[1] != eSwizzle_Green ||
             descriptorBind->swizzle[2] != eSwizzle_Blue ||
             descriptorBind->swizzle[3] != eSwizzle_Alpha)
          {
            format += tr(" swizzle[%1%2%3%4]")
                          .arg(ToQStr(descriptorBind->swizzle[0]))
                          .arg(ToQStr(descriptorBind->swizzle[1]))
                          .arg(ToQStr(descriptorBind->swizzle[2]))
                          .arg(ToQStr(descriptorBind->swizzle[3]));
          }

          if(restype == eResType_Texture1DArray || restype == eResType_Texture2DArray ||
             restype == eResType_Texture2DMSArray || restype == eResType_TextureCubeArray)
          {
            dim += QString(" %1[%2]").arg(ToQStr(restype)).arg(a);
          }

          if(restype == eResType_Texture2DMS || restype == eResType_Texture2DMSArray)
            dim += QString(", %1x MSAA").arg(samples);

          node = makeTreeNode({
              "", bindset, slotname, typeName, name, dim, format,
          });

          resources->setHoverIcons(node, action, action_hover);

          node->setData(0, Qt::UserRole, tag);

          if(!filledSlot)
            setEmptyRow(node);

          if(!usedSlot)
            setInactiveRow(node);
        }

        if(bindType == eBindType_ImageSampler)
        {
          if(descriptorBind == NULL || descriptorBind->sampler == ResourceId())
          {
            samplerNode = makeTreeNode({
                "", bindset, slotname, bindType, "-", "-", "",
            });

            setEmptyRow(node);
          }
          else
          {
            if(!samplers.contains(descriptorBind->sampler))
            {
              samplerNode = makeTreeNode(makeSampler("", "", *descriptorBind));

              if(!filledSlot)
                setEmptyRow(samplerNode);

              if(!usedSlot)
                setInactiveRow(samplerNode);

              SamplerData sampData;
              sampData.node = samplerNode;
              samplerNode->setData(0, Qt::UserRole, QVariant::fromValue(sampData));

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
                                                    const VulkanPipelineState::ShaderStage &stage,
                                                    int bindset, int bind,
                                                    const VulkanPipelineState::Pipeline &pipe,
                                                    RDTreeWidget *ubos)
{
  const ConstantBlock *cblock = NULL;
  const BindpointMap *bindMap = NULL;

  QIcon action(QPixmap(QString::fromUtf8(":/Resources/action.png")));
  QIcon action_hover(QPixmap(QString::fromUtf8(":/Resources/action_hover.png")));

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

  const rdctype::array<VulkanPipelineState::Pipeline::DescriptorSet::DescriptorBinding::BindingElement>
      *slotBinds = NULL;
  ShaderBindType bindType = eBindType_ConstantBuffer;
  ShaderStageBits stageBits = (ShaderStageBits)0;

  if(bindset < pipe.DescSets.count && bind < pipe.DescSets[bindset].bindings.count)
  {
    slotBinds = &pipe.DescSets[bindset].bindings[bind].binds;
    bindType = pipe.DescSets[bindset].bindings[bind].type;
    stageBits = pipe.DescSets[bindset].bindings[bind].stageFlags;
  }

  bool usedSlot = bindMap != NULL && bindMap->used;
  bool stageBitsIncluded = stageBits & ((ShaderStageBits)(1 << (int)stage.stage));

  // skip descriptors that aren't for this shader stage
  if(!usedSlot && !stageBitsIncluded)
    return;

  if(bindType != eBindType_ConstantBuffer)
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
    QTreeWidgetItem *parentNode = ubos->invisibleRootItem();

    QString setname = QString::number(bindset);

    QString slotname = QString::number(bind);
    if(cblock != NULL && cblock->name.count > 0)
      slotname += ": " + ToQStr(cblock->name);

    int arrayLength = 0;
    if(slotBinds != NULL)
      arrayLength = slotBinds->count;
    else
      arrayLength = (int)bindMap->arraySize;

    // for arrays, add a parent element that we add the real cbuffers below
    if(arrayLength > 1)
    {
      QTreeWidgetItem *node =
          makeTreeNode({"", setname, slotname, tr("Array[%1]").arg(arrayLength), "", ""});

      if(!filledSlot)
        setEmptyRow(node);

      if(!usedSlot)
        setInactiveRow(node);

      parentNode = node;

      ubos->showColumn(0);
    }

    for(int idx = 0; idx < arrayLength; idx++)
    {
      const VulkanPipelineState::Pipeline::DescriptorSet::DescriptorBinding::BindingElement *descriptorBind =
          NULL;
      if(slotBinds != NULL)
        descriptorBind = &(*slotBinds)[idx];

      if(arrayLength > 1)
      {
        if(cblock != NULL && cblock->name.count > 0)
          slotname = QString("%1[%2]: %3").arg(bind).arg(idx).arg(ToQStr(cblock->name));
        else
          slotname = QString("%1[%2]").arg(bind).arg(idx);
      }

      QString name = "Empty";
      uint64_t length = 0;
      int numvars = cblock != NULL ? cblock->variables.count : 0;
      uint64_t byteSize = cblock != NULL ? cblock->byteSize : 0;

      QString vecrange = "-";

      if(filledSlot && descriptorBind != NULL)
      {
        name = "";
        length = descriptorBind->size;

        FetchBuffer *buf = m_Ctx->GetBuffer(descriptorBind->res);
        if(buf)
        {
          name = buf->name;
          if(length == 0xFFFFFFFFFFFFFFFFULL)
            length = buf->length - descriptorBind->offset;
        }

        if(name == "")
          name = "UBO " + ToQStr(descriptorBind->res);

        vecrange =
            QString("%1 - %2").arg(descriptorBind->offset).arg(descriptorBind->offset + length);
      }

      QString sizestr;

      // push constants or specialization constants
      if(cblock != NULL && !cblock->bufferBacked)
      {
        setname = "";
        slotname = cblock->name;
        name = "Push constants";
        vecrange = "";
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

      QTreeWidgetItem *node = makeTreeNode({"", setname, slotname, name, vecrange, sizestr});

      ubos->setHoverIcons(node, action, action_hover);

      node->setData(0, Qt::UserRole, QVariant::fromValue(CBufferTag(slot, (uint)idx)));

      if(!filledSlot)
        setEmptyRow(node);

      if(!usedSlot)
        setInactiveRow(node);

      parentNode->addChild(node);
    }
  }
}

void VulkanPipelineStateViewer::setShaderState(const VulkanPipelineState::ShaderStage &stage,
                                               const VulkanPipelineState::Pipeline &pipe,
                                               QLabel *shader, RDTreeWidget *resources,
                                               RDTreeWidget *ubos)
{
  ShaderReflection *shaderDetails = stage.ShaderDetails;

  QIcon action(QPixmap(QString::fromUtf8(":/Resources/action.png")));
  QIcon action_hover(QPixmap(QString::fromUtf8(":/Resources/action_hover.png")));

  if(stage.Shader == ResourceId())
    shader->setText(tr("Unbound Shader"));
  else
    shader->setText(ToQStr(stage.ShaderName));

  if(shaderDetails != NULL && shaderDetails->DebugInfo.entryFunc.count > 0)
  {
    QString entryFunc = ToQStr(shaderDetails->DebugInfo.entryFunc);
    if(shaderDetails->DebugInfo.files.count > 0 || entryFunc != "main")
      shader->setText(entryFunc + "()");

    if(shaderDetails->DebugInfo.files.count > 0)
    {
      QString shaderfn = "";

      int entryFile = shaderDetails->DebugInfo.entryFile;
      if(entryFile < 0 || entryFile >= shaderDetails->DebugInfo.files.count)
        entryFile = 0;

      shaderfn = ToQStr(shaderDetails->DebugInfo.files[entryFile].first);

      shader->setText(entryFunc + "() - " + shaderfn);
    }
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

        QTreeWidgetItem *node = makeTreeNode({"", "", ToQStr(cblock.name), "Push constants", "",
                                              tr("%1 Variable(s)", "", cblock.variables.count)});

        ubos->setHoverIcons(node, action, action_hover);
        node->setData(0, Qt::UserRole, QVariant::fromValue(CBufferTag(cb, 0)));

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
  if(!m_Ctx->LogLoaded())
  {
    clearState();
    return;
  }

  m_CombinedImageSamplers.clear();

  const VulkanPipelineState &state = m_Ctx->CurVulkanPipelineState;
  const FetchDrawcall *draw = m_Ctx->CurDrawcall();

  bool showDisabled = ui->showDisabled->isChecked();
  bool showEmpty = ui->showEmpty->isChecked();

  QPixmap tick(QString::fromUtf8(":/Resources/tick.png"));
  QPixmap cross(QString::fromUtf8(":/Resources/cross.png"));

  QIcon action(QPixmap(QString::fromUtf8(":/Resources/action.png")));
  QIcon action_hover(QPixmap(QString::fromUtf8(":/Resources/action_hover.png")));

  bool usedBindings[128] = {};

  ////////////////////////////////////////////////
  // Vertex Input

  int vs = 0;

  vs = ui->viAttrs->verticalScrollBar()->value();
  ui->viAttrs->setUpdatesEnabled(false);
  ui->viAttrs->clear();
  {
    int i = 0;
    for(const VulkanPipelineState::VertexInput::Attribute &a : state.VI.attrs)
    {
      bool filledSlot = true;
      bool usedSlot = false;

      QString name = tr("Attribute %1").arg(i);

      if(state.VS.Shader != ResourceId())
      {
        int attrib = -1;
        if((int32_t)a.location < state.VS.BindpointMapping.InputAttributes.count)
          attrib = state.VS.BindpointMapping.InputAttributes[a.location];

        if(attrib >= 0 && attrib < state.VS.ShaderDetails->InputSig.count)
        {
          name = state.VS.ShaderDetails->InputSig[attrib].varName;
          usedSlot = true;
        }
      }

      if(showNode(usedSlot, filledSlot))
      {
        QTreeWidgetItem *node =
            makeTreeNode({i, name, a.location, a.binding, ToQStr(a.format.strname), a.byteoffset});

        usedBindings[a.binding] = true;

        ui->viAttrs->setHoverIcons(node, action, action_hover);

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

  PrimitiveTopology topo = draw != NULL ? draw->topology : eTopology_Unknown;

  if(topo > eTopology_PatchList)
  {
    int numCPs = (int)topo - (int)eTopology_PatchList_1CPs + 1;

    ui->topology->setText(QString("PatchList (%1 Control Points)").arg(numCPs));
  }
  else
  {
    ui->topology->setText(ToQStr(topo));
  }

  ui->primRestart->setVisible(state.IA.primitiveRestartEnable);

  switch(topo)
  {
    case eTopology_PointList:
      ui->topologyDiagram->setPixmap(
          QPixmap(QString::fromUtf8(":/Resources/topologies/topo_pointlist.png")));
      break;
    case eTopology_LineList:
      ui->topologyDiagram->setPixmap(
          QPixmap(QString::fromUtf8(":/Resources/topologies/topo_linelist.png")));
      break;
    case eTopology_LineStrip:
      ui->topologyDiagram->setPixmap(
          QPixmap(QString::fromUtf8(":/Resources/topologies/topo_linestrip.png")));
      break;
    case eTopology_TriangleList:
      ui->topologyDiagram->setPixmap(
          QPixmap(QString::fromUtf8(":/Resources/topologies/topo_trilist.png")));
      break;
    case eTopology_TriangleStrip:
      ui->topologyDiagram->setPixmap(
          QPixmap(QString::fromUtf8(":/Resources/topologies/topo_tristrip.png")));
      break;
    case eTopology_LineList_Adj:
      ui->topologyDiagram->setPixmap(
          QPixmap(QString::fromUtf8(":/Resources/topologies/topo_linelist_adj.png")));
      break;
    case eTopology_LineStrip_Adj:
      ui->topologyDiagram->setPixmap(
          QPixmap(QString::fromUtf8(":/Resources/topologies/topo_linestrip_adj.png")));
      break;
    case eTopology_TriangleList_Adj:
      ui->topologyDiagram->setPixmap(
          QPixmap(QString::fromUtf8(":/Resources/topologies/topo_trilist_adj.png")));
      break;
    case eTopology_TriangleStrip_Adj:
      ui->topologyDiagram->setPixmap(
          QPixmap(QString::fromUtf8(":/Resources/topologies/topo_tristrip_adj.png")));
      break;
    default:
      ui->topologyDiagram->setPixmap(
          QPixmap(QString::fromUtf8(":/Resources/topologies/topo_patch.png")));
      break;
  }

  vs = ui->viBuffers->verticalScrollBar()->value();
  ui->viBuffers->setUpdatesEnabled(false);
  ui->viBuffers->clear();

  bool ibufferUsed = draw != NULL && (draw->flags & eDraw_UseIBuffer);

  if(state.IA.ibuffer.buf != ResourceId())
  {
    if(ibufferUsed || showDisabled)
    {
      QString name = "Buffer " + ToQStr(state.IA.ibuffer.buf);
      uint64_t length = 1;

      if(!ibufferUsed)
        length = 0;

      FetchBuffer *buf = m_Ctx->GetBuffer(state.IA.ibuffer.buf);

      if(buf)
      {
        name = buf->name;
        length = buf->length;
      }

      QTreeWidgetItem *node = makeTreeNode({"Index", name, "Index", (qulonglong)state.IA.ibuffer.offs,
                                            draw->indexByteWidth, (qulonglong)length, ""});

      ui->viBuffers->setHoverIcons(node, action, action_hover);

      node->setData(
          0, Qt::UserRole,
          QVariant::fromValue(VBIBTag(state.IA.ibuffer.buf, draw != NULL ? draw->indexOffset : 0)));

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
      QTreeWidgetItem *node =
          makeTreeNode({"Index", tr("No Buffer Set"), "Index", "-", "-", "-", ""});

      ui->viBuffers->setHoverIcons(node, action, action_hover);

      node->setData(
          0, Qt::UserRole,
          QVariant::fromValue(VBIBTag(state.IA.ibuffer.buf, draw != NULL ? draw->indexOffset : 0)));

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
      const VulkanPipelineState::VertexInput::VertexBuffer *vbuff =
          (i < state.VI.vbuffers.count ? &state.VI.vbuffers[i] : NULL);
      const VulkanPipelineState::VertexInput::Binding *bind = NULL;

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
        QString rate = "-";
        uint64_t length = 1;
        uint64_t offset = 0;
        uint32_t stride = 0;

        if(vbuff != NULL)
        {
          name = "Buffer " + ToQStr(vbuff->buffer);
          offset = vbuff->offset;

          FetchBuffer *buf = m_Ctx->GetBuffer(vbuff->buffer);
          if(buf)
          {
            name = buf->name;
            length = buf->length;
          }
        }

        if(bind != NULL)
        {
          stride = bind->bytestride;
          rate = bind->perInstance ? "Instance" : "Vertex";
        }
        else
        {
          name += ", No Binding";
        }

        QTreeWidgetItem *node = NULL;

        if(filledSlot)
          node = makeTreeNode({i, name, rate, (qulonglong)offset, stride, (qulonglong)length, ""});
        else
          node = makeTreeNode({i, tr("No Binding"), "-", "-", "-", "-", ""});

        ui->viBuffers->setHoverIcons(node, action, action_hover);

        node->setData(0, Qt::UserRole,
                      QVariant::fromValue(VBIBTag(vbuff != NULL ? vbuff->buffer : ResourceId(),
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
        QTreeWidgetItem *node = makeTreeNode({i, tr("No Binding"), "-", "-", "-", "-", ""});

        ui->viBuffers->setHoverIcons(node, action, action_hover);

        node->setData(0, Qt::UserRole, QVariant::fromValue(VBIBTag(ResourceId(), 0)));

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

  setShaderState(state.VS, state.graphics, ui->vsShader, ui->vsResources, ui->vsUBOs);
  setShaderState(state.GS, state.graphics, ui->gsShader, ui->gsResources, ui->gsUBOs);
  setShaderState(state.TCS, state.graphics, ui->tcsShader, ui->tcsResources, ui->tcsUBOs);
  setShaderState(state.TES, state.graphics, ui->tesShader, ui->tesResources, ui->tesUBOs);
  setShaderState(state.FS, state.graphics, ui->fsShader, ui->fsResources, ui->fsUBOs);
  setShaderState(state.CS, state.compute, ui->csShader, ui->csResources, ui->csUBOs);

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
        makeTreeNode({"Render Area", state.Pass.renderArea.x, state.Pass.renderArea.y,
                      state.Pass.renderArea.width, state.Pass.renderArea.height}));
  }

  {
    int i = 0;
    for(const VulkanPipelineState::ViewState::ViewportScissor &v : state.VP.viewportScissors)
    {
      QTreeWidgetItem *node =
          makeTreeNode({i, v.vp.x, v.vp.y, v.vp.width, v.vp.height, v.vp.minDepth, v.vp.maxDepth});
      ui->viewports->addTopLevelItem(node);

      if(v.vp.width == 0 || v.vp.height == 0)
        setEmptyRow(node);

      node = makeTreeNode({i, v.scissor.x, v.scissor.y, v.scissor.width, v.scissor.height});
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

  ui->fillMode->setText(ToQStr(state.RS.FillMode));
  ui->cullMode->setText(ToQStr(state.RS.CullMode));
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
  ui->sampleMask->setText(QString("%1").arg(state.MSAA.sampleMask, 8, 16, QChar('0')));

  ////////////////////////////////////////////////
  // Output Merger

  bool targets[32] = {};

  vs = ui->framebuffer->verticalScrollBar()->value();
  ui->framebuffer->setUpdatesEnabled(false);
  ui->framebuffer->clear();
  {
    int i = 0;
    for(const VulkanPipelineState::CurrentPass::Framebuffer::Attachment &p :
        state.Pass.framebuffer.attachments)
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

      bool filledSlot = (p.img != ResourceId());
      bool usedSlot = (colIdx >= 0 || state.Pass.renderpass.depthstencilAttachment == i);

      if(showNode(usedSlot, filledSlot))
      {
        uint32_t w = 1, h = 1, d = 1;
        uint32_t a = 1;
        QString format = ToQStr(p.viewfmt.strname);
        QString name = "Texture " + ToQStr(p.img);
        QString typeName = "Unknown";

        if(p.img == ResourceId())
        {
          name = "Empty";
          format = "-";
          typeName = "-";
          w = h = d = a = 0;
        }

        FetchTexture *tex = m_Ctx->GetTexture(p.img);
        if(tex)
        {
          w = tex->width;
          h = tex->height;
          d = tex->depth;
          a = tex->arraysize;
          name = tex->name;
          typeName = ToQStr(tex->resType);

          if(!tex->customName && state.FS.ShaderDetails != NULL)
          {
            for(int s = 0; s < state.FS.ShaderDetails->OutputSig.count; s++)
            {
              if(state.FS.ShaderDetails->OutputSig[s].regIndex == (uint32_t)colIdx &&
                 (state.FS.ShaderDetails->OutputSig[s].systemValue == eAttr_None ||
                  state.FS.ShaderDetails->OutputSig[s].systemValue == eAttr_ColourOutput))
              {
                name = QString("<%1>").arg(ToQStr(state.FS.ShaderDetails->OutputSig[s].varName));
              }
            }
          }
        }

        if(p.swizzle[0] != eSwizzle_Red || p.swizzle[1] != eSwizzle_Green ||
           p.swizzle[2] != eSwizzle_Blue || p.swizzle[3] != eSwizzle_Alpha)
        {
          format += tr(" swizzle[%1%2%3%4]")
                        .arg(ToQStr(p.swizzle[0]))
                        .arg(ToQStr(p.swizzle[1]))
                        .arg(ToQStr(p.swizzle[2]))
                        .arg(ToQStr(p.swizzle[3]));
        }

        QTreeWidgetItem *node = makeTreeNode({i, name, typeName, w, h, d, a, format, ""});

        ui->framebuffer->setHoverIcons(node, action, action_hover);

        if(tex)
          node->setData(0, Qt::UserRole, QVariant::fromValue(p.img));

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
    for(const VulkanPipelineState::ColorBlend::Attachment &blend : state.CB.attachments)
    {
      bool filledSlot = true;
      bool usedSlot = (targets[i]);

      if(showNode(usedSlot, filledSlot))
      {
        QTreeWidgetItem *node =
            makeTreeNode({i, blend.blendEnable,

                          ToQStr(blend.blend.Source), ToQStr(blend.blend.Destination),
                          ToQStr(blend.blend.Operation),

                          ToQStr(blend.alphaBlend.Source), ToQStr(blend.alphaBlend.Destination),
                          ToQStr(blend.alphaBlend.Operation),

                          QString("%1%2%3%4")
                              .arg((blend.writeMask & 0x1) == 0 ? "_" : "R")
                              .arg((blend.writeMask & 0x2) == 0 ? "_" : "G")
                              .arg((blend.writeMask & 0x4) == 0 ? "_" : "B")
                              .arg((blend.writeMask & 0x8) == 0 ? "_" : "A")});

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

  ui->blendFactor->setText(QString("%1, %2, %3, %4")
                               .arg(state.CB.blendConst[0], 2)
                               .arg(state.CB.blendConst[1], 2)
                               .arg(state.CB.blendConst[2], 2)
                               .arg(state.CB.blendConst[3], 2));
  ui->logicOp->setText(state.CB.logicOpEnable ? ToQStr(state.CB.logicOp) : "-");
  ui->alphaToOne->setPixmap(state.CB.alphaToOneEnable ? tick : cross);

  ui->depthEnabled->setPixmap(state.DS.depthTestEnable ? tick : cross);
  ui->depthFunc->setText(ToQStr(state.DS.depthCompareOp));
  ui->depthWrite->setPixmap(state.DS.depthWriteEnable ? tick : cross);

  if(state.DS.depthBoundsEnable)
  {
    ui->depthBounds->setText(Formatter::Format(state.DS.minDepthBounds) + "-" +
                             Formatter::Format(state.DS.maxDepthBounds));
    ui->depthBounds->setPixmap(QPixmap());
  }
  else
  {
    ui->depthBounds->setText("");
    ui->depthBounds->setPixmap(cross);
  }

  ui->stencils->setUpdatesEnabled(false);
  ui->stencils->clear();
  if(state.DS.stencilTestEnable)
  {
    ui->stencils->addTopLevelItems(
        {makeTreeNode({"Front", ToQStr(state.DS.front.func), ToQStr(state.DS.front.failOp),
                       ToQStr(state.DS.front.depthFailOp), ToQStr(state.DS.front.passOp),
                       QString("%1").arg(state.DS.front.writeMask, 2, 16, QChar('0')),
                       QString("%1").arg(state.DS.front.compareMask, 2, 16, QChar('0')),
                       QString("%1").arg(state.DS.front.ref, 2, 16, QChar('0'))}),
         makeTreeNode({"Back", ToQStr(state.DS.back.func), ToQStr(state.DS.back.failOp),
                       ToQStr(state.DS.back.depthFailOp), ToQStr(state.DS.back.passOp),
                       QString("%1").arg(state.DS.back.writeMask, 2, 16, QChar('0')),
                       QString("%1").arg(state.DS.back.compareMask, 2, 16, QChar('0')),
                       QString("%1").arg(state.DS.back.ref, 2, 16, QChar('0'))})});
  }
  else
  {
    ui->stencils->addTopLevelItems({makeTreeNode({"Front", "-", "-", "-", "-", "-", "-", "-"}),
                                    makeTreeNode({"Back", "-", "-", "-", "-", "-", "-", "-"})});
  }
  ui->stencils->clearSelection();
  ui->stencils->setUpdatesEnabled(true);

// highlight the appropriate stages in the flowchart
#if 0
  if(draw == null)
  {
    pipeFlow.SetStagesEnabled(new bool[] { true, true, true, true, true, true, true, true, true });
  }
  else if((draw.flags & DrawcallFlags.Dispatch) != 0)
  {
    pipeFlow.SetStagesEnabled(new bool[] { false, false, false, false, false, false, false, false, true });
  }
  else
  {
    pipeFlow.SetStagesEnabled(new bool[] {
      true,
        true,
        state.TCS.Shader != ResourceId(),
        state.TES.Shader != ResourceId(),
        state.GS.Shader != ResourceId(),
        true,
        state.FS.Shader != ResourceId(),
        true,
        false
    });

    // if(streamout only)
    //{
    //    pipeFlow.Rasterizer = false;
    //    pipeFlow.OutputMerger = false;
    //}
  }
#endif
}

QString VulkanPipelineStateViewer::formatMembers(int indent, const QString &nameprefix,
                                                 const rdctype::array<ShaderConstant> &vars)
{
  QString indentstr(indent * 4, QChar(' '));

  QString ret = "";

  int i = 0;

  for(const ShaderConstant &v : vars)
  {
    if(v.type.members.count > 0)
    {
      if(i > 0)
        ret += "\n";
      ret += indentstr + QString("// struct %1\n").arg(ToQStr(v.type.descriptor.name));
      ret += indentstr + "{\n" + formatMembers(indent + 1, ToQStr(v.name) + "_", v.type.members) +
             indentstr + "}\n";
      if(i < vars.count - 1)
        ret += "\n";
    }
    else
    {
      QString arr = "";
      if(v.type.descriptor.elements > 1)
        arr = QString("[%1]").arg(v.type.descriptor.elements);
      ret += indentstr + ToQStr(v.type.descriptor.name) + " " + nameprefix + v.name + arr + ";\n";
    }

    i++;
  }

  return ret;
}

void VulkanPipelineStateViewer::resource_itemActivated(QTreeWidgetItem *item, int column)
{
  const VulkanPipelineState::ShaderStage *stage = stageForSender(item->treeWidget());

  if(stage == NULL)
    return;

  QVariant tag = item->data(0, Qt::UserRole);

  if(tag.canConvert<ResourceId>())
  {
    FetchTexture *tex = m_Ctx->GetTexture(tag.value<ResourceId>());

    if(tex)
    {
      if(tex->resType == eResType_Buffer)
      {
        // TODO Buffer viewer
        // var viewer = new BufferViewer(m_Core, false);
        // viewer.ViewRawBuffer(false, 0, ulong.MaxValue, tex.ID);
        // viewer.Show(m_DockContent.DockPanel);
      }
      else
      {
        if(!m_Ctx->hasTextureViewer())
          m_Ctx->showTextureViewer();
        TextureViewer *viewer = m_Ctx->textureViewer();
        viewer->ViewTexture(tex->ID, true);
      }

      return;
    }
  }
  else if(tag.canConvert<BufferTag>())
  {
    BufferTag buf = tag.value<BufferTag>();

    const ShaderResource &shaderRes = buf.rwRes
                                          ? stage->ShaderDetails->ReadWriteResources[buf.bindPoint]
                                          : stage->ShaderDetails->ReadOnlyResources[buf.bindPoint];

    QString format = QString("// struct %1\n").arg(ToQStr(shaderRes.variableType.descriptor.name));

    if(shaderRes.variableType.members.count > 1)
    {
      format += "// members skipped as they are fixed size:\n";
      for(int i = 0; i < shaderRes.variableType.members.count - 1; i++)
        format += QString("%1 %2;\n")
                      .arg(ToQStr(shaderRes.variableType.members[i].type.descriptor.name))
                      .arg(ToQStr(shaderRes.variableType.members[i].name));
    }

    if(shaderRes.variableType.members.count > 0)
    {
      format +=
          "{\n" + formatMembers(1, "", shaderRes.variableType.members.back().type.members) + "}";
    }
    else
    {
      const auto &desc = shaderRes.variableType.descriptor;

      format = "";
      if(desc.rowMajorStorage)
        format += "row_major ";

      format += ToQStr(desc.type);
      if(desc.rows > 1 && desc.cols > 1)
        format += QString("%1x%2").arg(desc.rows).arg(desc.cols);
      else if(desc.cols > 1)
        format += desc.cols;

      if(desc.name.count > 0)
        format += " " + ToQStr(desc.name);

      if(desc.elements > 1)
        format += QString("[%1]").arg(desc.elements);
    }

    if(buf.ID != ResourceId())
    {
      // TODO Buffer viewer
      // var viewer = new BufferViewer(m_Core, false);
      // viewer.ViewRawBuffer(true, buf.offset, buf.size, buf.ID, format);
      // viewer.Show(m_DockContent.DockPanel);
    }
  }
}

void VulkanPipelineStateViewer::ubo_itemActivated(QTreeWidgetItem *item, int column)
{
  const VulkanPipelineState::ShaderStage *stage = stageForSender(item->treeWidget());

  if(stage == NULL)
    return;

  QVariant tag = item->data(0, Qt::UserRole);

  if(!tag.canConvert<CBufferTag>())
    return;

  CBufferTag cb = tag.value<CBufferTag>();

  ConstantBufferPreviewer *existing =
      ConstantBufferPreviewer::has(stage->stage, cb.slotIdx, cb.arrayIdx);
  if(existing != NULL)
  {
    ToolWindowManager::raiseToolWindow(existing);
    return;
  }

  ConstantBufferPreviewer *prev =
      new ConstantBufferPreviewer(m_Ctx, stage->stage, cb.slotIdx, cb.arrayIdx, m_Ctx->mainWindow());

  m_Ctx->setupDockWindow(prev);

  ToolWindowManager *manager = ToolWindowManager::managerOf(this);

  ToolWindowManager::AreaReference ref(ToolWindowManager::RightOf, manager->areaOf(this), 0.3f);
  manager->addToolWindow(prev, ref);
}

void VulkanPipelineStateViewer::on_viAttrs_itemActivated(QTreeWidgetItem *item, int column)
{
  on_meshView_clicked();
}

void VulkanPipelineStateViewer::on_viBuffers_itemActivated(QTreeWidgetItem *item, int column)
{
  QVariant tag = item->data(0, Qt::UserRole);

  if(tag.canConvert<VBIBTag>())
  {
    VBIBTag buf = tag.value<VBIBTag>();

    if(buf.id != ResourceId())
    {
      // TODO Buffer Viewer
      // var viewer = new BufferViewer(m_Core, false);
      // viewer.ViewRawBuffer(true, buf.offset, ulong.MaxValue, buf.id);
      // viewer.Show(m_DockContent.DockPanel);
    }
  }
}

void VulkanPipelineStateViewer::highlightIABind(int slot)
{
  int idx = ((slot + 1) * 21) % 32;    // space neighbouring colours reasonably distinctly

  const VulkanPipelineState::VertexInput &VI = m_Ctx->CurVulkanPipelineState.VI;

  QColor col = QColor::fromHslF(float(idx) / 32.0f, 1.0f, 0.95f);

  if(slot < m_VBNodes.count())
  {
    QTreeWidgetItem *item = m_VBNodes[(int)slot];

    for(int c = 0; c < item->columnCount(); c++)
      item->setBackground(c, QBrush(col));
  }

  if(slot < m_BindNodes.count())
  {
    QTreeWidgetItem *item = m_BindNodes[(int)slot];

    for(int c = 0; c < item->columnCount(); c++)
      item->setBackground(c, QBrush(col));
  }

  for(int i = 0; i < ui->viAttrs->topLevelItemCount(); i++)
  {
    QTreeWidgetItem *item = ui->viAttrs->topLevelItem(i);

    QBrush itemBrush = QBrush(col);

    if((int)VI.attrs[i].binding != slot)
      itemBrush = QBrush();

    for(int c = 0; c < item->columnCount(); c++)
      item->setBackground(c, itemBrush);
  }
}

void VulkanPipelineStateViewer::on_viAttrs_mouseMove(QMouseEvent *e)
{
  if(!m_Ctx->LogLoaded())
    return;

  QModelIndex idx = ui->viAttrs->indexAt(e->pos());

  vertex_leave(NULL);

  const VulkanPipelineState::VertexInput &VI = m_Ctx->CurVulkanPipelineState.VI;

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
  if(!m_Ctx->LogLoaded())
    return;

  QTreeWidgetItem *item = ui->viBuffers->itemAt(e->pos());

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
      for(int c = 0; c < item->columnCount(); c++)
        item->setBackground(c, QBrush(ui->viBuffers->palette().color(QPalette::Window)));
    }
  }
}

void VulkanPipelineStateViewer::vertex_leave(QEvent *e)
{
  for(int i = 0; i < ui->viAttrs->topLevelItemCount(); i++)
  {
    QTreeWidgetItem *item = ui->viAttrs->topLevelItem(i);
    for(int c = 0; c < item->columnCount(); c++)
      item->setBackground(c, QBrush());
  }

  for(int i = 0; i < ui->viBuffers->topLevelItemCount(); i++)
  {
    QTreeWidgetItem *item = ui->viBuffers->topLevelItem(i);
    for(int c = 0; c < item->columnCount(); c++)
      item->setBackground(c, QBrush());
  }
}

void VulkanPipelineStateViewer::shaderView_clicked()
{
  const VulkanPipelineState::ShaderStage *stage =
      stageForSender(qobject_cast<QWidget *>(QObject::sender()));

  if(stage == NULL || stage->Shader == ResourceId())
    return;

  ShaderReflection *shaderDetails = stage->ShaderDetails;

  ShaderViewer *shad = new ShaderViewer(m_Ctx, shaderDetails, stage->stage, NULL, "");

  m_Ctx->setupDockWindow(shad);

  ToolWindowManager *manager = ToolWindowManager::managerOf(this);

  ToolWindowManager::AreaReference ref(ToolWindowManager::AddTo, manager->areaOf(this));
  manager->addToolWindow(shad, ref);
}

void VulkanPipelineStateViewer::shaderEdit_clicked()
{
}

void VulkanPipelineStateViewer::shaderSave_clicked()
{
  const VulkanPipelineState::ShaderStage *stage =
      stageForSender(qobject_cast<QWidget *>(QObject::sender()));

  if(stage == NULL)
    return;

  ShaderReflection *shaderDetails = stage->ShaderDetails;

  if(stage->Shader == ResourceId())
    return;

  QString filename =
      RDDialog::getSaveFileName(this, tr("Save Shader As"), QString(), "SPIR-V files (*.spv)");

  if(filename != "")
  {
    QDir dirinfo = QFileInfo(filename).dir();
    if(dirinfo.exists())
    {
      QFile f(filename);
      if(f.open(QIODevice::WriteOnly | QIODevice::Truncate))
      {
        f.write((const char *)shaderDetails->RawBytes.elems, (qint64)shaderDetails->RawBytes.count);
      }
      else
      {
        RDDialog::critical(
            this, tr("Error saving shader"),
            tr("Couldn't open path %1 for write.\n%2").arg(filename).arg(f.errorString()));
      }
    }
    else
    {
      RDDialog::critical(this, tr("Invalid directory"),
                         tr("Cannot find target directory to save to"));
    }
  }
}

void VulkanPipelineStateViewer::on_exportHTML_clicked()
{
}

void VulkanPipelineStateViewer::on_meshView_clicked()
{
  if(!m_Ctx->hasMeshPreview())
    m_Ctx->showMeshPreview();
  ToolWindowManager::raiseToolWindow(m_Ctx->meshPreview());
}
