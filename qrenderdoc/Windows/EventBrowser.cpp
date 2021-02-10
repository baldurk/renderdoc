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

#include "EventBrowser.h"
#include <QAbstractItemModel>
#include <QAbstractSpinBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMenu>
#include <QShortcut>
#include <QSortFilterProxyModel>
#include <QTextEdit>
#include <QTimer>
#include "Code/QRDUtils.h"
#include "Code/Resources.h"
#include "Widgets/Extended/RDHeaderView.h"
#include "Widgets/Extended/RDListWidget.h"
#include "flowlayout/FlowLayout.h"
#include "scintilla/include/qt/ScintillaEdit.h"
#include "ui_EventBrowser.h"

enum
{
  COL_NAME,
  COL_EID,
  COL_DRAW,
  COL_DURATION,
  COL_COUNT,
};

enum
{
  ROLE_SELECTED_EID = Qt::UserRole,
  ROLE_EFFECTIVE_EID,
};

static uint32_t GetSelectedEID(QModelIndex idx)
{
  return idx.data(ROLE_SELECTED_EID).toUInt();
}

static uint32_t GetEffectiveEID(QModelIndex idx)
{
  return idx.data(ROLE_EFFECTIVE_EID).toUInt();
}

struct EventItemModel : public QAbstractItemModel
{
  EventItemModel(QAbstractItemView *view, ICaptureContext &ctx) : m_View(view), m_Ctx(ctx)
  {
    UpdateDurationColumn();

    m_CurrentEID = createIndex(0, 0, TagCaptureStart);
  }

  void ResetModel()
  {
    emit beginResetModel();
    emit endResetModel();

    m_Nodes.clear();
    m_RowInParentCache.clear();
    m_EIDNameCache.clear();
    m_Draws.clear();

    if(!m_Ctx.CurDrawcalls().empty())
      m_Nodes[0] = CreateDrawNode(NULL);

    m_CurrentEID = createIndex(0, 0, TagCaptureStart);

    RefreshCache();
  }

  void RefreshCache()
  {
    if(!m_Ctx.IsCaptureLoaded())
      return;

    uint32_t eid = m_Ctx.CurSelectedEvent();

    m_MessageCounts[eid] = m_Ctx.CurPipelineState().GetShaderMessages().count();
    m_EIDNameCache.remove(eid);

    if(eid != data(m_CurrentEID, ROLE_SELECTED_EID) || eid == 0)
    {
      QModelIndex oldCurrent = m_CurrentEID;

      m_CurrentEID = GetIndexForEID(eid);

      if(eid == 0 && m_Ctx.CurEvent() != 0)
        m_CurrentEID = createIndex(0, 0, TagRoot);

      RefreshIcon(oldCurrent);
      RefreshIcon(m_CurrentEID);
    }

    if(m_Ctx.GetBookmarks() != m_Bookmarks)
    {
      rdcarray<QModelIndex> indices;
      indices.swap(m_BookmarkIndices);

      m_Bookmarks = m_Ctx.GetBookmarks();

      for(const EventBookmark &b : m_Bookmarks)
        m_BookmarkIndices.push_back(GetIndexForEID(b.eventId));

      for(QModelIndex idx : indices)
        RefreshIcon(idx);
      for(QModelIndex idx : m_BookmarkIndices)
        RefreshIcon(idx);
    }
  }

  bool HasTimes() { return !m_Times.empty(); }
  void SetTimes(const rdcarray<CounterResult> &times)
  {
    // set all times for events to -1.0
    m_Times.fill(m_Nodes[0].effectiveEID + 1, -1.0);

    // fill in the actual times
    for(const CounterResult &r : times)
    {
      if(r.eventId < m_Times.size())
      {
        m_Times[r.eventId] = r.value.d;
      }
    }

    // iterate nodes in reverse order, because parent nodes will always be before children
    // so we know we'll have the results
    QList<uint32_t> nodeEIDs = m_Nodes.keys();
    for(auto it = nodeEIDs.rbegin(); it != nodeEIDs.rend(); it++)
      CalculateTotalDuration(m_Nodes[*it]);

    // Qt's item model kind of sucks and doesn't have a good way to say "all data in this column has
    // changed" let alone "all data has changed". dataChanged() is limited to only a group of model
    // indices under a single parent. Instead we just force the view itself to refresh here.
    m_View->viewport()->update();
  }

  void UpdateDurationColumn()
  {
    m_TimeUnit = m_Ctx.Config().EventBrowser_TimeUnit;
    emit headerDataChanged(Qt::Horizontal, COL_DURATION, COL_DURATION);

    m_View->viewport()->update();
  }

  void SetFindText(QString text)
  {
    if(m_FindString == text)
      return;

    rdcarray<QModelIndex> oldResults;
    oldResults.swap(m_FindResults);

    m_FindString = text;
    m_FindResults.clear();

    if(!m_FindString.isEmpty())
    {
      // do a depth-first search to find results
      AccumulateFindResults(createIndex(0, 0, TagRoot));
    }

    for(QModelIndex i : oldResults)
      RefreshIcon(i);
    for(QModelIndex i : m_FindResults)
      RefreshIcon(i);
  }

  QModelIndex Find(bool forward)
  {
    if(m_FindResults.empty())
      return QModelIndex();

    // if we're already on a find result we can just zoom to the next one
    int idx = m_FindResults.indexOf(m_CurrentEID);
    if(idx >= 0)
    {
      if(forward)
        idx++;
      else
        idx--;
      if(idx < 0)
        idx = m_FindResults.count() - 1;

      idx %= m_FindResults.count();

      return m_FindResults[idx];
    }

    // otherwise we need to do a more expensive search. Get the EID for the current, and find the
    // next find result after that (wrapping around)
    uint32_t eid = data(m_CurrentEID, ROLE_EFFECTIVE_EID).toUInt();

    if(forward)
    {
      // find the first result >= our selected EID
      for(int i = 0; i < m_FindResults.count(); i++)
      {
        uint32_t findEID = data(m_FindResults[i], ROLE_SELECTED_EID).toUInt();
        if(findEID >= eid)
          return m_FindResults[i];
      }

      // if we didn't find any, we're past all the results - return the first one to wrap
      return m_FindResults[0];
    }
    else
    {
      // find the last result <= our selected EID
      for(int i = m_FindResults.count() - 1; i >= 0; i--)
      {
        uint32_t findEID = data(m_FindResults[i], ROLE_SELECTED_EID).toUInt();
        if(findEID <= eid)
          return m_FindResults[i];
      }

      // if we didn't find any, we're before all the results - return the last one to wrap
      return m_FindResults.back();
    }
  }

  int NumFindResults() { return m_FindResults.count(); }
  QModelIndex GetIndexForEID(uint32_t eid)
  {
    if(eid == 0)
      return createIndex(0, 0, TagCaptureStart);

    const DrawcallDescription *draw = m_Draws[eid];
    if(draw)
    {
      // this function is not called regularly (anywhere to do with painting) but only occasionally
      // to find an index by EID, so we do an 'expensive' search for the row in the parent. The
      // cache is LRU and holds a few entries in case the user is jumping back and forth between a
      // few.
      for(size_t i = 0; i < m_RowInParentCache.size(); i++)
      {
        if(m_RowInParentCache[i].first == eid)
          return createIndex(m_RowInParentCache[i].second, 0, eid);
      }

      const int MaxCacheSize = 10;

      // oldest entry is on the back, pop it
      if(m_RowInParentCache.size() == MaxCacheSize)
        m_RowInParentCache.pop_back();

      // account for the Capture Start row we'll add at the top level
      int rowInParent = draw->parent ? 0 : 1;

      const rdcarray<DrawcallDescription> &draws =
          draw->parent ? draw->parent->children : m_Ctx.CurDrawcalls();

      for(const DrawcallDescription &d : draws)
      {
        // the first draw with an EID greater than the one we're searching for should contain it.
        if(d.eventId >= eid)
        {
          // keep counting until we get to the row within this draw
          for(size_t i = 0; i < d.events.size(); i++)
          {
            if(d.events[i].eventId < eid)
            {
              rowInParent++;
              continue;
            }

            if(d.events[i].eventId != eid)
              qCritical() << "Couldn't find event" << eid << "within draw" << draw->eventId;

            // stop, we should be at the event now
            break;
          }

          break;
        }

        rowInParent += d.events.count();
      }

      // insert the new element on the front of the cache
      m_RowInParentCache.insert(0, {eid, rowInParent});

      return createIndex(rowInParent, 0, eid);
    }
    else
    {
      qCritical() << "Couldn't find draw for event" << eid;
    }

    return QModelIndex();
  }

  const DrawcallDescription *GetDrawcallForEID(uint32_t eid)
  {
    if(eid < m_Draws.size())
      return m_Draws[eid];

    return NULL;
  }

  //////////////////////////////////////////////////////////////////////////////////////
  //
  // QAbstractItemModel methods

  QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override
  {
    if(!m_Ctx.IsCaptureLoaded())
      return QModelIndex();

    // create fake root if the parent is invalid
    if(!parent.isValid())
      return createIndex(0, column, TagRoot);

    // if the parent is the root, return the index for the specified child if it's in bounds
    if(parent.internalId() == TagRoot)
    {
      // first child is the capture start
      if(row == 0)
        return createIndex(row, column, TagCaptureStart);

      // the rest are in the root draw node
      return GetIndexForDrawChildRow(m_Nodes[0], row, column);
    }
    else if(parent.internalId() == TagCaptureStart)
    {
      // no children for this
      return QModelIndex();
    }
    else
    {
      // otherwise the parent is a real draw
      auto it = m_Nodes.find(parent.internalId());
      if(it != m_Nodes.end())
        return GetIndexForDrawChildRow(*it, row, column);
    }

    return QModelIndex();
  }

  QModelIndex parent(const QModelIndex &index) const override
  {
    if(!m_Ctx.IsCaptureLoaded() || !index.isValid() || index.internalId() == TagRoot)
      return QModelIndex();

    // Capture Start's parent is the root
    if(index.internalId() == TagCaptureStart)
      return createIndex(0, 0, TagRoot);

    // otherwise it's a draw
    const DrawcallDescription *draw = m_Draws[index.internalId()];

    // if it has no parent draw, the parent is the root
    if(draw->parent == NULL)
      return createIndex(0, 0, TagRoot);

    // the parent must be a node since it has at least one child (this draw), so we'll have a cached
    // index
    return m_Nodes[draw->parent->eventId].index;
  }

  int rowCount(const QModelIndex &parent = QModelIndex()) const override
  {
    if(!m_Ctx.IsCaptureLoaded())
      return 0;

    // only one root
    if(!parent.isValid())
      return 1;

    // only column 1 has children (Qt convention)
    if(parent.column() != 0)
      return 0;

    if(parent.internalId() == TagRoot)
      return m_Nodes[0].rowCount;

    if(parent.internalId() == TagCaptureStart)
      return 0;

    // otherwise it's an event
    uint32_t eid = (uint32_t)parent.internalId();

    // if it's a node, return the row count
    auto it = m_Nodes.find(eid);
    if(it != m_Nodes.end())
      return it->rowCount;

    // no other nodes have children
    return 0;
  }

  int columnCount(const QModelIndex &parent = QModelIndex()) const override { return COL_COUNT; }
  QVariant headerData(int section, Qt::Orientation orientation, int role) const override
  {
    if(orientation == Qt::Horizontal && role == Qt::DisplayRole)
    {
      switch(section)
      {
        case COL_NAME: return tr("Name");
        case COL_EID: return lit("EID");
        case COL_DRAW: return lit("Draw #");
        case COL_DURATION: return tr("Duration (%1)").arg(UnitSuffix(m_TimeUnit));
        default: break;
      }
    }

    return QVariant();
  }

  QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override
  {
    if(!index.isValid())
      return QVariant();

    if(role == Qt::DecorationRole)
    {
      if(index == m_CurrentEID)
        return Icons::flag_green();
      else if(m_BookmarkIndices.contains(index))
        return Icons::asterisk_orange();
      else if(m_FindResults.contains(index))
        return Icons::find();
      return QVariant();
    }

    if(index.column() == COL_DURATION && role == Qt::TextAlignmentRole)
      return int(Qt::AlignRight | Qt::AlignVCenter);

    if(index.internalId() == TagRoot)
    {
      if(role == Qt::DisplayRole && index.column() == COL_NAME)
      {
        uint32_t frameNumber = m_Ctx.FrameInfo().frameNumber;
        return frameNumber == ~0U ? tr("User-defined Capture") : tr("Frame #%1").arg(frameNumber);
      }

      if(role == ROLE_SELECTED_EID)
        return 0;

      if(role == ROLE_EFFECTIVE_EID)
        return m_Nodes[0].effectiveEID;

      if(index.column() == COL_DURATION && role == Qt::DisplayRole)
        return FormatDuration(0);
    }
    else if(index.internalId() == TagCaptureStart)
    {
      if(role == Qt::DisplayRole)
      {
        switch(index.column())
        {
          case COL_NAME: return tr("Capture Start");
          case COL_EID: return lit("0");
          case COL_DRAW: return lit("0");
          default: break;
        }
      }

      if(role == ROLE_SELECTED_EID || role == ROLE_EFFECTIVE_EID)
        return 0;

      if(index.column() == COL_DURATION && role == Qt::DisplayRole)
        return FormatDuration(~0U);
    }
    else
    {
      uint32_t eid = index.internalId();

      if(role == ROLE_SELECTED_EID)
        return eid;

      if(role == ROLE_EFFECTIVE_EID)
      {
        auto it = m_Nodes.find(eid);
        if(it != m_Nodes.end())
          return it->effectiveEID;
        return eid;
      }

      if(index.column() == COL_DURATION && role == Qt::DisplayRole)
        return FormatDuration(eid);

      if(role == Qt::DisplayRole)
      {
        const DrawcallDescription *draw = m_Draws[eid];

        switch(index.column())
        {
          case COL_NAME: return GetCachedEIDName(eid);
          case COL_EID: return eid;
          case COL_DRAW: return draw->eventId == eid ? QVariant(draw->drawcallId) : QVariant();
          default: break;
        }
      }
      else if(role == Qt::BackgroundRole || role == Qt::ForegroundRole ||
              role == RDTreeView::TreeLineColorRole)
      {
        if(m_Ctx.Config().EventBrowser_ApplyColors)
        {
          if(!m_Ctx.Config().EventBrowser_ColorEventRow && role != RDTreeView::TreeLineColorRole)
            return QVariant();

          const DrawcallDescription *draw = m_Draws[eid];

          // skip events that aren't the actual draw
          if(draw->eventId != eid)
            return QVariant();

          // if alpha isn't 0, assume the colour is valid
          if((draw->flags & (DrawFlags::PushMarker | DrawFlags::SetMarker)) &&
             draw->markerColor.w > 0.0f)
          {
            QColor col =
                QColor::fromRgb(qRgb(draw->markerColor.x * 255.0f, draw->markerColor.y * 255.0f,
                                     draw->markerColor.z * 255.0f));

            if(role == Qt::BackgroundRole || role == RDTreeView::TreeLineColorRole)
              return QBrush(col);
            else if(role == Qt::ForegroundRole)
              return QBrush(contrastingColor(col, m_View->palette().color(QPalette::Text)));
          }
        }
      }
    }

    return QVariant();
  }

private:
  ICaptureContext &m_Ctx;

  QAbstractItemView *m_View;

  static const quintptr TagRoot = quintptr(UINTPTR_MAX);
  static const quintptr TagCaptureStart = quintptr(0);

  rdcarray<double> m_Times;
  TimeUnit m_TimeUnit = TimeUnit::Count;

  QModelIndex m_CurrentEID;
  rdcarray<EventBookmark> m_Bookmarks;
  rdcarray<QModelIndex> m_BookmarkIndices;
  QString m_FindString;
  rdcarray<QModelIndex> m_FindResults;

  QMap<uint32_t, int> m_MessageCounts;

  // captures could have a lot of events, 1 million is high but not unreasonable and certainly not
  // an upper bound. We store only 1 pointer per event which gives us reasonable memory usage (i.e.
  // 8MB for that 1-million case on 64-bit) and still lets us look up what we need in O(1) and avoid
  // more expensive lookups when we need the properties for an event.
  // This gives us a pointer for every event ID pointing to the draw that contains it.
  rdcarray<const DrawcallDescription *> m_Draws;

  // we can have a bigger structure for every nested node (i.e. draw with children).
  // This drastically limits how many we need to worry about - some hierarchies have more nodes than
  // others but even in hierarchies with lots of nodes the number is small, compared to
  // draws/events.
  // we cache this with a depth-first search at init time while populating m_Draws
  struct DrawTreeNode
  {
    const DrawcallDescription *draw;
    uint32_t effectiveEID;

    // cache the index for this node to make parent() significantly faster
    QModelIndex index;

    // this is the number of child events, meaning all the draws and all of their events, but *not*
    // the events in any of their children.
    uint32_t rowCount;

    // this is a cache of row index to draw. Rather than being present for every row, this is
    // spaced out such that there are roughly Row2EIDFactor entries at most. This means that the
    // O(n) lookup to find the EID for a given row has to process that many times less entries since
    // it can jump to somewhere nearby.
    //
    // The key is the row, the value is the index in the list of children of the draw
    QMap<int, size_t> row2draw;
    static const int Row2DrawFactor = 100;
  };
  QMap<uint32_t, DrawTreeNode> m_Nodes;

  // a cache of EID -> row in parent for looking up indices for arbitrary EIDs.
  rdcarray<rdcpair<uint32_t, int>> m_RowInParentCache;

  // needs to be mutable because we update this inside data()
  mutable QMap<uint32_t, QVariant> m_EIDNameCache;

  void AccumulateFindResults(QModelIndex root)
  {
    for(int i = 0, count = rowCount(root); i < count; i++)
    {
      QModelIndex idx = index(i, 0, root);

      // check if there's a match first
      QString name = data(idx).toString();

      if(name.contains(m_FindString, Qt::CaseInsensitive))
        m_FindResults.push_back(idx);

      // now recurse
      AccumulateFindResults(idx);
    }
  }

  void RefreshIcon(QModelIndex idx)
  {
    emit dataChanged(idx.sibling(idx.row(), 0), idx.sibling(idx.row(), COL_COUNT - 1),
                     {Qt::DecorationRole, Qt::DisplayRole});
  }

  QVariant FormatDuration(uint32_t eid) const
  {
    if(m_Times.empty())
      return lit("---");

    double secs = eid < m_Times.size() ? m_Times[eid] : -1.0;

    if(secs < 0.0)
      return QVariant();

    if(m_TimeUnit == TimeUnit::Milliseconds)
      secs *= 1000.0;
    else if(m_TimeUnit == TimeUnit::Microseconds)
      secs *= 1000000.0;
    else if(m_TimeUnit == TimeUnit::Nanoseconds)
      secs *= 1000000000.0;

    return Formatter::Format(secs);
  }

  void CalculateTotalDuration(DrawTreeNode &node)
  {
    const rdcarray<DrawcallDescription> &drawChildren =
        node.draw ? node.draw->children : m_Ctx.CurDrawcalls();

    double duration = 0.0;

    for(const DrawcallDescription &d : drawChildren)
    {
      // ignore out of bounds EIDs - should not happen
      if(d.eventId >= m_Times.size())
        continue;

      // add the time for this event, if it's non-negative. Because we fill out nodes in reverse
      // order, any children that are nodes themselves should be populated by now
      duration += qMax(0.0, m_Times[d.eventId]);
    }

    m_Times[node.draw ? node.draw->eventId : 0] = duration;
  }

  DrawTreeNode CreateDrawNode(const DrawcallDescription *draw)
  {
    const rdcarray<DrawcallDescription> &drawRange = draw ? draw->children : m_Ctx.CurDrawcalls();

    // account for the Capture Start row we'll add at the top level
    int row = draw ? 0 : 1;

    DrawTreeNode ret;

    ret.draw = draw;
    ret.effectiveEID = drawRange.back().eventId;

    ret.row2draw[0] = 0;

    uint32_t row2eidStride = (drawRange.count() / DrawTreeNode::Row2DrawFactor) + 1;

    for(int i = 0; i < drawRange.count(); i++)
    {
      const DrawcallDescription &d = drawRange[i];

      if((i % row2eidStride) == 0)
        ret.row2draw[row] = i;

      for(const APIEvent &e : d.events)
      {
        m_Draws.resize_for_index(e.eventId);
        m_Draws[e.eventId] = &d;
      }

      row += d.events.count();

      if(d.children.empty())
        continue;

      DrawTreeNode node = CreateDrawNode(&d);

      node.index = createIndex(row - 1, 0, d.eventId);

      if(d.eventId == ret.effectiveEID)
        ret.effectiveEID = node.effectiveEID;

      m_Nodes[d.eventId] = node;
    }

    ret.rowCount = row;

    return ret;
  }

  QModelIndex GetIndexForDrawChildRow(const DrawTreeNode &node, int row, int column) const
  {
    // if the row is out of bounds, bail
    if(row < 0 || (uint32_t)row >= node.rowCount)
      return QModelIndex();

    // we do the linear 'counting' within the draws list to find the event at the given row. It's
    // still essentially O(n) however we keep a limited cached of skip entries at each draw node
    // which helps lower the cost significantly (by a factor of the roughly number of skip entries
    // we store).

    const rdcarray<DrawcallDescription> &draws =
        node.draw ? node.draw->children : m_Ctx.CurDrawcalls();
    int curRow = 0;
    size_t curDraw = 0;

    // lowerBound doesn't do exactly what we want, we want the first row that is less-equal. So
    // instead we use upperBound and go back one step if we don't get returned the first entry
    auto it = node.row2draw.upperBound(row);
    if(it != node.row2draw.begin())
      --it;

    // start at the first skip before the desired row
    curRow = it.key();
    curDraw = it.value();

    if(curRow > row)
    {
      // we should always have a skip, even if it's only the first child
      qCritical() << "Couldn't find skip for" << row << "in draw node"
                  << (node.draw ? node.draw->eventId : 0);

      // start at the first child row instead
      curRow = 0;
      curDraw = 0;
    }

    // if this draw doesn't contain the desired row, advance
    while(curDraw < draws.size() && curRow + draws[curDraw].events.count() <= row)
    {
      curRow += draws[curDraw].events.count();
      curDraw++;
    }

    // we've iterated all the draws and didn't come up with this row - but we checked above that
    // we're in bounds of rowCount so something went wrong
    if(curDraw >= draws.size())
    {
      qCritical() << "Couldn't find draw containing row" << row << "in draw node"
                  << (node.draw ? node.draw->eventId : 0);
      return QModelIndex();
    }

    // now curDraw contains the row at the idx'th event
    int idx = row - curRow;

    if(idx < 0 || idx >= draws[curDraw].events.count())
    {
      qCritical() << "Got invalid relative index for row" << row << "in draw node"
                  << (node.draw ? node.draw->eventId : 0);
      return QModelIndex();
    }

    return createIndex(row, column, draws[curDraw].events[idx].eventId);
  }

  QVariant GetCachedEIDName(uint32_t eid) const
  {
    auto it = m_EIDNameCache.find(eid);
    if(it != m_EIDNameCache.end())
      return it.value();

    const DrawcallDescription *draw = m_Draws[eid];

    QString name;

    if(draw->eventId == eid)
    {
      name = draw->name;
    }
    else
    {
      for(const APIEvent &e : draw->events)
      {
        if(e.eventId == eid)
        {
          name = m_Ctx.GetStructuredFile().chunks[e.chunkIndex]->name;
          break;
        }
      }

      if(name.isEmpty())
        qCritical() << "Couldn't find APIEvent for" << eid;
    }

    if(m_MessageCounts.contains(eid))
    {
      int count = m_MessageCounts[eid];
      if(count > 0)
        name += lit(" __rd_msgs::%1:%2").arg(eid).arg(count);
    }

    QVariant v = name;

    RichResourceTextInitialise(v, &m_Ctx);

    m_EIDNameCache[eid] = v;

    return v;
  }
};

struct EventFilterModel : public QSortFilterProxyModel
{
  EventFilterModel(ICaptureContext &ctx) : m_Ctx(ctx) {}
private:
  ICaptureContext &m_Ctx;
};

static bool textEditControl(QWidget *sender)
{
  if(qobject_cast<QLineEdit *>(sender) || qobject_cast<QTextEdit *>(sender) ||
     qobject_cast<QAbstractSpinBox *>(sender) || qobject_cast<ScintillaEditBase *>(sender))
  {
    return true;
  }

  QComboBox *combo = qobject_cast<QComboBox *>(sender);
  if(combo && combo->isEditable())
    return true;

  return false;
}

EventBrowser::EventBrowser(ICaptureContext &ctx, QWidget *parent)
    : QFrame(parent), ui(new Ui::EventBrowser), m_Ctx(ctx)
{
  ui->setupUi(this);

  clearBookmarks();

  m_Model = new EventItemModel(ui->events, m_Ctx);
  m_FilterModel = new EventFilterModel(m_Ctx);
  m_FilterModel->setSourceModel(m_Model);

  ui->events->setModel(m_FilterModel);

  m_delegate = new RichTextViewDelegate(ui->events);
  ui->events->setItemDelegate(m_delegate);

  ui->jumpToEID->setFont(Formatter::PreferredFont());
  ui->find->setFont(Formatter::PreferredFont());
  ui->events->setFont(Formatter::PreferredFont());

  ui->events->setHeader(new RDHeaderView(Qt::Horizontal, this));
  ui->events->header()->setStretchLastSection(true);
  ui->events->header()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);

  // we set up the name column as column 0 so that it gets the tree controls.
  ui->events->header()->setSectionResizeMode(COL_NAME, QHeaderView::Interactive);
  ui->events->header()->setSectionResizeMode(COL_EID, QHeaderView::Interactive);
  ui->events->header()->setSectionResizeMode(COL_DRAW, QHeaderView::Interactive);
  ui->events->header()->setSectionResizeMode(COL_DURATION, QHeaderView::Interactive);

  ui->events->header()->setMinimumSectionSize(40);

  ui->events->header()->setSectionsMovable(true);

  ui->events->header()->setCascadingSectionResizes(false);

  ui->events->setItemVerticalMargin(0);
  ui->events->setIgnoreIconSize(true);

  ui->events->setColoredTreeLineWidth(3.0f);

  // set up default section layout. This will be overridden in restoreState()
  ui->events->header()->resizeSection(COL_EID, 80);
  ui->events->header()->resizeSection(COL_DRAW, 60);
  ui->events->header()->resizeSection(COL_NAME, 200);
  ui->events->header()->resizeSection(COL_DURATION, 80);

  ui->events->header()->hideSection(COL_DRAW);
  ui->events->header()->hideSection(COL_DURATION);

  ui->events->header()->moveSection(COL_NAME, 2);

  UpdateDurationColumn();

  m_FindHighlight = new QTimer(this);
  m_FindHighlight->setInterval(400);
  m_FindHighlight->setSingleShot(true);
  connect(m_FindHighlight, &QTimer::timeout, this, &EventBrowser::findHighlight_timeout);

  QObject::connect(ui->closeFind, &QToolButton::clicked, this, &EventBrowser::on_HideFindJump);
  QObject::connect(ui->closeJump, &QToolButton::clicked, this, &EventBrowser::on_HideFindJump);
  QObject::connect(ui->events, &RDTreeView::keyPress, this, &EventBrowser::events_keyPress);
  QObject::connect(ui->events->selectionModel(), &QItemSelectionModel::currentChanged, this,
                   &EventBrowser::events_currentChanged);
  ui->jumpStrip->hide();
  ui->findStrip->hide();
  ui->bookmarkStrip->hide();

  m_BookmarkStripLayout = new FlowLayout(ui->bookmarkStrip, 0, 3, 3);
  m_BookmarkSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

  ui->bookmarkStrip->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
  m_BookmarkStripLayout->addWidget(ui->bookmarkStripHeader);
  m_BookmarkStripLayout->addItem(m_BookmarkSpacer);

  Qt::Key keys[] = {
      Qt::Key_1, Qt::Key_2, Qt::Key_3, Qt::Key_4, Qt::Key_5,
      Qt::Key_6, Qt::Key_7, Qt::Key_8, Qt::Key_9, Qt::Key_0,
  };
  for(int i = 0; i < 10; i++)
  {
    ctx.GetMainWindow()->RegisterShortcut(QKeySequence(keys[i] | Qt::ControlModifier).toString(),
                                          NULL, [this, i](QWidget *) { jumpToBookmark(i); });
  }

  ctx.GetMainWindow()->RegisterShortcut(QKeySequence(Qt::Key_Left | Qt::ControlModifier).toString(),
                                        NULL, [this](QWidget *sender) {
                                          // don't apply this shortcut if we're in a text edit type
                                          // control
                                          if(textEditControl(sender))
                                            return;

                                          on_stepPrev_clicked();
                                        });

  ctx.GetMainWindow()->RegisterShortcut(
      QKeySequence(Qt::Key_Right | Qt::ControlModifier).toString(), NULL, [this](QWidget *sender) {
        // don't apply this shortcut if we're in a text edit type
        // control
        if(textEditControl(sender))
          return;

        on_stepNext_clicked();
      });

  ctx.GetMainWindow()->RegisterShortcut(QKeySequence(Qt::Key_Escape).toString(), ui->findStrip,
                                        [this](QWidget *) { on_HideFindJump(); });
  ctx.GetMainWindow()->RegisterShortcut(QKeySequence(Qt::Key_Escape).toString(), ui->jumpStrip,
                                        [this](QWidget *) { on_HideFindJump(); });

  ui->events->setContextMenuPolicy(Qt::CustomContextMenu);
  QObject::connect(ui->events, &RDTreeView::customContextMenuRequested, this,
                   &EventBrowser::events_contextMenu);

  ui->events->header()->setContextMenuPolicy(Qt::CustomContextMenu);
  QObject::connect(ui->events->header(), &QHeaderView::customContextMenuRequested, this,
                   &EventBrowser::events_contextMenu);

  {
    QMenu *extensionsMenu = new QMenu(this);

    ui->extensions->setMenu(extensionsMenu);
    ui->extensions->setPopupMode(QToolButton::InstantPopup);

    QObject::connect(extensionsMenu, &QMenu::aboutToShow, [this, extensionsMenu]() {
      extensionsMenu->clear();
      m_Ctx.Extensions().MenuDisplaying(PanelMenu::EventBrowser, extensionsMenu, ui->extensions, {});
    });
  }

  OnCaptureClosed();

  m_redPalette = palette();
  m_redPalette.setColor(QPalette::Base, Qt::red);

  m_Ctx.AddCaptureViewer(this);
}

EventBrowser::~EventBrowser()
{
  // unregister any shortcuts we registered
  Qt::Key keys[] = {
      Qt::Key_1, Qt::Key_2, Qt::Key_3, Qt::Key_4, Qt::Key_5,
      Qt::Key_6, Qt::Key_7, Qt::Key_8, Qt::Key_9, Qt::Key_0,
  };
  for(int i = 0; i < 10; i++)
  {
    m_Ctx.GetMainWindow()->UnregisterShortcut(
        QKeySequence(keys[i] | Qt::ControlModifier).toString(), NULL);
  }

  m_Ctx.GetMainWindow()->UnregisterShortcut(
      QKeySequence(Qt::Key_Left | Qt::ControlModifier).toString(), NULL);

  m_Ctx.GetMainWindow()->UnregisterShortcut(
      QKeySequence(Qt::Key_Right | Qt::ControlModifier).toString(), NULL);

  m_Ctx.GetMainWindow()->UnregisterShortcut(QString(), ui->findStrip);
  m_Ctx.GetMainWindow()->UnregisterShortcut(QString(), ui->jumpStrip);

  m_Ctx.BuiltinWindowClosed(this);
  m_Ctx.RemoveCaptureViewer(this);
  delete ui;
}

void EventBrowser::OnCaptureLoaded()
{
  ui->events->setIgnoreBackgroundColors(!m_Ctx.Config().EventBrowser_ColorEventRow);

  // older Qt versions lose all the sections when a model resets even if the sections don't change.
  // Manually save/restore them
  QVariant p = persistData();
  m_Model->ResetModel();
  setPersistData(p);

  // expand the root frame node
  ui->events->expand(ui->events->model()->index(0, 0));

  clearBookmarks();
  repopulateBookmarks();

  ui->find->setEnabled(true);
  ui->gotoEID->setEnabled(true);
  ui->timeDraws->setEnabled(true);
  ui->bookmark->setEnabled(true);
  ui->exportDraws->setEnabled(true);
  ui->stepPrev->setEnabled(true);
  ui->stepNext->setEnabled(true);
}

void EventBrowser::OnCaptureClosed()
{
  clearBookmarks();

  on_HideFindJump();

  // older Qt versions lose all the sections when a model resets even if the sections don't change.
  // Manually save/restore them
  QVariant p = persistData();
  m_Model->ResetModel();
  setPersistData(p);

  ui->find->setEnabled(false);
  ui->gotoEID->setEnabled(false);
  ui->timeDraws->setEnabled(false);
  ui->bookmark->setEnabled(false);
  ui->exportDraws->setEnabled(false);
  ui->stepPrev->setEnabled(false);
  ui->stepNext->setEnabled(false);
}

void EventBrowser::OnEventChanged(uint32_t eventId)
{
  SelectEvent(eventId);
  repopulateBookmarks();
  highlightBookmarks();

  m_Model->RefreshCache();
}

void EventBrowser::on_find_clicked()
{
  ui->jumpStrip->hide();
  ui->findStrip->show();
  ui->findEvent->setFocus();
}

void EventBrowser::on_gotoEID_clicked()
{
  ui->jumpStrip->show();
  ui->findStrip->hide();
  ui->jumpToEID->setFocus();
}

void EventBrowser::on_bookmark_clicked()
{
  QModelIndex idx = ui->events->currentIndex();

  if(idx.isValid())
  {
    EventBookmark mark(GetSelectedEID(idx));

    if(m_Ctx.GetBookmarks().contains(mark))
      m_Ctx.RemoveBookmark(mark.eventId);
    else
      m_Ctx.SetBookmark(mark);
  }
}

void EventBrowser::on_timeDraws_clicked()
{
  ANALYTIC_SET(UIFeatures.DrawcallTimes, true);

  ui->events->header()->showSection(COL_DURATION);

  m_Ctx.Replay().AsyncInvoke([this](IReplayController *r) {
    m_Model->SetTimes(r->FetchCounters({GPUCounter::EventGPUDuration}));

    GUIInvoke::call(this, [this]() { ui->events->update(); });
  });
}

void EventBrowser::events_currentChanged(const QModelIndex &current, const QModelIndex &previous)
{
  if(!current.isValid())
    return;

  uint32_t selectedEID = GetSelectedEID(current);
  uint32_t effectiveEID = GetEffectiveEID(current);

  m_Ctx.SetEventID({this}, selectedEID, effectiveEID);

  m_Model->RefreshCache();

  const DrawcallDescription *draw = m_Ctx.GetDrawcall(effectiveEID);

  ui->stepPrev->setEnabled(draw && draw->previous);
  ui->stepNext->setEnabled(draw && draw->next);

  // special case for the first draw in the frame
  if(effectiveEID == 0)
    ui->stepNext->setEnabled(true);

  // special case for the first 'virtual' draw at EID 0
  if(m_Ctx.GetFirstDrawcall() && effectiveEID == m_Ctx.GetFirstDrawcall()->eventId)
    ui->stepPrev->setEnabled(true);

  highlightBookmarks();
}

void EventBrowser::on_HideFindJump()
{
  ui->jumpStrip->hide();
  ui->findStrip->hide();

  ui->jumpToEID->setText(QString());

  ui->findEvent->setText(QString());
  m_Model->SetFindText(QString());
  updateFindResultsAvailable();
}

void EventBrowser::on_jumpToEID_returnPressed()
{
  bool ok = false;
  uint eid = ui->jumpToEID->text().toUInt(&ok);
  if(ok)
  {
    SelectEvent(eid);
  }
}

void EventBrowser::findHighlight_timeout()
{
  m_Model->SetFindText(ui->findEvent->text());
  SelectEvent(m_FilterModel->mapFromSource(m_Model->Find(true)));
  updateFindResultsAvailable();
}

void EventBrowser::on_findEvent_textEdited(const QString &arg1)
{
  if(arg1.isEmpty())
  {
    m_FindHighlight->stop();

    m_Model->SetFindText(QString());
    updateFindResultsAvailable();
  }
  else
  {
    m_FindHighlight->start();    // restart
  }
}

void EventBrowser::on_findEvent_returnPressed()
{
  // stop the timer, we'll manually fire it instantly
  if(m_FindHighlight->isActive())
    m_FindHighlight->stop();

  findHighlight_timeout();
}

void EventBrowser::on_findEvent_keyPress(QKeyEvent *event)
{
  if(event->key() == Qt::Key_F3)
  {
    // stop the timer, we'll manually fire it instantly
    if(m_FindHighlight->isActive())
      m_FindHighlight->stop();

    if(!ui->findEvent->text().isEmpty())
      SelectEvent(m_FilterModel->mapFromSource(
          m_Model->Find(event->modifiers() & Qt::ShiftModifier ? false : true)));

    event->accept();
  }
}

void EventBrowser::on_findNext_clicked()
{
  SelectEvent(m_FilterModel->mapFromSource(m_Model->Find(true)));
  updateFindResultsAvailable();
}

void EventBrowser::on_findPrev_clicked()
{
  SelectEvent(m_FilterModel->mapFromSource(m_Model->Find(false)));
  updateFindResultsAvailable();
}

void EventBrowser::on_stepNext_clicked()
{
  if(!m_Ctx.IsCaptureLoaded() || !ui->stepNext->isEnabled())
    return;

  const DrawcallDescription *draw = m_Ctx.CurDrawcall();

  if(draw && draw->next)
    SelectEvent(draw->next->eventId);

  // special case for the first 'virtual' draw at EID 0
  if(m_Ctx.CurEvent() == 0)
    SelectEvent(m_Ctx.GetFirstDrawcall()->eventId);
}

void EventBrowser::on_stepPrev_clicked()
{
  if(!m_Ctx.IsCaptureLoaded() || !ui->stepPrev->isEnabled())
    return;

  const DrawcallDescription *draw = m_Ctx.CurDrawcall();

  if(draw && draw->previous)
    SelectEvent(draw->previous->eventId);

  // special case for the first 'virtual' draw at EID 0
  if(m_Ctx.CurEvent() == m_Ctx.GetFirstDrawcall()->eventId)
    SelectEvent(0);
}

void EventBrowser::on_exportDraws_clicked()
{
  QString filename =
      RDDialog::getSaveFileName(this, tr("Save Event List"), QString(), tr("Text files (*.txt)"));

  if(!filename.isEmpty())
  {
    ANALYTIC_SET(Export.EventBrowser, true);

    QDir dirinfo = QFileInfo(filename).dir();
    if(dirinfo.exists())
    {
      QFile f(filename);
      if(f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
      {
        QTextStream stream(&f);

        stream << tr("%1 - Frame #%2\n\n")
                      .arg(m_Ctx.GetCaptureFilename())
                      .arg(m_Ctx.FrameInfo().frameNumber);

        int maxNameLength = 0;

        QModelIndex root = ui->events->model()->index(0, COL_NAME);

        // skip child 0, which is the Capture Start entry that we don't want to include
        for(int i = 1, rowCount = ui->events->model()->rowCount(root); i < rowCount; i++)
          GetMaxNameLength(maxNameLength, 0, false, ui->events->model()->index(i, COL_NAME, root));

        QString line = QFormatStr(" EID  | %1 | Draw #").arg(lit("Event"), -maxNameLength);

        if(m_Model->HasTimes())
        {
          line += QFormatStr(" | %1 (%2)").arg(tr("Duration")).arg(ToQStr(m_TimeUnit));
        }

        stream << line << "\n";

        line = QFormatStr("--------%1-----------").arg(QString(), maxNameLength, QLatin1Char('-'));

        if(m_Model->HasTimes())
        {
          int maxDurationLength = 0;
          maxDurationLength = qMax(maxDurationLength, Formatter::Format(1.0).length());
          maxDurationLength = qMax(maxDurationLength, Formatter::Format(1.2345e-200).length());
          maxDurationLength =
              qMax(maxDurationLength, Formatter::Format(123456.7890123456789).length());
          line += QString(3 + maxDurationLength, QLatin1Char('-'));    // 3 extra for " | "
        }

        stream << line << "\n";

        for(int i = 1, rowCount = ui->events->model()->rowCount(root); i < rowCount; i++)
          ExportDrawcall(stream, maxNameLength, 0, false,
                         ui->events->model()->index(i, COL_NAME, root));
      }
      else
      {
        RDDialog::critical(
            this, tr("Error saving event list"),
            tr("Couldn't open path %1 for write.\n%2").arg(filename).arg(f.errorString()));
        return;
      }
    }
    else
    {
      RDDialog::critical(this, tr("Invalid directory"),
                         tr("Cannot find target directory to save to"));
      return;
    }
  }
}

void EventBrowser::on_colSelect_clicked()
{
  QStringList headers;
  for(int i = 0; i < ui->events->model()->columnCount(); i++)
    headers << ui->events->model()->headerData(i, Qt::Horizontal).toString();
  UpdateVisibleColumns(tr("Select Event Browser Columns"), COL_COUNT, ui->events->header(), headers);
}

QString EventBrowser::GetExportString(int indent, bool firstchild, const QModelIndex &idx)
{
  QString prefix = QString(indent * 2 - (firstchild ? 1 : 0), QLatin1Char(' '));
  if(firstchild)
    prefix += QLatin1Char('\\');

  return QFormatStr("%1- %2").arg(prefix).arg(
      RichResourceTextFormat(m_Ctx, idx.data(Qt::DisplayRole)));
}

void EventBrowser::GetMaxNameLength(int &maxNameLength, int indent, bool firstchild,
                                    const QModelIndex &idx)
{
  QString nameString = GetExportString(indent, firstchild, idx);

  maxNameLength = qMax(maxNameLength, nameString.count());

  firstchild = true;

  for(int i = 0, rowCount = idx.model()->rowCount(idx); i < rowCount; i++)
  {
    GetMaxNameLength(maxNameLength, indent + 1, firstchild, idx.child(i, COL_NAME));
    firstchild = false;
  }
}

void EventBrowser::ExportDrawcall(QTextStream &writer, int maxNameLength, int indent,
                                  bool firstchild, const QModelIndex &idx)
{
  QString nameString = GetExportString(indent, firstchild, idx);

  QModelIndex eidIdx = idx.model()->sibling(idx.row(), COL_EID, idx);
  QModelIndex drawIdx = idx.model()->sibling(idx.row(), COL_DRAW, idx);

  QString line = QFormatStr("%1 | %2 | %3")
                     .arg(eidIdx.data(Qt::DisplayRole).toString(), -5)
                     .arg(nameString, -maxNameLength)
                     .arg(drawIdx.data(Qt::DisplayRole).toString(), -6);

  if(m_Model->HasTimes())
  {
    QVariant duration = idx.model()->sibling(idx.row(), COL_DURATION, idx).data(Qt::DisplayRole);

    if(duration != QVariant())
    {
      line += QFormatStr(" | %1").arg(duration.toString());
    }
    else
    {
      line += lit(" |");
    }
  }

  writer << line << "\n";

  firstchild = true;

  for(int i = 0, rowCount = idx.model()->rowCount(idx); i < rowCount; i++)
  {
    ExportDrawcall(writer, maxNameLength, indent + 1, firstchild, idx.child(i, COL_NAME));
    firstchild = false;
  }
}

QVariant EventBrowser::persistData()
{
  QVariantMap state;

  // temporarily turn off stretching the last section so we can get the real sizes.
  ui->events->header()->setStretchLastSection(false);

  QVariantList columns;
  for(int i = 0; i < COL_COUNT; i++)
  {
    QVariantMap col;

    bool hidden = ui->events->header()->isSectionHidden(i);

    // we temporarily make the section visible to get its size, since otherwise it returns 0.
    // There's no other way to access the 'hidden section sizes' which are transient and will be
    // lost otherwise.
    ui->events->header()->showSection(i);
    int size = ui->events->header()->sectionSize(i);
    if(hidden)
      ui->events->header()->hideSection(i);

    // name is just informative
    col[lit("name")] = ui->events->model()->headerData(i, Qt::Horizontal);
    col[lit("index")] = ui->events->header()->visualIndex(i);
    col[lit("hidden")] = hidden;
    col[lit("size")] = size;
    columns.push_back(col);
  }

  ui->events->header()->setStretchLastSection(true);

  state[lit("columns")] = columns;

  return state;
}

void EventBrowser::setPersistData(const QVariant &persistData)
{
  QVariantMap state = persistData.toMap();

  QVariantList columns = state[lit("columns")].toList();
  for(int i = 0; i < columns.count() && i < COL_COUNT; i++)
  {
    QVariantMap col = columns[i].toMap();

    int oldVisIdx = ui->events->header()->visualIndex(i);
    int visIdx = col[lit("index")].toInt();
    int size = col[lit("size")].toInt();
    bool hidden = col[lit("hidden")].toBool();

    ui->events->header()->moveSection(oldVisIdx, visIdx);
    ui->events->header()->resizeSection(i, size);
    if(hidden)
      ui->events->header()->hideSection(i);
    else
      ui->events->header()->showSection(i);
  }
}

void EventBrowser::events_keyPress(QKeyEvent *event)
{
  if(!m_Ctx.IsCaptureLoaded())
    return;

  if(event->key() == Qt::Key_F3)
  {
    if(event->modifiers() == Qt::ShiftModifier)
      SelectEvent(m_FilterModel->mapFromSource(m_Model->Find(false)));
    else
      SelectEvent(m_FilterModel->mapFromSource(m_Model->Find(true)));
    updateFindResultsAvailable();
  }

  if(event->modifiers() == Qt::ControlModifier)
  {
    if(event->key() == Qt::Key_F)
    {
      on_find_clicked();
      event->accept();
    }
    else if(event->key() == Qt::Key_G)
    {
      on_gotoEID_clicked();
      event->accept();
    }
    else if(event->key() == Qt::Key_B)
    {
      on_bookmark_clicked();
      event->accept();
    }
    else if(event->key() == Qt::Key_T)
    {
      on_timeDraws_clicked();
      event->accept();
    }
  }
}

void EventBrowser::events_contextMenu(const QPoint &pos)
{
  QModelIndex index = ui->events->indexAt(pos);

  QMenu contextMenu(this);

  QAction expandAll(tr("&Expand All"), this);
  QAction collapseAll(tr("&Collapse All"), this);
  QAction toggleBookmark(tr("Toggle &Bookmark"), this);
  QAction selectCols(tr("&Select Columns..."), this);
  QAction rgpSelect(tr("Select &RGP Event"), this);
  rgpSelect.setIcon(Icons::connect());

  contextMenu.addAction(&expandAll);
  contextMenu.addAction(&collapseAll);
  contextMenu.addAction(&toggleBookmark);
  contextMenu.addAction(&selectCols);

  expandAll.setIcon(Icons::arrow_out());
  collapseAll.setIcon(Icons::arrow_in());
  toggleBookmark.setIcon(Icons::asterisk_orange());
  selectCols.setIcon(Icons::timeline_marker());

  expandAll.setEnabled(index.isValid() && ui->events->model()->rowCount(index) > 0);
  collapseAll.setEnabled(expandAll.isEnabled());
  toggleBookmark.setEnabled(m_Ctx.IsCaptureLoaded());

  QObject::connect(&expandAll, &QAction::triggered,
                   [this, index]() { ui->events->expandAll(index); });

  QObject::connect(&collapseAll, &QAction::triggered,
                   [this, index]() { ui->events->collapseAll(index); });

  QObject::connect(&toggleBookmark, &QAction::triggered, this, &EventBrowser::on_bookmark_clicked);

  QObject::connect(&selectCols, &QAction::triggered, this, &EventBrowser::on_colSelect_clicked);

  IRGPInterop *rgp = m_Ctx.GetRGPInterop();
  if(rgp && rgp->HasRGPEvent(m_Ctx.CurEvent()))
  {
    contextMenu.addAction(&rgpSelect);
    QObject::connect(&rgpSelect, &QAction::triggered,
                     [this, rgp]() { rgp->SelectRGPEvent(m_Ctx.CurEvent()); });
  }

  contextMenu.addSeparator();

  m_Ctx.Extensions().MenuDisplaying(ContextMenu::EventBrowser_Event, &contextMenu,
                                    {{"eventId", m_Ctx.CurEvent()}});

  RDDialog::show(&contextMenu, ui->events->viewport()->mapToGlobal(pos));
}

void EventBrowser::clearBookmarks()
{
  for(QToolButton *b : m_BookmarkButtons)
    delete b;

  m_BookmarkButtons.clear();

  ui->bookmarkStrip->setVisible(false);
}

void EventBrowser::repopulateBookmarks()
{
  const rdcarray<EventBookmark> bookmarks = m_Ctx.GetBookmarks();

  // add any bookmark markers that we don't have
  for(const EventBookmark &mark : bookmarks)
  {
    if(!m_BookmarkButtons.contains(mark.eventId))
    {
      uint32_t EID = mark.eventId;

      QToolButton *but = new QToolButton(this);

      but->setText(QString::number(EID));
      but->setCheckable(true);
      but->setAutoRaise(true);
      but->setProperty("eid", EID);
      QObject::connect(but, &QToolButton::clicked, [this, but, EID]() {
        but->setChecked(true);
        SelectEvent(EID);
        highlightBookmarks();
      });

      m_BookmarkButtons[EID] = but;

      highlightBookmarks();

      m_BookmarkStripLayout->removeItem(m_BookmarkSpacer);
      m_BookmarkStripLayout->addWidget(but);
      m_BookmarkStripLayout->addItem(m_BookmarkSpacer);
    }
  }

  // remove any bookmark markers we shouldn't have
  for(uint32_t EID : m_BookmarkButtons.keys())
  {
    if(!bookmarks.contains(EventBookmark(EID)))
    {
      delete m_BookmarkButtons[EID];
      m_BookmarkButtons.remove(EID);
    }
  }

  ui->bookmarkStrip->setVisible(!bookmarks.isEmpty());

  m_Model->RefreshCache();
}

void EventBrowser::jumpToBookmark(int idx)
{
  const rdcarray<EventBookmark> bookmarks = m_Ctx.GetBookmarks();
  if(idx < 0 || idx >= bookmarks.count() || !m_Ctx.IsCaptureLoaded())
    return;

  // don't exclude ourselves, so we're updated as normal
  SelectEvent(bookmarks[idx].eventId);
}

void EventBrowser::highlightBookmarks()
{
  for(uint32_t eid : m_BookmarkButtons.keys())
  {
    if(eid == m_Ctx.CurEvent())
      m_BookmarkButtons[eid]->setChecked(true);
    else
      m_BookmarkButtons[eid]->setChecked(false);
  }
}

void EventBrowser::ExpandNode(QModelIndex idx)
{
  QModelIndex i = idx;
  while(idx.isValid())
  {
    ui->events->expand(idx);
    idx = idx.parent();
  }

  if(i.isValid())
    ui->events->scrollTo(i);
}

bool EventBrowser::SelectEvent(uint32_t eventId)
{
  if(!m_Ctx.IsCaptureLoaded())
    return false;

  QModelIndex found = m_FilterModel->mapFromSource(m_Model->GetIndexForEID(eventId));
  if(found.isValid())
  {
    SelectEvent(found);

    return true;
  }

  return false;
}

void EventBrowser::SelectEvent(QModelIndex idx)
{
  if(!idx.isValid())
    return;

  ui->events->selectionModel()->setCurrentIndex(
      idx, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);

  ExpandNode(idx);
}

void EventBrowser::updateFindResultsAvailable()
{
  if(m_Model->NumFindResults() > 0 || ui->findEvent->text().isEmpty())
    ui->findEvent->setPalette(palette());
  else
    ui->findEvent->setPalette(m_redPalette);
}

void EventBrowser::UpdateDurationColumn()
{
  if(m_TimeUnit == m_Ctx.Config().EventBrowser_TimeUnit)
    return;

  m_TimeUnit = m_Ctx.Config().EventBrowser_TimeUnit;

  m_Model->UpdateDurationColumn();
}

APIEvent EventBrowser::GetAPIEventForEID(uint32_t eid)
{
  const DrawcallDescription *draw = GetDrawcallForEID(eid);

  if(draw)
  {
    for(const APIEvent &ev : draw->events)
    {
      if(ev.eventId == eid)
        return ev;
    }
  }

  return APIEvent();
}

const DrawcallDescription *EventBrowser::GetDrawcallForEID(uint32_t eid)
{
  return m_Model->GetDrawcallForEID(eid);
}
