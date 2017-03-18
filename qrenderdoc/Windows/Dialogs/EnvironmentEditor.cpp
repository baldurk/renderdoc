/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016-2017 Baldur Karlsson
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
#include "Code/QRDUtils.h"
#include "ui_EnvironmentEditor.h"

Q_DECLARE_METATYPE(EnvironmentModification);

EnvironmentEditor::EnvironmentEditor(QWidget *parent)
    : QDialog(parent), ui(new Ui::EnvironmentEditor)
{
  ui->setupUi(this);

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
      ToQStr(eEnvSep_Platform), ToQStr(eEnvSep_SemiColon), ToQStr(eEnvSep_Colon), ToQStr(eEnvSep_None),
  });

  ui->separator->setCurrentIndex(0);

  ui->setValue->setChecked(true);
  ui->name->setFocus();

  m_Completer = new QCompleter({}, this);

  ui->name->setCompleter(m_Completer);

  ui->variables->header()->setSectionResizeMode(0, QHeaderView::Interactive);
  ui->variables->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);

  ui->variables->setClearSelectionOnFocusLoss(false);
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

    ui->variables->clearSelection();
    ui->variables->topLevelItem(idx)->setSelected(true);
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

void EnvironmentEditor::on_variables_currentItemChanged(QTreeWidgetItem *current,
                                                        QTreeWidgetItem *previous)
{
  QList<QTreeWidgetItem *> sel = ui->variables->selectedItems();

  if(sel.isEmpty())
    return;

  EnvironmentModification mod = sel[0]->data(0, Qt::UserRole).value<EnvironmentModification>();

  if(!mod.variable.isEmpty())
  {
    ui->name->setText(mod.variable);
    ui->value->setText(mod.value);
    ui->separator->setCurrentIndex((int)mod.separator);

    if(mod.type == eEnvMod_Set)
      ui->setValue->setChecked(true);
    else if(mod.type == eEnvMod_Append)
      ui->appendValue->setChecked(true);
    else if(mod.type == eEnvMod_Prepend)
      ui->prependValue->setChecked(true);
  }
}

void EnvironmentEditor::on_addUpdate_clicked()
{
  EnvironmentModification mod;
  mod.variable = ui->name->text();
  mod.value = ui->value->text();
  mod.separator = (EnvironmentSeparator)ui->separator->currentIndex();

  if(ui->appendValue->isChecked())
    mod.type = eEnvMod_Append;
  else if(ui->prependValue->isChecked())
    mod.type = eEnvMod_Prepend;
  else
    mod.type = eEnvMod_Set;

  addModification(mod, false);

  on_name_textChanged(ui->name->text());
}

void EnvironmentEditor::on_deleteButton_clicked()
{
  QList<QTreeWidgetItem *> sel = ui->variables->selectedItems();

  if(sel.isEmpty())
    return;

  delete ui->variables->takeTopLevelItem(ui->variables->indexOfTopLevelItem(sel[0]));

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
  if(mod.variable.trimmed() == "")
  {
    if(!silent)
      RDDialog::critical(this, tr("Invalid variable"),
                         tr("Environment variable cannot be just whitespace"));

    return;
  }

  QTreeWidgetItem *node = NULL;

  int idx = existingIndex();

  if(idx < 0)
  {
    node = makeTreeNode({mod.variable, mod.GetTypeString(), mod.value});
    ui->variables->addTopLevelItem(node);
  }
  else
  {
    node = ui->variables->topLevelItem(idx);
    node->setText(0, mod.variable);
    node->setText(1, mod.GetTypeString());
    node->setText(2, mod.value);
  }

  node->setData(0, Qt::UserRole, QVariant::fromValue(mod));

  ui->variables->clearSelection();
  node->setSelected(true);

  delete m_Completer;

  QStringList names;
  for(int i = 0; i < ui->variables->topLevelItemCount(); i++)
    names << ui->variables->topLevelItem(i)->text(0);

  m_Completer = new QCompleter(names, this);

  ui->name->setCompleter(m_Completer);
}

QList<EnvironmentModification> EnvironmentEditor::modifications()
{
  QList<EnvironmentModification> ret;

  for(int i = 0; i < ui->variables->topLevelItemCount(); i++)
  {
    EnvironmentModification mod =
        ui->variables->topLevelItem(i)->data(0, Qt::UserRole).value<EnvironmentModification>();

    if(!mod.variable.isEmpty())
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
