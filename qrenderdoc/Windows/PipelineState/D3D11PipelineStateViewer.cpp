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

#include "D3D11PipelineStateViewer.h"
#include <float.h>
#include <QScrollBar>
#include "3rdparty/toolwindowmanager/ToolWindowManager.h"
#include "Code/Resources.h"
#include "Windows/BufferViewer.h"
#include "Windows/ConstantBufferPreviewer.h"
#include "Windows/MainWindow.h"
#include "Windows/ShaderViewer.h"
#include "Windows/TextureViewer.h"
#include "PipelineStateViewer.h"
#include "ui_D3D11PipelineStateViewer.h"

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
  ViewTag(ResType t, int i, const D3D11PipelineState::ShaderStage::ResourceView &r)
      : type(t), index(i), res(r)
  {
  }

  ResType type;
  int index;
  D3D11PipelineState::ShaderStage::ResourceView res;
};

Q_DECLARE_METATYPE(ViewTag);

D3D11PipelineStateViewer::D3D11PipelineStateViewer(CaptureContext &ctx, PipelineStateViewer &common,
                                                   QWidget *parent)
    : QFrame(parent), ui(new Ui::D3D11PipelineStateViewer), m_Ctx(ctx), m_Common(common)
{
  ui->setupUi(this);

  RDLabel *shaderLabels[] = {
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

  for(QToolButton *b : viewButtons)
    QObject::connect(b, &QToolButton::clicked, this, &D3D11PipelineStateViewer::shaderView_clicked);

  for(RDLabel *b : shaderLabels)
    QObject::connect(b, &RDLabel::clicked, this, &D3D11PipelineStateViewer::shaderView_clicked);

  for(QToolButton *b : editButtons)
    QObject::connect(b, &QToolButton::clicked, this, &D3D11PipelineStateViewer::shaderEdit_clicked);

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

  for(RDTreeWidget *res : resources)
    QObject::connect(res, &RDTreeWidget::itemActivated, this,
                     &D3D11PipelineStateViewer::resource_itemActivated);

  for(RDTreeWidget *cbuffer : cbuffers)
    QObject::connect(cbuffer, &RDTreeWidget::itemActivated, this,
                     &D3D11PipelineStateViewer::cbuffer_itemActivated);

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

  for(RDTreeWidget *tex : resources)
  {
    // Slot | Resource | Type | Width | Height | Depth | Array Size | Format | Go
    tex->header()->resizeSection(0, 120);
    tex->header()->setSectionResizeMode(0, QHeaderView::Interactive);
    tex->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    tex->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    tex->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    tex->header()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    tex->header()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    tex->header()->setSectionResizeMode(6, QHeaderView::ResizeToContents);
    tex->header()->setSectionResizeMode(7, QHeaderView::ResizeToContents);
    tex->header()->setSectionResizeMode(8, QHeaderView::ResizeToContents);

    tex->setHoverIconColumn(8);
    tex->setDefaultHoverColor(ui->targetOutputs->palette().color(QPalette::Window));
  }

  for(RDTreeWidget *samp : samplers)
  {
    // Slot | Addressing | Filter | LOD Clamp | LOD Bias
    samp->header()->resizeSection(0, 120);
    samp->header()->setSectionResizeMode(0, QHeaderView::Interactive);
    samp->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    samp->header()->setSectionResizeMode(2, QHeaderView::Stretch);
    samp->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    samp->header()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
  }

  for(RDTreeWidget *cbuffer : cbuffers)
  {
    // Slot | Buffer | Byte Range | Size | Go
    cbuffer->header()->resizeSection(0, 120);
    cbuffer->header()->setSectionResizeMode(0, QHeaderView::Interactive);
    cbuffer->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    cbuffer->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    cbuffer->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    cbuffer->header()->setSectionResizeMode(4, QHeaderView::ResizeToContents);

    cbuffer->setHoverIconColumn(4);
    cbuffer->setDefaultHoverColor(ui->targetOutputs->palette().color(QPalette::Window));
  }

  for(RDTreeWidget *cl : classes)
  {
    // Slot | Interface | Instance
    cl->header()->resizeSection(0, 120);
    cl->header()->setSectionResizeMode(0, QHeaderView::Interactive);
    cl->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    cl->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
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

  {
    // Slot | Resource | Type | Width | Height | Depth | Array Size | Format | Go
    ui->csUAVs->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    ui->csUAVs->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    ui->csUAVs->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    ui->csUAVs->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    ui->csUAVs->header()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    ui->csUAVs->header()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    ui->csUAVs->header()->setSectionResizeMode(6, QHeaderView::ResizeToContents);
    ui->csUAVs->header()->setSectionResizeMode(7, QHeaderView::ResizeToContents);
    ui->csUAVs->header()->setSectionResizeMode(8, QHeaderView::ResizeToContents);

    ui->csUAVs->setHoverIconColumn(8);
    ui->csUAVs->setDefaultHoverColor(ui->targetOutputs->palette().color(QPalette::Window));
  }

  // this is often changed just because we're changing some tab in the designer.
  ui->stagesTabs->setCurrentIndex(0);

  ui->stagesTabs->tabBar()->setVisible(false);

  ui->pipeFlow->setStages(
      {
          "IA", "VS", "HS", "DS", "GS", "RS", "PS", "OM", "CS",
      },
      {
          "Input Assembler", "Vertex Shader", "Hull Shader", "Domain Shader", "Geometry Shader",
          "Rasterizer", "Pixel Shader", "Output Merger", "Compute Shader",
      });

  ui->pipeFlow->setIsolatedStage(8);    // compute shader isolated

  ui->pipeFlow->setStagesEnabled({true, true, true, true, true, true, true, true, true});

  // reset everything back to defaults
  clearState();
}

D3D11PipelineStateViewer::~D3D11PipelineStateViewer()
{
  delete ui;
}

void D3D11PipelineStateViewer::OnLogfileLoaded()
{
  OnEventChanged(m_Ctx.CurEvent());
}

void D3D11PipelineStateViewer::OnLogfileClosed()
{
  ui->pipeFlow->setStagesEnabled({true, true, true, true, true, true, true, true, true});

  clearState();
}

void D3D11PipelineStateViewer::OnEventChanged(uint32_t eventID)
{
  setState();
}

void D3D11PipelineStateViewer::on_showDisabled_toggled(bool checked)
{
  setState();
}

void D3D11PipelineStateViewer::on_showEmpty_toggled(bool checked)
{
  setState();
}

void D3D11PipelineStateViewer::setInactiveRow(QTreeWidgetItem *node)
{
  for(int i = 0; i < node->columnCount(); i++)
  {
    QFont f = node->font(i);
    f.setItalic(true);
    node->setFont(i, f);
  }
}

void D3D11PipelineStateViewer::setEmptyRow(QTreeWidgetItem *node)
{
  for(int i = 0; i < node->columnCount(); i++)
    node->setBackgroundColor(i, QColor(255, 70, 70));
}

bool D3D11PipelineStateViewer::HasImportantViewParams(
    const D3D11PipelineState::ShaderStage::ResourceView &view, FetchTexture *tex)
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

bool D3D11PipelineStateViewer::HasImportantViewParams(
    const D3D11PipelineState::ShaderStage::ResourceView &view, FetchBuffer *buf)
{
  if(view.FirstElement > 0 || view.NumElements * view.ElementSize < buf->length)
    return true;

  return false;
}

void D3D11PipelineStateViewer::setViewDetails(QTreeWidgetItem *node, const ViewTag &view,
                                              FetchTexture *tex)
{
  if(tex == NULL)
    return;

  QString text;

  const D3D11PipelineState::ShaderStage::ResourceView &res = view.res;

  bool viewdetails = false;

  if(res.Format != tex->format)
  {
    text += tr("The texture is format %1, the view treats it as %2.\n")
                .arg(ToQStr(tex->format.strname))
                .arg(ToQStr(res.Format.strname));

    viewdetails = true;
  }

  if(view.type == ViewTag::OMDepth)
  {
    if(m_Ctx.CurD3D11PipelineState.m_OM.DepthReadOnly)
      text += tr("Depth component is read-only\n");
    if(m_Ctx.CurD3D11PipelineState.m_OM.StencilReadOnly)
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

void D3D11PipelineStateViewer::setViewDetails(QTreeWidgetItem *node, const ViewTag &view,
                                              FetchBuffer *buf)
{
  if(buf == NULL)
    return;

  QString text;

  const D3D11PipelineState::ShaderStage::ResourceView &res = view.res;

  if((res.FirstElement * res.ElementSize) > 0 || (res.NumElements * res.ElementSize) < buf->length)
  {
    text += tr("The view covers bytes %1-%2 (%3 elements).\nThe buffer is %4 bytes in length (%5 "
               "elements).")
                .arg(res.FirstElement * res.ElementSize)
                .arg((res.FirstElement + res.NumElements) * res.ElementSize)
                .arg(res.NumElements)
                .arg(buf->length)
                .arg(buf->length / res.ElementSize);
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

void D3D11PipelineStateViewer::addResourceRow(const ViewTag &view, const ShaderResource *shaderInput,
                                              RDTreeWidget *resources)
{
  const QIcon &action = Icons::action();
  const QIcon &action_hover = Icons::action_hover();

  const D3D11PipelineState::ShaderStage::ResourceView &r = view.res;

  bool viewDetails = false;

  if(view.type == ViewTag::OMDepth)
    viewDetails = m_Ctx.CurD3D11PipelineState.m_OM.DepthReadOnly ||
                  m_Ctx.CurD3D11PipelineState.m_OM.StencilReadOnly;

  bool filledSlot = (r.Resource != ResourceId());
  bool usedSlot = (shaderInput);

  // if a target is set to RTVs or DSV, it is implicitly used
  if(filledSlot)
    usedSlot = usedSlot || view.type == ViewTag::OMTarget || view.type == ViewTag::OMDepth;

  if(showNode(usedSlot, filledSlot))
  {
    QString slotname = view.type == ViewTag::OMDepth ? "Depth" : QString::number(view.index);

    if(shaderInput && !shaderInput->name.empty())
      slotname += ": " + ToQStr(shaderInput->name);

    uint32_t w = 1, h = 1, d = 1;
    uint32_t a = 1;
    QString format = "Unknown";
    QString name = "Shader Resource " + ToQStr(r.Resource);
    QString typeName = "Unknown";

    if(!filledSlot)
    {
      name = "Empty";
      format = "-";
      typeName = "-";
      w = h = d = a = 0;
    }

    FetchTexture *tex = m_Ctx.GetTexture(r.Resource);

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

    FetchBuffer *buf = m_Ctx.GetBuffer(r.Resource);

    if(buf)
    {
      w = buf->length;
      h = 0;
      d = 0;
      a = 0;
      format = "";
      name = buf->name;
      typeName = "Buffer";

      if(r.Flags & RawBuffer)
      {
        typeName = QString("%1ByteAddressBuffer").arg(view.type == ViewTag::UAV ? "RW" : "");
      }
      else if(r.ElementSize > 0)
      {
        // for structured buffers, display how many 'elements' there are in the buffer
        typeName = QString("%1StructuredBuffer[%2]")
                       .arg(view.type == ViewTag::UAV ? "RW" : "")
                       .arg(buf->length / r.ElementSize);
      }

      if(r.Flags & (AppendBuffer | CounterBuffer))
      {
        typeName += QString(" (Count: %1)").arg(r.BufferStructCount);
      }

      // get the buffer type, whether it's just a basic type or a complex struct
      if(shaderInput && !shaderInput->IsTexture)
      {
        if(r.Format.compType == eCompType_None)
        {
          if(!shaderInput->variableType.members.empty())
            format = "struct " + ToQStr(shaderInput->variableType.descriptor.name);
          else
            format = ToQStr(shaderInput->variableType.descriptor.name);
        }
        else
        {
          format = r.Format.strname;
        }
      }

      if(HasImportantViewParams(r, buf))
        viewDetails = true;
    }

    QTreeWidgetItem *node = makeTreeNode({slotname, name, typeName, w, h, d, a, format, ""});

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

bool D3D11PipelineStateViewer::showNode(bool usedSlot, bool filledSlot)
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

const D3D11PipelineState::ShaderStage *D3D11PipelineStateViewer::stageForSender(QWidget *widget)
{
  if(!m_Ctx.LogLoaded())
    return NULL;

  while(widget)
  {
    if(widget == ui->stagesTabs->widget(0))
      return &m_Ctx.CurD3D11PipelineState.m_VS;
    if(widget == ui->stagesTabs->widget(1))
      return &m_Ctx.CurD3D11PipelineState.m_VS;
    if(widget == ui->stagesTabs->widget(2))
      return &m_Ctx.CurD3D11PipelineState.m_HS;
    if(widget == ui->stagesTabs->widget(3))
      return &m_Ctx.CurD3D11PipelineState.m_DS;
    if(widget == ui->stagesTabs->widget(4))
      return &m_Ctx.CurD3D11PipelineState.m_GS;
    if(widget == ui->stagesTabs->widget(5))
      return &m_Ctx.CurD3D11PipelineState.m_PS;
    if(widget == ui->stagesTabs->widget(6))
      return &m_Ctx.CurD3D11PipelineState.m_PS;
    if(widget == ui->stagesTabs->widget(7))
      return &m_Ctx.CurD3D11PipelineState.m_PS;
    if(widget == ui->stagesTabs->widget(8))
      return &m_Ctx.CurD3D11PipelineState.m_CS;

    widget = widget->parentWidget();
  }

  qCritical() << "Unrecognised control calling event handler";

  return NULL;
}

void D3D11PipelineStateViewer::clearShaderState(QLabel *shader, RDTreeWidget *tex, RDTreeWidget *samp,
                                                RDTreeWidget *cbuffer, RDTreeWidget *sub)
{
  shader->setText(tr("Unbound Shader"));
  tex->clear();
  samp->clear();
  sub->clear();
  cbuffer->clear();
}

void D3D11PipelineStateViewer::clearState()
{
  m_VBNodes.clear();

  ui->iaLayouts->clear();
  ui->iaBuffers->clear();
  ui->iaBytecodeMismatch->setVisible(false);
  ui->topology->setText("");
  ui->topologyDiagram->setPixmap(QPixmap());

  clearShaderState(ui->vsShader, ui->vsResources, ui->vsSamplers, ui->vsCBuffers, ui->vsClasses);
  clearShaderState(ui->gsShader, ui->gsResources, ui->gsSamplers, ui->gsCBuffers, ui->gsClasses);
  clearShaderState(ui->hsShader, ui->hsResources, ui->hsSamplers, ui->hsCBuffers, ui->hsClasses);
  clearShaderState(ui->dsShader, ui->dsResources, ui->dsSamplers, ui->dsCBuffers, ui->dsClasses);
  clearShaderState(ui->psShader, ui->psResources, ui->psSamplers, ui->psCBuffers, ui->psClasses);
  clearShaderState(ui->csShader, ui->csResources, ui->csSamplers, ui->csCBuffers, ui->csClasses);

  ui->csUAVs->clear();

  const QPixmap &tick = Pixmaps::tick();
  const QPixmap &cross = Pixmaps::cross();

  ui->fillMode->setText(tr("Solid", "Fill Mode"));
  ui->cullMode->setText(tr("Front", "Cull Mode"));
  ui->frontCCW->setPixmap(tick);
  ui->conservativeRaster->setPixmap(cross);

  ui->depthBias->setText("0.0");
  ui->depthBiasClamp->setText("0.0");
  ui->slopeScaledBias->setText("0.0");
  ui->forcedSampleCount->setText("0");

  ui->depthClip->setPixmap(tick);
  ui->scissorEnabled->setPixmap(tick);
  ui->multisample->setPixmap(tick);
  ui->lineAA->setPixmap(tick);

  ui->independentBlend->setPixmap(cross);
  ui->alphaToCoverage->setPixmap(tick);

  ui->blendFactor->setText("0.00, 0.00, 0.00, 0.00");
  ui->sampleMask->setText("FFFFFFFF");

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

void D3D11PipelineStateViewer::setShaderState(const D3D11PipelineState::ShaderStage &stage,
                                              QLabel *shader, RDTreeWidget *resources,
                                              RDTreeWidget *samplers, RDTreeWidget *cbuffers,
                                              RDTreeWidget *classes)
{
  ShaderReflection *shaderDetails = stage.ShaderDetails;

  if(stage.Shader == ResourceId())
    shader->setText(tr("Unbound Shader"));
  else
    shader->setText(ToQStr(stage.ShaderName));

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
  for(int i = 0; i < stage.SRVs.count; i++)
  {
    const ShaderResource *shaderInput = NULL;

    if(shaderDetails)
    {
      for(const ShaderResource &bind : shaderDetails->ReadOnlyResources)
      {
        if(bind.IsSRV && bind.bindPoint == i)
        {
          shaderInput = &bind;
          break;
        }
      }
    }

    addResourceRow(ViewTag(ViewTag::SRV, i, stage.SRVs[i]), shaderInput, resources);
  }
  resources->clearSelection();
  resources->setUpdatesEnabled(true);
  resources->verticalScrollBar()->setValue(vs);

  vs = samplers->verticalScrollBar()->value();
  samplers->setUpdatesEnabled(false);
  samplers->clear();
  for(int i = 0; i < stage.Samplers.count; i++)
  {
    const D3D11PipelineState::ShaderStage::Sampler &s = stage.Samplers[i];

    const ShaderResource *shaderInput = NULL;

    if(shaderDetails)
    {
      for(const ShaderResource &bind : shaderDetails->ReadOnlyResources)
      {
        if(bind.IsSampler && bind.bindPoint == i)
        {
          shaderInput = &bind;
          break;
        }
      }
    }

    bool filledSlot = (!s.AddressU.empty());
    bool usedSlot = (shaderInput);

    if(showNode(usedSlot, filledSlot))
    {
      QString slotname = QString::number(i);

      if(shaderInput && !shaderInput->name.empty())
        slotname += ": " + ToQStr(shaderInput->name);

      if(s.customSamplerName)
        slotname += "(" + ToQStr(s.SamplerName) + ")";

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

      QString filter = ToQStr(s.Filter);

      if(s.MaxAniso > 1)
        filter += QString(" %1x").arg(s.MaxAniso);

      if(s.UseComparison)
        filter = QString(" (%1)").arg(ToQStr(s.Comparison));

      QTreeWidgetItem *node =
          makeTreeNode({slotname, addressing, filter,
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

  samplers->clearSelection();
  samplers->setUpdatesEnabled(true);
  samplers->verticalScrollBar()->setValue(vs);

  vs = cbuffers->verticalScrollBar()->value();
  cbuffers->setUpdatesEnabled(false);
  cbuffers->clear();
  for(int i = 0; i < stage.ConstantBuffers.count; i++)
  {
    const D3D11PipelineState::ShaderStage::CBuffer &b = stage.ConstantBuffers[i];

    const ConstantBlock *shaderCBuf = NULL;

    if(shaderDetails)
    {
      for(const ConstantBlock &bind : shaderDetails->ConstantBlocks)
      {
        if(bind.bindPoint == i)
        {
          shaderCBuf = &bind;
          break;
        }
      }
    }

    bool filledSlot = b.Buffer != ResourceId();
    bool usedSlot = shaderCBuf;

    if(showNode(usedSlot, filledSlot))
    {
      QString name = QString("Constant Buffer %1").arg(ToQStr(b.Buffer));
      ulong length = 1;
      int numvars = shaderCBuf ? shaderCBuf->variables.count : 0;
      uint32_t bytesize = shaderCBuf ? shaderCBuf->byteSize : 0;

      if(!filledSlot)
      {
        name = "Empty";
        length = 0;
      }

      FetchBuffer *buf = m_Ctx.GetBuffer(b.Buffer);

      if(buf)
      {
        name = buf->name;
        length = buf->length;
      }

      QString slotname = QString::number(i);

      if(shaderCBuf && !shaderCBuf->name.empty())
        slotname += ": " + ToQStr(shaderCBuf->name);

      QString sizestr;
      if(bytesize == (uint32_t)length)
        sizestr = tr("%1 Variables, %2 bytes").arg(numvars).arg(length);
      else
        sizestr =
            tr("%1 Variables, %2 bytes needed, %3 provided").arg(numvars).arg(bytesize).arg(length);

      if(length < bytesize)
        filledSlot = false;

      QString vecrange = QString("%1 - %2").arg(b.VecOffset).arg(b.VecOffset + b.VecCount);

      QTreeWidgetItem *node = makeTreeNode({slotname, name, vecrange, sizestr, ""});

      node->setData(0, Qt::UserRole, QVariant::fromValue(i));

      if(!filledSlot)
        setEmptyRow(node);

      if(!usedSlot)
        setInactiveRow(node);

      cbuffers->addTopLevelItem(node);
    }
  }
  cbuffers->clearSelection();
  cbuffers->setUpdatesEnabled(true);
  cbuffers->verticalScrollBar()->setValue(vs);

  vs = classes->verticalScrollBar()->value();
  classes->setUpdatesEnabled(false);
  classes->clear();
  for(int i = 0; i < stage.ClassInstances.count; i++)
  {
    QString interfaceName = QString("Interface %1").arg(i);

    if(shaderDetails && i < shaderDetails->Interfaces.count)
      interfaceName = ToQStr(shaderDetails->Interfaces[i]);

    classes->addTopLevelItem(makeTreeNode({i, interfaceName, ToQStr(stage.ClassInstances[i])}));
  }
  classes->clearSelection();
  classes->setUpdatesEnabled(true);
  classes->verticalScrollBar()->setValue(vs);

  classes->parentWidget()->setVisible(!stage.ClassInstances.empty());
}

void D3D11PipelineStateViewer::setState()
{
  if(!m_Ctx.LogLoaded())
  {
    clearState();
    return;
  }

  const D3D11PipelineState &state = m_Ctx.CurD3D11PipelineState;
  const FetchDrawcall *draw = m_Ctx.CurDrawcall();

  const QPixmap &tick = Pixmaps::tick();
  const QPixmap &cross = Pixmaps::cross();

  const QIcon &action = Icons::action();
  const QIcon &action_hover = Icons::action_hover();

  ////////////////////////////////////////////////
  // Vertex Input

  if(state.m_IA.Bytecode)
  {
    QString layout = ToQStr(state.m_IA.LayoutName);

    if(state.m_IA.Bytecode && !state.m_IA.Bytecode->DebugInfo.entryFunc.empty())
      layout += " (" + ToQStr(state.m_IA.Bytecode->DebugInfo.entryFunc) + ")";

    ui->iaBytecode->setText(layout);
  }
  else
  {
    ui->iaBytecode->setText(tr("None"));
  }

  ui->iaBytecodeMismatch->setVisible(false);

  // check for IA-VS mismatches here.
  // This should be moved to a "Render Doctor" window reporting problems
  if(state.m_IA.Bytecode && state.m_VS.ShaderDetails)
  {
    QString mismatchDetails = "";

    // VS wants more elements
    if(state.m_IA.Bytecode->InputSig.count < state.m_VS.ShaderDetails->InputSig.count)
    {
      int excess = state.m_VS.ShaderDetails->InputSig.count - state.m_IA.Bytecode->InputSig.count;

      bool allSystem = true;

      // The VS signature can consume more elements as long as they are all system value types
      // (ie. SV_VertexID or SV_InstanceID)
      for(int e = 0; e < excess; e++)
      {
        if(state.m_VS.ShaderDetails->InputSig[state.m_VS.ShaderDetails->InputSig.count - 1 - e]
               .systemValue == eAttr_None)
        {
          allSystem = false;
          break;
        }
      }

      if(!allSystem)
        mismatchDetails += "IA bytecode provides fewer elements than VS wants.\n";
    }

    {
      const rdctype::array<SigParameter> &IA = state.m_IA.Bytecode->InputSig;
      const rdctype::array<SigParameter> &VS = state.m_VS.ShaderDetails->InputSig;

      int count = qMin(IA.count, VS.count);

      for(int i = 0; i < count; i++)
      {
        QString IAname = ToQStr(IA[i].semanticIdxName);
        QString VSname = ToQStr(VS[i].semanticIdxName);

        // misorder or misnamed semantics
        if(IAname.toUpper() != VSname.toUpper())
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
                  .arg(IA[i].compType)
                  .arg(VS[i].compType);
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
  ui->iaLayouts->setUpdatesEnabled(false);
  ui->iaLayouts->clear();
  {
    int i = 0;
    for(const D3D11PipelineState::InputAssembler::LayoutInput &l : state.m_IA.layouts)
    {
      QString byteOffs = QString::number(l.ByteOffset);

      // D3D11 specific value
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

      for(int ia = 0; state.m_IA.Bytecode && ia < state.m_IA.Bytecode->InputSig.count; ia++)
      {
        if(ToQStr(state.m_IA.Bytecode->InputSig[ia].semanticName).toUpper() ==
               ToQStr(l.SemanticName).toUpper() &&
           state.m_IA.Bytecode->InputSig[ia].semanticIndex == l.SemanticIndex)
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
    case eTopology_PointList: ui->topologyDiagram->setPixmap(Pixmaps::topo_pointlist()); break;
    case eTopology_LineList: ui->topologyDiagram->setPixmap(Pixmaps::topo_linelist()); break;
    case eTopology_LineStrip: ui->topologyDiagram->setPixmap(Pixmaps::topo_linestrip()); break;
    case eTopology_TriangleList: ui->topologyDiagram->setPixmap(Pixmaps::topo_trilist()); break;
    case eTopology_TriangleStrip: ui->topologyDiagram->setPixmap(Pixmaps::topo_tristrip()); break;
    case eTopology_LineList_Adj:
      ui->topologyDiagram->setPixmap(Pixmaps::topo_linelist_adj());
      break;
    case eTopology_LineStrip_Adj:
      ui->topologyDiagram->setPixmap(Pixmaps::topo_linestrip_adj());
      break;
    case eTopology_TriangleList_Adj:
      ui->topologyDiagram->setPixmap(Pixmaps::topo_trilist_adj());
      break;
    case eTopology_TriangleStrip_Adj:
      ui->topologyDiagram->setPixmap(Pixmaps::topo_tristrip_adj());
      break;
    default: ui->topologyDiagram->setPixmap(Pixmaps::topo_patch()); break;
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

      FetchBuffer *buf = m_Ctx.GetBuffer(state.m_IA.ibuffer.Buffer);

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
    const D3D11PipelineState::InputAssembler::VertexBuffer &v = state.m_IA.vbuffers[i];

    bool filledSlot = (v.Buffer != ResourceId());
    bool usedSlot = (usedVBuffers[i]);

    if(showNode(usedSlot, filledSlot))
    {
      QString name = "Buffer " + ToQStr(v.Buffer);
      qulonglong length = 1;

      if(!filledSlot)
      {
        name = "Empty";
        length = 0;
      }

      FetchBuffer *buf = m_Ctx.GetBuffer(v.Buffer);
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
                 ui->vsClasses);
  setShaderState(state.m_GS, ui->gsShader, ui->gsResources, ui->gsSamplers, ui->gsCBuffers,
                 ui->gsClasses);
  setShaderState(state.m_HS, ui->hsShader, ui->hsResources, ui->hsSamplers, ui->hsCBuffers,
                 ui->hsClasses);
  setShaderState(state.m_DS, ui->dsShader, ui->dsResources, ui->dsSamplers, ui->dsCBuffers,
                 ui->dsClasses);
  setShaderState(state.m_PS, ui->psShader, ui->psResources, ui->psSamplers, ui->psCBuffers,
                 ui->psClasses);
  setShaderState(state.m_CS, ui->csShader, ui->csResources, ui->csSamplers, ui->csCBuffers,
                 ui->csClasses);

  vs = ui->csUAVs->verticalScrollBar()->value();
  ui->csUAVs->setUpdatesEnabled(false);
  ui->csUAVs->clear();
  for(int i = 0; i < state.m_CS.UAVs.count; i++)
  {
    const ShaderResource *shaderInput = NULL;

    if(state.m_CS.ShaderDetails)
    {
      for(const ShaderResource &bind : state.m_CS.ShaderDetails->ReadWriteResources)
      {
        if(bind.bindPoint == i)
        {
          shaderInput = &bind;
          break;
        }
      }
    }

    addResourceRow(ViewTag(ViewTag::UAV, i, state.m_CS.UAVs[i]), shaderInput, ui->csUAVs);
  }
  ui->csUAVs->clearSelection();
  ui->csUAVs->setUpdatesEnabled(true);
  ui->csUAVs->verticalScrollBar()->setValue(vs);

  bool streamoutSet = false;
  vs = ui->gsStreamOut->verticalScrollBar()->value();
  ui->gsStreamOut->setUpdatesEnabled(false);
  ui->gsStreamOut->clear();
  for(int i = 0; i < state.m_SO.Outputs.count; i++)
  {
    const D3D11PipelineState::Streamout::Output &s = state.m_SO.Outputs[i];

    bool filledSlot = (s.Buffer != ResourceId());
    bool usedSlot = (filledSlot);

    if(showNode(usedSlot, filledSlot))
    {
      QString name = "Buffer " + ToQStr(s.Buffer);
      qulonglong length = 0;

      if(!filledSlot)
      {
        name = "Empty";
      }

      FetchBuffer *buf = m_Ctx.GetBuffer(s.Buffer);

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
    const D3D11PipelineState::Rasterizer::Viewport &v = state.m_RS.Viewports[i];

    if(v.Enabled || ui->showEmpty->isChecked())
    {
      QTreeWidgetItem *node =
          makeTreeNode({i, v.TopLeft[0], v.TopLeft[1], v.Width, v.Height, v.MinDepth, v.MaxDepth});

      if(v.Width == 0 || v.Height == 0 || v.MinDepth == v.MaxDepth)
        setEmptyRow(node);

      if(!v.Enabled)
        setInactiveRow(node);

      ui->viewports->addTopLevelItem(node);
    }
  }
  ui->viewports->verticalScrollBar()->setValue(vs);
  ui->viewports->clearSelection();
  ui->viewports->setUpdatesEnabled(true);

  vs = ui->scissors->verticalScrollBar()->value();
  ui->scissors->setUpdatesEnabled(false);
  ui->scissors->clear();
  for(int i = 0; i < state.m_RS.Scissors.count; i++)
  {
    const D3D11PipelineState::Rasterizer::Scissor &s = state.m_RS.Scissors[i];

    if(s.Enabled || ui->showEmpty->isChecked())
    {
      QTreeWidgetItem *node = makeTreeNode({i, s.left, s.top, s.right - s.left, s.bottom - s.top});

      if(s.right == s.left || s.bottom == s.top)
        setEmptyRow(node);

      if(!s.Enabled)
        setInactiveRow(node);

      ui->scissors->addTopLevelItem(node);
    }
  }
  ui->scissors->clearSelection();
  ui->scissors->verticalScrollBar()->setValue(vs);
  ui->scissors->setUpdatesEnabled(true);

  ui->fillMode->setText(ToQStr(state.m_RS.m_State.FillMode));
  ui->cullMode->setText(ToQStr(state.m_RS.m_State.CullMode));
  ui->frontCCW->setPixmap(state.m_RS.m_State.FrontCCW ? tick : cross);

  ui->scissorEnabled->setPixmap(state.m_RS.m_State.ScissorEnable ? tick : cross);
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
      addResourceRow(ViewTag(ViewTag::OMTarget, i, state.m_OM.RenderTargets[i]), NULL,
                     ui->targetOutputs);

      if(state.m_OM.RenderTargets[i].Resource != ResourceId())
        targets[i] = true;
    }

    for(int i = 0; i < state.m_OM.UAVs.count; i++)
    {
      const ShaderResource *shaderInput = NULL;

      if(state.m_PS.ShaderDetails)
      {
        for(const ShaderResource &bind : state.m_PS.ShaderDetails->ReadWriteResources)
        {
          if(bind.bindPoint == i + (int)state.m_OM.UAVStartSlot)
          {
            shaderInput = &bind;
            break;
          }
        }
      }

      addResourceRow(ViewTag(ViewTag::UAV, i, state.m_OM.UAVs[i]), shaderInput, ui->targetOutputs);
    }

    addResourceRow(ViewTag(ViewTag::OMDepth, 0, state.m_OM.DepthTarget), NULL, ui->targetOutputs);
  }
  ui->targetOutputs->clearSelection();
  ui->targetOutputs->setUpdatesEnabled(true);
  ui->targetOutputs->verticalScrollBar()->setValue(vs);

  vs = ui->blends->verticalScrollBar()->value();
  ui->blends->setUpdatesEnabled(false);
  ui->blends->clear();
  {
    int i = 0;
    for(const D3D11PipelineState::OutputMerger::BlendState::RTBlend &blend :
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
  ui->sampleMask->setText(
      QString("%1").arg(state.m_OM.m_BlendState.SampleMask, 8, 16, QChar('0')).toUpper());

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
  if(draw == NULL)
  {
    ui->pipeFlow->setStagesEnabled({true, true, true, true, true, true, true, true, true});
  }
  else if(draw->flags & eDraw_Dispatch)
  {
    ui->pipeFlow->setStagesEnabled({false, false, false, false, false, false, false, false, true});
  }
  else
  {
    ui->pipeFlow->setStagesEnabled(
        {true, true, state.m_HS.Shader != ResourceId(), state.m_DS.Shader != ResourceId(),
         state.m_GS.Shader != ResourceId(), true, state.m_PS.Shader != ResourceId(), true, false});
  }
}

QString D3D11PipelineStateViewer::formatMembers(int indent, const QString &nameprefix,
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

void D3D11PipelineStateViewer::resource_itemActivated(QTreeWidgetItem *item, int column)
{
  const D3D11PipelineState::ShaderStage *stage = stageForSender(item->treeWidget());

  if(stage == NULL)
    return;

  QVariant tag = item->data(0, Qt::UserRole);

  FetchTexture *tex = NULL;
  FetchBuffer *buf = NULL;

  if(tag.canConvert<ResourceId>())
  {
    ResourceId id = tag.value<ResourceId>();
    tex = m_Ctx.GetTexture(id);
    buf = m_Ctx.GetBuffer(id);
  }
  else if(tag.canConvert<ViewTag>())
  {
    ViewTag view = tag.value<ViewTag>();
    tex = m_Ctx.GetTexture(view.res.Resource);
    buf = m_Ctx.GetBuffer(view.res.Resource);
  }

  if(tex)
  {
    if(tex->resType == eResType_Buffer)
    {
      BufferViewer *viewer = new BufferViewer(m_Ctx, false, m_Ctx.mainWindow());

      viewer->ViewTexture(0, 0, tex->ID);

      m_Ctx.setupDockWindow(viewer);

      ToolWindowManager *manager = ToolWindowManager::managerOf(this);

      ToolWindowManager::AreaReference ref(ToolWindowManager::AddTo, manager->areaOf(this));
      manager->addToolWindow(viewer, ref);
    }
    else
    {
      if(!m_Ctx.hasTextureViewer())
        m_Ctx.showTextureViewer();
      TextureViewer *viewer = m_Ctx.textureViewer();
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

      if(stage == &m_Ctx.CurD3D11PipelineState.m_GS)
      {
        for(int i = 0; i < m_Ctx.CurD3D11PipelineState.m_SO.Outputs.count; i++)
        {
          if(buf->ID == m_Ctx.CurD3D11PipelineState.m_SO.Outputs[i].Buffer)
          {
            size -= m_Ctx.CurD3D11PipelineState.m_SO.Outputs[i].Offset;
            offs += m_Ctx.CurD3D11PipelineState.m_SO.Outputs[i].Offset;
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
          view.type == ViewTag::SRV ? stage->ShaderDetails->ReadOnlyResources
                                    : stage->ShaderDetails->ReadWriteResources;

      for(const ShaderResource &res : resArray)
      {
        if(!res.IsTexture && !res.IsSampler && res.bindPoint == view.index)
        {
          shaderRes = &res;
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
                     .arg(formatMembers(1, "", res.variableType.members));
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
            format += QString::number(desc.cols);

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

            if(view.res.Flags & RawBuffer)
              format = "xint";

            format += QString::number(fmt.compCount);
          }
        }
      }
    }

    {
      BufferViewer *viewer = new BufferViewer(m_Ctx, false, m_Ctx.mainWindow());

      viewer->ViewBuffer(offs, size, view.res.Resource, format);

      m_Ctx.setupDockWindow(viewer);

      ToolWindowManager *manager = ToolWindowManager::managerOf(this);

      ToolWindowManager::AreaReference ref(ToolWindowManager::AddTo, manager->areaOf(this));
      manager->addToolWindow(viewer, ref);
    }
  }
}

void D3D11PipelineStateViewer::cbuffer_itemActivated(QTreeWidgetItem *item, int column)
{
  const D3D11PipelineState::ShaderStage *stage = stageForSender(item->treeWidget());

  if(stage == NULL)
    return;

  QVariant tag = item->data(0, Qt::UserRole);

  if(!tag.canConvert<int>())
    return;

  int cb = tag.value<int>();

  ConstantBufferPreviewer *existing = ConstantBufferPreviewer::has(stage->stage, cb, 0);
  if(existing)
  {
    ToolWindowManager::raiseToolWindow(existing);
    return;
  }

  ConstantBufferPreviewer *prev =
      new ConstantBufferPreviewer(m_Ctx, stage->stage, cb, 0, m_Ctx.mainWindow());

  m_Ctx.setupDockWindow(prev);

  ToolWindowManager *manager = ToolWindowManager::managerOf(this);

  ToolWindowManager::AreaReference ref(ToolWindowManager::RightOf, manager->areaOf(this), 0.3f);
  manager->addToolWindow(prev, ref);
}

void D3D11PipelineStateViewer::on_iaLayouts_itemActivated(QTreeWidgetItem *item, int column)
{
  on_meshView_clicked();
}

void D3D11PipelineStateViewer::on_iaBuffers_itemActivated(QTreeWidgetItem *item, int column)
{
  QVariant tag = item->data(0, Qt::UserRole);

  if(tag.canConvert<VBIBTag>())
  {
    VBIBTag buf = tag.value<VBIBTag>();

    if(buf.id != ResourceId())
    {
      BufferViewer *viewer = new BufferViewer(m_Ctx, false, m_Ctx.mainWindow());

      viewer->ViewBuffer(buf.offset, UINT64_MAX, buf.id);

      m_Ctx.setupDockWindow(viewer);

      ToolWindowManager *manager = ToolWindowManager::managerOf(this);

      ToolWindowManager::AreaReference ref(ToolWindowManager::AddTo, manager->areaOf(this));
      manager->addToolWindow(viewer, ref);
    }
  }
}

void D3D11PipelineStateViewer::highlightIABind(int slot)
{
  int idx = ((slot + 1) * 21) % 32;    // space neighbouring colours reasonably distinctly

  const D3D11PipelineState::InputAssembler &IA = m_Ctx.CurD3D11PipelineState.m_IA;

  QColor col = QColor::fromHslF(float(idx) / 32.0f, 1.0f, 0.95f);

  ui->iaLayouts->model()->blockSignals(true);
  ui->iaBuffers->model()->blockSignals(true);
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
  ui->iaLayouts->model()->blockSignals(false);
  ui->iaBuffers->model()->blockSignals(false);

  if(ui->iaLayouts->topLevelItemCount() > 0)
  {
    ui->iaLayouts->topLevelItem(0)->setDisabled(true);
    ui->iaLayouts->topLevelItem(0)->setDisabled(false);
  }

  if(ui->iaBuffers->topLevelItemCount() > 0)
  {
    ui->iaBuffers->topLevelItem(0)->setDisabled(true);
    ui->iaBuffers->topLevelItem(0)->setDisabled(false);
  }
}

void D3D11PipelineStateViewer::on_iaLayouts_mouseMove(QMouseEvent *e)
{
  if(!m_Ctx.LogLoaded())
    return;

  QModelIndex idx = ui->iaLayouts->indexAt(e->pos());

  vertex_leave(NULL);

  const D3D11PipelineState::InputAssembler &IA = m_Ctx.CurD3D11PipelineState.m_IA;

  if(idx.isValid())
  {
    if(idx.row() >= 0 && idx.row() < IA.layouts.count)
    {
      uint32_t buffer = IA.layouts[idx.row()].InputSlot;

      highlightIABind((int)buffer);
    }
  }
}

void D3D11PipelineStateViewer::on_iaBuffers_mouseMove(QMouseEvent *e)
{
  if(!m_Ctx.LogLoaded())
    return;

  QTreeWidgetItem *item = ui->iaBuffers->itemAt(e->pos());

  vertex_leave(NULL);

  ui->iaLayouts->model()->blockSignals(true);
  ui->iaBuffers->model()->blockSignals(true);
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
  ui->iaLayouts->model()->blockSignals(false);
  ui->iaBuffers->model()->blockSignals(false);

  if(ui->iaLayouts->topLevelItemCount() > 0)
  {
    ui->iaLayouts->topLevelItem(0)->setDisabled(true);
    ui->iaLayouts->topLevelItem(0)->setDisabled(false);
  }

  if(ui->iaBuffers->topLevelItemCount() > 0)
  {
    ui->iaBuffers->topLevelItem(0)->setDisabled(true);
    ui->iaBuffers->topLevelItem(0)->setDisabled(false);
  }
}

void D3D11PipelineStateViewer::on_pipeFlow_stageSelected(int index)
{
  ui->stagesTabs->setCurrentIndex(index);
}

void D3D11PipelineStateViewer::vertex_leave(QEvent *e)
{
  ui->iaLayouts->model()->blockSignals(true);
  ui->iaBuffers->model()->blockSignals(true);
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
  ui->iaLayouts->model()->blockSignals(false);
  ui->iaBuffers->model()->blockSignals(false);

  if(ui->iaLayouts->topLevelItemCount() > 0)
  {
    ui->iaLayouts->topLevelItem(0)->setDisabled(true);
    ui->iaLayouts->topLevelItem(0)->setDisabled(false);
  }

  if(ui->iaBuffers->topLevelItemCount() > 0)
  {
    ui->iaBuffers->topLevelItem(0)->setDisabled(true);
    ui->iaBuffers->topLevelItem(0)->setDisabled(false);
  }
}

void D3D11PipelineStateViewer::shaderView_clicked()
{
  ShaderStageType shaderStage = eShaderStage_Vertex;
  ShaderReflection *shaderDetails = NULL;
  const ShaderBindpointMapping *bindMap = NULL;

  QWidget *sender = qobject_cast<QWidget *>(QObject::sender());
  if(sender == ui->iaBytecode || sender == ui->iaBytecodeViewButton)
  {
    shaderDetails = m_Ctx.CurD3D11PipelineState.m_IA.Bytecode;
    bindMap = NULL;
  }
  else
  {
    const D3D11PipelineState::ShaderStage *stage = stageForSender(sender);

    if(stage == NULL || stage->Shader == ResourceId())
      return;

    bindMap = &stage->BindpointMapping;
    shaderDetails = stage->ShaderDetails;
    shaderStage = stage->stage;
  }

  ShaderViewer *shad =
      ShaderViewer::viewShader(m_Ctx, bindMap, shaderDetails, shaderStage, m_Ctx.mainWindow());

  m_Ctx.setupDockWindow(shad);

  ToolWindowManager *manager = ToolWindowManager::managerOf(this);

  ToolWindowManager::AreaReference ref(ToolWindowManager::AddTo, manager->areaOf(this));
  manager->addToolWindow(shad, ref);
}

void D3D11PipelineStateViewer::shaderEdit_clicked()
{
  QWidget *sender = qobject_cast<QWidget *>(QObject::sender());
  const D3D11PipelineState::ShaderStage *stage = stageForSender(sender);

  if(!stage || stage->Shader == ResourceId())
    return;

  const ShaderReflection *shaderDetails = stage->ShaderDetails;

  if(!shaderDetails)
    return;

  QString entryFunc = QString("EditedShader%1S").arg(ToQStr(stage->stage, eGraphicsAPI_D3D11)[0]);

  QString mainfile = "";

  QStringMap files;

  bool hasOrigSource = m_Common.PrepareShaderEditing(shaderDetails, entryFunc, files, mainfile);

  if(!hasOrigSource)
  {
    QString hlsl = "// TODO - generate stub HLSL";

    mainfile = "generated.hlsl";

    files[mainfile] = hlsl;
  }

  if(files.empty())
    return;

  m_Common.EditShader(stage->stage, stage->Shader, shaderDetails, entryFunc, files, mainfile);
}

void D3D11PipelineStateViewer::shaderSave_clicked()
{
  const D3D11PipelineState::ShaderStage *stage =
      stageForSender(qobject_cast<QWidget *>(QObject::sender()));

  if(stage == NULL)
    return;

  ShaderReflection *shaderDetails = stage->ShaderDetails;

  if(stage->Shader == ResourceId())
    return;

  m_Common.SaveShaderFile(shaderDetails);
}

void D3D11PipelineStateViewer::on_exportHTML_clicked()
{
}

void D3D11PipelineStateViewer::on_meshView_clicked()
{
  if(!m_Ctx.hasMeshPreview())
    m_Ctx.showMeshPreview();
  ToolWindowManager::raiseToolWindow(m_Ctx.meshPreview());
}
