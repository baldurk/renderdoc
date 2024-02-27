/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2020-2024 Baldur Karlsson
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

#include <QDialog>
#include <QString>
#include <QStyledItemDelegate>
#include <QVector>

struct SDObject;

namespace Ui
{
class ConfigEditor;
}

class ConfigEditor;
class SettingModel;
class SettingFilterModel;
class RDTreeView;

class SettingDelegate : public QStyledItemDelegate
{
  Q_OBJECT

  ConfigEditor *m_Editor;
  RDTreeView *m_View;

public:
  explicit SettingDelegate(ConfigEditor *editor, RDTreeView *parent);
  ~SettingDelegate();
  void paint(QPainter *painter, const QStyleOptionViewItem &option,
             const QModelIndex &index) const override;

  QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;

  bool editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option,
                   const QModelIndex &index) override;

  QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                        const QModelIndex &index) const override;

  void setEditorData(QWidget *editor, const QModelIndex &index) const override;

  void setModelData(QWidget *editor, QAbstractItemModel *model,
                    const QModelIndex &index) const override;

public slots:
  void editorKeyPress(QKeyEvent *ev);
};

class ConfigEditor : public QDialog
{
  Q_OBJECT

public:
  explicit ConfigEditor(QWidget *parent = 0);
  ~ConfigEditor();

private slots:
  // automatic slots
  void on_filter_textChanged(const QString &text);

private:
  void keyPressEvent(QKeyEvent *e) override;

  SettingModel *m_SettingModel = NULL;
  SettingFilterModel *m_FilterModel = NULL;

  SDObject *m_Config = NULL;

  friend class SettingModel;
  friend class SettingFilterModel;
  friend class SettingDelegate;

  Ui::ConfigEditor *ui;
};
