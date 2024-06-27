/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2020-2024 Baldur Karlsson
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

#include "ConfigEditor.h"
#include <float.h>
#include <QDoubleSpinBox>
#include <QKeyEvent>
#include <QSpinBox>
#include "Code/QRDUtils.h"
#include "Code/Resources.h"
#include "Widgets/Extended/RDHeaderView.h"
#include "Widgets/Extended/RDLineEdit.h"
#include "Widgets/OrderedListEditor.h"
#include "ui_ConfigEditor.h"

static QString valueString(const SDObject *o)
{
  if(o->type.basetype == SDBasic::String)
    return o->data.str;

  if(o->type.basetype == SDBasic::UnsignedInteger)
    return Formatter::Format(o->data.basic.u);

  if(o->type.basetype == SDBasic::SignedInteger)
    return Formatter::Format(o->data.basic.i);

  if(o->type.basetype == SDBasic::Float)
    return Formatter::Format(o->data.basic.d);

  if(o->type.basetype == SDBasic::Boolean)
    return o->data.basic.b ? lit("True") : lit("False");

  if(o->type.basetype == SDBasic::Array)
    return lit("{...}");

  return lit("??");
}

static bool anyChildChanged(const SDObject *o)
{
  const SDObject *def = o->FindChild("default");
  const SDObject *val = o->FindChild("value");

  if(val && def)
    return !val->HasEqualValue(def);

  for(const SDObject *c : *o)
  {
    if(anyChildChanged(c))
      return true;
  }

  return false;
}

class SettingModel : public QAbstractItemModel
{
public:
  SettingModel(ConfigEditor *view) : QAbstractItemModel(view), m_Viewer(view)
  {
    populateParents(m_Viewer->m_Config, QModelIndex());
  }

  void refresh()
  {
    emit beginResetModel();
    emit endResetModel();
  }

  QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override
  {
    SDObject *o = obj(parent);

    if(row < 0 || row > rowCount(parent))
      return QModelIndex();

    return createIndex(row, column, o->GetChild(row));
  }

  QModelIndex parent(const QModelIndex &index) const override
  {
    SDObject *o = obj(index);

    if(o == m_Viewer->m_Config)
      return QModelIndex();

    QModelIndex ret = parents[o];

    if(!ret.isValid())
      return ret;

    return createIndex(ret.row(), index.column(), ret.internalPointer());
  }

  int rowCount(const QModelIndex &parent = QModelIndex()) const override
  {
    SDObject *o = obj(parent);

    // values don't have children
    if(o->FindChild("value"))
      return 0;

    return (int)o->NumChildren();
  }

  enum Columns
  {
    Column_Name,
    Column_Value,
    Column_ResetButton,
    Column_Count,
  };

  Qt::ItemFlags flags(const QModelIndex &index) const override
  {
    if(!index.isValid())
      return 0;

    Qt::ItemFlags ret = QAbstractItemModel::flags(index);

    if(index.column() == Column_Value)
    {
      SDObject *o = obj(index);
      SDObject *value = o->FindChild("value");

      if(value)
      {
        ret |= Qt::ItemIsEditable;
        if(value->type.basetype == SDBasic::Boolean)
          ret |= Qt::ItemIsUserCheckable;
      }
    }

    return ret;
  }

  int columnCount(const QModelIndex &parent = QModelIndex()) const override { return Column_Count; }
  QVariant headerData(int section, Qt::Orientation orientation, int role) const override
  {
    if(orientation == Qt::Horizontal && role == Qt::DisplayRole)
    {
      switch(section)
      {
        case Column_Name: return lit("Setting");
        case Column_Value: return lit("Value");
        case Column_ResetButton: return lit("Reset");
        default: break;
      }
    }

    return QVariant();
  }

  bool setData(const QModelIndex &index, const QVariant &val, int role) override
  {
    SDObject *o = NULL;

    if(role == Qt::UserRole)
    {
      // if we have setData for user role, that means we got reset. Just need to emit data changed
      o = obj(index);
    }
    else if(index.column() == Column_Value)
    {
      o = obj(index);

      SDObject *value = o->FindChild("value");

      if(role == Qt::CheckStateRole && value)
      {
        value->data.basic.b = (val.toInt() == Qt::CheckState::Checked);
      }
      else
      {
        // didn't change anything we care about
        o = NULL;
      }
    }

    if(o)
    {
      // dataChanged this index and all parents (in case a section became non-customised, or
      // customised, and it wasn't before)
      QModelIndex idx = index;
      while(idx.isValid())
      {
        o = obj(idx);
        emit dataChanged(createIndex(idx.row(), 0, o), createIndex(idx.row(), Column_Count, o),
                         {Qt::DisplayRole, Qt::CheckStateRole, Qt::FontRole});

        idx = parents[o];
      }

      return true;
    }

    return false;
  }

  QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override
  {
    if(index.isValid())
    {
      SDObject *o = obj(index);
      int col = index.column();

      SDObject *value = o->FindChild("value");

      if(role == Qt::UserRole)
      {
        return QVariant::fromValue((quintptr)o);
      }
      else if(role == Qt::DisplayRole)
      {
        switch(col)
        {
          case Column_Name: return o->name;
          case Column_Value:
            return value && value->type.basetype != SDBasic::Boolean ? valueString(value)
                                                                     : QVariant();
          case Column_ResetButton: return anyChildChanged(o) ? lit("...") : QVariant();
          default: break;
        }
      }
      else if(role == Qt::CheckStateRole && col == Column_Value)
      {
        if(value && value->type.basetype == SDBasic::Boolean)
          return value->data.basic.b ? Qt::CheckState::Checked : Qt::CheckState::Unchecked;
        return QVariant();
      }
      else if(role == Qt::TextAlignmentRole && col == Column_ResetButton)
      {
        return Qt::AlignHCenter + Qt::AlignTop;
      }
      else if(role == Qt::ToolTipRole)
      {
        SDObject *desc = o->FindChild("description");
        if(desc)
        {
          rdcstr ret = desc->AsString();

          if(o->FindChild("key") == NULL)
          {
            ret =
                "WARNING: Unknown setting, possibly it has been removed or from a different "
                "build.\n\n" +
                ret;
          }

          return ret;
        }
      }
      else if(role == Qt::FontRole)
      {
        if(anyChildChanged(o))
        {
          QFont font;
          font.setBold(true);
          return font;
        }
        // if this is a value but has no key, it's an unrecognised setting (stale/removed, or from
        // a different or future build).
        if(o->FindChild("description") && o->FindChild("key") == NULL)
        {
          QFont font;
          font.setItalic(true);
          return font;
        }
      }
    }

    return QVariant();
  }

private:
  SDObject *obj(const QModelIndex &parent) const
  {
    SDObject *ret = (SDObject *)parent.internalPointer();
    if(ret == NULL)
      ret = m_Viewer->m_Config;
    return ret;
  }

  // Qt models need child->parent relationships. We don't have that with SDObject but they are
  // immutable so we can cache them
  QMap<SDObject *, QModelIndex> parents;

  void populateParents(SDObject *o, QModelIndex parent)
  {
    if(o->FindChild("value"))
      return;

    int i = 0;
    for(SDObject *c : *o)
    {
      parents[c] = parent;
      populateParents(c, index(i++, 0, parent));
    }
  }

  ConfigEditor *m_Viewer;
};

class SettingFilterModel : public QSortFilterProxyModel
{
public:
  explicit SettingFilterModel(ConfigEditor *view) : QSortFilterProxyModel(view), m_Viewer(view) {}
  void setFilter(QString text)
  {
    m_Text = text;
    m_KeyText = m_Text;
    m_KeyText.replace(QLatin1Char('.'), QLatin1Char('_'));
    invalidateFilter();
  }

protected:
  virtual bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override
  {
    if(m_Text.isEmpty())
      return true;

    SDObject *o = obj(source_parent);

    return matchesAnyChild(o->GetChild(source_row));
  }

  bool matchesAnyChild(SDObject *o) const
  {
    if(!o)
      return false;

    if(QString(o->name).contains(m_Text, Qt::CaseInsensitive))
      return true;

    if(o->FindChild("value"))
    {
      if(o->FindChild("key") &&
         QString(o->FindChild("key")->AsString()).contains(m_KeyText, Qt::CaseInsensitive))
        return true;

      return false;
    }

    for(SDObject *c : *o)
      if(matchesAnyChild(c))
        return true;

    return false;
  }

private:
  SDObject *obj(const QModelIndex &parent) const
  {
    SDObject *ret = (SDObject *)parent.internalPointer();
    if(ret == NULL)
      ret = m_Viewer->m_Config;
    return ret;
  }
  ConfigEditor *m_Viewer;

  QString m_Text;
  QString m_KeyText;
};

SettingDelegate::SettingDelegate(ConfigEditor *editor, RDTreeView *parent)
    : QStyledItemDelegate(parent), m_Editor(editor), m_View(parent)
{
}

SettingDelegate::~SettingDelegate()
{
}

void SettingDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                            const QModelIndex &index) const
{
  if(index.column() == SettingModel::Column_ResetButton)
  {
    SDObject *o = (SDObject *)index.data(Qt::UserRole).toULongLong();

    SDObject *def = o->FindChild("default");
    SDObject *val = o->FindChild("value");

    if(val && def && !val->HasEqualValue(def))
    {
      // draw the item without text, so we get the proper background/selection/etc.
      // we'd like to be able to use the parent delegate's paint here, but either it calls to
      // QStyledItemDelegate which will re-fetch the text (bleh), or it calls to the manual
      // delegate which could do anything. So for this case we just use the style and skip the
      // delegate and hope it works out.
      QStyleOptionViewItem opt = option;
      QStyledItemDelegate::initStyleOption(&opt, index);
      opt.text.clear();
      m_Editor->style()->drawControl(QStyle::CE_ItemViewItem, &opt, painter, m_Editor);

      QStyleOptionToolButton buttonOpt;

      int size = m_Editor->style()->pixelMetric(QStyle::PM_SmallIconSize, 0, m_Editor);

      buttonOpt.iconSize = QSize(size, size);
      buttonOpt.subControls = 0;
      buttonOpt.activeSubControls = 0;
      buttonOpt.features = QStyleOptionToolButton::None;
      buttonOpt.arrowType = Qt::NoArrow;
      buttonOpt.state = QStyle::State_Active | QStyle::State_Enabled | QStyle::State_AutoRaise;

      buttonOpt.rect = option.rect.adjusted(0, 0, -1, -1);
      buttonOpt.icon = Icons::arrow_undo();

      if(m_View->currentHoverIndex() == index)
        buttonOpt.state |= QStyle::State_MouseOver;

      m_Editor->style()->drawComplexControl(QStyle::CC_ToolButton, &buttonOpt, painter, m_Editor);
      return;
    }
  }

  return QStyledItemDelegate::paint(painter, option, index);
}

QSize SettingDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
  return QStyledItemDelegate::sizeHint(option, index);
}

bool SettingDelegate::editorEvent(QEvent *event, QAbstractItemModel *model,
                                  const QStyleOptionViewItem &option, const QModelIndex &index)
{
  if(event->type() == QEvent::MouseButtonRelease && index.column() == SettingModel::Column_ResetButton)
  {
    SDObject *o = (SDObject *)index.data(Qt::UserRole).toULongLong();

    SDObject *def = o->FindChild("default");
    SDObject *val = o->FindChild("value");

    if(def && val)
    {
      val->data.str = def->data.str;
      memcpy(&val->data.basic, &def->data.basic, sizeof(val->data.basic));

      val->DeleteChildren();

      for(size_t c = 0; c < def->NumChildren(); c++)
        val->DuplicateAndAddChild(def->GetChild(c));

      // call setData() to emit the dataChanged for this element and all parents
      model->setData(index, QVariant(), Qt::UserRole);

      return true;
    }
  }

  return QStyledItemDelegate::editorEvent(event, model, option, index);
}

QWidget *SettingDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                                       const QModelIndex &index) const
{
  QWidget *ret = NULL;

  SDObject *o = (SDObject *)index.data(Qt::UserRole).toULongLong();
  SDObject *val = o->FindChild("value");
  if(val)
  {
    // bools should have checkboxes
    if(val->type.basetype == SDBasic::Boolean)
    {
      qWarning() << "Unexpected createEditor for boolean " << QString(o->name);
      return ret;
    }

    QString settingName;

    SDObject *key = o->FindChild("key");
    if(key)
      settingName = key->AsString();
    else
      settingName = tr("Unknown Setting %1").arg(o->name);

    // for numbers, provide a spinbox
    if(val->type.basetype == SDBasic::UnsignedInteger || val->type.basetype == SDBasic::SignedInteger)
    {
      QSpinBox *spin = new QSpinBox(parent);
      ret = spin;
      if(val->type.basetype == SDBasic::UnsignedInteger)
        spin->setMinimum(0);
      else
        spin->setMinimum(INT_MIN);
      spin->setMaximum(INT_MAX);
    }
    else if(val->type.basetype == SDBasic::Float)
    {
      QDoubleSpinBox *spin = new QDoubleSpinBox(parent);
      ret = spin;
      spin->setSingleStep(0.1);
      spin->setMinimum(-FLT_MAX);
      spin->setMaximum(FLT_MAX);
    }
    else if(val->type.basetype == SDBasic::String)
    {
      if(QString(o->name).contains(lit("DirPath"), Qt::CaseSensitive))
      {
        QString dir = RDDialog::getExistingDirectory(m_Editor, tr("Browse for %1").arg(settingName));

        if(!dir.isEmpty())
        {
          val->data.str = dir;

          // we've handled the edit synchronously, don't create an edit widget
          ret = NULL;

          // call setData() to emit the dataChanged for this element and all parents
          m_View->model()->setData(index, QVariant(), Qt::UserRole);
        }
      }
      else if(QString(o->name).contains(lit("Path"), Qt::CaseSensitive))
      {
        QString file = RDDialog::getOpenFileName(m_Editor, tr("Browse for %1").arg(settingName));

        if(!file.isEmpty())
        {
          val->data.str = file;

          // we've handled the edit synchronously, don't create an edit widget
          ret = NULL;

          // call setData() to emit the dataChanged for this element and all parents
          m_View->model()->setData(index, QVariant(), Qt::UserRole);
        }
      }
      else
      {
        RDLineEdit *line = new RDLineEdit(parent);
        ret = line;

        QObject::connect(line, &RDLineEdit::keyPress, this, &SettingDelegate::editorKeyPress);
      }
    }
    else if(val->type.basetype == SDBasic::Array)
    {
      // only support arrays of strings. Pop up a separate editor to handle this
      QDialog listEditor;

      listEditor.setWindowTitle(tr("Edit values of %1").arg(QString(settingName)));
      listEditor.setWindowFlags(listEditor.windowFlags() & ~Qt::WindowContextHelpButtonHint);

      ItemButton mode = ItemButton::None;

      if(QString(o->name).contains(lit("DirPath"), Qt::CaseSensitive))
        mode = ItemButton::BrowseFolder;
      else if(QString(o->name).contains(lit("Path"), Qt::CaseSensitive))
        mode = ItemButton::BrowseFile;

      OrderedListEditor list(tr("Entry"), mode);

      QVBoxLayout layout;
      QDialogButtonBox okCancel;
      okCancel.setStandardButtons(QDialogButtonBox::Cancel | QDialogButtonBox::Ok);
      layout.addWidget(&list);
      layout.addWidget(&okCancel);

      QObject::connect(&okCancel, &QDialogButtonBox::accepted, &listEditor, &QDialog::accept);
      QObject::connect(&okCancel, &QDialogButtonBox::rejected, &listEditor, &QDialog::reject);

      listEditor.setLayout(&layout);

      QStringList items;

      for(SDObject *c : *val)
        items << c->data.str;

      list.setItems(items);

      int res = RDDialog::show(&listEditor);

      if(res)
      {
        items = list.getItems();

        val->DeleteChildren();
        val->ReserveChildren(items.size());

        for(int i = 0; i < items.size(); i++)
          val->AddAndOwnChild(makeSDString("$el"_lit, items[i]));
      }

      // we've handled the edit synchronously, don't create an edit widget
      ret = NULL;

      // call setData() to emit the dataChanged for this element and all parents
      m_View->model()->setData(index, QVariant(), Qt::UserRole);
    }
    else
    {
      qWarning() << "Unexpected type of " << QString(o->name)
                 << " to edit: " << ToQStr(val->type.basetype);
    }
  }

  return ret;
}

void SettingDelegate::editorKeyPress(QKeyEvent *ev)
{
  QLineEdit *line = qobject_cast<QLineEdit *>(QObject::sender());

  if(ev->key() == Qt::Key_Return || ev->key() == Qt::Key_Enter)
  {
    commitData(line);
    closeEditor(line);
  }
  else if(ev->key() == Qt::Key_Escape)
  {
    closeEditor(line);
  }
}

void SettingDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
  SDObject *o = (SDObject *)index.data(Qt::UserRole).toULongLong();
  SDObject *val = o->FindChild("value");
  if(val)
  {
    if(val->type.basetype == SDBasic::Boolean)
    {
      qWarning() << "Unexpected setEditorData for boolean " << QString(o->name);
      return;
    }

    if(val->type.basetype == SDBasic::UnsignedInteger)
      qobject_cast<QSpinBox *>(editor)->setValue(val->AsUInt32() & 0x7fffffffU);
    else if(val->type.basetype == SDBasic::SignedInteger)
      qobject_cast<QSpinBox *>(editor)->setValue(val->AsInt32());
    else if(val->type.basetype == SDBasic::Float)
      qobject_cast<QDoubleSpinBox *>(editor)->setValue(val->AsDouble());
    else if(val->type.basetype == SDBasic::String)
      qobject_cast<QLineEdit *>(editor)->setText(val->AsString());
    else
      qWarning() << "Unexpected type of " << QString(o->name) << ": " << ToQStr(val->type.basetype);
  }
}

void SettingDelegate::setModelData(QWidget *editor, QAbstractItemModel *model,
                                   const QModelIndex &index) const
{
  SDObject *o = (SDObject *)index.data(Qt::UserRole).toULongLong();
  SDObject *val = o->FindChild("value");
  if(val)
  {
    if(val->type.basetype == SDBasic::Boolean)
    {
      qWarning() << "Unexpected setModelData for boolean " << QString(o->name);
      return;
    }

    if(val->type.basetype == SDBasic::UnsignedInteger)
      val->data.basic.u = qMax(0, qobject_cast<QSpinBox *>(editor)->value());
    else if(val->type.basetype == SDBasic::SignedInteger)
      val->data.basic.i = qobject_cast<QSpinBox *>(editor)->value();
    else if(val->type.basetype == SDBasic::Float)
      val->data.basic.d = qobject_cast<QSpinBox *>(editor)->value();
    else if(val->type.basetype == SDBasic::String)
      val->data.str = qobject_cast<QLineEdit *>(editor)->text();
    else
      qWarning() << "Unexpected type of " << QString(o->name) << ": " << ToQStr(val->type.basetype);
  }
}

ConfigEditor::ConfigEditor(QWidget *parent) : QDialog(parent), ui(new Ui::ConfigEditor)
{
  ui->setupUi(this);
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

  m_Config = RENDERDOC_SetConfigSetting("");

  m_SettingModel = new SettingModel(this);
  m_FilterModel = new SettingFilterModel(this);

  m_FilterModel->setSourceModel(m_SettingModel);
  ui->settings->setModel(m_FilterModel);

  {
    RDHeaderView *header = new RDHeaderView(Qt::Horizontal, ui->settings);
    ui->settings->setHeader(header);

    header->setColumnStretchHints({-1, 1, -1});
  }

  ui->settings->setItemDelegate(new SettingDelegate(this, ui->settings));
}

ConfigEditor::~ConfigEditor()
{
  delete ui;
}

void ConfigEditor::on_filter_textChanged(const QString &text)
{
  RDTreeViewExpansionState state;
  ui->settings->saveExpansion(state, 0);
  m_FilterModel->setFilter(text);
  ui->settings->applyExpansion(state, 0);
}

void ConfigEditor::keyPressEvent(QKeyEvent *e)
{
  if(e->key() == Qt::Key_Enter || e->key() == Qt::Key_Return)
    return;
  QDialog::keyPressEvent(e);
}
