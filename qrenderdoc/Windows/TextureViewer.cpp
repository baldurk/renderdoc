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

#include "TextureViewer.h"
#include <float.h>
#include <math.h>
#include <QClipboard>
#include <QColorDialog>
#include <QFileSystemWatcher>
#include <QFontDatabase>
#include <QItemDelegate>
#include <QJsonDocument>
#include <QMenu>
#include <QPainter>
#include <QPointer>
#include <QStyledItemDelegate>
#include "3rdparty/flowlayout/FlowLayout.h"
#include "3rdparty/toolwindowmanager/ToolWindowManagerArea.h"
#include "Code/QRDUtils.h"
#include "Code/Resources.h"
#include "Dialogs/TextureSaveDialog.h"
#include "Widgets/ResourcePreview.h"
#include "Widgets/TextureGoto.h"
#include "ui_TextureViewer.h"

float area(const QSizeF &s)
{
  return s.width() * s.height();
}

float aspect(const QSizeF &s)
{
  return s.width() / s.height();
}

static QMap<QString, ShaderEncoding> encodingExtensions = {
    {lit("hlsl"), ShaderEncoding::HLSL},
    {lit("glsl"), ShaderEncoding::GLSL},
    {lit("frag"), ShaderEncoding::GLSL},
    {lit("spvasm"), ShaderEncoding::SPIRVAsm},
};

Q_DECLARE_METATYPE(Following);

const Following Following::Default = Following();

Following::Following(FollowType t, ShaderStage s, int i, int a)
{
  Type = t;
  Stage = s;
  index = i;
  arrayEl = a;
}

Following::Following()
{
  Type = FollowType::OutputColour;
  Stage = ShaderStage::Pixel;
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

void Following::GetDrawContext(ICaptureContext &ctx, bool &copy, bool &clear, bool &compute)
{
  const DrawcallDescription *curDraw = ctx.CurDrawcall();
  copy = curDraw != NULL &&
         (curDraw->flags & (DrawFlags::Copy | DrawFlags::Resolve | DrawFlags::Present));
  clear = curDraw != NULL && (curDraw->flags & DrawFlags::Clear);
  compute = curDraw != NULL && (curDraw->flags & DrawFlags::Dispatch) &&
            ctx.CurPipelineState().GetShader(ShaderStage::Compute) != ResourceId();
}

int Following::GetHighestMip(ICaptureContext &ctx)
{
  return GetBoundResource(ctx, arrayEl).firstMip;
}

int Following::GetFirstArraySlice(ICaptureContext &ctx)
{
  return GetBoundResource(ctx, arrayEl).firstSlice;
}

CompType Following::GetTypeHint(ICaptureContext &ctx)
{
  return GetBoundResource(ctx, arrayEl).typeHint;
}

ResourceId Following::GetResourceId(ICaptureContext &ctx)
{
  return GetBoundResource(ctx, arrayEl).resourceId;
}

BoundResource Following::GetBoundResource(ICaptureContext &ctx, int arrayIdx)
{
  BoundResource ret;

  if(Type == FollowType::OutputColour)
  {
    rdcarray<BoundResource> outputs = GetOutputTargets(ctx);

    if(index < outputs.count())
      ret = outputs[index];
  }
  else if(Type == FollowType::OutputDepth)
  {
    ret = GetDepthTarget(ctx);
  }
  else if(Type == FollowType::ReadWrite)
  {
    rdcarray<BoundResourceArray> rw = GetReadWriteResources(ctx);

    ShaderBindpointMapping mapping = GetMapping(ctx);

    if(index < mapping.readWriteResources.count())
    {
      Bindpoint &key = mapping.readWriteResources[index];

      int residx = rw.indexOf(key);
      if(residx >= 0)
        ret = rw[residx].resources[arrayIdx];
    }
  }
  else if(Type == FollowType::ReadOnly)
  {
    rdcarray<BoundResourceArray> ro = GetReadOnlyResources(ctx);

    ShaderBindpointMapping mapping = GetMapping(ctx);

    if(index < mapping.readOnlyResources.count())
    {
      Bindpoint &key = mapping.readOnlyResources[index];

      int residx = ro.indexOf(key);
      if(residx >= 0)
        ret = ro[residx].resources[arrayIdx];
    }
  }

  return ret;
}

rdcarray<BoundResource> Following::GetOutputTargets(ICaptureContext &ctx)
{
  const DrawcallDescription *curDraw = ctx.CurDrawcall();
  bool copy = false, clear = false, compute = false;
  GetDrawContext(ctx, copy, clear, compute);

  if(copy || clear)
  {
    return {BoundResource(curDraw->copyDestination)};
  }
  else if(compute)
  {
    return {};
  }
  else
  {
    rdcarray<BoundResource> ret = ctx.CurPipelineState().GetOutputTargets();

    if(ret.isEmpty() && curDraw != NULL && (curDraw->flags & DrawFlags::Present))
    {
      if(curDraw->copyDestination != ResourceId())
        return {BoundResource(curDraw->copyDestination)};

      for(const TextureDescription &tex : ctx.GetTextures())
      {
        if(tex.creationFlags & TextureCategory::SwapBuffer)
          return {BoundResource(tex.resourceId)};
      }
    }

    return ret;
  }
}

BoundResource Following::GetDepthTarget(ICaptureContext &ctx)
{
  bool copy = false, clear = false, compute = false;
  GetDrawContext(ctx, copy, clear, compute);

  if(copy || clear || compute)
    return BoundResource(ResourceId());
  else
    return ctx.CurPipelineState().GetDepthTarget();
}

rdcarray<BoundResourceArray> Following::GetReadWriteResources(ICaptureContext &ctx, ShaderStage stage)
{
  bool copy = false, clear = false, compute = false;
  GetDrawContext(ctx, copy, clear, compute);

  if(copy || clear)
  {
    return rdcarray<BoundResourceArray>();
  }
  else if(compute)
  {
    // only return compute resources for one stage
    if(stage == ShaderStage::Pixel || stage == ShaderStage::Compute)
      return ctx.CurPipelineState().GetReadWriteResources(ShaderStage::Compute);
    else
      return rdcarray<BoundResourceArray>();
  }
  else
  {
    return ctx.CurPipelineState().GetReadWriteResources(stage);
  }
}

rdcarray<BoundResourceArray> Following::GetReadWriteResources(ICaptureContext &ctx)
{
  return GetReadWriteResources(ctx, Stage);
}

rdcarray<BoundResourceArray> Following::GetReadOnlyResources(ICaptureContext &ctx, ShaderStage stage)
{
  const DrawcallDescription *curDraw = ctx.CurDrawcall();
  bool copy = false, clear = false, compute = false;
  GetDrawContext(ctx, copy, clear, compute);

  if(copy || clear)
  {
    rdcarray<BoundResourceArray> ret;

    // only return copy source for one stage
    if(copy && stage == ShaderStage::Pixel)
      ret.push_back(BoundResourceArray(Bindpoint(0, 0), {BoundResource(curDraw->copySource)}));

    return ret;
  }
  else if(compute)
  {
    // only return compute resources for one stage
    if(stage == ShaderStage::Pixel || stage == ShaderStage::Compute)
      return ctx.CurPipelineState().GetReadOnlyResources(ShaderStage::Compute);
    else
      return rdcarray<BoundResourceArray>();
  }
  else
  {
    return ctx.CurPipelineState().GetReadOnlyResources(stage);
  }
}

rdcarray<BoundResourceArray> Following::GetReadOnlyResources(ICaptureContext &ctx)
{
  return GetReadOnlyResources(ctx, Stage);
}

const ShaderReflection *Following::GetReflection(ICaptureContext &ctx, ShaderStage stage)
{
  bool copy = false, clear = false, compute = false;
  GetDrawContext(ctx, copy, clear, compute);

  if(copy || clear)
    return NULL;
  else if(compute)
    return ctx.CurPipelineState().GetShaderReflection(ShaderStage::Compute);
  else
    return ctx.CurPipelineState().GetShaderReflection(stage);
}

const ShaderReflection *Following::GetReflection(ICaptureContext &ctx)
{
  return GetReflection(ctx, Stage);
}

const ShaderBindpointMapping &Following::GetMapping(ICaptureContext &ctx, ShaderStage stage)
{
  bool copy = false, clear = false, compute = false;
  GetDrawContext(ctx, copy, clear, compute);

  if(copy || clear)
  {
    static ShaderBindpointMapping mapping;

    // for PS only add a single mapping to get the copy source
    if(copy && stage == ShaderStage::Pixel)
      mapping.readOnlyResources = {Bindpoint(0, 0)};
    else
      mapping.readOnlyResources.clear();

    return mapping;
  }
  else if(compute)
  {
    return ctx.CurPipelineState().GetBindpointMapping(ShaderStage::Compute);
  }
  else
  {
    return ctx.CurPipelineState().GetBindpointMapping(stage);
  }
}

const ShaderBindpointMapping &Following::GetMapping(ICaptureContext &ctx)
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

  TextureListItemModel(ICaptureContext &ctx, QWidget *parent)
      : QAbstractItemModel(parent), m_Ctx(ctx)
  {
    goArrow.addPixmap(Pixmaps::action(parent), QIcon::Normal, QIcon::Off);
    goArrow.addPixmap(Pixmaps::action_hover(parent), QIcon::Normal, QIcon::Off);
  }
  void reset(FilterType type, const QString &filter)
  {
    const rdcarray<TextureDescription> src = m_Ctx.GetTextures();

    texs.clear();
    texs.reserve(src.count());

    emit beginResetModel();

    TextureCategory rtFlags = TextureCategory::ColorTarget | TextureCategory::DepthTarget;

    for(const TextureDescription &t : src)
    {
      if(type == Textures)
      {
        if(!(t.creationFlags & rtFlags))
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
        else if(QString(m_Ctx.GetResourceName(t.resourceId)).contains(filter, Qt::CaseInsensitive))
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
          return m_Ctx.GetResourceName(texs[index.row()].resourceId);
      }

      if(role == Qt::UserRole)
      {
        return QVariant::fromValue(texs[index.row()].resourceId);
      }

      if(role == Qt::DecorationRole)
      {
        return QVariant(goArrow);
      }
    }

    return QVariant();
  }

private:
  ICaptureContext &m_Ctx;
  QVector<TextureDescription> texs;
  QIcon goArrow;
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

TextureDescription *TextureViewer::GetCurrentTexture()
{
  return m_CachedTexture;
}

void TextureViewer::UI_UpdateCachedTexture()
{
  if(!m_Ctx.IsCaptureLoaded())
  {
    m_CachedTexture = NULL;
    return;
  }

  ResourceId id = m_LockedId;
  if(id == ResourceId())
    id = m_Following.GetResourceId(m_Ctx);

  if(id == ResourceId())
    id = m_TexDisplay.resourceId;

  m_CachedTexture = m_Ctx.GetTexture(id);

  ui->debugPixelContext->setEnabled(m_Ctx.CurPipelineState().IsCaptureD3D11() &&
                                    m_CachedTexture != NULL);
  ui->pixelHistory->setEnabled(m_Ctx.CurPipelineState().IsCaptureD3D11() && m_CachedTexture != NULL);
}

TextureViewer::TextureViewer(ICaptureContext &ctx, QWidget *parent)
    : QFrame(parent), ui(new Ui::TextureViewer), m_Ctx(ctx)
{
  ui->setupUi(this);

  ui->textureList->setFont(Formatter::PreferredFont());
  ui->textureListFilter->setFont(Formatter::PreferredFont());
  ui->rangeBlack->setFont(Formatter::PreferredFont());
  ui->rangeWhite->setFont(Formatter::PreferredFont());
  ui->hdrMul->setFont(Formatter::PreferredFont());
  ui->channels->setFont(Formatter::PreferredFont());
  ui->mipLevel->setFont(Formatter::PreferredFont());
  ui->sliceFace->setFont(Formatter::PreferredFont());
  ui->zoomOption->setFont(Formatter::PreferredFont());

  Reset();

  on_checkerBack_clicked();

  QObject::connect(ui->zoomOption->lineEdit(), &QLineEdit::returnPressed, this,
                   &TextureViewer::zoomOption_returnPressed);

  QObject::connect(ui->depthDisplay, &QToolButton::toggled, this,
                   &TextureViewer::channelsWidget_toggled);
  QObject::connect(ui->stencilDisplay, &QToolButton::toggled, this,
                   &TextureViewer::channelsWidget_toggled);
  QObject::connect(ui->flip_y, &QToolButton::toggled, this, &TextureViewer::channelsWidget_toggled);
  QObject::connect(ui->gammaDisplay, &QToolButton::toggled, this,
                   &TextureViewer::channelsWidget_toggled);
  QObject::connect(ui->channels, OverloadedSlot<int>::of(&QComboBox::currentIndexChanged), this,
                   &TextureViewer::channelsWidget_selected);
  QObject::connect(ui->hdrMul, OverloadedSlot<int>::of(&QComboBox::currentIndexChanged), this,
                   &TextureViewer::channelsWidget_selected);
  QObject::connect(ui->hdrMul, &QComboBox::currentTextChanged, [this] { UI_UpdateChannels(); });
  QObject::connect(ui->customShader, OverloadedSlot<int>::of(&QComboBox::currentIndexChanged), this,
                   &TextureViewer::channelsWidget_selected);
  QObject::connect(ui->customShader, &QComboBox::currentTextChanged, [this] { UI_UpdateChannels(); });
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

  for(RDToolButton *butt : {ui->channelRed, ui->channelGreen, ui->channelBlue, ui->channelAlpha})
  {
    QObject::connect(butt, &RDToolButton::toggled, this, &TextureViewer::channelsWidget_toggled);
    QObject::connect(butt, &RDToolButton::mouseClicked, this,
                     &TextureViewer::channelsWidget_mouseClicked);
    QObject::connect(butt, &RDToolButton::doubleClicked, this,
                     &TextureViewer::channelsWidget_mouseClicked);
  }

  {
    QMenu *extensionsMenu = new QMenu(this);

    ui->extensions->setMenu(extensionsMenu);
    ui->extensions->setPopupMode(QToolButton::InstantPopup);

    QObject::connect(extensionsMenu, &QMenu::aboutToShow, [this, extensionsMenu]() {
      extensionsMenu->clear();
      m_Ctx.Extensions().MenuDisplaying(PanelMenu::TextureViewer, extensionsMenu, ui->extensions, {});
    });
  }

  QWidget *renderContainer = ui->renderContainer;

  ui->dockarea->addToolWindow(ui->renderContainer, ToolWindowManager::EmptySpace);
  ui->dockarea->setToolWindowProperties(
      renderContainer, ToolWindowManager::DisallowUserDocking | ToolWindowManager::HideCloseButton |
                           ToolWindowManager::DisableDraggableTab |
                           ToolWindowManager::AlwaysDisplayFullTabs);

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
  ui->dockarea->setToolWindowProperties(ui->textureListFrame, ToolWindowManager::HideOnClose);

  ui->dockarea->setAllowFloatingWindow(false);

  renderContainer->setWindowTitle(tr("Unbound"));
  ui->pixelContextLayout->setWindowTitle(tr("Pixel Context"));
  ui->outputThumbs->setWindowTitle(tr("Outputs"));
  ui->inputThumbs->setWindowTitle(tr("Inputs"));
  ui->textureListFrame->setWindowTitle(tr("Texture List"));

  ui->textureList->setHoverCursor(Qt::PointingHandCursor);

  m_Goto = new TextureGoto(this, [this](QPoint p) { GotoLocation(p.x(), p.y()); });

  QVBoxLayout *vertical = new QVBoxLayout(this);

  vertical->setSpacing(3);
  vertical->setContentsMargins(3, 3, 3, 3);

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

  ui->texStatusDim->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
  ui->statusText->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));

  ui->statusbar->addWidget(statusflowWidget);

  ui->channels->addItems({lit("RGBA"), lit("RGBM"), lit("YUVA decode"), tr("Custom")});

  ui->zoomOption->addItems({lit("10%"), lit("25%"), lit("50%"), lit("75%"), lit("100%"),
                            lit("200%"), lit("400%"), lit("800%")});

  ui->hdrMul->addItems({lit("2"), lit("4"), lit("8"), lit("16"), lit("32"), lit("128")});

  ui->overlay->addItems({tr("None"), tr("Highlight Drawcall"), tr("Wireframe Mesh"),
                         tr("Depth Test"), tr("Stencil Test"), tr("Backface Cull"),
                         tr("Viewport/Scissor Region"), tr("NaN/INF/-ve Display"),
                         tr("Histogram Clipping"), tr("Clear Before Pass"), tr("Clear Before Draw"),
                         tr("Quad Overdraw (Pass)"), tr("Quad Overdraw (Draw)"),
                         tr("Triangle Size (Pass)"), tr("Triangle Size (Draw)")});

  ui->textureListFilter->addItems({QString(), tr("Textures"), tr("Render Targets")});

  ui->textureList->setModel(new TextureListItemModel(m_Ctx, this));
  ui->textureList->setItemDelegate(new TextureListItemDelegate(ui->textureList));
  ui->textureList->viewport()->setAttribute(Qt::WA_Hover);

  ui->zoomOption->setCurrentText(QString());
  ui->fitToWindow->toggle();

  m_Ctx.AddCaptureViewer(this);

  SetupTextureTabs();
}

TextureViewer::~TextureViewer()
{
  if(m_Output)
  {
    m_Ctx.Replay().BlockInvoke([this](IReplayController *r) { m_Output->Shutdown(); });
  }

  m_Ctx.BuiltinWindowClosed(this);
  m_Ctx.RemoveCaptureViewer(this);
  delete ui;
}

void TextureViewer::enterEvent(QEvent *event)
{
  HighlightUsage();
}

void TextureViewer::showEvent(QShowEvent *event)
{
  HighlightUsage();
}

void TextureViewer::changeEvent(QEvent *event)
{
  if(event->type() == QEvent::PaletteChange || event->type() == QEvent::StyleChange)
  {
    updateBackgroundColors();
    ui->render->update();
  }
}

void TextureViewer::HighlightUsage()
{
  TextureDescription *texptr = GetCurrentTexture();

  if(texptr && m_Ctx.HasTimelineBar())
    m_Ctx.GetTimelineBar()->HighlightResourceUsage(texptr->resourceId);
}

void TextureViewer::RT_FetchCurrentPixel(uint32_t x, uint32_t y, PixelValue &pickValue,
                                         PixelValue &realValue)
{
  TextureDescription *texptr = GetCurrentTexture();

  if(texptr == NULL)
    return;

  if(m_TexDisplay.flipY)
    y = (texptr->height - 1) - y;

  pickValue = m_Output->PickPixel(m_TexDisplay.resourceId, true, x, y, m_TexDisplay.sliceFace,
                                  m_TexDisplay.mip, m_TexDisplay.sampleIdx);

  if(m_TexDisplay.customShaderId != ResourceId())
    realValue = m_Output->PickPixel(m_TexDisplay.resourceId, false, x, y, m_TexDisplay.sliceFace,
                                    m_TexDisplay.mip, m_TexDisplay.sampleIdx);
}

void TextureViewer::RT_PickPixelsAndUpdate(IReplayController *)
{
  PixelValue pickValue, realValue;

  if(m_PickedPoint.x() < 0 || m_PickedPoint.y() < 0)
    return;

  uint32_t x = (uint32_t)m_PickedPoint.x();
  uint32_t y = (uint32_t)m_PickedPoint.y();

  RT_FetchCurrentPixel(x, y, pickValue, realValue);

  m_Output->SetPixelContextLocation(x, y);

  m_CurHoverValue = pickValue;

  m_CurPixelValue = pickValue;
  m_CurRealValue = realValue;

  GUIInvoke::call(this, [this]() { UI_UpdateStatusText(); });
}

void TextureViewer::RT_PickHoverAndUpdate(IReplayController *)
{
  PixelValue pickValue, realValue;

  uint32_t x = (uint32_t)m_CurHoverPixel.x();
  uint32_t y = (uint32_t)m_CurHoverPixel.y();

  RT_FetchCurrentPixel(x, y, pickValue, realValue);

  m_CurHoverValue = pickValue;

  GUIInvoke::call(this, [this]() { UI_UpdateStatusText(); });
}

void TextureViewer::RT_UpdateAndDisplay(IReplayController *)
{
  if(m_Output != NULL)
    m_Output->SetTextureDisplay(m_TexDisplay);

  GUIInvoke::call(this, [this]() { ui->render->update(); });
}

void TextureViewer::RT_UpdateVisualRange(IReplayController *)
{
  TextureDescription *texptr = GetCurrentTexture();

  if(!m_Visualise || texptr == NULL || m_Output == NULL)
    return;

  ResourceFormat fmt = texptr->format;

  if(m_TexDisplay.customShaderId != ResourceId())
    fmt.compCount = 4;

  bool channels[] = {
      m_TexDisplay.red ? true : false, m_TexDisplay.green && fmt.compCount > 1,
      m_TexDisplay.blue && fmt.compCount > 2, m_TexDisplay.alpha && fmt.compCount > 3,
  };

  rdcarray<uint32_t> histogram = m_Output->GetHistogram(ui->rangeHistogram->rangeMin(),
                                                        ui->rangeHistogram->rangeMax(), channels);

  if(!histogram.empty())
  {
    QVector<uint32_t> histogramVec(histogram.count());
    if(!histogram.isEmpty())
      memcpy(histogramVec.data(), histogram.data(), histogram.byteSize());

    GUIInvoke::call(this, [this, histogramVec]() {
      ui->rangeHistogram->setHistogramRange(ui->rangeHistogram->rangeMin(),
                                            ui->rangeHistogram->rangeMax());
      ui->rangeHistogram->setHistogramData(histogramVec);
    });
  }
}

void TextureViewer::UI_UpdateStatusText()
{
  TextureDescription *texptr = GetCurrentTexture();
  if(texptr == NULL)
    return;

  TextureDescription &tex = *texptr;

  bool dsv = (tex.creationFlags & TextureCategory::DepthTarget) ||
             (tex.format.compType == CompType::Depth);
  bool uintTex = (tex.format.compType == CompType::UInt);
  bool sintTex = (tex.format.compType == CompType::SInt);

  if(tex.format.compType == CompType::Typeless && m_TexDisplay.typeHint == CompType::UInt)
    uintTex = true;

  if(tex.format.compType == CompType::Typeless && m_TexDisplay.typeHint == CompType::SInt)
    sintTex = true;

  if(m_TexDisplay.overlay == DebugOverlay::QuadOverdrawPass ||
     m_TexDisplay.overlay == DebugOverlay::QuadOverdrawDraw ||
     m_TexDisplay.overlay == DebugOverlay::TriangleSizeDraw ||
     m_TexDisplay.overlay == DebugOverlay::TriangleSizeDraw)
  {
    dsv = false;
    uintTex = false;
    sintTex = false;
  }

  QColor swatchColor;

  if(dsv || uintTex || sintTex)
  {
    swatchColor = QColor(0, 0, 0);
  }
  else
  {
    float r = qBound(0.0f, m_CurHoverValue.floatValue[0], 1.0f);
    float g = qBound(0.0f, m_CurHoverValue.floatValue[1], 1.0f);
    float b = qBound(0.0f, m_CurHoverValue.floatValue[2], 1.0f);

    if(tex.format.SRGBCorrected() || (tex.creationFlags & TextureCategory::SwapBuffer))
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

  if(m_Ctx.APIProps().pipelineType == GraphicsAPI::OpenGL)
    y = (int)(mipHeight - 1) - y;
  if(m_TexDisplay.flipY)
    y = (int)(mipHeight - 1) - y;

  y = qMax(0, y);

  int x = m_CurHoverPixel.x() >> (int)m_TexDisplay.mip;
  float invWidth = 1.0f / mipWidth;
  float invHeight = 1.0f / mipHeight;

  QString hoverCoords = QFormatStr("%1, %2 (%3, %4)")
                            .arg(x, 4)
                            .arg(y, 4)
                            .arg((x * invWidth), 5, 'f', 4)
                            .arg((y * invHeight), 5, 'f', 4);

  QString statusText = tr("Hover - ") + hoverCoords;

  uint32_t hoverX = (uint32_t)m_CurHoverPixel.x();
  uint32_t hoverY = (uint32_t)m_CurHoverPixel.y();

  if(hoverX > tex.width || hoverY > tex.height)
    statusText = tr("Hover - [%1]").arg(hoverCoords);

  if(m_PickedPoint.x() >= 0)
  {
    x = m_PickedPoint.x() >> (int)m_TexDisplay.mip;
    y = m_PickedPoint.y() >> (int)m_TexDisplay.mip;
    if(m_Ctx.APIProps().pipelineType == GraphicsAPI::OpenGL)
      y = (int)(mipHeight - 1) - y;
    if(m_TexDisplay.flipY)
      y = (int)(mipHeight - 1) - y;

    y = qMax(0, y);

    statusText += tr(" - Right click - %1, %2: ").arg(x, 4).arg(y, 4);

    PixelValue val = m_CurPixelValue;

    if(m_TexDisplay.customShaderId != ResourceId())
    {
      statusText += QFormatStr("%1, %2, %3, %4")
                        .arg(Formatter::Format(val.floatValue[0]))
                        .arg(Formatter::Format(val.floatValue[1]))
                        .arg(Formatter::Format(val.floatValue[2]))
                        .arg(Formatter::Format(val.floatValue[3]));

      val = m_CurRealValue;

      statusText += tr(" (Real: ");
    }

    if(dsv)
    {
      statusText += tr("Depth ");
      if(uintTex)
      {
        statusText += Formatter::Format(val.uintValue[0]);
      }
      else
      {
        statusText += Formatter::Format(val.floatValue[0]);
      }

      int stencil = (int)(255.0f * val.floatValue[1]);

      statusText +=
          tr(", Stencil %1 / 0x%2").arg(stencil).arg(Formatter::Format(uint8_t(stencil & 0xff), true));
    }
    else
    {
      if(uintTex)
      {
        statusText += QFormatStr("%1, %2, %3, %4")
                          .arg(Formatter::Format(val.uintValue[0]))
                          .arg(Formatter::Format(val.uintValue[1]))
                          .arg(Formatter::Format(val.uintValue[2]))
                          .arg(Formatter::Format(val.uintValue[3]));
      }
      else if(sintTex)
      {
        statusText += QFormatStr("%1, %2, %3, %4")
                          .arg(Formatter::Format(val.intValue[0]))
                          .arg(Formatter::Format(val.intValue[1]))
                          .arg(Formatter::Format(val.intValue[2]))
                          .arg(Formatter::Format(val.intValue[3]));
      }
      else
      {
        statusText += QFormatStr("%1, %2, %3, %4")
                          .arg(Formatter::Format(val.floatValue[0]))
                          .arg(Formatter::Format(val.floatValue[1]))
                          .arg(Formatter::Format(val.floatValue[2]))
                          .arg(Formatter::Format(val.floatValue[3]));
      }
    }

    if(m_TexDisplay.customShaderId != ResourceId())
      statusText += lit(")");
  }
  else
  {
    statusText += tr(" - Right click to pick a pixel");
  }

  // try and keep status text consistent by sticking to the high water mark
  // of length (prevents nasty oscillation when the length of the string is
  // just popping over/under enough to overflow onto the next line).

  if(statusText.length() > m_HighWaterStatusLength)
    m_HighWaterStatusLength = statusText.length();

  if(statusText.length() < m_HighWaterStatusLength)
    statusText += QString(m_HighWaterStatusLength - statusText.length(), QLatin1Char(' '));

  ui->statusText->setText(statusText);
}

void TextureViewer::UI_UpdateTextureDetails()
{
  QString status;

  TextureDescription *texptr = GetCurrentTexture();
  if(texptr == NULL)
  {
    ui->texStatusDim->setText(status);

    ui->renderContainer->setWindowTitle(tr("Unbound"));
    return;
  }

  TextureDescription &current = *texptr;

  ResourceId followID = m_Following.GetResourceId(m_Ctx);

  {
    TextureDescription *followtex = m_Ctx.GetTexture(followID);
    BufferDescription *followbuf = m_Ctx.GetBuffer(followID);

    QString title;

    if(followID == ResourceId())
    {
      title = tr("Unbound");
    }
    else if(followtex || followbuf)
    {
      QString name = m_Ctx.GetResourceName(followID);

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

  status = m_Ctx.GetResourceName(current.resourceId) + lit(" - ");

  if(current.dimension >= 1)
    status += QString::number(current.width);
  if(current.dimension >= 2)
    status += lit("x") + QString::number(current.height);
  if(current.dimension >= 3)
    status += lit("x") + QString::number(current.depth);

  if(current.arraysize > 1)
    status += QFormatStr("[%1]").arg(QString::number(current.arraysize));

  if(current.msQual > 0 || current.msSamp > 1)
    status += QFormatStr(" MS{%1x %2Q}").arg(current.msSamp).arg(current.msQual);

  status += QFormatStr(" %1 mips").arg(current.mips);

  status += lit(" - ") + current.format.Name();

  const bool yuv = (current.format.type == ResourceFormatType::YUV8 ||
                    current.format.type == ResourceFormatType::YUV10 ||
                    current.format.type == ResourceFormatType::YUV12 ||
                    current.format.type == ResourceFormatType::YUV16);

  CompType viewCast = CompType::Typeless;

  if(current.format.compType != m_TexDisplay.typeHint &&
     m_TexDisplay.typeHint != CompType::Typeless && !yuv)
  {
    viewCast = m_TexDisplay.typeHint;
  }
  else if(current.format.compType == CompType::Typeless &&
          m_TexDisplay.typeHint == CompType::Typeless && !yuv)
  {
    // if it's a typeless texture and we don't have a hint, ensure the user knows it's being viewed
    // as unorm as a fallback
    viewCast = CompType::UNorm;
  }

  if(viewCast != CompType::Typeless)
    status += tr(" Viewed as %1").arg(ToQStr(viewCast));

  ui->texStatusDim->setText(status);
}

void TextureViewer::UI_OnTextureSelectionChanged(bool newdraw)
{
  TextureDescription *texptr = GetCurrentTexture();

  // reset high-water mark
  m_HighWaterStatusLength = 0;

  if(texptr == NULL)
    return;

  TextureDescription &tex = *texptr;

  bool newtex = (m_TexDisplay.resourceId != tex.resourceId);

  // save settings for this current texture
  if(m_Ctx.Config().TextureViewer_PerTexSettings)
  {
    m_TextureSettings[m_TexDisplay.resourceId].r = ui->channelRed->isChecked();
    m_TextureSettings[m_TexDisplay.resourceId].g = ui->channelGreen->isChecked();
    m_TextureSettings[m_TexDisplay.resourceId].b = ui->channelBlue->isChecked();
    m_TextureSettings[m_TexDisplay.resourceId].a = ui->channelAlpha->isChecked();

    // save state regardless, we just don't apply it without the setting
    m_TextureSettings[m_TexDisplay.resourceId].flip_y = ui->flip_y->isChecked();

    m_TextureSettings[m_TexDisplay.resourceId].displayType = qMax(0, ui->channels->currentIndex());
    m_TextureSettings[m_TexDisplay.resourceId].customShader = ui->customShader->currentText();

    m_TextureSettings[m_TexDisplay.resourceId].depth = ui->depthDisplay->isChecked();
    m_TextureSettings[m_TexDisplay.resourceId].stencil = ui->stencilDisplay->isChecked();

    m_TextureSettings[m_TexDisplay.resourceId].mip = qMax(0, ui->mipLevel->currentIndex());
    m_TextureSettings[m_TexDisplay.resourceId].slice = qMax(0, ui->sliceFace->currentIndex());

    m_TextureSettings[m_TexDisplay.resourceId].minrange = ui->rangeHistogram->blackPoint();
    m_TextureSettings[m_TexDisplay.resourceId].maxrange = ui->rangeHistogram->whitePoint();

    if(m_TexDisplay.typeHint != CompType::Typeless)
      m_TextureSettings[m_TexDisplay.resourceId].typeHint = m_TexDisplay.typeHint;
  }

  m_TexDisplay.resourceId = tex.resourceId;

  // interpret the texture according to the currently following type.
  if(!currentTextureIsLocked())
    m_TexDisplay.typeHint = m_Following.GetTypeHint(m_Ctx);
  else
    m_TexDisplay.typeHint = CompType::Typeless;

  // if there is no such type or it isn't being followed, use the last seen interpretation
  if(m_TexDisplay.typeHint == CompType::Typeless &&
     m_TextureSettings.contains(m_TexDisplay.resourceId))
    m_TexDisplay.typeHint = m_TextureSettings[m_TexDisplay.resourceId].typeHint;

  // try to maintain the pan in the new texture. If the new texture
  // is approx an integer multiple of the old texture, just changing
  // the scale will keep everything the same. This is useful for
  // downsample chains and things where you're flipping back and forth
  // between overlapping textures, but even in the non-integer case
  // pan will be kept approximately the same.
  QSizeF curSize((float)tex.width, (float)tex.height);
  float curArea = area(curSize);
  float prevArea = area(m_PrevSize);

  if(prevArea > 0.0f && m_PrevSize.width() > 0.0f)
  {
    float prevX = m_TexDisplay.xOffset;
    float prevY = m_TexDisplay.yOffset;

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

      m_TexDisplay.xOffset = prevX * scaleFactor;
      m_TexDisplay.yOffset = prevY * scaleFactor;
    }
  }

  m_PrevSize = curSize;

  // refresh scroll position
  setScrollPosition(getScrollPosition());

  UI_UpdateStatusText();

  // block signals for mipLevel and sliceFace comboboxes while editing them
  ui->mipLevel->blockSignals(true);
  ui->sliceFace->blockSignals(true);

  ui->mipLevel->clear();

  m_TexDisplay.mip = 0;
  m_TexDisplay.sliceFace = 0;

  bool usemipsettings = true;
  bool useslicesettings = true;

  if(tex.msSamp > 1)
  {
    for(uint32_t i = 0; i < tex.msSamp; i++)
      ui->mipLevel->addItem(tr("Sample %1").arg(i));

    // add an option to display unweighted average resolved value,
    // to get an idea of how the samples average
    if(tex.format.compType != CompType::UInt && tex.format.compType != CompType::SInt &&
       tex.format.compType != CompType::Depth && !(tex.creationFlags & TextureCategory::DepthTarget))
      ui->mipLevel->addItem(tr("Average val"));

    ui->mipLabel->setText(tr("Sample"));

    ui->mipLevel->setCurrentIndex(0);
  }
  else
  {
    for(uint32_t i = 0; i < tex.mips; i++)
      ui->mipLevel->addItem(
          QFormatStr("%1 - %2x%3").arg(i).arg(qMax(1U, tex.width >> i)).arg(qMax(1U, tex.height >> i)));

    ui->mipLabel->setText(tr("Mip"));
  }

  if(tex.mips == 1 && tex.msSamp <= 1)
    ui->mipLevel->setEnabled(false);
  else
    ui->mipLevel->setEnabled(true);

  ui->sliceFace->clear();

  uint32_t numSlices = 1;

  if(tex.arraysize == 1 && tex.depth <= 1)
  {
    ui->sliceFace->setEnabled(false);
  }
  else
  {
    ui->sliceFace->setEnabled(true);

    QString cubeFaces[] = {lit("X+"), lit("X-"), lit("Y+"), lit("Y-"), lit("Z+"), lit("Z-")};

    numSlices = tex.arraysize;

    // for 3D textures, display the number of slices at this mip
    if(tex.depth > 1)
      numSlices = qMax(1u, tex.depth >> (int)ui->mipLevel->currentIndex());

    for(uint32_t i = 0; i < numSlices; i++)
    {
      if(tex.cubemap)
      {
        QString name = cubeFaces[i % 6];
        if(numSlices > 6)
          name = QFormatStr("[%1] %2").arg(i / 6).arg(
              cubeFaces[i % 6]);    // Front 1, Back 2, 3, 4 etc for cube arrays
        ui->sliceFace->addItem(name);
      }
      else
      {
        ui->sliceFace->addItem(tr("Slice %1").arg(i));
      }
    }
  }

  // enable signals for mipLevel and sliceFace
  ui->mipLevel->blockSignals(false);
  ui->sliceFace->blockSignals(false);

  {
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

  {
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
  if(m_Ctx.Config().TextureViewer_PerTexSettings && m_TextureSettings.contains(tex.resourceId))
  {
    if(usemipsettings)
      ui->mipLevel->setCurrentIndex(m_TextureSettings[tex.resourceId].mip);

    if(useslicesettings)
      ui->sliceFace->setCurrentIndex(m_TextureSettings[tex.resourceId].slice);

    if(m_Ctx.Config().TextureViewer_PerTexYFlip)
      ui->flip_y->setChecked(m_TextureSettings[tex.resourceId].flip_y);
  }

  // handling for if we've switched to a new texture
  if(newtex)
  {
    // if we save certain settings per-texture, restore them (if we have any)
    if(m_Ctx.Config().TextureViewer_PerTexSettings && m_TextureSettings.contains(tex.resourceId))
    {
      ui->channels->setCurrentIndex(m_TextureSettings[tex.resourceId].displayType);

      ui->customShader->setCurrentText(m_TextureSettings[tex.resourceId].customShader);

      ui->channelRed->setChecked(m_TextureSettings[tex.resourceId].r);
      ui->channelGreen->setChecked(m_TextureSettings[tex.resourceId].g);
      ui->channelBlue->setChecked(m_TextureSettings[tex.resourceId].b);
      ui->channelAlpha->setChecked(m_TextureSettings[tex.resourceId].a);

      ui->depthDisplay->setChecked(m_TextureSettings[tex.resourceId].depth);
      ui->stencilDisplay->setChecked(m_TextureSettings[tex.resourceId].stencil);

      if(m_Ctx.Config().TextureViewer_PerTexYFlip)
        ui->flip_y->setChecked(m_TextureSettings[tex.resourceId].flip_y);

      m_NoRangePaint = true;
      ui->rangeHistogram->setRange(m_TextureSettings[m_TexDisplay.resourceId].minrange,
                                   m_TextureSettings[m_TexDisplay.resourceId].maxrange);
      m_NoRangePaint = false;
    }
    else if(m_Ctx.Config().TextureViewer_PerTexSettings)
    {
      // if we are using per-tex settings, reset back to RGB
      ui->channels->setCurrentIndex(0);

      ui->customShader->setCurrentText(QString());

      ui->channelRed->setChecked(true);
      ui->channelGreen->setChecked(true);
      ui->channelBlue->setChecked(true);
      ui->channelAlpha->setChecked(false);

      ui->depthDisplay->setChecked(true);
      ui->stencilDisplay->setChecked(false);

      if(m_Ctx.Config().TextureViewer_PerTexYFlip)
        ui->flip_y->setChecked(false);

      m_NoRangePaint = true;
      UI_SetHistogramRange(texptr, m_TexDisplay.typeHint);
      m_NoRangePaint = false;
    }

    // reset the range if desired
    if(m_Ctx.Config().TextureViewer_ResetRange)
    {
      UI_SetHistogramRange(texptr, m_TexDisplay.typeHint);
    }
  }

  UI_UpdateFittedScale();
  UI_UpdateTextureDetails();
  UI_UpdateChannels();

  if(ui->autoFit->isChecked())
    AutoFitRange();

  m_Ctx.Replay().AsyncInvoke([this](IReplayController *r) {
    RT_UpdateVisualRange(r);

    RT_UpdateAndDisplay(r);

    if(m_Output != NULL)
      RT_PickPixelsAndUpdate(r);
  });

  HighlightUsage();
}

void TextureViewer::UI_SetHistogramRange(const TextureDescription *tex, CompType typeHint)
{
  if(tex != NULL && (tex->format.compType == CompType::SNorm || typeHint == CompType::SNorm))
    ui->rangeHistogram->setRange(-1.0f, 1.0f);
  else
    ui->rangeHistogram->setRange(0.0f, 1.0f);
}

void TextureViewer::UI_UpdateChannels()
{
  TextureDescription *tex = GetCurrentTexture();

#define SHOW(widget) widget->setVisible(true)
#define HIDE(widget) widget->setVisible(false)
#define ENABLE(widget) widget->setEnabled(true)
#define DISABLE(widget) widget->setEnabled(false)

  if(tex != NULL && (tex->creationFlags & TextureCategory::SwapBuffer))
  {
    // swapbuffer is always srgb for 8-bit types, linear for 16-bit types
    DISABLE(ui->gammaDisplay);

    if(tex->format.compByteWidth == 2 && tex->format.type == ResourceFormatType::Regular)
      m_TexDisplay.linearDisplayAsGamma = false;
    else
      m_TexDisplay.linearDisplayAsGamma = true;
  }
  else
  {
    if(tex != NULL && !tex->format.SRGBCorrected())
      ENABLE(ui->gammaDisplay);
    else
      DISABLE(ui->gammaDisplay);

    m_TexDisplay.linearDisplayAsGamma =
        !ui->gammaDisplay->isEnabled() || ui->gammaDisplay->isChecked();
  }

  if(tex != NULL && tex->format.SRGBCorrected())
    m_TexDisplay.linearDisplayAsGamma = false;

  bool dsv = false;
  if(tex != NULL)
    dsv = (tex->creationFlags & TextureCategory::DepthTarget) ||
          (tex->format.compType == CompType::Depth);

  bool yuv = false;
  if(tex != NULL)
    yuv = (tex->format.type == ResourceFormatType::YUV8 ||
           tex->format.type == ResourceFormatType::YUV10 ||
           tex->format.type == ResourceFormatType::YUV12 ||
           tex->format.type == ResourceFormatType::YUV16);

  const bool defaultDisplay = ui->channels->currentIndex() == 0;
  const bool rgbmDisplay = ui->channels->currentIndex() == 1;
  const bool yuvDecodeDisplay = ui->channels->currentIndex() == 2;
  const bool customDisplay = ui->channels->currentIndex() == 3;

  if(customDisplay && m_NeedCustomReload)
  {
    m_NeedCustomReload = false;

    reloadCustomShaders(QString());
  }

  ui->channels->setItemText(0, yuv ? lit("YUVA") : lit("RGBA"));
  ui->channelRed->setText(lit("R"));
  ui->channelGreen->setText(lit("G"));
  ui->channelBlue->setText(lit("B"));

  if(dsv && !customDisplay)
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
    SHOW(ui->depthDisplay);
    SHOW(ui->stencilDisplay);

    m_TexDisplay.red = ui->depthDisplay->isChecked();
    m_TexDisplay.green = ui->stencilDisplay->isChecked();
    m_TexDisplay.blue = false;
    m_TexDisplay.alpha = false;

    if(m_TexDisplay.red == m_TexDisplay.green && !m_TexDisplay.red)
    {
      m_TexDisplay.red = true;
      ui->depthDisplay->setChecked(true);
    }

    m_TexDisplay.decodeYUV = false;
    m_TexDisplay.hdrMultiplier = -1.0f;
    if(m_TexDisplay.customShaderId != ResourceId())
    {
      memset(m_CurPixelValue.floatValue, 0, sizeof(float) * 4);
      memset(m_CurRealValue.floatValue, 0, sizeof(float) * 4);
      UI_UpdateStatusText();
    }
    m_TexDisplay.customShaderId = ResourceId();
  }
  else if(defaultDisplay || yuvDecodeDisplay || !m_Ctx.IsCaptureLoaded())
  {
    // RGBA. YUV Decode is almost identical but we set decodeYUV
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
    HIDE(ui->depthDisplay);
    HIDE(ui->stencilDisplay);

    m_TexDisplay.red = ui->channelRed->isChecked();
    m_TexDisplay.green = ui->channelGreen->isChecked();
    m_TexDisplay.blue = ui->channelBlue->isChecked();
    m_TexDisplay.alpha = ui->channelAlpha->isChecked();

    if(!yuvDecodeDisplay && yuv)
    {
      ui->channelRed->setText(lit("V"));
      ui->channelGreen->setText(lit("Y"));
      ui->channelBlue->setText(lit("U"));
    }

    m_TexDisplay.decodeYUV = yuvDecodeDisplay;
    m_TexDisplay.hdrMultiplier = -1.0f;
    if(m_TexDisplay.customShaderId != ResourceId())
    {
      memset(m_CurPixelValue.floatValue, 0, sizeof(float) * 4);
      memset(m_CurRealValue.floatValue, 0, sizeof(float) * 4);
      UI_UpdateStatusText();
    }
    m_TexDisplay.customShaderId = ResourceId();
  }
  else if(rgbmDisplay)
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
    HIDE(ui->depthDisplay);
    HIDE(ui->stencilDisplay);

    m_TexDisplay.red = ui->channelRed->isChecked();
    m_TexDisplay.green = ui->channelGreen->isChecked();
    m_TexDisplay.blue = ui->channelBlue->isChecked();
    m_TexDisplay.alpha = false;

    bool ok = false;
    float mul = ui->hdrMul->currentText().toFloat(&ok);

    if(!ok)
    {
      mul = 32.0f;
      ui->hdrMul->setCurrentText(lit("32"));
    }

    m_TexDisplay.decodeYUV = false;
    m_TexDisplay.hdrMultiplier = mul;
    if(m_TexDisplay.customShaderId != ResourceId())
    {
      memset(m_CurPixelValue.floatValue, 0, sizeof(float) * 4);
      memset(m_CurRealValue.floatValue, 0, sizeof(float) * 4);
      UI_UpdateStatusText();
    }
    m_TexDisplay.customShaderId = ResourceId();
  }
  else if(customDisplay)
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
    HIDE(ui->depthDisplay);
    HIDE(ui->stencilDisplay);

    m_TexDisplay.red = ui->channelRed->isChecked();
    m_TexDisplay.green = ui->channelGreen->isChecked();
    m_TexDisplay.blue = ui->channelBlue->isChecked();
    m_TexDisplay.alpha = ui->channelAlpha->isChecked();

    m_TexDisplay.decodeYUV = false;
    m_TexDisplay.hdrMultiplier = -1.0f;

    m_TexDisplay.customShaderId = ResourceId();

    QString shaderName = ui->customShader->currentText().toUpper();

    if(m_CustomShaders.contains(shaderName))
    {
      if(m_TexDisplay.customShaderId == ResourceId())
      {
        memset(m_CurPixelValue.floatValue, 0, sizeof(float) * 4);
        memset(m_CurRealValue.floatValue, 0, sizeof(float) * 4);
        UI_UpdateStatusText();
      }
      m_TexDisplay.customShaderId = m_CustomShaders[shaderName];
      ui->customDelete->setEnabled(true);
      ui->customEdit->setEnabled(true);
    }
    else
    {
      ui->customDelete->setEnabled(false);
      ui->customEdit->setEnabled(false);
    }
  }

#undef HIDE
#undef SHOW
#undef ENABLE
#undef DISABLE

  m_TexDisplay.flipY = ui->flip_y->isChecked();

  INVOKE_MEMFN(RT_UpdateAndDisplay);
  INVOKE_MEMFN(RT_UpdateVisualRange);
}

void TextureViewer::SetupTextureTabs()
{
  ToolWindowManagerArea *textureTabs = ui->dockarea->areaOf(ui->renderContainer);

  if(!textureTabs)
    return;

  QIcon tabIcon;
  tabIcon.addFile(QStringLiteral(":/logo.svg"), QSize(), QIcon::Normal, QIcon::Off);

  textureTabs->setTabIcon(0, tabIcon);

  textureTabs->setElideMode(Qt::ElideRight);

  QObject::connect(textureTabs, &QTabWidget::currentChanged, this,
                   &TextureViewer::textureTab_Changed);
  QObject::connect(textureTabs, &QTabWidget::tabCloseRequested, this,
                   &TextureViewer::textureTab_Closing);

  textureTabs->disableUserDrop();

  textureTabs->tabBar()->setContextMenuPolicy(Qt::CustomContextMenu);

  QObject::connect(textureTabs->tabBar(), &QTabBar::customContextMenuRequested, this,
                   &TextureViewer::textureTab_Menu);
}

void TextureViewer::textureTab_Menu(const QPoint &pos)
{
  ToolWindowManagerArea *textureTabs = ui->dockarea->areaOf(ui->renderContainer);

  int tabIndex = textureTabs->tabBar()->tabAt(pos);

  if(tabIndex == -1)
    return;

  QAction closeTab(tr("Close tab"), this);
  QAction closeOtherTabs(tr("Close other tabs"), this);
  QAction closeRightTabs(tr("Close tabs to the right"), this);

  if(textureTabs->widget(tabIndex) == ui->renderContainer)
    closeTab.setEnabled(false);

  QMenu contextMenu(this);

  contextMenu.addAction(&closeTab);
  contextMenu.addAction(&closeOtherTabs);
  contextMenu.addAction(&closeRightTabs);

  QObject::connect(&closeTab, &QAction::triggered, [textureTabs, tabIndex]() {
    // remove the tab at this index
    textureTabs->removeTab(tabIndex);
  });

  QObject::connect(&closeRightTabs, &QAction::triggered, [textureTabs, tabIndex]() {
    // remove all tabs with a greater index
    while(textureTabs->count() > tabIndex + 1)
      textureTabs->removeTab(tabIndex + 1);
  });

  QObject::connect(&closeOtherTabs, &QAction::triggered, [textureTabs, tabIndex]() {
    // remove all tabs with a greater index
    while(textureTabs->count() > tabIndex + 1)
      textureTabs->removeTab(tabIndex + 1);

    // remove all tabs at index 1 until there's only two, these are the ones between the locked tab
    // 0 and the tabIndex
    while(textureTabs->count() > 2)
      textureTabs->removeTab(1);
  });

  RDDialog::show(&contextMenu, QCursor::pos());
}

void TextureViewer::textureTab_Changed(int index)
{
  ToolWindowManagerArea *textureTabs = ui->dockarea->areaOf(ui->renderContainer);

  if(!textureTabs)
    return;

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
      m_LockedTabs.remove(id);

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
  if(!m_Ctx.IsCaptureLoaded())
    return;

  TextureDescription *tex = GetCurrentTexture();

  if(tex == NULL)
    return;

  m_PickedPoint = QPoint(x, y);

  uint32_t mipHeight = qMax(1U, tex->height >> (int)m_TexDisplay.mip);
  if(m_Ctx.APIProps().pipelineType == GraphicsAPI::OpenGL)
    m_PickedPoint.setY((int)(mipHeight - 1) - m_PickedPoint.y());
  if(m_TexDisplay.flipY)
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
    GUIInvoke::call(this, [this, ID, focus] { this->ViewTexture(ID, focus); });
    return;
  }

  if(m_LockedTabs.contains(ID))
  {
    if(focus)
      ToolWindowManager::raiseToolWindow(this);

    QWidget *w = m_LockedTabs[ID];
    ToolWindowManagerArea *textureTabs = ui->dockarea->areaOf(ui->renderContainer);

    int idx = textureTabs->indexOf(w);

    if(idx >= 0)
      textureTabs->setCurrentIndex(idx);

    INVOKE_MEMFN(RT_UpdateAndDisplay);
    return;
  }

  TextureDescription *tex = m_Ctx.GetTexture(ID);
  if(tex)
  {
    QWidget *lockedContainer = new QWidget(this);
    lockedContainer->setWindowTitle(m_Ctx.GetResourceName(ID));
    lockedContainer->setProperty("id", QVariant::fromValue(ID));

    ToolWindowManagerArea *textureTabs = ui->dockarea->areaOf(ui->renderContainer);

    ToolWindowManager::AreaReference ref(ToolWindowManager::AddTo, textureTabs);

    ui->dockarea->addToolWindow(lockedContainer, ref);
    ui->dockarea->setToolWindowProperties(
        lockedContainer,
        ToolWindowManager::DisallowUserDocking | ToolWindowManager::AlwaysDisplayFullTabs);

    lockedContainer->setLayout(ui->renderLayout);

    int idx = textureTabs->indexOf(lockedContainer);

    if(idx >= 0)
      textureTabs->setTabIcon(idx, Icons::page_go());
    else
      qCritical() << "Couldn't get tab index of new tab to set icon";

    // newPanel.DockHandler.TabPageContextMenuStrip = tabContextMenu;

    if(focus)
      ToolWindowManager::raiseToolWindow(this);

    m_LockedTabs[ID] = lockedContainer;

    INVOKE_MEMFN(RT_UpdateAndDisplay);
    return;
  }

  BufferDescription *buf = m_Ctx.GetBuffer(ID);
  if(buf)
  {
    IBufferViewer *viewer = m_Ctx.ViewBuffer(0, 0, ID);

    m_Ctx.AddDockWindow(viewer->Widget(), DockReference::AddTo, this);
  }
}

void TextureViewer::texContextItem_triggered()
{
  QAction *act = qobject_cast<QAction *>(QObject::sender());

  QVariant eid = act->property("eid");
  if(eid.isValid())
  {
    m_Ctx.SetEventID({}, eid.toUInt(), eid.toUInt());
    return;
  }

  QVariant id = act->property("id");
  if(id.isValid())
  {
    ViewTexture(id.value<ResourceId>(), false);
    return;
  }
}

void TextureViewer::showUnused_triggered()
{
  m_ShowUnused = !m_ShowUnused;

  if(m_Ctx.IsCaptureLoaded())
    m_Ctx.RefreshStatus();
}

void TextureViewer::showEmpty_triggered()
{
  m_ShowEmpty = !m_ShowEmpty;

  if(m_Ctx.IsCaptureLoaded())
    m_Ctx.RefreshStatus();
}

void TextureViewer::AddResourceUsageEntry(QMenu &menu, uint32_t start, uint32_t end,
                                          ResourceUsage usage)
{
  QAction *item = NULL;

  if(start == end)
    item = new QAction(
        QFormatStr("EID %1: %2").arg(start).arg(ToQStr(usage, m_Ctx.APIProps().pipelineType)), this);
  else
    item = new QAction(
        QFormatStr("EID %1-%2: %3").arg(start).arg(end).arg(ToQStr(usage, m_Ctx.APIProps().pipelineType)),
        this);

  QObject::connect(item, &QAction::triggered, this, &TextureViewer::texContextItem_triggered);
  item->setProperty("eid", QVariant(end));

  menu.addAction(item);
}

void TextureViewer::OpenResourceContextMenu(ResourceId id, bool input,
                                            const rdcarray<EventUsage> &usage)
{
  QMenu contextMenu(this);

  QAction showUnused(tr("Show Unused"), this);
  QAction showEmpty(tr("Show Empty"), this);
  QAction openLockedTab(tr("Open new Locked Tab"), this);
  QAction openResourceInspector(tr("Open in Resource Inspector"), this);
  QAction usageTitle(tr("Used:"), this);
  QAction imageLayout(this);

  openLockedTab.setIcon(Icons::action_hover());
  openResourceInspector.setIcon(Icons::link());

  showUnused.setChecked(m_ShowUnused);
  showUnused.setChecked(m_ShowEmpty);

  contextMenu.addAction(&showUnused);
  contextMenu.addAction(&showEmpty);

  QObject::connect(&showUnused, &QAction::triggered, this, &TextureViewer::showUnused_triggered);
  QObject::connect(&showEmpty, &QAction::triggered, this, &TextureViewer::showEmpty_triggered);

  if(m_Ctx.CurPipelineState().SupportsBarriers())
  {
    contextMenu.addSeparator();
    imageLayout.setText(tr("Image is in layout ") + m_Ctx.CurPipelineState().GetResourceLayout(id));
    contextMenu.addAction(&imageLayout);
  }

  if(id != ResourceId())
  {
    contextMenu.addSeparator();
    contextMenu.addAction(&openLockedTab);
    contextMenu.addAction(&openResourceInspector);

    contextMenu.addSeparator();
    m_Ctx.Extensions().MenuDisplaying(input ? ContextMenu::TextureViewer_InputThumbnail
                                            : ContextMenu::TextureViewer_OutputThumbnail,
                                      &contextMenu, {{"resourceId", id}});

    contextMenu.addSeparator();
    contextMenu.addAction(&usageTitle);

    openLockedTab.setProperty("id", QVariant::fromValue(id));

    QObject::connect(&openLockedTab, &QAction::triggered, this,
                     &TextureViewer::texContextItem_triggered);

    QObject::connect(&openResourceInspector, &QAction::triggered, [this, id]() {
      m_Ctx.ShowResourceInspector();

      m_Ctx.GetResourceInspector()->Inspect(id);
    });

    CombineUsageEvents(m_Ctx, usage,
                       [this, &contextMenu](uint32_t start, uint32_t end, ResourceUsage use) {
                         AddResourceUsageEntry(contextMenu, start, end, use);
                       });

    RDDialog::show(&contextMenu, QCursor::pos());
  }
  else
  {
    contextMenu.addSeparator();
    m_Ctx.Extensions().MenuDisplaying(input ? ContextMenu::TextureViewer_InputThumbnail
                                            : ContextMenu::TextureViewer_OutputThumbnail,
                                      &contextMenu, {});

    RDDialog::show(&contextMenu, QCursor::pos());
  }
}

void TextureViewer::InitResourcePreview(ResourcePreview *prev, BoundResource res, bool force,
                                        Following &follow, const QString &bindName,
                                        const QString &slotName)
{
  if(res.resourceId != ResourceId() || force)
  {
    QString fullname = bindName;
    if(!m_Ctx.IsAutogeneratedName(res.resourceId))
    {
      if(!fullname.isEmpty())
        fullname += lit(" = ");
      fullname += m_Ctx.GetResourceName(res.resourceId);
    }
    if(fullname.isEmpty())
      fullname = m_Ctx.GetResourceName(res.resourceId);

    prev->setResourceName(fullname);

    WindowingData winData = m_Ctx.CreateWindowingData(prev->thumbWidget());

    if(m_Ctx.GetTexture(res.resourceId))
    {
      m_Ctx.Replay().AsyncInvoke([this, winData, res](IReplayController *) {
        m_Output->AddThumbnail(winData, res.resourceId, res.typeHint,
                               res.firstMip >= 0 ? res.firstMip : 0,
                               res.firstSlice >= 0 ? res.firstSlice : 0);
      });
    }
    else
    {
      m_Ctx.Replay().AsyncInvoke([this, winData](IReplayController *) {
        m_Output->AddThumbnail(winData, ResourceId(), CompType::Typeless, 0, 0);
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

    WindowingData winData = m_Ctx.CreateWindowingData(prev->thumbWidget());
    m_Ctx.Replay().AsyncInvoke([this, winData](IReplayController *) {
      m_Output->AddThumbnail(winData, ResourceId(), CompType::Typeless, 0, 0);
    });
  }
  else
  {
    prev->setResourceName(QString());
    prev->setActive(false);
    prev->setSelected(false);
  }
}

void TextureViewer::InitStageResourcePreviews(ShaderStage stage,
                                              const rdcarray<ShaderResource> &resourceDetails,
                                              const rdcarray<Bindpoint> &mapping,
                                              rdcarray<BoundResourceArray> &ResList,
                                              ThumbnailStrip *prevs, int &prevIndex, bool copy,
                                              bool rw)
{
  for(int idx = 0; idx < mapping.count(); idx++)
  {
    const Bindpoint &key = mapping[idx];

    const rdcarray<BoundResource> *resArray = NULL;
    uint32_t dynamicallyUsedResCount = 1;

    int residx = ResList.indexOf(key);
    if(residx >= 0)
    {
      resArray = &ResList[residx].resources;
      dynamicallyUsedResCount = ResList[residx].dynamicallyUsedCount;
    }

    const bool collapseArray = dynamicallyUsedResCount > 20;

    const int arrayLen = resArray != NULL ? resArray->count() : 1;

    for(int arrayIdx = 0; arrayIdx < arrayLen; arrayIdx++)
    {
      if(resArray && !resArray->at(arrayIdx).dynamicallyUsed)
        continue;

      BoundResource res = {};

      if(resArray)
        res = resArray->at(arrayIdx);

      bool used = key.used;

      QString bindName;

      for(const ShaderResource &bind : resourceDetails)
      {
        if(bind.bindPoint == idx)
        {
          bindName = bind.name;
          break;
        }
      }

      if(copy)
      {
        used = true;
        bindName = tr("Source");
      }

      Following follow(rw ? FollowType::ReadWrite : FollowType::ReadOnly, stage, idx, arrayIdx);
      QString slotName = QFormatStr("%1 %2%3")
                             .arg(m_Ctx.CurPipelineState().Abbrev(stage))
                             .arg(rw ? lit("RW ") : lit(""))
                             .arg(idx);

      if(collapseArray)
        slotName += QFormatStr(" Arr[%1]").arg(arrayLen);
      else
        slotName += QFormatStr("[%1]").arg(arrayIdx);

      if(copy)
        slotName = tr("SRC");

      // show if it's referenced by the shader - regardless of empty or not
      bool show = used;

      // it's bound, but not referenced, and we have "show disabled"
      show = show || (m_ShowUnused && !used && res.resourceId != ResourceId());

      // it's empty, and we have "show empty"
      show = show || (m_ShowEmpty && res.resourceId == ResourceId());

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

      InitResourcePreview(prev, show ? res : BoundResource(), show, follow, bindName, slotName);

      if(collapseArray)
        break;
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
      id = m_TexDisplay.resourceId;

    rdcarray<EventUsage> empty;

    bool input = follow.Type == FollowType::ReadOnly;

    if(id == ResourceId())
    {
      OpenResourceContextMenu(id, input, empty);
    }
    else
    {
      m_Ctx.Replay().AsyncInvoke([this, id, input](IReplayController *r) {
        rdcarray<EventUsage> usage = r->GetUsage(id);

        GUIInvoke::call(this,
                        [this, id, input, usage]() { OpenResourceContextMenu(id, input, usage); });
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
  UI_SetScale((float)expf(logScale), cursorPos.x() * ui->render->devicePixelRatio(),
              cursorPos.y() * ui->render->devicePixelRatio());

  e->accept();
}

void TextureViewer::render_mouseMove(QMouseEvent *e)
{
  if(m_Output == NULL)
    return;

  m_CurHoverPixel.setX(int((float(e->x() * ui->render->devicePixelRatio()) - m_TexDisplay.xOffset) /
                           m_TexDisplay.scale));
  m_CurHoverPixel.setY(int((float(e->y() * ui->render->devicePixelRatio()) - m_TexDisplay.yOffset) /
                           m_TexDisplay.scale));

  if(m_TexDisplay.resourceId != ResourceId())
  {
    TextureDescription *texptr = GetCurrentTexture();

    if(texptr != NULL)
    {
      if(e->buttons() & Qt::RightButton)
      {
        ui->render->setCursor(QCursor(Qt::CrossCursor));

        m_PickedPoint = m_CurHoverPixel;

        m_PickedPoint.setX(qBound(0, m_PickedPoint.x(), (int)texptr->width - 1));
        m_PickedPoint.setY(qBound(0, m_PickedPoint.y(), (int)texptr->height - 1));

        m_Ctx.Replay().AsyncInvoke(lit("PickPixelClick"),
                                   [this](IReplayController *r) { RT_PickPixelsAndUpdate(r); });
      }
      else if(e->buttons() == Qt::NoButton)
      {
        m_Ctx.Replay().AsyncInvoke(lit("PickPixelHover"),
                                   [this](IReplayController *r) { RT_PickHoverAndUpdate(r); });
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
  TextureDescription *texptr = GetCurrentTexture();

  if(texptr == NULL)
    return;

  if(e->matches(QKeySequence::Copy))
  {
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(ui->texStatusDim->text() + lit(" | ") + ui->statusText->text());
  }

  if(!m_Ctx.IsCaptureLoaded())
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
  else if(e->key() == Qt::Key_Right && m_PickedPoint.x() < (int)texptr->width - 1)
  {
    m_PickedPoint += QPoint(increment, 0);
    nudged = true;
  }

  if(nudged)
  {
    m_PickedPoint = QPoint(qBound(0, m_PickedPoint.x(), (int)texptr->width - 1),
                           qBound(0, m_PickedPoint.y(), (int)texptr->height - 1));
    e->accept();

    m_Ctx.Replay().AsyncInvoke([this](IReplayController *r) {
      RT_PickPixelsAndUpdate(r);
      RT_UpdateAndDisplay(r);
    });

    UI_UpdateStatusText();
  }
}

float TextureViewer::CurMaxScrollX()
{
  TextureDescription *texptr = GetCurrentTexture();

  QSizeF size(1.0f, 1.0f);

  if(texptr != NULL)
    size = QSizeF(texptr->width, texptr->height);

  return realRenderWidth() - size.width() * m_TexDisplay.scale;
}

float TextureViewer::CurMaxScrollY()
{
  TextureDescription *texptr = GetCurrentTexture();

  QSizeF size(1.0f, 1.0f);

  if(texptr != NULL)
    size = QSizeF(texptr->width, texptr->height);

  return realRenderHeight() - size.height() * m_TexDisplay.scale;
}

QPoint TextureViewer::getScrollPosition()
{
  return QPoint((int)m_TexDisplay.xOffset, m_TexDisplay.yOffset);
}

void TextureViewer::setScrollPosition(const QPoint &pos)
{
  m_TexDisplay.xOffset = qMax(CurMaxScrollX(), (float)pos.x());
  m_TexDisplay.yOffset = qMax(CurMaxScrollY(), (float)pos.y());

  m_TexDisplay.xOffset = qMin(0.0f, m_TexDisplay.xOffset);
  m_TexDisplay.yOffset = qMin(0.0f, m_TexDisplay.yOffset);

  if(ScrollUpdateScrollbars)
  {
    ScrollUpdateScrollbars = false;

    if(ui->renderHScroll->isEnabled())
      ui->renderHScroll->setValue(qBound(0, -int(m_TexDisplay.xOffset), ui->renderHScroll->maximum()));

    if(ui->renderVScroll->isEnabled())
      ui->renderVScroll->setValue(qBound(0, -int(m_TexDisplay.yOffset), ui->renderVScroll->maximum()));

    ScrollUpdateScrollbars = true;
  }

  INVOKE_MEMFN(RT_UpdateAndDisplay);
}

void TextureViewer::UI_CalcScrollbars()
{
  TextureDescription *texptr = GetCurrentTexture();

  QSizeF size(1.0f, 1.0f);

  if(texptr != NULL)
  {
    size = QSizeF(texptr->width, texptr->height);
  }

  if((int)floor(size.width() * m_TexDisplay.scale) <= realRenderWidth())
  {
    ui->renderHScroll->setEnabled(false);
  }
  else
  {
    ui->renderHScroll->setEnabled(true);

    ui->renderHScroll->setMaximum((int)ceil(size.width() * m_TexDisplay.scale - realRenderWidth()));
    ui->renderHScroll->setPageStep(qMax(1, ui->renderHScroll->maximum() / 6));
    ui->renderHScroll->setSingleStep(int(m_TexDisplay.scale));
  }

  if((int)floor(size.height() * m_TexDisplay.scale) <= realRenderHeight())
  {
    ui->renderVScroll->setEnabled(false);
  }
  else
  {
    ui->renderVScroll->setEnabled(true);

    ui->renderVScroll->setMaximum((int)ceil(size.height() * m_TexDisplay.scale - realRenderHeight()));
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
  ICaptureContext *ctx = &m_Ctx;

  // while a capture is loaded, pass NULL into the widget
  if(!m_Ctx.IsCaptureLoaded())
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

  updateBackgroundColors();

  QObject::connect(ui->render, &CustomPaintWidget::clicked, this, &TextureViewer::render_mouseClick);
  QObject::connect(ui->render, &CustomPaintWidget::mouseMove, this, &TextureViewer::render_mouseMove);
  QObject::connect(ui->render, &CustomPaintWidget::mouseWheel, this,
                   &TextureViewer::render_mouseWheel);
  QObject::connect(ui->render, &CustomPaintWidget::resize, this, &TextureViewer::render_resize);
  QObject::connect(ui->render, &CustomPaintWidget::keyPress, this, &TextureViewer::render_keyPress);

  QObject::connect(ui->pixelContext, &CustomPaintWidget::keyPress, this,
                   &TextureViewer::render_keyPress);
}

void TextureViewer::updateBackgroundColors()
{
  if(backCol.isValid())
  {
    ui->render->setColours(backCol, backCol);
    ui->pixelContext->setColours(backCol, backCol);
  }
  else
  {
    ui->render->setColours(Formatter::DarkCheckerColor(), Formatter::LightCheckerColor());
    ui->pixelContext->setColours(Formatter::DarkCheckerColor(), Formatter::LightCheckerColor());
  }
}

void TextureViewer::OnCaptureLoaded()
{
  Reset();

  WindowingData renderData = m_Ctx.CreateWindowingData(ui->render);
  WindowingData contextData = m_Ctx.CreateWindowingData(ui->pixelContext);

  ui->saveTex->setEnabled(true);
  ui->locationGoto->setEnabled(true);
  ui->viewTexBuffer->setEnabled(true);

  if(m_Ctx.CurPipelineState().IsCaptureD3D11())
  {
    ui->pixelHistory->setEnabled(true);
    ui->pixelHistory->setToolTip(QString());
  }
  else
  {
    ui->pixelHistory->setEnabled(false);
    ui->pixelHistory->setToolTip(tr("Pixel History not implemented on this API"));
  }

  if(m_Ctx.CurPipelineState().IsCaptureD3D11())
  {
    ui->debugPixelContext->setEnabled(true);
    ui->debugPixelContext->setToolTip(QString());
  }
  else
  {
    ui->debugPixelContext->setEnabled(false);
    ui->debugPixelContext->setToolTip(tr("Shader Debugging not implemented on this API"));
  }

  TextureListItemModel *model = (TextureListItemModel *)ui->textureList->model();

  model->reset(TextureListItemModel::String, QString());

  m_TexDisplay.backgroundColor =
      backCol.isValid() ? FloatVector(backCol.redF(), backCol.greenF(), backCol.blueF(), 1.0f)
                        : FloatVector();

  m_Ctx.Replay().BlockInvoke([renderData, contextData, this](IReplayController *r) {
    m_Output = r->CreateOutput(renderData, ReplayOutputType::Texture);

    m_Output->SetPixelContext(contextData);

    ui->render->setOutput(m_Output);
    ui->pixelContext->setOutput(m_Output);

    RT_UpdateAndDisplay(r);

    GUIInvoke::call(this, [this]() { OnEventChanged(m_Ctx.CurEvent()); });
  });

  m_Watcher = new QFileSystemWatcher({configFilePath(QString())}, this);

  QObject::connect(m_Watcher, &QFileSystemWatcher::fileChanged, this,
                   &TextureViewer::customShaderModified);
  QObject::connect(m_Watcher, &QFileSystemWatcher::directoryChanged, this,
                   &TextureViewer::customShaderModified);

  m_NeedCustomReload = true;
}

void TextureViewer::Reset()
{
  m_CachedTexture = NULL;

  m_PickedPoint.setX(-1);
  m_PickedPoint.setY(-1);

  memset(&m_TexDisplay, 0, sizeof(m_TexDisplay));
  m_TexDisplay.backgroundColor =
      backCol.isValid() ? FloatVector(backCol.redF(), backCol.greenF(), backCol.blueF(), 1.0f)
                        : FloatVector();

  m_Output = NULL;

  m_TextureSettings.clear();

  m_PrevSize = QSizeF();
  m_HighWaterStatusLength = 0;

  ui->rangeHistogram->setRange(0.0f, 1.0f);

  ui->textureListFilter->setCurrentIndex(0);

  ui->renderHScroll->setEnabled(false);
  ui->renderVScroll->setEnabled(false);

  ui->pixelHistory->setEnabled(false);
  ui->pixelHistory->setToolTip(QString());
  ui->debugPixelContext->setEnabled(false);
  ui->debugPixelContext->setToolTip(QString());

  ui->statusText->setText(QString());
  ui->renderContainer->setWindowTitle(tr("Current"));
  ui->mipLevel->clear();
  ui->sliceFace->clear();

  UI_SetScale(1.0f);

  ui->channels->setCurrentIndex(0);
  ui->overlay->setCurrentIndex(0);

  {
    QPalette Pal(palette());

    Pal.setColor(QPalette::Background, Qt::black);

    ui->pickSwatch->setAutoFillBackground(true);
    ui->pickSwatch->setPalette(Pal);
  }

  ui->customShader->clear();

  UI_RecreatePanels();

  ui->inputThumbs->clearThumbs();
  ui->outputThumbs->clearThumbs();

  UI_UpdateTextureDetails();
  UI_UpdateChannels();
}

void TextureViewer::OnCaptureClosed()
{
  Reset();

  refreshTextureList();

  delete m_Watcher;
  m_Watcher = NULL;

  ToolWindowManagerArea *textureTabs = ui->dockarea->areaOf(ui->renderContainer);
  while(textureTabs->count() > 1)
    textureTabs->removeTab(1);

  m_LockedTabs.clear();

  ui->customShader->clear();
  m_CustomShaders.clear();

  ui->saveTex->setEnabled(false);
  ui->locationGoto->setEnabled(false);
  ui->viewTexBuffer->setEnabled(false);

  UI_UpdateChannels();
}

void TextureViewer::OnEventChanged(uint32_t eventId)
{
  UI_UpdateCachedTexture();

  TextureDescription *CurrentTexture = GetCurrentTexture();

  if(!currentTextureIsLocked() ||
     (CurrentTexture != NULL && m_TexDisplay.resourceId != CurrentTexture->resourceId))
    UI_OnTextureSelectionChanged(true);

  if(m_Output == NULL)
    return;

  UI_CreateThumbnails();

  UI_UpdateTextureDetails();
  refreshTextureList();

  // iterate over locked tabs, and update the name if it's changed
  for(QWidget *w : m_LockedTabs.values())
  {
    ResourceId id = w->property("id").value<ResourceId>();
    w->setWindowTitle(m_Ctx.GetResourceName(id));
  }

  rdcarray<BoundResource> RTs = Following::GetOutputTargets(m_Ctx);
  BoundResource Depth = Following::GetDepthTarget(m_Ctx);

  int outIndex = 0;
  int inIndex = 0;

  bool copy = false, clear = false, compute = false;
  Following::GetDrawContext(m_Ctx, copy, clear, compute);

  for(int rt = 0; rt < RTs.count(); rt++)
  {
    ResourcePreview *prev;

    if(outIndex < ui->outputThumbs->thumbs().size())
      prev = ui->outputThumbs->thumbs()[outIndex];
    else
      prev = UI_CreateThumbnail(ui->outputThumbs);

    outIndex++;

    Following follow(FollowType::OutputColour, ShaderStage::Pixel, rt, 0);
    QString bindName = (copy || clear) ? tr("Destination") : QString();
    QString slotName = (copy || clear)
                           ? tr("DST")
                           : QString(m_Ctx.CurPipelineState().OutputAbbrev() + QString::number(rt));

    InitResourcePreview(prev, RTs[rt], false, follow, bindName, slotName);
  }

  // depth
  {
    ResourcePreview *prev;

    if(outIndex < ui->outputThumbs->thumbs().size())
      prev = ui->outputThumbs->thumbs()[outIndex];
    else
      prev = UI_CreateThumbnail(ui->outputThumbs);

    outIndex++;

    Following follow(FollowType::OutputDepth, ShaderStage::Pixel, 0, 0);

    InitResourcePreview(prev, Depth, false, follow, QString(), tr("DS"));
  }

  ShaderStage stages[] = {ShaderStage::Vertex, ShaderStage::Hull, ShaderStage::Domain,
                          ShaderStage::Geometry, ShaderStage::Pixel};

  int count = 5;

  if(compute)
  {
    stages[0] = ShaderStage::Compute;
    count = 1;
  }

  const rdcarray<ShaderResource> empty;

  // display resources used for all stages
  for(int i = 0; i < count; i++)
  {
    ShaderStage stage = stages[i];

    rdcarray<BoundResourceArray> RWs = Following::GetReadWriteResources(m_Ctx, stage);
    rdcarray<BoundResourceArray> ROs = Following::GetReadOnlyResources(m_Ctx, stage);

    const ShaderReflection *details = Following::GetReflection(m_Ctx, stage);
    const ShaderBindpointMapping &mapping = Following::GetMapping(m_Ctx, stage);

    InitStageResourcePreviews(stage, details != NULL ? details->readWriteResources : empty,
                              mapping.readWriteResources, RWs, ui->outputThumbs, outIndex, copy,
                              true);

    InitStageResourcePreviews(stage, details != NULL ? details->readOnlyResources : empty,
                              mapping.readOnlyResources, ROs, ui->inputThumbs, inIndex, copy, false);
  }

  // hide others
  const QVector<ResourcePreview *> &outThumbs = ui->outputThumbs->thumbs();

  for(; outIndex < outThumbs.size(); outIndex++)
  {
    ResourcePreview *prev = outThumbs[outIndex];
    prev->setResourceName(QString());
    prev->setActive(false);
    prev->setSelected(false);
  }

  ui->outputThumbs->refreshLayout();

  const QVector<ResourcePreview *> &inThumbs = ui->inputThumbs->thumbs();

  for(; inIndex < inThumbs.size(); inIndex++)
  {
    ResourcePreview *prev = inThumbs[inIndex];
    prev->setResourceName(QString());
    prev->setActive(false);
    prev->setSelected(false);
  }

  ui->inputThumbs->refreshLayout();

  INVOKE_MEMFN(RT_UpdateAndDisplay);

  if(ui->autoFit->isChecked())
    AutoFitRange();
}

QVariant TextureViewer::persistData()
{
  QVariantMap state = ui->dockarea->saveState();

  state[lit("backCol")] = backCol;
  state[lit("checker")] = !backCol.isValid();

  return state;
}

void TextureViewer::setPersistData(const QVariant &persistData)
{
  QVariantMap state = persistData.toMap();

  backCol = state[lit("backCol")].value<QColor>();
  bool checker = state[lit("checker")].value<bool>();

  if(checker)
    backCol = QColor();

  ui->backcolorPick->setChecked(!checker);
  ui->checkerBack->setChecked(checker);

  m_TexDisplay.backgroundColor =
      checker ? FloatVector() : FloatVector(backCol.redF(), backCol.greenF(), backCol.blueF(), 1.0f);

  ToolWindowManagerArea *textureTabs = ui->dockarea->areaOf(ui->renderContainer);
  while(textureTabs->count() > 1)
    textureTabs->removeTab(1);

  m_LockedTabs.clear();

  updateBackgroundColors();

  ui->dockarea->restoreState(state);

  SetupTextureTabs();
}

float TextureViewer::GetFitScale()
{
  TextureDescription *texptr = GetCurrentTexture();

  if(texptr == NULL)
    return 1.0f;

  float xscale = (float)realRenderWidth() / (float)texptr->width;
  float yscale = (float)realRenderHeight() / (float)texptr->height;

  return qMin(xscale, yscale);
}

int TextureViewer::realRenderWidth() const
{
  return ui->render->width() * ui->render->devicePixelRatio();
}

int TextureViewer::realRenderHeight() const
{
  return ui->render->height() * ui->render->devicePixelRatio();
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
  ui->zoomOption->setCurrentText(QString::number(ceil(zoom * 100)) + lit("%"));
}

float TextureViewer::getCurrentZoomValue()
{
  if(ui->fitToWindow->isChecked())
    return m_TexDisplay.scale;

  QString zoomText = ui->zoomOption->currentText().replace(QLatin1Char('%'), QLatin1Char(' '));

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
    ui->zoomOption->setCurrentText(QString());
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
  m_TexDisplay.overlay = DebugOverlay::NoOverlay;

  if(ui->overlay->currentIndex() > 0)
    m_TexDisplay.overlay = (DebugOverlay)ui->overlay->currentIndex();

#define ANALYTICS_OVERLAY(name) \
  case DebugOverlay::name: ANALYTIC_SET(TextureOverlays.name, true); break;

  switch(m_TexDisplay.overlay)
  {
    ANALYTICS_OVERLAY(Drawcall);
    ANALYTICS_OVERLAY(Wireframe);
    ANALYTICS_OVERLAY(Depth);
    ANALYTICS_OVERLAY(Stencil);
    ANALYTICS_OVERLAY(BackfaceCull);
    ANALYTICS_OVERLAY(ViewportScissor);
    ANALYTICS_OVERLAY(NaN);
    ANALYTICS_OVERLAY(Clipping);
    ANALYTICS_OVERLAY(ClearBeforePass);
    ANALYTICS_OVERLAY(ClearBeforeDraw);
    ANALYTICS_OVERLAY(QuadOverdrawPass);
    ANALYTICS_OVERLAY(QuadOverdrawDraw);
    ANALYTICS_OVERLAY(TriangleSizePass);
    ANALYTICS_OVERLAY(TriangleSizeDraw);
    default: break;
  }

#undef ANALYTICS_OVERLAY

  INVOKE_MEMFN(RT_UpdateAndDisplay);
  if(m_Output != NULL && m_PickedPoint.x() >= 0 && m_PickedPoint.y() >= 0)
  {
    INVOKE_MEMFN(RT_PickPixelsAndUpdate);
  }
}

void TextureViewer::channelsWidget_mouseClicked(QMouseEvent *event)
{
  RDToolButton *s = qobject_cast<RDToolButton *>(QObject::sender());

  if(event->button() == Qt::RightButton && s)
  {
    bool checkd = false;

    RDToolButton *butts[] = {ui->channelRed, ui->channelGreen, ui->channelBlue, ui->channelAlpha};

    for(RDToolButton *b : butts)
    {
      if(b->isChecked() && b != s)
        checkd = true;
      if(!b->isChecked() && b == s)
        checkd = true;
    }

    ui->channelRed->setChecked(!checkd);
    ui->channelGreen->setChecked(!checkd);
    ui->channelBlue->setChecked(!checkd);
    ui->channelAlpha->setChecked(!checkd);
    s->setChecked(checkd);
  }
}

void TextureViewer::range_rangeUpdated()
{
  m_TexDisplay.rangeMin = ui->rangeHistogram->blackPoint();
  m_TexDisplay.rangeMax = ui->rangeHistogram->whitePoint();

  ui->rangeBlack->setText(Formatter::Format(m_TexDisplay.rangeMin));
  ui->rangeWhite->setText(Formatter::Format(m_TexDisplay.rangeMax));

  if(m_NoRangePaint)
    return;

  INVOKE_MEMFN(RT_UpdateAndDisplay);

  if(m_Output == NULL)
  {
    ui->render->update();
    ui->pixelContext->update();
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
  ui->autoFit->setChecked(false);
}

void TextureViewer::on_autoFit_mouseClicked(QMouseEvent *e)
{
  if(e->buttons() & Qt::RightButton)
    ui->autoFit->setChecked(!ui->autoFit->isChecked());
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
  // no capture loaded or buffer/empty texture currently being viewed - don't autofit
  if(!m_Ctx.IsCaptureLoaded() || GetCurrentTexture() == NULL || m_Output == NULL)
    return;

  m_Ctx.Replay().AsyncInvoke([this](IReplayController *r) {
    PixelValue min, max;
    rdctie(min, max) = m_Output->GetMinMax();

    {
      float minval = FLT_MAX;
      float maxval = -FLT_MAX;

      bool changeRange = false;

      ResourceFormat fmt = GetCurrentTexture()->format;

      if(m_TexDisplay.customShaderId != ResourceId())
      {
        fmt.compType = CompType::Float;
      }

      for(int i = 0; i < 4; i++)
      {
        if(fmt.compType == CompType::UInt)
        {
          min.floatValue[i] = min.uintValue[i];
          max.floatValue[i] = max.uintValue[i];
        }
        else if(fmt.compType == CompType::SInt)
        {
          min.floatValue[i] = min.intValue[i];
          max.floatValue[i] = max.intValue[i];
        }
      }

      if(m_TexDisplay.red)
      {
        minval = qMin(minval, min.floatValue[0]);
        maxval = qMax(maxval, max.floatValue[0]);
        changeRange = true;
      }
      if(m_TexDisplay.green && fmt.compCount > 1)
      {
        minval = qMin(minval, min.floatValue[1]);
        maxval = qMax(maxval, max.floatValue[1]);
        changeRange = true;
      }
      if(m_TexDisplay.blue && fmt.compCount > 2)
      {
        minval = qMin(minval, min.floatValue[2]);
        maxval = qMax(maxval, max.floatValue[2]);
        changeRange = true;
      }
      if(m_TexDisplay.alpha && fmt.compCount > 3)
      {
        minval = qMin(minval, min.floatValue[3]);
        maxval = qMax(maxval, max.floatValue[3]);
        changeRange = true;
      }

      if(changeRange)
      {
        GUIInvoke::call(this, [this, minval, maxval]() {
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
    return;

  col = col.toRgb();
  m_TexDisplay.backgroundColor = FloatVector(col.redF(), col.greenF(), col.blueF(), 1.0f);

  backCol = col;

  updateBackgroundColors();

  ui->backcolorPick->setChecked(true);
  ui->checkerBack->setChecked(false);

  INVOKE_MEMFN(RT_UpdateAndDisplay);

  if(m_Output == NULL)
  {
    ui->render->update();
    ui->pixelContext->update();
  }
}

void TextureViewer::on_checkerBack_clicked()
{
  ui->checkerBack->setChecked(true);
  ui->backcolorPick->setChecked(false);

  backCol = QColor();

  m_TexDisplay.backgroundColor = FloatVector();

  updateBackgroundColors();

  INVOKE_MEMFN(RT_UpdateAndDisplay);

  if(m_Output == NULL)
  {
    ui->render->update();
    ui->pixelContext->update();
  }
}

void TextureViewer::on_mipLevel_currentIndexChanged(int index)
{
  TextureDescription *texptr = GetCurrentTexture();
  if(texptr == NULL)
    return;

  TextureDescription &tex = *texptr;

  uint32_t prevSlice = m_TexDisplay.sliceFace;

  if(tex.mips > 1)
  {
    m_TexDisplay.mip = (uint32_t)qMax(0, index);
    m_TexDisplay.sampleIdx = 0;
  }
  else
  {
    m_TexDisplay.mip = 0;
    m_TexDisplay.sampleIdx = (uint32_t)qMax(0, index);
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
      ui->sliceFace->addItem(tr("Slice %1").arg(i));

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
  TextureDescription *texptr = GetCurrentTexture();
  if(texptr == NULL)
    return;

  TextureDescription &tex = *texptr;
  m_TexDisplay.sliceFace = (uint32_t)qMax(0, index);

  if(tex.depth > 1)
    m_TexDisplay.sliceFace = (uint32_t)(qMax(0, index) << (int)m_TexDisplay.mip);

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
  TextureDescription *texptr = GetCurrentTexture();

  if(texptr)
  {
    QPoint p = m_PickedPoint;

    uint32_t mipHeight = qMax(1U, texptr->height >> (int)m_TexDisplay.mip);

    if(m_Ctx.APIProps().pipelineType == GraphicsAPI::OpenGL)
      p.setY((int)(mipHeight - 1) - p.y());
    if(m_TexDisplay.flipY)
      p.setY((int)(mipHeight - 1) - p.y());

    m_Goto->show(ui->render, p);
  }
}

void TextureViewer::on_viewTexBuffer_clicked()
{
  TextureDescription *texptr = GetCurrentTexture();

  if(texptr)
  {
    uint32_t slice = m_TexDisplay.sliceFace;

    if(texptr->msSamp > 1)
    {
      slice *= texptr->msSamp;
      if(m_TexDisplay.sampleIdx < texptr->msSamp)
        slice += m_TexDisplay.sampleIdx;
    }

    IBufferViewer *viewer =
        m_Ctx.ViewTextureAsBuffer(slice, m_TexDisplay.mip, texptr->resourceId,
                                  FormatElement::GenerateTextureBufferFormat(*texptr));

    m_Ctx.AddDockWindow(viewer->Widget(), DockReference::AddTo, this);
  }
}

void TextureViewer::on_resourceDetails_clicked()
{
  TextureDescription *texptr = GetCurrentTexture();

  if(texptr)
  {
    if(!m_Ctx.HasResourceInspector())
      m_Ctx.ShowResourceInspector();

    m_Ctx.GetResourceInspector()->Inspect(texptr->resourceId);

    ToolWindowManager::raiseToolWindow(m_Ctx.GetResourceInspector()->Widget());
  }
}

void TextureViewer::on_saveTex_clicked()
{
  TextureDescription *texptr = GetCurrentTexture();

  if(!texptr || !m_Output)
    return;

  // overwrite save params with current texture display settings
  m_SaveConfig.resourceId = m_TexDisplay.resourceId;
  m_SaveConfig.typeHint = m_TexDisplay.typeHint;
  m_SaveConfig.slice.sliceIndex = (int)m_TexDisplay.sliceFace;
  m_SaveConfig.mip = (int)m_TexDisplay.mip;

  if(texptr->depth > 1)
    m_SaveConfig.slice.sliceIndex = (int)m_TexDisplay.sliceFace >> (int)m_TexDisplay.mip;

  m_SaveConfig.channelExtract = -1;
  if(m_TexDisplay.red && !m_TexDisplay.green && !m_TexDisplay.blue && !m_TexDisplay.alpha)
    m_SaveConfig.channelExtract = 0;
  if(!m_TexDisplay.red && m_TexDisplay.green && !m_TexDisplay.blue && !m_TexDisplay.alpha)
    m_SaveConfig.channelExtract = 1;
  if(!m_TexDisplay.red && !m_TexDisplay.green && m_TexDisplay.blue && !m_TexDisplay.alpha)
    m_SaveConfig.channelExtract = 2;
  if(!m_TexDisplay.red && !m_TexDisplay.green && !m_TexDisplay.blue && m_TexDisplay.alpha)
    m_SaveConfig.channelExtract = 3;

  m_SaveConfig.comp.blackPoint = m_TexDisplay.rangeMin;
  m_SaveConfig.comp.whitePoint = m_TexDisplay.rangeMax;
  m_SaveConfig.alphaCol = m_TexDisplay.backgroundColor;

  if(m_TexDisplay.customShaderId != ResourceId())
  {
    ResourceId id;
    m_Ctx.Replay().BlockInvoke(
        [this, &id](IReplayController *r) { id = m_Output->GetCustomShaderTexID(); });

    if(id != ResourceId())
      m_SaveConfig.resourceId = id;
  }

  ResourceId overlayTexID;
  if(m_TexDisplay.overlay != DebugOverlay::NoOverlay)
  {
    m_Ctx.Replay().BlockInvoke([this, &overlayTexID](IReplayController *r) {
      overlayTexID = m_Output->GetDebugOverlayTexID();
    });
  }
  const bool hasSelectedOverlay = (m_TexDisplay.overlay != DebugOverlay::NoOverlay);
  const bool hasOverlay = (hasSelectedOverlay && overlayTexID != ResourceId());
  TextureSaveDialog saveDialog(*texptr, hasOverlay, m_SaveConfig, this);
  int res = RDDialog::show(&saveDialog);

  m_SaveConfig = saveDialog.config();

  if(saveDialog.saveOverlayInstead())
  {
    m_SaveConfig.resourceId = overlayTexID;

    if(m_TexDisplay.overlay == DebugOverlay::QuadOverdrawDraw ||
       m_TexDisplay.overlay == DebugOverlay::QuadOverdrawPass ||
       m_TexDisplay.overlay == DebugOverlay::TriangleSizeDraw ||
       m_TexDisplay.overlay == DebugOverlay::TriangleSizePass)
    {
      m_SaveConfig.comp.blackPoint = 0.0f;
      m_SaveConfig.comp.whitePoint = 255.0f;
    }
  }

  if(res)
  {
    ANALYTIC_SET(Export.Texture, true);

    bool ret = false;
    QString fn = saveDialog.filename();

    m_Ctx.Replay().BlockInvoke([this, &ret, fn](IReplayController *r) {
      ret = r->SaveTexture(m_SaveConfig, fn.toUtf8().data());
    });

    if(!ret)
    {
      RDDialog::critical(
          NULL, tr("Error saving texture"),
          tr("Error saving texture %1.\n\nCheck diagnostic log in Help menu for more details.").arg(fn));
    }
  }
}

void TextureViewer::on_debugPixelContext_clicked()
{
  if(m_PickedPoint.x() < 0 || m_PickedPoint.y() < 0)
    return;

  int x = m_PickedPoint.x() >> (int)m_TexDisplay.mip;
  int y = m_PickedPoint.y() >> (int)m_TexDisplay.mip;

  TextureDescription *texptr = GetCurrentTexture();

  uint32_t mipHeight = qMax(1U, texptr->height >> (int)m_TexDisplay.mip);

  if(m_TexDisplay.flipY)
    y = (int)(mipHeight - 1) - y;

  bool done = false;
  ShaderDebugTrace *trace = NULL;

  m_Ctx.Replay().AsyncInvoke([this, &trace, &done, x, y](IReplayController *r) {
    trace = r->DebugPixel((uint32_t)x, (uint32_t)y, m_TexDisplay.sampleIdx, ~0U);

    if(trace->states.isEmpty())
    {
      r->FreeTrace(trace);
      trace = NULL;
    }

    done = true;

  });

  QString debugContext = tr("Pixel %1,%2").arg(x).arg(y);

  // wait a short while before displaying the progress dialog (which won't show if we're already
  // done by the time we reach it)
  for(int i = 0; !done && i < 100; i++)
    QThread::msleep(5);

  ShowProgressDialog(this, tr("Debugging %1").arg(debugContext), [&done]() { return done; });

  // if we couldn't debug the pixel on this event, open up a pixel history
  if(!trace)
  {
    on_pixelHistory_clicked();
    return;
  }

  const ShaderReflection *shaderDetails =
      m_Ctx.CurPipelineState().GetShaderReflection(ShaderStage::Pixel);
  const ShaderBindpointMapping &bindMapping =
      m_Ctx.CurPipelineState().GetBindpointMapping(ShaderStage::Pixel);
  ResourceId pipeline = m_Ctx.CurPipelineState().GetGraphicsPipelineObject();

  // viewer takes ownership of the trace
  IShaderViewer *s = m_Ctx.DebugShader(&bindMapping, shaderDetails, pipeline, trace, debugContext);

  m_Ctx.AddDockWindow(s->Widget(), DockReference::AddTo, this);
}

void TextureViewer::on_pixelHistory_clicked()
{
  TextureDescription *texptr = GetCurrentTexture();

  if(!texptr || !m_Output)
    return;

  ANALYTIC_SET(UIFeatures.PixelHistory, true);

  int x = m_PickedPoint.x() >> (int)m_TexDisplay.mip;
  int y = m_PickedPoint.y() >> (int)m_TexDisplay.mip;

  uint32_t mipHeight = qMax(1U, texptr->height >> (int)m_TexDisplay.mip);

  if(m_TexDisplay.flipY)
    y = (int)(mipHeight - 1) - y;

  IPixelHistoryView *hist = m_Ctx.ViewPixelHistory(texptr->resourceId, x, y, m_TexDisplay);

  m_Ctx.AddDockWindow(hist->Widget(), DockReference::TransientPopupArea, this, 0.3f);

  // we use this pointer to ensure that the history viewer is still visible (and hasn't been closed)
  // by the time we want to set the results.
  QPointer<QWidget> histWidget = hist->Widget();

  // add a short delay so that controls repainting after a new panel appears can get at the
  // render thread before we insert the long blocking pixel history task
  LambdaThread *thread = new LambdaThread([this, texptr, x, y, hist, histWidget]() {
    QThread::msleep(150);
    m_Ctx.Replay().AsyncInvoke([this, texptr, x, y, hist, histWidget](IReplayController *r) {
      rdcarray<PixelModification> history =
          r->PixelHistory(texptr->resourceId, (uint32_t)x, (int32_t)y, m_TexDisplay.sliceFace,
                          m_TexDisplay.mip, m_TexDisplay.sampleIdx, m_TexDisplay.typeHint);

      GUIInvoke::call(this, [hist, histWidget, history] {
        if(histWidget)
          hist->SetHistory(history);
      });
    });
  });
  thread->selfDelete(true);
  thread->start();
}

void TextureViewer::on_texListShow_clicked()
{
  if(ui->textureListFrame->isVisible())
  {
    ui->dockarea->moveToolWindow(ui->textureListFrame, ToolWindowManager::NoArea);
  }
  else
  {
    ui->textureListFilter->setCurrentText(QString());
    ui->dockarea->moveToolWindow(
        ui->textureListFrame,
        ToolWindowManager::AreaReference(ToolWindowManager::LeftOf,
                                         ui->dockarea->areaOf(ui->renderContainer), 0.2f));
    ui->dockarea->setToolWindowProperties(ui->textureListFrame, ToolWindowManager::HideOnClose);
  }
}

void TextureViewer::on_cancelTextureListFilter_clicked()
{
  ui->textureListFilter->setCurrentText(QString());
}

void TextureViewer::on_textureListFilter_editTextChanged(const QString &text)
{
  TextureListItemModel *model = (TextureListItemModel *)ui->textureList->model();

  if(model == NULL)
    return;

  model->reset(TextureListItemModel::String, text);
}

void TextureViewer::on_textureListFilter_currentIndexChanged(int index)
{
  refreshTextureList();
}

void TextureViewer::refreshTextureList()
{
  TextureListItemModel *model = (TextureListItemModel *)ui->textureList->model();

  if(model == NULL)
    return;

  if(ui->textureListFilter->currentIndex() == 1)
    model->reset(TextureListItemModel::Textures, QString());
  else if(ui->textureListFilter->currentIndex() == 2)
    model->reset(TextureListItemModel::RenderTargets, QString());
  else
    model->reset(TextureListItemModel::String, ui->textureListFilter->currentText());
}

void TextureViewer::on_textureList_clicked(const QModelIndex &index)
{
  ResourceId id = index.model()->data(index, Qt::UserRole).value<ResourceId>();
  ViewTexture(id, false);
}

bool TextureViewer::canCompileCustomShader(ShaderEncoding encoding)
{
  rdcarray<ShaderEncoding> supported = m_Ctx.CustomShaderEncodings();

  // if it's directly supported, we can trivially compile it
  if(supported.contains(encoding))
    return true;

  // otherwise see if we have a tool that can compile it for us

  for(const ShaderProcessingTool &tool : m_Ctx.Config().ShaderProcessors)
  {
    // if this tool transforms from the encoding to one we support, we can compile a shader of this
    // encoding
    if(tool.input == encoding && supported.contains(tool.output))
      return true;
  }

  // all options exhausted, we can't compile this
  return false;
}

void TextureViewer::reloadCustomShaders(const QString &filter)
{
  if(!m_Ctx.IsCaptureLoaded())
    return;

  if(filter.isEmpty())
  {
    QString prevtext = ui->customShader->currentText();

    QList<ResourceId> shaders = m_CustomShaders.values();

    m_Ctx.Replay().AsyncInvoke([shaders](IReplayController *r) {
      for(ResourceId s : shaders)
        r->FreeCustomShader(s);
    });

    ui->customShader->clear();
    m_CustomShaders.clear();

    ui->customShader->setCurrentText(prevtext);
  }
  else
  {
    QString fn = QFileInfo(filter).fileName();
    QString key = fn.toUpper();

    if(m_CustomShaders.contains(key))
    {
      if(m_CustomShadersBusy.contains(key))
        return;

      ResourceId freed = m_CustomShaders[key];
      m_Ctx.Replay().AsyncInvoke([freed](IReplayController *r) { r->FreeCustomShader(freed); });

      m_CustomShaders.remove(key);

      QString text = ui->customShader->currentText();

      for(int i = 0; i < ui->customShader->count(); i++)
      {
        if(ui->customShader->itemText(i).compare(fn, Qt::CaseInsensitive) == 0)
        {
          ui->customShader->removeItem(i);
          break;
        }
      }

      ui->customShader->setCurrentText(text);
    }
  }

  QStringList filters;
  for(auto it = encodingExtensions.begin(); it != encodingExtensions.end(); ++it)
  {
    if(!canCompileCustomShader(it.value()))
      continue;

    filters.push_back(lit("*.") + it.key());
  }

  QStringList files =
      QDir(configFilePath(QString()))
          .entryList(filters, QDir::Files | QDir::NoDotAndDotDot, QDir::Name | QDir::IgnoreCase);

  QStringList watchedFiles = m_Watcher->files();
  if(!watchedFiles.isEmpty())
    m_Watcher->removePaths(watchedFiles);

  for(const QString &f : files)
  {
    QFileInfo fileInfo(f);
    QString fn = fileInfo.fileName();
    QString key = fn.toUpper();
    ShaderEncoding encoding = encodingExtensions[fileInfo.completeSuffix()];

    if(!filter.isEmpty() && filter.toUpper() != key)
      continue;

    m_Watcher->addPath(configFilePath(f));

    if(!m_CustomShaders.contains(key) && !m_CustomShadersBusy.contains(key))
    {
      QFile fileHandle(configFilePath(f));
      if(fileHandle.open(QFile::ReadOnly | QFile::Text))
      {
        QTextStream stream(&fileHandle);
        QString source = stream.readAll();

        bytebuf shaderBytes(source.toUtf8());

        rdcarray<ShaderEncoding> supported = m_Ctx.TargetShaderEncodings();

        rdcstr errors;

        // we don't accept this encoding directly, need to compile
        if(!supported.contains(encoding))
        {
          for(const ShaderProcessingTool &tool : m_Ctx.Config().ShaderProcessors)
          {
            // pick the first tool that can convert to an accepted format
            if(tool.input == encoding && supported.contains(tool.output))
            {
              ShaderToolOutput out =
                  tool.CompileShader(this, source, "main", ShaderStage::Pixel, "");

              errors = out.log;

              if(m_CustomShaderEditor.contains(key))
                m_CustomShaderEditor[key]->ShowErrors(errors);

              if(out.result.isEmpty())
                break;

              encoding = tool.output;
              shaderBytes = out.result;
              break;
            }
          }
        }

        // if the encoding still isn't supported, all tools failed. Bail out now
        if(!supported.contains(encoding))
        {
          if(m_CustomShaderEditor.contains(key))
            m_CustomShaderEditor[key]->ShowErrors(errors);

          m_CustomShaders[key] = ResourceId();

          QString prevtext = ui->customShader->currentText();
          ui->customShader->addItem(fn);
          ui->customShader->setCurrentText(prevtext);
          continue;
        }

        ANALYTIC_SET(UIFeatures.CustomTextureVisualise, true);

        fileHandle.close();

        m_CustomShaders[key] = ResourceId();
        m_CustomShadersBusy.push_back(key);
        m_Ctx.Replay().AsyncInvoke(
            [this, fn, key, shaderBytes, encoding, errors](IReplayController *r) {
              rdcstr buildErrors;

              ResourceId id;
              rdctie(id, buildErrors) = r->BuildCustomShader(
                  "main", encoding, shaderBytes, ShaderCompileFlags(), ShaderStage::Pixel);

              if(!errors.empty())
                buildErrors = errors + "\n\n" + buildErrors;

              if(m_CustomShaderEditor.contains(key))
              {
                IShaderViewer *editor = m_CustomShaderEditor[key];
                GUIInvoke::call(editor->Widget(),
                                [editor, buildErrors]() { editor->ShowErrors(buildErrors); });
              }

              GUIInvoke::call(this, [this, fn, key, id]() {
                QString prevtext = ui->customShader->currentText();
                ui->customShader->addItem(fn);
                ui->customShader->setCurrentText(prevtext);

                m_CustomShaders[key] = id;
                m_CustomShadersBusy.removeOne(key);

                UI_UpdateChannels();
              });
            });
      }
    }
  }
}

void TextureViewer::on_customCreate_clicked()
{
  QString filename = ui->customShader->currentText();

  if(filename.isEmpty())
  {
    RDDialog::critical(this, tr("Error Creating Shader"),
                       tr("No shader name specified.\nEnter a new name in the textbox"));
    return;
  }

  if(m_CustomShaders.contains(filename.toUpper()))
  {
    RDDialog::critical(this, tr("Error Creating Shader"),
                       tr("Selected shader already exists.\nEnter a new name in the textbox."));
    ui->customShader->setCurrentText(QString());
    UI_UpdateChannels();
    return;
  }

  ShaderEncoding enc = encodingExtensions[QFileInfo(filename).completeSuffix()];

  if(enc == ShaderEncoding::Unknown)
  {
    QString extString;

    for(auto it = encodingExtensions.begin(); it != encodingExtensions.end(); ++it)
    {
      if(!canCompileCustomShader(it.value()))
        continue;

      if(!extString.isEmpty())
        extString += lit(", ");
      extString += it.key();
    }

    RDDialog::critical(this, tr("Error Creating Shader"),
                       tr("No file extension specified, unknown shading language.\n"
                          "Filename must contain one of: %1")
                           .arg(extString));
    return;
  }

  QString path = configFilePath(filename);

  QString src;

  if(enc == ShaderEncoding::HLSL)
  {
    src =
        lit("float4 main(float4 pos : SV_Position, float4 uv : TEXCOORD0) : SV_Target0\n"
            "{\n"
            "    return float4(0,0,0,1);\n"
            "}\n");
  }
  else if(enc == ShaderEncoding::GLSL)
  {
    src =
        lit("#version 420 core\n\n"
            "layout (location = 0) in vec2 uv;\n\n"
            "layout (location = 0) out vec4 color_out;\n\n"
            "void main()\n"
            "{\n"
            "    color_out = vec4(0,0,0,1);\n"
            "}\n");
  }
  else if(enc == ShaderEncoding::SPIRVAsm)
  {
    src = lit("; SPIR-V");
  }
  else
  {
    src = tr("Unknown format - no template available");
  }

  QFile fileHandle(path);
  if(fileHandle.open(QFile::WriteOnly | QIODevice::Truncate | QIODevice::Text))
  {
    fileHandle.write(src.toUtf8());
    fileHandle.close();
  }
  else
  {
    RDDialog::critical(
        this, tr("Cannot create shader"),
        tr("Couldn't create file for shader %1\n%2").arg(filename).arg(fileHandle.errorString()));
  }

  // auto-open edit window
  on_customEdit_clicked();

  reloadCustomShaders(filename);
}

void TextureViewer::on_customEdit_clicked()
{
  QString filename = ui->customShader->currentText();
  QString key = filename.toUpper();

  if(filename.isEmpty())
  {
    RDDialog::critical(this, tr("Error Editing Shader"),
                       tr("No shader selected.\nSelect a custom shader from the drop-down"));
    return;
  }

  QString path = configFilePath(filename);

  QString src;

  QFile fileHandle(path);
  if(fileHandle.open(QFile::ReadOnly | QFile::Text))
  {
    QTextStream stream(&fileHandle);
    src = stream.readAll();
    fileHandle.close();
  }
  else
  {
    RDDialog::critical(
        this, tr("Cannot open shader"),
        tr("Couldn't open file for shader %1\n%2").arg(filename).arg(fileHandle.errorString()));
    return;
  }

  rdcstrpairs files;
  files.push_back({filename, src});

  QPointer<TextureViewer> thisPointer(this);

  IShaderViewer *s = m_Ctx.EditShader(
      true, ShaderStage::Fragment, lit("main"), files,
      encodingExtensions[QFileInfo(filename).completeSuffix()], ShaderCompileFlags(),
      // Save Callback
      [thisPointer, key, filename, path](ICaptureContext *ctx, IShaderViewer *viewer,
                                         ShaderEncoding, ShaderCompileFlags, rdcstr, bytebuf bytes) {
        {
          // don't trigger a full refresh
          if(thisPointer)
            thisPointer->m_CustomShaderWriteTime = thisPointer->m_CustomShaderTimer.elapsed();

          QFile fileHandle(path);
          if(fileHandle.open(QFile::WriteOnly | QIODevice::Truncate | QIODevice::Text))
          {
            fileHandle.write(QByteArray(bytes));
            fileHandle.close();

            // watcher doesn't trigger on internal modifications
            if(thisPointer)
              thisPointer->reloadCustomShaders(filename);
          }
          else
          {
            if(thisPointer)
            {
              RDDialog::critical(
                  thisPointer, tr("Cannot save shader"),
                  tr("Couldn't save file for shader %1\n%2").arg(filename).arg(fileHandle.errorString()));
            }
          }
        }
      },

      [thisPointer, key](ICaptureContext *ctx) {
        if(thisPointer)
          thisPointer->m_CustomShaderEditor.remove(key);
      });

  m_CustomShaderEditor[key] = s;

  m_Ctx.AddDockWindow(s->Widget(), DockReference::AddTo, this);
}

void TextureViewer::on_customDelete_clicked()
{
  QString shaderName = ui->customShader->currentText();

  if(shaderName.isEmpty())
  {
    RDDialog::critical(this, tr("Error Deleting Shader"),
                       tr("No shader selected.\nSelect a custom shader from the drop-down"));
    return;
  }

  if(!m_CustomShaders.contains(shaderName.toUpper()))
  {
    RDDialog::critical(
        this, tr("Error Deleting Shader"),
        tr("Selected shader doesn't exist.\nSelect a custom shader from the drop-down"));
    return;
  }

  QMessageBox::StandardButton res =
      RDDialog::question(this, tr("Deleting Custom Shader"),
                         tr("Really delete %1?").arg(shaderName), RDDialog::YesNoCancel);

  if(res == QMessageBox::Yes)
  {
    QString path = configFilePath(shaderName);
    if(!QFileInfo::exists(path))
    {
      RDDialog::critical(
          this, tr("Error Deleting Shader"),
          tr("Shader file %1 can't be found.\nSelect a custom shader from the drop-down")
              .arg(shaderName));
      return;
    }

    if(!QFile::remove(path))
    {
      RDDialog::critical(this, tr("Error Deleting Shader"),
                         tr("Error deleting shader %1 from disk").arg(shaderName));
      return;
    }

    ui->customShader->setCurrentText(QString());
    UI_UpdateChannels();
  }
}

void TextureViewer::customShaderModified(const QString &path)
{
  static bool recurse = false;

  if(recurse)
    return;

  // if we just wrote a shader less than 100ms ago, don't refresh - this will have been handled
  // internally at a finer granularity than 'all shaders'
  if(m_CustomShaderWriteTime > m_CustomShaderTimer.elapsed() - 100)
    return;

  recurse = true;

  // allow time for modifications to finish
  QThread::msleep(15);

  reloadCustomShaders(QString());

  recurse = false;
}
