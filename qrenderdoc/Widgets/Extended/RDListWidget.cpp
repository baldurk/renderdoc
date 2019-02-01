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

#include "RDListWidget.h"
#include <QApplication>
#include <QClipboard>
#include <QMouseEvent>
#include "Code/Interface/QRDInterface.h"

RDListWidget::RDListWidget(QWidget *parent) : QListWidget(parent)
{
}

RDListWidget::~RDListWidget()
{
}

void RDListWidget::mousePressEvent(QMouseEvent *event)
{
  QListWidget::mousePressEvent(event);
  emit(mouseClicked(event));
}

void RDListWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
  QListWidget::mouseDoubleClickEvent(event);
  emit(mouseDoubleClicked(event));
}

void RDListWidget::keyPressEvent(QKeyEvent *event)
{
  if(!m_customCopyPaste && event->matches(QKeySequence::Copy))
  {
    QList<QListWidgetItem *> items = selectedItems();

    std::sort(items.begin(), items.end(),
              [this](QListWidgetItem *a, QListWidgetItem *b) { return row(a) < row(b); });

    QString clipboardText;
    for(QListWidgetItem *i : items)
    {
      clipboardText += i->text() + lit("\n");
    }

    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(clipboardText.trimmed());
  }
  else
  {
    QListWidget::keyPressEvent(event);
  }

  emit(keyPress(event));
}
