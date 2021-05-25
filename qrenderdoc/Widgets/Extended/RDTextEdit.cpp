/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2021 Baldur Karlsson
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

#include "RDTextEdit.h"
#include <QKeyEvent>
#include <QScrollBar>
#include "Code/QRDUtils.h"

RDTextEdit::RDTextEdit(QWidget *parent) : QTextEdit(parent)
{
}

RDTextEdit::~RDTextEdit()
{
}

void RDTextEdit::setSingleLine()
{
  if(m_singleLine)
    return;

  m_singleLine = true;

  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setLineWrapMode(QTextEdit::NoWrap);

  document()->setDocumentMargin(0);

  QFontMetrics fm(font());
  const int iconSize = style()->pixelMetric(QStyle::PM_SmallIconSize, 0, this);

  int height = qMax(fm.height() + 2, iconSize);

  QStyleOptionFrame opt;
  initStyleOption(&opt);
  QSize sz = style()->sizeFromContents(QStyle::CT_LineEdit, &opt, QSize(100, height), this);

  setFixedHeight(sz.height());

  QObject::connect(this, &QTextEdit::textChanged, [this]() {
    if(m_singleLine)
    {
      QString text = toPlainText();

      if(text.contains(QLatin1Char('\r')) || text.contains(QLatin1Char('\n')))
      {
        text.replace(lit("\r\n"), lit(" "));
        text.replace(QLatin1Char('\r'), lit(" "));
        text.replace(QLatin1Char('\n'), lit(" "));
        setPlainText(text);
      }
    }
  });
}

void RDTextEdit::setHoverTrack()
{
  setAttribute(Qt::WA_Hover);
}

void RDTextEdit::focusInEvent(QFocusEvent *e)
{
  QTextEdit::focusInEvent(e);
  emit(enter());
}

void RDTextEdit::focusOutEvent(QFocusEvent *e)
{
  QTextEdit::focusOutEvent(e);
  emit(leave());
}

void RDTextEdit::keyPressEvent(QKeyEvent *e)
{
  if(m_singleLine)
  {
    if(e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter)
    {
      emit(keyPress(e));
      e->accept();
      return;
    }
  }

  // add ctrl-end and ctrl-home shortcuts, which aren't implemented for read-only edits
  if(e->key() == Qt::Key_End && (e->modifiers() & Qt::ControlModifier))
  {
    verticalScrollBar()->setValue(verticalScrollBar()->maximum());
  }
  else if(e->key() == Qt::Key_Home && (e->modifiers() & Qt::ControlModifier))
  {
    verticalScrollBar()->setValue(verticalScrollBar()->minimum());
  }
  else
  {
    QTextEdit::keyPressEvent(e);
  }
  emit(keyPress(e));
}

void RDTextEdit::mouseMoveEvent(QMouseEvent *e)
{
  emit(mouseMoved(e));
  QTextEdit::mouseMoveEvent(e);
}

bool RDTextEdit::event(QEvent *e)
{
  if(e->type() == QEvent::HoverEnter)
  {
    emit(hoverEnter());
  }
  else if(e->type() == QEvent::HoverLeave)
  {
    emit(hoverLeave());
  }
  return QTextEdit::event(e);
}
