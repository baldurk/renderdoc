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

#include "RDTableView.h"
#include <QAbstractButton>
#include <QApplication>
#include <QClipboard>
#include <QHeaderView>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QScrollBar>
#include "Code/QRDUtils.h"
#include "RDHeaderView.h"

RDTableView::RDTableView(QWidget *parent) : QTableView(parent)
{
  m_horizontalHeader = new RDHeaderView(Qt::Horizontal, this);
  setHorizontalHeader(m_horizontalHeader);
  m_horizontalHeader->setSectionsClickable(true);

  m_delegate = new RichTextViewDelegate(this);
  QTableView::setItemDelegate(m_delegate);

  QObject::connect(m_horizontalHeader, &QHeaderView::sectionResized,
                   [this](int, int, int) { viewport()->update(); });
}

void RDTableView::setItemDelegate(QAbstractItemDelegate *delegate)
{
  m_userDelegate = delegate;
  m_delegate->setForwardDelegate(m_userDelegate);
}

QAbstractItemDelegate *RDTableView::itemDelegate() const
{
  return m_userDelegate;
}

int RDTableView::columnViewportPosition(int column) const
{
  return horizontalHeader()->sectionViewportPosition(column);
}

int RDTableView::columnAt(int x) const
{
  return horizontalHeader()->visualIndexAt(x);
}

int RDTableView::columnWidth(int column) const
{
  return horizontalHeader()->sectionSize(column);
}

void RDTableView::setColumnWidth(int column, int width)
{
  horizontalHeader()->resizeSection(column, width);

  updateGeometries();
}

void RDTableView::setColumnWidths(const QList<int> &widths)
{
  horizontalHeader()->resizeSections(widths);

  updateGeometries();
}

void RDTableView::resizeColumnsToContents()
{
  horizontalHeader()->resizeSections(QHeaderView::ResizeToContents);

  updateGeometries();
}

QRect RDTableView::visualRect(const QModelIndex &index) const
{
  if(!index.isValid())
    return QRect();

  const int row = index.row();
  const int col = index.column();

  const int gridWidth = showGrid() ? 1 : 0;

  return QRect(columnViewportPosition(col), rowViewportPosition(row), columnWidth(col) - gridWidth,
               rowHeight(row) - gridWidth);
}

QRegion RDTableView::visualRegionForSelection(const QItemSelection &selection) const
{
  QRegion selectionRegion;
  const QRect viewRect = viewport()->rect();

  QAbstractItemModel *m = model();

  for(const QItemSelectionRange &selRange : selection)
  {
    for(int row = selRange.top(); row <= selRange.bottom(); row++)
    {
      for(int col = selRange.left(); col <= selRange.right(); col++)
      {
        const QRect &rangeRect = visualRect(m->index(row, col));
        if(viewRect.intersects(rangeRect))
          selectionRegion += rangeRect;
      }
    }
  }

  return selectionRegion;
}

QModelIndex RDTableView::indexAt(const QPoint &p) const
{
  int row = rowAt(p.y());
  int col = columnAt(p.x());

  if(row < 0 || col < 0)
    return QModelIndex();

  return model()->index(row, col);
}

void RDTableView::setColumnGroupRole(int role)
{
  m_columnGroupRole = role;
  m_horizontalHeader->setColumnGroupRole(role);
}

void RDTableView::setPinnedColumns(int numColumns)
{
  m_pinnedColumns = numColumns;
  m_horizontalHeader->setPinnedColumns(numColumns, this);
}

void RDTableView::keyPressEvent(QKeyEvent *e)
{
  if(e == QKeySequence::Copy)
  {
    copySelectedIndices();

    e->accept();
    return;
  }

  return QTableView::keyPressEvent(e);
}

void RDTableView::copyIndices(const QModelIndexList &sel)
{
  if(!sel.isEmpty())
  {
    int stackWidths[16];
    int *heapWidths = NULL;

    int colCount = model()->columnCount();

    if(colCount >= 16)
      heapWidths = new int[colCount];

    int *widths = heapWidths ? heapWidths : stackWidths;

    for(int i = 0; i < colCount; i++)
      widths[i] = 0;

    int top = sel[0].row(), bottom = sel[0].row();
    int left = sel[0].column(), right = sel[0].column();

    // align the copied data so that each column is the same width
    for(QModelIndex idx : sel)
    {
      QString text = model()->data(idx).toString();
      widths[idx.column()] = qMax(widths[idx.column()], text.count());

      top = qMin(top, idx.row());
      bottom = qMax(bottom, idx.row());
      left = qMin(left, idx.column());
      right = qMax(right, idx.column());
    }

    // only align up to 50 characters so one really long item doesn't mess up the whole thing
    for(int i = 0; i < colCount; i++)
      widths[i] = qMin(50, widths[i]);

    QString clipData;
    for(int row = top; row <= bottom; row++)
    {
      for(int col = left; col <= right; col++)
      {
        QString format = col == left ? lit("%1") : lit(" %1");
        QString text = model()->data(model()->index(row, col)).toString();

        clipData += format.arg(text, -widths[col]);
      }

      clipData += lit("\n");
    }

    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(clipData.trimmed());

    delete[] heapWidths;
  }
}

void RDTableView::copySelectedIndices()
{
  copyIndices(selectionModel()->selectedIndexes());
}

void RDTableView::keyboardSearch(const QString &search)
{
  if(m_allowKeyboardSearches)
    return QTableView::keyboardSearch(search);
}

void RDTableView::paintEvent(QPaintEvent *e)
{
  const int gridWidth = showGrid() ? 1 : 0;
  QStyleOptionViewItem opt = viewOptions();

  QPainter painter(viewport());

  if(model()->rowCount() == 0 || model()->columnCount() == 0)
    return;

  int firstRow = qMax(verticalHeader()->visualIndexAt(0), 0);
  int lastRow = verticalHeader()->visualIndexAt(viewport()->height());
  if(lastRow < 0)
  {
    // if lastRow is negative, display as many will fit. This is a reasonable upper bound without
    // displaying all rows which could be massive
    lastRow = firstRow + (viewport()->height() / qMax(1, rowHeight(firstRow))) + 1;
  }
  lastRow = qMin(lastRow, verticalHeader()->count() - 1);

  int firstCol = qMax(horizontalHeader()->visualIndexAt(horizontalHeader()->pinnedWidth() + 1), 0);
  int lastCol = horizontalHeader()->visualIndexAt(viewport()->width());
  if(lastCol < 0)
    lastCol = horizontalHeader()->count() - 1;
  lastCol = qMin(lastCol, horizontalHeader()->count() - 1);

  firstCol = qMax(m_pinnedColumns, firstCol);

  for(int row = firstRow; row <= lastRow; row++)
  {
    for(int col = firstCol; col <= lastCol; col++)
    {
      const QModelIndex index = model()->index(row, col);
      if(index.isValid())
        paintCell(&painter, index, opt);
    }
    for(int col = 0; col < m_pinnedColumns; col++)
    {
      const QModelIndex index = model()->index(row, col);
      if(index.isValid())
        paintCell(&painter, index, opt);
    }
  }

  if(gridWidth)
  {
    QPen prevPen = painter.pen();
    QBrush prevBrush = painter.brush();

    QColor gridCol(QRgb(style()->styleHint(QStyle::SH_Table_GridLineColor, &opt, this)));

    painter.setPen(QPen(gridCol, 0, gridStyle()));
    painter.setBrush(QBrush(gridCol));

    // draw bottom line of each row
    for(int row = firstRow; row <= lastRow; row++)
    {
      int y = rowViewportPosition(row) + rowHeight(row) - gridWidth;
      painter.drawLine(viewport()->rect().left(), y, viewport()->rect().right(), y);
    }

    int gapSize = m_horizontalHeader->groupGapSize();

    // draw lines for each column, and group gaps
    for(int col = firstCol; col <= lastCol; col++)
    {
      int x = columnViewportPosition(col) + columnWidth(col) - gridWidth;

      if(m_horizontalHeader->hasGroupGap(col))
        painter.drawRect(x, viewport()->rect().top(), gapSize, viewport()->rect().height());
      else
        painter.drawLine(x, viewport()->rect().top(), x, viewport()->rect().bottom());
    }
    for(int col = 0; col < m_pinnedColumns; col++)
    {
      int x = columnViewportPosition(col) + columnWidth(col) - gridWidth;

      if(m_horizontalHeader->hasGroupGap(col))
        painter.drawRect(x, viewport()->rect().top(), gapSize, viewport()->rect().height());
      else
        painter.drawLine(x, viewport()->rect().top(), x, viewport()->rect().bottom());
    }

    painter.setPen(prevPen);
    painter.setBrush(prevBrush);
  }
}

void RDTableView::paintCell(QPainter *painter, const QModelIndex &index,
                            const QStyleOptionViewItem &opt)
{
  QStyleOptionViewItem cellopt = opt;

  cellopt.rect = QRect(columnViewportPosition(index.column()), rowViewportPosition(index.row()),
                       columnWidth(index.column()), rowHeight(index.row()));

  // erase the rect here since we need to draw over any overlapping non-pinned cells and
  // there's no way to just clip the above painting :(
  if(index.column() < m_pinnedColumns)
    painter->eraseRect(cellopt.rect);

  if(selectionModel() && selectionModel()->isSelected(index))
    cellopt.state |= QStyle::State_Selected;

  if(cellopt.rect.contains(viewport()->mapFromGlobal(QCursor::pos())))
    cellopt.state |= QStyle::State_MouseOver;

  // draw the background, then the cell
  style()->drawPrimitive(QStyle::PE_PanelItemViewRow, &cellopt, painter, this);
  QTableView::itemDelegate(index)->paint(painter, cellopt, index);
}

void RDTableView::mouseMoveEvent(QMouseEvent *e)
{
  QModelIndex oldHover = m_currentHoverIndex;

  QTableView::mouseMoveEvent(e);

  QModelIndex newHover = m_currentHoverIndex = indexAt(e->pos());

  update(newHover);

  if(m_delegate && m_delegate->linkHover(e, font(), newHover))
  {
    setCursor(QCursor(Qt::PointingHandCursor));
  }
  else
  {
    unsetCursor();
  }

  if(oldHover == newHover)
    return;

  update(oldHover);
}

void RDTableView::scrollTo(const QModelIndex &index, ScrollHint hint)
{
  if(!index.isValid())
    return;

  QRect cellRect = QRect(columnViewportPosition(index.column()), rowViewportPosition(index.row()),
                         columnWidth(index.column()), rowHeight(index.row()));

  QRect dataRect = viewport()->rect();
  dataRect.setLeft(horizontalHeader()->pinnedWidth());

  // if it's already visible then just bail, common case
  if(dataRect.contains(cellRect) && hint == QAbstractItemView::EnsureVisible)
    return;

  // assume per-item vertical scrolling and per-pixel horizontal scrolling

  // for any hint except position at center, we just ensure it's visible horizontally
  if(hint == QAbstractItemView::PositionAtCenter)
  {
    // center it horizontally from the left
    QPoint dataCenter = dataRect.center();
    QPoint cellCenter = cellRect.center();

    if(dataCenter.x() > cellCenter.x())
    {
      horizontalScrollBar()->setValue(horizontalScrollBar()->value() -
                                      (dataCenter.x() - cellCenter.x()));
    }

    // center it horizontally from the right
    if(dataCenter.x() < cellCenter.x())
    {
      horizontalScrollBar()->setValue(horizontalScrollBar()->value() +
                                      (cellCenter.x() - dataCenter.x()));
    }
  }
  else if(hint == QAbstractItemView::PositionAtTop)
  {
    horizontalScrollBar()->setValue(cellRect.left() - dataRect.left());
  }
  else if(hint == QAbstractItemView::PositionAtBottom)
  {
    horizontalScrollBar()->setValue(cellRect.right() - dataRect.right());
  }

  // collapse EnsureVisible to either PositionAtTop or PositionAtBottom depending on which side it's
  // on, or just return if we only had to make it visible horizontally
  if(hint == QAbstractItemView::EnsureVisible)
  {
    if(dataRect.bottom() < cellRect.bottom())
      hint = QAbstractItemView::PositionAtBottom;
    else if(dataRect.top() > cellRect.top())
      hint = QAbstractItemView::PositionAtTop;
    else
      return;
  }

  int firstRow = qMax(verticalHeader()->visualIndexAt(0), 0);
  int lastRow = verticalHeader()->visualIndexAt(viewport()->height());
  if(lastRow == -1)
    lastRow = verticalHeader()->count();

  int visibleRows = lastRow - firstRow + 1;

  // a partially displayed row doesn't count
  if(verticalHeader()->sectionViewportPosition(lastRow) + verticalHeader()->sectionSize(lastRow) >
     viewport()->height())
    visibleRows--;

  if(hint == QAbstractItemView::PositionAtTop)
  {
    verticalScrollBar()->setValue(index.row());
  }
  else if(hint == QAbstractItemView::PositionAtBottom)
  {
    verticalScrollBar()->setValue(index.row() - visibleRows + 1);
  }
  else if(hint == QAbstractItemView::PositionAtCenter)
  {
    verticalScrollBar()->setValue(index.row() - (visibleRows + 1) / 2);
  }

  update(index);
}

void RDTableView::updateGeometries()
{
  if(!m_horizontalHeader->customSizing())
    return QTableView::updateGeometries();

  static bool recurse = false;
  if(recurse)
    return;
  recurse = true;

  QAbstractButton *cornerButton = findChild<QAbstractButton *>();
  cornerButton->setVisible(false);

  QRect geom = viewport()->geometry();

  // assume no vertical header

  int horizHeight =
      qBound(horizontalHeader()->minimumHeight(), horizontalHeader()->sizeHint().height(),
             horizontalHeader()->maximumHeight());

  setViewportMargins(0, horizHeight, 0, 0);

  horizontalHeader()->setGeometry(geom.left(), geom.top() - horizHeight, geom.width(), horizHeight);

  // even though it's not visible we need to set the geometry right so that it looks up rows by
  // position properly.
  verticalHeader()->setGeometry(0, horizHeight, 0, geom.height());

  // if the headers are hidden nothing else will update their geometries and some things like
  // scrolling etc depend on it being up to date, so hackily call the protected slot. Yuk!
  if(verticalHeader()->isHidden())
    QMetaObject::invokeMethod(verticalHeader(), "updateGeometries");
  if(horizontalHeader()->isHidden())
    QMetaObject::invokeMethod(horizontalHeader(), "updateGeometries");

  // assume per-item vertical scrolling and per-pixel horizontal scrolling

  // vertical scroll bar
  {
    int firstRow = qMax(verticalHeader()->visualIndexAt(0), 0);
    int lastRow = verticalHeader()->visualIndexAt(viewport()->height());
    bool last = false;
    if(lastRow == -1)
    {
      last = true;
      lastRow = verticalHeader()->count();
    }

    int visibleRows = lastRow - firstRow + 1;

    // a partially displayed row doesn't count
    if(verticalHeader()->sectionViewportPosition(lastRow) + verticalHeader()->sectionSize(lastRow) >
       viewport()->height())
      visibleRows--;

    verticalScrollBar()->setRange(0, verticalHeader()->count() - visibleRows);
    verticalScrollBar()->setSingleStep(1);
    verticalScrollBar()->setPageStep(visibleRows);
    if(visibleRows >= verticalHeader()->count())
      verticalHeader()->setOffset(0);
    else if(last)
      verticalHeader()->setOffsetToLastSection();
  }

  // horizontal scroll bar
  {
    int totalWidth = horizontalHeader()->sizeHint().width();

    horizontalScrollBar()->setPageStep(viewport()->width() - horizontalHeader()->pinnedWidth());
    horizontalScrollBar()->setRange(0, totalWidth - viewport()->width());
    horizontalScrollBar()->setSingleStep(qMax(totalWidth / (horizontalHeader()->count() + 1), 2));
  }

  recurse = false;
  QAbstractItemView::updateGeometries();
}

void RDTableView::scrollContentsBy(int dx, int dy)
{
  QTableView::scrollContentsBy(dx, dy);

  viewport()->update();
}
