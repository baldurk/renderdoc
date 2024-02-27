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

#include "LogView.h"
#include <QAbstractProxyModel>
#include <QClipboard>
#include <QDesktopServices>
#include <QFontDatabase>
#include <QKeyEvent>
#include <QStandardItemModel>
#include "Code/QRDUtils.h"
#include "Widgets/Extended/RDHeaderView.h"
#include "ui_LogView.h"

enum Columns
{
  Column_Source,
  Column_PID,
  Column_Timestamp,
  Column_Location,
  Column_Type,
  Column_Message,
  Column_Count,
};

class LogItemModel : public QAbstractItemModel
{
public:
  LogItemModel(LogView *view) : QAbstractItemModel(view), m_Viewer(view) {}
  void addRows(int numLines)
  {
    int count = rowCount();
    emit beginInsertRows(QModelIndex(), count - numLines, count - 1);
    emit endInsertRows();
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
    if(parent == QModelIndex())
      return m_Viewer->m_Messages.count();
    return 0;
  }
  int columnCount(const QModelIndex &parent = QModelIndex()) const override { return Column_Count; }
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
        case Column_Source: return lit("Source");
        case Column_PID: return lit("PID");
        case Column_Timestamp: return lit("Timestamp");
        case Column_Location: return lit("Location");
        case Column_Type: return lit("Type");
        case Column_Message: return lit("Message");
        default: break;
      }
    }

    return QVariant();
  }

  QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override
  {
    if(index.isValid())
    {
      int row = index.row();
      int col = index.column();

      if(col >= 0 && col < columnCount() && row < rowCount())
      {
        const LogMessage &msg = m_Viewer->m_Messages[row];

        if(role == Qt::DisplayRole)
        {
          switch(col)
          {
            case Column_Source: return msg.Source;
            case Column_PID: return QString::number(msg.PID);
            case Column_Timestamp: return msg.Timestamp.toString(lit("HH:mm:ss"));
            case Column_Location: return msg.Location;
            case Column_Type: return ToQStr(msg.Type);
            case Column_Message:
            {
              QVariant desc = msg.Message;
              RichResourceTextInitialise(desc, &m_Viewer->m_Ctx);
              return desc;
            }
            default: break;
          }
        }
        else if(msg.Type == LogType::Error)
        {
          if(role == Qt::BackgroundRole)
            return QBrush(QColor(255, 70, 70));
          if(role == Qt::ForegroundRole)
            return QBrush(QColor(0, 0, 0));
        }
      }
    }

    return QVariant();
  }

private:
  LogView *m_Viewer;
};

class LogFilterModel : public QAbstractProxyModel
{
public:
  LogFilterModel(LogView *view) : QAbstractProxyModel(view), m_Viewer(view) {}
  bool m_UseRegexp = false;
  bool m_IncludeTextMatches = true;
  QString m_FilterText;
  QRegularExpression m_FilterRegexp;
  QSet<uint32_t> m_HiddenPIDs;
  QSet<uint32_t> m_HiddenTypes;

  void refresh()
  {
    emit beginResetModel();
    m_VisibleRows.clear();
    for(int i = 0; i < sourceModel()->rowCount(); i++)
      if(isVisibleRow(i))
        m_VisibleRows.push_back(i);
    emit endResetModel();
  }

  void addRows(int addedRows)
  {
    int numRows = sourceModel()->rowCount();
    emit beginInsertRows(QModelIndex(), numRows - addedRows, numRows - 1);

    m_VisibleRows.reserve(m_VisibleRows.count() + addedRows);

    for(int i = 0; i < addedRows; i++)
      if(isVisibleRow(numRows - addedRows + i))
        m_VisibleRows.push_back(numRows - addedRows + i);

    emit endInsertRows();
  }

  virtual QModelIndex mapFromSource(const QModelIndex &sourceIndex) const override
  {
    auto it = std::lower_bound(m_VisibleRows.begin(), m_VisibleRows.end(), sourceIndex.row());

    int row = -1;

    if(it != m_VisibleRows.end() && *it == sourceIndex.row())
      row = it - m_VisibleRows.begin();

    return createIndex(row, sourceIndex.column(), sourceIndex.internalId());
  }

  virtual QModelIndex mapToSource(const QModelIndex &proxyIndex) const override
  {
    int row = -1;
    if(proxyIndex.row() >= 0 && proxyIndex.row() < m_VisibleRows.count())
      row = m_VisibleRows[proxyIndex.row()];

    return sourceModel()->index(row, proxyIndex.column());
  }

  virtual int rowCount(const QModelIndex &parent = QModelIndex()) const override
  {
    return m_VisibleRows.count();
  }
  virtual int columnCount(const QModelIndex &parent = QModelIndex()) const override
  {
    return sourceModel()->columnCount(parent);
  }
  QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override
  {
    return createIndex(row, column);
  }
  QModelIndex parent(const QModelIndex &index) const override
  {
    return sourceModel()->parent(index);
  }
  QVariant headerData(int section, Qt::Orientation orientation, int role) const override
  {
    if(orientation == Qt::Horizontal)
    {
      return sourceModel()->headerData(section, orientation, role);
    }
    else if(section >= 0 && section < m_VisibleRows.count())
    {
      return sourceModel()->headerData(m_VisibleRows[section], orientation, role);
    }
    return QVariant();
  }
  void itemChanged(const QModelIndex &idx, const QVector<int> &roles)
  {
    QModelIndex topLeft = index(idx.row(), 0);
    QModelIndex bottomRight = index(idx.row(), columnCount() - 1);
    emit dataChanged(topLeft, bottomRight, roles);
  }

protected:
  QVector<int> m_VisibleRows;

  bool isVisibleRow(int sourceRow) const
  {
    const LogMessage &msg = m_Viewer->m_Messages[sourceRow];

    if(m_HiddenPIDs.contains(msg.PID))
      return false;

    if(m_HiddenTypes.contains((uint32_t)msg.Type))
      return false;

    if(m_UseRegexp)
    {
      if(m_FilterRegexp.isValid())
      {
        return (m_FilterRegexp.match(msg.Message).hasMatch()) == m_IncludeTextMatches;
      }
    }
    else
    {
      if(!m_FilterText.isEmpty())
      {
        return (msg.Message.contains(m_FilterText, Qt::CaseInsensitive)) == m_IncludeTextMatches;
      }
    }

    return true;
  }

private:
  LogView *m_Viewer;
};

static QList<QString> logTypeStrings;

LogView::LogView(ICaptureContext &ctx, QWidget *parent)
    : QFrame(parent), ui(new Ui::LogView), m_Ctx(ctx)
{
  ui->setupUi(this);

  m_ItemModel = new LogItemModel(this);
  m_FilterModel = new LogFilterModel(this);

  m_FilterModel->setSourceModel(m_ItemModel);
  ui->messages->setModel(m_FilterModel);

  ui->messages->viewport()->installEventFilter(this);

  m_delegate = new RichTextViewDelegate(ui->messages);
  ui->messages->setItemDelegate(m_delegate);

  ui->messages->setMouseTracking(true);

  ui->messages->setFont(Formatter::FixedFont());

  m_TypeModel = new QStandardItemModel(0, 1, this);

  m_TypeModel->appendRow(new QStandardItem(tr("Log Type")));

  for(LogType type : values<LogType>())
  {
    logTypeStrings.push_back(ToQStr(type));

    QStandardItem *item = new QStandardItem(ToQStr(type));
    item->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
    item->setData(Qt::Checked, Qt::CheckStateRole);
    m_TypeModel->appendRow(item);
  }

  ui->typeFilter->setModel(m_TypeModel);

  m_PIDModel = new QStandardItemModel(0, 1, this);

  m_PIDModel->appendRow(new QStandardItem(tr("PID")));

  ui->pidFilter->setModel(m_PIDModel);

  ui->messages->header()->setSectionResizeMode(QHeaderView::Fixed);

  for(int c = 0; c < m_ItemModel->columnCount(); c++)
    ui->messages->resizeColumnToContents(c);

  messages_refresh();

  QObject::connect(m_TypeModel, &QStandardItemModel::itemChanged, this, &LogView::typeFilter_changed);
  QObject::connect(m_PIDModel, &QStandardItemModel::itemChanged, this, &LogView::pidFilter_changed);

  QObject::connect(ui->messages, &RDTreeView::keyPress, this, &LogView::messages_keyPress);

  QObject::connect(&m_RefreshTimer, &QTimer::timeout, this, &LogView::messages_refresh);
  m_RefreshTimer.setSingleShot(false);
  m_RefreshTimer.setInterval(125);
  m_RefreshTimer.start();
}

LogView::~LogView()
{
  m_Ctx.BuiltinWindowClosed(this);

  m_Messages.clear();

  delete ui;
}

void LogView::on_openExternal_clicked()
{
  QString logPath = QString::fromUtf8(RENDERDOC_GetLogFile());
  if(QFileInfo::exists(logPath))
    QDesktopServices::openUrl(QUrl::fromLocalFile(logPath));
}

void LogView::on_save_clicked()
{
  QString filename =
      RDDialog::getSaveFileName(this, tr("Export log to disk"), QString(),
                                tr("Log Files (*.log);;Text files (*.txt);;All files (*)"));

  if(filename.isEmpty())
    return;

  QFile *f = new QFile(filename);

  if(!f->open(QIODevice::WriteOnly | QFile::Truncate))
  {
    delete f;
    RDDialog::critical(this, tr("Error exporting log"),
                       tr("Couldn't open file '%1' for writing").arg(filename));
    return;
  }

  rdcstr contents;
  RENDERDOC_GetLogFileContents(0, contents);

  f->write(QByteArray(contents.c_str(), contents.count()));

  delete f;
}

void LogView::on_textFilter_textChanged(const QString &text)
{
  m_FilterModel->m_FilterText = text;
  m_FilterModel->m_FilterRegexp = QRegularExpression(text);
  m_FilterModel->m_FilterRegexp.setPatternOptions(QRegularExpression::CaseInsensitiveOption);
  m_FilterModel->refresh();
}

void LogView::on_textFilterMeaning_currentIndexChanged(int index)
{
  // 0 is Include, 1 is Exclude
  m_FilterModel->m_IncludeTextMatches = (index == 0);
  m_FilterModel->refresh();
}

void LogView::on_regexpFilter_toggled()
{
  m_FilterModel->m_UseRegexp = ui->regexpFilter->isChecked();
  m_FilterModel->refresh();
}

void LogView::messages_keyPress(QKeyEvent *event)
{
  if(event->matches(QKeySequence::Copy))
  {
    QModelIndexList items = ui->messages->selectionModel()->selectedIndexes();

    QList<int> rows;

    for(QModelIndex idx : items)
    {
      if(!rows.contains(idx.row()))
        rows.push_back(idx.row());
    }

    std::sort(rows.begin(), rows.end());

    int columns = m_ItemModel->columnCount();

    QString clipboardText;
    for(int r : rows)
    {
      const LogMessage &msg = m_Messages[r];

      clipboardText += QFormatStr("%1 PID %2: [%3] %4 - %5 - %6\n")
                           .arg(msg.Source, -8)
                           .arg(msg.PID, 6)
                           .arg(msg.Timestamp.toString(lit("HH:mm:ss")))
                           .arg(msg.Location, 26)
                           .arg(ToQStr(msg.Type), -7)
                           .arg(msg.Message);
    }

    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(clipboardText.trimmed());
  }
}

void LogView::typeFilter_changed(QStandardItem *item)
{
  uint32_t type = m_TypeModel->indexFromItem(item).row() - 1;

  if(item->checkState() == Qt::Checked)
    m_FilterModel->m_HiddenTypes.remove(type);
  else
    m_FilterModel->m_HiddenTypes.insert(type);

  m_FilterModel->refresh();

  ui->typeFilter->setCurrentIndex(0);
}

bool LogView::eventFilter(QObject *watched, QEvent *event)
{
  if(watched == ui->messages->viewport() && event->type() == QEvent::MouseMove)
  {
    bool ret = QObject::eventFilter(watched, event);

    if(m_delegate->linkHover((QMouseEvent *)event, font(), ui->messages->currentHoverIndex()))
    {
      m_FilterModel->itemChanged(ui->messages->currentHoverIndex(), {Qt::DecorationRole});
      ui->messages->setCursor(QCursor(Qt::PointingHandCursor));
    }
    else
    {
      ui->messages->unsetCursor();
    }

    return ret;
  }

  return QObject::eventFilter(watched, event);
}

void LogView::pidFilter_changed(QStandardItem *item)
{
  uint32_t PID = item->text().toUInt();

  if(item->checkState() == Qt::Checked)
    m_FilterModel->m_HiddenPIDs.remove(PID);
  else
    m_FilterModel->m_HiddenPIDs.insert(PID);

  m_FilterModel->refresh();

  ui->pidFilter->setCurrentIndex(0);
}

void LogView::messages_refresh()
{
  rdcstr contents;
  RENDERDOC_GetLogFileContents(prevOffset, contents);

  if(contents.empty())
    return;

  // look at all new lines since the last one we saw
  QStringList lines = QString(contents).split(QRegularExpression(lit("[\r\n]")));
  prevOffset += contents.size();

  QString r =
      lit("^"                                                // start of the line
          "([A-Z][A-Z][A-Z][A-Z]) "                          // project
          "([0-9]+): "                                       // PID
          "\\[([0-9][0-9]):([0-9][0-9]):([0-9][0-9])\\] "    // timestamp
          "\\s*([^(]+)\\(\\s*([0-9]+)\\) - "                 // filename.ext( line)
          "([A-Za-z]+)\\s+- "                                // type
          "(.*)");

  QRegularExpression logRegex(r);

  int prevCount = m_Messages.count();

  for(const QString &line : lines)
  {
    QRegularExpressionMatch match = logRegex.match(line);

    if(match.hasMatch())
    {
      LogMessage msg;
      msg.Source = match.captured(1);

      if(msg.Source == lit("ADRD"))
        msg.Source = tr("Android");
      else if(msg.Source == lit("QTRD"))
        msg.Source = tr("UI");
      else if(msg.Source == lit("RDOC"))
        msg.Source = tr("Core");

      msg.PID = match.captured(2).toUInt();
      msg.Timestamp =
          QTime(match.captured(3).toUInt(), match.captured(4).toUInt(), match.captured(5).toUInt());
      msg.Location = QFormatStr("%1(%2)").arg(match.captured(6)).arg(match.captured(7));
      msg.Type = (LogType)logTypeStrings.indexOf(match.captured(8));
      msg.Message = match.captured(9).trimmed();

      m_Messages.push_back(msg);

      if(!m_PIDs.contains(msg.PID))
      {
        m_PIDs.append(msg.PID);
        QStandardItem *item = new QStandardItem(QString::number(msg.PID));
        item->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
        item->setData(Qt::Checked, Qt::CheckStateRole);
        m_PIDModel->appendRow(item);
      }
    }
  }

  if(!lines.isEmpty())
  {
    m_ItemModel->addRows(m_Messages.count() - prevCount);
    m_FilterModel->addRows(m_Messages.count() - prevCount);
  }

  // go through each new message and size up columns to fit
  for(int i = prevCount; i < m_Messages.count(); i++)
  {
    for(int c = 0; c < ui->messages->model()->columnCount(); c++)
    {
      QSize s = ui->messages->sizeHintForIndex(ui->messages->model()->index(i, c));

      int w = ui->messages->header()->sectionSize(c);

      if(s.width() > w)
        ui->messages->header()->resizeSection(c, s.width());
    }
  }

  if(ui->followNew->isChecked())
    ui->messages->scrollToBottom();
}
