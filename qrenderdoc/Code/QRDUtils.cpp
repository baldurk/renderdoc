/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Baldur Karlsson
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

#include "QRDUtils.h"
#include <QGuiApplication>
#include <QJsonDocument>
#include <QMenu>

bool SaveToJSON(QVariantMap &data, QIODevice &f, const char *magicIdentifier, uint32_t magicVersion)
{
  // marker that this data is valid
  data[magicIdentifier] = magicVersion;

  QJsonDocument doc = QJsonDocument::fromVariant(data);

  if(doc.isEmpty() || doc.isNull())
  {
    qCritical() << "Failed to convert data to JSON document";
    return false;
  }

  QByteArray jsontext = doc.toJson(QJsonDocument::Indented);

  qint64 ret = f.write(jsontext);

  if(ret != jsontext.size())
  {
    qCritical() << "Failed to write JSON data: " << ret << " " << f.errorString();
    return false;
  }

  return true;
}

bool LoadFromJSON(QVariantMap &data, QIODevice &f, const char *magicIdentifier, uint32_t magicVersion)
{
  QByteArray json = f.readAll();

  if(json.isEmpty())
  {
    qCritical() << "Read invalid empty JSON data from file " << f.errorString();
    return false;
  }

  QJsonDocument doc = QJsonDocument::fromJson(json);

  if(doc.isEmpty() || doc.isNull())
  {
    qCritical() << "Failed to convert file to JSON document";
    return false;
  }

  data = doc.toVariant().toMap();

  if(data.isEmpty() || !data.contains(magicIdentifier))
  {
    qCritical() << "Converted config data is invalid or unrecognised";
    return false;
  }

  if(data[magicIdentifier].toUInt() != magicVersion)
  {
    qCritical() << "Converted config data is not the right version";
    return false;
  }

  return true;
}

void GUIInvoke::call(const std::function<void()> &f)
{
  if(qApp->thread() == QThread::currentThread())
  {
    f();
    return;
  }

  // TODO: could maybe do away with string compare here via caching
  // invoke->metaObject()->indexOfMethod("doInvoke"); ?

  GUIInvoke *invoke = new GUIInvoke(f);
  invoke->moveToThread(qApp->thread());
  QMetaObject::invokeMethod(invoke, "doInvoke", Qt::QueuedConnection);
}

void GUIInvoke::blockcall(const std::function<void()> &f)
{
  if(qApp->thread() == QThread::currentThread())
  {
    f();
    return;
  }

  // TODO: could maybe do away with string compare here via caching
  // invoke->metaObject()->indexOfMethod("doInvoke"); ?

  GUIInvoke *invoke = new GUIInvoke(f);
  invoke->moveToThread(qApp->thread());
  QMetaObject::invokeMethod(invoke, "doInvoke", Qt::BlockingQueuedConnection);
}

void RDDialog::show(QMenu *menu, QPoint pos)
{
  menu->setWindowModality(Qt::ApplicationModal);
  menu->popup(pos);
  QEventLoop loop;
  while(menu->isVisible())
  {
    loop.processEvents(QEventLoop::WaitForMoreEvents);
    QCoreApplication::sendPostedEvents();
  }
}

int RDDialog::show(QDialog *dialog)
{
  dialog->setWindowModality(Qt::ApplicationModal);
  dialog->show();
  QEventLoop loop;
  while(dialog->isVisible())
  {
    loop.processEvents(QEventLoop::WaitForMoreEvents);
    QCoreApplication::sendPostedEvents();
  }

  return dialog->result();
}

QMessageBox::StandardButton RDDialog::messageBox(QMessageBox::Icon icon, QWidget *parent,
                                                 const QString &title, const QString &text,
                                                 QMessageBox::StandardButtons buttons,
                                                 QMessageBox::StandardButton defaultButton)
{
  QMessageBox::StandardButton ret = defaultButton;

  // if we're already on the right thread, this boils down to a function call
  GUIInvoke::blockcall([&]() {
    QMessageBox mb(icon, title, text, buttons, parent);
    mb.setDefaultButton(defaultButton);
    show(&mb);
    ret = mb.standardButton(mb.clickedButton());
  });
  return ret;
}

QString RDDialog::getExistingDirectory(QWidget *parent, const QString &caption, const QString &dir,
                                       QFileDialog::Options options)
{
  QFileDialog fd(parent, caption, dir, QString());
  fd.setAcceptMode(QFileDialog::AcceptOpen);
  fd.setFileMode(QFileDialog::DirectoryOnly);
  fd.setOptions(options);
  show(&fd);

  if(fd.result() == QFileDialog::Accepted)
  {
    QStringList files = fd.selectedFiles();
    if(!files.isEmpty())
      return files[0];
  }

  return QString();
}

QString RDDialog::getOpenFileName(QWidget *parent, const QString &caption, const QString &dir,
                                  const QString &filter, QString *selectedFilter,
                                  QFileDialog::Options options)
{
  QFileDialog fd(parent, caption, dir, filter);
  fd.setAcceptMode(QFileDialog::AcceptOpen);
  fd.setOptions(options);
  show(&fd);

  if(fd.result() == QFileDialog::Accepted)
  {
    if(selectedFilter)
      *selectedFilter = fd.selectedNameFilter();

    QStringList files = fd.selectedFiles();
    if(!files.isEmpty())
      return files[0];
  }

  return QString();
}

QString RDDialog::getSaveFileName(QWidget *parent, const QString &caption, const QString &dir,
                                  const QString &filter, QString *selectedFilter,
                                  QFileDialog::Options options)
{
  QFileDialog fd(parent, caption, dir, filter);
  fd.setAcceptMode(QFileDialog::AcceptSave);
  fd.setOptions(options);
  show(&fd);

  if(fd.result() == QFileDialog::Accepted)
  {
    if(selectedFilter)
      *selectedFilter = fd.selectedNameFilter();

    QStringList files = fd.selectedFiles();
    if(!files.isEmpty())
      return files[0];
  }

  return QString();
}
