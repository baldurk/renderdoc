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

#include "ResourcePreview.h"
#include <QMouseEvent>
#include "Code/QRDUtils.h"
#include "ui_ResourcePreview.h"

ResourcePreview::ResourcePreview(ICaptureContext &c, IReplayOutput *output, QWidget *parent)
    : QFrame(parent), ui(new Ui::ResourcePreview)
{
  ui->setupUi(this);

  ui->thumbnail->SetContext(c);
  ui->thumbnail->SetOutput(output);

  Initialise();
}

ResourcePreview::ResourcePreview(bool, QWidget *parent)
    : QFrame(parent), ui(new Ui::ResourcePreview)
{
  ui->setupUi(this);

  delete ui->thumbnail;
  ui->thumbnail = NULL;
  m_ManualThumbnail = new RDLabel();
  m_ManualThumbnail->setGeometry(QRect(0, 0, 16, 16));
  m_ManualThumbnail->setScaledContents(true);
  m_ManualThumbnail->setSizePolicy(
      QSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding));

  ui->gridLayout->addWidget(m_ManualThumbnail, 0, 0, 1, 2);

  Initialise();
}

void ResourcePreview::Initialise()
{
  setBackgroundRole(QPalette::Background);
  setForegroundRole(QPalette::Foreground);

  QPalette Pal(palette());

  Pal.setColor(QPalette::Highlight, QColor(Qt::red));

  setPalette(Pal);

  setSelected(false);

  ui->slotLabel->setPalette(palette());
  ui->slotLabel->setBackgroundRole(QPalette::Dark);
  ui->slotLabel->setForegroundRole(QPalette::Foreground);
  ui->slotLabel->setAutoFillBackground(true);
  ui->slotLabel->setFont(Formatter::PreferredFont());
  ui->descriptionLabel->setPalette(palette());
  ui->descriptionLabel->setAutoFillBackground(true);
  ui->descriptionLabel->setBackgroundRole(QPalette::Dark);
  ui->descriptionLabel->setForegroundRole(QPalette::Foreground);
  ui->descriptionLabel->setFont(Formatter::PreferredFont());

  if(ui->thumbnail)
    QObject::connect(ui->thumbnail, &CustomPaintWidget::clicked, this, &ResourcePreview::clickEvent);
  else if(m_ManualThumbnail)
    QObject::connect(m_ManualThumbnail, &RDLabel::clicked, this, &ResourcePreview::clickEvent);
  QObject::connect(ui->slotLabel, &RDLabel::clicked, this, &ResourcePreview::clickEvent);
  QObject::connect(ui->descriptionLabel, &RDLabel::clicked, this, &ResourcePreview::clickEvent);

  if(ui->thumbnail)
    QObject::connect(ui->thumbnail, &CustomPaintWidget::doubleClicked, this,
                     &ResourcePreview::doubleClickEvent);
  else if(m_ManualThumbnail)
    QObject::connect(m_ManualThumbnail, &RDLabel::doubleClicked, this,
                     &ResourcePreview::doubleClickEvent);
  QObject::connect(ui->slotLabel, &RDLabel::doubleClicked, this, &ResourcePreview::doubleClickEvent);
  QObject::connect(ui->descriptionLabel, &RDLabel::doubleClicked, this,
                   &ResourcePreview::doubleClickEvent);

  if(m_ManualThumbnail)
    QObject::connect(m_ManualThumbnail, &RDLabel::resized, [this]() { emit resized(this); });
}

ResourcePreview::~ResourcePreview()
{
  delete ui;
}

void ResourcePreview::clickEvent(QMouseEvent *e)
{
  emit clicked(e);
}

void ResourcePreview::doubleClickEvent(QMouseEvent *e)
{
  emit doubleClicked(e);
}

void ResourcePreview::setSlotName(const QString &n)
{
  ui->slotLabel->setText(n);
}

void ResourcePreview::setResourceName(const QString &n)
{
  ui->descriptionLabel->setText(n);
}

void ResourcePreview::setSize(QSize s)
{
  setFixedWidth(s.width());
  setFixedHeight(s.height());
  setMinimumSize(s);
  setMaximumSize(s);
}

void ResourcePreview::setSelected(bool sel)
{
  m_Selected = sel;

  setForegroundRole(sel ? QPalette::Highlight : QPalette::Foreground);
}

WindowingData ResourcePreview::GetWidgetWindowingData()
{
  if(ui->thumbnail)
    return ui->thumbnail->GetWidgetWindowingData();
  return {};
}

QSize ResourcePreview::GetThumbSize()
{
  if(ui->thumbnail)
    return ui->thumbnail->size();

  if(m_ManualThumbnail)
    return m_ManualThumbnail->size();

  return {};
}

void ResourcePreview::UpdateThumb(QSize s, const bytebuf &imgData)
{
  if(!m_ManualThumbnail)
    return;

  QImage thumb(imgData.data(), s.width(), s.height(), s.width() * 3, QImage::Format_RGB888);
  m_ManualThumbnail->setPixmap(QPixmap::fromImage(thumb));
}

void ResourcePreview::resizeEvent(QResizeEvent *e)
{
  emit resized(this);
  QFrame::resizeEvent(e);
}
