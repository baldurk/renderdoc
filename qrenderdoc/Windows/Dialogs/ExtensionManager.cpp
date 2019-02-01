/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2018-2019 Baldur Karlsson
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

#include "ExtensionManager.h"
#include <QDesktopServices>
#include <QFileInfo>
#include <QKeyEvent>
#include <QRegularExpression>
#include "Code/Interface/QRDInterface.h"
#include "Code/Resources.h"
#include "Widgets/Extended/RDHeaderView.h"
#include "Windows/MainWindow.h"
#include "ui_ExtensionManager.h"

Q_DECLARE_METATYPE(ExtensionMetadata);

ExtensionManager::ExtensionManager(ICaptureContext &ctx)
    : QDialog(NULL), ui(new Ui::ExtensionManager), m_Ctx(ctx)
{
  ui->setupUi(this);

  {
    RDHeaderView *header = new RDHeaderView(Qt::Horizontal, this);
    ui->extensions->setHeader(header);

    ui->extensions->setColumns({tr("Package"), tr("Name"), tr("Loaded")});
    header->setColumnStretchHints({1, 4, -1});
  }

  ui->name->setText(lit("---"));
  ui->version->setText(lit("---"));
  ui->author->setText(lit("---"));
  ui->URL->setText(lit("---"));
  ui->reload->setEnabled(false);
  ui->alwaysLoad->setEnabled(false);

  QObject::connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);

  QString extensionFolder = configFilePath("extensions");

  m_Extensions = m_Ctx.Extensions().GetInstalledExtensions();

  if(m_Extensions.isEmpty())
  {
    ui->extensions->addTopLevelItem(
        new RDTreeWidgetItem({QString(), lit("No extensions found available"), QString()}));
    ui->extensions->addTopLevelItem(new RDTreeWidgetItem(
        {QString(), lit("Create packages in %1").arg(extensionFolder), QString()}));
  }
  else
  {
    for(const ExtensionMetadata &e : m_Extensions)
    {
      RDTreeWidgetItem *item = new RDTreeWidgetItem({e.package, e.name, QString()});

      item->setCheckState(
          2, m_Ctx.Extensions().IsExtensionLoaded(e.package) ? Qt::Checked : Qt::Unchecked);

      ui->extensions->addTopLevelItem(item);
    }

    ui->extensions->setCurrentItem(ui->extensions->topLevelItem(0));
  }
}

ExtensionManager::~ExtensionManager()
{
  delete ui;
}

void ExtensionManager::on_reload_clicked()
{
  RDTreeWidgetItem *item = ui->extensions->currentItem();
  if(!item)
    return;

  int idx = ui->extensions->indexOfTopLevelItem(item);

  if(idx >= 0 && idx < m_Extensions.count())
  {
    const ExtensionMetadata &e = m_Extensions[idx];
    if(!e.name.isEmpty())
    {
      // if the load succeeds, set us as checked. Otherwise, unchecked
      if(m_Ctx.Extensions().LoadExtension(e.package))
      {
        item->setCheckState(2, Qt::Checked);
      }
      else
      {
        item->setCheckState(2, Qt::Unchecked);
        RDDialog::critical(this, tr("Failed to load extension"),
                           tr("Failed to load extension '%1'.\n"
                              "Check the diagnostic log for python errors")
                               .arg(e.name));
      }

      update_currentItem(item);
    }
  }
}

void ExtensionManager::on_openLocation_clicked()
{
  if(m_Extensions.empty())
  {
    QDesktopServices::openUrl(QString(configFilePath("extensions")));
    return;
  }

  RDTreeWidgetItem *item = ui->extensions->currentItem();
  if(!item)
    return;

  int idx = ui->extensions->indexOfTopLevelItem(item);

  if(idx >= 0 && idx < m_Extensions.count())
  {
    const ExtensionMetadata &e = m_Extensions[idx];
    if(!e.name.isEmpty())
    {
      QDesktopServices::openUrl(QFileInfo(e.filePath).absoluteFilePath());
    }
  }
}

void ExtensionManager::on_alwaysLoad_toggled(bool checked)
{
  RDTreeWidgetItem *item = ui->extensions->currentItem();
  if(!item)
    return;

  int idx = ui->extensions->indexOfTopLevelItem(item);

  if(idx >= 0 && idx < m_Extensions.count())
  {
    const ExtensionMetadata &e = m_Extensions[idx];
    if(!e.name.isEmpty())
    {
      m_Ctx.Config().AlwaysLoad_Extensions.removeOne(e.package);
      if(checked)
        m_Ctx.Config().AlwaysLoad_Extensions.push_back(e.package);

      m_Ctx.Config().Save();
    }
  }
}

void ExtensionManager::on_extensions_currentItemChanged(RDTreeWidgetItem *item, RDTreeWidgetItem *)
{
  update_currentItem(item);
}

void ExtensionManager::on_extensions_itemChanged(RDTreeWidgetItem *item, int col)
{
  if(col == 2)
  {
    ui->extensions->setCurrentItem(item);

    bool loaded = m_Ctx.Extensions().IsExtensionLoaded(item->text(0));

    // if the extension is loaded, don't allow unchecking
    if(loaded && item->checkState(2) != Qt::Checked)
    {
      item->setCheckState(2, Qt::Checked);
      return;
    }

    // if the extension is unloaded, if we're now checked then try to load it. If
    // we're unchecked allow that (it is a code-change after we failed to load)
    if(!loaded)
    {
      if(item->checkState(2) == Qt::Checked)
        on_reload_clicked();
    }
  }
}

void ExtensionManager::update_currentItem(RDTreeWidgetItem *item)
{
  if(!item)
    return;

  if(item != ui->extensions->currentItem())
  {
    ui->extensions->setCurrentItem(item);
    return;
  }

  int idx = ui->extensions->indexOfTopLevelItem(item);

  if(idx >= 0 && idx < m_Extensions.count())
  {
    const ExtensionMetadata &e = m_Extensions[idx];
    if(!e.name.isEmpty())
    {
      QRegularExpression authRE(lit("^(.*) <(.*)>$"));

      ui->name->setText(e.name);
      ui->version->setText(e.version);
      ui->URL->setText(QFormatStr("<a href=\"%1\">%1</a>").arg(e.extensionURL));
      ui->description->setText(e.description);

      QRegularExpressionMatch match = authRE.match(QString(e.author).trimmed());

      if(match.hasMatch() && match.captured(2).contains(QLatin1Char('@')))
        ui->author->setText(
            QFormatStr("<a href=\"mailto:%2\">%1</a>").arg(match.captured(1)).arg(match.captured(2)));
      else
        ui->author->setText(e.author);

      bool loaded = item->checkState(2) == Qt::Checked;
      ui->reload->setEnabled(true);
      ui->reload->setText(loaded ? tr("Reload") : tr("Load"));
      ui->alwaysLoad->setEnabled(loaded);

      ui->alwaysLoad->setChecked(m_Ctx.Config().AlwaysLoad_Extensions.contains(e.package));
    }
  }
}