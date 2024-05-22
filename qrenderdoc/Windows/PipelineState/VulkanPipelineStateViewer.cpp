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

#include "VulkanPipelineStateViewer.h"
#include <float.h>
#include <QJsonDocument>
#include <QMenu>
#include <QMouseEvent>
#include <QScrollBar>
#include <QXmlStreamWriter>
#include "Code/Resources.h"
#include "Widgets/ComputeDebugSelector.h"
#include "Widgets/Extended/RDHeaderView.h"
#include "flowlayout/FlowLayout.h"
#include "toolwindowmanager/ToolWindowManager.h"
#include "PipelineStateViewer.h"
#include "ui_VulkanPipelineStateViewer.h"

Q_DECLARE_METATYPE(CombinedSamplerData);

namespace
{
QString getTextureRenderSamples(const TextureDescription *tex, const VKPipe::RenderPass &renderpass)
{
  const uint32_t texSamples = tex ? tex->msSamp : 1;
  const uint32_t renderSamples = renderpass.tileOnlyMSAASampleCount;

  QString result = lit("%1x").arg(std::max(texSamples, renderSamples));

  // With VK_EXT_multisampled_render_to_single_sampled, attachments can either have N or 1 samples,
  // where N is the same number of samples specified for MSRTSS.  Attachments with Nx samples are
  // rendered to normally, while 1x ones are implicitly rendered with N samples.  The latter are
  // specifically tagged as such.
  if(renderSamples > 1 && texSamples == 1)
  {
    result += lit(" (tile-only)");
  }

  return result;
}
}    // namespace

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
  VulkanCBufferTag() { index = DescriptorAccess::NoShaderBinding; }
  VulkanCBufferTag(uint32_t index, uint32_t arrayElement) : index(index), arrayElement(arrayElement)
  {
  }
  VulkanCBufferTag(Descriptor descriptor)
      : index(DescriptorAccess::NoShaderBinding), arrayElement(0), descriptor(descriptor)
  {
  }

  Descriptor descriptor;
  uint32_t index, arrayElement;
};

Q_DECLARE_METATYPE(VulkanCBufferTag);

struct VulkanBufferTag
{
  VulkanBufferTag() {}
  VulkanBufferTag(const DescriptorAccess &access, const Descriptor &desc)
      : access(access), descriptor(desc)
  {
  }
  VulkanBufferTag(ResourceId id, uint64_t offset, uint64_t length)
  {
    access.index = DescriptorAccess::NoShaderBinding;
    descriptor.resource = id;
    descriptor.byteOffset = offset;
    descriptor.byteSize = length;
  }
  DescriptorAccess access;
  Descriptor descriptor;
};

Q_DECLARE_METATYPE(VulkanBufferTag);

struct VulkanTextureTag
{
  VulkanTextureTag() { compType = CompType::Typeless; }
  VulkanTextureTag(ResourceId id, CompType ty)
  {
    ID = id;
    compType = ty;
  }
  ResourceId ID;
  CompType compType;
};

Q_DECLARE_METATYPE(VulkanTextureTag);

VulkanPipelineStateViewer::VulkanPipelineStateViewer(ICaptureContext &ctx,
                                                     PipelineStateViewer &common, QWidget *parent)
    : QFrame(parent), ui(new Ui::VulkanPipelineStateViewer), m_Ctx(ctx), m_Common(common)
{
  ui->setupUi(this);

  m_ComputeDebugSelector = new ComputeDebugSelector(this);

  const QIcon &action = Icons::action();
  const QIcon &action_hover = Icons::action_hover();

  RDLabel *shaderLabels[] = {
      ui->tsShader,  ui->msShader, ui->vsShader, ui->tcsShader,
      ui->tesShader, ui->gsShader, ui->fsShader, ui->csShader,
  };

  RDLabel *pipeLayoutLabels[] = {
      ui->tsPipeLayout,  ui->msPipeLayout, ui->vsPipeLayout, ui->tcsPipeLayout,
      ui->tesPipeLayout, ui->gsPipeLayout, ui->fsPipeLayout, ui->csPipeLayout,
  };

  QToolButton *viewButtons[] = {
      ui->tsShaderViewButton,  ui->msShaderViewButton,  ui->vsShaderViewButton,
      ui->tcsShaderViewButton, ui->tesShaderViewButton, ui->gsShaderViewButton,
      ui->fsShaderViewButton,  ui->csShaderViewButton,
  };

  QToolButton *editButtons[] = {
      ui->tsShaderEditButton,  ui->msShaderEditButton,  ui->vsShaderEditButton,
      ui->tcsShaderEditButton, ui->tesShaderEditButton, ui->gsShaderEditButton,
      ui->fsShaderEditButton,  ui->csShaderEditButton,
  };

  QToolButton *saveButtons[] = {
      ui->tsShaderSaveButton,  ui->msShaderSaveButton,  ui->vsShaderSaveButton,
      ui->tcsShaderSaveButton, ui->tesShaderSaveButton, ui->gsShaderSaveButton,
      ui->fsShaderSaveButton,  ui->csShaderSaveButton,
  };

  QToolButton *messageButtons[] = {
      ui->tsShaderMessagesButton,  ui->msShaderMessagesButton,  ui->vsShaderMessagesButton,
      ui->tcsShaderMessagesButton, ui->tesShaderMessagesButton, ui->gsShaderMessagesButton,
      ui->fsShaderMessagesButton,  ui->csShaderMessagesButton,
  };

  QToolButton *viewPredicateBufferButtons[] = {
      ui->predicateBufferViewButton,
      ui->csPredicateBufferViewButton,
  };

  RDTreeWidget *resources[] = {
      ui->tsResources,  ui->msResources, ui->vsResources, ui->tcsResources,
      ui->tesResources, ui->gsResources, ui->fsResources, ui->csResources,
  };

  RDTreeWidget *ubos[] = {
      ui->tsUBOs,  ui->msUBOs, ui->vsUBOs, ui->tcsUBOs,
      ui->tesUBOs, ui->gsUBOs, ui->fsUBOs, ui->csUBOs,
  };

  RDTreeWidget *descSets[] = {
      ui->tsDescSets,  ui->msDescSets, ui->vsDescSets, ui->tcsDescSets,
      ui->tesDescSets, ui->gsDescSets, ui->fsDescSets, ui->csDescSets,
  };

  // setup FlowLayout for CS shader group, with debugging controls
  {
    QLayout *oldLayout = ui->csShaderGroup->layout();

    QObjectList childs = ui->csShaderGroup->children();
    childs.removeOne((QObject *)oldLayout);

    delete oldLayout;

    FlowLayout *csShaderFlow = new FlowLayout(ui->csShaderGroup, -1, 3, 3);

    for(QObject *o : childs)
      csShaderFlow->addWidget(qobject_cast<QWidget *>(o));

    ui->csShaderGroup->setLayout(csShaderFlow);
  }

  for(QToolButton *b : viewButtons)
    QObject::connect(b, &QToolButton::clicked, this, &VulkanPipelineStateViewer::shaderView_clicked);

  for(RDLabel *b : shaderLabels)
  {
    b->setAutoFillBackground(true);
    b->setBackgroundRole(QPalette::ToolTipBase);
    b->setForegroundRole(QPalette::ToolTipText);
    b->setMinimumSizeHint(QSize(250, 0));
    b->setFont(Formatter::PreferredFont());
  }

  for(RDLabel *b : pipeLayoutLabels)
  {
    b->setAutoFillBackground(true);
    b->setBackgroundRole(QPalette::ToolTipBase);
    b->setForegroundRole(QPalette::ToolTipText);
    b->setMinimumSizeHint(QSize(250, ui->vsShaderViewButton->minimumSizeHint().height()));
    b->setFont(Formatter::PreferredFont());
  }

  // collapse the descriptor groups by default
  ui->vsDescGroup->setCollapsed(true);
  ui->tcsDescGroup->setCollapsed(true);
  ui->tesDescGroup->setCollapsed(true);
  ui->gsDescGroup->setCollapsed(true);
  ui->fsDescGroup->setCollapsed(true);
  ui->csDescGroup->setCollapsed(true);
  ui->tsDescGroup->setCollapsed(true);
  ui->msDescGroup->setCollapsed(true);

  QObject::connect(m_ComputeDebugSelector, &ComputeDebugSelector::beginDebug, this,
                   &VulkanPipelineStateViewer::computeDebugSelector_beginDebug);

  for(QToolButton *b : editButtons)
    QObject::connect(b, &QToolButton::clicked, &m_Common, &PipelineStateViewer::shaderEdit_clicked);

  for(QToolButton *b : saveButtons)
    QObject::connect(b, &QToolButton::clicked, this, &VulkanPipelineStateViewer::shaderSave_clicked);

  for(QToolButton *b : messageButtons)
    QObject::connect(b, &QToolButton::clicked, this,
                     &VulkanPipelineStateViewer::shaderMessages_clicked);

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
  {
    QObject::connect(res, &RDTreeWidget::itemActivated, this,
                     &VulkanPipelineStateViewer::resource_itemActivated);
    QObject::connect(res, &RDTreeWidget::hoverItemChanged, this,
                     &VulkanPipelineStateViewer::resource_hoverItemChanged);
  }

  for(RDTreeWidget *ubo : ubos)
    QObject::connect(ubo, &RDTreeWidget::itemActivated, this,
                     &VulkanPipelineStateViewer::ubo_itemActivated);

  for(RDTreeWidget *desc : descSets)
    QObject::connect(desc, &RDTreeWidget::itemActivated, this,
                     &VulkanPipelineStateViewer::descSet_itemActivated);

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

    m_Common.SetupResourceView(ui->viBuffers);
  }

  for(RDTreeWidget *res : resources)
  {
    RDHeaderView *header = new RDHeaderView(Qt::Horizontal, this);
    res->setHeader(header);

    res->setColumns(
        {tr("Binding"), tr("Type"), tr("Resource"), tr("Contents"), tr("Additional"), tr("Go")});
    header->setColumnStretchHints({2, 2, 2, 4, 4, -1});

    res->setHoverIconColumn(5, action, action_hover);
    res->setClearSelectionOnFocusLoss(true);
    res->setInstantTooltips(true);

    m_Common.SetupResourceView(res);
  }

  for(RDTreeWidget *ubo : ubos)
  {
    RDHeaderView *header = new RDHeaderView(Qt::Horizontal, this);
    ubo->setHeader(header);

    ubo->setColumns({tr("Binding"), tr("Buffer"), tr("Byte Range"), tr("Size"), tr("Go")});
    header->setColumnStretchHints({2, 4, 3, 3, -1});

    ubo->setHoverIconColumn(4, action, action_hover);
    ubo->setClearSelectionOnFocusLoss(true);
    ubo->setInstantTooltips(true);

    m_Common.SetupResourceView(ubo);
  }

  for(RDTreeWidget *desc : descSets)
  {
    RDHeaderView *header = new RDHeaderView(Qt::Horizontal, this);
    desc->setHeader(header);

    desc->setColumns({tr("Index"), tr("Layout"), tr("Bound Set"), tr("Go")});
    header->setColumnStretchHints({-1, 4, 4, -1});

    desc->setHoverIconColumn(3, action, action_hover);
    desc->setClearSelectionOnFocusLoss(true);
    desc->setInstantTooltips(true);

    m_Common.SetupResourceView(desc);
  }

  ui->vsDescGroupVLayout->activate();

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

    m_Common.SetupResourceView(ui->xfbBuffers);
  }

  {
    RDHeaderView *header = new RDHeaderView(Qt::Horizontal, this);
    ui->viewports->setHeader(header);

    ui->viewports->setColumns({tr("Slot"), tr("X"), tr("Y"), tr("Width"), tr("Height"),
                               tr("MinDepth"), tr("MaxDepth"), tr("NDCDepthRange")});
    header->setColumnStretchHints({-1, -1, -1, -1, -1, -1, -1, 1});
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

    ui->fbAttach->setColumns({tr("Slot"), tr("Resource"), tr("Type"), tr("Dimensions"),
                              tr("Format"), tr("Samples"), tr("Go")});
    header->setColumnStretchHints({2, 4, 2, 2, 3, 1, -1});

    ui->fbAttach->setHoverIconColumn(6, action, action_hover);
    ui->fbAttach->setClearSelectionOnFocusLoss(true);
    ui->fbAttach->setInstantTooltips(true);

    m_Common.SetupResourceView(ui->fbAttach);
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

  setOldMeshPipeFlow();
  ui->pipeFlow->setStagesEnabled({true, true, true, true, true, true, true, true, true});

  m_Common.setMeshViewPixmap(ui->meshView);

  ui->viAttrs->setFont(Formatter::PreferredFont());
  ui->viBuffers->setFont(Formatter::PreferredFont());
  ui->tsResources->setFont(Formatter::PreferredFont());
  ui->tsUBOs->setFont(Formatter::PreferredFont());
  ui->msResources->setFont(Formatter::PreferredFont());
  ui->msUBOs->setFont(Formatter::PreferredFont());
  ui->vsResources->setFont(Formatter::PreferredFont());
  ui->vsUBOs->setFont(Formatter::PreferredFont());
  ui->gsResources->setFont(Formatter::PreferredFont());
  ui->gsUBOs->setFont(Formatter::PreferredFont());
  ui->tcsResources->setFont(Formatter::PreferredFont());
  ui->tcsUBOs->setFont(Formatter::PreferredFont());
  ui->tesResources->setFont(Formatter::PreferredFont());
  ui->tesUBOs->setFont(Formatter::PreferredFont());
  ui->fsResources->setFont(Formatter::PreferredFont());
  ui->fsUBOs->setFont(Formatter::PreferredFont());
  ui->csResources->setFont(Formatter::PreferredFont());
  ui->csUBOs->setFont(Formatter::PreferredFont());
  ui->xfbBuffers->setFont(Formatter::PreferredFont());
  ui->viewports->setFont(Formatter::PreferredFont());
  ui->scissors->setFont(Formatter::PreferredFont());
  ui->renderpass->setFont(Formatter::PreferredFont());
  ui->framebuffer->setFont(Formatter::PreferredFont());
  ui->fbAttach->setFont(Formatter::PreferredFont());
  ui->blends->setFont(Formatter::PreferredFont());

  m_ExportMenu = new QMenu(this);

  m_ExportHTML = new QAction(tr("Export current state to &HTML"), this);
  m_ExportHTML->setIcon(Icons::save());
  m_ExportFOZ = new QAction(tr("Export to &Fossilize database"), this);
  m_ExportFOZ->setIcon(Icons::save());

  m_ExportMenu->addAction(m_ExportHTML);
  m_ExportMenu->addAction(m_ExportFOZ);

  ui->exportDrop->setMenu(m_ExportMenu);

  QObject::connect(m_ExportHTML, &QAction::triggered, this,
                   &VulkanPipelineStateViewer::exportHTML_clicked);
  QObject::connect(m_ExportFOZ, &QAction::triggered, this,
                   &VulkanPipelineStateViewer::exportFOZ_clicked);

  QObject::connect(ui->exportDrop, &QToolButton::clicked, this,
                   &VulkanPipelineStateViewer::exportHTML_clicked);

  // reset everything back to defaults
  clearState();
}

VulkanPipelineStateViewer::~VulkanPipelineStateViewer()
{
  m_CombinedImageSamplers.clear();
  delete ui;
  delete m_ComputeDebugSelector;
}

void VulkanPipelineStateViewer::OnCaptureLoaded()
{
  OnEventChanged(m_Ctx.CurEvent());
}

void VulkanPipelineStateViewer::OnCaptureClosed()
{
  setOldMeshPipeFlow();
  ui->pipeFlow->setStagesEnabled({true, true, true, true, true, true, true, true, true});

  clearState();
}

void VulkanPipelineStateViewer::OnEventChanged(uint32_t eventId)
{
  setState();
}

void VulkanPipelineStateViewer::SelectPipelineStage(PipelineStage stage)
{
  if(stage == PipelineStage::SampleMask)
    ui->pipeFlow->setSelectedStage((int)PipelineStage::Rasterizer);
  else
    ui->pipeFlow->setSelectedStage((int)stage);
}

ResourceId VulkanPipelineStateViewer::GetResource(RDTreeWidgetItem *item)
{
  QVariant tag = item->tag();

  if(tag.canConvert<ResourceId>())
  {
    return tag.value<ResourceId>();
  }
  else if(tag.canConvert<VulkanTextureTag>())
  {
    VulkanTextureTag texTag = tag.value<VulkanTextureTag>();
    return texTag.ID;
  }
  else if(tag.canConvert<VulkanBufferTag>())
  {
    VulkanBufferTag buf = tag.value<VulkanBufferTag>();
    return buf.descriptor.resource;
  }
  else if(tag.canConvert<VulkanVBIBTag>())
  {
    VulkanVBIBTag buf = tag.value<VulkanVBIBTag>();
    return buf.id;
  }
  else if(tag.canConvert<VulkanCBufferTag>())
  {
    const VKPipe::Shader *stage = stageForSender(item->treeWidget());

    if(stage == NULL)
      return ResourceId();

    VulkanCBufferTag cb = tag.value<VulkanCBufferTag>();

    if(cb.index == DescriptorAccess::NoShaderBinding)
      return cb.descriptor.resource;

    return m_Ctx.CurPipelineState()
        .GetConstantBlock(stage->stage, cb.index, cb.arrayElement)
        .descriptor.resource;
  }

  return ResourceId();
}

void VulkanPipelineStateViewer::on_showUnused_toggled(bool checked)
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

bool VulkanPipelineStateViewer::setViewDetails(RDTreeWidgetItem *node, const Descriptor &descriptor,
                                               TextureDescription *tex,
                                               const QString &hiddenCombinedSampler,
                                               bool includeSampleLocations, bool includeOffsets)
{
  if(tex == NULL)
    return false;

  QString text;

  bool viewdetails = false;

  const VKPipe::State &state = *m_Ctx.CurVulkanPipelineState();

  {
    for(const VKPipe::ImageData &im : state.images)
    {
      if(im.resourceId == tex->resourceId)
      {
        text += tr("Texture is in the '%1' layout\n").arg(im.layouts[0].name);
        break;
      }
    }

    text += hiddenCombinedSampler;

    text += lit("\n");

    if(descriptor.format != tex->format)
    {
      text += tr("The texture is format %1, the view treats it as %2.\n")
                  .arg(tex->format.Name())
                  .arg(descriptor.format.Name());

      viewdetails = true;
    }

    if(tex->mips > 1 && (tex->mips != descriptor.numMips || descriptor.firstMip > 0))
    {
      if(descriptor.numMips == 1)
        text += tr("The texture has %1 mips, the view covers mip %2.\n")
                    .arg(tex->mips)
                    .arg(descriptor.firstMip);
      else
        text += tr("The texture has %1 mips, the view covers mips %2-%3.\n")
                    .arg(tex->mips)
                    .arg(descriptor.firstMip)
                    .arg(descriptor.firstMip + descriptor.numMips - 1);

      viewdetails = true;
    }

    if(tex->arraysize > 1 && (tex->arraysize != descriptor.numSlices || descriptor.firstSlice > 0))
    {
      if(descriptor.numSlices == 1)
        text += tr("The texture has %1 array slices, the view covers slice %2.\n")
                    .arg(tex->arraysize)
                    .arg(descriptor.firstSlice);
      else
        text += tr("The texture has %1 array slices, the view covers slices %2-%3.\n")
                    .arg(tex->arraysize)
                    .arg(descriptor.firstSlice)
                    .arg(descriptor.firstSlice + descriptor.numSlices - 1);

      viewdetails = true;
    }

    if(tex->depth > 1 && ((tex->depth != descriptor.numSlices && descriptor.numSlices > 0) ||
                          descriptor.firstSlice > 0))
    {
      if(descriptor.numSlices == 1)
        text += tr("The texture has %1 3D slices, the view covers slice %2.\n")
                    .arg(tex->depth)
                    .arg(descriptor.firstSlice);
      else
        text += tr("The texture has %1 3D slices, the view covers slices %2-%3.\n")
                    .arg(tex->depth)
                    .arg(descriptor.firstSlice)
                    .arg(descriptor.firstSlice + descriptor.numSlices - 1);

      viewdetails = true;
    }
  }

  if(descriptor.minLODClamp != 0.0f)
  {
    text += tr("Clamped to a minimum LOD of %1\n").arg(descriptor.minLODClamp);
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

  if(includeOffsets)
  {
    text += tr("Rendering with %1 offsets:\n")
                .arg(state.currentPass.renderpass.fragmentDensityOffsets.size());
    for(uint32_t j = 0; j < state.currentPass.renderpass.fragmentDensityOffsets.size(); j++)
    {
      const Offset &o = state.currentPass.renderpass.fragmentDensityOffsets[j];
      if(j > 0)
        text += tr(", ");

      text += tr(" %1x%2").arg(o.x).arg(o.y);
    }
    text += lit("\n");
  }

  text = text.trimmed();

  node->setToolTip(text);

  if(viewdetails)
  {
    node->setBackgroundColor(m_Common.GetViewDetailsColor());
  }

  return viewdetails;
}

bool VulkanPipelineStateViewer::setViewDetails(RDTreeWidgetItem *node, const Descriptor &descriptor,
                                               BufferDescription *buf)
{
  if(buf == NULL)
    return false;

  QString text;

  if(descriptor.byteOffset > 0 || descriptor.byteSize < buf->length)
  {
    text += tr("The view covers bytes %1-%2.\nThe buffer is %3 bytes in length.\n")
                .arg(Formatter::HumanFormat(descriptor.byteOffset, Formatter::OffsetSize))
                .arg(Formatter::HumanFormat(descriptor.byteOffset + descriptor.byteSize,
                                            Formatter::OffsetSize))
                .arg(Formatter::HumanFormat(buf->length, Formatter::OffsetSize));
  }
  else
  {
    return false;
  }

  node->setToolTip(text);
  node->setBackgroundColor(m_Common.GetViewDetailsColor());

  return true;
}

bool VulkanPipelineStateViewer::showNode(bool usedSlot, bool filledSlot)
{
  // show if it's referenced by the shader - regardless of empty or not
  if(usedSlot)
    return true;

  // it's not referenced, but if it's bound and we have "show unused" then show it
  if(m_ShowUnused && filledSlot)
    return true;

  // it's empty, and we have "show empty"
  if(m_ShowEmpty && !filledSlot)
    return true;

  return false;
}

QString VulkanPipelineStateViewer::formatByteRange(const BufferDescription *buf,
                                                   const Descriptor &descriptor,
                                                   uint32_t dynamicOffset)
{
  if(buf == NULL)
    return lit("-");

  uint64_t byteOffset = descriptor.byteOffset + dynamicOffset;

  if(descriptor.byteSize == 0)
  {
    return tr("%1 - %2 (empty view)").arg(byteOffset).arg(byteOffset);
  }
  else if(descriptor.byteSize == UINT64_MAX)
  {
    return QFormatStr("%1 - %2 (VK_WHOLE_SIZE)")
        .arg(Formatter::HumanFormat(byteOffset, Formatter::OffsetSize))
        .arg(Formatter::HumanFormat(byteOffset + (buf->length - byteOffset), Formatter::OffsetSize));
  }
  else
  {
    return QFormatStr("%1 - %2")
        .arg(Formatter::HumanFormat(byteOffset, Formatter::OffsetSize))
        .arg(Formatter::HumanFormat(byteOffset + descriptor.byteSize, Formatter::OffsetSize));
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
    if(widget == ui->stagesTabs->widget(9))
      return &m_Ctx.CurVulkanPipelineState()->taskShader;
    if(widget == ui->stagesTabs->widget(10))
      return &m_Ctx.CurVulkanPipelineState()->meshShader;

    widget = widget->parentWidget();
  }

  qCritical() << "Unrecognised control calling event handler";

  return NULL;
}

void VulkanPipelineStateViewer::setOldMeshPipeFlow()
{
  m_MeshPipe = false;

  ui->pipeFlow->setStages(
      {
          lit("VTX"),
          lit("VS"),
          lit("TCS"),
          lit("TES"),
          lit("GS"),
          lit("RS"),
          lit("FS"),
          lit("FB"),
          lit("CS"),
      },
      {
          tr("Vertex Input"),
          tr("Vertex Shader"),
          tr("Tess. Control Shader"),
          tr("Tess. Eval. Shader"),
          tr("Geometry Shader"),
          tr("Rasterizer"),
          tr("Fragment Shader"),
          tr("Framebuffer Output"),
          tr("Compute Shader"),
      });

  ui->pipeFlow->setIsolatedStage(8);    // compute shader isolated
}

void VulkanPipelineStateViewer::setNewMeshPipeFlow()
{
  m_MeshPipe = true;

  ui->pipeFlow->setStages(
      {
          lit("TS"),
          lit("MS"),
          lit("RS"),
          lit("FS"),
          lit("FB"),
          lit("CS"),
      },
      {
          tr("Task Shader"),
          tr("Mesh Shader"),
          tr("Rasterizer"),
          tr("Fragment Shader"),
          tr("Framebuffer Output"),
          tr("Compute Shader"),
      });

  ui->pipeFlow->setIsolatedStage(5);    // compute shader isolated
}

void VulkanPipelineStateViewer::clearShaderState(RDLabel *shader, RDLabel *pipeLayout,
                                                 RDTreeWidget *resources, RDTreeWidget *cbuffers,
                                                 RDTreeWidget *descSets)
{
  pipeLayout->setText(tr("Pipeline Layout"));
  shader->setText(QFormatStr("%1: %1").arg(ToQStr(ResourceId())));
  resources->clear();
  cbuffers->clear();
  descSets->clear();
}

void VulkanPipelineStateViewer::clearState()
{
  m_CombinedImageSamplers.clear();

  m_VBNodes.clear();
  m_BindNodes.clear();
  m_EmptyNodes.clear();

  ui->viAttrs->clear();
  ui->viBuffers->clear();
  ui->topology->setText(QString());
  ui->primRestart->setVisible(false);
  ui->topologyDiagram->setPixmap(QPixmap());

  clearShaderState(ui->tsShader, ui->tsPipeLayout, ui->tsResources, ui->tsUBOs, ui->tsDescSets);
  clearShaderState(ui->msShader, ui->msPipeLayout, ui->msResources, ui->msUBOs, ui->msDescSets);
  clearShaderState(ui->vsShader, ui->vsPipeLayout, ui->vsResources, ui->vsUBOs, ui->vsDescSets);
  clearShaderState(ui->tcsShader, ui->tcsPipeLayout, ui->tcsResources, ui->tcsUBOs, ui->tcsDescSets);
  clearShaderState(ui->tesShader, ui->tesPipeLayout, ui->tesResources, ui->tesUBOs, ui->tesDescSets);
  clearShaderState(ui->gsShader, ui->gsPipeLayout, ui->gsResources, ui->gsUBOs, ui->gsDescSets);
  clearShaderState(ui->fsShader, ui->fsPipeLayout, ui->fsResources, ui->fsUBOs, ui->fsDescSets);
  clearShaderState(ui->csShader, ui->csPipeLayout, ui->csResources, ui->csUBOs, ui->csDescSets);

  ui->xfbBuffers->clear();

  QToolButton *shaderButtons[] = {
      // view buttons
      ui->tsShaderViewButton,
      ui->msShaderViewButton,
      ui->vsShaderViewButton,
      ui->tcsShaderViewButton,
      ui->tesShaderViewButton,
      ui->gsShaderViewButton,
      ui->fsShaderViewButton,
      ui->csShaderViewButton,
      // edit buttons
      ui->tsShaderEditButton,
      ui->msShaderEditButton,
      ui->vsShaderEditButton,
      ui->tcsShaderEditButton,
      ui->tesShaderEditButton,
      ui->gsShaderEditButton,
      ui->fsShaderEditButton,
      ui->csShaderEditButton,
      // save buttons
      ui->tsShaderSaveButton,
      ui->msShaderSaveButton,
      ui->vsShaderSaveButton,
      ui->tcsShaderSaveButton,
      ui->tesShaderSaveButton,
      ui->gsShaderSaveButton,
      ui->fsShaderSaveButton,
      ui->csShaderSaveButton,
  };

  for(QToolButton *b : shaderButtons)
    b->setEnabled(false);

  QToolButton *messageButtons[] = {
      ui->tsShaderMessagesButton,  ui->msShaderMessagesButton,  ui->vsShaderMessagesButton,
      ui->tcsShaderMessagesButton, ui->tesShaderMessagesButton, ui->gsShaderMessagesButton,
      ui->fsShaderMessagesButton,  ui->csShaderMessagesButton,
  };

  for(QToolButton *b : messageButtons)
    b->setVisible(false);

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
  ui->multiview->setText(tr("Disabled"));

  ui->stippleFactor->setText(QString());
  ui->stippleFactor->setPixmap(cross);
  ui->stipplePattern->setText(QString());
  ui->stipplePattern->setPixmap(cross);

  ui->pipelineShadingRate->setText(tr("1x1"));
  ui->shadingRateCombiners->setText(tr("Keep, Keep"));
  ui->provokingVertex->setText(tr("First"));

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

  ui->computeDebugSelector->setEnabled(false);

  ui->conditionalRenderingGroup->setVisible(false);
  ui->csConditionalRenderingGroup->setVisible(false);
}

QVariantList VulkanPipelineStateViewer::makeSampler(const QString &slotname,
                                                    const SamplerDescriptor &descriptor)
{
  QString addressing;
  QString addPrefix;
  QString addVal;

  QString filter;

  QString addr[] = {ToQStr(descriptor.addressU, GraphicsAPI::Vulkan),
                    ToQStr(descriptor.addressV, GraphicsAPI::Vulkan),
                    ToQStr(descriptor.addressW, GraphicsAPI::Vulkan)};

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
  {
    if(descriptor.borderColorType == CompType::Float)
      addressing += QFormatStr(" <%1, %2, %3, %4>")
                        .arg(descriptor.borderColorValue.floatValue[0])
                        .arg(descriptor.borderColorValue.floatValue[1])
                        .arg(descriptor.borderColorValue.floatValue[2])
                        .arg(descriptor.borderColorValue.floatValue[3]);
    else
      addressing += QFormatStr(" <%1, %2, %3, %4>")
                        .arg(descriptor.borderColorValue.uintValue[0])
                        .arg(descriptor.borderColorValue.uintValue[1])
                        .arg(descriptor.borderColorValue.uintValue[2])
                        .arg(descriptor.borderColorValue.uintValue[3]);
  }

  if(descriptor.unnormalized)
    addressing += lit(" (Un-norm)");

  filter = ToQStr(descriptor.filter);

  if(descriptor.maxAnisotropy >= 1.0f)
    filter += lit(" Aniso %1x").arg(descriptor.maxAnisotropy);

  if(descriptor.filter.filter == FilterFunction::Comparison)
    filter += QFormatStr(" (%1)").arg(ToQStr(descriptor.compareFunction));
  else if(descriptor.filter.filter != FilterFunction::Normal)
    filter += QFormatStr(" (%1)").arg(ToQStr(descriptor.filter.filter));

  QString minLOD = QString::number(descriptor.minLOD);
  QString maxLOD = QString::number(descriptor.maxLOD);

  if(descriptor.minLOD == -FLT_MAX)
    minLOD = lit("0");
  if(descriptor.minLOD == -1000.0)
    minLOD = lit("VK_LOD_CLAMP_NONE");

  if(descriptor.maxLOD == FLT_MAX)
    maxLOD = lit("FLT_MAX");
  if(descriptor.maxLOD == 1000.0)
    maxLOD = lit("VK_LOD_CLAMP_NONE");

  QString lod = lit("LODs: %1 - %2").arg(minLOD).arg(maxLOD);

  if(descriptor.mipBias != 0.0f)
    lod += lit(" Bias %1").arg(descriptor.mipBias);

  if(!lod.isEmpty())
    lod = lit(", ") + lod;

  QString obj = ToQStr(descriptor.object);

  if(descriptor.swizzle.red != TextureSwizzle::Red ||
     descriptor.swizzle.green != TextureSwizzle::Green ||
     descriptor.swizzle.blue != TextureSwizzle::Blue ||
     descriptor.swizzle.alpha != TextureSwizzle::Alpha)
  {
    obj += tr(" swizzle[%1%2%3%4]")
               .arg(ToQStr(descriptor.swizzle.red))
               .arg(ToQStr(descriptor.swizzle.green))
               .arg(ToQStr(descriptor.swizzle.blue))
               .arg(ToQStr(descriptor.swizzle.alpha));
  }

  if(!descriptor.seamlessCubemaps)
    addressing += tr(" Non-Seamless");

  if(descriptor.ycbcrSampler != ResourceId())
  {
    obj += lit(" ") + ToQStr(descriptor.ycbcrSampler);

    filter +=
        QFormatStr(", %1 %2").arg(ToQStr(descriptor.ycbcrModel)).arg(ToQStr(descriptor.ycbcrRange));

    addressing += tr(", Chroma %1 [%2,%3]")
                      .arg(ToQStr(descriptor.chromaFilter))
                      .arg(ToQStr(descriptor.xChromaOffset))
                      .arg(ToQStr(descriptor.yChromaOffset));

    if(descriptor.forceExplicitReconstruction)
      addressing += tr(" Explicit");
  }

  return {slotname,     descriptor.creationTimeConstant ? tr("Immutable Sampler") : tr("Sampler"),
          obj,          addressing,
          filter + lod, QString()};
}

void VulkanPipelineStateViewer::addResourceRow(const ShaderResource *shaderRes,
                                               const ShaderSampler *shaderSamp,
                                               const UsedDescriptor &used, uint32_t dynamicOffset,
                                               RDTreeWidget *resources,
                                               QMap<ResourceId, RDTreeWidgetItem *> &samplers)
{
  const Descriptor &descriptor = used.descriptor;
  const SamplerDescriptor &samplerDescriptor = used.sampler;

  bool filledSlot = (descriptor.resource != ResourceId() || samplerDescriptor.object != ResourceId() ||
                     samplerDescriptor.creationTimeConstant);
  // Vulkan does not report unused elements at all because we enumerate exclusively from the
  // perspective of which descriptors are used
  bool usedSlot = true;

  if(showNode(usedSlot, filledSlot))
  {
    QString slotname;
    if(used.access.index == DescriptorAccess::NoShaderBinding)
    {
      slotname = m_Locations[{used.access.descriptorStore, used.access.byteOffset}].logicalBindName;

      slotname += QFormatStr("[%1]").arg(used.access.arrayElement);
    }
    else if(shaderRes)
    {
      if(IsPushSet(used.access.stage, used.access.descriptorStore))
        slotname = tr("Push ");

      slotname +=
          QFormatStr("Set %1, %2").arg(shaderRes->fixedBindSetOrSpace).arg(shaderRes->fixedBindNumber);

      if(!shaderRes->name.empty())
        slotname += lit(": ") + shaderRes->name;

      if(shaderRes->bindArraySize > 1)
        slotname += QFormatStr("[%1]").arg(used.access.arrayElement);
    }
    else if(shaderSamp)
    {
      if(IsPushSet(used.access.stage, used.access.descriptorStore))
        slotname = tr("Push ");

      slotname +=
          QFormatStr("Set %1, %2").arg(shaderSamp->fixedBindSetOrSpace).arg(shaderSamp->fixedBindNumber);

      if(!shaderSamp->name.empty())
        slotname += lit(": ") + shaderSamp->name;

      if(shaderSamp->bindArraySize > 1)
        slotname += QFormatStr("[%1]").arg(used.access.arrayElement);
    }

    bool isbuf = false;
    uint32_t w = 1, h = 1, d = 1;
    uint32_t a = 1;
    uint32_t samples = 1;
    uint64_t resourceByteSize = 0;
    QString format = descriptor.format.Name();
    TextureType restype = TextureType::Unknown;
    QVariant tag;

    TextureDescription *tex = NULL;
    BufferDescription *buf = NULL;

    if(descriptor.resource != ResourceId())
    {
      // check to see if it's a texture
      tex = m_Ctx.GetTexture(descriptor.resource);
      if(tex)
      {
        w = tex->width;
        h = tex->height;
        d = tex->depth;
        a = tex->arraysize;
        restype = tex->type;
        samples = tex->msSamp;

        tag = QVariant::fromValue(VulkanTextureTag(descriptor.resource, descriptor.format.compType));
      }

      // if not a texture, it must be a buffer
      buf = m_Ctx.GetBuffer(descriptor.resource);
      if(buf)
      {
        resourceByteSize = buf->length;
        w = 0;
        h = 0;
        d = 0;
        a = 0;
        restype = TextureType::Buffer;

        tag = QVariant::fromValue(VulkanBufferTag(used.access, used.descriptor));

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

    QString bindType = ToQStr(used.access.type);

    if(shaderRes && shaderRes->isInputAttachment)
      bindType = tr("Input Attachment");

    if(used.access.type == DescriptorType::ReadWriteBuffer)
    {
      if(!isbuf)
      {
        node = new RDTreeWidgetItem({
            slotname,
            bindType,
            ResourceId(),
            lit("-"),
            QString(),
            QString(),
        });

        setEmptyRow(node);
      }
      else
      {
        node = new RDTreeWidgetItem({
            slotname,
            bindType,
            descriptor.resource,
            tr("%1 bytes").arg(Formatter::HumanFormat(resourceByteSize, Formatter::OffsetSize)),
            QFormatStr("Viewing bytes %1").arg(formatByteRange(buf, descriptor, dynamicOffset)),
            QString(),
        });

        node->setTag(tag);

        if(!filledSlot)
          setEmptyRow(node);
      }
    }
    else if(used.access.type == DescriptorType::TypedBuffer ||
            used.access.type == DescriptorType::ReadWriteTypedBuffer)
    {
      node = new RDTreeWidgetItem({
          slotname,
          bindType,
          descriptor.resource,
          format,
          QFormatStr("bytes %1").arg(formatByteRange(buf, descriptor, dynamicOffset)),
          QString(),
      });

      node->setTag(tag);

      if(!filledSlot)
        setEmptyRow(node);
    }
    else if(used.access.type == DescriptorType::AccelerationStructure)
    {
      node = new RDTreeWidgetItem({
          slotname,
          bindType,
          descriptor.resource,
          QString(),
          QFormatStr("%1 bytes").arg(Formatter::HumanFormat(descriptor.byteSize, Formatter::OffsetSize)),
          QString(),
      });

      node->setTag(tag);

      if(!filledSlot)
        setEmptyRow(node);
    }
    else if(used.access.type == DescriptorType::Sampler)
    {
      if(samplerDescriptor.object == ResourceId())
      {
        node = new RDTreeWidgetItem({
            slotname,
            bindType,
            ResourceId(),
            lit("-"),
            QString(),
            QString(),
        });

        setEmptyRow(node);
      }
      else
      {
        node = new RDTreeWidgetItem(makeSampler(slotname, samplerDescriptor));

        if(!filledSlot)
          setEmptyRow(node);
      }
    }
    else
    {
      if(descriptor.resource == ResourceId())
      {
        node = new RDTreeWidgetItem({
            slotname,
            bindType,
            ResourceId(),
            lit("-"),
            QString(),
            QString(),
        });

        setEmptyRow(node);
      }
      else
      {
        QString typeName = ToQStr(restype) + lit(" ") + bindType;

        QString dim;

        if(restype == TextureType::Texture3D)
          dim = QFormatStr("%1x%2x%3").arg(w).arg(h).arg(d);
        else if(restype == TextureType::Texture1D || restype == TextureType::Texture1DArray)
          dim = QString::number(w);
        else
          dim = QFormatStr("%1x%2").arg(w).arg(h);

        if(descriptor.swizzle.red != TextureSwizzle::Red ||
           descriptor.swizzle.green != TextureSwizzle::Green ||
           descriptor.swizzle.blue != TextureSwizzle::Blue ||
           descriptor.swizzle.alpha != TextureSwizzle::Alpha)
        {
          format += tr(" swizzle[%1%2%3%4]")
                        .arg(ToQStr(descriptor.swizzle.red))
                        .arg(ToQStr(descriptor.swizzle.green))
                        .arg(ToQStr(descriptor.swizzle.blue))
                        .arg(ToQStr(descriptor.swizzle.alpha));
        }

        if(restype == TextureType::Texture1DArray || restype == TextureType::Texture2DArray ||
           restype == TextureType::Texture2DMSArray || restype == TextureType::TextureCubeArray)
        {
          dim += QFormatStr(" %1[%2]").arg(ToQStr(restype)).arg(a);
        }

        if(restype == TextureType::Texture2DMS || restype == TextureType::Texture2DMSArray)
          dim += QFormatStr(", %1x MSAA").arg(samples);

        node = new RDTreeWidgetItem({
            slotname,
            typeName,
            descriptor.resource,
            dim,
            format,
            QString(),
        });

        node->setTag(tag);

        if(!filledSlot)
          setEmptyRow(node);

        if(used.access.type == DescriptorType::ImageSampler)
        {
          if(samplerDescriptor.object == ResourceId())
          {
            samplerNode = new RDTreeWidgetItem({
                slotname,
                bindType,
                ResourceId(),
                lit("-"),
                QString(),
                QString(),
            });

            setEmptyRow(samplerNode);
          }
          else
          {
            if(!samplers.contains(samplerDescriptor.object))
            {
              samplerNode = new RDTreeWidgetItem(makeSampler(QString(), samplerDescriptor));

              if(!filledSlot)
                setEmptyRow(samplerNode);

              CombinedSamplerData sampData;
              sampData.node = samplerNode;
              samplerNode->setTag(QVariant::fromValue(sampData));

              samplers.insert(samplerDescriptor.object, samplerNode);
            }

            {
              RDTreeWidgetItem *combinedSamp = m_CombinedImageSamplers[node] =
                  samplers[samplerDescriptor.object];

              CombinedSamplerData sampData = combinedSamp->tag().value<CombinedSamplerData>();
              sampData.images.push_back(node);
              combinedSamp->setTag(QVariant::fromValue(sampData));
            }
          }
        }
      }
    }

    if(tex)
    {
      // for rows with view details we can't highlight used combined samplers, so instead we put
      // it in the tooltip for that row and remove it from the m_CombinedImageSamplers list.
      QString samplerString =
          used.access.type == DescriptorType::ImageSampler
              ? tr("Image combined with sampler %1\n").arg(m_Ctx.GetResourceName(descriptor.secondary))
              : QString();

      bool hasViewDetails = setViewDetails(node, descriptor, tex, samplerString);

      if(hasViewDetails)
      {
        node->setText(
            4, tr("%1 viewed by %2").arg(ToQStr(descriptor.resource)).arg(ToQStr(descriptor.view)));

        if(used.access.type == DescriptorType::ImageSampler)
        {
          RDTreeWidgetItem *combinedSamp = m_CombinedImageSamplers[node];

          if(combinedSamp)
          {
            CombinedSamplerData sampData = combinedSamp->tag().value<CombinedSamplerData>();
            sampData.images.removeOne(node);
            combinedSamp->setTag(QVariant::fromValue(sampData));

            m_CombinedImageSamplers.remove(node);
          }
        }
      }
    }
    else if(buf)
    {
      setViewDetails(node, descriptor, buf);
    }

    resources->addTopLevelItem(node);

    if(samplerNode)
      resources->addTopLevelItem(samplerNode);
  }
}

void VulkanPipelineStateViewer::addConstantBlockRow(const ConstantBlock *cblock,
                                                    const UsedDescriptor &used,
                                                    uint32_t dynamicOffset, RDTreeWidget *ubos)
{
  const Descriptor &descriptor = used.descriptor;

  VulkanCBufferTag tag(used.access.index, used.access.arrayElement);

  bool filledSlot = (descriptor.resource != ResourceId());
  // Vulkan does not report unused elements at all because we enumerate exclusively from the
  // perspective of which descriptors are used
  bool usedSlot = true;

  if(showNode(usedSlot, filledSlot))
  {
    QString slotname;
    if(used.access.index == DescriptorAccess::NoShaderBinding)
    {
      slotname = m_Locations[{used.access.descriptorStore, used.access.byteOffset}].logicalBindName;

      slotname += QFormatStr("[%1]").arg(used.access.arrayElement);
    }
    else if(cblock)
    {
      if(IsPushSet(used.access.stage, used.access.descriptorStore))
        slotname = tr("Push ");

      slotname +=
          QFormatStr("Set %1, %2").arg(cblock->fixedBindSetOrSpace).arg(cblock->fixedBindNumber);

      if(!cblock->name.empty())
        slotname += lit(": ") + cblock->name;

      if(cblock->bindArraySize > 1)
        slotname += QFormatStr("[%1]").arg(used.access.arrayElement);
    }

    uint64_t bufferByteSize = descriptor.byteSize;
    int numvars = cblock != NULL ? cblock->variables.count() : 0;
    uint64_t declaredByteSize = cblock != NULL ? cblock->byteSize : 0;

    QString byteRange = lit("-");

    uint64_t byteOffset = descriptor.byteOffset + dynamicOffset;

    {
      BufferDescription *buf = m_Ctx.GetBuffer(descriptor.resource);
      if(buf && bufferByteSize == UINT64_MAX)
        bufferByteSize = buf->length - byteOffset;

      byteRange = formatByteRange(buf, descriptor, dynamicOffset);
    }

    QString sizestr;

    QVariant name = descriptor.resource;

    // push constants or specialization constants
    if(cblock && !cblock->bufferBacked)
    {
      slotname = cblock->name;
      if(cblock->compileConstants)
      {
        name = tr("Specialization constants");
        byteRange = QString();
      }
      else
      {
        name = tr("Push constants");

        uint32_t minOffset = getMinOffset(cblock->variables);

        if(minOffset == ~0U)
          minOffset = 0;

        byteRange =
            QFormatStr("%1 - %2 bytes")
                .arg(Formatter::HumanFormat(byteOffset + minOffset, Formatter::OffsetSize))
                .arg(Formatter::HumanFormat(byteOffset + cblock->byteSize, Formatter::OffsetSize));

        if(byteOffset + descriptor.byteSize > m_Ctx.CurVulkanPipelineState()->pushconsts.size())
        {
          filledSlot = false;
          byteRange +=
              tr(", only %1 bytes pushed")
                  .arg(Formatter::HumanFormat(m_Ctx.CurVulkanPipelineState()->pushconsts.size(),
                                              Formatter::OffsetSize));
        }
      }

      sizestr = tr("%1 Variables").arg(numvars);
    }
    else
    {
      if(descriptor.flags & DescriptorFlags::InlineData)
      {
        name = tr("Inline block");
        byteRange = tr("%1 bytes").arg(Formatter::HumanFormat(bufferByteSize, Formatter::OffsetSize));
      }

      if(bufferByteSize == declaredByteSize)
        sizestr = tr("%1 Variables, %2 bytes")
                      .arg(numvars)
                      .arg(Formatter::HumanFormat(bufferByteSize, Formatter::OffsetSize));
      else
        sizestr = tr("%1 Variables, %2 bytes needed, %3 provided")
                      .arg(numvars)
                      .arg(Formatter::HumanFormat(declaredByteSize, Formatter::OffsetSize))
                      .arg(Formatter::HumanFormat(bufferByteSize, Formatter::OffsetSize));

      if(bufferByteSize < declaredByteSize)
        filledSlot = false;
    }

    RDTreeWidgetItem *node = new RDTreeWidgetItem({slotname, name, byteRange, sizestr, QString()});

    node->setTag(QVariant::fromValue(tag));

    if(!filledSlot)
      setEmptyRow(node);

    if(!usedSlot)
      setInactiveRow(node);

    ubos->addTopLevelItem(node);
  }
}

void VulkanPipelineStateViewer::setShaderState(const VKPipe::Pipeline &pipe,
                                               const VKPipe::Shader &stage, RDLabel *shader,
                                               RDLabel *pipeLayout, RDTreeWidget *descSets)
{
  ShaderReflection *shaderDetails = stage.reflection;

  QString shText;
  if(stage.shaderObject)
    shText = QFormatStr("%1").arg(ToQStr(stage.resourceId));
  else
    shText = QFormatStr("%1: %2").arg(ToQStr(pipe.pipelineResourceId)).arg(ToQStr(stage.resourceId));

  if(shaderDetails != NULL)
  {
    QString entryFunc = shaderDetails->entryPoint;

    if(entryFunc != lit("main"))
      shText += lit(": ") + entryFunc + lit("()");

    const ShaderDebugInfo &dbg = shaderDetails->debugInfo;
    int entryFile = qMax(0, dbg.entryLocation.fileIndex);

    if(!dbg.files.isEmpty())
      shText += lit(" - ") + QFileInfo(dbg.files[entryFile].filename).fileName();
  }

  if(stage.requiredSubgroupSize != 0)
    shText += tr(" (Subgroup size %1)").arg(stage.requiredSubgroupSize);

  shader->setText(shText);

  if(pipe.pipelineComputeLayoutResourceId != ResourceId())
  {
    pipeLayout->setText(tr("Pipeline Layout: %1").arg(ToQStr(pipe.pipelineComputeLayoutResourceId)));
  }
  else if(pipe.pipelinePreRastLayoutResourceId == pipe.pipelineFragmentLayoutResourceId)
  {
    pipeLayout->setText(tr("Pipeline Layout: %1").arg(ToQStr(pipe.pipelineFragmentLayoutResourceId)));
  }
  else
  {
    pipeLayout->setText(tr("Pipeline Layouts: %1 and %2")
                            .arg(ToQStr(pipe.pipelinePreRastLayoutResourceId))
                            .arg(ToQStr(pipe.pipelineFragmentLayoutResourceId)));
  }

  descSets->clear();
  for(uint32_t i = 0; i < pipe.descriptorSets.size(); i++)
  {
    RDTreeWidgetItem *item =
        new RDTreeWidgetItem({i, pipe.descriptorSets[i].layoutResourceId,
                              pipe.descriptorSets[i].descriptorSetResourceId, QString()});
    item->setTag(i);
    descSets->addTopLevelItem(item);
  }
}

void VulkanPipelineStateViewer::setState()
{
  if(!m_Ctx.IsCaptureLoaded())
  {
    clearState();
    return;
  }

  // cache latest state of these checkboxes
  m_ShowUnused = ui->showUnused->isChecked();
  m_ShowEmpty = ui->showEmpty->isChecked();

  m_CombinedImageSamplers.clear();

  const VKPipe::State &state = *m_Ctx.CurVulkanPipelineState();
  const ActionDescription *action = m_Ctx.CurAction();

  bool showUnused = ui->showUnused->isChecked();
  bool showEmpty = ui->showEmpty->isChecked();

  const QPixmap &tick = Pixmaps::tick(this);
  const QPixmap &cross = Pixmaps::cross(this);

  bool usedBindings[128] = {};

  // highlight the appropriate stages in the flowchart
  if(action == NULL)
  {
    QList<bool> allOn;
    for(int i = 0; i < ui->pipeFlow->stageNames().count(); i++)
      allOn.append(true);
    ui->pipeFlow->setStagesEnabled(allOn);
  }
  else if(action->flags & ActionFlags::Dispatch)
  {
    QList<bool> computeOnly;
    for(int i = 0; i < ui->pipeFlow->stageNames().count(); i++)
      computeOnly.append(false);
    computeOnly.back() = true;
    ui->pipeFlow->setStagesEnabled(computeOnly);
  }
  else if(action->flags & ActionFlags::MeshDispatch)
  {
    setNewMeshPipeFlow();
    ui->pipeFlow->setStagesEnabled(
        {state.taskShader.resourceId != ResourceId(), true, true, true, true, false});
  }
  else
  {
    bool xfbActive = !state.transformFeedback.buffers.isEmpty();

    bool raster = true;

    if(state.rasterizer.rasterizerDiscardEnable)
    {
      raster = false;
    }

    if(state.geometryShader.resourceId == ResourceId() && xfbActive)
    {
      ui->pipeFlow->setStageName(4, lit("XFB"), tr("Transform Feedback"));
    }
    else
    {
      ui->pipeFlow->setStageName(4, lit("GS"), tr("Geometry Shader"));
    }

    setOldMeshPipeFlow();
    ui->pipeFlow->setStagesEnabled(
        {true, true, state.tessControlShader.resourceId != ResourceId(),
         state.tessEvalShader.resourceId != ResourceId(),
         state.geometryShader.resourceId != ResourceId() || xfbActive, raster,
         raster && state.fragmentShader.resourceId != ResourceId(), raster, false});
  }

  ////////////////////////////////////////////////
  // Vertex Input

  int vs = 0;

  if(m_MeshPipe)
  {
    setShaderState(state.graphics, state.taskShader, ui->tsShader, ui->tsPipeLayout, ui->tsDescSets);
    setShaderState(state.graphics, state.meshShader, ui->msShader, ui->msPipeLayout, ui->msDescSets);

    if(state.meshShader.reflection)
      ui->msTopology->setText(ToQStr(state.meshShader.reflection->outputTopology));
    else
      ui->msTopology->setText(QString());
  }
  else
  {
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
          uint32_t attrib = a.location;

          if(attrib < state.vertexShader.reflection->inputSignature.size())
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

    int numCPs = PatchList_Count(state.inputAssembly.topology);
    if(numCPs > 0)
    {
      ui->topology->setText(tr("PatchList (%1 Control Points)").arg(numCPs));
    }
    else
    {
      ui->topology->setText(ToQStr(state.inputAssembly.topology));
    }

    m_Common.setTopologyDiagram(ui->topologyDiagram, state.inputAssembly.topology);

    ui->primRestart->setVisible(state.inputAssembly.primitiveRestartEnable);

    vs = ui->viBuffers->verticalScrollBar()->value();
    ui->viBuffers->beginUpdate();
    ui->viBuffers->clear();

    bool ibufferUsed = action != NULL && (action->flags & ActionFlags::Indexed);

    if(state.inputAssembly.indexBuffer.resourceId != ResourceId())
    {
      if(ibufferUsed || showUnused)
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
             (qulonglong)state.inputAssembly.indexBuffer.byteStride, (qulonglong)length, QString()});

        QString iformat;

        if(state.inputAssembly.indexBuffer.byteStride == 1)
          iformat = lit("ubyte");
        else if(state.inputAssembly.indexBuffer.byteStride == 2)
          iformat = lit("ushort");
        else if(state.inputAssembly.indexBuffer.byteStride == 4)
          iformat = lit("uint");

        iformat +=
            lit(" indices[%1]").arg(RENDERDOC_NumVerticesPerPrimitive(state.inputAssembly.topology));

        node->setTag(QVariant::fromValue(VulkanVBIBTag(
            state.inputAssembly.indexBuffer.resourceId,
            state.inputAssembly.indexBuffer.byteOffset +
                (action ? action->indexOffset * state.inputAssembly.indexBuffer.byteStride : 0),
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
        RDTreeWidgetItem *node =
            new RDTreeWidgetItem({tr("Index"), ResourceId(), tr("Index"), lit("-"), lit("-"),
                                  lit("-"), lit("-"), QString()});

        QString iformat;

        if(state.inputAssembly.indexBuffer.byteStride == 1)
          iformat = lit("ubyte");
        else if(state.inputAssembly.indexBuffer.byteStride == 2)
          iformat = lit("ushort");
        else if(state.inputAssembly.indexBuffer.byteStride == 4)
          iformat = lit("uint");

        iformat +=
            lit(" indices[%1]").arg(RENDERDOC_NumVerticesPerPrimitive(state.inputAssembly.topology));

        node->setTag(QVariant::fromValue(VulkanVBIBTag(
            state.inputAssembly.indexBuffer.resourceId,
            state.inputAssembly.indexBuffer.byteOffset +
                (action ? action->indexOffset * state.inputAssembly.indexBuffer.byteStride : 0),
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
            (i < state.vertexInput.vertexBuffers.count() ? &state.vertexInput.vertexBuffers[i]
                                                         : NULL);
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
            stride = vbuff->byteStride;
            length = vbuff->byteSize;

            BufferDescription *buf = m_Ctx.GetBuffer(vbuff->resourceId);
            if(buf && length >= ULONG_MAX)
              length = buf->length;
          }

          if(bind != NULL)
          {
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
              vbuff != NULL ? vbuff->resourceId : ResourceId(),
              vbuff != NULL ? vbuff->byteOffset : 0, m_Common.GetVBufferFormatString(i))));

          if(!filledSlot || bind == NULL || vbuff == NULL || vbuff->resourceId == ResourceId())
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

    setShaderState(state.graphics, state.vertexShader, ui->vsShader, ui->vsPipeLayout,
                   ui->vsDescSets);
    setShaderState(state.graphics, state.geometryShader, ui->gsShader, ui->gsPipeLayout,
                   ui->gsDescSets);
    setShaderState(state.graphics, state.tessControlShader, ui->tcsShader, ui->tcsPipeLayout,
                   ui->tcsDescSets);
    setShaderState(state.graphics, state.tessEvalShader, ui->tesShader, ui->tesPipeLayout,
                   ui->tesDescSets);
  }

  setShaderState(state.graphics, state.fragmentShader, ui->fsShader, ui->fsPipeLayout,
                 ui->fsDescSets);
  setShaderState(state.compute, state.computeShader, ui->csShader, ui->csPipeLayout, ui->csDescSets);

  // fill in descriptor access
  {
    RDTreeWidget *resources[] = {
        ui->vsResources, ui->tcsResources, ui->tesResources, ui->gsResources,
        ui->fsResources, ui->csResources,  ui->tsResources,  ui->msResources,
    };

    RDTreeWidget *ubos[] = {
        ui->vsUBOs, ui->tcsUBOs, ui->tesUBOs, ui->gsUBOs,
        ui->fsUBOs, ui->csUBOs,  ui->tsUBOs,  ui->msUBOs,
    };

    ScopedTreeUpdater restorers[] = {
        ui->vsResources, ui->tcsResources, ui->tesResources, ui->gsResources,
        ui->fsResources, ui->csResources,  ui->tsResources,  ui->msResources,
        ui->vsUBOs,      ui->tcsUBOs,      ui->tesUBOs,      ui->gsUBOs,
        ui->fsUBOs,      ui->csUBOs,       ui->tsUBOs,       ui->msUBOs,
    };

    // samplers we only deduplicate within a stage
    QMap<ResourceId, RDTreeWidgetItem *> samplers[NumShaderStages];

    const ShaderReflection *shaderRefls[NumShaderStages];

    for(ShaderStage stage : values<ShaderStage>())
      shaderRefls[(uint32_t)stage] = m_Ctx.CurPipelineState().GetShaderReflection(stage);

    rdcarray<UsedDescriptor> descriptors = m_Ctx.CurPipelineState().GetAllUsedDescriptors();
    rdcarray<ResourceId> descSets;

    const VKPipe::Pipeline &pipeline =
        (action && (action->flags & ActionFlags::Dispatch)) ? state.compute : state.graphics;

    QMap<QPair<ResourceId, uint64_t>, uint32_t> dynamicOffsets;

    for(const VKPipe::DescriptorSet &set : pipeline.descriptorSets)
    {
      descSets.push_back(set.descriptorSetResourceId);

      for(const VKPipe::DynamicOffset &offs : set.dynamicOffsets)
      {
        dynamicOffsets[{set.descriptorSetResourceId, offs.descriptorByteOffset}] =
            offs.dynamicBufferByteOffset;
      }
    }

    std::sort(descriptors.begin(), descriptors.end(),
              [descSets](const UsedDescriptor &a, const UsedDescriptor &b) {
                int32_t a_set = descSets.indexOf(a.access.descriptorStore);
                int32_t b_set = descSets.indexOf(b.access.descriptorStore);

                // non-set associated things (specialisation constants, push constants, etc) to the end
                if(a_set == -1)
                  a_set = descSets.count() + 1;
                if(b_set == -1)
                  b_set = descSets.count() + 1;

                if(a_set != b_set)
                  return a_set < b_set;

                // for non-sets, sort by interface index
                if(a_set == b_set && a_set > descSets.count())
                {
                  return a.access.index < b.access.index;
                }

                // otherwise for normal sets, sort by byte offset
                return a.access.byteOffset < b.access.byteOffset;
              });

    for(const UsedDescriptor &used : descriptors)
    {
      if(used.access.type == DescriptorType::Unknown || used.access.stage == ShaderStage::Count)
        continue;

      const ShaderReflection *refl = shaderRefls[(uint32_t)used.access.stage];

      uint32_t dynamicOffset = 0;
      auto dynIt =
          dynamicOffsets.find({used.access.descriptorStore, (uint64_t)used.access.byteOffset});
      if(dynIt != dynamicOffsets.end())
        dynamicOffset = *dynIt;

      if(IsConstantBlockDescriptor(used.access.type))
      {
        const ConstantBlock *shaderBind = NULL;

        if(refl && used.access.index < refl->constantBlocks.size())
          shaderBind = &refl->constantBlocks[used.access.index];

        addConstantBlockRow(shaderBind, used, dynamicOffset, ubos[(uint32_t)used.access.stage]);
      }
      else
      {
        const bool ro = IsReadOnlyDescriptor(used.access.type);

        const ShaderResource *shaderRes = NULL;
        const ShaderSampler *shaderSamp = NULL;

        if(IsSamplerDescriptor(used.access.type))
        {
          if(refl && used.access.index < refl->samplers.size())
            shaderSamp = &refl->samplers[used.access.index];
        }
        else if(IsReadOnlyDescriptor(used.access.type))
        {
          if(refl && used.access.index < refl->readOnlyResources.size())
            shaderRes = &refl->readOnlyResources[used.access.index];
        }
        else
        {
          if(refl && used.access.index < refl->readWriteResources.size())
            shaderRes = &refl->readWriteResources[used.access.index];
        }

        addResourceRow(shaderRes, shaderSamp, used, dynamicOffset,
                       resources[(uint32_t)used.access.stage], samplers[(uint32_t)used.access.stage]);
      }
    }
  }

  QToolButton *shaderButtons[] = {
      // view buttons
      ui->tsShaderViewButton,
      ui->msShaderViewButton,
      ui->vsShaderViewButton,
      ui->tcsShaderViewButton,
      ui->tesShaderViewButton,
      ui->gsShaderViewButton,
      ui->fsShaderViewButton,
      ui->csShaderViewButton,
      // edit buttons
      ui->tsShaderEditButton,
      ui->msShaderEditButton,
      ui->vsShaderEditButton,
      ui->tcsShaderEditButton,
      ui->tesShaderEditButton,
      ui->gsShaderEditButton,
      ui->fsShaderEditButton,
      ui->csShaderEditButton,
      // save buttons
      ui->tsShaderSaveButton,
      ui->msShaderSaveButton,
      ui->vsShaderSaveButton,
      ui->tcsShaderSaveButton,
      ui->tesShaderSaveButton,
      ui->gsShaderSaveButton,
      ui->fsShaderSaveButton,
      ui->csShaderSaveButton,
  };

  for(QToolButton *b : shaderButtons)
  {
    const VKPipe::Shader *stage = stageForSender(b);

    if(stage == NULL || stage->resourceId == ResourceId())
      continue;

    ResourceId pipe = stage->stage == ShaderStage::Compute ? state.compute.pipelineResourceId
                                                           : state.graphics.pipelineResourceId;

    b->setEnabled(stage->reflection && (pipe != ResourceId() || stage->shaderObject));

    m_Common.SetupShaderEditButton(b, pipe, stage->resourceId, stage->reflection);
  }

  QToolButton *messageButtons[] = {
      ui->vsShaderMessagesButton, ui->tcsShaderMessagesButton, ui->tesShaderMessagesButton,
      ui->gsShaderMessagesButton, ui->fsShaderMessagesButton,  ui->csShaderMessagesButton,
      ui->tsShaderMessagesButton, ui->msShaderMessagesButton,
  };

  int numMessages[NumShaderStages] = {};

  for(const ShaderMessage &msg : state.shaderMessages)
    numMessages[(uint32_t)msg.stage]++;

  static_assert(ARRAY_COUNT(messageButtons) <= ARRAY_COUNT(numMessages),
                "More buttons than shader stages");

  for(uint32_t i = 0; i < ARRAY_COUNT(messageButtons); i++)
  {
    messageButtons[i]->setVisible(numMessages[i] > 0);
    messageButtons[i]->setText(tr("%n Message(s)", "", numMessages[i]));
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

      RDTreeWidgetItem *node = new RDTreeWidgetItem({
          i,
          s.active ? tr("Active") : tr("Inactive"),
          s.bufferResourceId,
          Formatter::HumanFormat(s.byteOffset, Formatter::OffsetSize),
          Formatter::HumanFormat(length, Formatter::OffsetSize),
          s.counterBufferResourceId,
          Formatter::HumanFormat(s.counterBufferOffset, Formatter::OffsetSize),
          QString(),
      });

      node->setTag(QVariant::fromValue(VulkanBufferTag(s.bufferResourceId, s.byteOffset, length)));

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

  if(state.currentPass.renderpass.resourceId != ResourceId() || state.currentPass.renderpass.dynamic)
  {
    ui->scissors->addTopLevelItem(new RDTreeWidgetItem(
        {tr("Render Area"), state.currentPass.renderArea.x, state.currentPass.renderArea.y,
         state.currentPass.renderArea.width, state.currentPass.renderArea.height}));
  }

  {
    const QString ndcDepthRange =
        state.viewportScissor.depthNegativeOneToOne ? lit("[-1, 1]") : lit("[0, 1]");

    int i = 0;
    for(const VKPipe::ViewportScissor &v : state.viewportScissor.viewportScissors)
    {
      RDTreeWidgetItem *node = new RDTreeWidgetItem({i, v.vp.x, v.vp.y, v.vp.width, v.vp.height,
                                                     v.vp.minDepth, v.vp.maxDepth, ndcDepthRange});
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

  if(state.rasterizer.depthBiasEnable)
  {
    ui->depthBias->setPixmap(QPixmap());
    ui->depthBiasClamp->setPixmap(QPixmap());
    ui->slopeScaledBias->setPixmap(QPixmap());
    ui->depthBias->setText(Formatter::Format(state.rasterizer.depthBias));
    ui->depthBiasClamp->setText(Formatter::Format(state.rasterizer.depthBiasClamp));
    ui->slopeScaledBias->setText(Formatter::Format(state.rasterizer.slopeScaledDepthBias));
  }
  else
  {
    ui->depthBias->setText(QString());
    ui->depthBiasClamp->setText(QString());
    ui->slopeScaledBias->setText(QString());
    ui->depthBias->setPixmap(cross);
    ui->depthBiasClamp->setPixmap(cross);
    ui->slopeScaledBias->setPixmap(cross);
  }

  ui->depthClamp->setPixmap(state.rasterizer.depthClampEnable ? tick : cross);
  ui->depthClip->setPixmap(state.rasterizer.depthClipEnable ? tick : cross);
  ui->rasterizerDiscard->setPixmap(state.rasterizer.rasterizerDiscardEnable ? tick : cross);
  ui->lineWidth->setText(Formatter::Format(state.rasterizer.lineWidth));

  QString conservRaster = ToQStr(state.rasterizer.conservativeRasterization);
  if(state.rasterizer.conservativeRasterization == ConservativeRaster::Overestimate &&
     state.rasterizer.extraPrimitiveOverestimationSize > 0.0f)
    conservRaster += QFormatStr(" (+%1)").arg(state.rasterizer.extraPrimitiveOverestimationSize);

  ui->conservativeRaster->setText(conservRaster);

  if(state.rasterizer.lineStippleFactor == 0)
  {
    ui->stippleFactor->setText(QString());
    ui->stippleFactor->setPixmap(cross);
    ui->stipplePattern->setText(QString());
    ui->stipplePattern->setPixmap(cross);
  }
  else
  {
    ui->stippleFactor->setPixmap(QPixmap());
    ui->stippleFactor->setText(ToQStr(state.rasterizer.lineStippleFactor));
    ui->stipplePattern->setPixmap(QPixmap());
    ui->stipplePattern->setText(QString::number(state.rasterizer.lineStipplePattern, 2));
  }

  ui->pipelineShadingRate->setText(QFormatStr("%1x%2")
                                       .arg(state.rasterizer.pipelineShadingRate.first)
                                       .arg(state.rasterizer.pipelineShadingRate.second));
  ui->shadingRateCombiners->setText(
      QFormatStr("%1, %2")
          .arg(ToQStr(state.rasterizer.shadingRateCombiners.first, GraphicsAPI::Vulkan))
          .arg(ToQStr(state.rasterizer.shadingRateCombiners.second, GraphicsAPI::Vulkan)));

  ui->provokingVertex->setText(state.rasterizer.provokingVertexFirst ? tr("First") : tr("Last"));

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

  if(state.currentPass.renderpass.dynamic)
  {
    QString dynamic = tr("Dynamic", "Dynamic rendering renderpass name");
    QString text = QFormatStr("Render Pass: %1").arg(dynamic);
    if(state.currentPass.renderpass.suspended)
      text += tr(" (Suspended)", "Dynamic rendering renderpass name");
    ui->renderpass->setText(text);
    ui->framebuffer->setText(tr("Framebuffer: %1").arg(dynamic));
  }
  else
  {
    QString text = QFormatStr("Render Pass: %1 (Subpass %2)")
                       .arg(ToQStr(state.currentPass.renderpass.resourceId))
                       .arg(state.currentPass.renderpass.subpass);
    if(state.currentPass.renderpass.feedbackLoop)
      text += tr(" (Feedback Loop)");
    ui->renderpass->setText(text);
    ui->framebuffer->setText(
        QFormatStr("Framebuffer: %1").arg(ToQStr(state.currentPass.framebuffer.resourceId)));
  }

  vs = ui->fbAttach->verticalScrollBar()->value();
  ui->fbAttach->beginUpdate();
  ui->fbAttach->clear();

  vs2 = ui->blends->verticalScrollBar()->value();
  ui->blends->beginUpdate();
  ui->blends->clear();
  {
    const VKPipe::Framebuffer &fb = state.currentPass.framebuffer;
    const VKPipe::RenderPass &rp = state.currentPass.renderpass;

    enum class AttType
    {
      Color,
      Resolve,
      Depth,
      DepthResolve,
      Density,
      ShadingRate
    };

    struct AttachRef
    {
      int32_t fbIdx;
      int32_t localIdx;
      AttType type;
    };

    rdcarray<AttachRef> attachs;

    // iterate the attachments in logical order, checking each index into the framebuffer

    for(int c = 0; c < rp.colorAttachments.count(); c++)
      attachs.push_back({int32_t(rp.colorAttachments[c]), c, AttType::Color});

    for(int c = 0; c < rp.resolveAttachments.count(); c++)
      attachs.push_back({int32_t(rp.resolveAttachments[c]), c, AttType::Resolve});

    attachs.push_back({rp.depthstencilAttachment, 0, AttType::Depth});
    attachs.push_back({rp.depthstencilResolveAttachment, 0, AttType::DepthResolve});
    attachs.push_back({rp.fragmentDensityAttachment, 0, AttType::Density});
    attachs.push_back({rp.shadingRateAttachment, 0, AttType::ShadingRate});

    for(const AttachRef &a : attachs)
    {
      int32_t attIdx = a.fbIdx;

      // negative index means unused
      bool usedSlot = (attIdx >= 0);

      bool filledSlot = false;
      if(usedSlot && attIdx < fb.attachments.count())
        filledSlot = fb.attachments[attIdx].resource != ResourceId();

      if(showNode(usedSlot, filledSlot))
      {
        QString slotname;

        if(a.type == AttType::Color)
        {
          slotname = QFormatStr("Color %1").arg(a.localIdx);

          if(state.fragmentShader.reflection != NULL)
          {
            const rdcarray<SigParameter> &outSig = state.fragmentShader.reflection->outputSignature;
            for(int s = 0; s < outSig.count(); s++)
            {
              if(outSig[s].regIndex == (uint32_t)a.localIdx &&
                 (outSig[s].systemValue == ShaderBuiltin::Undefined ||
                  outSig[s].systemValue == ShaderBuiltin::ColorOutput))
              {
                slotname += QFormatStr(": %1").arg(outSig[s].varName);
              }
            }
          }
        }
        else if(a.type == AttType::Resolve)
        {
          slotname = QFormatStr("Resolve %1").arg(a.localIdx);
        }
        else if(a.type == AttType::Depth)
        {
          slotname = lit("Depth/Stencil");

          if(filledSlot)
          {
            const Descriptor &p = fb.attachments[attIdx];

            slotname = lit("Depth");

            if(p.format.type == ResourceFormatType::D16S8 ||
               p.format.type == ResourceFormatType::D24S8 ||
               p.format.type == ResourceFormatType::D32S8)
              slotname = lit("Depth/Stencil");
            else if(p.format.type == ResourceFormatType::S8)
              slotname = lit("Stencil");
          }
        }
        else if(a.type == AttType::DepthResolve)
        {
          slotname = lit("Depth/Stencil Resolve");
        }
        else if(a.type == AttType::Density)
        {
          slotname = lit("Fragment Density Map");
        }
        else if(a.type == AttType::ShadingRate)
        {
          slotname = lit("Fragment Shading Rate Map");
        }

        RDTreeWidgetItem *node;

        if(filledSlot)
        {
          const Descriptor &p = fb.attachments[attIdx];

          QString format;
          QString typeName;
          QString dimensions;
          QString samples;
          bool tooltipOffsets = false;

          if(p.resource != ResourceId())
          {
            format = p.format.Name();
            typeName = tr("Unknown");
          }
          else
          {
            format = lit("-");
            typeName = lit("-");
            dimensions = lit("-");
            samples = lit("-");
          }

          TextureDescription *tex = m_Ctx.GetTexture(p.resource);
          if(tex)
          {
            dimensions += tr("%1x%2").arg(tex->width).arg(tex->height);
            if(tex->depth > 1)
              dimensions += tr("x%1").arg(tex->depth);
            if(tex->arraysize > 1)
              dimensions += tr("[%1]").arg(tex->arraysize);

            typeName = ToQStr(tex->type);
          }
          samples = getTextureRenderSamples(tex, state.currentPass.renderpass);

          if(p.swizzle.red != TextureSwizzle::Red || p.swizzle.green != TextureSwizzle::Green ||
             p.swizzle.blue != TextureSwizzle::Blue || p.swizzle.alpha != TextureSwizzle::Alpha)
          {
            format += tr(" swizzle[%1%2%3%4]")
                          .arg(ToQStr(p.swizzle.red))
                          .arg(ToQStr(p.swizzle.green))
                          .arg(ToQStr(p.swizzle.blue))
                          .arg(ToQStr(p.swizzle.alpha));
          }

          rdcpair<uint32_t, uint32_t> shadingRateTexelSize = {0, 0};

          if(a.type == AttType::Density)
          {
            if(state.currentPass.renderpass.fragmentDensityOffsets.size() > 2)
            {
              tooltipOffsets = true;
            }
            else if(state.currentPass.renderpass.fragmentDensityOffsets.size() > 0)
            {
              dimensions += tr(" : offsets");
              for(uint32_t j = 0; j < state.currentPass.renderpass.fragmentDensityOffsets.size(); j++)
              {
                const Offset &o = state.currentPass.renderpass.fragmentDensityOffsets[j];
                if(j > 0)
                  dimensions += tr(", ");

                dimensions += tr(" %1x%2").arg(o.x).arg(o.y);
              }
            }
          }
          else if(a.type == AttType::ShadingRate)
          {
            shadingRateTexelSize = state.currentPass.renderpass.shadingRateTexelSize;
          }

          QString resName = ToQStr(p.resource);

          if(shadingRateTexelSize.first > 0)
            resName +=
                tr(" (%1x%2 texels)").arg(shadingRateTexelSize.first).arg(shadingRateTexelSize.second);

          // append if colour or depth/stencil feedback is allowed
          if(a.type == AttType::Color && state.currentPass.colorFeedbackAllowed)
          {
            resName += tr(" (Feedback)");
          }
          else if(a.type == AttType::Depth && state.currentPass.depthFeedbackAllowed &&
                  state.currentPass.stencilFeedbackAllowed)
          {
            resName += tr(" (Feedback)");
          }
          else if(a.type == AttType::Depth && (state.currentPass.depthFeedbackAllowed ||
                                               state.currentPass.stencilFeedbackAllowed))
          {
            // if only one of depth or stencil is allowed, display that specifically
            if(tex->format.type == ResourceFormatType::D16S8 ||
               tex->format.type == ResourceFormatType::D24S8 ||
               tex->format.type == ResourceFormatType::D32S8)
            {
              if(state.currentPass.depthFeedbackAllowed)
                resName += tr(" (Depth Feedback)");
              else if(state.currentPass.stencilFeedbackAllowed)
                resName += tr(" (Depth Feedback)");
            }
            else if(tex->format.type == ResourceFormatType::S8 &&
                    state.currentPass.stencilFeedbackAllowed)
            {
              resName += tr(" (Feedback)");
            }
            // this case must be depth-only, since depth/stencil and stencil-only are covered above.
            else if(state.currentPass.depthFeedbackAllowed)
            {
              resName += tr(" (Feedback)");
            }
          }

          node = new RDTreeWidgetItem(
              {slotname, resName, typeName, dimensions, format, samples, QString()});

          if(tex)
            node->setTag(QVariant::fromValue(VulkanTextureTag(p.resource, p.format.compType)));

          if(p.resource == ResourceId())
            setEmptyRow(node);
          else if(!usedSlot)
            setInactiveRow(node);

          bool hasViewDetails = setViewDetails(
              node, p, tex, QString(),
              a.type == AttType::Resolve || a.type == AttType::DepthResolve, tooltipOffsets);

          if(hasViewDetails)
            node->setText(1, tr("%1 viewed by %2").arg(ToQStr(p.resource)).arg(ToQStr(p.view)));
        }
        else
        {
          // special simple case for an attachment that's not used. No framebuffer to look up so
          // just display the name and empty contents.

          node = new RDTreeWidgetItem({slotname, usedSlot ? ToQStr(ResourceId()) : tr("Unused"),
                                       QString(), QString(), QString(), QString(), QString()});

          setEmptyRow(node);
        }

        ui->fbAttach->addTopLevelItem(node);
      }
    }

    int i = 0;
    for(const ColorBlend &blend : state.colorBlend.blends)
    {
      bool usedSlot =
          (i < rp.colorAttachments.count() && rp.colorAttachments[i] < fb.attachments.size());

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

  ui->fbAttach->clearSelection();
  ui->fbAttach->endUpdate();
  ui->fbAttach->verticalScrollBar()->setValue(vs);

  ui->blends->clearSelection();
  ui->blends->endUpdate();
  ui->blends->verticalScrollBar()->setValue(vs2);

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

  if(state.depthStencil.depthTestEnable)
  {
    ui->depthEnabled->setPixmap(tick);
    ui->depthFunc->setText(ToQStr(state.depthStencil.depthFunction));
    ui->depthWrite->setPixmap(state.depthStencil.depthWriteEnable ? tick : cross);
    ui->depthWrite->setText(QString());
  }
  else
  {
    ui->depthEnabled->setPixmap(cross);
    ui->depthFunc->setText(tr("Disabled"));
    ui->depthWrite->setPixmap(QPixmap());
    ui->depthWrite->setText(tr("Disabled"));
  }

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
    ui->stencils->addTopLevelItem(new RDTreeWidgetItem({
        tr("Front"),
        ToQStr(state.depthStencil.frontFace.function),
        ToQStr(state.depthStencil.frontFace.failOperation),
        ToQStr(state.depthStencil.frontFace.depthFailOperation),
        ToQStr(state.depthStencil.frontFace.passOperation),
        QVariant(),
        QVariant(),
        QVariant(),
    }));

    m_Common.SetStencilTreeItemValue(ui->stencils->topLevelItem(0), 5,
                                     state.depthStencil.frontFace.writeMask);
    m_Common.SetStencilTreeItemValue(ui->stencils->topLevelItem(0), 6,
                                     state.depthStencil.frontFace.compareMask);
    m_Common.SetStencilTreeItemValue(ui->stencils->topLevelItem(0), 7,
                                     state.depthStencil.frontFace.reference);

    ui->stencils->addTopLevelItem(new RDTreeWidgetItem({
        tr("Back"),
        ToQStr(state.depthStencil.backFace.function),
        ToQStr(state.depthStencil.backFace.failOperation),
        ToQStr(state.depthStencil.backFace.depthFailOperation),
        ToQStr(state.depthStencil.backFace.passOperation),
        QVariant(),
        QVariant(),
        QVariant(),
    }));

    m_Common.SetStencilTreeItemValue(ui->stencils->topLevelItem(1), 5,
                                     state.depthStencil.backFace.writeMask);
    m_Common.SetStencilTreeItemValue(ui->stencils->topLevelItem(1), 6,
                                     state.depthStencil.backFace.compareMask);
    m_Common.SetStencilTreeItemValue(ui->stencils->topLevelItem(1), 7,
                                     state.depthStencil.backFace.reference);
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

  // set up thread debugging inputs
  bool enableDebug = m_Ctx.APIProps().shaderDebugging && state.computeShader.reflection &&
                     state.computeShader.reflection->debugInfo.debuggable && action &&
                     (action->flags & ActionFlags::Dispatch);
  if(enableDebug)
  {
    // Validate dispatch/threadgroup dimensions
    enableDebug &= action->dispatchDimension[0] > 0;
    enableDebug &= action->dispatchDimension[1] > 0;
    enableDebug &= action->dispatchDimension[2] > 0;

    const rdcfixedarray<uint32_t, 3> &threadDims =
        (action->dispatchThreadsDimension[0] == 0)
            ? state.computeShader.reflection->dispatchThreadsDimension
            : action->dispatchThreadsDimension;
    enableDebug &= threadDims[0] > 0;
    enableDebug &= threadDims[1] > 0;
    enableDebug &= threadDims[2] > 0;
  }

  if(enableDebug)
  {
    ui->computeDebugSelector->setEnabled(true);

    // set maximums for CS debugging
    m_ComputeDebugSelector->SetThreadBounds(
        action->dispatchDimension, (action->dispatchThreadsDimension[0] == 0)
                                       ? state.computeShader.reflection->dispatchThreadsDimension
                                       : action->dispatchThreadsDimension);

    ui->computeDebugSelector->setToolTip(
        tr("Debug this compute shader by specifying group/thread ID or dispatch ID"));
  }
  else
  {
    ui->computeDebugSelector->setEnabled(false);

    if(!m_Ctx.APIProps().shaderDebugging)
      ui->computeDebugSelector->setToolTip(tr("This API does not support shader debugging"));
    else if(!action || !(action->flags & ActionFlags::Dispatch))
      ui->computeDebugSelector->setToolTip(tr("No dispatch selected"));
    else if(!state.computeShader.reflection)
      ui->computeDebugSelector->setToolTip(tr("No compute shader bound"));
    else if(!state.computeShader.reflection->debugInfo.debuggable)
      ui->computeDebugSelector->setToolTip(
          tr("This shader doesn't support debugging: %1")
              .arg(state.computeShader.reflection->debugInfo.debugStatus));
    else
      ui->computeDebugSelector->setToolTip(tr("Invalid dispatch/threadgroup dimensions."));
  }
}

void VulkanPipelineStateViewer::resource_itemActivated(RDTreeWidgetItem *item, int column)
{
  const VKPipe::Shader *stage = stageForSender(item->treeWidget());

  if(stage == NULL)
    return;

  QVariant tag = item->tag();

  if(tag.canConvert<VulkanTextureTag>())
  {
    VulkanTextureTag vtex = tag.value<VulkanTextureTag>();

    TextureDescription *tex = m_Ctx.GetTexture(vtex.ID);

    if(tex)
    {
      if(tex->type == TextureType::Buffer)
      {
        IBufferViewer *viewer = m_Ctx.ViewTextureAsBuffer(
            tex->resourceId, Subresource(), BufferFormatter::GetTextureFormatString(*tex));

        m_Ctx.AddDockWindow(viewer->Widget(), DockReference::AddTo, this);
      }
      else
      {
        if(!m_Ctx.HasTextureViewer())
          m_Ctx.ShowTextureViewer();
        ITextureViewer *viewer = m_Ctx.GetTextureViewer();
        viewer->ViewTexture(tex->resourceId, vtex.compType, true);
      }

      return;
    }
  }
  else if(tag.canConvert<VulkanBufferTag>())
  {
    VulkanBufferTag buf = tag.value<VulkanBufferTag>();

    QString format;

    if(stage->reflection)
    {
      const rdcarray<ShaderResource> &resArray =
          (IsReadWriteDescriptor(buf.access.type) ? stage->reflection->readWriteResources
                                                  : stage->reflection->readOnlyResources);

      if(buf.access.index < resArray.size())
      {
        const ShaderResource &shaderRes = resArray[buf.access.index];

        format = BufferFormatter::GetBufferFormatString(
            BufferFormatter::EstimatePackingRules(stage->resourceId, shaderRes.variableType.members),
            stage->resourceId, shaderRes, buf.descriptor.format);
      }
    }

    if(buf.descriptor.resource != ResourceId())
    {
      IBufferViewer *viewer = m_Ctx.ViewBuffer(buf.descriptor.byteOffset, buf.descriptor.byteSize,
                                               buf.descriptor.resource, format);

      m_Ctx.AddDockWindow(viewer->Widget(), DockReference::AddTo, this);
    }
  }
}

void VulkanPipelineStateViewer::resource_hoverItemChanged(RDTreeWidgetItem *hover)
{
  // first make all rows transparent.
  for(RDTreeWidgetItem *item : m_CombinedImageSamplers.keys())
  {
    item->setBackground(QBrush());
    m_CombinedImageSamplers[item]->setBackground(QBrush());
  }

  if(hover)
  {
    // try to get combined sampler data from the current row
    CombinedSamplerData sampData = hover->tag().value<CombinedSamplerData>();

    // or try to see if it's a combined image
    if(m_CombinedImageSamplers.contains(hover))
      sampData = m_CombinedImageSamplers[hover]->tag().value<CombinedSamplerData>();

    // if we got a sampler, highlight it and all images using it
    if(sampData.node)
    {
      sampData.node->setBackgroundColor(QColor(127, 212, 255, 100));
      for(RDTreeWidgetItem *item : sampData.images)
        item->setBackgroundColor(QColor(127, 212, 255, 100));
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

  if(cb.index == DescriptorAccess::NoShaderBinding)
  {
    if(cb.descriptor.resource != ResourceId())
    {
      IBufferViewer *viewer =
          m_Ctx.ViewBuffer(cb.descriptor.byteOffset, cb.descriptor.byteSize, cb.descriptor.resource);

      m_Ctx.AddDockWindow(viewer->Widget(), DockReference::AddTo, this);
    }

    return;
  }

  IBufferViewer *prev = m_Ctx.ViewConstantBuffer(stage->stage, cb.index, cb.arrayElement);

  m_Ctx.AddDockWindow(prev->Widget(), DockReference::TransientPopupArea, this, 0.3f);
}

void VulkanPipelineStateViewer::descSet_itemActivated(RDTreeWidgetItem *item, int column)
{
  const VKPipe::Shader *stage = stageForSender(item->treeWidget());

  if(stage == NULL)
    return;

  int index = item->tag().toInt();

  const rdcarray<VKPipe::DescriptorSet> &descSets =
      stage->stage == ShaderStage::Compute ? m_Ctx.CurVulkanPipelineState()->compute.descriptorSets
                                           : m_Ctx.CurVulkanPipelineState()->graphics.descriptorSets;

  if(index < descSets.count())
  {
    IDescriptorViewer *viewer = m_Ctx.ViewDescriptorStore(descSets[index].descriptorSetResourceId);

    m_Ctx.AddDockWindow(viewer->Widget(), DockReference::AddTo, this);
  }
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

bool VulkanPipelineStateViewer::IsPushSet(ShaderStage stage, ResourceId id)
{
  if(stage == ShaderStage::Compute)
  {
    for(const VKPipe::DescriptorSet &set : m_Ctx.CurVulkanPipelineState()->compute.descriptorSets)
      if(set.descriptorSetResourceId == id)
        return set.pushDescriptor;
  }
  else
  {
    for(const VKPipe::DescriptorSet &set : m_Ctx.CurVulkanPipelineState()->graphics.descriptorSets)
      if(set.descriptorSetResourceId == id)
        return set.pushDescriptor;
  }

  return false;
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
  if(m_MeshPipe)
  {
    // remap since TS/MS are the last tabs but appear first in the flow
    switch(index)
    {
      // TS
      case 0: ui->stagesTabs->setCurrentIndex(9); break;
      // MS
      case 1: ui->stagesTabs->setCurrentIndex(10); break;
      // raster onwards are the same, just skipping VTX,VS,TCS,TES,GS
      case 2: ui->stagesTabs->setCurrentIndex(5); break;
      case 3: ui->stagesTabs->setCurrentIndex(6); break;
      case 4: ui->stagesTabs->setCurrentIndex(7); break;
      case 5: ui->stagesTabs->setCurrentIndex(8); break;
    }
  }
  else
  {
    ui->stagesTabs->setCurrentIndex(index);
  }
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

void VulkanPipelineStateViewer::shaderMessages_clicked()
{
  const VKPipe::Shader *stage = stageForSender(qobject_cast<QWidget *>(QObject::sender()));

  if(stage == NULL)
    return;

  IShaderMessageViewer *shad = m_Ctx.ViewShaderMessages(MaskForStage(stage->stage));

  m_Ctx.AddDockWindow(shad->Widget(), DockReference::AddTo, this);
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
      rows.push_back(
          {attr.vertexBufferBinding, attr.perInstance ? tr("PER_INSTANCE") : tr("PER_VERTEX")});

    m_Common.exportHTMLTable(xml, {tr("Binding"), tr("Step Rate")}, rows);
  }

  {
    xml.writeStartElement(lit("h3"));
    xml.writeCharacters(tr("Vertex Buffers"));
    xml.writeEndElement();

    QList<QVariantList> rows;

    int i = 0;
    for(const VKPipe::VertexBuffer &vb : vi.vertexBuffers)
    {
      uint64_t length = vb.byteSize;

      if(vb.resourceId == ResourceId())
      {
        continue;
      }
      else
      {
        BufferDescription *buf = m_Ctx.GetBuffer(vb.resourceId);
        if(buf && length >= ULONG_MAX)
          length = buf->length;
      }

      rows.push_back({i, vb.resourceId, (qulonglong)vb.byteOffset, (qulonglong)vb.byteStride,
                      (qulonglong)length});

      i++;
    }

    m_Common.exportHTMLTable(
        xml, {tr("Binding"), tr("Buffer"), tr("Offset"), tr("Byte Stride"), tr("Byte Length")}, rows);
  }
}

void VulkanPipelineStateViewer::exportHTML(QXmlStreamWriter &xml, const VKPipe::InputAssembly &ia)
{
  const ActionDescription *action = m_Ctx.CurAction();

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

    if(ia.indexBuffer.byteStride == 1)
      ifmt = lit("UINT8");
    else if(ia.indexBuffer.byteStride == 2)
      ifmt = lit("UINT16");
    else if(ia.indexBuffer.byteStride == 4)
      ifmt = lit("UINT32");

    m_Common.exportHTMLTable(
        xml, {tr("Buffer"), tr("Format"), tr("Offset"), tr("Byte Length"), tr("Primitive Restart")},
        {name, ifmt, (qulonglong)ia.indexBuffer.byteOffset, (qulonglong)length,
         ia.primitiveRestartEnable ? tr("Yes") : tr("No")});
  }

  xml.writeStartElement(lit("p"));
  xml.writeEndElement();

  m_Common.exportHTMLTable(
      xml, {tr("Primitive Topology"), tr("Tessellation Control Points")},
      {ToQStr(ia.topology), m_Ctx.CurVulkanPipelineState()->tessellation.numControlPoints});
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
      const ShaderDebugInfo &dbg = shaderDetails->debugInfo;
      int entryFile = qMax(0, dbg.entryLocation.fileIndex);
      if(entryFunc != lit("main"))
        shadername = QFormatStr("%1()").arg(entryFunc);
      else if(!dbg.files.isEmpty())
        shadername = QFormatStr("%1() - %2")
                         .arg(entryFunc)
                         .arg(QFileInfo(dbg.files[entryFile].filename).fileName());
    }

    xml.writeStartElement(lit("p"));
    xml.writeCharacters(shadername);
    xml.writeEndElement();

    if(sh.resourceId == ResourceId())
      return;
  }

  if(!shaderDetails)
    return;

  const VKPipe::Pipeline &pipeline =
      (sh.stage == ShaderStage::Compute ? m_Ctx.CurVulkanPipelineState()->compute
                                        : m_Ctx.CurVulkanPipelineState()->graphics);

  QList<QVariantList> uboRows;
  QList<QVariantList> roRows;
  QList<QVariantList> rwRows;
  QList<QVariantList> sampRows;

  for(const UsedDescriptor &used : m_Ctx.CurPipelineState().GetConstantBlocks(sh.stage))
  {
    if(used.access.stage != sh.stage)
      continue;

    const Descriptor &descriptor = used.descriptor;

    uint32_t dynamicOffset = 0;
    for(const VKPipe::DescriptorSet &set : pipeline.descriptorSets)
    {
      for(const VKPipe::DynamicOffset &offs : set.dynamicOffsets)
      {
        if(set.descriptorSetResourceId == used.access.descriptorStore &&
           offs.descriptorByteOffset == used.access.byteOffset)
        {
          dynamicOffset += offs.dynamicBufferByteOffset;
        }
      }
    }

    QString name = m_Ctx.GetResourceName(descriptor.resource);
    uint64_t byteOffset = descriptor.byteOffset + dynamicOffset;
    uint64_t length = descriptor.byteSize;
    int numvars = 0;
    uint32_t bindByteSize = 0;
    QString slotname;

    if(used.access.index == DescriptorAccess::NoShaderBinding)
    {
      slotname = m_Locations[{used.access.descriptorStore, used.access.byteOffset}].logicalBindName;

      slotname += QFormatStr("[%1]").arg(used.access.arrayElement);
    }
    else
    {
      const ConstantBlock &b = shaderDetails->constantBlocks[used.access.index];

      // push constants
      if(!b.bufferBacked)
      {
        if(b.compileConstants)
          name = tr("Specialization constants");
        else
          name = tr("Push constants");

        qulonglong offset = 0, size = 0;

        offset = byteOffset;
        size = descriptor.byteSize;

        // could maybe get range/size from ShaderVariable.reg if it's filled out
        // from SPIR-V side.
        uboRows.push_back({b.name, name, offset, size, b.variables.count(), b.byteSize});

        continue;
      }

      if(IsPushSet(used.access.stage, used.access.descriptorStore))
        slotname = tr("Push ");

      slotname += QFormatStr("Set %1, %2").arg(b.fixedBindSetOrSpace).arg(b.fixedBindNumber);

      if(!b.name.empty())
        slotname += lit(": ") + b.name;

      if(b.bindArraySize > 1)
        slotname += QFormatStr("[%1]").arg(used.access.arrayElement);

      numvars = b.variables.count();
      bindByteSize = b.byteSize;
    }

    if(descriptor.flags & DescriptorFlags::InlineData)
      name = tr("Inline block");

    if(descriptor.resource == ResourceId())
    {
      name = tr("Empty");
      length = 0;
    }

    BufferDescription *buf = m_Ctx.GetBuffer(descriptor.resource);
    if(buf)
    {
      if(length == UINT64_MAX)
        length = buf->length - byteOffset;
    }

    uboRows.push_back(
        {slotname, name, (qulonglong)byteOffset, (qulonglong)length, numvars, bindByteSize});
  }

  for(const UsedDescriptor &used : m_Ctx.CurPipelineState().GetReadOnlyResources(sh.stage))
  {
    if(used.access.stage != sh.stage)
      continue;

    const Descriptor &descriptor = used.descriptor;

    uint32_t dynamicOffset = 0;
    for(const VKPipe::DescriptorSet &set : pipeline.descriptorSets)
    {
      for(const VKPipe::DynamicOffset &offs : set.dynamicOffsets)
      {
        if(set.descriptorSetResourceId == used.access.descriptorStore &&
           offs.descriptorByteOffset == used.access.byteOffset)
        {
          dynamicOffset += offs.dynamicBufferByteOffset;
        }
      }
    }

    exportDescriptorHTML(used, sh.reflection, descriptor, dynamicOffset, roRows);
  }

  for(const UsedDescriptor &used : m_Ctx.CurPipelineState().GetReadWriteResources(sh.stage))
  {
    if(used.access.stage != sh.stage)
      continue;

    const Descriptor &descriptor = used.descriptor;

    uint32_t dynamicOffset = 0;
    for(const VKPipe::DescriptorSet &set : pipeline.descriptorSets)
    {
      for(const VKPipe::DynamicOffset &offs : set.dynamicOffsets)
      {
        if(set.descriptorSetResourceId == used.access.descriptorStore &&
           offs.descriptorByteOffset == used.access.byteOffset)
        {
          dynamicOffset += offs.dynamicBufferByteOffset;
        }
      }
    }

    exportDescriptorHTML(used, sh.reflection, descriptor, dynamicOffset, rwRows);
  }

  for(const UsedDescriptor &used : m_Ctx.CurPipelineState().GetSamplers(sh.stage))
  {
    if(used.access.stage != sh.stage)
      continue;

    const SamplerDescriptor &descriptor = used.sampler;
    const ShaderSampler *shaderSamp = NULL;

    if(used.access.index < sh.reflection->samplers.size())
      shaderSamp = &sh.reflection->samplers[used.access.index];

    {
      QString borderColor;

      if(descriptor.borderColorType == CompType::Float)
        borderColor = QFormatStr("%1, %2, %3, %4")
                          .arg(descriptor.borderColorValue.floatValue[0])
                          .arg(descriptor.borderColorValue.floatValue[1])
                          .arg(descriptor.borderColorValue.floatValue[2])
                          .arg(descriptor.borderColorValue.floatValue[3]);
      else
        borderColor = QFormatStr("%1, %2, %3, %4")
                          .arg(descriptor.borderColorValue.uintValue[0])
                          .arg(descriptor.borderColorValue.uintValue[1])
                          .arg(descriptor.borderColorValue.uintValue[2])
                          .arg(descriptor.borderColorValue.uintValue[3]);

      QString addressing;

      QString addPrefix;
      QString addVal;

      QString addr[] = {ToQStr(descriptor.addressU, GraphicsAPI::D3D12),
                        ToQStr(descriptor.addressV, GraphicsAPI::D3D12),
                        ToQStr(descriptor.addressW, GraphicsAPI::D3D12)};

      // arrange like either UVW: WRAP or UV: WRAP, W: CLAMP
      for(int a = 0; a < 3; a++)
      {
        const QString str[] = {lit("U"), lit("V"), lit("W")};
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

      if(descriptor.UseBorder())
        addressing += QFormatStr("<%1>").arg(borderColor);

      if(descriptor.unnormalized)
        addressing += lit(" (Un-norm)");

      QString filter = ToQStr(descriptor.filter);

      if(descriptor.maxAnisotropy > 1)
        filter += QFormatStr(" %1x").arg(descriptor.maxAnisotropy);

      if(descriptor.filter.filter == FilterFunction::Comparison)
        filter += QFormatStr(" (%1)").arg(ToQStr(descriptor.compareFunction));
      else if(descriptor.filter.filter != FilterFunction::Normal)
        filter += QFormatStr(" (%1)").arg(ToQStr(descriptor.filter.filter));

      QString slotname;
      if(used.access.index == DescriptorAccess::NoShaderBinding)
      {
        slotname = m_Locations[{used.access.descriptorStore, used.access.byteOffset}].logicalBindName;

        slotname += QFormatStr("[%1]").arg(used.access.arrayElement);
      }
      else if(shaderSamp)
      {
        if(IsPushSet(used.access.stage, used.access.descriptorStore))
          slotname = tr("Push ");

        slotname +=
            QFormatStr("Set %1, %2").arg(shaderSamp->fixedBindSetOrSpace).arg(shaderSamp->fixedBindNumber);

        if(!shaderSamp->name.empty())
          slotname += lit(": ") + shaderSamp->name;

        if(shaderSamp->bindArraySize > 1)
          slotname += QFormatStr("[%1]").arg(used.access.arrayElement);
      }

      sampRows.push_back(
          {slotname, addressing, filter,
           QFormatStr("%1 - %2")
               .arg(descriptor.minLOD == -FLT_MAX ? lit("0") : QString::number(descriptor.minLOD))
               .arg(descriptor.maxLOD == FLT_MAX ? lit("FLT_MAX")
                                                 : QString::number(descriptor.maxLOD)),
           descriptor.mipBias});
    }
  }

  if(!roRows.empty())
  {
    xml.writeStartElement(lit("h3"));
    xml.writeCharacters(tr("Read-only Resources"));
    xml.writeEndElement();

    m_Common.exportHTMLTable(
        xml,
        {tr("Binding"), tr("Resource"), tr("Type"), tr("Width"), tr("Height"), tr("Depth"),
         tr("Array Size"), tr("Resource Format"), tr("View Parameters")},
        roRows);
  }

  if(!rwRows.empty())
  {
    xml.writeStartElement(lit("h3"));
    xml.writeCharacters(tr("Read-write Resources"));
    xml.writeEndElement();

    m_Common.exportHTMLTable(
        xml,
        {tr("Binding"), tr("Resource"), tr("Type"), tr("Width"), tr("Height"), tr("Depth"),
         tr("Array Size"), tr("Resource Format"), tr("View Parameters")},
        rwRows);
  }

  if(!sampRows.empty())
  {
    xml.writeStartElement(lit("h3"));
    xml.writeCharacters(tr("Samplers"));
    xml.writeEndElement();

    m_Common.exportHTMLTable(
        xml, {tr("Binding"), tr("Addressing"), tr("Filter"), tr("LOD Clamp"), tr("LOD Bias")},
        sampRows);
  }

  if(!uboRows.empty())
  {
    xml.writeStartElement(lit("h3"));
    xml.writeCharacters(tr("UBOs"));
    xml.writeEndElement();

    m_Common.exportHTMLTable(xml,
                             {tr("Binding"), tr("Buffer"), tr("Byte Offset"), tr("Byte Size"),
                              tr("Number of Variables"), tr("Bytes Needed")},
                             uboRows);
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

    m_Common.exportHTMLTable(xml,
                             {tr("Slot"), tr("Buffer"), tr("Byte Offset"), tr("Byte Length"),
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

    m_Common.exportHTMLTable(xml,
                             {
                                 tr("Depth Clamp Enable"),
                                 tr("Depth Clip Enable"),
                                 tr("Rasterizer Discard Enable"),
                             },
                             {
                                 rs.depthClampEnable ? tr("Yes") : tr("No"),
                                 rs.depthClipEnable ? tr("Yes") : tr("No"),
                                 rs.rasterizerDiscardEnable ? tr("Yes") : tr("No"),
                             });

    xml.writeStartElement(lit("p"));
    xml.writeEndElement();

    m_Common.exportHTMLTable(xml,
                             {tr("Depth Bias Enable"), tr("Depth Bias"), tr("Depth Bias Clamp"),
                              tr("Slope Scaled Bias"), tr("Line Width")},
                             {
                                 rs.depthBiasEnable ? tr("Yes") : tr("No"),
                                 Formatter::Format(rs.depthBias),
                                 Formatter::Format(rs.depthBiasClamp),
                                 Formatter::Format(rs.slopeScaledDepthBias),
                                 Formatter::Format(rs.lineWidth),
                             });
  }

  const VKPipe::MultiSample &msaa = m_Ctx.CurVulkanPipelineState()->multisample;

  {
    xml.writeStartElement(lit("h3"));
    xml.writeCharacters(tr("Multisampling State"));
    xml.writeEndElement();

    m_Common.exportHTMLTable(
        xml,
        {tr("Raster Samples"), tr("Sample-rate shading"), tr("Min Sample Shading Rate"),
         tr("Sample Mask")},
        {msaa.rasterSamples, msaa.sampleShadingEnable ? tr("Yes") : tr("No"),
         Formatter::Format(msaa.minSampleShading), Formatter::Format(msaa.sampleMask, true)});
  }

  const VKPipe::ViewState &vp = m_Ctx.CurVulkanPipelineState()->viewportScissor;

  {
    xml.writeStartElement(lit("h3"));
    xml.writeCharacters(tr("Viewports"));
    xml.writeEndElement();

    const QString ndcDepthRange = vp.depthNegativeOneToOne ? lit("[-1, 1]") : lit("[0, 1]");

    QList<QVariantList> rows;

    int i = 0;
    for(const VKPipe::ViewportScissor &vs : vp.viewportScissors)
    {
      const Viewport &v = vs.vp;

      rows.push_back({i, v.x, v.y, v.width, v.height, v.minDepth, v.maxDepth, ndcDepthRange});

      i++;
    }

    QStringList header = {tr("Slot"),   tr("X"),         tr("Y"),         tr("Width"),
                          tr("Height"), tr("Min Depth"), tr("Max Depth"), tr("NDC Depth Range")};
    m_Common.exportHTMLTable(xml, header, rows);
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
          logic ? ToQStr(cb.blends[0].logicOperation) : tr("Disabled"),
          blendConst,
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

  m_Common.exportHTMLTable(xml,
                           {
                               tr("Slot"),
                               tr("Blend Enable"),
                               tr("Blend Source"),
                               tr("Blend Destination"),
                               tr("Blend Operation"),
                               tr("Alpha Blend Source"),
                               tr("Alpha Blend Destination"),
                               tr("Alpha Blend Operation"),
                               tr("Write Mask"),
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
        xml,
        {tr("Depth Test Enable"), tr("Depth Writes Enable"), tr("Depth Function"), tr("Depth Bounds")},
        {
            ds.depthTestEnable ? tr("Yes") : tr("No"),
            ds.depthWriteEnable ? tr("Yes") : tr("No"),
            ToQStr(ds.depthFunction),
            ds.depthBoundsEnable ? QFormatStr("%1 - %2")
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
          tr("Front"),
          Formatter::Format(ds.frontFace.reference, true),
          Formatter::Format(ds.frontFace.compareMask, true),
          Formatter::Format(ds.frontFace.writeMask, true),
          ToQStr(ds.frontFace.function),
          ToQStr(ds.frontFace.passOperation),
          ToQStr(ds.frontFace.failOperation),
          ToQStr(ds.frontFace.depthFailOperation),
      });

      rows.push_back({
          tr("back"),
          Formatter::Format(ds.backFace.reference, true),
          Formatter::Format(ds.backFace.compareMask, true),
          Formatter::Format(ds.backFace.writeMask, true),
          ToQStr(ds.backFace.function),
          ToQStr(ds.backFace.passOperation),
          ToQStr(ds.backFace.failOperation),
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
    for(const Descriptor &a : pass.framebuffer.attachments)
    {
      TextureDescription *tex = m_Ctx.GetTexture(a.resource);

      QString name = m_Ctx.GetResourceName(a.resource);

      rows.push_back({i, name, tex->width, tex->height, tex->depth, tex->arraysize, a.firstMip,
                      a.numMips, a.firstSlice, a.numSlices,
                      getTextureRenderSamples(tex, pass.renderpass)});

      i++;
    }

    m_Common.exportHTMLTable(xml,
                             {
                                 tr("Slot"),
                                 tr("Image"),
                                 tr("Width"),
                                 tr("Height"),
                                 tr("Depth"),
                                 tr("Array Size"),
                                 tr("First mip"),
                                 tr("Number of mips"),
                                 tr("First array layer"),
                                 tr("Number of layers"),
                                 tr("Sample Count"),
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

    if(!pass.renderpass.resolveAttachments.isEmpty())
    {
      QList<QVariantList> resolves;

      for(int i = 0; i < pass.renderpass.resolveAttachments.count(); i++)
        resolves.push_back({pass.renderpass.resolveAttachments[i]});

      m_Common.exportHTMLTable(xml,
                               {
                                   tr("Resolve Attachment"),
                               },
                               resolves);

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

    if(pass.renderpass.depthstencilResolveAttachment >= 0)
    {
      xml.writeStartElement(lit("p"));
      xml.writeCharacters(tr("Depth-stencil Resolve Attachment: %1")
                              .arg(pass.renderpass.depthstencilResolveAttachment));
      xml.writeEndElement();
    }

    if(pass.renderpass.fragmentDensityAttachment >= 0)
    {
      xml.writeStartElement(lit("p"));
      xml.writeCharacters(
          tr("Fragment Density Attachment: %1").arg(pass.renderpass.fragmentDensityAttachment));
      if(pass.renderpass.fragmentDensityOffsets.size() > 0)
      {
        xml.writeCharacters(
            tr(". Rendering with %1 offsets : ").arg(pass.renderpass.fragmentDensityOffsets.size()));
        for(uint32_t j = 0; j < pass.renderpass.fragmentDensityOffsets.size(); j++)
        {
          const Offset &o = pass.renderpass.fragmentDensityOffsets[j];
          if(j > 0)
            xml.writeCharacters(tr(", "));

          xml.writeCharacters(tr(" %1x%2").arg(o.x).arg(o.y));
        }
      }
      xml.writeEndElement();
    }

    if(pass.renderpass.shadingRateAttachment >= 0)
    {
      xml.writeStartElement(lit("p"));
      xml.writeCharacters(tr("Fragment Shading Rate Attachment: %1 (texel size %2x%3)")
                              .arg(pass.renderpass.shadingRateAttachment)
                              .arg(pass.renderpass.shadingRateTexelSize.first)
                              .arg(pass.renderpass.shadingRateTexelSize.second));
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
          cr.isPassing ? tr("Yes") : tr("No"),
          cr.isInverted ? tr("Yes") : tr("No"),
          bufferName,
          (qulonglong)cr.byteOffset,
      });
}

const ShaderResource *VulkanPipelineStateViewer::exportDescriptorHTML(const UsedDescriptor &used,
                                                                      const ShaderReflection *refl,
                                                                      const Descriptor &descriptor,
                                                                      uint32_t dynamicOffset,
                                                                      QList<QVariantList> &rows)
{
  const ShaderResource *shaderRes = NULL;

  if(IsReadOnlyDescriptor(used.access.type))
  {
    if(used.access.index < refl->readOnlyResources.size())
      shaderRes = &refl->readOnlyResources[used.access.index];
  }
  else
  {
    if(used.access.index < refl->readWriteResources.size())
      shaderRes = &refl->readWriteResources[used.access.index];
  }

  QString slotname;

  if(used.access.index == DescriptorAccess::NoShaderBinding)
  {
    slotname = m_Locations[{used.access.descriptorStore, used.access.byteOffset}].logicalBindName;

    slotname += QFormatStr("[%1]").arg(used.access.arrayElement);
  }
  else if(shaderRes)
  {
    if(IsPushSet(used.access.stage, used.access.descriptorStore))
      slotname = tr("Push ");

    slotname +=
        QFormatStr("Set %1, %2").arg(shaderRes->fixedBindSetOrSpace).arg(shaderRes->fixedBindNumber);

    if(!shaderRes->name.empty())
      slotname += lit(": ") + shaderRes->name;

    if(shaderRes->bindArraySize > 1)
      slotname += QFormatStr("[%1]").arg(used.access.arrayElement);
  }

  ResourceId id = descriptor.resource;

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
      viewParams =
          tr("Mips: %1-%2").arg(descriptor.firstMip).arg(descriptor.firstMip + descriptor.numMips - 1);
    }

    if(tex->arraysize > 1)
    {
      if(!viewParams.isEmpty())
        viewParams += lit(", ");
      viewParams += tr("Layers: %1-%2")
                        .arg(descriptor.firstSlice)
                        .arg(descriptor.firstSlice + descriptor.numSlices - 1);
    }
  }

  if(buf)
  {
    w = buf->length;
    h = 0;
    d = 0;
    arr = 0;
    format = lit("-");

    viewParams = tr("Byte Range: %1").arg(formatByteRange(buf, descriptor, dynamicOffset));
  }

  if(descriptor.type != DescriptorType::Sampler)
    rows.push_back(
        {slotname, name, ToQStr(descriptor.type), (qulonglong)w, h, d, arr, format, viewParams});

  if(descriptor.type == DescriptorType::ImageSampler)
  {
    QString samplerName = m_Ctx.GetResourceName(used.sampler.object);

    if(used.sampler.object == ResourceId())
      samplerName = tr("Empty");

    QVariantList sampDetails = makeSampler(QString(), used.sampler);
    rows.push_back({slotname, samplerName, ToQStr(descriptor.type), QString(), QString(), QString(),
                    QString(), sampDetails[3], sampDetails[4]});
  }
  return shaderRes;
}

QString VulkanPipelineStateViewer::GetFossilizeHash(ResourceId id)
{
  uint h = qHash(ToQStr(id));

  if(id == ResourceId())
    h = 0;

  return QFormatStr("%1").arg(h, 16, 16, QLatin1Char('0'));
}

QString VulkanPipelineStateViewer::GetFossilizeFilename(QDir d, uint32_t tag, ResourceId id)
{
  return d.absoluteFilePath(
      lit("%1.%2.json").arg(tag, 2, 16, QLatin1Char('0')).arg(GetFossilizeHash(id)));
}

QByteArray VulkanPipelineStateViewer::ReconstructSpecializationData(const VKPipe::Shader &sh,
                                                                    const SDObject *mapEntries)
{
  bytebuf specData;

  if(mapEntries->NumChildren() == 0)
    return specData;

  if(sh.reflection == NULL)
  {
    qCritical("Tried to reconstruct specialization constants but reflection data is missing");
    return specData;
  }
  auto specBlockIt =
      std::find_if(sh.reflection->constantBlocks.begin(), sh.reflection->constantBlocks.end(),
                   [](const ConstantBlock &block) { return block.compileConstants; });
  if(specBlockIt == sh.reflection->constantBlocks.end())
  {
    qCritical("Cannot find the constant block for specialization constants");
    return specData;
  }
  const rdcarray<ShaderConstant> &specVars = specBlockIt->variables;

  // We don't have access to the buffers in the original creation info, so we try to reconstruct
  // from our preprocessed pipeline state instead. Note that this data might have a different order
  // from the original call or have unused entries eliminated based on shader reflection.
  const bytebuf &src = sh.specializationData;

  for(size_t i = 0; i < mapEntries->NumChildren(); i++)
  {
    const SDObject *map = mapEntries->GetChild(i);

    size_t dstByteOffset = map->FindChild("offset")->AsUInt32();
    size_t size = map->FindChild("size")->AsUInt32();
    specData.resize_for_index(dstByteOffset + size - 1);

    uint32_t constantId = map->FindChild("constantID")->AsUInt32();
    int32_t idx = sh.specializationIds.indexOf(constantId);
    if(idx == -1)
      continue;    // Entry was eliminated as it was probably unused --- skip it
    size_t srcByteOffset = specVars[idx].byteOffset;
    Q_ASSERT(srcByteOffset + size <= src.size());

    memcpy(specData.data() + dstByteOffset, src.data() + srcByteOffset, size);
  }

  return specData;
}

QString VulkanPipelineStateViewer::GetBufferForFossilize(const SDObject *obj)
{
  const VKPipe::State *pipe = m_Ctx.CurVulkanPipelineState();

  QByteArray ret;
  if(obj->name == "pData" && obj->GetParent() && obj->GetParent()->name == "pSpecializationInfo")
  {
    const SDObject *shad = obj->GetParent()->GetParent();
    const SDObject *stage = NULL;
    if(shad)
      stage = shad->FindChild("stage");

    const SDObject *mapEntries = obj->GetParent()->FindChild("pMapEntries");

    if(stage)
    {
      switch(ShaderStageMask(stage->AsUInt32()))
      {
        case ShaderStageMask::Vertex:
          ret = ReconstructSpecializationData(pipe->vertexShader, mapEntries);
          break;
        case ShaderStageMask::Tess_Control:
          ret = ReconstructSpecializationData(pipe->tessControlShader, mapEntries);
          break;
        case ShaderStageMask::Tess_Eval:
          ret = ReconstructSpecializationData(pipe->tessEvalShader, mapEntries);
          break;
        case ShaderStageMask::Geometry:
          ret = ReconstructSpecializationData(pipe->geometryShader, mapEntries);
          break;
        case ShaderStageMask::Pixel:
          ret = ReconstructSpecializationData(pipe->fragmentShader, mapEntries);
          break;
        case ShaderStageMask::Compute:
          ret = ReconstructSpecializationData(pipe->computeShader, mapEntries);
          break;
        case ShaderStageMask::Task:
          ret = ReconstructSpecializationData(pipe->taskShader, mapEntries);
          break;
        case ShaderStageMask::Mesh:
          ret = ReconstructSpecializationData(pipe->meshShader, mapEntries);
          break;
        default: break;
      }
    }

    const SDObject *size = obj->GetParent()->FindChild("dataSize");
    if(size)
    {
      Q_ASSERT((uint32_t)ret.size() <= size->AsUInt32());
      ret.resize(size->AsUInt32());
    }
  }
  return QString::fromLatin1(ret.toBase64());
}

void VulkanPipelineStateViewer::AddFossilizeNexts(QVariantMap &info, const SDObject *baseStruct)
{
  QVariantList nexts;

  while(baseStruct)
  {
    const SDObject *next = baseStruct->FindChild("pNext");

    if(next && next->type.basetype != SDBasic::Null)
    {
      QVariant v = ConvertSDObjectToFossilizeJSON(
          next, {
                    // VkPipelineVertexInputDivisorStateCreateInfoEXT
                    {"pVertexBindingDivisors", "vertexBindingDivisors"},
                    // VkRenderPassMultiviewCreateInfo
                    {"subpassCount", ""},
                    {"pViewMasks", "viewMasks"},
                    {"dependencyCount", ""},
                    {"pViewOffsets", "viewOffsets"},
                    {"correlationMaskCount", ""},
                    {"pCorrelationMasks", "correlationMasks"},
                    // VkDescriptorSetLayoutBindingFlagsCreateInfoEXT
                    {"bindingCount", ""},
                    {"pBindingFlags", "bindingFlags"},
                    // VkSubpassDescriptionDepthStencilResolve
                    {"pDepthStencilResolveAttachment", "depthStencilResolveAttachment"},
                    // VkFragmentShadingRateAttachmentInfoKHR
                    {"pFragmentShadingRateAttachment", "fragmentShadingRateAttachment"},
                    // VkPipelineRenderingCreateInfo
                    {"colorAttachmentCount", ""},
                    {"pColorAttachmentFormats", "colorAttachmentFormats"},
                });

      QVariantMap &vm = (QVariantMap &)v.data_ptr();

      vm[lit("sType")] = next->FindChild("sType")->AsUInt32();
      nexts.push_back(v);
      baseStruct = next;
    }
    else
    {
      break;
    }
  }

  if(!nexts.empty())
  {
    info[lit("pNext")] = nexts;
  }
}

QVariant VulkanPipelineStateViewer::ConvertSDObjectToFossilizeJSON(const SDObject *obj,
                                                                   QMap<QByteArray, QByteArray> renames)
{
  switch(obj->type.basetype)
  {
    case SDBasic::Chunk:
    case SDBasic::Struct:
    {
      QVariantMap map;
      for(size_t i = 0; i < obj->NumChildren(); i++)
      {
        const SDObject *ch = obj->GetChild(i);

        if(ch->name == "sType" || ch->name == "pNext" || ch->name == "pNextType")
          continue;

        QByteArray name(ch->name.c_str(), (int)ch->name.size());

        auto it = renames.find(name);
        if(it != renames.end())
          name = it.value();

        if(name.isEmpty())
          continue;

        QString key = QString::fromLatin1(name);

        QVariant v = ConvertSDObjectToFossilizeJSON(ch, renames);
        if(v.isValid())
          map[key] = v;
      }

      AddFossilizeNexts(map, obj);

      if(map.contains(lit("sampleMask")))
      {
        QVariantList sampleMaskArray = QVariantList({map[lit("sampleMask")]});
        map[lit("sampleMask")] = sampleMaskArray;
      }

      return map;
    }
    case SDBasic::Null: break;
    case SDBasic::Buffer: return GetBufferForFossilize(obj);
    case SDBasic::Array:
    {
      if(obj->NumChildren() == 0)
        return QVariant();

      QVariantList list;
      for(size_t j = 0; j < obj->NumChildren(); j++)
        list.push_back(ConvertSDObjectToFossilizeJSON(obj->GetChild(j), renames));
      return list;
    }
    case SDBasic::String: return QString(obj->AsString());
    case SDBasic::Enum:
    case SDBasic::UnsignedInteger: return (qulonglong)obj->AsUInt64();
    case SDBasic::SignedInteger: return (qlonglong)obj->AsInt64();
    case SDBasic::Float: return obj->AsDouble();
    case SDBasic::Boolean: return obj->AsBool() ? 1U : 0U;
    case SDBasic::Character: return QString(QLatin1Char(obj->AsChar()));
    case SDBasic::Resource: return GetFossilizeHash(obj->AsResourceId());
  }

  return QVariant();
}

void VulkanPipelineStateViewer::EncodeFossilizeVarint(const bytebuf &spirv, bytebuf &varint)
{
  if((spirv.size() % 4) != 0)
    return;

  const uint32_t *curWord = (const uint32_t *)spirv.data();

  varint.reserve(spirv.size() / 2);

  for(size_t i = 0; i < spirv.size(); i += 4)
  {
    uint32_t w = *curWord;

    do
    {
      if(w <= 0x7f)
        varint.push_back(uint8_t(w));
      else
        varint.push_back(uint8_t(w & 0x7fU) | 0x80U);

      w >>= 7;
    } while(w);

    curWord++;
  }
}

void VulkanPipelineStateViewer::WriteFossilizeJSON(QIODevice &f, QVariantMap &contents)
{
  contents[lit("version")] = 6;

  QJsonDocument doc = QJsonDocument::fromVariant(contents);

  QByteArray jsontext = doc.toJson(QJsonDocument::Compact);

  f.write(jsontext);
}

void VulkanPipelineStateViewer::exportFOZ(QString dir, ResourceId pso)
{
  enum
  {
    TagAppInfo = 0,
    TagSampler = 1,
    TagDescriptorSetLayout = 2,
    TagPipelineLayout = 3,
    TagShaderModule = 4,
    TagRenderPass = 5,
    TagGraphicsPipe = 6,
    TagComputePipe = 7,
  };

  QDir d(dir);

  const SDFile &sdfile = m_Ctx.GetStructuredFile();

  const VKPipe::State *pipe = m_Ctx.CurVulkanPipelineState();

  // enumerate all the parents of the pipeline, and cache the name of the first initialisation
  // chunk (easy way to find things by type)
  rdcarray<rdcpair<rdcstr, const ResourceDescription *>> resources;

  {
    rdcarray<ResourceId> todo;
    rdcarray<ResourceId> done;

    todo.push_back(pso);

    while(!todo.empty())
    {
      ResourceId cur = todo.back();
      todo.pop_back();

      const ResourceDescription *desc = m_Ctx.GetResource(cur);
      resources.push_back({sdfile.chunks[desc->initialisationChunks[0]]->name, desc});
      done.push_back(cur);

      for(ResourceId parent : desc->parentResources)
      {
        if(!done.contains(parent))
          todo.push_back(parent);
      }
    }
  }

  {
    const ResourceDescription *instance = NULL;
    const ResourceDescription *device = NULL;

    for(size_t i = 0; i < resources.size(); i++)
    {
      if(resources[i].first == "vkCreateInstance")
        instance = resources[i].second;
      else if(resources[i].first == "vkCreateDevice")
        device = resources[i].second;
    }

    if(!instance || instance->type != ResourceType::Device)
    {
      RDDialog::critical(this, tr("Couldn't locate instance"),
                         tr("Couldn't locate VkInstance from current PSO!"));
      return;
    }
    if(!device || device->type != ResourceType::Device)
    {
      RDDialog::critical(this, tr("Couldn't locate device"),
                         tr("Couldn't locate VkDevice from current PSO!"));
      return;
    }

    QFile f(GetFossilizeFilename(d, TagAppInfo, instance->resourceId));
    if(f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
    {
      QVariantMap instanceData;

      const SDChunk *instCreate = sdfile.chunks[instance->initialisationChunks[0]];

      QVariantMap appInfo;
      QVariantMap physicalDeviceFeatures;

      const SDObject *apiVersion = instCreate->FindChildRecursively("APIVersion");
      if(apiVersion && apiVersion->AsUInt32() > 0)
      {
        appInfo[lit("applicationName")] = instCreate->FindChildRecursively("AppName")->AsString();
        appInfo[lit("engineName")] = instCreate->FindChildRecursively("EngineName")->AsString();
        appInfo[lit("applicationVersion")] =
            instCreate->FindChildRecursively("AppVersion")->AsUInt32();
        appInfo[lit("engineVersion")] = instCreate->FindChildRecursively("EngineVersion")->AsUInt32();
        appInfo[lit("apiVersion")] = apiVersion->AsUInt32();
      }

      const SDChunk *devCreate = sdfile.chunks[device->initialisationChunks[0]];

      // this is a recursive search so we don't need to care if it's in PDF or PDF2
      const SDObject *robustBufferAccess = devCreate->FindChildRecursively("robustBufferAccess");
      if(robustBufferAccess)
      {
        physicalDeviceFeatures[lit("robustBufferAccess")] = robustBufferAccess->AsUInt32();
      }

      instanceData[lit("applicationInfo")] = appInfo;
      instanceData[lit("physicalDeviceFeatures")] = physicalDeviceFeatures;

      WriteFossilizeJSON(f, instanceData);
    }
  }

  for(size_t i = 0; i < resources.size(); i++)
  {
    const SDChunk *create = sdfile.chunks[resources[i].second->initialisationChunks[0]];

    ResourceId id = resources[i].second->resourceId;

    if(resources[i].first == "vkCreateSampler")
    {
      QFile f(GetFossilizeFilename(d, TagSampler, id));
      if(f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
      {
        const SDObject *createInfo = create->FindChildRecursively("CreateInfo");

        QVariant samplerData = ConvertSDObjectToFossilizeJSON(createInfo, {});

        QVariantMap root({{lit("samplers"), QVariantMap({{GetFossilizeHash(id), samplerData}})}});

        WriteFossilizeJSON(f, root);
      }
    }
    else if(resources[i].first == "vkCreateDescriptorSetLayout")
    {
      QFile f(GetFossilizeFilename(d, TagDescriptorSetLayout, id));
      if(f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
      {
        const SDObject *createInfo = create->FindChildRecursively("CreateInfo");

        QVariant layoutData =
            ConvertSDObjectToFossilizeJSON(createInfo, {
                                                           {"bindingCount", ""},
                                                           {"pBindings", "bindings"},
                                                       });

        QVariantMap root({{lit("setLayouts"), QVariantMap({{GetFossilizeHash(id), layoutData}})}});

        WriteFossilizeJSON(f, root);
      }
    }
    else if(resources[i].first == "vkCreatePipelineLayout")
    {
      QFile f(GetFossilizeFilename(d, TagPipelineLayout, id));
      if(f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
      {
        const SDObject *createInfo = create->FindChildRecursively("CreateInfo");

        QVariant layoutData = ConvertSDObjectToFossilizeJSON(
            createInfo, {
                            {"setLayoutCount", ""},
                            {"pSetLayouts", "setLayouts"},
                            {"pushConstantRangeCount", ""},
                            {"pPushConstantRanges", "pushConstantRanges"},
                        });

        QVariantMap root(
            {{lit("pipelineLayouts"), QVariantMap({{GetFossilizeHash(id), layoutData}})}});

        WriteFossilizeJSON(f, root);
      }
    }
    else if(resources[i].first == "vkCreateRenderPass" ||
            resources[i].first == "vkCreateRenderPass2")
    {
      QFile f(GetFossilizeFilename(d, TagRenderPass, id));
      if(f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
      {
        const SDObject *createInfo = create->FindChildRecursively("CreateInfo");

        QVariant layoutData = ConvertSDObjectToFossilizeJSON(
            createInfo, {
                            {"attachmentCount", ""},
                            {"pAttachments", "attachments"},
                            {"dependencyCount", ""},
                            {"pDependencies", "dependencies"},
                            {"subpassCount", ""},
                            {"pSubpasses", "subpasses"},
                            {"pDepthStencilAttachment", "depthStencilAttachment"},
                            {"colorAttachmentCount", ""},
                            {"pColorAttachments", "colorAttachments"},
                            {"inputAttachmentCount", ""},
                            {"pInputAttachments", "inputAttachments"},
                            {"preserveAttachmentCount", ""},
                            {"pPreserveAttachments", "preserveAttachments"},
                            {"resolveAttachmentCount", ""},
                            {"pResolveAttachments", "resolveAttachments"},
                        });

        QVariantMap root({{lit("renderPasses"), QVariantMap({{GetFossilizeHash(id), layoutData}})}});

        WriteFossilizeJSON(f, root);
      }
    }
    else if(resources[i].first == "vkCreateGraphicsPipelines")
    {
      QFile f(GetFossilizeFilename(d, TagGraphicsPipe, id));
      if(f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
      {
        const SDObject *createInfo = create->FindChildRecursively("CreateInfo");

        QVariant layoutData = ConvertSDObjectToFossilizeJSON(
            createInfo, {
                            {"pName", "name"},
                            {"mapEntryCount", ""},
                            {"pMapEntries", "mapEntries"},
                            {"pSpecializationInfo", "specializationInfo"},
                            {"pData", "data"},
                            {"pTessellationState", "tessellationState"},
                            {"pDynamicState", "dynamicState"},
                            {"pMultisampleState", "multisampleState"},
                            {"pSampleMask", "sampleMask"},
                            {"pVertexInputState", "vertexInputState"},
                            {"vertexAttributeDescriptionCount", ""},
                            {"vertexBindingDescriptionCount", ""},
                            {"pVertexAttributeDescriptions", "attributes"},
                            {"pVertexBindingDescriptions", "bindings"},
                            {"pRasterizationState", "rasterizationState"},
                            {"pInputAssemblyState", "inputAssemblyState"},
                            {"pColorBlendState", "colorBlendState"},
                            {"attachmentCount", ""},
                            {"pAttachments", "attachments"},
                            {"pViewportState", "viewportState"},
                            {"dynamicStateCount", ""},
                            {"pDynamicStates", "dynamicState"},
                            {"pViewports", "viewports"},
                            {"pScissors", "scissors"},
                            {"pDepthStencilState", "depthStencilState"},
                            {"stageCount", ""},
                            {"pStages", "stages"},
                        });

        QVariantMap root(
            {{lit("graphicsPipelines"), QVariantMap({{GetFossilizeHash(id), layoutData}})}});

        WriteFossilizeJSON(f, root);
      }
    }
    else if(resources[i].first == "vkCreateComputePipelines")
    {
      QFile f(GetFossilizeFilename(d, TagComputePipe, id));
      if(f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
      {
        const SDObject *createInfo = create->FindChildRecursively("CreateInfo");

        QVariant layoutData = ConvertSDObjectToFossilizeJSON(
            createInfo, {
                            {"pName", "name"},
                            {"mapEntryCount", ""},
                            {"pMapEntries", "mapEntries"},
                            {"pSpecializationInfo", "specializationInfo"},
                            {"pData", "data"},
                        });

        QVariantMap root(
            {{lit("computePipelines"), QVariantMap({{GetFossilizeHash(id), layoutData}})}});

        WriteFossilizeJSON(f, root);
      }
    }
    else if(resources[i].first == "vkCreateShaderModule")
    {
      QFile f(GetFossilizeFilename(d, TagShaderModule, id));
      if(f.open(QIODevice::WriteOnly | QIODevice::Truncate))
      {
        // shaders we handle specially
        QVariantMap shaderData;

        const bytebuf *spirv = NULL;

        // we don't care which reflection we get, as long as the ID matches
        for(const VKPipe::Shader *sh :
            {&pipe->taskShader, &pipe->meshShader, &pipe->vertexShader, &pipe->tessControlShader,
             &pipe->tessEvalShader, &pipe->geometryShader, &pipe->fragmentShader,
             &pipe->computeShader})
        {
          if(sh->resourceId == id)
            spirv = &sh->reflection->rawBytes;
        }

        if(!spirv)
        {
          RDDialog::critical(
              this, tr("Shader not found"),
              tr("Couldn't get SPIR-V bytes for bound shader %1").arg(m_Ctx.GetResourceName(id)));
          return;
        }

        bytebuf varint;

        EncodeFossilizeVarint(*spirv, varint);

        shaderData[lit("varintOffset")] = 0;
        shaderData[lit("varintSize")] = qulonglong(varint.size());
        shaderData[lit("codeSize")] = qulonglong(spirv->size());
        shaderData[lit("flags")] =
            create->FindChildRecursively("CreateInfo")->FindChild("flags")->AsUInt32();

        QVariantMap root({{lit("shaderModules"), QVariantMap({{GetFossilizeHash(id), shaderData}})}});

        WriteFossilizeJSON(f, root);

        f.write(QByteArray(1, '\0'));
        f.write((const char *)varint.data(), (qint64)varint.size());
      }
    }
  }
}

uint32_t VulkanPipelineStateViewer::getMinOffset(const rdcarray<ShaderConstant> &variables)
{
  uint32_t minOffset = ~0U;
  for(const ShaderConstant &v : variables)
    minOffset = qMin(v.byteOffset, minOffset);

  return minOffset;
}

void VulkanPipelineStateViewer::exportFOZ_clicked()
{
  if(!m_Ctx.IsCaptureLoaded())
    return;

  if(!m_Ctx.CurAction())
  {
    RDDialog::critical(this, tr("No action selected"),
                       tr("To export the pipeline as FOZ an action must be selected."));
    return;
  }

  ResourceId pso;

  if(m_Ctx.CurAction()->flags & ActionFlags::Dispatch)
    pso = m_Ctx.CurVulkanPipelineState()->compute.pipelineResourceId;
  else if(m_Ctx.CurAction()->flags & (ActionFlags::MeshDispatch | ActionFlags::Drawcall))
    pso = m_Ctx.CurVulkanPipelineState()->graphics.pipelineResourceId;

  if(pso == ResourceId())
  {
    RDDialog::critical(
        this, tr("No pipeline bound"),
        tr("To export the pipeline as FOZ an action must be selected which has a pipeline bound."));
    return;
  }

  QString dir = RDDialog::getExistingDirectory(this, tr("Export pipeline state as fossilize DB"));

  if(!dir.isEmpty())
    exportFOZ(dir, pso);
}

void VulkanPipelineStateViewer::exportHTML_clicked()
{
  if(!m_Ctx.IsCaptureLoaded())
    return;

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

      if(m_MeshPipe)
      {
        switch(stage)
        {
          case 0: exportHTML(xml, m_Ctx.CurVulkanPipelineState()->taskShader); break;
          case 1: exportHTML(xml, m_Ctx.CurVulkanPipelineState()->meshShader); break;
          case 2:
            exportHTML(xml, m_Ctx.CurVulkanPipelineState()->rasterizer);
            exportHTML(xml, m_Ctx.CurVulkanPipelineState()->conditionalRendering);
            break;
          case 3: exportHTML(xml, m_Ctx.CurVulkanPipelineState()->fragmentShader); break;
          case 4:
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
          case 5:
            exportHTML(xml, m_Ctx.CurVulkanPipelineState()->computeShader);
            exportHTML(xml, m_Ctx.CurVulkanPipelineState()->conditionalRendering);
            break;
        }
      }
      else
      {
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
      }

      xml.writeEndElement();

      stage++;
    }

    m_Common.endHTMLExport(xmlptr);
  }
}

void VulkanPipelineStateViewer::on_msMeshButton_clicked()
{
  if(!m_Ctx.HasMeshPreview())
    m_Ctx.ShowMeshPreview();
  ToolWindowManager::raiseToolWindow(m_Ctx.GetMeshPreview()->Widget());
}

void VulkanPipelineStateViewer::on_meshView_clicked()
{
  if(!m_Ctx.HasMeshPreview())
    m_Ctx.ShowMeshPreview();
  ToolWindowManager::raiseToolWindow(m_Ctx.GetMeshPreview()->Widget());
}

void VulkanPipelineStateViewer::on_computeDebugSelector_clicked()
{
  // Check whether debugging is valid for this event before showing the dialog
  if(!m_Ctx.APIProps().shaderDebugging)
    return;

  if(!m_Ctx.IsCaptureLoaded())
    return;

  const ActionDescription *action = m_Ctx.CurAction();

  if(!action)
    return;

  const ShaderReflection *shaderDetails =
      m_Ctx.CurPipelineState().GetShaderReflection(ShaderStage::Compute);

  if(!shaderDetails)
    return;

  RDDialog::show(m_ComputeDebugSelector);
}

void VulkanPipelineStateViewer::computeDebugSelector_beginDebug(
    const rdcfixedarray<uint32_t, 3> &group, const rdcfixedarray<uint32_t, 3> &thread)
{
  const ActionDescription *action = m_Ctx.CurAction();

  if(!action)
    return;

  const ShaderReflection *shaderDetails =
      m_Ctx.CurPipelineState().GetShaderReflection(ShaderStage::Compute);

  if(!shaderDetails)
    return;

  struct threadSelect
  {
    rdcfixedarray<uint32_t, 3> g;
    rdcfixedarray<uint32_t, 3> t;
  } debugThread = {
      // g[]
      {group[0], group[1], group[2]},
      // t[]
      {thread[0], thread[1], thread[2]},
  };

  bool done = false;
  ShaderDebugTrace *trace = NULL;

  m_Ctx.Replay().AsyncInvoke([&trace, &done, debugThread](IReplayController *r) {
    trace = r->DebugThread(debugThread.g, debugThread.t);

    if(trace->debugger == NULL)
    {
      r->FreeTrace(trace);
      trace = NULL;
    }

    done = true;
  });

  QString debugContext = lit("Group [%1,%2,%3] Thread [%4,%5,%6]")
                             .arg(group[0])
                             .arg(group[1])
                             .arg(group[2])
                             .arg(thread[0])
                             .arg(thread[1])
                             .arg(thread[2]);

  // wait a short while before displaying the progress dialog (which won't show if we're already
  // done by the time we reach it)
  for(int i = 0; !done && i < 100; i++)
    QThread::msleep(5);

  ShowProgressDialog(this, tr("Debugging %1").arg(debugContext), [&done]() { return done; });

  if(!trace)
  {
    RDDialog::critical(
        this, tr("Error debugging"),
        tr("Error debugging thread - make sure a valid group and thread is selected"));
    return;
  }

  // viewer takes ownership of the trace
  IShaderViewer *s = m_Ctx.DebugShader(
      shaderDetails, m_Ctx.CurPipelineState().GetComputePipelineObject(), trace, debugContext);

  m_Ctx.AddDockWindow(s->Widget(), DockReference::AddTo, this);
}
