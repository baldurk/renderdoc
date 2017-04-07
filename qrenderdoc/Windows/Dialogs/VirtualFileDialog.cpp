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

#include "VirtualFileDialog.h"
#include <QDateTime>
#include <QKeyEvent>
#include <QPainter>
#include <QPushButton>
#include <QRegExp>
#include <QSortFilterProxyModel>
#include "Code/CaptureContext.h"
#include "Code/RenderManager.h"
#include "Code/Resources.h"
#include "ui_VirtualFileDialog.h"

class RemoteFileModel : public QAbstractItemModel
{
public:
  enum Roles
  {
    FileIsDirRole = Qt::UserRole,
    FileIsHiddenRole,
    FileIsExecutableRole,
    FileIsRootRole,
    FileIsAccessDeniedRole,
    FilePathRole,
    FileNameRole,
  };

  RemoteFileModel(RenderManager &r, QObject *parent = NULL)
      : Renderer(r), QAbstractItemModel(parent)
  {
    makeIconStates(fileIcon, Pixmaps::page_white_database());
    makeIconStates(exeIcon, Pixmaps::page_white_code());
    makeIconStates(dirIcon, Pixmaps::folder_page());

    Renderer.GetHomeFolder(true, [this](const char *path, const rdctype::array<DirectoryFile> &files) {
      QString homeDir = QString::fromUtf8(path);

      if(QChar(path[0]).isLetter() && path[1] == ':')
      {
        NTPaths = true;

        // NT paths
        Renderer.ListFolder(
            "/", true, [this, homeDir](const char *path, const rdctype::array<DirectoryFile> &files) {
              for(int i = 0; i < files.count; i++)
              {
                FSNode *node = new FSNode();
                node->parent = NULL;
                node->parentIndex = i;
                node->file = files[i];
                roots.push_back(node);

                home = indexForPath(homeDir);
              }
            });
      }
      else
      {
        NTPaths = false;

        FSNode *node = new FSNode();
        node->parent = NULL;
        node->parentIndex = 0;
        node->file.filename = "/";
        node->file.flags = eFileProp_Directory;
        roots.push_back(node);

        home = indexForPath(homeDir);
      }
    });

    for(FSNode *node : roots)
      populate(node);
  }

  ~RemoteFileModel()
  {
    for(FSNode *n : roots)
      delete n;
  }

  QModelIndex homeFolder() const { return home; }
  QModelIndex indexForPath(const QString &path) const
  {
    QModelIndex ret = index(0, 0);

    QString normPath = path;

    // locate the drive
    if(NTPaths)
    {
      // normalise to unix directory separators
      normPath.replace(QChar('\\'), QChar('/'));

      for(int i = 0; i < roots.count(); i++)
      {
        if(normPath[0] == roots[i]->file.filename.front())
        {
          ret = index(i, 0);
          normPath.remove(0, 2);
          break;
        }
      }
    }

    // normPath is now of the form /subdir1/subdir2/subdir3/...
    // with ret pointing to the root directory (trivial on unix)

    while(!normPath.isEmpty())
    {
      if(normPath[0] != '/')
      {
        qCritical() << "Malformed/unexpected path" << path;
        return QModelIndex();
      }

      // ignore multiple /s adjacent
      int start = 1;
      while(start < normPath.count() && normPath[start] == '/')
        start++;

      // if we've hit trailing slashes just stop
      if(start >= normPath.count())
        break;

      int nextDirEnd = normPath.indexOf(QChar('/'), start);

      if(nextDirEnd == -1)
        nextDirEnd = normPath.count();

      QString nextDir = normPath.mid(start, nextDirEnd - start);
      normPath.remove(0, nextDirEnd);

      FSNode *node = getNode(ret);
      populate(node);

      for(int i = 0; i < node->children.count(); i++)
      {
        if(ToQStr(node->children[i]->file.filename)
               .compare(nextDir, NTPaths ? Qt::CaseInsensitive : Qt::CaseSensitive) == 0)
        {
          ret = index(i, 0, ret);
          break;
        }
      }

      // if we didn't move to a child, stop searching
      if(node == getNode(ret))
      {
        qCritical() << "Couldn't find child" << nextDir << "at" << ToQStr(node->file.filename)
                    << "from" << path;
        return QModelIndex();
      }
    }

    return ret;
  }

  QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override
  {
    if(row < 0 || column < 0 || row >= rowCount(parent) || column >= columnCount(parent))
      return QModelIndex();

    FSNode *node = getNode(parent);

    if(node == NULL)
      return createIndex(row, column, roots[row]);

    return createIndex(row, column, node->children[row]);
  }

  QModelIndex parent(const QModelIndex &index) const override
  {
    if(!index.isValid())
      return QModelIndex();

    FSNode *node = getNode(index);

    // root nodes have no index
    if(node->parent == NULL)
      return QModelIndex();

    node = node->parent;

    return createIndex(node->parentIndex, 0, node);
  }

  int rowCount(const QModelIndex &parent = QModelIndex()) const override
  {
    if(!parent.isValid())
      return roots.count();

    return getNode(parent)->children.count();
  }

  int columnCount(const QModelIndex &parent = QModelIndex()) const override
  {
    // Name | Date Modified | Type | Size
    return 4;
  }

  Qt::ItemFlags flags(const QModelIndex &index) const override
  {
    Qt::ItemFlags ret = QAbstractItemModel::flags(index);

    // disable drag/drop
    ret &= ~(Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled);

    // disable editing, we don't support remote renaming
    ret &= ~Qt::ItemIsEditable;

    if(!index.isValid())
      return ret;

    // if it's not a dir, there are no children
    if(getNode(index)->file.flags & eFileProp_Directory)
      ret &= ~Qt::ItemNeverHasChildren;

    // if we can't populate it, set it as disabled
    if(getNode(index)->file.flags & eFileProp_ErrorAccessDenied)
      ret &= ~Qt::ItemIsEnabled;

    return ret;
  }

  QVariant headerData(int section, Qt::Orientation orientation, int role) const override
  {
    if(orientation == Qt::Horizontal && role == Qt::DisplayRole)
    {
      switch(section)
      {
        case 0: return tr("Name");
        case 1: return tr("Size");
        case 2: return tr("Type");
        case 3: return tr("Date Modified");
        default: break;
      }
    }

    return QVariant();
  }

  bool canFetchMore(const QModelIndex &parent) const override
  {
    FSNode *node = getNode(parent);
    if(!node)
      return true;

    if(!node->populated)
      return true;

    for(FSNode *c : node->children)
      if(!c->populated)
        return true;

    return false;
  }

  void fetchMore(const QModelIndex &parent) override
  {
    FSNode *node = getNode(parent);
    if(!node)
      return;

    populate(node);
    for(FSNode *c : node->children)
      populate(c);
  }

  QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override
  {
    if(index.isValid())
    {
      FSNode *node = getNode(index);

      switch(role)
      {
        case Qt::DisplayRole:
        {
          switch(index.column())
          {
            case 0: { return ToQStr(node->file.filename);
            }
            case 1:
            {
              if(node->file.flags & eFileProp_Directory)
                return QVariant();
              return qulonglong(node->file.size);
            }
            case 2:
            {
              if(node->file.flags & eFileProp_Directory)
                return tr("Directory");
              else if(node->file.flags & eFileProp_Executable)
                return tr("Executable file");
              else
                return tr("File");
            }
            case 3:
            {
              if(node->file.lastmod == 0)
                return QVariant();
              return QDateTime::fromTime_t(node->file.lastmod);
            }
            default: break;
          }
          break;
        }
        case Qt::DecorationRole:
          if(index.column() == 0)
          {
            int hideIndex = (node->file.flags & eFileProp_Hidden) ? 1 : 0;

            if(node->file.flags & eFileProp_Directory)
              return dirIcon[hideIndex];
            else if(node->file.flags & eFileProp_Executable)
              return exeIcon[hideIndex];
            else
              return fileIcon[hideIndex];
          }
        case Qt::TextAlignmentRole:
          if(index.column() == 1)
            return Qt::AlignRight;
          break;
        case FileIsDirRole: return bool(node->file.flags & eFileProp_Directory);
        case FileIsHiddenRole: return bool(node->file.flags & eFileProp_Hidden);
        case FileIsExecutableRole: return bool(node->file.flags & eFileProp_Executable);
        case FileIsRootRole: return roots.contains(node);
        case FileIsAccessDeniedRole: return bool(node->file.flags & eFileProp_ErrorAccessDenied);
        case FilePathRole: return makePath(node);
        case FileNameRole: return ToQStr(node->file.filename);
        default: break;
      }
    }

    return QVariant();
  }

private:
  RenderManager &Renderer;

  QIcon dirIcon[2];
  QIcon exeIcon[2];
  QIcon fileIcon[2];

  void makeIconStates(QIcon *icon, const QPixmap &normalPixmap)
  {
    QPixmap disabledPixmap(normalPixmap.size());
    disabledPixmap.fill(Qt::transparent);
    QPainter p(&disabledPixmap);

    p.setBackgroundMode(Qt::TransparentMode);
    p.setBackground(QBrush(Qt::transparent));
    p.eraseRect(normalPixmap.rect());

    p.setOpacity(0.5);
    p.drawPixmap(0, 0, normalPixmap);

    p.end();

    icon[0].addPixmap(normalPixmap);
    icon[1].addPixmap(disabledPixmap);
  }

  struct FSNode
  {
    FSNode() { memset(&file, 0, sizeof(file)); }
    ~FSNode()
    {
      for(FSNode *n : children)
        delete n;
    }

    FSNode *parent = NULL;
    int parentIndex = 0;

    bool populated = false;

    DirectoryFile file;

    QList<FSNode *> children;
  };

  bool NTPaths = false;
  QList<FSNode *> roots;
  QModelIndex home;

  FSNode *getNode(const QModelIndex &idx) const
  {
    FSNode *node = (FSNode *)idx.internalPointer();
    return node;
  }

  QString makePath(FSNode *node) const
  {
    QChar sep = NTPaths ? '\\' : '/';
    QString ret = ToQStr(node->file.filename);
    FSNode *parent = node->parent;
    // iterate through subdirs but stop before a root
    while(parent && parent->parent)
    {
      ret = ToQStr(parent->file.filename) + sep + ret;
      parent = parent->parent;
    }

    if(parent)
    {
      // parent is now a root
      ret = ToQStr(parent->file.filename) + ret;
    }
    ret.replace('/', sep);
    return ret;
  }

  void populate(FSNode *node) const
  {
    if(!node || node->populated)
      return;

    node->populated = true;

    // nothing to do for non-directories
    if(!(node->file.flags & eFileProp_Directory))
      return;

    Renderer.ListFolder(
        makePath(node), true,
        [this, node](const char *path, const rdctype::array<DirectoryFile> &files) {

          if(files.count == 1 && files[0].flags & eFileProp_ErrorAccessDenied)
          {
            node->file.flags |= eFileProp_ErrorAccessDenied;
            return;
          }

          QVector<DirectoryFile> sortedFiles;
          sortedFiles.reserve(files.count);
          for(const DirectoryFile &f : files)
            sortedFiles.push_back(f);

          qSort(sortedFiles.begin(), sortedFiles.end(),
                [](const DirectoryFile &a, const DirectoryFile &b) {
                  // sort greater than so that files with the flag are sorted before those without
                  if((a.flags & eFileProp_Directory) != (b.flags & eFileProp_Directory))
                    return (a.flags & eFileProp_Directory) > (b.flags & eFileProp_Directory);

                  return strcmp(a.filename.c_str(), b.filename.c_str()) < 0;
                });

          for(int i = 0; i < sortedFiles.count(); i++)
          {
            FSNode *child = new FSNode();
            child->parent = node;
            child->parentIndex = i;
            child->file = sortedFiles[i];
            node->children.push_back(child);
          }
        });
  }
};

class RemoteFileProxy : public QSortFilterProxyModel
{
public:
  RemoteFileProxy(QObject *parent = NULL) : QSortFilterProxyModel(parent) {}
  int columnCount(const QModelIndex &parent = QModelIndex()) const override
  {
    return qMin(maxColCount, sourceModel()->columnCount(parent));
  }

  void refresh() { QSortFilterProxyModel::filterChanged(); }
  int maxColCount = INT_MAX;
  bool showFiles = true;
  bool showDirs = true;
  bool showHidden = true;
  bool showNonExecutables = true;

protected:
  bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override
  {
    bool isDir = sourceModel()
                     ->data(sourceModel()->index(source_row, 0, source_parent),
                            RemoteFileModel::FileIsDirRole)
                     .toBool();

    if(!showDirs && isDir)
      return false;

    if(!showFiles && !isDir)
      return false;

    bool isHidden = sourceModel()
                        ->data(sourceModel()->index(source_row, 0, source_parent),
                               RemoteFileModel::FileIsHiddenRole)
                        .toBool();

    if(!showHidden && isHidden)
      return false;

    // if we're showing dirs, never apply further filters like filename matching
    if(isDir)
      return true;

    bool isExe = sourceModel()
                     ->data(sourceModel()->index(source_row, 0, source_parent),
                            RemoteFileModel::FileIsExecutableRole)
                     .toBool();

    if(!showNonExecutables && !isExe)
      return false;

    return QSortFilterProxyModel::filterAcceptsRow(source_row, source_parent);
  }

  virtual bool lessThan(const QModelIndex &source_left, const QModelIndex &source_right) const override
  {
    // always sort dirs first
    bool isLeftDir = sourceModel()->data(source_left, RemoteFileModel::FileIsDirRole).toBool();
    bool isRightDir = sourceModel()->data(source_right, RemoteFileModel::FileIsDirRole).toBool();

    if(isLeftDir && !isRightDir)
      return true;
    if(!isLeftDir && isRightDir)
      return false;

    return QSortFilterProxyModel::lessThan(source_left, source_right);
  }
};

VirtualFileDialog::VirtualFileDialog(CaptureContext &ctx, QWidget *parent)
    : QDialog(parent), ui(new Ui::VirtualFileDialog)
{
  ui->setupUi(this);

  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

  m_Model = new RemoteFileModel(ctx.Renderer(), this);

  m_DirProxy = new RemoteFileProxy(this);
  m_DirProxy->setSourceModel(m_Model);

  m_DirProxy->showFiles = false;
  m_DirProxy->showHidden = ui->showHidden->isChecked();
  m_DirProxy->maxColCount = 1;

  m_FileProxy = new RemoteFileProxy(this);
  m_FileProxy->setSourceModel(m_Model);

  m_FileProxy->showHidden = ui->showHidden->isChecked();

  ui->dirList->setModel(m_DirProxy);
  ui->fileList->setModel(m_FileProxy);

  ui->fileList->sortByColumn(0, Qt::AscendingOrder);

  ui->fileList->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
  ui->fileList->header()->setSectionResizeMode(1, QHeaderView::Stretch);
  ui->fileList->header()->setSectionResizeMode(2, QHeaderView::Stretch);
  ui->fileList->header()->setSectionResizeMode(3, QHeaderView::Stretch);

  ui->filter->addItems({tr("Executables"), tr("All Files")});

  ui->back->setEnabled(false);
  ui->forward->setEnabled(false);
  ui->upFolder->setEnabled(false);

  ui->buttonBox->button(QDialogButtonBox::Ok)->setDefault(false);

  // switch to home folder and expand it
  changeCurrentDir(m_Model->homeFolder());
  ui->dirList->expand(m_DirProxy->mapFromSource(currentDir()));
}

VirtualFileDialog::~VirtualFileDialog()
{
  delete ui;
}

void VirtualFileDialog::setDirBrowse()
{
  m_FileProxy->showFiles = false;

  ui->buttonBox->button(QDialogButtonBox::Ok)->setText(tr("Select Folder"));

  ui->filter->hide();
}

void VirtualFileDialog::keyPressEvent(QKeyEvent *e)
{
  // swallow return/enter events
  if(e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter)
    return;
}

void VirtualFileDialog::accept()
{
  // do nothing, don't accept except via our explicit calls
}

void VirtualFileDialog::on_location_keyPress(QKeyEvent *e)
{
  // only process when enter is pressed
  if(e->key() != Qt::Key_Return && e->key() != Qt::Key_Enter)
    return;

  // parse folder
  QModelIndex idx = m_Model->indexForPath(ui->location->text());

  if(idx.isValid())
    changeCurrentDir(idx);
  else
    fileNotFound(ui->location->text());
}

QModelIndex VirtualFileDialog::currentDir()
{
  return m_FileProxy->mapToSource(ui->fileList->rootIndex());
}

void VirtualFileDialog::changeCurrentDir(const QModelIndex &index, bool recordHistory)
{
  // shouldn't happen, but sanity check
  if(!index.isValid())
    return;

  // ignore changes to current dir
  if(currentDir() == index)
    return;

  if(recordHistory)
  {
    // erase the history we backed up over
    while(m_History.count() > m_HistoryIndex + 1)
      m_History.pop_back();

    // add new history
    m_History.push_back(index);
    m_HistoryIndex = m_History.count() - 1;
  }

  ui->back->setEnabled(m_HistoryIndex > 0);
  ui->forward->setEnabled(m_HistoryIndex < m_History.count() - 1);

  QModelIndex fileIndex = m_FileProxy->mapFromSource(index);
  QModelIndex dirIndex = m_DirProxy->mapFromSource(index);

  // set file list to this dir
  ui->fileList->setRootIndex(fileIndex);

  // update location text
  ui->location->setText(m_FileProxy->data(fileIndex, RemoteFileModel::FilePathRole).toString());

  // enable up button if we're not at a root
  bool isRoot = m_FileProxy->data(fileIndex, RemoteFileModel::FileIsRootRole).toBool();
  ui->upFolder->setEnabled(!isRoot);

  // expand the directory list so this directory is visible
  QModelIndex parent = m_DirProxy->parent(dirIndex);
  while(parent.isValid())
  {
    ui->dirList->expand(parent);
    parent = m_DirProxy->parent(parent);
  }

  // select this directory
  ui->dirList->selectionModel()->setCurrentIndex(dirIndex, QItemSelectionModel::ClearAndSelect);

  // if it was access denied, show an error now
  if(m_FileProxy->data(fileIndex, RemoteFileModel::FileIsAccessDeniedRole).toBool())
  {
    accessDenied(ui->location->text());
  }
}

void VirtualFileDialog::on_dirList_clicked(const QModelIndex &index)
{
  changeCurrentDir(m_DirProxy->mapToSource(index));
}

void VirtualFileDialog::on_fileList_doubleClicked(const QModelIndex &index)
{
  bool isDir = m_FileProxy->data(index, RemoteFileModel::FileIsDirRole).toBool();

  if(isDir)
  {
    changeCurrentDir(m_FileProxy->mapToSource(index));
  }
  else
  {
    m_ChosenPath = m_FileProxy->data(index, RemoteFileModel::FilePathRole).toString();
    QDialog::accept();
  }
}

void VirtualFileDialog::on_fileList_clicked(const QModelIndex &index)
{
  ui->filename->setText(m_FileProxy->data(index, RemoteFileModel::FileNameRole).toString());
}

void VirtualFileDialog::on_showHidden_toggled(bool checked)
{
  m_DirProxy->showHidden = ui->showHidden->isChecked();
  m_FileProxy->showHidden = ui->showHidden->isChecked();
  m_DirProxy->refresh();
  m_FileProxy->refresh();
}

void VirtualFileDialog::on_filename_keyPress(QKeyEvent *e)
{
  // only process when enter is pressed
  if(e->key() != Qt::Key_Return && e->key() != Qt::Key_Enter)
    return;

  QModelIndex curDir = ui->fileList->rootIndex();

  QString text = ui->filename->text();

  QRegExp re(text);
  re.setPatternSyntax(QRegExp::Wildcard);

  int fileCount = m_FileProxy->rowCount(curDir);
  int matches = 0;
  QString match;

  for(int f = 0; f < fileCount; f++)
  {
    QModelIndex file = m_FileProxy->index(f, 0, curDir);
    bool isDir = m_FileProxy->data(file, RemoteFileModel::FileIsDirRole).toBool();

    if(isDir)
      continue;

    QString filename = m_FileProxy->data(file, RemoteFileModel::FileNameRole).toString();

    if(re.exactMatch(filename))
    {
      matches++;
      match = m_FileProxy->data(file, RemoteFileModel::FilePathRole).toString();
    }
  }

  if(matches == 1)
  {
    m_ChosenPath = match;
    QDialog::accept();
  }

  if(matches == 0 && !text.trimmed().isEmpty())
    fileNotFound(text);

  m_FileProxy->setFilterRegExp(re);
  m_FileProxy->refresh();
}

void VirtualFileDialog::on_filter_currentIndexChanged(int index)
{
  m_FileProxy->showNonExecutables = (index == 1);
  m_FileProxy->refresh();
}

void VirtualFileDialog::on_buttonBox_accepted()
{
  if(!m_FileProxy->showFiles)
  {
    // if browsing for a directory, accept current dir as path
    m_ChosenPath = m_Model->data(currentDir(), RemoteFileModel::FilePathRole).toString();
    QDialog::accept();
    return;
  }

  // simulate enter being pressed
  QKeyEvent fakeEvent(QEvent::KeyPress, Qt::Key_Return, 0);
  on_filename_keyPress(&fakeEvent);
}

void VirtualFileDialog::on_back_clicked()
{
  m_HistoryIndex = qMax(0, m_HistoryIndex - 1);
  changeCurrentDir(m_History[m_HistoryIndex], false);
}

void VirtualFileDialog::on_forward_clicked()
{
  m_HistoryIndex = qMin(m_HistoryIndex + 1, m_History.count() - 1);
  changeCurrentDir(m_History[m_HistoryIndex], false);
}

void VirtualFileDialog::on_upFolder_clicked()
{
  QModelIndex curDir = currentDir();

  changeCurrentDir(m_Model->parent(curDir));
}

void VirtualFileDialog::fileNotFound(const QString &path)
{
  RDDialog::critical(this, tr("File not found"),
                     tr("%1\nFile not found.\nCheck the file name and try again.").arg(path));
}

void VirtualFileDialog::accessDenied(const QString &path)
{
  RDDialog::critical(this, tr("Access is denied"),
                     tr("%1 is not accessible\n\nAccess is denied.").arg(path));
}
