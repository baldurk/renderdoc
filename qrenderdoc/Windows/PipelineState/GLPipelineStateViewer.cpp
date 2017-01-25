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

#include "GLPipelineStateViewer.h"
#include <float.h>
#include <QScrollBar>
#include "3rdparty/toolwindowmanager/ToolWindowManager.h"
#include "Windows/BufferViewer.h"
#include "Windows/ConstantBufferPreviewer.h"
#include "Windows/MainWindow.h"
#include "Windows/ShaderViewer.h"
#include "Windows/TextureViewer.h"
#include "ui_GLPipelineStateViewer.h"

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

struct ReadWriteTag
{
  ReadWriteTag()
  {
    bindPoint = 0;
    offset = size = 0;
  }
  ReadWriteTag(uint32_t b, ResourceId id, uint64_t offs, uint64_t sz)
  {
    bindPoint = b;
    ID = id;
    offset = offs;
    size = sz;
  }
  uint32_t bindPoint;
  ResourceId ID;
  uint64_t offset;
  uint64_t size;
};

Q_DECLARE_METATYPE(ReadWriteTag);

GLPipelineStateViewer::GLPipelineStateViewer(CaptureContext *ctx, QWidget *parent)
    : QFrame(parent), ui(new Ui::GLPipelineStateViewer), m_Ctx(ctx)
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

  RDTreeWidget *textures[] = {
      ui->vsTextures, ui->tcsTextures, ui->tesTextures,
      ui->gsTextures, ui->fsTextures,  ui->csTextures,
  };

  RDTreeWidget *samplers[] = {
      ui->vsSamplers, ui->tcsSamplers, ui->tesSamplers,
      ui->gsSamplers, ui->fsSamplers,  ui->csSamplers,
  };

  RDTreeWidget *ubos[] = {
      ui->vsUBOs, ui->tcsUBOs, ui->tesUBOs, ui->gsUBOs, ui->fsUBOs, ui->csUBOs,
  };

  RDTreeWidget *subroutines[] = {
      ui->vsSubroutines, ui->tcsSubroutines, ui->tesSubroutines,
      ui->gsSubroutines, ui->fsSubroutines,  ui->csSubroutines,
  };

  RDTreeWidget *readwrites[] = {
      ui->vsReadWrite, ui->tcsReadWrite, ui->tesReadWrite,
      ui->gsReadWrite, ui->fsReadWrite,  ui->csReadWrite,
  };

  for(QToolButton *b : viewButtons)
    QObject::connect(b, &QToolButton::clicked, this, &GLPipelineStateViewer::shaderView_clicked);

  for(RDLabel *b : shaderLabels)
    QObject::connect(b, &RDLabel::clicked, this, &GLPipelineStateViewer::shaderView_clicked);

  for(QToolButton *b : editButtons)
    QObject::connect(b, &QToolButton::clicked, this, &GLPipelineStateViewer::shaderEdit_clicked);

  for(QToolButton *b : saveButtons)
    QObject::connect(b, &QToolButton::clicked, this, &GLPipelineStateViewer::shaderSave_clicked);

  QObject::connect(ui->viAttrs, &RDTreeWidget::leave, this, &GLPipelineStateViewer::vertex_leave);
  QObject::connect(ui->viBuffers, &RDTreeWidget::leave, this, &GLPipelineStateViewer::vertex_leave);

  QObject::connect(ui->framebuffer, &RDTreeWidget::itemActivated, this,
                   &GLPipelineStateViewer::resource_itemActivated);

  for(RDTreeWidget *res : textures)
    QObject::connect(res, &RDTreeWidget::itemActivated, this,
                     &GLPipelineStateViewer::resource_itemActivated);

  for(RDTreeWidget *ubo : ubos)
    QObject::connect(ubo, &RDTreeWidget::itemActivated, this,
                     &GLPipelineStateViewer::ubo_itemActivated);

  for(RDTreeWidget *res : readwrites)
    QObject::connect(res, &RDTreeWidget::itemActivated, this,
                     &GLPipelineStateViewer::resource_itemActivated);

  addGridLines(ui->rasterizerGridLayout);
  addGridLines(ui->MSAAGridLayout);
  addGridLines(ui->blendStateGridLayout);
  addGridLines(ui->depthStateGridLayout);

  // no way to set this up in the UI :(
  {
    // Index | Enabled | Name | Format/Generic Value | Buffer Slot | Relative Offset | Go
    ui->viAttrs->header()->resizeSection(0, 75);
    ui->viAttrs->header()->setSectionResizeMode(0, QHeaderView::Interactive);
    ui->viAttrs->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    ui->viAttrs->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    ui->viAttrs->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    ui->viAttrs->header()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    ui->viAttrs->header()->setSectionResizeMode(5, QHeaderView::Stretch);
    ui->viAttrs->header()->setSectionResizeMode(6, QHeaderView::ResizeToContents);

    ui->viAttrs->setHoverIconColumn(6);
  }

  {
    // Slot | Buffer | Divisor | Offset | Stride | Byte Length | Go
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

  for(RDTreeWidget *tex : textures)
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
    tex->setDefaultHoverColor(ui->framebuffer->palette().color(QPalette::Window));
  }

  for(RDTreeWidget *samp : samplers)
  {
    // Slot | Addressing | Min Filter | Mag Filter | LOD Clamp | LOD Bias
    samp->header()->resizeSection(0, 120);
    samp->header()->setSectionResizeMode(0, QHeaderView::Interactive);
    samp->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    samp->header()->setSectionResizeMode(2, QHeaderView::Stretch);
    samp->header()->setSectionResizeMode(3, QHeaderView::Stretch);
    samp->header()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    samp->header()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
  }

  for(RDTreeWidget *ubo : ubos)
  {
    // Slot | Buffer | Byte Range | Size | Go
    ubo->header()->resizeSection(0, 120);
    ubo->header()->setSectionResizeMode(0, QHeaderView::Interactive);
    ubo->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    ubo->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    ubo->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    ubo->header()->setSectionResizeMode(4, QHeaderView::ResizeToContents);

    ubo->setHoverIconColumn(4);
    ubo->setDefaultHoverColor(ui->framebuffer->palette().color(QPalette::Window));
  }

  for(RDTreeWidget *sub : subroutines)
  {
    // Uniform | Value
    sub->header()->resizeSection(0, 120);
    sub->header()->setSectionResizeMode(0, QHeaderView::Interactive);
    sub->header()->setSectionResizeMode(1, QHeaderView::Stretch);
  }

  for(RDTreeWidget *ubo : readwrites)
  {
    // Binding | Slot | Resource | Dimensions | Format | Access | Go
    ubo->header()->resizeSection(1, 120);
    ubo->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    ubo->header()->setSectionResizeMode(1, QHeaderView::Interactive);
    ubo->header()->setSectionResizeMode(2, QHeaderView::Stretch);
    ubo->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    ubo->header()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    ubo->header()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
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
    // Slot | X | Y | Width | Height | Enabled
    ui->scissors->header()->resizeSection(0, 100);
    ui->scissors->header()->setSectionResizeMode(0, QHeaderView::Interactive);
    ui->scissors->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    ui->scissors->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    ui->scissors->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    ui->scissors->header()->setSectionResizeMode(4, QHeaderView::Stretch);
    ui->scissors->header()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
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

GLPipelineStateViewer::~GLPipelineStateViewer()
{
  delete ui;
}

void GLPipelineStateViewer::OnLogfileLoaded()
{
  OnEventChanged(m_Ctx->CurEvent());
}

void GLPipelineStateViewer::OnLogfileClosed()
{
  clearState();
}

void GLPipelineStateViewer::OnEventChanged(uint32_t eventID)
{
  setState();
}

void GLPipelineStateViewer::on_showDisabled_toggled(bool checked)
{
  setState();
}

void GLPipelineStateViewer::on_showEmpty_toggled(bool checked)
{
  setState();
}

void GLPipelineStateViewer::setInactiveRow(QTreeWidgetItem *node)
{
  for(int i = 0; i < node->columnCount(); i++)
  {
    QFont f = node->font(i);
    f.setItalic(true);
    node->setFont(i, f);
  }
}

void GLPipelineStateViewer::setEmptyRow(QTreeWidgetItem *node)
{
  for(int i = 0; i < node->columnCount(); i++)
    node->setBackgroundColor(i, QColor(255, 70, 70));
}

bool GLPipelineStateViewer::showNode(bool usedSlot, bool filledSlot)
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

const GLPipelineState::ShaderStage *GLPipelineStateViewer::stageForSender(QWidget *widget)
{
  if(!m_Ctx->LogLoaded())
    return NULL;

  while(widget)
  {
    if(widget == ui->stagesTabs->widget(0))
      return &m_Ctx->CurGLPipelineState.m_VS;
    if(widget == ui->stagesTabs->widget(1))
      return &m_Ctx->CurGLPipelineState.m_VS;
    if(widget == ui->stagesTabs->widget(2))
      return &m_Ctx->CurGLPipelineState.m_TCS;
    if(widget == ui->stagesTabs->widget(3))
      return &m_Ctx->CurGLPipelineState.m_TES;
    if(widget == ui->stagesTabs->widget(4))
      return &m_Ctx->CurGLPipelineState.m_GS;
    if(widget == ui->stagesTabs->widget(5))
      return &m_Ctx->CurGLPipelineState.m_FS;
    if(widget == ui->stagesTabs->widget(6))
      return &m_Ctx->CurGLPipelineState.m_FS;
    if(widget == ui->stagesTabs->widget(7))
      return &m_Ctx->CurGLPipelineState.m_FS;
    if(widget == ui->stagesTabs->widget(8))
      return &m_Ctx->CurGLPipelineState.m_CS;

    widget = widget->parentWidget();
  }

  qCritical() << "Unrecognised control calling event handler";

  return NULL;
}

void GLPipelineStateViewer::clearShaderState(QLabel *shader, RDTreeWidget *tex, RDTreeWidget *samp,
                                             RDTreeWidget *ubo, RDTreeWidget *sub, RDTreeWidget *rw)
{
  shader->setText(tr("Unbound Shader"));
  tex->clear();
  samp->clear();
  sub->clear();
  ubo->clear();
  rw->clear();
}

void GLPipelineStateViewer::clearState()
{
  m_VBNodes.clear();

  ui->viAttrs->clear();
  ui->viBuffers->clear();
  ui->topology->setText("");
  ui->primRestart->setVisible(false);
  ui->topologyDiagram->setPixmap(QPixmap());

  clearShaderState(ui->vsShader, ui->vsTextures, ui->vsSamplers, ui->vsUBOs, ui->vsSubroutines,
                   ui->vsReadWrite);
  clearShaderState(ui->gsShader, ui->gsTextures, ui->gsSamplers, ui->gsUBOs, ui->gsSubroutines,
                   ui->gsReadWrite);
  clearShaderState(ui->tcsShader, ui->tcsTextures, ui->tcsSamplers, ui->tcsUBOs, ui->tcsSubroutines,
                   ui->tcsReadWrite);
  clearShaderState(ui->tesShader, ui->tesTextures, ui->tesSamplers, ui->tesUBOs, ui->tesSubroutines,
                   ui->tesReadWrite);
  clearShaderState(ui->fsShader, ui->fsTextures, ui->fsSamplers, ui->fsUBOs, ui->fsSubroutines,
                   ui->fsReadWrite);
  clearShaderState(ui->csShader, ui->csTextures, ui->csSamplers, ui->csUBOs, ui->csSubroutines,
                   ui->csReadWrite);

  QPixmap tick(QString::fromUtf8(":/Resources/tick.png"));
  QPixmap cross(QString::fromUtf8(":/Resources/cross.png"));

  ui->fillMode->setText(tr("Solid", "Fill Mode"));
  ui->cullMode->setText(tr("Front", "Cull Mode"));
  ui->frontCCW->setPixmap(tick);

  ui->scissorEnabled->setPixmap(tick);
  ui->provoking->setText("Last");
  ui->rasterizerDiscard->setPixmap(cross);

  ui->pointSize->setText("1.0");
  ui->lineWidth->setText("1.0");

  ui->clipSetup->setText("0,0 Lower Left, Z= -1 to 1");
  ui->clipDistance->setText("-");

  ui->depthClamp->setPixmap(tick);
  ui->depthBias->setText("0.0");
  ui->slopeScaledBias->setText("0.0");
  ui->offsetClamp->setText("");
  ui->offsetClamp->setPixmap(cross);

  ui->multisample->setPixmap(tick);
  ui->sampleShading->setPixmap(tick);
  ui->minSampleShading->setText("0.0");
  ui->alphaToOne->setPixmap(tick);
  ui->alphaToCoverage->setPixmap(tick);

  ui->sampleCoverage->setText("");
  ui->sampleCoverage->setPixmap(cross);
  ui->sampleMask->setText("");
  ui->sampleMask->setPixmap(cross);

  ui->viewports->clear();
  ui->scissors->clear();

  ui->framebuffer->clear();
  ui->blends->clear();

  ui->blendFactor->setText("0.00, 0.00, 0.00, 0.00");

  ui->depthEnabled->setPixmap(tick);
  ui->depthFunc->setText("GREATER_EQUAL");
  ui->depthWrite->setPixmap(tick);

  ui->depthBounds->setText("0.0-1.0");
  ui->depthBounds->setPixmap(QPixmap());

  ui->stencils->clear();
}

void GLPipelineStateViewer::setShaderState(const GLPipelineState::ShaderStage &stage,
                                           QLabel *shader, RDTreeWidget *textures,
                                           RDTreeWidget *samplers, RDTreeWidget *ubos,
                                           RDTreeWidget *subs, RDTreeWidget *readwrites)
{
  ShaderReflection *shaderDetails = stage.ShaderDetails;
  const ShaderBindpointMapping &mapping = stage.BindpointMapping;
  const GLPipelineState &state = m_Ctx->CurGLPipelineState;

  QIcon action(QPixmap(QString::fromUtf8(":/Resources/action.png")));
  QIcon action_hover(QPixmap(QString::fromUtf8(":/Resources/action_hover.png")));

  if(stage.Shader == ResourceId())
  {
    shader->setText(tr("Unbound Shader"));
  }
  else
  {
    QString shaderName = ToQStr(stage.stage, eGraphicsAPI_OpenGL) + " Shader";

    if(!stage.customShaderName && !stage.customProgramName && !stage.customPipelineName)
    {
      shader->setText(shaderName + " " + ToQStr(stage.Shader));
    }
    else
    {
      if(stage.customShaderName)
        shaderName = ToQStr(stage.ShaderName);

      if(stage.customProgramName)
        shaderName = ToQStr(stage.ProgramName) + " - " + shaderName;

      if(stage.customPipelineName && stage.PipelineActive)
        shaderName = ToQStr(stage.PipelineName) + " - " + shaderName;

      shader->setText(shaderName);
    }
  }

  int vs = 0;
  int vs2 = 0;

  // simultaneous update of resources and samplers
  vs = textures->verticalScrollBar()->value();
  textures->setUpdatesEnabled(false);
  textures->clear();
  vs2 = samplers->verticalScrollBar()->value();
  samplers->setUpdatesEnabled(false);
  samplers->clear();

  for(int i = 0; i < state.Textures.count; i++)
  {
    const GLPipelineState::Texture &r = state.Textures[i];
    const GLPipelineState::Sampler &s = state.Samplers[i];

    const ShaderResource *shaderInput = NULL;
    const BindpointMap *map = NULL;

    if(shaderDetails)
    {
      for(const ShaderResource &bind : shaderDetails->ReadOnlyResources)
      {
        if(bind.IsSRV && mapping.ReadOnlyResources[bind.bindPoint].bind == i)
        {
          shaderInput = &bind;
          map = &mapping.ReadOnlyResources[bind.bindPoint];
        }
      }
    }

    bool filledSlot = (r.Resource != ResourceId());
    bool usedSlot = (shaderInput && map && map->used);

    if(showNode(usedSlot, filledSlot))
    {
      // do texture
      {
        QString slotname = QString::number(i);

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

          if(tex->format.special && (tex->format.specialFormat == eSpecial_D16S8 ||
                                     tex->format.specialFormat == eSpecial_D24S8 ||
                                     tex->format.specialFormat == eSpecial_D32S8))
          {
            if(r.DepthReadChannel == 0)
              format += " Depth-Read";
            else if(r.DepthReadChannel == 1)
              format += " Stencil-Read";
          }
          else if(r.Swizzle[0] != eSwizzle_Red || r.Swizzle[1] != eSwizzle_Green ||
                  r.Swizzle[2] != eSwizzle_Blue || r.Swizzle[3] != eSwizzle_Alpha)
          {
            format += QString(" swizzle[%1%2%3%4]")
                          .arg(ToQStr(r.Swizzle[0]))
                          .arg(ToQStr(r.Swizzle[1]))
                          .arg(ToQStr(r.Swizzle[2]))
                          .arg(ToQStr(r.Swizzle[3]));
          }
        }

        QTreeWidgetItem *node = makeTreeNode({slotname, name, typeName, w, h, d, a, format, ""});

        textures->setHoverIcons(node, action, action_hover);

        if(!filledSlot)
          setEmptyRow(node);

        if(!usedSlot)
          setInactiveRow(node);

        textures->addTopLevelItem(node);
      }

      // do sampler
      {
        QString slotname = QString::number(i);

        if(shaderInput && !shaderInput->name.empty())
          slotname += ": " + ToQStr(shaderInput->name);

        QString borderColor =
            QString::number(s.BorderColor[0]) + ", " + QString::number(s.BorderColor[1]) + ", " +
            QString::number(s.BorderColor[2]) + ", " + QString::number(s.BorderColor[3]);

        QString addressing = "";

        QString addPrefix = "";
        QString addVal = "";

        QString addr[] = {ToQStr(s.AddressS), ToQStr(s.AddressT), ToQStr(s.AddressR)};

        // arrange like either STR: WRAP or ST: WRAP, R: CLAMP
        for(int a = 0; a < 3; a++)
        {
          const QString str[] = {"S", "T", "R"};
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

        if(r.ResType == eResType_TextureCube || r.ResType == eResType_TextureCubeArray)
        {
          addressing += s.SeamlessCube ? " Seamless" : " Non-Seamless";
        }

        QString minfilter = ToQStr(s.MinFilter);

        if(s.MaxAniso > 1)
          minfilter += QString(" Aniso%1x").arg(s.MaxAniso);

        if(s.UseComparison)
          minfilter = ToQStr(s.Comparison);

        QTreeWidgetItem *node =
            makeTreeNode({slotname, addressing, minfilter, ToQStr(s.MagFilter),
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
  samplers->verticalScrollBar()->setValue(vs2);
  textures->clearSelection();
  textures->setUpdatesEnabled(true);
  textures->verticalScrollBar()->setValue(vs);

  vs = ubos->verticalScrollBar()->value();
  ubos->setUpdatesEnabled(false);
  ubos->clear();
  for(int i = 0; shaderDetails && i < shaderDetails->ConstantBlocks.count; i++)
  {
    const ConstantBlock &shaderCBuf = shaderDetails->ConstantBlocks[i];
    int bindPoint = stage.BindpointMapping.ConstantBlocks[i].bind;

    const GLPipelineState::Buffer *b = NULL;

    if(bindPoint >= 0 && bindPoint < state.UniformBuffers.count)
      b = &state.UniformBuffers[bindPoint];

    bool filledSlot = !shaderCBuf.bufferBacked || (b && b->Resource != ResourceId());
    bool usedSlot = stage.BindpointMapping.ConstantBlocks[i].used;

    if(showNode(usedSlot, filledSlot))
    {
      ulong offset = 0;
      ulong length = 0;
      int numvars = shaderCBuf.variables.count;
      ulong byteSize = (ulong)shaderCBuf.byteSize;

      QString slotname = "Uniforms";
      QString name = "";
      QString sizestr = tr("%1 Variables").arg(numvars);
      QString byterange = "";

      if(!filledSlot)
      {
        name = "Empty";
        length = 0;
      }

      if(b)
      {
        slotname = QString("%1: %2").arg(bindPoint).arg(ToQStr(shaderCBuf.name));
        name = "UBO " + ToQStr(b->Resource);
        offset = b->Offset;
        length = b->Size;

        FetchBuffer *buf = m_Ctx->GetBuffer(b->Resource);
        if(buf)
        {
          name = buf->name;
          if(length == 0)
            length = buf->length;
        }

        if(length == byteSize)
          sizestr = tr("%1 Variables, %2 bytes").arg(numvars).arg(length);
        else
          sizestr =
              tr("%1 Variables, %2 bytes needed, %3 provided").arg(numvars).arg(byteSize).arg(length);

        if(length < byteSize)
          filledSlot = false;

        byterange = QString("%1 - %2").arg(offset).arg(offset + length);
      }

      QTreeWidgetItem *node = makeTreeNode({slotname, name, byterange, sizestr, ""});

      node->setData(0, Qt::UserRole, QVariant::fromValue(i));

      if(!filledSlot)
        setEmptyRow(node);

      if(!usedSlot)
        setInactiveRow(node);

      ubos->addTopLevelItem(node);
    }
  }
  ubos->clearSelection();
  ubos->setUpdatesEnabled(true);
  ubos->verticalScrollBar()->setValue(vs);

  vs = subs->verticalScrollBar()->value();
  subs->setUpdatesEnabled(false);
  subs->clear();
  for(int i = 0; i < stage.Subroutines.count; i++)
    subs->addTopLevelItem(makeTreeNode({i, stage.Subroutines[i]}));
  subs->clearSelection();
  subs->setUpdatesEnabled(true);
  subs->verticalScrollBar()->setValue(vs);

  subs->parentWidget()->setVisible(!stage.Subroutines.empty());

  vs = readwrites->verticalScrollBar()->value();
  readwrites->setUpdatesEnabled(false);
  readwrites->clear();
  for(int i = 0; shaderDetails && i < shaderDetails->ReadWriteResources.count; i++)
  {
    const ShaderResource &res = shaderDetails->ReadWriteResources[i];
    int bindPoint = stage.BindpointMapping.ReadWriteResources[i].bind;

    GLReadWriteType readWriteType = GetGLReadWriteType(res);

    const GLPipelineState::Buffer *bf = NULL;
    const GLPipelineState::ImageLoadStore *im = NULL;
    ResourceId id;

    if(readWriteType == GLReadWriteType::Image && bindPoint >= 0 && bindPoint < state.Images.count)
    {
      im = &state.Images[bindPoint];
      id = state.Images[bindPoint].Resource;
    }

    if(readWriteType == GLReadWriteType::Atomic && bindPoint >= 0 &&
       bindPoint < state.AtomicBuffers.count)
    {
      bf = &state.AtomicBuffers[bindPoint];
      id = state.AtomicBuffers[bindPoint].Resource;
    }

    if(readWriteType == GLReadWriteType::SSBO && bindPoint >= 0 &&
       bindPoint < state.ShaderStorageBuffers.count)
    {
      bf = &state.ShaderStorageBuffers[bindPoint];
      id = state.ShaderStorageBuffers[bindPoint].Resource;
    }

    bool filledSlot = id != ResourceId();
    bool usedSlot = stage.BindpointMapping.ReadWriteResources[i].used;

    if(showNode(usedSlot, filledSlot))
    {
      QString binding = readWriteType == GLReadWriteType::Image
                            ? "Image"
                            : readWriteType == GLReadWriteType::Atomic
                                  ? "Atomic"
                                  : readWriteType == GLReadWriteType::SSBO ? "SSBO" : "Unknown";

      QString slotname = QString("%1: %2").arg(bindPoint).arg(ToQStr(res.name));
      QString name = "";
      QString dimensions = "";
      QString format = "-";
      QString access = "Read/Write";
      if(im)
      {
        if(im->readAllowed && !im->writeAllowed)
          access = "Read-Only";
        if(!im->readAllowed && im->writeAllowed)
          access = "Write-Only";
        format = im->Format.strname;
      }

      QVariant tag;

      FetchTexture *tex = m_Ctx->GetTexture(id);

      if(tex)
      {
        if(tex->dimension == 1)
        {
          if(tex->arraysize > 1)
            dimensions = QString("%1[%2]").arg(tex->width).arg(tex->arraysize);
          else
            dimensions = QString("%1").arg(tex->width);
        }
        else if(tex->dimension == 2)
        {
          if(tex->arraysize > 1)
            dimensions = QString("%1x%2[%3]").arg(tex->width).arg(tex->height).arg(tex->arraysize);
          else
            dimensions = QString("%1x%2").arg(tex->width).arg(tex->height);
        }
        else if(tex->dimension == 3)
        {
          dimensions = QString("%1x%2x%3").arg(tex->width).arg(tex->height).arg(tex->depth);
        }

        name = tex->name;

        tag = QVariant::fromValue(id);
      }

      FetchBuffer *buf = m_Ctx->GetBuffer(id);

      if(buf)
      {
        uint64_t offset = 0;
        uint64_t length = buf->length;
        if(bf && bf->Size > 0)
        {
          offset = bf->Offset;
          length = bf->Size;
        }

        if(offset > 0)
          dimensions = tr("%1 bytes at offset %2 bytes").arg(length).arg(offset);
        else
          dimensions = tr("%1 bytes").arg(length);

        name = ToQStr(buf->name);

        tag = QVariant::fromValue(ReadWriteTag(i, id, offset, length));
      }

      if(!filledSlot)
      {
        name = "Empty";
        dimensions = "-";
        access = "-";
      }

      QTreeWidgetItem *node =
          makeTreeNode({binding, slotname, name, dimensions, format, access, ""});

      node->setData(0, Qt::UserRole, tag);

      if(!filledSlot)
        setEmptyRow(node);

      if(!usedSlot)
        setInactiveRow(node);

      readwrites->addTopLevelItem(node);
    }
  }
  readwrites->clearSelection();
  readwrites->setUpdatesEnabled(true);
  readwrites->verticalScrollBar()->setValue(vs);

  readwrites->parentWidget()->setVisible(!stage.Subroutines.empty());
}

QString GLPipelineStateViewer::MakeGenericValueString(
    uint32_t compCount, FormatComponentType compType,
    const GLPipelineState::VertexInput::VertexAttribute &val)
{
  QString fmtstr = "";
  if(compCount == 1)
    fmtstr = "<%1>";
  else if(compCount == 2)
    fmtstr = "<%1, %2>";
  else if(compCount == 3)
    fmtstr = "<%1, %2, %3>";
  else if(compCount == 4)
    fmtstr = "<%1, %2, %3, %4>";

  if(compType == eCompType_UInt)
    return QString(fmtstr)
        .arg(val.GenericValue.u[0])
        .arg(val.GenericValue.u[1])
        .arg(val.GenericValue.u[2])
        .arg(val.GenericValue.u[3]);
  else if(compType == eCompType_SInt)
    return QString(fmtstr)
        .arg(val.GenericValue.i[0])
        .arg(val.GenericValue.i[1])
        .arg(val.GenericValue.i[2])
        .arg(val.GenericValue.i[3]);
  else
    return QString(fmtstr)
        .arg(val.GenericValue.f[0])
        .arg(val.GenericValue.f[1])
        .arg(val.GenericValue.f[2])
        .arg(val.GenericValue.f[3]);
}

GLPipelineStateViewer::GLReadWriteType GLPipelineStateViewer::GetGLReadWriteType(ShaderResource res)
{
  GLReadWriteType ret = GLReadWriteType::Image;

  if(res.IsTexture)
  {
    ret = GLReadWriteType::Image;
  }
  else
  {
    if(res.variableType.descriptor.rows == 1 && res.variableType.descriptor.cols == 1 &&
       res.variableType.descriptor.type == eVar_UInt)
    {
      ret = GLReadWriteType::Atomic;
    }
    else
    {
      ret = GLReadWriteType::SSBO;
    }
  }

  return ret;
}

void GLPipelineStateViewer::setState()
{
  if(!m_Ctx->LogLoaded())
  {
    clearState();
    return;
  }

  const GLPipelineState &state = m_Ctx->CurGLPipelineState;
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
    for(const GLPipelineState::VertexInput::VertexAttribute &a : state.m_VtxIn.attributes)
    {
      bool filledSlot = true;
      bool usedSlot = false;

      QString name = tr("Attribute %1").arg(i);

      uint32_t compCount = 4;
      FormatComponentType compType = eCompType_Float;

      if(state.m_VS.Shader != ResourceId())
      {
        int attrib = -1;
        if(i < state.m_VS.BindpointMapping.InputAttributes.count)
          attrib = state.m_VS.BindpointMapping.InputAttributes[i];

        if(attrib >= 0 && attrib < state.m_VS.ShaderDetails->InputSig.count)
        {
          name = state.m_VS.ShaderDetails->InputSig[attrib].varName;
          compCount = state.m_VS.ShaderDetails->InputSig[attrib].compCount;
          compType = state.m_VS.ShaderDetails->InputSig[attrib].compType;
          usedSlot = true;
        }
      }

      if(showNode(usedSlot, filledSlot))
      {
        QString genericVal = "Generic=" + MakeGenericValueString(compCount, compType, a);

        QTreeWidgetItem *node = makeTreeNode({i, a.Enabled ? tr("Enabled") : tr("Disabled"), name,
                                              a.Enabled ? ToQStr(a.Format.strname) : genericVal,
                                              a.BufferSlot, a.RelativeOffset});

        if(a.Enabled)
          usedBindings[a.BufferSlot] = true;

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

  bool ibufferUsed = draw && (draw->flags & eDraw_UseIBuffer);

  if(ibufferUsed)
  {
    ui->primRestart->setVisible(true);
    if(state.m_VtxIn.primitiveRestart)
      ui->primRestart->setText(
          QString("Restart Idx: 0x%1").arg(state.m_VtxIn.restartIndex, 8, 16, QChar('0')).toUpper());
    else
      ui->primRestart->setText("Restart Idx: Disabled");
  }
  else
  {
    ui->primRestart->setVisible(false);
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

  vs = ui->viBuffers->verticalScrollBar()->value();
  ui->viBuffers->setUpdatesEnabled(false);
  ui->viBuffers->clear();

  if(state.m_VtxIn.ibuffer != ResourceId())
  {
    if(ibufferUsed || showDisabled)
    {
      QString name = "Buffer " + ToQStr(state.m_VtxIn.ibuffer);
      uint64_t length = 1;

      if(!ibufferUsed)
        length = 0;

      FetchBuffer *buf = m_Ctx->GetBuffer(state.m_VtxIn.ibuffer);

      if(buf)
      {
        name = buf->name;
        length = buf->length;
      }

      QTreeWidgetItem *node = makeTreeNode(
          {"Element", name, 0, 0, draw ? draw->indexByteWidth : 0, (qulonglong)length, ""});

      ui->viBuffers->setHoverIcons(node, action, action_hover);

      node->setData(0, Qt::UserRole, QVariant::fromValue(VBIBTag(state.m_VtxIn.ibuffer,
                                                                 draw ? draw->indexOffset : 0)));

      if(!ibufferUsed)
        setInactiveRow(node);

      if(state.m_VtxIn.ibuffer == ResourceId())
        setEmptyRow(node);

      ui->viBuffers->addTopLevelItem(node);
    }
  }
  else
  {
    if(ibufferUsed || showEmpty)
    {
      QTreeWidgetItem *node = makeTreeNode({"Element", tr("No Buffer Set"), "-", "-", "-", "-", ""});

      ui->viBuffers->setHoverIcons(node, action, action_hover);

      node->setData(0, Qt::UserRole, QVariant::fromValue(VBIBTag(state.m_VtxIn.ibuffer,
                                                                 draw ? draw->indexOffset : 0)));

      setEmptyRow(node);

      if(!ibufferUsed)
        setInactiveRow(node);

      ui->viBuffers->addTopLevelItem(node);
    }
  }

  m_VBNodes.clear();

  for(int i = 0; i < state.m_VtxIn.vbuffers.count; i++)
  {
    const GLPipelineState::VertexInput::VertexBuffer &v = state.m_VtxIn.vbuffers[i];

    bool filledSlot = (v.Buffer != ResourceId());
    bool usedSlot = (usedBindings[i]);

    if(showNode(usedSlot, filledSlot))
    {
      QString name = "Buffer " + ToQStr(v.Buffer);
      uint64_t length = 1;
      uint64_t offset = v.Offset;

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

      QTreeWidgetItem *node =
          makeTreeNode({i, name, v.Stride, (qulonglong)offset, v.Divisor, (qulonglong)length, ""});

      ui->viBuffers->setHoverIcons(node, action, action_hover);

      node->setData(0, Qt::UserRole, QVariant::fromValue(VBIBTag(v.Buffer, v.Offset)));

      if(!filledSlot)
        setEmptyRow(node);

      if(!usedSlot)
        setInactiveRow(node);

      m_VBNodes.push_back(node);

      ui->viBuffers->addTopLevelItem(node);
    }
  }
  ui->viBuffers->clearSelection();
  ui->viBuffers->setUpdatesEnabled(true);
  ui->viBuffers->verticalScrollBar()->setValue(vs);

  setShaderState(state.m_VS, ui->vsShader, ui->vsTextures, ui->vsSamplers, ui->vsUBOs,
                 ui->vsSubroutines, ui->vsReadWrite);
  setShaderState(state.m_GS, ui->gsShader, ui->gsTextures, ui->gsSamplers, ui->gsUBOs,
                 ui->gsSubroutines, ui->gsReadWrite);
  setShaderState(state.m_TCS, ui->tcsShader, ui->tcsTextures, ui->tcsSamplers, ui->tcsUBOs,
                 ui->tcsSubroutines, ui->tcsReadWrite);
  setShaderState(state.m_TES, ui->tesShader, ui->tesTextures, ui->tesSamplers, ui->tesUBOs,
                 ui->tesSubroutines, ui->tesReadWrite);
  setShaderState(state.m_FS, ui->fsShader, ui->fsTextures, ui->fsSamplers, ui->fsUBOs,
                 ui->fsSubroutines, ui->fsReadWrite);
  setShaderState(state.m_CS, ui->csShader, ui->csTextures, ui->csSamplers, ui->csUBOs,
                 ui->csSubroutines, ui->csReadWrite);

  vs = ui->gsFeedback->verticalScrollBar()->value();
  ui->gsFeedback->setUpdatesEnabled(false);
  ui->gsFeedback->clear();
  if(state.m_Feedback.Active)
  {
    ui->xfbPaused->setPixmap(state.m_Feedback.Paused ? tick : cross);
    for(int i = 0; i < (int)ARRAY_COUNT(state.m_Feedback.BufferBinding); i++)
    {
      bool filledSlot = (state.m_Feedback.BufferBinding[i] != ResourceId());
      bool usedSlot = (filledSlot);

      if(showNode(usedSlot, filledSlot))
      {
        QString name = "Buffer " + ToQStr(state.m_Feedback.BufferBinding[i]);
        qulonglong length = state.m_Feedback.Size[i];

        if(!filledSlot)
        {
          name = "Empty";
        }

        FetchBuffer *buf = m_Ctx->GetBuffer(state.m_Feedback.BufferBinding[i]);

        if(buf)
        {
          name = buf->name;
          if(length == 0)
            length = buf->length;
        }

        QTreeWidgetItem *node =
            makeTreeNode({i, name, length, (qulonglong)state.m_Feedback.Offset[i], ""});

        ui->gsFeedback->setHoverIcons(node, action, action_hover);

        node->setData(0, Qt::UserRole, QVariant::fromValue(state.m_Feedback.BufferBinding[i]));

        if(!filledSlot)
          setEmptyRow(node);

        if(!usedSlot)
          setInactiveRow(node);

        ui->gsFeedback->addTopLevelItem(node);
      }
    }
  }
  ui->gsFeedback->verticalScrollBar()->setValue(vs);
  ui->gsFeedback->clearSelection();
  ui->gsFeedback->setUpdatesEnabled(true);

  ui->gsFeedback->setVisible(state.m_Feedback.Active);
  ui->xfbGroup->setVisible(state.m_Feedback.Active);

  ////////////////////////////////////////////////
  // Rasterizer

  vs = ui->viewports->verticalScrollBar()->value();
  ui->viewports->setUpdatesEnabled(false);
  ui->viewports->clear();

  {
    // accumulate identical viewports to save on visual repetition
    int prev = 0;
    for(int i = 0; i < state.m_Rasterizer.Viewports.count; i++)
    {
      const GLPipelineState::Rasterizer::Viewport &v1 = state.m_Rasterizer.Viewports[prev];
      const GLPipelineState::Rasterizer::Viewport &v2 = state.m_Rasterizer.Viewports[i];

      if(v1.Width != v2.Width || v1.Height != v2.Height || v1.Left != v2.Left ||
         v1.Bottom != v2.Bottom || v1.MinDepth != v2.MinDepth || v1.MaxDepth != v2.MaxDepth)
      {
        if(v1.Width != v1.Height || v1.Width != 0 || v1.Height != 0 || v1.MinDepth != v1.MaxDepth ||
           ui->showEmpty->isChecked())
        {
          QString indexstring;
          if(prev < i - 1)
            indexstring = QString("%1-%2").arg(prev).arg(i - 1);
          else
            indexstring = QString::number(prev);

          QTreeWidgetItem *node = makeTreeNode(
              {indexstring, v1.Left, v1.Bottom, v1.Width, v1.Height, v1.MinDepth, v1.MaxDepth});

          if(v1.Width == 0 || v1.Height == 0 || v1.MinDepth == v1.MaxDepth)
            setEmptyRow(node);

          ui->viewports->addTopLevelItem(node);
        }

        prev = i;
      }
    }

    // handle the last batch (the loop above leaves the last batch un-added)
    if(prev < state.m_Rasterizer.Viewports.count)
    {
      const GLPipelineState::Rasterizer::Viewport &v1 = state.m_Rasterizer.Viewports[prev];

      // must display at least one viewport - otherwise if they are
      // all empty we get an empty list - we want a nice obvious
      // 'invalid viewport' entry. So check if last is 0

      if(v1.Width != v1.Height || v1.Width != 0 || v1.Height != 0 || v1.MinDepth != v1.MaxDepth ||
         ui->showEmpty->isChecked() || prev == 0)
      {
        QString indexstring;
        if(prev < state.m_Rasterizer.Viewports.count - 1)
          indexstring = QString("%1-%2").arg(prev).arg(state.m_Rasterizer.Viewports.count - 1);
        else
          indexstring = QString::number(prev);

        QTreeWidgetItem *node = makeTreeNode(
            {indexstring, v1.Left, v1.Bottom, v1.Width, v1.Height, v1.MinDepth, v1.MaxDepth});

        if(v1.Width == 0 || v1.Height == 0 || v1.MinDepth == v1.MaxDepth)
          setEmptyRow(node);

        ui->viewports->addTopLevelItem(node);
      }
    }
  }
  ui->viewports->verticalScrollBar()->setValue(vs);
  ui->viewports->clearSelection();
  ui->viewports->setUpdatesEnabled(true);

  bool anyScissorEnable = false;

  vs = ui->scissors->verticalScrollBar()->value();
  ui->scissors->setUpdatesEnabled(false);
  ui->scissors->clear();
  {
    // accumulate identical scissors to save on visual repetition
    int prev = 0;
    for(int i = 0; i < state.m_Rasterizer.Scissors.count; i++)
    {
      const GLPipelineState::Rasterizer::Scissor &s1 = state.m_Rasterizer.Scissors[prev];
      const GLPipelineState::Rasterizer::Scissor &s2 = state.m_Rasterizer.Scissors[i];

      if(s1.Width != s2.Width || s1.Height != s2.Height || s1.Left != s2.Left ||
         s1.Bottom != s2.Bottom || s1.Enabled != s2.Enabled)
      {
        if(s1.Enabled || ui->showEmpty->isChecked())
        {
          QString indexstring;
          if(prev < i - 1)
            indexstring = QString("%1-%2").arg(prev).arg(i - 1);
          else
            indexstring = QString::number(prev);

          QTreeWidgetItem *node = makeTreeNode({indexstring, s1.Left, s1.Bottom, s1.Width,
                                                s1.Height, s1.Enabled ? tr("True") : tr("False")});

          if(s1.Width == 0 || s1.Height == 0)
            setEmptyRow(node);

          if(!s1.Enabled)
            setInactiveRow(node);

          anyScissorEnable = anyScissorEnable || s1.Enabled;

          ui->scissors->addTopLevelItem(node);
        }

        prev = i;
      }
    }

    // handle the last batch (the loop above leaves the last batch un-added)
    if(prev < state.m_Rasterizer.Scissors.count)
    {
      const GLPipelineState::Rasterizer::Scissor &s1 = state.m_Rasterizer.Scissors[prev];

      if(s1.Enabled || ui->showEmpty->isChecked())
      {
        QString indexstring;
        if(prev < state.m_Rasterizer.Scissors.count - 1)
          indexstring = QString("%1-%2").arg(prev).arg(state.m_Rasterizer.Scissors.count - 1);
        else
          indexstring = QString::number(prev);

        QTreeWidgetItem *node = makeTreeNode({indexstring, s1.Left, s1.Bottom, s1.Width, s1.Height,
                                              s1.Enabled ? tr("True") : tr("False")});

        if(s1.Width == 0 || s1.Height == 0)
          setEmptyRow(node);

        if(!s1.Enabled)
          setInactiveRow(node);

        anyScissorEnable = anyScissorEnable || s1.Enabled;

        ui->scissors->addTopLevelItem(node);
      }
    }
  }
  ui->scissors->clearSelection();
  ui->scissors->verticalScrollBar()->setValue(vs);
  ui->scissors->setUpdatesEnabled(true);

  ui->fillMode->setText(ToQStr(state.m_Rasterizer.m_State.FillMode));
  ui->cullMode->setText(ToQStr(state.m_Rasterizer.m_State.CullMode));
  ui->frontCCW->setPixmap(state.m_Rasterizer.m_State.FrontCCW ? tick : cross);

  ui->scissorEnabled->setPixmap(anyScissorEnable ? tick : cross);
  ui->provoking->setText(state.m_VtxIn.provokingVertexLast ? "Last" : "First");

  ui->rasterizerDiscard->setPixmap(state.m_VtxProcess.discard ? tick : cross);

  if(state.m_Rasterizer.m_State.ProgrammablePointSize)
    ui->pointSize->setText(tr("Program", "ProgrammablePointSize"));
  else
    ui->pointSize->setText(Formatter::Format(state.m_Rasterizer.m_State.PointSize));
  ui->lineWidth->setText(Formatter::Format(state.m_Rasterizer.m_State.LineWidth));

  QString clipSetup = "";
  if(state.m_VtxProcess.clipOriginLowerLeft)
    clipSetup += "0,0 Lower Left";
  else
    clipSetup += "0,0 Upper Left";
  clipSetup += ", ";
  if(state.m_VtxProcess.clipNegativeOneToOne)
    clipSetup += "Z= -1 to 1";
  else
    clipSetup += "Z= 0 to 1";

  ui->clipSetup->setText(clipSetup);

  QString clipDistances = "";

  int numDist = 0;
  for(int i = 0; i < (int)ARRAY_COUNT(state.m_VtxProcess.clipPlanes); i++)
  {
    if(state.m_VtxProcess.clipPlanes[i])
    {
      if(numDist > 0)
        clipDistances += ", ";
      clipDistances += QString::number(i);

      numDist++;
    }
  }

  if(numDist == 0)
    clipDistances = "-";
  else
    clipDistances += " enabled";

  ui->clipDistance->setText(clipDistances);

  ui->depthClamp->setPixmap(state.m_Rasterizer.m_State.DepthClamp ? tick : cross);
  ui->depthBias->setText(Formatter::Format(state.m_Rasterizer.m_State.DepthBias));
  ui->slopeScaledBias->setText(Formatter::Format(state.m_Rasterizer.m_State.SlopeScaledDepthBias));

  if(state.m_Rasterizer.m_State.OffsetClamp == 0.0f || qIsNaN(state.m_Rasterizer.m_State.OffsetClamp))
  {
    ui->offsetClamp->setText("");
    ui->offsetClamp->setPixmap(cross);
  }
  else
  {
    ui->offsetClamp->setText(Formatter::Format(state.m_Rasterizer.m_State.OffsetClamp));
    ui->offsetClamp->setPixmap(QPixmap());
  }

  ui->multisample->setPixmap(state.m_Rasterizer.m_State.MultisampleEnable ? tick : cross);
  ui->sampleShading->setPixmap(state.m_Rasterizer.m_State.SampleShading ? tick : cross);
  ui->minSampleShading->setText(Formatter::Format(state.m_Rasterizer.m_State.MinSampleShadingRate));
  ui->alphaToCoverage->setPixmap(state.m_Rasterizer.m_State.SampleAlphaToCoverage ? tick : cross);
  ui->alphaToOne->setPixmap(state.m_Rasterizer.m_State.SampleAlphaToOne ? tick : cross);
  if(state.m_Rasterizer.m_State.SampleCoverage)
  {
    QString sampleCoverage = Formatter::Format(state.m_Rasterizer.m_State.SampleCoverageValue);
    if(state.m_Rasterizer.m_State.SampleCoverageInvert)
      sampleCoverage += " inverted";
    ui->sampleCoverage->setText(sampleCoverage);
    ui->sampleCoverage->setPixmap(QPixmap());
  }
  else
  {
    ui->sampleCoverage->setText("");
    ui->sampleCoverage->setPixmap(cross);
  }

  if(state.m_Rasterizer.m_State.SampleMask)
  {
    ui->sampleMask->setText(
        QString("%1").arg(state.m_Rasterizer.m_State.SampleMaskValue, 8, 16, QChar('0')).toUpper());
    ui->sampleMask->setPixmap(QPixmap());
  }
  else
  {
    ui->sampleMask->setText("");
    ui->sampleMask->setPixmap(cross);
  }

  ////////////////////////////////////////////////
  // Output Merger

  bool targets[32] = {};

  vs = ui->framebuffer->verticalScrollBar()->value();
  ui->framebuffer->setUpdatesEnabled(false);
  ui->framebuffer->clear();
  {
    int i = 0;
    for(int db : state.m_FB.m_DrawFBO.DrawBuffers)
    {
      ResourceId p;
      const GLPipelineState::FrameBuffer::Attachment *r = NULL;

      if(db >= 0 && db < state.m_FB.m_DrawFBO.Color.count)
      {
        p = state.m_FB.m_DrawFBO.Color[db].Obj;
        r = &state.m_FB.m_DrawFBO.Color[db];
      }

      bool filledSlot = (p != ResourceId());
      bool usedSlot = db >= 0;

      if(showNode(usedSlot, filledSlot))
      {
        uint32_t w = 1, h = 1, d = 1;
        uint32_t a = 1;
        QString format = "Unknown";
        QString name = "Texture " + ToQStr(p);
        QString typeName = "Unknown";

        if(p == ResourceId())
        {
          name = "Empty";
          format = "-";
          typeName = "-";
          w = h = d = a = 0;
        }

        FetchTexture *tex = m_Ctx->GetTexture(p);
        if(tex)
        {
          w = tex->width;
          h = tex->height;
          d = tex->depth;
          a = tex->arraysize;
          name = tex->name;
          typeName = ToQStr(tex->resType);

          if(tex->format.srgbCorrected && !state.m_FB.FramebufferSRGB)
            name += " (GL_FRAMEBUFFER_SRGB = 0)";

          if(!tex->customName && state.m_FS.ShaderDetails)
          {
            for(int s = 0; s < state.m_FS.ShaderDetails->OutputSig.count; s++)
            {
              if(state.m_FS.ShaderDetails->OutputSig[s].regIndex == (uint32_t)db &&
                 (state.m_FS.ShaderDetails->OutputSig[s].systemValue == eAttr_None ||
                  state.m_FS.ShaderDetails->OutputSig[s].systemValue == eAttr_ColourOutput))
              {
                name = QString("<%1>").arg(ToQStr(state.m_FS.ShaderDetails->OutputSig[s].varName));
              }
            }
          }
        }

        if(r && (r->Swizzle[0] != eSwizzle_Red || r->Swizzle[1] != eSwizzle_Green ||
                 r->Swizzle[2] != eSwizzle_Blue || r->Swizzle[3] != eSwizzle_Alpha))
        {
          format += tr(" swizzle[%1%2%3%4]")
                        .arg(ToQStr(r->Swizzle[0]))
                        .arg(ToQStr(r->Swizzle[1]))
                        .arg(ToQStr(r->Swizzle[2]))
                        .arg(ToQStr(r->Swizzle[3]));
        }

        QTreeWidgetItem *node = makeTreeNode({i, name, typeName, w, h, d, a, format, ""});

        ui->framebuffer->setHoverIcons(node, action, action_hover);

        if(tex)
          node->setData(0, Qt::UserRole, QVariant::fromValue(p));

        if(p == ResourceId())
        {
          setEmptyRow(node);
        }
        else
        {
          targets[i] = true;
        }

        ui->framebuffer->addTopLevelItem(node);
      }

      i++;
    }

    ResourceId dsObjects[] = {
        state.m_FB.m_DrawFBO.Depth.Obj, state.m_FB.m_DrawFBO.Stencil.Obj,
    };

    for(int dsIdx = 0; dsIdx < 2; dsIdx++)
    {
      ResourceId ds = dsObjects[dsIdx];

      bool filledSlot = (ds != ResourceId());
      bool usedSlot = true;
      if(showNode(usedSlot, filledSlot))
      {
        uint32_t w = 1, h = 1, d = 1;
        uint32_t a = 1;
        QString format = "Unknown";
        QString name = "Texture " + ToQStr(ds);
        QString typeName = "Unknown";

        if(ds == ResourceId())
        {
          name = "Empty";
          format = "-";
          typeName = "-";
          w = h = d = a = 0;
        }

        FetchTexture *tex = m_Ctx->GetTexture(ds);
        if(tex)
        {
          w = tex->width;
          h = tex->height;
          d = tex->depth;
          a = tex->arraysize;
          name = tex->name;
          typeName = ToQStr(tex->resType);
        }

        QString slot = "Depth";
        if(i == 1)
          slot = "Stencil";

        bool depthstencil = false;

        if(state.m_FB.m_DrawFBO.Depth.Obj == state.m_FB.m_DrawFBO.Stencil.Obj &&
           state.m_FB.m_DrawFBO.Depth.Obj != ResourceId())
        {
          depthstencil = true;
          slot = "Depthstencil";
        }

        QTreeWidgetItem *node = makeTreeNode({slot, name, typeName, w, h, d, a, format, ""});

        ui->framebuffer->setHoverIcons(node, action, action_hover);

        if(tex)
          node->setData(0, Qt::UserRole, QVariant::fromValue(ds));

        if(ds == ResourceId())
          setEmptyRow(node);

        ui->framebuffer->addTopLevelItem(node);

        // if we added a combined depth-stencil row, break now
        if(depthstencil)
          break;
      }
    }
  }

  ui->framebuffer->clearSelection();
  ui->framebuffer->setUpdatesEnabled(true);
  ui->framebuffer->verticalScrollBar()->setValue(vs);

  vs = ui->blends->verticalScrollBar()->value();
  ui->blends->setUpdatesEnabled(false);
  ui->blends->clear();
  {
    bool logic = !state.m_FB.m_Blending.Blends[0].LogicOp.empty();

    int i = 0;
    for(const GLPipelineState::FrameBuffer::BlendState::RTBlend &blend : state.m_FB.m_Blending.Blends)
    {
      bool filledSlot = (blend.Enabled || targets[i]);
      bool usedSlot = (targets[i]);

      // if logic operation is enabled, blending is disabled
      if(logic)
        filledSlot = (i == 0);

      if(showNode(usedSlot, filledSlot))
      {
        QTreeWidgetItem *node = NULL;

        if(i == 0 && logic)
        {
          node = makeTreeNode({i, tr("True"),

                               "-", "-", ToQStr(blend.LogicOp),

                               "-", "-", "-",

                               QString("%1%2%3%4")
                                   .arg((blend.WriteMask & 0x1) == 0 ? "_" : "R")
                                   .arg((blend.WriteMask & 0x2) == 0 ? "_" : "G")
                                   .arg((blend.WriteMask & 0x4) == 0 ? "_" : "B")
                                   .arg((blend.WriteMask & 0x8) == 0 ? "_" : "A")});
        }
        else
        {
          node = makeTreeNode({i, blend.Enabled ? tr("True") : tr("False"),

                               ToQStr(blend.m_Blend.Source), ToQStr(blend.m_Blend.Destination),
                               ToQStr(blend.m_Blend.Operation),

                               ToQStr(blend.m_AlphaBlend.Source),
                               ToQStr(blend.m_AlphaBlend.Destination),
                               ToQStr(blend.m_AlphaBlend.Operation),

                               QString("%1%2%3%4")
                                   .arg((blend.WriteMask & 0x1) == 0 ? "_" : "R")
                                   .arg((blend.WriteMask & 0x2) == 0 ? "_" : "G")
                                   .arg((blend.WriteMask & 0x4) == 0 ? "_" : "B")
                                   .arg((blend.WriteMask & 0x8) == 0 ? "_" : "A")});
        }

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
                               .arg(state.m_FB.m_Blending.BlendFactor[0], 2)
                               .arg(state.m_FB.m_Blending.BlendFactor[1], 2)
                               .arg(state.m_FB.m_Blending.BlendFactor[2], 2)
                               .arg(state.m_FB.m_Blending.BlendFactor[3], 2));

  ui->depthEnabled->setPixmap(state.m_DepthState.DepthEnable ? tick : cross);
  ui->depthFunc->setText(ToQStr(state.m_DepthState.DepthFunc));
  ui->depthWrite->setPixmap(state.m_DepthState.DepthWrites ? tick : cross);

  if(state.m_DepthState.DepthBounds)
  {
    ui->depthBounds->setText(Formatter::Format(state.m_DepthState.NearBound) + "-" +
                             Formatter::Format(state.m_DepthState.FarBound));
    ui->depthBounds->setPixmap(QPixmap());
  }
  else
  {
    ui->depthBounds->setText("");
    ui->depthBounds->setPixmap(cross);
  }

  ui->stencils->setUpdatesEnabled(false);
  ui->stencils->clear();
  if(state.m_StencilState.StencilEnable)
  {
    ui->stencils->addTopLevelItems(
        {makeTreeNode(
             {"Front", ToQStr(state.m_StencilState.m_FrontFace.Func),
              ToQStr(state.m_StencilState.m_FrontFace.FailOp),
              ToQStr(state.m_StencilState.m_FrontFace.DepthFailOp),
              ToQStr(state.m_StencilState.m_FrontFace.PassOp),
              QString("%1").arg(state.m_StencilState.m_FrontFace.WriteMask, 2, 16, QChar('0')).toUpper(),
              QString("%1").arg(state.m_StencilState.m_FrontFace.ValueMask, 2, 16, QChar('0')).toUpper(),
              QString("%1").arg(state.m_StencilState.m_FrontFace.Ref, 2, 16, QChar('0')).toUpper()}),
         makeTreeNode(
             {"Back", ToQStr(state.m_StencilState.m_BackFace.Func),
              ToQStr(state.m_StencilState.m_BackFace.FailOp),
              ToQStr(state.m_StencilState.m_BackFace.DepthFailOp),
              ToQStr(state.m_StencilState.m_BackFace.PassOp),
              QString("%1").arg(state.m_StencilState.m_BackFace.WriteMask, 2, 16, QChar('0')).toUpper(),
              QString("%1").arg(state.m_StencilState.m_BackFace.ValueMask, 2, 16, QChar('0')).toUpper(),
              QString("%1").arg(state.m_StencilState.m_BackFace.Ref, 2, 16, QChar('0')).toUpper()})});
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

QString GLPipelineStateViewer::formatMembers(int indent, const QString &nameprefix,
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

void GLPipelineStateViewer::resource_itemActivated(QTreeWidgetItem *item, int column)
{
  const GLPipelineState::ShaderStage *stage = stageForSender(item->treeWidget());

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
  else if(tag.canConvert<ReadWriteTag>())
  {
    ReadWriteTag buf = tag.value<ReadWriteTag>();

    const ShaderResource &shaderRes = stage->ShaderDetails->ReadWriteResources[buf.bindPoint];

    QString format = QString("// struct %1\n").arg(ToQStr(shaderRes.variableType.descriptor.name));

    if(shaderRes.variableType.members.count > 1)
    {
      format += "// members skipped as they are fixed size:\n";
      for(int i = 0; i < shaderRes.variableType.members.count - 1; i++)
        format += QString("%1 %2;\n")
                      .arg(ToQStr(shaderRes.variableType.members[i].type.descriptor.name))
                      .arg(ToQStr(shaderRes.variableType.members[i].name));
    }

    if(!shaderRes.variableType.members.empty())
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

      if(!desc.name.empty())
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

void GLPipelineStateViewer::ubo_itemActivated(QTreeWidgetItem *item, int column)
{
  const GLPipelineState::ShaderStage *stage = stageForSender(item->treeWidget());

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
      new ConstantBufferPreviewer(m_Ctx, stage->stage, cb, 0, m_Ctx->mainWindow());

  m_Ctx->setupDockWindow(prev);

  ToolWindowManager *manager = ToolWindowManager::managerOf(this);

  ToolWindowManager::AreaReference ref(ToolWindowManager::RightOf, manager->areaOf(this), 0.3f);
  manager->addToolWindow(prev, ref);
}

void GLPipelineStateViewer::on_viAttrs_itemActivated(QTreeWidgetItem *item, int column)
{
  on_meshView_clicked();
}

void GLPipelineStateViewer::on_viBuffers_itemActivated(QTreeWidgetItem *item, int column)
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

void GLPipelineStateViewer::highlightIABind(int slot)
{
  int idx = ((slot + 1) * 21) % 32;    // space neighbouring colours reasonably distinctly

  const GLPipelineState::VertexInput &VI = m_Ctx->CurGLPipelineState.m_VtxIn;

  QColor col = QColor::fromHslF(float(idx) / 32.0f, 1.0f, 0.95f);

  if(slot < m_VBNodes.count())
  {
    QTreeWidgetItem *item = m_VBNodes[(int)slot];

    for(int c = 0; c < item->columnCount(); c++)
      item->setBackground(c, QBrush(col));
  }

  for(int i = 0; i < ui->viAttrs->topLevelItemCount(); i++)
  {
    QTreeWidgetItem *item = ui->viAttrs->topLevelItem(i);

    QBrush itemBrush = QBrush(col);

    if((int)VI.attributes[i].BufferSlot != slot)
      itemBrush = QBrush();

    for(int c = 0; c < item->columnCount(); c++)
      item->setBackground(c, itemBrush);
  }
}

void GLPipelineStateViewer::on_viAttrs_mouseMove(QMouseEvent *e)
{
  if(!m_Ctx->LogLoaded())
    return;

  QModelIndex idx = ui->viAttrs->indexAt(e->pos());

  vertex_leave(NULL);

  const GLPipelineState::VertexInput &VI = m_Ctx->CurGLPipelineState.m_VtxIn;

  if(idx.isValid())
  {
    if(idx.row() >= 0 && idx.row() < VI.attributes.count)
    {
      uint32_t buffer = VI.attributes[idx.row()].BufferSlot;

      highlightIABind((int)buffer);
    }
  }
}

void GLPipelineStateViewer::on_viBuffers_mouseMove(QMouseEvent *e)
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

void GLPipelineStateViewer::vertex_leave(QEvent *e)
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

void GLPipelineStateViewer::shaderView_clicked()
{
  const GLPipelineState::ShaderStage *stage =
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

void GLPipelineStateViewer::shaderEdit_clicked()
{
}

void GLPipelineStateViewer::shaderSave_clicked()
{
  const GLPipelineState::ShaderStage *stage =
      stageForSender(qobject_cast<QWidget *>(QObject::sender()));

  if(stage == NULL)
    return;

  ShaderReflection *shaderDetails = stage->ShaderDetails;

  if(stage->Shader == ResourceId())
    return;

  QString filename =
      RDDialog::getSaveFileName(this, tr("Save Shader As"), QString(), "GLSL files (*.glsl)");

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

void GLPipelineStateViewer::on_exportHTML_clicked()
{
}

void GLPipelineStateViewer::on_meshView_clicked()
{
  if(!m_Ctx->hasMeshPreview())
    m_Ctx->showMeshPreview();
  ToolWindowManager::raiseToolWindow(m_Ctx->meshPreview());
}
