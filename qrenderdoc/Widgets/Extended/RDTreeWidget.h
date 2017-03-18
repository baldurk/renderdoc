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

#pragma once

#include <QTreeWidget>

class RDTreeWidget : public QTreeWidget
{
  Q_OBJECT
public:
  explicit RDTreeWidget(QWidget *parent = 0);

  void setDefaultHoverColor(QColor col) { m_defaultHoverColour = col; }
  void setHoverIconColumn(int column)
  {
    m_hoverColumn = column;
    m_hoverHandCursor = true;
    m_activateOnClick = true;
  }
  void setHoverHandCursor(bool hand) { m_hoverHandCursor = hand; }
  void setHoverClickActivate(bool click) { m_activateOnClick = click; }
  void setClearSelectionOnFocusLoss(bool clear) { m_clearSelectionOnFocusLoss = clear; }
  void setHoverIcons(QTreeWidgetItem *item, QIcon normal, QIcon hover);
  void setHoverColour(QTreeWidgetItem *item, QColor col);

signals:
  void mouseMove(QMouseEvent *e);
  void leave(QEvent *e);
  void keyPress(QKeyEvent *e);

public slots:

private:
  void mouseMoveEvent(QMouseEvent *e) override;
  void mouseReleaseEvent(QMouseEvent *e) override;
  void leaveEvent(QEvent *e) override;
  void focusOutEvent(QFocusEvent *event) override;
  void keyPressEvent(QKeyEvent *e) override;

  void clearHovers(QTreeWidgetItem *root, QTreeWidgetItem *exception);

  QColor m_defaultHoverColour;
  int m_hoverColumn = 0;
  bool m_hoverHandCursor = false;
  bool m_clearSelectionOnFocusLoss = true;
  bool m_activateOnClick = false;

  static const Qt::ItemDataRole hoverIconRole = Qt::ItemDataRole(Qt::UserRole + 10000);
  static const Qt::ItemDataRole backupNormalIconRole = Qt::ItemDataRole(Qt::UserRole + 10001);
  static const Qt::ItemDataRole hoverBackColourRole = Qt::ItemDataRole(Qt::UserRole + 10002);
  static const Qt::ItemDataRole backupBackColourRole = Qt::ItemDataRole(Qt::UserRole + 10002);
};
