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

#include "D3D11PipelineStateViewer.h"
#include <float.h>
#include <QMenu>
#include <QMouseEvent>
#include <QScrollBar>
#include <QXmlStreamWriter>
#include "3rdparty/flowlayout/FlowLayout.h"
#include "3rdparty/toolwindowmanager/ToolWindowManager.h"
#include "Code/Resources.h"
#include "Widgets/Extended/RDHeaderView.h"
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
  D3D11ViewTag(ResType t, int i, const D3D11Pipe::View &r) : type(t), index(i), res(r) {}
  ResType type;
  int index;
  D3D11Pipe::View res;
};

Q_DECLARE_METATYPE(D3D11ViewTag);

D3D11PipelineStateViewer::D3D11PipelineStateViewer(ICaptureContext &ctx,
                                                   PipelineStateViewer &common, QWidget *parent)
    : QFrame(parent), ui(new Ui::D3D11PipelineStateViewer), m_Ctx(ctx), m_Common(common)
{
  ui->setupUi(this);

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
  }

  for(RDTreeWidget *cbuffer : cbuffers)
  {
    RDHeaderView *header = new RDHeaderView(Qt::Horizontal, this);
    cbuffer->setHeader(header);

    cbuffer->setColumns({tr("Slot"), tr("Buffer"), tr("Vec4 Range"), tr("Size"), tr("Go")});
    header->setColumnStretchHints({1, 2, 3, 3, -1});

    cbuffer->setHoverIconColumn(4, action, action_hover);
    cbuffer->setClearSelectionOnFocusLoss(true);
    cbuffer->setInstantTooltips(true);
  }

  for(RDTreeWidget *cl : classes)
  {
    RDHeaderView *header = new RDHeaderView(Qt::Horizontal, this);
    cl->setHeader(header);

    cl->setColumns({tr("Slot"), tr("Interface"), tr("Instance")});
    header->setColumnStretchHints({1, 1, 1});

    cl->setClearSelectionOnFocusLoss(true);
    cl->setInstantTooltips(true);
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

  ui->csUAVs->setFont(Formatter::PreferredFont());
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
  setState();
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

bool D3D11PipelineStateViewer::HasImportantViewParams(const D3D11Pipe::View &view,
                                                      TextureDescription *tex)
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
  if(view.viewFormat.compType != CompType::Typeless && view.viewFormat != tex->format)
    return true;

  return false;
}

bool D3D11PipelineStateViewer::HasImportantViewParams(const D3D11Pipe::View &view,
                                                      BufferDescription *buf)
{
  if(view.firstElement > 0 || view.numElements * view.elementByteSize < buf->length)
    return true;

  return false;
}

void D3D11PipelineStateViewer::setViewDetails(RDTreeWidgetItem *node, const D3D11ViewTag &view,
                                              TextureDescription *tex)
{
  if(tex == NULL)
    return;

  QString text;

  const D3D11Pipe::View &res = view.res;

  bool viewdetails = false;

  if(res.viewFormat != tex->format)
  {
    text += tr("The texture is format %1, the view treats it as %2.\n")
                .arg(tex->format.Name())
                .arg(res.viewFormat.Name());

    viewdetails = true;
  }

  if(view.type == D3D11ViewTag::OMDepth)
  {
    if(m_Ctx.CurD3D11PipelineState()->outputMerger.depthReadOnly)
      text += tr("Depth component is read-only\n");
    if(m_Ctx.CurD3D11PipelineState()->outputMerger.stencilReadOnly)
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

  text = text.trimmed();

  node->setToolTip(text);

  if(viewdetails)
  {
    node->setBackgroundColor(QColor(127, 255, 212));
    node->setForegroundColor(QColor(0, 0, 0));
  }
}

void D3D11PipelineStateViewer::setViewDetails(RDTreeWidgetItem *node, const D3D11ViewTag &view,
                                              BufferDescription *buf)
{
  if(buf == NULL)
    return;

  QString text;

  const D3D11Pipe::View &res = view.res;

  if((res.firstElement * res.elementByteSize) > 0 ||
     (res.numElements * res.elementByteSize) < buf->length)
  {
    text += tr("The view covers bytes %1-%2 (%3 elements).\nThe buffer is %4 bytes in length (%5 "
               "elements).")
                .arg(res.firstElement * res.elementByteSize)
                .arg((res.firstElement + res.numElements) * res.elementByteSize)
                .arg(res.numElements)
                .arg(buf->length)
                .arg(buf->length / res.elementByteSize);
  }
  else
  {
    return;
  }

  node->setToolTip(text);
  node->setBackgroundColor(QColor(127, 255, 212));
  node->setForegroundColor(QColor(0, 0, 0));
}

void D3D11PipelineStateViewer::addResourceRow(const D3D11ViewTag &view,
                                              const ShaderResource *shaderInput,
                                              const Bindpoint *map, RDTreeWidget *resources)
{
  const D3D11Pipe::View &r = view.res;

  bool viewDetails = false;

  if(view.type == D3D11ViewTag::OMDepth)
    viewDetails = m_Ctx.CurD3D11PipelineState()->outputMerger.depthReadOnly ||
                  m_Ctx.CurD3D11PipelineState()->outputMerger.stencilReadOnly;

  bool filledSlot = (r.resourceResourceId != ResourceId());
  bool usedSlot = (map && map->used);

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

    TextureDescription *tex = m_Ctx.GetTexture(r.resourceResourceId);

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

      if(tex->format != r.viewFormat)
        format = tr("Viewed as %1").arg(r.viewFormat.Name());

      if(HasImportantViewParams(r, tex))
        viewDetails = true;
    }

    BufferDescription *buf = m_Ctx.GetBuffer(r.resourceResourceId);

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
        typeName = QFormatStr("%1ByteAddressBuffer")
                       .arg(view.type == D3D11ViewTag::UAV ? lit("RW") : QString());
      }
      else if(r.elementByteSize > 0)
      {
        // for structured buffers, display how many 'elements' there are in the buffer
        typeName = QFormatStr("%1StructuredBuffer[%2]")
                       .arg(view.type == D3D11ViewTag::UAV ? lit("RW") : QString())
                       .arg(buf->length / r.elementByteSize);
      }

      if(r.bufferFlags & (D3DBufferViewFlags::Append | D3DBufferViewFlags::Counter))
      {
        typeName += tr(" (Count: %1)").arg(r.bufferStructCount);
      }

      // get the buffer type, whether it's just a basic type or a complex struct
      if(shaderInput && !shaderInput->isTexture)
      {
        if(r.viewFormat.compType == CompType::Typeless)
        {
          if(!shaderInput->variableType.members.empty())
            format = lit("struct ") + shaderInput->variableType.descriptor.name;
          else
            format = shaderInput->variableType.descriptor.name;
        }
        else
        {
          format = r.viewFormat.Name();
        }
      }

      if(HasImportantViewParams(r, buf))
        viewDetails = true;
    }

    QVariant name = r.resourceResourceId;

    if(viewDetails)
      name = tr("%1 viewed by %2").arg(ToQStr(r.resourceResourceId)).arg(ToQStr(r.viewResourceId));

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

bool D3D11PipelineStateViewer::showNode(bool usedSlot, bool filledSlot)
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

void D3D11PipelineStateViewer::setShaderState(const D3D11Pipe::Shader &stage, RDLabel *shader,
                                              RDTreeWidget *resources, RDTreeWidget *samplers,
                                              RDTreeWidget *cbuffers, RDTreeWidget *classes)
{
  ShaderReflection *shaderDetails = stage.reflection;
  const ShaderBindpointMapping &mapping = stage.bindpointMapping;

  QString shText = ToQStr(stage.resourceId);

  if(shaderDetails && !shaderDetails->debugInfo.files.empty())
  {
    shText += QFormatStr(": %1() - %2")
                  .arg(shaderDetails->entryPoint)
                  .arg(QFileInfo(shaderDetails->debugInfo.files[0].filename).fileName());
  }

  shader->setText(shText);

  int vs = 0;

  vs = resources->verticalScrollBar()->value();
  resources->beginUpdate();
  resources->clear();
  for(int i = 0; i < stage.srvs.count(); i++)
  {
    const ShaderResource *shaderInput = NULL;
    const Bindpoint *map = NULL;

    if(shaderDetails)
    {
      for(int b = 0; b < shaderDetails->readOnlyResources.count(); b++)
      {
        const ShaderResource &res = shaderDetails->readOnlyResources[b];
        const Bindpoint &bind = mapping.readOnlyResources[b];

        if(bind.bind == i)
        {
          shaderInput = &res;
          map = &bind;
          break;
        }
      }
    }

    addResourceRow(D3D11ViewTag(D3D11ViewTag::SRV, i, stage.srvs[i]), shaderInput, map, resources);
  }
  resources->clearSelection();
  resources->endUpdate();
  resources->verticalScrollBar()->setValue(vs);

  vs = samplers->verticalScrollBar()->value();
  samplers->beginUpdate();
  samplers->clear();
  for(int i = 0; i < stage.samplers.count(); i++)
  {
    const D3D11Pipe::Sampler &s = stage.samplers[i];

    const ShaderSampler *shaderInput = NULL;
    const Bindpoint *map = NULL;

    if(shaderDetails)
    {
      for(int b = 0; b < shaderDetails->samplers.count(); b++)
      {
        const ShaderSampler &res = shaderDetails->samplers[b];
        const Bindpoint &bind = mapping.samplers[b];

        if(bind.bind == i)
        {
          shaderInput = &res;
          map = &bind;
          break;
        }
      }
    }

    bool filledSlot = s.resourceId != ResourceId();
    bool usedSlot = (map && map->used);

    if(showNode(usedSlot, filledSlot))
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
          {slotname, s.resourceId, addressing, filter,
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

  samplers->clearSelection();
  samplers->endUpdate();
  samplers->verticalScrollBar()->setValue(vs);

  vs = cbuffers->verticalScrollBar()->value();
  cbuffers->beginUpdate();
  cbuffers->clear();
  for(int i = 0; i < stage.constantBuffers.count(); i++)
  {
    const D3D11Pipe::ConstantBuffer &b = stage.constantBuffers[i];

    const ConstantBlock *shaderCBuf = NULL;
    const Bindpoint *map = NULL;

    if(shaderDetails)
    {
      for(int cb = 0; cb < shaderDetails->constantBlocks.count(); cb++)
      {
        const ConstantBlock &cbuf = shaderDetails->constantBlocks[cb];
        const Bindpoint &bind = mapping.constantBlocks[cb];

        if(bind.bind == i)
        {
          shaderCBuf = &cbuf;
          map = &bind;
          break;
        }
      }
    }

    bool filledSlot = b.resourceId != ResourceId();
    bool usedSlot = (map && map->used);

    if(showNode(usedSlot, filledSlot))
    {
      ulong length = 0;
      int numvars = shaderCBuf ? shaderCBuf->variables.count() : 0;
      uint32_t bytesize = shaderCBuf ? shaderCBuf->byteSize : 0;

      BufferDescription *buf = m_Ctx.GetBuffer(b.resourceId);

      if(buf)
        length = buf->length;

      QString slotname = QString::number(i);

      if(shaderCBuf && !shaderCBuf->name.empty())
        slotname += lit(": ") + shaderCBuf->name;

      QString sizestr;
      if(bytesize == (uint32_t)length)
        sizestr = tr("%1 Variables, %2 bytes").arg(numvars).arg(length);
      else
        sizestr =
            tr("%1 Variables, %2 bytes needed, %3 provided").arg(numvars).arg(bytesize).arg(length);

      if(length < bytesize)
        filledSlot = false;

      QString vecrange = QFormatStr("%1 - %2").arg(b.vecOffset).arg(b.vecOffset + b.vecCount);

      RDTreeWidgetItem *node =
          new RDTreeWidgetItem({slotname, b.resourceId, vecrange, sizestr, QString()});

      node->setTag(QVariant::fromValue(i));

      if(!filledSlot)
        setEmptyRow(node);

      if(!usedSlot)
        setInactiveRow(node);

      cbuffers->addTopLevelItem(node);
    }
  }
  cbuffers->clearSelection();
  cbuffers->endUpdate();
  cbuffers->verticalScrollBar()->setValue(vs);

  vs = classes->verticalScrollBar()->value();
  classes->beginUpdate();
  classes->clear();
  for(int i = 0; i < stage.classInstances.count(); i++)
  {
    QString interfaceName = lit("Interface %1").arg(i);

    if(shaderDetails && i < shaderDetails->interfaces.count())
      interfaceName = shaderDetails->interfaces[i];

    classes->addTopLevelItem(new RDTreeWidgetItem({i, interfaceName, stage.classInstances[i]}));
  }
  classes->clearSelection();
  classes->endUpdate();
  classes->verticalScrollBar()->setValue(vs);

  classes->parentWidget()->setVisible(!stage.classInstances.empty());
}

void D3D11PipelineStateViewer::setState()
{
  if(!m_Ctx.IsCaptureLoaded())
  {
    clearState();
    return;
  }

  const D3D11Pipe::State &state = *m_Ctx.CurD3D11PipelineState();
  const DrawcallDescription *draw = m_Ctx.CurDrawcall();

  const QPixmap &tick = Pixmaps::tick(this);
  const QPixmap &cross = Pixmaps::cross(this);

  ////////////////////////////////////////////////
  // Vertex Input

  if(state.inputAssembly.bytecode)
  {
    QString layout = ToQStr(state.inputAssembly.resourceId);

    if(state.inputAssembly.bytecode && !state.inputAssembly.bytecode->debugInfo.files.empty())
    {
      layout +=
          QFormatStr(": %1() - %2")
              .arg(state.inputAssembly.bytecode->entryPoint)
              .arg(QFileInfo(state.inputAssembly.bytecode->debugInfo.files[0].filename).fileName());
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
        if(IA[i].compType != VS[i].compType)
          mismatchDetails +=
              tr("IA bytecode semantic %1 (%2) is %4).arg(VS bytecode semantic %1 (%3) is %5\n")
                  .arg(i)
                  .arg(IAname)
                  .arg(VSname)
                  .arg(ToQStr(IA[i].compType))
                  .arg(ToQStr(VS[i].compType));
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
      QString byteOffs = QString::number(l.byteOffset);

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
      uint64_t length = 0;

      BufferDescription *buf = m_Ctx.GetBuffer(state.inputAssembly.indexBuffer.resourceId);

      if(buf)
        length = buf->length;

      RDTreeWidgetItem *node = new RDTreeWidgetItem(
          {tr("Index"), state.inputAssembly.indexBuffer.resourceId, draw ? draw->indexByteWidth : 0,
           state.inputAssembly.indexBuffer.byteOffset, (qulonglong)length, QString()});

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

      node->setTag(
          QVariant::fromValue(D3D11VBIBTag(state.inputAssembly.indexBuffer.resourceId,
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

      node->setTag(
          QVariant::fromValue(D3D11VBIBTag(state.inputAssembly.indexBuffer.resourceId,
                                           state.inputAssembly.indexBuffer.byteOffset +
                                               (draw ? draw->indexOffset * draw->indexByteWidth : 0),
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
        node = new RDTreeWidgetItem({i, v.resourceId, v.byteStride, v.byteOffset, length, QString()});
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

  setShaderState(state.vertexShader, ui->vsShader, ui->vsResources, ui->vsSamplers, ui->vsCBuffers,
                 ui->vsClasses);
  setShaderState(state.geometryShader, ui->gsShader, ui->gsResources, ui->gsSamplers,
                 ui->gsCBuffers, ui->gsClasses);
  setShaderState(state.hullShader, ui->hsShader, ui->hsResources, ui->hsSamplers, ui->hsCBuffers,
                 ui->hsClasses);
  setShaderState(state.domainShader, ui->dsShader, ui->dsResources, ui->dsSamplers, ui->dsCBuffers,
                 ui->dsClasses);
  setShaderState(state.pixelShader, ui->psShader, ui->psResources, ui->psSamplers, ui->psCBuffers,
                 ui->psClasses);
  setShaderState(state.computeShader, ui->csShader, ui->csResources, ui->csSamplers, ui->csCBuffers,
                 ui->csClasses);

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

  vs = ui->csUAVs->verticalScrollBar()->value();
  ui->csUAVs->beginUpdate();
  ui->csUAVs->clear();
  for(int i = 0; i < state.computeShader.uavs.count(); i++)
  {
    const ShaderResource *shaderInput = NULL;
    const Bindpoint *map = NULL;

    const D3D11Pipe::Shader &cs = state.computeShader;

    if(cs.reflection)
    {
      for(int b = 0; b < cs.reflection->readWriteResources.count(); b++)
      {
        const ShaderResource &res = cs.reflection->readWriteResources[b];
        const Bindpoint &bind = cs.bindpointMapping.readWriteResources[b];

        if(bind.bind == i)
        {
          shaderInput = &res;
          map = &bind;
          break;
        }
      }
    }

    addResourceRow(D3D11ViewTag(D3D11ViewTag::UAV, i, state.computeShader.uavs[i]), shaderInput,
                   map, ui->csUAVs);
  }
  ui->csUAVs->clearSelection();
  ui->csUAVs->endUpdate();
  ui->csUAVs->verticalScrollBar()->setValue(vs);

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

      RDTreeWidgetItem *node =
          new RDTreeWidgetItem({i, s.resourceId, length, s.byteOffset, QString()});

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

    if(s.enabled || ui->showEmpty->isChecked())
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

  bool targets[32] = {};

  vs = ui->targetOutputs->verticalScrollBar()->value();
  ui->targetOutputs->beginUpdate();
  ui->targetOutputs->clear();
  {
    for(int i = 0; i < state.outputMerger.renderTargets.count(); i++)
    {
      addResourceRow(D3D11ViewTag(D3D11ViewTag::OMTarget, i, state.outputMerger.renderTargets[i]),
                     NULL, NULL, ui->targetOutputs);

      if(state.outputMerger.renderTargets[i].resourceResourceId != ResourceId())
        targets[i] = true;
    }

    for(int i = 0; i < state.outputMerger.uavs.count(); i++)
    {
      const ShaderResource *shaderInput = NULL;
      const Bindpoint *map = NULL;

      // any non-CS shader can use these. When that's not supported (Before feature level 11.1)
      // this search will just boil down to only PS.
      // When multiple stages use the UAV, we allow the last stage to 'win' and define its type,
      // although it would be very surprising if the types were actually different anyway.
      const D3D11Pipe::Shader *nonCS[] = {&state.vertexShader, &state.domainShader, &state.hullShader,
                                          &state.geometryShader, &state.pixelShader};
      for(const D3D11Pipe::Shader *stage : nonCS)
      {
        if(stage->reflection)
        {
          for(int b = 0; b < stage->reflection->readWriteResources.count(); b++)
          {
            const ShaderResource &res = stage->reflection->readWriteResources[b];
            const Bindpoint &bind = stage->bindpointMapping.readWriteResources[b];

            if(bind.bind == i + (int)state.outputMerger.uavStartSlot)
            {
              shaderInput = &res;
              map = &bind;
              break;
            }
          }
        }
      }
      addResourceRow(D3D11ViewTag(D3D11ViewTag::UAV, i, state.outputMerger.uavs[i]), shaderInput,
                     map, ui->targetOutputs);
    }

    addResourceRow(D3D11ViewTag(D3D11ViewTag::OMDepth, 0, state.outputMerger.depthTarget), NULL,
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

  ui->depthEnabled->setPixmap(state.outputMerger.depthStencilState.depthEnable ? tick : cross);
  ui->depthFunc->setText(ToQStr(state.outputMerger.depthStencilState.depthFunction));
  ui->depthWrite->setPixmap(state.outputMerger.depthStencilState.depthWrites ? tick : cross);

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
  if(state.computeShader.reflection && draw && (draw->flags & DrawFlags::Dispatch))
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

  if(tag.canConvert<ResourceId>())
  {
    ResourceId id = tag.value<ResourceId>();
    tex = m_Ctx.GetTexture(id);
    buf = m_Ctx.GetBuffer(id);
  }
  else if(tag.canConvert<D3D11ViewTag>())
  {
    D3D11ViewTag view = tag.value<D3D11ViewTag>();
    tex = m_Ctx.GetTexture(view.res.resourceResourceId);
    buf = m_Ctx.GetBuffer(view.res.resourceResourceId);
  }

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
  else if(buf)
  {
    D3D11ViewTag view;

    view.res.resourceResourceId = buf->resourceId;

    if(tag.canConvert<D3D11ViewTag>())
      view = tag.value<D3D11ViewTag>();

    uint64_t offs = 0;
    uint64_t size = buf->length;

    if(view.res.resourceResourceId != ResourceId())
    {
      offs = uint64_t(view.res.firstElement) * view.res.elementByteSize;
      size = uint64_t(view.res.numElements) * view.res.elementByteSize;
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

    int bind = view.index;

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

      bind += state.outputMerger.uavStartSlot;

      for(const D3D11Pipe::Shader *searchstage : nonCS)
      {
        if(searchstage->reflection)
        {
          for(const ShaderResource &res : searchstage->reflection->readWriteResources)
          {
            if(!res.isTexture && res.bindPoint == bind)
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

      const rdcarray<Bindpoint> &bindArray = view.type == D3D11ViewTag::SRV
                                                 ? stage->bindpointMapping.readOnlyResources
                                                 : stage->bindpointMapping.readWriteResources;

      for(const ShaderResource &res : resArray)
      {
        if(!res.isTexture && res.bindPoint < bindArray.count() &&
           bindArray[res.bindPoint].bind == bind)
        {
          shaderRes = &res;
          break;
        }
      }
    }

    if(shaderRes)
    {
      format = m_Common.GenerateBufferFormatter(*shaderRes, view.res.viewFormat, offs);

      if(view.res.bufferFlags & D3DBufferViewFlags::Raw)
        format = lit("xint");
    }

    IBufferViewer *viewer = m_Ctx.ViewBuffer(offs, size, view.res.resourceResourceId, format);

    m_Ctx.AddDockWindow(viewer->Widget(), DockReference::AddTo, this);
  }
}

void D3D11PipelineStateViewer::cbuffer_itemActivated(RDTreeWidgetItem *item, int column)
{
  const D3D11Pipe::Shader *stage = stageForSender(item->treeWidget());

  if(stage == NULL)
    return;

  QVariant tag = item->tag();

  if(!tag.canConvert<int>())
    return;

  int cb = tag.value<int>();

  int cbufIdx = -1;

  for(int i = 0; i < stage->bindpointMapping.constantBlocks.count(); i++)
  {
    if(stage->bindpointMapping.constantBlocks[i].bind == cb)
    {
      cbufIdx = i;
      break;
    }
  }

  if(cbufIdx == -1)
  {
    // unused cbuffer, open regular buffer viewer
    if(cb >= stage->constantBuffers.count())
      return;

    const D3D11Pipe::ConstantBuffer &bind = stage->constantBuffers[cb];

    IBufferViewer *viewer = m_Ctx.ViewBuffer(bind.vecOffset * sizeof(float) * 4,
                                             bind.vecCount * sizeof(float) * 4, bind.resourceId);

    m_Ctx.AddDockWindow(viewer->Widget(), DockReference::AddTo, this);
    return;
  }

  IConstantBufferPreviewer *prev = m_Ctx.ViewConstantBuffer(stage->stage, cbufIdx, 0);

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

QVariantList D3D11PipelineStateViewer::exportViewHTML(const D3D11Pipe::View &view, int i,
                                                      ShaderReflection *refl,
                                                      const QString &extraParams)
{
  const ShaderResource *shaderInput = NULL;

  bool rw = false;

  if(refl)
  {
    for(const ShaderResource &bind : refl->readOnlyResources)
    {
      if(bind.bindPoint == i)
      {
        shaderInput = &bind;
        break;
      }
    }
    for(const ShaderResource &bind : refl->readWriteResources)
    {
      if(bind.bindPoint == i)
      {
        shaderInput = &bind;
        rw = true;
        break;
      }
    }
  }

  QString name = view.resourceResourceId == ResourceId()
                     ? tr("Empty")
                     : QString(m_Ctx.GetResourceName(view.resourceResourceId));
  QString typeName = tr("Unknown");
  QString format = tr("Unknown");
  uint64_t w = 1;
  uint32_t h = 1, d = 1;
  uint32_t a = 0;

  QString viewFormat = view.viewFormat.Name();

  TextureDescription *tex = m_Ctx.GetTexture(view.resourceResourceId);
  BufferDescription *buf = m_Ctx.GetBuffer(view.resourceResourceId);

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
    format = view.viewFormat.Name();
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
  }

  if(viewParams.isEmpty())
    viewParams = extraParams;
  else
    viewParams += lit(", ") + extraParams;

  return {i, name, ToQStr(view.type), typeName, (qulonglong)w, h,
          d, a,    viewFormat,        format,   viewParams};
}

void D3D11PipelineStateViewer::exportHTML(QXmlStreamWriter &xml, const D3D11Pipe::InputAssembly &ia)
{
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
    if(m_Ctx.CurDrawcall()->indexByteWidth == 2)
      ifmt = lit("R16_UINT");
    if(m_Ctx.CurDrawcall()->indexByteWidth == 4)
      ifmt = lit("R32_UINT");

    m_Common.exportHTMLTable(xml, {tr("Buffer"), tr("Format"), tr("Offset"), tr("Byte Length")},
                             {name, ifmt, ia.indexBuffer.byteOffset, (qulonglong)length});
  }

  xml.writeStartElement(lit("p"));
  xml.writeEndElement();

  m_Common.exportHTMLTable(xml, {tr("Primitive Topology")}, {ToQStr(m_Ctx.CurDrawcall()->topology)});
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

  {
    xml.writeStartElement(lit("h3"));
    xml.writeCharacters(tr("Resources"));
    xml.writeEndElement();

    QList<QVariantList> rows;

    for(int i = 0; i < sh.srvs.count(); i++)
    {
      if(sh.srvs[i].viewResourceId == ResourceId())
        continue;

      rows.push_back(exportViewHTML(sh.srvs[i], i, shaderDetails, QString()));
    }

    m_Common.exportHTMLTable(xml,
                             {
                                 tr("Slot"), tr("Name"), tr("View Type"), tr("Resource Type"),
                                 tr("Width"), tr("Height"), tr("Depth"), tr("Array Size"),
                                 tr("View Format"), tr("Resource Format"), tr("View Parameters"),
                             },
                             rows);
  }

  if(sh.stage == ShaderStage::Compute)
  {
    xml.writeStartElement(lit("h3"));
    xml.writeCharacters(tr("Unordered Access Views"));
    xml.writeEndElement();

    QList<QVariantList> rows;

    for(int i = 0; i < sh.uavs.count(); i++)
    {
      if(sh.uavs[i].viewResourceId == ResourceId())
        continue;

      rows.push_back(exportViewHTML(sh.uavs[i], i, shaderDetails, QString()));
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
    xml.writeCharacters(tr("Samplers"));
    xml.writeEndElement();

    QList<QVariantList> rows;

    for(int i = 0; i < sh.samplers.count(); i++)
    {
      const D3D11Pipe::Sampler &s = sh.samplers[i];

      if(s.resourceId == ResourceId())
        continue;

      QString borderColor = QFormatStr("%1, %2, %3, %4")
                                .arg(s.borderColor[0])
                                .arg(s.borderColor[1])
                                .arg(s.borderColor[2])
                                .arg(s.borderColor[3]);

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
                      s.maxLOD == FLT_MAX ? lit("FLT_MAX") : QString::number(s.maxLOD), s.mipLODBias});
    }

    m_Common.exportHTMLTable(
        xml,
        {
            tr("Slot"), tr("Addressing"), tr("Border Colour"), tr("Comparison"), tr("Filter"),
            tr("Max Anisotropy"), tr("Min LOD"), tr("Max LOD"), tr("Mip Bias"),
        },
        rows);
  }

  {
    xml.writeStartElement(lit("h3"));
    xml.writeCharacters(tr("Constant Buffers"));
    xml.writeEndElement();

    QList<QVariantList> rows;

    for(int i = 0; i < sh.constantBuffers.count(); i++)
    {
      ConstantBlock *shaderCBuf = NULL;

      if(sh.constantBuffers[i].resourceId == ResourceId())
        continue;

      if(shaderDetails && i < shaderDetails->constantBlocks.count() &&
         !shaderDetails->constantBlocks[i].name.isEmpty())
        shaderCBuf = &shaderDetails->constantBlocks[i];

      QString name = m_Ctx.GetResourceName(sh.constantBuffers[i].resourceId);
      uint64_t length = 1;
      int numvars = shaderCBuf ? shaderCBuf->variables.count() : 0;
      uint32_t byteSize = shaderCBuf ? shaderCBuf->byteSize : 0;

      if(sh.constantBuffers[i].resourceId == ResourceId())
      {
        name = tr("Empty");
        length = 0;
      }

      BufferDescription *buf = m_Ctx.GetBuffer(sh.constantBuffers[i].resourceId);
      if(buf)
        length = buf->length;

      rows.push_back({i, name, sh.constantBuffers[i].vecOffset, sh.constantBuffers[i].vecCount,
                      numvars, byteSize, (qulonglong)length});
    }

    m_Common.exportHTMLTable(xml,
                             {tr("Slot"), tr("Buffer"), tr("Vector Offset"), tr("Vector Count"),
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
                                 tr("Slot"), tr("Interface Name"), tr("Instance Name"),
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
        xml, {tr("Scissor Enable"), tr("Line AA Enable"), tr("Multisample Enable"),
              tr("Forced Sample Count"), tr("Conservative Raster")},
        {rs.state.scissorEnable ? tr("Yes") : tr("No"),
         rs.state.antialiasedLines ? tr("Yes") : tr("No"),
         rs.state.multisampleEnable ? tr("Yes") : tr("No"), rs.state.forcedSampleCount,
         rs.state.conservativeRasterization != ConservativeRaster::Disabled ? tr("Yes")
                                                                            : tr("No")});

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

    m_Common.exportHTMLTable(xml, {tr("Slot"), tr("X"), tr("Y"), tr("Width"), tr("Height"),
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

    m_Common.exportHTMLTable(xml, {tr("Independent Blend Enable"), tr("Alpha to Coverage"),
                                   tr("Sample Mask"), tr("Blend Factor")},
                             {
                                 om.blendState.independentBlend ? tr("Yes") : tr("No"),
                                 om.blendState.alphaToCoverage ? tr("Yes") : tr("No"),
                                 Formatter::Format(om.blendState.sampleMask, true), blendFactor,
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
      if(om.renderTargets[i].viewResourceId == ResourceId())
        continue;

      rows.push_back(exportViewHTML(om.renderTargets[i], i, NULL, QString()));
    }

    m_Common.exportHTMLTable(xml,
                             {
                                 tr("Slot"), tr("Name"), tr("View Type"), tr("Resource Type"),
                                 tr("Width"), tr("Height"), tr("Depth"), tr("Array Size"),
                                 tr("View Format"), tr("Resource Format"), tr("View Parameters"),
                             },
                             rows);
  }

  if(!om.uavs.isEmpty() && om.uavs[0].viewResourceId != ResourceId())
  {
    xml.writeStartElement(lit("h3"));
    xml.writeCharacters(tr("Unordered Access Views"));
    xml.writeEndElement();

    QList<QVariantList> rows;

    uint32_t i = 0;

    for(; i < om.uavStartSlot; i++)
      rows.push_back({i, tr("Empty"), QString(), QString(), QString(), QString(), 0, 0, 0, 0,
                      QString(), QString(), QString()});

    for(; i < (uint32_t)om.renderTargets.count(); i++)
    {
      if(om.uavs[i - om.uavStartSlot].viewResourceId == ResourceId())
        continue;

      rows.push_back(exportViewHTML(om.uavs[i - om.uavStartSlot], i,
                                    m_Ctx.CurD3D11PipelineState()->pixelShader.reflection, QString()));
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

    QList<QVariantList> rows;

    QString extra;

    if(om.depthReadOnly && om.stencilReadOnly)
      extra = tr("Depth & Stencil Read-Only");
    else if(om.depthReadOnly)
      extra = tr("Depth Read-Only");
    else if(om.stencilReadOnly)
      extra = tr("Stencil Read-Only");

    rows.push_back(exportViewHTML(om.depthTarget, 0, NULL, extra));

    m_Common.exportHTMLTable(xml,
                             {
                                 tr("Slot"), tr("Name"), tr("View Type"), tr("Resource Type"),
                                 tr("Width"), tr("Height"), tr("Depth"), tr("Array Size"),
                                 tr("View Format"), tr("Resource Format"), tr("View Parameters"),
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

void D3D11PipelineStateViewer::on_debugThread_clicked()
{
  if(!m_Ctx.IsCaptureLoaded())
    return;

  const DrawcallDescription *draw = m_Ctx.CurDrawcall();

  if(!draw)
    return;

  ShaderReflection *shaderDetails = m_Ctx.CurD3D11PipelineState()->computeShader.reflection;
  const ShaderBindpointMapping &bindMapping =
      m_Ctx.CurD3D11PipelineState()->computeShader.bindpointMapping;

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

    if(trace->states.isEmpty())
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
      m_Ctx.DebugShader(&bindMapping, shaderDetails, ResourceId(), trace, debugContext);

  m_Ctx.AddDockWindow(s->Widget(), DockReference::AddTo, this);
}
