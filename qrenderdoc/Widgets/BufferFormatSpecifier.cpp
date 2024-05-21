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

#include "BufferFormatSpecifier.h"
#include <QFontDatabase>
#include <QKeyEvent>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QTextCursor>
#include <QTextDocument>
#include "Code/QRDUtils.h"
#include "Code/ScintillaSyntax.h"
#include "Widgets/Extended/RDSplitter.h"
#include "scintilla/include/SciLexer.h"
#include "scintilla/include/qt/ScintillaEdit.h"
#include "ui_BufferFormatSpecifier.h"

static const int ERROR_STYLE = STYLE_LASTPREDEFINED + 1;

BufferFormatList *globalFormatList = NULL;

BufferFormatList::BufferFormatList(ICaptureContext &ctx, QObject *parent)
    : m_Ctx(ctx), QObject(parent)
{
  rdcarray<rdcstr> saved = m_Ctx.Config().BufferFormatter_SavedFormats;

  for(rdcstr f : saved)
  {
    int idx = f.indexOf('\n');

    formats[QString(f.substr(0, idx))] = QString(f.substr(idx + 1)).trimmed();
  }
}

void BufferFormatList::setFormat(QString name, QString format)
{
  bool newFmt = !formats.contains(name);
  if(format.isEmpty())
    formats.remove(name);
  else
    formats[name] = format;
  if(newFmt || (!newFmt && format.isEmpty()))
    emit formatListUpdated();

  QStringList keys = formats.keys();
  keys.sort(Qt::CaseInsensitive);
  rdcarray<rdcstr> saved;
  for(QString k : keys)
    saved.push_back(k + lit("\n") + formats[k]);
  m_Ctx.Config().BufferFormatter_SavedFormats = saved;
  m_Ctx.Config().Save();
}

BufferFormatSpecifier::BufferFormatSpecifier(QWidget *parent)
    : QWidget(parent), ui(new Ui::BufferFormatSpecifier)
{
  ui->setupUi(this);

  formatText = new ScintillaEdit(this);

  formatText->styleSetFont(STYLE_DEFAULT, Formatter::FixedFont().family().toUtf8().data());
  formatText->styleSetSize(STYLE_DEFAULT, Formatter::FixedFont().pointSize());

  formatText->styleSetFont(ERROR_STYLE, Formatter::FixedFont().family().toUtf8().data());
  formatText->styleSetSize(ERROR_STYLE, Formatter::FixedFont().pointSize());

  QColor base = formatText->palette().color(QPalette::Base);

  QColor backCol = QColor::fromHslF(0.0f, 1.0f, qBound(0.1, base.lightnessF(), 0.9));
  QColor foreCol = contrastingColor(backCol, QColor(0, 0, 0));

  formatText->styleSetBack(ERROR_STYLE,
                           SCINTILLA_COLOUR(backCol.red(), backCol.green(), backCol.blue()));
  formatText->styleSetFore(ERROR_STYLE,
                           SCINTILLA_COLOUR(foreCol.red(), foreCol.green(), foreCol.blue()));

  ConfigureSyntax(formatText, SCLEX_BUFFER);

  formatText->setTabWidth(4);

  formatText->setScrollWidth(1);
  formatText->setScrollWidthTracking(true);

  formatText->annotationSetVisible(ANNOTATION_BOXED);

  formatText->colourise(0, -1);

  formatText->setMarginWidthN(0, 0);
  formatText->setMarginWidthN(1, 0);
  formatText->setMarginWidthN(2, 0);

  QFrame *formatContainer = new QFrame(this);
  QVBoxLayout *layout = new QVBoxLayout;
  layout->setContentsMargins(2, 2, 2, 2);
  layout->addWidget(formatText);
  formatContainer->setLayout(layout);

  QPalette pal = formatContainer->palette();
  pal.setColor(QPalette::Window, pal.color(QPalette::Base));
  formatContainer->setPalette(pal);
  formatContainer->setAutoFillBackground(true);

  formatContainer->setFrameShape(QFrame::Panel);
  formatContainer->setFrameShadow(QFrame::Plain);

  QObject::connect(formatText, &ScintillaEdit::modified,
                   [this](int type, int, int, int, const QByteArray &, int, int, int) {
                     ui->savedList->clearSelection();
                     if(!(type & (SC_MOD_CHANGEANNOTATION | SC_MOD_CHANGESTYLE)))
                       formatText->annotationClearAll();
                   });

  QHBoxLayout *hbox = new QHBoxLayout();
  hbox->setSpacing(0);
  hbox->setContentsMargins(2, 2, 2, 2);

  QWidget *helpOrFormat = new QWidget(this);
  helpOrFormat->setLayout(hbox);

  hbox->insertWidget(0, formatContainer);
  hbox->insertWidget(1, ui->helpText);

  m_Splitter = new RDSplitter(Qt::Horizontal, this);
  m_Splitter->setHandleWidth(12);
  m_Splitter->setChildrenCollapsible(false);

  m_Splitter->addWidget(helpOrFormat);
  m_Splitter->addWidget(ui->savedContainer);

  ui->formatGroup->layout()->addWidget(m_Splitter);

  ui->savedList->setItemDelegate(new FullEditorDelegate(ui->savedList));
  ui->savedList->setFont(Formatter::PreferredFont());
  ui->savedList->setColumns({tr("Saved formats")});

  setErrors({});

  on_showHelp_toggled(false);
}

BufferFormatSpecifier::~BufferFormatSpecifier()
{
  delete ui;

  // unregister any shortcuts on this window
  if(m_Ctx)
    m_Ctx->GetMainWindow()->UnregisterShortcut(QString(), this);
}

void BufferFormatSpecifier::setAutoFormat(QString autoFormat)
{
  m_AutoFormat = autoFormat;

  updateFormatList();

  setFormat(autoFormat);

  formatText->emptyUndoBuffer();

  on_apply_clicked();
}

void BufferFormatSpecifier::setContext(ICaptureContext *ctx)
{
  m_Ctx = ctx;

  if(!globalFormatList)
    globalFormatList = new BufferFormatList(*ctx, ctx->GetMainWindow()->Widget());

  QObject::connect(globalFormatList, &BufferFormatList::formatListUpdated, this,
                   &BufferFormatSpecifier::updateFormatList);

  m_Ctx->GetMainWindow()->RegisterShortcut(QKeySequence(QKeySequence::Refresh).toString(), this,
                                           [this](QWidget *) { on_apply_clicked(); });

  updateFormatList();
}

void BufferFormatSpecifier::setTitle(QString title)
{
  ui->formatGroup->setTitle(title);
}

void BufferFormatSpecifier::setFormat(const QString &format)
{
  formatText->setText(format.toUtf8().data());
}

void BufferFormatSpecifier::setErrors(const QMap<int, QString> &errors)
{
  formatText->annotationClearAll();

  bool first = true;

  for(auto err = errors.begin(); err != errors.end(); ++err)
  {
    int line = err.key();

    formatText->annotationSetStyle(line, ERROR_STYLE);
    formatText->annotationSetText(line, (tr("Error: %1").arg(err.value())).toUtf8().data());

    if(first)
    {
      int firstLine = formatText->firstVisibleLine();
      int linesVisible = formatText->linesOnScreen();

      if(line < firstLine || line > (firstLine + linesVisible - 1))
        formatText->setFirstVisibleLine(qMax(0, line - linesVisible / 2));

      first = false;
    }
  }
}

void BufferFormatSpecifier::updateFormatList()
{
  QString sel;

  RDTreeWidgetItem *item = ui->savedList->selectedItem();
  if(item)
    sel = item->text(0);

  {
    QSignalBlocker block(ui->savedList);
    int vs = ui->savedList->verticalScrollBar()->value();
    ui->savedList->beginUpdate();

    ui->savedList->clear();

    int selidx = -1;

    if(!m_AutoFormat.isEmpty())
    {
      item = new RDTreeWidgetItem({tr("<Auto-generated>")});
      item->setItalic(true);
      ui->savedList->addTopLevelItem(item);

      if(item->text(0) == sel)
        selidx = 0;
    }

    QStringList formats = globalFormatList->getFormats();

    for(QString f : formats)
    {
      if(f == sel)
        selidx = ui->savedList->topLevelItemCount();

      ui->savedList->addTopLevelItem(new RDTreeWidgetItem({f}));
    }

    {
      item = new RDTreeWidgetItem({tr("New...")});
      if(item->text(0) == sel)
        selidx = ui->savedList->topLevelItemCount();
      item->setEditable(0, true);
      ui->savedList->addTopLevelItem(item);
    }

    if(selidx >= 0)
      ui->savedList->setSelectedItem(ui->savedList->topLevelItem(selidx));

    ui->savedList->resizeColumnToContents(0);

    ui->savedList->endUpdate();
    ui->savedList->verticalScrollBar()->setValue(vs);
  }

  on_savedList_itemSelectionChanged();
}

void BufferFormatSpecifier::on_savedList_keyPress(QKeyEvent *event)
{
  if(event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace)
  {
    on_delDef_clicked();
  }
}

void BufferFormatSpecifier::on_savedList_itemChanged(RDTreeWidgetItem *item, int column)
{
  // ignore updates for anything but the last one
  if(ui->savedList->indexOfTopLevelItem(item) != ui->savedList->topLevelItemCount() - 1)
    return;

  QString name = item->text(0);

  // prevent recursion
  {
    QSignalBlocker block(ui->savedList);

    // if they didn't actually edit it, ignore
    if(name == tr("New..."))
      return;

    if(globalFormatList->hasFormat(name))
    {
      RDDialog::critical(this, tr("Name already in use"),
                         tr("The definition name '%1' is already in used.\n"
                            "To update this definition, select it and click update."));
      item->setText(0, tr("New..."));
      return;
    }
  }

  globalFormatList->setFormat(name,
                              QString::fromUtf8(formatText->getText(formatText->textLength() + 1)));
}

void BufferFormatSpecifier::on_savedList_itemDoubleClicked(RDTreeWidgetItem *item, int column)
{
  ui->savedList->setSelectedItem(item);

  if(ui->loadDef->isEnabled())
    on_loadDef_clicked();
}

void BufferFormatSpecifier::on_savedList_itemSelectionChanged()
{
  RDTreeWidgetItem *item = ui->savedList->selectedItem();

  ui->saveDef->setEnabled(item != NULL);
  ui->loadDef->setEnabled(item != NULL);
  ui->delDef->setEnabled(item != NULL);

  // auto format is always the first, and can't be saved to or deleted
  if(!m_AutoFormat.isEmpty() && ui->savedList->indexOfTopLevelItem(item) == 0)
  {
    ui->saveDef->setEnabled(false);
    ui->delDef->setEnabled(false);
  }

  // the 'new' format is always the last, and can't be loaded from or deleted
  if(ui->savedList->indexOfTopLevelItem(item) == ui->savedList->topLevelItemCount() - 1)
  {
    ui->loadDef->setEnabled(false);
    ui->delDef->setEnabled(false);

    ui->saveDef->setToolTip(tr("Create new current structure definition"));
  }
  else
  {
    ui->saveDef->setToolTip(tr("Update selected with current structure definition"));
  }
}

void BufferFormatSpecifier::on_showHelp_toggled(bool help)
{
  ui->helpText->setVisible(help);
  formatText->parentWidget()->setVisible(!help);

  if(help)
    ui->verticalLayout->invalidate();
}

void BufferFormatSpecifier::on_loadDef_clicked()
{
  RDTreeWidgetItem *item = ui->savedList->selectedItem();

  if(!item)
    return;

  QString name = item->text(0);

  QString format;
  if(!m_AutoFormat.isEmpty() && ui->savedList->indexOfTopLevelItem(item) == 0)
    format = m_AutoFormat;
  else
    format = globalFormatList->getFormat(name);

  {
    QSignalBlocker block(formatText);
    formatText->setText(format.toUtf8().data());
  }

  emit processFormat(format);
}

void BufferFormatSpecifier::on_saveDef_clicked()
{
  RDTreeWidgetItem *item = ui->savedList->selectedItem();

  if(!item)
    return;

  // for the 'new...' just trigger an edit and let the user do it that way. This reduces
  // duplication, avoids need for a prompt for the name, and educates the user that they can edit
  // directly
  if(ui->savedList->indexOfTopLevelItem(item) == ui->savedList->topLevelItemCount() - 1)
  {
    ui->savedList->editItem(item);
    return;
  }

  QString name = item->text(0);

  QMessageBox::StandardButton res = RDDialog::question(
      this, tr("Updating definition"),
      tr("Are you sure you wish to overwrite definition '%1'?").arg(name), RDDialog::YesNoCancel);

  if(res != QMessageBox::Yes)
    return;

  globalFormatList->setFormat(name,
                              QString::fromUtf8(formatText->getText(formatText->textLength() + 1)));
}

void BufferFormatSpecifier::on_delDef_clicked()
{
  RDTreeWidgetItem *item = ui->savedList->selectedItem();

  if(!item)
    return;

  QString name = item->text(0);

  QMessageBox::StandardButton res = RDDialog::question(
      this, tr("Deleting definition"),
      tr("Are you sure you wish to delete definition '%1'?").arg(name), RDDialog::YesNoCancel);

  if(res != QMessageBox::Yes)
    return;

  ui->savedList->clearSelection();

  globalFormatList->setFormat(name, QString());
}

void BufferFormatSpecifier::on_apply_clicked()
{
  setErrors({});
  emit processFormat(QString::fromUtf8(formatText->getText(formatText->textLength() + 1)));
}
