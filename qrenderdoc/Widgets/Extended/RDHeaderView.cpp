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

#include "RDHeaderView.h"
#include <QDebug>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QPointer>
#include "Code/QRDUtils.h"

/////////////////////////////////////////////////////////////////////////////////
//
// this file contains a few hardcoded assumptions for my use case, especially
// with the 'custom sizing' mode that allows merging sections and pinning sections
// and so on.
//
// * No handling for moving/rearranging/hiding sections with the custom sizing
//   mode. Just needs more careful handling and distinguishing between logical
//   and visual indices.
// * Probably a few places vertical orientation isn't handled right, but that
//   shouldn't be too bad.
//
/////////////////////////////////////////////////////////////////////////////////

RDHeaderView::RDHeaderView(Qt::Orientation orient, QWidget *parent) : QHeaderView(orient, parent)
{
  m_sectionPreview = new QLabel(this);

  setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
}

RDHeaderView::~RDHeaderView()
{
}

QSize RDHeaderView::sizeHint() const
{
  if(!m_customSizing)
    return QHeaderView::sizeHint();
  return m_sizeHint;
}

void RDHeaderView::setModel(QAbstractItemModel *model)
{
  QAbstractItemModel *m = this->model();

  if(m)
  {
    QObject::disconnect(m, &QAbstractItemModel::headerDataChanged, this,
                        &RDHeaderView::headerDataChanged);
    QObject::disconnect(m, &QAbstractItemModel::columnsInserted, this,
                        &RDHeaderView::columnsInserted);
    QObject::disconnect(m, &QAbstractItemModel::rowsInserted, this, &RDHeaderView::rowsChanged);
    QObject::disconnect(m, &QAbstractItemModel::rowsRemoved, this, &RDHeaderView::rowsChanged);
  }

  QHeaderView::setModel(model);

  if(model)
  {
    QObject::connect(model, &QAbstractItemModel::headerDataChanged, this,
                     &RDHeaderView::headerDataChanged);
    QObject::connect(model, &QAbstractItemModel::columnsInserted, this,
                     &RDHeaderView::columnsInserted);
    QObject::connect(model, &QAbstractItemModel::rowsInserted, this, &RDHeaderView::rowsChanged);
    QObject::connect(model, &QAbstractItemModel::rowsRemoved, this, &RDHeaderView::rowsChanged);
  }
}

void RDHeaderView::reset()
{
  if(m_customSizing)
    cacheSections();
}

void RDHeaderView::cacheSections()
{
  if(m_suppressSectionCache)
    return;

  QAbstractItemModel *m = this->model();

  int oldCount = m_sections.count();
  m_sections.resize(m->columnCount());

  // give new sections a default minimum size
  for(int col = oldCount; col < m_sections.count(); col++)
    m_sections[col].size = 10;

  for(int col = 0; col < m_sections.count(); col++)
  {
    if(m_columnGroupRole > 0)
    {
      QVariant v = m->data(m->index(0, col), m_columnGroupRole);
      if(v.isValid())
        m_sections[col].group = v.toInt();
      else
        m_sections[col].group = -m_columnGroupRole - col;

      if(col > 0)
      {
        m_sections[col - 1].groupGap =
            (m_sections[col].group != m_sections[col - 1].group) && m_sections[col].group >= 0;
      }
    }
    else
    {
      m_sections[col].group = col;
      m_sections[col].groupGap = true;
    }
  }

  int accum = 0;

  for(int col = 0; col < m_sections.count(); col++)
  {
    if(col == m_pinnedColumns)
      m_pinnedWidth = accum;

    m_sections[col].offset = accum;
    accum += m_sections[col].size;

    if(hasGroupGap(col))
      accum += groupGapSize();
  }

  if(m_pinnedColumns >= m_sections.count())
    m_pinnedWidth = m_pinnedColumns;

  QHeaderView::initializeSections();

  QStyleOptionHeader opt;
  initStyleOption(&opt);

  QFont f = font();
  f.setBold(true);

  opt.section = 0;
  opt.fontMetrics = QFontMetrics(f);
  opt.text = m->headerData(0, orientation(), Qt::DisplayRole).toString();

  m_sizeHint = style()->sizeFromContents(QStyle::CT_HeaderSection, &opt, QSize(), this);
  m_sizeHint.setWidth(accum);

  viewport()->update(viewport()->rect());
}

int RDHeaderView::sectionSize(int logicalIndex) const
{
  if(m_customSizing)
  {
    if(logicalIndex < 0 || logicalIndex >= m_sections.count())
      return 0;

    return m_sections[logicalIndex].size;
  }

  return QHeaderView::sectionSize(logicalIndex);
}

int RDHeaderView::sectionViewportPosition(int logicalIndex) const
{
  if(m_customSizing)
  {
    if(logicalIndex < 0 || logicalIndex >= m_sections.count())
      return -1;

    int offs = m_sections[logicalIndex].offset;

    if(logicalIndex >= m_pinnedColumns)
      offs -= offset();

    return offs;
  }

  return QHeaderView::sectionViewportPosition(logicalIndex);
}

int RDHeaderView::visualIndexAt(int position) const
{
  if(m_customSizing)
  {
    if(m_sections.isEmpty())
      return -1;

    if(position >= m_pinnedWidth)
      position += offset();

    SectionData search;
    search.offset = position;
    auto it = std::lower_bound(
        m_sections.begin(), m_sections.end(), search,
        [](const SectionData &a, const SectionData &b) { return a.offset <= b.offset; });

    if(it != m_sections.begin())
      --it;

    if(it->offset <= position &&
       position < (it->offset + it->size + (it->groupGap ? groupGapSize() : 0)))
      return (it - m_sections.begin());

    return -1;
  }

  return QHeaderView::visualIndexAt(position);
}

int RDHeaderView::logicalIndexAt(int position) const
{
  return visualIndexAt(position);
}

int RDHeaderView::count() const
{
  if(m_customSizing)
    return m_sections.count();

  return QHeaderView::count();
}

void RDHeaderView::resizeSection(int logicalIndex, int size)
{
  if(!m_customSizing)
    return QHeaderView::resizeSection(logicalIndex, size);

  if(logicalIndex >= 0 && logicalIndex < m_sections.count())
  {
    int oldSize = m_sections[logicalIndex].size;
    m_sections[logicalIndex].size = size;

    emit sectionResized(logicalIndex, oldSize, size);
  }

  cacheSections();
}

void RDHeaderView::resizeSections(QHeaderView::ResizeMode mode)
{
  if(!m_sectionStretchHints.isEmpty())
  {
    resizeSectionsWithHints();
    return;
  }

  if(!m_customSizing)
    return QHeaderView::resizeSections(mode);

  if(mode != ResizeToContents)
    return;

  QAbstractItemModel *m = this->model();

  int rowCount = m->rowCount();

  for(int col = 0; col < m_sections.count(); col++)
  {
    QSize sz;

    for(int row = 0; row < rowCount; row++)
    {
      QVariant v = m->data(m->index(row, col), Qt::SizeHintRole);
      if(v.isValid() && v.canConvert<QSize>())
        sz = sz.expandedTo(v.value<QSize>());
    }

    int oldSize = m_sections[col].size;

    m_sections[col].size = sz.width();

    emit sectionResized(col, oldSize, sz.width());
  }
}

void RDHeaderView::resizeSections(const QList<int> &sizes)
{
  if(!m_customSizing)
  {
    for(int i = 0; i < qMin(sizes.count(), QHeaderView::count()); i++)
    {
      QHeaderView::resizeSection(i, sizes[i]);
    }
    return;
  }

  for(int i = 0; i < qMin(sizes.count(), m_sections.count()); i++)
  {
    int oldSize = m_sections[i].size;

    m_sections[i].size = sizes[i];

    emit sectionResized(i, oldSize, sizes[i]);
  }

  cacheSections();
}

bool RDHeaderView::hasGroupGap(int columnIndex) const
{
  if(columnIndex >= 0 && columnIndex < m_sections.count())
    return m_sections[columnIndex].groupGap;

  return false;
}

bool RDHeaderView::hasGroupTitle(int columnIndex) const
{
  if(columnIndex == m_sections.count() - 1)
    return true;

  if(columnIndex >= 0 && columnIndex < m_sections.count())
    return m_sections[columnIndex].groupGap || m_sections[columnIndex].group < 0;

  return false;
}

void RDHeaderView::cacheSectionMinSizes()
{
  m_sectionMinSizes.resize(count());

  for(int i = 0; i < m_sectionMinSizes.count(); i++)
  {
    int sz = 0;

    // see if we can fetch the column/row size hint from the item view
    QAbstractItemView *view = qobject_cast<QAbstractItemView *>(parent());
    if(view)
    {
      if(orientation() == Qt::Horizontal)
        sz = view->sizeHintForColumn(i);
      else
        sz = view->sizeHintForRow(i);
    }

    // also include the size for the header as another minimum
    if(orientation() == Qt::Horizontal)
      sz = qMax(sz, sectionSizeFromContents(i).width());
    else
      sz = qMax(sz, sectionSizeFromContents(i).height());

    // finally respect the minimum section size specified
    sz = qMax(sz, minimumSectionSize());

    // update the minimum size for this section and count the total which we'll need
    m_sectionMinSizes[i] = sz;
  }
}

void RDHeaderView::resizeSectionsWithHints()
{
  if(m_sectionMinSizes.isEmpty() || m_sectionStretchHints.isEmpty())
    return;

  QVector<int> sizes = m_sectionMinSizes;
  int minsizesTotal = 0;
  int stretchHintTotal = 0;

  for(int i = 0; i < count() && i < sizes.count() && i < m_sectionStretchHints.count(); i++)
  {
    if(isSectionHidden(i))
      sizes[i] = 0;
    else if(m_sectionStretchHints[i] >= 0)
      stretchHintTotal += m_sectionStretchHints[i];

    minsizesTotal += sizes[i];
  }

  if(stretchHintTotal == 0)
    return;

  int available = 0;

  if(orientation() == Qt::Horizontal)
    available = rect().width();
  else
    available = rect().height();

  // see if we even have any extra space to allocate
  if(available > minsizesTotal)
  {
    // this is how much space we can allocate to stretch sections
    available -= minsizesTotal;

    // distribute the available space between the sections. Dividing by the total stretch tells us
    // how many 'whole' multiples we can allocate:
    int wholeMultiples = available / stretchHintTotal;

    if(wholeMultiples > 0)
    {
      for(int i = 0; i < sizes.count() && i < m_sectionStretchHints.count(); i++)
      {
        int hint = m_sectionStretchHints[i];
        if(hint > 0 && !isSectionHidden(i))
          sizes[i] += wholeMultiples * hint;
      }
    }

    available -= wholeMultiples * stretchHintTotal;

    // we now have a small amount (less than stretchHintTotal) of extra space to allocate.
    // we still want to assign this leftover proportional to the hints, otherwise we'd end up with a
    // stair-stepping effect.
    // To do this we calculate hint/total for each section then loop around adding on fractional
    // components to the sizes until one is above 1, then it gets a pixel, and we keep going until
    // all the remainder is allocated
    QVector<float> fractions, increment;
    fractions.resize(sizes.count());
    increment.resize(sizes.count());

    // set up increments
    for(int i = 0; i < sizes.count(); i++)
    {
      // don't assign any space to sections with negative hints, or sections without hints, or
      // hidden sections
      if(i >= m_sectionStretchHints.count() || m_sectionStretchHints[i] <= 0 || isSectionHidden(i))
      {
        increment[i] = 0.0f;
        continue;
      }

      increment[i] = float(m_sectionStretchHints[i]) / float(stretchHintTotal);
    }

    while(available > 0)
    {
      // loop along each section incrementing it.
      for(int i = 0; i < fractions.count(); i++)
      {
        fractions[i] += increment[i];

        // if we have a whole pixel now, assign it
        if(fractions[i] > 1.0f)
        {
          fractions[i] -= 1.0f;
          sizes[i]++;
          available--;

          // if we've assigned all pixels, stop
          if(available == 0)
            break;
        }
      }
    }
  }

  resizeSections(sizes.toList());
}

void RDHeaderView::setColumnStretchHints(const QList<int> &hints)
{
  if(hints.count() != count())
    qCritical() << "Got" << hints.count() << "hints, but have" << count() << "columns";
  m_sectionStretchHints = hints;

  // we take control of the sizing, we don't currently support custom resizing AND stretchy size
  // hints.
  QHeaderView::setSectionResizeMode(QHeaderView::Fixed);

  cacheSectionMinSizes();
  resizeSectionsWithHints();
}

void RDHeaderView::setRootIndex(const QModelIndex &index)
{
  QHeaderView::setRootIndex(index);

  // need to enqueue this after the root index is actually processed (this function is called
  // *before* the root index changes).
  if(!m_sectionStretchHints.isEmpty())
  {
    QPointer<RDHeaderView> ptr = this;
    GUIInvoke::defer(this, [this]() {
      cacheSectionMinSizes();
      resizeSectionsWithHints();
    });
  }
}

void RDHeaderView::headerDataChanged(Qt::Orientation orientation, int logicalFirst, int logicalLast)
{
  if(m_customSizing)
    cacheSections();
}

void RDHeaderView::columnsInserted(const QModelIndex &parent, int first, int last)
{
  if(m_customSizing)
    cacheSections();
}

void RDHeaderView::rowsChanged(const QModelIndex &parent, int first, int last)
{
  if(!m_sectionStretchHints.isEmpty())
  {
    cacheSectionMinSizes();
    resizeSectionsWithHints();
  }
}

void RDHeaderView::mousePressEvent(QMouseEvent *event)
{
  int mousePos = event->x();
  int idx = logicalIndexAt(mousePos);

  if(sectionsMovable() && idx >= 0 && event->buttons() == Qt::LeftButton)
  {
    int secSize = sectionSize(idx);
    int secPos = sectionViewportPosition(idx);

    int handleWidth = style()->pixelMetric(QStyle::PM_HeaderGripMargin, 0, this);

    if(secPos >= 0 && secSize > 0 && mousePos >= secPos + handleWidth &&
       mousePos <= secPos + secSize - handleWidth)
    {
      m_movingSection = idx;

      m_sectionPreview->resize(secSize, height());

      QPixmap preview(m_sectionPreview->size());
      preview.fill(QColor::fromRgba(qRgba(0, 0, 0, 100)));

      QPainter painter(&preview);
      painter.setOpacity(0.75f);
      paintSection(&painter, QRect(QPoint(0, 0), m_sectionPreview->size()), idx);
      painter.end();

      m_sectionPreview->setPixmap(preview);

      m_sectionPreviewOffset = mousePos - secPos;

      m_sectionPreview->move(mousePos - m_sectionPreviewOffset, 0);
      m_sectionPreview->show();

      return;
    }
  }

  if(m_customSizing)
  {
    m_resizeState = checkResizing(event);
    m_cursorPos = QCursor::pos().x();

    return QAbstractItemView::mousePressEvent(event);
  }

  QHeaderView::mousePressEvent(event);
}

void RDHeaderView::mouseMoveEvent(QMouseEvent *event)
{
  if(m_movingSection >= 0)
  {
    m_sectionPreview->move(event->x() - m_sectionPreviewOffset, 0);
    return;
  }

  if(m_customSizing)
  {
    if(m_resizeState.first == NoResize || m_resizeState.second < 0 ||
       m_resizeState.second >= m_sections.count())
    {
      auto res = checkResizing(event);

      bool hasCursor = testAttribute(Qt::WA_SetCursor);

      if(res.first != NoResize)
      {
        if(!hasCursor)
          setCursor(Qt::SplitHCursor);
      }
      else if(hasCursor)
      {
        unsetCursor();
      }
    }
    else
    {
      int curX = QCursor::pos().x();
      int delta = curX - m_cursorPos;

      int idx = m_resizeState.second;

      if(m_resizeState.first == LeftResize && idx > 0)
        idx--;

      // batch the cache update
      m_suppressSectionCache = true;

      int firstCol = idx;
      int lastCol = idx;

      // idx is the last in a group, so search backwards to see if there are neighbour sections we
      // should share the resize with
      while(firstCol > 0 && m_sections[firstCol - 1].group == m_sections[lastCol].group)
        firstCol--;

      // how much space could we lose on the columns, in total
      int freeSpace = 0;
      for(int col = firstCol; col <= lastCol; col++)
        freeSpace += m_sections[col].size - minimumSectionSize();

      int numCols = lastCol - firstCol + 1;

      // spread the delta amonst the colummns
      int perSectionDelta = delta / numCols;

      // call resizeSection to emit the sectionResized signal but we set m_suppressSectionCache so
      // we won't cache sections.
      for(int col = firstCol; col <= lastCol; col++)
        resizeSection(col, qMax(minimumSectionSize(), m_sections[col].size + perSectionDelta));

      // if there was an uneven spread, a few pixels will remain
      int remainder = delta - perSectionDelta * numCols;

      // loop around for the remainder pixels, assigning them one by one to the smallest/largest
      // column.
      // this is inefficient but remainder is very small - at most 3.
      int step = remainder < 0 ? -1 : 1;
      for(int i = 0; i < qAbs(remainder); i++)
      {
        int chosenCol = firstCol;
        for(int col = firstCol; col <= lastCol; col++)
        {
          if(step > 0 && m_sections[col].size < m_sections[chosenCol].size)
            chosenCol = col;
          else if(step < 0 && m_sections[col].size > m_sections[chosenCol].size)
            chosenCol = col;
        }

        resizeSection(chosenCol, qMax(minimumSectionSize(), m_sections[chosenCol].size + step));
      }

      // only updating the cursor when the section is moving means that it becomes 'sticky'. If we
      // try to size down below the minimum size and keep going then it doesn't start resizing up
      // until it passes the divider again.
      int appliedDelta = delta;

      // if we were resizing down, at best we removed the remaining free space
      if(delta < 0)
        appliedDelta = qMax(delta, -freeSpace);

      m_cursorPos += appliedDelta;

      m_suppressSectionCache = false;

      cacheSections();
    }

    return QAbstractItemView::mouseMoveEvent(event);
  }

  QHeaderView::mouseMoveEvent(event);
}

QPair<RDHeaderView::ResizeType, int> RDHeaderView::checkResizing(QMouseEvent *event)
{
  int mousePos = event->x();
  int idx = logicalIndexAt(mousePos);

  bool hasCursor = testAttribute(Qt::WA_SetCursor);
  bool cursorSet = false;

  bool leftResize = idx > 0 && (m_sections[idx - 1].group != m_sections[idx].group);
  bool rightResize = idx >= 0 && hasGroupTitle(idx);

  if(leftResize || rightResize)
  {
    int secSize = sectionSize(idx);
    int secPos = sectionViewportPosition(idx);

    int handleWidth = style()->pixelMetric(QStyle::PM_HeaderGripMargin, 0, this);

    int gapWidth = 0;
    if(hasGroupGap(idx))
      gapWidth = groupGapSize();

    if(leftResize && secPos >= 0 && secSize > 0 && mousePos < secPos + handleWidth)
    {
      return qMakePair(LeftResize, idx);
    }
    if(rightResize && secPos >= 0 && secSize > 0 &&
       mousePos > secPos + secSize - handleWidth - gapWidth)
    {
      return qMakePair(RightResize, idx);
    }
  }

  return qMakePair(NoResize, -1);
}

void RDHeaderView::mouseReleaseEvent(QMouseEvent *event)
{
  if(m_movingSection >= 0)
  {
    int mousePos = event->x();
    int idx = logicalIndexAt(mousePos);

    if(idx >= 0)
    {
      int secSize = sectionSize(idx);
      int secPos = sectionPosition(idx);

      int srcSection = visualIndex(m_movingSection);
      int dstSection = visualIndex(idx);

      if(srcSection >= 0 && dstSection >= 0 && srcSection != dstSection)
      {
        // the half-way point of the section decides whether we're dropping to the left
        // or the right of it.
        if(mousePos < secPos + secSize / 2)
        {
          // if we're moving from the left, place it to the left of dstSection
          if(srcSection < dstSection)
            moveSection(srcSection, dstSection - 1);
          else
            moveSection(srcSection, dstSection);
        }
        else
        {
          // if we're moving it from the right, place it to the right of dstSection
          if(srcSection > dstSection)
            moveSection(srcSection, dstSection + 1);
          else
            moveSection(srcSection, dstSection);
        }
      }
    }

    m_sectionPreview->hide();
  }

  m_movingSection = -1;

  if(m_customSizing)
  {
    m_resizeState = qMakePair(NoResize, -1);

    return QAbstractItemView::mouseReleaseEvent(event);
  }

  QHeaderView::mouseReleaseEvent(event);
}

void RDHeaderView::paintEvent(QPaintEvent *e)
{
  if(!m_customSizing)
    return QHeaderView::paintEvent(e);

  if(count() == 0)
    return;

  QPainter painter(viewport());

  int start = qMax(visualIndexAt(e->rect().left()), 0);
  int end = visualIndexAt(e->rect().right());

  if(end == -1)
    end = count() - 1;

  // make sure we always paint the whole header for any merged headers
  while(start > 0 && !hasGroupTitle(start - 1))
    start--;
  while(end < m_sections.count() && !hasGroupTitle(end))
    end++;

  QRect accumRect;
  for(int i = start; i <= end; ++i)
  {
    int pos = sectionViewportPosition(i);
    int size = sectionSize(i);

    if(!hasGroupGap(i) && pos < 0)
    {
      size += pos;
      pos = 0;
    }

    // either set or accumulate this section's rect
    if(accumRect.isEmpty())
      accumRect.setRect(pos, 0, size, viewport()->height());
    else
      accumRect.setWidth(accumRect.width() + size);

    if(hasGroupTitle(i))
    {
      painter.save();

      if(accumRect.left() < m_pinnedWidth && i >= m_pinnedColumns)
        accumRect.setLeft(m_pinnedWidth);

      paintSection(&painter, accumRect, i);
      painter.restore();

      // if we have more sections to go, reset so we can accumulate the next group
      if(i < end)
        accumRect = QRect();
    }
  }

  // clear the remainder of the header if there's a gap
  if(accumRect.right() < e->rect().right())
  {
    QStyleOption opt;
    opt.init(this);
    opt.state |= QStyle::State_Horizontal;
    opt.rect =
        QRect(accumRect.right() + 1, 0, e->rect().right() - accumRect.right(), viewport()->height());
    style()->drawControl(QStyle::CE_HeaderEmptyArea, &opt, &painter, this);
  }
}

void RDHeaderView::paintSection(QPainter *painter, const QRect &rect, int section) const
{
  if(!m_customSizing)
    return QHeaderView::paintSection(painter, rect, section);

  if(!rect.isValid())
    return;

  QStyleOptionHeader opt;
  initStyleOption(&opt);

  QAbstractItemModel *m = this->model();

  if(hasFocus())
    opt.state |= (QStyle::State_Active | QStyle::State_HasFocus);
  else
    opt.state &= ~(QStyle::State_Active | QStyle::State_HasFocus);

  opt.rect = rect;
  opt.section = section;
  opt.textAlignment = defaultAlignment();
  opt.iconAlignment = Qt::AlignVCenter;

  QVariant variant;

  if(m_columnGroupRole)
  {
    variant = m->headerData(section, orientation(), m_columnGroupRole);
    if(variant.isValid() && variant.canConvert<QString>())
      opt.text = variant.toString();
  }

  if(opt.text.isEmpty())
    opt.text = m->headerData(section, orientation(), Qt::DisplayRole).toString();

  int margin = 2 * style()->pixelMetric(QStyle::PM_HeaderMargin, 0, this);

  if(textElideMode() != Qt::ElideNone)
    opt.text = opt.fontMetrics.elidedText(opt.text, textElideMode(), rect.width() - margin);

  if(section == 0 && section == m_sections.count() - 1)
    opt.position = QStyleOptionHeader::OnlyOneSection;
  else if(section == 0)
    opt.position = QStyleOptionHeader::Beginning;
  else if(section == m_sections.count() - 1)
    opt.position = QStyleOptionHeader::End;
  else
    opt.position = QStyleOptionHeader::Middle;

  opt.orientation = orientation();

  bool prevSel = section > 0 && selectionModel()->isColumnSelected(section - 1, QModelIndex());
  bool nextSel = section + 1 < m_sections.count() &&
                 selectionModel()->isColumnSelected(section + 1, QModelIndex());

  if(prevSel && nextSel)
    opt.selectedPosition = QStyleOptionHeader::NextAndPreviousAreSelected;
  else if(prevSel)
    opt.selectedPosition = QStyleOptionHeader::PreviousIsSelected;
  else if(nextSel)
    opt.selectedPosition = QStyleOptionHeader::NextIsSelected;
  else
    opt.selectedPosition = QStyleOptionHeader::NotAdjacent;

  style()->drawControl(QStyle::CE_Header, &opt, painter, this);
}

void RDHeaderView::currentChanged(const QModelIndex &current, const QModelIndex &old)
{
  if(!m_customSizing)
    return QHeaderView::currentChanged(current, old);

  // not optimal at all
  if(current != old)
  {
    QRect r = viewport()->rect();

    if(old.isValid())
    {
      QRect rect = r;

      if(orientation() == Qt::Horizontal)
      {
        rect.setLeft(sectionViewportPosition(old.column()));
        rect.setWidth(sectionSize(old.column()));
      }
      else
      {
        rect.setTop(sectionViewportPosition(old.column()));
        rect.setHeight(sectionSize(old.column()));
      }

      viewport()->update(rect);
    }

    if(current.isValid())
    {
      QRect rect = r;

      if(orientation() == Qt::Horizontal)
      {
        rect.setLeft(sectionViewportPosition(current.column()));
        rect.setWidth(sectionSize(current.column()));
      }
      else
      {
        rect.setTop(sectionViewportPosition(current.column()));
        rect.setHeight(sectionSize(current.column()));
      }

      viewport()->update(rect);
    }
  }
}

void RDHeaderView::updateGeometries()
{
  if(!m_sectionStretchHints.isEmpty())
    resizeSectionsWithHints();

  QHeaderView::updateGeometries();
}
