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

#include "VulkanPipelineStateViewer.h"
#include <float.h>
#include <QMenu>
#include <QMouseEvent>
#include <QScrollBar>
#include <QXmlStreamWriter>
#include "3rdparty/toolwindowmanager/ToolWindowManager.h"
#include "Code/Resources.h"
#include "Widgets/Extended/RDHeaderView.h"
#include "PipelineStateViewer.h"
#include "ui_VulkanPipelineStateViewer.h"

Q_DECLARE_METATYPE(SamplerData);

struct VulkanVBIBTag
{
  VulkanVBIBTag() { offset = 0; }
  VulkanVBIBTag(ResourceId i, uint64_t offs, QString f = QString())
  {
    id = i;
    offset = offs;
    format = f;
  }

  ResourceId id;
  uint64_t offset;
  QString format;
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
  VulkanBufferTag(bool rw, uint32_t b, ResourceFormat f, ResourceId id, uint64_t offs, uint64_t sz)
  {
    rwRes = rw;
    bindPoint = b;
    ID = id;
    fmt = f;
    offset = offs;
    size = sz;
  }
  bool rwRes;
  uint32_t bindPoint;
  ResourceFormat fmt;
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

  QToolButton *viewPredicateBufferButtons[] = {
      ui->predicateBufferViewButton, ui->csPredicateBufferViewButton,
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
    b->setAutoFillBackground(true);
    b->setBackgroundRole(QPalette::ToolTipBase);
    b->setForegroundRole(QPalette::ToolTipText);
    b->setMinimumSizeHint(QSize(250, 0));
  }

  for(QToolButton *b : editButtons)
    QObject::connect(b, &QToolButton::clicked, &m_Common, &PipelineStateViewer::shaderEdit_clicked);

  for(QToolButton *b : saveButtons)
    QObject::connect(b, &QToolButton::clicked, this, &VulkanPipelineStateViewer::shaderSave_clicked);

  for(QToolButton *b : viewPredicateBufferButtons)
    QObject::connect(b, &QToolButton::clicked, this,
                     &VulkanPipelineStateViewer::predicateBufferView_clicked);

  QObject::connect(ui->viAttrs, &RDTreeWidget::leave, this, &VulkanPipelineStateViewer::vertex_leave);
  QObject::connect(ui->viBuffers, &RDTreeWidget::leave, this,
                   &VulkanPipelineStateViewer::vertex_leave);

  QObject::connect(ui->xfbBuffers, &RDTreeWidget::itemActivated, this,
                   &VulkanPipelineStateViewer::resource_itemActivated);

  QObject::connect(ui->fbAttach, &RDTreeWidget::itemActivated, this,
                   &VulkanPipelineStateViewer::resource_itemActivated);

  for(RDTreeWidget *res : resources)
    QObject::connect(res, &RDTreeWidget::itemActivated, this,
                     &VulkanPipelineStateViewer::resource_itemActivated);

  for(RDTreeWidget *ubo : ubos)
    QObject::connect(ubo, &RDTreeWidget::itemActivated, this,
                     &VulkanPipelineStateViewer::ubo_itemActivated);

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

    ui->viBuffers->setColumns({tr("Slot"), tr("Buffer"), tr("Rate"), tr("Divisor"), tr("Offset"),
                               tr("Stride"), tr("Byte Length"), tr("Go")});
    header->setColumnStretchHints({1, 4, 2, 2, 2, 2, 3, -1});

    ui->viBuffers->setHoverIconColumn(7, action, action_hover);
    ui->viBuffers->setClearSelectionOnFocusLoss(true);
    ui->viBuffers->setInstantTooltips(true);
  }

  for(RDTreeWidget *res : resources)
  {
    RDHeaderView *header = new RDHeaderView(Qt::Horizontal, this);
    res->setHeader(header);

    res->setColumns({QString(), tr("Set"), tr("Binding"), tr("Type"), tr("Resource"),
                     tr("Contents"), tr("Additional"), tr("Go")});
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
    ui->xfbBuffers->setHeader(header);

    ui->xfbBuffers->setColumns({tr("Slot"), tr("Active"), tr("Data Buffer"), tr("Byte Offset"),
                                tr("Byte Length"), tr("Written Count Buffer"),
                                tr("Written Count Offset"), tr("Go")});
    header->setColumnStretchHints({1, 1, 4, 2, 3, 4, 2, -1});
    header->setMinimumSectionSize(40);

    ui->xfbBuffers->setHoverIconColumn(7, action, action_hover);
    ui->xfbBuffers->setClearSelectionOnFocusLoss(true);
    ui->xfbBuffers->setInstantTooltips(true);
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
    ui->discards->setHeader(header);

    ui->discards->setColumns({tr("Slot"), tr("X"), tr("Y"), tr("Width"), tr("Height")});
    header->setColumnStretchHints({-1, -1, -1, -1, 1});
    header->setMinimumSectionSize(40);

    ui->discards->setClearSelectionOnFocusLoss(true);
    ui->discards->setInstantTooltips(true);
  }

  for(RDLabel *rp : {ui->renderpass, ui->framebuffer, ui->predicateBuffer, ui->csPredicateBuffer})
  {
    rp->setAutoFillBackground(true);
    rp->setBackgroundRole(QPalette::ToolTipBase);
    rp->setForegroundRole(QPalette::ToolTipText);
    rp->setMinimumSizeHint(QSize(250, 0));
  }

  {
    RDHeaderView *header = new RDHeaderView(Qt::Horizontal, this);
    ui->fbAttach->setHeader(header);

    ui->fbAttach->setColumns({tr("Slot"), tr("Resource"), tr("Type"), tr("Width"), tr("Height"),
                              tr("Depth"), tr("Array Size"), tr("Format"), tr("Go")});
    header->setColumnStretchHints({2, 4, 2, 1, 1, 1, 1, 3, -1});

    ui->fbAttach->setHoverIconColumn(8, action, action_hover);
    ui->fbAttach->setClearSelectionOnFocusLoss(true);
    ui->fbAttach->setInstantTooltips(true);
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
  ui->xfbBuffers->setFont(Formatter::PreferredFont());
  ui->viewports->setFont(Formatter::PreferredFont());
  ui->scissors->setFont(Formatter::PreferredFont());
  ui->renderpass->setFont(Formatter::PreferredFont());
  ui->framebuffer->setFont(Formatter::PreferredFont());
  ui->fbAttach->setFont(Formatter::PreferredFont());
  ui->blends->setFont(Formatter::PreferredFont());

  // reset everything back to defaults
  clearState();
}

VulkanPipelineStateViewer::~VulkanPipelineStateViewer()
{
  delete ui;
}

void VulkanPipelineStateViewer::OnCaptureLoaded()
{
  OnEventChanged(m_Ctx.CurEvent());
}

void VulkanPipelineStateViewer::OnCaptureClosed()
{
  ui->pipeFlow->setStagesEnabled({true, true, true, true, true, true, true, true, true});

  clearState();
}

void VulkanPipelineStateViewer::OnEventChanged(uint32_t eventId)
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
                                               TextureDescription *tex, bool includeSampleLocations)
{
  if(tex == NULL)
    return;

  QString text;

  bool viewdetails = false;

  const VKPipe::State &state = *m_Ctx.CurVulkanPipelineState();

  {
    for(const VKPipe::ImageData &im : state.images)
    {
      if(im.resourceId == tex->resourceId)
      {
        text += tr("Texture is in the '%1' layout\n\n").arg(im.layouts[0].name);
        break;
      }
    }

    if(view.viewFormat != tex->format)
    {
      text += tr("The texture is format %1, the view treats it as %2.\n")
                  .arg(tex->format.Name())
                  .arg(view.viewFormat.Name());

      viewdetails = true;
    }

    if(tex->mips > 1 && (tex->mips != view.numMips || view.firstMip > 0))
    {
      if(view.numMips == 1)
        text +=
            tr("The texture has %1 mips, the view covers mip %2.\n").arg(tex->mips).arg(view.firstMip);
      else
        text += tr("The texture has %1 mips, the view covers mips %2-%3.\n")
                    .arg(tex->mips)
                    .arg(view.firstMip)
                    .arg(view.firstMip + view.numMips - 1);

      viewdetails = true;
    }

    if(tex->arraysize > 1 && (tex->arraysize != view.numSlices || view.firstSlice > 0))
    {
      if(view.numSlices == 1)
        text += tr("The texture has %1 array slices, the view covers slice %2.\n")
                    .arg(tex->arraysize)
                    .arg(view.firstSlice);
      else
        text += tr("The texture has %1 array slices, the view covers slices %2-%3.\n")
                    .arg(tex->arraysize)
                    .arg(view.firstSlice)
                    .arg(view.firstSlice + view.numSlices);

      viewdetails = true;
    }
  }

  if(includeSampleLocations && state.multisample.rasterSamples > 1 &&
     !state.multisample.sampleLocations.customLocations.isEmpty())
  {
    text += tr("Rendering with custom sample locations over %1x%2 grid:\n")
                .arg(state.multisample.sampleLocations.gridWidth)
                .arg(state.multisample.sampleLocations.gridHeight);

    const rdcarray<FloatVector> &locations = state.multisample.sampleLocations.customLocations;

    for(int i = 0; i < locations.count(); i++)
    {
      text += QFormatStr("  [%1]: %2, %3\n")
                  .arg(i)
                  .arg(Formatter::Format(locations[i].x))
                  .arg(Formatter::Format(locations[i].y));
    }

    viewdetails = true;
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

  if(view.byteOffset > 0 || view.byteSize < buf->length)
  {
    text += tr("The view covers bytes %1-%2.\nThe buffer is %3 bytes in length.")
                .arg(view.byteOffset)
                .arg(view.byteOffset + view.byteSize)
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

QString VulkanPipelineStateViewer::formatByteRange(const BufferDescription *buf,
                                                   const VKPipe::BindingElement *descriptorBind)
{
  if(buf == NULL || descriptorBind == NULL)
    return lit("-");
  if(descriptorBind->byteSize == 0)
  {
    return tr("%1 - %2 (empty view)").arg(descriptorBind->byteOffset).arg(descriptorBind->byteOffset);
  }
  else if(descriptorBind->byteSize == UINT64_MAX)
  {
    return QFormatStr("%1 - %2 (VK_WHOLE_SIZE)")
        .arg(descriptorBind->byteOffset)
        .arg(descriptorBind->byteOffset + (buf->length - descriptorBind->byteOffset));
  }
  else
  {
    return QFormatStr("%1 - %2")
        .arg(descriptorBind->byteOffset)
        .arg(descriptorBind->byteOffset + descriptorBind->byteSize);
  }
}

const VKPipe::Shader *VulkanPipelineStateViewer::stageForSender(QWidget *widget)
{
  if(!m_Ctx.IsCaptureLoaded())
    return NULL;

  while(widget)
  {
    if(widget == ui->stagesTabs->widget(0))
      return &m_Ctx.CurVulkanPipelineState()->vertexShader;
    if(widget == ui->stagesTabs->widget(1))
      return &m_Ctx.CurVulkanPipelineState()->vertexShader;
    if(widget == ui->stagesTabs->widget(2))
      return &m_Ctx.CurVulkanPipelineState()->tessControlShader;
    if(widget == ui->stagesTabs->widget(3))
      return &m_Ctx.CurVulkanPipelineState()->tessEvalShader;
    if(widget == ui->stagesTabs->widget(4))
      return &m_Ctx.CurVulkanPipelineState()->geometryShader;
    if(widget == ui->stagesTabs->widget(5))
      return &m_Ctx.CurVulkanPipelineState()->fragmentShader;
    if(widget == ui->stagesTabs->widget(6))
      return &m_Ctx.CurVulkanPipelineState()->fragmentShader;
    if(widget == ui->stagesTabs->widget(7))
      return &m_Ctx.CurVulkanPipelineState()->fragmentShader;
    if(widget == ui->stagesTabs->widget(8))
      return &m_Ctx.CurVulkanPipelineState()->computeShader;

    widget = widget->parentWidget();
  }

  qCritical() << "Unrecognised control calling event handler";

  return NULL;
}

void VulkanPipelineStateViewer::clearShaderState(RDLabel *shader, RDTreeWidget *resources,
                                                 RDTreeWidget *cbuffers)
{
  shader->setText(QFormatStr("%1: %1").arg(ToQStr(ResourceId())));
  resources->clear();
  cbuffers->clear();
}

void VulkanPipelineStateViewer::clearState()
{
  m_VBNodes.clear();
  m_BindNodes.clear();
  m_EmptyNodes.clear();

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

  ui->depthBias->setText(lit("0.0"));
  ui->depthBiasClamp->setText(lit("0.0"));
  ui->slopeScaledBias->setText(lit("0.0"));

  ui->depthClamp->setPixmap(tick);
  ui->depthClip->setPixmap(cross);
  ui->rasterizerDiscard->setPixmap(tick);
  ui->lineWidth->setText(lit("1.0"));

  ui->conservativeRaster->setText(tr("Disabled"));
  ui->overestimationSize->setText(lit("0.0"));
  ui->multiview->setText(tr("Disabled"));

  ui->sampleCount->setText(lit("1"));
  ui->sampleShading->setPixmap(tick);
  ui->minSampleShading->setText(lit("0.0"));
  ui->sampleMask->setText(lit("FFFFFFFF"));

  ui->viewports->clear();
  ui->scissors->clear();
  ui->discards->clear();
  ui->discardMode->setText(tr("Inclusive"));
  ui->discardGroup->setVisible(false);

  ui->renderpass->setText(QFormatStr("Render Pass: %1").arg(ToQStr(ResourceId())));
  ui->framebuffer->setText(QFormatStr("Framebuffer: %1").arg(ToQStr(ResourceId())));

  ui->fbAttach->clear();
  ui->blends->clear();

  ui->blendFactor->setText(lit("0.00, 0.00, 0.00, 0.00"));
  ui->logicOp->setText(lit("-"));
  ui->alphaToOne->setPixmap(tick);

  ui->depthEnabled->setPixmap(tick);
  ui->depthFunc->setText(lit("GREATER_EQUAL"));
  ui->depthWrite->setPixmap(tick);

  ui->depthBounds->setPixmap(QPixmap());
  ui->depthBounds->setText(lit("0.0-1.0"));

  ui->stencils->clear();

  ui->conditionalRenderingGroup->setVisible(false);
  ui->csConditionalRenderingGroup->setVisible(false);
}

QVariantList VulkanPipelineStateViewer::makeSampler(const QString &bindset, const QString &slotname,
                                                    const VKPipe::BindingElement &descriptor)
{
  QString addressing;
  QString addPrefix;
  QString addVal;

  QString filter;

  QString addr[] = {ToQStr(descriptor.addressU), ToQStr(descriptor.addressV),
                    ToQStr(descriptor.addressW)};

  // arrange like either UVW: WRAP or UV: WRAP, W: CLAMP
  for(int a = 0; a < 3; a++)
  {
    const char *uvw = "UVW";
    QString prefix = QString(QLatin1Char(uvw[a]));

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
                      .arg(descriptor.borderColor[0])
                      .arg(descriptor.borderColor[1])
                      .arg(descriptor.borderColor[2])
                      .arg(descriptor.borderColor[3]);

  if(descriptor.unnormalized)
    addressing += lit(" (Un-norm)");

  filter = ToQStr(descriptor.filter);

  if(descriptor.maxAnisotropy > 1.0f)
    filter += lit(" Aniso %1x").arg(descriptor.maxAnisotropy);

  if(descriptor.filter.filter == FilterFunction::Comparison)
    filter += QFormatStr(" (%1)").arg(ToQStr(descriptor.compareFunction));
  else if(descriptor.filter.filter != FilterFunction::Normal)
    filter += QFormatStr(" (%1)").arg(ToQStr(descriptor.filter.filter));

  QString lod =
      lit("LODs: %1 - %2")
          .arg((descriptor.minLOD == -FLT_MAX ? lit("0") : QString::number(descriptor.minLOD)))
          .arg((descriptor.maxLOD == FLT_MAX ? lit("FLT_MAX") : QString::number(descriptor.maxLOD)));

  // omit lod clamp if this is an immutable sampler and the attached resource is entirely within the
  // range
  if(descriptor.immutableSampler)
  {
    TextureDescription *tex = m_Ctx.GetTexture(descriptor.resourceResourceId);
    if(tex && descriptor.minLOD <= 0.0f && descriptor.maxLOD >= (float)(tex->mips - 1))
    {
      lod = QString();
    }
  }

  if(descriptor.mipBias != 0.0f)
    lod += lit(" Bias %1").arg(descriptor.mipBias);

  if(!lod.isEmpty())
    lod = lit(", ") + lod;

  QString obj = ToQStr(descriptor.samplerResourceId);

  if(descriptor.ycbcrSampler != ResourceId())
  {
    obj += lit(" ") + ToQStr(descriptor.ycbcrSampler);

    if(descriptor.ycbcrSwizzle[0] != TextureSwizzle::Red ||
       descriptor.ycbcrSwizzle[1] != TextureSwizzle::Green ||
       descriptor.ycbcrSwizzle[2] != TextureSwizzle::Blue ||
       descriptor.ycbcrSwizzle[3] != TextureSwizzle::Alpha)
    {
      obj += tr(" swizzle[%1%2%3%4]")
                 .arg(ToQStr(descriptor.swizzle[0]))
                 .arg(ToQStr(descriptor.swizzle[1]))
                 .arg(ToQStr(descriptor.swizzle[2]))
                 .arg(ToQStr(descriptor.swizzle[3]));
    }

    filter +=
        QFormatStr(", %1 %2").arg(ToQStr(descriptor.ycbcrModel)).arg(ToQStr(descriptor.ycbcrRange));

    addressing += tr(", Chroma %1 [%2,%3]")
                      .arg(ToQStr(descriptor.chromaFilter))
                      .arg(ToQStr(descriptor.xChromaOffset))
                      .arg(ToQStr(descriptor.yChromaOffset));

    if(descriptor.forceExplicitReconstruction)
      addressing += tr(" Explicit");
  }

  return {QString(),    bindset,
          slotname,     descriptor.immutableSampler ? tr("Immutable Sampler") : tr("Sampler"),
          obj,          addressing,
          filter + lod, QString()};
}

void VulkanPipelineStateViewer::addResourceRow(ShaderReflection *shaderDetails,
                                               const VKPipe::Shader &stage, int bindset, int bind,
                                               const VKPipe::Pipeline &pipe, RDTreeWidget *resources,
                                               QMap<ResourceId, SamplerData> &samplers)
{
  const ShaderResource *shaderRes = NULL;
  const ShaderSampler *shaderSamp = NULL;
  const Bindpoint *bindMap = NULL;

  bool isrw = false;
  uint bindPoint = 0;

  if(shaderDetails != NULL)
  {
    // we find the matching binding for this set/binding.
    // The spec requires that there are no overlapping definitions, or if there are they have
    // compatible types so we can just pick the first one we come across.
    // The spec also doesn't require variables which are statically unused to have valid bindings,
    // so they may be overlapping or possibly just defaulted to 0.
    // Any variables with no binding declared at all were set to 0 and sorted to the end at
    // reflection time, so we can just use a single algorithm to select the best candidate:
    //
    // 1. Search for matching bindset/bind resources. It doesn't matter which 'namespace' (sampler/
    //    read-only/read-write) we search in, because if there's a conflict the behaviour is
    //    illegal and if there's no conflict we won't get any ambiguity.
    // 2. If we find a match, select it for use.
    // 3. If we find a second match, use it in preference only if the old one was !used, and the new
    //    one is used.
    //
    // This will make us select the best possible option - the first declared used resource
    // at a particular binding, ignoring any unused resources at that binding before/after. Or if
    // there's no used resource at all, the first declared unused resource (which will prefer
    // resources with proper bindings over those without, as with the sorting mentioned above).

    for(int i = 0; i < shaderDetails->samplers.count(); i++)
    {
      const ShaderSampler &s = shaderDetails->samplers[i];

      if(stage.bindpointMapping.samplers[s.bindPoint].bindset == bindset &&
         stage.bindpointMapping.samplers[s.bindPoint].bind == bind)
      {
        // use this one either if we have no candidate, or the candidate we have is unused and this
        // one is used
        if(bindMap == NULL || (!bindMap->used && stage.bindpointMapping.samplers[s.bindPoint].used))
        {
          bindPoint = (uint)i;
          shaderSamp = &s;
          bindMap = &stage.bindpointMapping.samplers[s.bindPoint];
        }
      }
    }

    for(int i = 0; i < shaderDetails->readOnlyResources.count(); i++)
    {
      const ShaderResource &ro = shaderDetails->readOnlyResources[i];

      if(stage.bindpointMapping.readOnlyResources[ro.bindPoint].bindset == bindset &&
         stage.bindpointMapping.readOnlyResources[ro.bindPoint].bind == bind)
      {
        // use this one either if we have no candidate, or the candidate we have is unused and this
        // one is used
        if(bindMap == NULL ||
           (!bindMap->used && stage.bindpointMapping.readOnlyResources[ro.bindPoint].used))
        {
          bindPoint = (uint)i;
          shaderRes = &ro;
          shaderSamp = NULL;
          bindMap = &stage.bindpointMapping.readOnlyResources[ro.bindPoint];
        }
      }
    }

    for(int i = 0; i < shaderDetails->readWriteResources.count(); i++)
    {
      const ShaderResource &rw = shaderDetails->readWriteResources[i];

      if(stage.bindpointMapping.readWriteResources[rw.bindPoint].bindset == bindset &&
         stage.bindpointMapping.readWriteResources[rw.bindPoint].bind == bind)
      {
        // use this one either if we have no candidate, or the candidate we have is unused and this
        // one is used
        if(bindMap == NULL ||
           (!bindMap->used && stage.bindpointMapping.readWriteResources[rw.bindPoint].used))
        {
          bindPoint = (uint)i;
          isrw = true;
          shaderRes = &rw;
          shaderSamp = NULL;
          bindMap = &stage.bindpointMapping.readWriteResources[rw.bindPoint];
        }
      }
    }
  }

  const rdcarray<VKPipe::BindingElement> *slotBinds = NULL;
  BindType bindType = BindType::Unknown;
  ShaderStageMask stageBits = ShaderStageMask::Unknown;
  bool pushDescriptor = false;
  uint32_t dynamicallyUsedCount = 1;

  if(bindset < pipe.descriptorSets.count() && bind < pipe.descriptorSets[bindset].bindings.count())
  {
    pushDescriptor = pipe.descriptorSets[bindset].pushDescriptor;
    dynamicallyUsedCount = pipe.descriptorSets[bindset].bindings[bind].dynamicallyUsedCount;
    slotBinds = &pipe.descriptorSets[bindset].bindings[bind].binds;
    bindType = pipe.descriptorSets[bindset].bindings[bind].type;
    stageBits = pipe.descriptorSets[bindset].bindings[bind].stageFlags;
  }
  else
  {
    if(shaderSamp)
      bindType = BindType::Sampler;
    else if(shaderRes && shaderRes->resType == TextureType::Buffer)
      bindType = isrw ? BindType::ReadWriteBuffer : BindType::ReadOnlyBuffer;
    else
      bindType = isrw ? BindType::ReadWriteImage : BindType::ReadOnlyImage;
  }

  bool usedSlot = bindMap != NULL && bindMap->used && dynamicallyUsedCount > 0;
  bool stageBitsIncluded = bool(stageBits & MaskForStage(stage.stage));

  // skip descriptors that aren't for this shader stage
  if(!usedSlot && !stageBitsIncluded)
    return;

  if(bindType == BindType::ConstantBuffer)
    return;

  // TODO - check compatibility between bindType and shaderRes.resType ?

  // consider it filled if any array element is filled
  bool filledSlot = false;
  for(int idx = 0; slotBinds != NULL && idx < slotBinds->count(); idx++)
  {
    filledSlot |= (*slotBinds)[idx].resourceResourceId != ResourceId();
    if(bindType == BindType::Sampler || bindType == BindType::ImageSampler)
      filledSlot |= (*slotBinds)[idx].samplerResourceId != ResourceId();
  }

  // if it's masked out by stage bits, act as if it's not filled, so it's marked in red
  if(!stageBitsIncluded)
    filledSlot = false;

  if(showNode(usedSlot, filledSlot))
  {
    RDTreeWidgetItem *parentNode = resources->invisibleRootItem();

    QString setname = QString::number(bindset);

    if(pushDescriptor)
      setname = tr("Push ") + setname;

    QString slotname = QString::number(bind);
    if(shaderRes && !shaderRes->name.isEmpty())
      slotname += lit(": ") + shaderRes->name;
    else if(shaderSamp && !shaderSamp->name.isEmpty())
      slotname += lit(": ") + shaderSamp->name;

    int arrayLength = 0;
    if(slotBinds != NULL)
      arrayLength = slotBinds->count();
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
      {
        descriptorBind = &(*slotBinds)[idx];

        if(!showNode(usedSlot && descriptorBind->dynamicallyUsed, filledSlot))
          continue;
      }

      if(arrayLength > 1)
      {
        if(shaderRes && !shaderRes->name.isEmpty())
          slotname = QFormatStr("%1[%2]: %3").arg(bind).arg(idx).arg(shaderRes->name);
        else if(shaderSamp && !shaderSamp->name.isEmpty())
          slotname = QFormatStr("%1[%2]: %3").arg(bind).arg(idx).arg(shaderSamp->name);
        else
          slotname = QFormatStr("%1[%2]").arg(bind).arg(idx);
      }

      bool isbuf = false;
      uint32_t w = 1, h = 1, d = 1;
      uint32_t a = 1;
      uint32_t samples = 1;
      uint64_t len = 0;
      QString format = tr("Unknown");
      TextureType restype = TextureType::Unknown;
      QVariant tag;

      TextureDescription *tex = NULL;
      BufferDescription *buf = NULL;

      uint64_t descriptorLen = descriptorBind ? descriptorBind->byteSize : 0;

      if(filledSlot && descriptorBind != NULL)
      {
        format = descriptorBind->viewFormat.Name();

        // check to see if it's a texture
        tex = m_Ctx.GetTexture(descriptorBind->resourceResourceId);
        if(tex)
        {
          w = tex->width;
          h = tex->height;
          d = tex->depth;
          a = tex->arraysize;
          restype = tex->type;
          samples = tex->msSamp;

          tag = QVariant::fromValue(descriptorBind->resourceResourceId);
        }

        // if not a texture, it must be a buffer
        buf = m_Ctx.GetBuffer(descriptorBind->resourceResourceId);
        if(buf)
        {
          len = buf->length;
          w = 0;
          h = 0;
          d = 0;
          a = 0;
          restype = TextureType::Buffer;

          if(descriptorLen == UINT64_MAX)
            descriptorLen = len - descriptorBind->byteOffset;

          tag = QVariant::fromValue(VulkanBufferTag(isrw, bindPoint, descriptorBind->viewFormat,
                                                    buf->resourceId, descriptorBind->byteOffset,
                                                    descriptorLen));

          isbuf = true;
        }
      }
      else
      {
        format = lit("-");
        w = h = d = a = 0;
      }

      RDTreeWidgetItem *node = NULL;
      RDTreeWidgetItem *samplerNode = NULL;

      if(bindType == BindType::ReadWriteBuffer)
      {
        if(!isbuf)
        {
          node = new RDTreeWidgetItem({
              QString(), setname, slotname, ToQStr(bindType), ResourceId(), lit("-"), QString(),
              QString(),
          });

          setEmptyRow(node);
        }
        else
        {
          node = new RDTreeWidgetItem({
              QString(), setname, slotname, ToQStr(bindType),
              descriptorBind ? descriptorBind->resourceResourceId : ResourceId(),
              tr("%1 bytes").arg(len),
              QFormatStr("Viewing bytes %1").arg(formatByteRange(buf, descriptorBind)), QString(),
          });

          node->setTag(tag);

          if(!filledSlot)
            setEmptyRow(node);

          if(!usedSlot)
            setInactiveRow(node);
        }
      }
      else if(bindType == BindType::ReadOnlyTBuffer || bindType == BindType::ReadWriteTBuffer)
      {
        node = new RDTreeWidgetItem({
            QString(), setname, slotname, ToQStr(bindType),
            descriptorBind ? descriptorBind->resourceResourceId : ResourceId(), format,
            QFormatStr("bytes %1").arg(formatByteRange(buf, descriptorBind)), QString(),
        });

        node->setTag(tag);

        if(!filledSlot)
          setEmptyRow(node);

        if(!usedSlot)
          setInactiveRow(node);
      }
      else if(bindType == BindType::Sampler)
      {
        if(descriptorBind == NULL || descriptorBind->samplerResourceId == ResourceId())
        {
          node = new RDTreeWidgetItem({
              QString(), setname, slotname, ToQStr(bindType), ResourceId(), lit("-"), QString(),
              QString(),
          });

          setEmptyRow(node);
        }
        else
        {
          node = new RDTreeWidgetItem(makeSampler(setname, slotname, *descriptorBind));

          if(!filledSlot)
            setEmptyRow(node);

          if(!usedSlot)
            setInactiveRow(node);

          SamplerData sampData;
          sampData.node = node;
          node->setTag(QVariant::fromValue(sampData));

          if(!samplers.contains(descriptorBind->samplerResourceId))
            samplers.insert(descriptorBind->samplerResourceId, sampData);
        }
      }
      else
      {
        if(descriptorBind == NULL || descriptorBind->resourceResourceId == ResourceId())
        {
          node = new RDTreeWidgetItem({
              QString(), setname, slotname, ToQStr(bindType), ResourceId(), lit("-"), QString(),
              QString(),
          });

          setEmptyRow(node);
        }
        else
        {
          QString typeName = ToQStr(restype) + lit(" ") + ToQStr(bindType);

          QString dim;

          if(restype == TextureType::Texture3D)
            dim = QFormatStr("%1x%2x%3").arg(w).arg(h).arg(d);
          else if(restype == TextureType::Texture1D || restype == TextureType::Texture1DArray)
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

          if(restype == TextureType::Texture1DArray || restype == TextureType::Texture2DArray ||
             restype == TextureType::Texture2DMSArray || restype == TextureType::TextureCubeArray)
          {
            dim += QFormatStr(" %1[%2]").arg(ToQStr(restype)).arg(a);
          }

          if(restype == TextureType::Texture2DMS || restype == TextureType::Texture2DMSArray)
            dim += QFormatStr(", %1x MSAA").arg(samples);

          node = new RDTreeWidgetItem({
              QString(), setname, slotname, typeName, descriptorBind->resourceResourceId, dim,
              format, QString(),
          });

          node->setTag(tag);

          if(!filledSlot)
            setEmptyRow(node);

          if(!usedSlot)
            setInactiveRow(node);

          if(bindType == BindType::ImageSampler)
          {
            if(descriptorBind == NULL || descriptorBind->samplerResourceId == ResourceId())
            {
              samplerNode = new RDTreeWidgetItem({
                  QString(), setname, slotname, ToQStr(bindType), ResourceId(), lit("-"), QString(),
                  QString(),
              });

              setEmptyRow(samplerNode);
            }
            else
            {
              if(!samplers.contains(descriptorBind->samplerResourceId))
              {
                samplerNode =
                    new RDTreeWidgetItem(makeSampler(QString(), QString(), *descriptorBind));

                if(!filledSlot)
                  setEmptyRow(samplerNode);

                if(!usedSlot)
                  setInactiveRow(samplerNode);

                SamplerData sampData;
                sampData.node = samplerNode;
                samplerNode->setTag(QVariant::fromValue(sampData));

                samplers.insert(descriptorBind->samplerResourceId, sampData);
              }

              if(node != NULL)
              {
                m_CombinedImageSamplers[node] = samplers[descriptorBind->samplerResourceId].node;
                samplers[descriptorBind->samplerResourceId].images.push_back(node);
              }
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
  const Bindpoint *bindMap = NULL;

  uint32_t slot = ~0U;
  if(shaderDetails != NULL)
  {
    for(slot = 0; slot < (uint)shaderDetails->constantBlocks.count(); slot++)
    {
      const ConstantBlock &cb = shaderDetails->constantBlocks[slot];
      if(stage.bindpointMapping.constantBlocks[cb.bindPoint].bindset == bindset &&
         stage.bindpointMapping.constantBlocks[cb.bindPoint].bind == bind)
      {
        cblock = &cb;
        bindMap = &stage.bindpointMapping.constantBlocks[cb.bindPoint];
        break;
      }
    }

    if(slot >= (uint)shaderDetails->constantBlocks.count())
      slot = ~0U;
  }

  const rdcarray<VKPipe::BindingElement> *slotBinds = NULL;
  BindType bindType = BindType::ConstantBuffer;
  ShaderStageMask stageBits = ShaderStageMask::Unknown;
  uint32_t dynamicallyUsedCount = 1;

  bool pushDescriptor = false;

  if(bindset < pipe.descriptorSets.count() && bind < pipe.descriptorSets[bindset].bindings.count())
  {
    pushDescriptor = pipe.descriptorSets[bindset].pushDescriptor;
    dynamicallyUsedCount = pipe.descriptorSets[bindset].bindings[bind].dynamicallyUsedCount;
    slotBinds = &pipe.descriptorSets[bindset].bindings[bind].binds;
    bindType = pipe.descriptorSets[bindset].bindings[bind].type;
    stageBits = pipe.descriptorSets[bindset].bindings[bind].stageFlags;
  }

  bool usedSlot = bindMap != NULL && bindMap->used && dynamicallyUsedCount > 0;
  bool stageBitsIncluded = bool(stageBits & MaskForStage(stage.stage));

  // skip descriptors that aren't for this shader stage
  if(!usedSlot && !stageBitsIncluded)
    return;

  if(bindType != BindType::ConstantBuffer)
    return;

  // consider it filled if any array element is filled (or it's push constants)
  bool filledSlot = cblock != NULL && !cblock->bufferBacked;
  for(int idx = 0; slotBinds != NULL && idx < slotBinds->count(); idx++)
    filledSlot |= (*slotBinds)[idx].resourceResourceId != ResourceId();

  // if it's masked out by stage bits, act as if it's not filled, so it's marked in red
  if(!stageBitsIncluded)
    filledSlot = false;

  if(showNode(usedSlot, filledSlot))
  {
    RDTreeWidgetItem *parentNode = ubos->invisibleRootItem();

    QString setname = QString::number(bindset);

    if(pushDescriptor)
      setname = tr("Push ") + setname;

    QString slotname = QString::number(bind);
    if(cblock != NULL && !cblock->name.isEmpty())
      slotname += lit(": ") + cblock->name;

    int arrayLength = 0;
    if(slotBinds != NULL)
      arrayLength = slotBinds->count();
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
      {
        descriptorBind = &(*slotBinds)[idx];

        if(!showNode(usedSlot && descriptorBind->dynamicallyUsed, filledSlot))
          continue;
      }

      if(arrayLength > 1)
      {
        if(cblock != NULL && !cblock->name.isEmpty())
          slotname = QFormatStr("%1[%2]: %3").arg(bind).arg(idx).arg(cblock->name);
        else
          slotname = QFormatStr("%1[%2]").arg(bind).arg(idx);
      }

      uint64_t length = 0;
      int numvars = cblock != NULL ? cblock->variables.count() : 0;
      uint64_t byteSize = cblock != NULL ? cblock->byteSize : 0;

      QString vecrange = lit("-");

      if(filledSlot && descriptorBind != NULL)
      {
        length = descriptorBind->byteSize;

        BufferDescription *buf = m_Ctx.GetBuffer(descriptorBind->resourceResourceId);
        if(buf && length == UINT64_MAX)
          length = buf->length - descriptorBind->byteOffset;

        vecrange = formatByteRange(buf, descriptorBind);
      }

      QString sizestr;

      QVariant name = descriptorBind ? descriptorBind->resourceResourceId : ResourceId();

      // push constants or specialization constants
      if(cblock != NULL && !cblock->bufferBacked)
      {
        setname = QString();
        slotname = cblock->name;
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
          new RDTreeWidgetItem({QString(), setname, slotname, name, vecrange, sizestr, QString()});

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
                                               const VKPipe::Pipeline &pipe, RDLabel *shader,
                                               RDTreeWidget *resources, RDTreeWidget *ubos)
{
  ShaderReflection *shaderDetails = stage.reflection;

  QString shText =
      QFormatStr("%1: %2").arg(ToQStr(pipe.pipelineResourceId)).arg(ToQStr(stage.resourceId));

  if(shaderDetails != NULL)
  {
    QString entryFunc = shaderDetails->entryPoint;

    if(entryFunc != lit("main"))
      shText += lit(": ") + entryFunc + lit("()");

    if(!shaderDetails->debugInfo.files.isEmpty())
      shText += lit(" - ") + QFileInfo(shaderDetails->debugInfo.files[0].filename).fileName();
  }

  shader->setText(shText);

  int vs = 0;

  // hide the tree columns. The functions below will add it
  // if any array bindings are present
  resources->hideColumn(0);
  ubos->hideColumn(0);

  // generate expansion key from columns 1 (set) and 2 (binding)
  auto bindsetKeygen = [](QModelIndex idx, uint seed) {
    int row = idx.row();
    QString combined = idx.sibling(row, 1).data().toString() + idx.sibling(row, 2).data().toString();
    return qHash(combined, seed);
  };

  RDTreeViewExpansionState expansion;
  resources->saveExpansion(expansion, bindsetKeygen);

  vs = resources->verticalScrollBar()->value();
  resources->beginUpdate();
  resources->clear();

  QMap<ResourceId, SamplerData> samplers;

  for(int bindset = 0; bindset < pipe.descriptorSets.count(); bindset++)
  {
    for(int bind = 0; bind < pipe.descriptorSets[bindset].bindings.count(); bind++)
    {
      addResourceRow(shaderDetails, stage, bindset, bind, pipe, resources, samplers);
    }

    // if we have a shader bound, go through and add rows for any resources it wants for binds that
    // aren't
    // in this descriptor set (e.g. if layout mismatches)
    if(shaderDetails != NULL)
    {
      for(int i = 0; i < shaderDetails->readOnlyResources.count(); i++)
      {
        const ShaderResource &ro = shaderDetails->readOnlyResources[i];

        if(stage.bindpointMapping.readOnlyResources[ro.bindPoint].bindset == bindset &&
           stage.bindpointMapping.readOnlyResources[ro.bindPoint].bind >=
               pipe.descriptorSets[bindset].bindings.count())
        {
          addResourceRow(shaderDetails, stage, bindset,
                         stage.bindpointMapping.readOnlyResources[ro.bindPoint].bind, pipe,
                         resources, samplers);
        }
      }

      for(int i = 0; i < shaderDetails->readWriteResources.count(); i++)
      {
        const ShaderResource &rw = shaderDetails->readWriteResources[i];

        if(stage.bindpointMapping.readWriteResources[rw.bindPoint].bindset == bindset &&
           stage.bindpointMapping.readWriteResources[rw.bindPoint].bind >=
               pipe.descriptorSets[bindset].bindings.count())
        {
          addResourceRow(shaderDetails, stage, bindset,
                         stage.bindpointMapping.readWriteResources[rw.bindPoint].bind, pipe,
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
    for(int i = 0; i < shaderDetails->readOnlyResources.count(); i++)
    {
      const ShaderResource &ro = shaderDetails->readOnlyResources[i];

      if(stage.bindpointMapping.readOnlyResources[ro.bindPoint].bindset >= pipe.descriptorSets.count())
      {
        addResourceRow(
            shaderDetails, stage, stage.bindpointMapping.readOnlyResources[ro.bindPoint].bindset,
            stage.bindpointMapping.readOnlyResources[ro.bindPoint].bind, pipe, resources, samplers);
      }
    }

    for(int i = 0; i < shaderDetails->readWriteResources.count(); i++)
    {
      const ShaderResource &rw = shaderDetails->readWriteResources[i];

      if(stage.bindpointMapping.readWriteResources[rw.bindPoint].bindset >=
         pipe.descriptorSets.count())
      {
        addResourceRow(
            shaderDetails, stage, stage.bindpointMapping.readWriteResources[rw.bindPoint].bindset,
            stage.bindpointMapping.readWriteResources[rw.bindPoint].bind, pipe, resources, samplers);
      }
    }
  }

  resources->clearSelection();
  resources->endUpdate();
  resources->verticalScrollBar()->setValue(vs);

  resources->applyExpansion(expansion, bindsetKeygen);

  ubos->saveExpansion(expansion, bindsetKeygen);

  vs = ubos->verticalScrollBar()->value();
  ubos->beginUpdate();
  ubos->clear();
  for(int bindset = 0; bindset < pipe.descriptorSets.count(); bindset++)
  {
    for(int bind = 0; bind < pipe.descriptorSets[bindset].bindings.count(); bind++)
    {
      addConstantBlockRow(shaderDetails, stage, bindset, bind, pipe, ubos);
    }

    // if we have a shader bound, go through and add rows for any cblocks it wants for binds that
    // aren't
    // in this descriptor set (e.g. if layout mismatches)
    if(shaderDetails != NULL)
    {
      for(int i = 0; i < shaderDetails->constantBlocks.count(); i++)
      {
        const ConstantBlock &cb = shaderDetails->constantBlocks[i];

        if(stage.bindpointMapping.constantBlocks[cb.bindPoint].bindset == bindset &&
           stage.bindpointMapping.constantBlocks[cb.bindPoint].bind >=
               pipe.descriptorSets[bindset].bindings.count())
        {
          addConstantBlockRow(shaderDetails, stage, bindset,
                              stage.bindpointMapping.constantBlocks[cb.bindPoint].bind, pipe, ubos);
        }
      }
    }
  }

  // if we have a shader bound, go through and add rows for any resources it wants for descriptor
  // sets that aren't
  // bound at all
  if(shaderDetails != NULL)
  {
    for(int i = 0; i < shaderDetails->constantBlocks.count(); i++)
    {
      const ConstantBlock &cb = shaderDetails->constantBlocks[i];

      if(stage.bindpointMapping.constantBlocks[cb.bindPoint].bindset >= pipe.descriptorSets.count() &&
         cb.bufferBacked)
      {
        addConstantBlockRow(shaderDetails, stage,
                            stage.bindpointMapping.constantBlocks[cb.bindPoint].bindset,
                            stage.bindpointMapping.constantBlocks[cb.bindPoint].bind, pipe, ubos);
      }
    }
  }

  // search for push constants and add them last
  if(shaderDetails != NULL)
  {
    for(int cb = 0; cb < shaderDetails->constantBlocks.count(); cb++)
    {
      ConstantBlock &cblock = shaderDetails->constantBlocks[cb];
      if(cblock.bufferBacked == false)
      {
        // could maybe get range from ShaderVariable.reg if it's filled out
        // from SPIR-V side.

        RDTreeWidgetItem *node =
            new RDTreeWidgetItem({QString(), QString(), cblock.name, tr("Push constants"), QString(),
                                  tr("%1 Variables").arg(cblock.variables.count()), QString()});

        node->setTag(QVariant::fromValue(VulkanCBufferTag(cb, 0)));

        ubos->addTopLevelItem(node);
      }
    }
  }
  ubos->clearSelection();
  ubos->endUpdate();
  ubos->verticalScrollBar()->setValue(vs);

  ubos->applyExpansion(expansion, bindsetKeygen);
}

void VulkanPipelineStateViewer::setState()
{
  if(!m_Ctx.IsCaptureLoaded())
  {
    clearState();
    return;
  }

  m_CombinedImageSamplers.clear();

  const VKPipe::State &state = *m_Ctx.CurVulkanPipelineState();
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
  ui->viAttrs->beginUpdate();
  ui->viAttrs->clear();
  {
    int i = 0;
    for(const VKPipe::VertexAttribute &a : state.vertexInput.attributes)
    {
      bool usedSlot = false;

      QString name = tr("Attribute %1").arg(i);

      if(state.vertexShader.resourceId != ResourceId())
      {
        int attrib = -1;
        if((int32_t)a.location < state.vertexShader.bindpointMapping.inputAttributes.count())
          attrib = state.vertexShader.bindpointMapping.inputAttributes[a.location];

        if(attrib >= 0 && attrib < state.vertexShader.reflection->inputSignature.count())
        {
          name = state.vertexShader.reflection->inputSignature[attrib].varName;
          usedSlot = true;
        }
      }

      if(showNode(usedSlot, /*filledSlot*/ true))
      {
        RDTreeWidgetItem *node = new RDTreeWidgetItem(
            {i, name, a.location, a.binding, a.format.Name(), a.byteOffset, QString()});

        node->setTag(i);

        usedBindings[a.binding] = true;

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

  m_BindNodes.clear();
  m_VBNodes.clear();
  m_EmptyNodes.clear();

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

  ui->primRestart->setVisible(state.inputAssembly.primitiveRestartEnable);

  vs = ui->viBuffers->verticalScrollBar()->value();
  ui->viBuffers->beginUpdate();
  ui->viBuffers->clear();

  bool ibufferUsed = draw != NULL && (draw->flags & DrawFlags::Indexed);

  if(state.inputAssembly.indexBuffer.resourceId != ResourceId())
  {
    if(ibufferUsed || showDisabled)
    {
      uint64_t length = 1;

      if(!ibufferUsed)
        length = 0;

      BufferDescription *buf = m_Ctx.GetBuffer(state.inputAssembly.indexBuffer.resourceId);

      if(buf)
        length = buf->length;

      RDTreeWidgetItem *node = new RDTreeWidgetItem(
          {tr("Index"), state.inputAssembly.indexBuffer.resourceId, tr("Index"), lit("-"),
           (qulonglong)state.inputAssembly.indexBuffer.byteOffset,
           draw != NULL ? draw->indexByteWidth : 0, (qulonglong)length, QString()});

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

      node->setTag(QVariant::fromValue(
          VulkanVBIBTag(state.inputAssembly.indexBuffer.resourceId,
                        state.inputAssembly.indexBuffer.byteOffset +
                            (draw ? draw->indexOffset * draw->indexByteWidth : 0),
                        iformat)));

      if(!ibufferUsed)
        setInactiveRow(node);

      if(state.inputAssembly.indexBuffer.resourceId == ResourceId())
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
      RDTreeWidgetItem *node = new RDTreeWidgetItem({tr("Index"), ResourceId(), tr("Index"), lit("-"),
                                                     lit("-"), lit("-"), lit("-"), QString()});

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

      node->setTag(QVariant::fromValue(
          VulkanVBIBTag(state.inputAssembly.indexBuffer.resourceId,
                        state.inputAssembly.indexBuffer.byteOffset +
                            (draw ? draw->indexOffset * draw->indexByteWidth : 0),
                        iformat)));

      setEmptyRow(node);
      m_EmptyNodes.push_back(node);

      if(!ibufferUsed)
        setInactiveRow(node);

      ui->viBuffers->addTopLevelItem(node);
    }
  }

  {
    int i = 0;
    for(; i < qMax(state.vertexInput.vertexBuffers.count(), state.vertexInput.bindings.count()); i++)
    {
      const VKPipe::VertexBuffer *vbuff =
          (i < state.vertexInput.vertexBuffers.count() ? &state.vertexInput.vertexBuffers[i] : NULL);
      const VKPipe::VertexBinding *bind = NULL;

      for(int b = 0; b < state.vertexInput.bindings.count(); b++)
      {
        if(state.vertexInput.bindings[b].vertexBufferBinding == (uint32_t)i)
          bind = &state.vertexInput.bindings[b];
      }

      bool filledSlot = ((vbuff != NULL && vbuff->resourceId != ResourceId()) || bind != NULL);
      bool usedSlot = (usedBindings[i]);

      if(showNode(usedSlot, filledSlot))
      {
        QString rate = lit("-");
        uint64_t length = 1;
        uint64_t offset = 0;
        uint32_t stride = 0;
        uint32_t divisor = 1;

        if(vbuff != NULL)
        {
          offset = vbuff->byteOffset;

          BufferDescription *buf = m_Ctx.GetBuffer(vbuff->resourceId);
          if(buf)
            length = buf->length;
        }

        if(bind != NULL)
        {
          stride = bind->byteStride;
          rate = bind->perInstance ? tr("Instance") : tr("Vertex");
          if(bind->perInstance)
            divisor = bind->instanceDivisor;
        }
        else
        {
          rate += tr("No Binding");
        }

        RDTreeWidgetItem *node = NULL;

        if(filledSlot)
          node = new RDTreeWidgetItem({i, vbuff->resourceId, rate, divisor, (qulonglong)offset,
                                       stride, (qulonglong)length, QString()});
        else
          node = new RDTreeWidgetItem(
              {i, tr("No Binding"), lit("-"), lit("-"), lit("-"), lit("-"), lit("-"), QString()});

        node->setTag(QVariant::fromValue(VulkanVBIBTag(
            vbuff != NULL ? vbuff->resourceId : ResourceId(), vbuff != NULL ? vbuff->byteOffset : 0,
            m_Common.GetVBufferFormatString(i))));

        if(!filledSlot || bind == NULL || vbuff == NULL)
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

    for(; i < (int)ARRAY_COUNT(usedBindings); i++)
    {
      if(usedBindings[i])
      {
        RDTreeWidgetItem *node = new RDTreeWidgetItem(
            {i, tr("No Binding"), lit("-"), lit("-"), lit("-"), lit("-"), lit("-"), QString()});

        node->setTag(QVariant::fromValue(VulkanVBIBTag(ResourceId(), 0)));

        setEmptyRow(node);
        m_EmptyNodes.push_back(node);

        setInactiveRow(node);

        ui->viBuffers->addTopLevelItem(node);

        m_VBNodes.push_back(node);
      }
      else
      {
        m_VBNodes.push_back(NULL);
      }
    }
  }
  ui->viBuffers->clearSelection();
  ui->viBuffers->endUpdate();
  ui->viBuffers->verticalScrollBar()->setValue(vs);

  setShaderState(state.vertexShader, state.graphics, ui->vsShader, ui->vsResources, ui->vsUBOs);
  setShaderState(state.geometryShader, state.graphics, ui->gsShader, ui->gsResources, ui->gsUBOs);
  setShaderState(state.tessControlShader, state.graphics, ui->tcsShader, ui->tcsResources,
                 ui->tcsUBOs);
  setShaderState(state.tessEvalShader, state.graphics, ui->tesShader, ui->tesResources, ui->tesUBOs);
  setShaderState(state.fragmentShader, state.graphics, ui->fsShader, ui->fsResources, ui->fsUBOs);
  setShaderState(state.computeShader, state.compute, ui->csShader, ui->csResources, ui->csUBOs);

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
    const VKPipe::Shader *stage = stageForSender(b);

    if(stage == NULL || stage->resourceId == ResourceId())
      continue;

    ShaderReflection *shaderDetails = stage->reflection;

    ResourceId pipe = stage->stage == ShaderStage::Compute ? state.compute.pipelineResourceId
                                                           : state.graphics.pipelineResourceId;

    b->setEnabled(shaderDetails && pipe != ResourceId());

    m_Common.SetupShaderEditButton(b, pipe, stage->resourceId, shaderDetails);
  }

  bool xfbSet = false;
  vs = ui->xfbBuffers->verticalScrollBar()->value();
  ui->xfbBuffers->beginUpdate();
  ui->xfbBuffers->clear();
  for(int i = 0; i < state.transformFeedback.buffers.count(); i++)
  {
    const VKPipe::XFBBuffer &s = state.transformFeedback.buffers[i];

    bool filledSlot = (s.bufferResourceId != ResourceId());
    bool usedSlot = (s.active);

    if(showNode(usedSlot, filledSlot))
    {
      qulonglong length = s.byteSize;

      BufferDescription *buf = m_Ctx.GetBuffer(s.bufferResourceId);

      if(buf && length == UINT64_MAX)
        length = buf->length - s.byteOffset;

      RDTreeWidgetItem *node = new RDTreeWidgetItem(
          {i, s.active ? tr("Active") : tr("Inactive"), s.bufferResourceId, (qulonglong)s.byteOffset,
           length, s.counterBufferResourceId, (qulonglong)s.counterBufferOffset, QString()});

      node->setTag(QVariant::fromValue(
          VulkanBufferTag(false, ~0U, ResourceFormat(), s.bufferResourceId, s.byteOffset, length)));

      if(!filledSlot)
        setEmptyRow(node);

      if(!usedSlot)
        setInactiveRow(node);

      xfbSet = true;

      ui->xfbBuffers->addTopLevelItem(node);
    }
  }
  ui->xfbBuffers->verticalScrollBar()->setValue(vs);
  ui->xfbBuffers->clearSelection();
  ui->xfbBuffers->endUpdate();

  ui->xfbBuffers->setVisible(xfbSet);
  ui->xfbGroup->setVisible(xfbSet);

  ////////////////////////////////////////////////
  // Rasterizer

  vs = ui->discards->verticalScrollBar()->value();
  ui->discards->beginUpdate();
  ui->discards->clear();

  {
    int i = 0;
    for(const VKPipe::RenderArea &v : state.viewportScissor.discardRectangles)
    {
      RDTreeWidgetItem *node = new RDTreeWidgetItem({i, v.x, v.y, v.width, v.height});
      ui->discards->addTopLevelItem(node);

      if(v.width == 0 || v.height == 0)
        setEmptyRow(node);

      i++;
    }
  }

  ui->discards->verticalScrollBar()->setValue(vs);
  ui->discards->clearSelection();
  ui->discards->endUpdate();

  ui->discardMode->setText(state.viewportScissor.discardRectanglesExclusive ? tr("Exclusive")
                                                                            : tr("Inclusive"));

  ui->discardGroup->setVisible(!state.viewportScissor.discardRectanglesExclusive ||
                               !state.viewportScissor.discardRectangles.isEmpty());

  vs = ui->viewports->verticalScrollBar()->value();
  ui->viewports->beginUpdate();
  ui->viewports->clear();

  int vs2 = ui->scissors->verticalScrollBar()->value();
  ui->scissors->beginUpdate();
  ui->scissors->clear();

  if(state.currentPass.renderpass.resourceId != ResourceId())
  {
    ui->scissors->addTopLevelItem(new RDTreeWidgetItem(
        {tr("Render Area"), state.currentPass.renderArea.x, state.currentPass.renderArea.y,
         state.currentPass.renderArea.width, state.currentPass.renderArea.height}));
  }

  {
    int i = 0;
    for(const VKPipe::ViewportScissor &v : state.viewportScissor.viewportScissors)
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

  ui->viewports->endUpdate();
  ui->scissors->endUpdate();

  ui->fillMode->setText(ToQStr(state.rasterizer.fillMode));
  ui->cullMode->setText(ToQStr(state.rasterizer.cullMode));
  ui->frontCCW->setPixmap(state.rasterizer.frontCCW ? tick : cross);

  ui->depthBias->setText(Formatter::Format(state.rasterizer.depthBias));
  ui->depthBiasClamp->setText(Formatter::Format(state.rasterizer.depthBiasClamp));
  ui->slopeScaledBias->setText(Formatter::Format(state.rasterizer.slopeScaledDepthBias));

  ui->depthClamp->setPixmap(state.rasterizer.depthClampEnable ? tick : cross);
  ui->depthClip->setPixmap(state.rasterizer.depthClipEnable ? tick : cross);
  ui->rasterizerDiscard->setPixmap(state.rasterizer.rasterizerDiscardEnable ? tick : cross);
  ui->lineWidth->setText(Formatter::Format(state.rasterizer.lineWidth));

  ui->conservativeRaster->setText(ToQStr(state.rasterizer.conservativeRasterization));
  ui->overestimationSize->setText(
      Formatter::Format(state.rasterizer.extraPrimitiveOverestimationSize));

  if(state.currentPass.renderpass.multiviews.isEmpty())
  {
    ui->multiview->setText(tr("Disabled"));
  }
  else
  {
    QString views = tr("Views: ");
    for(int i = 0; i < state.currentPass.renderpass.multiviews.count(); i++)
    {
      if(i > 0)
        views += lit(", ");
      views += QString::number(state.currentPass.renderpass.multiviews[i]);
    }
    ui->multiview->setText(views);
  }

  ui->sampleCount->setText(QString::number(state.multisample.rasterSamples));
  ui->sampleShading->setPixmap(state.multisample.sampleShadingEnable ? tick : cross);
  ui->minSampleShading->setText(Formatter::Format(state.multisample.minSampleShading));
  ui->sampleMask->setText(Formatter::Format(state.multisample.sampleMask, true));
  ui->alphaToOne->setPixmap(state.colorBlend.alphaToOneEnable ? tick : cross);
  ui->alphaToCoverage->setPixmap(state.colorBlend.alphaToCoverageEnable ? tick : cross);

  ////////////////////////////////////////////////
  // Conditional Rendering

  if(state.conditionalRendering.bufferId == ResourceId())
  {
    ui->conditionalRenderingGroup->setVisible(false);
    ui->csConditionalRenderingGroup->setVisible(false);
  }
  else
  {
    ui->conditionalRenderingGroup->setVisible(true);
    ui->predicateBuffer->setText(QFormatStr("%1 (Byte Offset %2)")
                                     .arg(ToQStr(state.conditionalRendering.bufferId))
                                     .arg(state.conditionalRendering.byteOffset));
    ui->predicatePassing->setPixmap(state.conditionalRendering.isPassing ? tick : cross);
    ui->predicateInverted->setPixmap(state.conditionalRendering.isInverted ? tick : cross);

    ui->csConditionalRenderingGroup->setVisible(true);
    ui->csPredicateBuffer->setText(QFormatStr("%1 (Byte Offset %2)")
                                       .arg(ToQStr(state.conditionalRendering.bufferId))
                                       .arg(state.conditionalRendering.byteOffset));
    ui->csPredicatePassing->setPixmap(state.conditionalRendering.isPassing ? tick : cross);
    ui->csPredicateInverted->setPixmap(state.conditionalRendering.isInverted ? tick : cross);
  }

  ////////////////////////////////////////////////
  // Output Merger

  bool targets[32] = {};

  ui->renderpass->setText(QFormatStr("Render Pass: %1 (Subpass %2)")
                              .arg(ToQStr(state.currentPass.renderpass.resourceId))
                              .arg(state.currentPass.renderpass.subpass));
  ui->framebuffer->setText(
      QFormatStr("Framebuffer: %1").arg(ToQStr(state.currentPass.framebuffer.resourceId)));

  vs = ui->fbAttach->verticalScrollBar()->value();
  ui->fbAttach->beginUpdate();
  ui->fbAttach->clear();
  {
    int i = 0;
    for(const VKPipe::Attachment &p : state.currentPass.framebuffer.attachments)
    {
      int colIdx = -1;
      for(int c = 0; c < state.currentPass.renderpass.colorAttachments.count(); c++)
      {
        if(state.currentPass.renderpass.colorAttachments[c] == (uint)i)
        {
          colIdx = c;
          break;
        }
      }
      int resIdx = -1;
      for(int c = 0; c < state.currentPass.renderpass.resolveAttachments.count(); c++)
      {
        if(state.currentPass.renderpass.resolveAttachments[c] == (uint)i)
        {
          resIdx = c;
          break;
        }
      }

      bool filledSlot = (p.imageResourceId != ResourceId());
      bool usedSlot =
          (colIdx >= 0 || resIdx >= 0 || state.currentPass.renderpass.depthstencilAttachment == i ||
           state.currentPass.renderpass.fragmentDensityAttachment == i);

      if(showNode(usedSlot, filledSlot))
      {
        uint32_t w = 1, h = 1, d = 1;
        uint32_t a = 1;
        QString format;
        QString typeName;

        if(p.imageResourceId != ResourceId())
        {
          format = p.viewFormat.Name();
          typeName = tr("Unknown");
        }
        else
        {
          format = lit("-");
          typeName = lit("-");
          w = h = d = a = 0;
        }

        TextureDescription *tex = m_Ctx.GetTexture(p.imageResourceId);
        if(tex)
        {
          w = tex->width;
          h = tex->height;
          d = tex->depth;
          a = tex->arraysize;
          typeName = ToQStr(tex->type);
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
        else if(state.currentPass.renderpass.fragmentDensityAttachment == i)
          slotname = lit("Fragment Density Map");
        else
          slotname = lit("Depth");

        if(state.fragmentShader.reflection != NULL)
        {
          for(int s = 0; s < state.fragmentShader.reflection->outputSignature.count(); s++)
          {
            if(state.fragmentShader.reflection->outputSignature[s].regIndex == (uint32_t)colIdx &&
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

        RDTreeWidgetItem *node = new RDTreeWidgetItem(
            {slotname, p.imageResourceId, typeName, w, h, d, a, format, QString()});

        if(tex)
          node->setTag(QVariant::fromValue(p.imageResourceId));

        if(p.imageResourceId == ResourceId())
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

        setViewDetails(node, p, tex, resIdx < 0);

        ui->fbAttach->addTopLevelItem(node);
      }

      i++;
    }
  }

  ui->fbAttach->clearSelection();
  ui->fbAttach->endUpdate();
  ui->fbAttach->verticalScrollBar()->setValue(vs);

  vs = ui->blends->verticalScrollBar()->value();
  ui->blends->beginUpdate();
  ui->blends->clear();
  {
    int i = 0;
    for(const ColorBlend &blend : state.colorBlend.blends)
    {
      bool usedSlot = (targets[i]);

      if(showNode(usedSlot, /*filledSlot*/ true))
      {
        RDTreeWidgetItem *node = new RDTreeWidgetItem(
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
                               .arg(state.colorBlend.blendFactor[0], 0, 'f', 2)
                               .arg(state.colorBlend.blendFactor[1], 0, 'f', 2)
                               .arg(state.colorBlend.blendFactor[2], 0, 'f', 2)
                               .arg(state.colorBlend.blendFactor[3], 0, 'f', 2));
  if(state.colorBlend.blends.count() > 0)
    ui->logicOp->setText(state.colorBlend.blends[0].logicOperationEnabled
                             ? ToQStr(state.colorBlend.blends[0].logicOperation)
                             : lit("-"));
  else
    ui->logicOp->setText(lit("-"));

  ui->depthEnabled->setPixmap(state.depthStencil.depthTestEnable ? tick : cross);
  ui->depthFunc->setText(ToQStr(state.depthStencil.depthFunction));
  ui->depthWrite->setPixmap(state.depthStencil.depthWriteEnable ? tick : cross);

  if(state.depthStencil.depthBoundsEnable)
  {
    ui->depthBounds->setPixmap(QPixmap());
    ui->depthBounds->setText(Formatter::Format(state.depthStencil.minDepthBounds) + lit("-") +
                             Formatter::Format(state.depthStencil.maxDepthBounds));
  }
  else
  {
    ui->depthBounds->setText(QString());
    ui->depthBounds->setPixmap(cross);
  }

  ui->stencils->beginUpdate();
  ui->stencils->clear();
  if(state.depthStencil.stencilTestEnable)
  {
    ui->stencils->addTopLevelItem(new RDTreeWidgetItem(
        {tr("Front"), ToQStr(state.depthStencil.frontFace.function),
         ToQStr(state.depthStencil.frontFace.failOperation),
         ToQStr(state.depthStencil.frontFace.depthFailOperation),
         ToQStr(state.depthStencil.frontFace.passOperation),
         Formatter::Format((uint8_t)state.depthStencil.frontFace.writeMask, true),
         Formatter::Format((uint8_t)state.depthStencil.frontFace.compareMask, true),
         Formatter::Format((uint8_t)state.depthStencil.frontFace.reference, true)}));
    ui->stencils->addTopLevelItem(new RDTreeWidgetItem(
        {tr("Back"), ToQStr(state.depthStencil.backFace.function),
         ToQStr(state.depthStencil.backFace.failOperation),
         ToQStr(state.depthStencil.backFace.depthFailOperation),
         ToQStr(state.depthStencil.backFace.passOperation),
         Formatter::Format((uint8_t)state.depthStencil.backFace.writeMask, true),
         Formatter::Format((uint8_t)state.depthStencil.backFace.compareMask, true),
         Formatter::Format((uint8_t)state.depthStencil.backFace.reference, true)}));
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
    bool xfbActive = !state.transformFeedback.buffers.isEmpty();

    if(state.geometryShader.resourceId == ResourceId() && xfbActive)
    {
      ui->pipeFlow->setStageName(4, lit("XFB"), tr("Transform Feedback"));
    }
    else
    {
      ui->pipeFlow->setStageName(4, lit("GS"), tr("Geometry Shader"));
    }

    ui->pipeFlow->setStagesEnabled({true, true, state.tessControlShader.resourceId != ResourceId(),
                                    state.tessEvalShader.resourceId != ResourceId(),
                                    state.geometryShader.resourceId != ResourceId() || xfbActive, true,
                                    state.fragmentShader.resourceId != ResourceId(), true, false});
  }
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
  else if(tag.canConvert<VulkanBufferTag>())
  {
    VulkanBufferTag buf = tag.value<VulkanBufferTag>();

    QString format;

    if(stage->reflection &&
       buf.bindPoint < (buf.rwRes ? stage->reflection->readWriteResources.size()
                                  : stage->reflection->readOnlyResources.size()))
    {
      const ShaderResource &shaderRes = buf.rwRes
                                            ? stage->reflection->readWriteResources[buf.bindPoint]
                                            : stage->reflection->readOnlyResources[buf.bindPoint];

      format = m_Common.GenerateBufferFormatter(shaderRes, buf.fmt, buf.offset);
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

  m_Ctx.AddDockWindow(prev->Widget(), DockReference::TransientPopupArea, this, 0.3f);
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
      IBufferViewer *viewer = m_Ctx.ViewBuffer(buf.offset, UINT64_MAX, buf.id, buf.format);

      m_Ctx.AddDockWindow(viewer->Widget(), DockReference::AddTo, this);
    }
  }
}

void VulkanPipelineStateViewer::highlightIABind(int slot)
{
  int idx = ((slot + 1) * 21) % 32;    // space neighbouring colours reasonably distinctly

  const VKPipe::VertexInput &VI = m_Ctx.CurVulkanPipelineState()->vertexInput;

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

  if(slot < m_BindNodes.count())
  {
    m_BindNodes[slot]->setBackgroundColor(col);
    m_BindNodes[slot]->setForegroundColor(contrastingColor(col, QColor(0, 0, 0)));
  }

  for(int i = 0; i < ui->viAttrs->topLevelItemCount(); i++)
  {
    RDTreeWidgetItem *item = ui->viAttrs->topLevelItem(i);

    if((int)VI.attributes[item->tag().toUInt()].binding != slot)
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
  if(!m_Ctx.IsCaptureLoaded())
    return;

  RDTreeWidgetItem *item = ui->viAttrs->itemAt(e->pos());

  vertex_leave(NULL);

  const VKPipe::VertexInput &VI = m_Ctx.CurVulkanPipelineState()->vertexInput;

  if(item)
  {
    uint32_t binding = VI.attributes[item->tag().toUInt()].binding;

    highlightIABind((int)binding);
  }
}

void VulkanPipelineStateViewer::on_viBuffers_mouseMove(QMouseEvent *e)
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
    RDTreeWidgetItem *item = ui->viBuffers->topLevelItem(i);

    if(m_EmptyNodes.contains(item))
      continue;

    item->setBackground(QBrush());
    item->setForeground(QBrush());
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

  if(stage == NULL || stage->resourceId == ResourceId())
    return;

  ShaderReflection *shaderDetails = stage->reflection;

  ResourceId pipe = stage->stage == ShaderStage::Compute
                        ? m_Ctx.CurVulkanPipelineState()->compute.pipelineResourceId
                        : m_Ctx.CurVulkanPipelineState()->graphics.pipelineResourceId;

  if(!shaderDetails)
    return;

  IShaderViewer *shad = m_Ctx.ViewShader(shaderDetails, pipe);

  m_Ctx.AddDockWindow(shad->Widget(), DockReference::AddTo, this);
}

void VulkanPipelineStateViewer::shaderSave_clicked()
{
  const VKPipe::Shader *stage = stageForSender(qobject_cast<QWidget *>(QObject::sender()));

  if(stage == NULL)
    return;

  ShaderReflection *shaderDetails = stage->reflection;

  if(stage->resourceId == ResourceId())
    return;

  m_Common.SaveShaderFile(shaderDetails);
}

void VulkanPipelineStateViewer::predicateBufferView_clicked()
{
  const VKPipe::ConditionalRendering &cr = m_Ctx.CurVulkanPipelineState()->conditionalRendering;

  IBufferViewer *viewer = m_Ctx.ViewBuffer(cr.byteOffset, sizeof(uint32_t), cr.bufferId, "uint");

  m_Ctx.AddDockWindow(viewer->Widget(), DockReference::AddTo, this);
}

void VulkanPipelineStateViewer::exportHTML(QXmlStreamWriter &xml, const VKPipe::VertexInput &vi)
{
  {
    xml.writeStartElement(lit("h3"));
    xml.writeCharacters(tr("Attributes"));
    xml.writeEndElement();

    QList<QVariantList> rows;

    for(const VKPipe::VertexAttribute &attr : vi.attributes)
      rows.push_back({attr.location, attr.binding, attr.format.Name(), attr.byteOffset});

    m_Common.exportHTMLTable(xml, {tr("Location"), tr("Binding"), tr("Format"), tr("Offset")}, rows);
  }

  {
    xml.writeStartElement(lit("h3"));
    xml.writeCharacters(tr("Bindings"));
    xml.writeEndElement();

    QList<QVariantList> rows;

    for(const VKPipe::VertexBinding &attr : vi.bindings)
      rows.push_back({attr.vertexBufferBinding, attr.byteStride,
                      attr.perInstance ? tr("PER_INSTANCE") : tr("PER_VERTEX")});

    m_Common.exportHTMLTable(xml, {tr("Binding"), tr("Byte Stride"), tr("Step Rate")}, rows);
  }

  {
    xml.writeStartElement(lit("h3"));
    xml.writeCharacters(tr("Vertex Buffers"));
    xml.writeEndElement();

    QList<QVariantList> rows;

    int i = 0;
    for(const VKPipe::VertexBuffer &vb : vi.vertexBuffers)
    {
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

      rows.push_back({i, vb.resourceId, (qulonglong)vb.byteOffset, (qulonglong)length});

      i++;
    }

    m_Common.exportHTMLTable(xml, {tr("Binding"), tr("Buffer"), tr("Offset"), tr("Byte Length")},
                             rows);
  }
}

void VulkanPipelineStateViewer::exportHTML(QXmlStreamWriter &xml, const VKPipe::InputAssembly &ia)
{
  {
    xml.writeStartElement(lit("h3"));
    xml.writeCharacters(tr("Index Buffer"));
    xml.writeEndElement();

    BufferDescription *ib = m_Ctx.GetBuffer(ia.indexBuffer.resourceId);

    QString name = tr("Empty");
    uint64_t length = 0;

    if(ib)
    {
      name = m_Ctx.GetResourceName(ia.indexBuffer.resourceId);
      length = ib->length;
    }

    QString ifmt = lit("UNKNOWN");
    if(m_Ctx.CurDrawcall()->indexByteWidth == 2)
      ifmt = lit("UINT16");
    if(m_Ctx.CurDrawcall()->indexByteWidth == 4)
      ifmt = lit("UINT32");

    m_Common.exportHTMLTable(
        xml, {tr("Buffer"), tr("Format"), tr("Offset"), tr("Byte Length"), tr("Primitive Restart")},
        {name, ifmt, (qulonglong)ia.indexBuffer.byteOffset, (qulonglong)length,
         ia.primitiveRestartEnable ? tr("Yes") : tr("No")});
  }

  xml.writeStartElement(lit("p"));
  xml.writeEndElement();

  m_Common.exportHTMLTable(xml, {tr("Primitive Topology"), tr("Tessellation Control Points")},
                           {ToQStr(m_Ctx.CurDrawcall()->topology),
                            m_Ctx.CurVulkanPipelineState()->tessellation.numControlPoints});
}

void VulkanPipelineStateViewer::exportHTML(QXmlStreamWriter &xml, const VKPipe::Shader &sh)
{
  ShaderReflection *shaderDetails = sh.reflection;

  {
    xml.writeStartElement(lit("h3"));
    xml.writeCharacters(tr("Shader"));
    xml.writeEndElement();

    QString shadername = tr("Unknown");

    if(sh.resourceId == ResourceId())
      shadername = tr("Unbound");
    else
      shadername = m_Ctx.GetResourceName(sh.resourceId);

    if(shaderDetails)
    {
      QString entryFunc = shaderDetails->entryPoint;
      if(entryFunc != lit("main"))
        shadername = QFormatStr("%1()").arg(entryFunc);
      else if(!shaderDetails->debugInfo.files.isEmpty())
        shadername = QFormatStr("%1() - %2")
                         .arg(entryFunc)
                         .arg(QFileInfo(shaderDetails->debugInfo.files[0].filename).fileName());
    }

    xml.writeStartElement(lit("p"));
    xml.writeCharacters(shadername);
    xml.writeEndElement();

    if(sh.resourceId == ResourceId())
      return;
  }

  const VKPipe::Pipeline &pipeline =
      (sh.stage == ShaderStage::Compute ? m_Ctx.CurVulkanPipelineState()->compute
                                        : m_Ctx.CurVulkanPipelineState()->graphics);

  if(shaderDetails && !shaderDetails->constantBlocks.isEmpty())
  {
    xml.writeStartElement(lit("h3"));
    xml.writeCharacters(tr("UBOs"));
    xml.writeEndElement();

    QList<QVariantList> rows;

    for(int i = 0; i < shaderDetails->constantBlocks.count(); i++)
    {
      const ConstantBlock &b = shaderDetails->constantBlocks[i];
      const Bindpoint &bindMap = sh.bindpointMapping.constantBlocks[i];

      if(!bindMap.used)
        continue;

      // push constants
      if(!b.bufferBacked)
      {
        // could maybe get range/size from ShaderVariable.reg if it's filled out
        // from SPIR-V side.
        rows.push_back({QString(), b.name, tr("Push constants"), (qulonglong)0, (qulonglong)0,
                        b.variables.count(), b.byteSize});

        continue;
      }

      const VKPipe::DescriptorSet &set =
          pipeline.descriptorSets[sh.bindpointMapping.constantBlocks[i].bindset];
      const VKPipe::DescriptorBinding &bind =
          set.bindings[sh.bindpointMapping.constantBlocks[i].bind];

      QString setname = QString::number(bindMap.bindset);

      if(set.pushDescriptor)
        setname = tr("Push ") + setname;

      QString slotname = QFormatStr("%1: %2").arg(bindMap.bind).arg(b.name);

      for(uint32_t a = 0; a < bind.descriptorCount; a++)
      {
        const VKPipe::BindingElement &descriptorBind = bind.binds[a];

        ResourceId id = bind.binds[a].resourceResourceId;

        if(bindMap.arraySize > 1)
          slotname = QFormatStr("%1: %2[%3]").arg(bindMap.bind).arg(b.name).arg(a);

        QString name = m_Ctx.GetResourceName(descriptorBind.resourceResourceId);
        uint64_t byteOffset = descriptorBind.byteOffset;
        uint64_t length = descriptorBind.byteSize;
        int numvars = b.variables.count();

        if(descriptorBind.resourceResourceId == ResourceId())
        {
          name = tr("Empty");
          length = 0;
        }

        BufferDescription *buf = m_Ctx.GetBuffer(id);
        if(buf)
        {
          if(length == UINT64_MAX)
            length = buf->length - byteOffset;
        }

        rows.push_back({setname, slotname, name, (qulonglong)byteOffset, (qulonglong)length,
                        numvars, b.byteSize});
      }
    }

    m_Common.exportHTMLTable(xml, {tr("Set"), tr("Bind"), tr("Buffer"), tr("Byte Offset"),
                                   tr("Byte Size"), tr("Number of Variables"), tr("Bytes Needed")},
                             rows);
  }

  if(shaderDetails && !shaderDetails->readOnlyResources.isEmpty())
  {
    xml.writeStartElement(lit("h3"));
    xml.writeCharacters(tr("Read-only Resources"));
    xml.writeEndElement();

    QList<QVariantList> rows;

    for(int i = 0; i < shaderDetails->readOnlyResources.count(); i++)
    {
      const ShaderResource &b = shaderDetails->readOnlyResources[i];
      const Bindpoint &bindMap = sh.bindpointMapping.readOnlyResources[i];

      if(!bindMap.used)
        continue;

      const VKPipe::DescriptorSet &set =
          pipeline.descriptorSets[sh.bindpointMapping.readOnlyResources[i].bindset];
      const VKPipe::DescriptorBinding &bind =
          set.bindings[sh.bindpointMapping.readOnlyResources[i].bind];

      QString setname = QString::number(bindMap.bindset);

      if(set.pushDescriptor)
        setname = tr("Push ") + setname;

      QString slotname = QFormatStr("%1: %2").arg(bindMap.bind).arg(b.name);

      for(uint32_t a = 0; a < bind.descriptorCount; a++)
      {
        const VKPipe::BindingElement &descriptorBind = bind.binds[a];

        ResourceId id = descriptorBind.resourceResourceId;

        if(bindMap.arraySize > 1)
          slotname = QFormatStr("%1: %2[%3]").arg(bindMap.bind).arg(b.name).arg(a);

        QString name = m_Ctx.GetResourceName(id);

        if(id == ResourceId())
          name = tr("Empty");

        BufferDescription *buf = m_Ctx.GetBuffer(id);
        TextureDescription *tex = m_Ctx.GetTexture(id);

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
          format = tex->format.Name();

          if(tex->mips > 1)
          {
            viewParams = tr("Mips: %1-%2")
                             .arg(descriptorBind.firstMip)
                             .arg(descriptorBind.firstMip + descriptorBind.numMips - 1);
          }

          if(tex->arraysize > 1)
          {
            if(!viewParams.isEmpty())
              viewParams += lit(", ");
            viewParams += tr("Layers: %1-%2")
                              .arg(descriptorBind.firstSlice)
                              .arg(descriptorBind.firstSlice + descriptorBind.numSlices - 1);
          }
        }

        if(buf)
        {
          w = buf->length;
          h = 0;
          d = 0;
          a = 0;
          format = lit("-");

          viewParams = tr("Byte Range: %1").arg(formatByteRange(buf, &descriptorBind));
        }

        if(bind.type != BindType::Sampler)
          rows.push_back({setname, slotname, name, ToQStr(bind.type), (qulonglong)w, h, d, arr,
                          format, viewParams});

        if(bind.type == BindType::ImageSampler || bind.type == BindType::Sampler)
        {
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

  if(shaderDetails && !shaderDetails->readWriteResources.isEmpty())
  {
    xml.writeStartElement(lit("h3"));
    xml.writeCharacters(tr("Read-write Resources"));
    xml.writeEndElement();

    QList<QVariantList> rows;

    for(int i = 0; i < shaderDetails->readWriteResources.count(); i++)
    {
      const ShaderResource &b = shaderDetails->readWriteResources[i];
      const Bindpoint &bindMap = sh.bindpointMapping.readWriteResources[i];

      if(!bindMap.used)
        continue;

      const VKPipe::DescriptorSet &set =
          pipeline.descriptorSets[sh.bindpointMapping.readWriteResources[i].bindset];
      const VKPipe::DescriptorBinding &bind =
          set.bindings[sh.bindpointMapping.readWriteResources[i].bind];

      QString setname = QString::number(bindMap.bindset);

      if(set.pushDescriptor)
        setname = tr("Push ") + setname;

      QString slotname = QFormatStr("%1: %2").arg(bindMap.bind).arg(b.name);

      for(uint32_t a = 0; a < bind.descriptorCount; a++)
      {
        const VKPipe::BindingElement &descriptorBind = bind.binds[a];

        ResourceId id = descriptorBind.resourceResourceId;

        if(bindMap.arraySize > 1)
          slotname = QFormatStr("%1: %2[%3]").arg(bindMap.bind).arg(b.name).arg(a);

        QString name = m_Ctx.GetResourceName(id);

        BufferDescription *buf = m_Ctx.GetBuffer(id);

        TextureDescription *tex = m_Ctx.GetTexture(id);

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
          format = tex->format.Name();

          if(tex->mips > 1)
          {
            viewParams = tr("Mips: %1-%2")
                             .arg(descriptorBind.firstMip)
                             .arg(descriptorBind.firstMip + descriptorBind.numMips - 1);
          }

          if(tex->arraysize > 1)
          {
            if(!viewParams.isEmpty())
              viewParams += lit(", ");
            viewParams += tr("Layers: %1-%2")
                              .arg(descriptorBind.firstSlice)
                              .arg(descriptorBind.firstSlice + descriptorBind.numSlices - 1);
          }
        }

        if(buf)
        {
          w = buf->length;
          h = 0;
          d = 0;
          a = 0;
          format = lit("-");

          viewParams = tr("Byte Range: %1").arg(formatByteRange(buf, &descriptorBind));
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

void VulkanPipelineStateViewer::exportHTML(QXmlStreamWriter &xml, const VKPipe::TransformFeedback &xfb)
{
  {
    xml.writeStartElement(lit("h3"));
    xml.writeCharacters(tr("Transform Feedback Bindings"));
    xml.writeEndElement();

    QList<QVariantList> rows;

    int i = 0;
    for(const VKPipe::XFBBuffer &b : xfb.buffers)
    {
      QString name = m_Ctx.GetResourceName(b.bufferResourceId);
      uint64_t length = b.byteSize;
      QString counterName = m_Ctx.GetResourceName(b.counterBufferResourceId);

      if(b.bufferResourceId == ResourceId())
      {
        name = tr("Empty");
      }
      else
      {
        BufferDescription *buf = m_Ctx.GetBuffer(b.bufferResourceId);
        if(buf && length == UINT64_MAX)
          length = buf->length - b.byteOffset;
      }

      if(b.counterBufferResourceId == ResourceId())
      {
        counterName = tr("Empty");
      }

      rows.push_back({i, name, (qulonglong)b.byteOffset, (qulonglong)length, counterName,
                      (qulonglong)b.counterBufferOffset});

      i++;
    }

    m_Common.exportHTMLTable(xml, {tr("Slot"), tr("Buffer"), tr("Byte Offset"), tr("Byte Length"),
                                   tr("Counter Buffer"), tr("Counter Offset")},
                             rows);
  }
}

void VulkanPipelineStateViewer::exportHTML(QXmlStreamWriter &xml, const VKPipe::Rasterizer &rs)
{
  {
    xml.writeStartElement(lit("h3"));
    xml.writeCharacters(tr("Raster State"));
    xml.writeEndElement();

    m_Common.exportHTMLTable(
        xml, {tr("Fill Mode"), tr("Cull Mode"), tr("Front CCW")},
        {ToQStr(rs.fillMode), ToQStr(rs.cullMode), rs.frontCCW ? tr("Yes") : tr("No")});

    xml.writeStartElement(lit("p"));
    xml.writeEndElement();

    m_Common.exportHTMLTable(
        xml,
        {
            tr("Depth Clamp Enable"), tr("Depth Clip Enable"), tr("Rasterizer Discard Enable"),
        },
        {
            rs.depthClampEnable ? tr("Yes") : tr("No"), rs.depthClipEnable ? tr("Yes") : tr("No"),
            rs.rasterizerDiscardEnable ? tr("Yes") : tr("No"),
        });

    xml.writeStartElement(lit("p"));
    xml.writeEndElement();

    m_Common.exportHTMLTable(
        xml, {tr("Depth Bias"), tr("Depth Bias Clamp"), tr("Slope Scaled Bias"), tr("Line Width")},
        {Formatter::Format(rs.depthBias), Formatter::Format(rs.depthBiasClamp),
         Formatter::Format(rs.slopeScaledDepthBias), Formatter::Format(rs.lineWidth)});
  }

  const VKPipe::MultiSample &msaa = m_Ctx.CurVulkanPipelineState()->multisample;

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

  const VKPipe::ViewState &vp = m_Ctx.CurVulkanPipelineState()->viewportScissor;

  {
    xml.writeStartElement(lit("h3"));
    xml.writeCharacters(tr("Viewports"));
    xml.writeEndElement();

    QList<QVariantList> rows;

    int i = 0;
    for(const VKPipe::ViewportScissor &vs : vp.viewportScissors)
    {
      const Viewport &v = vs.vp;

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
      const Scissor &s = vs.scissor;

      rows.push_back({i, s.x, s.y, s.width, s.height});

      i++;
    }

    m_Common.exportHTMLTable(xml, {tr("Slot"), tr("X"), tr("Y"), tr("Width"), tr("Height")}, rows);
  }
}

void VulkanPipelineStateViewer::exportHTML(QXmlStreamWriter &xml, const VKPipe::ColorBlendState &cb)
{
  xml.writeStartElement(lit("h3"));
  xml.writeCharacters(tr("Color Blend State"));
  xml.writeEndElement();

  QString blendConst = QFormatStr("%1, %2, %3, %4")
                           .arg(cb.blendFactor[0], 0, 'f', 2)
                           .arg(cb.blendFactor[1], 0, 'f', 2)
                           .arg(cb.blendFactor[2], 0, 'f', 2)
                           .arg(cb.blendFactor[3], 0, 'f', 2);

  bool logic = !cb.blends.isEmpty() && cb.blends[0].logicOperationEnabled;

  m_Common.exportHTMLTable(
      xml, {tr("Alpha to Coverage"), tr("Alpha to One"), tr("Logic Op"), tr("Blend Constant")},
      {
          cb.alphaToCoverageEnable ? tr("Yes") : tr("No"),
          cb.alphaToOneEnable ? tr("Yes") : tr("No"),
          logic ? ToQStr(cb.blends[0].logicOperation) : tr("Disabled"), blendConst,
      });

  xml.writeStartElement(lit("h3"));
  xml.writeCharacters(tr("Attachment Blends"));
  xml.writeEndElement();

  QList<QVariantList> rows;

  int i = 0;
  for(const ColorBlend &b : cb.blends)
  {
    rows.push_back({i, b.enabled ? tr("Yes") : tr("No"), ToQStr(b.colorBlend.source),
                    ToQStr(b.colorBlend.destination), ToQStr(b.colorBlend.operation),
                    ToQStr(b.alphaBlend.source), ToQStr(b.alphaBlend.destination),
                    ToQStr(b.alphaBlend.operation),
                    ((b.writeMask & 0x1) == 0 ? lit("_") : lit("R")) +
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

void VulkanPipelineStateViewer::exportHTML(QXmlStreamWriter &xml, const VKPipe::DepthStencil &ds)
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
            ToQStr(ds.depthFunction), ds.depthBoundsEnable
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
          tr("Front"), Formatter::Format(ds.frontFace.reference, true),
          Formatter::Format(ds.frontFace.compareMask, true),
          Formatter::Format(ds.frontFace.writeMask, true), ToQStr(ds.frontFace.function),
          ToQStr(ds.frontFace.passOperation), ToQStr(ds.frontFace.failOperation),
          ToQStr(ds.frontFace.depthFailOperation),
      });

      rows.push_back({
          tr("back"), Formatter::Format(ds.backFace.reference, true),
          Formatter::Format(ds.backFace.compareMask, true),
          Formatter::Format(ds.backFace.writeMask, true), ToQStr(ds.backFace.function),
          ToQStr(ds.backFace.passOperation), ToQStr(ds.backFace.failOperation),
          ToQStr(ds.backFace.depthFailOperation),
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

void VulkanPipelineStateViewer::exportHTML(QXmlStreamWriter &xml, const VKPipe::CurrentPass &pass)
{
  {
    xml.writeStartElement(lit("h3"));
    xml.writeCharacters(tr("Framebuffer"));
    xml.writeEndElement();

    m_Common.exportHTMLTable(
        xml, {tr("Width"), tr("Height"), tr("Layers")},
        {pass.framebuffer.width, pass.framebuffer.height, pass.framebuffer.layers});

    xml.writeStartElement(lit("p"));
    xml.writeEndElement();

    QList<QVariantList> rows;

    int i = 0;
    for(const VKPipe::Attachment &a : pass.framebuffer.attachments)
    {
      TextureDescription *tex = m_Ctx.GetTexture(a.imageResourceId);

      QString name = m_Ctx.GetResourceName(a.imageResourceId);

      rows.push_back({i, name, a.firstMip, a.numMips, a.firstSlice, a.numSlices});

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

    if(!pass.renderpass.inputAttachments.isEmpty())
    {
      QList<QVariantList> inputs;

      for(int i = 0; i < pass.renderpass.inputAttachments.count(); i++)
        inputs.push_back({pass.renderpass.inputAttachments[i]});

      m_Common.exportHTMLTable(xml,
                               {
                                   tr("Input Attachment"),
                               },
                               inputs);

      xml.writeStartElement(lit("p"));
      xml.writeEndElement();
    }

    if(!pass.renderpass.colorAttachments.isEmpty())
    {
      QList<QVariantList> colors;

      for(int i = 0; i < pass.renderpass.colorAttachments.count(); i++)
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

    if(pass.renderpass.fragmentDensityAttachment >= 0)
    {
      xml.writeStartElement(lit("p"));
      xml.writeCharacters(
          tr("Fragment Density Attachment: %1").arg(pass.renderpass.fragmentDensityAttachment));
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

void VulkanPipelineStateViewer::exportHTML(QXmlStreamWriter &xml,
                                           const VKPipe::ConditionalRendering &cr)
{
  if(cr.bufferId == ResourceId())
    return;

  xml.writeStartElement(lit("h3"));
  xml.writeCharacters(tr("Conditional Rendering"));
  xml.writeEndElement();

  QString bufferName = m_Ctx.GetResourceName(cr.bufferId);

  m_Common.exportHTMLTable(
      xml, {tr("Predicate Passing"), tr("Is Inverted"), tr("Buffer"), tr("Byte Offset")},
      {
          cr.isPassing ? tr("Yes") : tr("No"), cr.isInverted ? tr("Yes") : tr("No"), bufferName,
          (qulonglong)cr.byteOffset,
      });
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
          exportHTML(xml, m_Ctx.CurVulkanPipelineState()->inputAssembly);

          xml.writeStartElement(lit("h2"));
          xml.writeCharacters(tr("Vertex Input"));
          xml.writeEndElement();
          exportHTML(xml, m_Ctx.CurVulkanPipelineState()->vertexInput);
          break;
        case 1: exportHTML(xml, m_Ctx.CurVulkanPipelineState()->vertexShader); break;
        case 2: exportHTML(xml, m_Ctx.CurVulkanPipelineState()->tessControlShader); break;
        case 3: exportHTML(xml, m_Ctx.CurVulkanPipelineState()->tessEvalShader); break;
        case 4:
          exportHTML(xml, m_Ctx.CurVulkanPipelineState()->geometryShader);
          exportHTML(xml, m_Ctx.CurVulkanPipelineState()->transformFeedback);
          break;
        case 5:
          exportHTML(xml, m_Ctx.CurVulkanPipelineState()->rasterizer);
          exportHTML(xml, m_Ctx.CurVulkanPipelineState()->conditionalRendering);
          break;
        case 6: exportHTML(xml, m_Ctx.CurVulkanPipelineState()->fragmentShader); break;
        case 7:
          // FB
          xml.writeStartElement(lit("h2"));
          xml.writeCharacters(tr("Color Blend"));
          xml.writeEndElement();
          exportHTML(xml, m_Ctx.CurVulkanPipelineState()->colorBlend);

          xml.writeStartElement(lit("h2"));
          xml.writeCharacters(tr("Depth Stencil"));
          xml.writeEndElement();
          exportHTML(xml, m_Ctx.CurVulkanPipelineState()->depthStencil);

          xml.writeStartElement(lit("h2"));
          xml.writeCharacters(tr("Current Pass"));
          xml.writeEndElement();
          exportHTML(xml, m_Ctx.CurVulkanPipelineState()->currentPass);
          break;
        case 8:
          exportHTML(xml, m_Ctx.CurVulkanPipelineState()->computeShader);
          exportHTML(xml, m_Ctx.CurVulkanPipelineState()->conditionalRendering);
          break;
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