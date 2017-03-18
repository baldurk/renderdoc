/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2017 Baldur Karlsson
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

#include "DebugMessageView.h"
#include <QAction>
#include <QMenu>
#include "ui_DebugMessageView.h"

static const int EIDRole = Qt::UserRole + 1;

class DebugMessageItemModel : public QAbstractItemModel
{
public:
  DebugMessageItemModel(CaptureContext &ctx, QObject *parent)
      : QAbstractItemModel(parent), m_Ctx(ctx)
  {
  }

  void refresh()
  {
    emit beginResetModel();
    emit endResetModel();
  }

  QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override
  {
    if(row < 0 || row >= rowCount())
      return QModelIndex();

    return createIndex(row, column);
  }

  QModelIndex parent(const QModelIndex &index) const override { return QModelIndex(); }
  int rowCount(const QModelIndex &parent = QModelIndex()) const override
  {
    return m_Ctx.DebugMessages.count();
  }
  int columnCount(const QModelIndex &parent = QModelIndex()) const override { return 6; }
  Qt::ItemFlags flags(const QModelIndex &index) const override
  {
    if(!index.isValid())
      return 0;

    return QAbstractItemModel::flags(index);
  }

  QVariant headerData(int section, Qt::Orientation orientation, int role) const override
  {
    if(orientation == Qt::Horizontal && role == Qt::DisplayRole)
    {
      switch(section)
      {
        case 0: return "EID";
        case 1: return "Source";
        case 2: return "Severity";
        case 3: return "Category";
        case 4: return "ID";
        case 5: return "Description";
        default: break;
      }
    }

    return QVariant();
  }

  QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override
  {
    if(index.isValid() && role == Qt::DisplayRole)
    {
      int row = index.row();
      int col = index.column();

      if(col >= 0 && col < columnCount() && row < rowCount())
      {
        const DebugMessage &msg = m_Ctx.DebugMessages[row];

        switch(col)
        {
          case 0: return msg.eventID;
          case 1: return ToQStr(msg.source);
          case 2: return ToQStr(msg.severity);
          case 3: return ToQStr(msg.category);
          case 4: return msg.messageID;
          case 5: return ToQStr(msg.description);
          default: break;
        }
      }
    }

    if(index.isValid() && role == EIDRole && index.row() >= 0 &&
       index.row() < m_Ctx.DebugMessages.count())
      return m_Ctx.DebugMessages[index.row()].eventID;

    return QVariant();
  }

private:
  CaptureContext &m_Ctx;
};

class DebugMessageFilterModel : public QSortFilterProxyModel
{
public:
  DebugMessageFilterModel(CaptureContext &ctx, QObject *parent)
      : QSortFilterProxyModel(parent), m_Ctx(ctx)
  {
  }

  typedef QPair<QPair<DebugMessageSource, DebugMessageCategory>, uint32_t> DebugMessageType;
  static DebugMessageType makeType(const DebugMessage &msg)
  {
    return qMakePair(qMakePair(msg.source, msg.category), msg.messageID);
  }

  QList<DebugMessageSource> m_HiddenSources;
  QList<DebugMessageCategory> m_HiddenCategories;
  QList<DebugMessageSeverity> m_HiddenSeverities;
  QList<DebugMessageType> m_HiddenTypes;

  bool showHidden = false;

  void refresh() { invalidateFilter(); }
  QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override
  {
    if(role == Qt::FontRole && !isVisibleRow(mapToSource(index).row()))
    {
      QFont font;
      font.setItalic(true);
      return font;
    }

    return QSortFilterProxyModel::data(index, role);
  }

protected:
  bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override
  {
    if(showHidden)
      return true;

    return isVisibleRow(sourceRow);
  }

  bool isVisibleRow(int sourceRow) const
  {
    const DebugMessage &msg = m_Ctx.DebugMessages[sourceRow];

    if(m_HiddenSources.contains(msg.source))
      return false;

    if(m_HiddenCategories.contains(msg.category))
      return false;

    if(m_HiddenSeverities.contains(msg.severity))
      return false;

    if(m_HiddenTypes.contains(makeType(msg)))
      return false;

    return true;
  }

  bool lessThan(const QModelIndex &left, const QModelIndex &right) const override
  {
    const DebugMessage &leftMsg = m_Ctx.DebugMessages[left.row()];
    const DebugMessage &rightMsg = m_Ctx.DebugMessages[right.row()];

    if(leftMsg.eventID < rightMsg.eventID)
      return true;

    if(leftMsg.source < rightMsg.source)
      return true;

    if(leftMsg.severity < rightMsg.severity)
      return true;

    if(leftMsg.category < rightMsg.category)
      return true;

    if(leftMsg.messageID < rightMsg.messageID)
      return true;

    return strcmp(leftMsg.description.c_str(), rightMsg.description.c_str()) < 0;
  }

private:
  CaptureContext &m_Ctx;
};

DebugMessageView::DebugMessageView(CaptureContext &ctx, QWidget *parent)
    : QFrame(parent), ui(new Ui::DebugMessageView), m_Ctx(ctx)
{
  ui->setupUi(this);

  m_ItemModel = new DebugMessageItemModel(m_Ctx, this);
  m_FilterModel = new DebugMessageFilterModel(m_Ctx, this);

  m_FilterModel->setSourceModel(m_ItemModel);
  ui->messages->setModel(m_FilterModel);

  ui->messages->setContextMenuPolicy(Qt::CustomContextMenu);
  QObject::connect(ui->messages, &QWidget::customContextMenuRequested, this,
                   &DebugMessageView::messages_contextMenu);

  m_Ctx.AddLogViewer(this);

  m_ContextMenu = new QMenu(this);

  m_ShowHidden = new QAction(tr("Show hidden rows"), this);
  m_ToggleSource = new QAction("", this);
  m_ToggleSeverity = new QAction("", this);
  m_ToggleCategory = new QAction("", this);
  m_ToggleMessageType = new QAction("", this);

  m_ShowHidden->setCheckable(true);

  m_ContextMenu->addAction(m_ShowHidden);
  m_ContextMenu->addSeparator();
  m_ContextMenu->addAction(m_ToggleSource);
  m_ContextMenu->addAction(m_ToggleSeverity);
  m_ContextMenu->addAction(m_ToggleCategory);
  m_ContextMenu->addAction(m_ToggleMessageType);

  QObject::connect(m_ShowHidden, &QAction::triggered, this, &DebugMessageView::messages_toggled);
  QObject::connect(m_ToggleSource, &QAction::triggered, this, &DebugMessageView::messages_toggled);
  QObject::connect(m_ToggleSeverity, &QAction::triggered, this, &DebugMessageView::messages_toggled);
  QObject::connect(m_ToggleCategory, &QAction::triggered, this, &DebugMessageView::messages_toggled);
  QObject::connect(m_ToggleMessageType, &QAction::triggered, this,
                   &DebugMessageView::messages_toggled);

  RefreshMessageList();
}

DebugMessageView::~DebugMessageView()
{
  m_Ctx.windowClosed(this);

  m_Ctx.RemoveLogViewer(this);
  delete ui;
}

void DebugMessageView::OnLogfileClosed()
{
  m_FilterModel->showHidden = false;
  RefreshMessageList();
}

void DebugMessageView::OnLogfileLoaded()
{
  m_FilterModel->showHidden = false;
  RefreshMessageList();
}

void DebugMessageView::RefreshMessageList()
{
  m_ItemModel->refresh();

  ui->messages->resizeColumnsToContents();

  if(m_Ctx.UnreadMessageCount > 0)
    setWindowTitle(tr("(%1) Errors and Warnings").arg(m_Ctx.UnreadMessageCount));
  else
    setWindowTitle(tr("Errors and Warnings"));
}

void DebugMessageView::on_messages_doubleClicked(const QModelIndex &index)
{
  QVariant var = m_ItemModel->data(index, EIDRole);

  if(var.isValid())
  {
    uint32_t eid = var.toUInt();
    m_Ctx.SetEventID({}, eid, eid);
  }
}

void DebugMessageView::messages_toggled()
{
  QAction *action = qobject_cast<QAction *>(QObject::sender());

  if(action == m_ShowHidden)
  {
    m_FilterModel->showHidden = !m_FilterModel->showHidden;
    m_ShowHidden->setChecked(m_FilterModel->showHidden);
  }
  else if(action == m_ToggleSource)
  {
    if(m_FilterModel->m_HiddenSources.contains(m_ContextMessage.source))
      m_FilterModel->m_HiddenSources.removeOne(m_ContextMessage.source);
    else
      m_FilterModel->m_HiddenSources.push_back(m_ContextMessage.source);
  }
  else if(action == m_ToggleSeverity)
  {
    if(m_FilterModel->m_HiddenSeverities.contains(m_ContextMessage.severity))
      m_FilterModel->m_HiddenSeverities.removeOne(m_ContextMessage.severity);
    else
      m_FilterModel->m_HiddenSeverities.push_back(m_ContextMessage.severity);
  }
  else if(action == m_ToggleSeverity)
  {
    if(m_FilterModel->m_HiddenCategories.contains(m_ContextMessage.category))
      m_FilterModel->m_HiddenCategories.removeOne(m_ContextMessage.category);
    else
      m_FilterModel->m_HiddenCategories.push_back(m_ContextMessage.category);
  }
  else if(action == m_ToggleSeverity)
  {
    auto type = DebugMessageFilterModel::makeType(m_ContextMessage);
    if(m_FilterModel->m_HiddenTypes.contains(type))
      m_FilterModel->m_HiddenTypes.removeOne(type);
    else
      m_FilterModel->m_HiddenTypes.push_back(type);
  }

  m_FilterModel->refresh();
}

void DebugMessageView::messages_contextMenu(const QPoint &pos)
{
  if(!m_Ctx.LogLoaded())
    return;

  QModelIndex index = ui->messages->indexAt(pos);

  if(index.isValid())
  {
    index = m_FilterModel->mapToSource(index);

    const DebugMessage &msg = m_Ctx.DebugMessages[index.row()];

    QString hide = tr("Hide");
    QString show = tr("Show");

    bool hidden = m_FilterModel->m_HiddenSources.contains(msg.source);
    m_ToggleSource->setText(tr("%1 Source: %2").arg(hidden ? show : hide).arg(ToQStr(msg.source)));

    hidden = m_FilterModel->m_HiddenSeverities.contains(msg.severity);
    m_ToggleSeverity->setText(
        tr("%1 Severity: %2").arg(hidden ? show : hide).arg(ToQStr(msg.severity)));

    hidden = m_FilterModel->m_HiddenCategories.contains(msg.category);
    m_ToggleCategory->setText(
        tr("%1 Category: %2").arg(hidden ? show : hide).arg(ToQStr(msg.category)));

    hidden = m_FilterModel->m_HiddenTypes.contains(DebugMessageFilterModel::makeType(msg));
    m_ToggleMessageType->setText(tr("%1 Message Type").arg(hidden ? show : hide));

    m_ContextMessage = msg;

    RDDialog::show(m_ContextMenu, ui->messages->viewport()->mapToGlobal(pos));
  }
}

void DebugMessageView::paintEvent(QPaintEvent *e)
{
  if(m_Ctx.UnreadMessageCount > 0)
  {
    m_Ctx.UnreadMessageCount = 0;
    RefreshMessageList();
  }

  QFrame::paintEvent(e);
}
