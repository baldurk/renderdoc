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

#pragma once

#include <QMap>
#include <QWidget>

struct ICaptureContext;
class ScintillaEdit;

class RDTreeWidgetItem;
class RDSplitter;

namespace Ui
{
class BufferFormatSpecifier;
}

class BufferFormatList : public QObject
{
  Q_OBJECT

  ICaptureContext &m_Ctx;
  QMap<QString, QString> formats;

public:
  explicit BufferFormatList(ICaptureContext &ctx, QObject *parent = 0);
  QStringList getFormats() { return formats.keys(); }
  QString getFormat(QString name) { return formats[name]; }
  bool hasFormat(QString name) { return formats.contains(name); }
  void setFormat(QString name, QString format);

signals:
  void formatListUpdated();
};

extern BufferFormatList *globalFormatList;

class BufferFormatSpecifier : public QWidget
{
  Q_OBJECT

public:
  explicit BufferFormatSpecifier(QWidget *parent = 0);
  ~BufferFormatSpecifier();

  void setAutoFormat(QString autoFormat);

  void setContext(ICaptureContext *ctx);
  void setTitle(QString title);
  void setErrors(const QMap<int, QString> &errors);

signals:
  void processFormat(const QString &format);

public slots:
  // automatic slots
  void on_showHelp_toggled(bool help);
  void on_loadDef_clicked();
  void on_saveDef_clicked();
  void on_delDef_clicked();
  void on_savedList_keyPress(QKeyEvent *event);
  void on_savedList_itemChanged(RDTreeWidgetItem *item, int column);
  void on_savedList_itemDoubleClicked(RDTreeWidgetItem *item, int column);
  void on_savedList_itemSelectionChanged();

  // manual slots
  void setFormat(const QString &format);
  void updateFormatList();

private slots:
  void on_apply_clicked();

private:
  Ui::BufferFormatSpecifier *ui;
  ICaptureContext *m_Ctx;

  ScintillaEdit *formatText;

  RDSplitter *m_Splitter = NULL;

  QString m_AutoFormat;
};
