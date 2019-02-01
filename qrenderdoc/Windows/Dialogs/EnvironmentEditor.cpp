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

#include "EnvironmentEditor.h"
#include <QCompleter>
#include <QFontDatabase>
#include <QKeyEvent>
#include <QStringListModel>
#include "Code/QRDUtils.h"
#include "ui_EnvironmentEditor.h"

static QString GetTypeString(const EnvironmentModification &env)
{
  QString ret;

  if(env.mod == EnvMod::Append)
    ret = QApplication::translate("EnvironmentModification", "Append, %1").arg(ToQStr(env.sep));
  else if(env.mod == EnvMod::Prepend)
    ret = QApplication::translate("EnvironmentModification", "Prepend, %1").arg(ToQStr(env.sep));
  else
    ret = QApplication::translate("EnvironmentModification", "Set");

  return ret;
}

Q_DECLARE_METATYPE(EnvironmentModification);

EnvironmentEditor::EnvironmentEditor(QWidget *parent)
    : QDialog(parent), ui(new Ui::EnvironmentEditor)
{
  ui->setupUi(this);

  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

  auto commitLambda = [this](QKeyEvent *event) {
    if(event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
      on_addUpdate_clicked();
  };

  QObject::connect(ui->name, &RDLineEdit::keyPress, commitLambda);
  QObject::connect(ui->value, &RDLineEdit::keyPress, commitLambda);

  auto separatorLambda = [this]() { ui->separator->setEnabled(!ui->setValue->isChecked()); };

  QObject::connect(ui->setValue, &QRadioButton::toggled, separatorLambda);
  QObject::connect(ui->prependValue, &QRadioButton::toggled, separatorLambda);
  QObject::connect(ui->appendValue, &QRadioButton::toggled, separatorLambda);

  ui->separator->addItems({
      ToQStr(EnvSep::Platform), ToQStr(EnvSep::SemiColon), ToQStr(EnvSep::Colon),
      ToQStr(EnvSep::NoSep),
  });

  ui->separator->setCurrentIndex(0);

  ui->setValue->setChecked(true);
  ui->name->setFocus();

  m_Completer = new QCompleter({}, this);

  ui->name->setCompleter(m_Completer);

  ui->variables->setColumns({tr("Name"), tr("Modification"), tr("Value")});

  ui->variables->header()->setSectionResizeMode(0, QHeaderView::Interactive);
  ui->variables->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);

  ui->variables->sortByColumn(0, Qt::DescendingOrder);

  ui->variables->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
}

EnvironmentEditor::~EnvironmentEditor()
{
  delete ui;
}

void EnvironmentEditor::on_name_textChanged(const QString &text)
{
  int idx = existingIndex();

  if(idx >= 0)
  {
    ui->addUpdate->setText(tr("Update"));
    ui->deleteButton->setEnabled(true);

    ui->variables->setSelectedItem(ui->variables->topLevelItem(idx));
  }
  else
  {
    ui->addUpdate->setText(tr("Add"));
    ui->deleteButton->setEnabled(false);

    ui->addUpdate->setEnabled(!text.trimmed().isEmpty());
  }
}

void EnvironmentEditor::on_variables_keyPress(QKeyEvent *event)
{
  if(event->key() == Qt::Key_Delete && ui->deleteButton->isEnabled())
    on_deleteButton_clicked();
}

void EnvironmentEditor::on_variables_currentItemChanged(RDTreeWidgetItem *current,
                                                        RDTreeWidgetItem *previous)
{
  if(!current)
    return;

  EnvironmentModification mod = current->tag().value<EnvironmentModification>();

  if(!mod.value.empty())
  {
    ui->name->setText(mod.name);
    ui->value->setText(mod.value);
    ui->separator->setCurrentIndex((int)mod.sep);

    if(mod.mod == EnvMod::Set)
      ui->setValue->setChecked(true);
    else if(mod.mod == EnvMod::Append)
      ui->appendValue->setChecked(true);
    else if(mod.mod == EnvMod::Prepend)
      ui->prependValue->setChecked(true);
  }
}

void EnvironmentEditor::on_addUpdate_clicked()
{
  EnvironmentModification mod;
  mod.name = ui->name->text().toUtf8().data();
  mod.value = ui->value->text().toUtf8().data();
  mod.sep = (EnvSep)qMax(0, ui->separator->currentIndex());

  if(ui->appendValue->isChecked())
    mod.mod = EnvMod::Append;
  else if(ui->prependValue->isChecked())
    mod.mod = EnvMod::Prepend;
  else
    mod.mod = EnvMod::Set;

  addModification(mod, false);

  on_name_textChanged(ui->name->text());
}

void EnvironmentEditor::on_deleteButton_clicked()
{
  RDTreeWidgetItem *sel = ui->variables->selectedItem();

  if(!sel)
    return;

  int idx = ui->variables->indexOfTopLevelItem(sel);

  if(idx >= 0)
    delete ui->variables->takeTopLevelItem(idx);
  else
    qCritical() << "Can't find item to delete";

  on_name_textChanged(ui->name->text());
}

int EnvironmentEditor::existingIndex()
{
  QString name = ui->name->text();

  for(int i = 0; i < ui->variables->topLevelItemCount(); i++)
    if(ui->variables->topLevelItem(i)->text(0) == name)
      return i;

  return -1;
}

void EnvironmentEditor::addModification(EnvironmentModification mod, bool silent)
{
  if(mod.name.empty())
  {
    if(!silent)
      RDDialog::critical(this, tr("Invalid variable"),
                         tr("Environment variable cannot be just whitespace"));

    return;
  }

  RDTreeWidgetItem *node = NULL;

  int idx = existingIndex();

  if(idx < 0)
  {
    node = new RDTreeWidgetItem({mod.name, GetTypeString(mod), mod.value});
    ui->variables->addTopLevelItem(node);
  }
  else
  {
    node = ui->variables->topLevelItem(idx);
    node->setText(0, mod.name);
    node->setText(1, GetTypeString(mod));
    node->setText(2, mod.value);
  }

  node->setTag(QVariant::fromValue(mod));

  ui->variables->setSelectedItem(node);

  QStringList names;
  for(int i = 0; i < ui->variables->topLevelItemCount(); i++)
    names << ui->variables->topLevelItem(i)->text(0);

  m_Completer->setModel(new QStringListModel(names, m_Completer));
}

QList<EnvironmentModification> EnvironmentEditor::modifications()
{
  QList<EnvironmentModification> ret;

  for(int i = 0; i < ui->variables->topLevelItemCount(); i++)
  {
    EnvironmentModification mod =
        ui->variables->topLevelItem(i)->tag().value<EnvironmentModification>();

    if(!mod.name.empty())
      ret.push_back(mod);
  }

  return ret;
}

void EnvironmentEditor::on_buttonBox_accepted()
{
  int idx = existingIndex();

  if(idx >= 0 || ui->name->text().isEmpty())
  {
    accept();
    return;
  }

  QMessageBox::StandardButton res = RDDialog::question(
      this, tr("Variable not added"),
      tr("You did not add the variable modification you were editing. Add it now?"),
      RDDialog::YesNoCancel, QMessageBox::Yes);

  if(res == QMessageBox::Yes)
  {
    on_addUpdate_clicked();
  }
  else if(res == QMessageBox::Cancel)
  {
    // don't close
    return;
  }

  accept();
}
