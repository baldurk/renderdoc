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

#include "D3D12PipelineStateViewer.h"
#include <float.h>
#include <QMouseEvent>
#include <QScrollBar>
#include <QXmlStreamWriter>
#include "3rdparty/flowlayout/FlowLayout.h"
#include "3rdparty/toolwindowmanager/ToolWindowManager.h"
#include "Code/Resources.h"
#include "Widgets/Extended/RDHeaderView.h"
#include "PipelineStateViewer.h"
#include "ui_D3D12PipelineStateViewer.h"

Q_DECLARE_METATYPE(ResourceId);

struct D3D12VBIBTag
{
  D3D12VBIBTag() { offset = 0; }
  D3D12VBIBTag(ResourceId i, uint64_t offs)
  {
    id = i;
    offset = offs;
  }

  ResourceId id;
  uint64_t offset;
};

Q_DECLARE_METATYPE(D3D12VBIBTag);

struct D3D12CBufTag
{
  D3D12CBufTag()
  {
    idx = ~0U;
    space = reg = 0;
  }
  D3D12CBufTag(uint32_t s, uint32_t r)
  {
    idx = ~0U;
    space = s;
    reg = r;
  }
  D3D12CBufTag(uint32_t i)
  {
    idx = i;
    space = reg = 0;
  }

  uint32_t idx, space, reg;
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

  D3D12ViewTag() {}
  D3D12ViewTag(ResType t, int s, int r, const D3D12Pipe::View &rs)
      : type(t), space(s), reg(r), res(rs)
  {
  }

  ResType type;
  int space, reg;
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
    QObject::connect(b, &RDLabel::clicked, this, &D3D12PipelineStateViewer::shaderLabel_clicked);
    b->setAutoFillBackground(true);
    b->setBackgroundRole(QPalette::ToolTipBase);
    b->setForegroundRole(QPalette::ToolTipText);
  }

  for(QToolButton *b : editButtons)
    QObject::connect(b, &QToolButton::clicked, this, &D3D12PipelineStateViewer::shaderEdit_clicked);

  for(QToolButton *b : saveButtons)
    QObject::connect(b, &QToolButton::clicked, this, &D3D12PipelineStateViewer::shaderSave_clicked);

  QObject::connect(ui->iaLayouts, &RDTreeWidget::leave, this,
                   &D3D12PipelineStateViewer::vertex_leave);
  QObject::connect(ui->iaBuffers, &RDTreeWidget::leave, this,
                   &D3D12PipelineStateViewer::vertex_leave);

  QObject::connect(ui->targetOutputs, &RDTreeWidget::itemActivated, this,
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

    ui->blends->setColumns({tr("Slot"), tr("Logic"), tr("Enabled"), tr("Col Src"), tr("Col Dst"),
                            tr("Col Op"), tr("Alpha Src"), tr("Alpha Dst"), tr("Alpha Op"),
                            tr("Logic Op"), tr("Write Mask")});
    header->setColumnStretchHints({-1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 1});

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

void D3D12PipelineStateViewer::OnLogfileLoaded()
{
  OnEventChanged(m_Ctx.CurEvent());
}

void D3D12PipelineStateViewer::OnLogfileClosed()
{
  ui->pipeFlow->setStagesEnabled({true, true, true, true, true, true, true, true, true});

  clearState();
}

void D3D12PipelineStateViewer::OnEventChanged(uint32_t eventID)
{
  setState();
}

void D3D12PipelineStateViewer::on_showDisabled_toggled(bool checked)
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

bool D3D12PipelineStateViewer::HasImportantViewParams(const D3D12Pipe::View &view,
                                                      TextureDescription *tex)
{
  // we don't count 'upgrade typeless to typed' as important, we just display the typed format
  // in the row since there's no real hidden important information there. The formats can't be
  // different for any other reason (if the SRV format differs from the texture format, the
  // texture must have been typeless.
  if(view.HighestMip > 0 || view.FirstArraySlice > 0 ||
     (view.NumMipLevels < tex->mips && tex->mips > 1) ||
     (view.ArraySize < tex->arraysize && tex->arraysize > 1))
    return true;

  // in the case of the swapchain case, types can be different and it won't have shown
  // up as taking the view's format because the swapchain already has one. Make sure to mark it
  // as important
  if(view.Format.compType != CompType::Typeless && view.Format != tex->format)
    return true;

  return false;
}

bool D3D12PipelineStateViewer::HasImportantViewParams(const D3D12Pipe::View &view,
                                                      BufferDescription *buf)
{
  if(view.FirstElement > 0 || view.NumElements * view.ElementSize < buf->length)
    return true;

  return false;
}

void D3D12PipelineStateViewer::setViewDetails(RDTreeWidgetItem *node, const D3D12ViewTag &view,
                                              TextureDescription *tex)
{
  if(tex == NULL)
    return;

  QString text;

  const D3D12Pipe::View &res = view.res;

  bool viewdetails = false;

  for(const D3D12Pipe::ResourceData &im : m_Ctx.CurD3D12PipelineState().Resources)
  {
    if(im.id == tex->ID)
    {
      text += tr("Texture is in the '%1' state\n\n").arg(ToQStr(im.states[0].name));
      break;
    }
  }

  if(res.Format != tex->format)
  {
    text += tr("The texture is format %1, the view treats it as %2.\n")
                .arg(ToQStr(tex->format.strname))
                .arg(ToQStr(res.Format.strname));

    viewdetails = true;
  }

  if(view.space == D3D12ViewTag::OMDepth)
  {
    if(m_Ctx.CurD3D12PipelineState().m_OM.DepthReadOnly)
      text += tr("Depth component is read-only\n");
    if(m_Ctx.CurD3D12PipelineState().m_OM.StencilReadOnly)
      text += tr("Stencil component is read-only\n");
  }

  if(tex->mips > 1 && (tex->mips != res.NumMipLevels || res.HighestMip > 0))
  {
    if(res.NumMipLevels == 1)
      text +=
          tr("The texture has %1 mips, the view covers mip %2.\n").arg(tex->mips).arg(res.HighestMip);
    else
      text += tr("The texture has %1 mips, the view covers mips %2-%3.\n")
                  .arg(tex->mips)
                  .arg(res.HighestMip)
                  .arg(res.HighestMip + res.NumMipLevels - 1);

    viewdetails = true;
  }

  if(tex->arraysize > 1 && (tex->arraysize != res.ArraySize || res.FirstArraySlice > 0))
  {
    if(res.ArraySize == 1)
      text += tr("The texture has %1 array slices, the view covers slice %2.\n")
                  .arg(tex->arraysize)
                  .arg(res.FirstArraySlice);
    else
      text += tr("The texture has %1 array slices, the view covers slices %2-%3.\n")
                  .arg(tex->arraysize)
                  .arg(res.FirstArraySlice)
                  .arg(res.FirstArraySlice + res.ArraySize);

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

  for(const D3D12Pipe::ResourceData &im : m_Ctx.CurD3D12PipelineState().Resources)
  {
    if(im.id == buf->ID)
    {
      text += tr("Buffer is in the '%1' state\n\n").arg(ToQStr(im.states[0].name));
      break;
    }
  }

  bool viewdetails = false;

  if((res.FirstElement * res.ElementSize) > 0 || (res.NumElements * res.ElementSize) < buf->length)
  {
    text += tr("The view covers bytes %1-%2 (%3 elements).\nThe buffer is %3 bytes in length (%5 "
               "elements).")
                .arg(res.FirstElement * res.ElementSize)
                .arg((res.FirstElement + res.NumElements) * res.ElementSize)
                .arg(res.NumElements)
                .arg(buf->length)
                .arg(buf->length / res.ElementSize);

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

  // consider this register to not exist - it's in a gap defined by sparse root signature elements
  if(r.RootElement == ~0U)
    return;

  const BindpointMap *bind = NULL;
  const ShaderResource *shaderInput = NULL;

  if(stage && stage->ShaderDetails)
  {
    const rdctype::array<BindpointMap> &binds = uav ? stage->BindpointMapping.ReadWriteResources
                                                    : stage->BindpointMapping.ReadOnlyResources;
    const rdctype::array<ShaderResource> &res =
        uav ? stage->ShaderDetails->ReadWriteResources : stage->ShaderDetails->ReadOnlyResources;
    for(int i = 0; i < binds.count; i++)
    {
      const BindpointMap &b = binds[i];

      bool regMatch = b.bind == view.reg;

      // handle unbounded arrays specially. It's illegal to have an unbounded array with
      // anything after it
      if(b.bind <= view.reg)
        regMatch = (b.arraySize == ~0U) || (b.bind + (int)b.arraySize > view.reg);

      if(b.bindset == view.space && regMatch && !res[i].IsSampler)
      {
        bind = &b;
        shaderInput = &res[i];
        break;
      }
    }
  }

  bool viewDetails = false;

  if(view.space == D3D12ViewTag::OMDepth)
    viewDetails = m_Ctx.CurD3D12PipelineState().m_OM.DepthReadOnly ||
                  m_Ctx.CurD3D12PipelineState().m_OM.StencilReadOnly;

  QString rootel = r.Immediate ? tr("#%1 Direct").arg(r.RootElement)
                               : tr("#%1 Table[%2]").arg(r.RootElement).arg(r.TableIndex);

  bool filledSlot = (r.Resource != ResourceId());
  bool usedSlot = (bind && bind->used);

  // if a target is set to RTVs or DSV, it is implicitly used
  if(filledSlot)
    usedSlot =
        usedSlot || view.space == D3D12ViewTag::OMTarget || view.space == D3D12ViewTag::OMDepth;

  if(showNode(usedSlot, filledSlot))
  {
    QString regname = QString::number(view.reg);

    if(shaderInput && !shaderInput->name.empty())
      regname += lit(": ") + ToQStr(shaderInput->name);

    if(view.space == D3D12ViewTag::OMDepth)
      regname = tr("Depth");

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

      if(tex->resType == TextureDim::Texture2DMS || tex->resType == TextureDim::Texture2DMSArray)
      {
        typeName += QFormatStr(" %1x").arg(tex->msSamp);
      }

      if(tex->format != r.Format)
        format = tr("Viewed as %1").arg(ToQStr(r.Format.strname));

      if(HasImportantViewParams(r, tex))
        viewDetails = true;
    }

    BufferDescription *buf = m_Ctx.GetBuffer(r.Resource);

    if(buf)
    {
      w = buf->length;
      h = 0;
      d = 0;
      a = 0;
      format = QString();
      name = ToQStr(buf->name);
      typeName = lit("Buffer");

      if(r.BufferFlags & D3DBufferViewFlags::Raw)
      {
        typeName = QFormatStr("%1ByteAddressBuffer").arg(uav ? lit("RW") : QString());
      }
      else if(r.ElementSize > 0)
      {
        // for structured buffers, display how many 'elements' there are in the buffer
        a = buf->length / r.ElementSize;
        typeName = QFormatStr("%1StructuredBuffer[%2]").arg(uav ? lit("RW") : QString()).arg(a);
      }

      if(r.CounterResource != ResourceId())
      {
        typeName += tr(" (Count: %1)").arg(r.BufferStructCount);
      }

      // get the buffer type, whether it's just a basic type or a complex struct
      if(shaderInput && !shaderInput->IsTexture)
      {
        if(!shaderInput->variableType.members.empty())
          format = lit("struct ") + ToQStr(shaderInput->variableType.descriptor.name);
        else if(r.Format.compType == CompType::Typeless)
          format = ToQStr(shaderInput->variableType.descriptor.name);
        else
          format = ToQStr(r.Format.strname);
      }

      if(HasImportantViewParams(r, buf))
        viewDetails = true;
    }

    RDTreeWidgetItem *node = new RDTreeWidgetItem(
        {rootel, view.space, regname, name, typeName, w, h, d, a, format, QString()});

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

bool D3D12PipelineStateViewer::showNode(bool usedSlot, bool filledSlot)
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

const D3D12Pipe::Shader *D3D12PipelineStateViewer::stageForSender(QWidget *widget)
{
  if(!m_Ctx.LogLoaded())
    return NULL;

  while(widget)
  {
    if(widget == ui->stagesTabs->widget(0))
      return &m_Ctx.CurD3D12PipelineState().m_VS;
    if(widget == ui->stagesTabs->widget(1))
      return &m_Ctx.CurD3D12PipelineState().m_VS;
    if(widget == ui->stagesTabs->widget(2))
      return &m_Ctx.CurD3D12PipelineState().m_HS;
    if(widget == ui->stagesTabs->widget(3))
      return &m_Ctx.CurD3D12PipelineState().m_DS;
    if(widget == ui->stagesTabs->widget(4))
      return &m_Ctx.CurD3D12PipelineState().m_GS;
    if(widget == ui->stagesTabs->widget(5))
      return &m_Ctx.CurD3D12PipelineState().m_PS;
    if(widget == ui->stagesTabs->widget(6))
      return &m_Ctx.CurD3D12PipelineState().m_PS;
    if(widget == ui->stagesTabs->widget(7))
      return &m_Ctx.CurD3D12PipelineState().m_PS;
    if(widget == ui->stagesTabs->widget(8))
      return &m_Ctx.CurD3D12PipelineState().m_CS;

    widget = widget->parentWidget();
  }

  qCritical() << "Unrecognised control calling event handler";

  return NULL;
}

void D3D12PipelineStateViewer::clearShaderState(QLabel *shader, RDTreeWidget *tex, RDTreeWidget *samp,
                                                RDTreeWidget *cbuffer, RDTreeWidget *sub)
{
  shader->setText(tr("Unbound Shader"));
  tex->clear();
  samp->clear();
  sub->clear();
  cbuffer->clear();
}

void D3D12PipelineStateViewer::clearState()
{
  m_VBNodes.clear();

  ui->iaLayouts->clear();
  ui->iaBuffers->clear();
  ui->topology->setText(QString());
  ui->topologyDiagram->setPixmap(QPixmap());

  clearShaderState(ui->vsShader, ui->vsResources, ui->vsSamplers, ui->vsCBuffers, ui->vsUAVs);
  clearShaderState(ui->gsShader, ui->gsResources, ui->gsSamplers, ui->gsCBuffers, ui->gsUAVs);
  clearShaderState(ui->hsShader, ui->hsResources, ui->hsSamplers, ui->hsCBuffers, ui->hsUAVs);
  clearShaderState(ui->dsShader, ui->dsResources, ui->dsSamplers, ui->dsCBuffers, ui->dsUAVs);
  clearShaderState(ui->psShader, ui->psResources, ui->psSamplers, ui->psCBuffers, ui->psUAVs);
  clearShaderState(ui->csShader, ui->csResources, ui->csSamplers, ui->csCBuffers, ui->csUAVs);

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

  ui->stencilEnabled->setPixmap(cross);
  ui->stencilReadMask->setText(lit("FF"));
  ui->stencilWriteMask->setText(lit("FF"));
  ui->stencilRef->setText(lit("FF"));

  ui->stencils->clear();
}

void D3D12PipelineStateViewer::setShaderState(const D3D12Pipe::Shader &stage, QLabel *shader,
                                              RDTreeWidget *resources, RDTreeWidget *samplers,
                                              RDTreeWidget *cbuffers, RDTreeWidget *uavs)
{
  ShaderReflection *shaderDetails = stage.ShaderDetails;
  const D3D12Pipe::State &state = m_Ctx.CurD3D12PipelineState();

  if(stage.Object == ResourceId())
    shader->setText(tr("Unbound Shader"));
  else if(state.customName)
    shader->setText(
        QFormatStr("%1 - %2").arg(ToQStr(state.name)).arg(m_Ctx.CurPipelineState().Abbrev(stage.stage)));
  else
    shader->setText(
        tr("%1 - %2 Shader").arg(ToQStr(state.name)).arg(ToQStr(stage.stage, GraphicsAPI::D3D12)));

  if(shaderDetails && !shaderDetails->DebugInfo.files.empty())
  {
    shader->setText(QFormatStr("%1() - %2")
                        .arg(ToQStr(shaderDetails->EntryPoint))
                        .arg(QFileInfo(ToQStr(shaderDetails->DebugInfo.files[0].first)).fileName()));
  }

  int vs = 0;

  vs = resources->verticalScrollBar()->value();
  resources->setUpdatesEnabled(false);
  resources->clear();
  for(int space = 0; space < stage.Spaces.count; space++)
  {
    for(int reg = 0; reg < stage.Spaces[space].SRVs.count; reg++)
    {
      addResourceRow(D3D12ViewTag(D3D12ViewTag::SRV, space, reg, stage.Spaces[space].SRVs[reg]),
                     &stage, resources);
    }
  }
  resources->clearSelection();
  resources->setUpdatesEnabled(true);
  resources->verticalScrollBar()->setValue(vs);

  vs = uavs->verticalScrollBar()->value();
  uavs->setUpdatesEnabled(false);
  uavs->clear();
  for(int space = 0; space < stage.Spaces.count; space++)
  {
    for(int reg = 0; reg < stage.Spaces[space].UAVs.count; reg++)
    {
      addResourceRow(D3D12ViewTag(D3D12ViewTag::UAV, space, reg, stage.Spaces[space].UAVs[reg]),
                     &stage, uavs);
    }
  }
  uavs->clearSelection();
  uavs->setUpdatesEnabled(true);
  uavs->verticalScrollBar()->setValue(vs);

  vs = samplers->verticalScrollBar()->value();
  samplers->setUpdatesEnabled(false);
  samplers->clear();
  for(int space = 0; space < stage.Spaces.count; space++)
  {
    for(int reg = 0; reg < stage.Spaces[space].Samplers.count; reg++)
    {
      const D3D12Pipe::Sampler &s = stage.Spaces[space].Samplers[reg];

      // consider this register to not exist - it's in a gap defined by sparse root signature
      // elements
      if(s.RootElement == ~0U)
        continue;

      const BindpointMap *bind = NULL;
      const ShaderResource *shaderInput = NULL;

      if(stage.ShaderDetails)
      {
        for(int i = 0; i < stage.BindpointMapping.ReadOnlyResources.count; i++)
        {
          const BindpointMap &b = stage.BindpointMapping.ReadOnlyResources[i];
          const ShaderResource &res = stage.ShaderDetails->ReadOnlyResources[i];

          bool regMatch = b.bind == reg;

          // handle unbounded arrays specially. It's illegal to have an unbounded array with
          // anything after it
          if(b.bind <= reg)
            regMatch = (b.arraySize == ~0U) || (b.bind + (int)b.arraySize > reg);

          if(b.bindset == space && regMatch && res.IsSampler)
          {
            bind = &b;
            shaderInput = &res;
            break;
          }
        }
      }

      QString rootel = s.Immediate ? tr("#%1 Static").arg(s.RootElement)
                                   : tr("#%1 Table[%2]").arg(s.RootElement).arg(s.TableIndex);

      bool filledSlot = s.Filter.minify != FilterMode::NoFilter;
      bool usedSlot = (bind && bind->used);

      if(showNode(usedSlot, filledSlot))
      {
        QString regname = QString::number(reg);

        if(shaderInput && !shaderInput->name.empty())
          regname += lit(": ") + ToQStr(shaderInput->name);

        QString borderColor = QFormatStr("%1, %2, %3, %4")
                                  .arg(s.BorderColor[0])
                                  .arg(s.BorderColor[1])
                                  .arg(s.BorderColor[2])
                                  .arg(s.BorderColor[3]);

        QString addressing;

        QString addPrefix;
        QString addVal;

        QString addr[] = {ToQStr(s.AddressU), ToQStr(s.AddressV), ToQStr(s.AddressW)};

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

        QString filter = ToQStr(s.Filter);

        if(s.MaxAniso > 1)
          filter += QFormatStr(" %1x").arg(s.MaxAniso);

        if(s.Filter.func == FilterFunc::Comparison)
          filter += QFormatStr(" (%1)").arg(ToQStr(s.Comparison));
        else if(s.Filter.func != FilterFunc::Normal)
          filter += QFormatStr(" (%1)").arg(ToQStr(s.Filter.func));

        RDTreeWidgetItem *node = new RDTreeWidgetItem(
            {rootel, space, regname, addressing, filter,
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
  samplers->verticalScrollBar()->setValue(vs);

  vs = cbuffers->verticalScrollBar()->value();
  cbuffers->setUpdatesEnabled(false);
  cbuffers->clear();
  for(int space = 0; space < stage.Spaces.count; space++)
  {
    for(int reg = 0; reg < stage.Spaces[space].ConstantBuffers.count; reg++)
    {
      const D3D12Pipe::CBuffer &b = stage.Spaces[space].ConstantBuffers[reg];

      QVariant tag;

      const BindpointMap *bind = NULL;
      const ConstantBlock *shaderCBuf = NULL;

      if(stage.ShaderDetails)
      {
        for(int i = 0; i < stage.BindpointMapping.ConstantBlocks.count; i++)
        {
          const BindpointMap &bm = stage.BindpointMapping.ConstantBlocks[i];
          const ConstantBlock &res = stage.ShaderDetails->ConstantBlocks[i];

          bool regMatch = bm.bind == reg;

          // handle unbounded arrays specially. It's illegal to have an unbounded array with
          // anything after it
          if(bm.bind <= reg)
            regMatch = (bm.arraySize == ~0U) || (bm.bind + (int)bm.arraySize > reg);

          if(bm.bindset == space && regMatch)
          {
            bind = &bm;
            shaderCBuf = &res;
            tag = QVariant::fromValue(D3D12CBufTag(i));
            break;
          }
        }
      }

      if(!tag.isValid())
        tag = QVariant::fromValue(D3D12CBufTag(space, reg));

      QString rootel;

      if(b.Immediate)
      {
        if(!b.RootValues.empty())
          rootel = tr("#%1 Consts").arg(b.RootElement);
        else
          rootel = tr("#%1 Direct").arg(b.RootElement);
      }
      else
      {
        rootel = tr("#%1 Table[%2]").arg(b.RootElement).arg(b.TableIndex);
      }

      bool filledSlot = (b.Buffer != ResourceId());
      if(b.Immediate && !b.RootValues.empty())
        filledSlot = true;

      bool usedSlot = (bind && bind->used);

      if(showNode(usedSlot, filledSlot))
      {
        QString name = tr("Constant Buffer %1").arg(ToQStr(b.Buffer));
        ulong length = b.ByteSize;
        uint64_t offset = b.Offset;
        int numvars = shaderCBuf ? shaderCBuf->variables.count : 0;
        uint32_t bytesize = shaderCBuf ? shaderCBuf->byteSize : 0;

        if(b.Immediate && !b.RootValues.empty())
          bytesize = uint32_t(b.RootValues.count * 4);

        if(!filledSlot)
          name = tr("Empty");

        BufferDescription *buf = m_Ctx.GetBuffer(b.Buffer);

        if(buf)
          name = ToQStr(buf->name);

        QString regname = QString::number(reg);

        if(shaderCBuf && !shaderCBuf->name.empty())
          regname += lit(": ") + ToQStr(shaderCBuf->name);

        QString sizestr;
        if(bytesize == (uint32_t)length)
          sizestr = tr("%1 Variables, %2 bytes").arg(numvars).arg(length);
        else
          sizestr =
              tr("%1 Variables, %2 bytes needed, %3 provided").arg(numvars).arg(bytesize).arg(length);

        if(length < bytesize)
          filledSlot = false;

        RDTreeWidgetItem *node = new RDTreeWidgetItem(
            {rootel, (qulonglong)space, regname, name, (qulonglong)offset, sizestr, QString()});

        node->setTag(tag);

        if(!filledSlot)
          setEmptyRow(node);

        if(!usedSlot)
          setInactiveRow(node);

        cbuffers->addTopLevelItem(node);
      }
    }
  }
  cbuffers->clearSelection();
  cbuffers->setUpdatesEnabled(true);
  cbuffers->verticalScrollBar()->setValue(vs);
}

void D3D12PipelineStateViewer::setState()
{
  if(!m_Ctx.LogLoaded())
  {
    clearState();
    return;
  }

  const D3D12Pipe::State &state = m_Ctx.CurD3D12PipelineState();
  const DrawcallDescription *draw = m_Ctx.CurDrawcall();

  const QPixmap &tick = Pixmaps::tick(this);
  const QPixmap &cross = Pixmaps::cross(this);

  ////////////////////////////////////////////////
  // Vertex Input

  int vs = 0;

  bool usedVBuffers[128] = {};
  uint32_t layoutOffs[128] = {};

  vs = ui->iaLayouts->verticalScrollBar()->value();
  ui->iaLayouts->setUpdatesEnabled(false);
  ui->iaLayouts->clear();
  {
    int i = 0;
    for(const D3D12Pipe::Layout &l : state.m_IA.layouts)
    {
      QString byteOffs = QString::number(l.ByteOffset);

      // D3D12 specific value
      if(l.ByteOffset == ~0U)
      {
        byteOffs = lit("APPEND_ALIGNED (%1)").arg(layoutOffs[l.InputSlot]);
      }
      else
      {
        layoutOffs[l.InputSlot] = l.ByteOffset;
      }

      layoutOffs[l.InputSlot] += l.Format.compByteWidth * l.Format.compCount;

      bool filledSlot = true;
      bool usedSlot = false;

      for(int ia = 0; state.m_VS.ShaderDetails && ia < state.m_VS.ShaderDetails->InputSig.count; ia++)
      {
        if(ToQStr(state.m_VS.ShaderDetails->InputSig[ia].semanticName).toUpper() ==
               ToQStr(l.SemanticName).toUpper() &&
           state.m_VS.ShaderDetails->InputSig[ia].semanticIndex == l.SemanticIndex)
        {
          usedSlot = true;
          break;
        }
      }

      if(showNode(usedSlot, filledSlot))
      {
        RDTreeWidgetItem *node = new RDTreeWidgetItem(
            {i, ToQStr(l.SemanticName), l.SemanticIndex, ToQStr(l.Format.strname), l.InputSlot,
             byteOffs, l.PerInstance ? lit("PER_INSTANCE") : lit("PER_VERTEX"),
             l.InstanceDataStepRate, QString()});

        if(usedSlot)
          usedVBuffers[l.InputSlot] = true;

        if(!usedSlot)
          setInactiveRow(node);

        ui->iaLayouts->addTopLevelItem(node);
      }

      i++;
    }
  }
  ui->iaLayouts->clearSelection();
  ui->iaLayouts->setUpdatesEnabled(true);
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

  bool ibufferUsed = draw && (draw->flags & DrawFlags::UseIBuffer);

  vs = ui->iaBuffers->verticalScrollBar()->value();
  ui->iaBuffers->setUpdatesEnabled(false);
  ui->iaBuffers->clear();

  if(state.m_IA.ibuffer.Buffer != ResourceId())
  {
    if(ibufferUsed || ui->showDisabled->isChecked())
    {
      QString name = tr("Buffer ") + ToQStr(state.m_IA.ibuffer.Buffer);
      uint64_t length = 1;

      if(!ibufferUsed)
        length = 0;

      BufferDescription *buf = m_Ctx.GetBuffer(state.m_IA.ibuffer.Buffer);

      if(buf)
      {
        name = ToQStr(buf->name);
        length = buf->length;
      }

      RDTreeWidgetItem *node = new RDTreeWidgetItem(
          {tr("Index"), name, draw ? draw->indexByteWidth : 0,
           (qulonglong)state.m_IA.ibuffer.Offset, (qulonglong)length, QString()});

      node->setTag(QVariant::fromValue(
          D3D12VBIBTag(state.m_IA.ibuffer.Buffer, draw ? draw->indexOffset : 0)));

      if(!ibufferUsed)
        setInactiveRow(node);

      if(state.m_IA.ibuffer.Buffer == ResourceId())
        setEmptyRow(node);

      ui->iaBuffers->addTopLevelItem(node);
    }
  }
  else
  {
    if(ibufferUsed || ui->showEmpty->isChecked())
    {
      RDTreeWidgetItem *node = new RDTreeWidgetItem(
          {tr("Index"), tr("No Buffer Set"), lit("-"), lit("-"), lit("-"), QString()});

      node->setTag(QVariant::fromValue(
          D3D12VBIBTag(state.m_IA.ibuffer.Buffer, draw ? draw->indexOffset : 0)));

      setEmptyRow(node);

      if(!ibufferUsed)
        setInactiveRow(node);

      ui->iaBuffers->addTopLevelItem(node);
    }
  }

  m_VBNodes.clear();

  for(int i = 0; i < state.m_IA.vbuffers.count; i++)
  {
    const D3D12Pipe::VB &v = state.m_IA.vbuffers[i];

    bool filledSlot = (v.Buffer != ResourceId());
    bool usedSlot = (usedVBuffers[i]);

    if(showNode(usedSlot, filledSlot))
    {
      QString name = tr("Buffer ") + ToQStr(v.Buffer);
      qulonglong length = 1;

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

      RDTreeWidgetItem *node = NULL;

      if(filledSlot)
        node = new RDTreeWidgetItem({i, name, v.Stride, (qulonglong)v.Offset, length, QString()});
      else
        node =
            new RDTreeWidgetItem({i, tr("No Buffer Set"), lit("-"), lit("-"), lit("-"), QString()});

      node->setTag(QVariant::fromValue(D3D12VBIBTag(v.Buffer, v.Offset)));

      if(!filledSlot)
        setEmptyRow(node);

      if(!usedSlot)
        setInactiveRow(node);

      m_VBNodes.push_back(node);

      ui->iaBuffers->addTopLevelItem(node);
    }
  }
  ui->iaBuffers->clearSelection();
  ui->iaBuffers->setUpdatesEnabled(true);
  ui->iaBuffers->verticalScrollBar()->setValue(vs);

  setShaderState(state.m_VS, ui->vsShader, ui->vsResources, ui->vsSamplers, ui->vsCBuffers,
                 ui->vsUAVs);
  setShaderState(state.m_GS, ui->gsShader, ui->gsResources, ui->gsSamplers, ui->gsCBuffers,
                 ui->gsUAVs);
  setShaderState(state.m_HS, ui->hsShader, ui->hsResources, ui->hsSamplers, ui->hsCBuffers,
                 ui->hsUAVs);
  setShaderState(state.m_DS, ui->dsShader, ui->dsResources, ui->dsSamplers, ui->dsCBuffers,
                 ui->dsUAVs);
  setShaderState(state.m_PS, ui->psShader, ui->psResources, ui->psSamplers, ui->psCBuffers,
                 ui->psUAVs);
  setShaderState(state.m_CS, ui->csShader, ui->csResources, ui->csSamplers, ui->csCBuffers,
                 ui->csUAVs);

  bool streamoutSet = false;
  vs = ui->gsStreamOut->verticalScrollBar()->value();
  ui->gsStreamOut->setUpdatesEnabled(false);
  ui->gsStreamOut->clear();
  for(int i = 0; i < state.m_SO.Outputs.count; i++)
  {
    const D3D12Pipe::SOBind &s = state.m_SO.Outputs[i];

    bool filledSlot = (s.Buffer != ResourceId());
    bool usedSlot = (filledSlot);

    if(showNode(usedSlot, filledSlot))
    {
      QString name = tr("Buffer ") + ToQStr(s.Buffer);
      qulonglong length = 0;

      if(!filledSlot)
      {
        name = tr("Empty");
      }

      BufferDescription *buf = m_Ctx.GetBuffer(s.Buffer);

      if(buf)
      {
        name = ToQStr(buf->name);
        if(length == 0)
          length = buf->length;
      }

      RDTreeWidgetItem *node =
          new RDTreeWidgetItem({i, name, length, (qulonglong)s.Offset, QString()});

      node->setTag(QVariant::fromValue(s.Buffer));

      if(!filledSlot)
        setEmptyRow(node);

      if(!usedSlot)
        setInactiveRow(node);

      ui->gsStreamOut->addTopLevelItem(node);
    }
  }
  ui->gsStreamOut->verticalScrollBar()->setValue(vs);
  ui->gsStreamOut->clearSelection();
  ui->gsStreamOut->setUpdatesEnabled(true);

  ui->gsStreamOut->setVisible(streamoutSet);
  ui->soGroup->setVisible(streamoutSet);

  ////////////////////////////////////////////////
  // Rasterizer

  vs = ui->viewports->verticalScrollBar()->value();
  ui->viewports->setUpdatesEnabled(false);
  ui->viewports->clear();
  for(int i = 0; i < state.m_RS.Viewports.count; i++)
  {
    const D3D12Pipe::Viewport &v = state.m_RS.Viewports[i];

    RDTreeWidgetItem *node =
        new RDTreeWidgetItem({i, v.X, v.Y, v.Width, v.Height, v.MinDepth, v.MaxDepth});

    if(v.Width == 0 || v.Height == 0 || v.MinDepth == v.MaxDepth)
      setEmptyRow(node);

    ui->viewports->addTopLevelItem(node);
  }
  ui->viewports->verticalScrollBar()->setValue(vs);
  ui->viewports->clearSelection();
  ui->viewports->setUpdatesEnabled(true);

  vs = ui->scissors->verticalScrollBar()->value();
  ui->scissors->setUpdatesEnabled(false);
  ui->scissors->clear();
  for(int i = 0; i < state.m_RS.Scissors.count; i++)
  {
    const D3D12Pipe::Scissor &s = state.m_RS.Scissors[i];

    RDTreeWidgetItem *node =
        new RDTreeWidgetItem({i, s.left, s.top, s.right - s.left, s.bottom - s.top});

    if(s.right == s.left || s.bottom == s.top)
      setEmptyRow(node);

    ui->scissors->addTopLevelItem(node);
  }
  ui->scissors->clearSelection();
  ui->scissors->verticalScrollBar()->setValue(vs);
  ui->scissors->setUpdatesEnabled(true);

  ui->fillMode->setText(ToQStr(state.m_RS.m_State.fillMode));
  ui->cullMode->setText(ToQStr(state.m_RS.m_State.cullMode));
  ui->frontCCW->setPixmap(state.m_RS.m_State.FrontCCW ? tick : cross);

  ui->lineAA->setPixmap(state.m_RS.m_State.AntialiasedLineEnable ? tick : cross);
  ui->multisample->setPixmap(state.m_RS.m_State.MultisampleEnable ? tick : cross);

  ui->depthClip->setPixmap(state.m_RS.m_State.DepthClip ? tick : cross);
  ui->depthBias->setText(Formatter::Format(state.m_RS.m_State.DepthBias));
  ui->depthBiasClamp->setText(Formatter::Format(state.m_RS.m_State.DepthBiasClamp));
  ui->slopeScaledBias->setText(Formatter::Format(state.m_RS.m_State.SlopeScaledDepthBias));
  ui->forcedSampleCount->setText(QString::number(state.m_RS.m_State.ForcedSampleCount));
  ui->conservativeRaster->setPixmap(state.m_RS.m_State.ConservativeRasterization ? tick : cross);

  ////////////////////////////////////////////////
  // Output Merger

  bool targets[32] = {};

  vs = ui->targetOutputs->verticalScrollBar()->value();
  ui->targetOutputs->setUpdatesEnabled(false);
  ui->targetOutputs->clear();
  {
    for(int i = 0; i < state.m_OM.RenderTargets.count; i++)
    {
      addResourceRow(D3D12ViewTag(D3D12ViewTag::OMTarget, 0, i, state.m_OM.RenderTargets[i]), NULL,
                     ui->targetOutputs);

      if(state.m_OM.RenderTargets[i].Resource != ResourceId())
        targets[i] = true;
    }

    addResourceRow(D3D12ViewTag(D3D12ViewTag::OMDepth, 0, 0, state.m_OM.DepthTarget), NULL,
                   ui->targetOutputs);
  }
  ui->targetOutputs->clearSelection();
  ui->targetOutputs->setUpdatesEnabled(true);
  ui->targetOutputs->verticalScrollBar()->setValue(vs);

  vs = ui->blends->verticalScrollBar()->value();
  ui->blends->setUpdatesEnabled(false);
  ui->blends->clear();
  {
    int i = 0;
    for(const D3D12Pipe::Blend &blend : state.m_OM.m_BlendState.Blends)
    {
      bool filledSlot = (blend.Enabled || targets[i]);
      bool usedSlot = (targets[i]);

      if(showNode(usedSlot, filledSlot))
      {
        RDTreeWidgetItem *node = NULL;

        node = new RDTreeWidgetItem({i, blend.Enabled ? tr("True") : tr("False"),
                                     blend.LogicEnabled ? tr("True") : tr("False"),

                                     ToQStr(blend.m_Blend.Source), ToQStr(blend.m_Blend.Destination),
                                     ToQStr(blend.m_Blend.Operation),

                                     ToQStr(blend.m_AlphaBlend.Source),
                                     ToQStr(blend.m_AlphaBlend.Destination),
                                     ToQStr(blend.m_AlphaBlend.Operation),

                                     ToQStr(blend.Logic),

                                     QFormatStr("%1%2%3%4")
                                         .arg((blend.WriteMask & 0x1) == 0 ? lit("_") : lit("R"))
                                         .arg((blend.WriteMask & 0x2) == 0 ? lit("_") : lit("G"))
                                         .arg((blend.WriteMask & 0x4) == 0 ? lit("_") : lit("B"))
                                         .arg((blend.WriteMask & 0x8) == 0 ? lit("_") : lit("A"))});

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

  ui->alphaToCoverage->setPixmap(state.m_OM.m_BlendState.AlphaToCoverage ? tick : cross);
  ui->independentBlend->setPixmap(state.m_OM.m_BlendState.IndependentBlend ? tick : cross);

  ui->blendFactor->setText(QFormatStr("%1, %2, %3, %4")
                               .arg(state.m_OM.m_BlendState.BlendFactor[0], 0, 'f', 2)
                               .arg(state.m_OM.m_BlendState.BlendFactor[1], 0, 'f', 2)
                               .arg(state.m_OM.m_BlendState.BlendFactor[2], 0, 'f', 2)
                               .arg(state.m_OM.m_BlendState.BlendFactor[3], 0, 'f', 2));

  ui->depthEnabled->setPixmap(state.m_OM.m_State.DepthEnable ? tick : cross);
  ui->depthFunc->setText(ToQStr(state.m_OM.m_State.DepthFunc));
  ui->depthWrite->setPixmap(state.m_OM.m_State.DepthWrites ? tick : cross);

  ui->stencilEnabled->setPixmap(state.m_OM.m_State.StencilEnable ? tick : cross);
  ui->stencilReadMask->setText(Formatter::Format(state.m_OM.m_State.StencilReadMask, true));
  ui->stencilWriteMask->setText(Formatter::Format(state.m_OM.m_State.StencilWriteMask, true));
  ui->stencilRef->setText(Formatter::Format(state.m_OM.m_State.StencilRef, true));

  ui->stencils->setUpdatesEnabled(false);
  ui->stencils->clear();
  ui->stencils->addTopLevelItem(
      new RDTreeWidgetItem({tr("Front"), ToQStr(state.m_OM.m_State.m_FrontFace.Func),
                            ToQStr(state.m_OM.m_State.m_FrontFace.FailOp),
                            ToQStr(state.m_OM.m_State.m_FrontFace.DepthFailOp),
                            ToQStr(state.m_OM.m_State.m_FrontFace.PassOp)}));
  ui->stencils->addTopLevelItem(new RDTreeWidgetItem(
      {tr("Back"), ToQStr(state.m_OM.m_State.m_BackFace.Func),
       ToQStr(state.m_OM.m_State.m_BackFace.FailOp), ToQStr(state.m_OM.m_State.m_BackFace.DepthFailOp),
       ToQStr(state.m_OM.m_State.m_BackFace.PassOp)}));
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
        {true, true, state.m_HS.Object != ResourceId(), state.m_DS.Object != ResourceId(),
         state.m_GS.Object != ResourceId(), true, state.m_PS.Object != ResourceId(), true, false});
  }
}

QString D3D12PipelineStateViewer::formatMembers(int indent, const QString &nameprefix,
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
      ret += indentstr + lit("// struct %1\n").arg(ToQStr(v.type.descriptor.name));
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
    tex = m_Ctx.GetTexture(view.res.Resource);
    buf = m_Ctx.GetBuffer(view.res.Resource);
  }

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
  else if(buf)
  {
    D3D12ViewTag view;
    if(tag.canConvert<D3D12ViewTag>())
      view = tag.value<D3D12ViewTag>();

    uint64_t offs = 0;
    uint64_t size = buf->length;

    if(view.res.Resource != ResourceId())
    {
      offs = view.res.FirstElement * view.res.ElementSize;
      size = view.res.NumElements * view.res.ElementSize;
    }
    else
    {
      // last thing, see if it's a streamout buffer

      if(stage == &m_Ctx.CurD3D12PipelineState().m_GS)
      {
        for(int i = 0; i < m_Ctx.CurD3D12PipelineState().m_SO.Outputs.count; i++)
        {
          if(buf->ID == m_Ctx.CurD3D12PipelineState().m_SO.Outputs[i].Buffer)
          {
            size -= m_Ctx.CurD3D12PipelineState().m_SO.Outputs[i].Offset;
            offs += m_Ctx.CurD3D12PipelineState().m_SO.Outputs[i].Offset;
            break;
          }
        }
      }
    }

    QString format;

    const ShaderResource *shaderRes = NULL;

    if(stage->ShaderDetails)
    {
      const rdctype::array<ShaderResource> &resArray =
          view.space == D3D12ViewTag::SRV ? stage->ShaderDetails->ReadOnlyResources
                                          : stage->ShaderDetails->ReadWriteResources;

      const rdctype::array<BindpointMap> &bindArray =
          view.space == D3D12ViewTag::SRV ? stage->BindpointMapping.ReadOnlyResources
                                          : stage->BindpointMapping.ReadOnlyResources;

      for(int i = 0; i < bindArray.count; i++)
      {
        if(bindArray[i].bindset == view.space && bindArray[i].bind == view.reg &&
           !resArray[i].IsSampler)
        {
          shaderRes = &resArray[i];
          break;
        }
      }
    }

    if(shaderRes)
    {
      const ShaderResource &res = *shaderRes;

      if(!res.variableType.members.empty())
      {
        format = QFormatStr("// struct %1\n{\n%2}")
                     .arg(ToQStr(res.variableType.descriptor.name))
                     .arg(formatMembers(1, QString(), res.variableType.members));
      }
      else
      {
        const auto &desc = res.variableType.descriptor;

        if(view.res.Format.strname.empty())
        {
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
        else
        {
          const ResourceFormat &fmt = view.res.Format;
          if(fmt.special)
          {
            if(fmt.specialFormat == SpecialFormat::R10G10B10A2)
            {
              if(fmt.compType == CompType::UInt)
                format = lit("uintten");
              if(fmt.compType == CompType::UNorm)
                format = lit("unormten");
            }
            else if(fmt.specialFormat == SpecialFormat::R11G11B10)
            {
              format = lit("floateleven");
            }
          }
          else
          {
            switch(fmt.compByteWidth)
            {
              case 1:
              {
                if(fmt.compType == CompType::UNorm)
                  format = lit("unormb");
                if(fmt.compType == CompType::SNorm)
                  format = lit("snormb");
                if(fmt.compType == CompType::UInt)
                  format = lit("ubyte");
                if(fmt.compType == CompType::SInt)
                  format = lit("byte");
                break;
              }
              case 2:
              {
                if(fmt.compType == CompType::UNorm)
                  format = lit("unormh");
                if(fmt.compType == CompType::SNorm)
                  format = lit("snormh");
                if(fmt.compType == CompType::UInt)
                  format = lit("ushort");
                if(fmt.compType == CompType::SInt)
                  format = lit("short");
                if(fmt.compType == CompType::Float)
                  format = lit("half");
                break;
              }
              case 4:
              {
                if(fmt.compType == CompType::UNorm)
                  format = lit("unormf");
                if(fmt.compType == CompType::SNorm)
                  format = lit("snormf");
                if(fmt.compType == CompType::UInt)
                  format = lit("uint");
                if(fmt.compType == CompType::SInt)
                  format = lit("int");
                if(fmt.compType == CompType::Float)
                  format = lit("float");
                break;
              }
            }

            if(view.res.BufferFlags & D3DBufferViewFlags::Raw)
              format = lit("xint");

            format += fmt.compCount;
          }
        }
      }
    }

    IBufferViewer *viewer = m_Ctx.ViewBuffer(offs, size, view.res.Resource, format);

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
    const D3D12Pipe::CBuffer &buf = stage->Spaces[cb.space].ConstantBuffers[cb.reg];

    IBufferViewer *viewer = m_Ctx.ViewBuffer(buf.Offset, buf.ByteSize, buf.Buffer);

    m_Ctx.AddDockWindow(viewer->Widget(), DockReference::AddTo, this);

    return;
  }

  IConstantBufferPreviewer *prev = m_Ctx.ViewConstantBuffer(stage->stage, cb.idx, 0);

  m_Ctx.AddDockWindow(prev->Widget(), DockReference::RightOf, this, 0.3f);
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
      IBufferViewer *viewer = m_Ctx.ViewBuffer(buf.offset, UINT64_MAX, buf.id);

      m_Ctx.AddDockWindow(viewer->Widget(), DockReference::AddTo, this);
    }
  }
}

void D3D12PipelineStateViewer::highlightIABind(int slot)
{
  int idx = ((slot + 1) * 21) % 32;    // space neighbouring colours reasonably distinctly

  const D3D12Pipe::IA &IA = m_Ctx.CurD3D12PipelineState().m_IA;

  QColor col = QColor::fromHslF(float(idx) / 32.0f, 1.0f,
                                qBound(0.05, palette().color(QPalette::Base).lightnessF(), 0.95));

  ui->iaLayouts->beginUpdate();
  ui->iaBuffers->beginUpdate();

  if(slot < m_VBNodes.count())
  {
    m_VBNodes[slot]->setBackgroundColor(col);
    m_VBNodes[slot]->setForegroundColor(contrastingColor(col, QColor(0, 0, 0)));
  }

  for(int i = 0; i < ui->iaLayouts->topLevelItemCount(); i++)
  {
    RDTreeWidgetItem *item = ui->iaLayouts->topLevelItem(i);

    if((int)IA.layouts[i].InputSlot != slot)
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
  if(!m_Ctx.LogLoaded())
    return;

  QModelIndex idx = ui->iaLayouts->indexAt(e->pos());

  vertex_leave(NULL);

  const D3D12Pipe::IA &IA = m_Ctx.CurD3D12PipelineState().m_IA;

  if(idx.isValid())
  {
    if(idx.row() >= 0 && idx.row() < IA.layouts.count)
    {
      uint32_t buffer = IA.layouts[idx.row()].InputSlot;

      highlightIABind((int)buffer);
    }
  }
}

void D3D12PipelineStateViewer::on_iaBuffers_mouseMove(QMouseEvent *e)
{
  if(!m_Ctx.LogLoaded())
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
      item->setBackground(ui->iaBuffers->palette().brush(QPalette::Window));
      item->setForeground(ui->iaBuffers->palette().brush(QPalette::WindowText));
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

  if(stage == NULL || stage->Object == ResourceId())
    return;

  IShaderViewer *shad =
      m_Ctx.ViewShader(&stage->BindpointMapping, stage->ShaderDetails, stage->stage);

  m_Ctx.AddDockWindow(shad->Widget(), DockReference::AddTo, this);
}

void D3D12PipelineStateViewer::shaderLabel_clicked(QMouseEvent *event)
{
  // forward to shaderView_clicked, we only need this to handle the different parameter, and we
  // can't use a lambda because then QObject::sender() is NULL
  shaderView_clicked();
}

void D3D12PipelineStateViewer::shaderEdit_clicked()
{
  QWidget *sender = qobject_cast<QWidget *>(QObject::sender());
  const D3D12Pipe::Shader *stage = stageForSender(sender);

  if(!stage || stage->Object == ResourceId())
    return;

  const ShaderReflection *shaderDetails = stage->ShaderDetails;

  if(!shaderDetails)
    return;

  QString entryFunc = lit("EditedShader%1S").arg(ToQStr(stage->stage, GraphicsAPI::D3D12)[0]);

  QString mainfile;

  QStringMap files;

  bool hasOrigSource = m_Common.PrepareShaderEditing(shaderDetails, entryFunc, files, mainfile);

  if(!hasOrigSource)
  {
    mainfile = lit("generated.hlsl");
    files[mainfile] = m_Common.GenerateHLSLStub(shaderDetails, entryFunc);
  }

  if(files.empty())
    return;

  m_Common.EditShader(stage->stage, stage->Object, shaderDetails, entryFunc, files, mainfile);
}

void D3D12PipelineStateViewer::shaderSave_clicked()
{
  const D3D12Pipe::Shader *stage = stageForSender(qobject_cast<QWidget *>(QObject::sender()));

  if(stage == NULL)
    return;

  ShaderReflection *shaderDetails = stage->ShaderDetails;

  if(stage->Object == ResourceId())
    return;

  m_Common.SaveShaderFile(shaderDetails);
}

QVariantList D3D12PipelineStateViewer::exportViewHTML(const D3D12Pipe::View &view, bool rw,
                                                      const ShaderResource *shaderInput,
                                                      const QString &extraParams)
{
  QString name = tr("Empty");
  QString typeName = tr("Unknown");
  QString format = tr("Unknown");
  uint64_t w = 1;
  uint32_t h = 1, d = 1;
  uint32_t a = 0;

  QString viewFormat = ToQStr(view.Format.strname);

  TextureDescription *tex = m_Ctx.GetTexture(view.Resource);
  BufferDescription *buf = m_Ctx.GetBuffer(view.Resource);

  QString viewParams;

  // check to see if it's a texture
  if(tex)
  {
    w = tex->width;
    h = tex->height;
    d = tex->depth;
    a = tex->arraysize;
    format = ToQStr(tex->format.strname);
    name = ToQStr(tex->name);
    typeName = ToQStr(tex->resType);

    if(view.swizzle[0] != TextureSwizzle::Red || view.swizzle[1] != TextureSwizzle::Green ||
       view.swizzle[2] != TextureSwizzle::Blue || view.swizzle[3] != TextureSwizzle::Alpha)
    {
      format += tr(" swizzle[%1%2%3%4]")
                    .arg(ToQStr(view.swizzle[0]))
                    .arg(ToQStr(view.swizzle[1]))
                    .arg(ToQStr(view.swizzle[2]))
                    .arg(ToQStr(view.swizzle[3]));
    }

    if(tex->mips > 1)
      viewParams = tr("Highest Mip: %1, Num Mips: %2").arg(view.HighestMip).arg(view.NumMipLevels);

    if(tex->arraysize > 1)
    {
      if(!viewParams.isEmpty())
        viewParams += lit(", ");
      viewParams +=
          tr("First Slice: %1, Array Size: %2").arg(view.FirstArraySlice).arg(view.ArraySize);
    }

    if(view.MinLODClamp > 0.0f)
    {
      if(!viewParams.isEmpty())
        viewParams += lit(", ");
      viewParams += tr("MinLODClamp: %1").arg(view.MinLODClamp);
    }
  }

  // if not a texture, it must be a buffer
  if(buf)
  {
    w = buf->length;
    h = 0;
    d = 0;
    a = 0;
    format = ToQStr(view.Format.strname);
    name = ToQStr(buf->name);
    typeName = lit("Buffer");

    if(view.BufferFlags & D3DBufferViewFlags::Raw)
    {
      typeName = rw ? lit("RWByteAddressBuffer") : lit("ByteAddressBuffer");
    }
    else if(view.ElementSize > 0)
    {
      // for structured buffers, display how many 'elements' there are in the buffer
      typeName = QFormatStr("%1[%2]")
                     .arg(rw ? lit("RWStructuredBuffer") : lit("StructuredBuffer"))
                     .arg(buf->length / view.ElementSize);
    }

    if(view.BufferFlags & D3DBufferViewFlags::Append || view.BufferFlags & D3DBufferViewFlags::Counter)
    {
      typeName += tr(" (Count: %1)").arg(view.BufferStructCount);
    }

    if(shaderInput && !shaderInput->IsTexture)
    {
      if(view.Format.compType == CompType::Typeless)
      {
        if(shaderInput->variableType.members.count > 0)
          viewFormat = format = lit("struct ") + ToQStr(shaderInput->variableType.descriptor.name);
        else
          viewFormat = format = ToQStr(shaderInput->variableType.descriptor.name);
      }
      else
      {
        format = ToQStr(view.Format.strname);
      }
    }

    viewParams = tr("First Element: %1, Num Elements %2, Flags %3")
                     .arg(view.FirstElement)
                     .arg(view.NumElements)
                     .arg(ToQStr(view.BufferFlags));

    if(view.CounterResource != ResourceId())
    {
      QString counterName = tr("Buffer %1").arg(ToQStr(view.CounterResource));
      BufferDescription *counterBuf = m_Ctx.GetBuffer(view.CounterResource);
      if(counterBuf)
        counterName = ToQStr(counterBuf->name);
      viewParams += tr(", Counter in %1 at %2 bytes").arg(counterName).arg(view.CounterByteOffset);
    }
  }

  if(viewParams.isEmpty())
    viewParams = extraParams;
  else
    viewParams += lit(", ") + extraParams;

  return {name, ToQStr(view.Type), typeName, (qulonglong)w, h, d,
          a,    viewFormat,        format,   viewParams};
}

void D3D12PipelineStateViewer::exportHTML(QXmlStreamWriter &xml, D3D12Pipe::IA &ia)
{
  {
    xml.writeStartElement(lit("h3"));
    xml.writeCharacters(tr("Input Layouts"));
    xml.writeEndElement();

    QList<QVariantList> rows;

    int i = 0;
    for(const D3D12Pipe::Layout &l : ia.layouts)
    {
      rows.push_back({i, ToQStr(l.SemanticName), l.SemanticIndex, ToQStr(l.Format.strname),
                      l.InputSlot, l.ByteOffset, (bool)l.PerInstance, l.InstanceDataStepRate});

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
    for(const D3D12Pipe::VB &vb : ia.vbuffers)
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

      length = qMin(length, (uint64_t)vb.Size);

      rows.push_back({i, name, vb.Stride, (qulonglong)vb.Offset, (qulonglong)length});

      i++;
    }

    m_Common.exportHTMLTable(
        xml, {tr("Slot"), tr("Buffer"), tr("Stride"), tr("Offset"), tr("Byte Length")}, rows);
  }

  {
    xml.writeStartElement(lit("h3"));
    xml.writeCharacters(tr("Index Buffer"));
    xml.writeEndElement();

    QString name = tr("Buffer %1").arg(ToQStr(ia.ibuffer.Buffer));
    uint64_t length = 0;

    if(ia.ibuffer.Buffer == ResourceId())
    {
      name = tr("Empty");
    }
    else
    {
      BufferDescription *buf = m_Ctx.GetBuffer(ia.ibuffer.Buffer);
      if(buf)
      {
        name = ToQStr(buf->name);
        length = buf->length;
      }
    }

    length = qMin(length, (uint64_t)ia.ibuffer.Size);

    QString ifmt = lit("UNKNOWN");
    if(m_Ctx.CurDrawcall()->indexByteWidth == 2)
      ifmt = lit("R16_UINT");
    if(m_Ctx.CurDrawcall()->indexByteWidth == 4)
      ifmt = lit("R32_UINT");

    m_Common.exportHTMLTable(xml, {tr("Buffer"), tr("Format"), tr("Offset"), tr("Byte Length")},
                             {name, ifmt, (qulonglong)ia.ibuffer.Offset, (qulonglong)length});
  }

  xml.writeStartElement(lit("p"));
  xml.writeEndElement();

  m_Common.exportHTMLTable(xml, {tr("Primitive Topology")}, {ToQStr(m_Ctx.CurDrawcall()->topology)});
}

void D3D12PipelineStateViewer::exportHTML(QXmlStreamWriter &xml, D3D12Pipe::Shader &sh)
{
  ShaderReflection *shaderDetails = sh.ShaderDetails;

  {
    xml.writeStartElement(lit("h3"));
    xml.writeCharacters(tr("Shader"));
    xml.writeEndElement();

    QString shadername = tr("Unknown");

    const D3D12Pipe::State &state = m_Ctx.CurD3D12PipelineState();

    if(sh.Object == ResourceId())
      shadername = tr("Unbound");
    else if(state.customName)
      shadername =
          QFormatStr("%1 - %2").arg(ToQStr(state.name)).arg(m_Ctx.CurPipelineState().Abbrev(sh.stage));
    else
      shadername =
          tr("%1 - %2 Shader").arg(ToQStr(state.name)).arg(ToQStr(sh.stage, GraphicsAPI::D3D12));

    if(shaderDetails && !shaderDetails->DebugInfo.files.empty())
    {
      shadername = QFormatStr("%1() - %2")
                       .arg(ToQStr(shaderDetails->EntryPoint))
                       .arg(QFileInfo(ToQStr(shaderDetails->DebugInfo.files[0].first)).fileName());
    }

    xml.writeStartElement(lit("p"));
    xml.writeCharacters(shadername);
    xml.writeEndElement();

    if(sh.Object == ResourceId())
      return;
  }

  {
    xml.writeStartElement(lit("h3"));
    xml.writeCharacters(tr("Shader Resource Views"));
    xml.writeEndElement();

    QList<QVariantList> rows;

    for(int space = 0; space < sh.Spaces.count; space++)
    {
      for(int reg = 0; reg < sh.Spaces[space].SRVs.count; reg++)
      {
        const D3D12Pipe::View &v = sh.Spaces[space].SRVs[reg];

        // consider this register to not exist - it's in a gap defined by sparse root signature
        // elements
        if(v.RootElement == ~0U)
          continue;

        const ShaderResource *shaderInput = NULL;

        if(sh.ShaderDetails)
        {
          for(int i = 0; i < sh.BindpointMapping.ReadOnlyResources.count; i++)
          {
            const BindpointMap &b = sh.BindpointMapping.ReadOnlyResources[i];
            const ShaderResource &res = sh.ShaderDetails->ReadOnlyResources[i];

            bool regMatch = b.bind == reg;

            // handle unbounded arrays specially. It's illegal to have an unbounded array with
            // anything after it
            if(b.bind <= reg)
              regMatch = (b.arraySize == ~0U) || (b.bind + (int)b.arraySize > reg);

            if(b.bindset == space && regMatch && !res.IsReadOnly && !res.IsSampler)
            {
              shaderInput = &res;
              break;
            }
          }
        }

        QString rootel = v.Immediate ? tr("#%1 Direct").arg(v.RootElement)
                                     : tr("#%1 Table[%2]").arg(v.RootElement).arg(v.TableIndex);

        QVariantList row = exportViewHTML(v, false, shaderInput, QString());

        row.push_front(reg);
        row.push_front(space);
        row.push_front(rootel);

        rows.push_back(row);
      }
    }

    m_Common.exportHTMLTable(
        xml, {tr("Root Sig El"), tr("Space"), tr("Register"), tr("Resource"), tr("View Type"),
              tr("Resource Type"), tr("Width"), tr("Height"), tr("Depth"), tr("Array Size"),
              tr("View Format"), tr("Resource Format"), tr("View Parameters")},
        rows);
  }

  {
    xml.writeStartElement(lit("h3"));
    xml.writeCharacters(tr("Unordered Access Views"));
    xml.writeEndElement();

    QList<QVariantList> rows;

    for(int space = 0; space < sh.Spaces.count; space++)
    {
      for(int reg = 0; reg < sh.Spaces[space].UAVs.count; reg++)
      {
        const D3D12Pipe::View &v = sh.Spaces[space].UAVs[reg];

        // consider this register to not exist - it's in a gap defined by sparse root signature
        // elements
        if(v.RootElement == ~0U)
          continue;

        const ShaderResource *shaderInput = NULL;

        if(sh.ShaderDetails)
        {
          for(int i = 0; i < sh.BindpointMapping.ReadOnlyResources.count; i++)
          {
            const BindpointMap &b = sh.BindpointMapping.ReadOnlyResources[i];
            const ShaderResource &res = sh.ShaderDetails->ReadOnlyResources[i];

            bool regMatch = b.bind == reg;

            // handle unbounded arrays specially. It's illegal to have an unbounded array with
            // anything after it
            if(b.bind <= reg)
              regMatch = (b.arraySize == ~0U) || (b.bind + (int)b.arraySize > reg);

            if(b.bindset == space && regMatch && !res.IsReadOnly && !res.IsSampler)
            {
              shaderInput = &res;
              break;
            }
          }
        }

        QString rootel = v.Immediate ? tr("#%1 Direct").arg(v.RootElement)
                                     : tr("#%1 Table[%2]").arg(v.RootElement).arg(v.TableIndex);

        QVariantList row = exportViewHTML(v, true, shaderInput, QString());

        row.push_front(reg);
        row.push_front(space);
        row.push_front(rootel);

        rows.push_back(row);
      }
    }

    m_Common.exportHTMLTable(
        xml, {tr("Root Sig El"), tr("Space"), tr("Register"), tr("Resource"), tr("View Type"),
              tr("Resource Type"), tr("Width"), tr("Height"), tr("Depth"), tr("Array Size"),
              tr("View Format"), tr("Resource Format"), tr("View Parameters")},
        rows);
  }

  {
    xml.writeStartElement(lit("h3"));
    xml.writeCharacters(tr("Samplers"));
    xml.writeEndElement();

    QList<QVariantList> rows;

    for(int space = 0; space < sh.Spaces.count; space++)
    {
      for(int reg = 0; reg < sh.Spaces[space].Samplers.count; reg++)
      {
        const D3D12Pipe::Sampler &s = sh.Spaces[space].Samplers[reg];

        // consider this register to not exist - it's in a gap defined by sparse root signature
        // elements
        if(s.RootElement == ~0U)
          continue;

        const ShaderResource *shaderInput = NULL;

        if(sh.ShaderDetails)
        {
          for(int i = 0; i < sh.BindpointMapping.ReadOnlyResources.count; i++)
          {
            const BindpointMap &b = sh.BindpointMapping.ReadOnlyResources[i];
            const ShaderResource &res = sh.ShaderDetails->ReadOnlyResources[i];

            bool regMatch = b.bind == reg;

            // handle unbounded arrays specially. It's illegal to have an unbounded array with
            // anything after it
            if(b.bind <= reg)
              regMatch = (b.arraySize == ~0U) || (b.bind + (int)b.arraySize > reg);

            if(b.bindset == space && regMatch && res.IsSampler)
            {
              shaderInput = &res;
              break;
            }
          }
        }

        QString rootel = s.Immediate ? tr("#%1 Static").arg(s.RootElement)
                                     : tr("#%1 Table[%2]").arg(s.RootElement).arg(s.TableIndex);

        {
          QString regname = QString::number(reg);

          if(shaderInput && !shaderInput->name.empty())
            regname += lit(": ") + ToQStr(shaderInput->name);

          QString borderColor = QFormatStr("%1, %2, %3, %4")
                                    .arg(s.BorderColor[0])
                                    .arg(s.BorderColor[1])
                                    .arg(s.BorderColor[2])
                                    .arg(s.BorderColor[3]);

          QString addressing;

          QString addPrefix;
          QString addVal;

          QString addr[] = {ToQStr(s.AddressU), ToQStr(s.AddressV), ToQStr(s.AddressW)};

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

          QString filter = ToQStr(s.Filter);

          if(s.MaxAniso > 1)
            filter += QFormatStr(" %1x").arg(s.MaxAniso);

          if(s.Filter.func == FilterFunc::Comparison)
            filter += QFormatStr(" (%1)").arg(ToQStr(s.Comparison));
          else if(s.Filter.func != FilterFunc::Normal)
            filter += QFormatStr(" (%1)").arg(ToQStr(s.Filter.func));

          rows.push_back({rootel, space, regname, addressing, filter,
                          QFormatStr("%1 - %2")
                              .arg(s.MinLOD == -FLT_MAX ? lit("0") : QString::number(s.MinLOD))
                              .arg(s.MaxLOD == FLT_MAX ? lit("FLT_MAX") : QString::number(s.MaxLOD)),
                          s.MipLODBias});
        }
      }
    }

    m_Common.exportHTMLTable(xml, {tr("Root Sig El"), tr("Space"), tr("Register"), tr("Addressing"),
                                   tr("Filter"), tr("LOD Clamp"), tr("LOD Bias")},
                             rows);
  }

  {
    xml.writeStartElement(lit("h3"));
    xml.writeCharacters(tr("Constant Buffers"));
    xml.writeEndElement();

    QList<QVariantList> rows;

    for(int space = 0; space < sh.Spaces.count; space++)
    {
      for(int reg = 0; reg < sh.Spaces[space].ConstantBuffers.count; reg++)
      {
        const D3D12Pipe::CBuffer &b = sh.Spaces[space].ConstantBuffers[reg];

        const ConstantBlock *shaderCBuf = NULL;

        if(sh.ShaderDetails)
        {
          for(int i = 0; i < sh.BindpointMapping.ConstantBlocks.count; i++)
          {
            const BindpointMap &bm = sh.BindpointMapping.ConstantBlocks[i];
            const ConstantBlock &res = sh.ShaderDetails->ConstantBlocks[i];

            bool regMatch = bm.bind == reg;

            // handle unbounded arrays specially. It's illegal to have an unbounded array with
            // anything after it
            if(bm.bind <= reg)
              regMatch = (bm.arraySize == ~0U) || (bm.bind + (int)bm.arraySize > reg);

            if(bm.bindset == space && regMatch)
            {
              shaderCBuf = &res;
              break;
            }
          }
        }

        QString rootel;

        if(b.Immediate)
        {
          if(!b.RootValues.empty())
            rootel = tr("#%1 Consts").arg(b.RootElement);
          else
            rootel = tr("#%1 Direct").arg(b.RootElement);
        }
        else
        {
          rootel = tr("#%1 Table[%2]").arg(b.RootElement).arg(b.TableIndex);
        }

        {
          QString name = tr("Constant Buffer %1").arg(ToQStr(b.Buffer));
          uint64_t length = b.ByteSize;
          uint64_t offset = b.Offset;
          int numvars = shaderCBuf ? shaderCBuf->variables.count : 0;
          uint32_t bytesize = shaderCBuf ? shaderCBuf->byteSize : 0;

          if(b.Immediate && !b.RootValues.empty())
            bytesize = uint32_t(b.RootValues.count * 4);

          BufferDescription *buf = m_Ctx.GetBuffer(b.Buffer);

          if(buf)
            name = ToQStr(buf->name);
          else
            name = tr("Empty");

          QString regname = QString::number(reg);

          if(shaderCBuf && !shaderCBuf->name.empty())
            regname += lit(": ") + ToQStr(shaderCBuf->name);

          length = qMin(length, (uint64_t)bytesize);

          rows.push_back(
              {rootel, space, regname, name, (qulonglong)offset, (qulonglong)length, numvars});
        }
      }
    }

    m_Common.exportHTMLTable(xml,
                             {tr("Root Signature Index"), tr("Space"), tr("Register"), tr("Buffer"),
                              tr("Byte Offset"), tr("Byte Size"), tr("Number of Variables")},
                             rows);
  }
}

void D3D12PipelineStateViewer::exportHTML(QXmlStreamWriter &xml, D3D12Pipe::Streamout &so)
{
  {
    xml.writeStartElement(lit("h3"));
    xml.writeCharacters(tr("Stream Out Targets"));
    xml.writeEndElement();

    QList<QVariantList> rows;

    int i = 0;
    for(const D3D12Pipe::SOBind &o : so.Outputs)
    {
      QString name = tr("Buffer %1").arg(ToQStr(o.Buffer));
      uint64_t length = 0;
      QString counterName = tr("Buffer %1").arg(ToQStr(o.WrittenCountBuffer));
      uint64_t counterLength = 0;

      if(o.Buffer == ResourceId())
      {
        name = tr("Empty");
      }
      else
      {
        BufferDescription *buf = m_Ctx.GetBuffer(o.Buffer);
        if(buf)
        {
          name = ToQStr(buf->name);
          length = buf->length;
        }
      }

      if(o.WrittenCountBuffer == ResourceId())
      {
        counterName = tr("Empty");
      }
      else
      {
        BufferDescription *buf = m_Ctx.GetBuffer(o.WrittenCountBuffer);
        if(buf)
        {
          counterName = ToQStr(buf->name);
          counterLength = buf->length;
        }
      }

      length = qMin(length, o.Size);

      rows.push_back({i, name, (qulonglong)o.Offset, (qulonglong)length, counterName,
                      (qulonglong)o.WrittenCountOffset, (qulonglong)counterLength});

      i++;
    }

    m_Common.exportHTMLTable(
        xml, {tr("Slot"), tr("Buffer"), tr("Offset"), tr("Byte Length"), tr("Counter Buffer"),
              tr("Counter Offset"), tr("Counter Byte Length")},
        rows);
  }
}

void D3D12PipelineStateViewer::exportHTML(QXmlStreamWriter &xml, D3D12Pipe::Rasterizer &rs)
{
  {
    xml.writeStartElement(lit("h3"));
    xml.writeCharacters(tr("States"));
    xml.writeEndElement();

    m_Common.exportHTMLTable(xml, {tr("Fill Mode"), tr("Cull Mode"), tr("Front CCW")},
                             {ToQStr(rs.m_State.fillMode), ToQStr(rs.m_State.cullMode),
                              rs.m_State.FrontCCW ? tr("Yes") : tr("No")});

    xml.writeStartElement(lit("p"));
    xml.writeEndElement();

    m_Common.exportHTMLTable(
        xml, {tr("Line AA Enable"), tr("Multisample Enable"), tr("Forced Sample Count"),
              tr("Conservative Raster"), tr("Sample Mask")},
        {rs.m_State.AntialiasedLineEnable ? tr("Yes") : tr("No"),
         rs.m_State.MultisampleEnable ? tr("Yes") : tr("No"), rs.m_State.ForcedSampleCount,
         rs.m_State.ConservativeRasterization ? tr("Yes") : tr("No"),
         Formatter::Format(rs.SampleMask, true)});

    xml.writeStartElement(lit("p"));
    xml.writeEndElement();

    m_Common.exportHTMLTable(
        xml, {tr("Depth Clip"), tr("Depth Bias"), tr("Depth Bias Clamp"), tr("Slope Scaled Bias")},
        {rs.m_State.DepthClip ? tr("Yes") : tr("No"), rs.m_State.DepthBias,
         Formatter::Format(rs.m_State.DepthBiasClamp),
         Formatter::Format(rs.m_State.SlopeScaledDepthBias)});
  }

  {
    xml.writeStartElement(lit("h3"));
    xml.writeCharacters(tr("Viewports"));
    xml.writeEndElement();

    QList<QVariantList> rows;

    int i = 0;
    for(const D3D12Pipe::Viewport &v : rs.Viewports)
    {
      if(v.Width == v.Height && v.Width == 0 && v.Height == 0)
        continue;

      rows.push_back({i, v.X, v.Y, v.Width, v.Height, v.MinDepth, v.MaxDepth});

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
    for(const D3D12Pipe::Scissor &s : rs.Scissors)
    {
      if(s.right == 0 && s.bottom == 0)
        continue;

      rows.push_back({i, s.left, s.top, s.right - s.left, s.bottom - s.top});

      i++;
    }

    m_Common.exportHTMLTable(xml, {tr("Slot"), tr("X"), tr("Y"), tr("Width"), tr("Height")}, rows);
  }
}

void D3D12PipelineStateViewer::exportHTML(QXmlStreamWriter &xml, D3D12Pipe::OM &om)
{
  {
    xml.writeStartElement(lit("h3"));
    xml.writeCharacters(tr("Blend State"));
    xml.writeEndElement();

    QString blendFactor = QFormatStr("%1, %2, %3, %4")
                              .arg(om.m_BlendState.BlendFactor[0], 0, 'f', 2)
                              .arg(om.m_BlendState.BlendFactor[1], 0, 'f', 2)
                              .arg(om.m_BlendState.BlendFactor[2], 0, 'f', 2)
                              .arg(om.m_BlendState.BlendFactor[3], 0, 'f', 2);

    m_Common.exportHTMLTable(xml, {tr("Independent Blend Enable"), tr("Alpha to Coverage"),
                                   tr("Blend Factor"), tr("Multisampling Rate")},
                             {om.m_BlendState.IndependentBlend ? tr("Yes") : tr("No"),
                              om.m_BlendState.AlphaToCoverage ? tr("Yes") : tr("No"), blendFactor,
                              tr("%1x %2 qual").arg(om.multiSampleCount).arg(om.multiSampleQuality)});

    xml.writeStartElement(lit("h3"));
    xml.writeCharacters(tr("Target Blends"));
    xml.writeEndElement();

    QList<QVariantList> rows;

    int i = 0;
    for(const D3D12Pipe::Blend &b : om.m_BlendState.Blends)
    {
      if(i >= om.RenderTargets.count)
        continue;

      QString mask = QFormatStr("%1%2%3%4")
                         .arg((b.WriteMask & 0x1) == 0 ? lit("_") : lit("R"))
                         .arg((b.WriteMask & 0x2) == 0 ? lit("_") : lit("G"))
                         .arg((b.WriteMask & 0x4) == 0 ? lit("_") : lit("B"))
                         .arg((b.WriteMask & 0x8) == 0 ? lit("_") : lit("A"));

      rows.push_back({i, b.Enabled ? tr("Yes") : tr("No"), b.LogicEnabled ? tr("Yes") : tr("No"),
                      ToQStr(b.m_Blend.Source), ToQStr(b.m_Blend.Destination),
                      ToQStr(b.m_Blend.Operation), ToQStr(b.m_AlphaBlend.Source),
                      ToQStr(b.m_AlphaBlend.Destination), ToQStr(b.m_AlphaBlend.Operation),
                      ToQStr(b.Logic), mask});

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
        xml, {tr("Depth Test Enable"), tr("Depth Writes Enable"), tr("Depth Function")},
        {om.m_State.DepthEnable ? tr("Yes") : tr("No"),
         om.m_State.DepthWrites ? tr("Yes") : tr("No"), ToQStr(om.m_State.DepthFunc)});
  }

  {
    xml.writeStartElement(lit("h3"));
    xml.writeCharacters(tr("Stencil State"));
    xml.writeEndElement();

    m_Common.exportHTMLTable(
        xml, {tr("Stencil Test Enable"), tr("Stencil Read Mask"), tr("Stencil Write Mask")},
        {om.m_State.StencilEnable ? tr("Yes") : tr("No"),
         Formatter::Format(om.m_State.StencilReadMask, true),
         Formatter::Format(om.m_State.StencilWriteMask, true)});

    xml.writeStartElement(lit("p"));
    xml.writeEndElement();

    m_Common.exportHTMLTable(
        xml, {tr("Face"), tr("Function"), tr("Pass Operation"), tr("Fail Operation"),
              tr("Depth Fail Operation")},
        {
            {tr("Front"), ToQStr(om.m_State.m_FrontFace.Func), ToQStr(om.m_State.m_FrontFace.PassOp),
             ToQStr(om.m_State.m_FrontFace.FailOp), ToQStr(om.m_State.m_FrontFace.DepthFailOp)},
            {tr("Back"), ToQStr(om.m_State.m_BackFace.Func), ToQStr(om.m_State.m_BackFace.PassOp),
             ToQStr(om.m_State.m_BackFace.FailOp), ToQStr(om.m_State.m_BackFace.DepthFailOp)},
        });
  }

  {
    xml.writeStartElement(lit("h3"));
    xml.writeCharacters(tr("Render targets"));
    xml.writeEndElement();

    QList<QVariantList> rows;

    for(int i = 0; i < om.RenderTargets.count; i++)
    {
      if(om.RenderTargets[i].Resource == ResourceId())
        continue;

      QVariantList row = exportViewHTML(om.RenderTargets[i], false, NULL, QString());
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

    if(om.DepthReadOnly && om.StencilReadOnly)
      extra = tr("Depth & Stencil Read-Only");
    else if(om.DepthReadOnly)
      extra = tr("Depth Read-Only");
    else if(om.StencilReadOnly)
      extra = tr("Stencil Read-Only");

    m_Common.exportHTMLTable(xml,
                             {
                                 tr("Name"), tr("View Type"), tr("Resource Type"), tr("Width"),
                                 tr("Height"), tr("Depth"), tr("Array Size"), tr("View Format"),
                                 tr("Resource Format"), tr("View Parameters"),
                             },
                             {exportViewHTML(om.DepthTarget, false, NULL, extra)});
  }
}

void D3D12PipelineStateViewer::on_exportHTML_clicked()
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
        case 0: exportHTML(xml, m_Ctx.CurD3D12PipelineState().m_IA); break;
        case 1: exportHTML(xml, m_Ctx.CurD3D12PipelineState().m_VS); break;
        case 2: exportHTML(xml, m_Ctx.CurD3D12PipelineState().m_HS); break;
        case 3: exportHTML(xml, m_Ctx.CurD3D12PipelineState().m_DS); break;
        case 4:
          exportHTML(xml, m_Ctx.CurD3D12PipelineState().m_GS);
          exportHTML(xml, m_Ctx.CurD3D12PipelineState().m_SO);
          break;
        case 5: exportHTML(xml, m_Ctx.CurD3D12PipelineState().m_RS); break;
        case 6: exportHTML(xml, m_Ctx.CurD3D12PipelineState().m_PS); break;
        case 7: exportHTML(xml, m_Ctx.CurD3D12PipelineState().m_OM); break;
        case 8: exportHTML(xml, m_Ctx.CurD3D12PipelineState().m_CS); break;
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
