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

#pragma once

#include <QHeaderView>

class QLabel;

class RDHeaderView : public QHeaderView
{
private:
  Q_OBJECT
public:
  explicit RDHeaderView(Qt::Orientation orient, QWidget *parent = 0);
  ~RDHeaderView();

  int groupGapSize() const { return 6; }
  QSize sizeHint() const override;
  void setModel(QAbstractItemModel *model) override;
  void reset() override;

  // these aren't virtual so we can't override them properly, but it's convenient for internal use
  // and any external calls that go to this type directly to use the correct version
  int sectionSize(int logicalIndex) const;
  int sectionViewportPosition(int logicalIndex) const;
  int visualIndexAt(int position) const;
  int logicalIndexAt(int position) const;
  int count() const;
  void resizeSection(int logicalIndex, int size);
  void resizeSections(const QList<int> &sizes);
  void resizeSections(QHeaderView::ResizeMode mode);

  inline int logicalIndexAt(int x, int y) const;
  inline int logicalIndexAt(const QPoint &pos) const;

  bool hasGroupGap(int columnIndex) const;
  bool hasGroupTitle(int columnIndex) const;

  void setColumnStretchHints(const QList<int> &hints);

  void setColumnGroupRole(int role) { m_columnGroupRole = role; }
  int columnGroupRole() const { return m_columnGroupRole; }
  void setPinnedColumns(int numColumns) { m_pinnedColumns = numColumns; }
  int pinnedColumns() const { return m_pinnedColumns; }
  void setCustomSizing(bool sizing) { m_customSizing = sizing; }
  bool customSizing() const { return m_customSizing; }
  int pinnedWidth() { return m_pinnedWidth; }
public slots:
  void setRootIndex(const QModelIndex &index) override;
  void headerDataChanged(Qt::Orientation orientation, int logicalFirst, int logicalLast);
  void columnsInserted(const QModelIndex &parent, int first, int last);
  void rowsChanged(const QModelIndex &parent, int first, int last);

protected:
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void paintEvent(QPaintEvent *e) override;

  void paintSection(QPainter *painter, const QRect &rect, int section) const override;
  void currentChanged(const QModelIndex &current, const QModelIndex &old) override;

  void updateGeometries() override;

  enum ResizeType
  {
    NoResize,
    LeftResize,
    RightResize
  };

  QPair<ResizeType, int> checkResizing(QMouseEvent *event);
  QPair<ResizeType, int> m_resizeState;
  int m_cursorPos = -1;

  void cacheSections();
  void resizeSectionsWithHints();
  void cacheSectionMinSizes();

  struct SectionData
  {
    int offset = 0;
    int size = 0;
    int group = 0;
    bool groupGap = false;
  };

  QSize m_sizeHint;
  QVector<SectionData> m_sections;
  int m_pinnedWidth = 0;

  bool m_suppressSectionCache = false;
  bool m_customSizing = false;

  QList<int> m_sectionStretchHints;
  QVector<int> m_sectionMinSizes;

  int m_columnGroupRole = 0;
  int m_pinnedColumns = 0;

  int m_movingSection = -1;
  QLabel *m_sectionPreview = NULL;
  int m_sectionPreviewOffset = 0;
};

inline int RDHeaderView::logicalIndexAt(int ax, int ay) const
{
  return orientation() == Qt::Horizontal ? logicalIndexAt(ax) : logicalIndexAt(ay);
}
inline int RDHeaderView::logicalIndexAt(const QPoint &apos) const
{
  return logicalIndexAt(apos.x(), apos.y());
}