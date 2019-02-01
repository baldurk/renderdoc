/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2017-2019 Baldur Karlsson
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
#include "3rdparty/scintilla/include/SciLexer.h"
#include "3rdparty/scintilla/include/qt/ScintillaEdit.h"
#include "Code/ScintillaSyntax.h"
#include "ui_CommentView.h"

CommentView::CommentView(ICaptureContext &ctx, QWidget *parent)
    : QFrame(parent), ui(new Ui::CommentView), m_Ctx(ctx)
{
  ui->setupUi(this);

  m_commentsEditor = new ScintillaEdit(this);

  m_commentsEditor->styleSetFont(
      STYLE_DEFAULT, QFontDatabase::systemFont(QFontDatabase::FixedFont).family().toUtf8().data());
  m_commentsEditor->setTabWidth(4);

  m_commentsEditor->setWrapMode(SC_WRAP_WORD);
  m_commentsEditor->setWrapVisualFlags(SC_WRAPVISUALFLAG_MARGIN);

  m_commentsEditor->setMarginWidthN(0, sptr_t(30.0 * devicePixelRatioF()));

  ConfigureSyntax(m_commentsEditor, SCLEX_NULL);

  QObject::connect(m_commentsEditor, &ScintillaEdit::modified, [this](int type, int, int, int,
                                                                      const QByteArray &, int, int,
                                                                      int) {

    if(m_ignoreModifications)
      return;

    if(type & (SC_MOD_INSERTTEXT | SC_MOD_DELETETEXT | SC_MOD_BEFOREINSERT | SC_MOD_BEFOREDELETE))
    {
      QString text = QString::fromUtf8(m_commentsEditor->getText(m_commentsEditor->textLength() + 1));
      text.remove(QLatin1Char('\r'));
      m_Ctx.SetNotes(lit("comments"), text);
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
  m_ignoreModifications = true;
  m_commentsEditor->setText("");
  m_commentsEditor->emptyUndoBuffer();
}

void CommentView::OnCaptureLoaded()
{
  m_commentsEditor->setText(m_Ctx.GetNotes("comments").c_str());
  m_commentsEditor->emptyUndoBuffer();
  m_ignoreModifications = false;
}

void CommentView::OnEventChanged(uint32_t eventId)
{
  QString oldText = QString::fromUtf8(m_commentsEditor->getText(m_commentsEditor->textLength() + 1));
  QString newText = m_Ctx.GetNotes("comments");

  if(oldText != newText)
  {
    bool oldIgnore = m_ignoreModifications;
    m_ignoreModifications = true;
    m_commentsEditor->setText(newText.toUtf8().data());
    m_commentsEditor->emptyUndoBuffer();
    m_ignoreModifications = oldIgnore;
  }
}
