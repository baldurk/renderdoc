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

#include "ResourcePreview.h"
#include <QMouseEvent>
#include "ui_ResourcePreview.h"

ResourcePreview::ResourcePreview(CaptureContext &c, IReplayOutput *output, QWidget *parent)
    : QFrame(parent), ui(new Ui::ResourcePreview)
{
  ui->setupUi(this);

  CustomPaintWidget *thumb = new CustomPaintWidget(&c, this);
  thumb->setOutput(output);
  thumb->setObjectName(ui->thumbnail->objectName());
  thumb->setSizePolicy(ui->thumbnail->sizePolicy());
  thumb->setMinimumSize(QSize(0, 0));

  delete ui->thumbnail;
  ui->thumbnail = thumb;
  ui->gridLayout->addWidget(ui->thumbnail, 0, 0, 1, 2);

  QPalette Pal(ui->slotLabel->palette());

  QWidget tmp;

  Pal.setColor(ui->slotLabel->foregroundRole(), tmp.palette().color(QPalette::Foreground));
  Pal.setColor(ui->slotLabel->backgroundRole(), tmp.palette().color(QPalette::Dark));

  ui->slotLabel->setAutoFillBackground(true);
  ui->slotLabel->setPalette(Pal);
  ui->descriptionLabel->setAutoFillBackground(true);
  ui->descriptionLabel->setPalette(Pal);

  QObject::connect(ui->thumbnail, &CustomPaintWidget::clicked, this, &ResourcePreview::clickEvent);
  QObject::connect(ui->slotLabel, &RDLabel::clicked, this, &ResourcePreview::clickEvent);
  QObject::connect(ui->descriptionLabel, &RDLabel::clicked, this, &ResourcePreview::clickEvent);

  QObject::connect(ui->thumbnail, &CustomPaintWidget::doubleClicked, this,
                   &ResourcePreview::doubleClickEvent);
  QObject::connect(ui->slotLabel, &RDLabel::doubleClicked, this, &ResourcePreview::doubleClickEvent);
  QObject::connect(ui->descriptionLabel, &RDLabel::doubleClicked, this,
                   &ResourcePreview::doubleClickEvent);
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
  QPalette Pal(palette());

  Pal.setColor(QPalette::Foreground, sel ? Qt::red : Qt::black);

  setPalette(Pal);
}

WId ResourcePreview::thumbWinId()
{
  return ui->thumbnail->winId();
}
