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

#include "EventBrowser.h"
#include <QAbstractItemModel>
#include <QAbstractSpinBox>
#include <QComboBox>
#include <QCompleter>
#include <QDialogButtonBox>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QScrollBar>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSortFilterProxyModel>
#include <QStringListModel>
#include <QStylePainter>
#include <QTextEdit>
#include <QTimer>
#include "Code/QRDUtils.h"
#include "Code/Resources.h"
#include "Widgets/CollapseGroupBox.h"
#include "Widgets/Extended/RDHeaderView.h"
#include "Widgets/Extended/RDLabel.h"
#include "Widgets/Extended/RDLineEdit.h"
#include "Widgets/Extended/RDListWidget.h"
#include "Widgets/Extended/RDToolButton.h"
#include "Widgets/Extended/RDTreeWidget.h"
#include "Widgets/MarkerBreadcrumbs.h"
#include "flowlayout/FlowLayout.h"
#include "scintilla/include/qt/ScintillaEdit.h"
#include "ui_EventBrowser.h"

struct EventBrowserPersistentStorage : public CustomPersistentStorage
{
  EventBrowserPersistentStorage() : CustomPersistentStorage(rdcstr())
  {
    version = LatestVersion;
    SetDefaultCurrentFilter();
    AddDefaultSavedFilters();
  }
  EventBrowserPersistentStorage(rdcstr name) : CustomPersistentStorage(name)
  {
    version = LatestVersion;
    SetDefaultCurrentFilter();
    AddDefaultSavedFilters();
  }

  void save(QVariant &v) const
  {
    QVariantMap settings;

    settings[lit("version")] = LatestVersion;

    settings[lit("current")] = CurrentFilter;

    QVariantList filters;
    for(const QPair<QString, QString> &f : SavedFilters)
      filters << QVariant(QVariantList({f.first, f.second}));

    settings[lit("filters")] = filters;

    v = settings;
  }

  void load(const QVariant &v)
  {
    QVariantMap settings = v.toMap();

    int loadedVersion = settings[lit("version")].toInt();

    QVariant current = settings[lit("current")];
    if(current.isValid() && current.type() == QVariant::String)
    {
      CurrentFilter = current.toString();
    }
    else
    {
      SetDefaultCurrentFilter();
    }

    SavedFilters.clear();

    QVariant saved = settings[lit("filters")];
    if(saved.isValid() && saved.type() == QVariant::List)
    {
      QVariantList filters = saved.toList();
      for(QVariant filter : filters)
      {
        QVariantList filterPair = filter.toList();
        if(filterPair.count() == 2 && filterPair[0].type() == QVariant::String &&
           filterPair[1].type() == QVariant::String)
        {
          QString name = filterPair[0].toString();
          QString expr = filterPair[1].toString();

          if(!name.isEmpty())
            SavedFilters.push_back({name, expr});
        }
      }
    }
    else
    {
      AddDefaultSavedFilters();
    }

    // version 2 we fixed an issue where a default profile wouldn't have the proper default filter
    // etc. That shipped in v1.15.
    // If we detect an old profile being loaded AND there are: no saved filters, or empty filter, we
    // choose to override and set the default filter/add saved filters. If the user has set a
    // filter, or saved some, we don't do anything as they have used the feature and we'll respect
    // what they've done. If the user has deliberately left it blank we have no way of telling, so
    // we set the default filter to cover the common case of someone who hasn't used the filter at
    // all.
    if(loadedVersion < 2)
    {
      if(CurrentFilter.isEmpty())
        SetDefaultCurrentFilter();

      if(SavedFilters.isEmpty())
        AddDefaultSavedFilters();
    }
  }

  void SetDefaultCurrentFilter() { CurrentFilter = lit("$action()"); }
  void AddDefaultSavedFilters()
  {
    SavedFilters.push_back(qMakePair(lit("Default"), lit("$action()")));
    SavedFilters.push_back(qMakePair(lit("Actions and Barriers"), lit("$action() Barrier")));
    SavedFilters.push_back(qMakePair(lit("Hide Copies & Clears"), lit("$action() -Copy -Clear")));
  }

  static const int LatestVersion = 2;
  int version;
  QString CurrentFilter;
  QList<QPair<QString, QString>> SavedFilters;
};

static EventBrowserPersistentStorage persistantStorage("EventBrowser");

enum
{
  COL_NAME,
  COL_EID,
  COL_ACTION,
  COL_DURATION,
  COL_COUNT,
};

enum
{
  ROLE_SELECTED_EID = Qt::UserRole,
  ROLE_EFFECTIVE_EID,
  ROLE_GROUPED_ACTION,
  ROLE_EXACT_ACTION,
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
    m_MessageCounts.clear();
    m_EIDNameCache.clear();
    m_Actions.clear();
    m_Chunks.clear();
    m_Times.clear();

    if(!m_Ctx.CurRootActions().empty())
      m_Nodes[0] = CreateActionNode(NULL);

    m_CurrentEID = createIndex(0, 0, TagCaptureStart);

    m_Bookmarks.clear();
    m_BookmarkIndices.clear();

    m_FindResults.clear();
    m_FindString.clear();
    m_FindEIDSearch = false;

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
    m_Times.fill(m_Actions.size() + 1, -1.0);

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
    // calculate fake marker durations last, as they have higher eventIds even as parents so are
    // out of order. Normal markers do not have this problem
    QList<uint32_t> fakeMarkers;
    for(auto it = nodeEIDs.rbegin(); it != nodeEIDs.rend(); it++)
    {
      if(m_Nodes[*it].action && m_Nodes[*it].action->IsFakeMarker())
        fakeMarkers.push_back(*it);
      else
        CalculateTotalDuration(m_Nodes[*it]);
    }

    for(uint32_t markerEID : fakeMarkers)
      CalculateTotalDuration(m_Nodes[markerEID]);

    // if we had fake markers recalculate the root node for the frame
    if(!fakeMarkers.empty())
      CalculateTotalDuration(m_Nodes[0]);

    // Qt's item model kind of sucks and doesn't have a good way to say "all data in this column has
    // changed" let alone "all data has changed". dataChanged() is limited to only a group of model
    // indices under a single parent. Instead we just force the view itself to refresh here.
    m_View->viewport()->update();
  }

  bool ShowParameterNames() { return m_ShowParameterNames; }
  void SetShowParameterNames(bool show)
  {
    if(m_ShowParameterNames != show)
    {
      m_ShowParameterNames = show;

      m_EIDNameCache.clear();
      m_View->viewport()->update();
    }
  }

  bool ShowAllParameters() { return m_ShowAllParameters; }
  void SetShowAllParameters(bool show)
  {
    if(m_ShowAllParameters != show)
    {
      m_ShowAllParameters = show;

      m_EIDNameCache.clear();
      m_View->viewport()->update();
    }
  }

  bool UseCustomActionNames() { return m_UseCustomActionNames; }
  void SetUseCustomActionNames(bool use)
  {
    if(m_UseCustomActionNames != use)
    {
      m_UseCustomActionNames = use;

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

    if(!m_Ctx.IsCaptureLoaded())
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
    if(eidOK && eid > 0 && eid < m_Actions.size())
    {
      // if the text doesn't exactly match the EID after converting back, don't treat this as an
      // EID-only search
      eidSearch = (text.trimmed() == QString::number(eid));

      if(eidSearch)
        m_FindResults.push_back(GetIndexForEID(eid));
    }

    m_FindEIDSearch = eidSearch;

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

  QModelIndex Find(bool forward, QModelIndex eidStart)
  {
    if(m_FindResults.empty())
      return QModelIndex();

    // if we're already on a find result we can just zoom to the next one
    int idx = m_FindResults.indexOf(eidStart);
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
    uint32_t eid = data(eidStart, ROLE_EFFECTIVE_EID).toUInt();

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

  int NumFindResults() const { return m_FindResults.count(); }
  bool FindEIDSearch() const { return m_FindEIDSearch; }
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

    if(eid >= m_Actions.size())
      return QModelIndex();

    const ActionDescription *action = m_Actions[eid];
    if(action)
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
      int rowInParent = action->parent ? 0 : 1;

      const rdcarray<ActionDescription> &actions =
          action->parent ? action->parent->children : m_Ctx.CurRootActions();

      for(const ActionDescription &a : actions)
      {
        // the first action with an EID greater than the one we're searching for should contain it.
        if(a.eventId >= eid)
        {
          // except if the action is a fake marker. In this case its own event ID is invalid, so we
          // check the range of its children (knowing it only has one layer of children)
          if(a.IsFakeMarker())
          {
            if(a.eventId == eid)
              break;

            if(a.children[0].eventId < eid)
            {
              rowInParent++;
              continue;
            }
          }

          // keep counting until we get to the row within this action
          for(size_t i = 0; i < a.events.size(); i++)
          {
            if(a.events[i].eventId < eid)
            {
              rowInParent++;
              continue;
            }

            if(a.events[i].eventId != eid)
              qCritical() << "Couldn't find event" << eid << "within action" << action->eventId;

            // stop, we should be at the event now
            break;
          }

          break;
        }

        rowInParent += a.events.count();
      }

      // insert the new element on the front of the cache
      m_RowInParentCache.insert(0, {eid, rowInParent});

      return createIndex(rowInParent, 0, eid);
    }
    else
    {
      qCritical() << "Couldn't find action for event" << eid;
    }

    return QModelIndex();
  }

  const ActionDescription *GetActionForEID(uint32_t eid)
  {
    if(eid < m_Actions.size())
      return m_Actions[eid];

    return NULL;
  }

  rdcstr GetEventName(uint32_t eid)
  {
    if(eid < m_Actions.size())
      return RichResourceTextFormat(m_Ctx, GetCachedEIDName(eid));

    return rdcstr();
  }

  rdcarray<rdcstr> GetMarkerList() const
  {
    rdcarray<rdcstr> ret;

    for(auto it = m_Nodes.begin(); it != m_Nodes.end(); ++it)
      if(it.value().action && (it.value().action->flags & ActionFlags::PushMarker) &&
         !it.value().action->customName.isEmpty())
        ret.push_back(it.value().action->customName);

    return ret;
  }

  QModelIndex GetCurrentEID() const { return m_CurrentEID; }
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

      // the rest are in the root action node
      return GetIndexForActionChildRow(m_Nodes[0], row, column);
    }
    else if(parent.internalId() == TagCaptureStart)
    {
      // no children for this
      return QModelIndex();
    }
    else
    {
      // otherwise the parent is a real action
      auto it = m_Nodes.find(parent.internalId());
      if(it != m_Nodes.end())
        return GetIndexForActionChildRow(*it, row, column);
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

    if(index.internalId() >= m_Actions.size())
      return QModelIndex();

    // otherwise it's an action
    const ActionDescription *action = m_Actions[index.internalId()];

    // if it has no parent action, the parent is the root
    if(action->parent == NULL)
      return createIndex(0, 0, TagRoot);

    // the parent must be a node since it has at least one child (this action), so we'll have a
    // cached
    // index
    return m_Nodes[action->parent->eventId].index;
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
        case COL_ACTION: return lit("Action #");
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
          case COL_ACTION: return lit("0");
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

      if(eid >= m_Actions.size())
        return QVariant();

      if(role == ROLE_SELECTED_EID)
        return eid;

      if(role == ROLE_GROUPED_ACTION)
        return QVariant::fromValue(qulonglong(m_Actions[eid]));

      if(role == ROLE_EXACT_ACTION)
      {
        const ActionDescription *action = m_Actions[eid];
        return action->eventId == eid ? QVariant::fromValue(qulonglong(m_Actions[eid])) : QVariant();
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
        const ActionDescription *action = m_Actions[eid];

        switch(index.column())
        {
          case COL_NAME: return GetCachedEIDName(eid);
          case COL_EID:
          case COL_ACTION:
            if(action->eventId == eid && !action->children.empty())
            {
              uint32_t effectiveEID = eid;
              auto it = m_Nodes.find(eid);
              if(it != m_Nodes.end())
                effectiveEID = it->effectiveEID;

              if(action->IsFakeMarker())
                eid = action->children[0].events[0].eventId;

              if(index.column() == COL_EID)
              {
                return eid == effectiveEID
                           ? QVariant(eid)
                           : QVariant(QFormatStr("%1-%2").arg(eid).arg(effectiveEID));
              }
              else
              {
                uint32_t actionId = action->actionId;
                uint32_t endActionId = m_Actions[effectiveEID]->actionId;

                if(action->IsFakeMarker())
                  actionId = action->children[0].actionId;

                return actionId == endActionId
                           ? QVariant()
                           : QVariant(QFormatStr("%1-%2").arg(actionId).arg(endActionId));
              }
            }

            if(index.column() == COL_EID)
              return eid;
            else
              return action->eventId == eid &&
                             !(action->flags & (ActionFlags::SetMarker | ActionFlags::PushMarker |
                                                ActionFlags::PopMarker))
                         ? QVariant(action->actionId)
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

          const ActionDescription *action = m_Actions[eid];

          // skip events that aren't the actual action
          if(action->eventId != eid)
            return QVariant();

          // if alpha isn't 0, assume the colour is valid
          if((action->flags & (ActionFlags::PushMarker | ActionFlags::SetMarker)) &&
             action->markerColor.w > 0.0f)
          {
            QColor col =
                QColor::fromRgb(qRgb(action->markerColor.x * 255.0f, action->markerColor.y * 255.0f,
                                     action->markerColor.z * 255.0f));

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
  bool m_FindEIDSearch = false;

  int32_t m_RenameCacheID = 0;
  QString m_ParamColCode;

  QMap<uint32_t, int> m_MessageCounts;

  // captures could have a lot of events, 1 million is high but not unreasonable and certainly not
  // an upper bound. We store only 1 pointer per event which gives us reasonable memory usage (i.e.
  // 8MB for that 1-million case on 64-bit) and still lets us look up what we need in O(1) and avoid
  // more expensive lookups when we need the properties for an event.
  // This gives us a pointer for every event ID pointing to the action that contains it.
  rdcarray<const ActionDescription *> m_Actions;
  rdcarray<const SDChunk *> m_Chunks;

  // we can have a bigger structure for every nested node (i.e. action with children).
  // This drastically limits how many we need to worry about - some hierarchies have more nodes than
  // others but even in hierarchies with lots of nodes the number is small, compared to
  // actions/events.
  // we cache this with a depth-first search at init time while populating m_Actions
  struct ActionTreeNode
  {
    const ActionDescription *action;
    uint32_t effectiveEID;

    // cache the index for this node to make parent() significantly faster
    QModelIndex index;

    // this is the number of child events, meaning all the action and all of their events, but *not*
    // the events in any of their children.
    uint32_t rowCount;

    // this is a cache of row index to action. Rather than being present for every row, this is
    // spaced out such that there are roughly Row2EIDFactor entries at most. This means that the
    // O(n) lookup to find the EID for a given row has to process that many times less entries since
    // it can jump to somewhere nearby.
    //
    // The key is the row, the value is the index in the list of children of the action
    QMap<int, size_t> row2action;
    static const int Row2ActionFactor = 100;
  };
  QMap<uint32_t, ActionTreeNode> m_Nodes;

  bool m_ShowParameterNames = false;
  bool m_ShowAllParameters = false;
  bool m_UseCustomActionNames = true;

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

  void CalculateTotalDuration(ActionTreeNode &node)
  {
    const rdcarray<ActionDescription> &actionChildren =
        node.action ? node.action->children : m_Ctx.CurRootActions();

    double duration = 0.0;

    for(const ActionDescription &a : actionChildren)
    {
      // ignore out of bounds EIDs - should not happen
      if(a.eventId >= m_Times.size())
        continue;

      if(qIsNaN(m_Times[a.eventId]) || qIsInf(m_Times[a.eventId]))
        continue;

      // add the time for this event, if it's non-negative. Because we fill out nodes in reverse
      // order, any children that are nodes themselves should be populated by now
      duration += qMax(0.0, m_Times[a.eventId]);
    }

    m_Times[node.action ? node.action->eventId : 0] = duration;
  }

  ActionTreeNode CreateActionNode(const ActionDescription *action)
  {
    const rdcarray<ActionDescription> &actionRange =
        action ? action->children : m_Ctx.CurRootActions();

    // account for the Capture Start row we'll add at the top level
    int row = action ? 0 : 1;

    ActionTreeNode ret;

    ret.action = action;
    ret.effectiveEID = actionRange.back().eventId;
    if(actionRange.back().flags & ActionFlags::PopMarker)
      ret.effectiveEID--;

    ret.row2action[0] = 0;

    uint32_t row2eidStride = (actionRange.count() / ActionTreeNode::Row2ActionFactor) + 1;

    const SDFile &sdfile = m_Ctx.GetStructuredFile();

    for(int i = 0; i < actionRange.count(); i++)
    {
      const ActionDescription &a = actionRange[i];

      if((i % row2eidStride) == 0)
        ret.row2action[row] = i;

      for(const APIEvent &e : a.events)
      {
        m_Actions.resize_for_index(e.eventId);
        m_Chunks.resize_for_index(e.eventId);
        m_Actions[e.eventId] = &a;
        if(e.chunkIndex != APIEvent::NoChunk && e.chunkIndex < sdfile.chunks.size())
          m_Chunks[e.eventId] = sdfile.chunks[e.chunkIndex];
      }

      row += a.events.count();

      if(a.children.empty())
        continue;

      ActionTreeNode node = CreateActionNode(&a);

      node.index = createIndex(row - 1, 0, a.eventId);

      if(a.eventId == ret.effectiveEID)
        ret.effectiveEID = node.effectiveEID;

      m_Nodes[a.eventId] = node;
    }

    ret.rowCount = row;

    return ret;
  }

  QModelIndex GetIndexForActionChildRow(const ActionTreeNode &node, int row, int column) const
  {
    // if the row is out of bounds, bail
    if(row < 0 || (uint32_t)row >= node.rowCount)
      return QModelIndex();

    // we do the linear 'counting' within the actions list to find the event at the given row. It's
    // still essentially O(n) however we keep a limited cached of skip entries at each action node
    // which helps lower the cost significantly (by a factor of the roughly number of skip entries
    // we store).

    const rdcarray<ActionDescription> &actions =
        node.action ? node.action->children : m_Ctx.CurRootActions();
    int curRow = 0;
    size_t curAction = 0;

    // lowerBound doesn't do exactly what we want, we want the first row that is less-equal. So
    // instead we use upperBound and go back one step if we don't get returned the first entry
    auto it = node.row2action.upperBound(row);
    if(it != node.row2action.begin())
      --it;

    // start at the first skip before the desired row
    curRow = it.key();
    curAction = it.value();

    if(curRow > row)
    {
      // we should always have a skip, even if it's only the first child
      qCritical() << "Couldn't find skip for" << row << "in action node"
                  << (node.action ? node.action->eventId : 0);

      // start at the first child row instead
      curRow = 0;
      curAction = 0;
    }

    // if this action doesn't contain the desired row, advance
    while(curAction < actions.size() && curRow + actions[curAction].events.count() <= row)
    {
      curRow += actions[curAction].events.count();
      curAction++;
    }

    // we've iterated all the actions and didn't come up with this row - but we checked above that
    // we're in bounds of rowCount so something went wrong
    if(curAction >= actions.size())
    {
      qCritical() << "Couldn't find action containing row" << row << "in action node"
                  << (node.action ? node.action->eventId : 0);
      return QModelIndex();
    }

    // now curAction contains the row at the idx'th event
    int idx = row - curRow;

    if(idx < 0 || idx >= actions[curAction].events.count())
    {
      qCritical() << "Got invalid relative index for row" << row << "in action node"
                  << (node.action ? node.action->eventId : 0);
      return QModelIndex();
    }

    return createIndex(row, column, actions[curAction].events[idx].eventId);
  }

  QVariant GetCachedEIDName(uint32_t eid) const
  {
    auto it = m_EIDNameCache.find(eid);
    if(it != m_EIDNameCache.end())
      return it.value();

    if(eid == 0)
      return tr("Capture Start");

    if(eid >= m_Actions.size())
      return QVariant();

    const ActionDescription *action = m_Actions[eid];

    QString name;

    // markers always use the action name no matter what (fake markers must because we don't have a
    // chunk to use to name them). This doesn't apply to 'marker' regions that are multiactions or
    // command buffer boundaries.
    if(action->eventId == eid)
    {
      if(m_UseCustomActionNames)
      {
        name = action->customName;
      }
      else
      {
        if((action->flags & (ActionFlags::SetMarker | ActionFlags::PushMarker)) &&
           !(action->flags & (ActionFlags::CommandBufferBoundary | ActionFlags::PassBoundary |
                              ActionFlags::CmdList | ActionFlags::MultiAction)))
          name = action->customName;
      }
    }

    bool forceHTML = false;

    if(name.isEmpty())
    {
      auto eidit = std::lower_bound(action->events.begin(), action->events.end(), eid,
                                    [](const APIEvent &e, uint32_t eid) { return e.eventId < eid; });

      if(eidit != action->events.end() && eidit->eventId == eid)
      {
        const APIEvent &e = *eidit;

        const StructuredChunkList &chunks = m_Ctx.GetStructuredFile().chunks;

        if(e.chunkIndex >= chunks.size())
          return QVariant();

        const SDChunk *chunk = chunks[e.chunkIndex];

        if(chunk == NULL)
          return QVariant();

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
      }

      if(name.isEmpty())
        qCritical() << "Couldn't find APIEvent for" << eid;

      forceHTML = m_ShowParameterNames;
    }

    if(m_MessageCounts.contains(eid))
    {
      int count = m_MessageCounts[eid];
      if(count > 0)
        name += lit(" __rd_msgs::%1:%2").arg(eid).arg(count);
    }

    // force html even for events that don't reference resources etc, to get the italics for
    // parameters
    if(forceHTML)
      name = lit("<rdhtml>") + name + lit("</rdhtml>");

    QVariant v = name;

    RichResourceTextInitialise(v, &m_Ctx);

    m_EIDNameCache[eid] = v;

    return v;
  }

  friend struct EventFilterModel;
};

struct EventFilter
{
  EventFilter(IEventBrowser::EventFilterCallback c) : callback(c), type(MatchType::Normal) {}
  EventFilter(IEventBrowser::EventFilterCallback c, MatchType t) : callback(c), type(t) {}
  IEventBrowser::EventFilterCallback callback;
  MatchType type;
};

static QMap<QString, ActionFlags> ActionFlagsLookup;
static QStringList ActionFlagsList;

void CacheActionFlagsLookup()
{
  if(ActionFlagsLookup.empty())
  {
    for(uint32_t i = 0; i <= 31; i++)
    {
      ActionFlags flag = ActionFlags(1U << i);

      // bit of a hack, see if it's a valid flag by stringising and seeing if it contains
      // ActionFlags(
      QString str = ToQStr(flag);
      if(str.contains(lit("ActionFlags(")))
        continue;

      ActionFlagsList.push_back(str);
      ActionFlagsLookup[str.toLower()] = flag;
    }

    ActionFlagsList.sort();
  }
}

bool EvaluateFilterSet(ICaptureContext &ctx, const rdcarray<EventFilter> &filters, bool all,
                       uint32_t eid, const SDChunk *chunk, const ActionDescription *action,
                       QString name)
{
  if(filters.empty())
    return true;

  bool accept = false;

  bool anyMust = false;
  for(const EventFilter &filter : filters)
    anyMust |= (filter.type == MatchType::MustMatch);

  // if there are MustMatch filters we ignore normals for the sake of matching. We also default
  // to acceptance so that if they all pass then we match. This handles cases like +foo -bar which
  // would return the default on a string like "foothing" which would return a false match if we
  // didn't do this
  if(anyMust)
    accept = true;

  // if any top-level filter matches, we match
  for(const EventFilter &filter : filters)
  {
    // ignore normal filters when we have must matches. Only consider must/can't
    if(anyMust && filter.type == MatchType::Normal)
      continue;

    // if we've already accepted and this is a normal filter and we are in non-all mode, it won't
    // change the result so don't bother running the filter.
    if(accept && !all && filter.type == MatchType::Normal)
      continue;

    bool match = filter.callback(&ctx, rdcstr(), rdcstr(), eid, chunk, action, name);

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
  QString description;
  IEventBrowser::EventFilterCallback filter;
  IEventBrowser::FilterParseCallback parser;
  IEventBrowser::AutoCompleteCallback completer;
};

struct BuiltinFilterCallbacks
{
  QString description;
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

#define MAKE_BUILTIN_FILTER(filter_name)                                               \
  m_BuiltinFilters[lit(STRINGIZE(filter_name))].makeFilter =                           \
                       [this](QString name, QString parameters, ParseTrace &trace) {   \
                         return filterFunction_##filter_name(name, parameters, trace); \
                       };                                                              \
  m_BuiltinFilters[lit(STRINGIZE(filter_name))].description =                          \
                       filterDescription_##filter_name().trimmed();

      MAKE_BUILTIN_FILTER(regex);
      MAKE_BUILTIN_FILTER(param);
      // MAKE_BUILTIN_FILTER(event);
      MAKE_BUILTIN_FILTER(action);
      MAKE_BUILTIN_FILTER(dispatch);
      MAKE_BUILTIN_FILTER(childOf);
      MAKE_BUILTIN_FILTER(parent);

      /*
      m_BuiltinFilters[lit("event")].completer = [this](ICaptureContext *ctx, QString name,
                                                        QString parameters) {
        return filterCompleter_event(ctx, name, parameters);
      };
      */
      m_BuiltinFilters[lit("action")].completer = [this](ICaptureContext *ctx, QString name,
                                                         QString parameters) {
        return filterCompleter_action(ctx, name, parameters);
      };
      m_BuiltinFilters[lit("dispatch")].completer = [this](ICaptureContext *ctx, QString name,
                                                           QString parameters) {
        return filterCompleter_dispatch(ctx, name, parameters);
      };
      m_BuiltinFilters[lit("childOf")].completer = m_BuiltinFilters[lit("parent")].completer =
          [this](ICaptureContext *ctx, QString name, QString parameters) {
            return m_Model->GetMarkerList();
          };
    }
  }
  void ResetCache() { m_VisibleCache.clear(); }
  ParseTrace ParseExpressionToFilters(QString expr, rdcarray<EventFilter> &filters) const;

  void SetFilters(const rdcarray<EventFilter> &filters)
  {
    m_VisibleCache.clear();
    m_Filters = filters;
    invalidateFilter();
  }

  QString GetDescription(QString filter)
  {
    if(m_BuiltinFilters.contains(filter))
      return m_BuiltinFilters[filter].description;
    else if(m_CustomFilters.contains(filter))
      return m_CustomFilters[filter].description;

    return tr("Unknown filter $%1()", "EventFilterModel").arg(filter);
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

  QStringList GetFunctions()
  {
    QStringList ret = m_BuiltinFilters.keys();
    ret << m_CustomFilters.keys();
    ret.sort();
    return ret;
  }

  void SetEmptyRegionsVisible(bool visible) { m_EmptyRegionsVisible = visible; }
  static bool RegisterEventFilterFunction(const rdcstr &name, const rdcstr &description,
                                          IEventBrowser::EventFilterCallback filter,
                                          IEventBrowser::FilterParseCallback parser,
                                          IEventBrowser::AutoCompleteCallback completer)
  {
    if(m_BuiltinFilters.contains(name))
    {
      qCritical() << "Registering filter function" << QString(name) << "which is a builtin function.";
      return false;
    }

    if(m_CustomFilters[name].filter != NULL)
    {
      qCritical() << "Registering filter function" << QString(name) << "which is already registered.";
      return false;
    }

    m_CustomFilters[name] = {description, filter, parser, completer};
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
    const ActionDescription *action =
        (const ActionDescription *)(sourceModel()->data(source_idx, ROLE_GROUPED_ACTION).toULongLong());
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

    m_VisibleCache[eid] = EvaluateFilterSet(m_Ctx, m_Filters, false, eid, chunk, action, name);
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

  IEventBrowser::EventFilterCallback MakeLiteralMatcher(QString string) const
  {
    QString matchString = string.toLower();
    return [matchString](ICaptureContext *, const rdcstr &, const rdcstr &, uint32_t,
                         const SDChunk *, const ActionDescription *, const rdcstr &name) {
      return QString(name).toLower().contains(matchString);
    };
  }

  struct Token
  {
    int position;
    int length;
    QString text;
  };

  static QList<Token> tokenise(QString parameters)
  {
    QList<Token> ret;

    const QString operatorChars = lit("<>=:&");

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

      bool tokenIsOperator = operatorChars.contains(parameters[p]);

      while(p < parameters.size() && !parameters[p].isSpace() &&
            operatorChars.contains(parameters[p]) == tokenIsOperator)
        p++;
      t.length = p - t.position;
      t.text = parameters.mid(t.position, t.length);
      ret.push_back(t);
    }

    return ret;
  }

  QString filterDescription_regex() const
  {
    return tr(R"EOD(
<h3>$regex</h3>

<br />
<table>
<tr><td><code>$regex(/my regex.*match/i)</code></td>- passes if the specified regex matches the event name.</td></tr>
</table>

<p>
This filter can be used to do regex matching against events. The regex itself must
be surrounded with //s. The syntax is perl-like and supports perl compatible options
after the trailing /:
</p>

<table>
<tr><td>/i</td><td>- Case insensitive match</td>
<tr><td>/x</td><td>- Extended syntax. See regex documentation for more information</td>
<tr><td>/u</td><td>- Use unicode properties. Character classes like \w and \d match more than ASCII</td>
</table>
)EOD",
              "EventFilterModel");
  }

  IEventBrowser::EventFilterCallback filterFunction_regex(QString name, QString parameters,
                                                          ParseTrace &trace)
  {
    parameters = parameters.trimmed();

    if(parameters.size() < 2 || parameters[0] != QLatin1Char('/'))
    {
      trace.setError(tr("Parameter to to regex() should be /regex/", "EventFilterModel"));
      return NULL;
    }

    int end = 0;

    for(int i = 1; i < parameters.size(); i++)
    {
      if(parameters[i] == QLatin1Char('\\'))
      {
        i++;
        continue;
      }

      if(parameters[i] == QLatin1Char('/'))
      {
        end = i;
        break;
      }
    }

    if(end == 0)
    {
      trace.setError(tr("Unterminated regex", "EventFilterModel"));
      return NULL;
    }

    QString opts = parameters.right(parameters.size() - end - 1);

    QRegularExpression::PatternOptions reOpts = QRegularExpression::NoPatternOption;

    for(QChar c : opts)
    {
      switch(c.toLatin1())
      {
        case 'i': reOpts |= QRegularExpression::CaseInsensitiveOption; break;
        case 'x': reOpts |= QRegularExpression::ExtendedPatternSyntaxOption; break;
        case 'u': reOpts |= QRegularExpression::UseUnicodePropertiesOption; break;
        default:
          trace.setError(tr("Unexpected option '%1' after regex", "EventFilterModel").arg(c));
          return NULL;
      }
    }

    QRegularExpression regex(parameters.mid(1, end - 1));

    if(!regex.isValid())
    {
      trace.setError(tr("Invalid regex", "EventFilterModel"));
      return NULL;
    }

    return [regex](ICaptureContext *, const rdcstr &, const rdcstr &, uint32_t, const SDChunk *,
                   const ActionDescription *, const rdcstr &name) {
      QRegularExpressionMatch match = regex.match(QString(name));
      return match.isValid() && match.hasMatch();
    };
  }

  QString filterDescription_param() const
  {
    return tr(R"EOD(
<h3>$param</h3>

<br />
<table>
<tr><td><code>$param(name: value)</code><br />
<code>$param(name = value)</code></td><td> - passes if a given parameter matches a value.</td></tr>
</table>

<p>
This filter searches through the parameters to each API call to find a matching name.
The name is specified case-sensitive and can be at any nesting level. The value is
searched for as a case-insensitive substring.
</p>
)EOD",
              "EventFilterModel");
  }

  IEventBrowser::EventFilterCallback filterFunction_param(QString name, QString parameters,
                                                          ParseTrace &trace)
  {
    // $param() => check for a named parameter having a particular value
    int idx = parameters.indexOf(QLatin1Char(':'));

    if(idx < 0)
      idx = parameters.indexOf(QLatin1Char('='));

    if(idx < 0)
    {
      trace.setError(
          tr("Parameter to to $param() should be name: value or name = value", "EventFilterModel"));
      return NULL;
    }

    QString paramName = parameters.mid(0, idx).trimmed();
    QString paramValue = parameters.mid(idx + 1).trimmed();

    if(paramValue.isEmpty())
    {
      trace.setError(
          tr("Parameter to to $param() should be name: value or name = value", "EventFilterModel"));
      return NULL;
    }

    return [paramName, paramValue](ICaptureContext *ctx, const rdcstr &, const rdcstr &, uint32_t,
                                   const SDChunk *chunk, const ActionDescription *, const rdcstr &) {
      if(!chunk)
        return false;

      const SDObject *o = chunk->FindChildRecursively(paramName);

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

  QString filterDescription_event() const
  {
    return tr(R"EOD(
<h3>$event</h3>

<br />
<table>
<tr><td><code>$event(condition)</code></td>- passes if an event property matches a condition.</td></tr>
</table>

<p>
This filter queries given properties of an event to match simple conditions. A
condition must be specified, and only one condition can be queried in each <code>$event()</code>.
</p>

<p>
Available numeric properties. Compare with <code>$event(prop > 100)</code> or <code>$event(prop <= 200)</code>.
</p>

<table>
<tr><td>EID:</td> <td>The event's EID.</td></tr>
</table>
)EOD",
              "EventFilterModel");
  }

  rdcarray<rdcstr> filterCompleter_event(ICaptureContext *ctx, const rdcstr &name,
                                         const rdcstr &params)
  {
    QList<Token> tokens = tokenise(params);

    if(tokens.size() <= 1)
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

    int operatorIdx = -1;

    if(tokens.size() >= 2)
    {
      operatorIdx = operators.indexOf(tokens[1].text);

      if(tokens[1].text == lit("="))
        operatorIdx = 0;
    }

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
                     const SDChunk *, const ActionDescription *action,
                     const rdcstr &) { return eventId == eid; };
      case 1:
        return [eid](ICaptureContext *, const rdcstr &, const rdcstr &, uint32_t eventId,
                     const SDChunk *, const ActionDescription *action,
                     const rdcstr &) { return eventId != eid; };
      case 2:
        return [eid](ICaptureContext *, const rdcstr &, const rdcstr &, uint32_t eventId,
                     const SDChunk *, const ActionDescription *action,
                     const rdcstr &) { return eventId < eid; };
      case 3:
        return [eid](ICaptureContext *, const rdcstr &, const rdcstr &, uint32_t eventId,
                     const SDChunk *, const ActionDescription *action,
                     const rdcstr &) { return eventId > eid; };
      case 4:
        return [eid](ICaptureContext *, const rdcstr &, const rdcstr &, uint32_t eventId,
                     const SDChunk *, const ActionDescription *action,
                     const rdcstr &) { return eventId <= eid; };
      case 5:
        return [eid](ICaptureContext *, const rdcstr &, const rdcstr &, uint32_t eventId,
                     const SDChunk *, const ActionDescription *action,
                     const rdcstr &) { return eventId >= eid; };
      default: trace.setError(tr("Internal error", "EventFilterModel")); return NULL;
    }
  }

  QString filterDescription_action() const
  {
    return tr(R"EOD(
<h3>$action</h3>

<br />
<table>
<tr><td>$action()</td><td>- passes if an event is an action.</td></tr>
<tr><td>$action(condition)</td><td>- passes if an event is an action and matches a condition.</td></tr>
</table>

<p>
If no condition is specified then the event is just included if it's an action - meaning any event
such as draws, dispatches, copies, clears and others that do work and modify resources.<br />
Otherwise the event is included if it's an action AND if the condition is true.
</p>

<p>Available numeric properties. Compare with <code>$action(prop > 100)</code> or <code>$action(prop <= 200)</code></p>

<table>
<tr><td>EID:</td> <td>The action's EID.</td></tr>
<tr><td>actionId:</td> <td>The action ID, starting from 1 and numbering each action.</td></tr>
<tr><td>numIndices:</td> <td>The number of vertices or indices in an action.</td></tr>
<tr><td>baseVertex:</td> <td>The base vertex value for an indexed action.</td></tr>
<tr><td>indexOffset:</td> <td>The index offset for an indexed action.</td></tr>
<tr><td>vertexOffset:</td> <td>The vertex offset for a non-indexed action.</td></tr>
<tr><td>instanceOffset:</td> <td>The instance offset for an instanced action.</td></tr>
<tr><td>dispatchX:</td> <td>The number of groups in the X dimension of a dispatch.</td></tr>
<tr><td>dispatchY:</td> <td>The number of groups in the Y dimension of a dispatch.</td></tr>
<tr><td>dispatchZ:</td> <td>The number of groups in the Z dimension of a dispatch.</td></tr>
<tr><td>dispatchSize:</td> <td>The total number of groups (X * Y * Z) of a dispatch.</td></tr>
<tr><td>duration:</td> <td>The listed duration of an action (only available with durations).</td></tr>
</table>

<p>
Also available is the <code>flags</code> property. Actions have different flags and properties
and these can be queried with a filter such as <code>$action(flags & Clear|ClearDepthStencil)</code>
</p>

<p>The flags available are:</p>

<table>
<tr><td>Clear:</td> <td>This is a clear call, clearing all or a subset of a resource.</td></tr>
<tr><td>Drawcall:</td> <td>This is a graphics pipeline action, rasterizing polygons.</td></tr>
<tr><td>Dispatch:</td> <td>This is a compute dispatch.</td></tr>
<tr><td>CmdList:</td> <td>This is part of the book-keeping for a command list, secondary command buffer or bundle.</td></tr>
<tr><td>SetMarker:</td> <td>This is an individual string debug marker, with no children.</td></tr>
<tr><td>PushMarker:</td> <td>This is a push of a debug marker region, with some child actions</td></tr>
<tr><td>PopMarker:</td> <td>This is the pop of a debug marker region.</td></tr>
<tr><td>Present:</td> <td>This is a graphics present to a window.</td></tr>
<tr><td>MultiAction:</td> <td>This is part of a multi-action, like a MultiDraw or ExecuteIndirect.</td></tr>
<tr><td>Copy:</td> <td>This is a copy call, copying between one resource and another.</td></tr>
<tr><td>Resolve:</td> <td>This is a resolve or non-identical blit, as opposed to a copy.</td></tr>
<tr><td>GenMips:</td> <td>This call is generating mip-maps for a texture</td></tr>
<tr><td>PassBoundary:</td> <td>This is the boundary of a 'pass' explicitly denoted by the API.</td></tr>
<tr><td>Indexed:</td> <td>For graphics pipeline drawcalls, the call uses an index buffer.</td></tr>
<tr><td>Instanced:</td> <td>For graphics pipeline drawcalls, the call uses instancing.</td></tr>
<tr><td>Auto:</td> <td>For graphics pipeline drawcalls, it's automatic based on stream-out.</td></tr>
<tr><td>Indirect:</td> <td>This is an indirect call, sourcing parameters from the GPU.</td></tr>
<tr><td>ClearColor:</td> <td>For clear calls, color values are cleared.</td></tr>
<tr><td>ClearDepthStencil:</td> <td>For clear calls, depth/stencil values are cleared.</td></tr>
<tr><td>BeginPass:</td> <td>For pass boundaries, this begins a pass. A boundary could begin and end.</td></tr>
<tr><td>EndPass:</td> <td>For pass boundaries, this ends a pass. A boundary could begin and end.</td></tr>
<tr><td>CommandBufferBoundary:</td> <td>This denotes the boundary of an entire command buffer.</td></tr>
</table>
)EOD",
              "EventFilterModel");
  }

  rdcarray<rdcstr> filterCompleter_action(ICaptureContext *ctx, const rdcstr &name,
                                          const rdcstr &params)
  {
    QList<Token> tokens = tokenise(params);

    if(tokens.size() <= 1)
      return {
          "EID",
          "actionId",
          "numIndices",
          // most aliases we don't autocomplete but this one we leave
          "numVertices",
          "numInstances",
          "baseVertex",
          "indexOffset",
          "vertexOffset",
          "instanceOffset",
          "dispatchX",
          "dispatchY",
          "dispatchZ",
          "dispatchSize",
          "duration",
          "flags",
      };

    if(tokens[0].text == lit("flags") && tokens.size() >= 2)
    {
      CacheActionFlagsLookup();

      rdcarray<rdcstr> flags;

      for(QString s : ActionFlagsList)
        flags.push_back(s);

      return flags;
    }

    return {};
  }

  IEventBrowser::EventFilterCallback filterFunction_action(QString name, QString parameters,
                                                           ParseTrace &trace)
  {
    // action(...) => returns true only for actions, optionally with a particular property filter
    // PopMarker actions are actions so they can contain the other non-action events that might
    // trail in a group, but we don't count them as real actions for any of this filtering except
    // for
    // flags filtering, where they can be manually included
    QList<Token> tokens = tokenise(parameters);

    // no parameters, just return if it's an action
    if(tokens.isEmpty())
      return [](ICaptureContext *, const rdcstr &, const rdcstr &, uint32_t eventId,
                const SDChunk *, const ActionDescription *action, const rdcstr &) {
        return action->eventId == eventId && !(action->flags & ActionFlags::PopMarker);
      };

    // we upcast to int64_t so we can compare both unsigned and signed values without losing any
    // precision (we don't have any uint64_ts to compare)
    using PropGetter = int64_t (*)(const ActionDescription *);

    struct NamedProp
    {
      const char *name;
      PropGetter getter;
    };

#define NAMED_PROP(name, access)                                            \
  {                                                                         \
    name, [](const ActionDescription *action) -> int64_t { return access; } \
  }

    static const NamedProp namedProps[] = {
        NAMED_PROP("eventid", action->eventId),
        NAMED_PROP("eid", action->eventId),
        NAMED_PROP("actionid", action->actionId),
        NAMED_PROP("numindices", action->numIndices),
        NAMED_PROP("numindexes", action->numIndices),
        NAMED_PROP("numvertices", action->numIndices),
        NAMED_PROP("numvertexes", action->numIndices),
        NAMED_PROP("indexcount", action->numIndices),
        NAMED_PROP("vertexcount", action->numIndices),
        NAMED_PROP("numinstances", action->numInstances),
        NAMED_PROP("instancecount", action->numInstances),
        NAMED_PROP("basevertex", action->baseVertex),
        NAMED_PROP("indexoffset", action->indexOffset),
        NAMED_PROP("vertexoffset", action->vertexOffset),
        NAMED_PROP("instanceoffset", action->instanceOffset),
        NAMED_PROP("dispatchx", action->dispatchDimension[0]),
        NAMED_PROP("dispatchy", action->dispatchDimension[1]),
        NAMED_PROP("dispatchz", action->dispatchDimension[2]),
        NAMED_PROP("dispatchsize", action->dispatchDimension[0] * action->dispatchDimension[1] *
                                       action->dispatchDimension[2]),
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
                                     const ActionDescription *action, const rdcstr &) {
            return action->eventId == eventId && !(action->flags & ActionFlags::PopMarker) &&
                   propGetter(action) == value;
          };
        case 1:
          return [propGetter, value](ICaptureContext *, const rdcstr &, const rdcstr &,
                                     uint32_t eventId, const SDChunk *,
                                     const ActionDescription *action, const rdcstr &) {
            return action->eventId == eventId && !(action->flags & ActionFlags::PopMarker) &&
                   propGetter(action) != value;
          };
        case 2:
          return [propGetter, value](ICaptureContext *, const rdcstr &, const rdcstr &,
                                     uint32_t eventId, const SDChunk *,
                                     const ActionDescription *action, const rdcstr &) {
            return action->eventId == eventId && !(action->flags & ActionFlags::PopMarker) &&
                   propGetter(action) < value;
          };
        case 3:
          return [propGetter, value](ICaptureContext *, const rdcstr &, const rdcstr &,
                                     uint32_t eventId, const SDChunk *,
                                     const ActionDescription *action, const rdcstr &) {
            return action->eventId == eventId && !(action->flags & ActionFlags::PopMarker) &&
                   propGetter(action) > value;
          };
        case 4:
          return [propGetter, value](ICaptureContext *, const rdcstr &, const rdcstr &,
                                     uint32_t eventId, const SDChunk *,
                                     const ActionDescription *action, const rdcstr &) {
            return action->eventId == eventId && !(action->flags & ActionFlags::PopMarker) &&
                   propGetter(action) <= value;
          };
        case 5:
          return [propGetter, value](ICaptureContext *, const rdcstr &, const rdcstr &,
                                     uint32_t eventId, const SDChunk *,
                                     const ActionDescription *action, const rdcstr &) {
            return action->eventId == eventId && !(action->flags & ActionFlags::PopMarker) &&
                   propGetter(action) >= value;
          };
        default: trace.setError(tr("Internal error", "EventFilterModel")); return NULL;
      }
    }
    else if(tokens[0].text.toLower() == lit("duration"))
    {
      // deliberately don't allow equality/inequality
      static const QStringList operators = {
          lit("<"),
          lit(">"),
          lit("<="),
          lit(">="),
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
                                const SDChunk *, const ActionDescription *, const rdcstr &) {
                double dur = m_Model->GetSecondsDurationForEID(eventId);
                return dur >= 0.0 && int64_t(dur * 1000000000.0) < nanoValue;
              };
        case 1:
          return
              [this, nanoValue](ICaptureContext *, const rdcstr &, const rdcstr &, uint32_t eventId,
                                const SDChunk *, const ActionDescription *, const rdcstr &) {
                double dur = m_Model->GetSecondsDurationForEID(eventId);
                return dur >= 0.0 && int64_t(dur * 1000000000.0) > nanoValue;
              };
        case 2:
          return
              [this, nanoValue](ICaptureContext *, const rdcstr &, const rdcstr &, uint32_t eventId,
                                const SDChunk *, const ActionDescription *, const rdcstr &) {
                double dur = m_Model->GetSecondsDurationForEID(eventId);
                return dur >= 0.0 && int64_t(dur * 1000000000.0) <= nanoValue;
              };
        case 3:
          return
              [this, nanoValue](ICaptureContext *, const rdcstr &, const rdcstr &, uint32_t eventId,
                                const SDChunk *, const ActionDescription *, const rdcstr &) {
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
        trace.length = (tokens.back().position + tokens.back().length) - trace.position + 1;
        trace.setError(tr("Expected $action(flags & ...)", "EventFilterModel"));
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
        trace.setError(tr("Invalid action flags expression", "EventFilterModel"));
        return NULL;
      }

      CacheActionFlagsLookup();

      ActionFlags flags = ActionFlags::NoFlags;
      for(const QString &flagString : flagStrings)
      {
        auto it = ActionFlagsLookup.find(flagString.toLower());
        if(it == ActionFlagsLookup.end())
        {
          trace.position = tokens[2].position;
          trace.length = (tokens.back().position + tokens.back().length) - trace.position + 1;
          trace.setError(tr("Unrecognised action flag '%1'", "EventFilterModel").arg(flagString));
          return NULL;
        }

        flags |= it.value();
      }

      return [flags](ICaptureContext *, const rdcstr &, const rdcstr &, uint32_t eventId,
                     const SDChunk *, const ActionDescription *action, const rdcstr &) {
        return action->eventId == eventId && (action->flags & flags);
      };
    }
    else
    {
      trace.setError(tr("Unrecognised property expression", "EventFilterModel"));
      return NULL;
    }
  }

  QString filterDescription_dispatch() const
  {
    return tr(R"EOD(
<h3>$dispatch</h3>

<br />
<table>
<tr><td>$dispatch()</td><td>- passes if an event is a dispatch.</td></tr>
<tr><td>$dispatch(condition)</td><td>- passes if an event is a dispatch and matches a condition.</td></tr>
</table>

<p>
If no condition is specified then the event is just included if it's a dispatch.<br />
Otherwise the event is included if it's a dispatch AND if the condition is true.
</p>

<p>Available numeric properties. Compare with <code>$dispatch(prop > 100)</code> or
<code>$dispatch(prop <= 200)</code></p>

<table>
<tr><td>EID:</td> <td>The dispatch's EID.</td></tr>
<tr><td>actionId:</td> <td>The dispatch's action ID, starting from 1 and numbering each action.</td></tr>
<tr><td>x:</td> <td>The number of groups in the X dimension of the dispatch.</td></tr>
<tr><td>y:</td> <td>The number of groups in the Y dimension of the dispatch.</td></tr>
<tr><td>z:</td> <td>The number of groups in the Z dimension of the dispatch.</td></tr>
<tr><td>size:</td> <td>The total number of groups (X * Y * Z) of the dispatch.</td></tr>
<tr><td>duration:</td> <td>The listed duration of the dispatch (only available with durations).</td></tr>
</table>
)EOD",
              "EventFilterModel");
  }

  rdcarray<rdcstr> filterCompleter_dispatch(ICaptureContext *ctx, const rdcstr &name,
                                            const rdcstr &params)
  {
    QList<Token> tokens = tokenise(params);

    if(tokens.size() <= 1)
      return {
          "EID", "actionId", "x", "y", "z", "size", "duration",
      };

    return {};
  }

  IEventBrowser::EventFilterCallback filterFunction_dispatch(QString name, QString parameters,
                                                             ParseTrace &trace)
  {
    // $dispatch(...) => returns true only for dispatches, optionally with a particular property
    // filter
    QList<Token> tokens = tokenise(parameters);

    // no parameters, just return if it's a dispatch
    if(tokens.isEmpty())
      return [](ICaptureContext *, const rdcstr &, const rdcstr &, uint32_t eventId,
                const SDChunk *, const ActionDescription *action, const rdcstr &) {
        return action->eventId == eventId && (action->flags & ActionFlags::Dispatch);
      };

    // we upcast to int64_t so we can compare both unsigned and signed values without losing any
    // precision (we don't have any uint64_ts to compare)
    using PropGetter = int64_t (*)(const ActionDescription *);

    struct NamedProp
    {
      const char *name;
      PropGetter getter;
    };

    static const NamedProp namedProps[] = {
        NAMED_PROP("eventid", action->eventId),
        NAMED_PROP("eid", action->eventId),
        NAMED_PROP("actionid", action->actionId),
        NAMED_PROP("dispatchx", action->dispatchDimension[0]),
        NAMED_PROP("dispatchy", action->dispatchDimension[1]),
        NAMED_PROP("dispatchz", action->dispatchDimension[2]),
        NAMED_PROP("x", action->dispatchDimension[0]),
        NAMED_PROP("y", action->dispatchDimension[1]),
        NAMED_PROP("z", action->dispatchDimension[2]),
        NAMED_PROP("dispatchsize", action->dispatchDimension[0] * action->dispatchDimension[1] *
                                       action->dispatchDimension[2]),
        NAMED_PROP("size", action->dispatchDimension[0] * action->dispatchDimension[1] *
                               action->dispatchDimension[2]),
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
                                     const ActionDescription *action, const rdcstr &) {
            return action->eventId == eventId && (action->flags & ActionFlags::Dispatch) &&
                   propGetter(action) == value;
          };
        case 1:
          return [propGetter, value](ICaptureContext *, const rdcstr &, const rdcstr &,
                                     uint32_t eventId, const SDChunk *,
                                     const ActionDescription *action, const rdcstr &) {
            return action->eventId == eventId && (action->flags & ActionFlags::Dispatch) &&
                   propGetter(action) != value;
          };
        case 2:
          return [propGetter, value](ICaptureContext *, const rdcstr &, const rdcstr &,
                                     uint32_t eventId, const SDChunk *,
                                     const ActionDescription *action, const rdcstr &) {
            return action->eventId == eventId && (action->flags & ActionFlags::Dispatch) &&
                   propGetter(action) < value;
          };
        case 3:
          return [propGetter, value](ICaptureContext *, const rdcstr &, const rdcstr &,
                                     uint32_t eventId, const SDChunk *,
                                     const ActionDescription *action, const rdcstr &) {
            return action->eventId == eventId && (action->flags & ActionFlags::Dispatch) &&
                   propGetter(action) > value;
          };
        case 4:
          return [propGetter, value](ICaptureContext *, const rdcstr &, const rdcstr &,
                                     uint32_t eventId, const SDChunk *,
                                     const ActionDescription *action, const rdcstr &) {
            return action->eventId == eventId && (action->flags & ActionFlags::Dispatch) &&
                   propGetter(action) <= value;
          };
        case 5:
          return [propGetter, value](ICaptureContext *, const rdcstr &, const rdcstr &,
                                     uint32_t eventId, const SDChunk *,
                                     const ActionDescription *action, const rdcstr &) {
            return action->eventId == eventId && (action->flags & ActionFlags::Dispatch) &&
                   propGetter(action) >= value;
          };
        default: trace.setError(tr("Internal error", "EventFilterModel")); return NULL;
      }
    }
    else if(tokens[0].text.toLower() == lit("duration"))
    {
      // deliberately don't allow equality/inequality
      static const QStringList operators = {
          lit("<"),
          lit(">"),
          lit("<="),
          lit(">="),
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
                                const SDChunk *, const ActionDescription *action, const rdcstr &) {
                double dur = m_Model->GetSecondsDurationForEID(eventId);
                return action->eventId == eventId && (action->flags & ActionFlags::Dispatch) &&
                       dur >= 0.0 && int64_t(dur * 1000000000.0) < nanoValue;
              };
        case 1:
          return
              [this, nanoValue](ICaptureContext *, const rdcstr &, const rdcstr &, uint32_t eventId,
                                const SDChunk *, const ActionDescription *action, const rdcstr &) {
                double dur = m_Model->GetSecondsDurationForEID(eventId);
                return action->eventId == eventId && (action->flags & ActionFlags::Dispatch) &&
                       dur >= 0.0 && int64_t(dur * 1000000000.0) > nanoValue;
              };
        case 2:
          return
              [this, nanoValue](ICaptureContext *, const rdcstr &, const rdcstr &, uint32_t eventId,
                                const SDChunk *, const ActionDescription *action, const rdcstr &) {
                double dur = m_Model->GetSecondsDurationForEID(eventId);
                return action->eventId == eventId && (action->flags & ActionFlags::Dispatch) &&
                       dur >= 0.0 && int64_t(dur * 1000000000.0) <= nanoValue;
              };
        case 3:
          return
              [this, nanoValue](ICaptureContext *, const rdcstr &, const rdcstr &, uint32_t eventId,
                                const SDChunk *, const ActionDescription *action, const rdcstr &) {
                double dur = m_Model->GetSecondsDurationForEID(eventId);
                return action->eventId == eventId && (action->flags & ActionFlags::Dispatch) &&
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

  QString filterDescription_parent() const
  {
    return tr(R"EOD(
<h3>$parent</h3>

<br />
<table>
<tr><td>$parent(marker)<br />
$childOf(marker)</td>
<td>- passes if an event is contained somewhere under a marker of the given name.</td></tr>
</table>

<p>
Both aliases of this filter function in the same way. Any event that is a child of the given markers
will pass the filter. This applies not just to immediate children but any grandchildren or further
nesting level.
</p>
)EOD",
              "EventFilterModel");
  }

  IEventBrowser::EventFilterCallback filterFunction_parent(QString name, QString parameters,
                                                           ParseTrace &trace)
  {
    QString markerName = parameters.trimmed();

    return [markerName](ICaptureContext *, const rdcstr &, const rdcstr &, uint32_t eventId,
                        const SDChunk *, const ActionDescription *action, const rdcstr &) {
      while(action->parent)
      {
        if(QString(action->parent->customName).contains(markerName, Qt::CaseInsensitive))
          return true;

        action = action->parent;
      }

      return false;
    };
  }

  QString filterDescription_childOf() const { return filterDescription_parent(); }
  IEventBrowser::EventFilterCallback filterFunction_childOf(QString name, QString parameters,
                                                            ParseTrace &trace)
  {
    return filterFunction_parent(name, parameters, trace);
  }

  IEventBrowser::EventFilterCallback MakeFunctionMatcher(QString name, QString parameters,
                                                         ParseTrace &trace) const
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
                                 const SDChunk *chunk, const ActionDescription *action,
                                 const rdcstr &name) {
        return innerFilter(ctx, n, p, eid, chunk, action, name);
      };
    }

    trace.setError(tr("Unknown filter function", "EventFilterModel").arg(name));
    return NULL;
  }

  // static so we don't lose this when the event browser is closed and the model is deleted. We
  // could store this in the capture context but it makes more sense logically to keep it here.
  static QMap<QString, CustomFilterCallbacks> m_CustomFilters;
  static QMap<QString, BuiltinFilterCallbacks> m_BuiltinFilters;
};

ParseTrace EventFilterModel::ParseExpressionToFilters(QString expr, rdcarray<EventFilter> &filters) const
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
  // - We allow nesting with simple () brackets. Unquoted literals cannot contain a ( to avoid
  //   parsing ambiguity between e.g. Foo(Bar Blah) and Foo (Bar Blah).
  //
  // This means there's a simple state machine we can follow from any point.
  //
  // 1) start -> whitespace -> start
  // 2) start -> " -> quoted_expression -> wait until unescaped " -> start
  // 3) start -> $ -> function_expression -> parse name > optional whitespace ->
  //             ( -> wait until matching parenthesis -> ) -> whitespace -> start
  // 4) start -> ( -> nested_expression -> wait until matching unquoted ) -> start
  // 5) start -> anything else -> literal -> wait until whitespace -> start
  //
  // We also have two modifiers:
  //
  // 6) start -> + -> note that next filter is a MustMatch -> start
  // 7) start -> - -> note that next filter is a CantMatch -> start
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
    Nested,
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
  // if we're parsing a nested expression and we go inside a quoted string (so parentheses should be
  // ignored)
  bool nestQuote = false;

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

      // 6) and 7)
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

      // 4.1) move to nested expression if we see a (
      if(expr[pos] == QLatin1Char('('))
      {
        // only update the position if the match type is normal. If it's mustmatch/cantmatch
        // include everything from the preceeding - or +
        if(matchType == MatchType::Normal)
          trace.position = pos;

        state = Nested;
        parenDepth = 1;
        nestQuote = false;
        pos++;
        paramStartPos = pos;
        continue;
      }

      // don't allow a literal to start with )
      if(expr[pos] == QLatin1Char(')'))
      {
        trace.length = expr.length() - trace.position;
        return trace.setError(
            tr("Invalid function expression\n"
               "Unexpected close parenthesis, if this should be a literal match surround it in "
               "quotes.",
               "EventFilterModel"));
      }

      // only update the position if the match type is normal. If it's mustmatch/cantmatch
      // include everything from the preceeding - or +
      if(matchType == MatchType::Normal)
        trace.position = pos;

      // 5.1) for anything else begin parsing a literal expression
      state = Literal;
      // don't continue here, we need to parse the first character of the literal
    }

    if(state == Nested)
    {
      // 4.2) handle quoted strings and escaping
      if(nestQuote && expr[pos] == QLatin1Char('\\'))
      {
        if(pos == expr.length() - 1)
        {
          trace.length = expr.length() - trace.position;
          return trace.setError(tr("Invalid escape sequence in quoted string", "EventFilterModel"));
        }

        // append the escape character. We won't process the " below and nestQuote will be
        // true still, so we'll then also append whatever character is quoted
        params.append(expr[pos]);
        pos++;
      }
      else if(expr[pos] == QLatin1Char('"'))
      {
        // start or stop quoting
        nestQuote = !nestQuote;
      }

      if(!nestQuote)
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
          // we've finished the nested expression

          // however we expect the end of the expression or whitespace next
          if(pos + 1 < expr.length() && !expr[pos + 1].isSpace())
          {
            trace.length = (pos + 1) - trace.position + 1;
            return trace.setError(
                tr("Unexpected character after nested expression", "EventFilterModel"));
          }

          // reset errors, we'll fix up the location afterwards depending on what's returned
          rdcarray<EventFilter> subFilters;
          ParseTrace subTrace = ParseExpressionToFilters(params, subFilters);

          if(subTrace.hasErrors())
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
          else if(subFilters.empty())
          {
            trace.length = pos - trace.position + 1;
            return trace.setError(tr("Unexpected empty nested expression", "EventFilterModel"));
          }

          FilterExpression subexpr;

          subexpr.matchType = matchType;
          subexpr.function = true;
          subexpr.name = lit("$any$");
          subexpr.params = params;

          subexpr.position = trace.position;
          subexpr.length = trace.length;
          subexpr.exprs = subTrace.exprs;

          for(FilterExpression &f : subexpr.exprs)
          {
            f.position += paramStartPos;
          }

          trace.exprs.push_back(subexpr);

          auto filter = [subFilters](ICaptureContext *ctx, const rdcstr &, const rdcstr &,
                                     uint32_t eid, const SDChunk *chunk,
                                     const ActionDescription *action, const rdcstr &name) {
            return EvaluateFilterSet(*ctx, subFilters, false, eid, chunk, action, name);
          };

          filters.push_back(EventFilter(filter, matchType));

          s.clear();
          params.clear();
          nestQuote = false;

          // move back to the start state
          state = Start;
          matchType = MatchType::Normal;

          // skip the )
          pos++;
          continue;
        }
      }

      params.append(expr[pos]);

      pos++;
      continue;
    }

    if(state == QuotedExpr)
    {
      // 2.2) handle escaping
      if(expr[pos] == QLatin1Char('\\'))
      {
        if(pos == expr.length() - 1)
        {
          trace.length = expr.length() - trace.position;
          return trace.setError(tr("Invalid escape sequence in quoted string", "EventFilterModel"));
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
          return trace.setError(tr("Unexpected character after quoted string", "EventFilterModel"));
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
      // illegal character in unquoted literal
      if(expr[pos] == QLatin1Char('(') || expr[pos] == QLatin1Char(')') ||
         expr[pos] == QLatin1Char('$'))
      {
        trace.length = (pos + 1) - trace.position + 1;
        return trace.setError(
            tr("Unexpected character '%1' in unquoted literal, surround in \"quotes\" to allow "
               "special characters",
               "EventFilterModel")
                .arg(expr[pos]));
      }

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
        subexpr.params = params;

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

  // we shouldn't be in any parentheses
  if(parenDepth > 0)
  {
    return trace.setError(tr("Encountered unterminated parenthesis", "EventFilterModel"));
  }

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

void AddFilterSelections(QTextCursor cursor, int &idx, QColor backCol, QColor foreCol,
                         QVector<FilterExpression> &exprs, QList<QTextEdit::ExtraSelection> &sels)
{
  QTextEdit::ExtraSelection sel;

  sel.cursor = cursor;

  bool hasMust = false;
  for(const FilterExpression &f : exprs)
    hasMust |= (f.matchType == MatchType::MustMatch);

  for(FilterExpression &f : exprs)
  {
    bool ignored = false;
    if(hasMust && f.matchType == MatchType::Normal)
      ignored = true;

    // we use 6 colours around the wheel to get reasonable distinction. Note that this is not
    // critical information and selecting each entry will underline it in the phrase, since we want
    // to be colour blind friendly where possible.
    // we avoid the 60 degrees near red (which is at 0) to avoid confusion with errors.
    QColor col = QColor::fromHslF((float(idx) / 6.0f) * (300.0f / 360.0f) + (0.5f / 6.0f),
                                  ignored ? 0.0f : 1.0f,
                                  qBound(0.05, 0.2 * 0.5 + 0.8 * backCol.lightnessF(), 0.95));

    idx = (idx + 1) % 6;

    f.col = col;

    sel.cursor.setPosition(f.position, QTextCursor::MoveAnchor);
    sel.cursor.setPosition(f.position + f.length, QTextCursor::KeepAnchor);
    sel.format.setBackground(QBrush(col));
    sel.format.setForeground(QBrush(contrastingColor(col, foreCol)));
    sel.format.setFontStrikeOut(ignored);

    sels.push_back(sel);

    if(!ignored)
      AddFilterSelections(cursor, idx, backCol, foreCol, f.exprs, sels);
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
  m_ParseTrace = new ParseTrace;

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
  ui->events->header()->setSectionResizeMode(COL_ACTION, QHeaderView::Interactive);
  ui->events->header()->setSectionResizeMode(COL_DURATION, QHeaderView::Interactive);

  ui->events->header()->setMinimumSectionSize(40);

  ui->events->header()->setSectionsMovable(true);

  ui->events->header()->setCascadingSectionResizes(false);

  ui->events->setItemVerticalMargin(0);
  ui->events->setIgnoreIconSize(true);

  ui->events->setColoredTreeLineWidth(3.0f);

  // set up default section layout. This will be overridden in restoreState()
  ui->events->header()->resizeSection(COL_EID, 80);
  ui->events->header()->resizeSection(COL_ACTION, 60);
  ui->events->header()->resizeSection(COL_NAME, 200);
  ui->events->header()->resizeSection(COL_DURATION, 80);

  ui->events->header()->hideSection(COL_ACTION);
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
  ui->find->setChecked(false);
  ui->bookmarkStrip->hide();

  m_BookmarkStripLayout = new FlowLayout(ui->bookmarkStrip, 0, 3, 3);
  m_BookmarkSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

  ui->bookmarkStrip->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
  m_BookmarkStripLayout->addWidget(ui->bookmarkStripHeader);
  m_BookmarkStripLayout->addItem(m_BookmarkSpacer);

  {
    QHBoxLayout *box = new QHBoxLayout(ui->breadcrumbStrip);
    box->setContentsMargins(QMargins(0, 0, 0, 0));
    box->setMargin(0);
    box->setSpacing(0);
    m_Breadcrumbs = new MarkerBreadcrumbs(m_Ctx, this, this);
    box->addWidget(m_Breadcrumbs);

    m_BreadcrumbLocationText = new RDLineEdit();

    m_BreadcrumbLocationEditButton = new RDToolButton();
    m_BreadcrumbLocationEditButton->setIcon(Icons::page_white_edit());
    m_BreadcrumbLocationEditButton->setToolTip(tr("Edit marker location as text"));
    box->addWidget(m_BreadcrumbLocationText);
    box->addWidget(m_BreadcrumbLocationEditButton);
    ui->breadcrumbStrip->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);

    QObject::connect(m_BreadcrumbLocationEditButton, &RDToolButton::clicked, this,
                     &EventBrowser::locationEdit_clicked);
    QObject::connect(m_BreadcrumbLocationText, &RDLineEdit::leave, this,
                     &EventBrowser::location_leave);
    QObject::connect(m_BreadcrumbLocationText, &RDLineEdit::keyPress, this,
                     &EventBrowser::location_keyPress);

    m_Breadcrumbs->setVisible(true);
    m_BreadcrumbLocationEditButton->setVisible(true);
    m_BreadcrumbLocationText->setVisible(false);

    ui->breadcrumbStrip->hide();
  }

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
  ui->filterExpression->setDropDown();
  ui->filterExpression->setHoverTrack();
  ui->filterExpression->enableCompletion();
  ui->filterExpression->setAcceptRichText(false);

  ui->filterExpression->setText(persistantStorage.CurrentFilter);

  m_SavedCompleter = new QCompleter(this);
  m_SavedCompleter->setWidget(ui->filterExpression);
  m_SavedCompleter->setCompletionMode(QCompleter::UnfilteredPopupCompletion);
  m_SavedCompleter->setWrapAround(false);
  m_SavedCompletionModel = new QStringListModel(this);
  m_SavedCompleter->setModel(m_SavedCompletionModel);
  m_SavedCompleter->setCompletionRole(Qt::DisplayRole);

  QObject::connect(m_SavedCompleter, OverloadedSlot<const QModelIndex &>::of(&QCompleter::activated),
                   [this](const QModelIndex &idx) {
                     int i = idx.row();
                     if(i >= 0 && i < persistantStorage.SavedFilters.count())
                     {
                       m_CurrentFilterText->setPlainText(persistantStorage.SavedFilters[i].second);

                       QTextCursor c = m_CurrentFilterText->textCursor();
                       c.movePosition(QTextCursor::EndOfLine);
                       m_CurrentFilterText->setTextCursor(c);
                     }
                   });

  QObject::connect(ui->filterExpression, &RDTextEdit::keyPress, this,
                   &EventBrowser::savedFilter_keyPress);

  QObject::connect(ui->filterExpression, &RDTextEdit::dropDownClicked,
                   [this]() { ShowSavedFilterCompleter(ui->filterExpression); });

  QObject::connect(ui->filterSettings, &QToolButton::clicked, this,
                   &EventBrowser::filterSettings_clicked);

  QObject::connect(ui->filterExpression, &RDTextEdit::keyPress, this,
                   &EventBrowser::filter_forceCompletion_keyPress);
  QObject::connect(ui->filterExpression, &RDTextEdit::completionBegin, this,
                   &EventBrowser::filter_CompletionBegin);

  ui->filterExpression->setCompletionWordCharacters(lit("_$"));

  QObject::connect(ui->filterExpression, &RDTextEdit::completionEnd, this,
                   &EventBrowser::on_filterExpression_textChanged);

  if(m_FilterTimeout->isActive())
    m_FilterTimeout->stop();

  CreateFilterDialog();

  m_redPalette = palette();
  m_redPalette.setColor(QPalette::Base, Qt::red);

  m_Ctx.AddCaptureViewer(this);
}

EventBrowser::~EventBrowser()
{
  delete m_ParseError;
  delete m_ParseTrace;

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

  filter_apply();

  ui->events->scrollTo(ui->events->model()->index(0, 0));

  clearBookmarks();
  repopulateBookmarks();

  m_Breadcrumbs->ForceRefresh();

  m_Breadcrumbs->setVisible(true);
  m_BreadcrumbLocationEditButton->setVisible(true);
  m_BreadcrumbLocationText->setVisible(false);
  ui->breadcrumbStrip->show();

  ui->find->setEnabled(true);
  ui->timeActions->setEnabled(true);
  ui->bookmark->setEnabled(true);
  ui->exportActions->setEnabled(true);
  ui->stepPrev->setEnabled(true);
  ui->stepNext->setEnabled(true);
}

void EventBrowser::OnCaptureClosed()
{
  clearBookmarks();

  on_HideFind();

  m_Breadcrumbs->setVisible(true);
  m_BreadcrumbLocationEditButton->setVisible(true);
  m_BreadcrumbLocationText->setVisible(false);
  ui->breadcrumbStrip->hide();

  m_FilterModel->ResetCache();
  // older Qt versions lose all the sections when a model resets even if the sections don't change.
  // Manually save/restore them
  QVariant p = persistData();
  m_Model->ResetModel();
  setPersistData(p);

  ui->find->setEnabled(false);
  ui->timeActions->setEnabled(false);
  ui->bookmark->setEnabled(false);
  ui->exportActions->setEnabled(false);
  ui->stepPrev->setEnabled(false);
  ui->stepNext->setEnabled(false);
}

void EventBrowser::OnEventChanged(uint32_t eventId)
{
  if(!SelectEvent(m_Ctx.CurSelectedEvent()))
    ui->events->setCurrentIndex(QModelIndex());
  repopulateBookmarks();
  highlightBookmarks();

  m_Model->RefreshCache();
  m_Breadcrumbs->OnEventChanged(eventId);
}

void EventBrowser::on_find_toggled(bool checked)
{
  ui->findStrip->setVisible(checked);
  if(checked)
    ui->findEvent->setFocus();
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

void EventBrowser::on_timeActions_clicked()
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
  m_Breadcrumbs->OnEventChanged(effectiveEID);

  const ActionDescription *action = m_Ctx.GetAction(selectedEID);

  if(action && action->IsFakeMarker())
    action = &action->children.back();

  ui->stepPrev->setEnabled(action && action->previous);
  ui->stepNext->setEnabled(action && action->next);

  // special case for the first action in the frame
  if(selectedEID == 0)
    ui->stepNext->setEnabled(true);

  // special case for the first 'virtual' action at EID 0
  if(m_Ctx.GetFirstAction() && selectedEID == m_Ctx.GetFirstAction()->eventId)
    ui->stepPrev->setEnabled(true);

  highlightBookmarks();
}

void EventBrowser::on_HideFind()
{
  ui->findStrip->hide();

  ui->find->setChecked(false);

  m_Model->SetFindText(QString());
  ui->findEvent->setPalette(palette());
}

void EventBrowser::findHighlight_timeout()
{
  FindNext(true);
}

void EventBrowser::FindNext(bool forward)
{
  if(!m_Ctx.IsCaptureLoaded())
    return;

  m_Model->SetFindText(ui->findEvent->text());

  // get the first result in this direction
  QModelIndex firstResult = m_Model->Find(forward, m_Model->GetCurrentEID());

  bool forceNone = false;

  // we got a result but it might be filtered out. This also means that for EID selects we'll jump
  // to the next visible EID
  if(firstResult.isValid())
  {
    QModelIndex result = firstResult;
    QModelIndex mappedResult = m_FilterModel->mapFromSource(result);

    // loop if we have a valid result but it's invalid when mapped (i.e. filtered out)
    while(result.isValid() && !mappedResult.isValid())
    {
      // get the next result in this direction. If we're doing an EID search, step EID-wise
      if(m_Model->FindEIDSearch())
        result = m_Model->GetIndexForEID(result.data(ROLE_SELECTED_EID).toUInt() + 1);
      else
        result = m_Model->Find(forward, result);

      // if we've looped around or somehow get an invalid result, break
      if(!result.isValid() || result == firstResult)
        break;

      // map it
      mappedResult = m_FilterModel->mapFromSource(result);

      // the loop condition will break if we got a mapped result
    }

    // if we got a valid visible result, select it
    if(mappedResult.isValid())
    {
      SelectEvent(m_FilterModel->mapFromSource(result));
    }
    else
    {
      // otherwise tell the results below that there are no visible results, even though there might
      // be some that are filtered out
      forceNone = true;
    }
  }

  updateFindResultsAvailable(forceNone);
}

void EventBrowser::CreateFilterDialog()
{
  // we create this dialog manually since it's relatively simple, and it needs fairly tight
  // integration with the main window
  m_FilterSettings.Dialog = new QDialog(this);

  QDialogButtonBox *buttons = new QDialogButtonBox(this);
  RDLabel *explainTitle = new RDLabel(this);
  RDLabel *listLabel = new RDLabel(this);
  RDLabel *filterLabel = new RDLabel(this);
  CollapseGroupBox *settingsGroup = new CollapseGroupBox(this);
  QToolButton *saveFilter = new QToolButton(this);

  QMenu *importExportMenu = new QMenu(this);

  QVBoxLayout *settingsLayout = new QVBoxLayout();
  m_FilterSettings.ShowParams = new QCheckBox(this);
  m_FilterSettings.ShowAll = new QCheckBox(this);
  m_FilterSettings.UseCustom = new QCheckBox(this);

  m_FilterSettings.Notes = new RDLabel(this);
  m_FilterSettings.FuncDocs = new RDTextEdit(this);
  m_FilterSettings.Filter = new RDTextEdit(this);
  m_FilterSettings.FuncList = new QListWidget(this);
  m_FilterSettings.Explanation = new RDTreeWidget(this);

  m_FilterSettings.Dialog->setWindowTitle(tr("Event Filter Configuration"));
  m_FilterSettings.Dialog->setWindowFlags(m_FilterSettings.Dialog->windowFlags() &
                                          ~Qt::WindowContextHelpButtonHint);
  m_FilterSettings.Dialog->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
  m_FilterSettings.Dialog->setMinimumSize(QSize(600, 600));

  settingsGroup->setTitle(tr("General settings"));
  {
    m_FilterSettings.ShowParams->setText(tr("Show parameter names and values"));
    m_FilterSettings.ShowParams->setToolTip(
        tr("Show parameter names in event names well as the values."));
    m_FilterSettings.ShowParams->setCheckable(true);

    QObject::connect(m_FilterSettings.ShowParams, &QCheckBox::toggled,
                     [this](bool on) { m_Model->SetShowParameterNames(on); });

    m_FilterSettings.ShowAll->setText(tr("Show all parameters"));
    m_FilterSettings.ShowAll->setToolTip(
        tr("Show all parameters in each event, instead of only the most relevant."));
    m_FilterSettings.ShowAll->setCheckable(true);

    QObject::connect(m_FilterSettings.ShowAll, &QCheckBox::toggled,
                     [this](bool on) { m_Model->SetShowAllParameters(on); });

    m_FilterSettings.UseCustom->setText(tr("Show custom action names"));
    m_FilterSettings.UseCustom->setToolTip(
        tr("Show custom action names for e.g. indirect draws where the explicit parameters are not "
           "as directly useful."));
    m_FilterSettings.UseCustom->setCheckable(true);

    QObject::connect(m_FilterSettings.UseCustom, &QCheckBox::toggled,
                     [this](bool on) { m_Model->SetUseCustomActionNames(on); });

    settingsLayout->addWidget(m_FilterSettings.ShowParams);
    settingsLayout->addWidget(m_FilterSettings.ShowAll);
    settingsLayout->addWidget(m_FilterSettings.UseCustom);
  }
  settingsGroup->setLayout(settingsLayout);

  buttons->addButton(QDialogButtonBox::Ok);
  QObject::connect(buttons, &QDialogButtonBox::accepted, m_FilterSettings.Dialog, &QDialog::accept);

  filterLabel->setText(tr("Current filter:"));

  m_FilterSettings.Filter->setReadOnly(false);
  m_FilterSettings.Filter->enableCompletion();
  m_FilterSettings.Filter->setAcceptRichText(false);
  m_FilterSettings.Filter->setSingleLine();
  m_FilterSettings.Filter->setDropDown();

  QObject::connect(m_FilterSettings.Filter, &RDTextEdit::keyPress, this,
                   &EventBrowser::filter_forceCompletion_keyPress);
  QObject::connect(m_FilterSettings.Filter, &RDTextEdit::completionBegin, this,
                   &EventBrowser::filter_CompletionBegin);

  m_FilterSettings.Filter->setCompletionWordCharacters(lit("_$"));

  QObject::connect(m_FilterSettings.Filter, &RDTextEdit::completionEnd, m_FilterSettings.Filter,
                   &RDTextEdit::textChanged);

  QObject::connect(m_FilterSettings.Filter, &RDTextEdit::keyPress, this,
                   &EventBrowser::savedFilter_keyPress);

  QObject::connect(m_FilterSettings.Filter, &RDTextEdit::dropDownClicked,
                   [this]() { ShowSavedFilterCompleter(m_FilterSettings.Filter); });

  QObject::connect(saveFilter, &QToolButton::clicked, [this]() {
    QDialog *dialog = new QDialog(this);

    dialog->setWindowTitle(tr("Save filters"));
    dialog->setWindowFlags(dialog->windowFlags() & ~Qt::WindowContextHelpButtonHint);
    dialog->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    RDLineEdit saveName;
    saveName.setPlaceholderText(tr("Name of filter"));

    RDListWidget filters;
    for(const QPair<QString, QString> &f : persistantStorage.SavedFilters)
      filters.addItem(f.first);

    QPushButton saveButton;
    saveButton.setText(tr("Save"));
    saveButton.setIcon(Icons::save());
    QPushButton deleteButton;
    deleteButton.setText(tr("Delete"));

    QDialogButtonBox saveDialogButtons;
    saveDialogButtons.addButton(QDialogButtonBox::Ok);
    QObject::connect(&saveDialogButtons, &QDialogButtonBox::accepted, dialog, &QDialog::accept);

    QGridLayout grid;
    grid.addWidget(&saveName, 0, 0, 1, 1);
    grid.addWidget(&saveButton, 0, 1, 1, 1);
    grid.addWidget(&filters, 1, 0, 1, 1);
    grid.addWidget(&deleteButton, 1, 1, 1, 1, Qt::AlignHCenter | Qt::AlignTop);
    grid.addWidget(&saveDialogButtons, 2, 0, 1, 2);

    dialog->setLayout(&grid);

    auto enterCallback = [&saveButton, &deleteButton](QKeyEvent *e) {
      if(e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter)
      {
        saveButton.click();
        e->accept();
      }

      if(e->key() == Qt::Key_Backspace || e->key() == Qt::Key_Delete)
      {
        deleteButton.click();
        e->accept();
      }
    };

    QObject::connect(&saveName, &RDLineEdit::keyPress, enterCallback);
    QObject::connect(&filters, &RDListWidget::keyPress, enterCallback);
    QObject::connect(&filters, &RDListWidget::itemActivated,
                     [&saveButton](QListWidgetItem *) { saveButton.click(); });

    QObject::connect(&saveName, &RDLineEdit::textChanged, [&saveName, &filters]() {
      for(int i = 0; i < persistantStorage.SavedFilters.count(); i++)
      {
        if(saveName.text().trimmed().toLower() == persistantStorage.SavedFilters[i].first.toLower())
        {
          if(filters.currentRow() != i)
            filters.setCurrentRow(i);
          return;
        }
      }

      filters.setCurrentItem(NULL);
    });

    QObject::connect(&filters, &QListWidget::currentRowChanged, [&saveName](int row) {
      if(row >= 0 && row < persistantStorage.SavedFilters.count())
        saveName.setText(persistantStorage.SavedFilters[row].first);
    });

    QObject::connect(&saveButton, &QPushButton::clicked, [this, dialog, &saveName, &filters]() {
      QString n = saveName.text().trimmed();
      QString f = m_FilterSettings.Filter->toPlainText();

      for(int i = 0; i < persistantStorage.SavedFilters.count(); i++)
      {
        if(n.toLower() == persistantStorage.SavedFilters[i].first.toLower())
        {
          if(persistantStorage.SavedFilters[i].second.trimmed() == f.trimmed())
          {
            dialog->accept();
            return;
          }

          QMessageBox::StandardButton res = RDDialog::question(
              dialog, tr("Delete filter?"),
              tr("Are you sure you want to overwrite the %1 filter? From:\n\n%2\n\nTo:\n\n%3")
                  .arg(persistantStorage.SavedFilters[i].first)
                  .arg(persistantStorage.SavedFilters[i].second)
                  .arg(f),
              QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);

          if(res != QMessageBox::Yes)
            return;

          delete filters.takeItem(i);
          persistantStorage.SavedFilters.erase(persistantStorage.SavedFilters.begin() + i);
          break;
        }
      }

      persistantStorage.SavedFilters.insert(0, {n, f});

      dialog->accept();
    });

    QObject::connect(&deleteButton, &QPushButton::clicked, [dialog, &filters]() {
      QListWidgetItem *item = filters.currentItem();
      if(!item)
        return;

      for(int i = 0; i < persistantStorage.SavedFilters.count(); i++)
      {
        if(item->text().trimmed().toLower() == persistantStorage.SavedFilters[i].first.toLower())
        {
          QMessageBox::StandardButton res =
              RDDialog::question(dialog, tr("Delete filter?"),
                                 tr("Are you sure you want to delete the %1 filter?\n\n%2")
                                     .arg(persistantStorage.SavedFilters[i].first)
                                     .arg(persistantStorage.SavedFilters[i].second),
                                 QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);

          if(res == QMessageBox::Yes)
          {
            delete filters.takeItem(i);
            persistantStorage.SavedFilters.erase(persistantStorage.SavedFilters.begin() + i);
          }

          return;
        }
      }
    });

    RDDialog::show(dialog);

    dialog->deleteLater();
  });

  saveFilter->setAutoRaise(true);
  saveFilter->setIcon(Icons::save());
  saveFilter->setText(tr("Save"));
  saveFilter->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  saveFilter->setToolTip(tr("Save current filter"));
  saveFilter->setPopupMode(QToolButton::MenuButtonPopup);

  QAction *importAction = new QAction(this);
  importAction->setText(tr("Import filter set"));

  QObject::connect(importAction, &QAction::triggered, [this]() {
    QString filename = RDDialog::getOpenFileName(this, tr("Import filter set"), QString(),
                                                 tr("JSON Files (*.json)"));

    if(!filename.isEmpty())
    {
      QFile f(filename);
      if(f.open(QIODevice::ReadOnly | QIODevice::Text))
      {
        QVariantMap filters;
        bool success = LoadFromJSON(filters, f, "rdocFilterSet", 1);

        EventBrowserPersistentStorage storedFilters;
        storedFilters.load(filters);

        success &= !storedFilters.SavedFilters.isEmpty();
        if(success)
        {
          QString prompt =
              tr("Are you sure you want to overwrite the current set of filters with these?\n\n");

          for(int i = 0; i < storedFilters.SavedFilters.count(); i++)
            prompt += QFormatStr("%1: %2\n")
                          .arg(storedFilters.SavedFilters[i].first)
                          .arg(storedFilters.SavedFilters[i].second);

          QMessageBox::StandardButton res =
              RDDialog::question(this, tr("Confirm importing filter set"), prompt,
                                 QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);

          if(res == QMessageBox::Yes)
          {
            persistantStorage.SavedFilters = storedFilters.SavedFilters;
          }
        }
        else
        {
          RDDialog::critical(this, tr("Error importing filter set"),
                             tr("Couldn't parse filter set %1.").arg(filename));
        }
      }
      else
      {
        RDDialog::critical(this, tr("Error importing filter set"),
                           tr("Couldn't open path %1.").arg(filename));
      }
    }
  });

  QAction *exportAction = new QAction(this);
  exportAction->setText(tr("Export filter set"));

  QObject::connect(exportAction, &QAction::triggered, [this]() {
    QString filename = RDDialog::getSaveFileName(this, tr("Export filter set"), QString(),
                                                 tr("JSON Files (*.json)"));

    if(!filename.isEmpty())
    {
      QFile f(filename);
      if(f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
      {
        QVariant v;
        persistantStorage.save(v);

        QVariantMap filters = v.toMap();
        SaveToJSON(filters, f, "rdocFilterSet", 1);
      }
      else
      {
        RDDialog::critical(this, tr("Error exporting filter set"),
                           tr("Couldn't open path %1.").arg(filename));
      }
    }
  });

  importExportMenu->addAction(importAction);
  importExportMenu->addAction(exportAction);

  // disable export if there are no filters to export
  QObject::connect(importExportMenu, &QMenu::aboutToShow, [exportAction]() {
    exportAction->setEnabled(!persistantStorage.SavedFilters.isEmpty());
  });

  saveFilter->setMenu(importExportMenu);

  explainTitle->setText(lit("Show an event if:"));

  m_FilterSettings.Notes->setWordWrap(true);

  listLabel->setText(tr("Available functions"));

  m_FilterSettings.FuncList->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
  m_FilterSettings.FuncList->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);

  m_FilterSettings.Explanation->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
  m_FilterSettings.Explanation->setColumns({lit("explanation")});
  m_FilterSettings.Explanation->setHeaderHidden(true);
  m_FilterSettings.Explanation->setClearSelectionOnFocusLoss(true);

  // set up the same timeout system for filter changes
  m_FilterSettings.Timeout = new QTimer();
  m_FilterSettings.Timeout->setInterval(1200);
  m_FilterSettings.Timeout->setSingleShot(true);

  m_FilterSettings.FuncDocs->setReadOnly(true);
  m_FilterSettings.FuncDocs->setAcceptRichText(false);

  QObject::connect(m_FilterSettings.FuncList, &QListWidget::currentItemChanged,
                   [this](QListWidgetItem *current, QListWidgetItem *) {
                     if(current)
                     {
                       QString f = current->text();
                       if(m_FilterSettings.FuncList->row(current) == 0)
                       {
                         m_FilterSettings.FuncDocs->setHtml(tr(R"EOD(
<h3>General filter help</h3>

<p>
Filters loosely follow a general search-term syntax, by default matching an event
if any of the terms matches. E.g. a filter such as:
<p>

<pre>  Draw Clear Copy</pre>

<p>
would match each string against event names, and include any event that matches
<code>Draw</code> OR <code>Clear</code> OR <code>Copy</code>.
</p>

<p>
You can also exclude matches with - such as:
</p>

<pre>  Draw Clear Copy -Depth</pre>

<p>
which will match as in the first example, except exclude any events that match
<code>Depth<code>.
</p>

<p>
You can also require matches, which overrides any optional matches:
</p>

<pre>  +Draw +Indexed -Instanced</pre>

<p>
which will match only events which match <code>Draw</code> and match <code>Indexed</code> but don't
match <code>Instanced</code>. In this case adding a term with no + or - prefix will be ignored,
since an event will be excluded if it doesn't match all the +required terms
anyway.
</p>

<p>
More complex expressions can be built with nesting:
</p>

<pre>  +Draw +(Indexed Instanced) -Indirect</pre>

<p>
Would match only events matching <code>Draw</code> which also match at least one of
<code>Indexed</code> or <code>Instanced</code> but not <code>Indirect</code>.
</p>

<p>
Finally you can use filter functions for more advanced matching than just
strings. These are documented on the left here, but for example
</p>

<pre>  $action(numIndices > 1000) Indexed</pre>

<p>
will include any action that matches "Indexed" as a plain string match, OR
is an action which renders more than 1000 indices.
</p>
)EOD")
                                                                .trimmed());
                       }
                       else if(f == lit("Literal String"))
                       {
                         m_FilterSettings.FuncDocs->setHtml(tr(R"EOD(
<h3>Literal String</h3>

<br />
<table>
<tr><td>"Literal string"<br />
literal_string</td>
<td>- passes if the event's name contains a literal substring anywhere in its name</td></tr>
</table>

<p>
Any literal string, optionally included in quotes to include special characters
or whitespace, will be case-insensitively matched against the event name. Note that
this doesn't include all parameters, only those that appear in the name summary.
For searching arbitrary parameters consider using the <code>$param()</code> function.
</p>
)EOD")
                                                                .trimmed());
                       }
                       else
                       {
                         f = f.mid(1, f.size() - 3);
                         m_FilterSettings.FuncDocs->setHtml(m_FilterModel->GetDescription(f));
                       }
                     }
                     else
                     {
                       m_FilterSettings.FuncDocs->setText(QString());
                     }
                   });

  // if the filter is changed, clear any current notes/explanation and start the usual timeout
  QObject::connect(m_FilterSettings.Filter, &RDTextEdit::textChanged, [this]() {
    m_FilterSettings.Filter->setExtraSelections({});
    m_FilterSettings.Explanation->clear();
    m_FilterSettings.Notes->clear();

    m_FilterSettings.Timeout->start();
  });

  // if return/enter is hit, immediately fire the timeout
  QObject::connect(m_FilterSettings.Filter, &RDTextEdit::keyPress, [this](QKeyEvent *e) {
    if(e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter)
    {
      // stop the timer, we'll manually fire it instantly
      m_FilterSettings.Timeout->stop();
      m_FilterSettings.Timeout->timeout({});
    }

    if(e->key() == Qt::Key_Down)
    {
      ShowSavedFilterCompleter(m_FilterSettings.Filter);
    }
  });

  QObject::connect(m_FilterSettings.Explanation, &RDTreeWidget::currentItemChanged, this,
                   &EventBrowser::explanation_currentItemChanged);

  QObject::connect(m_FilterSettings.Timeout, &QTimer::timeout, this,
                   &EventBrowser::settings_filterApply);

  QVBoxLayout *layout = new QVBoxLayout();

  QHBoxLayout *filterLayout = new QHBoxLayout();
  {
    filterLayout->addWidget(filterLabel);
    filterLayout->addWidget(m_FilterSettings.Filter);
    filterLayout->addWidget(saveFilter);

    layout->addLayout(filterLayout);
  }

  layout->addWidget(m_FilterSettings.Notes);
  layout->addWidget(explainTitle);
  layout->addWidget(m_FilterSettings.Explanation);

  layout->addWidget(settingsGroup);

  QHBoxLayout *funcsLayout = new QHBoxLayout();

  {
    layout->addWidget(listLabel);
    funcsLayout->addWidget(m_FilterSettings.FuncList);
    funcsLayout->addWidget(m_FilterSettings.FuncDocs);
    layout->addLayout(funcsLayout);
  }

  layout->addWidget(buttons);

  m_FilterSettings.Dialog->setLayout(layout);
}

void EventBrowser::explanation_currentItemChanged(RDTreeWidgetItem *current, RDTreeWidgetItem *prev)
{
  // when an item is selected, its tag should have a QSize containing the expression range which
  // we underline

  QString funcName;

  if(current->dataCount() >= 1)
    funcName = current->text(1);

  QStringList funcs = m_FilterModel->GetFunctions();
  funcs.insert(0, tr("General Help"));
  funcs.insert(1, tr("Literal String"));

  int idx = funcs.indexOf(funcName);
  if(idx >= 0)
    m_FilterSettings.FuncList->setCurrentItem(m_FilterSettings.FuncList->item(idx));

  QList<QTextEdit::ExtraSelection> sels = m_FilterSettings.Filter->extraSelections();

  // remove any previously underlined phrases
  for(auto it = sels.begin(); it != sels.end();)
  {
    if(it->format.underlineStyle() == QTextCharFormat::SingleUnderline)
    {
      it = sels.erase(it);
      continue;
    }

    it++;
  }

  QSize range = current->tag().toSize();

  int pos = range.width();
  int len = range.height();

  if(pos >= 0 && len > 0)
  {
    QTextEdit::ExtraSelection sel;

    sel.cursor = m_FilterSettings.Filter->textCursor();
    sel.cursor.setPosition(pos, QTextCursor::MoveAnchor);

    sel.cursor.setPosition(pos + len, QTextCursor::KeepAnchor);
    sel.format.setUnderlineStyle(QTextCharFormat::SingleUnderline);

    sels.push_back(sel);
  }

  m_FilterSettings.Filter->setExtraSelections(sels);
}

void EventBrowser::settings_filterApply()
{
  if(m_FilterSettings.Timeout->isActive())
    m_FilterSettings.Timeout->stop();

  ParseTrace trace;
  rdcarray<EventFilter> filters;
  trace = m_FilterModel->ParseExpressionToFilters(m_FilterSettings.Filter->toPlainText(), filters);

  QList<QTextEdit::ExtraSelection> sels;

  QPalette pal = m_FilterSettings.Filter->palette();

  if(trace.hasErrors())
  {
    pal.setColor(QPalette::WindowText, QColor(170, 0, 0));
    m_FilterSettings.Notes->setPalette(pal);
    m_FilterSettings.Explanation->clear();

    m_FilterSettings.Notes->setText(trace.errorText.trimmed());

    QTextEdit::ExtraSelection sel;

    sel.cursor = m_FilterSettings.Filter->textCursor();
    sel.cursor.setPosition(trace.position, QTextCursor::MoveAnchor);

    sel.cursor.setPosition(trace.position + trace.length, QTextCursor::KeepAnchor);
    sel.format.setUnderlineStyle(QTextCharFormat::SingleUnderline);
    sel.format.setUnderlineColor(QColor(Qt::red));
    sels.push_back(sel);
  }
  else
  {
    m_FilterSettings.Notes->setPalette(pal);

    int idx = 0;
    QString notesText;

    AddFilterSelections(m_FilterSettings.Filter->textCursor(), idx,
                        m_FilterSettings.Filter->palette().color(QPalette::Base),
                        m_FilterSettings.Filter->palette().color(QPalette::Text), trace.exprs, sels);
    m_FilterSettings.Explanation->clear();
    AddFilterExplanations(m_FilterSettings.Explanation->invisibleRootItem(), trace.exprs, notesText);

    if(trace.exprs.isEmpty())
    {
      m_FilterSettings.Explanation->addTopLevelItem(
          new RDTreeWidgetItem({tr("No filters - all events shown"), QString()}));
    }

    m_FilterSettings.Notes->setText(QFormatStr("<html>%1</html>").arg(notesText.trimmed()));

    ui->filterExpression->setPlainText(m_FilterSettings.Filter->toPlainText());
    m_FilterTimeout->stop();
    filter_apply();
  }

  m_FilterSettings.Filter->setExtraSelections(sels);
  m_FilterSettings.Explanation->expandAllItems(m_FilterSettings.Explanation->invisibleRootItem());
}

void EventBrowser::filterSettings_clicked()
{
  // resolve any current pending filter timeout first
  filter_apply();

  // update the global parameter checkboxes
  m_FilterSettings.ShowParams->setChecked(m_Model->ShowParameterNames());
  m_FilterSettings.ShowAll->setChecked(m_Model->ShowAllParameters());
  m_FilterSettings.UseCustom->setChecked(m_Model->UseCustomActionNames());

  // fill out the list of filter functions with the current list
  m_FilterSettings.FuncList->clear();
  m_FilterSettings.FuncList->addItem(tr("General Help"));
  m_FilterSettings.FuncList->addItem(tr("Literal String"));
  for(QString f : m_FilterModel->GetFunctions())
    m_FilterSettings.FuncList->addItem(QFormatStr("$%1()").arg(f));

  m_FilterSettings.FuncList->setCurrentRow(0);

  m_FilterSettings.Filter->setText(ui->filterExpression->toPlainText());
  // immediately process and apply the filter
  settings_filterApply();

  // show the dialog now
  RDDialog::show(m_FilterSettings.Dialog);

  // if the filter changed and hasn't been applied, update it immediately
  QString filterExpr = m_FilterSettings.Filter->toPlainText();
  if(filterExpr != ui->filterExpression->toPlainText())
  {
    ui->filterExpression->setPlainText(filterExpr);
    filter_apply();
  }
}

void EventBrowser::filter_CompletionBegin(QString prefix)
{
  if(m_FilterTimeout->isActive())
    m_FilterTimeout->stop();

  RDTextEdit *sender = qobject_cast<RDTextEdit *>(QObject::sender());

  QString context = sender->toPlainText();
  int pos = sender->textCursor().position();
  context.remove(pos, context.length() - pos);

  pos = context.lastIndexOf(QLatin1Char('$'));
  if(pos > 0)
    context.remove(0, pos);

  // if the prefix starts with a $, set completion for all the
  if(prefix.startsWith(QLatin1Char('$')))
  {
    QStringList completions;

    for(const QString &s : m_FilterModel->GetFunctions())
      completions.append(QLatin1Char('$') + s);

    sender->setCompletionStrings(completions);
  }
  else if(context.startsWith(QLatin1Char('$')) && context.contains(QLatin1Char('(')) &&
          !context.contains(QLatin1Char(')')))
  {
    pos = context.indexOf(QLatin1Char('('));

    QString filter = context.mid(1, pos - 1);
    context.remove(0, pos + 1);

    sender->setCompletionStrings(m_FilterModel->GetCompletions(filter, context));
  }
  else
  {
    sender->setCompletionStrings({});
  }
}

void EventBrowser::filter_apply()
{
  if(!m_Ctx.IsCaptureLoaded())
    return;

  if(ui->filterExpression->completionInProgress())
    return;

  if(m_FilterTimeout->isActive())
    m_FilterTimeout->stop();

  m_Breadcrumbs->ForceRefresh();

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
  persistantStorage.CurrentFilter = expression;

  rdcarray<EventFilter> filters;
  *m_ParseTrace = m_FilterModel->ParseExpressionToFilters(expression, filters);

  if(m_ParseTrace->hasErrors())
    filters.clear();

  m_FilterModel->SetFilters(filters);

  QList<QTextEdit::ExtraSelection> sels;

  if(m_ParseTrace->errorText.isEmpty())
    m_ParseError->setText(QString());
  else
    m_ParseError->setText(m_ParseTrace->errorText +
                          tr("\n\nOpen filter settings window for syntax help & explanation"));
  m_ParseError->hide();

  if(m_ParseTrace->hasErrors())
  {
    QTextEdit::ExtraSelection sel;

    m_ParseTrace->exprs.clear();

    sel.cursor = ui->filterExpression->textCursor();
    sel.cursor.setPosition(m_ParseTrace->position, QTextCursor::MoveAnchor);

    m_ParseErrorPos = ui->filterExpression->cursorRect(sel.cursor).bottomLeft();

    sel.cursor.setPosition(m_ParseTrace->position + m_ParseTrace->length, QTextCursor::KeepAnchor);
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
    m_ParseError->hide();
  }

  ui->filterExpression->setExtraSelections(sels);

  ui->events->applyExpansion(m_EventsExpansion, keygen);

  ui->events->setCurrentIndex(m_FilterModel->mapFromSource(m_Model->GetIndexForEID(curSelEvent)));
}

void EventBrowser::AddFilterExplanations(RDTreeWidgetItem *root, QVector<FilterExpression> exprs,
                                         QString &notes)
{
  // sort by match type
  std::sort(exprs.begin(), exprs.end(), [](const FilterExpression &a, const FilterExpression &b) {
    return a.matchType < b.matchType;
  });

  bool hasMust = false;
  for(const FilterExpression &f : exprs)
    hasMust |= (f.matchType == MatchType::MustMatch);

  if(hasMust)
  {
    FilterExpression must;

    QString ignored;
    for(const FilterExpression &f : exprs)
    {
      if(f.matchType == MatchType::MustMatch)
      {
        must = f;
      }
      else if(f.matchType == MatchType::Normal)
      {
        if(!ignored.isEmpty())
          ignored += lit(", ");
        ignored += f.printName();
      }
    }

    if(!ignored.isEmpty())
    {
      notes +=
          tr("NOTE: The terms %1 are ignored because must-match terms like <span>+%2</span> take "
             "precedence.\n"
             "There is no sorting amongst matches based on optional keywords, so consider making "
             "them <span>+required</span> to require all of them, or nesting in "
             "<span>+( .. )</span> to require at least one of them.\n")
              .arg(ignored)
              .arg(must.printName());
    }
  }

  bool first = true;
  for(const FilterExpression &f : exprs)
  {
    QString explanation;

    if(f.matchType == MatchType::MustMatch)
    {
      explanation = tr("Must be that: ");
    }
    else if(f.matchType == MatchType::CantMatch)
    {
      explanation = tr("Can't be that: ");
    }
    else
    {
      // omit any normal matches if we have a must, they are pointless and we warn the user in the
      // notes
      if(hasMust)
        continue;

      if(!first)
        explanation = tr("Or: ");

      first = false;
    }

    if(!f.function)
    {
      explanation += tr("Name matches '%1'").arg(f.name);
    }
    else if(f.name == lit("$any$"))
    {
      explanation += tr("Any of...");
    }
    else
    {
      explanation += tr("Function %1 passes").arg(f.printName());
    }

    RDTreeWidgetItem *item =
        new RDTreeWidgetItem({explanation, f.function ? f.name : tr("Literal String")});
    item->setBackgroundColor(f.col);

    item->setTag(QSize(f.position, f.length));

    root->addChild(item);

    AddFilterExplanations(item, f.exprs, notes);
  }
}

void EventBrowser::on_findEvent_textEdited(const QString &arg1)
{
  if(arg1.isEmpty())
  {
    m_FindHighlight->stop();

    m_Model->SetFindText(QString());
    updateFindResultsAvailable(false);
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
  else if(e->key() == Qt::Key_Down)
  {
    ShowSavedFilterCompleter(ui->filterExpression);
  }
  else
  {
    events_keyPress(e);
  }
}

void EventBrowser::filter_forceCompletion_keyPress(QKeyEvent *e)
{
  if(e->key() == Qt::Key_Dollar || e->key() == Qt::Key_Ampersand || e->key() == Qt::Key_Equal ||
     e->key() == Qt::Key_Bar || e->key() == Qt::Key_ParenLeft)
  {
    // force autocompletion for filter functions, as long as we're not inside a quoted string

    RDTextEdit *sender = qobject_cast<RDTextEdit *>(QObject::sender());

    QString str = sender->toPlainText();

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
      sender->triggerCompletion();
  }
}

void EventBrowser::ShowSavedFilterCompleter(RDTextEdit *filter)
{
  QStringList strs;

  for(QPair<QString, QString> &f : persistantStorage.SavedFilters)
    strs << f.first + lit(": ") + f.second;

  m_SavedCompletionModel->setStringList(strs);

  m_SavedCompleter->setWidget(filter);

  QRect r = filter->rect();
  m_SavedCompleter->complete(r);

  m_CurrentFilterText = filter;
}

void EventBrowser::savedFilter_keyPress(QKeyEvent *e)
{
  if(!m_SavedCompleter->popup()->isVisible())
    return;

  switch(e->key())
  {
    // if a completion is in progress ignore any events the completer will process
    case Qt::Key_Return:
    case Qt::Key_Enter:
      m_SavedCompleter->activated(m_SavedCompleter->popup()->selectionModel()->currentIndex());
      m_SavedCompleter->popup()->hide();
      return;
    // allow key scrolling
    case Qt::Key_Up:
    case Qt::Key_Down:
    case Qt::Key_PageUp:
    case Qt::Key_PageDown: return;
    default: break;
  }

  // all other keys close the popup
  m_SavedCompleter->popup()->hide();
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
    {
      FindNext(event->modifiers() & Qt::ShiftModifier ? false : true);
    }

    event->accept();
  }
}

void EventBrowser::on_findNext_clicked()
{
  FindNext(true);
}

void EventBrowser::on_findPrev_clicked()
{
  FindNext(false);
}

void EventBrowser::on_stepNext_clicked()
{
  if(!m_Ctx.IsCaptureLoaded() || !ui->stepNext->isEnabled())
    return;

  const ActionDescription *action = m_Ctx.CurAction();

  if(action)
    action = action->next;

  // special case for the first 'virtual' action at EID 0
  if(m_Ctx.CurEvent() == 0)
    action = m_Ctx.GetFirstAction();

  while(action)
  {
    // try to select the next event. If successful, stop
    if(SelectEvent(action->eventId))
      return;

    // if it failed, possibly the next action is filtered out. Step along the list until we find one
    // which isn't
    action = action->next;
  }
}

void EventBrowser::on_stepPrev_clicked()
{
  if(!m_Ctx.IsCaptureLoaded() || !ui->stepPrev->isEnabled())
    return;

  const ActionDescription *action = m_Ctx.CurAction();

  if(action)
    action = action->previous;

  while(action)
  {
    // try to select the previous event. If successful, stop
    if(SelectEvent(action->eventId))
      return;

    // if it failed, possibly the previous action is filtered out. Step along the list until we find
    // one which isn't
    action = action->previous;
  }

  // special case for the first 'virtual' action at EID 0
  SelectEvent(0);
}

void EventBrowser::on_exportActions_clicked()
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

        QString line = QFormatStr(" EID  | %1 | Action #").arg(lit("Event"), -maxNameLength);

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
          ExportAction(stream, maxNameLength, 0, false,
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

void EventBrowser::ExportAction(QTextStream &writer, int maxNameLength, int indent, bool firstchild,
                                const QModelIndex &idx)
{
  QString nameString = GetExportString(indent, firstchild, idx);

  QModelIndex eidIdx = idx.model()->sibling(idx.row(), COL_EID, idx);
  QModelIndex actionIdx = idx.model()->sibling(idx.row(), COL_ACTION, idx);

  QString line = QFormatStr("%1 | %2 | %3")
                     .arg(eidIdx.data(ROLE_SELECTED_EID).toString(), -5)
                     .arg(nameString, -maxNameLength)
                     .arg(actionIdx.data(Qt::DisplayRole).toString(), -6);

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
    ExportAction(writer, maxNameLength, indent + 1, firstchild, idx.child(i, COL_NAME));
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
    FindNext(event->modifiers() & Qt::ShiftModifier ? false : true);
  }

  if(event->modifiers() == Qt::ControlModifier)
  {
    // support Ctrl-G as a legacy shortcut, from the old 'goto EID' which was separate from find
    if(event->key() == Qt::Key_F || event->key() == Qt::Key_G)
    {
      ui->find->setChecked(true);
      ui->findEvent->setFocus();
      event->accept();
    }
    else if(event->key() == Qt::Key_B)
    {
      on_bookmark_clicked();
      event->accept();
    }
    else if(event->key() == Qt::Key_T)
    {
      on_timeActions_clicked();
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

static QString GetBookmarkDisplayText(const EventBookmark &bookmark)
{
  return bookmark.text.empty() ? QString::number(bookmark.eventId) : (QString)bookmark.text;
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

      QRClickToolButton *but = new QRClickToolButton(this);

      but->setText(GetBookmarkDisplayText(mark));
      but->setCheckable(true);
      but->setAutoRaise(true);
      but->setProperty("eid", EID);
      QObject::connect(but, &QRClickToolButton::clicked, [this, but, EID]() {
        but->setChecked(true);
        if(!SelectEvent(EID))
          ui->events->setCurrentIndex(QModelIndex());
        highlightBookmarks();
      });
      QObject::connect(but, &QRClickToolButton::rightClicked,
                       [this, but, EID]() { bookmarkContextMenu(but, EID); });

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

void EventBrowser::bookmarkContextMenu(QRClickToolButton *button, uint32_t EID)
{
  QMenu contextMenu(this);

  QAction renameBookmark(tr("&Rename"), this);
  QAction deleteBookmark(tr("&Delete"), this);

  renameBookmark.setIcon(Icons::page_white_edit());
  deleteBookmark.setIcon(Icons::del());

  contextMenu.addAction(&renameBookmark);
  contextMenu.addAction(&deleteBookmark);

  QObject::connect(&deleteBookmark, &QAction::triggered, [this, EID]() {
    m_Ctx.RemoveBookmark(EID);
    repopulateBookmarks();
  });

  QObject::connect(&renameBookmark, &QAction::triggered, [this, button, EID]() {
    EventBookmark editedBookmark;
    for(const EventBookmark &bookmark : m_Ctx.GetBookmarks())
    {
      if(bookmark.eventId == EID)
      {
        editedBookmark = bookmark;
        break;
      }
    }

    if(editedBookmark.eventId == EID)
    {
      bool ok;
      editedBookmark.text =
          QInputDialog::getText(this, lit("Rename"), lit("New name:"), QLineEdit::Normal,
                                GetBookmarkDisplayText(editedBookmark), &ok);
      if(ok)
      {
        button->setText(GetBookmarkDisplayText(editedBookmark));
        m_Ctx.SetBookmark(editedBookmark);
      }
    }
  });

  RDDialog::show(&contextMenu, QCursor::pos());
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

void EventBrowser::locationEdit_clicked()
{
  m_BreadcrumbLocationText->setMinimumHeight(m_Breadcrumbs->sizeHint().height());

  m_Breadcrumbs->setVisible(false);
  m_BreadcrumbLocationEditButton->setVisible(false);
  m_BreadcrumbLocationText->setVisible(true);

  if(m_BreadcrumbLocationText->isVisible())
  {
    QVector<const ActionDescription *> path = m_Breadcrumbs->getPath();

    QString pathString;
    for(const ActionDescription *p : path)
    {
      QString name = p->GetName(m_Ctx.GetStructuredFile());
      if(name.isEmpty())
        name = lit("unnamed%1").arg(p->eventId);

      name.replace(QLatin1Char('\\'), lit("\\\\"));
      name.replace(QLatin1Char('/'), lit("\\/"));

      if(!pathString.isEmpty())
        pathString += QLatin1Char('/');

      pathString += name.trimmed();
    }

    m_BreadcrumbLocationText->setText(pathString);
    m_BreadcrumbLocationText->setFocus(Qt::MouseFocusReason);
    m_BreadcrumbLocationText->selectAll();

    m_InitialBreadcrumbLocation = pathString;
  }
}

void EventBrowser::location_leave()
{
  m_Breadcrumbs->setVisible(true);
  m_BreadcrumbLocationEditButton->setVisible(true);
  m_BreadcrumbLocationText->setVisible(false);
}

void EventBrowser::location_keyPress(QKeyEvent *e)
{
  if(e->key() == Qt::Key_Escape)
  {
    return location_leave();
  }
  if(e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter)
  {
    // if the text hasn't changed (ignoring any whitespace added) don't move
    if(m_BreadcrumbLocationText->text().trimmed() == m_InitialBreadcrumbLocation)
      return location_leave();

    QString locationText = m_BreadcrumbLocationText->text();

    // split naively by /
    QStringList elements = locationText.split(QLatin1Char('/'));

    // for each split string, combine any that end with a \ because that means it was an escaped
    // slash like foo\/bar
    for(int i = 0; i < elements.count(); i++)
    {
      if(elements[i][elements[i].count() - 1] == QLatin1Char('\\'))
      {
        elements[i] += elements[i + 1];
        elements.removeAt(i + 1);
        continue;
      }
    }

    // unescape
    for(QString &el : elements)
    {
      el.replace(lit("\\/"), lit("/"));
      el.replace(lit("\\\\"), lit("\\"));
    }

    const ActionDescription *curAction = NULL;
    const rdcarray<ActionDescription> *actions = &m_Ctx.CurRootActions();

    while(!elements.isEmpty())
    {
      QString el = elements.front().trimmed();
      elements.pop_front();

      // ignore empty elements (so foo//bar is equivalent to foo/bar is equivalent to /foo/bar)
      if(el.isEmpty())
        continue;

      // try finding an exact name match first
      bool found = false;
      for(size_t i = 0; i < actions->size(); i++)
      {
        QString name = actions->at(i).GetName(m_Ctx.GetStructuredFile());
        if(name == el)
        {
          found = true;
          curAction = &actions->at(i);
          actions = &curAction->children;
          break;
        }
      }

      // if not, try via unnamed
      if(!found)
      {
        for(size_t i = 0; i < actions->size(); i++)
        {
          QString name = lit("unnamed%1").arg(actions->at(i).eventId);
          if(name == el)
          {
            found = true;
            curAction = &actions->at(i);
            actions = &curAction->children;
            break;
          }
        }
      }

      if(!found)
      {
        QString curActionName;
        if(curAction)
        {
          curActionName = curAction->GetName(m_Ctx.GetStructuredFile());
          if(curActionName.trimmed().isEmpty())
            curActionName = lit("un-named marker at EID %1").arg(curAction->eventId);

          curActionName = QFormatStr("'%1'").arg(curActionName);
        }
        else
        {
          curActionName = tr("root of capture");
        }

        // ignore signals telling us focus is lost while displaying the dialog
        {
          QSignalBlocker block(m_BreadcrumbLocationText);
          RDDialog::critical(this, tr("Error locating marker"),
                             tr("Could not find marker '%1' under %2.\n\nPath: '%3'")
                                 .arg(el)
                                 .arg(curActionName)
                                 .arg(locationText));
        }

        // focus back on the location and select it again
        m_BreadcrumbLocationText->setFocus(Qt::MouseFocusReason);
        m_BreadcrumbLocationText->selectAll();

        return;
      }
    }

    // hide the edit box
    location_leave();

    // go to the action found, and update ourselves because this is not a normal browse.
    if(curAction)
    {
      QModelIndex idx = m_Model->GetIndexForEID(curAction->eventId);

      uint32_t selectedEID = GetSelectedEID(idx);
      uint32_t effectiveEID = GetEffectiveEID(idx);

      m_Ctx.SetEventID({}, selectedEID, effectiveEID);
    }
    else
    {
      m_Ctx.SetEventID({}, 0, 0);
    }
  }
}

void EventBrowser::updateFindResultsAvailable(bool forceNoResults)
{
  if(!forceNoResults && (m_Model->NumFindResults() > 0 || ui->findEvent->text().isEmpty()))
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
  const ActionDescription *action = GetActionForEID(eid);

  if(action)
  {
    for(const APIEvent &ev : action->events)
    {
      if(ev.eventId == eid)
        return ev;
    }
  }

  return APIEvent();
}

const ActionDescription *EventBrowser::GetActionForEID(uint32_t eid)
{
  if(!m_Ctx.IsCaptureLoaded())
    return NULL;

  return m_Model->GetActionForEID(eid);
}

rdcstr EventBrowser::GetEventName(uint32_t eid)
{
  if(!m_Ctx.IsCaptureLoaded())
    return rdcstr();

  return m_Model->GetEventName(eid);
}

bool EventBrowser::IsAPIEventVisible(uint32_t eid)
{
  return m_FilterModel->mapFromSource(m_Model->GetIndexForEID(eid)).isValid();
}

bool EventBrowser::RegisterEventFilterFunction(const rdcstr &name, const rdcstr &description,
                                               EventFilterCallback filter, FilterParseCallback parser,
                                               AutoCompleteCallback completer)
{
  return EventFilterModel::RegisterEventFilterFunction(name, description, filter, parser, completer);
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

void EventBrowser::SetUseCustomActionNames(bool use)
{
  m_Model->SetUseCustomActionNames(use);
}

void EventBrowser::SetEmptyRegionsVisible(bool show)
{
  m_FilterModel->SetEmptyRegionsVisible(show);
}
