/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2020 Baldur Karlsson
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

#include "D3D12PipelineStateViewer.h"
#include <float.h>
#include <QMenu>
#include <QMouseEvent>
#include <QScrollBar>
#include <QXmlStreamWriter>
#include "Code/Resources.h"
#include "Widgets/Extended/RDHeaderView.h"
#include "flowlayout/FlowLayout.h"
#include "toolwindowmanager/ToolWindowManager.h"
#include "PipelineStateViewer.h"
#include "ui_D3D12PipelineStateViewer.h"

struct D3D12VBIBTag
{
  D3D12VBIBTag() { offset = 0; }
  D3D12VBIBTag(ResourceId i, uint64_t offs, uint64_t sz, QString f = QString())
  {
    id = i;
    offset = offs;
    size = sz;
    format = f;
  }

  ResourceId id;
  uint64_t offset;
  uint64_t size;
  QString format;
};

Q_DECLARE_METATYPE(D3D12VBIBTag);

struct D3D12CBufTag
{
  D3D12CBufTag()
  {
    idx = ~0U;
    space = reg = rootElement = arrayIdx = 0;
  }
  D3D12CBufTag(uint32_t s, uint32_t r, uint32_t el)
  {
    idx = ~0U;
    space = s;
    reg = r;
    rootElement = el;
    arrayIdx = 0;
  }
  D3D12CBufTag(uint32_t i)
  {
    idx = i;
    space = reg = rootElement = arrayIdx = 0;
  }

  uint32_t idx, space, reg, rootElement, arrayIdx;
};

Q_DECLARE_METATYPE(D3D12CBufTag);

struct D3D12ViewTag
{
  enum ResType
  {
    SRV,
    UAV,
    OMTarget,
    OMDepth,
  };

  D3D12ViewTag() : type(SRV), space(0), rootElement(0), immediate(false) {}
  D3D12ViewTag(ResType t, int s, int el, bool imm, const D3D12Pipe::View &rs)
      : type(t), space(s), rootElement(el), immediate(imm), res(rs)
  {
  }

  ResType type;
  int space, rootElement;
  bool immediate;
  D3D12Pipe::View res;
};

Q_DECLARE_METATYPE(D3D12ViewTag);

D3D12PipelineStateViewer::D3D12PipelineStateViewer(ICaptureContext &ctx,
                                                   PipelineStateViewer &common, QWidget *parent)
    : QFrame(parent), ui(new Ui::D3D12PipelineStateViewer), m_Ctx(ctx), m_Common(common)
{
  ui->setupUi(this);

  const QIcon &action = Icons::action();
  const QIcon &action_hover = Icons::action_hover();

  RDLabel *shaderLabels[] = {
      ui->vsShader, ui->hsShader, ui->dsShader, ui->gsShader, ui->psShader, ui->csShader,
  };

  RDLabel *rootsigLabels[] = {
      ui->vsRootSig, ui->hsRootSig, ui->dsRootSig, ui->gsRootSig, ui->psRootSig, ui->csRootSig,
  };

  QToolButton *viewButtons[] = {
      ui->vsShaderViewButton, ui->hsShaderViewButton, ui->dsShaderViewButton,
      ui->gsShaderViewButton, ui->psShaderViewButton, ui->csShaderViewButton,
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

  RDTreeWidget *uavs[] = {
      ui->vsUAVs, ui->hsUAVs, ui->dsUAVs, ui->gsUAVs, ui->psUAVs, ui->csUAVs,
  };

  RDTreeWidget *samplers[] = {
      ui->vsSamplers, ui->hsSamplers, ui->dsSamplers,
      ui->gsSamplers, ui->psSamplers, ui->csSamplers,
  };

  RDTreeWidget *cbuffers[] = {
      ui->vsCBuffers, ui->hsCBuffers, ui->dsCBuffers,
      ui->gsCBuffers, ui->psCBuffers, ui->csCBuffers,
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
    QObject::connect(b, &QToolButton::clicked, this, &D3D12PipelineStateViewer::shaderView_clicked);

  for(RDLabel *b : shaderLabels)
  {
    b->setAutoFillBackground(true);
    b->setBackgroundRole(QPalette::ToolTipBase);
    b->setForegroundRole(QPalette::ToolTipText);
    b->setMinimumSizeHint(QSize(250, 0));
  }

  for(RDLabel *b : rootsigLabels)
  {
    b->setAutoFillBackground(true);
    b->setBackgroundRole(QPalette::ToolTipBase);
    b->setForegroundRole(QPalette::ToolTipText);
    b->setMinimumSizeHint(QSize(100, 0));
  }

  for(QToolButton *b : editButtons)
    QObject::connect(b, &QToolButton::clicked, &m_Common, &PipelineStateViewer::shaderEdit_clicked);

  for(QToolButton *b : saveButtons)
    QObject::connect(b, &QToolButton::clicked, this, &D3D12PipelineStateViewer::shaderSave_clicked);

  QObject::connect(ui->iaLayouts, &RDTreeWidget::leave, this,
                   &D3D12PipelineStateViewer::vertex_leave);
  QObject::connect(ui->iaBuffers, &RDTreeWidget::leave, this,
                   &D3D12PipelineStateViewer::vertex_leave);

  QObject::connect(ui->targetOutputs, &RDTreeWidget::itemActivated, this,
                   &D3D12PipelineStateViewer::resource_itemActivated);
  QObject::connect(ui->gsStreamOut, &RDTreeWidget::itemActivated, this,
                   &D3D12PipelineStateViewer::resource_itemActivated);

  for(RDTreeWidget *res : resources)
    QObject::connect(res, &RDTreeWidget::itemActivated, this,
                     &D3D12PipelineStateViewer::resource_itemActivated);

  for(RDTreeWidget *res : uavs)
    QObject::connect(res, &RDTreeWidget::itemActivated, this,
                     &D3D12PipelineStateViewer::resource_itemActivated);

  for(RDTreeWidget *cbuffer : cbuffers)
    QObject::connect(cbuffer, &RDTreeWidget::itemActivated, this,
                     &D3D12PipelineStateViewer::cbuffer_itemActivated);

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

  {
    RDHeaderView *header = new RDHeaderView(Qt::Horizontal, this);
    ui->iaLayouts->setHeader(header);

    ui->iaLayouts->setColumns({tr("Slot"), tr("Semantic"), tr("Index"), tr("Format"),
                               tr("Input Slot"), tr("Offset"), tr("Class"), tr("Step Rate"),
                               tr("Go")});
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
  }

  for(RDTreeWidget *res : resources)
  {
    RDHeaderView *header = new RDHeaderView(Qt::Horizontal, this);
    res->setHeader(header);

    res->setColumns({tr("Root Sig El"), tr("Space"), tr("Register"), tr("Resource"), tr("Type"),
                     tr("Width"), tr("Height"), tr("Depth"), tr("Array Size"), tr("Format"),
                     tr("Go")});
    header->setColumnStretchHints({1, 1, 2, 4, 2, 1, 1, 1, 1, 3, -1});

    res->setHoverIconColumn(10, action, action_hover);
    res->setClearSelectionOnFocusLoss(true);
    res->setInstantTooltips(true);
  }

  for(RDTreeWidget *uav : uavs)
  {
    RDHeaderView *header = new RDHeaderView(Qt::Horizontal, this);
    uav->setHeader(header);

    uav->setColumns({tr("Root Sig El"), tr("Space"), tr("Register"), tr("Resource"), tr("Type"),
                     tr("Width"), tr("Height"), tr("Depth"), tr("Array Size"), tr("Format"),
                     tr("Go")});
    header->setColumnStretchHints({1, 1, 2, 4, 2, 1, 1, 1, 1, 3, -1});

    uav->setHoverIconColumn(10, action, action_hover);
    uav->setClearSelectionOnFocusLoss(true);
    uav->setInstantTooltips(true);
  }

  for(RDTreeWidget *samp : samplers)
  {
    RDHeaderView *header = new RDHeaderView(Qt::Horizontal, this);
    samp->setHeader(header);

    samp->setColumns({tr("Root Sig El"), tr("Space"), tr("Register"), tr("Addressing"),
                      tr("Filter"), tr("LOD Clamp"), tr("LOD Bias")});
    header->setColumnStretchHints({1, 1, 2, 2, 2, 2, 2});

    samp->setClearSelectionOnFocusLoss(true);
    samp->setInstantTooltips(true);
  }

  for(RDTreeWidget *cbuffer : cbuffers)
  {
    RDHeaderView *header = new RDHeaderView(Qt::Horizontal, this);
    cbuffer->setHeader(header);

    cbuffer->setColumns({tr("Root Sig El"), tr("Space"), tr("Register"), tr("Buffer"),
                         tr("Byte Range"), tr("Size"), tr("Go")});
    header->setColumnStretchHints({1, 1, 2, 4, 3, 3, -1});

    cbuffer->setHoverIconColumn(6, action, action_hover);
    cbuffer->setClearSelectionOnFocusLoss(true);
    cbuffer->setInstantTooltips(true);
  }

  {
    RDHeaderView *header = new RDHeaderView(Qt::Horizontal, this);
    ui->gsStreamOut->setHeader(header);

    ui->gsStreamOut->setColumns({tr("Slot"), tr("Buffer"), tr("Byte Offset"), tr("Byte Length"),
                                 tr("Count Buffer"), tr("Count Byte Offset"), tr("Go")});
    header->setColumnStretchHints({1, 4, 2, 3, 4, 2, -1});
    header->setMinimumSectionSize(40);

    ui->gsStreamOut->setHoverIconColumn(6, action, action_hover);
    ui->gsStreamOut->setClearSelectionOnFocusLoss(true);
    ui->gsStreamOut->setInstantTooltips(true);
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

    ui->targetOutputs->setColumns({tr("Slot"), tr("Resource"), tr("Type"), tr("Width"),
                                   tr("Height"), tr("Depth"), tr("Array Size"), tr("Format"),
                                   tr("Go")});
    header->setColumnStretchHints({2, 4, 2, 1, 1, 1, 1, 3, -1});

    ui->targetOutputs->setHoverIconColumn(8, action, action_hover);
    ui->targetOutputs->setClearSelectionOnFocusLoss(true);
    ui->targetOutputs->setInstantTooltips(true);
  }

  {
    RDHeaderView *header = new RDHeaderView(Qt::Horizontal, this);
    ui->blends->setHeader(header);

    ui->blends->setColumns({tr("Slot"), tr("Enabled"), tr("Col Src"), tr("Col Dst"), tr("Col Op"),
                            tr("Alpha Src"), tr("Alpha Dst"), tr("Alpha Op"), tr("Logic Op"),
                            tr("Write Mask")});
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

  // this is often changed just because we're changing some tab in the designer.
  ui->stagesTabs->setCurrentIndex(0);

  ui->stagesTabs->tabBar()->setVisible(false);

  ui->pipeFlow->setStages(
      {
          lit("IA"), lit("VS"), lit("HS"), lit("DS"), lit("GS"), lit("RS"), lit("PS"), lit("OM"),
          lit("CS"),
      },
      {
          tr("Input Assembler"), tr("Vertex Shader"), tr("Hull Shader"), tr("Domain Shader"),
          tr("Geometry Shader"), tr("Rasterizer"), tr("Pixel Shader"), tr("Output Merger"),
          tr("Compute Shader"),
      });

  ui->pipeFlow->setIsolatedStage(8);    // compute shader isolated

  ui->pipeFlow->setStagesEnabled({true, true, true, true, true, true, true, true, true});

  m_Common.setMeshViewPixmap(ui->meshView);

  ui->iaLayouts->setFont(Formatter::PreferredFont());
  ui->iaBuffers->setFont(Formatter::PreferredFont());
  ui->gsStreamOut->setFont(Formatter::PreferredFont());
  ui->groupX->setFont(Formatter::PreferredFont());
  ui->groupY->setFont(Formatter::PreferredFont());
  ui->groupZ->setFont(Formatter::PreferredFont());
  ui->threadX->setFont(Formatter::PreferredFont());
  ui->threadY->setFont(Formatter::PreferredFont());
  ui->threadZ->setFont(Formatter::PreferredFont());
  ui->vsShader->setFont(Formatter::PreferredFont());
  ui->vsResources->setFont(Formatter::PreferredFont());
  ui->vsSamplers->setFont(Formatter::PreferredFont());
  ui->vsCBuffers->setFont(Formatter::PreferredFont());
  ui->vsUAVs->setFont(Formatter::PreferredFont());
  ui->gsShader->setFont(Formatter::PreferredFont());
  ui->gsResources->setFont(Formatter::PreferredFont());
  ui->gsSamplers->setFont(Formatter::PreferredFont());
  ui->gsCBuffers->setFont(Formatter::PreferredFont());
  ui->gsUAVs->setFont(Formatter::PreferredFont());
  ui->hsShader->setFont(Formatter::PreferredFont());
  ui->hsResources->setFont(Formatter::PreferredFont());
  ui->hsSamplers->setFont(Formatter::PreferredFont());
  ui->hsCBuffers->setFont(Formatter::PreferredFont());
  ui->hsUAVs->setFont(Formatter::PreferredFont());
  ui->dsShader->setFont(Formatter::PreferredFont());
  ui->dsResources->setFont(Formatter::PreferredFont());
  ui->dsSamplers->setFont(Formatter::PreferredFont());
  ui->dsCBuffers->setFont(Formatter::PreferredFont());
  ui->dsUAVs->setFont(Formatter::PreferredFont());
  ui->psShader->setFont(Formatter::PreferredFont());
  ui->psResources->setFont(Formatter::PreferredFont());
  ui->psSamplers->setFont(Formatter::PreferredFont());
  ui->psCBuffers->setFont(Formatter::PreferredFont());
  ui->psUAVs->setFont(Formatter::PreferredFont());
  ui->csShader->setFont(Formatter::PreferredFont());
  ui->csResources->setFont(Formatter::PreferredFont());
  ui->csSamplers->setFont(Formatter::PreferredFont());
  ui->csCBuffers->setFont(Formatter::PreferredFont());
  ui->csUAVs->setFont(Formatter::PreferredFont());
  ui->viewports->setFont(Formatter::PreferredFont());
  ui->scissors->setFont(Formatter::PreferredFont());
  ui->targetOutputs->setFont(Formatter::PreferredFont());
  ui->blends->setFont(Formatter::PreferredFont());

  // reset everything back to defaults
  clearState();
}

D3D12PipelineStateViewer::~D3D12PipelineStateViewer()
{
  delete ui;
}

void D3D12PipelineStateViewer::OnCaptureLoaded()
{
  OnEventChanged(m_Ctx.CurEvent());
}

void D3D12PipelineStateViewer::OnCaptureClosed()
{
  ui->pipeFlow->setStagesEnabled({true, true, true, true, true, true, true, true, true});

  clearState();
}

void D3D12PipelineStateViewer::OnEventChanged(uint32_t eventId)
{
  setState();
}

void D3D12PipelineStateViewer::on_showUnused_toggled(bool checked)
{
  setState();
}

void D3D12PipelineStateViewer::on_showEmpty_toggled(bool checked)
{
  setState();
}
void D3D12PipelineStateViewer::setInactiveRow(RDTreeWidgetItem *node)
{
  node->setItalic(true);
}
void D3D12PipelineStateViewer::setEmptyRow(RDTreeWidgetItem *node)
{
  node->setBackgroundColor(QColor(255, 70, 70));
  node->setForegroundColor(QColor(0, 0, 0));
}

void D3D12PipelineStateViewer::setViewDetails(RDTreeWidgetItem *node, const D3D12ViewTag &view,
                                              TextureDescription *tex)
{
  if(tex == NULL)
    return;

  QString text;

  const D3D12Pipe::View &res = view.res;

  bool viewdetails = false;

  for(const D3D12Pipe::ResourceData &im : m_Ctx.CurD3D12PipelineState()->resourceStates)
  {
    if(im.resourceId == tex->resourceId)
    {
      text += tr("Texture is in the '%1' state\n\n").arg(im.states[0].name);
      break;
    }
  }

  if(res.viewFormat.compType != CompType::Typeless && res.viewFormat != tex->format)
  {
    text += tr("The texture is format %1, the view treats it as %2.\n")
                .arg(tex->format.Name())
                .arg(res.viewFormat.Name());

    viewdetails = true;
  }

  if(view.space == D3D12ViewTag::OMDepth)
  {
    if(m_Ctx.CurD3D12PipelineState()->outputMerger.depthReadOnly)
      text += tr("Depth component is read-only\n");
    if(m_Ctx.CurD3D12PipelineState()->outputMerger.stencilReadOnly)
      text += tr("Stencil component is read-only\n");
  }

  if(tex->mips > 1 && (tex->mips != res.numMips || res.firstMip > 0))
  {
    if(res.numMips == 1)
      text +=
          tr("The texture has %1 mips, the view covers mip %2.\n").arg(tex->mips).arg(res.firstMip);
    else
      text += tr("The texture has %1 mips, the view covers mips %2-%3.\n")
                  .arg(tex->mips)
                  .arg(res.firstMip)
                  .arg(res.firstMip + res.numMips - 1);

    viewdetails = true;
  }

  if(tex->arraysize > 1 && (tex->arraysize != res.numSlices || res.firstSlice > 0))
  {
    if(res.numSlices == 1)
      text += tr("The texture has %1 array slices, the view covers slice %2.\n")
                  .arg(tex->arraysize)
                  .arg(res.firstSlice);
    else
      text += tr("The texture has %1 array slices, the view covers slices %2-%3.\n")
                  .arg(tex->arraysize)
                  .arg(res.firstSlice)
                  .arg(res.firstSlice + res.numSlices);

    viewdetails = true;
  }

  if(view.res.minLODClamp != 0.0f)
  {
    text += tr("The texture has a ResourceMinLODClamp of %1.\n").arg(view.res.minLODClamp);

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

void D3D12PipelineStateViewer::setViewDetails(RDTreeWidgetItem *node, const D3D12ViewTag &view,
                                              BufferDescription *buf)
{
  if(buf == NULL)
    return;

  QString text;

  const D3D12Pipe::View &res = view.res;

  for(const D3D12Pipe::ResourceData &im : m_Ctx.CurD3D12PipelineState()->resourceStates)
  {
    if(im.resourceId == buf->resourceId)
    {
      text += tr("Buffer is in the '%1' state\n\n").arg(im.states[0].name);
      break;
    }
  }

  bool viewdetails = false;

  if(res.firstElement > 0 || (res.numElements * res.elementByteSize) < buf->length)
  {
    text += tr("The view covers bytes %1-%2 (%3 elements).\nThe buffer is %4 bytes in length (%5 "
               "elements).")
                .arg(res.firstElement * res.elementByteSize)
                .arg((res.firstElement + res.numElements) * res.elementByteSize)
                .arg(res.numElements)
                .arg(buf->length)
                .arg(buf->length / res.elementByteSize);

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

void D3D12PipelineStateViewer::addResourceRow(const D3D12ViewTag &view,
                                              const D3D12Pipe::Shader *stage, RDTreeWidget *resources)
{
  const D3D12Pipe::View &r = view.res;
  bool uav = view.type == D3D12ViewTag::UAV;

  const Bindpoint *bind = NULL;
  const ShaderResource *shaderInput = NULL;

  if(stage && stage->reflection)
  {
    const rdcarray<Bindpoint> &binds = uav ? stage->bindpointMapping.readWriteResources
                                           : stage->bindpointMapping.readOnlyResources;
    const rdcarray<ShaderResource> &res =
        uav ? stage->reflection->readWriteResources : stage->reflection->readOnlyResources;
    for(int i = 0; i < binds.count(); i++)
    {
      const Bindpoint &b = binds[i];

      bool regMatch = (b.bind == (int)view.res.bind);

      // handle unbounded arrays specially. It's illegal to have an unbounded array with
      // anything after it
      if(b.bind <= (int)view.res.bind)
        regMatch = (b.arraySize == ~0U) || (b.bind + (int)b.arraySize > (int)view.res.bind);

      if(b.bindset == view.space && regMatch)
      {
        bind = &b;
        shaderInput = &res[i];
        break;
      }
    }
  }

  bool filledSlot = (r.resourceId != ResourceId());
  bool usedSlot = (bind && bind->used);

  // if a target is set to RTVs or DSV, it is implicitly used
  if(filledSlot)
    usedSlot = usedSlot || view.type == D3D12ViewTag::OMTarget || view.type == D3D12ViewTag::OMDepth;

  if(showNode(usedSlot, filledSlot))
  {
    QString regname = QString::number(view.res.bind);

    if(shaderInput && !shaderInput->name.empty())
      regname += lit(": ") + shaderInput->name;

    if(view.type == D3D12ViewTag::OMDepth)
      regname = tr("Depth");

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

    TextureDescription *tex = m_Ctx.GetTexture(r.resourceId);

    if(tex)
    {
      w = tex->width;
      h = tex->height;
      d = tex->depth;
      a = tex->arraysize;
      format = tex->format.Name();
      typeName = ToQStr(tex->type);

      if(r.swizzle.red != TextureSwizzle::Red || r.swizzle.green != TextureSwizzle::Green ||
         r.swizzle.blue != TextureSwizzle::Blue || r.swizzle.alpha != TextureSwizzle::Alpha)
      {
        format += tr(" swizzle[%1%2%3%4]")
                      .arg(ToQStr(r.swizzle.red))
                      .arg(ToQStr(r.swizzle.green))
                      .arg(ToQStr(r.swizzle.blue))
                      .arg(ToQStr(r.swizzle.alpha));
      }

      if(tex->type == TextureType::Texture2DMS || tex->type == TextureType::Texture2DMSArray)
      {
        typeName += QFormatStr(" %1x").arg(tex->msSamp);
      }

      if(tex->format != r.viewFormat)
        format = tr("Viewed as %1").arg(r.viewFormat.Name());
    }

    BufferDescription *buf = m_Ctx.GetBuffer(r.resourceId);

    if(buf)
    {
      w = buf->length;
      h = 0;
      d = 0;
      a = 0;
      format = QString();
      typeName = lit("Buffer");

      if(r.bufferFlags & D3DBufferViewFlags::Raw)
      {
        typeName = QFormatStr("%1ByteAddressBuffer").arg(uav ? lit("RW") : QString());
      }
      else if(r.elementByteSize > 0)
      {
        // for structured buffers, display how many 'elements' there are in the buffer
        a = buf->length / r.elementByteSize;
        typeName = QFormatStr("%1StructuredBuffer[%2]").arg(uav ? lit("RW") : QString()).arg(a);
      }

      if(r.counterResourceId != ResourceId())
      {
        typeName += tr(" (Counter %1: %2)").arg(ToQStr(r.counterResourceId)).arg(r.bufferStructCount);
      }

      // get the buffer type, whether it's just a basic type or a complex struct
      if(shaderInput && !shaderInput->isTexture)
      {
        if(!shaderInput->variableType.members.empty())
          format = lit("struct ") + shaderInput->variableType.descriptor.name;
        else if(r.viewFormat.compType == CompType::Typeless)
          format = shaderInput->variableType.descriptor.name;
        else
          format = r.viewFormat.Name();
      }
    }

    RDTreeWidgetItem *node = NULL;

    if(view.type == D3D12ViewTag::OMTarget)
    {
      node = new RDTreeWidgetItem(
          {view.res.bind, r.resourceId, typeName, w, h, d, a, format, QString()});
    }
    else if(view.type == D3D12ViewTag::OMDepth)
    {
      node =
          new RDTreeWidgetItem({tr("Depth"), r.resourceId, typeName, w, h, d, a, format, QString()});
    }
    else
    {
      QString rootel = view.immediate ? tr("#%1 Direct").arg(view.rootElement)
                                      : tr("#%1 Table[%2]").arg(view.rootElement).arg(r.tableIndex);

      node = new RDTreeWidgetItem(
          {rootel, view.space, regname, r.resourceId, typeName, w, h, d, a, format, QString()});
    }

    node->setTag(QVariant::fromValue(view));

    if(tex)
      setViewDetails(node, view, tex);
    else if(buf)
      setViewDetails(node, view, buf);

    if(!filledSlot)
      setEmptyRow(node);

    if(!usedSlot)
      setInactiveRow(node);

    resources->addTopLevelItem(node);
  }
}

bool D3D12PipelineStateViewer::showNode(bool usedSlot, bool filledSlot)
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

const D3D12Pipe::Shader *D3D12PipelineStateViewer::stageForSender(QWidget *widget)
{
  if(!m_Ctx.IsCaptureLoaded())
    return NULL;

  while(widget)
  {
    if(widget == ui->stagesTabs->widget(0))
      return &m_Ctx.CurD3D12PipelineState()->vertexShader;
    if(widget == ui->stagesTabs->widget(1))
      return &m_Ctx.CurD3D12PipelineState()->vertexShader;
    if(widget == ui->stagesTabs->widget(2))
      return &m_Ctx.CurD3D12PipelineState()->hullShader;
    if(widget == ui->stagesTabs->widget(3))
      return &m_Ctx.CurD3D12PipelineState()->domainShader;
    if(widget == ui->stagesTabs->widget(4))
      return &m_Ctx.CurD3D12PipelineState()->geometryShader;
    if(widget == ui->stagesTabs->widget(5))
      return &m_Ctx.CurD3D12PipelineState()->pixelShader;
    if(widget == ui->stagesTabs->widget(6))
      return &m_Ctx.CurD3D12PipelineState()->pixelShader;
    if(widget == ui->stagesTabs->widget(7))
      return &m_Ctx.CurD3D12PipelineState()->pixelShader;
    if(widget == ui->stagesTabs->widget(8))
      return &m_Ctx.CurD3D12PipelineState()->computeShader;

    widget = widget->parentWidget();
  }

  qCritical() << "Unrecognised control calling event handler";

  return NULL;
}

void D3D12PipelineStateViewer::clearShaderState(RDLabel *shader, RDLabel *rootSig,
                                                RDTreeWidget *tex, RDTreeWidget *samp,
                                                RDTreeWidget *cbuffer, RDTreeWidget *sub)
{
  rootSig->setText(ToQStr(ResourceId()));
  shader->setText(ToQStr(ResourceId()));
  tex->clear();
  samp->clear();
  sub->clear();
  cbuffer->clear();
}

void D3D12PipelineStateViewer::clearState()
{
  m_VBNodes.clear();
  m_EmptyNodes.clear();

  ui->iaLayouts->clear();
  ui->iaBuffers->clear();
  ui->topology->setText(QString());
  ui->topologyDiagram->setPixmap(QPixmap());

  clearShaderState(ui->vsShader, ui->vsRootSig, ui->vsResources, ui->vsSamplers, ui->vsCBuffers,
                   ui->vsUAVs);
  clearShaderState(ui->gsShader, ui->gsRootSig, ui->gsResources, ui->gsSamplers, ui->gsCBuffers,
                   ui->gsUAVs);
  clearShaderState(ui->hsShader, ui->hsRootSig, ui->hsResources, ui->hsSamplers, ui->hsCBuffers,
                   ui->hsUAVs);
  clearShaderState(ui->dsShader, ui->dsRootSig, ui->dsResources, ui->dsSamplers, ui->dsCBuffers,
                   ui->dsUAVs);
  clearShaderState(ui->psShader, ui->psRootSig, ui->psResources, ui->psSamplers, ui->psCBuffers,
                   ui->psUAVs);
  clearShaderState(ui->csShader, ui->csRootSig, ui->csResources, ui->csSamplers, ui->csCBuffers,
                   ui->csUAVs);

  ui->gsStreamOut->clear();

  QToolButton *shaderButtons[] = {
      ui->vsShaderViewButton, ui->hsShaderViewButton, ui->dsShaderViewButton,
      ui->gsShaderViewButton, ui->psShaderViewButton, ui->csShaderViewButton,
      ui->vsShaderEditButton, ui->hsShaderEditButton, ui->dsShaderEditButton,
      ui->gsShaderEditButton, ui->psShaderEditButton, ui->csShaderEditButton,
      ui->vsShaderSaveButton, ui->hsShaderSaveButton, ui->dsShaderSaveButton,
      ui->gsShaderSaveButton, ui->psShaderSaveButton, ui->csShaderSaveButton,
  };

  for(QToolButton *b : shaderButtons)
    b->setEnabled(false);

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
  ui->multisample->setPixmap(tick);
  ui->lineAA->setPixmap(tick);
  ui->sampleMask->setText(lit("FFFFFFFF"));

  ui->independentBlend->setPixmap(cross);
  ui->alphaToCoverage->setPixmap(tick);

  ui->blendFactor->setText(lit("0.00, 0.00, 0.00, 0.00"));

  ui->viewports->clear();
  ui->scissors->clear();

  ui->targetOutputs->clear();
  ui->blends->clear();

  ui->depthEnabled->setPixmap(tick);
  ui->depthFunc->setText(lit("GREATER_EQUAL"));
  ui->depthWrite->setPixmap(tick);

  ui->depthBounds->setPixmap(QPixmap());
  ui->depthBounds->setText(lit("0.0-1.0"));

  ui->stencilEnabled->setPixmap(cross);
  ui->stencilReadMask->setText(lit("FF"));
  ui->stencilWriteMask->setText(lit("FF"));
  ui->stencilRef->setText(lit("FF"));

  ui->stencils->clear();

  {
    ui->groupX->setEnabled(false);
    ui->groupY->setEnabled(false);
    ui->groupZ->setEnabled(false);

    ui->threadX->setEnabled(false);
    ui->threadY->setEnabled(false);
    ui->threadZ->setEnabled(false);

    ui->debugThread->setEnabled(false);
  }
}

void D3D12PipelineStateViewer::setShaderState(
    const rdcarray<D3D12Pipe::RootSignatureRange> &rootElements, const D3D12Pipe::Shader &stage,
    RDLabel *shader, RDLabel *rootSig, RDTreeWidget *resources, RDTreeWidget *samplers,
    RDTreeWidget *cbuffers, RDTreeWidget *uavs)
{
  ShaderReflection *shaderDetails = stage.reflection;
  const D3D12Pipe::State &state = *m_Ctx.CurD3D12PipelineState();

  rootSig->setText(ToQStr(state.rootSignatureResourceId));

  QString shText = ToQStr(stage.resourceId);

  if(stage.resourceId != ResourceId())
    shText = tr("%1 - %2 Shader")
                 .arg(ToQStr(state.pipelineResourceId))
                 .arg(ToQStr(stage.stage, GraphicsAPI::D3D12));

  if(shaderDetails && !shaderDetails->debugInfo.files.empty())
  {
    shText += QFormatStr(": %1() - %2")
                  .arg(shaderDetails->entryPoint)
                  .arg(QFileInfo(shaderDetails->debugInfo.files[0].filename).fileName());
  }
  shader->setText(shText);

  int resVs = resources->verticalScrollBar()->value();
  resources->beginUpdate();
  resources->clear();
  int uavVs = uavs->verticalScrollBar()->value();
  uavs->beginUpdate();
  uavs->clear();
  int sampVs = samplers->verticalScrollBar()->value();
  samplers->beginUpdate();
  samplers->clear();
  int cbVs = cbuffers->verticalScrollBar()->value();
  cbuffers->beginUpdate();
  cbuffers->clear();

  for(size_t i = 0; i < rootElements.size(); ++i)
  {
    if((rootElements[i].visibility & MaskForStage(stage.stage)) == ShaderStageMask::Unknown)
      continue;

    switch(rootElements[i].type)
    {
      case BindType::ReadOnlyResource:
      {
        for(size_t j = 0; j < rootElements[i].views.size(); ++j)
        {
          addResourceRow(D3D12ViewTag(D3D12ViewTag::SRV, rootElements[i].registerSpace, (int)i,
                                      rootElements[i].immediate, rootElements[i].views[j]),
                         &stage, resources);
        }
        break;
      }
      case BindType::ReadWriteResource:
      {
        for(size_t j = 0; j < rootElements[i].views.size(); ++j)
        {
          addResourceRow(D3D12ViewTag(D3D12ViewTag::UAV, rootElements[i].registerSpace, (int)i,
                                      rootElements[i].immediate, rootElements[i].views[j]),
                         &stage, uavs);
        }
        break;
      }
      case BindType::Sampler:
      {
        for(size_t j = 0; j < rootElements[i].samplers.size(); ++j)
        {
          const D3D12Pipe::Sampler &s = rootElements[i].samplers[j];

          const Bindpoint *bind = NULL;
          const ShaderSampler *shaderInput = NULL;

          if(stage.reflection)
          {
            for(int k = 0; k < stage.bindpointMapping.samplers.count(); ++k)
            {
              const Bindpoint &b = stage.bindpointMapping.samplers[k];
              const ShaderSampler &res = stage.reflection->samplers[k];

              bool regMatch = (b.bind == (int)s.bind);

              // handle unbounded arrays specially. It's illegal to have an unbounded array with
              // anything after it
              if(b.bind <= (int)s.bind)
                regMatch = (b.arraySize == ~0U) || (b.bind + (int)b.arraySize > (int)s.bind);

              if(b.bindset == (int)rootElements[i].registerSpace && regMatch)
              {
                bind = &b;
                shaderInput = &res;
                break;
              }
            }
          }

          QString rootel =
              rootElements[i].immediate
                  ? tr("#%1 Static").arg(rootElements[i].rootElement)
                  : tr("#%1 Table[%2]").arg(rootElements[i].rootElement).arg(s.tableIndex);

          bool filledSlot = s.filter.minify != FilterMode::NoFilter;
          bool usedSlot = (bind && bind->used);

          if(showNode(usedSlot, filledSlot))
          {
            QString regname = QString::number(s.bind);

            if(shaderInput && !shaderInput->name.empty())
              regname += lit(": ") + shaderInput->name;

            QString borderColor = QFormatStr("%1, %2, %3, %4")
                                      .arg(s.borderColor[0])
                                      .arg(s.borderColor[1])
                                      .arg(s.borderColor[2])
                                      .arg(s.borderColor[3]);

            QString addressing;

            QString addPrefix;
            QString addVal;

            QString addr[] = {ToQStr(s.addressU, GraphicsAPI::D3D12),
                              ToQStr(s.addressV, GraphicsAPI::D3D12),
                              ToQStr(s.addressW, GraphicsAPI::D3D12)};

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

            if(s.UseBorder())
              addressing += QFormatStr("<%1>").arg(borderColor);

            QString filter = ToQStr(s.filter);

            if(s.maxAnisotropy > 1)
              filter += QFormatStr(" %1x").arg(s.maxAnisotropy);

            if(s.filter.filter == FilterFunction::Comparison)
              filter += QFormatStr(" (%1)").arg(ToQStr(s.compareFunction));
            else if(s.filter.filter != FilterFunction::Normal)
              filter += QFormatStr(" (%1)").arg(ToQStr(s.filter.filter));

            RDTreeWidgetItem *node = new RDTreeWidgetItem(
                {rootel, rootElements[i].registerSpace, regname, addressing, filter,
                 QFormatStr("%1 - %2")
                     .arg(s.minLOD == -FLT_MAX ? lit("0") : QString::number(s.minLOD))
                     .arg(s.maxLOD == FLT_MAX ? lit("FLT_MAX") : QString::number(s.maxLOD)),
                 s.mipLODBias});

            if(!filledSlot)
              setEmptyRow(node);

            if(!usedSlot)
              setInactiveRow(node);

            samplers->addTopLevelItem(node);
          }
        }
        break;
      }
      case BindType::ConstantBuffer:
      {
        for(size_t j = 0; j < rootElements[i].constantBuffers.size(); ++j)
        {
          const D3D12Pipe::ConstantBuffer &b = rootElements[i].constantBuffers[j];

          QVariant tag;

          const Bindpoint *bind = NULL;
          const ConstantBlock *shaderCBuf = NULL;

          if(stage.reflection)
          {
            for(int k = 0; k < stage.bindpointMapping.constantBlocks.count(); ++k)
            {
              const Bindpoint &bm = stage.bindpointMapping.constantBlocks[k];
              const ConstantBlock &res = stage.reflection->constantBlocks[k];

              bool regMatch = bm.bind == (int)b.bind;

              // handle unbounded arrays specially. It's illegal to have an unbounded array with
              // anything after it
              if(bm.bind <= (int)b.bind)
                regMatch = (bm.arraySize == ~0U) || (bm.bind + (int)bm.arraySize > (int)b.bind);

              if(bm.bindset == (int)rootElements[i].registerSpace && regMatch)
              {
                bind = &bm;
                shaderCBuf = &res;
                D3D12CBufTag cbufTag((uint32_t)k);
                cbufTag.arrayIdx = b.bind - bm.bind;
                tag = QVariant::fromValue(cbufTag);
                break;
              }
            }
          }

          if(!tag.isValid())
            tag = QVariant::fromValue(
                D3D12CBufTag(rootElements[i].registerSpace, b.bind, rootElements[i].rootElement));

          QString rootel;

          if(rootElements[i].immediate)
          {
            if(!b.rootValues.empty())
              rootel = tr("#%1 Consts").arg(rootElements[i].rootElement);
            else
              rootel = tr("#%1 Direct").arg(rootElements[i].rootElement);
          }
          else
          {
            rootel = tr("#%1 Table[%2]").arg(rootElements[i].rootElement).arg(b.tableIndex);
          }

          bool filledSlot = (b.resourceId != ResourceId());
          if(rootElements[i].immediate && !b.rootValues.empty())
            filledSlot = true;

          bool usedSlot = (bind && bind->used);

          if(showNode(usedSlot, filledSlot))
          {
            ulong length = b.byteSize;
            uint64_t offset = b.byteOffset;
            int numvars = shaderCBuf ? shaderCBuf->variables.count() : 0;
            uint32_t bytesize = shaderCBuf ? shaderCBuf->byteSize : 0;

            if(rootElements[i].immediate && !b.rootValues.empty())
              bytesize = uint32_t(b.rootValues.count() * 4);

            QString regname = QString::number(b.bind);

            if(shaderCBuf && !shaderCBuf->name.empty())
              regname += lit(": ") + shaderCBuf->name;

            if(bind != NULL && bind->arraySize > 1)
              regname += tr("[%1]").arg(b.bind - bind->bind);

            QString sizestr;
            if(bytesize == (uint32_t)length)
              sizestr = tr("%1 Variables, %2 bytes").arg(numvars).arg(length);
            else
              sizestr =
                  tr("%1 Variables, %2 bytes needed, %3 provided").arg(numvars).arg(bytesize).arg(length);

            if(length < bytesize)
              filledSlot = false;

            RDTreeWidgetItem *node = new RDTreeWidgetItem(
                {rootel, (qulonglong)rootElements[i].registerSpace, regname, b.resourceId,
                 QFormatStr("%1 - %2").arg(offset).arg(offset + bytesize), sizestr, QString()});

            node->setTag(tag);

            if(!filledSlot)
              setEmptyRow(node);

            if(!usedSlot)
              setInactiveRow(node);

            cbuffers->addTopLevelItem(node);
          }
        }
        break;
      }
      default: qCritical() << "Unexpected BindType for D3D12 pipeline"; break;
    }
  }

  resources->clearSelection();
  resources->endUpdate();
  resources->verticalScrollBar()->setValue(resVs);
  uavs->clearSelection();
  uavs->endUpdate();
  uavs->verticalScrollBar()->setValue(uavVs);
  samplers->clearSelection();
  samplers->endUpdate();
  samplers->verticalScrollBar()->setValue(sampVs);
  cbuffers->clearSelection();
  cbuffers->endUpdate();
  cbuffers->verticalScrollBar()->setValue(cbVs);
}

void D3D12PipelineStateViewer::setState()
{
  if(!m_Ctx.IsCaptureLoaded())
  {
    clearState();
    return;
  }

  const D3D12Pipe::State &state = *m_Ctx.CurD3D12PipelineState();
  const DrawcallDescription *draw = m_Ctx.CurDrawcall();

  const QPixmap &tick = Pixmaps::tick(this);
  const QPixmap &cross = Pixmaps::cross(this);

  ////////////////////////////////////////////////
  // Vertex Input

  int vs = 0;

  bool usedVBuffers[128] = {};
  uint32_t layoutOffs[128] = {};

  vs = ui->iaLayouts->verticalScrollBar()->value();
  ui->iaLayouts->beginUpdate();
  ui->iaLayouts->clear();
  {
    int i = 0;
    for(const D3D12Pipe::Layout &l : state.inputAssembly.layouts)
    {
      QString byteOffs = QString::number(l.byteOffset);

      // D3D12 specific value
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

      for(int ia = 0; state.vertexShader.reflection &&
                      ia < state.vertexShader.reflection->inputSignature.count();
          ia++)
      {
        if(!QString(state.vertexShader.reflection->inputSignature[ia].semanticName)
                .compare(l.semanticName, Qt::CaseInsensitive) &&
           state.vertexShader.reflection->inputSignature[ia].semanticIndex == l.semanticIndex)
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

  m_VBNodes.clear();
  m_EmptyNodes.clear();

  vs = ui->iaBuffers->verticalScrollBar()->value();
  ui->iaBuffers->beginUpdate();
  ui->iaBuffers->clear();

  if(state.inputAssembly.indexBuffer.resourceId != ResourceId())
  {
    if(ibufferUsed || ui->showUnused->isChecked())
    {
      uint64_t length = state.inputAssembly.indexBuffer.byteSize;

      BufferDescription *buf = m_Ctx.GetBuffer(state.inputAssembly.indexBuffer.resourceId);

      RDTreeWidgetItem *node = new RDTreeWidgetItem(
          {tr("Index"), state.inputAssembly.indexBuffer.resourceId, draw ? draw->indexByteWidth : 0,
           (qulonglong)state.inputAssembly.indexBuffer.byteOffset, (qulonglong)length, QString()});

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

      uint32_t drawOffset = (draw ? draw->indexOffset * draw->indexByteWidth : 0);

      node->setTag(QVariant::fromValue(
          D3D12VBIBTag(state.inputAssembly.indexBuffer.resourceId,
                       state.inputAssembly.indexBuffer.byteOffset + drawOffset,
                       drawOffset > state.inputAssembly.indexBuffer.byteSize
                           ? 0
                           : state.inputAssembly.indexBuffer.byteSize - drawOffset,
                       iformat)));

      for(const D3D12Pipe::ResourceData &res : m_Ctx.CurD3D12PipelineState()->resourceStates)
      {
        if(res.resourceId == state.inputAssembly.indexBuffer.resourceId)
        {
          node->setToolTip(tr("Buffer is in the '%1' state").arg(res.states[0].name));
          break;
        }
      }

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

      uint32_t drawOffset = (draw ? draw->indexOffset * draw->indexByteWidth : 0);

      node->setTag(QVariant::fromValue(
          D3D12VBIBTag(state.inputAssembly.indexBuffer.resourceId,
                       state.inputAssembly.indexBuffer.byteOffset + drawOffset,
                       drawOffset > state.inputAssembly.indexBuffer.byteSize
                           ? 0
                           : state.inputAssembly.indexBuffer.byteSize - drawOffset,
                       iformat)));

      for(const D3D12Pipe::ResourceData &res : m_Ctx.CurD3D12PipelineState()->resourceStates)
      {
        if(res.resourceId == state.inputAssembly.indexBuffer.resourceId)
        {
          node->setToolTip(tr("Buffer is in the '%1' state").arg(res.states[0].name));
          break;
        }
      }

      setEmptyRow(node);
      m_EmptyNodes.push_back(node);

      if(!ibufferUsed)
        setInactiveRow(node);

      ui->iaBuffers->addTopLevelItem(node);
    }
  }

  for(int i = 0; i < 128; i++)
  {
    if(i >= state.inputAssembly.vertexBuffers.count())
    {
      // for vbuffers that are referenced but not bound, make sure we add an empty row
      if(usedVBuffers[i])
      {
        RDTreeWidgetItem *node =
            new RDTreeWidgetItem({i, tr("No Buffer Set"), lit("-"), lit("-"), lit("-"), QString()});
        node->setTag(QVariant::fromValue(D3D12VBIBTag(ResourceId(), 0, 0)));

        setEmptyRow(node);
        m_EmptyNodes.push_back(node);

        m_VBNodes.push_back(node);

        ui->iaBuffers->addTopLevelItem(node);
      }
      else
      {
        m_VBNodes.push_back(NULL);
      }

      continue;
    }

    const D3D12Pipe::VertexBuffer &v = state.inputAssembly.vertexBuffers[i];

    bool filledSlot = (v.resourceId != ResourceId());
    bool usedSlot = (usedVBuffers[i]);

    if(showNode(usedSlot, filledSlot))
    {
      qulonglong length = v.byteSize;

      BufferDescription *buf = m_Ctx.GetBuffer(v.resourceId);

      RDTreeWidgetItem *node = NULL;

      if(filledSlot)
        node = new RDTreeWidgetItem(
            {i, v.resourceId, v.byteStride, (qulonglong)v.byteOffset, length, QString()});
      else
        node =
            new RDTreeWidgetItem({i, tr("No Buffer Set"), lit("-"), lit("-"), lit("-"), QString()});

      node->setTag(QVariant::fromValue(D3D12VBIBTag(v.resourceId, v.byteOffset, v.byteSize,
                                                    m_Common.GetVBufferFormatString(i))));

      for(const D3D12Pipe::ResourceData &res : m_Ctx.CurD3D12PipelineState()->resourceStates)
      {
        if(res.resourceId == v.resourceId)
        {
          node->setToolTip(tr("Buffer is in the '%1' state").arg(res.states[0].name));
          break;
        }
      }

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

  setShaderState(state.rootElements, state.vertexShader, ui->vsShader, ui->vsRootSig,
                 ui->vsResources, ui->vsSamplers, ui->vsCBuffers, ui->vsUAVs);
  setShaderState(state.rootElements, state.geometryShader, ui->gsShader, ui->gsRootSig,
                 ui->gsResources, ui->gsSamplers, ui->gsCBuffers, ui->gsUAVs);
  setShaderState(state.rootElements, state.hullShader, ui->hsShader, ui->hsRootSig, ui->hsResources,
                 ui->hsSamplers, ui->hsCBuffers, ui->hsUAVs);
  setShaderState(state.rootElements, state.domainShader, ui->dsShader, ui->dsRootSig,
                 ui->dsResources, ui->dsSamplers, ui->dsCBuffers, ui->dsUAVs);
  setShaderState(state.rootElements, state.pixelShader, ui->psShader, ui->psRootSig,
                 ui->psResources, ui->psSamplers, ui->psCBuffers, ui->psUAVs);
  setShaderState(state.rootElements, state.computeShader, ui->csShader, ui->csRootSig,
                 ui->csResources, ui->csSamplers, ui->csCBuffers, ui->csUAVs);

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
    const D3D12Pipe::Shader *stage = stageForSender(b);

    if(stage == NULL || stage->resourceId == ResourceId())
      continue;

    b->setEnabled(stage->reflection && state.pipelineResourceId != ResourceId());

    m_Common.SetupShaderEditButton(b, state.pipelineResourceId, stage->resourceId,
                                   stage->bindpointMapping, stage->reflection);
  }

  bool streamoutSet = false;
  vs = ui->gsStreamOut->verticalScrollBar()->value();
  ui->gsStreamOut->beginUpdate();
  ui->gsStreamOut->clear();
  for(int i = 0; i < state.streamOut.outputs.count(); i++)
  {
    const D3D12Pipe::StreamOutBind &s = state.streamOut.outputs[i];

    bool filledSlot = (s.resourceId != ResourceId());
    bool usedSlot = (filledSlot);

    if(showNode(usedSlot, filledSlot))
    {
      qulonglong length = s.byteSize;

      BufferDescription *buf = m_Ctx.GetBuffer(s.resourceId);

      RDTreeWidgetItem *node = new RDTreeWidgetItem(
          {i, s.resourceId, (qulonglong)s.byteOffset, length, s.writtenCountResourceId,
           (qulonglong)s.writtenCountByteOffset, QString()});

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

    RDTreeWidgetItem *node =
        new RDTreeWidgetItem({i, v.x, v.y, v.width, v.height, v.minDepth, v.maxDepth});

    if(v.width == 0 || v.height == 0 || v.minDepth == v.maxDepth)
      setEmptyRow(node);

    ui->viewports->addTopLevelItem(node);
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

    RDTreeWidgetItem *node = new RDTreeWidgetItem({i, s.x, s.y, s.width, s.height});

    if(s.width == 0 || s.height == 0)
      setEmptyRow(node);

    ui->scissors->addTopLevelItem(node);
  }
  ui->scissors->clearSelection();
  ui->scissors->verticalScrollBar()->setValue(vs);
  ui->scissors->endUpdate();

  ui->fillMode->setText(ToQStr(state.rasterizer.state.fillMode));
  ui->cullMode->setText(ToQStr(state.rasterizer.state.cullMode));
  ui->frontCCW->setPixmap(state.rasterizer.state.frontCCW ? tick : cross);

  ui->lineAA->setPixmap(state.rasterizer.state.antialiasedLines ? tick : cross);
  ui->sampleMask->setText(Formatter::Format(state.rasterizer.sampleMask, true));
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
  // Output Merger

  bool targets[32] = {};

  vs = ui->targetOutputs->verticalScrollBar()->value();
  ui->targetOutputs->beginUpdate();
  ui->targetOutputs->clear();
  {
    for(int i = 0; i < state.outputMerger.renderTargets.count(); i++)
    {
      addResourceRow(
          D3D12ViewTag(D3D12ViewTag::OMTarget, 0, 0, false, state.outputMerger.renderTargets[i]),
          NULL, ui->targetOutputs);

      if(state.outputMerger.renderTargets[i].resourceId != ResourceId())
        targets[i] = true;
    }

    addResourceRow(D3D12ViewTag(D3D12ViewTag::OMDepth, 0, 0, false, state.outputMerger.depthTarget),
                   NULL, ui->targetOutputs);
  }
  ui->targetOutputs->clearSelection();
  ui->targetOutputs->endUpdate();
  ui->targetOutputs->verticalScrollBar()->setValue(vs);

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

  ui->alphaToCoverage->setPixmap(state.outputMerger.blendState.alphaToCoverage ? tick : cross);
  ui->independentBlend->setPixmap(state.outputMerger.blendState.independentBlend ? tick : cross);

  ui->blendFactor->setText(QFormatStr("%1, %2, %3, %4")
                               .arg(state.outputMerger.blendState.blendFactor[0], 0, 'f', 2)
                               .arg(state.outputMerger.blendState.blendFactor[1], 0, 'f', 2)
                               .arg(state.outputMerger.blendState.blendFactor[2], 0, 'f', 2)
                               .arg(state.outputMerger.blendState.blendFactor[3], 0, 'f', 2));

  ui->depthEnabled->setPixmap(state.outputMerger.depthStencilState.depthEnable ? tick : cross);
  ui->depthFunc->setText(ToQStr(state.outputMerger.depthStencilState.depthFunction));
  ui->depthWrite->setPixmap(state.outputMerger.depthStencilState.depthWrites ? tick : cross);

  if(state.outputMerger.depthStencilState.depthBoundsEnable)
  {
    ui->depthBounds->setPixmap(QPixmap());
    ui->depthBounds->setText(Formatter::Format(state.outputMerger.depthStencilState.minDepthBounds) +
                             lit("-") +
                             Formatter::Format(state.outputMerger.depthStencilState.maxDepthBounds));
  }
  else
  {
    ui->depthBounds->setText(QString());
    ui->depthBounds->setPixmap(cross);
  }

  ui->stencilEnabled->setPixmap(state.outputMerger.depthStencilState.stencilEnable ? tick : cross);
  ui->stencilReadMask->setText(
      Formatter::Format((uint8_t)state.outputMerger.depthStencilState.frontFace.compareMask, true));
  ui->stencilWriteMask->setText(
      Formatter::Format((uint8_t)state.outputMerger.depthStencilState.frontFace.writeMask, true));
  ui->stencilRef->setText(
      Formatter::Format((uint8_t)state.outputMerger.depthStencilState.frontFace.reference, true));

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
  if(m_Ctx.APIProps().shaderDebugging && state.computeShader.reflection &&
     state.computeShader.reflection->debugInfo.debuggable && draw &&
     (draw->flags & DrawFlags::Dispatch))
  {
    ui->groupX->setEnabled(true);
    ui->groupY->setEnabled(true);
    ui->groupZ->setEnabled(true);

    ui->threadX->setEnabled(true);
    ui->threadY->setEnabled(true);
    ui->threadZ->setEnabled(true);

    ui->debugThread->setEnabled(true);

    // set maximums for CS debugging
    ui->groupX->setMaximum((int)draw->dispatchDimension[0] - 1);
    ui->groupY->setMaximum((int)draw->dispatchDimension[1] - 1);
    ui->groupZ->setMaximum((int)draw->dispatchDimension[2] - 1);

    if(draw->dispatchThreadsDimension[0] == 0)
    {
      ui->threadX->setMaximum((int)state.computeShader.reflection->dispatchThreadsDimension[0] - 1);
      ui->threadY->setMaximum((int)state.computeShader.reflection->dispatchThreadsDimension[1] - 1);
      ui->threadZ->setMaximum((int)state.computeShader.reflection->dispatchThreadsDimension[2] - 1);
    }
    else
    {
      ui->threadX->setMaximum((int)draw->dispatchThreadsDimension[0] - 1);
      ui->threadY->setMaximum((int)draw->dispatchThreadsDimension[1] - 1);
      ui->threadZ->setMaximum((int)draw->dispatchThreadsDimension[2] - 1);
    }

    ui->debugThread->setToolTip(QString());
  }
  else
  {
    ui->groupX->setEnabled(false);
    ui->groupY->setEnabled(false);
    ui->groupZ->setEnabled(false);

    ui->threadX->setEnabled(false);
    ui->threadY->setEnabled(false);
    ui->threadZ->setEnabled(false);

    ui->debugThread->setEnabled(false);

    if(!m_Ctx.APIProps().shaderDebugging)
      ui->debugThread->setToolTip(tr("This API does not support shader debugging"));
    else if(!draw || !(draw->flags & DrawFlags::Dispatch))
      ui->debugThread->setToolTip(tr("No dispatch selected"));
    else if(!state.computeShader.reflection)
      ui->debugThread->setToolTip(tr("No compute shader bound"));
    else if(!state.computeShader.reflection->debugInfo.debuggable)
      ui->debugThread->setToolTip(tr("This shader doesn't support debugging: %1")
                                      .arg(state.computeShader.reflection->debugInfo.debugStatus));
  }

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
    bool streamOutActive = false;

    for(const D3D12Pipe::StreamOutBind &o : state.streamOut.outputs)
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

void D3D12PipelineStateViewer::resource_itemActivated(RDTreeWidgetItem *item, int column)
{
  const D3D12Pipe::Shader *stage = stageForSender(item->treeWidget());

  if(stage == NULL)
    return;

  QVariant tag = item->tag();

  TextureDescription *tex = NULL;
  BufferDescription *buf = NULL;

  if(tag.canConvert<ResourceId>())
  {
    ResourceId id = tag.value<ResourceId>();
    tex = m_Ctx.GetTexture(id);
    buf = m_Ctx.GetBuffer(id);
  }
  else if(tag.canConvert<D3D12ViewTag>())
  {
    D3D12ViewTag view = tag.value<D3D12ViewTag>();
    tex = m_Ctx.GetTexture(view.res.resourceId);
    buf = m_Ctx.GetBuffer(view.res.resourceId);
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
      viewer->ViewTexture(tex->resourceId, true);
    }

    return;
  }
  else if(buf)
  {
    D3D12ViewTag view;

    view.res.resourceId = buf->resourceId;

    if(tag.canConvert<D3D12ViewTag>())
      view = tag.value<D3D12ViewTag>();

    uint64_t offs = 0;
    uint64_t size = buf->length;

    if(view.res.resourceId != ResourceId())
    {
      offs = view.res.firstElement * view.res.elementByteSize;
      size = uint64_t(view.res.numElements) * view.res.elementByteSize;
    }
    else
    {
      // last thing, see if it's a streamout buffer

      if(stage == &m_Ctx.CurD3D12PipelineState()->geometryShader)
      {
        for(int i = 0; i < m_Ctx.CurD3D12PipelineState()->streamOut.outputs.count(); i++)
        {
          if(buf->resourceId == m_Ctx.CurD3D12PipelineState()->streamOut.outputs[i].resourceId)
          {
            size = m_Ctx.CurD3D12PipelineState()->streamOut.outputs[i].byteSize;
            offs += m_Ctx.CurD3D12PipelineState()->streamOut.outputs[i].byteOffset;
            break;
          }
        }
      }
    }

    QString format;

    const ShaderResource *shaderRes = NULL;

    if(stage->reflection)
    {
      const rdcarray<ShaderResource> &resArray = view.type == D3D12ViewTag::SRV
                                                     ? stage->reflection->readOnlyResources
                                                     : stage->reflection->readWriteResources;

      const rdcarray<Bindpoint> &bindArray = view.type == D3D12ViewTag::SRV
                                                 ? stage->bindpointMapping.readOnlyResources
                                                 : stage->bindpointMapping.readWriteResources;

      for(int i = 0; i < bindArray.count(); i++)
      {
        if(bindArray[i].bindset == view.space && bindArray[i].bind == (int)view.res.bind)
        {
          shaderRes = &resArray[i];
          break;
        }
      }
    }

    if(shaderRes)
    {
      format = BufferFormatter::GetBufferFormatString(*shaderRes, view.res.viewFormat, offs);

      if(view.res.bufferFlags & D3DBufferViewFlags::Raw)
        format = lit("xint");
    }

    IBufferViewer *viewer = m_Ctx.ViewBuffer(offs, size, view.res.resourceId, format);

    m_Ctx.AddDockWindow(viewer->Widget(), DockReference::AddTo, this);
  }
}

void D3D12PipelineStateViewer::cbuffer_itemActivated(RDTreeWidgetItem *item, int column)
{
  const D3D12Pipe::Shader *stage = stageForSender(item->treeWidget());

  if(stage == NULL)
    return;

  QVariant tag = item->tag();

  if(!tag.canConvert<D3D12CBufTag>())
    return;

  D3D12CBufTag cb = tag.value<D3D12CBufTag>();

  if(cb.idx == ~0U)
  {
    // unused cbuffer, open regular buffer viewer
    const D3D12Pipe::ConstantBuffer &buf =
        m_Ctx.CurD3D12PipelineState()->rootElements[cb.rootElement].constantBuffers[cb.reg];

    if(buf.resourceId != ResourceId())
    {
      IBufferViewer *viewer = m_Ctx.ViewBuffer(buf.byteOffset, buf.byteSize, buf.resourceId);

      m_Ctx.AddDockWindow(viewer->Widget(), DockReference::AddTo, this);
    }

    return;
  }

  IConstantBufferPreviewer *prev = m_Ctx.ViewConstantBuffer(stage->stage, cb.idx, cb.arrayIdx);

  m_Ctx.AddDockWindow(prev->Widget(), DockReference::TransientPopupArea, this, 0.3f);
}

void D3D12PipelineStateViewer::on_iaLayouts_itemActivated(RDTreeWidgetItem *item, int column)
{
  on_meshView_clicked();
}

void D3D12PipelineStateViewer::on_iaBuffers_itemActivated(RDTreeWidgetItem *item, int column)
{
  QVariant tag = item->tag();

  if(tag.canConvert<D3D12VBIBTag>())
  {
    D3D12VBIBTag buf = tag.value<D3D12VBIBTag>();

    if(buf.id != ResourceId())
    {
      IBufferViewer *viewer = m_Ctx.ViewBuffer(buf.offset, buf.size, buf.id, buf.format);

      m_Ctx.AddDockWindow(viewer->Widget(), DockReference::AddTo, this);
    }
  }
}

void D3D12PipelineStateViewer::highlightIABind(int slot)
{
  int idx = ((slot + 1) * 21) % 32;    // space neighbouring colours reasonably distinctly

  const D3D12Pipe::InputAssembly &IA = m_Ctx.CurD3D12PipelineState()->inputAssembly;

  QColor col = QColor::fromHslF(float(idx) / 32.0f, 1.0f,
                                qBound(0.05, palette().color(QPalette::Base).lightnessF(), 0.95));

  ui->iaLayouts->beginUpdate();
  ui->iaBuffers->beginUpdate();

  if(slot < m_VBNodes.count())
  {
    if(m_VBNodes[slot] && !m_EmptyNodes.contains(m_VBNodes[slot]))
    {
      m_VBNodes[slot]->setBackgroundColor(col);
      m_VBNodes[slot]->setForegroundColor(contrastingColor(col, QColor(0, 0, 0)));
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

void D3D12PipelineStateViewer::on_iaLayouts_mouseMove(QMouseEvent *e)
{
  if(!m_Ctx.IsCaptureLoaded())
    return;

  RDTreeWidgetItem *item = ui->iaLayouts->itemAt(e->pos());

  vertex_leave(NULL);

  const D3D12Pipe::InputAssembly &IA = m_Ctx.CurD3D12PipelineState()->inputAssembly;

  if(item)
  {
    uint32_t buffer = IA.layouts[item->tag().toUInt()].inputSlot;

    highlightIABind((int)buffer);
  }
}

void D3D12PipelineStateViewer::on_iaBuffers_mouseMove(QMouseEvent *e)
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
        item->setForeground(ui->iaBuffers->palette().brush(QPalette::WindowText));
      }
    }
  }
}

void D3D12PipelineStateViewer::vertex_leave(QEvent *e)
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

void D3D12PipelineStateViewer::on_pipeFlow_stageSelected(int index)
{
  ui->stagesTabs->setCurrentIndex(index);
}

void D3D12PipelineStateViewer::shaderView_clicked()
{
  QWidget *sender = qobject_cast<QWidget *>(QObject::sender());
  const D3D12Pipe::Shader *stage = stageForSender(sender);

  if(stage == NULL || stage->resourceId == ResourceId())
    return;

  if(!stage->reflection)
    return;

  IShaderViewer *shad =
      m_Ctx.ViewShader(stage->reflection, m_Ctx.CurD3D12PipelineState()->pipelineResourceId);

  m_Ctx.AddDockWindow(shad->Widget(), DockReference::AddTo, this);
}

void D3D12PipelineStateViewer::shaderSave_clicked()
{
  const D3D12Pipe::Shader *stage = stageForSender(qobject_cast<QWidget *>(QObject::sender()));

  if(stage == NULL)
    return;

  ShaderReflection *shaderDetails = stage->reflection;

  if(stage->resourceId == ResourceId())
    return;

  m_Common.SaveShaderFile(shaderDetails);
}

QVariantList D3D12PipelineStateViewer::exportViewHTML(const D3D12Pipe::View &view, bool rw,
                                                      const ShaderResource *shaderInput,
                                                      const QString &extraParams)
{
  QString name = view.resourceId == ResourceId() ? tr("Empty")
                                                 : QString(m_Ctx.GetResourceName(view.resourceId));
  QString viewType = tr("Unknown");
  QString typeName = tr("Unknown");
  QString format = tr("Unknown");
  uint64_t w = 1;
  uint32_t h = 1, d = 1;
  uint32_t a = 0;

  QString viewFormat = view.viewFormat.Name();

  TextureDescription *tex = m_Ctx.GetTexture(view.resourceId);
  BufferDescription *buf = m_Ctx.GetBuffer(view.resourceId);

  QString viewParams;

  // check to see if it's a texture
  if(tex)
  {
    w = tex->width;
    h = tex->height;
    d = tex->depth;
    a = tex->arraysize;
    format = tex->format.Name();
    viewType = ToQStr(view.type);
    typeName = ToQStr(tex->type);

    if(view.swizzle.red != TextureSwizzle::Red || view.swizzle.green != TextureSwizzle::Green ||
       view.swizzle.blue != TextureSwizzle::Blue || view.swizzle.alpha != TextureSwizzle::Alpha)
    {
      format += tr(" swizzle[%1%2%3%4]")
                    .arg(ToQStr(view.swizzle.red))
                    .arg(ToQStr(view.swizzle.green))
                    .arg(ToQStr(view.swizzle.blue))
                    .arg(ToQStr(view.swizzle.alpha));
    }

    if(tex->mips > 1)
      viewParams = tr("Highest Mip: %1, Num Mips: %2").arg(view.firstMip).arg(view.numMips);

    if(tex->arraysize > 1)
    {
      if(!viewParams.isEmpty())
        viewParams += lit(", ");
      viewParams += tr("First Slice: %1, Array Size: %2").arg(view.firstSlice).arg(view.numSlices);
    }

    if(view.minLODClamp != 0.0f)
    {
      if(!viewParams.isEmpty())
        viewParams += lit(", ");
      viewParams += tr("MinLODClamp: %1").arg(view.minLODClamp);
    }
  }

  // if not a texture, it must be a buffer
  if(buf)
  {
    w = buf->length;
    h = 0;
    d = 0;
    a = 0;
    format = view.viewFormat.Name();
    viewType = ToQStr(view.type);
    typeName = lit("Buffer");

    if(view.bufferFlags & D3DBufferViewFlags::Raw)
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

    if(view.bufferFlags & D3DBufferViewFlags::Append || view.bufferFlags & D3DBufferViewFlags::Counter)
    {
      typeName += tr(" (Count: %1)").arg(view.bufferStructCount);
    }

    if(shaderInput && !shaderInput->isTexture)
    {
      if(view.viewFormat.compType == CompType::Typeless)
      {
        if(!shaderInput->variableType.members.isEmpty())
          viewFormat = format = lit("struct ") + shaderInput->variableType.descriptor.name;
        else
          viewFormat = format = shaderInput->variableType.descriptor.name;
      }
      else
      {
        format = view.viewFormat.Name();
      }
    }

    viewParams = tr("First Element: %1, Num Elements %2, Flags %3")
                     .arg(view.firstElement)
                     .arg(view.numElements)
                     .arg(ToQStr(view.bufferFlags));

    if(view.counterResourceId != ResourceId())
    {
      viewParams += tr(", Counter in %1 at %2 bytes")
                        .arg(m_Ctx.GetResourceName(view.counterResourceId))
                        .arg(view.counterByteOffset);
    }
  }

  if(viewParams.isEmpty())
    viewParams = extraParams;
  else
    viewParams += lit(", ") + extraParams;

  return {view.bind, name, viewType,   typeName, (qulonglong)w, h,
          d,         a,    viewFormat, format,   viewParams};
}

void D3D12PipelineStateViewer::exportHTML(QXmlStreamWriter &xml, const D3D12Pipe::InputAssembly &ia)
{
  const DrawcallDescription *draw = m_Ctx.CurDrawcall();

  {
    xml.writeStartElement(lit("h3"));
    xml.writeCharacters(tr("Input Layouts"));
    xml.writeEndElement();

    QList<QVariantList> rows;

    int i = 0;
    for(const D3D12Pipe::Layout &l : ia.layouts)
    {
      rows.push_back({i, l.semanticName, l.semanticIndex, l.format.Name(), l.inputSlot,
                      l.byteOffset, (bool)l.perInstance, l.instanceDataStepRate});

      i++;
    }

    m_Common.exportHTMLTable(
        xml, {tr("Slot"), tr("Semantic Name"), tr("Semantic Index"), tr("Format"), tr("Input Slot"),
              tr("Byte Offset"), tr("Per Instance"), tr("Instance Data Step Rate")},
        rows);
  }

  {
    xml.writeStartElement(lit("h3"));
    xml.writeCharacters(tr("Vertex Buffers"));
    xml.writeEndElement();

    QList<QVariantList> rows;

    int i = 0;
    for(const D3D12Pipe::VertexBuffer &vb : ia.vertexBuffers)
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

      length = qMin(length, (uint64_t)vb.byteSize);

      rows.push_back({i, name, vb.byteStride, (qulonglong)vb.byteOffset, (qulonglong)length});

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

    length = qMin(length, (uint64_t)ia.indexBuffer.byteSize);

    QString ifmt = lit("UNKNOWN");
    if(draw)
    {
      if(draw->indexByteWidth == 2)
        ifmt = lit("R16_UINT");
      if(draw->indexByteWidth == 4)
        ifmt = lit("R32_UINT");
    }

    m_Common.exportHTMLTable(xml, {tr("Buffer"), tr("Format"), tr("Offset"), tr("Byte Length")},
                             {name, ifmt, (qulonglong)ia.indexBuffer.byteOffset, (qulonglong)length});
  }

  xml.writeStartElement(lit("p"));
  xml.writeEndElement();

  m_Common.exportHTMLTable(xml, {tr("Primitive Topology")},
                           {ToQStr(draw ? draw->topology : Topology::Unknown)});
}

void D3D12PipelineStateViewer::exportHTML(QXmlStreamWriter &xml, const D3D12Pipe::Shader &sh,
                                          const rdcarray<D3D12Pipe::RootSignatureRange> &els)
{
  ShaderReflection *shaderDetails = sh.reflection;

  {
    xml.writeStartElement(lit("h3"));
    xml.writeCharacters(tr("Shader"));
    xml.writeEndElement();

    QString shadername = tr("Unknown");

    const D3D12Pipe::State &state = *m_Ctx.CurD3D12PipelineState();

    if(sh.resourceId == ResourceId())
      shadername = tr("Unbound");
    else
      shadername = tr("%1 - %2 Shader")
                       .arg(m_Ctx.GetResourceName(state.pipelineResourceId))
                       .arg(ToQStr(sh.stage, GraphicsAPI::D3D12));

    if(shaderDetails && !shaderDetails->debugInfo.files.empty())
    {
      shadername = QFormatStr("%1() - %2")
                       .arg(shaderDetails->entryPoint)
                       .arg(QFileInfo(shaderDetails->debugInfo.files[0].filename).fileName());
    }

    xml.writeStartElement(lit("p"));
    xml.writeCharacters(shadername);
    xml.writeEndElement();

    if(sh.resourceId == ResourceId())
      return;
  }

  QList<QVariantList> rowsRO;
  QList<QVariantList> rowsRW;
  QList<QVariantList> rowsSampler;
  QList<QVariantList> rowsCB;
  for(size_t i = 0; i < els.size(); ++i)
  {
    if((els[i].visibility & MaskForStage(sh.stage)) == ShaderStageMask::Unknown)
      continue;

    switch(els[i].type)
    {
      case BindType::ReadOnlyResource:
      {
        for(size_t j = 0; j < els[i].views.size(); ++j)
        {
          const D3D12Pipe::View &v = els[i].views[j];
          const ShaderResource *shaderInput = NULL;

          if(sh.reflection)
          {
            for(int32_t k = 0; k < sh.bindpointMapping.readOnlyResources.count(); ++k)
            {
              const Bindpoint &b = sh.bindpointMapping.readOnlyResources[k];
              const ShaderResource &res = sh.reflection->readOnlyResources[k];

              bool regMatch = b.bind == (int32_t)v.bind;

              // handle unbounded arrays specially. It's illegal to have an unbounded array with
              // anything after it
              if(b.bind <= (int32_t)v.bind)
              {
                regMatch = (b.arraySize == ~0U) || (b.bind + (int32_t)b.arraySize > (int32_t)v.bind);
              }

              if(b.bindset == (int32_t)els[i].registerSpace && regMatch)
              {
                shaderInput = &res;
                break;
              }
            }
          }

          QString rootel = els[i].immediate
                               ? tr("#%1 Direct").arg(els[i].rootElement)
                               : tr("#%1 Table[%2]").arg(els[i].rootElement).arg(v.tableIndex);

          QVariantList row = exportViewHTML(v, false, shaderInput, QString());

          row.push_front(els[i].registerSpace);
          row.push_front(rootel);

          rowsRO.push_back(row);
        }
        break;
      }
      case BindType::ReadWriteResource:
      {
        for(size_t j = 0; j < els[i].views.size(); ++j)
        {
          const D3D12Pipe::View &v = els[i].views[j];
          const ShaderResource *shaderInput = NULL;

          if(sh.reflection)
          {
            for(int32_t k = 0; k < sh.bindpointMapping.readWriteResources.count(); ++k)
            {
              const Bindpoint &b = sh.bindpointMapping.readWriteResources[k];
              const ShaderResource &res = sh.reflection->readWriteResources[k];

              bool regMatch = b.bind == (int32_t)v.bind;

              // handle unbounded arrays specially. It's illegal to have an unbounded array with
              // anything after it
              if(b.bind <= (int32_t)v.bind)
              {
                regMatch = (b.arraySize == ~0U) || (b.bind + (int32_t)b.arraySize > (int32_t)v.bind);
              }

              if(b.bindset == (int32_t)els[i].registerSpace && regMatch)
              {
                shaderInput = &res;
                break;
              }
            }
          }

          QString rootel = els[i].immediate
                               ? tr("#%1 Direct").arg(els[i].rootElement)
                               : tr("#%1 Table[%2]").arg(els[i].rootElement).arg(v.tableIndex);

          QVariantList row = exportViewHTML(v, true, shaderInput, QString());

          row.push_front(els[i].registerSpace);
          row.push_front(rootel);

          rowsRW.push_back(row);
        }
        break;
      }
      case BindType::Sampler:
      {
        for(size_t j = 0; j < els[i].samplers.size(); ++j)
        {
          const D3D12Pipe::Sampler &s = els[i].samplers[j];
          const ShaderSampler *shaderInput = NULL;

          if(sh.reflection)
          {
            for(int32_t k = 0; k < sh.bindpointMapping.samplers.count(); ++k)
            {
              const Bindpoint &b = sh.bindpointMapping.samplers[k];
              const ShaderSampler &res = sh.reflection->samplers[k];

              bool regMatch = b.bind == (int32_t)s.bind;

              // handle unbounded arrays specially. It's illegal to have an unbounded array with
              // anything after it
              if(b.bind <= (int32_t)s.bind)
              {
                regMatch = (b.arraySize == ~0U) || (b.bind + (int32_t)b.arraySize > (int32_t)s.bind);
              }

              if(b.bindset == (int32_t)els[i].registerSpace && regMatch)
              {
                shaderInput = &res;
                break;
              }
            }
          }

          QString rootel = els[i].immediate
                               ? tr("#%1 Static").arg(els[i].rootElement)
                               : tr("#%1 Table[%2]").arg(els[i].rootElement).arg(s.tableIndex);

          {
            QString regname = QString::number(s.bind);

            if(shaderInput && !shaderInput->name.empty())
              regname += lit(": ") + shaderInput->name;

            QString borderColor = QFormatStr("%1, %2, %3, %4")
                                      .arg(s.borderColor[0])
                                      .arg(s.borderColor[1])
                                      .arg(s.borderColor[2])
                                      .arg(s.borderColor[3]);

            QString addressing;

            QString addPrefix;
            QString addVal;

            QString addr[] = {ToQStr(s.addressU, GraphicsAPI::D3D12),
                              ToQStr(s.addressV, GraphicsAPI::D3D12),
                              ToQStr(s.addressW, GraphicsAPI::D3D12)};

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

            if(s.UseBorder())
              addressing += QFormatStr("<%1>").arg(borderColor);

            QString filter = ToQStr(s.filter);

            if(s.maxAnisotropy > 1)
              filter += QFormatStr(" %1x").arg(s.maxAnisotropy);

            if(s.filter.filter == FilterFunction::Comparison)
              filter += QFormatStr(" (%1)").arg(ToQStr(s.compareFunction));
            else if(s.filter.filter != FilterFunction::Normal)
              filter += QFormatStr(" (%1)").arg(ToQStr(s.filter.filter));

            rowsSampler.push_back(
                {rootel, els[i].registerSpace, regname, addressing, filter,
                 QFormatStr("%1 - %2")
                     .arg(s.minLOD == -FLT_MAX ? lit("0") : QString::number(s.minLOD))
                     .arg(s.maxLOD == FLT_MAX ? lit("FLT_MAX") : QString::number(s.maxLOD)),
                 s.mipLODBias});
          }
        }
        break;
      }
      case BindType::ConstantBuffer:
      {
        for(size_t j = 0; j < els[i].constantBuffers.size(); ++j)
        {
          const D3D12Pipe::ConstantBuffer &b = els[i].constantBuffers[j];
          const ConstantBlock *shaderCBuf = NULL;

          if(sh.reflection)
          {
            for(int32_t k = 0; k < sh.bindpointMapping.constantBlocks.count(); ++k)
            {
              const Bindpoint &bm = sh.bindpointMapping.constantBlocks[k];
              const ConstantBlock &res = sh.reflection->constantBlocks[k];

              bool regMatch = bm.bind == (int32_t)b.bind;

              // handle unbounded arrays specially. It's illegal to have an unbounded array with
              // anything after it
              if(bm.bind <= (int32_t)b.bind)
              {
                regMatch =
                    (bm.arraySize == ~0U) || (bm.bind + (int32_t)bm.arraySize > (int32_t)b.bind);
              }

              if(bm.bindset == (int32_t)els[i].registerSpace && regMatch)
              {
                shaderCBuf = &res;
                break;
              }
            }
          }

          QString rootel;

          if(els[i].immediate)
          {
            if(!b.rootValues.empty())
              rootel = tr("#%1 Consts").arg(els[i].rootElement);
            else
              rootel = tr("#%1 Direct").arg(els[i].rootElement);
          }
          else
          {
            rootel = tr("#%1 Table[%2]").arg(els[i].rootElement).arg(b.tableIndex);
          }

          {
            QString name = tr("Constant Buffer %1").arg(ToQStr(b.resourceId));
            uint64_t length = b.byteSize;
            uint64_t offset = b.byteOffset;
            int numvars = shaderCBuf ? shaderCBuf->variables.count() : 0;
            uint32_t bytesize = shaderCBuf ? shaderCBuf->byteSize : 0;

            if(els[i].immediate && !b.rootValues.empty())
              bytesize = uint32_t(b.rootValues.count() * 4);

            if(b.resourceId != ResourceId())
              name = m_Ctx.GetResourceName(b.resourceId);
            else
              name = tr("Empty");

            QString regname = QString::number(b.bind);

            if(shaderCBuf && !shaderCBuf->name.empty())
              regname += lit(": ") + shaderCBuf->name;

            length = qMin(length, (uint64_t)bytesize);

            rowsCB.push_back({rootel, els[i].registerSpace, regname, name, (qulonglong)offset,
                              (qulonglong)length, numvars});
          }
        }
        break;
      }
      default: qCritical() << "Unexpected BindType for D3D12 pipeline"; break;
    }
  }

  xml.writeStartElement(lit("h3"));
  xml.writeCharacters(tr("Shader Resource Views"));
  xml.writeEndElement();

  m_Common.exportHTMLTable(
      xml, {tr("Root Sig El"), tr("Space"), tr("Register"), tr("Resource"), tr("View Type"),
            tr("Resource Type"), tr("Width"), tr("Height"), tr("Depth"), tr("Array Size"),
            tr("View Format"), tr("Resource Format"), tr("View Parameters")},
      rowsRO);

  xml.writeStartElement(lit("h3"));
  xml.writeCharacters(tr("Unordered Access Views"));
  xml.writeEndElement();

  m_Common.exportHTMLTable(
      xml, {tr("Root Sig El"), tr("Space"), tr("Register"), tr("Resource"), tr("View Type"),
            tr("Resource Type"), tr("Width"), tr("Height"), tr("Depth"), tr("Array Size"),
            tr("View Format"), tr("Resource Format"), tr("View Parameters")},
      rowsRW);

  xml.writeStartElement(lit("h3"));
  xml.writeCharacters(tr("Samplers"));
  xml.writeEndElement();

  m_Common.exportHTMLTable(xml, {tr("Root Sig El"), tr("Space"), tr("Register"), tr("Addressing"),
                                 tr("Filter"), tr("LOD Clamp"), tr("LOD Bias")},
                           rowsSampler);

  xml.writeStartElement(lit("h3"));
  xml.writeCharacters(tr("Constant Buffers"));
  xml.writeEndElement();

  m_Common.exportHTMLTable(xml,
                           {tr("Root Signature Index"), tr("Space"), tr("Register"), tr("Buffer"),
                            tr("Byte Offset"), tr("Byte Size"), tr("Number of Variables")},
                           rowsCB);
}

void D3D12PipelineStateViewer::exportHTML(QXmlStreamWriter &xml, const D3D12Pipe::StreamOut &so)
{
  {
    xml.writeStartElement(lit("h3"));
    xml.writeCharacters(tr("Stream Out Targets"));
    xml.writeEndElement();

    QList<QVariantList> rows;

    int i = 0;
    for(const D3D12Pipe::StreamOutBind &o : so.outputs)
    {
      QString name = m_Ctx.GetResourceName(o.resourceId);
      uint64_t length = 0;
      QString counterName = m_Ctx.GetResourceName(o.writtenCountResourceId);
      uint64_t counterLength = 0;

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

      if(o.writtenCountResourceId == ResourceId())
      {
        counterName = tr("Empty");
      }
      else
      {
        BufferDescription *buf = m_Ctx.GetBuffer(o.writtenCountResourceId);
        if(buf)
          counterLength = buf->length;
      }

      length = qMin(length, o.byteSize);

      rows.push_back({i, name, (qulonglong)o.byteOffset, (qulonglong)length, counterName,
                      (qulonglong)o.writtenCountByteOffset, (qulonglong)counterLength});

      i++;
    }

    m_Common.exportHTMLTable(
        xml, {tr("Slot"), tr("Buffer"), tr("Offset"), tr("Byte Length"), tr("Counter Buffer"),
              tr("Counter Offset"), tr("Counter Byte Length")},
        rows);
  }
}

void D3D12PipelineStateViewer::exportHTML(QXmlStreamWriter &xml, const D3D12Pipe::Rasterizer &rs)
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
        xml, {tr("Line AA Enable"), tr("Multisample Enable"), tr("Forced Sample Count"),
              tr("Conservative Raster"), tr("Sample Mask")},
        {rs.state.antialiasedLines ? tr("Yes") : tr("No"),
         rs.state.multisampleEnable ? tr("Yes") : tr("No"), rs.state.forcedSampleCount,
         rs.state.conservativeRasterization != ConservativeRaster::Disabled ? tr("Yes") : tr("No"),
         Formatter::Format(rs.sampleMask, true)});

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
    for(const Scissor &s : rs.scissors)
    {
      rows.push_back({i, s.x, s.y, s.width, s.height});

      i++;
    }

    m_Common.exportHTMLTable(xml, {tr("Slot"), tr("X"), tr("Y"), tr("Width"), tr("Height")}, rows);
  }
}

void D3D12PipelineStateViewer::exportHTML(QXmlStreamWriter &xml, const D3D12Pipe::OM &om)
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

    m_Common.exportHTMLTable(xml, {tr("Independent Blend Enable"), tr("Alpha to Coverage"),
                                   tr("Blend Factor"), tr("Multisampling Rate")},
                             {om.blendState.independentBlend ? tr("Yes") : tr("No"),
                              om.blendState.alphaToCoverage ? tr("Yes") : tr("No"), blendFactor,
                              tr("%1x %2 qual").arg(om.multiSampleCount).arg(om.multiSampleQuality)});

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

    m_Common.exportHTMLTable(
        xml,
        {
            tr("Slot"), tr("Blend Enable"), tr("Logic Enable"), tr("Blend Source"),
            tr("Blend Destination"), tr("Blend Operation"), tr("Alpha Blend Source"),
            tr("Alpha Blend Destination"), tr("Alpha Blend Operation"), tr("Logic Operation"),
            tr("Write Mask"),
        },
        rows);
  }

  {
    xml.writeStartElement(lit("h3"));
    xml.writeCharacters(tr("Depth State"));
    xml.writeEndElement();

    m_Common.exportHTMLTable(
        xml,
        {
            tr("Depth Test Enable"), tr("Depth Writes Enable"), tr("Depth Function"),
            tr("Depth Bounds"),
        },
        {
            om.depthStencilState.depthEnable ? tr("Yes") : tr("No"),
            om.depthStencilState.depthWrites ? tr("Yes") : tr("No"),
            ToQStr(om.depthStencilState.depthFunction),
            om.depthStencilState.depthBoundsEnable
                ? QFormatStr("%1 - %2")
                      .arg(Formatter::Format(om.depthStencilState.minDepthBounds))
                      .arg(Formatter::Format(om.depthStencilState.maxDepthBounds))
                : tr("Disabled"),
        });
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

    m_Common.exportHTMLTable(xml, {tr("Face"), tr("Function"), tr("Pass Operation"),
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

    for(int i = 0; i < om.renderTargets.count(); i++)
    {
      if(om.renderTargets[i].resourceId == ResourceId())
        continue;

      QVariantList row = exportViewHTML(om.renderTargets[i], false, NULL, QString());
      row.push_front(i);

      rows.push_back(row);
    }

    m_Common.exportHTMLTable(xml,
                             {
                                 tr("Slot"), tr("Name"), tr("View Type"), tr("Resource Type"),
                                 tr("Width"), tr("Height"), tr("Depth"), tr("Array Size"),
                                 tr("View Format"), tr("Resource Format"), tr("View Parameters"),
                             },
                             rows);
  }

  {
    xml.writeStartElement(lit("h3"));
    xml.writeCharacters(tr("Depth target"));
    xml.writeEndElement();

    QString extra;

    if(om.depthReadOnly && om.stencilReadOnly)
      extra = tr("Depth & Stencil Read-Only");
    else if(om.depthReadOnly)
      extra = tr("Depth Read-Only");
    else if(om.stencilReadOnly)
      extra = tr("Stencil Read-Only");

    m_Common.exportHTMLTable(xml,
                             {
                                 tr("Name"), tr("View Type"), tr("Resource Type"), tr("Width"),
                                 tr("Height"), tr("Depth"), tr("Array Size"), tr("View Format"),
                                 tr("Resource Format"), tr("View Parameters"),
                             },
                             {exportViewHTML(om.depthTarget, false, NULL, extra)});
  }
}

void D3D12PipelineStateViewer::on_exportHTML_clicked()
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

      switch(stage)
      {
        case 0: exportHTML(xml, m_Ctx.CurD3D12PipelineState()->inputAssembly); break;
        case 1:
          exportHTML(xml, m_Ctx.CurD3D12PipelineState()->vertexShader,
                     m_Ctx.CurD3D12PipelineState()->rootElements);
          break;
        case 2:
          exportHTML(xml, m_Ctx.CurD3D12PipelineState()->hullShader,
                     m_Ctx.CurD3D12PipelineState()->rootElements);
          break;
        case 3:
          exportHTML(xml, m_Ctx.CurD3D12PipelineState()->domainShader,
                     m_Ctx.CurD3D12PipelineState()->rootElements);
          break;
        case 4:
          exportHTML(xml, m_Ctx.CurD3D12PipelineState()->geometryShader,
                     m_Ctx.CurD3D12PipelineState()->rootElements);
          exportHTML(xml, m_Ctx.CurD3D12PipelineState()->streamOut);
          break;
        case 5: exportHTML(xml, m_Ctx.CurD3D12PipelineState()->rasterizer); break;
        case 6:
          exportHTML(xml, m_Ctx.CurD3D12PipelineState()->pixelShader,
                     m_Ctx.CurD3D12PipelineState()->rootElements);
          break;
        case 7: exportHTML(xml, m_Ctx.CurD3D12PipelineState()->outputMerger); break;
        case 8:
          exportHTML(xml, m_Ctx.CurD3D12PipelineState()->computeShader,
                     m_Ctx.CurD3D12PipelineState()->rootElements);
          break;
      }

      xml.writeEndElement();

      stage++;
    }

    m_Common.endHTMLExport(xmlptr);
  }
}

void D3D12PipelineStateViewer::on_meshView_clicked()
{
  if(!m_Ctx.HasMeshPreview())
    m_Ctx.ShowMeshPreview();
  ToolWindowManager::raiseToolWindow(m_Ctx.GetMeshPreview()->Widget());
}

void D3D12PipelineStateViewer::on_debugThread_clicked()
{
  if(!m_Ctx.APIProps().shaderDebugging)
    return;

  if(!m_Ctx.IsCaptureLoaded())
    return;

  const DrawcallDescription *draw = m_Ctx.CurDrawcall();

  if(!draw)
    return;

  ShaderReflection *shaderDetails = m_Ctx.CurD3D12PipelineState()->computeShader.reflection;
  const ShaderBindpointMapping &bindMapping =
      m_Ctx.CurD3D12PipelineState()->computeShader.bindpointMapping;

  if(!shaderDetails)
    return;

  uint32_t groupdim[3] = {};

  for(int i = 0; i < 3; i++)
    groupdim[i] = draw->dispatchDimension[i];

  uint32_t threadsdim[3] = {};
  for(int i = 0; i < 3; i++)
    threadsdim[i] = draw->dispatchThreadsDimension[i];

  if(threadsdim[0] == 0)
  {
    for(int i = 0; i < 3; i++)
      threadsdim[i] = shaderDetails->dispatchThreadsDimension[i];
  }

  struct threadSelect
  {
    uint32_t g[3];
    uint32_t t[3];
  } thread = {
      // g[]
      {(uint32_t)ui->groupX->value(), (uint32_t)ui->groupY->value(), (uint32_t)ui->groupZ->value()},
      // t[]
      {(uint32_t)ui->threadX->value(), (uint32_t)ui->threadY->value(), (uint32_t)ui->threadZ->value()},
  };

  bool done = false;
  ShaderDebugTrace *trace = NULL;

  m_Ctx.Replay().AsyncInvoke([&trace, &done, thread](IReplayController *r) {
    trace = r->DebugThread(thread.g, thread.t);

    if(trace->debugger == NULL)
    {
      r->FreeTrace(trace);
      trace = NULL;
    }

    done = true;
  });

  QString debugContext = lit("Group [%1,%2,%3] Thread [%4,%5,%6]")
                             .arg(thread.g[0])
                             .arg(thread.g[1])
                             .arg(thread.g[2])
                             .arg(thread.t[0])
                             .arg(thread.t[1])
                             .arg(thread.t[2]);

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
  IShaderViewer *s =
      m_Ctx.DebugShader(&bindMapping, shaderDetails,
                        m_Ctx.CurPipelineState().GetComputePipelineObject(), trace, debugContext);

  m_Ctx.AddDockWindow(s->Widget(), DockReference::AddTo, this);
}
