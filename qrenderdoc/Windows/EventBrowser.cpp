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
#include <QScrollBar>
#include <QShortcut>
#include <QSortFilterProxyModel>
#include <QStringListModel>
#include <QStylePainter>
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
  ROLE_GROUPED_DRAWCALL,
  ROLE_EXACT_DRAWCALL,
  ROLE_CHUNK,
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

    const QColor highlight = view->palette().color(QPalette::Highlight).toHsl();

    // this is a bit arbitrary but most highlight colours are blue, and blue text looks like a link.
    // Rotate towards red (a 1/3rd turn). If the highlight colour is something different this will
    // give a similar effect, hopefully.
    qreal hue = highlight.hueF() + 0.333;
    if(hue >= 1.0f)
      hue -= 1.0;

    // saturate the color a bit
    qreal sat = qMin(1.0, 1.25 * highlight.saturationF());

    // keep the lightness the same
    qreal light = highlight.lightnessF();

    QColor paramcol;
    paramcol.setHslF(hue, sat, light);

    m_ParamColCode = QFormatStr("#%1%2%3")
                         .arg(paramcol.red(), 2, 16, QLatin1Char('0'))
                         .arg(paramcol.green(), 2, 16, QLatin1Char('0'))
                         .arg(paramcol.blue(), 2, 16, QLatin1Char('0'));
  }

  void ResetModel()
  {
    emit beginResetModel();
    emit endResetModel();

    m_Nodes.clear();
    m_RowInParentCache.clear();
    m_EIDNameCache.clear();
    m_Draws.clear();
    m_Chunks.clear();

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

    if(m_Ctx.ResourceNameCacheID() != m_RenameCacheID)
    {
      m_View->viewport()->update();
    }

    m_RenameCacheID = m_Ctx.ResourceNameCacheID();
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

  void SetShowParameterNames(bool show)
  {
    if(m_ShowParameterNames != show)
    {
      m_ShowParameterNames = show;

      m_EIDNameCache.clear();
      m_View->viewport()->update();
    }
  }

  void SetShowAllParameters(bool show)
  {
    if(m_ShowAllParameters != show)
    {
      m_ShowAllParameters = show;

      m_EIDNameCache.clear();
      m_View->viewport()->update();
    }
  }

  void SetUseCustomDrawNames(bool use)
  {
    if(m_UseCustomDrawNames != use)
    {
      m_UseCustomDrawNames = use;

      m_EIDNameCache.clear();
      m_View->viewport()->update();
    }
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

    bool eidSearch = false;
    // if the user puts in @123
    if(!text.isEmpty() && text[0] == QLatin1Char('@'))
    {
      eidSearch = true;
      text = text.mid(1);
    }

    bool eidOK = false;
    uint32_t eid = text.toUInt(&eidOK);

    // include EID in results first if the text parses as an integer
    if(eidOK && eid > 0 && eid < m_Draws.size())
    {
      // if the text doesn't exactly match the EID after converting back, don't treat this as an
      // EID-only search
      if(text.trimmed() != QString::number(eid))
        eidSearch = false;

      m_FindResults.push_back(GetIndexForEID(eid));
    }

    if(!m_FindString.isEmpty() && !eidSearch)
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
  double GetSecondsDurationForEID(uint32_t eid)
  {
    if(eid < m_Times.size())
      return m_Times[eid];

    return -1.0;
  }

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
          // except if the draw is a fake marker. In this case its own event ID is invalid, so we
          // check the range of its children (knowing it only has one layer of children)
          if(d.IsFakeMarker() && d.children[0].eventId < eid)
          {
            rowInParent++;
            continue;
          }

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

    if(index.internalId() >= m_Draws.size())
      return QModelIndex();

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

      if(eid >= m_Draws.size())
        return QVariant();

      if(role == ROLE_SELECTED_EID)
        return eid;

      if(role == ROLE_GROUPED_DRAWCALL)
        return QVariant::fromValue(qulonglong(m_Draws[eid]));

      if(role == ROLE_EXACT_DRAWCALL)
      {
        const DrawcallDescription *draw = m_Draws[eid];
        return draw->eventId == eid ? QVariant::fromValue(qulonglong(m_Draws[eid])) : QVariant();
      }

      if(role == ROLE_CHUNK)
        return QVariant::fromValue(qulonglong(m_Chunks[eid]));

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
          case COL_EID:
          case COL_DRAW:
            if(draw->eventId == eid && !draw->children.empty())
            {
              uint32_t effectiveEID = eid;
              auto it = m_Nodes.find(eid);
              if(it != m_Nodes.end())
                effectiveEID = it->effectiveEID;

              if(draw->IsFakeMarker())
                eid = draw->children[0].events[0].eventId;

              if(index.column() == COL_EID)
              {
                return eid == effectiveEID
                           ? QVariant(eid)
                           : QVariant(QFormatStr("%1-%2").arg(eid).arg(effectiveEID));
              }
              else
              {
                uint32_t drawId = draw->drawcallId;
                uint32_t endDrawId = m_Draws[effectiveEID]->drawcallId;

                if(draw->IsFakeMarker())
                  drawId = draw->children[0].drawcallId;

                return drawId == endDrawId
                           ? QVariant()
                           : QVariant(QFormatStr("%1-%2").arg(drawId).arg(endDrawId));
              }
            }

            if(index.column() == COL_EID)
              return eid;
            else
              return draw->eventId == eid &&
                             !(draw->flags &
                               (DrawFlags::SetMarker | DrawFlags::PushMarker | DrawFlags::PopMarker))
                         ? QVariant(draw->drawcallId)
                         : QVariant();
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

  int32_t m_RenameCacheID;
  QString m_ParamColCode;

  QMap<uint32_t, int> m_MessageCounts;

  // captures could have a lot of events, 1 million is high but not unreasonable and certainly not
  // an upper bound. We store only 1 pointer per event which gives us reasonable memory usage (i.e.
  // 8MB for that 1-million case on 64-bit) and still lets us look up what we need in O(1) and avoid
  // more expensive lookups when we need the properties for an event.
  // This gives us a pointer for every event ID pointing to the draw that contains it.
  rdcarray<const DrawcallDescription *> m_Draws;
  rdcarray<const SDChunk *> m_Chunks;

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

  bool m_ShowParameterNames = false;
  bool m_ShowAllParameters = false;
  bool m_UseCustomDrawNames = true;

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

    const SDFile &sdfile = m_Ctx.GetStructuredFile();

    for(int i = 0; i < drawRange.count(); i++)
    {
      const DrawcallDescription &d = drawRange[i];

      if((i % row2eidStride) == 0)
        ret.row2draw[row] = i;

      for(const APIEvent &e : d.events)
      {
        m_Draws.resize_for_index(e.eventId);
        m_Chunks.resize_for_index(e.eventId);
        m_Draws[e.eventId] = &d;
        if(e.chunkIndex != APIEvent::NoChunk && e.chunkIndex < sdfile.chunks.size())
          m_Chunks[e.eventId] = sdfile.chunks[e.chunkIndex];
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

    // markers always use the draw name no matter what (fake markers must because we don't have a
    // chunk to use to name them). This doesn't apply to 'marker' regions that are multidraws or
    // command buffer boundaries.
    if(draw->eventId == eid)
    {
      if(m_UseCustomDrawNames)
      {
        name = draw->name;
      }
      else
      {
        if((draw->flags & (DrawFlags::SetMarker | DrawFlags::PushMarker)) &&
           !(draw->flags & (DrawFlags::CommandBufferBoundary | DrawFlags::PassBoundary |
                            DrawFlags::CmdList | DrawFlags::MultiDraw)))
          name = draw->name;
      }
    }

    if(name.isEmpty())
    {
      for(const APIEvent &e : draw->events)
      {
        if(e.eventId == eid)
        {
          const SDChunk *chunk = m_Ctx.GetStructuredFile().chunks[e.chunkIndex];
          name = chunk->name;

          // don't display any "ClassName::" prefix. We keep it for the API inspector which is more
          // verbose
          int nsSep = name.indexOf(lit("::"));
          if(nsSep > 0)
            name.remove(0, nsSep + 2);

          name += QLatin1Char('(');

          bool onlyImportant(chunk->type.flags & SDTypeFlags::ImportantChildren);

          for(size_t i = 0; i < chunk->NumChildren(); i++)
          {
            if(chunk->GetChild(i)->type.flags & SDTypeFlags::Hidden)
              continue;

            const SDObject *o = chunk->GetChild(i);

            // never display hidden members
            if(o->type.flags & SDTypeFlags::Hidden)
              continue;

            if(!onlyImportant || (o->type.flags & SDTypeFlags::Important) || m_ShowAllParameters)
            {
              if(name.at(name.size() - 1) != QLatin1Char('('))
                name += lit(", ");

              if(m_ShowParameterNames)
              {
                name += lit("<span style='color: %1'>").arg(m_ParamColCode);
                name += o->name;
                name += lit("</span>");
                name += QLatin1Char('=');
              }

              name += SDObject2Variant(o, true).toString();
            }
          }

          name += QLatin1Char(')');

          break;
        }
      }

      if(name.isEmpty())
        qCritical() << "Couldn't find APIEvent for" << eid;

      // force html even for events that don't reference resources etc, to get the italics for
      // parameters
      if(m_ShowParameterNames)
        name = lit("<rdhtml>") + name + lit("</rdhtml>");
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

  friend struct EventFilterModel;
};

enum class MatchType
{
  MustMatch,
  Normal,
  CantMatch
};

struct EventFilter
{
  EventFilter(IEventBrowser::EventFilterCallback c) : callback(c), type(MatchType::Normal) {}
  EventFilter(IEventBrowser::EventFilterCallback c, MatchType t) : callback(c), type(t) {}
  IEventBrowser::EventFilterCallback callback;
  MatchType type;
};

static QMap<QString, DrawFlags> DrawFlagsLookup;
static QStringList DrawFlagsList;

void CacheDrawFlagsLookup()
{
  if(DrawFlagsLookup.empty())
  {
    for(uint32_t i = 0; i <= 31; i++)
    {
      DrawFlags flag = DrawFlags(1U << i);

      // bit of a hack, see if it's a valid flag by stringising and seeing if it contains
      // DrawFlags(
      QString str = ToQStr(flag);
      if(str.contains(lit("DrawFlags(")))
        continue;

      DrawFlagsList.push_back(str);
      DrawFlagsLookup[str.toLower()] = flag;
    }

    DrawFlagsList.sort();
  }
}

bool EvaluateFilterSet(ICaptureContext &ctx, const rdcarray<EventFilter> &filters, bool all,
                       uint32_t eid, const SDChunk *chunk, const DrawcallDescription *draw,
                       QString name)
{
  if(filters.empty())
    return true;

  bool accept = false;

  bool anyNormal = false;
  for(const EventFilter &filter : filters)
    anyNormal |= (filter.type == MatchType::Normal);

  // if there aren't any normal filters, they're all CantMatch or MustMatch. In this case we default
  // to acceptance so that if they all pass then we match. This handles cases like +foo -bar which
  // would return the default on a string like "foothing" which would return a false match
  if(!anyNormal)
    accept = true;

  // if any top-level filter matches, we match
  for(const EventFilter &filter : filters)
  {
    // if we've already accepted and this is a normal filter and we are in non-all mode, it won't
    // change the result so don't bother running the filter.
    if(accept && !all && filter.type == MatchType::Normal)
      continue;

    bool match = filter.callback(&ctx, rdcstr(), rdcstr(), eid, chunk, draw, name);

    // in normal mode, if it matches it should be included (unless a subsequent filter excludes
    // it)
    if(filter.type == MatchType::Normal)
    {
      if(match)
        accept = true;

      // in all mode, if any normal filters fails then we fail the whole thing
      if(all && !match)
        return false;
    }
    else if(filter.type == MatchType::CantMatch)
    {
      // if we matched a can't match (e.g -foo) then we can't accept this row
      if(match)
        return false;
    }
    else if(filter.type == MatchType::MustMatch)
    {
      // similarly if we *didn't* match a must match (e.g +foo) then we can't accept this row
      if(!match)
        return false;
    }
  }

  // if we didn't early out with a can't/must failure, then return the result from whatever normal
  // matches we got
  return accept;
}

static const SDObject *FindChildRecursively(const SDObject *parent, rdcstr name)
{
  const SDObject *o = parent->FindChild(name);
  if(o)
    return o;

  for(size_t i = 0; i < parent->NumChildren(); i++)
  {
    o = FindChildRecursively(parent->GetChild(i), name);
    if(o)
      return o;
  }

  return NULL;
}

struct FilterExpression
{
  MatchType matchType;

  bool function;
  QString name;

  int position = -1;
  int length = 0;

  QVector<FilterExpression> exprs;
};

struct ParseTrace
{
  int position = -1;
  int length = 0;
  QString errorText;

  QVector<FilterExpression> exprs;

  operator bool() const = delete;
  bool hasErrors() const { return length > 0; }
  ParseTrace &setError(QString text)
  {
    errorText = text;
    return *this;
  }
  void clearErrors()
  {
    errorText.clear();
    position = -1;
    length = 0;
  }
};

struct CustomFilterCallbacks
{
  IEventBrowser::EventFilterCallback filter;
  IEventBrowser::FilterParseCallback parser;
  IEventBrowser::AutoCompleteCallback completer;
};

struct BuiltinFilterCallbacks
{
  std::function<IEventBrowser::EventFilterCallback(QString, QString, ParseTrace &)> makeFilter;
  IEventBrowser::AutoCompleteCallback completer;
};

struct EventFilterModel : public QSortFilterProxyModel
{
public:
  EventFilterModel(EventItemModel *model, ICaptureContext &ctx) : m_Model(model), m_Ctx(ctx)
  {
    setSourceModel(m_Model);

    if(m_BuiltinFilters.empty())
    {
#ifndef STRINGIZE
#define STRINGIZE2(a) #a
#define STRINGIZE(a) STRINGIZE2(a)
#endif

#define MAKE_BUILTIN_FILTER(filter_name)                             \
  m_BuiltinFilters[lit(STRINGIZE(filter_name))].makeFilter = [this]( \
      QString name, QString parameters, ParseTrace &trace) {         \
    return filterFunction_##filter_name(name, parameters, trace);    \
  };

      MAKE_BUILTIN_FILTER(any);
      MAKE_BUILTIN_FILTER(all);
      MAKE_BUILTIN_FILTER(param);
      MAKE_BUILTIN_FILTER(event);
      MAKE_BUILTIN_FILTER(draw);
      MAKE_BUILTIN_FILTER(dispatch);

      m_BuiltinFilters[lit("event")].completer = [this](ICaptureContext *ctx, QString name,
                                                        QString parameters) {
        return filterCompleter_event(ctx, name, parameters);
      };
      m_BuiltinFilters[lit("draw")].completer = [this](ICaptureContext *ctx, QString name,
                                                       QString parameters) {
        return filterCompleter_draw(ctx, name, parameters);
      };
      m_BuiltinFilters[lit("dispatch")].completer = [this](ICaptureContext *ctx, QString name,
                                                           QString parameters) {
        return filterCompleter_dispatch(ctx, name, parameters);
      };
    }
  }
  void ResetCache() { m_VisibleCache.clear(); }
  ParseTrace ParseExpression(QString expr)
  {
    m_VisibleCache.clear();
    m_Filters.clear();
    ParseTrace ret = ParseExpressionToFilters(expr, m_Filters);
    if(ret.hasErrors())
      m_Filters.clear();
    invalidateFilter();
    return ret;
  }

  QStringList GetCompletions(QString filter, QString params)
  {
    IEventBrowser::AutoCompleteCallback cb;

    if(m_BuiltinFilters.contains(filter))
      cb = m_BuiltinFilters[filter].completer;
    else if(m_CustomFilters.contains(filter))
      cb = m_CustomFilters[filter].completer;

    rdcarray<rdcstr> ret;

    if(cb)
      ret = m_BuiltinFilters[filter].completer(&m_Ctx, filter, params);

    QStringList qret;
    for(const rdcstr &s : ret)
      qret << s;
    return qret;
  }

  QStringList GetBuiltinFunctions() { return m_BuiltinFilters.keys(); }
  QStringList GetCustomFunctions() { return m_CustomFilters.keys(); }
  void SetEmptyRegionsVisible(bool visible) { m_EmptyRegionsVisible = visible; }
  static bool RegisterEventFilterFunction(const rdcstr &name,
                                          IEventBrowser::EventFilterCallback filter,
                                          IEventBrowser::FilterParseCallback parser,
                                          IEventBrowser::AutoCompleteCallback completer)
  {
    if(m_BuiltinFilters.contains(name))
    {
      qCritical() << "Registering filter function" << QString(name)
                  << "which is a builtin function.";
      return false;
    }

    if(m_CustomFilters[name].filter != NULL)
    {
      qCritical() << "Registering filter function" << QString(name)
                  << "which is already registered.";
      return false;
    }

    m_CustomFilters[name] = {filter, parser, completer};
    return true;
  }

  static bool UnregisterEventFilterFunction(const rdcstr &name)
  {
    m_CustomFilters.remove(name);
    return true;
  }

protected:
  virtual bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override
  {
    // manually implement recursive filtering since older Qt versions don't support it
    if(filterAcceptsSingleRow(source_row, source_parent))
      return true;

    QModelIndex idx = sourceModel()->index(source_row, 0, source_parent);
    for(int i = 0, row_count = sourceModel()->rowCount(idx); i < row_count; i++)
      if(filterAcceptsRow(i, idx))
        return true;

    return false;
  }

  virtual bool filterAcceptsSingleRow(int source_row, const QModelIndex &source_parent) const
  {
    // always include capture start and the root
    if(!source_parent.isValid() ||
       (source_parent.internalId() == EventItemModel::TagRoot && source_row == 0))
      return true;

    QModelIndex source_idx = sourceModel()->index(source_row, 0, source_parent);

    // since we have recursive filtering enabled (so a child keeps its parent visible, to allow
    // filtering for specific things and not hiding the hierarchy along the way that doesn't
    // match), we can implement hiding of empty regions by simply forcing anything with children
    // to not match. That way it will only be included if at least one child matches
    if(!m_EmptyRegionsVisible && sourceModel()->rowCount(source_idx) > 0)
      return false;

    uint32_t eid = sourceModel()->data(source_idx, ROLE_SELECTED_EID).toUInt();

    m_VisibleCache.resize_for_index(eid);
    if(m_VisibleCache[eid] != 0)
      return m_VisibleCache[eid] > 0;

    const SDChunk *chunk =
        (const SDChunk *)(sourceModel()->data(source_idx, ROLE_CHUNK).toULongLong());
    const DrawcallDescription *draw =
        (const DrawcallDescription
             *)(sourceModel()->data(source_idx, ROLE_GROUPED_DRAWCALL).toULongLong());
    QString name = source_idx.data(Qt::DisplayRole).toString();

    int off = name.indexOf(QLatin1Char('<'));
    while(off >= 0 && off + 4 < name.size())
    {
      if(name[off + 1] == QLatin1Char('/') || name.midRef(off, 5) == lit("<span"))
      {
        int end = name.indexOf(QLatin1Char('>'), off);
        name.remove(off, end - off + 1);
      }

      off = name.indexOf(QLatin1Char('<'), off + 1);
    }

    m_VisibleCache[eid] = EvaluateFilterSet(m_Ctx, m_Filters, false, eid, chunk, draw, name);
    return m_VisibleCache[eid] > 0;
  }

private:
  ICaptureContext &m_Ctx;

  // this caches whether a row passes or not, with 0 meaning 'uncached' and 1/-1 being true or
  // false. This means we can resize it and know that we don't pollute with bad data.
  // it must be mutable since we update it in a const function filterAcceptsRow.
  mutable rdcarray<int8_t> m_VisibleCache;

  EventItemModel *m_Model = NULL;

  bool m_EmptyRegionsVisible = true;
  rdcarray<EventFilter> m_Filters;

  IEventBrowser::EventFilterCallback MakeLiteralMatcher(QString string)
  {
    QString matchString = string.toLower();
    return [matchString](ICaptureContext *, const rdcstr &, const rdcstr &, uint32_t,
                         const SDChunk *, const DrawcallDescription *, const rdcstr &name) {
      return QString(name).toLower().contains(matchString);
    };
  }

  struct Token
  {
    int position;
    int length;
    QString text;
  };

  QList<Token> tokenise(QString parameters)
  {
    QList<Token> ret;

    int p = 0;
    while(p < parameters.size())
    {
      if(parameters[p].isSpace())
      {
        p++;
        continue;
      }

      Token t;
      t.position = p;
      while(p < parameters.size() && !parameters[p].isSpace())
        p++;
      t.length = p - t.position;
      t.text = parameters.mid(t.position, t.length);
      ret.push_back(t);
    }

    return ret;
  }

  IEventBrowser::EventFilterCallback filterFunction_any(QString name, QString parameters,
                                                        ParseTrace &trace)
  {
    // $any(...) => returns true if any of the nested subexpressions are true (with exclusions
    // treated as normal).
    rdcarray<EventFilter> filters;
    trace = ParseExpressionToFilters(parameters, filters);

    if(!trace.hasErrors())
    {
      if(filters.isEmpty())
      {
        trace.setError(tr("No filters provided to $any()", "EventFilterModel"));
        return NULL;
      }

      return [filters](ICaptureContext *ctx, const rdcstr &, const rdcstr &, uint32_t eid,
                       const SDChunk *chunk, const DrawcallDescription *draw, const rdcstr &name) {
        return EvaluateFilterSet(*ctx, filters, false, eid, chunk, draw, name);
      };
    }

    return NULL;
  }

  IEventBrowser::EventFilterCallback filterFunction_all(QString name, QString parameters,
                                                        ParseTrace &trace)
  {
    // $all(...) => returns true only if all the nested subexpressions are true
    rdcarray<EventFilter> filters;
    trace = ParseExpressionToFilters(parameters, filters);

    if(!trace.hasErrors())
    {
      if(filters.isEmpty())
      {
        trace.setError(tr("No filters provided to $all()", "EventFilterModel"));
        return NULL;
      }

      return [filters](ICaptureContext *ctx, const rdcstr &, const rdcstr &, uint32_t eid,
                       const SDChunk *chunk, const DrawcallDescription *draw, const rdcstr &name) {
        return EvaluateFilterSet(*ctx, filters, true, eid, chunk, draw, name);
      };
    }

    return NULL;
  }

  IEventBrowser::EventFilterCallback filterFunction_param(QString name, QString parameters,
                                                          ParseTrace &trace)
  {
    // $param() => check for a named parameter having a particular value
    int idx = parameters.indexOf(QLatin1Char(':'));

    if(idx < 0)
    {
      trace.setError(tr("Parameter to to $param() should be name: value", "EventFilterModel"));
      return NULL;
    }

    QString paramName = parameters.mid(0, idx).trimmed();
    QString paramValue = parameters.mid(idx + 1).trimmed();

    if(paramValue.isEmpty())
    {
      trace.setError(tr("Parameter to to $param() should be name: value", "EventFilterModel"));
      return NULL;
    }

    return
        [paramName, paramValue](ICaptureContext *ctx, const rdcstr &, const rdcstr &, uint32_t,
                                const SDChunk *chunk, const DrawcallDescription *, const rdcstr &) {
          const SDObject *o = FindChildRecursively(chunk, paramName);

          if(!o)
            return false;

          if(o->IsArray())
          {
            for(const SDObject *c : *o)
            {
              if(RichResourceTextFormat(*ctx, SDObject2Variant(c, false))
                     .contains(paramValue, Qt::CaseInsensitive))
                return true;
            }

            return false;
          }
          else
          {
            return RichResourceTextFormat(*ctx, SDObject2Variant(o, false))
                .contains(paramValue, Qt::CaseInsensitive);
          }

        };
  }

  rdcarray<rdcstr> filterCompleter_event(ICaptureContext *ctx, const rdcstr &name,
                                         const rdcstr &params)
  {
    if(params.find_first_of(" \t") == -1)
      return {"EID"};

    return {};
  }

  IEventBrowser::EventFilterCallback filterFunction_event(QString name, QString parameters,
                                                          ParseTrace &trace)
  {
    // $event(...) => filters on any event property (at the moment only EID)
    QList<Token> tokens = tokenise(parameters);

    static const QStringList operators = {
        lit("=="), lit("!="), lit("<"), lit(">"), lit("<="), lit(">="),
    };

    if(tokens.size() < 1)
    {
      trace.setError(tr("Filter parameters required", "EventFilterModel"));
      return NULL;
    }

    if(tokens[0].text.toLower() != lit("eid") && tokens[0].text.toLower() != lit("eventid"))
    {
      trace.position = tokens[0].position;
      trace.length = tokens[0].length;
      trace.setError(tr("Unrecognised event property", "EventFilterModel"));
      return NULL;
    }

    int operatorIdx = operators.indexOf(tokens[1].text);

    if(tokens[1].text == lit("="))
      operatorIdx = 0;

    if(tokens.size() != 3 || operatorIdx < 0 || operatorIdx >= operators.size())
    {
      trace.setError(tr("Invalid expression, expected single comparison with operators: %3",
                        "EventFilterModel")
                         .arg(operators.join(lit(", "))));
      return NULL;
    }

    bool ok = false;
    uint32_t eid = tokens[2].text.toUInt(&ok, 0);

    if(!ok)
    {
      trace.position = tokens[2].position;
      trace.length = tokens[2].length;
      trace.setError(tr("Invalid value, expected integer", "EventFilterModel"));
      return NULL;
    }

    switch(operatorIdx)
    {
      case 0:
        return [eid](ICaptureContext *, const rdcstr &, const rdcstr &, uint32_t eventId,
                     const SDChunk *, const DrawcallDescription *draw,
                     const rdcstr &) { return eventId == eid; };
      case 1:
        return [eid](ICaptureContext *, const rdcstr &, const rdcstr &, uint32_t eventId,
                     const SDChunk *, const DrawcallDescription *draw,
                     const rdcstr &) { return eventId != eid; };
      case 2:
        return [eid](ICaptureContext *, const rdcstr &, const rdcstr &, uint32_t eventId,
                     const SDChunk *, const DrawcallDescription *draw,
                     const rdcstr &) { return eventId < eid; };
      case 3:
        return [eid](ICaptureContext *, const rdcstr &, const rdcstr &, uint32_t eventId,
                     const SDChunk *, const DrawcallDescription *draw,
                     const rdcstr &) { return eventId > eid; };
      case 4:
        return [eid](ICaptureContext *, const rdcstr &, const rdcstr &, uint32_t eventId,
                     const SDChunk *, const DrawcallDescription *draw,
                     const rdcstr &) { return eventId <= eid; };
      case 5:
        return [eid](ICaptureContext *, const rdcstr &, const rdcstr &, uint32_t eventId,
                     const SDChunk *, const DrawcallDescription *draw,
                     const rdcstr &) { return eventId >= eid; };
      default: trace.setError(tr("Internal error", "EventFilterModel")); return NULL;
    }
  }

  rdcarray<rdcstr> filterCompleter_draw(ICaptureContext *ctx, const rdcstr &name, const rdcstr &params)
  {
    if(params.find_first_of(" \t") == -1)
      return {
          "EID", "parent", "drawcallId", "numIndices",
          // most aliases we don't autocomplete but this one we leave
          "numVertices", "numInstances", "baseVertex", "indexOffset", "vertexOffset",
          "instanceOffset", "dispatchX", "dispatchY", "dispatchZ", "dispatchSize", "duration",
          "flags",
      };

    QList<Token> tokens = tokenise(params);

    if(tokens[0].text == lit("flags") && tokens.size() >= 2)
    {
      CacheDrawFlagsLookup();

      rdcarray<rdcstr> flags;

      for(QString s : DrawFlagsList)
        flags.push_back(s);

      return flags;
    }

    return {};
  }

  IEventBrowser::EventFilterCallback filterFunction_draw(QString name, QString parameters,
                                                         ParseTrace &trace)
  {
    // $draw(...) => returns true only for draws, optionally with a particular property filter
    QList<Token> tokens = tokenise(parameters);

    // no parameters, just return if it's a draw
    if(tokens.isEmpty())
      return
          [](ICaptureContext *, const rdcstr &, const rdcstr &, uint32_t eventId, const SDChunk *,
             const DrawcallDescription *draw, const rdcstr &) { return draw->eventId == eventId; };

    // we upcast to int64_t so we can compare both unsigned and signed values without losing any
    // precision (we don't have any uint64_ts to compare)
    using PropGetter = int64_t (*)(const DrawcallDescription *);

    struct NamedProp
    {
      const char *name;
      PropGetter getter;
    };

#define NAMED_PROP(name, access)                                            \
  {                                                                         \
    name, [](const DrawcallDescription *draw) -> int64_t { return access; } \
  }

    static const NamedProp namedProps[] = {
        NAMED_PROP("eventid", draw->eventId), NAMED_PROP("eid", draw->eventId),
        NAMED_PROP("parent", draw->parent ? draw->parent->eventId : -12341234),
        NAMED_PROP("drawcallid", draw->drawcallId), NAMED_PROP("drawid", draw->drawcallId),
        NAMED_PROP("numindices", draw->numIndices), NAMED_PROP("numindexes", draw->numIndices),
        NAMED_PROP("numvertices", draw->numIndices), NAMED_PROP("numvertexes", draw->numIndices),
        NAMED_PROP("indexcount", draw->numIndices), NAMED_PROP("vertexcount", draw->numIndices),
        NAMED_PROP("numinstances", draw->numInstances),
        NAMED_PROP("instancecount", draw->numInstances), NAMED_PROP("basevertex", draw->baseVertex),
        NAMED_PROP("indexoffset", draw->indexOffset), NAMED_PROP("vertexoffset", draw->vertexOffset),
        NAMED_PROP("instanceoffset", draw->instanceOffset),
        NAMED_PROP("dispatchx", draw->dispatchDimension[0]),
        NAMED_PROP("dispatchy", draw->dispatchDimension[1]),
        NAMED_PROP("dispatchz", draw->dispatchDimension[2]),
        NAMED_PROP("dispatchsize", draw->dispatchDimension[0] * draw->dispatchDimension[1] *
                                       draw->dispatchDimension[2]),
    };

    QByteArray prop = tokens[0].text.toLower().toLatin1();

    int numericPropIndex = -1;
    for(size_t i = 0; i < ARRAY_COUNT(namedProps); i++)
    {
      if(prop == namedProps[i].name)
        numericPropIndex = (int)i;
    }

    // any numeric value that can be compared to a number
    if(numericPropIndex >= 0)
    {
      PropGetter propGetter = namedProps[numericPropIndex].getter;

      static const QStringList operators = {
          lit("=="), lit("!="), lit("<"), lit(">"), lit("<="), lit(">="),
      };

      int operatorIdx = tokens.size() > 1 ? operators.indexOf(tokens[1].text) : -1;

      if(tokens.size() > 1 && tokens[1].text == lit("="))
        operatorIdx = 0;

      if(tokens.size() != 3 || operatorIdx < 0 || operatorIdx >= operators.size())
      {
        trace.setError(tr("Invalid expression, expected single comparison with operators: %3",
                          "EventFilterModel")
                           .arg(operators.join(lit(", "))));
        return NULL;
      }

      bool ok = false;
      int64_t value = tokens[2].text.toLongLong(&ok, 0);

      if(!ok)
      {
        trace.position = tokens[2].position;
        trace.length = tokens[2].length;
        trace.setError(tr("Invalid value, expected integer", "EventFilterModel"));
        return NULL;
      }

      switch(operatorIdx)
      {
        case 0:
          return [propGetter, value](ICaptureContext *, const rdcstr &, const rdcstr &,
                                     uint32_t eventId, const SDChunk *,
                                     const DrawcallDescription *draw, const rdcstr &) {
            return draw->eventId == eventId && propGetter(draw) == value;
          };
        case 1:
          return [propGetter, value](ICaptureContext *, const rdcstr &, const rdcstr &,
                                     uint32_t eventId, const SDChunk *,
                                     const DrawcallDescription *draw, const rdcstr &) {
            return draw->eventId == eventId && propGetter(draw) != value;
          };
        case 2:
          return [propGetter, value](ICaptureContext *, const rdcstr &, const rdcstr &,
                                     uint32_t eventId, const SDChunk *,
                                     const DrawcallDescription *draw, const rdcstr &) {
            return draw->eventId == eventId && propGetter(draw) < value;
          };
        case 3:
          return [propGetter, value](ICaptureContext *, const rdcstr &, const rdcstr &,
                                     uint32_t eventId, const SDChunk *,
                                     const DrawcallDescription *draw, const rdcstr &) {
            return draw->eventId == eventId && propGetter(draw) > value;
          };
        case 4:
          return [propGetter, value](ICaptureContext *, const rdcstr &, const rdcstr &,
                                     uint32_t eventId, const SDChunk *,
                                     const DrawcallDescription *draw, const rdcstr &) {
            return draw->eventId == eventId && propGetter(draw) <= value;
          };
        case 5:
          return [propGetter, value](ICaptureContext *, const rdcstr &, const rdcstr &,
                                     uint32_t eventId, const SDChunk *,
                                     const DrawcallDescription *draw, const rdcstr &) {
            return draw->eventId == eventId && propGetter(draw) >= value;
          };
        default: trace.setError(tr("Internal error", "EventFilterModel")); return NULL;
      }
    }
    else if(tokens[0].text.toLower() == lit("duration"))
    {
      // deliberately don't allow equality/inequality
      static const QStringList operators = {
          lit("<"), lit(">"), lit("<="), lit(">="),
      };

      int operatorIdx = tokens.size() > 1 ? operators.indexOf(tokens[1].text) : -1;

      if(tokens.size() != 3 || operatorIdx < 0 || operatorIdx >= operators.size())
      {
        trace.setError(tr("Invalid expression, expected single comparison with operators: %3",
                          "EventFilterModel")
                           .arg(operators.join(lit(", "))));
        return NULL;
      }

      // multiplier to change the read value into nanoseconds
      double mult = 1.0;
      if(tokens[2].text.endsWith(lit("ms")))
      {
        mult = 1000000.0;
        tokens[2].text.resize(tokens[2].text.size() - 2);
      }
      else if(tokens[2].text.endsWith(lit("us")))
      {
        mult = 1000.0;
        tokens[2].text.resize(tokens[2].text.size() - 2);
      }
      else if(tokens[2].text.endsWith(lit("ns")))
      {
        mult = 1.0;
        tokens[2].text.resize(tokens[2].text.size() - 2);
      }
      else if(tokens[2].text.endsWith(lit("s")))
      {
        mult = 1000000000.0;
        tokens[2].text.resize(tokens[2].text.size() - 1);
      }
      else
      {
        trace.position = tokens[2].position;
        trace.length = tokens[2].length;
        trace.setError(
            tr("Duration must be suffixed with one of s, ms, us, or ns", "EventFilterModel"));
        return NULL;
      }

      bool ok = false;
      double value = tokens[2].text.toDouble(&ok);

      // if it doesn't read as a double, try as an integer
      if(!ok)
      {
        int64_t valInt = tokens[2].text.toLongLong(&ok);

        if(ok)
          value = double(valInt);
      }

      if(!ok)
      {
        trace.position = tokens[2].position;
        trace.length = tokens[2].length;
        trace.setError(tr("Invalid value, expected duration suffixed with one of s, ms, us, or ns",
                          "EventFilterModel"));
        return NULL;
      }

      int64_t nanoValue = int64_t(value * mult);

      switch(operatorIdx)
      {
        case 0:
          return
              [this, nanoValue](ICaptureContext *, const rdcstr &, const rdcstr &, uint32_t eventId,
                                const SDChunk *, const DrawcallDescription *draw, const rdcstr &) {
                double dur = m_Model->GetSecondsDurationForEID(eventId);
                return dur >= 0.0 && int64_t(dur * 1000000000.0) < nanoValue;
              };
        case 1:
          return
              [this, nanoValue](ICaptureContext *, const rdcstr &, const rdcstr &, uint32_t eventId,
                                const SDChunk *, const DrawcallDescription *draw, const rdcstr &) {
                double dur = m_Model->GetSecondsDurationForEID(eventId);
                return dur >= 0.0 && int64_t(dur * 1000000000.0) > nanoValue;
              };
        case 2:
          return
              [this, nanoValue](ICaptureContext *, const rdcstr &, const rdcstr &, uint32_t eventId,
                                const SDChunk *, const DrawcallDescription *draw, const rdcstr &) {
                double dur = m_Model->GetSecondsDurationForEID(eventId);
                return dur >= 0.0 && int64_t(dur * 1000000000.0) <= nanoValue;
              };
        case 3:
          return
              [this, nanoValue](ICaptureContext *, const rdcstr &, const rdcstr &, uint32_t eventId,
                                const SDChunk *, const DrawcallDescription *draw, const rdcstr &) {
                double dur = m_Model->GetSecondsDurationForEID(eventId);
                return dur >= 0.0 && int64_t(dur * 1000000000.0) >= nanoValue;
              };
        default: trace.setError(tr("Internal error", "EventFilterModel")); return NULL;
      }
    }
    else if(tokens[0].text.toLower() == lit("flags"))
    {
      if(tokens.size() < 3 || (tokens[1].text != lit("&") && tokens[1].text != lit("=")))
      {
        trace.position = tokens[0].position;
        trace.length = (tokens[1].position + tokens[1].length) - trace.position + 1;
        trace.setError(tr("Expected $draw(flags & ...)", "EventFilterModel"));
        return NULL;
      }

      // there could be whitespace in the flags list so iterate over all remaining parameters (as
      // split by whitespace)
      QStringList flagStrings;
      for(int i = 2; i < tokens.count(); i++)
        flagStrings.append(tokens[i].text.split(QLatin1Char('|'), QString::KeepEmptyParts));

      // if we have an empty string in the list somewhere that means the | list was broken
      if(flagStrings.contains(QString()))
      {
        trace.position = tokens[2].position;
        trace.length = (tokens.back().position + tokens.back().length) - trace.position + 1;
        trace.setError(tr("Invalid draw flags expression", "EventFilterModel"));
        return NULL;
      }

      CacheDrawFlagsLookup();

      DrawFlags flags = DrawFlags::NoFlags;
      for(const QString &flagString : flagStrings)
      {
        auto it = DrawFlagsLookup.find(flagString.toLower());
        if(it == DrawFlagsLookup.end())
        {
          trace.position = tokens[2].position;
          trace.length = (tokens.back().position + tokens.back().length) - trace.position + 1;
          trace.setError(tr("Unrecognised draw flag '%1'", "EventFilterModel").arg(flagString));
          return NULL;
        }

        flags |= it.value();
      }

      return [flags](ICaptureContext *, const rdcstr &, const rdcstr &, uint32_t eventId,
                     const SDChunk *, const DrawcallDescription *draw,
                     const rdcstr &) { return draw->eventId == eventId && (draw->flags & flags); };
    }
    else
    {
      trace.setError(tr("Unrecognised property expression", "EventFilterModel"));
      return NULL;
    }
  }

  rdcarray<rdcstr> filterCompleter_dispatch(ICaptureContext *ctx, const rdcstr &name,
                                            const rdcstr &params)
  {
    if(params.find_first_of(" \t") == -1)
      return {
          "EID", "eventId", "parent", "drawcallId", "drawId", "x", "y", "z", "size", "duration",
      };

    return {};
  }

  IEventBrowser::EventFilterCallback filterFunction_dispatch(QString name, QString parameters,
                                                             ParseTrace &trace)
  {
    // $dispatch(...) => returns true only for dispatches, optionally with a particular property
    // filter
    QList<Token> tokens = tokenise(parameters);

    // no parameters, just return if it's a draw
    if(tokens.isEmpty())
      return [](ICaptureContext *, const rdcstr &, const rdcstr &, uint32_t eventId,
                const SDChunk *, const DrawcallDescription *draw, const rdcstr &) {
        return draw->eventId == eventId && (draw->flags & DrawFlags::Dispatch);
      };

    // we upcast to int64_t so we can compare both unsigned and signed values without losing any
    // precision (we don't have any uint64_ts to compare)
    using PropGetter = int64_t (*)(const DrawcallDescription *);

    struct NamedProp
    {
      const char *name;
      PropGetter getter;
    };

    static const NamedProp namedProps[] = {
        NAMED_PROP("eventid", draw->eventId), NAMED_PROP("eid", draw->eventId),
        NAMED_PROP("parent", draw->parent ? draw->parent->eventId : -12341234),
        NAMED_PROP("drawcallid", draw->drawcallId), NAMED_PROP("drawid", draw->drawcallId),
        NAMED_PROP("dispatchx", draw->dispatchDimension[0]),
        NAMED_PROP("dispatchy", draw->dispatchDimension[1]),
        NAMED_PROP("dispatchz", draw->dispatchDimension[2]),
        NAMED_PROP("x", draw->dispatchDimension[0]), NAMED_PROP("y", draw->dispatchDimension[1]),
        NAMED_PROP("z", draw->dispatchDimension[2]),
        NAMED_PROP("dispatchsize", draw->dispatchDimension[0] * draw->dispatchDimension[1] *
                                       draw->dispatchDimension[2]),
        NAMED_PROP("size", draw->dispatchDimension[0] * draw->dispatchDimension[1] *
                               draw->dispatchDimension[2]),
    };

    QByteArray prop = tokens[0].text.toLower().toLatin1();

    int numericPropIndex = -1;
    for(size_t i = 0; i < ARRAY_COUNT(namedProps); i++)
    {
      if(prop == namedProps[i].name)
        numericPropIndex = (int)i;
    }

    // any numeric value that can be compared to a number
    if(numericPropIndex >= 0)
    {
      PropGetter propGetter = namedProps[numericPropIndex].getter;

      static const QStringList operators = {
          lit("=="), lit("!="), lit("<"), lit(">"), lit("<="), lit(">="),
      };

      int operatorIdx = tokens.size() > 1 ? operators.indexOf(tokens[1].text) : -1;

      if(tokens.size() > 1 && tokens[1].text == lit("="))
        operatorIdx = 0;

      if(tokens.size() != 3 || operatorIdx < 0 || operatorIdx >= operators.size())
      {
        trace.setError(tr("Invalid expression, expected single comparison with operators: %3",
                          "EventFilterModel")
                           .arg(operators.join(lit(", "))));
        return NULL;
      }

      bool ok = false;
      int64_t value = tokens[2].text.toLongLong(&ok, 0);

      if(!ok)
      {
        trace.position = tokens[2].position;
        trace.length = tokens[2].length;
        trace.setError(tr("Invalid value, expected integer", "EventFilterModel"));
        return NULL;
      }

      switch(operatorIdx)
      {
        case 0:
          return [propGetter, value](ICaptureContext *, const rdcstr &, const rdcstr &,
                                     uint32_t eventId, const SDChunk *,
                                     const DrawcallDescription *draw, const rdcstr &) {
            return draw->eventId == eventId && (draw->flags & DrawFlags::Dispatch) &&
                   propGetter(draw) == value;
          };
        case 1:
          return [propGetter, value](ICaptureContext *, const rdcstr &, const rdcstr &,
                                     uint32_t eventId, const SDChunk *,
                                     const DrawcallDescription *draw, const rdcstr &) {
            return draw->eventId == eventId && (draw->flags & DrawFlags::Dispatch) &&
                   propGetter(draw) != value;
          };
        case 2:
          return [propGetter, value](ICaptureContext *, const rdcstr &, const rdcstr &,
                                     uint32_t eventId, const SDChunk *,
                                     const DrawcallDescription *draw, const rdcstr &) {
            return draw->eventId == eventId && (draw->flags & DrawFlags::Dispatch) &&
                   propGetter(draw) < value;
          };
        case 3:
          return [propGetter, value](ICaptureContext *, const rdcstr &, const rdcstr &,
                                     uint32_t eventId, const SDChunk *,
                                     const DrawcallDescription *draw, const rdcstr &) {
            return draw->eventId == eventId && (draw->flags & DrawFlags::Dispatch) &&
                   propGetter(draw) > value;
          };
        case 4:
          return [propGetter, value](ICaptureContext *, const rdcstr &, const rdcstr &,
                                     uint32_t eventId, const SDChunk *,
                                     const DrawcallDescription *draw, const rdcstr &) {
            return draw->eventId == eventId && (draw->flags & DrawFlags::Dispatch) &&
                   propGetter(draw) <= value;
          };
        case 5:
          return [propGetter, value](ICaptureContext *, const rdcstr &, const rdcstr &,
                                     uint32_t eventId, const SDChunk *,
                                     const DrawcallDescription *draw, const rdcstr &) {
            return draw->eventId == eventId && (draw->flags & DrawFlags::Dispatch) &&
                   propGetter(draw) >= value;
          };
        default: trace.setError(tr("Internal error", "EventFilterModel")); return NULL;
      }
    }
    else if(tokens[0].text.toLower() == lit("duration"))
    {
      // deliberately don't allow equality/inequality
      static const QStringList operators = {
          lit("<"), lit(">"), lit("<="), lit(">="),
      };

      int operatorIdx = tokens.size() > 1 ? operators.indexOf(tokens[1].text) : -1;

      if(tokens.size() != 3 || operatorIdx < 0 || operatorIdx >= operators.size())
      {
        trace.setError(tr("Invalid expression, expected single comparison with operators: %3",
                          "EventFilterModel")
                           .arg(operators.join(lit(", "))));
        return NULL;
      }

      // multiplier to change the read value into nanoseconds
      double mult = 1.0;
      if(tokens[2].text.endsWith(lit("ms")))
      {
        mult = 1000000.0;
        tokens[2].text.resize(tokens[2].text.size() - 2);
      }
      else if(tokens[2].text.endsWith(lit("us")))
      {
        mult = 1000.0;
        tokens[2].text.resize(tokens[2].text.size() - 2);
      }
      else if(tokens[2].text.endsWith(lit("ns")))
      {
        mult = 1.0;
        tokens[2].text.resize(tokens[2].text.size() - 2);
      }
      else if(tokens[2].text.endsWith(lit("s")))
      {
        mult = 1000000000.0;
        tokens[2].text.resize(tokens[2].text.size() - 1);
      }
      else
      {
        trace.position = tokens[2].position;
        trace.length = tokens[2].length;
        trace.setError(
            tr("Duration must be suffixed with one of s, ms, us, or ns", "EventFilterModel"));
        return NULL;
      }

      bool ok = false;
      double value = tokens[2].text.toDouble(&ok);

      // if it doesn't read as a double, try as an integer
      if(!ok)
      {
        int64_t valInt = tokens[2].text.toLongLong(&ok);

        if(ok)
          value = double(valInt);
      }

      if(!ok)
      {
        trace.position = tokens[2].position;
        trace.length = tokens[2].length;
        trace.setError(tr("Invalid value, expected duration suffixed with one of s, ms, us, or ns",
                          "EventFilterModel"));
        return NULL;
      }

      int64_t nanoValue = int64_t(value * mult);

      switch(operatorIdx)
      {
        case 0:
          return
              [this, nanoValue](ICaptureContext *, const rdcstr &, const rdcstr &, uint32_t eventId,
                                const SDChunk *, const DrawcallDescription *draw, const rdcstr &) {
                double dur = m_Model->GetSecondsDurationForEID(eventId);
                return draw->eventId == eventId && (draw->flags & DrawFlags::Dispatch) &&
                       dur >= 0.0 && int64_t(dur * 1000000000.0) < nanoValue;
              };
        case 1:
          return
              [this, nanoValue](ICaptureContext *, const rdcstr &, const rdcstr &, uint32_t eventId,
                                const SDChunk *, const DrawcallDescription *draw, const rdcstr &) {
                double dur = m_Model->GetSecondsDurationForEID(eventId);
                return draw->eventId == eventId && (draw->flags & DrawFlags::Dispatch) &&
                       dur >= 0.0 && int64_t(dur * 1000000000.0) > nanoValue;
              };
        case 2:
          return
              [this, nanoValue](ICaptureContext *, const rdcstr &, const rdcstr &, uint32_t eventId,
                                const SDChunk *, const DrawcallDescription *draw, const rdcstr &) {
                double dur = m_Model->GetSecondsDurationForEID(eventId);
                return draw->eventId == eventId && (draw->flags & DrawFlags::Dispatch) &&
                       dur >= 0.0 && int64_t(dur * 1000000000.0) <= nanoValue;
              };
        case 3:
          return
              [this, nanoValue](ICaptureContext *, const rdcstr &, const rdcstr &, uint32_t eventId,
                                const SDChunk *, const DrawcallDescription *draw, const rdcstr &) {
                double dur = m_Model->GetSecondsDurationForEID(eventId);
                return draw->eventId == eventId && (draw->flags & DrawFlags::Dispatch) &&
                       dur >= 0.0 && int64_t(dur * 1000000000.0) >= nanoValue;
              };
        default: trace.setError(tr("Internal error", "EventFilterModel")); return NULL;
      }
    }
    else
    {
      trace.setError(tr("Unrecognised property expression", "EventFilterModel"));
      return NULL;
    }
  }

  IEventBrowser::EventFilterCallback MakeFunctionMatcher(QString name, QString parameters,
                                                         ParseTrace &trace)
  {
    if(m_BuiltinFilters.contains(name))
    {
      return m_BuiltinFilters[name].makeFilter(name, parameters, trace);
    }

    if(m_CustomFilters.contains(name))
    {
      IEventBrowser::EventFilterCallback innerFilter = m_CustomFilters[name].filter;
      IEventBrowser::FilterParseCallback innerParser = m_CustomFilters[name].parser;

      rdcstr n = name;
      rdcstr p = parameters;

      QString errString;

      if(innerParser)
        errString = innerParser(&m_Ctx, n, p);

      if(!errString.isEmpty())
      {
        trace.setError(errString);
        return NULL;
      }

      return [n, p, innerFilter](ICaptureContext *ctx, const rdcstr &, const rdcstr &, uint32_t eid,
                                 const SDChunk *chunk, const DrawcallDescription *draw,
                                 const rdcstr &name) {
        return innerFilter(ctx, n, p, eid, chunk, draw, name);
      };
    }

    trace.setError(tr("Unknown filter function", "EventFilterModel").arg(name));
    return NULL;
  }

  ParseTrace ParseExpressionToFilters(QString expr, rdcarray<EventFilter> &filters)
  {
    // we have a simple grammar, we pick out subexpressions and they're all independent.
    //
    // - Individual words are literal subexpressions on their own and end with whitespace.
    // - A " starts a quoted literal subexpression, which is a literal string and ends on the next
    //   ". It supports escaping " with \ and otherwise any escaped character is the literal
    //   character. The quotes must be followed by whitespace or the end of the expression.
    // - Both literal subexpressions are matched case-insensitively. For case-sensitive matching
    //   you can use a regexp as a functional expression.
    // - A $ starts a functional expression, with parameters in brackets. The name starts with a
    //   letter and contains alphanumeric and . characters. The name is followed by a ( to begin the
    //   parameters and ends when the matching ) is found. Whitespace between the name and the ( is
    //   ignored, but any other character is a parse error. The ) must be followed by whitespace or
    //   the end of the expression or that's a parse error. Note that whitespace is allowed in the
    //   parameters.
    //
    // This means there's a simple state machine we can follow from any point.
    //
    // 1) start -> whitespace -> start
    // 2) start -> " -> quoted_expression -> wait until unescaped " -> start
    // 3) start -> $ -> function_expression -> parse name > optional whitespace ->
    //             ( -> wait until matching parenthesis -> ) -> whitespace -> start
    // 4) start -> anything else -> literal -> wait until whitespace -> start
    //
    // We also have two modifiers:
    //
    // 5) start -> + -> note that next filter is a MustMatch -> start
    // 6) start -> - -> note that next filter is a CantMatch -> start
    //
    // any non-existant edge is a parse error

    ParseTrace trace;

    enum
    {
      Start,
      QuotedExpr,
      FunctionExprName,
      FunctionExprParams,
      Literal,
    } state = Start;

    expr = expr.trimmed();

    MatchType matchType = MatchType::Normal;

    // temporary string we're building up somewhere
    QString s;

    // parenthesis depth
    int parenDepth = 0;
    // start of the current set of parameters
    int paramStartPos = 0;
    // parameters (since s holds the function name)
    QString params;

    int pos = 0;
    while(pos < expr.length())
    {
      trace.length = qMax(0, pos - trace.position + 1);

      if(state == Start)
      {
        // 1) skip whitespace while in start
        if(expr[pos].isSpace())
        {
          pos++;
          continue;
        }

        // 5) and 6)
        if(expr[pos] == QLatin1Char('-'))
        {
          trace.position = pos;

          // stay in the Start state, but store match type
          matchType = MatchType::CantMatch;
          pos++;
          continue;
        }

        if(expr[pos] == QLatin1Char('+'))
        {
          trace.position = pos;

          matchType = MatchType::MustMatch;
          pos++;
          continue;
        }

        // 3.1) move to function expression if we see a $
        if(expr[pos] == QLatin1Char('$'))
        {
          // only update the position if the match type is normal. If it's mustmatch/cantmatch
          // include everything from the preceeding - or +
          if(matchType == MatchType::Normal)
            trace.position = pos;

          // we need at minimum 3 more characters for $x()
          if(pos + 3 >= expr.length())
          {
            trace.length = expr.length() - trace.position;
            return trace.setError(
                tr("Invalid function expression\n"
                   "If this is not a filter function, surround with quotes.",
                   "EventFilterModel"));
          }

          // consume the $
          state = FunctionExprName;
          pos++;

          continue;
        }

        // 2.1) move to quoted expression if we see a "
        if(expr[pos] == QLatin1Char('"'))
        {
          // only update the position if the match type is normal. If it's mustmatch/cantmatch
          // include everything from the preceeding - or +
          if(matchType == MatchType::Normal)
            trace.position = pos;

          state = QuotedExpr;
          pos++;
          continue;
        }

        // only update the position if the match type is normal. If it's mustmatch/cantmatch
        // include everything from the preceeding - or +
        if(matchType == MatchType::Normal)
          trace.position = pos;

        // 4.1) for anything else begin parsing a literal expression
        state = Literal;
        // don't continue here, we need to parse the first character of the literal
      }

      if(state == QuotedExpr)
      {
        // 2.2) handle escaping
        if(expr[pos] == QLatin1Char('\\'))
        {
          if(pos == expr.length() - 1)
          {
            trace.length = expr.length() - trace.position;
            return trace.setError(
                tr("Invalid escape sequence in quoted string", "EventFilterModel"));
          }

          // append the next character, whatever it is, and skip the escape character
          s.append(expr[pos + 1]);
          pos += 2;
          continue;
        }
        else if(expr[pos] == QLatin1Char('"'))
        {
          // if we encounter an unescaped quote we're done

          // however we expect the end of the expression or whitespace next
          if(pos + 1 < expr.length() && !expr[pos + 1].isSpace())
          {
            trace.length = (pos + 1) - trace.position + 1;
            return trace.setError(
                tr("Unexpected character after quoted string", "EventFilterModel"));
          }

          filters.push_back(EventFilter(MakeLiteralMatcher(s), matchType));

          FilterExpression subexpr;

          subexpr.matchType = matchType;
          subexpr.function = false;
          subexpr.name = s;
          subexpr.position = trace.position;
          subexpr.length = trace.length;

          trace.exprs.push_back(subexpr);

          s.clear();
          pos++;
          state = Start;
          matchType = MatchType::Normal;

          trace.position = pos;

          continue;
        }
        else
        {
          // just another character, append and continue
          s.append(expr[pos]);
          pos++;
          continue;
        }
      }

      if(state == Literal)
      {
        // if we encounter whitespace or the end of the expression, we're done
        if(expr[pos].isSpace() || pos == expr.length() - 1)
        {
          if(expr[pos].isSpace())
            trace.length--;
          else
            s.append(expr[pos]);

          filters.push_back(EventFilter(MakeLiteralMatcher(s), matchType));

          FilterExpression subexpr;

          subexpr.matchType = matchType;
          subexpr.function = false;
          subexpr.name = s;
          subexpr.position = trace.position;
          subexpr.length = trace.length;

          trace.exprs.push_back(subexpr);

          s.clear();
          pos++;
          state = Start;
          matchType = MatchType::Normal;

          trace.position = pos;

          continue;
        }
        else
        {
          // just another character, append and continue
          s.append(expr[pos]);
          pos++;
          continue;
        }
      }

      if(state == FunctionExprName)
      {
        // if we encounter a parenthesis check we have a valid filter function name and move to
        // parsing parameters
        if(expr[pos] == QLatin1Char('('))
        {
          state = FunctionExprParams;
          parenDepth = 1;
          pos++;
          paramStartPos = pos;

          trace.length = pos - trace.position + 1;

          if(s.isEmpty())
            return trace.setError(
                tr("Filter function with no name before arguments.\n"
                   "If this is not a filter function, surround with quotes.",
                   "EventFilterModel"));

          continue;
        }

        // otherwise we're still parsing the name.

        // name must begin with a letter
        if(s.isEmpty() && !expr[pos].isLetter())
        {
          // scan to the end of what looks like a function name for the error message
          int end = pos;
          while(end < expr.length() && (expr[end].isLetterOrNumber() || expr[end] == QLatin1Char('.')))
            end++;

          trace.length = end - trace.position + 1;

          return trace.setError(
              tr("Invalid filter function name, must begin with a letter.\n"
                 "If this is not a filter function, surround with quotes.",
                 "EventFilterModel"));
        }

        // add this character to the name we're building
        s.append(expr[pos]);
        pos++;
        continue;
      }

      if(state == FunctionExprParams)
      {
        if(expr[pos] == QLatin1Char('('))
        {
          parenDepth++;
        }
        else if(expr[pos] == QLatin1Char(')'))
        {
          parenDepth--;
        }

        if(parenDepth == 0)
        {
          // we've finished the filter function

          // however we expect the end of the expression or whitespace next
          if(pos + 1 < expr.length() && !expr[pos + 1].isSpace())
          {
            trace.length = (pos + 1) - trace.position + 1;
            return trace.setError(
                tr("Unexpected character after filter function", "EventFilterModel"));
          }

          // reset errors, we'll fix up the location afterwards depending on what's returned
          ParseTrace subTrace;
          IEventBrowser::EventFilterCallback filter = MakeFunctionMatcher(s, params, subTrace);

          if(!filter)
          {
            // if the errors returned some sub-range for the errors, the position will be
            // relative to the params string so rebase it.
            if(subTrace.position > 0)
            {
              subTrace.position += paramStartPos;
            }
            else
            {
              // otherwise use the whole parent range which includes the function name
              subTrace.position = trace.position;
              subTrace.length = trace.length;
            }

            return subTrace;
          }

          FilterExpression subexpr;

          subexpr.matchType = matchType;
          subexpr.function = true;
          subexpr.name = s;

          subexpr.position = trace.position;
          subexpr.length = trace.length;
          subexpr.exprs = subTrace.exprs;

          for(FilterExpression &f : subexpr.exprs)
          {
            f.position += paramStartPos;
          }

          trace.exprs.push_back(subexpr);

          filters.push_back(EventFilter(filter, matchType));

          s.clear();
          params.clear();

          // move back to the start state
          state = Start;
          matchType = MatchType::Normal;
        }
        else
        {
          params.append(expr[pos]);
        }

        pos++;
        continue;
      }
    }

    trace.length = expr.length() - trace.position;

    // we should be back in the normal state, because all the other states have termination states
    if(state == Literal)
    {
      // shouldn't be possible as the Literal state terminates itself when it sees the end of the
      // string
      return trace.setError(tr("Encountered unterminated literal", "EventFilterModel"));
    }
    else if(state == QuotedExpr)
    {
      return trace.setError(tr("Unterminated quoted expression", "EventFilterModel"));
    }
    else if(state == FunctionExprName)
    {
      return trace.setError(tr("Filter function has no parameters", "EventFilterModel"));
    }
    else if(state == FunctionExprParams)
    {
      return trace.setError(tr("Filter function parameters incomplete", "EventFilterModel"));
    }

    // any - or + should have been consumed by an expression, if we still have it then it was
    // dangling
    if(matchType == MatchType::CantMatch)
      return trace.setError(tr("- expects an expression to exclude", "EventFilterModel"));
    if(matchType == MatchType::MustMatch)
      return trace.setError(tr("+ expects an expression to require", "EventFilterModel"));

    // if we got here, we succeeded. Stop tracking errors
    trace.clearErrors();

    return trace;
  }

  // static so we don't lose this when the event browser is closed and the model is deleted. We
  // could store this in the capture context but it makes more sense logically to keep it here.
  static QMap<QString, CustomFilterCallbacks> m_CustomFilters;
  static QMap<QString, BuiltinFilterCallbacks> m_BuiltinFilters;
};

QMap<QString, CustomFilterCallbacks> EventFilterModel::m_CustomFilters;
QMap<QString, BuiltinFilterCallbacks> EventFilterModel::m_BuiltinFilters;

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

void AddFilterSelections(QTextCursor cursor, int &idx, QColor backCol,
                         const QVector<FilterExpression> &exprs,
                         QList<QTextEdit::ExtraSelection> &sels)
{
  QTextEdit::ExtraSelection sel;

  sel.cursor = cursor;

  for(const FilterExpression &f : exprs)
  {
    QColor col = QColor::fromHslF(float(idx++ % 6) / 6.0f, 1.0f,
                                  qBound(0.05, 0.5 + 0.5 * backCol.lightnessF(), 0.95));

    sel.cursor.setPosition(f.position, QTextCursor::MoveAnchor);
    sel.cursor.setPosition(f.position + f.length, QTextCursor::KeepAnchor);
    sel.format.setBackground(QBrush(col));
    sels.push_back(sel);

    AddFilterSelections(cursor, idx, backCol, f.exprs, sels);
  }
}

ParseErrorTipLabel::ParseErrorTipLabel(QWidget *widget) : QLabel(NULL), m_Widget(widget)
{
  int margin = style()->pixelMetric(QStyle::PM_ToolTipLabelFrameWidth, NULL, this);
  int opacity = style()->styleHint(QStyle::SH_ToolTipLabel_Opacity, NULL, this);

  setWindowFlags(Qt::ToolTip);
  setAttribute(Qt::WA_TransparentForMouseEvents);
  setForegroundRole(QPalette::ToolTipText);
  setBackgroundRole(QPalette::ToolTipBase);
  setMargin(margin + 1);
  setFrameStyle(QFrame::NoFrame);
  setAlignment(Qt::AlignLeft);
  setIndent(1);
  setWindowOpacity(opacity / 255.0);
}

void ParseErrorTipLabel::paintEvent(QPaintEvent *ev)
{
  QStylePainter p(this);
  QStyleOptionFrame opt;
  opt.init(this);
  p.drawPrimitive(QStyle::PE_PanelTipLabel, opt);
  p.end();

  if(!m_Widget->hasFocus())
    GUIInvoke::call(this, [this]() { hide(); });

  QLabel::paintEvent(ev);
}

void ParseErrorTipLabel::resizeEvent(QResizeEvent *e)
{
  QStyleHintReturnMask frameMask;
  QStyleOption option;
  option.init(this);
  if(style()->styleHint(QStyle::SH_ToolTip_Mask, &option, this, &frameMask))
    setMask(frameMask.region);

  QLabel::resizeEvent(e);
}

EventBrowser::EventBrowser(ICaptureContext &ctx, QWidget *parent)
    : QFrame(parent), ui(new Ui::EventBrowser), m_Ctx(ctx)
{
  ui->setupUi(this);

  m_ParseError = new ParseErrorTipLabel(ui->filterExpression);

  clearBookmarks();

  m_Model = new EventItemModel(ui->events, m_Ctx);
  m_FilterModel = new EventFilterModel(m_Model, m_Ctx);

  ui->events->setModel(m_FilterModel);

  m_delegate = new RichTextViewDelegate(ui->events);
  ui->events->setItemDelegate(m_delegate);

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

  m_FilterTimeout = new QTimer(this);
  m_FilterTimeout->setInterval(1200);
  m_FilterTimeout->setSingleShot(true);
  connect(m_FilterTimeout, &QTimer::timeout, this, &EventBrowser::filter_apply);

  QObject::connect(ui->events, &RDTreeView::keyPress, this, &EventBrowser::events_keyPress);
  QObject::connect(ui->events->selectionModel(), &QItemSelectionModel::currentChanged, this,
                   &EventBrowser::events_currentChanged);
  on_find_toggled(false);
  on_filter_toggled(false);
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
                                        [this](QWidget *) { on_HideFind(); });

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

  ui->filterExpression->setSingleLine();
  ui->filterExpression->setHoverTrack();
  ui->filterExpression->enableCompletion();

  // set default filter, include only draws that aren't pop markers
  ui->filterExpression->setText(lit("$draw() -$draw(flags & PopMarker)"));

  QObject::connect(ui->filterExpression, &RDTextEdit::completionBegin, [this](QString prefix) {
    if(m_FilterTimeout->isActive())
      m_FilterTimeout->stop();

    QString context = ui->filterExpression->toPlainText();
    int pos = ui->filterExpression->textCursor().position();
    context.remove(pos, context.length() - pos);

    pos = context.lastIndexOf(QLatin1Char('$'));
    if(pos > 0)
      context.remove(0, pos);

    // if the prefix starts with a $, set completion for all the
    if(prefix.startsWith(QLatin1Char('$')))
    {
      QStringList completions;

      for(const QString &s : m_FilterModel->GetBuiltinFunctions())
        completions.append(QLatin1Char('$') + s);

      for(const QString &s : m_FilterModel->GetCustomFunctions())
        completions.append(QLatin1Char('$') + s);

      ui->filterExpression->setCompletionStrings(completions);
    }
    else if(context.startsWith(QLatin1Char('$')) && context.contains(QLatin1Char('(')))
    {
      pos = context.indexOf(QLatin1Char('('));

      QString filter = context.mid(1, pos - 1);
      context.remove(0, pos + 1);

      ui->filterExpression->setCompletionStrings(m_FilterModel->GetCompletions(filter, context));
    }
    else
    {
      ui->filterExpression->setCompletionStrings({});
    }
  });

  ui->filterExpression->setCompletionWordCharacters(lit("_$"));

  QObject::connect(ui->filterExpression, &RDTextEdit::completionEnd, this,
                   &EventBrowser::on_filterExpression_textChanged);

  if(m_FilterTimeout->isActive())
    m_FilterTimeout->stop();

  filter_apply();

  m_redPalette = palette();
  m_redPalette.setColor(QPalette::Base, Qt::red);

  m_Ctx.AddCaptureViewer(this);
}

EventBrowser::~EventBrowser()
{
  delete m_ParseError;

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

  m_Ctx.BuiltinWindowClosed(this);
  m_Ctx.RemoveCaptureViewer(this);
  delete ui;
}

void EventBrowser::OnCaptureLoaded()
{
  ui->events->setIgnoreBackgroundColors(!m_Ctx.Config().EventBrowser_ColorEventRow);

  m_FilterModel->ResetCache();
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
  ui->timeDraws->setEnabled(true);
  ui->bookmark->setEnabled(true);
  ui->exportDraws->setEnabled(true);
  ui->stepPrev->setEnabled(true);
  ui->stepNext->setEnabled(true);
}

void EventBrowser::OnCaptureClosed()
{
  clearBookmarks();

  on_HideFind();

  m_FilterModel->ResetCache();
  // older Qt versions lose all the sections when a model resets even if the sections don't change.
  // Manually save/restore them
  QVariant p = persistData();
  m_Model->ResetModel();
  setPersistData(p);

  ui->find->setEnabled(false);
  ui->timeDraws->setEnabled(false);
  ui->bookmark->setEnabled(false);
  ui->exportDraws->setEnabled(false);
  ui->stepPrev->setEnabled(false);
  ui->stepNext->setEnabled(false);
}

void EventBrowser::OnEventChanged(uint32_t eventId)
{
  if(!SelectEvent(eventId))
    ui->events->setCurrentIndex(QModelIndex());
  repopulateBookmarks();
  highlightBookmarks();

  m_Model->RefreshCache();
}

void EventBrowser::on_find_toggled(bool checked)
{
  ui->findStrip->setVisible(checked);
  if(checked)
    ui->findEvent->setFocus();
}

void EventBrowser::on_filter_toggled(bool checked)
{
  ui->filterStrip->setVisible(checked);
  if(checked)
    ui->filterExpression->setFocus();
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

  if(selectedEID == m_Ctx.CurSelectedEvent() && effectiveEID == m_Ctx.CurEvent())
    return;

  m_Ctx.SetEventID({this}, selectedEID, effectiveEID);

  m_Model->RefreshCache();

  const DrawcallDescription *draw = m_Ctx.GetDrawcall(selectedEID);

  if(draw && draw->IsFakeMarker())
    draw = &draw->children.back();

  ui->stepPrev->setEnabled(draw && draw->previous);
  ui->stepNext->setEnabled(draw && draw->next);

  // special case for the first draw in the frame
  if(selectedEID == 0)
    ui->stepNext->setEnabled(true);

  // special case for the first 'virtual' draw at EID 0
  if(m_Ctx.GetFirstDrawcall() && selectedEID == m_Ctx.GetFirstDrawcall()->eventId)
    ui->stepPrev->setEnabled(true);

  highlightBookmarks();
}

void EventBrowser::on_HideFind()
{
  ui->findStrip->hide();

  ui->find->setChecked(false);

  ui->findEvent->setText(QString());
  m_Model->SetFindText(QString());
  updateFindResultsAvailable();
}

void EventBrowser::findHighlight_timeout()
{
  m_Model->SetFindText(ui->findEvent->text());
  SelectEvent(m_FilterModel->mapFromSource(m_Model->Find(true)));
  updateFindResultsAvailable();
}

void EventBrowser::filter_apply()
{
  if(ui->filterExpression->completionInProgress())
    return;

  // unselect everything while applying the filter, to avoid updating the source model while the
  // filter is processing if the current event is no longer selected
  uint32_t curSelEvent = m_Ctx.CurSelectedEvent();
  ui->events->clearSelection();
  ui->events->setCurrentIndex(QModelIndex());

  ExpansionKeyGen keygen = [](QModelIndex idx, uint) { return idx.data(ROLE_SELECTED_EID).toUInt(); };

  // update the expansion with the current state. Any rows that were hidden won't have their
  // collapsed/expanded state lost by this. We use the EID as a key because it will be the same even
  // if the model index changes (e.g. if some events are shown or hidden)
  ui->events->updateExpansion(m_EventsExpansion, keygen);

  QString expression = ui->filterExpression->toPlainText();

  ParseTrace parseTrace = m_FilterModel->ParseExpression(expression);

  QList<QTextEdit::ExtraSelection> sels;

  m_ParseError->setText(parseTrace.errorText);
  m_ParseError->hide();

  if(parseTrace.hasErrors())
  {
    QTextEdit::ExtraSelection sel;

    sel.cursor = ui->filterExpression->textCursor();

    sel.cursor.movePosition(QTextCursor::StartOfLine);
    sel.cursor.setPosition(parseTrace.position, QTextCursor::MoveAnchor);

    m_ParseErrorPos = ui->filterExpression->cursorRect(sel.cursor).bottomLeft();

    sel.cursor.setPosition(parseTrace.position + parseTrace.length, QTextCursor::KeepAnchor);
    sel.format.setUnderlineStyle(QTextCharFormat::SingleUnderline);
    sel.format.setUnderlineColor(QColor(Qt::red));
    sels.push_back(sel);

    QPoint pos = ui->filterExpression->viewport()->mapToGlobal(m_ParseErrorPos);
    pos.setY(pos.y() + 4);

    if(ui->filterExpression->viewport()->rect().contains(
           ui->filterExpression->viewport()->mapFromGlobal(QCursor::pos())))
    {
      m_ParseError->move(pos);
      m_ParseError->show();
    }
    else
    {
      m_ParseError->hide();
    }
  }
  else
  {
    int idx = 0;
    AddFilterSelections(ui->filterExpression->textCursor(), idx, palette().color(QPalette::Base),
                        parseTrace.exprs, sels);

    m_ParseError->hide();
  }

  ui->filterExpression->setExtraSelections(sels);

  ui->events->applyExpansion(m_EventsExpansion, keygen);

  ui->events->setCurrentIndex(m_FilterModel->mapFromSource(m_Model->GetIndexForEID(curSelEvent)));
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

void EventBrowser::on_filterExpression_keyPress(QKeyEvent *e)
{
  if(e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter)
  {
    // stop the timer, we'll manually fire it instantly
    if(m_FilterTimeout->isActive())
      m_FilterTimeout->stop();

    filter_apply();
  }

  if(e->key() == Qt::Key_Dollar || e->key() == Qt::Key_Ampersand || e->key() == Qt::Key_Equal ||
     e->key() == Qt::Key_Bar || e->key() == Qt::Key_ParenLeft)
  {
    // force autocompletion for filter functions, as long as we're not inside a quoted string

    QString str = ui->filterExpression->toPlainText();

    bool inQuote = false;
    for(int i = 0; i < str.length(); i++)
    {
      if(!inQuote && str[i] == QLatin1Char('"'))
      {
        inQuote = true;
      }
      else if(inQuote)
      {
        if(str[i] == QLatin1Char('\\'))
        {
          // skip over the next character
          i++;
        }
        else if(str[i] == QLatin1Char('"'))
        {
          inQuote = false;
        }
      }
    }

    if(!inQuote)
      ui->filterExpression->triggerCompletion();
  }
}

void EventBrowser::on_filterExpression_textChanged()
{
  if(!ui->filterExpression->completionInProgress())
    m_FilterTimeout->start();

  m_ParseError->setText(QString());
  m_ParseError->hide();
  ui->filterExpression->setExtraSelections({});
}

void EventBrowser::on_filterExpression_mouseMoved(QMouseEvent *event)
{
  if(!m_ParseError->text().isEmpty())
  {
    QPoint pos = ui->filterExpression->viewport()->mapToGlobal(m_ParseErrorPos);
    pos.setY(pos.y() + 4);

    m_ParseError->move(pos);
    m_ParseError->show();
  }
}

void EventBrowser::on_filterExpression_hoverEnter()
{
  if(!m_ParseError->text().isEmpty())
  {
    QPoint pos = ui->filterExpression->viewport()->mapToGlobal(m_ParseErrorPos);
    pos.setY(pos.y() + 4);

    m_ParseError->move(pos);
    m_ParseError->show();
  }
}

void EventBrowser::on_filterExpression_hoverLeave()
{
  m_ParseError->hide();
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

  if(draw)
    draw = draw->next;

  // special case for the first 'virtual' draw at EID 0
  if(m_Ctx.CurEvent() == 0)
    draw = m_Ctx.GetFirstDrawcall();

  while(draw)
  {
    // try to select the next event. If successful, stop
    if(SelectEvent(draw->eventId))
      return;

    // if it failed, possibly the next draw is filtered out. Step along the list until we find one
    // which isn't
    draw = draw->next;
  }
}

void EventBrowser::on_stepPrev_clicked()
{
  if(!m_Ctx.IsCaptureLoaded() || !ui->stepPrev->isEnabled())
    return;

  const DrawcallDescription *draw = m_Ctx.CurDrawcall();

  if(draw)
    draw = draw->previous;

  while(draw)
  {
    // try to select the previous event. If successful, stop
    if(SelectEvent(draw->eventId))
      return;

    // if it failed, possibly the previous draw is filtered out. Step along the list until we find
    // one which isn't
    draw = draw->previous;
  }

  // special case for the first 'virtual' draw at EID 0
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
    // support Ctrl-G as a legacy shortcut, from the old 'goto EID' which was separate from find
    if(event->key() == Qt::Key_F || event->key() == Qt::Key_G)
    {
      on_find_toggled(true);
      event->accept();
    }
    else if(event->key() == Qt::Key_L)
    {
      on_filter_toggled(true);
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
        if(!SelectEvent(EID))
          ui->events->setCurrentIndex(QModelIndex());
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
  if(!SelectEvent(bookmarks[idx].eventId))
    ui->events->setCurrentIndex(QModelIndex());
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

bool EventBrowser::RegisterEventFilterFunction(const rdcstr &name, EventFilterCallback filter,
                                               FilterParseCallback parser,
                                               AutoCompleteCallback completer)
{
  return EventFilterModel::RegisterEventFilterFunction(name, filter, parser, completer);
}

bool EventBrowser::UnregisterEventFilterFunction(const rdcstr &name)
{
  return EventFilterModel::UnregisterEventFilterFunction(name);
}

void EventBrowser::SetCurrentFilterText(const rdcstr &text)
{
  ui->filterExpression->setPlainText(text);
  filter_apply();
}

rdcstr EventBrowser::GetCurrentFilterText()
{
  return ui->filterExpression->toPlainText();
}

void EventBrowser::SetShowParameterNames(bool show)
{
  m_Model->SetShowParameterNames(show);
}

void EventBrowser::SetShowAllParameters(bool show)
{
  m_Model->SetShowAllParameters(show);
}

void EventBrowser::SetUseCustomDrawNames(bool use)
{
  m_Model->SetUseCustomDrawNames(use);
}

void EventBrowser::SetEmptyRegionsVisible(bool show)
{
  m_FilterModel->SetEmptyRegionsVisible(show);
}
