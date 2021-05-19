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

#pragma once

#include <QFrame>
#include <QIcon>
#include <QSet>
#include "Code/Interface/QRDInterface.h"

namespace Ui
{
class EventBrowser;
}

class QSpacerItem;
class QToolButton;
class QTimer;
class QTextStream;
class FlowLayout;
struct EventItemTag;

typedef QSet<uint> RDTreeViewExpansionState;

class RichTextViewDelegate;
struct EventItemModel;
struct EventFilterModel;

class EventBrowser : public QFrame, public IEventBrowser, public ICaptureViewer
{
private:
  Q_OBJECT

  Q_PROPERTY(QVariant persistData READ persistData WRITE setPersistData DESIGNABLE false SCRIPTABLE false)

public:
  explicit EventBrowser(ICaptureContext &ctx, QWidget *parent = 0);
  ~EventBrowser();

  // IEventBrowser
  QWidget *Widget() override { return this; }
  void UpdateDurationColumn() override;
  APIEvent GetAPIEventForEID(uint32_t eid) override;
  const DrawcallDescription *GetDrawcallForEID(uint32_t eid) override;
  bool RegisterEventFilterFunction(const rdcstr &name, EventFilterCallback filter,
                                   FilterParseCallback parser) override;
  bool UnregisterEventFilterFunction(const rdcstr &name) override;

  void SetCurrentFilterText(const rdcstr &text) override;
  rdcstr GetCurrentFilterText() override;
  void SetUseCustomDrawNames(bool use) override;
  void SetShowParameterNames(bool show) override;
  void SetShowAllParameters(bool show) override;
  void SetEmptyRegionsVisible(bool show) override;

  // ICaptureViewer
  void OnCaptureLoaded() override;
  void OnCaptureClosed() override;
  void OnSelectedEventChanged(uint32_t eventId) override {}
  void OnEventChanged(uint32_t eventId) override;

  QVariant persistData();
  void setPersistData(const QVariant &persistData);

private slots:
  // automatic slots
  void on_find_toggled(bool checked);
  void on_filter_toggled(bool checked);
  void on_timeDraws_clicked();
  void on_bookmark_clicked();
  void on_HideFind();
  void on_findEvent_returnPressed();
  void on_findEvent_keyPress(QKeyEvent *event);
  void on_findEvent_textEdited(const QString &arg1);
  void on_filterExpression_returnPressed();
  void on_filterExpression_textEdited(const QString &text);
  void on_findNext_clicked();
  void on_findPrev_clicked();
  void on_stepNext_clicked();
  void on_stepPrev_clicked();
  void on_exportDraws_clicked();
  void on_colSelect_clicked();

  // manual slots
  void findHighlight_timeout();
  void filter_apply();
  void events_keyPress(QKeyEvent *event);
  void events_contextMenu(const QPoint &pos);
  void events_currentChanged(const QModelIndex &current, const QModelIndex &previous);

private:
  void ExpandNode(QModelIndex idx);

  bool SelectEvent(uint32_t eventId);
  void SelectEvent(QModelIndex found);

  void updateFindResultsAvailable();

  void clearBookmarks();
  void jumpToBookmark(int idx);
  void repopulateBookmarks();
  void highlightBookmarks();

  int FindEvent(QModelIndex parent, QString filter, uint32_t after, bool forward);
  int FindEvent(QString filter, uint32_t after, bool forward);
  void Find(bool forward);

  QString GetExportString(int indent, bool firstchild, const QModelIndex &idx);
  void GetMaxNameLength(int &maxNameLength, int indent, bool firstchild, const QModelIndex &idx);
  void ExportDrawcall(QTextStream &writer, int maxNameLength, int indent, bool firstchild,
                      const QModelIndex &idx);

  QPalette m_redPalette;

  RichTextViewDelegate *m_delegate = NULL;

  EventItemModel *m_Model;
  EventFilterModel *m_FilterModel;

  TimeUnit m_TimeUnit = TimeUnit::Count;

  RDTreeViewExpansionState m_EventsExpansion;

  QTimer *m_FindHighlight, *m_FilterTimeout;

  FlowLayout *m_BookmarkStripLayout;
  QSpacerItem *m_BookmarkSpacer;
  QMap<uint32_t, QToolButton *> m_BookmarkButtons;

  void RefreshShaderMessages();
  Ui::EventBrowser *ui;
  ICaptureContext &m_Ctx;
};
