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

#include "D3D11PipelineStateViewer.h"
#include <float.h>
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
#include "ui_D3D11PipelineStateViewer.h"

struct D3D11VBIBTag
{
  D3D11VBIBTag() { offset = 0; }
  D3D11VBIBTag(ResourceId i, uint64_t offs, QString f = QString())
  {
    id = i;
    offset = offs;
    format = f;
  }

  ResourceId id;
  uint64_t offset;
  QString format;
};

Q_DECLARE_METATYPE(D3D11VBIBTag);

struct D3D11ViewTag
{
  enum ResType
  {
    SRV,
    UAV,
    OMTarget,
    OMDepth,
  };

  D3D11ViewTag() : type(SRV), index(0) {}
  D3D11ViewTag(ResType t, uint32_t i, const Descriptor &d) : type(t), index(i), desc(d) {}
  ResType type;
  uint32_t index;
  Descriptor desc;
};

Q_DECLARE_METATYPE(D3D11ViewTag);

D3D11PipelineStateViewer::D3D11PipelineStateViewer(ICaptureContext &ctx,
                                                   PipelineStateViewer &common, QWidget *parent)
    : QFrame(parent), ui(new Ui::D3D11PipelineStateViewer), m_Ctx(ctx), m_Common(common)
{
  ui->setupUi(this);

  m_ComputeDebugSelector = new ComputeDebugSelector(this);

  const QIcon &action = Icons::action();
  const QIcon &action_hover = Icons::action_hover();

  RDLabel *objectLabels[] = {
      ui->vsShader, ui->hsShader, ui->dsShader,   ui->gsShader,
      ui->psShader, ui->csShader, ui->iaBytecode,
  };

  QToolButton *viewButtons[] = {
      ui->vsShaderViewButton,   ui->hsShaderViewButton, ui->dsShaderViewButton,
      ui->gsShaderViewButton,   ui->psShaderViewButton, ui->csShaderViewButton,
      ui->iaBytecodeViewButton,
  };

  QToolButton *editButtons[] = {
      ui->vsShaderEditButton, ui->hsShaderEditButton, ui->dsShaderEditButton,
      ui->gsShaderEditButton, ui->psShaderEditButton, ui->csShaderEditButton,
  };

  QToolButton *saveButtons[] = {
      ui->vsShaderSaveButton, ui->hsShaderSaveButton, ui->dsShaderSaveButton,
      ui->gsShaderSaveButton, ui->psShaderSaveButton, ui->csShaderSaveButton,
  };

  RDTreeWidget *resources[] = {
      ui->vsResources, ui->hsResources, ui->dsResources,
      ui->gsResources, ui->psResources, ui->csResources,
  };

  RDTreeWidget *samplers[] = {
      ui->vsSamplers, ui->hsSamplers, ui->dsSamplers,
      ui->gsSamplers, ui->psSamplers, ui->csSamplers,
  };

  RDTreeWidget *cbuffers[] = {
      ui->vsCBuffers, ui->hsCBuffers, ui->dsCBuffers,
      ui->gsCBuffers, ui->psCBuffers, ui->csCBuffers,
  };

  RDTreeWidget *classes[] = {
      ui->vsClasses, ui->hsClasses, ui->dsClasses, ui->gsClasses, ui->psClasses, ui->csClasses,
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
    QObject::connect(b, &QToolButton::clicked, this, &D3D11PipelineStateViewer::shaderView_clicked);

  for(RDLabel *b : objectLabels)
  {
    b->setAutoFillBackground(true);
    b->setBackgroundRole(QPalette::ToolTipBase);
    b->setForegroundRole(QPalette::ToolTipText);
    b->setMinimumSizeHint(QSize(250, 0));
  }

  QObject::connect(m_ComputeDebugSelector, &ComputeDebugSelector::beginDebug, this,
                   &D3D11PipelineStateViewer::computeDebugSelector_beginDebug);

  for(QToolButton *b : editButtons)
    QObject::connect(b, &QToolButton::clicked, &m_Common, &PipelineStateViewer::shaderEdit_clicked);

  for(QToolButton *b : saveButtons)
    QObject::connect(b, &QToolButton::clicked, this, &D3D11PipelineStateViewer::shaderSave_clicked);

  QObject::connect(ui->iaLayouts, &RDTreeWidget::leave, this,
                   &D3D11PipelineStateViewer::vertex_leave);
  QObject::connect(ui->iaBuffers, &RDTreeWidget::leave, this,
                   &D3D11PipelineStateViewer::vertex_leave);

  QObject::connect(ui->targetOutputs, &RDTreeWidget::itemActivated, this,
                   &D3D11PipelineStateViewer::resource_itemActivated);
  QObject::connect(ui->csUAVs, &RDTreeWidget::itemActivated, this,
                   &D3D11PipelineStateViewer::resource_itemActivated);
  QObject::connect(ui->gsStreamOut, &RDTreeWidget::itemActivated, this,
                   &D3D11PipelineStateViewer::resource_itemActivated);

  for(RDTreeWidget *res : resources)
    QObject::connect(res, &RDTreeWidget::itemActivated, this,
                     &D3D11PipelineStateViewer::resource_itemActivated);

  for(RDTreeWidget *cbuffer : cbuffers)
    QObject::connect(cbuffer, &RDTreeWidget::itemActivated, this,
                     &D3D11PipelineStateViewer::cbuffer_itemActivated);

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
  addGridLines(ui->blendStateGridLayout, palette().color(QPalette::WindowText));
  addGridLines(ui->depthStateGridLayout, palette().color(QPalette::WindowText));

  for(RDLabel *st : {ui->depthState, ui->blendState, ui->rastState, ui->predicate})
  {
    st->setAutoFillBackground(true);
    st->setBackgroundRole(QPalette::ToolTipBase);
    st->setForegroundRole(QPalette::ToolTipText);
    st->setMinimumSizeHint(QSize(100, 0));
  }

  {
    RDHeaderView *header = new RDHeaderView(Qt::Horizontal, this);
    ui->iaLayouts->setHeader(header);

    ui->iaLayouts->setColumns({tr("Slot"), tr("Semantic"), tr("Index"), tr("Format"), tr("Input Slot"),
                               tr("Offset"), tr("Class"), tr("Step Rate"), tr("Go")});
    header->setColumnStretchHints({1, 4, 2, 3, 2, 2, 1, 1, -1});

    ui->iaLayouts->setClearSelectionOnFocusLoss(true);
    ui->iaLayouts->setInstantTooltips(true);
    ui->iaLayouts->setHoverIconColumn(8, action, action_hover);
  }

  {
    RDHeaderView *header = new RDHeaderView(Qt::Horizontal, this);
    ui->iaBuffers->setHeader(header);

    ui->iaBuffers->setColumns(
        {tr("Slot"), tr("Buffer"), tr("Stride"), tr("Offset"), tr("Byte Length"), tr("Go")});
    header->setColumnStretchHints({1, 4, 2, 2, 3, -1});

    ui->iaBuffers->setClearSelectionOnFocusLoss(true);
    ui->iaBuffers->setInstantTooltips(true);
    ui->iaBuffers->setHoverIconColumn(5, action, action_hover);

    m_Common.SetupResourceView(ui->iaBuffers);
  }

  for(RDTreeWidget *tex : resources)
  {
    RDHeaderView *header = new RDHeaderView(Qt::Horizontal, this);
    tex->setHeader(header);

    tex->setColumns({tr("Slot"), tr("Resource"), tr("Type"), tr("Width"), tr("Height"), tr("Depth"),
                     tr("Array Size"), tr("Format"), tr("Go")});
    header->setColumnStretchHints({2, 4, 2, 1, 1, 1, 1, 3, -1});

    tex->setHoverIconColumn(8, action, action_hover);
    tex->setClearSelectionOnFocusLoss(true);
    tex->setInstantTooltips(true);

    m_Common.SetupResourceView(tex);
  }

  for(RDTreeWidget *samp : samplers)
  {
    RDHeaderView *header = new RDHeaderView(Qt::Horizontal, this);
    samp->setHeader(header);

    samp->setColumns({tr("Slot"), tr("Sampler"), tr("Addressing"), tr("Filter"), tr("LOD Clamp"),
                      tr("LOD Bias")});
    header->setColumnStretchHints({2, 1, 4, 4, 4, 4});

    samp->setClearSelectionOnFocusLoss(true);
    samp->setInstantTooltips(true);

    m_Common.SetupResourceView(samp);
  }

  for(RDTreeWidget *cbuffer : cbuffers)
  {
    RDHeaderView *header = new RDHeaderView(Qt::Horizontal, this);
    cbuffer->setHeader(header);

    cbuffer->setColumns({tr("Slot"), tr("Buffer"), tr("Byte Range"), tr("Size"), tr("Go")});
    header->setColumnStretchHints({1, 2, 3, 3, -1});

    cbuffer->setHoverIconColumn(4, action, action_hover);
    cbuffer->setClearSelectionOnFocusLoss(true);
    cbuffer->setInstantTooltips(true);

    m_Common.SetupResourceView(cbuffer);
  }

  for(RDTreeWidget *cl : classes)
  {
    RDHeaderView *header = new RDHeaderView(Qt::Horizontal, this);
    cl->setHeader(header);

    cl->setColumns({tr("Slot"), tr("Interface"), tr("Instance")});
    header->setColumnStretchHints({1, 1, 1});

    cl->setClearSelectionOnFocusLoss(true);
    cl->setInstantTooltips(true);

    m_Common.SetupResourceView(cl);
  }

  {
    RDHeaderView *header = new RDHeaderView(Qt::Horizontal, this);
    ui->gsStreamOut->setHeader(header);

    ui->gsStreamOut->setColumns(
        {tr("Slot"), tr("Buffer"), tr("Byte Length"), tr("Byte Offset"), tr("Go")});
    header->setColumnStretchHints({1, 4, 3, 2, -1});
    header->setMinimumSectionSize(40);

    ui->gsStreamOut->setHoverIconColumn(4, action, action_hover);
    ui->gsStreamOut->setClearSelectionOnFocusLoss(true);
    ui->gsStreamOut->setInstantTooltips(true);

    m_Common.SetupResourceView(ui->gsStreamOut);
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
    ui->targetOutputs->setHeader(header);

    ui->targetOutputs->setColumns({tr("Slot"), tr("Resource"), tr("Type"), tr("Width"), tr("Height"),
                                   tr("Depth"), tr("Array Size"), tr("Format"), tr("Go")});
    header->setColumnStretchHints({2, 4, 2, 1, 1, 1, 1, 3, -1});

    ui->targetOutputs->setHoverIconColumn(8, action, action_hover);
    ui->targetOutputs->setClearSelectionOnFocusLoss(true);
    ui->targetOutputs->setInstantTooltips(true);

    m_Common.SetupResourceView(ui->targetOutputs);
  }

  {
    RDHeaderView *header = new RDHeaderView(Qt::Horizontal, this);
    ui->blends->setHeader(header);

    ui->blends->setColumns({tr("Slot"), tr("Enabled"), tr("Col Src"), tr("Col Dst"), tr("Col Op"),
                            tr("Alpha Src"), tr("Alpha Dst"), tr("Alpha Op"), tr("Logic Op"),
                            tr("Write Mask")});
    header->setColumnStretchHints({-1, 1, 2, 2, 2, 2, 2, 2, 2, 1});

    ui->blends->setClearSelectionOnFocusLoss(true);
    ui->blends->setInstantTooltips(true);
  }

  {
    RDHeaderView *header = new RDHeaderView(Qt::Horizontal, this);
    ui->stencils->setHeader(header);

    ui->stencils->setColumns(
        {tr("Face"), tr("Func"), tr("Fail Op"), tr("Depth Fail Op"), tr("Pass Op")});
    header->setColumnStretchHints({1, 2, 2, 2, 2});

    ui->stencils->setClearSelectionOnFocusLoss(true);
    ui->stencils->setInstantTooltips(true);
  }

  {
    RDHeaderView *header = new RDHeaderView(Qt::Horizontal, this);
    ui->csUAVs->setHeader(header);

    ui->csUAVs->setColumns({tr("Slot"), tr("Resource"), tr("Type"), tr("Width"), tr("Height"),
                            tr("Depth"), tr("Array Size"), tr("Format"), tr("Go")});
    header->setColumnStretchHints({2, 4, 2, 1, 1, 1, 1, 3, -1});

    ui->csUAVs->setHoverIconColumn(8, action, action_hover);
    ui->csUAVs->setClearSelectionOnFocusLoss(true);
    ui->csUAVs->setInstantTooltips(true);

    m_Common.SetupResourceView(ui->csUAVs);
  }

  // this is often changed just because we're changing some tab in the designer.
  ui->stagesTabs->setCurrentIndex(0);

  ui->stagesTabs->tabBar()->setVisible(false);

  ui->pipeFlow->setStages(
      {
          lit("IA"),
          lit("VS"),
          lit("HS"),
          lit("DS"),
          lit("GS"),
          lit("RS"),
          lit("PS"),
          lit("OM"),
          lit("CS"),
      },
      {
          tr("Input Assembler"),
          tr("Vertex Shader"),
          tr("Hull Shader"),
          tr("Domain Shader"),
          tr("Geometry Shader"),
          tr("Rasterizer"),
          tr("Pixel Shader"),
          tr("Output Merger"),
          tr("Compute Shader"),
      });

  ui->pipeFlow->setIsolatedStage(8);    // compute shader isolated

  ui->pipeFlow->setStagesEnabled({true, true, true, true, true, true, true, true, true});

  m_Common.setMeshViewPixmap(ui->meshView);

  ui->iaLayouts->setFont(Formatter::PreferredFont());
  ui->iaBuffers->setFont(Formatter::PreferredFont());

  ui->csUAVs->setFont(Formatter::PreferredFont());
  ui->gsStreamOut->setFont(Formatter::PreferredFont());

  ui->vsShader->setFont(Formatter::PreferredFont());
  ui->vsResources->setFont(Formatter::PreferredFont());
  ui->vsSamplers->setFont(Formatter::PreferredFont());
  ui->vsCBuffers->setFont(Formatter::PreferredFont());
  ui->vsClasses->setFont(Formatter::PreferredFont());
  ui->gsShader->setFont(Formatter::PreferredFont());
  ui->gsResources->setFont(Formatter::PreferredFont());
  ui->gsSamplers->setFont(Formatter::PreferredFont());
  ui->gsCBuffers->setFont(Formatter::PreferredFont());
  ui->gsClasses->setFont(Formatter::PreferredFont());
  ui->hsShader->setFont(Formatter::PreferredFont());
  ui->hsResources->setFont(Formatter::PreferredFont());
  ui->hsSamplers->setFont(Formatter::PreferredFont());
  ui->hsCBuffers->setFont(Formatter::PreferredFont());
  ui->hsClasses->setFont(Formatter::PreferredFont());
  ui->dsShader->setFont(Formatter::PreferredFont());
  ui->dsResources->setFont(Formatter::PreferredFont());
  ui->dsSamplers->setFont(Formatter::PreferredFont());
  ui->dsCBuffers->setFont(Formatter::PreferredFont());
  ui->dsClasses->setFont(Formatter::PreferredFont());
  ui->psShader->setFont(Formatter::PreferredFont());
  ui->psResources->setFont(Formatter::PreferredFont());
  ui->psSamplers->setFont(Formatter::PreferredFont());
  ui->psCBuffers->setFont(Formatter::PreferredFont());
  ui->psClasses->setFont(Formatter::PreferredFont());
  ui->csShader->setFont(Formatter::PreferredFont());
  ui->csResources->setFont(Formatter::PreferredFont());
  ui->csSamplers->setFont(Formatter::PreferredFont());
  ui->csCBuffers->setFont(Formatter::PreferredFont());
  ui->csClasses->setFont(Formatter::PreferredFont());

  ui->viewports->setFont(Formatter::PreferredFont());
  ui->scissors->setFont(Formatter::PreferredFont());

  ui->targetOutputs->setFont(Formatter::PreferredFont());
  ui->blends->setFont(Formatter::PreferredFont());

  // reset everything back to defaults
  clearState();
}

D3D11PipelineStateViewer::~D3D11PipelineStateViewer()
{
  delete ui;
  delete m_ComputeDebugSelector;
}

void D3D11PipelineStateViewer::OnCaptureLoaded()
{
  OnEventChanged(m_Ctx.CurEvent());
}

void D3D11PipelineStateViewer::OnCaptureClosed()
{
  ui->pipeFlow->setStagesEnabled({true, true, true, true, true, true, true, true, true});

  clearState();
}

void D3D11PipelineStateViewer::OnEventChanged(uint32_t eventId)
{
  m_Ctx.Replay().AsyncInvoke([this](IReplayController *r) {
    const D3D11Pipe::State *state = r->GetD3D11PipelineState();
    ResourceId descriptorStore = state->descriptorStore;
    DescriptorRange range;
    range.offset = 0;
    range.descriptorSize = state->descriptorByteSize;
    range.count = state->descriptorCount;

    rdcarray<DescriptorRange> ranges = {range};

    rdcarray<DescriptorLogicalLocation> locations =
        r->GetDescriptorLocations(descriptorStore, ranges);
    rdcarray<Descriptor> descriptors = r->GetDescriptors(descriptorStore, ranges);
    rdcarray<SamplerDescriptor> samplerDescriptors =
        r->GetSamplerDescriptors(descriptorStore, ranges);

    // we only write to m_Locations etc on the GUI thread so we know there's no race here.
    GUIInvoke::call(this,
                    [this, locations = std::move(locations), descriptors = std::move(descriptors),
                     samplerDescriptors = std::move(samplerDescriptors)]() {
                      m_Locations = locations;
                      m_Descriptors = descriptors;
                      m_SamplerDescriptors = samplerDescriptors;
                      setState();
                    });
  });
}

void D3D11PipelineStateViewer::SelectPipelineStage(PipelineStage stage)
{
  if(stage == PipelineStage::SampleMask)
    ui->pipeFlow->setSelectedStage((int)PipelineStage::ColorDepthOutput);
  else
    ui->pipeFlow->setSelectedStage((int)stage);
}

ResourceId D3D11PipelineStateViewer::GetResource(RDTreeWidgetItem *item)
{
  QVariant tag = item->tag();

  const rdcarray<RDTreeWidget *> cbuffers = {
      ui->vsCBuffers, ui->hsCBuffers, ui->dsCBuffers,
      ui->gsCBuffers, ui->psCBuffers, ui->csCBuffers,
  };

  if(tag.canConvert<ResourceId>())
  {
    return tag.value<ResourceId>();
  }
  else if(tag.canConvert<D3D11ViewTag>())
  {
    D3D11ViewTag viewTag = tag.value<D3D11ViewTag>();
    return viewTag.desc.resource;
  }
  else if(tag.canConvert<D3D11VBIBTag>())
  {
    D3D11VBIBTag buf = tag.value<D3D11VBIBTag>();
    return buf.id;
  }
  else if(cbuffers.contains(item->treeWidget()))
  {
    const D3D11Pipe::Shader *stage = stageForSender(item->treeWidget());

    if(stage == NULL)
      return ResourceId();

    return FindDescriptor(stage->stage, DescriptorCategory::ConstantBlock, tag.value<uint32_t>()).resource;
  }

  return ResourceId();
}

void D3D11PipelineStateViewer::on_showUnused_toggled(bool checked)
{
  setState();
}

void D3D11PipelineStateViewer::on_showEmpty_toggled(bool checked)
{
  setState();
}

void D3D11PipelineStateViewer::setInactiveRow(RDTreeWidgetItem *node)
{
  node->setItalic(true);
}

void D3D11PipelineStateViewer::setEmptyRow(RDTreeWidgetItem *node)
{
  node->setBackgroundColor(QColor(255, 70, 70));
  node->setForegroundColor(QColor(0, 0, 0));
}

bool D3D11PipelineStateViewer::HasImportantViewParams(const Descriptor &view, TextureDescription *tex)
{
  // we don't count 'upgrade typeless to typed' as important, we just display the typed format
  // in the row since there's no real hidden important information there. The formats can't be
  // different for any other reason (if the SRV format differs from the texture format, the
  // texture must have been typeless.
  if(view.firstMip > 0 || view.firstSlice > 0 || (view.numMips < tex->mips && tex->mips > 1) ||
     (view.numSlices < tex->arraysize && tex->arraysize > 1))
    return true;

  // in the case of the swapchain case, types can be different and it won't have shown
  // up as taking the view's format because the swapchain already has one. Make sure to mark it
  // as important
  if(view.format.compType != CompType::Typeless && view.format != tex->format)
    return true;

  return false;
}

bool D3D11PipelineStateViewer::HasImportantViewParams(const Descriptor &view, BufferDescription *buf)
{
  if(view.byteOffset > 0 || view.byteSize < buf->length)
    return true;

  return false;
}

void D3D11PipelineStateViewer::setViewDetails(RDTreeWidgetItem *node, const D3D11ViewTag &view,
                                              TextureDescription *tex)
{
  if(tex == NULL)
    return;

  QString text;

  const Descriptor &desc = view.desc;

  bool viewdetails = false;

  if(desc.format != tex->format)
  {
    text += tr("The texture is format %1, the view treats it as %2.\n")
                .arg(tex->format.Name())
                .arg(desc.format.Name());

    viewdetails = true;
  }

  if(view.type == D3D11ViewTag::OMDepth)
  {
    if(m_Ctx.CurD3D11PipelineState()->outputMerger.depthReadOnly)
      text += tr("Depth component is read-only\n");
    if(m_Ctx.CurD3D11PipelineState()->outputMerger.stencilReadOnly)
      text += tr("Stencil component is read-only\n");
  }

  if(tex->mips > 1 && (tex->mips != desc.numMips || desc.firstMip > 0))
  {
    if(desc.numMips == 1)
      text +=
          tr("The texture has %1 mips, the view covers mip %2.\n").arg(tex->mips).arg(desc.firstMip);
    else
      text += tr("The texture has %1 mips, the view covers mips %2-%3.\n")
                  .arg(tex->mips)
                  .arg(desc.firstMip)
                  .arg(desc.firstMip + desc.numMips - 1);

    viewdetails = true;
  }

  if(tex->arraysize > 1 && (tex->arraysize != desc.numSlices || desc.firstSlice > 0))
  {
    if(desc.numSlices == 1)
      text += tr("The texture has %1 array slices, the view covers slice %2.\n")
                  .arg(tex->arraysize)
                  .arg(desc.firstSlice);
    else
      text += tr("The texture has %1 array slices, the view covers slices %2-%3.\n")
                  .arg(tex->arraysize)
                  .arg(desc.firstSlice)
                  .arg(desc.firstSlice + desc.numSlices - 1);

    viewdetails = true;
  }

  text = text.trimmed();

  node->setToolTip(text);

  if(viewdetails)
    node->setBackgroundColor(m_Common.GetViewDetailsColor());
}

void D3D11PipelineStateViewer::setViewDetails(RDTreeWidgetItem *node, const D3D11ViewTag &view,
                                              BufferDescription *buf)
{
  if(buf == NULL)
    return;

  QString text;

  const Descriptor &desc = view.desc;

  if(desc.byteOffset > 0 || desc.byteSize < buf->length)
  {
    text += tr("The view covers bytes %1-%2 (%3 elements).\nThe buffer is %4 bytes in length (%5 "
               "elements).\n")
                .arg(desc.byteOffset)
                .arg(desc.byteOffset + desc.byteSize)
                .arg(desc.byteSize / desc.elementByteSize)
                .arg(buf->length)
                .arg(buf->length / desc.elementByteSize);
  }
  else
  {
    return;
  }

  node->setToolTip(text);
  node->setBackgroundColor(m_Common.GetViewDetailsColor());
}

void D3D11PipelineStateViewer::addResourceRow(const D3D11ViewTag &view,
                                              const ShaderResource *shaderInput, bool usedSlot,
                                              RDTreeWidget *resources)
{
  const Descriptor &desc = view.desc;

  bool viewDetails = false;

  if(view.type == D3D11ViewTag::OMDepth)
    viewDetails = m_Ctx.CurD3D11PipelineState()->outputMerger.depthReadOnly ||
                  m_Ctx.CurD3D11PipelineState()->outputMerger.stencilReadOnly;

  bool filledSlot = (desc.resource != ResourceId());

  // if a target is set to RTVs or DSV, it is implicitly used
  if(filledSlot)
    usedSlot = usedSlot || view.type == D3D11ViewTag::OMTarget || view.type == D3D11ViewTag::OMDepth;

  if(showNode(usedSlot, filledSlot))
  {
    QString slotname = view.type == D3D11ViewTag::OMDepth ? tr("Depth") : QString::number(view.index);

    if(shaderInput && !shaderInput->name.empty())
      slotname += lit(": ") + shaderInput->name;

    uint32_t w = 1, h = 1, d = 1;
    uint32_t a = 1;
    QString format = tr("Unknown");
    QString typeName = tr("Unknown");

    if(!filledSlot)
    {
      format = lit("-");
      typeName = lit("-");
      w = h = d = a = 0;
    }

    TextureDescription *tex = m_Ctx.GetTexture(desc.resource);

    if(tex)
    {
      w = tex->width;
      h = tex->height;
      d = tex->depth;
      a = tex->arraysize;
      format = tex->format.Name();
      typeName = ToQStr(tex->type);

      if(tex->type == TextureType::Texture2DMS || tex->type == TextureType::Texture2DMSArray)
      {
        typeName += QFormatStr(" %1x").arg(tex->msSamp);
      }

      if(tex->format != desc.format)
        format = tr("Viewed as %1").arg(desc.format.Name());

      if(HasImportantViewParams(desc, tex))
        viewDetails = true;
    }

    BufferDescription *buf = m_Ctx.GetBuffer(desc.resource);

    if(buf)
    {
      w = buf->length;
      h = 0;
      d = 0;
      a = 0;
      format = QString();
      typeName = QFormatStr("%1Buffer").arg(view.type == D3D11ViewTag::UAV ? lit("RW") : QString());

      if(desc.flags & DescriptorFlags::RawBuffer)
      {
        typeName = QFormatStr("%1ByteAddressBuffer")
                       .arg(view.type == D3D11ViewTag::UAV ? lit("RW") : QString());
      }
      else if(desc.elementByteSize > 0 && desc.format.type == ResourceFormatType::Undefined)
      {
        // for structured buffers, display how many 'elements' there are in the buffer
        typeName = QFormatStr("%1StructuredBuffer[%2]")
                       .arg(view.type == D3D11ViewTag::UAV ? lit("RW") : QString())
                       .arg(buf->length / desc.elementByteSize);
      }

      if(desc.secondary != ResourceId())
      {
        typeName += tr(" (%1: %2)").arg(ToQStr(desc.secondary)).arg(desc.bufferStructCount);
      }

      // get the buffer type, whether it's just a basic type or a complex struct
      if(shaderInput && !shaderInput->isTexture)
      {
        if(desc.format.compType == CompType::Typeless)
        {
          if(shaderInput->variableType.baseType == VarType::Struct)
            format = lit("struct ") + shaderInput->variableType.name;
          else
            format = shaderInput->variableType.name;
        }
        else
        {
          format = desc.format.Name();
        }
      }

      if(HasImportantViewParams(desc, buf))
        viewDetails = true;
    }

    QVariant name = desc.resource;

    if(viewDetails)
      name = tr("%1 viewed by %2").arg(ToQStr(desc.resource)).arg(ToQStr(desc.view));

    RDTreeWidgetItem *node =
        new RDTreeWidgetItem({slotname, name, typeName, w, h, d, a, format, QString()});

    node->setTag(QVariant::fromValue(view));

    if(viewDetails)
    {
      if(tex)
        setViewDetails(node, view, tex);
      else if(buf)
        setViewDetails(node, view, buf);
    }

    if(!filledSlot)
      setEmptyRow(node);

    if(!usedSlot)
      setInactiveRow(node);

    resources->addTopLevelItem(node);
  }
}

void D3D11PipelineStateViewer::addSamplerRow(const SamplerDescriptor &descriptor, uint32_t reg,
                                             const ShaderSampler *shaderBind, bool usedSlot,
                                             RDTreeWidget *samplers)
{
  bool filledSlot = descriptor.object != ResourceId();

  if(showNode(usedSlot, filledSlot))
  {
    QString slotname = QString::number(reg);

    if(shaderBind && !shaderBind->name.empty())
      slotname += lit(": ") + shaderBind->name;

    QString borderColor = QFormatStr("%1, %2, %3, %4")
                              .arg(descriptor.borderColorValue.floatValue[0])
                              .arg(descriptor.borderColorValue.floatValue[1])
                              .arg(descriptor.borderColorValue.floatValue[2])
                              .arg(descriptor.borderColorValue.floatValue[3]);

    QString addressing;

    QString addPrefix;
    QString addVal;

    QString addr[] = {ToQStr(descriptor.addressU, GraphicsAPI::D3D11),
                      ToQStr(descriptor.addressV, GraphicsAPI::D3D11),
                      ToQStr(descriptor.addressW, GraphicsAPI::D3D11)};

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

    QString filter = ToQStr(descriptor.filter);

    if(descriptor.maxAnisotropy > 1)
      filter += QFormatStr(" %1x").arg(descriptor.maxAnisotropy);

    if(descriptor.filter.filter == FilterFunction::Comparison)
      filter += QFormatStr(" (%1)").arg(ToQStr(descriptor.compareFunction));
    else if(descriptor.filter.filter != FilterFunction::Normal)
      filter += QFormatStr(" (%1)").arg(ToQStr(descriptor.filter.filter));

    RDTreeWidgetItem *node = new RDTreeWidgetItem(
        {slotname, descriptor.object, addressing, filter,
         QFormatStr("%1 - %2")
             .arg(descriptor.minLOD == -FLT_MAX ? lit("0") : QString::number(descriptor.minLOD))
             .arg(descriptor.maxLOD == FLT_MAX ? lit("FLT_MAX") : QString::number(descriptor.maxLOD)),
         descriptor.mipBias});

    if(!filledSlot)
      setEmptyRow(node);

    if(!usedSlot)
      setInactiveRow(node);

    samplers->addTopLevelItem(node);
  }
}

void D3D11PipelineStateViewer::addCBufferRow(const Descriptor &descriptor, uint32_t reg,
                                             const ConstantBlock *shaderBind, bool usedSlot,
                                             RDTreeWidget *cbuffers)
{
  bool filledSlot = descriptor.resource != ResourceId();
  if(showNode(usedSlot, filledSlot))
  {
    ulong length = 0;
    int numvars = shaderBind ? shaderBind->variables.count() : 0;
    uint32_t bytesize = shaderBind ? shaderBind->byteSize : 0;

    BufferDescription *buf = m_Ctx.GetBuffer(descriptor.resource);

    if(buf)
      length = buf->length;

    QString slotname = QString::number(reg);

    if(shaderBind && !shaderBind->name.empty())
      slotname += lit(": ") + shaderBind->name;

    QString sizestr;
    if(bytesize == (uint32_t)length)
      sizestr = tr("%1 Variables, %2 bytes")
                    .arg(numvars)
                    .arg(Formatter::HumanFormat(length, Formatter::OffsetSize));
    else
      sizestr = tr("%1 Variables, %2 bytes needed, %3 provided")
                    .arg(numvars)
                    .arg(Formatter::HumanFormat(bytesize, Formatter::OffsetSize))
                    .arg(Formatter::HumanFormat(length, Formatter::OffsetSize));

    if(length < bytesize)
      filledSlot = false;

    QString vecrange = QFormatStr("%1 - %2")
                           .arg(Formatter::HumanFormat(descriptor.byteOffset, Formatter::OffsetSize))
                           .arg(Formatter::HumanFormat(descriptor.byteOffset + descriptor.byteSize,
                                                       Formatter::OffsetSize));

    RDTreeWidgetItem *node =
        new RDTreeWidgetItem({slotname, descriptor.resource, vecrange, sizestr, QString()});

    node->setTag(QVariant::fromValue(reg));

    if(!filledSlot)
      setEmptyRow(node);

    if(!usedSlot)
      setInactiveRow(node);

    cbuffers->addTopLevelItem(node);
  }
}

bool D3D11PipelineStateViewer::showNode(bool usedSlot, bool filledSlot)
{
  const bool showUnused = ui->showUnused->isChecked();
  const bool showEmpty = ui->showEmpty->isChecked();

  // show if it's referenced by the shader - regardless of empty or not
  if(usedSlot)
    return true;

  // it's not referenced, but if it's bound and we have "show unused" then show it
  if(showUnused && filledSlot)
    return true;

  // it's empty, and we have "show empty"
  if(showEmpty && !filledSlot)
    return true;

  return false;
}

const Descriptor &D3D11PipelineStateViewer::FindDescriptor(ShaderStage stage,
                                                           DescriptorCategory category, uint32_t reg)
{
  const ShaderStageMask mask = MaskForStage(stage);
  // locations for D3D11 descriptors should have an accurate category for us to look up
  for(size_t i = 0; i < m_Locations.size(); i++)
  {
    if((m_Locations[i].stageMask & mask) && m_Locations[i].category == category &&
       m_Locations[i].fixedBindNumber == reg)
    {
      return m_Descriptors[i];
    }
  }

  static Descriptor empty;
  return empty;
}

bool D3D11PipelineStateViewer::HasAccess(ShaderStage stage, DescriptorCategory category,
                                         uint32_t index)
{
  for(const DescriptorAccess &access : m_Ctx.CurPipelineState().GetDescriptorAccess())
  {
    if(access.stage == stage && CategoryForDescriptorType(access.type) == category &&
       access.index == index)
    {
      return true;
    }
  }

  return false;
}

const D3D11Pipe::Shader *D3D11PipelineStateViewer::stageForSender(QWidget *widget)
{
  if(!m_Ctx.IsCaptureLoaded())
    return NULL;

  while(widget)
  {
    if(widget == ui->stagesTabs->widget(0))
      return &m_Ctx.CurD3D11PipelineState()->vertexShader;
    if(widget == ui->stagesTabs->widget(1))
      return &m_Ctx.CurD3D11PipelineState()->vertexShader;
    if(widget == ui->stagesTabs->widget(2))
      return &m_Ctx.CurD3D11PipelineState()->hullShader;
    if(widget == ui->stagesTabs->widget(3))
      return &m_Ctx.CurD3D11PipelineState()->domainShader;
    if(widget == ui->stagesTabs->widget(4))
      return &m_Ctx.CurD3D11PipelineState()->geometryShader;
    if(widget == ui->stagesTabs->widget(5))
      return &m_Ctx.CurD3D11PipelineState()->pixelShader;
    if(widget == ui->stagesTabs->widget(6))
      return &m_Ctx.CurD3D11PipelineState()->pixelShader;
    if(widget == ui->stagesTabs->widget(7))
      return &m_Ctx.CurD3D11PipelineState()->pixelShader;
    if(widget == ui->stagesTabs->widget(8))
      return &m_Ctx.CurD3D11PipelineState()->computeShader;

    widget = widget->parentWidget();
  }

  qCritical() << "Unrecognised control calling event handler";

  return NULL;
}

void D3D11PipelineStateViewer::clearShaderState(RDLabel *shader, RDTreeWidget *tex,
                                                RDTreeWidget *samp, RDTreeWidget *cbuffer,
                                                RDTreeWidget *sub)
{
  shader->setText(ToQStr(ResourceId()));
  tex->clear();
  samp->clear();
  sub->clear();
  cbuffer->clear();
}

void D3D11PipelineStateViewer::clearState()
{
  m_VBNodes.clear();
  m_EmptyNodes.clear();

  ui->iaLayouts->clear();
  ui->iaBuffers->clear();
  ui->iaBytecodeMismatch->setVisible(false);
  ui->topology->setText(QString());
  ui->topologyDiagram->setPixmap(QPixmap());

  clearShaderState(ui->vsShader, ui->vsResources, ui->vsSamplers, ui->vsCBuffers, ui->vsClasses);
  clearShaderState(ui->gsShader, ui->gsResources, ui->gsSamplers, ui->gsCBuffers, ui->gsClasses);
  clearShaderState(ui->hsShader, ui->hsResources, ui->hsSamplers, ui->hsCBuffers, ui->hsClasses);
  clearShaderState(ui->dsShader, ui->dsResources, ui->dsSamplers, ui->dsCBuffers, ui->dsClasses);
  clearShaderState(ui->psShader, ui->psResources, ui->psSamplers, ui->psCBuffers, ui->psClasses);
  clearShaderState(ui->csShader, ui->csResources, ui->csSamplers, ui->csCBuffers, ui->csClasses);

  QToolButton *shaderButtons[] = {
      ui->vsShaderViewButton,   ui->hsShaderViewButton, ui->dsShaderViewButton,
      ui->gsShaderViewButton,   ui->psShaderViewButton, ui->csShaderViewButton,
      ui->vsShaderEditButton,   ui->hsShaderEditButton, ui->dsShaderEditButton,
      ui->gsShaderEditButton,   ui->psShaderEditButton, ui->csShaderEditButton,
      ui->vsShaderSaveButton,   ui->hsShaderSaveButton, ui->dsShaderSaveButton,
      ui->gsShaderSaveButton,   ui->psShaderSaveButton, ui->csShaderSaveButton,
      ui->iaBytecodeViewButton,
  };

  ui->gsStreamOut->clear();

  for(QToolButton *b : shaderButtons)
    b->setEnabled(false);

  ui->csUAVs->clear();

  const QPixmap &tick = Pixmaps::tick(this);
  const QPixmap &cross = Pixmaps::cross(this);

  ui->fillMode->setText(tr("Solid", "Fill Mode"));
  ui->cullMode->setText(tr("Front", "Cull Mode"));
  ui->frontCCW->setPixmap(tick);
  ui->conservativeRaster->setPixmap(cross);

  ui->depthBias->setText(lit("0.0"));
  ui->depthBiasClamp->setText(lit("0.0"));
  ui->slopeScaledBias->setText(lit("0.0"));
  ui->forcedSampleCount->setText(lit("0"));

  ui->depthClip->setPixmap(tick);
  ui->scissorEnabled->setPixmap(tick);
  ui->multisample->setPixmap(tick);
  ui->lineAA->setPixmap(tick);

  ui->independentBlend->setPixmap(cross);
  ui->alphaToCoverage->setPixmap(tick);

  ui->blendFactor->setText(lit("0.00, 0.00, 0.00, 0.00"));
  ui->sampleMask->setText(lit("FFFFFFFF"));

  ui->viewports->clear();
  ui->scissors->clear();

  ui->targetOutputs->clear();
  ui->blends->clear();

  ui->depthEnabled->setPixmap(tick);
  ui->depthFunc->setText(lit("GREATER_EQUAL"));
  ui->depthWrite->setPixmap(tick);

  ui->stencilEnabled->setPixmap(cross);
  ui->stencilReadMask->setText(lit("FF"));
  ui->stencilWriteMask->setText(lit("FF"));
  ui->stencilRef->setText(lit("FF"));

  ui->stencils->clear();

  ui->predicateGroup->setVisible(false);

  ui->computeDebugSelector->setEnabled(false);
}

void D3D11PipelineStateViewer::setShaderState(const D3D11Pipe::Shader &stage, RDLabel *shader,
                                              RDTreeWidget *resources, RDTreeWidget *samplers,
                                              RDTreeWidget *cbuffers, RDTreeWidget *classes)
{
  ShaderReflection *shaderDetails = stage.reflection;

  QString shText = ToQStr(stage.resourceId);

  if(shaderDetails && !shaderDetails->debugInfo.files.empty())
  {
    const ShaderDebugInfo &dbg = shaderDetails->debugInfo;
    int entryFile = qMax(0, dbg.entryLocation.fileIndex);

    shText += QFormatStr(": %1() - %2")
                  .arg(shaderDetails->entryPoint)
                  .arg(QFileInfo(dbg.files[entryFile].filename).fileName());
  }

  shader->setText(shText);

  for(int i = 0; i < stage.classInstances.count(); i++)
  {
    QString interfaceName = lit("Interface %1").arg(i);

    if(shaderDetails && i < shaderDetails->interfaces.count())
      interfaceName = shaderDetails->interfaces[i];

    classes->addTopLevelItem(new RDTreeWidgetItem({i, interfaceName, stage.classInstances[i]}));
  }
}

void D3D11PipelineStateViewer::setState()
{
  if(!m_Ctx.IsCaptureLoaded())
  {
    clearState();
    return;
  }

  const D3D11Pipe::State &state = *m_Ctx.CurD3D11PipelineState();
  const ActionDescription *action = m_Ctx.CurAction();

  const QPixmap &tick = Pixmaps::tick(this);
  const QPixmap &cross = Pixmaps::cross(this);

  ////////////////////////////////////////////////
  // Vertex Input

  if(state.inputAssembly.bytecode)
  {
    QString layout = ToQStr(state.inputAssembly.resourceId);

    if(state.inputAssembly.bytecode && !state.inputAssembly.bytecode->debugInfo.files.empty())
    {
      const ShaderDebugInfo &dbg = state.inputAssembly.bytecode->debugInfo;
      int entryFile = qMax(0, dbg.entryLocation.fileIndex);

      layout += QFormatStr(": %1() - %2")
                    .arg(state.inputAssembly.bytecode->entryPoint)
                    .arg(QFileInfo(dbg.files[entryFile].filename).fileName());
    }

    ui->iaBytecode->setText(layout);
  }
  else
  {
    ui->iaBytecode->setText(ToQStr(state.inputAssembly.resourceId));
  }

  ui->iaBytecodeMismatch->setVisible(false);

  // check for IA-VS mismatches here.
  // This should be moved to a "Render Doctor" window reporting problems
  if(state.inputAssembly.bytecode && state.vertexShader.reflection)
  {
    QString mismatchDetails;

    // VS wants more elements
    if(state.inputAssembly.bytecode->inputSignature.count() <
       state.vertexShader.reflection->inputSignature.count())
    {
      int excess = state.vertexShader.reflection->inputSignature.count() -
                   state.inputAssembly.bytecode->inputSignature.count();

      bool allSystem = true;

      // The VS signature can consume more elements as long as they are all system value types
      // (ie. SV_VertexID or SV_InstanceID)
      for(int e = 0; e < excess; e++)
      {
        if(state.vertexShader.reflection
               ->inputSignature[state.vertexShader.reflection->inputSignature.count() - 1 - e]
               .systemValue == ShaderBuiltin::Undefined)
        {
          allSystem = false;
          break;
        }
      }

      if(!allSystem)
        mismatchDetails += tr("IA bytecode provides fewer elements than VS wants.\n");
    }

    {
      const rdcarray<SigParameter> &IA = state.inputAssembly.bytecode->inputSignature;
      const rdcarray<SigParameter> &VS = state.vertexShader.reflection->inputSignature;

      int count = qMin(IA.count(), VS.count());

      for(int i = 0; i < count; i++)
      {
        QString IAname = IA[i].semanticIdxName;
        QString VSname = VS[i].semanticIdxName;

        // misorder or misnamed semantics
        if(IAname.compare(VSname, Qt::CaseInsensitive))
          mismatchDetails += tr("IA bytecode semantic %1: %2 != VS bytecode semantic %1: %3\n")
                                 .arg(i)
                                 .arg(IAname)
                                 .arg(VSname);

        // VS wants more components
        if(IA[i].compCount < VS[i].compCount)
          mismatchDetails += tr("IA bytecode semantic %1 (%2) is %4-wide).arg(VS bytecode semantic "
                                "%1 (%2) %3 is %5-wide\n")
                                 .arg(i)
                                 .arg(IAname)
                                 .arg(VSname)
                                 .arg(IA[i].compCount)
                                 .arg(VS[i].compCount);

        // VS wants different types
        if(IA[i].varType != VS[i].varType)
          mismatchDetails +=
              tr("IA bytecode semantic %1 (%2) is %4).arg(VS bytecode semantic %1 (%3) is %5\n")
                  .arg(i)
                  .arg(IAname)
                  .arg(VSname)
                  .arg(ToQStr(IA[i].varType))
                  .arg(ToQStr(VS[i].varType));
      }
    }

    if(!mismatchDetails.isEmpty())
    {
      ui->iaBytecodeMismatch->setText(
          tr("Warning: Mismatch detected between bytecode used to create IA and currently bound VS "
             "inputs"));
      ui->iaBytecodeMismatch->setToolTip(mismatchDetails.trimmed());
      ui->iaBytecodeMismatch->setVisible(true);
    }
  }

  int vs = 0;

  bool usedVBuffers[128] = {};
  uint32_t layoutOffs[128] = {};

  vs = ui->iaLayouts->verticalScrollBar()->value();
  ui->iaLayouts->beginUpdate();
  ui->iaLayouts->clear();
  {
    int i = 0;
    for(const D3D11Pipe::Layout &l : state.inputAssembly.layouts)
    {
      QString byteOffs = Formatter::HumanFormat(l.byteOffset, Formatter::OffsetSize);

      // D3D11 specific value
      if(l.byteOffset == ~0U)
      {
        byteOffs = lit("APPEND_ALIGNED (%1)").arg(layoutOffs[l.inputSlot]);
      }
      else
      {
        layoutOffs[l.inputSlot] = l.byteOffset;
      }

      layoutOffs[l.inputSlot] += l.format.compByteWidth * l.format.compCount;

      bool filledSlot = true;
      bool usedSlot = false;

      for(int ia = 0;
          state.inputAssembly.bytecode && ia < state.inputAssembly.bytecode->inputSignature.count();
          ia++)
      {
        if(!QString(state.inputAssembly.bytecode->inputSignature[ia].semanticName)
                .compare(l.semanticName, Qt::CaseInsensitive) &&
           state.inputAssembly.bytecode->inputSignature[ia].semanticIndex == l.semanticIndex)
        {
          usedSlot = true;
          break;
        }
      }

      if(showNode(usedSlot, filledSlot))
      {
        RDTreeWidgetItem *node =
            new RDTreeWidgetItem({i, l.semanticName, l.semanticIndex, l.format.Name(), l.inputSlot,
                                  byteOffs, l.perInstance ? lit("PER_INSTANCE") : lit("PER_VERTEX"),
                                  l.instanceDataStepRate, QString()});

        node->setTag(i);

        if(usedSlot)
          usedVBuffers[l.inputSlot] = true;

        if(!usedSlot)
          setInactiveRow(node);

        ui->iaLayouts->addTopLevelItem(node);
      }

      i++;
    }
  }
  ui->iaLayouts->clearSelection();
  ui->iaLayouts->endUpdate();
  ui->iaLayouts->verticalScrollBar()->setValue(vs);

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

  bool ibufferUsed = action && (action->flags & ActionFlags::Indexed);

  m_VBNodes.clear();
  m_EmptyNodes.clear();

  vs = ui->iaBuffers->verticalScrollBar()->value();
  ui->iaBuffers->beginUpdate();
  ui->iaBuffers->clear();

  if(state.inputAssembly.indexBuffer.resourceId != ResourceId())
  {
    if(ibufferUsed || ui->showUnused->isChecked())
    {
      uint64_t length = 0;

      BufferDescription *buf = m_Ctx.GetBuffer(state.inputAssembly.indexBuffer.resourceId);

      if(buf)
        length = buf->length;

      RDTreeWidgetItem *node = new RDTreeWidgetItem({
          tr("Index"),
          state.inputAssembly.indexBuffer.resourceId,
          Formatter::HumanFormat(state.inputAssembly.indexBuffer.byteStride, Formatter::OffsetSize),
          Formatter::HumanFormat(state.inputAssembly.indexBuffer.byteOffset, Formatter::OffsetSize),
          Formatter::HumanFormat(length, Formatter::OffsetSize),
          QString(),
      });

      QString iformat;

      if(state.inputAssembly.indexBuffer.byteStride == 1)
        iformat = lit("ubyte");
      else if(state.inputAssembly.indexBuffer.byteStride == 2)
        iformat = lit("ushort");
      else if(state.inputAssembly.indexBuffer.byteStride == 4)
        iformat = lit("uint");

      iformat +=
          lit(" indices[%1]").arg(RENDERDOC_NumVerticesPerPrimitive(state.inputAssembly.topology));

      node->setTag(QVariant::fromValue(D3D11VBIBTag(
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

      ui->iaBuffers->addTopLevelItem(node);
    }
  }
  else
  {
    if(ibufferUsed || ui->showEmpty->isChecked())
    {
      RDTreeWidgetItem *node = new RDTreeWidgetItem(
          {tr("Index"), tr("No Buffer Set"), lit("-"), lit("-"), lit("-"), QString()});

      QString iformat;

      if(state.inputAssembly.indexBuffer.byteStride == 1)
        iformat = lit("ubyte");
      else if(state.inputAssembly.indexBuffer.byteStride == 2)
        iformat = lit("ushort");
      else if(state.inputAssembly.indexBuffer.byteStride == 4)
        iformat = lit("uint");

      iformat +=
          lit(" indices[%1]").arg(RENDERDOC_NumVerticesPerPrimitive(state.inputAssembly.topology));

      node->setTag(QVariant::fromValue(D3D11VBIBTag(
          state.inputAssembly.indexBuffer.resourceId,
          state.inputAssembly.indexBuffer.byteOffset +
              (action ? action->indexOffset * state.inputAssembly.indexBuffer.byteStride : 0),
          iformat)));

      setEmptyRow(node);
      m_EmptyNodes.push_back(node);

      if(!ibufferUsed)
        setInactiveRow(node);

      ui->iaBuffers->addTopLevelItem(node);
    }
  }

  for(int i = 0; i < state.inputAssembly.vertexBuffers.count(); i++)
  {
    const D3D11Pipe::VertexBuffer &v = state.inputAssembly.vertexBuffers[i];

    bool filledSlot = (v.resourceId != ResourceId());
    bool usedSlot = (usedVBuffers[i]);

    if(showNode(usedSlot, filledSlot))
    {
      qulonglong length = 0;

      BufferDescription *buf = m_Ctx.GetBuffer(v.resourceId);
      if(buf)
        length = buf->length;

      RDTreeWidgetItem *node = NULL;

      if(filledSlot)
        node = new RDTreeWidgetItem({
            i,
            v.resourceId,
            Formatter::HumanFormat(v.byteStride, Formatter::OffsetSize),
            Formatter::HumanFormat(v.byteOffset, Formatter::OffsetSize),
            Formatter::HumanFormat(length, Formatter::OffsetSize),
            QString(),
        });
      else
        node =
            new RDTreeWidgetItem({i, tr("No Buffer Set"), lit("-"), lit("-"), lit("-"), QString()});

      node->setTag(QVariant::fromValue(
          D3D11VBIBTag(v.resourceId, v.byteOffset, m_Common.GetVBufferFormatString(i))));

      if(!filledSlot)
      {
        setEmptyRow(node);
        m_EmptyNodes.push_back(node);
      }

      if(!usedSlot)
        setInactiveRow(node);

      m_VBNodes.push_back(node);

      ui->iaBuffers->addTopLevelItem(node);
    }
    else
    {
      m_VBNodes.push_back(NULL);
    }
  }
  ui->iaBuffers->clearSelection();
  ui->iaBuffers->endUpdate();
  ui->iaBuffers->verticalScrollBar()->setValue(vs);

  QToolButton *shaderButtons[] = {
      ui->vsShaderViewButton, ui->hsShaderViewButton, ui->dsShaderViewButton,
      ui->gsShaderViewButton, ui->psShaderViewButton, ui->csShaderViewButton,
      ui->vsShaderEditButton, ui->hsShaderEditButton, ui->dsShaderEditButton,
      ui->gsShaderEditButton, ui->psShaderEditButton, ui->csShaderEditButton,
      ui->vsShaderSaveButton, ui->hsShaderSaveButton, ui->dsShaderSaveButton,
      ui->gsShaderSaveButton, ui->psShaderSaveButton, ui->csShaderSaveButton,
  };

  for(QToolButton *b : shaderButtons)
  {
    const D3D11Pipe::Shader *stage = stageForSender(b);

    if(stage == NULL || stage->resourceId == ResourceId())
      continue;

    b->setEnabled(stage->reflection != NULL);

    m_Common.SetupShaderEditButton(b, ResourceId(), stage->resourceId, stage->reflection);
  }

  ui->iaBytecodeViewButton->setEnabled(true);

  ////////////////////////////////////////////////
  // Main iteration over descriptor storage

  bool targets[32] = {};

  {
    ScopedTreeUpdater restorers[] = {
        // VS
        ui->vsResources,
        ui->vsSamplers,
        ui->vsCBuffers,
        ui->vsClasses,
        // GS
        ui->gsResources,
        ui->gsSamplers,
        ui->gsCBuffers,
        ui->gsClasses,
        // HS
        ui->hsResources,
        ui->hsSamplers,
        ui->hsCBuffers,
        ui->hsClasses,
        // DS
        ui->dsResources,
        ui->dsSamplers,
        ui->dsCBuffers,
        ui->dsClasses,
        // PS
        ui->psResources,
        ui->psSamplers,
        ui->psCBuffers,
        ui->psClasses,
        // CS
        ui->csResources,
        ui->csSamplers,
        ui->csCBuffers,
        ui->csClasses,
        ui->csUAVs,
        // OM - we handle this here since it overlaps with the shader-based UAVs
        ui->targetOutputs,
    };

    rdcarray<Descriptor> outputs = m_Ctx.CurPipelineState().GetOutputTargets();
    for(uint32_t i = 0; i < outputs.size(); i++)
    {
      addResourceRow(D3D11ViewTag(D3D11ViewTag::OMTarget, i, outputs[i]), NULL, true,
                     ui->targetOutputs);

      if(outputs[i].resource != ResourceId())
        targets[i] = true;
    }

    setShaderState(state.vertexShader, ui->vsShader, ui->vsResources, ui->vsSamplers,
                   ui->vsCBuffers, ui->vsClasses);
    setShaderState(state.geometryShader, ui->gsShader, ui->gsResources, ui->gsSamplers,
                   ui->gsCBuffers, ui->gsClasses);
    setShaderState(state.hullShader, ui->hsShader, ui->hsResources, ui->hsSamplers, ui->hsCBuffers,
                   ui->hsClasses);
    setShaderState(state.domainShader, ui->dsShader, ui->dsResources, ui->dsSamplers,
                   ui->dsCBuffers, ui->dsClasses);
    setShaderState(state.pixelShader, ui->psShader, ui->psResources, ui->psSamplers, ui->psCBuffers,
                   ui->psClasses);
    setShaderState(state.computeShader, ui->csShader, ui->csResources, ui->csSamplers,
                   ui->csCBuffers, ui->csClasses);

    const ShaderReflection *shaderRefls[NumShaderStages];
    RDTreeWidget *resources[] = {
        ui->vsResources, ui->hsResources, ui->dsResources,
        ui->gsResources, ui->psResources, ui->csResources,
    };
    RDTreeWidget *samplers[] = {
        ui->vsSamplers, ui->hsSamplers, ui->dsSamplers,
        ui->gsSamplers, ui->psSamplers, ui->csSamplers,
    };
    RDTreeWidget *cbuffers[] = {
        ui->vsCBuffers, ui->hsCBuffers, ui->dsCBuffers,
        ui->gsCBuffers, ui->psCBuffers, ui->csCBuffers,
    };

    for(ShaderStage stage : values<ShaderStage>())
      shaderRefls[(uint32_t)stage] = m_Ctx.CurPipelineState().GetShaderReflection(stage);

    for(uint32_t i = 0; i < m_Locations.size(); i++)
    {
      // expect only one stage per location
      ShaderStage stage = FirstStageForMask(m_Locations[i].stageMask);
      uint32_t reg = m_Locations[i].fixedBindNumber;

      bool usedSlot = false;

      if(m_Locations[i].category == DescriptorCategory::ConstantBlock)
      {
        const ConstantBlock *shaderBind = NULL;

        if(shaderRefls[(uint32_t)stage])
        {
          for(int b = 0; b < shaderRefls[(uint32_t)stage]->constantBlocks.count(); b++)
          {
            const ConstantBlock &res = shaderRefls[(uint32_t)stage]->constantBlocks[b];

            if(res.fixedBindNumber == reg)
            {
              shaderBind = &res;
              usedSlot = HasAccess(stage, m_Locations[i].category, b);
              break;
            }
          }
        }

        Descriptor b = m_Descriptors[i];

        addCBufferRow(b, reg, shaderBind, usedSlot, cbuffers[(uint32_t)stage]);
      }
      else if(m_Locations[i].category == DescriptorCategory::Sampler)
      {
        const ShaderSampler *shaderBind = NULL;

        if(shaderRefls[(uint32_t)stage])
        {
          for(int b = 0; b < shaderRefls[(uint32_t)stage]->samplers.count(); b++)
          {
            const ShaderSampler &res = shaderRefls[(uint32_t)stage]->samplers[b];

            if(res.fixedBindNumber == reg)
            {
              shaderBind = &res;
              usedSlot = HasAccess(stage, m_Locations[i].category, b);
              break;
            }
          }
        }

        addSamplerRow(m_SamplerDescriptors[i], reg, shaderBind, usedSlot, samplers[(uint32_t)stage]);
      }
      else if(m_Locations[i].category == DescriptorCategory::ReadOnlyResource)
      {
        const ShaderResource *shaderBind = NULL;

        if(shaderRefls[(uint32_t)stage])
        {
          for(int b = 0; b < shaderRefls[(uint32_t)stage]->readOnlyResources.count(); b++)
          {
            const ShaderResource &res = shaderRefls[(uint32_t)stage]->readOnlyResources[b];

            if(res.fixedBindNumber == reg)
            {
              shaderBind = &res;
              usedSlot = HasAccess(stage, m_Locations[i].category, b);
              break;
            }
          }
        }

        addResourceRow(D3D11ViewTag(D3D11ViewTag::SRV, reg, m_Descriptors[i]), shaderBind, usedSlot,
                       resources[(uint32_t)stage]);
      }
      else if(m_Locations[i].category == DescriptorCategory::ReadWriteResource)
      {
        const ShaderResource *shaderBind = NULL;

        if(stage == ShaderStage::Compute)
        {
          if(shaderRefls[(uint32_t)stage])
          {
            for(int b = 0; b < shaderRefls[(uint32_t)stage]->readWriteResources.count(); b++)
            {
              const ShaderResource &res = shaderRefls[(uint32_t)stage]->readWriteResources[b];

              if(res.fixedBindNumber == reg)
              {
                shaderBind = &res;
                usedSlot = HasAccess(stage, m_Locations[i].category, b);
                break;
              }
            }
          }

          addResourceRow(D3D11ViewTag(D3D11ViewTag::SRV, reg, m_Descriptors[i]), shaderBind,
                         usedSlot, ui->csUAVs);
        }
        else
        {
          // skip any descriptors from before the first valid OM UAV
          if(reg < state.outputMerger.uavStartSlot)
            continue;

          // only iterate UAV descriptors from the pixel shader stage - they will be duplicated
          // per-stage and below we iterate over every stage
          if(stage != ShaderStage::Pixel)
            continue;

          // any non-CS shader can use these. When that's not supported (Before feature level 11.1)
          // this search will just boil down to only PS.
          // When multiple stages use the UAV, we allow the last stage to 'win' and define its type,
          // although it would be very surprising if the types were actually different anyway.
          for(const ShaderReflection *refl : shaderRefls)
          {
            if(refl && refl->stage != ShaderStage::Compute)
            {
              for(int b = 0; b < refl->readWriteResources.count(); b++)
              {
                const ShaderResource &res = refl->readWriteResources[b];

                if(res.fixedBindNumber == reg)
                {
                  shaderBind = &res;
                  usedSlot = HasAccess(stage, m_Locations[i].category, b);
                  break;
                }
              }
            }
          }

          addResourceRow(D3D11ViewTag(D3D11ViewTag::UAV, reg, m_Descriptors[i]), shaderBind,
                         usedSlot, ui->targetOutputs);
        }
      }
    }

    addResourceRow(D3D11ViewTag(D3D11ViewTag::OMDepth, 0, m_Ctx.CurPipelineState().GetDepthTarget()),
                   NULL, true, ui->targetOutputs);

    ui->vsClasses->parentWidget()->setVisible(ui->vsClasses->topLevelItemCount() > 0);
    ui->hsClasses->parentWidget()->setVisible(ui->hsClasses->topLevelItemCount() > 0);
    ui->dsClasses->parentWidget()->setVisible(ui->dsClasses->topLevelItemCount() > 0);
    ui->gsClasses->parentWidget()->setVisible(ui->gsClasses->topLevelItemCount() > 0);
    ui->psClasses->parentWidget()->setVisible(ui->psClasses->topLevelItemCount() > 0);
    ui->csClasses->parentWidget()->setVisible(ui->csClasses->topLevelItemCount() > 0);
  }

  bool streamoutSet = false;
  vs = ui->gsStreamOut->verticalScrollBar()->value();
  ui->gsStreamOut->beginUpdate();
  ui->gsStreamOut->clear();
  for(int i = 0; i < state.streamOut.outputs.count(); i++)
  {
    const D3D11Pipe::StreamOutBind &s = state.streamOut.outputs[i];

    bool filledSlot = (s.resourceId != ResourceId());
    bool usedSlot = (filledSlot);

    if(showNode(usedSlot, filledSlot))
    {
      qulonglong length = 0;
      BufferDescription *buf = m_Ctx.GetBuffer(s.resourceId);

      if(buf)
        length = buf->length;

      RDTreeWidgetItem *node = new RDTreeWidgetItem({
          i,
          s.resourceId,
          Formatter::HumanFormat(length, Formatter::OffsetSize),
          Formatter::HumanFormat(s.byteOffset, Formatter::OffsetSize),
          QString(),
      });

      node->setTag(QVariant::fromValue(s.resourceId));

      if(!filledSlot)
        setEmptyRow(node);

      if(!usedSlot)
        setInactiveRow(node);

      streamoutSet = true;

      ui->gsStreamOut->addTopLevelItem(node);
    }
  }
  ui->gsStreamOut->verticalScrollBar()->setValue(vs);
  ui->gsStreamOut->clearSelection();
  ui->gsStreamOut->endUpdate();

  ui->gsStreamOut->setVisible(streamoutSet);
  ui->soGroup->setVisible(streamoutSet);

  ////////////////////////////////////////////////
  // Rasterizer

  vs = ui->viewports->verticalScrollBar()->value();
  ui->viewports->beginUpdate();
  ui->viewports->clear();
  for(int i = 0; i < state.rasterizer.viewports.count(); i++)
  {
    const Viewport &v = state.rasterizer.viewports[i];

    if(v.enabled || ui->showEmpty->isChecked())
    {
      RDTreeWidgetItem *node =
          new RDTreeWidgetItem({i, v.x, v.y, v.width, v.height, v.minDepth, v.maxDepth});

      if(v.width == 0 || v.height == 0 || v.minDepth == v.maxDepth)
        setEmptyRow(node);

      if(!v.enabled)
        setInactiveRow(node);

      ui->viewports->addTopLevelItem(node);
    }
  }
  ui->viewports->verticalScrollBar()->setValue(vs);
  ui->viewports->clearSelection();
  ui->viewports->endUpdate();

  vs = ui->scissors->verticalScrollBar()->value();
  ui->scissors->beginUpdate();
  ui->scissors->clear();
  for(int i = 0; i < state.rasterizer.scissors.count(); i++)
  {
    const Scissor &s = state.rasterizer.scissors[i];

    if((s.enabled && state.rasterizer.state.scissorEnable) || ui->showEmpty->isChecked())
    {
      RDTreeWidgetItem *node = new RDTreeWidgetItem({i, s.x, s.y, s.width, s.height});

      if(s.width == 0 || s.height == 0)
        setEmptyRow(node);

      if(!s.enabled)
        setInactiveRow(node);

      ui->scissors->addTopLevelItem(node);
    }
  }
  ui->scissors->clearSelection();
  ui->scissors->verticalScrollBar()->setValue(vs);
  ui->scissors->endUpdate();

  ui->rastState->setText(ToQStr(state.rasterizer.state.resourceId));

  ui->fillMode->setText(ToQStr(state.rasterizer.state.fillMode));
  ui->cullMode->setText(ToQStr(state.rasterizer.state.cullMode));
  ui->frontCCW->setPixmap(state.rasterizer.state.frontCCW ? tick : cross);

  ui->scissorEnabled->setPixmap(state.rasterizer.state.scissorEnable ? tick : cross);
  ui->lineAA->setPixmap(state.rasterizer.state.antialiasedLines ? tick : cross);
  ui->multisample->setPixmap(state.rasterizer.state.multisampleEnable ? tick : cross);

  ui->depthClip->setPixmap(state.rasterizer.state.depthClip ? tick : cross);
  ui->depthBias->setText(Formatter::Format(state.rasterizer.state.depthBias));
  ui->depthBiasClamp->setText(Formatter::Format(state.rasterizer.state.depthBiasClamp));
  ui->slopeScaledBias->setText(Formatter::Format(state.rasterizer.state.slopeScaledDepthBias));
  ui->forcedSampleCount->setText(QString::number(state.rasterizer.state.forcedSampleCount));
  ui->conservativeRaster->setPixmap(
      state.rasterizer.state.conservativeRasterization != ConservativeRaster::Disabled ? tick
                                                                                       : cross);

  ////////////////////////////////////////////////
  // Predication

  if(state.predication.resourceId == ResourceId())
  {
    ui->predicateGroup->setVisible(false);
  }
  else
  {
    ui->predicateGroup->setVisible(true);
    ui->predicate->setText(ToQStr(state.predication.resourceId));
    ui->predicateValue->setText(state.predication.value ? lit("TRUE") : lit("FALSE"));
    ui->predicatePassing->setPixmap(state.predication.isPassing ? tick : cross);
  }

  ////////////////////////////////////////////////
  // Output Merger

  vs = ui->blends->verticalScrollBar()->value();
  ui->blends->beginUpdate();
  ui->blends->clear();
  {
    int i = 0;
    for(const ColorBlend &blend : state.outputMerger.blendState.blends)
    {
      bool filledSlot = (blend.enabled || targets[i]);
      bool usedSlot = (targets[i]);

      if(showNode(usedSlot, filledSlot))
      {
        RDTreeWidgetItem *node = NULL;

        node = new RDTreeWidgetItem(
            {i, blend.enabled ? tr("True") : tr("False"),

             ToQStr(blend.colorBlend.source), ToQStr(blend.colorBlend.destination),
             ToQStr(blend.colorBlend.operation),

             ToQStr(blend.alphaBlend.source), ToQStr(blend.alphaBlend.destination),
             ToQStr(blend.alphaBlend.operation),

             blend.logicOperationEnabled ? ToQStr(blend.logicOperation) : tr("Disabled"),

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
  ui->blends->endUpdate();
  ui->blends->verticalScrollBar()->setValue(vs);

  ui->blendState->setText(ToQStr(state.outputMerger.blendState.resourceId));

  ui->alphaToCoverage->setPixmap(state.outputMerger.blendState.alphaToCoverage ? tick : cross);
  ui->independentBlend->setPixmap(state.outputMerger.blendState.independentBlend ? tick : cross);
  ui->sampleMask->setText(Formatter::Format(state.outputMerger.blendState.sampleMask, true));

  ui->blendFactor->setText(QFormatStr("%1, %2, %3, %4")
                               .arg(state.outputMerger.blendState.blendFactor[0], 0, 'f', 2)
                               .arg(state.outputMerger.blendState.blendFactor[1], 0, 'f', 2)
                               .arg(state.outputMerger.blendState.blendFactor[2], 0, 'f', 2)
                               .arg(state.outputMerger.blendState.blendFactor[3], 0, 'f', 2));

  ui->depthState->setText(ToQStr(state.outputMerger.depthStencilState.resourceId));

  if(state.outputMerger.depthStencilState.depthEnable)
  {
    ui->depthEnabled->setPixmap(tick);
    ui->depthFunc->setText(ToQStr(state.outputMerger.depthStencilState.depthFunction));
    ui->depthWrite->setPixmap(state.outputMerger.depthStencilState.depthWrites ? tick : cross);
    ui->depthWrite->setText(QString());
  }
  else
  {
    ui->depthEnabled->setPixmap(cross);
    ui->depthFunc->setText(tr("Disabled"));
    ui->depthWrite->setPixmap(QPixmap());
    ui->depthWrite->setText(tr("Disabled"));
  }

  ui->stencilEnabled->setPixmap(state.outputMerger.depthStencilState.stencilEnable ? tick : cross);
  m_Common.SetStencilLabelValue(
      ui->stencilReadMask, (uint8_t)state.outputMerger.depthStencilState.frontFace.compareMask);
  m_Common.SetStencilLabelValue(ui->stencilWriteMask,
                                (uint8_t)state.outputMerger.depthStencilState.frontFace.writeMask);
  m_Common.SetStencilLabelValue(ui->stencilRef,
                                (uint8_t)state.outputMerger.depthStencilState.frontFace.reference);

  ui->stencils->beginUpdate();
  ui->stencils->clear();
  ui->stencils->addTopLevelItem(new RDTreeWidgetItem(
      {tr("Front"), ToQStr(state.outputMerger.depthStencilState.frontFace.function),
       ToQStr(state.outputMerger.depthStencilState.frontFace.failOperation),
       ToQStr(state.outputMerger.depthStencilState.frontFace.depthFailOperation),
       ToQStr(state.outputMerger.depthStencilState.frontFace.passOperation)}));
  ui->stencils->addTopLevelItem(new RDTreeWidgetItem(
      {tr("Back"), ToQStr(state.outputMerger.depthStencilState.backFace.function),
       ToQStr(state.outputMerger.depthStencilState.backFace.failOperation),
       ToQStr(state.outputMerger.depthStencilState.backFace.depthFailOperation),
       ToQStr(state.outputMerger.depthStencilState.backFace.passOperation)}));
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

  // highlight the appropriate stages in the flowchart
  if(action == NULL)
  {
    ui->pipeFlow->setStagesEnabled({true, true, true, true, true, true, true, true, true});
  }
  else if(action->flags & ActionFlags::Dispatch)
  {
    ui->pipeFlow->setStagesEnabled({false, false, false, false, false, false, false, false, true});
  }
  else
  {
    bool streamOutActive = false;

    for(const D3D11Pipe::StreamOutBind &o : state.streamOut.outputs)
    {
      if(o.resourceId != ResourceId())
      {
        streamOutActive = true;
        break;
      }
    }

    if(state.geometryShader.resourceId == ResourceId() && streamOutActive)
    {
      ui->pipeFlow->setStageName(4, lit("SO"), tr("Stream Out"));
    }
    else
    {
      ui->pipeFlow->setStageName(4, lit("GS"), tr("Geometry Shader"));
    }

    ui->pipeFlow->setStagesEnabled(
        {true, true, state.hullShader.resourceId != ResourceId(),
         state.domainShader.resourceId != ResourceId(),
         state.geometryShader.resourceId != ResourceId() || streamOutActive, true,
         state.pixelShader.resourceId != ResourceId(), true, false});
  }
}

void D3D11PipelineStateViewer::resource_itemActivated(RDTreeWidgetItem *item, int column)
{
  const D3D11Pipe::Shader *stage = stageForSender(item->treeWidget());

  if(stage == NULL)
    return;

  QVariant tag = item->tag();

  TextureDescription *tex = NULL;
  BufferDescription *buf = NULL;
  CompType typeCast = CompType::Typeless;

  if(tag.canConvert<ResourceId>())
  {
    ResourceId id = tag.value<ResourceId>();
    tex = m_Ctx.GetTexture(id);
    buf = m_Ctx.GetBuffer(id);
  }
  else if(tag.canConvert<D3D11ViewTag>())
  {
    D3D11ViewTag view = tag.value<D3D11ViewTag>();
    tex = m_Ctx.GetTexture(view.desc.resource);
    buf = m_Ctx.GetBuffer(view.desc.resource);
    typeCast = view.desc.format.compType;
  }

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
      viewer->ViewTexture(tex->resourceId, typeCast, true);
    }

    return;
  }
  else if(buf)
  {
    D3D11ViewTag view;

    view.desc.resource = buf->resourceId;

    if(tag.canConvert<D3D11ViewTag>())
      view = tag.value<D3D11ViewTag>();

    uint64_t offs = 0;
    uint64_t size = buf->length;

    if(view.desc.resource != ResourceId())
    {
      offs = view.desc.byteOffset;
      size = view.desc.byteSize;
    }
    else
    {
      // last thing, see if it's a streamout buffer

      if(stage->stage == ShaderStage::Geometry)
      {
        for(int i = 0; i < m_Ctx.CurD3D11PipelineState()->streamOut.outputs.count(); i++)
        {
          if(buf->resourceId == m_Ctx.CurD3D11PipelineState()->streamOut.outputs[i].resourceId)
          {
            size -= m_Ctx.CurD3D11PipelineState()->streamOut.outputs[i].byteOffset;
            offs += m_Ctx.CurD3D11PipelineState()->streamOut.outputs[i].byteOffset;
            break;
          }
        }
      }
    }

    QString format;

    const ShaderResource *shaderRes = NULL;

    uint32_t reg = view.index;

    // for OM UAVs these can be bound to any non-CS stage, so make sure
    // we have the right shader details for it.
    // This search allows later stage bindings to override earlier stage bindings,
    // which is a reasonable behaviour when the same resource can be referenced
    // in multiple places. Most likely the bindings are equivalent anyway.
    // The main point is that it allows us to pick up the binding if it's not
    // bound in the PS but only in an earlier stage.
    if(view.type == D3D11ViewTag::UAV && stage->stage != ShaderStage::Compute)
    {
      const D3D11Pipe::State &state = *m_Ctx.CurD3D11PipelineState();
      const D3D11Pipe::Shader *nonCS[] = {&state.vertexShader, &state.domainShader, &state.hullShader,
                                          &state.geometryShader, &state.pixelShader};

      for(const D3D11Pipe::Shader *searchstage : nonCS)
      {
        if(searchstage->reflection)
        {
          for(const ShaderResource &res : searchstage->reflection->readWriteResources)
          {
            if(!res.isTexture && res.fixedBindNumber == reg)
            {
              stage = searchstage;
              break;
            }
          }
        }
      }
    }

    if(stage->reflection)
    {
      const rdcarray<ShaderResource> &resArray = view.type == D3D11ViewTag::SRV
                                                     ? stage->reflection->readOnlyResources
                                                     : stage->reflection->readWriteResources;

      for(const ShaderResource &res : resArray)
      {
        if(!res.isTexture && res.fixedBindNumber == reg)
        {
          shaderRes = &res;
          break;
        }
      }
    }

    if(shaderRes)
    {
      format = BufferFormatter::GetBufferFormatString(Packing::D3DUAV, stage->resourceId,
                                                      *shaderRes, view.desc.format);

      if(view.desc.flags & DescriptorFlags::RawBuffer)
        format = lit("xint");
    }

    IBufferViewer *viewer = m_Ctx.ViewBuffer(offs, size, view.desc.resource, format);

    m_Ctx.AddDockWindow(viewer->Widget(), DockReference::AddTo, this);
  }
}

void D3D11PipelineStateViewer::cbuffer_itemActivated(RDTreeWidgetItem *item, int column)
{
  const D3D11Pipe::Shader *stage = stageForSender(item->treeWidget());

  if(stage == NULL)
    return;

  QVariant tag = item->tag();

  if(!tag.canConvert<uint32_t>())
    return;

  uint32_t reg = tag.value<uint32_t>();

  uint32_t index = ~0U;
  for(uint32_t i = 0; i < stage->reflection->constantBlocks.size(); i++)
    if(stage->reflection->constantBlocks[i].fixedBindNumber == reg)
      index = i;

  if(index == ~0U)
  {
    const Descriptor &desc = FindDescriptor(stage->stage, DescriptorCategory::ConstantBlock, reg);

    IBufferViewer *viewer = m_Ctx.ViewBuffer(desc.byteOffset, desc.byteSize, desc.resource);

    m_Ctx.AddDockWindow(viewer->Widget(), DockReference::AddTo, this);
    return;
  }

  IBufferViewer *prev = m_Ctx.ViewConstantBuffer(stage->stage, index, 0);

  m_Ctx.AddDockWindow(prev->Widget(), DockReference::TransientPopupArea, this, 0.3f);
}

void D3D11PipelineStateViewer::on_iaLayouts_itemActivated(RDTreeWidgetItem *item, int column)
{
  on_meshView_clicked();
}

void D3D11PipelineStateViewer::on_iaBuffers_itemActivated(RDTreeWidgetItem *item, int column)
{
  QVariant tag = item->tag();

  if(tag.canConvert<D3D11VBIBTag>())
  {
    D3D11VBIBTag buf = tag.value<D3D11VBIBTag>();

    if(buf.id != ResourceId())
    {
      IBufferViewer *viewer = m_Ctx.ViewBuffer(buf.offset, UINT64_MAX, buf.id, buf.format);

      m_Ctx.AddDockWindow(viewer->Widget(), DockReference::AddTo, this);
    }
  }
}

void D3D11PipelineStateViewer::highlightIABind(int slot)
{
  int idx = ((slot + 1) * 21) % 32;    // space neighbouring colours reasonably distinctly

  const D3D11Pipe::InputAssembly &IA = m_Ctx.CurD3D11PipelineState()->inputAssembly;

  QColor col = QColor::fromHslF(float(idx) / 32.0f, 1.0f,
                                qBound(0.05, palette().color(QPalette::Base).lightnessF(), 0.95));

  ui->iaLayouts->beginUpdate();
  ui->iaBuffers->beginUpdate();

  if(slot < m_VBNodes.count())
  {
    RDTreeWidgetItem *item = m_VBNodes[(int)slot];

    if(item && !m_EmptyNodes.contains(item))
    {
      item->setBackgroundColor(col);
      item->setForegroundColor(contrastingColor(col, QColor(0, 0, 0)));
    }
  }

  for(int i = 0; i < ui->iaLayouts->topLevelItemCount(); i++)
  {
    RDTreeWidgetItem *item = ui->iaLayouts->topLevelItem(i);

    if((int)IA.layouts[item->tag().toUInt()].inputSlot != slot)
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

  ui->iaLayouts->endUpdate();
  ui->iaBuffers->endUpdate();
}

void D3D11PipelineStateViewer::on_iaLayouts_mouseMove(QMouseEvent *e)
{
  if(!m_Ctx.IsCaptureLoaded())
    return;

  RDTreeWidgetItem *item = ui->iaLayouts->itemAt(e->pos());

  vertex_leave(NULL);

  const D3D11Pipe::InputAssembly &IA = m_Ctx.CurD3D11PipelineState()->inputAssembly;

  if(item)
  {
    uint32_t buffer = IA.layouts[item->tag().toUInt()].inputSlot;

    highlightIABind((int)buffer);
  }
}

void D3D11PipelineStateViewer::on_iaBuffers_mouseMove(QMouseEvent *e)
{
  if(!m_Ctx.IsCaptureLoaded())
    return;

  RDTreeWidgetItem *item = ui->iaBuffers->itemAt(e->pos());

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
        item->setBackground(ui->iaBuffers->palette().brush(QPalette::Window));
        item->setForeground(QBrush());
      }
    }
  }
}

void D3D11PipelineStateViewer::on_pipeFlow_stageSelected(int index)
{
  ui->stagesTabs->setCurrentIndex(index);
}

void D3D11PipelineStateViewer::vertex_leave(QEvent *e)
{
  ui->iaLayouts->beginUpdate();
  ui->iaBuffers->beginUpdate();

  for(int i = 0; i < ui->iaLayouts->topLevelItemCount(); i++)
  {
    RDTreeWidgetItem *item = ui->iaLayouts->topLevelItem(i);

    item->setBackground(QBrush());
    item->setForeground(QBrush());
  }

  for(int i = 0; i < ui->iaBuffers->topLevelItemCount(); i++)
  {
    RDTreeWidgetItem *item = ui->iaBuffers->topLevelItem(i);

    if(m_EmptyNodes.contains(item))
      continue;

    item->setBackground(QBrush());
    item->setForeground(QBrush());
  }

  ui->iaLayouts->endUpdate();
  ui->iaBuffers->endUpdate();
}

void D3D11PipelineStateViewer::shaderView_clicked()
{
  ShaderReflection *shaderDetails = NULL;

  QWidget *sender = qobject_cast<QWidget *>(QObject::sender());
  if(sender == ui->iaBytecode || sender == ui->iaBytecodeViewButton)
  {
    shaderDetails = m_Ctx.CurD3D11PipelineState()->inputAssembly.bytecode;
  }
  else
  {
    const D3D11Pipe::Shader *stage = stageForSender(sender);

    if(stage == NULL || stage->resourceId == ResourceId())
      return;

    shaderDetails = stage->reflection;
  }

  if(!shaderDetails)
    return;

  IShaderViewer *shad = m_Ctx.ViewShader(shaderDetails, ResourceId());

  m_Ctx.AddDockWindow(shad->Widget(), DockReference::AddTo, this);
}

void D3D11PipelineStateViewer::shaderSave_clicked()
{
  const D3D11Pipe::Shader *stage = stageForSender(qobject_cast<QWidget *>(QObject::sender()));

  if(stage == NULL)
    return;

  ShaderReflection *shaderDetails = stage->reflection;

  if(stage->resourceId == ResourceId())
    return;

  m_Common.SaveShaderFile(shaderDetails);
}

QVariantList D3D11PipelineStateViewer::exportViewHTML(const Descriptor &view, uint32_t reg,
                                                      ShaderReflection *refl,
                                                      const QString &extraParams)
{
  const ShaderResource *shaderInput = NULL;

  bool rw = false;

  if(refl)
  {
    for(const ShaderResource &bind : refl->readOnlyResources)
    {
      if(bind.fixedBindNumber == reg)
      {
        shaderInput = &bind;
        break;
      }
    }
    for(const ShaderResource &bind : refl->readWriteResources)
    {
      if(bind.fixedBindNumber == reg)
      {
        shaderInput = &bind;
        rw = true;
        break;
      }
    }
  }

  QString name =
      view.resource == ResourceId() ? tr("Empty") : QString(m_Ctx.GetResourceName(view.resource));
  QString typeName = tr("Unknown");
  QString format = tr("Unknown");
  uint64_t w = 1;
  uint32_t h = 1, d = 1;
  uint32_t a = 0;

  QString viewFormat = view.format.Name();

  TextureDescription *tex = m_Ctx.GetTexture(view.resource);
  BufferDescription *buf = m_Ctx.GetBuffer(view.resource);

  QString viewParams;

  // check to see if it's a texture
  if(tex)
  {
    w = tex->width;
    h = tex->height;
    d = tex->depth;
    a = tex->arraysize;
    format = tex->format.Name();
    typeName = ToQStr(tex->type);

    if(tex->mips > 1)
      viewParams = tr("Highest Mip: %1, Num Mips: %2").arg(view.firstMip).arg(view.numMips);

    if(tex->arraysize > 1)
    {
      if(!viewParams.isEmpty())
        viewParams += lit(", ");
      viewParams += tr("First Slice: %1, Array Size: %2").arg(view.firstSlice).arg(view.numSlices);
    }
  }

  // if not a texture, it must be a buffer
  if(buf)
  {
    w = buf->length;
    h = 0;
    d = 0;
    a = 0;
    format = view.format.Name();
    typeName = lit("Buffer");

    if(view.flags & DescriptorFlags::RawBuffer)
    {
      typeName = rw ? lit("RWByteAddressBuffer") : lit("ByteAddressBuffer");
    }
    else if(view.elementByteSize > 0)
    {
      // for structured buffers, display how many 'elements' there are in the buffer
      typeName = QFormatStr("%1[%2]")
                     .arg(rw ? lit("RWStructuredBuffer") : lit("StructuredBuffer"))
                     .arg(buf->length / view.elementByteSize);
    }

    if(view.flags & DescriptorFlags::AppendBuffer || view.flags & DescriptorFlags::CounterBuffer)
    {
      typeName += tr(" (Count: %1)").arg(view.bufferStructCount);
    }

    if(shaderInput && !shaderInput->isTexture)
    {
      if(view.format.compType == CompType::Typeless)
      {
        if(shaderInput->variableType.baseType == VarType::Struct)
          viewFormat = format = lit("struct ") + shaderInput->variableType.name;
        else
          viewFormat = format = shaderInput->variableType.name;
      }
      else
      {
        format = view.format.Name();
      }
    }

    viewParams = tr("Byte Offset: %1, Byte Size %2, Flags %3")
                     .arg(view.byteOffset)
                     .arg(view.byteSize)
                     .arg(ToQStr(view.flags));
  }

  if(viewParams.isEmpty())
    viewParams = extraParams;
  else
    viewParams += lit(", ") + extraParams;

  return {reg,    name,      ToQStr(view.textureType), typeName, (qulonglong)w, h, d, a, viewFormat,
          format, viewParams};
}

void D3D11PipelineStateViewer::exportHTML(QXmlStreamWriter &xml, const D3D11Pipe::InputAssembly &ia)
{
  const ActionDescription *action = m_Ctx.CurAction();

  {
    xml.writeStartElement(lit("h3"));
    xml.writeCharacters(tr("Input Layouts"));
    xml.writeEndElement();

    QList<QVariantList> rows;

    int i = 0;
    for(const D3D11Pipe::Layout &l : ia.layouts)
    {
      rows.push_back({i, l.semanticName, l.semanticIndex, l.format.Name(), l.inputSlot,
                      l.byteOffset, (bool)l.perInstance, l.instanceDataStepRate});

      i++;
    }

    m_Common.exportHTMLTable(
        xml,
        {tr("Slot"), tr("Semantic Name"), tr("Semantic Index"), tr("Format"), tr("Input Slot"),
         tr("Byte Offset"), tr("Per Instance"), tr("Instance Data Step Rate")},
        rows);
  }

  {
    xml.writeStartElement(lit("h3"));
    xml.writeCharacters(tr("Vertex Buffers"));
    xml.writeEndElement();

    QList<QVariantList> rows;

    int i = 0;
    for(const D3D11Pipe::VertexBuffer &vb : ia.vertexBuffers)
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

      rows.push_back({i, name, vb.byteStride, vb.byteOffset, (qulonglong)length});

      i++;
    }

    m_Common.exportHTMLTable(
        xml, {tr("Slot"), tr("Buffer"), tr("Stride"), tr("Offset"), tr("Byte Length")}, rows);
  }

  {
    xml.writeStartElement(lit("h3"));
    xml.writeCharacters(tr("Index Buffer"));
    xml.writeEndElement();

    QString name = m_Ctx.GetResourceName(ia.indexBuffer.resourceId);
    uint64_t length = 0;

    if(ia.indexBuffer.resourceId == ResourceId())
    {
      name = tr("Empty");
    }
    else
    {
      BufferDescription *buf = m_Ctx.GetBuffer(ia.indexBuffer.resourceId);
      if(buf)
        length = buf->length;
    }

    QString ifmt = lit("UNKNOWN");
    if(ia.indexBuffer.byteStride == 2)
      ifmt = lit("R16_UINT");
    if(ia.indexBuffer.byteStride == 4)
      ifmt = lit("R32_UINT");

    m_Common.exportHTMLTable(xml, {tr("Buffer"), tr("Format"), tr("Offset"), tr("Byte Length")},
                             {name, ifmt, ia.indexBuffer.byteOffset, (qulonglong)length});
  }

  xml.writeStartElement(lit("p"));
  xml.writeEndElement();

  m_Common.exportHTMLTable(xml, {tr("Primitive Topology")}, {ToQStr(ia.topology)});
}

void D3D11PipelineStateViewer::exportHTML(QXmlStreamWriter &xml, const D3D11Pipe::Shader &sh)
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

    if(shaderDetails && !shaderDetails->debugInfo.files.isEmpty())
    {
      const ShaderDebugInfo &dbg = shaderDetails->debugInfo;
      int entryFile = qMax(0, dbg.entryLocation.fileIndex);

      shadername = QFormatStr("%1() - %2")
                       .arg(shaderDetails->entryPoint)
                       .arg(QFileInfo(dbg.files[entryFile].filename).fileName());
    }

    xml.writeStartElement(lit("p"));
    xml.writeCharacters(shadername);
    xml.writeEndElement();

    if(sh.resourceId == ResourceId())
      return;
  }

  {
    xml.writeStartElement(lit("h3"));
    xml.writeCharacters(tr("Resources"));
    xml.writeEndElement();

    QList<QVariantList> rows;

    // do a plain search here, this is not super efficient but it's simpler
    for(uint32_t i = 0; i < m_Locations.size(); i++)
    {
      if(!(m_Locations[i].stageMask & MaskForStage(sh.stage)))
        continue;

      if(m_Locations[i].category != DescriptorCategory::ReadOnlyResource)
        continue;

      if(m_Descriptors[i].view == ResourceId())
        continue;

      rows.push_back(exportViewHTML(m_Descriptors[i], m_Locations[i].fixedBindNumber, shaderDetails,
                                    QString()));
    }

    m_Common.exportHTMLTable(xml,
                             {
                                 tr("Slot"),
                                 tr("Name"),
                                 tr("View Type"),
                                 tr("Resource Type"),
                                 tr("Width"),
                                 tr("Height"),
                                 tr("Depth"),
                                 tr("Array Size"),
                                 tr("View Format"),
                                 tr("Resource Format"),
                                 tr("View Parameters"),
                             },
                             rows);
  }

  if(sh.stage == ShaderStage::Compute)
  {
    xml.writeStartElement(lit("h3"));
    xml.writeCharacters(tr("Unordered Access Views"));
    xml.writeEndElement();

    QList<QVariantList> rows;

    // do a plain search here, this is not super efficient but it's simpler
    for(uint32_t i = 0; i < m_Locations.size(); i++)
    {
      if(!(m_Locations[i].stageMask & MaskForStage(sh.stage)))
        continue;

      if(m_Locations[i].category != DescriptorCategory::ReadWriteResource)
        continue;

      if(m_Descriptors[i].view == ResourceId())
        continue;

      rows.push_back(exportViewHTML(m_Descriptors[i], m_Locations[i].fixedBindNumber, shaderDetails,
                                    QString()));
    }

    m_Common.exportHTMLTable(xml,
                             {
                                 tr("Slot"),
                                 tr("Name"),
                                 tr("View Type"),
                                 tr("Resource Type"),
                                 tr("Width"),
                                 tr("Height"),
                                 tr("Depth"),
                                 tr("Array Size"),
                                 tr("View Format"),
                                 tr("Resource Format"),
                                 tr("View Parameters"),
                             },
                             rows);
  }

  {
    xml.writeStartElement(lit("h3"));
    xml.writeCharacters(tr("Samplers"));
    xml.writeEndElement();

    QList<QVariantList> rows;

    const rdcarray<UsedDescriptor> &samplers = m_Ctx.CurPipelineState().GetSamplers(sh.stage);

    for(int i = 0; i < samplers.count(); i++)
    {
      const SamplerDescriptor &s = samplers[i].sampler;

      if(s.object == ResourceId())
        continue;

      QString borderColor = QFormatStr("%1, %2, %3, %4")
                                .arg(s.borderColorValue.floatValue[0])
                                .arg(s.borderColorValue.floatValue[1])
                                .arg(s.borderColorValue.floatValue[2])
                                .arg(s.borderColorValue.floatValue[3]);

      QString addressing;

      QString addPrefix;
      QString addVal;

      QString addr[] = {ToQStr(s.addressU, GraphicsAPI::D3D11),
                        ToQStr(s.addressV, GraphicsAPI::D3D11),
                        ToQStr(s.addressW, GraphicsAPI::D3D11)};

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

      rows.push_back({i, addressing, borderColor, ToQStr(s.compareFunction), ToQStr(s.filter),
                      s.maxAnisotropy, s.minLOD == -FLT_MAX ? lit("0") : QString::number(s.minLOD),
                      s.maxLOD == FLT_MAX ? lit("FLT_MAX") : QString::number(s.maxLOD), s.mipBias});
    }

    m_Common.exportHTMLTable(xml,
                             {
                                 tr("Slot"),
                                 tr("Addressing"),
                                 tr("Border Colour"),
                                 tr("Comparison"),
                                 tr("Filter"),
                                 tr("Max Anisotropy"),
                                 tr("Min LOD"),
                                 tr("Max LOD"),
                                 tr("Mip Bias"),
                             },
                             rows);
  }

  {
    xml.writeStartElement(lit("h3"));
    xml.writeCharacters(tr("Constant Buffers"));
    xml.writeEndElement();

    QList<QVariantList> rows;

    const rdcarray<UsedDescriptor> &cblocks = m_Ctx.CurPipelineState().GetConstantBlocks(sh.stage);

    for(int i = 0; i < cblocks.count(); i++)
    {
      ConstantBlock *shaderCBuf = NULL;

      if(cblocks[i].descriptor.resource == ResourceId())
        continue;

      if(shaderDetails && cblocks[i].access.index < shaderDetails->constantBlocks.count() &&
         !shaderDetails->constantBlocks[cblocks[i].access.index].name.isEmpty())
        shaderCBuf = &shaderDetails->constantBlocks[cblocks[i].access.index];

      QString name = m_Ctx.GetResourceName(cblocks[i].descriptor.resource);
      uint64_t length = 1;
      int numvars = shaderCBuf ? shaderCBuf->variables.count() : 0;
      uint32_t byteSize = shaderCBuf ? shaderCBuf->byteSize : 0;

      if(cblocks[i].descriptor.resource == ResourceId())
      {
        name = tr("Empty");
        length = 0;
      }

      BufferDescription *buf = m_Ctx.GetBuffer(cblocks[i].descriptor.resource);
      if(buf)
        length = buf->length;

      rows.push_back({i, name, (qulonglong)cblocks[i].descriptor.byteOffset,
                      (qulonglong)cblocks[i].descriptor.byteSize, numvars, byteSize,
                      (qulonglong)length});
    }

    m_Common.exportHTMLTable(xml,
                             {tr("Slot"), tr("Buffer"), tr("Byte Offset"), tr("Byte Range"),
                              tr("Number of Variables"), tr("Bytes Needed"), tr("Bytes Provided")},
                             rows);
  }

  if(!sh.classInstances.isEmpty())
  {
    xml.writeStartElement(lit("h3"));
    xml.writeCharacters(tr("Class Instances"));
    xml.writeEndElement();

    QList<QVariantList> rows;

    for(int i = 0; i < sh.classInstances.count(); i++)
    {
      QString interfaceName = tr("Interface %1").arg(i);

      if(sh.reflection && i < sh.reflection->interfaces.count())
        interfaceName = sh.reflection->interfaces[i];

      rows.push_back({i, interfaceName, sh.classInstances[i]});
    }

    m_Common.exportHTMLTable(xml,
                             {
                                 tr("Slot"),
                                 tr("Interface Name"),
                                 tr("Instance Name"),
                             },
                             rows);
  }
}

void D3D11PipelineStateViewer::exportHTML(QXmlStreamWriter &xml, const D3D11Pipe::StreamOut &so)
{
  {
    xml.writeStartElement(lit("h3"));
    xml.writeCharacters(tr("Stream Out Targets"));
    xml.writeEndElement();

    QList<QVariantList> rows;

    int i = 0;
    for(const D3D11Pipe::StreamOutBind &o : so.outputs)
    {
      QString name = m_Ctx.GetResourceName(o.resourceId);
      uint64_t length = 0;

      if(o.resourceId == ResourceId())
      {
        name = tr("Empty");
      }
      else
      {
        BufferDescription *buf = m_Ctx.GetBuffer(o.resourceId);
        if(buf)
          length = buf->length;
      }

      rows.push_back({i, name, o.byteOffset, (qulonglong)length});

      i++;
    }

    m_Common.exportHTMLTable(xml, {tr("Slot"), tr("Buffer"), tr("Offset"), tr("Byte Length")}, rows);
  }
}

void D3D11PipelineStateViewer::exportHTML(QXmlStreamWriter &xml, const D3D11Pipe::Rasterizer &rs)
{
  {
    xml.writeStartElement(lit("h3"));
    xml.writeCharacters(tr("States"));
    xml.writeEndElement();

    m_Common.exportHTMLTable(xml, {tr("Fill Mode"), tr("Cull Mode"), tr("Front CCW")},
                             {ToQStr(rs.state.fillMode), ToQStr(rs.state.cullMode),
                              rs.state.frontCCW ? tr("Yes") : tr("No")});

    xml.writeStartElement(lit("p"));
    xml.writeEndElement();

    m_Common.exportHTMLTable(
        xml,
        {tr("Scissor Enable"), tr("Line AA Enable"), tr("Multisample Enable"),
         tr("Forced Sample Count"), tr("Conservative Raster")},
        {rs.state.scissorEnable ? tr("Yes") : tr("No"),
         rs.state.antialiasedLines ? tr("Yes") : tr("No"),
         rs.state.multisampleEnable ? tr("Yes") : tr("No"), rs.state.forcedSampleCount,
         rs.state.conservativeRasterization != ConservativeRaster::Disabled ? tr("Yes") : tr("No")});

    xml.writeStartElement(lit("p"));
    xml.writeEndElement();

    m_Common.exportHTMLTable(
        xml, {tr("Depth Clip"), tr("Depth Bias"), tr("Depth Bias Clamp"), tr("Slope Scaled Bias")},
        {rs.state.depthClip ? tr("Yes") : tr("No"), rs.state.depthBias,
         Formatter::Format(rs.state.depthBiasClamp),
         Formatter::Format(rs.state.slopeScaledDepthBias)});
  }

  {
    xml.writeStartElement(lit("h3"));
    xml.writeCharacters(tr("Viewports"));
    xml.writeEndElement();

    QList<QVariantList> rows;

    int i = 0;
    for(const Viewport &v : rs.viewports)
    {
      rows.push_back({i, v.x, v.y, v.width, v.height, v.minDepth, v.maxDepth,
                      v.enabled ? tr("Yes") : tr("No")});

      i++;
    }

    m_Common.exportHTMLTable(xml,
                             {tr("Slot"), tr("X"), tr("Y"), tr("Width"), tr("Height"),
                              tr("Min Depth"), tr("Max Depth"), tr("Enabled")},
                             rows);
  }

  {
    xml.writeStartElement(lit("h3"));
    xml.writeCharacters(tr("Scissors"));
    xml.writeEndElement();

    QList<QVariantList> rows;

    int i = 0;
    for(const Scissor &s : rs.scissors)
    {
      rows.push_back({i, s.x, s.y, s.width, s.height, s.enabled ? tr("Yes") : tr("No")});

      i++;
    }

    m_Common.exportHTMLTable(
        xml, {tr("Slot"), tr("X"), tr("Y"), tr("Width"), tr("Height"), tr("Enabled")}, rows);
  }
}

void D3D11PipelineStateViewer::exportHTML(QXmlStreamWriter &xml, const D3D11Pipe::OutputMerger &om)
{
  {
    xml.writeStartElement(lit("h3"));
    xml.writeCharacters(tr("Blend State"));
    xml.writeEndElement();

    QString blendFactor = QFormatStr("%1, %2, %3, %4")
                              .arg(om.blendState.blendFactor[0], 0, 'f', 2)
                              .arg(om.blendState.blendFactor[1], 0, 'f', 2)
                              .arg(om.blendState.blendFactor[2], 0, 'f', 2)
                              .arg(om.blendState.blendFactor[3], 0, 'f', 2);

    m_Common.exportHTMLTable(xml,
                             {tr("Independent Blend Enable"), tr("Alpha to Coverage"),
                              tr("Sample Mask"), tr("Blend Factor")},
                             {
                                 om.blendState.independentBlend ? tr("Yes") : tr("No"),
                                 om.blendState.alphaToCoverage ? tr("Yes") : tr("No"),
                                 Formatter::Format(om.blendState.sampleMask, true),
                                 blendFactor,
                             });

    xml.writeStartElement(lit("h3"));
    xml.writeCharacters(tr("Target Blends"));
    xml.writeEndElement();

    QList<QVariantList> rows;

    int i = 0;
    for(const ColorBlend &b : om.blendState.blends)
    {
      if(i >= om.renderTargets.count())
        continue;

      QString mask = QFormatStr("%1%2%3%4")
                         .arg((b.writeMask & 0x1) == 0 ? lit("_") : lit("R"))
                         .arg((b.writeMask & 0x2) == 0 ? lit("_") : lit("G"))
                         .arg((b.writeMask & 0x4) == 0 ? lit("_") : lit("B"))
                         .arg((b.writeMask & 0x8) == 0 ? lit("_") : lit("A"));

      rows.push_back({i, b.enabled ? tr("Yes") : tr("No"),
                      b.logicOperationEnabled ? tr("Yes") : tr("No"), ToQStr(b.colorBlend.source),
                      ToQStr(b.colorBlend.destination), ToQStr(b.colorBlend.operation),
                      ToQStr(b.alphaBlend.source), ToQStr(b.alphaBlend.destination),
                      ToQStr(b.alphaBlend.operation), ToQStr(b.logicOperation), mask});

      i++;
    }

    m_Common.exportHTMLTable(xml,
                             {
                                 tr("Slot"),
                                 tr("Blend Enable"),
                                 tr("Logic Enable"),
                                 tr("Blend Source"),
                                 tr("Blend Destination"),
                                 tr("Blend Operation"),
                                 tr("Alpha Blend Source"),
                                 tr("Alpha Blend Destination"),
                                 tr("Alpha Blend Operation"),
                                 tr("Logic Operation"),
                                 tr("Write Mask"),
                             },
                             rows);
  }

  {
    xml.writeStartElement(lit("h3"));
    xml.writeCharacters(tr("Depth State"));
    xml.writeEndElement();

    m_Common.exportHTMLTable(
        xml, {tr("Depth Test Enable"), tr("Depth Writes Enable"), tr("Depth Function")},
        {om.depthStencilState.depthEnable ? tr("Yes") : tr("No"),
         om.depthStencilState.depthWrites ? tr("Yes") : tr("No"),
         ToQStr(om.depthStencilState.depthFunction)});
  }

  {
    xml.writeStartElement(lit("h3"));
    xml.writeCharacters(tr("Stencil State"));
    xml.writeEndElement();

    m_Common.exportHTMLTable(
        xml, {tr("Stencil Test Enable"), tr("Stencil Read Mask"), tr("Stencil Write Mask")},
        {om.depthStencilState.stencilEnable ? tr("Yes") : tr("No"),
         Formatter::Format(om.depthStencilState.frontFace.compareMask, true),
         Formatter::Format(om.depthStencilState.frontFace.writeMask, true)});

    xml.writeStartElement(lit("p"));
    xml.writeEndElement();

    m_Common.exportHTMLTable(xml,
                             {tr("Face"), tr("Function"), tr("Pass Operation"),
                              tr("Fail Operation"), tr("Depth Fail Operation")},
                             {
                                 {tr("Front"), ToQStr(om.depthStencilState.frontFace.function),
                                  ToQStr(om.depthStencilState.frontFace.passOperation),
                                  ToQStr(om.depthStencilState.frontFace.failOperation),
                                  ToQStr(om.depthStencilState.frontFace.depthFailOperation)},
                                 {tr("Back"), ToQStr(om.depthStencilState.backFace.function),
                                  ToQStr(om.depthStencilState.backFace.passOperation),
                                  ToQStr(om.depthStencilState.backFace.failOperation),
                                  ToQStr(om.depthStencilState.backFace.depthFailOperation)},
                             });
  }

  {
    xml.writeStartElement(lit("h3"));
    xml.writeCharacters(tr("Render targets"));
    xml.writeEndElement();

    QList<QVariantList> rows;

    rdcarray<Descriptor> rts = m_Ctx.CurPipelineState().GetOutputTargets();
    for(int i = 0; i < rts.count(); i++)
    {
      if(rts[i].view == ResourceId())
        continue;

      rows.push_back(exportViewHTML(rts[i], i, NULL, QString()));
    }

    m_Common.exportHTMLTable(xml,
                             {
                                 tr("Slot"),
                                 tr("Name"),
                                 tr("View Type"),
                                 tr("Resource Type"),
                                 tr("Width"),
                                 tr("Height"),
                                 tr("Depth"),
                                 tr("Array Size"),
                                 tr("View Format"),
                                 tr("Resource Format"),
                                 tr("View Parameters"),
                             },
                             rows);
  }

  {
    QList<QVariantList> rows;

    // do a plain search here, this is not super efficient but it's simpler
    for(uint32_t i = 0; i < m_Locations.size(); i++)
    {
      // only look for PS UAVs
      if(m_Locations[i].category != DescriptorCategory::ReadWriteResource ||
         !(m_Locations[i].stageMask & ShaderStageMask::Pixel))
        continue;

      if(m_Descriptors[i].view == ResourceId())
        continue;

      // skip any descriptors from before the first valid OM UAV
      if(m_Locations[i].fixedBindNumber < om.uavStartSlot)
        continue;

      rows.push_back(exportViewHTML(m_Descriptors[i], m_Locations[i].fixedBindNumber,
                                    m_Ctx.CurD3D11PipelineState()->pixelShader.reflection, QString()));
    }

    if(!rows.isEmpty())
    {
      xml.writeStartElement(lit("h3"));
      xml.writeCharacters(tr("Unordered Access Views"));
      xml.writeEndElement();

      for(uint32_t i = 0; i < om.uavStartSlot; i++)
        rows.insert(0, {i, tr("Empty"), QString(), QString(), QString(), QString(), 0, 0, 0, 0,
                        QString(), QString(), QString()});

      m_Common.exportHTMLTable(xml,
                               {
                                   tr("Slot"),
                                   tr("Name"),
                                   tr("View Type"),
                                   tr("Resource Type"),
                                   tr("Width"),
                                   tr("Height"),
                                   tr("Depth"),
                                   tr("Array Size"),
                                   tr("View Format"),
                                   tr("Resource Format"),
                                   tr("View Parameters"),
                               },
                               rows);
    }
  }

  {
    xml.writeStartElement(lit("h3"));
    xml.writeCharacters(tr("Depth target"));
    xml.writeEndElement();

    QList<QVariantList> rows;

    QString extra;

    if(om.depthReadOnly && om.stencilReadOnly)
      extra = tr("Depth & Stencil Read-Only");
    else if(om.depthReadOnly)
      extra = tr("Depth Read-Only");
    else if(om.stencilReadOnly)
      extra = tr("Stencil Read-Only");

    rows.push_back(exportViewHTML(m_Ctx.CurPipelineState().GetDepthTarget(), 0, NULL, extra));

    m_Common.exportHTMLTable(xml,
                             {
                                 tr("Slot"),
                                 tr("Name"),
                                 tr("View Type"),
                                 tr("Resource Type"),
                                 tr("Width"),
                                 tr("Height"),
                                 tr("Depth"),
                                 tr("Array Size"),
                                 tr("View Format"),
                                 tr("Resource Format"),
                                 tr("View Parameters"),
                             },
                             rows);
  }
}

void D3D11PipelineStateViewer::on_exportHTML_clicked()
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
        case 0: exportHTML(xml, m_Ctx.CurD3D11PipelineState()->inputAssembly); break;
        case 1: exportHTML(xml, m_Ctx.CurD3D11PipelineState()->vertexShader); break;
        case 2: exportHTML(xml, m_Ctx.CurD3D11PipelineState()->hullShader); break;
        case 3: exportHTML(xml, m_Ctx.CurD3D11PipelineState()->domainShader); break;
        case 4:
          exportHTML(xml, m_Ctx.CurD3D11PipelineState()->geometryShader);
          exportHTML(xml, m_Ctx.CurD3D11PipelineState()->streamOut);
          break;
        case 5: exportHTML(xml, m_Ctx.CurD3D11PipelineState()->rasterizer); break;
        case 6: exportHTML(xml, m_Ctx.CurD3D11PipelineState()->pixelShader); break;
        case 7: exportHTML(xml, m_Ctx.CurD3D11PipelineState()->outputMerger); break;
        case 8: exportHTML(xml, m_Ctx.CurD3D11PipelineState()->computeShader); break;
      }

      xml.writeEndElement();

      stage++;
    }

    m_Common.endHTMLExport(xmlptr);
  }
}

void D3D11PipelineStateViewer::on_meshView_clicked()
{
  if(!m_Ctx.HasMeshPreview())
    m_Ctx.ShowMeshPreview();
  ToolWindowManager::raiseToolWindow(m_Ctx.GetMeshPreview()->Widget());
}

void D3D11PipelineStateViewer::on_computeDebugSelector_clicked()
{
  // Check whether debugging is valid for this event before showing the dialog
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

void D3D11PipelineStateViewer::computeDebugSelector_beginDebug(
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
