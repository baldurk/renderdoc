/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Baldur Karlsson
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

#include "TextureViewer.h"
#include <float.h>
#include <math.h>
#include <QClipboard>
#include <QColorDialog>
#include <QJsonDocument>
#include <QMenu>
#include <QPainter>
#include <QStyledItemDelegate>
#include "3rdparty/toolwindowmanager/ToolWindowManagerArea.h"
#include "Code/CaptureContext.h"
#include "Dialogs/TextureSaveDialog.h"
#include "Widgets/ResourcePreview.h"
#include "Widgets/TextureGoto.h"
#include "FlowLayout.h"
#include "ui_TextureViewer.h"

float area(const QSizeF &s)
{
  return s.width() * s.height();
}

float aspect(const QSizeF &s)
{
  return s.width() / s.height();
}

Q_DECLARE_METATYPE(Following);
Q_DECLARE_METATYPE(ResourceId);

const Following Following::Default = Following();

Following::Following(FollowType t, ShaderStageType s, int i, int a)
{
  Type = t;
  Stage = s;
  index = i;
  arrayEl = a;
}

Following::Following()
{
  Type = FollowType::OutputColour;
  Stage = eShaderStage_Pixel;
  index = 0;
  arrayEl = 0;
}

bool Following::operator!=(const Following &o)
{
  return !(*this == o);
}

bool Following::operator==(const Following &o)
{
  return Type == o.Type && Stage == o.Stage && index == o.index;
}

void Following::GetDrawContext(CaptureContext *ctx, bool &copy, bool &compute)
{
  const FetchDrawcall *curDraw = ctx->CurDrawcall();
  copy = curDraw != NULL && (curDraw->flags & (eDraw_Copy | eDraw_Resolve));
  compute = curDraw != NULL && (curDraw->flags & eDraw_Dispatch) &&
            ctx->CurPipelineState.GetShader(eShaderStage_Compute) != ResourceId();
}

int Following::GetHighestMip(CaptureContext *ctx)
{
  return GetBoundResource(ctx, arrayEl).HighestMip;
}

int Following::GetFirstArraySlice(CaptureContext *ctx)
{
  return GetBoundResource(ctx, arrayEl).FirstSlice;
}

FormatComponentType Following::GetTypeHint(CaptureContext *ctx)
{
  return GetBoundResource(ctx, arrayEl).typeHint;
}

ResourceId Following::GetResourceId(CaptureContext *ctx)
{
  return GetBoundResource(ctx, arrayEl).Id;
}

BoundResource Following::GetBoundResource(CaptureContext *ctx, int arrayIdx)
{
  BoundResource ret;

  if(Type == FollowType::OutputColour)
  {
    auto outputs = GetOutputTargets(ctx);

    if(index < outputs.size())
      ret = outputs[index];
  }
  else if(Type == FollowType::OutputDepth)
  {
    ret = GetDepthTarget(ctx);
  }
  else if(Type == FollowType::ReadWrite)
  {
    auto rw = GetReadWriteResources(ctx);

    ShaderBindpointMapping mapping = GetMapping(ctx);

    if(index < mapping.ReadWriteResources.count)
    {
      BindpointMap &key = mapping.ReadWriteResources[index];

      if(rw.contains(key))
        ret = rw[key][arrayIdx];
    }
  }
  else if(Type == FollowType::ReadOnly)
  {
    auto ro = GetReadOnlyResources(ctx);

    ShaderBindpointMapping mapping = GetMapping(ctx);

    if(index < mapping.ReadOnlyResources.count)
    {
      BindpointMap &key = mapping.ReadOnlyResources[index];

      if(ro.contains(key))
        ret = ro[key][arrayIdx];
    }
  }

  return ret;
}

QVector<BoundResource> Following::GetOutputTargets(CaptureContext *ctx)
{
  const FetchDrawcall *curDraw = ctx->CurDrawcall();
  bool copy = false, compute = false;
  GetDrawContext(ctx, copy, compute);

  if(copy)
  {
    return {BoundResource(curDraw->copyDestination)};
  }
  else if(compute)
  {
    return {};
  }
  else
  {
    QVector<BoundResource> ret = ctx->CurPipelineState.GetOutputTargets();

    if(ret.isEmpty() && curDraw != NULL && (curDraw->flags & eDraw_Present))
    {
      if(curDraw->copyDestination != ResourceId())
        return {BoundResource(curDraw->copyDestination)};

      for(const FetchTexture &tex : ctx->GetTextures())
      {
        if(tex.creationFlags & eTextureCreate_SwapBuffer)
          return {BoundResource(tex.ID)};
      }
    }

    return ret;
  }
}

BoundResource Following::GetDepthTarget(CaptureContext *ctx)
{
  bool copy = false, compute = false;
  GetDrawContext(ctx, copy, compute);

  if(copy || compute)
    return BoundResource(ResourceId());
  else
    return ctx->CurPipelineState.GetDepthTarget();
}

QMap<BindpointMap, QVector<BoundResource>> Following::GetReadWriteResources(CaptureContext *ctx,
                                                                            ShaderStageType stage)
{
  bool copy = false, compute = false;
  GetDrawContext(ctx, copy, compute);

  if(copy)
  {
    return QMap<BindpointMap, QVector<BoundResource>>();
  }
  else if(compute)
  {
    // only return compute resources for one stage
    if(stage == eShaderStage_Pixel || stage == eShaderStage_Compute)
      return ctx->CurPipelineState.GetReadWriteResources(eShaderStage_Compute);
    else
      return QMap<BindpointMap, QVector<BoundResource>>();
  }
  else
  {
    return ctx->CurPipelineState.GetReadWriteResources(stage);
  }
}

QMap<BindpointMap, QVector<BoundResource>> Following::GetReadWriteResources(CaptureContext *ctx)
{
  return GetReadWriteResources(ctx, Stage);
}

QMap<BindpointMap, QVector<BoundResource>> Following::GetReadOnlyResources(CaptureContext *ctx,
                                                                           ShaderStageType stage)
{
  const FetchDrawcall *curDraw = ctx->CurDrawcall();
  bool copy = false, compute = false;
  GetDrawContext(ctx, copy, compute);

  if(copy)
  {
    QMap<BindpointMap, QVector<BoundResource>> ret;

    // only return copy source for one stage
    if(stage == eShaderStage_Pixel)
      ret[BindpointMap(0, 0)] = {BoundResource(curDraw->copySource)};

    return ret;
  }
  else if(compute)
  {
    // only return compute resources for one stage
    if(stage == eShaderStage_Pixel || stage == eShaderStage_Compute)
      return ctx->CurPipelineState.GetReadOnlyResources(eShaderStage_Compute);
    else
      return QMap<BindpointMap, QVector<BoundResource>>();
  }
  else
  {
    return ctx->CurPipelineState.GetReadOnlyResources(stage);
  }
}

QMap<BindpointMap, QVector<BoundResource>> Following::GetReadOnlyResources(CaptureContext *ctx)
{
  return GetReadOnlyResources(ctx, Stage);
}

ShaderReflection *Following::GetReflection(CaptureContext *ctx, ShaderStageType stage)
{
  bool copy = false, compute = false;
  GetDrawContext(ctx, copy, compute);

  if(copy)
    return NULL;
  else if(compute)
    return ctx->CurPipelineState.GetShaderReflection(eShaderStage_Compute);
  else
    return ctx->CurPipelineState.GetShaderReflection(stage);
}

ShaderReflection *Following::GetReflection(CaptureContext *ctx)
{
  return GetReflection(ctx, Stage);
}

ShaderBindpointMapping Following::GetMapping(CaptureContext *ctx, ShaderStageType stage)
{
  bool copy = false, compute = false;
  GetDrawContext(ctx, copy, compute);

  if(copy)
  {
    ShaderBindpointMapping mapping;

    // for PS only add a single mapping to get the copy source
    if(stage == eShaderStage_Pixel)
      mapping.ReadOnlyResources = {BindpointMap(0, 0)};

    return mapping;
  }
  else if(compute)
  {
    return ctx->CurPipelineState.GetBindpointMapping(eShaderStage_Compute);
  }
  else
  {
    return ctx->CurPipelineState.GetBindpointMapping(stage);
  }
}

ShaderBindpointMapping Following::GetMapping(CaptureContext *ctx)
{
  return GetMapping(ctx, Stage);
}

class TextureListItemModel : public QAbstractItemModel
{
public:
  enum FilterType
  {
    Textures,
    RenderTargets,
    String
  };

  TextureListItemModel(QObject *parent) : QAbstractItemModel(parent) {}
  void reset(FilterType type, const QString &filter, CaptureContext *ctx)
  {
    const rdctype::array<FetchTexture> src = ctx->GetTextures();

    texs.clear();
    texs.reserve(src.count);

    emit beginResetModel();

    int rtFlags = eTextureCreate_RTV | eTextureCreate_DSV;

    for(const FetchTexture &t : src)
    {
      if(type == Textures)
      {
        if((t.creationFlags & rtFlags) == 0)
          texs.push_back(t);
      }
      else if(type == RenderTargets)
      {
        if((t.creationFlags & rtFlags))
          texs.push_back(t);
      }
      else
      {
        if(filter.isEmpty())
          texs.push_back(t);
        else if(QString(t.name).contains(filter, Qt::CaseInsensitive))
          texs.push_back(t);
      }
    }

    emit endResetModel();
  }

  QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override
  {
    if(row < 0 || row >= rowCount())
      return QModelIndex();

    return createIndex(row, 0);
  }

  QModelIndex parent(const QModelIndex &index) const override { return QModelIndex(); }
  int rowCount(const QModelIndex &parent = QModelIndex()) const override { return texs.count(); }
  int columnCount(const QModelIndex &parent = QModelIndex()) const override { return 1; }
  Qt::ItemFlags flags(const QModelIndex &index) const override
  {
    if(!index.isValid())
      return 0;

    return QAbstractItemModel::flags(index);
  }

  QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override
  {
    if(index.isValid())
    {
      if(role == Qt::DisplayRole)
      {
        if(index.row() >= 0 && index.row() < texs.count())
          return QVariant(texs[index.row()].name);
      }

      if(role == Qt::UserRole)
      {
        return QVariant::fromValue(texs[index.row()].ID);
      }

      if(role == Qt::DecorationRole)
      {
        QIcon goArrow;
        goArrow.addFile(QStringLiteral(":/Resources/RightArrow_Gray_16x16.png"), QSize(),
                        QIcon::Normal, QIcon::Off);
        goArrow.addFile(QStringLiteral(":/Resources/RightArrow_Green_16x16.png"), QSize(),
                        QIcon::Active, QIcon::Off);
        return QVariant(goArrow);
      }
    }

    return QVariant();
  }

private:
  QVector<FetchTexture> texs;
};

class TextureListItemDelegate : public QItemDelegate
{
public:
  TextureListItemDelegate(QObject *parent = 0) : QItemDelegate(parent) {}
  void paint(QPainter *painter, const QStyleOptionViewItem &opt, const QModelIndex &index) const override
  {
    if(index.isValid())
    {
      QStyleOptionViewItem option = opt;
      option.decorationAlignment = Qt::AlignBaseline | Qt::AlignRight;
      painter->eraseRect(option.rect);

      QIcon icon = index.model()->data(index, Qt::DecorationRole).value<QIcon>();

      drawBackground(painter, option, index);
      if(option.state & QStyle::State_MouseOver)
        drawDecoration(painter, option, option.rect,
                       icon.pixmap(option.decorationSize, QIcon::Active));
      else
        drawDecoration(painter, option, option.rect,
                       icon.pixmap(option.decorationSize, QIcon::Normal));
      drawDisplay(painter, option, option.rect,
                  index.model()->data(index, Qt::DisplayRole).toString());
      drawFocus(painter, option, option.rect);

      if(option.state & QStyle::State_MouseOver)
      {
        QRect r = option.rect;
        r.adjust(0, 0, -1, -1);

        painter->drawRect(r);
      }
    }
  }
};

FetchTexture *TextureViewer::GetCurrentTexture()
{
  return m_CachedTexture;
}

void TextureViewer::UI_UpdateCachedTexture()
{
  if(!m_Ctx->LogLoaded())
  {
    m_CachedTexture = NULL;
    return;
  }

  ResourceId id = m_LockedId;
  if(id == ResourceId())
    id = m_Following.GetResourceId(m_Ctx);

  if(id == ResourceId())
    id = m_TexDisplay.texid;

  m_CachedTexture = m_Ctx->GetTexture(id);
}

TextureViewer::TextureViewer(CaptureContext *ctx, QWidget *parent)
    : QFrame(parent), ui(new Ui::TextureViewer), m_Ctx(ctx)
{
  ui->setupUi(this);

  m_Ctx->AddLogViewer(this);

  Reset();

  on_checkerBack_clicked();

  QObject::connect(ui->zoomOption->lineEdit(), &QLineEdit::returnPressed, this,
                   &TextureViewer::zoomOption_returnPressed);

  QObject::connect(ui->depthDisplay, &QToolButton::toggled, this,
                   &TextureViewer::channelsWidget_toggled);
  QObject::connect(ui->stencilDisplay, &QToolButton::toggled, this,
                   &TextureViewer::channelsWidget_toggled);
  QObject::connect(ui->flip_y, &QToolButton::toggled, this, &TextureViewer::channelsWidget_toggled);
  QObject::connect(ui->channelRed, &QToolButton::toggled, this,
                   &TextureViewer::channelsWidget_toggled);
  QObject::connect(ui->channelGreen, &QToolButton::toggled, this,
                   &TextureViewer::channelsWidget_toggled);
  QObject::connect(ui->channelBlue, &QToolButton::toggled, this,
                   &TextureViewer::channelsWidget_toggled);
  QObject::connect(ui->channelAlpha, &QToolButton::toggled, this,
                   &TextureViewer::channelsWidget_toggled);
  QObject::connect(ui->gammaDisplay, &QToolButton::toggled, this,
                   &TextureViewer::channelsWidget_toggled);
  QObject::connect(ui->channels, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                   &TextureViewer::channelsWidget_selected);
  QObject::connect(ui->hdrMul, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                   &TextureViewer::channelsWidget_selected);
  QObject::connect(ui->customShader, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                   &TextureViewer::channelsWidget_selected);
  QObject::connect(ui->rangeHistogram, &RangeHistogram::rangeUpdated, this,
                   &TextureViewer::range_rangeUpdated);
  QObject::connect(ui->rangeBlack, &RDLineEdit::textChanged, this,
                   &TextureViewer::rangePoint_textChanged);
  QObject::connect(ui->rangeBlack, &RDLineEdit::leave, this, &TextureViewer::rangePoint_leave);
  QObject::connect(ui->rangeBlack, &RDLineEdit::keyPress, this, &TextureViewer::rangePoint_keyPress);
  QObject::connect(ui->rangeWhite, &RDLineEdit::textChanged, this,
                   &TextureViewer::rangePoint_textChanged);
  QObject::connect(ui->rangeWhite, &RDLineEdit::leave, this, &TextureViewer::rangePoint_leave);
  QObject::connect(ui->rangeWhite, &RDLineEdit::keyPress, this, &TextureViewer::rangePoint_keyPress);

  QWidget *renderContainer = ui->renderContainer;

  ui->dockarea->addToolWindow(ui->renderContainer, ToolWindowManager::EmptySpace);
  ui->dockarea->setToolWindowProperties(renderContainer, ToolWindowManager::DisallowUserDocking |
                                                             ToolWindowManager::HideCloseButton |
                                                             ToolWindowManager::DisableDraggableTab);

  ui->dockarea->addToolWindow(ui->inputThumbs, ToolWindowManager::AreaReference(
                                                   ToolWindowManager::RightOf,
                                                   ui->dockarea->areaOf(renderContainer), 0.25f));
  ui->dockarea->setToolWindowProperties(ui->inputThumbs, ToolWindowManager::HideCloseButton);

  ui->dockarea->addToolWindow(
      ui->outputThumbs, ToolWindowManager::AreaReference(ToolWindowManager::AddTo,
                                                         ui->dockarea->areaOf(ui->inputThumbs)));
  ui->dockarea->setToolWindowProperties(ui->outputThumbs, ToolWindowManager::HideCloseButton);

  ui->dockarea->addToolWindow(
      ui->pixelContextLayout,
      ToolWindowManager::AreaReference(ToolWindowManager::BottomOf,
                                       ui->dockarea->areaOf(ui->outputThumbs), 0.25f));
  ui->dockarea->setToolWindowProperties(ui->pixelContextLayout, ToolWindowManager::HideCloseButton);

  ui->dockarea->addToolWindow(ui->textureListFrame, ToolWindowManager::NoArea);

  ui->dockarea->setAllowFloatingWindow(false);
  ui->dockarea->setRubberBandLineWidth(50);

  renderContainer->setWindowTitle(tr("Unbound"));
  ui->pixelContextLayout->setWindowTitle(tr("Pixel Context"));
  ui->outputThumbs->setWindowTitle(tr("Outputs"));
  ui->inputThumbs->setWindowTitle(tr("Inputs"));
  ui->textureListFrame->setWindowTitle(tr("Texture List"));

  ui->textureList->setHoverCursor(Qt::PointingHandCursor);

  m_Goto = new TextureGoto(this, [this](QPoint p) { GotoLocation(p.x(), p.y()); });

  QVBoxLayout *vertical = new QVBoxLayout(this);

  vertical->setSpacing(3);
  vertical->setContentsMargins(0, 0, 0, 0);

  QWidget *flow1widget = new QWidget(this);
  QWidget *flow2widget = new QWidget(this);

  FlowLayout *flow1 = new FlowLayout(flow1widget, 0, 3, 3);
  FlowLayout *flow2 = new FlowLayout(flow2widget, 0, 3, 3);

  flow1widget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
  flow2widget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);

  flow1->addWidget(ui->channelsToolbar);
  flow1->addWidget(ui->subresourceToolbar);
  flow1->addWidget(ui->actionToolbar);

  flow2->addWidget(ui->zoomToolbar);
  flow2->addWidget(ui->overlayToolbar);
  flow2->addWidget(ui->rangeToolbar);

  vertical->addWidget(flow1widget);
  vertical->addWidget(flow2widget);
  vertical->addWidget(ui->dockarea);

  Ui_TextureViewer *u = ui;
  u->pixelcontextgrid->setAlignment(u->pixelHistory, Qt::AlignCenter);
  u->pixelcontextgrid->setAlignment(u->debugPixelContext, Qt::AlignCenter);

  QWidget *statusflowWidget = new QWidget(this);

  FlowLayout *statusflow = new FlowLayout(statusflowWidget, 0, 3, 0);

  statusflowWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);

  ui->statusbar->removeWidget(ui->texStatusDim);
  ui->statusbar->removeWidget(ui->pickSwatch);
  ui->statusbar->removeWidget(ui->statusText);

  statusflow->addWidget(ui->texStatusDim);
  statusflow->addWidget(ui->pickSwatch);
  statusflow->addWidget(ui->statusText);

  ui->statusbar->addWidget(statusflowWidget);

  ui->channels->addItems({tr("RGBA"), tr("RGBM"), tr("Custom")});

  ui->zoomOption->addItems({"10%", "25%", "50%", "75%", "100%", "200%", "400%", "800%"});

  ui->hdrMul->addItems({"2", "4", "8", "16", "32", "128"});

  ui->overlay->addItems({tr("None"), tr("Highlight Drawcall"), tr("Wireframe Mesh"),
                         tr("Depth Test"), tr("Stencil Test"), tr("Backface Cull"),
                         tr("Viewport/Scissor Region"), tr("NaN/INF/-ve Display"), tr("Clipping"),
                         tr("Clear Before Pass"), tr("Clear Before Draw"),
                         tr("Quad Overdraw (Pass)"), tr("Quad Overdraw (Draw)"),
                         tr("Triangle Size (Pass)"), tr("Triangle Size (Draw)")});

  ui->textureListFilter->addItems({"", "Textures", "Render Targets"});

  ui->textureList->setModel(new TextureListItemModel(this));
  ui->textureList->setItemDelegate(new TextureListItemDelegate(ui->textureList));
  ui->textureList->viewport()->setAttribute(Qt::WA_Hover);

  ui->zoomOption->setCurrentText("");
  ui->fitToWindow->toggle();

  SetupTextureTabs();
}

TextureViewer::~TextureViewer()
{
  m_Ctx->RemoveLogViewer(this);
  delete ui;
}

void TextureViewer::RT_FetchCurrentPixel(uint32_t x, uint32_t y, PixelValue &pickValue,
                                         PixelValue &realValue)
{
  FetchTexture *texptr = GetCurrentTexture();

  if(texptr == NULL)
    return;

  if(m_TexDisplay.FlipY)
    y = (texptr->height - 1) - y;

  m_Output->PickPixel(m_TexDisplay.texid, true, x, y, m_TexDisplay.sliceFace, m_TexDisplay.mip,
                      m_TexDisplay.sampleIdx, &pickValue);

  if(m_TexDisplay.CustomShader != ResourceId())
    m_Output->PickPixel(m_TexDisplay.texid, false, x, y, m_TexDisplay.sliceFace, m_TexDisplay.mip,
                        m_TexDisplay.sampleIdx, &realValue);
}

void TextureViewer::RT_PickPixelsAndUpdate(IReplayRenderer *)
{
  PixelValue pickValue, realValue;

  uint32_t x = (uint32_t)m_PickedPoint.x();
  uint32_t y = (uint32_t)m_PickedPoint.y();

  RT_FetchCurrentPixel(x, y, pickValue, realValue);

  m_Output->SetPixelContextLocation(x, y);

  m_CurHoverValue = pickValue;

  m_CurPixelValue = pickValue;
  m_CurRealValue = realValue;

  GUIInvoke::call([this]() { UI_UpdateStatusText(); });
}

void TextureViewer::RT_PickHoverAndUpdate(IReplayRenderer *)
{
  PixelValue pickValue, realValue;

  uint32_t x = (uint32_t)m_CurHoverPixel.x();
  uint32_t y = (uint32_t)m_CurHoverPixel.y();

  RT_FetchCurrentPixel(x, y, pickValue, realValue);

  m_CurHoverValue = pickValue;

  GUIInvoke::call([this]() { UI_UpdateStatusText(); });
}

void TextureViewer::RT_UpdateAndDisplay(IReplayRenderer *)
{
  if(m_Output != NULL)
    m_Output->SetTextureDisplay(m_TexDisplay);

  GUIInvoke::call([this]() { ui->render->update(); });
}

void TextureViewer::RT_UpdateVisualRange(IReplayRenderer *)
{
  FetchTexture *texptr = GetCurrentTexture();

  if(!m_Visualise || texptr == NULL || m_Output == NULL)
    return;

  ResourceFormat fmt = texptr->format;

  if(m_TexDisplay.CustomShader != ResourceId())
    fmt.compCount = 4;

  bool success = true;

  bool channels[] = {
      m_TexDisplay.Red ? true : false, m_TexDisplay.Green && fmt.compCount > 1,
      m_TexDisplay.Blue && fmt.compCount > 2, m_TexDisplay.Alpha && fmt.compCount > 3,
  };

  rdctype::array<uint32_t> histogram;
  success = m_Output->GetHistogram(ui->rangeHistogram->rangeMin(), ui->rangeHistogram->rangeMax(),
                                   channels, &histogram);

  if(success)
  {
    QVector<uint32_t> histogramVec(histogram.count);
    if(histogram.count > 0)
      memcpy(histogramVec.data(), histogram.elems, histogram.count * sizeof(uint32_t));

    GUIInvoke::call([this, histogramVec]() {
      ui->rangeHistogram->setHistogramRange(ui->rangeHistogram->rangeMin(),
                                            ui->rangeHistogram->rangeMax());
      ui->rangeHistogram->setHistogramData(histogramVec);
    });
  }
}

void TextureViewer::UI_UpdateStatusText()
{
  FetchTexture *texptr = GetCurrentTexture();
  if(texptr == NULL)
    return;

  FetchTexture &tex = *texptr;

  bool dsv =
      ((tex.creationFlags & eTextureCreate_DSV) != 0) || (tex.format.compType == eCompType_Depth);
  bool uintTex = (tex.format.compType == eCompType_UInt);
  bool sintTex = (tex.format.compType == eCompType_SInt);

  if(m_TexDisplay.overlay == eTexOverlay_QuadOverdrawPass ||
     m_TexDisplay.overlay == eTexOverlay_QuadOverdrawDraw)
  {
    dsv = false;
    uintTex = false;
    sintTex = true;
  }

  QColor swatchColor;

  if(dsv || uintTex || sintTex)
  {
    swatchColor = QColor(0, 0, 0);
  }
  else
  {
    float r = qBound(0.0f, m_CurHoverValue.value_f[0], 1.0f);
    float g = qBound(0.0f, m_CurHoverValue.value_f[1], 1.0f);
    float b = qBound(0.0f, m_CurHoverValue.value_f[2], 1.0f);

    if(tex.format.srgbCorrected || (tex.creationFlags & eTextureCreate_SwapBuffer) > 0)
    {
      r = powf(r, 1.0f / 2.2f);
      g = powf(g, 1.0f / 2.2f);
      b = powf(b, 1.0f / 2.2f);
    }

    swatchColor = QColor(int(255.0f * r), int(255.0f * g), int(255.0f * b));
  }

  {
    QPalette Pal(palette());

    Pal.setColor(QPalette::Background, swatchColor);

    ui->pickSwatch->setAutoFillBackground(true);
    ui->pickSwatch->setPalette(Pal);
  }

  int y = m_CurHoverPixel.y() >> (int)m_TexDisplay.mip;

  uint32_t mipWidth = qMax(1U, tex.width >> (int)m_TexDisplay.mip);
  uint32_t mipHeight = qMax(1U, tex.height >> (int)m_TexDisplay.mip);

  if(m_Ctx->APIProps().pipelineType == eGraphicsAPI_OpenGL)
    y = (int)(mipHeight - 1) - y;
  if(m_TexDisplay.FlipY)
    y = (int)(mipHeight - 1) - y;

  y = qMax(0, y);

  int x = m_CurHoverPixel.x() >> (int)m_TexDisplay.mip;
  float invWidth = mipWidth > 0 ? 1.0f / mipWidth : 0.0f;
  float invHeight = mipHeight > 0 ? 1.0f / mipHeight : 0.0f;

  QString hoverCoords = QString("%1, %2 (%3, %4)")
                            .arg(x, 4)
                            .arg(y, 4)
                            .arg((x * invWidth), 5, 'f', 4)
                            .arg((y * invHeight), 5, 'f', 4);

  QString statusText = tr("Hover - ") + hoverCoords;

  uint32_t hoverX = (uint32_t)m_CurHoverPixel.x();
  uint32_t hoverY = (uint32_t)m_CurHoverPixel.y();

  if(hoverX > tex.width || hoverY > tex.height)
    statusText = tr("Hover - ") + "[" + hoverCoords + "]";

  if(m_PickedPoint.x() >= 0)
  {
    x = m_PickedPoint.x() >> (int)m_TexDisplay.mip;
    y = m_PickedPoint.y() >> (int)m_TexDisplay.mip;
    if(m_Ctx->APIProps().pipelineType == eGraphicsAPI_OpenGL)
      y = (int)(mipHeight - 1) - y;
    if(m_TexDisplay.FlipY)
      y = (int)(mipHeight - 1) - y;

    y = qMax(0, y);

    statusText += tr(" - Right click - ") + QString("%1, %2: ").arg(x, 4).arg(y, 4);

    PixelValue val = m_CurPixelValue;

    if(m_TexDisplay.CustomShader != ResourceId())
    {
      statusText += Formatter::Format(val.value_f[0]) + ", " + Formatter::Format(val.value_f[1]) +
                    ", " + Formatter::Format(val.value_f[2]) + ", " +
                    Formatter::Format(val.value_f[3]);

      val = m_CurRealValue;

      statusText += tr(" (Real: ");
    }

    if(dsv)
    {
      statusText += tr("Depth ");
      if(uintTex)
      {
        if(tex.format.compByteWidth == 2)
          statusText += Formatter::Format(val.value_u16[0]);
        else
          statusText += Formatter::Format(val.value_u[0]);
      }
      else
      {
        statusText += Formatter::Format(val.value_f[0]);
      }

      int stencil = (int)(255.0f * val.value_f[1]);

      statusText += tr(", Stencil %1 / 0x%2").arg(stencil).arg(stencil, 0, 16);
    }
    else
    {
      if(uintTex)
      {
        statusText += Formatter::Format(val.value_u[0]) + ", " + Formatter::Format(val.value_u[1]) +
                      ", " + Formatter::Format(val.value_u[2]) + ", " +
                      Formatter::Format(val.value_u[3]);
      }
      else if(sintTex)
      {
        statusText += Formatter::Format(val.value_i[0]) + ", " + Formatter::Format(val.value_i[1]) +
                      ", " + Formatter::Format(val.value_i[2]) + ", " +
                      Formatter::Format(val.value_i[3]);
      }
      else
      {
        statusText += Formatter::Format(val.value_f[0]) + ", " + Formatter::Format(val.value_f[1]) +
                      ", " + Formatter::Format(val.value_f[2]) + ", " +
                      Formatter::Format(val.value_f[3]);
      }
    }

    if(m_TexDisplay.CustomShader != ResourceId())
      statusText += ")";

    // PixelPicked = true;
  }
  else
  {
    statusText += tr(" - Right click to pick a pixel");

    if(m_Output != NULL)
    {
      m_Ctx->Renderer()->AsyncInvoke([this](IReplayRenderer *) { m_Output->DisablePixelContext(); });
    }

    // PixelPicked = false;
  }

  // try and keep status text consistent by sticking to the high water mark
  // of length (prevents nasty oscillation when the length of the string is
  // just popping over/under enough to overflow onto the next line).

  if(statusText.length() > m_HighWaterStatusLength)
    m_HighWaterStatusLength = statusText.length();

  if(statusText.length() < m_HighWaterStatusLength)
    statusText += QString(m_HighWaterStatusLength - statusText.length(), ' ');

  ui->statusText->setText(statusText);
}

void TextureViewer::UI_UpdateTextureDetails()
{
  QString status;

  FetchTexture *texptr = GetCurrentTexture();
  if(texptr == NULL)
  {
    ui->texStatusDim->setText(status);

    ui->renderContainer->setWindowTitle(tr("Unbound"));
    return;
  }

  FetchTexture &current = *texptr;

  ResourceId followID = m_Following.GetResourceId(m_Ctx);

  {
    FetchTexture *followtex = m_Ctx->GetTexture(followID);
    FetchBuffer *followbuf = m_Ctx->GetBuffer(followID);

    QString title;

    if(followID == ResourceId())
    {
      title = tr("Unbound");
    }
    else if(followtex || followbuf)
    {
      QString name;

      if(followtex)
        name = followtex->name;
      else
        name = followbuf->name;

      switch(m_Following.Type)
      {
        case FollowType::OutputColour:
          title = QString(tr("Cur Output %1 - %2")).arg(m_Following.index).arg(name);
          break;
        case FollowType::OutputDepth: title = QString(tr("Cur Depth Output - %1")).arg(name); break;
        case FollowType::ReadWrite:
          title = QString(tr("Cur RW Output %1 - %2")).arg(m_Following.index).arg(name);
          break;
        case FollowType::ReadOnly:
          title = QString(tr("Cur Input %1 - %2")).arg(m_Following.index).arg(name);
          break;
      }
    }
    else
    {
      switch(m_Following.Type)
      {
        case FollowType::OutputColour:
          title = QString(tr("Cur Output %1")).arg(m_Following.index);
          break;
        case FollowType::OutputDepth: title = QString(tr("Cur Depth Output")); break;
        case FollowType::ReadWrite:
          title = QString(tr("Cur RW Output %1")).arg(m_Following.index);
          break;
        case FollowType::ReadOnly:
          title = QString(tr("Cur Input %1")).arg(m_Following.index);
          break;
      }
    }

    ui->renderContainer->setWindowTitle(title);
  }

  status = QString(current.name) + " - ";

  if(current.dimension >= 1)
    status += QString::number(current.width);
  if(current.dimension >= 2)
    status += "x" + QString::number(current.height);
  if(current.dimension >= 3)
    status += "x" + QString::number(current.depth);

  if(current.arraysize > 1)
    status += "[" + QString::number(current.arraysize) + "]";

  if(current.msQual > 0 || current.msSamp > 1)
    status += QString(" MS{%1x %2Q}").arg(current.msSamp).arg(current.msQual);

  status += QString(" %1 mips").arg(current.mips);

  status += " - " + QString(current.format.strname);

  if(current.format.compType != m_TexDisplay.typeHint && m_TexDisplay.typeHint != eCompType_None)
  {
    status += tr(" Viewed as TODO");    // m_TexDisplay.typeHint.Str();
  }

  ui->texStatusDim->setText(status);
}

void TextureViewer::UI_OnTextureSelectionChanged(bool newdraw)
{
  FetchTexture *texptr = GetCurrentTexture();

  // reset high-water mark
  m_HighWaterStatusLength = 0;

  if(texptr == NULL)
    return;

  FetchTexture &tex = *texptr;

  bool newtex = (m_TexDisplay.texid != tex.ID);

  // save settings for this current texture
  if(m_Ctx->Config.TextureViewer_PerTexSettings)
  {
    m_TextureSettings[m_TexDisplay.texid].r = ui->channelRed->isChecked();
    m_TextureSettings[m_TexDisplay.texid].g = ui->channelGreen->isChecked();
    m_TextureSettings[m_TexDisplay.texid].b = ui->channelBlue->isChecked();
    m_TextureSettings[m_TexDisplay.texid].a = ui->channelAlpha->isChecked();

    m_TextureSettings[m_TexDisplay.texid].displayType = ui->channels->currentIndex();
    m_TextureSettings[m_TexDisplay.texid].customShader = ui->customShader->currentText();

    m_TextureSettings[m_TexDisplay.texid].depth = ui->depthDisplay->isChecked();
    m_TextureSettings[m_TexDisplay.texid].stencil = ui->stencilDisplay->isChecked();

    m_TextureSettings[m_TexDisplay.texid].mip = ui->mipLevel->currentIndex();
    m_TextureSettings[m_TexDisplay.texid].slice = ui->sliceFace->currentIndex();

    m_TextureSettings[m_TexDisplay.texid].minrange = ui->rangeHistogram->blackPoint();
    m_TextureSettings[m_TexDisplay.texid].maxrange = ui->rangeHistogram->whitePoint();

    m_TextureSettings[m_TexDisplay.texid].typeHint = m_Following.GetTypeHint(m_Ctx);
  }

  m_TexDisplay.texid = tex.ID;

  // interpret the texture according to the currently following type.
  if(!currentTextureIsLocked())
    m_TexDisplay.typeHint = m_Following.GetTypeHint(m_Ctx);
  else
    m_TexDisplay.typeHint = eCompType_None;

  // if there is no such type or it isn't being followed, use the last seen interpretation
  if(m_TexDisplay.typeHint == eCompType_None && m_TextureSettings.contains(m_TexDisplay.texid))
    m_TexDisplay.typeHint = m_TextureSettings[m_TexDisplay.texid].typeHint;

  // try to maintain the pan in the new texture. If the new texture
  // is approx an integer multiple of the old texture, just changing
  // the scale will keep everything the same. This is useful for
  // downsample chains and things where you're flipping back and forth
  // between overlapping textures, but even in the non-integer case
  // pan will be kept approximately the same.
  QSizeF curSize((float)tex.width, (float)tex.height);
  float curArea = area(curSize);
  float prevArea = area(m_PrevSize);

  if(prevArea > 0.0f)
  {
    float prevX = m_TexDisplay.offx;
    float prevY = m_TexDisplay.offy;

    // allow slight difference in aspect ratio for rounding errors
    // in downscales (e.g. 1680x1050 -> 840x525 -> 420x262 in the
    // last downscale the ratios are 1.6 and 1.603053435).
    if(qAbs(aspect(curSize) - aspect(m_PrevSize)) < 0.01f)
    {
      m_TexDisplay.scale *= m_PrevSize.width() / curSize.width();
      setCurrentZoomValue(m_TexDisplay.scale);
    }
    else
    {
      // this scale factor is arbitrary really, only intention is to have
      // integer scales come out precisely, other 'similar' sizes will be
      // similar ish
      float scaleFactor = (float)(sqrt(curArea) / sqrt(prevArea));

      m_TexDisplay.offx = prevX * scaleFactor;
      m_TexDisplay.offy = prevY * scaleFactor;
    }
  }

  m_PrevSize = curSize;

  // refresh scroll position
  setScrollPosition(getScrollPosition());

  UI_UpdateStatusText();

  ui->mipLevel->clear();

  m_TexDisplay.mip = 0;
  m_TexDisplay.sliceFace = 0;

  bool usemipsettings = true;
  bool useslicesettings = true;

  if(tex.msSamp > 1)
  {
    for(uint32_t i = 0; i < tex.msSamp; i++)
      ui->mipLevel->addItem(QString("Sample %1").arg(i));

    // add an option to display unweighted average resolved value,
    // to get an idea of how the samples average
    if(tex.format.compType != eCompType_UInt && tex.format.compType != eCompType_SInt &&
       tex.format.compType != eCompType_Depth && (tex.creationFlags & eTextureCreate_DSV) == 0)
      ui->mipLevel->addItem(tr("Average val"));

    ui->mipLabel->setText(tr("Sample"));

    ui->mipLevel->setCurrentIndex(0);
  }
  else
  {
    for(uint32_t i = 0; i < tex.mips; i++)
      ui->mipLevel->addItem(QString::number(i) + QString(" - ") +
                            QString::number(qMax(1U, tex.width >> i)) + QString("x") +
                            QString::number(qMax(1U, tex.height >> i)));

    ui->mipLabel->setText(tr("Mip"));

    int highestMip = -1;

    // only switch to the selected mip for outputs, and when changing drawcall
    if(!currentTextureIsLocked() && m_Following.Type != FollowType::ReadOnly && newdraw)
      highestMip = m_Following.GetHighestMip(m_Ctx);

    // assuming we get a valid mip for the highest mip, only switch to it
    // if we've selected a new texture, or if it's different than the last mip.
    // This prevents the case where the user has clicked on another mip and
    // we don't want to snap their view back when stepping between events with the
    // same mip used. But it does mean that if they are stepping between
    // events with different mips used, then we will update in that case.
    if(highestMip >= 0 && (newtex || highestMip != m_PrevHighestMip))
    {
      usemipsettings = false;
      ui->mipLevel->setCurrentIndex(qBound(0, highestMip, (int)tex.mips - 1));
    }

    if(ui->mipLevel->currentIndex() == -1)
      ui->mipLevel->setCurrentIndex(qBound(0, m_PrevHighestMip, (int)tex.mips - 1));

    m_PrevHighestMip = highestMip;
  }

  if(tex.mips == 1 && tex.msSamp <= 1)
    ui->mipLevel->setEnabled(false);
  else
    ui->mipLevel->setEnabled(true);

  ui->sliceFace->clear();

  if(tex.arraysize == 1 && tex.depth <= 1)
  {
    ui->sliceFace->setEnabled(false);
  }
  else
  {
    ui->mipLevel->setEnabled(true);

    QString cubeFaces[] = {"X+", "X-", "Y+", "Y-", "Z+", "Z-"};

    uint32_t numSlices = tex.arraysize;

    // for 3D textures, display the number of slices at this mip
    if(tex.depth > 1)
      numSlices = qMax(1u, tex.depth >> (int)ui->mipLevel->currentIndex());

    for(uint32_t i = 0; i < numSlices; i++)
    {
      if(tex.cubemap)
      {
        QString name = cubeFaces[i % 6];
        if(numSlices > 6)
          name = QString("[%1] %2").arg(i / 6).arg(
              cubeFaces[i % 6]);    // Front 1, Back 2, 3, 4 etc for cube arrays
        ui->sliceFace->addItem(name);
      }
      else
      {
        ui->sliceFace->addItem(tr("Slice ") + i);
      }
    }

    int firstArraySlice = -1;
    // only switch to the selected mip for outputs, and when changing drawcall
    if(!currentTextureIsLocked() && m_Following.Type != FollowType::ReadOnly && newdraw)
      firstArraySlice = m_Following.GetFirstArraySlice(m_Ctx);

    // see above with highestMip and prevHighestMip for the logic behind this
    if(firstArraySlice >= 0 && (newtex || firstArraySlice != m_PrevFirstArraySlice))
    {
      useslicesettings = false;
      ui->sliceFace->setCurrentIndex(qBound(0, firstArraySlice, (int)numSlices - 1));
    }

    if(ui->sliceFace->currentIndex() == -1)
      ui->sliceFace->setCurrentIndex(qBound(0, m_PrevFirstArraySlice, (int)numSlices - 1));

    m_PrevFirstArraySlice = firstArraySlice;
  }

  // because slice and mip are specially set above, we restore any per-tex settings to apply
  // even if we don't switch to a new texture.
  // Note that if the slice or mip was changed because that slice or mip is the selected one
  // at the API level, we leave this alone.
  if(m_Ctx->Config.TextureViewer_PerTexSettings && m_TextureSettings.contains(tex.ID))
  {
    if(usemipsettings)
      ui->mipLevel->setCurrentIndex(m_TextureSettings[tex.ID].mip);

    if(useslicesettings)
      ui->sliceFace->setCurrentIndex(m_TextureSettings[tex.ID].slice);
  }

  // handling for if we've switched to a new texture
  if(newtex)
  {
    // if we save certain settings per-texture, restore them (if we have any)
    if(m_Ctx->Config.TextureViewer_PerTexSettings && m_TextureSettings.contains(tex.ID))
    {
      ui->channels->setCurrentIndex(m_TextureSettings[tex.ID].displayType);

      ui->customShader->setCurrentText(m_TextureSettings[tex.ID].customShader);

      ui->channelRed->setChecked(m_TextureSettings[tex.ID].r);
      ui->channelGreen->setChecked(m_TextureSettings[tex.ID].g);
      ui->channelBlue->setChecked(m_TextureSettings[tex.ID].b);
      ui->channelAlpha->setChecked(m_TextureSettings[tex.ID].a);

      ui->depthDisplay->setChecked(m_TextureSettings[tex.ID].depth);
      ui->stencilDisplay->setChecked(m_TextureSettings[tex.ID].stencil);

      m_NoRangePaint = true;
      ui->rangeHistogram->setRange(m_TextureSettings[m_TexDisplay.texid].minrange,
                                   m_TextureSettings[m_TexDisplay.texid].maxrange);
      m_NoRangePaint = false;
    }
    else if(m_Ctx->Config.TextureViewer_PerTexSettings)
    {
      // if we are using per-tex settings, reset back to RGB
      ui->channels->setCurrentIndex(0);

      ui->customShader->setCurrentText("");

      ui->channelRed->setChecked(true);
      ui->channelGreen->setChecked(true);
      ui->channelBlue->setChecked(true);
      ui->channelAlpha->setChecked(false);

      ui->depthDisplay->setChecked(true);
      ui->stencilDisplay->setChecked(false);

      m_NoRangePaint = true;
      UI_SetHistogramRange(texptr, m_TexDisplay.typeHint);
      m_NoRangePaint = false;
    }

    // reset the range if desired
    if(m_Ctx->Config.TextureViewer_ResetRange)
    {
      UI_SetHistogramRange(texptr, m_TexDisplay.typeHint);
    }
  }

  UI_UpdateFittedScale();
  UI_UpdateTextureDetails();
  UI_UpdateChannels();

  if(ui->autoFit->isChecked())
    AutoFitRange();

  m_Ctx->Renderer()->AsyncInvoke([this](IReplayRenderer *r) {
    RT_UpdateVisualRange(r);

    RT_UpdateAndDisplay(r);

    if(m_Output != NULL)
      RT_PickPixelsAndUpdate(r);

    // TODO - GetUsage and update TimelineBar
  });
}

void TextureViewer::UI_SetHistogramRange(const FetchTexture *tex, FormatComponentType typeHint)
{
  if(tex != NULL && (tex->format.compType == eCompType_SNorm || typeHint == eCompType_SNorm))
    ui->rangeHistogram->setRange(-1.0f, 1.0f);
  else
    ui->rangeHistogram->setRange(0.0f, 1.0f);
}

void TextureViewer::UI_UpdateChannels()
{
  FetchTexture *tex = GetCurrentTexture();

#define SHOW(widget) widget->setVisible(true)
#define HIDE(widget) widget->setVisible(false)
#define ENABLE(widget) widget->setEnabled(true)
#define DISABLE(widget) widget->setEnabled(false)

  if(tex != NULL && (tex->creationFlags & eTextureCreate_SwapBuffer))
  {
    // swapbuffer is always srgb for 8-bit types, linear for 16-bit types
    DISABLE(ui->gammaDisplay);

    if(tex->format.compByteWidth == 2 && !tex->format.special)
      m_TexDisplay.linearDisplayAsGamma = false;
    else
      m_TexDisplay.linearDisplayAsGamma = true;
  }
  else
  {
    if(tex != NULL && !tex->format.srgbCorrected)
      ENABLE(ui->gammaDisplay);
    else
      DISABLE(ui->gammaDisplay);

    m_TexDisplay.linearDisplayAsGamma =
        !ui->gammaDisplay->isEnabled() || ui->gammaDisplay->isChecked();
  }

  if(tex != NULL && tex->format.srgbCorrected)
    m_TexDisplay.linearDisplayAsGamma = false;

  bool dsv = false;
  if(tex != NULL)
    dsv = ((tex->creationFlags & eTextureCreate_DSV) != 0) ||
          (tex->format.compType == eCompType_Depth);

  if(dsv && ui->channels->currentIndex() != 2)
  {
    // Depth display (when not using custom)

    HIDE(ui->channelRed);
    HIDE(ui->channelGreen);
    HIDE(ui->channelBlue);
    HIDE(ui->channelAlpha);
    HIDE(ui->mulSep);
    HIDE(ui->mulLabel);
    HIDE(ui->hdrMul);
    HIDE(ui->customShader);
    HIDE(ui->customCreate);
    HIDE(ui->customEdit);
    HIDE(ui->customDelete);
    SHOW(ui->depthStencilSep);
    SHOW(ui->depthDisplay);
    SHOW(ui->stencilDisplay);

    m_TexDisplay.Red = ui->depthDisplay->isChecked();
    m_TexDisplay.Green = ui->stencilDisplay->isChecked();
    m_TexDisplay.Blue = false;
    m_TexDisplay.Alpha = false;

    if(m_TexDisplay.Red == m_TexDisplay.Green && !m_TexDisplay.Red)
    {
      m_TexDisplay.Red = true;
      ui->depthDisplay->setChecked(true);
    }

    m_TexDisplay.HDRMul = -1.0f;
    if(m_TexDisplay.CustomShader != ResourceId())
    {
      memset(m_CurPixelValue.value_f, 0, sizeof(float) * 4);
      memset(m_CurRealValue.value_f, 0, sizeof(float) * 4);
      UI_UpdateStatusText();
    }
    m_TexDisplay.CustomShader = ResourceId();
  }
  else if(ui->channels->currentIndex() == 0 || !m_Ctx->LogLoaded())
  {
    // RGBA
    SHOW(ui->channelRed);
    SHOW(ui->channelGreen);
    SHOW(ui->channelBlue);
    SHOW(ui->channelAlpha);
    HIDE(ui->mulSep);
    HIDE(ui->mulLabel);
    HIDE(ui->hdrMul);
    HIDE(ui->customShader);
    HIDE(ui->customCreate);
    HIDE(ui->customEdit);
    HIDE(ui->customDelete);
    HIDE(ui->depthStencilSep);
    HIDE(ui->depthDisplay);
    HIDE(ui->stencilDisplay);

    m_TexDisplay.Red = ui->channelRed->isChecked();
    m_TexDisplay.Green = ui->channelGreen->isChecked();
    m_TexDisplay.Blue = ui->channelBlue->isChecked();
    m_TexDisplay.Alpha = ui->channelAlpha->isChecked();

    m_TexDisplay.HDRMul = -1.0f;
    if(m_TexDisplay.CustomShader != ResourceId())
    {
      memset(m_CurPixelValue.value_f, 0, sizeof(float) * 4);
      memset(m_CurRealValue.value_f, 0, sizeof(float) * 4);
      UI_UpdateStatusText();
    }
    m_TexDisplay.CustomShader = ResourceId();
  }
  else if(ui->channels->currentIndex() == 1)
  {
    // RGBM
    SHOW(ui->channelRed);
    SHOW(ui->channelGreen);
    SHOW(ui->channelBlue);
    HIDE(ui->channelAlpha);
    SHOW(ui->mulSep);
    SHOW(ui->mulLabel);
    SHOW(ui->hdrMul);
    HIDE(ui->customShader);
    HIDE(ui->customCreate);
    HIDE(ui->customEdit);
    HIDE(ui->customDelete);
    HIDE(ui->depthStencilSep);
    HIDE(ui->depthDisplay);
    HIDE(ui->stencilDisplay);

    m_TexDisplay.Red = ui->channelRed->isChecked();
    m_TexDisplay.Green = ui->channelGreen->isChecked();
    m_TexDisplay.Blue = ui->channelBlue->isChecked();
    m_TexDisplay.Alpha = false;

    bool ok = false;
    float mul = ui->hdrMul->currentText().toFloat(&ok);

    if(!ok)
    {
      mul = 32.0f;
      ui->hdrMul->setCurrentText("32");
    }

    m_TexDisplay.HDRMul = mul;
    if(m_TexDisplay.CustomShader != ResourceId())
    {
      memset(m_CurPixelValue.value_f, 0, sizeof(float) * 4);
      memset(m_CurRealValue.value_f, 0, sizeof(float) * 4);
      UI_UpdateStatusText();
    }
    m_TexDisplay.CustomShader = ResourceId();
  }
  else if(ui->channels->currentIndex() == 2)
  {
    // custom shaders
    SHOW(ui->channelRed);
    SHOW(ui->channelGreen);
    SHOW(ui->channelBlue);
    SHOW(ui->channelAlpha);
    HIDE(ui->mulSep);
    HIDE(ui->mulLabel);
    HIDE(ui->hdrMul);
    SHOW(ui->customShader);
    SHOW(ui->customCreate);
    SHOW(ui->customEdit);
    SHOW(ui->customDelete);
    HIDE(ui->depthStencilSep);
    HIDE(ui->depthDisplay);
    HIDE(ui->stencilDisplay);

    m_TexDisplay.Red = ui->channelRed->isChecked();
    m_TexDisplay.Green = ui->channelGreen->isChecked();
    m_TexDisplay.Blue = ui->channelBlue->isChecked();
    m_TexDisplay.Alpha = ui->channelAlpha->isChecked();

    m_TexDisplay.HDRMul = -1.0f;

    m_TexDisplay.CustomShader = ResourceId();
    /*
    if (m_CustomShaders.ContainsKey(customShader.Text.ToUpperInvariant()))
    {
      if (m_TexDisplay.CustomShader == ResourceId.Null) { m_CurPixelValue = null; m_CurRealValue =
  null; UI_UpdateStatusText(); }
      m_TexDisplay.CustomShader = m_CustomShaders[customShader.Text.ToUpperInvariant()];
      customDelete.Enabled = customEdit.Enabled = true;
    }
    else
    {
      customDelete.Enabled = customEdit.Enabled = false;
    }*/
  }

#undef HIDE
#undef SHOW
#undef ENABLE
#undef DISABLE

  m_TexDisplay.FlipY = ui->flip_y->isChecked();

  INVOKE_MEMFN(RT_UpdateAndDisplay);
  INVOKE_MEMFN(RT_UpdateVisualRange);
}

void TextureViewer::SetupTextureTabs()
{
  ToolWindowManagerArea *textureTabs = ui->dockarea->areaOf(ui->renderContainer);

  QIcon tabIcon;
  tabIcon.addFile(QStringLiteral(":/Resources/icon.ico"), QSize(), QIcon::Normal, QIcon::Off);

  textureTabs->setTabIcon(0, tabIcon);

  textureTabs->setElideMode(Qt::ElideRight);

  QObject::connect(textureTabs, &QTabWidget::currentChanged, this,
                   &TextureViewer::textureTab_Changed);
  QObject::connect(textureTabs, &QTabWidget::tabCloseRequested, this,
                   &TextureViewer::textureTab_Closing);

  textureTabs->disableUserDrop();
}

void TextureViewer::textureTab_Changed(int index)
{
  ToolWindowManagerArea *textureTabs = ui->dockarea->areaOf(ui->renderContainer);

  QWidget *w = textureTabs->widget(index);

  if(w)
  {
    w->setLayout(ui->renderLayout);

    if(w == ui->renderContainer)
      m_LockedId = ResourceId();
    else
      m_LockedId = w->property("id").value<ResourceId>();

    UI_UpdateCachedTexture();
  }

  UI_OnTextureSelectionChanged(false);
}

void TextureViewer::textureTab_Closing(int index)
{
  ToolWindowManagerArea *textureTabs = ui->dockarea->areaOf(ui->renderContainer);
  if(index > 0)
  {
    // this callback happens AFTER the widget has already been removed unfortunately, so
    // we need to search through the locked tab list to see which one was removed to be
    // able to delete it properly.
    QList<ResourceId> ids = m_LockedTabs.keys();
    for(int i = 0; i < textureTabs->count(); i++)
    {
      QWidget *w = textureTabs->widget(i);
      ResourceId id = w->property("id").value<ResourceId>();
      ids.removeOne(id);
    }

    if(ids.count() != 1)
      qWarning() << "Expected only one removed tab, got " << ids.count();

    for(ResourceId id : ids)
    {
      delete m_LockedTabs[id];
      m_LockedTabs.remove(id);
    }

    textureTabs->setCurrentIndex(index - 1);
    textureTabs->widget(index - 1)->show();

    return;
  }

  // should never get here - tab 0 is the dynamic tab which is uncloseable.
  qCritical() << "Somehow closing dynamic tab?";
  if(textureTabs->count() > 1)
  {
    textureTabs->setCurrentIndex(1);
    textureTabs->widget(1)->show();
  }
}

ResourcePreview *TextureViewer::UI_CreateThumbnail(ThumbnailStrip *strip)
{
  ResourcePreview *prev = new ResourcePreview(m_Ctx, m_Output);

  QObject::connect(prev, &ResourcePreview::clicked, this, &TextureViewer::thumb_clicked);
  QObject::connect(prev, &ResourcePreview::doubleClicked, this, &TextureViewer::thumb_doubleClicked);

  prev->setActive(false);
  strip->addThumb(prev);
  return prev;
}

void TextureViewer::UI_CreateThumbnails()
{
  if(!ui->outputThumbs->thumbs().isEmpty())
    return;

  // these will expand, but we make sure that there is a good set reserved
  for(int i = 0; i < 9; i++)
  {
    ResourcePreview *prev = UI_CreateThumbnail(ui->outputThumbs);

    if(i == 0)
      prev->setSelected(true);
  }

  for(int i = 0; i < 128; i++)
    UI_CreateThumbnail(ui->inputThumbs);
}

void TextureViewer::GotoLocation(int x, int y)
{
  if(!m_Ctx->LogLoaded())
    return;

  FetchTexture *tex = GetCurrentTexture();

  if(tex == NULL)
    return;

  m_PickedPoint = QPoint(x, y);

  uint32_t mipHeight = qMax(1U, tex->height >> (int)m_TexDisplay.mip);
  if(m_Ctx->APIProps().pipelineType == eGraphicsAPI_OpenGL)
    m_PickedPoint.setY((int)(mipHeight - 1) - m_PickedPoint.y());
  if(m_TexDisplay.FlipY)
    m_PickedPoint.setY((int)(mipHeight - 1) - m_PickedPoint.x());

  if(m_Output != NULL)
    INVOKE_MEMFN(RT_PickPixelsAndUpdate);
  INVOKE_MEMFN(RT_UpdateAndDisplay);

  UI_UpdateStatusText();
}

void TextureViewer::ViewTexture(ResourceId ID, bool focus)
{
  if(QThread::currentThread() != QCoreApplication::instance()->thread())
  {
    GUIInvoke::call([this, ID, focus] { this->ViewTexture(ID, focus); });
    return;
  }

  if(m_LockedTabs.contains(ID))
  {
    if(focus)
      show();

    QWidget *w = m_LockedTabs[ID];
    ToolWindowManagerArea *textureTabs = ui->dockarea->areaOf(ui->renderContainer);

    int idx = textureTabs->indexOf(w);

    if(idx >= 0)
      textureTabs->setCurrentIndex(idx);

    INVOKE_MEMFN(RT_UpdateAndDisplay);
    return;
  }

  FetchTexture *tex = m_Ctx->GetTexture(ID);
  if(tex)
  {
    QWidget *lockedContainer = new QWidget(this);
    lockedContainer->setWindowTitle(QString(tex->name));
    lockedContainer->setProperty("id", QVariant::fromValue(ID));

    ToolWindowManagerArea *textureTabs = ui->dockarea->areaOf(ui->renderContainer);

    ToolWindowManager::AreaReference ref(ToolWindowManager::AddTo, textureTabs);

    ui->dockarea->addToolWindow(lockedContainer, ref);
    ui->dockarea->setToolWindowProperties(lockedContainer, ToolWindowManager::DisallowUserDocking);

    lockedContainer->setLayout(ui->renderLayout);

    QIcon lockedIcon;
    lockedIcon.addFile(QStringLiteral(":/Resources/page_white_link.png"), QSize(), QIcon::Normal,
                       QIcon::Off);

    int idx = textureTabs->indexOf(lockedContainer);

    if(idx >= 0)
      textureTabs->setTabIcon(idx, lockedIcon);
    else
      qCritical() << "Couldn't get tab index of new tab to set icon";

    // newPanel.DockHandler.TabPageContextMenuStrip = tabContextMenu;

    if(focus)
      show();

    m_LockedTabs[ID] = lockedContainer;

    INVOKE_MEMFN(RT_UpdateAndDisplay);
    return;
  }

  FetchBuffer *buf = m_Ctx->GetBuffer(ID);
  if(buf)
  {
    // load in BufferViewer
  }
}

void TextureViewer::texContextItem_triggered()
{
  QAction *act = qobject_cast<QAction *>(QObject::sender());

  QVariant eid = act->property("eid");
  if(eid.isValid())
  {
    m_Ctx->SetEventID(NULL, eid.toUInt());
    return;
  }

  QVariant id = act->property("id");
  if(id.isValid())
  {
    ViewTexture(id.value<ResourceId>(), false);
    return;
  }
}

void TextureViewer::showDisabled_triggered()
{
  m_ShowDisabled = !m_ShowDisabled;

  if(m_Ctx->LogLoaded())
    m_Ctx->RefreshStatus();
}

void TextureViewer::showEmpty_triggered()
{
  m_ShowEmpty = !m_ShowEmpty;

  if(m_Ctx->LogLoaded())
    m_Ctx->RefreshStatus();
}

void TextureViewer::AddResourceUsageEntry(QMenu &menu, uint32_t start, uint32_t end,
                                          ResourceUsage usage)
{
  QAction *item = NULL;

  if(start == end)
    item = new QAction(
        "EID " + QString::number(start) + ": " + "TODO" /*usage.Str(m_Core.APIProps.pipelineType)*/,
        this);
  else
    item = new QAction("EID " + QString::number(start) + "-" + QString::number(end) + ": " +
                           "TODO" /*usage.Str(m_Core.APIProps.pipelineType)*/,
                       this);

  QObject::connect(item, &QAction::triggered, this, &TextureViewer::texContextItem_triggered);
  item->setProperty("eid", QVariant(end));

  menu.addAction(item);
}

void TextureViewer::OpenResourceContextMenu(ResourceId id, const rdctype::array<EventUsage> &usage)
{
  QMenu contextMenu(this);

  QAction showDisabled(tr("Show Disabled"), this);
  QAction showEmpty(tr("Show Empty"), this);
  QAction openLockedTab(tr("Open new Locked Tab"), this);
  QAction usageTitle(tr("Used:"), this);
  QAction imageLayout(this);

  QIcon goArrow;
  goArrow.addFile(QStringLiteral(":/Resources/RightArrow_Green_16x16.png"), QSize(), QIcon::Normal,
                  QIcon::Off);
  openLockedTab.setIcon(goArrow);

  showDisabled.setChecked(m_ShowDisabled);
  showDisabled.setChecked(m_ShowEmpty);

  contextMenu.addAction(&showDisabled);
  contextMenu.addAction(&showEmpty);

  QObject::connect(&showDisabled, &QAction::triggered, this, &TextureViewer::showDisabled_triggered);
  QObject::connect(&showEmpty, &QAction::triggered, this, &TextureViewer::showEmpty_triggered);

  if(m_Ctx->CurPipelineState.SupportsBarriers())
  {
    contextMenu.addSeparator();
    imageLayout.setText(tr("Image is in layout ") + m_Ctx->CurPipelineState.GetImageLayout(id));
    contextMenu.addAction(&imageLayout);
  }

  if(id != ResourceId())
  {
    contextMenu.addSeparator();
    contextMenu.addAction(&openLockedTab);
    contextMenu.addSeparator();
    contextMenu.addAction(&usageTitle);

    openLockedTab.setProperty("id", QVariant::fromValue(id));

    QObject::connect(&openLockedTab, &QAction::triggered, this,
                     &TextureViewer::texContextItem_triggered);

    uint32_t start = 0;
    uint32_t end = 0;
    ResourceUsage us = eUsage_IndexBuffer;

    for(const EventUsage u : usage)
    {
      if(start == 0)
      {
        start = end = u.eventID;
        us = u.usage;
        continue;
      }

      const FetchDrawcall *curDraw = m_Ctx->GetDrawcall(u.eventID);

      bool distinct = false;

      // if the usage is different from the last, add a new entry,
      // or if the previous draw link is broken.
      if(u.usage != us || curDraw == NULL || curDraw->previous == 0)
      {
        distinct = true;
      }
      else
      {
        // otherwise search back through real draws, to see if the
        // last event was where we were - otherwise it's a new
        // distinct set of drawcalls and should have a separate
        // entry in the context menu
        const FetchDrawcall *prev = m_Ctx->GetDrawcall(curDraw->previous);

        while(prev != NULL && prev->eventID > end)
        {
          if((prev->flags & (eDraw_Dispatch | eDraw_Drawcall | eDraw_CmdList)) == 0)
          {
            prev = m_Ctx->GetDrawcall(prev->previous);
          }
          else
          {
            distinct = true;
            break;
          }

          if(prev == NULL)
            distinct = true;
        }
      }

      if(distinct)
      {
        AddResourceUsageEntry(contextMenu, start, end, us);
        start = end = u.eventID;
        us = u.usage;
      }

      end = u.eventID;
    }

    if(start != 0)
      AddResourceUsageEntry(contextMenu, start, end, us);

    RDDialog::show(&contextMenu, QCursor::pos());
  }
  else
  {
    RDDialog::show(&contextMenu, QCursor::pos());
  }
}

void TextureViewer::InitResourcePreview(ResourcePreview *prev, ResourceId id,
                                        FormatComponentType typeHint, bool force, Following &follow,
                                        const QString &bindName, const QString &slotName)
{
  if(id != ResourceId() || force)
  {
    FetchTexture *texptr = m_Ctx->GetTexture(id);
    FetchBuffer *bufptr = m_Ctx->GetBuffer(id);

    if(texptr != NULL)
    {
      QString fullname = bindName;
      if(texptr->customName)
      {
        if(!fullname.isEmpty())
          fullname += " = ";
        fullname += texptr->name;
      }
      if(fullname.isEmpty())
        fullname = texptr->name;

      prev->setResourceName(fullname);
      WId handle = prev->thumbWinId();
      m_Ctx->Renderer()->AsyncInvoke([this, handle, id, typeHint](IReplayRenderer *) {
        m_Output->AddThumbnail(m_Ctx->m_CurWinSystem, m_Ctx->FillWindowingData(handle), id, typeHint);
      });
    }
    else if(bufptr != NULL)
    {
      QString fullname = bindName;
      if(bufptr->customName)
      {
        if(!fullname.isEmpty())
          fullname += " = ";
        fullname += bufptr->name;
      }
      if(fullname.isEmpty())
        fullname = bufptr->name;

      prev->setResourceName(fullname);
      WId handle = prev->thumbWinId();
      m_Ctx->Renderer()->AsyncInvoke([this, handle](IReplayRenderer *) {
        m_Output->AddThumbnail(m_Ctx->m_CurWinSystem, m_Ctx->FillWindowingData(handle),
                               ResourceId(), eCompType_None);
      });
    }
    else
    {
      prev->setResourceName("");
      WId handle = prev->thumbWinId();
      m_Ctx->Renderer()->AsyncInvoke([this, handle](IReplayRenderer *) {
        m_Output->AddThumbnail(m_Ctx->m_CurWinSystem, m_Ctx->FillWindowingData(handle),
                               ResourceId(), eCompType_None);
      });
    }

    prev->setProperty("f", QVariant::fromValue(follow));
    prev->setSlotName(slotName);
    prev->setActive(true);
    prev->setSelected(m_Following == follow);
  }
  else if(m_Following == follow)
  {
    prev->setResourceName(tr("Unused"));
    prev->setActive(true);
    prev->setSelected(true);

    WId handle = prev->thumbWinId();
    m_Ctx->Renderer()->AsyncInvoke([this, handle](IReplayRenderer *) {
      m_Output->AddThumbnail(m_Ctx->m_CurWinSystem, m_Ctx->FillWindowingData(handle), ResourceId(),
                             eCompType_None);
    });
  }
  else
  {
    prev->setResourceName("");
    prev->setActive(false);
    prev->setSelected(false);
  }
}

void TextureViewer::InitStageResourcePreviews(ShaderStageType stage,
                                              const rdctype::array<ShaderResource> &resourceDetails,
                                              const rdctype::array<BindpointMap> &mapping,
                                              QMap<BindpointMap, QVector<BoundResource>> &ResList,
                                              ThumbnailStrip *prevs, int &prevIndex, bool copy,
                                              bool rw)
{
  for(int idx = 0; idx < mapping.count; idx++)
  {
    const BindpointMap &key = mapping[idx];

    const QVector<BoundResource> *resArray = NULL;

    if(ResList.contains(key))
      resArray = &ResList[key];

    int arrayLen = resArray != NULL ? resArray->size() : 1;

    for(int arrayIdx = 0; arrayIdx < arrayLen; arrayIdx++)
    {
      ResourceId id = resArray != NULL ? resArray->at(arrayIdx).Id : ResourceId();
      FormatComponentType typeHint =
          resArray != NULL ? resArray->at(arrayIdx).typeHint : eCompType_None;

      bool used = key.used;
      bool samplerBind = false;
      bool otherBind = false;

      QString bindName;

      for(int b = 0; b < resourceDetails.count; b++)
      {
        const ShaderResource &bind = resourceDetails[b];
        if(bind.bindPoint == idx && bind.IsSRV)
        {
          bindName = bind.name;
          otherBind = true;
          break;
        }

        if(bind.bindPoint == idx)
        {
          if(bind.IsSampler && !bind.IsSRV)
            samplerBind = true;
          else
            otherBind = true;
        }
      }

      if(samplerBind && !otherBind)
        continue;

      if(copy)
      {
        used = true;
        bindName = "Source";
      }

      Following follow(rw ? FollowType::ReadWrite : FollowType::ReadOnly, stage, idx, arrayIdx);
      QString slotName =
          QString("%1 %2%3").arg(m_Ctx->CurPipelineState.Abbrev(stage)).arg(rw ? "RW " : "").arg(idx);

      if(arrayLen > 1)
        slotName += QString("[%1]").arg(arrayIdx);

      if(copy)
        slotName = "SRC";

      // show if it's referenced by the shader - regardless of empty or not
      bool show = used;

      // it's bound, but not referenced, and we have "show disabled"
      show = show || (m_ShowDisabled && !used && id != ResourceId());

      // it's empty, and we have "show empty"
      show = show || (m_ShowEmpty && id == ResourceId());

      // it's the one we're following
      show = show || (follow == m_Following);

      ResourcePreview *prev = NULL;

      if(prevIndex < prevs->thumbs().size())
      {
        prev = prevs->thumbs()[prevIndex];

        // don't use it if we're not actually going to show it
        if(!show && !prev->isActive())
          continue;
      }
      else
      {
        // don't create it if we're not actually going to show it
        if(!show)
          continue;

        prev = UI_CreateThumbnail(prevs);
      }

      prevIndex++;

      InitResourcePreview(prev, show ? id : ResourceId(), typeHint, show, follow, bindName, slotName);
    }
  }
}

void TextureViewer::thumb_doubleClicked(QMouseEvent *e)
{
  if(e->buttons() & Qt::LeftButton)
  {
    ResourceId id = m_Following.GetResourceId(m_Ctx);

    if(id != ResourceId())
      ViewTexture(id, false);
  }
}

void TextureViewer::thumb_clicked(QMouseEvent *e)
{
  if(e->buttons() & Qt::LeftButton)
  {
    ResourcePreview *prev = qobject_cast<ResourcePreview *>(QObject::sender());

    Following follow = prev->property("f").value<Following>();

    for(ResourcePreview *p : ui->outputThumbs->thumbs())
      p->setSelected(false);

    for(ResourcePreview *p : ui->inputThumbs->thumbs())
      p->setSelected(false);

    m_Following = follow;
    prev->setSelected(true);

    UI_UpdateCachedTexture();

    ResourceId id = m_Following.GetResourceId(m_Ctx);

    if(id != ResourceId())
    {
      UI_OnTextureSelectionChanged(false);
      ui->renderContainer->show();
    }
  }

  if(e->buttons() & Qt::RightButton)
  {
    ResourcePreview *prev = qobject_cast<ResourcePreview *>(QObject::sender());

    Following follow = prev->property("f").value<Following>();

    ResourceId id = follow.GetResourceId(m_Ctx);

    if(id == ResourceId() && follow == m_Following)
      id = m_TexDisplay.texid;

    rdctype::array<EventUsage> empty;

    if(id == ResourceId())
    {
      OpenResourceContextMenu(id, empty);
    }
    else
    {
      m_Ctx->Renderer()->AsyncInvoke([this, id](IReplayRenderer *r) {
        rdctype::array<EventUsage> usage;

        r->GetUsage(id, &usage);

        GUIInvoke::call([this, id, usage]() { OpenResourceContextMenu(id, usage); });
      });
    }
  }
}

void TextureViewer::render_mouseWheel(QWheelEvent *e)
{
  QPoint cursorPos = e->pos();

  setFitToWindow(false);

  // scroll in logarithmic scale
  double logScale = logf(m_TexDisplay.scale);
  logScale += e->delta() / 2500.0;
  UI_SetScale((float)expf(logScale), cursorPos.x(), cursorPos.y());

  e->accept();
}

void TextureViewer::render_mouseMove(QMouseEvent *e)
{
  if(m_Output == NULL)
    return;

  m_CurHoverPixel.setX(int(((float)e->x() - m_TexDisplay.offx) / m_TexDisplay.scale));
  m_CurHoverPixel.setY(int(((float)e->y() - m_TexDisplay.offy) / m_TexDisplay.scale));

  if(m_TexDisplay.texid != ResourceId())
  {
    FetchTexture *texptr = GetCurrentTexture();

    if(texptr != NULL)
    {
      if(e->buttons() & Qt::RightButton)
      {
        ui->render->setCursor(QCursor(Qt::CrossCursor));

        m_PickedPoint = m_CurHoverPixel;

        m_PickedPoint.setX(qBound(0, m_PickedPoint.x(), (int)texptr->width - 1));
        m_PickedPoint.setY(qBound(0, m_PickedPoint.y(), (int)texptr->height - 1));

        INVOKE_MEMFN(RT_PickPixelsAndUpdate);
      }
      else if(e->buttons() == Qt::NoButton)
      {
        INVOKE_MEMFN(RT_PickHoverAndUpdate);
      }
    }
  }

  QPoint curpos = QCursor::pos();

  if(e->buttons() & Qt::LeftButton)
  {
    if(qAbs(m_DragStartPos.x() - curpos.x()) > ui->renderHScroll->singleStep() ||
       qAbs(m_DragStartPos.y() - curpos.y()) > ui->renderVScroll->singleStep())
    {
      setScrollPosition(QPoint(m_DragStartScroll.x() + (curpos.x() - m_DragStartPos.x()),
                               m_DragStartScroll.y() + (curpos.y() - m_DragStartPos.y())));
    }

    ui->render->setCursor(QCursor(Qt::SizeAllCursor));
  }

  if(e->buttons() == Qt::NoButton)
  {
    ui->render->unsetCursor();
  }

  UI_UpdateStatusText();
}

void TextureViewer::render_mouseClick(QMouseEvent *e)
{
  ui->render->setFocus();

  if(e->buttons() & Qt::RightButton)
    render_mouseMove(e);

  if(e->buttons() & Qt::LeftButton)
  {
    m_DragStartPos = QCursor::pos();
    m_DragStartScroll = getScrollPosition();

    ui->render->setCursor(QCursor(Qt::SizeAllCursor));
  }
}

void TextureViewer::render_resize(QResizeEvent *e)
{
  UI_UpdateFittedScale();
  UI_CalcScrollbars();

  INVOKE_MEMFN(RT_UpdateAndDisplay);
}

void TextureViewer::render_keyPress(QKeyEvent *e)
{
  FetchTexture *texptr = GetCurrentTexture();

  if(texptr == NULL)
    return;

  if((e->modifiers() & Qt::ControlModifier) && e->key() == Qt::Key_C)
  {
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(ui->texStatusDim->text() + " | " + ui->statusText->text());
  }

  if(!m_Ctx->LogLoaded())
    return;

  if((e->modifiers() & Qt::ControlModifier) && e->key() == Qt::Key_G)
  {
    ShowGotoPopup();
  }

  bool nudged = false;

  int increment = 1 << (int)m_TexDisplay.mip;

  if(e->key() == Qt::Key_Up && m_PickedPoint.y() > 0)
  {
    m_PickedPoint -= QPoint(0, increment);
    nudged = true;
  }
  else if(e->key() == Qt::Key_Down && m_PickedPoint.y() < (int)texptr->height - 1)
  {
    m_PickedPoint += QPoint(0, increment);
    nudged = true;
  }
  else if(e->key() == Qt::Key_Left && m_PickedPoint.x() > 0)
  {
    m_PickedPoint -= QPoint(increment, 0);
    nudged = true;
  }
  else if(e->key() == Qt::Key_Right && m_PickedPoint.x() < (int)texptr->height - 1)
  {
    m_PickedPoint += QPoint(increment, 0);
    nudged = true;
  }

  if(nudged)
  {
    m_PickedPoint = QPoint(qBound(0, m_PickedPoint.x(), (int)texptr->width - 1),
                           qBound(0, m_PickedPoint.y(), (int)texptr->height - 1));
    e->accept();

    m_Ctx->Renderer()->AsyncInvoke([this](IReplayRenderer *r) {
      RT_PickPixelsAndUpdate(r);
      RT_UpdateAndDisplay(r);
    });

    UI_UpdateStatusText();
  }
}

float TextureViewer::CurMaxScrollX()
{
  FetchTexture *texptr = GetCurrentTexture();

  QSizeF size(1.0f, 1.0f);

  if(texptr != NULL)
    size = QSizeF(texptr->width, texptr->height);

  return ui->render->width() - size.width() * m_TexDisplay.scale;
}

float TextureViewer::CurMaxScrollY()
{
  FetchTexture *texptr = GetCurrentTexture();

  QSizeF size(1.0f, 1.0f);

  if(texptr != NULL)
    size = QSizeF(texptr->width, texptr->height);

  return ui->render->height() - size.height() * m_TexDisplay.scale;
}

QPoint TextureViewer::getScrollPosition()
{
  return QPoint((int)m_TexDisplay.offx, m_TexDisplay.offy);
}

void TextureViewer::setScrollPosition(const QPoint &pos)
{
  m_TexDisplay.offx = qMax(CurMaxScrollX(), (float)pos.x());
  m_TexDisplay.offy = qMax(CurMaxScrollY(), (float)pos.y());

  m_TexDisplay.offx = qMin(0.0f, m_TexDisplay.offx);
  m_TexDisplay.offy = qMin(0.0f, m_TexDisplay.offy);

  if(ScrollUpdateScrollbars)
  {
    ScrollUpdateScrollbars = false;

    if(ui->renderHScroll->isEnabled())
      ui->renderHScroll->setValue(qBound(0, -int(m_TexDisplay.offx), ui->renderHScroll->maximum()));

    if(ui->renderVScroll->isEnabled())
      ui->renderVScroll->setValue(qBound(0, -int(m_TexDisplay.offy), ui->renderVScroll->maximum()));

    ScrollUpdateScrollbars = true;
  }

  INVOKE_MEMFN(RT_UpdateAndDisplay);
}

void TextureViewer::UI_CalcScrollbars()
{
  FetchTexture *texptr = GetCurrentTexture();

  QSizeF size(1.0f, 1.0f);

  if(texptr != NULL)
  {
    size = QSizeF(texptr->width, texptr->height);
  }

  if((int)floor(size.width() * m_TexDisplay.scale) <= ui->render->width())
  {
    ui->renderHScroll->setEnabled(false);
  }
  else
  {
    ui->renderHScroll->setEnabled(true);

    ui->renderHScroll->setMaximum(
        (int)ceil(size.width() * m_TexDisplay.scale - (float)ui->render->width()));
    ui->renderHScroll->setPageStep(qMax(1, ui->renderHScroll->maximum() / 6));
    ui->renderHScroll->setSingleStep(int(m_TexDisplay.scale));
  }

  if((int)floor(size.height() * m_TexDisplay.scale) <= ui->render->height())
  {
    ui->renderVScroll->setEnabled(false);
  }
  else
  {
    ui->renderVScroll->setEnabled(true);

    ui->renderVScroll->setMaximum(
        (int)ceil(size.height() * m_TexDisplay.scale - (float)ui->render->height()));
    ui->renderVScroll->setPageStep(qMax(1, ui->renderVScroll->maximum() / 6));
    ui->renderVScroll->setSingleStep(int(m_TexDisplay.scale));
  }
}

void TextureViewer::on_renderHScroll_valueChanged(int position)
{
  if(!ScrollUpdateScrollbars)
    return;

  ScrollUpdateScrollbars = false;

  {
    float delta = (float)position / (float)ui->renderHScroll->maximum();
    setScrollPosition(QPoint((int)(CurMaxScrollX() * delta), getScrollPosition().y()));
  }

  ScrollUpdateScrollbars = true;
}

void TextureViewer::on_renderVScroll_valueChanged(int position)
{
  if(!ScrollUpdateScrollbars)
    return;

  ScrollUpdateScrollbars = false;

  {
    float delta = (float)position / (float)ui->renderVScroll->maximum();
    setScrollPosition(QPoint(getScrollPosition().x(), (int)(CurMaxScrollY() * delta)));
  }

  ScrollUpdateScrollbars = true;
}

void TextureViewer::UI_RecreatePanels()
{
  CaptureContext *ctx = m_Ctx;

  // while a log is loaded, pass NULL into the widget
  if(!ctx->LogLoaded())
    ctx = NULL;

  {
    CustomPaintWidget *render = new CustomPaintWidget(ctx, ui->renderContainer);
    render->setObjectName(ui->render->objectName());
    render->setSizePolicy(ui->render->sizePolicy());
    delete ui->render;
    ui->render = render;
    ui->gridLayout->addWidget(render, 1, 0, 1, 1);
  }

  {
    CustomPaintWidget *pixelContext = new CustomPaintWidget(ctx, ui->pixelContextLayout);
    pixelContext->setObjectName(ui->pixelContext->objectName());
    pixelContext->setSizePolicy(ui->pixelContext->sizePolicy());
    delete ui->pixelContext;
    ui->pixelContext = pixelContext;
    ui->pixelcontextgrid->addWidget(pixelContext, 0, 0, 1, 2);
  }

  ui->render->setColours(darkBack, lightBack);
  ui->pixelContext->setColours(darkBack, lightBack);

  QObject::connect(ui->render, &CustomPaintWidget::clicked, this, &TextureViewer::render_mouseClick);
  QObject::connect(ui->render, &CustomPaintWidget::mouseMove, this, &TextureViewer::render_mouseMove);
  QObject::connect(ui->render, &CustomPaintWidget::mouseWheel, this,
                   &TextureViewer::render_mouseWheel);
  QObject::connect(ui->render, &CustomPaintWidget::resize, this, &TextureViewer::render_resize);
  QObject::connect(ui->render, &CustomPaintWidget::keyPress, this, &TextureViewer::render_keyPress);

  QObject::connect(ui->pixelContext, &CustomPaintWidget::keyPress, this,
                   &TextureViewer::render_keyPress);
}

void TextureViewer::OnLogfileLoaded()
{
  Reset();

  WId renderID = ui->render->winId();
  WId contextID = ui->pixelContext->winId();

  TextureListItemModel *model = (TextureListItemModel *)ui->textureList->model();

  model->reset(TextureListItemModel::String, "", m_Ctx);

  m_TexDisplay.darkBackgroundColour =
      FloatVector(darkBack.redF(), darkBack.greenF(), darkBack.blueF(), 1.0f);
  m_TexDisplay.lightBackgroundColour =
      FloatVector(lightBack.redF(), lightBack.greenF(), lightBack.blueF(), 1.0f);

  m_Ctx->Renderer()->BlockInvoke([renderID, contextID, this](IReplayRenderer *r) {
    m_Output = r->CreateOutput(m_Ctx->m_CurWinSystem, m_Ctx->FillWindowingData(renderID),
                               eOutputType_TexDisplay);

    m_Output->SetPixelContext(m_Ctx->m_CurWinSystem, m_Ctx->FillWindowingData(contextID));

    ui->render->setOutput(m_Output);
    ui->pixelContext->setOutput(m_Output);

    OutputConfig c = {eOutputType_TexDisplay};
    m_Output->SetOutputConfig(c);

    RT_UpdateAndDisplay(r);
  });
}

void TextureViewer::Reset()
{
  m_CachedTexture = NULL;

  memset(&m_TexDisplay, 0, sizeof(m_TexDisplay));
  m_TexDisplay.darkBackgroundColour =
      FloatVector(darkBack.redF(), darkBack.greenF(), darkBack.blueF(), 1.0f);
  m_TexDisplay.lightBackgroundColour =
      FloatVector(lightBack.redF(), lightBack.greenF(), lightBack.blueF(), 1.0f);

  m_Output = NULL;

  m_TextureSettings.clear();

  m_PrevSize = QSizeF();
  m_HighWaterStatusLength = 0;

  ui->rangeHistogram->setRange(0.0f, 1.0f);

  ui->textureListFilter->setCurrentIndex(0);

  ui->renderHScroll->setEnabled(false);
  ui->renderVScroll->setEnabled(false);

  // PixelPicked = false;

  ui->statusText->setText("");
  ui->renderContainer->setWindowTitle(tr("Current"));
  ui->zoomOption->setCurrentText("");
  ui->mipLevel->clear();
  ui->sliceFace->clear();

  ui->channels->setCurrentIndex(0);
  ui->overlay->setCurrentIndex(0);

  ui->customShader->clear();

  UI_RecreatePanels();

  ui->inputThumbs->clearThumbs();
  ui->outputThumbs->clearThumbs();

  UI_UpdateTextureDetails();
  UI_UpdateChannels();
}

void TextureViewer::OnLogfileClosed()
{
  Reset();

  for(ResourceId id : m_LockedTabs.keys())
    delete m_LockedTabs[id];

  m_LockedTabs.clear();

  ui->saveTex->setEnabled(false);
  ui->locationGoto->setEnabled(false);
  ui->viewTexBuffer->setEnabled(false);
}

void TextureViewer::OnEventSelected(uint32_t eventID)
{
  UI_UpdateCachedTexture();

  FetchTexture *CurrentTexture = GetCurrentTexture();

  if(!currentTextureIsLocked() ||
     (CurrentTexture != NULL && m_TexDisplay.texid != CurrentTexture->ID))
    UI_OnTextureSelectionChanged(true);

  if(m_Output == NULL)
    return;

  UI_CreateThumbnails();

  QVector<BoundResource> RTs = Following::GetOutputTargets(m_Ctx);
  BoundResource Depth = Following::GetDepthTarget(m_Ctx);

  int outIndex = 0;
  int inIndex = 0;

  bool copy = false, compute = false;
  Following::GetDrawContext(m_Ctx, copy, compute);

  for(int rt = 0; rt < RTs.size(); rt++)
  {
    ResourcePreview *prev;

    if(outIndex < ui->outputThumbs->thumbs().size())
      prev = ui->outputThumbs->thumbs()[outIndex];
    else
      prev = UI_CreateThumbnail(ui->outputThumbs);

    outIndex++;

    Following follow(FollowType::OutputColour, eShaderStage_Pixel, rt, 0);
    QString bindName = copy ? tr("Destination") : "";
    QString slotName =
        copy ? tr("DST") : (m_Ctx->CurPipelineState.OutputAbbrev() + QString::number(rt));

    InitResourcePreview(prev, RTs[rt].Id, RTs[rt].typeHint, false, follow, bindName, slotName);
  }

  // depth
  {
    ResourcePreview *prev;

    if(outIndex < ui->outputThumbs->thumbs().size())
      prev = ui->outputThumbs->thumbs()[outIndex];
    else
      prev = UI_CreateThumbnail(ui->outputThumbs);

    outIndex++;

    Following follow(FollowType::OutputDepth, eShaderStage_Pixel, 0, 0);

    InitResourcePreview(prev, Depth.Id, Depth.typeHint, false, follow, "", tr("DS"));
  }

  ShaderStageType stages[] = {eShaderStage_Vertex, eShaderStage_Hull, eShaderStage_Domain,
                              eShaderStage_Geometry, eShaderStage_Pixel};

  int count = 5;

  if(compute)
  {
    stages[0] = eShaderStage_Compute;
    count = 1;
  }

  const rdctype::array<ShaderResource> empty;

  // display resources used for all stages
  for(int i = 0; i < count; i++)
  {
    ShaderStageType stage = stages[i];

    QMap<BindpointMap, QVector<BoundResource>> RWs = Following::GetReadWriteResources(m_Ctx, stage);
    QMap<BindpointMap, QVector<BoundResource>> ROs = Following::GetReadOnlyResources(m_Ctx, stage);

    ShaderReflection *details = Following::GetReflection(m_Ctx, stage);
    ShaderBindpointMapping mapping = Following::GetMapping(m_Ctx, stage);

    InitStageResourcePreviews(stage, details != NULL ? details->ReadWriteResources : empty,
                              mapping.ReadWriteResources, RWs, ui->outputThumbs, outIndex, copy,
                              true);

    InitStageResourcePreviews(stage, details != NULL ? details->ReadOnlyResources : empty,
                              mapping.ReadOnlyResources, ROs, ui->inputThumbs, inIndex, copy, false);
  }

  // hide others
  const QVector<ResourcePreview *> &outThumbs = ui->outputThumbs->thumbs();

  for(; outIndex < outThumbs.size(); outIndex++)
  {
    ResourcePreview *prev = outThumbs[outIndex];
    prev->setResourceName("");
    prev->setActive(false);
    prev->setSelected(false);
  }

  ui->outputThumbs->refreshLayout();

  const QVector<ResourcePreview *> &inThumbs = ui->inputThumbs->thumbs();

  for(; inIndex < inThumbs.size(); inIndex++)
  {
    ResourcePreview *prev = inThumbs[inIndex];
    prev->setResourceName("");
    prev->setActive(false);
    prev->setSelected(false);
  }

  ui->inputThumbs->refreshLayout();

  INVOKE_MEMFN(RT_UpdateAndDisplay);

  // if(autoFit.Checked)
  // AutoFitRange();
}

QVariant TextureViewer::persistData()
{
  QVariantMap state = ui->dockarea->saveState();

  state["darkBack"] = darkBack;
  state["lightBack"] = lightBack;

  return state;
}

void TextureViewer::setPersistData(const QVariant &persistData)
{
  QVariantMap state = persistData.toMap();

  darkBack = state["darkBack"].value<QColor>();
  lightBack = state["lightBack"].value<QColor>();

  if(darkBack != lightBack)
  {
    ui->backcolorPick->setChecked(false);
    ui->checkerBack->setChecked(true);
  }
  else
  {
    ui->backcolorPick->setChecked(true);
    ui->checkerBack->setChecked(false);
  }

  m_TexDisplay.darkBackgroundColour =
      FloatVector(darkBack.redF(), darkBack.greenF(), darkBack.blueF(), 1.0f);
  m_TexDisplay.lightBackgroundColour =
      FloatVector(lightBack.redF(), lightBack.greenF(), lightBack.blueF(), 1.0f);

  ui->render->setColours(darkBack, lightBack);
  ui->pixelContext->setColours(darkBack, lightBack);

  ui->dockarea->restoreState(state);

  SetupTextureTabs();
}

float TextureViewer::GetFitScale()
{
  FetchTexture *texptr = GetCurrentTexture();

  if(texptr == NULL)
    return 1.0f;

  float xscale = (float)ui->render->width() / (float)texptr->width;
  float yscale = (float)ui->render->height() / (float)texptr->height;
  return qMin(xscale, yscale);
}

void TextureViewer::UI_UpdateFittedScale()
{
  if(ui->fitToWindow->isChecked())
    UI_SetScale(1.0f);
}

void TextureViewer::UI_SetScale(float s)
{
  UI_SetScale(s, ui->render->width() / 2, ui->render->height() / 2);
}

void TextureViewer::UI_SetScale(float s, int x, int y)
{
  if(ui->fitToWindow->isChecked())
    s = GetFitScale();

  float prevScale = m_TexDisplay.scale;

  m_TexDisplay.scale = qBound(0.1f, s, 256.0f);

  INVOKE_MEMFN(RT_UpdateAndDisplay);

  float scaleDelta = (m_TexDisplay.scale / prevScale);

  QPoint newPos = getScrollPosition();

  newPos -= QPoint(x, y);
  newPos = QPoint((int)(newPos.x() * scaleDelta), (int)(newPos.y() * scaleDelta));
  newPos += QPoint(x, y);

  setScrollPosition(newPos);

  setCurrentZoomValue(m_TexDisplay.scale);

  UI_CalcScrollbars();
}

void TextureViewer::setCurrentZoomValue(float zoom)
{
  ui->zoomOption->setCurrentText(QString::number(ceil(zoom * 100)) + "%");
}

float TextureViewer::getCurrentZoomValue()
{
  if(ui->fitToWindow->isChecked())
    return m_TexDisplay.scale;

  QString zoomText = ui->zoomOption->currentText().replace('%', ' ');

  bool ok = false;
  int zoom = zoomText.toInt(&ok);

  if(!ok)
    zoom = 100;

  return (float)(zoom) / 100.0f;
}

void TextureViewer::setFitToWindow(bool checked)
{
  if(checked)
  {
    UI_UpdateFittedScale();
    ui->fitToWindow->setChecked(true);
  }
  else if(!checked)
  {
    ui->fitToWindow->setChecked(false);
    float curScale = m_TexDisplay.scale;
    ui->zoomOption->setCurrentText("");
    setCurrentZoomValue(curScale);
  }
}

void TextureViewer::on_fitToWindow_toggled(bool checked)
{
  UI_UpdateFittedScale();
}

void TextureViewer::on_zoomExactSize_clicked()
{
  ui->fitToWindow->setChecked(false);
  UI_SetScale(1.0f);
}

void TextureViewer::on_zoomOption_currentIndexChanged(int index)
{
  if(index >= 0)
  {
    setFitToWindow(false);
    ui->zoomOption->setCurrentText(ui->zoomOption->itemText(index));
    UI_SetScale(getCurrentZoomValue());
  }
}

void TextureViewer::zoomOption_returnPressed()
{
  UI_SetScale(getCurrentZoomValue());
}

void TextureViewer::on_overlay_currentIndexChanged(int index)
{
  m_TexDisplay.overlay = eTexOverlay_None;

  if(ui->overlay->currentIndex() > 0)
    m_TexDisplay.overlay = (TextureDisplayOverlay)ui->overlay->currentIndex();

  INVOKE_MEMFN(RT_UpdateAndDisplay);
}

void TextureViewer::range_rangeUpdated()
{
  m_TexDisplay.rangemin = ui->rangeHistogram->blackPoint();
  m_TexDisplay.rangemax = ui->rangeHistogram->whitePoint();

  ui->rangeBlack->setText(Formatter::Format(m_TexDisplay.rangemin));
  ui->rangeWhite->setText(Formatter::Format(m_TexDisplay.rangemax));

  if(m_NoRangePaint)
    return;

  INVOKE_MEMFN(RT_UpdateAndDisplay);

  if(m_Output == NULL)
  {
    ui->render->update();
    ui->pixelcontextgrid->update();
  }
}

void TextureViewer::rangePoint_textChanged(QString text)
{
  m_RangePoint_Dirty = true;
}

void TextureViewer::rangePoint_Update()
{
  float black = ui->rangeHistogram->blackPoint();
  float white = ui->rangeHistogram->whitePoint();

  bool ok = false;
  double d = ui->rangeBlack->text().toDouble(&ok);

  if(ok)
    black = d;

  d = ui->rangeWhite->text().toDouble(&ok);

  if(ok)
    white = d;

  ui->rangeHistogram->setRange(black, white);

  INVOKE_MEMFN(RT_UpdateVisualRange);
}

void TextureViewer::rangePoint_leave()
{
  if(!m_RangePoint_Dirty)
    return;

  rangePoint_Update();

  m_RangePoint_Dirty = false;
}

void TextureViewer::rangePoint_keyPress(QKeyEvent *e)
{
  // escape key
  if(e->key() == Qt::Key_Escape)
  {
    m_RangePoint_Dirty = false;
    ui->rangeHistogram->setRange(ui->rangeHistogram->blackPoint(), ui->rangeHistogram->whitePoint());
  }
  if(e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter)
  {
    rangePoint_Update();
  }
}

void TextureViewer::on_zoomRange_clicked()
{
  float black = ui->rangeHistogram->blackPoint();
  float white = ui->rangeHistogram->whitePoint();

  ui->autoFit->setChecked(false);

  ui->rangeHistogram->setRange(black, white);

  INVOKE_MEMFN(RT_UpdateVisualRange);
}

void TextureViewer::on_autoFit_clicked()
{
  AutoFitRange();
}

void TextureViewer::on_reset01_clicked()
{
  UI_SetHistogramRange(GetCurrentTexture(), m_TexDisplay.typeHint);

  ui->autoFit->setChecked(false);

  INVOKE_MEMFN(RT_UpdateVisualRange);
}

void TextureViewer::on_visualiseRange_clicked()
{
  if(ui->visualiseRange->isChecked())
  {
    ui->rangeHistogram->setMinimumSize(QSize(300, 90));

    m_Visualise = true;
    INVOKE_MEMFN(RT_UpdateVisualRange);
  }
  else
  {
    m_Visualise = false;
    ui->rangeHistogram->setMinimumSize(QSize(200, 0));

    ui->rangeHistogram->setHistogramData({});
  }
}

void TextureViewer::AutoFitRange()
{
  // no log loaded or buffer/empty texture currently being viewed - don't autofit
  if(!m_Ctx->LogLoaded() || GetCurrentTexture() == NULL || m_Output == NULL)
    return;

  m_Ctx->Renderer()->AsyncInvoke([this](IReplayRenderer *r) {
    PixelValue min, max;
    bool success = m_Output->GetMinMax(&min, &max);

    if(success)
    {
      float minval = FLT_MAX;
      float maxval = -FLT_MAX;

      bool changeRange = false;

      ResourceFormat fmt = GetCurrentTexture()->format;

      if(m_TexDisplay.CustomShader != ResourceId())
      {
        fmt.compType = eCompType_Float;
      }

      for(int i = 0; i < 4; i++)
      {
        if(fmt.compType == eCompType_UInt)
        {
          min.value_f[i] = min.value_u[i];
          max.value_f[i] = max.value_u[i];
        }
        else if(fmt.compType == eCompType_SInt)
        {
          min.value_f[i] = min.value_i[i];
          max.value_f[i] = max.value_i[i];
        }
      }

      if(m_TexDisplay.Red)
      {
        minval = qMin(minval, min.value_f[0]);
        maxval = qMax(maxval, max.value_f[0]);
        changeRange = true;
      }
      if(m_TexDisplay.Green && fmt.compCount > 1)
      {
        minval = qMin(minval, min.value_f[1]);
        maxval = qMax(maxval, max.value_f[1]);
        changeRange = true;
      }
      if(m_TexDisplay.Blue && fmt.compCount > 2)
      {
        minval = qMin(minval, min.value_f[2]);
        maxval = qMax(maxval, max.value_f[2]);
        changeRange = true;
      }
      if(m_TexDisplay.Alpha && fmt.compCount > 3)
      {
        minval = qMin(minval, min.value_f[3]);
        maxval = qMax(maxval, max.value_f[3]);
        changeRange = true;
      }

      if(changeRange)
      {
        GUIInvoke::call([this, minval, maxval]() {
          ui->rangeHistogram->setRange(minval, maxval);
          INVOKE_MEMFN(RT_UpdateVisualRange);
        });
      }
    }
  });
}

void TextureViewer::on_backcolorPick_clicked()
{
  QColor col = QColorDialog::getColor(Qt::black, this, tr("Choose background colour"));

  if(!col.isValid())
    col = QColor(0, 0, 0);

  col = col.toRgb();
  m_TexDisplay.darkBackgroundColour = m_TexDisplay.lightBackgroundColour =
      FloatVector(col.redF(), col.greenF(), col.blueF(), 1.0f);

  darkBack = lightBack = col;

  ui->render->setColours(darkBack, lightBack);
  ui->pixelContext->setColours(darkBack, lightBack);

  ui->backcolorPick->setChecked(true);
  ui->checkerBack->setChecked(false);

  INVOKE_MEMFN(RT_UpdateAndDisplay);

  if(m_Output == NULL)
  {
    ui->render->update();
    ui->pixelcontextgrid->update();
  }
}

void TextureViewer::on_checkerBack_clicked()
{
  ui->checkerBack->setChecked(true);
  ui->backcolorPick->setChecked(false);

  m_TexDisplay.lightBackgroundColour = FloatVector(0.81f, 0.81f, 0.81f, 1.0f);
  m_TexDisplay.darkBackgroundColour = FloatVector(0.57f, 0.57f, 0.57f, 1.0f);

  darkBack = QColor::fromRgb(int(m_TexDisplay.darkBackgroundColour.x * 255.0f),
                             int(m_TexDisplay.darkBackgroundColour.y * 255.0f),
                             int(m_TexDisplay.darkBackgroundColour.z * 255.0f));

  lightBack = QColor::fromRgb(int(m_TexDisplay.lightBackgroundColour.x * 255.0f),
                              int(m_TexDisplay.lightBackgroundColour.y * 255.0f),
                              int(m_TexDisplay.lightBackgroundColour.z * 255.0f));

  ui->render->setColours(darkBack, lightBack);
  ui->pixelContext->setColours(darkBack, lightBack);

  INVOKE_MEMFN(RT_UpdateAndDisplay);

  if(m_Output == NULL)
  {
    ui->render->update();
    ui->pixelcontextgrid->update();
  }
}

void TextureViewer::on_mipLevel_currentIndexChanged(int index)
{
  FetchTexture *texptr = GetCurrentTexture();
  if(texptr == NULL)
    return;

  FetchTexture &tex = *texptr;

  uint32_t prevSlice = m_TexDisplay.sliceFace;

  if(tex.mips > 1)
  {
    m_TexDisplay.mip = (uint32_t)index;
    m_TexDisplay.sampleIdx = 0;
  }
  else
  {
    m_TexDisplay.mip = 0;
    m_TexDisplay.sampleIdx = (uint32_t)index;
    if(m_TexDisplay.sampleIdx == tex.msSamp)
      m_TexDisplay.sampleIdx = ~0U;
  }

  // For 3D textures, update the slice list for this mip
  if(tex.depth > 1)
  {
    uint32_t newSlice = prevSlice >> (int)m_TexDisplay.mip;

    uint32_t numSlices = qMax(1U, tex.depth >> (int)m_TexDisplay.mip);

    ui->sliceFace->clear();

    for(uint32_t i = 0; i < numSlices; i++)
      ui->sliceFace->addItem(tr("Slice ") + i);

    // changing sliceFace index will handle updating range & re-picking
    ui->sliceFace->setCurrentIndex((int)qBound(0U, newSlice, numSlices - 1));

    return;
  }

  INVOKE_MEMFN(RT_UpdateVisualRange);

  if(m_Output != NULL && m_PickedPoint.x() >= 0 && m_PickedPoint.y() >= 0)
  {
    INVOKE_MEMFN(RT_PickPixelsAndUpdate);
  }

  INVOKE_MEMFN(RT_UpdateAndDisplay);
}

void TextureViewer::on_sliceFace_currentIndexChanged(int index)
{
  FetchTexture *texptr = GetCurrentTexture();
  if(texptr == NULL)
    return;

  FetchTexture &tex = *texptr;
  m_TexDisplay.sliceFace = (uint32_t)index;

  if(tex.depth > 1)
    m_TexDisplay.sliceFace = (uint32_t)(index << (int)m_TexDisplay.mip);

  INVOKE_MEMFN(RT_UpdateVisualRange);

  if(m_Output != NULL && m_PickedPoint.x() >= 0 && m_PickedPoint.y() >= 0)
  {
    INVOKE_MEMFN(RT_PickPixelsAndUpdate);
  }

  INVOKE_MEMFN(RT_UpdateAndDisplay);
}

void TextureViewer::on_locationGoto_clicked()
{
  ShowGotoPopup();
}

void TextureViewer::ShowGotoPopup()
{
  FetchTexture *texptr = GetCurrentTexture();

  if(texptr)
  {
    QPoint p = m_PickedPoint;

    uint32_t mipHeight = qMax(1U, texptr->height >> (int)m_TexDisplay.mip);

    if(m_Ctx->APIProps().pipelineType == eGraphicsAPI_OpenGL)
      p.setY((int)(mipHeight - 1) - p.y());
    if(m_TexDisplay.FlipY)
      p.setY((int)(mipHeight - 1) - p.y());

    m_Goto->show(ui->render, p);
  }
}

void TextureViewer::on_viewTexBuffer_clicked()
{
}

void TextureViewer::on_saveTex_clicked()
{
  FetchTexture *texptr = GetCurrentTexture();

  if(!texptr || !m_Output)
    return;

  TextureSave config;
  memset(&config, 0, sizeof(config));

  config.id = m_TexDisplay.texid;
  config.typeHint = m_TexDisplay.typeHint;
  config.slice.sliceIndex = (int)m_TexDisplay.sliceFace;
  config.mip = (int)m_TexDisplay.mip;

  if(texptr->depth > 1)
    config.slice.sliceIndex = (int)m_TexDisplay.sliceFace >> (int)m_TexDisplay.mip;

  config.channelExtract = -1;
  if(m_TexDisplay.Red && !m_TexDisplay.Green && !m_TexDisplay.Blue && !m_TexDisplay.Alpha)
    config.channelExtract = 0;
  if(!m_TexDisplay.Red && m_TexDisplay.Green && !m_TexDisplay.Blue && !m_TexDisplay.Alpha)
    config.channelExtract = 1;
  if(!m_TexDisplay.Red && !m_TexDisplay.Green && m_TexDisplay.Blue && !m_TexDisplay.Alpha)
    config.channelExtract = 2;
  if(!m_TexDisplay.Red && !m_TexDisplay.Green && !m_TexDisplay.Blue && m_TexDisplay.Alpha)
    config.channelExtract = 3;

  config.comp.blackPoint = m_TexDisplay.rangemin;
  config.comp.whitePoint = m_TexDisplay.rangemax;
  config.alphaCol = m_TexDisplay.lightBackgroundColour;
  config.alpha = m_TexDisplay.Alpha ? eAlphaMap_BlendToCheckerboard : eAlphaMap_Discard;
  if(m_TexDisplay.Alpha && !ui->checkerBack->isChecked())
    config.alpha = eAlphaMap_BlendToColour;

  if(m_TexDisplay.CustomShader != ResourceId())
  {
    ResourceId id;
    m_Ctx->Renderer()->BlockInvoke(
        [this, &id](IReplayRenderer *r) { id = m_Output->GetCustomShaderTexID(); });

    if(id != ResourceId())
      config.id = id;
  }

  TextureSaveDialog saveDialog(*texptr, config, this);
  int res = RDDialog::show(&saveDialog);

  config = saveDialog.config();

  if(res)
  {
    bool ret = false;
    QString fn = saveDialog.filename();

    m_Ctx->Renderer()->BlockInvoke([this, &ret, config, fn](IReplayRenderer *r) {
      ret = r->SaveTexture(config, fn.toUtf8().data());
    });

    if(!ret)
    {
      RDDialog::critical(
          NULL, tr("Error saving texture"),
          tr("Error saving texture %1.\n\nCheck diagnostic log in Help menu for more details.").arg(fn));
    }
  }
}

void TextureViewer::on_texListShow_clicked()
{
  if(ui->textureListFrame->isVisible())
  {
    ui->dockarea->moveToolWindow(ui->textureListFrame, ToolWindowManager::NoArea);
  }
  else
  {
    ui->textureListFilter->setCurrentText("");
    ui->dockarea->moveToolWindow(
        ui->textureListFrame,
        ToolWindowManager::AreaReference(ToolWindowManager::LeftOf,
                                         ui->dockarea->areaOf(ui->renderContainer), 0.2f));
  }
}

void TextureViewer::on_cancelTextureListFilter_clicked()
{
  ui->textureListFilter->setCurrentText("");
}

void TextureViewer::on_textureListFilter_editTextChanged(const QString &text)
{
  TextureListItemModel *model = (TextureListItemModel *)ui->textureList->model();

  if(model == NULL)
    return;

  model->reset(TextureListItemModel::String, text, m_Ctx);
}

void TextureViewer::on_textureListFilter_currentIndexChanged(int index)
{
  TextureListItemModel *model = (TextureListItemModel *)ui->textureList->model();

  if(model == NULL)
    return;

  if(ui->textureListFilter->currentIndex() == 1)
    model->reset(TextureListItemModel::Textures, "", m_Ctx);
  else if(ui->textureListFilter->currentIndex() == 2)
    model->reset(TextureListItemModel::RenderTargets, "", m_Ctx);
  else
    model->reset(TextureListItemModel::String, ui->textureListFilter->currentText(), m_Ctx);
}

void TextureViewer::on_textureList_clicked(const QModelIndex &index)
{
  ResourceId id = index.model()->data(index, Qt::UserRole).value<ResourceId>();
  ViewTexture(id, false);
}
