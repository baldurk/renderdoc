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

#include "ThumbnailStrip.h"
#include <QScrollBar>
#include "Widgets/ResourcePreview.h"
#include "ui_ThumbnailStrip.h"

ThumbnailStrip::ThumbnailStrip(QWidget *parent) : QWidget(parent), ui(new Ui::ThumbnailStrip)
{
  ui->setupUi(this);

  layout = new QVBoxLayout(ui->scrollAreaWidgetContents);
  layout->setSpacing(6);
  layout->setContentsMargins(6, 6, 6, 6);
  layout->setAlignment(Qt::AlignTop);
}

ThumbnailStrip::~ThumbnailStrip()
{
  delete layout;
  delete ui;
}

void ThumbnailStrip::addThumb(ResourcePreview *prev)
{
  layout->addWidget(prev);
  m_Thumbnails.push_back(prev);
}

void ThumbnailStrip::clearThumbs()
{
  for(ResourcePreview *p : m_Thumbnails)
  {
    layout->removeWidget(p);
    delete p;
  }

  m_Thumbnails.clear();
}

void ThumbnailStrip::resizeEvent(QResizeEvent *event)
{
  refreshLayout();
}

void ThumbnailStrip::mousePressEvent(QMouseEvent *event)
{
  emit(mouseClick(event));
}

void ThumbnailStrip::showEvent(QShowEvent *event)
{
  refreshLayout();
}

void ThumbnailStrip::refreshLayout()
{
  QRect avail = geometry();
  avail.adjust(6, 6, -6, -6);

  int numActive = 0;
  for(ResourcePreview *c : m_Thumbnails)
    if(c->isActive())
      numActive++;

  // depending on overall aspect ratio, we either lay out the strip horizontally or
  // vertically. This tries to account for whether the strip is docked along one side
  // or another of the texture viewer
  if(avail.width() > avail.height())
  {
    avail.setWidth(avail.width() + 6);    // controls implicitly have a 6 margin on the right

    int aspectWidth = (int)((float)avail.height() * 1.3f);

    ui->scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    delete layout;
    layout = new QHBoxLayout(ui->scrollAreaWidgetContents);
    for(ResourcePreview *w : m_Thumbnails)
      layout->addWidget(w);
    layout->setSpacing(6);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setAlignment(Qt::AlignTop);

    int noscrollWidth = numActive * (aspectWidth + 20);

    if(noscrollWidth <= avail.width())
    {
      ui->scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

      for(ResourcePreview *c : m_Thumbnails)
        c->setSize(QSize(aspectWidth, avail.height()));
    }
    else
    {
      ui->scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

      QScrollBar *hs = ui->scrollArea->horizontalScrollBar();

      avail.setHeight(qMax(1, avail.height() - hs->geometry().height()));

      aspectWidth = qMax(1, (int)((float)avail.height() * 1.3f));

      int totalWidth = numActive * (aspectWidth + 20);
      hs->setEnabled(totalWidth > avail.width());

      for(ResourcePreview *c : m_Thumbnails)
        c->setSize(QSize(aspectWidth, avail.height()));
    }
  }
  else
  {
    avail.setHeight(avail.height() + 6);    // controls implicitly have a 6 margin on the bottom

    int aspectHeight = (int)((float)avail.width() / 1.3f);

    ui->scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    delete layout;
    layout = new QVBoxLayout(ui->scrollAreaWidgetContents);
    for(ResourcePreview *w : m_Thumbnails)
      layout->addWidget(w);
    layout->setSpacing(6);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setAlignment(Qt::AlignTop);

    int noscrollHeight = numActive * (aspectHeight + 6);

    if(noscrollHeight <= avail.height())
    {
      ui->scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

      for(ResourcePreview *c : m_Thumbnails)
        c->setSize(QSize(avail.width(), aspectHeight));
    }
    else
    {
      ui->scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

      QScrollBar *vs = ui->scrollArea->verticalScrollBar();

      avail.setWidth(qMax(1, avail.width() - vs->geometry().width()));

      aspectHeight = qMax(1, (int)((float)avail.width() / 1.3f));

      int totalHeight = numActive * (aspectHeight + 6);
      vs->setEnabled(totalHeight > avail.height());

      for(ResourcePreview *c : m_Thumbnails)
        c->setSize(QSize(avail.width(), aspectHeight));
    }
  }
}