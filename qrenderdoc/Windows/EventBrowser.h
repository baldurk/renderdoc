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

#include <QFrame>
#include <QIcon>
#include "Code/Interface/QRDInterface.h"

namespace Ui
{
class EventBrowser;
}

class QSpacerItem;
class QToolButton;
class RDTreeWidgetItem;
class QTimer;
class QTextStream;
class FlowLayout;
struct EventItemTag;

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
  // ICaptureViewer
  void OnCaptureLoaded() override;
  void OnCaptureClosed() override;
  void OnSelectedEventChanged(uint32_t eventId) override {}
  void OnEventChanged(uint32_t eventId) override;

  QVariant persistData();
  void setPersistData(const QVariant &persistData);

private slots:
  // automatic slots
  void on_find_clicked();
  void on_gotoEID_clicked();
  void on_timeDraws_clicked();
  void on_bookmark_clicked();
  void on_HideFindJump();
  void on_jumpToEID_returnPressed();
  void on_findEvent_returnPressed();
  void on_findEvent_keyPress(QKeyEvent *event);
  void on_findEvent_textEdited(const QString &arg1);
  void on_events_currentItemChanged(RDTreeWidgetItem *current, RDTreeWidgetItem *previous);
  void on_findNext_clicked();
  void on_findPrev_clicked();
  void on_stepNext_clicked();
  void on_stepPrev_clicked();
  void on_exportDraws_clicked();
  void on_colSelect_clicked();

  // manual slots
  void findHighlight_timeout();
  void events_keyPress(QKeyEvent *event);
  void events_contextMenu(const QPoint &pos);

public slots:
  void clearBookmarks();
  bool hasBookmark(uint32_t EID);
  void toggleBookmark(uint32_t EID);
  void jumpToBookmark(int idx);

private:
  bool ShouldHide(const DrawcallDescription &drawcall);
  QPair<uint32_t, uint32_t> AddDrawcalls(RDTreeWidgetItem *parent,
                                         const rdcarray<DrawcallDescription> &draws);
  void SetDrawcallTimes(RDTreeWidgetItem *node, const rdcarray<CounterResult> &results);

  void ExpandNode(RDTreeWidgetItem *node);

  bool FindEventNode(RDTreeWidgetItem *&found, RDTreeWidgetItem *parent, uint32_t eventId);
  bool SelectEvent(uint32_t eventId);

  void ClearFindIcons(RDTreeWidgetItem *parent);
  void ClearFindIcons();

  int SetFindIcons(RDTreeWidgetItem *parent, QString filter);
  int SetFindIcons(QString filter);

  void repopulateBookmarks();
  void highlightBookmarks();
  bool hasBookmark(RDTreeWidgetItem *node);

  RDTreeWidgetItem *FindNode(RDTreeWidgetItem *parent, QString filter, uint32_t after);
  int FindEvent(RDTreeWidgetItem *parent, QString filter, uint32_t after, bool forward);
  int FindEvent(QString filter, uint32_t after, bool forward);
  void Find(bool forward);

  QString GetExportDrawcallString(int indent, bool firstchild, const DrawcallDescription &drawcall);
  double GetDrawTime(const DrawcallDescription &drawcall);
  void GetMaxNameLength(int &maxNameLength, int indent, bool firstchild,
                        const DrawcallDescription &drawcall);
  void ExportDrawcall(QTextStream &writer, int maxNameLength, int indent, bool firstchild,
                      const DrawcallDescription &drawcall);

  QPalette m_redPalette;

  TimeUnit m_TimeUnit = TimeUnit::Count;

  rdcarray<CounterResult> m_Times;

  QTimer *m_FindHighlight;

  FlowLayout *m_BookmarkStripLayout;
  QSpacerItem *m_BookmarkSpacer;
  QMap<uint32_t, QToolButton *> m_BookmarkButtons;

  void RefreshIcon(RDTreeWidgetItem *item, EventItemTag tag);

  Ui::EventBrowser *ui;
  ICaptureContext &m_Ctx;
};
