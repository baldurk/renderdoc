#if defined(__linux__)
#define RENDERDOC_WINDOWING_XLIB 1
#define RENDERDOC_WINDOWING_XCB 1
#endif

#include "TextureViewer.h"
#include "Code/Core.h"
#include "FlowLayout.h"
#include "ui_TextureViewer.h"

#if defined(__linux__)
#include <GL/glx.h>
#include <X11/Xlib.h>
#include <xcb/xcb.h>
#include <QX11Info>
#endif

struct Formatter
{
  static QString Format(float f) { return QString::number(f); }
  static QString Format(double d) { return QString::number(d); }
  static QString Format(uint32_t u) { return QString::number(u); }
  static QString Format(uint16_t u) { return QString::number(u); }
  static QString Format(int32_t i) { return QString::number(i); }
};

TextureViewer::TextureViewer(Core *core, QWidget *parent)
    : QFrame(parent), ui(new Ui::TextureViewer), m_Core(core)
{
  ui->setupUi(this);

  m_Core->AddLogViewer(this);

  ui->render->SetOutput(m_Core, NULL);
  ui->pixelContext->SetOutput(m_Core, NULL);

  m_PickedPoint = QPoint(-1, -1);

  QWidget *renderContainer = ui->renderContainer;

  QObject::connect(ui->render, &CustomPaintWidget::clicked, this, &TextureViewer::render_mouseClick);
  QObject::connect(ui->render, &CustomPaintWidget::mouseMove, this, &TextureViewer::render_mouseMove);
  QObject::connect(ui->render, &CustomPaintWidget::mouseWheel, this,
                   &TextureViewer::render_mouseWheel);
  QObject::connect(ui->render, &CustomPaintWidget::resize, this, &TextureViewer::render_resize);

  QObject::connect(ui->zoomOption->lineEdit(), &QLineEdit::returnPressed, this,
                   &TextureViewer::on_zoomOption_returnPressed);

  ui->dockarea->addToolWindow(ui->renderContainer, ToolWindowManager::EmptySpace);
  ui->dockarea->setToolWindowProperties(renderContainer, ToolWindowManager::DisallowUserDocking |
                                                             ToolWindowManager::HideCloseButton |
                                                             ToolWindowManager::DisableDraggableTab);

  ToolWindowManager::AreaReference ref(ToolWindowManager::AddTo,
                                       ui->dockarea->areaOf(renderContainer));

  /*
  QWidget *lockedTabTest = new QWidget(this);
  lockedTabTest->setWindowTitle(tr("Locked Tab #1"));

  ui->dockarea->addToolWindow(lockedTabTest, ref);
  ui->dockarea->setToolWindowProperties(lockedTabTest, ToolWindowManager::DisallowUserDocking |
  ToolWindowManager::HideCloseButton);

  lockedTabTest = new QWidget(this);
  lockedTabTest->setWindowTitle(tr("Locked Tab #2"));

  ui->dockarea->addToolWindow(lockedTabTest, ref);
  ui->dockarea->setToolWindowProperties(lockedTabTest, ToolWindowManager::DisallowUserDocking |
  ToolWindowManager::HideCloseButton);

  lockedTabTest = new QWidget(this);
  lockedTabTest->setWindowTitle(tr("Locked Tab #3"));

  ui->dockarea->addToolWindow(lockedTabTest, ref);
  ui->dockarea->setToolWindowProperties(lockedTabTest, ToolWindowManager::DisallowUserDocking |
  ToolWindowManager::HideCloseButton);

  lockedTabTest = new QWidget(this);
  lockedTabTest->setWindowTitle(tr("Locked Tab #4"));

  ui->dockarea->addToolWindow(lockedTabTest, ref);
  ui->dockarea->setToolWindowProperties(lockedTabTest, ToolWindowManager::DisallowUserDocking |
  ToolWindowManager::HideCloseButton);*/

  ui->dockarea->addToolWindow(ui->resourceThumbs, ToolWindowManager::AreaReference(
                                                      ToolWindowManager::RightOf,
                                                      ui->dockarea->areaOf(renderContainer), 0.25f));
  ui->dockarea->setToolWindowProperties(ui->resourceThumbs, ToolWindowManager::HideCloseButton);

  ui->dockarea->addToolWindow(
      ui->targetThumbs, ToolWindowManager::AreaReference(ToolWindowManager::AddTo,
                                                         ui->dockarea->areaOf(ui->resourceThumbs)));
  ui->dockarea->setToolWindowProperties(ui->targetThumbs, ToolWindowManager::HideCloseButton);

  // need to add a way to make this less than 50% programmatically
  ui->dockarea->addToolWindow(
      ui->pixelContextLayout,
      ToolWindowManager::AreaReference(ToolWindowManager::BottomOf,
                                       ui->dockarea->areaOf(ui->targetThumbs), 0.25f));
  ui->dockarea->setToolWindowProperties(ui->pixelContextLayout, ToolWindowManager::HideCloseButton);

  ui->dockarea->setAllowFloatingWindow(false);
  ui->dockarea->setRubberBandLineWidth(50);

  renderContainer->setWindowTitle(tr("OM RenderTarget 0 - GBuffer Colour"));
  ui->pixelContextLayout->setWindowTitle(tr("Pixel Context"));
  ui->targetThumbs->setWindowTitle(tr("OM Targets"));
  ui->resourceThumbs->setWindowTitle(tr("PS Resources"));

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
  u->pixelcontextgrid->setAlignment(u->pushButton, Qt::AlignCenter);
  u->pixelcontextgrid->setAlignment(u->pushButton_2, Qt::AlignCenter);

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

  ui->zoomOption->addItems({"10%", "25%", "50%", "75%", "100%", "200%", "400%", "800%"});

  ui->zoomOption->setCurrentText("");
  ui->fitToWindow->toggle();

  UI_UpdateTextureDetails();
}

TextureViewer::~TextureViewer()
{
  m_Core->RemoveLogViewer(this);
  delete ui;
}

void TextureViewer::RT_FetchCurrentPixel(uint32_t x, uint32_t y, PixelValue &pickValue,
                                         PixelValue &realValue)
{
  // FetchTexture tex = CurrentTexture;

  // if (tex == null) return;

  // if (m_TexDisplay.FlipY)
  // y = (tex.height - 1) - y;

  m_Output->PickPixel(m_TexDisplay.texid, true, x, y, m_TexDisplay.sliceFace, m_TexDisplay.mip,
                      m_TexDisplay.sampleIdx, &pickValue);

  if(m_TexDisplay.CustomShader != ResourceId())
    m_Output->PickPixel(m_TexDisplay.texid, false, x, y, m_TexDisplay.sliceFace, m_TexDisplay.mip,
                        m_TexDisplay.sampleIdx, &realValue);
}

void TextureViewer::RT_PickPixelsAndUpdate()
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

void TextureViewer::RT_PickHoverAndUpdate()
{
  PixelValue pickValue, realValue;

  uint32_t x = (uint32_t)m_CurHoverPixel.x();
  uint32_t y = (uint32_t)m_CurHoverPixel.y();

  RT_FetchCurrentPixel(x, y, pickValue, realValue);

  m_CurHoverValue = pickValue;

  GUIInvoke::call([this]() { UI_UpdateStatusText(); });
}

void TextureViewer::RT_UpdateAndDisplay()
{
  if(m_Output != NULL)
    m_Output->SetTextureDisplay(m_TexDisplay);

  GUIInvoke::call([this]() { ui->render->update(); });
}

void TextureViewer::UI_UpdateStatusText()
{
  FetchTexture *texptr = m_Core->GetTexture(m_TexDisplay.texid);
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

  uint mipWidth = qMax(1U, tex.width >> (int)m_TexDisplay.mip);
  uint mipHeight = qMax(1U, tex.height >> (int)m_TexDisplay.mip);

  if(m_Core->APIProps().pipelineType == eGraphicsAPI_OpenGL)
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

  QString statusText = "Hover - " + hoverCoords;

  uint32_t hoverX = (uint32_t)m_CurHoverPixel.x();
  uint32_t hoverY = (uint32_t)m_CurHoverPixel.y();

  if(hoverX > tex.width || hoverY > tex.height || hoverX < 0 || hoverY < 0)
    statusText = "Hover - [" + hoverCoords + "]";

  if(m_PickedPoint.x() >= 0)
  {
    x = m_PickedPoint.x() >> (int)m_TexDisplay.mip;
    y = m_PickedPoint.y() >> (int)m_TexDisplay.mip;
    if(m_Core->APIProps().pipelineType == eGraphicsAPI_OpenGL)
      y = (int)(mipHeight - 1) - y;
    if(m_TexDisplay.FlipY)
      y = (int)(mipHeight - 1) - y;

    y = qMax(0, y);

    statusText += " - Right click - " + QString("%1, %2: ").arg(x, 4).arg(y, 4);

    PixelValue val = m_CurPixelValue;

    if(m_TexDisplay.CustomShader != ResourceId())
    {
      statusText += Formatter::Format(val.value_f[0]) + ", " + Formatter::Format(val.value_f[1]) +
                    ", " + Formatter::Format(val.value_f[2]) + ", " +
                    Formatter::Format(val.value_f[3]);

      val = m_CurRealValue;

      statusText += " (Real: ";
    }

    if(dsv)
    {
      statusText += "Depth ";
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

      statusText += QString(", Stencil %1 / 0x%2").arg(stencil).arg(stencil, 0, 16);
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
    statusText += " - Right click to pick a pixel";

    m_Core->Renderer()->AsyncInvoke([this](IReplayRenderer *) {
      if(m_Output != NULL)
        m_Output->DisablePixelContext();
    });

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

  FetchTexture *texptr = m_Core->GetTexture(m_TexDisplay.texid);
  if(texptr == NULL)
  {
    ui->texStatusDim->setText(status);
    return;
  }

  FetchTexture &current = *texptr;

#if 0
  ResourceId followID = m_Following.GetResourceId(m_Core);

  {
    bool found = false;

    string name = "";

    foreach(var t in m_Core.CurTextures)
    {
      if (t.ID == followID)
      {
        name = t.name;
        found = true;
      }
    }

    foreach(var b in m_Core.CurBuffers)
    {
      if (b.ID == followID)
      {
        name = b.name;
        found = true;
      }
    }

    if (followID == ResourceId.Null)
    {
      m_PreviewPanel.Text = "Unbound";
    }
    else if (found)
    {
      switch (m_Following.Type)
      {
        case FollowType.OutputColour:
          m_PreviewPanel.Text = string.Format("Cur Output {0} - {1}", m_Following.index, name);
          break;
        case FollowType.OutputDepth:
          m_PreviewPanel.Text = string.Format("Cur Depth Output - {0}", name);
          break;
        case FollowType.ReadWrite:
          m_PreviewPanel.Text = string.Format("Cur RW Output - {0}", name);
          break;
        case FollowType.ReadOnly:
          m_PreviewPanel.Text = string.Format("Cur Input {0} - {1}", m_Following.index, name);
          break;
      }
    }
    else
    {
      switch (m_Following.Type)
      {
        case FollowType.OutputColour:
          m_PreviewPanel.Text = string.Format("Cur Output {0}", m_Following.index);
          break;
        case FollowType.OutputDepth:
          m_PreviewPanel.Text = string.Format("Cur Depth Output");
          break;
        case FollowType.ReadWrite:
          m_PreviewPanel.Text = string.Format("Cur RW Output");
          break;
        case FollowType.ReadOnly:
          m_PreviewPanel.Text = string.Format("Cur Input {0}", m_Following.index);
          break;
      }
    }
  }
#endif

  status = QString(current.name.elems) + " - ";

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

  status += " - " + QString(current.format.strname.elems);

  if(current.format.compType != m_TexDisplay.typeHint && m_TexDisplay.typeHint != eCompType_None)
  {
    status += " Viewed as TODO";    // m_TexDisplay.typeHint.Str();
  }

  ui->texStatusDim->setText(status);
}

void TextureViewer::UI_OnTextureSelectionChanged(bool newdraw)
{
  UI_UpdateFittedScale();
  UI_UpdateTextureDetails();
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
  m_CurHoverPixel.setX(int(((float)e->x() - m_TexDisplay.offx) / m_TexDisplay.scale));
  m_CurHoverPixel.setY(int(((float)e->y() - m_TexDisplay.offy) / m_TexDisplay.scale));

  if(m_TexDisplay.texid != ResourceId())
  {
    FetchTexture *texptr = m_Core->GetTexture(m_TexDisplay.texid);

    if(texptr != NULL)
    {
      if(e->buttons() & Qt::RightButton)
      {
        ui->render->setCursor(QCursor(Qt::CrossCursor));

        m_PickedPoint = m_CurHoverPixel;

        m_PickedPoint.setX(qBound(0, m_PickedPoint.x(), (int)texptr->width - 1));
        m_PickedPoint.setY(qBound(0, m_PickedPoint.y(), (int)texptr->height - 1));

        m_Core->Renderer()->AsyncInvoke([this](IReplayRenderer *) {
          if(m_Output != NULL)
            RT_PickPixelsAndUpdate();
        });
      }
      else if(e->buttons() == Qt::NoButton)
      {
        m_Core->Renderer()->AsyncInvoke([this](IReplayRenderer *) {
          if(m_Output != NULL)
            RT_PickHoverAndUpdate();
        });
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
    ui->render->setCursor(QCursor(Qt::ArrowCursor));
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

  m_Core->Renderer()->AsyncInvoke([this](IReplayRenderer *) { RT_UpdateAndDisplay(); });
}

float TextureViewer::CurMaxScrollX()
{
  FetchTexture *texptr = m_Core->GetTexture(m_TexDisplay.texid);

  QSizeF size(1.0f, 1.0f);

  if(texptr != NULL)
    size = QSizeF(texptr->width, texptr->height);

  return ui->render->width() - size.width() * m_TexDisplay.scale;
}

float TextureViewer::CurMaxScrollY()
{
  FetchTexture *texptr = m_Core->GetTexture(m_TexDisplay.texid);

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
    if(ui->renderHScroll->isEnabled())
      ui->renderHScroll->setValue(qBound(0, int(m_TexDisplay.offx), ui->renderHScroll->maximum()));

    if(ui->renderVScroll->isEnabled())
      ui->renderVScroll->setValue(qBound(0, int(m_TexDisplay.offy), ui->renderVScroll->maximum()));
  }

  m_Core->Renderer()->AsyncInvoke([this](IReplayRenderer *) { RT_UpdateAndDisplay(); });
}

void TextureViewer::UI_CalcScrollbars()
{
  FetchTexture *texptr = m_Core->GetTexture(m_TexDisplay.texid);

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
  }
}

void TextureViewer::on_renderHScroll_valueChanged(int position)
{
  ScrollUpdateScrollbars = false;

  {
    float delta = (float)position / (float)ui->renderHScroll->maximum();
    setScrollPosition(QPoint((int)(CurMaxScrollX() * delta), getScrollPosition().y()));
  }

  ScrollUpdateScrollbars = true;
}

void TextureViewer::on_renderVScroll_valueChanged(int position)
{
  ScrollUpdateScrollbars = false;

  {
    float delta = (float)position / (float)ui->renderVScroll->maximum();
    setScrollPosition(QPoint(getScrollPosition().x(), (int)(CurMaxScrollY() * delta)));
  }

  ScrollUpdateScrollbars = true;
}

void TextureViewer::OnLogfileLoaded()
{
#if defined(WIN32)

  WindowingSystem system = eWindowingSystem_Win32;
  HWND wnd = (HWND)ui->render->winId();

#elif defined(__linux__)

  XCBWindowData xcb = {
      QX11Info::connection(), (xcb_window_t)ui->render->winId(),
  };

  XlibWindowData xlib = {QX11Info::display(), (Drawable)ui->render->winId()};

  rdctype::array<WindowingSystem> systems;
  m_Core->Renderer()->BlockInvoke(
      [&systems](IReplayRenderer *r) { r->GetSupportedWindowSystems(&systems); });

  WindowingSystem system = eWindowingSystem_Unknown;
  void *wnd = NULL;

  // prefer XCB
  for(int32_t i = 0; i < systems.count; i++)
  {
    if(systems[i] == eWindowingSystem_XCB)
    {
      system = eWindowingSystem_XCB;
      wnd = &xcb;
      break;
    }
  }

  for(int32_t i = 0; wnd == NULL && i < systems.count; i++)
  {
    if(systems[i] == eWindowingSystem_Xlib)
    {
      system = eWindowingSystem_Xlib;
      wnd = &xlib;
      break;
    }
  }

#else

#error "Unknown platform"

#endif

  m_Core->Renderer()->BlockInvoke([system, wnd, this](IReplayRenderer *r) {
    m_Output = r->CreateOutput(system, wnd, eOutputType_TexDisplay);
    ui->render->SetOutput(m_Core, m_Output);

    OutputConfig c = {eOutputType_TexDisplay};
    m_Output->SetOutputConfig(c);
  });
}

void TextureViewer::OnLogfileClosed()
{
  m_Output = NULL;
  ui->render->SetOutput(m_Core, NULL);

  UI_UpdateTextureDetails();
}

void TextureViewer::OnEventSelected(uint32_t eventID)
{
  TextureDisplay &d = m_TexDisplay;

  if(m_Core->APIProps().pipelineType == eGraphicsAPI_D3D11)
  {
    d.texid = m_Core->CurD3D11PipelineState.m_OM.RenderTargets[0].Resource;
  }
  else if(m_Core->APIProps().pipelineType == eGraphicsAPI_OpenGL)
  {
    d.texid = m_Core->CurGLPipelineState.m_FB.m_DrawFBO.Color[0].Obj;
  }
  else
  {
    const VulkanPipelineState &pipe = m_Core->CurVulkanPipelineState;
    if(pipe.Pass.renderpass.colorAttachments.count > 0)
      d.texid = pipe.Pass.framebuffer.attachments[pipe.Pass.renderpass.colorAttachments[0]].img;

    if(d.texid == ResourceId())
    {
      const FetchDrawcall *draw = m_Core->CurDrawcall();
      if(draw)
        d.texid = draw->copyDestination;
    }
  }

  d.mip = 0;
  d.sampleIdx = ~0U;
  d.overlay = eTexOverlay_None;
  d.CustomShader = ResourceId();
  d.HDRMul = -1.0f;
  d.linearDisplayAsGamma = true;
  d.FlipY = false;
  d.rangemin = 0.0f;
  d.rangemax = 1.0f;
  d.scale = 1.0f;
  d.offx = 0.0f;
  d.offy = 0.0f;
  d.sliceFace = 0;
  d.rawoutput = false;
  d.lightBackgroundColour = d.darkBackgroundColour = FloatVector(0.0f, 0.0f, 0.0f, 0.0f);
  d.Red = d.Green = d.Blue = true;
  d.Alpha = false;

  m_Core->Renderer()->AsyncInvoke([this](IReplayRenderer *) { RT_UpdateAndDisplay(); });

  FetchTexture *tex = m_Core->GetTexture(d.texid);

  if(tex)
    ui->renderContainer->setWindowTitle(tr(tex->name.elems));

  // if (!CurrentTextureIsLocked || (CurrentTexture != null && m_TexDisplay.texid !=
  // CurrentTexture.ID))
  UI_OnTextureSelectionChanged(true);
}

float TextureViewer::GetFitScale()
{
  FetchTexture *texptr = m_Core->GetTexture(m_TexDisplay.texid);

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

  m_Core->Renderer()->AsyncInvoke([this](IReplayRenderer *) { RT_UpdateAndDisplay(); });

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

void TextureViewer::on_zoomOption_returnPressed()
{
  UI_SetScale(getCurrentZoomValue());
}
