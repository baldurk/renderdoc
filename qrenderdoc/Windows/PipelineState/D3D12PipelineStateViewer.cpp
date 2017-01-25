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
#include <QScrollBar>
#include "3rdparty/toolwindowmanager/ToolWindowManager.h"
#include "Windows/BufferViewer.h"
#include "Windows/ConstantBufferPreviewer.h"
#include "Windows/MainWindow.h"
#include "Windows/ShaderViewer.h"
#include "Windows/TextureViewer.h"
#include "ui_D3D12PipelineStateViewer.h"

Q_DECLARE_METATYPE(ResourceId);

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

struct CBufTag
{
  CBufTag()
  {
    idx = ~0U;
    space = reg = 0;
  }
  CBufTag(uint32_t s, uint32_t r)
  {
    idx = ~0U;
    space = s;
    reg = r;
  }
  CBufTag(uint32_t i)
  {
    idx = i;
    space = reg = 0;
  }

  uint32_t idx, space, reg;
};

Q_DECLARE_METATYPE(CBufTag);

struct ViewTag
{
  enum ResType
  {
    SRV,
    UAV,
    OMTarget,
    OMDepth,
  };

  ViewTag() {}
  ViewTag(ResType t, int s, int r, const D3D12PipelineState::ResourceView &rs)
      : type(t), space(s), reg(r), res(rs)
  {
  }

  ResType type;
  int space, reg;
  D3D12PipelineState::ResourceView res;
};

Q_DECLARE_METATYPE(ViewTag);

D3D12PipelineStateViewer::D3D12PipelineStateViewer(CaptureContext *ctx, QWidget *parent)
    : QFrame(parent), ui(new Ui::D3D12PipelineStateViewer), m_Ctx(ctx)
{
  ui->setupUi(this);

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

  for(QToolButton *b : viewButtons)
    QObject::connect(b, &QToolButton::clicked, this, &D3D12PipelineStateViewer::shaderView_clicked);

  for(RDLabel *b : shaderLabels)
    QObject::connect(b, &RDLabel::clicked, this, &D3D12PipelineStateViewer::shaderView_clicked);

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

  addGridLines(ui->rasterizerGridLayout);
  addGridLines(ui->blendStateGridLayout);
  addGridLines(ui->depthStateGridLayout);

  // no way to set this up in the UI :(
  {
    // Slot | Semantic | Index | Format | Input Slot | Offset | Class | Step Rate | Go
    ui->iaLayouts->header()->resizeSection(0, 75);
    ui->iaLayouts->header()->setSectionResizeMode(0, QHeaderView::Interactive);
    ui->iaLayouts->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    ui->iaLayouts->header()->setSectionResizeMode(2, QHeaderView::Stretch);
    ui->iaLayouts->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    ui->iaLayouts->header()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    ui->iaLayouts->header()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    ui->iaLayouts->header()->setSectionResizeMode(6, QHeaderView::ResizeToContents);
    ui->iaLayouts->header()->setSectionResizeMode(7, QHeaderView::ResizeToContents);
    ui->iaLayouts->header()->setSectionResizeMode(8, QHeaderView::ResizeToContents);

    ui->iaLayouts->setHoverIconColumn(8);
  }

  {
    // Slot | Buffer | Stride | Offset | Byte Length | Go
    ui->iaBuffers->header()->resizeSection(0, 75);
    ui->iaBuffers->header()->setSectionResizeMode(0, QHeaderView::Interactive);
    ui->iaBuffers->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    ui->iaBuffers->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    ui->iaBuffers->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    ui->iaBuffers->header()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    ui->iaBuffers->header()->setSectionResizeMode(5, QHeaderView::ResizeToContents);

    ui->iaBuffers->setHoverIconColumn(5);
  }

  for(RDTreeWidget *res : resources)
  {
    // Root Sig El | Space | Register | Resource | Type | Width | Height | Depth | Array Size |
    // Format | Go
    res->header()->resizeSection(0, 100);
    res->header()->resizeSection(1, 40);
    res->header()->resizeSection(2, 120);
    res->header()->setSectionResizeMode(0, QHeaderView::Interactive);
    res->header()->setSectionResizeMode(1, QHeaderView::Interactive);
    res->header()->setSectionResizeMode(2, QHeaderView::Interactive);
    res->header()->setSectionResizeMode(3, QHeaderView::Stretch);
    res->header()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    res->header()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    res->header()->setSectionResizeMode(6, QHeaderView::ResizeToContents);
    res->header()->setSectionResizeMode(7, QHeaderView::ResizeToContents);
    res->header()->setSectionResizeMode(8, QHeaderView::ResizeToContents);
    res->header()->setSectionResizeMode(9, QHeaderView::ResizeToContents);
    res->header()->setSectionResizeMode(10, QHeaderView::ResizeToContents);

    res->setHoverIconColumn(10);
    res->setDefaultHoverColor(ui->targetOutputs->palette().color(QPalette::Window));
  }

  for(RDTreeWidget *uav : uavs)
  {
    // Root Sig El | Space | Register | Resource | Type | Width | Height | Depth | Array Size |
    // Format | Go
    uav->header()->resizeSection(0, 100);
    uav->header()->resizeSection(1, 40);
    uav->header()->resizeSection(2, 120);
    uav->header()->setSectionResizeMode(0, QHeaderView::Interactive);
    uav->header()->setSectionResizeMode(1, QHeaderView::Interactive);
    uav->header()->setSectionResizeMode(2, QHeaderView::Interactive);
    uav->header()->setSectionResizeMode(3, QHeaderView::Stretch);
    uav->header()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    uav->header()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    uav->header()->setSectionResizeMode(6, QHeaderView::ResizeToContents);
    uav->header()->setSectionResizeMode(7, QHeaderView::ResizeToContents);
    uav->header()->setSectionResizeMode(8, QHeaderView::ResizeToContents);
    uav->header()->setSectionResizeMode(9, QHeaderView::ResizeToContents);
    uav->header()->setSectionResizeMode(10, QHeaderView::ResizeToContents);

    uav->setHoverIconColumn(10);
    uav->setDefaultHoverColor(ui->targetOutputs->palette().color(QPalette::Window));
  }

  for(RDTreeWidget *samp : samplers)
  {
    // Root Sig El | Space | Register | Addressing | Filter | LOD Clamp | LOD Bias
    samp->header()->resizeSection(0, 100);
    samp->header()->resizeSection(1, 40);
    samp->header()->resizeSection(2, 120);
    samp->header()->setSectionResizeMode(0, QHeaderView::Interactive);
    samp->header()->setSectionResizeMode(1, QHeaderView::Interactive);
    samp->header()->setSectionResizeMode(2, QHeaderView::Interactive);
    samp->header()->setSectionResizeMode(3, QHeaderView::Stretch);
    samp->header()->setSectionResizeMode(4, QHeaderView::Stretch);
    samp->header()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    samp->header()->setSectionResizeMode(6, QHeaderView::ResizeToContents);
  }

  for(RDTreeWidget *cbuffer : cbuffers)
  {
    // Root Sig El | Space | Register | Buffer | Byte Range | Size | Go
    cbuffer->header()->resizeSection(0, 100);
    cbuffer->header()->resizeSection(1, 40);
    cbuffer->header()->resizeSection(2, 120);
    cbuffer->header()->setSectionResizeMode(0, QHeaderView::Interactive);
    cbuffer->header()->setSectionResizeMode(1, QHeaderView::Interactive);
    cbuffer->header()->setSectionResizeMode(2, QHeaderView::Interactive);
    cbuffer->header()->setSectionResizeMode(3, QHeaderView::Stretch);
    cbuffer->header()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    cbuffer->header()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    cbuffer->header()->setSectionResizeMode(6, QHeaderView::ResizeToContents);

    cbuffer->setHoverIconColumn(6);
    cbuffer->setDefaultHoverColor(ui->targetOutputs->palette().color(QPalette::Window));
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
    ui->scissors->header()->setSectionResizeMode(4, QHeaderView::Stretch);
  }

  {
    // Slot | Resource | Type | Width | Height | Depth | Array Size | Format | Go
    ui->targetOutputs->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    ui->targetOutputs->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    ui->targetOutputs->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    ui->targetOutputs->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    ui->targetOutputs->header()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    ui->targetOutputs->header()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    ui->targetOutputs->header()->setSectionResizeMode(6, QHeaderView::ResizeToContents);
    ui->targetOutputs->header()->setSectionResizeMode(7, QHeaderView::ResizeToContents);
    ui->targetOutputs->header()->setSectionResizeMode(8, QHeaderView::ResizeToContents);

    ui->targetOutputs->setHoverIconColumn(8);
    ui->targetOutputs->setDefaultHoverColor(ui->targetOutputs->palette().color(QPalette::Window));
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
    // Face | Func | Fail Op | Depth Fail Op | Pass Op
    ui->stencils->header()->resizeSection(0, 50);
    ui->stencils->header()->setSectionResizeMode(0, QHeaderView::Interactive);
    ui->stencils->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    ui->stencils->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    ui->stencils->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    ui->stencils->header()->setSectionResizeMode(4, QHeaderView::Stretch);
  }

  // this is often changed just because we're changing some tab in the designer.
  ui->stagesTabs->setCurrentIndex(0);

  // reset everything back to defaults
  clearState();
}

D3D12PipelineStateViewer::~D3D12PipelineStateViewer()
{
  delete ui;
}

void D3D12PipelineStateViewer::OnLogfileLoaded()
{
  OnEventChanged(m_Ctx->CurEvent());
}

void D3D12PipelineStateViewer::OnLogfileClosed()
{
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

void D3D12PipelineStateViewer::setInactiveRow(QTreeWidgetItem *node)
{
  for(int i = 0; i < node->columnCount(); i++)
  {
    QFont f = node->font(i);
    f.setItalic(true);
    node->setFont(i, f);
  }
}

void D3D12PipelineStateViewer::setEmptyRow(QTreeWidgetItem *node)
{
  for(int i = 0; i < node->columnCount(); i++)
    node->setBackgroundColor(i, QColor(255, 70, 70));
}

bool D3D12PipelineStateViewer::HasImportantViewParams(const D3D12PipelineState::ResourceView &view,
                                                      FetchTexture *tex)
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
  if(view.Format.compType != eCompType_None && view.Format != tex->format)
    return true;

  return false;
}

bool D3D12PipelineStateViewer::HasImportantViewParams(const D3D12PipelineState::ResourceView &view,
                                                      FetchBuffer *buf)
{
  if(view.FirstElement > 0 || view.NumElements * view.ElementSize < buf->length)
    return true;

  return false;
}

void D3D12PipelineStateViewer::setViewDetails(QTreeWidgetItem *node, const ViewTag &view,
                                              FetchTexture *tex)
{
  if(tex == NULL)
    return;

  QString text;

  const D3D12PipelineState::ResourceView &res = view.res;

  bool viewdetails = false;

  for(const D3D12PipelineState::ResourceData &im : m_Ctx->CurD3D12PipelineState.Resources)
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

  if(view.space == ViewTag::OMDepth)
  {
    if(m_Ctx->CurD3D12PipelineState.m_OM.DepthReadOnly)
      text += tr("Depth component is read-only\n");
    if(m_Ctx->CurD3D12PipelineState.m_OM.StencilReadOnly)
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

void D3D12PipelineStateViewer::setViewDetails(QTreeWidgetItem *node, const ViewTag &view,
                                              FetchBuffer *buf)
{
  if(buf == NULL)
    return;

  QString text;

  const D3D12PipelineState::ResourceView &res = view.res;

  for(const D3D12PipelineState::ResourceData &im : m_Ctx->CurD3D12PipelineState.Resources)
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

void D3D12PipelineStateViewer::addResourceRow(const ViewTag &view,
                                              const D3D12PipelineState::ShaderStage *stage,
                                              RDTreeWidget *resources)
{
  QIcon action(QPixmap(QString::fromUtf8(":/Resources/action.png")));
  QIcon action_hover(QPixmap(QString::fromUtf8(":/Resources/action_hover.png")));

  const D3D12PipelineState::ResourceView &r = view.res;
  bool uav = view.type == ViewTag::UAV;

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

  if(view.space == ViewTag::OMDepth)
    viewDetails = m_Ctx->CurD3D12PipelineState.m_OM.DepthReadOnly ||
                  m_Ctx->CurD3D12PipelineState.m_OM.StencilReadOnly;

  QString rootel = r.Immediate ? QString("#%1 Direct").arg(r.RootElement)
                               : QString("#%1 Table[%2]").arg(r.RootElement).arg(r.TableIndex);

  bool filledSlot = (r.Resource != ResourceId());
  bool usedSlot = (bind && bind->used);

  // if a target is set to RTVs or DSV, it is implicitly used
  if(filledSlot)
    usedSlot = usedSlot || view.space == ViewTag::OMTarget || view.space == ViewTag::OMDepth;

  if(showNode(usedSlot, filledSlot))
  {
    QString regname = QString::number(view.reg);

    if(shaderInput && !shaderInput->name.empty())
      regname += ": " + ToQStr(shaderInput->name);

    if(view.space == ViewTag::OMDepth)
      regname = "Depth";

    uint32_t w = 1, h = 1, d = 1;
    uint32_t a = 1;
    QString format = "Unknown";
    QString name = QString("Shader Resource %1").arg(ToQStr(r.Resource));
    QString typeName = "Unknown";

    if(!filledSlot)
    {
      name = "Empty";
      format = "-";
      typeName = "-";
      w = h = d = a = 0;
    }

    FetchTexture *tex = m_Ctx->GetTexture(r.Resource);

    if(tex)
    {
      w = tex->width;
      h = tex->height;
      d = tex->depth;
      a = tex->arraysize;
      format = ToQStr(tex->format.strname);
      name = tex->name;
      typeName = ToQStr(tex->resType);

      if(tex->resType == eResType_Texture2DMS || tex->resType == eResType_Texture2DMSArray)
      {
        typeName += QString(" %1x").arg(tex->msSamp);
      }

      if(tex->format != r.Format)
        format = tr("Viewed as %1").arg(ToQStr(r.Format.strname));

      if(HasImportantViewParams(r, tex))
        viewDetails = true;
    }

    FetchBuffer *buf = m_Ctx->GetBuffer(r.Resource);

    if(buf)
    {
      w = buf->length;
      h = 0;
      d = 0;
      a = 0;
      format = "";
      name = buf->name;
      typeName = "Buffer";

      if(r.BufferFlags & RawBuffer)
      {
        typeName = QString("%1ByteAddressBuffer").arg(uav ? "RW" : "");
      }
      else if(r.ElementSize > 0)
      {
        // for structured buffers, display how many 'elements' there are in the buffer
        a = buf->length / r.ElementSize;
        typeName = QString("%1StructuredBuffer[%2]").arg(uav ? "RW" : "").arg(a);
      }

      if(r.CounterResource != ResourceId())
      {
        typeName += QString(" (Count: %1)").arg(r.BufferStructCount);
      }

      // get the buffer type, whether it's just a basic type or a complex struct
      if(shaderInput && !shaderInput->IsTexture)
      {
        if(!shaderInput->variableType.members.empty())
          format = "struct " + ToQStr(shaderInput->variableType.descriptor.name);
        else if(r.Format.compType == eCompType_None)
          format = ToQStr(shaderInput->variableType.descriptor.name);
        else
          format = r.Format.strname;
      }

      if(HasImportantViewParams(r, buf))
        viewDetails = true;
    }

    QTreeWidgetItem *node =
        makeTreeNode({rootel, view.space, regname, name, typeName, w, h, d, a, format, ""});

    node->setData(0, Qt::UserRole, QVariant::fromValue(view));

    resources->setHoverIcons(node, action, action_hover);

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

const D3D12PipelineState::ShaderStage *D3D12PipelineStateViewer::stageForSender(QWidget *widget)
{
  if(!m_Ctx->LogLoaded())
    return NULL;

  while(widget)
  {
    if(widget == ui->stagesTabs->widget(0))
      return &m_Ctx->CurD3D12PipelineState.m_VS;
    if(widget == ui->stagesTabs->widget(1))
      return &m_Ctx->CurD3D12PipelineState.m_VS;
    if(widget == ui->stagesTabs->widget(2))
      return &m_Ctx->CurD3D12PipelineState.m_HS;
    if(widget == ui->stagesTabs->widget(3))
      return &m_Ctx->CurD3D12PipelineState.m_DS;
    if(widget == ui->stagesTabs->widget(4))
      return &m_Ctx->CurD3D12PipelineState.m_GS;
    if(widget == ui->stagesTabs->widget(5))
      return &m_Ctx->CurD3D12PipelineState.m_PS;
    if(widget == ui->stagesTabs->widget(6))
      return &m_Ctx->CurD3D12PipelineState.m_PS;
    if(widget == ui->stagesTabs->widget(7))
      return &m_Ctx->CurD3D12PipelineState.m_PS;
    if(widget == ui->stagesTabs->widget(8))
      return &m_Ctx->CurD3D12PipelineState.m_CS;

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
  ui->topology->setText("");
  ui->topologyDiagram->setPixmap(QPixmap());

  clearShaderState(ui->vsShader, ui->vsResources, ui->vsSamplers, ui->vsCBuffers, ui->vsUAVs);
  clearShaderState(ui->gsShader, ui->gsResources, ui->gsSamplers, ui->gsCBuffers, ui->gsUAVs);
  clearShaderState(ui->hsShader, ui->hsResources, ui->hsSamplers, ui->hsCBuffers, ui->hsUAVs);
  clearShaderState(ui->dsShader, ui->dsResources, ui->dsSamplers, ui->dsCBuffers, ui->dsUAVs);
  clearShaderState(ui->psShader, ui->psResources, ui->psSamplers, ui->psCBuffers, ui->psUAVs);
  clearShaderState(ui->csShader, ui->csResources, ui->csSamplers, ui->csCBuffers, ui->csUAVs);

  QPixmap tick(QString::fromUtf8(":/Resources/tick.png"));
  QPixmap cross(QString::fromUtf8(":/Resources/cross.png"));

  ui->fillMode->setText(tr("Solid", "Fill Mode"));
  ui->cullMode->setText(tr("Front", "Cull Mode"));
  ui->frontCCW->setPixmap(tick);
  ui->conservativeRaster->setPixmap(cross);

  ui->depthBias->setText("0.0");
  ui->depthBiasClamp->setText("0.0");
  ui->slopeScaledBias->setText("0.0");
  ui->forcedSampleCount->setText("0");

  ui->depthClip->setPixmap(tick);
  ui->multisample->setPixmap(tick);
  ui->lineAA->setPixmap(tick);
  ui->sampleMask->setText("FFFFFFFF");

  ui->independentBlend->setPixmap(cross);
  ui->alphaToCoverage->setPixmap(tick);

  ui->blendFactor->setText("0.00, 0.00, 0.00, 0.00");

  ui->viewports->clear();
  ui->scissors->clear();

  ui->targetOutputs->clear();
  ui->blends->clear();

  ui->depthEnabled->setPixmap(tick);
  ui->depthFunc->setText("GREATER_EQUAL");
  ui->depthWrite->setPixmap(tick);

  ui->stencilEnabled->setPixmap(cross);
  ui->stencilReadMask->setText("FF");
  ui->stencilWriteMask->setText("FF");
  ui->stencilRef->setText("FF");

  ui->stencils->clear();
}

void D3D12PipelineStateViewer::setShaderState(const D3D12PipelineState::ShaderStage &stage,
                                              QLabel *shader, RDTreeWidget *resources,
                                              RDTreeWidget *samplers, RDTreeWidget *cbuffers,
                                              RDTreeWidget *uavs)
{
  ShaderReflection *shaderDetails = stage.ShaderDetails;
  const D3D12PipelineState &state = m_Ctx->CurD3D12PipelineState;

  QIcon action(QPixmap(QString::fromUtf8(":/Resources/action.png")));
  QIcon action_hover(QPixmap(QString::fromUtf8(":/Resources/action_hover.png")));

  if(stage.Shader == ResourceId())
    shader->setText(tr("Unbound Shader"));
  else if(state.customName)
    shader->setText(ToQStr(state.PipelineName) + " - " + m_Ctx->CurPipelineState.Abbrev(stage.stage));
  else
    shader->setText(ToQStr(state.PipelineName) + " - " + ToQStr(stage.stage, eGraphicsAPI_D3D12) +
                    " Shader");

  if(shaderDetails && !shaderDetails->DebugInfo.entryFunc.empty() &&
     !shaderDetails->DebugInfo.files.empty())
  {
    QString shaderfn;

    int entryFile = shaderDetails->DebugInfo.entryFile;
    if(entryFile < 0 || entryFile >= shaderDetails->DebugInfo.files.count)
      entryFile = 0;

    shaderfn = QFileInfo(ToQStr(shaderDetails->DebugInfo.files[entryFile].first)).fileName();

    shader->setText(ToQStr(shaderDetails->DebugInfo.entryFunc) + "()" + " - " + shaderfn);
  }

  int vs = 0;

  vs = resources->verticalScrollBar()->value();
  resources->setUpdatesEnabled(false);
  resources->clear();
  for(int space = 0; space < stage.Spaces.count; space++)
  {
    for(int reg = 0; reg < stage.Spaces[space].SRVs.count; reg++)
    {
      addResourceRow(ViewTag(ViewTag::SRV, space, reg, stage.Spaces[space].SRVs[reg]), &stage,
                     resources);
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
      addResourceRow(ViewTag(ViewTag::UAV, space, reg, stage.Spaces[space].UAVs[reg]), &stage, uavs);
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
      const D3D12PipelineState::Sampler &s = stage.Spaces[space].Samplers[reg];

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

      QString rootel = s.Immediate ? QString("#%1 Static").arg(s.RootElement)
                                   : QString("#%1 Table[%2]").arg(s.RootElement).arg(s.TableIndex);

      bool filledSlot = (!s.AddressU.empty());
      bool usedSlot = (bind && bind->used);

      if(showNode(usedSlot, filledSlot))
      {
        QString regname = QString::number(reg);

        if(shaderInput && !shaderInput->name.empty())
          regname += ": " + ToQStr(shaderInput->name);

        QString borderColor =
            QString::number(s.BorderColor[0]) + ", " + QString::number(s.BorderColor[1]) + ", " +
            QString::number(s.BorderColor[2]) + ", " + QString::number(s.BorderColor[3]);

        QString addressing = "";

        QString addPrefix = "";
        QString addVal = "";

        QString addr[] = {ToQStr(s.AddressU), ToQStr(s.AddressV), ToQStr(s.AddressW)};

        // arrange like either UVW: WRAP or UV: WRAP, W: CLAMP
        for(int a = 0; a < 3; a++)
        {
          const QString str[] = {"U", "V", "W"};
          QString prefix = str[a];

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

        if(s.UseBorder)
          addressing += QString("<%1>").arg(borderColor);

        QString filter = s.Filter;

        if(s.MaxAniso > 1)
          filter += QString(" %1x").arg(s.MaxAniso);

        if(s.UseComparison)
          filter = QString(" (%1)").arg(ToQStr(s.Comparison));

        QTreeWidgetItem *node =
            makeTreeNode({rootel, space, regname, addressing, filter,
                          (s.MinLOD == -FLT_MAX ? "0" : QString::number(s.MinLOD)) + " - " +
                              (s.MaxLOD == FLT_MAX ? "FLT_MAX" : QString::number(s.MaxLOD)),
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
      const D3D12PipelineState::CBuffer &b = stage.Spaces[space].ConstantBuffers[reg];

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
            tag = QVariant::fromValue(CBufTag(i));
            break;
          }
        }
      }

      if(!tag.isValid())
        tag = QVariant::fromValue(CBufTag(space, reg));

      QString rootel;

      if(b.Immediate)
      {
        if(!b.RootValues.empty())
          rootel = QString("#%1 Consts").arg(b.RootElement);
        else
          rootel = QString("#%1 Direct").arg(b.RootElement);
      }
      else
      {
        rootel = QString("#%1 Table[%2]").arg(b.RootElement).arg(b.TableIndex);
      }

      bool filledSlot = (b.Buffer != ResourceId());
      if(b.Immediate && !b.RootValues.empty())
        filledSlot = true;

      bool usedSlot = (bind && bind->used);

      if(showNode(usedSlot, filledSlot))
      {
        QString name = QString("Constant Buffer %1").arg(ToQStr(b.Buffer));
        ulong length = b.ByteSize;
        uint64_t offset = b.Offset;
        int numvars = shaderCBuf ? shaderCBuf->variables.count : 0;
        uint32_t bytesize = shaderCBuf ? shaderCBuf->byteSize : 0;

        if(b.Immediate && !b.RootValues.empty())
          bytesize = uint32_t(b.RootValues.count * 4);

        if(!filledSlot)
          name = "Empty";

        FetchBuffer *buf = m_Ctx->GetBuffer(b.Buffer);

        if(buf)
          name = buf->name;

        QString regname = QString::number(reg);

        if(shaderCBuf && !shaderCBuf->name.empty())
          regname += ": " + ToQStr(shaderCBuf->name);

        QString sizestr;
        if(bytesize == (uint32_t)length)
          sizestr = tr("%1 Variables, %2 bytes").arg(numvars).arg(length);
        else
          sizestr =
              tr("%1 Variables, %2 bytes needed, %3 provided").arg(numvars).arg(bytesize).arg(length);

        if(length < bytesize)
          filledSlot = false;

        QTreeWidgetItem *node =
            makeTreeNode({rootel, (uint64_t)space, regname, name, offset, sizestr, ""});

        node->setData(0, Qt::UserRole, tag);

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
  if(!m_Ctx->LogLoaded())
  {
    clearState();
    return;
  }

  const D3D12PipelineState &state = m_Ctx->CurD3D12PipelineState;
  const FetchDrawcall *draw = m_Ctx->CurDrawcall();

  QPixmap tick(QString::fromUtf8(":/Resources/tick.png"));
  QPixmap cross(QString::fromUtf8(":/Resources/cross.png"));

  QIcon action(QPixmap(QString::fromUtf8(":/Resources/action.png")));
  QIcon action_hover(QPixmap(QString::fromUtf8(":/Resources/action_hover.png")));

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
    for(const D3D12PipelineState::InputAssembler::LayoutInput &l : state.m_IA.layouts)
    {
      QString byteOffs = QString::number(l.ByteOffset);

      // D3D12 specific value
      if(l.ByteOffset == ~0U)
      {
        byteOffs = QString("APPEND_ALIGNED (%1)").arg(layoutOffs[l.InputSlot]);
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
        QTreeWidgetItem *node = makeTreeNode(
            {i, ToQStr(l.SemanticName), l.SemanticIndex, ToQStr(l.Format.strname), l.InputSlot,
             byteOffs, l.PerInstance ? "PER_INSTANCE" : "PER_VERTEX", l.InstanceDataStepRate, ""});

        if(usedSlot)
          usedVBuffers[l.InputSlot] = true;

        ui->iaLayouts->setHoverIcons(node, action, action_hover);

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

  PrimitiveTopology topo = draw ? draw->topology : eTopology_Unknown;

  if(topo > eTopology_PatchList)
  {
    int numCPs = (int)topo - (int)eTopology_PatchList_1CPs + 1;

    ui->topology->setText(QString("PatchList (%1 Control Points)").arg(numCPs));
  }
  else
  {
    ui->topology->setText(ToQStr(topo));
  }

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

  bool ibufferUsed = draw && (draw->flags & eDraw_UseIBuffer);

  vs = ui->iaBuffers->verticalScrollBar()->value();
  ui->iaBuffers->setUpdatesEnabled(false);
  ui->iaBuffers->clear();

  if(state.m_IA.ibuffer.Buffer != ResourceId())
  {
    if(ibufferUsed || ui->showDisabled->isChecked())
    {
      QString name = "Buffer " + ToQStr(state.m_IA.ibuffer.Buffer);
      uint64_t length = 1;

      if(!ibufferUsed)
        length = 0;

      FetchBuffer *buf = m_Ctx->GetBuffer(state.m_IA.ibuffer.Buffer);

      if(buf)
      {
        name = buf->name;
        length = buf->length;
      }

      QTreeWidgetItem *node = makeTreeNode({"Index", name, draw ? draw->indexByteWidth : 0,
                                            state.m_IA.ibuffer.Offset, (qulonglong)length, ""});

      ui->iaBuffers->setHoverIcons(node, action, action_hover);

      node->setData(0, Qt::UserRole, QVariant::fromValue(VBIBTag(state.m_IA.ibuffer.Buffer,
                                                                 draw ? draw->indexOffset : 0)));

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
      QTreeWidgetItem *node = makeTreeNode({"Index", tr("No Buffer Set"), "-", "-", "-", ""});

      ui->iaBuffers->setHoverIcons(node, action, action_hover);

      node->setData(0, Qt::UserRole, QVariant::fromValue(VBIBTag(state.m_IA.ibuffer.Buffer,
                                                                 draw ? draw->indexOffset : 0)));

      setEmptyRow(node);

      if(!ibufferUsed)
        setInactiveRow(node);

      ui->iaBuffers->addTopLevelItem(node);
    }
  }

  m_VBNodes.clear();

  for(int i = 0; i < state.m_IA.vbuffers.count; i++)
  {
    const D3D12PipelineState::InputAssembler::VertexBuffer &v = state.m_IA.vbuffers[i];

    bool filledSlot = (v.Buffer != ResourceId());
    bool usedSlot = (usedVBuffers[i]);

    if(showNode(usedSlot, filledSlot))
    {
      QString name = "Buffer " + ToQStr(v.Buffer);
      uint64_t length = 1;

      if(!filledSlot)
      {
        name = "Empty";
        length = 0;
      }

      FetchBuffer *buf = m_Ctx->GetBuffer(v.Buffer);
      if(buf)
      {
        name = buf->name;
        length = buf->length;
      }

      QTreeWidgetItem *node = NULL;

      if(filledSlot)
        node = makeTreeNode({i, name, v.Stride, v.Offset, length, ""});
      else
        node = makeTreeNode({i, "No Buffer Set", "-", "-", "-", ""});

      ui->iaBuffers->setHoverIcons(node, action, action_hover);

      node->setData(0, Qt::UserRole, QVariant::fromValue(VBIBTag(v.Buffer, v.Offset)));

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
    const D3D12PipelineState::Streamout::Output &s = state.m_SO.Outputs[i];

    bool filledSlot = (s.Buffer != ResourceId());
    bool usedSlot = (filledSlot);

    if(showNode(usedSlot, filledSlot))
    {
      QString name = "Buffer " + ToQStr(s.Buffer);
      uint64_t length = 0;

      if(!filledSlot)
      {
        name = "Empty";
      }

      FetchBuffer *buf = m_Ctx->GetBuffer(s.Buffer);

      if(buf)
      {
        name = buf->name;
        if(length == 0)
          length = buf->length;
      }

      QTreeWidgetItem *node = makeTreeNode({i, name, length, s.Offset, ""});

      ui->gsStreamOut->setHoverIcons(node, action, action_hover);

      node->setData(0, Qt::UserRole, QVariant::fromValue(s.Buffer));

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
    const D3D12PipelineState::Rasterizer::Viewport &v = state.m_RS.Viewports[i];

    QTreeWidgetItem *node =
        makeTreeNode({i, v.TopLeft[0], v.TopLeft[1], v.Width, v.Height, v.MinDepth, v.MaxDepth});

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
    const D3D12PipelineState::Rasterizer::Scissor &s = state.m_RS.Scissors[i];

    QTreeWidgetItem *node = makeTreeNode({i, s.left, s.top, s.right - s.left, s.bottom - s.top});

    if(s.right == s.left || s.bottom == s.top)
      setEmptyRow(node);

    ui->scissors->addTopLevelItem(node);
  }
  ui->scissors->clearSelection();
  ui->scissors->verticalScrollBar()->setValue(vs);
  ui->scissors->setUpdatesEnabled(true);

  ui->fillMode->setText(ToQStr(state.m_RS.m_State.FillMode));
  ui->cullMode->setText(ToQStr(state.m_RS.m_State.CullMode));
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
      addResourceRow(ViewTag(ViewTag::OMTarget, 0, i, state.m_OM.RenderTargets[i]), NULL,
                     ui->targetOutputs);

      if(state.m_OM.RenderTargets[i].Resource != ResourceId())
        targets[i] = true;
    }

    addResourceRow(ViewTag(ViewTag::OMDepth, 0, 0, state.m_OM.DepthTarget), NULL, ui->targetOutputs);
  }
  ui->targetOutputs->clearSelection();
  ui->targetOutputs->setUpdatesEnabled(true);
  ui->targetOutputs->verticalScrollBar()->setValue(vs);

  vs = ui->blends->verticalScrollBar()->value();
  ui->blends->setUpdatesEnabled(false);
  ui->blends->clear();
  {
    int i = 0;
    for(const D3D12PipelineState::OutputMerger::BlendState::RTBlend &blend :
        state.m_OM.m_BlendState.Blends)
    {
      bool filledSlot = (blend.Enabled || targets[i]);
      bool usedSlot = (targets[i]);

      if(showNode(usedSlot, filledSlot))
      {
        QTreeWidgetItem *node = NULL;

        node =
            makeTreeNode({i, blend.Enabled ? tr("True") : tr("False"),
                          blend.LogicEnabled ? tr("True") : tr("False"),

                          ToQStr(blend.m_Blend.Source), ToQStr(blend.m_Blend.Destination),
                          ToQStr(blend.m_Blend.Operation),

                          ToQStr(blend.m_AlphaBlend.Source), ToQStr(blend.m_AlphaBlend.Destination),
                          ToQStr(blend.m_AlphaBlend.Operation),

                          ToQStr(blend.LogicOp),

                          QString("%1%2%3%4")
                              .arg((blend.WriteMask & 0x1) == 0 ? "_" : "R")
                              .arg((blend.WriteMask & 0x2) == 0 ? "_" : "G")
                              .arg((blend.WriteMask & 0x4) == 0 ? "_" : "B")
                              .arg((blend.WriteMask & 0x8) == 0 ? "_" : "A")});

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

  ui->blendFactor->setText(QString("%1, %2, %3, %4")
                               .arg(state.m_OM.m_BlendState.BlendFactor[0], 2)
                               .arg(state.m_OM.m_BlendState.BlendFactor[1], 2)
                               .arg(state.m_OM.m_BlendState.BlendFactor[2], 2)
                               .arg(state.m_OM.m_BlendState.BlendFactor[3], 2));

  ui->depthEnabled->setPixmap(state.m_OM.m_State.DepthEnable ? tick : cross);
  ui->depthFunc->setText(ToQStr(state.m_OM.m_State.DepthFunc));
  ui->depthWrite->setPixmap(state.m_OM.m_State.DepthWrites ? tick : cross);

  ui->stencilEnabled->setPixmap(state.m_OM.m_State.StencilEnable ? tick : cross);
  ui->stencilReadMask->setText(
      QString("%1").arg(state.m_OM.m_State.StencilReadMask, 2, 16, QChar('0')).toUpper());
  ui->stencilWriteMask->setText(
      QString("%1").arg(state.m_OM.m_State.StencilWriteMask, 2, 16, QChar('0')).toUpper());
  ui->stencilRef->setText(
      QString("%1").arg(state.m_OM.m_State.StencilRef, 2, 16, QChar('0')).toUpper());

  ui->stencils->setUpdatesEnabled(false);
  ui->stencils->clear();
  ui->stencils->addTopLevelItems({makeTreeNode({"Front", ToQStr(state.m_OM.m_State.m_FrontFace.Func),
                                                ToQStr(state.m_OM.m_State.m_FrontFace.FailOp),
                                                ToQStr(state.m_OM.m_State.m_FrontFace.DepthFailOp),
                                                ToQStr(state.m_OM.m_State.m_FrontFace.PassOp)}),
                                  makeTreeNode({"Back", ToQStr(state.m_OM.m_State.m_BackFace.Func),
                                                ToQStr(state.m_OM.m_State.m_BackFace.FailOp),
                                                ToQStr(state.m_OM.m_State.m_BackFace.DepthFailOp),
                                                ToQStr(state.m_OM.m_State.m_BackFace.PassOp)})});
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
        state.HS.Shader != ResourceId(),
        state.DS.Shader != ResourceId(),
        state.GS.Shader != ResourceId(),
        true,
        state.PS.Shader != ResourceId(),
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

QString D3D12PipelineStateViewer::formatMembers(int indent, const QString &nameprefix,
                                                const rdctype::array<ShaderConstant> &vars)
{
  QString indentstr(indent * 4, QChar(' '));

  QString ret = "";

  int i = 0;

  for(const ShaderConstant &v : vars)
  {
    if(!v.type.members.empty())
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

void D3D12PipelineStateViewer::resource_itemActivated(QTreeWidgetItem *item, int column)
{
  const D3D12PipelineState::ShaderStage *stage = stageForSender(item->treeWidget());

  if(stage == NULL)
    return;

  QVariant tag = item->data(0, Qt::UserRole);

  FetchTexture *tex = NULL;
  FetchBuffer *buf = NULL;

  if(tag.canConvert<ResourceId>())
  {
    ResourceId id = tag.value<ResourceId>();
    tex = m_Ctx->GetTexture(id);
    buf = m_Ctx->GetBuffer(id);
  }
  else if(tag.canConvert<ViewTag>())
  {
    ViewTag view = tag.value<ViewTag>();
    tex = m_Ctx->GetTexture(view.res.Resource);
    buf = m_Ctx->GetBuffer(view.res.Resource);
  }

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
  else if(buf)
  {
    ViewTag view;
    if(tag.canConvert<ViewTag>())
      view = tag.value<ViewTag>();

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

      if(stage == &m_Ctx->CurD3D12PipelineState.m_GS)
      {
        for(int i = 0; i < m_Ctx->CurD3D12PipelineState.m_SO.Outputs.count; i++)
        {
          if(buf->ID == m_Ctx->CurD3D12PipelineState.m_SO.Outputs[i].Buffer)
          {
            size -= m_Ctx->CurD3D12PipelineState.m_SO.Outputs[i].Offset;
            offs += m_Ctx->CurD3D12PipelineState.m_SO.Outputs[i].Offset;
            break;
          }
        }
      }
    }

    QString format = "";

    const ShaderResource *shaderRes = NULL;

    if(stage->ShaderDetails)
    {
      const rdctype::array<ShaderResource> &resArray =
          view.space == ViewTag::SRV ? stage->ShaderDetails->ReadOnlyResources
                                     : stage->ShaderDetails->ReadWriteResources;

      const rdctype::array<BindpointMap> &bindArray =
          view.space == ViewTag::SRV ? stage->BindpointMapping.ReadOnlyResources
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
        format = QString("// struct %1\n{\n%2}")
                     .arg(ToQStr(res.variableType.descriptor.name))
                     .arg(formatMembers(1, "", res.variableType.members.back().type.members));
      }
      else
      {
        const auto &desc = res.variableType.descriptor;

        if(view.res.Format.strname.empty())
        {
          format = "";
          if(desc.rowMajorStorage)
            format += "row_major ";

          format += ToQStr(desc.type);
          if(desc.rows > 1 && desc.cols > 1)
            format += QString("%1x%2").arg(desc.rows).arg(desc.cols);
          else if(desc.cols > 1)
            format += desc.cols;

          if(!desc.name.empty())
            format += " " + ToQStr(desc.name);

          if(desc.elements > 1)
            format += QString("[%1]").arg(desc.elements);
        }
        else
        {
          const ResourceFormat &fmt = view.res.Format;
          if(fmt.special && fmt.specialFormat == eSpecial_R10G10B10A2)
          {
            if(fmt.compType == eCompType_UInt)
              format = "uintten";
            if(fmt.compType == eCompType_UNorm)
              format = "unormten";
          }
          else if(!fmt.special)
          {
            switch(fmt.compByteWidth)
            {
              case 1:
              {
                if(fmt.compType == eCompType_UNorm)
                  format = "unormb";
                if(fmt.compType == eCompType_SNorm)
                  format = "snormb";
                if(fmt.compType == eCompType_UInt)
                  format = "ubyte";
                if(fmt.compType == eCompType_SInt)
                  format = "byte";
                break;
              }
              case 2:
              {
                if(fmt.compType == eCompType_UNorm)
                  format = "unormh";
                if(fmt.compType == eCompType_SNorm)
                  format = "snormh";
                if(fmt.compType == eCompType_UInt)
                  format = "ushort";
                if(fmt.compType == eCompType_SInt)
                  format = "short";
                if(fmt.compType == eCompType_Float)
                  format = "half";
                break;
              }
              case 4:
              {
                if(fmt.compType == eCompType_UNorm)
                  format = "unormf";
                if(fmt.compType == eCompType_SNorm)
                  format = "snormf";
                if(fmt.compType == eCompType_UInt)
                  format = "uint";
                if(fmt.compType == eCompType_SInt)
                  format = "int";
                if(fmt.compType == eCompType_Float)
                  format = "float";
                break;
              }
            }

            if(view.res.BufferFlags & RawBuffer)
              format = "xint";

            format += fmt.compCount;
          }
        }
      }
    }

    {
      // TODO Buffer viewer
      // var viewer = new BufferViewer(m_Core, false);
      // viewer.ViewRawBuffer(true, buf.offset, buf.size, buf.ID, format);
      // viewer.Show(m_DockContent.DockPanel);
    }
  }
}

void D3D12PipelineStateViewer::cbuffer_itemActivated(QTreeWidgetItem *item, int column)
{
  const D3D12PipelineState::ShaderStage *stage = stageForSender(item->treeWidget());

  if(stage == NULL)
    return;

  QVariant tag = item->data(0, Qt::UserRole);

  if(!tag.canConvert<CBufTag>())
    return;

  CBufTag cb = tag.value<CBufTag>();

  if(cb.idx == ~0U)
  {
    // unused cbuffer, open regular buffer viewer
    // TODO Buffer Viewer
    // var viewer = new BufferViewer(m_Core, false);

    // var buf = stage.Spaces[tag.space].ConstantBuffers[tag.reg];
    // viewer.ViewRawBuffer(true, buf.Offset, buf.ByteSize, buf.Buffer);
    // viewer.Show(m_DockContent.DockPanel);

    return;
  }

  ConstantBufferPreviewer *existing = ConstantBufferPreviewer::has(stage->stage, cb.idx, 0);
  if(existing)
  {
    ToolWindowManager::raiseToolWindow(existing);
    return;
  }

  ConstantBufferPreviewer *prev =
      new ConstantBufferPreviewer(m_Ctx, stage->stage, cb.idx, 0, m_Ctx->mainWindow());

  m_Ctx->setupDockWindow(prev);

  ToolWindowManager *manager = ToolWindowManager::managerOf(this);

  ToolWindowManager::AreaReference ref(ToolWindowManager::RightOf, manager->areaOf(this), 0.3f);
  manager->addToolWindow(prev, ref);
}

void D3D12PipelineStateViewer::on_iaLayouts_itemActivated(QTreeWidgetItem *item, int column)
{
  on_meshView_clicked();
}

void D3D12PipelineStateViewer::on_iaBuffers_itemActivated(QTreeWidgetItem *item, int column)
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

void D3D12PipelineStateViewer::highlightIABind(int slot)
{
  int idx = ((slot + 1) * 21) % 32;    // space neighbouring colours reasonably distinctly

  const D3D12PipelineState::InputAssembler &IA = m_Ctx->CurD3D12PipelineState.m_IA;

  QColor col = QColor::fromHslF(float(idx) / 32.0f, 1.0f, 0.95f);

  if(slot < m_VBNodes.count())
  {
    QTreeWidgetItem *item = m_VBNodes[(int)slot];

    for(int c = 0; c < item->columnCount(); c++)
    {
      item->setBackground(c, QBrush(col));
      item->setForeground(c, QBrush(QColor(0, 0, 0)));
    }
  }

  for(int i = 0; i < ui->iaLayouts->topLevelItemCount(); i++)
  {
    QTreeWidgetItem *item = ui->iaLayouts->topLevelItem(i);

    QBrush itemBrush = QBrush(col);

    if((int)IA.layouts[i].InputSlot != slot)
      itemBrush = QBrush();

    for(int c = 0; c < item->columnCount(); c++)
    {
      item->setBackground(c, itemBrush);
      item->setForeground(c, QBrush(QColor(0, 0, 0)));
    }
  }
}

void D3D12PipelineStateViewer::on_iaLayouts_mouseMove(QMouseEvent *e)
{
  if(!m_Ctx->LogLoaded())
    return;

  QModelIndex idx = ui->iaLayouts->indexAt(e->pos());

  vertex_leave(NULL);

  const D3D12PipelineState::InputAssembler &IA = m_Ctx->CurD3D12PipelineState.m_IA;

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
  if(!m_Ctx->LogLoaded())
    return;

  QTreeWidgetItem *item = ui->iaBuffers->itemAt(e->pos());

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
      {
        item->setBackground(c, QBrush(ui->iaBuffers->palette().color(QPalette::Window)));
        item->setForeground(c, QBrush());
      }
    }
  }
}

void D3D12PipelineStateViewer::vertex_leave(QEvent *e)
{
  for(int i = 0; i < ui->iaLayouts->topLevelItemCount(); i++)
  {
    QTreeWidgetItem *item = ui->iaLayouts->topLevelItem(i);
    for(int c = 0; c < item->columnCount(); c++)
    {
      item->setBackground(c, QBrush());
      item->setForeground(c, QBrush());
    }
  }

  for(int i = 0; i < ui->iaBuffers->topLevelItemCount(); i++)
  {
    QTreeWidgetItem *item = ui->iaBuffers->topLevelItem(i);
    for(int c = 0; c < item->columnCount(); c++)
    {
      item->setBackground(c, QBrush());
      item->setForeground(c, QBrush());
    }
  }
}

void D3D12PipelineStateViewer::shaderView_clicked()
{
  QWidget *sender = qobject_cast<QWidget *>(QObject::sender());
  const D3D12PipelineState::ShaderStage *stage = stageForSender(sender);

  if(stage == NULL || stage->Shader == ResourceId())
    return;

  ShaderViewer *shad = new ShaderViewer(m_Ctx, stage->ShaderDetails, stage->stage, NULL, "");

  m_Ctx->setupDockWindow(shad);

  ToolWindowManager *manager = ToolWindowManager::managerOf(this);

  ToolWindowManager::AreaReference ref(ToolWindowManager::AddTo, manager->areaOf(this));
  manager->addToolWindow(shad, ref);
}

void D3D12PipelineStateViewer::shaderEdit_clicked()
{
}

void D3D12PipelineStateViewer::shaderSave_clicked()
{
  const D3D12PipelineState::ShaderStage *stage =
      stageForSender(qobject_cast<QWidget *>(QObject::sender()));

  if(stage == NULL)
    return;

  ShaderReflection *shaderDetails = stage->ShaderDetails;

  if(stage->Shader == ResourceId())
    return;

  QString filename = RDDialog::getSaveFileName(this, tr("Save Shader As"), QString(),
                                               "DXBC Shader files (*.dxbc)");

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

void D3D12PipelineStateViewer::on_exportHTML_clicked()
{
}

void D3D12PipelineStateViewer::on_meshView_clicked()
{
  if(!m_Ctx->hasMeshPreview())
    m_Ctx->showMeshPreview();
  ToolWindowManager::raiseToolWindow(m_Ctx->meshPreview());
}
