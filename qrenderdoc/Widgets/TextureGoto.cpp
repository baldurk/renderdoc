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

#include "TextureGoto.h"
#include <QApplication>
#include <QDebug>
#include <QFocusEvent>
#include <QGridLayout>
#include <QLabel>
#include "Widgets/Extended/RDDoubleSpinBox.h"

TextureGoto::TextureGoto(QWidget *parent, std::function<void(QPoint)> callback) : QDialog(parent)
{
  m_Callback = callback;

  setWindowFlags(Qt::Window | Qt::FramelessWindowHint);

  QHBoxLayout *hbox = new QHBoxLayout(this);
  hbox->setSpacing(5);
  hbox->setMargin(0);

  QFrame *frame = new QFrame(this);
  frame->setGeometry(geometry());
  frame->setFrameShadow(QFrame::Raised);
  frame->setFrameStyle(QFrame::StyledPanel);

  hbox->addWidget(frame);

  QGridLayout *gridLayout = new QGridLayout(frame);
  gridLayout->setSpacing(4);
  gridLayout->setContentsMargins(3, 3, 3, 3);
  QLabel *label = new QLabel(this);
  label->setSizePolicy(QSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum));
  label->setText(tr("Goto Location"));
  label->setAlignment(Qt::AlignCenter);

  gridLayout->addWidget(label, 0, 0, 1, 2);

  m_X = new RDDoubleSpinBox(frame);
  m_X->setSizePolicy(QSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum));
  m_X->setMinimumSize(QSize(40, 0));
  m_X->setDecimals(0);
  m_X->setSingleStep(1.0);
  m_X->setRange(0.0, 65536.0);
  m_X->setValue(10);

  gridLayout->addWidget(m_X, 1, 0, 1, 1);

  m_Y = new RDDoubleSpinBox(frame);
  m_Y->setSizePolicy(QSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum));
  m_Y->setMinimumSize(QSize(40, 0));
  m_Y->setDecimals(0);
  m_Y->setSingleStep(1.0);
  m_Y->setRange(0.0, 65536.0);
  m_Y->setValue(20);

  QObject::connect(m_X, &RDDoubleSpinBox::keyPress, this, &TextureGoto::location_keyPress);
  QObject::connect(m_Y, &RDDoubleSpinBox::keyPress, this, &TextureGoto::location_keyPress);

  gridLayout->addWidget(m_Y, 1, 1, 1, 1);

  setTabOrder(m_X, m_Y);
  setTabOrder(m_Y, m_X);
}

QPoint TextureGoto::point()
{
  return QPoint(m_X->value(), m_Y->value());
}

void TextureGoto::show(QWidget *showParent, QPoint p)
{
  m_X->setValue(p.x());
  m_Y->setValue(p.y());

  move(showParent->mapToGlobal(showParent->geometry().topLeft()) + showParent->rect().center() -
       rect().center());

  QDialog::show();

  m_Y->setFocus(Qt::TabFocusReason);
  m_X->setFocus(Qt::TabFocusReason);
}

void TextureGoto::leaveEvent(QEvent *event)
{
  QDialog::hide();
}

void TextureGoto::location_keyPress(QKeyEvent *event)
{
  if(event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
  {
    m_Callback(point());
    QDialog::hide();
  }
}
