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

#include "EventBrowser.h"
#include <QAbstractSpinBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMenu>
#include <QShortcut>
#include <QTextEdit>
#include <QTimer>
#include "3rdparty/flowlayout/FlowLayout.h"
#include "3rdparty/scintilla/include/qt/ScintillaEdit.h"
#include "Code/QRDUtils.h"
#include "Code/Resources.h"
#include "Widgets/Extended/RDHeaderView.h"
#include "Widgets/Extended/RDListWidget.h"
#include "ui_EventBrowser.h"

struct EventItemTag
{
  EventItemTag() = default;
  EventItemTag(uint32_t eventId) : EID(eventId), lastEID(eventId) {}
  EventItemTag(uint32_t eventId, uint32_t lastEventID) : EID(eventId), lastEID(lastEventID) {}
  uint32_t EID = 0;
  uint32_t lastEID = 0;
  double duration = -1.0;
  bool current = false;
  bool find = false;
  bool bookmark = false;
};

Q_DECLARE_METATYPE(EventItemTag);

enum
{
  COL_NAME,
  COL_EID,
  COL_DRAW,
  COL_DURATION,
  COL_COUNT,
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

  ui->jumpToEID->setFont(Formatter::PreferredFont());
  ui->find->setFont(Formatter::PreferredFont());
  ui->events->setFont(Formatter::PreferredFont());

  ui->events->setColumns(
      {tr("Name"), lit("EID"), lit("Draw #"), lit("Duration - replaced in UpdateDurationColumn")});

  ui->events->setHeader(new RDHeaderView(Qt::Horizontal, this));
  ui->events->header()->setStretchLastSection(true);
  ui->events->header()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);

  // we set up the name column as column 0 so that it gets the tree controls.
  ui->events->header()->setSectionResizeMode(COL_NAME, QHeaderView::Interactive);
  ui->events->header()->setSectionResizeMode(COL_EID, QHeaderView::Interactive);
  ui->events->header()->setSectionResizeMode(COL_DRAW, QHeaderView::Interactive);
  ui->events->header()->setSectionResizeMode(COL_DURATION, QHeaderView::Interactive);

  ui->events->setColumnAlignment(COL_DURATION, Qt::AlignRight | Qt::AlignCenter);

  ui->events->header()->setMinimumSectionSize(40);

  ui->events->header()->setSectionsMovable(true);

  ui->events->header()->setCascadingSectionResizes(false);

  ui->events->setItemVerticalMargin(0);
  ui->events->setIgnoreIconSize(true);

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
  QObject::connect(ui->events, &RDTreeWidget::keyPress, this, &EventBrowser::events_keyPress);
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
  QObject::connect(ui->events, &RDTreeWidget::customContextMenuRequested, this,
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
  RDTreeWidgetItem *frame = new RDTreeWidgetItem(
      {QFormatStr("Frame #%1").arg(m_Ctx.FrameInfo().frameNumber), QString(), QString(), QString()});

  RDTreeWidgetItem *framestart =
      new RDTreeWidgetItem({tr("Frame Start"), lit("0"), lit("0"), QString()});
  framestart->setTag(QVariant::fromValue(EventItemTag(0, 0)));

  frame->addChild(framestart);

  QPair<uint32_t, uint32_t> lastEIDDraw = AddDrawcalls(frame, m_Ctx.CurDrawcalls());
  frame->setTag(QVariant::fromValue(EventItemTag(0, lastEIDDraw.first)));

  ui->events->addTopLevelItem(frame);

  ui->events->expandItem(frame);

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

  ui->events->clear();

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
}

bool EventBrowser::ShouldHide(const DrawcallDescription &drawcall)
{
  if(drawcall.flags & DrawFlags::PushMarker)
  {
    if(m_Ctx.Config().EventBrowser_HideEmpty)
    {
      if(drawcall.children.isEmpty())
        return true;

      bool allhidden = true;

      for(const DrawcallDescription &child : drawcall.children)
      {
        if(ShouldHide(child))
          continue;

        allhidden = false;
        break;
      }

      if(allhidden)
        return true;
    }

    if(m_Ctx.Config().EventBrowser_HideAPICalls)
    {
      if(drawcall.children.isEmpty())
        return false;

      bool onlyapi = true;

      for(const DrawcallDescription &child : drawcall.children)
      {
        if(ShouldHide(child))
          continue;

        if(!(child.flags & DrawFlags::APICalls))
        {
          onlyapi = false;
          break;
        }
      }

      if(onlyapi)
        return true;
    }
  }

  return false;
}

QPair<uint32_t, uint32_t> EventBrowser::AddDrawcalls(RDTreeWidgetItem *parent,
                                                     const rdcarray<DrawcallDescription> &draws)
{
  uint lastEID = 0, lastDraw = 0;

  for(int32_t i = 0; i < draws.count(); i++)
  {
    const DrawcallDescription &d = draws[i];

    if(ShouldHide(d))
      continue;

    QVariant name = QString(d.name);

    RichResourceTextInitialise(name);

    RDTreeWidgetItem *child = new RDTreeWidgetItem(
        {name, QString::number(d.eventId), QString::number(d.drawcallId), lit("---")});

    QPair<uint32_t, uint32_t> last = AddDrawcalls(child, d.children);
    lastEID = last.first;
    lastDraw = last.second;

    if(lastEID > d.eventId)
    {
      child->setText(COL_EID, QFormatStr("%1-%2").arg(d.eventId).arg(lastEID));
      child->setText(COL_DRAW, QFormatStr("%1-%2").arg(d.drawcallId).arg(lastDraw));
    }

    if(lastEID == 0)
    {
      lastEID = d.eventId;
      lastDraw = d.drawcallId;

      if((draws[i].flags & DrawFlags::SetMarker) && i + 1 < draws.count())
        lastEID = draws[i + 1].eventId;
    }

    child->setTag(QVariant::fromValue(EventItemTag(draws[i].eventId, lastEID)));

    if(m_Ctx.Config().EventBrowser_ApplyColors)
    {
      // if alpha isn't 0, assume the colour is valid
      if((d.flags & (DrawFlags::PushMarker | DrawFlags::SetMarker)) && d.markerColor[3] > 0.0f)
      {
        QColor col = QColor::fromRgb(
            qRgb(d.markerColor[0] * 255.0f, d.markerColor[1] * 255.0f, d.markerColor[2] * 255.0f));

        child->setTreeColor(col, 3.0f);

        if(m_Ctx.Config().EventBrowser_ColorEventRow)
        {
          QColor textCol = ui->events->palette().color(QPalette::Text);

          child->setBackgroundColor(col);
          child->setForegroundColor(contrastingColor(col, textCol));
        }
      }
    }

    parent->addChild(child);
  }

  return qMakePair(lastEID, lastDraw);
}

void EventBrowser::SetDrawcallTimes(RDTreeWidgetItem *node, const rdcarray<CounterResult> &results)
{
  if(node == NULL)
    return;

  // parent nodes take the value of the sum of their children
  double duration = 0.0;

  // look up leaf nodes in the dictionary
  if(node->childCount() == 0)
  {
    uint32_t eid = node->tag().value<EventItemTag>().EID;

    duration = -1.0;

    for(const CounterResult &r : results)
    {
      if(r.eventId == eid)
        duration = r.value.d;
    }

    double secs = duration;

    if(m_TimeUnit == TimeUnit::Milliseconds)
      secs *= 1000.0;
    else if(m_TimeUnit == TimeUnit::Microseconds)
      secs *= 1000000.0;
    else if(m_TimeUnit == TimeUnit::Nanoseconds)
      secs *= 1000000000.0;

    node->setText(COL_DURATION, duration < 0.0f ? QString() : Formatter::Format(secs));
    EventItemTag tag = node->tag().value<EventItemTag>();
    tag.duration = duration;
    node->setTag(QVariant::fromValue(tag));

    return;
  }

  for(int i = 0; i < node->childCount(); i++)
  {
    SetDrawcallTimes(node->child(i), results);

    double nd = node->child(i)->tag().value<EventItemTag>().duration;

    if(nd > 0.0)
      duration += nd;
  }

  double secs = duration;

  if(m_TimeUnit == TimeUnit::Milliseconds)
    secs *= 1000.0;
  else if(m_TimeUnit == TimeUnit::Microseconds)
    secs *= 1000000.0;
  else if(m_TimeUnit == TimeUnit::Nanoseconds)
    secs *= 1000000000.0;

  node->setText(COL_DURATION, duration < 0.0f ? QString() : Formatter::Format(secs));
  EventItemTag tag = node->tag().value<EventItemTag>();
  tag.duration = duration;
  node->setTag(QVariant::fromValue(tag));
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
  RDTreeWidgetItem *n = ui->events->currentItem();

  if(n)
    toggleBookmark(n->tag().value<EventItemTag>().lastEID);
}

void EventBrowser::on_timeDraws_clicked()
{
  ANALYTIC_SET(UIFeatures.DrawcallTimes, true);

  ui->events->header()->showSection(COL_DURATION);

  m_Ctx.Replay().AsyncInvoke([this](IReplayController *r) {

    m_Times = r->FetchCounters({GPUCounter::EventGPUDuration});

    GUIInvoke::call(this, [this]() {
      if(ui->events->topLevelItemCount() == 0)
        return;

      SetDrawcallTimes(ui->events->topLevelItem(0), m_Times);
      ui->events->update();
    });
  });
}

void EventBrowser::on_events_currentItemChanged(RDTreeWidgetItem *current, RDTreeWidgetItem *previous)
{
  if(previous)
  {
    EventItemTag tag = previous->tag().value<EventItemTag>();
    tag.current = false;
    previous->setTag(QVariant::fromValue(tag));
    RefreshIcon(previous, tag);
  }

  if(!current)
    return;

  EventItemTag tag = current->tag().value<EventItemTag>();
  tag.current = true;
  current->setTag(QVariant::fromValue(tag));
  RefreshIcon(current, tag);

  m_Ctx.SetEventID({this}, tag.EID, tag.lastEID);

  const DrawcallDescription *draw = m_Ctx.GetDrawcall(tag.lastEID);

  ui->stepPrev->setEnabled(draw && draw->previous);
  ui->stepNext->setEnabled(draw && draw->next);

  // special case for the first draw in the frame
  if(tag.lastEID == 0)
    ui->stepNext->setEnabled(true);

  // special case for the first 'virtual' draw at EID 0
  if(m_Ctx.GetFirstDrawcall() && tag.lastEID == m_Ctx.GetFirstDrawcall()->eventId)
    ui->stepPrev->setEnabled(true);

  highlightBookmarks();
}

void EventBrowser::on_HideFindJump()
{
  ui->jumpStrip->hide();
  ui->findStrip->hide();

  ui->jumpToEID->setText(QString());

  ClearFindIcons();
  ui->findEvent->setPalette(palette());
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
  ClearFindIcons();

  int results = SetFindIcons(ui->findEvent->text());

  if(results > 0)
    ui->findEvent->setPalette(palette());
  else
    ui->findEvent->setPalette(m_redPalette);
}

void EventBrowser::on_findEvent_textEdited(const QString &arg1)
{
  if(arg1.isEmpty())
  {
    m_FindHighlight->stop();

    ui->findEvent->setPalette(palette());
    ClearFindIcons();
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

  if(!ui->findEvent->text().isEmpty())
    Find(true);

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
      Find(event->modifiers() & Qt::ShiftModifier ? false : true);

    findHighlight_timeout();

    event->accept();
  }
}

void EventBrowser::on_findNext_clicked()
{
  Find(true);
}

void EventBrowser::on_findPrev_clicked()
{
  Find(false);
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

        for(const DrawcallDescription &d : m_Ctx.CurDrawcalls())
          GetMaxNameLength(maxNameLength, 0, false, d);

        QString line = QFormatStr(" EID  | %1 | Draw #").arg(lit("Event"), -maxNameLength);

        if(!m_Times.empty())
        {
          line += QFormatStr(" | %1 (%2)").arg(tr("Duration")).arg(ToQStr(m_TimeUnit));
        }

        stream << line << "\n";

        line = QFormatStr("--------%1-----------").arg(QString(), maxNameLength, QLatin1Char('-'));

        if(!m_Times.empty())
        {
          int maxDurationLength = 0;
          maxDurationLength = qMax(maxDurationLength, Formatter::Format(1.0).length());
          maxDurationLength = qMax(maxDurationLength, Formatter::Format(1.2345e-200).length());
          maxDurationLength =
              qMax(maxDurationLength, Formatter::Format(123456.7890123456789).length());
          line += QString(3 + maxDurationLength, QLatin1Char('-'));    // 3 extra for " | "
        }

        stream << line << "\n";

        for(const DrawcallDescription &d : m_Ctx.CurDrawcalls())
          ExportDrawcall(stream, maxNameLength, 0, false, d);
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
  QDialog dialog;
  RDListWidget list;
  QDialogButtonBox buttons;

  dialog.setWindowTitle(tr("Select Event Browser Columns"));
  dialog.setWindowFlags(dialog.windowFlags() & ~Qt::WindowContextHelpButtonHint);

  for(int visIdx = 0; visIdx < COL_COUNT; visIdx++)
  {
    int logIdx = ui->events->header()->logicalIndex(visIdx);

    QListWidgetItem *item = new QListWidgetItem(ui->events->headerText(logIdx), &list);

    item->setData(Qt::UserRole, logIdx);

    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);

    // this must stay enabled
    if(logIdx == COL_NAME)
      item->setFlags(item->flags() & ~Qt::ItemIsEnabled);

    item->setCheckState(ui->events->header()->isSectionHidden(logIdx) ? Qt::Unchecked : Qt::Checked);
  }

  list.setSelectionMode(QAbstractItemView::SingleSelection);
  list.setDragDropMode(QAbstractItemView::DragDrop);
  list.setDefaultDropAction(Qt::MoveAction);

  buttons.setOrientation(Qt::Horizontal);
  buttons.setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
  buttons.setCenterButtons(true);

  QObject::connect(&buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  QObject::connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

  QVBoxLayout *layout = new QVBoxLayout(&dialog);
  layout->addWidget(new QLabel(tr("Select the columns to enable."), &dialog));
  layout->addWidget(&list);
  layout->addWidget(&buttons);

  int res = RDDialog::show(&dialog);

  if(res)
  {
    for(int i = 0; i < COL_COUNT; i++)
    {
      int logicalIdx = list.item(i)->data(Qt::UserRole).toInt();

      if(list.item(i)->checkState() == Qt::Unchecked)
        ui->events->header()->hideSection(logicalIdx);
      else
        ui->events->header()->showSection(logicalIdx);

      ui->events->header()->moveSection(ui->events->header()->visualIndex(logicalIdx), i);
    }
  }
}

QString EventBrowser::GetExportDrawcallString(int indent, bool firstchild,
                                              const DrawcallDescription &drawcall)
{
  QString prefix = QString(indent * 2 - (firstchild ? 1 : 0), QLatin1Char(' '));
  if(firstchild)
    prefix += QLatin1Char('\\');

  return QFormatStr("%1- %2").arg(prefix).arg(drawcall.name);
}

double EventBrowser::GetDrawTime(const DrawcallDescription &drawcall)
{
  if(!drawcall.children.empty())
  {
    double total = 0.0;

    for(const DrawcallDescription &d : drawcall.children)
    {
      double f = GetDrawTime(d);
      if(f >= 0)
        total += f;
    }

    return total;
  }

  for(const CounterResult &r : m_Times)
  {
    if(r.eventId == drawcall.eventId)
      return r.value.d;
  }

  return -1.0;
}

void EventBrowser::GetMaxNameLength(int &maxNameLength, int indent, bool firstchild,
                                    const DrawcallDescription &drawcall)
{
  QString nameString = GetExportDrawcallString(indent, firstchild, drawcall);

  maxNameLength = qMax(maxNameLength, nameString.count());

  firstchild = true;

  for(const DrawcallDescription &d : drawcall.children)
  {
    GetMaxNameLength(maxNameLength, indent + 1, firstchild, d);
    firstchild = false;
  }
}

void EventBrowser::ExportDrawcall(QTextStream &writer, int maxNameLength, int indent,
                                  bool firstchild, const DrawcallDescription &drawcall)
{
  QString eidString = drawcall.children.empty() ? QString::number(drawcall.eventId) : QString();

  QString nameString = GetExportDrawcallString(indent, firstchild, drawcall);

  QString line = QFormatStr("%1 | %2 | %3")
                     .arg(eidString, -5)
                     .arg(nameString, -maxNameLength)
                     .arg(drawcall.drawcallId, -6);

  if(!m_Times.empty())
  {
    double f = GetDrawTime(drawcall);

    if(f >= 0)
    {
      if(m_TimeUnit == TimeUnit::Milliseconds)
        f *= 1000.0;
      else if(m_TimeUnit == TimeUnit::Microseconds)
        f *= 1000000.0;
      else if(m_TimeUnit == TimeUnit::Nanoseconds)
        f *= 1000000000.0;

      line += QFormatStr(" | %1").arg(Formatter::Format(f));
    }
    else
    {
      line += lit(" |");
    }
  }

  writer << line << "\n";

  firstchild = true;

  for(const DrawcallDescription &d : drawcall.children)
  {
    ExportDrawcall(writer, maxNameLength, indent + 1, firstchild, d);
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
    col[lit("name")] = ui->events->headerText(i);
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
      Find(false);
    else
      Find(true);
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
  RDTreeWidgetItem *item = ui->events->itemAt(pos);

  QMenu contextMenu(this);

  QAction expandAll(tr("&Expand All"), this);
  QAction collapseAll(tr("&Collapse All"), this);
  QAction selectCols(tr("&Select Columns..."), this);
  QAction rgpSelect(tr("Select &RGP Event"), this);
  rgpSelect.setIcon(Icons::connect());

  contextMenu.addAction(&expandAll);
  contextMenu.addAction(&collapseAll);
  contextMenu.addAction(&selectCols);

  expandAll.setIcon(Icons::arrow_out());
  collapseAll.setIcon(Icons::arrow_in());
  selectCols.setIcon(Icons::timeline_marker());

  expandAll.setEnabled(item && item->childCount() > 0);
  collapseAll.setEnabled(item && item->childCount() > 0);

  QObject::connect(&expandAll, &QAction::triggered,
                   [this, item]() { ui->events->expandAllItems(item); });

  QObject::connect(&collapseAll, &QAction::triggered,
                   [this, item]() { ui->events->collapseAllItems(item); });

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

      RDTreeWidgetItem *found = NULL;
      FindEventNode(found, ui->events->topLevelItem(0), EID);

      if(found)
      {
        EventItemTag tag = found->tag().value<EventItemTag>();
        tag.bookmark = true;
        found->setTag(QVariant::fromValue(tag));
        RefreshIcon(found, tag);
      }

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

      RDTreeWidgetItem *found = NULL;
      FindEventNode(found, ui->events->topLevelItem(0), EID);

      if(found)
      {
        EventItemTag tag = found->tag().value<EventItemTag>();
        tag.bookmark = false;
        found->setTag(QVariant::fromValue(tag));
        RefreshIcon(found, tag);
      }
    }
  }

  ui->bookmarkStrip->setVisible(!bookmarks.isEmpty());
}

void EventBrowser::toggleBookmark(uint32_t EID)
{
  EventBookmark mark(EID);

  if(m_Ctx.GetBookmarks().contains(mark))
    m_Ctx.RemoveBookmark(EID);
  else
    m_Ctx.SetBookmark(mark);
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

bool EventBrowser::hasBookmark(RDTreeWidgetItem *node)
{
  if(node)
    return hasBookmark(node->tag().value<EventItemTag>().EID);

  return false;
}

bool EventBrowser::hasBookmark(uint32_t EID)
{
  return m_Ctx.GetBookmarks().contains(EventBookmark(EID));
}

void EventBrowser::RefreshIcon(RDTreeWidgetItem *item, EventItemTag tag)
{
  if(tag.current)
    item->setIcon(COL_NAME, Icons::flag_green());
  else if(tag.bookmark)
    item->setIcon(COL_NAME, Icons::asterisk_orange());
  else if(tag.find)
    item->setIcon(COL_NAME, Icons::find());
  else
    item->setIcon(COL_NAME, QIcon());
}

bool EventBrowser::FindEventNode(RDTreeWidgetItem *&found, RDTreeWidgetItem *parent, uint32_t eventId)
{
  // do a reverse search to find the last match (in case of 'set' markers that
  // inherit the event of the next real draw).
  for(int i = parent->childCount() - 1; i >= 0; i--)
  {
    RDTreeWidgetItem *n = parent->child(i);

    uint nEID = n->tag().value<EventItemTag>().lastEID;
    uint fEID = found ? found->tag().value<EventItemTag>().lastEID : 0;

    if(nEID >= eventId && (found == NULL || nEID <= fEID))
      found = n;

    if(nEID == eventId && n->childCount() == 0)
      return true;

    if(n->childCount() > 0)
    {
      bool exact = FindEventNode(found, n, eventId);
      if(exact)
        return true;
    }
  }

  return false;
}

void EventBrowser::ExpandNode(RDTreeWidgetItem *node)
{
  RDTreeWidgetItem *n = node;
  while(node != NULL)
  {
    ui->events->expandItem(node);
    node = node->parent();
  }

  if(n)
    ui->events->scrollToItem(n);
}

bool EventBrowser::SelectEvent(uint32_t eventId)
{
  if(!m_Ctx.IsCaptureLoaded())
    return false;

  RDTreeWidgetItem *found = NULL;
  FindEventNode(found, ui->events->topLevelItem(0), eventId);
  if(found != NULL)
  {
    ui->events->setCurrentItem(found);
    ui->events->setSelectedItem(found);

    ExpandNode(found);
    return true;
  }

  return false;
}

void EventBrowser::ClearFindIcons(RDTreeWidgetItem *parent)
{
  for(int i = 0; i < parent->childCount(); i++)
  {
    RDTreeWidgetItem *n = parent->child(i);

    EventItemTag tag = n->tag().value<EventItemTag>();
    tag.find = false;
    n->setTag(QVariant::fromValue(tag));
    RefreshIcon(n, tag);

    if(n->childCount() > 0)
      ClearFindIcons(n);
  }
}

void EventBrowser::ClearFindIcons()
{
  if(m_Ctx.IsCaptureLoaded())
    ClearFindIcons(ui->events->topLevelItem(0));
}

int EventBrowser::SetFindIcons(RDTreeWidgetItem *parent, QString filter)
{
  int results = 0;

  for(int i = 0; i < parent->childCount(); i++)
  {
    RDTreeWidgetItem *n = parent->child(i);

    if(n->text(COL_NAME).contains(filter, Qt::CaseInsensitive))
    {
      EventItemTag tag = n->tag().value<EventItemTag>();
      tag.find = true;
      n->setTag(QVariant::fromValue(tag));
      RefreshIcon(n, tag);
      results++;
    }

    if(n->childCount() > 0)
    {
      results += SetFindIcons(n, filter);
    }
  }

  return results;
}

int EventBrowser::SetFindIcons(QString filter)
{
  if(filter.isEmpty())
    return 0;

  return SetFindIcons(ui->events->topLevelItem(0), filter);
}

RDTreeWidgetItem *EventBrowser::FindNode(RDTreeWidgetItem *parent, QString filter, uint32_t after)
{
  for(int i = 0; i < parent->childCount(); i++)
  {
    RDTreeWidgetItem *n = parent->child(i);

    uint eid = n->tag().value<EventItemTag>().lastEID;

    if(eid > after && n->text(COL_NAME).contains(filter, Qt::CaseInsensitive))
      return n;

    if(n->childCount() > 0)
    {
      RDTreeWidgetItem *found = FindNode(n, filter, after);

      if(found != NULL)
        return found;
    }
  }

  return NULL;
}

int EventBrowser::FindEvent(RDTreeWidgetItem *parent, QString filter, uint32_t after, bool forward)
{
  if(parent == NULL)
    return -1;

  for(int i = forward ? 0 : parent->childCount() - 1; i >= 0 && i < parent->childCount();
      i += forward ? 1 : -1)
  {
    auto n = parent->child(i);

    uint eid = n->tag().value<EventItemTag>().lastEID;

    bool matchesAfter = (forward && eid > after) || (!forward && eid < after);

    if(matchesAfter)
    {
      QString name = n->text(COL_NAME);
      if(name.contains(filter, Qt::CaseInsensitive))
        return (int)eid;
    }

    if(n->childCount() > 0)
    {
      int found = FindEvent(n, filter, after, forward);

      if(found > 0)
        return found;
    }
  }

  return -1;
}

int EventBrowser::FindEvent(QString filter, uint32_t after, bool forward)
{
  if(!m_Ctx.IsCaptureLoaded())
    return 0;

  return FindEvent(ui->events->topLevelItem(0), filter, after, forward);
}

void EventBrowser::Find(bool forward)
{
  if(ui->findEvent->text().isEmpty())
    return;

  uint32_t curEID = m_Ctx.CurSelectedEvent();

  RDTreeWidgetItem *node = ui->events->selectedItem();
  if(node)
    curEID = node->tag().value<EventItemTag>().lastEID;

  int eid = FindEvent(ui->findEvent->text(), curEID, forward);
  if(eid >= 0)
  {
    SelectEvent((uint32_t)eid);
    ui->findEvent->setPalette(palette());
  }
  else    // if(WrapSearch)
  {
    eid = FindEvent(ui->findEvent->text(), forward ? 0 : ~0U, forward);
    if(eid >= 0)
    {
      SelectEvent((uint32_t)eid);
      ui->findEvent->setPalette(palette());
    }
    else
    {
      ui->findEvent->setPalette(m_redPalette);
    }
  }
}

void EventBrowser::UpdateDurationColumn()
{
  if(m_TimeUnit == m_Ctx.Config().EventBrowser_TimeUnit)
    return;

  m_TimeUnit = m_Ctx.Config().EventBrowser_TimeUnit;

  ui->events->setHeaderText(COL_DURATION, tr("Duration (%1)").arg(UnitSuffix(m_TimeUnit)));

  if(!m_Times.empty())
    SetDrawcallTimes(ui->events->topLevelItem(0), m_Times);
}
