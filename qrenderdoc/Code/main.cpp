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

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include "Code/CaptureContext.h"
#include "Windows/MainWindow.h"

int main(int argc, char *argv[])
{
  RENDERDOC_LogText("QRenderDoc initialising.");

  QString filename = "";
  bool temp = false;

  for(int i = 0; i < argc; i++)
  {
    if(!QString::compare(argv[i], "--tempfile", Qt::CaseInsensitive))
      temp = true;
  }

  QString remoteHost = "";
  uint remoteIdent = 0;

  for(int i = 0; i + 1 < argc; i++)
  {
    if(!QString::compare(argv[i], "--REMOTEACCESS", Qt::CaseInsensitive))
    {
      QRegularExpression regexp("^([a-zA-Z0-9_-]+:)?([0-9]+)$");

      QRegularExpressionMatch match = regexp.match(argv[i + 1]);

      if(match.hasMatch())
      {
        QString host = match.captured(1);

        if(host.length() > 0 && host[host.length() - 1] == ':')
          host.chop(1);

        bool ok = false;
        uint32_t ident = match.captured(2).toUInt(&ok);

        if(ok)
        {
          remoteHost = host;
          remoteIdent = ident;
        }
      }
    }
  }

  if(argc > 1)
  {
    filename = argv[argc - 1];
    QFileInfo checkFile(filename);
    if(!checkFile.exists() || !checkFile.isFile())
      filename = "";
  }

  argc += 2;

  char **argv_mod = new char *[argc];

  for(int i = 0; i < argc - 2; i++)
    argv_mod[i] = argv[i];

  char arg[] = "-platformpluginpath";
  QString path = QFileInfo(argv[0]).absoluteDir().absolutePath();
  QByteArray pathChars = path.toUtf8();

  argv_mod[argc - 2] = arg;
  argv_mod[argc - 1] = pathChars.data();

  QApplication application(argc, argv_mod);

  {
    CaptureContext ctx(filename, remoteHost, remoteIdent, temp);

    while(ctx.isRunning())
    {
      application.processEvents(QEventLoop::WaitForMoreEvents);
      QCoreApplication::sendPostedEvents();
    }
  }

  delete[] argv_mod;

  return 0;
}
