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
#include <QAbstractItemView>
#include <QCompleter>
#include <QKeyEvent>
#include <QScrollBar>
#include <QStringListModel>
#include "Code/QRDUtils.h"

RDTextEdit::RDTextEdit(QWidget *parent) : QTextEdit(parent)
{
  m_WordCharacters = lit("_");
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

void RDTextEdit::enableCompletion()
{
  if(m_Completer)
    return;

  m_Completer = new QCompleter(this);
  m_Completer->setWidget(this);
  m_Completer->setCompletionMode(QCompleter::PopupCompletion);
  m_Completer->setCaseSensitivity(Qt::CaseInsensitive);
  m_Completer->setWrapAround(false);

  QObject::connect(
      m_Completer, OverloadedSlot<const QString &>::of(&QCompleter::activated),
      [this](const QString &str) {
        QTextCursor cur = textCursor();

        // if we're in the middle of a word, move to the end of it
        QString text = toPlainText();
        if(cur.position() > 0 && (text[cur.position() - 1].isLetterOrNumber() ||
                                  m_WordCharacters.contains(text[cur.position() - 1])) &&
           (text[cur.position()].isLetterOrNumber() ||
            m_WordCharacters.contains(text[cur.position()])))
        {
          cur.movePosition(QTextCursor::EndOfWord);
        }

        // insert what's remaining of the word, after the prefix which is what's already there
        cur.insertText(str.right(str.length() - m_Completer->completionPrefix().length()));
        setTextCursor(cur);
      });
  m_Completer->popup()->installEventFilter(this);

  m_Completer->setCompletionRole(Qt::DisplayRole);

  m_CompletionModel = new QStringListModel(this);

  m_Completer->setModel(m_CompletionModel);
  m_Completer->setModelSorting(QCompleter::CaseInsensitivelySortedModel);
}

bool RDTextEdit::eventFilter(QObject *watched, QEvent *event)
{
  if(m_Completer && watched == m_Completer->popup() && event->type() == QEvent::Hide)
  {
    emit(completionEnd());
  }

  return QTextEdit::eventFilter(watched, event);
}

void RDTextEdit::setCompletionWordCharacters(QString chars)
{
  m_WordCharacters = chars;
}

void RDTextEdit::setCompletionStrings(QStringList list)
{
  if(m_CompletionModel)
  {
    list.sort(Qt::CaseInsensitive);
    m_CompletionModel->setStringList(list);
  }
}

bool RDTextEdit::completionInProgress()
{
  return m_Completer && m_Completer->popup()->isVisible();
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
  if(completionInProgress())
  {
    switch(e->key())
    {
      // if a completion is in progress ignore any events the completer will process
      case Qt::Key_Return:
      case Qt::Key_Enter:
      case Qt::Key_Tab:
      case Qt::Key_Backtab:
      case Qt::Key_Escape:
        e->ignore();
        return;

      // also the completer doesn't hide itself when the cursor is moved so make sure we do that
      // ourselves
      case Qt::Key_Left:
      case Qt::Key_Right:
      case Qt::Key_Home:
      case Qt::Key_End:
        m_Completer->popup()->hide();
        emit(completionEnd());
        break;
      default: break;
    }
  }

  if(m_singleLine)
  {
    if(e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter)
    {
      emit(keyPress(e));
      e->accept();
      return;
    }
  }

  bool completionShortcut = false;

  // tab triggers completion
  if(e->key() == Qt::Key_Tab)
    completionShortcut = true;

  // as does ctrl-space and ctrl-E
  if(e->modifiers() & Qt::ControlModifier)
  {
    if(e->key() == Qt::Key_E)
      completionShortcut = true;
    if(e->key() == Qt::Key_Space)
      completionShortcut = true;
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
  else if(!completionShortcut)
  {
    QTextEdit::keyPressEvent(e);
  }

  // stop completing if the character just entered is not a word-compatible character
  if(completionInProgress() && e->text().length() > 0)
  {
    QChar c = e->text()[0];
    if(c.isPrint() && !e->text()[0].isLetterOrNumber() && !m_WordCharacters.contains(e->text()[0]))
    {
      if(c.isSpace() && m_Completer->completionPrefix().trimmed() == QString())
      {
        // don't do anything if we have no prefix so far and the user enters whitespace
      }
      else
      {
        m_Completer->popup()->hide();
        emit(completionEnd());
      }
    }
  }

  emit(keyPress(e));

  // update the completion if it's in progress, or we have our shortcut (and there's no selected
  // text)
  if((completionShortcut && !textCursor().hasSelection()) || completionInProgress())
  {
    triggerCompletion();

    if(completionShortcut)
      e->accept();
  }
}

void RDTextEdit::triggerCompletion()
{
  QString text = toPlainText();

  // start at the cursor position
  int end = textCursor().position();
  int start = end;

  // glob forward through word characters
  while(end < text.length())
  {
    QChar c = text.at(end);
    if(c.isLetterOrNumber() || m_WordCharacters.contains(c))
    {
      end++;
      continue;
    }

    break;
  }

  // glob backwards through word characters too
  while(start > 0)
  {
    start--;

    QChar c = text.at(start);
    if(!c.isLetterOrNumber() && !m_WordCharacters.contains(c))
    {
      start++;
      break;
    }
  }

  // grab the current prefix to be working with
  QString prefix = text.mid(start, end - start);

  bool startedCompletion = false;

  if(!completionInProgress())
  {
    emit(completionBegin(prefix));
    startedCompletion = true;
  }

  // stop completing if text is selected or if there are no candidates
  if(m_CompletionModel->stringList().empty() || textCursor().hasSelection())
  {
    if(startedCompletion || completionInProgress())
    {
      m_Completer->popup()->hide();
      emit(completionEnd());
    }
  }
  else
  {
    // update the prefix as needed
    if(prefix != m_Completer->completionPrefix())
      m_Completer->setCompletionPrefix(prefix);

    // select the first item
    m_Completer->popup()->setCurrentIndex(m_Completer->completionModel()->index(0, 0));

    QRect r = cursorRect();
    r.setWidth(m_Completer->popup()->sizeHintForColumn(0) +
               m_Completer->popup()->verticalScrollBar()->sizeHint().width());
    m_Completer->complete(r);

    // we have to start the completion to get the list of suggestions, but if none of them matched
    // and the popup never appeared we need to end it here as we won't get another notification
    if(startedCompletion && !completionInProgress())
    {
      m_Completer->popup()->hide();
      emit(completionEnd());
    }
  }
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
