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

#include "CommentView.h"
#include <QFontDatabase>
#include <QRegularExpression>
#include "Code/QRDUtils.h"
#include "Code/ScintillaSyntax.h"
#include "scintilla/include/SciLexer.h"
#include "scintilla/include/qt/ScintillaEdit.h"
#include "ui_CommentView.h"

static const sptr_t link_style = 100;

CommentView::CommentView(ICaptureContext &ctx, QWidget *parent)
    : QFrame(parent), ui(new Ui::CommentView), m_Ctx(ctx)
{
  ui->setupUi(this);

  m_commentsEditor = new ScintillaEdit(this);

  m_commentsEditor->styleSetFont(STYLE_DEFAULT, Formatter::FixedFont().family().toUtf8().data());
  m_commentsEditor->setTabWidth(4);

  m_commentsEditor->setWrapMode(SC_WRAP_WORD);
  m_commentsEditor->setWrapVisualFlags(SC_WRAPVISUALFLAG_MARGIN);

  m_commentsEditor->setMarginWidthN(0, 30.0);

  ConfigureSyntax(m_commentsEditor, SCLEX_NULL);

  QObject::connect(
      m_commentsEditor, &ScintillaEdit::modified,
      [this](int type, int position, int length, int, const QByteArray &, int, int, int) {
        // if there has been a change, restyle the region around the modification. We can't use just
        // word boundaries so search back to the last whitespace character - this means we will
        // restyle at most a line, likely much less.
        if(type & (SC_MOD_INSERTTEXT | SC_MOD_DELETETEXT))
        {
          int start = m_commentsEditor->wordStartPosition(position, false);
          while(!isspace(m_commentsEditor->charAt(start)) && start > 0)
            start--;
          int end = m_commentsEditor->wordEndPosition(position + length, false);
          while(!isspace(m_commentsEditor->charAt(end)) && end < m_commentsEditor->length())
            end++;

          if(start < 0)
            start = 0;
          if(end < 0 || end > m_commentsEditor->length())
            end = m_commentsEditor->length();

          Restyle(start, end);
        }

        if(m_ignoreModifications)
          return;

        if(type & (SC_MOD_INSERTTEXT | SC_MOD_DELETETEXT | SC_MOD_BEFOREINSERT | SC_MOD_BEFOREDELETE))
        {
          QString text =
              QString::fromUtf8(m_commentsEditor->getText(m_commentsEditor->textLength() + 1));
          text.remove(QLatin1Char('\r'));
          m_ignoreModifications = true;
          m_Ctx.SetNotes(lit("comments"), text);
          m_ignoreModifications = false;
        }
      });

  m_commentsEditor->styleSetHotSpot(link_style, true);
  QColor back = palette().base().color();
  QColor fore = palette().link().color();
  m_commentsEditor->styleSetBack(link_style, SCINTILLA_COLOUR(back.red(), back.green(), back.blue()));
  m_commentsEditor->styleSetFore(link_style, SCINTILLA_COLOUR(fore.red(), fore.green(), fore.blue()));

  QObject::connect(
      m_commentsEditor, &ScintillaEdit::hotSpotClick, [this](int position, int modifiers) {
        int start = position;
        while(m_commentsEditor->styleAt(start - 1) == link_style)
          start--;
        int end = position;
        while(m_commentsEditor->styleAt(end + 1) == link_style)
          end++;

        QString eventLink = QString::fromLatin1(m_commentsEditor->get_text_range(start, end + 1));

        if(eventLink[0] == QLatin1Char('@'))
        {
          eventLink.remove(0, 1);
          bool ok = false;
          uint32_t EID = eventLink.toUInt(&ok);
          if(ok)
            m_Ctx.SetEventID({}, EID, EID);
        }
      });

  ui->mainLayout->addWidget(m_commentsEditor);

  m_ignoreModifications = true;

  m_Ctx.AddCaptureViewer(this);
}

CommentView::~CommentView()
{
  m_Ctx.BuiltinWindowClosed(this);

  m_Ctx.RemoveCaptureViewer(this);
  delete ui;
}

void CommentView::OnCaptureClosed()
{
  SetComments(rdcstr());
  m_ignoreModifications = true;
}

void CommentView::OnCaptureLoaded()
{
  SetComments(m_Ctx.GetNotes("comments"));
  m_ignoreModifications = false;
}

void CommentView::OnEventChanged(uint32_t eventId)
{
  if(m_ignoreModifications)
    return;

  QString oldText = GetComments();
  QString newText = m_Ctx.GetNotes("comments");

  if(oldText != newText)
    SetComments(newText);
}

void CommentView::Restyle(int start, int end)
{
  m_commentsEditor->startStyling(start, 0);
  m_commentsEditor->setStyling(end - start, STYLE_DEFAULT);

  static QRegularExpression event_links(lit("(\\b|\\s|^)(@\\d+)\\b"));

  QString t(GetComments());

  QRegularExpressionMatch match = event_links.match(t, start);

  while(match.hasMatch())
  {
    sptr_t offset = match.capturedStart(2);

    if(offset > end)
      break;

    m_commentsEditor->startStyling(offset, 0);
    m_commentsEditor->setStyling(match.capturedLength(2), link_style);

    match = event_links.match(t, offset + 1);
  }
}

void CommentView::SetComments(const rdcstr &text)
{
  m_ignoreModifications = true;
  m_commentsEditor->setText(text.c_str());

  Restyle(0, m_commentsEditor->length());

  m_commentsEditor->emptyUndoBuffer();
  m_ignoreModifications = false;
}

rdcstr CommentView::GetComments()
{
  return QString::fromUtf8(m_commentsEditor->getText(m_commentsEditor->textLength() + 1));
}
